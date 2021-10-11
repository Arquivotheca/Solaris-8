/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)open.c	1.7	97/09/25 SMI"	/* SVr4.0 1.4	*/
/*LINTLIBRARY*/

#include <sys/types.h>
#include <fcntl.h>

int vti = -1;

void
openvt(void)
{
	vti = open("/dev/vt0", 1);
}

void
openpl(void)
{
	vti = open("/dev/vt0", 1);
}
