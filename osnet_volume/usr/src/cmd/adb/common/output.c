/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)output.c	1.40	99/05/04 SMI"

/*
 * adb - output routines
 */

#include <stdio.h>
#include <stdarg.h>
#include "adb.h"
#include <strings.h>
#include <time.h>


int		infile = 0;
int		outfile = 1;
extern int	maxpos;
extern int	maxoff;
int		radix = 16;

static void printhex(uint_t);
static void printhex_cmn(u_longlong_t, int);
static void printoct_cmn(longlong_t, int, int);
static void print_ll(longlong_t, char);
static void printnum(longlong_t, char, int);
int	convert(char **);
static	void	printoct(int, int);
static	void	printdate(time_t);

#define	MAXLIN	255
#define	LEN_DEC_64	20	/* no of digits in 64 bits in octal format */
#define	LEN_OCT_64	22	/* no of digits in 64 bits in octal format */
#define	LEN_OCT_32	11	/* no of digits in 32 bits in octal format */

char	printbuf[MAXLIN];
char	*printptr = printbuf;
char	*digitptr;


void
printc(c)
	char c;
{
	char d, *q;
	int posn, tabs, p;
	extern	int	_write();

	if (interrupted)
		return;
	if ((*printptr = c) == '\n') {
		tabs = 0;
		posn = 0;
		q = printbuf;
		for (p = 0; p < printptr - printbuf; p++) {
			d = printbuf[p];
			if ((p&7) == 0 && posn) {
				tabs++;
				posn = 0;
			}
			if (d == ' ')
				posn++;
			else {
				while (tabs > 0) {
					*q++ = '\t';
					tabs--;
				}
				while (posn > 0) {
					*q++ = ' ';
					posn--;
				}
				*q++ = d;
			}
		}
		*q++ = '\n';
#if defined(KADB)
		trypause();
#endif
		(void) write(outfile, printbuf, q - printbuf);
#if defined(KADB)
		trypause();
#endif
		printptr = printbuf;
		return;
	}
	if (c == '\t') {
		*printptr++ = ' ';
		while ((printptr - printbuf) & 7)
			*printptr++ = ' ';
	} else if (c)
		printptr++;
#if defined(KADB)
	trypause();
#endif
	if (printptr >= &printbuf[sizeof (printbuf)-9]) {
		(void) write(outfile, printbuf, printptr - printbuf);
		printptr = printbuf;
	}
#if defined(KADB)
	trypause();
#endif
}

int
charpos()
{

	return (printptr - printbuf);
}

void
flushbuf()
{

	if (printptr != printbuf)
		printc('\n');
}

void
killbuf()
{

	if (printptr != printbuf) {
		printptr = printbuf;
		printc('\n');
	}
}

#define	VAL va_arg(vptr, int)

/*
 * This printf declaration MUST look just like the prototype definition
 * in stdio.h to propitiate the ANSI gods.
 */
printf(const char *fmat, ...)
{
	char *fmt = (char *)fmat;
	char *s;
	int width, prec;
	char c, adj;
	int n, val, sygned;
	longlong_t llval;
	u_longlong_t ullval;
	double d;
	char digits[1024 /* temporary kluge for sprintf bug */];

	va_list vptr;
	va_start(vptr, fmat);

	while (c = *fmt++) {
		if (c != '%') {
			printc(c);
			continue;
		}
		if (*fmt == '-') {
			adj = 'l';
			fmt++;
		} else
			adj = 'r';
		width = convert(&fmt);
		if (*fmt == '.') {
			fmt++;
			prec = convert(&fmt);
		} else
			prec = -1;
		if (*fmt == '+') {
			fmt++;
			sygned = 1;
		} else {
			sygned = 0;
		}
		digitptr = digits;

		s = 0;
		switch (c = *fmt++) {
		case 'g':	/* signed octal */
		case 'G':	/* unsigned octal */
		case 'e':	/* signed decimal */
		case 'E':	/* unsigned decimal */
		case 'J':	/* hexadecimal */
			{
				llval = va_arg(vptr, longlong_t);
				print_ll(llval, c);
				break;
			}

		case 'o':
			printoct((unsigned short)VAL, 0); break;
		case 'q':
			printoct((short)VAL, -1); break;
		case 'x':
			printhex((unsigned short)VAL); break;
		case 'Y':
			printdate((time_t)((unsigned)VAL)); break;
#ifdef _LP64
		/* Prints in 64 bit date format */
		case 'y':
			ullval = va_arg(vptr, u_longlong_t);
			printdate((time_t)ullval);
			break;
#endif
		case 'r':
			/*
			 * "%+r" is printed in the current radix
			 * with a minus sign if the value is negative
			 */
			if (sygned) {
				printnum((short)VAL, '+', radix);
			} else {
				printnum((unsigned short)VAL, c, radix);
			}
			break;
		case 'R':
			if (radix == 0x10)
				printnum((unsigned)VAL,
						(sygned? '+': c), radix);
			else
				printnum(VAL, (sygned? '+': c), radix);
			break;
		case 'd':
			printnum((short)VAL, c, 10); break;
		case 'u':
			printnum((unsigned short)VAL, c, 10); break;
		case 'D':
			printnum(VAL, c, 10); break;
		case 'U':
			printnum((unsigned)VAL, c, 10); break;
		case 'O':
			printoct(VAL, 0); break;
		case 'Q':
			printoct(VAL, -1); break;
		case 'X':
			printhex(VAL); break;
		case 'c':
			printc(VAL); break;
		case 's':
			s = va_arg(vptr, char *); break;
		case 'z':
			{
			/* form for disassembled 16 bit immediate constants. */
			val = VAL;
			if ((-9 <= val) && (val <= 9)) {
				/* Let's not use 0x for unambiguous numbers. */
				printnum(val, 'd', 10);
			} else {
				/* 0xhex for big numbers. */
				if (sygned && (val < 0))
					printc('-');
				printc('0');
				printc('x');
				if (sygned && (val < 0))
					printhex(-val);
				else
					printhex(val);
			}
			break;
			}
		case 'Z':
			{
			/* form for disassembled 32 bit immediate constants. */
			val = VAL;
			if ((-9 <= val) && (val <= 9)) {
				/* Let's not use 0x for unambiguous numbers. */
				printnum(val, 'D', 10);
			} else {
				/* 0xhex for big numbers. */
				if (sygned && (val < 0))
					printc('-');
				printc('0');
				printc('x');
				if (sygned && (val < 0))
					printhex(-val);
				else
					printhex(val);
			}
			break;
			}
#ifndef KADB
		case 'f':
		case 'F':
			s = digits;

			d =  va_arg(vptr, double);

			prec = -1;
			if (c == 'f') {
				(void) sprintf(s, "%+.7e", d);
			} else {
				(void) sprintf(s, "%+.16e", d);
			}
			break;
#endif !KADB
		case 'm':
			break;
		case 'M':
			width = VAL; break;
		case 'T':
		case 't':
			if (c == 'T')
				width = VAL;
			if (width)
				width -= charpos() % width;
			break;
		default:
			printc(c);
		}
		if (s == 0) {
			*digitptr = 0;
			s = digits;
		}
		n = strlen(s);
		if (prec < n && prec >= 0)
			n = prec;
		width -= n;
		if (adj == 'r')
			while (width-- > 0)
				printc(' ');
		while (n--)
			printc(*s++);
		while (width-- > 0)
			printc(' ');
		digitptr = digits;
	}
	return (0);
} /* end printf */

static
void
printdate(tvec)
	time_t tvec;
{
	register int i;
	register char *timeptr = ctime((time_t *)&tvec);

	if (timeptr == NULL) {
		strcpy(digitptr, "VALUE TOO LARGE");
		digitptr = (char *)((uintptr_t)digitptr + strlen(digitptr));
		return;
	}
	for (i = 20; i < 24; i++)
		*digitptr++ = timeptr[i];
	for (i = 3; i < 19; i++)
		*digitptr++ = timeptr[i];
}

void
prints(s)
	char *s;
{

	printf("%s", s);
}

void
newline()
{

	printc('\n');
}

convert(cp)
	register char **cp;
{
	register char c;
	int n;

	n = 0;
	while ((c = *(*cp)++) >= '0' && c <= '9')
		n = n * 10 + c - '0';
	(*cp)--;
	return (n);
}

static void
printnum(longlong_t n, char fmat, int base)
{
	register int k;
	register unsigned long int un;
	char digs[LEN_DEC_64 +1];
	register char *dptr = digs;

	/*
	 * if signs are wanted, put 'em out
	 */
	switch (fmat) {
	case 'r':
	case 'R':
		if (base != 10) break;
	case '+':
	case 'd':
	case 'D':
	case 'e':
	case 'E':
	case 'q':
	case 'Q':
		if (n < 0) {
			n = -n;
			*digitptr++ = '-';
		}
		break;
	}
	/*
	 * put out radix
	 */
	switch (base) {
	default:
		break;
	case 010:
		*digitptr++ = '0';
		break;
	case 0x10:
		*digitptr++ = '0';
		*digitptr++ = 'x';
		break;
	}
	un = n;
	while (un) {
		*dptr++ = un % base;
		un /= base;
	}
	if (dptr == digs)
		*dptr++ = 0;
	while (dptr != digs) {
		k = *--dptr;
		*digitptr++ = k + (k <= 9 ? '0' : 'a'-10);
	}
}


static
void
printoct(o, s)
	int o, s;
{
	printoct_cmn(o, s, LEN_OCT_32);
}

static void
printoct_cmn(longlong_t o, int s, int ndigs)
{
	int i;
	longlong_t po = o;
	char digs[LEN_OCT_64 +1];

	if (s) {
		if (po < 0) {
			po = -po;
			*digitptr++ = '-';
		} else
			if (s > 0)
				*digitptr++ = '+';
	}
	for (i = 0; i < ndigs; i++) {
		digs[i] = po & 7;
		po >>= 3;
	}

	if (ndigs == LEN_OCT_64)
		digs[LEN_OCT_64 -1] &= 01;
	else if (ndigs == LEN_OCT_32)
		digs[LEN_OCT_32 -1] &= 03;

	digs[ndigs] = 0;

	for (i = ndigs; i >= 0; i--)
		if (digs[i])
			break;
	for (i++; i >= 0; i--)
		*digitptr++ = digs[i] + '0';
}

static void
print_ll(longlong_t x, char fmt)
{
	switch (fmt) {
	case 'g':	/* signed octal */
		printoct_cmn(x, -1, LEN_OCT_64);
		break;
	case 'G':	/* unsigned octal */
		printoct_cmn(x, 0, LEN_OCT_64);
		break;
	case 'e':	/* signed decimal */
	case 'E':	/* unsigned decimal */
		printnum(x, fmt, 10);
		break;
	case 'J':	/* hexadecimal */
		printhex_cmn(x, 16);
		break;
	}
}

static void
printhex(x)
	uint_t x;
{
	printhex_cmn((u_longlong_t)(uint_t)x, 8);
}

static void
printhex_cmn(x, ndigs)
	u_longlong_t x;
	int ndigs;
{
	int i;
	char digs[16];
	static char hexe[] = "0123456789abcdef";

	for (i = 0; i < ndigs; i++) {
		digs[i] = x & 0xf;
		x >>= 4;
	}
	for (i = ndigs-1; i > 0; i--)
		if (digs[i])
			break;
	for (; i >= 0; i--)
		*digitptr++ = hexe[digs[i]];
}

void
oclose()
{
	db_printf(7, "oclose, outfile=%D", outfile);
	if (outfile != 1) {
		flushbuf();
		(void) close(outfile);
		outfile = 1;
	}
}

void
endline()
{

	if (maxpos <= charpos())
		printf("\n");
}



/*
 * adb-debugging printout routine
 */

int adb_debug = 0;	/* public, set via "+d" adb arg */

#ifdef	__STDC__
db_printf(short level, ...)
#else	/* __STDC__ */
db_printf(va_alist)
	va_dcl
#endif	/* __STDC__ */
{

	if (adb_debug) {
		va_list vptr;
#ifndef	__STDC__
		short level;
#endif

#ifdef	__STDC__
	va_start(vptr, level);
#else	/* __STDC__ */
	va_start(vptr);
#endif	/* __STDC__ */

		/*
		 * Set the first field (level) in db_printf() to the following:
		 *
		 * level=0	does not print the message
		 * level=1	very important message, used rarely
		 * level=2	important message
		 * level=3	less important message
		 * level=4	print args when entering the function or
		 *			print the return values
		 * level=5	same as 4, but for less important functions
		 * level=6	same as 5, but for less important functions
		 * level=7	detailed
		 * level=8	very detailed
		 * ...
		 * level=ADB_DEBUG_MAX	prints ALL the messages
		 */
#ifndef	__STDC__
		level = va_arg(vptr, short);
#endif
		if (level && adb_debug >= level) {
			long a, b, c, d, e;
			char fmt[256];

			(void) sprintf(fmt, "  %d==>\t%s\n",
						level, va_arg(vptr, char *));
			a = va_arg(vptr, long);
			b = va_arg(vptr, long);
			c = va_arg(vptr, long);
			d = va_arg(vptr, long);
			e = va_arg(vptr, long);
			printf(fmt, a, b, c, d, e);
#ifndef KADB
			fflush(stdout);
#endif
			va_end(vptr);
		}
	}
	return (0);
}

#ifdef KADB


/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void
_printn(uint64_t n, int b, int width, int pad,
    void (*emit)(char, char **, int *), char **bp, int *countp)
{
	char prbuf[40];
	char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
		width--;
	} while (n);
	while (width-- > 0)
		*cp++ = (char)pad;
	do {
		(*emit)(*--cp, bp, countp);
	} while (cp > prbuf);
}
static void
_sput(char c, char **p, int *countp)
{
	**p = c;
	*p += 1;
	*countp++;
}




static int
_doprint(const char *fmt, va_list adx, void (*emit)(char, char **, int *),
    char **bp)
{
	int b, c, i, pad, width, ells;
	char *s;
	int64_t l;
	uint64_t ul;
	int count = 0;
	int cc;
	int left;

loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return (count);
		(*emit)((char)c, bp, &count);
	}
	left = 0;
	c = *fmt++;
	if (c == '-') {
		left = 1; c = *fmt++;
	}

	for (pad = ' '; c == '0'; c = *fmt++)
		pad = '0';

	for (width = 0; c >= '0' && c <= '9'; c = *fmt++)
		width = width * 10 + c - '0';

	for (ells = 0; c == 'l'; c = *fmt++)
		ells++;

	switch (c) {
	case 'd':
	case 'D':
		b = 10;
		if (ells == 0)
			l = (int64_t)va_arg(adx, int);
		else if (ells == 1)
			l = (int64_t)va_arg(adx, long);
		else
			l = (int64_t)va_arg(adx, int64_t);
		if (l < 0) {
			(*emit)('-', bp, &count);
			width--;
			ul = -l;
		} else
			ul = l;
		goto number;

	case 'p':
		ells = 1;
		/*FALLTHROUGH*/
	case 'x':
	case 'X':
		b = 16;
		goto u_number;

	case 'u':
		b = 10;
		goto u_number;

	case 'o':
	case 'O':
		b = 8;
u_number:
		if (ells == 0)
			ul = (uint64_t)va_arg(adx, uint_t);
		else if (ells == 1)
			ul = (uint64_t)va_arg(adx, ulong_t);
		else
			ul = (uint64_t)va_arg(adx, uint64_t);
number:
		_printn(ul, b, width, pad, emit, bp, &count);
		break;

	case 'c':
		b = va_arg(adx, int);
		for (i = 24; i >= 0; i -= 8)
			if ((c = ((b >> i) & 0x7f)) != 0)
				(*emit)(c, bp, &count);
		break;

	case 's':
		s = va_arg(adx, char *);
		cc = 0;
		while ((c = *s++) != 0) {
			(*emit)(c, bp, &count);
			cc++;
		}
		while (left && (cc < width)) {
			(*emit)(' ', bp, &count);
			cc++;
		}
		break;

	case '%':
		(*emit)('%', bp, &count);
		break;
	}
	goto loop;
}


/*VARARGS2*/
int
sprintf(char *s, const char *fmt, ...)
{
	char *bp = s;
	va_list adx;
	int count;

	va_start(adx, fmt);
	count = _doprint(fmt, adx, _sput, &bp);
	*bp++ = (char)0;
	va_end(adx);
	return (count);
}
#endif
