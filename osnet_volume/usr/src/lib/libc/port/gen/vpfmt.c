/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)vpfmt.c	1.4	99/06/10 SMI"
/*LINTLIBRARY*/

/*
 * vpfmt() - format and print (variable argument list)
 */
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
vpfmt(FILE *stream, long flag, const char *format, va_list args)
{
	return (__pfmt_print(stream, flag, format, NULL, NULL, args));
}
