/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VM_MACHPARAM_H
#define	_SYS_VM_MACHPARAM_H

#pragma ident	"@(#)vm_machparam.h	1.37	98/06/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent constants for Sun4m.
 */

/*
 * USRTEXT is the start of the user text/data space.
 */
#define	USRTEXT		0x2000

/*
 * Virtual memory related constants for UNIX resource control, all in bytes
 * The default stack size of 8M allows an optimization of mmu mapping
 * resources so that in normal use a single mmu region map entry (smeg)
 * can be used to map both the stack and shared libraries
 */
#define	MAXSSIZ		(0x7ffff000)	/* max stack size limit */
#define	DFLSSIZ		(8*1024*1024)	/* initial stack size limit */

/*
 * DSIZE_LIMIT and SSIZE_LIMIT exist to work-around an SVVS bug (1094085),
 * and should be removed from the kernel (1094089)
 */

#define	DSIZE_LIMIT	(USERLIMIT-USRTEXT)	/* physical data limit */
#define	SSIZE_LIMIT	(0x7fffffff)	/* physical stack limit */

/*
 * Size of the kernel segkmem system pte table.  This virtual
 * space is controlled by the resource map "kernelmap".
 */
#define	SYSPTSIZE	((100*1024*1024) / MMU_PAGESIZE)

/*
 * Minimum allowable virtual address space to be used
 * by the seg_map segment driver for fast kernel mappings.
 */
#define	MINMAPSIZE	0x200000


/*
 * The virtual address space to be used by the seg_map segment
 * driver for fast kernel mappings.
 */
#define	SEGMAPSIZE	0x1000000

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time. You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20

/*
 * A swapped in process is given a small amount of core without being bothered
 * by the page replacement algorithm. Basically this says that if you are
 * swapped in you deserve some resources. We protect the last SAFERSS
 * pages against paging and will just swap you out rather than paging you.
 * Note that each process has at least UPAGES pages which are not
 * paged anyways so this number just means a swapped in process is
 * given around 32k bytes.
 */
/*
 * nominal ``small'' resident set size
 * protected against replacement
 */
#define	SAFERSS		3

/*
 * DISKRPM is used to estimate the number of paging i/o operations
 * which one can expect from a single disk controller.
 *
 * XXX - The system doesn't account for multiple swap devices.
 */
#define	DISKRPM		60

/*
 * The maximum value for handspreadpages which is the the distance
 * between the two clock hands in pages.
 */
#define	MAXHANDSPREADPAGES	((64 * 1024 * 1024) / PAGESIZE)

/*
 * Paged text files that are less than PGTHRESH bytes
 * may be "prefaulted in" instead of demand paged.
 */
#define	PGTHRESH	(280 * 1024)

/*
 * Cacheable bit for 64 bit MXCC Stream Source registers
 */
#define	BC_CACHE_SHIFT	36

/*
 * set type for 64 bit phys addr variables.  Needed at least for interface
 * with MXCC.
 */

#ifndef _ASM
typedef unsigned long long pa_t;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VM_MACHPARAM_H */
