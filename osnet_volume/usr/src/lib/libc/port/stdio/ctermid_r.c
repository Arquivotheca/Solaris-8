/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)ctermid_r.c	1.4	96/12/02	SMI"	/* SVr4.0 1.12  */

/*LINTLIBRARY*/

#pragma weak ctermid_r = _ctermid_r

#include "synonyms.h"
#include "mtlib.h"
#include <stdio.h>
#include <string.h>
#include <thread.h>
#include <synch.h>

/*
 * re-entrant version of ctermid()
 */

char *
_ctermid_r(char *s)
{
	return (s ? strcpy(s, "/dev/tty") : NULL);
}
