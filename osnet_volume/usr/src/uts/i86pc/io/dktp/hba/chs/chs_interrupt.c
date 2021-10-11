/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_interrupt.c	1.6	99/01/11 SMI"

#include "chs.h"




/*
 * Debug Interrupt Routine: this debug routine removes the overhead
 * of setting up dma requests, minimizing the amount of time spent
 * between commands. For performance profiling.
 */

#ifdef DADKIO_RWCMD_READ
#if defined(CHS_DEBUG)
void
chs_dintr(register chs_ccb_t *ccb,
	register chs_t *chs,
	register struct cmpkt *cmpkt)
{
	long	seccnt;
	off_t	coff;
	int	i;
	int	num_segs = 0;

	/* subtract previous i/o operation */
	cmpkt->cp_byteleft -= cmpkt->cp_bytexfer;
	cmpkt->cp_retry = 0;
	seccnt = cmpkt->cp_bytexfer >> SCTRSHFT;
	cmpkt->cp_secleft -= seccnt;
	cmpkt->cp_srtsec += seccnt;

	/* calculate next i/o operation */
	if (cmpkt->cp_secleft > 0) {
		/* adjust scatter-gather length */
		if (cmpkt->cp_bytexfer > cmpkt->cp_byteleft) {
			cmpkt->cp_bytexfer = cmpkt->cp_byteleft;
		}
		for (i = cmpkt->cp_bytexfer;
			i > 0;
			i -= ccb->ccb_sg_list[num_segs++].data02_len32)
				;
		if (i < 0)
			ccb->ccb_sg_list[num_segs-1].data02_len32 += i;

		seccnt = cmpkt->cp_bytexfer >> SCTRSHFT;
		coff = RWCMDP->blkaddr + cmpkt->cp_srtsec;
	CHS_IOSETUP(chs, ccb, num_segs, seccnt, coff, 0, 1);

		if (chs_sendcmd(chs, ccb, 0) == DDI_FAILURE) {
			/* complete packet */
			cmpkt->cp_byteleft = cmpkt->cp_bytexfer;
			mutex_exit(&chs->mutex);
			(*cmpkt->cp_callback)(cmpkt);
			mutex_enter(&chs->mutex);
		}
	} else {
		/* complete packet */
		cmpkt->cp_byteleft = cmpkt->cp_bytexfer;
		mutex_exit(&chs->mutex);
		(*cmpkt->cp_callback)(cmpkt);
		mutex_enter(&chs->mutex);
	}
}
#endif
#endif

/*
 * Autovector interrupt entry point.  Passed to ddi_add_intr() in
 * chs_attach().
 */

/*
 * This interrupt handler is not only called at interrupt time by the
 * cpu interrupt thread, it is also called from chs_{scsi,dac}_transport()
 * through chs_pollstat().  In the latter case, although the interrupts are
 * disabled, some might have come in because operations initiated earlier
 * could have completed and raised the interrupt.
 */
u_int
chs_intr(caddr_t arg)
{
	register chs_ccb_t *ccb;
	register chs_t *chs = (chs_t *)arg;
	chs_stat_t hw_stat;
	chs_stat_t *hw_statp = &hw_stat;
	int ret = DDI_INTR_UNCLAIMED;

	ASSERT(chs != NULL);
	mutex_enter(&chs->mutex);

	for (; CHS_IREADY(chs); ) {
		/*
		 * The following inb()/inw() seems wasteful if the above
		 * stat_id does not correspond to an outstanding cmdid
		 * (statistically minority of cases).  However, in
		 * the absolute majority of the cases it does match
		 * some outstanding cmdid and by reading the status
		 * earlier and setting the BMIC registers ASAP the
		 * chances of iterating this loop is increased.
		 */
		while (CHS_GET_ISTAT(chs, hw_statp, 1) != 0) {

			ret = DDI_INTR_CLAIMED;

			if (hw_statp->stat_id == CHS_INVALID_CMDID) {
				continue;
			}

			if (hw_statp->stat_id == CHS_NEPTUNE_CMDID) {
				cmn_err(CE_WARN, "?chs_intr: Possible Neptune "
				    "chipset errata");
				continue;
			}

#ifdef DEBUG
			if (hw_statp->stat_id >= chs->max_cmd) {
				cmn_err(CE_WARN,
					"chs_intr: stat_id=%d, max_cmd=%d",
					hw_statp->stat_id, chs->max_cmd);
				continue;
			}
#endif

			ccb = (chs_ccb_t *)chs_process_status(chs, hw_statp);
			if (ccb == NULL) {
				MDBG1(("chs_intr: bad stat_id %d,"
					" spurious intr",
						hw_statp->stat_id));
				continue;
			}
			QueueAdd(&chs->doneq, &ccb->ccb_q, ccb);

		}
	}


	/*
	 * Check to see if we got any other source of interrupt and if so
	 * clear it
	 */
	if (CHS_GET_ISTAT(chs, NULL, 2) != 0) {
		ret = DDI_INTR_CLAIMED;
	}

	chs_process_doneq(chs, &chs->doneq);

	mutex_exit(&chs->mutex);

	return (ret);
}

/*
 * given a hw_stat, get its ccb, and do chkstatus on it
 * update the ccb's hw_stat to hw_stat
 * return pointer to the ccb i, returns NULL if failed to find a ccb
 * corresponding to the stat id of hw_stat.
 */
chs_ccb_t *
chs_process_status(chs_t *chs, chs_stat_t *hw_statp)
{

	register chs_ccb_stk_t *ccb_stk;
	chs_ccb_t *ccb;
	struct scsi_pkt *pkt;
	struct cmpkt *cmpkt;

	ASSERT(mutex_owned(&chs->mutex));
	ASSERT(chs->ccb_stk != NULL);
	ccb_stk = chs->ccb_stk + hw_statp->stat_id;
	ccb = ccb_stk->ccb;
	if (ccb == NULL) {
		cmn_err(CE_NOTE, "returning null form process_stat\n");
		return (NULL);
	}
	ASSERT(hw_statp->stat_id == ccb->ccb_cmdid);
	ASSERT(hw_statp->stat_id == ccb->ccb_stat_id);
	ccb->ccb_hw_stat = *hw_statp;
	ccb_stk->next = chs->free_ccb - chs->ccb_stk;
	chs->free_ccb = ccb_stk;
	ccb_stk->ccb = NULL;

	if (ccb->type == CHS_SCSI_CTYPE) {
		pkt = (struct scsi_pkt *)ccb->ccb_ownerp;
		ASSERT(pkt != NULL);
		sema_v(&chs->scsi_ncdb_sema);
		CHS_SCSI_CHKERR(chs, ccb,
			pkt, hw_statp->status);

	} else {
		cmpkt = (struct cmpkt *)ccb->ccb_ownerp;
		ASSERT(cmpkt != NULL);

		if (CHS_DAC_CHECK_STATUS(chs, cmpkt,
			hw_statp->status) != CHS_SUCCESS) {
			cmn_err(CE_CONT, "?chs_intr: opcode="
				"0x%x, error status=0x%x",
					ccb->ccb_opcode, hw_statp->status);
		}
	}
	return (ccb);
}
void
chs_process_doneq(chs_t *chs, Que_t *doneqp)
{
	chs_ccb_t *ccb;
	struct scsi_pkt *pkt;
	struct cmpkt *cmpkt;


	while (ccb = (chs_ccb_t *)QueueRemove(doneqp)) {
		if (ccb->type == CHS_SCSI_CTYPE) {
			pkt = (struct scsi_pkt *)ccb->ccb_ownerp;
			if (pkt && !(pkt->pkt_flags & FLAG_NOINTR) &&
			    pkt->pkt_comp != NULL) {
				mutex_exit(&chs->mutex);
				(*pkt->pkt_comp)(pkt);
				mutex_enter(&chs->mutex);
			}
		} else {
			cmpkt = (struct cmpkt *)ccb->ccb_ownerp;
			ASSERT(cmpkt != NULL);

#ifdef	DADKIO_RWCMD_READ
#if defined(CHS_DEBUG)
			if (cmpkt->cp_passthru && cmpkt->cp_bp &&
			    cmpkt->cp_bp->b_back && (RWCMDP->flags & 0x8000))
				chs_dintr(ccb, chs, cmpkt);
			else
#endif
#endif
				{
				mutex_exit(&chs->mutex);
				(*cmpkt->cp_callback)(cmpkt);
				mutex_enter(&chs->mutex);
				}
		}
	}
}
