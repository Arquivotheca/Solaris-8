#include <stdio.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/asm_linkage.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "thread.h"
#include "utrace.h"

static struct timeval before, after;
long end_time();

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

main()
{
	register int i;
	extern proc_a(), proc_b();
	extern void tm1(),tm2(),ts1(),ts2();
	thread_t t;
	thread_t departedtid;
	int of;
	char *addr;

#ifdef TRLIB
	enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_START);
	enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END);
	enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_START);
	enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_END);
	enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_FLUSH);
#endif
	setbuf(stdout, 0);

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
	t = thr_create(NULL, 0, tm2, 0, THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	t = thr_create(NULL, 0, tm1, 0, 0);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &departedtid);
	printf("unbound thread sync with mutex = %d usec\n", rt/n);
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
	t = thr_create(NULL, 0, ts1, &s1, 0);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	}
	thr_join(t, &departedtid);
	printf("unbound thread sync with semaphores = %d usec\n", rt/n);
	for (i = 5000000; --i;)
		;		/* sleep doesn't work */
	return (0);
}

s()
{
	register int i;

	start_time();
	for (i = n; --i >= 0;) {
		if (setjmp(m) == 0)
			longjmp(m, 1);
	}
	rt = end_time();
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

void
tm1()
{
	register int i;

	sema_p(&s1);
	start_time();
	for (i = n/2; --i >= 0;) {
		mutex_exit(&m2);
		mutex_enter(&m1);
	}
	rt = end_time();
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

	s1p = &sp[0];
	s2p = &sp[1];
	sema_p(s1p);
	start_time();
	for (i = n/2; --i >= 0;) {
		sema_v(s2p);
		sema_p(s1p);
	}
	rt = end_time();
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
