/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gcvt.c	1.10	96/12/04 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

/*
 * gcvt  - Floating output conversion to
 *
 * pleasant-looking string.
 */

#pragma weak gcvt = _gcvt

#include "synonyms.h"
#include <floatingpoint.h>

char *
gcvt(double number, int ndigits, char *buf)
{
	return (gconvert(number, ndigits, 0, buf));
}
