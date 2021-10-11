/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Copyright (c) 1995 by Cray Research, Inc.
 */

#ifndef	_SYS_SCSI_TARGETS_SSDDEF_H
#define	_SYS_SCSI_TARGETS_SSDDEF_H

#pragma ident	"@(#)ssddef.h	1.37	99/10/22 SMI"

#include <sys/note.h>
#include <sys/mhd.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for SCSI direct access devices
 */

#if	defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Manifest defines
 */

#define	USCSI_DEFAULT_MAXPHYS	0x80000

/*
 * Local definitions, for clarity of code
 */
#define	SSD_SCSI_DEVP	(un->un_sd)
#define	SSD_DEVINFO	(SSD_SCSI_DEVP->sd_dev)
#define	SSD_INQUIRY	(SSD_SCSI_DEVP->sd_inq)
#define	SSD_RQSENSE	(SSD_SCSI_DEVP->sd_sense)
#define	SSD_MUTEX	(&SSD_SCSI_DEVP->sd_mutex)
#define	ROUTE		(&SSD_SCSI_DEVP->sd_address)
#define	SCBP(pkt)	((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)	((*(pkt)->pkt_scbp) & STATUS_MASK)
#define	CDBP(pkt)	((union scsi_cdb *)(pkt)->pkt_cdbp)
#define	NO_PKT_ALLOCATED ((struct buf *)0)
#define	ALLOCATING_PKT	((struct buf *)-1)
#define	BP_PKT(bp)	((struct scsi_pkt *)bp->av_back)
#define	BP_HAS_NO_PKT(bp) (bp->av_back == NO_PKT_ALLOCATED)

#define	STATUS_SCBP_C(statusp)	(*(uchar_t *)(statusp) & STATUS_MASK)

#define	Tgt(devp)	(devp->sd_address.a_target)
#define	Lun(devp)	(devp->sd_address.a_lun)

#define	New_state(un, s)	\
	(un)->un_last_state = (un)->un_state,  (un)->un_state = (s)
#define	Restore_state(un)	\
	{ uchar_t tmp = (un)->un_last_state; New_state((un), tmp); }



/*
 * Structure for recording whether a device is fully open or closed.
 * Assumptions:
 *
 *	+ There are only 8 partitions possible.
 *	+ BLK, MNT, CHR, SWP don't change in some future release!
 *
 */

#define	SSDUNIT_SHIFT	3
#define	SSDPART_MASK	7
#define	SSDUNIT(dev)	(getminor((dev))>>SSDUNIT_SHIFT)
#define	SSDPART(dev)	(getminor((dev))&SSDPART_MASK)

struct ocinfo {
	/*
	* Types BLK, MNT, CHR, SWP,
	* assumed to be types 0-3.
	*/
	uint_t	lyr_open[NDKMAP];
	uchar_t  reg_open[OTYPCNT - 1];
};
#define	OCSIZE  sizeof (struct ocinfo)
union ocmap {
	uchar_t chkd[OCSIZE];
	struct ocinfo rinfo;
};
#define	lyropen rinfo.lyr_open
#define	regopen rinfo.reg_open

/*
 * Private info for scsi disks.
 *
 * Pointed to by the un_private pointer
 * of one of the SCSI_DEVICE structures.
 */

struct ssa_disk {
	struct scsi_device *un_sd;	/* back pointer to SCSI_DEVICE */
	struct scsi_pkt *un_rqs;	/* ptr to request sense command pkt */
	struct buf *un_rqs_bp;		/* ptr to request sense bp */
	ksema_t	un_rqs_sema;		/* sema to protect req sense pkt */
	struct	buf *un_sbufp;		/* for use in special io */
	char		*un_srqbufp;	/* sense buffer for special io */
	kcondvar_t	un_sbuf_cv;	/* Conditional Variable on sbufp */
	union	ocmap un_ocmap;		/* open partition map, block && char */
	struct	dk_map un_map[NDKMAP];	/* logical partitions */
	uint_t	un_offset[NDKMAP];	/* starting block for partitions */
	uchar_t	un_last_pkt_reason;	/* used for suppressing multiple msgs */
	uchar_t	un_exclopen;		/* exclusive open bits */
	struct	dk_geom un_g;		/* disk geometry */
	struct	dk_vtoc un_vtoc;	/* disk Vtoc */
	struct	diskhd un_utab;		/* for queuing */
	struct	kstat *un_stats;	/* for statistics */
	struct	kstat *un_pstats[NDKMAP];	/* for partition statistics */
	ksema_t	un_semoclose;		/* lock for serializing opens/closes */
	uint_t	un_err_blkno;		/* disk block where error occurred */
	int	un_capacity;		/* capacity of drive */
	uchar_t	un_gvalid;		/* geometry is valid */
	uchar_t	un_state;		/* current state */
	uchar_t	un_last_state;		/* last state */
	uchar_t	un_format_in_progress;	/* disk is formatting currently */
	uchar_t	un_start_stop_issued;	/* START_STOP cmd issued to disk */
	uchar_t	un_asciilabel[LEN_DKL_ASCII];	/* Copy of asciilabel */
	short	un_throttle;		/* max outstanding cmds */
	short	un_save_throttle;	/* max outstanding cmds saved */
	short	un_ncmds;		/* number of cmds in transport */
	int	un_tagflags;		/* Pkt Flags for Tagged Queueing  */
	short	un_sbuf_busy;		/* Busy wait flag for the sbuf */
	short	un_resvd_status;	/* Reservation Status */
	opaque_t	un_mhd_token;	/* scsi watch request */
	timeout_id_t un_resvd_timeid;	/* resvd recover */
	timeout_id_t un_reset_throttle_timeid; /* reset throttle */
	timeout_id_t un_restart_timeid;	/* restart unit */
	timeout_id_t un_reissued_timeid; /* sdrestarts */
	uchar_t	un_ssa_fast_writes;	/* SSA(Pluto) supports fast writes */
	uchar_t	un_reservation_type;	/* scsi-3 or scsi-2 */
	ddi_devid_t	un_devid;	/* device id */
	uint_t	un_max_xfer_size;	/* max transfer size */
	kstat_t *un_errstats;		/* for error statistics */
	ddi_eventcookie_t	un_insert_event; /* insert event */
	ddi_eventcookie_t	un_remove_event; /* remove event */
	kcondvar_t	un_suspend_cv;  /* Cond Var on cpr SUSPEND */
	kcondvar_t	un_disk_busy_cv; /* Cond var to wait for IO */
	uint_t	un_notready_retry_count; /* per disk notready retry count */
	uint_t   un_busy_retry_count;	/* per disk BUSY retry count */

	uint_t	un_start_stop_cycle_page; /* Saves the start/stop cycle page */
	short	un_power_level;		/* current power level- Spindle motor */
	timeout_id_t un_pm_timeid;	/* timeout id for power management */
	uchar_t  un_flag_busy;		/* device is about to become busy */
	ulong_t	un_detach_count;	/* !0 if executing detach routine */
	ulong_t	un_layer_count;		/* Current total # of layered opens */
	ulong_t un_opens_in_progress;	/* Current # of threads in ssdopen */
					/*  for this insance */
};

/*
 * SCSI-3 Reservation Stuff
 */


typedef struct ssd_prin_readkeys {
	uint32_t	generation;
	uint32_t	len;
	mhioc_resv_key_t *keylist;
} ssd_prin_readkeys_t;

typedef struct ssd_readresv_desc {
	mhioc_resv_key_t	resvkey;
	uint32_t		scope_specific_addr;
	uint8_t			reserved_1;
	uint8_t			scope:4,
				type:4;
	uint8_t			reserved_2;
	uint8_t			reserved_3;
} ssd_readresv_desc_t;
typedef struct ssd_prin_readresv {
	uint32_t		generation;
	uint32_t		len;
	ssd_readresv_desc_t	*readresv_desc;
} ssd_prin_readresv_t;

typedef struct ssd_prout {
	uchar_t		res_key[MHIOC_RESV_KEY_SIZE];
	uchar_t		service_key[MHIOC_RESV_KEY_SIZE];
	uint32_t 	scope_address;
	uchar_t		reserved:7,
			aptpl:1;
	uchar_t		reserved_1;
	uint16_t	ext_len;
} ssd_prout_t;


#define	SSD_SCMD_PRIN	0x5e
#define	SSD_SCMD_PROUT	0x5f

#define	SSD_READ_KEYS		0x00
#define	SSD_READ_RESV		0x01

#define	SSD_SCSI3_REGISTER		0x00
#define	SSD_SCSI3_RESERVE		0x01
#define	SSD_SCSI3_RELEASE		0x02
#define	SSD_SCSI3_PREEMPTANDABORT	0x05

#define	SSD_SCSI3_RESERVATION		0x0
#define	SSD_SCSI2_RESERVATION		0x1
/*
 * device error kstats
 */
struct ssa_errstats {
	struct kstat_named	ssa_softerrs;
	struct kstat_named	ssa_harderrs;
	struct kstat_named	ssa_transerrs;
	struct kstat_named	ssa_vid;
	struct kstat_named	ssa_pid;
	struct kstat_named	ssa_revision;
	struct kstat_named	ssa_serial;
	struct kstat_named	ssa_capacity;
	struct kstat_named	ssa_rq_media_err;
	struct kstat_named	ssa_rq_ntrdy_err;
	struct kstat_named	ssa_rq_nodev_err;
	struct kstat_named	ssa_rq_recov_err;
	struct kstat_named	ssa_rq_illrq_err;
	struct kstat_named	ssa_rq_pfa_err;
};

#define	SSD_MAX_XFER_SIZE	(1024 * 1024)

_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex, ssa_disk))
_NOTE(READ_ONLY_DATA(ssa_disk::un_sd))
_NOTE(DATA_READABLE_WITHOUT_LOCK(ssa_disk::un_reservation_type))
_NOTE(SCHEME_PROTECTS_DATA("save sharing",
	ssa_disk::un_mhd_token
	ssa_disk::un_state
	ssa_disk::un_tagflags
	ssa_disk::un_format_in_progress
	ssa_disk::un_gvalid
	ssa_disk::un_resvd_timeid))

_NOTE(SCHEME_PROTECTS_DATA("stable data",
	ssa_disk::un_max_xfer_size
	ssa_disk::un_offset))

_NOTE(SCHEME_PROTECTS_DATA("semaphore",
	ssa_disk::un_rqs
	ssa_disk::un_rqs_bp))

_NOTE(SCHEME_PROTECTS_DATA("cv",
	ssa_disk::un_sbufp
	ssa_disk::un_srqbufp
	ssa_disk::un_sbuf_busy))

_NOTE(SCHEME_PROTECTS_DATA("Unshared data",
	uio
	buf
	scsi_pkt
	uscsi_cmd
	scsi_capacity
	scsi_cdb scsi_arq_status
	dk_label
	mhioc_inkeys
	mhioc_inresvs
	ssd_prout
	dk_map))

/*
 * we use pkt_private area for storing bp and retry_count
 */
struct ssd_pkt_private {
	struct buf	*ssdpp_bp;
	short		 ssdpp_retry_count;
	short		 ssdpp_victim_retry_count;
};

#define	PP_LEN	(sizeof (struct ssd_pkt_private))

#define	PKT_SET_BP(pkt, bp)	\
	((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_bp = bp
#define	PKT_GET_BP(pkt) \
	(((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_bp)


#define	PKT_SET_RETRY_CNT(pkt, n) \
	((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_retry_count = n

#define	PKT_GET_RETRY_CNT(pkt) \
	(((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_retry_count)

#define	PKT_INCR_RETRY_CNT(pkt, n) \
	((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_retry_count += n

#define	PKT_SET_VICTIM_RETRY_CNT(pkt, n) \
	((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_victim_retry_count \
			= n

#define	PKT_GET_VICTIM_RETRY_CNT(pkt) \
	(((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_victim_retry_count)

#define	PKT_INCR_VICTIM_RETRY_CNT(pkt, n) \
	((struct ssd_pkt_private *)pkt->pkt_private)->ssdpp_victim_retry_count \
				+= n

#define	SSD_VICTIM_RETRY_COUNT		(ssd_victim_retry_count)

_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", ssd_pkt_private buf))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_device dk_cinfo))
_NOTE(SCHEME_PROTECTS_DATA("stable data", ssa_disk::un_ssa_fast_writes))
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", scsi_status scsi_cdb))

#if DEBUG || lint
#define	SDDEBUG
#endif

/*
 * Debugging macros
 */
#ifdef	SDDEBUG
#define	DEBUGGING	(ssddebug > 1)
#define	SSD_DEBUG	if (ssddebug == 1) scsi_log
#define	SSD_DEBUG2	if (ssddebug == 2) scsi_log
#define	SSD_DEBUG3	if (ssddebug == 3) scsi_log
#else	/* SDDEBUG */
#define	ssddebug	(0)
#define	DEBUGGING	(0)
#define	SSD_DEBUG	if (0) scsi_log
#define	SSD_DEBUG2	if (0) scsi_log
#define	SSD_DEBUG3	if (0) scsi_log
#endif

#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#define	MAX_THROTTLE	256

/*
 * Disk driver states
 */

#define	SSD_STATE_NORMAL	0
#define	SSD_STATE_OFFLINE	1
#define	SSD_STATE_RWAIT		2
#define	SSD_STATE_DUMPING	3
#define	SSD_STATE_EJECTED	4
#define	SSD_STATE_SUSPENDED	5
#define	SSD_STATE_PM_SUSPENDED	6

/*
 * The table is to be interpreted as follows: The rows lists all the states
 * and each column is a state that a state in each row *can* reach. The entries
 * in the table list the event that cause that transition to take place.
 * For e.g.: To go from state RWAIT to SUSPENDED, event (d)-- which is the
 * invocation of DDI_SUSPEND-- has to take place. Note the same event could
 * cause the transition from one state to two different states. e.g., from
 * state SUSPENDED, when we get a DDI_RESUME, we just go back to the *last
 * state* whatever that might be. (NORMAL or OFFLINE).
 *
 *
 * State Transition Table:
 *
 *		      NORMAL  OFFLINE  RWAIT  DUMPING  SUSPENDED PM_SUSPENDED
 *
 *   NORMAL		 -	(a)	 (b)     (c)	   (d)	   (h)
 *
 *   OFFLINE		(e)	 -       (e)     (c)       (d)	   NP
 *
 *   RWAIT		(f)	NP        -      (c)       (d)	   (h)
 *
 *   DUMPING		NP      NP        NP      -         NP	   NP
 *
 *   SUSPENDED          (g)     (g)       (b)     NP*       -	   NP
 *
 *   PM_SUSPENDED	(i) 	NP	  (b)	  (c)	   (d)	   -
 *
 *
 *   NP :	Not Possible.
 *   (a): 	Disk does not respond.
 *   (b): 	Packet Allocation Fails
 *   (c):	Panic - Crash dump
 *   (d):	DDI_SUSPEND is called.
 *   (e):	Disk has a successful I/O completed.
 *   (f):	ssdrunout() calls ssdstart() which sets it NORMAL
 *   (g):	DDI_RESUME is called.
 *   (h): 	Device threshold exceeded pm Framework called power
 *		entry point or pm_lower_power called in detach.
 *   (i):	When a new I/O came in.
 *    * :	When suspended, we dont change state during panic dump
 */

/*
 * Error levels
 */

#define	SSDERR_ALL		0
#define	SSDERR_UNKNOWN		1
#define	SSDERR_INFORMATIONAL	2
#define	SSDERR_RECOVERED	3
#define	SSDERR_RETRYABLE	4
#define	SSDERR_FATAL		5

/*
 * Parameters
 */

/*
 * 60 seconds is a *very* reasonable amount of time for most slow CD
 * operations.
 */

#define	SSD_IO_TIME	60

/*
 * 2 hours is an excessively reasonable amount of time for format operations.
 */

#define	SSD_FMT_TIME	120*60

/*
 * 5 seconds is what we'll wait if we get a Busy Status back
 */

#define	SSD_BSY_TIMEOUT		(drv_usectohz((clock_t)5 * 1000000))

/*
 * 60 seconds is what we will wait for to reset the
 * throttle back to it MAX_THROTTLE.
 */
#define	SSD_RESET_THROTTLE_TIMEOUT	60

/*
 * Number of times we'll retry a normal operation.
 *
 * This includes retries due to transport failure
 * (need to distinguish between Target and Transport failure)
 */

#define	SSD_RETRY_COUNT		3


/*
 * Maximum number of units we can support
 * (controlled by room in minor device byte)
 * XXX: this is out of date!
 */
#define	SSD_MAXUNIT		32

/*
 * 10 seconds is what we will wait for the IO to finish
 * before we issue a bus device reset in DDI_SUSPEND
 */

#define	SSD_WAIT_CMDS_COMPLETE	30

/*
 * Reservation Status's
 */
#define	SSD_RELEASE			0x0000
#define	SSD_RESERVE			0x0001
#define	SSD_TKOWN			0x0002
#define	SSD_LOST_RESERVE		0x0004
#define	SSD_FAILFAST			0x0080
#define	SSD_WANT_RESERVE		0x0100
#define	SSD_RESERVATION_CONFLICT	0x0200
#define	SSD_PRIORITY_RESERVE		0x0400

#define	SSD_MAX_NODES	16
/*
 * delay before reclaiming reservation is 6 seconds, in units of micro seconds
 */
#define	SSD_REINSTATE_RESV_DELAY	6000000

/*
 * sdintr action codes
 */

#define	COMMAND_DONE		0
#define	COMMAND_DONE_ERROR	1
#define	QUE_COMMAND		2
#define	QUE_SENSE		3
#define	JUST_RETURN		4

/*
 * Commands for sd_start_stop
 */
#define	SSD_STOP	(0)
#define	SSD_START	(1)

#define	VIDMAX 8
#define	PIDMAX 16

/*
 * Options
 */
#define	SSD_NODISC	0x0001	/* has problem w/ disconnect-reconnect */
#define	SSD_NOPARITY	0x0002	/* target does not generate parity */
#define	SSD_MULTICMD	0x0004	/* target supports SCSI-2 multiple commands */
#define	SSD_EIOENABLE	0x0008	/* Enable retruning EIO on media change	*/
#define	SSD_QUEUEING	0x0010	/* Enable Command Queuing to Host Adapter */

/*
 * Some internal error codes for driver functions.
 */
#define	SSD_EACCES	1

/*
 * this should be moved to common/sys/scsi/generic/commands.h (Group 1 cmd)
 */
#define	SCMD_SYNCHRONIZE_CACHE	0x35
#define	SSA_PRIORITY_RESERVE	0x80

/*
 * ssd-config-list version number.
 */
#define	SSD_CONF_VERSION	1

/*
 * Bit in flags telling driver to set throttle from ssd.conf ssd-config-list
 * and driver table.  This word is the first word in the datalist, after flags.
 */
#define	SSD_CONF_SET_THROTTLE		0
#define	SSD_CONF_BSET_THROTTLE		(1 << SSD_CONF_SET_THROTTLE)


/*
 * Bit in flags telling driver to set Not Ready Retries from ssd.conf
 * ssd-config-list and driver table.  This word is the second word in
 * the datalist, after flags and throttle.
 */
#define	SSD_CONF_SET_NOTREADY_RETRIES	1
#define	SSD_CONF_BSET_NRR_COUNT		(1 << SSD_CONF_SET_NOTREADY_RETRIES)

/*
 * Bit in flags telling driver to set SCSI status BUSY Retries from ssd.conf
 * ssd-config-list and driver table.  This word is the third word in
 * the datalist, after flags, throttle and not ready retry count.
 */
#define	SSD_CONF_SET_BUSY_RETRIES	2
#define	SSD_CONF_BSET_BSY_RETRY_COUNT	(1 << SSD_CONF_SET_BUSY_RETRIES)

/*
 * This is the number of items currently settable in the ssd.conf
 * ssd-config-list.  Update it when add new properties above.
 */
#define	SSD_CONF_MAX_ITEMS	3

/* Power management defines */

#define	SSD_SPINDLE_OFF		0x0
#define	SSD_SPINDLE_ON		0x1

/*
 * Could move this define to some thing like log sense.h in SCSA headers
 * But for now let it live here.
 */
#define	START_STOP_CYCLE_COUNTER_PAGE_SIZE   0x28
#define	START_STOP_CYCLE_PAGE  0x0E
#define	START_STOP_CYCLE_VU_PAGE  0x31

/*
 * This define is used to take care of the race condition in power management.
 * This flag when set in un_flag_busy it indicates that device is about to
 * become busy.
 */
#define	SSD_BECOMING_ACTIVE	0x01
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_TARGETS_SSDDEF_H */
