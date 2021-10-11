/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Local SCSI definitions
 */

#ifndef	_P_SCSI
#define	_P_SCSI

#pragma ident	"@(#)scsi.h	1.6	99/07/29 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct  ms_pg21_struct	{
	uchar_t	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	uchar_t	rsvd;			/* 01 reserved */
	ushort_t pg_len;		/* 02-03 page length */
	uchar_t
		rsvd1	: 2,	/* reserved */
		tb	: 1,	/* Transfer block */
		arbb	: 1,	/* Automatic reallocation of bad block */
		rsvd2   : 2,	/* reserved */
		aps	: 1,	/* accumulate performance statistics */
		aes	: 1;	/* Accumulate error statistics */
	uchar_t	rsvd3;		/* 05 reserved */
	uchar_t	rsvd4;		/* 06 reserved */
	uchar_t	rsvd5;		/* 07 reserved */
	uchar_t			/* Heat, ventilation & air conditioning */
		rsvd6	: 6,	/* */
		hvac_fc	: 1,	/* Fan Control */
		hvac_lobt : 1;	/* Low Battery */
	uchar_t	rsvd7;		/* 09 reserved */
	uchar_t	rsvd8;		/* 10 reserved */
	uchar_t	rsvd9;		/* 11 reserved */
} Ms_pg21;


/* single drive record for page 22 */
typedef struct sd_rec_struct {
	uint_t		state_flags;	/* drive state flags :da2: */
	uchar_t		grp_addr_port;	/* Group address - Port */
	uchar_t		grp_addr_tgt;	/* Group address - tgt */
	ushort_t		rsrv;			/* */
} Sd_rec;

typedef struct ms_pg22_struct {
	uchar_t	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	uchar_t	rsvd;			/* 01 reserved */
	ushort_t	pg_len;			/* 02-03 page length */
	uchar_t	num_ports;		/* 04 number of ports */
	uchar_t	num_tgts;		/* 05 number of targets per port */
	ushort_t	rsrv1;			/* 06-07 */
	/* 08-n each drive information */
	Sd_rec	drv[P_NPORTS][P_NTARGETS];
} Ms_pg22;


/* fast write page - Mode Select only */
typedef	struct	ms_pg23_struct {
	uchar_t	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	uchar_t	rsvd;			/* 01 reserved */
	ushort_t	pg_len;			/* 02-03 page length */
	uchar_t	group : 1,	/* 04 group flag */
		all 	: 1,	/* 04 all flag */
		rsvr 	: 6;
	uchar_t	rsvr1;		/* 05 */
	uchar_t	port;		/* 06 address of drive */
	uchar_t	tgt;		/* 07 address of drive */
	uchar_t	purge : 1,	/* 08 purge  the write buffer flag */
		rsvr2	: 4,
		enable	: 1,	/* enable required for fastwrites */
		pcfw	: 1,	/* Follow the command bit for fast writes */
		fwe	: 1;	/* Fast write flag */
	uchar_t	rsvr3;		/* 09 */
	uchar_t	rsvr4;		/* 10 */
	uchar_t	rsvr5;		/* 11 */
} Ms_pg23;

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
	uchar_t	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	uchar_t	rsvd;			/* 01 reserved */
	ushort_t	pg_len;			/* page length */
	ushort_t period;
	ushort_t idle;
	uchar_t  ports;
	uchar_t  tgts;
	uchar_t  rsvr2[22];		/* bytes 10 - 31 are reserved */
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
	uchar_t	ps		:1,	/* Page savable bit */
				:1,	/* Reserved */
		pg_code		:6;	/* page code */
	uchar_t	rsvd;			/* 01 reserved */
	ushort_t	pg_len;			/* page length */
	uchar_t  ports;
	uchar_t  tgts;
	uchar_t  rsvr2[26];		/* bytes 6 - 31 are reserved */
	Wb_log_entry	wb_log;
} Ls_pg3c;

typedef struct p_inquiry_struct {
	/*
	* byte 0
	*
	* Bits 7-5 are the Peripheral Device Qualifier
	* Bits 4-0 are the Peripheral Device Type
	*
	*/
	uchar_t	inq_dtype;
	/* byte 1 */
	uchar_t	inq_rmb		: 1,	/* removable media */
		inq_qual	: 7;	/* device type qualifier */

	/* byte 2 */
	uchar_t	inq_iso		: 2,	/* ISO version */
		inq_ecma	: 3,	/* ECMA version */
		inq_ansi	: 3;	/* ANSI version */

	/* byte 3 */
	uchar_t	inq_aenc	: 1,	/* async event notification cap. */
		inq_trmiop	: 1,	/* supports TERMINATE I/O PROC msg */
				: 2,	/* reserved */
		inq_rdf		: 4;	/* response data format */

	/* bytes 4-7 */

	uchar_t	inq_len;		/* additional length */
	uchar_t			: 8;	/* reserved */
	uchar_t			: 8;	/* reserved */
	uchar_t	inq_reladdr	: 1,	/* supports relative addressing */
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
	uint_t	last_block_addr;
	uint_t	block_size;
} Read_capacity_data;


#ifdef	__cplusplus
}
#endif

#endif	/* _P_SCSI */
