/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ghd_debug.c	1.11	99/04/09 SMI"

#include "ghd.h"



#ifndef	GHD_DEBUG
ulong	ghd_debug_flags = 0;
#else
ulong	ghd_debug_flags = GDBG_FLAG_ERROR
		/*	| GDBG_FLAG_WAITQ	*/
		/*	| GDBG_FLAG_INTR	*/
		/*	| GDBG_FLAG_START	*/
		/*	| GDBG_FLAG_WARN	*/
		/*	| GDBG_FLAG_DMA		*/
		/*	| GDBG_FLAG_PEND_INTR	*/
		/*	| GDBG_FLAG_START	*/
		/*	| GDBG_FLAG_PKT		*/
		/*	| GDBG_FLAG_INIT	*/
			;
#endif

void
ghd_err(char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vcmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

#ifdef GHD_DEBUG
void	prom_printf(char *, ...);
#define	PRF	prom_printf

static void
ghd_dump_ccc(ccc_t *P)
{
	PRF("nextp 0x%x tmrp 0x%x label 0x%x &mutex 0x%x\n",
		P->ccc_nextp, P->ccc_tmrp, P->ccc_label, &P->ccc_activel_mutex);
	PRF("&activel 0x%x dip 0x%x iblock 0x%x\n",
		&P->ccc_activel, P->ccc_hba_dip, P->ccc_iblock);
	PRF("softid 0x%x &hba_mutext 0x%x\n poll 0x%x\n",
		P->ccc_soft_id, &P->ccc_hba_mutex);
	PRF("&devs 0x%x &waitq_mutex 0x%x &waitq 0x%x\n",
		&P->ccc_devs, &P->ccc_waitq_mutex, &P->ccc_waitq);
	PRF("waitq_freezetime 0x%x waitq_freezedelay %d\n",
		&P->ccc_waitq_freezetime, &P->ccc_waitq_freezedelay);
	PRF("dq softid 0x%x &dq_mutex 0x%x &doneq 0x%x\n",
		P->ccc_doneq_softid, &P->ccc_doneq_mutex, &P->ccc_doneq);
	PRF("handle 0x%x &ccballoc 0x%x\n",
		P->ccc_hba_handle, &P->ccc_ccballoc);
	PRF("hba_reset_notify_callback 0x%x notify_list 0x%x mutex 0x%x\n",
		P->ccc_hba_reset_notify_callback, &P->ccc_reset_notify_list,
		P->ccc_reset_notify_mutex);
}


static void
ghd_dump_gcmd(gcmd_t *P)
{

	PRF("cmd_q nextp 0x%x prevp 0x%x private 0x%x\n",
		P->cmd_q.l2_nextp, P->cmd_q.l2_prevp, P->cmd_q.l2_private);
	PRF("state %d wq lev %d flags 0x%x\n", P->cmd_state,
		P->cmd_waitq_level, P->cmd_flags);
	PRF("timer Q nextp 0x%x prevp 0x%x private 0x%x\n",
		P->cmd_timer_link.l2_nextp, P->cmd_timer_link.l2_prevp,
		P->cmd_timer_link.l2_private);

	PRF("start time 0x%x timeout 0x%x hba private 0x%x pktp 0x%x\n",
		P->cmd_start_time, P->cmd_timeout, P->cmd_private,
		P->cmd_pktp);
	PRF("gtgtp 0x%x dma_flags 0x%x dma_handle 0x%x dmawin 0x%x "
		"dmaseg 0x%x\n", P->cmd_gtgtp, P->cmd_dma_flags,
			P->cmd_dma_handle, P->cmd_dmawin, P->cmd_dmaseg);
	PRF("wcount %d windex %d ccount %d cindex %d\n",
		P->cmd_wcount, P->cmd_windex, P->cmd_ccount, P->cmd_cindex);
	PRF("totxfer %d\n", P->cmd_totxfer);
}
#endif
