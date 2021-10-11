/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)expr.c	1.27	99/05/04 SMI"

#include <stdlib.h>
#include <regexpr.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>

#define	A_STRING 258
#define	NOARG 259
#define	OR 260
#define	AND 261
#define	EQ 262
#define	LT 263
#define	GT 264
#define	GEQ 265
#define	LEQ 266
#define	NEQ 267
#define	ADD 268
#define	SUBT 269
#define	MULT 270
#define	DIV 271
#define	REM 272
#define	MCH 273
#define	MATCH 274
#ifdef  _iBCS2
#define	SUBSTR 276
#define	LENGTH 277
#define	INDEX  278
#endif  /* _iBCS2 */

/* size of subexpression array */
#define	MSIZE	LINE_MAX
#define	error(c)	errxx()
#define	EQL(x, y) (strcmp(x, y) == 0)

#define	ERROR(c)	errxx()
#define	MAX_MATCH 20
static int ematch(char *, char *);
static void yyerror(char *);
static void errxx();

long atol();
char *strcpy(), *strncpy();
void exit();

static char *ltoa();
static char *lltoa();
static char	**Av;
static char *buf;
static int	Ac;
static int	Argi;
static int noarg;
static int paren;
#ifdef  _iBCS2
char    *sysv3_set;
#endif  /* _iBCS2 */
/*
 *	Array used to store subexpressions in regular expressions
 *	Only one subexpression allowed per regular expression currently
 */
static char Mstring[1][MSIZE];


static char *operator[] = {
	"|", "&", "+", "-", "*", "/", "%", ":",
	"=", "==", "<", "<=", ">", ">=", "!=",
	"match",
#ifdef	_iBCS2
	"substr", "length", "index",
#endif	/* _iBCS2 */
	"\0" };
static	int op[] = {
	OR, AND, ADD,  SUBT, MULT, DIV, REM, MCH,
	EQ, EQ, LT, LEQ, GT, GEQ, NEQ,
	MATCH
#ifdef	_iBCS2
, SUBSTR, LENGTH, INDEX
#endif	/* _iBCS2 */
	};
static	int pri[] = {
	1, 2, 3, 3, 3, 3, 3, 3, 4, 4, 5, 5, 5, 6, 7
#ifdef	_iBCS2
, 7, 7, 7
#endif	/* _iBCS2 */
	};


/*
 * clean_buf - XCU4 mod to remove leading zeros from negative signed
 *		numeric output, e.g., -00001 becomes -1
 */
static void
clean_buf(buf)
	char *buf;
{
	int i = 0;
	int is_a_num = 1;
	int len;
	long long num;

	if (buf[0] == '\0')
		return;
	len = strlen(buf);
	if (len <= 0)
		return;

	if (buf[0] == '-') {
		i++;		/* Skip the leading '-' see while loop */
		if (len <= 1)	/* Is it a '-' all by itself? */
			return; /* Yes, so return */

		while (i < len) {
			if (! isdigit(buf[i])) {
				is_a_num = 0;
				break;
			}
			i++;
		}
		if (is_a_num) {
			(void) sscanf(buf, "%lld", &num);
			(void) sprintf(buf, "%lld", num);
		}
	}
}

/*
 * End XCU4 mods.
 */

static int
yylex() {
	register char *p;
	register i;

	if (Argi >= Ac)
		return (NOARG);

	p = Av[Argi];

	if ((*p == '(' || *p == ')') && p[1] == '\0')
		return ((int)*p);
	for (i = 0; *operator[i]; ++i)
		if (EQL(operator[i], p))
			return (op[i]);


	return (A_STRING);
}

static char
*rel(oper, r1, r2) register char *r1, *r2;
{
	long i;

	if (ematch(r1, "-\\{0,1\\}[0-9]*$") && ematch(r2, "-\\{0,1\\}[0-9]*$"))
		i = atol(r1) - atol(r2);
	else
		i = strcoll(r1, r2);
	switch (oper) {
	case EQ:
		i = i == 0;
		break;
	case GT:
		i = i > 0;
		break;
	case GEQ:
		i = i >= 0;
		break;
	case LT:
		i = i < 0;
		break;
	case LEQ:
		i = i <= 0;
		break;
	case NEQ:
		i = i != 0;
		break;
	}
	return (i ? "1": "0");
}

static char
*arith(oper, r1, r2) char *r1, *r2;
{
	long long i1, i2;
	register char *rv;

	if (!(ematch(r1, "-\\{0,1\\}[0-9]*$") &&
	    ematch(r2, "-\\{0,1\\}[0-9]*$")))
		yyerror("non-numeric argument");
	i1 = atoll(r1);
	i2 = atoll(r2);

	switch (oper) {
	case ADD:
		i1 = i1 + i2;
		break;
	case SUBT:
		i1 = i1 - i2;
		break;
	case MULT:
		i1 = i1 * i2;
		break;
	case DIV:
		if (i2 == 0)
			yyerror("division by zero");
		i1 = i1 / i2;
		break;
	case REM:
		if (i2 == 0)
			yyerror("division by zero");
		i1 = i1 % i2;
		break;
	}
	rv = malloc(25);
	(void) strcpy(rv, lltoa(i1));
	return (rv);
}

static char
*conj(oper, r1, r2)
	char *r1, *r2;
{
	register char *rv;

	switch (oper) {

	case OR:
		if (EQL(r1, "0") || EQL(r1, "")) {
			if (EQL(r2, "0") || EQL(r2, ""))
				rv = "0";
			else
				rv = r2;
		} else
			rv = r1;
		break;
	case AND:
		if (EQL(r1, "0") || EQL(r1, ""))
			rv = "0";
		else if (EQL(r2, "0") || EQL(r2, ""))
			rv = "0";
		else
			rv = r1;
		break;
	}
	return (rv);
}

#ifdef	_iBCS2
char *
substr(char *v, char *s, char *w)
{
	register si, wi;
	register char *res;

	si = atol(s);
	wi = atol(w);
	while (--si)
		if (*v) ++v;

	res = v;

	while (wi--)
		if (*v) ++v;

	*v = '\0';
	return (res);
}

char *
index(char *s, char *t)
{
	register long i, j;
	register char *rv;

	for (i = 0; s[i]; ++i)
		for (j = 0; t[j]; ++j)
			if (s[i] == t[j]) {
				(void) strcpy(rv = malloc(8), ltoa(++i));
				return (rv);
			}
	return ("0");
}

char *
length(s)
register char *s;
{
	register long i = 0;
	register char *rv;

	while (*s++) ++i;

	rv = malloc(8);
	(void) strcpy(rv, ltoa(i));
	return (rv);
}
#endif	/* _iBCS2 */

static char *
match(s, p)
char *s, *p;
{
	register char *rv;
	long val;			/* XCU4 */

	(void) strcpy(rv = malloc(8), ltoa(val = (long)ematch(s, p)));
	if (nbra /* && val != 0 */) {
		rv = malloc((unsigned)strlen(Mstring[0]) + 1);
		(void) strcpy(rv, Mstring[0]);
	}
	return (rv);
}


/*
 * ematch 	- XCU4 mods involve calling compile/advance which simulate
 *		  the obsolete compile/advance functions using regcomp/regexec
 */
static int
ematch(s, p)
char *s;
register char *p;
{
	static char *expbuf;
	char *nexpbuf;
	register num;
#ifdef XPG4
	int nmatch;		/* number of matched bytes */
	char tempbuf[256];
	char *tmptr1 = 0;	/* If tempbuf is not large enough */
	char *tmptr;
	int nmbchars;		/* number characters in multibyte string */
#endif

	nexpbuf = compile(p, (char *)0, (char *)0);	/* XCU4 regex mod */
	if (0 /* XXX nbra > 1*/)
		yyerror("Too many '\\('s");
	if (regerrno) {
		if (regerrno != 41 || expbuf == NULL)
			errxx();
	} else {
		if (expbuf)
			free(expbuf);
		expbuf = nexpbuf;
	}
	if (advance(s, expbuf)) {
		if (nbra > 0) {
			p = braslist[0];
			num = braelist[0] - p;
			if ((num > MSIZE - 1) || (num < 0))
				yyerror("string too long");
			(void) strncpy(Mstring[0], p, num);
			Mstring[0][num] = '\0';
		}
#ifdef XPG4
		/*
		 *  Use mbstowcs to find the number of multibyte characters
		 *  in the multibyte string beginning at s, and
		 *  ending at loc2.  Create a separate string
		 *  of the substring, so it can be passed to mbstowcs.
		 */
		nmatch = loc2 - s;
		if (nmatch > ((sizeof (tempbuf) / sizeof (char)) - 1)) {
			if ((tmptr1 = malloc(nmatch + 1)) == NULL) {
				yyerror("malloc error");
				return (0);
			}
			tmptr = tmptr1;
		} else {
			tmptr = tempbuf;
		}
		memcpy(tmptr, s, nmatch);
		*(tmptr + nmatch) = '\0';
		if ((nmbchars = mbstowcs(NULL, tmptr, NULL)) == -1) {
			yyerror("invalid multibyte character encountered");
			if (tmptr1 != NULL)
				free(tmptr1);
			return (0);
		}
		if (tmptr1 != NULL)
			free(tmptr1);
		return (nmbchars);
#else
		return (loc2-s);
#endif
	}
	return (0);
}

static void
errxx()
{
	yyerror("RE error");
}

static void
yyerror(s)
char *s;
{
	(void) write(2, "expr: ", 6);
	(void) write(2, gettext(s), (unsigned)strlen(gettext(s)));
	(void) write(2, "\n", 1);
	exit(2);
	/* NOTREACHED */
}

static char
*ltoa(l)
long l;
{
	static str[20];
	register char *sp = (char *)&str[18];	/* u370 */
	register i;
	register neg = 0;

	if ((unsigned long)l == 0x80000000UL)
		return ("-2147483648");
	if (l < 0)
		++neg, l = -l;
	str[19] = '\0';
	do {
		i = l % 10;
		*sp-- = '0' + i;
		l /= 10;
	}
	while (l);
	if (neg)
		*sp-- = '-';
	return (++sp);
}

static char
*lltoa(l)
long long l;
{
	static str[25];
	register char *sp = (char *)&str[23];
	register i;
	register neg = 0;

	if (l == 0x8000000000000000ULL)
		return ("-9223372036854775808");
	if (l < 0)
		++neg, l = -l;
	str[24] = '\0';
	do {
		i = l % 10;
		*sp-- = '0' + i;
		l /= 10;
	}
	while (l);
	if (neg)
		*sp-- = '-';
	return (++sp);
}

static char
*expres(prior, par)
	int prior, par;
{
	int ylex, temp, op1;
	char *r1, *ra, *rb, *rc;
	ylex = yylex();
	if (ylex >= NOARG && ylex < MATCH) {
		yyerror("syntax error");
	}
	if (ylex == A_STRING) {
		r1 = Av[Argi++];
		temp = Argi;
	} else {
		if (ylex == '(') {
			paren++;
			Argi++;
			r1 = expres(0, Argi);
			Argi--;
		}
	}
lop:
	ylex = yylex();
	if (ylex > NOARG && ylex < MATCH) {
		op1 = ylex;
		Argi++;
		if (pri[op1-OR] <= prior)
			return (r1);
		else {
			switch (op1) {
			case OR:
			case AND:
				r1 = conj(op1, r1, expres(pri[op1-OR], 0));
				break;
			case EQ:
			case LT:
			case GT:
			case LEQ:
			case GEQ:
			case NEQ:
				r1 = rel(op1, r1, expres(pri[op1-OR], 0));
				break;
			case ADD:
			case SUBT:
			case MULT:
			case DIV:
			case REM:
				r1 = arith(op1, r1, expres(pri[op1-OR], 0));
				break;
			case MCH:
				r1 = match(r1, expres(pri[op1-OR], 0));
				break;
			}
			if (noarg == 1) {
				return (r1);
			}
			Argi--;
			goto lop;
		}
	}
	ylex = yylex();
	if (ylex == ')') {
		if (par == Argi) {
			yyerror("syntax error");
		}
		if (par != 0) {
			paren--;
			Argi++;
		}
		Argi++;
		return (r1);
	}
	ylex = yylex();
#ifdef	_iBCS2
	if (ylex > MCH && ((sysv3_set && ylex <= INDEX) || ylex <= MATCH)) {
#else
	if (ylex > MCH && ylex <= MATCH) {
#endif	/* _iBCS2 */
		if (Argi == temp) {
			return (r1);
		}
		op1 = ylex;
		Argi++;
		switch (op1) {
		case MATCH:
			rb = expres(pri[op1-OR], 0);
			ra = expres(pri[op1-OR], 0);
			break;
#ifdef	_iBCS2
		case SUBSTR:
			rc = expres(pri[op1-OR], 0);
			rb = expres(pri[op1-OR], 0);
			ra = expres(pri[op1-OR], 0);
			break;
		case LENGTH:
			ra = expres(pri[op1-OR], 0);
			break;
		case INDEX:
			rb = expres(pri[op1-OR], 0);
			ra = expres(pri[op1-OR], 0);
			break;
#endif	/* _iBCS2 */
		}
		switch (op1) {
		case MATCH:
			r1 = match(rb, ra);
			break;
#ifdef	_iBCS2
		case SUBSTR:
			r1 = substr(rc, rb, ra);
			break;
		case LENGTH:
			r1 = length(ra);
			break;
		case INDEX:
			r1 = index(rb, ra);
			break;
#endif	/* _iBCS2 */
		}
		if (noarg == 1) {
			return (r1);
		}
		Argi--;
		goto lop;
	}
	ylex = yylex();
	if (ylex == NOARG) {
		noarg = 1;
	}
	return (r1);
}

void
main(argc, argv)
	char **argv;
{

	/*
	 * XCU4 allow "--" as argument
	 */
	if (argc > 1 && strcmp(argv[1], "--") == 0)
		argv++, argc--;
	/*
	 * XCU4 - print usage message when invoked without args
	 */
	if (argc < 2) {
		fprintf(stderr, gettext("Usage: expr expression\n"));
		exit(3);
	}
	Ac = argc;
	Argi = 1;
	noarg = 0;
	paren = 0;
	Av = argv;
#ifdef	_iBCS2
	sysv3_set = getenv("SYSV3");
#endif	/* _iBCS2 */

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);
	buf = expres(0, 1);
	if (Ac != Argi || paren != 0) {
		yyerror("syntax error");
	}
	/*
	 * XCU4 - strip leading zeros from numeric output
	 */
	clean_buf(buf);
	(void) write(1, buf, (unsigned)strlen(buf));
	(void) write(1, "\n", 1);
	exit((strcmp(buf, "0") == 0 || buf[0] == 0) ? 1 : 0);
	/* NOTREACHED */
}
