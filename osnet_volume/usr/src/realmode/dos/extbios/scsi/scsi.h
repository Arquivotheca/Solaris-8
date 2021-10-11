/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)scsi.h	1.9	97/11/14 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   File name:		scsi.h
 *
 *   Description:    Contains definitions used by SCSI HBA drivers
 *
 *
 */

/*
 * boot optimization - only check for lun 0 as a bootable device.
 */

#define	LAST_LUN 1

/*
 * In the following two definitions, a "device" is
 * assumed to be a unique, * valid <target-id><LUN> pair...
 *
 * NOTE: the following 3 numbers allow the config table
 * to fit in 256 bytes.
 */
#define	SCSI_DISK_DEVS  56      /* Maximum number of Direct Access (disk) */
				/* devices on any single SCSI bus */
#define	SCSI_SEQ_DEVS   56      /* Maximum number of Sequential Access devices*/
				/* (tapes) on any single bus */
#define	SCSI_OTHER_DEVS 56      /* Max number of 'other' devs on a bus */

/*
 *	Definitions for use by SCSI device drivers and host adapter drivers
 */

/******************* Message Codes *******************/

#define	MC_CMD_DONE	0x0	/* Command Complete */
#define	MC_EXMSG	0x1	/* Extended Message */
#define	MC_SAVDP	0x2	/* Save Data Pointer */
#define	MC_RESP		0x3	/* Restore Pointers */
#define	MC_DISC		0x4	/* DISCONNECT */
#define	MC_IDERR	0x5	/* Initiator detected error */
#define	MC_ABORT	0x6	/* ABORT */
#define	MC_MREJECT	0x7	/* Message REJECT */
#define	MC_NOOP		0x8	/* No operation */
#define	MC_PARERR	0x9	/* Message Parity Error */
#define	MC_LKCOMP	0xA	/* Linked Command complete */
#define	MC_FLKCOMP	0xB	/* linked command complete (w/Flag) */
#define	MC_BRST		0xC	/* BUS device reset */
#define	MC_IDENT	0x80	/* IDENTIFY bit mask */
#define	MC_IDISC	0x40    /* disconnect bit for IDENTIFY */
#define	MC_LUMASK	0x7	/* mask target lu during reselection */

/*	Status Codes returned by SCSI target during status phase */

#define	S_GOOD		0x00	/* Target has successfully completed command */
#define	S_CK_COND	0x02	/* CHECK CONDITION 
				 * Any error, exception, or abnormal condition 
				 * that causes sense data to be set
				 */
#define	S_COND_MET	0x04	/* CONDITION MET/GOOD 
				 * returned by SEARCH DATA commands whenever
				 * search condition is satisfied
				 */
#define	S_BUSY		0x08	/* BUSY, the target is busy */
#define	S_IGOOD		0x10	/* INTERMEDIATE/GOOD
				 * returned for every command in a 
				 * series of linked commands, unless error
				 */
#define	S_IMET		0x12	/* INTERMEDIATE/CONDITION MET/GOOD  */
#define	S_RESERV	0x18	/* RESERVATION CONFLICT
				 * returned whenever an SCSI device attempts to 
				 * accesses a logical unit reserved by another 
				 * SCSI device
				 */


/*	SCSI Commands Group 0 */
#define	SC_TESTUNIT	0x00	/* TEST UNIT READY */
#define	SC_REZERO	0x01	/* Rezero unit */
#define	SC_REWIND	0x01	/* Rewind tape */
#define	SC_RSENSE	0x03	/* Request Sense */
#define	SC_FORMAT	0x04	/* Format */
#define	SC_RBLOCKL	0x05	/* READ BLOCK LIMIT */
#define	SC_REASSIGNB	0x07	/* Reassign blocks */
#define	SC_READ		0x08	/* Read */
#define	SC_RECEIVE	0x08	/* Receive */
#define	SC_WRITE	0x0A	/* write */
#define	SC_PRINT	0x0A	/* print - write to printer */
#define	SC_SEND		0x0A	/* send - write to processor device */
#define	SC_SEEK		0x0B	/* seek */
#define	SC_TRK_SLCT	0x0B	/* track select */
#define	SC_PRT_SLEW	0x0B	/* print and slew */
#define	SC_NOOP         0x0D    /* no operation (cancel previous op) */
#define	SC_READ_REV	0x0F	/* read reverse - tape */
#define	SC_FILEMARK	0x10	/* write file mark - tape */
#define	SC_FLSHB	0x10	/* flush buffer - printer */
#define	SC_SPACE	0x11	/* Space - tape */
#define	SC_INQUIRY	0x12	/* inquiry */
#define	SC_VERIFY	0x13	/* verify - tape */
#define	SC_RECOVR	0x14	/* Recover buffered data */
#define	SC_MSEL		0x15	/* Mode Select */
#define	SC_RELEASE	0x17	/* Release */
#define	SC_COPY		0x18	/* Copy */
#define	SC_ERASE	0x19	/* Erase - tape */
#define	SC_MSEN		0x1A	/* Mode Sense */
#define	SC_STRT_STOP	0x1B	/* Start/Stop unit */
#define	SC_LOAD		0x1B	/* Load/UNLOAD */
#define	SC_ULOAD	0x1B	/* Load/UNLOAD */
#define	SC_RDIAG	0x1C	/* Receive diagnostic results */
#define	SC_SDIAG	0x1D	/* Send Diagnostic */
#define	SC_REMOV	0x1E	/* prevent/allow medium removal */

/*
 * Code bits for SC_SPACE command  (go in bits 0-1 of byte 1):
 */

#define	SCSPAC_BLOCKS   0	/* Count spceifies data blocks */
#define	SCSPAC_MARKS    1	/* Count spceifies file marks  */
#define	SCSPAC_SEQMARKS 2	/* Count spceifies sequential file marks */
#define	SCSPAC_DATAEND  3	/* Space to end-of-data (ignore count) */

/*	SCSI Commands Group 1  - Extended */
#define	SX_READCAP	0x25	/* Read Capacity */
#define	SX_READ		0x28	/* Read */
#define	SX_WRITE	0x2A	/* Write */
#define	SX_SEEK		0x2B	/* Seek */
#define	SX_WRITE_VER	0x2E	/* Write and Verify */
#define	SX_VERIFY	0x2f	/* Verify */
#define	SX_HISEARCH	0x30	/* Search Data High */
#define	SX_EQSEARCH	0x31	/* Search Data Equal */
#define	SX_LOWSEARCH	0x32	/* Search Data Low */
#define	SX_DEFDATA	0x37	/* Read Defect Data */
#define	SX_COMPARE	0x39	/* Compare */
#define	SX_COPYVER	0x3A	/* Copy and Verify */

#define	CMDCTRL		0	/* control byte in the cdb is usually 0 */
#define	RQSENLEN	18	/* size of request sense buffer */

/* definitions for use in conjunction with gendisk.[ch] */
#define	ctl_t		struct gdev_ctl_block
#define	drv_t		struct gdev_parm_block
#define	cfg_t		struct gdev_cfg_entry
#define	dpb_scsi	dpb_lowlev[5]	/* must be cast on each use */


/*
 * values for sct_hatype:
 */
#define	SCHA_TMC8X0     1       /* Future Domain TMC-8X0 */
#define	SCHA_AD1540     2       /* Adaptec AHA-1540 */
#define	SCHA_WD7000ASC  3       /* Western Digital WD7000ASC */
#define	SCHA_WD7000EX   4       /* Western Digital WD7000EX */
#define	SCHA_BT742A     5       /* BusTek 742A */
#define	SCHA_AD1520     6       /* Adaptec AHA-1520 */
#define	SCHA_AD1740     7       /* Adaptec AHA-1740 */
#define	SCHA_DPT20XX    8       /* DPT 20xx series */

/*
 * structure of some data returned by SCSI commands is below:
 */

/* INQUIRY DATA */
struct  inquiry_data {
     unchar  inqd_pdt;       /* Peripheral Device Type (see below) */
     unchar  inqd_dtq;       /* Device Type Qualifier (see below) */
     unchar  inqd_ver;       /* Version #s */
     unchar  inqd_pad1;      /* pad must be zero */
     unchar  inqd_len;       /* additional length */
     unchar  inqd_pad2[3];   /* pad must be zero */
     unchar  inqd_vid[8];    /* Vendor ID  */
     unchar  inqd_pid[16];   /* Product ID */
     unchar  inqd_prl[4];    /* Product Revision Level */
};

/* values for inqd_pdt: */
#define	INQD_PDT_DA     0x00    /* Direct-access (DISK) device */
#define	INQD_PDT_SEQ    0x01    /* Sequential-access (TAPE) device */
#define	INQD_PDT_PRINT  0x02    /* Printer device */
#define	INQD_PDT_PROC   0x03    /* Processor device */
#define	INQD_PDT_WORM   0x04    /* Write-once read-many direct-access device */
#define	INQD_PDT_ROM    0x05    /* Read-only directe-access device */
#define	INQD_PDT_SCAN	0x06	/* Scanner device	(scsi2) */
#define	INQD_PDT_OPTIC	0x07	/* Optical device (probably write many scsi2) */
#define	INQD_PDT_CHNGR	0x08	/* Changer (jukebox, scsi2) */
#define	INQD_PDT_COMM	0x09	/* Communication device (scsi2) */
#define	INQD_PDT_NOLUN2 0x1f    /* Unknown Device (scsi2) */
#define	INQD_PDT_NOLUN  0x7f    /* Logical Unit Not Present */

#define	INQD_PDT_DMASK	0x1F	/* Peripheral Device Type Mask */
#define	INQD_PDT_QMASK  0xE0	/* Peripheral Device Qualifer Mask */


/* masks for inqd_dtq: */
#define	INQD_DTQ_RMB    0x80    /* Removable Medium Bit mask */
#define	INQD_DTQ_MASK   0x7f    /* mask for device type qualifier field */


/* READ CAPACITY DATA */
struct readcap_data {
     unchar rdcd_lba[4];     /* Logical Block Address (MSB first, LSB last) */
     unchar rdcd_bl[4];      /* Block Length (MSB first, LSB last) */
};


/* SENSE DATA (non-extended) */
struct nxsense_data
	{
	unchar  nxsd_err;       /* error class and code (see fields, below) */
	unchar  nxsd_lba[3];    /* logical block number */
	};

#define	NXSD_LBA_MASK0  0x1f    /* mask for block # portion of nxsd_lba[0] */

/* SENSE DATA (extended) */
struct exsense_data
	{
	unchar  exsd_err;       /* error class and code (see fields, below) */
	unchar  exsd_seg;       /* segment number */
	unchar  exsd_key;       /* sense key & other fields (defined below) */
	unchar  exsd_info[4];   /* information bytes */
	unchar  exsd_adlen;     /* additional sense length */
	unchar  exsd_adsd[1];   /* additional sense bytes */
	};

/* fields for nxsd_err and exsd_err bytes: */
union   sd_err_fields
    {
    struct
	{
	uint    sder_errcod : 4;        /* error code (0 for extended sense) */
	uint    sder_errcls : 3;        /* error class (7 for ext sense) */
	uint    sder_valid  : 1;        /* Address Valid bit */
	} sef;
    unchar  sec;
    };

/* fields for exsd_key byte: */
union   sd_key_fields
	{
	struct
		{
		uint    sdky_key    : 4;        /* actual sense key */
		uint    sdky_res1   : 1;        /* reserved */
		uint    sdky_ili    : 1;        /* Incorrect Length Indicator */
		uint    sdky_eom    : 1;        /* End of Medium bit */
		uint    sdky_fmark  : 1;        /* Filemark bit */
		} skf;
	unchar  skc;
	};


/* Read Block Limits Data */
struct blocklim_data
	{
	unchar  bkld_res;       /* reserved */
	unchar  bkld_max[3];    /* Maximum block length */
	unchar  bkld_min[2];    /* Minimum block length */
	};

/* Viper Tape (Archive) Mode Sense Data */
struct tpmsen_data
	{
	unchar  tmsd_dl;        /* Sense Data Length */
	unchar  tmsd_med;       /* Medium Type */
	unchar  tmsd_modes;     /* WP bit,  buffer mode, speed (below) */
	unchar  tmsd_bdl;       /* Block Descriptor Length */
	/* Following 8 bytes are Block Descriptors */
	unchar  tmsd_den;       /* Density code (below) */
	unchar  tmsd_nblk[3];   /* Number of blocks field */
	unchar  tmsd_res;       /* Reserved */
	unchar  tmsd_blkl[3];   /* Block Length */
	};

/* Field for accessing tmsd_modes byte */
union tms_mode_fields
	{
	struct
		{
		uint    tmsm_speed : 4; /* Speed */
		uint    tmsm_bufm  : 3; /* Buffer mode (below) */
		uint    tmsm_wp    : 1; /* Write-Protect bit */
		} tmf;
	unchar  tmc;
	};

/* Definitions for Density Code field */
#define	TMSD_60MB       5       /* Low-density (9-trk) mode */
#define	TMSD_125MB      0x0f    /* Medium-density (15-trk) mode */
#define	TMSD_150MB      0x10    /* High-density (18-trk) mode */

/*
 * Public function prototypes
 */
unchar next_MDB_code();
void scsi_dev(ushort base_port, ushort targid, ushort lun);
void scsi_dev_pci(ushort base_port, ushort targid, ushort lun,
    unchar bus, ushort ven_id, ushort vdev_id, unchar dev, unchar func);
int scsi_dev_bootpath(ushort base_port, ushort targid, ushort lun,
	char far *path);
struct bdev_info *scsi_dev_info(void);
void llshift(union halves far *hp, ushort dist);
void lrshift(union halves far *hp, ushort dist);
long longaddr(ushort offset, ushort segment);
