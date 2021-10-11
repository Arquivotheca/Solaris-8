/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma	ident	"@(#)values-Xa.c	1.4	96/11/26 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <math.h>

/*
 * variables which differ depending on the
 * compilation mode
 *
 * ANSI conforming mode
 * This file is linked into the a.out immediately following
 * the startup routine if the -Xa compilation mode is selected
 */

const enum version _lib_version = ansi_1;
