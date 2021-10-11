/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)ldglobals.c	1.14	97/05/31 SMI"

/*
 * Global variables
 */
#include	<stdio.h>
#include	"_ld.h"

/*
 * Debugging mask (refer to include/debug.h).
 */
/* int		dbg_mask; */

/*
 * List of support libraries specified (-S option).
 */
List		lib_support;

/*
 * Define a local variable to track whether input files have been encountered.
 */
Boolean	files =	FALSE;

/*
 * Paths and directories for library searches.  These are used to set up
 * linked lists of directories which are maintained in the ofl structure.
 */
char *		Plibpath;	/* User specified -YP or defaults to LIBPATH */
char *		Llibdir;	/* User specified -YL */
char *		Ulibdir;	/* User specified -YU */
Listnode *	insert_lib;	/* insertion point for -L libraries */
