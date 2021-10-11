/*
 * Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
 */

#ident  "@(#)dpt.h 1.12     97/05/01 SMI\n"

/*
 * Definitions for DPT SCSI Host Adapters,  models PM2011/9x and PM2012/9x.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
 */

/*
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

#define gdev_dpbp    ulong

#define DPT_SG_MAX      64      /* Max # of Scatter/Gather elements */
#define DPT_SENSE     0x18      /* AUTO-Request sense data size.    */

#define DPT_SCRATCH_SIZE (8 * DPT_SG_MAX)

#define HEADS           64                      /* Num heads for disks.       */
#define SECTORS         64                      /* Num sectors for disks.     */
#define SCSI_CDB_MAX    12                      /* Max SCSI cdb size suptd.   */

#define BYTE            unsigned char

#define DMA0_3MD        0x000B                  /* 8237A DMA Mode Reg (0-3)   */
#define DMA4_7MD        0x00D6                  /* 8237A DMA Mode Reg (4-7)   */
#define CASCADE_DMA     0x00C0                  /* Puts DMA in Cascade Mode   */
#define DMA0_3MK        0x000A                  /* 8237A DMA mask register    */
#define DMA4_7MK        0x00D4                  /* 8237A DMA mask register    */

/*******************************************************************************
**               Driver IOCTL interface command Definitions                   **
*******************************************************************************/
#define DPT_IOCTL_DEBUG	(('D'<<8)|64)
#define EATAUSRCMD	(('D'<<8)|65)

/*******************************************************************************
**                Controller IO Register Offset Definitions                   **
*******************************************************************************/
#define HA_COMMAND      0x07		 /* Command register             */
#define HA_STATUS       0x07		 /* Status register              */
#define	HA_IMMED_MOD	0x05		 /* EATA Immediate Modifier	 */
#define HA_DMA_BASE     0x02             /* LSB for DMA Physical Address */
#define HA_ERROR        0x01             /* Error register               */
#define HA_DATA         0x00             /* Data In/Out register         */
#define HA_AUX_STATUS   0x08             /* Auxillary Status Reg on 2012 */

#define HA_AUX_BUSY     0x01             /* Aux Reg Busy bit.            */
#define HA_AUX_INTR     0x02             /* Aux Reg Interrupt Pending.   */

#define HA_ST_ERROR     0x01             /* HA_STATUS register bit defs  */
#define HA_ST_INDEX     0x02
#define HA_ST_CORRCTD   0x04
#define HA_ST_DRQ       0x08
#define HA_ST_SEEK_COMP 0x10
#define HA_ST_WRT_FLT   0x20
#define HA_ST_READY     0x40
#define HA_ST_BUSY      0x80

#define HA_ST_DATA_RDY  HA_ST_SEEK_COMP + HA_ST_READY + HA_ST_DRQ

#define HA_SELTO	0x01

/*******************************************************************************
**                      Controller Command Definitions			      **
*******************************************************************************/
#define CP_READ_CFG_PIO 0xF0
#define CP_PIO_CMD      0xF2
#define CP_TRUCATE_CMD  0xF4
#define CP_READ_CFG_DMA 0xFD
#define CP_EATA_RESET   0xF9
#define CP_DMA_CMD      0xFF
#define ECS_EMULATE_SEN 0xD4
#define	CP_EATA_IMMED	0xFA

/* EATA Immediate Modifiers */
#define	CP_EI_MOD_ENABLE	0x00
#define	CP_EI_MOD_DISABLE	0x01

/*******************************************************************************
**                    EATA Command/Status Packet Definitions 		      **
*******************************************************************************/
#define HA_DATA_IN      0x80
#define HA_DATA_OUT     0x40
#define HA_CP_QUICK     0x10
#define HA_SCATTER      0x08
#define HA_AUTO_REQSEN  0x04
#define HA_HBA_INIT     0x02
#define HA_SCSI_RESET   0x01

#define HA_STATUS_MASK  0x7F
#define HA_IDENTIFY_MSG 0x80
#define HA_DISCO_RICO   0x40

#define FLUSH_CACHE_CMD		0x35		 /* Synchronize Cache Command */


/*******************************************************************************
** EATA Command Packet control bits structure definition - this controlls the **
**   data direction, cdb interpret, and other misc controll bits.	      **
*******************************************************************************/
struct cp_bits {
	BYTE SCSI_Reset:1;		/* Cause a SCSI Bus reset on the cmd  */
	BYTE HBA_Init:1;		/* Cause Controller to reInitialize   */
	BYTE Auto_Req_Sen:1;		/* Do Auto Request Sense on errors    */
	BYTE Scatter_Gather:1;		/* Data Ptr points to a SG Packet     */
	BYTE Resrvd:1;			/* RFU				      */
        BYTE Interpret:1;		/* Interpret the SCSI cdb of own use  */
	BYTE DataOut:1;			/* Data Out phase with command 	      */
	BYTE DataIn:1;			/* Data In phase with command	      */
};

/*******************************************************************************
** EATA Command Packet definition					      **
*******************************************************************************/
#pragma pack(1)
typedef struct EATACommandPacket {
	union {
	  struct cp_bits bit;
	  unsigned char byte;
	} cp_option;
	BYTE    cp_Req_Len;             /* AutoRequestSense Data length.      */
	BYTE    reserved[5];
	BYTE    cp_id;                  /* Target SCSI ID, if no interpret.   */
	BYTE    cp_msg0;                /* Identify and Disconnect Message.   */
	BYTE    cp_msg1;
	BYTE    cp_msg2;
	BYTE    cp_msg3;
	BYTE    cp_cdb[12];             /* SCSI cdb for command.              */
	ulong   cp_dataLen;             /* Data length in bytes for command.  */
	struct  dpt_ccb *cp_vp;         /* Command Packet Virtual Pointer.    */
	ushort  cp_vpseg;		/* Segment part of prev field	      */
	paddr_t cp_dataDMA;             /* Data Physical Address for command. */
	paddr_t cp_statDMA;             /* Status Packet Physical Address.    */
	paddr_t cp_reqDMA;              /* AutoRequestSense data Phy Address. */
} EATA_CP;
#pragma pack()

/*******************************************************************************
** StatusPacket data structure - this structure is returned by the controller **
**   upon command completion.   It contains status, message info and pointer  **
**   to the initiating command ccb (virtual).				      **
*******************************************************************************/
#pragma pack(1)
typedef struct {
	BYTE    sp_hastat;		/* Controller Status message.         */
	BYTE    sp_SCSI_stat;		/* SCSI Bus Status message.           */
	BYTE    reserved[2];
	BYTE    sp_inv_residue[4];
	struct  dpt_ccb *sp_vp;         /*  Command Packet Virtual Pointer. */
	ushort  sp_vpseg;		/* Segment part of prev field	      */
	BYTE    sp_ID_Message;
	BYTE    sp_Que_Message;
	BYTE    sp_Tag_Message;
	BYTE    sp_Messages[9];
} DPT_STAT;
#pragma pack()

/*******************************************************************************
** Scatter Gather - definitions and list structure.  			      **
*******************************************************************************/
typedef struct ScatterGather {
	paddr_t data_addr;
	paddr_t	data_len;
} SG_T;


/*******************************************************************************
** ReadConfig data structure - this structure contains the EATA Configuration **
*******************************************************************************/
#pragma pack(1)
struct ReadConfig {
	BYTE ConfigLength[4];		/* Len in bytes after this field.     */
	BYTE EATAsignature[4];
	BYTE EATAversion;

	BYTE OverLapCmds:1;		/* TRUE if overlapped cmds supported  */
	BYTE TargetMode:1;		/* TRUE if target mode supported      */
	BYTE TrunNotNec:1;
	BYTE MoreSupported:1;
	BYTE DMAsupported:1;		/* TRUE if DMA Mode Supported	      */
	BYTE DMAChannelValid:1;		/* TRUE if DMA Channel is Valid	      */
	BYTE ATAdevice:1;
	BYTE HBAvalid:1;		/* TRUE if HBA field is valid	      */

	BYTE PadLength[2];
	BYTE HBA[4];
	BYTE CPlength[4];		/* Command Packet Length 	      */
	BYTE SPlength[4];		/* Status Packet Length		      */
	BYTE QueueSize[2];		/* Controller Que depth		      */
	ulong SG_Size;

	BYTE IRQ_Number:4;		/* IRQ Ctlr is on ... ie 14,15,12     */
	BYTE IRQ_Trigger:1;		/* 0 =Edge Trigger, 1 =Level Trigger  */
	BYTE Secondary:1;		/* TRUE if ctlr not parked on 0x1F0   */
	BYTE DMA_Channel:2;             /* DMA Channel used if PM2011         */

	BYTE Reserved0;		/* Reserved Field			*/

	BYTE	Disable:1;	/* Secondary I/O Address Disabled	*/
	BYTE ForceAddr:1;	/* PCI Forced To An EISA/ISA Address	*/
	BYTE	Reserved1:6;	/* Reserved Field			*/

	BYTE	MaxScsiID:5;	/* Maximun SCSI Target ID Supported	*/
	BYTE	MaxChannel:3;	/* Maximum Channel Number Supported	*/

	BYTE	MaxLUN;		/* Maximun LUN Supported		*/

	BYTE	Reserved2:6;	/* Reserved Field			*/
	BYTE	PCIbus:1;	/* PCI Adapter Flag			*/
	BYTE	EISAbus:1;	/* EISA Adapter				*/
	BYTE	RaidNum;	/* RAID HBA Number For Stripping	*/
	BYTE	Reserved3;	/* Reserved Field			*/
};
#pragma pack()

/*******************************************************************************
** Emulation Sense structure - this structure contains the Emulation Config.  **
*******************************************************************************/
struct EmulationSense {
	BYTE Byte0;
	BYTE Cyls[2];
	BYTE Heads;
	BYTE Sectors;
	BYTE padding;
	BYTE drtype[2];
	BYTE lunmap[8];
};

/*******************************************************************************
** Controller Command Block - CCB - This structure contains the EATA Command  **
**   Packet, REQ_IO pointer and other command control information.   The EATA **
**   command packet should remain at the begining of the structure for ease   **
**   of processing the command to the controller.			      **
*******************************************************************************/
#pragma pack(1)
typedef struct dpt_ccb {
	/***************** Begin EATA Command Packet Portion ******************/
	BYTE	cp_option;
	BYTE    cp_reqLen;              /* AutoRequestSense Data length.      */
	BYTE    reserved[5];
	BYTE    cp_id;                  /* Target SCSI ID, if no interpret.   */
	BYTE    cp_msg0;                /* Identify and Disconnect Message.   */
	BYTE    cp_msg1;
	BYTE    cp_msg2;
	BYTE    cp_msg3;
	BYTE    cp_cdb[12];             /* SCSI cdb for command.              */
	ulong	cp_dataLen;		/* Data length in bytes for command.  */
	struct  dpt_ccb *cp_vp;         /* Command Packet Virtual Pointer.    */
	ushort  cp_vpseg;		/* Segment part of prev field	      */
	paddr_t cp_dataDMA;             /* Data Physical Address for command. */
	paddr_t cp_statDMA;             /* Status Packet Physical Address.    */
	paddr_t cp_reqDMA;              /* AutoRequestSense data Phy Address. */
	/******************* End EATA Command Packet Portion ******************/

	BYTE    ctlr_status;            /* Ctlr Status after ccb complete     */
	BYTE    SCSI_status;            /* SCSI Status after ccb complete     */
	/*********************** End EATAUSERCMD portion **********************/

	BYTE    flags;                  /* Active, ...                        */
	BYTE    reserved2;              /* alignment                          */
	paddr_t ccb_addr;               /* ccb physical address.              */
	gdev_dpbp dpbp;
	ushort  drive_num;
	gdev_dpbp (*intfunc)();
	BYTE    ccb_sense[DPT_SENSE];   /* Auto-Request sense data area.      */
	SG_T    ccbSG[DPT_SG_MAX];      /* DMA scatter/gather list            */
	BYTE    ccb_scratch[256];        /* spare buffer space, if needed      */
} DPT_CCB;
#pragma pack()

/******************************************************************************
** DPT Emulation drives information - One structure for each of two drives.  **
******************************************************************************/
#pragma pack(1)
typedef struct {
	int 		ha;
	BYTE		id;
	BYTE		lun;
	unsigned	heads;
	unsigned	sects;
} DPT_EM_T;
#pragma pack()

/*****************************************************************************
** gdev_ctl_block  - overlay definitions for dcb_lowlev fields:             **
** NOTE: these need to be coerced to/from the proper data types...          **
*****************************************************************************/
#define dcb_dpt_sp      dcb_lowlev[0]   /* pointer to EATA Status Packet.   */
#define dcb_dpt_ccbp    dcb_lowlev[1]   /* pointer to CCB for this command. */
#define dcb_dpt_SGsize  dcb_lowlev[2]   /* Scatter Gather list max elements */
#define dcb_dpt_emdr	dcb_lowlev[3]   /* Emulated Drives, if any.	    */
#define dcb_dpt_flags   dcb_lowlev[4]   /* Various controller flags.        */
#define dcb_dpt_inuse   dcb_lowlev[5]   /* In use for ioctl routines        */
#define dcb_dpt_ioctlCCB  dcb_lowlev[6] /* CCBs for ioctl routines          */

/** Definitions for dcb_dpt_flags	**/
#define	DPT_PRIMARY	0x0001
#define	DPT_SECONDARY	0x0002
#define DPT_EISA_CTLR   0x0004


/** Definitions for DPT_CCB->flags		**/
#define DPT_CCB_BUSY	0x01
#define DPT_INTERNAL	0x02


/*****************************************************************************
** gdev_parm_block  - overlay definitions for dpb_lowlev fields:            **
** NOTE: these need to be coerced to/from the proper data types...          **
*****************************************************************************/
#define dpb_dpt_ccbp    dpb_lowlev[0]   /* pointer to CCB for this device.  */
#define dpb_dpt_id      dpb_lowlev[1]   /* target ID of device.             */
#define dpb_dpt_lun     dpb_lowlev[2]   /* lun (on target) of device.       */
#define dpb_dpt_xfer    dpb_lowlev[3]   /* number of sectors transferred.   */



/*****************************************************************************
**                       Tuneable parameters and values                     **
*****************************************************************************/


/* The following are set in dpt20xx/space.c so they are tuneable: */

extern DPT_CCB *dpt_ccbinit();

/* function def's for space.c files  */

extern int dpt_bdinit();
extern int dpt_drvinit();
extern int dpt_cmd();
extern int dpt_open();
extern int dpt_close();
extern int dpt_mastint();
extern int dptioctl();
extern struct gdev_parm_block *dpt_diskint();

extern int dpt_tpbdinit();
extern int dpt_tpdinit();
extern struct gdev_parm_block *dpt_tapeint();

