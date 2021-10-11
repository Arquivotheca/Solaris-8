/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_TARGETS_SDDEF_H
#define	_SYS_SCSI_TARGETS_SDDEF_H

#pragma ident	"@(#)sddef.h	1.96	99/11/08 SMI"

#include <sys/note.h>
#include <sys/mhd.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for SCSI direct access devices
 */

#define	FIXEDFIRMWARE	/* fixed firmware for volume control */

#if	defined(_KERNEL) || defined(_KMEMUSER)

#define	SD_UNIT_ATTENTION_RETRY	40
#define	MAX_READ_CAP_RETRY	20

#define	USCSI_DEFAULT_MAXPHYS	0x80000

/*
 * Local definitions, for clarity of code
 */
#define	SD_SCSI_DEVP	(un->un_sd)
#define	SD_DEVINFO	(SD_SCSI_DEVP->sd_dev)
#define	SD_INQUIRY	(SD_SCSI_DEVP->sd_inq)
#define	SD_RQSENSE	(SD_SCSI_DEVP->sd_sense)
#define	SD_MUTEX	(&SD_SCSI_DEVP->sd_mutex)
#define	ROUTE		(&SD_SCSI_DEVP->sd_address)
#define	SCBP(pkt)	((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)	((*(pkt)->pkt_scbp) & STATUS_MASK)
#define	CDBP(pkt)	((union scsi_cdb *)(pkt)->pkt_cdbp)
#define	NO_PKT_ALLOCATED ((struct buf *)0)
#define	ALLOCATING_PKT	((struct buf *)-1)
/*
 * BP_PKT is used to access the av_back member of the buffer
 * when we know for sure that the av_back member has a pointer
 * to the scsi_pkt.
 */
#define	BP_PKT(bp)	((struct scsi_pkt *)bp->av_back)
#define	BP_HAS_NO_PKT(bp) (bp->av_back == NO_PKT_ALLOCATED)

#define	STATUS_SCBP_C(statusp)	(*(uchar_t *)(statusp) & STATUS_MASK)

#define	Tgt(devp)	(devp->sd_address.a_target)
#define	Lun(devp)	(devp->sd_address.a_lun)

#define	ISCD(un)	((un)->un_ctype == CTYPE_CDROM)
#define	ISREMOVABLE(un)	(ISCD(un) || un->un_sd->sd_inq->inq_rmb)
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

#define	SDUNIT_SHIFT	3
#define	SDPART_MASK	7
#define	SDUNIT(dev)	(getminor((dev))>>SDUNIT_SHIFT)
#define	SDPART(dev)	(getminor((dev))&SDPART_MASK)

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

struct scsi_disk {
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
	struct	kstat *un_pstats[NDKMAP]; /* for partition statistics */
	uchar_t	un_arq_enabled;		/* auto request sense enabled */
	uchar_t	un_last_pkt_reason;	/* used for suppressing multiple msgs */
	char	un_ctype;		/* controller type */
	char	un_options;		/* drive options */
	struct	dk_vtoc un_vtoc;	/* disk Vtoc */
	struct	diskhd un_utab;		/* for queuing */
	struct	kstat *un_stats;	/* for statistics */
	struct	dk_geom un_g;		/* disk geometry */
	uchar_t	un_exclopen;		/* exclusive open bits */
	uchar_t	un_gvalid;		/* geometry is valid */
	ksema_t	un_semoclose;		/* lock for serializing opens/closes */
	uint_t	un_err_blkno;		/* disk block where error occurred */
	int	un_capacity;		/* capacity of drive */
	int	un_lbasize;		/* logical (i.e. device) block size */
	uchar_t	un_state;		/* current state */
	uchar_t	un_last_state;		/* last state */
	uchar_t	un_format_in_progress;	/* disk is formatting currently */
	uchar_t	un_start_stop_issued;	/* START_STOP cmd issued to disk */
	clock_t un_timestamp;		/* Time of last device access */
	uchar_t	un_asciilabel[LEN_DKL_ASCII];	/* Copy of asciilabel */
	short	un_throttle;		/* max outstanding cmds */
	short	un_save_throttle;	/* max outstanding cmds saved */
	short	un_ncmds;		/* number of cmds in transport */
	int	un_tagflags;		/* Pkt Flags for Tagged Queueing  */
	short	un_sbuf_busy;		/* Busy wait flag for the sbuf */
	short	un_resvd_status;	/* Reservation Status */
	kcondvar_t	un_state_cv;	/* Cond Var on mediastate */
	enum dkio_state un_mediastate;	/* current media state */
	enum dkio_state un_specified_mediastate; /* expected state */
	opaque_t	un_mhd_token;	/* scsi watch request */
	int	un_cmd_flags;		/* cache some frequently used values */
	int	un_cmd_stat_size;	/* in make_sd_cmd */
	timeout_id_t un_resvd_timeid;	/* timeout id for resvd recover */
	timeout_id_t un_reset_throttle_timeid; /* reset throttle */
	timeout_id_t un_restart_timeid;	/* restart units */
	timeout_id_t un_reissued_timeid; /* sdrestarts */
	timeout_id_t un_dcvb_timeid;	/* dlyd cv broadcast */
	ddi_devid_t	un_devid;	/* device id */
	opaque_t	un_swr_token;	/* scsi_watch request token */
	uint_t	un_max_xfer_size;	/* max transfer size */
	kstat_t *un_errstats;		/* for error statistics */
	short	un_isatapi;		/* To recognise atapi cdrom's */
	short	un_isusb;		/* To recognise usb disks */
	kcondvar_t	un_suspend_cv;	/* Cond Var on power management */
	kcondvar_t	un_disk_busy_cv; /* Cond var to wait for IO */
	short	un_power_level;		/* Power Level */
	uint_t	un_allow_bus_device_reset;	/* boolean: allow or don't */
	uint_t	un_notready_retry_count; /* per disk notready retry count */
	uchar_t	un_reservation_type;	/* scsi-3 or scsi-2 */
	uchar_t un_serial_num_buf[16];	/* Buffer for unit serial number */
	uint_t  un_start_stop_cycle_page; /* Saves the start/stop cycle page */
	timeout_id_t	un_pm_timeid;	/* timeout id for power management */
	uchar_t	un_flag_busy;	/* The device is becoming active */
	ulong_t	un_detach_count;	/* !0 if executing detach routine */
	ulong_t	un_layer_count;		/* Current total # of layered opens */
	ulong_t un_opens_in_progress;	/* Current # of threads in sdopen */
					/*  for this insance */
};

/*
 * SCSI-3 Reservation Stuff
 */


typedef struct sd_prin_readkeys {
	uint32_t	generation;
	uint32_t	len;
	mhioc_resv_key_t *keylist;
} sd_prin_readkeys_t;

typedef struct sd_readresv_desc {
	mhioc_resv_key_t	resvkey;
	uint32_t		scope_specific_addr;
	uint8_t			reserved_1;
	uint8_t			scope:4,
				type:4;
	uint8_t			reserved_2;
	uint8_t			reserved_3;
} sd_readresv_desc_t;
typedef struct sd_prin_readresv {
	uint32_t		generation;
	uint32_t		len;
	sd_readresv_desc_t	*readresv_desc;
} sd_prin_readresv_t;

typedef struct sd_prout {
	uchar_t		res_key[MHIOC_RESV_KEY_SIZE];
	uchar_t		service_key[MHIOC_RESV_KEY_SIZE];
	uint32_t 	scope_address;
	uchar_t		reserved:7,
			aptpl:1;
	uchar_t		reserved_1;
	uint16_t	ext_len;
} sd_prout_t;


#define	SD_SCMD_PRIN	0x5e
#define	SD_SCMD_PROUT	0x5f

#define	SD_READ_KEYS		0x00
#define	SD_READ_RESV		0x01

#define	SD_SCSI3_REGISTER		0x00
#define	SD_SCSI3_RESERVE		0x01
#define	SD_SCSI3_RELEASE		0x02
#define	SD_SCSI3_PREEMPTANDABORT	0x05

#define	SD_SCSI3_RESERVATION		0x0
#define	SD_SCSI2_RESERVATION		0x1

/*
 * device error kstats
 */
struct sd_errstats {
	struct kstat_named	sd_softerrs;
	struct kstat_named	sd_harderrs;
	struct kstat_named	sd_transerrs;
	struct kstat_named	sd_vid;
	struct kstat_named	sd_pid;
	struct kstat_named	sd_revision;
	struct kstat_named	sd_serial;
	struct kstat_named	sd_capacity;
	struct kstat_named	sd_rq_media_err;
	struct kstat_named	sd_rq_ntrdy_err;
	struct kstat_named	sd_rq_nodev_err;
	struct kstat_named	sd_rq_recov_err;
	struct kstat_named	sd_rq_illrq_err;
	struct kstat_named	sd_rq_pfa_err;
};

#define	SD_MAX_XFER_SIZE	(1024 * 1024)

_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex, scsi_disk))
_NOTE(READ_ONLY_DATA(scsi_disk::un_sd))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_reservation_type))
_NOTE(READ_ONLY_DATA(scsi_disk::un_cmd_stat_size))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_arq_enabled))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_ctype))
_NOTE(SCHEME_PROTECTS_DATA("save sharing",
	scsi_disk::un_mhd_token
	scsi_disk::un_state
	scsi_disk::un_tagflags
	scsi_disk::un_format_in_progress
	scsi_disk::un_gvalid
	scsi_disk::un_resvd_timeid
	scsi_disk::un_allow_bus_device_reset))

_NOTE(SCHEME_PROTECTS_DATA("Safe sharing",
	scsi_disk::un_lbasize))

_NOTE(SCHEME_PROTECTS_DATA("stable data",
	scsi_disk::un_max_xfer_size
	scsi_disk::un_offset
	scsi_disk::un_cmd_flags
	scsi_disk::un_cmd_stat_size))

_NOTE(SCHEME_PROTECTS_DATA("semaphore",
	scsi_disk::un_rqs
	scsi_disk::un_rqs_bp))

_NOTE(SCHEME_PROTECTS_DATA("cv",
	scsi_disk::un_sbufp
	scsi_disk::un_srqbufp
	scsi_disk::un_sbuf_busy))

_NOTE(SCHEME_PROTECTS_DATA("Unshared data",
	dk_cinfo
	uio
	buf
	scsi_pkt
	cdrom_subchnl cdrom_tocentry cdrom_tochdr cdrom_read
	uscsi_cmd
	scsi_capacity
	scsi_cdb scsi_arq_status
	dk_label
	mhioc_inkeys
	mhioc_inresvs
	sd_prout
	dk_map))

/*
 * we use pkt_private area for storing bp and retry_count
 */
struct sd_pkt_private {
	struct buf	*sdpp_bp;
	short		 sdpp_retry_count;
	short		 sdpp_victim_retry_count;
};

#define	PP_LEN	(sizeof (struct sd_pkt_private))

#define	PKT_SET_BP(pkt, bp)	\
	((struct sd_pkt_private *)pkt->pkt_private)->sdpp_bp = bp
#define	PKT_GET_BP(pkt) \
	(((struct sd_pkt_private *)pkt->pkt_private)->sdpp_bp)


#define	PKT_SET_RETRY_CNT(pkt, n) \
	((struct sd_pkt_private *)pkt->pkt_private)->sdpp_retry_count = n

#define	PKT_GET_RETRY_CNT(pkt) \
	(((struct sd_pkt_private *)pkt->pkt_private)->sdpp_retry_count)

#define	PKT_INCR_RETRY_CNT(pkt, n) \
	((struct sd_pkt_private *)pkt->pkt_private)->sdpp_retry_count += n

#define	PKT_SET_VICTIM_RETRY_CNT(pkt, n) \
	((struct sd_pkt_private *)pkt->pkt_private)->sdpp_victim_retry_count \
			= n

#define	PKT_GET_VICTIM_RETRY_CNT(pkt) \
	(((struct sd_pkt_private *)pkt->pkt_private)->sdpp_victim_retry_count)
#define	PKT_INCR_VICTIM_RETRY_CNT(pkt, n) \
	((struct sd_pkt_private *)pkt->pkt_private)->sdpp_victim_retry_count \
			+= n

#define	SD_VICTIM_RETRY_COUNT		(sd_victim_retry_count)
#define	CD_NOT_READY_RETRY_COUNT	(sd_retry_count * 2)
#define	DISK_NOT_READY_RETRY_COUNT	(sd_retry_count / 2)

_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", sd_pkt_private buf))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_device))
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", scsi_status scsi_cdb))

#if DEBUG || lint
#define	SDDEBUG
#endif

/*
 * Debugging macros
 */
#ifdef	SDDEBUG
#define	DEBUGGING	(sddebug > 1)
#define	SD_DEBUG	if (sddebug == 1) scsi_log
#define	SD_DEBUG2	if (sddebug == 2) scsi_log
#define	SD_DEBUG3	if (sddebug == 3) scsi_log
#else	/* SDDEBUG */
#define	sddebug		(0)
#define	DEBUGGING	(0)
#define	SD_DEBUG	if (0) scsi_log
#define	SD_DEBUG2	if (0) scsi_log
#define	SD_DEBUG3	if (0) scsi_log
#endif


#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#define	MAX_THROTTLE	256

/*
 * Disk driver states
 */

#define	SD_STATE_NORMAL		0
#define	SD_STATE_OFFLINE	1
#define	SD_STATE_RWAIT		2
#define	SD_STATE_DUMPING	3
#define	SD_STATE_SUSPENDED	4
#define	SD_STATE_PM_SUSPENDED	5

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
 *                    NORMAL  OFFLINE  RWAIT  DUMPING  SUSPENDED  PM_SUSPENDED
 *
 *   NORMAL              -      (a)      (b)     (c)      (d)       (h)
 *
 *   OFFLINE            (e)      -       (e)     (c)      (d)       NP
 *
 *   RWAIT              (f)     NP        -      (c)      (d)       (h)
 *
 *   DUMPING            NP      NP        NP      -        NP       NP
 *
 *   SUSPENDED          (g)     (g)       (b)     NP*      -        NP
 *
 *   PM_SUSPENDED       (i)     NP        (b)    (c)      (d)       -
 *
 *   NP :       Not Possible.
 *   (a):       Disk does not respond.
 *   (b):       Packet Allocation Fails
 *   (c):       Panic - Crash dump
 *   (d):       DDI_SUSPEND is called.
 *   (e):       Disk has a successful I/O completed.
 *   (f):       sdrunout() calls sdstart() which sets it NORMAL
 *   (g):       DDI_RESUME is called.
 *   (h):	Device threshold exceeded pm framework called power
 *		entry point or pm_lower_power called in detach.
 *   (i):	When new I/O come in.
 *    * :       When suspended, we dont change state during panic dump
 */


/*
 * Error levels
 */

#define	SDERR_ALL		0
#define	SDERR_UNKNOWN		1
#define	SDERR_INFORMATIONAL	2
#define	SDERR_RECOVERED		3
#define	SDERR_RETRYABLE		4
#define	SDERR_FATAL		5

/*
 * Parameters
 */

/*
 * 60 seconds is a *very* reasonable amount of time for most slow CD
 * operations.
 */

#define	SD_IO_TIME	60

/*
 * 2 hours is an excessively reasonable amount of time for format operations.
 */

#define	SD_FMT_TIME	120*60

/*
 * 5 seconds is what we'll wait if we get a Busy Status back
 */

#define	SD_BSY_TIMEOUT		(drv_usectohz(5 * 1000000))

/*
 * 60 seconds is what we will wait for to reset the
 * throttle back to it MAX_THROTTLE.
 */
#define	SD_RESET_THROTTLE_TIMEOUT	60

/*
 * Number of times we'll retry a normal operation.
 *
 * This includes retries due to transport failure
 * (need to distinguish between Target and Transport failure)
 */

#define	SD_RETRY_COUNT		5


/*
 * Maximum number of units we can support
 * (controlled by room in minor device byte)
 * XXX: this is out of date!
 */
#define	SD_MAXUNIT		32

/*
 * 30 seconds is what we will wait for the IO to finish
 * before we fail the DDI_SUSPEND
 */

#define	SD_WAIT_CMDS_COMPLETE	30

/*
 * Prevent/allow media removal flags
 */
#define	SD_REMOVAL_ALLOW	0
#define	SD_REMOVAL_PREVENT	1

/*
 * Save page in mode_select
 */
#define	SD_DONTSAVE_PAGE	0
#define	SD_SAVE_PAGE		1

/*
 * Reservation Status's
 */
#define	SD_RELEASE		0x0000
#define	SD_RESERVE		0x0001
#define	SD_TKOWN		0x0002
#define	SD_LOST_RESERVE		0x0004
#define	SD_FAILFAST		0x0080
#define	SD_WANT_RESERVE		0x0100
#define	SD_RESERVATION_CONFLICT	0x0200

#define	SD_MAX_NODES		16

/*
 * delay before reclaiming reservation is 6 seconds, in units of micro seconds
 */
#define	SD_REINSTATE_RESV_DELAY	6000000

/*
 * sdintr action codes
 */

#define	COMMAND_DONE		0
#define	COMMAND_DONE_ERROR	1
#define	QUE_COMMAND		2
#define	QUE_SENSE		3
#define	JUST_RETURN		4

/*
 * Drive Types (and characteristics)
 */
#define	VIDMAX 8
#define	PIDMAX 16

/*
 * Commands for sd_start_stop
 */
#define	SD_STOP		(0)
#define	SD_START	(1)
#define	SD_EJECT	(2)

/*
 * Target 'type'.
 */
#define	CTYPE_CDROM		0
#define	CTYPE_MD21		1
#define	CTYPE_CCS		2

/*
 * Options
 */
#define	SD_QUEUEING	0x0010	/* Enable Command Queuing to Host Adapter */
#define	SD_NOSERIAL	0x0020	/* Disk has no valid/unique serial number. */

#ifndef	LOG_EMERG
#define	LOG_WARNING	CE_NOTE
#define	LOG_NOTICE	CE_NOTE
#define	LOG_CRIT	CE_WARN
#define	LOG_ERR		CE_WARN
#define	LOG_INFO	CE_NOTE
#define	log	cmn_err
#endif

/*
 * Some internal error codes for driver functions.
 */
#define	SD_EACCES	1

/* sd_ready_and_valid() return value bits */
#define	SD_READY_VALID			0
#define	SD_TUR_FAILED			1
#define	SD_MEDIUM_NOT_PRESENT		2
#define	SD_EINTR			5
/*
 * sd-config-list version number.
 */
#define	SD_CONF_VERSION			1

/*
 * Bit in flags telling driver to set throttle from sd.conf sd-config-list
 * and driver table.  The throttle value used is the first word in the
 * datalist, after flags.
 */
#define	SD_CONF_SET_THROTTLE		0

/*
 * Bit in flags telling driver to set controller type.  This value must be
 * either CTYPE_CDROM, CTYPE_CCS or CTYPE_MD21 as defined in this file.
 * The value used is the word two past the flags.
 */
#define	SD_CONF_SET_CTYPE		1

/*
 * Bit in flags telling driver to set the not ready retry count for a device.
 */
#define	SD_CONF_SET_NOTREADY_RETRIES	2

/*
 * Set disk attribute to not have a valid/unique serial number.
 * The field in the property value array is ignored.
 */
#define	SD_CONF_SET_NOSERIAL		3

/*
 * Bit in flags telling driver to disable all caching for disk device.  The
 * corresponding value in the datalist is ignored.
 */
#define	SD_CONF_SET_NOCACHE		4


#define	SD_CONF_BSET_THROTTLE	(1 << SD_CONF_SET_THROTTLE)
#define	SD_CONF_BSET_CTYPE	(1 << SD_CONF_SET_CTYPE)
#define	SD_CONF_BSET_NRR_COUNT	(1 << SD_CONF_SET_NOTREADY_RETRIES)
#define	SD_CONF_BSET_NOSERIAL	(1 << SD_CONF_SET_NOSERIAL)
#define	SD_CONF_BSET_NOCACHE	(1 << SD_CONF_SET_NOCACHE)

/*
 * This is the number of items currently settable in the sd.conf
 * sd-config-list.  Update it when add new properties above.
 */
#define	SD_CONF_MAX_ITEMS		5

/*
 * This is the number of items currently settable in the ssd.conf
 * ssd-config-list.  Update it when add new properties above.
 */
#define	SSD_CONF_MAX_ITEMS 	2

/* Power management defines */

#define	SD_SPINDLE_OFF	0x0
#define	SD_SPINDLE_ON	0x1

/*
 * Could move this define to some thing like log sense.h in SCSA headers
 * But for now let it live here.
 */
#define	START_STOP_CYCLE_COUNTER_PAGE_SIZE	0x28
#define	START_STOP_CYCLE_PAGE		0x0E
#define	START_STOP_CYCLE_VU_PAGE	0x31

/*
 * This define is used to take care of the race condition in power
 * management. This flag when set in un_flag_busy it indicates that device is
 * about to become busy.
 */
#define	SD_BECOMING_ACTIVE	0x01


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_TARGETS_SDDEF_H */
