/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)values-Xc.c	1.3	96/11/26 SMI"	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <math.h>

/*
 * variables which differ depending on the
 * compilation mode
 *
 * Strict ANSI mode
 * This file is linked into the a.out immediately following
 * the startup routine if the -Xc compilation mode is selected
 */

const enum version _lib_version = strict_ansi;
