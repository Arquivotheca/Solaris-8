/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dup2.c	1.13	96/10/15 SMI"	/* SVr4.0 1.8	*/

/* LINTLIBRARY */

#pragma weak dup2 = _dup2
#include 	"synonyms.h"
#include 	<sys/types.h>
#include	<fcntl.h>

int
dup2(int fildes, int fildes2)
{
	return (fcntl(fildes, F_DUP2FD, fildes2));
}
