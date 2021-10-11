/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tnf_res.c	1.28	98/11/13 SMI"

/*
 * "Resident" part of TNF -- this has to be around even when the
 * driver is not loaded.
 */

#ifndef NPROBE
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/klwp.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/msacct.h>
#include <sys/tnf_com.h>
#include <sys/tnf_writer.h>
#include <sys/tnf_probe.h>
#include <sys/tnf.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/kobj.h>

#include "tnf_buf.h"
#include "tnf_types.h"
#include "tnf_trace.h"

/*
 * Defines
 */

#define	TNF_PC_COUNT	8

/*
 * Declarations
 */

/*
 * tnf_mod_load and tnf_mod_unload globals
 */
extern void *__tnf_probe_list_head;
extern void *__tnf_tag_list_head;
int    tnf_changed_probe_list = 1; /* 1=probe list has changed; 0 = not */

/*
 * This makes the state of the TNFW_B_STOPPED bit externally visible
 * in the kernel.
 */
volatile int tnf_tracing_active = 0;

/*
 * The trace buffer pointer
 */
caddr_t tnf_buf;

static uintptr_t *pcstack(uintptr_t *);

/*
 * Stub definitions for tag data pointers
 */

/* tnf_writer module */
tnf_tag_data_t *tnf_inline_tag_data = NULL;
tnf_tag_data_t *tnf_tagged_tag_data = NULL;
tnf_tag_data_t *tnf_scalar_tag_data = NULL;
tnf_tag_data_t *tnf_char_tag_data = NULL;
tnf_tag_data_t *tnf_int8_tag_data = NULL;
tnf_tag_data_t *tnf_uint8_tag_data = NULL;
tnf_tag_data_t *tnf_int16_tag_data = NULL;
tnf_tag_data_t *tnf_uint16_tag_data = NULL;
tnf_tag_data_t *tnf_int32_tag_data = NULL;
tnf_tag_data_t *tnf_uint32_tag_data = NULL;
tnf_tag_data_t *tnf_int64_tag_data = NULL;
tnf_tag_data_t *tnf_uint64_tag_data = NULL;
tnf_tag_data_t *tnf_float32_tag_data = NULL;
tnf_tag_data_t *tnf_float64_tag_data = NULL;
tnf_tag_data_t *tnf_array_tag_data = NULL;
tnf_tag_data_t *tnf_string_tag_data = NULL;
tnf_tag_data_t *tnf_type_array_tag_data = NULL;
tnf_tag_data_t *tnf_name_array_tag_data = NULL;
tnf_tag_data_t *tnf_derived_tag_data = NULL;
tnf_tag_data_t *tnf_align_tag_data = NULL;
tnf_tag_data_t *tnf_derived_base_tag_data = NULL;
tnf_tag_data_t *tnf_element_type_tag_data = NULL;
tnf_tag_data_t *tnf_header_size_tag_data = NULL;
tnf_tag_data_t *tnf_name_tag_data = NULL;
tnf_tag_data_t *tnf_opaque_tag_data = NULL;
tnf_tag_data_t *tnf_properties_tag_data = NULL;
tnf_tag_data_t *tnf_self_size_tag_data = NULL;
tnf_tag_data_t *tnf_size_tag_data = NULL;
tnf_tag_data_t *tnf_slot_names_tag_data = NULL;
tnf_tag_data_t *tnf_slot_types_tag_data = NULL;
tnf_tag_data_t *tnf_tag_tag_data = NULL;
tnf_tag_data_t *tnf_tag_arg_tag_data = NULL;
tnf_tag_data_t *tnf_type_size_tag_data = NULL;
tnf_tag_data_t *tnf_struct_tag_data = NULL;
tnf_tag_data_t *tnf_file_header_tag_data = NULL;
tnf_tag_data_t *tnf_block_header_tag_data = NULL;
tnf_tag_data_t *tnf_type_tag_data = NULL;
tnf_tag_data_t *tnf_array_type_tag_data = NULL;
tnf_tag_data_t *tnf_derived_type_tag_data = NULL;
tnf_tag_data_t *tnf_scalar_type_tag_data = NULL;
tnf_tag_data_t *tnf_struct_type_tag_data = NULL;

/* tnf_trace module */
tnf_tag_data_t *tnf_probe_event_tag_data = NULL;
tnf_tag_data_t *tnf_time_base_tag_data = NULL;
tnf_tag_data_t *tnf_time_delta_tag_data = NULL;
tnf_tag_data_t *tnf_pid_tag_data = NULL;
tnf_tag_data_t *tnf_lwpid_tag_data = NULL;
tnf_tag_data_t *tnf_kthread_id_tag_data = NULL;
tnf_tag_data_t *tnf_cpuid_tag_data = NULL;
tnf_tag_data_t *tnf_device_tag_data = NULL;
tnf_tag_data_t *tnf_symbol_tag_data = NULL;
tnf_tag_data_t *tnf_symbols_tag_data = NULL;
tnf_tag_data_t *tnf_sysnum_tag_data = NULL;
tnf_tag_data_t *tnf_microstate_tag_data = NULL;
tnf_tag_data_t *tnf_offset_tag_data = NULL;
tnf_tag_data_t *tnf_fault_type_tag_data = NULL;
tnf_tag_data_t *tnf_seg_access_tag_data = NULL;
tnf_tag_data_t *tnf_bioflags_tag_data = NULL;
tnf_tag_data_t *tnf_diskaddr_tag_data = NULL;
tnf_tag_data_t *tnf_kernel_schedule_tag_data = NULL;

tnf_tag_data_t *tnf_probe_type_tag_data = NULL;

/* Exported properties */
tnf_tag_data_t	***tnf_user_struct_properties = NULL;

/*
 * tnf_thread_create()
 * Called from thread_create() to initialize thread's tracing state.
 * XXX Do this when tracing is first turned on
 */

void
tnf_thread_create(kthread_t *t)
{
	/* If the allocation fails, this thread doesn't trace */
	t->t_tnf_tpdp = kmem_zalloc(sizeof (tnf_ops_t), KM_NOSLEEP);

	TNF_PROBE_3(thread_create, "thread", /* CSTYLED */,
		tnf_kthread_id,	tid,		t,
		tnf_pid,	pid,		ttoproc(t)->p_pid,
		tnf_symbol,	start_pc,	t->t_startpc);
}

/*
 * tnf_thread_exit()
 * Called from thread_exit() and lwp_exit() if thread has a tpdp.
 * From this point on, we're off the allthreads list
 */

void
tnf_thread_exit(void)
{
	tnf_ops_t *ops;
	tnf_block_header_t *block;

	TNF_PROBE_0(thread_exit, "thread", /* CSTYLED */);
        /* LINTED pointer cast may result in improper alignment */
	ops = (tnf_ops_t *)curthread->t_tnf_tpdp;
	/*
	 * Mark ops as busy from now on, so it will never be used
	 * again.  If we fail on the busy lock, the buffer
	 * deallocation code is cleaning our ops, so we don't need to
	 * do anything.  If we get the lock and the buffer exists,
	 * release all blocks we hold.  Once we're off allthreads,
	 * the deallocator will not examine our ops.
	 */
	if (ops->busy)
		return;
	LOCK_INIT_HELD(&ops->busy);
	if (tnf_buf != NULL) {
		/* Release any A-locks held */
		block = ops->wcb.tnfw_w_pos.tnfw_w_block;
		ops->wcb.tnfw_w_pos.tnfw_w_block = NULL;
		if (block != NULL)
			lock_clear(&block->A_lock);
		block = ops->wcb.tnfw_w_tag_pos.tnfw_w_block;
		ops->wcb.tnfw_w_tag_pos.tnfw_w_block = NULL;
		if (block != NULL)
			lock_clear(&block->A_lock);
	}
}

/*
 * Called from thread_free() if thread has tpdp.
 */

void
tnf_thread_free(kthread_t *t)
{
	tnf_ops_t *ops;
	/* LINTED pointer cast may result in improper alignment */
	ops = (tnf_ops_t *)t->t_tnf_tpdp;
	t->t_tnf_tpdp = NULL;
	kmem_free(ops, sizeof (*ops));
}

/*
 * tnf_thread_queue()
 * Probe wrapper called when tracing is enabled and a thread is
 * placed on some dispatch queue.
 */

void
tnf_thread_queue(kthread_t *t, cpu_t *cp, pri_t tpri)
{
	TNF_PROBE_4(thread_queue, "dispatcher", /* CSTYLED */,
		tnf_kthread_id,		tid,		t,
		tnf_cpuid,		cpuid,		cp->cpu_id,
		tnf_long,		priority,	tpri,
		tnf_ulong,		queue_length,
			/* cp->cpu_disp.disp_q[tpri].dq_sruncnt */
			cp->cpu_disp.disp_nrunnable);

	TNF_PROBE_2(thread_state, "thread", /* CSTYLED */,
		tnf_kthread_id,		tid,		t,
		tnf_microstate,		state,		LMS_WAIT_CPU);
}

/*
 * pcstack(): fill in, NULL-terminate and return pc stack.
 */

static uintptr_t *
pcstack(uintptr_t *pcs)
{
	u_int n;

	n = getpcstack(pcs, TNF_PC_COUNT);
	pcs[n] = 0;
	return (pcs);
}

/*
 * tnf_thread_switch()
 * Probe wrapper called when tracing enabled and curthread is about to
 * switch to the next thread.
 * XXX Simple sleepstate and runstate calculations
 */

#define	SLPSTATE(t, ts)					\
	(((ts) == TS_STOPPED) ? LMS_STOPPED :		\
	    ((t)->t_wchan0 ? LMS_USER_LOCK : LMS_SLEEP))

#define	RUNSTATE(next, lwp)				\
	((((lwp = ttolwp(next)) != NULL) &&		\
		lwp->lwp_state == LWP_USER) ?		\
		LMS_USER : LMS_SYSTEM)

void
tnf_thread_switch(kthread_t *next)
{
	kthread_t	*t;
	klwp_t		*lwp;
	caddr_t		ztpdp;
	int		borrow;
	u_int		ts;
	uintptr_t	pcs[TNF_PC_COUNT + 1];

	t = curthread;
	ts = t->t_state;

	/*
	 * If we're a zombie, borrow idle thread's tpdp.  This lets
	 * the driver decide whether the buffer is busy by examining
	 * allthreads (idle threads are always on the list).
	 */
	if ((borrow = (ts == TS_ZOMB)) != 0) {
		ztpdp = t->t_tnf_tpdp;
		t->t_tnf_tpdp = CPU->cpu_idle_thread->t_tnf_tpdp;
		goto do_next;
	}

	/*
	 * If we're blocked, test the blockage probe
	 */
	if (ts == TS_SLEEP && t->t_wchan)
#if defined(_LP64)
	/* LINTED pointer cast may result in improper alignment */
		TNF_PROBE_2(thread_block, "synch", /* CSTYLED */,
		    tnf_opaque,		reason,		t->t_wchan,
		    tnf_symbols,	stack,		(void **)pcstack(pcs));
#else
	TNF_PROBE_2(thread_block, "synch", /* CSTYLED */,
		tnf_opaque,	reason,	t->t_wchan,
		tnf_symbols,	stack,	(void **)pcstack(pcs));
#endif

	/*
	* Record outgoing thread's state
	* Kernel thread ID is implicit in schedule record
	* supress lint: cast from 32-bit integer to 8-bit integer
	* tnf_microstate_t = tnf_uint8_t
	*/
#if defined(_LP64)
	/* LINTED */
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
	    tnf_microstate,	state,		SLPSTATE(t, ts));
#else
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
		tnf_microstate,	state,	SLPSTATE(t, ts));
#endif

do_next:
	/*
	* Record incoming thread's state
	*
	* supress lint: cast from 32-bit integer to 8-bit integer
	* tnf_microstate_t = tnf_uint8_t
	*/
#if defined(_LP64)
	/* LINTED */
	TNF_PROBE_2(thread_state, "thread", /* CSTYLED */,
	    tnf_kthread_id,	tid,		next,
	    tnf_microstate,	state,		RUNSTATE(next, lwp));
#else
	TNF_PROBE_2(thread_state, "thread", /* CSTYLED */,
		tnf_kthread_id,	tid,	next,
		tnf_microstate,	state,	RUNSTATE(next, lwp));
#endif

	/*
	 * If we borrowed idle thread's tpdp above, restore the zombies
	 * tpdp so that it will be freed from tnf_thread_free().
	 */
	if (borrow)
		t->t_tnf_tpdp = ztpdp;

}

#endif	/* NPROBE */

/*
 * Resets probe count. Must be called from loadable modules at _init
 * entry point.
 */
int
tnf_mod_load()
{
	tnf_changed_probe_list = 1;
	return (0);
}

/*
 * Unsplices module's probes and tags out of the global lists.
 * Must be called from unloadable modules.
 */

int
tnf_mod_unload(struct modlinkage *mlp)
{
	struct modctl		*mcp;
	void			*mp;
	tnf_probe_control_t	**ppp, *pp;
	tnf_tag_data_t		**tpp, *tp;

	if ((mcp = mod_getctl(mlp)) == NULL)
		return (EUNATCH);

	mp = mcp->mod_mp;

	if (mp == NULL) {
		/* should we log an error ? */
		return (EUNATCH);
	}

	mutex_enter(&mod_lock); /* protects probe and tag lists */

	/*
	 * Unsplice probes
	 */
	ppp = (tnf_probe_control_t **)&__tnf_probe_list_head;
	while (ppp && (pp = *ppp))
		if (kobj_addrcheck(mp, (caddr_t)pp) == 0) {
			/*
			 * prex should not be attached
			 */
			*ppp = pp->next;
		} else
			ppp = &pp->next;
	/*
	 * Unsplice tags
	 */
	tpp = (tnf_tag_data_t **)&__tnf_tag_list_head;
	while (tpp && (tp = *tpp))
		if (kobj_addrcheck(mp, (caddr_t)tp) == 0)
			*tpp = (tnf_tag_data_t *)tp->tag_version; /* next */
		else
			tpp = (tnf_tag_data_t **)&tp->tag_version;
	mutex_exit(&mod_lock);  /* protects probe and tag lists */

	tnf_changed_probe_list = 1;

	return (0);
}
