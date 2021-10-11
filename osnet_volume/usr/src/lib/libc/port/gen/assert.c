/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)assert.c	1.15	96/10/15 SMI"	/* SVr4.0 1.4.1.7 */

/*LINTLIBRARY*/

#pragma weak _assert = __assert

#include "synonyms.h"
#include "_libc_gettext.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>

/*
 * called from "assert" macro; prints without printf or stdio.
 */
void
_assert(const char *assertion, const char *filename, int line_num)
{
	char buf[512];

	(void) sprintf(buf,
	    _libc_gettext("Assertion failed: %s, file %s, line %d\n"),
	    assertion, filename, line_num);
	(void) write(2, buf, strlen(buf));
	(void) abort();
}
