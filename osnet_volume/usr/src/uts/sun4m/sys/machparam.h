/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"@(#)machparam.h	1.49	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent parameters and limits - Sun4m version.
 */

/*
 * Define the VAC symbol (etc.) if we could run on a machine
 * which has a Virtual Address Cache
 *
 * This stuff gotta go.
 */
#define	VAC			/* support virtual addressed caches */

/*
 * This is an artificial limit that will later be dynamic ??
 */
#define	NCPU	4

/*
 * Define the FPU symbol if we could run on a machine with an external
 * FPU (i.e. not integrated with the normal machine state like the vax).
 *
 * The fpu is defined in the architecture manual, and the kernel hides
 * its absence if it is not present, that's pretty integrated, no?
 */

/*
 * MMU_PAGES* describes the physical page size used by the mapping hardware.
 * PAGES* describes the logical page size used by the system.
 */

#define	MMU_PAGESIZE	0x1000		/* 4096 bytes */
#define	MMU_PAGESHIFT	12		/* log2(MMU_PAGESIZE) */
#define	MMU_PAGEOFFSET	(MMU_PAGESIZE-1) /* Mask of address bits in page */
#define	MMU_PAGEMASK	(~MMU_PAGEOFFSET)

#define	MMU_SEGSIZE	0x40000		/* 256 K bytes */
#define	MMU_SEGSHIFT	18
#define	MMU_SEGOFFSET	(MMU_SEGSIZE - 1)
#define	MMU_SEGMASK	(~MMUPAGEOFFSET)

#define	PAGESIZE	0x1000		/* All of the above, for logical */
#define	PAGESHIFT	12
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 * We leave this 8K so we are Sun4 binary compatible.
 */
#define	DATA_ALIGN	0x2000

/*
 * DEFAULT KERNEL THREAD stack size.
 */
#define	DEFAULTSTKSZ	2*PAGESIZE

/*
 * KERNELSIZE is the amount of virtual address space the kernel
 * uses in all contexts.
 */
#define	KERNELSIZE	(256*1024*1024)

/*
 * KERNELBASE is the virtual address which
 * the kernel text/data mapping starts in all contexts.
 */
#define	KERNELBASE	(0-KERNELSIZE)

/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE
#define	USERLIMIT32	USERLIMIT

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 */
#define	MONSTART	(0xffd00000)
#define	ARGSBASE	(MONSTART - NCARGS)

/*
 * Allocate space for kernel modules.
 */
#define	MODSPACE ((1024 * 1024) + (1024 * 512))

/*
 * SYSBASE is the virtual address which
 * the kernel allocated memory mapping starts in all contexts.
 */
#define	SYSLIMIT	0xFBD00000	/* DEBUGADDR */
#define	SYSBASE		(SYSLIMIT - (SYSPTSIZE * MMU_PAGESIZE))

#define	NKL2PTS		((0 - KERNELBASE) / L2PTSIZE)
#define	NKL3PTS		((0 - KERNELBASE) / L3PTSIZE)

/*
 * Size of a kernel threads stack.  It must be a whole number of pages
 * since the segment it comes from will only allocate space in pages.
 */
#define	T_STACKSZ	2*PAGESIZE

/*
 * Use the tmpunload fast context switching srmmu code on sun4m's
 * to dramatically increase SX graphics performance.
 */
#define	SRMMU_TMPUNLOAD

/*
 * Page coloring values.  The deferred flag is to prevent memory
 * fragmentation due to page coloring before sx_cmem has been allocated.
 */
#define	PG_COLORING_ON		1
#define	PG_COLORING_TWOCOLORS	2
#define	PG_COLORING_DEFERRED    (1 << 8)


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
