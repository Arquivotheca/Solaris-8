/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)vlfmt.c	1.4	99/06/10 SMI"
/*LINTLIBRARY*/

/* vlfmt() - format, print and log (variable arguments) */

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include "pfmt_data.h"

int
vlfmt(FILE *stream, long flag, const char *format, va_list args)
{
	int ret;

	const char *text, *sev;
	if ((ret = __pfmt_print(stream, flag, format, &text, &sev, args)) < 0)
		return (ret);

	return (__lfmt_log(text, sev, args, flag, ret));
}
