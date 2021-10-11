/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)safe.c	1.1	99/05/14 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include "stabspf_impl.h"

#define	FD_WINDOW	10

static int fd = -1;
static char bigbuf[MAXBSIZE];
static char const *my_as = "/proc/self/as";

static int
open_as(void)
{
	struct rlimit rl;
	int newfd;
	int targetfd, lowerlimit;

	(void) getrlimit(RLIMIT_NOFILE, &rl);

	/* If open fails, return */
	if ((newfd = open(my_as, O_RDONLY)) == -1) {
		(void) fprintf(stderr,
		    gettext("apptrace: open of %s failed: %s\n"),
		    my_as, strerror(errno));
		return (newfd);
	}

	lowerlimit = rl.rlim_cur - FD_WINDOW;
	for (targetfd = rl.rlim_cur; targetfd > lowerlimit; targetfd--) {
		if ((fd = fcntl(newfd, F_DUPFD, targetfd)) != -1)
			break;
	}

	if (fd == -1) {
		(void) fprintf(stderr,
		    gettext("apptrace: fcntl F_DUPFD failed: %s\n"),
		    strerror(errno));
	}

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		(void) fprintf(stderr,
		    gettext("apptrace: fcntl FD_CLOEXEC failed: %s\n"),
		    strerror(errno));
	}

	/* Need to unconditionally close newfd */
	(void) close(newfd);

	return (fd);
}

/*
 * Essentially an address validator.  We pay no attention to buf for
 * reentrancy reasons.
 */
ssize_t
check_addr(void const *src, size_t size)
{
	ssize_t		retval = 0, ret;
	uintptr_t	sp;
	int i;
	int loops, leftover;

	if (fd == -1)
		if (open_as() == -1)
			return (0);

	loops = size / MAXBSIZE;
	leftover = size % MAXBSIZE;
	sp = (uintptr_t)src;

	do {
		ret = pread(fd, bigbuf,
		    loops != 0 ? MAXBSIZE : leftover, (off_t)sp);

		/* On success (else case), accumulate bytes read */
		if (ret == -1 && errno == EBADF) {
			/*
			 * fd has been closed
			 * attempt to open the address space again
			 * if that fails, return fail else loop
			 * like nothing happened.
			 */
			if (open_as() == -1)
				return (0);
			else
				continue;
		} else
			retval += ret;

		/* Decrement loop count and advance source pointer */
		loops--;
		sp += MAXBSIZE;

		/* Yes, the equal sign below is deliberate */
	} while (loops >= 0);

	return (retval);
}
