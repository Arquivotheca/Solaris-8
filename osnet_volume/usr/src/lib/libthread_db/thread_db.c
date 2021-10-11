/*
 * Copyright (c) 1990-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thread_db.c	1.15	99/07/27 SMI"

/*
 * Threads library interface
 */

#include <stdio.h>
#include <stdarg.h>
#include <sys/signal.h>
#include "sys/param.h"
#include "dbx_export.h"
#include "event_export.h"
#include "thread.h"
#include "libthread.h"
#include "thread_db.h"
#ifdef i386
#include <sys/sysi86.h>
#include <sys/segment.h>
#endif

char thread_db_err[256];

/*
 * Group of macros used to disect a uthread_t.
 */
#define	thread_state(x)		(dreadb((Address)&((x)->t_state)))
#define	thread_wchan(x)		(dreadl((Address)&((x)->t_wchan)))
#define	thread_lwpid(x)		(dreadl((Address)&((x)->t_lwpid)))
#define	thread_usropts(x)	(dreads((Address)&((x)->t_usropts)))
#define	thread_tid(x)		((thread_t)(x))
#define	thread_pc(x)		(dreadl((Address)&((x)->t_pc)))
#define	thread_next(x)		(dreadl((Address)&((x)->t_next)))
#define	thread_startpc(x)	(dreadl((Address)&((x)->t_startpc)))
#define	thread_id(x)		(dreadl((Address)&((x)->t_tid)))
#define	thread_flag(x)		(dreads((Address)&((x)->t_flag)))
#define	thread_stkbottom(x)	(dreadl((Address)&((x)->t_stk)))
#define	thread_stksize(x)	(dreadl((Address)&((x)->t_stksize)))

/*
 * Macro used to determine if a thread is bound to a LWP
 * or not.
 */
#define	IS_BOUND(x)	(thread_usropts(x) & THR_BOUND)

/*
 * Macro used to determine if thread is ONPROC or not.
 */
#define	IS_ONPROC(x)	((thread_state(x) == TS_ONPROC) || \
			    (thread_flag(x) & T_PARKED))

static int
thread_list(uthread_t **tl)
{
	Address allthreads_addr;
	Address totalthreads_addr;

	if (!(allthreads_addr = quick_lookup("_allthreads")))
		return (NULL);
	if (!(totalthreads_addr = quick_lookup("_totalthreads")))
		return (NULL);
	*tl = (uthread_t *)dreadl(allthreads_addr);
	return (dreadl(totalthreads_addr));
}

/*
 * This routine is called when a thr_exit() event occurs.
 */

static void
cb_thread_death(Proc proc, Handler h, Event e, void *client_data)
{
}

/*
 * This routine is called when a thr_create() event occurs.
 */
static
void
cb_thread_create(Proc proc, Handler h, Event e, void *client_data)
{
	thread_t tid;

	thread_db_from_vcpu(e->vcpu, &tid);
}

/*
 * setup a handler to catch thread_create() events.
 */
static thread_db_err_e
notify_on_create()
{
	Handler h;
	Action a;
	unsigned addr;

	if (!(addr = quick_lookup("_thread_start")))
		return (TDB_ERR);

	h = Handler_new_bpt(Event_BPT, Handler_INTERNAL, addr, false);
	a = Action_new_callback(Action_CALLBACK, cb_thread_create, NULL);
	Handler_add_action(h, a);
	Handler_set_origin_str(h, "# thread_db: thread created 1");
	Handler_toggle(h, true);
	return (TDB_OK);
}

static void
terror(char *fmt, ...)
{
	va_list vp;

	/*
	 * Stuff a formatted error message into 'thread_db_err'
	 */

	va_start(vp, fmt);
	vsprintf(thread_db_err, xlat_text(fmt), vp);
	va_end(vp);
#ifdef DEBUG
	if (terror_fd == 0) {
		terror_fd = open("thread_db", O_RDWR | O_CREAT, 0666);
	}
	write(terror_fd, thread_db_err, strlen(thread_db_err));
#endif
}

int
thread_db_vers()
{
	return (THREAD_DB_VERS);
}

/*
 * Update debugger's list of debuggee's threads.
 */
thread_db_err_e
thread_db_sync()
{
	return (TDB_OK);
}

thread_db_err_e
thread_db_init()
{
	if ((Address)quick_lookup("thr_create"))
		return (TDB_OK);
	return (TDB_ERR);
}

int
thread_db_iter(thread_iter_f *cb, void *client_data)
{
	thrtab_t *allthreads_addr;
	uthread_t *t, *first;
	int totalthreads;
	int thx;	/* thread hash index */
	thrtab_t thrtab[ALLTHR_TBLSIZ];	/* local copy */

	if (!(allthreads_addr = (thrtab_t *) quick_lookup("_allthreads")))
		return (0);

	/*
	 * get a copy of the hash table. going through it
	 * one element at a time is too slow
	 */
	dread((char *)thrtab, (Address)allthreads_addr, sizeof (thrtab));

	totalthreads = 0;
	for (thx = 0; thx < ALLTHR_TBLSIZ; thx++) {
		first = thrtab[thx].first;
		if (first == NULL)
			continue;
		t = first;
		do {
			if (cb)
				(*cb)(thread_tid(t), client_data);
			totalthreads++;
		} while ((t = (uthread_t *)thread_next(t)) != first);
	}

	return (totalthreads);
}

/*
 * Validate the given thread id
 */
int
thread_db_valid(thread_t tid)
{
	uthread_t *t;
	int totalthreads;
	int i;

/*
	totalthreads = thread_list(&t);
	for (i = 0; i < totalthreads; i++) {
		if ((thread_t)t == tid)
			return (1);
		t = (uthread_t *)thread_next(t);
	}
*/
	return (1);
}

/*
 * convert the debugger's idea of a thread ID to the thread's real
 * ID as allocated by thr_create().
 */
thread_db_err_e
thread_db_id(thread_t dbid, thread_t *id)
{
	uthread_t *t = (uthread_t *)dbid;

	*id = thread_id(t);
	return (TDB_OK);
}

thread_db_err_e
thread_db_lwpid(thread_t tid, int *lidp)
{
	uthread_t *t = (uthread_t *)tid;

	ASSERT(IS_BOUND(t) || IS_ONPROC(t));
	*lidp = thread_lwpid(t);
	return (TDB_OK);
}

thread_db_err_e
thread_db_pc(thread_t tid, Proc proc, unsigned *pc)
{
	struct RegGSet_t rset;

	if (thread_db_get_regs(tid, &rset, proc) == TDB_OK) {
#if defined(sparc)
		*pc = rset.pc;
#elif defined(i386)
		*pc = rset.regs[EIP];
#else
#error Unknown architecture!
#endif
		return (TDB_OK);
	}
	return (TDB_ERR);
}

thread_db_err_e
thread_db_startpc(thread_t tid, Proc proc, unsigned *pc)
{
	uthread_t *t = (uthread_t *)tid;

	*pc = thread_startpc(t);
	return (TDB_OK);
}

thread_db_err_e
thread_db_status(thread_t tid, thread_status_t *tstatp)
{
	uthread_t *t = (uthread_t *)tid;

	tstatp->sleep_addr = NULL;
	tstatp->lwpid = NULL;
	switch (thread_state(t)) {
		case TS_SLEEP:
			tstatp->state = thread_SLEEP;
			tstatp->sleep_addr = thread_wchan(t);
			break;
		case TS_RUN:
			tstatp->state = thread_RUN;
			break;
		case TS_ONPROC:
			tstatp->state = thread_ONPROC;
			tstatp->lwpid = thread_lwpid(t);
			break;
		case TS_ZOMB:
			tstatp->state = thread_ZOMB;
			break;
		case TS_STOPPED:
			tstatp->state = thread_STOPPED;
			break;
		default:
			tstatp->state = thread_UNKNOWN;
	}
	tstatp->flags = thread_usropts(t);
	return (TDB_OK);
}

/*
 * Convert a vcpu to a tid (thread_t).
 */
thread_db_err_e
thread_db_from_vcpu(VCpu vcpu, thread_t *tidp)
{
#if defined(i386)
	struct RegGSet_t *gregp;
	struct ssd *ldt, *ldt1;
	int n, rval;

	rval = VCpu_get_nldt(vcpu, &n);

	/*
	 * This condition can occur when dbx first starts a process and
	 * the setup code has not yet run.
	 */
	if (n == 0) {
		*tidp = 0;
		return (TDB_OK);
	}
	if ((ldt1 = (struct ssd *)calloc(n + 1, sizeof (struct ssd))) == NULL) {
		terror("Failed to alloc space for LDT table\n");
		return (TDB_ERR);
	}
	VCpu_get_ldt(vcpu, ldt1);
	gregp = VCpu_get_regs(vcpu);
	for (ldt = ldt1; ldt->acc1; ldt++) {
		if (seltoi(ldt->sel) == seltoi(gregp->regs[GS])) {
			*tidp = (thread_t)dreadl(ldt->bo);
			free(ldt1);
			return (TDB_OK);
		}
	}
	free(ldt1);
	terror("Invalid %%gs reg %d\n", seltoi(gregp->regs[GS]));
	return (TDB_ERR);
#elif defined(sparc)
	struct RegGSet_t *gregp;
	gregp = VCpu_get_regs(vcpu);

	if (gregp->g[7] == 0) {
		*tidp = 0;
	} else if (gregp->g[7] < 0x2000) {
		terror ("small %%g7 (%#x)", gregp->g[7]);
		return (TDB_ERR);
	} else {
		*tidp = (thread_t)(gregp->g[7]);
	}
	return (TDB_OK);
#else
#error Unknown architecture!
#endif
}


thread_db_err_e
thread_db_get_regs(thread_t tid, struct RegGSet_t *rp, Proc proc)
{
	uthread_t *t = (uthread_t *)tid;
	struct rwindow *rwin;

	if (IS_ONPROC(t) || IS_BOUND(t)) {
		VCpu vcpu;
		vcpu = VCpu_by_id(proc, thread_lwpid(t), 0);
		if (!vcpu) {
			terror ("cannot locate l@%d.", tid);
			return (TDB_ERR);
		}
		*rp = * VCpu_get_regs(vcpu);
	} else {
#if defined(i386)
		rp->regs[GS] = 0x10000;
		rp->regs[EIP] = thread_pc(t);
		rp->regs[ESP] = dreadl((Address)&(t->t_sp));
		rp->regs[EDI] = dreadl((Address)&(t->t_edi));
		rp->regs[ESI] = dreadl((Address)&(t->t_esi));
		rp->regs[EBX] = dreadl((Address)&(t->t_ebx));
#elif defined(sparc)
		rp->g[7] = (int)t;
		rp->pc = thread_pc(t);
		if (rp->o[6] = dreadl((Address) &(t->t_sp))) {
			rwin = (struct rwindow *)rp->o[6];
			rp->i[6] = dreadl((Address) &rwin->rw_fp);
		}
#else
#error Unknown architecture!
#endif /* sparc */
	}
	return (TDB_OK);
}

thread_db_err_e
thread_db_get_fregs(thread_t thread_id, struct RegFSet_t *rp, Proc proc)
{
	return (TDB_ERR);
}

thread_db_err_e
thread_db_set_regs(thread_t tid, struct RegGSet_t *rp, Proc proc)
{
	uthread_t *t = (uthread_t *)tid;

	if (IS_ONPROC(t) || IS_BOUND(t)) {
		VCpu vcpu;
		struct RegGSet_t * vrp;

		vcpu = VCpu_by_id(proc, thread_lwpid(t), 0);
		if (!vcpu) {
			terror ("cannot locate l@%d.", tid);
			return (TDB_ERR);
		}
		vrp = VCpu_get_regs(vcpu);
		*vrp = *rp;
		VCpu_make_dirty(vcpu);
	} else {
		terror ("setting on non-ONPROC thread doesn't work yet");
		return (TDB_ERR);
	}
	return (TDB_OK);

}

thread_db_err_e
thread_db_set_fregs(thread_t tid, struct RegFSet_t *rp, Proc proc)
{
	uthread_t *t = (uthread_t *)tid;

	if (IS_ONPROC(t) || IS_BOUND(t)) {
		struct RegFSet_t * vrp;
		VCpu vcpu;

		vcpu = VCpu_by_id(proc, thread_lwpid(t), false);
		if (!vcpu) {
			terror ("cannot locate l@%d.", tid);
			return (TDB_ERR);
		}
		vrp = VCpu_get_fregs(vcpu);
		*vrp = *rp;
		VCpu_make_dirty(vcpu);
	} else {
		terror ("setting on non-ONPROC threads doesn't work yet");
		return (TDB_ERR);
	}
}

/*
 * setup a handler to catch thr_exit() or lwp_exit() events.
 */
thread_db_err_e
thread_db_notify_on_death(thread_t tid)
{
	Handler h;
	Action a;
	VCpu vcpu = 0;
	unsigned addr = 0;

	/* setup thr_exit() call back */

	if (!(addr = quick_lookup("thr_exit")))
		return (TDB_ERR);
	h = Handler_new_bpt(Event_BPT, Handler_INTERNAL, addr, false);
	a = Action_new_callback(Action_CALLBACK, cb_thread_death, NULL);
	Handler_add_action(h, a);
	Handler_set_origin_str(h, "# thread_db: thread death 1");
	Handler_toggle(h, true);

	/* setup call back for when a LWP exits */

	h = Handler_new(Event_VCPU_DEATH, Handler_INTERNAL);
	a = Action_new_callback(Action_CALLBACK, cb_thread_death, NULL);
	Handler_add_action(h, a);
	Handler_set_origin_str(h, "# thread_db: thread death LWP died");
	Handler_toggle(h, true);

	return (TDB_OK);
}

thread_db_err_e
thread_db_stksegment(thread_t tid, stack_t *stk)
{
	uthread_t *t = (uthread_t *)tid;

	stk->ss_sp = (caddr_t)thread_stkbottom(t);
	stk->ss_size = thread_stksize(t);
	stk->ss_flags = 0;
	return (TDB_OK);
}
