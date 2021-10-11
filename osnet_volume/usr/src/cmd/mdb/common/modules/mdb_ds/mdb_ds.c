/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_ds.c	1.2	99/11/19 SMI"

/*
 * MDB developer support module.  This module is loaded automatically when the
 * proc target is initialized and the target is mdb itself.  In the future, we
 * should document these facilities in the answerbook to aid module developers.
 */

#define	_MDB
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb.h>

static mdb_t M;

static int
cmd_stack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char sep[] =
	    "-----------------------------------------------------------------";

	if (flags & DCMD_ADDRSPEC) {
		char buf[MDB_NV_NAMELEN + 1];
		mdb_idcmd_t idc;
		mdb_frame_t f;
		mdb_cmd_t c;

		if (mdb_vread(&f, sizeof (f), addr) == -1) {
			mdb_warn("failed to read frame at %p", addr);
			return (DCMD_ERR);
		}

		if (mdb_vread(&c, sizeof (c), (uintptr_t)f.f_cp) < 0 ||
		    mdb_vread(&idc, sizeof (idc), (uintptr_t)c.c_dcmd) < 0 ||
		    mdb_readstr(buf, sizeof (buf), (uintptr_t)idc.idc_name) < 1)
			(void) strcpy(buf, "?");

		mdb_printf("+>\tframe %p (%s)\n", addr, buf);
		mdb_printf("\tf_cmds = %-?p\tf_istk = %p\n",
		    addr + OFFSETOF(mdb_frame_t, f_cmds),
		    addr + OFFSETOF(mdb_frame_t, f_istk));
		mdb_printf("\tf_wcbs = %-?p\tf_ostk = %p\n",
		    f.f_wcbs, addr + OFFSETOF(mdb_frame_t, f_ostk));
		mdb_printf("\tf_mblks = %-?p\tf_prev = %p\n",
		    f.f_mblks, f.f_prev);
		mdb_printf("\tf_pcmd = %-?p\tf_pcb = %p\n",
		    f.f_pcmd, addr + OFFSETOF(mdb_frame_t, f_pcb));
		mdb_printf("\tf_cp = %-?p\t\tf_flags = 0x%x\n",
		    f.f_cp, f.f_flags);
		mdb_printf("%s\n", sep);

	} else {
		mdb_printf("%s\n", sep);
		(void) mdb_walk_dcmd("mdb_frame", "mdb_stack", argc, argv);
	}

	return (DCMD_OK);
}

static int
cmd_frame(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if ((flags & DCMD_ADDRSPEC) && argc == 0)
		return (cmd_stack(addr, flags, argc, argv));

	return (DCMD_USAGE);
}

/*ARGSUSED*/
static int
cmd_iob(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_iob_t iob;
	mdb_io_t io;

	if (!(flags & DCMD_ADDRSPEC) || argc != 0)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%?s %6s %6s %?s %s\n",
		    "IOB", "NBYTES", "FLAGS", "IOP", "OPS");
	}

	if (mdb_vread(&iob, sizeof (iob), addr) == -1 ||
	    mdb_vread(&io, sizeof (io), (uintptr_t)iob.iob_iop) == -1) {
		mdb_warn("failed to read iob at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%?p %6lu %6x %?p %a\n", addr, (ulong_t)iob.iob_nbytes,
	    iob.iob_flags, iob.iob_iop, io.io_ops);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_in(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_printf("%p\n", M.m_in);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_out(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_printf("%p\n", M.m_out);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_err(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_printf("%p\n", M.m_err);
	return (DCMD_OK);
}

static int
iob_stack_walk_init(mdb_walk_state_t *wsp)
{
	mdb_iob_stack_t stk;

	if (mdb_vread(&stk, sizeof (stk), wsp->walk_addr) == -1) {
		mdb_warn("failed to read iob_stack at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)stk.stk_top;
	return (WALK_NEXT);
}

static int
iob_stack_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t addr = wsp->walk_addr;
	mdb_iob_t iob;

	if (addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(&iob, sizeof (iob), addr) == -1) {
		mdb_warn("failed to read iob at %p", addr);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)iob.iob_next;
	return (wsp->walk_callback(addr, &iob, wsp->walk_cbdata));
}

static int
frame_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL)
		wsp->walk_addr = (uintptr_t)M.m_frame;

	return (WALK_NEXT);
}

static int
frame_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t addr = wsp->walk_addr;
	mdb_frame_t f;

	if (addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(&f, sizeof (f), addr) == -1) {
		mdb_warn("failed to read frame at %p", addr);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)f.f_prev;
	return (wsp->walk_callback(addr, &f, wsp->walk_cbdata));
}

static const mdb_dcmd_t dcmds[] = {
	{ "mdb_stack", "?", "print debugger stack", cmd_stack },
	{ "mdb_frame", ":", "print debugger frame", cmd_frame },
	{ "mdb_iob", ":", "print i/o buffer information", cmd_iob },
	{ "mdb_in", NULL, "print stdin iob", cmd_in },
	{ "mdb_out", NULL, "print stdout iob", cmd_out },
	{ "mdb_err", NULL, "print stderr iob", cmd_err },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "mdb_frame", "iterate over mdb_frame stack",
		frame_walk_init, frame_walk_step, NULL },
	{ "mdb_iob_stack", "iterate over mdb_iob_stack elements",
		iob_stack_walk_init, iob_stack_walk_step, NULL },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	char buf[256], *p;
	uintptr_t addr;
	int rcount;

	if (mdb_readvar(&M, "mdb") == -1) {
		mdb_warn("failed to read 'mdb' struct");
		return (NULL);
	}

	if (mdb_readvar(&addr, "_mdb_abort_str") != -1 && addr != NULL &&
	    mdb_readstr(buf, sizeof (buf), addr) > 0) {
		if ((p = strchr(buf, ':')) != NULL &&
		    (strncmp(p, ": assertion", strlen(": assertion")) == 0))
			p[1] = '\n'; /* split across multiple lines */
		mdb_printf("mdb: debugger failed with error:\n%s\n", buf);
	}

	if (mdb_readvar(&rcount, "_mdb_abort_rcount") != -1 && rcount != 0)
		mdb_printf("mdb: WARNING: resume executed %d times\n", rcount);

	return (&modinfo);
}
