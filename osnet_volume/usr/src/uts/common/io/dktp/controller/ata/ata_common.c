/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ata_common.c	1.72	99/12/06 SMI"

#if defined(i386)
#define	_mca_bus_supported
#define	_eisa_bus_supported
#define	_isa_bus_supported
#endif

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/debug.h>
#include <sys/promif.h>
#include <sys/nvm.h>
#include <sys/eisarom.h>
#include <sys/pci.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/uio.h>
#include <sys/cred.h>


#include "ata_common.h"
#include "ata_disk.h"
#include "atapi.h"
#include "ata_blacklist.h"

/*
 * Solaris Entry Points.
 */

static	int	ata_identify(dev_info_t *dip);
static	int	ata_probe(dev_info_t *dip);
static	int	ata_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static	int	ata_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static	int	ata_bus_ctl(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t o,
			    void *a, void *v);
static	u_int	ata_intr(caddr_t arg);

/*
 * GHD Entry points
 */

static	int	ata_get_status(void *hba_handle, void *intr_status);
static	void	ata_process_intr(void *hba_handle, void *intr_status);
static	int	ata_hba_start(void *handle, gcmd_t *gcmdp);
static	void	ata_hba_complete(void *handle, gcmd_t *gcmdp, int do_callback);
static	int	ata_timeout_func(void *hba_handle, gcmd_t  *gcmdp,
			gtgt_t *gtgtp, gact_t  action, int calltype);

/*
 * Local Function Prototypes
 */

static	int	ata_ctlr_fsm(uchar_t fsm_func, ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp,
				int *DoneFlgp);
static	void	ata_destroy_controller(dev_info_t *dip);
static	int	ata_drive_type(uchar_t drvhd,
			ddi_acc_handle_t io_hdl1, caddr_t ioaddr1,
			ddi_acc_handle_t io_hdl2, caddr_t ioaddr2,
			struct ata_id *ata_id_bufp);
static	ata_ctl_t *ata_init_controller(dev_info_t *dip);
static	ata_drv_t *ata_init_drive(ata_ctl_t *ata_ctlp,
			uchar_t targ, uchar_t lun);
static	int	ata_init_drive_pcidma(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp);
static	int	ata_flush_cache(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp);
static	void	ata_init_pciide(dev_info_t *dip, ata_ctl_t *ata_ctlp);
static	int	ata_reset_bus(ata_ctl_t *ata_ctlp);
static	int	ata_setup_ioaddr(dev_info_t *dip,
			ddi_acc_handle_t *iohandle1, caddr_t *ioaddr1p,
			ddi_acc_handle_t *iohandle2, caddr_t *ioaddr2p,
			ddi_acc_handle_t *bm_hdlp, caddr_t *bm_addrp);
static	int	ata_software_reset(ata_ctl_t *ata_ctlp);
static	int	ata_start_arq(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_strncmp(char *p1, char *p2, int cnt);
static	void	ata_uninit_drive(ata_drv_t *ata_drvp);

static	int	ata_check_pciide_blacklist(dev_info_t *dip, uint_t flags);

#if defined(_eisa_bus_supported)
static	int	ata_check_addr(NVM_PORT *ports, int ioaddr);
static	int	ata_detect_dpt(dev_info_t *dip, int ioaddr);
#endif

/*
 * Local static data
 */
static	void	*ata_state;

static	tmr_t	ata_timer_conf; /* single timeout list for all instances */
static	int	ata_watchdog_usec = 100000; /* check timeouts every 100 ms */

int	ata_hba_start_watchdog = 1000;
int	ata_process_intr_watchdog = 1000;
int	ata_reset_bus_watchdog = 1000;


/*
 * number of seconds to wait during various operations
 */
int	ata_flush_delay = 5 * 1000000;
uint_t	ata_set_feature_wait = 4 * 1000000;
uint_t	ata_flush_cache_wait = 60 * 1000000;	/* may take a long time */

/*
 * Change this for SFF-8070i support. Currently SFF-8070i is
 * using a field in the IDENTIFY PACKET DEVICE response which
 * already seems to be in use by some vendor's drives. I suspect
 * SFF will either move their laslun field or provide a reliable
 * way to validate it.
 */
int	ata_enable_atapi_luns = FALSE;

/*
 * set this to disable all DMA requests
 */
int	ata_dma_disabled = FALSE;

/*
 * set this to enable storing the IDENTIFY DEVICE result in the
 * "ata" or "atapi" property.
 */
int	ata_id_debug = FALSE;

/*
 * bus nexus operations
 */
static	struct bus_ops	 ata_bus_ops;
static	struct bus_ops	*scsa_bus_ops_p;

/* ARGSUSED */
static int
ata_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	if (ddi_get_soft_state(ata_state, getminor(*devp)) == NULL)
		return (ENXIO);

	return (0);
}



/*
 * The purpose of this function is to pass the ioaddress of the controller
 * to the caller, specifically used for upgrade from pre-pciide
 * to pciide nodes
 */
/* ARGSUSED */
static int
ata_read(dev_t dev, struct uio *uio_p, cred_t *cred_p)
{
	ata_ctl_t *ata_ctlp;
	char	buf[18];
	int len;

	ata_ctlp = ddi_get_soft_state(ata_state, getminor(dev));

	if (ata_ctlp == NULL)
		return (ENXIO);

	(void) sprintf(buf, "%p\n", (void *) ata_ctlp->ac_ioaddr1);

	len = strlen(buf) - uio_p->uio_offset;
	len = min(uio_p->uio_resid,  len);
	if (len <= 0)
		return (0);

	return (uiomove((caddr_t)(buf + uio_p->uio_offset), len,
	    UIO_READ, uio_p));
}

int
ata_devo_reset(
	dev_info_t *dip,
	ddi_reset_cmd_t cmd)
{
	ata_ctl_t *ata_ctlp;
	ata_drv_t *ata_drvp;
	int	   instance;
	int	   i;
	int	   rc;
	int	   flush_okay;

	if (cmd != DDI_RESET_FORCE)
		return (0);

	instance = ddi_get_instance(dip);
	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (!ata_ctlp)
		return (0);

	/*
	 * reset ATA drives and flush the write cache of any drives
	 */
	flush_okay = TRUE;
	for (i = 0; i < ATA_MAXTARG; i++) {
		if ((ata_drvp = CTL2DRV(ata_ctlp, i, 0)) == 0)
			continue;
		if (ata_drvp->ad_flags & AD_DISK) {
			/*
			 * Enable revert to defaults when reset
			 */
			(void) ata_set_feature(ata_ctlp, ata_drvp, 0xCC, 0);
		}

		/*
		 * Bug 4183194 - skip flush cache if device type is cdrom
		 *
		 * notes: the structure definitions for ata_drvp->ad_id are
		 * defined for the ATA IDENTIFY_DEVICE, but if AD_ATAPI is set
		 * the struct holds data for the ATAPI IDENTIFY_PACKET_DEVICE
		 */
		if (((ata_drvp->ad_flags & AD_ATAPI) == 0) ||
		    ((ata_drvp->ad_id.ai_config >> 8) & DTYPE_MASK) !=
		    DTYPE_RODIRECT) {

			/*
			 * Try the ATA/ATAPI flush write cache command
			 */
			rc = ata_flush_cache(ata_ctlp, ata_drvp);
			ADBG_WARN(("ata_flush_cache %s\n",
				rc ? "okay" : "failed"));

			if (!rc)
				flush_okay = FALSE;
		}


		/*
		 * do something else if flush cache not supported
		 */
	}

	/*
	 * just busy wait if any drive doesn't support FLUSH CACHE
	 */
	if (!flush_okay)
		drv_usecwait(ata_flush_delay);
	return (0);
}


static struct cb_ops ata_cb_ops = {
	ata_open,		/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	ata_read,		/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* chpoll */
	nodev,			/* prop_op */
	NULL,			/* stream info */
	D_MP,			/* driver compatibility flag */
	CB_REV,			/* cb_ops revsion */
	nodev,			/* aread */
	nodev			/* awrite */
};




static struct dev_ops	ata_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	ata_identify,		/* identify */
	ata_probe,		/* probe */
	ata_attach,		/* attach */
	ata_detach,		/* detach */
	ata_devo_reset,		/* reset */
	&ata_cb_ops,		/* driver operations */
	NULL			/* bus operations */
};

/* driver loadable module wrapper */
struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"ATA AT-bus attachment disk controller Driver",	/* module name */
	&ata_ops,					/* driver ops */
};

struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

#ifdef ATA_DEBUG
int	ata_debug_init = FALSE;
int	ata_debug_probe = FALSE;
int	ata_debug_attach = FALSE;

int	ata_debug = ADBG_FLAG_ERROR
		/* | ADBG_FLAG_ARQ */
		/* | ADBG_FLAG_INIT */
		/* | ADBG_FLAG_TRACE */
		/* | ADBG_FLAG_TRANSPORT */
		/* | ADBG_FLAG_WARN */
		;
#endif

int
_init(void)
{
	int err;

#ifdef ATA_DEBUG
	if (ata_debug_init)
		debug_enter("\nATA _INIT\n");
#endif

	if ((err = ddi_soft_state_init(&ata_state, sizeof (ata_ctl_t), 0)) != 0)
		return (err);

	if ((err = scsi_hba_init(&modlinkage)) != 0) {
		ddi_soft_state_fini(&ata_state);
		return (err);
	}

	/* save pointer to SCSA provided bus_ops struct */
	scsa_bus_ops_p = ata_ops.devo_bus_ops;

	/* make a copy of SCSA bus_ops */
	ata_bus_ops = *(ata_ops.devo_bus_ops);

	/* modify our bus_ops to call our bus_ctl routine */
	ata_bus_ops.bus_ctl = ata_bus_ctl;

	/* patch our bus_ops into the dev_ops struct */
	ata_ops.devo_bus_ops = &ata_bus_ops;

	if ((err = mod_install(&modlinkage)) != 0) {
		scsi_hba_fini(&modlinkage);
		ddi_soft_state_fini(&ata_state);
	}

	/*
	 * Initialize the per driver timer info.
	 */

	ghd_timer_init(&ata_timer_conf, drv_usectohz(ata_watchdog_usec));

	return (err);
}

int
_fini(void)
{
	int err;

	if ((err = mod_remove(&modlinkage)) == 0) {
		ghd_timer_fini(&ata_timer_conf);
		scsi_hba_fini(&modlinkage);
		ddi_soft_state_fini(&ata_state);
	}

	return (err);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/* driver identify entry point */

/* ARGSUSED */
static int
ata_identify(
	dev_info_t *dip)
{
	ADBG_TRACE(("ata_identify entered\n"));
	return (DDI_IDENTIFIED);
}

/* driver probe entry point */

static int
ata_probe(
	dev_info_t *dip)
{
	ddi_acc_handle_t io_hdl1 = NULL;
	ddi_acc_handle_t io_hdl2 = NULL;
	ddi_acc_handle_t bm_hdl = NULL;
	caddr_t		 ioaddr1;
	caddr_t		 ioaddr2;
	caddr_t		 bm_addr;
	int		 drive;
	struct ata_id	*ata_id_bufp;
	int		 rc = DDI_PROBE_FAILURE;

	ADBG_TRACE(("ata_probe entered\n"));
#ifdef ATA_DEBUG
	if (ata_debug_probe)
		debug_enter("\nATA_PROBE\n");
#endif

	if (!ata_setup_ioaddr(dip, &io_hdl1, &ioaddr1, &io_hdl2, &ioaddr2,
			&bm_hdl, &bm_addr))
		return (rc);

	ata_id_bufp = (struct ata_id *)kmem_zalloc(sizeof (*ata_id_bufp),
		KM_SLEEP);

	if (ata_id_bufp == NULL) {
		ADBG_WARN(("ata_probe: kmem_zalloc failed\n"));
		goto out1;
	}

	for (drive = 0; drive < ATA_MAXTARG; drive++) {
		uchar_t	drvhd;

		/* set up drv/hd and feature registers */

		drvhd = (drive == 0 ? ATDH_DRIVE0 : ATDH_DRIVE1);


		if (ata_drive_type(drvhd, io_hdl1, ioaddr1, io_hdl2, ioaddr2,
				ata_id_bufp) != ATA_DEV_NONE) {
			rc = (DDI_PROBE_SUCCESS);
			break;
		}
	}

	/* always leave the controller set to drive 0 */
	if (drive != 0) {
		ddi_putb(io_hdl1, (uchar_t *)ioaddr1 + AT_DRVHD, ATDH_DRIVE0);
		ATA_DELAY_400NSEC(io_hdl2, ioaddr2);
	}

out2:
	kmem_free(ata_id_bufp, sizeof (*ata_id_bufp));
out1:
	if (io_hdl1)
		ddi_regs_map_free(&io_hdl1);
	if (io_hdl2)
		ddi_regs_map_free(&io_hdl2);
	if (bm_hdl)
		ddi_regs_map_free(&bm_hdl);
	return (rc);
}

/*
 *
 * driver attach entry point
 *
 */

static int
ata_attach(
	dev_info_t *dip,
	ddi_attach_cmd_t cmd)
{
	ata_ctl_t	*ata_ctlp;
	ata_drv_t	*ata_drvp;
	ata_drv_t	*first_drvp = NULL;
	uchar_t		 targ;
	uchar_t		 lun;
	uchar_t		 lastlun;
	int		 atapi_count = 0;
	int		 disk_count = 0;

	ADBG_TRACE(("ata_attach entered\n"));
#ifdef ATA_DEBUG
	if (ata_debug_attach)
		debug_enter("\nATA_ATTACH\n\n");
#endif

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	/* initialize controller */

	ata_ctlp = ata_init_controller(dip);

	if (ata_ctlp == NULL)
		goto errout;

	mutex_enter(&ata_ctlp->ac_ccc.ccc_hba_mutex);

	/* initialize drives */

	for (targ = 0; targ < ATA_MAXTARG; targ++) {

		ata_drvp = ata_init_drive(ata_ctlp, targ, 0);

		if (ata_drvp == NULL)
			continue;

		if (first_drvp == NULL)
			first_drvp = ata_drvp;

		if (ATAPIDRV(ata_drvp)) {
			atapi_count++;
			lastlun = ata_drvp->ad_id.ai_lastlun;
		} else {
			disk_count++;
			lastlun = 0;
		}

		/*
		 * LUN support is currently disabled. Check with SFF-8070i
		 * before enabling.
		 */
		if (!ata_enable_atapi_luns)
			lastlun = 0;

		/* Initialize higher LUNs, if there are any */
		for (lun = 1; lun <= lastlun && lun < ATA_MAXLUN; lun++)
			(void) ata_init_drive(ata_ctlp, targ, lun);

	}

	if ((atapi_count == 0) && (disk_count == 0)) {
		ADBG_WARN(("ata_attach: no drives detected\n"));
		goto errout1;
	}

	/*
	 * Always make certain that a valid drive is selected so
	 * that routines which poll the status register don't get
	 * confused by non-existent drives.
	 */
	ddi_putb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_drvhd,
		first_drvp->ad_drive_bits);
	ATA_DELAY_400NSEC(ata_ctlp->ac_iohandle2, ata_ctlp->ac_ioaddr2);

	/*
	 * make certain the drive selected
	 */
	if (!ata_wait(ata_ctlp->ac_iohandle2, ata_ctlp->ac_ioaddr2,
			0, ATS_BSY, 5000000)) {
		ADBG_ERROR(("ata_attach: select failed\n"));
	}

	/*
	 * initialize atapi/ata_dsk modules if we have at least
	 * one drive of that type.
	 */

	if (atapi_count) {
		if (!atapi_attach(ata_ctlp))
			goto errout1;
		ata_ctlp->ac_flags |= AC_ATAPI_INIT;
	}

	if (disk_count) {
		if (!ata_disk_attach(ata_ctlp))
			goto errout;
		ata_ctlp->ac_flags |= AC_DISK_INIT;
	}

	/*
	 * make certain the interrupt and error latches are clear
	 */
	if (ata_ctlp->ac_pciide) {

		int instance = ddi_get_instance(dip);
		if (ddi_create_minor_node(dip, "control", S_IFCHR, instance,
		    DDI_PSEUDO, 0) != DDI_SUCCESS) {
			goto errout1;
		}

		(void) ata_pciide_status_clear(ata_ctlp);

	}

	/*
	 * enable the interrupt handler and drop the mutex
	 */
	ata_ctlp->ac_flags |= AC_ATTACHED;
	mutex_exit(&ata_ctlp->ac_ccc.ccc_hba_mutex);

	ddi_report_dev(dip);
	return (DDI_SUCCESS);

errout1:
	mutex_exit(&ata_ctlp->ac_ccc.ccc_hba_mutex);
errout:
	(void) ata_detach(dip, DDI_DETACH);
	return (DDI_FAILURE);
}

/* driver detach entry point */

static int
ata_detach(
	dev_info_t *dip,
	ddi_detach_cmd_t cmd)
{
	ata_ctl_t *ata_ctlp;
	ata_drv_t *ata_drvp;
	int	   instance;
	int	   i;
	int	   j;

	ADBG_TRACE(("ata_detach entered\n"));

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(dip);
	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (!ata_ctlp)
		return (DDI_SUCCESS);

	ata_ctlp->ac_flags &= ~AC_ATTACHED;

	/* destroy ata module */
	if (ata_ctlp->ac_flags & AC_DISK_INIT)
		ata_disk_detach(ata_ctlp);

	/* destroy atapi module */
	if (ata_ctlp->ac_flags & AC_ATAPI_INIT)
		atapi_detach(ata_ctlp);

	ddi_remove_minor_node(dip, NULL);

	/* destroy drives */
	for (i = 0; i < ATA_MAXTARG; i++) {
		for (j = 0; j < ATA_MAXLUN; j++) {
			ata_drvp = CTL2DRV(ata_ctlp, i, j);
			if (ata_drvp != NULL)
				ata_uninit_drive(ata_drvp);
		}
	}

	if (ata_ctlp->ac_iohandle1)
		ddi_regs_map_free(&ata_ctlp->ac_iohandle1);
	if (ata_ctlp->ac_iohandle2)
		ddi_regs_map_free(&ata_ctlp->ac_iohandle2);
	if (ata_ctlp->ac_bmhandle)
		ddi_regs_map_free(&ata_ctlp->ac_bmhandle);

	ddi_prop_remove_all(dip);

	/* destroy controller */
	ata_destroy_controller(dip);

	return (DDI_SUCCESS);
}

/* Nexus driver bus_ctl entry point */

/*ARGSUSED*/
static int
ata_bus_ctl(
	dev_info_t *d,
	dev_info_t *r,
	ddi_ctl_enum_t o,
	void *a,
	void *v)
{
	dev_info_t *tdip;
	int	target_type;
	int	rc;
	char	*bufp;

	ADBG_TRACE(("ata_bus_ctl entered\n"));

	switch (o) {

	case DDI_CTLOPS_IOMIN:

		/*
		 * Since we use PIO, we return a minimum I/O size of
		 * one byte.  This will need to be updated when we
		 * implement DMA support
		 */

		*((int *)v) = 1;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_DMAPMAPC:
	case DDI_CTLOPS_REPORTINT:
	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
	case DDI_CTLOPS_SIDDEV:
	case DDI_CTLOPS_SLAVEONLY:
	case DDI_CTLOPS_AFFINITY:
	case DDI_CTLOPS_POKE_INIT:
	case DDI_CTLOPS_POKE_FLUSH:
	case DDI_CTLOPS_POKE_FINI:
	case DDI_CTLOPS_INTR_HILEVEL:
	case DDI_CTLOPS_XLATE_INTRS:

		/* These ops shouldn't be called by a target driver */
		ADBG_ERROR(("ata_bus_ctl: %s%d: invalid op (%d) from %s%d\n",
			ddi_get_name(d), ddi_get_instance(d), o,
			ddi_get_name(r), ddi_get_instance(r)));

		return (DDI_FAILURE);

	case DDI_CTLOPS_REPORTDEV:
	case DDI_CTLOPS_INITCHILD:
	case DDI_CTLOPS_UNINITCHILD:

		/* these require special handling below */
		break;

	default:
		return (ddi_ctlops(d, r, o, a, v));
	}

	/* get targets dip */

	if (o == DDI_CTLOPS_INITCHILD)
		tdip = (dev_info_t *)a;
	else
		tdip = r;

	/*
	 * XXX - Get class of target
	 *   Before the "class" entry in a conf file becomes
	 *   a real property, we use an additional property
	 *   tentatively called "class_prop".  We will require that
	 *   new classes (ie. direct) export "class_prop".
	 *   SCSA target drivers will not have this property, so
	 *   no property implies SCSA.
	 */

	if ((ddi_prop_lookup_string(DDI_DEV_T_ANY, tdip, DDI_PROP_DONTPASS,
		"class", &bufp) == DDI_PROP_SUCCESS) ||
	    (ddi_prop_lookup_string(DDI_DEV_T_ANY, tdip, DDI_PROP_DONTPASS,
		"class_prop", &bufp) == DDI_PROP_SUCCESS)) {
		if (strcmp(bufp, "dada") == 0)
			target_type = ATA_DEV_DISK;
		else if (strcmp(bufp, "scsi") == 0)
			target_type = ATA_DEV_ATAPI;
		else {
			ADBG_WARN(("ata_bus_ctl: invalid target class %s\n",
				bufp));
			ddi_prop_free(bufp);
			return (DDI_FAILURE);
		}
		ddi_prop_free(bufp);
	} else {
		target_type = ATA_DEV_ATAPI; /* no class prop, assume SCSI */
	}

	if (o == DDI_CTLOPS_INITCHILD) {
		int	instance = ddi_get_instance(d);
		ata_ctl_t *ata_ctlp = ddi_get_soft_state(ata_state, instance);
		ata_drv_t *ata_drvp;
		int	targ;
		int	lun;
		int	drive_type;
		char	*disk_prop;
		char	*class_prop;

		if (ata_ctlp == NULL) {
			ADBG_WARN(("ata_bus_ctl: failed to find ctl struct\n"));
			return (DDI_FAILURE);
		}

		/* get (target,lun) of child device */

		targ = ddi_prop_get_int(DDI_DEV_T_ANY, tdip, DDI_PROP_DONTPASS,
					"target", -1);
		if (targ == -1) {
			ADBG_WARN(("ata_bus_ctl: failed to get targ num\n"));
			return (DDI_FAILURE);
		}

		lun = ddi_prop_get_int(DDI_DEV_T_ANY, tdip, DDI_PROP_DONTPASS,
			"lun", 0);

		if ((targ < 0) || (targ >= ATA_MAXTARG) ||
				(lun < 0) || (lun >= ATA_MAXLUN)) {
			return (DDI_FAILURE);
		}

		ata_drvp = CTL2DRV(ata_ctlp, targ, lun);

		if (ata_drvp == NULL)
			return (DDI_FAILURE);	/* no drive */

		/* get type of device */

		if (ATAPIDRV(ata_drvp))
			drive_type = ATA_DEV_ATAPI;
		else
			drive_type = ATA_DEV_DISK;

		/*
		 * Check for special handling when child driver is
		 * cmdk (which morphs to the correct interface)
		 * XXX - Remove when we replace cmdk
		 */
		if (strcmp(ddi_get_name(tdip), "cmdk") == 0) {

			if ((target_type == ATA_DEV_DISK) &&
				(target_type != drive_type))
				return (DDI_FAILURE);

			target_type = drive_type;

			if (drive_type == ATA_DEV_ATAPI) {
				class_prop = "scsi";
			} else {
				disk_prop = "dadk";
				class_prop = "dada";

				if (ddi_prop_create(DDI_DEV_T_NONE, tdip,
				    DDI_PROP_CANSLEEP, "disk",
				    (caddr_t)disk_prop,
				    (int)strlen(disk_prop)+1) !=
				    DDI_PROP_SUCCESS) {
					ADBG_WARN(("ata_bus_ctl: failed to "
						"create disk prop\n"));
					return (DDI_FAILURE);
				    }
			}

			if (ddi_prop_create(DDI_DEV_T_NONE, tdip,
				DDI_PROP_CANSLEEP, "class_prop",
				(caddr_t)class_prop,
				(int)strlen(class_prop)+1) !=
				    DDI_PROP_SUCCESS) {
					ADBG_WARN(("ata_bus_ctl: failed to "
						"create class prop\n"));
					return (DDI_FAILURE);
			}
		}

		/* Check that target class matches the device */

		if (target_type != drive_type)
			return (DDI_FAILURE);

		/* save pointer to drive struct for ata_disk_bus_ctl */
		ddi_set_driver_private(tdip, (caddr_t)ata_drvp);
	}

	if (target_type == ATA_DEV_ATAPI) {
		rc = scsa_bus_ops_p->bus_ctl(d, r, o, a, v);
	} else {
		rc = ata_disk_bus_ctl(d, r, o, a, v);
	}

	return (rc);
}


/*
 *
 * GHD ccc_hba_complete callback
 *
 */

/* ARGSUSED */
static void
ata_hba_complete(
	void *hba_handle,
	gcmd_t *gcmdp,
	int do_callback)
{
	ata_pkt_t *ata_pktp;

	ADBG_TRACE(("ata_hba_complete entered\n"));

	ata_pktp = GCMD2APKT(gcmdp);
	if (ata_pktp->ap_complete)
		(*ata_pktp->ap_complete)(ata_pktp, do_callback);
}

/* GHD ccc_timeout_func callback */

/* ARGSUSED */
static int
ata_timeout_func(
	void	*hba_handle,
	gcmd_t	*gcmdp,
	gtgt_t	*gtgtp,
	gact_t	 action,
	int	 calltype)
{
	ata_ctl_t *ata_ctlp;
	ata_pkt_t *ata_pktp;

	ADBG_TRACE(("ata_timeout_func entered\n"));

	ata_ctlp = (ata_ctl_t *)hba_handle;

	if (gcmdp != NULL)
		ata_pktp = GCMD2APKT(gcmdp);
	else
		ata_pktp = NULL;

	switch (action) {
	case GACTION_EARLY_ABORT:
		/* abort before request was started */
		if (ata_pktp != NULL) {
			ata_pktp->ap_flags |= AP_ABORT;
		}
		ghd_complete(&ata_ctlp->ac_ccc, gcmdp);
		return (TRUE);

	case GACTION_EARLY_TIMEOUT:
		/* timeout before request was started */
		if (ata_pktp != NULL) {
			ata_pktp->ap_flags |= AP_TIMEOUT;
		}
		ghd_complete(&ata_ctlp->ac_ccc, gcmdp);
		return (TRUE);

	case GACTION_RESET_TARGET:
		/*
		 * Reset a device is not supported. Resetting a specific
		 * device can't be done at all to an ATA device and if
		 * you send a RESET to an ATAPI device you have to
		 * reset the whole bus to make certain both devices
		 * on the bus stay in sync regarding which device is
		 * the currently selected one.
		 */
		return (FALSE);

	case GACTION_RESET_BUS:
		/*
		 * Issue bus reset and reinitialize both drives.
		 * But only if this is a timed-out request. Target
		 * driver reset requests are ignored because ATA
		 * and ATAPI devices shouldn't be gratuitously reset.
		 */
		if (gcmdp == NULL)
			break;
		return (ata_reset_bus(ata_ctlp));
	}

	return (FALSE);
}

/*
 *
 * Initialize controller's soft-state structure
 *
 */

static ata_ctl_t *
ata_init_controller(
	dev_info_t *dip)
{
	ata_ctl_t *ata_ctlp;
	int	   instance;
	caddr_t	   ioaddr1;
	caddr_t	   ioaddr2;

	ADBG_TRACE(("ata_init_controller entered\n"));

	instance = ddi_get_instance(dip);

	/* allocate controller structure */
	if (ddi_soft_state_zalloc(ata_state, instance) != DDI_SUCCESS) {
		ADBG_WARN(("ata_init_controller: soft_state_zalloc failed\n"));
		return (NULL);
	}

	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (ata_ctlp == NULL) {
		ADBG_WARN(("ata_init_controller: failed to find "
				"controller struct\n"));
		return (NULL);
	}

	/*
	 * initialize per-controller data
	 */
	ata_ctlp->ac_dip = dip;
	ata_ctlp->ac_arq_pktp = kmem_zalloc(sizeof (ata_pkt_t), KM_SLEEP);

	/*
	 * map the device registers
	 */
	if (!ata_setup_ioaddr(dip, &ata_ctlp->ac_iohandle1, &ioaddr1,
			&ata_ctlp->ac_iohandle2, &ioaddr2,
			&ata_ctlp->ac_bmhandle, &ata_ctlp->ac_bmaddr)) {
		(void) ata_detach(dip, DDI_DETACH);
		return (NULL);
	}

	ADBG_INIT(("ata_init_controller: ioaddr1 = 0x%p, ioaddr2 = 0x%p\n",
			ioaddr1, ioaddr2));

	/*
	 * Do ARQ setup
	 */
	atapi_init_arq(ata_ctlp);

	/*
	 * Do PCI-IDE setup
	 */
	ata_init_pciide(dip, ata_ctlp);

	/*
	 * port addresses associated with ioaddr1
	 */
	ata_ctlp->ac_ioaddr1	= ioaddr1;
	ata_ctlp->ac_data	= (ushort_t *)ioaddr1 + AT_DATA;
	ata_ctlp->ac_error	= (uchar_t *)ioaddr1 + AT_ERROR;
	ata_ctlp->ac_feature	= (uchar_t *)ioaddr1 + AT_FEATURE;
	ata_ctlp->ac_count	= (uchar_t *)ioaddr1 + AT_COUNT;
	ata_ctlp->ac_sect	= (uchar_t *)ioaddr1 + AT_SECT;
	ata_ctlp->ac_lcyl	= (uchar_t *)ioaddr1 + AT_LCYL;
	ata_ctlp->ac_hcyl	= (uchar_t *)ioaddr1 + AT_HCYL;
	ata_ctlp->ac_drvhd	= (uchar_t *)ioaddr1 + AT_DRVHD;
	ata_ctlp->ac_status	= (uchar_t *)ioaddr1 + AT_STATUS;
	ata_ctlp->ac_cmd	= (uchar_t *)ioaddr1 + AT_CMD;

	/*
	 * port addresses associated with ioaddr2
	 */
	ata_ctlp->ac_ioaddr2	= ioaddr2;
	ata_ctlp->ac_altstatus	= (uchar_t *)ioaddr2 + AT_ALTSTATUS;
	ata_ctlp->ac_devctl	= (uchar_t *)ioaddr2 + AT_DEVCTL;

	/*
	 * Bug 1256489:
	 *
	 * If AC_BSY_WAIT needs to be set  for laptops that do
	 * suspend/resume but do not correctly wait for the busy bit to
	 * drop after a resume.
	 */
	ata_ctlp->ac_timing_flags = ddi_prop_get_int(DDI_DEV_T_ANY,
			dip, DDI_PROP_DONTPASS, "timing_flags", 0);
	/*
	 * get max transfer size, default to 256 sectors
	 */
	ata_ctlp->ac_max_transfer = ddi_prop_get_int(DDI_DEV_T_ANY,
			dip, DDI_PROP_DONTPASS, "max_transfer", 0x100);
	if (ata_ctlp->ac_max_transfer < 1)
		ata_ctlp->ac_max_transfer = 1;
	if (ata_ctlp->ac_max_transfer > 0x100)
		ata_ctlp->ac_max_transfer = 0x100;

	/*
	 * Get the standby timer value
	 */
	ata_ctlp->ac_standby_time = ddi_prop_get_int(DDI_DEV_T_ANY,
			dip, DDI_PROP_DONTPASS, "standby", -1);

	/*
	 * If this is a /pci/pci-ide instance check to see if
	 * it's supposed to be attached as an /isa/ata
	 */
	if (ata_ctlp->ac_pciide) {
		static char prop_buf[] = "SUNW-ata-ffff-isa";

		(void) sprintf(prop_buf, "SUNW-ata-%p-isa",
			(void *) ata_ctlp->ac_ioaddr1);
		if (ddi_prop_exists(DDI_DEV_T_ANY, ddi_root_node(),
				    DDI_PROP_DONTPASS, prop_buf)) {
			(void) ata_detach(dip, DDI_DETACH);
			return (NULL);
		}
	}

	/*
	 * initialize GHD
	 */

	GHD_WAITQ_INIT(&ata_ctlp->ac_ccc.ccc_waitq, NULL, 1);

	if (!ghd_register("ata", &ata_ctlp->ac_ccc, dip, 0, ata_ctlp,
			atapi_ccballoc, atapi_ccbfree,
			ata_pciide_dma_sg_func, ata_hba_start,
			ata_hba_complete, ata_intr,
			ata_get_status, ata_process_intr, ata_timeout_func,
			&ata_timer_conf, NULL)) {
		(void) ata_detach(dip, DDI_DETACH);
		return (NULL);
	}

	ata_ctlp->ac_flags |= AC_GHD_INIT;
	return (ata_ctlp);
}

/* destroy a controller */

static void
ata_destroy_controller(
	dev_info_t *dip)
{
	ata_ctl_t *ata_ctlp;
	int	instance;

	ADBG_TRACE(("ata_destroy_controller entered\n"));

	instance = ddi_get_instance(dip);
	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (ata_ctlp == NULL)
		return;

	/* destroy ghd */
	if (ata_ctlp->ac_flags & AC_GHD_INIT)
		ghd_unregister(&ata_ctlp->ac_ccc);

	/* free the pciide buffer (if any) */
	ata_pciide_free(ata_ctlp);

	/* destroy controller struct */
	kmem_free(ata_ctlp->ac_arq_pktp, sizeof (ata_pkt_t));
	ddi_soft_state_free(ata_state, instance);

}


/*
 *
 * initialize a drive
 *
 */

static ata_drv_t *
ata_init_drive(
	ata_ctl_t	*ata_ctlp,
	uchar_t		targ,
	uchar_t		lun)
{
	static	char	 nec_260[]	= "NEC CD-ROM DRIVE";
	ata_drv_t *ata_drvp;
	struct ata_id	*aidp;
	char	buf[80];
	int	drive_type;
	int	i;

	ADBG_TRACE(("ata_init_drive entered, targ = %d, lun = %d\n",
		    targ, lun));

	/* check if device already exists */

	ata_drvp = CTL2DRV(ata_ctlp, targ, lun);

	if (ata_drvp != NULL)
		return (ata_drvp);

	/* allocate new device structure */

	ata_drvp = (ata_drv_t *)kmem_zalloc((unsigned)sizeof (ata_drv_t),
					    KM_NOSLEEP);
	if (!ata_drvp) {
		ADBG_ERROR(("ata_init_drive: kmem_zalloc failed\n"));
		return (NULL);
	}
	aidp = &ata_drvp->ad_id;

	/*
	 * set up drive struct
	 */
	ata_drvp->ad_ctlp = ata_ctlp;
	ata_drvp->ad_targ = targ;
	ata_drvp->ad_drive_bits =
		(ata_drvp->ad_targ == 0 ? ATDH_DRIVE0 : ATDH_DRIVE1);
	/*
	 * Add the LUN for SFF-8070i support
	 */
	ata_drvp->ad_lun = lun;
	ata_drvp->ad_drive_bits |= ata_drvp->ad_lun;

	/*
	 * get drive type, side effect is to collect
	 * IDENTIFY DRIVE data
	 */


	drive_type = ata_drive_type(ata_drvp->ad_drive_bits,
				    ata_ctlp->ac_iohandle1,
				    ata_ctlp->ac_ioaddr1,
				    ata_ctlp->ac_iohandle2,
				    ata_ctlp->ac_ioaddr2,
				    aidp);

	switch (drive_type) {
	case ATA_DEV_NONE:
		/* no drive found */
		goto errout;
	case ATA_DEV_ATAPI:
		ata_drvp->ad_flags |= AD_ATAPI;
		break;
	case ATA_DEV_DISK:
		ata_drvp->ad_flags |= AD_DISK;
		break;
	}

	/*
	 * swap bytes of all text fields
	 */
	if (!ata_strncmp(nec_260, aidp->ai_model, sizeof (aidp->ai_model))) {
		swab(aidp->ai_drvser, aidp->ai_drvser,
			sizeof (aidp->ai_drvser));
		swab(aidp->ai_fw, aidp->ai_fw,
			sizeof (aidp->ai_fw));
		swab(aidp->ai_model, aidp->ai_model,
			sizeof (aidp->ai_model));
	}

	/*
	 * Determine whether to enable DMA support for this drive.
	 */
	ata_drvp->ad_pciide_dma = ata_init_drive_pcidma(ata_ctlp, ata_drvp);

	/*
	 * Check if this drive has the Single Sector bug
	 */

	if (ata_check_drive_blacklist(&ata_drvp->ad_id, ATA_BL_1SECTOR))
		ata_drvp->ad_flags |= AD_1SECTOR;
	else
		ata_drvp->ad_flags &= ~AD_1SECTOR;

	/*
	 * dump the drive info
	 */
	(void) strncpy(buf, aidp->ai_model, sizeof (aidp->ai_model));
	buf[sizeof (aidp->ai_model)-1] = '\0';
	for (i = sizeof (aidp->ai_model) - 2; buf[i] == ' '; i--)
		buf[i] = '\0';


#define	ATAPRT(fmt)	ghd_err fmt

	ATAPRT(("?\t%s device at targ %d, lun %d lastlun 0x%x\n",
		(ATAPIDRV(ata_drvp) ? "ATAPI":"IDE"),
		ata_drvp->ad_targ, ata_drvp->ad_lun, aidp->ai_lastlun));

	ATAPRT(("?\tmodel %s, stat %x, err %x\n",
		buf,
		ddi_getb(ata_ctlp->ac_iohandle2, ata_ctlp->ac_altstatus),
		ddi_getb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_error)));
	ATAPRT(("?\t\tcfg 0x%x, cyl %d, hd %d, sec/trk %d\n",
		aidp->ai_config,
		aidp->ai_fixcyls,
		aidp->ai_heads,
		aidp->ai_sectors));
	ATAPRT(("?\t\tmult1 0x%x, mult2 0x%x, dwcap 0x%x, cap 0x%x\n",
		aidp->ai_mult1,
		aidp->ai_mult2,
		aidp->ai_dwcap,
		aidp->ai_cap));
	ATAPRT(("?\t\tpiomode 0x%x, dmamode 0x%x, advpiomode 0x%x\n",
		aidp->ai_piomode,
		aidp->ai_dmamode,
		aidp->ai_advpiomode));
	ATAPRT(("?\t\tminpio %d, minpioflow %d\n",
		aidp->ai_minpio,
		aidp->ai_minpioflow));
	ATAPRT(("?\t\tvalid 0x%x, dwdma 0x%x, majver 0x%x\n",
		aidp->ai_validinfo,
		aidp->ai_dworddma,
		aidp->ai_majorversion));

	if (ATAPIDRV(ata_drvp)) {
		if (!atapi_init_drive(ata_drvp))
			goto errout;
	} else {
		if (!ata_disk_init_drive(ata_drvp))
			goto errout;
	}

	/*
	 * store pointer in controller struct
	 */
	CTL2DRV(ata_ctlp, targ, lun) = ata_drvp;

	/*
	 * lock the drive's current settings in case I have to
	 * reset the drive due to some sort of error
	 */
	(void) ata_set_feature(ata_ctlp, ata_drvp, 0x66, 0);

	return (ata_drvp);

errout:
	ata_uninit_drive(ata_drvp);
	return (NULL);
}

/* destroy a drive */

static void
ata_uninit_drive(
	ata_drv_t *ata_drvp)
{
#if 0
	ata_ctl_t *ata_ctlp = ata_drvp->ad_ctlp;
#endif

	ADBG_TRACE(("ata_uninit_drive entered\n"));

#if 0
	/*
	 * DON'T DO THIS. disabling interrupts floats the IRQ line
	 * which generates spurious interrupts
	 */

	/*
	 * Select the correct drive
	 */
	ddi_putb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_drvhd,
		ata_drvp->ad_drive_bits);
	ATA_DELAY_400NSEC(ata_ctlp->ac_iohandle2, ata_ctlp->ac_ioaddr2);

	/*
	 * Disable interrupts from the drive
	 */
	ddi_putb(ata_ctlp->ac_iohandle2, ata_ctlp->ac_devctl,
		(ATDC_D3 | ATDC_NIEN));
#endif

	/* interface specific clean-ups */

	if (ata_drvp->ad_flags & AD_ATAPI)
		atapi_uninit_drive(ata_drvp);
	else if (ata_drvp->ad_flags & AD_DISK)
		ata_disk_uninit_drive(ata_drvp);

	/* free drive struct */

	kmem_free(ata_drvp, (unsigned)sizeof (ata_drv_t));
}


/*
 * ata_drive_type()
 *
 * The timeout values and exact sequence of checking is critical
 * especially for atapi device detection, and should not be changed lightly.
 *
 */
static int
ata_drive_type(
	uchar_t		 drvhd,
	ddi_acc_handle_t io_hdl1,
	caddr_t		 ioaddr1,
	ddi_acc_handle_t io_hdl2,
	caddr_t		 ioaddr2,
	struct ata_id	*ata_id_bufp)
{
	uchar_t	status;

	ADBG_TRACE(("ata_drive_type entered\n"));

	/*
	 * select the appropriate drive and LUN
	 */
	ddi_putb(io_hdl1, (uchar_t *)ioaddr1 + AT_DRVHD, drvhd);
	ATA_DELAY_400NSEC(io_hdl2, ioaddr2);

	/*
	 * make certain the drive is selected, and wait for not busy
	 */
	(void) ata_wait3(io_hdl2, ioaddr2, 0, ATS_BSY, 0x7f, 0, 0x7f, 0,
		5 * 1000000);

	status = ddi_getb(io_hdl2, (uchar_t *)ioaddr2 + AT_ALTSTATUS);

	if (status & ATS_BSY) {
		ADBG_TRACE(("ata_drive_type BUSY 0x%x 0x%x\n",
			    ioaddr1, status));
		return (ATA_DEV_NONE);
	}

	if (ata_disk_id(io_hdl1, ioaddr1, io_hdl2, ioaddr2, ata_id_bufp))
		return (ATA_DEV_DISK);

	/*
	 * No disk, check for atapi unit.
	 */
	if (!atapi_signature(io_hdl1, ioaddr1)) {
#ifndef ATA_DISABLE_ATAPI_1_7
		/*
		 * Check for old (but prevalent) atapi 1.7B
		 * spec device, the only known example is the
		 * NEC CDR-260 (not 260R which is (mostly) ATAPI 1.2
		 * compliant). This device has no signature
		 * and requires conversion from hex to BCD
		 * for some scsi audio commands.
		 */
		if (atapi_id(io_hdl1, ioaddr1, io_hdl2, ioaddr2, ata_id_bufp)) {
			return (ATA_DEV_ATAPI);
		}
#endif
		return (ATA_DEV_NONE);
	}

	if (atapi_id(io_hdl1, ioaddr1, io_hdl2, ioaddr2, ata_id_bufp)) {
		return (ATA_DEV_ATAPI);
	}

	return (ATA_DEV_NONE);

}

/*
 * Wait for a register of a controller to achieve a specific state.
 * To return normally, all the bits in the first sub-mask must be ON,
 * all the bits in the second sub-mask must be OFF.
 * If timeout_usec microseconds pass without the controller achieving
 * the desired bit configuration, we return TRUE, else FALSE.
 */

int ata_usec_delay = 10;

ata_wait(
	ddi_acc_handle_t io_hdl,
	caddr_t		ioaddr,
	uchar_t		onbits,
	uchar_t		offbits,
	uint_t		timeout_usec)
{
	ushort val;

	do  {
		val = ddi_getb(io_hdl, (uchar_t *)ioaddr + AT_ALTSTATUS);
		if ((val & onbits) == onbits && (val & offbits) == 0)
			return (TRUE);
		drv_usecwait(ata_usec_delay);
		timeout_usec -= ata_usec_delay;
	} while (timeout_usec > 0);

	return (FALSE);
}



/*
 *
 * This is a slightly more complicated version that checks
 * for error conditions and bails-out rather than looping
 * until the timeout expires
 */

ata_wait3(
	ddi_acc_handle_t io_hdl,
	caddr_t		ioaddr,
	uchar_t		onbits1,
	uchar_t		offbits1,
	uchar_t		failure_onbits2,
	uchar_t		failure_offbits2,
	uchar_t		failure_onbits3,
	uchar_t		failure_offbits3,
	uint_t		timeout_usec)
{
	ushort val;

	do  {
		val = ddi_getb(io_hdl, (uchar_t *)ioaddr + AT_ALTSTATUS);

		/*
		 * check for expected condition
		 */
		if ((val & onbits1) == onbits1 && (val & offbits1) == 0)
			return (TRUE);

		/*
		 * check for error conditions
		 */
		if ((val & failure_onbits2) == failure_onbits2 &&
				(val & failure_offbits2) == 0) {
			return (FALSE);
		}

		if ((val & failure_onbits3) == failure_onbits3 &&
				(val & failure_offbits3) == 0) {
			return (FALSE);
		}

		drv_usecwait(ata_usec_delay);
		timeout_usec -= ata_usec_delay;
	} while (timeout_usec > 0);

	return (FALSE);
}


/*
 *
 * low level routine for ata_disk_id() and atapi_id()
 *
 */

int
ata_id_common(
	uchar_t		 id_cmd,
	int		 expect_drdy,
	ddi_acc_handle_t io_hdl1,
	caddr_t		 ioaddr1,
	ddi_acc_handle_t io_hdl2,
	caddr_t		 ioaddr2,
	struct ata_id	*aidp)
{
	uchar_t	status;

	ADBG_TRACE(("ata_id_common entered\n"));

	bzero((caddr_t)aidp, sizeof (struct ata_id));

	/*
	 * clear the features register
	 */
	ddi_putb(io_hdl1, (uchar_t *)ioaddr1 + AT_FEATURE, 0);

	/*
	 * enable interrupts from the device
	 */
	ddi_putb(io_hdl2, (uchar_t *)ioaddr2 + AT_DEVCTL, ATDC_D3);

	/*
	 * issue IDENTIFY DEVICE or IDENTIFY PACKET DEVICE command
	 */
	ddi_putb(io_hdl1, (uchar_t *)ioaddr1 + AT_CMD, id_cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ioaddr2);

	/*
	 * According to the ATA specification, some drives may have
	 * to read the media to complete this command.  We need to
	 * make sure we give them enough time to respond.
	 */

	(void) ata_wait3(io_hdl2, ioaddr2, 0, ATS_BSY,
			ATS_ERR, ATS_BSY, 0x7f, 0, 5 * 1000000);

	/*
	 * read the status byte and clear the pending interrupt
	 */
	status = ddi_getb(io_hdl2, (uchar_t *)ioaddr1 + AT_STATUS);

	/*
	 * this happens if there's no drive present
	 */
	if (status == 0xff || status == 0x7f) {
		/* invalid status, can't be an ATA or ATAPI device */
		return (FALSE);
	}

	if (status & ATS_BSY) {
		ADBG_ERROR(("ata_id_common: BUSY status 0x%x error 0x%x\n",
			ddi_getb(io_hdl2, (uchar_t *)ioaddr2 +AT_ALTSTATUS),
			ddi_getb(io_hdl1, (uchar_t *)ioaddr1 + AT_ERROR)));
		return (FALSE);
	}

	if (!(status & ATS_DRQ)) {
		if (status & (ATS_ERR | ATS_DF)) {
			return (FALSE);
		}
		/*
		 * Give the drive another second to assert DRQ. Some older
		 * drives de-assert BSY before asserting DRQ.
		 */
		if (!ata_wait(io_hdl2, ioaddr2, ATS_DRQ, ATS_BSY, 1000000)) {
		ADBG_WARN(("ata_id_common: !DRQ status 0x%x error 0x%x\n",
			ddi_getb(io_hdl2, (uchar_t *)ioaddr2 +AT_ALTSTATUS),
			ddi_getb(io_hdl1, (uchar_t *)ioaddr1 + AT_ERROR)));
		return (FALSE);
		}
	}

	/*
	 * transfer the data
	 */
	ddi_rep_getw(io_hdl1, (ushort *)aidp, (ushort_t *)ioaddr1 + AT_DATA,
		NBPSCTR >> 1, DDI_DEV_NO_AUTOINCR);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ioaddr2);


	/*
	 * Wait for the drive to recognize I've read all the data.
	 * Some drives have been observed to take as much as 3msec to
	 * deassert DRQ after reading the data; allow 10 msec just in case.
	 *
	 * Note: some non-compliant ATAPI drives (e.g., NEC Multispin 6V,
	 * CDR-1350A) don't assert DRDY. If we've made it this far we can
	 * safely ignore the DRDY bit since the ATAPI Packet command
	 * actually doesn't require it to ever be asserted.
	 *
	 */
	if (!ata_wait(io_hdl2, ioaddr2, (unchar)(expect_drdy ? ATS_DRDY : 0),
					(ATS_BSY | ATS_DRQ), 1000000)) {
		ADBG_WARN(("ata_id_common: bad status 0x%x error 0x%x\n",
			ddi_getb(io_hdl2, (uchar_t *)ioaddr2 + AT_ALTSTATUS),
			ddi_getb(io_hdl1, (uchar_t *)ioaddr1 + AT_ERROR)));
		return (FALSE);
	}

	/*
	 * Check to see if the command aborted. This happens if
	 * an IDENTIFY DEVICE command is issued to an ATAPI PACKET device,
	 * or if an IDENTIFY PACKET DEVICE command is issued to an ATA
	 * (non-PACKET) device.
	 */
	if (status & (ATS_DF | ATS_ERR)) {
		ADBG_WARN(("ata_id_common: status 0x%x error 0x%x \n",
			ddi_getb(io_hdl2, (uchar_t *)ioaddr2 + AT_ALTSTATUS),
			ddi_getb(io_hdl1, (uchar_t *)ioaddr1 + AT_ERROR)));
		return (FALSE);
	}
	return (TRUE);
}


/*
 * Low level routine to issue a non-data command and busy wait for
 * the completion status.
 */

int
ata_command(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	int		 expect_drdy,
	int		 silent,
	uint_t		 busy_wait,
	uchar_t		 cmd,
	uchar_t		 feature,
	uchar_t		 count,
	uchar_t		 sector,
	uchar_t		 head,
	uchar_t		 cyl_low,
	uchar_t		 cyl_hi)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	uchar_t		 status;

	/* select the drive */
	ddi_putb(io_hdl1, ata_ctlp->ac_drvhd, ata_drvp->ad_drive_bits);
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/* make certain the drive selected */
	if (!ata_wait(io_hdl2, ata_ctlp->ac_ioaddr2,
			(unchar)(expect_drdy ? ATS_DRDY : 0),
			ATS_BSY, busy_wait)) {
		ADBG_ERROR(("ata_command: select failed "
			    "DRDY 0x%x CMD 0x%x F 0x%x N 0x%x  "
			    "S 0x%x H 0x%x CL 0x%x CH 0x%x\n",
			    expect_drdy, cmd, feature, count,
			    sector, head, cyl_low, cyl_hi));
		return (FALSE);
	}

	/*
	 * set all the regs
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_drvhd, (head | ata_drvp->ad_drive_bits));
	ddi_putb(io_hdl1, ata_ctlp->ac_sect, sector);
	ddi_putb(io_hdl1, ata_ctlp->ac_count, count);
	ddi_putb(io_hdl1, ata_ctlp->ac_lcyl, cyl_low);
	ddi_putb(io_hdl1, ata_ctlp->ac_hcyl, cyl_hi);
	ddi_putb(io_hdl1, ata_ctlp->ac_feature, feature);

	/* send the command */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/* wait for not busy */
	if (!ata_wait(io_hdl2, ata_ctlp->ac_ioaddr2, 0, ATS_BSY, busy_wait)) {
		ADBG_ERROR(("ata_command: BSY too long!"
			    "DRDY 0x%x CMD 0x%x F 0x%x N 0x%x  "
			    "S 0x%x H 0x%x CL 0x%x CH 0x%x\n",
			    expect_drdy, cmd, feature, count,
			    sector, head, cyl_low, cyl_hi));
		return (FALSE);
	}

	/*
	 * wait for DRDY before continuing
	 */
	(void) ata_wait3(io_hdl2, ata_ctlp->ac_ioaddr2,
			ATS_DRDY, ATS_BSY, /* okay */
			ATS_ERR, ATS_BSY, /* cmd failed */
			ATS_DF, ATS_BSY, /* drive failed */
			busy_wait);

	/* read status to clear IRQ, and check for error */
	status =  ddi_getb(io_hdl1, ata_ctlp->ac_status);

	if ((status & (ATS_BSY | ATS_DF | ATS_ERR)) == 0)
		return (TRUE);

	if (!silent) {
		ADBG_ERROR(("ata_command status 0x%x error 0x%x "
			    "DRDY 0x%x CMD 0x%x F 0x%x N 0x%x  "
			    "S 0x%x H 0x%x CL 0x%x CH 0x%x\n",
			    ddi_getb(io_hdl1, ata_ctlp->ac_status),
			    ddi_getb(io_hdl1, ata_ctlp->ac_error),
			    expect_drdy, cmd, feature, count,
			    sector, head, cyl_low, cyl_hi));
	}
	return (FALSE);
}



/*
 *
 * Issue a SET FEATURES command
 *
 */

int
ata_set_feature(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	uchar_t    feature,
	uchar_t    value)
{
	int		 rc;

	rc = ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, ata_set_feature_wait,
		ATC_SET_FEAT, feature, value, 0, 0, 0, 0);
		/* feature, count, sector, head, cyl_low, cyl_hi */

	if (rc) {
		return (TRUE);
	}

	ADBG_ERROR(("?ata_set_feature: (0x%x,0x%x) failed\n", feature, value));
	return (FALSE);
}



/*
 *
 * Issue a FLUSH CACHE command
 *
 */

static int
ata_flush_cache(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{
	/* this command is optional so fail silently */
	return (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE,
			    ata_flush_cache_wait,
			    ATC_FLUSH_CACHE, 0, 0, 0, 0, 0, 0));
}


#if defined(_eisa_bus_supported)
/* the following masks are all in little-endian byte order */
#define	DPT_MASK1		0xffff
#define	DPT_MASK2		0xffffff
#define	DPT_ID1			0x1412
#define	DPT_ID2			0x82a338

static int
ata_detect_dpt(
	dev_info_t *dip,
	int ioaddr)
{
	int	eisa_nvm(char *data, KEY_MASK key_mask, ...);
	KEY_MASK key_mask = {0};
	NVM_PORT *ports;
	struct {
		short slotnum;
		NVM_SLOTINFO slot;
		NVM_FUNCINFO func;
	} eisa_data;
	short	func;
	short	slot;
	uint	bytes;
	uint	boardid;
	char	*bus_type;
	int	rc;

	ADBG_TRACE(("ata_detect_dpt entered\n"));
	ASSERT(ioaddr != 0);

	/* get bus type and check if it is EISA */
	rc = ddi_prop_lookup_string(DDI_DEV_T_ANY, dip, 0, "device_type",
				    &bus_type);
	if (rc != DDI_PROP_SUCCESS)
			return (FALSE);
	if (strcmp(bus_type, DEVI_EISA_NEXNAME) != 0) {
		ddi_prop_free(bus_type);
		return (FALSE);
	}
	ddi_prop_free(bus_type);

	/*
	 * walk through all the eisa slots looking for DPT cards.  for
	 * each slot, get only one function, function 0
	 */
	key_mask.slot = TRUE;
	key_mask.function = TRUE;
	for (slot = 0; slot < 16; slot++) {
		bytes = eisa_nvm((char *)&eisa_data, key_mask, slot, 0);
		if (bytes == 0)
			continue;
		if (slot != eisa_data.slotnum)
			/* shouldn't happen */
			continue;

		/*
		 * check if found card is a DPT card.  if not, move
		 * on to next slot.  note that boardid will be in
		 * little-endian byte order
		 */
		boardid = eisa_data.slot.boardid[3] << 24;
		boardid |= eisa_data.slot.boardid[2] << 16;
		boardid |= eisa_data.slot.boardid[1] << 8;
		boardid |= eisa_data.slot.boardid[0];
		if ((boardid & DPT_MASK1) != DPT_ID1 &&
				(boardid & DPT_MASK2) != DPT_ID2)
			continue;

		/*
		 * check all the functions of the card in this slot, looking
		 * for port descriptions.  note that the info for function
		 * 0 is already in eisa_data.func (from above call to
		 * eisa_nvm)
		 */
		key_mask.board_id = FALSE;
		for (func = 1; ; func++) {
			if (eisa_data.func.fib.port != 0) {
				ports = eisa_data.func.un.r.port;
				if (ata_check_addr(ports, ioaddr)) {
					ADBG_INIT(("DPT card is using IO "
						"address in the range %x to "
						"%x\n", ioaddr, ioaddr + 7));
					return (TRUE);
				}
			}

			/* get info for next function of card in this slot */
			bytes = eisa_nvm((char *)&eisa_data, key_mask, slot,
				func);

			/*
			 * no more functions for card in this slot so go
			 * on to next slot
			 */
			if (bytes == 0)
				break;
			if (slot != eisa_data.slotnum)
				/* shouldn't happen */
				break;
		}
	}

	return (FALSE);
}

static int
ata_check_addr(
	NVM_PORT *ports,
	int ioaddr)
{
	int indx;

	ADBG_TRACE(("ata_check_addr entered\n"));

	/*
	 * do I/O port range check to see if IDE addresses are being
	 * used.  note that ports.count is really count - 1.
	 */
	for (indx = 0; indx < NVM_MAX_PORT; indx++) {
		if (ports[indx].address > ioaddr + 7 ||
			ports[indx].address + ports[indx].count < ioaddr) {
			if (ports[indx].more != 1)
				break;
		}
		else
			return (TRUE);
	}

	return (FALSE);
}
#endif



/*
 * ata_setup_ioaddr()
 *
 * Map the device registers and return the handles.
 *
 * If this is a ISA-ATA controller then only two handles are
 * initialized and returned.
 *
 * If this is a PCI-IDE controller than a third handle (for the
 * PCI-IDE Bus Mastering registers) is initialized and returned.
 *
 */

static int
ata_setup_ioaddr(
	dev_info_t	 *dip,
	ddi_acc_handle_t *handle1p,
	caddr_t		 *addr1p,
	ddi_acc_handle_t *handle2p,
	caddr_t		 *addr2p,
	ddi_acc_handle_t *bm_hdlp,
	caddr_t		 *bm_addrp)
{
	ddi_device_acc_attr_t dev_attr;
	char	*bufp;
	int	 rnumber;
	int	 rc;
	off_t	 regsize;

	/*
	 * Make certain the controller is enabled and its regs are map-able
	 *
	 */
	rc = ddi_dev_regsize(dip, 0, &regsize);
	if (rc != DDI_SUCCESS || regsize <= AT_CMD) {
		ADBG_INIT(("ata_setup_ioaddr(1): rc %d regsize %d\n",
			    rc, regsize));
		return (FALSE);
	}

	rc = ddi_dev_regsize(dip, 1, &regsize);
	if (rc != DDI_SUCCESS || regsize <= AT_ALTSTATUS) {
		ADBG_INIT(("ata_setup_ioaddr(2): rc %d regsize %d\n",
			    rc, regsize));
		return (FALSE);
	}

	/*
	 * setup the device attribute structure for little-endian,
	 * strict ordering access.
	 */
	dev_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	dev_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	dev_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	*handle1p = NULL;
	*handle2p = NULL;
	*bm_hdlp = NULL;

	/*
	 * Determine whether this is a ISA, PNP-ISA, or PCI-IDE device
	 */
	if (ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "pnp-csn")) {
		/* it's PNP-ISA, skip over the extra reg tuple */
		rnumber = 1;
		goto not_pciide;
	}

	/* else, it's ISA or PCI-IDE, check further */
	rnumber = 0;

	rc = ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_get_parent(dip),
				    DDI_PROP_DONTPASS, "device_type", &bufp);
	if (rc != DDI_PROP_SUCCESS) {
		ADBG_ERROR(("ata_setup_ioaddr !device_type\n"));
		goto not_pciide;
	}

	if (strcmp(bufp, "pci-ide") != 0) {
		/*
		 * If it's not a PCI-IDE, there are only two reg tuples
		 * and the first one contains the I/O base (170 or 1f0)
		 * rather than the controller instance number.
		 */
		ADBG_TRACE(("ata_setup_ioaddr !pci-ide\n"));
		ddi_prop_free(bufp);
		goto not_pciide;
	}
	ddi_prop_free(bufp);


	/*
	 * Map the correct half of the PCI-IDE Bus Master registers.
	 * There's a single BAR that maps these registers for both
	 * controller's in a dual-controller chip and it's upto my
	 * parent nexus, pciide, to adjust which (based on my instance
	 * number) half this call maps.
	 */
	rc = ddi_dev_regsize(dip, 2, &regsize);
	if (rc != DDI_SUCCESS || regsize < 8) {
		ADBG_INIT(("ata_setup_ioaddr(3): rc %d regsize %d\n",
			    rc, regsize));
		goto not_pciide;
	}

	rc = ddi_regs_map_setup(dip, 2, bm_addrp, 0, 0, &dev_attr, bm_hdlp);

	if (rc != DDI_SUCCESS) {
		/* map failed, try to use in non-pci-ide mode */
		ADBG_WARN(("ata_setup_ioaddr bus master map failed, rc=0x%x\n",
			rc));
		*bm_hdlp = NULL;
	}

not_pciide:
	/*
	 * map the lower command block registers
	 */

	rc = ddi_regs_map_setup(dip, rnumber, addr1p, 0, 0, &dev_attr,
				handle1p);

	if (rc != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ata: reg tuple 0 map failed, rc=0x%x\n", rc);
		goto out1;
	}

	/*
	 * If the controller is being used in compatibility mode
	 * via /devices/isa/ata@1,{1f0,1f0}/..., the reg property
	 * will specify zeros for the I/O ports for the PCI
	 * instance.
	 */
	if (*addr1p == 0) {
		ADBG_TRACE(("ata_setup_ioaddr ioaddr1 0\n"));
		goto out2;
	}


#if defined(_eisa_bus_supported)
	/*
	 * check to see if this is really a DPT EATA controller
	 */
	if (ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			    "ignore-hardware-nodes")) {
		if (ata_detect_dpt(dip, (int)*addr1p)) {
			cmn_err(CE_CONT, "?ata_probe(0x%p): I/O port conflict "
				"with EISA DPT HBA\n", (void *) *addr1p);
			goto out2;
		}
	}
#endif

	/*
	 * map the upper control block registers
	 */
	rc = ddi_regs_map_setup(dip, rnumber + 1, addr2p, 0, 0, &dev_attr,
				handle2p);
	if (rc == DDI_SUCCESS)
		return (TRUE);

	cmn_err(CE_WARN, "ata: reg tuple 1 map failed, rc=0x%x\n", rc);

out2:
	if (*handle1p != NULL) {
		ddi_regs_map_free(handle1p);
		*handle1p = NULL;
	}

out1:
	if (*bm_hdlp != NULL) {
		ddi_regs_map_free(bm_hdlp);
		*bm_hdlp = NULL;
	}
	return (FALSE);

}

/*
 *
 * Currently, the only supported controllers are ones which
 * support the SFF-8038 Bus Mastering spec.
 *
 * Check the parent node's IEEE 1275 class-code property to
 * determine if it's an PCI-IDE instance which supports SFF-8038
 * Bus Mastering. It's perfectly valid to have a PCI-IDE controller
 * that doesn't do Bus Mastering. In that case, my interrupt handler
 * only uses the interrupt latch bit in PCI-IDE status register.
 *
 * Whether the drive support supports the DMA option still needs
 * to be checked later. Each individual request also has to be
 * checked for alignment and size to decide whether to use the
 * DMA transfer mode.
 *
 */

static void
ata_init_pciide(
	dev_info_t	 *dip,
	ata_ctl_t *ata_ctlp)
{
	uint_t	 class_code;
	uchar_t	 status;

	if (ata_ctlp->ac_bmhandle == NULL) {
		ata_ctlp->ac_pciide = FALSE;
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}

	/*
	 * check if it's a known bogus PCI-IDE chip
	 */
	if (ata_check_pciide_blacklist(dip, ATA_BL_BOGUS)) {
		ADBG_WARN(("ata_setup_ioaddr pci-ide blacklist\n"));
		ata_ctlp->ac_pciide = FALSE;
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}
	ata_ctlp->ac_pciide = TRUE;

	/*
	 * check if a PCI-IDE chip is not compliant with spec with
	 * respect of use of interrupt bit for bus master register status
	 * when pio mode is used
	 */

	if (ata_check_pciide_blacklist(dip, ATA_BL_BMSTATREG_PIO_BROKEN)) {
		ata_ctlp->ac_flags |= AC_BMSTATREG_PIO_BROKEN;
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}



	/*
	 * check for a PCI-IDE chip with a broken DMA engine
	 */
	if (ata_check_pciide_blacklist(dip, ATA_BL_NODMA)) {
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}

	/*
	 * Check the Programming Interface register to determine
	 * if this device supports PCI-IDE Bus Mastering. Some PCI-IDE
	 * devices don't support Bus Mastering or DMA. This actually
	 * checks all three "class" bytes rather than just the low-byte.
	 */

	class_code = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(dip),
		DDI_PROP_DONTPASS, "class-code", 0);
	if ((class_code & PCIIDE_BM_CLASS_MASK) != PCIIDE_BM_CLASS) {
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}

	/*
	 * Avoid doing DMA on "simplex" chips which share hardware
	 * between channels
	 */
	status = ddi_getb(ata_ctlp->ac_bmhandle,
			(uchar_t *)ata_ctlp->ac_bmaddr + PCIIDE_BMISX_REG);
	if (status & PCIIDE_BMISX_SIMPLEX) {
		ata_ctlp->ac_pciide_bm = FALSE;
		return;
	}

	/*
	 * It's a compatible PCI-IDE Bus Mastering controller,
	 * allocate and map the DMA Scatter/Gather list (PRDE table).
	 */
	if (ata_pciide_alloc(dip, ata_ctlp))
		ata_ctlp->ac_pciide_bm = TRUE;
	else
		ata_ctlp->ac_pciide_bm = FALSE;
}


/*
 *
 * Determine whether to enable DMA support for this drive.
 * The controller and the drive both have to support DMA.
 * The controller's capabilities were already checked in
 * ata_init_pciide(), now just check the drive's capabilities.
 *
 */

static int
ata_init_drive_pcidma(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{
	char *bufp;

	int ata_options = 0;

	if (ata_ctlp->ac_pciide_bm != TRUE) {
		/* contoller isn't Bus Master capable */
		return (FALSE);
	}

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ata_ctlp->ac_dip,
		0, "ata-dma-enabled", &bufp) == DDI_PROP_SUCCESS) {

		if (strcmp(bufp, "0") == 0) {

			cmn_err(CE_CONT, "?ata_init_drive_pciide: "
					"dma disabled");

			ddi_prop_free(bufp);
			return (FALSE);
		}
		ddi_prop_free(bufp);
	}



	ata_options = ddi_prop_get_int(DDI_DEV_T_ANY, ata_ctlp->ac_dip,
			0, "ata-options", 0);

	if (!(ata_options & ATA_OPTIONS_DMA)) {
		/*
		 * Either the ata-options property was not found or
		 * DMA is not enabled by this property
		 */
		return (FALSE);
	}



	if (ata_check_drive_blacklist(&ata_drvp->ad_id, ATA_BL_NODMA))
		return (FALSE);

	/*
	 * DMA mode is mandatory on ATA-3 (or newer) drives but is
	 * optional on ATA-2 (or older) drives.
	 */

	if ((ata_drvp->ad_id.ai_majorversion & 0x8000) == 0 &&
			ata_drvp->ad_id.ai_majorversion >= (1 << 2)) {
		return (TRUE);
	}

	/*
	 * On ATA-2 drives the ai_majorversion word will probably
	 * be 0xffff or 0x0000, check the (now obsolete) DMA bit in
	 * the capabiliites word instead. The order of these tests
	 * is important since an ATA-3 drive doesn't have to set
	 * the DMA bit in the capabilities word.
	 */
	if (ata_drvp->ad_id.ai_cap & ATAC_DMA_SUPPORT) {
		return (TRUE);
	}

	return (FALSE);
}



/*
 * this compare routine squeezes out extra blanks and
 * returns TRUE if p1 matches the leftmost substring of p2
 */

static int
ata_strncmp(
	char *p1,
	char *p2,
	int cnt)
{

	for (;;) {
		/*
		 * skip over any extra blanks in both strings
		 */
		while (*p1 != '\0' && *p1 == ' ')
			p1++;

		while (cnt != 0 && *p2 == ' ') {
			p2++;
			cnt--;
		}

		/*
		 * compare the two strings
		 */

		if (cnt == 0 || *p1 != *p2)
			break;

		while (cnt > 0 && *p1 == *p2) {
			p1++;
			p2++;
			cnt--;
		}

	}

	/* return TRUE if both strings ended at same point */
	return ((*p1 == '\0') ? TRUE : FALSE);
}

/*
 * Per PSARC/1997/281 create variant="atapi" property (if necessary)
 * on the target's dev_info node. Currently, the sd target driver
 * is the only driver which refers to this property.
 *
 * If the flag ata_id_debug is set also create the
 * the "ata" or "atapi" property on the target's dev_info node
 *
 */

int
ata_prop_create(
	dev_info_t *tgt_dip,
	ata_drv_t  *ata_drvp,
	char	   *name)
{
	int	rc;

	ADBG_TRACE(("ata_prop_create 0x%x 0x%x %s\n", tgt_dip, ata_drvp, name));

	if (strcmp("atapi", name) == 0) {
		rc =  ddi_prop_update_string(DDI_DEV_T_NONE, tgt_dip,
			"variant", name);
		if (rc != DDI_PROP_SUCCESS)
			return (FALSE);
	}

	if (!ata_id_debug)
		return (TRUE);

	rc =  ddi_prop_update_byte_array(DDI_DEV_T_NONE, tgt_dip, name,
		(u_char *)&ata_drvp->ad_id, sizeof (ata_drvp->ad_id));
	if (rc != DDI_PROP_SUCCESS) {
		ADBG_ERROR(("ata_prop_create failed, rc=%d\n", rc));
	}
	return (TRUE);
}

/* *********************************************************************** */
/* *********************************************************************** */
/* *********************************************************************** */

/*
 * This state machine doesn't implement the ATAPI Optional Overlap
 * feature. You need that feature to efficiently support ATAPI
 * tape drives. See the 1394-ATA Tailgate spec (D97107), Figure 24,
 * for an example of how to add the necessary additional NextActions
 * and NextStates to this FSM and the atapi_fsm, in order to support
 * the Overlap Feature.
 */


uchar_t ata_ctlr_fsm_NextAction[ATA_CTLR_NSTATES][ATA_CTLR_NFUNCS] = {
/* --------------------- next action --------------------- | - current - */
/* start0 --- start1 ---- intr ------ fini --- reset --- */
{ AC_START,   AC_START,	  AC_NADA,    AC_NADA, AC_RESET_I }, /* idle	 */
{ AC_BUSY,    AC_BUSY,	  AC_INTR,    AC_FINI, AC_RESET_A }, /* active0  */
{ AC_BUSY,    AC_BUSY,	  AC_INTR,    AC_FINI, AC_RESET_A }, /* active1  */
};

uchar_t ata_ctlr_fsm_NextState[ATA_CTLR_NSTATES][ATA_CTLR_NFUNCS] = {

/* --------------------- next state --------------------- | - current - */
/* start0 --- start1 ---- intr ------ fini --- reset --- */
{ AS_ACTIVE0, AS_ACTIVE1, AS_IDLE,    AS_IDLE, AS_IDLE	  }, /* idle    */
{ AS_ACTIVE0, AS_ACTIVE0, AS_ACTIVE0, AS_IDLE, AS_ACTIVE0 }, /* active0 */
{ AS_ACTIVE1, AS_ACTIVE1, AS_ACTIVE1, AS_IDLE, AS_ACTIVE1 }, /* active1 */
};


static int
ata_ctlr_fsm(
	uchar_t		 fsm_func,
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp,
	int		*DoneFlgp)
{
	uchar_t	   action;
	uchar_t	   current_state;
	uchar_t	   next_state;
	int	   rc;

	current_state = ata_ctlp->ac_state;
	action = ata_ctlr_fsm_NextAction[current_state][fsm_func];
	next_state = ata_ctlr_fsm_NextState[current_state][fsm_func];

	/*
	 * Set the controller's new state
	 */
	ata_ctlp->ac_state = next_state;
	switch (action) {

	case AC_BUSY:
		return (ATA_FSM_RC_BUSY);

	case AC_NADA:
		return (ATA_FSM_RC_OKAY);

	case AC_START:
		ASSERT(ata_ctlp->ac_active_pktp == NULL);
		ASSERT(ata_ctlp->ac_active_drvp == NULL);

		ata_ctlp->ac_active_pktp = ata_pktp;
		ata_ctlp->ac_active_drvp = ata_drvp;

		rc = (*ata_pktp->ap_start)(ata_ctlp, ata_drvp, ata_pktp);

		if (rc == ATA_FSM_RC_BUSY) {
			/* the request didn't start, GHD will requeue it */
			ata_ctlp->ac_state = AS_IDLE;
			ata_ctlp->ac_active_pktp = NULL;
			ata_ctlp->ac_active_drvp = NULL;
		}
		return (rc);

	case AC_INTR:
		ASSERT(ata_ctlp->ac_active_pktp != NULL);
		ASSERT(ata_ctlp->ac_active_drvp != NULL);

		ata_drvp = ata_ctlp->ac_active_drvp;
		ata_pktp = ata_ctlp->ac_active_pktp;
		return ((*ata_pktp->ap_intr)(ata_ctlp, ata_drvp, ata_pktp));

	case AC_RESET_A: /* Reset, controller active */
		ASSERT(ata_ctlp->ac_active_pktp != NULL);
		ASSERT(ata_ctlp->ac_active_drvp != NULL);

		/* clean up the active request */
		ata_pktp = ata_ctlp->ac_active_pktp;
		ata_pktp->ap_flags |= AP_DEV_RESET | AP_BUS_RESET;

		/* halt the DMA engine */
		if (ata_pktp->ap_pciide_dma) {
			ata_pciide_dma_stop(ata_ctlp);
			(void) ata_pciide_status_clear(ata_ctlp);
		}

		/* Do a Software Reset to unwedge the bus */
		if (!ata_software_reset(ata_ctlp)) {
			return (ATA_FSM_RC_BUSY);
		}

		/* Then send a DEVICE RESET cmd to each ATAPI device */
		atapi_fsm_reset(ata_ctlp);
		return (ATA_FSM_RC_FINI);

	case AC_RESET_I: /* Reset, controller idle */
		/* Do a Software Reset to unwedge the bus */
		if (!ata_software_reset(ata_ctlp)) {
			return (ATA_FSM_RC_BUSY);
		}

		/* Then send a DEVICE RESET cmd to each ATAPI device */
		atapi_fsm_reset(ata_ctlp);
		return (ATA_FSM_RC_OKAY);

	case AC_FINI:
		break;
	}

	/*
	 * AC_FINI, check ARQ needs to be started or finished
	 */

	ASSERT(action == AC_FINI);
	ASSERT(ata_ctlp->ac_active_pktp != NULL);
	ASSERT(ata_ctlp->ac_active_drvp != NULL);

	/*
	 * The active request is done now.
	 * Disconnect the request from the controller and
	 * add it to the done queue.
	 */
	ata_drvp = ata_ctlp->ac_active_drvp;
	ata_pktp = ata_ctlp->ac_active_pktp;

	/*
	 * If ARQ pkt is done, get ptr to original pkt and wrap it up.
	 */
	if (ata_pktp == ata_ctlp->ac_arq_pktp) {
		ata_pkt_t *arq_pktp;

		ADBG_ARQ(("ata_ctlr_fsm 0x%x ARQ done\n", ata_ctlp));

		arq_pktp = ata_pktp;
		ata_pktp = ata_ctlp->ac_fault_pktp;
		ata_ctlp->ac_fault_pktp = NULL;
		if (arq_pktp->ap_flags & (AP_ERROR | AP_BUS_RESET))
			ata_pktp->ap_flags |= AP_ARQ_ERROR;
		else
			ata_pktp->ap_flags |= AP_ARQ_OKAY;
		goto all_done;
	}


#define	AP_ARQ_NEEDED	(AP_ARQ_ON_ERROR | AP_GOT_STATUS | AP_ERROR)

	/*
	 * Start ARQ pkt if necessary
	 */
	if ((ata_pktp->ap_flags & AP_ARQ_NEEDED) == AP_ARQ_NEEDED &&
			(ata_pktp->ap_status & ATS_ERR)) {

		/* set controller state back to active */
		ata_ctlp->ac_state = current_state;

		/* try to start the ARQ pkt */
		rc = ata_start_arq(ata_ctlp, ata_drvp, ata_pktp);

		if (rc == ATA_FSM_RC_BUSY) {
			ADBG_ARQ(("ata_ctlr_fsm 0x%x ARQ BUSY\n", ata_ctlp));
			/* let the target driver handle the problem */
			ata_ctlp->ac_state = AS_IDLE;
			ata_ctlp->ac_active_pktp = NULL;
			ata_ctlp->ac_active_drvp = NULL;
			ata_ctlp->ac_fault_pktp = NULL;
			goto all_done;
		}

		ADBG_ARQ(("ata_ctlr_fsm 0x%x ARQ started\n", ata_ctlp));
		return (rc);
	}

	/*
	 * Normal completion, no error status, and not an ARQ pkt,
	 * just fall through.
	 */

all_done:

	/*
	 * wrap everything up and tie a ribbon around it
	 */
	ata_ctlp->ac_active_pktp = NULL;
	ata_ctlp->ac_active_drvp = NULL;
	if (APKT2GCMD(ata_pktp) != (gcmd_t *)0) {
		ghd_complete(&ata_ctlp->ac_ccc, APKT2GCMD(ata_pktp));
		if (DoneFlgp)
			*DoneFlgp = TRUE;
	}

	return (ATA_FSM_RC_OKAY);
}


static int
ata_start_arq(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ata_pkt_t		*arq_pktp;
	int			 bytes;
	uchar_t			 senselen;

	ADBG_ARQ(("ata_start_arq 0x%x ARQ needed\n", ata_ctlp));

	/*
	 * Determine just the size of the Request Sense Data buffer within
	 * the scsi_arq_status structure.
	 */
#define	SIZEOF_ARQ_HEADER	(sizeof (struct scsi_arq_status)	\
				- sizeof (struct scsi_extended_sense))
	senselen = ata_pktp->ap_statuslen - SIZEOF_ARQ_HEADER;
	ASSERT(senselen > 0);


	/* save ptr to original pkt */
	ata_ctlp->ac_fault_pktp = ata_pktp;

	/* switch the controller's active pkt to the ARQ pkt */
	arq_pktp = ata_ctlp->ac_arq_pktp;
	ata_ctlp->ac_active_pktp = arq_pktp;

	/* finish initializing the ARQ CDB */
	ata_ctlp->ac_arq_cdb[1] = ata_drvp->ad_lun << 4;
	ata_ctlp->ac_arq_cdb[4] = senselen;

	/* finish initializing the ARQ pkt */
	arq_pktp->ap_v_addr = (caddr_t)&ata_pktp->ap_scbp->sts_sensedata;

	arq_pktp->ap_resid = senselen;
	arq_pktp->ap_flags = AP_ATAPI | AP_READ;
	arq_pktp->ap_cdb_pad =
	((unsigned)(ata_drvp->ad_cdb_len - arq_pktp->ap_cdb_len)) >> 1;

	bytes = min(senselen, ATAPI_MAX_BYTES_PER_DRQ);
	arq_pktp->ap_hicyl = (uchar_t)(bytes >> 8);
	arq_pktp->ap_lwcyl = (uchar_t)bytes;

	/*
	 * This packet is shared by all drives on this controller
	 * therefore we need to init the drive number on every ARQ.
	 */
	arq_pktp->ap_hd = ata_drvp->ad_drive_bits;

	/* start it up */
	return ((*arq_pktp->ap_start)(ata_ctlp, ata_drvp, arq_pktp));
}

/*
 *
 * reset the bus
 *
 */

static int
ata_reset_bus(
	ata_ctl_t *ata_ctlp)
{
	int	watchdog;
	uchar_t	drive;
	int	rc = FALSE;
	uchar_t	fsm_func;
	int	DoneFlg = FALSE;

	/*
	 * Do a Software Reset to unwedge the bus, and send
	 * ATAPI DEVICE RESET to each ATAPI drive.
	 */
	fsm_func = ATA_FSM_RESET;
	for (watchdog = ata_reset_bus_watchdog; watchdog > 0; watchdog--) {
		switch (ata_ctlr_fsm(fsm_func, ata_ctlp, NULL, NULL,
						&DoneFlg)) {
		case ATA_FSM_RC_OKAY:
			rc = TRUE;
			goto fsm_done;

		case ATA_FSM_RC_BUSY:
			return (FALSE);

		case ATA_FSM_RC_INTR:
			fsm_func = ATA_FSM_INTR;
			rc = TRUE;
			continue;

		case ATA_FSM_RC_FINI:
			fsm_func = ATA_FSM_FINI;
			rc = TRUE;
			continue;
		}
	}
	ADBG_WARN(("ata_reset_bus: watchdog\n"));

fsm_done:

	/*
	 * Reinitialize the ATA drives
	 */
	for (drive = 0; drive < ATA_MAXTARG; drive++) {
		ata_drv_t *ata_drvp;

		if ((ata_drvp = CTL2DRV(ata_ctlp, drive, 0)) == NULL)
			continue;

		if (ATAPIDRV(ata_drvp))
			continue;

		/*
		 * Reprogram the Read/Write Multiple block factor
		 * and current geometry into the drive.
		 */
		if (!ata_disk_setup_parms(ata_ctlp, ata_drvp))
			rc = FALSE;
	}

	/* If DoneFlg is TRUE, it means that ghd_complete() function */
	/* has been already called. In this case ignore any errors and */
	/* return TRUE to the caller, otherwise return the value of rc */
	/* to the caller (See Bug# 4231008). */
	if (DoneFlg)
		return (TRUE);
	else
		return (rc);
}


/*
 *
 * Low level routine to toggle the Software Reset bit
 *
 */

static int
ata_software_reset(
	ata_ctl_t *ata_ctlp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 time_left;

	ADBG_TRACE(("ata_reset_bus entered\n"));

	/* disable interrupts and turn the software reset bit on */
	ddi_putb(io_hdl2, ata_ctlp->ac_devctl, (ATDC_D3 | ATDC_SRST));

	/* why 30 milliseconds, the ATA/ATAPI-4 spec says 5 usec. */
	drv_usecwait(30000);

	/* turn the software reset bit back off */
	ddi_putb(io_hdl2, ata_ctlp->ac_devctl, ATDC_D3);

	/*
	 * Wait for the controller to assert BUSY status.
	 * I don't think 300 msecs is correct. The ATA/ATAPI-4
	 * spec says 400 nsecs, (and 2 msecs if device
	 * was in sleep mode; but we don't put drives to sleep
	 * so it probably doesn't matter).
	 */
	drv_usecwait(300000);

	/*
	 * If drive 0 exists the test for completion is simple
	 */
	time_left = 31 * 1000000;
	if (CTL2DRV(ata_ctlp, 0, 0)) {
		goto wait_for_not_busy;
	}

	ASSERT(CTL2DRV(ata_ctlp, 1, 0) != NULL);

	/*
	 * This must be a single device configuration, with drive 1
	 * only. This complicates the test for completion because
	 * issuing the software reset just caused drive 1 to
	 * deselect. With drive 1 deselected, if I just read the
	 * status register to test the BSY bit I get garbage, but
	 * I can't re-select drive 1 until I'm certain the BSY bit
	 * is de-asserted. Catch-22.
	 *
	 * In ATA/ATAPI-4, rev 15, section 9.16.2, it says to handle
	 * this situation like this:
	 */

	/* give up if the drive doesn't settle within 31 seconds */
	while (time_left > 0) {
		/*
		 * delay 10msec each time around the loop
		 */
		drv_usecwait(10000);
		time_left -= 10000;

		/*
		 * try to select drive 1
		 */
		ddi_putb(io_hdl1, ata_ctlp->ac_drvhd, ATDH_DRIVE1);

		ddi_putb(io_hdl1, ata_ctlp->ac_sect, 0x55);
		ddi_putb(io_hdl1, ata_ctlp->ac_sect, 0xaa);
		if (ddi_getb(io_hdl1, ata_ctlp->ac_sect) != 0xaa)
			continue;

		ddi_putb(io_hdl1, ata_ctlp->ac_count, 0x55);
		ddi_putb(io_hdl1, ata_ctlp->ac_count, 0xaa);
		if (ddi_getb(io_hdl1, ata_ctlp->ac_count) != 0xaa)
			continue;

		goto wait_for_not_busy;
	}
	return (FALSE);

wait_for_not_busy:

	/*
	 * Now wait upto 31 seconds for BUSY to clear.
	 */
	(void) ata_wait3(io_hdl2, ata_ctlp->ac_ioaddr2, 0, ATS_BSY,
		ATS_ERR, ATS_BSY, ATS_DF, ATS_BSY, time_left);

	return (TRUE);
}

/*
 *
 * DDI interrupt handler
 *
 */

static u_int
ata_intr(
	caddr_t arg)
{
	ata_ctl_t *ata_ctlp;
	int	   one_shot = 1;

	ata_ctlp = (ata_ctl_t *)arg;

	return (ghd_intr(&ata_ctlp->ac_ccc, (void *)&one_shot));
}


/*
 *
 * GHD ccc_get_status callback
 *
 */

static int
ata_get_status(
	void *hba_handle,
	void *intr_status)
{
	ata_ctl_t *ata_ctlp = (ata_ctl_t *)hba_handle;
	uchar_t	   status;

	ADBG_TRACE(("ata_get_status entered\n"));

	/*
	 * ignore interrupts before ata_attach completes
	 */
	if (!(ata_ctlp->ac_flags & AC_ATTACHED))
		return (FALSE);

	/*
	 * can't be interrupt pending if nothing active
	 */
	switch (ata_ctlp->ac_state) {
	case AS_IDLE:
		return (FALSE);
	case AS_ACTIVE0:
	case AS_ACTIVE1:
		ASSERT(ata_ctlp->ac_active_drvp != NULL);
		ASSERT(ata_ctlp->ac_active_pktp != NULL);
		break;
	}

	/*
	 * If this is a PCI-IDE controller, check the PCI-IDE controller's
	 * interrupt status latch. But don't clear it yet.
	 *
	 * AC_BMSTATREG_PIO_BROKEN flag is used currently for
	 * CMD chips with device id 0x646. Since the interrupt bit on
	 * Bus master IDE register is not usable when in PIO mode,
	 * this chip is treated as a legacy device for interrupt
	 * indication.  See bug id 4147137. The following code for CMD
	 * chips may need to be revisited when we enable support for dma.
	 */

	if ((ata_ctlp->ac_pciide) &&
			!((ata_ctlp->ac_flags & AC_BMSTATREG_PIO_BROKEN) &&
			(ata_ctlp->ac_pciide_bm == FALSE))) {
		if (!ata_pciide_status_pending(ata_ctlp))
			return (FALSE);
	} else {
		/*
		 * Interrupts from legacy ATA/IDE controllers are
		 * edge-triggered but the dumb legacy ATA/IDE controllers
		 * and drives don't have an interrupt status bit.
		 *
		 * Use a one_shot variable to make sure we only return
		 * one status per interrupt.
		 */
		if (intr_status != NULL) {
			int *one_shot = (int *)intr_status;

			if (*one_shot == 1)
				*one_shot = 0;
			else
				return (FALSE);
		}
	}

	/* check if device is still busy */

	status = ddi_getb(ata_ctlp->ac_iohandle2, ata_ctlp->ac_altstatus);
	if (status & ATS_BSY)
		return (FALSE);
	return (TRUE);
}


/*
 *
 * get the current status and clear the IRQ
 *
 */

int
ata_get_status_clear_intr(
	ata_ctl_t *ata_ctlp,
	ata_pkt_t *ata_pktp)
{
	uchar_t	status;

	/*
	 * Here's where we clear the PCI-IDE interrupt latch. If this
	 * request used DMA mode then we also have to check and clear
	 * the DMA error latch at the same time.
	 */

	if (ata_pktp->ap_pciide_dma) {
		if (ata_pciide_status_dmacheck_clear(ata_ctlp))
			ata_pktp->ap_flags |= AP_ERROR | AP_TRAN_ERROR;
	} else if ((ata_ctlp->ac_pciide) &&
			!((ata_ctlp->ac_flags & AC_BMSTATREG_PIO_BROKEN) &&
			(ata_ctlp->ac_pciide_bm == FALSE))) {
		/*
		 * Some requests don't use DMA mode and therefore won't
		 * set the DMA error latch, but we still have to clear
		 * the interrupt latch.
		 */
		(void) ata_pciide_status_clear(ata_ctlp);
	}

	/*
	 * this clears the drive's interrupt
	 */
	status = ddi_getb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_status);
	ADBG_TRACE(("ata_get_status_clear_intr: 0x%x\n", status));
	return (status);
}



/*
 *
 * GHD interrupt handler
 *
 */

/* ARGSUSED */
static void
ata_process_intr(
	void *hba_handle,
	void *intr_status)
{
	ata_ctl_t *ata_ctlp = (ata_ctl_t *)hba_handle;
	int	   watchdog;
	uchar_t	   fsm_func;
	int	   rc;

	ADBG_TRACE(("ata_process_intr entered\n"));

	/*
	 * process the ATA or ATAPI interrupt
	 */

	fsm_func = ATA_FSM_INTR;
	for (watchdog = ata_process_intr_watchdog; watchdog > 0; watchdog--) {
		rc =  ata_ctlr_fsm(fsm_func, ata_ctlp, NULL, NULL, NULL);

		switch (rc) {
		case ATA_FSM_RC_OKAY:
			return;

		case ATA_FSM_RC_BUSY:	/* wait for the next interrupt */
			return;

		case ATA_FSM_RC_INTR:	/* re-invoke the FSM */
			fsm_func = ATA_FSM_INTR;
			break;

		case ATA_FSM_RC_FINI:	/* move a request to done Q */
			fsm_func = ATA_FSM_FINI;
			break;
		}
	}
	ADBG_WARN(("ata_process_intr: watchdog\n"));
}



/*
 *
 * GHD ccc_hba_start callback
 *
 */

static int
ata_hba_start(
	void *hba_handle,
	gcmd_t *gcmdp)
{
	ata_ctl_t *ata_ctlp;
	ata_drv_t *ata_drvp;
	ata_pkt_t *ata_pktp;
	uchar_t	   fsm_func;
	int	   request_started;
	int	   watchdog;

	ADBG_TRACE(("ata_hba_start entered\n"));

	ata_ctlp = (ata_ctl_t *)hba_handle;

	if (ata_ctlp->ac_active_drvp != NULL) {
		ADBG_WARN(("ata_hba_start drvp not null\n"));
		return (FALSE);
	}
	if (ata_ctlp->ac_active_pktp != NULL) {
		ADBG_WARN(("ata_hba_start pktp not null\n"));
		return (FALSE);
	}

	ata_pktp = GCMD2APKT(gcmdp);
	ata_drvp = GCMD2DRV(gcmdp);

	/*
	 * which drive?
	 */
	if (ata_drvp->ad_targ == 0)
		fsm_func = ATA_FSM_START0;
	else
		fsm_func = ATA_FSM_START1;

	/*
	 * start the request
	 */
	request_started = FALSE;
	for (watchdog = ata_hba_start_watchdog; watchdog > 0; watchdog--) {
		switch (ata_ctlr_fsm(fsm_func, ata_ctlp, ata_drvp, ata_pktp,
				NULL)) {
		case ATA_FSM_RC_OKAY:
			request_started = TRUE;
			goto fsm_done;

		case ATA_FSM_RC_BUSY:
			/* if first time, tell GHD to requeue the request */
			goto fsm_done;

		case ATA_FSM_RC_INTR:
			/*
			 * The start function polled for the next
			 * bus phase, now fake an interrupt to process
			 * the next action.
			 */
			request_started = TRUE;
			fsm_func = ATA_FSM_INTR;
			ata_drvp = NULL;
			ata_pktp = NULL;
			break;

		case ATA_FSM_RC_FINI: /* move request to the done queue */
			request_started = TRUE;
			fsm_func = ATA_FSM_FINI;
			ata_drvp = NULL;
			ata_pktp = NULL;
			break;
		}
	}
	ADBG_WARN(("ata_hba_start: watchdog\n"));

fsm_done:
	return (request_started);

}

static int
ata_check_pciide_blacklist(
	dev_info_t *dip,
	uint_t flags)
{
	ushort_t vendorid;
	ushort_t deviceid;
	pcibl_t	*blp;
	int	*propp;
	u_int	 count;
	int	 rc;


	vendorid = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(dip),
				    DDI_PROP_DONTPASS, "vendor-id", 0);
	deviceid = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_get_parent(dip),
				    DDI_PROP_DONTPASS, "device-id", 0);

	/*
	 * first check for a match in the "pci-ide-blacklist" property
	 */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, 0,
		"pci-ide-blacklist", &propp, &count);

	if (rc == DDI_PROP_SUCCESS) {
		int	count = (count * sizeof (u_int)) / sizeof (pcibl_t);
		blp = (pcibl_t *)propp;
		while (count--) {
			/* check for matching ID */
			if ((vendorid & blp->b_vmask)
					!= (blp->b_vendorid & blp->b_vmask))
				continue;
			if ((deviceid & blp->b_dmask)
					!= (blp->b_deviceid & blp->b_dmask))
				continue;

			/* got a match */
			if (blp->b_flags & flags) {
				ddi_prop_free(propp);
				return (TRUE);
			} else {
				ddi_prop_free(propp);
				return (FALSE);
			}
		}
		ddi_prop_free(propp);
	}

	/*
	 * then check the built-in blacklist
	 */
	for (blp = ata_pciide_blacklist; blp->b_vendorid; blp++) {
		if ((vendorid & blp->b_vmask) != blp->b_vendorid)
			continue;
		if ((deviceid & blp->b_dmask) != blp->b_deviceid)
			continue;
		if (!(blp->b_flags & flags))
			continue;
		return (TRUE);
	}
	return (FALSE);
}

int
ata_check_drive_blacklist(
	struct ata_id *aidp,
	uint_t flags)
{
	atabl_t	*blp;

	for (blp = ata_drive_blacklist; blp->b_model; blp++) {
		if (!ata_strncmp(blp->b_model, aidp->ai_model,
				sizeof (aidp->ai_model)))
			continue;
		if (blp->b_flags & flags)
			return (TRUE);
		return (FALSE);
	}
	return (FALSE);
}

/*
 * Queue a request to perform some sort of internally
 * generated command. When this request packet reaches
 * the front of the queue (*func)() is invoked.
 *
 */

int
ata_queue_cmd(
	int	  (*func)(ata_ctl_t *, ata_drv_t *, ata_pkt_t *),
	void	  *arg,
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	gtgt_t	  *gtgtp)
{
	ata_pkt_t	*ata_pktp;
	gcmd_t		*gcmdp;
	int		 rc;

	if (!(gcmdp = ghd_gcmd_alloc(gtgtp, sizeof (*ata_pktp), TRUE))) {
		ADBG_ERROR(("atapi_id_update alloc failed\n"));
		return (FALSE);
	}


	/* set the back ptr from the ata_pkt to the gcmd_t */
	ata_pktp = GCMD2APKT(gcmdp);
	ata_pktp->ap_gcmdp = gcmdp;
	ata_pktp->ap_hd = ata_drvp->ad_drive_bits;
	ata_pktp->ap_bytes_per_block = ata_drvp->ad_bytes_per_block;

	/*
	 * over-ride the default start function
	 */
	ata_pktp = GCMD2APKT(gcmdp);
	ata_pktp->ap_start = func;
	ata_pktp->ap_complete = NULL;
	ata_pktp->ap_v_addr = (caddr_t)arg;

	/*
	 * add it to the queue, when it gets to the front the
	 * ap_start function is called.
	 */
	rc = ghd_transport(&ata_ctlp->ac_ccc, gcmdp, gcmdp->cmd_gtgtp,
		0, TRUE, NULL);

	if (rc != TRAN_ACCEPT) {
		/* this should never, ever happen */
		return (FALSE);
	}

	if (ata_pktp->ap_flags & AP_ERROR)
		return (FALSE);
	return (TRUE);
}
