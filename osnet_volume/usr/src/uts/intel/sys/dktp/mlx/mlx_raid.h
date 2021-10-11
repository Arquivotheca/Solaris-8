/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MLX_MLX_RAID_H
#define	_SYS_DKTP_MLX_MLX_RAID_H

#pragma ident	"@(#)mlx_raid.h	1.4	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#define	NCHNS			3
#define	MAX_TGT			16
#define	MAX_SPANS		4
#define	MAX_ARMS		8

#define	MAX_SYSDRIVES		32
#define	MAX_BAD_DATA_BLKS	100

/*
 * APPENDIX A :	THE DAC960 CONFIGURATION TABLE
 *
 *
 * Structure of	the DAC960 CONFIGURATION TABLE :
 *
 * The DAC960 configuration table (CORE_CFG) consists of a set of system-drives
 * (SYS_DRV), each of which can	have upto 4 SPANs. Each	SPAN consists of upto
 * 8 DEVICEs.  Data is striped across the DEVICEs of a system drive, and if
 * there are multiple SPANs, across the	spans themselves. A DEVICEmay
 * actually map	onto a sub-section of a	physical disk. Physical	disks may be
 * shared across different system-drives. Each of those	SYS_DRV	entries	in
 * the configuration table will	have a lower level DEVICE entry	containing
 * the same physical disk (at address chn:tgt).	Note, however, that each of
 * these DEVICE	entries	identifies different (non- overlapping)	sections of
 * the physical	disk.
 *
 * The DAC960 configuration table also contains	a second section which has
 * information about the device	at each	of the possible	SCSI addresses.	The
 * structure of	the entry for each device-address is the same as the
 * structure of	information returned by	the GET	DEVICE STATE command
 * described earlier.
 */

/*
 * APPENDIX B :	THE DAC960 CONFIG2 (CONTROLLER PARAMETERS) TABLE
 *
 *
 * Structure of	the DAC960 CONFIG2 TABLE :
 *
 * The DAC960 CONFIG2 TABLE contains parameters	associated with	the hardware,
 * with	the physical distribution of data across disks (stripe-size, etc),
 * SCSI-transfer, disk spinup, SCSI-Target features, Host-
 * channel-configuration, and serial port parameters. Of these some sections
 * are reserved	for some members of the	DAC960 family. For example, the	disk
 * spinup parameters are passed	through	the EISA configuration in the DAC960
 * EISA	line. The serial ports exist only on the DAC960S (SCSI-SCSI).
 *
 * Config2 contains parameters associated with the controller and is divided
 * into	the following sections :
 *
 * HARDWARE   (hardware	features like battery-backup) PHYSICAL	 (sectorsizes,
 * stripesize, etc) SCSI_XFR   (sync,wide,etc) SSTARTUP	  (spinup control)
 * SCSI_TGT   (misc tgt	features) HOST_CFG   (Host channel config to control
 * LUN mapping)	SLIP_PM	   (Serial port	parameters)
 */

struct HARDWARE	{
	uchar_t		hctl;
	uchar_t		vendor_flags;
	uchar_t		OEM_code;
	uchar_t		rsvd;	/* For future use : set	to 0  */
};

/* hctl	: bit mask  */
#define	BBACKEN	   0x01		/* DAC960E, DAC960P  */
#define	SCSI_EAN   0x02		/* reserved */
#define	WCACHEEN   0x10		/* reserved */
#define	MASSLVEN   0x20		/* reserved */
#define	MS_MDLY	   0x40		/* reserved */

/* vendor_flags	:  */
#define	V_NDISFC  0x04		/* When	set, disables disc on first command  */
#define	V_ARM	  0x20		/* Automatic Rebuild Management	 */
#define	V_OFM	  0x40		/* Operational Fault Management	 */

/* OEM_code :  */
#define	OEM_MLX0   0x00
#define	MSMASTER   0x40		/* reserved */

struct PHYSICAL	{
	uchar_t		phy_sector;
	uchar_t		log_sector;
	uchar_t		blk_factor;
	uchar_t		fw_ctl;
	uchar_t		df_rc_rate;
	/*
	 * cod_ctl controls the	config_on_disk stuff.  Note that this byte
	 * only	reflects
	 */
	/* the cod_ctl byte in NVRAM.  The bits	here are mapped	as follows : */
	/* Bit 0	: 1 = enable cod, 0 = disable cod */
	/* Bit 1,2,3	: User selected	startup	option.	*/
	/* STRT_OPT_NO_CHANGE			0x00  */
	/* STRT_OPT_NO_LUN_CHANGE	0x01 */
	/* STRT_OPT_NO_LUN_OFFLINE	0x02 */
	/* STRT_OPT_LUN0_NO_CHANGE	0x03 */
	/* STRT_OPT_LUN0_NO_OFFLINE	0x04 */
	/* Bit 4	: Extended missing drive reassignment support. */
	/* 0 =	reassign only to same slot if possible */
	/* 1 =	reassign wherever possible on controller  */
	/* (for	removable media	cases) */
	/* Bit 5-7	: free */
	uchar_t		cod_ctl;

	uchar_t		rsvd[2];
};

/* PHYSICAL :  */
/* phy_sector		physical sector, in multiples of 512 (BLKSIZ) */
/* log_sector		logical	sector,	in multiples of	512 (BLKSIZ) */
/*
 * blk_factor		block factor (determines stripesize and
 * controller logical block (cl_block) size )
 */
/*
 * fw_ctl		firmware ctl - RA-enable, RAID 5 Algorithm
 * type, etc.
 */
/* df_rc_rate		default	rebuild	rate (0-50) */
/* cod_ctl		Config on Disk control field */
/* rsvd			reserved */

/*
 * phy_sector :		This parameter is restricted to	a value	of 1 on	all
 * models currently.
 */
/* This	corresponds to a disk sector-size of 512 bytes.	*/
/* log_sector :		Set to 1 */
/*
 * blk_factor :			This parameter controls	the stripe-size
 * (which is the amount	of
 */
/* data	which is contiguous on a disk).	 */
/* Stripe size can be varied on	all controllers	from 8KB to 64KB by  */
/* varying blk_factor from 1 to	8. In general, sequential throughput */
/* increases with stripe-size. */
/* fw_ctl :		All controllers	support	these. */
#define	RDAHEN	0x01	/* Enable read-ahead on	the controller	*/
#define	BILODLY	0x02
#define	REASS1S	0x10	/* If enabled reassigns	only 1 sector per command. */
/* The sector reassigned is the	sector returning an */
/* error with a	valid sense information.  */
#define	R5ALLS	0x80		/* Raid5 algo control, 1=> Left	Symmetric  */

/*
 * df_rc_rate :		This is	the default value of rc_rate set at
 * power up on the controller.
 */
/* (See	REBUILD	CONTROL	command.)  */

/*
 * SCSI_XFR : These parameters are for the disk-side channels only. They are
 * applicable to
 */
/* the DAC960P.	*/
/* schn_pm : bit mask  */
#define	SCHN_TAG_BIT	0x80	/* set to 1 for	tag support  */
#define	SCHN_8B_BIT	0x04	/* set to 1 to restrict	to 8-bit  */
#define	SCHN_SPD_MASK	0x03
#define	SCHN_SPD_FAST	0x03	/* 10 MB/s  */
#define	SCHN_SPD_8MBS	0x01	/* 8 MB/s  */
#define	SCHN_SPD_NORM	0x02	/* 5 MB/s  */
#define	SCHN_SPD_ASYN	0x00	/* Asynch  */

struct SCSI_XFR	{
	uchar_t		schn_pm[6];
	uchar_t		rsvd[2];
};


/*
 * SSTARTUP : These control the	spin-up	of devices. Parameters in this
 * section are applicable to
 */
/* the DAC960P.	*/
/* Start-modes */
/* 1. spin/start devs (devs await ssu) - devspersp,spwait  */
/* 2. start devs (devs spin on pwr) - seqdly,initialwait */
/* 3. wait ssu (idle till ssu cmd) - devspersp,spwait */
/* All spinup is done with tgt major (id0 on all chns, then 1, etc) */

/* start_mode :	 */
#define	AUTOSPIN	0	/* issue SSU to	all devices automatically  */
#define	PWRSPIN		1	/* devices spin	on power  */
#define	WSSUSPIN	2	/* await system	SSU, then start	devices	 */

/*
 * Of the above	options, only AUTOSPIN and PWRSPIN are supported on the
 * DAC960P.
 *
 * When	the controller is set to AUTOSPIN, it spins up ndevs devices at	a
 * time, then waits for	dly1 seconds before going back and spinning ndevs
 * evices.
 *
 * When	the controller is set to PWRSPIN, it assumes that the devices spin-up on
 * power-on. It	waits for dly1 seconds before scanning the bus and then
 * scans all channels for devices with id of 0.	It then	waits for dly2
 * ds before scanning for devices with id of 1,	and so on.
 */

struct SSTARTUP	{
	uchar_t		start_mode;	/* AUTOSPIN / PWRSPIN / WSSUSPIN */
	uchar_t		ndevs;	/* devs_per_sp / (undefined) / devs_per_sp  */
	uchar_t		dly1;	/* dev_sp_wait / initial_dly / dev_sp_wait  */
	uchar_t		dly2;	/* (0)/ seqence_dly / (0) */
	uchar_t		rsvd[4];
};

/* XXX MISSING DEFINES */

struct SCSI_TGT {
	int one;
	int two;
};

struct HOST_CFG {
	int one;
};

struct SLIP_PM {
	char one[6];
};

struct CONFIG2 {
	struct HARDWARE	hdw;	/* All controllers bytes */
	struct PHYSICAL	phy;	/* All controllers bytes */
	struct SCSI_XFR	sxf;	/* DAC960P (PCI) bytes */
	struct SSTARTUP	spn;	/* DAC960P (PCI) bytes */
	struct SCSI_TGT	stg;	/* reserved bytes */
	struct HOST_CFG	hcf;	/* reserved bytes */
	struct SLIP_PM	slp[2];	/* reserved *2 bytes */
	uchar_t		rsvd[10];
	short		chksum;	/* Sum of shorts upto this point */
};				/* Total size */

/* CONFIG_LABEL	provides holds a name and an id; these together	help in	the   */
/* identification of a set of configured disks.	*/
struct CONFIG_LABEL {
	uchar_t		name[64];	/* free	format - usually text  */
	uint_t		config_id;	/* a unique number * */
	ushort_t	seq_num;	/* # of times this config changed */
	uchar_t		reserved[2];

};

/*
 * DISK_CONFIG_HDR is the structure of the header portion of the disk
 * configuration information.
 * This	structure contains the following sizing	information :
 * . max_sys_drives
 * . max_arms
 * . max_spans
 * . max_chns (actual channels on the controller)
 * . max_tgts_per_chn
 * This	sizing information should be used by utilities to determine the
 * size	of the structures pertaining to	the configuration.
 */

struct DISK_CONFIG_HDR {
	ulong_t		magic_num;	/* determines config format  */
	struct CONFIG_LABEL label;
	ulong_t		time_stamp;	/* seconds elapsed since Jan 1,	1980  */
	uchar_t		max_sys_drives;	/* must	be set to MAX_SYSDRIVES	   */
	uchar_t		max_arms;	/* must	be set to MAX_VDRIVES	 */
	uchar_t		max_spans;	/* must	be set to MAX_SPANS  */
	uchar_t		max_chns;	/* actual channels on the controller  */
	uchar_t		max_tgts_per_chn; /* # targets supported per channel  */
	uchar_t		max_luns_per_tgt; /* max luns per target - set to 1 */
	uchar_t		reserved1;
	uchar_t		disk_chn_num;	/* channel # of	this drive  */
	uchar_t		disk_tgt_num;	/* target id of	this drive  */
	uchar_t		disk_lun_num;	/* lun num of this drive  */
	uchar_t		reserved2;
	uchar_t		reserved3;

	struct CONFIG2	cfg2;	/* copy	of config 2  */
	uchar_t		reserved4[30];	/* reserved for	future use  */
	ushort_t	checksum;	/* checksum of whole header portion */
};

struct PHYS_DRV	{
	uchar_t		present;	/* Bit 1-7	Reserved */
	/*
	 * Bit 0		set to 1 by the	configuration utility if
	 * configured, else 0
	 */
	/*
	 * Bit 7	Tag indicates whether tag queueing is supported	(0 =
	 * not supported, 1 = supported).
	 */
	/*
	 * Bit 6	Wide indicates the bus width (0	= 8 bit	SCSI, 1	=
	 * Wide	SCSI)
	 */
	/*
	 * Bit 5	Fast indicates the device speed	(0 = Normal SCSI (5
	 * Mhz), 1 = Fast SCSI)
	 */
	/*
	 * Bit 4	Sync indicates whether the device is synchronous or
	 * not (0 = async, 1 = sync).
	 */
	/* Bit 2,3	Reserved */
	/* Bit 1,0	Type is	the device type	: */
	/* 00	Reserved */
	/* 01	Disk */
	/* 10	Non-disk */
	/* 11	Reserved */
	/*
	 * This	field is set by	the configuration utility, and not by the
	 * DAC960.
	 */
	uchar_t		status;	/* current status of this drive	 */
	/* 00h	Dead */
	/* 02h	Write-Only */
	/* 03h	Online */
	/* 10h	Standby	*/
	uchar_t		reserved;	/* not used at present	*/
	uchar_t		reserved1;	/* not used at present	*/
	uchar_t		reserved2;	/* not used at present	*/
	ulong_t		configured_size;	/* Size	of drive, in sectors */
};

struct DEVICE {
	uchar_t		address; /* bits 0-3 : target, bits 4-7 : chan */
};

struct SPAN {
	ulong_t		start_blk;
	ulong_t		num_blks;
	struct DEVICE	arm[MAX_ARMS];
};

struct SYS_DRV {
	uchar_t		status;	/* 0x03=online, 0x04=critical, 0xff=offline */
	uchar_t		extended_status;	/* not used at present	*/
	uchar_t		modifier1;	/* not used at present	*/
	uchar_t		modifier2;	/* not used at present	*/
	/* raid	level :	*/
	/* Bit 7    : 0	= write	through, 1 = write back	*/
	/* Bit 6-0 : 0,	1, 5, 6, 7 */
	uchar_t		raid_level;
	uchar_t		num_arms;	/* # arms in each span */
	uchar_t 	num_spans;	/* # spans in this sys drive */
			uchar_t reserved1;
	struct SPAN	span[MAX_SPANS];
};

/*
 * Physical Drive Information
 *
 * Present indicates whether the device	has been configured or not (0 =	not
 * configured, 1 = configured).
 *
 * Tag indicates whether tag queueing is supported (0 =	not supported, 1 =
 * supported).
 *
 * Wide	indicates the bus width	(0 = 8 bit SCSI, 1 = Wide SCSI)
 *
 * Fast	indicates the device speed (0 =	Normal SCSI (5 Mhz), 1 = Fast SCSI)
 *
 * Sync	indicates whether the device is	synchronous or not (0 =	async, 1 =
 * sync).
 *
 * Fast20 indicates whether 20MHz  transfers are supported. (0 = Not Supported,
 * 1 = Supported)
 *
 * Type	is the device type :
 *
 * 00	Any other type.	01	Disk (direct access storage device) 10
 * Sequential access device 11	CDROM or WORM device
 *
 * Device State	can be one of the following :
 *
 * 00h	Dead 02h	Write-Only 03h	Online 10h	Standby
 *
 * Sync	Multiplier is the Synchronous transfer rate multiplier
 *
 * Offset indicates the	synchronous offset in bytes.  This value can vary from 1
 * to 16.  A value of 0	is used	for asynchronous SCSI.
 *
 * Disk	Size is	the size of the	disk (in sectors).
 *
 * Note	: Device Parameters are	set by the configuration utility, and NOT by the
 * DAC960. The DAC960 does not change the lower	4 bits - it only modifies the
 * upper 4 bits	of byte	1.
 *
 * Byte	0 is written to	only by	the configuration utility (and not the
 * controller).	 The Mylex configuration utility sets this field to 0x01 for
 * all configured devices.  Bytes 2 & 4	are reserved for the configuration
 * utility, and	byte 7 is reserved for the controller.
 */

/* BDT_TABLE is	the structure holding all the bad data information for all  */
/* system drives. */
/* MAX_SYS_DRIVES = 32,	and MAX_BAD_DATA_BLOCKS	= 100 */
struct BDT_TABLE {
	uchar_t		max_sys_drives;	/* must	be set to MAX_SYS_DRIVES */
	uchar_t		max_chns;	/* # loaded channels on controller */
	uchar_t		max_tgts_per_chn; /* # targets per channel */
	uchar_t		reserved;
	/*
	 * bad_data_cnt	holds the number of bad	data blocks for	each system
	 * drive
	 */
	uchar_t		bad_data_cnt[MAX_SYSDRIVES];
	/*
	 * list	holds the block	numbers	of all the bad data blocks for each
	 * system
	 */
	/* drive. */
	uint_t		list[MAX_SYSDRIVES][MAX_BAD_DATA_BLKS];
	/* soft_err_cnt	holds the per-device soft error	counts.	*/
	/* misc_err_cnt	holds the per-device misc error	counts.	*/
	/*
	 * busy_or_par_err_cnt holds the per-device busy or parity error
	 * counts.
	 */
	ushort_t	soft_err_cnt[NCHNS][MAX_TGT];
	ushort_t	misc_err_cnt[NCHNS][MAX_TGT];
	ushort_t	busy_or_par_err_cnt[NCHNS][MAX_TGT];
	/* dirty_count keeps track of how many changes have been made to this */
	/* table since the last	time it	was written to disk */
	ushort_t	dirty_count;

	ushort_t	checksum;
};

struct SYS_DRV_TBL {
	uchar_t		num_sys_drvs;	/* # configured system drives  */
	uchar_t		reserved1[3];
	struct SYS_DRV	s_d_e[MAX_SYSDRIVES];
};

struct CORE_CFG	{	/* the 'core' of the configuration information */
	struct SYS_DRV_TBL sys_drives;
	struct PHYS_DRV	p_d_e[NCHNS][MAX_TGT];
};

/*
 * APPENDIX C :	STATISTICS RELATED STRUCTURES
 *
 * The controller supports two statistics-related commands :
 *
 * GET SD STATS, and GET PD STATS.
 *
 * The information returned by these commands is given below.
 */

/*
 * GET SD STATS	: Returns 288 bytes (32	bytes of controller related info, 256
 * bytes of system drive related info).
 */

struct SDVST {			/* 32 bytes of statistics per sys drive	 */
	long		reads;		/* # read commands  */
	long		read_secs;	/* # sectors read  */
	long		writes;		/* # write commands  */
	long		write_secs;	/* # sectors written  */
	long		read_hits;	/* # read sectors from cache  */
	uchar_t		rsvd[12];	/* 12 reserved bytes  */
};

struct SDSTATS {
	ushort_t	etime;
		/* Elapsed time	(secs) since start of controller  */
	uchar_t		parerrs;	/* # of parity errs (recovered)	*/
	uchar_t		reserved[29];	/* 29 bytes of reserved	area  */
	struct SDVST	sdvst[8];
		/* Statistics associated with each sys drive  */
};


/*
 * GET PD STATS	: Returns upto 1120 bytes of information. Actual amount
 * returned depends on NCHNS and MAX_TGT. The controller returns 32 bytes per
 * device.
 */

struct PDSTAT {			/* 32 bytes of statistics per device  */
	long		reads;		/* # read commands  */
	long		read_secs;	/* # sectors read  */
	long		writes;		/* # write commands  */
	long		write_secs;	/* # sectors written  */
	uchar_t		rsvd[16];	/* 16 reserved bytes  */
};

/*
 * DISK_CONFIG is the structure	of the configuration information that resides
 * on disk and in controller
 */
/* memory. */
struct DISK_CONFIG {
	struct DISK_CONFIG_HDR header;	/* header info	*/
	struct CORE_CFG	core_cfg;	/* core	configuration  */
	uchar_t		reserved[128];	/* reserved for	future use  */
};

/* XXX MISSING DEFINES */

struct ENQUIRY {
	uchar_t	nsd;
	uchar_t	resvd1[3];
	ulong_t sd_sz[MAX_SYSDRIVES]; 	/* sizes of system drives in sectors */
	ushort_t fw_age;		/* age of the firmware flash eep */
	uchar_t write_bk_err;		/* flag indicating write back error */
	uchar_t nfree_chg;		/* # of free entries in chg list */
	struct {
		uchar_t	version;
		uchar_t	release;
	} fw_num;			/* firmware release & version number */
	uchar_t stbyrbld;		/* standby drive being rebuilt to */
					/* replace a dead drive */
	uchar_t max_cmd;		/* # Max iop Q tables */
	uchar_t noffline;		/*  # drives offline */
	uchar_t resvd2[3];
	uchar_t ncritical;		/* # drives critical */
	uchar_t resvd3[3];
	uchar_t ndead;			/* # drives dead */
	uchar_t resvd4;
	uchar_t rbldcnt;		/* # rebuilding */
	uchar_t misc;			/* misc flags */
	ushort_t deaddrv[MAX_SYSDRIVES]; /* dead drive list */
};

struct ENQUIRY2 {
	uchar_t	hw_id[4];
	uchar_t	fw_id[4];
	ulong_t	resvd1;
	uchar_t	conf_chns;
	uchar_t	act_chns;
	uchar_t	max_tgts;
	uchar_t	max_tags;
	uchar_t	max_sys_drives;
	uchar_t	max_arms;
	uchar_t	max_spans;
	uchar_t	resvd2[5];
	ulong_t	dram_size;
	ulong_t	cache_size;
	ulong_t	flash_size;
	ulong_t	nvram_size;
	ushort_t resvd3;
	ushort_t clock_speed;
	ushort_t memory_speed;
	ushort_t hw_speed;
	uchar_t	resvd4[12];
	ushort_t max_cmds;
	ushort_t max_sglen;
	ushort_t max_dp;
	ushort_t max_iod;
	ushort_t max_comb;
	uchar_t	latency;
	uchar_t	resvd5;
	uchar_t	scsi_timeout;
	uchar_t	resvd6;
	ushort_t min_freelines;
	uchar_t	resvd7[8];
	uchar_t	rate_const;
	uchar_t	resvd8[11];
	ushort_t phys_blksize;
	ushort_t log_blksize;
	ushort_t max_blk;
	ushort_t blk_factor;
	ushort_t cache_line_size;
	uchar_t	scsi_cap;
	uchar_t	resvd9[5];
	ushort_t fw_build_num;
	uchar_t	fault_mgmt_type;
	uchar_t	resvd10[13];
};

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MLX_MLX_RAID_H */
