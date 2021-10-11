/*
 * Copyright (c) 1991, Sun Microsystems, Inc.
 */

#ident	"@(#)dumpcs.c	1.3	94/10/14 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <locale.h>
#include <wctype.h>
#include <getwidth.h>

#define	SCRWID 80
#define	LC_NAMELEN 255

#if !defined SS2
#define	SS2 0x8e
#endif
#if !defined SS3
#define	SS3 0x8f
#endif

static unsigned int cpl;	/* current characters per line */
static unsigned int cplmax;	/* maximum characters per line */
static unsigned char codestr[MB_LEN_MAX + 1];
static char linebuf[SCRWID / 2 * (MB_LEN_MAX + 1)];
static int pinline;		/* any printable to be listed in the line? */
static int omitting;		/* omitting blank lines under no vflag? */
static int vflag = 0;
static int wflag = 0;
static int csprint();
static int prcode();

main(ac, av)
int ac;
char *av[];
{
	int c;
	short int eucw[4];
	short int scrw[4];
	int csflag[4];
	int cs;
	int i;
	eucwidth_t eucwidth;
	char *lc_ctype;
	char titlebar[LC_NAMELEN + 14];

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
	lc_ctype = setlocale(LC_CTYPE, NULL);
	getwidth(&eucwidth);
	eucw[0] = 1;
	eucw[1] = eucwidth._eucw1;
	eucw[2] = eucwidth._eucw2;
	eucw[3] = eucwidth._eucw3;
	scrw[0] = 1;
	scrw[1] = eucwidth._scrw1;
	scrw[2] = eucwidth._scrw2;
	scrw[3] = eucwidth._scrw3;
	for (i = 0; i <= 3; i++)
		csflag[i] = 0;
	for (i = 1; i < ac; i++)
		if (*av[i] != '-')
			goto usage;
	while ((c = getopt(ac, av, "0123vw")) != -1) {
		switch (c) {
			case '0':
			case '1':
			case '2':
			case '3':
				csflag[c - '0'] = 1;
				break;
			case 'v':
				vflag++;
				break;
			case 'w':
				wflag++;
				break;
			default:
			usage:
				(void) printf(gettext("usage: %s [-0123vw]\n"), av[0]);
				exit (1);
		}
	}
	if ((csflag[0] + csflag[1] + csflag[2] + csflag[3]) == 0) {
		for (i = 0; i <= 3; i++)
			csflag[i]++;
	}
	(void) strcpy(titlebar, "");
	for (i = strlen(lc_ctype) + 14; i; i--)
		(void) strcat(titlebar, ":");
	for (cs = 0; cs <= 3; cs++) {
		if (csflag[cs] && eucw[cs] && scrw[cs]) {
			(void) printf("%s\n", titlebar);
			(void) printf("LC_CTYPE:%s", lc_ctype);
			(void) printf(" CS:%d\n", cs);
			(void) printf("%s", titlebar);
			(void) csprint(cs, (int) eucw[cs], (int) scrw[cs]);
			(void) printf("\n");
		}
	}
	exit (0);
}

int
csprint(cs, bytes, columns)
int cs, bytes, columns;
{
	int col, i, position;
	int bytelen;
	int minvalue;
	int maxvalue;

	col = SCRWID - bytes * 2 - 1;
	cplmax = 1;
	for (i = columns + 1; i <= col; i *= 2) cplmax *= 2;
	cplmax /= 2;
	bytelen = bytes;
	minvalue = 0x20;
	maxvalue = 0x7f;
	position = 0;
	if (cs > 0) {
		minvalue = 0xa0;
		maxvalue = 0xff;
	}
	if (cs == 2) {
		codestr[position++] = SS2;
		bytelen++;
	}
	if (cs == 3) {
		codestr[position++] = SS3;
		bytelen++;
	}
	codestr[position] = '\0';
	cpl = 0;
	(void) strcpy(linebuf, "");
	(void) prcode(bytelen, position, minvalue, maxvalue, columns);
	if (pinline || vflag) {
		(void) printf("\n%s", linebuf);
	} else if (!omitting) {
		(void) printf("\n*");
	}
	omitting = 0;
	return (0);
}

/*
 * prcode() prints series of len-byte codes, of which each byte can
 * have a code value between min and max, in incremental code order.
 */
int
prcode(len, pos, min, max, col)
int len, pos, min, max, col;
{
	int byte, i, nextpos;
	unsigned long widechar;	/* limitting wchar_t width - not good */
	char *prep;
	wchar_t wc;
	int	mbl;

	if (len - pos > 1) {
		for (byte = min; byte <= max; byte++) {
			codestr[pos] = (unsigned char) byte;
			nextpos = pos + 1;
			codestr[nextpos] = '\0';
			(void) prcode(len, nextpos, min, max, col);
		}
	} else {
		for (byte = min; byte <= max; byte++) {
			codestr[pos] = (unsigned char) byte;
			nextpos = pos + 1;
			codestr[nextpos] = '\0';
			if (!cpl) {
				widechar = 0;
				i = 0;
				while (codestr[i] != '\0') {
					widechar = (widechar << 8) |
						(unsigned long) codestr[i++];
				}
				if (*linebuf) {
					if (pinline || vflag) {
						(void) printf("\n%s", linebuf);
						omitting = 0;
					} else if (!omitting) {
						(void) printf("\n*");
						omitting++;
					}
				}
				if (!wflag) {
					(void) sprintf(linebuf, "%lx ",
					    widechar);
				} else {
					(void) mbtowc(&wc, (char *) codestr,
					    MB_CUR_MAX);
					(void) sprintf(linebuf, "%lx ", wc);
				}
				pinline = 0;
			}
			prep = " ";
			if ((mbl = mbtowc(&wc, (char *) codestr,
			    MB_CUR_MAX)) < 0)
				prep = "*";
			if (mbl == 1) {
				if (!(isprint(codestr[0]))) prep = "*";
			} else if (!(iswprint(wc)))
				prep = "*";
			if (prep[0] == '*') {
				(void) strcat(linebuf, prep);
				for (i = 1; i <= col; i++)
					(void) strcat(linebuf, " ");
			} else {
				(void) strcat(linebuf, prep);
				(void) strcat(linebuf, (char *) codestr);
				pinline = 1;
			}
			cpl++;
			cpl = cpl % cplmax;
		}
	}
	return (0);
}
