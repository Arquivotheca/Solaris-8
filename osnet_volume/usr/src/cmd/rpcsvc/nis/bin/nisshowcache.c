/*
 *	nisshowcache.c
 *
 *	Copyright (c) 1988-1995 Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nisshowcache.c	1.9	95/05/09 SMI"

/*
 * Ported from
 *  "@(#)dump_cache.c  1.5  90/11/13  Copyr 1988 Sun Micro";
 *
 * This program dumps the contents of the NIS+ location cache
 * to the standard output.
 */

#include	<stdio.h>
#include	<sys/types.h>
#include	<rpcsvc/nis.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>

#ifndef _sys_sem_h
/*
 * This guard is defined only in 4.1 systems and is to
 * be included only in 5.0 as it is not defined there.
 */
union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
};
#endif /* _sys_sem_h  */

static void print_semaphores();
static void __nis_print_sems(int, int);


void
usage(char *name)
{
	fprintf(stderr, "usage: %s [-v]\n", name);
	exit(1);
}


/* dump the context cache on the system */
void
main(int argc, char *argv[])
{
	extern int __nis_debuglevel;
	int c;
	nis_error status;
	int new__nis_debug_level = 0;
	char *cptr;	/* passed to __nis_CacheInit(); we don't use it */

	__nis_debuglevel = 0;

	/* get command line options */
	while ((c = getopt(argc, argv, "vsd")) != EOF) {
		switch (c) {
		    case 'v':	/* verbose mode */
			new__nis_debug_level = 3;
			break;

		    case 's':
			/*
			 * special format - prints information on one line.
			 * undocumented feature. see cache_entry.cc
			 */
			new__nis_debug_level = 6;
			break;

		    case 'd':
			/*
			 * another undocumented feature.
			 * prints the semaphores
			 */
			print_semaphores();
			break;

		    case '?':	/* error */
			usage(argv[0]);
			break;
		}
	}
	if ((status = __nis_CacheInit(&cptr)) != NIS_SUCCESS) {
	    fprintf(stderr, "NIS+ initialization error: %s\n",
		nis_sperrno(status));
	    exit((int)status);
	}
	if (new__nis_debug_level)
		__nis_debuglevel = new__nis_debug_level;
	else
		__nis_debuglevel = 1;

	__nis_CachePrint();

	exit(0);
}


#define	NIS_SEM_R_KEY		100302
#define	NIS_SEM_W_KEY		100303
#define	NIS_W_NSEMS		2
#define	NIS_R_NSEMS		1

static void
print_semaphores()
{
	int sem_writer, sem_reader;
	int semflg = 0;

	sem_reader = semget(NIS_SEM_R_KEY, NIS_R_NSEMS, semflg);
	sem_writer = semget(NIS_SEM_W_KEY, NIS_W_NSEMS, semflg);

	if (sem_reader == -1 || sem_writer == -1) {
		printf("Could not open semaphores!\n");
		return;
	}
	__nis_print_sems(sem_writer, sem_reader);
}


static void
__nis_print_sems(int sem_writer, int sem_reader)
{
	int i;
	ushort w_array[NIS_W_NSEMS];
	ushort r_array[NIS_R_NSEMS];
	union semun semarg;


	/* cache_manager (writer) semaphores */
	if (sem_writer != -1) {
		semarg.array = w_array;
		if (semctl(sem_writer, 0, GETALL, semarg) == -1) {
			printf("nis_print_sems: semctl GETALL failed");
			return;
		}
		for (i = 0; i < NIS_W_NSEMS; i++)
			printf("sem_writer[%d] = %d\n", i, w_array[i]);
	}
	/* reader */
	if (sem_reader != -1) {
		semarg.array = r_array;
		if (semctl(sem_reader, 0, GETALL, semarg) == -1) {
			printf("nis_print_sems: semctl GETALL failed");
			return;
		}
		for (i = 0; i < NIS_R_NSEMS; i++)
			printf("sem_reader[%d] = %d\n", i, r_array[i]);
	}
}
