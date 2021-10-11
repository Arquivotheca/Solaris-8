#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#define M60

#ifdef M260
#define PERIOD	(60e-9)
#endif M260
#ifdef M60
#define PERIOD	(50e-9)
#endif M60
#ifdef M490
#define PERIOD	(30e-9)
#endif M490
#ifdef M65
#define PERIOD	(40e-9)
#endif M65

static struct timeval before, after;
long end_time();
void null();

#define N 100000

main()
{
	register int i;
	register int pid;
	long lt, mt, gt, st, ratio;
	long not, ot;

	setbuf(stdout, 0);
#ifdef PRINT
	printf("empty loop: %d iterations\n", 100*N);
	sleep(1);	/* let bytes dribble out */
#endif PRINT
	start_time();
	for (i = 100*N; --i >= 0; ) {
	}
	lt = end_time() / 100;

#ifdef PRINT
	printf("%d moves (%.2f sec @ %.2fMhz)\n",
	    1000*N, (long)PERIOD*(1000*N), (long)1.0/(PERIOD*1000000));
	sleep(1);	/* let bytes dribble out */
#endif PRINT
	start_time();
#define	move5 asm("mov %g1,%g1;mov %g1,%g1;mov %g1,%g1;mov %g1,%g1;mov %g1,%g1")
	for (i = N; --i >= 0; ) {
		/* 20 lines of 10 * 5 move instructions == 1000 moves */
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
		move5;move5;move5;move5;move5;move5;move5;move5;move5;move5;
	}
	mt = end_time();
	printf("speed of light: %d.%.2d%% degradation\n",
	    (mt-lt-(50*N))*100/(50*N),
	    (mt-lt-(50*N))*10000/(50*N)%100);
	printf(
"\t(computed = %d usec, actual = %d usec,\n\t%d loop overhead = %d usec)\n",
	    50*N, mt, N, lt);
	sleep(1);	/* let bytes dribble out */

#ifdef PRINT
	printf("%d getpid()s\n", N);
	sleep(1);	/* let bytes dribble out */
#endif PRINT
	start_time();
	for (i = N; --i >= 0; ) {
		getpid();
	}
	gt = end_time();
	printf("getpid = %d usec\n", (gt - lt) / N);
	sleep(1);	/* let bytes dribble out */

#ifdef PRINT
	printf("%d signals\n", N);
	sleep(1);	/* let bytes dribble out */
#endif PRINT
	sigset(SIGUSR1, null);
	pid = getpid();
	start_time();
	for (i = N; --i >= 0; ) {
		kill(pid, SIGUSR1);
	}
	st = end_time();
	printf("signal = %d usec\n", (st - gt) / N);
	cswitch();
}

cswitch()
{
	register int i;
	struct p {
		int in;
		int out;
	} p_next, p_prev;
	char c1, c2;
	int pid;
	long pt, ct;

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
	start_time();
	for (i = N/2; --i >= 0; ) {
		if (write(p_next.out, &c1, 1) != 1) {
			perror("cswitch");
			exit(2);
		}
		if (read(p_next.in, &c2, 1) != 1) {
			perror("cswitch");
			exit(3);
		}
	}
	pt = end_time();
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
		start_time();
		for (i = N/2; --i >= 0; ) {
			if (write(p_next.out, &c1, 1) != 1) {
				perror("cswitch");
				exit(4);
			}
			if (read(p_prev.in, &c2, 1) != 1) {
				perror("cswitch");
				exit(5);
			}
		}
		ct = end_time();
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
		for (i = N/2; --i >= 0; ) {
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
	printf("cswitch = %d usec\n", (ct - (2*pt)) / (N/2));
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
	    (after.tv_sec - before.tv_sec) * 1000000 +
            after.tv_usec - before.tv_usec;
/*
	printf("real time = %d usec\n", usec);
*/
	return(usec);
}

void null() {}
