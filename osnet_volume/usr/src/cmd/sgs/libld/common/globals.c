/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)globals.c	1.27	99/10/22 SMI"

/*
 * Global variables
 */
#include	<stdio.h>
#include	"_libld.h"

Ofl_desc	Ofl;		/* provided for signal handler */
Ld_heap *	ld_heap;	/* list of allocated blocks for link-edit */
				/*	dynamic allocations */

/*
 * liblddbg sometimes takes an ehdr in order to figure out the elf class or
 * machine type.  Symobls that are added by ld, such as _etext, don't have a
 * corresponding ehdr, so we pass this instead.
 */
Ehdr		def_ehdr = { { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
			M_CLASS, M_DATA }, 0, M_MACH };
