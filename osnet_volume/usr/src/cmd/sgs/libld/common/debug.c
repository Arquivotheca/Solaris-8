/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)debug.c	1.10	98/08/28 SMI"

#include	<stdio.h>
#include	<dlfcn.h>
#include	"debug.h"
#include	"_ld.h"

/*
 * It's possible that dbg_setup may be called more than once.
 */
static int	dbg_init = 0;

#ifndef	DEBUG

int
/* ARGSUSED0 */
dbg_setup(const char * options)
{
	if (!dbg_init) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_DBG_NOINABLE));
		dbg_init++;
	}
	return (0);
}

#else

int
dbg_setup(const char * options)
{
	if (dbg_init)
		return (0);
	/*
	 * Call the debugging setup routine to initialize the mask and
	 * debug function array.
	 */
	return (Dbg_setup(options));
}

#endif
