/* @(#) thr_create.c 1.4 92/07/20 */

#include <thread.h>
#include <stdio.h>
#include "utrace.h"

#define DEFAULT_THREADS 100
#define MAX_NAME_LENGTH 30

static char pgm_name [MAX_NAME_LENGTH];

static void bth(), bth_det(), uth(), usage();

static sema_t dummy_sema = SHAREDSEMA;

thread_t dummytid;
void *dummystat;

#define CACHE_SIZE 30

thread_t tid[CACHE_SIZE];

main(argc, argv)
int argc;
char *argv[];
{

	int threads, i, j, ref = 0, refu = 0, refb = 0,  cache = 0;
	int ret;
	thread_t departedtid;
	char opt;
	register long thr_create_flag = THR_DETACHED;

#ifdef TRACE
	disable_all_tracepoints();
#endif
        threads = DEFAULT_THREADS;
	sprintf (pgm_name, "%s", argv[0]);
	if (argc > 5) {
		(void) usage();
		exit(1);
	} 
	argv += 1;
	argc -= 1;
	while (argc > 0) {
		while (argc > 0 && **argv != '-') {
			argv++; argc--;
		}
		if (argc <= 0)
			break;
		(*argv)++; /* skip '-' */
		opt = **argv; /* get option name */
		switch (opt) {
		case 'b' : argv++; argc--;
			   if (argc > 0) {
                          	threads = atoi(*argv++);
                                argc--;
				thr_create_flag |= THR_BOUND;
                           }
                           else
                                printf("option -%c without option value\n",opt);
                           break;
		case 'u' : argv++; argc--;
			   if (argc > 0) {
                          	threads = atoi(*argv++);
                                argc--;
				if (thr_create_flag & THR_BOUND) {
					printf("bound/unbound are mutually ");
					printf("exclusive.\n");
					(void) usage();
				}
                           }
                           else
                                printf("option -%c without option value\n",opt);
			   break;
		case 'r' : argv++; argc--;
			   ref = 1;
			   if (argc > 0) {
				if (**argv == 'u')
					refu = 1;
			   	else if (**argv == 'b')
					refb = 1;
			   }
			   break;
		case 'c' : argv++; argc--;
			   cache = 1;
			   break;
		default:
			   printf("invalid option -%c %s\n",opt,*argv);
			   argc--;
		}
	}
	if (cache) {
		int zombies;

		printf("caching threads\n");
		thr_create_flag &= ~THR_DETACHED;
		/* cache threads */
		for (i = 0; i < threads; i++) {
			if (thr_create_flag & THR_BOUND)
			    ret = thr_create (NULL, 0, bth, 0, thr_create_flag, 
				&(tid[i % CACHE_SIZE]));
			else
			    ret = thr_create (NULL, 0, uth, 0, thr_create_flag, 
				&tid [i % CACHE_SIZE]);
			if (ret != 0) {
				fprintf(stderr,
				    "c thr_create() # %d failed !\n", i);
				exit(1);
			} 
			if ((i+1) % CACHE_SIZE == 0)
				for (j = 0; j < CACHE_SIZE; j++)
					thr_join(tid[j], &dummytid, &dummystat);
		}
		zombies = i % CACHE_SIZE;
		for (j = 0; j < zombies; j++)
			thr_join(tid[j], &dummytid, &dummystat);
		thr_create_flag |= THR_DETACHED;
		sleep(2);
		printf("finished caching threads\n");
	}
#ifdef TRACE
	enable_tracepoint (UTR_FAC_THREAD, UTR_THR_CREATE_START);
	enable_tracepoint (UTR_FAC_THREAD, UTR_THR_CREATE_END);
	enable_tracepoint (UTR_FAC_THREAD, UTR_THR_CREATE_END2);
	if (ref) {
		if (thr_create_flag & THR_BOUND) {
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_CACHE_MISS);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_CACHE_HIT);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_STK);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_ALLOCSTK);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_ALLOC_TLS_END);
			enable_tracepoint (UTR_FAC_TLIB_MISC, 
			    UTR_LWP_EXEC_START);
			enable_tracepoint (UTR_FAC_TLIB_MISC, 
			    UTR_LWP_EXEC_END);
			enable_tracepoint (UTR_FAC_TLIB_MISC, 
			    UTR_ALWP_MEMSET);
		} else
			refu = 1;
		if (refu) {
			enable_tracepoint (UTR_FAC_THREAD, UTR_THR_CONTINUE_START);
			enable_tracepoint (UTR_FAC_THREAD, UTR_THR_CONTINUE_END);
			/*
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_STK);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_CONT1);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_CONT2);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_ALLOCSTK);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_MEMSET1);
			enable_tracepoint (UTR_FAC_TLIB_MISC, UTR_THC_MEMSET2);
			*/
		}
	}
#ifdef ITRACE
	if (thr_create_flag & THR_BOUND)
		enable_tracepoint (TR_FAC_SYS_LWP, TR_SYS_LWP_CREATE_END2);
#endif
#endif
	printf("creating %d threads all %s\n", threads, 
	    thr_create_flag & THR_BOUND ? "bound" : "unbound");
	/*
	system("trctl -o /trace/bthc/k -f 124 -e 126 0 1 -v &");
	*/
	for (i = 0; i < threads; i++) {
		if (thr_create_flag & THR_BOUND)
			ret = thr_create (NULL, 0, bth_det, 0, thr_create_flag, NULL);
		else
			ret = thr_create (NULL, 0, uth, 0, thr_create_flag, NULL);
		if (ret != 0) {
			fprintf(stderr,"thr_create() # %d failed !\n", i);
			exit(1);
		} else
			if (thr_create_flag & THR_BOUND) {
				/* yield(); */
				sema_wait (&dummy_sema);
			} else
				thr_yield();
	}
	printf("finished creating %d threads all %s\n", threads, 
	    thr_create_flag & THR_BOUND ? "bound" : "unbound");
}

void
bth()
{
	thr_exit(0);
}

void
bth_det()
{
	sema_post(&dummy_sema);
	thr_exit(0);
}

void
uth()
{
	thr_exit(0);
}

void
usage()
{
	printf("%s [{-u,-b}] [{-r, -ru, -rb}] [-c] <num_of_threads>\n", 
	    pgm_name);
	printf("-u  - unbound\n-b  - bound\n-r  - refine into components\n");
	printf("-ru - refine for unbound threads\n");
	printf("-rb - refine for bound threads\n");
	printf("-c  - do caching of thread stacks\n");
}
