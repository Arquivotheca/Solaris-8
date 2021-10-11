#include <stdio.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/asm_linkage.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "thread.h"

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
	extern void ts1(),ts2();
	thread_t t;
	thread_t departedtid;
	int of;
	char *addr;

	setbuf(stdout, 0);

	/*
	 * Bound thread synchronization using semaphores
	 */
	n = N/8 & ~0x01;
	sema_init(&s1, 0, 0, 0);
	sema_init(&s2, 0, 0, 0);
	t = thr_create(NULL, 0, ts2, &s1, THR_BOUND|THR_DETACHED);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	} /*else printf("created bound thread ts2 = 0x%x, n=%d\n",t,n);*/
	t = thr_create(NULL, 0, ts1, &s1, THR_BOUND);
	if (t == NULL) {
		printf("cannot create thread\n");
		exit(1);
	} /*else printf("created bound thread ts1 = 0x%x, n=%d\n",t,n);*/
	thr_join(t, &departedtid);
	printf("bound thread sync with semaphores = %d usec\n", rt/n);
}

void
ts1(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	/*printf("from ts1\n");*/
	s1p = &sp[0];
	s2p = &sp[1];
	sema_p(s1p);
	start_time();
	/*printf("ts1 : started \n");*/
	for (i = n/2; --i >= 0;) {
		sema_v(s2p);
		sema_p(s1p);
	}
	rt = end_time();
	/*printf("ts1 : finished \n");*/
}

void
ts2(sp)
	sema_t *sp;
{
	register sema_t *s1p;
	register sema_t *s2p;
	register int i;

	/*printf("from ts2\n");*/
	s1p = &sp[0];
	s2p = &sp[1];
	sema_v(s1p);
	/*printf("ts2 : started \n");*/
	for (i = n/2; --i >= 0;) {
		sema_p(s2p);
		sema_v(s1p);
	}
	/*printf("ts2 : finished \n");*/
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
