#include <stdio.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/asm_linkage.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <thread.h>
#include <synch.h>
#ifdef TRACE
#include <utrace.h>
#endif

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

	char pgmname[30];
main(argc, argv)
int argc;
char *argv[];
{
	register int i;
	extern proc_a(), proc_b();
	extern void tm1(),tm2(),ts1(),ts2();
	int ret;
	thread_t t, wid;
	void *status;
	int of;
	char *addr;
	int mx = 0, se = 0;
	char opt;

#ifdef TRACE
	disable_all_tracepoints();
#endif
	if (argc > 3) {
		usage();
		exit(1);
	}
	sprintf(pgmname, "%s", argv[0]);
	setbuf(stdout, 0);
	
	se = 1;
	argv += 1;
        argc -= 1;
        while (argc > 0) {
		se = 0;
                while (argc > 0 && **argv != '-') {
                        argv++; argc--;
                }
                if (argc <= 0)
                        break;
                (*argv)++; /* skip '-' */
                opt = **argv; /* get option name */
                switch (opt) {
                case 'm' : argv++; argc--;
			   /*
                           if (argc > 0) {
                                threads = atoi(*argv++);
                                argc--;
                                thr_create_flag |= THR_BOUND;
                           }
                           else
                                printf("option -%c without option value\n",opt);
				*/
			   mx = 1;
                           break;
                case 's' : argv++; argc--;
			   /*
                           if (argc > 0) {
                                threads = atoi(*argv++);
                                argc--;
                                if (thr_create_flag & THR_BOUND) {
                                        printf("bound/unbound are mutually ");
 
                                       printf("bound/unbound are mutually ");
                                        printf("exclusive.\n");
                                        (void) usage();
                                }
                           }
                           else
                                printf("option -%c without option value\n",opt);
				*/
			   se = 1;
                           break;
                default:
                           printf("invalid option -%c %s\n",opt,*argv);
                           argc--;
                } /* swtch */
        } /* while */

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
	if (mx) {
		printf("benchmarking semaphores...\n");
#ifdef  TRACE
		enable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_START);
		enable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_END);
		enable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_START);
		enable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_END);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_START);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_START);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_FLUSH);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_END);
#endif
		/*
		 * Unbound thread synchronization using
		 * mutexes as binary semaphores (illegal)
		 */
		n = N/6 & ~0x01;
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
#ifdef TRACE
		disable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_START);
		disable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_END);
		disable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_START);
		disable_tracepoint(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_END);
#endif
		printf("unbound thread sync with mutex = %d usec\n", rt/n);

		sleep(1);
	}
#define UTR_SWTCH_BSIGP 7
#define UTR_SWTCH_ASIGP 8
	if (se) {
		printf("benchmarking semaphores...\n");
#ifdef  TRACE
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_START);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_END);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_BSIGP);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_SWTCH_ASIGP);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_START);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_FLUSH);
		enable_tracepoint(UTR_FAC_TLIB_SWTCH, UTR_RESUME_END);
#endif
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

		sleep(1);
	}
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

void
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

void
tm2()
{
	register int i;

	sema_post(&s1);
	for (i = n/2; --i >= 0;) {
		mutex_lock(&m2);
		mutex_unlock(&m1);
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
	sema_wait(s1p);
	start_time();
	for (i = n/2; --i >= 0;) {
		sema_post(s2p);
		sema_wait(s1p);
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

usage()
{
	printf("%s {-m, -s, -ms}\n-m  - benchmark mutexes\n", pgmname);
	printf("-s  - benchmark semaphores\n");
	printf("-ms - benchmark both mutexes and semaphores\n");
}
