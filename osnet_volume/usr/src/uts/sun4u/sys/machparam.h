/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"@(#)machparam.h	1.53	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
#define	ADDRESS_C(c)    c ## ul
#else   /* _ASM */
#define	ADDRESS_C(c)    (c)
#endif	/* _ASM */

/*
 * Machine dependent parameters and limits - sun4u version.
 */

/*
 * Define the VAC symbol (etc.) if we could run on a machine
 * which has a Virtual Address Cache
 *
 * This stuff gotta go.
 */
#define	VAC			/* support virtual addressed caches */

/*
 * The maximum possible number of UPA devices in a system.
 */
#ifdef	_STARFIRE
/*
 * We have a 7 bit id space for UPA devices in Xfire
 */
#define	MAX_UPA			128
#else
#define	MAX_UPA			32
#endif	/* _STARFIRE */

/*
 * Maximum number of CPUs we support
 */
#if	(defined(_STARFIRE) && !defined(lint))
#define	NCPU	64
#else
#define	NCPU	32
#endif	/* _STARFIRE && !lint */

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
#define	MMU_PAGE_SIZES		4	/* supported page sizes by mmu */

/*
 * XXX make sure the MMU_PAGESHIFT definition here is
 * consistent with the one in param.h
 */
#define	MMU_PAGESHIFT		13
#define	MMU_PAGESIZE		(1<<MMU_PAGESHIFT)
#define	MMU_PAGEOFFSET		(MMU_PAGESIZE - 1)
#define	MMU_PAGEMASK		(~MMU_PAGEOFFSET)

#define	MMU_PAGESHIFT64K	16
#define	MMU_PAGESIZE64K		(1 << MMU_PAGESHIFT64K)
#define	MMU_PAGEOFFSET64K	(MMU_PAGESIZE64K - 1)
#define	MMU_PAGEMASK64K		(~MMU_PAGEOFFSET64K)

#define	MMU_PAGESHIFT512K	19
#define	MMU_PAGESIZE512K	(1 << MMU_PAGESHIFT512K)
#define	MMU_PAGEOFFSET512K	(MMU_PAGESIZE512K - 1)
#define	MMU_PAGEMASK512K	(~MMU_PAGEOFFSET512K)

#define	MMU_PAGESHIFT4M		22
#define	MMU_PAGESIZE4M		(1 << MMU_PAGESHIFT4M)
#define	MMU_PAGEOFFSET4M	(MMU_PAGESIZE4M - 1)
#define	MMU_PAGEMASK4M		(~MMU_PAGEOFFSET4M)

#define	PAGESHIFT	13
#define	PAGESIZE	(1<<PAGESHIFT)
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 */
#define	DATA_ALIGN	ADDRESS_C(0x2000)

/*
 * DEFAULT KERNEL THREAD stack size.
 */
#ifdef __sparcv9
#define	DEFAULTSTKSZ	(2*PAGESIZE)
#else
#define	DEFAULTSTKSZ	PAGESIZE
#endif

/*
 * DEFAULT initial thread stack size.
 */
#define	T0STKSZ		(2 * DEFAULTSTKSZ)

/*
 * KERNELBASE is the virtual address which
 * the kernel text/data mapping starts in all contexts.
 */
#define	KERNELBASE	ADDRESS_C(0x10000000)

/*
 * Define the userlimits
 */
#ifdef __sparcv9
#define	USERLIMIT	ADDRESS_C(0xFFFFFFFF80000000)
#define	USERLIMIT32	ADDRESS_C(0xFFBF0000)
#else
#define	USERLIMIT	ADDRESS_C(0xFFBF0000)
#define	USERLIMIT32	USERLIMIT
#endif

/*
 * Define SEGKPBASE, start of the segkp segment.
 */
#ifdef	__sparcv9
#define	SEGKPBASE	ADDRESS_C(0x2a100000000)
#else
#define	SEGKPBASE	ADDRESS_C(0x40000000)
#endif

/*
 * Define SEGMAPBASE, start of the segmap segment.
 */
#ifdef __sparcv9
#define	SEGMAPBASE	ADDRESS_C(0x2a750000000)
#else
#define	SEGMAPBASE	ADDRESS_C(0x60000000)
#endif

/*
 * SYSBASE is the virtual address which the kernel allocated memory
 * mapping starts in all contexts.  SYSLIMIT is the end of the Sysbase segment.
 */
#ifdef __sparcv9
#define	SYSBASE		ADDRESS_C(0x30000000000)
#define	SYSLIMIT	ADDRESS_C(0x31000000000)
#define	SYSBASE32	ADDRESS_C(0x78000000)
#define	SYSLIMIT32	ADDRESS_C(0x7c000000)
#else
#define	SYSBASE		ADDRESS_C(0x70000000)
#define	SYSLIMIT	ADDRESS_C(0xEDD00000)
#endif

/*
 * KADBBASE is the address where kadb is mapped.
 */
#define	KADBBASE	ADDRESS_C(0xEDD00000)

/*
 * MEMSCRUBBASE is the base virtual address for the memory scrubber
 * to read large pages.  It MUST be 4MB page aligned.
 */
#ifdef __sparcv9
#define	MEMSCRUBBASE	0x2a000000000
#else
#define	MEMSCRUBBASE	(SEGKPBASE - 0x400000)
#endif

/*
 * Define the kernel address space range allocated to Open Firmware
 */
#define	OFW_START_ADDR	0xf0000000
#define	OFW_END_ADDR	0xffffffff

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 */
#define	ARGSBASE	(MEMSCRUBBASE - NCARGS)

/*
 * PPMAPBASE is the base virtual address of the range which
 * the kernel uses to quickly map pages for operations such
 * as ppcopy, pagecopy, pagezero, and pagesum.
 */
#define	PPMAPSIZE	(512 * 1024)
#define	PPMAPBASE	(ARGSBASE - PPMAPSIZE)

#define	PP_SLOTS	ADDRESS_C(8)
#define	PPMAP_FAST_SIZE	(PP_SLOTS * PAGESIZE * NCPU)
#define	PPMAP_FAST_BASE	(PPMAPBASE - PPMAP_FAST_SIZE)

/*
 * Allocate space for kernel modules on nucleus pages
 */
#define	MODTEXT	1024 * 1024 * 2
#define	MODDATA	1024 * 256

/*
 * Preallocate an area for setting up the user stack during
 * the exec(). This way we have a faster allocator and also
 * make sure the stack is always VAC aligned correctly. see
 * get_arg_base() in startup.c.
 */
#define	ARG_SLOT_SIZE	(0x8000)
#define	ARG_SLOT_SHIFT	(15)
#define	N_ARG_SLOT	(0x80)

#define	NARG_BASE	(PPMAP_FAST_BASE - (ARG_SLOT_SIZE * N_ARG_SLOT))

/*
 * ktextseg+kvalloc should not use space beyond KERNEL_LIMIT.
 */
#ifdef __sparcv9
#define	KERNEL_LIMIT	(SYSBASE32)
#else
#define	KERNEL_LIMIT	(NARG_BASE)
#endif

#ifdef __sparcv9
/*
 * This is just dead cruft -- use DEFAULTSTKSZ above
 */
#else
/*
 * Size of a kernel threads stack.  It must be a whole number of pages
 * since the segment it comes from will only allocate space in pages.
 */
#define	T_STACKSZ	PAGESIZE
#endif

#define	PFN_TO_BUSTYPE(pfn)	(((pfn) >> 19) & 0x1FF)
#define	BUSTYPE_TO_PFN(btype, pfn)			\
	(((btype) << 19) | ((pfn) & 0x7FFFF))
#define	IO_BUSTYPE(pfn)	((PFN_TO_BUSTYPE(pfn) & 0x100) >> 8)

#ifdef	_STARFIRE
#define	PFN_TO_UPAID(pfn)	BUSTYPE_TO_UPAID(PFN_TO_BUSTYPE(pfn))
#else
#define	PFN_TO_UPAID(pfn)	(((pfn) >> 20) & 0x1F)
#endif	/* _STARFIRE */

#ifndef	_ASM
/*
 * Example buffer control and data headers stored in locore.s:
 */
typedef union {
	struct _ptl1_d {
		u_longlong_t	ptl1_tstate;
		u_longlong_t	ptl1_tick;
		u_longlong_t	ptl1_tpc;
		u_longlong_t	ptl1_tnpc;
		ushort_t	ptl1_tt;
		ushort_t	ptl1_tl;
	} d;
	uchar_t		cache_linesize[64];
} PTL1_DAT;

extern	void		ptl1_panic(uint_t reason);
extern	uint_t		ptl1_panic_cpu;
extern	uint_t		ptl1_panic_tr;
extern	PTL1_DAT	ptl1_dat[];
extern	char		ptl1_stk[];

#endif	/* _ASM */

/*
 * Defines used for ptl1_panic parameter.
 * %g1 comes in with one of these values.
 */
#define	PTL1_BAD_WTRAP		1
#define	PTL1_BAD_KMISS		2
#define	PTL1_BAD_KPROT_TL0	3
#define	PTL1_BAD_KPROT_FAULT	4
#define	PTL1_BAD_KPROT_INVAL	5
#define	PTL1_BAD_ISM		6
#define	PTL1_BAD_TTE_PA		7
#define	PTL1_BAD_MMUTRAP	8
#define	PTL1_BAD_TRAP		9
#define	PTL1_BAD_FPTRAP		10
#define	PTL1_BAD_8KTSBP		11
#define	PTL1_BAD_4MTSBP		12

/*
 * Defines used for tl1_bad_trap stack and related data structs.
 */
#define	PTL1_MAXTL		4
#define	PTL1_SSIZE		DEFAULTSTKSZ
#define	PTL1_SIZE_SHIFT		6
#define	PTL1_SIZE		(1 << PTL1_SIZE_SHIFT)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
