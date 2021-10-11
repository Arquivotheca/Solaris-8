/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)accesssr.c	1.19	99/02/09 SMI"

#include "adb.h"
#include "ptrace.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include "fpascii.h"
#include "symtab.h"
#include "allregs.h"
#include <strings.h>

#define	V9BIAS64	(2048-1)

/*
 * adb's idea of the current value of most of the
 * processor registers lives in "adb_regs". For SPARC-V9 we
 * need a separate structure, since some types and regs differ.
 */

static struct allregs adb_regs;
static struct allregs_v9 adb_regs_v9;

int v9flag = 1;			/* use SPARC-V9 display mode? */
				/* Default to 1 for adb64  */

/*
 * Libkvm is used (by adb only) to dig things out of the kernel
 */
#include <kvm.h>
#include <sys/ucontext.h>

extern kvm_t *kvmd;					/* see main.c */
extern long readstackreg(struct stackpos *, int);

#define	adb_oreg (adb_regs.r_outs)
#define	adb_ireg (adb_regs.r_ins)
#define	adb_lreg (adb_regs.r_locals)

#define	adb_oreg_v9 (adb_regs_v9.r_outs)
#define	adb_ireg_v9 (adb_regs_v9.r_ins)
#define	adb_lreg_v9 (adb_regs_v9.r_locals)

/*
 * Read a word from kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kread(addr, p)
	unsigned long addr;
	int *p;
{
	if (kvm_read(kvmd, (long)addr, (char *)p, sizeof (*p)) != sizeof (*p))
		return (-1);
	return (0);
}

/*
 * Write a word to kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kwrite(addr, p)
	unsigned long addr;
	int *p;
{
	if (kvm_write(kvmd, (long)addr, (char *)p, sizeof (*p)) != sizeof (*p))
		return (-1);
	return (0);
}

/*
 * Construct an informative error message
 */
static void
regerr(reg, wind)
{
	static char rw_invalid[ 60 ];
	char *wp;

	wp = wind ? "window-" : "";
	if (reg < 0 || reg > NREGISTERS) {
		sprintf(rw_invalid, "Invalid %sregister %d", wp, reg);
	} else {
		sprintf(rw_invalid, "Invalid %sregister %s (%d)", wp,
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
static void
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
		if (v9flag) {
			ra->ra_raddr = (long *)&arp_v9->r_pc;
			ra->ra_mode = RA_64BIT;
		} else ra->ra_raddr = &arp->r_pc;
		break;
	case Reg_NPC:
		if (v9flag) {
			ra->ra_raddr = (long *)&arp_v9->r_npc;
			ra->ra_mode = RA_64BIT;
		} else ra->ra_raddr = &arp->r_npc;
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
			ra->ra_raddr = (long *)&(adb_oreg_v9[reg - Reg_O0]);
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
			ra->ra_mode = RA_64BIT;
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
			ra->ra_raddr = (long *)&(adb_ireg_v9[ reg - Reg_I0 ]);
			ra->ra_mode = RA_64BIT;
		} else {
			ra->ra_raddr = &(adb_ireg[ reg - Reg_I0 ]);
		}
		break;

	case Reg_FQ:	/* Can't get the FQ */
		regerr(reg, 0);
		ra->ra_raddr = 0;
		ra->ra_type = r_invalid;
		break;
	case Reg_FSR:
		ra->ra_raddr = (long *)&Prfpregs.pr_fsr;
		ra->ra_type = r_normal;
		break;
	default:
		if (reg >= Reg_F0 && reg <= Reg_F31) {
			ra->ra_type = r_floating;
			ra->ra_raddr =
			    (long *)&Prfpregs.pr_fr.pr_regs[reg - Reg_F0];
		} else if (reg >= Reg_F32 && reg <= Reg_F63) {
			ra->ra_type = r_floating;
			ra->ra_raddr = (long *)
			    &xregs.pr_un.pr_v8p.pr_xfr.pr_regs[reg - Reg_F32];
		} else {
			regerr(reg, 0);
			ra->ra_raddr = 0;
			ra->ra_type = r_invalid;
		}
		break;

	}
}


void
setreg(reg, val)
int reg;
long val;
{
	reg_address(reg);

	switch (adb_raddr.ra_type) {

	case r_gzero:  /* Always zero -- setreg does nothing */
		break;
	case r_floating:
		*(int *)adb_raddr.ra_raddr = (int)val;
		break;
	case r_normal: /* Normal one -- we have a good address */
		*(long *)adb_raddr.ra_raddr = val;
		if (reg == Reg_PC)
			userpc = val;
		break;
	}
}


/*
 * readsavedreg -- retrieve value of register reg from a saved call
 *    frame.  The register must be one of those saved by a "save"
 *    instruction, i.e. Reg_L0 <= reg <= Reg_I7
 */
long
readsavedreg(struct stackpos *pos, int reg)
{
	static char reginvalid[60];

	if (reg < Reg_L0 || reg > Reg_I7) {
	    sprintf(reginvalid, "Invalid window register number %d", reg);
	    errflg = reginvalid;
	    return (0);
	}

	/*
	 * For adb, all windows are saved on the stack
	 */
	return (readstackreg(pos, reg));
}


/*
 * readreg -- retrieve value of register reg from adb_regs.
 */
long
readreg(int reg)
{
	unsigned long val = 0;
	register struct adb_raddr *ra = &adb_raddr;
	int sval = 0;

	reg_address(reg);

	db_printf(1, "readreg:  Reg_Address of reg %d is %X",
		reg, ra->ra_raddr);

	switch (ra->ra_type) {

	case r_gzero:  /* Always zero -- val is already zero */
		break;
	case r_floating: /* floating regs are still 32 bits */
		sval = *(int *)ra->ra_raddr;
		val = (u_longlong_t)sval;
		break;
	case r_normal: /* Normal one -- we have a good address */
		if (ra->ra_mode == RA_64BIT) {
			val = *((u_longlong_t *)(ra->ra_raddr));
			ra->ra_mode = 0;
		} else {
			val = (u_longlong_t)((int)*(ra->ra_raddr));
			val &= 0xffffffff;
		}
		break;
	default:
		db_printf(1, "readreg: unknown reg type %x", ra->ra_type);
		break;
	}
	return (val);
}

static unsigned long
get_from_sp(addr_t sp, unsigned off, int regset)
{
	/*
	 * Given an addr and offset, return the long long point to it
	 * from the child's image
	 */

	unsigned int lower, upper;
	unsigned long adr;
	int is64;

	is64 = 0;
	if (((unsigned long)sp % 2) != 0) { /* Stack pointer is odd */
		    sp = (unsigned long)sp + V9BIAS64;
		    off = off*8; /* reg*4 or reg*8 to get the nth reg */
		    is64++;
	} else off = off*4;
	if (regset == 1) {
		    if (is64)
			    off = off + 8*8;  /* Ins, skip the locals */
		    else off = off + 8*4;

	}
	adr = (unsigned long)sp + (unsigned long)off;
	upper = (unsigned)get((addr_t)adr, DSP); /* Assume big endian */

	if (is64) {
		    adr = adr + 4;
		    lower = (unsigned)get((addr_t)adr, DSP);
	}
	return (is64 ? MAKE_LL(upper, lower) : MAKE_LL(0, upper));
}


static void
put_to_sp(addr_t sp, unsigned off, int regset, unsigned long val)
{
	/*
	 * Given an addr and offset, return the long long point to it
	 * from the child's image
	 */

	unsigned int lower, upper;
	unsigned long adr;
	int is64;

	upper = val >> 32;
	lower = 0xFFFFFFFF & val;
	is64 = 0;
	if (((unsigned long)sp % 2) != 0) { /* Stack pointer is odd */
		sp = (unsigned long)sp + V9BIAS64;
		off = off*8; /* reg*4 or reg*8 to get the nth reg */
		is64++;
	} else off = off*4;
	if (regset == 1) {
		if (is64)
			off = off + 8*8;  /* Ins, skip the locals */
		else off = off + 8*4;

	}
	adr = (unsigned long)sp + (unsigned long)off;
	if (is64) {
		put(adr, SSP, upper);
		put(adr+4, SSP, lower);
	} else put(adr, SSP, lower);
}


void
core_to_regs()
{
	register int reg;

	db_printf(1, "Copy regs from Prstatus.pr_lwp.pr_reg to adb_regs");
	if (v9flag) {
		register struct allregs_v9 *a = &adb_regs_v9;
		register prxregset_t *xp = (prxregset_t *)&xregs;

		a->r_tstate = xp->pr_un.pr_v8p.pr_tstate;
		a->r_pc  = Prstatus.pr_lwp.pr_reg[R_PC];
db_printf(1, " PC:%J\n", Prstatus.pr_lwp.pr_reg[R_PC]);
		a->r_npc = Prstatus.pr_lwp.pr_reg[R_nPC];
		a->r_y   = Prstatus.pr_lwp.pr_reg[R_Y];
		for (reg = 0; reg <= 7; reg++) {

			a->r_outs[reg] =
			    Prstatus.pr_lwp.pr_reg[R_O0 + reg];
			a->r_locals[reg] =
			    get_from_sp(Prstatus.pr_lwp.pr_reg[R_O6], reg, 0);
			a->r_ins[reg] =
			    get_from_sp(Prstatus.pr_lwp.pr_reg[R_O6], reg, 1);
			if (reg < 7)
				a->r_globals[reg] =
				    Prstatus.pr_lwp.pr_reg[R_G1 + reg];


db_printf(1, " outs%d:%J\n", reg, Prstatus.pr_lwp.pr_reg[R_O0 + reg]);
db_printf(1, " locals%d:%J\n", reg, Prstatus.pr_lwp.pr_reg[R_L0 + reg]);
db_printf(1, " ins%d:%J\n", reg, Prstatus.pr_lwp.pr_reg[R_I0 + reg]);
if (reg < 7) db_printf(1, "global%d:%J\n", Prstatus.pr_lwp.pr_reg[R_G1 + reg]);

		}
	} else {
		register struct allregs *a = &adb_regs;

		a->r_psr = Prstatus.pr_lwp.pr_reg[R_CCR];
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
	db_printf(1,
	    "Done copying regs from Prstatus.pr_lwp.pr_reg to adb_regs");
}

/*
 * For ptrace(SETREGS or GETREGS) to work, the registers must be in
 * the form that they take in the core file (instead of the form used
 * by the access routines in this file, i.e., the full machine state).
 * These routines copy the relevant registers.
 */
static void
regs_to_core()
{
	register int reg;

	db_printf(1, "Copy regs to Prstatus.pr_lwp.pr_reg from adb_regs\n");
	if (v9flag) {
		register struct allregs_v9 *a = &adb_regs_v9;

		Prstatus.pr_lwp.pr_reg[R_CCR] = (int)a->r_tstate;
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


		for (reg = 0; reg <= 7; reg++)
			put_to_sp(Prstatus.pr_lwp.pr_reg[R_O6],
				    reg, 0, a->r_locals[reg]);
		for (reg = 0; reg <= 7; reg++)
			put_to_sp(Prstatus.pr_lwp.pr_reg[R_O6],
				    reg, 1, a->r_ins[reg]);
	} else {
		register struct allregs *a = &adb_regs;

		Prstatus.pr_lwp.pr_reg[R_CCR] = a->r_psr;
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

		for (reg = Reg_L0; reg <= Reg_L7; reg++) {
			put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_LREG(reg), SSP,
				a->r_locals[reg - Reg_L0]);
		}
		for (reg = Reg_I0; reg <= Reg_I7; reg++) {
			put(Prstatus.pr_lwp.pr_reg[R_O6] + FR_IREG(reg), SSP,
				a->r_ins[reg - Reg_I0]);
		}
	}

	db_printf(1, "Done copying regs to core from adb_regs\n");
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


int
writereg(int i, long val)
{
	extern int errno;

	if (i >= NREGISTERS) {
		errno = EIO;
		return (0);
	}
	setreg(i, val);

	/* normal adb:  regs in adb_regs must be copied */
	regs_to_core();
	db_printf(1, "writereg:  e.g., PC is %X\n",
		Prstatus.pr_lwp.pr_reg[R_PC]);
	ptrace(PTRACE_SETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0, 0);
	db_printf(1, "writereg after rw_pt:  e.g., PC is %X\n",
		Prstatus.pr_lwp.pr_reg[R_PC]);
	ptrace(PTRACE_SETFPREGS, pid, &Prfpregs, 0, 0);

	return (sizeof (int));
}
