/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getpentry.c	1.1	96/03/03 SMI"	/* SVr4.0 1.21	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "printers.h"

extern struct {
	char			*v;
	short			len,
				okremote;
}			prtrheadings[];

/*
 * getpentry() - EXTRACT ONE PRINTER ENTRY FROM DISK FILE
 */

char *
getpentry(char *name, int want_fld)
{
	static long		lastdir		= -1;
	char			buf[BUFSIZ];
	int			fld;
	int fd;
	register char *		p;
	register char *		path;
	int			isNameAll;
	char * option_entry = NULL;



	if (!name || !*name) {
		errno = EINVAL;
		return (0);
	}

	/*
	 * Getting ``all''? If so, jump into the directory
	 * wherever we left off.
	 */
	isNameAll = STREQU(NAME_ALL, name);
	for (; ; ) {
		/*
		 * fix for bug 1117241
		 * occasionally when a printer is removed, a printer directory
		 * is left behind, but the CONFIGFILE is removed.  In this
		 * case this directory terminates the search for additional
		 * printers as we have been returning 0 in this case.
		 * Now, we loop back and try the next directory until
		 * we have no more directories or we find a directory with
		 * a CONFIGFILE
		 */
		if (isNameAll) {
			if (!(name = next_dir(Lp_A_Printers, &lastdir)))
				return (0);
		} else
			lastdir = -1;

		/*
		 * Get the printer configuration information.
		 */

		path = getprinterfile(name, CONFIGFILE);
		if (!path) {
			if (isNameAll)
				Free(name);
			return (0);
		}

		if ((fd = open_locked(path, "r", 0)) < 0) {
			Free(path);

			/*
			 * go around to loop again for
			 * NAME_ALL case
			 */

			if (!isNameAll) /* fix for bug 1117241 */
				return (0);
			else
				Free(name);
		}
		else
			break;
	}
	Free(path);

	/*
	 * Read the file.
	 */
	errno = 0;
	while (fdgets(buf, BUFSIZ, fd) != NULL) {

		buf[strlen(buf) - 1] = 0;

		for (fld = 0; fld < PR_MAX; fld++)
			if (prtrheadings[fld].v &&
				prtrheadings[fld].len &&
				STRNEQU(
					buf,
					prtrheadings[fld].v,
					prtrheadings[fld].len)) {

				p = buf + prtrheadings[fld].len;
				while (*p && *p == ' ')
					p++;
				break;
			}

		/*
		 * To allow future extensions to not impact applications
		 * using old versions of this routine, ignore strange
		 * fields.
		 */
		if (fld >= PR_MAX)
			continue;

		if (fld == want_fld) {
			if ((option_entry = strdup(p)) == NULL) {
				return (0);
			}
		}


	}
	if (errno != 0) {
		int save_errno = errno;
		close(fd);
		errno = save_errno;
		return (0);
	}
	close(fd);

	return (option_entry);
}
