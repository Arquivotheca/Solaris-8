/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_RESOURCE_H
#define	_SYS_SCSI_SCSI_RESOURCE_H

#pragma ident	"@(#)scsi_resource.h	1.21	96/09/24 SMI"

#include <sys/scsi/scsi_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI Resource Function Declaratoins
 */

/*
 * Defines for stating preferences in resource allocation
 */

#define	NULL_FUNC	((int (*)())0)
#define	SLEEP_FUNC	((int (*)())1)

#ifdef	_KERNEL
/*
 * Defines for the flags to scsi_init_pkt()
 */
#define	PKT_CONSISTENT	0x0001		/* this is an 'iopb' packet */
#define	PKT_DMA_PARTIAL	0x040000	/* partial xfer ok */

/*
 * Old PKT_CONSISTENT value for binary compatibility with x86 2.1
 */
#define	PKT_CONSISTENT_OLD	0x001000

/*
 * Kernel function declarations
 */

#ifdef	__STDC__

extern struct buf *scsi_alloc_consistent_buf(struct scsi_address *,
    struct buf *, size_t, uint_t, int (*)(caddr_t), caddr_t);

extern struct scsi_pkt *scsi_init_pkt(struct scsi_address *,
    struct scsi_pkt *, struct buf *, int, int, int, int,
    int (*)(caddr_t), caddr_t);

extern void scsi_destroy_pkt(struct scsi_pkt *);

extern void scsi_free_consistent_buf(struct buf *);

extern struct scsi_pkt *scsi_resalloc(struct scsi_address *, int,
    int, opaque_t, int (*)());

extern struct scsi_pkt *scsi_pktalloc(struct scsi_address *, int, int,
    int (*)());

extern struct scsi_pkt *scsi_dmaget(struct scsi_pkt *,
    opaque_t, int (*)());

extern void scsi_dmafree(struct scsi_pkt *);

extern void scsi_sync_pkt(struct scsi_pkt *);

extern void scsi_resfree(struct scsi_pkt *);

#else	/* __STDC__ */
extern struct scsi_pkt	*scsi_init_pkt();
extern void 		scsi_destroy_pkt();
extern struct buf 	*scsi_alloc_consistent_buf();
extern void		scsi_free_consistent_buf();
extern struct scsi_pkt *scsi_resalloc();
extern struct scsi_pkt *scsi_pktalloc();
extern struct scsi_pkt *scsi_dmaget();
extern void		scsi_resfree();
extern void		scsi_dmafree();
extern void		scsi_sync_pkt();
#endif	/* __STDC__ */

/*
 * Preliminary version of the SCSA specification
 * mentioned a routine called scsi_pktfree, which
 * turned out to be semantically equivialent to
 * scsi_resfree.
 */

#define	scsi_pktfree	scsi_resfree

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_RESOURCE_H */
