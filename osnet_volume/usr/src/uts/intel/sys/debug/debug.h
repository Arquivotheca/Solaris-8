/*
 * Copyright (c) 1989,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DEBUG_DEBUG_H
#define	_SYS_DEBUG_DEBUG_H

#pragma ident	"@(#)debug.h	1.10	99/08/19 SMI"

#include <sys/types.h>		/* for caddr_t */
#include <sys/consdev.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef int (*func_t)();

/*
 * The debugger gets four megabytes virtual address range in which it
 * can reside.  It is hoped that this space is large enough to accommodate
 * the largest kernel debugger that would be needed but not too large to
 * cramp the kernel's virtual address space.
 */
#define	DEBUGSIZE	0x400000
#define	DEBUGSTART	0xFF800000

/*
 * The debugvec structure (below) is versioned.
 */
#define	DEBUGVEC_VERSION_2		2	/* First Intel version */

typedef void (*cons_t)(cons_polledio_t *);

struct debugvec {
	unsigned dv_version;	/* structure version */
	cons_t	dv_set_polled_callbacks; /* call to set polled callbacks */
};

#if	defined(_KERNEL) && !defined(_KADB)
extern struct debugvec *dvec;
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUG_H */
