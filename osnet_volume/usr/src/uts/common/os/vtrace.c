/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vtrace.c	1.32	99/09/23 SMI"

/*
 * Code to drive the kernel tracing mechanism and vtrace system call
 */

#ifdef	TRACE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/spl.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>

/*
 * Trace buffer locking.
 *
 * Since trace buffers are per-CPU, it suffices to simply block interrupts
 * while dumping a trace record.  This is cheaper than grabbing a mutex,
 * and makes it possible to put trace points in places that would otherwise
 * be illegal (e.g., while schedlock is held).
 *
 * For speed, the routines trace_0(), ..., trace_5() are implemented as
 * fast traps.  There is also a trace_write_buffer() trap wrapper, which
 * provides a generic mechanism for writing data to a trace buffer.
 * See trace_write_data() for sample usage.
 *
 * The flushing process doesn't need any locking at all, because all it
 * has to do is read the head pointer (an atomic operation) and the tail
 * pointer (which is not modified by anyone else).  The data behind the
 * head pointer is always valid, because trace points don't update it until
 * *after* writing in their data.
 */

/*
 * Event map.
 *
 * The event map is a per-CPU bitmap describing which events are enabled
 * and which have been used (i.e., encountered at least once).
 * There is one byte per event, structured as follows:
 *
 * enabled:used:reserved:string5:string4:string3:string2:string1
 *
 * The enabled bit (7) determines whether or not we should trace the event.
 * The used bit (6) determines whether or not we should label the event.
 * (Events are marked as used after they are labeled.)
 * The string bits (0-4) specify whether the corresponding data word is an
 * integer (0) or a string pointer (1).
 *
 * When tracing is enabled, each CPU's event_map points to its real_event_map;
 * when tracing is disabled, each CPU's event_map points to the global
 * null_event_map.  This wastes some memory (64K) compared to testing a
 * global variable, but saves a load/compare/branch at every trace point.
 */

uchar_t	null_event_map[VT_MAPSIZE];	/* when tracing is disabled */

ulong_t	bytes2data[512];		/* see store_data() */

static	pgcnt_t trace_npages;	/* number of trace buffer pages */
static	size_t trace_nbytes;	/* above, times PAGESIZE */
static	int trace_lowater;	/* trace buffer low water mark, in bytes */

static	vt_global_info_t	global_trace_info;
static	vt_cpu_info_t		*cpu_trace_info;

static char	*overflow_err;
tracedata_t	**tracedata_ptrs;	/* to facilitate crash analysis */

/*
 * Kernel tracing is controlled by a small state machine.  The states are:
 *
 *	VTR_STATE_NULL		all tracing off, no resources allocated
 *	VTR_STATE_READY		all tracing off, all resources allocated
 *	VTR_STATE_PAUSE		administrative tracing on, everything else off
 *	VTR_STATE_ACTIVE	all tracing on
 *	VTR_STATE_PERPROC	all tracing on, on a per-process basis
 *	VTR_STATE_HALTED	all tracing off, resources not yet freed
 *
 * The legal state transitions and associated mechanisms are:
 *
 *	Old	New		Transition Mechanism(s)
 *	State	State	kernel			userland
 *	-----	-----	---------------------------------------------------
 *	NULL	READY	trace_init()		vtrace(VTR_INIT)
 *	READY	PAUSE	trace_start()		vtrace(VTR_START)
 *	PAUSE	ACTIVE	trace_resume()		vtrace(VTR_RESUME)
 *	PAUSE	PERPROC	trace_resume()		vtrace(VTR_RESUME)
 *	ACTIVE	PAUSE	trace_pause()		vtrace(VTR_PAUSE)
 *	PERPROC	PAUSE	trace_pause()		vtrace(VTR_PAUSE)
 *	PAUSE	HALTED	trace_halt()		*
 *	ACTIVE	HALTED	trace_halt()		*
 *	PERPROC	HALTED	trace_halt()		*
 *	[any]	NULL	trace_reset()		vtrace(VTR_RESET)
 *
 * Once tracing begins, only the tracing process is allowed to perform these
 * transitions, with the following exceptions:
 *
 * (1)	PAUSE <--> ACTIVE, PAUSE <--> PERPROC.
 *	Anyone can toggle the tracing state between PAUSE and ACTIVE, or
 *	between PAUSE and PERPROC, by calling trace_resume() / trace_pause()
 *	from inside the kernel, or via the system calls vtrace(VTR_RESUME) /
 *	vtrace(VTR_PAUSE) from userland.  This allows you to enable tracing
 *	only during specific activities.  For example, if you just want to
 *	observe clock thread activity, you can put calls to trace_resume()
 *	and trace_pause() around the call to clock().  (These functions
 *	are no-ops if you aren't in the proper state when you call them,
 *	so you don't have to do any checking.)
 *
 * (2)	PAUSE --> HALTED, ACTIVE --> HALTED, PERPROC --> HALTED.
 *	Anyone can move the tracing state from PAUSE, ACTIVE, or PERPROC
 *	to HALTED by calling trace_halt() from inside the kernel.
 *	As described above, the trace_resume() / trace_pause() pair allows
 *	you to trace all occurrences of a selected activity.  If you just want
 *	to trace the *first* occurrence, you can use the trace_resume() /
 *	trace_halt() pair instead.
 *
 * Since any thread can perform transitions out of the PAUSE, ACTIVE, and
 * PERPROC states, as described above, all such transitions must be protected
 * by tracing_state_lock.  Transitions out of other states can only be
 * performed by the tracing process, so no locking is needed.
 * (tracing_state_lock also protects updates to tracing_pid.)
 *
 * Finally, anyone can force a reset by calling trace_reset(VTR_FORCE) from
 * inside the kernel, or via the system call vtrace(VTR_RESET, VTR_FORCE)
 * from userland.  This feature is provided as a safety valve in case the
 * tracing process gets wedged.
 */

int			tracing_state = VTR_STATE_NULL;
static	int		new_labels;
static	int		trace_forced_reset = 0;
static	kmutex_t	tracing_state_lock;
static	kmutex_t	trace_reset_lock;
static	pid_t		tracing_pid = 0;
static	cred_t		*tracing_cred;

#define	PAUSE_OR_ACTIVE \
	(VTR_STATE_PAUSE | VTR_STATE_ACTIVE)
#define	ADMIN_TRACING \
	(VTR_STATE_PAUSE | VTR_STATE_ACTIVE | VTR_STATE_PERPROC)

/*
 * Per-process tracing.
 *
 * Normally, tracing is active for all processes in the system.  However,
 * you can choose to trace only a certain subset of processes by calling
 * trace_set_process_list() from inside the kernel, or by using the system
 * call vtrace(VTR_PROCESS) from userland.  trace_set_process_list() takes a
 * pointer to an array of PIDs and a count.  If count > 0, only the listed
 * processes are traced; if count < 0, *all but* the listed processes are
 * traced; and if count == 0, tracing returns to the normal mode (trace all
 * processes).
 *
 * Calls to trace_set_process_list() are not cumulative: each call blows away
 * the existing trace_process_list.
 *
 * The current process list is kept in trace_process_list.  The current state
 * of per-process tracing is kept in trace_process_mode; the values are:
 *
 *	VTR_PROCESS_NULL	ignore trace_process_list, trace everything
 *	VTR_PROCESS_TRACE	only trace processes in trace_process_list
 *	VTR_PROCESS_NOTRACE	only trace processes not in trace_process_list
 *
 * Only the tracing process is allowed to update trace_process_list,
 * trace_nprocs, and trace_process_mode.
 *
 * To implement per-process tracing, we call trace_check_process() just
 * before the end of resume().  If the resuming thread's PID is in the
 * trace_process_list, then we either enable or disable tracing on the host
 * CPU, depending on whether the trace_process_mode is TRACE or NOTRACE,
 * respectively; if the resuming thread's PID is *not* in the list, we just
 * toggle the above logic.  Notice that in the PERPROC state, we will in
 * general have a mix of active and quiet (wrt tracing) CPUs.
 *
 * We don't allow more than MAX_TRACE_NPROCS processes to be traced on an
 * individual basis like this, because of the Heisenberg effect on resume().
 */

static	pid_t	trace_process_list[MAX_TRACE_NPROCS];
static	int	trace_process_mode = VTR_PROCESS_NULL;
static	int	trace_nprocs = 0;

int tracing_exists = 1;

static	hrtime_t start_hrtime, abs_hrtime;

#define	IS_VALID_CPU(i)	((cpu[i] != NULL) && (cpu[i]->cpu_flags & CPU_EXISTS))

void
inittrace(void)
{
	int i;

	/*
	 * Initialize the bytes2data array.  This array is used to determine
	 * the appropriate size data record for dumping strings.
	 * bytes2data[len] is the header word for the smallest data record
	 * that can hold a string of length len (including NULL terminator);
	 * bytes2data[len + 256] is the length, in bytes, of that data record.
	 */
	for (i = 0; i <= 4 * 4 - 4; i++) {
		bytes2data[i] = FTT2HEAD(TR_FAC_TRACE, TR_DATA_4, 0);
		bytes2data[i + 256] = 16;
	}
	for (i = 4 * 4 - 3; i <= 8 * 4 - 4; i++) {
		bytes2data[i] = FTT2HEAD(TR_FAC_TRACE, TR_DATA_8, 0);
		bytes2data[i + 256] = 32;
	}
	for (i = 8 * 4 - 3; i <= 16 * 4 - 4; i++) {
		bytes2data[i] = FTT2HEAD(TR_FAC_TRACE, TR_DATA_16, 0);
		bytes2data[i + 256] = 64;
	}
	for (i = 16 * 4 - 3; i <= 32 * 4 - 4; i++) {
		bytes2data[i] = FTT2HEAD(TR_FAC_TRACE, TR_DATA_32, 0);
		bytes2data[i + 256] = 128;
	}
	for (i = 32 * 4 - 3; i <= 64 * 4 - 4; i++) {
		bytes2data[i] = FTT2HEAD(TR_FAC_TRACE, TR_DATA_64, 0);
		bytes2data[i + 256] = 256;
	}
	/*
	 * Allocate the per-cpu data structures.
	 */
	overflow_err = (char *) kmem_alloc(sizeof (char) * NCPU, KM_SLEEP);
	cpu_trace_info = (vt_cpu_info_t *)
	    kmem_alloc(sizeof (vt_cpu_info_t) * NCPU, KM_SLEEP);
	tracedata_ptrs = (tracedata_t **)
	    kmem_alloc(sizeof (tracedata_t *) * NCPU, KM_SLEEP);

	mutex_init(&tracing_state_lock, NULL, MUTEX_SPIN,
	    (void *)ipltospl(SPL7));
	mutex_init(&trace_reset_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * All platforms except SPARC-V9 use microsecond timestamps at present,
 * so rather than replicate the following code everywhere, we provide
 * a common implementation for all non-SPARC-V9 platforms.
 */
#ifndef __sparcv9cpu

hrtime_t
get_vtrace_time(void)
{
	return (gethrtime() / (NANOSEC / MICROSEC));
}

uint_t
get_vtrace_frequency(void)
{
	return (MICROSEC);
}

#endif

/*
 * Utility macros: VT_BCOPY(), VT_BZERO(), TBUF_OPEN(), TBUF_WRITE()
 * Utility functions: store_string()
 */
#define	VT_BCOPY(src, targ, count) \
	{ \
		int xvt_i; \
		char *xvt_src = (char *)(src); \
		char *xvt_targ = (char *)(targ); \
		for (xvt_i = (count); xvt_i > 0; xvt_i--) \
			*xvt_targ++ = *xvt_src++; \
	}

#define	VT_BZERO(targ, count) \
	{ \
		int xvt_i; \
		char *xvt_targ = (char *)(targ); \
		for (xvt_i = (count); xvt_i > 0; xvt_i--) \
			*xvt_targ++ = 0; \
	}

/*
 * Trace buffer writes are done "on credit": you put your data there first,
 * then update the head pointer.  This way, data behind the head pointer
 * is always valid.
 */
#define	TBUF_OPEN(cp, tdp) \
	cp = tdp->tbuf_head;

#define	TBUF_WRITE(cp, tdp, size) \
	cp += size; \
	if (cp >= tdp->tbuf_wrap) { \
		while (cp < tdp->tbuf_end) { \
			*((ulong_t *)cp) = FTT2HEAD(TR_FAC_TRACE, TR_PAD, 0); \
			cp += sizeof (ulong_t); \
		} \
		cp = tdp->tbuf_start; \
	} \
	tdp->tbuf_head = cp;

/*
 * Store a string (including the NULL terminator) into a data record.
 * The size of the data record is determined by the length of the string.
 * Currently, the available sizes are 4, 8, 16, 32, and 64 words, which
 * makes the maximum string length 251 characters.
 * (64 words - header - NULL = (256 - 4 - 1) bytes = 251 chars).
 * The bytes2data array determines what size data record to use; for an
 * asciiz string of length n (n - 1 + NULL), we get the data record's header
 * word from bytes2data[n], and its size (in bytes) from bytes2data[n + 256].
 */
static void
store_string(tracedata_t *tdp, char *str)
{
	vt_pointer_t vtp;
	int len, size, bytes;

	for (len = 0; str[len]; len++);

	bytes = MIN(len + 1, VT_MAX_BYTES - 5);

	TBUF_OPEN(vtp.char_p, tdp);
	size = bytes2data[bytes + 256];
	VT_BZERO(vtp.char_p, size);
	vtp.generic_p->head = bytes2data[bytes];
	VT_BCOPY(str, vtp.generic_p->data, bytes);
	TBUF_WRITE(vtp.char_p, tdp, size);
}

/*
 * Write data to the trace buffer.  Uses the trace_write_buffer() trap
 * wrapper if tracing is enabled (cpunum == -1), or just writes to the
 * buffer directly if tracing is disabled (cpunum >= 0).
 */
static void
trace_write_data(char *data, int size, int cpunum)
{
	tracedata_t *tdp;
	vt_pointer_t vtp;

	if (cpunum == -1) {
		trace_write_buffer((ulong_t *)data, size);
	} else {
		tdp = &cpu[cpunum]->cpu_trace;
		TBUF_OPEN(vtp.char_p, tdp);
		VT_BCOPY(data, vtp.char_p, size);
		TBUF_WRITE(vtp.char_p, tdp, size);
	}
}

/*
 * Write thread info.  This should be called for every thread in the system
 * just before tracing is enabled, and then at every thread_create().
 * Note that this trace point is active even in the paused state; this
 * is necessary to capture complete thread information.
 */
void
trace_kthread_label(kthread_id_t t, int cpunum)
{
	vt_kthread_label_t ktl;

	if ((cpunum == -1) && !(tracing_state & ADMIN_TRACING))
		return;

	ktl.head	= FTT2HEAD(TR_FAC_TRACE, TR_KTHREAD_LABEL, 0);
	ktl.pid		= (ulong_t)(ttoproc(t)->p_pid);
	ktl.lwpid	= (ulong_t)(t->t_tid);
	ktl.tid		= (ulong_t)t;
	ktl.startpc	= (ulong_t)(t->t_startpc);

	if (cpunum == -1)
		trace_4(ktl.head, ktl.pid, ktl.lwpid, ktl.tid, ktl.startpc);
	else
		trace_write_data((char *)&ktl, sizeof (ktl), cpunum);
}

void
trace_process_name(ulong_t pid, char *name)
{
	if (tracing_state & ADMIN_TRACING)
		trace_2(FTT2HEAD(TR_FAC_TRACE, TR_PROCESS_NAME, 0x2),
			pid, (ulong_t)name);
}

void
trace_process_fork(ulong_t cpid, ulong_t ppid)
{
	if (tracing_state & ADMIN_TRACING)
		trace_2(FTT2HEAD(TR_FAC_TRACE, TR_PROCESS_FORK, 0x0),
			cpid, ppid);
}

int
trace_init(int npages, int lowater)
{
	int i;
	char *bp, *emp;
	tracedata_t *tdp;

	mutex_enter(&tracing_state_lock);
	if (tracing_pid != 0) {
		mutex_exit(&tracing_state_lock);
		return (EACCES);
	}
	tracing_pid = curproc->p_pid;
	tracing_cred = CRED();
	trace_forced_reset = 0;
	mutex_exit(&tracing_state_lock);

	if (tracing_state != VTR_STATE_NULL)
		return (EBUSY);

	trace_npages = npages;
	trace_lowater = lowater;
	trace_nbytes = trace_npages * PAGESIZE;
	trace_lowater *= PAGESIZE;

	/*
	 * Initialize pointers to NULL, counters to zero.
	 */

	for (i = 0; i < NCPU; i++) {
		tracedata_ptrs[i] = NULL;
		if (!IS_VALID_CPU(i))
			continue;
		tdp = &cpu[i]->cpu_trace;
		tracedata_ptrs[i] = tdp;
		tdp->real_event_map = NULL;
		tdp->tbuf_start = NULL;
		tdp->trace_file = NULL;
		tdp->tbuf_overflow = NULL;
		overflow_err[i] = 0;
		cpu_trace_info[i].bytes_flushed = 0;
		cpu_trace_info[i].max_flushsize = 0;
	}

	/*
	 * Allocate event map and trace buffer for each CPU.
	 * We check trace_forced_reset after every kmem_alloc,
	 * to make sure we haven't had the rug yanked out from
	 * under us by a trace_reset(VTR_FORCE).
	 */

	for (i = 0; i < NCPU; i++) {
		if (!IS_VALID_CPU(i))
			continue;
		tdp = &cpu[i]->cpu_trace;
		emp = kmem_zalloc(VT_MAPSIZE, KM_SLEEP);
		if (trace_forced_reset) {
			cmn_err(CE_NOTE, "vtrace: forced reset detected");
			kmem_free(emp, VT_MAPSIZE);
			return (EBUSY);
		}
		tdp->real_event_map = (uchar_t *)emp;
		bp = kmem_alloc(ptob(trace_npages), KM_SLEEP);
		if (trace_forced_reset) {
			cmn_err(CE_NOTE, "vtrace: forced reset detected");
			kmem_free(bp, ptob(trace_npages));
			return (EBUSY);
		}
		tdp->tbuf_start = bp;
		tdp->tbuf_end = bp + trace_nbytes;
		tdp->tbuf_wrap = tdp->tbuf_end -
			sizeof (vt_raw_kthread_id_t) -
			sizeof (vt_elapsed_time_t) -
			6 * sizeof (vt_generic_t);
		tdp->tbuf_head = bp;
		tdp->tbuf_tail = bp;
		tdp->tbuf_redzone = (char *)
			((uint_t)(tdp->tbuf_end - 0x1000) & ~0xfff);
		tdp->last_thread = curthread;
	}

	if (trace_forced_reset) {
		cmn_err(CE_NOTE, "vtrace: forced reset detected");
		return (EBUSY);
	}
	tracing_state = VTR_STATE_READY;
	return (0);
}

/*
 * Enable tracing: point each CPU's event_map at its real_event_map.
 */
void
trace_resume(void)
{
	int i;

	/*
	 * Don't even try to grab tracing_state_lock if tracing is off.
	 * This prevents callers from trying to enter an uninitialized
	 * mutex during boot.
	 */

	if (tracing_state != VTR_STATE_PAUSE)
		return;

	mutex_enter(&tracing_state_lock);

	if (tracing_state == VTR_STATE_PAUSE) {
		if (trace_process_mode == VTR_PROCESS_NULL) {
			for (i = 0; i < NCPU; i++) {
				if (!IS_VALID_CPU(i))
					continue;
				cpu[i]->cpu_trace.event_map =
					cpu[i]->cpu_trace.real_event_map;
			}
			tracing_state = VTR_STATE_ACTIVE;
		} else {
			tracing_state = VTR_STATE_PERPROC;
		}
		new_labels = 0;
	}

	mutex_exit(&tracing_state_lock);
}

/*
 * Disable tracing: point each CPU's event_map at the global null_event_map.
 */
void
trace_pause(void)
{
	int i;

	/*
	 * Don't even try to grab tracing_state_lock if tracing is off.
	 * This prevents callers from trying to enter an uninitialized
	 * mutex during boot.
	 */

	if (!(tracing_state & (VTR_STATE_ACTIVE | VTR_STATE_PERPROC)))
		return;

	mutex_enter(&tracing_state_lock);

	if (tracing_state & (VTR_STATE_ACTIVE | VTR_STATE_PERPROC)) {
		for (i = 0; i < NCPU; i++) {
			if (!IS_VALID_CPU(i))
				continue;
			cpu[i]->cpu_trace.event_map = null_event_map;
		}
		tracing_state = VTR_STATE_PAUSE;
	}

	mutex_exit(&tracing_state_lock);
}

/*
 * Halt all tracing, even administrative (facility 0).
 */
void
trace_halt(void)
{
	int i;

	/*
	 * Don't even try to grab tracing_state_lock if tracing is off.
	 * This prevents callers from trying to enter an uninitialized
	 * mutex during boot.
	 */

	if (!(tracing_state & ADMIN_TRACING))
		return;

	mutex_enter(&tracing_state_lock);

	if (tracing_state & ADMIN_TRACING) {
		for (i = 0; i < NCPU; i++) {
			if (!IS_VALID_CPU(i))
				continue;
			cpu[i]->cpu_trace.event_map = null_event_map;
		}
		tracing_state = VTR_STATE_HALTED;
	}

	mutex_exit(&tracing_state_lock);
}

/*
 * Reset trace buffer: sets tbuf_head = tbuf_tail.
 * Used when the buffer contents are deemed uninteresting.
 */
void
trace_reset_pointers(void)
{
	int i, old_state;

	if (!(tracing_state & ADMIN_TRACING))
		return;

	mutex_enter(&tracing_state_lock);

	if (tracing_state & ADMIN_TRACING) {
		old_state = tracing_state;
		tracing_state = VTR_STATE_HALTED;
		DELAY(5);
		if (new_labels)
			trace_flush(1);
		for (i = 0; i < NCPU; i++) {
			if (!IS_VALID_CPU(i))
				continue;
			cpu[i]->cpu_trace.tbuf_head =
			    cpu[i]->cpu_trace.tbuf_tail;
		}
		tracing_state = old_state;
	}

	mutex_exit(&tracing_state_lock);
}

/*
 * Reset tracing, free all tracing resources
 */
void
trace_reset(int force)
{
	int i;
	tracedata_t *tdp;
	char *bp, *emp;

	if ((!force) && (curproc->p_pid != tracing_pid))
		return;

	mutex_enter(&trace_reset_lock);

	trace_halt();

	if (force) {
		trace_forced_reset = 1;
		cmn_err(CE_NOTE, "vtrace: forced reset by process %d\n",
			curproc->p_pid);
	}

	(void) trace_flush(VTR_FORCE);

	for (i = 0; i < NCPU; i++) {
		if (!IS_VALID_CPU(i))
			continue;
		tdp = &cpu[i]->cpu_trace;
		tdp->event_map = null_event_map;
		tdp->last_thread = NULL;
		DELAY(1000);
		if ((bp = tdp->tbuf_start) != NULL)
			kmem_free(bp, ptob(trace_npages));
		tdp->tbuf_start = NULL;
		if ((emp = (char *)tdp->real_event_map) != NULL)
			kmem_free(emp, VT_MAPSIZE);
		tdp->real_event_map = NULL;
		if (tdp->trace_file)
			(void) closef(tdp->trace_file);
		tdp->trace_file = NULL;
		tdp->tbuf_overflow = NULL;
		overflow_err[i] = 0;
	}

	mutex_enter(&tracing_state_lock);
	tracing_pid = 0;
	tracing_cred = NULL;
	tracing_state = VTR_STATE_NULL;
	trace_process_mode = VTR_PROCESS_NULL;
	mutex_exit(&tracing_state_lock);

	mutex_exit(&trace_reset_lock);
}

int
trace_set_process_list(int from_userland, pid_t *plist, int pcount)
{
	int mode, error = 0;

	if (curproc->p_pid != tracing_pid)
		return (EACCES);
	if (!(tracing_state & (VTR_STATE_READY | VTR_STATE_PAUSE)))
		return (EBUSY);

	if (pcount == 0) {
		trace_process_mode = VTR_PROCESS_NULL;
		trace_nprocs = 0;
		return (0);
	}

	if (pcount > 0) {
		mode = VTR_PROCESS_TRACE;
	} else {
		mode = VTR_PROCESS_NOTRACE;
		pcount = -pcount;
	}

	if (pcount > MAX_TRACE_NPROCS)
		return (EINVAL);

	if (from_userland) {
		if (copyin((char *)plist, (char *)trace_process_list,
		    pcount * sizeof (pid_t)) != 0)
			error = EFAULT;
	} else {
		VT_BCOPY(plist, trace_process_list, pcount * sizeof (pid_t));
	}

	if (error) {
		trace_process_mode = VTR_PROCESS_NULL;
		trace_nprocs = 0;
	} else {
		trace_process_mode = mode;
		trace_nprocs = pcount;
	}

	return (error);
}

/*
 * Called by resume() to turn tracing on and off on a per-process basis.
 */
void
trace_check_process(void)
{
	int i;
	pid_t mypid;
	tracedata_t *tdp;
	uchar_t *emap;

	mypid = curproc->p_pid;
	tdp = &CPU->cpu_trace;

	mutex_enter(&tracing_state_lock);

	if (tracing_state != VTR_STATE_PERPROC) {
		mutex_exit(&tracing_state_lock);
		return;
	}

	switch (trace_process_mode) {

	    case VTR_PROCESS_TRACE:

		emap = null_event_map;
		for (i = 0; i < trace_nprocs; i++) {
			if (trace_process_list[i] == mypid) {
				emap = tdp->real_event_map;
				break;
			}
		}
		break;

	    case VTR_PROCESS_NOTRACE:

		emap = tdp->real_event_map;
		for (i = 0; i < trace_nprocs; i++) {
			if (trace_process_list[i] == mypid) {
				emap = null_event_map;
				break;
			}
		}
		break;

	    default:

		emap = tdp->event_map;
		break;
	}
	tdp->event_map = emap;
	mutex_exit(&tracing_state_lock);
}

/*
 * Write len bytes of trace data, starting at addr, to vp.
 */
static int
trace_vn_write(struct vnode *vp, struct cred *cr, char *addr, int len)
{
	int error;
	ssize_t resid;
	rlim64_t limit;

	if (len == 0)
		return (0);

	/*
	 * Large Files: We limit the size of the trace file not
	 * not to exceed 2GB by the following check. Trace utilities
	 * are not converted to be large file aware.
	 */
	limit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	if (limit > (rlim64_t)MAXOFF32_T)
		limit = (rlim64_t)MAXOFF32_T;
	TRACE_3(TR_FAC_TEST, TR_TRACE_VN_WRITE_START,
		"trace_vn_write_start:vp %p addr %p len %d", vp, addr, len);
	error = vn_rdwr(UIO_WRITE, vp, addr, len, 0LL, UIO_SYSSPACE,
		FAPPEND, (rlim64_t)limit, cr, &resid);
	if (resid)
		error = EFBIG;
	if (error)
		trace_halt();
	TRACE_0(TR_FAC_TEST, TR_TRACE_VN_WRITE_END, "trace_vn_write_end");
	return (error);
}

/*
 * Flush out all of the trace buffers.
 */
int
trace_flush(int force)
{
	int err = 0, i, size1, size2, size;
	struct vnode *vp;
	tracedata_t *tdp;
	char *start, *head, *tail, *end;
	cred_t *cr = tracing_cred;

	if (tracing_state & (VTR_STATE_NULL | VTR_STATE_READY))
		return (0);
	for (i = 0; i < NCPU; i++) {
		if (!IS_VALID_CPU(i))
			continue;
		tdp = &cpu[i]->cpu_trace;
		if (tdp->trace_file == NULL)
			continue;
		vp = tdp->trace_file->f_vnode;
		start = tdp->tbuf_start;
		end = tdp->tbuf_end;
		head = tdp->tbuf_head;
		tail = tdp->tbuf_tail;
		if (tdp->tbuf_overflow != NULL) {
			if (overflow_err[i] == 0) {
				cmn_err(CE_WARN,
				    "vtrace: CPU %d trace buffer overflow", i);
				overflow_err[i] = 1;
			}
			continue;
		}
		TRACE_3(TR_FAC_TEST, TR_TRACE_FLUSH_START,
			"trace_flush_start:cpu %d head %x tail %x",
			i, head, tail);
		if (tail <= head) {
			size = head - tail;
			if (force || size >= trace_lowater) {
				if (err = trace_vn_write(vp, cr, tail, size))
					return (err);
				tail = head;
			} else {
				size = 0;
			}
		} else {
			size1 = end - tail;
			size2 = head - start;
			size = size1 + size2;
			if (force || size >= trace_lowater) {
				if (err = trace_vn_write(vp, cr, tail, size1))
					return (err);
				if (err = trace_vn_write(vp, cr, start, size2))
					return (err);
				tail = head;
			} else {
				size = 0;
			}
		}
		if (tdp->tbuf_overflow != NULL) {
			if (overflow_err[i] == 0) {
				cmn_err(CE_WARN,
				    "vtrace: CPU %d trace buffer overflow", i);
				overflow_err[i] = 2;
			}
			continue;
		}
		tdp->tbuf_tail = tail;
		tdp->tbuf_redzone = (tail >= start + 0x1000) ?
			(char *)((uint_t)(tail - 0x1000) & ~0xfff) :
			(char *)((uint_t)(end - 0x1000) & ~0xfff);
		cpu_trace_info[i].bytes_flushed += size;
		if (size > cpu_trace_info[i].max_flushsize)
			cpu_trace_info[i].max_flushsize = size;
		TRACE_1(TR_FAC_TEST, TR_TRACE_FLUSH_END,
			"trace_flush_end:bytes_written %d", size);
	}
	if (err)
		return (err);
	for (i = 0; i < NCPU; i++)
		err = MAX(err, overflow_err[i]);
	if (err == 1)
		return (ENOMEM);	/* overflow, files OK */
	if (err == 2)
		return (EINVAL);	/* overflow, files corrupt */
	return (0);
}

/*
 * Write out some ID records at the beginning of a trace buffer:
 *
 *   - version
 *   - labels for all standard (facility 0) events
 *   - vtrace clock frequency (e.g. a freq of 1000000 means usec timestamps)
 *   - absolute time stamp
 *   - initial hrtime (needed to compute subsequent hrtime deltas)
 *   - system page size
 *   - number of CPUs
 *   - writing cpu number
 *   - trace file title
 *   - kthread id of the flushing process
 */
static void
trace_init_buffer(int cpunum)
{
	vt_pointer_t vtp;
	tracedata_t *tdp = &cpu[cpunum]->cpu_trace;
	char *title = "Untitled";

	tdp->last_hrtime_lo32 = (uint32_t)(start_hrtime & 0xffffffff);

	TBUF_OPEN(vtp.char_p, tdp);

	vtp.version_p->head = FTT2HEAD(TR_FAC_TRACE, TR_VERSION, 0);
	vtp.version_p->v_major = VT_VERSION_MAJOR;
	vtp.version_p->v_minor = VT_VERSION_MINOR;
	vtp.version_p->v_micro = VT_VERSION_MICRO;
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_version_t));
	store_string(tdp, VT_VERSION_NAME);

	trace_label(TR_FAC_TRACE, TR_END, sizeof (ulong_t),
		"end_of_trace_file", cpunum);
	trace_label(TR_FAC_TRACE, TR_VERSION, sizeof (vt_version_t),
		"binary_trace_format:Version %u.%u.%u (%s)", cpunum);
	trace_label(TR_FAC_TRACE, TR_TITLE, sizeof (vt_title_t),
		"Title:%s", cpunum);
	trace_label(TR_FAC_TRACE, TR_PAD, sizeof (ulong_t),
		"pad_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_LABEL, sizeof (vt_label_t),
		"event_label:fac %3d tag %3d len %2d info %2x name %s", cpunum);
	trace_label(TR_FAC_TRACE, TR_PAGESIZE, sizeof (vt_pagesize_t),
		"system_page_size:page size %u bytes", cpunum);
	trace_label(TR_FAC_TRACE, TR_NUM_CPUS, sizeof (vt_num_cpus_t),
		"number_of_CPUs:number of CPUs %u", cpunum);
	trace_label(TR_FAC_TRACE, TR_CPU, sizeof (vt_cpu_t),
		"writing_cpu_number:writing CPU %u", cpunum);
	trace_label(TR_FAC_TRACE, TR_DATA_4, sizeof (vt_data_4_t),
		"4-word_data_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_DATA_8, sizeof (vt_data_8_t),
		"8-word_data_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_DATA_16, sizeof (vt_data_16_t),
		"16-word_data_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_DATA_32, sizeof (vt_data_32_t),
		"32-word_data_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_DATA_64, sizeof (vt_data_64_t),
		"64-word_data_record", cpunum);
	trace_label(TR_FAC_TRACE, TR_ABS_TIME, sizeof (vt_abs_time_t),
		"absolute_time:time since epoch (%u << 32) + %u microseconds",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_START_TIME,
		sizeof (vt_start_time_t),
		"start_time:start time (%u << 32) + %u microseconds", cpunum);
	trace_label(TR_FAC_TRACE, TR_ELAPSED_TIME,
		sizeof (vt_elapsed_time_t),
		"32_bit_elapsed_time:time delta %u microseconds", cpunum);
	trace_label(TR_FAC_TRACE, TR_TOTAL_TIME,
		sizeof (vt_total_time_t),
		"total_elapsed_time:total time (%u << 32) + %u microseconds",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_KTHREAD_ID,
		sizeof (vt_kthread_id_t),
		"kernel_thread_id:pid %u lwpid %u tid %x vid %u = %s",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_UTHREAD_ID,
		sizeof (vt_uthread_id_t),
		"user_thread_id:pid %u lwpid %u tid %x vid %u = %s",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_CLOCK_FREQUENCY,
		sizeof (vt_clock_frequency_t),
		"vtrace_clock_frequency:%u Hz", cpunum);
	trace_label(TR_FAC_TRACE, TR_RAW_KTHREAD_ID,
		sizeof (vt_raw_kthread_id_t),
		"raw_kernel_thread_id:tid %x", cpunum);
	trace_label(TR_FAC_TRACE, TR_RAW_UTHREAD_ID,
		sizeof (vt_raw_uthread_id_t),
		"raw_user_thread_id:pid %u lwpid %u tid %x", cpunum);
	trace_label(TR_FAC_TRACE, TR_KTHREAD_LABEL,
		sizeof (vt_kthread_label_t),
		"kernel_thread_label:pid %u lwpid %u tid %x startpc %K",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_UTHREAD_LABEL,
		sizeof (vt_uthread_label_t),
		"user_thread_label:pid %u lwpid %u tid %x startpc %x",
		cpunum);
	trace_label(TR_FAC_TRACE, TR_PROCESS_NAME,
		sizeof (vt_process_name_t),
		"process_name:pid %u = %s", cpunum);
	trace_label(TR_FAC_TRACE, TR_PROCESS_FORK,
		sizeof (vt_process_fork_t),
		"process_fork:cpid %u ppid %u", cpunum);

	/*
	 * There are a few trace points at places where traps are
	 * disabled.  It would be too intrusive to label them there,
	 * so we do it here.
	 */

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_TRAP, TR_TRAP_START, VT_ENABLED)) {
		trace_label(TR_FAC_TRAP, TR_TRAP_START, 8,
			"trap_start:type %x", cpunum);
	}

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_TRAP, TR_TRAP_END, VT_ENABLED)) {
		trace_label(TR_FAC_TRAP, TR_TRAP_END, 4,
			"trap_end", cpunum);
	}

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_SYSCALL, TR_SYSCALL_START, VT_ENABLED)) {
		trace_label(TR_FAC_SYSCALL, TR_SYSCALL_START, 8,
			"syscall_start:syscall %x", cpunum);
	}

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_SYSCALL, TR_SYSCALL_END, VT_ENABLED)) {
		trace_label(TR_FAC_SYSCALL, TR_SYSCALL_END, 4,
			"syscall_end", cpunum);
	}

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_TRAP, TR_KERNEL_WINDOW_OVERFLOW, VT_ENABLED)) {
		trace_label(TR_FAC_TRAP, TR_KERNEL_WINDOW_OVERFLOW, 8,
			"kernel_window_overflow:save at %K", cpunum);
	}

	if (VT_TEST_FT(tdp->real_event_map,
	    TR_FAC_TRAP, TR_KERNEL_WINDOW_UNDERFLOW, VT_ENABLED)) {
		trace_label(TR_FAC_TRAP, TR_KERNEL_WINDOW_UNDERFLOW, 8,
			"kernel_window_underflow:restore at %K", cpunum);
	}

	TBUF_OPEN(vtp.char_p, tdp);

	vtp.clock_frequency_p->head = FTT2HEAD(TR_FAC_TRACE,
		TR_CLOCK_FREQUENCY, 0);
	vtp.clock_frequency_p->freq = get_vtrace_frequency();
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_clock_frequency_t));

	vtp.abs_time_p->head = FTT2HEAD(TR_FAC_TRACE, TR_ABS_TIME, 0);
	vtp.abs_time_p->time.hi32 = (ulong_t)(abs_hrtime >> 32);
	vtp.abs_time_p->time.lo32 = (ulong_t)(abs_hrtime & 0xffffffff);
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_abs_time_t));

	vtp.start_time_p->head = FTT2HEAD(TR_FAC_TRACE, TR_START_TIME, 0);
	vtp.start_time_p->time.hi32 = (ulong_t)(start_hrtime >> 32);
	vtp.start_time_p->time.lo32 = (ulong_t)(start_hrtime & 0xffffffff);
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_start_time_t));

	vtp.pagesize_p->head = FTT2HEAD(TR_FAC_TRACE, TR_PAGESIZE, 0);
	vtp.pagesize_p->pagesize = PAGESIZE;
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_pagesize_t));

	vtp.num_cpus_p->head = FTT2HEAD(TR_FAC_TRACE, TR_NUM_CPUS, 0);
	vtp.num_cpus_p->num_cpus = NCPU;
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_num_cpus_t));

	vtp.cpu_p->head = FTT2HEAD(TR_FAC_TRACE, TR_CPU, 0);
	vtp.cpu_p->cpu_num = cpunum;
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_cpu_t));

	vtp.title_p->head = FTT2HEAD(TR_FAC_TRACE, TR_TITLE, 0);
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_title_t));
	store_string(tdp, title);
	TBUF_OPEN(vtp.char_p, tdp);

	vtp.raw_kthread_id_p->head =
		FTT2HEAD(TR_FAC_TRACE, TR_RAW_KTHREAD_ID, 0);
	vtp.raw_kthread_id_p->tid  = (ulong_t)curthread;
	TBUF_WRITE(vtp.char_p, tdp, sizeof (vt_raw_kthread_id_t));
}

static void
trace_start(void)
{
	int i;
	kthread_id_t t;

	start_hrtime = get_vtrace_time();

	abs_hrtime = ((hrtime_t)hrestime.tv_sec * 1000000) +
		(hrestime.tv_nsec) / 1000;

	for (i = 0; i < NCPU; i++) {
		if (!IS_VALID_CPU(i))
			continue;
		trace_init_buffer(i);
	}

	/*
	 * write out thread labels for all threads to this CPU's buffer
	 */

	i = CPU->cpu_id;
	mutex_enter(&pidlock);
	t = curthread;
	do {
		trace_kthread_label(t, i);
	} while ((t = t->t_next) != curthread);
	trace_kthread_label(&t0, i);
	tracing_state = VTR_STATE_PAUSE;
	mutex_exit(&pidlock);

	/*
	 * We don't allow the tracing process to be swapped, because we
	 * might overflow the trace buffers while waiting to be swapped
	 * back in.  We should revisit this now that we have real time
	 * scheduling.
	 */

	mutex_enter(&curproc->p_lock);
	curproc->p_flag |= SLOCK;
	mutex_exit(&curproc->p_lock);
}

static void
update_global_trace_info(void)
{
	hrtime_t cur_hrtime;

	cur_hrtime = get_vtrace_time();
	global_trace_info.elapsed_time	= cur_hrtime - start_hrtime;
	global_trace_info.v_major	= VT_VERSION_MAJOR;
	global_trace_info.v_minor	= VT_VERSION_MINOR;
	global_trace_info.v_micro	= VT_VERSION_MICRO;
	global_trace_info.tracing_state	= tracing_state;
	global_trace_info.tracing_pid	= tracing_pid;
	global_trace_info.ncpus		= NCPU;
	global_trace_info.tracedata_ptrs = (void *) tracedata_ptrs;
}

static void
update_cpu_trace_info(void)
{
	int i;
	tracedata_t *tdp;
	vt_cpu_info_t *cpi;

	for (i = 0; i < NCPU; i++) {
		cpi = &cpu_trace_info[i];
		cpi->cpu_online = IS_VALID_CPU(i);
		if (!cpi->cpu_online)
			continue;
		tdp = &cpu[i]->cpu_trace;
		cpi->tbuf_size		= trace_nbytes;
		cpi->tbuf_lowater	= trace_lowater;
		cpi->tbuf_headp		= &tdp->tbuf_head;
		cpi->tbuf_start		= tdp->tbuf_start;
		cpi->tbuf_end		= tdp->tbuf_end;
		cpi->tbuf_wrap		= tdp->tbuf_wrap;
		cpi->tbuf_head		= tdp->tbuf_head;
		cpi->tbuf_tail		= tdp->tbuf_tail;
		cpi->tbuf_redzone	= tdp->tbuf_redzone;
		cpi->tbuf_overflow	= tdp->tbuf_overflow;
		cpi->real_event_map	= tdp->real_event_map;
		cpi->event_map		= tdp->event_map;
		cpi->trace_file		= tdp->trace_file;
	}
}

/*
 * Write out a label record for the (fac, tag) event.
 * Called when an event is seen for the first time on CPU cpunum.
 * (A value of cpunum == -1 implies the calling CPU's id, which is the
 * common case; actual CPU numbers are used only by trace_init_buffer().)
 * trace_label() writes the label to cpunum's trace buffer, sets the used bit
 * for the event, and sets the string bits based on the contents of the
 * format string.
 */
uchar_t
trace_label(uchar_t fac, uchar_t tag, ushort_t len, char *name, int cpunum)
{
	uchar_t string_bits = 0;
	uchar_t mask = VT_STRING_1;
	char oldc, newc, *cp;
	int colon, namelen, size, bytes;
	struct {
		vt_label_t	l;
		vt_generic_t	g;
	} data;

	/*
	 * Determine the string bits by looking for %s (or variants,
	 * like %-12.8s) in the format string.
	 */

	colon = 0;
	for (cp = name; (newc = *cp) != '\0'; cp++)
		if (newc == ':')
			break;
	if (newc) {
		/*
		 * A format string is present, and cp points to it
		 */
		colon = (int)cp - (int)name;
		oldc = NULL;
		while ((newc = *cp) != '\0') {
			cp++;
			if (oldc != '%') {
				oldc = newc;
				continue;
			}
			if (newc == '%') {
				oldc = NULL;
				continue;
			}
			if (((newc >= 'A') && (newc <= 'Z')) ||
			    ((newc >= 'a') && (newc <= 'z'))) {
				if (newc == 's')
					string_bits |= mask;
				mask <<= 1;
			} else
				continue;
			oldc = newc;
		}
	}
	namelen = (int)cp - (int)name;

	data.l.head = FTT2HEAD(TR_FAC_TRACE, TR_LABEL, 0);
	data.l.facility = fac;
	data.l.tag = tag;
	data.l.length = len >> 2; /* convert size to words */
	data.l.info = string_bits;

	bytes = colon ? namelen + 1 : namelen + 2;
	if (bytes > VT_MAX_BYTES - 4) {
		bytes = VT_MAX_BYTES - 5;	/* force NULL */
		if (colon >= bytes)
			colon = 0;
	}

	size = bytes2data[bytes + 256];
	VT_BZERO(&data.g, size);
	data.g.head = bytes2data[bytes];
	VT_BCOPY(name, &data.g.data, namelen);
	if (colon)
		*(((char *)data.g.data) + colon) = NULL;

	size += sizeof (vt_label_t);

	trace_write_data((char *)&data, size, cpunum);

	if (cpunum >= 0) {
		VT_SET_FT(cpu[cpunum]->cpu_trace.real_event_map,
			(ulong_t)fac, (ulong_t)tag,
			string_bits | VT_USED | VT_ENABLED);
		new_labels = 1;
	} else {
		mutex_enter(&tracing_state_lock);
		if (CPU->cpu_trace.real_event_map != null_event_map) {
			VT_SET_FT(CPU->cpu_trace.real_event_map,
				(ulong_t)fac, (ulong_t)tag,
				string_bits | VT_USED | VT_ENABLED);
		}
		new_labels = 1;
		mutex_exit(&tracing_state_lock);
	}

	return (string_bits | VT_USED | VT_ENABLED);
}

/*
 * Vtrace system call interface.
 */
vtrace(ulong_t *uap, rval_t *rvp)
{
	char *emp, *bp;
	uchar_t *emap;
	size_t size;
	int count, fd, event, i, info_type, force;
	struct file **file;
	int error = 0;

	switch (*uap++) {

	case VTR_INIT:		/* vtrace(VTR_INIT, npages, lowater) */

		error = trace_init(uap[0], uap[1]);
		break;

	case VTR_FILE:		/* vtrace(VTR_FILE, cpu, fd) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		if (tracing_state != VTR_STATE_READY)
			return (EACCES);
		i = *uap++;
		fd = *uap;
		if (IS_VALID_CPU(i)) {
			file = &cpu[i]->cpu_trace.trace_file;
			if (((*file) = getf(fd)) == NULL ||
			    (*file)->f_vnode == NULL) {
				if ((*file) != NULL)
					releasef(fd);
				cpu[i]->cpu_trace.trace_file = NULL;
				return (EBADF);
			}
			mutex_enter(&(*file)->f_tlock);
			(*file)->f_count++;
			mutex_exit(&(*file)->f_tlock);
			releasef(fd);
			rvp->r_val1 = 0;
		} else {
			rvp->r_val1 = 1;
		}
		break;

	case VTR_EVENTMAP:	/* vtrace(VTR_EVENTMAP, bitmap, size) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		if (tracing_state == VTR_STATE_NULL)
			return (EACCES);
		emp = (char *)(*uap++);
		size = *uap;
		if (size > VT_MAPSIZE)
			return (EINVAL);
		emap = kmem_zalloc(VT_MAPSIZE, KM_SLEEP);
		if (copyin(emp, (char *)emap, size) != 0) {
			error = EFAULT;
			kmem_free(emap, VT_MAPSIZE);
			break;
		}
		/*
		 * Enable all events in the administrative facility
		 */
		VT_FAC_ENABLE(emap, TR_FAC_TRACE);
		/*
		 * Copy event map to each CPU
		 */
		for (i = 0; i < NCPU; i++) {
			if (!IS_VALID_CPU(i))
				continue;
			VT_BCOPY(emap, cpu[i]->cpu_trace.real_event_map,
				VT_MAPSIZE);
		}
		kmem_free(emap, VT_MAPSIZE);
		break;

	case VTR_EVENT:		/* vtrace(VTR_EVENT, event) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		if (tracing_state == VTR_STATE_NULL)
			return (EACCES);
		event = *uap;
		if (event >> 16)
			return (EINVAL);
		/*
		 * set event for each CPU
		 */
		for (i = 0; i < NCPU; i++) {
			if (!IS_VALID_CPU(i))
				continue;
			VT_SET(cpu[i]->cpu_trace.real_event_map,
				event, VT_ENABLED);
		}
		break;

	case VTR_START:		/* vtrace(VTR_START) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		if (tracing_state == VTR_STATE_NULL)
			return (EACCES);
		if (tracing_state != VTR_STATE_READY)
			return (EBUSY);

		trace_start();
		break;

	case VTR_PAUSE:		/* vtrace(VTR_PAUSE) */

		trace_pause();
		break;

	case VTR_RESUME:	/* vtrace(VTR_RESUME) */

		trace_resume();
		break;

	case VTR_FLUSH:		/* vtrace(VTR_FLUSH, force) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		force = *uap;
		if (tracing_state & PAUSE_OR_ACTIVE)
			error = trace_flush(force);
		break;

	case VTR_INFO:		/* vtrace(VTR_INFO, info_type, buf_ptr) */

		info_type = *uap++;
		bp = (char *)*uap;

		switch (info_type) {

		    case VTR_INFO_GLOBAL:

			update_global_trace_info();
			if (copyout((char *)&global_trace_info, bp,
			    sizeof (vt_global_info_t)) != 0)
				error = EFAULT;
			break;

		    case VTR_INFO_PERCPU:

			update_cpu_trace_info();
			if (copyout((char *)cpu_trace_info, bp,
			    NCPU * sizeof (vt_cpu_info_t)) != 0)
				error = EFAULT;
			break;

		    default:

			error = EINVAL;
		}
		break;

	case VTR_RESET:		/* vtrace(VTR_RESET, force) */

		force = *uap;
		trace_halt();
		error = trace_flush(VTR_FORCE);
		trace_reset(force);
		break;

	case VTR_TEST:		/* vtrace(VTR_TEST, count) */

		if (curproc->p_pid != tracing_pid)
			return (EACCES);
		if (tracing_state != VTR_STATE_ACTIVE)
			return (EACCES);
		count = *uap;
		for (i = 0; i < (count >> 2); i++) {
			TRACE_0(TR_FAC_TEST, TR_SPEED_0, "speed_test_0");
			TRACE_0(TR_FAC_TEST, TR_SPEED_0, "speed_test_0");
			TRACE_0(TR_FAC_TEST, TR_SPEED_0, "speed_test_0");
			TRACE_0(TR_FAC_TEST, TR_SPEED_0, "speed_test_0");
		}
		for (i = 0; i < count; i++) {
			TRACE_1(TR_FAC_TEST, TR_SPEED_1,
				"speed_test_1:%d", i);
		}
		for (i = 0; i < count; i++) {
			TRACE_1(TR_FAC_TEST, TR_SPEED_1_STRING,
				"speed_test_1_string:%s", "test string");
		}
		for (i = 0; i < count; i++) {
			TRACE_2(TR_FAC_TEST, TR_SPEED_2,
				"speed_test_2:%d %d", i, i);
		}
		for (i = 0; i < count; i++) {
			TRACE_2(TR_FAC_TEST, TR_SPEED_2_STRING,
				"speed_test_2_string:%s %d",
				"test string", i);
		}
		for (i = 0; i < count; i++) {
			TRACE_3(TR_FAC_TEST, TR_SPEED_3,
				"speed_test_3:%d %d %d", i, i, i);
		}
		for (i = 0; i < count; i++) {
			TRACE_3(TR_FAC_TEST, TR_SPEED_3_STRING,
				"speed_test_3_string:%d %s %d",
				i, "test string", i);
		}
		for (i = 0; i < count; i++) {
			TRACE_4(TR_FAC_TEST, TR_SPEED_4,
				"speed_test_4:%d %d %d %d", i, i, i, i);
		}
		for (i = 0; i < count; i++) {
			TRACE_4(TR_FAC_TEST, TR_SPEED_4_STRING,
				"speed_test_4_string:%d %d %d %s",
				i, i, i, "test string");
		}
		for (i = 0; i < count; i++) {
			TRACE_5(TR_FAC_TEST, TR_SPEED_5,
				"speed_test_5:%d %d %d %d %d", i, i, i, i, i);
		}
		for (i = 0; i < count; i++) {
			TRACE_5(TR_FAC_TEST, TR_SPEED_5_STRING,
				"speed_test_5_string:%d %s %d %s %d",
				i, "string1", i, "string2", i);
		}
		break;

	case VTR_PROCESS:	/* vtrace(VTR_PROCESS, pidlist, count) */

		error = trace_set_process_list(1, (pid_t *)uap[0], uap[1]);
		break;

	case VTR_GET_STRING:	/* vtrace(VTR_GET_STRING, kaddr, uaddr) */

		error = copyoutstr((char *)uap[0], (char *)uap[1],
			VT_MAX_BYTES - 5, &size);
		break;

	default:
		error = EINVAL;
	}
	return (error);
}

#else	/* TRACE */

int	tracing_exists = 0;

#endif	/* TRACE */
