/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)echo.c 1.13	97/12/09 SMI"	/* SVr4.0 1.3   */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <limits.h>
#include <string.h>
#include <locale.h>

int
main(argc, argv)
int argc;
char **argv;
{

	register char	*cp;
	register int	i, wd;
	int	j;
	wchar_t		wc;
	int		b_len;
	char		*ep;

#ifdef	_iBCS2
	int		no_nl = 0;
#endif
	(void) setlocale(LC_ALL, "");

	if (--argc == 0) {
		(void) putchar('\n');
		if (fflush(stdout) != 0)
			return (1);
		return (0);
	}

#ifdef	_iBCS2
	/* If SYSV3 is set, check for ISC/SCO style -n option parsing. */
	if (getenv("SYSV3")) {
		if (strcmp(argv[1], "-n") == 0)
			no_nl ++;
	}
	for (i = 1 + no_nl; i <= argc; i++) {
#else
	for (i = 1; i <= argc; i++) {
#endif	/*  _iBCS2 */
		for (cp = argv[i], ep = cp + (int)strlen(cp);
			cp < ep; cp += b_len) {
		if ((b_len = mbtowc(&wc, cp, MB_CUR_MAX)) <= 0) {
			(void) putchar(*cp);
			b_len = 1;
			continue;
		}

		if (wc != '\\') {
			putwchar(wc);
			continue;
		}

			cp += b_len;
			b_len = 1;
			switch (*cp) {
				case 'a':	/* alert - XCU4 */
					(void) putchar('\a');
					continue;

				case 'b':
					(void) putchar('\b');
					continue;

				case 'c':
					if (fflush(stdout) != 0)
						return (1);
					return (0);

				case 'f':
					(void) putchar('\f');
					continue;

				case 'n':
					(void) putchar('\n');
					continue;

				case 'r':
					(void) putchar('\r');
					continue;

				case 't':
					(void) putchar('\t');
					continue;

				case 'v':
					(void) putchar('\v');
					continue;

				case '\\':
					(void) putchar('\\');
					continue;
				case '0':
					j = wd = 0;
					while ((*++cp >= '0' && *cp <= '7') &&
						j++ < 3) {
						wd <<= 3;
						wd |= (*cp - '0');
					}
					(void) putchar(wd);
					--cp;
					continue;

				default:
					cp--;
					(void) putchar(*cp);
			}
		}
#ifdef	_iBCS2
		if (!(no_nl && i == argc))
#endif	/* _iBCS2 */
			(void) putchar(i == argc? '\n': ' ');
			if (fflush(stdout) != 0)
				return (1);
	}
	return (0);
}
