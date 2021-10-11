/*
 * Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
 */

#ifndef _SYS_DKTP_CHS_H
#define	_SYS_DKTP_CHS_H

#ident	"@(#)chs.h	1.8	99/11/01 SMI"

/* Card Types and Ids */
#define		CHS_PCIVPR	1	/* PCI card  IBM Copperhead */

#define		MAX_HBA_TYPES	1	/* # of supported HBAs types */


/*
 * PCI configuration space offsets
 */
#define	PCI_CMDREG	0x04
#define	PCI_ILINEREG	0x3c
#define	PCI_BASEAD	0x10
#define	PCI_COMM_IO	0x0001
#define	PCI_COMM_ME	0x0002
#define	PCI_COMM_PARITY_DETECT	0x0040
/*
 * Bits in PCI_CMDREG
 */
#define	PCI_CMD_IOEN	1

#define	DACVPR_VID	0x1014
#define	DACVPR_DID1	0x002E

/* handy for shifts */

#define	MS_NIBBLE	12	/* shift slot_num 12 bits left */
#define	MS2_NIBBLE	 8	/* shift slot_num  8 bits left */


/* For command/status handshaking */
#define	DAC_CHKINTR	0x01	/* Check for  EISA SDBELL Interrupt */

/* Commands */
#define	TYPE_0		0x0
#define	TYPE_1		0x1
#define	TYPE_2		0x2
#define	TYPE_3		0x3
#define	TYPE_4		0x4
#define	TYPE_5		0x5
#define	TYPE_6		0x6

/* Type 1 Command used */
#define		DAC_READ	0x02	/* Read */

/* Type 3 Command  */
#define		DAC_DCDB	0x04	/* Direct CDB Command */

/* Type 5 Command */
#define		DAC_CONFIG	0x07	/* Read-ROM-Configuration */
#define		DAC_ENQ_CONFIG	0x05	/* Get adapter status */


/* Uniform status codes */
#define	CHS_SUCCESS		0x00
#define	CHS_ERROR		0x01
#define	CHS_RETRY		0x02
#define	CHS_INVALDEV  		0x04	/* Invalid device address or attempt */
					/* to xfr more than 0xFC00 bytes */




/* Generic status/error codes */
#define		NO_ERROR	0x00	/* Normal Completion of command */
#define		E_ILL_OPCODE	0x104	/* Unimplemented opcode */

/* Status/Error codes for read command */
#define		E_UNREC		0x01	/* Unrecoverable data error */
#define		E_NODRV		0x02	/* System Driver does not exist */
#define		E_LIMIT		0x105	/* Attempt to read beyond limit */

/* Status/Error codes for read-rom config command */
#define		E_CHK_SUM	0x02	/* Checksum error in read ROM config. */
#define		E_NO_MATCH	0x105	/* No match between NVRAM config and */
					/* EEPROM config */

/* Status/Error codes for direct-CDB command */
#define		E_CHK_COND	0x02	/* Check condition received */
#define		E_DEV_BUSY	0x08	/* Device is busy */
#define		E_TIMEOUT1	0xF	/* Selection timeout */
#define		E_TIMEOUT2	0xE	/* Selection timeout */
#define		E_INVALDEV  	0x105	/* Invalid device address or attempt */
					/* to xfr more than 0xFC00 bytes */

/* Length of scsi inquiry command */
#define		CDB_SCINQ_LEN		6

/* Length of scsi read/readcap command */
#define		CDB_SCREAD_LEN		10

/* Length of scsi lock command for removable medium */
#define		CDB_SCREMV_MED		6

/* Length of scsi start-stop motor command */
#define		CDB_SCSTRT_STP		6

/* Miscellaneous info. */
#define	DUMMY		0x0	/* dummy parameter-don't care */
#define	MAX_PCI_SLOTS	0x20	/* 32 slots from 0x1 to 0x1F */
#define	SYS_DRV		0x0	/* indicates system drive   */
#define	NONSYS_DRV	0x1	/* indicates non-sys drive   */
#define	SYS_DRV_BSIZE	512	/* block-size of system drives */
#define	MAX_SENSE_LEN	64	/* maximum sense-length	*/
#define	MAX_COUNTER	300000	/* counter for loop-timeout */
#define	MAX_SYS_DRVS	 8	/* max. no. of system drives */

#define	MAX_POSS_TGTS	16	/* max. possible tgts  */
#define	MAX_POSS_CHNS	10	/* assume max. possible chans */
				/* for any current and future model */

/* macro used to access the correct chn:tgt dev_info */
#define	SET_ARR_CHN_TGT(chn, tgt)			\
	(tgt)++;						\
	if ((tgt) == MAX_POSS_TGTS) {			\
		(chn)++;					\
		(tgt) = 0;				\
	}


/* system-drive states */
#define		ONLINE			0x03
#define		CRITICAL		0x04
#define		OFFLINE			0xFF

/* lower 5 bits give device type from inquiry data */
#define		DEV_TYPE(arg)		((arg) & 0x1F)
/* upper 3 bits give device qualifier from inquiry data */
#define		PERI_QUAL(arg)		(((arg) & 0xE0) >> 5)

/* DAC Direct CDB Structure */
struct daccdb {
	unchar cdb_unit;	/*  Chan(upper 4 bits),target(lower 4) */
	unchar cdb_cmdctl;	/* Command control */
	ushort cdb_xfersz;	/* Transfer length */
	ulong  cdb_databuf;	/* 32-bit addr to data buffer in memory */
	unchar cdb_cdblen;	/*  Size of CDB in bytes */
	unchar cdb_senselen;	/*  Size of sense length in bytes */
	union {
		struct {
			unchar SGLength;	/* SG length */
			unchar filler1;
			unchar cdb_data[12];
			unchar cdb_sensebuf[64];
			unchar cdb_status;
			unchar filler3[3];
		} v;
	} fmt;
};


struct hwdisk {
	unchar board_disc[8];
	unchar processor[8];
	unchar NoChanType;
	unchar NoHostIntType;
	unchar Compression;
	unchar NvramType;
	ulong ulNvramSize;
};


struct CHUNK {
	unchar chn;
	unchar tgt;
	ushort reserved;
	ulong ulstartSect;
	ulong ul_nosects;
};

#define	CHS_MAX_CHUNKS  16

struct logical_drive{
	ushort userfield;
	unchar state;
	unchar raidcache;
	unchar NoChunkUnits;
	unchar stripsize;
	unchar params;
	unchar Reserved;
	ulong ulLogDrvSize;
	struct CHUNK chunk[CHS_MAX_CHUNKS];
};


struct devstate {
	unchar Initiator;
	unchar Params;
	unchar miscflags;
	unchar state;
	ulong ulBlockCount;
	unchar DeviceID[28];
};

struct 	V_CONFIG {
	unchar n_sysdrives;
	unchar Date[3];
	unchar init_id[4];
	unchar host_id[12];
	unchar time_sign[8];
	struct {
		uint usCfgDrvUpdateCnt	:16;
		uint concurDrvStartCnt	:4;
		uint startupdelay	:4;
		uint auto_reaarrange	:1;
		uint cd_boot		:1;
		uint cluster		:1;
		int reserved		:5;
	} useroptions;
	ushort userfield;
	unchar  RebuildRate;
	unchar  Reserve;
	struct	hwdisk  hardware_disc;
	struct	logical_drive log_drv[MAX_SYS_DRVS];
	struct	devstate dev[MAX_POSS_CHNS][MAX_POSS_TGTS];
};

#define	CHS_DAC_TGT_STANDBY(s)	((s & 0x8f) == 0x01)
#define	CHS_DAC_TGT_DISK(s)	((s & 0x1f) == 0x00)

#define	VIPER_OFFLINE	0x02

union  CONFIG {
	struct V_CONFIG v_conf;
};


struct TGT {
	unchar 	target	: 1;	/* bit indicates target used in system-drive */
};

struct CHN {			/* used for any model  */
	struct TGT tgt[MAX_POSS_TGTS]; 	/* targets on a channel */
};


/* Information returned by  CHS_DAC_ENQUIRY command */
struct ENQ_CONFIG {
	unchar nsd;			/* Total Number of logical drives */
	unchar MiscFlag;
	unchar SLTFlag;
	unchar BSTFlag;
	unchar PwrChgcnt;
	unchar WrongAdrCnt;
	unchar UnidentCnt;
	unchar NVramDevChgCnt;
	unchar CodeBlkVersion[8];
	unchar BootBlkVersion[8];
	ulong  sd_sz[MAX_SYS_DRVS];	/* sizes of system drives */
					/* in sectors */
	unchar ConCurrentCmdCount;
	unchar MaxPhysicalDevices;
	ushort usFlashRepgmCount;
	unchar DefuncDiskCount;
	unchar RebuildFlag;
	unchar OffLineLogDrvCount;
	unchar CriticalDrvCOunt;
	ushort usConfigUpdateCount;
	unchar BlkFlag;
	unchar res;
	ushort AddrDeadDisk[MAX_POSS_CHNS][MAX_POSS_TGTS];
	/* Note that if get adapter status command is sent wth desc field */
	/* not equal to 0 we would get additional VPD info and devstate	*/
	/* which are not needed at this time so we won't define it */

};
#define	TRUE			1
#define	FALSE			0
#define	SUCCESS			1
#define	FAIL			0

/* dmc specific registers */
/* Viper specific registers */


#define	CPR_REG			0x00		/* Command Port */
#define	APR_REG			0x04		/* Attention Port */
#define	SCPR_REG		0x05		/* Subsystem Control Port */
#define	ISPR_REG		0x06		/* Interrupt Status Port */
#define	CBSP_REG		0x07		/* Command Busy/Status Port */
#define	HIST_REG		0x08		/* Host Interrupt status */
#define	CCSAR_REG		0x10		/* Command Channel address */
#define	CCCR_REG		0x14		/* Command Channel Control */
#define	SQHR_REG		0x20		/* Status Queue head */
#define	SQTR_REG		0x24		/* Status Queue Tail */
#define	SQER_REG		0x28		/* Status Queue End */
#define	SQSR_REG		0x2c		/* Status Queue Start */
#define	VIPER_GSC_MASK		0x000f
#define	VIPER_STATUS_MASK	0xff0f



#define	VIPER_NOERROR		0
#define	VIPER_TGT_BUSY		0x08		/* Target returns busy */
#define	VIPER_INV_DEV		0x5005		/*
						 * Invalid device address
						 * in DCDB
						 */
#define	VIPER_INV_PARAMS	0x5204		/* Invalid params in DCDB */
#define	VIPER_CMD_TIMEOUT	0x0e		/* Command timeout */
#define	VIPER_SEL_TIMEOUT	0xf00f		/* Selection timeout */



char *viper_gsc_errmsg[] = {
	"success",
	"recovered error",
	"check condition",
	"Invalid opcode in command",
	"Invalid parameters in command block",
	"System Busy",
	"Undefined error",
	"Undefined error",
	"Adapter hardware error",
	"Adapter firmware error",
	"Download jumper set",
	"Command completed with errors",
	"Logical drive not available at this time",
	"System command timeout",
	"Physical drive error",
};





#pragma pack(1)
	struct viper_statusq_element {
		unchar fill1;
		unchar stat_id;
		unchar bsb;
		unchar esb;
	};
#pragma pack()

#define	VIPER_STATUS_QUEUE_ELEMS	2


/* Total statusq's + 1 for 4 byte boundary alignment */
struct viper_statusq_element totalstatusq[
		((MAX_PCI_SLOTS + 1) * VIPER_STATUS_QUEUE_ELEMS) +1];

struct viper_statusq_element *nextavailstatusq;



#define	VIPER_CCCR_SS		0x0002
#define	VIPER_CCCR_SEMBIT	0x0008
#define	VIPER_CCCR_ILE		0x0010


#define	VIPER_HIST_GHI		0x04
#define	VIPER_HIST_EI		0x80
#define	VIPER_RESET_ADAPTER	0x80
#define	VIPER_OP_PENDING	0x01
#define	VIPER_ENABLE_BUS_MASTER	0x02


#define	VPR_CMD_OPCODE		0x0
#define	VPR_STATID		0x1
#define	VPR_DCDB_ADDR_START	0x8
#define	VPR_DRV			0x2
#define	VPR_CMD_BLOCK_0		0x4
#define	VPR_CMD_BLOCK_1		0x5
#define	VPR_CMD_BLOCK_2		0x6
#define	VPR_CMD_BLOCK_3		0x7
#define	VPR_COUNT0		0xc
#define	VPR_COUNT1		0xd

#endif	/* _SYS_DKTP_CHS_H */
