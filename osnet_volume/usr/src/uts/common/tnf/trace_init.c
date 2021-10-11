/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)trace_init.c	1.20	97/10/28 SMI"

/*
 * Includes
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/tnf.h>
#include <sys/thread.h>

#include "tnf_buf.h"
#include "tnf_types.h"
#include "tnf_trace.h"

#ifndef NPROBE

/*
 * Globals
 */

size_t tnf_trace_file_size = TNF_TRACE_FILE_DEFAULT;

/*
 * tnf_trace_on
 */
void
tnf_trace_on(void)
{
	TNFW_B_UNSET_STOPPED(tnfw_b_state);
	tnf_tracing_active = 1;
	/* Enable system call tracing for all processes */
	set_all_proc_sys();
}

/*
 * tnf_trace_off
 */
void
tnf_trace_off(void)
{
	TNFW_B_SET_STOPPED(tnfw_b_state);
	tnf_tracing_active = 0;
	/* System call tracing is automatically disabled */
}

/*
 * tnf_trace_init
 * 	Not reentrant: only called from tnf_allocbuf(), which is
 *	single-threaded.
 */
void
tnf_trace_init(void)
{
	int stopped;
	tnf_ops_t *ops;

	ASSERT(tnf_buf != NULL);
	ASSERT(!tnf_tracing_active);

	stopped = tnfw_b_state & TNFW_B_STOPPED;

	/*
	 * Initialize the buffer
	 */
	tnfw_b_init_buffer(tnf_buf, tnf_trace_file_size);

	/*
	 * Mark allocator running (not stopped). Luckily,
	 * tnf_trace_alloc() first checks tnf_tracing_active, so no
	 * trace data will be written.
	 */
	tnfw_b_state = TNFW_B_RUNNING;

	/*
	 * 1195835: Write out some tags now.  The stopped bit needs
	 * to be clear while we do this.
	 */
	/* LINTED pointer cast may result in improper alignment */
	if ((ops = (tnf_ops_t *)curthread->t_tnf_tpdp) != NULL) {
		tnf_tag_data_t	*tag;
		TNFW_B_POS	*pos;

		ASSERT(!LOCK_HELD(&ops->busy));
		LOCK_INIT_HELD(&ops->busy); /* XXX save a call */

		tag = TAG_DATA(tnf_struct_type);
		(void) tag->tag_desc(ops, tag);
		tag = TAG_DATA(tnf_probe_type);
		(void) tag->tag_desc(ops, tag);
		tag = TAG_DATA(tnf_kernel_schedule);
		(void) tag->tag_desc(ops, tag);

		pos = &ops->wcb.tnfw_w_tag_pos;
		TNFW_B_COMMIT(pos);
		pos = &ops->wcb.tnfw_w_pos;
		TNFW_B_ROLLBACK(pos);

		LOCK_INIT_CLEAR(&ops->busy); /* XXX save a call */
	}

	/* Restore stopped bit */
	tnfw_b_state |= stopped;
}

#endif /* NPROBE */
