/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MMU_H
#define	_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.1	99/05/04 SMI"

#if defined(i386) || defined(__i386)

#include <ia32/sys/mmu.h>

#elif defined(__ia64)

#include <ia64/sys/mmu.h>

#endif

#endif	/* _SYS_MMU_H */
