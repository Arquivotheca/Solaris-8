/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma  ident	"@(#)ttyslot.c	1.21	97/08/23 SMI"	/* SVr4.0 1.16	*/

/*LINTLIBRARY*/
/*
 * Return the number of the slot in the utmp file
 * corresponding to the current user: try for file 0, 1, 2.
 * Returns -1 if slot not found.
 */
#if !defined(ABI) && !defined(DSHLIB)
#pragma weak ttyslot = _ttyslot
#endif
#include "synonyms.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utmpx.h>
#include <stdlib.h>

#define	TRUE 1
#define	FALSE 0

int
ttyslot(void)
{
	struct futmpx *ubufp;
	static struct futmpx ubuf;
	char *tp, *p;
	int s;
	int ret = -1, console = FALSE, tmp;
	char ttynm[128];
	FILE	*fp;

	if ((tp = ttyname_r(0, ttynm, 128)) == NULL &&
	    (tp = ttyname_r(1, ttynm, 128)) == NULL &&
	    (tp = ttyname_r(2, ttynm, 128)) == NULL)
		return (-1);

	p = tp;
	if (strncmp(tp, "/dev/", 5) == 0)
		p += 5;

	if (strcmp(p, "console") == 0)
		console = TRUE;

	s = 0;
	if ((fp = fopen(UTMPX_FILE, "r")) == NULL)
		return (-1);
	while ((fread(&ubuf, sizeof (ubuf), 1, fp)) == 1) {
		ubufp = &ubuf;
		if ((ubufp->ut_type == INIT_PROCESS ||
		    ubufp->ut_type == LOGIN_PROCESS ||
		    ubufp->ut_type == USER_PROCESS) &&
		    strncmp(p, ubufp->ut_line, sizeof (ubufp->ut_line)) == 0) {
			ret = s;
			if (!console || strncmp(ubufp->ut_host, ":0", 2) == 0)
				break;
		}
		s++;
	}
	(void) fclose(fp);
	return (ret);
}
