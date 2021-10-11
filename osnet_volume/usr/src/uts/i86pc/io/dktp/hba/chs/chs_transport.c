/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_transport.c	1.7	99/01/11 SMI"

#include "chs.h"

int
chs_tgt_init(
    dev_info_t *const hba_dip,
    dev_info_t *const tgt_dip,
    register scsi_hba_tran_t *const hba_tran,
    struct scsi_device *const sd)
{
	register chs_hba_t *hba;

	ASSERT(hba_tran != NULL);
	hba = CHS_SCSI_TRAN2HBA(hba_tran);

	ASSERT(hba != NULL);
	return ((CHS_DAC(hba)) ?
		chs_dac_tran_tgt_init(hba_dip, tgt_dip, hba_tran, sd) :
		chs_tran_tgt_init(hba_dip, tgt_dip, hba_tran, sd));
}

/*ARGSUSED*/
void
chs_tgt_free(
    register dev_info_t *const hba_dip,
    register dev_info_t *const tgt_dip,
    register scsi_hba_tran_t *const hba_tran,
    register struct scsi_device *const sd)
{
	size_t size;
	chs_hba_t *hba;
	chs_unit_t *child_unit;

	ASSERT(hba_dip != NULL);
	ASSERT(tgt_dip != NULL);
	ASSERT(hba_tran != NULL);
	hba = CHS_SCSI_TRAN2HBA(hba_tran);
	ASSERT(hba != NULL);

	MDBG2(("chs_tgt_free: %s%d %s%d\n",
	    ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
	    ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip)));

	size = sizeof (chs_unit_t);
	child_unit = hba_tran->tran_tgt_private;
	ASSERT(child_unit != NULL);
	if (CHS_DAC(hba)) {
		size += sizeof (struct ctl_obj);
		if (sd->sd_inq != NULL) {
			kmem_free((caddr_t)sd->sd_inq,
			    sizeof (struct scsi_inquiry));
		}
		sd->sd_inq = NULL;
	}
	kmem_free(child_unit, size);

	mutex_enter(&hba->mutex);
	hba->refcount--;		/* decrement active children */
	mutex_exit(&hba->mutex);
}

int
chs_tgt_probe(
    register struct scsi_device *const sd,
    int (*const callback)())
{
	int rval = SCSIPROBE_FAILURE;
#ifdef CHS_DEBUG
	char *s;
#endif
	register chs_hba_t *hba;
	int tgt;

	ASSERT(sd != NULL);
	hba = CHS_SDEV2HBA(sd);
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));

	tgt = sd->sd_address.a_target;

	if (sd->sd_address.a_lun) {
		MDBG2(("chs%d: %s target-zero %d lun!=0 %d\n",
			ddi_get_instance(hba->dip),
			ddi_get_name(sd->sd_dev),
			tgt, sd->sd_address.a_lun));
		return (rval);
	}

	if (chs_dont_access(hba->chs, hba->chn, (u_char)tgt))
		return (rval);

	rval = scsi_hba_probe(sd, callback);

#ifdef CHS_DEBUG
	switch (rval) {
	case SCSIPROBE_NOMEM:
		s = "scsi_probe_nomem";
		break;
	case SCSIPROBE_EXISTS:
		s = "scsi_probe_exists";
		break;
	case SCSIPROBE_NONCCS:
		s = "scsi_probe_nonccs";
		break;
	case SCSIPROBE_FAILURE:
		s = "scsi_probe_failure";
		break;
	case SCSIPROBE_BUSY:
		s = "scsi_probe_busy";
		break;
	case SCSIPROBE_NORESP:
		s = "scsi_probe_noresp";
		break;
	default:
		s = "???";
		break;
	}
#endif

	MDBG2(("chs%d: %s target %d lun %d %s\n",
	    ddi_get_instance(hba->dip), ddi_get_name(sd->sd_dev),
	    sd->sd_address.a_target, sd->sd_address.a_lun, s));

	return (rval);
}

/*
 * Execute a SCSI command during init time using no interrupts or
 * command overlapping.
 */
int
chs_init_cmd(register chs_t *const chs,
		register chs_ccb_t *const ccb)
{
	ASSERT(chs != NULL);
	ASSERT(ccb != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));

	if (chs_sendcmd(chs, ccb, 1) == DDI_FAILURE ||
	    chs_pollstat(chs, ccb, 1) == DDI_FAILURE)
		return (DDI_FAILURE);

	if (CHS_DAC_CHECK_STATUS(chs, NULL, ccb->ccb_status) &
	    CHS_INV_OPCODE) {
		cmn_err(CE_WARN,
		    "chs_init_cmd: failed opcode: %x cmdid: %d status %x",
			ccb->ccb_opcode, ccb->ccb_cmdid,
			ccb->ccb_status);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * init = 1	=> called during initialization from chs_init_cmd()
 * init = 0	=> called from chs_{scsi,dac}_transport()
 */
int
chs_sendcmd(register chs_t *const chs,
		register chs_ccb_t *ccb,
		int init)
{
	chs_ccb_t	*ccb1;
	chs_ccb_stk_t	*ccb2;
	int		cntr;

	ASSERT(ccb != NULL);
	ASSERT(chs != NULL);

	if (init) {
		ASSERT(mutex_owned(&chs_global_mutex));
		/*
		 * Need not change cmdid during init, as chs_attach()
		 * is serialized and polls on the status of cmd's 1 by 1.
		 */
		ccb->ccb_cmdid = CHS_INVALID_CMDID;
	} else {
		ASSERT(mutex_owned(&chs->mutex));
		if (chs->free_ccb->next == CHS_INVALID_CMDID)
			return (CHS_V_GSC_BUSY);
		ccb->ccb_cmdid = chs->free_ccb - chs->ccb_stk;
	}
	ccb->ccb_stat_id = ccb->ccb_cmdid;

	/* Check if command mail box is free, if not retry. */
	for (cntr = CHS_MAX_RETRY; cntr > 0; cntr--) {
		if (CHS_CREADY(chs))
			break;
		if (cntr & 1)
			drv_usecwait(1);
	}
	if (cntr == 0) {
		cmn_err(CE_NOTE, "?chs_sendcmd: not ready to accept "
			"commands, retried %u times", CHS_MAX_RETRY);
		return (CHS_V_GSC_BUSY);
	}

	if (!init) {
		/* Push the ccb onto the ccb_stk stack */
		ASSERT(mutex_owned(&chs->mutex));
		ASSERT(chs->free_ccb->ccb == NULL);
		ccb1 = chs->free_ccb->ccb;
		ccb2 = chs->free_ccb;
		chs->free_ccb->ccb = ccb;
		chs->free_ccb = chs->ccb_stk + chs->free_ccb->next;
	}

	/* Issue the command. */
	if (!CHS_CSEND(chs, (void *)ccb)) {
		if (!init) {
			/* Pop the aborted ccb from the ccb_stk stack */
			chs->free_ccb->ccb = ccb1;
			chs->free_ccb = ccb2;
		}
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}



/*
 * init = 1	=> called during initialization from chs_init_cmd()
 * init = 0	=> called from chs_{scsi,dac}_transport()
 */
int
chs_pollstat(
    register chs_t	*chs,
    register chs_ccb_t	*const ccb,
    int init)
{
	int cntr;
	int retry;
	chs_ccb_t	*lccb;
	chs_stat_t	hw_stat;
	chs_stat_t	*hw_statp = &hw_stat;
	int 		gotit = 0, needstatinit;



	ASSERT(ccb != NULL);
	ASSERT(chs != NULL);

	ASSERT((init) ? mutex_owned(&chs_global_mutex) : 1);
	ASSERT(mutex_owned(&chs->mutex));

	/* check if intr handled this */
	ccb->intr_wanted = 1;
	for (retry = CHS_MAX_RETRY; retry > 0; drv_usecwait(10), retry--) {
		for (cntr = CHS_MAX_RETRY; cntr > 0;
		    drv_usecwait(10), cntr--) {
			if (CHS_IREADY(chs))
				break;
		}
		if (cntr == 0) {
			cmn_err(CE_WARN,
				"chs_pollstat: status not ready, "
				"retried %u times", CHS_MAX_RETRY);
			return (DDI_FAILURE);
		}
		if (CHS_GET_ISTAT(chs, hw_statp, 1) == 0) {
			/* empty */
			continue;
		}

		if (hw_statp->stat_id == CHS_NEPTUNE_CMDID) {
			cmn_err(CE_NOTE, "?chs_intr: Possible Neptune"
					"chipset errata\n");
			continue;
		}
		needstatinit = 1;
		if (ccb->ccb_cmdid == hw_statp->stat_id) {
			gotit = 1;
			/* got it */
			if (init) {
				/* do not do any thing with stat */
				/* whoever asked for it, takes care of stat */
				needstatinit = 0;
			}
		}

		if (needstatinit) {
			lccb = (chs_ccb_t *)
				chs_process_status(chs, hw_statp);
		} else {
			ccb->ccb_hw_stat = *hw_statp;
		}
		if (!gotit) {
			if (lccb == NULL) {
				MDBG1(("imbraid_pollstat: NULL ccb,"
					" spurious stat from card"));
			} else
				QueueAdd(&chs->doneq, &lccb->ccb_q, lccb);
		}
		if (gotit)
			break;
	}

	chs_process_doneq(chs, &chs->doneq);

	if (retry == 0) {
		cmn_err(CE_WARN, "chs_pollstat: failed to get the status"
			" of cmdid %d, retried %d times", ccb->ccb_cmdid,
				CHS_MAX_RETRY);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

int
chs_tran_tgt_init(
    dev_info_t *const hba_dip,
    dev_info_t *const tgt_dip,
    scsi_hba_tran_t *const hba_tran,
    register struct scsi_device *const sd)
{
	int tgt;
	register chs_t *chs;
	register chs_hba_t *hba;
	register chs_unit_t *hba_unit;			/* self */
	register chs_unit_t *child_unit;
	register struct scsi_inquiry *scsi_inq;

	ASSERT(sd != NULL);
	hba_unit = CHS_SDEV2HBA_UNIT(sd);
	/* nexus should already be initialized */
	ASSERT(hba_unit != NULL);
	hba = hba_unit->hba;
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));
	chs = hba->chs;
	ASSERT(chs != NULL);

	tgt = sd->sd_address.a_target;
	if (tgt >= chs->max_tgt)
		return (DDI_FAILURE);
	if (sd->sd_address.a_lun) {
		cmn_err(CE_WARN, "chs_tran_tgt_init: non-zero lun on "
		    "chn %d tgt %d cannot be supported", hba->chn, tgt);
		return (DDI_FAILURE);
	}
	scsi_inq = sd->sd_inq;

	mutex_enter(&chs->mutex);
	if (scsi_inq && (chs->flags & CHS_NO_HOT_PLUGGING)) {
		/*
		 * chs_getinq() set inq_dtype for the device
		 * based on its accessiblility.
		 */
		if (scsi_inq->inq_dtype == DTYPE_NOTPRESENT ||
		    scsi_inq->inq_dtype == DTYPE_UNKNOWN) {
			mutex_exit(&chs->mutex);
			return (DDI_FAILURE);
		}
	}

	if (chs_dont_access(chs, hba->chn, tgt)) {
		mutex_exit(&chs->mutex);
		return (DDI_FAILURE);
	}
	mutex_exit(&chs->mutex);

	if (scsi_inq && (scsi_inq->inq_rdf == RDF_SCSI2) &&
	    (scsi_inq->inq_cmdque)) {
		ASSERT(hba_dip != NULL);
		ASSERT(tgt_dip != NULL);
		chs_childprop(hba_dip, tgt_dip);
	}

	child_unit = (chs_unit_t *)kmem_zalloc(sizeof (chs_unit_t),
		KM_NOSLEEP);
	if (child_unit == NULL)
		return (DDI_FAILURE);

	bcopy((caddr_t)hba_unit, (caddr_t)child_unit, sizeof (*hba_unit));
	child_unit->dma_lim = chs_dma_lim;
	/*
	 * After this point always use (chs_unit_t *)->dma_lim and refrain
	 * from using chs_dac_dma_lim as the former can be customized per
	 * unit target driver instance but the latter is the generic hba
	 * instance dma limits.
	 */

	ASSERT(hba_tran != NULL);
	hba_tran->tran_tgt_private = child_unit;

	mutex_enter(&hba->mutex);
	hba->refcount++;   		/* increment active child refcount */
	mutex_exit(&hba->mutex);

	MDBG2(("chs_tran_tgt_init: S%xC%xt%x dip=0x%p sd=0x%p unit=0x%p",
	    CHS_SLOT(hba->chs->reg) /* Slot */, hba->chn, tgt,
	    (void*)tgt_dip, (void*)sd, (void*)child_unit));

	return (DDI_SUCCESS);
}

struct scsi_pkt *
chs_init_pkt(
    struct scsi_address *const sa,
    register struct scsi_pkt *pkt,
    register buf_t *const bp,
    int cmdlen,
    int statuslen,
    int tgt_priv_len,
    int flags, int (*const callback)(),
    const caddr_t arg)
{
	struct scsi_pkt	*new_pkt = NULL;

	ASSERT(sa != NULL);
	ASSERT(CHS_SCSI(CHS_SA2HBA(sa)));

	/* Allocate a pkt only if NULL */
	if (pkt == NULL) {
		pkt = chs_pktalloc(sa, cmdlen, statuslen,
		    tgt_priv_len, callback, arg);
		if (pkt == NULL)
			return (NULL);
		((struct scsi_cmd *)pkt)->cmd_flags = flags;
		new_pkt = pkt;
	} else
		new_pkt = NULL;

	/* Set up dma info only if bp is non-NULL */
	if (bp != NULL &&
	    chs_dmaget(pkt, (opaque_t)bp, callback, arg) == NULL) {
		if (new_pkt != NULL)
			chs_pktfree(new_pkt);
		return (NULL);
	}

	return (pkt);
}

void
chs_destroy_pkt(struct scsi_address *const sa, struct scsi_pkt *const pkt)
{
	ASSERT(sa != NULL);
	ASSERT(CHS_SCSI(CHS_SA2HBA(sa)));

	chs_dmafree(sa, pkt);
	chs_pktfree(pkt);
}

struct scsi_pkt *
chs_pktalloc(
    register struct scsi_address *const sa,
    int cmdlen,
    int statuslen,
    int tgt_priv_len,
    int (*const callback)(),
    const caddr_t arg)
{
	register struct scsi_cmd *cmd;
	chs_ccb_t *ccb;
	register chs_cdbt_t *cdbt;
	register chs_hba_t *hba;
	register chs_unit_t *unit;
	caddr_t	tgt_priv;
	chs_dcdb_uncommon_t  chs_dcdb_portion;
	int kf = HBA_KMFLAG(callback);

	ASSERT(sa != NULL);
	unit = CHS_SA2UNIT(sa);
	ASSERT(unit != NULL);
	hba = unit->hba;
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));

	/* Allocate target private data, if necessary */
	if (tgt_priv_len > PKT_PRIV_LEN) {
		tgt_priv = kmem_zalloc(tgt_priv_len, kf);
		if (tgt_priv == NULL) {
			ASSERT(callback != SLEEP_FUNC);
			if (callback != NULL_FUNC)
				ddi_set_callback(callback, arg,
				    &hba->callback_id);
			return (NULL);
		}
	} else		/* not necessary to allocate target private data */
		tgt_priv = NULL;

	cmd = (struct scsi_cmd *)kmem_zalloc(sizeof (*cmd), kf);
	if (cmd != NULL) {
		size_t mem = sizeof (*ccb) + sizeof (*cdbt);

		ASSERT(hba->dip != NULL);

		/*
		 * Allocate a ccb.
		 */
		if (ddi_iopb_alloc(hba->dip, &chs_dma_lim,
		    mem, (caddr_t *)&ccb) != DDI_SUCCESS) {
			kmem_free((caddr_t)cmd, sizeof (*cmd));
			cmd = NULL;
		} else {
			bzero((caddr_t)ccb, mem);
		}

	}

	if (cmd == NULL) {
		if (tgt_priv != NULL)
			kmem_free(tgt_priv, tgt_priv_len);
		if (callback != DDI_DMA_DONTWAIT)
			ddi_set_callback(callback, arg, &hba->callback_id);
		return (NULL);
	}

	ccb->paddr = CHS_KVTOP(ccb);
	ccb->type = CHS_SCSI_CTYPE;
	ccb->ccb_opcode = CHS_SCSI_DCDB;

	ccb->ccb_cdbt = cdbt = (chs_cdbt_t *)(ccb + 1);
	ccb->ccb_xferpaddr = ccb->paddr + sizeof (*ccb);

	/* Set up target private data */
	cmd->cmd_privlen = (u_char)tgt_priv_len;
	if (tgt_priv_len > PKT_PRIV_LEN)
		cmd->cmd_pkt.pkt_private = (opaque_t)tgt_priv;
	else if (tgt_priv_len > 0)
		cmd->cmd_pkt.pkt_private = cmd->cmd_pkt_private;

	cdbt->unit = (hba->chn << 4) | (u_char)sa->a_target;

	/* XXX - How about early status? */
	CHS_SCSI_CDB_NORMAL_STAT(cdbt->cmd_ctrl);
	CHS_SCSI_CDB_1HR_TIMEOUT(cdbt->cmd_ctrl);

	CHS_GET_SCSI_ITEM(hba->chs, cdbt, &chs_dcdb_portion);
	if (unit->scsi_auto_req) {
		CHS_SCSI_CDB_AUTO_REQ_SENSE(cdbt->cmd_ctrl);
		cmd->cmd_pkt.pkt_scbp = (u_char *)&ccb->ccb_arq_stat;
	} else {
		CHS_SCSI_CDB_NO_AUTO_REQ_SENSE(cdbt->cmd_ctrl);
		cmd->cmd_pkt.pkt_scbp = (u_char *) chs_dcdb_portion.statusp;
	}
	/*
	 * Disconnects will be enabled, if appropriate,
	 * after the pkt_flags bit is set
	 */
	cdbt->cdblen = (u_char)cmdlen;
	cdbt->senselen = CHS_SCSI_MAX_SENSE;

	cmd->cmd_pkt.pkt_cdbp = (opaque_t)chs_dcdb_portion.cdbp;

	ccb->ccb_ownerp = cmd;

	cmd->cmd_private = (opaque_t)ccb;
	cmd->cmd_cdblen = (u_char)cmdlen;
	cmd->cmd_scblen = (u_char)statuslen;
	cmd->cmd_pkt.pkt_address = *sa;
	return ((struct scsi_pkt *)cmd);
}

void
chs_pktfree(register struct scsi_pkt *const pkt)
{
	register chs_ccb_t *ccb;
	register chs_hba_t *hba;
	register struct scsi_cmd *cmd;

	ASSERT(pkt != NULL);
	cmd = (struct scsi_cmd *)pkt;

	hba = CHS_SCSI_PKT2HBA(pkt);
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));

	if (cmd->cmd_privlen > PKT_PRIV_LEN) {
		ASSERT(pkt->pkt_private != NULL);
		kmem_free(pkt->pkt_private, cmd->cmd_privlen);
	}

	ccb = (chs_ccb_t *)cmd->cmd_private;
	if (ccb != NULL)
		ddi_iopb_free((caddr_t)ccb);

	kmem_free((caddr_t)cmd, sizeof (*cmd));


	if (hba->callback_id)
		ddi_run_callback(&hba->callback_id);
}

struct scsi_pkt *
chs_dmaget(
    register struct scsi_pkt *const pkt,
    const opaque_t dmatoken,
    int (*const callback)(),
    const caddr_t arg)
{
	ushort max_xfer;	/* max that can be transferred w/ or w/o SG */
	int fw_supports_sg;
	off_t offset;
	off_t len;
	register int cnt;
	register u_int pkt_totxfereq;		/* total xfer request */
	register buf_t *bp = (buf_t *)dmatoken;
	register struct scsi_cmd *cmd = (struct scsi_cmd *)pkt;
	register chs_t *chs;
	register chs_hba_t *hba;
	register chs_ccb_t *ccb;
	register chs_cdbt_t *cdbt;
	chs_dcdb_uncommon_t  chs_dcdb_portion;
	ddi_dma_cookie_t dmac;
	register ddi_dma_cookie_t *dmacp = &dmac;

	ASSERT(pkt != NULL);		/* also cmd != NULL */
	hba = CHS_SCSI_PKT2HBA(pkt);
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));
	chs = hba->chs;
	ASSERT(chs != NULL);
	ASSERT(bp != NULL);
	pkt_totxfereq = bp->b_bcount;
	ccb = (chs_ccb_t *)cmd->cmd_private;
	ASSERT(ccb != NULL);
	cdbt = ccb->ccb_cdbt;
	ASSERT(cdbt != NULL);

	if (!pkt_totxfereq) {
		pkt->pkt_resid = 0;
		cdbt->xfersz = 0;
		CHS_SCSI_CDB_NODATA(cdbt->cmd_ctrl);
		return (pkt);
	}

	/* Check direction for data transfer */
	if (bp->b_flags & B_READ) {
		CHS_SCSI_CDB_DATAIN(cdbt->cmd_ctrl);
		cmd->cmd_cflags &= ~CFLAG_DMASEND;
	} else {
		CHS_SCSI_CDB_DATAOUT(cdbt->cmd_ctrl);
		cmd->cmd_cflags |= CFLAG_DMASEND;
	}

	/* Setup dma memory and position to the next xfer segment */
	if (scsi_impl_dmaget(pkt, (opaque_t)bp, callback, arg,
	    &(CHS_SCSI_PKT2UNIT(pkt)->dma_lim)) == NULL)
		return (NULL);
	if (ddi_dma_segtocookie(cmd->cmd_dmaseg, &offset, &len, dmacp) ==
	    DDI_FAILURE)
		return (NULL);

	/* Establish how many bytes can be transferred w/o SG */
	max_xfer = (ushort)CHS_MIN(CHS_MIN(pkt_totxfereq, CHS_SCSI_MAX_XFER),
	    dmacp->dmac_size);
	if (CHS_SCSI_AUTO_REQ_OFF(cdbt) &&
	    (*(pkt->pkt_cdbp) == SCMD_REQUEST_SENSE))
		max_xfer = CHS_MIN(CHS_SCSI_MAX_SENSE, max_xfer);

	mutex_enter(&chs->mutex);
	fw_supports_sg = chs->flags & CHS_SUPPORTS_SG;
	mutex_exit(&chs->mutex);

	/* Check for one single block transfer */
	if (pkt_totxfereq <= max_xfer || !fw_supports_sg) {
		/* need not or cannot do SG transfer */
		cdbt->xfersz = (ushort)CHS_MIN(pkt_totxfereq, max_xfer);
		cdbt->databuf = (paddr_t)dmacp->dmac_address;
		cmd->cmd_totxfer = cdbt->xfersz;
	} else if (fw_supports_sg) {	/* attempt multi-block SG transfer */
		register int bxfer;
		register chs_sg_element_t *sge;

		/* Request Sense shouldn't need scatter-gather io */
		ASSERT(*(pkt->pkt_cdbp) != SCMD_REQUEST_SENSE);

		/* ccb->type is set to CHS_SCSI_CTYPE, in _pktalloc() */
		ccb->ccb_opcode = CHS_SCSI_SG_DCDB;

		/* max_xfer is no longer limited to the 1st dmacp->dmac_size */
		max_xfer = (ushort)CHS_MIN(pkt_totxfereq, CHS_SCSI_MAX_XFER);

		/* Set address of scatter-gather segs */
		sge = ccb->ccb_sg_list;
		for (bxfer = 0, cnt = 1; ; cnt++, sge++) {
			bxfer += dmacp->dmac_size;

			sge->data01_ptr32 = (ulong)dmacp->dmac_address;
			sge->data02_len32 = (ulong)dmacp->dmac_size;

			/* Check for end of list condition */
			if (pkt_totxfereq == (bxfer + cmd->cmd_totxfer))
				break;
			ASSERT(pkt_totxfereq > (bxfer + cmd->cmd_totxfer));

			/*
			 * Check for end of list condition and check
			 * end of physical scatter-gather list limit,
			 * then attempt to get the next dma segment
			 * and cookie.
			 */
			if (bxfer >= max_xfer || cnt >= CHS_MAX_NSG ||
			    ddi_dma_nextseg(cmd->cmd_dmawin,
				cmd->cmd_dmaseg,
				&cmd->cmd_dmaseg) != DDI_SUCCESS ||
				ddi_dma_segtocookie(cmd->cmd_dmaseg, &offset,
				    &len, dmacp) == DDI_FAILURE)
				break;
		}
		ASSERT(cnt <= CHS_MAX_NSG);

		CHS_GET_SCSI_ITEM(hba->chs, cdbt, &chs_dcdb_portion);

		if (chs_dcdb_portion.sglengthp != NULL) {
			/* For viper it is not null */
			*(chs_dcdb_portion.sglengthp) = (u_char) cnt;
		}


		/* In case bxfer exceeded max_xfer in the last iteration */
		bxfer = CHS_MIN(bxfer, max_xfer);
		cdbt->xfersz = (ushort)bxfer;
		cdbt->databuf = ccb->paddr +
		    CHS_OFFSET(ccb, ccb->ccb_sg_list);
		cmd->cmd_totxfer += bxfer;
	}

	/* f/w updates cdbt->xfersz, so we have to preserve it */
	ccb->bytexfer = cdbt->xfersz;

	/*
	 * We have to calculate the "tentative" value of pkt_resid which
	 * is the left over of data *if* transport is successful.  This
	 * value needs to be communicated back to the target layer to
	 * update the contents of the SCSI cdb which the hba layer is
	 * not allowed to change.
	 *
	 * Based on this tentative value of pkt_resid the target layer
	 * updates the SCSI cdb and then attempts the transport of the
	 * (same) packet.
	 */
	pkt->pkt_resid = pkt_totxfereq - cmd->cmd_totxfer;	/* tentative */
	ASSERT(pkt->pkt_resid >= 0);

	return (pkt);
}

/* Dma resource deallocation */
/*ARGSUSED*/
void
chs_dmafree(
    struct scsi_address *const sa,
    register struct scsi_pkt *const pkt)
{
	register struct	scsi_cmd *cmd = (struct scsi_cmd *)pkt;

	ASSERT(sa != NULL);
	ASSERT(CHS_SCSI(CHS_SA2HBA(sa)));
	ASSERT(cmd != NULL);

	if (cmd->cmd_dmahandle) {		/* Free the mapping. */
		if (ddi_dma_free(cmd->cmd_dmahandle) == DDI_FAILURE)
			cmn_err(CE_WARN, "chs_dmafree: dma free failed, "
				"dmahandle %p", (void*)cmd->cmd_dmahandle);
		cmd->cmd_dmahandle = NULL;
	}
}

/*ARGSUSED*/
void
chs_sync_pkt(
    struct scsi_address *const sa,
    register struct scsi_pkt *const pkt)
{
	register int rval;
	register struct	scsi_cmd *cmd = (struct scsi_cmd *)pkt;

	ASSERT(sa != NULL);
	ASSERT(CHS_SCSI(CHS_SA2HBA(sa)));
	ASSERT(cmd != NULL);

	if (cmd->cmd_dmahandle) {
		rval = ddi_dma_sync(cmd->cmd_dmahandle, 0, 0,
		    (cmd->cmd_cflags & CFLAG_DMASEND) ?
			DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
		if (rval != DDI_SUCCESS)
			cmn_err(CE_WARN, "chs_sync_pkt: dma sync failed");
	}
}

/*ARGSUSED*/
int
chs_transport(
    struct scsi_address *const sa,
    register struct scsi_pkt *const pkt)
{
	register chs_hba_t *hba;
	register chs_ccb_t *ccb;
	register chs_t *chs;
	int ret;

	ASSERT(pkt != NULL);
	ccb = (chs_ccb_t *)SCMD_PKTP(pkt)->cmd_private;
	ASSERT(ccb != NULL);

	hba = CHS_SCSI_PKT2HBA(pkt);
	ASSERT(hba != NULL);
	ASSERT(CHS_SCSI(hba));
	chs = hba->chs;
	ASSERT(chs != NULL);

	ASSERT(ccb->ccb_cdbt != NULL);

	/*
	 * Initialize some pkt vars that might need it on re-transport.
	 * Note that due to a fw bug, no disconnect direct cdb commands
	 * may hang, and are therefore not allowed.
	 */
	CHS_SCSI_CDB_DISCON(ccb->ccb_cdbt->cmd_ctrl);
	if (pkt->pkt_time == 0)
		CHS_SCSI_CDB_1HR_TIMEOUT(ccb->ccb_cdbt->cmd_ctrl);
	else if (pkt->pkt_time <= 10)
		CHS_SCSI_CDB_10SEC_TIMEOUT(ccb->ccb_cdbt->cmd_ctrl);
	else if (pkt->pkt_time <= 60)
		CHS_SCSI_CDB_60SEC_TIMEOUT(ccb->ccb_cdbt->cmd_ctrl);
	else if (pkt->pkt_time <= 60*20)
		CHS_SCSI_CDB_20MIN_TIMEOUT(ccb->ccb_cdbt->cmd_ctrl);
	else
		CHS_SCSI_CDB_1HR_TIMEOUT(ccb->ccb_cdbt->cmd_ctrl);

	pkt->pkt_statistics = 0;
	pkt->pkt_resid = 0;
	pkt->pkt_state = 0;

	if (ccb->type != CHS_SCSI_CTYPE) {
		cmn_err(CE_WARN, "chs_transport: bad SCSI ccb cmd, "
		    "type %d, pkt %x", ccb->type, (int)pkt);
		return (TRAN_FATAL_ERROR);
	}
	switch (ccb->type) {
	case CHS_DAC_CTYPE0:
	case CHS_DAC_CTYPE1:
	case CHS_DAC_CTYPE2:
	case CHS_DAC_CTYPE4:
	case CHS_DAC_CTYPE5:
	case CHS_DAC_CTYPE6:
		break;
	case CHS_SCSI_CTYPE:
		if (!sema_tryp(&chs->scsi_ncdb_sema)) {
			MDBG2(("chs_transport: refused to xport "
				"Direct CDB cmd, hit the max"));
			return (TRAN_BUSY);
		}
		break;
	default:
		cmn_err(CE_WARN, "chs_transport: bad dac cmd type %d, "
		    "pkt %x", ccb->type, (int)pkt);
		return (TRAN_BADPKT);
	}

	/* XXX - ddi_dma_sync() for device, not required for EISA cards. */

	mutex_enter(&chs->mutex);
	if ((ret = chs_sendcmd(chs, ccb, 0)) == DDI_FAILURE) {
		mutex_exit(&chs->mutex);
		return (TRAN_BADPKT);
	} else if (ret != DDI_SUCCESS) {
		mutex_exit(&chs->mutex);
		return (TRAN_BUSY);
	}

	if (pkt->pkt_flags & FLAG_NOINTR) {
		CHS_DISABLE_INTR(chs);
		if (chs_pollstat(chs, ccb, 0) == DDI_FAILURE) {
			pkt->pkt_reason = CMD_TRAN_ERR;
			pkt->pkt_state = 0;
		}
		CHS_ENABLE_INTR(chs);
	}
	mutex_exit(&chs->mutex);
	return (TRAN_ACCEPT);
}

/* Abort specific command on target device */
/*ARGSUSED*/
int
chs_abort(struct scsi_address *const sa, struct scsi_pkt *const pkt)
{
	ASSERT(CHS_SCSI(CHS_SA2HBA(sa)));
	/* IBM RAID PCI card does not support recall of command in process */
	return (0);
}

/*
 * Reset the bus, or just one target device on the bus.
 * Returns 1 on success and 0 on failure.
 *
 * Currently no packet queueing is done prior to transport.  Therefore,
 * after a successful reset operation, we can only wait until the aborted
 * commands are returned with the appropriate error status from the
 * adapter and then set the pkt_reason and pkt_statistics accordingly.
 */
/*ARGSUSED*/
int
chs_reset(register struct scsi_address *const sa, int level)
{
	/*
	 * Because of performance reasons the adapter cannot tolerate
	 * long timeouts on the drives.  The SCSI disk drives which do
	 * not have a quick enough response to Device Ready after a
	 * reset are set to DEAD by the f/w which causes a serious
	 * inconvenience to the user.  Unfortunately, the majority of
	 * the SCSI disk drives in the market fall into this catagory
	 * and except a few which are approved by Mylex do not behave
	 * properly at reset.  Hence, it is more appropriate to return
	 * failure for this entry point than cause grieviance to users.
	 *
	 * Taq queueing on some disks at the f/w level which display the
	 * above symptom.  Hence, it is strongly recommended to turn
	 * taq queueing off unless the dirve is explicitly listed in the
	 * tested and approved drives by IBM.
	 */
	return (0);
}

int
chs_capchk(
    register char *cap,
    int tgtonly,
    int *cap_idxp)
{
	if ((tgtonly && tgtonly != 1) || cap == NULL)
		return (0);

	*cap_idxp = scsi_hba_lookup_capstr(cap);
	return (1);
}

int
chs_getcap(
    register struct scsi_address *const sa,
    char *cap,
    int tgtonly)
{
	int 			cap_idx;
	int			heads = 64, sectors = 32;
	register chs_unit_t 	*unit;

	ASSERT(sa != NULL);
	unit = CHS_SA2UNIT(sa);
	ASSERT(unit != NULL);
	ASSERT(unit->hba != NULL);
	ASSERT(CHS_SCSI(unit->hba));

	if (!chs_capchk(cap, tgtonly, &cap_idx))
		return (UNDEFINED);

	switch (cap_idx) {
		case SCSI_CAP_GEOMETRY:
			if (unit->hba->chs)
				return (CHS_GEOMETRY(unit->hba->chs, sa,
					unit->capacity));

			return (HBA_SETGEOM(heads, sectors));
		case SCSI_CAP_ARQ:
			return (unit->scsi_auto_req);
		case SCSI_CAP_TAGGED_QING:
			return (unit->scsi_tagq);
		default:
			return (UNDEFINED);
	}
}

int
chs_setcap(
    register struct scsi_address *const sa,
    char *cap,
    int value,
    int tgtonly)
{
	int cap_idx;
	register chs_unit_t *unit;

	ASSERT(sa != NULL);
	unit = CHS_SA2UNIT(sa);
	ASSERT(unit != NULL);
	ASSERT(unit->hba != NULL);
	ASSERT(CHS_SCSI(unit->hba));

	if (!chs_capchk(cap, tgtonly, &cap_idx))
		return (UNDEFINED);

	switch (cap_idx) {
	case SCSI_CAP_TAGGED_QING:
		if (tgtonly) {
			unit->scsi_tagq = (u_int)value;
			return (1);
		}
		break;
	case SCSI_CAP_ARQ:
		if (tgtonly) {
			unit->scsi_auto_req = (u_int)value;
			return (1);
		}
		break;
	case SCSI_CAP_SECTOR_SIZE:
		unit->dma_lim.dlim_granular = (u_int)value;
		return (1);
	case SCSI_CAP_TOTAL_SECTORS:
		unit->capacity = (u_int)value;
		return (1);
	case SCSI_CAP_GEOMETRY:
		/*FALLTHROUGH*/
	default:
		break;
	}
	return (0);
}
