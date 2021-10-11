/*
 * Copyright (c) 1992, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printir.c	1.15	99/03/23 SMI"

/*
 * adb - i386 specific print routines
 *	printregs
 *	printstack
 *	fp_print
 */
#ifndef KADB
#include "ptrace.h"
#include <sys/fp.h>
#include <ieeefp.h>
#include <string.h>
#include <stdlib.h>
#endif
#include "adb.h"
#include "symtab.h"
#ifndef KADB
#include "allregs.h"
#include "fpascii.h"
#endif

int fpa_disasm = 0, fpa_avail = 0, mc68881 = 0;
int nprflag = 0;

#ifndef KADB
static void
print_reg_val(const char *fmt, const int i, const int val)
{
	if ((kernel == LIVE || kernel == CMN_ERR_PANIC) && /* volatile regs */
		(i == EAX || i == ECX || i == EDX || i == EFL || i == UESP)) {
		(void) printf("?");
		return;
	} else
		(void) printf(fmt, val);

	if (i != GS && i != FS && i != ES && i != DS && i != CS &&
				i != SS && i != EFL && i != TRAPNO && i != ERR)
		valpr(val, DSP);

	return;
}
#endif

void
printregs(void)
{
	register int i, val;
#ifdef KADB
	register short *dtp;
	short dt[3];
	char buf[257];
	short *rgdt(), *ridt(), *rldt();
#else
	static void print_reg_val(const char *, const int, const int);
#endif

	db_printf(5, "printregs: called");
	for (i = 0; i <= LAST_NREG/2; i++) {
		val = readreg(i);
#ifdef KADB
		printf("%s%8t%R", regnames[i], val);
		printf("%24t");
		valpr(val, DSP);
		val = readreg(i + LAST_NREG/2);
		printf("%40t%s%8t%R", regnames[i + LAST_NREG/2], val);
		printf("%64t");
		valpr(val, DSP);
#else
		(void) printf("%s%8t", regnames[i]);
		print_reg_val("%R  ", i, val);
		if (i != LAST_NREG/2) {
			register int j = i + LAST_NREG/2 + 1;

			val = readreg(j);
			(void) printf("%40t%s%8t", regnames[j]);
			print_reg_val("%R  ", j, val);
		}
#endif
		(void) printf("\n");
	}
#ifdef KADB
	dtp = rgdt();
	for (i = 0; i < 3; i++)
		dt[i] = *dtp++;
	printf("gdt%8t");
	for (i = 2; i >= 0; i--) {
		if (i == 0)
			printf("/");		/* sep. limit from address */
		sprintf(buf, "%04x", dt[i] & 0xffff);
		printf(buf);
	}
	printf("%40tcr0%8t%R", val = cr0());
	printf("%64t");
	valpr(val, DSP);
	printf("\n");
	dtp = ridt();
	for (i = 0; i < 3; i++)
		dt[i] = *dtp++;
	printf("idt%8t");
	for (i = 2; i >= 0; i--) {
		if (i == 0)
			printf("/");		/* sep. limit from address */
		sprintf(buf, "%04x", dt[i] & 0xffff);
		printf(buf);
	}
	printf("%40tcr2%8t%R", val = cr2());
	printf("%64t");
	valpr(val, DSP);
	printf("\n");
	dtp = rldt();
	printf("ldt%8t%R", *dtp & 0xffff);
	printf("%40tcr3%8t%R", val = cr3());
	printf("%64t");
	valpr(val, DSP);
	printf("\n");
	printf("taskreg%8t%R\n", rtaskreg());
#else
	(void) printf("\n");
#endif
	(void) print_dis(Reg_PC, 0);
}

/*
 * look at the procedure prolog of the current called procedure.
 * figure out which registers we saved, and where they are
 */

findregs(sp, addr)
	register struct stackpos *sp;
	register caddr_t addr;
{
	/* this is too messy to even think about right now */
	db_printf(9, "findregs: XXX not done yet!");
}

/*
 * printstack -- implements "$c" and "$C" commands:  print out the
 * stack frames.  printstack was moved from print.c due to heavy
 * machine-dependence.
 */

void
printstack(modif)
	int modif;
{
	int i, val, nargs, spa;
	caddr_t regp;
	char *name;
	struct stackpos pos;
	struct asym *savesym;
#ifdef KADB
	struct regs regs_trap;
#else
	struct allregs regs_trap;
#endif


	db_printf(4, "printstack: modif='%c'", modif);
	stacktop(&pos);
	if (hadaddress) {
#ifdef KADB
		pos.k_fp = address + 0xc;
#else
		if (kernel)
			pos.k_fp = address + 0xc;
		else
			pos.k_fp = address;
#endif
		pos.k_nargs = 0;
		pos.k_pc = MAXINT;
		pos.k_entry = MAXINT;
		db_printf(2, "printstack: pos.k_fp=%X, pos.k_nargs=0, "
				"pos.k_pc=MAXINT, pos.k_entry=MAXINT",
				address);
		/* sorry, we cannot find our registers without knowing our pc */
		for (i = 0; i < NREGISTERS; pos.k_regloc[i++] = 0)
			;
		findentry(&pos, 0);
	}
	while (count) {
		count--;
		chkerr();
		/* HACK */
		db_printf(2, "printstack: pos.k_pc %s MAXINT",
					(pos.k_pc == MAXINT) ? "==" : "!=");
		if (pos.k_pc == MAXINT) {
			val = 0;
			name = "?";
			pos.k_pc = 0;
		} else {
			val =  findsym(pos.k_pc, ISYM);
			db_printf(2, "printstack: cursym=%X", cursym);
			if (cursym)
				name = demangled(cursym);
			else
				name = "?";
		}

		if (modif == 'C')
			printf("%X ", pos.k_fp);
		printf("%s", name);
		if (pos.k_entry != MAXINT) {
			savesym = cursym;
			findsym(pos.k_entry, ISYM);
		}
		printf("(");
		regp = (caddr_t) pos.k_fp + FR_SAVFP + 4;
		db_printf(2, "printstack: regp=%X, pos.k_nargs=%D",
		    regp, pos.k_nargs);
		if (nargs = pos.k_nargs) {
			while (nargs--) {
				printf("%R", get(regp += 4, DSP));
				if (nargs)
					printf(",");
			}
		}
		if (val == MAXINT)
			printf(") at %X\n", pos.k_pc);
		else
			printf(") + %X\n", val);
		if (modif == 'C' && cursym) {
			register struct afield *f;

			errflg = NULL;
			for (i = cursym->s_fcnt, f = cursym->s_f;
							i > 0; i--, f++) {
				switch (f->f_type) {

				case N_PSYM:
					continue;

				case N_LSYM:
					val = get(pos.k_fp - f->f_offset, DSP);
					break;

				case N_RSYM:
					val = pos.k_regloc[f->f_offset];
					if (val == 0)
						errflg =
						"register location not known";
					else
						val = get(val, DSP);
				}
				printf("%8t%s:%10t", f->f_name);
				if (errflg != NULL) {
					printf("?\n");
					errflg = NULL;
					continue;
				}
				printf("%R\n", val);
			}
		}
#ifndef KADB
	if (kernel) {
#endif

		/*
		 * This code will try to handle all the cases that put us
		 * in the kernel.
		 * 1.) An exception (ie trap).
		 * 2.) A system call.
		 * 3.) A hardware interrupt.
		 * 4.) We started up Solaris from boot
		 */
		if (strcmp(name, "trap") == 0) {

			/*
			 * At this point k_fp points to the bp from locore
			 * which points to the regs structure.
			 */
			/* add of pointer to reg struct pointer */
			int Ltrapargs = pos.k_fp;
			int trapno; /* address of trapno on stack */
			int err;
			int type;
			char *typename;

			/*
			 * Get and display what caused the exception
			 */
			trapno = get(Ltrapargs, DSP) +
			((int) &regs_trap.r_trapno - (int) &regs_trap);
			type = get(trapno, DSP);

			err = get(Ltrapargs, DSP) +
			((int) &regs_trap.r_err - (int) &regs_trap);
			err = get(err, DSP);

			switch (type) {
			case 0: typename = "ZERODIV"; break;
			case 1: typename = "DEBUG"; break;
			case 2: typename = "NMI"; break;
			case 3: typename = "BRKPT"; break;
			case 4: typename = "OVFLW"; break;
			case 5: typename = "BOUND"; break;
			case 6: typename = "ILLINST"; break;
			case 7: typename = "NOCOPROC"; break;
			case 8: typename = "DBLFLT"; break;
			case 9: typename = "EXTOVRFLT"; break;
			case 10: typename = "TSS"; break;
			case 11: typename = "SEGFLT"; break;
			case 12: typename = "STKFLT"; break;
			case 13: typename = "PROT"; break;
			case 14: typename = "PGFLT"; break;
			case 16: typename = "COPROC"; break;
			case 512: typename = "INVALID"; break;
			case 513: typename = "UNUSEDINT"; break;
			case T_AST: typename = "AST"; break;
			default: typename = "unknown"; break;
			}
			printf("TRAP TYPE %X = %s; ", type, typename);
			printf("hardware error code %X address of regs %X\n",
						err, get(Ltrapargs, DSP));

			/*
			 * Now get the next stack frame from the regs struct
			 */
			pos.k_pc = get(get(Ltrapargs, DSP) +
				((int)&regs_trap.r_eip - (int)&regs_trap), DSP);
			pos.k_fp = get (get (Ltrapargs, DSP) +
				((int)&regs_trap.r_ebp - (int)&regs_trap), DSP);
			/*
			 * do not access user stack from possibly
			 * non-current context
			 */
			if (hadaddress &&
			    ((unsigned)(pos.k_fp - Ltrapargs) > 0x2000))
				break;
			pos.k_nargs = 0;
			pos.k_entry = MAXINT;
			for (i = 0; i < NREGISTERS; pos.k_regloc[i++] = 0)
				;
			findentry(&pos, 0);
		} else if (strcmp (name, "sys_call") == 0) {
			/*
			 * add of reg struct pointer
			 */
			int Ltrapargs = pos.k_fp;
			int type;

			if (hadaddress) /* do not access user stack from */
				break;	/* possibly non-current context */

			/*
			 * Display the system call #
			 */

			type = get (Ltrapargs +
				((int)&regs_trap.r_eax - (int)&regs_trap), DSP);
			printf ("SYSCALL %d - address of regs %X\n",
						type, Ltrapargs);

			/*
			 * Now get the next stack frame from the regs struct
			 */
			pos.k_pc = get (Ltrapargs +
				((int)&regs_trap.r_eip - (int)&regs_trap), DSP);
			pos.k_fp = get (Ltrapargs +
				((int)&regs_trap.r_ebp - (int)&regs_trap), DSP);
			pos.k_nargs = 0;
			pos.k_entry = MAXINT;
			for (i = 0; i < NREGISTERS; pos.k_regloc[i++] = 0)
				;
			findentry(&pos, 0);
		} else {
			int oldfp = pos.k_fp;
			int startint = lookup("cmnint")->s_value;
			int endint = lookup("cmntrap")->s_value;
			/*
			 * if this is main where are done tracing!
			 * main is top of stack
			 */
			if (strcmp(name, "main") == 0)
				break;

			/*
			 * Get the next stack frame.
			 * If frame pointer is null we are at the end of the
			 * stack.
			 */
			nextframe(&pos);
			if (pos.k_fp == 0 || errflg != NULL)
				break;

			/*
			 * We need to avoid the following test for stack
			 * switches if the current function is panicsys,
			 * because we switch stacks in vpanic() but no regs
			 * structure is saved due to an interrupt.
			 */
			if (strcmp(name, "panicsys") == 0)
				continue;

			/*
			 * Check if we are in locore interrupt entry code
			 * Or if there was a thread switch.
			 * Either way we have a regs structure to deal with
			 *
			 * Note that the fp delta check is a real hack!
			 * But there is not really a better way to detect
			 * a thread switch.
			 */
			if ((pos.k_pc >= startint && pos.k_pc <= endint) ||
					(pos.k_fp >= oldfp+0x2000 ||
					pos.k_fp <= oldfp-0x2000))
			{ /* add of reg struct */
				int Ltrapargs = pos.k_fp;
				if ((pos.k_pc >= startint &&
							pos.k_pc <= endint))
					printf("INTERRUPT - "
						"address of regs %X\n",
						Ltrapargs);
				else
#ifdef KADB
					break;
#else
					printf("STACK SWITCH "
						"address of regs %X\n",
						Ltrapargs);
#endif

				/*
				 * We do not print the locore routines in the
				 * Trace. So get the next stack frame.
				 */
				pos.k_pc = get(Ltrapargs +
						((int)&regs_trap.r_eip
						- (int)&regs_trap), DSP);
				pos.k_fp = get(Ltrapargs +
						((int)&regs_trap.r_ebp
						- (int)&regs_trap), DSP);
				/*
				 * do not access user stack from possibly
				 * non-current context
				 */
				if (hadaddress &&
				    ((unsigned)(pos.k_fp - Ltrapargs) > 0x2000))
					break;

				pos.k_nargs = 0;
				pos.k_entry = MAXINT;
				for (i = 0; i < NREGISTERS;
							pos.k_regloc[i++] = 0)
					;
				findentry(&pos, 0);
				if (errflg != NULL)
					break;
			}
		}
#ifndef KADB
	} else {
		/*
		 * For adb the stack trace is simple.
		 * look for the start of it all (ie main)
		 * or a null frame pointer.
		 */
		if (!strcmp(name, "main") || nextframe(&pos) == 0 ||
							errflg != NULL)
				break;
	}
#endif
#ifdef	KADB
		if (pos.k_fp == 0)
			break;
#endif /* KADB */
	}

}

#ifndef KADB
static
#endif /* !KADB */
valpr(val, type)
	int val, type;
{
	register off;

	db_printf(9, "valpr: val=%D, type=%D", val, type);
	off = findsym(val, type);
	if (off != val && off < maxoff) {
		printf("%s", demangled(cursym));
		if (off) {
			printf("+%R", off);
		}
	}
}

#ifndef KADB
/*
 * Print the FPU control word.
 */
static void
fpu_cw_print(const unsigned short cw, const int verbose)
{
	(void) printf("\ncw      0x%x", cw);
	if (cw && verbose)
		(void) printf(": %s%s%s%s%s%s%s%s%s%s",
				(cw & FPINV) ? "FPINV " : "",
				(cw & FPDNO) ? "FPDNO " : "",
				(cw & FPZDIV) ? "FPZDIV " : "",
				(cw & FPOVR) ? "FPOVR " : "",
				(cw & FPUNR) ? "FPUNR " : "",
				(cw & FPPRE) ? "FPPRE " : "",
				(cw & FPPC) ? "FPPC " : "",
				(cw & FPRC) ? "FPRC " : "",
				(cw & FPIC) ? "FPIC " : "",
				(cw & WFPDE) ? "WFPDE " : "");
}

/*
 * Print the FPU status word.
 */
static void
fpu_sw_print(const char *name, const unsigned short sw,
	unsigned short *const top, const int verbose)
{
	(void) printf("\n%s      0x%x", name, sw);
	*top = (int) (sw & FPS_TOP) >> 11;
	if (sw && verbose)
		(void) printf(": top=%d %s%s%s%s%s%s%s%s%s%s%s%s%s", *top,
					(sw & FPS_IE) ? "FPS_IE " : "",
					(sw & FPS_DE) ? "FPS_DE " : "",
					(sw & FPS_ZE) ? "FPS_ZE " : "",
					(sw & FPS_OE) ? "FPS_OE " : "",
					(sw & FPS_UE) ? "FPS_UE " : "",
					(sw & FPS_PE) ? "FPS_PE " : "",
					(sw & FPS_SF) ? "FPS_SF " : "",
					(sw & FPS_ES) ? "FPS_ES " : "",
					(sw & FPS_C0) ? "FPS_C0 " : "",
					(sw & FPS_C1) ? "FPS_C1 " : "",
					(sw & FPS_C2) ? "FPS_C2 " : "",
					(sw & FPS_C3) ? "FPS_C3 " : "",
					(sw & FPS_B) ? "FPS_B " : "");
}

/*
 * Print the indexed FPU data register.
 */
static void
fpreg_print(const struct _fpreg *fpreg, const int precision, const int index,
	const unsigned short top, const unsigned short tag)
{
	int decpt, sign;
	char buf[128], *bufp = buf;

	bufp = qecvt(*(long double *) fpreg, precision + 1, &decpt, &sign);

	db_printf(1, "sign=%D, decpt=%D, bufp=%s\n", sign, decpt, bufp);

	(void) printf(" st[%d]%8t%s%c", index, sign ? "-" : "+", *bufp++);
	if (!isdigit(*bufp))	/* in case of NaN or INF */
		(void) printf("%s", bufp);
	else if (*(bufp - 1) == '0') 		/* 0.0 */
		(void) printf(".0");
	else {
		register int last = strlen(bufp) - 1;

		/*
		 * Getting rid of the unnecessary trailing 0's.
		 */
		while (last && *(bufp + last) == '0') {
			*(bufp + last) = '\0';
			last--;
		}
		(void) printf(".%s", bufp);
		if (decpt -1)
			(void) printf(" e%D", decpt - 1);
	}
	switch (tag) {
	case 0:
		(void) printf("%50tVALID\n");
		break;
	case 1:
		(void) printf("%50tZERO\n");
		break;
	case 2:
		(void) printf("%50tSPECIAL\n");
		break;
	case 3:
		(void) printf("%50tEMPTY\n");
		break;
	default:
		error("fpreg_print: impossible tag value!");
	}
}

/*
 * Print the 8 FPU data registers, each of which is 10 bytes.
 */
static void
fpregs_print(const struct _fpreg *fpreg, const int precision,
	const unsigned short top, const unsigned short tag)
{
	register int i;
	static void fpreg_print(const struct _fpreg *, const int, const int,
				const unsigned short, const unsigned short);

	db_printf(1, "fpreg=%R, precision=%D, top=%D, tag=%D \n",
						fpreg, precision, top, tag);

	for (i = 0; i < 8; i++)
		fpreg_print(fpreg + i, precision, i, top,
					((int) tag >> ((i + top) % 8) * 2) & 3);
}
#endif

/*
 * Machine-dependent routine to handle $R $x $X commands.
 * called by printtrace (../common/print.c)
 */
fp_print(modif)
	int modif;
{
#ifndef KADB
#define	PRECISION 25

	unsigned short top;		/* top of FPU data register stack */
	register int i;
	int verbose = 0;		/* 0 for $x and 1 for $X */
	int precision = PRECISION;
	struct _fpstate fpstate;
	static void fpu_cw_print(const unsigned short, const int);
	static void fpu_sw_print(const char *, const unsigned short,
				unsigned short *const, const int);
	static void fpregs_print(const struct _fpreg *, const int,
				const unsigned short, const unsigned short);

#endif
	db_printf(4, "fp_print: modif='%c'", modif);

	switch (modif) {

#ifdef KADB
	/* No floating point code in KADB */
	case 'R':
	case 'x':
	case 'X':
		error("Not legal in kadb");
		return;
	}
#else !KADB
	case 'R':
		error("No 68881");		/* never a 68881 on i386 */
	case 'X':
		verbose = 1;
	case 'x':
		if (kernel)
			error("Not available with -k option.");
		if (hadcount) {
			if (count <= 0)
				error("Invalid count.");
			precision = count;
		}
		break;
	}
	switch (fpa_avail) {
	case FP_NO:
		error("No floating point support is present.");
	case FP_SW:
		(void) printf("80387 software emulator is present.\n"
							"fp_emul[]:\n");
		for (i = 0; i < 62; i += 6) {
			register int j;

			(void) printf("  [%3D]:", i * 4);
			for (j = 0; j < 6; j++) {
				(void) printf(" %8X",
				Prfpregs.fp_reg_set.fp_emul_space.fp_emul[i]);
				if ((i * 4) + j == 240) {
					(void) printf(" %-8x",
				Prfpregs.fp_reg_set.fp_emul_space.fp_emul[i]);
					break;
				}
			}
			(void) printf("\n");
		}
		(void) printf("fp_epad[]: %x\n",
			(short int) Prfpregs.fp_reg_set.fp_emul_space.fp_epad);
		return (0);
	case FP_287:
		(void) printf("80287 chip is present.");
		break;
	case FP_387:
		(void) printf("80387 chip is present.");
		break;
	case FP_486:
		(void) printf("80486 chip is present.");
		break;
	default:
		error("Unidentified floating point support.");
	}
	if (ptrace(PTRACE_GETFPREGS, pid, &Prfpregs, 0) == -1 && errno) {
		perror("fpu");
		db_printf(3, "fp_print: ptrace failed, errno=%D", errno);
		return (-1);
	}

	/*
	 * Printing the FPU chip saved state info.
	 */
	memcpy(&fpstate, &Prfpregs.fp_reg_set.fpchip_state, sizeof (fpstate));
	if (verbose) {
		unsigned short top;  /* just to be different from other 'top' */

		(void) printf("  (Re: <ieeefp.h> and <sys/fp.h>)");
		fpu_sw_print("status word at exception",
				(unsigned short) fpstate.status, &top, verbose);
		printc('\n');
	}
	fpu_cw_print((unsigned short) fpstate.cw, verbose);
	fpu_sw_print("sw", (unsigned short) fpstate.sw, &top, verbose);
	(void) printf("\ncssel 0x%x  ipoff 0x%x%20t"
					"datasel 0x%x  dataoff 0x%x\n\n",
					fpstate.cssel, fpstate.ipoff,
					fpstate.datasel, fpstate.dataoff);
	fpregs_print(fpstate._st, precision, top, (unsigned short) fpstate.tag);
	return (0);
#endif !KADB
}
