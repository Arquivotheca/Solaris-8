/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.1	99/05/04 SMI"

#if defined(i386) || defined(__i386)

#include <ia32/sys/pte.h>

#elif defined(__ia64)

#include <ia64/sys/pte.h>

#endif

#endif	/* _SYS_PTE_H */
