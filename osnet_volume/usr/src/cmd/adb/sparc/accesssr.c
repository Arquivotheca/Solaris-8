/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)accesssr.c	1.51	99/10/05 SMI"

#include "adb.h"
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include "fpascii.h"
#include "ptrace.h"
#include "symtab.h"
#include "allregs.h"

#include "sr_instruction.h"

/*
 * adb's idea of the current value of most of the
 * processor registers lives in "adb_regs". For SPARC-V9 we
 * need a separate structure, since some types and regs differ.
 */

struct allregs adb_regs;
struct allregs_v9 adb_regs_v9;

int v9flag;			/* use SPARC-V9 display mode? */

#ifdef KADB
extern int nwindows;		/* initialized by kadb at startup */

#ifdef	NWINDOW
#undef	NWINDOW			/* do it my way */
#endif	/* NWINDOW */
#define NWINDOW nwindows	/* # of implemented windows */

#   define EIO		 5
#ifdef __sparcv9cpu
#   define CWP  (adb_regs_v9.r_cwp)
#else
#   define CWP  (((adb_regs.r_psr & 15) +1) % NWINDOW)
#endif

#define adb_oreg (adb_regs.r_window[ (-1 == ((CWP -1) % NWINDOW)) ? \
	NWINDOW - 1 : ((CWP -1) % NWINDOW)].rw_in)
#define adb_ireg (adb_regs.r_window[ CWP ].rw_in)
#define adb_lreg (adb_regs.r_window[ CWP ].rw_local)

/* #define adb_oreg_v9 (adb_regs_v9.r_window[((CWP+1) % NWINDOW)].rw_in) */
#define adb_oreg_v9	(adb_regs_v9.r_outs)
#define adb_ireg_v9 (adb_regs_v9.r_window[CWP].rw_in)
#define adb_lreg_v9 (adb_regs_v9.r_window[CWP].rw_local)

#else	/* !KADB */

/*
 * Libkvm is used (by adb only) to dig things out of the kernel
 */
#include <kvm.h>
#include <sys/ucontext.h>

extern kvm_t *kvmd;					/* see main.c */
extern struct asym *trampsym;				/* see setupsr.c */

#define adb_oreg (adb_regs.r_outs)
#define adb_ireg (adb_regs.r_ins)
#define adb_lreg (adb_regs.r_locals)

#define adb_oreg_v9 (adb_regs_v9.r_outs)
#define adb_ireg_v9 (adb_regs_v9.r_ins)
#define adb_lreg_v9 (adb_regs_v9.r_locals)

#endif !KADB

#ifndef KADB
/*
 * Read a word from kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kread(addr, p)
	unsigned addr;
	int *p;
{
	if (kvm_read(kvmd, (long)addr, (char *)p, sizeof *p) != sizeof *p)
		return (-1);
	return (0);
}

/*
 * Write a word to kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kwrite(addr, p)
	unsigned addr;
	int *p;
{
	if (kvm_write(kvmd, (long)addr, (char *)p, sizeof *p) != sizeof *p)
		return (-1);
	return (0);
}
#endif !KADB

extern	struct stackpos exppos;

void
tbia()
{

	exppos.k_fp = 0;
}



/*
 * Construct an informative error message
 */
static void
regerr (reg, wind)
{
	static char rw_invalid[ 60 ];
	char *wp;

	wp = wind ? "window-" : "" ;
	if (reg < 0  ||  reg > NREGISTERS) {
		sprintf(rw_invalid, "Invalid %sregister %d", wp, reg);
	} else {
		sprintf( rw_invalid, "Invalid %sregister %s (%d)", wp,
		    regnames[reg], reg);
	}
	errflg = rw_invalid;
}


/*
 * reg_address is given an adb register code;
 * it fills in the (global)adb_raddr structure.
 * "Fills in" means that it figures out the register type
 * and the address of where adb keeps its idea of that register's
 * value (i.e., in adb's own (global)adb_regs structure).
 *
 * reg_address is called by setreg and readreg;
 * it returns nothing.
 */
void
reg_address(reg)
int reg;
{
	register struct allregs *arp = &adb_regs;
	register struct allregs_v9 *arp_v9 = &adb_regs_v9;
	register struct adb_raddr *ra = &adb_raddr;

	ra->ra_mode = 0;
	ra->ra_type = r_normal;

	switch (reg) {
	case Reg_PSR:
		if (v9flag) {
			ra->ra_raddr = (long *)&arp_v9->r_tstate;
			ra->ra_mode = RA_64BIT;
		} else {
			ra->ra_raddr = &arp->r_psr;
		}
		break;
	case Reg_PC:
		ra->ra_raddr = (v9flag ? &arp_v9->r_pc : &arp->r_pc);
		break;
	case Reg_NPC:
		ra->ra_raddr = (v9flag ? &arp_v9->r_npc : &arp->r_npc);
		break;
	case Reg_TBR:
		ra->ra_raddr = (v9flag ? &arp_v9->r_tba : &arp->r_tbr);
		break;
	case Reg_WIM:
		ra->ra_raddr = &arp->r_wim;
		break;
	case Reg_Y:
		ra->ra_raddr = (v9flag ? &arp_v9->r_y : &arp->r_y);
		break;

	/* Globals */
	case Reg_G0:
		ra->ra_raddr = 0;
		ra->ra_type = r_gzero;
		break;
	 case Reg_G1:
	 case Reg_G2:
	 case Reg_G3:
	 case Reg_G4:
	 case Reg_G5:
	 case Reg_G6:
	 case Reg_G7:
		if (v9flag) {
			ra->ra_raddr = (long *)&arp_v9->r_globals[reg - Reg_G1];
			ra->ra_mode = RA_64BIT;
		} else {
			ra->ra_raddr = &arp->r_globals[ reg - Reg_G1 ];
		}
		break;
	 /* Other registers (O, L, I) in the current window */
	 case Reg_O0:
	 case Reg_O1:
	 case Reg_O2:
	 case Reg_O3:
	 case Reg_O4:
	 case Reg_O5:
	 case Reg_SP:   /* Reg_O6 is == Reg_SP */
	 case Reg_O7:
		if (v9flag) {
			ra->ra_raddr = (long *)&(adb_oreg_v9[reg - Reg_O0] );
			ra->ra_mode = RA_64BIT;
		} else {
			ra->ra_raddr = &(adb_oreg[ reg - Reg_O0]);
		}
		break;
	 case Reg_L0:
	 case Reg_L1:
	 case Reg_L2:
	 case Reg_L3:
	 case Reg_L4:
	 case Reg_L5:
	 case Reg_L6:
	 case Reg_L7:
		if (v9flag) {
			ra->ra_raddr = &(adb_lreg_v9[reg - Reg_L0]);
		} else {
			ra->ra_raddr = &(adb_lreg[reg - Reg_L0]);
		}
		break;

	 case Reg_I0:
	 case Reg_I1:
	 case Reg_I2:
	 case Reg_I3:
	 case Reg_I4:
	 case Reg_I5:
	 case Reg_I6:
	 case Reg_I7:
		if (v9flag) {
			ra->ra_raddr = &( adb_ireg_v9[ reg - Reg_I0 ] );
		} else {
			ra->ra_raddr = &( adb_ireg[ reg - Reg_I0 ] );
		}
		break;

	 case Reg_FQ:	/* Can't get the FQ */
		regerr( reg, 0 );
		ra->ra_raddr = 0;
		ra->ra_type = r_invalid;
		break;
	 case Reg_FSR:
		ra->ra_raddr = (long *)&Prfpregs.pr_fsr;
		ra->ra_type = r_normal;
		break;
	 default:
		if (reg >= Reg_F0  &&  reg <= Reg_F31) {
			ra->ra_type = r_floating;
			ra->ra_raddr = (long *)&Prfpregs.pr_fr.pr_regs[reg - Reg_F0];
		} else if (reg >= Reg_F32  &&  reg <= Reg_F63) {
			ra->ra_type = r_floating;
			ra->ra_raddr = (long *)&xregs.pr_un.pr_v8p.pr_xfr.pr_regs[reg - Reg_F32];
		}
#ifdef KADB
		else if ( reg <= MAX_WREGISTER ) {
			int w, r;
			/*
			 * Figure out where adb likes to keep the
			 * registers from other register-windows.
			 */
			w = WINDOW_OF_REG( reg );
			r = WREG_OF_REG( reg );

			ra->ra_raddr = (v9flag ? 
			    &(arp_v9->r_window[w].rw_local[r]) :
			    &(arp->r_window[w].rw_local[r]));
			ra->ra_type = r_window;
		}
#endif KADB
		 else {
			regerr( reg, 0 );
			ra->ra_raddr = 0;
			ra->ra_type = r_invalid;
		}
		break;

        }
#ifdef DEBUG
	{ char *rtp;
		db_printf(5, "reg_address of reg %d <%s> is %X; ", reg,
		    (reg < NREGISTERS ? regnames[reg] : "a window reg"),
		    ra->ra_raddr );

		switch (ra->ra_type) {
		case r_normal:     rtp= "r_normal";   break;
		case r_gzero:      rtp= "r_gzero";    break;
		case r_window:     rtp= "r_window";   break;
		case r_floating:   rtp= "r_floating"; break;
		case r_invalid:    rtp= "r_invalid";  break;
		default:           rtp= "default?";   break;
		}

		db_printf(1, "type %d (%s)", ra->ra_type, rtp);
		if (ra->ra_raddr) {
			db_printf(1, "contents %X", ra->ra_mode == RA_64BIT ?
				  *((u_longlong_t *)(ra->ra_raddr)) :
				  *(ra->ra_raddr));
		}
	}
#endif DEBUG
}


/*
 * reg_windows -- called from setreg and readreg, this routine
 * stores or retrieves the value of a register from one of the
 * non-current register windows.  "reg" should be between
 * MIN_WREGISTER and MAX_WREGISTER.
 *
 * We have already called reg_address, so adb_raddr tells us
 * where in adb_regs to get or put the register value.
 */
/*
 * w = WINDOW_OF_REG( reg );
 * r = WREG_OF_REG( reg );
 */

#if	defined(KADB)
static int
reg_windows( reg, val, wrt_flag ) {
	db_printf(4, "reg_windows: reg=%D val=%D wrt_flag=%D",
	    reg, val, wrt_flag);

	if (reg < MIN_WREGISTER || reg > MAX_WREGISTER) {
		regerr(reg, 1);
		return 0;
	}
	/* In any case, do adb_raddr.ra_raddr right. */
	if (wrt_flag) {
		*(adb_raddr.ra_raddr) = val;
	} else {
		val = *(adb_raddr.ra_raddr);
	}
	return val;

}
#endif	/* defined(KADB) */

void
setreg(reg, val)
int reg;
int val;
{
	extern addr_t usernpc;

	reg_address( reg );

	switch (adb_raddr.ra_type) {

	case r_gzero:  /* Always zero -- setreg does nothing */
		break;
	case r_floating: /* Treat floating regs like normals */
	case r_normal: /* Normal one -- we have a good address */
		db_printf(1, "setreg(%d): setting *(%X) to %d\n", reg, adb_raddr.ra_raddr, val);
		if (adb_raddr.ra_mode == RA_64BIT) {
			*((u_longlong_t *)(adb_raddr.ra_raddr)) = val;
			adb_raddr.ra_mode = 0;
		} else {
			*(adb_raddr.ra_raddr) = val;
		}
		if (reg == Reg_PC)
			userpc = val;
		else if (reg == Reg_NPC)
			usernpc = val;
		break;
#ifdef KADB
	case r_window: /* Go figure it out */
		(void) reg_windows( reg, val, 1 );
		break;
#endif KADB
	}
}


/*
 * readreg -- retrieve value of register reg from adb_regs.
 */
readreg(reg)
int reg;
{
	u_longlong_t val = 0;
	register struct adb_raddr *ra = &adb_raddr;

	reg_address(reg);

	db_printf(1, "readreg:  Reg_Address of reg %d is %X", reg, ra->ra_raddr);

	switch (ra->ra_type) {

	case r_gzero:  /* Always zero -- val is already zero */
		break;
	case r_floating: /* Treat floating regs like normals */
	case r_normal: /* Normal one -- we have a good address */
		if (ra->ra_mode == RA_64BIT) {
			val = *((u_longlong_t *)(ra->ra_raddr));
			ra->ra_mode = 0;
		} else {
			val = (u_longlong_t)((int)*(ra->ra_raddr));
			val &= 0xffffffff;
		}
		break;
#ifdef KADB
	case r_window: /* Go figure it out */
		val = reg_windows(reg, (int)val, 0);
		break;
#endif KADB
	default:
		db_printf(1, "readreg: unknown reg type %x", ra->ra_type);
		break;
	}
	return ((int)val);
}

#ifdef KADB
/*
 * For ptrace(SETREGS or GETREGS) to work, the registers must be in
 * the form that they take in the core file (instead of the form used
 * by the access routines in this file, i.e., the full machine state).
 * These routines copy the relevant registers.
 */
void
regs_to_core()
{
#ifdef	__sparcv9cpu
	int win;
	extern v9_fpregset_t fpuregs;
	struct allregs_v9 *a = &adb_regs_v9;

	db_printf(1, "Copy regs to core from adb_regs");
	/* XXX - PSR, globals, and outs are 64 bits in V9 */
	Prstatus.pr_lwp.pr_reg[R_PSR] = (int)a->r_tstate;
	Prstatus.pr_lwp.pr_reg[R_PC] = a->r_pc;
	Prstatus.pr_lwp.pr_reg[R_nPC] = a->r_npc;
	Prstatus.pr_lwp.pr_reg[R_Y] = a->r_y;
	Prstatus.pr_lwp.pr_reg[R_G1] = (int)a->r_globals[0];
	Prstatus.pr_lwp.pr_reg[R_G2] = (int)a->r_globals[1];
	Prstatus.pr_lwp.pr_reg[R_G3] = (int)a->r_globals[2];
	Prstatus.pr_lwp.pr_reg[R_G4] = (int)a->r_globals[3];
	Prstatus.pr_lwp.pr_reg[R_G5] = (int)a->r_globals[4];
	Prstatus.pr_lwp.pr_reg[R_G6] = (int)a->r_globals[5];
	Prstatus.pr_lwp.pr_reg[R_G7] = (int)a->r_globals[6];

	/* ins of next win == outs of this one */
	win = ((a->r_cwp + 1) % NWINDOW);

	Prstatus.pr_lwp.pr_reg[R_O0] = a->r_window[win].rw_in[0];
	Prstatus.pr_lwp.pr_reg[R_O1] = a->r_window[win].rw_in[1];
	Prstatus.pr_lwp.pr_reg[R_O2] = a->r_window[win].rw_in[2];
	Prstatus.pr_lwp.pr_reg[R_O3] = a->r_window[win].rw_in[3];
	Prstatus.pr_lwp.pr_reg[R_O4] = a->r_window[win].rw_in[4];
	Prstatus.pr_lwp.pr_reg[R_O5] = a->r_window[win].rw_in[5];
	Prstatus.pr_lwp.pr_reg[R_O6] = a->r_window[win].rw_in[6];
	Prstatus.pr_lwp.pr_reg[R_O7] = a->r_window[win].rw_in[7];

	/*
	 * Copy the extra V9 floating point registers, since
	 * they are used by kernel bcopy(), at least in sun4u.
	 */
	memcpy((caddr_t)&xregs.pr_un.pr_v8p.pr_xfr,
	    (caddr_t)&fpuregs.fpu_fr, sizeof (fpuregs.fpu_fr));
#else
	struct allregs *a = &adb_regs;

	db_printf(1, "Copy regs to core from adb_regs");
	Prstatus.pr_lwp.pr_reg[R_PSR] = a->r_psr;
	Prstatus.pr_lwp.pr_reg[R_PC] = a->r_pc;
	Prstatus.pr_lwp.pr_reg[R_nPC] = a->r_npc;
	Prstatus.pr_lwp.pr_reg[R_Y] = a->r_y;
	Prstatus.pr_lwp.pr_reg[R_G1] = a->r_globals[0];
	Prstatus.pr_lwp.pr_reg[R_G2] = a->r_globals[1];
	Prstatus.pr_lwp.pr_reg[R_G3] = a->r_globals[2];
	Prstatus.pr_lwp.pr_reg[R_G4] = a->r_globals[3];
	Prstatus.pr_lwp.pr_reg[R_G5] = a->r_globals[4];
	Prstatus.pr_lwp.pr_reg[R_G6] = a->r_globals[5];
	Prstatus.pr_lwp.pr_reg[R_G7] = a->r_globals[6];
	Prstatus.pr_lwp.pr_reg[R_O0] = a->r_window[0].rw_in[0];
	Prstatus.pr_lwp.pr_reg[R_O1] = a->r_window[0].rw_in[1];
	Prstatus.pr_lwp.pr_reg[R_O2] = a->r_window[0].rw_in[2];
	Prstatus.pr_lwp.pr_reg[R_O3] = a->r_window[0].rw_in[3];
	Prstatus.pr_lwp.pr_reg[R_O4] = a->r_window[0].rw_in[4];
	Prstatus.pr_lwp.pr_reg[R_O5] = a->r_window[0].rw_in[5];
	Prstatus.pr_lwp.pr_reg[R_O6] = a->r_window[0].rw_in[6];
	Prstatus.pr_lwp.pr_reg[R_O7] = a->r_window[0].rw_in[7];
	db_printf(1, "Done copying regs to core from adb_regs");
#endif	/* __sparcv9cpu */
}

#else !KADB
void
core_to_regs ()
{
	register int reg;

	db_printf(1, "Copy regs from Prstatus.pr_lwp.pr_reg to adb_regs");
	if (v9flag) {
		register struct allregs_v9 *a = &adb_regs_v9;
		register prxregset_t *xp = (prxregset_t *)&xregs;

		a->r_tstate = xp->pr_un.pr_v8p.pr_tstate;
		a->r_pc  = Prstatus.pr_lwp.pr_reg[R_PC];
		a->r_npc = Prstatus.pr_lwp.pr_reg[R_nPC];
		a->r_y   = Prstatus.pr_lwp.pr_reg[R_Y];
		for (reg = 0; reg <= 7; reg++) {
			a->r_outs[reg] =
			    MAKE_LL(xp->pr_un.pr_v8p.pr_xo[reg],
			    Prstatus.pr_lwp.pr_reg[R_O0 + reg]);
			a->r_locals[reg] = Prstatus.pr_lwp.pr_reg[R_L0 + reg];
			a->r_ins[reg] = Prstatus.pr_lwp.pr_reg[R_I0 + reg];
			if (reg < 7)
				a->r_globals[reg] =
				    MAKE_LL(xp->pr_un.pr_v8p.pr_xg[1 + reg],
				    Prstatus.pr_lwp.pr_reg[R_G1 + reg]);
		}
	} else {
		register struct allregs *a = &adb_regs;

		a->r_psr = Prstatus.pr_lwp.pr_reg[R_PSR];
		a->r_pc  = Prstatus.pr_lwp.pr_reg[R_PC];
		a->r_npc = Prstatus.pr_lwp.pr_reg[R_nPC];
		a->r_y   = Prstatus.pr_lwp.pr_reg[R_Y];

		for (reg = 0; reg <= 7; reg++) {
			a->r_outs[reg] = Prstatus.pr_lwp.pr_reg[R_O0 + reg];
	                a->r_locals[reg] = Prstatus.pr_lwp.pr_reg[R_L0 + reg];
	                a->r_ins[reg] =	Prstatus.pr_lwp.pr_reg[R_I0 + reg];
			if (reg < 7)
				a->r_globals[reg] =
					Prstatus.pr_lwp.pr_reg[R_G1 + reg];
		}
	}
        db_printf(1, "Done copying regs from Prstatus.pr_lwp.pr_reg to adb_regs");
} 
 
/*       
 * For ptrace(SETREGS or GETREGS) to work, the registers must be in
 * the form that they take in the core file (instead of the form used
 * by the access routines in this file, i.e., the full machine state).
 * These routines copy the relevant registers.
 */      
void
regs_to_core ()
{        
        register int reg;
         
        db_printf(1, "Copy regs to Prstatus.pr_lwp.pr_reg from adb_regs\n" );
	if (v9flag) {
	        register struct allregs_v9 *a = &adb_regs_v9;

	        Prstatus.pr_lwp.pr_reg[R_PSR] = (int)a->r_tstate;
	        Prstatus.pr_lwp.pr_reg[R_PC] = a->r_pc;
	        Prstatus.pr_lwp.pr_reg[R_nPC] = a->r_npc;
	        Prstatus.pr_lwp.pr_reg[R_Y] = a->r_y;
	        Prstatus.pr_lwp.pr_reg[R_G1] = a->r_globals[0];
	        Prstatus.pr_lwp.pr_reg[R_G2] = a->r_globals[1];
	        Prstatus.pr_lwp.pr_reg[R_G3] = a->r_globals[2];
	        Prstatus.pr_lwp.pr_reg[R_G4] = a->r_globals[3];
	        Prstatus.pr_lwp.pr_reg[R_G5] = a->r_globals[4];
	        Prstatus.pr_lwp.pr_reg[R_G6] = a->r_globals[5];
	        Prstatus.pr_lwp.pr_reg[R_G7] = a->r_globals[6];
	        Prstatus.pr_lwp.pr_reg[R_O0] = a->r_outs[0];
	        Prstatus.pr_lwp.pr_reg[R_O1] = a->r_outs[1];
	        Prstatus.pr_lwp.pr_reg[R_O2] = a->r_outs[2];
	        Prstatus.pr_lwp.pr_reg[R_O3] = a->r_outs[3];
	        Prstatus.pr_lwp.pr_reg[R_O4] = a->r_outs[4];
	        Prstatus.pr_lwp.pr_reg[R_O5] = a->r_outs[5];
	        Prstatus.pr_lwp.pr_reg[R_O6] = a->r_outs[6];
	        Prstatus.pr_lwp.pr_reg[R_O7] = a->r_outs[7];
	        for (reg = Reg_L0; reg <= Reg_L7; reg++){
	                put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_LREG(reg), SSP,
				a->r_locals[ reg - Reg_L0]);
	        }
	        for (reg = Reg_I0; reg <= Reg_I7; reg++){
	                put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_IREG(reg), SSP,
				a->r_ins[ reg - Reg_I0]);
	        }
	} else {	
	        register struct allregs *a = &adb_regs;

	        Prstatus.pr_lwp.pr_reg[R_PSR] = a->r_psr;
	        Prstatus.pr_lwp.pr_reg[R_PC] = a->r_pc;
	        Prstatus.pr_lwp.pr_reg[R_nPC] = a->r_npc;
	        Prstatus.pr_lwp.pr_reg[R_Y] = a->r_y;
	        Prstatus.pr_lwp.pr_reg[R_G1] = a->r_globals[0];
	        Prstatus.pr_lwp.pr_reg[R_G2] = a->r_globals[1];
	        Prstatus.pr_lwp.pr_reg[R_G3] = a->r_globals[2];
	        Prstatus.pr_lwp.pr_reg[R_G4] = a->r_globals[3];
	        Prstatus.pr_lwp.pr_reg[R_G5] = a->r_globals[4];
	        Prstatus.pr_lwp.pr_reg[R_G6] = a->r_globals[5];
	        Prstatus.pr_lwp.pr_reg[R_G7] = a->r_globals[6];
	        Prstatus.pr_lwp.pr_reg[R_O0] = a->r_outs[0];
	        Prstatus.pr_lwp.pr_reg[R_O1] = a->r_outs[1];
	        Prstatus.pr_lwp.pr_reg[R_O2] = a->r_outs[2];
	        Prstatus.pr_lwp.pr_reg[R_O3] = a->r_outs[3];
	        Prstatus.pr_lwp.pr_reg[R_O4] = a->r_outs[4];
	        Prstatus.pr_lwp.pr_reg[R_O5] = a->r_outs[5];
	        Prstatus.pr_lwp.pr_reg[R_O6] = a->r_outs[6];
	        Prstatus.pr_lwp.pr_reg[R_O7] = a->r_outs[7];
	        for (reg = Reg_L0; reg <= Reg_L7; reg++){
	                put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_LREG(reg), SSP,
				a->r_locals[ reg - Reg_L0]);
	        }
	        for (reg = Reg_I0; reg <= Reg_I7; reg++){
	                put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_IREG(reg), SSP,
				a->r_ins[ reg - Reg_I0]);
	        }
	}

        db_printf(1, "Done copying regs to core from adb_regs\n" );
} 

/*
 * Transfer V8 regs to V9 structure or vice-versa; "v9flag" indicates
 * which direction we're going.
 */
void
xfer_regs()
{
	register int reg;
	struct allregs *a = &adb_regs;
	struct allregs_v9 *v = &adb_regs_v9;

	if (v9flag) {
		v->r_tstate = (u_longlong_t)a->r_psr;
		v->r_pc = a->r_pc;
		v->r_npc = a->r_npc;
		v->r_y = a->r_y;
		for (reg = 0; reg <= 7; reg++) {
			v->r_outs[reg] = a->r_outs[reg];
			v->r_locals[reg] = a->r_locals[reg];
			v->r_ins[reg] = a->r_ins[reg];
			if (reg < 7)
				v->r_globals[reg] = a->r_globals[reg];
		}
	} else {
		a->r_psr = (int)v->r_tstate;
		a->r_pc = v->r_pc;
		a->r_npc = v->r_npc;
		a->r_y = v->r_y;
		for (reg = 0; reg <= 7; reg++) {
			a->r_outs[reg] = v->r_outs[reg];
			a->r_locals[reg] = v->r_locals[reg];
			a->r_ins[reg] =	v->r_ins[reg];
			if (reg < 7)
				a->r_globals[reg] = v->r_globals[reg];
		}
	}
}

#endif !KADB


writereg(i, val)
	register int i;
{
	extern int errno;

	if (i >= NREGISTERS) {
		errno = EIO;
		return (0);
	}
	setreg(i, val);
	db_printf(1, "writereg: setreg(%d, %X)\n", i, val);

#ifdef KADB
	/* kadb:  normal regs are in adb_regs */
	/*
	 * Bug 1253423:  The following two lines were removed at some
	 * point, causing register writes to have no effect.
	 * Reinstating them fixes the bug.  The second line
	 * (reading back the registers just written) appears
	 * to be unnecessary, but no-one understands this code
	 * enough to be willing to rip it out.
	 */
	if (v9flag) {
		ptrace(PTRACE_SETREGS, pid, &adb_regs_v9, 0, 0);
		ptrace(PTRACE_GETREGS, pid, &adb_regs_v9, 0, 0);
	} else {
		ptrace(PTRACE_SETREGS, pid, &adb_regs, 0, 0);
		ptrace(PTRACE_GETREGS, pid, &adb_regs, 0, 0);
	}
#else	/* KADB */
	/* normal adb:  regs in adb_regs must be copied */
	regs_to_core();
	db_printf(1, "writereg:  e.g., PC is %X\n",
		Prstatus.pr_lwp.pr_reg[R_PC] );
	ptrace(PTRACE_SETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0, 0);
	db_printf(1, "writereg after rw_pt:  e.g., PC is %X\n",
		Prstatus.pr_lwp.pr_reg[R_PC] );
	ptrace(PTRACE_SETFPREGS, pid, &Prfpregs, 0, 0);
#endif	/* !KADB */

#ifdef DEBUG
	{ int x = readreg(i);
	    if (x != val) {
		printf(" writereg %d miscompare: wanted %X got %X ",
			    i, val, x) ;
	    }
	}
#endif
	return (sizeof (int));
}

/*
 * stacktop collects information about the topmost (most recently
 * called) stack frame into its (struct stackpos) argument.  It's
 * easy in most cases to figure out this info, because the kernel
 * is nice enough to have saved the previous register windows into
 * the proper places on the stack, where possible.  But, if we're
 * a leaf routine (one that avoids a "save" instruction, and uses
 * its caller's frame), we must use the r_i* registers in the
 * (struct regs).  *All* system calls are leaf routines of this sort.
 *
 * On a sparc, it is impossible to determine how many of the
 * parameter-registers are meaningful.
 */

void
stacktop(spos)
	register struct stackpos *spos;
{
	register int i;
	int leaf;
	char *saveflg;

	for (i = FIRST_STK_REG; i <= LAST_STK_REG; i++)
		spos->k_regloc[ REG_RN(i) ] = REGADDR(i);

	spos->k_fp = readreg(Reg_SP);
	spos->k_pc = readreg(Reg_PC);
	spos->k_flags = 0;
	saveflg = errflg;
	/*
	 * If we've stopped in a routine but before it has done its
	 * "save" instruction, is_leaf_proc will tell us a little white
	 * lie about it, calling it a leaf proc.
	 */
	leaf = is_leaf_proc( spos, readreg(Reg_O7));
	if (errflg) {
		/*
		 * oops -- we touched something we ought not to have.
		 * cannot trace caller of "start"
		 */
		spos->k_entry = MAXINT;
		spos->k_nargs = 0;
		errflg = saveflg; /* you didn't see this */
		return;
	}
	errflg = saveflg;

	if( leaf ) {

#ifdef DEBUG
{ extern int adb_debug;

    if( adb_debug  &&  spos->k_fp != Prstatus.pr_lwp.pr_reg[R_SP] ) {
	printf( "stacktop -- fp %X != sp %X.\n", spos->k_fp,
		Prstatus.pr_lwp.pr_reg[R_SP] );
    }
}
#endif DEBUG
		if( (spos->k_flags & K_SIGTRAMP) == 0 )
		    spos->k_nargs = 0;

	} else {
		findentry(spos, 1);
	}
}



/* 
 * findentry -- assuming k_fp (and k_pc?) is already set, we set the
 * k_nargs, k_entry and k_caller fields in the stackpos structure.  This
 * routine is called from stacktop() and from nextframe().  It assumes it
 * is not dealing with a "leaf" procedure.  The k_entry is easy to find
 * for any frame except a "leaf" routine.  On a sparc, since we cannot
 * deduce the nargs, we'll call it "6".  (This can be overridden with the
 * "$cXX" command, where XX is a one- or two-digit hex number which will
 * tell adb how many parameters to display.)
 *
 * Note -- findentry is also expected to call findsym, thus setting
 * cursym to the symbol at the entry point for the current proc.
 * If this call was an indirect one, we rely on the symbol thus
 * found; otherwise we could not find our entry point.
 */
#define CALL_INDIRECT -1 /* flag from calltarget for an indirect call */

void
findentry (spos, fromTop )
	register struct stackpos *spos;
	int fromTop;
{ 
	char *saveflg = errflg; 
	long offset;


	errflg = 0;
	spos->k_caller = get( spos->k_fp + FR_SAVPC, DSP );
	if( errflg  &&  fromTop  /* &&  regs_not_on_stack( )*/ ) {
		errflg = 0;
		spos->k_fp = readreg(Reg_FP);
		spos->k_caller = get( spos->k_fp + FR_SAVPC, DSP );
	}
	if( errflg == 0 ) {
		spos->k_entry = calltarget( spos->k_caller );
	}

#if DEBUG
	dump_spos( "Findentry called with :->", spos );
#endif DEBUG

	if( errflg == 0 ) {
	    db_printf(1, "findentry:  caller 0x%X, entry 0x%X",
		spos->k_caller, spos->k_entry );
	} else {
	    db_printf(1, "findentry:  caller 0x%X, errflg %s",
		spos->k_caller, errflg );
	}

	if( errflg  ||  spos->k_entry == 0 ) {
		/*
		 * oops -- we touched something we ought not to have.
		 * cannot trace caller of "start"
		 */
		spos->k_entry = MAXINT;
		spos->k_nargs = 0;
		spos->k_fp = 0;   /* stopper for printstack */
		errflg = saveflg; /* you didn't see this */
		return;
	}
	errflg = saveflg;

	/* first 6 args are in regs -- may be overridden by trampcheck */
	spos->k_nargs = NARG_REGS;
	spos->k_flags &= ~K_LEAF;

	if( spos->k_entry == CALL_INDIRECT ) {
		offset = findsym( spos->k_pc );
		if( offset != MAXINT ) {
			spos->k_entry = cursym->s_value ;
		} else {
			spos->k_entry = MAXINT;
		}
#ifndef KADB
		trampcheck( spos );
#endif !KADB
	}

#if DEBUG
	dump_spos( "Findentry returns :->", spos );
#endif DEBUG
}



/*
 * is_leaf_proc -- figure out whether the routine we are in SAVEd its
 * registers.  If it did NOT, is_leaf_proc returns true and sets the k_entry
 * and k_caller fields of spos.   Here's why we have to know it:
 *
 *	Normal (non-Leaf) routine	Leaf routine
 * sp->		"my" frame		   caller's frame
 * i7->		caller's PC		   caller's caller's PC
 * o7->		invalid			   caller's PC
 *
 * I.e., we don't know who our caller is until we know if we're a
 * leaf proc.   (Note that for adb's purposes, we are considered to be
 * in a leaf proc even if we're stopped in a routine that will, but has
 * not yet, SAVEd its registers.) 
 *
 * The way to find out if we're a leaf proc is to find our own entry point
 * and then check the following few instructions for a "SAVE" instruction.
 * If there is none that are < PC, then we are a leaf proc.
 *
 * We find our own entry point by looking for a the largest symbol whose
 * address is <= the PC.  If the executable has been stripped, we will have
 * to do a little more guesswork; if it's been stripped AND we are in a leaf
 * proc, AND the call was indirect through a register, we may be out of luck.
 */

static
is_leaf_proc ( spos, cur_o7 )
	register struct stackpos *spos;
	addr_t cur_o7;		/* caller's PC if leaf proc (rtn addr) */
				/* invalid otherwise */
{
	addr_t	usp,		/* user's stack pointer */
		upc,		/* user's program counter */
		sv_i7,		/* if leaf, caller's PC ... */
				/* else, caller's caller's PC */
		cto7,		/* call target of call at cur_o7 */
				/* (if leaf, our entry point) */
		cti7,		/* call target of call at sv_i7 */
				/* (if leaf, caller's entry point) */
		near_sym;	/* nearest symbol below PC; we hope it ... */
				/* ... is the address of the proc we're IN */
	int offset, leaf;
	char *saveflg = errflg; 

	errflg = 0;

		/* Collect the info we'll need */
	usp = spos->k_fp;
	upc = spos->k_pc;

#ifdef DEBUG
{ extern int adb_debug;

    /*
     * Used to set usp, upc from core.c_regs.  Make sure that
     * it's now ok not to.
     */
    if( adb_debug  &&  (usp != Prstatus.pr_lwp.pr_reg[R_SP] ||
	upc != Prstatus.pr_lwp.pr_reg[R_PC] )) {
	printf( "is_leaf_proc -- usp or upc confusion:   usp %X upc %X\n",
	    usp, upc );
	printf( "\tPrstatus.pr_lwp.pr_reg[R_SP %X, R_PC %X ).\n",
	    Prstatus.pr_lwp.pr_reg[R_SP], Prstatus.pr_lwp.pr_reg[R_PC] );
    }
}
#endif DEBUG

	offset = findsym( upc, ISYM );
	if( offset == MAXINT ) {
		near_sym = -1;
	} else {
		near_sym = cursym->s_value;

		/*
		 * has_save will look at the first four instructions
		 * at near_sym, unless upc is within there.
		 */
		if( has_save( near_sym, upc ) ) {
			/* Not a leaf proc.  This is the most common case. */
			return 0;
		}
	}


	/*
	 * OK, we either had no save instr or we have no symbols.
	 * See if the saved o7 could possibly be valid.  (We could
	 * get fooled on this one, I think, if we're really in a non-leaf,
	 * have no symbols, and o7 still (can it?) has the address of
	 * an indirect call (a call to ptrcall or a jmp [%reg], [%o7]).)
	 *
	 * Also, if we ARE a leaf, and have no symbols, and o7 was an
	 * indirect call, we *cannot* find our own entry point.
	 */

	/*
	 * Is there a call at o7?  (or jmp w/ r[rd] == %o7)
	 */
	cto7 = calltarget( cur_o7 );
	if( cto7 == 0 ) return 0;		/* nope */

	/*
	 * Is that call near (but less than) where the pc is?
	 * If it's indirect, skip these two checks.
	 */
	db_printf(1, "Is_leaf_proc cur_o7 %X, cto7 %X\n", cur_o7, cto7 );

	if( cto7 == CALL_INDIRECT ) {
		if( near_sym != -1 ) {
			cto7 = near_sym;	/* best guess */
		} else {
			errflg = "Cannot trace stack" ;
			return 0;
		}
	} else {
		(void) get( cto7, ISP );	/* is the address ok? */
		if( errflg  ||  cto7 > upc ) {
			errflg = saveflg ;
			return 0;	/* nope */
		}

		/*
		 * Is the caller's call near that call?
		 */
		sv_i7 = get( usp + FR_SAVPC, SSP );
		cti7 = calltarget( sv_i7 );

		db_printf(1, "Is_leaf_proc caller's call sv_i7 %X, cti7 %X\n",
		    sv_i7, cti7 );

		if( cti7 != CALL_INDIRECT ) {
			if( cti7 == 0 ) return 0;
			(void) get( cti7, ISP );	/* is the address ok? */
			if( errflg  ||  cti7 > cur_o7 ) {
				errflg = saveflg ;
				return 0;	/* nope */
			}
		}

		/*
		 * check for a SAVE instruction
		 */
		if( has_save( cto7 ) ) {
			/* not a leaf. */
			return 0;
		}
	}

	/*
	 * Set the rest of the appropriate spos fields.
	 */
	spos->k_caller = cur_o7;
	spos->k_entry = cto7;
	spos->k_flags |= K_LEAF;

	/*
	 * Yes, it is possible (pathological, but possible) for a
	 * leaf routine to be called by _sigtramp.  Check for this.
	 */
#ifndef KADB
	trampcheck( spos );
#endif !KADB

	return 1;

} /* end is_leaf_proc */



/*
 * The "save" is typically the third instruction in a routine.
 * SAVE_INS is set to the farthest (in bytes!, EXclusive) we should reasonably
 * look for it.  "xlimit" (if it's between procaddr and procaddr+SAVE_INS)
 * is an overriding upper limit (again, exclusive) -- i.e., it is the
 * address after the last instruction we should check.
 */
#define SAVE_INS (5*4)

has_save ( procaddr, xlimit )
	addr_t procaddr, xlimit;
{
	char *asc_instr, *disassemble();

	if( procaddr > xlimit  ||  xlimit > procaddr + SAVE_INS ) {
		xlimit = procaddr + SAVE_INS ;
	}

	/*
	 * Find the first three instructions of the current proc:
	 * If none is a SAVE, then we are in a leaf proc and will
	 * have trouble finding caller's PC.
	 */
	while( procaddr < xlimit ) {
		asc_instr = disassemble( get( procaddr, ISP), procaddr );

		if( firstword( asc_instr, "save" ) ) {
			return 1;	/* yep, there's a save */
		}

		procaddr += 4;	/* next instruction */
	}

	return 0;	/* nope, there's no save */
} /* end has_save */


static
firstword ( ofthis, isthis )
	char *ofthis, *isthis;
{
	char *ws;

	while( *ofthis == ' '  ||  *ofthis == '\t' )
		++ofthis;
	ws = ofthis;
	while( *ofthis  &&  *ofthis != ' '  &&  *ofthis != '\t' )
		++ofthis;

	*ofthis = 0;
	return strcmp( ws, isthis ) == 0 ;
}



/*
 * calltarget returns 0 if there is no call there or if there is
 * no "there" there.  A sparc call is a 0 bit, a 1 bit and then
 * the word offset of the target (I.e., one fourth of the number
 * to add to the pc).  If there is a call but we can't tell its
 * target, we return "CALL_INDIRECT".
 *
 * Two complications:
 * 1-	it might be an indirect jump, in which case we can't know where
 *	its target was (the register was very probably modified since
 *	the call occurred).
 * 2-	it might be a "jmp [somewhere], %o7"  (r[rd] is %o7).
 *	if somewhere is immediate, we can check it, but if it's
 *	a register, we're again out of luck.
 */
static
calltarget ( calladdr )
	addr_t calladdr;
{
	char *saveflg = errflg; 
	long instr, offset, symoffs, ct, jt;

	errflg = 0;
	instr = get( calladdr, ISP );
	if( errflg ) {
		errflg = saveflg;
		return 0;	/* no "there" there */
	}

	if( X_OP(instr) == SR_CALL_OP ) {
		/* Normal CALL instruction */
		offset = SR_WA2BA( instr );
		ct = offset + calladdr;

		/*
		 * If the target of that call (ct) is an indirect jump
		 * through a register, then say so.
		 */
		instr = get( ct, ISP );
		if( errflg ) {
			errflg = saveflg;
			return 0;
		}
			
		if( ijmpreg( instr ) )	return CALL_INDIRECT;
		else			return ct;
	}

	/*
	 * Our caller wasn't a call.  Was it a jmp?
	 */
	return  jmpcall( instr );
}


static struct {
	int op, rd, op3, rs1, imm, rs2, simm13;
} jmp_fields;

static
ijmpreg ( instr ) long instr ; {
	if( splitjmp( instr ) ) {
		return jmp_fields.imm == 0  ||  jmp_fields.rs1 != 0 ;
	} else {
		return 0;
	}
}

static
jmpcall ( instr ) long instr; {
  int dest ;

	if(  splitjmp( instr ) == 0 	  /* Give up if it ain't a jump, */
	   || jmp_fields.rd != Reg_O7 ) { /* or it doesn't save pc into %o7 */
		return (CALL_INDIRECT);	/* ... a useful white lie */
	}

	/*
	 * It is a jump that saves pc into %o7.  Find its target, if we can.
	 */
	if( jmp_fields.imm == 0  ||  jmp_fields.rs1 != 0 )
		return CALL_INDIRECT;	/* can't find target */

	/*
	 * The target is simm13, sign extended, not even pc-relative.
	 * So sign-extend and return it.
	 */
	return SR_SEX13(instr);
}

static
splitjmp ( instr ) long instr; {

	jmp_fields.op     = X_OP( instr );
	jmp_fields.rd     = X_RD( instr );
	jmp_fields.op3    = X_OP3( instr );
	jmp_fields.rs1    = X_RS1( instr );
	jmp_fields.imm    = X_IMM( instr );
	jmp_fields.rs2    = X_RS2( instr );
	jmp_fields.simm13 = X_SIMM13( instr );

	if( jmp_fields.op == SR_FMT3a_OP )
		return  jmp_fields.op3 == SR_JUMP_OP ;
	else
		return 0;
}
	

/*
 * nextframe replaces the info in spos with the info appropriate
 * for the next frame "up" the stack (the current routine's caller).
 *
 * Called from printstack (printsr.c) and qualified (expr.c).
 */
nextframe (spos)
	register struct stackpos *spos;
{
	int val, regp, i;
	register instruc;


#ifndef KADB
	if (spos->k_flags & K_CALLTRAMP) {
		trampnext( spos );
		errflg = 0;
	} else
#endif !KADB
	{
		if (spos->k_entry == MAXINT) {
			/*
			 * we don't know what registers are
			 * involved here--invalidate all
			 */
			for (i = FIRST_STK_REG; i <= LAST_STK_REG; i++) {
				spos->k_regloc[ REG_RN(i) ] = -1;
			}
		} else {
			for (i = FIRST_STK_REG; i <= LAST_STK_REG; i++) {
				spos->k_regloc[ REG_RN(i) ] =
					spos->k_fp + 4*REG_RN(i) ;
			}
		}

		/* find caller's pc and fp */
		spos->k_pc = spos->k_caller;

		if( (spos->k_flags & (K_LEAF|K_SIGTRAMP)) == 0 ) {
			spos->k_fp = get(spos->k_fp+FR_SAVFP, DSP);
		}
		/* else (if it is a leaf or SIGTRAMP), don't change fp. */

		/* 
		 * now we have assumed the identity of our caller.
		 */
		if( spos->k_flags & K_SIGTRAMP ) {
		    /* Preserve K_LEAF */
		    spos->k_flags = (spos->k_flags | K_TRAMPED) & ~K_SIGTRAMP ;
		} else {
		    spos->k_flags = 0;
		}
		findentry(spos, 0);
	}
	db_printf(1, "nextframe returning %X",  spos->k_fp );
	return (spos->k_fp);

} /* end nextframe */


#ifndef KADB

/*
 * signal handling routines, sort of:
 * The following set of routines (tramp*) handle situations where
 * the stack trace includes a caught signal.
 *
 * If the current PC is in _sigtramp, then the debuggee has
 * caught a signal.  This causes an anomaly in the stack trace.
 *
 * trampcheck is called from findentry, and just sets the K_CALLTRAMP
 * flag (warning nextframe that the next frame will be _sigtramp).  Its
 * only effect on this line of the stack trace is to make
 * sure that only three arguments are printed out.
 *
 * [[  trampnext (see below) is called from nextframe just before the	]]
 * [[  "_sigtramp" frame is printed out; it sets up the stackpos so	]]
 * [[  as to be able to find the interrupted routine.			]]
 *
 * When the return address is in sigtramp, the current routine
 * is your signal-catcher routine.  It has been called with three
 * parameters:  (the signal number, a signal-code, and an address
 * of a sigcontext structure).
 */
void
trampcheck (spos) register struct stackpos *spos; {

	if( trampsym == 0 ) return;

#if DEBUG
	db_printf(1, "In trampcheck, cursym " );
	if( cursym == 0 ) db_printf(1, "is NULL\n" );
	else db_printf(1, "%X <%s>\n", cursym->s_value, cursym->s_name );
#endif DEBUG

	findsym(spos->k_caller, ISYM);

#if DEBUG
	db_printf(1, "In trampcheck after findsym, cursym " );
	if( cursym == 0 ) db_printf(1, "is NULL\n" );
	else db_printf(1, "%X <%s>\n", cursym->s_value, cursym->s_name );
#endif DEBUG

	if( cursym != trampsym ) return;

	spos->k_flags |= K_CALLTRAMP;
	spos->k_nargs = 3;

#if DEBUG
	db_printf(1, "trampcheck:  trampsym 0x%X->%X caller %X pc %X\n",
		trampsym, trampsym->s_value, spos->k_caller, spos->k_pc );
#endif
}

/*
 * trampnext sets the stackpos structure up so that "sighandler"
 * (the C library routine that catches signals and calls your C
 * signal handler) will show up as the next routine in the stack
 * trace, and so that the next routine found after that will be
 * the one that was interrupted.  One complication is that the
 * interrupted routine may have been a leaf routine.
 *
 * Let's give the stack frame where we found the PC in sighandler a
 * name:  "SHSF".
 *
 * if the interrupted routine was not a leaf routine:
 *    SHSF:fp points to a garbage register-save window.  Ignore it.
 *    The ucontext contains an sp and a pc -- the pc is an address
 *    in the routine that was interrupted, and the sp points to a valid
 *    stack frame.  Just go on from there.
 *	
 * if the interrupted routine was a leaf, it's much less straightforward:
 *    SHSF:fp points to a register-save window that includes
 *    the return address of the leaf's caller, and the arguments
 *    (o-regs) that were passed to the leaf, but not a valid sp.
 *    The next stack frame is found through the old-sp in the ucontext.
 *
 * This strategy will be complicated further if we decide to support
 * the "$cXX" command that looks for more than six parameters.
 */

/*
 * This sneaky and icky routine is called only by trampnext.
 */
static ucontext_t *
find_ucp ( kfp, was_leaf )
	addr_t kfp;
	int was_leaf;
{
	addr_t ucp, wf;

	wf = was_leaf ? kfp : get( kfp + FR_IREG(Reg_FP), DSP );
	ucp = get( wf + 40, DSP );
#if DEBUG
	db_printf(1, "find_ucp kfp = %X, find_ucp returns %X, ", kfp, ucp );
#endif
	return (ucontext_t *) ucp;
}

void
trampnext (spos) struct stackpos *spos; {
  int was_leaf;
  struct stackpos tsp;
  addr_t maybe_o7, save_fp;
  ucontext_t *ucp; /* address in the subprocess.  Don't dereference */

#if DEBUG
	dump_spos( "trampnext :-", spos );

	db_printf(1, "In trampnext, cursym " );
	if( cursym == 0 ) db_printf(1, "is NULL\n" );
	else db_printf(1, "%X <%s>\n", cursym->s_value, cursym->s_name );
#endif DEBUG

	/*
	 * The easy part -- set up the spos for _sighandler itself.
	 */
	spos->k_pc = spos->k_caller;
	spos->k_entry = trampsym->s_value;
	was_leaf = ( spos->k_flags & K_LEAF );
	spos->k_flags = K_SIGTRAMP;
	spos->k_nargs = 0;

	/*
	 * The hard part -- set up the spos to enable it to find
	 * sighandler's caller.  Need to know whether it was a leaf.
	 */
	ucp = find_ucp( spos->k_fp, was_leaf );
	tsp = *spos;

	maybe_o7 = get( spos->k_fp + FR_SAVPC, DSP );
	tsp.k_pc = get( &ucp->uc_mcontext.gregs[PC], DSP );

	/* set K_LEAF for use in printstack */
	if( is_leaf_proc( &tsp, maybe_o7 ) ) {
	    db_printf(1, "trampnext thinks it's a leaf proc.\n" );
	    spos->k_flags |= K_LEAF;
	} else {
	    db_printf(1, "trampnext thinks it's not a leaf proc.\n" );
	}

	spos->k_fp = get( &ucp->uc_mcontext.gregs[SP], DSP );
	spos->k_caller = get( &ucp->uc_mcontext.gregs[PC], DSP );

#if DEBUG
	db_printf(1, "In trampnext, cursym " );
	if( cursym == 0 ) db_printf(1, "is NULL\n" );
	else db_printf(1, "%X <%s>\n", cursym->s_value, cursym->s_name );
	dump_spos( "trampnext thinks it's done.  Its own tsp ==", &tsp );
	dump_spos( "trampnext has spos ==", spos );
#endif

} /* end trampnext */

#endif /* (not KADB) */

#if DEBUG
dump_spos ( msg, ppos ) char *msg; struct stackpos *ppos; {
  int orb;
  extern int adb_debug;

	if( adb_debug == 0 ) return;

	printf( "%s\n", msg );
	printf( "\tPC %8X\tEntry  %8X\t\tnargs %d\n",
	    ppos->k_pc, ppos->k_entry, ppos->k_nargs );

	printf( "\tFP %8X\tCaller %8X\t\t", ppos->k_fp, ppos->k_caller );

	orb = 0;
	if( ppos->k_flags & K_CALLTRAMP ) {
		orb=1; printf( "K_CALLTRAMP" );
	}
	if( ppos->k_flags & K_SIGTRAMP ) {
		if( orb ) printf( " | " );
		printf( "K_SIGTRAMP" );
		orb=1;
	}
	if( ppos->k_flags & K_LEAF ) {
		if( orb ) printf( " | " );
		printf( "K_LEAF" );
		orb=1;
	}
	if( ppos->k_flags & K_TRAMPED ) {
		if( orb ) printf( " | " );
		printf( "K_TRAMPED" );
		orb=1;
	}

	printf( "\n" );
}
#endif
