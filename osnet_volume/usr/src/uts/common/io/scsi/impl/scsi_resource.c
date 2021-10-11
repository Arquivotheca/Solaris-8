/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_resource.c	1.54	98/05/06 SMI"

#include <sys/scsi/scsi.h>
#include <sys/vtrace.h>


#define	A_TO_TRAN(ap)	((ap)->a_hba_tran)
#define	P_TO_TRAN(pkt)	((pkt)->pkt_address.a_hba_tran)
#define	P_TO_ADDR(pkt)	(&((pkt)->pkt_address))

/*
 * Callback id
 */
uintptr_t scsi_callback_id = 0;



struct buf *
scsi_alloc_consistent_buf(struct scsi_address *ap,
    struct buf *in_bp, size_t datalen, uint_t bflags,
    int (*callback)(caddr_t), caddr_t callback_arg)
{
	dev_info_t	*pdip;
	struct		buf *bp;
	int		kmflag;

	TRACE_0(TR_FAC_SCSI_RES, TR_SCSI_ALLOC_CONSISTENT_BUF_START,
		"scsi_alloc_consistent_buf_start");

	if (!in_bp) {
		kmflag = (callback == SLEEP_FUNC) ? KM_SLEEP : KM_NOSLEEP;
		if ((bp = getrbuf(kmflag)) == NULL) {
			goto no_resource;
		}
	} else
		bp = in_bp;

	bp->b_un.b_addr = 0;
	if (datalen) {
		pdip = (A_TO_TRAN(ap))->tran_hba_dip;

		while (ddi_iopb_alloc(pdip, (ddi_dma_lim_t *)0, datalen,
		    &bp->b_un.b_addr)) {
			if (callback == SLEEP_FUNC) {
				delay(drv_usectohz(10000));
			} else {
				if (!in_bp)
					freerbuf(bp);
				goto no_resource;
			}
		}
		bp->b_flags |= bflags;
	}
	bp->b_bcount = datalen;
	bp->b_resid = 0;

	TRACE_0(TR_FAC_SCSI_RES, TR_SCSI_ALLOC_CONSISTENT_BUF_END,
		"scsi_alloc_consistent_buf_end");
	return (bp);

no_resource:

	if (callback != NULL_FUNC && callback != SLEEP_FUNC) {
		ddi_set_callback(callback, callback_arg,
			&scsi_callback_id);
	}
	TRACE_0(TR_FAC_SCSI_RES,
	    TR_SCSI_ALLOC_CONSISTENT_BUF_RETURN1_END,
	    "scsi_alloc_consistent_buf_end (return1)");
	return (NULL);
}

void
scsi_free_consistent_buf(struct buf *bp)
{
	TRACE_0(TR_FAC_SCSI_RES, TR_SCSI_FREE_CONSISTENT_BUF_START,
		"scsi_free_consistent_buf_start");
	if (!bp)
		return;
	if (bp->b_un.b_addr)
		ddi_iopb_free((caddr_t)bp->b_un.b_addr);
	freerbuf(bp);
	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
	TRACE_0(TR_FAC_SCSI_RES, TR_SCSI_FREE_CONSISTENT_BUF_END,
		"scsi_free_consistent_buf_end");
}

struct scsi_pkt *
scsi_init_pkt(struct scsi_address *ap, struct scsi_pkt *in_pktp,
    struct buf *bp, int cmdlen, int statuslen, int pplen,
    int flags, int (*callback)(caddr_t), caddr_t callback_arg)
{
	struct scsi_pkt *pktp;
	scsi_hba_tran_t *tranp = ap->a_hba_tran;
	int		(*func)(caddr_t);

	TRACE_5(TR_FAC_SCSI_RES, TR_SCSI_INIT_PKT_START,
"scsi_init_pkt_start: addr %p in_pktp %p cmdlen %d statuslen %d pplen %d",
	    ap, in_pktp, cmdlen, statuslen, pplen);
#if defined(i386)
	if (flags & PKT_CONSISTENT_OLD) {
		flags &= ~PKT_CONSISTENT_OLD;
		flags |= PKT_CONSISTENT;
	}
#endif  /* defined(i386) */

	func = (callback == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC;

	pktp = (*tranp->tran_init_pkt) (ap, in_pktp, bp, cmdlen,
		statuslen, pplen, flags, func, NULL);
	if (pktp == NULL) {
		if (callback != NULL_FUNC && callback != SLEEP_FUNC) {
			ddi_set_callback(callback, callback_arg,
				&scsi_callback_id);
		}
	}

	TRACE_1(TR_FAC_SCSI_RES, TR_SCSI_INIT_PKT_END,
		"scsi_init_pkt_end: pktp %x", pktp);
	return (pktp);
}

void
scsi_destroy_pkt(struct scsi_pkt *pkt)
{
	struct scsi_address	*ap = P_TO_ADDR(pkt);

	TRACE_1(TR_FAC_SCSI_RES, TR_SCSI_DESTROY_PKT_START,
		"scsi_destroy_pkt_start: pkt %x", pkt);

	(*A_TO_TRAN(ap)->tran_destroy_pkt)(ap, pkt);

	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}

	TRACE_0(TR_FAC_SCSI_RES, TR_SCSI_DESTROY_PKT_END,
		"scsi_destroy_pkt_end");
}


/*
 *	Generic Resource Allocation Routines
 */

struct scsi_pkt *
scsi_resalloc(struct scsi_address *ap, int cmdlen, int statuslen,
    opaque_t dmatoken, int (*callback)())
{
	register struct	scsi_pkt *pkt;
	register scsi_hba_tran_t *tranp = ap->a_hba_tran;
	register int			(*func)(caddr_t);

	func = (callback == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC;

	pkt = (*tranp->tran_init_pkt) (ap, NULL, (struct buf *)dmatoken,
		cmdlen, statuslen, 0, 0, func, NULL);
	if (pkt == NULL) {
		if (callback != NULL_FUNC && callback != SLEEP_FUNC) {
			ddi_set_callback(callback, NULL, &scsi_callback_id);
		}
	}

	return (pkt);
}

struct scsi_pkt *
scsi_pktalloc(struct scsi_address *ap, int cmdlen, int statuslen,
    int (*callback)())
{
	struct scsi_pkt		*pkt;
	struct scsi_hba_tran	*tran = ap->a_hba_tran;
	register int			(*func)(caddr_t);

	func = (callback == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC;

	pkt = (*tran->tran_init_pkt) (ap, NULL, NULL, cmdlen,
		statuslen, 0, 0, func, NULL);
	if (pkt == NULL) {
		if (callback != NULL_FUNC && callback != SLEEP_FUNC) {
			ddi_set_callback(callback, NULL, &scsi_callback_id);
		}
	}

	return (pkt);
}

struct scsi_pkt *
scsi_dmaget(struct scsi_pkt *pkt, opaque_t dmatoken, int (*callback)())
{
	struct scsi_pkt		*new_pkt;
	register int		(*func)(caddr_t);

	func = (callback == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC;

	new_pkt = (*P_TO_TRAN(pkt)->tran_init_pkt) (&pkt->pkt_address,
		pkt, (struct buf *)dmatoken,
		0, 0, 0, 0, func, NULL);
	ASSERT(new_pkt == pkt || new_pkt == NULL);
	if (new_pkt == NULL) {
		if (callback != NULL_FUNC && callback != SLEEP_FUNC) {
			ddi_set_callback(callback, NULL, &scsi_callback_id);
		}
	}

	return (new_pkt);
}


/*
 *	Generic Resource Deallocation Routines
 */

void
scsi_dmafree(struct scsi_pkt *pkt)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	(*A_TO_TRAN(ap)->tran_dmafree)(ap, pkt);

	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
}

void
scsi_sync_pkt(struct scsi_pkt *pkt)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	(*A_TO_TRAN(ap)->tran_sync_pkt)(ap, pkt);
}

void
scsi_resfree(struct scsi_pkt *pkt)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	(*A_TO_TRAN(ap)->tran_destroy_pkt)(ap, pkt);

	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
}
