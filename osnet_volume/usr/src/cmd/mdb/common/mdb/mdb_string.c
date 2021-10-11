/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_string.c	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <limits.h>

#include <mdb/mdb_string.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_lex.h>

/*
 * Convert the specified integer value to a string represented in the given
 * base.  The flags parameter is a bitfield of the formatting flags defined in
 * mdb_string.h.  A pointer to a static conversion buffer is returned.
 */
const char *
numtostr(uintmax_t uvalue, int base, uint_t flags)
{
	static const char ldigits[] = "0123456789abcdef";
	static const char udigits[] = "0123456789ABCDEF";

	static char buf[68]; /* Enough for ULLONG_MAX in binary plus prefixes */

	const char *digits = (flags & NTOS_UPCASE) ? udigits : ldigits;
	int i = sizeof (buf);

	intmax_t value = (intmax_t)uvalue;
	int neg = (flags & NTOS_UNSIGNED) == 0 && value < 0;
	uintmax_t rem = neg ? -value : value;

	buf[--i] = 0;

	do {
		buf[--i] = digits[rem % base];
		rem /= base;
	} while (rem != 0);

	if (flags & NTOS_SHOWBASE) {
		uintmax_t lim;
		char c = 0;

		switch (base) {
		case 2:
			lim = 1;
			c = 'i';
			break;
		case 8:
			lim = 7;
			c = 'o';
			break;
		case 10:
			lim = 9;
			c = 't';
			break;
		case 16:
			lim = 9;
			c = 'x';
			break;
		}

		if (c != 0 && uvalue > lim) {
			buf[--i] = c;
			buf[--i] = '0';
		}
	}

	if (neg)
		buf[--i] = '-';
	else if (flags & NTOS_SIGNPOS)
		buf[--i] = '+';

	return ((const char *)(&buf[i]));
}

/*
 * Convert a string to an unsigned integer value using the specified base.
 * In the event of overflow or an invalid character, we generate an
 * error message and longjmp back to the main loop using yyerror().
 */
uintmax_t
strtonum(const char *s, int base)
{
#define	CTOI(x)	(((x) >= '0' && (x) <= '9') ? (x) - '0' : \
	((x) >= 'a' && (x) <= 'z') ? (x) + 10 - 'a' : (x) + 10 - 'A')

	uintmax_t multmax = (uintmax_t)ULLONG_MAX / (uintmax_t)(uint_t)base;
	uintmax_t val = 0;
	int c, i, neg = 0;

	switch (c = *s) {
	case '-':
		neg++;
		/*FALLTHRU*/
	case '+':
		c = *++s;
	}

	if (c == '\0')
		goto done;

	if ((val = CTOI(c)) >= base)
		yyerror("digit '%c' is invalid in current base\n", c);

	for (c = *++s; c != '\0'; c = *++s) {
		if (val > multmax)
			goto oflow;

		if ((i = CTOI(c)) >= base)
			yyerror("digit '%c' is invalid in current base\n", c);

		val *= base;

		if ((uintmax_t)ULLONG_MAX - val < (uintmax_t)i)
			goto oflow;

		val += i;
	}
done:
	return (neg ? -val : val);
oflow:
	yyerror("specified value exceeds maximum immediate value\n");
	return ((uintmax_t)ULLONG_MAX);
}

/*
 * Return a boolean value indicating whether or not a string consists
 * solely of characters which are digits 0..9.
 */
int
strisnum(const char *s)
{
	for (; *s != '\0'; s++) {
		if (*s < '0' || *s > '9')
			return (0);
	}

	return (1);
}

/*
 * Quick string to integer (base 10) conversion function.  This performs
 * no overflow checking and is only meant for internal mdb use.
 */
int
strtoi(const char *s)
{
	int c, n;

	for (n = 0; (c = *s) >= '0' && c <= '9'; s++)
		n = n * 10 + c - '0';

	return (n);
}

/*
 * Create a copy of string s using the mdb allocator interface.
 */
char *
strdup(const char *s)
{
	char *s1 = mdb_alloc(strlen(s) + 1, UM_SLEEP);

	(void) strcpy(s1, s);
	return (s1);
}

/*
 * Create a copy of string s, but only duplicate the first n bytes.
 */
char *
strndup(const char *s, size_t n)
{
	char *s2 = mdb_alloc(n + 1, UM_SLEEP);

	(void) strncpy(s2, s, n);
	s2[n] = '\0';
	return (s2);
}

/*
 * Convenience routine for freeing strings.
 */
void
strfree(char *s)
{
	mdb_free(s, strlen(s) + 1);
}

/*
 * Transform string s inline, converting each embedded C escape sequence string
 * to the corresponding character.  For example, the substring "\n" is replaced
 * by an inline '\n' character.  The length of the resulting string is returned.
 */
size_t
stresc2chr(char *s)
{
	char *p, *q, c;
	int esc = 0;

	for (p = q = s; (c = *p) != '\0'; p++) {
		if (esc) {
			switch (c) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c -= '0';
					p++;

					if (*p >= '0' && *p <= '7') {
						c = c * 8 + *p++ - '0';

						if (*p >= '0' && *p <= '7')
							c = c * 8 + *p - '0';
						else
							p--;
					} else
						p--;

					*q++ = c;
					break;

				case 'a':
					*q++ = '\a';
					break;
				case 'b':
					*q++ = '\b';
					break;
				case 'f':
					*q++ = '\f';
					break;
				case 'n':
					*q++ = '\n';
					break;
				case 'r':
					*q++ = '\r';
					break;
				case 't':
					*q++ = '\t';
					break;
				case 'v':
					*q++ = '\v';
					break;
				case '"':
				case '\\':
					*q++ = c;
					break;
				default:
					*q++ = '\\';
					*q++ = c;
			}

			esc = 0;

		} else {
			if ((esc = c == '\\') == 0)
				*q++ = c;
		}
	}

	*q = '\0';
	return ((size_t)(q - s));
}

/*
 * Create a copy of string s in which certain unprintable or special characters
 * have been converted to the string representation of their C escape sequence.
 * For example, the newline character is expanded to the string "\n".
 */
char *
strchr2esc(const char *s)
{
	const char *p;
	char *q, *s2, c;
	size_t n = 0;

	for (p = s; (c = *p) != '\0'; p++) {
		switch (c) {
		case '\a':
		case '\b':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
		case '"':
		case '\\':
			n++;		/* 1 add'l char needed to follow \ */
			break;
		case ' ':
			break;
		default:
			if (c < '!' || c > '~')
				n += 3;	/* 3 add'l chars needed following \ */
		}
	}

	s2 = mdb_alloc(strlen(s) + n + 1, UM_SLEEP);

	for (p = s, q = s2; (c = *p) != '\0'; p++) {
		switch (c) {
		case '\a':
			*q++ = '\\';
			*q++ = 'a';
			break;
		case '\b':
			*q++ = '\\';
			*q++ = 'b';
			break;
		case '\f':
			*q++ = '\\';
			*q++ = 'f';
			break;
		case '\n':
			*q++ = '\\';
			*q++ = 'n';
			break;
		case '\r':
			*q++ = '\\';
			*q++ = 'r';
			break;
		case '\t':
			*q++ = '\\';
			*q++ = 't';
			break;
		case '\v':
			*q++ = '\\';
			*q++ = 'v';
			break;
		case '"':
			*q++ = '\\';
			*q++ = '"';
			break;
		case '\\':
			*q++ = '\\';
			*q++ = '\\';
			break;
		case ' ':
			*q++ = c;
			break;
		default:
			if (c < '!' || c > '~') {
				*q++ = '\\';
				*q++ = ((c >> 6) & 7) + '0';
				*q++ = ((c >> 3) & 7) + '0';
				*q++ = (c & 7) + '0';
			} else
				*q++ = c;
		}
	}

	*q = '\0';
	return (s2);
}

/*
 * Split the string s at the first occurrence of character c.  This character
 * is replaced by \0, and a pointer to the remainder of the string is returned.
 */
char *
strsplit(char *s, char c)
{
	char *p;

	if ((p = strchr(s, c)) == NULL)
		return (NULL);

	*p++ = '\0';
	return (p);
}

/*
 * Same as strsplit, but split from the last occurrence of character c.
 */
char *
strrsplit(char *s, char c)
{
	char *p;

	if ((p = strrchr(s, c)) == NULL)
		return (NULL);

	*p++ = '\0';
	return (p);
}

/*
 * Return the address of the first occurrence of any character from s2
 * in the string s1, or NULL if none exists.  This is similar to libc's
 * strpbrk, but we add a third parameter to limit the search to the
 * specified number of bytes in s1, or a \0 character, whichever is
 * encountered first.
 */
const char *
strnpbrk(const char *s1, const char *s2, size_t nbytes)
{
	const char *p;

	if (nbytes == 0)
		return (NULL);

	do {
		for (p = s2; *p != '\0' && *p != *s1; p++)
			continue;

		if (*p != '\0')
			return (s1);

	} while (--nbytes != 0 && *s1++ != '\0');

	return (NULL);
}

/*
 * Return the basename (name after final /) of the given string.  We use
 * strbasename rather than basename to avoid conflicting with libgen.h's
 * non-const function prototype.
 */
const char *
strbasename(const char *s)
{
	const char *p = strrchr(s, '/');

	if (p == NULL)
		return (s);

	return (++p);
}

/*
 * Return the directory name (name prior to the final /) of the given string.
 * The string itself is modified.
 */
char *
strdirname(char *s)
{
	static char slash[] = "/";
	static char dot[] = ".";
	char *p;

	if (s == NULL || *s == '\0')
		return (dot);

	for (p = s + strlen(s); p != s && *--p == '/'; )
		continue;

	if (p == s && *p == '/')
		return (slash);

	while (p != s) {
		if (*--p == '/') {
			while (*p == '/' && p != s)
				p--;
			*++p = '\0';
			return (s);
		}
	}

	return (dot);
}

/*
 * Return a pointer to the first character in the string that makes it an
 * invalid identifer (i.e. incompatible with the mdb syntax), or NULL if
 * the string is a valid identifier.
 */
const char *
strbadid(const char *s)
{
	return (strpbrk(s, "#%^&*-+=,:$/\\?<>;|!`'\"[]\n\t() {}"));
}
