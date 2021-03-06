/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lconstants.c	1.1	96/11/20 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak lzero = _lzero
#pragma weak lone = _lone
#pragma weak lten = _lten

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>

dl_t	lzero	= {0,  0};
dl_t	lone	= {0,  1};
dl_t	lten	= {0, 10};
