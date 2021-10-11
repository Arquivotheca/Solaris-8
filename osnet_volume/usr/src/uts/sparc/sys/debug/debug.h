/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DEBUG_DEBUG_H
#define	_SYS_DEBUG_DEBUG_H

#pragma ident	"@(#)debug.h	1.35	99/05/25 SMI"	/* SunOS-4.0 1.15 */

#ifndef	_ASM

#include <sys/types.h>		/* for caddr_t */
#include <sys/consdev.h>

#endif	/* ! _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file describes the interface between the kernel their debuggers.
 * Actually, the term ``kernel'' here is too restrictive - it actually
 * applies to any standalone program which is to be run under a debugger.
 * The term ``debugger'' here applies to any debugger service which
 * uses this interface.
 *
 * The debugger requires that nobody mess with parts of virtual
 * memory if they expect any debugger services.  These rules
 * apply to all the virtual memory between DEBUGSTART and DEBUGEND:
 *
 *   *	Do not write to these addresses.
 *   *	Do not read from (depend on the contents of) these addresses,
 *	except as documented in the debugvec structure.
 *   *	Do not remap these addresses.
 *   *	Do not change or double-map the pmegs that these addresses
 *	map through.
 *   *	Do not change or double-map the main memory that these
 *	addresses map to.
 *   *	These rules apply in
 *		all map contexts (Sun-3, Sun-4, Sun-386i)
 *		the kernel context 0 (Sun-2)
 *
 * Besides the rules for virtual memory cooperation, the following
 * debugger/debuggee interface is defined:
 *
 *   *	If a debugger is present, it will pass in a '-d' flag
 *	to the program being debugged (although the program being
 *	debugged might want to verify that the debugger is actually
 *	present before taking it as the truth to avoid problems
 *	with users using the wrong flags in the wrong places).
 *	The '-d' flag being passed into the debugger itself will
 *	cause the debugger to prompt for debuggee program name and
 *	allow debugger commands before debuggee is started.
 *   *	The top *dvec->dv_pages tell the number of pages
 *	taken from the end of memory.  If the debuggee is
 *	managing all memory, the amount of usable memory is:
 *	  *romp->v_memorysize  - mmu_ptob(*dvec->dv_pages) [earlier PROMs]
 *	  *romp->v_memoryavail - mmu_ptob(*dvec->dv_pages) [later PROMs]
 *   *	If the debuggee changes the trace exception handler or
 *	the trap exception handler for TRAPBRKNO, it MUST call
 *	(*dvec->dv_scbsync)() after all exception handler changes
 *	are made.  Other exception handlers can be changed
 *	without any special provisions.
 *   *	If the debuggee is going to relocate itself, it is very
 *	desirable to have it call (*dvec->dv_scbsync)() soon
 *	AFTER the relocation to the correct virtual address.
 *   *	The debuggee can call the debugger at any time by
 *	an explicit call using the CALL_DEBUG() macro,
 *	a ``trap #TRAPBRKNO'' (68000), ``t TRAPBRKNO'' (sparc),
 *	``int 3'' (i386), or by jumping to *dvec->dv_trap.
 *   *	The debuggee is expected not to try and trace itself
 *	under the debugger - the only way the trace bit can
 *	be turned on in the debuggee is via the ``rte''
 *	instruction.  This case is detected and handled
 *	correctly by the debugger when single stepping,
 *	except in the case were the rte is returning to
 *	another rte which is returning to user land
 *	(welcome to the real world of 680x0 processors).
 */
#define	TRAPBRKNO	127			/* breakpoint trap no */

/*
 * XXX	Stop press:
 *
 * Note that the remarks in this file that deal with the
 * virtual address range occupied by the debugger are somewhat out
 * of date - in 5.0, the exact place the debugger sits
 * in memory is implicitly described by the 'virt-avail' list
 * exported by boot.
 *
 * If you *really* need to know what used to be called 'DEBUGSTART'
 * then issue a BOP_GETPROP() on the 'debugger-start' property which
 * is a property provided by the debugger itself.
 *
 * XXX	We should probably do the same thing for the 'DEBUGSIZE' value too.
 */

/*
 * The debugger gets a one megabyte virtual address range in which it
 * can reside.  It is hoped that this space is large enough to accommodate
 * the largest kernel debugger that would be needed but not too large to
 * cramp the kernel's virtual address space.  We locate the debugger
 * in the megabyte before the PROM monitor, except on the 386, where we
 * locate it 16 megabytes (minus 20k) in front of the monitor.
 */
/*
 * If we compile with LP64, kadb gets much larger.
 * Just bump up the size since we actually still have room.
 */
#ifdef	sun4u
#define	DEBUGSIZE	0x200000
#else
#define	DEBUGSIZE	0x100000
#endif

/*
 * The debugvec structure (below) is now versioned.
 * The dv_version field was not in the original structure. However,
 * the initialised structure in the kadb assembly code has always
 * had a word set to zero in the place where dv_version is now placed.
 */
#define	DEBUGVEC_VERSION_0		0
#define	DEBUGVEC_VERSION_1		1
#define	DEBUGVEC_VERSION_2		2

#ifndef	_ASM

#ifdef	__cplusplus
}
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * func_t no longer used in this header.  Several files including this
 * file rely on it though.
 */
typedef int (*func_t)();
typedef void (*vfunc_t)(void);
typedef void (*formf_t)(caddr_t *);
/*
 * The argument passed with KADB_CPU_XCALL is 0 for CPU not accepting
 * x-calls, non-0 for the reverse.
 */
typedef enum { KADB_CPU_XCALL } kadb_cpu_attr_t;
typedef void (*cpuf_t)(int, kadb_cpu_attr_t, int);
typedef void (*cons_t)(cons_polledio_t *);


/*
 * New versions of this structure must always start with the same
 * first five members of the version 0 structure.
 */
struct debugvec {
	/* The original debugvec structure - version 0. */
	int	dv_entry;	/* entry point into debugger */
	vfunc_t	dv_trap;	/* function to trap to enter debugger */
	int	*dv_pages;	/* ptr to # of pages stolen */
	vfunc_t	dv_scbsync;	/* function to call after scb is changed */
	unsigned dv_version;	/* structure version */
	/* The extensions - version 1. */
	formf_t	dv_format;	/* call to get words kadb defines to the prom */
	vfunc_t	dv_arm;		/* call to arm the prom to use the kadb words */
	cpuf_t	dv_cpu_change;	/* change kadb cpu attribute */
	/* The extensions - version 2. */
	cons_t	dv_set_polled_callbacks; /* call to set polled callbacks */
};

/*
 * If the version is 0, the members after dv_version will not exist in
 * the debugger (kadb) initialised version of this structure.
 * Function pointers dv_format, dv_arm and dv_cpu_change will be
 * NULL if not supported.
 *
 * If the version is 1, then the dv_set_polled_callbacks field will not
 * exist in the debugger (kadb) initialised version of this structure.
 * Version 2 of this structure supports the dv_set_polled_callbacks
 * field.
 */

/*
 *  Note that this is passed in %o2 by /boot when the debugger is loaded.
 */
extern struct debugvec *dvec;

#endif	/* ! _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUG_H */
