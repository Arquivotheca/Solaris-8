#include <stdio.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/asm_linkage.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "thread.h"

static timestruc_t before, after;

#define N 50000

#define JB_SP 1
#define JB_PC 2

double stacka[1024];
double stackb[1024];

jmp_buf a, b, m;
char *current;
long rt;

mutex_t m1, m2;
sema_t s1, s2;

int n;

int xcnt = 0;
long min, count, min_int;
#define reset_accum()	{min = 1000000; count = 0;}
#define start_time()	{hrestime(&before);}
#define accum_time()	{hrestime(&after); acc_time();}
#define end_accum()	(printf("count=%d (%d)\n", count, xcnt), min - min_int)

main()
{
	register int i;
	extern proc_a(), proc_b();
	extern void tm1(),tm2(),ts1(),ts2();
	thread_t t;
	thread_t departedtid;
	int of;
	char *addr;

	setbuf(stdout, 0);

	reset_accum();
	for (i = N; --i >= 0;) {
		start_time();
		accum_time();
	}
	min_int = end_accum();
	printf("min interval = %d\n", min_int);

	/*
	 * Test with 2 pseudo-threads and simple resume
	 */
	stacka[0] = 0;
	stackb[0] = 0;
	a[JB_PC] = (int)proc_a;
	a[JB_SP] = (int)&stacka[1024] - WINDOWSIZE;
	b[JB_PC] = (int)proc_b;
	b[JB_SP] = (int)&stackb[1024] - WINDOWSIZE;
	current = (char *) m;
	n = N;
	tresume(a);
	printf("resume context switch = %d usec\n", rt/2);
	sleep(1);	/* let the bytes dribble out */

	/*
	 * Context switch to yourself.
	 */
	n = N;
	s();
	printf("setjmp context switch = %d usec\n", rt);
	sleep(1);	/* let the bytes dribble out */

	/*
	 * Context switch to yourself with an extra stack frame
	 * to produce and additional overflow and underflow per switch.
	 */
	n = N;
	reset_accum();
	for (i = n; --i >= 0;) {
		start_time();
		o();
		accum_time();
	}
	rt = end_accum();
	printf("setjmp context switch + over/underflow = %d usec\n", rt);
	sleep(1);	/* let the bytes dribble out */

	thr_setconcurrency(1);
	/*
	 * Unbound thread synchronization using
	 * mutexes as binary semaphores (illegal)
	 */
	n = N/6 & ~0x01;
	t = thr_create(NULL, 0, tm2, 0, THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	t = thr_create(NULL, 0, tm1, 0, THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &departedtid);
	printf("unbound thread sync with mutex = %d usec\n", rt/2);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */

	/*
	 * Unbound thread synchronization using semaphores
	 */
	n = N/6 & ~0x01;
	t = thr_create(NULL, 0, ts2, &s1, THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	t = thr_create(NULL, 0, ts1, &s1, THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &departedtid);
	printf("unbound thread sync with semaphores = %d usec\n", rt/2);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */

	/*
	 * Bound thread synchronization using semaphores
	 */
	n = N/8 & ~0x01;
	t = thr_create(NULL, 0, ts2, &s1, THR_BOUND|THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	t = thr_create(NULL, 0, ts1, &s1, THR_BOUND);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &departedtid);
	printf("bound thread sync with semaphores = %d usec\n", rt/2);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */

	/*
	 * unbound thread in different processes using shared semaphores.
	 */
	n = N/8 & ~0x01;
	of = open("/dev/zero", O_RDWR);
	if (of < 0) {
		perror("open /dev/zero:");
		exit(1);
	}
	addr = mmap(NULL, 2*sizeof(sema_t),
		PROT_READ|PROT_WRITE, MAP_SHARED, of, 0);
	if ((int)addr == -1) {
		perror("mmap /dev/zero:");
		exit(1);
	}
	sema_init(&((sema_t *)addr)[0], 0, THREAD_SYNC_SHARED, 0);
	sema_init(&((sema_t *)addr)[1], 0, THREAD_SYNC_SHARED, 0);
	if (fork() == 0) {
		/* child */
		t = thr_create(NULL, 0, ts2, addr, 0);
		if (t == NULL) {
			printf("cannot create thread\n");
			exit(1);
		}
		thr_join(t, &departedtid);
		exit(0);
	} else {
		/* parent */
		t = thr_create(NULL, 0, ts1, addr, 0);
		if (t == NULL) {
			printf("cannot create thread\n");
			exit(1);
		}
		thr_join(t, &departedtid);
	}
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */
	printf("cross process thread sync = %d usec\n", rt/2);

	return (0);
}

s()
{
	register int i;

	reset_accum();
	for (i = n; --i >= 0;) {
		start_time();
		if (setjmp(m) == 0)
			longjmp(m, 1);
		accum_time();
	}
	rt = end_accum();
}

o()
{
	if (setjmp(m) == 0)
		longjmp(m, 1);
}

tresume(jb)
	jmp_buf jb;
{
	if (setjmp(current) == 0) {
		current = (char *)jb;
		longjmp(jb, 1);
	}
}

proc_a()
{
	register int i;

	reset_accum();
	for (i = n/2; --i >= 0;) {
		start_time();
		tresume(b);
		accum_time();
	}
	rt = end_accum();
	tresume(m);
}

proc_b()
{
	register int i;

	for (i = n; --i >= 0;)
		tresume(a);
}

void
tm1()
{
	register int i;

	sema_p(&s1);
	reset_accum();
	for (i = n/2; --i >= 0;) {
		start_time();
		mutex_exit(&m2);
		mutex_enter(&m1);
		accum_time();
	}
	rt = end_accum();
}

void
tm2()
{
	register int i;

	sema_v(&s1);
	for (i = n/2; --i >= 0;) {
		mutex_enter(&m2);
		mutex_exit(&m1);
	}
}

void
ts1(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	xcnt = 0;
	s1p = &sp[0];
	s2p = &sp[1];
	sema_p(s1p);
	reset_accum();
	for (i = n/2; --i >= 0;) {
		start_time();
		sema_v(s2p);
		sema_p(s1p);
		accum_time();
		xcnt++;
	}
	rt = end_accum();
}

void
ts2(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	s1p = &sp[0];
	s2p = &sp[1];
	sema_v(s1p);
	for (i = n/2; --i >= 0;) {
		sema_p(s2p);
		sema_v(s1p);
	}
}

#define TICKNS 10000000		/* ns/tick */

acc_time()
{
	long usec;

	if (after.tv_sec != before.tv_sec ||
	    after.tv_nsec/TICKNS != before.tv_nsec/TICKNS)
		return;

	usec =
	    (long)(after.tv_sec - before.tv_sec) * 1000000 +
            (after.tv_nsec - before.tv_nsec)/1000;

	if (usec < 0)
		return;
	count++;
	if (count == 1)
		return;
	if (usec < min)
		min = usec;
/*
	printf("real time = %.3f sec\n", usec/1000000);
*/
}
