/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sortmap.c	1.22	96/04/05 SMI"	/* SVr4.0 1.11.1.1 */

/*  5-20-92	add newroot functions */

/*
 * This module constructs a list of entries from the pkgmap associated
 * with this package. When finished, this list is sorted in alphabetical
 * order and an accompanying structure list, mergstat, provides
 * information about how these new files merge with existing files
 * already on the system.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

/* libinst/ocfile.c */
extern int	trunc_tcfile(void);

extern int	dbchg;

extern void	quit(int exitval);

static int	client_refer(struct cfextra **ext);
static int	server_refer(struct cfextra **ext);

int
sortmap(struct cfextra ***extlist, FILE *pkgmapfp,
    FILE *mapfp, FILE *tmpfp)
{
	int	i, n, nparts;

	echo(gettext("## Processing package information."));

	/*
	 * The following instruction puts the client-relative basedir
	 * into the environment iff it's a relocatable package and
	 * we're installing to a client. Otherwise, it uses the regular
	 * basedir. The only reason for this is so that mappath() upon
	 * finding $BASEDIR in a path will properly resolve it to the
	 * client-relative path. This way eval_path() can properly
	 * construct the server-relative path.
	 */
	if (is_relocatable() && is_an_inst_root())
		putparam("BASEDIR", get_info_basedir());

	/*
	 * read the pkgmap provided by this package into
	 * memory; map parameters specified in the pathname
	 * and sort in memory by pathname
	 */
	(void) fseek(pkgmapfp, 0L, 0); /* rewind input file */
	*extlist = pkgobjmap(pkgmapfp, 2, NULL);

	if (*extlist == NULL) {
		progerr(gettext("unable to process pkgmap"));
		quit(99);
	}

	/* Make all paths client-relative if necessary. */
	if (is_an_inst_root())
		client_refer(*extlist);

	echo(gettext("## Processing system information."));

	/*
	 * calculate the number of parts in this package
	 * by locating the entry with the largest "volno"
	 * associated with it
	 */
	nparts = 0;
	for (i = 0; (*extlist)[i]; i++) {
		n = (*extlist)[i]->cf_ent.volno;
		if (n > nparts)
			nparts = n;
	}

	/* truncate the t.contents file to 0 length */
	if (!trunc_tcfile())
		quit(99);

	dbchg = pkgdbmerg(mapfp, tmpfp, *extlist, 60);

	if (dbchg < 0) {
		progerr(
		    gettext("unable to merge package and system information"));
		quit(99);
	}

	/* Restore the original BASEDIR. */
	if (is_relocatable() && is_an_inst_root())
		putparam("BASEDIR", get_basedir());

	if (is_an_inst_root())
		server_refer(*extlist);

	return (nparts);
}

static int
client_refer(struct cfextra **ext)
{
	int count;

	for (count = 0; ext[count] != (struct cfextra *)NULL; count++) {
		ext[count]->cf_ent.path = ext[count]->client_path;
		ext[count]->cf_ent.ainfo.local = ext[count]->client_local;
	}

	return (1);
}

static int
server_refer(struct cfextra **ext)
{
	int count;

	for (count = 0; ext[count] != (struct cfextra *)NULL; count++) {
		ext[count]->cf_ent.path = ext[count]->server_path;
		ext[count]->cf_ent.ainfo.local = ext[count]->server_local;
	}

	return (1);
}
