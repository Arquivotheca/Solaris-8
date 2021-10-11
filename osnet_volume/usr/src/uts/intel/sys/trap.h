/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#pragma ident	"@(#)trap.h	1.9	99/05/04 SMI"

#if defined(i386) || defined(__i386)

#include <ia32/sys/trap.h>

#elif defined(__ia64)

#include <ia64/sys/trap.h>

#endif

#endif	/* _SYS_TRAP_H */
