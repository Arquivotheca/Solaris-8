/* @(#)fork1.c 1.1 92/06/01 */
#include <stdio.h>
#include <sys/wait.h>

#define DEFAULT_PROCS 100
#define MAX_NAME_LENGTH 30
static char pgm_name [MAX_NAME_LENGTH];

main(argc, argv)
int argc;
char *argv[];
{
	char opt;
	register int i, ret;
	register int procs;
	int stat;
	extern void usage();

        procs = DEFAULT_PROCS;
	sprintf (pgm_name, "%s", argv[0]);
	if (argc > 2) {
		usage();
		exit(1);
	} 
	argv += 1;
	argc -= 1;
	if (argc > 0)
		procs = atoi(argv[0]);
	printf("creating %d processes \n", procs);
	for (i = 0; i < procs; i++) {
		ret = fork1() ;
		if (ret > 0) {
			waitpid(ret, &stat, 0);
			if (!WIFEXITED(stat)) {
				printf("child not exited normally \n");
				printf("%d, pid %d, stat = %d",
				    i, ret, stat);
				exit(1);
			}
			continue;
		} else if (ret == 0)
			exit(0);
		 else {
			fprintf(stderr,"fork(0 # %d failed !\n", i);
			exit(1);
		}
	}
	printf("finished creating %d processes \n", procs);
}

void
usage()
{
	printf("%s  <num_of_processes>\n", pgm_name);
}
