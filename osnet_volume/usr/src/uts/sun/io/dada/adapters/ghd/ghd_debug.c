/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)ghd_debug.c 1.5	98/01/08 SMI"

#include <sys/dada/adapters/ghd/ghd.h>
#include <sys/note.h>



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

_NOTE(DATA_READABLE_WITHOUT_LOCK(ghd_debug_flags))

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

static void
ghd_dump_ccc(ccc_t *P)
{
	ghd_err("nextp 0x%x tmrp 0x%x label 0x%x &mutex 0x%x\n",
		P->ccc_nextp, P->ccc_tmrp, P->ccc_label, &P->ccc_activel_mutex);
	ghd_err("&activel 0x%x dip 0x%x iblock 0x%x\n",
		&P->ccc_activel, P->ccc_hba_dip, P->ccc_iblock);
	ghd_err("softid 0x%x &hba_mutext 0x%x\n poll 0x%x\n",
		P->ccc_soft_id, &P->ccc_hba_mutex);
	ghd_err("&devs 0x%x &waitq_mutex 0x%x &waitq 0x%x\n",
		&P->ccc_devs, &P->ccc_waitq_mutex, &P->ccc_waitq);
	ghd_err("dq softid 0x%x &dq_mutex 0x%x &doneq 0x%x\n",
		P->ccc_doneq_softid, &P->ccc_doneq_mutex, &P->ccc_doneq);
	ghd_err("handle 0x%x &ccballoc 0x%x\n",
		P->ccc_hba_handle, &P->ccc_ccballoc);
}
#endif
