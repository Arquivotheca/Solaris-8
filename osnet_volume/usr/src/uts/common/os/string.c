/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)string.c	1.2	99/05/06 SMI"

#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/systm.h>

#define	ADDCHAR(c)	if (bufp++ - buf < buflen) bufp[-1] = (c)

/*
 * Given a buffer 'buf' of size 'buflen', render as much of the string
 * described by <fmt, args> as possible.  The string will always be
 * null-terminated, so the maximum string length is 'buflen - 1'.
 * Returns the number of bytes that would be necessary to render the
 * entire string, not including null terminator (just like vsnprintf(3S)).
 * To determine buffer size in advance, use vsnprintf(NULL, 0, fmt, args) + 1.
 */
size_t
vsnprintf(char *buf, size_t buflen, const char *fmt, va_list args)
{
	uint64_t ul, tmp;
	char *bufp = buf;	/* current buffer pointer */
	int pad, width, ells, base, sign, c;
	char *digits, *sp, *bs;
	char numbuf[65];	/* sufficient for a 64-bit binary value */

	if ((ssize_t)buflen < 0)
		buflen = 0;

	while ((c = *fmt++) != '\0') {
		if (c != '%') {
			ADDCHAR(c);
			continue;
		}

		if ((c = *fmt++) == '\0')
			break;

		for (pad = ' '; c == '0'; c = *fmt++)
			pad = '0';

		for (width = 0; c >= '0' && c <= '9'; c = *fmt++)
			width = width * 10 + c - '0';

		for (ells = 0; c == 'l'; c = *fmt++)
			ells++;

		digits = "0123456789abcdef";

		if (c >= 'A' && c <= 'Z') {
			c += 'a' - 'A';
			digits = "0123456789ABCDEF";
		}

		base = sign = 0;

		switch (c) {
		case 'd':
			sign = 1;
			/*FALLTHROUGH*/
		case 'u':
			base = 10;
			break;
		case 'p':
			ells = 1;
			/*FALLTHROUGH*/
		case 'x':
			base = 16;
			break;
		case 'o':
			base = 8;
			break;
		case 'b':
			ells = 0;
			base = 1;
			break;
		case 'c':
			ul = (int64_t)va_arg(args, int);
			ADDCHAR((int)ul & 0x7f);
			break;
		case 's':
			sp = va_arg(args, char *);
			if (sp == NULL)
				sp = "<null string>";
			while ((c = *sp++) != 0)
				ADDCHAR(c);
			break;
		case '%':
			ADDCHAR('%');
			break;
		}

		if (base == 0)
			continue;

		if (ells == 0)
			ul = (int64_t)va_arg(args, int);
		else if (ells == 1)
			ul = (int64_t)va_arg(args, long);
		else
			ul = (int64_t)va_arg(args, int64_t);

		if (sign && (int64_t)ul < 0)
			ul = -ul;
		else
			sign = 0;

		if (ells < 8 / sizeof (long))
			ul &= 0xffffffffU;

		if (c == 'b') {
			bs = va_arg(args, char *);
			base = *bs++;
		}

		tmp = ul;
		do {
			width--;
		} while ((tmp /= base) != 0);

		if (sign && pad == '0')
			ADDCHAR('-');
		while (width-- > sign)
			ADDCHAR(pad);
		if (sign && pad == ' ')
			ADDCHAR('-');

		sp = numbuf;
		tmp = ul;
		do {
			*sp++ = digits[tmp % base];
		} while ((tmp /= base) != 0);

		while (sp > numbuf) {
			sp--;
			ADDCHAR(*sp);
		}

		if (c == 'b' && ul != 0) {
			int any = 0;
			c = *bs++;
			while (c != 0) {
				if (ul & (1 << (c - 1))) {
					if (any++ == 0)
						ADDCHAR('<');
					while ((c = *bs++) >= 32)
						ADDCHAR(c);
					ADDCHAR(',');
				} else {
					while ((c = *bs++) >= 32)
						continue;
				}
			}
			if (any) {
				bufp--;
				ADDCHAR('>');
			}
		}
	}
	if (bufp - buf < buflen)
		bufp[0] = c;
	else if (buflen != 0)
		buf[buflen - 1] = c;
	return (bufp - buf);
}

char *
vsprintf(char *buf, const char *fmt, va_list args)
{
	(void) vsnprintf(buf, INT_MAX, fmt, args);
	return (buf);
}

/*PRINTFLIKE1*/
size_t
snprintf(char *buf, size_t buflen, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	buflen = vsnprintf(buf, buflen, fmt, args);
	va_end(args);

	return (buflen);
}

/*PRINTFLIKE2*/
char *
sprintf(char *buf, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsnprintf(buf, INT_MAX, fmt, args);
	va_end(args);

	return (buf);
}

/*
 * Historical entry point: remove in Solaris 2.8.
 */
char *
vsprintf_len(size_t buflen, char *buf, const char *fmt, va_list args)
{
	(void) vsnprintf(buf, buflen, fmt, args);
	return (buf);
}

/*
 * Historical entry point: remove in Solaris 2.8.
 */
/*PRINTFLIKE3*/
char *
sprintf_len(size_t buflen, char *buf, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsnprintf(buf, buflen, fmt, args);
	va_end(args);

	return (buf);
}

/*
 * Simple-minded conversion of a long into a null-terminated character
 * string.  Caller must ensure there's enough space to hold the result.
 */
void
numtos(unsigned long num, char *s)
{
	char prbuf[40];

	char *cp = prbuf;

	do {
		*cp++ = "0123456789"[num % 10];
		num /= 10;
	} while (num);

	do {
		*s++ = *--cp;
	} while (cp > prbuf);
	*s = '\0';
}

/*
 * Returns the integer value of the string of decimal numeric
 * chars beginning at **str.  Does no overflow checking.
 * Note: updates *str to point at the last character examined.
 */
int
stoi(char **str)
{
	char	*p = *str;
	int	n;
	int	c;

	for (n = 0; (c = *p) >= '0' && c <= '9'; p++) {
		n = n * 10 + c - '0';
	}
	*str = p;
	return (n);
}

/*
 * libc string routines.  Some are in the DDI, some are not; but in either
 * case, strfoo() is indentical to strfoo(3C) unless otherwise noted below.
 */
char *
strcat(char *s1, const char *s2)
{
	size_t len;

	(void) knstrcpy(s1 + strlen(s1), s2, &len);
	return (s1);
}

char *
strchr(const char *sp, int c)
{
	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (NULL);
}

int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == s2)
		return (0);
	n++;
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return ((n == 0) ? 0 : *s1 - *--s2);
}

static const char charmap[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

int
strcasecmp(const char *s1, const char *s2)
{
	const char *cm = charmap;

	while (cm[*s1] == cm[*s2++])
		if (*s1++ == '\0')
			return (0);
	return ((unsigned char)cm[*(unsigned char *)s1] -
		(unsigned char)cm[*(unsigned char *)--s2]);
}

int
strncasecmp(const char *s1, const char *s2, size_t n)
{
	const char *cm = charmap;

	while (n != 0 && cm[*s1] == cm[*s2++]) {
		if (*s1++ == '\0')
			return (0);
		n--;
	}
	return (n == 0 ? 0 :
		(unsigned char)cm[*(unsigned char *)s1] -
		(unsigned char)cm[*(unsigned char *)--s2]);
}

char *
strcpy(char *s1, const char *s2)
{
	size_t len;

	return (knstrcpy(s1, s2, &len));
}

char *
strncpy(char *s1, const char *s2, size_t n)
{
	char *os1 = s1;

	n++;
	while (--n != 0 && (*s1++ = *s2++) != '\0')
		;
	if (n != 0)
		while (--n != 0)
			*s1++ = '\0';
	return (os1);
}

char *
strrchr(const char *sp, int c)
{
	char *r = NULL;

	do {
		if (*sp == (char)c)
			r = (char *)sp;
	} while (*sp++);

	return (r);
}

/*
 * Like strrchr(), except
 * (a) it takes a maximum length for the string to be searched, and
 * (b) if the string ends with a null, it is not considered part of the string.
 */
char *
strnrchr(const char *sp, int c, size_t n)
{
	const char *r = 0;

	while (n-- > 0 && *sp) {
		if (*sp == c)
			r = sp;
		sp++;
	}

	return ((char *)r);
}

char *
strstr(const char *as1, const char *as2)
{
	const char *s1, *s2;
	const char *tptr;
	char c;

	s1 = as1;
	s2 = as2;

	if (s2 == NULL || *s2 == '\0')
		return ((char *)s1);
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c)
				;
			if (c == 0)
				return ((char *)tptr - 1);
			s1 = tptr;
			s2 = as2;
			c = *s2;
		}

	return (NULL);
}
