/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)xbox.c	1.30	99/10/04 SMI"

/*
 * Combined nexus and leaf driver for the 'XBox' expansion box.
 */

/* #undef DEBUG */
#if defined(lint) && !defined(DEBUG)
#define	DEBUG	1
#endif

#if DEBUG
#define	XBOX_DEBUG
#endif

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/open.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/debug.h>

#include <sys/xboxreg.h>
#include <sys/xboximpl.h>
#include <sys/xboxif.h>

#ifdef XBOX_DEBUG
static int xdebug = 0;
static int xbox_go_non_transparent = 0;
void debug_enter(char *);
#endif /* XBOX_DEBUG */

/*
 * set this flag in /etc/system for manufacturing testing
 */
static int xbox_dont_panic = 0;
static int xbox_no_cards_in_slot0 = 0;

/*
 * Nexus ops
 */
static int
xbox_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

#ifdef DONTNEED
static int
xbox_dma_mctl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    enum ddi_dma_ctlops, off_t *, u_int *, caddr_t *, u_int);

static int
xbox_dma_map(dev_info_t *, dev_info_t *, struct ddi_dma_req *,
	ddi_dma_handle_t *);
#endif

static struct bus_ops xbox_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	xbox_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

/*
 * Leaf ops (supports diagnostic access to the device)
 */
static int xbox_open(dev_t *, int, int, cred_t *);
static int xbox_close(dev_t, int, int, cred_t *);
static int xbox_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static struct cb_ops xbox_cb_ops = {
	xbox_open,
	xbox_close,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	xbox_ioctl,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,
	D_NEW | D_MP
};

/*
 * Device ops
 */
static int xbox_identify(dev_info_t *);
static int xbox_probe(dev_info_t *);
static int xbox_attach(dev_info_t *, ddi_attach_cmd_t cmd);
static int xbox_detach(dev_info_t *, ddi_detach_cmd_t cmd);

static struct dev_ops xbox_ops = {
	DEVO_REV,
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	xbox_identify,
	xbox_probe,
	xbox_attach,
	xbox_detach,
	nodev,			/* reset */
	&xbox_cb_ops,		/* leaf driver operations */
	&xbox_bus_ops		/* bus operations */
};

/*
 * Module ops
 */

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"xbox nexus driver",	/* Name of module. */
	&xbox_ops		/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

static void *xbh;	/* where all our state lives */

/*
 * prototypes of all internal functions
 */
static u_int xbox_intr(caddr_t arg);
static int xbox_non_transparent(struct xbox_state *xac);
static void xbox_transparent(struct xbox_state *xac);
#ifdef XBOX_DEBUG
static void xbox_dump_csr(struct xbox_state *xac, char *s);
#endif
static int compare(struct xbox_state *xac, char *what, u_int found,
	u_int expected);
static int xbox_compare_csr(struct xbox_state *xac);
static void xbox_init(struct xbox_state *xac, int flag);
static void xbox_uninit(struct xbox_state *xac);
static int xbox_check_for_boot_errors(struct xbox_state *xac);
static int xbox_check_status(struct xbox_state *xac);
static void xbox_dump_epkt(struct xbox_state *xac, int who);
static void xbox_reset(struct xbox_state *xac, u_int type, u_int length);
static int xbox_wait_for_error(struct xbox_state *xac,
	struct xc_errs *epkt_p, clock_t timout);
static int xbox_reg_check(struct xbox_state *xac);
static void xprintf(struct xbox_state *xac, const char *fmt, ...);

#ifdef XBOX_DEBUG
static void
xbox_write0(struct xbox_state *xac, u_int offset, u_int data)
{
	int what = (xac->xac_write0_key24) |
	    (offset & 0x7f0000) | (data & 0xffff);

	XPRINTF_DEBUG(xac, "write0: offset=%x, data=%x, what=%x\n",
	    offset, data, what);

	*(xac->xac_xc.xac_write0) = what;
}
#else
#define	xbox_write0(xac, offset, data) \
	*(xac->xac_xc.xac_write0) = (xac->xac_write0_key24) | \
	    ((offset) & 0x7f0000) | ((data) & 0xffff)
#endif /* XBOX_DEBUG */


#define	XBOX_DMA_SYNC(xac) \
	(void) ddi_dma_sync((xac)->xac_dhandle, (off_t)0, \
	    2 * sizeof (struct xc_errs), DDI_DMA_SYNC_FORKERNEL)


/*
 * Module configuration stuff
 */
int
_init(void)
{
	register int e;

	if ((e = ddi_soft_state_init(&xbh,
	    sizeof (struct xbox_state), 1)) != 0)
		return (e);

	if ((e = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&xbh);

	return (e);
}

int
_fini(void)
{
	register int e;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	ddi_soft_state_fini(&xbh);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
xbox_identify(dev_info_t *devi)
{
	if (strcmp("SUNW,xbox", ddi_get_name(devi)) == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}


static int
xbox_probe(dev_info_t *devi)
{
	register int instance = ddi_get_instance(devi);
	char *buf;
	int len;
	int result;
	int child_present = 0;

#ifdef XBOX_DEBUG
	if (xdebug > 2)
		debug_enter("xbox_probe");
#endif /* XBOX_DEBUG */

	/*
	 * Refuse to be fostered onto non-self-identifying parents.
	 */
	if (ddi_dev_is_sid(devi) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	/*
	 * Did the FCode find any children of this adapter?
	 */
	result = ddi_getlongprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "child-present", (caddr_t)&buf, &len);

	if (result == DDI_PROP_SUCCESS) {
		child_present = (strcmp(buf, "true") == 0);
		kmem_free(buf, len);
	}
	if (child_present == 0) {
		cmn_err(CE_WARN,
		"xbox%d: no power, or no cable plugged in, or obsolete hw!",
		    instance);
		/*
		 * XXX	Maybe probe partial .. one day .. (though quite how
		 *	we get the FCode to re-probe is another entertaining
		 *	thought.)
		 */
		return (DDI_PROBE_FAILURE);
	}

	/*
	 * We might have been plugged into the wrong slot..
	 * XXX not very likely since xbox is not supported on ss1
	 */
	if (ddi_slaveonly(devi) == DDI_SUCCESS) {
		cmn_err(CE_WARN, "xbox%d: can't operate in slave-only slot!",
		    instance);
		return (DDI_PROBE_FAILURE);
	}
	return (DDI_PROBE_DONTCARE);
}


static int
xbox_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register int instance;
	register struct xbox_state *xac;
	register dev_info_t *pdevi;
	auto ddi_dma_cookie_t cookie;
	static ddi_dma_lim_t xbox_dma_lim = {
		(u_long) 0, (u_long) 0xffffffff, (u_long) 0xffffffff,
		1 << 4, 1 << 4, 8192
	};

#ifdef XBOX_DEBUG
	if (xdebug > 2)
		debug_enter("xbox_attach");
#endif /* XBOX_DEBUG */

	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(devi);
	if (ddi_soft_state_zalloc(xbh, instance) != 0) {
		return (DDI_FAILURE);
	}
	xac = ddi_get_soft_state(xbh, instance);
	xac->xac_dev_info = devi;
	xac->xac_instance = instance;

	/*
	 * save xac ptr in driver_private
	 */
	ddi_set_driver_private(devi, (caddr_t)xac);

	/*
	 * This driver supports access to a 'diagnostic' leaf device
	 */
	if (ddi_create_minor_node(devi,
	    "diag", S_IFCHR, instance, NULL, 0) != DDI_SUCCESS) {
		goto broken;
		/*NOTREACHED*/
	}

	/*
	 * Map in the 'write0' register into a handy page.
	 * This page is needed at all times.
	 */
	if (ddi_map_regs(devi, 0, (caddr_t *)&xac->xac_xc.xac_write0, 0L,
	    (off_t)ptob(1L)) != DDI_SUCCESS) {
		xprintf(xac, "cannot map write0 register\n");
		goto broken;
		/*NOTREACHED*/
	}
	XPRINTF(xac, "write0 page=%x\n", xac->xac_xc.xac_write0);

	/*
	 * the FCode left the hw transparent
	 */
	xac->xac_state	|=  XAC_STATE_TRANSPARENT;


	/*
	 * We rely on the FCode not to give us a write0-key of zero
	 */
	xac->xac_soft_status.xac_write0_key = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "write0-key", 0);
	xac->xac_write0_key24 = xac->xac_soft_status.xac_write0_key << 24;

	xac->xac_soft_status.xac_uadm = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "uadm", -1);

	XPRINTF(xac, "write0=%x, uadm=%x\n",
		xac->xac_soft_status.xac_write0_key,
		xac->xac_soft_status.xac_uadm);

	ASSERT(xac->xac_soft_status.xac_write0_key != 0);

	/*
	 * Allocate some nicely consistent buffers, map them into DMA
	 * space, and tell the xbox where they are so it can DMA error
	 * messages at us.
	 *
	 * XXX	Note that we have two buffers, but we make them adjacent
	 *	so that we only have to do a ddi_dma_sync() on one memory
	 *	object (and save DMA mapping resources)
	 */
	if (ddi_iopb_alloc(devi, &xbox_dma_lim, 2 * sizeof (struct xc_errs),
	    (caddr_t *)&xac->xac_epkt) != DDI_SUCCESS) {
		xprintf(xac, "can't allocate iopb for error packets\n");
		goto broken;
		/*NOTREACHED*/
	}

	/*
	 * buffers have to be 16 byte aligned; the xbox hw will currently
	 * xfer 16 bytes but can do more.
	 */
	bzero((caddr_t)xac->xac_epkt, 2 * sizeof (struct xc_errs));
	xac->xbc_epkt = xac->xac_epkt + 1;
	XPRINTF(xac, "xac_epkt=%x, xbc_epkt=%x\n",
		xac->xac_epkt, xac->xbc_epkt);

	/*
	 * clear error descriptor
	 */
	xac->xac_epkt->xc_errd = xac->xbc_epkt->xc_errd = NO_PKT;

	/*
	 * "This error information [the contents of the error registers]
	 * is transferred from the XAdapter to main memory in a DVMA
	 * 4-word burst write ..."
	 */
	if (ddi_dma_addr_setup(devi, NULL, (caddr_t)xac->xac_epkt,
	    2 * sizeof (struct xc_errs),
	    DDI_DMA_READ | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    &xbox_dma_lim, &xac->xac_dhandle) != DDI_DMA_MAPPED) {
		xprintf(xac,
		    "can't set up DMA mapping for xac/xbc packets\n");
		goto broken;
		/*NOTREACHED*/
	}

	if (ddi_dma_htoc(xac->xac_dhandle, (off_t)0, &cookie) !=
	    DDI_SUCCESS) {
		goto broken;
		/*NOTREACHED*/
	}
	xac->xac_epkt_dma_addr = cookie.dmac_address;
	xac->xbc_epkt_dma_addr = cookie.dmac_address + sizeof (struct xc_errs);
	XPRINTF(xac, "xac_epkt_dma_addr=%x, xbc_epkt_dma_addr=%x\n",
		xac->xac_epkt_dma_addr, xac->xbc_epkt_dma_addr);
	ASSERT(((int)xac->xac_epkt_dma_addr & 0xf) == 0);


#ifdef XBOX_DEBUG
	/*
	 * if we are debugging, go non-transparent; else just
	 * re-init the hw; if we don't do an errlog enable, any errors
	 * during booting will be preserved
	 */
	if (xbox_go_non_transparent) {
		/*
		 * switch to non-transparent mode, dump registers, and
		 * and check for errors
		 */
		if (xbox_non_transparent(xac)) {
			XPRINTF(xac, "cannot switch to non-transparent mode\n");
			goto broken;
		}

		if (DEBUGGING) {
			xbox_dump_csr(xac, "init");
		}
		xbox_transparent(xac); /* will also init */
	} else {
		xbox_init(xac, 0);
	}
#else
	/*
	 * reinitialize xbox hw thru write0 writes to be on the safe side
	 */
	xbox_init(xac, 0);
#endif

	/*
	 * check thru an errlog dvma  whether any error
	 * occurred between FCode releasing xbox and now
	 */
	if (xbox_check_for_boot_errors(xac)) {
		goto broken;
	}

	/*
	 * Add the interrupt handler for the XBox to post errors to us
	 * with.  We choose the lowest level available because we expect
	 * errors to be rare, and we don't want to load the autovectors
	 * up with polling our interrupt handler.
	 *
	 * XXX	This is unfortunate - we really want to sit at a
	 *	fairly high priority; but because every interrupt forces
	 *	a ddi_dma_sync(), we don't want to be first in a chain
	 *	of autovectored interrupts.
	 *
	 * For now, we allow the innocent user to specify an xbox
	 * interrupt level via a property lookup with the hope that they
	 * can identify an 'unused' level on their configuration.
	 * the FCode exports a list of 6 possible intr levels the xbox can
	 * interrupt at. The first entry is level 1 and that is what
	 * we use by default.
	 *
	 * XXX I really think that we could do better via some form of hints
	 * mechanism.
	 */

	xac->xac_inumber = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "xbox-interrupt", 1) - 1; /* SBus level 1 */

	XPRINTF(xac, "xbox-interrupt=%x\n", xac->xac_inumber + 1);

	if (xac->xac_inumber < 0 || xac->xac_inumber > 6 ||
	    ddi_intr_hilevel(devi, xac->xac_inumber)) {
		/*
		 * This message gets printed when they specify an
		 * impossibly silly interrupt, or a 'high level'
		 * interrupt that we're simply not prepared to handle
		 * (though we could do ..)
		 */
		xprintf(xac,
		    "sorry - can't service SBus level %d interrupt\n",
		    xac->xac_inumber + 1);
		goto broken;
		/*NOTREACHED*/
	}

	if (ddi_add_intr(devi, xac->xac_inumber, &xac->xac_iblkc,
	    NULL, xbox_intr, (caddr_t)xac) != DDI_SUCCESS) {
		xprintf(xac, "can't add interrupt handler\n");
		goto broken;
		/*NOTREACHED*/
	}

	/*
	 * This single mutex is used to protect the XBox from multiple
	 * concurrent synchronization operations.  In normal use,
	 * the only contention for this lock will be caused by
	 * dma synchronization operations.
	 */
	mutex_init(XAC_MUTEX, NULL, MUTEX_DRIVER, (void *)xac->xac_iblkc);

	/*
	 * initialize cv for sundiag test mode
	 */
	cv_init(XAC_CV, NULL, CV_DRIVER, NULL);

	xac->xac_state |= XAC_STATE_ALIVE;
	xac->xac_soft_status.xac_action_on_error = ACTION_CONT;

	/*
	 * Test to see if slot 0 of the XBox is occupied - we prefer to
	 * read this location to flush platform-specific store buffers
	 * when we're using the write0 mechanism.
	 *
	 * XXX	It should be perfectly possible to fix this driver
	 *	in such a way that it will function (albeit slowly)
	 *	when there is no card in slot 0 by using ddi_poke().
	 */

	if (xbox_no_cards_in_slot0 == 0 &&
	    ddi_peek32(devi, (int32_t *)xac->xac_xc.xac_write0,
			(int32_t *)0) != DDI_SUCCESS) {
		xprintf(xac, "there has to be an SBus card in slot 0!\n");
		goto broken;
	}

	xac->xac_soft_status.xac_action_on_error = ACTION_PANIC;

	/*
	 * Oh ick.  We've got such enormous 'reg' and 'intr'
	 * properties that the standard 'ddi_report_dev'
	 * looks a real mess. So (surprise) we cheat and grot
	 * around at the internals of the devinfo node.	 Sigh.
	 *
	 * (And no, we don't want to translate the SBus level to an
	 *  ipl here - this needs a ctlop!)
	 */
	pdevi = ddi_get_parent(devi);
	cmn_err(CE_CONT, "?xbox%d at %s%d: SBus slot %x 0x%x SBus level %d\n",
	    instance, ddi_get_name(pdevi), ddi_get_instance(pdevi),
	    sparc_pd_getreg(devi, 0)->regspec_bustype,	/* sizeof dependency */
	    sparc_pd_getreg(devi, 0)->regspec_addr,	/* sizeof dependency */
	    xac->xac_inumber + 1);

	return (DDI_SUCCESS);

broken:
#ifdef XBOX_DEBUG
	if (xdebug) {
		(void) xbox_non_transparent(xac);
		xbox_dump_csr(xac, "broken");
	}
#endif
	(void) xbox_detach(devi, DDI_DETACH);
	return (DDI_FAILURE);
}

static int
xbox_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register int instance;
	register struct xbox_state *xac;

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(devi);
	if ((xac = ddi_get_soft_state(xbh, instance)) != NULL) {

		XPRINTF(xac, "xbox detach\n");

		/*
		 * Quiesce the hardware.
		 */
		xbox_uninit(xac);

		/*
		 * deallocate resources
		 */
		if (xac->xac_dhandle)
			(void) ddi_dma_free(xac->xac_dhandle);

		if (xac->xac_epkt)
			(void) ddi_iopb_free((caddr_t)xac->xac_epkt);

		if (xac->xac_iblkc) {
			ddi_remove_intr(devi, xac->xac_inumber,
				&xac->xac_iblkc);
			mutex_destroy(XAC_MUTEX);
			cv_destroy(XAC_CV);
		}

		if (xac->xac_xc.xac_write0) {
			ddi_unmap_regs(devi, 0,
			    (caddr_t *)&xac->xac_xc.xac_write0,
			    0L, (off_t)ptob(1L));
		}

		ddi_remove_minor_node(devi, NULL);
		ddi_soft_state_free(xbh, instance);
	}

	return (DDI_SUCCESS);
}


/*
 * Interrupts are generated as a result of "errors".  Interrupts may
 * be caused by either the XAdapter or the XBox controller to tell
 * us something went wrong.
 */
static u_int
xbox_intr(caddr_t arg)
{
	register struct xbox_state *xac = (struct xbox_state *)arg;
	register struct xc_errs *xa_errs = xac->xac_epkt;
	register struct xc_errs *xb_errs = xac->xbc_epkt;
	register int e = DDI_INTR_UNCLAIMED;
	register int errs = 0;
	register int fatal_errs = 0;

	/*
	 * are we ready to service an interrupt yet? (ie. is the mutex
	 * initialized?
	 */
	if (xac->xac_state & XAC_STATE_ALIVE) {

		mutex_enter(XAC_MUTEX);

		/*
		 * We use the "cheaper" form of sync because we know that
		 * the DMA object will only ever be mapped in the kas.
		 *
		 * Sigh.  Note that we do a ddi_dma_sync() for every
		 * interrupt that we're ever asked about; 99.99% of the time
		 * the interrupt won't be ours, so we just do fruitless syncing
		 * everytime the autovector chain is polled.  That's why we
		 * default ourselves to a low level interrupt. (Ideally, we'd
		 * put ourselves on an interrupt level which no-one else is
		 * using, but this is very difficult to do in general).
		 */
		XBOX_DMA_SYNC(xac);

		if (xa_errs->xc_errd != NO_PKT) {
			xac->xac_saved_epkt = *xa_errs;

			XPRINTF_DEBUG(xac, "xbox_intr\n");
			if (xa_errs->xc_errd & ERRD_ESDB) {
				/*
				 * this is a real hardware problem
				 */
				xbox_dump_epkt(xac, XAC);

				/*
				 * reenable error log
				 */
				xbox_write0(xac, XAC_ERRLOG_ENABLE_OFFSET, 0);
				fatal_errs++;
			} else {
				/*
				 * this can only be caused by a DVTE+INTT;
				 * deassert DVTE + INTT
				 */
				xbox_write0(xac, XAC_CTL1_OFFSET, XAC_CTL1);
			}
			xa_errs->xc_errd = NO_PKT;
			errs++;

		}
		if (xb_errs->xc_errd != NO_PKT) {
			xac->xbc_saved_epkt = *xb_errs;

			XPRINTF_DEBUG(xac, "xbox_intr\n");
			if (xb_errs->xc_errd & ERRD_ESDB) {
				/*
				 * this is a real hardware problem
				 */
				xbox_dump_epkt(xac, XBC);

				/*
				 * reenable error log
				 */
				xbox_write0(xac, XBC_ERRLOG_ENABLE_OFFSET, 0);
				fatal_errs++;
			} else {
				/*
				 * this can only be caused by a DVTE+INTT;
				 * deassert DVTE + INTT
				 */
				xbox_write0(xac, XBC_CTL1_OFFSET, XBC_CTL1);
			}
			xb_errs->xc_errd = NO_PKT;
			errs++;
		}

		if (errs) {
			if (fatal_errs &&
			    xac->xac_soft_status.xac_action_on_error ==
				ACTION_PANIC) {
				/*
				 * XXX do we really want to panic?
				 */
				if (xbox_dont_panic == 0) {
					cmn_err(CE_PANIC,
					    "fatal xbox hw error");
				}
				/*NOTREACHED*/
			} else {
				/*
				 * signal anyone waiting for error
				 */
				e = DDI_INTR_CLAIMED;
				cv_signal(XAC_CV);
			}
		}
		mutex_exit(XAC_MUTEX);
	}
	return (e);
}

/*
 * Nexus operations
 */
/*
 * Interrupt handling.
 *
 * In the prototype version of this driver, we used an interrupt
 * wrapper whose jobs was to ensure that, in the case that the underlying
 * handler had claimed the interrupt, the interrupt transition
 * caused by the child SBus device had propagated back to the
 * parent SBus.	  This avoids apparently 'spurious' interrupts.
 *
 *	"The cable delays for any child should not
 *	exceed 5us.  This accounts for synchronization,
 *	serialization, and cable delays.  The additional
 *	latency for XBox will be less than 2.5us"
 *
 * Or in other words, we just did 'drv_usecdelay(8)' in the case where
 * the interrupt handler returned DDI_INTR_CLAIMED.
 *
 * However, I was assured by the hw designer that the act of writing
 * the child SBus hardware, followed by reading the value back will
 * guarantee that the interrupt will have propagated up to the parent
 * SBus, so this interrupt wrapper, and the special versions of
 * xbox_add_intrspec() and xbox_remove_intrspec() are not needed.
 */

static int
xbox_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t op, void *arg, void *result)
{
	dev_info_t *dipb;
	volatile u_int trash;

	switch (op) {

	/*
	 * Devices that share the same XBox SBus slot claim affinity.
	 */
	case DDI_CTLOPS_AFFINITY:
		dipb = (dev_info_t *)arg;
		if ((DEVI_PD(rdip) && sparc_pd_getnreg(rdip) > 0) &&
		    (DEVI_PD(dipb) && sparc_pd_getnreg(dipb) > 0)) {
			u_int slot = sparc_pd_getreg(rdip, 0)->regspec_bustype;
			u_int slot_b =
			    sparc_pd_getreg(dipb, 0)->regspec_bustype;
			if (slot == slot_b && (ddi_get_parent(rdip) ==
			    ddi_get_parent(dipb)))
				return (DDI_SUCCESS);
		}
		return (DDI_FAILURE);

	/*
	 * If we get to here, the poke has been attempted in
	 * the cpu's world.  So, before we do any flushing of
	 * our own, ensure that our parents have flushed out
	 * all their write buffers.
	 */
	case DDI_CTLOPS_POKE_FLUSH:
		if (ddi_ctlops(dip, rdip, op, arg, result) == DDI_SUCCESS) {
			/*
			 * The XBox guarantees to flush out it's write
			 * pipe (and thus get the stuff to the hardware)
			 * by reading back from *any* location. So we read
			 * back from the write0 location (note that we
			 * *depend* upon there being a valid SBus card in
			 * slot0!)
			 */
			trash = *(((struct xbox_state *)
			    ddi_get_driver_private(dip))->xac_xc.xac_write0);
#ifdef lint
			trash = trash;
#endif	/* lint */
			return (DDI_SUCCESS);
		}
		return (DDI_FAILURE);

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}

/*
 * Leaf operations (diagnostic access)
 * not much to do here.
 */
static int
xbox_open(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	register int instance = getminor(*dev_p);
	struct xbox_state *xac;

#ifdef lint
	flag = flag;
	cred_p = cred_p;
#endif	/* lint */

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if ((xac = (struct xbox_state *)ddi_get_soft_state(xbh, instance)) ==
	    NULL) {
		return (ENXIO);
	}

	XPRINTF(xac, "xboxopen, flags=%x\n", flag);

	xac->xac_state |= XAC_STATE_OPEN;

	/*
	 * we assume that we don't want to panic on errors and that the
	 * the user will take care of xbox errors now
	 */
	xac->xac_soft_status.xac_action_on_error = ACTION_CONT;

	XPRINTF_DEBUG(xac, "xbox opened\n");
	return (0);
}


/*
 * xbox_close: not much to do either
 * Just to be sure we reset the action_on_error to PANIC on last close
 */
static int
xbox_close(dev_t dev_p, int flag, int otyp, cred_t *cred_p)
{
	register int instance = getminor(dev_p);
	struct xbox_state *xac;

#ifdef lint
	flag = flag;
	cred_p = cred_p;
#endif	/* lint */

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if ((xac = (struct xbox_state *)ddi_get_soft_state(xbh, instance)) ==
	    NULL) {
		return (ENXIO);
	}

	XPRINTF(xac, "xboxclose, flags=%x\n", flag);

	xac->xac_state &= ~XAC_STATE_OPEN;

out:
	xac->xac_soft_status.xac_action_on_error = ACTION_PANIC;
	XPRINTF_DEBUG(xac, "xbox closed\n");
	return (0);
}

/*
 * xboxioctl: mostly for sundiag
 */
static int
xbox_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *cred_p, int *rval_p)
{
	int instance = getminor(dev);
	struct xbox_state *xac = ddi_get_soft_state(xbh, instance);
	int rval = 0;


	XPRINTF(xac, "xbox ioctl: cmd=%x, arg=%x, mode=%x\n",
	    cmd, arg, mode);

#ifdef lint
	cred_p = cred_p;
	rval_p = rval_p;
#endif /* lint */

	mutex_enter(XAC_MUTEX);

	switch (cmd) {
	case XAC_RESET:
		{
			/*
			 * assert reset hw according to type for length
			 * specified and reinitialize
			 */
			struct xac_ioctl_reset reset;

			if (copyin((caddr_t)arg, (caddr_t)&reset,
			    sizeof (struct xac_ioctl_reset))) {
				rval = EFAULT;
			} else {
				xbox_reset(xac, reset.xac_reset_type,
				    reset.xac_reset_length);
			}
		}
		break;

	case XAC_REG_CHECK:
		/*
		 * switch to non-transparent and check regs
		 */
		rval = xbox_reg_check(xac);
		break;

	case XAC_TRANSPARENT:
		xbox_transparent(xac);
		break;

	case XAC_NON_TRANSPARENT:
		rval = xbox_non_transparent(xac);
		break;

	case XAC_WRITE0:
		{
			struct xac_ioctl_write0 write0;

			if (copyin((caddr_t)arg, (caddr_t)&write0,
			    sizeof (struct xac_ioctl_write0))) {
				rval = EFAULT;
			} else {
				xbox_write0(xac, write0.xac_address,
				    write0.xac_data);
			}
		}
		break;

#ifdef XBOX_DEBUG
	case XAC_DUMP_REGS:
		if (xbox_non_transparent(xac)) {
			rval = EIO;
			break;
		}
		xbox_dump_csr(xac, NULL);
		xbox_transparent(xac);
		break;
#endif

	case XAC_GET_REG_VALUES:
		{
			/*
			 * get shadow copy of regs so they can be compared
			 * with the error pkt values
			 */
			struct xac_ioctl_get_reg_values xacregs;

			xacregs.xac_ctl0 = xac->xac_ctl0;
			xacregs.xbc_ctl0 = xac->xbc_ctl0;
			xacregs.xac_epkt_dma_addr =
				xac->xac_epkt_dma_addr;
			xacregs.xbc_epkt_dma_addr =
				xac->xbc_epkt_dma_addr;
			if (copyout((caddr_t)&xacregs,
			    (caddr_t)arg,
			    sizeof (struct xac_ioctl_get_reg_values))) {
				rval = EFAULT;
			}
		}
		break;

	case XAC_WAIT_FOR_ERROR_PKT:
		{
			struct xac_ioctl_wait_for_error xacwait;

			/*
			 * wait for error pkt for specified time
			 */
			if (copyin((caddr_t)arg, (caddr_t)&xacwait,
			    sizeof (struct xac_ioctl_wait_for_error))) {
				rval = EFAULT;
				break;
			}
			rval = xbox_wait_for_error(xac,
			    &xacwait.xac_errpkt,
			    xacwait.xac_timeout);
			if (copyout((caddr_t)&xacwait, (caddr_t)arg,
			    sizeof (struct xac_ioctl_wait_for_error))) {
					rval = EFAULT;
			}
		}
		break;

	case XAC_GET_ERROR_PKT:
		{
			struct xc_errs *xac_saved_pkt = &xac->xac_saved_epkt;
			struct xc_errs *xbc_saved_pkt = &xac->xbc_saved_epkt;

			/*
			 * errd may still be 0 so we check for erra
			 */
			if (xac_saved_pkt->xc_erra) {
				XPRINTF_DEBUG(xac,
				    "xac error packet available\n");
				if (copyout((caddr_t)xac_saved_pkt,
				    (caddr_t)arg,
				    sizeof (struct xc_errs))) {
					rval = EFAULT;
				}
				bzero((caddr_t)xac_saved_pkt,
				    sizeof (struct xc_errs));

			} else if (xbc_saved_pkt->xc_erra) {
				XPRINTF_DEBUG(xac,
				    "xbc error packet available\n");
				if (copyout((caddr_t)xbc_saved_pkt,
				    (caddr_t)arg,
				    sizeof (struct xc_errs))) {
					rval = EFAULT;
				}
				bzero((caddr_t)xbc_saved_pkt,
				    sizeof (struct xc_errs));
			} else {
				struct xc_errs nothing;

				bzero((caddr_t)&nothing,
				    sizeof (struct xc_errs));
				if (copyout((caddr_t)&nothing,
				    (caddr_t)arg,
				    sizeof (struct xc_errs))) {
					rval = EFAULT;
				}
			}
		}
		break;

	case XAC_CLEAR_WAIT_FOR_ERROR:
		cv_signal(XAC_CV);
		break;

	default:
		rval = ENOTTY;
	}

	mutex_exit(XAC_MUTEX);
	XPRINTF_DEBUG(xac, "xboxioctl: rval = %x\n", 0);
	return (rval);
}

/*
 * switch over to non-transparent mode
 * we need to map in a page for almost each register
 * the write0 register is permanently mapped in.
 */
static int
xbox_non_transparent(struct xbox_state *xac)
{
	caddr_t *xc = (caddr_t *)&xac->xac_xc;
	dev_info_t *devi = xac->xac_dev_info;
	int i;

	if ((xac->xac_state & XAC_STATE_TRANSPARENT) == 0) {
		XPRINTF_DEBUG(xac, "already non-transparent\n");
		return (0);
	}

	xc++;
	for (i = 1; i < N_XBOX_REGS; i++, xc++) {

		ASSERT(*xc == 0);

		if (ddi_map_regs(devi, i, xc, 0, 0) != DDI_SUCCESS) {
			xprintf(xac, "cannot map reg set #%d\n", i);
			return (-1);
		}
	}


	xbox_write0(xac, XBC_CTL1_OFFSET, 0);
	xbox_write0(xac, XAC_CTL1_OFFSET, 0);

	xac->xac_state &= ~XAC_STATE_TRANSPARENT;
	XPRINTF(xac, "xbox is now non_transparent\n");

	return (0);
}


/*
 * switch over to transparent mode
 * unmap all registers if they were mapped in
 */
static void
xbox_transparent(struct xbox_state *xac)
{
	caddr_t *xc = (caddr_t *)&xac->xac_xc;
	dev_info_t *devi = xac->xac_dev_info;
	int i;

	if (xac->xac_state & XAC_STATE_TRANSPARENT) {
		XPRINTF(xac, "already transparent\n");
		return;
	}

	xc++;
	for (i = 1; i < N_XBOX_REGS; i++, xc++) {
		ddi_unmap_regs(devi, i, xc, 0L, 0L);
		*xc = 0;
	}

	xbox_init(xac, XBOX_DO_ERRLOG_ENABLE);
	xac->xac_state |= XAC_STATE_TRANSPARENT;
	XPRINTF(xac, "xbox is now transparent\n");
}


#ifdef XBOX_DEBUG
/*
 * dump all registers
 */
static void
xbox_dump_csr(struct xbox_state *xac, char *s)
{
	struct xc *xc = &xac->xac_xc;

	if (s)
		xprintf(xac, "csr dump %s:\n", s);
	xprintf(0, "xac errd: status = 0x%b, xbss=%x\n",
		xc->xac_errs->xc_errd & ERRD_MASK, ERRD_STAT_BITS,
		(xc->xac_errs->xc_errd & ERRD_XBSS) >> 24);
	xprintf(0, "xac pktype = %x, sbsiz = %x\n",
		(xc->xac_errs->xc_errd & ERRD_PKTYP) >> 21,
		(xc->xac_errs->xc_errd & ERRD_SBSIZ) >> 16);
	xprintf(0, "xac errd: esty = 0x%b, erra = 0x%x\n",
		xc->xac_errs->xc_errd & ESTY_MASK, ERRD_ESTY_BITS,
		xc->xac_errs->xc_erra);
	xprintf(0, "xac status = 0x%b\n",
		xc->xac_errs->xc_status, STAT_BITS);
	xprintf(0, "xac ctl0 zkey = %x, uadm = %x, ilvl = %x\n",
		(*(xc->xac_ctl0) & CTL0_ZKEY) >> 8,
		(*(xc->xac_ctl0) & CTL0_UADM) >> 3,
		*(xc->xac_ctl0) & CTL0_ILVL);
	xprintf(0, "xac ctl1 = 0x%b, SRST = %x, ELDS = %x\n",
		*(xc->xac_ctl1), CTL1_BITS,
		(*(xc->xac_ctl1) & CTL1_SRST) >> 12,
		(*(xc->xac_ctl1) & CTL1_ELDS) >> 6);
	xprintf(0, "xac error log addr = 0x%x\n\n",
		(*(xc->xac_elua) << 16 | *(xc->xac_ella)));

	xprintf(0, "xbc errd: status = 0x%b, xbss = %x\n",
		xc->xbc_errs->xc_errd & ERRD_MASK, ERRD_STAT_BITS,
		(xc->xbc_errs->xc_errd & ERRD_XBSS) >> 24);
	xprintf(0, "xbc pktype = %x, sbsiz = %x\n",
		(xc->xbc_errs->xc_errd & ERRD_PKTYP) >> 21,
		(xc->xbc_errs->xc_errd & ERRD_SBSIZ) >> 16);
	xprintf(0, "xbc errd: esty = 0x%b, erra = 0x%x\n",
		xc->xbc_errs->xc_errd & ESTY_MASK, ERRD_ESTY_BITS,
		xc->xbc_errs->xc_erra);
	xprintf(0, "xbc status = 0x%b\n",
		xc->xbc_errs->xc_status, STAT_BITS);
	xprintf(0, "xbc ctl0 zkey = %x, uadm = %x, ilvl = %x\n",
		(*(xc->xbc_ctl0) & CTL0_ZKEY) >> 8,
		(*(xc->xbc_ctl0) & CTL0_UADM) >> 3,
		*(xc->xbc_ctl0) & CTL0_ILVL);
	xprintf(0, "xbc ctl1 = 0x%b, SRST = %x, ELDS = %x\n",
		*(xc->xbc_ctl1), CTL1_BITS,
		(*(xc->xbc_ctl1) & CTL1_SRST) >> 12,
		(*(xc->xbc_ctl1) & CTL1_ELDS) >> 6);
	xprintf(0, "xbc error log addr = 0x%x\n",
		(*(xc->xbc_elua) << 16 | *(xc->xbc_ella)));
}
#endif

static int
compare(struct xbox_state *xac, char *what, u_int found, u_int expected)
{
	if (found != expected) {
		xprintf(xac, "%s: expecting 0x%x, found 0x%x\n",
			what, expected, found);
		return (-1);
	}
	return (0);
}


static int
xbox_compare_csr(struct xbox_state *xac)
{
	struct xc *xc = &xac->xac_xc;
	int result = 0;

	result += compare(xac, "xac errd",
	    xc->xac_errs->xc_errd & 0x8000ffff, 0x0);
	result += compare(xac, "xac status", xc->xac_errs->xc_status & 0xff,
		STAT_CRDY);
	result += compare(xac, "xac ctl0", *(xc->xac_ctl0), (u_int)
		xac->xac_ctl0);
	result += compare(xac, "xac ctl1", *(xc->xac_ctl1), (u_int) 0);
	result += compare(xac, "xac error log addr",
		(*(xc->xac_elua) << 16 | *(xc->xac_ella)),
		(u_int) xac->xac_epkt_dma_addr);

	result += compare(xac, "xbc errd", xc->xbc_errs->xc_errd,
		(u_int) 0xb00000);
	result += compare(xac, "xbc status", xc->xbc_errs->xc_status & 0xff,
		(u_int) 0x64);
	result += compare(xac, "xbc ctl0", *(xc->xbc_ctl0), (u_int)
		xac->xbc_ctl0);
	result += compare(xac, "xbc ctl1", *(xc->xbc_ctl1), 0);
	result += compare(xac, "xbc error log addr",
		(*(xc->xbc_elua) << 16 | *(xc->xbc_ella)),
		(u_int) xac->xbc_epkt_dma_addr);

	return (result);
}


/*
 * xbox register initialization
 */


static void
xbox_init(struct xbox_state *xac, int flag)
{
	register u_int ctl0;

	bzero((caddr_t)xac->xac_epkt, sizeof (struct xc_errs));
	bzero((caddr_t)xac->xbc_epkt, sizeof (struct xc_errs));

	bzero((caddr_t)&xac->xac_saved_epkt, sizeof (struct xc_errs));
	bzero((caddr_t)&xac->xbc_saved_epkt, sizeof (struct xc_errs));

	xac->xac_epkt->xc_errd = NO_PKT;
	xac->xbc_epkt->xc_errd = NO_PKT;

	/*
	 * give xbox hw address for dvma error packet
	 */
	xbox_write0(xac, XAC_ELLA_OFFSET,
		(u_int)xac->xac_epkt_dma_addr & 0xffff);
	xbox_write0(xac, XAC_ELUA_OFFSET,
		(u_int)((int)xac->xac_epkt_dma_addr >> 16) & 0xffff);
	xbox_write0(xac, XBC_ELLA_OFFSET,
		(u_int)xac->xbc_epkt_dma_addr & 0xffff);
	xbox_write0(xac, XBC_ELUA_OFFSET,
		((u_int)xac->xbc_epkt_dma_addr >> 16) & 0xffff);

	/*
	 * enable error logging
	 */
	if (flag == XBOX_DO_ERRLOG_ENABLE) {
		xbox_write0(xac, XAC_ERRLOG_ENABLE_OFFSET, 0);
		xbox_write0(xac, XBC_ERRLOG_ENABLE_OFFSET, 0);
	}

	/*
	 * program control register 0 and 1
	 */
	xbox_write0(xac, XAC_CTL1_OFFSET, XAC_CTL1);
	xbox_write0(xac, XBC_CTL1_OFFSET, XBC_CTL1);

	ctl0 = xac->xac_soft_status.xac_write0_key << 8 |
		xac->xac_soft_status.xac_uadm << 3 |
		(xac->xac_inumber + 1);
	xac->xac_ctl0 = ctl0;
	xbox_write0(xac, (u_int)XAC_CTL0_OFFSET, (u_int)ctl0);

	ctl0 = xac->xac_soft_status.xac_uadm << 3 |
		(xac->xac_inumber + 1);
	xac->xbc_ctl0 = ctl0;
	xbox_write0(xac, (u_int)XBC_CTL0_OFFSET, (u_int)ctl0);

	/*
	 * we are done and are now transparent
	 */
	xac->xac_state |= XAC_STATE_TRANSPARENT;
}


static void
xbox_uninit(struct xbox_state *xac)
{
	xbox_write0(xac, XAC_CTL0_OFFSET, xac->xac_ctl0 & ~7);
	xbox_write0(xac, XAC_CTL1_OFFSET, CTL1_TRAN);
	xbox_write0(xac, XBC_CTL0_OFFSET, 0);
	xbox_write0(xac, XBC_CTL1_OFFSET, 0);
	xbox_write0(xac, XAC_ELLA_OFFSET, 0);
	xbox_write0(xac, XAC_ELUA_OFFSET, 0);
	xbox_write0(xac, XBC_ELLA_OFFSET, 0);
	xbox_write0(xac, XBC_ELUA_OFFSET, 0);
	xac->xac_state = 0;
}

static int
xbox_check_for_boot_errors(struct xbox_state *xac)
{
	int i;
	int rval = -1;

	/*
	 * first check if ELDE in xbox_init() caused a pkt to be
	 * xferred
	 */
	if (xac->xac_epkt->xc_errd != NO_PKT) {
		xac->xac_saved_epkt = *xac->xac_epkt;
		xbox_dump_epkt(xac, XAC);
		if (xac->xac_epkt->xc_errd & ERRD_ESDB) {
			goto bad;
		}
		xac->xac_epkt->xc_errd = 0;
	}

	if (xac->xbc_epkt->xc_errd != NO_PKT) {
		xac->xbc_saved_epkt = *xac->xbc_epkt;
		xbox_dump_epkt(xac, XBC);
		if (xac->xbc_epkt->xc_errd & ERRD_ESDB) {
			goto bad;
		}
		xac->xbc_epkt->xc_errd = 0;
	}

	/*
	 * cause a DVMA xfer into the error packets to check for
	 * any more errors
	 * XXX this isn't really necessary since we seem to get the
	 * error anyway after an ELDE but I left the code in since
	 * it exercises the hw nicely
	 */
	xac->xac_epkt->xc_errd = NO_PKT;
	xbox_write0(xac, XAC_CTL1_OFFSET, CTL1_DVTE | XAC_CTL1);

	for (i = 0; i < XBOX_TIMEOUT; i++) {
		/*
		 * we should not get an interrupt and the
		 * error packet should not be cleared by
		 * xbox_check_intr
		 * if it did get cleared a real problem occurred
		 */
		if (xac->xac_epkt->xc_errd != NO_PKT) {
			struct xc_errs *p = &xac->xac_saved_epkt;

			XPRINTF_DEBUG(xac, "received packet\n");
			xac->xac_saved_epkt = *xac->xac_epkt;
			xac->xac_epkt->xc_errd = 0;
#ifdef XBOX_DEBUG
			if (xdebug > 1)
				xbox_dump_epkt(xac, XAC);
#endif /* XBOX_DEBUG */
			if (compare(xac, "xac errd",
			    (u_int) p->xc_errd & 0x8000ffff,
			    (u_int) 0)) {
				xbox_dump_epkt(xac, XAC);
				xprintf(xac, "xac hw errors during booting\n");
				goto bad;
			}
			if (compare(xac, "xac status",
			    (u_int) p->xc_status & 0xff,
			    (u_int) STAT_CRDY)) {
				xbox_dump_epkt(xac, XAC);
				xprintf(xac, "xac hw errors during booting\n");
				goto bad;
			}
			break;
		}
		drv_usecwait(10);
		XBOX_DMA_SYNC(xac);
	}

	if (i == XBOX_TIMEOUT) {
		xprintf(xac, "timeout on receiving xac error packet\n");
		goto bad;
	}

	xac->xbc_epkt->xc_errd = NO_PKT;
	xbox_write0(xac, XBC_CTL1_OFFSET, CTL1_DVTE | XBC_CTL1);

	for (i = 0; i < XBOX_TIMEOUT; i++) {
		/*
		 * we should not get an interrupt and the
		 * error packet should not be cleared by
		 * xbox_check_intr
		 * if it did get cleared a real problem occurred
		 */
		if (xac->xbc_epkt->xc_errd != NO_PKT) {
			struct xc_errs *p = &xac->xbc_saved_epkt;

			XPRINTF_DEBUG(xac, "received packet\n");
			xac->xbc_saved_epkt = *xac->xbc_epkt;
			xac->xbc_epkt->xc_errd = 0;
#ifdef XBOX_DEBUG
			if (xdebug > 1)
				xbox_dump_epkt(xac, XBC);
#endif /* XBOX_DEBUG */
			if (compare(xac, "xbc errd",
			    (u_int) p->xc_errd & 0x8000ffff,
			    (u_int) 0)) {
				xbox_dump_epkt(xac, XBC);
				xprintf(xac, "xbc hw errors during booting\n");
				goto bad;
			}
			break;
		}
		drv_usecwait(10);
		XBOX_DMA_SYNC(xac);
	}

	if (i == XBOX_TIMEOUT) {
		xprintf(xac, "timeout on receiving xbc error packet\n");
		goto bad;
	}

	rval = 0;
	return (rval);
bad:
	xbox_write0(xac, XAC_ERRLOG_ENABLE_OFFSET, 0);
	xbox_write0(xac, XBC_ERRLOG_ENABLE_OFFSET, 0);
	return (rval);
}

/*
 * check status in non-transparent mode
 */
static int
xbox_check_status(struct xbox_state *xac)
{
	int rval = 0;
	struct xc *xc = &xac->xac_xc;

	if (xc->xac_errs->xc_errd & ERRD_ESDB) {
		xprintf(xac, "xac hw error:\n");
		xprintf(0, "xac errd: status = 0x%b, xbss=%x\n",
			xc->xac_errs->xc_errd & ERRD_MASK, ERRD_STAT_BITS,
			(xc->xac_errs->xc_errd & ERRD_XBSS) >> 24);
		xprintf(0, "pktype = %x, sbsiz=%x\n",
			(xc->xac_errs->xc_errd & ERRD_PKTYP) >> 21,
			(xc->xac_errs->xc_errd & ERRD_SBSIZ) >> 16);
		xprintf(0, "xac errd: esty= 0x%b, erra=0x%x\n",
			xc->xac_errs->xc_errd & ESTY_MASK, ERRD_ESTY_BITS,
			xc->xac_errs->xc_erra);
		xprintf(0, "xac status= 0x%b\n",
			xc->xac_errs->xc_status, STAT_BITS);
		rval = -1;
	}
	if (xc->xbc_errs->xc_errd & ERRD_ESDB) {
		xprintf(xac, "xbc hw error:\n");
		xprintf(0, "xbc errd: status = 0x%b, xbss=%x\n",
			xc->xbc_errs->xc_errd & ERRD_MASK, ERRD_STAT_BITS,
			(xc->xbc_errs->xc_errd & ERRD_XBSS) >> 24);
		xprintf(0, "pktype = %x, sbsiz=%x\n",
			(xc->xbc_errs->xc_errd & ERRD_PKTYP) >> 21,
			(xc->xbc_errs->xc_errd & ERRD_SBSIZ) >> 16);
		xprintf(0, "xbc errd: esty= 0x%b, erra=0x%x\n",
			xc->xbc_errs->xc_errd & ESTY_MASK, ERRD_ESTY_BITS,
			xc->xbc_errs->xc_erra);
		xprintf(0, "xbc status= 0x%b\n",
			xc->xbc_errs->xc_status, STAT_BITS);
		rval = -1;
	}
	return (rval);
}


static void
xbox_dump_epkt(struct xbox_state *xac, int who)
{
	struct xc_errs *p = ((who == XAC)?
		&xac->xac_saved_epkt : &xac->xbc_saved_epkt);

#ifdef XBOX_DEBUG
	xprintf(xac, "error packet from %s:\n", (who == XAC)? "xac" : "xbc");
	xprintf(0, "errd: status = 0x%b, xbss=%x\n\tpktype = %x, sbsiz=%x\n",
		p->xc_errd & ERRD_MASK, ERRD_STAT_BITS,
		(p->xc_errd & ERRD_XBSS) >> 24,
		(p->xc_errd & ERRD_PKTYP) >> 21,
		(p->xc_errd & ERRD_SBSIZ) >> 16);
	xprintf(0, "esty= 0x%b\n\terra=0x%x, status= 0x%b\n",
		p->xc_errd & ESTY_MASK, ERRD_ESTY_BITS,
		p->xc_erra,
		p->xc_status, STAT_BITS);
	xprintf(0, "ctl0 = 0x%x\n", p->xc_ctl0);
	if (who == XAC && ((p->xc_status & STAT_CRDY) == 0)) {
		xprintf(xac, "power failure or no cable plugged in\n");
	}
#else
	xprintf(xac, "error status type = 0x%b\n",
		p->xc_errd & ESTY_MASK, ERRD_ESTY_BITS);
	xprintf(0, "error address=0x%x, status= 0x%b\n",
		p->xc_erra, p->xc_status, STAT_BITS);
	if (who == XAC && ((p->xc_status & STAT_CRDY) == 0)) {
		xprintf(xac, "power failure or no cable plugged in\n");
	}
#endif
}


static void
xbox_reset(struct xbox_state *xac, u_int type, u_int length)
{
	struct xc *xc = &xac->xac_xc;

	(void) xbox_non_transparent(xac);
	*(xc->xac_ctl1) = type;
	drv_usecwait(length);
	*(xc->xac_ctl1) = 0;
	drv_usecwait(100);

	*(xc->xac_ctl0) = xac->xac_ctl0;
	*(xc->xac_ctl1) = XAC_CTL1;

	xbox_transparent(xac);
}



static int
xbox_reg_check(struct xbox_state *xac)
{
	int rval = 0;
	struct xc *xc = &xac->xac_xc;

	/*
	 * reinitialize to get back in a normal state
	 */
	xbox_init(xac, XBOX_DO_ERRLOG_ENABLE);

	/*
	 * switch to non-transparent mode, dump registers, and
	 * and check for errors
	 */
	XPRINTF(xac, "non transparent test\n");
	if (xbox_non_transparent(xac)) {
		XPRINTF_DEBUG(xac,
		    "cannot switch to non-transparent mode\n");
		rval = -1;
		goto exit;
	}

	if (xbox_check_status(xac)) {
		xc->xac_errs->xc_errd &= ~ERRD_ESDB;
		xc->xbc_errs->xc_errd &= ~ERRD_ESDB;
		rval = -1;
	}

#ifdef XBOX_DEBUG
	if (xdebug)
		xbox_dump_csr(xac, "xbox_test");
#endif /* XBOX_DEBUG */

	if (xbox_compare_csr(xac)) {
		rval = -1;
	}

	xbox_transparent(xac); /* will also init */

exit:
	if (rval) {
		xprintf(xac, "xbox test failed, resetting xbox\n");
		xbox_reset(xac, SRST_HRES, 10);
	}
	return (rval);
}


static int
xbox_wait_for_error(struct xbox_state *xac, struct xc_errs *errpkt_p,
    clock_t timout)
{
	struct xc_errs *xac_saved_pkt = &xac->xac_saved_epkt;
	struct xc_errs *xbc_saved_pkt = &xac->xbc_saved_epkt;
	clock_t time;

	XPRINTF_DEBUG(xac, "xbox_wait_for_error: %x %x\n", errpkt_p, timout);

	/*
	 * first check if there is still a saved pkt
	 */
	for (;;) {
		if (xac_saved_pkt->xc_errd & ERRD_ESDB) {
			XPRINTF_DEBUG(xac, "xac error packet available\n");
			*errpkt_p = *xac_saved_pkt;
			bzero((caddr_t)xac_saved_pkt,
			    sizeof (struct xc_errs));
			break;
		}
		if (xbc_saved_pkt->xc_errd & ERRD_ESDB) {
			XPRINTF_DEBUG(xac, "xbc error packet available\n");
			*errpkt_p = *xbc_saved_pkt;
			bzero((caddr_t)xbc_saved_pkt,
			    sizeof (struct xc_errs));
			break;
		}

		if (timout) {
			clock_t value = 0;

			(void) drv_getparm(LBOLT, &value);
			time = value + timout * hz;
			XPRINTF_DEBUG(xac, "time=%x, value=%x\n",
				time, value);
			(void) cv_timedwait(&xac->xac_cv, XAC_MUTEX, time);
			XPRINTF_DEBUG(xac, "returned from cv_timedwait\n");
			if (xac_saved_pkt->xc_errd & ERRD_ESDB) {
				XPRINTF_DEBUG(xac,
					"xac error packet now available\n");
				*errpkt_p = *xac_saved_pkt;
				bzero((caddr_t)xac_saved_pkt,
				    sizeof (struct xc_errs));
			} else if (xbc_saved_pkt->xc_errd & ERRD_ESDB) {
				XPRINTF_DEBUG(xac,
				    "xbc error packet now available\n");
				*errpkt_p = *xbc_saved_pkt;
				bzero((caddr_t)xbc_saved_pkt,
				    sizeof (struct xc_errs));
			} else {
				XPRINTF_DEBUG(xac,
				    "no error packet available\n");
				bzero((caddr_t)xbc_saved_pkt,
				    sizeof (struct xc_errs));
				*errpkt_p = *xbc_saved_pkt;
			}
			break;

		} else if (cv_wait_sig(&xac->xac_cv, XAC_MUTEX) == 0) {
			XPRINTF_DEBUG(xac, "interrupted\n");
			bzero((caddr_t)errpkt_p, sizeof (struct xc_errs));
			break;
		}
	}
	return (0);
}


#include <sys/varargs.h>

/*VARARGS*/
static void
xprintf(struct xbox_state *xac, const char *fmt, ...)
{
	auto char buf[256];
	va_list ap;

	if (xac) {
		cmn_err(CE_CONT, "xbox%d:\t", xac->xac_instance);
	} else {
		cmn_err(CE_CONT, "\t");
	}
	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	cmn_err(CE_CONT, "%s", buf);
}
