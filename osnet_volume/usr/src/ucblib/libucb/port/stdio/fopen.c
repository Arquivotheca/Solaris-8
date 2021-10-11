/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986-1998  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)fopen.c	1.7	98/12/11 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "file64.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include "stdiom.h"

/* Final argument to _endopen depends on build environment */
#define	ALWAYS_LARGE_OPEN	1
#define	LARGE_OPEN	(_FILE_OFFSET_BITS == 64)

static FILE *
_endopen(const char *file, const char *mode, FILE *iop, int largefile)
{
	int	plus, oflag, fd;

	if (iop == NULL || file == NULL || file[0] == '\0')
		return (NULL);
	plus = (mode[1] == '+');
	switch (mode[0]) {
	case 'w':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT;
		break;
	case 'a':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_CREAT;
		break;
	case 'r':
		oflag = plus ? O_RDWR : O_RDONLY;
		break;
	default:
		return (NULL);
	}
	if (largefile) {
		fd = open64(file, oflag, 0666);	/* mapped to open() for V9 */
	} else {
		fd = open(file, oflag, 0666);
	}
	if (fd < 0)
		return (NULL);
	iop->_cnt = 0;
	iop->_file = (unsigned char) fd;
	iop->_flag = plus ? _IORW : (mode[0] == 'r') ? _IOREAD : _IOWRT;
	if (mode[0] == 'a')   {
		if ((lseek64(fd, 0L, SEEK_END)) < 0)  {
			(void) close(fd);
			return (NULL);
		}
	}
	iop->_base = iop->_ptr = NULL;
	/*
	 * Sys5 does not support _bufsiz
	 *
	 * iop->_bufsiz = 0;
	 */
	return (iop);
}

FILE *
fopen(const char *file, const char *mode)
{
	FILE	*iop;
	FILE	*rc;

	iop = _findiop();
	rc = _endopen(file, mode, iop, LARGE_OPEN);
	if (rc == NULL && iop != NULL)
		iop->_flag = 0;	/* release iop */
	return (rc);
}

/*
 * For _LP64, all fopen() calls are 64-bit calls, i.e., open64() system call.
 * There should not be fopen64() calls.
 * Similar for freopen64().
 */
#if !defined(_LP64)
FILE *
fopen64(const char *file, const char *mode)
{
	FILE	*iop;
	FILE	*rc;

	iop = _findiop();
	rc = _endopen(file, mode, iop, ALWAYS_LARGE_OPEN);
	if (rc == NULL && iop != NULL)
		iop->_flag = 0;	/* release iop */
	return (rc);
}
#endif

FILE *
freopen(const char *file, const char *mode, FILE *iop)
{
	(void) fclose(iop); /* doesn't matter if this fails */
	return (_endopen(file, mode, iop, LARGE_OPEN));
}

#if !defined(_LP64)
FILE *
freopen64(const char *file, const char *mode, FILE *iop)
{
	(void) fclose(iop); /* doesn't matter if this fails */
	return (_endopen(file, mode, iop, ALWAYS_LARGE_OPEN));
}
#endif
