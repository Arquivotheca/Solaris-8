/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)llabs.c	1.3	96/10/15 SMI"

/*LINTLIBRARY*/

#pragma weak llabs = _llabs

#include "synonyms.h"
#include <stdlib.h>
#include <sys/types.h>

longlong_t
llabs(longlong_t arg)
{
	return (arg >= 0 ? arg : -arg);
}
