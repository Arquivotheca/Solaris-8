/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)fp_data.c	1.11	96/12/06 SMI"	/* SVr4.0 1.8	*/

/* LINTLIBRARY */

/*
 * contains the definitions
 * of the global constant __huge_val used
 * by the floating point environment
 */

#include "synonyms.h"
#include <math.h>

/* IEEE infinity */
const _h_val __huge_val = { 0x7ff00000, 0x0 };
