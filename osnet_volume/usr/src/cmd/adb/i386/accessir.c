/*
 * Copyright (c) 1987-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)accessir.c	1.11	98/07/21 SMI"

#include "adb.h"
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include "ptrace.h"
#include <sys/errno.h>
#include "fpascii.h"
#include "symtab.h"
#ifndef KADB
#include "allregs.h"
#include <ieeefp.h>
#endif

/*
 * adb's idea of the current value of most of the
 * processor registers lives in "adb_regs".
 */
#ifdef KADB
struct regs adb_regs;
#define adb_oreg (adb_regs)
#define adb_ireg (adb_regs)
#define adb_lreg (adb_regs)
#else
struct allregs adb_regs;
#endif

#ifdef KADB
/* nwindow is now a variable whose value is known to kadb.
*/
extern int nwindows;

#ifdef	NWINDOW
#	undef	NWINDOW			/* do it my way */
#endif	/* NWINDOW */
#define NWINDOW nwindows	/* # of implemented windows */

#define CWP  (((adb_regs.r_psr & 15) +1) % NWINDOW)

#define adb_sp	 (adb_oreg.r_esp)
#define adb_o7	 (adb_oreg[7])

#else /* !KADB */
/*
 * Libkvm is used (by adb only) to dig things out of the kernel
 */
#include <kvm.h>
#include <sys/ucontext.h>

extern kvm_t *kvmd;					/* see main.c */
extern struct asym *trampsym;				/* see setupsr.c */


/*
 * Read a word from kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kread(addr, p)
	unsigned addr;
	int *p;
{
	db_printf(5, "kread: addr=%u, p=%X", addr, p);
	if (kvm_read(kvmd, (long) addr, (char *) p, sizeof *p) != sizeof *p)
		return -1;
	db_printf(5, "kread: success");
	return 0;
}

/*
 * Write a word to kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kwrite(addr, p)
	unsigned addr;
	int *p;
{
	db_printf(5, "kwrite: addr=%u, p=%X", addr, p);
	if (kvm_write(kvmd, (long)addr, (char *) p, sizeof *p) != sizeof *p)
		return -1;
	db_printf(5, "kwrite: success");
	return 0;
}
#endif /* !KADB */

extern	struct stackpos exppos;


tbia(void)
{
#ifndef KADB
	db_printf(5, "tbia: sets exppos.k_fp to 0");
	return exppos.k_fp = 0;
#else
	return 0;
#endif	
}



/*
 * Construct an informative error message
 */
static void
regerr(reg)
	int reg;
{
	static char rw_invalid[60];

	if (reg < 0  ||  reg > NREGISTERS)
	    sprintf(rw_invalid, "Invalid register number (%d)", reg);
	else
	    sprintf(rw_invalid, "Invalid register %s (%d)",
							regnames[reg], reg);
	errflg = rw_invalid;
	db_printf(3, "regerr: errflg=%s", errflg);
}

/*
 * reg_address is given an adb register code;
 * it fills in the (global)adb_raddr structure.
 * "Fills in" means that it figures out the register type
 * and the address of where adb keeps its idea of that register's
 * value (i.e., in adb's own (global)adb_regs structure).
 *
 * reg_address is called by setreg() and readreg();
 * it returns nothing.
 */
void
reg_address(reg)
	int reg;
{
#ifdef KADB
	register struct regs *arp = &adb_regs;
#else
	register struct allregs *arp = &adb_regs;
#endif
	register struct adb_raddr *ra = &adb_raddr;
#ifndef KADB
	register struct _fpstate *afp = (struct _fpstate *)
		&Prfpregs.fp_reg_set.fpchip_state.state;
#endif

	db_printf(5, "reg_address: reg=%D", reg);
	ra->ra_type = r_long;
	switch (reg) {
		case REG_RN(GS):
			ra->ra_raddr = & arp->r_gs;
			ra->ra_type = r_short;
			break;
		case REG_RN(FS):
			ra->ra_raddr = & arp->r_fs;
			ra->ra_type = r_short;
			break;
		case REG_RN(ES):
			ra->ra_raddr = & arp->r_es;
			ra->ra_type = r_short;
			break;
		case REG_RN(DS):
			ra->ra_raddr = & arp->r_ds;
			ra->ra_type = r_short;
			break;
		case REG_RN(EDI):
			ra->ra_raddr = & arp->r_edi;
			break;
		case REG_RN(ESI):
			ra->ra_raddr = & arp->r_esi;
			break;
		case REG_RN(EBP):
			ra->ra_raddr = & arp->r_ebp;
			break;
		case REG_RN(ESP):
			ra->ra_raddr = & arp->r_esp;
			break;
		case REG_RN(EBX):
			ra->ra_raddr = & arp->r_ebx;
			break;
		case REG_RN(EDX):
			ra->ra_raddr = & arp->r_edx;
			break;
		case REG_RN(ECX):
			ra->ra_raddr = & arp->r_ecx;
			break;
		case REG_RN(EAX):
			ra->ra_raddr = & arp->r_eax;
			break;
		case REG_RN(TRAPNO):
			ra->ra_raddr = & arp->r_trapno;
			break;
		case REG_RN(ERR):
			ra->ra_raddr = & arp->r_err;
			break;
		case REG_RN(EIP):
			ra->ra_raddr = & arp->r_eip;
			break;
		case REG_RN(CS):
			ra->ra_raddr = & arp->r_cs;
			ra->ra_type = r_short;
			break;
		case REG_RN(EFL):
			ra->ra_raddr = & arp->r_efl;
			break;
		case REG_RN(UESP):
			ra->ra_raddr = & arp->r_uesp;
			break;
		case REG_RN(SS):
			ra->ra_raddr = & arp->r_ss;
			ra->ra_type = r_short;
			break;
#ifndef KADB
		case REG_FCW:
			ra->ra_raddr = (int *) & afp->cw;
			break;
		case REG_FSW:
			ra->ra_raddr = (int *) & afp->sw;
			break;
		case REG_FTAG:
			ra->ra_raddr = (int *) & afp->tag;
			break;
		case REG_FIP:
			ra->ra_raddr = (int *) & afp->ipoff;
			break;
		case REG_FCS:
			ra->ra_raddr = (int *) & afp->cssel;
			ra->ra_type = r_short;
			break;
		case REG_FOP:
			ra->ra_raddr = (int *) ((char *) & afp->cssel + 2);
			ra->ra_type = r_short;
			break;
		case REG_FDATAOFF:
			ra->ra_raddr = (int *) & afp->dataoff;
			break;
		case REG_FDATASEL:
			ra->ra_raddr = (int *) & afp->datasel;
			break;
		case REG_ST0:
			ra->ra_raddr = (int *) & afp->_st[0].significand[3];
			break;
		case REG_ST0A:
			ra->ra_raddr = (int *) & afp->_st[0].significand[1];
			break;
		case REG_ST0B:
			ra->ra_raddr = (int *) & afp->_st[0].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST1:
			ra->ra_raddr = (int *) & afp->_st[1].significand[3];
			break;
		case REG_ST1A:
			ra->ra_raddr = (int *) & afp->_st[1].significand[1];
			break;
		case REG_ST1B:
			ra->ra_raddr = (int *) & afp->_st[1].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST2:
			ra->ra_raddr = (int *) & afp->_st[2].significand[3];
			break;
		case REG_ST2A:
			ra->ra_raddr = (int *) & afp->_st[2].significand[1];
			break;
		case REG_ST2B:
			ra->ra_raddr = (int *) & afp->_st[2].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST3:
			ra->ra_raddr = (int *) & afp->_st[3].significand[3];
			break;
		case REG_ST3A:
			ra->ra_raddr = (int *) & afp->_st[3].significand[1];
			break;
		case REG_ST3B:
			ra->ra_raddr = (int *) & afp->_st[3].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST4:
			ra->ra_raddr = (int *) & afp->_st[4].significand[3];
			break;
		case REG_ST4A:
			ra->ra_raddr = (int *) & afp->_st[4].significand[1];
			break;
		case REG_ST4B:
			ra->ra_raddr = (int *) & afp->_st[4].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST5:
			ra->ra_raddr = (int *) & afp->_st[5].significand[3];
			break;
		case REG_ST5A:
			ra->ra_raddr = (int *) & afp->_st[5].significand[1];
			break;
		case REG_ST5B:
			ra->ra_raddr = (int *) & afp->_st[5].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST6:
			ra->ra_raddr = (int *) & afp->_st[6].significand[3];
			break;
		case REG_ST6A:
			ra->ra_raddr = (int *) & afp->_st[6].significand[1];
			break;
		case REG_ST6B:
			ra->ra_raddr = (int *) & afp->_st[6].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_ST7:
			ra->ra_raddr = (int *) & afp->_st[7].significand[3];
			break;
		case REG_ST7A:
			ra->ra_raddr = (int *) & afp->_st[7].significand[1];
			break;
		case REG_ST7B:
			ra->ra_raddr = (int *) & afp->_st[7].significand[0];
			ra->ra_type = r_hishort;
			break;
		case REG_XFSW:
			ra->ra_raddr = (int *) & afp->status;
			break;
#endif
		case REG_AX:
			ra->ra_raddr = & arp->r_eax;
			ra->ra_type = r_short;
			break;
		case REG_BX:
			ra->ra_raddr = & arp->r_ebx;
			ra->ra_type = r_short;
			break;
		case REG_CX:
			ra->ra_raddr = & arp->r_ecx;
			ra->ra_type = r_short;
			break;
		case REG_DX:
			ra->ra_raddr = & arp->r_edx;
			ra->ra_type = r_short;
			break;
		case REG_SI:
			ra->ra_raddr = & arp->r_esi;
			ra->ra_type = r_short;
			break;
		case REG_DI:
			ra->ra_raddr = & arp->r_edi;
			ra->ra_type = r_short;
			break;
		case REG_BP:
			ra->ra_raddr = & arp->r_ebp;
			ra->ra_type = r_short;
			break;
		case REG_WSP:
			ra->ra_raddr = & arp->r_esp;
			ra->ra_type = r_short;
			break;
		case REG_WIP:
			ra->ra_raddr = & arp->r_eip;
			ra->ra_type = r_short;
			break;
		case REG_WFL:
			ra->ra_raddr = & arp->r_efl;
			ra->ra_type = r_short;
			break;
		case REG_AL:
			ra->ra_raddr = & arp->r_eax;
			ra->ra_type = r_byte;
			break;
		case REG_AH:
			ra->ra_raddr = (int *) ((char *) & arp->r_eax + 1);
			ra->ra_type = r_byte;
			break;
		case REG_BL:
			ra->ra_raddr = & arp->r_ebx;
			ra->ra_type = r_byte;
			break;
		case REG_BH:
			ra->ra_raddr = (int *) ((char *) & arp->r_ebx + 1);
			ra->ra_type = r_byte;
			break;
		case REG_CL:
			ra->ra_raddr = & arp->r_ecx;
			ra->ra_type = r_byte;
			break;
		case REG_CH:
			ra->ra_raddr = (int *) ((char *) & arp->r_ecx + 1);
			ra->ra_type = r_byte;
			break;
		case REG_DL:
			ra->ra_raddr = & arp->r_edx;
			ra->ra_type = r_byte;
			break;
		case REG_DH:
			ra->ra_raddr = (int *) ((char *) & arp->r_edx + 1);
			ra->ra_type = r_byte;
			break;
		default:
			regerr(reg);
			ra->ra_raddr = 0;
			ra->ra_type = r_invalid;
			break;
        }
	db_printf(2, "reg_address: reg=%D, name='%s'\n\tra_type=%D, ra_raddr=%X",
		   reg, (reg >= 0 && reg < NREGISTERS) ? regnames[reg] : "?",
			adb_raddr.ra_type, adb_raddr.ra_raddr);
}

void
setreg(reg, val)
	int reg;
	int val;
{
	db_printf(4, "setreg: reg=%D, val=%D", reg, val);
#ifndef KADB
	switch(reg) {
	case GS:
	case FS:
	case ES:
	case DS:
	case CS:
	case SS:
		val &= 0xffff;	/* mask off selector registers */
		break;
	}
#endif
	reg_address(reg);
	switch (adb_raddr.ra_type) {
		 case r_long:
			*(adb_raddr.ra_raddr) = val;
			if (reg == REG_PC) {
				userpc = (addr_t)val;
				db_printf(2, "setreg: userPC=%X", userpc);
			}
			break;

		 case r_short:
			* (short *) (adb_raddr.ra_raddr) = val;
			if (reg == REG_WIP) {
				userpc = (addr_t) (((int) userpc & 0xffff0000) |
					(val & 0x0000ffff));
				db_printf(2, "setreg: userPC=%X", userpc);
			}
			break;

		 case r_hishort:
			* (short *) (adb_raddr.ra_raddr) = val >> 16;
			break;

		 case r_byte:
			* (char *) (adb_raddr.ra_raddr) = val;
			break;

		 default:	/* should never get here */
			db_printf(2, "setreg: bogus ra_type: %D\n",
							adb_raddr.ra_type);
			break;
	}
}


/*
 * readreg -- retrieve value of register reg from adb_regs.
 */
readreg(reg)
	int reg;
{
	int val = 0;

	db_printf(4, "readreg:  reg=%D, ra_raddr=%X",
					    reg, adb_raddr.ra_raddr);
	reg_address(reg);
	switch (adb_raddr.ra_type) {
		 case r_long:
			val = *(adb_raddr.ra_raddr);
			break;

		 case r_short:
			val = * (short *) (adb_raddr.ra_raddr) & 0xffff;
			break;

		 case r_hishort:
			val = * (short *) (adb_raddr.ra_raddr) << 16;
			break;

		 case r_byte:
			val = * (char *) (adb_raddr.ra_raddr) & 0xff;
			break;

		 default:	/* should never get here */
			db_printf(2, "readreg: bogus ra_type: %D\n",
							adb_raddr.ra_type);
			break;
	}
	db_printf(4, "readreg:  returns %X", val);
	return val;
}

/*
 * For ptrace(SETREGS or GETREGS) to work, the registers must be in
 * the form that they take in the core file (instead of the form used
 * by the access routines in this file, i.e., the full machine state).
 * These routines copy the relevant registers.
 */
void
regs_to_core(void)
{
#ifdef KADB
	struct regs *a = &adb_regs;
#else
	struct allregs *a = &adb_regs;
#endif

	Prstatus.pr_lwp.pr_reg[GS] = a->r_gs;
	Prstatus.pr_lwp.pr_reg[FS] = a->r_fs;
	Prstatus.pr_lwp.pr_reg[ES] = a->r_es;
	Prstatus.pr_lwp.pr_reg[DS] = a->r_ds;
	Prstatus.pr_lwp.pr_reg[EDI] = a->r_edi;
	Prstatus.pr_lwp.pr_reg[ESI] = a->r_esi;
	Prstatus.pr_lwp.pr_reg[EBP] = a->r_ebp;
	Prstatus.pr_lwp.pr_reg[ESP] = a->r_esp;
	Prstatus.pr_lwp.pr_reg[EBX] = a->r_ebx;
	Prstatus.pr_lwp.pr_reg[EDX] = a->r_edx;
	Prstatus.pr_lwp.pr_reg[ECX] = a->r_ecx;
	Prstatus.pr_lwp.pr_reg[EAX] = a->r_eax;
	Prstatus.pr_lwp.pr_reg[TRAPNO] = a->r_trapno;
	Prstatus.pr_lwp.pr_reg[ERR] = a->r_err;
	Prstatus.pr_lwp.pr_reg[EIP] = a->r_eip;
	Prstatus.pr_lwp.pr_reg[CS] = a->r_cs;
	Prstatus.pr_lwp.pr_reg[EFL] = a->r_efl;
	Prstatus.pr_lwp.pr_reg[UESP] = a->r_uesp;
	Prstatus.pr_lwp.pr_reg[SS] = a->r_ss;
	db_printf(4, "regs_to_core: copied regs from adb_regs to Prstatus.pr_lwp.pr_reg[]");
	return;
}

#ifndef KADB

void
core_to_regs()
{
	const int mask = 0xffff;	/* mask off selector registers */
	register int reg;
	register struct allregs *a = &adb_regs;

	a->r_gs = Prstatus.pr_lwp.pr_reg[GS] & mask;
	a->r_fs = Prstatus.pr_lwp.pr_reg[FS] & mask;
	a->r_es = Prstatus.pr_lwp.pr_reg[ES] & mask;
	a->r_ds = Prstatus.pr_lwp.pr_reg[DS] & mask;
	a->r_edi = Prstatus.pr_lwp.pr_reg[EDI];
	a->r_esi = Prstatus.pr_lwp.pr_reg[ESI];
	a->r_ebp = Prstatus.pr_lwp.pr_reg[EBP];
	a->r_esp = Prstatus.pr_lwp.pr_reg[ESP];
	a->r_ebx = Prstatus.pr_lwp.pr_reg[EBX];
	a->r_edx = Prstatus.pr_lwp.pr_reg[EDX];
	a->r_ecx = Prstatus.pr_lwp.pr_reg[ECX];
	a->r_eax = Prstatus.pr_lwp.pr_reg[EAX];
	a->r_trapno = Prstatus.pr_lwp.pr_reg[TRAPNO];
	a->r_err = Prstatus.pr_lwp.pr_reg[ERR];
	a->r_eip = Prstatus.pr_lwp.pr_reg[EIP];
	a->r_cs = Prstatus.pr_lwp.pr_reg[CS] & mask;
	a->r_efl = Prstatus.pr_lwp.pr_reg[EFL];
	a->r_uesp = Prstatus.pr_lwp.pr_reg[UESP];
	a->r_ss = Prstatus.pr_lwp.pr_reg[SS] & mask;
	db_printf(4, "core_to_regs: copied regs from Prstatus.pr_lwp.pr_reg[] to adb_regs");
	return;
} 
#endif



 
writereg(i, val)
	register i, val;
{
#ifndef KADB
	extern fpa_avail;
#endif

	db_printf(4, "writereg: i=%D, val=%D", i, val);
#ifndef KADB
	if (i < 0 || i >= NREGISTERS) {
		errno = EIO;
		return 0;
	}
#endif
	setreg(i, val);

#ifdef KADB
	ptrace(PTRACE_SETREGS, pid, &adb_regs, 0, 0);
	ptrace(PTRACE_GETREGS, pid, &adb_regs, 0, 0);
#else /* !KADB */
	regs_to_core();

	if (ptrace(PTRACE_SETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0) == -1) {
		perror(corfil);
		db_printf(3, "writereg: ptrace failed, errno=%D", errno);
	}
	if (ptrace(PTRACE_GETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0) == -1) {
		perror(corfil);
		db_printf(3, "writereg: ptrace failed, errno=%D", errno);
	}
	db_printf(2, "writereg: PC=%X", Prstatus.pr_lwp.pr_reg[R_PC]);

	if (fpa_avail) {
		if (ptrace(PTRACE_SETFPREGS, pid, &Prfpregs, 0) == -1) {
			perror(corfil);
			db_printf(3, "writereg: ptrace failed, errno=%D",
									errno);
		}
		if (ptrace(PTRACE_GETFPREGS, pid, &Prfpregs, 0) == -1) {
			perror(corfil);
			db_printf(3, "writereg: ptrace failed, errno=%D",
									errno);
		}
	}
	core_to_regs();

#endif /* !KADB */

	db_printf(2, "writereg: i=%D, val=%X, readreg(i)=%X",
						i, val, readreg(i)) ;
	return sizeof(int);
}

/*
 * stacktop collects information about the topmost (most recently
 * called) stack frame into its (struct stackpos) argument.
 *
 * XXX - This portion of this comment is sparc specific.  However,
 *       i386 code might need to do some work here:
 *                                                           It's
 * easy in most cases to figure out this info, because the kernel
 * is nice enough to have saved the previous register windows into
 * the proper places on the stack, where possible.  But, if we're
 * a leaf routine (one that avoids a "save" instruction, and uses
 * its caller's frame), we must use the r_i* registers in the
 * (struct regs).  *All* system calls are leaf routines of this sort.
 */

stacktop(sp)
	register struct stackpos *sp;
{
	register int i;

	db_printf(4, "stacktop: sp=%X", sp);
	sp->k_pc = readreg(REG_PC);
	sp->k_fp = readreg(REG_FP);
	sp->k_flags = 0;
	for (i = 0; i < NREGISTERS; i++)
		sp->k_regloc[i] = REGADDR(i);
	return findentry(sp, 1);
}

/* 
 * Set the k_caller, k_nargs and k_entry fields in the stackpos structure.
 * This is called from stacktop() and from nextframe().
 */

findentry(sp, first)
	register struct stackpos *sp;
	int first;
{ 
#	define	BYTEMASK	0xff

	register instruc;
	register val;
	caddr_t addr, calladdr, nextword;
	char *saveflg = errflg; 
#ifndef KADB
	extern unsigned int max_nargs;
#endif

	db_printf(4, "findentry: sp=%X, first=%X", sp, first);
	errflg = NULL;
	if (first && ((val = findsym(sp->k_pc, ISYM)) == 0)) {
		/* at entry point */
		addr = (caddr_t) readreg(ESP);
		sp->k_fp = (u_int) addr - 4;
		errflg = NULL;
		addr = (caddr_t) get(addr, DSP);
		db_printf(4, "findentry: addr=%X    s_name='%s'",
		    addr, cursym->s_name);
		goto findargs;
	} else if (first && (val == 1)) {
		/* at entry point + 1 */
		errflg = NULL;
		if ((get(sp->k_pc - 1, DSP) & 0xff) == 0x55) {	/* push %ebp */
			errflg = NULL;
			addr = (caddr_t) readreg(ESP);
			sp->k_fp = (u_int) addr;
			errflg = NULL;
			addr = (caddr_t) get(addr + 4, DSP);
			db_printf(4, "findentry: addr=%X    s_name='%s'",
			    addr, cursym->s_name);
			goto findargs;
		}
	}

	/*
	 * XXX - In the sparc code there is a set of routines (tramp*)
	 * to handle situations where the stack trace includes a caught
	 * signal.  Don't we need them here?
	 */
	sp->k_caller = MAXINT;
	errflg = NULL;
	addr = (caddr_t) get(sp->k_fp + FR_SAVPC, DSP);
	db_printf(2, "findentry: addr=%X", addr);
	if (errflg != NULL) {
		/*
		 * addr is invalid, not in a valid DSP map range
		 */
		db_printf(3, "findentry: %s", errflg);
		sp->k_entry = MAXINT;
		sp->k_nargs = 0;
		errflg = saveflg; /* you didn't see this */
		db_printf(4, "findentry: fails and returns -1");
		return -1;
	}
	errflg = saveflg;
findargs:
	instruc = get(addr - 5, ISP) & BYTEMASK;
	db_printf(2, "findentry: instruc=%X", instruc);
	if (errflg != NULL) {
		/*
		 * instruc is invalid, not in a valid ISP map range
		 */
		db_printf(3, "findentry: %s", errflg);
		sp->k_entry = MAXINT;
		sp->k_nargs = 0;
		errflg = saveflg; /* you didn't see this */
		db_printf(4, "findentry: fails and returns -1");
		return -1;
	}
	db_printf(2, "findentry: instruc %s CALLEIP", 
					(instruc == CALLEIP) ? "==" : "!=");
	if (instruc == CALLEIP) {
		sp->k_entry = (unsigned) (addr + get(addr-4, ISP));
	} else if ((get(addr - 2, ISP) & BYTEMASK) == CALLINDIRECT &&
		 (get(addr - 1, ISP) & CALLRMASK) == CALLDISP0) {
		if (findsym(sp->k_pc, ISYM) != MAXINT)
			sp->k_entry = cursym->s_value;
		else
			sp->k_entry = MAXINT;
	} else if ((get(addr - 3, ISP) & BYTEMASK) == CALLINDIRECT &&
		 (get(addr - 2, ISP) & CALLRMASK) == CALLDISP8 ) {
		if (findsym(sp->k_pc, ISYM) != MAXINT)
			sp->k_entry = cursym->s_value;
		else
			sp->k_entry = MAXINT;
	} else if ((get(addr - 6, ISP) & BYTEMASK) == CALLINDIRECT &&
		 (get(addr - 5, ISP) & CALLRMASK) == CALLDISP32 ) {
		if (findsym(sp->k_pc, ISYM) != MAXINT)
			sp->k_entry = cursym->s_value;
		else
			sp->k_entry = MAXINT;
	} else
		sp->k_entry = (unsigned) (addr - 2 +
					(get(addr-2, ISP) >> 16));
	sp->k_caller = (u_int) addr;
	instruc = get(addr, ISP);
	db_printf(4, "findentry: instruc=%X", instruc);
	db_printf(2, "findentry: (instruc & BYTEMASK)=%X",
							(instruc & BYTEMASK));
	if ((instruc & BYTEMASK) == ADDIMMBYTE)
		sp->k_nargs = ((instruc >> 16) & BYTEMASK) >> 2;
	else if ((instruc & BYTEMASK) == ADDL)
		sp->k_nargs = get(addr + 2, ISP) >> 2;
	else if ((instruc & BYTEMASK) == POPL)
		sp->k_nargs = 1;
	else if (findsym(sp->k_entry, ISYM) != MAXINT && !strcmp(cursym->s_name, "main"))
		sp->k_nargs = 3;
	else
		sp->k_nargs = 6;
#ifndef KADB
	if (sp->k_nargs > max_nargs)
		sp->k_nargs = max_nargs;
#endif
	db_printf(2, "findentry: sp->k_nargs=%D", sp->k_nargs);
	return 0;
}

static
firstword(ofthis, isthis)
	char *ofthis, *isthis;
{
	char *ws;

	while (*ofthis == ' '  ||  *ofthis == '\t')
		++ofthis;
	ws = ofthis;
	while (*ofthis  &&  *ofthis != ' '  &&  *ofthis != '\t')
		++ofthis;

	*ofthis = 0;
	return strcmp(ws, isthis) == 0;
}

/*
 * nextframe replaces the info in sp with the info appropriate
 * for the next frame "up" the stack (the current routine's caller).
 */
nextframe(sp)
	register struct stackpos *sp;
{
	int val, regp, i;
	caddr_t addr;
	caddr_t calladdr;
	register instruc;

	db_printf(4, "nextframe: sp=%X", sp);

	addr = (caddr_t) readreg(ESP);
	if ((sp->k_fp == (u_int) addr) ||
	    (sp->k_fp == (u_int) addr - 4)) {	/* 1st frame */
		errflg = NULL;
		val = findsym(sp->k_pc, ISYM);
		if (val == 0) {		/* at entry point */
			sp->k_pc = (u_int)((caddr_t) get(addr, DSP));
			sp->k_fp = readreg(REG_FP);
		} else if (val == 1) {	/* at entry point + 1 */
			sp->k_pc = (u_int)((caddr_t) get(addr + 4, DSP));
			sp->k_fp = readreg(REG_FP);
		} else {
			sp->k_pc = get(sp->k_fp + FR_SAVPC, DSP);
			sp->k_fp = get(sp->k_fp + FR_SAVFP, DSP);
		}
	} else {
		addr     = (caddr_t) sp->k_entry;
		db_printf(2, "nextframe: addr=%X", addr);
		if (addr == (caddr_t) MAXINT)
			/*
			 * we don't know what registers are
			 * involved here--invalidate all
			 */
			for (i = 0; i < 14; i++)
				sp->k_regloc[i] = -1;
		else
			findregs(sp, addr);
		/*
		 * find caller's pc and fp
		 */
		sp->k_pc = get(sp->k_fp + FR_SAVPC, DSP);
		sp->k_fp = get(sp->k_fp + FR_SAVFP, DSP);
	}
	db_printf(2, "nextframe: sp->k_pc=%X, sp->k_fp=%X",
	    sp->k_pc, sp->k_fp);
	if (sp->k_fp == 0xffffffff)
		return 0;
	/* 
	 * now that we have assumed the identity of our caller, find
	 * how many longwords of argument WE were called with.
	 */
	sp->k_flags = 0;
	(void) findentry(sp, 0);
	db_printf(4, "nextframe: returns sp->k_fp=%X", sp->k_fp);
	return sp->k_fp;
}
