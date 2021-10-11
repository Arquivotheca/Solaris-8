/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)format.c	1.54	99/05/04 SMI"

/*
 * adb - output formatting
 */

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "adb.h"
#include "symtab.h"
#include "fio.h"
#include "fpascii.h"

static void each_switch();

#ifdef _LP64
extern int	elf64mode;
#endif

void
scanform(icount, ifp, itype, ptype)
	int icount;
	char *ifp;
	int itype, ptype;
{
	char *fp, modifier;
	int fcount, exact;
#ifdef _LP64
	addr_t savdot;
#else
	int savdot;
#endif

	db_printf(6, "scanform: icount=%D, ifp=%X, itype=%D, ptype=%D",
						icount, ifp, itype, ptype);
	while (icount) {
		fp = ifp;
		savdot = dot;

		if (itype != SSP && var[PSYMVAR] == 0) {
			exact = findsym(dot, ptype) == 0;
			if (exact && maxoff && fp &&
				!((strncmp(fp, "ia", 2) == 0) ||
				(strncmp(fp, "ai", 2) == 0))) {
				if (charpos() != 0)
					printf("\n");
#if defined(KADB) && defined(i386)
				printf("%s:\n", demangled(cursym));
#else
				printf("%s:%16t", demangled(cursym));
#endif
			}
		}

		while (*fp && errflg == 0) {
			if (isdigit(modifier = *fp)) {
				fcount = 0;
				while (isdigit(modifier = *fp++)) {
					fcount *= 10;
					fcount += modifier-'0';
				}
				fp--;
			} else
				fcount = 1;
			if (*fp == 0)
				break;
			if (charpos() != 0 && strncmp(fp, "ai", 2) == 0)
				printf("\n");
			fp = exform(fcount, fp, itype, ptype);
		}

		dotinc = (int)(dot - savdot);
		dot = savdot;

		if (errflg) {
			if (icount < 0) {
				errflg = 0;
				break;
			}
			error(errflg);
		}

		if (--icount)
			dot = inkdot(dotinc);
		if (interrupted)
			error((char *)0);
	}
} /* end scanform */


/*
 * exform, eachform and each_switch together interpret the
 * given format string "ifp" for "fcount" times;
 */

/* exform_break allows eachform to tell exform to break its while loop */
static char exform_break;

/*
 * exform_first allows exform to tell each_switch whether it's
 * the first time around exform's loop.
 */
static char exform_first;

char *
exform(fcount, ifp, itype, ptype)
	int fcount;
	char *ifp;
	int itype, ptype;
{
	char *eachform();
	char *fp;

	db_printf(6, "exform: fcount=%D, ifp=%X, itype=%D, ptype=%D",
						fcount, ifp, itype, ptype);
	if (fcount <= 0) {
		/*
		 * stupid hack to make zero repeat
		 * count do something reasonable
		 */
		fp = ifp;
		if (*fp == '"') {
			/*
			 * scan out quoted string
			 * idiot really deserves a core dump here...
			 */
			fp++;
			while (*fp != '"' && *fp)
				fp++;
		}
		if (*fp)
			fp++;
		return (fp);
	}

	exform_first = 1;
	while (fcount > 0) {

		fp = eachform(fcount, ifp, itype, ptype);
		if (exform_break)
			break;

		exform_first = 0;

		if (itype != NSP)
			dot = inkdot(dotinc);
		fcount--;
		endline();
	}
	return (fp);
} /* end of exform */


/*
 * eachform is the guts of exform's for loop.  It contains a large
 * switch which handles the blank/tab and "tT+-IAaiz" modifiers
 * (except i and z where itype isn't SSP).  Note that while most
 * of the branches simply return fp, some "ret_break", i.e., return
 * to exform AND break out of its for loop.  These are the cases
 * where the fcount can be done all at once, or where we encounter
 * an error.
 *
 * The cases that fall through are handled in each_switch.
 */
#define	ret_break(fp)  { exform_break = 1; return (fp); /*NOTREACHED*/}

char *
eachform(fcount, ifp, itype, ptype)
	int fcount;
	char *ifp;
	int itype, ptype;
{
	ushort_t shortval;
#ifdef	_LP64
	addr_t savdot;
#else
	unsigned savdot;
#endif
	int val;
	uint_t uival;
	char *fp, modifier;
	unsigned char c;
	u_longlong_t llval = 0;
	char buf[MAXSYMSIZE];
	int tval[2];

	db_printf(6, "eachform: fcount=%D, ifp=%X, itype=%D, ptype=%D",
						fcount, ifp, itype, ptype);
	exform_break = 0;
	fp = ifp;
	c = *fp;
	var[0] = dot;
	modifier = *fp++;

	switch (modifier) {

	case 't': case 'T':
		printf("%T", fcount); ret_break(fp)
	case '+':
		dot = inkdot(fcount); ret_break(fp)
	case '-':
		dot = inkdot(-fcount); ret_break(fp)
	case '^':
		dot = inkdot(-dotinc*fcount); ret_break(fp)

	case ' ': case '\t':
		return (fp);

#if 0
	case 'I':
		if (itype != SSP) {
			(void) pctofilex(dot);
			if (filex == 0)
				error("source line not found");
			val = filex;
			dotinc = 0;
		} else {
			val = dot;
			/*
			 * if this is all we're doing, then we must
			 * bump dot. Otherwise, we probably want
			 * multiple representations of the same dot.
			 */
			if (*fp == 0)
				dotinc = 1;
			else
				dotinc = 0;
		}
		getline(XFILE(val), XLINE(val));
		if (errflg) {
			ret_break(fp)
		}
		printf("%s", linebuf);
		return (fp);

	case 'A':
		if (itype != SSP) {
			(void) pctofilex(dot);
			val = filex;
		} else
			val = dot;
		dotinc = 0;
		if (val) {
			printf("\"%s\"+%R:%16t",
			    indexf(XFILE(val))->f_name,
				XLINE(val));
			return (fp);
		}
		psymoff(val, ptype, ":%16t");
		return (fp);
#endif
	case 'A':
		(void) printf("%X ", dot);
	case 'a':
#if 0
		if (itype == SSP) {
			val = filextopc(dot);
			if (val == 0) {
				errflg = "pc for source line unknown";
				ret_break(fp);
			}
		} else
#endif
#ifdef	_LP64
		llval = dot;
		db_printf(9, "eachform: printing dot ");
		(void) ssymoff(llval, DSYM, buf, sizeof (buf));
#else
		val = dot;
		db_printf(9, "eachform: printing dot ");
		(void) ssymoff(val, DSYM, buf, sizeof (buf));
#endif
		printf("%s:%16t", buf);
		dotinc = 0;
		return (fp);
	case 'I': 		/* print ins with absolute address */
		(void) printf("%X ", dot);
#ifdef	_LP64
		/*
		 * make sure to prevent sign-extension while printing
		 * instruction bits because get() returns only 'int' and
		 * psymoff() takes 'long'
		 */
		uival = (uint_t)get(dot, itype);
		psymoff((long)uival, ptype, ": ");
#else
		val = get(dot, itype);
		psymoff(val, ptype, ": ");
#endif
	case 'i':
		if (itype != SSP)
			break;
		val = savdot = dot;
#if 0
		val = filextopc(dot);
#endif
		if (val != 0) {
			/*
			 * we used just to report an error for val == 0
			 * this was very unfriendly
			 */
			dot = llval;
			for (;;) {
#if 0
				(void) pctofilex(dot);
				if (filex != savdot)
					break;
#endif
				printins(modifier, ISP,
						t_srcinstr(chkget(dot, ISP)));

				printc('\n');
				dot = inkdot(dotinc);
			}
		}
		dot = savdot;
		dotinc = 1;
		return (fp);
	} /* end first switch on modifier */



	if (charpos() == 0 && modifier == 'J')
		printf("%12m");
	else if (charpos() == 0 && modifier != 'z' &&
	    modifier != 'i' && modifier != 'I')
		printf("%16m");

	switch (modifier) {

	case '"':
		dotinc = 0;
		while (*fp != '"' && *fp)
			printc(*fp++);
		if (*fp)
			fp++;
		return (fp);

	case 'r':
		printc(' ');
		dotinc = 0;
		return (fp);

	case 'n': case 'N':
		printc('\n'); dotinc = 0; return (fp);

	}

	if (itype == SSP)
		error("format not supported with @");
	if (itype == NSP) {
		val = dot;
		shortval = (short)dot;
		c = (char)dot;
		llval = (u_longlong_t)dot;	/* XXX - bogus for now */
	} else {
		/*
		 * Prepare short, long, and longlong values.
		 */
		savdot = dot;
		tval[0] = val = get(dot, itype);

		if (errflg == NULL) {
			/*
			 * Ignore any error on the second get().
			 * We may be at the end of a segment.
			 */
			tval[1] = get(dot+4, itype);
			if (errflg != NULL)
				tval[1] = 0;
			errflg = NULL;
		} else if (dot & 3) {
			/*
			 * Got an error on the first get() and not aligned.
			 * Try to get an integer from a proper alignment.
			 */
			errflg = NULL;
			uival = (uint_t)get(dot & ~3, itype);
			if (errflg == NULL) {
#if defined(_LITTLE_ENDIAN)
				uival >>= (dot & 3) * 8;
#else
				uival <<= (dot & 3) * 8;
#endif
			}
			tval[0] = (int)uival;
			tval[1] = 0;
		}
		dot = savdot;
		llval = *(u_longlong_t *)tval;
		val = *(int *)tval;

		/* handle big-endian and little-endian machines */
		shortval = *(short *)&val;
		c = *(char *)&val;
		/*
		 * All adb command/modifiers that want a 32-bit
		 * value should be listed in the "pPU4W..." string.
		 */
#if defined(sparc)
		/* All sparc instructions are 32 bits long */
		if (!strchr("JKpPU4WXYQODfFiI", modifier))
			val = shortval;
#else /* sparc */
		if (!strchr("pHKPU4WXYQODfF", modifier))
			val = shortval;
#endif /* sparc */
	}
	if (errflg) {
		ret_break(fp)
	}
	if (interrupted)
		error((char *)0);

#ifdef _LP64
	var[0] = llval;
#else
	var[0] = val;
#endif

	each_switch(modifier, val, itype, ptype, c, shortval, llval);

	return (fp);

} /* end of eachform */


static
void
printesc(c)
	char c;
{

	c &= 0177;
	if (c == 0177)
		printf("^?");
	else if (c < ' ')
		printf("^%c", c + '@');
	else
		printc(c);
}

/*
 * each_switch handles most of the "normal" modifiers for exform/eachform.
 * These are:
 *	B C D E F   J O P Q S U W X Y
 *	b c d e f i   o p q s u w x   z
 * This is your last chance, so finally we have a default case.
 */
static void
each_switch(modifier, val, itype, ptype, c, shortval, llval)
	char modifier;
	int val;
	int itype, ptype;
	unsigned char c;
	ushort_t shortval;
	u_longlong_t llval;
{
#ifdef _LP64
	extern int elf64mode;
	addr_t savdot;
#else
	unsigned savdot;
#endif
	double fval;
	union {
		int funi[3];
		double fund;
	} fun;
	static char buf[MAXSYMSIZE];


/* CSTYLED */
	db_printf(6, "each_switch: mod='%c', val=%D, itype=%D,\n\tptype=%D, c='%c', shortval=%u, llval=%J",
	    modifier, val, itype, ptype, c, shortval, llval);

	switch (modifier) {
	case 'I': 		/* print ins with absolute address */
	case 'i':
#if !defined(KADB) || !defined(i386)
		if (!exform_first) {
			/* itype or ptype? */
			if (ssymoff(dot, itype, buf, sizeof (buf)) == 0) {
				/* Hit a symbol exactly */
				printf("%s:", buf);
			}
			printf("%16t");
		}
		if (charpos() == 0)
			printf("%16t");
#endif
		printins(modifier, itype, val);
		printc('\n');
		break;

	case 'g':
		dotinc = 8; printf("%-16g", llval); break;
	case 'G':
		dotinc = 8; printf("%-16G", llval); break;
	case 'e':
		dotinc = 8; printf("%-16e", llval); break;
	case 'E':
		dotinc = 8; printf("%-16E", llval); break;
	case 'J':
		dotinc = 8; printf("%-17J", llval); break;

	case 'K':
#ifdef	_LP64
		if (elf64mode) {
			dotinc = 8; printf("%-16J", llval);
		} else {
			dotinc = 4; printf("%-16X", (int)val);
		}
#else	/* _LP64 */
		dotinc = 4; printf("%-16X", (int)val);
#endif	/* _LP64 */
		break;
	case 'P':
#if 0	/* just make it the same as 'p' */
		dotinc = 4;
		(void) pctofilex(val);
		if (filex) {
			val = filex;
			printf("\"%s\"+%R:%16t",
			    indexf(XFILE(val))->f_name, XLINE(val));
		} else
			psymoff(val, ptype, "%16t");
		break;
#endif

	case 'p':
		dotinc = 4;
#ifdef	_LP64
		/* sym print should always increment dot by sizeof ptr */
		if (elf64mode) {
		    dotinc = 8;
		    (void) ssymoff(llval, ptype, buf, sizeof (buf));
		} else
#endif
		    (void) ssymoff(val, ptype, buf, sizeof (buf));
		printf("%s%16t", buf);
		break;
	case 'u':
		dotinc = 2; printf("%-8u", shortval); break;
	case 'U':
		dotinc = 4; printf("%-16U", (int)val); break;
	case 'c':
		dotinc = 1; printc(c); break;
	case 'C':
		dotinc = 1; printesc(c); break;
	case 'v':
		dotinc = 1; printf("%-8d", (signed char)c); break;
	case 'V':
		dotinc = 1; printf("%-8u", c); break;
	case 'B':
		dotinc = 1; printf("%-8x", c); break;
	case 'b':
		dotinc = 1; printf("%-8o", c); break;
	case 'w':
		dotinc = 2; printf("%-8r", shortval); break;
	case 'W':
		dotinc = 4; printf("%-16R", (int)val); break;
	case 'Z':
		dotinc = 8; printf("%-16J", llval); break;

	case 's': case 'S':
		savdot = dot;
		dotinc = 1;

		for (; ; ) {
			int i, g;
			char *ch;

			g = get(dot, itype);
			if (errflg) {
				break;
			}
			ch = (char *)&g;
			for (i = 0; i < sizeof (g); i++) {
				if (*ch == '\0')
					goto gotitall;
				dot = inkdot(1);
				if (modifier == 'S')
					printesc(*ch);
				else
					printc(*ch);
				ch++;
				endline();
			}
		}
	    gotitall:
		dotinc = (int)(dot - savdot + 1);
		dot = savdot;
		break;

	case 'h':			/* Swap bytes */
		shortval = shortval << 8 | shortval >> 8;
	case 'x':
		dotinc = 2; printf("%-8x", shortval); break;
	case 'H':		/* swap bytes and shorts */
		val = ((val << 24) | ((val << 8) & 0xff0000) |
		    ((val >> 8) & 0xff00) | ((val >> 24) & 0xff));
	case 'X':
		dotinc = 4; printf("%-16X", (int)val); break;
	case 'Y':
		dotinc = 4; printf("%-24Y", (int)val); break;
#ifdef _LP64
	case 'y':
		dotinc = 8; printf("%-24y", llval); break;
#endif
	case 'q':
		dotinc = 2; printf("%-8q", shortval); break;
	case 'Q':
		dotinc = 4; printf("%-16Q", (int)val); break;
	case 'o':
		dotinc = 2; printf("%-8o", shortval); break;
	case 'O':
		dotinc = 4; printf("%-16O", (int)val); break;
	case 'd':
		dotinc = 2; printf("%-8d", shortval); break;
	case 'D':
		dotinc = 4; printf("%-16D", (int)val); break;
#ifndef KADB
	case 'f':
		dotinc = 4;
		fval = *(float *)&val;
		printf("%-16.9f", fval);
		break;
	case 'F':
		dotinc = 8;
		/* these lines depend on the layout of doubles */
		if (itype != NSP || adb_raddr.ra_raddr == 0) {
			fun.funi[0] = get(dot, itype);
			fun.funi[1] = get(dot+4, itype);
		} else {
#ifdef _LP64
			fun.funi[0] = *(int *)adb_raddr.ra_raddr;
			fun.funi[1] = *(int *)((uintptr_t)adb_raddr.ra_raddr +
					(uintptr_t)4);
#else
			fun.funi[0] = *(long *)adb_raddr.ra_raddr;
			fun.funi[1] = *(long *)(adb_raddr.ra_raddr+1);
#endif
		}
		printf("%-32.18F", fun.fund);
		break;
#if 0
	case 'e':
	case 'E':
		if (mc68881 == 0) {
			error("no 68881");
		} else {
#ifdef sun3
			char s[64];
			dotinc = 12;
			/*
			 * these lines depend on the
			 * layout of extendeds
			 */
			if (itype != NSP || adb_raddr.ra_raddr == 0) {
				fun.funi[0] = get(dot, itype);
				fun.funi[1] = get(dot+4, itype);
				fun.funi[2] = get(dot+8, itype);
			} else {
				fun.funi[0] = *(long *)adb_raddr.ra_raddr;
				fun.funi[1] = *(long *)
						(adb_raddr.ra_raddr+4);
				fun.funi[2] = *(long *)
						(adb_raddr.ra_raddr+8);
			}
			fprtos(&fun.x, s);
			prints(s);
#endif sun3
		}
		break;
#endif
#endif !KADB
	default:
		db_printf(3, "each_switch: bad modifier");
		error("bad modifier");
	} /* end 3rd and final switch on modifier */

} /* end of each_switch */




#ifdef _LP64
unsigned long
#endif
inkdot(incr)
	int incr;
{
	unsigned long newdot;

	newdot = dot + incr;
#ifdef _LP64
	if ((dot ^ newdot) & 0x8000000000000000UL)
#else
	if ((dot ^ newdot) & 0x80000000)
#endif
		error("address wrap around");
	return (newdot);
}
