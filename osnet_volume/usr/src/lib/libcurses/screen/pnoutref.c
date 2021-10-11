/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)pnoutref.c	1.6	97/06/25 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/* wnoutrefresh for pads. */

int
pnoutrefresh(WINDOW *pad, int pby, int pbx, int sby, int sbx, int sey, int sex)
{
	return (_prefresh(wnoutrefresh, pad, pby, pbx, sby, sbx, sey, sex));
}
