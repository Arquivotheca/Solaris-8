/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs.c	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>
#include <nfs/nfs.h>
#include <nfs/rnode.h>

typedef struct rtable_walk_data {
	uintptr_t *rw_table;
	int rw_table_sz;
	int rw_ndx;
	uintptr_t rw_rp;
} rtable_walk_data_t;

int
rtbl_walk_init(mdb_walk_state_t *wsp)
{
	rtable_walk_data_t *rw;
	uintptr_t rtable;
	int sz;

	if (mdb_readvar(&sz, "rtablesize") == -1) {
		mdb_warn("failed to read rtablesize");
		return (WALK_ERR);
	}

	if (mdb_readvar(&rtable, "rtable") == -1) {
		mdb_warn("failed to read rtable");
		return (WALK_ERR);
	}

	rw = mdb_zalloc(sizeof (rtable_walk_data_t), UM_SLEEP);

	rw->rw_table_sz = sz;
	rw->rw_table = mdb_alloc(sz * sizeof (uintptr_t), UM_SLEEP);
	rw->rw_ndx = 0;
	rw->rw_rp = NULL;

	if (mdb_vread(rw->rw_table, sz * sizeof (uintptr_t), rtable) == -1) {
		mdb_warn("failed to read rtable");
		return (WALK_ERR);
	}

	wsp->walk_data = rw;
	return (WALK_NEXT);
}

int
rtbl_walk_step(mdb_walk_state_t *wsp)
{
	rtable_walk_data_t *rw = wsp->walk_data;
	rnode_t rnode;
	uintptr_t addr;

again:
	while (rw->rw_rp == NULL && rw->rw_ndx < rw->rw_table_sz)
		rw->rw_rp = rw->rw_table[rw->rw_ndx++];

	if (rw->rw_rp == NULL)
		return (WALK_DONE);

	if (mdb_vread(&rnode, sizeof (rnode), addr = rw->rw_rp) == -1) {
		mdb_warn("failed to read rnode at %p", rw->rw_rp);
		rw->rw_rp = NULL;
		goto again;
	}

	rw->rw_rp = (uintptr_t)rnode.r_hash;
	return (wsp->walk_callback(addr, &rnode, wsp->walk_cbdata));
}

void
rtbl_walk_fini(mdb_walk_state_t *wsp)
{
	rtable_walk_data_t *rw = wsp->walk_data;

	mdb_free(rw->rw_table, rw->rw_table_sz * sizeof (uintptr_t));
	mdb_free(rw, sizeof (rtable_walk_data_t));
}

static const mdb_walker_t walkers[] = {
	{ "rtable", "rnodes in rtable",
		rtbl_walk_init, rtbl_walk_step, rtbl_walk_fini },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, NULL, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
