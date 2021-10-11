/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printsr.c	1.16	99/10/05 SMI"

/*
 * adb - sparc-specific print routines
 *	printregs
 *	printstack
 *		get_nrparms
 *		printargs
 *		printlocals
 *	fp_print
 */

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/stack.h>

#include "adb.h"
#include "sparc.h"
#include "symtab.h"
#include "allregs.h"

#include "sr_instruction.h"

int nprflag;
extern int v9flag;
static void printargs();
extern	int	_printf();
#ifndef KADB
#include <strings.h>
#include <stdio.h>
#endif

static int firstword();
static int calltarget();
static int 	ijmpreg();
static int 	jmpcall();
static int	splitjmp();

extern unsigned long getLong(addr_t, int, int);
extern long readsavedreg(struct stackpos *, int);
extern struct asym *trampsym;				/* see setupsr.c */

void
printregs()
{
	register int i;
	long val;
	unsigned off;
	int cp;

	/* Sparc has too many registers.  Print two columns. */
	for (i = 0; i <= LAST_NREG; i++) {
#if	!defined(KADB)
		/* adb can't get even get at these registers */
		if (i == Reg_WIM || i == Reg_TBR)
			continue;
#else
		if ((i == Reg_WIM) && v9flag) {
			printf("winreg: cur:%1d other:%1d clean:%1d ",
			    readreg(Reg_CWP),
			    readreg(Reg_OTHERWIN),
			    readreg(Reg_CLEANWIN));
			printf("cansave:%1d canrest:%1d wstate:%1d\n",
			    readreg(Reg_CANSAVE),
			    readreg(Reg_CANRESTORE),
			    readreg(Reg_WSTATE));
			continue;
		} else if ((i == Reg_TBR) && v9flag) {
			printf("tba%6t%R\n", (u_int) readreg(Reg_TBA));
			continue;
		}
#endif	/* !defined(KADB) */
		if ((i == Reg_PSR) && v9flag) {
			u_longlong_t t = readreg(Reg_TSTATE);
			u_longlong_t p;

			p = (t >> TSTATE_PSTATE_SHIFT & TSTATE_PSTATE_MASK);
			printf("tstate: %J", t);
			printf("  (ccr=0x%x, asi=0x%x, pstate=0x%x, cwp=0x%x)",
			    (u_int)((t >> TSTATE_CCR_SHIFT) & TSTATE_CCR_MASK),
			    (u_int)((t >> TSTATE_ASI_SHIFT) & TSTATE_ASI_MASK),
			    (u_int)p, (u_int)((t & TSTATE_CWP)));
			printf("\npstate: ag:%1d ie:%1d priv:%1d am:%1d ",
			    p & PSTATE_AG ? 1 : 0, p & PSTATE_IE ? 1 : 0,
			    p & PSTATE_PRIV ? 1 : 0, p & PSTATE_AM ? 1 : 0);
			printf("pef:%1d mm:%1d tle:%1d cle:%1d mg:%1d ig:%1d\n",
			    p & PSTATE_PEF ? 1 : 0, p & PSTATE_MM ? 1 : 0,
			    p & PSTATE_TLE ? 1 : 0, p & PSTATE_CLE ? 1 : 0,
			    p & PSTATE_MG ? 1 : 0, p & PSTATE_IG ? 1 : 0);
			continue;
		}
		val = readreg(i);
		printf("%s%6t%J ", regnames[i], val);

		if (i == Reg_PC || i == Reg_NPC) {
			if (val) {
				++nprflag;	/* avoid errmsg if invalid PC */
				print_dis(i, 0);
				nprflag = 0;
			} else {
				printc('\n');
			}
			continue;
		}

		/*
		 * Don't print symbols for regs that can't be addresses.
		 */
		if (i != Reg_PSR && i != Reg_WIM && i != Reg_TBR) {
			off = (unsigned)findsym((long)val, ISYM);
			if (off < maxoff && (u_int)cursym->s_value >=
				(u_int)txtmap.map_head->mpr_b) {
				if (i < Reg_L0) {
					if ((cp = charpos()) < 20)
						printf("%M", 20 - cp);
				} else if (i <= Reg_I7) {
					if ((cp = charpos()) < 60)
						printf("%M", 60 - cp);
				}
				printf("%s", demangled(cursym));
				if (off) {
					printf("+%R", off);
				}
				if (i < Reg_L0)
					printf(" ");
			}
		}
		if (i < Reg_L0) {
			/* go to column 40 */
			if ((cp = charpos()) < 40)
				printf("%M", 40 - cp);
			else
				printf("%8t");
			i += Reg_L0 - Reg_G0 -1;
		} else if (i < Reg_I7) {
			i -= Reg_L0 - Reg_G0;
			printf("\n");
		} else {
			printf("\n");
		}
	}
}

/*
 * printstack -- implements "$c" and "$C" commands:  print out the
 * stack frames.  printstack was moved from print.c due to heavy
 * machine-dependence.
 *
 * Since there are no hints in the code on a sparc as to the number
 * of parameters that were really passed,  printstack on a sparc
 * allows the user to specify a (hex) number following the c in a
 * $c command.  That number will tell how many parameters $c displays
 * for each routine (e.g., $c22).  We allow only two hex digits there.
 */
void
printstack(modif)
	int modif;
{
	int val = 0; /* was an un-initalised memory read, giving bad results */
	char *name;
	struct stackpos pos;

	int def_nargs = -1;

	/*
	 * Read the nr-of-parameters kluge command, if present.
	 * Up to two hex digits are allowed.
	 */
	if (nextchar()) {
		def_nargs = get_nrparms();
	}

	stacktop(&pos);
	db_printf(2, "Printstack after stacktop\n");
	if (hadaddress) {
		pos.k_fp = address;
		pos.k_nargs = 0;
		pos.k_pc = MAXINT;
		pos.k_entry = MAXINT;
		pos.k_flags |= K_ONSTACK;
		findentry(&pos);
	}

	db_printf(2, "Printstack after findentry\n");
	while (count) {		/* loop once per stack frame */
		count--;
		chkerr();
		/* HACK */
		if (pos.k_pc == MAXINT) {
			name = "?";
			pos.k_pc = 0;
		} else {
			val =  findsym((long)pos.k_pc, ISYM);
			if (cursym && strcmp(cursym->s_name, "start") == 0)
				break;
			if (cursym)
				name = demangled(cursym);
			else
				name = "?";
		}
		printf("%s", name);

		db_printf(2, " about to call printargs \n");

		/*
		 * Print out this procedure's arguments
		 */
		printargs(def_nargs, val, &pos, modif);

		if (nextframe(&pos) == 0 || errflg)
			break;
	} /* end while (looping once per stack frame) */

} /* end printstack */


/*
 * Get the (optional) number following a "$c", which tells
 * how many parameters adb should pretend were passed into
 * each routine in the stack trace.  Zero, one, or two hex
 * digits is/are allowed.
 */
int
get_nrparms() {
	int na = 0;

	if (isxdigit(lastc)) {
		na = convdig(lastc);
		if (nextchar()) {
			if (isxdigit(lastc))
				na = na * 16 + convdig(lastc);
		}
	}
	return (na);
}

/*
 * Print out procedure arguments
 *
 * There are two places to look -- where the kernel has stored the
 * current value of registers i0-i5, or where the callee may have
 * dumped those register arguments in order to take their addresses
 * or whatever.  Since optimized routines (usually) never do that,
 * we'll opt for the i-regs.
 */
static void
printargs(int def_nargs, int val, struct stackpos *ppos, int modif)
{
	addr_t argptr, argend, callers_fp;
	int anr, nargs;
	struct stackpos npos;

	unsigned argsize;
	unsigned long regval;


	printf("(");

	argend = 0;
	callers_fp = 0;
	npos = *ppos;
	if (nextframe(&npos) != 0) {
		if (errflg) {
			errflg = 0;	/* don't ask, don't tell */
		} else {
			callers_fp = npos.k_fp;
			argend = readsavedreg(&npos, Reg_FP);
			if (IS_V9STACK(argend))
				argend += V9BIAS64;
		}
	}


	db_printf(1, "printargs: callers_fp %J argend %J\n",
			callers_fp, argend);

	if (def_nargs < 0 || ppos->k_flags & K_CALLTRAMP ||
		    callers_fp == 0) {
		nargs = ppos->k_nargs;
	} else {
		nargs = def_nargs;
	}


	if (nargs) {
		extern int max_nargs;

		/* Calculate the address of the first stack arg */
		argptr = callers_fp;
		if (IS_V9STACK(argptr)) {
			/* 64-bit frame */
			argptr += V9BIAS64 + ARGPUSH64;
			argsize = 8;
		} else {
			/* 32-bit frame */
			argptr += ARGPUSH32;
			argsize = 4;
		}


		nargs = MIN(nargs, max_nargs);
		for (anr = 0; anr < nargs; ++anr) {
			if (anr < NARG_REGS) {
				regval = readsavedreg(ppos, Reg_I0 + anr);
			} else {
				/* Don't print past the caller's frame */
				if (argend && argptr >= argend)
					break;

				regval = getLong(argptr, DSP, argsize == 8);
				argptr += argsize;
			}

			if (anr) printf(",");

			printf("%J", regval);
		}
	}

	if (val == MAXINT)
		printf(") at %J\n", ppos->k_pc);
	else if (val)
		printf(") + %X\n", val);
	else
		printf(")\n");


	if (modif == 'C')
		printf("\t[savfp=0x%J,savpc=0x%X]\n",
			callers_fp, readsavedreg(ppos, Reg_I7));
}


/*
 * Machine-dependent routine to handle $R $x $X commands.
 * called by printtrace (../common/print.c)
 */
fp_print(modif) {
	switch (modif) {
	case 'R':
		error("no 68881");	/* never a 68881 on a sparc */
		break;
#if	defined(KADB) && !defined(sun4u)
	/* No floating point code in (non-sun4u) KADB */
	case 'x':
	case 'X':
	case 'y':	/* XXX - allow for Fusion: bcopy() uses FP regs */
	case 'Y':
		error("no fpa");
		break;
#else	/* KADB */
	case 'x':
		printfpuregs(Reg_F0);
		break;
	case 'X':
		printfpuregs(Reg_F16);
		break;
	case 'y':
		printfpuregs(Reg_F32);
		break;
	case 'Y':
		printfpuregs(Reg_F48);
		break;
#endif	/* KADB */
	default:
		error("fp_print: bad option");
		break;
	}
	return (0);
}


/*
 *  Read a register saved in the stack frame indicated by <*pos>.
 *  Our caller has determined that this call frame has been flushed
 *  to the stack.  Our caller must also ascertain that
 *      Reg_L0 <= reg <= Reg_I7
 */
long
readstackreg(struct stackpos *pos, int reg)
{
	addr_t addr;

	db_printf(1, "readstackreg:  fp=%J reg=%d\n", pos->k_fp, reg);


	addr = pos->k_fp;
	if (IS_V9STACK(addr)) {
		/* 64-bit call frame */
		addr += V9BIAS64 + (reg - Reg_L0) * 8;
	} else {
		/* 32-bit call frame */
		addr += (reg - Reg_L0) * 4;
	}

	return (getLong(addr, DSP, IS_V9STACK(pos->k_fp)));
}


unsigned long
getLong(addr_t addr, int space, int low_word_adj)
{
	unsigned long val;

	/*
	 *  If low_word_adj is 0, that means we are actually looking
	 *  at a v8 stack, so just return the word instead of long
	 *  pointed to by addr.
	 */
	val = (unsigned)get(addr, space);
	if (low_word_adj) {
	    val = (val << 32) + (unsigned)get(addr+4, space);
	}
	return (val);
}



#define	CALL_INDIRECT -1 /* flag from calltarget for an indirect call */

/*
 * The "save" is typically the third instruction in a routine.
 * SAVE_INS is set to the farthest (in bytes!, EXclusive) we should reasonably
 * look for it.  "xlimit" (if it's between procaddr and procaddr+SAVE_INS)
 * is an overriding upper limit (again, exclusive) -- i.e., it is the
 * address after the last instruction we should check.
 */
#define	SAVE_INS (5*4)

static int
has_save(procaddr, xlimit)
	addr_t procaddr, xlimit;
{
	char *asc_instr, *disassemble();

	if (procaddr > xlimit || xlimit > procaddr + SAVE_INS) {
		xlimit = procaddr + SAVE_INS;
	}

	/*
	 * Find the first three instructions of the current proc:
	 * If none is a SAVE, then we are in a leaf proc and will
	 * have trouble finding caller's PC.
	 */
	db_printf(8, "has_save procaddr %J\n", procaddr);
	while (procaddr < xlimit) {
		asc_instr = disassemble(get(procaddr, ISP), procaddr);

		if (firstword(asc_instr, "save")) {
			return (1);	/* yep, there's a save */
		}

		procaddr += 4;	/* next instruction */
	db_printf(8, "has_save procaddr+4 %J\n", procaddr);
	}

	return (0);	/* nope, there's no save */
} /* end has_save */



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
 * leaf proc.   (Note that for tracing purposes, we are considered to be
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
is_leaf_proc(spos, cur_o7)
	register struct stackpos *spos;
	addr_t cur_o7;		/* caller's PC if leaf proc (rtn addr) */
				/* invalid otherwise */
{
	addr_t	upc,		/* user's program counter */
		sv_i7,		/* if leaf, caller's PC ... */
				/* else, caller's caller's PC */
		cto7,		/* call target of call at cur_o7 */
				/* (if leaf, our entry point) */
		cti7,		/* call target of call at sv_i7 */
				/* (if leaf, caller's entry point) */
		near_sym;	/* nearest symbol below PC; we hope it ... */
				/* ... is the address of the proc we're IN */
	int offset;
	char *saveflg = errflg;


	errflg = 0;
	upc = spos->k_pc;


	offset = findsym((long)upc, ISYM);
	if (offset == MAXINT) {
		near_sym = (addr_t)-1;
	} else {
		near_sym = cursym->s_value;

		/*
		 * has_save will look at the first four instructions
		 * at near_sym, but not past upc.
		 */
		if (has_save(near_sym, upc)) {
			/* Not a leaf proc.  This is the most common case. */
			return (0);
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
	cto7 = calltarget(cur_o7);
	if (cto7 == 0)
		return (0);		/* nope */

	/*
	 * Is that call near (but less than) where the pc is?
	 * If it's indirect, skip these two checks.
	 */
	db_printf(1, "Is_leaf_proc cur_o7 %X, cto7 %X\n", cur_o7, cto7);

	if (cto7 == (addr_t)CALL_INDIRECT) {
		if (near_sym != (addr_t)-1) {
			cto7 = near_sym;	/* best guess */
		} else {
			errflg = "Cannot trace stack";
			return (0);
		}
	} else {
		(void) get(cto7, ISP);	/* is the address ok? */
		if (errflg || cto7 > upc) {
			errflg = saveflg;
			return (0);	/* nope */
		}

		/*
		 * Is the caller's call near that call?
		 */
		sv_i7 = readsavedreg(spos, Reg_I7);
		cti7 = calltarget(sv_i7);

		db_printf(1, "Is_leaf_proc caller's call sv_i7 %X, cti7 %X\n",
		    sv_i7, cti7);

		if (cti7 != (addr_t)CALL_INDIRECT) {
			if (cti7 == 0)
				return (0);
			(void) get(cti7, ISP);	/* is the address ok? */
			if (errflg || cti7 > cur_o7) {
				errflg = saveflg;
				return (0);	/* nope */
			}
		}

		/*
		 * check for a SAVE instruction
		 */
		if (has_save(cto7, (addr_t)0)) {
			/* not a leaf. */
			return (0);
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
	trampcheck(spos);
#endif !KADB

	return (1);

} /* end is_leaf_proc */


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
stacktop(struct stackpos *spos)
{
	int leaf;
	char *saveflg;

	spos->k_fp = readreg(Reg_SP);
	spos->k_pc = readreg(Reg_PC);
	spos->k_level = 0;
	spos->k_flags = 0;
	saveflg = errflg;
	db_printf(8, "In stacktop %J %J\n", spos->k_fp, spos->k_pc);

	/*
	 * If we've stopped in a routine but before it has done its
	 * "save" instruction, is_leaf_proc will tell us a little white
	 * lie about it, calling it a leaf proc.
	 */
	leaf = is_leaf_proc(spos, (addr_t)readreg(Reg_O7));
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

	if (leaf) {

		if ((spos->k_flags & K_SIGTRAMP) == 0)
		    spos->k_nargs = 0;

	} else {
		findentry(spos);
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

void
findentry(struct stackpos *spos)
{
	char *saveflg = errflg;
	long offset;

	errflg = 0;

	spos->k_caller = readsavedreg(spos, Reg_I7);
	db_printf(8, "find_entry caller %J\n", spos->k_caller);
	if (errflg == 0) {
		spos->k_entry = calltarget(spos->k_caller);
	}


	if (errflg == 0) {
	    db_printf(1, "findentry:  caller 0x%X, entry 0x%X",
		spos->k_caller, spos->k_entry);
	} else {
	    db_printf(1, "findentry:  caller 0x%X, errflg %s",
		spos->k_caller, errflg);
	}

	if (errflg || spos->k_entry == 0) {
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

	if (spos->k_entry == CALL_INDIRECT) {
		offset = findsym((long)spos->k_pc, ISYM);
		if (offset != MAXINT) {
			spos->k_entry = cursym->s_value;
		} else {
			spos->k_entry = MAXINT;
		}
#ifndef KADB
		trampcheck(spos);
#endif !KADB
	}

}

static
firstword(ofthis, isthis)
	char *ofthis, *isthis;
{
	char *ws;

	while (*ofthis == ' ' || *ofthis == '\t')
		++ofthis;
	ws = ofthis;
	while (*ofthis && *ofthis != ' ' && *ofthis != '\t')
		++ofthis;

	*ofthis = 0;
	return (strcmp(ws, isthis) == 0);
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
calltarget(calladdr)
	addr_t calladdr;
{
	char *saveflg = errflg;
	int instr;
	int offset;
	addr_t ct;

	errflg = 0;
	instr = (int)get(calladdr, ISP);
	if (errflg) {
		errflg = saveflg;
		return (0);	/* no "there" there */
	}
	db_printf(8, "calltarget %X %J", instr, calladdr);

	if (X_OP(instr) == SR_CALL_OP) {
		/* Normal CALL instruction */
		offset = SR_WA2BA(instr);
		ct = (addr_t)offset + calladdr;

		/*
		 * If the target of that call (ct) is an indirect jump
		 * through a register, then say so.
		 */
		instr = get((addr_t)ct, ISP);
		db_printf(8, "call_target ct is %J %X\n", ct, instr);
		if (errflg) {
			errflg = saveflg;
			return (0);
		}

		if (ijmpreg(instr))
			return (CALL_INDIRECT);
		else
			return (ct);
	}

	/*
	 * Our caller wasn't a call.  Was it a jmp?
	 */
	return (jmpcall(instr));
}


static struct {
	int op, rd, op3, rs1, imm, rs2, simm13;
} jmp_fields;

static
ijmpreg(instr) int instr; {
	if (splitjmp(instr)) {
		return (jmp_fields.imm == 0 || jmp_fields.rs1 != 0);
	} else {
		return (0);
	}
}

static
jmpcall(int instr)
{
	if (splitjmp(instr) == 0 ||	/* Give up if it ain't a jump, */
	    jmp_fields.rd != Reg_O7) {	/* or it doesn't save pc into %o7 */
		return (CALL_INDIRECT);	/* ... a useful white lie */
	}

	/*
	 * It is a jump that saves pc into %o7.  Find its target, if we can.
	 */
	if (jmp_fields.imm == 0 || jmp_fields.rs1 != 0)
		return (CALL_INDIRECT);	/* can't find target */

	/*
	 * The target is simm13, sign extended, not even pc-relative.
	 * So sign-extend and return it.
	 */
	return (SR_SEX13(instr));
}

static
splitjmp(instr) int instr; {

	jmp_fields.op	  = X_OP(instr);
	jmp_fields.rd	  = X_RD(instr);
	jmp_fields.op3	  = X_OP3(instr);
	jmp_fields.rs1	  = X_RS1(instr);
	jmp_fields.imm	  = X_IMM(instr);
	jmp_fields.rs2	  = X_RS2(instr);
	jmp_fields.simm13 = X_SIMM13(instr);

	if (jmp_fields.op == SR_FMT3a_OP)
		return (jmp_fields.op3 == SR_JUMP_OP);
	else
		return (0);
}


/*
 * nextframe replaces the info in spos with the info appropriate
 * for the next frame "up" the stack (the current routine's caller).
 *
 * Called from printstack (printsr.c) and qualified (expr.c).
 */
int
nextframe(struct stackpos *spos)
{
#ifndef KADB
	if (spos->k_flags & K_CALLTRAMP) {
		trampnext(spos);
		errflg = 0;
	} else
#endif !KADB
	{
		/* find caller's pc and fp */
		spos->k_pc = spos->k_caller;

		if ((spos->k_flags & (K_LEAF|K_SIGTRAMP)) == 0) {
			spos->k_fp = readsavedreg(spos, Reg_FP);
			spos->k_level++;
		}
		/* else (if it is a leaf or SIGTRAMP), don't change fp. */

		/*
		 * now we have assumed the identity of our caller.
		 */
		if (spos->k_flags & K_SIGTRAMP) {
		    /* Preserve K_LEAF */
		    spos->k_flags = (spos->k_flags | K_TRAMPED) & ~K_SIGTRAMP;
		} else {
		    spos->k_flags &= ~K_LEAF;
		}
		findentry(spos);
	}
	db_printf(1, "nextframe returning %X",  spos->k_fp);
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
trampcheck(spos) register struct stackpos *spos; {

	if (trampsym == 0)
		return;

	findsym((long)spos->k_caller, ISYM);

	if (cursym != trampsym)
		return;

	spos->k_flags |= K_CALLTRAMP;
	spos->k_nargs = 3;
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
find_ucp(addr_t kfp, int was_leaf)
{
	addr_t ucp, wf;
	int offset_adj, low_word_adj;

	offset_adj = ((kfp % 2)?2:1);
	low_word_adj = ((kfp % 2)?4:0);

	wf = was_leaf ? kfp
			: (addr_t)getLong(kfp + FR_IREG(Reg_FP)*offset_adj,
					    DSP, low_word_adj);
	ucp = get(wf + 40, DSP);
	return ((ucontext_t *)ucp);
}

void
trampnext(struct stackpos *spos)
{
	int was_leaf;
	struct stackpos tsp;
	addr_t maybe_o7;
	ucontext_t *ucp; /* address in the subprocess.  Don't dereference */

	int offset_adj;
	int low_word_adj;

	offset_adj = ((spos->k_fp % 2)?2:1);
	low_word_adj = ((spos->k_fp % 2)?4:0);


	/*
	 * The easy part -- set up the spos for _sighandler itself.
	 */
	spos->k_pc = spos->k_caller;
	spos->k_entry = trampsym->s_value;
	was_leaf = (spos->k_flags & K_LEAF);
	spos->k_flags = K_SIGTRAMP;
	spos->k_nargs = 0;

	/*
	 * The hard part -- set up the spos to enable it to find
	 * sighandler's caller.  Need to know whether it was a leaf.
	 */
	ucp = find_ucp(spos->k_fp, was_leaf);
	tsp = *spos;

	maybe_o7 = getLong(spos->k_fp + FR_SAVPC*offset_adj, DSP, low_word_adj);
	tsp.k_pc = get((addr_t)&ucp->uc_mcontext.gregs[REG_PC], DSP);

	/* set K_LEAF for use in printstack */
	if (is_leaf_proc(&tsp, maybe_o7)) {
	    db_printf(1, "trampnext thinks it's a leaf proc.\n");
	    spos->k_flags |= K_LEAF;
	} else {
	    db_printf(1, "trampnext thinks it's not a leaf proc.\n");
	}

	spos->k_fp = get((addr_t)&ucp->uc_mcontext.gregs[REG_SP], DSP);
	spos->k_caller = get((addr_t)&ucp->uc_mcontext.gregs[REG_PC], DSP);

} /* end trampnext */

#endif /* (not KADB) */
