/*
 * Copyright (c) 1986,1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ALLREGS_H
#define	_ALLREGS_H

#pragma ident	"@(#)allregs.h	1.15	98/02/09 SMI"

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "adb_regs" structure.  This is used
 * in different ways for kadb, adb -k, and normal adb.
 *
 * For kadb, this is really the machine state -- so kadb must
 * get the current window pointer (CWP) field of the psr and
 * use it to index into the windows to find the currently
 * relevant register set.
 *
 * For a normal adb, the TBR and WIM registers aren't present in the
 * struct regs that we get (either within a core file, or from
 * PTRACE_GETREGS); so I might use those two fields for something.
 * In this case, we always ignore the "locals" half of register
 * window zero.  Its "ins" half is used to hold the current "outs",
 * and window one has the current locals and "ins".
 *
 * For adb -k (post-mortem examination of kernel crash dumps), there
 * is no way to find the current globals or outs, but the stack frame
 * that sp points to will tell us the current locals and ins.  Because
 * we have no current outs, I suppose that we could use window zero for
 * the locals and ins, but I'd prefer to make it the same as normal adb.
 * Also, if the kernel crash-dumper is changed to make these available
 * somehow, I'd have to change things again.
 */

#ifndef KADB
#include <sys/reg.h>
#endif

#define	MAXKADBWIN	32

#ifndef _ASM

#include <sys/pcb.h>

struct allregs {
	long		r_psr;
	long		r_pc;
	long		r_npc;
	long		r_tbr;
	long		r_wim;
	long		r_y;
	long		r_globals[7];
#ifdef KADB
	struct rwindow	r_window[MAXKADBWIN];	/* locals, then ins */
#else
	long		r_outs[8];
	long		r_locals[8];
	long		r_ins[8];
#endif
};

struct allregs_v9 {
	u_longlong_t	r_tstate;
	long		r_pc;
	long		r_npc;
	long		r_tba;
	long		r_y;
	u_longlong_t	r_globals[7];
#ifdef KADB
	long		r_tt;
	long		r_pil;
	long		r_cwp;
	long		r_otherwin;
	long		r_cleanwin;
	long		r_cansave;
	long		r_canrestore;
	long		r_wstate;
	struct rwindow	r_window[MAXKADBWIN];	/* locals, then ins */
	u_longlong_t	r_outs[8];
#else
	u_longlong_t	r_outs[8];
	long		r_locals[8];
	long		r_ins[8];
#endif
};

#endif	/* _ASM */

/*
 * XXX - some v9 definitions from v9/sys/privregs.h. Need to define here
 * because we've already included v7/sys/privregs.h via other headers.
 * We could/should fix this by splitting up architecture version dependent
 * code.
 */

/*
 * Trap State Register (TSTATE)
 *
 *	|-------------------------------------|
 *	| CCR | ASI | --- | PSTATE | -- | CWP |
 *	|-----|-----|-----|--------|----|-----|
 *	 39 32 31 24 23 20 19	  8 7  5 4   0
 */
#define	TSTATE_CWP_MASK		0x01F
#define	TSTATE_CWP_SHIFT	0
#define	TSTATE_PSTATE_MASK	0xFFF
#define	TSTATE_PSTATE_SHIFT	8
#define	TSTATE_ASI_MASK		0x0FF
#define	TSTATE_ASI_SHIFT	24
#define	TSTATE_CCR_MASK		0x0FF
#define	TSTATE_CCR_SHIFT	32

/*
 * Some handy tstate macros
 */
#define	TSTATE_AG	(PSTATE_AG << TSTATE_PSTATE_SHIFT)
#define	TSTATE_IE	(PSTATE_IE << TSTATE_PSTATE_SHIFT)
#define	TSTATE_PRIV	(PSTATE_PRIV << TSTATE_PSTATE_SHIFT)
#define	TSTATE_AM	(PSTATE_AM << TSTATE_PSTATE_SHIFT)
#define	TSTATE_PEF	(PSTATE_PEF << TSTATE_PSTATE_SHIFT)
#define	TSTATE_MG	(PSTATE_MG << TSTATE_PSTATE_SHIFT)
#define	TSTATE_IG	(PSTATE_IG << TSTATE_PSTATE_SHIFT)
#define	TSTATE_CWP	TSTATE_CWP_MASK

/*
 * Processor State Register (PSTATE)
 *
 *   |-------------------------------------------------------------|
 *   |  IG | MG | CLE | TLE | MM | RED | PEF | AM | PRIV | IE | AG |
 *   |-----|----|-----|-----|----|-----|-----|----|------|----|----|
 *	11   10	   9     8   7  6   5	  4     3     2	    1    0
 */
#define	PSTATE_AG	0x001		/* alternate globals */
#define	PSTATE_IE	0x002		/* interrupt enable */
#define	PSTATE_PRIV	0x004		/* privileged mode */
#define	PSTATE_AM	0x008		/* use 32b address mask */
#define	PSTATE_PEF	0x010		/* fp enable */
#define	PSTATE_RED	0x020		/* red mode */
#define	PSTATE_MM	0x0C0		/* memory model */
#define	PSTATE_TLE	0x100		/* trap little endian */
#define	PSTATE_CLE	0x200		/* current little endian */
#define	PSTATE_MG	0x400		/* MMU globals */
#define	PSTATE_IG	0x800		/* interrupt globals */

#endif	/* !_ALLREGS_H */
