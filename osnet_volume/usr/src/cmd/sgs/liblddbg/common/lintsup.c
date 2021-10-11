/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc. 
 * All rights reserved. 
 */ 

#pragma ident	"@(#)lintsup.c	1.1	98/08/28 SMI"

/*
 * Supplimental definitions for lint that help us avoid
 * options like `-x' that filter out things we want to
 * know about as well as things we don't.
 */
#include "libld.h"

/*
 * Get the Elf32 side to think that the _ELF64 side
 * is defined, and vice versa.
 */
#if	defined(_ELF64)
#undef	_ELF64
#include "debug.h"
#define	_ELF64
#else
#define	_ELF64
#include "debug.h"
#undef	_ELF64
#endif

void	Dbg_reloc_doactiverel(void);
