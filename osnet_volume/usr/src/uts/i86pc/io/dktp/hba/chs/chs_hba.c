/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)chs_hba.c	1.5	99/05/20 SMI"

#include <sys/scsi/scsi.h>
#include <sys/dktp/hba.h>

#include <sys/var.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/byteorder.h>

/*	Local Function Prototype					*/
static void scsi_callback(struct scsi_cbthread *cbt);

/*
 * Local static data
 */
#define	HBA_CB_DISABLE		0x0001
static 	ulong	gsc_options = 0;

/*
 * functions to convert between host format and scsi format
 */
void
scsi_htos_3byte(unchar *ap, ulong nav)
{
	*(ushort *)ap = (ushort)(((nav & 0xff0000) >> 16) | (nav & 0xff00));
	ap[2] = (unchar)nav;
}

void
scsi_htos_long(unchar *ap, ulong niv)
{
	*(ulong *)ap = htonl(niv);
}

void
scsi_htos_short(unchar *ap, ushort nsv)
{
	*(ushort *)ap = htons(nsv);
}

ulong
scsi_stoh_3byte(unchar *ap)
{
	register ulong av = *(ulong *)ap;

	return (((av & 0xff) << 16) | (av & 0xff00) | ((av & 0xff0000) >> 16));
}

ulong
scsi_stoh_long(ulong ai)
{
	return (ntohl(ai));
}

ushort
scsi_stoh_short(ushort as)
{
	return (ntohs(as));
}

int
scsi_iopb_fast_zalloc(caddr_t *listp, dev_info_t *dip, ddi_dma_lim_t *limp,
	u_int len, caddr_t *iopbp)
{
	int 	status;

	status = scsi_iopb_fast_alloc(listp, dip, limp, len, iopbp);
	if (status == DDI_SUCCESS)
		bzero(*iopbp, len);
	return (status);
}

int
scsi_iopb_fast_alloc(caddr_t *listp, dev_info_t *dip, ddi_dma_lim_t *limp,
	u_int len, caddr_t *iopbp)
{
	if (*listp == 0)
		return (ddi_iopb_alloc(dip, limp, len, iopbp));

	*iopbp = *listp;
	*listp = *((caddr_t *)*iopbp);
	return (DDI_SUCCESS);
}

void
scsi_iopb_fast_free(caddr_t *base, caddr_t p)
{
	*(void **)p = *base;
	*base = p;
}

opaque_t
scsi_create_cbthread(ddi_iblock_cookie_t lkarg, int sleep)
{
	register struct scsi_cbthread *cbt;

	cbt = (struct scsi_cbthread *)kmem_zalloc(sizeof (*cbt), sleep);
	if (!cbt)
		return (NULL);

	cv_init(&cbt->cb_cv, NULL, CV_DRIVER, NULL);
	mutex_init(&cbt->cb_mutex, NULL, MUTEX_DRIVER, lkarg);
	cbt->cb_thread = thread_create((caddr_t)NULL, 0, scsi_callback,
		(caddr_t)cbt, 0, &p0, TS_RUN, v.v_maxsyspri - 2);
	if (!cbt->cb_thread) {
		kmem_free(cbt, sizeof (*cbt));
		return (NULL);
	}

	return ((opaque_t)cbt);
}

void
scsi_destroy_cbthread(opaque_t cbdhdl)
{
	register struct scsi_cbthread *cbt = (struct scsi_cbthread *)cbdhdl;

	if (!cbt)
		return;
	mutex_enter(&cbt->cb_mutex);
	cbt->cb_flag |= SCSI_CB_DESTROY;
	cv_signal(&cbt->cb_cv);
	mutex_exit(&cbt->cb_mutex);
}

void
scsi_run_cbthread(opaque_t cbdhdl, struct scsi_cmd *cmd)
{
	register struct scsi_cbthread *cbt = (struct scsi_cbthread *)cbdhdl;

	if (panicstr || !cbt || (gsc_options & HBA_CB_DISABLE) ||
		(cmd->cmd_pkt.pkt_flags & FLAG_IMMEDIATE_CB)) {
		(*cmd->cmd_pkt.pkt_comp)((struct scsi_pkt *)cmd);
		return;
	}

	mutex_enter(&cbt->cb_mutex);
	if (!cbt->cb_head)
		cbt->cb_head = cmd;
	else
		cbt->cb_tail->cmd_cblinkp = cmd;
	cbt->cb_tail = cmd;
	cmd->cmd_cblinkp = NULL;
	cv_signal(&cbt->cb_cv);
	mutex_exit(&cbt->cb_mutex);
}

static void
scsi_callback(struct scsi_cbthread *cbt)
{
	register struct scsi_cmd *cmd;

	mutex_enter(&cbt->cb_mutex);
	for (;;) {
		if (cbt->cb_flag & SCSI_CB_DESTROY) {
			kmem_free(cbt, sizeof (*cbt));
			return;
		}

		for (; cbt->cb_head; ) {
			cmd = cbt->cb_head;
			cbt->cb_head = cmd->cmd_cblinkp;
			if (!cbt->cb_head)
				cbt->cb_tail = NULL;

			mutex_exit(&cbt->cb_mutex);
			(*cmd->cmd_pkt.pkt_comp)((struct scsi_pkt *)cmd);
			mutex_enter(&cbt->cb_mutex);
		}
		cv_wait(&cbt->cb_cv, &cbt->cb_mutex);
	}
}

struct scsi_pkt *
scsi_impl_dmaget(struct scsi_pkt *pkt, opaque_t dmatoken,
    int (*callback)(), caddr_t callback_arg, ddi_dma_lim_t *dmalimp)
{
	register struct buf *bp = (struct buf *)dmatoken;
	register struct scsi_cmd *cmd = (struct scsi_cmd *)pkt;
	int 		flags;
	int 		status;

	if (!cmd->cmd_dmahandle) {
		if (bp->b_flags & B_READ)
			flags = DDI_DMA_READ;
		else
			flags = DDI_DMA_WRITE;

		if (cmd->cmd_flags & PKT_CONSISTENT)
			flags |= DDI_DMA_CONSISTENT;
		if (cmd->cmd_flags & PKT_DMA_PARTIAL)
			flags |= DDI_DMA_PARTIAL;

		status = ddi_dma_buf_setup(PKT2TRAN(pkt)->tran_hba_dip,
		    bp, flags, callback, callback_arg, dmalimp,
		    &cmd->cmd_dmahandle);

		if (status) {
			switch (status) {
			case DDI_DMA_NORESOURCES:
				bp->b_error = 0;
				break;
			case DDI_DMA_TOOBIG:
				bp->b_error = EINVAL;
				break;
			case DDI_DMA_NOMAPPING:
			default:
				bp->b_error = EFAULT;
				break;
			}
			return ((struct scsi_pkt *)NULL);
		}
	} else {
		/*
		 * get next segment
		 */
		status = ddi_dma_nextseg(cmd->cmd_dmawin, cmd->cmd_dmaseg,
		    &cmd->cmd_dmaseg);
		if (status == DDI_SUCCESS)
			return (pkt);
		else if (status == DDI_DMA_STALE)
			return ((struct scsi_pkt *)NULL);

		/* fall through at end of segment */
	}

	/*
	 * move to the next window
	 */
	status = ddi_dma_nextwin(cmd->cmd_dmahandle, cmd->cmd_dmawin,
	    &cmd->cmd_dmawin);
	if (status == DDI_DMA_STALE)
		return ((struct scsi_pkt *)NULL);
	if (status == DDI_DMA_DONE) {
		/*
		 * reset to first window
		 */
		if (ddi_dma_nextwin(cmd->cmd_dmahandle, NULL,
		    &cmd->cmd_dmawin) != DDI_SUCCESS)
			return ((struct scsi_pkt *)NULL);
	}

	/*
	 * get first segment
	 */
	if (ddi_dma_nextseg(cmd->cmd_dmawin, NULL, &cmd->cmd_dmaseg) !=
	    DDI_SUCCESS)
		return ((struct scsi_pkt *)NULL);
	return (pkt);
}

#ifdef SCSI_SYS_DEBUG
void
scsi_dump_cmd(struct scsi_cmd *p)
{
	u_char i;
	u_char *c;
	struct target_private *t;
	struct scsi_pkt *pktp = (struct scsi_pkt *)p;

PRF("cdblen 0x%x scblen 0x%x comp 0x%x flags 0x%x privlen 0x%x pkt_priv 0x%x\n",
	p->cmd_cdblen, p->cmd_scblen & 0xff, p->cmd_pkt.pkt_comp,
	p->cmd_flags, p->cmd_privlen & 0xff,
	((struct scsi_pkt *)p)->pkt_private);

	PRF("cblinkp %x cmd_private %x rqslen %x targ_priv %x\n",
		p->cmd_cblinkp, p->cmd_private,
		p->cmd_rqslen, p->cmd_pkt_private);

	if (p->cmd_pkt_private) {
		t = (struct target_private *)p->cmd_pkt_private;
	PRF("target priv: fltpktp 0x%x bp 0x%x srtsec 0x%x seccnt 0x%x\n",
		t->x_fltpktp, t->x_bp, t->x_srtsec, t->x_seccnt);
PRF("  byteleft 0x%x bytexfer 0x%x retry 0x%x sdevp 0x%x callback 0x%x\n",
		t->x_byteleft, t->x_bytexfer, t->x_retry, t->x_sdevp,
		t->x_callback);
	}

	PRF("scbp blk %x cdbp %x state %x reason %x stats %x resid %x\n",
		p->cmd_pkt.pkt_scbp, p->cmd_pkt.pkt_cdbp,
		p->cmd_pkt.pkt_state, p->cmd_pkt.pkt_reason,
		p->cmd_pkt.pkt_statistics, p->cmd_pkt.pkt_resid);

	PRF("t %d l %d status 0x%x ", pktp->pkt_address.a_target,
	    pktp->pkt_address.a_lun, *pktp->pkt_scbp);

	PRF("pkt_flags: ");
	if (p->cmd_pkt.pkt_flags & FLAG_NODISCON)
		PRF("NODISCON ");
	else
		PRF("DISCON ");

	if (p->cmd_pkt.pkt_flags & FLAG_NOINTR)
		PRF("NOINTR ");
	else
		PRF("INTR ");

	PRF("pkt_state: ");
	if (p->cmd_pkt.pkt_state & STATE_GOT_BUS)
		PRF("SCSI bus arbitration succeeded");
	if (p->cmd_pkt.pkt_state & STATE_GOT_TARGET)
		PRF("Target successfully selected");
	if (p->cmd_pkt.pkt_state & STATE_SENT_CMD)
		PRF("Command successfully sent");
	if (p->cmd_pkt.pkt_state & STATE_XFERRED_DATA)
		PRF("Data transfer took place");
	if (p->cmd_pkt.pkt_state & STATE_GOT_STATUS)
		PRF("SCSI status received");
	if (p->cmd_pkt.pkt_state & STATE_ARQ_DONE)
		PRF("auto rqsense took place");

	PRF("\npkt_reason: ");
	if (p->cmd_pkt.pkt_reason == CMD_CMPLT)
		PRF("Cmd Cmplt");
	if (p->cmd_pkt.pkt_reason == CMD_INCOMPLETE)
		PRF("Incmplt");
	if (p->cmd_pkt.pkt_reason == CMD_DMA_DERR)
		PRF("Dma_err");
	if (p->cmd_pkt.pkt_reason == CMD_TRAN_ERR)
		PRF("Unk err");
	if (p->cmd_pkt.pkt_reason == CMD_RESET)
		PRF("Bus reset");
	if (p->cmd_pkt.pkt_reason == CMD_ABORTED)
		PRF("Cmd aborted");
	if (p->cmd_pkt.pkt_reason == CMD_TIMEOUT)
		PRF("Cmd timeout");
	if (p->cmd_pkt.pkt_reason == CMD_DATA_OVR)
		PRF("Data Overrun");
	if (p->cmd_pkt.pkt_reason == CMD_CMD_OVR)
		PRF("Command Overrun");
	if (p->cmd_pkt.pkt_reason == CMD_STS_OVR)
		PRF("Status Overrun");
	if (p->cmd_pkt.pkt_reason == CMD_BADMSG)
		PRF("Bad Msg");
	if (p->cmd_pkt.pkt_reason == CMD_NOMSGOUT)
		PRF("No Message Out");
	if (p->cmd_pkt.pkt_reason == CMD_XID_FAIL)
		PRF("Extended Identify message rejected");
	if (p->cmd_pkt.pkt_reason == CMD_ABORT_FAIL)
		PRF("Abort Message Rejected");
	if (p->cmd_pkt.pkt_reason == CMD_NOP_FAIL)
		PRF("No Operation message rejected");
	if (p->cmd_pkt.pkt_reason == CMD_PER_FAIL)
		PRF("Message Parity Error message rejected");
	if (p->cmd_pkt.pkt_reason == CMD_BDR_FAIL)
		PRF("Bus Device Reset message rejected");
	if (p->cmd_pkt.pkt_reason == CMD_ID_FAIL)
		PRF("Identify Msg rejected");
	if (p->cmd_pkt.pkt_reason == CMD_UNX_BUS_FREE)
		PRF("Unexpected Bus Free Phase occurred");
	if (p->cmd_pkt.pkt_reason == CMD_TAG_REJECT)
		PRF("Target rejected our tag");

	PRF(" cdb: ");
	c = p->cmd_pkt.pkt_cdbp;
	for (i = 0; i < 12; i++, c++) {
		PRF("%x ", *c & 0xff);
	}
	PRF("\n");
}
#endif
