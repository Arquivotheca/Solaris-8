/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MLX_SCSI_H
#define	_SYS_DKTP_MLX_SCSI_H

#pragma ident	"@(#)mlx_scsi.h	1.6	99/05/04 SMI"

/*
 * Mylex DAC960 Host Adapter Driver Header File.
 * SCSI private interface.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* Command Type */
#define	MLX_SCSI_CTYPE		3	/* Direct CDB Commands */

/*  Type 3 Commands */
#define	MLX_SCSI_DCDB		0x04	/* Direct CDB Command */
#define	MLX_SCSI_SG_DCDB	0x84	/* Scatter-Gather Direct CDB Command */

/* Direct CDB Table */
#define	MLX_SCSI_MAX_NCDB	32	/* max CDB commands per Mylex card */

/*
 * XXX - Mylex claims that MLX_SCSI_MAX_XFER	should be variously
 * FFFD, FC00, FE00, ....
 * Unfortunately the dma_lim structure cannot express "maximum
 * single segment == FC00, maximum transfer == FC00~. F800
 * is the closest setting.
 */
#define	MLX_SCSI_MAX_XFER	0xF800	/* in/out using Direct CDB Command */
#define	MLX_SCSI_MAX_SENSE	 64

/* Macros for the command-control byte in the Direct CDB Table. */
#define	MLX_SCSI_CDB_NODATA(x)			((x) = (((x) >> 2) << 2))
#define	MLX_SCSI_CDB_DATAIN(x)			((x) = (((x) >> 2) << 2) | 1)
#define	MLX_SCSI_CDB_DATAOUT(x)			((x) = (((x) >> 2) << 2) | 2)

#define	MLX_SCSI_CDB_EARLY_STAT(x)		((x) |= 4)
#define	MLX_SCSI_CDB_NORMAL_STAT(x)		((x) &= 0xFB)

#define	MLX_SCSI_CDB_20MIN_TIMEOUT(x)		((x) |= 0x30)
#define	MLX_SCSI_CDB_60SEC_TIMEOUT(x)		((x) |= 0x20)
#define	MLX_SCSI_CDB_10SEC_TIMEOUT(x)		((x) |= 0x10)
#define	MLX_SCSI_CDB_1HR_TIMEOUT(x)		((x) &= 0xCF)

#define	MLX_SCSI_CDB_NO_AUTO_REQ_SENSE(x)	((x) |= 0x40)
#define	MLX_SCSI_CDB_AUTO_REQ_SENSE(x)		((x) &= 0xBF)

#define	MLX_SCSI_CDB_DISCON(x)			((x) |= 0x80)
#define	MLX_SCSI_CDB_NO_DISCON(x)		((x) &= 0x7F)

/* Convenient macros. */
#define	MLX_SCSI_TRAN2UNIT(tran) ((mlx_unit_t *)(tran)->tran_tgt_private)
#define	MLX_SCSI_TRAN2HBA_UNIT(tran) ((mlx_unit_t *)(tran)->tran_hba_private)
#define	MLX_SDEV2HBA_UNIT(sd)	(MLX_SCSI_TRAN2HBA_UNIT(SDEV2TRAN(sd)))

#define	MLX_SCSI_TRAN2HBA(tran)	(MLX_SCSI_TRAN2HBA_UNIT(tran)->hba)
#define	MLX_SDEV2HBA(sd)	(MLX_SCSI_TRAN2HBA(SDEV2TRAN(sd)))
#define	MLX_SCSI_PKT2UNIT(pkt)	(MLX_SCSI_TRAN2UNIT(PKT2TRAN(pkt)))
#define	MLX_SCSI_PKT2HBA(pkt)	(MLX_SCSI_PKT2UNIT(pkt)->hba)
#define	MLX_SA2UNIT(sa)		(MLX_SCSI_TRAN2UNIT(ADDR2TRAN(sa)))
#define	MLX_SA2HBA(sa)		(MLX_SA2UNIT(sa)->hba)

#define	MLX_SCSI_AUTO_REQ_OFF(cdbt) ((cdbt)->cmd_ctrl & 0x40)
#define	MLX_SCSI_AUTO_REQ_ON(cdbt)  (!MLX_IS_AUTO_REQ_OFF(cdbt))

#pragma	pack(1)

typedef struct mlx_cdbt {
	uchar_t unit;		/* channel(upper 4 bits),target(lower 4) */
	uchar_t cmd_ctrl;	/* command control */
	volatile ushort_t xfersz; /* Transfer length <= MLX_SCSI_MAX_XFER */
				/* Never set this field to 0		*/
	paddr_t databuf;	/* Data buffer in memory or s/g array	*/
	uchar_t cdblen;		/* Size of CDB in bytes <= 12		*/
	volatile uchar_t senselen;	/* sense length in bytes <= 64	*/
	uchar_t cdb[12];	/* Pointer to CDB Data upto 12 bytes	*/
	/* sense data transferred from device if auto request sense is on */
	volatile uchar_t sensedata[MLX_SCSI_MAX_SENSE];
	volatile struct scsi_status status;	/* of last SCSI cmd	*/
						/* including auto-sense */
	uchar_t reserved;
} mlx_cdbt_t;

#pragma	pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_MLX_SCSI_H */
