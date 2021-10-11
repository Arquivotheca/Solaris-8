/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)general.c	1.1	98/04/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread.h>

/*
 * This file contains a general health check for the libmtmalloc.so.1 lib.
 * It creates NCPUS worth of threads and has eadch perform 1000000 random
 * small allocs and then free them.
 *
 * cc -O -o general general.c -lmtmalloc -lthread
 */
#define	N	1000000

void *be_thread(void *);

main(int argc, char ** argv)
{
	int i;
	thread_t tid[512];	/* We'll never have more than that! hah */

	i = sysconf(_SC_NPROCESSORS_CONF);
	srand(getpid());

	while (i)
		thr_create(NULL, 1<<23, be_thread, NULL, THR_BOUND, &tid[i--]);

	while (thr_join(NULL, NULL, NULL) == 0);

	exit(0);
}

/* ARGSUSED */
void *
be_thread(void * foo)
{
	int i = N;
	char *bar[N];

	while (i) {
		bar[i] = malloc(rand()%64);
		i--;
	}

	i = N;
	while (i) {
		free(bar[i]);
		i--;
	}
}
