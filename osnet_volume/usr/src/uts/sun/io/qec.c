/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)qec.c 1.32	99/04/22 SMI"

/*
 * SunOS MT STREAMS Nexus Device Driver for QuadMACE and BigMAC
 * Ethernet deives.
 */

#include	<sys/types.h>
#include	<sys/debug.h>
#include	<sys/stropts.h>
#include	<sys/stream.h>
#include	<sys/cmn_err.h>
#include	<sys/vtrace.h>
#include	<sys/kmem.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/strsun.h>
#include	<sys/stat.h>
#include	<sys/kstat.h>
#include	<sys/modctl.h>
#include	<sys/ethernet.h>
#include	<sys/qec.h>

#define	QECLIMADDRLO	(0x00000000U)
#define	QECLIMADDRHI	(0xffffffffU)
#define	QECBUFSIZE	1024	/* size of dummy buffer for dvma burst */
#define	QECBURSTSZ	(0x7f)
/*
 * Function prototypes.
 */
static	int qecattach(dev_info_t *, ddi_attach_cmd_t);
static	int qecdetach(dev_info_t *, ddi_detach_cmd_t);
static	u_int qecintr(caddr_t arg);
static	u_int qecgreset(volatile struct qec_global *qgp);
static	int qecburst(dev_info_t *);
static	void qecerror(dev_info_t *dip, char *fmt, ...);
static	int qecinit(dev_info_t *);

static struct cb_ops qec_cb_ops = {
	nodev,			/* open		*/
	nodev,			/* close	*/
	nodev,			/* strategy	*/
	nodev,			/* print	*/
	nodev,			/* dump		*/
	nodev,			/* read		*/
	nodev,			/* write	*/
	nodev,			/* ioctl	*/
	nodev,			/* devmap	*/
	nodev,			/* mmap		*/
	nodev,			/* segmap	*/
	nochpoll,		/* chpoll	*/
	nopropop, 		/* cb_prop_op	*/
	0,			/* stream tab	*/
	D_NEW | D_MP | D_HOTPLUG, /* compat flags */
	CB_REV,			/* rev		*/
	nodev,			/* int (*cb_aread)() */
	nodev			/* int (*cb_awrite)() */
};

static struct bus_ops qec_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,		/* bus_map */
	i_ddi_get_intrspec,	/* bus_get_intrspec */
	i_ddi_add_intrspec,	/* bus_add_intrspec */
	i_ddi_remove_intrspec,	/* bus_remove_intrspec */
	i_ddi_map_fault,	/* bus_map_fault */
	ddi_dma_map,		/* bus_dma_map */
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,		/* bus_dma_ctl */
	ddi_ctlops,		/* bus_ctl */
	ddi_bus_prop_op,	/* bus_prop_op */
	0,			/* (*bus_get_eventcookie)();	*/
	0,			/* (*bus_add_eventcall)();	*/
	0,			/* (*bus_remove_eventcall)();	*/
	0,			/* (*bus_post_event)();		*/
	i_ddi_intr_ctlops
};

/*
 * Device ops - copied from dmaga.c .
 */
static struct dev_ops qec_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ddi_no_info,		/* devo_info */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	qecattach,		/* devo_attach */
	qecdetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&qec_cb_ops,		/* driver operations */
	&qec_bus_ops		/* bus operations */
};

/*
 * Claim the device is ultra-capable of burst in the beginning.  Use
 * the value returned by ddi_dma_burstsizes() to actually set the QEC
 * global control register later.
 */
static ddi_dma_lim_t qec_dma_limits = {
	QECLIMADDRLO,	/* dlim_addr_lo */
	QECLIMADDRHI,	/* dlim_addr_hi */
	QECLIMADDRHI,	/* dlim_cntr_max */
	QECBURSTSZ,	/* dlim_burstsizes */
	0x1,		/* dlim_minxfer */
	1024		/* dlim_speed */
};

/*
 * Single private "global" lock for the few rare conditions
 * we want single-threaded.
 */
kmutex_t	qeclock;

/*
 * Patchable packet size for BigMAC.
 */
static	u_int	qecframesize = 0;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"QEC driver v1.32",	/* Name of the module. */
	&qec_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

int
_init(void)
{
	int	status;

	mutex_init(&qeclock, NULL, MUTEX_DRIVER, NULL);
	status = mod_install(&modlinkage);
	if (status != 0) {
		mutex_destroy(&qeclock);
		return (status);
	}
	return (status);
}

int
_fini(void)
{
	int	status;

	status = mod_remove(&modlinkage);
	if (status != 0)
		return (status);
	mutex_destroy(&qeclock);
	return (status);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
qecattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct qec_soft *qsp = NULL;
	volatile struct qec_global *qgp = NULL;
	ddi_iblock_cookie_t	c;
	caddr_t parp = NULL;
	void (**funp)() = NULL;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		qsp = (struct qec_soft *)ddi_get_driver_private(dip);
		if (qsp != (struct qec_soft *)NULL) {
			/*
			 * Add interrupt to the system in case of QED. BigMAC
			 * registers its own interrupt. Register one interrupt
			 * per qec (not one interrupt per channel).
			 */
			qgp = qsp->qs_globregp;
			if ((qgp->control & QECG_CONTROL_MODE) ==
			    QECG_CONTROL_MACE) {
				if (ddi_add_intr(dip, 0, &qsp->qs_cookie, 0,
				    qecintr, (caddr_t)qsp)) {
					qecerror(dip, "ddi_add_intr() FAILED");
					return (DDI_FAILURE);
				}
			}
		}
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_slaveonly(dip) == DDI_SUCCESS) {
		qecerror(dip, "qec in slave only slot");
		return (DDI_FAILURE);
	}
	/*
	 * Allocate soft data structure
	 */
	qsp = kmem_zalloc(sizeof (struct qec_soft), KM_SLEEP);

	if (ddi_map_regs(dip, 0, (caddr_t *)&qgp, 0, 0)) {
		qecerror(dip, "unable to map registers");
		goto bad;
	}

	if (((qgp->control & QECG_CONTROL_MODE) != QECG_CONTROL_MACE) &&
		((qgp->control & QECG_CONTROL_MODE) != QECG_CONTROL_BMAC)) {
		qecerror(dip, "unknown chip identification");
		goto bad;
	}

	qsp->qs_globregp = qgp;
	qsp->qs_nchan = ddi_getprop(DDI_DEV_T_NONE, dip, 0, "#channels", 0);

	if (qecgreset(qgp)) {
		qecerror(dip, "global reset failed");
		goto bad;
	}

	/*
	 * Allocate location to save function pointers that will be
	 * exported to the child.
	 * Export a pointer to the qec_soft structure.
	 */
	parp = kmem_alloc(sizeof (void *) * qsp->qs_nchan, KM_SLEEP);
	qsp->qs_intr_arg = (void **)parp;
	funp = (void (**)()) kmem_alloc(sizeof (void *) * qsp->qs_nchan,
		KM_SLEEP);
	qsp->qs_intr_func = funp;
	qsp->qs_reset_func = qecgreset;
	qsp->qs_reset_arg = (void *) qgp;
	qsp->qs_init_func = qecinit;
	qsp->qs_init_arg = dip;
	ddi_set_driver_private(dip, (caddr_t)qsp);

	if (qecinit(dip))
		goto bad;

	/*
	 * Add interrupt to the system in case of QED. BigMAC registers
	 * its own interrupt. Register one interrupt per qec
	 * (not one interrupt per channel).
	 */
	if ((qgp->control & QECG_CONTROL_MODE) == QECG_CONTROL_MACE) {
		if (ddi_add_intr(dip, 0, &c, 0, qecintr, (caddr_t)qsp)) {
			qecerror(dip, "ddi_add_intr failed");
			goto bad;
		}
	qsp->qs_cookie = c;
	}

	ddi_report_dev(dip);
	return (DDI_SUCCESS);

bad:
	if (qsp)
		kmem_free((caddr_t)qsp, sizeof (*qsp));
	if (parp)
		kmem_free((caddr_t)parp, sizeof (*parp));
	if (funp)
		kmem_free((caddr_t)funp, sizeof (*funp));
	if (qgp)
		ddi_unmap_regs(dip, 0, (caddr_t *)&qgp, 0, 0);

	return (DDI_FAILURE);
}


static int
qecdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct qec_soft *qsp = NULL;
	volatile struct qec_global *qgp = NULL;
	ddi_iblock_cookie_t	c;
	caddr_t parp = NULL;
	void (**funp)() = NULL;

	qsp = (struct qec_soft *)ddi_get_driver_private(dip);

	switch (cmd) {
	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		/*
		 * We normally would set the driver suspended
		 * but we have no driver state flag.
		 */
		if (qsp != (struct qec_soft *)NULL) {
			qgp = qsp->qs_globregp;
			if ((qgp->control & QECG_CONTROL_MODE) ==
			    QECG_CONTROL_MACE)
				(void) ddi_remove_intr(dip, 0, qsp->qs_cookie);
		} else
			(void) ddi_remove_intr(dip, 0,
					    (ddi_iblock_cookie_t)NULL);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/*
	 * get soft data structure
	 */
	ddi_set_driver_private(dip, NULL);
	if (qsp != (struct qec_soft *)NULL) {
		qgp = qsp->qs_globregp;
		parp = (caddr_t)qsp->qs_intr_arg;
		funp = qsp->qs_intr_func;
		c = qsp->qs_cookie;

		if (parp)
			kmem_free((caddr_t)parp,
					sizeof (void *)* qsp->qs_nchan);
		if (funp)
			kmem_free((caddr_t)funp,
					sizeof (void *)* qsp->qs_nchan);
		kmem_free((caddr_t)qsp, sizeof (struct qec_soft));
		if (qgp)
			ddi_unmap_regs(dip, 0, (caddr_t *)&qgp, 0, 0);
		(void) ddi_remove_intr(dip, 0, c);
	}
	else
		(void) ddi_remove_intr(dip, 0, (ddi_iblock_cookie_t)NULL);
	/*
	 * remove all driver properties
	 */
	ddi_prop_remove_all(dip);

	return (DDI_SUCCESS);
}


#define	QEC_PHYRST_PERIOD	500
/*
 * QEC global reset.
 */
static u_int
qecgreset(volatile struct qec_global *qgp)
{
	volatile	u_int	*controlp;
	int	n;

	controlp = &(qgp->control);
	*controlp = QECG_CONTROL_RST;
	n = 1000;
	while (--n > 0) {
		drv_usecwait((clock_t)QEC_PHYRST_PERIOD);
		if ((*controlp & QECG_CONTROL_RST) == 0)
			return (0);
	}
	return (1);
}

static u_int
qecintr(caddr_t arg)
{
	int	i;
	volatile	u_int	global;
	u_int	serviced = 0;
	struct qec_soft	*qsp = (struct qec_soft *)arg;

	global = qsp->qs_globregp->status;

	/*
	 * this flag is set in qeinit() - fix for the fusion race condition
	 * - so that we claim any interrupt that was pending while
	 * the chip was being initialized. - see bug 1204247.
	 */

	if (qsp->qe_intr_flag) {
		serviced = 1;
		qsp->qe_intr_flag = 0;
		return (serviced);
	}

	/*
	 * Return if no bits set in global status register.
	 */

	if (global == 0)
		return (0);

	/*
	 * Service interrupting events for each channel.
	 */
	for (i = 0; i < qsp->qs_nchan; i++)
		if (QECCHANBITS(global, i) != 0)
			(*qsp->qs_intr_func[i])(qsp->qs_intr_arg[i]);

	serviced = 1;
	return (serviced);


}

/*
 * Calculate the dvma burst by setting up a dvma temporarily.  Return
 * 0 as burstsize upon failure as it signifies no burst size.
 */
static int
qecburst(dev_info_t *dip)
{
	caddr_t	addr;
	u_int size;
	int burst;
	ddi_dma_handle_t handle;

	size = QECBUFSIZE;
	addr = kmem_alloc(size, KM_SLEEP);
	if (ddi_dma_addr_setup(dip, (struct as *)0, addr, size,
		DDI_DMA_RDWR, DDI_DMA_DONTWAIT, 0, &qec_dma_limits, &handle)) {
		qecerror(dip, "ddi_dma_addr_setup failed");
		return (0);
	}
	burst = ddi_dma_burstsizes(handle);
	(void) ddi_dma_free(handle);
	kmem_free(addr, size);
	if (burst & 0x40)
		burst = QECG_CONTROL_BURST64;
	else if (burst & 0x20)
		burst = QECG_CONTROL_BURST32;
	else
		burst = QECG_CONTROL_BURST16;

	return (burst);
}

/*VARARGS*/
static void
qecerror(dev_info_t *dip, char *fmt, ...)
{
	char		msg_buffer[255];
	va_list	ap;

	mutex_enter(&qeclock);

	va_start(ap, fmt);
	(void) vsprintf(msg_buffer, fmt, ap);
	cmn_err(CE_CONT, "%s%d: %s\n", ddi_get_name(dip),
					ddi_get_instance(dip),
					msg_buffer);
	va_end(ap);

	mutex_exit(&qeclock);
}

static int
qecinit(dip)
dev_info_t	*dip;
{
	struct qec_soft *qsp;
	volatile	struct qec_global *qgp;
	off_t memsize;
	int burst;
	auto struct regtype {
		u_int hiaddr, loaddr, size;
	} regval[2];
	int reglen;

	if ((qsp = (struct qec_soft *)ddi_get_driver_private(dip)) == NULL) {
		qecerror(dip, "Failed to get driver private data");
		return (1);
	}
	qgp = qsp->qs_globregp;

	reglen = 2 * sizeof (struct regtype);
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		"reg", (caddr_t)&regval, &reglen) != DDI_PROP_SUCCESS) {
		qecerror(dip, "missing reg property!");
		return (DDI_FAILURE);
	}
	memsize = regval[1].size;

	qsp->qs_memsize = (u_int) memsize;

	/*
	 * Initialize all the QEC global registers.
	 */
	burst = qecburst(dip);
	qgp->control = burst;
	qgp->packetsize = qecframesize;
	qgp->memsize = memsize / qsp->qs_nchan;
	qgp->rxsize = qgp->txsize = (memsize / qsp->qs_nchan) / 2;
	return (0);
}
