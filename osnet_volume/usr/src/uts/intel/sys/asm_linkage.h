/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#pragma ident	"@(#)asm_linkage.h	1.10	99/05/04 SMI"

#if defined(i386) || defined(__i386)

#include <ia32/sys/asm_linkage.h>

#elif defined(__ia64)

#include <ia64/sys/asm_linkage.h>

#endif

#endif	/* _SYS_ASM_LINKAGE_H */
