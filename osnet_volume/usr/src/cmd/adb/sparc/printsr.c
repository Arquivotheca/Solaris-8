/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)printsr.c	1.26	98/08/21 SMI"

/*
 * adb - sparc-specific print routines
 *	printregs
 *	printstack
 *		get_nrparms
 *		printargs
 *		printlocals
 *	fp_print
 */

#include "adb.h"
#include "symtab.h"
#include "allregs.h"

int nprflag;
extern int v9flag;
extern struct allregs_v9 adb_regs_v9;
static void printargs();

void
printregs()
{
	register int i;
	int val, off, cp;
	struct allregs_v9 *a = &adb_regs_v9;

	/* Sparc has too many registers.  Print two columns. */
	for (i = 0; i <= LAST_NREG; ++i ) {
#if	!defined(KADB)
                /* adb can't get even get at these registers */
                if (( i == Reg_WIM ) || ( i == Reg_TBR ))
                        continue;
#else
		if ((i == Reg_WIM) && v9flag) {
			printf("winreg: cur:%1d other:%1d clean:%1d ",
			    a->r_cwp, a->r_otherwin, a->r_cleanwin);
			printf("cansave:%1d canrest:%1d wstate:%1d\n",
			    a->r_cansave, a->r_canrestore, a->r_wstate);
			continue;
		} else if ((i == Reg_TBR) && v9flag) {
			printf("tba%6t%R\n", a->r_tba);
			continue;
		}
#endif	/* !defined(KADB) */
		if ((i == Reg_PSR) && v9flag) {
			u_longlong_t t = a->r_tstate;
			u_longlong_t p;

			p = (t >> TSTATE_PSTATE_SHIFT & TSTATE_PSTATE_MASK);
			printf("tstate: %J", a->r_tstate);
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
		printf("%s%6t%R ", regnames[i], val);

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
			off = findsym(val, DSP);
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
			if( (cp = charpos() ) < 40 )
				printf( "%M", 40 - cp );
			else
				printf( "%8t" );
			i += Reg_L0 - Reg_G0 -1;
		} else if( i < Reg_I7 ) {
			i -= Reg_L0 - Reg_G0 ;
			printf( "\n" );
		} else {
			printf( "\n" );
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
	int i;
	int val = 0; /* was an un-initalised memory read, giving bad results */
	char *name;
	struct stackpos pos;

	int def_nargs = -1;
	
	/*
	 * Read the nr-of-parameters kluge command, if present.
	 * Up to two hex digits are allowed.
	 */
	if( nextchar( ) ) {
		def_nargs= get_nrparms( );
	}

	stacktop(&pos);
	if (hadaddress) {
		pos.k_fp = address;
		pos.k_nargs = 0;
		pos.k_pc = MAXINT;
		pos.k_entry = MAXINT;
		/* sorry, we cannot find our registers without knowing our pc */
		for (i = FIRST_STK_REG; i <= LAST_STK_REG; i++)
			pos.k_regloc[ REG_RN(i) ] = 0;
		findentry(&pos, 0);

	}
	while (count) {		/* loop once per stack frame */
		count--;
		chkerr();
		/* HACK */
		if (pos.k_pc == MAXINT) {
			name = "?";
			pos.k_pc = 0;
		}
		else {
			val =  findsym(pos.k_pc, ISYM);
			if (cursym && !strcmp(cursym->s_name, "start"))
				break;
			if (cursym)
				name = demangled(cursym);
			else
				name = "?";
		}
		printf("%s", name);

		/*
		 * Print out this procedure's arguments
		 */
		printargs( def_nargs, val, &pos, modif );

#ifdef	NOTDEF	/* Current compiler doesn't do "-go". */
		if (modif == 'C' && cursym) {
			printlocals( &pos );
		}
#endif	/* NOTDEF */

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
get_nrparms ( ) {
  int na = 0;

	if( isxdigit(lastc) ) {
		na = convdig(lastc);
		if( nextchar() ) {
			if( isxdigit(lastc) )
				na = na * 16 + convdig(lastc);
		}
	}
	return na;
}


/*
 * Print out procedure arguments
 *
 * There are two places to look -- where the kernel has stored the
 * current value of registers i0-i5, or where the callee may have
 * dumped those register arguments in order to take their addresses
 * or whatever.  Since optimized routines never do that, we'll opt
 * for the i-regs.
 */
static void
printargs ( def_nargs, val, ppos, modif ) struct stackpos *ppos; {
  addr_t regp, callers_fp, ccall_fp;
  int anr, nargs;

	printf("(");

	/* Stack frame loc of where reg i0 is stored */
	regp = ppos->k_fp + FR_I0 ;
	callers_fp = get( ppos->k_fp + FR_SAVFP, DSP );
	if( callers_fp )
		ccall_fp = get( callers_fp + FR_SAVFP, DSP );
	else
		ccall_fp = 0;
	db_printf(1, "printargs:  caller's fp %X; ccall_fp %X",
		callers_fp, ccall_fp );

	if( def_nargs < 0  ||  ppos->k_flags & K_CALLTRAMP
		|| callers_fp == 0 ) {
		nargs = ppos->k_nargs;
	} else {
		nargs = def_nargs;
	}
	
	if ( nargs ) {
		extern int max_nargs;

		nargs = MIN(nargs, max_nargs);
		for( anr=0; anr < nargs; ++anr ) {
			if( anr == NARG_REGS ) {
				/* Reset regp for >6 (register) args */
				regp = callers_fp + FR_XTRARGS ;
				db_printf(1, "printargs:  xtrargs %X", regp );
			}
			printf("%R", get(regp, DSP));
			regp += 4;

			/* Don't print past the caller's FP */
			if( ccall_fp  &&  regp >= ccall_fp )
				break;

			if( anr < nargs-1 ) printf(",");
		}
	}

	if (val == MAXINT)
		printf(") at %X\n", ppos->k_pc );
	else
		if ( val ) 
			printf(") + %X\n", val);
		else
			printf(")\n");

	if (modif == 'C')
		printf("\t[savfp=0x%X,savpc=0x%X]\n", callers_fp,
			get(ppos->k_fp + FR_SAVPC, DSP));
}

#ifdef	NOTDEF		/* Current compiler doesn't do '-go". */
printlocals ( spos )
	struct stackpos *spos;
{
	register struct afield *f;
	int i, val;

	errflg = 0;
	for (i = cursym->s_fcnt, f = cursym->s_f; i > 0; i--, f++) {
		switch (f->f_type) {

		case N_PSYM:
			continue;

		case N_LSYM:
			val = get(spos->k_fp - f->f_offset, DSP);
			break;

		case N_RSYM:
			val = spos->k_regloc[ REG_RN( f->f_offset ) ];
			if (val == 0)
				errflg = "reg location !known";
			else
				val = get(val, DSP);
		}
		printf("%8t%s:%10t", f->f_name);
		if (errflg) {
			printf("?\n");
			errflg = 0;
			continue;
		}
		printf("%R\n", val);
	}
} /* end printlocals */
#endif	/* NOTDEF */

/*
 * Machine-dependent routine to handle $R $x $X commands.
 * called by printtrace (../common/print.c)
 */
fp_print ( modif ) {
	switch( modif ) {
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
