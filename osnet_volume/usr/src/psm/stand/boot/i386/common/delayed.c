/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)delayed.c	1.4	99/06/06 SMI"

#include <sys/bootconf.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include "devtree.h"

/*
 *  External functions and structures
 */
extern struct bootops *bop;
extern struct dnode *active_node;
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

/*
 *  RAMfile globals
 */
static int num_delayed;

/*
 * RAMfiletoprop
 *	Convert a RAMfile to a delayed write property.
 */
void
RAMfiletoprop(rffd_t *handle)
{
	struct dnode *save;
	long	fullsize;
	void	*fillp;
	char	namebuf[16];
	char	*fbuf;
	int	xfrd;

	save = active_node;

	/* alloc fullsize buffer */
	fullsize = sizeof (long) + strlen(handle->file->name) + 1;
	fullsize += sizeof (long) + handle->file->size;
	if ((fbuf = (char *)bkmem_alloc((uint_t)fullsize)) == (char *)NULL) {
		printf("ERROR:	No memory for delayed-write buffer!\n");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto xit;
	}

	/* first bytes of buf: (long)(strlen(fname)+1) */
	fillp = fbuf;
	*((long *)fillp) = strlen(handle->file->name) + 1;
	fillp = ((long *)fillp) + 1;

	/* next bytes are strcpy(fname) */
	(void) strcpy((char *)fillp, handle->file->name);
	fillp = ((char *)fillp) + strlen(handle->file->name) + 1;

	/* next bytes are (long)(Ramfile_size) */
	*((long *)fillp) = handle->file->size;
	fillp = ((long *)fillp) + 1;

	/* next bytes are file contents */
	RAMrewind(handle->uid);
	xfrd = RAMfile_read(handle->uid, (char *)fillp, handle->file->size);
	if (xfrd < 0) {
		printf("ERROR: RAMfile conversion to property failed.  ");
		printf("RAMfile read failed.  ");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto clean;
	} else if (xfrd != handle->file->size) {
		printf("WARNING: Error in RAMfile conversion to property.  ");
		printf("Possibly short read.\n");
	}

	/* form new property name 'write'num_delayedwrites */
	(void) sprintf(namebuf, "write%d", num_delayed);

	/* binary setprop new name, value in buf */
	if (bsetprop(bop, namebuf, fbuf, (int)fullsize,
	    delayed_node.dn_nodeid)) {
		printf("ERROR: RAMfile conversion to property failed.  ");
		printf("Setprop failed.  ");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto clean;
	}

	num_delayed++;
	handle->file->flags &= ~RAMfp_modified;

clean: /* free buffer */
	bkmem_free(fbuf, (uint_t)fullsize);

xit:	/* reset active node */
	active_node = save;
}
