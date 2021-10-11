/*
 * Copyright (c) 1995, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)command.c	1.40	99/05/04 SMI"

/*
 * adb - command parser
 */

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "adb.h"
#include "fio.h"
#include "fpascii.h"
#include "symtab.h"

char	eqformat[512] = "z";
/* CSTYLED */
char	stformat[512] = "X^\"= \"i";

#define	QUOTE	0200
#define	STRIP	0177

int	lastcom = '=';


struct ecmd ecmd[];
static char ecmdbuf[100];


/*
 * This is used by both generic and arch specific extended cmd parsers.
 * buf:
 * If on input buf is null then scan will get chars from stdio else
 * buf points to the cmd that we will look for.
 * ecmd:
 * This points to an array of ecmd's to look in.
 *
 * On return we return the index into the ecmnd array of the found elemnt
 * If none was found return -1
 */
int
extend_scan(buf, ecmd)
char *buf;
struct ecmd *ecmd;
{
	int i, c;
	char *p;

	if (buf == NULL)
	{
		p = ecmdbuf;

		/* rdc skips spaces, readchar doesn't */
		for (c = rdc(); c != '\n' && c != ' ' && c != '\t' && c != ';';
		    c = readchar())
			*p++ = c;
		*p = 0;
		lp--;	/* don't swallow the newline here */

		p = ecmdbuf;
	} else
		p = buf;

	for (i = 0; ecmd[i].name; i++) {
		if (ecmd[i].name && strcmp(p, ecmd[i].name) == 0) {
			return (i);
		}
	}
	return (-1);
}
static
void
extended_command(void)
{
	int i;

	i = extend_scan((char *)NULL, ecmd);
	if (i >= 0) {
		(*ecmd[i].func)();
		return;
	}
	if (ext_ecmd(ecmdbuf))
		return;
	errflg = "extended command not found";
}



unsigned int max_nargs = 16;   /* max # of args to a function printable by $c */

static
void
ecmd_nargs(void)
{
	if (!hadaddress) {
		(void) printf("nargs = 0x%X\n", max_nargs);
		return;
#if !defined(_LP64)
	/* "address" is a pointer in _LP64 , so we remove the cc warning */
	} else if (address < 0) {
		(void) printf("Invalid nargs = 0x%X\n", address);
		return;
#endif
	}
	max_nargs = (int)address;
}

/*
 * Set level for debugging prints (1 = a little, 9 = a LOT).
 */
static
void
ecmd_dbprt(void)
{
	extern int adb_debug;

	if (!hadaddress) {	/* user just wants current level */
		(void) printf("adb_debug = 0x%X\n", adb_debug);
		return;
#if !defined(_LP64)
	/* "address" is a pointer in _LP64 , so we remove the cc warning */
	} else if (address < 0) {
		(void) printf("Invalid debugging level = 0x%X\n", address);
		return;
#endif
	}
	adb_debug = (int)address;
}

#ifdef KADB
/*
 * Set current module id (helps when same symbol appears in multiple kmods)
 */
static
void
ecmd_curmod(void)
{
	extern int kadb_curmod, current_module_mix;

	if (!hadaddress) {	/* user just wants current value */
		(void) printf("curmod = %D\n", kadb_curmod);
		return;
	}
	kadb_curmod = (int)address;
	current_module_mix = -1;	/* invalidate symbol cache */
}

/*
 * Set number of lines to scroll before asking 'more'
 */
static
void
ecmd_more(void)
{
	extern int adb_more;

	if (!hadaddress) {	/* user just wants current level */
		if (adb_more == 0)
			(void) printf("'more' is currently disabled.\n"
			    "Type 'N::more' to set 'more' to N lines.\n"
			    "Type '0::more' to disable 'more'.\n");
		else
			(void) printf("'more' is set to 0x%X lines.\n"
			    "Type '0::more' to disable 'more'.\n", adb_more);
		return;
#if !defined(_LP64)
	/* "address" is a pointer in _LP64 , so we remove the cc warning */
	} else if (address < 0) {
		(void) printf("Number of lines (0x%X) must be positive\n",
		    address);
		return;
#endif
	}
	adb_more = address;
}

static
void
ecmd_call()
{
	unsigned long	args[CALL_ARGS_MAX], retval;
	int	nargs;
	int	j;
	unsigned long	(*func)();

	if (hadaddress)
		func = (unsigned long(*)())address;
	else
		func = (unsigned long(*)())dot;

	for (nargs = 0; nargs < CALL_ARGS_MAX; nargs++) {
		if (expr(0))
			args[nargs] = expv;
		else
			break;
		if (rdc() != ',') {
			nextc = lastc;
			nargs++;
			break;
		}
	}
	retval = kernel_invoke(func, nargs, args);

	/*
	 * Both branches of the ifdef's below are nominally broken.
	 * retval is neither "%J" (unsigned long long) nor "%X"
	 * (unsigned int), it is "%lX" (unsigned long).  The problem is
	 * that printf() doesn't recognize "%lX" format (or anything
	 * with the 'l' modifier).  The ifdef's work by using
	 * equivalently sized formats for the appropriate compilation
	 * environment.  Changing printf() is the real right answer.  To
	 * avoid changing printf(), I'm using the ifdef's below instead.
	 *
	 * This is a gross hack, and I'm ashamed of it.
	 */

#ifdef _LP64
	printf("%J = ", retval);
#else
	printf("%X = ", retval);
#endif
	errflg = 0;
	psymoff((long)func, ISYM, "(");
	for (j = 0; j < nargs; j++) {
		if (j != 0)
			printf(",");
#ifdef _LP64
		printf("%J", args[j]);
#else
		printf("%X", args[j]);
#endif
	}
	printf(");\n");
}

#if defined(_ILP32)
static
void
ecmd_phys()
{
	if (hadaddress)
		phys_upper32 = address;
	else
		printf("The upper 32 bits of the 64-bit physical address "
		    "= 0x%X\n", phys_upper32);
}
#endif

static void
ecmd_morehelp(void)
{
	printf("Additional Help Screen\n"
	    "    :c            continue kernel execution\n"
	    "    :s            single step (see also ':e', ']' and '[')\n"
	    "    $M            display list of built-in macros\n"
	    "    $<threadlist  display kernel threads (most useful macro)\n"
	    "    $c            display current stack\n"
	    "    address$c     display other stack\n"
	    "    $r            display current registers\n"
	    "    $b            display current breakpoints\n"
	    "    address:b     set software instruction breakpoint\n"
#if defined(i386)
	    "    address:p     set hardware instruction breakpoint\n"
	    "    address:a     set hardware data read or write breakpoint\n"
	    "    address:w     set hardware data write breakpoint\n"
	    "    ioaddress:P   set I/O breakpoint (Pentium processors)\n"
#endif
	    "\n"
"    Display Formats     one byte    two bytes   four bytes  eight bytes\n"
"    octal               b           o           O           G\n"
"    decimal                         d           D           E\n"
"    hexadecimal         B           x           X           J\n"
"    instruction         i           i           i           i\n"
"    character           c\n"
"    string              s           s           s           s\n"
"    modify memory       v           w           W           Z\n");
}

#endif

static
void
ecmd_help(void)
{
	int i;
	struct ecmd *ecmdp;

	printf("Commands:\n");
	printf("    addr,count?fmt       print instructions\n");
	printf("    addr,count/fmt       print data\n");
	printf("    addr,count$cmd       print debugging info based on cmd\n");
	printf("    addr,count:cmd       control debugging based on cmd\n");
#if	!defined(KADB)
	printf("    !shell-cmd           perform the cmd shell command\n");
#else
	printf("    addr,count@fmt       print data from physical address\n");
#endif
	printf("    <num>::extcmd        call extended command\n");
	printf("\nExtended Commands:\n");

	for (i = 0; ecmd[i].name; i++)
		if (ecmd[i].help)
			printf("     %s\t%s", ecmd[i].name, ecmd[i].help);
	for (i = 0; ecmd[i].name; i++)
		if (ecmd[i].help == 0)
			printf("     %s ", ecmd[i].name);

	ecmdp = (struct ecmd *)ext_getstruct();
	for (i = 0; ecmdp[i].name; i++)
		if (ecmdp[i].help)
			printf("     %s\t%s", ecmdp[i].name, ecmdp[i].help);
	for (i = 0; ecmdp[i].name; i++)
		if (ecmdp[i].name && ecmdp[i].help == 0)
			printf("     %s ", ecmdp[i].name);
	printf("\n");
}


struct ecmd ecmd[] = {
{"nargs", ecmd_nargs, "set max # of args to functions in stack backtrace\n"},
{"dbprt", ecmd_dbprt, "set level for adb debugging printfs (1=some, 9=lots)\n"},
#ifdef KADB
{"curmod", ecmd_curmod, "set current module for kernel symbol lookups\n"},
{"more", ecmd_more, "set number of lines to display before asking '--More--'\n"}
,
{"call", ecmd_call, "call the named function (function::call arg1,arg2,arg3,"
	"...)\n"},
#if defined(_ILP32)
{"physrange", ecmd_phys, "upper 32 bits of 64-bit physical address for"
	" \"@fmt\"\n"},
#endif
#endif
{"?", ecmd_help, "\tprint this help display\n"},
{"help", ecmd_help, "print this help display\n"},
#ifdef KADB
{"morehelp", ecmd_morehelp, "print additional help display\n"},
#endif
{0}
};


command(char *buf, char defcom)
{
	int itype, ptype, modifier, reg;
	int bytes;
	int fourbyte, eqcom, atcom;
#ifdef	_LP64
	int eightbyte;
#endif
	char wformat[1];
	char c, savc;
#ifdef	_LP64
	long w;
	addr_t savdot;
#else
	int w, savdot;
#endif
	char *savlp = lp;
#ifdef _LP64
	long locval, locmsk;
#else
	int locval, locmsk;
#endif
#ifndef KADB
	extern char *prompt;
#endif

#if defined(KADB)
	extern char imod_name[];
	extern char isym_name[];
	extern int  is_deferred_bkpt;

	/*
	 * Deferred breakpoints
	 */
	imod_name[0] = '\0';
	isym_name[0] = '\0';
	is_deferred_bkpt = 0;

	phys_address = 0;
#endif

	db_printf(7, "command: buf=%X, defcom='%c'", buf, defcom);
	if (buf != NULL) {
		if (*buf == '\n') {
			db_printf(5, "command: returns 0");
			return (0);
		}
		lp = buf;
	}
	do {
		adb_raddr.ra_raddr = 0;		/* initialize to no */
		if (hadaddress = expr(0)) {
			dot = expv;
			ditto = dot;
		}
#ifdef DEBUG
		printf("dot is %J %J\n", dot, expv);
#endif
		address = dot;
		if (rdc() == ',' && expr(0)) {
			hadcount = 1;
			count = (int)expv;
		} else {
			hadcount = 0;
			count = 1;
			lp--;
		}
		if (!eol(rdc()))
			lastcom = lastc;
		else {
			if (hadaddress == 0)
				dot = inkdot(dotinc);
			lp--;
			lastcom = defcom;
		}

		switch (lastcom & STRIP) {

		case '?':
#ifndef KADB
			if (kernel == NOT_KERNEL) {
				itype = ISP; ptype = ISYM;
				goto trystar;
			}	/* fall through in case of adb -k */
#endif !KADB

		case '/':
			itype = DSP; ptype = DSYM;
			goto trystar;

		case '=':
			itype = NSP; ptype = 0;
			goto trypr;

		case '@':
#if defined(KADB)
			itype = PSP; ptype = 0;
			phys_address = 1;
			goto trystar;
#else
			itype = SSP; ptype = 0;
			goto trypr;
#endif

trystar:
			if (rdc() == '*')
				lastcom |= QUOTE;
			else
				lp--;
			if (lastcom & QUOTE) {
				itype |= STAR;
				ptype = (DSYM+ISYM) - ptype;
			}

trypr:
			fourbyte = 0;
			eqcom = (lastcom == '=');
#if defined(KADB)
			atcom = 0;
#else
			atcom = (lastcom == '@');
#endif
			c = rdc();
			if ((eqcom || atcom) && strchr("mLlWw", c))
				error(eqcom ?
				    "unexpected '='" : "unexpected '@'");
			switch (c) {

			case 'm': {
				int fcount;
#ifdef _LP64
				unsigned long	*mp;
#else
				int	*mp;
#endif
				struct map *smap;
				struct map_range *mpr;

				/*
				 * need a syntax for setting any map range -
				 * perhaps ?/[*|0-9]m b e f fn
				 * where the digit following the ?/ selects the
				 * map range - but it's too late for 4.0
				 */
				smap = (itype&DSP?&datmap:&txtmap);
				mpr = smap->map_head;
				if (itype&STAR)
					mpr = mpr->mpr_next;
				mp = &(mpr->mpr_b); fcount = 3;
				while (fcount && expr(0)) {
					*mp++ = (int)expv;
					fcount--;
				}
				if (rdc() == '?')
					mpr->mpr_fd = fsym;
				else if (lastc == '/')
					mpr->mpr_fd = fcor;
				else
					lp--;
				}
				break;
			case 'L':
				fourbyte = 1;
				goto docasel;
#ifdef	_LP64
			case 'M':
				eightbyte = 1;
#endif
			case 'l':
docasel:
#ifdef	_LP64
				if (eightbyte) dotinc = 8;
				else
#endif
				dotinc = (fourbyte ? 4 : 2);
				savdot = dot;
				(void) expr(1); locval =  expv;
				locmsk = expr(0) ? expv : -1;
#ifdef _LP64
				if ((!fourbyte) && (!eightbyte))
#else
				if (!fourbyte)
#endif
				{
					locmsk = (locmsk << 16);
					locval = (locval << 16);
				}
				for (;;) {
#if defined(KADB)
					tryabort(1); /* check for ctrl-c */
#endif
#ifdef _LP64
					if (eightbyte)
						w = getLong(dot, itype, 1);
					else
#else

						w = get(dot, itype);
#endif
					if (errflg || interrupted)
						break;
					if ((w&locmsk) == locval)
						break;
					dot = inkdot(dotinc);
				}
				if (errflg) {
					dot = savdot;
					errflg = "cannot locate value";
				}
				psymoff(dot, ptype, "");
				break;
#ifdef	_LP64
			case 'Z':
				bytes = 8;
				goto docasew;
#endif
			case 'W':
				bytes = 4;
				goto docasew;
			case 'v':
				bytes = 1;
				goto docasew;
			case 'w':
				bytes = 2;
docasew:
				wformat[0] = lastc; (void) expr(1);
				do {
#if defined(KADB)
				    tryabort(1); /* check for ctrl-c */
#endif
				    savdot = dot;
				    psymoff(dot, ptype, ":%16t");
				    (void) exform(1, wformat, itype, ptype);
				    errflg = 0;
				    dot = savdot;
				    if (bytes == 4)
					put(dot, itype, expv);
#ifdef _LP64
				    else if (bytes == 8) {
					put(dot, itype, *(int *)&expv);
					put(dot+4, itype, *((int *)&expv + 1));
				    }
#endif
				    else {
					int value = get(dot, itype);

					if (bytes == 2)
						*(short *)&value = (short)expv;
					else	/* bytes == 1 */
						*(char *)&value = (char)expv;
					put(dot, itype, value);
				    }
				    savdot = dot;
				    printf("=%8t");
				    (void) exform(1, wformat, itype, ptype);
				    newline();
				} while (expr(0) && errflg == 0);
				dot = savdot;
				chkerr();
				break;

			default:
				if (ext_slash(c, buf, defcom, eqcom, atcom,
				    itype, ptype))
					break;
				lp--;
				getformat(eqcom ? eqformat : stformat);
#if 0
				if (atcom) {
					if (indexf(XFILE(dot)) == 0)
						error("bad file index");
					printf("\"%s\"+%d:%16t",
					    indexf(XFILE(dot))->f_name,
					    XLINE(dot));
				} else
#endif 0

				if (!eqcom)
					psymoff(dot, ptype, ":%16t");
				scanform(count, (eqcom?eqformat:stformat),
				    itype, ptype);
			}
			break;

		case '>':
			lastcom = 0; savc = rdc();
			/*
			 * >/modifier/
			 *
			 * Assign char, short, int, or long to adb register
			 * variable
			 */
			if (savc == '/') {
				char	*dotaddr = (char *)&dot;
#ifdef	_LP64
				int	dot32;

				if (!elf64mode) {
					dot32 = dot;
					dotaddr = (char *)&dot32;
				}
#endif	/* _LP64 */
				savc = rdc();
				savdot = dot;
				switch (savc) {
				case 'c':
					savdot = *(unsigned char *)dotaddr;
					break;
				case 's':
					savdot = *(unsigned short *)dotaddr;
					break;
				case 'i':
					savdot = *(unsigned int *)dotaddr;
					break;
				case 'l':
					savdot = *(unsigned long *)dotaddr;
					break;
				default:
					error("bad assignment modifier");
					break;
				}
				savc = rdc();
				if (savc == '/') {
					dot = savdot;
				}
				else
					error("bad assignment modifier");
				savc = rdc();	/* get adb register name */
			}
			/*
			 * hardware register?
			 */
			reg = getreg(savc);
			if (reg >= 0) {
				if (!writereg(reg, dot)) {
#ifndef KADB
					perror(regnames[reg]);
#else
					error("could not write");
#endif
					db_printf(3, "command: errno=%D",
									errno);
				}
				break;
			}
			/*
			 * adb register variable
			 */
			modifier = varchk(savc);
			if (modifier == -1)
				error("bad variable");
			var[modifier] = dot;
			break;

#ifndef KADB
		case '!':
			lastcom = 0;
			shell();
			break;
#endif !KADB

		case '$':
			lastcom = 0;
			printtrace(nextchar());
			break;

		case ':':
			/* double colon means extended command */
			if (rdc() == ':') {
				extended_command();
				lastcom = 0;
				break;
			}
			lp--;
			if (!isdigit(lastc))	/* length of watchpoint */
				length = 1;
			else {
				length = 0;
				while (isdigit(rdc()))
					length = length * 10 + lastc - '0';
				if (length <= 0)
					length = 1;
				if (lastc)
					lp--;
			}
			if (!executing) {
				executing = 1;
				db_printf(9, "command: set executing=1");
				subpcs(nextchar());
				executing = 0;
				db_printf(9, "command: set executing=0");
				lastcom = 0;
			}
			break;
#ifdef  KADB
		case '[':
			if (!executing) {
				executing = 1;
				db_printf(9, "command: set executing=1");
				subpcs('e');
				executing = 0;
				db_printf(9, "command: set executing=0");
				lastcom = 0;
			}
			break;

		case ']':
			if (!executing) {
				executing = 1;
				db_printf(9, "command: set executing=1");
				subpcs('s');
				executing = 0;
				db_printf(9, "command: set executing=0");
				lastcom = 0;
			}
			break;
#endif	/* KADB */

		case 0:
#ifndef	KADB
			if (prompt == NULL)
				(void) printf("adb");
#endif	/* !KADB */
			break;

		default:
			error("bad command");
		}
		flushbuf();
	} while (rdc() == ';');
	if (buf)
		lp = savlp;
	else
		lp--;
	db_printf(5, "command: returns %D", (hadaddress && (dot != 0)));
	return (hadaddress && dot != 0);		/* for :b */
}
