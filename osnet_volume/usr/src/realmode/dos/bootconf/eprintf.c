/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * eprintf.c -- eprintf library routine
 */

#ident "@(#)eprintf.c   1.7   97/03/21 SMI"

/*
 *    This file contains the primary realmode printf engine, "eprintf".  We
 *    use this hand-crafted version of printf rather than the one from the
 *    C library for the following reasons:
 *
 *      It's smaller:
 *
 *          We don't support floating point format specifiers, which greatly
 *          reduces library bloat.
 *
 *      It supports X/Open argument repositioning:
 *
 *          This aids in internationalizing messages built piecemeal with
 *          printf's.
 *
 *	It supports the %! and %~ specifications:
 *
 *	    These were added to help make error messages easier to print.
 *	    %! prints the string corresponding to the value of errno when
 *	    eprintf was entered (i.e. like perror()). %~ prints "bootconf".
 *
 *      It uses minimal stdio features:
 *
 *          The only stdio routines used are in the write_func callback routine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "eprintf.h"

static struct printf_state {
	/* Recursive printf state struct ...				    */

	int  width;			/* Format modifiers:    Width	    */
	int  precision;			/*			Precision   */
	int  rjust;			/* Right justify if non-zero	    */
	int  pos;			/* Current arg list position	    */
	char fill;			/* Fill character		    */
} epfs;

/*
 * emit -- output a string of bytes
 *
 * This routine prints the "n"-byte text string at "cp" to the output
 * medium defined by "write_func".  The value returned is the number
 * of bytes transferred.
 *
 * If the string length ("n" argument) is given a zero, we output all
 * input bytes up to a trailing null.
 */

static int
emit(int (*write_func)(void *arg, char *ptr, int len), void *arg,
    const char *cp, int n, int m)

{
	int len;
	int outcount = 0;
	char fc;

	if ((len = n) == 0) {
		const char *ep = cp;

		/*
		 * If length is unspecified, find length of the input string.
		 * We can't use "_fstrlen" here because we don't want to drag
		 * in any routines from the C library.
		 */
		while (*ep++)
			len++;
	}

	if (epfs.rjust == 0) {
		/*
		 *  If result is to be left justified, compute the number of
		 *  trailing spaces we need to append.
		 */

		epfs.rjust = epfs.width - len;
		epfs.width = len;
	}

	while ((len < epfs.width--) && epfs.precision) {
		/*
		 * If the number of bytes to print is less than the specified
		 * field width, add enough "fill" characters to make up the
		 * difference.  If we need to place a minus sign, it becomes
		 * the first fill char.
		 */
		fc = ((epfs.fill == ' ') && (m-- > 0)) ? '-' : epfs.fill;

		if ((*write_func)(arg, &fc, 1) != 1)
			return (outcount);
		outcount++;
		if (epfs.precision)
			epfs.precision -= 1;
	}

	for (n = len; len-- && epfs.precision; cp++) {
		/*
		 * While we've still got bytes to print, either "putc" them to
		 * the output buffer or copy them directly into the receiving
		 * string.  If we haven't printed a required minus sign yet,
		 * it will be the first character we output.
		 */
		fc = (m-- > 0) ? (len++, cp--, '-') : *cp;

		if ((*write_func)(arg, &fc, 1) != 1)
			return (outcount);
		outcount++;
		if (epfs.precision)
			epfs.precision -= 1;
	}

	fc = ' ';
	while ((epfs.rjust-- > 0) && ((*write_func)(arg, &fc, 1) == 1));
	return (outcount);
}

/*
 * getarg -- get a printf argument
 *
 * This routine extracts an arg from the list.  The "islong" flags tells
 * us if we're extracting an int-sized arg or a long-sized arg.
 */

static long
getarg(va_list ap, int islong)
{
	long x;
	int n = epfs.pos;

	while (n-- > 0)
		x = va_arg(ap, int);
	x = (islong ? va_arg(ap, long) : (long)va_arg(ap, int));

	return (x);
}

/*
 * getnum -- extract ASCII numerics
 *
 * This routine is used to extract numeric values from a format specifier
 * string or the arg list.  Numeric values may appear in two places:  As
 * position numbers ("%$<num>"), and as field width/precision specifiers
 * ("%<num>[.<num>]).
 *
 * Routine returns the extracted value and advances the format pointer
 * at "*cpp" beyond the last ASCII digit.  It returns -1 if there is no
 * ASCII numeric string at the current position in the format spec.
 */

static int
getnum(const char **cpp, va_list ap)
{
	int c;
	int n = -1;

	if (**cpp == '*') {
		/*
		 * The value is stored in the arg list.  Use "getarg" to
		 * extract it and increment position pointers for both the
		 * format string ("*cpp") and the arg list ("pos").
		 */
		n = getarg(ap, 0);
		(*cpp) += 1;
		epfs.pos++;

	} else
		while (((c = **cpp) >= '0') && (c <= '9')) {
			/*
			 * As long as we have ASCII digits in the input string,
			 * add them to our running total ("n" register) and
			 * advance the format spec pointer.
			 */
			n = ((n > 0) ? (n * 10) : 0) + (c - '0');
			(*cpp)++;
		}

	return (n);
}

/*
 * eprintf -- primary printf engine
 *
 * This is the primary printf routine; other printf variants
 * simply call this routine with the appropriate arguments.
 */

int
eprintf(int (*write_func)(void *arg, char *ptr, int len),
    void *arg, const char *fmt, va_list ap)
{
	int n = 0;	/* Number of bytes written (return code) */
	int err = errno;
	struct printf_state xpfs = epfs;

	epfs.pos = 0;	/* Re-initialize arg list position */
	for (;;) {
		const char *digtab = "0123456789abcdef";
		int *np = &epfs.width;	/* Default format modifier */
		int base = 10;		/* Default integer base */
		int j, k = 0;
		char c, *cp;
		int dl = 0;

		/*
		 * Keep processing until we reach the end of the format
		 * string.  The outer loop is executed once for each format
		 * specification that we must interpret.  Hence, we must
		 * re-initialize various control variables ...
		 */

		epfs.fill = ' ';  /* Set default format modifiers */
		epfs.rjust = epfs.precision = epfs.width = -1;

		while (*fmt && (*fmt++ != '%')) {
			/*
			 * Pass all characters up to the next format escape
			 * directly to the output stream.  There's no point
			 * in being more efficient than this since "emit"
			 * spits things out a byte at a time anyway!
			 */
			n += emit(write_func, arg, fmt-1, 1, 0);
		}

		while (!k)
			switch (c = *fmt++) {
			/*
			 * Process the next input argument.  The "k" register
			 * remains zero while we process format modifiers
			 * (e.g, "%l.."), the "c" register contains the next
			 * escaped character.
			 */
			case '.':
				/*
				 * A dot switches format modifier from width
				 * to precision.  We'll pick up the new value
				 * in the next loop iteration.
				 */
				np = &epfs.precision;
				continue;

			case '-':
				/*
				 * Left justify the result ...
				 */

				epfs.rjust = 0;
				continue;

			case '0':
				/*
				 * If first digit of the width specifier is a
				 * zero, we left justify with zeros rather
				 * than spaces.
				 */
				if (np == &epfs.width) epfs.fill = '0';
				/*FALLTHROUGH*/

			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9': case '*':
				/*
				 * A format modifier.  Back the input pointer
				 * up to the first digit and use "getnum" to
				 * extract the modifier value.
				 */
				fmt -= (c != '0');
				*np = getnum(&fmt, ap);

				if ((c != '*') && (*fmt == '$')) {
					/*
					 * If this is an X/Open argument
					 * position specification, compute
					 * new "pos"ition register and reset
					 * other regs.
					 */
					epfs.pos = *np - 1;
					epfs.fill = ' ';
					*np = -1;
					fmt++;
				}
				continue;

			case 'l': case 'L':
				/*
				 * Long argument specifier.  Make sure the
				 * "dl" register is non-zero to indicate that
				 * "d"ata is "l"ong.
				 */
				dl = 1;
				continue;

			case 'h':
				/*
				 * Short argument specifier.  Note that we
				 * don't bother to check for mutually
				 * exclusive length specifiers.  If user mixes
				 * them in the same format spec, we'll use the
				 * last one we see.
				 */
				dl = 0;
				continue;

			case 'b':
				/*
				 * Binary integers:  OK you structured
				 * programming weenies, what's worse:  A bunch
				 * of goto's or an arithmetic expression spread
				 * across four arms of an case statement?
				 */
				base = 4;
				/*FALLTHROUGH*/

			case 'o':
				/*
				 * Octal integers:
				 * Have you figured this out yet?
				 */
				base -= 8;
				/*FALLTHROUGH*/

			case 'p': case 'X':
				/*
				 * Hex intergers with upper case digits
				 * (includes the ANSI %p specifier).
				 */
				digtab = "0123456789ABCDEF";
				/*FALLTHROUGH*/

			case 'x':
				/*
				 * Hex integers:  All right, I'll explain it
				 * to you.  The default base is decimal (10),
				 * but if I add 6 I get hex (10).  If I
				 * subtract 8 and then add 6 I get octal
				 * (because 10-8+6 = 8), and if I start with
				 * 4, subtract 8 and add 6 I get binary
				 * (4-8+6 = 2)!
				 */
				base += 6;
				/*FALLTHROUGH*/

			case 'u': case 'd':
			{
				int neg;
				long val;
				unsigned long uval;
				int rem;
				char num[128];
				/*
				 * Integer values:  Use the old "print
				 * remainders in a divide loop" technique to
				 * format intger values into a local ASCII
				 * "num"eric buffer which we then "emit"
				 * in the standard way.
				 */
				cp = &num[sizeof (num)];
				*--cp = j = 0;

				/*
				 * the 'd' format is signed,
				 * everything else is unsigned.
				 */
				if (c == 'd') {
					val = getarg(ap, dl);
					if ((neg = (val < 0)) != 0)
						val = -val;
					while (val) {
						j++;	/* output byte count */
						rem = val % base;
						val = val / base;
						*--cp = digtab[rem];
					}
				} else {
					uval = (unsigned long)getarg(ap, dl);
					/*CONSTANTCONDITION*/
					if ((dl == 0) && (sizeof (int) == 2))
						uval &= 0xffffUL;
					neg = 0;
					while (uval) {
						j++;	/* output byte count */
						rem = uval % base;
						uval = uval / base;
						*--cp = digtab[rem];
					}

				}

				while (((j < epfs.precision) || !*cp) &&
				    (cp > num))  {
					/*
					 * If we haven't reached the requested
					 * precision, add leading zeros until
					 * we do.  Results may be a bit strange
					 * if we run out of buffer, but who
					 * wants more than 127 digits of
					 * precision?
					 */
					*--cp = '0';
					j++;
				}

				n += emit(write_func, arg, cp, j, neg);
				goto fx;
			}

			case 's':
			{
				char *xp;

				/*
				 * Character strings:  "emit" does the work.
				 * use sizeof to determine if a "char *" is
				 * a long argument when calling getarg().
				 */
				/*CONSTANTCONDITION*/
				dl = dl || (sizeof (char *) == sizeof (long));
				if (!(xp = (char *)getarg(ap, dl)))
					xp = "(nil)";
				n += emit(write_func, arg, xp, 0, 0);
				goto fx;
			}

			case 'c':
				/*
				 * Print the next character from the arg list.
				 */
				c = getarg(ap, 0);
				goto fz;

			case '!':
			{
				char *xp;
				char unknown[100];

				if ((xp = strerror(err)) == NULL) {
					(void) sprintf(unknown,
					    "(errno %d)", err);
					xp = unknown;
				}
				n += emit(write_func, arg, xp, 0, 0);
				goto fx;
			}

			case '~':
				n += emit(write_func, arg, "bootconf", 0, 0);
				goto fx;

			default:
				/*
				 * Unrecognized escape character (or, perhaps
				 * it's %%).  Print it without interpretation.
				 */
				dl = -1;
			fz:
				epfs.precision = epfs.width = -1;

				/* Print next char from fmt string! */
				n += emit(write_func, arg, &c, 1, 0);

			fx:
				/*CONSTANTCONDITION*/
				if (sizeof (long) == sizeof (int))
					epfs.pos++;
				else
					epfs.pos += (dl+1);
				k = 100;
				break;

			case '\0':
				/*
				 * Null means we've reached the end of
				 * the format string.  Return number of
				 * bytes written to the caller.
				 */
				epfs = xpfs;
				return (n);
			}
	}
	/*NOTREACHED*/
}
