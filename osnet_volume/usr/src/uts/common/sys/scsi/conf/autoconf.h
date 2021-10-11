/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SCSI_CONF_AUTOCONF_H
#define	_SYS_SCSI_CONF_AUTOCONF_H

#pragma ident	"@(#)autoconf.h	1.32	99/07/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI subsystem options
 */

/*
 * Following are for debugging purposes (few Sun drivers support this)
 */
#define	SCSI_DEBUG_TGT	0x1	/* debug statements in target drivers */
#define	SCSI_DEBUG_LIB	0x2	/* debug statements in library */
#define	SCSI_DEBUG_HA	0x4	/* debug statements in host adapters */

/*
 * Following are applicable to all interconnects
 */
#define	SCSI_OPTIONS_LINK	0x10	/* Global linked commands */
#define	SCSI_OPTIONS_TAG	0x80	/* Global tagged command support */

/*
 * Following are for parallel SCSI only
 */
#define	SCSI_OPTIONS_DR		0x8	/* Global disconnect/reconnect	*/
#define	SCSI_OPTIONS_SYNC	0x20	/* Global synchronous xfer capability */
#define	SCSI_OPTIONS_PARITY	0x40	/* Global parity support */
#define	SCSI_OPTIONS_FAST	0x100	/* Global FAST scsi support */
#define	SCSI_OPTIONS_WIDE	0x200	/* Global WIDE scsi support */
#define	SCSI_OPTIONS_FAST20	0x400	/* Global FAST20 scsi support */
#define	SCSI_OPTIONS_FAST40	0x800	/* Global FAST40 scsi support */
#define	SCSI_OPTIONS_FAST80	0x1000	/* Global FAST80 scsi support */

/*
 * the following 3 bits are for being able to limit the max. number of LUNs
 * a nexus driver will allow -- "default" means that the adapter will
 * continue its default behaviour
 */
#define	SCSI_OPTIONS_NLUNS_MASK		(0x70000)

#define	SCSI_OPTIONS_NLUNS_DEFAULT	0x00000
#define	SCSI_OPTIONS_NLUNS_1		0x10000
#define	SCSI_OPTIONS_NLUNS_8		0x20000
#define	SCSI_OPTIONS_NLUNS_16		0x30000
#define	SCSI_OPTIONS_NLUNS_32		0x40000

#define	SCSI_OPTIONS_NLUNS(n)		((n) & SCSI_OPTIONS_NLUNS_MASK)

/*
 * SCSI autoconfiguration definitions.
 *
 * The library routine scsi_slave() is provided as a service to target
 * driver to check for existence  and readiness of a SCSI device. It is
 * defined as:
 *
 *	int scsi_slave(struct scsi_device *devp, int (*callback()))
 *
 * where devp is the scsi_device structure passed to the target driver
 * at probe time, and where callback declares whether scsi_slave() can
 * sleep awaiting resources or must return an error if it cannot get
 * resources (callback == SLEEP_FUNC implies that scsi_slave()
 * can sleep - although this
 * does not fully guarantee that resources will become available as
 * some are allocated from the iopbmap which may just be completely
 * full).  The user call also supplies a callback function or NULL_FUNC.
 * In the process of determining the existence of a SCSI device,
 * scsi_slave will allocate space for the sd_inq field of the scsi_device
 * pointed to by devp (if it is non-zero upon entry).
 *
 * scsi_slave() attempts to follow this sequence in order to determine
 * the existence of a SCSI device:
 *
 *	Attempt to send 2 TEST UNIT READY commands to the device.
 *
 *		If that gets a check condition, run a non-extended
 *		REQUEST SENSE command. Ignore the results of it, as
 *		a the non-extended sense information contains only
 *		Vendor Unique error codes (the idea is that during
 *		probe time the nearly invariant first command to a
 *		device will get a Check Condition, and the real reason
 *		is that the device wants to tell you that a SCSI bus
 *		reset just occurred.
 *
 *	Attempt to allocate an inquiry buffer and
 *	run an INQUIRY command (with response data format 0 set).
 *
 *		If that gets a check condition, run another
 *		non-extended REQUEST SENSE command.
 *
 * The library routine scsi_probe() is provided as a service to target
 * driver to check for bare-bones existence of a SCSI device. It is
 * defined as:
 *
 *	int scsi_probe(struct scsi_device *devp, int (*callback()))
 *
 * scsi_probe() only executes an inquiry.
 *
 * Both functions return one of the integer values as defined below:
 */

#define	SCSIPROBE_EXISTS	0	/* device exists, inquiry data valid */
#define	SCSIPROBE_NONCCS	1	/* device exists, no inquiry data */
#define	SCSIPROBE_NORESP	2	/* device didn't respond */
#define	SCSIPROBE_NOMEM		3	/* no space available for structures */
#define	SCSIPROBE_FAILURE	4	/* polled cmnd failure- unspecified */
#define	SCSIPROBE_BUSY		5	/* device was busy */
#define	SCSIPROBE_NOMEM_CB	6
					/*
					 * no space available for structures
					 * but callback request has been
					 * queued
					 */
/*
 * default value for scsi_reset_delay
 */
#define	SCSI_DEFAULT_RESET_DELAY	3000

/*
 * default value for scsi_selection_timeout
 */
#define	SCSI_DEFAULT_SELECTION_TIMEOUT	250

/*
 * Kernel references
 */

#ifdef	_KERNEL
/*
 * Global SCSI config variables
 */
extern int	scsi_host_id, scsi_tag_age_limit;
extern int	scsi_watchdog_tick;


/*
 * Global SCSI options
 */
extern int scsi_options;
extern unsigned int scsi_reset_delay;	/* specified in milli seconds */
extern int scsi_selection_timeout;	/* specified in milli seconds */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_CONF_AUTOCONF_H */
