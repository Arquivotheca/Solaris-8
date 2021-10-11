/*
 * Copyright (c) 1997, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)printf.c	1.3	98/07/29 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include "file64.h"
#include <stdio.h>
#include <stdarg.h>
#include "libc.h"

int
printf(const char *format, ...)
{
	int count;
	va_list ap;

	va_start(ap, /* */);
	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			return (EOF);
		}
	}
	count = _doprnt((char *) format, ap, stdout);
	va_end(ap);
	return (count);
}
