/*
 * Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __DPTIOCTL_H_
#define	__DPTIOCTL_H_

#pragma ident	"@(#)dptioctl.h	1.6	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* DPT ioctl defines and structures					*/

#define	DPT_MAX_CONTROLLERS	18

/* High order bit in dpt_found to signify controllers in ioctl		*/
#define	DPT_CTRL_HELD		0x8000000

typedef struct {
	ushort_t dc_number;
	ushort_t dc_addr[DPT_MAX_CONTROLLERS];
} dpt_get_ctlrs_t;

/* ReadConfig data structure - this structure contains the EATA Configuration */
typedef struct {
	uchar_t ConfigLength[4]; /* Len in bytes after this field.	*/
	uchar_t EATAsignature[4];
	uchar_t EATAversion;

	uchar_t OverLapCmds:1;	/* TRUE if overlapped cmds supported	*/
	uchar_t TargetMode:1;	/* TRUE if target mode supported	*/
	uchar_t TrunNotNec:1;
	uchar_t MoreSupported:1;
	uchar_t DMAsupported:1;	/* TRUE if DMA Mode Supported		*/
	uchar_t DMAChannelValid:1; /* TRUE if DMA Channel is Valid	*/
	uchar_t ATAdevice:1;
	uchar_t HBAvalid:1;	/* TRUE if HBA field is valid		*/

	uchar_t PadLength[2];
	uchar_t HBA[4];
	uchar_t CPlength[4];	/* Command Packet Length		*/
	uchar_t SPlength[4];	/* Status Packet Length			*/
	uchar_t QueueSize[2];	/* Controller Que depth			*/
	uchar_t SG_Size[4];

	uchar_t IRQ_Number:4;	/* IRQ Ctlr is on ... ie 14,15,12	*/
	uchar_t IRQ_Trigger:1;	/* 0 =Edge Trigger, 1 =Level Trigger	*/
	uchar_t Secondary:1;	/* TRUE if ctlr not parked on 0x1F0	*/
	uchar_t DMA_Channel:2;	/* DMA Channel used if PM2011		*/

	uchar_t	Reserved0;	/*	Reserved Field			*/

	uchar_t	Disable:1;	/* Secondary I/O Address Disabled	*/
	uchar_t	ForceAddr:1;	/* PCI Forced To An EISA/ISA Address    */
	uchar_t	Reserved1:6;	/* Reserved Field			*/

	uchar_t	MaxScsiID:5;	/* Maximun SCSI Target ID Supported	*/
	uchar_t	MaxChannel:3;	/* Maximum Channel Number Supported	*/

	uchar_t	MaxLUN;		/* Maximun LUN Supported		*/

	uchar_t	Reserved2:6;	/* Reserved Field			*/
	uchar_t	PCIbus:1;	/* PCI Adapter Flag			*/
	uchar_t	EISAbus:1;	/* EISA Adapter				*/

	uchar_t	RaidNum;	/* RAID HBA Number For Stripping	*/
	uchar_t	Reserved3;	/* Reserved Field			*/
}	dpt_ReadConfig_t;

typedef	struct {
	uint_t	rcf_base;
	dpt_ReadConfig_t	rcf_config;
}	dpt_readconfig_t;

/*	dpt ioctls		*/
#define	DPT_EATA_USR_CMD	0x01
#define	DPT_GET_SIG		0x02
#define	DPT_GET_CTLRS		0x03
#define	DPT_READ_CONFIG		0x04

#define	DPT_CORE_CCB_SIZ	24

#ifdef	__cplusplus
}
#endif

#endif /* __DPTIOCTL_H_ */
