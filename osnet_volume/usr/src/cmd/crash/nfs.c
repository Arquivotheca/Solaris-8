/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs.c	1.9	99/06/25 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <rpc/types.h>
#include <sys/t_lock.h>
#include <sys/vnode.h>
#include <nfs/nfs.h>
#include <nfs/rnode.h>
#include "crash.h"

extern char *vnodeheading;
static void prnfsnode();
static void kmprnfsnode(void *, void *);
static void kmprfreenfsnode(void *, void *);
static char *nfs_heading = "ADDR      R_CNT V_CNT FLAGS ERROR       "
	"SIZE    NEXTR     CRED\n";
static int nfs_lock = 0;
static int nfs_full = 0;

getnfsnode()
{
	int c;
	long addr;
	rnode_t rp;
	int phys = 0;
	int lfree = 0;

	nfs_lock = 0;
	nfs_full = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "flprw:")) != EOF) {
		switch (c) {
		case 'r':
			lfree = 1;
			break;
		case 'f':
			nfs_full = 1;
			break;
		case 'l':
			nfs_lock = 1;
			break;
		case 'p':
			phys = 1;
			break;
		case 'w':
			redirect();
			break;
		default:
			longjmp(syn, 0);
		}
	}

	if (!nfs_full && !nfs_lock)
		fprintf(fp, "%s", nfs_heading);
	if (lfree) {
		kmem_cache_apply(kmem_cache_find("rnode_cache"),
							kmprfreenfsnode);
	} else if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			readbuf((void *)addr, 0, phys, &rp,
			    sizeof (rnode_t), "rnode");
			prnfsnode(nfs_heading, addr, &rp, nfs_full, nfs_lock);
		} while (args[++optind]);
	} else
		kmem_cache_apply(kmem_cache_find("rnode_cache"), kmprnfsnode);
	return (0);
}


static void
kmprnfsnode(void *kaddr, void *buf)
{
	rnode_t *rp = buf;

	/*
	 * If not on freelist and v_count and r_count == 0 then kmem_freed
	 */
	if (rp->r_freef == NULL && rp->r_count == 0 && rp->r_vnode.v_count == 0)
		return;

	if (rp->r_freef == NULL)
		prnfsnode(nfs_heading, (long)kaddr, rp, nfs_full, nfs_lock);
}

static void
kmprfreenfsnode(void *kaddr, void *buf)
{
	rnode_t *rp = buf;

	/*
	 * If not on freelist and v_count and r_count == 0 then kmem_freed
	 */
	if (rp->r_freef == NULL && rp->r_count == 0 && rp->r_vnode.v_count == 0)
		return;

	if (rp->r_freef != NULL)
		prnfsnode(nfs_heading, (long)kaddr, rp, nfs_full, nfs_lock);
}


static void
prnfsnode(heading, slot, rp, full, lock)
char *heading;
long slot;
rnode_t *rp;
int full, lock;
{
	if (full || lock)
		fprintf(fp, "%s", heading);

	fprintf(fp, "%8lx %6d%6d  %4x %5d %10lld %8llx %8p\n",
		slot, rp->r_count, rp->r_vnode.v_count, rp->r_flags,
		rp->r_error, rp->r_size, rp->r_nextr, rp->r_cred);
	if (lock) {
		fprintf(fp, "r_rwlock:");
		fprintf(fp, " count %d, waiters %d, owner %p\n",
			rp->r_rwlock.count, rp->r_rwlock.waiters,
			rp->r_rwlock.owner);
		fprintf(fp, "r_lkserlock:");
		fprintf(fp, " count %d, waiters %d, owner %p\n",
			rp->r_lkserlock.count, rp->r_lkserlock.waiters,
			rp->r_lkserlock.owner);
		fprintf(fp, "r_statelock:");
		prmutex(&rp->r_statelock);
		fprintf(fp, "\n");
	}
	if (full) {
		/* print vnode info */
		(void) fprintf(fp, "\nVNODE :\n");
		fprintf(fp, "%s", vnodeheading);
		prvnode(&rp->r_vnode, lock);
		fprintf(fp, "\n");
	}
}
