/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ata_disk.c	1.39	99/06/21 SMI"

#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/cdio.h>

#include "ata_common.h"
#include "ata_disk.h"

/*
 * this typedef really should be in dktp/cmpkt.h
 */
typedef struct cmpkt cmpkt_t;


/*
 * DADA entry points
 */

static	int	ata_disk_abort(gtgt_t *gtgtp, cmpkt_t *pktp);
static	int	ata_disk_reset(gtgt_t *gtgtp, int level);
static	int	ata_disk_ioctl(gtgt_t *gtgtp, int cmd, int a, int flag);
static	cmpkt_t	*ata_disk_pktalloc(gtgt_t *gtgtp,
			int (*callback)(), caddr_t arg);
static	void	ata_disk_pktfree(gtgt_t *gtgtp, cmpkt_t *pktp);
static	cmpkt_t	*ata_disk_memsetup(gtgt_t *gtgtp,
			cmpkt_t *pktp, struct buf *bp,
			int (*callback)(), caddr_t arg);
static	void	ata_disk_memfree(gtgt_t *gtgtp, cmpkt_t *pktp);
static	cmpkt_t	*ata_disk_iosetup(gtgt_t *gtgtp, cmpkt_t *pktp);
static	int	ata_disk_transport(gtgt_t *gtgtp, cmpkt_t *pktp);

/*
 * DADA packet callbacks
 */

static	void	ata_disk_complete(ata_pkt_t *ata_pktp, int do_callback);

static	int	ata_disk_intr(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_intr_dma(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_intr_pio_in(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_intr_pio_out(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_start(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_start_dma_in(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_start_dma_out(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_start_pio_in(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);
static	int	ata_disk_start_pio_out(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp, ata_pkt_t *ata_pktp);

/*
 * Local Function prototypes
 */

static	int	ata_disk_eject(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	void	ata_disk_fake_inquiry(ata_drv_t *ata_drvp);
static	void	ata_disk_get_resid(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_initialize_device_parameters(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp);
static	int	ata_disk_lock(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_set_multiple(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp);
static	void	ata_disk_pio_xfer_data_in(ata_ctl_t *ata_ctlp,
			ata_pkt_t *ata_pktp);
static	void	ata_disk_pio_xfer_data_out(ata_ctl_t *ata_ctlp,
			ata_pkt_t *ata_pktp);
static	void	ata_disk_set_standby_timer(ata_ctl_t *ata_ctlp,
			ata_drv_t *ata_drvp);
static	int	ata_disk_recalibrate(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_standby(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_start_common(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_state(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static	int	ata_disk_unlock(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp,
			ata_pkt_t *ata_pktp);
static uint32_t ata_calculate_capacity(ata_drv_t *ata_drvp);


/*
 * Local static data
 */

uint_t	ata_disk_init_dev_parm_wait = 4 * 1000000;
uint_t	ata_disk_set_mult_wait = 4 * 1000000;
int	ata_disk_do_standby_timer = TRUE;


static struct ctl_objops ata_disk_objops = {
	ata_disk_pktalloc,
	ata_disk_pktfree,
	ata_disk_memsetup,
	ata_disk_memfree,
	ata_disk_iosetup,
	ata_disk_transport,
	ata_disk_reset,
	ata_disk_abort,
	nulldev,
	nulldev,
	ata_disk_ioctl,
	0, 0
};



/*
 *
 * initialize the ata_disk sub-system
 *
 */

/*ARGSUSED*/
int
ata_disk_attach(
	ata_ctl_t *ata_ctlp)
{
	ADBG_TRACE(("ata_disk_init entered\n"));
	return (TRUE);
}



/*
 *
 * destroy the ata_disk sub-system
 *
 */

/*ARGSUSED*/
void
ata_disk_detach(
	ata_ctl_t *ata_ctlp)
{
	ADBG_TRACE(("ata_disk_destroy entered\n"));
}



/*
 *
 * initialize the soft-structure for an ATA (non-PACKET) drive and
 * then configure the drive with the correct modes and options.
 *
 */

int
ata_disk_init_drive(
	ata_drv_t *ata_drvp)
{
	ata_ctl_t *ata_ctlp = ata_drvp->ad_ctlp;
	struct ctl_obj *ctlobjp;
	struct scsi_device *devp;
	int 	len;
	int	val;
	short	*chs;
	char 	buf[80];

	ADBG_TRACE(("ata_disk_init_drive entered\n"));

	/* ATA disks don't support LUNs */

	if (ata_drvp->ad_lun != 0)
		return (FALSE);

	/* set up drive structure */

	ata_drvp->ad_phhd = ata_drvp->ad_id.ai_heads;
	ata_drvp->ad_phsec = ata_drvp->ad_id.ai_sectors;
	ata_drvp->ad_bioshd   = ata_drvp->ad_id.ai_heads;
	ata_drvp->ad_biossec  = ata_drvp->ad_id.ai_sectors;
	ata_drvp->ad_bioscyl  = ata_drvp->ad_id.ai_fixcyls;
	ata_drvp->ad_acyl = 0;


#ifdef __old_version__
	/*
	 * determine if the drive supports LBA mode
	 */
	if (ata_drvp->ad_id.ai_cap & ATAC_LBA_SUPPORT)
		ata_drvp->ad_drive_bits |= ATDH_LBA;
#else
	/*
	 * Determine if the drive supports LBA mode
	 * LBA mode is mandatory on ATA-3 (or newer) drives but is
	 * optional on ATA-2 (or older) drives. On ATA-2 drives
	 * the ai_majorversion word should be 0xffff or 0x0000.
	 */
	if ((ata_drvp->ad_id.ai_majorversion & 0x8000) == 0 &&
			ata_drvp->ad_id.ai_majorversion >= (1 << 2)) {
		/* ATA-3 or better */
		ata_drvp->ad_drive_bits |= ATDH_LBA;
	} else if (ata_drvp->ad_id.ai_cap & ATAC_LBA_SUPPORT) {
		/* ATA-2 LBA capability bit set */
		ata_drvp->ad_drive_bits |= ATDH_LBA;
#ifdef ___not___used___
	} else {
		/* must use CHS mode */
#endif
	}
#endif


	/* straighten out the geometry */

	(void) sprintf(buf, "SUNW-ata-%p-d%d-chs", (void *) ata_ctlp->ac_data,
		ata_drvp->ad_targ+1);
	if (ddi_getlongprop(DDI_DEV_T_ANY, ddi_root_node(), 0,
			buf, (caddr_t)&chs, &len) == DDI_PROP_SUCCESS) {
		/*
		 * if the number of sectors and heads in bios matches the
		 * physical geometry, then so should the number of cylinders
		 * this is to prevent the 1023 limit in the older bios's
		 * causing loss of space.
		 */
		if (chs[1] == (ata_drvp->ad_bioshd - 1) &&
				chs[2] == ata_drvp->ad_biossec)
			/* Set chs[0] to zero-based number of cylinders. */
			chs[0] = ata_drvp->ad_id.ai_fixcyls - 1;
		else if (!(ata_drvp->ad_drive_bits & ATDH_LBA)) {
			/*
			 * if the the sector/heads do not match that of the
			 * bios and the drive does not support LBA. We go ahead
			 * and advertise the bios geometry but use the physical
			 * geometry for sector translation.
			 */
			cmn_err(CE_WARN, "!Disk 0x%p,%d: BIOS geometry "
				"different from physical, and no LBA support.",
				(void *)ata_ctlp->ac_data, ata_drvp->ad_targ);
		}

		/*
		 * chs[0,1] are zero-based; make them one-based.
		 */
		ata_drvp->ad_bioscyl = chs[0] + 1;
		ata_drvp->ad_bioshd = chs[1] + 1;
		ata_drvp->ad_biossec = chs[2];
	} else {
		uint32_t capacity;
		uint32_t cylsize;

		/*
		 * Property not present; this means that boot.bin has
		 * determined that the drive supports Int13 LBA.  Note
		 * this, but just return a geometry with a large
		 * cylinder count; this will be the signal for dadk to
		 * fail DKIOCG_VIRTGEOM.
		 *
		 * ad_bios* are already set; just recalculate ad_bioscyl
		 * from capacity.
		 */

		ata_drvp->ad_flags |= AD_INT13LBA;
		cylsize = ata_drvp->ad_bioshd * ata_drvp->ad_biossec;
		capacity = 0;

		if (cylsize != 0) {
			capacity = ata_calculate_capacity(ata_drvp);
		}
		if (capacity != 0) {
			ata_drvp->ad_bioscyl = capacity / cylsize;
		} else {
			/*
			 * Something's wrong; return something sure to
			 * fail the "cyls < 1024" test.  This will
			 * never make it out of the DKIOCG_VIRTGEOM
			 * call, so its total bogosity won't matter.
			 */
			ata_drvp->ad_bioscyl = 1025;
			ata_drvp->ad_bioshd = 1;
			ata_drvp->ad_biossec = 1;
		}
	}

	/*
	 * set up the scsi_device and ctl_obj structures
	 */
	devp = &ata_drvp->ad_device;
	ctlobjp = &ata_drvp->ad_ctl_obj;

	devp->sd_inq = &ata_drvp->ad_inquiry;
	devp->sd_dev = ata_ctlp->ac_dip;
	devp->sd_address.a_hba_tran = (scsi_hba_tran_t *)ctlobjp;
	devp->sd_address.a_target = (ushort)ata_drvp->ad_targ;
	devp->sd_address.a_lun = (uchar_t)ata_drvp->ad_lun;
	mutex_init(&devp->sd_mutex, NULL, MUTEX_DRIVER, NULL);
	ata_drvp->ad_flags |= AD_MUTEX_INIT;

	/*
	 * DADA ops vectors and cookie
	 */
	ctlobjp->c_ops  = (struct ctl_objops *)&ata_disk_objops;

	/*
	 * this is filled in with gtgtp by ata_disk_bus_ctl(INITCHILD)
	 */
	ctlobjp->c_data = NULL;

	ctlobjp->c_ext  = &(ctlobjp->c_extblk);
	ctlobjp->c_extblk.c_ctldip = ata_ctlp->ac_dip;
	ctlobjp->c_extblk.c_targ   = ata_drvp->ad_targ;
	ctlobjp->c_extblk.c_blksz  = NBPSCTR;

	/*
	 * Get highest block factor supported by the drive.
	 * Some drives report 0 if read/write multiple not supported,
	 * adjust their blocking factor to 1.
	 */
	ata_drvp->ad_block_factor = ata_drvp->ad_id.ai_mult1 & 0xff;

	/*
	 * If a block factor property exists, use the smaller of the
	 * property value and the highest value the drive can support.
	 */
	(void) sprintf(buf, "drive%d_block_factor", ata_drvp->ad_targ);
	val = ddi_prop_get_int(DDI_DEV_T_ANY, ata_ctlp->ac_dip, 0, buf,
		ata_drvp->ad_block_factor);

	ata_drvp->ad_block_factor = (short)min(val, ata_drvp->ad_block_factor);

	if (ata_drvp->ad_block_factor == 0)
		ata_drvp->ad_block_factor = 1;

	/*
	 * now program the drive
	 */
	if (!ata_disk_setup_parms(ata_ctlp, ata_drvp))
		return (FALSE);

	ata_disk_fake_inquiry(ata_drvp);

	return (TRUE);
}

static uint32_t
ata_calculate_capacity(ata_drv_t *ata_drvp)
{
	/*
	 * Asked x3t13 for advice; this implements Hale Landis'
	 * response, minus the "use ATA_INIT_DEVPARMS".
	 * See "capacity.notes".
	 */

	/* some local shorthand/renaming to clarify the meaning */

	ushort_t curcyls_w54, curhds_w55, cursect_w56;
	uint32_t curcap_w57_58;

	if ((ata_drvp->ad_drive_bits & ATDH_LBA) == ATDH_LBA) {
		return (ata_drvp->ad_id.ai_addrsec[0] +
		    ata_drvp->ad_id.ai_addrsec[1] * 0x10000);
	}

	/*
	 * If we're not LBA, then first try to validate "current" values.
	 */

	curcyls_w54 = ata_drvp->ad_id.ai_curcyls;
	curhds_w55 = ata_drvp->ad_id.ai_curheads;
	cursect_w56 = ata_drvp->ad_id.ai_cursectrk;
	curcap_w57_58 = ata_drvp->ad_id.ai_cursccp[0] +
	    ata_drvp->ad_id.ai_cursccp[1] * 0x10000;

	if (((ata_drvp->ad_id.ai_validinfo & 1) == 1) &&
	    (curhds_w55 >= 1) && (curhds_w55 <= 16) &&
	    (cursect_w56 >= 1) && (cursect_w56 <= 63) &&
	    (curcap_w57_58 == curcyls_w54 * curhds_w55 * cursect_w56)) {
		return (curcap_w57_58);
	}

	/*
	 * At this point, Hale recommends ATA_INIT_DEVPARMS.
	 * I don't want to do that, so simply use 1/3/6 as
	 * a final fallback, and continue to assume the BIOS
	 * has done whatever INIT_DEVPARMS are necessary.
	 */

	return (ata_drvp->ad_id.ai_fixcyls * ata_drvp->ad_id.ai_heads *
	    ata_drvp->ad_id.ai_sectors);
}

/*
 *
 * Setup the drives Read/Write Multiple Blocking factor and the
 * current translation geometry. Necessary during attach and after
 * Software Resets.
 *
 */

int
ata_disk_setup_parms(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{

	/*
	 * program geometry info back to the drive
	 */
	if (!ata_disk_initialize_device_parameters(ata_ctlp, ata_drvp)) {
		return (FALSE);
	}

	/*
	 * Determine the blocking factor
	 */
	if (ata_drvp->ad_block_factor > 1) {
		/*
		 * Program the block factor into the drive. If this
		 * fails, then go back to using a block size of 1.
		 */
		if (!ata_disk_set_multiple(ata_ctlp, ata_drvp))
			ata_drvp->ad_block_factor = 1;
	}


	if (ata_drvp->ad_block_factor > 1) {
		ata_drvp->ad_rd_cmd = ATC_RDMULT;
		ata_drvp->ad_wr_cmd = ATC_WRMULT;
	} else {
		ata_drvp->ad_rd_cmd = ATC_RDSEC;
		ata_drvp->ad_wr_cmd = ATC_WRSEC;
	}

	ata_drvp->ad_bytes_per_block = ata_drvp->ad_block_factor << SCTRSHFT;

	ADBG_INIT(("set block factor for drive %d to %d\n",
			ata_drvp->ad_targ, ata_drvp->ad_block_factor));

	if (ata_disk_do_standby_timer)
		ata_disk_set_standby_timer(ata_ctlp, ata_drvp);

	return (TRUE);
}


/*
 * Take the timeout value specified in the "standby" property
 * and convert from seconds to the magic parm expected by the
 * the drive. Then issue the IDLE command to set the drive's
 * internal standby timer.
 */

static void
ata_disk_set_standby_timer(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{
	uchar_t	parm;
	int	timeout = ata_ctlp->ac_standby_time;

	/*
	 * take the timeout value, specificed in seconds, and
	 * encode it into the proper command parm
	 */

	/*
	 * don't change it if no property specified or if
	 * the specified value is out of range
	 */
	if (timeout < 0 || timeout > (12 * 60 * 60))
		return;

	/* 1 to 1200 seconds (20 minutes) == N * 5 seconds */
	if (timeout <= (240 * 5))
		parm = (timeout + 4) / 5;

	/* 20 to 21 minutes == 21 minutes */
	else if (timeout <= (21 * 60))
		parm = 252;

	/* 21 minutes to 21 minutes 15 seconds == 21:15 */
	else if (timeout <= ((21 * 60) + 15))
		parm = 255;

	/* 21:15 to 330 minutes == N * 30 minutes */
	else if (timeout <= (11 * 30 * 60))
		parm = 240 + ((timeout + (30 * 60) - 1)/ (30 * 60));

	/* > 330 minutes == 8 to 12 hours */
	else
		parm = 253;

	(void) ata_command(ata_ctlp, ata_drvp, TRUE, FALSE, 5 * 1000000,
		    ATC_IDLE, 0, parm, 0, 0, 0, 0);
}



/*
 *
 * destroy an ata disk drive
 *
 */

void
ata_disk_uninit_drive(
	ata_drv_t *ata_drvp)
{
	struct scsi_device *devp = &ata_drvp->ad_device;

	ADBG_TRACE(("ata_disk_uninit_drive entered\n"));

	if (ata_drvp->ad_flags & AD_MUTEX_INIT)
		mutex_destroy(&devp->sd_mutex);
}




/*
 *
 * DADA compliant bus_ctl entry point
 *
 */

/*ARGSUSED*/
int
ata_disk_bus_ctl(
	dev_info_t	*d,
	dev_info_t	*r,
	ddi_ctl_enum_t	 o,
	void		*a,
	void		*v)
{
	ADBG_TRACE(("ata_disk_bus_ctl entered\n"));

	switch (o) {

	case DDI_CTLOPS_REPORTDEV:
	{
		int	targ;

		targ = ddi_prop_get_int(DDI_DEV_T_ANY, r, DDI_PROP_DONTPASS,
					"target", 0);
		cmn_err(CE_CONT, "?%s%d at %s%d target %d lun %d\n",
			ddi_get_name(r), ddi_get_instance(r),
			ddi_get_name(d), ddi_get_instance(d), targ, 0);
		return (DDI_SUCCESS);
	}
	case DDI_CTLOPS_INITCHILD:
	{
		dev_info_t	*cdip = (dev_info_t *)a;
		ata_drv_t	*ata_drvp;
		ata_ctl_t	*ata_ctlp;
		ata_tgt_t	*ata_tgtp;
		struct scsi_device *devp;
		struct ctl_obj	*ctlobjp;
		gtgt_t		*gtgtp;
		char		 name[MAXNAMELEN];

		/*
		 * save time by picking up ptr to drive struct left
		 * by ata_bus_ctl - isn't that convenient.
		 */
		ata_drvp = (ata_drv_t *)ddi_get_driver_private(cdip);
		ata_ctlp = ata_drvp->ad_ctlp;

		/* set up pointers to child dip */

		devp = &ata_drvp->ad_device;
		devp->sd_dev = cdip;

		ctlobjp = &ata_drvp->ad_ctl_obj;
		ctlobjp->c_extblk.c_devdip = cdip;

		/*
		 * Create the "ata" property for use by the target driver
		 */
		if (!ata_prop_create(cdip, ata_drvp, "ata")) {
			return (DDI_FAILURE);
		}

		gtgtp = ghd_target_init(d, cdip, &ata_ctlp->ac_ccc,
					sizeof (ata_tgt_t), ata_ctlp,
					ata_drvp->ad_targ,
					ata_drvp->ad_lun);

		/* gt_tgt_private points to ata_tgt_t */
		ata_tgtp = GTGTP2ATATGTP(gtgtp);
		ata_tgtp->at_drvp = ata_drvp;
		ata_tgtp->at_dma_attr = ata_pciide_dma_attr;
		ata_tgtp->at_dma_attr.dma_attr_maxxfer =
				ata_ctlp->ac_max_transfer << SCTRSHFT;

		/* gtgtp is the opaque arg to all my entry points */
		ctlobjp->c_data = gtgtp;

		/* create device name */

		(void) sprintf(name, "%x,%x", ata_drvp->ad_targ,
			ata_drvp->ad_lun);
		ddi_set_name_addr(cdip, name);
		ddi_set_driver_private(cdip, (caddr_t)devp);

		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_UNINITCHILD:
	{
		dev_info_t *cdip = (dev_info_t *)a;
		struct 	scsi_device *devp;
		struct	ctl_obj *ctlobjp;
		gtgt_t	*gtgtp;

		devp = (struct scsi_device *)ddi_get_driver_private(cdip);
		ctlobjp = (struct ctl_obj *)devp->sd_address.a_hba_tran;
		gtgtp = ctlobjp->c_data;

		ghd_target_free(d, cdip, &GTGTP2ATAP(gtgtp)->ac_ccc, gtgtp);

		ddi_set_driver_private(cdip, NULL);
		ddi_set_name_addr(cdip, NULL);
		return (DDI_SUCCESS);
	}

	default:
		return (DDI_FAILURE);
	}
}


/*
 *
 * DADA abort entry point - not currently used by dadk
 *
 */

/* ARGSUSED */
static int
ata_disk_abort(
	gtgt_t *gtgtp,
	cmpkt_t *pktp)
{
	ADBG_TRACE(("ata_disk_abort entered\n"));

	/* XXX - Note that this interface is currently not used by dadk */

	/*
	 *  GHD abort functions take a pointer to a scsi_address
	 *  and so they're unusable here.  The ata driver used to
	 *  return DDI_SUCCESS here without doing anything.  Its
	 *  seems that DDI_FAILURE is more appropriate.
	 */

	return (DDI_FAILURE);
}



/*
 *
 * DADA reset entry point - not currently used by dadk
 * (except in debug versions of driver)
 *
 */

/* ARGSUSED */
static int
ata_disk_reset(
	gtgt_t *gtgtp,
	int level)
{
	ata_drv_t *ata_drvp = GTGTP2ATADRVP(gtgtp);
	int	   rc;

	ADBG_TRACE(("ata_disk_reset entered\n"));

	/* XXX - Note that this interface is currently not used by dadk */

	if (level == RESET_TARGET) {
		rc = ghd_tran_reset_target(&ata_drvp->ad_ctlp->ac_ccc, gtgtp,
			NULL);
	} else if (level == RESET_ALL) {
		rc = ghd_tran_reset_bus(&ata_drvp->ad_ctlp->ac_ccc, gtgtp,
					NULL);
	}

	return (rc ? DDI_SUCCESS : DDI_FAILURE);
}



/*
 *
 * DADA ioctl entry point
 *
 */

/* ARGSUSED */
static int
ata_disk_ioctl(
	gtgt_t *gtgtp,
	int cmd,
	int arg,
	int flag)
{
	ata_ctl_t *ata_ctlp = GTGTP2ATAP(gtgtp);
	ata_drv_t *ata_drvp = GTGTP2ATADRVP(gtgtp);
	int	   rc;
	struct tgdk_geom *tg;

	ADBG_TRACE(("ata_disk_ioctl entered, cmd = %d\n", cmd));

	switch (cmd) {

	case DIOCTL_GETGEOM:
	case DIOCTL_GETPHYGEOM:
		tg = (struct tgdk_geom *)arg;
		tg->g_cyl = ata_drvp->ad_bioscyl;
		tg->g_head = ata_drvp->ad_bioshd;
		tg->g_sec = ata_drvp->ad_biossec;
		tg->g_acyl = ata_drvp->ad_acyl;
		tg->g_secsiz = 512;
		tg->g_cap = tg->g_cyl * tg->g_head * tg->g_sec;
		return (0);

	case DCMD_UPDATE_GEOM:
/* ??? fix this to issue IDENTIFY DEVICE ??? */
/* might not be necessary since I don't know of any ATA/IDE that */
/* can change its geometry. On the other hand, ATAPI devices like the  */
/* LS-120 or PD/CD can change their geometry when new media is inserted */
		return (0);

	case DCMD_GET_STATE:
		rc = ata_queue_cmd(ata_disk_state, NULL, ata_ctlp, ata_drvp,
			gtgtp);
		break;

	case DCMD_LOCK:
	case DKIOCLOCK:
		rc = ata_queue_cmd(ata_disk_lock, NULL, ata_ctlp, ata_drvp,
			gtgtp);
		break;

	case DCMD_UNLOCK:
	case DKIOCUNLOCK:
		rc = ata_queue_cmd(ata_disk_unlock, NULL, ata_ctlp, ata_drvp,
			gtgtp);
		break;

	case DCMD_START_MOTOR:
	case CDROMSTART:
		rc = ata_queue_cmd(ata_disk_recalibrate, NULL, ata_ctlp,
			ata_drvp, gtgtp);
		break;

	case DCMD_STOP_MOTOR:
	case CDROMSTOP:
		rc = ata_queue_cmd(ata_disk_standby, NULL, ata_ctlp, ata_drvp,
			gtgtp);
		break;

	case DKIOCEJECT:
	case CDROMEJECT:
		rc = ata_queue_cmd(ata_disk_eject, NULL, ata_ctlp, ata_drvp,
			gtgtp);
		break;

	default:
		ADBG_WARN(("ata_disk_ioctl: unsupported cmd 0x%x\n", cmd));
		return (ENOTTY);
	}

	if (rc)
		return (0);
	return (ENXIO);

}


#ifdef ___not___used___
/*
 * Issue an ATA command to the drive using the packet already
 * allocated by the target driver
 */

int
ata_disk_do_ioctl(
	int	(*func)(ata_ctl_t *, ata_drv_t *, ata_pkt_t *),
	void	  *arg,
	ata_ctl_t *ata_ctlp,
	gtgt_t	  *gtgtp,
	cmpkt_t   *pktp)
{
	gcmd_t	  *gcmdp = CPKT2GCMD(pktp);
	ata_pkt_t *ata_pktp = GCMD2APKT(gcmdp);
	int	   rc;

	ata_pktp->ap_start = func;
	ata_pktp->ap_intr = NULL;
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
		return (ENXIO);
	}

	if (ata_pktp->ap_flags & AP_ERROR)
		return (ENXIO);
	return (0);
}
#endif



/*
 *
 * DADA pktalloc entry point
 *
 */

/* ARGSUSED */
static cmpkt_t *
ata_disk_pktalloc(
	gtgt_t *gtgtp,
	int (*callback)(),
	caddr_t arg)
{
	ata_drv_t *ata_drvp = GTGTP2ATADRVP(gtgtp);
	cmpkt_t   *pktp;
	ata_pkt_t *ata_pktp;
	gcmd_t    *gcmdp;

	ADBG_TRACE(("ata_disk_pktalloc entered\n"));


	/*
	 * Allocate and  init the GHD gcmd_t structure and the
	 * DADA cmpkt and the ata_pkt
	 */
	if ((gcmdp = ghd_gcmd_alloc(gtgtp,
				    (sizeof (cmpkt_t) + sizeof (ata_pkt_t)),
				    (callback == DDI_DMA_SLEEP))) == NULL) {
		return ((cmpkt_t *)NULL);
	}
	ASSERT(gcmdp != NULL);

	ata_pktp = GCMD2APKT(gcmdp);
	ASSERT(ata_pktp != NULL);

	pktp = (cmpkt_t *)(ata_pktp + 1);

	pktp->cp_ctl_private = (void *)gcmdp;
	ata_pktp->ap_gcmdp = gcmdp;
	gcmdp->cmd_pktp = (void *)pktp;

	/*
	 * At this point the structures are linked like this:
	 *
	 *	(struct cmpkt) <--> (struct gcmd) <--> (struct ata_pkt)
	 */

	/* callback functions */

	ata_pktp->ap_start = ata_disk_start;
	ata_pktp->ap_intr = ata_disk_intr;
	ata_pktp->ap_complete = ata_disk_complete;

	/* other ata_pkt setup */

	ata_pktp->ap_bytes_per_block = ata_drvp->ad_bytes_per_block;

	/* cmpkt setup */

	pktp->cp_cdblen = 1;
	pktp->cp_cdbp   = (opaque_t)&ata_pktp->ap_cdb;
	pktp->cp_scbp   = (opaque_t)&ata_pktp->ap_scb;
	pktp->cp_scblen = 1;

	return (pktp);
}



/*
 *
 * DADA pktfree entry point
 *
 */

/* ARGSUSED */
static void
ata_disk_pktfree(
	gtgt_t *gtgtp,
	cmpkt_t *pktp)
{
	ata_pkt_t *ata_pktp = CPKT2APKT(pktp);

	ADBG_TRACE(("ata_disk_pktfree entered\n"));

	/* check not free already */

	ASSERT(!(ata_pktp->ap_flags & AP_FREE));
	ata_pktp->ap_flags = AP_FREE;

	ghd_gcmd_free(CPKT2GCMD(pktp));
}



/*
 *
 * DADA memsetup entry point
 *
 */

/* ARGSUSED */
static cmpkt_t *
ata_disk_memsetup(
	gtgt_t		*gtgtp,
	cmpkt_t		*pktp,
	struct buf	*bp,
	int		(*callback)(),
	caddr_t		arg)
{
	ata_pkt_t		*ata_pktp = CPKT2APKT(pktp);
	gcmd_t			*gcmdp = APKT2GCMD(ata_pktp);
	int			flags;

	ADBG_TRACE(("ata_disk_memsetup entered\n"));

	ata_pktp->ap_sg_cnt = 0;

	if (bp->b_bcount == 0) {
		ata_pktp->ap_v_addr = NULL;
		return (pktp);
	}

	if (GTGTP2ATADRVP(gtgtp)->ad_pciide_dma == FALSE)
		goto skip_dma_setup;

	if (ata_dma_disabled)
		goto skip_dma_setup;

	/*
	 * The PCI-IDE DMA engine is brain-damaged and can't
	 * DMA non-aligned buffers.
	 */
	if (!(bp->b_flags & B_PAGEIO) &&
	    ((uint_t)bp->b_un.b_addr) & PCIIDE_PRDE_ADDR_MASK) {
		goto skip_dma_setup;
	}

	/*
	 * It also insists that the byte count must be even.
	 */
	if (bp->b_bcount & 1)
		goto skip_dma_setup;

	/* check direction for data transfer */
	if (bp->b_flags & B_READ)
		flags = DDI_DMA_READ;
	else
		flags = DDI_DMA_WRITE;

	/*
	 * Bind the DMA handle to the buf
	 */
	if (ghd_dma_buf_bind_attr(&GTGTP2ATAP(gtgtp)->ac_ccc, gcmdp, bp, flags,
			callback, arg, &GTGTP2ATATGTP(gtgtp)->at_dma_attr)) {
		ata_pktp->ap_v_addr = 0;
		return (pktp);
	}

skip_dma_setup:
	bp_mapin(bp);
	ata_pktp->ap_v_addr = bp->b_un.b_addr;
	return (pktp);
}



/*
 *
 * DADA memfree entry point
 *
 */

/*
 * 1157317 sez that drivers shouldn't call bp_mapout(), as either
 * biodone() or biowait() will end up doing it, but after they
 * call bp->b_iodone(), which is a necessary sequence for
 * Online Disk Suite.  However, the DDI group wants to rethink
 * bp_mapin()/bp_mapout() and how they should behave in the
 * presence of layered drivers, etc.  For the moment, fix
 * the OLDS problem by removing the bp_mapout() call.
 */

#define	BUG_1157317

/* ARGSUSED */
static void
ata_disk_memfree(
	gtgt_t *gtgtp,
	cmpkt_t *pktp)
{
	gcmd_t	*gcmdp = CPKT2GCMD(pktp);

	ADBG_TRACE(("ata_disk_memfree entered\n"));

	if (gcmdp->cmd_dma_handle)
		ghd_dmafree_attr(gcmdp);
#if !defined(BUG_1157317)
	else
		bp_mapout(pktp->cp_bp);
#endif
}



/*
 *
 * DADA iosetup entry point
 *
 */

static cmpkt_t *
ata_disk_iosetup(
	gtgt_t *gtgtp,
	cmpkt_t *pktp)
{
	ata_drv_t		*ata_drvp = GTGTP2ATADRVP(gtgtp);
	ata_pkt_t		*ata_pktp = CPKT2APKT(pktp);
	gcmd_t			*gcmdp = APKT2GCMD(ata_pktp);
	uint_t			sec_count;
	uint_t			start_sec;
	uint_t			resid;
	uint_t			byte_count;
	uint_t			cyl;
	uchar_t			head;
	uchar_t			drvheads;
	uchar_t			drvsectors;

	ADBG_TRACE(("ata_disk_iosetup entered\n"));

	/* setup the task file registers */

	drvheads = ata_drvp->ad_phhd;
	drvsectors = ata_drvp->ad_phsec;

	/* check for error retry */
	if (ata_pktp->ap_flags & AP_ERROR) {
		/*
		 * this is a temporary work-around for dadk calling
		 * iosetup for retry (bug id 4242937). The correct
		 * solution is changing dadk to not to call iosetup
		 * for a retry.
		 * We do not aply the work-around for pio mode since
		 * that does not involve moving dma windows and reducing the
		 * sector count would work for pio mode on a retry
		 * for now.
		 */
		if (gcmdp->cmd_dma_handle != NULL) {
			ata_pktp->ap_flags = 0;
			return (NULL);
		}

		ata_pktp->ap_bytes_per_block = NBPSCTR;
		sec_count = 1;

		/*
		 * (Bug# 4160262)
		 * Since we are retrying the last read or write operation,
		 * restore the old values of the ap_v_addr and ap_resid.
		 * This assumes CTL_IOSETUP is called again on retry; if not,
		 * this needs to be done in CTL_TRANSPORT.
		 */
		if (ata_pktp->ap_flags & (AP_READ | AP_WRITE)) {
			ata_pktp->ap_v_addr = ata_pktp->ap_v_addr_sav;
			ata_pktp->ap_resid = ata_pktp->ap_resid_sav;
		}
	} else {
		/*
		 * Limit request to ac_max_transfer sectors.
		 * The value is specified by the user in the
		 * max_transfer property. It must be in the range 1 to 256.
		 * When max_transfer is 0x100 it is bigger than 8 bits.
		 * The spec says 0 represents 256 so it should be OK.
		 */
		sec_count = min((pktp->cp_bytexfer >> SCTRSHFT),
				ata_drvp->ad_ctlp->ac_max_transfer);
		/*
		 * (Bug# 4160262)
		 * Save the current values of ap_v_addr and ap_resid
		 * in case a retry operation happens. During a retry
		 * operation we need to restore these values.
		 */
		ata_pktp->ap_v_addr_sav = ata_pktp->ap_v_addr;
		ata_pktp->ap_resid_sav = ata_pktp->ap_resid;
	}

	/* reset flags */
	ata_pktp->ap_flags = 0;

#ifdef	DADKIO_RWCMD_READ
	start_sec = pktp->cp_passthru ? RWCMDP(pktp)->blkaddr : pktp->cp_srtsec;
#else
	start_sec = pktp->cp_srtsec;
#endif

	/*
	 * Setup the PCIDE Bus Master Scatter/Gather list
	 */
	ata_pktp->ap_sg_cnt = 0;
	ata_pktp->ap_pciide_dma = FALSE;
	if (gcmdp->cmd_dma_handle != NULL && sec_count != 0) {
		byte_count = sec_count << SCTRSHFT;
		if ((ghd_dmaget_attr(&GTGTP2ATAP(gtgtp)->ac_ccc, gcmdp,
			byte_count, ATA_DMA_NSEGS, &byte_count) == FALSE) ||
			(byte_count == 0)) {
			ADBG_ERROR(("ata_disk_iosetup: byte count zero\n"));
			return (NULL);
		}
		sec_count = byte_count >> SCTRSHFT;
	}

	ata_pktp->ap_count = (uchar_t)sec_count;
	if (ata_drvp->ad_drive_bits & ATDH_LBA) {
		ata_pktp->ap_sec = start_sec & 0xff;
		ata_pktp->ap_lwcyl = (start_sec >> 8) & 0xff;
		ata_pktp->ap_hicyl = (start_sec >> 16) & 0xff;
		ata_pktp->ap_hd = (start_sec >> 24) & 0xff;
		ata_pktp->ap_hd |= ata_drvp->ad_drive_bits;
	} else {
		resid = start_sec / drvsectors;
		head = resid % drvheads;
		cyl = resid / drvheads;
		ata_pktp->ap_sec = (start_sec % drvsectors) + 1;
		ata_pktp->ap_hd = head | ata_drvp->ad_drive_bits;
		ata_pktp->ap_lwcyl = cyl;  /* auto truncate to char */
				/* automatically truncate to char */
		ata_pktp->ap_hicyl = (cyl >> 8);
	}

#ifdef	DADKIO_RWCMD_READ
	if (pktp->cp_passthru) {
		switch (RWCMDP(pktp)->cmd) {
		case DADKIO_RWCMD_READ:
			if (ata_pktp->ap_sg_cnt) {
				ata_pktp->ap_cmd = ATC_READ_DMA;
				ata_pktp->ap_pciide_dma = TRUE;
				ata_pktp->ap_start = ata_disk_start_dma_in;
				ata_pktp->ap_intr = ata_disk_intr_dma;
			} else {
				ata_pktp->ap_cmd = ATC_RDSEC;
				ata_pktp->ap_start = ata_disk_start_pio_in;
				ata_pktp->ap_intr = ata_disk_intr_pio_in;
			}
			ata_pktp->ap_flags |= AP_READ;
			break;
		case DADKIO_RWCMD_WRITE:
			if (ata_pktp->ap_sg_cnt) {
				ata_pktp->ap_cmd = ATC_WRITE_DMA;
				ata_pktp->ap_pciide_dma = TRUE;
				ata_pktp->ap_start = ata_disk_start_dma_out;
				ata_pktp->ap_intr = ata_disk_intr_dma;
			} else {
				ata_pktp->ap_cmd = ATC_WRSEC;
				ata_pktp->ap_start = ata_disk_start_pio_out;
				ata_pktp->ap_intr = ata_disk_intr_pio_out;
			}
			ata_pktp->ap_flags |= AP_WRITE;
			break;
		}

		byte_count = RWCMDP(pktp)->buflen;
		pktp->cp_bytexfer = byte_count;
		pktp->cp_resid = byte_count;
		ata_pktp->ap_resid = byte_count;

		/*
		 * since we're not using READ/WRITE MULTIPLE, we
		 * should set bytes_per_block to one sector
		 * XXX- why wasn't this in the old driver??
		 */
		ata_pktp->ap_bytes_per_block = NBPSCTR;
	} else
#endif
	{
		byte_count = sec_count << SCTRSHFT;
		pktp->cp_bytexfer = byte_count;
		pktp->cp_resid = byte_count;
		ata_pktp->ap_resid = byte_count;

		/* setup the task file registers */

		switch (ata_pktp->ap_cdb) {
		case DCMD_READ:
			if (ata_pktp->ap_sg_cnt) {
				ata_pktp->ap_cmd = ATC_READ_DMA;
				ata_pktp->ap_pciide_dma = TRUE;
				ata_pktp->ap_start = ata_disk_start_dma_in;
				ata_pktp->ap_intr = ata_disk_intr_dma;
			} else {
				ata_pktp->ap_cmd = ata_drvp->ad_rd_cmd;
				ata_pktp->ap_start = ata_disk_start_pio_in;
				ata_pktp->ap_intr = ata_disk_intr_pio_in;
			}
			ata_pktp->ap_flags |= AP_READ;
			break;

		case DCMD_WRITE:
			if (ata_pktp->ap_sg_cnt) {
				ata_pktp->ap_cmd = ATC_WRITE_DMA;
				ata_pktp->ap_pciide_dma = TRUE;
				ata_pktp->ap_start = ata_disk_start_dma_out;
				ata_pktp->ap_intr = ata_disk_intr_dma;
			} else {
				ata_pktp->ap_cmd = ata_drvp->ad_wr_cmd;
				ata_pktp->ap_start = ata_disk_start_pio_out;
				ata_pktp->ap_intr = ata_disk_intr_pio_out;
			}
			ata_pktp->ap_flags |= AP_WRITE;
			break;

		default:
			ADBG_WARN(("ata_disk_iosetup: unknown command 0x%x\n",
					ata_pktp->ap_cdb));
			pktp = NULL;
			break;
		}
	}

	return (pktp);
}



/*
 *
 * DADA transport entry point
 *
 */

static int
ata_disk_transport(
	gtgt_t *gtgtp,
	cmpkt_t *pktp)
{
	ata_drv_t *ata_drvp = GTGTP2ATADRVP(gtgtp);
	ata_ctl_t *ata_ctlp = ata_drvp->ad_ctlp;
	ata_pkt_t *ata_pktp = CPKT2APKT(pktp);
	int	   rc;
	int	   polled = FALSE;

	ADBG_TRACE(("ata_disk_transport entered\n"));

	/* check for polling pkt */

	if (pktp->cp_flags & CPF_NOINTR) {
		polled = TRUE;
	}

	/* call ghd transport routine */

	rc = ghd_transport(&ata_ctlp->ac_ccc, APKT2GCMD(ata_pktp),
		gtgtp, pktp->cp_time, polled, NULL);

	/* see if pkt was not accepted */

	if (rc == TRAN_BUSY)
		return (CTL_SEND_BUSY);

	if (rc == TRAN_ACCEPT)
		return (CTL_SEND_SUCCESS);

	return (CTL_SEND_FAILURE);
}



/*
 *
 * packet start callback routines
 *
 */

/* ARGSUSED */
static int
ata_disk_start_common(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;

	ADBG_TRACE(("ata_disk_start_common entered\n"));

	ADBG_TRANSPORT(("ata_disk_start:\tpkt = 0x%x, pkt flags = 0x%x\n",
		(uint_t)ata_pktp, ata_pktp->ap_flags));
	ADBG_TRANSPORT(("\tcommand=0x%x, drvhd=0x%x, sect=0x%x\n",
		ata_pktp->ap_cmd, ata_pktp->ap_hd, ata_pktp->ap_sec));
	ADBG_TRANSPORT(("\tcount=0x%x, lwcyl=0x%x, hicyl=0x%x\n",
		ata_pktp->ap_count, ata_pktp->ap_lwcyl, ata_pktp->ap_hicyl));


	/*
	 * Bug 1256489:
	 *
	 * If AC_BSY_WAIT is set, wait for controller to not be busy,
	 * before issuing a command.  If AC_BSY_WAIT is not set,
	 * skip the wait.  This is important for laptops that do
	 * suspend/resume but do not correctly wait for the busy bit to
	 * drop after a resume.
	 *
	 * NOTE: this test for ATS_BSY is also needed if/when we
	 * implement the overlapped/queued command protocols. Currently,
	 * the overlap/queued feature is not supported so the test is
	 * conditional.
	 */
	if (ata_ctlp->ac_timing_flags & AC_BSY_WAIT) {
		if (!ata_wait(io_hdl2,  ata_ctlp->ac_ioaddr2,
				0, ATS_BSY, 5000000)) {
			ADBG_ERROR(("ata_disk_start: BUSY\n"));
			return (FALSE);
		}
	}

	ddi_putb(io_hdl1,  ata_ctlp->ac_drvhd, ata_pktp->ap_hd);
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/*
	 * make certain the drive selected
	 */
	if (!ata_wait(io_hdl2,  ata_ctlp->ac_ioaddr2,
			ATS_DRDY, ATS_BSY, 5 * 1000000)) {
		ADBG_ERROR(("ata_disk_start: select failed\n"));
		return (FALSE);
	}

	ddi_putb(io_hdl1, ata_ctlp->ac_sect, ata_pktp->ap_sec);
	ddi_putb(io_hdl1, ata_ctlp->ac_count, ata_pktp->ap_count);
	ddi_putb(io_hdl1, ata_ctlp->ac_lcyl, ata_pktp->ap_lwcyl);
	ddi_putb(io_hdl1, ata_ctlp->ac_hcyl, ata_pktp->ap_hicyl);
	ddi_putb(io_hdl1, ata_ctlp->ac_feature, 0);

	/*
	 * Always make certain interrupts are enabled. It's been reported
	 * (but not confirmed) that some notebook computers don't
	 * clear the interrupt disable bit after being resumed. The
	 * easiest way to fix this is to always clear the disable bit
	 * before every command.
	 */
	ddi_putb(io_hdl2, ata_ctlp->ac_devctl, ATDC_D3);
	return (TRUE);
}


/*
 *
 * Start a non-data ATA command (not DMA and not PIO):
 *
 */

static int
ata_disk_start(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 rc;

	rc = ata_disk_start_common(ata_ctlp, ata_drvp, ata_pktp);

	if (!rc)
		return (ATA_FSM_RC_BUSY);

	/*
	 * This next one sets the controller in motion
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, ata_pktp->ap_cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	return (ATA_FSM_RC_OKAY);
}



static int
ata_disk_start_dma_in(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 rc;

	rc = ata_disk_start_common(ata_ctlp, ata_drvp, ata_pktp);

	if (!rc)
		return (ATA_FSM_RC_BUSY);

	/*
	 * Copy the Scatter/Gather list to the controller's
	 * Physical Region Descriptor Table
	 */
	ata_pciide_dma_setup(ata_ctlp, ata_pktp->ap_sg_list,
		ata_pktp->ap_sg_cnt);

	/*
	 * reset the PCIIDE Controller's interrupt and error status bits
	 */
	(void) ata_pciide_status_clear(ata_ctlp);

	/*
	 * This next one sets the drive in motion
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, ata_pktp->ap_cmd);

	/* wait for the drive's busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	ata_pciide_dma_start(ata_ctlp, PCIIDE_BMICX_RWCON_WRITE_TO_MEMORY);

	return (ATA_FSM_RC_OKAY);
}



static int
ata_disk_start_dma_out(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 rc;

	rc = ata_disk_start_common(ata_ctlp, ata_drvp, ata_pktp);

	if (!rc)
		return (ATA_FSM_RC_BUSY);

	/*
	 * Copy the Scatter/Gather list to the controller's
	 * Physical Region Descriptor Table
	 */
	ata_pciide_dma_setup(ata_ctlp, ata_pktp->ap_sg_list,
		ata_pktp->ap_sg_cnt);

	/*
	 * reset the PCIIDE Controller's interrupt and error status bits
	 */
	(void) ata_pciide_status_clear(ata_ctlp);

	/*
	 * This next one sets the drive in motion
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, ata_pktp->ap_cmd);

	/* wait for the drive's busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	ata_pciide_dma_start(ata_ctlp, PCIIDE_BMICX_RWCON_READ_FROM_MEMORY);

	return (ATA_FSM_RC_OKAY);
}





/*
 *
 * Start a PIO data-in ATA command:
 *
 */

static int
ata_disk_start_pio_in(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 rc;

	rc = ata_disk_start_common(ata_ctlp, ata_drvp, ata_pktp);

	if (!rc)
		return (ATA_FSM_RC_BUSY);
	/*
	 * This next one sets the controller in motion
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, ata_pktp->ap_cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	return (ATA_FSM_RC_OKAY);
}




/*
 *
 * Start a PIO data-out ATA command:
 *
 */

static int
ata_disk_start_pio_out(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 rc;

	ata_pktp->ap_wrt_count = 0;

	rc = ata_disk_start_common(ata_ctlp, ata_drvp, ata_pktp);

	if (!rc)
		return (ATA_FSM_RC_BUSY);
	/*
	 * This next one sets the controller in motion
	 */
	ddi_putb(io_hdl1, ata_ctlp->ac_cmd, ata_pktp->ap_cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/*
	 * Wait for the drive to assert DRQ to send the first chunk
	 * of data. Have to busy wait because there's no interrupt for
	 * the first chunk. This sucks (a lot of cycles) if the
	 * drive responds too slowly or if the wait loop granularity
	 * is too large. It's really bad if the drive is defective and
	 * the loop times out.
	 */

	if (!ata_wait3(io_hdl2, ata_ctlp->ac_ioaddr2,
			ATS_DRQ, ATS_BSY, /* okay */
			ATS_ERR, ATS_BSY, /* cmd failed */
			ATS_DF, ATS_BSY, /* drive failed */
			4000000)) {
		ADBG_WARN(("ata_disk_start_pio_out: no DRQ\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		return (ATA_FSM_RC_INTR);
	}

	/*
	 * Tell the upper layer to fake a hardware interrupt which
	 * actually causes the first segment to be written to the drive.
	 */
	return (ATA_FSM_RC_INTR);
}



/*
 *
 * packet complete callback routine
 *
 */

static void
ata_disk_complete(
	ata_pkt_t *ata_pktp,
	int do_callback)
{
	cmpkt_t	*pktp;

	ADBG_TRACE(("ata_disk_complete entered\n"));
	ADBG_TRANSPORT(("ata_disk_complete: pkt = 0x%x\n", (uint_t)ata_pktp));

	pktp = APKT2CPKT(ata_pktp);

	/* update resid */

	pktp->cp_resid = ata_pktp->ap_resid;

	if (ata_pktp->ap_flags & AP_ERROR) {

		pktp->cp_reason = CPS_CHKERR;

		if (ata_pktp->ap_error & ATE_BBK)
			ata_pktp->ap_scb = DERR_BBK;
		else if (ata_pktp->ap_error & ATE_UNC)
			ata_pktp->ap_scb = DERR_UNC;
		else if (ata_pktp->ap_error & ATE_IDNF)
			ata_pktp->ap_scb = DERR_IDNF;
		else if (ata_pktp->ap_error & ATE_TKONF)
			ata_pktp->ap_scb = DERR_TKONF;
		else if (ata_pktp->ap_error & ATE_AMNF)
			ata_pktp->ap_scb = DERR_AMNF;
		else if (ata_pktp->ap_status & ATS_BSY)
			ata_pktp->ap_scb = DERR_BUSY;
		else if (ata_pktp->ap_status & ATS_DF)
			ata_pktp->ap_scb = DERR_DWF;
		else /* any unknown error	*/
			ata_pktp->ap_scb = DERR_ABORT;
	} else if (ata_pktp->ap_flags &
			(AP_ABORT|AP_TIMEOUT|AP_BUS_RESET)) {

		pktp->cp_reason = CPS_CHKERR;
		ata_pktp->ap_scb = DERR_ABORT;
	} else {
		pktp->cp_reason = CPS_SUCCESS;
		ata_pktp->ap_scb = DERR_SUCCESS;
	}

	/* callback */
	if (do_callback)
		(*pktp->cp_callback)(pktp);
}


/*
 *
 * Interrupt callbacks
 *
 */


/*
 *
 * ATA command, no data
 *
 */

/* ARGSUSED */
static int
ata_disk_intr(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	uchar_t		 status;

	ADBG_TRACE(("ata_disk_intr entered\n"));
	ADBG_TRANSPORT(("ata_disk_intr: pkt = 0x%x\n", (uint_t)ata_pktp));

	status = ata_get_status_clear_intr(ata_ctlp, ata_pktp);

	ASSERT((status & (ATS_BSY | ATS_DRQ)) == 0);

	/*
	 * check for errors
	 */

	if (status & (ATS_DF | ATS_ERR)) {
		ADBG_WARN(("ata_disk_intr: status 0x%x error 0x%x\n", status,
			ddi_getb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_error)));
		ata_pktp->ap_flags |= AP_ERROR;
	}

	if (ata_pktp->ap_flags & AP_ERROR) {
		ata_pktp->ap_status = ddi_getb(ata_ctlp->ac_iohandle2,
			ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(ata_ctlp->ac_iohandle1,
			ata_ctlp->ac_error);
	}

	/* tell the upper layer this request is complete */
	return (ATA_FSM_RC_FINI);
}


/*
 *
 * ATA command, PIO data in
 *
 */

/* ARGSUSED */
static int
ata_disk_intr_pio_in(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	uchar_t		 status;

	ADBG_TRACE(("ata_disk_pio_in entered\n"));
	ADBG_TRANSPORT(("ata_disk_pio_in: pkt = 0x%x\n", (uint_t)ata_pktp));

	/*
	 * first make certain DRQ is asserted (and no errors)
	 */
	(void) ata_wait3(io_hdl2, ata_ctlp->ac_ioaddr2,
			ATS_DRQ, ATS_BSY, ATS_ERR, ATS_BSY, ATS_DF, ATS_BSY,
			4000000);

	status = ata_get_status_clear_intr(ata_ctlp, ata_pktp);

	if (status & ATS_BSY) {
		ADBG_WARN(("ata_disk_pio_in: BUSY\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
		return (ATA_FSM_RC_BUSY);
	}

	/*
	 * record any errors
	 */
	if ((status & (ATS_DRQ | ATS_DF | ATS_ERR)) != ATS_DRQ) {
		ADBG_WARN(("ata_disk_pio_in: status 0x%x error 0x%x\n",
			status, ddi_getb(io_hdl1, ata_ctlp->ac_error)));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
	}

	/*
	 * read the next chunk of data (if any)
	 */
	if (status & ATS_DRQ) {
		ata_disk_pio_xfer_data_in(ata_ctlp, ata_pktp);
	}

	/*
	 * If that was the last chunk, wait for the device to clear DRQ
	 */
	if (ata_pktp->ap_resid == 0) {
		if (ata_wait(io_hdl2, ata_ctlp->ac_ioaddr2,
			0, (ATS_DRQ | ATS_BSY), 4000000)) {
			/* tell the upper layer this request is complete */
			return (ATA_FSM_RC_FINI);
		}

		ADBG_WARN(("ata_disk_pio_in: DRQ stuck\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
	}

	/*
	 * check for errors
	 */
	if (ata_pktp->ap_flags & AP_ERROR) {
		return (ATA_FSM_RC_FINI);
	}

	/*
	 * If the read command isn't done yet,
	 * wait for the next interrupt.
	 */
	ADBG_TRACE(("ata_disk_pio_in: partial\n"));
	return (ATA_FSM_RC_OKAY);
}



/*
 *
 * ATA command, PIO data out
 *
 */

/* ARGSUSED */
static int
ata_disk_intr_pio_out(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 tmp_count = ata_pktp->ap_wrt_count;
	uchar_t		 status;

	/*
	 * clear the IRQ
	 */
	status = ata_get_status_clear_intr(ata_ctlp, ata_pktp);

	ADBG_TRACE(("ata_disk_intr_pio_out entered\n"));
	ADBG_TRANSPORT(("ata_disk_intr_pio_out: pkt = 0x%x\n",
			(uint_t)ata_pktp));

	ASSERT(!(status & ATS_BSY));


	/*
	 * check for errors
	 */

	if (status & (ATS_DF | ATS_ERR)) {
		ADBG_WARN(("ata_disk_intr_pio_out: status 0x%x error 0x%x\n",
		status, ddi_getb(io_hdl1, ata_ctlp->ac_error)));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
		/* tell the upper layer this request is complete */
		return (ATA_FSM_RC_FINI);
	}


	/*
	 * last write was okay, bump the ptr and
	 * decr the resid count
	 */
	ata_pktp->ap_v_addr += tmp_count;
	ata_pktp->ap_resid -= tmp_count;

	/*
	 * check for final interrupt on write command
	 */
	if (ata_pktp->ap_resid <= 0) {
		/* tell the upper layer this request is complete */
		return (ATA_FSM_RC_FINI);
	}

	/*
	 * Perform the next data transfer
	 *
	 * First make certain DRQ is asserted and no error status.
	 * (I'm not certain but I think some drives might deassert BSY
	 * before asserting DRQ. This extra ata_wait3() will
	 * compensate for such drives).
	 *
	 */
	(void) ata_wait3(io_hdl2, ata_ctlp->ac_ioaddr2,
		ATS_DRQ, ATS_BSY, ATS_ERR, ATS_BSY, ATS_DF, ATS_BSY, 4000000);

	status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);

	if (status & ATS_BSY) {
		/* this should never happen */
		ADBG_WARN(("ata_disk_intr_pio_out: BUSY\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
		return (ATA_FSM_RC_BUSY);
	}

	/*
	 * bailout if any errors
	 */
	if ((status & (ATS_DRQ | ATS_DF | ATS_ERR)) != ATS_DRQ) {
		ADBG_WARN(("ata_disk_pio_out: status 0x%x error 0x%x\n",
			status, ddi_getb(io_hdl1, ata_ctlp->ac_error)));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
		return (ATA_FSM_RC_FINI);
	}

	/*
	 * write  the next chunk of data
	 */
	ADBG_TRACE(("ata_disk_intr_pio_out: write xfer\n"));
	ata_disk_pio_xfer_data_out(ata_ctlp, ata_pktp);

	/*
	 * Wait for the next interrupt before checking the transfer
	 * status and adjusting the tranfer count.
	 *
	 */
	return (ATA_FSM_RC_OKAY);
}


/*
 *
 * ATA command, DMA data in/out
 *
 */

static int
ata_disk_intr_dma(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	uchar_t		 status;

	ADBG_TRACE(("ata_disk_intr_dma entered\n"));
	ADBG_TRANSPORT(("ata_disk_intr_dma: pkt = 0x%x\n", (uint_t)ata_pktp));

	/*
	 * halt the DMA engine
	 */
	ata_pciide_dma_stop(ata_ctlp);

	/*
	 * wait for the device to clear DRQ
	 */
	if (!ata_wait(io_hdl2, ata_ctlp->ac_ioaddr2,
			0, (ATS_DRQ | ATS_BSY), 4000000)) {
		ADBG_WARN(("ata_disk_intr_dma: DRQ stuck\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
		return (ATA_FSM_RC_BUSY);
	}

	/*
	 * get the status and clear the IRQ, and check for DMA error
	 */
	status = ata_get_status_clear_intr(ata_ctlp, ata_pktp);

	/*
	 * check for drive errors
	 */

	if (status & (ATS_DF | ATS_ERR)) {
		ADBG_WARN(("ata_disk_intr_dma: status 0x%x error 0x%x\n",
			status, ddi_getb(io_hdl1, ata_ctlp->ac_error)));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_getb(io_hdl2, ata_ctlp->ac_altstatus);
		ata_pktp->ap_error = ddi_getb(io_hdl1, ata_ctlp->ac_error);
	}

	/*
	 * If there was a drive or DMA error, compute a resid count
	 */
	if (ata_pktp->ap_flags & AP_ERROR) {
		/*
		 * grab the last sector address from the drive regs
		 * and use that to compute the resid
		 */
		ata_disk_get_resid(ata_ctlp, ata_drvp, ata_pktp);
	} else {
		ata_pktp->ap_resid = 0;
	}

	/* tell the upper layer this request is complete */
	return (ATA_FSM_RC_FINI);
}


/*
 *
 * Low level PIO routine that transfers data from the drive
 *
 */

static void
ata_disk_pio_xfer_data_in(
	ata_ctl_t *ata_ctlp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 count;

	count = min(ata_pktp->ap_resid,
			ata_pktp->ap_bytes_per_block);

	ADBG_TRANSPORT(("ata_disk_pio_xfer_data_in: 0x%x bytes, addr = 0x%x\n",
			count, ata_pktp->ap_v_addr));

	/*
	 * read count bytes
	 */

	ASSERT(count != 0);

	ddi_rep_getw(io_hdl1, (ushort_t *)ata_pktp->ap_v_addr,
		ata_ctlp->ac_data, (count >> 1), DDI_DEV_NO_AUTOINCR);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/*
	 * this read command completed okay, bump the ptr and
	 * decr the resid count now.
	 */
	ata_pktp->ap_v_addr += count;
	ata_pktp->ap_resid -= count;
}


/*
 *
 * Low level PIO routine that transfers data to the drive
 *
 */

static void
ata_disk_pio_xfer_data_out(
	ata_ctl_t *ata_ctlp,
	ata_pkt_t *ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	ddi_acc_handle_t io_hdl2 = ata_ctlp->ac_iohandle2;
	int		 count;

	count = min(ata_pktp->ap_resid,
			ata_pktp->ap_bytes_per_block);

	ADBG_TRANSPORT(("ata_disk_pio_xfer_data_out: 0x%x bytes, addr = 0x%x\n",
			count, ata_pktp->ap_v_addr));

	/*
	 * read or write count bytes
	 */

	ASSERT(count != 0);

	ddi_rep_putw(io_hdl1, (ushort_t *)ata_pktp->ap_v_addr,
		ata_ctlp->ac_data, (count >> 1), DDI_DEV_NO_AUTOINCR);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(io_hdl2, ata_ctlp->ac_ioaddr2);

	/*
	 * save the count here so I can correctly adjust
	 * the ap_v_addr and ap_resid values at the next
	 * interrupt.
	 */
	ata_pktp->ap_wrt_count = count;
}


/*
 *
 * ATA Initialize Device Parameters (aka Set Params) command
 *
 * If the drive was put in some sort of CHS extended/logical geometry
 * mode by the BIOS, this function will reset it to its "native"
 * CHS geometry. This ensures that we don't run into any sort of
 * 1024 cylinder (or 65535 cylinder) limitation that may have been
 * created by a BIOS (or users) that chooses a bogus translated geometry.
 */

static int
ata_disk_initialize_device_parameters(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{
	int		 rc;

	rc = ata_command(ata_ctlp, ata_drvp, FALSE, FALSE,
			ata_disk_init_dev_parm_wait,
			ATC_SETPARAM,
			0, 			/* feature n/a */
			ata_drvp->ad_phsec,	/* max sector (1-based) */
			0,			/* sector n/a */
			(ata_drvp->ad_phhd -1),	/* max head (0-based) */
			0,			/* cyl_low n/a */
			0);			/* cyl_hi n/a */

	if (rc) {
		return (TRUE);
	}

	ADBG_ERROR(("ata_init_dev_parms: failed\n"));
	return (FALSE);
}



/*
 *
 * create fake inquiry data for DADA interface
 *
 */

static void
ata_disk_fake_inquiry(
	ata_drv_t *ata_drvp)
{
	struct ata_id *ata_idp = &ata_drvp->ad_id;
	struct scsi_inquiry *inqp = &ata_drvp->ad_inquiry;

	ADBG_TRACE(("ata_disk_fake_inquiry entered\n"));

	if (ata_idp->ai_config & ATA_ID_REM_DRV) /* ide removable bit */
		inqp->inq_rmb = 1;		/* scsi removable bit */

	(void) strncpy(inqp->inq_vid, "Gen-ATA ", sizeof (inqp->inq_vid));
	inqp->inq_dtype = DTYPE_DIRECT;
	inqp->inq_qual = DPQ_POSSIBLE;

	(void) strncpy(inqp->inq_pid, ata_idp->ai_model,
			sizeof (inqp->inq_pid));
	(void) strncpy(inqp->inq_revision, ata_idp->ai_fw,
			sizeof (inqp->inq_revision));
}

#define	LOOP_COUNT	10000


/*
 *
 * ATA Set Multiple Mode
 *
 */

static int
ata_disk_set_multiple(
	ata_ctl_t *ata_ctlp,
	ata_drv_t *ata_drvp)
{
	int		 rc;

	rc = ata_command(ata_ctlp, ata_drvp, TRUE, FALSE,
			ata_disk_set_mult_wait,
			ATC_SETMULT,
			0, 			/* feature n/a */
			ata_drvp->ad_block_factor, /* count */
			0,			/* sector n/a */
			0, 			/* head n/a */
			0,			/* cyl_low n/a */
			0);			/* cyl_hi n/a */

	if (rc) {
		return (TRUE);
	}

	ADBG_ERROR(("ata_disk_set_multiple: failed\n"));
	return (FALSE);
}


/*
 *
 * ATA Identify Device command
 *
 */

int
ata_disk_id(
	ddi_acc_handle_t io_hdl1,
	caddr_t		 ioaddr1,
	ddi_acc_handle_t io_hdl2,
	caddr_t		 ioaddr2,
	struct ata_id	*ata_idp)
{
	int	rc;

	ADBG_TRACE(("ata_disk_id entered\n"));

	rc = ata_id_common(ATC_ID_DEVICE, TRUE, io_hdl1, ioaddr1, io_hdl2,
		ioaddr2, ata_idp);

	if (!rc)
		return (FALSE);

	if ((ata_idp->ai_config & ATAC_ATA_TYPE_MASK) != ATAC_ATA_TYPE)
		return (FALSE);

	if (ata_idp->ai_heads == 0 || ata_idp->ai_sectors == 0) {
		return (FALSE);
	}

	return (TRUE);
}


/*
 *
 * Need to compute a value for ap_resid so that cp_resid can
 * be set by ata_disk_complete(). The cp_resid var is actually
 * misnamed. It's actually the offset to the block in which the
 * error occurred not the number of bytes transferred to the device.
 * At least that's how dadk actually uses the cp_resid when reporting
 * an error. In other words the sector that had the error and the
 * number of bytes transferred don't always indicate the same offset.
 * On top of that, when doing DMA transfers there's actually no
 * way to determine how many bytes have been transferred by the DMA
 * engine. On the other hand, the drive will report which sector
 * it faulted on. Using that address this routine computes the
 * number of residual bytes beyond that point which probably weren't
 * written to the drive (the drive is allowed to re-order sector
 * writes but on an ATA disk there's no way to deal with that
 * complication; in other words, the resid value calculated by
 * this routine is as good as we can manage).
 */

static void
ata_disk_get_resid(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ddi_acc_handle_t io_hdl1 = ata_ctlp->ac_iohandle1;
	uint_t		 lba_start;
	uint_t		 lba_stop;
	uint_t		 resid_sectors;
	uint_t		 resid_bytes;
	uchar_t		 sector;
	uchar_t		 head;
	uchar_t		 low_cyl;
	uchar_t		 hi_cyl;

	sector = ddi_getb(io_hdl1, ata_ctlp->ac_sect);
	head = ddi_getb(io_hdl1, ata_ctlp->ac_drvhd) & 0xf;
	low_cyl = ddi_getb(io_hdl1, ata_ctlp->ac_lcyl);
	hi_cyl = ddi_getb(io_hdl1, ata_ctlp->ac_hcyl);

	if (ata_drvp->ad_drive_bits & ATDH_LBA) {
		lba_start = ata_pktp->ap_sec;
		lba_start |= (uint_t)ata_pktp->ap_lwcyl << 8;
		lba_start |= (uint_t)ata_pktp->ap_hicyl << 16;
		lba_start |= ((uint_t)ata_pktp->ap_hd & 0xf) << 24;

		lba_stop = sector;
		lba_stop |= (((uint_t)low_cyl) << 8);
		lba_stop |= (((uint_t)hi_cyl) << 8);
		lba_stop |= (((uint_t)head) << 24);
	} else {
		uchar_t	drvheads = ata_drvp->ad_phhd;
		uchar_t	drvsectors = ata_drvp->ad_phsec;

		lba_start = ata_pktp->ap_lwcyl;
		lba_start |= (uint_t)ata_pktp->ap_hicyl << 8;
		lba_start *= (uint_t)drvheads;
		lba_start += (uint_t)head;
		lba_start *= (uint_t)drvsectors;
		lba_start += (uint_t)sector - 1;

		lba_stop = low_cyl;
		lba_stop |= (uint_t)hi_cyl << 8;
		lba_stop *= (uint_t)drvheads;
		lba_stop += (uint_t)head;
		lba_stop *= (uint_t)drvsectors;
		lba_stop += (uint_t)sector - 1;
	}

	resid_sectors = lba_start + ata_pktp->ap_count - lba_stop;
	resid_bytes = resid_sectors << SCTRSHFT;

	ADBG_TRACE(("ata_disk_get_resid start 0x%x cnt 0x%x stop 0x%x\n",
		    lba_start, ata_pktp->ap_count, lba_stop));
	ata_pktp->ap_resid = resid_bytes;
}



/*
 * Removable media commands *
 */



/*
 * get the media status
 *
 * NOTE: the error handling case probably isn't correct but it
 * will have to do until someone gives me a drive to test this on.
 */
static int
ata_disk_state(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	int	*statep = (int *)ata_pktp->ap_v_addr;
	uchar_t	 err;

	ADBG_TRACE(("ata_disk_state\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 5 * 1000000,
		    ATC_DOOR_LOCK, 0, 0, 0, 0, 0, 0)) {
		*statep = DKIO_INSERTED;
		return (ATA_FSM_RC_FINI);
	}

	err = ddi_getb(ata_ctlp->ac_iohandle1, ata_ctlp->ac_error);
	if (err & ATE_NM)
		*statep = DKIO_EJECTED;
	else
		*statep = DKIO_NONE;

	return (ATA_FSM_RC_FINI);
}

/*
 * eject the media
 */

static int
ata_disk_eject(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ADBG_TRACE(("ata_disk_eject\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 5 * 1000000,
			ATC_EJECT, 0, 0, 0, 0, 0, 0)) {
		return (ATA_FSM_RC_FINI);
	}
	ata_pktp->ap_flags |= AP_ERROR;
	return (ATA_FSM_RC_FINI);
}

/*
 * lock the drive
 *
 */
static int
ata_disk_lock(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ADBG_TRACE(("ata_disk_lock\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 5 * 1000000,
			ATC_DOOR_LOCK, 0, 0, 0, 0, 0, 0)) {
		return (ATA_FSM_RC_FINI);
	}
	ata_pktp->ap_flags |= AP_ERROR;
	return (ATA_FSM_RC_FINI);
}


/*
 * unlock the drive
 *
 */
static int
ata_disk_unlock(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ADBG_TRACE(("ata_disk_unlock\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 5 * 1000000,
			ATC_DOOR_UNLOCK, 0, 0, 0, 0, 0, 0)) {
		return (ATA_FSM_RC_FINI);
	}
	ata_pktp->ap_flags |= AP_ERROR;
	return (ATA_FSM_RC_FINI);
}


/*
 * put the drive into standby mode
 */
static int
ata_disk_standby(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ADBG_TRACE(("ata_disk_standby\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 5 * 1000000,
			ATC_STANDBY_IM, 0, 0, 0, 0, 0, 0)) {
		return (ATA_FSM_RC_FINI);
	}
	ata_pktp->ap_flags |= AP_ERROR;
	return (ATA_FSM_RC_FINI);
}


/*
 * Recalibrate
 *
 * Note the extra long timeout value. This is necessary in case
 * the drive was in standby mode and needs to spin up the media.
 *
 */
static int
ata_disk_recalibrate(
	ata_ctl_t	*ata_ctlp,
	ata_drv_t	*ata_drvp,
	ata_pkt_t	*ata_pktp)
{
	ADBG_TRACE(("ata_disk_recalibrate\n"));
	if (ata_command(ata_ctlp, ata_drvp, TRUE, TRUE, 31 * 1000000,
			ATC_RECAL, 0, 0, 0, 0, 0, 0)) {
		return (ATA_FSM_RC_FINI);
	}
	ata_pktp->ap_flags |= AP_ERROR;
	return (ATA_FSM_RC_FINI);
}
