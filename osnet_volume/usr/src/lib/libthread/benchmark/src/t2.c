#include <stdio.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/asm_linkage.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <thread.h>
#include <synch.h>

static struct timeval before, after;
long end_time();

#define N 50000

#define JB_SP 1
#define JB_PC 2

double stacka[1024];
double stackb[1024];

jmp_buf *a, *b, *m;
jmp_buf ax, by, mz;
jmp_buf *current;
long rt;

mutex_t m1, m2;
sema_t s1, s2;

int n;

main()
{
	register int i;
	extern proc_a(), proc_b();
	extern void *tm1(),*tm2(),*ts1(),*ts2();
	int ret;
	thread_t t, wid;
	void *status;
	int of;
	char *addr;

	setbuf(stdout, 0);

	a = &ax;
	b = &by;
	m = &mz;

	/*
	 * Test with 2 pseudo-threads and simple resume
	 */
	stacka[0] = 0;
	stackb[0] = 0;
	ax[JB_PC] = (int)proc_a;
	ax[JB_SP] = (int)&stacka[1024] - WINDOWSIZE;
	by[JB_PC] = (int)proc_b;
	by[JB_SP] = (int)&stackb[1024] - WINDOWSIZE;
	current = m;
	n = N;
	tresume(a);
	printf("resume context switch = %d usec\n", rt/n);
	sleep(1);	/* let the bytes dribble out */

	/*
	 * Context switch to yourself.
	 */
	n = N;
	s();
	printf("setjmp context switch = %d usec\n", rt/n);
	sleep(1);	/* let the bytes dribble out */

	/*
	 * Context switch to yourself with an extra stack frame
	 * to produce and additional overflow and underflow per switch.
	 */
	n = N;
	start_time();
	for (i = n; --i >= 0;) {
		o();
	}
	rt = end_time();
	printf("setjmp context switch + over/underflow = %d usec\n", rt/n);
	sleep(1);	/* let the bytes dribble out */

	thr_setconcurrency(1);
	/*
	 * Unbound thread synchronization using
	 * mutexes as binary semaphores (illegal)
	 */
	n = N/6 & ~0x01;
#ifdef UTRACE
	enable_all_tracepoints();
#endif
	ret = thr_create(NULL, 0, tm2, 0, THR_DETACHED, NULL);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	ret = thr_create(NULL, 0, tm1, 0, 0, &t);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &wid, &status);
	printf("unbound thread sync with mutex = %d usec\n", rt/n);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */

	/*
	 * Unbound thread synchronization using semaphores
	 */
	n = N/6 & ~0x01;
	ret = thr_create(NULL, 0, ts2, &s1, THR_DETACHED, NULL);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	ret = thr_create(NULL, 0, ts1, &s1, 0, &t);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &wid, &status);
	printf("unbound thread sync with semaphores = %d usec\n", rt/n);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */

	/*
	 * Bound thread synchronization using semaphores
	 */
	n = N/8 & ~0x01;
	ret = thr_create(NULL, 0, ts2, &s1, THR_BOUND, &t);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	ret = thr_create(NULL, 0, ts1, &s1, THR_BOUND, &t);
	if (ret != 0) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &wid, &status);
	printf("bound thread sync with semaphores = %d usec\n", rt/n);
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
	sema_init(&((sema_t *)addr)[0], 0, USYNC_PROCESS, 0);
	sema_init(&((sema_t *)addr)[1], 0, USYNC_PROCESS, 0);
	if (fork() == 0) {
		/* child */
		ret = thr_create(NULL, 0, ts2, (void *)addr, 0, &t);
		if (ret != 0) {
			printf("cannot create thread\n");
			exit(1);
		}
		thr_join(t, &wid, &status);
		exit(0);
	} else {
		/* parent */
		ret = thr_create(NULL, 0, ts1, (void *)addr, 0, &t);
		if (ret != 0) {
			printf("cannot create thread\n");
			exit(1);
		}
		thr_join(t, &wid, &status);
	}
	printf("cross process thread sync = %d usec\n", rt/n);

	return (0);
}

s()
{
	register int i;

	start_time();
	for (i = n; --i >= 0;) {
		if (setjmp(*m) == 0)
			longjmp(*m, 1);
	}
	rt = end_time();
}

o()
{
	if (setjmp(*m) == 0)
		longjmp(*m, 1);
}

tresume(jb)
	jmp_buf *jb;
{
	if (setjmp(*current) == 0) {
		current = jb;
		longjmp(*jb, 1);
	}
}

proc_a()
{
	register int i;

	start_time();
	for (i = n/2; --i >= 0;)
		tresume(b);
	rt = end_time();
	tresume(m);
}

proc_b()
{
	register int i;

	for (i = n; --i >= 0;)
		tresume(a);
}

void *
tm1()
{
	register int i;

	sema_wait(&s1);
	start_time();
	for (i = n/2; --i >= 0;) {
		mutex_unlock(&m2);
		mutex_lock(&m1);
	}
	rt = end_time();
}

void *
tm2()
{
	register int i;

	sema_post(&s1);
	for (i = n/2; --i >= 0;) {
		mutex_lock(&m2);
		mutex_unlock(&m1);
	}
}

void *
ts1(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	s1p = &sp[0];
	s2p = &sp[1];
	sema_wait(s1p);
	start_time();
	for (i = n/2; --i >= 0;) {
		sema_post(s2p);
		sema_wait(s1p);
	}
	rt = end_time();
}

void *
ts2(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	s1p = &sp[0];
	s2p = &sp[1];
	sema_post(s1p);
	for (i = n/2; --i >= 0;) {
		sema_wait(s2p);
		sema_post(s1p);
	}
}

start_time()
{
	gettimeofday(&before, 0);
}

long
end_time()
{
	long usec;

	gettimeofday(&after, 0);
	usec =
	    (long)(after.tv_sec - before.tv_sec) * 1000000 +
            after.tv_usec - before.tv_usec;
/*
	printf("real time = %.3f sec\n", usec/1000000);
*/
	return(usec);
}
