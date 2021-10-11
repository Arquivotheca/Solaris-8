/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rcv.h	1.8	92/07/14 SMI"	/* from SVr4.0 1.2.2.1 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * This file is included by normal files which want both
 * globals and declarations.
 */

/*#define	USG	1 */		/* System V */
#define	USG_TTY	1			/* termio(7) */

#include "def.h"
#include "glob.h"
