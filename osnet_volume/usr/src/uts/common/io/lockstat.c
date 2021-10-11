/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockstat.c	1.4	99/07/26 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <sys/atomic.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * A note on lockstat hashing: we hash based on three things: lock address,
 * caller and event.  As long as the maximum number of events (currently 64)
 * is less than the number of hash buckets (minimum is 64), we don't have to
 * check for ls_event == event during hash lookup: if the lock and caller
 * match, we could not have hashed to the chain we're in unless the event
 * matched as well.  This saves a load/compare/branch in lockstat_record.
 */
#define	LOCKSTAT_HASH(lp, caller, event)	\
	&ls_hash[((((lp) + (caller)) >> 3) + (event)) & ls_hash_mask]

#define	LOCKSTAT_PEND_HASH(lp, owner)		\
	&ls_pend[(((lp) >> 3) + ((owner) >> 8)) & ls_hash_mask]

#define	LOCKSTAT_PEND_MAX_SEARCH	256
#define	LOCKSTAT_PEND_STRIDE		51

static lsctl_t		lsctl;		/* user-supplied control parameters */
static lock_t		lockstat_lock;	/* protects hash table and freelist */
static int		lockstat_lock_max_depth; /* allows limited recursion */
static lsrec_t		**ls_hash;	/* hash table for lock lookup */
static uintptr_t	ls_hash_mask;	/* mask for hash table */
static ls_pend_t	*ls_pend;	/* pending event hash ring */
static int		ls_pend_size;	/* size of pending event hash ring */
static char  		*ls_buffer;	/* main buffer for lock records */
static char  		*ls_free;	/* next available record in buffer */
static size_t		ls_bufsize;	/* size of main buffer in bytes */
static uint_t		ls_stk_depth;	/* stack depth for LS_STACK() runs */
static uint32_t		ls_open;	/* set if driver is open */
static dev_info_t	*lockstat_devi;	/* saved in xxattach() for xxinfo() */
static lsrec_t		*ls_record_fail; /* pointer to LS_RECORD_FAILED rec */
static lsrec_t		ls_hash_terminator; /* ends all hash chains */
static kmutex_t		lockstat_test;	/* for testing purposes only */

/*
 * Like highbit(), but takes a 64-bit arg and maps 2^n -> n instead of n + 1.
 */
static int
lockstat_bucket(uint64_t x)
{
	int h = 0;
	uint32_t l;

	if (x >> 32)
		h = 32, l = x >> 32;
	else
		l = x;
	if (l >> 16)
		h += 16, l >>= 16;
	if (l >> 8)
		h += 8, l >>= 8;
	if (l >> 4)
		h += 4, l >>= 4;
	if (l >> 2)
		h += 2, l >>= 2;
	if (l >> 1)
		h += 1, l >>= 1;

	return (h);
}

static void
lockstat_record(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, hrtime_t duration)
{
	lsrec_t *lsp, **bucket;
	uintptr_t stk[LS_MAX_STACK_DEPTH + 1];
	uint_t fr, depth;
	lswatch_t *lw;

	if (lsctl.lc_wlock[0].lw_base != 0 && event < LS_ERROR_BASE) {
		for (lw = &lsctl.lc_wlock[0]; lw->lw_base != 0; lw++)
			if (lp - lw->lw_base < lw->lw_size)
				break;
		if (lw->lw_base == 0)
			return;
	}

	if (duration < lsctl.lc_min_duration[event])
		return;

	if (ls_stk_depth != 0)
		depth = getpcstack(stk, ls_stk_depth);
	else
		depth = 0;

	if (lsctl.lc_wfunc[0].lw_base != 0 && event < LS_ERROR_BASE) {
		for (lw = &lsctl.lc_wfunc[0]; lw->lw_base != 0; lw++) {
			if (caller - lw->lw_base < lw->lw_size)
				break;
			for (fr = 0; fr < depth; fr++)
				if (stk[fr] - lw->lw_base < lw->lw_size)
					break;
			if (fr < depth)
				break;
		}
		if (lw->lw_base == 0)
			return;
	}

	bucket = LOCKSTAT_HASH(lp, caller, event);
	lsp = *bucket;

	do {
		if (lsp->ls_lock == lp && lsp->ls_caller == caller) {
			for (fr = 0; fr < depth; fr++)
				if (stk[fr] != lsp->ls_stack[fr])
					break;
			if (fr != depth)
				continue;
			lsp->ls_count++;
			lsp->ls_refcnt += refcnt;
			if (lsctl.lc_recsize >= LS_TIME)
				lsp->ls_time += duration;
			if (lsctl.lc_recsize >= LS_HIST)
				lsp->ls_hist[lockstat_bucket(duration)]++;
			return;
		}
	} while ((lsp = lsp->ls_next) != NULL);

	/*
	 * This is the first occurrence of <event, lp, caller>,
	 * so we have to add a new record to the hash chain.
	 */
	if (ls_free - ls_buffer == ls_bufsize) {	/* out of records */
		ls_record_fail->ls_count++;
		return;
	}

	/*
	 * The lockstat driver can record events on its own lock, but:
	 *
	 *  (1) normally we don't want to do that because it adds to the
	 *	Heisenberg effect; we always get data on lockstat_lock
	 *	contention anyway via the LS_RECORD_FAILED events;
	 *  (2) we have to be careful to prevent recursion; and
	 *  (3) if we're tracing, we don't want to see lockstat_lock
	 *	appear as every other event (literally).
	 *
	 * Thus, we only record events on lockstat_lock if we're not
	 * tracing *and* lockstat_lock_max_depth is at least 3 (the
	 * degree of recursion necessary to bootstrap lockstat_lock
	 * onto the hash chains).
	 */
	if (lp == (uintptr_t)&lockstat_lock &&
	    (lockstat_depth() >= lockstat_lock_max_depth ||
	    (lockstat_event[event] & LSE_TRACE)))
		return;

	/*
	 * We must never block in the lockstat code, so if we can't
	 * grab lockstat_lock immediately we just punt.  If the lock is
	 * interesting (i.e. oft contended) we'll be back to try again.
	 */
	if (!lock_try(&lockstat_lock)) {
		ls_record_fail->ls_count++;
		return;
	}

	/*
	 * Make sure nobody else snuck in and added a matching record.
	 */
	lsp = *bucket;
	do {
		if (lsp->ls_lock != lp || lsp->ls_caller != caller)
			continue;
		for (fr = 0; fr < depth; fr++)
			if (stk[fr] != lsp->ls_stack[fr])
				break;
		if (fr != depth)
			continue;
		lock_clear(&lockstat_lock);
		return;
	} while ((lsp = lsp->ls_next) != NULL);

	if (ls_free - ls_buffer == ls_bufsize) {	/* out of records */
		ls_record_fail->ls_count++;
		lock_clear(&lockstat_lock);
		return;
	}

	lsp = (lsrec_t *)ls_free;
	ls_free += lsctl.lc_recsize;

	lsp->ls_next = *bucket;
	lsp->ls_lock = lp;
	lsp->ls_caller = caller;
	lsp->ls_event = event;
	lsp->ls_refcnt = refcnt;

	for (fr = 0; fr < depth; fr++)
		lsp->ls_stack[fr] = stk[fr];

	lsp->ls_count = 1;
	if (lsctl.lc_recsize >= LS_TIME)
		lsp->ls_time = duration;
	if (lsctl.lc_recsize >= LS_HIST)
		lsp->ls_hist[lockstat_bucket(duration)] = 1;


	/*
	 * If we're tracing (rather than the usual sampling) we record
	 * the owner but we don't add the record to the hash chain.
	 * This ensures that all events on the same <lp, caller, event>
	 * get distinct records.  Conveniently, this makes the ls_next
	 * field available to hold the owner.
	 */
	if (lockstat_event[event] & LSE_TRACE) {
		lsp->ls_next = (void *)curthread;
	} else {
		/*
		 * Make sure the contents of the new record are visible
		 * before the change to the hash chain becomes visible.
		 */
		membar_producer();
		*bucket = lsp;
	}

	lock_clear(&lockstat_lock);
}

static void
lockstat_enter(uintptr_t lp, uintptr_t caller, uintptr_t owner)
{
	ls_pend_t *lpp, *lastlpp;
	lswatch_t *lw;

	if (lsctl.lc_wlock[0].lw_base != 0) {
		for (lw = &lsctl.lc_wlock[0]; lw->lw_base != 0; lw++)
			if (lp - lw->lw_base < lw->lw_size)
				break;
		if (lw->lw_base == 0)
			return;
	}

	lpp = LOCKSTAT_PEND_HASH(lp, owner);
	lastlpp = lpp + LOCKSTAT_PEND_MAX_SEARCH;
	do {
		if (lockstat_event_start(lp, lpp) == 0) {
			lpp->lp_owner = owner;
			lpp->lp_caller = caller;
			return;
		}
		if (lpp->lp_lock == lp && lpp->lp_owner == owner) {
			lpp->lp_refcnt++;
			if (lockstat_event[LS_RECURSION_DETECTED] & LSE_RECORD)
				lockstat_record(lp, caller,
				    LS_RECURSION_DETECTED,
				    lpp->lp_refcnt + 1, 0);
			return;
		}
	} while ((lpp += LOCKSTAT_PEND_STRIDE) < lastlpp);
	if (lockstat_event[LS_ENTER_FAILED] & LSE_RECORD)
		lockstat_record(lp, caller, LS_ENTER_FAILED, 1, 0);
}

static void
lockstat_exit(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, uintptr_t owner)
{
	ls_pend_t *lpp, *lastlpp;
	lswatch_t *lw;

	if (lsctl.lc_wlock[0].lw_base != 0) {
		for (lw = &lsctl.lc_wlock[0]; lw->lw_base != 0; lw++)
			if (lp - lw->lw_base < lw->lw_size)
				break;
		if (lw->lw_base == 0)
			return;
	}

	lpp = LOCKSTAT_PEND_HASH(lp, owner);
	lastlpp = lpp + LOCKSTAT_PEND_MAX_SEARCH;
	do {
		if (lpp->lp_lock != lp || lpp->lp_owner != owner)
			continue;
		if (lpp->lp_refcnt != 0) {
			lpp->lp_refcnt--;
			return;
		}
		lpp->lp_owner = 0;
		lockstat_record(lp, caller, event, refcnt,
		    lockstat_event_end(lpp));
		return;
	} while ((lpp += LOCKSTAT_PEND_STRIDE) < lastlpp);
}

static void
lockstat_enable(void)
{
	lockstat_interrupt_on(lsctl.lc_interval);

	bcopy(lsctl.lc_event, lockstat_event, sizeof (lockstat_event));

	lockstat_hot_patch();

	lockstat_exit_op = lockstat_exit;
	membar_producer();

	lockstat_enter_op = lockstat_enter;
	membar_producer();

	lockstat_record_op = lockstat_record;
	membar_producer();

	/*
	 * Immediately generate a record for the lockstat_test mutex
	 * to verify that the mutex hot-patch code worked as expected.
	 */
	mutex_enter(&lockstat_test);
	mutex_exit(&lockstat_test);
}

static void
lockstat_disable(void)
{
	ls_pend_t *lpp;

	lockstat_record_op = lockstat_record_nop;
	membar_producer();

	lockstat_enter_op = lockstat_enter_nop;
	membar_producer();

	lockstat_exit_op = lockstat_exit_nop;
	membar_producer();

	bzero(lockstat_event, sizeof (lockstat_event));
	membar_producer();

	lockstat_hot_patch();

	/*
	 * The delay() here isn't as cheesy as you might think.  We don't
	 * want to busy-loop in the kernel, so we have to give up the
	 * CPU between calls to lockstat_active_threads(); that much is
	 * obvious.  But the reason it's a do..while loop rather than a
	 * while loop is subtle.  The memory barriers above guarantee that
	 * no threads will enter the lockstat code from this point forward.
	 * However, another thread could already be executing lockstat code
	 * without our knowledge if the update to its t_lockstat field hasn't
	 * cleared its CPU's store buffer.  Delaying for one clock tick
	 * guarantees that either (1) the thread will have *ample* time to
	 * complete its work, or (2) the thread will be preempted, in which
	 * case it will have to grab and release a dispatcher lock, which
	 * will flush that CPU's store buffer.  Either way we're covered.
	 */
	do {
		delay(1);
	} while (lockstat_active_threads());

	lpp = (ls_pend_t *)((char *)ls_pend + ls_pend_size);
	while (--lpp >= ls_pend) {
		uintptr_t lp = lpp->lp_lock;
		if (lp != 0 && (lsctl.lc_event[LS_EXIT_FAILED] & LSE_RECORD)) {
			uintptr_t caller = lpp->lp_caller;
			lockstat_record(lp, caller, LS_EXIT_FAILED,
			    1, lockstat_event_end(lpp));
		}
	}

	lockstat_interrupt_off();
}

/*ARGSUSED*/
static int
lockstat_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (cas32(&ls_open, 0, 1) == 0 ? 0 : EBUSY);
}

/*ARGSUSED*/
static int
lockstat_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	if (ls_buffer != NULL) {
		lockstat_disable();
		kmem_free(ls_buffer, ls_bufsize);
		kmem_free(ls_hash, (ls_hash_mask + 1) * sizeof (lsrec_t *));
		kmem_free(ls_pend, ls_pend_size);
		ls_buffer = NULL;
		ls_hash = NULL;
		ls_pend = NULL;
	}
	ls_open = 0;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
lockstat_read(dev_t dev, struct uio *uio, struct cred *cred)
{
	if (ls_buffer == NULL)
		return (ENXIO);

	if (uio->uio_offset >= ls_bufsize)
		return (EINVAL);

	return (uiomove(ls_buffer + uio->uio_offset, min(uio->uio_resid,
	    (ls_free - ls_buffer) - uio->uio_offset), UIO_READ, uio));
}

/* ARGSUSED */
static int
lockstat_write(dev_t dev, struct uio *uio, struct cred *cred)
{
	int i;

	if (ls_buffer != NULL)
		return (EBUSY);

	if (uio->uio_resid != sizeof (lsctl_t))
		return (ENXIO);

	/*
	 * Our control structures are sensitive to the data
	 * model of the kernel.  Fail nicely here, rather
	 * than horribly later.
	 */
	if (get_udatamodel() != DATAMODEL_NATIVE)
		return (EOVERFLOW);

	if (uiomove(&lsctl, uio->uio_resid, UIO_WRITE, uio) != 0)
		return (EFAULT);

	if (lsctl.lc_recsize % sizeof (uint64_t) != 0)
		return (EINVAL);

	switch (lsctl.lc_recsize) {

	case LS_BASIC:
	case LS_TIME:
	case LS_HIST:
		ls_stk_depth = 0;
		break;

	default:
		ls_stk_depth = (lsctl.lc_recsize - LS_HIST) / sizeof (void *);

		if (ls_stk_depth > LS_MAX_STACK_DEPTH)
			return (EINVAL);

		break;
	}

	lsctl.lc_wlock[LS_MAX_WATCH].lw_base = 0;
	lsctl.lc_wfunc[LS_MAX_WATCH].lw_base = 0;

	if (lsctl.lc_nrecs == 0)
		return (EINVAL);

	ls_bufsize = lsctl.lc_recsize * lsctl.lc_nrecs;
	if (ls_bufsize > kmem_maxavail() / 4)
		return (ENOMEM);

	ls_buffer = ls_free = kmem_zalloc(ls_bufsize, KM_SLEEP);

	ls_hash_mask = (1 << highbit(lsctl.lc_nrecs + LS_MAX_EVENTS)) - 1;
	ls_hash = kmem_zalloc((ls_hash_mask + 1) * sizeof (void *), KM_SLEEP);
	for (i = 0; i <= ls_hash_mask; i++)
		ls_hash[i] = &ls_hash_terminator;

	ls_pend_size = (ls_hash_mask + 1 + LOCKSTAT_PEND_MAX_SEARCH) *
	    sizeof (ls_pend_t);
	ls_pend = kmem_zalloc(ls_pend_size, KM_SLEEP);

	ls_record_fail = (lsrec_t *)ls_free;
	ls_free += lsctl.lc_recsize;
	ls_record_fail->ls_lock = (uintptr_t)&lockstat_lock;
	ls_record_fail->ls_caller = (uintptr_t)lockstat_record;
	ls_record_fail->ls_event = LS_RECORD_FAILED;

	lockstat_enable();
	return (0);
}

/* ARGSUSED */
static int
lockstat_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *) lockstat_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

static int
lockstat_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "lockstat", S_IFCHR,
	    0, DDI_PSEUDO, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	lockstat_devi = devi;
	return (DDI_SUCCESS);
}

static int
lockstat_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*
 * Configuration data structures
 */
static struct cb_ops lockstat_cb_ops = {
	lockstat_open,		/* open */
	lockstat_close,		/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	lockstat_read,		/* read */
	lockstat_write,		/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab */
	D_MP | D_NEW		/* Driver compatibility flag */
};

static struct dev_ops lockstat_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt */
	lockstat_info,		/* getinfo */
	nulldev,		/* identify */
	nulldev,		/* probe */
	lockstat_attach,	/* attach */
	lockstat_detach,	/* detach */
	nulldev,		/* reset */
	&lockstat_cb_ops,	/* cb_ops */
	(struct bus_ops *)0,	/* bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Lock Statistics",	/* name of module */
	&lockstat_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
