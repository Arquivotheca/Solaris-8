/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcflow.c	1.8	96/10/15 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak tcflow = _tcflow

#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * suspend transmission or reception of input or output
 */

/*
 * TCOOFF (0) -> suspend output
 * TCOON  (1) -> restart suspend output
 * TCIOFF (2) -> suspend input
 * TCION  (3) -> restart suspend input
 */

int
tcflow(int fildes, int action)
{
	return (ioctl(fildes, TCXONC, action));

}
