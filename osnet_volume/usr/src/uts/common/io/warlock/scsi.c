/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi.c	1.1	98/09/15 SMI"

/*
 * scsi.c - Warlock versions of DDI/DKI routines associated with scsi
 *
 * These renditions of the scsi-related DDI/DKI routines give warlock
 * info about control flow which warlock needs in order to do a good
 * job of analysis.
 */
#include <sys/note.h>
#include <sys/scsi/scsi.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/scsi/impl/transport.h>

void
scsi_init()
{
	struct scsi_hba_tran *p;

	p->tran_tgt_init(0, 0, 0, 0);
	p->tran_tgt_probe(0, 0);
	p->tran_tgt_free(0, 0, 0, 0);
	p->tran_add_eventcall(0, 0, 0, 0, 0);
	p->tran_get_eventcookie(0, 0, 0, 0, 0, 0);
	p->tran_post_event(0, 0, 0, 0);
	p->tran_remove_eventcall(0, 0, 0);
}
