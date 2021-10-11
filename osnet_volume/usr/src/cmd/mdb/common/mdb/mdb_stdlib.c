/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_stdlib.c	1.1	99/08/11 SMI"

#include <mdb/mdb_stdlib.h>
#include <mdb/mdb_string.h>

#include <sys/types.h>
#include <floatingpoint.h>
#include <poll.h>

/*
 * Post-processing routine for econvert and qeconvert.  This function is
 * called by both doubletos() and longdoubletos() below.
 */
static const char *
fptos(const char *p, char *buf, size_t buflen, int decpt, int sign, char expchr)
{
	char *q = buf;

	*q++ = sign ? '-' : '+';

	/*
	 * If the initial character is not a digit, the result is a special
	 * identifier such as "NaN" or "Inf"; just copy it verbatim.
	 */
	if (*p < '0' || *p > '9') {
		(void) strncpy(q, p, buflen);
		buf[buflen - 1] = '\0';
		return (buf);
	}

	*q++ = *p++;
	*q++ = '.';

	(void) strcpy(q, p);
	q += strlen(q);
	*q++ = expchr;

	if (--decpt < 0) {
		decpt = -decpt;
		*q++ = '-';
	} else
		*q++ = '+';

	if (decpt < 10)
		*q++ = '0';

	(void) strcpy(q, numtostr((uint_t)decpt, 10, 0));
	return (buf);
}

/*
 * Convert the specified double to a string, and return a pointer to a static
 * buffer containing the string value.  The double is converted using the
 * same formatting conventions as sprintf(buf, "%+.*e", precision, d).  The
 * expchr parameter specifies the character used to denote the exponent,
 * and is usually 'e' or 'E'.
 */
const char *
doubletos(double d, int precision, char expchr)
{
	static char buf[DECIMAL_STRING_LENGTH];
	char digits[DECIMAL_STRING_LENGTH];
	int decpt, sign;
	char *p;

	p = econvert(d, precision + 1, &decpt, &sign, digits);
	return (fptos(p, buf, sizeof (buf), decpt, sign, expchr));
}

/*
 * Same as doubletos(), but for long doubles (quad precision floating point).
 */
const char *
longdoubletos(long double *ldp, int precision, char expchr)
{
	static char buf[DECIMAL_STRING_LENGTH];
	char digits[DECIMAL_STRING_LENGTH];
	int decpt, sign;
	char *p;

	p = qeconvert(ldp, precision + 1, &decpt, &sign, digits);
	return (fptos(p, buf, sizeof (buf), decpt, sign, expchr));
}

/*
 * Convert a time_t into a 26-character string with constant-width fields
 * of the form "Fri Sep 13 00:00:00 1986\n\0".
 */
const char *
timetos(const time_t *tmp)
{
	return (ctime(tmp));
}

/*
 * Sleep for the specified number of milliseconds.
 */
void
delay(int msecs)
{
	(void) poll(NULL, 0, msecs);
}
