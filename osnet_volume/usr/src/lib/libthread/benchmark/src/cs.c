#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#define N 5000

longlong_t before, after;
long count;
long min,  min_int;

#define reset_accum()   {min = 1000000; count = 0;}
#define start_time()    {gethrtime(&before);}
#define accum_time()    {gethrtime(&after); acc_time();}
#define end_accum()     (printf("count=%d\n", count), min - min_int)

main()
{
	register int i;
	struct p {
		int in;
		int out;
	} p_next, p_prev;
	char c1, c2;
	int pid;
	long pt, ct;

	reset_accum();
	for (i = N; --i >= 0;) {
		start_time();
		accum_time();
	}
	min_int = end_accum();
	printf("min interval = %d\n", min_int);
	sleep(1);

	/*
	 * find pipe overhead.
	 */
	if (pipe(&p_next) < 0 || pipe(&p_prev) < 0) {
		perror("cswitch");
		exit(1);
	}
	c1 = 'A';
#ifdef PRINT
	printf("pipe and system call time\n");
#endif PRINT
	reset_accum();
	for (i = N; --i >= 0; ) {
		start_time();
		if (write(p_next.out, &c1, 1) != 1) {
			perror("cswitch");
			exit(2);
		}
		if (read(p_next.in, &c2, 1) != 1) {
			perror("cswitch");
			exit(3);
		}
		accum_time();
	}
	pt = end_accum();
	if (c1 != c2) {
		printf("bad pipe? c1=0x%x, c2=0x%x\n", c1, c2);
		exit (1);
	}
	/*
	 * Make a ring of procs connected by pipes
	 */
#ifdef PRINT
	printf("token ring of 2 processes connected by pipes\n");
#endif PRINT
	if ((pid = fork()) < 0) {
		perror("cswitch");
		exit(1);
	}
	if (pid) {
		int status;

		/*
		 * write once around ring to get past startup transient.
		 */
		if (write(p_next.out, &c1, 1) != 1) {
			perror("cswitch");
			exit(4);
		}
		if (read(p_prev.in, &c2, 1) != 1) {
			perror("cswitch");
			exit(5);
		}
		if (c1 != c2) {
			printf("bad pipe? c1=0x%x, c2=0x%x\n", c1, c2);
			exit (1);
		}
		reset_accum();
		for (i = N; --i >= 0; ) {
			start_time();
			if (write(p_next.out, &c1, 1) != 1) {
				perror("cswitch");
				exit(4);
			}
			if (read(p_prev.in, &c2, 1) != 1) {
				perror("cswitch");
				exit(5);
			}
			accum_time();
		}
		ct = end_accum();
		if (wait(&status) < 0) {
			perror("cswitch");
			exit(1);
		}
		if (c1 != c2) {
			printf("bad pipe? c1=0x%x, c2=0x%x\n", c1, c2);
			exit (1);
		}
		if (status) {
			fprintf(stderr,
			     "child proc returns status=0x%x\n", status);
			exit (1);
		}
	} else {
		if (read(p_next.in, &c1, 1) != 1) {
			perror("cswitch");
			exit(2);
		}
		if (write(p_prev.out, &c1, 1) != 1) {
			perror("cswitch");
			exit(1);
		}
		for (i = N; --i >= 0; ) {
			if (read(p_next.in, &c1, 1) != 1) {
				perror("cswitch");
				exit(2);
			}
			if (write(p_prev.out, &c1, 1) != 1) {
				perror("cswitch");
				exit(1);
			}
		}
		exit(0);
	}
	printf("cswitch = %d usec\n", (ct - (2 * pt))/2);
	printf("cswitch with syscalls = %d usec\n", ct/2);
}

#define TICKNS 10000000		/* ns/tick */
#define TICKUS 10000		/* us/tick */

acc_time()
{
	long usec;

	if (after/TICKUS != before/TICKUS)
		return;

	usec = (after - before);

	if (usec <= 0)
		return;
#ifdef PRINT
	printf("usec: %d\n",usec);
#endif
	count++;
	if (count == 1)
		return;
	if (usec < min)
		min = usec;
}

