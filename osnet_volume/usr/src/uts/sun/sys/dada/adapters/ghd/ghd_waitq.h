/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _GHD_WAITQ_H
#define	_GHD_WAITQ_H

#pragma ident	"@(#)ghd_waitq.h	1.6	97/12/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * there's a waitq_t per target device and one per HBA
 */

typedef struct ghd_q {
	struct ghd_q *Q_nextp;	/* ptr to next level of queuing */
	L2el_t	Q_qhead;	/* Q of waiting gcmds */
	long	Q_nactive;	/* current # of outstanding gcmds */
	long	Q_maxactive;	/* max gcmds to release concurrently */
} Q_t;

#define	GHD_WAITQ_INIT(qp, nxtp, maxactive)	\
	(L2_INIT(&(qp)->Q_qhead), 		\
	(qp)->Q_nextp = (nxtp),			\
	(qp)->Q_nactive = 0,			\
	(qp)->Q_maxactive = (maxactive))
/*
 * one per target device
 */
typedef struct ghd_device {
	Q_t	gd_waitq;	/* the queue structure for this device */
	L1el_t	gd_devlist;	/* all gdevs for a HBA are linked together */
	uint32_t gd_target;	/*  ... and are located by searching for */
	uint32_t gd_lun;	/*  ... a match on the (target,lun) values */

	L1_t 	gd_ilist;	/* linked list of instances for this device */
	uint32_t gd_ninstances;	/* # of instances for this device */
} gdev_t;

#define	GDEV_QHEAD(gdevp)	((gdevp)->gd_waitq.Q_qhead)
#define	GDEV_NACTIVE(gdevp)	((gdevp)->gd_waitq.Q_nactive)
#define	GDEV_MAXACTIVE(gdevp)	((gdevp)->gd_waitq.Q_maxactive)

/*
 * Be careful, this macro assumes there's a least one
 * target instance attached to this dev structure, Otherwise, l1_headp
 * is NULL.
 */
#define	GDEVP2GTGTP(gdevp)	\
	(gtgt_t *)((gdevp)->gd_ilist.l1_headp->le_datap)

#define	GDEV_NEXTP(gdevp)						\
	((gdevp)->gd_devlist.le_nextp					\
		? (gdev_t *)((gdevp)->gd_devlist.le_nextp->le_datap)	\
		: (gdev_t *)NULL)

#define	GDEV_QATTACH(gdevp, cccp, max)	{				\
	GHD_WAITQ_INIT(&(gdevp)->gd_waitq, &(cccp)->ccc_waitq, (max));	\
	L1EL_INIT(&gdevp->gd_devlist);					\
	L1HEADER_INIT(&gdevp->gd_ilist);				\
	/* add the per device structure to the HBA's device list */	\
	L1_add(&(cccp)->ccc_devs, &(gdevp)->gd_devlist, (gdevp));	\
}

#define	GDEV_QDETACH(gdevp, cccp)			\
	L1_delete(&(cccp)->ccc_devs, &(gdevp)->gd_devlist)

/*
 * GHD target structure, one per attached target driver instance
 */
typedef	struct	ghd_target_instance {
	L1el_t	 gt_ilist;	/* list of other instances for this device */
	gdev_t	*gt_gdevp;	/* ptr to info shared by all instances */
	uint32_t gt_maxactive;	/* max gcmds to release concurrently */
	void	*gt_hba_private; /* ptr to soft state of this HBA instance */
	void	*gt_tgt_private; /* ptr to soft state of this target instance */
	size_t	 gt_size;	/* size including tgt_private */
	uint32_t gt_target;	/* target number of this instance */
	uint32_t gt_lun;	/* LUN of this instance */
} gtgt_t;

#define	GTGTP2TARGET(gtgtp)	((gtgtp)->gt_tgt_private)
#define	GTGTP2HBA(gtgtp)	((gtgtp)->gt_hba_private)

#define	GTGT_INIT(gtgtp)	L1EL_INIT(&(gtgtp)->gt_ilist)

/* Add the per instance structure to the per device list  */
#define	GTGT_ATTACH(gtgtp, gdevp)	{				\
	(gdevp)->gd_ninstances++;					\
	L1_add(&(gdevp)->gd_ilist, &(gtgtp)->gt_ilist, (gtgtp));	\
}


/* remove this per-instance-structure from the device list */
#define	GTGT_DEATTACH(gtgtp, gdevp)	{			\
	(gdevp)->gd_ninstances--;				\
	L1_delete(&(gdevp)->gd_ilist, &(gtgtp)->gt_ilist);	\
}



#ifdef	__cplusplus
}
#endif

#endif /* _GHD_WAITQ_H */
