/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pkgenv.c	1.8	93/12/20 SMI"	/* SVr4.0 1.1.1.1 */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libadm.h"
#include "libinst.h"

#define	ERR_PKGINFO	"unable to access pkginfo file <%s>"
#define	ERR_PKGMAP	"unable to access pkgmap file <%s>"
#define	ERR_NOPARAM	"%s parameter is not defined in <%s>"
#define	ERR_PKGBAD	"PKG parameter is invalid <%s>"
#define	ERR_PKGMTCH	"PKG parameter <%s> does not match instance <%s>"

char	*pkgarch;
char	*pkgvers;
char	*pkgabrv;
char	*pkgname;
char	pkgwild[PKGSIZ+1];

/*
 * This function confirms the presence of pkgmap and pkginfo and verifies
 * that the mandatory parameters are available in the environment.
 */
int
pkgenv(char *pkginst, char *p_pkginfo, char *p_pkgmap)
{
	FILE	*fp;
	char 	*value,
		path[PATH_MAX],
		param[64];
	int	errflg;

	errflg = 0;
	if (access(p_pkgmap, 0)) {
		progerr(gettext(ERR_PKGMAP), p_pkgmap);
		return (1);
	}
	if ((fp = fopen(p_pkginfo, "r")) == NULL) {
		progerr(gettext(ERR_PKGINFO), p_pkginfo);
		return (1);
	}
	param[0] = '\0';
	while (value = fpkgparam(fp, param)) {
		if (strcmp("PATH", param))
			putparam(param, value);
		free(value);
		param[0] = '\0';
	}
	(void) fclose(fp);
	/*
	 * verify that required parameters are now present in
	 * the environment
	 */
	if ((pkgabrv = getenv("PKG")) == NULL) {
		progerr(gettext(ERR_NOPARAM), "PKG", path);
		errflg++;
	}
	if (pkgnmchk(pkgabrv, NULL, 0) || strchr(pkgabrv, '.')) {
		progerr(gettext(ERR_PKGBAD), pkgabrv);
		errflg++;
	}
	(void) sprintf(pkgwild, "%s.*", pkgabrv);
	if ((pkgname = getenv("NAME")) == NULL) {
		progerr(gettext(ERR_NOPARAM), "NAME", path);
		errflg++;
	}
	if ((pkgarch = getenv("ARCH")) == NULL) {
		progerr(gettext(ERR_NOPARAM), "ARCH", path);
		errflg++;
	}
	if ((pkgvers = getenv("VERSION")) == NULL) {
		progerr(gettext(ERR_NOPARAM), "VERSION", path);
		errflg++;
	}
	if (getenv("CATEGORY") == NULL) {
		progerr(gettext(ERR_NOPARAM), "CATEGORY", path);
		errflg++;
	}
	/*
	 * verify consistency between PKG parameter and pkginst that
	 * was determined from the directory structure
	 */
	(void) sprintf(param, "%s.*", pkgabrv);
	if (pkgnmchk(pkginst, param, 0)) {
		progerr(gettext(ERR_PKGMTCH), pkgabrv, pkginst);
		errflg++;
	}
	return (errflg);
}
