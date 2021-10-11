/*
 * Copyright (c) 1995-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PCTYPES_H
#define	_PCTYPES_H

#pragma ident	"@(#)pctypes.h	1.12	97/11/22 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PCMCIA General Types
 */

typedef int irq_t;		/* IRQ level */
typedef unsigned char *baseaddr_t; /* memory base address */
#if defined(i386)
typedef uint32_t ioaddr_t;
#endif
#if defined(sparc)
typedef caddr_t ioaddr_t;
#endif

typedef uint32_t (*intrfunc_t)(void *);

/*
 * Data access handle definitions for common access functions
 */
typedef void * acc_handle_t;

#if defined(_BIG_ENDIAN)
#define	leshort(a)	((((a) & 0xFF) << 8) | (((a) >> 8) & 0xFF))
#define	lelong(a)	(leshort((a) >> 16) | (leshort(a) << 16))
#else
#define	leshort(a)	(a)
#define	lelong(a)	(a)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PCTYPES_H */
