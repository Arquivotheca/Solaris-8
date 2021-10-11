/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)fdatasync.c 1.8     97/07/30 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/fcntl.h>

extern int __fdsync(int fd, mode_t mode);

int
fdatasync(int fd)
{
	return (__fdsync(fd, O_DSYNC));
}
