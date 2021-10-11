/*
 *  Copyright (c) 1996 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Realmode printf support:
 *
 *    This file contains the primary realmode printf engine, "vfprintf".  We
 *    use this hand-crafted version of printf rather than the one from the
 *    Microsoft C library for the following reasons:
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
 *      It supports the "%ls" specification:
 *
 *          Use of the "l" modifier in string format specifiers allows one
 *          to print "far" strings buffers without having to first copy them
 *          into a local buffer.
 *
 *      It uses minimal stdio features:
 *
 *          The only stdio routine called from the realmode printf routines
 *          is fputc.  This routine can be implemented with a low-level
 *          "putchar", thereby allowing printf to be called from realmode
 *          drivers that don't have access to the full library.
 */

#ident "<@(#)vfprintf.c	1.7	96/08/28	SMI>"
#include <dostypes.h>
#include <stdarg.h>
#include <stdio.h>

static struct printf_state {
	/* Recursive printf state struct ...				    */

	int  width;			/* Format modifiers:    Width	    */
	int  precision;			/*			Precision   */
	int  pos;			/* Current arg list position	    */
	char fill;			/* Fill character		    */
} epfs;

static int
emit(char **spp, const char far *cp, int n, int m)
{
	/*
	 *  Output a string of bytes:
	 *
	 *  This routine prints the "n"-byte text string at "cp" to the output
	 *  medium defined by "spp".  If "spp" is null, the output medium is
	 *  the standard output file, otherwise it's the string at "*spp".
	 *
	 *  If the string length ("n" argument) is given a zero, we output all
	 *  input bytes up to a trailing null.  If the output medium is a
	 *  string, we emit the null as well (but don't include it in the
	 *  return count).
	 */

	int len;
	unsigned fc;

	if (!(len = n)) {
		/*
		 *  If length is unspecified, find length of the input string.
		 *  We can't use "_fstrlen" here because we don't want to drag
		 *  in any routines from the C library.
		 */

		const char *ep = cp;
		while (*ep++) len += 1;
	}

	while ((len < epfs.width--) && epfs.precision) {
		/*
		 *  If the number of bytes to print is less than the specified
		 *  field width, add enough "fill" characters to make up the
		 *  difference.  If we need to place a minus sign, it becomes
		 *  the first fill char.
		 */

		fc = ((epfs.fill == ' ') && (m-- > 0)) ? '-' : epfs.fill;

		if (epfs.precision) epfs.precision -= 1;
		if (*spp)
			*(*spp)++ = fc;
		else
			putchar((char)fc);
	}

	for (n = len; len-- && epfs.precision; cp++) {
		/*
		 *  While we've still got bytes to print, either "putchar" them
		 *  to the output buffer or copy them directly into the re-
		 *  ceiving string.  If we haven't printed a required minus
		 *  sign yet, it will be the the first character we output.
		 */

		fc = (m-- > 0) ? (len++, cp--, '-') : *cp;

		if (epfs.precision) epfs.precision -= 1;
		if (*spp)
			*(*spp)++ = fc;
		else
			putchar((char)fc);
	}

	if (*spp) **spp = 0;
	return (n);
}

static long
getarg(va_list ap, int dl)
{
	/*
	 *  Get a printf argument:
	 *
	 *  This routine extracts a "dl+1"-word value from the current position
	 *  in the caller's argument list and returns it as a "long".
	 */

	long x;
	int n = epfs.pos;

	while (n-- > 0) x = va_arg(ap, int);
	x = (dl ? va_arg(ap, long) : (long)va_arg(ap, int));
	return (x);
}

static int
getnum(const char **cpp, va_list ap)
{
	/*
	 *  Extract ASCII numerics:
	 *
	 *  This routine is used to extract numeric values from a format
	 *  specifier string or the arg list.  Numeric values may appear in
	 *  two places:  As position numbers ("%$<num>"), and as field width/
	 *  precision specifiers ("%<num>[.<num>]).
	 *
	 *  Routine returns the extracted value and advances the format pointer
	 *  at "*cpp" beyond the last ASCII digit.  It returns -1 if there is
	 *  no ASCII numeric string at the current position in the format spec.
	 */

	int c, n = -1;

	if (**cpp == '*') {
		/*
		 *  The value is stored in the arg list.  Use "getarg" to
		 *  extract it and increment position pointers for both the
		 *  format string ("*cpp") and the arg list ("pos").
		 */

		n = getarg(ap, sizeof (int));
		epfs.pos += 1;
		(*cpp) += 1;

	} else while (((c = **cpp) >= '0') && (c <= '9')) {
		/*
		 *  As long as we have ASCII digits in the input string, add
		 *  them to our running total ("n" register) and advance the
		 *  format spec pointer.
		 */

		n = ((n > 0) ? (n * 10) : 0) + (c - '0');
		(*cpp)++;
	}

	return (n);
}

int
vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	/*
	 *  Primary printf engine:
	 *
	 *  This is the primary realmode printf routine; other printf variants
	 *  simply call this routine with the appropriate arguments.
	 *
	 *  The syntax of print format strings recognized by this routine dif-
	 *  fers from the ANSI standard in the following ways:
	 *
	 *     1.  We don't support floating point format specs (%e, %f, or %g).
	 *     2.  We don't support alternate output forms (#, -, or +).
	 *     3.  We do support a %b binary integer form.
	 *     4.  We do support X/Open parameter repositioning ($ modifier)
	 *     5.  The "%ls" format spec. means input is a far pointer.
	 */

	struct printf_state xpfs = epfs;
	char far *xp;
	int n = 0;

	epfs.pos = 0;

	for (;;) {
		/*
		 *  Keep processing until we reach the end of the format
		 *  string.  The outter loop is executed once for each format
		 *  specification that we must interpret.  Hence, we must re-
		 *  initialize various control variables ...
		 */

		const char *digtab = "0123456789abcdef";
		int *np = &epfs.width;
		int base = 10;
		int j, k = 0;
		char c, *cp;
		int dl = 0;

		epfs.fill = ' ';  /* Set default format modifiers	    */
		epfs.precision = epfs.width = -1;

		while (*fmt && (*fmt++ != '%')) {
			/*
			 *  Pass all characters up to the next format escape
			 *  directly to the output stream.  There's no point
			 *  in being more efficient than this since "emit"
			 *  spits things out a byte at a time anyway!
			 */

			n += emit((char **)&fp, fmt-1, 1, 0);
		}

		while (k == 0) {
			/*
			 *  Process the next input argument.  The "k" register
			 *  remains zero while we process format modifiers
			 *  (e.g, "%l.."), the "c" register contains the next
			 *  escaped character.
			 */

			switch (c = *fmt++) {

			case '.':
				/*
				 *  A dot switches format modifier from width
				 *  to precision.  We'll pick up the new value
				 *  in the next loop iteration.
				 */

				np = &epfs.precision;
				continue;

			case '0':
				/*
				 *  If first digit of the width specifier is
				 *  a zero, we left justify with zeros rather
				 *  than spaces.
				 */

				if (np == &epfs.width) epfs.fill = '0';
				/*FALLTHROUGH*/

			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9': case '*':
				/*
				 *  A format modifier.  Back the input pointer
				 *  up to the first digit and use "getnum" to
				 *  extract the modifier value.
				 */

				fmt -= 1;
				*np = getnum(&fmt, ap);

				if ((c != '*') && (*fmt == '$')) {
					/*
					 *  If this is an X/Open argument pos-
					 *  ition specification, compute new
					 *  "pos"ition register and reset other
					 *  regs.
					 */

					epfs.pos = *np - 1;
					epfs.fill = ' ';
					*np = -1;
					fmt++;
				}

				continue;

			case 'l': case 'L':
				/*
				 *  Long argument specifier.  Make sure the
				 *  "dl" register is non-zero to indicate that
				 *  "d"ata is "l"ong.
				 */

				dl = 1;
				continue;

			case 'h':
				/*
				 *  Short argument specifier.  Note that we
				 *  don't bother to check for mutually exclu-
				 *  sive length specifiers.  If user mixes them
				 *  in the same format spec, we'll use the
				 *  last one we see.
				 */

				dl = 0;
				continue;

			case 'b':

				base = 4;	/* Binary integers ...	    */
				/*FALLTHROUGH*/

			case 'o':

				base -= 8;	/* Octal integers ...	    */
				/*FALLTHROUGH*/

			case 'p': case 'X':
				/*  Upper case hex and pointers ...	    */
				digtab = "0123456789ABCDEF";
				/*FALLTHROUGH*/

			case 'x':

				base += 6;	/* Lower case hex ints.	    */
				/*FALLTHROUGH*/

			case 'u': case 'd': {
				/*
				 *  Integer values:  Use the old "print rem-
				 *  ainders in a divide loop" technique to
				 *  format intger values into a local ASCII
				 *  "num"eric buffer which we then "emit" in
				 *  the standard way.
				 */

				int neg = 0;
				long val = getarg(ap, dl);
				char num[128]; cp = &num[sizeof (num)];

				if (c != 'd') val &= (dl ? -1L : 0xFFFF);
				else if (neg = (val < 0)) val = -val;
				*--cp = j = 0;

				while (val != 0) {
					/*
					 *  Divide loop:  Each iteration right-
					 *  shifts a digit out of the "val"ue
					 *  word and into the "nun" buffer.
					 */

					int rem;
					j++;

					_asm {
						/*
						 *  Expand the division by hand
						 *  to avoid a call to C lib's
						 *  long divide routine.
						 */

						mov   ax, word ptr [val+2]
						xor   dx, dx
						div   base
						mov   word ptr [val+2], ax
						mov   ax, word ptr [val]
						div   base
						mov   rem, dx
						mov   word ptr [val], ax
					}

					*--cp = digtab[rem];
				}

				while (((j < epfs.precision) || !*cp) &&
							    (cp > num)) {
					/*
					 *  If we haven't reached the requested
					 *  precision, add leading zeros until
					 *  we do.
					 */

					*--cp = '0';
					j++;
				}

				n += emit((char **)&fp, cp, j, neg);
				goto fx;
			}

			case 's':
				/*
				 *  Character strings:  "emit" does the dirty
				 *  work, all we have to do is convert near
				 *  pointers to far (and watch for nulls!).
				 */

				if (!(xp = (char far *)getarg(ap, dl))) {
					/* Don't blow up if ptr is null!    */
					xp = "(nil)";

				} else if (dl == 0) _asm {
					/* Convert short ptr to long!	    */
					mov word ptr [xp+2], ds
				}

				n += emit((char **)&fp, xp, 0, 0);
				goto fx;

			case 'c':
				/*
				 *  Print the next character from the arg list.
				 *  Note that we support the non-standard "%lc"
				 *  form as well!
				 */

				c = getarg(ap, dl);
				goto fz;

			default:
				/*
				 *  Unrecognized escape character (or, perhaps
				 *  it's %%).  Print without interpretation.
				 */

				dl = -1;
			fz:	epfs.precision = epfs.width = -1;
				n += emit((char **)&fp, &c, 1, 0);

			fx:	epfs.pos += (dl+1);
				k = 100;
				break;

			case '\0':
				/*
				 *  Null means we've reached the end of the
				 *  format string.  Return number of bytes
				 *  written to the caller.
				 */

				epfs = xpfs;
				return (n);
			}
		}
	}

	/*NOTREACHED*/
}
