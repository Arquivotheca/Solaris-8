/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)printf.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	printf  (printf.c)
 *
 *   Calling Syntax:	printf ( fmt_string, arg1, arg2, ..., argx )
 *
 *   Description:	(adapted from prom_printf.c	1.4	91/12/12 SMI)
 *			ubiquitous formatted print routine.
 *
 */

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

#include <bioserv.h>
#include <sys/types.h>
#include <sys/varargs.h>

#define static                         /* vla veryfornow..... */
static void _doprint(char _FAR_ *, va_list, void (*)(char, char _FAR_ * _FAR_ *), char  _FAR_ * _FAR_ *);
static void _printn(u_long, long, long, short, void (*)(char, char  _FAR_ * _FAR_ *), char  _FAR_ * _FAR_ *);
static void _pput ();
static void _sput ();
void printf ();
void vprintf ();
char _FAR_ *sprintf ();
char _FAR_ *vsprintf ();

/*
 * Emit character functions...
 */

/*ARGSUSED*/
static void
_pput(char c, char _FAR_ * _FAR_ *p)
{
	(void) putchar(c);
}

static void
_sput(char c, char _FAR_ * _FAR_ *p)
{
	**p = c;
	*p += 1;
}

/*VARARGS1*/
void
printf(char _FAR_ *fmt, va_dcl )
{
	va_list adx;

	va_start(adx, fmt);
	(void) _doprint(fmt, adx, _pput, (char _FAR_ * _FAR_ *)0);
	va_end(adx);
}

void
vprintf(char _FAR_ *fmt, va_list adx)
{
	(void) _doprint(fmt, adx, _pput, (char _FAR_ * _FAR_ *)0);
}

/*VARARGS2*/
char _FAR_ *
sprintf(char _FAR_ *s, char _FAR_ *fmt, va_dcl)
{
	char _FAR_ *bp = s;
	va_list adx;

	va_start(adx, fmt);
	(void) _doprint(fmt, adx, _sput, (char _FAR_ * _FAR_ *)&bp);
	*bp++ = (char)0;
	va_end(adx);
	return (s);
}

char _FAR_ *
vsprintf(char _FAR_ *s, char _FAR_ *fmt, va_list adx)
{
	char _FAR_ *bp = s;

	(void) _doprint(fmt, adx, _sput, (char _FAR_ * _FAR_ *)&bp);
	*bp++ = (char)0;
	return (s);
}

static void
_doprint(char _FAR_ *fmt, va_list adx, void (*emit)(char, char  _FAR_ * _FAR_ *), char  _FAR_ * _FAR_ *bp)
{
	register long b, c, i, width;
   short lflg = 0, pad = 0x20;                   /* print a long value */
	register char _FAR_ *s;

loop:
	width = 0;
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			goto out;
		if (c == '\n')
			(*emit)('\r', bp);
		(*emit)((char)c, bp);
	}
again:
	c = *fmt++;
   if ( c == '0' ) {       /* %7d left pads with ' ', %07d left pads with '0' */
      pad = '0';
      c = *fmt++;
   }
	if (c >= '2' && c <= '9') {
		width = c - '0';
		c = *fmt++;
	}
	switch (c) {

	case 'l':
      lflg = 1;
		goto again;
	case 'x':
	case 'X':
		b = 16;
		goto number;
	case 'd':
	case 'D':
	case 'u':
		b = 10;
		goto number;
	case 'o':
	case 'O':
		b = 8;
number:
      if ( lflg )
		   _printn(va_arg(adx, u_long), b, width, pad, emit, bp);
         
      else
		   _printn(va_arg(adx, u_short), b, width, pad, emit, bp);

      lflg = 0;                     /* reset the long flag */
		break;
	case 'c':
		b = va_arg(adx, long);
		for (i = 24; i >= 0; i -= 8)
#ifdef MSC_DEPENDENCY
			if (c = (b >> i) & 0x7f) {       /* vla fornow..... */
#else
         if ( c = rl_shift ( b, (short)i ) & 0x7f ) {
#endif
				if (c == '\n')
					(*emit)('\r', bp);
				(*emit)((char)c, bp);
			}
		break;
	case 's':
		s = va_arg(adx, char _FAR_ *);
		while (c = *s++) {
			if (c == '\n')
				(*emit)('\r', bp);
			(*emit)((char)c, bp);
		}
		break;

	case '%':
		(*emit)('%', bp);
		break;
	}
	goto loop;
out:
	va_end(x1);
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void
_printn(u_long n, long b, long width, short pad, void (*emit)(char, char _FAR_ * _FAR_ *), char  _FAR_ * _FAR_ *bp)
{
	char prbuf[40];
	register char _FAR_ *cp;

	if (b == 10 && (long)n < 0) {
		(*emit)('-', bp);
		n = (unsigned)(-(long)n);
	}
	cp = prbuf;
	do {
#ifdef MSC_DEPENDENCY
		*cp++ = "0123456789abcdef"[n%b];       /* vla fornow..... */
		n /= b;                                /* vla fornow..... */
#else
      *cp++ = "0123456789abcdef"[(u_long)us_mod ( n, (short)b ) ];
      n = us_div ( n, (short)b );
#endif
		width--;
	} while (n);
	while (width-- > 0)
		*cp++ = (char)pad;
	do {
		(*emit)(*--cp, bp);
	} while (cp > prbuf);
}
