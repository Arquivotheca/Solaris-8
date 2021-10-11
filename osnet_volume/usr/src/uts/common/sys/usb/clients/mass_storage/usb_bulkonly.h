/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_BULKONLY_H
#define	_SYS_USB_BULKONLY_H

#pragma ident	"@(#)usb_bulkonly.h	1.2	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * usb_bulkonly.h: This header file provides the data structures
 * and variable definitions for the mass storage bulk only protocol.
 * (See Universal Serial Bus Mass Storage Class Bulk-Only Transport rev 1.0)
 */


#define	BULK_ONLY_RESET		0xFF		/* Reset value to be passed */
#define	BULK_ONLY_GET_MAXLUN	0xA1		/* Bulk Class specific req  */

/*
 * Command Block Wrapper:
 *	The CBW is used to transfer commands to the device.
 */
#define	CBW_SIGNATURE	0x43425355	/* "USBC" */
#define	CBW_DIR_IN	0x80		/* CBW from device to the host */
#define	CBW_DIR_OUT	0x00		/* CBW from host to the device */
#define	CBW_CDB_LEN	16		/* CDB Len to 10 byte cmds */

typedef struct usb_bulk_cbw {
	uchar_t		cbw_dCBWSignature[4];		/* CBW Signature */
	uchar_t		cbw_dCBWTag[4];			/* CBW tag */
	uchar_t		cbw_dCBWDataTransferLength[4];	/* CBW Data Len */
	uchar_t		cbw_bmCBWFlags;			/* CBW flags IN/OUT */
	uchar_t		cbw_bCBWLUN:4,			/* CBW Lun value */
			cbw_reserved1:4;
	uchar_t		cbw_bCBWCBLength:5,		/* CDB Len */
			cbw_reserved2:3;
	uchar_t		cbw_bCBWCDB[CBW_CDB_LEN];	/* CDB */
} usb_bulk_cbw_t;


#define	USB_BULK_CBWCMD_LEN	sizeof (usb_bulk_cbw_t)


#define	CBW_MSB(x)	((x) & 0xFF)		/* Swap msb */
#define	CBW_MID1(x)	((x) >> 8 & 0xFF)
#define	CBW_MID2(x)	((x) >> 16 & 0xFF)
#define	CBW_LSB(x)	((x) >> 24 & 0xFF)

/*
 * Command Status Wrapper:
 *	The device shall not execute any subsequent command until the
 *	associated CSW from the previous command has been successfully
 *	transported.
 *
 *	All CSW transfers shall be ordered withe LSB first.
 */
typedef	struct usb_bulk_csw {
	uchar_t	csw_dCSWSignature0;	/* Signature */
	uchar_t	csw_dCSWSignature1;
	uchar_t	csw_dCSWSignature2;
	uchar_t	csw_dCSWSignature3;
	uchar_t	csw_dCSWTag3;		/* random tag */
	uchar_t	csw_dCSWTag2;
	uchar_t	csw_dCSWTag1;
	uchar_t	csw_dCSWTag0;
	uchar_t	csw_dCSWDataResidue0;	/* data not transferred */
	uchar_t	csw_dCSWDataResidue1;
	uchar_t	csw_dCSWDataResidue2;
	uchar_t	csw_dCSWDataResidue3;
	uchar_t	csw_bCSWStatus;		/* command status */
} usb_bulk_csw_t;

#define	CSW_SIGNATURE	0x53425355	/* "SBSU" */

#define	CSW_STATUS_GOOD		0x0	/* Good status */
#define	CSW_STATUS_FAILED	0x1	/* Command failed */
#define	CSW_STATUS_PHASE_ERROR	0x2	/* Phase error */
#define	CSW_LEN			0xD	/* CSW Command Len */


/* SCSI commands that are specific to Bulk Only devices */
#define	CARTRIDGE_PROT_CMD	0x0C
#define	TRANSLATE_LBA_CMD	0x22
#define	READ_FORMAT_CAP_CMD	0x23
#define	FORMAT_VERIFY_CMD	0x24
#define	READ_LONG_CMD		0x3E
#define	WRITE_LONG_CMD		0x3F

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_BULKONLY_H */
