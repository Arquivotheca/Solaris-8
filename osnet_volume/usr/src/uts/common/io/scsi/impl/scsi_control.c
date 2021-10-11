/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_control.c	1.16	96/06/07 SMI"

/*
 * Generic Abort, Reset and Misc Routines
 */

#include <sys/scsi/scsi.h>


#define	A_TO_TRAN(ap)	(ap->a_hba_tran)

int
scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	return (*A_TO_TRAN(ap)->tran_abort)(ap, pkt);
}

int
scsi_reset(struct scsi_address *ap, int level)
{
	return (*A_TO_TRAN(ap)->tran_reset)(ap, level);
}

int
scsi_reset_notify(struct scsi_address *ap, int flag,
	void (*callback)(caddr_t), caddr_t arg)
{
	if ((A_TO_TRAN(ap)->tran_reset_notify) == NULL) {
		return (DDI_FAILURE);
	}
	return (*A_TO_TRAN(ap)->tran_reset_notify)(ap, flag, callback, arg);
}

int
scsi_clear_task_set(struct scsi_address *ap)
{
	if ((A_TO_TRAN(ap)->tran_clear_task_set) == NULL) {
		return (-1);
	}
	return (*A_TO_TRAN(ap)->tran_clear_task_set)(ap);
}

int
scsi_terminate_task(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	if ((A_TO_TRAN(ap)->tran_terminate_task) == NULL) {
		return (-1);
	}
	return (*A_TO_TRAN(ap)->tran_terminate_task)(ap, pkt);
}

/*
 * Other Misc Routines
 */

int
scsi_clear_aca(struct scsi_address *ap)
{
	if ((A_TO_TRAN(ap)->tran_clear_aca) == NULL) {
		return (-1);
	}
	return (*A_TO_TRAN(ap)->tran_clear_aca)(ap);
}
