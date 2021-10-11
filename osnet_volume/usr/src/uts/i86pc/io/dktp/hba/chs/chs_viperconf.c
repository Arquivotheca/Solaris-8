/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)chs_viperconf.c	1.6	99/01/11	SMI"


#include "chs.h"

char *viper_gsc_errmsg[] = {
	"success",
	"recovered error",
	"check condition",
	"Invalid opcode in command",
	"Invalid parameters in command block",
	"System Busy",
	"Undefined error",
	"Undefined error",
	"Adapter hardware error",
	"Adapter firmware error",
	"Download jumper set",
	"Command completed with errors",
	"Logical drive not available at this time",
	"System command timeout",
	"Physical drive error",
};

bool_t
chs_get_log_drv_info_viper(chs_t *chs,
				int tgt,
				chs_ld_t *ld_info)
{

	chs_dac_conf_viper_t *conf;

	if (tgt >= chs_get_nsd(chs)) {
		return (FALSE);
	}

	ASSERT(chs->conf != NULL);

	conf = (chs_dac_conf_viper_t *)(chs->conf);
	ld_info->state = conf->log_drv[tgt].state;
	ld_info->raid = conf->log_drv[tgt].raidcache & CHS_V_RAID_MASK;
	(void) strcpy(ld_info->vendorname, "IBM   ");
	ASSERT(chs->enq != NULL);

	ld_info->size =
		(((chs_dac_enquiry_viper_t *)(chs->enq))->sd_sz[tgt]);

	return (TRUE);
}

/*
 * This function will get the state of physical device. Return TRUE,
 * if the state shows that the drive can be accesses, FALSE otherwise
 */
bool_t
chs_can_physdrv_access_viper(chs_t *chs,
				const u_char chn,
				const u_char tgt)
{
	int index;
	chs_dac_conf_viper_t *conf = (chs_dac_conf_viper_t *)(chs->conf);

	index = (chn * chs->max_tgt) + tgt;

	if (chs_disks_scsi) {
		/*
		 * Allow RDY disks to attach if property set
		 */
		if (CHS_DAC_TGT_RDY((conf->dev[index].state)))
			return (TRUE);
	}

	if (!(CHS_DAC_TGT_STANDBY((conf->dev[index].state)))) {
		return (FALSE);
	}

	if (CHS_DAC_TGT_DISK(conf->dev[index].Params)) {
		return (FALSE);
	}

	return (TRUE);
}



/*
 * returns the number of logical drivers configured
 */
unchar
chs_get_nsd(chs_t *chsp)
{

	/*
	 * Eventhough the configuration structure is different
	 * between viper and others the nsd is the first char
	 * in both configurations
	 */
	return (*((unchar *)(chsp->conf)));
}


void
chs_get_scsi_item_viper(chs_cdbt_t *cdbt, chs_dcdb_uncommon_t *dcdb_portionp)
{
		dcdb_portionp->statusp =
			(struct scsi_status *)(&(cdbt->fmt.viper.status));

		dcdb_portionp->cdbp = (u_char *) cdbt->fmt.viper.cdb;

		dcdb_portionp->sglengthp =
			(u_char *) (&(cdbt->fmt.viper.SGLength));

		dcdb_portionp->sensedatap =
			(u_char *)(cdbt->fmt.viper.sensedata);
}


void
chs_iosetup_viper(chs_ccb_t *ccb,  int num_segs, u_long bytes_xfer,
		off_t cofforsrtsec, u_char sdnum, int old)
{
	if (!old) {
		ccb->type = 0x1; /* need to be type c */
		ccb->ccb_sg_count = (u_char)(num_segs);
		ccb->ccb_cptr = ccb->paddr +
			CHS_OFFSET(ccb, ccb->ccb_sg_list);
		ccb->ccb_ccnt = bytes_xfer / CHS_BLK_SIZE;
		if (bytes_xfer % CHS_BLK_SIZE)
			ccb->ccb_ccnt++;
		CCB_CBLK(ccb, cofforsrtsec);
		ccb->ccb_cdrv = sdnum;
		MDBG8(("viperiosetup: sdnum is %d", sdnum));
	}
#ifdef CHS_DEBUG
	else
	{
		/* called from chs_dintr */
		ccb->ccb_sg_count = (u_char)(num_segs);
		CCB_CBLK(ccb, cofforsrtsec);
		ccb->ccb_ccnt = (u_char)bytes_xfer;
	}
#endif

}


/*
 * if cmpkt is null, do not update it it should have been called from
 * dacioc or raid cfg .  returns the uniform error.
 */

int
chs_dac_check_status_viper(struct cmpkt *cmpktp, ushort_t status)
{
	int reason, scb, rc;
	/*
	 * The return value is CHS_SUCCEES, or CHS_FAILURE ored
	 * with the failure type.
	 */

	/* status is <esb><bsb>, masking to become <esb>gsc> */
	switch (status & VIPER_STATUS_GSC_MASK) {
	case	CHS_V_GSC_SUCCESS:
	case	CHS_V_GSC_RECOVERED:
		reason = CPS_SUCCESS;
		scb = DERR_SUCCESS;
		rc = CHS_SUCCESS;
		break;

	case	CHS_V_GSC_INVAL_OPCODE:
		reason = CPS_CHKERR;
		/* thisis invalid opcode should I set it to INVCDB? */
		scb = DERR_INVCDB;
		rc = CHS_INV_OPCODE | CHS_FAILURE;
		break;

	case	CHS_V_GSC_INVAL_CMD_BLK: /* Invalid params in cdb */
		reason = CPS_CHKERR;
		scb = DERR_INVCDB;
		rc = CHS_INVALCHNTGT | CHS_FAILURE;
		break;
	case	CHS_V_GSC_INVAL_PARAM_BLK:
		reason = CPS_CHKERR;
		scb = DERR_INVCDB;
		rc = CHS_FAILURE;
		break;
	case 	CHS_V_GSC_BUSY:
		reason = CPS_CHKERR;
		scb = DERR_BUSY;
		rc = CHS_FAILURE;
		break;
	case 	CHS_V_GSC_CMPLT_WERR:
		if ((((unchar)(status >> 8)) & VIPER_STATUS_ESB_MASK) ==
		    CHS_V_ESB_NORBLD) {
					/* NO RBLD in progress norbdlh */
			reason = CPS_CHKERR;
			scb = DERR_HARD;
			rc = CHS_NORBLD  | CHS_FAILURE;
		} else {
			reason = CPS_CHKERR;
			scb = DERR_HARD;
			rc = CHS_FAILURE;
		}
		break;

	case	CHS_V_GSC_HW_ERR:
	case	CHS_V_GSC_FW_ERR:
	case	CHS_V_GSC_JUMPER_SET:
	case	CHS_V_GSC_LOG_DRV_ERR:
	case	CHS_V_GSC_CMD_TIMEOUT:
	case	CHS_V_GSC_PHYS_DEV_ERR:
		reason = CPS_CHKERR;
		scb = DERR_HARD;
		rc = CHS_FAILURE;
		break;
	default:
		reason = CPS_CHKERR;
		scb = DERR_HARD;
		MDBG8(("chs_v_check-status: BAD STATUS %x", status));
		rc = CHS_FAILURE;
		break;
	}
	if (reason != CPS_SUCCESS) {
		MDBG8(("chs_v_check_status: %s",
			viper_gsc_errmsg[status & VIPER_STATUS_GSC_MASK]));
		MDBG8(("status is <esb><gsc> 0x%x", status));
	}

	if (cmpktp) {
		cmpktp->cp_reason = reason;
		((chs_dac_cmd_t *)cmpktp)->scb = scb;
	}
	return (rc);
}




void
chs_scsi_chkerr_viper(
    chs_t *const chs,
    register chs_ccb_t *const ccb,
    register struct scsi_pkt *const pkt,
    const ushort status)
{
	register struct scsi_arq_status *arq_stat;
	register chs_cdbt_t *cdbt;
	register struct scsi_extended_sense *rqsp;
	ulong	cdbxfered;

	ASSERT(ccb != NULL);
	cdbt = ccb->ccb_cdbt;

	ASSERT(cdbt != NULL);
	ASSERT(pkt != NULL);
	ASSERT((struct scsi_cmd *)SCMD_PKTP(pkt) != NULL);
	ASSERT(CHS_SCSI(CHS_SCSI_PKT2HBA(pkt)));

	pkt->pkt_scbp = (u_char *)&(cdbt->fmt.viper.status);

	if ((cdbt->cmd_ctrl & 0x8) && (cdbt->xfersz == 0)) {
		cdbxfered = 0x1000;
	} else {
		cdbxfered = cdbt->xfersz;
	}

	pkt->pkt_resid = ccb->bytexfer - cdbxfered;		/* final */

	switch (status & VIPER_STATUS_GSC_MASK) {

	case CHS_V_GSC_SUCCESS:
		pkt->pkt_resid  = ccb->bytexfer - cdbxfered;
		pkt->pkt_reason = CMD_CMPLT;
		pkt->pkt_state |= STATE_GOT_BUS  | STATE_GOT_TARGET |
		    STATE_SENT_CMD | STATE_GOT_STATUS;
		if (cdbxfered)
			pkt->pkt_state |= STATE_XFERRED_DATA;
		return;

	case CHS_V_GSC_BUSY:		/* device busy */
		MDBG2(("chs_scsi_chkerr_viper target busy"));
		pkt->pkt_reason = CMD_CMPLT;
		pkt->pkt_state |= STATE_GOT_BUS | STATE_GOT_TARGET |
		    STATE_SENT_CMD | STATE_GOT_STATUS;
		return;

	case CHS_V_GSC_INVAL_OPCODE: 	/* CHS_BAD_OPCODE: */
		MDBG2(("chs_scsi_chkerr_viper invalid opcode "));
		pkt->pkt_reason = CMD_TRAN_ERR;
		return;



	case CHS_V_GSC_INVAL_CMD_BLK:
	case CHS_V_GSC_INVAL_PARAM_BLK:
		/*
		 * Invalid chn# &/or tgt id or bad xfer length.
		 *
		 * NB. xfer length of zero and io beyond the
		 * device limits are the most common causes
		 * of this error.
		 */
		MDBG2(("chs_scsi_chkerr_viper invalid param %x", status));
		if ((chs->flags & CHS_NO_HOT_PLUGGING) &&
		    !(pkt->pkt_flags & FLAG_SILENT)) {
			MDBG2(("chs_chkerr: invalid device "
			    "address or bad xfer length(0x%lx)\n\t or io "
			    "beyond device limits: chn %d, tgt %d, cdbt=%p,"
			    "pkt=%p",
			    cdbxfered, cdbt->unit >> 4, cdbt->unit & 0xf,
			    (void*)cdbt, (void*)pkt));
		}

		pkt->pkt_reason = CMD_TRAN_ERR;
		return;


	case CHS_V_GSC_FW_ERR: /* firmware error */
		MDBG2(("chs_scsi_chkerr_viper firmware err %x", status));
		pkt->pkt_reason = CMD_TRAN_ERR;
		return;

	case CHS_V_GSC_CMD_TIMEOUT:
		MDBG2(("chs_scsi_chkerr_viper command  timeout"));
		pkt->pkt_reason = CMD_TIMEOUT;
		pkt->pkt_statistics |= STAT_TIMEOUT;
		return;
	case CHS_V_GSC_PHYS_DEV_ERR:

		switch (((unchar)(status >> 8)) & VIPER_STATUS_ESB_MASK)  {
		case CHS_V_ESB_SEL_TIMEOUT:
			MDBG2(("chs_scsi_chkerr_viper target timeout"));
			pkt->pkt_reason = CMD_TIMEOUT;
			pkt->pkt_statistics |= STAT_TIMEOUT;
			return;
		case CHS_V_ESB_UNX_BUS_FREE:
			MDBG2(("chs_scsi_chkerr_viper bus free %x",
				status));
			pkt->pkt_reason = CMD_UNX_BUS_FREE;
			return;

		case CHS_V_ESB_DATA_RUN:
			/* This is both for overrun and under run */
			MDBG2(("chs_scsi_chkerr_viper data over/under"));
			if (cdbxfered) {
				/* underrun */
				pkt->pkt_reason = CMD_CMPLT;
				pkt->pkt_state |= STATE_GOT_BUS |
					STATE_GOT_TARGET | STATE_SENT_CMD |
					STATE_GOT_STATUS;
				pkt->pkt_state |= STATE_XFERRED_DATA;
				return;
			}
			pkt->pkt_reason = CMD_DATA_OVR;
			return;
		case CHS_V_ESB_SCSI_PHASE_ERR:	/* scsi phase error */
			MDBG2(("chs_scsi_chkerr_viper scsi phase err"));
			pkt->pkt_reason = CMD_TRAN_ERR;
			return;


		case CHS_V_ESB_CMD_P_ABORTED:
			/* command aborted by PDM_KILL_BUCKET */
			MDBG2(("chs_scsi_chkerr_viper scsi aborted"));
			pkt->pkt_reason = CMD_ABORTED;
			pkt->pkt_statistics |= STAT_ABORTED;
			return;
		case CHS_V_ESB_CMD_ABORTED:
			/* command aborted by scsi controller */
			MDBG2(("chs_scsi_chkerr_viper scsi aborted"));
			pkt->pkt_reason = CMD_ABORTED;
			pkt->pkt_statistics |= STAT_ABORTED;
			return;

		case CHS_V_ESB_FAILED_ABORT:
			MDBG2(("chs_scsi_chkerr_viper scsi abort failed"));
			pkt->pkt_reason = CMD_RESET;
			pkt->pkt_statistics |= STAT_BUS_RESET;
			return;
		case CHS_V_ESB_BUS_RESET:
			MDBG2(("chs_scsi_chkerr_viper scsi bus reset"));
			pkt->pkt_reason = CMD_RESET;
			pkt->pkt_statistics |= STAT_BUS_RESET;
			return;
		case CHS_V_ESB_BUS_RESET_OTHER:
			MDBG2(("chs_scsi_chkerr_viper scsi bus reset"
				" other"));
			pkt->pkt_reason = CMD_RESET;
			pkt->pkt_statistics |= STAT_BUS_RESET;
			return;
		case CHS_V_ESB_ARQ_FAILED:		/* ARQ failed */
			MDBG2(("chs_scsi_chkerr_viper scsi arq failed"));
			pkt->pkt_reason = CMD_CMPLT;	/* ? */
			pkt->pkt_state &= ~STATE_ARQ_DONE;
			return;
		case CHS_V_ESB_MSG_REJECTED:
			/* Device rejected scsi message */
			MDBG2(("chs_scsi_chkerr_viper msg rejected"));
			pkt->pkt_reason = CMD_TRAN_ERR;
			return;
		case CHS_V_ESB_PARITY_ERR:	/* SCSI Parity Error */
			MDBG2(("chs_scsi_chkerr_viper parity err"));
			pkt->pkt_reason = CMD_TRAN_ERR;
			pkt->pkt_statistics |= STAT_PERR;
			return;
		case CHS_V_ESB_RECOVERED:
			/* Target sent Recovered Error sense key */
			MDBG2(("chs_scsi_chkerr_viper recovered error"));
			pkt->pkt_reason = CMD_CMPLT;
			return;

		case CHS_V_ESB_TGT_NORESPOND:
			/* Target device not responding */
			MDBG2(("chs_scsi_chkerr_viper target doesnot "
				"respond"));
			pkt->pkt_reason = CMD_TRAN_ERR;
			return;

		case CHS_V_ESB_CHN_UNFUNC:
			/* One or more scsi channels is not functioning */
			MDBG2(("chs_scsi_chkerr_viper channel "
				"unfunctional"));
			pkt->pkt_reason = CMD_TRAN_ERR;
			return;

		case CHS_V_ESB_CHECK_CONDITION:
			/* check-condition */
			MDBG2(("chs_scsi_chkerr_viper check condition"));
			pkt->pkt_reason = CMD_CMPLT;
			pkt->pkt_state |= STATE_GOT_BUS  | STATE_GOT_TARGET |
				STATE_SENT_CMD | STATE_GOT_STATUS;
			if (cdbxfered)
				pkt->pkt_state |= STATE_XFERRED_DATA;

			pkt->pkt_scbp = (u_char *)&ccb->ccb_arq_stat;
			arq_stat = &ccb->ccb_arq_stat;
			arq_stat->sts_status = *(struct scsi_status *)&status;
			arq_stat->sts_rqpkt_status = (cdbt->fmt.viper.status);
			arq_stat->sts_rqpkt_reason = CMD_CMPLT;
			arq_stat->sts_rqpkt_resid  = 0;
			arq_stat->sts_rqpkt_state |= STATE_XFERRED_DATA;

			/*
			 * ARQ isn't required by target driver, return
			 */
			if (!(CHS_SCSI_PKT2UNIT(pkt))->scsi_auto_req ||
			    CHS_SCSI_AUTO_REQ_OFF(cdbt))
				return;

			/*
			 * ARQ was not issued, indicate check condition
			 */
			if (!cdbt->senselen) {
				*pkt->pkt_scbp = (u_char)status;
				rqsp = (struct scsi_extended_sense *)
					&arq_stat->sts_sensedata;
				rqsp->es_class = CLASS_EXTENDED_SENSE;
				rqsp->es_key = KEY_NO_SENSE;

				MDBG2(("chs_chkerr_viper:"
					"no  sense data available"));
				return;
			}

			if (cdbt->senselen >
				sizeof (struct scsi_extended_sense)) {
				cdbt->senselen =
					sizeof (struct scsi_extended_sense);
			}

			bcopy((caddr_t)&cdbt->fmt.viper.sensedata,
			    (caddr_t)&arq_stat->sts_sensedata,
				cdbt->senselen);

			MDBG2(("chs_chkerr_viper: check-condition,"
				"arq_stat=%p", (void*)arq_stat));

			pkt->pkt_state |= STATE_ARQ_DONE;
			return;
		default:
			MDBG2(("chs_scsi_chkerr_viper: error status is %x",
				status));
			pkt->pkt_reason = CMD_TRAN_ERR;
			cmn_err(CE_NOTE, "chs_chkerr: invalid status 0x%x",
				status);
		}
	default:
		MDBG2(("chs_v_check_status: %s",
			viper_gsc_errmsg[status & VIPER_STATUS_GSC_MASK]));
		MDBG2(("status is <esb><gsc> 0x%x", status));
		cmn_err(CE_CONT,
		    "?chs_scsi_chkerr_viper received status"
		    " <esb><gsc> 0x%x %s",
		    status, viper_gsc_errmsg[status & VIPER_STATUS_GSC_MASK]);
		pkt->pkt_reason = CMD_TRAN_ERR;
	}
}





bool_t
chs_in_any_sd_viper(chs_t *chs,
			const u_char chn,
			const u_char tgt,
			u_char *raidl)
{
	u_char nsd;
	register int s;
	chs_dac_conf_viper_t *conf;

	ASSERT(chs != NULL);
	ASSERT(mutex_owned(&chs_global_mutex) || mutex_owned(&chs->mutex));

	conf = (chs_dac_conf_viper_t *)chs->conf;
	ASSERT(conf != NULL);

	nsd = chs_get_nsd(chs);
	if (!nsd)
		return (FALSE);

	ASSERT(nsd <= CHS_DAC_MAX_SD);
	for (s = 0; s < nsd; s++) {
		register int a;
		register int nochunks;
		register logical_drive  *sd = &conf->log_drv[s];

		ASSERT(sd != NULL);
		nochunks = sd->NoChunkUnits;
		if (!nochunks)
			continue;
		ASSERT(nochunks <= CHS_MAX_CHUNKS);
		for (a = 0; a < nochunks; a++) {
			register chs_chunk_t *chunk = &sd->chunk[a];

			ASSERT(chunk != NULL);
			if (chunk->tgt == tgt && chunk->chn == chn) {
				MDBG5(("chs_in_any_sd_viper: chn %d, tgt %d"
					" => System Drive %d "
					"chunk %d", chn, tgt, s, a));
				*raidl = sd->raidcache & CHS_V_RAID_MASK;
				return (TRUE);
			}
		}
	}
	return (FALSE);
}




/*
 * Extracts the info needed from the Enquiry Info and preserves it
 * it as part of the current s/w state.
 */
void
chs_getenq_info_viper(register chs_t *const chs)
{
	register chs_dac_enquiry_viper_t *enq;


	ASSERT(chs != NULL);
	ASSERT(mutex_owned(&chs_global_mutex) || mutex_owned(&chs->mutex));
	enq = (chs_dac_enquiry_viper_t *)chs->enq;
	ASSERT(enq != NULL);
	ASSERT(chs->conf != NULL);

	/* To ensure that chs->enq and chs->conf are always in sync */
	ASSERT((chs_get_nsd(chs)) == enq->nsd);

	chs->max_cmd = enq->ConCurrentCmdCount;

	chs->fw_version = UNKNOWN;

	chs->flags |= CHS_SUPPORTS_SG;
	chs->sgllen = CHS_MAX_NSG;
	chs->flags |= CHS_NO_HOT_PLUGGING;

	MDBG5(("chs_getenq_info_viper: f/w ver %d, max %d concurrent "
		"cmds, flags=0x%x",
			chs->fw_version, chs->max_cmd, chs->flags));
}
