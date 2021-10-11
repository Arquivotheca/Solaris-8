/*
 * Copyright (c) 1990,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPU_H
#define	_SYS_CPU_H

#pragma ident	"@(#)cpu.h	1.8	99/05/04 SMI"

/*
 * This file contains common identification and reference information
 * for all sparc-based kernels.
 *
 * Coincidentally, the arch and mach fields that uniquely identifies
 * a cpu is what is stored in either nvram or idprom for a platform.
 * XXX: This may change!
 */

/*
 * Include generic bustype cookies.
 */
#include <sys/bustypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CPU_ARCH	0xf0		/* mask for architecture bits */
#define	CPU_MACH	0x0f		/* mask for machine implementation */

#define	CPU_ANY		(CPU_ARCH|CPU_MACH)
#define	CPU_NONE	0

/*
 * intel architectures
 */

#define	I86_386_ARCH	0x10		/* arch value for i386 */
#define	I86_486_ARCH	0x20		/* arch value for i486 */
#define	I86_P5_ARCH	0x30		/* arch value for P5   */

#define	I86_PC		0x01

#define	I86PC_386	(I86_386_ARCH + I86_PC)
#define	I86PC_486	(I86_486_ARCH + I86_PC)
#define	I86PC_P5	(I86_P5_ARCH  + I86_PC)

#define	CPU_386		(I86_386_ARCH + CPU_MACH)
#define	CPU_486		(I86_486_ARCH + CPU_MACH)
#define	CPU_P5		(I86_P5_ARCH  + CPU_MACH)


/*
 * Global kernel variables of interest
 */

#if defined(_KERNEL) && !defined(_ASM)
extern short cputype;			/* machine type we are running on */

#endif /* defined(_KERNEL) && !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPU_H */
