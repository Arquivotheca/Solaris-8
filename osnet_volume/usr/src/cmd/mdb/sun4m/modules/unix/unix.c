/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)unix.c	1.2	99/11/19 SMI"

#include <mdb/mdb_modapi.h>
#include "ttrace.h"

static const mdb_dcmd_t dcmds[] = {
	{ "ttrace", ":[-x]", "dump trap trace buffer for a cpu", ttrace },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "ttrace", "walks the trap trace buffer for a CPU",
		ttrace_walk_init, ttrace_walk_step, ttrace_walk_fini },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
