/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpc.c	1.3	99/11/20 SMI"

/*
 * CPU Performance Counter system calls and device driver.
 *
 * This module uses a combination of thread context operators, and
 * thread-specific data to export CPU performance counters
 * via both a system call and a driver interface.
 *
 * There are three access methods exported - the 'shared' device
 * and the 'private' and 'agent' variants of the system call.
 *
 * The shared device treats the performance counter registers as
 * a processor metric, regardless of the work scheduled on them.
 * The private system call treats the performance counter registers
 * as a property of a single lwp.  This is achieved by using the
 * thread context operators to virtualize the contents of the
 * performance counter registers between lwps.
 *
 * The agent method is like the private method, except that it must
 * be accessed via /proc's agent lwp to allow the counter context of
 * other threads to be examined safely.
 *
 * The shared usage fundamentally conflicts with the agent and private usage;
 * almost all of the complexity of the module is needed to allow these two
 * models to co-exist in a reasonable way.
 *
 * The cpc_hw_*() routines used by the module are present in the ISA-specific
 * cpc_subr.c files.  A brief description of their function follows:
 *
 * void kcpc_hw_sample(kcpc_ctx_t *)
 *  sample the performance counter registers, updating the
 *  virtualized state in the ctx argument.
 *
 * void kcpc_hw_overflow_intr(void *, kcpc_ctx_t *)
 *  invoked when a performance counter overflow interrupt occurs
 *  invoked at high interrupt priority, so the only duty of the routine
 *  is to disable the source of the interrupt
 *
 * void kcpc_hw_overflow_trap(kthread_t *)
 *  invoked from trap via an ast prior to delivering a signal
 *  should sample the hardware context and mark it frozen
 *
 * void kcpc_hw_save(kcpc_ctx_t *)
 *  called when switching away from the current thread -or- when
 *  changing the state of the performance counter control registers directly.
 *  samples the hardware counters and passivates the control register(s).
 *
 * void kcpc_hw_restore(kcpc_ctx_t *)
 *  called when switching to the current thread -or- when
 *  changing the state of the performance counter control registers directly.
 *  presets the hardware counters, and loads the control register(s).
 *
 * int kcpc_hw_bind(kcpc_ctx_t *)
 *  validates the control register context and the flags, setting up
 *  the appropriate hardware dependent state in the context structure.
 *
 * void kcpc_hw_setusrsys(kcpc_ctx_t *, int, int)
 *  enable or disable counting in user or system mode
 *  only adjusts the software state bits
 *
 * void kcpc_hw_clone(kcpc_ctx_t *, kcpc_ctx_t *)
 *  a new zeroed context has just been created for a new lwp - inherit
 *  non-zero context from the current context to the new one.
 *
 * int kcpc_hw_probe(void)
 *  test to see if we can really do performance counting on this platform
 *
 * int kcpc_hw_add_ovf_intr(uint_t (*)(caddr_t))
 *  add an interrupt handler for hardware overflow interrupts.
 *  returns -1 if failed, 0 for success.
 *
 * void kcpc_hw_rem_ovf_intr(void)
 *  remove the interrupt handler for hardware overflow interrupts.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/atomic.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/cpc_event.h>
#include <sys/cpc_impl.h>

#define	HASH_LOG_BUCKETS	6	/* => 64, for now */
#define	HASH_BUCKETS		(1l << HASH_LOG_BUCKETS)
#define	HASH_CTX(ctx)		((((long)(ctx)) >> 7) & (HASH_BUCKETS - 1))

static krwlock_t kcpc_cpuctx_lock;	/* lock for 'kcpc_cpuctx' below */
static int kcpc_cpuctx;			/* number of cpu-specific contexts */

static kmutex_t	kcpc_klock;		/* create lock for kcpc_key */
static kmutex_t	kcpc_ctx_llock[HASH_BUCKETS];	/* protects ctx_list */
static kcpc_ctx_t *kcpc_ctx_list[HASH_BUCKETS];	/* head of list */

/*
 * Allocator for performance counter context.
 *
 * Contexts are kept on a singly linked list; this is so they can
 * be invalidated by whoever wants to use the 'shared' device.
 * Contexts are kept as thread-specific data; either on the current
 * lwp, or via the agent thread to some other lwp.
 */
static kcpc_ctx_t *
kcpc_ctx_zalloc(kthread_t *t)
{
	kcpc_ctx_t *ctx;
	long hash;

	ctx = kmem_zalloc(sizeof (*ctx), KM_SLEEP);
	(void) tsd_agent_set(t, kcpc_key, ctx);
	ASSERT(ctx == ttocpcctx(t));

	hash = HASH_CTX(ctx);
	mutex_enter(&kcpc_ctx_llock[hash]);
	ctx->c_next = kcpc_ctx_list[hash];
	kcpc_ctx_list[hash] = ctx;
	mutex_exit(&kcpc_ctx_llock[hash]);

	return (ctx);
}

/*
 * Grab every existing context and mark it as invalid.
 */
static void
kcpc_invalidate_all(void)
{
	kcpc_ctx_t *ctx;
	long hash;

	ASSERT(kcpc_cpuctx == 1 && RW_WRITE_HELD(&kcpc_cpuctx_lock));

	for (hash = 0; hash < HASH_BUCKETS; hash++) {
		mutex_enter(&kcpc_ctx_llock[hash]);
		ctx = kcpc_ctx_list[hash];
		while (ctx) {
			atomic_or_uint(&ctx->c_flags, KCPC_CTX_INVALID);
			ctx = ctx->c_next;
		}
		mutex_exit(&kcpc_ctx_llock[hash]);
	}
}

/*
 * We're being called to delete the context; we ensure that there
 * are no remaining references to it on our hash chain, nor via the
 * thread we're running on.
 */
static void
kcpc_free(kcpc_ctx_t *ctx, int isexec)
{
	kcpc_ctx_t **loc;
	long hash = HASH_CTX(ctx);

	mutex_enter(&kcpc_ctx_llock[hash]);
	loc = &kcpc_ctx_list[hash];
	while (*loc != ctx)
		loc = &(*loc)->c_next;
	*loc = ctx->c_next;
	mutex_exit(&kcpc_ctx_llock[hash]);

	if (isexec) {
		/*
		 * Thread destruction automatically removes the association
		 * from the thread to the context in exit(2) or lwp_exit(2).
		 * This case corresponds to an exec(2) on this thread that
		 * would otherwise leave a dangling TSD pointer behind.
		 */
		ASSERT(ttocpcctx(curthread) == ctx);
		(void) tsd_set(kcpc_key, NULL);
	}

	ctx->c_flags = ~KCPC_CTX_ALLFLAGS;
	kmem_free(ctx, sizeof (*ctx));
}

static void
kcpc_null()
{}

/*ARGSUSED*/
static void
kcpc_lwp_create(kthread_id_t t, kthread_id_t ct)
{
	kcpc_ctx_t *ctx, *cctx;

	if ((ctx = ttocpcctx(t)) == NULL ||
	    (ctx->c_flags & KCPC_CTX_LWPINHERIT) == 0)
		return;

	rw_enter(&kcpc_cpuctx_lock, RW_READER);
	if (ctx->c_flags & KCPC_CTX_INVALID) {
		rw_exit(&kcpc_cpuctx_lock);
		return;
	}
	cctx = kcpc_ctx_zalloc(ct);
	kcpc_hw_clone(ctx, cctx);
	rw_exit(&kcpc_cpuctx_lock);

	if (cctx->c_flags & KCPC_CTX_SIGOVF) {
		/*
		 * fake an "impossible" overflow on the new lwp so
		 * that we can give the SIGEMT handler control of
		 * the new lwp.
		 */
		atomic_or_uint(&cctx->c_flags, KCPC_CTX_FREEZE);
		cctx->c_event.ce_pic[0] = UINT64_MAX;
		cctx->c_event.ce_pic[1] = cctx->c_event.ce_pic[0];
		ttolwp(ct)->lwp_pcb.pcb_flags |= CPC_OVERFLOW;
		aston(ct);
	}

	installctx(ct, cctx, kcpc_hw_save, kcpc_hw_restore,
	    kcpc_null, kcpc_lwp_create, kcpc_free);
}

/*
 * Generic interrupt handler used on hardware that generates
 * overflow interrupts.  If we're being asked for a signal on
 * overflow, then we return non-zero to cause the platform-dependent
 * interrupt handler to disable the counters.
 *
 * Note: executed at high-level interrupt context!
 */
/*ARGSUSED*/
static uint_t
kcpc_overflow_intr(caddr_t arg)
{
	kcpc_ctx_t *ctx;

	if ((ctx = ttocpcctx(curthread)) != NULL &&
	    (ctx->c_flags & KCPC_CTX_INVALID) == 0 &&
	    ctx->c_flags & KCPC_CTX_SIGOVF) {
		ttolwp(curthread)->lwp_pcb.pcb_flags |= CPC_OVERFLOW;
		aston(curthread);
		return (1);
	}
	return (0);
}

static int
kcpc_bind(kcpc_ctx_t *ctx, int priv, int flags)
{
	uint_t outflags = 0;

	/*
	 * Contexts that are marked frozen or invalid must stay that way.
	 */
	atomic_and_uint(&ctx->c_flags, KCPC_CTX_FREEZE | KCPC_CTX_INVALID);

	if (flags & CPC_BIND_LWP_INHERIT)
		outflags |= KCPC_CTX_LWPINHERIT;
	if (flags & CPC_BIND_EMT_OVF)
		outflags |= KCPC_CTX_SIGOVF;
	if (!priv)
		outflags |= KCPC_CTX_NONPRIV;
	if (outflags)
		atomic_or_uint(&ctx->c_flags, outflags);

	return (kcpc_hw_bind(ctx));
}

/*
 * System call to access CPU performance counters.
 */
static int
cpc(int cmd, id_t lwpid, cpc_event_t *data, int flags)
{
	kcpc_ctx_t *ctx;
	kthread_t *t;
	int error;

	if (curproc->p_agenttp == curthread) {
		/*
		 * Only if /proc is invoking this system call from
		 * the agent thread do we allow the caller to examine
		 * the contexts of other lwps in the process.  And
		 * because we know we're the agent, we know we don't
		 * have to grab p_lock because no-one else can change
		 * the state of the process.
		 */
		t = idtot(curproc->p_agenttp, lwpid);
		if (t == NULL || t == curthread)
			return (set_errno(ESRCH));
		ASSERT(t->t_tid == lwpid && ttolwp(t) != NULL);
	} else
		t = curthread;

	if ((ctx = ttocpcctx(t)) == NULL) {
		if (cmd != CPC_BIND_EVENT)
			return (set_errno(EINVAL));
	} else if ((ctx->c_flags & KCPC_CTX_INVALID) != 0 && cmd != CPC_RELE)
		return (set_errno(EAGAIN));

	switch (cmd) {
	case CPC_BIND_EVENT:
		if (ctx == NULL) {
			rw_enter(&kcpc_cpuctx_lock, RW_READER);
			if (kcpc_cpuctx) {
				rw_exit(&kcpc_cpuctx_lock);
				return (set_errno(EAGAIN));
			}
			ctx = kcpc_ctx_zalloc(t);
			rw_exit(&kcpc_cpuctx_lock);
			installctx(t, ctx, kcpc_hw_save, kcpc_hw_restore,
			    kcpc_null, kcpc_lwp_create, kcpc_free);
		}
		atomic_or_uint(&ctx->c_flags, KCPC_CTX_FREEZE);
		if (copyin(data, &ctx->c_event, sizeof (*data)) == -1)
			return (set_errno(EFAULT));
		if (t == curthread) {
			if ((error = kcpc_bind(ctx, 1, flags)) != 0)
				return (set_errno(error));
			/*
			 * Disable preemption here to ensure that we
			 * don't get moved to a different cpu, half
			 * way through programming our current cpu.
			 */
			kpreempt_disable();
			atomic_and_uint(&ctx->c_flags, ~KCPC_CTX_FREEZE);
			kcpc_hw_restore(ctx);
			kpreempt_enable();
		} else {
			if ((error = kcpc_bind(ctx, 0, flags)) != 0)
				return (set_errno(error));
			atomic_and_uint(&ctx->c_flags, ~KCPC_CTX_FREEZE);
		}
		return (0);

	case CPC_TAKE_SAMPLE:
		if (t == curthread && (ctx->c_flags & KCPC_CTX_FREEZE) == 0) {
			/*
			 * Disable preemption here to ensure that we don't
			 * get halfway through copying out the current sample
			 * before being context switched.
			 */
			kpreempt_disable();
			atomic_or_uint(&ctx->c_flags, KCPC_CTX_FREEZE);
			kcpc_hw_sample(ctx);
			error = copyout(&ctx->c_event, data, sizeof (*data));
			atomic_and_uint(&ctx->c_flags, ~KCPC_CTX_FREEZE);
			kpreempt_enable();
		} else
			error = copyout(&ctx->c_event, data, sizeof (*data));
		return (error == -1 ? set_errno(EFAULT) : 0);

	case CPC_SYS_EVENTS:
		/*
		 * If we're generating profile signals via interrupts,
		 * we only allow system events to be disabled.
		 */
		if (data != NULL && ctx->c_flags & KCPC_CTX_SIGOVF)
			return (set_errno(EINVAL));
		/*FALLTHROUGH*/

	case CPC_USR_EVENTS:
		if (t != curthread)
			return (set_errno(EINVAL));
		kpreempt_disable();
		kcpc_hw_save(ctx);
		kcpc_hw_setusrsys(ctx, cmd == CPC_USR_EVENTS, data != NULL);
		kcpc_hw_restore(ctx);
		kpreempt_enable();
		return (0);

	case CPC_INVALIDATE:
		atomic_or_uint(&ctx->c_flags, KCPC_CTX_INVALID);
		return (0);

	case CPC_RELE:
		(void) tsd_agent_set(t, kcpc_key, 0);
		(void) removectx(t, ctx,
		    kcpc_hw_save, kcpc_hw_restore,
		    kcpc_null, kcpc_lwp_create, kcpc_free);
		return (0);

	default:
		return (set_errno(EINVAL));
	}
}

/*
 * The 'shared' device allows direct access to the
 * performance counter control register of the current CPU.
 * The major difference between the contexts created here and those
 * above is that the context handlers are -not- installed, thus
 * no context switching behaviour occurs.
 *
 * Because they manipulate per-cpu state, these ioctls can
 * only be invoked from a bound lwp, by a caller who can open
 * the relevant entry in /devices (the act of holding it open causes
 * other uses of the counters to be suspended).
 *
 * Note that for correct results, the caller -must- ensure that
 * all existing per-lwp contexts are either inactive or marked invalid;
 * that's what the open routine does.
 */
/*ARGSUSED*/
static int
kcpc_ioctl(dev_t dev, int cmd, intptr_t data, int flags, cred_t *cr, int *rvp)
{
	kcpc_ctx_t *ctx = ttocpcctx(curthread);
	int error;

	if (curthread->t_bind_cpu != getminor(dev))
		return (EAGAIN);  /* someone unbound it? */

	switch (cmd) {
	case CPCIO_BIND_EVENT:
		if (ctx == NULL) {
			ctx = kcpc_ctx_zalloc(curthread);
			installctx(curthread, ctx,
			    kcpc_null, kcpc_null,
			    kcpc_null, kcpc_null, kcpc_free);
		}
		if (copyin((void *)data,
		    &ctx->c_event, sizeof (ctx->c_event)) == -1)
			return (EFAULT);
		if ((error = kcpc_bind(ctx, 1, 0)) != 0)
			return (error);
		kcpc_hw_restore(ctx);
		return (0);

	case CPCIO_TAKE_SAMPLE:
		return (cpc(CPC_TAKE_SAMPLE, -1, (cpc_event_t *)data, 0));

	case CPCIO_RELE:
		if (ctx == NULL)
			return (EINVAL);
		(void) tsd_set(kcpc_key, NULL);
		(void) removectx(curthread, ctx,
		    kcpc_null, kcpc_null, kcpc_null, kcpc_null, kcpc_free);
		return (0);

	default:
		return (ENOTTY);
	}
}

/*
 * The device supports multiple opens, but only one open
 * is allowed per processor.  This is to enable multiple
 * instances of tools looking at different processors.
 */

#define	KCPC_MINOR_SHARED		((minor_t)0x3fffful)

static ulong_t *kcpc_cpumap;		/* bitmap of cpus */

/*ARGSUSED1*/
static int
kcpc_open(dev_t *dev, int flags, int otyp, cred_t *cr)
{
	processorid_t cpuid;

	if (getminor(*dev) != KCPC_MINOR_SHARED)
		return (ENXIO);
	if ((cpuid = curthread->t_bind_cpu) == PBIND_NONE)
		return (EINVAL);
	if (cpuid > NCPU - 1)
		return (EINVAL);

	rw_enter(&kcpc_cpuctx_lock, RW_WRITER);
	if (++kcpc_cpuctx == 1) {
		ASSERT(kcpc_cpumap == NULL);
		kcpc_cpumap = kmem_zalloc(BT_BITOUL(NCPU) * BT_NBIPUL,
		    KM_SLEEP);
		/*
		 * When this device is open for processor-based contexts,
		 * no further lwp-based contexts can be created.
		 *
		 * Since this is the first open, ensure that all existing
		 * contexts are invalidated.
		 */
		kcpc_invalidate_all();
	} else if (BT_TEST(kcpc_cpumap, cpuid)) {
		kcpc_cpuctx--;
		rw_exit(&kcpc_cpuctx_lock);
		return (EAGAIN);
	}
	BT_SET(kcpc_cpumap, cpuid);
	rw_exit(&kcpc_cpuctx_lock);

	*dev = makedevice(getmajor(*dev), (minor_t)cpuid);

	return (0);
}

/*ARGSUSED1*/
static int
kcpc_close(dev_t dev, int flags, int otyp, cred_t *cr)
{
	rw_enter(&kcpc_cpuctx_lock, RW_WRITER);
	BT_CLEAR(kcpc_cpumap, getminor(dev));
	if (--kcpc_cpuctx == 0) {
		kmem_free(kcpc_cpumap, BT_BITOUL(NCPU) * BT_NBIPUL);
		kcpc_cpumap = NULL;
	}
	ASSERT(kcpc_cpuctx >= 0);
	rw_exit(&kcpc_cpuctx_lock);

	return (0);
}

static struct cb_ops cb_ops = {
	kcpc_open,
	kcpc_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	kcpc_ioctl,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

/*ARGSUSED*/
static int
kcpc_probe(dev_info_t *devi)
{
	return (kcpc_hw_probe() ? DDI_PROBE_SUCCESS : DDI_PROBE_FAILURE);
}

static dev_info_t *kcpc_devi;

static int
kcpc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	kcpc_devi = devi;
	rw_init(&kcpc_cpuctx_lock, NULL, RW_DEFAULT, NULL);
	return (ddi_create_minor_node(devi, "shared", S_IFCHR,
	    KCPC_MINOR_SHARED, NULL, NODESPECIFIC_DEV));
}

/*
 * Making this module unloadable isn't practical for a couple
 * of reasons.  First, we rely on the system call be non-unloadable
 * to make it faster.  Second, and more seriously, the system
 * currently arranges to free tsd synchronously, but free contexts
 * lazily.  While it's relatively trivial to ask about the existence
 * of any particular tsd belonging to a specific key in the system,
 * it's more complex to ensure that all the threads in the system
 * have had their contexts freed up (and thus have no dangling
 * references to code in this module.)
 */

/*ARGSUSED*/
static int
kcpc_getinfo(dev_info_t *devi, ddi_info_cmd_t cmd, void *arg, void **result)
{
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		switch (getminor((dev_t)arg)) {
		case KCPC_MINOR_SHARED:
			*result = kcpc_devi;
			return (DDI_SUCCESS);
		default:
			break;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		return (DDI_SUCCESS);
	default:
		break;
	}

	return (DDI_FAILURE);
}

static struct dev_ops dev_ops = {
	DEVO_REV,
	0,
	kcpc_getinfo,
	nulldev,		/* identify */
	kcpc_probe,
	kcpc_attach,
	nodev,			/* detach */
	nodev,			/* reset */
	&cb_ops,
	(struct bus_ops *)0
};

static struct modldrv modldrv = {
	&mod_driverops,
	"cpc sampling driver v1.3",
	&dev_ops
};

static struct sysent cpc_sysent = {
	4,
	SE_NOUNLOAD | SE_ARGC | SE_32RVAL1,
	cpc
};

static struct modlsys modlsys = {
	&mod_syscallops,
	"cpc sampling system call",
	&cpc_sysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32,
	"32-bit cpc sampling system call",
	&cpc_sysent
};
#endif

static struct modlinkage modl = {
	MODREV_1,
	&modldrv,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
};

/*
 * If the system call isn't loaded, and multiple lwps
 * attempt to execute it, each one of them will find themselves
 * in the _init routine attempting to initialize the system call.
 *
 * While mod_install can handle this (it only installs the system
 * call once!), we need to serialize the creation of our tsd key
 * so that we don't end up with multiple keys!
 */
int
_init(void)
{
	int ret;

	mutex_enter(&kcpc_klock);
	if (kcpc_key == 0) {
		tsd_create(&kcpc_key, NULL);
		(void) kcpc_hw_add_ovf_intr(kcpc_overflow_intr);
	}
	mutex_exit(&kcpc_klock);

	if ((ret = mod_install(&modl)) != 0) {
		mutex_enter(&kcpc_klock);
		kcpc_hw_rem_ovf_intr();
		tsd_destroy(&kcpc_key);
		mutex_exit(&kcpc_klock);
	}
	return (ret);
}

int
_fini(void)
{
	return (mod_remove(&modl));
}

int
_info(struct modinfo *mi)
{
	return (mod_info(&modl, mi));
}
