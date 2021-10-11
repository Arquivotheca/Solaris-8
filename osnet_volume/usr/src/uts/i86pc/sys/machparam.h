/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"@(#)machparam.h	1.33	99/10/22 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent parameters and limits - PC version.
 */
#define	NCPU 	21

/*
 * The value defined below could grow to 16. hat structure and
 * machpage_t have room for 16 nodes.
 */
#define	MAXNODES 	4
#define	NUMA_NODEMASK	0x0f


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

#define	PAGESIZE	0x1000		/* All of the above, for logical */
#define	PAGESHIFT	12
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 */
#define	DATA_ALIGN	PAGESIZE

/*
 * DEFAULT KERNEL THREAD stack size.    XXX should this be 1???
 */
#define	DEFAULTSTKSZ	2*PAGESIZE

/*
 * KERNELSIZE is the default amount of virtual address space the kernel
 * uses in all contexts.
 */
#define	KERNELSIZE	(512*1024*1024)

/*
 * KERNELBASE is the virtual address at which
 * the kernel segments start in all contexts.
 * KERNELBASE is no longer fixed. The value of KERNELBASE could change
 * with installed memory and the eprom variable 'eprom_kernelbase'.
 * common/conf/param.c requires a compile time defined value for KERNELBASE,
 * which it saves in the variable _kernelbase.
 * We will use the 2.6 value (0xE0000000)  as the default value and overwrite
 * _kernelbase with the correct value in i86pc/os/startup.c
 * i86 and i86pc files use kernelbase instead of KERNLEBASE, which is
 * initialized in i86pc/os/startup.c.
 */
#define	KERNELBASE	(0-KERNELSIZE)
#if	!defined(_KADB)
extern uintptr_t kernelbase;
#endif

/*
 * KERNELBASE can't be tuned below this value. 386 ABI requires that
 * user be provided with atleast 3Gb of virtual space.
 */
#define	KERNELBASE_ABI_MIN	0xc0000000
/*
 * This is the last 4MB of the 4G address space. Some psm modules
 * need this region of virtual address space mapped 1-1
 */
#define	PROMSTART	(0xffc00000)
#define	KERNEL_TEXT	(0xfe800000)
#define	SEGKMAP_START	(0xfd800000)

/*
 * In Solaris 2.6 and before segkp shrinks in size as more memory is
 * installed. So, a 4Gb system would support less number of lwp's
 * than a 2Gb system. The same behavior is true in 2.7 when the system
 * has less than 4Gb of memory. When the system has more than 4Gb of
 * memory, we lower KERNELBASE if necessary to accommodate a segkp of
 * size SEGKPSIZE_DEFAULT.
 */
#define	SEGKPSIZE_DEFAULT (200 * 1024 * 1024)

/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE
#define	USERLIMIT32	USERLIMIT

/*
 * SYSBASE is the virtual address at which
 * the kernel allocated memory mapping starts in all contexts.
 * SYSLIMIT - SYSBASE is the maximum amount of memory that can be allocated
 * from kernelmap.  Currently we want this to be about 240 MB.  On
 * systems with less than 4Gb of memory, the trade-off
 * is that big servers hang if you don't provide enough kernelmap, but segkp
 * shrinks (consequently reducing the number of concurrent lwps) if you make
 * it too big.
 * The eprom variable 'eprom-kernelbase' can be used to increase the amount of
 * memory allocated from kernelmap.
 */
#define	SEGKMEMSIZE_DEFAULT	(240 * 1024 * 1024)

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 */
#define	ARGSBASE	PROMSTART

/*
 * reserve space for modules
 */
#define	MODTEXT	(1024 * 1024 * 2)
#define	MODDATA	(1024 * 300)

/*
 * Size of a kernel threads stack.  It must be a whole number of pages
 * since the segment it comes from will only allocate space in pages.
 */
#define	T_STACKSZ	2*PAGESIZE

/*
 * Bus types
 */
#define	BTISA		1
#define	BTEISA		2
#define	BTMCA		3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
