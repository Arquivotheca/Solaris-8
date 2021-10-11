
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Pluto state information
 */

#ifndef	_P_STATE
#define	_P_STATE

#pragma ident	"@(#)state.h	1.4	96/01/08 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif


#define	NBR_DRIVES_PER_TRAY	10	/* Number of drives per tray */
#define	NBR_TRAYS		3	/* Number of trays */
#define	NBR_200_TRAYS		6	/* Number of trays */


/*
 *  Individual drive state definitions (state_flags)
 */
#define	DS_OPNF		0x00000001  /* Open failed */
#define	DS_INQF		0x00000002  /* SCSI Inquiry cmd failed */
#define	DS_FWE		0x00010000  /* Fast write enable  */
#define	DS_PCFW		0x00020000  /* Per Command Fast Write  */
#define	DS_NDF		0x01000000  /* no drive found  */
#define	DS_NDS		0x02000000  /* no drive select  */
#define	DS_DNR		0x04000000  /* drive not ready  */
#define	DS_CNR		0x08000000  /* could not read  */
#define	DS_SPUN_DWN	0x10000000  /* drive spun down */

/*
 *	  global pointer to the error message
 *
 *	  This pointer will be NULL if no error message is available.
 *
 *	  If it is not NULL it will point to a
 *	  error message (NULL terminated).
 *	  This error message will contain the reason the function returned
 *	  a non-zero status.
 *
 */

extern	char *p_error_msg_ptr;


/*
 *	List sent as a parameter in "p_stripe" function
 *
 *	The list is terminated by port = 0xff & tgt = 0xff
 */
typedef struct  drv_list_entry_struct {
	u_char	  port;		/* 0 - max number of ports */
	u_char	  tgt;		/* 0 - max number of targets */
} Drv_list_entry;

/* id strings as reported by the drive, NULL terminated */
typedef	struct id_strings_struct {
	char	vendor_id[9];   /* vendor id */
	char	prod_id[17];	/* Product id */
	char	revision[5];	/* Product revision level */
	char	firmware_rev[5]; /* firmware revision level */
	char	ser_num[13];	/* drive serial number */
} Id_strings;

/*
 *	controller state
 */
typedef struct controller_state_struct {
	Id_strings	c_id;	/* controller id strings from INQUIRY cmd */
	u_char		num_ports;
	u_char		num_tgts; /* Actual numbers as reported by the Pluto */
	u_char
			: 2,	/* reserved */
			: 1,	/* reserved */
			: 1,	/* reserved */
			: 2,	/* reserved */
		aps	: 1,	/* accumulate performance statistics */
		aes	: 1;	/* Accumulate error statistics */
	u_char			/* Heat, ventilation & air conditioning */
		rsvd7   : 6,	/* */
		hvac_fc : 1,	/* Fan Control (1= fans stopped) */
		hvac_lobt : 1;	/* Low Battery (1= NVRAM battery low) */
} Controller_state;


/*
 *	Individual drive structures
 */


/* structure for individual drive state */
typedef struct drv_par_tbl_struct {
	u_int	 state_flags;	/* drive state (bits defined above ) */
	Id_strings id;		/* drive ID as reported by the drive */
	u_int	  num_blocks;	/* drive capacity in # of 512 byte blocks */
	/* ID message string for CLI display */
	char	 id1[22];
	int	  no_label_flag;	/* invalid UNIX label on disk flag */
	int	  reserved_flag;	/* disk is reserved */
} Drv_par_tbl;


/*
 *		State of the Pluto
 */
typedef struct p_state_struct {
	Controller_state  c_tbl;	/* state of controller */
	Drv_par_tbl	drv[P_NPORTS][P_NTARGETS]; /* state for each drive */
} P_state;

/*
 * Performance Statistics
 */
/*
 *   This structure contains the detailed IOPS information
 *   from the Sparc Disk Array.
 *   These parameters contain the number of commands/per second
 *   that were executed by the Sparc Disk Array.
 *   This table is filled in by the p_get_perf_statistics function.
 */
typedef struct performance_details_struct {
	int	num_lt_2k_reads;	/* number of < 2k reads/second */
	int	num_lt_2k_writes;	/* number of < 2k writes/second */
	int	num_gt_2k_lt_8k_reads;  /* number of reads/sec >2k & <8k */
	int	num_gt_2k_lt_8k_writes; /* number of writes/sec >2k & <8k */
	int	num_8k_reads;		/* number of 8k reads/second */
	int	num_8k_writes;		/* number of 8k writes/second */
	int	num_gt_8k_reads;	/* number of >8k reads/second */
	int	num_gt_8k_writes;	/* number of >8k writes/second */
} Perf_details;

typedef struct p_perf_statistics_struct {
	int	ctlr_percent_busy;
	int	ctlr_iops;	  /* Input/Output operations per second */
	int	drive_iops[P_NPORTS][P_NTARGETS];  /* I/O per second */
	/* detailed I/O per second */
	Perf_details perf_details[P_NPORTS][P_NTARGETS];
} P_perf_statistics;


#ifdef	__cplusplus
}
#endif

#endif	/* _P_STATE */
