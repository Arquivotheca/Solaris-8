/*
 * Copyright (c) 1995-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ADB_SPARC_H
#define	_ADB_SPARC_H

#pragma ident	"@(#)sparc.h	1.23	99/05/04 SMI"

#include <sys/vmparam.h>
#include <sys/types.h>
#if	KADB
#include <sys/trap.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * for sparc, adb supports different disassembly modes
 */
#define	V8_MODE		1	/* SPARC V8 */
#define	V9_MODE		2	/* SPARC V9 */
#define	V9_VIS_MODE	4	/* Sun UltraSPARC 1 */

/*
 * adb has used "addr_t" as == "unsigned" in a typedef, forever.
 * Now in 4.0 there is suddenly a new "addr_t" typedef'd as "char *".
 *
 * About a million changes would have to be made in adb if this #define
 * weren't able to unto that damage.
 */
#define	addr_t unsigned

typedef enum  {
	r_normal,
	r_gzero,
	r_window,
	r_floating,
	r_invalid
} reg_type;

/*
 * setreg/readreg/writereg use the adb_raddr structure to communicate
 * with one another, and with the floating point conversion routines.
 * "normal" includes the window registers that are in the current window.
 */
struct adb_raddr {
	reg_type ra_type;
	long	*ra_raddr;
	int	ra_mode;
};

#define	RA_64BIT	1	/* ra_raddr points to a 64-bit item */

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "allregs" structure.  This is used
 * in different ways for kadb, adb -k, and normal adb.  The struct
 * is defined in allregs.h, and the variable (adb_regs) is decleared
 * in accesssr.c.
 */

/*
 * adb's internal register codes for the sparc
 */

/* Integer Unit (IU)'s "r registers" */
#define	REG_RN(n)	((n) - FIRST_STK_REG)
#define	Reg_G0		 0
#define	Reg_G1		 1
#define	Reg_G2		 2
#define	Reg_G3		 3
#define	Reg_G4		 4
#define	Reg_G5		 5
#define	Reg_G6		 6
#define	Reg_G7		 7

#define	Reg_O0		 8
#define	Reg_O1		 9
#define	Reg_O2		10
#define	Reg_O3		11
#define	Reg_O4		12
#define	Reg_O5		13
#define	Reg_O6		14
#define	Reg_O7		15

#define	Reg_L0		16
#define	Reg_L1		17
#define	Reg_L2		18
#define	Reg_L3		19
#define	Reg_L4		20
#define	Reg_L5		21
#define	Reg_L6		22
#define	Reg_L7		23

#define	Reg_I0		24
#define	Reg_I1		25
#define	Reg_I2		26
#define	Reg_I3		27
#define	Reg_I4		28
#define	Reg_I5		29
#define	Reg_I6		30
#define	Reg_I7		31	/* (Return address minus eight) */

#define	Reg_SP		14	/* Stack pointer == Reg_O6 */
#define	Reg_FP		30	/* Frame pointer == Reg_I6 */

/* Miscellaneous registers */
#define	Reg_Y		32
#define	Reg_PSR		33
#define	Reg_WIM		34
#define	Reg_TBR		35
#define	Reg_PC		36
#define	Reg_NPC		37
#define	LAST_NREG	37	/* last normal (non-Floating) register */

#define	Reg_FSR		38	/* FPU status */
#define	Reg_FQ		39	/* FPU queue */

/* Floating Point Unit (FPU)'s "f registers" */
#define	Reg_F0		40
#define	Reg_F16		56
#define	Reg_F31		71
#define	Reg_F32		72
#define	Reg_F48		88
#define	Reg_F63		103

/*
 * NREGISTERS does not include registers from non-current windows.
 * See below.
 */
#define	NREGISTERS	(Reg_F63+1)
#define	FIRST_STK_REG	Reg_O0
#define	LAST_STK_REG	Reg_I5

/*
 * Here, we define	codes for the window (I/L/O) registers
 * of the other windows.  Note:  all such codes are greater
 * than NREGISTERS, including the codes that duplicate the
 * registers within the current window.
 *
 * Usage:  REG_WINDOW(window_number, register code)
 *	where window_number is always relative to the current window;
 *		window_number == 0 is the current window.
 *	register_code is between Reg_O0 and Reg_I7.
 */
#define	REG_WINDOW(win, reg) \
	(NREGISTERS - Reg_O0 + (win)*(16) + (reg))
#define	WINDOW_OF_REG(reg) \
	(((reg) - NREGISTERS + Reg_O0)/16)
#define	WREG_OF_REG(reg) \
	(((reg) - NREGISTERS + Reg_O0)%16)

#define	MIN_WREGISTER REG_WINDOW(0, Reg_O0)
#define	MAX_WREGISTER REG_WINDOW(NWINDOW-1, Reg_I7)

#define	REGADDR(r)	(4 * (r))


#ifndef	REGNAMESINIT
extern
#endif
char	*regnames[]
#ifdef REGNAMESINIT
	= {
	/* IU general regs */
	"g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
	"o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
	"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
	"i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

	/* Miscellaneous */
	"y", "psr", "wim", "tbr", "pc", "npc", "fsr", "fq",

	/* FPU regs */
	"f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
	"f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
	"f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
	"f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
	"f32", "f33", "f34", "f35", "f36", "f37", "f38", "f39",
	"f40", "f41", "f42", "f43", "f44", "f45", "f46", "f47",
	"f48", "f49", "f50", "f51", "f52", "f53", "f54", "f55",
	"f56", "f57", "f58", "f59", "f60", "f61", "f62", "f63",
	}
#endif
;

/*
 * Alternate names for the registers. This allows '<i6=X' to work, for
 * example.
 */
#ifndef	REGNAMESINIT
extern
#endif
struct altregnames {
	char *name;
	int reg;
}
#ifdef REGNAMESINIT
#define	NALTREGISTERS	3	/* number of aliases we define */
altregname[] = {
	"tstate", Reg_PSR,
	"i6", Reg_I6,
	"o6", Reg_O6,
}
#endif
;

#define	TXTRNDSIZ	SEGSIZ

#define	MAXINT		0x7fffffff
#define	MAXFILE		0xffffffff

/*
 * All 32 bits are valid on sparc:  VADDR_MASK is a no-op.
 * It's only here for the sake of some shared code in kadb.
 */
#define	VADDR_MASK	0xffffffff

/*
 * A "stackpos" contains everything we need to know in
 * order to do a stack trace.
 */
struct stackpos {
	uint_t	k_pc;		/* where we are in this proc */
	uint_t	k_fp;		/* this proc's frame pointer */
	uint_t	k_nargs;	/* This we can't figure out on sparc */
	uint_t	k_entry;	/* this proc's entry point */
	uint_t	k_caller;	/* PC of the call that called us */
	uint_t	k_flags;	/* sigtramp & leaf info */
	uint_t	k_regloc[ LAST_STK_REG - FIRST_STK_REG +1 ];
};
	/* Flags for k_flags:  */
#define	K_LEAF		1	/* this is a leaf procedure */
#define	K_CALLTRAMP	2	/* caller is _sigtramp */
#define	K_SIGTRAMP	4	/* this is _sigtramp */
#define	K_TRAMPED	8	/* this was interrupted by _sigtramp */

/*
 * sparc stack frame is just a struct window (saved local & in regs),
 * followed by some optional stuff:
 *	"6 words for callee to dump register arguments",
 *	"Outgoing parameters past the sixth",
 *  and "Local stack space".
 *
 * Since it is only used via "get", we don't really use it except
 * as documentation.
 *
 * struct sr_frame {
 *	struct window	fr_w;	// 8 each w_local, w_in //
 *	int		fr_regargs[ 6 ];
 *	int		fr_extraparms[ 1 ];
 * };
 */

/*
 * Number of registers usable as procedure arguments
 */
#define	NARG_REGS 6

/*
 * Offsets in stack frame of interesting locations:
 */
#define	FR_L0		(0*4)	/* (sr_frame).fr_w.w_local[ 0 ] */
#define	FR_LREG(reg)	(FR_L0 + (((reg)-Reg_L0) * 4))

#define	FR_I0		(8*4)	/* (sr_frame).fr_w.w_in[ 0 ] */
#define	FR_IREG(reg)	(FR_I0 + (((reg)-Reg_I0) * 4))

#define	FR_SAVFP	(14*4)	/* (sr_frame).fr_w.w_in[ 6 ] */
#define	FR_SAVPC	(15*4)	/* (sr_frame).fr_w.w_in[ 7 ] */

#define	FR_ARG0		(17*4)
#define	FR_ARG1		(18*4)
#define	FR_ARG2		(19*4)
#define	FR_XTRARGS	(23*4)  /* >6 Outgoing parameters */

#ifndef KADB
/*
 * On a sparc, to get any but the G and status registers, you need to
 * have a copy of the pcb structure (from the u structure -- u.u_pcb).
 * See /usr/include/sys/user.h and /usr/include/sys/{pcb.h, reg.h}.
 */
struct pcb u_pcb;
#endif !KADB


/* ******************************************************************* */
/*
 *	Breakpoint instructions
 */

/*
 * A breakpoint instruction lives in the extern "bpt".
 * Let's be explicit about it this time.  A sparc breakpoint
 * is a trap-always, trap number 1.
 *	"ta 1"
 */
#define	SZBPT 4
#define	PCFUDGE 0
#ifdef BPT_INIT
#define	KADB_BP (0x91D02000 + ST_KADB_BREAKPOINT)
	uint_t bpt =
#ifdef KADB
	    KADB_BP; /* asm("bpt:	ta   126   "); */
#else
	    0x91D02001; /* asm("bpt:	ta   1    "); */
#endif
#else	/* !BPT_INIT */
extern uint_t bpt;
#endif


/* ******************************************************************* */
/*
 *	These #defines are for those few places outside the
 *	disassembler that need to decode a few instructions.
 */

/* Values for the main "op" field, bits <31..30> */
#define	SR_FMT2_OP		0
#define	SR_CALL_OP		1
#define	SR_FMT3a_OP		2

/* Values for the tertiary "op3" field, bits <24..19> */
#define	SR_JUMP_OP		0x38
#define	SR_TICC_OP		0x3A

/* A value to compare with the cond field, bits <28..25> */
#define	SR_ALWAYS		8


/* ******************************************************************* */
/*
 *	These defines reduce the number of #ifdefs.
 */
#define	t_srcinstr(item)  (item)	/* always 32 bits on sparc */
#define	ins_type ulong_t
#define	first_byte(int32) ((int32) >> 24)
#define	INSTR_ALIGN_MASK 3
#define	INSTR_ALIGN_ERROR "address must be aligned on a 4-byte boundary"
#define	BPT_ALIGN_ERROR "breakpoint must be aligned on a 4-byte boundary"

#ifdef	__cplusplus
}
#endif

#endif	/* _ADB_SPARC_H */
