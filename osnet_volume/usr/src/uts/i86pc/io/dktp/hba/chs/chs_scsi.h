/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_DKTP_CHS_SCSI_H
#define	_SYS_DKTP_CHS_SCSI_H

#pragma ident	"@(#)chs_scsi.h	1.3	99/03/16 SMI"

/*
 * IBM RAID PCI Host Adapter Driver Header File.
 * SCSI private interface.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAX_WIDE_SCSI_TGTS	15

/* Command Type */
#define	CHS_SCSI_CTYPE		3	/* Direct CDB Commands */

/*  Type 3 Commands */
#define	CHS_SCSI_DCDB		0x04	/* Direct CDB Command */
#define	CHS_SCSI_SG_DCDB	0x84	/* Scatter-Gather Direct CDB Command */

/* Direct CDB Table */
#define	CHS_SCSI_MAX_NCDB	32	/* max CDB commands per Mylex card */

/*
 * XXX - Mylex claims that CHS_SCSI_MAX_XFER	should be variously
 * FFFD, FC00, FE00, ....
 * Unfortunately the dma_lim structure cannot express "maximum
 * single segment == FC00, maximum transfer == FC00~. F800
 * is the closest setting.
 */
#define	CHS_SCSI_MAX_XFER	0xF800	/* in/out using Direct CDB Command */
#define	CHS_SCSI_MAX_SENSE	 64

/* Macros for the command-control byte in the Direct CDB Table. */
#define	CHS_SCSI_CDB_NODATA(x)		((x) = (((x) >> 2) << 2))
#define	CHS_SCSI_CDB_DATAIN(x)		((x) = (((x) >> 2) << 2) | 1)
#define	CHS_SCSI_CDB_DATAOUT(x)		((x) = (((x) >> 2) << 2) | 2)

#define	CHS_SCSI_CDB_EARLY_STAT(x)		((x) |= 4)
#define	CHS_SCSI_CDB_NORMAL_STAT(x)		((x) &= 0xFB)

#define	CHS_SCSI_CDB_20MIN_TIMEOUT(x)	((x) |= 0x30)
#define	CHS_SCSI_CDB_60SEC_TIMEOUT(x)	((x) |= 0x20)
#define	CHS_SCSI_CDB_10SEC_TIMEOUT(x)	((x) |= 0x10)
#define	CHS_SCSI_CDB_1HR_TIMEOUT(x)		((x) &= 0xCF)

#define	CHS_SCSI_CDB_NO_AUTO_REQ_SENSE(x)	((x) |= 0x40)
#define	CHS_SCSI_CDB_AUTO_REQ_SENSE(x)	((x) &= 0xBF)

#define	CHS_SCSI_CDB_DISCON(x)		((x) |= 0x80)
#define	CHS_SCSI_CDB_NO_DISCON(x)		((x) &= 0x7F)

/* Convenient macros. */
#define	CHS_SCSI_TRAN2UNIT(tran) \
		((chs_unit_t *)(tran)->tran_tgt_private)

#define	CHS_SCSI_TRAN2HBA_UNIT(tran) \
		((chs_unit_t *)(tran)->tran_hba_private)

#define	CHS_SDEV2HBA_UNIT(sd) \
		(CHS_SCSI_TRAN2HBA_UNIT(SDEV2TRAN(sd)))

#define	CHS_SCSI_TRAN2HBA(tran)	(CHS_SCSI_TRAN2HBA_UNIT(tran)->hba)
#define	CHS_SDEV2HBA(sd)	(CHS_SCSI_TRAN2HBA(SDEV2TRAN(sd)))
#define	CHS_SCSI_PKT2UNIT(pkt)	(CHS_SCSI_TRAN2UNIT(PKT2TRAN(pkt)))
#define	CHS_SCSI_PKT2HBA(pkt)	(CHS_SCSI_PKT2UNIT(pkt)->hba)
#define	CHS_SA2UNIT(sa)		(CHS_SCSI_TRAN2UNIT(ADDR2TRAN(sa)))
#define	CHS_SA2HBA(sa)		(CHS_SA2UNIT(sa)->hba)

#define	CHS_SCSI_AUTO_REQ_OFF(cdbt) ((cdbt)->cmd_ctrl & 0x40)
#define	CHS_SCSI_AUTO_REQ_ON(cdbt)  (!CHS_IS_AUTO_REQ_OFF(cdbt))

enum {CHS_GET_SCSI_STATUSP, CHS_GET_SCSI_CDBP,
	CHS_GET_SCSI_USGLENGTHP };
#pragma	pack(1)

typedef struct chs_cdbt {
	uchar_t unit;		/* channel(upper 4 bits),target(lower 4) */
	uchar_t cmd_ctrl;	/* command control */
	volatile ushort_t xfersz; /* Transfer length <= CHS_SCSI_MAX_XFER */
				/* Never set this field to 0		*/
	paddr_t databuf;	/* Data buffer in memory or s/g array	*/
	uchar_t cdblen;	/* Size of CDB in bytes <= 12		*/
	volatile uchar_t senselen; /* sense length in bytes <= 64	*/
	union {
		struct {
			uchar_t SGLength;
			uchar_t filler1;
			uchar_t cdb[12];
			/*
			 * sense data transferred from device if auto request
			 * sense is on
			 */
			volatile uchar_t sensedata[CHS_SCSI_MAX_SENSE];

			/* status of last command including auto-sense */
			volatile struct scsi_status status;
			uchar_t filler3[3];
		} viper;
	} fmt;

} chs_cdbt_t;


typedef struct chs_dcdb_uncommon {
		uchar_t *cdbp;
		uchar_t *sensedatap;
		struct scsi_status *statusp;
		uchar_t *sglengthp;
} chs_dcdb_uncommon_t;




#pragma	pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CHS_SCSI_H */
