/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_test.c	1.2	99/11/19 SMI"

/*
 * MDB Regression Test Module
 *
 * This module contains dcmds and walkers that exercise various aspects of
 * MDB and the MDB Module API.  It can be manually loaded and executed to
 * verify that MDB is still working properly.
 */

#include <mdb/mdb_modapi.h>

static int
cd_init(mdb_walk_state_t *wsp)
{
	wsp->walk_addr = 0xf;
	return (WALK_NEXT);
}

static
cd_step(mdb_walk_state_t *wsp)
{
	int status = wsp->walk_callback(wsp->walk_addr, NULL, wsp->walk_cbdata);

	if (wsp->walk_addr-- == 0)
		return (WALK_DONE);

	return (status);
}

/*ARGSUSED*/
static void
cd_fini(mdb_walk_state_t *wsp)
{
	/* Nothing doing */
}

/*ARGSUSED*/
static int
cmd_praddr(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if ((flags != (DCMD_ADDRSPEC|DCMD_LOOP|DCMD_PIPE)) &&
	    (flags != (DCMD_ADDRSPEC|DCMD_LOOP|DCMD_PIPE|DCMD_LOOPFIRST))) {
		mdb_warn("ERROR: praddr invoked with flags = 0x%x\n", flags);
		return (DCMD_ERR);
	}

	if (argc != 0) {
		mdb_warn("ERROR: praddr invoked with argc = %lu\n", argc);
		return (DCMD_ERR);
	}

	mdb_printf("%lr\n", addr);
	return (DCMD_OK);
}

static int
compare(const void *lp, const void *rp)
{
	uintptr_t lhs = *((const uintptr_t *)lp);
	uintptr_t rhs = *((const uintptr_t *)rp);
	return (lhs - rhs);
}

/*ARGSUSED*/
static int
cmd_qsort(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_pipe_t p;
	size_t i;

	if (flags != (DCMD_ADDRSPEC | DCMD_LOOP |
	    DCMD_LOOPFIRST | DCMD_PIPE | DCMD_PIPE_OUT)) {
		mdb_warn("ERROR: qsort invoked with flags = 0x%x\n", flags);
		return (DCMD_ERR);
	}

	if (argc != 0) {
		mdb_warn("ERROR: qsort invoked with argc = %lu\n", argc);
		return (DCMD_ERR);
	}

	mdb_get_pipe(&p);

	if (p.pipe_data == NULL || p.pipe_len != 16) {
		mdb_warn("ERROR: qsort got bad results from mdb_get_pipe\n");
		return (DCMD_ERR);
	}

	if (p.pipe_data[0] != addr) {
		mdb_warn("ERROR: qsort pipe_data[0] != addr\n");
		return (DCMD_ERR);
	}

	qsort(p.pipe_data, p.pipe_len, sizeof (uintptr_t), compare);
	mdb_set_pipe(&p);

	for (i = 0; i < 16; i++) {
		if (p.pipe_data[i] != i) {
			mdb_warn("ERROR: qsort got bad data in slot %lu\n", i);
			return (DCMD_ERR);
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_runtest(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_walker_t w = { "count", "count", cd_init, cd_step, cd_fini };
	int i;

	mdb_printf("- adding countdown walker\n");
	if (mdb_add_walker(&w) != 0) {
		mdb_warn("ERROR: failed to add walker");
		return (DCMD_ERR);
	}

	mdb_printf("- executing countdown pipeline\n");
	if (mdb_eval("::walk mdb_test`count |::mdb_test`qsort |::praddr")) {
		mdb_warn("ERROR: failed to eval command");
		return (DCMD_ERR);
	}

	mdb_printf("- removing countdown walker\n");
	if (mdb_remove_walker("count") != 0) {
		mdb_warn("ERROR: failed to remove walker");
		return (DCMD_ERR);
	}

	mdb_printf("- kernel=%d postmortem=%d\n",
	    mdb_prop_kernel, mdb_prop_postmortem);

	if (mdb_prop_kernel && mdb_prop_postmortem) {
		mdb_printf("- exercising pipelines\n");
		for (i = 0; i < 100; i++) {
			if (mdb_eval("::walk proc p | ::map *. | ::grep .==0 "
			    "| ::map <p | ::ps") != 0) {
				mdb_warn("ERROR: failed to eval pipeline");
				return (DCMD_ERR);
			}
		}
	}

	return (DCMD_OK);
}

static int
cmd_vread(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	size_t nbytes;
	ssize_t rbytes;
	void *buf;

	if (!(flags & DCMD_ADDRSPEC) || argc != 1)
		return (DCMD_USAGE);

	if (argv->a_type == MDB_TYPE_STRING)
		nbytes = (size_t)mdb_strtoull(argv->a_un.a_str);
	else
		nbytes = (size_t)argv->a_un.a_val;

	buf = mdb_alloc(nbytes, UM_SLEEP | UM_GC);
	rbytes = mdb_vread(buf, nbytes, addr);

	if (rbytes >= 0) {
		mdb_printf("mdb_vread of %lu bytes returned %ld\n",
		    nbytes, rbytes);
	} else
		mdb_warn("mdb_vread returned %ld", rbytes);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_pread(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	size_t nbytes;
	ssize_t rbytes;
	void *buf;

	if (!(flags & DCMD_ADDRSPEC) || argc != 1)
		return (DCMD_USAGE);

	if (argv->a_type == MDB_TYPE_STRING)
		nbytes = (size_t)mdb_strtoull(argv->a_un.a_str);
	else
		nbytes = (size_t)argv->a_un.a_val;

	buf = mdb_alloc(nbytes, UM_SLEEP | UM_GC);
	rbytes = mdb_pread(buf, nbytes, mdb_get_dot());

	if (rbytes >= 0) {
		mdb_printf("mdb_pread of %lu bytes returned %ld\n",
		    nbytes, rbytes);
	} else
		mdb_warn("mdb_pread returned %ld", rbytes);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_readsym(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	size_t nbytes;
	ssize_t rbytes;
	void *buf;

	if ((flags & DCMD_ADDRSPEC) || argc != 2 ||
	    argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (argv[1].a_type == MDB_TYPE_STRING)
		nbytes = (size_t)mdb_strtoull(argv[1].a_un.a_str);
	else
		nbytes = (size_t)argv[1].a_un.a_val;

	buf = mdb_alloc(nbytes, UM_SLEEP | UM_GC);
	rbytes = mdb_readsym(buf, nbytes, argv->a_un.a_str);

	if (rbytes >= 0) {
		mdb_printf("mdb_readsym of %lu bytes returned %ld\n",
		    nbytes, rbytes);
	} else
		mdb_warn("mdb_readsym returned %ld", rbytes);

	return (DCMD_OK);
}

static int
cmd_call_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *dcmd;

	if (argc < 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	dcmd = argv->a_un.a_str;
	argv++;
	argc--;

	if (mdb_call_dcmd(dcmd, addr, flags, argc, argv) == -1) {
		mdb_warn("failed to execute %s", dcmd);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_getsetdot(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	mdb_set_dot(0x12345678feedbeef);

	if (mdb_get_dot() != 0x12345678feedbeef) {
		mdb_warn("mdb_get_dot() returned wrong value!\n");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static const mdb_dcmd_t dcmds[] = {
	{ "runtest", NULL, "run MDB regression tests", cmd_runtest },
	{ "qsort", NULL, "qsort addresses", cmd_qsort },
	{ "praddr", NULL, "print addresses", cmd_praddr },
	{ "vread", ":nbytes", "call mdb_vread", cmd_vread },
	{ "pread", ":nbytes", "call mdb_pread", cmd_pread },
	{ "readsym", "symbol nbytes", "call mdb_readsym", cmd_readsym },
	{ "call_dcmd", "dcmd [ args ... ]", "call dcmd", cmd_call_dcmd },
	{ "getsetdot", NULL, "test get and set dot", cmd_getsetdot },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
