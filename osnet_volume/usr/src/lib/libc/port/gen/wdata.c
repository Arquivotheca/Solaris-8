/*	Copyright (c) 1986 AT&T */
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)wdata.c	1.6	96/11/26 SMI"   /* from JAE2.0 1.0 */
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <euc.h>

/*	Character width in EUC		*/
int	_cswidth[4] = {
		0,	/* 1:_cswidth is set, 0: not set	*/
		1,	/* Code Set 1 */
		0,	/* Code Set 2 */
		0	/* Code Set 3 */
	};
