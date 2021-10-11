/*
 * Copyright (c) 1995-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_DPT_H
#define	_SYS_DKTP_DPT_H

#pragma ident	"@(#)dpt.h	1.27	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	HBA_INTPROP(devi, pname, pval, plen) \
	(ddi_prop_op(DDI_DEV_T_NONE, (devi), PROP_LEN_AND_VAL_BUF, \
		DDI_PROP_DONTPASS, (pname), (caddr_t)(pval), (plen)))

#define	HBA_KVTOP(vaddr, shf, msk) \
		((paddr_t)(hat_getkpfnum((caddr_t)(vaddr)) << (shf)) | \
			    ((paddr_t)(vaddr) & (msk)))

#define	DPT_KVTOP(vaddr) (HBA_KVTOP((vaddr), dpt_pgshf, dpt_pgmsk))

#define	PRF		prom_printf

#define	DPT_PHYMAX_DMASEGS	64	/* phy max Scatter/Gather seg	*/
#define	DPT_MAX_DMA_SEGS	32	/* Max used Scatter/Gather seg	*/
#define	DPT_SENSE_LEN		0x18	/* AUTO-Request sense size	*/

/* dpt host adapter error codes from location dpt_stat.sp_hastat */
#define	DPT_OK			0x0
#define	DPT_SELTO		0x1
#define	DPT_CMDTO		0x2
#define	DPT_BUSRST		0x3
#define	DPT_POWUP		0x4
#define	DPT_PHASERR		0x5
#define	DPT_BUSFREE		0x6
#define	DPT_BUSPARITY		0x7
#define	DPT_BUSHUNG		0x8
#define	DPT_MSGREJECT		0x9
#define	DPT_RSTSTUCK		0x0a
#define	DPT_REQSENFAIL		0x0b
#define	DPT_HAPARITY		0x0c
#define	DPT_CPABORTNOTACTIVE	0x0d
#define	DPT_CPBUSABORT		0x0e
#define	DPT_CPRESETNOTACTIVE	0x0f
#define	DPT_CPRESETONBUS	0x10
#define	DPT_CSRAMECC		0x11	/* Controller RAM ECC Error	*/
#define	DPT_CSPCIPE		0x12	/* PCI Parity Error:data or cmd	*/
#define	DPT_CSPCIRMA		0x13	/* PCI Received Master Abort	*/
#define	DPT_CSPCIRTA		0x14	/* PCI Received Target Abort	*/
#define	DPT_CSPCISTA		0x15	/* PCI Signaled Target Abort	*/

#ifdef  DPT_DEBUG
#define	DPT_UNKNOWN_ERROR	0x16
#endif

/* Scatter Gather - definitions and list structure.			*/
struct dpt_sg {
	paddr_t data_addr;
	ulong_t	data_len;
};

/* EATA Command Packet control bits structure definition - 		*/
/* this controlls the data direction, cdb interpret, and other misc 	*/
/* controll bits.							*/
struct cp_bits {
	uchar_t SCSI_Reset:1;	/* Cause a SCSI Bus reset on the cmd	*/
	uchar_t HBA_Init:1;	/* Cause Controller to reInitialize	*/
	uchar_t Auto_Req_Sen:1;	/* Do Auto Request Sense on errors	*/
	uchar_t Scatter_Gather:1; /* Data Ptr points to a SG Packet	*/
	uchar_t Resrvd:1;	/* RFU					*/
	uchar_t Interpret:1;	/* Interpret the SCSI cdb of own use	*/
	uchar_t DataOut:1;	/* Data Out phase with command		*/
	uchar_t DataIn:1;	/* Data In phase with command		*/
};

/*
 * The DPT 20xx Host Adapter Command Control Block (CCB)
 */

typedef struct dpt_ccb {
	/* Begin EATA Command Packet Portion 				*/
	union {
		struct cp_bits b_bit;
		uchar_t b_byte;
	} ccb_option;
	uchar_t  ccb_senselen;	/* AutoRequestSense Data length.	*/
	uchar_t  ccb_resv[5];
	uchar_t  ccb_id;	/* Target SCSI ID, if no interpret.   	*/
	uchar_t  ccb_msg0;	/* Identify and Disconnect Message.   	*/
	uchar_t  ccb_msg1;
	uchar_t  ccb_msg2;
	uchar_t  ccb_msg3;
	uchar_t  ccb_cdb[12];	/* SCSI cdb for command.		*/
	uchar_t	ccb_datalen[4];	/* Data length in bytes for command.  	*/
	struct  dpt_ccb *ccb_vp; /* Command Packet Virtual Pointer.	*/
	uchar_t	ccb_datap[4];   /* Data Physical Address for command. 	*/
	uchar_t	ccb_statp[4];   /* Status Packet Physical Address.    	*/
	uchar_t	ccb_sensep[4];  /* AutoRequestSense data Phy Address. 	*/
	/* End EATA Command Packet Portion 				*/

	uchar_t  ccb_ctlrstat;   /* Ctlr Status after ccb complete	*/
	uchar_t  ccb_scsistat;   /* SCSI Status after ccb complete	*/

	uchar_t	ccb_cdblen;	/* cdb length				*/
	uchar_t  ccb_scatgath_siz;

	struct	scsi_arq_status ccb_sense; /* Auto_Request sense blk	*/
	struct  dpt_sg ccb_sg_list[DPT_MAX_DMA_SEGS]; /* scatter/gather	*/
	struct  dpt_gcmd_wrapper *ccb_ownerp; /* ptr to the scsi_cmd	*/
	paddr_t ccb_paddr;	/* ccb physical address.		*/
	ushort_t ccb_ioaddr;	/* hba io base address for usr commands */
	uchar_t	ccb_ind;	/* index into active list		*/
	uchar_t	ccb_cnt;	/* active count				*/
}ccb_t;

#define	ccb_optbit	ccb_option.b_bit
#define	ccb_optbyte	ccb_option.b_byte

/*
 * Wrapper for HBA per-packet driver-private data area so that the
 * gcmd_t struture is linkted to the IOBP-alloced ccb_t
 */

typedef struct dpt_gcmd_wrapper {
	gcmd_t	 *cmd_gcmdp;
	ccb_t	*dw_ccbp;
	caddr_t cmd_cdbp;
	int	cmd_cdblen;
	caddr_t cmd_sensep;
	int	cmd_senselen;
	int	cmd_sg_cnt;
} dwrap_t;

#define	GCMDP2CMDP(gcmdp)	((dwrap_t *)(gcmdp)->cmd_private)
#define	GCMDP2CCBP(gcmdp)	GCMDP2CMDP(gcmdp)->dw_ccbp

#define	PKTP2CMDP(pktp)		GCMDP2CMDP(PKTP2GCMDP(pktp))
#define	PKTP2CCBP(pktp)		GCMDP2CCBP(PKTP2GCMDP(pktp))

#define	CCBP2CMDP(ccbp)		((ccbp)->ccb_ownerp)
#define	CCBP2GCMDP(ccbp)	((ccbp)->ccb_ownerp->cmd_gcmdp)


/* StatusPacket data structure - this structure is returned by the 	*/
/* controller upon command completion. It contains status, message info */
/* and pointer to the initiating command ccb (virtual).			*/
struct 	dpt_stat {
	uchar_t	sp_hastat;		/* Controller Status message.	*/
	uchar_t	sp_scsi_stat;		/* SCSI Bus Status message.	*/
	uchar_t	reserved[2];
	uchar_t	sp_inv_residue[4];	/* how much was not xferred	*/
	struct	dpt_ccb *sp_vp;		/* Command pkt Virtual Pointer.	*/
	uchar_t	sp_ID_Message;
	uchar_t	sp_Que_Message;
	uchar_t	sp_Tag_Message;
	uchar_t	sp_Messages[9];
};

struct  dpt_blk {
	struct dpt_stat 	db_stat; /* hardware completion struct 	*/
	paddr_t			db_stat_paddr; /* its phy addr		*/

	kmutex_t 		db_mutex;	/* overall driver mutex */
	kmutex_t 		db_rmutex;	/* pkt resource mutex */

	ccc_t			db_ccc;		/* CCB timer control */

	dev_info_t 		*db_dip;

	ushort_t		db_ioaddr;
	ushort_t  		db_scatgath_siz;

	uchar_t			db_targetid[4];
	uchar_t			db_intr;
	uchar_t			db_dmachan;
	uchar_t			db_numdev;


	struct	dpt_ccb 	**db_ccbp;
	ddi_dma_lim_t		*db_limp;
	ushort_t  		db_q_siz;
	ushort_t  		db_child;
	uchar_t			db_max_target;
	uchar_t			db_max_lun;
	uchar_t			db_max_chan;
	uchar_t			db_bustype;
	ushort_t		db_max_que;
};

struct dpt_unit {
	ddi_dma_lim_t	du_lim;
	unsigned du_arq : 1;		/* auto-request sense enable	*/
	unsigned du_tagque : 1;		/* tagged queueing enable	*/
	unsigned du_resv : 6;
	int	du_chan;
	int	du_total_sectors;	/* total capacity in sectors	*/
};


#define	DPT_DIP(dpt)		((dpt)->d_blkp)->db_dip

#define	GTGTP2DPTP(gtgtp)	((struct dpt *)(GTGTP2TARGET(gtgtp)))
#define	GTGTP2DPTBLKP(gtgtp)	((struct dpt_blk *)GTGTP2DPTP(gtgtp)->d_blkp)
#define	GTGTP2DPTUNITP(gtgtp)	((struct dpt_unit *)GTGTP2DPTP(gtgtp)->d_unitp)
#define	TRAN2DPTBLKP(tranp)	GTGTP2DPTBLKP(TRAN2GTGTP(tranp))


#define	PKT2DPT(pktp)		TRAN2DPTP(PKTP2TRAN(pktp))
#define	PKT2DPTUNITP(pktp)	TRAN2DPTUNITP(PKTP2TRAN(pktp))
#define	PKT2DPTBLKP(pktp)	TRAN2DPTBLKP(PKTP2TRAN(pktp))
#define	PKT2CMDP(pktp)		CMDP2CCBP(PKTP2GCMDP(pktp))

#define	ADDR2DPTUNITP(ap)	GTGTP2DPTUNITP(ADDR2GTGTP(ap))
#define	ADDR2DPTBLKP(ap)	GTGTP2DPTBLKP(ADDR2GTGTP(ap))

#define	TRAN2DPTP(tranp)	((struct dpt *)((tranp)->tran_hba_private))
#define	SDEV2DPTP(sd)		TRAN2DPTP(SDEV2TRAN(sd))

/* used to get dpt_blk from dpt struct from upper layers */
#define	DPT_BLKP(X) (((struct dpt *)(X))->d_blkp)
struct dpt {
	struct scsi_hba_tran	*d_tran;
	struct dpt_blk		*d_blkp;
	struct dpt_unit		*d_unitp;
};

#define	DPT_MAX_CTRLS		18
typedef struct {
	ushort_t dc_number;
	ushort_t dc_addr[DPT_MAX_CTRLS];
	struct dpt_blk  *dc_blkp[DPT_MAX_CTRLS];
} dpt_controllers_t;

#define	HBA_BUS_TYPE_LENGTH 32	/* Length For Bus Type Field		*/

/* ReadConfig data structure - this structure contains the EATA Configuration */
struct ReadConfig {
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
	uchar_t Reserved[2];	/* Reserved Field			*/
	uchar_t SG_Size[2];

	uchar_t IRQ_Number:4;	/* IRQ Ctlr is on ... ie 14,15,12	*/
	uchar_t IRQ_Trigger:1;	/* 0 =Edge Trigger, 1 =Level Trigger	*/
	uchar_t Secondary:1;	/* TRUE if ctlr not parked on 0x1F0	*/
	uchar_t DMA_Channel:2;	/* DMA Channel used if PM2011		*/

	uchar_t Reserved0;	/* Reserved Field			*/

	uchar_t	Disable:1;	/* Secondary I/O Address Disabled	*/
	uchar_t ForceAddr:1;	/* PCI Forced To An EISA/ISA Address	*/
	uchar_t	Reserved1:6;	/* Reserved Field			*/

	uchar_t	MaxScsiID:5;	/* Maximun SCSI Target ID Supported	*/
	uchar_t	MaxChannel:3;	/* Maximum Channel Number Supported	*/

	uchar_t	MaxLUN;		/* Maximun LUN Supported		*/

	uchar_t	Reserved2:6;	/* Reserved Field			*/
	uchar_t	PCIbus:1;	/* PCI Adapter Flag			*/
	uchar_t	EISAbus:1;	/* EISA Adapter				*/
	uchar_t	RaidNum;	/* RAID HBA Number For Stripping	*/
	uchar_t	Reserved3;	/* Reserved Field			*/
};

/* Controller IO Register Offset Definitions				*/
#define	HA_COMMAND	0x07	/* Command register			*/
#define	HA_STATUS	0x07	/* Status register			*/
#define	HA_DMA_BASE	0x02	/* LSB for DMA Physical Address 	*/
#define	HA_ERROR	0x01	/* Error register			*/
#define	HA_DATA		0x00	/* Data In/Out register			*/
#define	HA_AUX_STATUS   0x08	/* Auxillary Status Reg on 2012		*/
#define	HA_IMMED_MOD	0x05	/* Immediate Modifier Register		*/
#define	HA_IMMED_FUNC	0x06	/* Immediate Function Register		*/

/* for EATA immediate commands */
#define	HA_GEN_CODE		0x06
#define	HA_GEN_ABORT_TARGET	0x05
#define	HA_GEN_ABORT_LUN	0x04

#define	HA_AUX_BUSY	0x01	/* Aux Reg Busy bit.			*/
#define	HA_AUX_INTR	0x02	/* Aux Reg Interrupt Pending.		*/

#define	HA_ST_ERROR	0x01	/* HA_STATUS register bit defs		*/
#define	HA_ST_INDEX	0x02
#define	HA_ST_CORRCTD   0x04
#define	HA_ST_DRQ	0x08
#define	HA_ST_SEEK_COMP 0x10
#define	HA_ST_WRT_FLT   0x20
#define	HA_ST_READY	0x40
#define	HA_ST_BUSY	0x80

#define	HA_ST_DATA_RDY  HA_ST_SEEK_COMP + HA_ST_READY + HA_ST_DRQ

#define	HA_SELTO	0x01

/* Controller Command Definitions					*/
#define	CP_READ_CFG_PIO 0xF0
#define	CP_PIO_CMD	0xF2
#define	CP_TRUCATE_CMD  0xF4
#define	CP_READ_CFG_DMA 0xFD
#define	CP_SET_CFG_DMA  0xFE
#define	CP_EATA_RESET   0xF9
#define	CP_DMA_CMD	0xFF
#define	ECS_EMULATE_SEN 0xD4
#define	CP_EATA_IMMED   0xFA

/* EATA Immediate Sub Functions						*/
#define	CP_EI_ABORT_MSG		0x00
#define	CP_EI_RESET_MSG		0x01
#define	CP_EI_RESET_BUS		0x02
#define	CP_EI_ABORT_CP		0x03
#define	CP_EI_INTERRUPTS	0x04
#define	CP_EI_MOD_ENABLE	0x00
#define	CP_EI_MOD_DISABLE	0x01
#define	CP_EI_RESET_BUSES	0x09


/* EATA Command/Status Packet Definitions				*/
#define	HA_DATA_IN	0x80
#define	HA_DATA_OUT	0x40
#define	HA_CP_QUICK	0x10
#define	HA_SCATTER	0x08
#define	HA_AUTO_REQSEN	0x04
#define	HA_HBA_INIT	0x02
#define	HA_SCSI_RESET	0x01

#define	HA_STATUS_MASK  0x7F
#define	HA_IDENTIFY_MSG 0x80
#define	HA_DISCO_RICO   0x40

#define	FLUSH_CACHE_CMD		0x35		 /* Synchronize Cache Command */

#define	SCBP(pkt)	((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)	((*(pkt)->pkt_scbp) & STATUS_MASK)

#define	PCI_DPT_VEND_ID	0x1044		/* DPT Vendor ID   */
#define	PCI_DPT_DEV_ID	0xa400		/* DPT Device ID   */
#define	MAX_PCI_BUS	1
#define	MAX_PCI_DEVICE	32

#define	DPT_PCI_ADAPTER		0
#define	DPT_EISA_ADAPTER	1
#define	DPT_ISA_ADAPTER		2

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_DPT_H */
