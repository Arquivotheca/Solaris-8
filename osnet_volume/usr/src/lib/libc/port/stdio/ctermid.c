/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)ctermid.c	1.10	96/12/02 SMI"	/* SVr4.0 1.12	*/

/*LINTLIBRARY*/

#pragma weak ctermid = _ctermid

#include "synonyms.h"
#include "mtlib.h"
#include <stdio.h>
#include <string.h>
#include <thread.h>
#include <synch.h>

static char res[L_ctermid];

/*
 * non-reentrant version in ctermid_r.c
 */
char *
ctermid(char *s)
{
	return (strcpy((s ? s : res), "/dev/tty"));
}
