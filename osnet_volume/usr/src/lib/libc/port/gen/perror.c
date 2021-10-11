/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)perror.c	1.17	96/10/29 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "_libc_gettext.h"

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern int _sys_num_err;
extern const char _sys_errs[];
extern const int _sys_index[];

/*
 * Print the error indicated
 * in the cerror cell.
 */
void
perror(const char *s)
{
	const char *c;
	int err = errno;

	if (err < _sys_num_err && err >= 0)
		c = _libc_gettext(&_sys_errs[_sys_index[err]]);
	else
		c = _libc_gettext("Unknown error");

	if (s && *s) {
		(void) write(2, s, strlen(s));
		(void) write(2, ": ", 2);
	}
	(void) write(2, c, strlen(c));
	(void) write(2, "\n", 1);
}
