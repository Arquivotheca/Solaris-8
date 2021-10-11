
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Local SCSI definitions
 */

#ifndef	_P_SCSI
#define	_P_SCSI

#pragma ident	"@(#)scsi.h	1.4	98/02/01 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Definitions for Mode Sense/Select pages
 */
#define	GS_GRPD	0x80  /* group is grouped */
#define	GS_BAD	0x40  /* group is bad */

#define	PAGE_CONTROL_SAVED	3	/* Page control field in Mode Sense */
#define	SAVE_PAGE_FLAG		1	/* Save page flag for Mode Select */
/*
 * Request Sense extensions: offsets into es_add_len[]
 * in struct scsi_extended_sense.
 */
#define	ADD_SENSE_CODE		4
#define	ADD_SENSE_QUAL_CODE	5

#define	MIN_REQUEST_SENSE_LEN	18
#define	MAX_MODE_SENSE_LEN	0xffff

/*		NOTE: These command op codes are not defined in commands.h */
#define		SCMD_SYNC_CACHE			0x35
#define		SCMD_LOG_SENSE			0x4d

/* Member drive in page 20 */
typedef struct sd_info_struct {
	u_char		port;		/* drive port */
	u_char		tgt;		/* drive target */
	u_short		reserved;	/* */
	char		ser_num[12];	/* Drive serial number */
} Sd_info;

/* grouped drive record for page 20 */
typedef struct gd_rec_struct {
	u_char	grp_state_flags;	/* state of the group */
	u_char	rsvd1;			/* */
	u_short	num_drvs;		/* grouped number of drives */
	u_int	stripe_blk;		/* grouped drive stripe block size */
	char	host_name[8];		/* grouped by host name */
	u_int	dt_stamp;		/* grouped on date and time stamp */
	char	cntr_id[8];		/* grouped at controller id */
	char	vu_bytes[8];		/* grouped drive vendor unique bytes */
	/* Member drive information */
	Sd_info	members[P_NPORTS*P_NTARGETS];
} Gd_rec;

typedef struct ms_pg20_struct {
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short	pg_len;			/* 02-03 page length */
	u_int	num_grps;		/* 04-07 number of groups */
	Gd_rec	grp[P_NPORTS*P_NTARGETS];	/* 08- each group information */
}Ms_pg20;

typedef struct  ms_pg21_struct	{
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short pg_len;		/* 02-03 page length */
	u_char
		rsvd1	: 2,	/* reserved */
		tb	: 1,	/* Transfer block */
		arbb	: 1,	/* Automatic reallocation of bad block */
		rsvd2   : 2,	/* reserved */
		aps	: 1,	/* accumulate performance statistics */
		aes	: 1;	/* Accumulate error statistics */
	u_char	rsvd3;		/* 05 reserved */
	u_char	rsvd4;		/* 06 reserved */
	u_char	rsvd5;		/* 07 reserved */
	u_char			/* Heat, ventilation & air conditioning */
		rsvd6	: 6,	/* */
		hvac_fc	: 1,	/* Fan Control */
		hvac_lobt : 1;	/* Low Battery */
	u_char	rsvd7;		/* 09 reserved */
	u_char	rsvd8;		/* 10 reserved */
	u_char	rsvd9;		/* 11 reserved */
} Ms_pg21;

/* single drive record for page 22 */
typedef struct sd_rec_struct {
	u_int		state_flags;	/* drive state flags :da2: */
	u_char		grp_addr_port;	/* Group address - Port */
	u_char		grp_addr_tgt;	/* Group address - tgt */
	u_short		rsrv;			/* */
} Sd_rec;

typedef struct ms_pg22_struct {
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short	pg_len;			/* 02-03 page length */
	u_char	num_ports;		/* 04 number of ports */
	u_char	num_tgts;		/* 05 number of targets per port */
	u_short	rsrv1;			/* 06-07 */
	/* 08-n each drive information */
	Sd_rec	drv[P_NPORTS][P_NTARGETS];
} Ms_pg22;

/* fast write page - Mode Select only */
typedef	struct	ms_pg23_struct {
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short	pg_len;			/* 02-03 page length */
	u_char	group : 1,	/* 04 group flag */
		all 	: 1,	/* 04 all flag */
		rsvr 	: 6;
	u_char	rsvr1;		/* 05 */
	u_char	port;		/* 06 address of drive */
	u_char	tgt;		/* 07 address of drive */
	u_char	purge : 1,	/* 08 purge  the write buffer flag */
		rsvr2	: 4,
		enable	: 1,	/* enable required for fastwrites */
		pcfw	: 1,	/* Follow the command bit for fast writes */
		fwe	: 1;	/* Fast write flag */
	u_char	rsvr3;		/* 09 */
	u_char	rsvr4;		/* 10 */
	u_char	rsvr5;		/* 11 */
} Ms_pg23;

/*
 *  structure for MODE SELECT/SENSE 10 byte page header
 *
 */
typedef struct mode_header_10_struct {
	u_short length;
	u_char medium_type; /* device specific */
	u_char device_specific; /* device specfic parameters */
	u_short	rsvdl;		/* reserved */
	u_short bdesc_length;	/* length of block descriptor(s), if any */
} Mode_header_10;

/*
 *		LOG SENSE PAGE definitions
 */

/*
 * IOPS Performance log page
 */
typedef	struct	perf_drv_parms_struct {
	int	num_lt_2k_reads;	/* number of < 2k reads/second */
	int	num_lt_2k_writes;	/* number of < 2k writes/second */
	int	num_gt_2k_lt_8k_reads;  /* number of reads/sec >2k & <8k */
	int	num_gt_2k_lt_8k_writes; /* number of writes/sec >2k & <8k */
	int	num_8k_reads;		/* number of 8k reads/second */
	int	num_8k_writes;		/* number of 8k writes/second */
	int	num_gt_8k_reads;	/* number of >8k reads/second */
	int	num_gt_8k_writes;	/* number of >8k writes/second */
	int	rsvr[8];
} Perf_drv_parms;

typedef	struct	ls_pg3d_struct {
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short	pg_len;			/* page length */
	u_short period;
	u_short idle;
	u_char  ports;
	u_char  tgts;
	u_char  rsvr2[22];		/* bytes 10 - 31 are reserved */
	/* parameters for each drv */
	Perf_drv_parms	drv_parms[P_NPORTS][P_NTARGETS];
} Ls_pg3d;

/*
 * Fast Write buffer usage information
 */

struct	wb_log_drive_entry {
	char	serial_no[12];
	int	wb_mode;

	int	bs_unwritten;
	int	bs_total;
	int	bytes_total;
	int	bytes_unwritten;

	int	bs_inprogress;
	int	bytes_inprogress;
	int	bs_errored;
	int	io_cnt;

	int	io_nblocks;
	int	io_bs_cnt;
	int	read_cnt;
	int	read_full_overlap;

	int	read_partial_overlap;
	int	write_cnt;
	int	write_cancellations;
	int	mode_bycmd_fw;

	int	mode_bycmd_non_fw;
	int	mode_all_fw;
	int	mode_all_non_fw;
	int	mode_none_fw;

	int	mode_none_non_fw;
	int	error_cnt;
	int	non_read_write;
	int	read_overlap_gone;

	int	flush_writes;
	int	drive_idle_writes;
	int	pend_len;

	int	reserved[5];
};

typedef struct	wb_log_entry {
	int				bs_unwritten;
	int				bs_total;
	int				bs_inprogress;
	int				bytes_inprogress;
	int				bytes_total;
	int				bytes_unwritten;
	int				bs_thresh;
	int				battery_ok;
	int				free_bs_cnt;
	int				free_nvram_cnt;
	int				nv_avail;
	int				hours;
	int				minutes;
	int				seconds;
	int				ms;
	struct wb_log_drive_entry	drv[P_NPORTS][P_NTARGETS];
} Wb_log_entry;


typedef	struct	ls_pg3c_struct {
	u_char	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	u_char	rsvd;			/* 01 reserved */
	u_short	pg_len;			/* page length */
	u_char  ports;
	u_char  tgts;
	u_char  rsvr2[26];		/* bytes 6 - 31 are reserved */
	Wb_log_entry	wb_log;
} Ls_pg3c;


/*
 *		SCSI CDB structures
 */
typedef	struct	my_cdb_g0 {
	unsigned	char	cmd;
	unsigned	char	lba_msb;
	unsigned	char	lba;
	unsigned	char	lba_lsb;
	unsigned	char	count;
	unsigned	char	control;
	}my_cdb_g0;

typedef	struct	{
	unsigned	char	cmd;
	unsigned	char	byte1;
	unsigned	char	byte2;
	unsigned	char	byte3;
	unsigned	char	byte4;
	unsigned	char	byte5;
	unsigned	char	byte6;
	unsigned	char	byte7;
	unsigned	char	byte8;
	unsigned	char	byte9;
	}my_cdb_g1;


/*
 * NOTE: I use my own INQUIRY structure but it is based
 *	on the /scsi/generic/inquiry.h.
 *
 *	I use my own because I need the serial number and
 *	firmware revision level defined.
 */

/*
 * Format of data returned as a result of an INQUIRY command.
 * The actual values vary contingent on the contents of the
 * inq_rdf field- RDF_LEVEL0 means that only the first 4 bytes
 * are valid [but perhaps not even ISO, ECMA, or ANSI- essentially
 * all you can trust with level 0 is the the device type and whether
 * or not it is removable media].
 *
 * RDF_CCS means that this structure complies with CCS pseudo-spec.
 *
 * RDF_SCSI2 means that the structure complies with the SCSI-2 spec.
 *
 */

typedef struct p_inquiry_struct {
	/*
	* byte 0
	*
	* Bits 7-5 are the Peripheral Device Qualifier
	* Bits 4-0 are the Peripheral Device Type
	*
	*/
	u_char	inq_dtype;
	/* byte 1 */
	u_char	inq_rmb		: 1,	/* removable media */
		inq_qual	: 7;	/* device type qualifier */

	/* byte 2 */
	u_char	inq_iso		: 2,	/* ISO version */
		inq_ecma	: 3,	/* ECMA version */
		inq_ansi	: 3;	/* ANSI version */

	/* byte 3 */
	u_char	inq_aenc	: 1,	/* async event notification cap. */
		inq_trmiop	: 1,	/* supports TERMINATE I/O PROC msg */
				: 2,	/* reserved */
		inq_rdf		: 4;	/* response data format */

	/* bytes 4-7 */

	u_char	inq_len;		/* additional length */
	u_char			: 8;	/* reserved */
	u_char			: 8;	/* reserved */
	u_char	inq_reladdr	: 1,	/* supports relative addressing */
		inq_wbus32	: 1,	/* supports 32 bit wide data xfers */
		inq_wbus16	: 1,	/* supports 16 bit wide data xfers */
		inq_sync	: 1,	/* supports synchronous data xfers */
		inq_linked	: 1,	/* supports linked commands */
				: 1,	/* reserved */
		inq_cmdque	: 1,	/* supports command queueing */
		inq_sftre	: 1;	/* supports Soft Reset option */

	/* bytes 8-35 */

	char	inq_vid[8];		/* vendor ID */

	char	inq_pid[16];		/* product ID */

	char	inq_revision[4];	/* revision level */

	/* bytes 36 - 39 */
	char	inq_firmware_rev[4];	/* firmware revision level */

	/* bytes 40 - 52 */
	char	inq_serial[12];			/* serial number */

	/*
	* Bytes 36-55 are vendor-specific.
	* Bytes 56-95 are reserved.
	* 96 to 'n' are vendor-specific parameter bytes
	*/
} P_inquiry;

typedef struct	capacity_data_struct {
	u_int	last_block_addr;
	u_int	block_size;
} Read_capacity_data;


#ifdef	__cplusplus
}
#endif

#endif	/* _P_SCSI */
