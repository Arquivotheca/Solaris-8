/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)process.h	6.3	97/05/21 SMI"

/*
 *  process.h contains format strings for printing size information
 *
 *  Different format strings are used for hex, octal and decimal
 *  output.  The appropriate string is chosen by the value of numbase:
 *  pr???[0] for hex, pr???[1] for octal and pr???[2] for decimal.
 */


/* FORMAT STRINGS */

static char *prusect[3] = {
	"%lx",
	"%lo",
	"%ld"
	};

static char *prusum[3] = {
	" = 0x%lx\n",
	" = 0%lo\n",
	" = %ld\n"
	};
