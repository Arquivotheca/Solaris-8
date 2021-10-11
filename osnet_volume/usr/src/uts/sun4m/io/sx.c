/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Device and Segment Driver for SX graphics accelerator.
 */

#pragma ident	"@(#)sx.c	1.127	98/11/21 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/t_lock.h>
#include <sys/ksynch.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/varargs.h>
#include <sys/async.h>
#include <sys/aflt.h>
#include <sys/vmsystm.h>

#include <sys/time.h>
#include <sys/mmu.h>
#include <sys/pte.h>

#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <vm/mhat.h>

#include <sys/hat_sx.h>
#include <sys/sxreg.h>
#include <sys/sxio.h>

/*
 * SX Chip bugs.
 */
#define	SX_P0_BUG1	/* Proto0 SX chips generate spurious interrupts */
			/* about misaligned load/stores */

#ifdef SX_DEBUG
static int debug = 1;
#else /* SX_DEBUG */
static int debug = 0;
#endif /* SX_DEBUG */
void sx_debug(int, char *, ...);

int sx_tmpflag = SX_TMPUNLOAD;	/* patchable to 0 to turn off TMPUNLOAD */

/*
 * The SX segment driver sets up all translations with page frame numbers
 * whose high order 8-bits have the MBus type 0x08.
 */
#define	SX_MAKEPF(pf, bus_type)	((pf & 0x0fffff) | (bus_type << 20))

static const caddr_t SX_FAULT_MSG1 = "segsx_fault: Couldn't find orig seg\n";

/*
 *  Private support routines
 */
extern void	sx_fatal_err(struct proc *);
static void 	sx_cntxtinit(sx_cntxt_t *);
static sx_proc_t		*sx_getproc(int, struct as *);
static sx_proc_t		*sx_addproc(int, struct as *);
static void			sx_freeproc(int, sx_proc_t *);

static int 	sx_fault_cmem(struct hat *, struct as *, caddr_t, caddr_t,
				u_int, u_int);
static int 	sx_fault_vidmem(struct hat *, struct as *, caddr_t,
				caddr_t, u_int, u_int);
static int 	sx_fault_pageable_mem(struct hat *, struct as *, caddr_t,
				caddr_t, devpage_t *, u_int, u_int);

static u_int	sx_getpfnum(struct as *, caddr_t);

static int sx_setupinfo(dev_info_t *);
static int sx_vrfy_cntxt_setup(void);
static void sx_cntxtcopy(struct sx_proc *, struct sx_proc *);
static void sx_regrestore(struct sx_cntxt *);
static int sx_regsave(struct sx_cntxt *);
static int sx_cntxtswtch(struct sx_proc *);
static void sx_orig_unmap(struct as *, caddr_t, u_int);
static void sx_init(void);

extern int sx_vrfy_va(struct sx_valid_va *);
extern int sx_get_orig_va(struct sx_original_va *);
extern int sx_vrfy_pfn(u_int, u_int);

kmutex_t	sx_tr_lock;

#ifdef SX_DEBUG
#define	SX_TR_BUFSIZE		0x8000
int	sx_tr_flag = 1;		/* makes tracing patchable */
struct sx_trace {
	struct as 	*tr_as;
	char		*tr_op;
} sx_tr_buf[SX_TR_BUFSIZE];

u_int	sx_tr_event = 0;

char *sx_tr_messages[] = {
	"context create",
	"current context being destroyed",
	"non-current context being destroyed",
	"contextswitch:  in",
	"contextswitch:  out",
	""
};
#define	SX_TRACE(as, msg)						\
	if (sx_tr_flag) {						\
		mutex_enter(&sx_tr_lock); 				\
		sx_tr_buf[++sx_tr_event % SX_TR_BUFSIZE].tr_as = (as); 	\
		sx_tr_buf[sx_tr_event % SX_TR_BUFSIZE].tr_op = 		\
				sx_tr_messages[(msg)]; 			\
		mutex_exit(&sx_tr_lock);				\
	}

#else
#define	SX_TRACE(a, b)
#endif /* SX_DEBUG */

/*
 * Most of the work done by the SX driver involves cloning a range of
 * a process's virtual address space i.e setting up virtual memory segment
 * in the process's address space such that both the original virtual address
 * and the cloned virtual address are backed by the same physical store.
 * The cloned virtual address space will be called the SPAMified
 * virtual address space. What distinguishes the underlying physical address
 * of the original virtual address space from the underlying physical address
 * of the SPAMified virtual address space is the high order 4-bits of the
 * 36-bit MBus physical address space. It is 0 for the physical addresses
 * underlying the original virtual address space and 8 for the physical
 * addresses underlying the SPAMified virtual address space.
 * The SX graphics library routines create the SPAMified virtual address
 * space to write SX instructions. Reading from a SPAMified virtual address
 * will cause an MBus error.
 */

/*
 * The SX segment driver DOES NOT use any per segment or per device
 * locks. All these locks are global locks. These locks are:
 *
 *	0) sx_aflt_mutex, used ONLY during level 15 sx async fault handler.
 *
 * 	1) sx_hw_mutex, used to access the SX hardware and a few miscellaneous
 *	   data structures.
 *
 *	2) sx_cntxt_mutex, lock used to access all data
 *	   structures used for an SX context switch.
 *
 * 		a) sx_cntxt_tbl: the SX context table and all the other
 *		   data structures referenced by the entry in the context
 *		   table.
 *		b) sx_current_proc: the currently active SX process
 */

kmutex_t	sx_aflt_mutex;  /* Mutex for protecting async fault */
kmutex_t	sx_hw_mutex;	/* Mutex for synchronizing access to HW */
kmutex_t	sx_cntxt_mutex; /* SX context switch lock */

/*
 * Macros for SX context switch locking
 */
#define	SX_CNTXT_LOCK_ENTER(lock)		mutex_enter((lock))
#define	SX_CNTXT_LOCK_EXIT(lock)		mutex_exit((lock))
#define	SX_CNTXT_LOCK_DESTROY(lock)		mutex_destroy((lock))
#define	SX_CNTXT_LOCK_HELD(lock)		MUTEX_HELD((lock))

/*
 * Macros for SX HW lock
 */
#define	SX_HW_LOCK_ENTER(lock)		mutex_enter((lock))
#define	SX_HW_LOCK_EXIT(lock)		mutex_exit((lock))
#define	SX_HW_LOCK_DESTROY(lock)	mutex_destroy((lock))
#define	SX_HW_LOCK_HELD(lock)		MUTEX_HELD((lock))

/*
 * Macros for SX async fault lock
 */
#define	SX_AFLT_LOCK_ENTER(lock)	mutex_enter((lock))
#define	SX_AFLT_LOCK_EXIT(lock)		mutex_exit((lock))
#define	SX_AFLT_LOCK_DESTROY(lock)	mutex_destroy((lock))
#define	SX_AFLT_LOCK_HELD(lock)		MUTEX_HELD((lock))

#define	SX_REGSAVE_SUCCESS	0
#define	SX_REGSAVE_FAIL		-1

static	int sx_diag_default = SX_DIAG_INIT; /* Default value for diag reg */
static int sx_bus_type = 0x8;	/* MBus address space for SX accesses */
static dev_info_t *sx_dip;	/* Private copy of the devinfo ptr */

/*
 * Entry points
 */
static int sx_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int sx_close(dev_t, int flag, int otyp, cred_t *);
static int sx_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
			cred_t *credp, int *rvalp);
static int sx_mmap(dev_t dev, off_t off, int prot);
static int sx_segmap(dev_t dev, off_t off, struct as *as,
			caddr_t *addrp, off_t len, u_int prot,
			u_int maxprot, u_int flags, cred_t *credp);

extern int nodev();
extern int nulldev();

struct cb_ops   sx_cb_ops = {
	sx_open,		/* open */
	sx_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	sx_ioctl,		/* ioctl */
	nodev,			/* devmap */
	sx_mmap,		/* mmap */
	sx_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	NULL,			/* streamtab  */
	D_NEW | D_MP  		/* Driver compatibility flag */
};

static int sx_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
						void **result);
static int sx_identify(dev_info_t *dip);
static int sx_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int sx_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

struct dev_ops  sx_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	sx_getinfo,		/* get_dev_info */
	sx_identify,		/* identify */
	nulldev,		/* probe */
	sx_attach,		/* attach */
	sx_detach,		/* detach */
	nodev,			/* reset */
	&sx_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	nulldev			/* power */
};

/*
 * SX segment operations
 */
static	int	segsx_create(struct seg *seg, caddr_t argsp);
static	int	segsx_dup(struct seg *seg, struct seg *newseg);
static	int	segsx_unmap(struct seg *seg, caddr_t addr, size_t len);
static	void	segsx_free(struct seg *seg);
static	faultcode_t	segsx_fault(struct hat *hat, struct seg *seg,
				caddr_t addr, size_t len,
				enum fault_type type, enum seg_rw rw);
static	faultcode_t	segsx_faulta(struct seg *seg, caddr_t addr);
static	int	segsx_setprot(struct seg *seg, caddr_t addr, size_t len,
			u_int prot);
static	int	segsx_checkprot(struct seg *seg, caddr_t addr, size_t len,
			u_int prot);
static	int	segsx_kluster(struct seg *seg, caddr_t addr, ssize_t delta);
static	size_t	segsx_swapout(struct seg *seg);
static	int	segsx_sync(struct seg *seg, caddr_t addr, size_t len,
			int attr, u_int flags);
static	size_t	segsx_incore(struct seg *seg, caddr_t addr, size_t len,
			char *vec);
static	int	segsx_lockop(struct seg *seg, caddr_t addr, size_t len,
			int attr, int op, ulong *bitmap, size_t pos);
static	int	segsx_getprot(struct seg *seg, caddr_t addr, size_t len,
			u_int *protv);
static	u_offset_t segsx_getoffset(struct seg *seg, caddr_t addr);
static	int	segsx_gettype(struct seg *seg, caddr_t addr);
static	int	segsx_getvp(struct seg *seg, caddr_t addr,
			struct vnode **vpp);
static	int	segsx_advise(struct seg *seg, caddr_t addr, size_t len,
			u_int behav);
static	void	segsx_dump(struct seg *seg);
static	int	segsx_pagelock(struct seg *seg, caddr_t addr, size_t len,
			struct page ***ppp, enum lock_type type,
			enum seg_rw rw);
static int	segsx_getmemid(struct seg *seg, caddr_t addr,
			memid_t *memidp);

struct	seg_ops segsx_ops =  {

	segsx_dup,		/* dup */
	segsx_unmap,		/* unmap */
	segsx_free,		/* free */
	segsx_fault,		/* fault */
	segsx_faulta,		/* fault ahead */
	segsx_setprot,		/* setprot */
	segsx_checkprot,	/* checkprot */
	segsx_kluster,		/* kluster */
	segsx_swapout,		/* swapout */
	segsx_sync,		/* sync */
	segsx_incore,		/* incore */
	segsx_lockop,		/* lock_op */
	segsx_getprot,		/* get prot */
	segsx_getoffset,	/* get offset */
	segsx_gettype,		/* get type */
	segsx_getvp,		/* get vnode pointer */
	segsx_advise,		/* advise */
	segsx_dump,		/* dump */
	segsx_pagelock,		/* pagelock */
	segsx_getmemid,		/* getmemid */
};

static int sx_swtches;	/* Statistics. Total # of context switches */

static sx_cntxtinfo_t *sx_cntxt_tbl; /* SX context table */
static u_int sx_usr_pf;	/* For user mappings to SX register set */
static sx_proc_t	*sx_current_procp;
extern volatile struct sx_register_address *sxregsp;  /* SX register set */
static volatile struct sx_register_address *sxuregsp; /* User SX register set */
static void *sx_aflt_cookie;	/* Cookie for use with aflt_add_handler */

extern struct hatops sx_mmu_hatops;

#define	SX_GET_CNTXTINFO(cntxtnum) ((sx_cntxt_tbl + cntxtnum))

/*
 * Variables to control SX fault handling and cacheability of
 * original mappings.
 */

static int sx_fault_ahead = 0;	/* By default disable fault aheads for */
				/* pageable memory */
static int sx_fault_ahead_npages = 16; /* By default fault ahead 16 pages */

/*
 * By default,  unload virtual to physical address translations for CPU
 * accesses when the corresponding cloned SX address range is unmapped.
 * This behaviour is changed when the SXlib issues the cache control ioctl,
 * after which the default (sticky behaviour) is to not unload the virtual
 * to physical address translations for the original mappings. The variable
 * sx_unload_orig_xlations controls the performance of applications using
 * SX/XIL. If it is 1 the performance is bad. The default is 1 to guarantee
 * the (poor performance) behaviour of earlier releases of SXlib and SX/XIL.
 */


static int sx_unload_orig_xlations =  1;

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module. */
	"Device/Segment driver for SX V1.127",
	&sx_ops	/* devops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
#if defined(DEBUG)
	char version[] = "1.127";
#endif

#if defined(DEBUG)
	cmn_err(CE_CONT, "?SX driver V%s loaded.\n", version);
#endif

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

static int
sx_identify(dev_info_t *devi)
{
	if ((strcmp(ddi_get_name(devi), "SUNW,sx") == 0) ||
			(strcmp(ddi_get_name(devi), "sx") == 0))
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
sx_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	static char name[16];
	int instance;

	switch (cmd) {

	case DDI_ATTACH:
		sx_cntxt_tbl = NULL;
		sxregsp = NULL;
		sxuregsp = NULL;
		sx_aflt_cookie = NULL;

		instance = ddi_get_instance(devi);

		{
			ddi_iblock_cookie_t ic;

			if (aflt_get_iblock_cookie(devi, AFLT_SX, &ic) !=
			    AFLT_SUCCESS) {
				return (DDI_FAILURE);
			}

			mutex_init(&sx_aflt_mutex, NULL, MUTEX_DRIVER, ic);
		}
		mutex_init(&sx_hw_mutex, NULL, MUTEX_DRIVER, NULL);
		mutex_init(&sx_cntxt_mutex, NULL, MUTEX_DRIVER, NULL);
		mutex_init(&sx_tr_lock, NULL, MUTEX_DRIVER, NULL);

		sx_cntxt_tbl = (sx_cntxtinfo_t *)kmem_zalloc(
		    sizeof (sx_cntxtinfo_t) * SX_MAXCNTXTS, KM_NOSLEEP);
		if (sx_cntxt_tbl == NULL) {
			cmn_err(CE_WARN, "Can`t alloc mem for SX cntxts\n");
			goto failed;
		}

		sx_dip = devi;

		(void) sprintf(name, "sx%d", instance);
		if (ddi_create_minor_node(devi, name, S_IFCHR, instance,
		    NULL, NULL) == DDI_FAILURE) {
			cmn_err(CE_WARN, "Could not create %s", name);
			goto failed;
		}

		/* Get SX properties, map in regs etc. */

		if (sx_setupinfo(devi) == -1)
			goto failed;

		/*
		 * Initialize the pointer to the callback routine to be used
		 * to notify when address ranges cloned for SX are being
		 * unmapped.
		 */

		sx_mmu_unmap_callback = sx_orig_unmap;

		ddi_report_dev(devi);

		return (DDI_SUCCESS);
	case DDI_RESUME:
		SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
		SX_HW_LOCK_ENTER(&sx_hw_mutex);

		if (sx_current_procp) {
			sxregsp->s_r0_init = 0;
			sx_regrestore(sx_current_procp->spp_cntxtp);
		} else {
			/* redo register initialization */
			sx_init();
		}

		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

failed:
	/*
	 * Cleanup... this is safe since detach knows what can be cleaned
	 * up and what hasn't yet been initialized.
	 */
	(void) sx_detach(devi, DDI_DETACH);

	return (DDI_FAILURE);
}

static int
sx_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {

	case DDI_DETACH:
		/*
		 * This has to check before cleaning up since a failed
		 * attach comes through here.
		 */

		ddi_remove_minor_node(devi, NULL);

		if (sx_aflt_cookie != NULL) {
			(void) aflt_remove_handler(sx_aflt_cookie);
		}

		if (sxregsp != NULL) {
			ddi_unmap_regs(devi, 0, (caddr_t *)&sxregsp, 0,
			    (off_t)SX_PGSIZE);
		}
		if (sxuregsp != NULL) {
			ddi_unmap_regs(devi, 1, (caddr_t *)&sxuregsp, 0,
			    (off_t)SX_PGSIZE);
		}

		if (sx_cntxt_tbl != NULL) {
			kmem_free(sx_cntxt_tbl, sizeof (sx_cntxtinfo_t) *
			    SX_MAXCNTXTS);
		}

		/* no failure points in attach before these are initialized */
		SX_AFLT_LOCK_DESTROY(&sx_aflt_mutex);
		SX_HW_LOCK_DESTROY(&sx_hw_mutex);
		SX_CNTXT_LOCK_DESTROY(&sx_cntxt_mutex);
		mutex_destroy(&sx_tr_lock);

		return (DDI_SUCCESS);

	case DDI_SUSPEND:

		SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
		SX_HW_LOCK_ENTER(&sx_hw_mutex);

		if (sx_current_procp)
			(void) sx_regsave(sx_current_procp->spp_cntxtp);

		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
sx_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = sx_dip;
		error = DDI_SUCCESS;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;

	default:
		break;
	}

	return (error);
}

/*
 * Each process requesting graphics acceleration (i.e SX services) has to
 * open /dev/sx to establish an SX context. The driver clones each
 * open to provide redirection at the vnode level and provides  upto
 * SX_MAXCNTXTS SPAM contexts. Only one SX context is allowed per process.
 */

/*ARGSUSED*/
static int
sx_open(dev_t *dev_p, int flag, int otyp, cred_t *cr)
{
#ifdef SX_DEBUG
	int my_sx_opens = 0, my_sx_closes = 0, my_sx_other = 0;
#endif /* SX_DEBUG */
	dev_t dev = *dev_p;
	sx_cntxtinfo_t *cntxtinfop;
	int i;


	if (otyp != OTYP_CHR)
		return (EINVAL);

	/*
	 * Find an unused entry in the context table and clone the
	 * minor device number.
	 */
	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
	/*
	 * Check if this process already has opened the SX device and
	 * established a SX context.
	 */
	if (sx_vrfy_cntxt_setup() == B_TRUE) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		return (EINVAL);
	}
	cntxtinfop = sx_cntxt_tbl;
	for (i = 0; i < SX_MAXCNTXTS; i++) {

#if SX_DEBUG
		switch (cntxtinfop->spc_state) {
		case SX_SPC_OPEN:
			my_sx_opens++;
			break;
		case SX_SPC_CLOSE:
			my_sx_closes++;
			break;
		default:
			my_sx_other++;
			break;
		}
#endif /* SX_DEBUG */

		if (cntxtinfop->spc_state == SX_SPC_FREE) {
			cntxtinfop->spc_state = SX_SPC_OPEN;
			*dev_p = makedevice(getmajor(dev), i);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (0);
		}
		cntxtinfop++;
	}
#if SX_DEBUG
	cmn_err(CE_CONT, "open =%d, close = %d, other = %d\n", my_sx_opens,
		my_sx_closes, my_sx_other);
#endif /* SX_DEBUG */

	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (ENOMEM);	/* Context table full */
}

/*ARGSUSED*/
static int
sx_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	struct sx_cntxtinfo *cntxtinfop;
	int cntxtnum = SX_CLNUNIT(getminor(dev));
	boolean_t mutex_acquired = B_FALSE;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if (cntxtnum >= SX_MAXCNTXTS)
		return (ENXIO);

	/*
	 * Get the SX context information associated with this minor
	 * device number.
	 */
	if (!mutex_owned(&sx_cntxt_mutex)) {
		SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
		mutex_acquired = B_TRUE;
	}
	cntxtinfop = SX_GET_CNTXTINFO(cntxtnum);

	/*
	 * If there are no mappings corresponding to this file descriptor
	 * mark the slot in the context table so it can be used again. If there
	 * are mappings corresponding this file descriptor, then set the
	 * state to closed. The segment driver unmap routine will do the
	 * the cleanup work to mark the slot as free when all the
	 * mappings  are blown away.
	 */
	if (cntxtinfop->spc_sxprocp == (struct sx_proc *)0) {

		/*
		 * No contexts associated with this entry, mark it free
		 */
		cntxtinfop->spc_state = SX_SPC_FREE;

	} else { /* Mark the entry as closed */

		cntxtinfop->spc_state = SX_SPC_CLOSE;
	}
	if (mutex_acquired) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	}
	return (0);
}

/*ARGSUSED*/
int
sx_ioctl(dev_t dev, int cmd, intptr_t arg,
	int mode, cred_t *cred_p, int *rval_p)
{
	int error = 0;
	int cmdarg;
	int cntxtnum;
	sx_proc_t *p;
	struct sx_set_pgbnds sx_pgbnds;
	struct sx_cachectrl sx_cachectrl;
	struct seg *seg;
	u_int pf, lo_pf, hi_pf;
	struct as *as;
	struct sx_seg_data *sdp;
	caddr_t offset, orig_vaddr_start, orig_vaddr_end;
	caddr_t a, eaddr;
	struct page *pp;

	switch (cmd) {

	case SX_SET_DIAG_MODE:

		/*
		 * Sets only the low order 5 bits of the diag register
		 * Restrict execution of this command to superuser only
		 */
		cmdarg = arg & SX_DIAG_MASK;
		if (drv_priv(cred_p) == 0) {
			SX_HW_LOCK_ENTER(&sx_hw_mutex);
			sxregsp->s_diagreg = cmdarg;
			SX_HW_LOCK_EXIT(&sx_hw_mutex);
		} else
			return (EPERM);
		break;

	case SX_SETCLR_GO:

		/*
		 * Set SX GO bit. Restrict execution of this command to
		 * super user.
		 */
		if (drv_priv(cred_p) == 0) {
			if (arg == SX_SET_GO) {
				SX_HW_LOCK_ENTER(&sx_hw_mutex);
				sxregsp->s_csr |= SX_GO;
				SX_HW_LOCK_EXIT(&sx_hw_mutex);
			} else if (arg == SX_CLEAR_GO) {
				SX_HW_LOCK_ENTER(&sx_hw_mutex);
				sxregsp->s_csr &= ~SX_GO;
				SX_HW_LOCK_EXIT(&sx_hw_mutex);
			} else
				error = EINVAL;
		}
		else
			error = EPERM;
		break;

	case SX_SET_ERR_REG:
		/*
		 * Load the SX error register
		 */
		if (drv_priv(cred_p) == 0) {
			SX_HW_LOCK_ENTER(&sx_hw_mutex);
			sxregsp->s_ser = arg;
			SX_HW_LOCK_EXIT(&sx_hw_mutex);
		} else
			return (EPERM);
		break;

		/*
		 * Load the SX page bounds registers and enable
		 * SX extended bounds checking.
		 */
	case SX_SET_PGBNDS:

		if (copyin((caddr_t)arg, &sx_pgbnds,
		    sizeof (struct sx_set_pgbnds)) != 0) {
			return (EFAULT);
		}
		cntxtnum = SX_CLNUNIT(getminor(dev));
		as = curproc->p_as;

		/* Find the segment mapping the given virtual address */

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		if ((seg = as_segat(as, (caddr_t)sx_pgbnds.sp_vaddr)) ==
		    (struct seg *)0) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return (EINVAL);
		}
		/*
		 * Check if the SX segment driver maps the address given to
		 * us.
		 */
		if (seg->s_ops != &segsx_ops) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return (EINVAL);
		}

		/* Make sure that specified range is mapped in */

		if ((seg->s_base + seg->s_size) < (caddr_t)
				(sx_pgbnds.sp_vaddr + sx_pgbnds.sp_len)) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return (EINVAL);
		}

		/* Find corresponding range in the orig space (cmem area) */

		sdp = (struct sx_seg_data *)seg->s_data;
		offset = (caddr_t)(sx_pgbnds.sp_vaddr - (u_int)seg->s_base);
		orig_vaddr_start = sdp->sd_orig_vaddr + (u_int)offset;
		orig_vaddr_end = orig_vaddr_start + sx_pgbnds.sp_len -
					PAGESIZE;

		/*
		 * Fault in the low and high bound addresses to get the
		 * page frame numbers.
		 */
		if ((lo_pf = sx_getpfnum(seg->s_as, orig_vaddr_start)) == -1) {

			AS_LOCK_EXIT(as, &as->a_lock);
			(void) as_fault(seg->s_as->a_hat, seg->s_as,
			    orig_vaddr_start, PAGESIZE, F_INVAL, S_READ);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);

			if ((lo_pf = sx_getpfnum(seg->s_as,
					orig_vaddr_start)) == -1) {
				AS_LOCK_EXIT(as, &as->a_lock);
				return (EINVAL);
			}
		}

		/*
		 * Get the pageframe number to load the high bounds register
		 */
		if ((hi_pf = sx_getpfnum(seg->s_as, orig_vaddr_end)) == -1) {

			AS_LOCK_EXIT(as, &as->a_lock);
			(void) as_fault(seg->s_as->a_hat, seg->s_as,
			    orig_vaddr_end, PAGESIZE, F_INVAL, S_READ);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);

			if ((hi_pf = sx_getpfnum(seg->s_as,
					orig_vaddr_end)) == -1) {
				AS_LOCK_EXIT(as, &as->a_lock);
				return (EINVAL);
			}
		}

		/*
		 * Check page frames to see if they are in
		 * sx_cmem range:  if not, return error
		 */
		if (sx_vrfy_pfn(lo_pf, hi_pf) == NULL) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return (EINVAL);
		}

		SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
		if ((p = sx_getproc(cntxtnum, as)) == (struct sx_proc *)0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			AS_LOCK_EXIT(as, &as->a_lock);
			return (EINVAL);
		}
		/*
		 * The high order 4 bits of the 24-bit physical page frame
		 * number indicate the physical address space. What we need
		 * is 32 bit physical address.
		 *
		 * Finally, enable extended page bounds checking and load
		 * the page bounds registers
		 */
		if (p == sx_current_procp) {
			SX_HW_LOCK_ENTER(&sx_hw_mutex);
			sxregsp->s_pg_lo = lo_pf << MMU_STD_PAGESHIFT;
			sxregsp->s_pg_hi = hi_pf << MMU_STD_PAGESHIFT;
			sxregsp->s_csr |= SX_PB;
			SX_HW_LOCK_EXIT(&sx_hw_mutex);
		} else {  /* Apply changes in the context save area */
			p->spp_cntxtp->spc_pg_lo = lo_pf << MMU_STD_PAGESHIFT;
			p->spp_cntxtp->spc_pg_hi = hi_pf << MMU_STD_PAGESHIFT;
			p->spp_cntxtp->spc_csr |= SX_PB;
		}
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		AS_LOCK_EXIT(as, &as->a_lock);
		break;

	case SX_CACHE_CTRL:

		/*
		 * Turn off the default behaviour of unloading
		 * virtual to physical address translations to memory when
		 * the corresponding SX mapping is unmapped (in segsx_unmap).
		 * the use of this command is a hint that the SXlib wants to
		 * to control the cacheability of memory.
		 */
		if (sx_unload_orig_xlations)
			sx_unload_orig_xlations = 0;

		if (copyin((caddr_t)arg, &sx_cachectrl,
		    sizeof (struct sx_cachectrl)) != 0) {
			return (EFAULT);
		}
		as = curproc->p_as;

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);

		a = (caddr_t)sx_cachectrl.sc_orig_vaddr;
		eaddr = a + sx_cachectrl.sc_len;

		if (sx_cachectrl.sc_cmd == SX_DONE_WITHSX) {

			SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
			for (; a < eaddr; a += PAGESIZE) {

				if ((pf = hat_getpfnum(as->a_hat, a)) == -1)
					continue;

				if (sx_vrfy_pfn(pf, pf) != NULL)
					continue; /* Ignore cmem for now */

				if ((pp = page_numtopp(pf, SE_SHARED)) == NULL)
					continue;
				/*
				 * If an SX MMU mapping to this page exists,
				 * then this page cannot be marked as cached
				 */

				if (sx_mmu_vrfy_sxpp(pp) != 0) {
					page_unlock(pp);
					continue;
				}
				/*
				 * There is a valid translation for the addr.
				 * Now mark it as cached.
				 */
				hat_pagecachectl(pp, HAT_CACHE);
				page_unlock(pp);
			}
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		} else if (sx_cachectrl.sc_cmd == SX_PREP_FORSX) {
			for (; a < eaddr; a += PAGESIZE) {

				if ((pf = hat_getpfnum(as->a_hat, a)) == -1)
					continue;

				if (sx_vrfy_pfn(pf, pf) != NULL)
					continue; /* Ignore cmem for now */

				if ((pp = page_numtopp(pf, SE_SHARED)) == NULL)
					continue;
				/*
				 * There is a valid translation for the addr.
				 * Now mark it as uncached.
				 */
				hat_pagecachectl(pp, HAT_UNCACHE);
				page_unlock(pp);
			}
		} else {
			error = ENOTTY;
		}
		AS_LOCK_EXIT(as, &as->a_lock);
		break;

	case SX_VALID_VA:

		error = sx_vrfy_va((struct sx_valid_va *)arg);
		break;

	case SX_GET_ORIGINAL_VA:
		error = sx_get_orig_va((struct sx_original_va *)arg);
		break;

	default:
		error = ENOTTY;

	}
	return (error);
}


/*ARGSUSED*/
static int
sx_mmap(dev_t dev, off_t off, int prot)
{
	return (0);

}

/*
 * Routine to provide mappings to SX control address space and accelerated
 * mappings to DRAM or VRAM. The actual translation itself is set up during
 * fault handling.
 */

/*ARGSUSED*/
int
sx_segmap(dev_t dev, off_t offset, struct as *as, caddr_t *addr,
	off_t len, u_int prot, u_int maxprot, u_int flags, cred_t *cred)
{
	struct sx_seg_data seg_priv_data, *dp;
	struct seg *seg, *orig_seg;
	sx_proc_t *p = (struct sx_proc *)0;
	int cntxtnum, error = 0;
	struct vnode *vp = (struct vnode *)0;
	u_int pf;
	extern int pf_is_video(u_int);

	cntxtnum = SX_CLNUNIT(getminor(dev));
	len = roundup(len, PAGESIZE);

	/*
	 * Initialize the segment private data.
	 */

	seg_priv_data.sd_prot = prot;
	seg_priv_data.sd_maxprot = maxprot;
	seg_priv_data.sd_objtype = 0;
	seg_priv_data.sd_orig_vaddr = 0;
	seg_priv_data.sd_vp = (struct vnode *)NULL;
	seg_priv_data.sd_dev = dev;
	seg_priv_data.sd_cntxtnum = (u_short)cntxtnum;

	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);

	switch (offset) {

	case SX_REG_MAP:	  /* SX register set	*/

		if (len != SX_PGSIZE) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}
		/*
		 * If mapping already exists return the same
		 * address.
		 */
		if ((p = sx_getproc(cntxtnum, as)) != (struct sx_proc *)0) {
			/*
			 * Grab the reader's lock for this address space
			 * and then search the list of segments
			 */
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
			    seg = AS_SEGP(as, seg->s_next)) {
				if (seg->s_ops != &segsx_ops)
					continue;
				dp = (struct sx_seg_data *)seg->s_data;
				if ((dp->sd_orig_vaddr == NULL) &&
				    (dp->sd_objtype == SX_REG_MAP)) {
					*addr = seg->s_base;
					AS_LOCK_EXIT(as, &as->a_lock);
					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (0);
				}
			}
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}

		/*
		 * Link the process onto the context table.
		 */
		if ((p = sx_addproc(cntxtnum, as)) == (struct sx_proc *)0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}
		/* Allocate a SX HAT for this address space */

		AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
		if ((p->spp_hat = ohat_alloc(as, &sx_mmu_hatops)) == NULL) {
			cmn_err(CE_CONT, "SX HAT Allocation failed\n");
			AS_LOCK_EXIT(as, &as->a_lock);
			sx_freeproc(cntxtnum, p);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}
		AS_LOCK_EXIT(as, &as->a_lock);
		ASSERT(p->spp_hat->hat_op == &sx_mmu_hatops);
		/*
		 * Intialize a SX context
		 */
		sx_cntxtinit(p->spp_cntxtp);
		seg_priv_data.sd_objtype = SX_REG_MAP;
		break;

	case SX_PRIV_REG_MAP:

		if (len != SX_PGSIZE) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}
		if (drv_priv(cred) != 0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EPERM);
		}
		if ((p = sx_getproc(cntxtnum, as)) == (struct sx_proc *)0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENXIO);
		}
		if (p->spp_privflag) {
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
			    seg = AS_SEGP(as, seg->s_next)) {
				if (seg->s_ops != &segsx_ops)
					continue;
				dp = (struct sx_seg_data *)seg->s_data;
				if ((dp->sd_orig_vaddr == NULL) &&
				    (dp->sd_objtype == SX_PRIV_REG_MAP)) {
					*addr = seg->s_base;
					AS_LOCK_EXIT(as, &as->a_lock);
					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (0);
				}
			}
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENXIO);
		}
		seg_priv_data.sd_objtype = SX_PRIV_REG_MAP;
		p->spp_privflag = B_TRUE;
		break;

	default:
		/*
		 * Requesting SX acclerated mapping to D[V]RAM
		 * Get the segment mapping the virtual address passed in
		 * the 'offset' parameter to mmap(2). A process must map
		 * the SX register set before requesting a mapping to
		 * the D[V]RAM.
		 */
		if (len == 0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);

		/*
		 * Assume that this is a request to obtain an SX mapping to
		 * VRAM. If it is not we change it (below or in the fault
		 * handling routine) to indicate the type of memory we are
		 * rendering to.
		 */
		seg_priv_data.sd_objtype = SX_VRAM_MAP;

		if ((p = sx_getproc(cntxtnum, as)) == (struct sx_proc *)0) {
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}
		/*
		 * Get the segment that maps the original virtual address
		 * range.
		 */
		if ((orig_seg = as_segat(as, (caddr_t)offset)) ==
		    (struct seg *)0) {
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}

		if (orig_seg->s_ops == &segsx_ops) {
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}
		/*
		 * Probe the MMU to find out the system address space to which
		 * an SX mapping is requested. SX can only access main memory
		 * address space. I/O adress spaces are illegal.
		 */

		if ((pf = hat_getpfnum(as->a_hat, (caddr_t)offset)) == -1) {
			AS_LOCK_EXIT(as, &as->a_lock);
			(void) as_fault(as->a_hat, as, (caddr_t)offset,
				PAGESIZE, F_INVAL, S_READ);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			/*
			 * Probe again to get the page frame number
			 */
			if ((pf = hat_getpfnum(as->a_hat, (caddr_t)offset))
			    == -1) {
				AS_LOCK_EXIT(as, &as->a_lock);
				SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
				return (EINVAL);
			}
		}
		/*
		 * If page frame number indicates that it is not in the
		 * main memory address space, then we must fail the mmap
		 * request.
		 */
		if ((!pf_is_memory(pf)) && (!pf_is_video(pf))) {
			AS_LOCK_EXIT(as, &as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (EINVAL);
		}

		/* Get the vnode from the segops  */
		SEGOP_GETVP(orig_seg, (caddr_t)offset, &vp);
		AS_LOCK_EXIT(as, &as->a_lock);

		/*
		 *  If the user wants us to accelerate onto a regular vnode,
		 *  then first check to make sure we have write permission
		 *  on the vnode.  Otherwise, we could render into /etc/passwd!
		 */
		if (vp != NULL && vp->v_type == VREG) {
			if ((error = VOP_ACCESS(vp, VWRITE, 0, cred)) != 0) {
				SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
				return (error);
			}
		}
		seg_priv_data.sd_orig_vaddr = (caddr_t)offset;
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {

		/*
		 * Pick an address
		 */
		map_addr(addr, len, (offset_t)0, 0, flags);
		if (*addr == NULL) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User specified address -
		 * Blow away any previous mappings.
		 */
		(void) as_unmap((struct as *)as, *addr, len);
	}

	error = as_map((struct as *)as, *addr, len, segsx_create,
			&seg_priv_data);
	as_rangeunlock(as);

	if (error != 0) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		return (ENOMEM);
	}

	/*
	 * Add the SPAMified virtual address range we are mapping to the
	 * per -address space list of SPAMified virtual address ranges
	 * maintained in the SX hat private data area.
	 */
	if ((offset != SX_REG_MAP) && (offset != SX_PRIV_REG_MAP))
		sx_mmu_add_vaddr(as, (caddr_t)offset, *addr, len);

	p->spp_segcnt++;  /* Bump up reference count of SX segments */
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (error);
}

/*
 * Initialize segment segops and private data fields. The segment provides a
 * mapping to SX register set or the mappings to D[V]RAM for SX
 * memory reference instructions.
 */
static int
segsx_create(struct seg *seg, caddr_t argsp)
{
	struct sx_seg_data *sdp;
	int error = 0;
	extern struct vnode *specfind(), *common_specvp();

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	sdp = (struct sx_seg_data *)
		kmem_zalloc(sizeof (struct sx_seg_data), KM_SLEEP);

	*sdp = *((struct sx_seg_data *)argsp); /* structure copy */

	seg->s_ops = &segsx_ops;
	seg->s_data = (caddr_t)sdp;

	/*
	 *  Find the shadow vnode.  It will be held by specfind().
	 *  Holding the shadow causes the common to be held.
	 */
	sdp->sd_vp = specfind(sdp->sd_dev, VCHR);
	ASSERT(sdp->sd_vp != NULL);

	/*
	 *  Inform the vnode of the new mapping
	 */
	if ((error = VOP_ADDMAP(common_specvp(sdp->sd_vp), (offset_t)0,
			seg->s_as, seg->s_base, seg->s_size,
			sdp->sd_prot, sdp->sd_maxprot, MAP_SHARED,
			CRED())) != 0) {

		kmem_free(sdp, sizeof (struct sx_seg_data));
	}
	return (error);
}

/*
 * Duplicate seg and return new segment in newsegp.
 */
static int
segsx_dup(struct seg *seg, struct seg *newseg)
{
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;
	struct sx_proc *p, *q;
	int error = 0;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
	if ((q = sx_getproc(sdp->sd_cntxtnum, seg->s_as)) ==
	    (struct sx_proc *)0) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		cmn_err(CE_PANIC, "segsx_dup(): Parent SX proc not found\n");
	}

	/*
	 * If this is the first SX mapped segment of the process we
	 * are duplicating, then this process must be added onto the
	 * linked list of SX processes sharing the same context number.
	 */
	if ((p = sx_getproc(sdp->sd_cntxtnum, newseg->s_as)) ==
	    (struct sx_proc *)0) {
		if ((p = sx_addproc(sdp->sd_cntxtnum, newseg->s_as)) ==
		    (struct sx_proc *)0) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}

		/* Allocate a SX HAT for this address space */
		if ((p->spp_hat = ohat_alloc(newseg->s_as,
		    &sx_mmu_hatops)) == NULL) {
			cmn_err(CE_CONT, "sx_dup(): SX HAT Alloc failed\n");
			sx_freeproc(sdp->sd_cntxtnum, p);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			return (ENOMEM);
		}
	}
	if ((error = segsx_create(newseg, (caddr_t)sdp)) != 0) {
		cmn_err(CE_WARN, "could not dup segsx.\n");

		ohat_free(p->spp_hat, newseg->s_as);
		sx_freeproc(sdp->sd_cntxtnum, p);
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		return (error);
	}

	/*
	 * Add the <orig_vaddr, sx_vaddr, len> information to the
	 * per address space HAT
	 */

	if (sdp->sd_orig_vaddr != NULL)
		sx_mmu_add_vaddr(newseg->s_as, sdp->sd_orig_vaddr,
			newseg->s_base, newseg->s_size);

	/*
	 * If duplicating a mapping to the SX register set, copy the
	 * parent process's registers to the child's context save area.
	 */
	if (sdp->sd_objtype == SX_REG_MAP) {
		SX_HW_LOCK_ENTER(&sx_hw_mutex);
		sx_cntxtcopy(q, p);
		SX_HW_LOCK_EXIT(&sx_hw_mutex);
	}

	p->spp_segcnt++;
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (0);
}

static int
segsx_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	sx_proc_t *sx_procp;
	struct sx_seg_data *sdp;
	extern struct vnode *common_specvp();

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));
	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);

	sdp = (struct sx_seg_data *)seg->s_data;

	if ((sx_procp = sx_getproc(sdp->sd_cntxtnum, seg->s_as)) == NULL) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		cmn_err(CE_PANIC, "segsx_unmap() Can't find SX context\n");
	}

	/*
	 * Check for bad sizes. Partial unmaps are not allowed!
	 */
	if (addr < seg->s_base || addr + len != seg->s_base + seg->s_size ||
	    (len & PAGEOFFSET) || ((u_int)addr & PAGEOFFSET)) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		return (-1);
	}

	if (sdp->sd_orig_vaddr != NULL) {

		/*
		 * Delete the <original virtual address, len> information
		 * maintained by the SX HAT for each address space.
		 */
		sx_mmu_del_vaddr(seg->s_as, sdp->sd_orig_vaddr,
			seg->s_base, seg->s_size, ORIG_ADDR);
		/*
		 * Unload virtual to physical address translations, only
		 * if the underlying memory is pageable and the flag
		 * sx_unload_sx_orig_xlations is true.
		 */
		if ((sx_unload_orig_xlations) &&
		    (sdp->sd_objtype == SX_PGBLE_MAP)) {
			hat_unload(seg->s_as->a_hat, sdp->sd_orig_vaddr, len,
				HAT_UNLOAD);
		}
	}

	/*
	 * Unload SX translations in the range being unmapped.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	sx_procp->spp_rss -= mmu_btopr(len);

	ASSERT(sdp->sd_vp != NULL);

	if (sdp->sd_orig_vaddr != NULL) {
		/*
		 * Delete the <SX virtual address, len> information
		 * maintained by the SX HAT for each address space.
		 */
		sx_mmu_del_vaddr(seg->s_as, sdp->sd_orig_vaddr,
			seg->s_base, seg->s_size, SX_ADDR);
	}

	/*
	 *  Inform the vnode of the unmapping.
	 */
	VOP_DELMAP(common_specvp(sdp->sd_vp),
	    (offset_t)0, seg->s_as, addr, len,
	    sdp->sd_prot, sdp->sd_maxprot, MAP_SHARED, CRED());

	seg_free(seg);
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (0);
}

static void
segsx_free(struct seg *seg)
{
	struct hat *sx_hat;
	sx_proc_t *sx_procp;
	struct sx_cntxtinfo *cntxtinfop;
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	cntxtinfop = SX_GET_CNTXTINFO(sdp->sd_cntxtnum);

	if ((sx_procp = sx_getproc(sdp->sd_cntxtnum, seg->s_as)) == NULL)
		cmn_err(CE_PANIC, "segsx_free(): sx_getproc returns NULL");

	if (sdp->sd_objtype == SX_PRIV_REG_MAP)
		sx_procp->spp_privflag = B_FALSE;

	/*
	 * If the reference count of the number of SX segments goes
	 * to zero. We free up the data associated with SX process..
	 */
	if (--sx_procp->spp_segcnt == 0) {
		sx_hat = seg->s_as->a_hat;
		for (; sx_hat != NULL; sx_hat = sx_hat->hat_next) {
			if (sx_hat == sx_procp->spp_hat) {
				ohat_free(sx_procp->spp_hat, seg->s_as);
				break;
			}
		}
		sx_freeproc(sdp->sd_cntxtnum, sx_procp);
	}
	VN_RELE(sdp->sd_vp);	/* release the shadow vnode we hold */

	/*
	 * If this is the last segment of the last process and the last close
	 * on the device has already been done then mark the entry
	 * in the context table as free to be reallocated to another context.
	 */
	if (cntxtinfop->spc_sxprocp == (struct sx_proc *)0) {
		if (cntxtinfop->spc_state == SX_SPC_CLOSE)
			cntxtinfop->spc_state = SX_SPC_FREE;
	}

	/*
	 * Free up the segment private data we have allocated
	 */
	kmem_free(seg->s_data, sizeof (struct sx_seg_data));
	seg->s_data = NULL;
}

/*
 * Handle a fault for SX register set, mappings to D[V]RAM for SX memory
 * reference instructions. A SX context switch is necessary if the fault is
 * on a mapping that is not part of the current context.
 */

/*
 * sx_lastsched is the next available "appointment" to use the sx
 * hardware mapping, in ticks since boot.
 */
int sx_lastsched;
/*
 * This value was experimentally determined to give a good tradeoff between
 * sx context-switching overhead and responsiveness.
 */
#define	SX_QUANTUM	(hz / 25)

/*ARGSUSED*/
static faultcode_t
segsx_fault(struct hat *hat, struct seg *seg, caddr_t addr,
	size_t len, enum fault_type type, enum seg_rw rw)
{
	struct sx_proc *sx_procp;
	struct seg_ops *orig_segops;
	struct seg *orig_seg;
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;
	caddr_t sx_vaddr, orig_vaddr;
	u_int  prot;
	u_int pf;
	struct hat *sx_hat;
	struct devpage *dp = NULL;
	struct as *old_as;

	int dt = 0;		/* Delay time for "appointment" */
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(hat != NULL);

	if (type != F_INVAL) {
		return (FC_MAKE_ERR(EFAULT));
	}

	switch (rw) {

	case S_READ:
		prot = PROT_READ;
		break;

	case S_WRITE:
		prot = PROT_WRITE;
		break;

	case S_EXEC:
		prot = PROT_EXEC;
		break;

	case S_OTHER:

	default:

		prot = PROT_READ | PROT_WRITE;
		break;
	}
	if ((sdp->sd_prot & prot) == 0) {
		return (FC_MAKE_ERR(EACCES));
	}

	/* Grab the context switching lock.  */
top:
	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
	if ((sx_procp = sx_getproc(sdp->sd_cntxtnum, seg->s_as)) ==
	    (struct sx_proc *)0) {
		cmn_err(CE_PANIC, "segsx_fault: sx_getproc returns NULL");
	}

	/*
	 * Check if there is no currently active SX context.
	 */
	if (sx_current_procp == (struct sx_proc *)0) {

		SX_HW_LOCK_ENTER(&sx_hw_mutex);
		sx_regrestore(sx_procp->spp_cntxtp);
		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		sx_current_procp = sx_procp;

	} else if (sx_procp != sx_current_procp) { /* Different process */
		/*
		 * Try to get the reader's lock on the address space of the
		 * SX process whose context we are switching out.
		 */
		if (sx_lastsched - lbolt > 0 && !dt) {
			dt = sx_lastsched - lbolt;
			sx_lastsched += SX_QUANTUM;
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			delay(dt);
			goto top;
		}

		old_as = sx_current_procp->spp_as;
		if (!rw_tryenter(&old_as->a_lock, RW_READER)) {
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			delay(hz /10);	/* Wait 1/10 of a second */
			goto top;
		}

		SX_HW_LOCK_ENTER(&sx_hw_mutex);
		if ((sx_cntxtswtch(sx_procp)) == SX_REGSAVE_FAIL) {
			SX_HW_LOCK_EXIT(&sx_hw_mutex);
			AS_LOCK_EXIT(old_as, &old_as->a_lock);
			SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
			delay(hz / 50); /* Wait 1/50 second */
			goto top;
		}

		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		AS_LOCK_EXIT(old_as, &old_as->a_lock);
	}

	/* Set end of quantum when mapping was not busy */
	if (sx_lastsched - lbolt <= 0)
		sx_lastsched = lbolt + SX_QUANTUM;

	switch (sdp->sd_objtype) {

	case SX_REG_MAP:	/* Fault on mapping to SX register set */

		hat_devload(hat, addr, MMU_PAGESIZE, sx_usr_pf,
		    sdp->sd_prot, HAT_LOAD);
		break;

	case SX_PRIV_REG_MAP:	/* Fault on privileged mapping to SX */
				/* register set */
		pf = hat_getkpfnum((caddr_t)sxregsp);
		hat_devload(hat, addr, MMU_PAGESIZE,
		    pf, sdp->sd_prot, HAT_LOAD);
		break;

	default:	/* Fault on mapping to D[V]RAM */

		/*
		 * Fault in the original address and get the page frame
		 * number. Modify the high order 4-bits of the page frame
		 * number for settin up SX translations.
		 */

		/*
		 * 'a' is the SPAMified virtual address and
		 * 'b' is the corresponding original virtual address
		 */
		sx_hat = sx_procp->spp_hat;
		ASSERT(sx_hat != NULL);
		sx_vaddr = addr;
		orig_vaddr = sdp->sd_orig_vaddr + (addr - seg->s_base);
		for (; sx_vaddr < addr + len;
			sx_vaddr += PAGESIZE, orig_vaddr += PAGESIZE) {

			/*
			 * Probe the MMU to get the pageframe number for
			 * the original virtual address.
			 */
			if ((pf = hat_getpfnum(seg->s_as->a_hat, orig_vaddr))
			    == -1) {
				/*
				 * Probe failed. Load the original translation
				 * and then get the page frame number. To do
				 * this we need the corresponding original
				 * virtual address, the segment mapping the
				 * original virtual address and the original
				 * segment's segment operations vector which
				 * we stole.
				 */
				if ((orig_seg = as_segat(seg->s_as,
							orig_vaddr)) ==
				    (struct seg *)NULL) {
					cmn_err(CE_PANIC, SX_FAULT_MSG1);
				}

				orig_segops = orig_seg->s_ops;
				if ((*orig_segops->fault)(hat, orig_seg,
				    orig_vaddr, PAGESIZE, type, rw) != 0) {
					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (FC_MAKE_ERR(EFAULT));
				}

				if ((pf = hat_getpfnum(seg->s_as->a_hat,
						orig_vaddr)) == -1) {
					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (FC_MAKE_ERR(EFAULT));
				}
			}
			if (sx_vrfy_pfn(pf, pf)) {
				/*
				 * Set up translations to cmem.
				 * Load the entire segment now
				 */
#ifdef DEBUG
				dp = (struct devpage *)page_numtopp_nolock(pf);
				ASSERT(dp != NULL);
#endif /* DEBUG */

				if (sx_fault_cmem(hat, seg->s_as,
				    seg->s_base, sdp->sd_orig_vaddr,
				    seg->s_size, sdp->sd_prot) == -1) {

					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (FC_MAKE_ERR(EACCES));
				}
				sx_procp->spp_rss += mmu_btopr(seg->s_size);

				sdp->sd_objtype = SX_CMEM_MAP;
				break;

			} else if ((dp = (struct devpage *)
			    page_numtopp(pf, SE_SHARED)) != NULL) {

				u_int len;

				/*
				 * We are loading up translations to pageable
				 * DRAM.
				 */
				if (sx_fault_ahead) {
					len = min(ptob(sx_fault_ahead_npages),
						((seg->s_base + seg->s_size) -
							sx_vaddr));
				} else {
					len = MMU_PAGESIZE;
				}
				if (sx_fault_pageable_mem(sx_hat, seg->s_as,
					sx_vaddr, orig_vaddr, dp, len,
						sdp->sd_prot) == -1) {

					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (FC_MAKE_ERR(EACCES));
				}
				sx_procp->spp_rss += mmu_btopr(len);

				sdp->sd_objtype = SX_PGBLE_MAP;

				/* Release the lock on the page */
				page_unlock((struct page *)dp);
			} else {
				/*
				 * Set up translations to video memory.
				 * Load the entire segment now
				 */

				/* we're loading translations to VRAM  */
				if (sx_fault_vidmem(hat, seg->s_as,
				    seg->s_base, sdp->sd_orig_vaddr,
				    seg->s_size, sdp->sd_prot) == -1) {

					SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
					return (FC_MAKE_ERR(EACCES));
				}
				sx_procp->spp_rss += mmu_btopr(seg->s_size);

				sdp->sd_objtype = SX_VRAM_MAP;
				break;
			}
		}
		break;
	}
	sdp->sd_valid = B_TRUE;
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (0);
}

/*
 * Load translations from SPAMified vaddr to physically contiguous memory
 * (cmem). We load translations to the entire segment and hope that the
 * translations can be set up using the most optimal SRMMU page sizes.
 */
static int
sx_fault_cmem(struct hat *hat, struct as *as, caddr_t sx_vaddr,
	caddr_t orig_vaddr, u_int len, u_int prot)
{
	u_int pf, sx_pf;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	/*
	 * Probe the MMU for the original virtual address.
	 */
	if ((pf = sx_getpfnum(as, orig_vaddr)) == -1)
		return (-1);

	/*
	 * Set the high order four bits of the MBus address space for SX access.
	 */
	sx_pf = SX_MAKEPF(pf, sx_bus_type);

	/*  We want to load up the whole seg.  */
	hat_devload(hat, sx_vaddr, len, sx_pf, prot, HAT_LOAD);
	return (0);
}

/*
 * Load translations from SPAMified vaddr to Video Memory (cgfourteen)
 * (vram). We load translations to the entire segment and hope that the
 * translations can be set up using the most optimal SRMMU page sizes.
 */

/*ARGSUSED*/
static int
sx_fault_vidmem(struct hat *hat, struct as *as, caddr_t sx_vaddr,
	caddr_t orig_vaddr, u_int len, u_int prot)
{
	u_int pf, sx_pf;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	/*
	 * Probe the MMU for the original virtual address.
	 */
	if ((pf = sx_getpfnum(as, orig_vaddr)) == -1)
		return (-1);

	/*
	 * Set the high order four bits of the MBus address space for SX access.
	 */
	sx_pf = SX_MAKEPF(pf, sx_bus_type);

	/* We want to load up the whole seg.  */
	hat_devload(hat, sx_vaddr, len, sx_pf, prot, HAT_LOAD);

	return (0);
}

/*
 * We are loading up translations to pageable DRAM, so ensure that we are
 * setting up a non-cached translations.
 */
/*ARGSUSED*/
static int
sx_fault_pageable_mem(struct hat *sx_hat, struct as *as,
	caddr_t sx_vaddr, caddr_t orig_vaddr, struct devpage *dp,
	u_int len, u_int prot)
{
	struct seg_ops *orig_segops;
	struct seg *orig_seg;
	u_int  oprot;
	u_int sx_pf, pf;
	caddr_t end_vaddr;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	if ((orig_seg = as_segat(as, orig_vaddr)) ==
	    (struct seg *)NULL) {
		cmn_err(CE_PANIC, SX_FAULT_MSG1);
		return (-1);
	}
	orig_segops = orig_seg->s_ops;

	for (end_vaddr = sx_vaddr + len; sx_vaddr < end_vaddr;
		sx_vaddr += PAGESIZE, orig_vaddr += PAGESIZE) {

		/*
		 * Probe the MMU to get the pageframe number for
		 * the original virtual address.
		 */
		if ((pf = hat_getpfnum(as->a_hat, orig_vaddr)) == -1) {
			continue;
		}

		if ((dp = (page_t *)page_numtopp(pf, SE_SHARED)) == NULL) {
			continue;
		}

		/*
		 * Flush the cache ONLY if the page
		 * has NOT already been marked non-cacheable.
		 */
		if (!PP_ISNC((struct page *)dp)) {
			hat_pagecachectl((struct page *)dp, HAT_UNCACHE);
		}
		/*
		 * Get the protections for the original virtual
		 * address.
		 */
		(*orig_segops->getprot)(orig_seg, orig_vaddr, 0, &oprot);

		if ((oprot & PROT_WRITE) == 0)
			prot |= HAT_NOSYNC;

		/*
		 * Set the high order four bits of the MBus address
		 * space for SX access.
		 */
		pf = page_pptonum((page_t *)dp);
		sx_pf = SX_MAKEPF(pf, sx_bus_type);

		hat_devload(sx_hat, sx_vaddr, PAGESIZE, sx_pf, prot, HAT_LOAD);

		page_unlock((struct page *)dp);
	}
	return (0);
}


/*
 * asynchronous fault is a no op. Fail silently.
 */

/*ARGSUSED*/
static faultcode_t
segsx_faulta(struct seg *seg, caddr_t addr)
{
	return (FC_MAKE_ERR(EFAULT));
}

/*ARGSUSED*/
static int
segsx_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	return (EACCES);
}

/*ARGSUSED*/
static int
segsx_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	int error;
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg-s_as, &seg->s_as->a_lock));

	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
	/*
	 * Since we only use segment level protection, simply check against
	 * them.
	 */
	error = ((sdp->sd_prot & prot) != prot) ? EACCES : 0;
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (error);
}

/*
 * Return protections for the specified range. This segment driver does not
 * implement page-level protections. The same protection is used for the
 * entire range of virtual address mapped by this segment.
 */

static	int
segsx_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;
	u_int pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);
	if (pgno != 0) {
		do
			protv[--pgno] = sdp->sd_prot;
		while (pgno != 0);
	}
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segsx_getoffset(struct seg *seg, caddr_t addr)
{
	return ((u_offset_t)0);
}

/*ARGSUSED*/
static int
segsx_gettype(struct seg *seg, caddr_t addr)
{
	return (MAP_SHARED);
}

/*ARGSUSED*/
static int
segsx_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	*vpp = sdp->sd_vp;
	return (0);
}

/*ARGSUSED*/
static int
segsx_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (-1);
}

/*ARGSUSED*/
static size_t
segsx_swapout(struct seg *seg)
{
	return (0);
}

/*ARGSUSED*/
static int
segsx_sync(struct seg *seg, caddr_t addr, size_t len, int attr, u_int flags)
{
	return (0);
}

/*ARGSUSED*/
static size_t
segsx_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	u_int v = 0;
	struct sx_seg_data *sdp = (struct sx_seg_data *)seg->s_data;

	/*
	 * SX register file is always in core
	 */
	if (sdp->sd_objtype == SX_REG_MAP ||
	    sdp->sd_objtype == SX_PRIV_REG_MAP) {
		for (len = (len + PAGEOFFSET) & PAGEMASK; len;
		    len -= MMU_PAGESIZE, v += MMU_PAGESIZE) {
			*vec++ = 1;
		}
	}
	return (v);
}

/*ARGSUSED*/
static int
segsx_lockop(struct seg *seg, caddr_t addr, size_t len, int attr,
	int op, ulong *lockmap, size_t pos)
{
	return (0);
}

/*ARGSUSED*/
static int
segsx_advise(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	return (0);
}

/*ARGSUSED*/
static void
segsx_dump(struct seg *seg)
{
}

/*ARGSUSED*/
static int
segsx_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
segsx_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}

/*
 * Call back routine. Called by the SX HAT unload operation when the original
 * virtual address range which is cloned for SX access is unloaded. When an
 * address range cloned for SX is unmapped, the corresponding SX address range
 * must also be unmapped.
 */

/*ARGSUSED*/
static void
sx_orig_unmap(struct as *as, caddr_t vaddr, u_int len)
{
	struct seg *sxseg;
	struct sx_seg_data *sdp;
	extern struct vnode *common_specvp(struct vnode *);

	ASSERT(as && AS_WRITE_HELD(as, &as->a_lock));
	SX_CNTXT_LOCK_ENTER(&sx_cntxt_mutex);

	if ((sxseg = as_segat(as, vaddr)) == (struct seg *)NULL) {
		SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
		cmn_err(CE_PANIC, "sx_orig_unmap(): Cannot find SX segment\n");
	}
	sdp = (struct sx_seg_data *)sxseg->s_data;

	/*
	 * The ordering in which the <SX virtual address, len> information
	 * and <original virtual address, len> information is deleted from
	 * the per address space SX HAT lists is extremely important to ensure
	 * that this routine does not become re-entrant by the same thread.
	 * First delete information about the <original virtual address, len>.
	 * Then unmap the corresponding SX virtual address range. Then delete
	 * the <SX virtual address, len> information.
	 */

	sx_mmu_del_vaddr(sxseg->s_as, sdp->sd_orig_vaddr,
		sxseg->s_base, sxseg->s_size, ORIG_ADDR);

	hat_unload(sxseg->s_as->a_hat, sxseg->s_base, sxseg->s_size,
	    HAT_UNLOAD_UNMAP);

	sx_mmu_del_vaddr(sxseg->s_as, sdp->sd_orig_vaddr,
		sxseg->s_base, sxseg->s_size, SX_ADDR);

	ASSERT(sdp->sd_vp != NULL);
	VOP_DELMAP(common_specvp(sdp->sd_vp),
		(offset_t)0, sxseg->s_as, sxseg->s_base, sxseg->s_size,
		sdp->sd_prot, sdp->sd_maxprot, MAP_SHARED, CRED());

	seg_free(sxseg);
	SX_CNTXT_LOCK_EXIT(&sx_cntxt_mutex);
}

/*
 * Supporting routines
 */
static void
sx_init(void)
{
	/*
	 * Set up the SX Serial Error Register (SER) bits and enable
	 * the interrupts. Initialize the R0 port
	 * Extended page bounds checking is disabled by default
	 */
	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	sxregsp->s_csr = (SX_EE1 | SX_EE2 | SX_EE3 | SX_EE4 |
				SX_EE5 | SX_EE6 | SX_EI);
	sxregsp->s_r0_init = 0;
	sxregsp->s_diagreg = 0;
}

/*
 * Invoked by the attach routine to obtain SX properties from the PROM,
 * set up handler for SX interrupts and map in the SX register set.
 * SX supports access to its register set from two different base addresses.
 * If the address of SX register set is at an offset 0 from the base address
 * of SX then SX allows write access to all registers in the set. If the
 * address used to reference the register set is at offset 0x1000 from the
 * base address of SX then some of the registers (ex. control registers)
 * cannot be written to. The latter address is used in setting up a mapping
 * from user land to the SX register set. The former address is used by this
 * driver for accessing the SX register set.
 *
 */

static int
sx_setupinfo(dev_info_t *devi)
{
	caddr_t vaddr;	/* Base (kernel virtual) address of SX register set */

	SX_HW_LOCK_ENTER(&sx_hw_mutex);

	if (aflt_add_handler(devi, AFLT_SX, (void **)&sx_aflt_cookie,
	    (int (*)(void *, void *))sx_fatal_err, NULL) != AFLT_SUCCESS) {
		cmn_err(CE_WARN, "sx_setupinfo aflt_add_handler failed\n");
		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		return (-1);
	}

	if (ddi_map_regs(devi, 0, &vaddr, 0, (u_int)SX_PGSIZE) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "sx_setupinfo ddi_mapregs failed\n");
		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		return (-1);
	}

	sxregsp = (struct sx_register_address *)vaddr;

	/*
	 * The user maps in the non-privileged SX base address for
	 * access to the SX register set. This address is at offset
	 * SX_UADDR_OFFSET from the base address. Save the physical address
	 * which must be used at fault handling time, to set up translations.
	 */
	if (ddi_map_regs(devi, 1, &vaddr, 0, (u_int)SX_PGSIZE) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "sx_setupinfo ddi_mapregs (user) failed\n");
		SX_HW_LOCK_EXIT(&sx_hw_mutex);
		return (-1);
	}
	sxuregsp = (struct sx_register_address *)vaddr;

	sx_usr_pf = hat_getkpfnum(vaddr);

	sx_init();		/* and initialize */
	SX_HW_LOCK_EXIT(&sx_hw_mutex);
	return (0);
}


/*
 * Initialize a new context's control/status register
 * Note that the extended page bounds checking is not set by default. It is
 * set explictly by a process using the SX_SET_PGBNDS ioctl(2) command.
 * Setting the logical register bank sticky bits guarantees that
 * each new context gets a zero-filled register file.
 */

static void
sx_cntxtinit(struct sx_cntxt *cntxtp)
{
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
#ifdef	SX_P0_BUG1
	/*
	 * Disable misaligned access exception error interrupts from SX
	 * core (EE5). These seem to be generated spuriously due to a bug
	 * in the SX P0 HW.
	 */
	cntxtp->spc_csr = (SX_EE1 | SX_EE2 | /* SX_EE3 | */ SX_EE4 |
			    /* SX_EE5 | */ SX_EE6 | SX_EI | SX_GO);
#else
	cntxtp->spc_csr = (SX_EE1 | SX_EE2 | SX_EE3 | SX_EE4 |
			    SX_EE5 | SX_EE6 | SX_EI | SX_GO);
#endif	/* SX_P0_BUG1 */
	cntxtp->spc_ser = 0;
	cntxtp->spc_diagreg = sx_diag_default;
}

/*
 * Copy regsister contents of one context to the new context.
 * Done when a process with a mapping to the SX register set forks.
 */

static void
sx_cntxtcopy(struct sx_proc *src_procp, struct sx_proc *dst_procp)
{
	struct sx_cntxt *src_cntxtp;
	struct sx_cntxt *dst_cntxtp;
	struct sx_ctlregs *swctlregs;
	volatile struct sx_ctlregs *hwctlregs;
	struct sx_regfile *swregfile;
	volatile struct sx_regfile *hwregfile;


	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	/*
	 * If src_procp is the currently active context, then copy the
	 * register contents from the device, otherwise copy from the
	 * register save area.
	 */

	dst_cntxtp = dst_procp->spp_cntxtp;
	if (src_procp == sx_current_procp) {

		swctlregs = (struct sx_ctlregs *)dst_cntxtp;
		swregfile = (struct sx_regfile *)dst_cntxtp->spc_regfile;
		hwctlregs = (struct sx_ctlregs *)sxregsp;
		hwregfile = (struct sx_regfile *)sxregsp->s_regfile;
		*swctlregs = *hwctlregs; /* Structure copy */
		*swregfile = *hwregfile; /* Strucure copy */
	} else {

		src_cntxtp = src_procp->spp_cntxtp;

		swctlregs = (struct sx_ctlregs *)dst_cntxtp;
		swregfile = (struct sx_regfile *)dst_cntxtp->spc_regfile;
		hwctlregs = (struct sx_ctlregs *)src_cntxtp;
		hwregfile = (struct sx_regfile *)src_cntxtp->spc_regfile;

		*swctlregs = *hwctlregs; /* Structure copy */
		*swregfile = *hwregfile; /* Strucure copy */
	}
}

/*
 * Save SX hardware context, into the per-sx-process context save area.
 */
static int
sx_regsave(struct sx_cntxt *cntxtp)
{
	int i;
	volatile struct sx_register_address *regsp = sxregsp;
	volatile struct sx_ctlregs *hwctlregs =
					(struct sx_ctlregs *)sxregsp;
	struct sx_ctlregs *swctlregs =
					(struct sx_ctlregs *)cntxtp;

	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	/*
	 * Wait for any pending instructions to complete. At this point we
	 * are guaranteed that NO process has access to the SX device and that
	 * all locks required for setting up (MMU translations) for access to
	 * the device are owned by the process for which we are establishing a
	 * SX context.
	 */
	while (regsp->s_csr & SX_BZ); /* wait forever */

	if (regsp->s_ser) {	/* SX is indicating an error */
		return (SX_REGSAVE_FAIL);
	}

	*swctlregs = *hwctlregs;	/* Struct copy */


	/*
	 * Save only the bank of registers that were modified in this
	 * context
	 */

	if (swctlregs->spc_csr & SX_B0MOD) {
		for (i = 1; i < SX_B1_REGS; i++)
			cntxtp->spc_regfile[i] = regsp->s_regfile[i];
	}

	if (swctlregs->spc_csr & SX_B1MOD) {
		for (i = SX_B1_REGS; i < SX_B2_REGS; i++)
			cntxtp->spc_regfile[i] = regsp->s_regfile[i];
	}

	if (swctlregs->spc_csr & SX_B2MOD) {
		for (i = SX_B2_REGS; i < SX_B3_REGS; i++)
			cntxtp->spc_regfile[i] = regsp->s_regfile[i];
	}

	if (swctlregs->spc_csr & SX_B3MOD) {
		for (i = SX_B3_REGS; i < SX_NREGS; i++)
			cntxtp->spc_regfile[i] = regsp->s_regfile[i];
	}

	swctlregs->spc_csr &= ~(SX_B0MOD | SX_B1MOD | SX_B2MOD |
				SX_B3MOD); /* Clear the dirty bits */

	return (SX_REGSAVE_SUCCESS);
}

static void
sx_regrestore(struct sx_cntxt *cntxtp)
{
	int i;
	volatile struct sx_register_address *regsp = sxregsp;
	volatile struct sx_ctlregs *hwctlregs =
					(struct sx_ctlregs *)sxregsp;
	struct sx_ctlregs *swctlregs =
					(struct sx_ctlregs *)cntxtp;

	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	/*
	 * Wait for any pending instructions to complete. At this point we
	 * are guaranteed that NO process has access to the SX device and that
	 * all locks required for setting up (MMU translations) for access to
	 * the device are owned by the process for which we are establishing a
	 * SX context.
	 */
	while (regsp->s_csr & SX_BZ); /* wait forever */

	if (regsp->s_ser & SX_SI) { /* SX is indicating an error */
		cmn_err(CE_WARN,
			"sx_regsrestore(): SX indicating error!\n");
	}

	/*
	 * Restore SX csr, ser, planereg, rop reg, page bounds registers
	 * and timer information
	 * First, disable interrupts to avoid race conditions during context
	 * switches (SX diagnostics tests access the control and error
	 * registers causing much grief for the OS).
	 */

	hwctlregs->spc_csr &= ~SX_EI;
	hwctlregs->spc_diagreg = swctlregs->spc_diagreg;
	hwctlregs->spc_ser = swctlregs->spc_ser;
	hwctlregs->spc_planereg = swctlregs->spc_planereg;
	hwctlregs->spc_ropreg = swctlregs->spc_ropreg;
	hwctlregs->spc_iqcounter = swctlregs->spc_iqcounter;
	hwctlregs->spc_pg_lo = swctlregs->spc_pg_lo;
	hwctlregs->spc_pg_hi = swctlregs->spc_pg_hi;
	hwctlregs->spc_csr = swctlregs->spc_csr;

	for (i = 1; i < SX_NREGS; i++)
		regsp->s_regfile[i] = cntxtp->spc_regfile[i];
}

/*
 * Unload all translations in the current context, before loading a new
 * SX context. Invoked at fault time when a new SX context has to be
 * set up.
 */
static void
sx_unload(struct sx_proc *p)
{
	struct seg *seg;
	struct sx_seg_data *sdp;
	struct as *as = p->spp_as;

	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	ASSERT(as && AS_LOCK_HELD(as, &as->a_lock));

	/*
	 * The SX HAT will wait a short time for the
	 * SX pipline to empty out before the translation is unloaded.
	 */
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		if (seg->s_ops != &segsx_ops)
			continue;
		sdp = (struct sx_seg_data *)seg->s_data;
		if (sdp->sd_valid) {
			hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
			    HAT_UNLOAD | sx_tmpflag);
			sdp->sd_valid = B_FALSE;
		}
	}
}

/*
 * A sx context switch involves saving the register contents of the
 * current context and unloading all translations in the current context.
 */

static int
sx_cntxtswtch(struct sx_proc *new_sxprocp)
{
	ASSERT(SX_HW_LOCK_HELD(&sx_hw_mutex));
	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	if (sx_current_procp == NULL)
		cmn_err(CE_PANIC, "sx_cntxtswtch(): sx_current_procp NULL");
	/*
	 * Blow away the translations for the current
	 * context and save registers.
	 */
	sx_unload(sx_current_procp);

	SX_TRACE(sx_current_procp->spp_as, 3); /* unloading current ctx */

	if ((sx_regsave(sx_current_procp->spp_cntxtp)) == SX_REGSAVE_FAIL)
		return (SX_REGSAVE_FAIL);

	/*
	 * Restore registers for the new context.
	 */
	SX_TRACE(new_sxprocp->spp_as, 4);	/* loading new ctx */

	sx_regrestore(new_sxprocp->spp_cntxtp);
	sx_current_procp = new_sxprocp;
	sx_swtches++;

	return (SX_REGSAVE_SUCCESS);
}

/*
 * Given an address and a virtual address, probe the MMU and return the
 * appropriate page frame number. The probe is done at level1, level2
 * and level3 to determine the level and the resulting page frame number
 * is corrected for the given virtual address if the virtual address is not
 * aligned on the same boundary as the physical page. The virtual address
 * can be the original virtual address or the SPAMified virtual address.
 * This routine is predicated on the fact that SX accelarator runs on a
 * Sun4m/SRMMU machine.
 */

static u_int
sx_getpfnum(struct as *as, caddr_t addr)
{
	return (hat_getpfnum(as->a_hat, addr));
}

/*
 * Search the context table for the process whose address space is as.
 */

static struct sx_proc *
sx_getproc(int cntxtnum, struct as *as)
{
	struct sx_cntxtinfo *cntxtinfop;
	struct sx_proc *p;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));

	if (cntxtnum >= SX_MAXCNTXTS)
		return ((struct sx_proc *)0);
	cntxtinfop = SX_GET_CNTXTINFO(cntxtnum);

	p = cntxtinfop->spc_sxprocp;

	while (p) {

		if ((p->spp_as == as))
			return (p);
		p = p->spp_next;
	}
	return ((struct sx_proc *)0);
}


/*
 * Search the context table for the process whose address space is "as" and
 * whose context is cntxtnum and free it.
 */

static void
sx_freeproc(int cntxtnum, struct sx_proc *p)
{
	struct sx_cntxtinfo *cntxtinfop;
	struct sx_proc *q;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	cntxtinfop = SX_GET_CNTXTINFO(cntxtnum);
	/*
	 * If this process still has SX mappings the process cannot be
	 * freed from the list.
	 */

	if (p->spp_segcnt)
		return;

	q = p->spp_prev;
	if (q)
		q->spp_next = p->spp_next;
	q = p->spp_next;
	if (q)
		q->spp_prev = p->spp_prev;

	if (p == cntxtinfop->spc_sxprocp)
		cntxtinfop->spc_sxprocp = p->spp_next;
	kmem_free(p->spp_cntxtp, sizeof (struct sx_cntxt));
	p->spp_cntxtp = NULL;
	/*
	 * If the process is being freed is the currently
	 * active process, then set sx_current_procp to
	 * NULL
	 */
	if (p == sx_current_procp) {

		/* freeing current proc */
		SX_TRACE(p->spp_as, 1);
		sx_current_procp = (struct sx_proc *)0;

	}
#if	!defined(lint)
	    else {
		/* freeing some other proc */
		SX_TRACE(p->spp_as, 2);
	}
#endif

	kmem_free(p, sizeof (struct sx_proc));
	p = NULL;
}

/*
 * Add the SX proc whose address space is "as" and whose context number is
 * cntxtnum onto the context table.
 */

static struct sx_proc *
sx_addproc(int cntxtnum, struct as *as)
{
	struct sx_cntxtinfo *cntxtinfop;
	struct sx_proc *p, *q;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	cntxtinfop = SX_GET_CNTXTINFO(cntxtnum);
	p = cntxtinfop->spc_sxprocp;

	q = (struct sx_proc *)
		    kmem_zalloc(sizeof (struct sx_proc), KM_SLEEP);
	/*
	 * Allocate a register context save area for this process
	 */
	q->spp_cntxtp = (struct sx_cntxt *)
	    kmem_zalloc(sizeof (struct sx_cntxt), KM_SLEEP);
	q->spp_as = as;
	q->spp_cntxtnum = (u_short)cntxtnum;
	q->spp_segcnt = 0;

	SX_TRACE(as, 0);  /* a context is born */

	if (p == (struct sx_proc *)0) {	/* Initialize list head */
		cntxtinfop->spc_sxprocp = q;
		return (q);
	}
	while (p->spp_next)	/* Forked process */
		p = p->spp_next;
	p->spp_next = q;
	q->spp_prev = p;
	return (q);
}

/*
 * Supporting routines for ioctl(2) commands.
 */

/*
 * sx_vrfy_va() verifies whether a given virtual address is managed by the SX
 * driver or not. It initializes the user supplied arg with the segment base
 * address and size.
 */

int
sx_vrfy_va(struct sx_valid_va *arg)
{

	struct as *as;
	struct seg *seg;
	caddr_t saddr, eaddr, segbase, segend;
	struct sx_valid_va smv;

	if (copyin(arg, &smv, sizeof (smv)) != 0)
		return (EFAULT);
	as = curproc->p_as;
	saddr = (caddr_t)smv.sp_vaddr;
	eaddr = (caddr_t)smv.sp_vaddr + smv.sp_len;
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		if (seg->s_ops != &segsx_ops)
			continue;
		segbase = seg->s_base;
		segend = seg->s_base + seg->s_size;
		if ((segbase >= saddr && segbase < eaddr) ||
		    (segend > saddr && segend <= eaddr)) {
			smv.sp_base_vaddr = (u_int)MAX(saddr, segbase);
			smv.sp_base_len = (u_int)MIN(eaddr, segend) -
				smv.sp_base_vaddr;
			AS_LOCK_EXIT(as, &as->a_lock);
			if (copyout(&smv, arg, sizeof (smv)) != 0) {
				return (EFAULT);
			}
			return (0);
		}
		if ((saddr >= segbase && saddr < segend) ||
		    (eaddr > segbase && eaddr <= segend)) {
			smv.sp_base_vaddr = (u_int)MAX(saddr, segbase);
			smv.sp_base_len = (u_int)MIN(eaddr, segend) -
				smv.sp_base_vaddr;
			AS_LOCK_EXIT(as, &as->a_lock);
			if (copyout(&smv, arg, sizeof (smv)) != 0) {
				return (EFAULT);
			}
			return (0);
		}
	}
	AS_LOCK_EXIT(as, &as->a_lock);
	smv.sp_base_vaddr = 0;
	smv.sp_base_len = 0;
	if (copyout(&smv, arg, sizeof (smv)) != 0) {
		return (EFAULT);
	}
	return (0);
}


/*
 * Given a virtual address that is mapped by the SX segment driver,
 * return the corresponding original virtual address.
 */

int
sx_get_orig_va(struct sx_original_va *arg)
{
	struct as *as;
	struct seg *seg;
	struct sx_seg_data *sdp;
	struct sx_original_va smv;

	if (copyin(arg, &smv, sizeof (smv)) != 0)
		return (EFAULT);
	as = curproc->p_as;
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, (caddr_t)smv.sp_sx_vaddr)) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (EINVAL);
	}
	AS_LOCK_EXIT(as, &as->a_lock);
	if (seg->s_ops != &segsx_ops)
		return (EINVAL);
	sdp = (struct sx_seg_data *)seg->s_data;
	smv.sp_orig_vaddr = (u_int)sdp->sd_orig_vaddr;
	if (copyout(&smv, arg, sizeof (smv)) != 0)
		return (EFAULT);
	return (0);
}

/*
 * Handle fatal faults from SX core
 */
void
sx_fatal_err(struct proc *pp)
{

	u_int ser;

	SX_AFLT_LOCK_ENTER(&sx_aflt_mutex);

	if (!sxregsp->s_ser & SX_SI) {
		cmn_err(CE_WARN, "Spurious SX intr! SI not set in SER\n");
		SX_AFLT_LOCK_EXIT(&sx_aflt_mutex);
		return;
	}
	ser = sxregsp->s_ser;
	if (ser & SX_EE1) {
		cmn_err(CE_WARN, "Illegal/unimplemented instruction \n");
		psignal(pp, SIGILL);
	}
	if (ser & SX_EE2) {
		cmn_err(CE_WARN, "Out of page bounds access\n");
		psignal(pp, SIGSEGV);
	}
	if (ser & SX_EE3) {
		cmn_err(CE_WARN, "Attempted access to address outside \
			D[V]RAM\n");
		psignal(pp, SIGSEGV);
	}
	if (ser & SX_EE4) {
		cmn_err(CE_WARN, "Illegal register < 0 or > 127 used\n");
		psignal(pp, SIGILL);
	}
	if (ser & SX_EE5) {
		cmn_err(CE_WARN, "Misaligned access during load/store\n");
		psignal(pp, SIGSEGV);
	}
	if (ser & SX_EE6) {
		cmn_err(CE_WARN, "Illegal write to instruction queue\n");
		psignal(pp, SIGILL);
	}
	sxregsp->s_ser = 0;
	SX_AFLT_LOCK_EXIT(&sx_aflt_mutex);
}

void
sx_debug(int level, char *fmt, ...)
{
	va_list adx;

	if (!debug)
		return;

	va_start(adx, fmt);
	vcmn_err(level, fmt, adx);
	va_end(adx);
}


/*
 * For debug only....
 */
#ifdef SX_DEBUG
sx_dump_as_segs(struct as *as)
{
	struct seg *seg;

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		cmn_err(CE_CONT, "base = 0x%x size = 0x%x\n",
			seg->s_base, seg->s_size);
	}
	AS_LOCK_EXIT(as, &as->a_lock);
}
#endif

/*
 * Check if a process has established a SX context. A context is established
 * when a process opens the SX device. Only one open of the SX device is
 * allowed per process. Return 1 if we find the context is mapped and 0
 * otherwise.
 */
static int
sx_vrfy_cntxt_setup(void)
{
	int i;
	struct as *as;
	struct sx_cntxtinfo *cntxtinfop;
	struct sx_proc *p;

	ASSERT(SX_CNTXT_LOCK_HELD(&sx_cntxt_mutex));
	cntxtinfop = sx_cntxt_tbl;

	/*
	 * Search the context table for the process with the address as. Note
	 * that if a process has multiple SX contexts it could have an
	 * entry in several slots of the context table.
	 */
	as = curproc->p_as;
	for (i = 0; i < SX_MAXCNTXTS; i++) {

		p = cntxtinfop->spc_sxprocp;
		while (p) {
			if (p->spp_as == as) {
				return (1);
			}
			p = p->spp_next;
		}
		cntxtinfop++;
	}
	return (0);
}
