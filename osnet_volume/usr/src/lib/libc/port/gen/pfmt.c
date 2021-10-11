/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfmt.c	1.6	99/06/10 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include "mtlib.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include "pfmt_data.h"

/* pfmt() - format and print */

int
pfmt(FILE *stream, long flag, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	return (__pfmt_print(stream, flag, format, NULL, NULL, args));
}
