/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)versys.c	1.8	98/05/29 SMI"	/* from SVR4 bnu:versys.c 2.4 */

#include "uucp.h"

extern int getsysline();
extern void sysreset();

/*
 * verify system name
 * input:
 *	name	-> system name (char name[NAMESIZE])
 * returns:  
 *	0	-> success
 *	FAIL	-> failure
 */
int
versys(name)
char *name;
{
	register char *iptr;
	char line[BUFSIZ];
	extern char *aliasFind();
	char	*prev;

	if (name == 0 || *name == 0)
		return(FAIL);

	prev = _uu_setlocale(LC_ALL, "C");
	if ((iptr = aliasFind(name)) != NULL) {
		/* overwrite the original name with the real name */
		strncpy(name, iptr, MAXBASENAME);
		name[MAXBASENAME] = '\0';
	}

	if (EQUALS(name, Myname)) {
		(void) _uu_resetlocale(LC_ALL, prev);
		return(0);
	}

	while (getsysline(line, sizeof(line))) {
		if((line[0] == '#') || (line[0] == ' ') || (line[0] == '\t') || 
			(line[0] == '\n'))
			continue;

		if ((iptr=strpbrk(line, " \t")) == NULL)
		    continue;	/* why? */
		*iptr = '\0';
		if (EQUALS(name, line)) {
			sysreset();
			(void) _uu_resetlocale(LC_ALL, prev);
			return(0);
		}
	}
	sysreset();
	(void) _uu_resetlocale(LC_ALL, prev);
	return(FAIL);
}
