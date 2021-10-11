/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * adb - expression evaluation routines
 */

#pragma ident	"@(#)expr.c	1.29	99/05/04 SMI"

#include <stdio.h>
#include "adb.h"
#include "fio.h"
#include "symtab.h"

char	isymbol[BUFSIZ];

#if defined(KADB)
char	imod_name[MAXCOM];	/* global for deferred bkpt module name */
char	isym_name[MAXCOM];	/* global for deferred bkpt symbol name */
int	is_deferred_bkpt;	/* global to tell if deferred bkpt */
#endif

#define	NEEDEXPR	1
#define	EXPECTRPAREN	2


#ifdef _LP64
extern int		elf64mode;
extern unsigned long	chkgetlong();
#endif

static
symchar(dig)
	int dig;
{

	if (lastc == '\\') {
		(void) readchar();
		return (1);
	}
	return (isalpha(lastc) || lastc == '_' || dig && isdigit(lastc));
}

void
readsym(filename)
	int filename;
{
	register char *p;

	db_printf(9, "readsym: filename=%D", filename);
	p = isymbol;
	do {
		if (p < &isymbol[sizeof (isymbol)-1])
			*p++ = lastc;
		(void) readchar();
		if (filename && (lastc == 0 || lastc == '\n'))
			error("missing '\"'");
	} while (filename ? (lastc != '"') : symchar(1));
	*p++ = 0;
}

convdig(c)
	char c;
{

	if (isdigit(c))
		return (c - '0');
	if (isxdigit(c)) {
		if (islower(c))
			return (c - 'a' + 10);
		else
			return (c - 'A' + 10);
	}
	return (17);
}


varchk(name)
	char name;
{

	if (isdigit(name))
		return (name - '0');
	if (islower(name))
		return ((name - 'a') + 10);
	if (isupper(name))
		return ((name - 'A') + 10 + 26);
	if (name == '_')	/* special variable used by psymoff() */
		return (10 + 26 + 26);
	return (-1);
}

#if 0
eqsym(s1, s2, c)
	register char *s1, *s2;
	char c;
{

	if (strcmp(s1, s2) == 0)
		return (1);
	if (*s1 == c && (strcmp(s1+1, s2) == 0))
		return (1);
	return (0);
}
#endif

static
void
synerror()
{

	error("syntax error");
}

/* Scan out a number and convert it. */
static
getnum()
{
	int base, d, frpt;
	int hex = 0;
#ifndef KADB
	float r;
#endif !KADB

	if (!isdigit(lastc)) {
		if (lastc != '#')
			return (0);
		hex = 1;
		if (!isxdigit(readchar()))
			return (0);
	}
	expv = 0;
	base = (hex ? 16 : radix);
	while (base > 10 ? isxdigit(lastc) : isdigit(lastc)) {
		expv = (base == 16 ? expv<<4 : expv*base);
		if ((d = convdig(lastc)) >= base)
			synerror();
		expv += d;
		(void) readchar();
		if (expv == 0) {
			if (lastc == 'x' || lastc == 'X') {
				hex = 1; base = 16; (void) readchar();
				continue;
			}
			if (lastc == 't' || lastc == 'T') {
				hex = 0; base = 10; (void) readchar();
				continue;
			}
			if (lastc == 'o' || lastc == 'O') {
				hex = 0; base = 8; (void) readchar();
			}
		}
	}
#ifndef KADB
	if (lastc == '.' && (base == 10 || expv == 0) && !hex) {
		r = expv; frpt = 0; base = 10;
		while (isdigit(readchar())) {
			r *= base;
			frpt++;
			r += lastc - '0';
		}
		while (frpt--)
			r /= base;
		expv = *(int *)&r;
	}
#endif !KADB
	nextc = lastc;
	return (1);
}


/*
 * Take what we thought was a symbol, and try it as a hex number.
 * Return 1 for success (symbol WAS hex), zero for failure.
 * If successful, we set "expv" to the value scanned out.
 */
int
symhex(p)
	register char *p;
{
#ifdef _LP64
	register unsigned long val = 0;
#else
	register unsigned val = 0;
#endif

	if (radix != 16)
		return (0);	/* Only works for hex due to isxdigit */

	if (!isxdigit(*p))
		return (0);	/* There must be one digit */

	do {
		val = (val<<4) + convdig(*p++);
	} while (isxdigit(*p));

	if (*p == '\0') {
		expv = val;
		return (1);
	} else
		return (0);
}
/*
 * item:
 *	name
 *	name.local
 *	number
 *	.
 *	^
 *	<var
 *	<register
 *	'x
 *	\procedure
 *	"file"
 *	lambda
 */
item(flags)
	int flags;
{
	short base, d;
	char savc;
	int reg;
	struct asym *s;

	db_printf(9, "item: flags=%D", flags);
	(void) readchar();
	if (symchar(0)) {
		register struct asym *s;

		readsym(0);
		expv = 0;
		lp--;
#if defined(KADB)
		/*
		 * If this is a deferred breakpoint, isymbol is the
		 * module name.  Call to determine if this is a deferred
		 * breakpoint and if so, parse the symbol name as well.
		 */
		if (check_deferred_bkpt(isymbol)) {
			return (1);
		}
#endif /* KADB */
		if ((s = lookup(isymbol)) == 0) {
			if (symhex(isymbol)) {
				return (1);
			} else
				error("symbol not found");
		}
		expv = s->s_value;
		return (1);
	}
	if (getnum())
		return (1);
	switch (lastc) {

	case '.':
		(void) readchar();
		expv = dot;
#ifdef DEBUG
		printf("in item %J %J\n", expv, dot);
#endif
		lp--;
		break;

	case '&':
		expv = ditto; break;
	case '+':
		expv = inkdot(dotinc); break;
	case '^':
		expv = inkdot(-dotinc); break;

	case '<':
		savc = rdc();
		reg = getreg(savc);
		if (reg >= 0) {
			expv = readreg(reg);
			break;
		}
		base = varchk(savc);
		if (base == -1)
			error("bad variable");
		expv = var[base];
		break;

	case '\'':
		{
			/*
			 * Use union to avoid byte ordering problems
			 * for different machine types.
			 */
			union {
				uint_t ui;
				struct {
					uchar_t c0;
					uchar_t c1;
					uchar_t c2;
					uchar_t c3;
				} c;
			} ui;
			union {
				ushort_t us;
				struct {
					uchar_t c0;
					uchar_t c1;
				} c;
			} us;

			d = -1;
			ui.ui = 0;
			us.us = 0;
			while (quotchar()) {
				switch (++d) {
				case 0:
					ui.c.c0 = lastc;
					us.c.c0 = lastc;
					break;
				case 1:
					ui.c.c1 = lastc;
					us.c.c1 = lastc;
					break;
				case 2:
					ui.c.c2 = lastc;
					break;
				case 3:
					ui.c.c3 = lastc;
					break;
				default:
					synerror();
				}
			}
			if (d < sizeof (ushort_t))
				expv = us.us;
			else
				expv = ui.ui;
		}
		break;
#if 0	/* Current compilers don't support "-go". */
	case '"':
		(void) readchar();
		readsym(1);
		expv = FILEX(fget(isymbol)-file, 0) + FILEX(1, 0);
		if (expv <= 0)
			error("file name unknown");
		(void) readchar();
		lp--;
		break;

	case '`':
		(void) readchar();
		readsym(0);
		lp--;
		if ((s = lookup(isymbol)) == 0)
			error("symbol not found");
		(void) pctofilex(s->s_value);
		if (filex == 0)
			error("line for function unknown");
		expv = filex;
		break;
#endif
	default:
		if (flags)
			error("address expected");
		lp--;
		return (0);
	}
	return (1);
}

/*
 * term:
 *	item
 *	monadic item
 *	(expr)
 *	lambda
 */
term(flags)
	int flags;
{
	db_printf(8, "term: flags=%D", flags);
	switch (readchar()) {

	case '*':
		(void) term(flags|NEEDEXPR);
#ifdef _LP64
		if (elf64mode)
		    expv = chkgetlong(expv, DSP);
		else
#endif
		expv = chkget(expv, DSP);
		return (1);
	case '%':
		(void) term(flags|NEEDEXPR);
#ifdef _LP64
		if (elf64mode)
		    expv = chkgetlong(expv, ISP);
		else
#endif
		expv = chkget(expv, ISP);
		return (1);
	case '-':
		(void) term(flags|NEEDEXPR); expv = -expv; return (1);
	case '~':
		(void) term(flags|NEEDEXPR); expv = ~expv; return (1);
	case '#':
		(void) term(flags|NEEDEXPR); expv = !expv; return (1);
#if 0	/* Current compilers don't support "-go". */
	case 'a'&037:
		(void) term(flags|NEEDEXPR); expv = filextopc(expv);
		if (expv == 0)
			error("address for line not found");
		return (1);
	case 'f'&037:
		(void) term(flags|NEEDEXPR); (void) pctofilex(expv);
		if (filex == 0)
			error("source line for address not found");
		expv = filex;
		return (1);
#endif	/* 0 */
	case '(':
		(void) expr(EXPECTRPAREN);
		if (*lp != ')')
			synerror();
		else {
			lp++;
			return (1);
		}

	default:
		lp--;
		return (item(flags));
	}
}
/*
 * expr:
 *	term
 *	term dyadic expr
 *	lambda
 */
expr(flags)
	int flags;
{
#ifdef	_LP64
	int rc;
	long lhs;
#else
	int rc, lhs;
#endif

	db_printf(7, "expr: flags=%D", flags);
	(void) rdc(); lp--; rc = term(flags);
	while (rc) {
		lhs = expv;
		switch (readchar()) {

		case '+':
			(void) term(flags|NEEDEXPR); expv += lhs; break;
		case '-':
			(void) term(flags|NEEDEXPR); expv = lhs - expv; break;
		case '#':
			(void) term(flags|NEEDEXPR); expv = roundup(lhs, expv);
			break;
		case '*':
			(void) term(flags|NEEDEXPR); expv *= lhs; break;
		case '%':
			(void) term(flags|NEEDEXPR); expv = lhs/expv; break;
		case '&':
			(void) term(flags|NEEDEXPR); expv &= lhs; break;
		case '|':
			(void) term(flags|NEEDEXPR); expv |= lhs; break;

		case ')':
			if ((flags&EXPECTRPAREN) == 0)
				error("unexpected ')'");
			/* fall into ... */
		default:
			lp--;
			db_printf(7, "expr: returns %D", rc);
			return (rc);
		}
	}
	db_printf(7, "expr: returns %D", rc);
	return (rc);
}
struct stackpos exppos;

#if 0
static
qualified(name)
	char *name;
{
	register struct asym *s;
	register struct afield *f;
	int inpath;
	static struct asym *context = (struct asym *)NULL;

	expv = 0;
	if (name == 0) {
		if (exppos.k_fp == 0)
			error("no previous frame");
		goto localsym;
	}
	s = lookup(name);
	if (s == 0)
		error("symbol not found");
	expv = s->s_value;
	if (s->s_value < txtmap.map_head->mpr_e) {
		if (pid || (fcor >= 0)) {
			stacktop(&exppos);
			do {
				chkerr();
				if (exppos.k_pc < txtmap.map_head->mpr_e) {
					(void) findsym(exppos.k_pc, ISYM);
					if (cursym &&
					    strcmp(cursym->s_name,
								"start") == 0)
						break;
					if (cursym &&
					    eqsym(cursym->s_name, isymbol, '_'))
						goto found;
				}
			} while (inpath = nextframe(&exppos));
		} else {
			inpath = 0;
		}
found:
		if (lastc != '.') {
			lp--;
			return (1);
		}
		if (!inpath) {
			context = (struct asym *)NULL;
			error("routine not on our call path");
		}
		context = cursym;
		(void) rdc();
localsym:
		if (!symchar(0)) {
			lp--;
			return (1);
		}
		readsym(0);
		f = fieldlookup(isymbol, context->s_f, context->s_fcnt);
		if (f == 0)
			error("local variable not found");
		switch (f->f_type) {

		case N_PSYM:
#ifdef vax
			expv = exppos.k_ap + f->f_offset;
#endif
#ifdef sun
			expv = exppos.k_fp + f->f_offset;
#endif
			break;

		case N_LSYM:
			expv = exppos.k_fp - f->f_offset;
			break;

		case N_RSYM:
			expv = exppos.k_regloc[REG_RN(f->f_offset)];
			break;
		default:
			printf("%s:0x%X ", f->f_name, f->f_type);
			error("unknown local symbol type\n");
		}
	}
	for (;;) {
		if (lastc != '.') {
			lp--;
			return (1);
		}
		(void) rdc();
		if (!symchar(0))
			error("spurious '.'");
		readsym(0);
		f = globalfield(isymbol);
		if (f == 0)
			error("unknown field");
		expv += f->f_offset;
	}
}
#endif
