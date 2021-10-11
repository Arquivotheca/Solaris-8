/*
 * Copyright (c) 1987 - 1994 by Sun Microsystems, Inc.
 */

#ident "@(#)i386.h	1.9	97/10/24 SMI"

#ifndef _i386_H_
#define _i386_H_

#include <sys/vmparam.h>
#include <sys/mmu.h>
#include <sys/types.h>
#include <sys/trap.h>

/*
 * adb has used "addr_t" as == "unsigned" in a typedef, forever.
 * Now in 4.0 there is suddenly a new "addr_t" typedef'd as "char *".
 *
 * About a million changes would have to be made in adb if this #define
 * weren't able to unto that damage.
 */
#define addr_t unsigned

/*
 * setreg/readreg/writereg use the adb_raddr structure to communicate
 * with one another, and with the floating point conversion routines.
 */
struct adb_raddr {
    enum reg_type { r_long, r_short, r_hishort, r_byte, r_invalid}
	     ra_type;
    int	    *ra_raddr;
};

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "regs" structure.  This is used
 * in different ways for kadb, adb -k, and normal adb.  The struct
 * is defined in regs.h, and the variable (adb_regs) is decleared
 * in accessir.c.
 */

/* Integer Unit (IU)'s "r registers" */
/* this chunk of define's was taken from the 386i code */
#define REG_RN(n)	(n)
#define	REG_FP		EBP	/* Frame pointer */
#define	REG_SP		UESP	/* Stack pointer */
#define	REG_PC		EIP

#define	Reg_FP		REG_FP	/* Frame pointer */
#define	Reg_SP		REG_SP	/* Stack pointer */
#define	Reg_PC		REG_PC

#define	LAST_NREG	18	/* last normal (non-Floating) register */

#define	REG_FCW		19
#define	REG_FSW		20
#define	REG_FTAG	21
#define	REG_FIP		22
#define	REG_FCS		23
#define	REG_FOP		24
#define	REG_FDATAOFF	25
#define	REG_FDATASEL	26
#define	REG_ST0		27
#define	REG_ST0A	28
#define	REG_ST0B	29
#define	REG_ST1		30
#define	REG_ST1A	31
#define	REG_ST1B	32
#define	REG_ST2		33
#define	REG_ST2A	34
#define	REG_ST2B	35
#define	REG_ST3		36
#define	REG_ST3A	37
#define	REG_ST3B	38
#define	REG_ST4		39
#define	REG_ST4A	40
#define	REG_ST4B	41
#define	REG_ST5		42
#define	REG_ST5A	43
#define	REG_ST5B	44
#define	REG_ST6		45
#define	REG_ST6A	46
#define	REG_ST6B	47
#define	REG_ST7		48
#define	REG_ST7A	49
#define	REG_ST7B	50
#define	REG_XFSW	51
#define	REG_AX		52
#define	REG_BX		53
#define	REG_CX		54
#define	REG_DX		55
#define	REG_SI		56
#define	REG_DI		57
#define	REG_BP		58
#define	REG_WSP		59
#define	REG_WIP		60
#define	REG_WFL		61
#define	REG_AL		62
#define	REG_AH		63
#define	REG_BL		64
#define	REG_BH		65
#define	REG_CL		66
#define	REG_CH		67
#define	REG_DL		68
#define	REG_DH		69
#define	NREGISTERS	70

#define	REGADDR(r)	(4 * (r))


#ifndef	REGNAMESINIT
extern  char *regnames[NREGISTERS];
#else /* REGNAMESINIT */
char	*regnames[NREGISTERS] = {
	/* IU general regs */
	"gs",		/* GS		 0 */
	"fs",		/* FS		 1 */
	"es",		/* ES		 2 */
	"ds",		/* DS		 3 */
	"edi",		/* EDI		 4 */
	"esi",		/* ESI		 5 */
	"ebp",		/* EBP		 6 */
#ifdef KADB
	"esp",		/* ESP		 7 */
#else
	"kesp",		/* ESP		 7 */
#endif
	"ebx",		/* EBX		 8 */
	"edx",		/* EDX		 9 */
	"ecx",		/* ECX		10 */
	"eax",		/* EAX		11 */
	"trapno",	/* TRAPNO	12 */
	"err",		/* ERR		13 */
	"eip",		/* EIP		14 */
	"cs",		/* CS		15 */
	"efl",		/* EFL		16 */
#ifdef KADB
	"uesp",		/* UESP		17 */
#else
	"esp",		/* UESP		17 */
#endif
	"ss",		/* SS		18 */
	/* FPU regs */
	"fcw",		/* REG_FCW	19 */
	"fsw",		/* REG_FSW	20 */
	"ftag",		/* REG_FTAG	21 */
	"fip",		/* REG_FIP	22 */
	"fcs",		/* REG_FCS	23 */
	"fop",		/* REG_FOP	24 */
	"fdataoff",	/* REG_FDATAOFF	25 */
	"fdatasel",	/* REG_FDATASEL	26 */
	"st0",		/* REG_ST0	27 */
	"st0a",		/* REG_ST0A	28 */
	"st0b",		/* REG_ST0B	29 */
	"st1",		/* REG_ST1	30 */
	"st1a",		/* REG_ST1A	31 */
	"st1b",		/* REG_ST1B	32 */
	"st2",		/* REG_ST2	33 */
	"st2a",		/* REG_ST2A	34 */
	"st2b",		/* REG_ST2B	35 */
	"st3",		/* REG_ST3	36 */
	"st3a",		/* REG_ST3A	37 */
	"st3b",		/* REG_ST3B	38 */
	"st4",		/* REG_ST4	39 */
	"st4a",		/* REG_ST4A	40 */
	"st4b",		/* REG_ST4B	41 */
	"st5",		/* REG_ST5	42 */
	"st5a",		/* REG_ST5A	43 */
	"st5b",		/* REG_ST5B	44 */
	"st6",		/* REG_ST6	45 */
	"st6a",		/* REG_ST6A	46 */
	"st6b",		/* REG_ST6B	47 */
	"st7",		/* REG_ST7	48 */
	"st7a",		/* REG_ST7A	49 */
	"st7b",		/* REG_ST7B	50 */
	"xfsw",		/* REG_XFSW	51 */
	"ax",		/* REG_AX	52 */
	"bx",		/* REG_BX	53 */
	"cx",		/* REG_CX	54 */
	"dx",		/* REG_DX	55 */
	"si",		/* REG_SI	56 */
	"di",		/* REG_DI	57 */
	"bp",		/* REG_BP	58 */
	"sp",		/* REG_WSP	59 */
	"ip",		/* REG_WIP	60 */
	"fl",		/* REG_WFL	61 */
	"al",		/* REG_AL	62 */
	"ah",		/* REG_AH	63 */
	"bl",		/* REG_BL	64 */
	"bh",		/* REG_BH	65 */
	"cl",		/* REG_CL	66 */
	"ch",		/* REG_CH	67 */
	"dl",		/* REG_DL	68 */
	"dh",		/* REG_DH	69 */
   };
#endif /* REGNAMESINIT */

#define	U_PAGE	UADDR

#define	TXTRNDSIZ	SEGSIZ

#define	MAXINT		0x7fffffff
#define	MAXFILE		0xffffffff

/*
 * All 32 bits are valid on our i86 port:  VADDR_MASK is a no-op.
 * It's only here for the sake of some shared code in kadb.
 */
#define VADDR_MASK	0xffffffff

/*
 * This doesn't work, since the kernel is loaded into arbitrary (though
 * contiguous on a per-segment basis) chunks of physical memory as
 * determined by the prom.
 *
 * #define	KVTOPH(x) (((x) >= KERNELBASE)? (x) - KERNELBASE: (x))
 */

/*
 * A "stackpos" contains everything we need to know in
 * order to do a stack trace.
 */
struct stackpos {
	 u_int	k_pc;		/* where we are in this proc */
	 u_int	k_fp;		/* this proc's frame pointer */
	 u_int	k_nargs;	/* # of args passed to the func */
	 u_int	k_entry;	/* this proc's entry point */
	 u_int	k_caller;	/* PC of the call that called us */
	 u_int	k_flags;	/* sigtramp & leaf info */
	 u_int	k_regloc[NREGISTERS];
};
	/* Flags for k_flags:  */
#define K_LEAF		1	/* this is a leaf procedure */
#define K_CALLTRAMP	2	/* caller is _sigtramp */
#define K_SIGTRAMP	4	/* this is _sigtramp */
#define K_TRAMPED	8	/* this was interrupted by _sigtramp */

/*
 * Useful stack frame offsets.
 */
#define FR_SAVFP	0
#define FR_SAVPC	4

/***********************************************************************/
/*
 *	Breakpoint instructions
 */

/*
 * A breakpoint instruction lives in the extern "bpt".
 * Let's be explicit about it this time.
 */
#define SZBPT 1
#define PCFUDGE (-1)

#ifdef BPT_INIT
#define KADB_BP (0xcc)
	int bpt = KADB_BP;
#else	/* !BPT_INIT */
extern int bpt;
#endif /* !BPT_INIT */

#define ADDIMMBYTE      0x83
#define ADDL            0x81
#define POPL            0x59
#define CALLEIP         0xe8            /* call off.l(%eip) */
#define CALLINDIRECT	0xff            /* call *reg or call *OFFSET(reg) */
#define CALLRMASK	0xc0
#define	CALLDISP8	0x40
#define CALLDISP32	0x80
#define CALLDISP0	0xc0


/***********************************************************************/
/*
 *	These defines reduce the number of #ifdefs.
 */
#define t_srcinstr(item)  (item)
#define ins_type u_long
#define first_byte( int32 ) ( (int32) & 0xff )

#endif		/* _i386_H_ */
