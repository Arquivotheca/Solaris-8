/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * Copyright  (c) 1985 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)if_exec.c	1.4	92/07/14 SMI"       /* SVr4.0 1.1 */

#include <stdio.h>
#include <varargs.h>
#include "wish.h"
#include "terror.h"


int
IF_exec_open(argv)
char *argv[];
{
	(void) proc_openv(0, NULL, NULL, argv);
}
