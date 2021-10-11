/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * SCSI device structure.
 *
 *	All SCSI target drivers will have one of these per target/lun.
 *	It will be created by a parent device and stored as driver private
 *	data in that device's dev_info_t (and thus can be retrieved by
 *	the function ddi_get_driver_private).
 */

#ifndef	_SYS_SCSI_CONF_DEVICE_H
#define	_SYS_SCSI_CONF_DEVICE_H

#pragma ident	"@(#)device.h	1.20	96/06/07 SMI"

#include <sys/scsi/scsi_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct scsi_device {
	/*
	 * Routing info for this device.
	 */

	struct scsi_address	sd_address;

	/*
	 * Cross-reference to our dev_info_t.
	 */

	dev_info_t		*sd_dev;

	/*
	 * Mutex for this device, initialized by
	 * parent prior to calling probe or attach
	 * routine.
	 */

	kmutex_t		sd_mutex;

	/*
	 * Reserved, do not use.
	 */

	void			*sd_reserved;


	/*
	 * If scsi_slave is used to probe out this device,
	 * a scsi_inquiry data structure will be allocated
	 * and an INQUIRY command will be run to fill it in.
	 *
	 * The allocation will be done via ddi_iopb_alloc,
	 * so any manual freeing may be done by ddi_iopb_free.
	 */

	struct scsi_inquiry	*sd_inq;


	/*
	 * Place to point to an extended request sense buffer.
	 * The target driver is responsible for managing this.
	 */

	struct scsi_extended_sense	*sd_sense;

	/*
	 * More detailed information is 'private' information, i.e., is
	 * only pertinent to Target drivers.
	 */

	caddr_t			sd_private;

};


#ifdef	_KERNEL
#ifdef	__STDC__
extern int scsi_slave(struct scsi_device *devp, int (*callback)());
extern int scsi_probe(struct scsi_device *devp, int (*callback)());
extern void scsi_unslave(struct scsi_device *devp);
extern void scsi_unprobe(struct scsi_device *devp);
#else	/* __STDC__ */
extern int scsi_slave();
extern void scsi_unslave();
extern int scsi_probe();
extern void scsi_unprobe();
#endif	/* __STDC__ */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_CONF_DEVICE_H */
