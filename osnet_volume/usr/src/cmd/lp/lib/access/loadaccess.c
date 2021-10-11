/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)loadaccess.c	1.6	97/05/14 SMI"	/* SVr4.0 1.9	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

#include "lp.h"
#include "access.h"

static char		**_loadaccess ( char * );

/**
 ** load_userform_access() - LOAD ALLOW/DENY LISTS FOR USER+FORM
 **/

int
load_userform_access(char *form, char ***pallow, char ***pdeny)
{
	return (loadaccess(Lp_A_Forms, form, "", pallow, pdeny));
}

/**
 ** load_userprinter_access() - LOAD ALLOW/DENY LISTS FOR USER+PRINTER
 **/

int
load_userprinter_access(char *printer, char ***pallow, char ***pdeny)
{
	return (loadaccess(Lp_A_Printers, printer, UACCESSPREFIX, pallow,
		pdeny));
}

/**
 ** load_formprinter_access() - LOAD ALLOW/DENY LISTS FOR FORM+PRINTER
 **/

int
load_formprinter_access(char *printer, char ***pallow, char ***pdeny)
{
	return (loadaccess(Lp_A_Printers, printer, FACCESSPREFIX, pallow,
		pdeny));
}

/**
 ** load_paperprinter_access() - LOAD ALLOW/DENY LISTS FOR FORM+PRINTER
 **/

int
load_paperprinter_access(char *printer, char ***pallow, char ***pdeny)
{
	return (loadaccess(Lp_A_Printers, printer, PACCESSPREFIX, pallow,
		pdeny));
}

/**
 ** loadaccess() - LOAD ALLOW OR DENY LISTS
 **/

int
loadaccess(char *dir, char *name, char *prefix, char ***pallow, char ***pdeny)
{
	register char		*allow_file	= 0,
				*deny_file	= 0;

	int			ret;

	if (
		!(allow_file = getaccessfile(dir, name, prefix, ALLOWFILE))
	     || !(*pallow = _loadaccess(allow_file)) && errno != ENOENT
	     || !(deny_file = getaccessfile(dir, name, prefix, DENYFILE))
	     || !(*pdeny = _loadaccess(deny_file)) && errno != ENOENT
	)
		ret = -1;
	else
		ret = 0;

	if (allow_file)
		Free (allow_file);
	if (deny_file)
		Free (deny_file);

	return (ret);
}

/**
 ** _loadaccess() - LOAD ALLOW OR DENY FILE
 **/

static char **
_loadaccess(char *file)
{
	register size_t		nalloc,
				nlist;

	register char		**list;

	int fd;

	char			buf[BUFSIZ];


	if ((fd = open_locked(file, "r", 0)) < 0)
		return (0);

	/*
	 * Preallocate space for the initial list. We'll always
	 * allocate one more than the list size, for the terminating null.
	 */
	nalloc = ACC_MAX_GUESS;
	list = (char **)Malloc((nalloc + 1) * sizeof(char *));
	if (!list) {
		close(fd);
		errno = ENOMEM;
		return (0);
	}

	errno = 0;
	for (nlist = 0; fdgets(buf, BUFSIZ, fd); ) {

		buf[strlen(buf) - 1] = 0;

		/*
		 * Allocate more space if needed.
		 */
		if (nlist >= nalloc) {
			nalloc += ACC_MAX_GUESS;
			list = (char **)Realloc(
				(char *)list,
				(nalloc + 1) * sizeof(char *)
			);
			if (!list) {
				close(fd);
				return (0);
			}
		}

		list[nlist] = Strdup(buf);   /* if fail, minor problem */
		list[++nlist] = 0;

	}
	if (errno != 0) {
		int			save_errno = errno;

		close(fd);
		freelist (list);
		errno = save_errno;
		return (0);
	}
	close(fd);

	/*
	 * If we have more space allocated than we need,
	 * return the extra.
	 */
	if (nlist != nalloc) {
		list = (char **)Realloc(
			(char *)list,
			(nlist + 1) * sizeof(char *)
		);
		if (!list) {
			errno = ENOMEM;
			return (0);
		}
	}
	list[nlist] = 0;

	return (list);
}
