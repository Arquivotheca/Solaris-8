/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)reboot.c	1.4	99/03/23 SMI"

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <sys/uadmin.h>
#include <sys/reboot.h>

/*
 * Note that not all of BSD's semantics are supported.
 */
int
reboot(int howto, char *bootargs)
{
	int cmd = A_SHUTDOWN;
	int fcn = AD_BOOT;

	if (howto & RB_DUMP)
		cmd = A_DUMP;

	if (howto & RB_HALT)
		fcn = AD_HALT;
	else if (howto & RB_ASKNAME)
		fcn = AD_IBOOT;

	return (uadmin(cmd, fcn, (uintptr_t)bootargs));
}       
