/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lofs.c	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/fs/lofs_node.h>
#include <sys/fs/lofs_info.h>

#include <mdb/mdb_modapi.h>

typedef struct lnode_walk {
	lnode_t **lw_table;		/* Snapshot of ltable hash */
	size_t lw_tabsz;		/* Size of hash table */
	size_t lw_tabi;			/* Current table index */
	lnode_t *lw_lnode;		/* Current buffer */
} lnode_walk_t;

int
lnode_walk_init(mdb_walk_state_t *wsp)
{
	lnode_walk_t *lwp;
	GElf_Sym ltable;

	if (wsp->walk_addr != NULL) {
		mdb_warn("only global lnode walk supported\n");
		return (WALK_ERR);
	}

	if (mdb_lookup_by_name("ltable", &ltable) == -1) {
		mdb_warn("failed to locate 'ltable' symbol\n");
		return (WALK_ERR);
	}

	lwp = mdb_alloc(sizeof (lnode_walk_t), UM_SLEEP);

	lwp->lw_tabsz = ltable.st_size / sizeof (lnode_t *);
	lwp->lw_table = mdb_alloc(lwp->lw_tabsz * sizeof (lnode_t *), UM_SLEEP);
	lwp->lw_tabi = 0;
	lwp->lw_lnode = mdb_alloc(sizeof (lnode_t), UM_SLEEP);

	(void) mdb_vread(lwp->lw_table,
	    lwp->lw_tabsz * sizeof (lnode_t *), ltable.st_value);

	wsp->walk_addr = (uintptr_t)lwp->lw_table[0];
	wsp->walk_data = lwp;

	return (WALK_NEXT);
}

int
lnode_walk_step(mdb_walk_state_t *wsp)
{
	lnode_walk_t *lwp = wsp->walk_data;
	uintptr_t addr;

	/*
	 * If the next lnode_t address we want is NULL, advance to the next
	 * hash bucket.  When we reach lw_tabsz, we're done.
	 */
	while (wsp->walk_addr == NULL) {
		if (++lwp->lw_tabi < lwp->lw_tabsz)
			wsp->walk_addr = (uintptr_t)lwp->lw_table[lwp->lw_tabi];
		else
			return (WALK_DONE);
	}

	/*
	 * When we have an lnode_t address, read the lnode and invoke the
	 * walk callback.  Keep the next lnode_t address in wsp->walk_addr.
	 */
	addr = wsp->walk_addr;
	(void) mdb_vread(lwp->lw_lnode, sizeof (lnode_t), addr);
	wsp->walk_addr = (uintptr_t)lwp->lw_lnode->lo_next;

	return (wsp->walk_callback(addr, lwp->lw_lnode, wsp->walk_cbdata));
}

void
lnode_walk_fini(mdb_walk_state_t *wsp)
{
	lnode_walk_t *lwp = wsp->walk_data;

	mdb_free(lwp->lw_table, lwp->lw_tabsz * sizeof (lnode_t *));
	mdb_free(lwp->lw_lnode, sizeof (lnode_t));
	mdb_free(lwp, sizeof (lnode_walk_t));
}

/*ARGSUSED*/
static int
lnode_format(uintptr_t addr, const void *data, void *private)
{
	const lnode_t *lop = data;

	mdb_printf("%?p %?p %?p %?p\n",
	    addr, addr + OFFSETOF(lnode_t, lo_vnode),
	    lop->lo_vp, lop->lo_crossedvp);

	return (DCMD_OK);
}

/*ARGSUSED*/
int
lnode(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%<u>%?s %?s %?s %?s%</u>\n",
		    "LNODE", "VNODE", "REALVP", "CROSSVP");
	}

	if (flags & DCMD_ADDRSPEC) {
		lnode_t lo;

		(void) mdb_vread(&lo, sizeof (lo), addr);
		return (lnode_format(addr, &lo, NULL));
	}

	if (mdb_walk("lnode", lnode_format, NULL) == -1)
		return (DCMD_ERR);

	return (DCMD_OK);
}

/*ARGSUSED*/
int
lnode2dev(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	lnode_t lo;
	vfs_t vfs;

	if (argc != 0)
		return (DCMD_ERR);

	(void) mdb_vread(&lo, sizeof (lo), addr);
	(void) mdb_vread(&vfs, sizeof (vfs), (uintptr_t)lo.lo_vnode.v_vfsp);

	mdb_printf("lnode %p vfs_dev %0?lx\n", addr, vfs.vfs_dev);
	return (DCMD_OK);
}

/*ARGSUSED*/
int
lnode2rdev(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct loinfo li;
	lnode_t lo;
	vfs_t vfs;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_vread(&lo, sizeof (lo), addr) == -1 || mdb_vread(&vfs,
	    sizeof (vfs), (uintptr_t)lo.lo_vnode.v_vfsp) == -1 ||
	    mdb_vread(&li, sizeof (li), (uintptr_t)vfs.vfs_data) == -1)
		return (DCMD_ERR);

	mdb_printf("lnode %p li_rdev %0?lx\n", addr, li.li_rdev);
	return (DCMD_OK);
}

static const mdb_dcmd_t dcmds[] = {
	{ "lnode", NULL, "print lnode structure(s)", lnode },
	{ "lnode2dev", ":", "print vfs_dev given lnode", lnode2dev },
	{ "lnode2rdev", ":", "print vfs_rdev given lnode", lnode2rdev },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "lnode", "hash of lnode structures",
		lnode_walk_init, lnode_walk_step, lnode_walk_fini },
	{ NULL }
};

static const mdb_modinfo_t modinfo = {
	MDB_API_VERSION, dcmds, walkers
};

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
