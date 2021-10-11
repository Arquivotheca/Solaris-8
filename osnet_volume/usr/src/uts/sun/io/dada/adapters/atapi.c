/*
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)atapi.c 1.43	99/07/07 SMI"

#include <sys/types.h>
#include <sys/scsi/scsi.h>
#include <sys/dada/adapters/ata_common.h>
#include <sys/dada/adapters/atapi.h>


/*
 * External Functions.
 */
extern void
make_prd(gcmd_t *gcmdp, ddi_dma_cookie_t *cookie, int single_seg, int num_segs);
extern void
write_prd(struct ata_controller *ata_ctlp, struct ata_pkt *ata_pktp);
extern int prd_init(struct ata_controller *ata_ctlp, int chno);
extern void change_endian(unsigned char *string, int length);
extern void ata_ghd_complete_wraper(struct ata_controller *ata_ctlp,
			struct ata_pkt *ata_pktp, int chno);

/*
 * External Data.
 */
extern ddi_dma_attr_t ata_dma_attrs;

/*
 * SCSA entry points
 */
static int atapi_tran_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
	scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int atapi_tran_tgt_probe(struct scsi_device *sd, int (*callback)());
static void atapi_tran_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
	scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int atapi_tran_abort(struct scsi_address *ap, struct scsi_pkt *spktp);
static int atapi_tran_reset(struct scsi_address *ap, int level);
static int atapi_tran_getcap(struct scsi_address *ap, char *capstr, int whom);
static int atapi_tran_setcap(struct scsi_address *ap, char *capstr,
	int value, int whom);
static struct scsi_pkt *atapi_tran_init_pkt(struct scsi_address *ap,
	struct scsi_pkt *spktp, struct buf *bp, int cmdlen, int statuslen,
	int tgtlen, int flags, int (*callback)(caddr_t), caddr_t arg);
static void atapi_tran_destroy_pkt(struct scsi_address *ap,
		struct scsi_pkt *spktp);
static void atapi_tran_dmafree(struct scsi_address *ap, struct scsi_pkt *spktp);
static void atapi_tran_sync_pkt(struct scsi_address *ap,
		struct scsi_pkt *spktp);
static int atapi_tran_start(struct scsi_address *ap, struct scsi_pkt *spktp);

/*
 * packet callbacks
 */
static int atapi_start(struct ata_controller *ata_ctlp,
			struct ata_pkt *ata_pktp);
static void atapi_complete(struct ata_pkt *ata_pktp, int do_callback);
static int atapi_intr(struct ata_controller *ata_ctlp,
			struct ata_pkt *ata_pktp);

/*
 * local functions
 */
static int atapi_send_cdb(struct ata_controller *ata_ctlp, int chno);

#define	ATAPI_NO_OVERLAP
#undef	DSC_OVERLAP_SUPPORT

#ifdef DSC_OVERLAP_SUPPORT
static int atapi_dsc_init(struct ata_drive *ata_drvp, struct scsi_address *ap);
static void atapi_dsc_complete(struct ata_drive *ata_drvp);
#endif

/*
 * Local static data
 */
static ushort ata_bit_bucket[ATAPI_SECTOR_SIZE >> 1];
/*
 * Need to set the following variable to 1 in /etc/system if
 * it is desired to run just in pio mode
 */
int atapi_work_pio = 0;
static int atapi_id_timewarn = 0;

/*
 * initialize atapi sub-system
 */
int
atapi_init(struct ata_controller *ata_ctlp)
{
	dev_info_t *dip = ata_ctlp->ac_dip;
	scsi_hba_tran_t *tran;
	int value = 1;
	int targ;
	struct ata_drive *ata_drvp;
	struct dcd_identify *ad_id;
	u_char  mode;


	ADBG_TRACE(("atapi_init entered\n"));

	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, 0, "atapi",
		(caddr_t)&value, sizeof (int));

	/*
	 * allocate transport structure
	 */

	tran = scsi_hba_tran_alloc(dip, SCSI_HBA_CANSLEEP);

	if (tran == NULL) {
		ADBG_WARN(("atapi_init: scsi_hba_tran_alloc failed\n"));
		goto errout;
	}

	ata_ctlp->ac_atapi_tran = tran;
	ata_ctlp->ac_flags |= AC_SCSI_HBA_TRAN_ALLOC;

	/*
	 * initialize transport structure
	 */
	tran->tran_hba_private = ata_ctlp;
	tran->tran_tgt_private = NULL;

	tran->tran_tgt_init = atapi_tran_tgt_init;
	tran->tran_tgt_probe = atapi_tran_tgt_probe;
	tran->tran_tgt_free = atapi_tran_tgt_free;
	tran->tran_start = atapi_tran_start;
	tran->tran_reset = atapi_tran_reset;
	tran->tran_abort = atapi_tran_abort;
	tran->tran_getcap = atapi_tran_getcap;
	tran->tran_setcap = atapi_tran_setcap;
	tran->tran_init_pkt = atapi_tran_init_pkt;
	tran->tran_destroy_pkt = atapi_tran_destroy_pkt;
	tran->tran_dmafree = atapi_tran_dmafree;
	tran->tran_sync_pkt = atapi_tran_sync_pkt;
	tran->tran_add_eventcall = NULL;
	tran->tran_bus_reset = NULL;
	tran->tran_get_bus_addr = NULL;
	tran->tran_get_eventcookie = NULL;
	tran->tran_get_name = NULL;
	tran->tran_post_event = NULL;
	tran->tran_quiesce = NULL;
	tran->tran_remove_eventcall = NULL;
	tran->tran_unquiesce = NULL;

	if (scsi_hba_attach_setup(ata_ctlp->ac_dip, &ata_dma_attrs, tran,
				SCSI_HBA_TRAN_CLONE) != DDI_SUCCESS) {
		ADBG_WARN(("atapi_init: scsi_hba_attach failed\n"));
		goto errout;
	}

	ata_ctlp->ac_flags |= AC_SCSI_HBA_ATTACH;
	/*
	 * Set up the dma mode for the drive
	 */
	if (!atapi_work_pio) {
		for (targ = 0; targ < ATA_MAXTARG; targ++) {
			ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
			if (ata_drvp == NULL) {
				continue;
			}
			if (ata_drvp->ad_flags & AD_DISK) {
				continue;
			}
			ad_id = &(ata_drvp->ad_id);
			/*
			 * Check if the drive supports the dma mode
			 */
			mode = 0;
			if (ad_id->dcd_dworddma & 0x7) {
				if (ad_id->dcd_dworddma &  DMA_MODE2) {
					mode = DCD_MULT_DMA_MODE2;
				} else if (ad_id->dcd_dworddma & DMA_MODE1) {
					mode = DCD_MULT_DMA_MODE1;
				} else {
					mode  = DCD_MULT_DMA_MODE0;
				}
				mode |= ENABLE_DMA_FEATURE;
				atapi_work_pio = 0;
				ata_drvp->ad_dmamode = mode;
				ata_drvp->ad_piomode = 0x7f;
				ata_drvp->ad_cur_disk_mode = DMA_MODE;
			} else {
				/*
				 * check pio mode
				 */
				if ((ad_id->dcd_advpiomode & PIO_MODE4_MASK)
					== 0x2) {
					mode = 4;
				} else if ((ad_id->dcd_advpiomode &
					PIO_MODE3_MASK) == 0x1) {
					mode = 3;
				} else {
					mode = 2;
				}
				mode |= ENABLE_PIO_FEATURE;
				atapi_work_pio = 1;
				ata_drvp->ad_piomode = mode;
				ata_drvp->ad_dmamode = 0x7f;
				ata_drvp->ad_cur_disk_mode = PIO_MODE;
			}
			ata_write_config(ata_drvp);
			if (ata_set_feature(ata_drvp, ATA_FEATURE_SET_MODE,
				mode) != SUCCESS) {
				return (FAILURE);
			}
		}
	} else {
		for (targ = 0; targ < ATA_MAXTARG; targ++) {
			ata_drvp = CTL2DRV(ata_ctlp, targ, 0);
			if ((ata_drvp == NULL) ||
				(ata_drvp->ad_flags & AD_DISK))
				continue;
			mode = 0;
			mode |= ENABLE_PIO_FEATURE;
			ata_drvp->ad_piomode = mode;
			ata_drvp->ad_dmamode = 0x7f;
			ata_drvp->ad_cur_disk_mode = PIO_MODE;
			ata_write_config(ata_drvp);
			if (ata_set_feature(ata_drvp, ATA_FEATURE_SET_MODE,
				mode) != SUCCESS) {
				return (FAILURE);
			}
		}
	}
	return (SUCCESS);

errout:
	atapi_destroy(ata_ctlp);
	return (FAILURE);
}


/*
 * destroy the atapi sub-system
 */
void
atapi_destroy(struct ata_controller *ata_ctlp)
{
	ADBG_TRACE(("atapi_destroy entered\n"));

	if (ata_ctlp->ac_flags & AC_SCSI_HBA_ATTACH) {
		(void) scsi_hba_detach(ata_ctlp->ac_dip);
	}

	if (ata_ctlp->ac_flags & AC_SCSI_HBA_TRAN_ALLOC) {
		scsi_hba_tran_free(ata_ctlp->ac_atapi_tran);
	}
}

/*
 * initialize an atapi drive
 */
int
atapi_init_drive(struct ata_drive *ata_drvp)
{
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	int chno = ata_drvp->ad_channel;

	ADBG_TRACE(("atapi_init_drive entered\n"));

	/*
	 * Determine ATAPI CDB size
	 */

	switch (ata_drvp->ad_id.dcd_config & ATAPI_ID_CFG_PKT_SZ) {

		case ATAPI_ID_CFG_PKT_12B:
			ata_drvp->ad_cdb_len = 12;
			break;
		case ATAPI_ID_CFG_PKT_16B:
			ata_drvp->ad_cdb_len = 16;
			break;
		default:
			ADBG_WARN(("atapi_init_drive: bad pkt size support\n"));
			return (FAILURE);
	}

	/*
	 * determine if drive gives
	 * an intr when it wants the CDB
	 */

	if ((ata_drvp->ad_id.dcd_config & ATAPI_ID_CFG_DRQ_TYPE) !=
			ATAPI_ID_CFG_DRQ_INTR) {
		ata_drvp->ad_flags |= AD_NO_CDB_INTR;
	}

#ifndef ATAPI_NO_OVERLAP
	/*
	 * Check for ATAPI overlap support
	 */
	if (ata_drvp->ad_id.dcd_cap & ATAPI_ID_CAP_OVERLAP) {

		/*
		 * Try to enable interrupts for Release after receipt of
		 * overlap command and completion of Service command
		 */

		if ((ata_set_feature(ata_drvp, ATAPI_FEAT_RELEASE_INTR, 0) !=
		SUCCESS) || (ata_set_feature(ata_drvp, ATAPI_FEAT_SERVICE_INTR,
		0) != SUCCESS)) {
			printf("WARN\n");
			ADBG_WARN(("atapi_init_drive: failed to enable"
					"ATAPI overlap\n"));
		} else {
			ADBG_INIT(("atapi_init_drive: targ %d lun %d is "
					"supports ATAPI overlap\n",
					ata_drvp->ad_targ, ata_drvp->ad_lun));
			ata_drvp->ad_flags |= AD_ATAPI_OVERLAP;
		}
	}

#ifdef DSC_OVERLAP_SUPPORT
	else {
		u_int drive_type;

		/*
		 * Tape drives which don't support ATAPI overlap must
		 * support "legacy" DSC overlap.
		 */

		drive_type = (u_int)(ata_drvp->ad_id.dcd_config &
			ATAPI_ID_CFG_DEV_TYPE) >> ATAPI_ID_CFG_DEV_SHFT;

		if (drive_type == DTYPE_SEQUENTIAL) {

			ADBG_INIT(("atapi_init_drive: targ %d lun %d is "
					"a LEGACY (DSC overlap) tape drive\n",
					ata_drvp->ad_targ, ata_drvp->ad_lun));

			ata_drvp->ad_flags |= AD_DSC_OVERLAP;
		}
	}
#endif
#endif	/* ATAPI_NO_OVERLAP */

	if (!atapi_work_pio) {
		if (!ata_ctlp->ata_prd_acc_handle[chno])
			return (prd_init(ata_ctlp, chno));
	}
	return (SUCCESS);
}

/*
 * destroy an atapi drive
 */
/* ARGSUSED */
void
atapi_destroy_drive(struct ata_drive *ata_drvp)
{
#ifdef DSC_OVERLAP_SUPPORT
	struct scsi_pkt *spktp;
#endif
	ADBG_TRACE(("atapi_destroy_drive entered\n"));

#ifdef DSC_OVERLAP_SUPPORT
	/* Destroy special TUR packet for DSC overlap */

	if (ata_drvp->ad_tur_pkt != NULL) {

		spktp = APKT2SPKT(ata_drvp->ad_tur_pkt);

		atapi_tran_destroy_pkt(&spktp->pkt_address, spktp);

		ata_drvp->ad_tur_pkt = NULL;
	}
#endif
}

/*
 * ATAPI Identify Device command
 */
int
atapi_id(ddi_acc_handle_t handle, uint8_t *ioaddr, ushort *buf)
{
	int i;
	ADBG_TRACE(("atapi_id entered\n"));

	if (ata_wait(handle, ioaddr + AT_STATUS, 0, ATS_BSY, 100, 2)) {
		/*
		 * Will not issue the command in case the BSY is high
		 */
		return (FAILURE);
	}

	ddi_put8(handle, ioaddr + AT_CMD, ATC_PI_ID_DEV);
	/*
	 * As per the atapi specs, we need to wait for 200ms after ATAPI IDENT
	 * command has been issued to the drive. Originally the wait time in
	 * the code was 100ms. There is a bug with the LGE Goldstar 32X cdrom
	 * drives as a result of which they do not respond within 200ms and
	 * time taken to respond can be in tune of seconds too. As a workaround
	 * to that am providing longer wait period in code so the drive gets
	 * recoganised.
	 */
	for (i = 0; i < 200; i++) {
		if (!ata_wait(handle, ioaddr + AT_STATUS, ATS_DRQ,
			ATS_BSY, 10, 10000)) {
			break;
		}
	}

	if (atapi_id_timewarn) {
		if (i >= 2) {
			cmn_err(CE_WARN, "Atapi specification violation:\
			\n Response time to ATAPI IDENTIFY command was \
			greater than 200ms\n");
		}
	}
	if (i >= 200) {
		return (FAILURE);
	}

	drv_usecwait(10000);
	ddi_rep_get16(handle, buf,
		(uint16_t *)(ioaddr + AT_DATA), NBPSCTR >> 1, 0);
	change_endian((u_char *)buf, NBPSCTR);

	/*
	 * wait for the drive to recognize I've read all the data.  some
	 * drives have been observed to take as much as 3msec to finish
	 * sending the data; allow 5 msec just in case.
	 */

	if (ata_wait(handle, ioaddr + AT_STATUS, ATS_DRDY,
		ATS_BSY | ATS_DRQ, 10, 500)) {
		ADBG_WARN(("atapi_id: no DRDY\n"));
		return (FAILURE);
	}

	/*
	 * check for error
	 */

	if (ddi_get8(handle, ioaddr + AT_STATUS) & ATS_ERR) {
		ADBG_WARN(("atapi_id: ERROR status\n"));
		return (FAILURE);
	}

	return (SUCCESS);
}

/*
 * Look for atapi signature
 */
int
atapi_signature(ddi_acc_handle_t handle, uint8_t *ioaddr)
{
	ADBG_TRACE(("atapi_signature entered\n"));

	if ((ddi_get8(handle, ioaddr + AT_HCYL) == ATAPI_SIG_HI) &&
		(ddi_get8(handle, ioaddr + AT_LCYL) == ATAPI_SIG_LO)) {
		return (SUCCESS);
	}

	return (FAILURE);
}

/*
 * reset an atapi device
 */
int
atapi_reset_drive(struct ata_drive *ata_drvp)
{
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	struct ata_pkt *ata_pktp;
	int chno = ata_drvp->ad_channel;

	/*
	 * XXX - what should we do when controller is busy
	 * with I/O for another device?  For now, do nothing.
	 */

	ata_pktp = ata_ctlp->ac_active[chno];

	if ((ata_pktp != NULL) && (APKT2DRV(ata_pktp) != ata_drvp)) {
		return (FAILURE);
	}

	/*
	 * issue atapi soft reset
	 */

	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_cmd[chno], ATC_PI_SRESET);
	drv_usecwait(3*1024*1024);

	/*
	 * Clean up any active packet on this drive
	 */

	if (ata_pktp != NULL) {
		ata_ctlp->ac_active[chno] = NULL;
		ata_pktp->ap_flags |= AP_DEV_RESET;
		ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
	}

#ifndef ATAPI_NO_OVERLAP
	/*
	 * Clean up any overlap packet on this drive
	 */
	ata_pktp = ata_ctlp->ac_overlap[chno];

	if ((ata_pktp != NULL) && (APKT2DRV(ata_pktp) == ata_drvp)) {
		ata_ctlp->ac_overlap[chno] = NULL;
		ata_pktp->ap_flags |= AP_DEV_RESET;
		ata_ghd_complete_wraper(ata_ctlp, ata_pktp, chno);
	}
#endif /* ATAPI_NO_OVERLAP */

	return (SUCCESS);
}


/*
 * SCSA tran_tgt_init entry point
 */
/* ARGSUSED */
static int
atapi_tran_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
	scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	gtgt_t  *gtgtp;	/* GHD's per-target-instance structure */
	struct ata_controller *ata_ctlp;
	struct ata_drive *ata_drvp;
	struct scsi_address *ap;
	int rc = DDI_SUCCESS, chno;

	ADBG_TRACE(("atapi_tran_tgt_init entered\n"));

	/*
	 * Qualification of targ, lun, and ATAPI device presence
	 *  have already been taken care of by ata_bus_ctl
	 */

	/*
	 * store pointer to drive
	 * struct in cloned tran struct
	 */

	ata_ctlp = TRAN2CTL(hba_tran);
	ap = &sd->sd_address;

	chno = SADR2CHNO(ap);

	ata_drvp = CTL2DRV(ata_ctlp, ap->a_target, ap->a_lun);

	gtgtp = ghd_target_init(hba_dip, tgt_dip, &ata_ctlp->ac_ccc[chno], 0,
		ata_ctlp, (uint32_t)ap->a_target, (uint32_t)ap->a_lun);

	hba_tran->tran_tgt_private = gtgtp;
	ata_drvp->ad_gtgtp = gtgtp;

	GTGTP2TARGET(gtgtp) = ata_drvp;

#ifdef	DSC_OVERLAP_SUPPORT

	/*
	 * create special TUR packet.
	 * This is done here (rather than in atapi_init_drive)
	 * since we need a valid scsi_address pointer
	 */

	if (ata_drvp->ad_flags & AD_DSC_OVERLAP) {
		if (atapi_dsc_init(ata_drvp, ap) != SUCCESS) {
			rc = DDI_FAILURE;
		}
	}
#endif

	return (rc);
}

/*
 * SCSA tran_tgt_probe entry point
 */
static int
atapi_tran_tgt_probe(struct scsi_device *sd, int (*callback)())
{
	ADBG_TRACE(("atapi_tran_tgt_probe entered\n"));

	return (scsi_hba_probe(sd, callback));
}

/*
 * SCSA tran_tgt_free entry point
 */
/* ARGSUSED */
static void
atapi_tran_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
	scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	int chno;
	struct scsi_address *ap = &sd->sd_address;
	struct ata_controller *ata_ctlp = ADDR2CTLP(ap);
	struct ata_drive *ata_drvp =
			CTL2DRV(ata_ctlp, ap->a_target, ap->a_lun);

	ADBG_TRACE(("atapi_tran_tgt_free entered\n"));

	chno = SADR2CHNO(&sd->sd_address);

	ghd_target_free(hba_dip, tgt_dip,
		&TRAN2ATAP(hba_tran)->ac_ccc[chno], ata_drvp->ad_gtgtp);
	hba_tran->tran_tgt_private = NULL;
}

/*
 * SCSA tran_abort entry point
 */
/* ARGSUSED */
static int
atapi_tran_abort(struct scsi_address *ap, struct scsi_pkt *spktp)
{
	int chno;
	struct ata_controller *ata_ctlp = ADDR2CTLP(ap);
	struct ata_drive *ata_drvp =
			CTL2DRV(ata_ctlp, ap->a_target, ap->a_lun);

	ADBG_TRACE(("atapi_tran_abort entered\n"));

	chno = SADR2CHNO(ap);

	if (spktp) {
		return (ghd_tran_abort(&ADDR2CTL(ap)->ac_ccc[chno],
				PKTP2GCMDP(spktp),
				ata_drvp->ad_gtgtp, NULL));
	}

	return (ghd_tran_abort_lun(&ADDR2CTL(ap)->ac_ccc[chno],
			ata_drvp->ad_gtgtp, NULL));
}

/*
 * SCSA tran_reset entry point
 */
/* ARGSUSED */
static int
atapi_tran_reset(struct scsi_address *ap, int level)
{
	int chno;
	struct ata_controller *ata_ctlp = ADDR2CTLP(ap);
	struct ata_drive *ata_drvp =
			CTL2DRV(ata_ctlp, ap->a_target, ap->a_lun);

	ADBG_TRACE(("atapi_tran_reset entered\n"));

	chno = SADR2CHNO(ap);

	if (level == RESET_TARGET) {
		return (ghd_tran_reset_target(&ADDR2CTL(ap)->ac_ccc[chno],
				ata_drvp->ad_gtgtp, NULL));
	}
	if (level == RESET_ALL) {
		return (ghd_tran_reset_bus(&ADDR2CTL(ap)->ac_ccc[chno],
				ata_drvp->ad_gtgtp, NULL));
	}
	return (FALSE);

}

/*
 * SCSA tran_setcap entry point
 */
/* ARGSUSED */
static int
atapi_tran_setcap(struct scsi_address *ap, char *capstr, int value, int whom)
{
	ADBG_TRACE(("atapi_tran_setcap entered\n"));

	/* we have no settable capabilities */
	return (0);
}

/*
 * SCSA tran_getcap entry point
 */
/* ARGSUSED0 */
static int
atapi_tran_getcap(struct scsi_address *ap, char *capstr, int whom)
{
	int rval = -1;

	ADBG_TRACE(("atapi_tran_getcap entered\n"));

	if (capstr == NULL || whom == 0) {
		return (-1);
	}
	switch (scsi_hba_lookup_capstr(capstr)) {
		case SCSI_CAP_INITIATOR_ID:
			rval = 7;
			break;

		case SCSI_CAP_DMA_MAX:
			/* XXX - what should the real limit be?? */
			rval = 1 << 24;	/* limit to 16 megabytes */
			break;

		case SCSI_CAP_GEOMETRY:
			/* Default geometry */
			rval = ATAPI_HEADS << 16 | ATAPI_SECTORS_PER_TRK;
			break;
	}

	return (rval);
}


/*
 * SCSA tran_init_pkt entry point
 */
/* ARGSUSED6 */
static struct scsi_pkt *
atapi_tran_init_pkt(struct scsi_address *ap, struct scsi_pkt *spktp,
	struct buf *bp, int cmdlen, int statuslen, int tgtlen,
	int flags, int (*callback)(caddr_t), caddr_t arg)
{
	struct ata_controller *ata_ctlp = ADDR2CTL(ap);
	struct ata_pkt *ata_pktp;
	struct ata_drive *ata_drvp;
	gcmd_t *gcmdp;
	int bytes;

	ADBG_TRACE(("atapi_tran_init_pkt entered\n"));

	if (spktp == NULL) {
		spktp = scsi_hba_pkt_alloc(ata_ctlp->ac_dip, ap,
				cmdlen, statuslen, tgtlen,
				sizeof (gcmd_t), callback, arg);
		if (spktp == NULL) {
			return (NULL);
		}

		ata_pktp = (struct ata_pkt *)
			kmem_zalloc(sizeof (struct ata_pkt),
			(callback == SLEEP_FUNC)? KM_SLEEP: KM_NOSLEEP);

		if (ata_pktp == NULL) {
			scsi_hba_pkt_free(ap, spktp);
			return (NULL);
		}

		gcmdp = APKT2GCMD(ata_pktp);

		ata_drvp = CTL2DRV(ata_ctlp, ap->a_target, ap->a_lun);

		GHD_GCMD_INIT(gcmdp, (void *)ata_pktp, ata_drvp->ad_gtgtp);

		spktp->pkt_ha_private = (void *)gcmdp;
		gcmdp->cmd_pktp = (void *)spktp;

		/*
		 * save length of the SCSI CDB, and calculate CDB padding
		 * note that for convenience, padding is expressed in shorts.
		 */

		ata_pktp->ap_cdb_len = (u_char)cmdlen;
		ata_pktp->ap_cdb_pad =
			((unsigned)(ata_drvp->ad_cdb_len - cmdlen)) >> 1;

		/*
		 * set up callback functions
		 */

		ata_pktp->ap_start = atapi_start;
		ata_pktp->ap_intr = atapi_intr;
		ata_pktp->ap_complete = atapi_complete;

		/*
		 * set-up for start
		 */

		ata_pktp->ap_flags = AP_ATAPI;
		ata_pktp->ap_cmd = ATC_PI_PKT;
		ata_pktp->ap_hd = ata_drvp->ad_drive_bits;
		ata_pktp->ap_chno = ata_drvp->ad_channel;
		ata_pktp->ap_targ = ata_drvp->ad_targ;
		ata_pktp->ap_bytes_per_block = ata_drvp->ad_bytes_per_block;

		if ((bp) && (bp->b_bcount)) {
			if (!atapi_work_pio) {
				int	dma_flags;
				/*
				 * Set up dma info if there's any data and
				 * if the device supports DMA.
				 */
				if (bp->b_flags & B_WRITE) {
					dma_flags = DDI_DMA_WRITE;
					ata_pktp->ap_flags |= AP_WRITE | AP_DMA;
				} else {
					dma_flags = DDI_DMA_READ;
					ata_pktp->ap_flags |= AP_READ | AP_DMA;
				}
				/*
				 * check dma option flags
				 */
				if (flags & PKT_CONSISTENT) {
					dma_flags |= DDI_DMA_CONSISTENT;
				}
				if (flags & PKT_DMA_PARTIAL) {
					dma_flags |= DDI_DMA_PARTIAL;
				}
				/*
				 * map the buffer
				 */
				if (ghd_dmaget(ata_ctlp->ac_dip,
					gcmdp, bp, dma_flags,
					callback, arg, 0, make_prd) == NULL) {
					return (NULL);
				}
			}
			spktp->pkt_resid =
				APKT2GCMD(ata_pktp)->cmd_resid = bp->b_bcount;
			ata_pktp->ap_count_bytes = bp->b_bcount;
			ata_pktp->ap_v_addr = bp->b_un.b_addr;
			ata_pktp->ap_buf_addr = bp->b_un.b_addr;
		} else {
			spktp->pkt_resid =
				APKT2GCMD(ata_pktp)->cmd_resid = 0;
			ata_pktp->ap_count_bytes = 0;
		}

		bytes = min(ata_pktp->ap_gcmd.cmd_resid,
				ATAPI_MAX_BYTES_PER_DRQ);
		ata_pktp->ap_hicyl = (u_char)(bytes >> 8);
		ata_pktp->ap_lwcyl = (u_char)bytes;

		/*
		 * fill these with zeros
		 * for ATA/ATAPI-4 compatibility
		 */
		ata_pktp->ap_sec = 0;
		ata_pktp->ap_count = 0;
	} else {
		printf("atapi_tran_init_pkt called with pre allocated pkt\n");
	}

	return (spktp);
}

/*
 * SCSA tran_destroy_pkt entry point
 */
static void
atapi_tran_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *spktp)
{
	struct ata_pkt *ata_pktp = SPKT2APKT(spktp);

	ADBG_TRACE(("atapi_tran_destroy_pkt entered\n"));

	if (ata_pktp->ap_flags & AP_DMA) {
		/*
		 * release the old DMA resources
		 */
		ghd_dmafree(&ata_pktp->ap_gcmd);
	}

	/*
	 * bp_mapout happens automagically from biodone
	 */

	if (SPKT2APKT(spktp)) {
		kmem_free((caddr_t)SPKT2APKT(spktp), sizeof (struct ata_pkt));
	}

	scsi_hba_pkt_free(ap, spktp);

#ifdef	NOTNOW
	ghd_pktfree(&ADDR2CTL(ap)->ac_ccc[chno], ap, spktp);
#endif
}

/*ARGSUSED*/
static void
atapi_tran_dmafree(struct scsi_address *ap, struct scsi_pkt *spktp)
{
	ADBG_TRACE(("atapi_tran_dmafree entered\n"));

	/* bp_mapout happens automagically from biodone */
}

/* SCSA tran_sync_pkt entry point */

/*ARGSUSED*/
static void
atapi_tran_sync_pkt(struct scsi_address *ap, struct scsi_pkt *spktp)
{
	ADBG_TRACE(("atapi_tran_sync_pkt entered\n"));
}

/*
 * SCSA tran_start entry point
 */
/* ARGSUSED */
static int
atapi_tran_start(struct scsi_address *ap, struct scsi_pkt *spktp)
{
	struct ata_pkt *ata_pktp = SPKT2APKT(spktp);
	struct ata_drive *ata_drvp = APKT2DRV(ata_pktp);
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	int rc, polled = FALSE, chno, intr_status = 0;

	ADBG_TRACE(("atapi_tran_start entered\n"));

	if (ata_drvp->ad_invalid == 1) {
		return (TRAN_FATAL_ERROR);
	}

	/*
	 * Atapi driver can not handle odd count requests
	 */
	if (ata_pktp->ap_count_bytes % 2) {
		return (TRAN_BADPKT);
	}
	chno = SADR2CHNO(ap);

	/*
	 * basic initialization
	 */

	ata_pktp->ap_flags |= AP_ATAPI;
	spktp->pkt_state = 0;
	spktp->pkt_statistics = 0;


	/*
	 * check for polling pkt
	 */

	if (spktp->pkt_flags & FLAG_NOINTR) {
		ata_pktp->ap_flags |= AP_POLL;
		mutex_enter(&ata_ctlp->ac_hba_mutex);
		ata_ctlp->ac_polled_count++;
		mutex_exit(&ata_ctlp->ac_hba_mutex);
		polled = TRUE;
	} else {
		ata_pktp->ap_flags &= ~AP_POLL;
	}


	/*
	 * driver cannot accept tagged commands
	 */

	if (spktp->pkt_flags & (FLAG_HTAG|FLAG_OTAG|FLAG_STAG)) {
		spktp->pkt_reason = CMD_TRAN_ERR;
		return (TRAN_BADPKT);
	}

	/*
	 * call common transport routine
	 */

	rc = ghd_transport(&ata_ctlp->ac_ccc[chno], APKT2GCMD(ata_pktp),
			APKT2GCMD(ata_pktp)->cmd_gtgtp,
			spktp->pkt_time, polled, (void *) &intr_status);

	/*
	 * see if pkt was not accepted
	 */

	if (rc == TRAN_BUSY) {
		return (TRAN_BUSY);
	}

	/*
	 * for polled pkt, set up return status
	 */

	if (polled) {
		atapi_complete(ata_pktp, FALSE);
	}

	return (rc);
}

/*
 * packet start callback routine
 */
static int
atapi_start(struct ata_controller *ata_ctlp, struct ata_pkt *ata_pktp)
{
	struct ata_drive *ata_drvp = APKT2DRV(ata_pktp);
	int chno = ata_drvp->ad_channel;
	u_char  val;

	ADBG_TRACE(("atapi_start entered\n"));
	ADBG_TRANSPORT(("atapi_start:\tpkt = 0x%x, pkt flags = 0x%x\n",
		(u_long)ata_pktp, ata_pktp->ap_flags));
	ADBG_TRANSPORT(("atapi_start:\tcnt = 0x%x, addr = 0x%x\n",
		ata_pktp->ap_gcmd.cmd_resid, ata_pktp->ap_v_addr));

	/*
	 * Reinitialise the gcmd.resid as sd allocs pkt at beginning and
	 * re-uses them for request sense where as the resid is decremented
	 * depending on what is read as data from the devices. So a new
	 * field is being introduced to take the count so that it can be
	 * restored here in this place to reset the gcmd.resid properly.
	 * All other feilds like ap_status, ap_error and ap_flags also need
	 * to be reset to take care of reusability of pkts.
	 */
	if (ata_drvp->ad_invalid == 1) {
		ata_pktp->ap_flags &= ~(AP_TIMEOUT|AP_ABORT|
			AP_BUS_RESET|AP_ERROR);
		ata_pktp->ap_flags |= AP_FATAL;
		return (TRAN_FATAL_ERROR);
	}
	ata_pktp->ap_gcmd.cmd_resid = ata_pktp->ap_count_bytes;
	ata_pktp->ap_error = 0;
	ata_pktp->ap_status = 0;
	ata_pktp->ap_flags &= ~AP_ERROR;
	ata_pktp->ap_v_addr = ata_pktp->ap_buf_addr;

	/*
	 * Check for conflict with overlap commands already running
	 */
#ifdef DSC_OVERLAP_SUPPORT

	/*
	 * Allow DSC TUR packet to always go through
	 */

	if (ata_pktp == ata_drvp->ad_tur_pkt) {
		;
	} else
#endif
#ifndef ATAPI_NO_OVERLAP
	if (ata_ctlp->ac_overlap[chno] != NULL) {

#ifdef DSC_OVERLAP_SUPPORT

		/*
		 * For DSC_OVERLAP devices, we won't start an I/O if
		 * there's *any* overlap commands outstanding
		 */

		if (ata_drvp->ad_flags & AD_DSC_OVERLAP) {
			return (TRAN_BUSY);
		}
#endif

		/*
		 * For all types of overlap, make sure that this target
		 * isn't already running an overlap command
		 */

		if (APKT2DRV(ata_ctlp->ac_overlap[chno])->ad_targ ==
				ata_drvp->ad_targ) {
			return (TRAN_BUSY);
		}
	}
#endif /* ATAPI_NO_OVERLAP */

	/*
	 * check for busy before starting command.  This
	 * is important for laptops that do suspend/resume
	 */

	if (ata_wait(ata_ctlp->ata_datap1[chno], ata_ctlp->ac_altstatus[chno],
		0, ATS_BSY, 10, 500000)) {
		ADBG_WARN(("atapi_start: BSY too long!\n"));
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status =
				ddi_get8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_status[chno]);
		ata_pktp->ap_error =
				ddi_get8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_error[chno]);
		return (TRAN_FATAL_ERROR);
	}
	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_drvhd[chno], ata_pktp->ap_hd);
	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_lcyl[chno], ata_pktp->ap_lwcyl);
	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_hcyl[chno], ata_pktp->ap_hicyl);
	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_sect[chno], ata_pktp->ap_sec);
	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_count[chno], ata_pktp->ap_count);

#ifndef ATAPI_NO_OVERLAP
	/*
	 * Enable ATAPI overlap feature when:
	 *  1) device supports ATAPI overlap
	 *  2) there's no overlap command running
	 *  3) it's not a polling packet
	 */

	if ((ata_drvp->ad_flags & AD_ATAPI_OVERLAP) &&
	    (ata_ctlp->ac_overlap[chno] == NULL) &&
	    (!(ata_pktp->ap_flags & AP_POLL))) {

		/*
		 * Enable ATAPI overlap
		 */

		ata_pktp->ap_flags |= AP_ATAPI_OVERLAP;
		ddi_put8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_feature[chno], ATF_OVERLAP);
	} else
#endif /* ATAPI_NO_OVERLAP */

	/*
	 * enable or disable interrupts for command
	 */

	if (ata_pktp->ap_flags & AP_POLL) {
		if (!atapi_work_pio) {
			val = ddi_get8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 1)) & 0x33;
			if (chno == 0) {
				val |= 0x10;
			} else {
				val |= 0x20;
			}
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 1), val);
		} else {
			ddi_put8(ata_ctlp->ata_datap1[chno],
				ata_ctlp->ac_devctl[chno], ATDC_D3|ATDC_NIEN);
		}
	} else {
		ddi_put8(ata_ctlp->ata_datap1[chno],
			ata_ctlp->ac_devctl[chno], ATDC_D3);
	}

	if ((ata_drvp->ad_cur_disk_mode == DMA_MODE) &&
		(ata_pktp->ap_flags & AP_DMA)) {
		ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_feature[chno], 1);
		write_prd(ata_ctlp, ata_pktp);
		if (ata_pktp->ap_flags & AP_READ) {
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno), 8);
		} else {
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno), 0);
		}
	} else {
		ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_feature[chno], 0);
	}
	/*
	 * This next one sets the controller in motion
	 */

	ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_cmd[chno], ata_pktp->ap_cmd);

	/*
	 * If  we don't receive an interrupt requesting the scsi CDB,
	 * we must poll for DRQ, and then send out the CDB.
	 */

	if (ata_drvp->ad_flags & AD_NO_CDB_INTR) {

		if (ata_wait(ata_ctlp->ata_datap1[chno],
			ata_ctlp->ac_altstatus[chno], ATS_DRQ, ATS_BSY, 10,
			400000)) {
			ata_pktp->ap_flags |= AP_TRAN_ERROR;
			ata_pktp->ap_status = ddi_get8
				(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_status[chno]);
			ata_pktp->ap_error =
				ddi_get8(ata_ctlp->ata_datap[chno],
					ata_ctlp->ac_error[chno]);
			ADBG_WARN(("atapi_start: no DRQ\n"));
			return (TRAN_FATAL_ERROR);
		}

		if (atapi_send_cdb(ata_ctlp, chno) != SUCCESS) {
			return (TRAN_FATAL_ERROR);
		}
		if ((ata_drvp->ad_cur_disk_mode == DMA_MODE) &&
			(ata_pktp->ap_flags & AP_DMA)) {
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno),
				(ddi_get8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno))
				| 0x01));
		}

	}

	return (TRAN_ACCEPT);
}

/*
 * packet complete callback
 */

static void
atapi_complete(struct ata_pkt *ata_pktp, int do_callback)
{
	struct scsi_pkt *spktp = APKT2SPKT(ata_pktp);
	struct scsi_status *scsi_stat = (struct scsi_status *)spktp->pkt_scbp;

	ADBG_TRACE(("atapi_complete entered\n"));
	ADBG_TRANSPORT(("atapi_complete: pkt = 0x%x\n", (u_long)ata_pktp));

	/*
	 * update resid
	 */

	spktp->pkt_resid = ata_pktp->ap_gcmd.cmd_resid;

	/*
	 * check for fatal errors
	 */

	if (ata_pktp->ap_flags & AP_FATAL) {
		spktp->pkt_reason = CMD_TRAN_ERR;
	} else if (ata_pktp->ap_flags & AP_TRAN_ERROR) {
		spktp->pkt_reason = CMD_TRAN_ERR;
	} else if (ata_pktp->ap_flags & AP_BUS_RESET) {
		spktp->pkt_reason = CMD_RESET;
		spktp->pkt_statistics |= STAT_BUS_RESET;
	} else if (ata_pktp->ap_flags & AP_DEV_RESET) {
		spktp->pkt_reason = CMD_RESET;
		spktp->pkt_statistics |= STAT_DEV_RESET;
	} else if (ata_pktp->ap_flags & AP_ABORT) {
		spktp->pkt_reason = CMD_ABORTED;
		spktp->pkt_statistics |= STAT_ABORTED;
	} else if (ata_pktp->ap_flags & AP_TIMEOUT) {
		spktp->pkt_reason = CMD_TIMEOUT;
		spktp->pkt_statistics |= STAT_TIMEOUT;
	} else {
		spktp->pkt_reason = CMD_CMPLT;
	}

	/*
	 * non-fatal errors
	 */

	if (ata_pktp->ap_flags & AP_ERROR) {
		scsi_stat->sts_chk = 1;
	} else {
		scsi_stat->sts_chk = 0;
	}

	ADBG_TRANSPORT(("atapi_complete: reason = 0x%x stats = 0x%x "
			"sts_chk = %d\n", spktp->pkt_reason,
			spktp->pkt_statistics, scsi_stat->sts_chk));

	if (do_callback && (spktp->pkt_comp)) {
		(*spktp->pkt_comp)(spktp);
	}
}


/*
 * packet "process interrupt" callback
 *
 * returns STATUS_PKT_DONE when a packet has completed.
 * returns STATUS_PARTIAL when an event occured but no packet completed
 * returns STATUS_NOINTR when there was no interrupt status
 */
static int
atapi_intr(struct ata_controller *ata_ctlp, struct ata_pkt *ata_pktp)
{
	struct ata_drive *ata_drvp = APKT2DRV(ata_pktp);
	struct scsi_pkt	*spktp;
	int	drive_bytes, data_bytes;
	u_char	status, intr, val;
	int chno = ata_drvp->ad_channel;


	ADBG_TRACE(("atapi_intr entered\n"));
	ADBG_TRANSPORT(("atapi_intr: pkt = 0x%x\n", (u_long)ata_pktp));

	/*
	 * this clears the interrupt
	 */

	status = ddi_get8(ata_ctlp->ata_datap[chno], ata_ctlp->ac_status[chno]);

	if (!atapi_work_pio) {
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
	}

	/*
	 * if busy, can't be our interrupt
	 */

	if (status & ATS_BSY) {
		return (STATUS_NOINTR);
	}

	spktp = APKT2SPKT(ata_pktp);

	intr = ddi_get8(ata_ctlp->ata_datap[chno], ata_ctlp->ac_count[chno]);

	/*
	 * The atapi interrupt reason register (ata count)
	 * RELEASE/CoD/IO bits and status register SERVICE/DRQ bits
	 * define the state of the atapi packet command.
	 *
	 * If RELEASE is 1, then the device has released the
	 * bus before completing the command in progress (overlap)
	 *
	 * If SERVICE is 1, then the device has completed an
	 * overlap command and needs to be issued the SERVICE command
	 *
	 * Otherwise, the interrupt reason can be interpreted
	 * from other bits as follows:
	 *
	 *  IO  DRQ  CoD
	 *  --  ---  ---
	 *   0    1   1  Ready for atapi (scsi) pkt
	 *   1    1   1  Future use
	 *   1    1   0  Data from device.
	 *   0    1   0  Data to device
	 *   1    0   1  Status ready
	 *
	 * There is a separate interrupt for each of phases.
	 */

#ifndef ATAPI_NO_OVERLAP
	/*
	 * check if interrupt is for an overlapped command
	 */

	if (ata_pktp == ata_ctlp->ac_overlap[chno]) {

		if (status & ATS_SERVICE) {

			/*
			 * Overlap packet is requesting the bus. Move back
			 * to the  active packet slot
			 */

			ata_ctlp->ac_active[chno] = ata_ctlp->ac_overlap[chno];
			ata_ctlp->ac_overlap[chno] = NULL;

			/*
			 * Issue service command
			 */

			ddi_put8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_cmd[chno], ATC_PI_SERVICE);

			return (STATUS_PARTIAL);
		}

		return (STATUS_NOINTR);
	}

	if (intr & ATI_RELEASE) {

		/*
		 * Active packet is releasing the bus. Move to the overlap
		 * packet slot
		 */

		if (ata_ctlp->ac_overlap[chno] != NULL) {
			ADBG_WARN(("atapi_intr: multiple overlap commands!\n"));
			goto errout;
		}

		ata_ctlp->ac_overlap[chno] = ata_ctlp->ac_active[chno];
		ata_ctlp->ac_active[chno] = NULL;

		return (STATUS_PARTIAL);
	}
#endif /* ATAPI_NO_OVERLAP */


	if (status & ATS_DRQ) {

		if ((intr & (ATI_COD | ATI_IO)) == ATI_COD) {

			/*
			 * send out atapi pkt
			 */

			if (atapi_send_cdb(ata_ctlp, chno) == FAILURE) {
				goto errout;
			} else {
				if ((ata_drvp->ad_cur_disk_mode == DMA_MODE) &&
					(ata_pktp->ap_flags & AP_DMA)) {
					ddi_put8(ata_ctlp->ata_cs_handle,
					(uchar_t *)(ata_ctlp->ata_cs_addr
					+ 8*chno),
					(ddi_get8(ata_ctlp->ata_cs_handle,
					(uchar_t *)(ata_ctlp->ata_cs_addr
					+ 8*chno)) | 0x01));
				}
				return (STATUS_PARTIAL);
			}
		}

		drive_bytes = (int)(ddi_get8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_hcyl[chno]) << 8) +
			ddi_get8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_lcyl[chno]);

		ASSERT(!(drive_bytes & 1)); /* even bytes */

		if ((intr & (ATI_COD | ATI_IO)) == ATI_IO) {

			/*
			 * Data from device
			 */

			data_bytes = min(ata_pktp->ap_gcmd.cmd_resid,
						drive_bytes);

			if (data_bytes) {
				ADBG_TRANSPORT(("atapi_intr: read "
					"0x%x bytes\n",
					data_bytes));
				ddi_rep_get16(ata_ctlp->ata_datap[chno],
					(ushort *)
					ata_pktp->ap_v_addr, (uint16_t *)
					ata_ctlp->ac_data[chno],  (data_bytes
						>> 1), 0);
				ata_pktp->ap_gcmd.cmd_resid -= data_bytes;
				ata_pktp->ap_v_addr += data_bytes;
				drive_bytes -= data_bytes;
			}

			if (drive_bytes) {
				ADBG_TRANSPORT(("atapi_intr: dump "
					"0x%x bytes\n",
					drive_bytes));
				ddi_rep_get16(ata_ctlp->ata_datap[chno],
					ata_bit_bucket, (uint16_t *)
					ata_ctlp->ac_data[chno],  (drive_bytes
						>> 1),
					0);
			}
			if (ata_wait(ata_ctlp->ata_datap1[chno],
				ata_ctlp->ac_altstatus[chno], 0, ATS_BSY, 10,
				500000)) {
				/* EMPTY */
				ADBG_WARN(("atapi_id: no DRDY\n"));
			}
			spktp->pkt_state |= STATE_XFERRED_DATA;
			return (STATUS_PARTIAL);
		}

		if ((intr & (ATI_COD | ATI_IO)) == 0) {

			/*
			 * Data to device
			 */

			ddi_rep_put16(ata_ctlp->ata_datap[chno],
			    (ushort *)ata_pktp->ap_v_addr,
			    (uint16_t *)ata_ctlp->ac_data[chno],
			    (drive_bytes >> 1), 0);
			ADBG_TRANSPORT(("atapi_intr: wrote 0x%x bytes\n",
				drive_bytes));
			ata_pktp->ap_v_addr += drive_bytes;
			ata_pktp->ap_gcmd.cmd_resid -= drive_bytes;

			spktp->pkt_state |= STATE_XFERRED_DATA;
			return (STATUS_PARTIAL);
		}

		if (!((intr & (ATI_COD | ATI_IO)) == (ATI_COD | ATI_IO))) {

			/*
			 * Unsupported intr combination
			 */

			return (STATUS_PARTIAL);
		}
	} else {
		if ((ata_drvp->ad_cur_disk_mode == DMA_MODE) &&
			(ata_pktp->ap_flags & AP_DMA)) {
			if ((intr & (ATI_COD | ATI_IO)) == (ATI_IO | ATI_COD)) {
				ata_pktp->ap_gcmd.cmd_resid = 0;
				spktp->pkt_state |= STATE_XFERRED_DATA;
				ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno),
				(ddi_get8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 8*chno))
				& 0xfe));
			}
		}
	}

#ifdef DSC_OVERLAP_SUPPORT
	/* check for DSC overlap command that is not really done */

	if ((ata_drvp->ad_flags & AD_DSC_OVERLAP) && (!(status & ATS_DSC))) {

		/*
		 * Legacy device has relased the bus. Move active command
		 * to overlap slot
		 */

		if (ata_ctlp->ac_overlap[chno] != NULL) {
			ADBG_WARN(("atapi_intr: multiple overlap commands!\n"));
			goto errout;
		}

		ata_ctlp->ac_overlap[chno] = ata_ctlp->ac_active[chno];
		ata_ctlp->ac_active[chno] = NULL;

		/*
		 * stop packet active timer
		 */

		ghd_timer_stop(&ata_ctlp->ac_ccc[chno],
			APKT2GCMD(ata_ctlp->ac_overlap[chno]));

		/*
		 * start polling
		 */

		atapi_dsc_poll(ata_drvp);

		return (STATUS_PARTIAL);
	}
#endif

	/* If we get here, a command has completed! */

	/*
	 * re-enable interrupts after a polling packet
	 */
	if (ata_pktp->ap_flags & AP_POLL) {
		if (!atapi_work_pio) {
			val = ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 1)) & 0x33;
			if (chno == 0) {
				val &= 0x23;
			} else {
				val &= 0x13;
			}
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 1), val);
		}
		ddi_put8(ata_ctlp->ata_datap1[chno],
				ata_ctlp->ac_devctl[chno], ATDC_D3);
	}

	/*
	 * check status of completed command
	 */
	if (status & ATS_ERR) {
		ata_pktp->ap_flags |= AP_ERROR;
		ata_pktp->ap_status = ddi_get8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_status[chno]);
		ata_pktp->ap_error = ddi_get8(ata_ctlp->ata_datap[chno],
				ata_ctlp->ac_error[chno]);
	}

	spktp->pkt_state |= STATE_GOT_STATUS;

#ifdef DSC_OVERLAP_SUPPORT
	/*
	 * Check if active packet that just completed is a DSC
	 * TUR packet.  If so, perform special cleanup.
	 */
	if (ata_pktp == ata_drvp->ad_tur_pkt) {
		atapi_dsc_complete(ata_drvp);
		return (STATUS_PARTIAL);
	}
#endif
#ifndef ATAPI_NO_OVERLAP
	/*
	 * if there's a background overlap command, then set
	 * the DRV/HD register to point to that device so that
	 * it can interrupt us later
	 */

	if (ata_ctlp->ac_overlap[chno] != NULL) {
		struct ata_drive *overlap_drvp =
			APKT2DRV(ata_ctlp->ac_overlap[chno]);

		ddi_put8(ata_ctlp->ata_datap[chno],
			ata_ctlp->ac_drvhd[chno], overlap_drvp->ad_drive_bits);

		/*
		 * immediately check for a service request.  This prevents
		 * possible starvation of overlapped device due to
		 * repeated commands to the foreground device
		 */

#ifdef DSC_OVERLAP_SUPPORT
		if (overlap_drvp->ad_flags & AD_DSC_OVERLAP) {

			/*
			 * stop poll timer
			 */
			ghd_timer_stop(&ata_ctlp->ac_ccc[chno],
				APKT2GCMD(ata_ctlp->ac_overlap[chno]));

			/*
			 * clear active slot early so that poll routine
			 * will work properly
			 */
			ata_ctlp->ac_active[chno] = NULL;

			/*
			 * call poll routine directly
			 */
			atapi_dsc_poll(overlap_drvp);
		} else
#endif
		{
			/*
			 * once back around to see if the SERVICE
			 * bit is set for the overlap packet
			 */
			(void) atapi_intr(ata_ctlp, ata_ctlp->ac_overlap[chno]);
		}
	}
#endif /* ATAPI_NO_OVERLAP */

	return (STATUS_PKT_DONE);

errout:
	ata_pktp->ap_flags |= AP_TRAN_ERROR;

	/*
	 * re-enable interrupts after a polling packet
	 */
	if (ata_pktp->ap_flags & AP_POLL) {
		ddi_put8(ata_ctlp->ata_datap1[chno],
			ata_ctlp->ac_devctl[chno], ATDC_D3);
		if (!atapi_work_pio) {
			val = ddi_get8(ata_ctlp->ata_cs_handle,
			(uchar_t *)(ata_ctlp->ata_cs_addr + 1)) & 0x33;
			if (chno == 0) {
				val &= 0x23;
			} else {
				val &= 0x13;
			}
			ddi_put8(ata_ctlp->ata_cs_handle,
				(uchar_t *)(ata_ctlp->ata_cs_addr + 1), val);
		}
	}


	return (STATUS_PKT_DONE);
}

#ifdef DSC_OVERLAP_SUPPORT

static int
atapi_dsc_init(struct ata_drive *ata_drvp, struct scsi_address *ap)
{
	struct scsi_pkt *spktp;

	ADBG_TRACE(("atapi_dsc_init entered\n"));

	/*
	 * Create special TUR packet for use with DSC overlap
	 *
	 * Notes:
	 *
	 * TUR packet may already exists since more than one
	 * target driver can call tran_tgt_init for a particular
	 * target/lun.
	 *
	 */

	if (ata_drvp->ad_tur_pkt != NULL) {
		return (SUCCESS);
	}

	spktp = atapi_tran_init_pkt(ap, NULL, NULL, CDB_GROUP0, 1, 0, 0,
			SLEEP_FUNC, NULL);

	if (spktp == NULL) {
		return (FAILURE);
	}

	bzero((caddr_t)spktp->pkt_cdbp, CDB_GROUP0);
	makecom_g0(spktp,
		(struct scsi_device *)&(spktp->pkt_address),
		spktp->pkt_flags, SCMD_TEST_UNIT_READY, 0, 0);

	ata_drvp->ad_tur_pkt = SPKT2APKT(spktp);

	return (SUCCESS);
}

void
atapi_dsc_poll(struct ata_drive *ata_drvp)
{
	int chno = ata_drvp->ad_channel;
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	struct ata_pkt *tur_pkt = ata_drvp->ad_tur_pkt;
	struct ata_pkt *overlap_pkt = ata_ctlp->ac_overlap[chno];
	struct scsi_pkt *overlap_spktp = APKT2SPKT(overlap_pkt);
	gcmd_t *overlap_gcmdp = APKT2GCMD(ata_ctlp->ac_overlap[chno]);
	u_char status, *cdb;

	ADBG_TRACE(("atapi_dsc_poll entered\n"));

	/*
	 * can't poll if there is an
	 * active command on another drive
	 */
	if (ata_ctlp->ac_active[chno] != NULL) {
		goto out;
	}

	/*
	 * set DRV/HD reg
	 */
	ddi_put8(ata_ctlp->ata_datap[chno],
		ata_ctlp->ac_drvhd[chno], ata_drvp->ad_drive_bits);

	/*
	 * check for DSC complete
	 */
	status = ddi_get8(ata_ctlp->ata_datap[chno], ata_ctlp->ac_status[chno]);

	if (status & ATS_DSC) {

		/*
		 * After UNLOAD command, TUR is expected to fail so
		 * we don't bother to send one.
		 */
		cdb = overlap_spktp->pkt_cdbp;

		if ((cdb[0] == SCMD_LOAD) && (!(cdb[4] & 0x1))) {
			overlap_spktp->pkt_state |= STATE_GOT_STATUS;
			ata_ctlp->ac_overlap[chno] = NULL;
			ata_ghd_complete_wraper(ata_ctlp, overlap_pkt, chno);
		}

		/*
		 * For all other cmds, send TUR packet to get status
		 */
		ata_ctlp->ac_active[chno] = tur_pkt;

		if (atapi_start(tur_pkt) != TRAN_ACCEPT) {

			/*
			 * If we fail to start TUR pkt, then we treat
			 * the whole thing as a transport error
			 */
			ata_ctlp->ac_active[chno] = NULL;

			ata_ctlp->ac_overlap[chno]->ap_flags |= AP_TRAN_ERROR;
			ata_ctlp->ac_overlap[chno] = NULL;
			ata_ghd_complete_wraper(ata_ctlp, overlap_pkt, chno);
		}

		return;
	}

out:

	/*
	 * Note that for packets in GCMD_STATE_POLL, the timeout value
	 * passed to ghd_timer_start is in ticks rather than seconds.
	 * We are setting the poll timer to go off in 1 tick.  This
	 * guarantees that GHD will call us back the next time it checks
	 * its timeout queue.  The rate at which GHD checks its timeout
	 * queue is controlled by ata_watchdog_usec, which is currently
	 * set to 100 ms.  For ATAPI tape, it's important that we poll
	 * DSC commands at least every 100 ms so that we can keep the
	 * tape streaming.
	 *
	 * XXX - we should really use a timestamp so that we only
	 * poll as long as we would have originally waited for
	 * the packet to finish.  As currently implemented, we
	 * can poll forever for DSC overlap commands to finish.
	 */

	overlap_gcmdp->cmd_state = GCMD_STATE_POLL;
	ghd_timer_start(&ata_ctlp->ac_ccc[chno], overlap_gcmdp, 1);
}

static void
atapi_dsc_complete(struct ata_drive *ata_drvp)
{
	int chno = ata_drvp->ad_channel;
	struct ata_controller *ata_ctlp = ata_drvp->ad_ctlp;
	struct ata_pkt *tur_pkt = ata_drvp->ad_tur_pkt;
	struct ata_pkt *overlap_pkt = ata_ctlp->ac_overlap[chno];
	struct scsi_pkt *overlap_spktp = APKT2SPKT(overlap_pkt);

	ADBG_TRACE(("atapi_dsc_complete entered\n"));

	/*
	 * This routine is called when the TUR packet following a
	 * DSC overlap command has completed
	 */

	/*
	 * Copy status from TUR packet to overlap packet
	 */
	if (tur_pkt->ap_flags & AP_ERROR) {
		overlap_pkt->ap_flags |= AP_ERROR;
		overlap_pkt->ap_status = tur_pkt->ap_status;
		overlap_pkt->ap_error = tur_pkt->ap_error;
	}

	overlap_spktp->pkt_state |= STATE_GOT_STATUS;

	/*
	 * clear active and overlap slot
	 */
	ata_ctlp->ac_active[chno] = NULL;
	ata_ctlp->ac_overlap[chno] = NULL;

	/*
	 * call ghd_complete for overlap packet
	 */
	ata_ghd_complete_wraper(ata_ctlp, overlap_pkt, chno);
}

#endif


/*
 * Send a SCSI CDB to ATAPI device
 */
static int
atapi_send_cdb(struct ata_controller *ata_ctlp, int chno)
{
	struct ata_pkt *ata_pktp = ata_ctlp->ac_active[chno];
	struct scsi_pkt *spktp = APKT2SPKT(ata_pktp);
	int padding;
	char my_cdb[10];

	ADBG_TRACE(("atapi_send_cdb entered\n"));

	if (spktp->pkt_cdbp[0] == 0x03) {
		spktp->pkt_cdbp[4] = 0x12;
	}

	if (spktp->pkt_cdbp[0] == 0x08 && ata_pktp->ap_cdb_len == 6) {
		ata_pktp->ap_cdb_pad -= 2;
		my_cdb[0] = 0x28;
		my_cdb[1] = (unsigned char)(spktp->pkt_cdbp[1] & 0xf0) >> 4;
		my_cdb[2] = 0;
		my_cdb[3] = spktp->pkt_cdbp[1] & 0x0f;
		my_cdb[4] = spktp->pkt_cdbp[2];
		my_cdb[5] = spktp->pkt_cdbp[3];
		my_cdb[6] = 0;
		my_cdb[7] = 0;
		my_cdb[8] = spktp->pkt_cdbp[4];
		if (my_cdb[8] == 0)
			my_cdb[8] = 1;
		my_cdb[9] = 0;

		ddi_rep_put16(ata_ctlp->ata_datap[chno],
			(ushort *)(&my_cdb[0]),
			(uint16_t *)ata_ctlp->ac_data[chno],  5, 0);
	} else {
		ddi_rep_put16(ata_ctlp->ata_datap[chno],
			(ushort *)spktp->pkt_cdbp,
			(uint16_t *)ata_ctlp->ac_data[chno],
			ata_pktp->ap_cdb_len >> 1, 0);
	}

	/*
	 * pad to ad_cdb_len bytes
	 */
	padding = ata_pktp->ap_cdb_pad;

	while (padding) {
		ddi_put16(ata_ctlp->ata_datap[chno], (uint16_t *)
			ata_ctlp->ac_data[chno],
				0);
		padding--;
	}

#ifdef ATA_DEBUG
	{
		/* LINTED */
		unsigned char *cp = (unsigned char *)spktp->pkt_cdbp;

		ADBG_TRANSPORT(("\tatapi scsi cmd (%d bytes):\n ",
				ata_pktp->ap_cdb_len));
		ADBG_TRANSPORT(("\t\t 0x%x 0x%x 0x%x 0x%x\n",
			cp[0], cp[1], cp[2], cp[3]));
		ADBG_TRANSPORT(("\t\t 0x%x 0x%x 0x%x 0x%x\n",
			cp[4], cp[5], cp[6], cp[7]));
		ADBG_TRANSPORT(("\t\t 0x%x 0x%x 0x%x 0x%x\n",
			cp[8], cp[9], cp[10], cp[11]));
	}
#endif

	spktp->pkt_state = (STATE_GOT_BUS|STATE_GOT_TARGET|STATE_SENT_CMD);

	return (SUCCESS);
}
