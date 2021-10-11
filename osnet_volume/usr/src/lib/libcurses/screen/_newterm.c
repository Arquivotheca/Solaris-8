/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)_newterm.c	1.6	97/06/25 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#define		NOMACROS
#include	<sys/types.h>
#include	"curses_inc.h"

SCREEN	*
newterm(char *type, FILE *fout, FILE *fin)
{
	return (newscreen(type, 0, 0, 0, fout, fin));
}
