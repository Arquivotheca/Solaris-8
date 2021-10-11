/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_frame.c	1.2	99/11/19 SMI"

/*
 * Utility routines to manage debugger frames and commands.  A debugger frame
 * is used by each invocation of mdb_run() (the main parsing loop) to manage
 * its state.  Refer to the comments in mdb.c for more information on frames.
 * Each frame has a list of commands (that is, a dcmd, argument list, and
 * optional address list) that represent a pipeline after it has been parsed.
 */

#include <mdb/mdb_debug.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_io.h>
#include <mdb/mdb.h>

mdb_cmd_t *
mdb_cmd_create(mdb_idcmd_t *idcp, mdb_argvec_t *argv)
{
	mdb_cmd_t *cp = mdb_zalloc(sizeof (mdb_cmd_t), UM_NOSLEEP);

	if (cp == NULL) {
		warn("failed to allocate memory for command");
		longjmp(mdb.m_frame->f_pcb, MDB_ERR_NOMEM);
	}

	mdb_list_append(&mdb.m_frame->f_cmds, cp);
	mdb_argvec_copy(&cp->c_argv, argv);
	mdb_argvec_zero(argv);
	cp->c_dcmd = idcp;

	return (cp);
}

void
mdb_cmd_destroy(mdb_cmd_t *cp)
{
	mdb_addrvec_destroy(&cp->c_addrv);
	mdb_argvec_destroy(&cp->c_argv);
	mdb_vcb_purge(cp->c_vcbs);
	mdb_free(cp, sizeof (mdb_cmd_t));
}

void
mdb_cmd_reset(mdb_cmd_t *cp)
{
	mdb_addrvec_destroy(&cp->c_addrv);
	mdb_vcb_purge(cp->c_vcbs);
	cp->c_vcbs = NULL;
}

void
mdb_frame_reset(mdb_frame_t *fp)
{
	mdb_cmd_t *cp;

	while ((cp = mdb_list_next(&fp->f_cmds)) != NULL) {
		mdb_list_delete(&fp->f_cmds, cp);
		mdb_cmd_destroy(cp);
	}

	while (mdb_iob_stack_size(&fp->f_ostk) != 0) {
		mdb_iob_destroy(mdb.m_out);
		mdb.m_out = mdb_iob_stack_pop(&fp->f_ostk);
	}

	mdb_wcb_purge(&fp->f_wcbs);
	mdb_recycle(&fp->f_mblks);
}

void
mdb_frame_push(mdb_frame_t *fp)
{
	mdb_intr_disable();

	if (mdb.m_fmark == NULL)
		mdb.m_fmark = fp;

	bzero(fp, sizeof (mdb_frame_t));
	fp->f_prev = mdb.m_frame;
	fp->f_flags = mdb.m_flags & MDB_FL_VOLATILE;
	fp->f_pcmd = mdb.m_frame->f_pcmd;

	mdb.m_frame = fp;
	mdb.m_depth++;

	mdb_dprintf(MDB_DBG_DSTK, "[%u] push frame %p, mark=%p in=%s out=%s\n",
	    mdb.m_depth, (void *)fp, (void *)mdb.m_fmark,
	    mdb_iob_name(mdb.m_in), mdb_iob_name(mdb.m_out));

	mdb_intr_enable();
}

void
mdb_frame_pop(mdb_frame_t *fp, int err)
{
	mdb_intr_disable();

	ASSERT(mdb_iob_stack_size(&fp->f_istk) == 0);
	ASSERT(mdb_iob_stack_size(&fp->f_ostk) == 0);
	ASSERT(mdb_list_next(&fp->f_cmds) == NULL);
	ASSERT(fp->f_mblks == NULL);
	ASSERT(fp->f_wcbs == NULL);

	mdb_dprintf(MDB_DBG_DSTK, "[%u] pop frame %p, status=%s\n",
	    mdb.m_depth, (void *)fp, mdb_err2str(err));

	if (mdb.m_frame != fp) {
		mdb_frame_t *pfp = mdb.m_frame;

		while (pfp != NULL && pfp->f_prev != fp)
			pfp = pfp->f_prev;

		if (pfp == NULL)
			fail("frame %p not on debugger stack\n", (void *)fp);

		pfp->f_prev = fp->f_prev;

	} else {
		mdb.m_flags &= ~MDB_FL_VOLATILE;
		mdb.m_flags |= fp->f_flags;
		mdb.m_frame = fp->f_prev;
	}

	ASSERT(mdb.m_depth != 0);
	mdb.m_depth--;

	if (mdb.m_fmark == fp)
		mdb.m_fmark = NULL;

	mdb_intr_enable();
}
