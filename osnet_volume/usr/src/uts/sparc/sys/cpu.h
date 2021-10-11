/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPU_H
#define	_SYS_CPU_H

#pragma ident	"@(#)cpu.h	1.18	99/10/22 SMI"

/*
 * Include generic bustype cookies.
 */
#include <sys/bustypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Global kernel variables of interest
 */

#if defined(_KERNEL) && !defined(_ASM)

extern int dvmasize;			/* usable dvma size in pages */

/*
 * Cache defines - right now these are only used on sun4m machines
 *
 * Each bit represents an attribute of the system's caches that
 * the OS must handle.  For example, VAC caches must have virtual
 * alias detection, VTAG caches must be flushed on every demap, etc.
 */
#define	CACHE_NONE		0	/* No caches of any type */
#define	CACHE_VAC		0x01	/* Virtual addressed cache */
#define	CACHE_VTAG		0x02	/* Virtual tagged cache */
#define	CACHE_PAC		0x04	/* Physical addressed cache */
#define	CACHE_PTAG		0x08	/* Physical tagged cache */
#define	CACHE_WRITEBACK		0x10	/* Writeback cache */
#define	CACHE_IOCOHERENT	0x20	/* I/O coherent cache */

extern int cache;

/*
 * Virtual Address Cache defines- right now just determine whether it
 * is a writeback or a writethru cache.
 *
 * MJ: a future merge with some of the sun4m structure defines could
 * MJ: tell us whether or not this cache has I/O going thru it, or
 * MJ: whether it is consistent, etc.
 */

#define	NO_VAC		0x0
#define	VAC_WRITEBACK	0x1	/* this vac is a writeback vac */
#define	VAC_WRITETHRU	0x2	/* this vac is a writethru vac */
#define	VAC_IOCOHERENT	0x100	/* i/o uses vac consistently */

/* set this to zero if no vac */
extern int vac;

#ifdef	BCOPY_BUF
extern int bcopy_buf;			/* there is a bcopy buffer */
#else
#define	bcopy_buf 0
#endif	/* BCOPY_BUF */

#endif /* defined(_KERNEL) && !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPU_H */
