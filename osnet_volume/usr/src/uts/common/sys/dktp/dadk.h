/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_DADK_H
#define	_SYS_DKTP_DADK_H

#pragma ident	"@(#)dadk.h	1.7	99/03/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	dadk {
	struct tgdk_ext	*dad_extp;	/* back pointer to ext data	*/
	struct scsi_device *dad_sd;	/* back pointer to SCSI_DEVICE 	*/

	struct  tgdk_geom dad_logg;	/* logical disk geometry 	*/
	struct  tgdk_geom dad_phyg;	/* physical disk geometry 	*/

	unsigned dad_rmb : 1;		/* removable device		*/
	unsigned dad_rdonly : 1;	/* read only device		*/
	unsigned dad_cdrom : 1;		/* cdrom device			*/
	unsigned dad_resv : 5;
	unsigned char dad_type;		/* device type			*/
	unsigned char dad_ctype;	/* controller type 		*/

	short	 dad_secshf;
	short	 dad_blkshf;

	opaque_t dad_bbhobjp;		/* bbh object ptr		*/
	opaque_t dad_flcobjp;		/* flow control object ptr	*/
	opaque_t dad_ctlobjp;		/* controller object ptr	*/
	struct	tgcom_obj dad_com;	/* com object for flowctrl	*/
	enum dkio_state dad_iostate;	/* ejected/inserted		*/
	kmutex_t	dad_mutex;	/* protect dad_state		*/
	kcondvar_t	dad_state_cv;	/* condition variable for state */
	uchar_t		dad_thread_cnt;	/* reference count on removable	*/
					/* - disk state watcher thread	*/
};

#define	DAD_SECSIZ	dad_phyg.g_secsiz

/*
 * Local definitions, for clarity of code
 */

/*
 * Parameters
 */
#define	DADK_BSY_TIMEOUT	(drv_usectohz(5 * 1000000))
#define	DADK_IO_TIME		35
#define	DADK_RETRY_COUNT	5
#define	DADK_SILENT		1

#define	PKT2DADK(pktp)	((struct dadk *)(pktp)->cp_dev_private)

/*
 * packet action codes
 */
#define	COMMAND_DONE		0
#define	COMMAND_DONE_ERROR	1
#define	QUE_COMMAND		2
#define	QUE_SENSE		3
#define	JUST_RETURN		4

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_DADK_H */
