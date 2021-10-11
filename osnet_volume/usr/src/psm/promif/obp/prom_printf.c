/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_printf.c	1.10	96/10/27 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/varargs.h>

static void _doprint(const char *, va_list, void (*)(char, char **), char **);
static void _printn(uint64_t, int, int, int, void (*)(char, char **), char **);

/*
 * Emit character functions...
 */

/*ARGSUSED*/
static void
_pput(char c, char **p)
{
	(void) prom_putchar(c);
}

static void
_sput(char c, char **p)
{
	**p = c;
	*p += 1;
}

/*VARARGS1*/
void
prom_printf(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	(void) _doprint(fmt, adx, _pput, (char **)0);
	va_end(adx);
}

void
prom_vprintf(const char *fmt, va_list adx)
{
	(void) _doprint(fmt, adx, _pput, (char **)0);
}

/*VARARGS2*/
char *
prom_sprintf(char *s, const char *fmt, ...)
{
	char *bp = s;
	va_list adx;

	va_start(adx, fmt);
	(void) _doprint(fmt, adx, _sput, &bp);
	*bp++ = (char)0;
	va_end(adx);
	return (s);
}

char *
prom_vsprintf(char *s, const char *fmt, va_list adx)
{
	char *bp = s;

	(void) _doprint(fmt, adx, _sput, &bp);
	*bp++ = (char)0;
	return (s);
}

static void
_doprint(const char *fmt, va_list adx, void (*emit)(char, char **), char **bp)
{
	int b, c, i, pad, width, ells;
	register char *s;
	int64_t	l;
	uint64_t ul;

loop:
	width = 0;
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
		if (c == '\n')
			(*emit)('\r', bp);
		(*emit)(c, bp);
	}

	c = *fmt++;

	for (pad = ' '; c == '0'; c = *fmt++)
		pad = '0';

	for (width = 0; c >= '0' && c <= '9'; c = *fmt++)
		width = (width * 10) + (c - '0');

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
			(*emit)('-', bp);
			width--;
			ul = -l;
		} else
			ul = l;
		goto number;

	case 'p':
		ells = 1;
		/* FALLTHROUGH */
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
			ul = (uint64_t)va_arg(adx, u_int);
		else if (ells == 1)
			ul = (uint64_t)va_arg(adx, u_long);
		else
			ul = (uint64_t)va_arg(adx, uint64_t);
number:
		_printn(ul, b, width, pad, emit, bp);
		break;

	case 'c':
		b = va_arg(adx, int);
		for (i = 24; i >= 0; i -= 8)
			if ((c = ((b >> i) & 0x7f)) != 0) {
				if (c == '\n')
					(*emit)('\r', bp);
				(*emit)(c, bp);
			}
		break;
	case 's':
		s = va_arg(adx, char *);
		while ((c = *s++) != 0) {
			if (c == '\n')
				(*emit)('\r', bp);
			(*emit)(c, bp);
		}
		break;

	case '%':
		(*emit)('%', bp);
		break;
	}
	goto loop;
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void
_printn(uint64_t n, int b, int width, int pad, void (*emit)(char, char **),
	char **bp)
{
	char prbuf[40];
	register char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
		width--;
	} while (n);
	while (width-- > 0)
		*cp++ = (char)pad;
	do {
		(*emit)(*--cp, bp);
	} while (cp > prbuf);
}
