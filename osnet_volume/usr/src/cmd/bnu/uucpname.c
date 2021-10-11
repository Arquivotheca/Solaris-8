/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uucpname.c	1.8	95/03/01 SMI"	/* from SVR4 bnu:uucpname.c 2.3 */

#include "uucp.h"

/*
 * get the uucp name
 * return:
 *	none
 */
void
uucpname(name)
register char *name;
{
	char *s;
	char NameBuf[MAXBASENAME + 1];
	FILE *NameFile;

	NameBuf[0] = '\0';
	if ((NameFile = fopen(SYSNAMEFILE, "r")) != NULL) {
		if (fscanf(NameFile, "%14s", NameBuf) != 1) {
			(void) fprintf(stderr,
				gettext("No system name specified in %s\n"),
				SYSNAMEFILE);
			cleanup(-1);
		}
		s = NameBuf;
		(void) fclose(NameFile);
	} else {
#ifdef BSD4_2
	char	NameBuf[MAXBASENAME + 1];

	gethostname(NameBuf, MAXBASENAME);
	/* strip off any domain name part */
	if ((s = index(NameBuf, '.')) != NULL)
		*s = '\0';
	s = NameBuf;
	s[MAXBASENAME] = '\0';
#else /* !BSD4_2 */
#ifdef UNAME
	struct utsname utsn;

	uname(&utsn);
	s = utsn.nodename;
#else /* !UNAME */
	char	NameBuf[MAXBASENAME + 1], *strchr();
	FILE	*NameFile;

	s = MYNAME;
	NameBuf[0] = '\0';

	if ((NameFile = fopen("/etc/whoami", "r")) != NULL) {
		/* etc/whoami wins */
		(void) fgets(NameBuf, MAXBASENAME + 1, NameFile);
		(void) fclose(NameFile);
		NameBuf[MAXBASENAME] = '\0';
		if (NameBuf[0] != '\0') {
			if ((s = strchr(NameBuf, '\n')) != NULL)
				*s = '\0';
			s = NameBuf;
		}
	}
#endif /* UNAME */
#endif /* BSD4_2 */
	}

	(void) strncpy(name, s, MAXBASENAME);
	name[MAXBASENAME] = '\0';
	/* strip off any domain name from the host name */
	if ((s = strchr(name, '.')) != NULL)
		*s = '\0';
	return;
}
