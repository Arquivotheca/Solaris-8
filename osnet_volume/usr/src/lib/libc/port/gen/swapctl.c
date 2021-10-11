/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)swapctl.c	1.8	96/11/22 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak swapctl = _swapctl

#include "synonyms.h"
#include "sys/uadmin.h"
#include <sys/types.h>
#include <sys/swap.h>

int
swapctl(int cmd, void *arg)
{
	return (uadmin(A_SWAPCTL, cmd, (uintptr_t)arg));
}
