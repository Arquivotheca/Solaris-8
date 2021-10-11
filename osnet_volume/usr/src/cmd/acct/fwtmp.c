/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1999 by Sun Microsystems, Inc. */
/*	All rights reserved. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)fwtmp.c	1.8	99/02/18 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <utmpx.h>
#include <locale.h>

struct	utmpx	Ut;
static char time_buf[50];

static int inp(FILE *, struct utmpx *);

int
main(int c, char **v)
{

	int	iflg, cflg;

	(void) setlocale(LC_ALL, "");

	iflg = cflg = 0;

	while (--c > 0) {
		if (**++v == '-')
			while (*++*v)
				switch (**v) {
				case 'c':
					cflg++;
					continue;
				case 'i':
					iflg++;
					continue;
				}
	}

	for (;;) {
		if (iflg) {
			if (inp(stdin, &Ut) == EOF)
				break;
		} else {
			if (fread(&Ut, sizeof (Ut), 1, stdin) != 1)
				break;
		}
		if (cflg)
			fwrite(&Ut, sizeof (Ut), 1, stdout);
		else {
			cftime(time_buf, DATE_FMT, &Ut.ut_xtime);
			printf("%-*.*s %-4.4s %-*.*s %9hd %2hd "
			    "%4.4ho %4.4ho %lu %s",
			    OUTPUT_NSZ,
			    OUTPUT_NSZ,
			    Ut.ut_name,
			    Ut.ut_id,
			    OUTPUT_LSZ,
			    OUTPUT_LSZ,
			    Ut.ut_line,
			    Ut.ut_pid,
			    Ut.ut_type,
			    Ut.ut_exit.e_termination,
			    Ut.ut_exit.e_exit,
			    Ut.ut_xtime,
			    time_buf);
		}
	}
	exit(0);
}

static int
inp(FILE *file, struct utmpx *u)
{

	char	buf[BUFSIZ];
	char *p;
	int i;

	if (fgets((p = buf), BUFSIZ, file) == NULL)
		return (EOF);

	for (i = 0; i < NSZ; i++)	/* Allow a space in name field */
		u->ut_name[i] = *p++;
	for (i = NSZ-1; i >= 0; i--) {
		if (u->ut_name[i] == ' ')
			u->ut_name[i] = '\0';
		else
			break;
	}
	p++;

	for (i = 0; i < 4; i++)
		if ((u->ut_id[i] = *p++) == ' ')
			u->ut_id[i] = '\0';
	p++;

	for (i = 0; i < LSZ; i++)	/* Allow a space in line field */
		u->ut_line[i] = *p++;
	for (i = LSZ-1; i >= 0; i--) {
		if (u->ut_line[i] == ' ')
			u->ut_line[i] = '\0';
		else
			break;
	}

	sscanf(p, "%hd %hd %ho %ho %ld",
		&u->ut_pid,
		&u->ut_type,
		&u->ut_exit.e_termination,
		&u->ut_exit.e_exit,
		&u->ut_xtime);

	return (1);
}
