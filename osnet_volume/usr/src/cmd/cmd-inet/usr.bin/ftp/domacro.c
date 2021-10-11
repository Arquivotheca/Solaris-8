/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)domacro.c 1.12	99/08/17 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986-1990,1993,1995-1996,1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#include <libintl.h>
#include <stdlib.h>

#include "ftp_var.h"

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

void
domacro(int argc, char *argv[])
{
	register int i, j;
	register char *cp1, *cp2;
	int count = 2, loopflg = 0;
	char line2[200];
	struct cmd *c;
	int	len;

	if (argc < 2) {
		stop_timer();
		(void) strcat(line, " ");
		printf("(macro name) ");
		(void) gets(&line[strlen(line)]);
		reset_timer();
		makeargv();
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		printf("Usage: %s macro_name.\n", argv[0]);
		code = -1;
		return;
	}
	for (i = 0; i < macnum; ++i) {
		if (strncmp(argv[1], macros[i].mac_name, 9) == 0) {
			break;
		}
	}
	if (i == macnum) {
		printf("'%s' macro not found.\n", argv[1]);
		code = -1;
		return;
	}
	(void) strcpy(line2, line);
TOP:
	cp1 = macros[i].mac_start;
	while (cp1 != macros[i].mac_end) {
		while (isspace(*cp1)) {
			cp1++;
		}
		cp2 = line;
		while (*cp1 != '\0') {
			switch (*cp1) {
			case '\\':
				cp1++;
				if ((len = mblen(cp1, MB_CUR_MAX)) <= 0)
					len = 1;
				memcpy(cp2, cp1, len);
				cp2 += len;
				cp1 += len;
				break;

			case '$':
				if (isdigit(*(cp1+1))) {
					j = 0;
					while (isdigit(*++cp1))
						j = 10 * j +  *cp1 - '0';
					if (argc - 2 >= j) {
						(void) strcpy(cp2, argv[j+1]);
						cp2 += strlen(argv[j+1]);
					}
					break;
				}
				if (*(cp1+1) == 'i') {
					loopflg = 1;
					cp1 += 2;
					if (count < argc) {
						(void) strcpy(cp2, argv[count]);
						cp2 += strlen(argv[count]);
					}
					break;
				}
				/* intentional drop through */
			default:
				if ((len = mblen(cp1, MB_CUR_MAX)) <= 0)
					len = 1;
				memcpy(cp2, cp1, len);
				cp2 += len;
				cp1 += len;
				break;
			}
		}
		*cp2 = '\0';
		makeargv();
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			code = -1;
		} else if (c == 0) {
			printf("?Invalid command\n");
			code = -1;
		} else if (c->c_conn && !connected) {
			printf("Not connected.\n");
			code = -1;
		} else {
			if (verbose) {
				printf("%s\n", line);
			}
			(*c->c_handler)(margc, margv);
#define	CTRL(c) ((c)&037)
			if (bell && c->c_bell) {
				(void) putchar(CTRL('g'));
			}
			(void) strcpy(line, line2);
			makeargv();
			argc = margc;
			argv = margv;
		}
		if (cp1 != macros[i].mac_end) {
			cp1++;
		}
	}
	if (loopflg && ++count < argc) {
		goto TOP;
	}
}
