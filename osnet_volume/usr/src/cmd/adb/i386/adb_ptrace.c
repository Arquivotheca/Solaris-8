/*
 * Copyright (c) 1987, 1988, 1989, 1990, 1991, 1992 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)adb_ptrace.c	1.5	97/11/21 SMI"

#ifdef _KERNEL
#	undef	_KERNEL
#	include "ptrace.h"
#	define	_KERNEL
#else	/* !_KERNEL */
#	include "ptrace.h"
#endif	/* !_KERNEL */

#ifndef KADB
#include <sys/types.h>
#endif
#include <sys/debug/debugger.h>
#include "adb.h"
#ifndef KADB
#include "allregs.h"
#endif

/*
 * This will replace all of adb's ptrace calls that might be asking
 * for a single-step operation, which must be simulated.
 * These all happen to be located in runpcs.c.
 */

typedef enum {
	br_error,
	not_branch,
	bicc,
	bicc_annul,
	ba,
	ba_annul,
	ticc,
	ta
} br_type;

#ifdef KADB
extern debugging;
#endif	
/*
 * The chg_pc argument allows us to tell the kernel whether the user
 * explicitly asked to change the pc.  If so (chg_pc != 0), the kernel
 * will set PC to the upc argument.  Otherwise, we just give the kernel
 * a "1" for the PC, indicating that it should just go on from "here".
 */
adb_ptrace(mode, pid, upc, xsig, chg_pc)
	int mode;
	pid_t pid;
	int upc, xsig, chg_pc;
{
	int rtn;
	struct bkpt bk_upc;
	caddr_t	ptpc;				/* pc to give ptrace */
#ifdef KADB
	extern caddr_t	systrap;		/* address of kernel's trap() */
	struct bkpt bk_systrap;
	extern struct regs adb_regs;
#else
	extern struct allregs adb_regs;
#endif

	ptpc =  chg_pc ?  (caddr_t) upc  :  (caddr_t) 1 ;

#if defined(KADB)
	if (mode != PTRACE_SINGLESTEP)
#endif	/* KADB */
		return ptrace(mode, pid, ptpc, xsig);

#if defined(KADB)
	/*
	 * We use the 386/486 trace hardware to trace one instruction.
	 * This is much more simple then the sparc version.
	 */
	ss_traceflg();
	(void) ptrace( PTRACE_SINGLESTEP, pid, upc, xsig ); /*set dotrace */
	rtn = ptrace ( PTRACE_CONT, pid, 1, xsig );
	bpwait(PTRACE_CONT);
        chkerr();
	return rtn;
#endif	/* defined(KADB) */
}

static
ss_traceflg(void)
{
	reg->r_efl |= 0x100;	/* turn on single-step flag */
}
