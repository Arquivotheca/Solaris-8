/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)xgetwidth.c	1.6	96/11/26 SMI"   /* from JAE2.0 1.0 */
/*LINTLIBRARY*/
/*
 * _Xgetwidth calls _getwidth to get the values of environment variables
 * CSWIDTH, SCRWIDTH and PCWIDTH, and checks the values to fit C process
 * environment. This function is called only once in a program.
 */

/* #include "shlib.h" */
#include "synonyms.h"
#include <sys/types.h>
#include <euc.h>
#include <getwidth.h>
extern int _cswidth[];

void
_xgetwidth(void)
{
	eucwidth_t	_eucwidth;

	*_cswidth = 1; /* set to 1 when called */
	getwidth(&_eucwidth);

	if (_eucwidth._eucw1 <= 4)
		_cswidth[1] = _eucwidth._eucw1;
	if (_eucwidth._eucw2 <= 4)
		_cswidth[2] = _eucwidth._eucw2;
	if (_eucwidth._eucw3 <= 4)
		_cswidth[3] = _eucwidth._eucw3;
}
