/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)exhaust.c	1.1	98/04/15 SMI"

#include <stdio.h>
#include "mtmalloc.h"
#include <unistd.h>
#include <thread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

/*
 * This file tests for swap space exhaustion
 *
 * cc -O -o exhaust exhaust.c -lmtmalloc -lthread
 */

void * be_thread(void *);
int iwin = 0;

main(int argc, char ** argv)
{
	int ncpus;
	thread_t tid[512];
	int fd;
	caddr_t stacks[512];

	srand(getpid());
	ncpus = sysconf(_SC_NPROCESSORS_CONF);

	fd = open("/dev/zero", O_RDONLY);

	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	while (ncpus--)
		stacks[ncpus] = mmap(0, 1<<23, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);

	close(fd);

	mallocctl(MTCHUNKSIZE, 150);

	ncpus = sysconf(_SC_NPROCESSORS_CONF);
	while (ncpus--)
		thr_create(stacks[ncpus], 1<<23, be_thread, NULL, THR_BOUND,
			&tid[ncpus]);

	while (thr_join(NULL, NULL, NULL) == 0);

	exit(0);
}

/* ARGSUSED */
void *
be_thread(void *foo)
{
	char *p;

	if (iwin) {
		printf("why am I here\n");
		return;
	}

	if ((p = malloc(rand())) == NULL) {
		iwin = 1;
		fprintf(stderr, "Errno is %d\n", errno);
		perror("malloc");
	} else {
		be_thread(NULL);
		printf("free %p\n", p);
		free(p);
	}

}
