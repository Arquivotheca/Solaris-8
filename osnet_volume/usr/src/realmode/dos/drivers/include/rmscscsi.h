/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)rmscscsi.h	1.3	97/03/07 SMI"
 */

/*
 * Definitions for the SCSI real mode driver interface.
 */

#ifndef _RMSC_SCSI_H_
#define	_RMSC_SCSI_H_

#include <rmsc.h>



/*
 * The scsi_op structure is used for passing the details of SCSI
 * requests from the SCSI layer to the hardware-specific code.
 */
#define	RMSC_MAX_SCSI_CMD	12
#define	RMSC_MAX_ARS		32

struct scsi_op {
	ushort cmd_len;
	ushort ars_len;
	ushort target;
	ushort lun;
	ushort request_flags;
	ushort result_flags;
	void far *buffer;
	ulong transfer_len;
	unchar scsi_status;
	unchar ars_data[RMSC_MAX_ARS];
	unchar scsi_cmd[RMSC_MAX_SCSI_CMD];
};

/* Flags defined for request_flags */
#define	RMSC_SFL_BUS_RESET	1
#define	RMSC_SFL_DATA_IN	2
#define	RMSC_SFL_DATA_OUT	4

/* Flags defined for result_flags */
#define	RMSC_SOF_COMPLETED	1	/* SCSI command completed */
#define	RMSC_SOF_GOOD		2	/* target status was good */
#define	RMSC_SOF_ERROR		4	/* unspecified error occurred */
#define	RMSC_SOF_BAD_TARGET	8	/* target is out of range */
#define	RMSC_SOF_BAD_LUN	0x10	/* lun is out of range */
#define	RMSC_SOF_NO_TARGET	0x20	/* target is not present */
#define	RMSC_SOF_ARS_DONE	0x40	/* sense data was returned */



/*
 *	Initialization structure passed by the SCSI layer to the
 *	hardware-specific code's initialization structure.
 *
 *	In the following structure definition the comment at the
 *	start of the each line containing structure members indicates
 *	whether the scsi_driver_init routine is required to
 *	initialize that member (REQ) or whether initialization
 *	is optional (OPT).
 */
struct scsi_driver_init
{
/* REQ */	char *driver_name;
/* OPT */	ushort flags;
#define	RMSC_SCSI_MSCSI	1
/* OPT */	void (*legacy_probe)(void);
/* REQ */	int (*configure)(ushort, rmsc_handle *, char **, int *);
/* REQ */	int (*initialize)(rmsc_handle);
/* OPT */	void (*device_interrupt)(int);
/* REQ */	int (*scsi_op)(rmsc_handle, struct scsi_op *);
/* OPT */	int (*modify_dev_info)(rmsc_handle, struct bdev_info *);
/* OPT */	int (*find_devices)(rmsc_handle);
};
typedef struct scsi_driver_init rmsc_scsi_driver_init;

extern int scsi_driver_init(rmsc_scsi_driver_init *);



#define	RMSC_NO_INTERRUPTS	0xFF

/*	Status Codes returned by SCSI target during status phase */
#define	S_GOOD		0x00	/* Target has successfully completed command */
#define	S_CK_COND	0x02	/* CHECK CONDITION */
#define	S_COND_MET	0x04	/* CONDITION MET/GOOD */
#define	S_BUSY		0x08	/* BUSY, the target is busy */
#define	S_IGOOD		0x10	/* INTERMEDIATE/GOOD */
#define	S_IMET		0x12	/* INTERMEDIATE/CONDITION MET/GOOD */
#define	S_RESERV	0x18	/* RESERVATION CONFLICT */

/*	SCSI Commands Group 0 */
#define	SC_TESTUNIT	0x00	/* TEST UNIT READY */
#define	SC_RSENSE	0x03	/* Request Sense */
#define	SC_INQUIRY	0x12	/* inquiry */
#define	SC_STRT_STOP	0x1B	/* Start/Stop unit */
#define	SC_REMOV	0x1E	/* prevent/allow medium removal */

/*	SCSI Commands Group 1 - Extended */
#define	SX_READCAP	0x25	/* Read Capacity */
#define	SX_READ		0x28	/* Read */

/* SCSI inquiry data */
struct inquiry_data {
	unchar inqd_pdt;	/* Peripheral Device Type (see below) */
	unchar inqd_dtq;	/* Device Type Qualifier (see below) */
	unchar inqd_ver;	/* Version #s */
	unchar inqd_pad1;	/* pad must be zero */
	unchar inqd_len;	/* additional length */
	unchar inqd_pad2[3];	/* pad must be zero */
	unchar inqd_vid[8];	/* Vendor ID */
	unchar inqd_pid[16];	/* Product ID */
	unchar inqd_prl[4];	/* Product Revision Level */
};

/* values for inqd_pdt: */
#define	INQD_PDT_DA	0x00	/* Direct-access (DISK) device */
#define	INQD_PDT_SEQ	0x01	/* Sequential-access (TAPE) device */
#define	INQD_PDT_PRINT	0x02	/* Printer device */
#define	INQD_PDT_PROC	0x03	/* Processor device */
#define	INQD_PDT_WORM	0x04	/* Write-once read-many direct-access device */
#define	INQD_PDT_ROM	0x05	/* Read-only directe-access device */
#define	INQD_PDT_SCAN	0x06	/* Scanner device	(scsi2) */
#define	INQD_PDT_OPTIC	0x07	/* Optical device (probably write many scsi2) */
#define	INQD_PDT_CHNGR	0x08	/* Changer (jukebox, scsi2) */
#define	INQD_PDT_COMM	0x09	/* Communication device (scsi2) */
#define	INQD_PDT_NOLUN2 0x1f	/* Unknown Device (scsi2) */
#define	INQD_PDT_NOLUN	0x7f	/* Logical Unit Not Present */

#define	INQD_PDT_DMASK	0x1F	/* Peripheral Device Type Mask */
#define	INQD_PDT_QMASK	0xE0	/* Peripheral Device Qualifer Mask */


/* masks for inqd_dtq: */
#define	INQD_DTQ_RMB	0x80	/* Removable Medium Bit mask */
#define	INQD_DTQ_MASK	0x7f	/* mask for device type qualifier field */

/* SCSI read capacity data */
struct readcap_data {
	unchar rdcd_lba[4];	/* Logical Block Addr (MSB first, LSB last) */
	unchar rdcd_bl[4];	/* Block Length (MSB first, LSB last) */
};

/* SCSI sense keys */
#define	KEY_NO_SENSE		0x00
#define	KEY_RECOVERABLE_ERROR	0x01
#define	KEY_NOT_READY		0x02
#define	KEY_MEDIUM_ERROR	0x03
#define	KEY_HARDWARE_ERROR	0x04
#define	KEY_ILLEGAL_REQUEST	0x05
#define	KEY_UNIT_ATTENTION	0x06
#define	KEY_WRITE_PROTECT	0x07
#define	KEY_DATA_PROTECT	KEY_WRITE_PROTECT
#define	KEY_BLANK_CHECK		0x08
#define	KEY_VENDOR_UNIQUE	0x09
#define	KEY_COPY_ABORTED	0x0A
#define	KEY_ABORTED_COMMAND	0x0B	/* often a parity error */
#define	KEY_EQUAL		0x0C
#define	KEY_VOLUME_OVERFLOW	0x0D
#define	KEY_MISCOMPARE		0x0E
#define	KEY_RESERVED		0x0F

#endif /* _RMSC_SCSI_H_ */
