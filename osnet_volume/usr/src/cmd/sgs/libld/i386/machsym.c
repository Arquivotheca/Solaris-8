/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machsym.c	1.3	98/08/28 SMI"

#include	<stdio.h>
#include	"debug.h"
#include	"_libld.h"


/*
 * This file contains stub routines since currently register symbols
 * are not relevant to the i386 architecture.  But - having these
 * stub routines avoids #ifdefs in common codes - and I hate that.
 */

/* ARGSUSED */
uintptr_t
mach_sym_typecheck(Sym_desc * sdp, Sym * nsym, Ifl_desc * ifl, Ofl_desc * ofl)
{
	return (1);
}

/* ARGSUSED */
uintptr_t
add_regsym(Sym_desc * sdp, Ofl_desc * ofl)
{
	return (1);
}

/* ARGSUSED */
uintptr_t
mach_dyn_locals(Sym * syms, const char * strs, size_t locals,
	Ifl_desc * ifl, Ofl_desc * ofl)
{
	return (0);
}
