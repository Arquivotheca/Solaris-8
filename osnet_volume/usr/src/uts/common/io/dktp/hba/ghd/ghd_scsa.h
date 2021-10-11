/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GHD_SCSA_H
#define	_GHD_SCSA_H

#pragma	ident	"@(#)ghd_scsa.h	1.6	99/04/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/scsi/scsi.h>

/*
 * This really belongs in some sort of scsa include file since
 * it's used by the getcap/setcap interface.
 */
#define	HBA_SETGEOM(hd, sec) (((hd) << 16) | (sec))


struct scsi_pkt	*ghd_tran_init_pkt(ccc_t *cccp, struct scsi_address *ap,
			struct scsi_pkt *pktp, struct buf *bp, int cmdlen,
			int statuslen, int tgtlen, int flags,
			int (*callback)(), caddr_t arg, int ccblen,
			ddi_dma_lim_t *sg_limitp);

void		 ghd_tran_sync_pkt(struct scsi_address *ap,
			struct scsi_pkt *pktp);

void		 ghd_pktfree(ccc_t *cccp, struct scsi_address *ap,
			struct scsi_pkt *pktp);

struct scsi_pkt *ghd_tran_init_pkt_attr(ccc_t *cccp, struct scsi_address *ap,
			struct scsi_pkt *pktp, struct buf *bp,
			int cmdlen, int statuslen, int tgtlen,
			int flags, int (*callback)(),
			caddr_t arg, int ccblen,
			ddi_dma_attr_t *sg_attrp);

#ifdef	__cplusplus
}
#endif

#endif  /* _GHD_SCSA_H */
