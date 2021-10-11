/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_USB_HID_H
#define	_SYS_USB_HID_H

#pragma ident	"@(#)hid.h	1.6	99/11/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	USB_DESCR_TYPE_HID	0x21
#define	USB_HID_DESCR_SIZE	10	/* Hid descriptor length */

/*
 * HID : This header file defines the interface between the hid
 * module and the hid driver.
 */

/*
 * There is an M_CTL command per class specific HID command defined in
 * section 7.2 of the specification.
 */

#define	HID_GET_REPORT		0x0001		/* receive report */
#define	HID_GET_IDLE		0x0002		/* find the idle value */
#define	HID_GET_PROTOCOL	0x0003		/* get the protocol */
#define	HID_SET_REPORT		0x0009		/* send a report to device */
#define	HID_SET_IDLE		0x000a		/* set the idle value */
#define	HID_SET_PROTOCOL	0x000b		/* set the protocol */

/*
 * Hid descriptor
 */
typedef struct usb_hid_descr {
	uchar_t		bLength;		/* Size of this descriptor */
	uchar_t		bDescriptorType;	/* HID descriptor */
	ushort_t	bcdHID;			/* HID spec release */
	uchar_t		bCountryCode;		/* Country code */
	uchar_t		bNumDescriptors;	/* No. class descriptors */
	uchar_t		bReportDescriptorType;	/* Class descr. type */
	ushort_t	wReportDescriptorLength; /* size of report descr */
} usb_hid_descr_t;

/*
 * Hid will turn the M_CTL request into a request control request on the
 * default pipe.  Hid needs the following information in the hid_req_t
 * structure.  See the details below for specific values for each command.
 */
typedef struct hid_req_struct {
	uint16_t	hid_req_version_no;	/* Version number */
	uint16_t	hid_req_wValue;		/* wValue field of request */
	uint16_t	hid_req_wLength;	/* wLength of request */
	mblk_t		*hid_req_data;		/* data for send case */
} hid_req_t;


/*
 * hid_req_wValue values HID_GET_REPORT and HID_SET_REPORT
 */
#define	REPORT_TYPE_INPUT	0x0100			/* Input report */
#define	REPORT_TYPE_OUTPUT	0x0200			/* Output report */
#define	REPORT_TYPE_FEATURE	0x0300			/* Feature report */


/*
 * hid_req_wLength value for HID_GET_IDLE and HID_SET_IDLE
 */
#define	GET_IDLE_LENGTH		0x0001
#define	SET_IDLE_LENGTH		0x0000

/*
 * hid_req_wValue values for SET_PROTOCOL
 */
#define	SET_BOOT_PROTOCOL	0x0000			/* Boot protocol */
#define	SET_REPORT_PROTOCOL	0x0100			/* Report protocol */

/*
 * return values for GET_PROTOCOL
 */
#define	BOOT_PROTOCOL		0x00		/* Returned boot protocol */
#define	REPORT_PROTOCOL		0x01		/* Returned report protocol */

/*
 * There is an additional M_CTL command for obtaining the
 * hid parser handle.  This M_CTL returns a pointer to  the handle.
 * The type of the pointer is intpr_t because this type is large enough to
 * hold any data pointer.
 */
#define	HID_GET_PARSER_HANDLE	0x0100		/* obtain parser handle */

/*
 * M_CTL commands for event notifications
 */
#define	HID_POWER_OFF		0x00DC
#define	HID_FULL_POWER		0x00DD
#define	HID_DISCONNECT_EVENT	0x00DE
#define	HID_CONNECT_EVENT	0x00DF

/*
 * To get the report descriptor,
 * This is the wValue
 */
#define	USB_CLASS_DESCR_TYPE_REPORT	0x2200


/* Version numbers */
#define	HID_VERSION_V_0		0

#define	HID_SUCCESS	0	/* Success returned by a function */
#define	HID_FAILURE	1	/* Failure returned by a function */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USB_HID_H */
