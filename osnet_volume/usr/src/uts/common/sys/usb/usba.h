/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USBA_H
#define	_SYS_USBA_H

#pragma ident	"@(#)usba.h	1.5	99/11/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/note.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/devops.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/devctl.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/open.h>
#include <sys/kmem.h>

typedef struct usb_opaque *usb_opaque_t;

/*
 * Header file for USBA
 * client driver interfaces
 */
#include <sys/usb/usbai.h>

/*
 * available class codes
 */
#define	HID_SUBCLASS		1
#define	HID_CLASS_CODE		3
#define	MASS_STORAGE_CLASS_CODE 8	/* Mass storage class code */
#define	HUB_CLASS_CODE		9

/* Boot protocol values for keyboard and mouse */
#define	HID_KEYBOARD_PROTOCOL	0x01	/* legacy keyboard */
#define	HID_MOUSE_PROTOCOL	0x02	/* legacy mouse */


/* mass storage class */
#define	MS_RBC_T10_SUB_CLASS	0x1	/* flash */
#define	MS_SFF8020I_SUB_CLASS	0x2	/* CD-ROM */
#define	MS_QIC_157_SUB_CLASS	0x3	/* tape */
#define	MS_UFI_SUB_CLASS	0x4	/* USB Floppy Disk Drive   */
#define	MS_SFF8070I_SUB_CLASS	0x5	/* floppy */
#define	MS_SCSI_SUB_CLASS	0x6	/* transparent scsi */

#define	MS_CBI_WC_PROTOCOL		0x00
					/* USB CBI Protocol with cmp intr */
#define	MS_CBI_PROTOCOL			0x01	/* USB CBI Protocol */
#define	MS_ISD_1999_SILICON_PROTOCOL	0x02	/* ZIP Protocol */
#define	MS_BULK_ONLY_PROTOCOL		0x50	/* USB Bulk Only Protocol */



#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USBA_H */
