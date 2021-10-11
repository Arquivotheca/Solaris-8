#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>

#define N 5000

longlong_t before, after;
long min, count, min_int;

#define reset_accum()   {min = 1000000; count = 0;}
#define start_time()    {gethrtime(&before);}
#define accum_time()    {gethrtime(&after); acc_time();}
#define end_accum()     (printf("count=%d\n", count), min - min_int)

struct sembuf p1[2];
struct sembuf p2[2];

main()
{
	register int i;
	int pid;
	int s;
	long st, ct;

	reset_accum();
	for (i = N; --i >= 0;) {
		start_time();
		accum_time();
	}
	min_int = end_accum();
	printf("min interval = %d\n", min_int);
	sleep(1);

	/*
	 * find semaphore overhead
	 */
	s = semget(IPC_PRIVATE, 2, 0777);
	if (s < 0) {
		perror("cswitch");
		exit(1);
	}
	
	p1[0].sem_num = 0;
	p1[0].sem_op = 1;
	p1[0].sem_flg = 0;
	p1[1].sem_num = 0;
	p1[1].sem_op = -1;
	p1[1].sem_flg = 0;
	p2[0].sem_num = 1;
	p2[0].sem_op = 1;
	p2[0].sem_flg = 0;
	p2[1].sem_num = 1;
	p2[1].sem_op = -1;
	p2[1].sem_flg = 0;

	for (i = N; --i >= 0; ) {
		start_time();
		if (semop(s, p1, 2) < 0) {
			perror("cswitch");
			exit(1);
		}
		if (semop(s, p2, 2) < 0) {
			perror("cswitch");
			exit(1);
		}
		accum_time();
	}
	st = end_accum();

	if ((pid = fork()) < 0) {
		perror("cswitch");
		exit(1);
	}

	p1[0].sem_num = 1;
	p1[0].sem_op = 1;
	p1[1].sem_num = 0;
	p1[1].sem_op = -1;
	p2[0].sem_num = 1;
	p2[0].sem_op = -1;
	p2[1].sem_num = 0;
	p2[1].sem_op = 1;

	if (pid) {
		if (semop(s, &p1[1], 1) < 0) {
			perror("cswitch");
			exit(1);
		}
		for (i = N; --i >= 0; ) {
			start_time();
			if (semop(s, p1, 2) < 0) {
				perror("cswitch");
				exit(1);
			}
			accum_time();
		}
		ct = end_accum();
	} else {
		if (semop(s, &p2[1], 1) < 0) {
			perror("cswitch");
			exit(1);
		}
		for (i = N; --i >= 0; ) {
			if (semop(s, p2, 2) < 0) {
				perror("cswitch");
				exit(1);
			}
		}
	}
	printf("cswitch = %d usec\n", (ct - st)/2);
	printf("cswitch with syscalls = %d usec\n", ct/2);
}

#define TICKNS 10000000		/* ns/tick */
#define TICKUS 10000		/* us/tick */

acc_time()
{
	long usec;

	if (after/TICKUS != before/TICKUS)
		return;

	usec = after - before;

	if (usec <= 0)
		return;
	count++;
	if (count == 1)
		return;
	if (usec < min)
		min = usec;
}

