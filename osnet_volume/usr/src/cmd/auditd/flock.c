#ifndef lint
static char sccsid[] = "@(#)flock.c 97/11/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <fcntl.h>
#include "flock.h"

extern int errno;

int
flock(fd, operation)
int fd, operation;
{
	struct flock	fl;
	int	cmd;
	int	ret;

	/* initialize the flock struct to set lock on entire file */
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_type = 0;

	/* In non-blocking lock, use F_SETLK for cmd, F_SETLKW otherwise */
	if (operation & LOCK_NB) {
		cmd = F_SETLK;
		operation &= ~LOCK_NB;	/* turn off this bit */
	} else
		cmd = F_SETLKW;

	switch (operation) {
	case LOCK_UN:
		fl.l_type |= F_UNLCK;
		break;
	case LOCK_SH:
		fl.l_type |= F_RDLCK;
		break;
	case LOCK_EX:
		fl.l_type |= F_WRLCK;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	ret = fcntl(fd, cmd, &fl);

	if (ret == -1 && errno == EACCES)
		errno = EWOULDBLOCK;

	return (ret);
}
