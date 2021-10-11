/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ata_common.c	1.79	99/07/15 SMI"

#if defined(i386)
#define	_mca_bus_supported
#define	_eisa_bus_supported
#define	_isa_bus_supported
#endif

#if defined(__ppc)
#define	_isa_bus_supported
#endif

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/debug.h>
#include <sys/note.h>
#if defined(i386)
#include <sys/nvm.h>
#include <sys/eisarom.h>
#endif
#include <sys/dada/adapters/ata_common.h>
#include <sys/dada/adapters/ata_disk.h>
#include <sys/dada/adapters/atapi.h>


/*
 * Solaris Entry Points.
 */
static int ata_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int ata_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int ata_bus_ctl(dev_info_t *d,
	dev_info_t *r, ddi_ctl_enum_t o, void *a, void *v);
static u_int ata_intr(caddr_t arg);

/*
 * GHD Entry points
 */
static int ata_get_status(void *hba_handle, void *intr_status, int chno);
static void ata_process_intr(void *hba_handle, void *intr_status, int chno);
static int ata_hba_start(void *handle, gcmd_t *cmdp);
static void ata_hba_complete(void *handle, gcmd_t *cmdp, int do_callback);
static int ata_timeout_func(void *hba_handle,
		gcmd_t  *gcmdp, gtgt_t  *gtgtp, gact_t  action);

/*
 * Local Function Prototypes
 */
static struct ata_controller *ata_init_controller(dev_info_t *dip);
static void ata_destroy_controller(dev_info_t *dip);
static struct ata_drive *ata_init_drive(struct ata_controller *ata_ctlp,
		u_char targ, u_char lun);
static void ata_destroy_drive(struct ata_drive *ata_drvp);
static int ata_drive_type(struct ata_controller *ata_ctlp,
		ushort *secbuf, int chno);
static int ata_reset_bus(struct ata_controller *ata_ctlp, int chno);
extern int ata_disk_set_rw_multiple(struct ata_drive *ata_drvp);
void ata_ghd_complete_wraper(struct ata_controller *ata_ctlp,
			struct ata_pkt *ata_pktp, int chno);
void
make_prd(gcmd_t *gcmdp, ddi_dma_cookie_t *cookie, int single_seg, int num_segs);
void write_prd(struct ata_controller *ata_ctlp, struct ata_pkt *ata_pktp);
int prd_init(struct ata_controller *ata_ctlp, int chno);
void change_endian(unsigned char *string, int length);

#if defined(_eisa_bus_supported)
static int ata_detect_dpt(dev_info_t *dip, uint8_t *ioaddr);
static int ata_check_io_addr(NVM_PORT *ports, uint8_t *ioaddr);
#endif

/*
 * Linked list of all the controllers
 */
struct ata_controller *ata_head = NULL;
kmutex_t ata_global_mutex;

/*
 * Local static data
 */
void *ata_state;
#if defined(i386)
static int  irq13_addr;
#endif

static	tmr_t	ata_timer_conf; /* single timeout list for all instances */
static	clock_t	ata_watchdog_usec = 100000; /* check timeouts every 100 ms */

/*
 * external dependencies
 */
char _depends_on[] = "misc/scsi misc/dada";

/* bus nexus operations */
static struct bus_ops ata_bus_ops, *scsa_bus_ops_p;

static struct dev_ops	ata_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	ata_attach,		/* attach */
	ata_detach,		/* detach */
	nulldev,		/* reset */
	(struct cb_ops *)0,	/* driver operations */
	NULL,			/* bus operations */
	ddi_power
};


/* driver loadable module wrapper */
static	struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"ATA AT-bus attachment disk controller Driver",	/* module name */
	&ata_ops,					/* driver ops */
};

static	struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

static ddi_device_acc_attr_t dev_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

static ddi_device_acc_attr_t dev_attr1 = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_BE_ACC,
	DDI_STRICTORDER_ACC
};

ddi_dma_attr_t ata_dma_attrs = {
	DMA_ATTR_V0,	/* attribute layout version */
	0x0ull,		/* address low - should be 0 (longlong) */
	0xffffffffull,	/* address high - 32-bit max range */
	0x00ffffffull,	/* count max - max DMA object size */
	4,		/* allocation alignment requirements */
	0x78,		/* burstsizes - binary encoded values */
	1,		/* minxfer - gran. of DMA engine */
	0x00ffffffull,	/* maxxfer - gran. of DMA engine */
	0xffffffffull,	/* max segment size (DMA boundary) */
	1,		/* scatter/gather list length */
	512,		/* granularity - device transfer size */
	0		/* flags, set to 0 */
};

extern int atapi_work_pio;
/*
 * warlock directives
 */
_NOTE(MUTEX_PROTECTS_DATA(ata_global_mutex, ata_head))
_NOTE(MUTEX_PROTECTS_DATA(ata_global_mutex, ata_controller::ac_next))
_NOTE(MUTEX_PROTECTS_DATA(ata_controller::ac_hba_mutex, \
				ata_controller::ac_actv_chnl))
_NOTE(MUTEX_PROTECTS_DATA(ata_controller::ac_hba_mutex, \
				ata_controller::ac_pending))
_NOTE(MUTEX_PROTECTS_DATA(ata_controller::ac_hba_mutex, \
				ata_controller::ac_polled_finish))

_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", ata_bus_ops))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsa_bus_ops_p))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", dcd_address ata_drive))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", ghd_target_instance))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsi_hba_tran::tran_tgt_private))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsi_status))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsi_cdb))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsi_device))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", dcd_device::dcd_dev))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", dcd_device::dcd_ident))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", dcd_hba_tran::tran_tgt_private))
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", ghd_cmd scsi_pkt dcd_pkt ata_pkt))
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", buf))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", \
				ata_controller::ac_intr_unclaimed))

_NOTE(READ_ONLY_DATA(ata_watchdog_usec))



int
_init(void)
{
	int err;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	if ((err = ddi_soft_state_init(&ata_state,
			sizeof (struct ata_controller), 0)) != 0) {
		return (err);
	}

	if ((err = scsi_hba_init(&modlinkage)) != 0) {
		ddi_soft_state_fini(&ata_state);
		return (err);
	}
	/*
	 * save pointer to SCSA provided bus_ops struct
	 */
	scsa_bus_ops_p = ata_ops.devo_bus_ops;

	/*
	 * make a copy of SCSA bus_ops
	 */
	ata_bus_ops = *(ata_ops.devo_bus_ops);

	/*
	 * modify our bus_ops to call our bus_ctl routine
	 */
	ata_bus_ops.bus_ctl = ata_bus_ctl;

	/*
	 * patch our bus_ops into the dev_ops struct
	 */
	ata_ops.devo_bus_ops = &ata_bus_ops;

	if ((err = mod_install(&modlinkage)) != 0) {
		scsi_hba_fini(&modlinkage);
		ddi_soft_state_fini(&ata_state);
	}

	/*
	 * Initialize the per driver timer info.
	 */
	ghd_timer_init(&ata_timer_conf, drv_usectohz(ata_watchdog_usec));

	mutex_init(&ata_global_mutex, NULL, MUTEX_DRIVER, NULL);
	return (err);
}

int
_fini(void)
{
	int err;

	/* CONSTCOND */
	if ((err = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&ata_global_mutex);
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


/*
 * driver attach entry point
 */
static int
ata_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct ata_controller	*ata_ctlp;
	struct ata_drive 	*ata_drvp;
	struct ata_pkt	*ata_pktp;
	u_char 	targ, lun, lastlun, val;
	u_short	control;
	u_short	enable_mask = 0;
	int	atapi_count = 0, disk_count = 0, one_atapi = 0;
	u_char	ch0_rahead, ch1_rahead;
	int	instance;
	int 	chno;


	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	ADBG_TRACE(("ata_attach entered\n"));

	switch (cmd) {
	case DDI_ATTACH:
		/*
		 * initialize controller
		 */
		ata_ctlp = ata_init_controller(dip);

		if (ata_ctlp == NULL) {
			goto errout;
		}

		ata_ctlp->ac_flags |= AC_ATTACH_IN_PROGRESS;

		/*
		 * initialize drives
		 */
		for (targ = 0; targ < ATA_MAXTARG; targ++) {
			ata_drvp = ata_init_drive(ata_ctlp, targ, 0);

			if (ata_drvp == NULL) {
				/*
				 * Do not check for slave in case master
				 * is not there.
				*/
				if ((targ == 0) || (targ == 2)) targ++;
				continue;
			}

			if (ATAPIDRV(ata_drvp)) {
				atapi_count++;
				lastlun = ata_drvp->ad_id.dcd_lastlun & 0x03;
				/* Initialize higher LUNs, if there are any */
				for (lun = 1; lun <= lastlun; lun++)
					(void) ata_init_drive(ata_ctlp,
								targ, lun);
			} else {
				disk_count++;
				lastlun = 0;
				/* If it is ATA disks we donot have LUNs */
			}
		}

		if ((atapi_count == 0) && (disk_count == 0)) {
			if (ata_ctlp) {
				ata_ctlp->ac_flags &= ~AC_ATTACH_IN_PROGRESS;
			}
			return (DDI_SUCCESS);
		}

		/* reset */

		if ((ata_ctlp->ac_vendor_id == NSVID) &&
			(ata_ctlp->ac_device_id == NSDID)) {
			ddi_put16(ata_ctlp->ata_conf_handle,
				(ushort_t *)ata_ctlp->ata_conf_addr + CTRLW,
				(ddi_get16(ata_ctlp->ata_conf_handle,
				(ushort_t *)ata_ctlp->ata_conf_addr + CTRLW) |
					RESETCTL));
		}
		ddi_put8(ata_ctlp->ata_datap1[0], ata_ctlp->ac_devctl[0],
			ATDC_D3| ATDC_NIEN|ATDC_SRST);
		drv_usecwait(30000);
		ddi_put8(ata_ctlp->ata_datap1[0], ata_ctlp->ac_devctl[0],
				ATDC_D3);
		drv_usecwait(30000);

		/*
		 * Issue reset for the secondary channel
		 */
		ddi_put8(ata_ctlp->ata_datap1[1], ata_ctlp->ac_devctl[1],
			ATDC_D3 | ATDC_NIEN|ATDC_SRST);
		drv_usecwait(30000);
		ddi_put8(ata_ctlp->ata_datap1[1], ata_ctlp->ac_devctl[1],
			ATDC_D3);
		drv_usecwait(90000);

		if ((ata_ctlp->ac_vendor_id == NSVID) &&
			(ata_ctlp->ac_device_id == NSDID)) {
			if (CTL2DRV(ata_ctlp, 0, 0) || CTL2DRV(ata_ctlp, 1, 0))
				enable_mask = CH1MASK;
			if (CTL2DRV(ata_ctlp, 2, 0) || CTL2DRV(ata_ctlp, 3, 0))
				enable_mask |= CH2MASK;
			control = ddi_get16(ata_ctlp->ata_conf_handle,
				(ushort_t *)ata_ctlp->ata_conf_addr + CTRLW);
			control &= (u_short)~(enable_mask);
			control &= ~RESETCTL;
			ddi_put16(ata_ctlp->ata_conf_handle, (ushort_t *)
				ata_ctlp->ata_conf_addr + CTRLW, control);
			ata_ctlp->ac_piortable[0] = (u_char)NPIOR0;
			ata_ctlp->ac_piortable[1] = (u_char)NPIOR1;
			ata_ctlp->ac_piortable[2] = (u_char)NPIOR2;
			ata_ctlp->ac_piortable[3] = (u_char)NPIOR3;
			ata_ctlp->ac_piortable[4] = (u_char)NPIOR4;
			ata_ctlp->ac_piowtable[0] = (u_char)NPIOW0;
			ata_ctlp->ac_piowtable[1] = (u_char)NPIOW1;
			ata_ctlp->ac_piowtable[2] = (u_char)NPIOW2;
			ata_ctlp->ac_piowtable[3] = (u_char)NPIOW3;
			ata_ctlp->ac_piowtable[4] = (u_char)NPIOW4;
			ata_ctlp->ac_dmartable[0] = (u_char)NDMAR0;
			ata_ctlp->ac_dmartable[1] = (u_char)NDMAR1;
			ata_ctlp->ac_dmartable[2] = (u_char)NDMAR2;
			ata_ctlp->ac_dmawtable[0] = (u_char)NDMAW0;
			ata_ctlp->ac_dmawtable[1] = (u_char)NDMAW1;
			ata_ctlp->ac_dmawtable[2] = (u_char)NDMAW2;
		} else {
			ata_ctlp->ac_piortable[0] = (u_char)PIOR0;
			ata_ctlp->ac_piortable[1] = (u_char)PIOR1;
			ata_ctlp->ac_piortable[2] = (u_char)PIOR2;
			ata_ctlp->ac_piortable[3] = (u_char)PIOR3;
			ata_ctlp->ac_piortable[4] = (u_char)PIOR4;
			ata_ctlp->ac_piowtable[0] = (u_char)PIOW0;
			ata_ctlp->ac_piowtable[1] = (u_char)PIOW1;
			ata_ctlp->ac_piowtable[2] = (u_char)PIOW2;
			ata_ctlp->ac_piowtable[3] = (u_char)PIOW3;
			ata_ctlp->ac_piowtable[4] = (u_char)PIOW4;
			ata_ctlp->ac_dmartable[0] = (u_char)DMAR0;
			ata_ctlp->ac_dmartable[1] = (u_char)DMAR1;
			ata_ctlp->ac_dmartable[2] = (u_char)DMAR2;
			ata_ctlp->ac_dmawtable[0] = (u_char)DMAW0;
			ata_ctlp->ac_dmawtable[1] = (u_char)DMAW1;
			ata_ctlp->ac_dmawtable[2] = (u_char)DMAW2;
		}

		/*
		 * Now we have an idea of
		 * what devies exist now program the
		 * read ahead count
		 */

		if ((ata_ctlp->ac_vendor_id == CMDVID) &&
			(ata_ctlp->ac_device_id == CMDDID)) {
			ch0_rahead = ddi_get8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr + 0x51);
			ch0_rahead |= 0xC0;
			ch1_rahead = ddi_get8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr + 0x57);
			ch1_rahead |= 0xC;
			for (targ = 0; targ < ATA_MAXTARG; targ++) {
				ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
				if (ata_drvp) {
					if (ata_drvp->ad_flags & AD_DISK) {
						switch (targ) {
						case 0 :
							ch0_rahead &= ~0x40;
			ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_conf_addr + 0x53, 0x80);
							break;
						case 1 :
							ch0_rahead &= ~0x80;
			ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_conf_addr + 0x55, 0x80);
							break;
						case 2 :
							ch1_rahead &= ~0x4;
							break;
						case 3 :
							ch1_rahead &= ~0x8;
							break;
						}
					} else if (ata_drvp->ad_flags &
							AD_ATAPI) {
						if ((targ == 2) ||
							(targ == 3)) {
							one_atapi = 1;
						}
					}
				}
			}
			ddi_put8(ata_ctlp->ata_conf_handle,
					(uchar_t *)ata_ctlp->ata_conf_addr +
					0x51, ch0_rahead);
			if (!one_atapi) {
				ch1_rahead &= 0x3f;
				ch1_rahead |= 0x80;
			}
			ddi_put8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr + 0x57,
				ch1_rahead);
		}


		/*
		 * initialize atapi/ata_dsk modules if we have at least
		 * one drive of that type.
		 */
		if (atapi_count) {
			if (atapi_init(ata_ctlp) != SUCCESS)
				goto errout;
			ata_ctlp->ac_flags |= AC_ATAPI_INIT;
		}

		if (disk_count) {
			if (ata_disk_init(ata_ctlp) != SUCCESS)
				goto errout;
			ata_ctlp->ac_flags |= AC_DISK_INIT;
		}
		ddi_report_dev(dip);
		ata_ctlp->ac_flags &= ~AC_ATTACH_IN_PROGRESS;
		/*
		 * Add to the ctlr list for debugging
		 */
		mutex_enter(&ata_global_mutex);
		if (ata_head) {
			ata_ctlp->ac_next = ata_head;
			ata_head = ata_ctlp;
		} else {
			ata_head = ata_ctlp;
		}
		mutex_exit(&ata_global_mutex);

		ata_ctlp->ac_cccp[0] = &ata_ctlp->ac_ccc[0];
		ata_ctlp->ac_cccp[1] = &ata_ctlp->ac_ccc[1];
		/*
		 * Clear any pending intterupt
		 */
		(void) ddi_get8(ata_ctlp->ata_datap[0],
				ata_ctlp->ac_status[0]);
		(void) ddi_get8(ata_ctlp->ata_datap[1],
				ata_ctlp->ac_status[1]);
		(void) ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50), 0x4);
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr +0x57),
			ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57)) | 0x10);

		/*
		 * Clear all the interrupt pending status indicated in DMA
		 * status register.
		 */
		chno = 0;
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));

		chno = 1;
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));


		if (ddi_add_intr(dip, 0, &ata_ctlp->ac_iblock, NULL,
				ata_intr, (caddr_t)ata_ctlp) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}

		/*
		 * Enable the interrupts for both the channels
		 * as OBP disables it.
		 */
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1);
		val &= 0x3;
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1, val);

		/*
		 * Initialize power management bookkeeping; components are
		 * created idle.
		 */
		if (pm_create_components(dip, 1) == DDI_SUCCESS) {
			pm_set_normal_power(dip, 0, 1);
		} else {
			return (DDI_FAILURE);
		}


		return (DDI_SUCCESS);

	case DDI_PM_RESUME:
	case DDI_RESUME:
		instance = ddi_get_instance(dip);
		ata_ctlp = ddi_get_soft_state(ata_state, instance);

		ASSERT(ata_ctlp != NULL);

		ata_ctlp->ac_suspended = 0;
		/*
		 * Reset the channel and set all the targets in right mode
		 */
		chno = 0;
		if (ata_ctlp->ac_active[chno] != NULL) {
			ata_pktp = ata_ctlp->ac_active[chno];
			ata_drvp = CTL2DRV(ata_ctlp, ata_pktp->ap_targ, 0);
			(void) ghd_tran_reset_bus(&ata_ctlp->ac_ccc[chno],
				ata_drvp->ad_gtgtp, NULL);
		} else {
			/*
			 * We cannot choose a arbit drive and call
			 * ghd_tran_reset_bus as there is a possiblility that
			 * drive doesnot exist at all.
			 */
			(void) ata_reset_bus(ata_ctlp, chno);
		}
		chno = 1;
		if (ata_ctlp->ac_active[chno] != NULL) {
			ata_pktp = ata_ctlp->ac_active[chno];
			ata_drvp = CTL2DRV(ata_ctlp, ata_pktp->ap_targ, 0);
			(void) ghd_tran_reset_bus(&ata_ctlp->ac_ccc[chno],
				ata_drvp->ad_gtgtp, NULL);
		} else {
			/*
			 * We cannot choose a arbit drive and call
			 * ghd_tran_reset_bus as there is a possiblility that
			 * drive doesnot exist at all.
			 */
			(void) ata_reset_bus(ata_ctlp, chno);
		}


		/*
		 * There is no need for any watch kind of timeout to be
		 * started. In this case there is no such watch routine.
		 */

		/*
		 * Just do the non standard hack to enable the interrupt to
		 * take care of "Level 4 interrupt not serviced
		 */
		if (ddi_add_intr(dip, 0, &ata_ctlp->ac_iblock, NULL,
				ata_intr, (caddr_t)ata_ctlp) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}
		/*
		 * Enable the interrupts for both the channels
		 * as OBP disables it.
		 */
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1);
		val &= 0x3;
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1, val);

		return (DDI_SUCCESS);

	default:

errout:
		if (ata_ctlp) {
			ata_ctlp->ac_flags &= ~AC_ATTACH_IN_PROGRESS;
		}
		(void) ata_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
	}
}

/*
 * driver detach entry point
 */
static int
ata_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance;
	struct	ata_controller *ata_ctlp, *ata;
	struct	ata_drive *ata_drvp;
	struct  ata_pkt *ata_pktp;
	int	i, j;

	ADBG_TRACE(("ata_detach entered\n"));

	switch (cmd) {
	case DDI_DETACH:
		instance = ddi_get_instance(dip);
		ata_ctlp = ddi_get_soft_state(ata_state, instance);

		if (!ata_ctlp)
			return (DDI_SUCCESS);

		/*
		 * destroy ata module
		 */
		if (ata_ctlp->ac_flags & AC_DISK_INIT)
			ata_disk_destroy(ata_ctlp);

		mutex_enter(&ata_global_mutex);
		if (ata_head == ata_ctlp) {
			ata_head = ata_ctlp->ac_next;
			ata_ctlp->ac_next = NULL;
		} else {
			for (ata = ata_head; ata; ata = ata->ac_next) {
				if (ata->ac_next == ata_ctlp) {
					ata->ac_next = ata_ctlp->ac_next;
					ata_ctlp->ac_next = NULL;
					break;
				}
			}
		}
		mutex_exit(&ata_global_mutex);

		/*
		 * destroy atapi module
		 */
		if (ata_ctlp->ac_flags & AC_ATAPI_INIT)
			atapi_destroy(ata_ctlp);

		/*
		 * destroy drives
		 */
		for (i = 0; i < ATA_MAXTARG; i++) {
			for (j = 0; j < ATA_MAXLUN; j++) {
				ata_drvp = CTL2DRV(ata_ctlp, i, j);
				if (ata_drvp != NULL)
					ata_destroy_drive(ata_drvp);
			}
		}

		/*
		 * destroy controller
		 */
		ata_destroy_controller(dip);

		ddi_prop_remove_all(dip);

		return (DDI_SUCCESS);
	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		/* Get the Controller Pointer */
		instance = ddi_get_instance(dip);
		ata_ctlp = ddi_get_soft_state(ata_state, instance);
		/*
		 * A non standard hack to take care of interrupts not
		 * serviced problem with cpr
		 */
		ddi_remove_intr(dip, 0, ata_ctlp->ac_iblock);

		/*
		 * There are no timeouts which are set to be untimedout
		 * The call to ghd_complete should take care of those
		 * untimeout for any command which has not timedout when
		 * a command is active.
		 */
		/* reset the first channel */
		if ((ata_ctlp->ac_active[0] != NULL) ||
			(ata_ctlp->ac_overlap[0] != NULL)) {
			ata_pktp = ata_ctlp->ac_active[0];
			ata_drvp = CTL2DRV(ata_ctlp, ata_pktp->ap_targ, 0);
			(void) ghd_tran_reset_bus(&ata_ctlp->ac_ccc[0],
				ata_drvp->ad_gtgtp, NULL);
		}

		if ((ata_ctlp->ac_active[1] != NULL) ||
			(ata_ctlp->ac_overlap[1] != NULL)) {
			ata_pktp = ata_ctlp->ac_active[1];
			ata_drvp = CTL2DRV(ata_ctlp, ata_pktp->ap_targ, 0);
			(void) ghd_tran_reset_bus(&ata_ctlp->ac_ccc[1],
				ata_drvp->ad_gtgtp, NULL);
		}

		/*
		 * Initial thought was that we need to drain all the commands
		 * That is not required as there will be only one command
		 * active on a channel at any point in time and if we blow it
		 * out it should be OK to retain the commands in the Queue.
		 * Hence there is no need to drain the command.
		 */


		/* Just Set the flag */
		ata_ctlp->ac_suspended = 1;

		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

/*
 * Nexus driver bus_ctl entry point
 */
/*ARGSUSED*/
static int
ata_bus_ctl(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t o, void *a, void *v)
{
	dev_info_t *tdip;
	int target_type, rc, len, drive_type;
	char buf[80];
	struct ata_drive *ata_drvp;
	int instance;
	struct ata_controller *ata_ctlp;

	instance = ddi_get_instance(d);
	ata_ctlp = ddi_get_soft_state(ata_state, instance);

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

		/*
		 * These ops shouldn't be called by a target driver
		 */
		ADBG_ERROR(("ata_bus_ctl: %s%d: invalid op (%d) "
		"from %s%d\n",
		ddi_get_name(d), ddi_get_instance(d),
		o, ddi_get_name(r), ddi_get_instance(r)));
		return (DDI_FAILURE);


	case DDI_CTLOPS_REPORTDEV:
	case DDI_CTLOPS_INITCHILD:
	case DDI_CTLOPS_UNINITCHILD:

		/* these require special handling below */
		break;

	default:
		return (ddi_ctlops(d, r, o, a, v));
	}

	/*
	 * get targets dip
	 */
	if ((o == DDI_CTLOPS_INITCHILD) ||
		(o == DDI_CTLOPS_UNINITCHILD)) {

		tdip = (dev_info_t *)a; /* Getting the childs dip */
	} else {
		tdip = r;
	}

	len = 80;
	target_type = ATA_DEV_DISK;
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, tdip, DDI_PROP_DONTPASS,
		"class_prop", buf, &len) == DDI_PROP_SUCCESS) {
		if (strcmp(buf, "ata") == 0) {
			target_type = ATA_DEV_DISK;
		} else if (strcmp(buf, "atapi") == 0) {
			target_type = ATA_DEV_ATAPI;
		} else {
			ADBG_WARN(("ata_bus_ctl: invalud target class %s\n",
				buf));
			return (DDI_FAILURE);
		}
	} else {
		target_type = ATA_DEV_ATAPI;
		return (DDI_FAILURE);
	}


	if (o == DDI_CTLOPS_INITCHILD) {
		int targ, lun;

		if (ata_ctlp == NULL) {
			ADBG_WARN(("ata_bus_ctl: failed to find ctl struct\n"));
			return (DDI_FAILURE);
		}

		/*
		 * get (target,lun) of child device
		 */
		len = sizeof (int);
		if (ATA_INTPROP(tdip, "target", &targ, &len) != DDI_SUCCESS) {
			ADBG_WARN(("ata_bus_ctl: failed to get targ num\n"));
			return (DDI_FAILURE);
		}
		if (ATA_INTPROP(tdip, "lun", &lun, &len) != DDI_SUCCESS) {
			lun = 0;
		}

		if ((targ < 0) || (targ >= ATA_MAXTARG) ||
			(lun < 0) || (lun >= ATA_MAXLUN)) {
			return (DDI_FAILURE);
		}

		ata_drvp = CTL2DRV(ata_ctlp, targ, lun);

		if (ata_drvp == NULL) {
			return (DDI_FAILURE);	/* no drive */
		}

		/*
		 * get type of device
		 */
		if (ATAPIDRV(ata_drvp)) {
			drive_type = ATA_DEV_ATAPI;
		} else {
			drive_type = ATA_DEV_DISK;
		}

		if (target_type != drive_type) {
			return (DDI_FAILURE);
		}

		/* save pointer to drive struct for ata_disk_bus_ctl */
		ddi_set_driver_private(tdip, (caddr_t)ata_drvp);
	}

	if (target_type == ATA_DEV_ATAPI) {
		rc = scsa_bus_ops_p->bus_ctl(d, r, o, a, v);
	} else {
		rc = ata_disk_bus_ctl(d, r, o, a, v, ata_drvp);
	}


	return (rc);
}

/*
 * driver interrupt handler
 */
static u_int
ata_intr(caddr_t arg)
{
	struct ata_controller *ata_ctlp;
	int one_shot = 1, ret1, ret2;
	unsigned char chno;

	ret1 = ret2 = DDI_INTR_UNCLAIMED;
	ata_ctlp = (struct ata_controller *)arg;

	if (ata_ctlp->ac_flags & AC_ATTACH_IN_PROGRESS) {
		goto clear_intr;
	}
	if (ata_ctlp->ac_simplex == 1) {
		mutex_enter(&ata_ctlp->ac_hba_mutex);
		if (ata_ctlp->ac_actv_chnl != 2) {
			int chno = ata_ctlp->ac_actv_chnl;

			mutex_exit(&ata_ctlp->ac_hba_mutex);
			ret1 = ghd_intr(&ata_ctlp->ac_ccc[chno],
				(void *)&one_shot, chno);
			if (ret1 == DDI_INTR_CLAIMED) {
				return (ret1);
			}
			mutex_enter(&ata_ctlp->ac_hba_mutex);
		}
		mutex_exit(&ata_ctlp->ac_hba_mutex);
	} else {
		/*
		 * We donot know which channel is the intr for
		 * so check and process both the channels
		 */
		ret1 = ghd_intr(&ata_ctlp->ac_ccc[0], (void *)&one_shot, 0);
		if (ret1 != DDI_INTR_CLAIMED) {
			one_shot = 1;
			ret2 = ghd_intr(&ata_ctlp->ac_ccc[1],
					(void *)&one_shot, 1);
		}
	}

	if ((ret1 == DDI_INTR_UNCLAIMED) &&
		(ret2 == DDI_INTR_UNCLAIMED)) {
clear_intr:
		(void) ddi_get8(ata_ctlp->ata_datap[0],
				ata_ctlp->ac_status[0]);
		(void) ddi_get8(ata_ctlp->ata_datap[1],
				ata_ctlp->ac_status[1]);
		(void) ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50), 0x4);
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr +0x57),
			ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57)) | 0x10);

		/*
		 * Clear all the interrupt pending status indicated in DMA
		 * status register.
		 */
		chno = 0;
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));

		chno = 1;
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));
		ata_ctlp->ac_intr_unclaimed++;
	}
	return (DDI_INTR_CLAIMED);
}

/*
 * GHD ccc_get_status callback
 */
/* ARGSUSED */
static int
ata_get_status(void *hba_handle, void *intr_status, int chno)
{
	struct ata_controller *ata_ctlp = NULL;
	struct ata_pkt  *active_pktp = NULL;
	struct ata_drive *ata_drvp = NULL;
	struct scsi_pkt *spktp;
	int val;


	ADBG_TRACE(("ata_get_status entered\n"));
	ata_ctlp = (struct ata_controller *)hba_handle;

	{
		if (ata_ctlp->ac_active[chno]) {
			active_pktp = ata_ctlp->ac_active[chno];
		} else if (ata_ctlp->ac_overlap[chno]) {
			active_pktp = ata_ctlp->ac_overlap[chno];
		}

		if (active_pktp == NULL) {
			return (FALSE);
		}
	}


	ata_drvp = CTL2DRV(ata_ctlp, active_pktp->ap_targ, 0);

	if (ata_drvp->ad_cur_disk_mode == DMA_MODE) {
		val = ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8 * chno +2)) & 0x4;
		if (!val) {
			if (ata_drvp->ad_flags & AD_ATAPI) {
				spktp = APKT2SPKT(active_pktp);
				if ((!(active_pktp->ap_flags & AP_DMA)) ||
				((!(ata_drvp->ad_flags & AD_NO_CDB_INTR)) &&
				(!(spktp->pkt_state & STATE_SENT_CMD)))) {
					val =
					ddi_get8(ata_ctlp->ata_datap1[chno],
					ata_ctlp->ac_altstatus[chno]);
					if (val & ATS_BSY) {
						return (FALSE);
					}
				} else {
					/*
					 * None of the special ATAPI cases
					 */
					return (FALSE);
				}
			} else {
				/*
				 * Return False for the Disk
				 */
				return (FALSE);
			}
		}
	} else {
		val = ddi_get8(ata_ctlp->ata_datap1[chno],
			ata_ctlp->ac_altstatus[chno]);
		if (val & ATS_BSY) {
			return (FALSE);
		}
	}

	return (TRUE);
}

/*
 * GHD ccc_process_intr callback
 */
/* ARGSUSED */
static void
ata_process_intr(void *hba_handle, void *intr_status, int chno)
{
	struct ata_controller *ata_ctlp;
	struct ata_pkt	*active_pktp = NULL;
	int rc;

	ADBG_TRACE(("ata_process_intr entered\n"));

	ata_ctlp = (struct ata_controller *)hba_handle;

	if (ata_ctlp->ac_active[chno]) {
		active_pktp = ata_ctlp->ac_active[chno];
	} else {
		active_pktp = ata_ctlp->ac_overlap[chno];
	}

	rc = active_pktp->ap_intr(ata_ctlp, active_pktp);

	if ((rc != STATUS_NOINTR) && (!(* (int *)intr_status)) &&
		(!(active_pktp->ap_flags & AP_POLL))) {
		/*
		 * We are through the polled route and packet is non-polled.
		 */
		mutex_enter(&ata_ctlp->ac_hba_mutex);
		ata_ctlp->ac_polled_finish++;
		mutex_exit(&ata_ctlp->ac_hba_mutex);
	}

	/*
	 * check if packet completed
	 */
	if (rc == STATUS_PKT_DONE) {
		/*
		 * Be careful to only reset ac_active if it's
		 * the command we just finished.  An overlap
		 * command may have already become active again.
		 */
		if (ata_ctlp->ac_active[chno] == active_pktp) {
			ata_ctlp->ac_active[chno] = NULL;
		}
		ata_ghd_complete_wraper(ata_ctlp, active_pktp, chno);
	}
}


/*
 * GHD ccc_hba_start callback
 */
static int
ata_hba_start(void *hba_handle, gcmd_t *cmdp)
{
	struct ata_controller *ata_ctlp;
	struct ata_pkt *ata_pktp;
	int rc, chno;

	ADBG_TRACE(("ata_hba_start entered\n"));

	ata_ctlp = (struct ata_controller *)hba_handle;
	ata_pktp = GCMD2APKT(cmdp);
	chno = ata_pktp->ap_chno;

	if (ata_ctlp->ac_active[chno] != NULL) {
		return (TRAN_BUSY);
	}

	ata_ctlp->ac_active[chno] = ata_pktp;

	if (ata_ctlp->ac_simplex == 1) {
		mutex_enter(&ata_ctlp->ac_hba_mutex);
		ata_ctlp->ac_pending[chno] = 1;
		if (ata_ctlp->ac_actv_chnl != 2) {
			mutex_exit(&ata_ctlp->ac_hba_mutex);
			return (TRAN_ACCEPT);
		}
		ata_ctlp->ac_actv_chnl = chno;
		mutex_exit(&ata_ctlp->ac_hba_mutex);
	}

	rc = ata_pktp->ap_start(ata_ctlp, ata_pktp);

	if (rc != TRAN_ACCEPT) {
		ata_ctlp->ac_active[chno] = NULL;
	}

	return (rc);
}

/*
 * GHD ccc_hba_complete callback
 */
/* ARGSUSED */
static void
ata_hba_complete(void *hba_handle, gcmd_t *cmdp, int do_callback)
{
	struct ata_pkt *ata_pktp;

	ADBG_TRACE(("ata_hba_complete entered\n"));

	ata_pktp = GCMD2APKT(cmdp);
	ata_pktp->ap_complete(ata_pktp, do_callback);
}

/*
 * GHD ccc_timeout_func callback
 */
static int
ata_timeout_func(void *hba_handle, gcmd_t  *gcmdp,
	gtgt_t  *gtgtp, gact_t  action)
{
	struct ata_controller *ata_ctlp;
	struct ata_drive *ata_drvp;
	struct ata_pkt *ata_pktp;
	int chno;

	ADBG_TRACE(("ata_timeout_func entered\n"));

	ata_ctlp = (struct ata_controller *)hba_handle;
	ata_drvp = GTGTP2ATADRVP(gtgtp);
	chno = ata_drvp->ad_channel;

	if (gcmdp != NULL) {
		ata_pktp = GCMD2APKT(gcmdp);
	} else {
		ata_pktp = NULL;
	}

	if ((ata_pktp != NULL) && (ata_drvp->ad_invalid) &&
		((action == GACTION_EARLY_ABORT) ||
		(action == GACTION_EARLY_TIMEOUT) ||
		(action == GACTION_RESET_BUS))) {
		ata_pktp->ap_flags |= AP_FATAL;
		ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
		return (FALSE);
	}

	switch (action) {
	case GACTION_EARLY_ABORT:
		/*
		 * abort before request was started
		 */
		if (ata_pktp != NULL) {
			ata_pktp->ap_flags |= AP_ABORT;
			ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
		}
		return (TRUE);

	case GACTION_EARLY_TIMEOUT:
		/*
		 * timeout before request was started
		 */
		if (ata_pktp != NULL) {
			ata_pktp->ap_flags |= AP_TIMEOUT;
			ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
		}
		return (TRUE);

	case GACTION_RESET_TARGET:
		/* reset a device */

		/* can't currently reset a single IDE disk drive */
		if (!(ata_drvp->ad_flags & AD_ATAPI)) {
			return (FALSE);
		}

		if (atapi_reset_drive(ata_drvp) == SUCCESS) {
			return (TRUE);
		} else {
			return (FALSE);
		}

	case GACTION_RESET_BUS:
		/* reset bus */
		if (ata_reset_bus(ata_ctlp, chno) == FALSE) {
			if (ata_pktp != NULL) {
				ata_pktp->ap_flags |= AP_FATAL;
				ata_ghd_complete_wraper(ata_ctlp,
					ata_pktp, chno);
			}
			return (FALSE);
		} else {
			return (TRUE);
		}

#ifdef DSC_OVERLAP_SUPPORT
	case GACTION_POLL:
		atapi_dsc_poll(ata_drvp);
		return (TRUE);
#endif
	}

	return (FALSE);
}

/*
 * reset the bus
 */
static int
ata_reset_bus(struct ata_controller *ata_ctlp, int chno)
{
	struct ata_pkt *ata_pktp;
	int  targ;
	struct ata_drive *ata_drvp;
	unsigned char status, val;
	uint32_t conf_stat;


	ADBG_TRACE(("ata_reset_bus entered\n"));

	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {
		ddi_put16(ata_ctlp->ata_conf_handle, (ushort_t *)
		ata_ctlp->ata_conf_addr + CTRLW, (ddi_get16
		(ata_ctlp->ata_conf_handle, (ushort_t *)
		ata_ctlp->ata_conf_addr + CTRLW) | RESETCTL));
	}


	ata_pktp = ata_ctlp->ac_active[chno];

	/*
	 * Stop the DMA bus master operation for that channel
	 */
	ddi_put8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno),
		(ddi_get8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno))  & 0xFE));
	/*
	 * Clear the dma error bit for channel
	 */
	ddi_put8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
		(ddi_get8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));

	/*
	 * Clear the interrupts
	 */
	if (chno == 0) {
	val = ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		if ((val & 0x4) && (ata_ctlp->ac_revision >= 3)) {
			ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50), val | 0x4);
		}
	} else {
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57));
		if ((val & 0x10) && (ata_ctlp->ac_revision >= 3)) {
			ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57),
			val | 0x10);
		}
	}

	ata_ctlp->ac_reset_done = 1;

	if (chno == 1) {
		/*
		 * Enable the secondary channel
		 */
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_conf_addr + 0x51);
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_conf_addr + 0x51, val | 0x8);
	}

	/*
	 * Issue soft reset , wait for some time & enable intterupts
	 */
	ddi_put8(ata_ctlp->ata_datap1[chno],
		ata_ctlp->ac_devctl[chno], ATDC_D3 | ATDC_NIEN | ATDC_SRST);
	drv_usecwait(30000);
	ddi_put8(ata_ctlp->ata_datap1[chno],
		ata_ctlp->ac_devctl[chno], ATDC_D3);
	drv_usecwait(1024*1024);
	/*
	 * Wait for the BSY to go low
	 */
	if (ata_wait(ata_ctlp->ata_datap[chno], ata_ctlp->ac_status[chno], 0,
		ATS_BSY, 3000000, 10)) {
		if (chno == 0)
			targ = 0;
		else
			targ = 2;
		ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
		if (ata_drvp) {
			ata_drvp->ad_invalid = 1;
			cmn_err(CE_WARN, "Drive not ready before set_features");
		}
		targ++;
		ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
		if (ata_drvp)
			ata_drvp->ad_invalid = 1;
		return (FALSE);
	}

	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {
		ddi_put16(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW, (ddi_get16
			(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW) & ~RESETCTL));
	}

	/*
	 * After Resetting the drives connected to that channel should be
	 * set back to the requested mode. Otherwise the DMA willnot happen.
	 * It is done for all targets : TBD with Sanjay
	 */
	for (targ = chno * 2; targ < ((chno + 1) * 2); targ++) {
		ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
		if (ata_drvp == NULL) {
			continue;
		}

		/*
		 * check wether any of the DMA has errored by checking the
		 * Bus Master IDE status register approprite channel
		 */
		status = ddi_get8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr +
					8*ata_drvp->ad_channel +2));
		if (status & 0x02) {
			ddi_put8(ata_ctlp->ata_cs_handle,
					(uchar_t *)(ata_ctlp->ata_cs_addr +
					8 * ata_drvp->ad_channel + 2), 0x02);
			status = ddi_get8(ata_ctlp->ata_cs_handle,
					(uchar_t *)(ata_ctlp->ata_cs_addr +
					8 * ata_drvp->ad_channel + 2));
			conf_stat = ddi_get32(ata_ctlp->ata_conf_handle,
				(uint_t *)(ata_ctlp->ata_conf_addr + 4));
			ddi_put32(ata_ctlp->ata_conf_handle,
			(uint_t *)(ata_ctlp->ata_conf_addr + 4), conf_stat);

			conf_stat = ddi_get32(ata_ctlp->ata_conf_handle,
				(uint_t *)(ata_ctlp->ata_conf_addr + 4));
		}
		ata_drvp->ad_dmamode &= 0x7f;
		if (((ata_drvp->ad_flags & AD_DISK) &&
			(ata_ctlp->ac_dcd_options & DCD_DMA_MODE)) ||
			((ata_drvp->ad_flags & AD_ATAPI) &&
			(ata_drvp->ad_cur_disk_mode == DMA_MODE))) {
			ata_write_config(ata_drvp);
			if (ata_set_feature(ata_drvp, ATA_FEATURE_SET_MODE,
				ata_drvp->ad_dmamode) != SUCCESS) {
				ata_drvp->ad_invalid = 1;
				if ((targ == 0) || (targ == 2)) {
					ata_drvp = CTL2DRV(ata_ctlp, targ+1, 0);
					if (ata_drvp)
						ata_drvp->ad_invalid = 1;
				} else {
					ata_drvp = CTL2DRV(ata_ctlp, targ-1, 0);
					if (ata_drvp)
						ata_drvp->ad_invalid = 1;
				}
				return (FALSE);
			}
			ata_drvp->ad_cur_disk_mode = DMA_MODE;
		} else {
			ata_write_config(ata_drvp);
			if (ata_set_feature(ata_drvp, ATA_FEATURE_SET_MODE,
				ENABLE_PIO_FEATURE |
				(ata_drvp->ad_piomode & 0x07)) != SUCCESS) {
				ata_drvp->ad_invalid = 1;
				if ((targ == 0) || (targ == 2)) {
					ata_drvp = CTL2DRV(ata_ctlp, targ+1, 0);
					if (ata_drvp)
						ata_drvp->ad_invalid = 1;
				} else {
					ata_drvp = CTL2DRV(ata_ctlp, targ-1, 0);
					if (ata_drvp)
						ata_drvp->ad_invalid = 1;
				}
				return (FALSE);
			}

			if (ata_drvp->ad_block_factor > 1) {
				/*
				 * Program the block factor into the drive.
				 * If this fails, then go back to using a
				 * block size of 1.
				 */
				if ((ata_disk_set_rw_multiple(ata_drvp)
						== FAILURE))
					ata_drvp->ad_block_factor = 1;
			}
		}
	}

	/*
	 * Clear the interrupt if pending  by reading config reg
	 */

	ddi_put8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
		(ddi_get8(ata_ctlp->ata_cs_handle,
		(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));

	if (chno == 0) {
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		if ((val & 0x4) && (ata_ctlp->ac_revision >= 3)) {
			ddi_put8(ata_ctlp->ata_conf_handle,
				(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50),
				val | 0x4);
		}
	} else {
		val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57));
		if ((val & 0x10) && (ata_ctlp->ac_revision >= 3)) {
			ddi_put8(ata_ctlp->ata_conf_handle,
				(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57),
				val | 0x10);
		}
	}

	ADBG_TRACE(("value from Interrupt status register is %x\n", val));

	if (ata_pktp != NULL) {
		ata_ctlp->ac_active[chno] = NULL;
		ata_pktp->ap_flags |= AP_BUS_RESET;
		ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
	}

	ata_pktp = ata_ctlp->ac_overlap[chno];

	if (ata_pktp != NULL) {
		ata_ctlp->ac_overlap[chno] = NULL;
		ata_pktp->ap_flags |= AP_BUS_RESET;
		ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
	}

	return (TRUE);
}

/*
 * Initialize a controller structure
 */
static struct ata_controller *
ata_init_controller(dev_info_t *dip)
{
	int instance, chno;
	struct ata_controller *ata_ctlp;
	int	val, len;
	uint8_t *ioaddr1, *ioaddr2;
	u_short control;
	u_char	pif;
	int hw_simplex, ata_simplex = 0;

	ADBG_TRACE(("ata_init_controller entered\n"));

	instance = ddi_get_instance(dip);

	/*
	 * allocate controller structure
	 */
	if (ddi_soft_state_zalloc(ata_state, instance) != DDI_SUCCESS) {
		ADBG_WARN(("ata_init_controller: soft_state_zalloc "
			"failed\n"));
		return (NULL);
	}

	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (ata_ctlp == NULL) {
		ADBG_WARN(("ata_init_controller: failed to find "
				"controller struct\n"));
		return (NULL);
	}

	/*
	 * initialize controller
	 */
	ata_ctlp->ac_dip = dip;

#if defined(i386)
	/*
	 * get property info
	 */
	len = sizeof (int);
	if (ATA_INTPROP(dip, "irq13_share", &irq13_addr, &len) !=
		DDI_PROP_SUCCESS) {
		irq13_addr = 0;
	}
#endif

	if (ddi_regs_map_setup(dip, 1, &ata_ctlp->ata_devaddr[0],
	    0, 8, &dev_attr1, &ata_ctlp->ata_datap[0]) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);

	}

	if (ddi_regs_map_setup(dip, 2, &ata_ctlp->ata_devaddr1[0],
	    0, 4, &dev_attr, &ata_ctlp->ata_datap1[0]) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);

	}



	if (ddi_regs_map_setup(dip, 3, &ata_ctlp->ata_devaddr[1],
	    0, 8, &dev_attr1, &ata_ctlp->ata_datap[1]) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);

	}

	if (ddi_regs_map_setup(dip, 4, &ata_ctlp->ata_devaddr1[1],
	    0, 4, &dev_attr, &ata_ctlp->ata_datap1[1]) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);

	}


	if (ddi_regs_map_setup(dip, 0, &ata_ctlp->ata_conf_addr,
	    0, 0, &dev_attr, &ata_ctlp->ata_conf_handle) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);
	}

	if (ddi_regs_map_setup(dip, 5, &ata_ctlp->ata_cs_addr,
	    0, 16, &dev_attr, &ata_ctlp->ata_cs_handle) != DDI_SUCCESS) {
		ddi_soft_state_free(ata_state, instance);
		return (NULL);
	}

	ata_ctlp->ac_vendor_id = ddi_get16(ata_ctlp->ata_conf_handle,
			((ushort_t *)ata_ctlp->ata_conf_addr + VENDORID));
	ata_ctlp->ac_device_id = ddi_get16(ata_ctlp->ata_conf_handle,
			(ushort_t *)ata_ctlp->ata_conf_addr + DEVICEID);
	ata_ctlp->ac_revision = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_conf_addr + REVISION);

	ata_simplex = ddi_prop_get_int(DDI_DEV_T_ANY,
				dip, 0, "ata-simplex", 0);
	hw_simplex = ddi_get8(ata_ctlp->ata_cs_handle,
			(uint8_t *)(ata_ctlp->ata_cs_addr + 2));
	if ((ata_simplex == 1) || (hw_simplex & 0x80)) {
		ata_ctlp->ac_simplex = 1;
	} else {
		ata_ctlp->ac_simplex = 0;
	}
	ata_ctlp->ac_pending[0] = ata_ctlp->ac_pending[1] = 0;
	ata_ctlp->ac_actv_chnl = 2;

	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {

		pif = ddi_get8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr + PIF);

		pif |= (u_char)(CH1LMODE | CH2LMODE);

		ddi_put8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr + PIF, pif);

		control = ddi_get16(ata_ctlp->ata_conf_handle,
				(ushort_t *)ata_ctlp->ata_conf_addr + CTRLW);

		/*
		 * Channel 2 Interrupt is masked for now
		 */
		control |= (u_short)(CH1MASK | CH2MASK);

		ddi_put16(ata_ctlp->ata_conf_handle,
			(ushort_t *)ata_ctlp->ata_conf_addr + CTRLW, control);
	}

	ioaddr1 = (uint8_t *)(ata_ctlp->ata_devaddr[0]);
	ioaddr2 = (uint8_t *)(ata_ctlp->ata_devaddr1[0]);

	/*
	 * port addresses associated with ioaddr1
	 */
	ata_ctlp->ioaddr1[0]	= ioaddr1;
	ata_ctlp->ac_data[0]	= ioaddr1 + AT_DATA;
	ata_ctlp->ac_error[0]	= ioaddr1 + AT_ERROR;
	ata_ctlp->ac_feature[0]	= ioaddr1 + AT_FEATURE;
	ata_ctlp->ac_count[0]	= ioaddr1 + AT_COUNT;
	ata_ctlp->ac_sect[0]	= ioaddr1 + AT_SECT;
	ata_ctlp->ac_lcyl[0]	= ioaddr1 + AT_LCYL;
	ata_ctlp->ac_hcyl[0]	= ioaddr1 + AT_HCYL;
	ata_ctlp->ac_drvhd[0]	= ioaddr1 + AT_DRVHD;
	ata_ctlp->ac_status[0]	= ioaddr1 + AT_STATUS;
	ata_ctlp->ac_cmd[0]	= ioaddr1 + AT_CMD;

	/*
	 * port addresses associated with ioaddr2
	 */
	ata_ctlp->ioaddr2[0]		= ioaddr2;
	ata_ctlp->ac_altstatus[0] = ioaddr2 + 2;
	ata_ctlp->ac_devctl[0]    = ioaddr2 + 2;
	ata_ctlp->ac_drvaddr[0]   = ioaddr2 + AT_DRVADDR;



	ioaddr1 = (uint8_t *)(ata_ctlp->ata_devaddr[1]);
	ioaddr2 = (uint8_t *)(ata_ctlp->ata_devaddr1[1]);
	ata_ctlp->ioaddr1[1]	= ioaddr1;
	ata_ctlp->ac_data[1]	= ioaddr1 + AT_DATA;
	ata_ctlp->ac_error[1]	= ioaddr1 + AT_ERROR;
	ata_ctlp->ac_feature[1]	= ioaddr1 + AT_FEATURE;
	ata_ctlp->ac_count[1]	= ioaddr1 + AT_COUNT;
	ata_ctlp->ac_sect[1]	= ioaddr1 + AT_SECT;
	ata_ctlp->ac_lcyl[1]	= ioaddr1 + AT_LCYL;
	ata_ctlp->ac_hcyl[1]	= ioaddr1 + AT_HCYL;
	ata_ctlp->ac_drvhd[1]	= ioaddr1 + AT_DRVHD;
	ata_ctlp->ac_status[1]	= ioaddr1 + AT_STATUS;
	ata_ctlp->ac_cmd[1]	= ioaddr1 + AT_CMD;

	/*
	 * port addresses associated with ioaddr2
	 */
	ata_ctlp->ioaddr2[1]	  = ioaddr2;
	ata_ctlp->ac_altstatus[1] = ioaddr2 + 2;
	ata_ctlp->ac_devctl[1]    = ioaddr2 + 2;
	ata_ctlp->ac_drvaddr[1]   = ioaddr2 + AT_DRVADDR;

	/*
	 * get max transfer size
	 */
	len = sizeof (int);
	if (ATA_INTPROP(dip, "max_transfer", &val, &len) != DDI_PROP_SUCCESS) {
		ata_ctlp->ac_max_transfer = 0x100;
	} else {
		ata_ctlp->ac_max_transfer = (int)val;
		if (ata_ctlp->ac_max_transfer < 1) {
			ata_ctlp->ac_max_transfer = 1;
		}
		if (ata_ctlp->ac_max_transfer > 0x100) {
			ata_ctlp->ac_max_transfer = 0x100;
		}
	}

	/* reset */
	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {
		ddi_put16(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW, (ddi_get16
			(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW) | RESETCTL));
	}
	ddi_put8(ata_ctlp->ata_datap1[0],
			ata_ctlp->ac_devctl[0],
			ATDC_D3 | ATDC_NIEN | ATDC_SRST);
	drv_usecwait(30000);
	ddi_put8(ata_ctlp->ata_datap1[0],
			ata_ctlp->ac_devctl[0], ATDC_D3);
	drv_usecwait(30000);

	/*
	 * Enable the secondary channel and issue soft reset
	 * Enabling is required as a workaround to a CMD chip problem
	 */
	val = ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)ata_ctlp->ata_conf_addr + 0x51);
	ddi_put8(ata_ctlp->ata_conf_handle,
		(uchar_t *)ata_ctlp->ata_conf_addr + 0x51, val | 0x8);

	ddi_put8(ata_ctlp->ata_datap1[1], ata_ctlp->ac_devctl[1],
			ATDC_D3 | ATDC_NIEN | ATDC_SRST);
	drv_usecwait(30000);
	ddi_put8(ata_ctlp->ata_datap1[1], ata_ctlp->ac_devctl[1], ATDC_D3);

	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {
		ddi_put16(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW, (ddi_get16
			(ata_ctlp->ata_conf_handle, (ushort_t *)
			ata_ctlp->ata_conf_addr + CTRLW) & ~RESETCTL));
	}
	drv_usecwait(90000);

	if (ddi_get_iblock_cookie(dip, 0, &ata_ctlp->ac_iblock) !=
			DDI_SUCCESS) {
		cmn_err(CE_WARN, "ddi_get_iblock_cookie failed");
	}


	mutex_init(&ata_ctlp->ac_hba_mutex, NULL, MUTEX_DRIVER,
	    ata_ctlp->ac_iblock);

	for (chno = 0; chno < ATA_CHANNELS; chno ++) {
		/*
		 * initialize ghd
		 */
		GHD_WAITQ_INIT(&ata_ctlp->ac_ccc[chno].ccc_waitq, NULL, 1);

		/*
		 * initialize ghd
		 */
		if (!ghd_register("ata",
			&(ata_ctlp->ac_ccc[chno]),
			dip,
			0,
			ata_ctlp,
			NULL,
			NULL,
			NULL,
			ata_hba_start,
			ata_hba_complete,
			ata_intr,
			ata_get_status,
			ata_process_intr,
			ata_timeout_func,
			&ata_timer_conf,
			ata_ctlp->ac_iblock,
			chno)) {
			return (NULL);
		}
	}

	ata_ctlp->ac_flags |= AC_GHD_INIT;

	return (ata_ctlp);
}

/*
 * destroy a controller
 */
static void
ata_destroy_controller(dev_info_t *dip)
{
	int instance;
	struct ata_controller *ata_ctlp;

	ADBG_TRACE(("ata_destroy_controller entered\n"));

	instance = ddi_get_instance(dip);
	ata_ctlp = ddi_get_soft_state(ata_state, instance);

	if (ata_ctlp == NULL) {
		return;
	}

	/*
	 * destroy ghd
	 */
	if (ata_ctlp->ac_flags & AC_GHD_INIT) {
		ghd_unregister(&ata_ctlp->ac_ccc[0]);
		ghd_unregister(&ata_ctlp->ac_ccc[1]);
	}

	mutex_destroy(&ata_ctlp->ac_hba_mutex);

	/*
	 * destroy controller struct
	 */
	ddi_soft_state_free(ata_state, instance);
}

/*
 * initialize a drive
 */
static struct ata_drive *
ata_init_drive(struct ata_controller *ata_ctlp, u_char targ, u_char lun)
{
	struct ata_drive *ata_drvp;
	struct dcd_identify *ata_idp;
	int drive_type, chno;

	ADBG_TRACE(("ata_init_drive entered, targ = %d, lun = %d\n",
		targ, lun));

	/*
	 * check if device already exists
	 */
	ata_drvp = CTL2DRV(ata_ctlp, targ, lun);

	if (ata_drvp != NULL) {
		return (ata_drvp);
	}

	/*
	 * allocate new device structure
	 */
	ata_drvp = (struct ata_drive *)
		kmem_zalloc((unsigned)sizeof (struct ata_drive), KM_NOSLEEP);

	if (!ata_drvp) {
		return (NULL);
	}

	/*
	 * set up drive struct
	 */
	ata_drvp->ad_ctlp = ata_ctlp;
	ata_drvp->ad_targ = targ;
	ata_drvp->ad_lun = lun;

	if (targ & 1) {
		ata_drvp->ad_drive_bits = ATDH_DRIVE1;
	} else {
		ata_drvp->ad_drive_bits = ATDH_DRIVE0;
	}

	if (targ < 2) {
		chno = ata_drvp->ad_channel = 0;
	} else {
		chno = ata_drvp->ad_channel = 1;
	}

	/*
	 * Program drive/hd and feature register
	 */
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);
	ddi_put8(ata_ctlp->ata_datap[chno], ata_ctlp->ac_feature[chno], 0);

	/*
	 * Disable interrupts for device
	ddi_put8(ata_ctlp->ata_datap1[chno],
		ata_ctlp->ac_devctl[chno], ATDC_D3 | ATDC_NIEN);
			ADDed NIEN
	*/

	/*
	 * get drive type, side effect is to collect
	 * IDENTIFY DRIVE data
	 */
	ata_idp = &ata_drvp->ad_id;

	drive_type = ata_drive_type(ata_ctlp, (ushort *)ata_idp, chno);

	switch (drive_type) {

		case ATA_DEV_NONE:
			/*
			 * Soft Reset the chip
			 */
			ddi_put8(ata_ctlp->ata_datap1[chno],
					ata_ctlp->ac_devctl[chno],
					ATDC_D3|ATDC_NIEN|ATDC_SRST);
			goto errout;
		case ATA_DEV_ATAPI:
			ata_drvp->ad_flags |= AD_ATAPI;
			break;
		case ATA_DEV_DISK:
			ata_drvp->ad_flags |= AD_DISK;
			break;
	}

	if (ATAPIDRV(ata_drvp)) {
		if (atapi_init_drive(ata_drvp) != SUCCESS)
			goto errout;
	} else {
		if (ata_disk_init_drive(ata_drvp) != SUCCESS)
			goto errout;
	}

	/*
	 * store pointer in controller struct
	 */
	ata_ctlp->ac_drvp[targ][lun] = ata_drvp;
	return (ata_drvp);

errout:
	ata_destroy_drive(ata_drvp);
	return (NULL);
}

/*
 * destroy a drive
 */
static void
ata_destroy_drive(struct ata_drive *ata_drvp)
{
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	int	chno = ata_drvp->ad_channel;
	ADBG_TRACE(("ata_destroy_drive entered\n"));

	/*
	 * Disable interrupts for drive
	 */
	ddi_put8(ata_ctlp->ata_datap[chno], ata_ctlp->ac_drvhd[chno],
		ata_drvp->ad_drive_bits);
	ddi_put8(ata_ctlp->ata_datap1[chno], ata_ctlp->ac_devctl[chno], ATDC_D3|
		ATDC_NIEN);

	/*
	 * interface specific clean-ups
	 */
	if (ata_drvp->ad_flags & AD_ATAPI) {
		atapi_destroy_drive(ata_drvp);
	} else if (ata_drvp->ad_flags & AD_DISK) {
		ata_disk_destroy_drive(ata_drvp);
	}

	/*
	 * free drive struct
	 */
	kmem_free(ata_drvp, (unsigned)sizeof (struct ata_drive));
}

/*
 * ata_drive_type()
 *
 * The timeout values and exact sequence of checking is critical
 * especially for atapi device detection, and should not be changed lightly.
 */
static int
ata_drive_type(struct ata_controller *ata_ctlp, ushort *secbuf, int chno)
{
	ADBG_TRACE(("ata_drive_type entered\n"));

	if (ata_wait(ata_ctlp->ata_datap[chno], ata_ctlp->ac_status[chno],
		(ATS_DRDY | ATS_DSC), (ATS_BSY | ATS_ERR), 30, 100000)
			== FAILURE) {

		/*
		 * No disk, check for atapi unit.
		 */
		if ((atapi_signature(ata_ctlp->ata_datap[chno],
			ata_ctlp->ioaddr1[chno]) == SUCCESS) && ((ddi_get8
			(ata_ctlp->ata_datap[chno], ata_ctlp->ac_status[chno])
			& ~ATS_DSC) == 0)) {
			if (atapi_id(ata_ctlp->ata_datap[chno],
				ata_ctlp->ioaddr1[chno], secbuf) == SUCCESS) {
				return (ATA_DEV_ATAPI);
			}
		}
	}

	if (ata_disk_id(ata_ctlp->ata_datap[chno], ata_ctlp->ioaddr1[chno],
		secbuf) == FAILURE) {
		/*
		 * No disk, check for atapi unit.
		 */
		if (atapi_signature(ata_ctlp->ata_datap[chno],
			ata_ctlp->ioaddr1[chno]) == SUCCESS) {
			if (atapi_id(ata_ctlp->ata_datap[chno],
				ata_ctlp->ioaddr1[chno], secbuf) == SUCCESS) {
				return (ATA_DEV_ATAPI);
			}
		}

		return (ATA_DEV_NONE);
	}

	return (ATA_DEV_DISK);
}

/*
 * Wait for a register of a controller to achieve a specific state.
 * To return normally, all the bits in the first sub-mask must be ON,
 * all the bits in the second sub-mask must be OFF.
 * If (usec_delay * iterations) passes without the controller achieving
 * the desired bit configuration, we return SUCCESS, else FAILURE.
 */
int
ata_wait(ddi_acc_handle_t handle, uint8_t  *port, ushort onbits,
	ushort offbits, int usec_delay, int iterations)
{
	int i;
	ushort val;

	for (i = iterations; i; i--) {
		val = ddi_get8(handle, port);
		if (((val & onbits) == onbits) &&
		    ((val & offbits) == 0)) {
			return (SUCCESS);
		}
		drv_usecwait(usec_delay);
	}

	return (FAILURE);
}

/*
 * Use SET FEATURES command
 */
int
ata_set_feature(struct ata_drive *ata_drvp, u_char feature, u_char value)
{
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	int chno = ata_drvp->ad_channel, retval = SUCCESS;
	unsigned char val, secbuf[512];

	ADBG_TRACE(("ata_set_feature entered\n"));

	val = ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1);
	if (chno == 0) {
		/*
		 * Clear the status bit so that we can clearly verify
		 * whether a interrupt happened
		 */
		(void) ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50), 0x4);
		/* Preserve intr status of channel 1 */
		val = (val & 0x23) | 0x10;
	} else {
		/*
		 * clear the status bit so that we can clearly identify
		 * the interrupt happening
		 */
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr +0x57),
			ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57)) | 0x10);
		/* Preserve intr status of channel 0 */
		val = (val & 0x13) | 0x20;
	}
	ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1, val);

	/*
	 * set up drive/head register
	 */
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);

	if (ata_drvp->ad_flags & AD_ATAPI) {
		(void) atapi_id(ata_ctlp->ata_datap[chno],
			ata_ctlp->ioaddr1[chno], (ushort *) secbuf);
	}


	/* Wait for the BSY to go low */
	if (ata_wait(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_status[chno], 0,
		ATS_BSY, 3000000, 10)) {
		cmn_err(CE_WARN, "Drive not ready before set_features");
		retval = FAILURE;
		goto end_set_feature;
	}


	/*
	 * set up feature and value
	 */
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_feature[chno], feature);
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_count[chno], value);

	/*
	 * issue SET FEATURE command
	 */
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_cmd[chno], ATC_SET_FEAT);

	/*
	 * Wait for the interrupt pending bit to be set
	 * and then clear the status and then the interrupt.
	 */
	if (chno == 0) {
		if (ata_wait(ata_ctlp->ata_conf_handle,
			(uint8_t *)(ata_ctlp->ata_conf_addr + 0x50), 4,
			0, 400000, 50)) {
			cmn_err(CE_WARN,
				"Interrupt not seen after set_features");
			retval = FAILURE;
			goto end_set_feature;
		}
	} else {
		if (ata_wait(ata_ctlp->ata_conf_handle,
			(uint8_t *)(ata_ctlp->ata_conf_addr + 0x57), 0x10,
			0, 400000, 50)) {
			cmn_err(CE_WARN,
				"Interrupt not seen after set_features");
			retval = FAILURE;
			goto end_set_feature;
		}
	}

	/*
	 * wait for not-busy status
	 */
	if (ata_wait(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_status[chno], 0,
		ATS_BSY, 30, 400000)) {
		cmn_err(CE_WARN, "Drive not ready after set_features");
		retval = FAILURE;
		goto end_set_feature;
	}

	mutex_enter(&ata_ctlp->ac_hba_mutex);
	ata_ctlp->ac_polled_count++;
	mutex_exit(&ata_ctlp->ac_hba_mutex);

	/*
	 * check for error
	 */
	if (ddi_get8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_status[chno]) & ATS_ERR) {

		ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);
		retval = FAILURE;
		goto end_set_feature;
	}
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);

	/*
	 * Clear any pending intterupt
	 */
	if (chno == 0) {
		if (ata_wait(ata_ctlp->ata_conf_handle,
			(uint8_t *)(ata_ctlp->ata_conf_addr + 0x50), 4,
			0, 400000, 50)) {
			cmn_err(CE_WARN,
				"Interrupt not seen after set_features");
			retval = FAILURE;
			goto end_set_feature;
		}
		(void) ddi_get8(ata_ctlp->ata_conf_handle,
		(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50));
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x50), 0x4);
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));
	} else {
		if (ata_wait(ata_ctlp->ata_conf_handle,
			(uint8_t *)(ata_ctlp->ata_conf_addr + 0x57), 0x10,
			0, 400000, 50)) {
			cmn_err(CE_WARN,
				"Interrupt not seen after set_features");
			retval = FAILURE;
			goto end_set_feature;
		}
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr +0x57),
			ddi_get8(ata_ctlp->ata_conf_handle,
			(uchar_t *)(ata_ctlp->ata_conf_addr + 0x57)) | 0x10);
		ddi_put8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2),
			(ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno +2))  | 6));
	}


end_set_feature:
	/*
	 * Do not enable the interrupts if attach in progress
	 * This could generate "level 4 Interrupts not serviced
	 * Message because of the anamolus behaviour of NIEN bit.
	 * The enabling of the interrupt will happen after the
	 * attach is complete.
	 */
	if ((ata_ctlp->ac_flags & AC_ATTACH_IN_PROGRESS) == 0) {
		val = ddi_get8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_cs_addr + 0x1);
		if (chno == 0) {
			/* Preserve intr status of channel 1 */
			val = val & 0x23;
		} else {
			/* Preserve intr status of channel 0 */
			val = val & 0x13;
		}
		ddi_put8(ata_ctlp->ata_conf_handle,
			(uchar_t *)ata_ctlp->ata_cs_addr + 0x1, val);
	}

	return (retval);
}

#if defined(_eisa_bus_supported)

#define	DPT_MASK		0x70ffff
#define	DPT_ID			0x00201412
#define	DPT_2012A		0x12142402
#define	DPT_2012B		0x1214a401
#define	DPT_2012B2		0x1214a402
#define	DPT_2012B2_9x		0x1214a502

/*
 * XXX - This needs Tony's fix to not crash if there's a DPT
 *  card with more than 15 EISA functions
 */
static int
ata_detect_dpt(dev_info_t *dip, uint8_t *ioaddr)
{
	char	bus_type[16];
	int	len;
	caddr_t eisa_data;
	unsigned char numfuncs;
	short func;
	short slot;
	size_t size;
	uint bytes;
	uint boardid;
	NVM_SLOTINFO *slotinfo;
	NVM_FUNCINFO *funcinfo;
	NVM_PORT *ports;

	ADBG_TRACE(("ata_detect_dpt entered\n"));

	/*
	 * get bus type
	 */
	len = sizeof (bus_type);
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_BUF, 0,
		"device_type", (caddr_t)bus_type, &len) != DDI_PROP_SUCCESS) {
		return (FAILURE);
	}

	/*
	 * check for EISA
	 */
	if (strncmp(bus_type, DEVI_EISA_NEXNAME, len) != 0) {
		return (FAILURE);
	}

	/* check for broken DPT cards that conflict with ata IO addresses */

	/*
	 * allocate space to hold eisa config data.  eisa_nvm does not allow
	 * you to figure out how many function blocks there are for a given
	 * slot (eisa_nvm should really be fixed), so we just pick a "magic"
	 * constant of 15 functions which seems big enough
	 */
	size = sizeof (short) + sizeof (NVM_SLOTINFO) +
		sizeof (NVM_FUNCINFO) * 15;
	eisa_data = (caddr_t)kmem_zalloc(size, KM_NOSLEEP);
	if (eisa_data == NULL) {
		return (FAILURE);
	}

	/*
	 * walk through all the
	 * eisa slots looking for DPT cards
	 */
	for (slot = 0; slot < 16; slot++) {
		bytes = eisa_nvm(eisa_data,
			EISA_SLOT | EISA_BOARD_ID, slot, DPT_ID, DPT_MASK);
		if (bytes == 0) {
			continue;
		}

		/*
		 * check if found card is one of the broken DPT cards.  if not,
		 * move on to next slot
		 */
		slotinfo = (NVM_SLOTINFO *)(eisa_data + sizeof (short));
		boardid = slotinfo->boardid[0] << 24;
		boardid |= slotinfo->boardid[1] << 16;
		boardid |= slotinfo->boardid[2] << 8;
		boardid |= slotinfo->boardid[3];
		if (boardid != DPT_2012A && boardid != DPT_2012B &&
			boardid != DPT_2012B2 && boardid != DPT_2012B2_9x) {
				continue;
		}

		ADBG_INIT(("DPT %s card found: board id=0x%x\n",
			(boardid == DPT_2012A) ? "2012A" :
			(boardid == DPT_2012B) ? "2012B" :
			(boardid == DPT_2012B2) ? "2012B2" :
			"2012B2/9x", boardid));

		/*
		 * check all the functions for the card looking for port
		 * descriptions
		 */
		numfuncs = slotinfo->functions;
		funcinfo = (NVM_FUNCINFO *)(slotinfo + 1);
		for (func = 0; func < numfuncs; func++, funcinfo++) {
			if (funcinfo->fib.port == 0) {
				continue;
			}
			ports = funcinfo->un.r.port;
			if (ata_check_io_addr(ports, ioaddr) == SUCCESS) {
				ADBG_INIT(("DPT card is using IO address "
						"%x\n", ioaddr));
				kmem_free(eisa_data, size);
				return (SUCCESS);
			}
		}
	}

	kmem_free(eisa_data, size);
	return (FAILURE);
}

static int
ata_check_io_addr(NVM_PORT *ports, uint8_t *ioaddr)
{
	int indx;

	ADBG_TRACE(("ata_check_io_addr entered\n"));

	/*
	 * compare ioaddr to port entries.  assume that, if ioaddr is
	 * present, that it will always be at the bottom of a range of
	 * ports (i.e. stored in ports[indx].address)
	 */
	for (indx = 0; indx < NVM_MAX_PORT; indx++) {
		if (ports[indx].address == ioaddr) {
			return (SUCCESS);
		}
		if (ports[indx].more != 1) {
			break;
		}
	}
	return (FAILURE);
}
#endif

void
ata_write_config(struct ata_drive *ata_drvp)
{
	u_char	rd_par, wr_par, val, clocks;
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	int	targ = ata_drvp->ad_targ;
	int chno = ata_drvp->ad_channel;

	if (ata_drvp->ad_piomode != 0x7f) {
		/* PIOMODE */
		rd_par = ata_ctlp->ac_piortable[ata_drvp->ad_piomode];
		wr_par = ata_ctlp->ac_piowtable[ata_drvp->ad_piomode];
	} else {
		/* DMAMODE */
		rd_par = ata_ctlp->ac_dmartable[ata_drvp->ad_dmamode & 0x03];
		wr_par = ata_ctlp->ac_dmawtable[ata_drvp->ad_dmamode & 0x03];
	}

	if ((ata_ctlp->ac_vendor_id == NSVID) &&
		(ata_ctlp->ac_device_id == NSDID)) {
		ddi_put8(ata_ctlp->ata_conf_handle, (uchar_t *)
			ata_ctlp->ata_conf_addr + CH1D1DR + targ*4, rd_par);
		ddi_put8(ata_ctlp->ata_conf_handle, (uchar_t *)
			ata_ctlp->ata_conf_addr + CH1D1DW +targ*4, wr_par);
	} else if ((ata_ctlp->ac_vendor_id == CMDVID) &&
		(ata_ctlp->ac_device_id == CMDDID)) {
		if (ata_drvp->ad_run_ultra) {
			switch (ata_drvp->ad_dmamode & 0x03) {
				default :
					/* FALLTHROUGH */
				case 0 :
					clocks = 3;
					break;
				case 1 :
					clocks = 2;
					break;
				case 2 :
					clocks = 1;
					break;
			}

			val = ddi_get8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr +
					0x73 + (chno * 8));
			if (targ & 1) {
				/* Slave's */
				val &= 0x31;
				val |= (clocks << 6) | 2;
			} else {
				/* Master's */
				val &= 0xC2;
				val |= (clocks << 4) | 1;
			}
			ddi_put8(ata_ctlp->ata_conf_handle,
				(uchar_t *)ata_ctlp->ata_conf_addr +
					0x73 + (chno * 8), val);

			return;
		}
		targ *= 2;
		if (targ == 6) {
			targ = 7;
		}
		ddi_put8(ata_ctlp->ata_conf_handle,
		(uchar_t *)ata_ctlp->ata_conf_addr + 0x54 + targ, rd_par);
	}
}

void
ata_ghd_complete_wraper(struct ata_controller *ata_ctlp,
			struct ata_pkt *ata_pktp, int chno)
{
	if (ata_ctlp->ac_simplex == 1) {
		int rc, nchno = (chno) ? 0 : 1;

		mutex_enter(&ata_ctlp->ac_hba_mutex);
		ata_ctlp->ac_pending[chno] = 0;
		if (ata_ctlp->ac_pending[nchno]) {
			ata_ctlp->ac_actv_chnl = nchno;
		} else {
			ata_ctlp->ac_actv_chnl = 2;
		}
		if (ata_ctlp->ac_actv_chnl != 2) {
			struct ata_pkt *lata_pktp;

			mutex_exit(&ata_ctlp->ac_hba_mutex);
			lata_pktp = ata_ctlp->ac_active[nchno];

			rc = lata_pktp->ap_start(ata_ctlp, lata_pktp);

			if (rc != TRAN_ACCEPT) {

				mutex_enter(&ata_ctlp->ac_hba_mutex);
				ata_ctlp->ac_actv_chnl = 2;
				ata_ctlp->ac_pending[nchno] = 0;
				mutex_exit(&ata_ctlp->ac_hba_mutex);

				ata_ctlp->ac_active[nchno] = NULL;

				ghd_async_complete(&ata_ctlp->ac_ccc[nchno],
						APKT2GCMD(lata_pktp));
			}
		} else {
			mutex_exit(&ata_ctlp->ac_hba_mutex);
		}
	}
	ghd_complete(&ata_ctlp->ac_ccc[chno], APKT2GCMD(ata_pktp));
}

void
make_prd(gcmd_t *gcmdp, ddi_dma_cookie_t *cookie, int single_seg, int num_segs)
{
	struct ata_pkt *ata_pktp = gcmdp->cmd_private;
#ifdef __lint
	int seg = num_segs;

	single_seg = seg;
	seg = single_seg;
#endif
	ata_pktp->ap_addr = cookie->dmac_address;
	ata_pktp->ap_cnt = cookie->dmac_size;
}

void
write_prd(struct ata_controller *ata_ctlp, struct ata_pkt *ata_pktp)
{
	int chno = ata_pktp->ap_chno;
	ddi_acc_handle_t ata_prd_acc_handle;
	ddi_dma_handle_t ata_prd_dma_handle;
	caddr_t  memp;
	int i = 0, andfactor;
	uint32_t addr = ata_pktp->ap_addr;
	size_t  cnt = ata_pktp->ap_cnt;
	struct ata_drive *ata_drvp;

	ata_prd_acc_handle = (void *)ata_ctlp->ata_prd_acc_handle[chno];
	ata_prd_dma_handle = (void *)ata_ctlp->ata_prd_dma_handle[chno];
	memp = (void *)ata_ctlp->ac_memp[chno];
	ata_drvp = CTL2DRV(ata_ctlp, ata_pktp->ap_targ, 0);
	if (ata_drvp->ad_flags & AD_ATAPI) {
		andfactor = 0x00ffffff;
	} else {
		andfactor = 0x0003ffff;
	}

	while (cnt) {
		if (((addr & 0x0000ffff) + (cnt &
			andfactor)) <= 0x00010000) {
			ddi_put32(ata_prd_acc_handle, (uint32_t *)memp + i++,
				addr);
			ddi_put32(ata_prd_acc_handle, (uint32_t *)memp + i++,
				(cnt & 0x0000ffff) | 0x80000000);
			break;
		} else {
			ddi_put32(ata_prd_acc_handle, (uint32_t *)memp +i++,
				addr);
			ddi_put32(ata_prd_acc_handle, (uint32_t *)memp + i++,
				(0x10000 - (addr & 0x0000ffff)));
			cnt -= (0x10000 - (addr & 0x0000ffff));
			addr += (0x10000 - (addr & 0x0000ffff));
		}
	}
	ddi_put32(ata_ctlp->ata_cs_handle,
		(uint32_t *)(ata_ctlp->ata_cs_addr + 4 + 8*chno),
		(uint32_t)ata_ctlp->ac_saved_dmac_address[chno]);
	(void) ddi_dma_sync(ata_prd_dma_handle, 0, 2048, DDI_DMA_SYNC_FORDEV);
}


/*
 * prd_init()
 */
int
prd_init(struct ata_controller *ata_ctlp, int chno)
{
	size_t	alloc_len;
	u_int	ncookie;
	ddi_dma_cookie_t cookie;
	ddi_dma_attr_t  prd_dma_attrs;
	ddi_acc_handle_t ata_prd_acc_handle;
	ddi_dma_handle_t ata_prd_dma_handle;
	caddr_t  memp;

	prd_dma_attrs = ata_dma_attrs;
	prd_dma_attrs.dma_attr_sgllen = 1;
	prd_dma_attrs.dma_attr_granular = 32;
	prd_dma_attrs.dma_attr_addr_lo = 0x0ull;
	prd_dma_attrs.dma_attr_addr_hi = 0xffffffffull; /* 32-bit max range */

	if (ddi_dma_alloc_handle(ata_ctlp->ac_dip, &prd_dma_attrs,
		DDI_DMA_DONTWAIT, NULL, &ata_prd_dma_handle) != DDI_SUCCESS) {
		return (FAILURE);
	}

	/*
	 * The area being allocated for the prd tables is 2048 bytes,
	 * Each entry takes 8 bytes, so the table would be able to hold
	 * 256 entries of 64K each. Thus allowing a DMA size upto 16MB
	 */
	if (ddi_dma_mem_alloc(ata_prd_dma_handle, 2048,
		&dev_attr, DDI_DMA_STREAMING, DDI_DMA_DONTWAIT,
		NULL, &memp, &alloc_len, &ata_prd_acc_handle) != DDI_SUCCESS) {
		return (FAILURE);
	}

	if (ddi_dma_addr_bind_handle(ata_prd_dma_handle, NULL, memp,
		alloc_len, DDI_DMA_READ | DDI_DMA_STREAMING, DDI_DMA_DONTWAIT,
		NULL, &cookie, &ncookie) != DDI_DMA_MAPPED) {
		return (FAILURE);
	}

	ata_ctlp->ata_prd_acc_handle[chno] = (void *)ata_prd_acc_handle;
	ata_ctlp->ata_prd_dma_handle[chno] = (void *)ata_prd_dma_handle;
	ata_ctlp->ac_memp[chno] = memp;
	ddi_put32(ata_ctlp->ata_cs_handle,
		(uint32_t *)(ata_ctlp->ata_cs_addr + 4 + 8*chno),
		cookie.dmac_address);
	/*
	 * This need to be saved as the address written to the prd pointer
	 * register will go away after a cpr cycle and power might have been
	 * removed and hencewe need to use the saved address to program
	 * that register
	 */
	ata_ctlp->ac_saved_dmac_address[chno] = cookie.dmac_address;
	return (SUCCESS);
}

void
change_endian(unsigned char *string, int length)
{
	unsigned char x;
	int i;
	for (i = 0; i < length; i++) {
		x = string[i];
		string[i] = string[i+1];
		string[i+1] = x;
		i++;
	}
}
