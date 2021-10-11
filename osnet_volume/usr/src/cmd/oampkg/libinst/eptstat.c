/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)eptstat.c	1.17	96/04/05 SMI"	/* SVr4.0 1.4.1.1	*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <sys/stat.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libinst.h"
#include "libadm.h"

#define	PINFOALLOC	200

#define	ERR_MEMORY	"memory allocation failure, errno=%d"

int	otherstoo;
char	*useclass;

static pinfo_handle = -1;

extern void	quit(int exitval);

/* Free all allocated package info structures. */
void
pinfo_free(void)
{
	bl_free(pinfo_handle);
}

/*
 * This function manipulates the pinfo entry corresponding to the package
 * indicated on the command line.
 */
struct pinfo *
eptstat(struct cfent *entry, char *pkg, char c)
{
	struct pinfo *pinfo, *last, *me, *myparent;

	otherstoo = 0;
	useclass = entry->pkg_class;

	me = myparent = last = (struct pinfo *)0;

	if (pinfo_handle == -1) {
		pinfo_handle = bl_create(PINFOALLOC, sizeof (struct pinfo),
		    "package data");
	}

	for (pinfo = entry->pinfo; pinfo; pinfo = pinfo->next) {
		if (strcmp(pkg, pinfo->pkg) == 0) {
			if (*pinfo->aclass)
				useclass = pinfo->aclass;
			myparent = last;
			me = pinfo;
		} else
			otherstoo++;
		last = pinfo;
	}

	if (c) {
		/*
		 * use a delete/add strategy to keep package list
		 * ordered by modification time
		 */
		if (me) {
			/* remove from list first */
			if (myparent)
				myparent->next = me->next;
			else
				entry->pinfo = me->next;
			if (me == last)
				last = myparent;
			entry->npkgs--;
			/* leave 'me' around until later! */
		}
		if ((c != STAT_NEXT) && (me || (c != RM_RDY))) {
			/* need to add onto end */
			entry->npkgs++;
			if (me == NULL) {
				me = (struct pinfo *)
				    bl_next_avail(pinfo_handle);
				if (me == NULL) {
					progerr(gettext(ERR_MEMORY), errno);
					quit(99);
				}
			} else {
				me->next = (struct pinfo *)NULL;
				if (entry->npkgs == 1) {
					if (me->aclass[0])
						(void) strcpy(entry->pkg_class,
							me->aclass);
					useclass = entry->pkg_class;
				} else
					useclass = me->aclass;
			}
			(void) strncpy(me->pkg, pkg, PKGSIZ);

			/*
			 * Only change status for local objects.  Need
			 * to maintain "shared" status for objects that
			 * are provided from a server.
			 */
			if (me->status != SERVED_FILE)
				me->status = ((c == DUP_ENTRY) ? '\0' : c);

			if (last)
				last->next = me; /* add to end */
			else
				entry->pinfo = me; /* only item */
		} else {
			/* just wanted to remove this package from list */
			if (me) {
				free(me);
				me = (struct pinfo *) 0;
			}
		}
	}
	return (me);
}
