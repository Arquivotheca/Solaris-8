/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rename.c	1.13	96/11/22 SMI"	/* SVr4.0 1.10 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "libc.h"

int
remove(const char *filename)
{
	struct stat64	statb;

	/*
	 * If filename is not a directory, call unlink(filename)
	 * Otherwise, call rmdir(filename)
	 */

	if (lstat64(filename, &statb) != 0)
		return (-1);
	if ((statb.st_mode & S_IFMT) != S_IFDIR)
		return (unlink(filename));
	return (rmdir(filename));
}

int
rename(const char *old, const char *new)
{
	return (_rename(old, new));
}
