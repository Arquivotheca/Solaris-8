/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dofinal.c	1.20	96/04/05 SMI"	/* SVr4.0 1.10.1.1	*/

/*  5-20-92 	added newroot functions */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libinst.h"
#include "libadm.h"

extern struct cfextra **extlist;

extern char	errbuf[],
		*errstr,
		*pkginst;

extern void	quit(int exitval);

#define	ERR_WRITE	"write of intermediate contents file failed"

int
dofinal(FILE *fp, FILE *fpo, int rmflag, char *myclass)
{
	struct cfextra entry;
	struct pinfo *pinfo;
	int	n, indx, dbchg, fs_entry;
	char	*save_path;
	char	path[PATH_MAX];
	char	*tp;

	entry.cf_ent.pinfo = NULL;
	entry.fsys_value = BADFSYS;
	entry.fsys_base = BADFSYS;
	indx = 0;
	while (extlist && extlist[indx] && (extlist[indx]->cf_ent.ftype == 'i'))
		indx++;

	dbchg = 0;
	while (n = srchcfile(&(entry.cf_ent), "*", fp, fpo)) {
		if (n < 0) {
			progerr(gettext("bad entry read in contents file"));
			logerr(gettext("pathname=%s"),
			    (entry.cf_ent.path && *(entry.cf_ent.path)) ?
			    entry.cf_ent.path : "Unknown");
			logerr(gettext("problem=%s"),
			    (errstr && *errstr) ? errstr : "Unknown");
			quit(99);
		}
		if (myclass && strcmp(myclass, entry.cf_ent.pkg_class)) {
			if (putcfile(&(entry.cf_ent), fpo)) {
				progerr(gettext(ERR_WRITE));
				quit(99);
			}
			continue;
		}

		/*
		 * Now scan each package instance holding this file or
		 * directory and see if it matches the package we are
		 * updating here.
		 */
		pinfo = entry.cf_ent.pinfo;
		while (pinfo) {
			if (strcmp(pkginst, pinfo->pkg) == 0)
				break;
			pinfo = pinfo->next;
		}

		/*
		 * If pinfo == NULL at this point, then this file or
		 * directory isn't part of the package of interest.
		 * So the loop below executes only on files in the package
		 * of interest.
		 */

		save_path = NULL;

		if (pinfo) {
			if (rmflag && (pinfo->status == RM_RDY)) {
				dbchg++;
				(void) eptstat(&(entry.cf_ent), pkginst, '@');
				if (entry.cf_ent.npkgs)
					if (putcfile(&(entry.cf_ent), fpo)) {
						progerr(gettext(ERR_WRITE));
						quit(99);
					}
				continue;
			} else if (!rmflag && (pinfo->status == INST_RDY)) {
				dbchg++;

				/* tp is the server-relative path */
				tp = fixpath(entry.cf_ent.path);
				/* save_path is the cmd line path */
				save_path = entry.cf_ent.path;
				/* entry has the server-relative path */
				entry.cf_ent.path = tp;

				/*
				 * The next if statement figures out how
				 * the contents file entry should be
				 * annotated.
				 *
				 * Don't install or verify objects for
				 * remote, read-only filesystems.  We
				 * need only verify their presence and
				 * flag them appropriately from some
				 * server. Otherwise, ok to do final
				 * check.
				 */
				fs_entry = fsys(entry.cf_ent.path);

				if (is_remote_fs_n(fs_entry) &&
				    !is_fs_writeable_n(fs_entry)) {
					/*
					 * Mark it shared whether it's present
					 * or not. life's too funny for me
					 * to explain.
					 */
					pinfo->status = SERVED_FILE;

					/*
					 * restore for now. This may
					 * chg soon.
					 */
					entry.cf_ent.path = save_path;
				} else {
					/*
					 * If the object is accessible, check
					 * the new entry for existence and
					 * attributes. If there's a problem,
					 * mark it NOT_FND; otherwise,
					 * ENTRY_OK.
					 */
					if (is_mounted_n(fs_entry))
						pinfo->status =
						    (finalck(&(entry.cf_ent),
						    1, 1) ?
						    NOT_FND : ENTRY_OK);
					/*
					 * It's not remote, read-only but it
					 * may look that way to the client.
					 * If it does, overwrite the above
					 * result - mark it shared.
					 */
					if (is_served_n(fs_entry))
						pinfo->status = SERVED_FILE;

					/* restore original path */
					entry.cf_ent.path = save_path;
					/*   and clear save_path */
					save_path = NULL;
				}
			}
		}

		/* Output entry to contents file. */
		if (putcfile(&(entry.cf_ent), fpo)) {
			progerr(gettext(ERR_WRITE));
			quit(99);
		}

		/* Restore original server-relative path, if needed */
		if (save_path != NULL) {
			entry.cf_ent.path = save_path;
			save_path = NULL;
		}
	}
	return (dbchg);
}
