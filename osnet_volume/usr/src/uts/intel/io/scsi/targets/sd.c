/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sd.c	1.350	99/11/01 SMI"

/*
 * SCSI disk driver for Solaris machines.
 *
 * Supports:
 * 	1. SCSI-2 disks.
 *	2. 512 bytes and 2K CDROMs.
 *	3. removable devices (currently not tested)
 */

/*
 * Includes, Declarations and Local Data
 */
#include <sys/scsi/scsi.h>
#include <sys/dkbad.h>
#include <sys/dklabel.h>
#include <sys/dkio.h>
#include <sys/cdio.h>
#include <sys/mhd.h>
#include <sys/vtoc.h>
#include <sys/dktp/fdisk.h>
#include <sys/scsi/targets/sddef.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kstat.h>
#include <sys/vtrace.h>
#include <sys/aio_req.h>
#include <sys/note.h>
#include "sd_ddi.h"
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/var.h>

#ifndef _lock_lint
#undef _NOTE
#define	_NOTE(s)
#define	LOCK_LINTED
#else
#define	LOCK_LINTED	/*LINTED*/
#endif

/*
 * es_code value for deferred error
 */
#define	SD_DEFERRED_ERROR	0x01

/*
 * Global Error Levels for Error Reporting
 */
int sd_error_level	= SCSI_ERR_RETRYABLE;

/*
 * Local Static Data
 */

static int sd_io_time		= SD_IO_TIME;
static int sd_reset_retry_count	= SD_RETRY_COUNT/2;
static int sd_retry_count	= SD_RETRY_COUNT;
static int sd_victim_retry_count = 2*SD_RETRY_COUNT;
static int sd_rot_delay		= 4;	/* default 4ms Rotation delay */
static int sd_check_media_time	= 3000000;	/* 3 Second State Check */
static int sd_failfast_enable	= 1;	/* Really Halt */
static int sd_max_throttle	= MAX_THROTTLE;
static int sd_reset_throttle_timeout = SD_RESET_THROTTLE_TIMEOUT;
static int sd_retry_on_reservation_conflict = 1;
static int reinstate_resv_delay = SD_REINSTATE_RESV_DELAY;
static int sd_report_pfa = 1;
static int sd_wait_cmds_complete = SD_WAIT_CMDS_COMPLETE;
static int sd_max_nodes = SD_MAX_NODES;

/*
 * Local Function Prototypes
 */

static int sdopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p);
static int sdclose(dev_t dev, int flag, int otyp, cred_t *cred_p);
static int sdstrategy(struct buf *bp);
static int sddump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk);
static int sdioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int sdread(dev_t dev, struct uio *uio, cred_t *cred_p);
static int sdwrite(dev_t dev, struct uio *uio, cred_t *cred_p);
static int sd_prop_op(dev_t, dev_info_t *, ddi_prop_op_t, int,
    char *, caddr_t, int *);
static int sdaread(dev_t dev, struct aio_req *aio, cred_t *cred_p);
static int sdawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p);

static int sd_blank_cmp(struct scsi_disk *un, char *device_id);
static int sd_get_sdconf_table(struct scsi_disk *un, int *showstopper);
static int sd_set_dev_properties(struct scsi_disk *un, int flags,
    int *data_list);
static void sd_free_softstate(struct scsi_disk *un, dev_info_t *devi);
static int sd_doattach(dev_info_t *devi, int (*f)());
static int sd_validate_geometry(struct scsi_disk *un, int (*f)());
static int sd_uselabel(struct scsi_disk *un, struct dk_label *l);
static int sd_winchester_exists(struct scsi_disk *un,
	struct scsi_pkt *rqpkt, int (*canwait)());
int sd_read_capacity(struct scsi_disk *un, struct scsi_capacity *cptr);


int sd_clear_cont_alleg(struct scsi_disk *un, struct scsi_pkt *pkt);
int sd_scsi_poll(struct scsi_disk *un, struct scsi_pkt *pkt);
static void sdmin(struct buf *bp);
static void sduscsimin(struct buf *bp);

static int sdioctl_cmd(dev_t, struct uscsi_cmd *,
	enum uio_seg, enum uio_seg, enum uio_seg);

static void sdstart(struct scsi_disk *un);
static int sdrunout(caddr_t arg);
static void sddone_and_mutex_exit(struct scsi_disk *un, struct buf *bp);
static int sd_setup_next_xfer(struct scsi_disk *un, struct buf *bp,
	    struct scsi_pkt *pkt, struct sd_pkt_private *sdpp);
static struct buf *make_sd_cmd(struct scsi_disk *un, struct buf *bp,
	int (*f)());

static int sd_check_wp(dev_t dev, int use_group2);
static int sd_disable_caching(struct scsi_disk *un);
static int sd_unit_ready(dev_t dev);
static int sd_lock_unlock(dev_t dev, int flag);
static void sdrestart(void *arg);
static void sd_reset_throttle(void *arg);
static void sd_handle_tran_busy(struct buf *bp, struct diskhd *dp,
				struct scsi_disk *un);
static int sd_mhd_watch_cb(caddr_t arg, struct scsi_watch_result *resultp);
static void sd_mhd_watch_incomplete(struct scsi_disk *un, struct scsi_pkt *pkt);
static void sd_mhd_resvd_recover(void *arg);
static void sd_mhd_reset_notify_cb(caddr_t arg);
static void sd_restart_unit(void *);
static void sd_delayed_cv_broadcast(void *);
static void sd_restart_unit_callback(struct scsi_pkt *pkt);
static int sd_start_stop(dev_t dev, int  mode);
static int sd_eject(dev_t dev);
static int sd_take_ownership(dev_t dev, struct mhioctkown *p);
static int sd_reserve_release(dev_t dev, int cmd);
static int sd_check_media(dev_t dev, enum dkio_state state);
static int sd_check_mhd(dev_t dev, int interval);
static void sd_timeout_thread();
static int sd_create_timeout_thread(void (*func)());
static void sd_timeout_destroy(dev_t dev);
static void sd_check_pr(struct scsi_disk *un);
static int sd_prout(dev_t dev, int cmd, void *p);
static int sd_prin(dev_t dev, int cmd, void *p, int flag);


static void sdintr(struct scsi_pkt *pkt);
static int sd_handle_incomplete(struct scsi_disk *un, struct buf *bp);
static int sd_handle_sense(struct scsi_disk *un, struct buf *bp);
static int sd_handle_autosense(struct scsi_disk *un, struct buf *bp);
static int sd_decode_sense(struct scsi_disk *un, struct buf *bp,
    struct scsi_status *statusp, ulong_t state, uchar_t resid);
static int sd_handle_resv_conflict(struct scsi_disk *un, struct buf *bp);
static void sd_errmsg(struct scsi_disk *un,
    struct scsi_pkt *pkt, int severity, daddr_t blkno);
static int sd_check_error(struct scsi_disk *un, struct buf *bp);
static int sd_handle_ua(struct scsi_disk *un);
static int sd_handle_mchange(struct scsi_disk *un);
static void sd_ejected(struct scsi_disk *un);
static int sd_not_ready(struct scsi_disk *un, struct scsi_pkt *pkt);
static void sd_offline(struct scsi_disk *un, int bechatty);
char *sd_sname(uchar_t status);
struct buf *sd_shadow_iodone(struct buf *bp);
struct buf *sd_nodevbsize_blksize(struct buf *bp, struct scsi_disk *un);
static int sd_ready_and_valid(dev_t dev, struct scsi_disk *un);
static void sd_reset_disk(struct scsi_disk *un, struct scsi_pkt *pkt);
static void sd_requeue_cmd(struct scsi_disk *un, struct buf *bp, clock_t tval);

struct buf *sd_overrun(struct buf *bp, size_t resid);
struct buf *sd_overrun_iodone(struct buf *bp);
static int sd_synchronize_cache(dev_t dev);
void sd_read_vers1_params(struct scsi_disk *un, int *data_list,
			    int data_list_len, caddr_t dataname_ptr,
				int *showstopper);
void sd_read_vers10_params(struct scsi_disk *un, int *data_list,
			    int data_list_len, caddr_t dataname_ptr);

/*
 * CDROM specific stuff
 */
static int sr_sector_mode(dev_t dev, int mode);

static int sr_pause_resume(dev_t dev, int mode);
static int sr_play_msf(dev_t dev, caddr_t data, int flag);
static int sr_play_trkind(dev_t dev, caddr_t data, int flag);
static int sr_volume_ctrl(dev_t dev, caddr_t  data, int flag);
static int sr_read_subchannel(dev_t dev, caddr_t data, int flag);
static int sr_read_mode2(dev_t dev, caddr_t data, int flag);
static int sr_read_cd_mode2(dev_t dev, caddr_t data, int flag);
static int sr_read_mode1(dev_t dev, caddr_t data, int flag);
static int sr_read_tochdr(dev_t dev, caddr_t data, int flag);
static int sr_read_tocentry(dev_t dev, caddr_t data, int flag);
#ifdef CDROMREADOFFSET
static int sr_read_sony_session_offset(dev_t dev, caddr_t data, int flag);
#endif
int sd_mode_sense(dev_t dev, int page, char *page_data, int page_size,
    int use_group2);
static int sd_mode_select(dev_t dev, char *page_data, int page_size,
    int save_page, int use_group2);
static int sr_change_blkmode(dev_t dev, int cmd, intptr_t data, int flag);
static int sr_change_speed(dev_t dev, int cmd, intptr_t data, int flag);
static int sr_set_speed(dev_t dev, int cmd, intptr_t data, int flag);
static int sr_read_cdda(dev_t dev, caddr_t data, int flag);
static int sr_read_cdxa(dev_t dev, caddr_t data, int flag);
static int sr_read_all_subcodes(dev_t dev, caddr_t data, int flag);

/*
 * Get information about the device media
 */
static int sd_get_media_info(dev_t dev, caddr_t data, int flag);

/*
 * Vtoc Control
 */
#if defined(_SUNOS_VTOC_8)
static void sd_build_user_vtoc(struct scsi_disk *un, struct vtoc *vtoc);
#endif	/* defined(_SUNOS_VTOC_8) */
static int sd_build_label_vtoc(struct scsi_disk *un, struct vtoc *vtoc);
static int sd_write_label(dev_t dev);

/*
 * Error and Logging Functions
 */
static void clean_print(dev_info_t *dev, char *label, uint_t level,
	char *title, char *data, int len);
static void inq_fill(char *p, int l, char *s);

/*
 * Error statistics create/update functions
 */
static int sd_create_errstats(struct scsi_disk *, int);

/*
 * Configuration Routines
 */
static int sdinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int sdprobe(dev_info_t *devi);
static int sdattach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int sddetach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int sd_dr_detach(dev_info_t *devi);
static int sdpower(dev_info_t *devi, int component, int level);

/*
 * byte order conversion functions
 */
uint_t	sd_stoh_int(uchar_t *p);
ushort_t sd_stoh_short(uchar_t *p);
uint_t sd_letoh_int(uchar_t *p);
ushort_t sd_letoh_short(uchar_t *p);

extern struct scsi_key_strings scsi_cmds[];

void *sd_state;
static int sd_max_instance;
char *sd_label = "sd";

static char *diskokay = "disk okay\n";

#if DEBUG || lint
#define	SDDEBUG
int sddebug = 0;
#endif

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", reinstate_resv_delay))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_isusb))


/*
 * Create a single thread to handle reinstating reservations on all
 * devices that have lost reservations. This is to prevent blocking
 * from within a 'timeout' thread. We log sd_timeout_requests for
 * all devices that we think have LOST RESERVATIONS when the scsi watch
 * thread callsback sd_mhd_watch_cb and this timeout thread loops through
 * the requests to regain the lost reservations.
 */

struct sd_thr_request {
	dev_t	dev;
	struct sd_thr_request *sd_thr_req_next;
};

static struct sd_timeout_request {
	kthread_t *sd_timeout_thread;
	struct sd_thr_request *sd_thr_req_head;
	struct sd_thr_request *sd_thr_cur_req;
	kcondvar_t sd_inprocess_cv;
	kmutex_t sd_timeout_mutex;
	kcondvar_t sd_timeout_cv;
} sd_tr = {NULL, NULL, NULL, 0, };

uchar_t sd_save_state = 0;

_NOTE(MUTEX_PROTECTS_DATA(sd_timeout_request::sd_timeout_mutex,
				sd_timeout_request))
_NOTE(SCHEME_PROTECTS_DATA("unshared data", sd_thr_request))

static struct driver_minor_data {
	char	*name;
	minor_t	minor;
	int	type;
} sd_minor_data[] = {
	{"a", 0, S_IFBLK},
	{"b", 1, S_IFBLK},
	{"c", 2, S_IFBLK},
	{"d", 3, S_IFBLK},
	{"e", 4, S_IFBLK},
	{"f", 5, S_IFBLK},
	{"g", 6, S_IFBLK},
	{"h", 7, S_IFBLK},
#if defined(_SUNOS_VTOC_16)
	{"i", 8, S_IFBLK},
	{"j", 9, S_IFBLK},
	{"k", 10, S_IFBLK},
	{"l", 11, S_IFBLK},
	{"m", 12, S_IFBLK},
	{"n", 13, S_IFBLK},
	{"o", 14, S_IFBLK},
	{"p", 15, S_IFBLK},
#endif			/* defined(_SUNOS_VTOC_16) */
#if defined(_FIRMWARE_NEEDS_FDISK)
	{"q", 16, S_IFBLK},
	{"r", 17, S_IFBLK},
	{"s", 18, S_IFBLK},
	{"t", 19, S_IFBLK},
	{"u", 20, S_IFBLK},
#endif			/* defined(_FIRMWARE_NEEDS_FDISK) */
	{"a,raw", 0, S_IFCHR},
	{"b,raw", 1, S_IFCHR},
	{"c,raw", 2, S_IFCHR},
	{"d,raw", 3, S_IFCHR},
	{"e,raw", 4, S_IFCHR},
	{"f,raw", 5, S_IFCHR},
	{"g,raw", 6, S_IFCHR},
	{"h,raw", 7, S_IFCHR},
#if defined(_SUNOS_VTOC_16)
	{"i,raw", 8, S_IFCHR},
	{"j,raw", 9, S_IFCHR},
	{"k,raw", 10, S_IFCHR},
	{"l,raw", 11, S_IFCHR},
	{"m,raw", 12, S_IFCHR},
	{"n,raw", 13, S_IFCHR},
	{"o,raw", 14, S_IFCHR},
	{"p,raw", 15, S_IFCHR},
#endif			/* defined(_SUNOS_VTOC_16) */
#if defined(_FIRMWARE_NEEDS_FDISK)
	{"q,raw", 16, S_IFCHR},
	{"r,raw", 17, S_IFCHR},
	{"s,raw", 18, S_IFCHR},
	{"t,raw", 19, S_IFCHR},
	{"u,raw", 20, S_IFCHR},
#endif			/* defined(_FIRMWARE_NEEDS_FDISK) */
	{0}
};

/*
 * Note that the SD_CONF_SET_THROTTLE definition in sddef.h applies to
 * the hard disk table as well as to reading the prop from the config
 * file.
 */

#define	ELITE_THROTTLE_VALUE	10
#define	ST31200N_THROTTLE_VALUE	8
#define	SYMBIOS_THROTTLE_VALUE	16

/*
 * The SD_CONF_SET_NOTREADY_RETRIES definition in sddef.h applies to the
 * disk config table as well as to reading the property from the config
 * file
 */

#define	SYMBIOS_NOTREADY_RETRIES	12

/*
 * Fields of the properties arrays below are defined by the SD_CONF_SET_*
 * definitions in sddef.h.
 */

static int elite_properties[SD_CONF_MAX_ITEMS] = {
	ELITE_THROTTLE_VALUE, 0, 0
};
static int emulex_properties[SD_CONF_MAX_ITEMS] = {
	0, CTYPE_MD21, 0
};
static int st31200n_properties[SD_CONF_MAX_ITEMS] = {
	ST31200N_THROTTLE_VALUE, 0, 0
};
static int symbios_properties[SD_CONF_MAX_ITEMS] = {
	SYMBIOS_THROTTLE_VALUE, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	SYMBIOS_NOTREADY_RETRIES,
};

/*
 * This is the table of disks which need throttle adjustment (or, perhaps
 * something else as defined by the flags at a future time.)  device_id
 * is a string consisting of concatenated vid (vendor), pid (product/model)
 * and revision strings as defined in the scsi_inquiry structure.  Offsets of
 * the parts of the string are as defined by the sizes in the scsi_inquiry
 * structure.  Device type is searched as far as the device_id string is
 * defined.  Flags defines which values are to be set in the driver from the
 * properties list.
 */
struct sd_disk_config {
	char	*device_id;
	uint_t	flags;
	int	*properties;
};
typedef struct sd_disk_config sd_disk_config_t;

/*
 * Entries below which begin and end with a "*" are a special case.
 * These do not have a specific vendor, and the string which follows
 * can appear anywhere in the 16 byte PID portion of the inquiry data.
 *
 * Entries below which begin and end with a " " (blank) are a special
 * case. The comparison function will treat multiple consecutive blanks
 * as equivalent to a single blank. For example, this causes a
 * sd_disk_table entry of " NEC CDROM " to match a device's id string
 * of  "NEC       CDROM".

 *
 */

static sd_disk_config_t sd_disk_table[] = {
	{ "SEAGATE ST42400N", SD_CONF_BSET_THROTTLE, elite_properties },
	{ "SEAGATE ST31200N", SD_CONF_BSET_THROTTLE, st31200n_properties },
	{ "CONNER  CP30540", SD_CONF_BSET_NOCACHE, NULL },
	{ "EMULEX  MD21", SD_CONF_BSET_CTYPE, emulex_properties },
	{ "*SUN0104*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN0207*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN0327*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN0340*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN0424*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN0669*", SD_CONF_BSET_NOSERIAL, NULL },
	{ "*SUN1.0G*", SD_CONF_BSET_NOSERIAL, NULL },

	{ " NEC CD-ROM DRIVE:260 ", (SD_CONF_BSET_PLAYMSF_BCD
				    | SD_CONF_BSET_READSUB_BCD
				    | SD_CONF_BSET_READ_TOC_ADDR_BCD
				    | SD_CONF_BSET_NO_READ_HEADER
				    | SD_CONF_BSET_READ_CD_XD4), NULL },

	{ " NEC CD-ROM DRIVE:270 ", (SD_CONF_BSET_PLAYMSF_BCD
				    | SD_CONF_BSET_READSUB_BCD
				    | SD_CONF_BSET_READ_TOC_ADDR_BCD
				    | SD_CONF_BSET_NO_READ_HEADER
				    | SD_CONF_BSET_READ_CD_XD4), NULL },
	{ "SYMBIOS INF-01-00       ", SD_CONF_BSET_NOSERIAL, NULL },
	{ "Symbios", SD_CONF_BSET_THROTTLE|SD_CONF_BSET_NRR_COUNT,
		symbios_properties },
};
static int sd_disk_table_size =
    sizeof (sd_disk_table)/ sizeof (sd_disk_config_t);

/*
 * Urk!
 */
#define	SET_BP_ERROR(bp, err)	\
	(bp)->b_oerror = (err), bioerror(bp, err);

#define	IOSP			KSTAT_IO_PTR(un->un_stats)
#define	IO_PARTITION_STATS	un->un_pstats[SDPART(bp->b_edev)]
#define	IOSP_PARTITION		KSTAT_IO_PTR(IO_PARTITION_STATS)

#define	SD_DO_KSTATS(un, kstat_function, bp) \
	ASSERT(mutex_owned(SD_MUTEX)); \
	if (bp != un->un_sbufp) { \
		if (un->un_stats) { \
			kstat_function(IOSP); \
		} \
		if (IO_PARTITION_STATS) { \
			kstat_function(IOSP_PARTITION); \
		} \
	}

#define	SD_DO_ERRSTATS(un, x)  \
	if (un->un_errstats) { \
		struct sd_errstats *stp; \
		stp = (struct sd_errstats *)un->un_errstats->ks_data; \
		stp->x.value.ui32++; \
	}

#define	GET_SOFT_STATE(dev)						\
	register struct scsi_disk *un;					\
	register int instance, part;					\
	register minor_t minor = getminor(dev);				\
									\
	part = minor & SDPART_MASK;					\
	instance = minor >> SDUNIT_SHIFT;				\
	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL) {	\
		scsi_probe_cache_clear(instance);			\
		return (ENXIO);						\
	}

#ifndef FDEJECT
#define	FDEJECT (('f'<<8)|112)	/* floppy eject */
#endif

/*
 * Configuration Data
 */

/*
 * Device driver ops vector
 */

static struct cb_ops sd_cb_ops = {

	sdopen,			/* open */
	sdclose,		/* close */
	sdstrategy,		/* strategy */
	nodev,			/* print */
	sddump,			/* dump */
	sdread,			/* read */
	sdwrite,		/* write */
	sdioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	sd_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_64BIT | D_MP | D_NEW | D_HOTPLUG, /* Driver compatibility flag */
	CB_REV,			/* cb_rev */
	sdaread, 		/* async I/O read entry point */
	sdawrite		/* async I/O write entry point */
};

static struct dev_ops sd_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	sdinfo,			/* info */
	nulldev,		/* identify */
	sdprobe,		/* probe */
	sdattach,		/* attach */
	sddetach,		/* detach */
	nodev,			/* reset */
	&sd_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	sdpower			/* power */
};


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"SCSI Disk Driver 1.350",	/* Name of the module. */
	&sd_ops,	/* driver ops */
};



static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * the sd_attach_mutex only protects sd_max_instance in multi-threaded
 * attach situations
 */
static kmutex_t sd_attach_mutex;

/*
 * sd_log_mutex is for printing logs without having to use expensive
 * stack allocations
 */
kmutex_t sd_log_mutex;

char _depends_on[] = "misc/scsi";

/* Probe cache variables */
/* #define	SCSI_PROBE_CACHE_DEBUG */
int sd_probe_cache_enable = 1;
static kmutex_t scsi_probe_cache_mutex;
struct scsi_probe_cache {
	struct scsi_probe_cache *next;
	dev_info_t *pdip;
	int cache[NTARGETS_WIDE];
};
static struct scsi_probe_cache *scsi_probe_cache_head;

int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&sd_state, sizeof (struct scsi_disk),
	    SD_MAXUNIT)) != 0)
		return (e);

	mutex_init(&sd_attach_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&sd_log_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&sd_tr.sd_timeout_mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&sd_tr.sd_timeout_cv, NULL, CV_DRIVER, NULL);
	cv_init(&sd_tr.sd_inprocess_cv, NULL, CV_DRIVER, NULL);
	mutex_init(&scsi_probe_cache_mutex, NULL, MUTEX_DRIVER, NULL);
	e = mod_install(&modlinkage);
	if (e != 0) {
		mutex_destroy(&scsi_probe_cache_mutex);
		mutex_destroy(&sd_attach_mutex);
		mutex_destroy(&sd_log_mutex);
		mutex_destroy(&sd_tr.sd_timeout_mutex);
		cv_destroy(&sd_tr.sd_timeout_cv);
		cv_destroy(&sd_tr.sd_inprocess_cv);
		ddi_soft_state_fini(&sd_state);
		return (e);
	}

	return (e);
}

int
_fini(void)
{
	int e;
	struct scsi_probe_cache *cp, *ncp;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	for (cp = scsi_probe_cache_head; cp != NULL; cp = ncp) {
		ncp = cp->next;
		kmem_free(cp, sizeof (struct scsi_probe_cache));
	}
	scsi_probe_cache_head = NULL;
	mutex_destroy(&scsi_probe_cache_mutex);
	ddi_soft_state_fini(&sd_state);
	mutex_destroy(&sd_attach_mutex);
	mutex_destroy(&sd_log_mutex);
	mutex_destroy(&sd_tr.sd_timeout_mutex);
	cv_destroy(&sd_tr.sd_timeout_cv);
	cv_destroy(&sd_tr.sd_inprocess_cv);


	return (e);
}

int
_info(struct modinfo *modinfop)
{

	return (mod_info(&modlinkage, modinfop));
}

/*
 * scsi_probe with NORESP cache per target.
 *
 * Called from sdprobe in place of the call to scsi_probe.
 *
 * If we get no response from a target duing a probe inquiry, we
 * remember that, and we avoid additional calls to scsi_probe on
 * non-zero LUNs on the same target until the cache is cleared.
 * By doing so we avoid the 1/4 sec selection timeout for nonzero
 * LUNs.
 *
 * We need not do this from sd_doattach, since it is only called
 * if sdprobe succeeds.
 *
 * We always do the probe for lun=0.  That way if a drvconfig is
 * done we will recompute the cache for that target.  This depends
 * on the probe order being LUN zero first, which presently depends
 * on lun=0 for a target being enumerated in the sd.conf file ahead
 * of nonzero luns for that target.  This is one reason this is
 * considered a workaround rather than a long term fix.  XXX
 *
 * Another possible optimization that could be added to this
 * workaround might be logic that examines the inquiry string from
 * LUN zero and determines what the maximum possible LUN can be
 * for that device.  SCSI-1 devices have a maximum of 8 LUNs.
 * SCSI-2 theoretically also has this maximum, but some SCSI-2
 * devices support up to 32 using reserved bits.  LUNs above these
 * limits could be rejected (NORESP) immediately from this routine.
 *
 * Per-target-type properties could also be used to limit the
 * maximum LUN that this routine will pass to scsi_probe.
 *
 * Since there is no selection timeout for NOTPRESENT LUNs on a
 * target where LUN zero responds, such additional optimizations
 * would not result in large performance benefits in terms of probe
 * time.  However, they could possibly reduce the risk associated
 * with sending large LUN numbers into older HBA drivers (via the
 * class="scsi" entries in sd.conf); older HBA drivers have never
 * been tested to make sure they properly reject LUN numbers they
 * cannot handle.
 *
 * Even more ambitious would be to use the REPORT_LUNS command to
 * augment the above additional possible optimization.
 *
 */

static int
scsi_probe_cache_probe(struct scsi_device *devp, int (*waitfunc)())
{
	struct scsi_probe_cache *cp;
	dev_info_t *pdip = ddi_get_parent(devp->sd_dev);
	int rval;
	int lun = devp->sd_address.a_lun;
	int tgt = devp->sd_address.a_target;

	/* Make sure cacheing enabled and target in range */
	if (!sd_probe_cache_enable || tgt < 0 || tgt >= NTARGETS_WIDE) {
		/* do it the old way (no cache) */
		return (scsi_probe(devp, waitfunc));
	}

	/* Find the cache for this scsi bus instance */
	mutex_enter(&scsi_probe_cache_mutex);

	for (cp = scsi_probe_cache_head; cp != NULL; cp = cp->next) {
		if (cp->pdip == pdip)
			break;
	}

	/* If we can't find a cache for this pdip, create one */
	if (cp == NULL) {
		cp = kmem_zalloc(sizeof (struct scsi_probe_cache), KM_SLEEP);
		cp->pdip = pdip;
		cp->next = scsi_probe_cache_head;
		scsi_probe_cache_head = cp;
	}

	mutex_exit(&scsi_probe_cache_mutex);

	/* Recompute the cache for this target if LUN zero */
	if (lun == 0) {
		cp->cache[tgt] = 0;
	}

	/* If cache remembers a NORESP from a previous LUN, return NORESP now */
	if (cp->cache[tgt] != 0) {
#ifdef	SCSI_PROBE_CACHE_DEBUG
		cmn_err(CE_NOTE, "sdprobe: return cached NORESP"
		    " target %d, lun %d", tgt, lun);
#endif
		return (SCSIPROBE_NORESP);
	}

	/* Need to recompute the cache -- do the probe */
	rval = scsi_probe(devp, waitfunc);

#ifdef	SCSI_PROBE_CACHE_DEBUG
	cmn_err(CE_NOTE, "sdprobe: scsi_probe pdip 0x%p,"
	    " t=%d, l=%d (%d), resp=%d (%s)",
	    ddi_get_parent(devp->sd_dev),
	    devp->sd_address.a_target,
	    devp->sd_address.a_lun,
	    ddi_getprop(DDI_DEV_T_ANY, devp->sd_dev, DDI_PROP_DONTPASS,
		"lun", -1),
	    rval,
	    rval == SCSIPROBE_EXISTS?"EXISTS":
		rval == SCSIPROBE_NONCCS ? "NONCCS" :
		rval == SCSIPROBE_NORESP ? "NORESP" :
		rval == SCSIPROBE_NOMEM ? "NOMEM" :
		rval == SCSIPROBE_FAILURE ? "FAILURE" :
		rval == SCSIPROBE_BUSY ? "BUSY" :
		rval == SCSIPROBE_NOMEM_CB ? "NOMEM_CB" : "???");
	if (rval == SCSIPROBE_EXISTS)
		cmn_err(CE_NOTE, "sdprobe: scsi_probe inq_dtype=%d (%s)",
		    devp->sd_inq->inq_dtype,
		    devp->sd_inq->inq_dtype == DTYPE_DIRECT ? "DIRECT":
		    devp->sd_inq->inq_dtype == DTYPE_RODIRECT ? "RODIRECT":
		    devp->sd_inq->inq_dtype == DTYPE_OPTICAL ? "OPTICAL":
		    devp->sd_inq->inq_dtype == DTYPE_NOTPRESENT ? "NOTPRESENT" :
		    "???");
#endif

	/* If we got a timeout on this target, remember for later */
	if (rval == SCSIPROBE_NORESP) {
		cp->cache[tgt] = 1;
	}

	return (rval);
}

/*
 * Clear the probe response cache.
 *
 * This is done when open() returns ENXIO so that when deferred
 * attach is attempted (possibly after a device has been turned
 * on) we will retry the probe.  Since we don't know which target
 * we failed to open, we just clear the entire cache.
 *
 */
static void
scsi_probe_cache_clear(int instance)
{
	int i;
	struct scsi_probe_cache *cp;

#if	defined(SCSI_PROBE_CACHE_DEBUG) || defined(lint)
	cmn_err(CE_NOTE, "sd ENXIO instance %d", instance);
#endif

	mutex_enter(&scsi_probe_cache_mutex);
	for (cp = scsi_probe_cache_head; cp != NULL; cp = cp->next) {
		for (i = 0; i < NTARGETS_WIDE; i++)
			cp->cache[i] = 0;
	}
	mutex_exit(&scsi_probe_cache_mutex);
}

static int
sdprobe(dev_info_t *devi)
{
	register struct scsi_device *devp;
	register int rval = DDI_PROBE_PARTIAL;
	int instance;

	devp = (struct scsi_device *)ddi_get_driver_private(devi);
	instance = ddi_get_instance(devi);

	/*
	 * Keep a count of how many disks (ie. highest instance no) we have
	 * XXX currently not used but maybe useful later again
	 */
	mutex_enter(&sd_attach_mutex);
	if (instance > sd_max_instance)
		sd_max_instance = instance;
	mutex_exit(&sd_attach_mutex);

	SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
		    "sdprobe:\n");

	if (ddi_get_soft_state(sd_state, instance) != NULL)
		return (DDI_PROBE_PARTIAL);

	/*
	 * Turn around and call utility probe routine
	 * to see whether we actually have a disk at
	 * this SCSI nexus.
	 */

	SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
	    "sdprobe: %x\n", scsi_probe(devp, NULL_FUNC));

	switch (scsi_probe_cache_probe(devp, NULL_FUNC)) {
	default:
	case SCSIPROBE_NORESP:
	case SCSIPROBE_NONCCS:
	case SCSIPROBE_NOMEM:
	case SCSIPROBE_FAILURE:
	case SCSIPROBE_BUSY:
		break;

	case SCSIPROBE_EXISTS:
		switch (devp->sd_inq->inq_dtype) {
		case DTYPE_DIRECT:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_DIRECT\n");
			rval = DDI_PROBE_SUCCESS;
			break;

		case DTYPE_RODIRECT:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_RODIRECT\n");
			rval = DDI_PROBE_SUCCESS;
			break;

		case DTYPE_OPTICAL:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_OPTICAL\n");
			rval = DDI_PROBE_SUCCESS;
			break;

		case DTYPE_NOTPRESENT:
		default:
			rval = DDI_PROBE_FAILURE;
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_FAILURE\n");
			break;
		}
	}
	scsi_unprobe(devp);

	return (rval);
}


static int
sd_write_deviceid(struct scsi_disk *un)
{
	int status;
	daddr_t spc, blk, head, cyl;
	struct uscsi_cmd ucmd;
	union scsi_cdb cdb;
	struct dk_devid *dkdevid;
	uint_t *ip, chksum;
	int i;
	dev_t dev;

	if (un->un_g.dkg_acyl < 2)
		return (EINVAL);

	/* Subtract 2 guarantees that the next to last cylinder is used */
	cyl = un->un_g.dkg_ncyl + un->un_g.dkg_acyl - 2;
	spc = un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	head = un->un_g.dkg_nhead - 1;
	blk = (cyl * (spc - un->un_g.dkg_apc)) +
	    (head * un->un_g.dkg_nsect) + 1;

	/* Allocate the buffer */
	mutex_exit(SD_MUTEX);
	dkdevid = kmem_zalloc(DEV_BSIZE, KM_SLEEP);
	mutex_enter(SD_MUTEX);

	/* Fill in the revision */
	dkdevid->dkd_rev_hi = DK_DEVID_REV_MSB;
	dkdevid->dkd_rev_lo = DK_DEVID_REV_LSB;

	/* Copy in the device id */
	bcopy(un->un_devid, &dkdevid->dkd_devid,
	    ddi_devid_sizeof(un->un_devid));

	/* Calculate the checksum */
	chksum = 0;
	ip = (uint_t *)dkdevid;
	for (i = 0; i < ((DEV_BSIZE - sizeof (int))/sizeof (int)); i++)
		chksum ^= sd_stoh_int((uchar_t *)&ip[i]);

	/* Fill-in checksum */
	DKD_FORMCHKSUM(chksum, dkdevid);

	bzero(&ucmd, sizeof (ucmd));
	bzero(&cdb, sizeof (cdb));
	ucmd.uscsi_flags = USCSI_WRITE;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_bufaddr = (caddr_t)dkdevid;
	ucmd.uscsi_buflen = DEV_BSIZE;
	ucmd.uscsi_flags |= USCSI_SILENT;

	dev = makedevice(ddi_name_to_major(ddi_get_name(SD_DEVINFO)),
	    ddi_get_instance(SD_DEVINFO) << SDUNIT_SHIFT);

	/*
	 * Write and verify the backup labels.
	 */
	if ((blk >= (2 << 20)) || SD_GRP1_2_CDBS(un)) {
		cdb.scc_cmd = SCMD_WRITE_G1;
		FORMG1ADDR(&cdb, blk);
		FORMG1COUNT(&cdb, 1);
		ucmd.uscsi_cdblen = CDB_GROUP1;
	} else {
		cdb.scc_cmd = SCMD_WRITE;
		FORMG0ADDR(&cdb, blk);
		FORMG0COUNT(&cdb, (uchar_t)1);
		ucmd.uscsi_cdblen = CDB_GROUP0;
	}

	/* Write the reserved sector */
	mutex_exit(SD_MUTEX);
	status = sdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	mutex_enter(SD_MUTEX);
	kmem_free(dkdevid, DEV_BSIZE);
	return (status);
}

static int
sd_read_deviceid(struct scsi_disk *un)
{
	int status;
	daddr_t spc, blk, head, cyl;
	struct uscsi_cmd ucmd;
	union scsi_cdb cdb;
	struct dk_devid *dkdevid;
	uint_t *ip;
	int chksum;
	int i;
	size_t sz;
	dev_t dev;

	if (un->un_g.dkg_acyl < 2)
		return (EINVAL);

	/* Subtract 2 guarantees that the next to last cylinder is used */
	cyl = un->un_g.dkg_ncyl + un->un_g.dkg_acyl - 2;
	spc = un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	head = un->un_g.dkg_nhead - 1;
	blk = (cyl * (spc - un->un_g.dkg_apc)) +
	    (head * un->un_g.dkg_nsect) + 1;

	dkdevid = kmem_alloc(DEV_BSIZE, KM_SLEEP);

	bzero(&ucmd, sizeof (ucmd));
	bzero(&cdb, sizeof (cdb));
	ucmd.uscsi_flags = USCSI_READ;
	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_bufaddr = (caddr_t)dkdevid;
	ucmd.uscsi_buflen = DEV_BSIZE;
	ucmd.uscsi_flags |= USCSI_SILENT;

	dev = makedevice(ddi_name_to_major(ddi_get_name(SD_DEVINFO)),
	    ddi_get_instance(SD_DEVINFO) << SDUNIT_SHIFT);
	/*
	 * Read and verify device id, stored in the
	 * reserved cylinders at the end of the disk.
	 * Backup label is on the odd sectors of the last track
	 * of the last cylinder.
	 * Device id will be on track of the next to last cylinder.
	 */
	if ((blk >= (2 << 20)) || SD_GRP1_2_CDBS(un)) {
		cdb.scc_cmd = SCMD_READ_G1;
		FORMG1ADDR(&cdb, blk);
		FORMG1COUNT(&cdb, 1);
		ucmd.uscsi_cdblen = CDB_GROUP1;
	} else {
		cdb.scc_cmd = SCMD_READ;
		FORMG0ADDR(&cdb, blk);
		FORMG0COUNT(&cdb, (uchar_t)1);
		ucmd.uscsi_cdblen = CDB_GROUP0;
	}

	/* Read the reserved sector */
	mutex_exit(SD_MUTEX);
	status = sdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE,
	    UIO_SYSSPACE);
	mutex_enter(SD_MUTEX);
	if (status != 0) {
		kmem_free(dkdevid, DEV_BSIZE);
		if (ucmd.uscsi_status == STATUS_RESERVATION_CONFLICT)
		    status = EACCES;
		return (status);
	}

	/* Validate the revision */
	if ((dkdevid->dkd_rev_hi != DK_DEVID_REV_MSB) ||
	    (dkdevid->dkd_rev_lo != DK_DEVID_REV_LSB)) {
		kmem_free(dkdevid, DEV_BSIZE);
		return (EINVAL);
	}

	/* Calculate the checksum */
	chksum = 0;
	ip = (uint_t *)dkdevid;
	for (i = 0; i < ((DEV_BSIZE - sizeof (int))/sizeof (int)); i++)
		chksum ^= sd_stoh_int((uchar_t *)&ip[i]);

	/* Compare the checksums */
	if (DKD_GETCHKSUM(dkdevid) != chksum) {
		kmem_free(dkdevid, DEV_BSIZE);
		return (EINVAL);
	}

	/* Validate the device id */
	if (ddi_devid_valid((ddi_devid_t)&dkdevid->dkd_devid)
	    != DDI_SUCCESS) {
		kmem_free(dkdevid, DEV_BSIZE);
		return (EINVAL);
	}

	/* return a copy of the device id */
	sz = ddi_devid_sizeof((ddi_devid_t)&dkdevid->dkd_devid);
	un->un_devid = kmem_alloc(sz, KM_SLEEP);
	bcopy(&dkdevid->dkd_devid, un->un_devid, sz);
	kmem_free(dkdevid, DEV_BSIZE);

	return (0);
}

/* Return 0 if we successfully got it */
static int
sd_get_devid(struct scsi_disk *un)
{
	if (un->un_devid != NULL)
		return (0);

	return (sd_read_deviceid(un));
}

static ddi_devid_t
sd_create_devid(struct scsi_disk *un)
{
	if (ddi_devid_init(SD_DEVINFO, DEVID_FAB, 0, NULL, &un->un_devid)
	    == DDI_FAILURE)
		return (NULL);

	if (sd_write_deviceid(un)) {
		ddi_devid_free(un->un_devid);
		un->un_devid = NULL;
		return (NULL);
	}

	return (un->un_devid);
}

#define	SERIAL_NUM_PAGE_CODE	0x80
#define	EVPD_BIT					0x01
/* Maximum size for the KSTAT_DATA_CHAR value in kstat */
#define	KSTAT_DATA_CHAR_LEN	16

/*
 * Get Unit Serial number if supported.
 */
static int
sd_get_serial_num(struct scsi_disk *un, caddr_t serial_num_buf,
	uint_t *serial_num_len)
{
	register struct scsi_pkt *pkt;
	auto caddr_t wrkbuf;
	int	rval = -1;
	int	valid = 0;
	int	i;
	struct buf *bp;

	/*
	 * Get a work packet to play with. Get one with a buffer big
	 * enough for getting the Unit Serial number page (16 bytes for
	 * the serial number) and get a cdb big enough for Inquiry command.
	 */
	mutex_exit(SD_MUTEX);
	bp = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
	    KSTAT_DATA_CHAR_LEN	+ 4, B_READ, SLEEP_FUNC, NULL);
	if (!bp) {
		mutex_enter(SD_MUTEX);
		return (rval);
	}
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
	    bp, CDB_GROUP0, un->un_cmd_stat_size, PP_LEN,
	    PKT_CONSISTENT, SLEEP_FUNC, NULL);
	if (!pkt) {
		scsi_free_consistent_buf(bp);
		mutex_enter(SD_MUTEX);
		return (rval);
	}
	mutex_enter(SD_MUTEX);

	*((caddr_t *)&wrkbuf) = bp->b_un.b_addr;

	/*
	 * Send an inquiry for unit serial number page
	 *
	 * If we fail on this, we don't care presently what
	 * precisely is wrong.
	 */
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp, SCMD_INQUIRY,
		0, KSTAT_DATA_CHAR_LEN + 4, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	pkt->pkt_cdbp[1] |= EVPD_BIT;
	pkt->pkt_cdbp[2] = SERIAL_NUM_PAGE_CODE;

	if ((sd_scsi_poll(un, pkt) == 0) &&
	    (pkt->pkt_reason == CMD_CMPLT) &&
	    (SCBP_C(pkt) == 0)) {
		/*
		 * Limit the serial number length to KSTAT_DATA_CHAR_LEN.
		 */
		*serial_num_len = (wrkbuf[3] > KSTAT_DATA_CHAR_LEN) ?
			KSTAT_DATA_CHAR_LEN : wrkbuf[3];

		/*
		 * Device returns ASCII space (20h) in all the bytes of
		 * successful data transfer, if the product serial number
		 * is not available.
		 */
		for (i = 0; i < *serial_num_len; i++) {
			if (wrkbuf[4+ i] != ' ') {
				valid = 1;
				break;
			}
		}

		if (valid) {
			bcopy(&wrkbuf[4], serial_num_buf, *serial_num_len);
			rval = 0;
		}
	}

	scsi_destroy_pkt(pkt);
	scsi_free_consistent_buf(bp);
	return (rval);
}

/*ARGSUSED*/
static int
sdattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	register struct scsi_device *devp;
	struct driver_minor_data *dmdp;
	struct scsi_disk *un;
	char *node_type;
	char name[48];
	int rval;
	struct diskhd *dp;
	uchar_t   id[sizeof (SD_INQUIRY->inq_vid) + KSTAT_DATA_CHAR_LEN];
	uint_t		serial_num_len;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	devp = (struct scsi_device *)ddi_get_driver_private(devi);
	instance = ddi_get_instance(devi);

	switch (cmd) {
		case DDI_ATTACH:
			break;

		case DDI_PM_RESUME:
			if (!(un = ddi_get_soft_state(sd_state, instance)))
				return (DDI_FAILURE);
			mutex_enter(SD_MUTEX);
			Restore_state(un);
			cv_broadcast(&un->un_suspend_cv);
			/* restart I/O */
			dp = &un->un_utab;
			if (dp->b_actf && (dp->b_forw == NULL)) {
				sdstart(un);
			}
			mutex_exit(SD_MUTEX);
			return (DDI_SUCCESS);

		case DDI_RESUME:
			if (!(un = ddi_get_soft_state(sd_state, instance)))
				return (DDI_FAILURE);
			mutex_enter(SD_MUTEX);

			/* are we pm_suspended? */
			if (un->un_state == SD_STATE_PM_SUSPENDED) {
				mutex_exit(SD_MUTEX);
				return (DDI_SUCCESS);
			}
			Restore_state(un);
			cv_broadcast(&un->un_suspend_cv);
			if (un->un_state == SD_STATE_PM_SUSPENDED) {
				un->un_last_state = sd_save_state;
				mutex_exit(SD_MUTEX);
				return (DDI_SUCCESS);
			}
			un->un_throttle = un->un_save_throttle;
			cv_broadcast(&un->un_state_cv);
			/* restart thread */
			if (un->un_swr_token) {
				scsi_watch_resume(un->un_swr_token);
			}

			/*
			 * start unit - if this is a low-activity device
			 * commands in queue will have to wait until new
			 * commands come in, which may take awhile.
			 * Also, we specifically don't check un_ncmds
			 * because we know that there really are no
			 * commands in progress after the unit was suspended
			 * and we could have reached the throttle level, been
			 * suspended, and have no new commands coming in for
			 * awhile.  Highly unlikely, but so is the low-
			 * activity disk scenario.
			 */
			dp = &un->un_utab;
			if (dp->b_actf && (dp->b_forw == NULL)) {
				sdstart(un);
			}

			mutex_exit(SD_MUTEX);
			return (DDI_SUCCESS);

		default:
			return (DDI_FAILURE);
	}

	if ((rval = sd_doattach(devi, SLEEP_FUNC)) == DDI_FAILURE) {
		return (DDI_FAILURE);
	}

	if (!(un = (struct scsi_disk *)
	    ddi_get_soft_state(sd_state, instance))) {
		return (DDI_FAILURE);
	}
	devp->sd_private = (opaque_t)un;

	for (dmdp = sd_minor_data; dmdp->name != NULL; dmdp++) {
		switch (un->un_ctype) {
		case CTYPE_CDROM:
			node_type = DDI_NT_CD_CHAN;
			break;
		default:
			node_type = DDI_NT_BLOCK_CHAN;
			break;
		}
		(void) sprintf(name, "%s", dmdp->name);
		if (ddi_create_minor_node(devi, name, dmdp->type,
		    (instance << SDUNIT_SHIFT) | dmdp->minor,
		    node_type, NULL) == DDI_FAILURE) {
			/*
			 * Free Resouces allocated in sd_doattach
			 */
			sd_free_softstate(un, devi);
			return (DDI_FAILURE);
		}
	}

	/*
	 * Add a zero-length attribute to tell the world we support
	 * kernel ioctls (for layered drivers)
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    DDI_KERNEL_IOCTL, NULL, 0);

	/*
	 * Initialize power management bookkeeping; components are
	 * created idle.
	 */
	if (pm_create_components(devi, 1) == DDI_SUCCESS) {
		pm_set_normal_power(devi, 0, 1);
		un->un_power_level = 1;
	} else {
		sd_free_softstate(un, devi);
		return (DDI_FAILURE);
	}

	/*
	 * Since the sd device does not have the 'reg' property,
	 * cpr will not call its DDI_SUSPEND/DDI_RESUME entries.
	 * The following code is to tell cpr that this device
	 * does need to be suspended and resumed.
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "pm-hardware-state", (caddr_t)"needs-suspend-resume",
	    (int)strlen("needs-suspend-resume") + 1);

	ddi_report_dev(devi);

	un->un_gvalid = FALSE;

	/*
	 * Taking the SD_MUTEX isn't necessary in this context, but
	 * the devid functions release the SD_MUTEX. We take the
	 * SD_MUTEX here in order to ensure that the devid functions
	 * can function coherently in other contexts.
	 */
	mutex_enter(SD_MUTEX);

	if (ISREMOVABLE(un)) {
		un->un_mediastate = DKIO_NONE;
	} else {
		(void) sd_validate_geometry(un, NULL_FUNC);
	}

	if (!ISREMOVABLE(un) && !ISCD(un)) {
		if (un->un_options & SD_NOSERIAL) {
			/*
			 * Depending on EINVAL isn't reliable, since a
			 * reserved disk may result in invalid geometry,
			 * so check to make sure sd_doattach didn't
			 * return SD_EACCES.
			 */
			if ((sd_get_devid(un) == EINVAL) &&
			    (rval != SD_EACCES)) {
				(void) sd_create_devid(un);
			}
			if (un->un_devid)
			    (void) ddi_devid_register(SD_DEVINFO,
				    un->un_devid);
		} else {
			/* init & register devid */
			bzero(id, sizeof (id));
			bcopy(&SD_INQUIRY->inq_vid, id,
			    sizeof (SD_INQUIRY->inq_vid));
			if (sd_get_serial_num(un,
				(caddr_t)&un->un_serial_num_buf[0],
				&serial_num_len) == 0) {
				bcopy(un->un_serial_num_buf,
					&id[sizeof (SD_INQUIRY->inq_vid)],
					serial_num_len);
			} else {
				bcopy(&SD_INQUIRY->inq_serial,
					&id[sizeof (SD_INQUIRY->inq_vid)],
					sizeof (SD_INQUIRY->inq_serial));
				bcopy(&SD_INQUIRY->inq_serial,
					un->un_serial_num_buf,
					sizeof (SD_INQUIRY->inq_serial));
			}
			if (ddi_devid_init(SD_DEVINFO, DEVID_SCSI_SERIAL,
			    sizeof (id), (void *) &id, &un->un_devid)
			    == DDI_SUCCESS)
				(void) ddi_devid_register(SD_DEVINFO,
				    un->un_devid);
		}
	} else if (sd_get_serial_num(un, (caddr_t)&un->un_serial_num_buf[0],
		&serial_num_len) != 0) {
		bcopy(&SD_INQUIRY->inq_serial, un->un_serial_num_buf,
			sizeof (SD_INQUIRY->inq_serial));
	}
	mutex_exit(SD_MUTEX);


	/*
	 * set up kstats for Removable media here as sdopen() will fail
	 * if no CD in drive, and consequently no kstats will
	 * be created
	 */
	if (ISREMOVABLE(un)) {
		un->un_stats = kstat_create("sd", instance,
		    NULL, "disk", KSTAT_TYPE_IO, 1, KSTAT_FLAG_PERSISTENT);
		if (un->un_stats) {
			un->un_stats->ks_lock = SD_MUTEX;
			kstat_install(un->un_stats);
		}
		(void) sd_create_errstats(un, instance);
	}

	/*
	 * This property is set to 0 by HA software to avoid retries on a
	 * reserved disk. The first one is the preferred name (1189689)
	 */
	sd_retry_on_reservation_conflict = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "retry-on-reservation-conflict",
		sd_retry_on_reservation_conflict);
	if (sd_retry_on_reservation_conflict != 0) {
	    sd_retry_on_reservation_conflict = ddi_getprop(DDI_DEV_T_ANY, devi,
		DDI_PROP_DONTPASS, "sd_retry_on_reservation_conflict",
		sd_retry_on_reservation_conflict);
	}

	/* get "allow bus device reset" property - default to 1 */
	un->un_allow_bus_device_reset = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "allow-bus-device-reset", 1);

	/*
	 * checking max xfer size is done in sd_doattach (only enabled
	 * for wide/TQ drives)
	 *
	 * qfull handling
	 */
	if ((rval = ddi_getprop(DDI_DEV_T_ANY, devi, 0,
		"qfull-retries", -1)) != -1) {
		(void) scsi_ifsetcap(ROUTE, "qfull-retries", rval, 1);
	}
	if ((rval = ddi_getprop(DDI_DEV_T_ANY, devi, 0,
		"qfull-retry-interval", -1)) != -1) {
		(void) scsi_ifsetcap(ROUTE, "qfull-retry-interval", rval, 1);
	}

	/* Find out what type of reservation this disk supports */
	sd_check_pr(un);

	return (DDI_SUCCESS);
}



/*
 * Look properties up from the config file.
 *
 * The config file has a sd-config-list property which lists one or more
 * duplets.
 *
 * sd-config-list=<duplet>[,<duplet>*];
 *
 * <duplet>:= <vid+pid>,<data-property-name-list>
 *
 * <data-property-name-list>:=<data-property-name> [<data-property-name>]
 *
 *
 * The first entry of the duplet is a device_id string defined the same way
 * as in the sd_disk_table.
 *
 * The syntax of <data-property-name> depends on the <version> field.
 *
 * If version = SD_CONF_VERSION_1 we have the following syntax:
 *
 * 	<data-property-name>:=<version>,<flags>,<prop0>,<prop1>,.....<propN>
 *
 * where the prop0 value will be used to set prop0 if bit0 set in the
 * flags, prop1 if bit1 set, etc. and N = SD_CONF_MAX_ITEMS -1
 *
 * If version = SD_CONF_VERSION_10 we have the following syntax:
 *
 * 	<data-property-name>:=<version>,<prop0>,<prop1>,<prop2>,<prop3>
 *
 * The form of a config file entry is :
 *
 * sd-config-list=
 *	"SEAGATE ST32550W SUN2.1G0414","seagate-data",
 *	"SEAGATE ST31200W","seagate-data seagate-data-1",
 *	"CONNER  CFP1080E SUN1.05","conner-data";
 *
 * seagate-data= 1,0x1,63,0,0;
 * seagate-data-1 = 10,5,6,0,70;
 * conner-data= 1,0x1,32,0,0;
 *
 */
static int
sd_get_sdconf_table(struct scsi_disk *un, int *showstopper)
{
	caddr_t config_list;
	int config_list_len;
	int len;
	int dupletlen = 0;
	caddr_t vidptr;
	int vidlen;
	caddr_t dnlist_ptr, dataname_ptr;
	int dnlist_len, dataname_len;
	int *data_list;
	int data_list_len;
	int rval = 0;
	int i;

	if (ddi_getlongprop(DDI_DEV_T_ANY, SD_DEVINFO, DDI_PROP_DONTPASS,
	    "sd-config-list", (caddr_t)&config_list, &config_list_len)
	    != DDI_PROP_SUCCESS) {
		return (rval);
	}

	/*
	 * Compare vids in each duplet - if it matches, get value for
	 * disk_config struct
	 * dupletlen is not set yet!
	 */
	for (len = config_list_len, vidptr = config_list; len > 0;
	    vidptr += dupletlen, len -= dupletlen) {

		vidlen = (int)strlen(vidptr);
		dnlist_ptr = vidptr + vidlen + 1;
		dnlist_len = (int)strlen(dnlist_ptr);
		dupletlen = vidlen + dnlist_len + 2;

		if (vidlen == 0)
			continue;

		/*
		 * Check two cases: the whole vid/pid/rev string is
		 * checked until a NULL is found in the user-provided
		 * data, or, if the first character is a "*" then check
		 * the rest of the user-provided string for a match
		 * anywhere in the inquiry pid data.
		 */
		if (bcmp(SD_INQUIRY->inq_vid, vidptr, vidlen) != 0) {

			int found = 0;

			if ((vidptr[0] == '*') &&
			    (vidptr[vidlen-1] == '*')) {

				int i;
				char *pidptr = &vidptr[1];
				int pidstrlen = vidlen - 2;
				int looplimit = sizeof (SD_INQUIRY->inq_pid) -
					pidstrlen;

				if (looplimit < 0) {
					scsi_log(SD_DEVINFO,
					    sd_label, CE_WARN,
					    "Ignoring ID string %s in "
					    "sd.conf as too long", vidptr);
					continue;
				}

				for (i = 0; i < looplimit; i++) {
					if (bcmp(&SD_INQUIRY->inq_pid[i],
					    pidptr, pidstrlen) == 0) {
						found = 1;
						break;
					}
				}
			}

			/*
			 * If the id string starts and ends with a blank,
			 * treat multiple consecutive blanks as equivalent
			 * to a single blank. For example, this causes a
			 * sd_disk_table entry of " NEC CDROM " to match a
			 * device's id string of "NEC       CDROM".
			 *
			 * Also, sd_blank_cmp() includes the inq_vid
			 * (vendor ID) as part of the string to match.
			 */
			if (!found && (vidptr[0] == ' ') &&
			    (vidptr[vidlen-1] == ' '))
				found = sd_blank_cmp(un, vidptr);

			if (!found)
				/*
				 * Go on to the next duplet.
				 */
				continue;
		}

		/* dnlist contains 1 or more blank separated names */
		dataname_ptr = dnlist_ptr;

		dataname_len = 0;

		while (dataname_len < dnlist_len) {
			for (i = 0; dataname_ptr[i] != ' ' &&
			    dataname_ptr[i] != 0; i++);
			dataname_len += i;
			if (dataname_ptr[i] == ' ') {
			    dataname_ptr[i] = 0;
			    dataname_len++;
			}

			SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"sd_get_sdconf_table: disk:%s, dataname:%s\n",
			vidptr, dataname_ptr);

			/*
			 * get the data list
			 */
			if (ddi_getlongprop(DDI_DEV_T_ANY,
			    SD_DEVINFO, 0,
			    dataname_ptr, (caddr_t)&data_list,
			    &data_list_len) != DDI_PROP_SUCCESS) {
				/*
				 * Error in getting property value
				 * print warning!
				 */
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    "data property (%s) has no value\n",
				    dataname_ptr);
			} else {
				switch (*data_list) {
				case SD_CONF_VERSION_1:
					sd_read_vers1_params(un, data_list,
						data_list_len, dataname_ptr,
						showstopper);
					break;
				case SD_CONF_VERSION_10:
					sd_read_vers10_params(un, data_list,
					    data_list_len, dataname_ptr);
					break;
				default:
					scsi_log(SD_DEVINFO,
					    sd_label, CE_WARN,
					    " data property %s list"
					    " version is incorrect.",
					    dataname_ptr);
					break;
				}
				kmem_free(data_list, data_list_len);
			}

			dataname_ptr += (i + 1);
		}
	}

	/*
	 * free up the memory allocated by ddi_getlongprop
	 */
	kmem_free(config_list, config_list_len);

	return (rval);
}

void
sd_read_vers1_params(struct scsi_disk *un, int *data_list, int data_list_len,
    caddr_t dataname_ptr, int *showstopper)
{
	int data_list_long;

	/*
	 * First verify the length of the list.  The first two
	 * words are the version and the flags.  The rest of the
	 * list are the items to configure.  If there aren't at
	 * least three items to configure, reject it because it
	 * never would've worked with any version of sd.
	 */
	data_list_long = (int)(data_list_len / sizeof (int));
	if ((data_list_long - 2) < SD_CONF_MIN_ITEMS) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    " data property list %s"
		    " size is incorrect.  No props set.", dataname_ptr);
		scsi_log(SD_DEVINFO, sd_label, CE_CONT,
		    " Size expected: version + 1 flagword + %d props.\n",
		    SD_CONF_MAX_ITEMS);
		return;
	}

	/*
	 * Display a warning if undefined bits are set in the flags.
	 */
	if (data_list[1] >> (data_list_long - 2)) {
		int invalid_bits = (data_list[1] >> (data_list_long - 2))
			<< (data_list_long - 2);

		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    " ignoring bits 0x%x in data property list %s",
		    data_list[1] & invalid_bits,
		    dataname_ptr);
		data_list[1] &= ~invalid_bits;
	}

	*showstopper = sd_set_dev_properties(un, data_list[1], &data_list[2]);
}

void
sd_read_vers10_params(struct scsi_disk *un, int *data_list, int data_list_len,
    caddr_t dataname_ptr)
{

	if ((data_list_len / sizeof (int)) != SD_CONF_TMOUT_ITEMS) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    "data property list %s"
		    "size incorrect. %d\n",
		    dataname_ptr, data_list_len / sizeof (int));
		return;
	}
	if (data_list[1] > 0) {
		un->un_notrdy_delay = (ushort_t) SD_USECTOHZ(data_list[1]);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_read_vers10_params: un_notrdy_delay set to %d\n",
		    data_list[1]);
	}
	if (data_list[2] > 0) {
		un->un_bsy_delay = (ushort_t) SD_USECTOHZ(data_list[2]);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_read_vers10_params: un_bsy_delay set to %d\n",
		    data_list[2]);
	}
	if (data_list[3] >= 0) {
		un->un_err_delay = (ushort_t) SD_USECTOHZ(data_list[3]);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_read_vers10_params: un_err_delay set to %d\n",
		    data_list[3]);
	}
	if (data_list[4] > 0) {
		if (data_list[4] < 10) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "data property list %s "
			    "cmd_timeout value incorrect: %d < 10\n",
			    dataname_ptr, data_list[4]);
		} else {
			un->un_cmd_timeout = (ushort_t) data_list[4];
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"sd_read_vers10_params: un_cmd_timeout set to %d\n",
			un->un_cmd_timeout);
		}
	}
}

/*
 * Set device properties based on a property list.
 *
 * Size of the property list is assumed correct.
 * Return value is nonzero if there was trouble setting stuff up, and it is a
 * showstopper.
 */

static int
sd_set_dev_properties(struct scsi_disk *un, int flags, int *prop_list)
{
	int rval = 0;

	/*
	 * Match the values with the properties.
	 *
	 * Bits of first value read correspond to variables to
	 * be set with remaining values, bit 0 set corresponds
	 * to setting proper variable with value in
	 * prop_list[0], bit 1: prop_list[1], etc.
	 *
	 */

	/*
	 * Caveats of setting throttle:
	 *
	 * 1) If tagged queueing property not enabled on HBA, this value will
	 * be overridden and set down in sd_doattach to 1 or 3.
	 *
	 * 2) max transfers are enabled only when throttle is unchanged from
	 * default sd_max_throttle, to accomodate eliteII drives which have
	 * smaller throttle of 10, and max transfers disabled.
	 */

	if ((flags & SD_CONF_BSET_THROTTLE) && (prop_list != NULL)) {
		if (un->un_throttle > prop_list[SD_CONF_SET_THROTTLE]) {
			un->un_save_throttle = un->un_throttle =
			    prop_list[SD_CONF_SET_THROTTLE];
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: throttle set to %d\n",
			    prop_list[SD_CONF_SET_THROTTLE]);
		}
	}

	if ((flags & SD_CONF_BSET_CTYPE) && (prop_list != NULL)) {
		switch (prop_list[SD_CONF_SET_CTYPE]) {
			case CTYPE_CDROM:
			case CTYPE_MD21:
			case CTYPE_CCS:
			case CTYPE_ROD:
			case CTYPE_PXRE:
				un->un_ctype = prop_list[SD_CONF_SET_CTYPE];
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				    "sd_set_dev_properties: ctype set to %d\n",
				    prop_list[SD_CONF_SET_CTYPE]);
				break;
			default:
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    "Could not set invalid ctype value (%d)\n",
				    prop_list[SD_CONF_SET_CTYPE]);
		}
	}

	if ((flags & SD_CONF_BSET_NOSERIAL) && (prop_list != NULL)) {
		un->un_options |= SD_NOSERIAL;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_set_dev_properties: noserial bit set\n");
	}

	if (flags & SD_CONF_BSET_NOCACHE) {
		if (sd_disable_caching(un)) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "Could not disable caching for device\n");
			rval = 1;
		}
	}

	if (flags & SD_CONF_BSET_PLAYMSF_BCD) {
		un->un_config_flags |= SDF_PLAYMSF_BCD;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: playmsf_bcd set\n");
	}
	if (flags & SD_CONF_BSET_READSUB_BCD) {
		un->un_config_flags |= SDF_READSUB_BCD;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: readsub_bcd set\n");
	}
	if (flags & SD_CONF_BSET_READ_TOC_TRK_BCD) {
		un->un_config_flags |= SDF_READ_TOC_TRK_BCD;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: read_toc_trk_bcd set\n");
	}
	if (flags & SD_CONF_BSET_READ_TOC_ADDR_BCD) {
		un->un_config_flags |= SDF_READ_TOC_ADDR_BCD;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: read_toc_addr_bcd set\n");
	}
	if (flags & SD_CONF_BSET_NO_READ_HEADER) {
		un->un_config_flags |= SDF_NO_READ_HEADER;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: no_read_header set\n");
	}
	if (flags & SD_CONF_BSET_READ_CD_XD4) {
		un->un_config_flags |= SDF_READ_CD_XD4;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "sd_set_dev_properties: read_cd_xd4 set\n");
	}
	if ((flags & SD_CONF_BSET_NRR_COUNT) && (prop_list != NULL)) {
		if (prop_list[SD_CONF_SET_NRR_COUNT]) {
			un->un_notready_retry_count =
				prop_list[SD_CONF_SET_NRR_COUNT];
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"not ready retry count set to %x \n",
				un->un_notready_retry_count);
		}
	}
	return (rval);
}


static void
sd_free_softstate(struct scsi_disk *un, dev_info_t *devi)
{
	struct	scsi_device		*devp;
	int	instance = ddi_get_instance(devi);
	int	i;

	devp = (struct scsi_device *)ddi_get_driver_private(devi);

	if (un) {
		sema_destroy(&un->un_semoclose);
		sema_destroy(&un->un_rqs_sema);
		cv_destroy(&un->un_sbuf_cv);
		cv_destroy(&un->un_state_cv);
		cv_destroy(&un->un_disk_busy_cv);
		cv_destroy(&un->un_suspend_cv);

		/*
		 * Deallocate command packet resources.
		 */
		if (un->un_sbufp)
			freerbuf(un->un_sbufp);
		if (un->un_rqs)
			scsi_destroy_pkt(un->un_rqs);
		if (un->un_rqs_bp)
			scsi_free_consistent_buf(un->un_rqs_bp);
		if (un->un_uscsi_rqs_buf) {
			kmem_free(un->un_uscsi_rqs_buf, SENSE_LENGTH);
		}

		/*
		 * Delete kstats. Kstats for non REMOVABLE devices are deleted
		 * in sdclose, but we delete the kstats if for some reason
		 * the kstats are not already deleted when we are detaching.
		 */
		if (un->un_stats) {
			kstat_delete(un->un_stats);
			un->un_stats = (kstat_t *)0;
		}
		if (un->un_errstats) {
			kstat_delete(un->un_errstats);
			un->un_errstats = (kstat_t *)0;
		}
		if (!ISREMOVABLE(un)) {
			for (i = 0; i < NDKMAP; i++) {
				if (un->un_pstats[i]) {
					kstat_delete(un->un_pstats[i]);
				}
			}
		}

		/* unregister and free device id */
		ddi_devid_unregister(devi);
		if (un->un_devid) {
			ddi_devid_free(un->un_devid);
			un->un_devid = NULL;
		}
	}

	/*
	 * Cleanup scsi_device resources.
	 */
	ddi_soft_state_free(sd_state, instance);
	devp->sd_private = (opaque_t)0;
	devp->sd_sense = (struct scsi_extended_sense *)0;
	/* unprobe scsi device */
	scsi_unprobe(devp);

	/* Remove properties created during attach */
	ddi_prop_remove_all(devi);
	ddi_remove_minor_node(devi, NULL);
}

static int
sddetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct scsi_disk *un;
	register struct diskhd *dp;
	clock_t wait_cmds_complete;

	instance = ddi_get_instance(devi);

	if (!(un = ddi_get_soft_state(sd_state, instance)))
		return (DDI_FAILURE);
	dp = &un->un_utab;

	switch (cmd) {
		case DDI_DETACH:
			return (sd_dr_detach(devi));

		case DDI_SUSPEND:
			mutex_enter(SD_MUTEX);
			if (un->un_state == SD_STATE_SUSPENDED) {
				mutex_exit(SD_MUTEX);
				return (DDI_SUCCESS);
			}
			/*
			 * If the un_state is PM_SUSPENDED, then we
			 * need to save last state in order to restore
			 * un_last_state in DDI_PM_RESUME
			 */
			if (un->un_state == SD_STATE_PM_SUSPENDED) {
				sd_save_state = un->un_last_state;
				New_state(un, SD_STATE_SUSPENDED);
				mutex_exit(SD_MUTEX);
				return (DDI_SUCCESS);
			}
			/*
			 * If the device is being used by HA, fail
			 * the DDI_SUSPEND
			 */
			if ((un->un_resvd_status & SD_RESERVE) ||
			    (un->un_resvd_status & SD_WANT_RESERVE) ||
			    (un->un_resvd_status & SD_LOST_RESERVE)) {
				mutex_exit(SD_MUTEX);
				return (DDI_FAILURE);
			}
			/*
			 * If we are in resource wait state, fail the
			 * DDI_SUSPEND
			 */
			if (un->un_state == SD_STATE_RWAIT) {
				SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
				    "sddetach: SUSPEND failed due to "
				    "resource wait state\n");
				mutex_exit(SD_MUTEX);
				return (DDI_FAILURE);
			}
			New_state(un, SD_STATE_SUSPENDED);
			/*
			 * wait till current operation completed. If we are
			 * in the resource wait state (with an intr outstanding)
			 * then we need to wait till the intr completes and
			 * starts the next cmd. We wait for SD_WAIT_CMDS_
			 * COMPLETE seconds before failing the DDI_SUSPEND.
			 */
			wait_cmds_complete = ddi_get_lbolt();
			wait_cmds_complete +=
			    sd_wait_cmds_complete * drv_usectohz(1000000);
			while (un->un_ncmds || dp->b_forw) {
				if (cv_timedwait(&un->un_disk_busy_cv, SD_MUTEX,
				    wait_cmds_complete) == -1) {
					/*
					 * Commands Didn't finish in the
					 * specified time, fail the DDI_SUSPEND.
					 */
					SD_DEBUG2(SD_DEVINFO, sd_label,
					    SCSI_DEBUG, "sddetach: SUSPEND "
					    "failed due to outstanding cmds\n");
					Restore_state(un);
					mutex_exit(SD_MUTEX);
					return (DDI_FAILURE);
				}
			}
			/* cancel swr thread */
			if (un->un_swr_token) {
				opaque_t temp_token = un->un_swr_token;
				mutex_exit(SD_MUTEX);
				scsi_watch_suspend(temp_token);
				mutex_enter(SD_MUTEX);
			}
			/* Cancel timeouts */
			if (un->un_reset_throttle_timeid) {
				timeout_id_t temp_id =
				    un->un_reset_throttle_timeid;
				un->un_reset_throttle_timeid = 0;
				mutex_exit(SD_MUTEX);
				(void) untimeout(temp_id);
				mutex_enter(SD_MUTEX);
			}
			if (un->un_dcvb_timeid) {
				timeout_id_t temp_id = un->un_dcvb_timeid;
				un->un_dcvb_timeid = 0;
				mutex_exit(SD_MUTEX);
				(void) untimeout(temp_id);
				mutex_enter(SD_MUTEX);
			}
			if (un->un_reissued_timeid) {
				timeout_id_t temp_id =
				    un->un_reissued_timeid;
				un->un_reissued_timeid = 0;
				mutex_exit(SD_MUTEX);
				(void) untimeout(temp_id);
				mutex_enter(SD_MUTEX);
			}
			mutex_exit(SD_MUTEX);
			return (DDI_SUCCESS);

		case DDI_PM_SUSPEND:

			mutex_enter(SD_MUTEX);
			if (un->un_state == SD_STATE_PM_SUSPENDED) {
				mutex_exit(SD_MUTEX);
				return (DDI_SUCCESS);
			}
			/*
			 * If the device is being used by HA, fail
			 * the DDI_PM_SUSPEND
			 */
			if ((un->un_resvd_status & SD_RESERVE) ||
			    (un->un_resvd_status & SD_WANT_RESERVE) ||
			    (un->un_resvd_status & SD_LOST_RESERVE)) {
				mutex_exit(SD_MUTEX);
				return (DDI_FAILURE);
			}

			/* if the device is busy then we fail the PM_SUSPEND */
			if (dp->b_forw || dp->b_actf || un->un_ncmds ||
				(un->un_state == SD_STATE_RWAIT)) {
				mutex_exit(SD_MUTEX);
				return (DDI_FAILURE);
			}
			New_state(un, SD_STATE_PM_SUSPENDED);
			mutex_exit(SD_MUTEX);
			return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
sd_dr_detach(dev_info_t *devi)
{
	struct scsi_device		*devp;
	struct scsi_disk		*un;
	int				i;
	dev_t				dev;
#if !defined(__i386) && !defined(__ia64)
	int				reset_retval;
#endif

	/*
	 * Get scsi_device structure for this instance.
	 */
	if (!(devp = (struct scsi_device *)ddi_get_driver_private(devi))) {
		return (DDI_FAILURE);
	}

	/*
	 * Get scsi_disk structure containing target 'private' information
	 */
	un = (struct scsi_disk *)devp->sd_private;

	if (un->un_sbuf_busy) {
		return (DDI_FAILURE);
	}

	dev = makedevice(ddi_name_to_major(ddi_get_name(SD_DEVINFO)),
			ddi_get_instance(SD_DEVINFO) << SDUNIT_SHIFT);

	/*
	 * fail the detach if there are any outstanding layered
	 * opens on this device.
	 */

	LOCK_LINTED
	_NOTE(COMPETING_THREADS_NOW);
	mutex_enter(SD_MUTEX);

	for (i = 0; i < NDKMAP; i++) {
		if (un->un_ocmap.lyropen[i] != 0) {
			mutex_exit(SD_MUTEX);
			_NOTE(NO_COMPETING_THREADS_NOW);
			return (DDI_FAILURE);
		}
	}



	/*
	 * Verify there are NO outstanding commands issued to this device.
	 * ie, un_ncmds == 0.
	 * It's possible to have outstanding commands through the physio
	 * code path, even though everything's closed.
	 */

	if (un->un_ncmds || un->un_reissued_timeid ||
	    (un->un_state == SD_STATE_RWAIT)) {
		mutex_exit(SD_MUTEX);
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_dr_detach: Detach failure due to outstanding cmds\n");
		_NOTE(NO_COMPETING_THREADS_NOW);
		return (DDI_FAILURE);
	}


	/*
	 * Release reservation on the device only if reserved.
	 */
	if ((un->un_resvd_status & SD_RESERVE) &&
	    !(un->un_resvd_status & SD_LOST_RESERVE)) {
		mutex_exit(SD_MUTEX);
		if (sd_reserve_release(dev, SD_RELEASE) != 0) {
			SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_dr_detach: Cannot release reservation \n");
		}
	} else {
		mutex_exit(SD_MUTEX);
	}


	/*
	 * Untimeout any reserve recover, throttle reset, restart unit
	 * and delayed broadcast timeout threads.
	 * we have to untimeout without holding the mutex. Tell warlock
	 * to ignore this
	 */
	_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_resvd_timeid));
	_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_reset_throttle_timeid));
	_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_restart_timeid));
	_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_dcvb_timeid));

	if (un->un_resvd_timeid) {
		(void) untimeout(un->un_resvd_timeid);
	}
	if (un->un_reset_throttle_timeid) {
		(void) untimeout(un->un_reset_throttle_timeid);
	}
	if (un->un_restart_timeid) {
		(void) untimeout(un->un_restart_timeid);
	}
	if (un->un_dcvb_timeid) {
		(void) untimeout(un->un_dcvb_timeid);
	}

	/*
	 * Now destroy the timeout thread
	*/
	sd_timeout_destroy(dev);



	/*
	 * If active, CANCEL multi-host disk watch thread request
	 */
	mutex_enter(SD_MUTEX);
	if (un->un_mhd_token) {
		mutex_exit(SD_MUTEX);
		_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_mhd_token));
		if (scsi_watch_request_terminate(un->un_mhd_token,
				SCSI_WATCH_TERMINATE_NOWAIT)) {
			_NOTE(NO_COMPETING_THREADS_NOW);
			return (DDI_FAILURE);
		}
		mutex_enter(SD_MUTEX);
		un->un_mhd_token = (opaque_t)NULL;
	}
	if (un->un_swr_token) {
		mutex_exit(SD_MUTEX);
		_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_disk::un_swr_token));
		if (scsi_watch_request_terminate(un->un_swr_token,
				SCSI_WATCH_TERMINATE_NOWAIT)) {
			_NOTE(NO_COMPETING_THREADS_NOW);
			return (DDI_FAILURE);
		}
		mutex_enter(SD_MUTEX);
		un->un_swr_token = (opaque_t)NULL;
	}
	mutex_exit(SD_MUTEX);

	(void) scsi_ifsetcap(ROUTE, "auto-rqsense", 0, 1);

	/*
	 * Clear any scsi_reset_notifies. We clear the reset notifies
	 * if we have not registered one.
	 */
	(void) scsi_reset_notify(ROUTE, SCSI_RESET_CANCEL,
			sd_mhd_reset_notify_cb, (caddr_t)un);

#if defined(__i386) || defined(__ia64)
	/*
	 * gratuitous bus resets sometimes cause an otherwise
	 * okay ATA/ATAPI bus to hang. This is due the lack of
	 * a clear spec of how resets should be implemented by ATA
	 * disk drives.
	 */
#else
	/*
	 * Reset target/bus.  This is a temporary workaround for Elite III
	 * dual-port drives that will not come online after an aborted
	 * detach and subsequent re-attach.  It should be removed when the
	 * Elite III FW is fixed, or the drives are no longer supported.
	 */

	reset_retval = 0;
	if (un->un_allow_bus_device_reset) {
		reset_retval = scsi_reset(ROUTE, RESET_TARGET);
	}

	if (reset_retval == 0) {
		(void) scsi_reset(ROUTE, RESET_ALL);
	}
#endif

	_NOTE(NO_COMPETING_THREADS_NOW);

	/*
	 * at this point there are no competing threads anymore
	 * release active MT locks and all device resources.
	 */
	sd_free_softstate(un, devi);

	return (DDI_SUCCESS);
}

static int
sdpower(dev_info_t *devi, int component, int level)
{
	struct scsi_pkt *pkt;
	struct scsi_disk *un;
	int		instance;
	int		rval = DDI_SUCCESS;

	instance = ddi_get_instance(devi);

	if (!(un = ddi_get_soft_state(sd_state, instance)) ||
	    (0 > level) || (level > 1) || component != 0)
		return (DDI_FAILURE);

	mutex_enter(SD_MUTEX);
	if (level == un->un_power_level) {
		/* already at this power level */
		mutex_exit(SD_MUTEX);
		return (DDI_SUCCESS);
	}
	mutex_exit(SD_MUTEX);

	if (!ISREMOVABLE(un) && !ISPXRE(un)) {
		pkt = scsi_init_pkt(ROUTE, NULL, NULL, CDB_GROUP0,
			(un->un_arq_enabled ?
			sizeof (struct scsi_arq_status) : 1),
			0, 0, NULL_FUNC, NULL);

		if (pkt == (struct scsi_pkt *)NULL) {
			return (DDI_FAILURE);
		}
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_START_STOP, 0, (level ? SD_START : SD_STOP), 0);
		FILL_SCSI1_LUN(un->un_sd, pkt);

		rval = sd_scsi_poll(un, pkt);
		if (rval || SCBP_C(pkt) != STATUS_GOOD) {
			/*
			 * If the first command gets a check condition and
			 * returns an error, we do it again. The check condition
			 * results from a SCSI bus reset or power down
			 */
			if (SCBP(pkt)->sts_chk) {
				if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
					(void) sd_clear_cont_alleg(un,
						un->un_rqs);
				}
			}
			rval = sd_scsi_poll(un, pkt);
		}
		scsi_destroy_pkt(pkt);
	}
	if (!rval) {
		mutex_enter(SD_MUTEX);
		un->un_power_level = level;
		mutex_exit(SD_MUTEX);
	}
	return (rval);
}

static int
sd_doattach(dev_info_t *devi, int (*canwait)())
{
	struct scsi_device *devp;
	auto struct scsi_capacity cbuf;
	register struct scsi_pkt *rqpkt = (struct scsi_pkt *)0;
	register struct scsi_disk *un = (struct scsi_disk *)0;
	int instance;
	int km_flags = (canwait != NULL_FUNC)? KM_SLEEP : KM_NOSLEEP;
	struct buf *bp;
	int showstopper = 0;
	int rval;
#if !defined(__i386) && !defined(__ia64)
	extern int maxphys;
#endif
	char *variantp;

	devp = (struct scsi_device *)ddi_get_driver_private(devi);

	/*
	 * Call the routine scsi_probe to do some of the dirty work.
	 * If the INQUIRY command succeeds, the field sd_inq in the
	 * device structure will be filled in. The sd_sense structure
	 * will also be allocated.
	 */

	switch (scsi_probe(devp, canwait)) {
	default:
		return (DDI_FAILURE);

	case SCSIPROBE_EXISTS:
		switch (devp->sd_inq->inq_dtype) {
		case DTYPE_DIRECT:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_DIRECT\n");
			break;
		case DTYPE_RODIRECT:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_RODIRECT\n");
			break;
		case DTYPE_OPTICAL:
			SD_DEBUG2(devp->sd_dev, sd_label, SCSI_DEBUG,
			    "sdprobe: DTYPE_OPTICAL\n");
			break;

		case DTYPE_NOTPRESENT:
		default:
			return (DDI_FAILURE);
		}
	}


	/*
	 * Allocate a request sense packet.
	 */
	bp = scsi_alloc_consistent_buf(&devp->sd_address, (struct buf *)NULL,
	    SENSE_LENGTH, B_READ, canwait, NULL);
	if (!bp) {
		return (DDI_FAILURE);
	}

	rqpkt = scsi_init_pkt(&devp->sd_address,
	    (struct scsi_pkt *)NULL, bp, CDB_GROUP0, 1, PP_LEN,
	    PKT_CONSISTENT, canwait, NULL);
	if (!rqpkt) {
		goto error;
	}
	devp->sd_sense = (struct scsi_extended_sense *)bp->b_un.b_addr;


	(void) scsi_setup_cdb((union scsi_cdb *)rqpkt->pkt_cdbp,
			SCMD_REQUEST_SENSE, 0, SENSE_LENGTH, 0);
	FILL_SCSI1_LUN(devp, rqpkt);

	/*
	 * The actual unit is present.
	 *
	 * The attach routine will check validity of label
	 * (and print out whether it is there).
	 *
	 * Now is the time to fill in the rest of our info..
	 */
	instance = ddi_get_instance(devp->sd_dev);

	if (ddi_soft_state_zalloc(sd_state, instance) != DDI_SUCCESS)
		goto error;

	un = ddi_get_soft_state(sd_state, instance);

	un->un_sbufp = getrbuf(km_flags);
	if (un->un_sbufp == (struct buf *)NULL) {
		goto error;
	}

	un->un_uscsi_rqs_buf  = kmem_alloc(SENSE_LENGTH, KM_SLEEP);

	rqpkt->pkt_comp = sdintr;
	rqpkt->pkt_time = sd_io_time;

	/* SunBug 1222170 */
	rqpkt->pkt_flags |= (FLAG_SENSING | FLAG_HEAD);

	un->un_isusb = ddi_prop_get_int(DDI_DEV_T_ANY, devi, 0, "usb", -1);
	un->un_rqs = rqpkt;
	un->un_sd = devp;
	un->un_rqs_bp = bp;

	un->un_swr_token = (opaque_t)NULL;

	un->ebp_enabled = 0;
	un->un_rqs_state = 0;

	un->un_notrdy_delay = SD_USECTOHZ(DEFAULT_NOTRDY_DELAY);
	un->un_bsy_delay =    SD_USECTOHZ(DEFAULT_BSY_DELAY);
	un->un_err_delay =    SD_USECTOHZ(DEFAULT_ERR_DELAY);
	un->un_cmd_timeout =  DEFAULT_CMD_TIMEOUT;

	sema_init(&un->un_semoclose, 1, NULL, SEMA_DRIVER, NULL);
	sema_init(&un->un_rqs_sema, 1, NULL, SEMA_DRIVER, NULL);
	cv_init(&un->un_sbuf_cv, NULL, CV_DRIVER, NULL);
	cv_init(&un->un_state_cv, NULL, CV_DRIVER, NULL);

	/* Initialize power management conditional variable */
	cv_init(&un->un_disk_busy_cv, NULL, CV_DRIVER, NULL);
	cv_init(&un->un_suspend_cv, NULL, CV_DRIVER, NULL);

	/*
	 * Assume CCS drive, assume parity, but call
	 * it a CDROM if it is a RODIRECT device.
	 */

	switch (devp->sd_inq->inq_dtype) {
	case DTYPE_RODIRECT:
		un->un_ctype = CTYPE_CDROM;
		break;
	case DTYPE_OPTICAL:
		un->un_ctype = CTYPE_ROD;
		break;
	default:
		un->un_ctype = CTYPE_CCS;
		break;
	}

	un->un_options = 0;

	/*
	 * Check if this is an ATAPI device. ATAPI devices use
	 * Group 1 Read/Write commands and Group 2 Mode Sense/Select
	 * commands. Additional flags may be set by sd_set_dev_properties()
	 */
	un->un_config_flags = 0;
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, devi, 0, "variant",
				    &variantp) == DDI_PROP_SUCCESS) {
		if (strcmp(variantp, "atapi") == 0)
			un->un_config_flags = SDF_IS_ATAPI | SDF_GRP1_2_CDBS |
					    SDF_READ_CD;
		ddi_prop_free(variantp);
	}

	/*
	 * enable autorequest sense; keep the rq packet around in case
	 * the autorequest sense fails because of a busy condition
	 */
	un->un_arq_enabled =
	    ((scsi_ifsetcap(ROUTE, "auto-rqsense", 1, 1) == 1)? 1: 0);

	un->un_cmd_stat_size =	(int)((un->un_arq_enabled ?
		    sizeof (struct scsi_arq_status) : 1));

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "auto request sense %s\n",
	    (un->un_arq_enabled ? "enabled" : "disabled"));

	un->un_throttle = un->un_save_throttle = sd_max_throttle;

	/*
	 * set the per disk retry count to the default no. of retries
	 * for disks and CDROMs. This value can be overridden by the
	 * disk property list or an entry in sd.conf.
	 */
	un->un_notready_retry_count = ISCD(un) ? CD_NOT_READY_RETRY_COUNT :
					DISK_NOT_READY_RETRY_COUNT;

	/*
	 * The following implements a property lookup mechanism.  Properties
	 * for particular disks (keyed on vendor, model and rev numbers) are
	 * sought in the sd.conf file via sd_get_sdconf_table(), and if
	 * not found there, are looked for in a list hardcoded in this driver.
	 *
	 * See comments near the sd_disk_table for info on lookups, props, etc.
	 */

	if (!sd_get_sdconf_table(un, &showstopper)) {
		int table_index;
		for (table_index = 0; table_index < sd_disk_table_size;
		    table_index++) {

			/*
			 * Set device properties on a match.
			 */

			/*
			 * An implicit assumption made here is that the
			 * scsi inquiry structure will always keep the
			 * vid, pid and revision strings in consecutive
			 * sequence, so they can be read as a single string.
			 * The bcmp and "found" loop below reads multiple
			 * strings, starting with vid and extending into
			 * the others.
			 *
			 * If this assumption is not the case, a separate
			 * string, to be used for the check, needs to be
			 * built with these strings concatenated.
			 */

			/*
			 * The first check below sets "found" true when
			 * the id string in the sd_disk_table starts and
			 * ends with a "*", and the rest of the string
			 * matches some portion of the inquiry pid string.
			 *
			 * The history behind this is to locate all
			 * disks containing "SUN" in their pid string,
			 * which do not have valid/unique serial numbers
			 * (those which have SD_CONF_BSET_NOSERIAL in the
			 * sd_disk_table).
			 */
			int found = 0;
			char *devstr = sd_disk_table[table_index].device_id;
			size_t devstrlen = strlen(devstr);

			if ((devstr[0] == '*') &&
			    (devstr[devstrlen-1] == '*')) {

				int i;
				size_t looplimit;

				/*
				 * Advance beyond first *.  Don't count last *.
				 */
				devstr += 1;
				devstrlen -= 2;
				looplimit =
				    sizeof (SD_INQUIRY->inq_pid) -
				    devstrlen;
				for (i = 0; i < looplimit; i++) {
					if (bcmp(&SD_INQUIRY->inq_pid[i],
					    devstr, devstrlen) == 0) {
						found = 1;
						break;
					}
				}
			}

			/*
			 * If the id string starts and ends with a blank,
			 * treat multiple consecutive blanks as equivalent
			 * to a single blank. For example, this causes a
			 * sd_disk_table entry of " NEC CDROM " to match a
			 * device's id string of "NEC       CDROM".
			 *
			 * Also, sd_blank_cmp() includes the inq_vid
			 * (vendor ID) as part of the string to match.
			 */
			if (!found && (devstr[0] == ' ') &&
			    (devstr[devstrlen-1] == ' '))
				found = sd_blank_cmp(un, devstr);

			if (found ||
			    ((bcmp(SD_INQUIRY->inq_vid,
			    sd_disk_table[table_index].device_id,
			    strlen(sd_disk_table[table_index].device_id)))
			    == 0)) {
				showstopper = sd_set_dev_properties(un,
				    sd_disk_table[table_index].flags,
				    sd_disk_table[table_index].properties);
				break;
			}
		}
	}

	if (showstopper) {
		goto error;
	}

	/*
	 * If the device is not a removable media device, make sure that
	 * it can be started (if possible) with the START command, and
	 * attempt to read it's capacity too (if possible).
	 */

	cbuf.capacity = (uint_t)-1;
	cbuf.lbasize = (uint_t)-1;
	rval = 0;
	if (devp->sd_inq->inq_dtype == DTYPE_DIRECT && !ISREMOVABLE(un)) {
		switch (sd_winchester_exists(un, rqpkt, canwait)) {
		case 0:
			/*
			 * try to read capacity, do not get upset if it fails.
			 */
			(void) sd_read_capacity(un, &cbuf);
			break;
		case SD_EACCES:	/* disk is reserved */
			rval = SD_EACCES;
			break;
		default:
			goto error;
		}
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "sd_doattach: drive selected\n");

	if (cbuf.capacity != ((uint_t)-1)) {
		un->un_capacity = cbuf.capacity;
		un->un_lbasize = cbuf.lbasize;
	} else if (!ISCD(un) && !ISROD(un)) {
#if defined(__i386) || defined(__ia64)
		un->un_capacity = -1;
		un->un_lbasize = -1;
#else
		un->un_capacity = 0;
		un->un_lbasize = DEV_BSIZE;
#endif
	} else if (ISCD(un) || ISROD(un)) {
		un->un_capacity = -1;
		un->un_lbasize = -1;
	}

	/*
	 * If SCSI-2 tagged queueing is supported by the disk drive and
	 * by the host adapter then we will enable it.
	 */
	un->un_tagflags = 0;
	if ((devp->sd_inq->inq_rdf == RDF_SCSI2) &&
	    (devp->sd_inq->inq_cmdque) && un->un_arq_enabled) {
		if (scsi_ifsetcap(ROUTE, "tagged-qing", 1, 1) == 1) {
			un->un_tagflags = FLAG_STAG;
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "tagged queueing enabled\n");
		} else if (scsi_ifgetcap(ROUTE, "untagged-qing", 0) == 1) {
			un->un_options |= SD_QUEUEING;
			un->un_save_throttle = un->un_throttle =
			    min(un->un_throttle, 3);
		} else {
			un->un_options &= ~SD_QUEUEING;
			un->un_save_throttle = un->un_throttle = 1;
		}
	/*
	 * Does the Host Adapter Support Internal Queueing?
	 */
	} else if ((scsi_ifgetcap(ROUTE, "untagged-qing", 0) == 1) &&
			un->un_arq_enabled) {
		un->un_options |= SD_QUEUEING;
		un->un_save_throttle = un->un_throttle =
		    min(un->un_throttle, 3);
	} else {
		un->un_options &= ~SD_QUEUEING;
		un->un_save_throttle = un->un_throttle = 1;
	}


	/*
	 * set default max_xfer_size
	 */

#if defined(__i386) || defined(__ia64)
	un->un_max_xfer_size = (uint_t)SD_DEFAULT_MAX_XFER_SIZE;
#else
	un->un_max_xfer_size = (uint_t)maxphys;
#endif

	/*
	 * Setup or tear down default wide operations for
	 * disks
	 */
	if ((devp->sd_inq->inq_rdf == RDF_SCSI2) &&
	    (devp->sd_inq->inq_wbus16 || devp->sd_inq->inq_wbus32)) {
		if (scsi_ifsetcap(ROUTE, "wide-xfer", 1, 1) == 1) {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"Wide Transfer enabled\n");
		}

		/*
		 * if tagged queuing has also been enabled, then
		 * enable large xfers
		 */
		if (un->un_save_throttle == sd_max_throttle) {
			un->un_max_xfer_size = ddi_getprop(DDI_DEV_T_ANY,
			devi, 0, "sd_max_xfer_size", SD_MAX_XFER_SIZE);
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "max transfer size = 0x%x\n",
			    un->un_max_xfer_size);
		}

	} else {
		if (scsi_ifsetcap(ROUTE, "wide-xfer", 0, 1) == 1) {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"Wide Transfer disabled\n");
		}
	}

	return (rval);

error:
	sd_free_softstate(un, devi);
	return (DDI_FAILURE);
}

static int
sd_winchester_exists(struct scsi_disk *un,
    struct scsi_pkt *rqpkt, int (*canwait)())
{
	register struct scsi_pkt *pkt;
	auto caddr_t wrkbuf;
	int rval = -1;
	struct buf *bp;

	/*
	 * Get a work packet to play with. Get one with a buffer big
	 * enough for another INQUIRY command, and get one with
	 * a cdb big enough for the READ CAPACITY command.
	 */
	bp = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
	    SUN_INQSIZE, B_READ, canwait, NULL);
	if (!bp) {
		return (rval);
	}
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
	    bp, CDB_GROUP0, un->un_cmd_stat_size, PP_LEN,
	    PKT_CONSISTENT, canwait, NULL);
	if (!pkt) {
		scsi_free_consistent_buf(bp);
		return (rval);
	}

	*((caddr_t *)&wrkbuf) = bp->b_un.b_addr;

	/*
	 * Send a TUR & throwaway START UNIT command.
	 *
	 * If we fail on this, we don't care presently what
	 * precisely is wrong. Fire off a throwaway REQUEST
	 * SENSE command if the failure is a CHECK_CONDITION.
	 */
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
		SCMD_TEST_UNIT_READY, 0, 0, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	(void) sd_scsi_poll(un, pkt);
	if (SCBP(pkt)->sts_chk) {
		if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
			(void) sd_clear_cont_alleg(un, rqpkt);
		}
	} else if (SCBP_C(pkt) == STATUS_RESERVATION_CONFLICT) {
		/*
		 * if the disk is reserved by other host, then the inquiry
		 * data we got earlier is probably OK. Also there is no
		 * need for START and REZERO commands.
		 */
		rval = SD_EACCES;
		goto out;
	}

	pkt->pkt_time = 200;
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp, SCMD_START_STOP,
		0, 1, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	if (!ISPXRE(un)) {
	    (void) sd_scsi_poll(un, pkt);
	    if (SCBP(pkt)->sts_chk) {
		    if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
			    (void) sd_clear_cont_alleg(un, rqpkt);
		    }
	    } else if (SCBP_C(pkt) == STATUS_RESERVATION_CONFLICT) {
		    rval = SD_EACCES;
		    goto out;
	    }
	}

	/*
	 * Send another Inquiry command to the target. This is necessary
	 * for non-removable media direct access devices because their
	 * Inquiry data may not be fully qualified until they are spun up
	 * (perhaps via the START command above)
	 */

	bzero(wrkbuf, SUN_INQSIZE);
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
		SCMD_INQUIRY, 0, SUN_INQSIZE, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	if (sd_scsi_poll(un, pkt) < 0)	 {
		goto out;
	} else if (SCBP(pkt)->sts_chk) {
		if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
			(void) sd_clear_cont_alleg(un, rqpkt);
		}
	} else {
		if ((pkt->pkt_state & STATE_XFERRED_DATA) &&
		    (SUN_INQSIZE - pkt->pkt_resid) >= SUN_MIN_INQLEN) {
			bcopy(wrkbuf, SD_INQUIRY, SUN_INQSIZE);
		}
	}

	/*
	 * It would be nice to attempt to make sure that there is a disk there,
	 * but that turns out to be very hard- We used to use the REZERO
	 * command to verify that a disk was attached, but some SCSI disks
	 * can't handle a REZERO command. We can't use a READ command
	 * because the disk may not be formatted yet. Therefore, we just
	 * have to believe a disk is there until proven otherwise, *except*
	 * (hack hack hack) for the MD21.
	 */
	if (un->un_ctype == CTYPE_MD21) {
		/*
		 * Don't need to set noparity- the MD21 supports it
		 */
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				SCMD_REZERO_UNIT, 0, 0, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		if (sd_scsi_poll(un, pkt) || SCBP_C(pkt) != STATUS_GOOD) {
			if (SCBP(pkt)->sts_chk) {
				if (((pkt->pkt_state & STATE_ARQ_DONE) == 0)) {
					(void) sd_clear_cont_alleg(un, rqpkt);
				}
			}
			goto out;
		}
	}

	/*
	 * At this point, we've 'succeeded' with this winchester
	 */

	rval = 0;
out:
	scsi_destroy_pkt(pkt);
	scsi_free_consistent_buf(bp);
	return (rval);
}

int
sd_read_capacity(struct scsi_disk *un, struct scsi_capacity *cptr)
{
	struct	scsi_pkt *pkt;
	caddr_t buffer;
	int rval = EIO;
	struct buf *bp;
	int	secsize;
	int	secdiv;
	int	i;

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "reading the drive's capacity\n");

	bp = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
	    sizeof (struct scsi_capacity), B_READ, NULL_FUNC, NULL);
	if (!bp) {
		return (ENOMEM);
	}
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL, bp,
	    CDB_GROUP1, un->un_cmd_stat_size, PP_LEN, PKT_CONSISTENT,
	    NULL_FUNC, NULL);
	if (!pkt) {
		scsi_free_consistent_buf(bp);
		return (ENOMEM);
	}
	*((caddr_t *)&buffer) = bp->b_un.b_addr;

	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_READ_CAPACITY, 0, 0, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	for (i = 0; i < 3; i++) {
		if (sd_scsi_poll(un, pkt) >= 0 && SCBP_C(pkt) == STATUS_GOOD &&
		    (pkt->pkt_state & STATE_XFERRED_DATA) &&
		    (pkt->pkt_resid == 0)) {
			/*
			 * The returned capacity is the LBA of the last
			 * addressable logical block, so the real capacity
			 * is one greater
			 *
			 * The capacity is always stored in terms of DEV_BSIZE.
			 */

			/*
			 * Note that we are updating the scsi_capacity
			 * capacity and lbasize for byte conversions here
			 * so that the byte order conversion does not need
			 * to be done anymore outside this function
			 */


			if (ISCD(un) && SD_IS_ATAPI(un))
				/*
				 * Some ATAPI CD drives lie about their
				 * lbasize (e.g., 2352 or 0 are common bogus
				 * responses). The ATAPI spec requires 2k so
				 * just force that value for all drives.
				 */
				cptr->lbasize = 2048;
			else
				cptr->lbasize = sd_stoh_int((uchar_t *)&
				    ((struct scsi_capacity *)buffer)->lbasize);

			cptr->capacity = sd_stoh_int((uchar_t *)&
			    (((struct scsi_capacity *)buffer)->capacity));
			cptr->capacity = (cptr->capacity + 1) *
			    (cptr->lbasize / DEV_BSIZE);
#if defined(__i386) || defined(__ia64)
			/*
			 * see bug #1175930 - off-by-1 error, #sectors on media.
			 */
			if (!ISCD(un) && !ISROD(un) &&
				(cptr->lbasize == DEV_BSIZE))
				cptr->capacity -= 1;
#endif	/* __i386 || __ia64 */
			rval = 0;

			if (cptr->lbasize > 0) {
				/*
				 * take a log base 2 of sector size
				 */

				secsize = cptr->lbasize;
				for (secdiv = 0; secsize = secsize >> 1;
					secdiv++)
					;

				un->un_secdiv = secdiv;
				un->un_blkshf = secdiv - DEV_BSHIFT;
			}
			break;
		} else {
			if (SCBP_C(pkt) == STATUS_RESERVATION_CONFLICT) {
				rval = EACCES;
				break;
			}
			if (SCBP(pkt)->sts_chk) {
				int 	rqdone = 1;
				struct scsi_extended_sense *arqstat_sense;

				if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
					if (sd_clear_cont_alleg(un, un->un_rqs)
									!= 0) {
						rqdone = 0;
					} else
						arqstat_sense = SD_RQSENSE;
				} else {
					arqstat_sense =
						(struct scsi_extended_sense *)
					    &(((struct scsi_arq_status *)
					    (pkt->pkt_scbp))->sts_sensedata);
				}

				if ((rqdone) &&
				    (arqstat_sense->es_add_code == 4) &&
				    arqstat_sense->es_qual_code == 1) {
					rval = EAGAIN;
				}
			}
		}
	}




	scsi_destroy_pkt(pkt);
	scsi_free_consistent_buf(bp);
	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "drive capacity %d block size %d\n",
	    cptr->capacity, cptr->lbasize);

	if (cptr->capacity <= 0 || cptr->lbasize <= 0) {
		cptr->capacity = (uint_t)-1;
		cptr->lbasize = (uint_t)-1;
		rval = EIO;
	}
	return (rval);
}


/*
 * Validate the geometry for this disk, e.g.,
 * see whether it has a valid label.
 */
static int
sd_validate_geometry(struct scsi_disk *un, int (*f)())
{
	char *label = 0;
	static char labelstring[128];
	static char buf[256];
	int count;
	int label_rc = 0;
	int gvalid = un->un_gvalid;
	int 	iopb_rval;
	int	fdisk_rval;
	int	lbasize;
	int	capacity;

	ASSERT(mutex_owned(SD_MUTEX));


	if (un->un_lbasize < 0) {
		return (EINVAL);
	}

#if defined(__i386) || defined(__ia64)
	if (un->un_capacity <= 0) {
		return (EINVAL);
	}
#endif


#if defined(_SUNOS_VTOC_16)
	/*
	 * Set up the "whole disk" fdisk partition; this should always
	 * exist, regardless of whether the disk contains an fdisk table
	 * or vtoc.
	 */
	un->un_map[P0_RAW_DISK].dkl_cylno = 0;
	un->un_map[P0_RAW_DISK].dkl_nblk = un->un_capacity;
#endif	/* defined(_SUNOS_VTOC_16) */

	/*
	 * copy the lbasize and capacity so that if they're
	 * reset while we're not holding the SD_MUTEX, we will
	 * continue to use valid values after the SD_MUTEX is
	 * reacquired.
	 */
	lbasize = un->un_lbasize;
	capacity = un->un_capacity;

	/*
	 * refresh the logical and physical geometry caches.
	 * (data from mode sense format/rigid disk geometry pages,
	 * and scsi_ifgetcap("geometry").
	 */
	sd_resync_geom_caches(un, capacity, lbasize);

	/*
	 * Only DIRECT ACCESS devices will have Sun labels.
	 * CD's supposedly have a Sun label, too
	 */

	if (SD_INQUIRY->inq_dtype == DTYPE_DIRECT || ISREMOVABLE(un)) {
		struct buf	*bp;
		struct dk_label	*dkl;
		int		 label_addr;

		if ((fdisk_rval = sd_read_fdisk(un, f, capacity, lbasize)) ==
			FDISK_NOMEM) {
			ASSERT(mutex_owned(SD_MUTEX));
			return (ENOMEM);
		}

		if (fdisk_rval == FDISK_RESV_CONFLICT) {
			ASSERT(mutex_owned(SD_MUTEX));
			return (EACCES);
		}

		if (un->un_solaris_size <= DK_LABEL_LOC) {
			/*
			 * Found fdisk table but no Solaris partition entry,
			 * so don't call sd_uselabel() and don't create
			 * a default label.
			 */
			label_rc = 0;
			un->un_gvalid = TRUE;
			goto no_solaris_partition;
		}

		/*
		 * Release the mutex while we allocate resources. We may
		 * sleep while the resources are allocated.
		 */
		mutex_exit(SD_MUTEX);
		bp = scsi_alloc_consistent_buf(ROUTE,
		    (struct buf *)NULL, lbasize, B_READ, f, NULL);
		mutex_enter(SD_MUTEX);
		if (!bp) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "no bp for disk label\n");
			ASSERT(mutex_owned(SD_MUTEX));
			return (ENOMEM);
		}

		clrbuf(bp);
		/*
		 * Ok, it's ready - try to read and use the label.
		 */
		label_addr = un->un_solaris_offset + DK_LABEL_LOC;
		iopb_rval = sd_iopb_read_block(un, label_addr, bp,
							(caddr_t *)&dkl, f);
		if (iopb_rval == IOPB_NOMEM) {
			scsi_free_consistent_buf(bp);
			ASSERT(mutex_owned(SD_MUTEX));
			return (ENOMEM);
		}

		if (iopb_rval == IOPB_SUCCESS) {
			/*
			 * sd_uselabel will establish
			 * that the geometry is valid
			 * sd_uselabel returns FALSE if the
			 * geometry is invalid
			 */
			mutex_exit(SD_MUTEX);
			if (sd_uselabel(un, dkl) == FALSE)
				label_rc = EINVAL;
			mutex_enter(SD_MUTEX);
		}

#if defined(_SUNOS_VTOC_8)
		label = (char *)un->un_asciilabel;

#elif defined(_SUNOS_VTOC_16)
		label = (char *)un->un_vtoc.v_asciilabel;
#else
#error "No VTOC format defined."
#endif
		scsi_free_consistent_buf(bp);
	} else if (capacity < 0) {
		ASSERT(mutex_owned(SD_MUTEX));
		return (EINVAL);
	}

	if (un->un_gvalid == FALSE) {
		sd_build_default_label(un);
		label_rc = 0;
	}

no_solaris_partition:

	if ((!ISREMOVABLE(un) ||
	    (ISREMOVABLE(un) && un->un_mediastate == DKIO_EJECTED)) &&
	    (un->un_state == SD_STATE_NORMAL && gvalid == FALSE)) {
		/*
		 * Print out a message indicating who and what we are.
		 * We do this only when we happen to really validate the
		 * geometry. We may call sd_validate_geometry() at other
		 * times, eg, ioctl()'s like Get VTOC in which case we
		 * dont want to print the label.
		 * If the geometry is valid, print the label string,
		 * else print vendor and product info, if available
		 */
		if (un->un_gvalid == TRUE && label) {
			scsi_log(SD_DEVINFO, sd_label, CE_CONT,
				"?<%s>\n", label);
		} else {
			mutex_enter(&sd_log_mutex);
			inq_fill(SD_INQUIRY->inq_vid, VIDMAX,
					labelstring);
			inq_fill(SD_INQUIRY->inq_pid, PIDMAX,
					&labelstring[64]);
			(void) sprintf(buf, "?Vendor '%s', product '%s'",
			    labelstring, &labelstring[64]);
			if (un->un_capacity > 0) {
				(void) sprintf(&buf[strlen(buf)],
					", %d %d byte blocks\n",
					(int)un->un_capacity,
					(int)un->un_lbasize);
			} else {
				(void) sprintf(&buf[strlen(buf)],
					", (unknown capacity)\n");
			}
			scsi_log(SD_DEVINFO, sd_label, CE_CONT, buf);
			mutex_exit(&sd_log_mutex);
		}
		if (sddebug) {
			mutex_enter(&sd_log_mutex);
			inq_fill(SD_INQUIRY->inq_vid, VIDMAX, labelstring);
			inq_fill(SD_INQUIRY->inq_serial, 12, &labelstring[16]);
			bcopy(&SD_INQUIRY->inq_serial,
				(void *)&un->un_serial_num_buf[0], 12);
			inq_fill(SD_INQUIRY->inq_revision, 4, &labelstring[32]);
			scsi_log(SD_DEVINFO, sd_label, CE_CONT,
			    "?%s serial number: %s - %s\n",
			    labelstring, &labelstring[16],
			    &labelstring[32]);
			mutex_exit(&sd_log_mutex);
		}
	}

#if defined(_SUNOS_VTOC_16)
	/*
	 * If we have valid geometry, set up the remaining fdisk partitions.
	 * Note that dkl_cylno is not used for the fdisk map entries, so
	 * we set it to an entirely bogus value.
	 */
	for (count = 0; count < FD_NUMPART; count++) {
		un->un_map[FDISK_P1 + count].dkl_cylno = -1;
		un->un_map[FDISK_P1 + count].dkl_nblk =
			un->un_fmap[count].fmap_nblk;

		un->un_offset[FDISK_P1 + count] =
			un->un_fmap[count].fmap_start;
	}
#endif

	for (count = 0; count < NDKMAP; count++) {
#if defined(_SUNOS_VTOC_8)
		struct dk_map *lp  = &un->un_map[count];

		un->un_offset[count] =
		    un->un_g.dkg_nhead *
		    un->un_g.dkg_nsect *
		    lp->dkl_cylno;
#elif defined(_SUNOS_VTOC_16)
		struct dkl_partition *vp = &un->un_vtoc.v_part[count];

		un->un_offset[count] = vp->p_start + un->un_solaris_offset;
#else
#error "No VTOC format defined."
#endif
	}


	ASSERT(mutex_owned(SD_MUTEX));
	return (label_rc);
}

/*
 * Check the label for righteousity, and snarf yummos from validated label.
 * Marks the geometry of the unit as being valid.
 */

static int
sd_uselabel(struct scsi_disk *un, struct dk_label *l)
{
	static char *geom = "Label says %d blocks, Drive says %d blocks\n";
	static char *badlab = "corrupt label - %s\n";
	short *sp, sum, count;
	int	i, computed_capacity, part_end;
	int	label_rc = TRUE;
	int	index;
	struct	scsi_capacity cbuf;
	int	track_capacity;
#if defined(_SUNOS_VTOC_16)
	struct	dkl_partition	*vpartp;
#endif
	int err;

	/*
	 * Check magic number of the label
	 */
	if (l->dkl_magic != DKL_MAGIC) {
#if defined(__sparc)
		if (un->un_state == SD_STATE_NORMAL && !ISREMOVABLE(un))
			scsi_log(SD_DEVINFO, sd_label, CE_WARN, badlab,
			    "wrong magic number");
#endif	/* defined(__sparc) */
		return (EINVAL);
	}

	/*
	 * Check the checksum of the label,
	 */
	sp = (short *)l;
	sum = 0;
	count = sizeof (struct dk_label) / sizeof (short);
	while (count--)	 {
		sum ^= *sp++;
	}

	if (sum) {
#if defined(_SUNOS_VTOC_16)
		if (un->un_state == SD_STATE_NORMAL && !ISCD(un))
#elif defined(_SUNOS_VTOC_8)
		if (un->un_state == SD_STATE_NORMAL && !ISREMOVABLE(un))
#else
#error "No VTOC format defined."
#endif
		{
			scsi_log(SD_DEVINFO, sd_label, CE_WARN, badlab,
			    "label checksum failed");
		}
		return (EINVAL);
	}

	mutex_enter(SD_MUTEX);
	/*
	 * Fill in geometry structure with data from label.
	 */
	un->un_g.dkg_ncyl = l->dkl_ncyl;
	un->un_g.dkg_acyl = l->dkl_acyl;
	un->un_g.dkg_bcyl = 0;
	un->un_g.dkg_nhead = l->dkl_nhead;
	un->un_g.dkg_nsect = l->dkl_nsect;
#if defined(_SUNOS_VTOC_8)
	un->un_g.dkg_gap1 = l->dkl_gap1;
	un->un_g.dkg_gap2 = l->dkl_gap2;
	un->un_g.dkg_bhead = l->dkl_bhead;

#elif defined(_SUNOS_VTOC_16)
	un->un_dkg_skew = l->dkl_skew;
#else
#error "No VTOC format defined."
#endif
	un->un_g.dkg_intrlv = l->dkl_intrlv;
	un->un_g.dkg_apc = l->dkl_apc;
	un->un_g.dkg_rpm = l->dkl_rpm;
	un->un_g.dkg_pcyl = l->dkl_pcyl;

	/*
	 * The Read and Write reinstruct values may not be valid
	 * for older disks.
	 */
	un->un_g.dkg_read_reinstruct = l->dkl_read_reinstruct;
	un->un_g.dkg_write_reinstruct = l->dkl_write_reinstruct;

	/*
	 * If labels don't have pcyl in them, make a guess at it.
	 * Take the value from the physical geometry cache if valid,
	 * or fudge it (for non-CCS devices).
	 */
	if (un->un_g.dkg_pcyl == 0) {
		un->un_g.dkg_pcyl = un->un_g.dkg_ncyl + un->un_g.dkg_acyl;
	}

#if defined(_SUNOS_VTOC_8)
	/*
	 * Fill in partition table.
	 */
	for (i = 0; i < NDKMAP; i++) {
		un->un_map[i].dkl_cylno =
			l->dkl_map[i].dkl_cylno;
		un->un_map[i].dkl_nblk =
			l->dkl_map[i].dkl_nblk;
	}

#elif defined(_SUNOS_VTOC_16)
	vpartp = l->dkl_vtoc.v_part;
	track_capacity = l->dkl_nhead * l->dkl_nsect;

	for (i = 0; i < NDKMAP; i++, vpartp++) {
		un->un_map[i].dkl_cylno = vpartp->p_start / track_capacity;
		un->un_map[i].dkl_nblk = vpartp->p_size;
	}
#else
#error "No VTOC format defined."
#endif
	/*
	 * Fill in VTOC Structure.
	 */
	bcopy(&l->dkl_vtoc, &un->un_vtoc, sizeof (struct dk_vtoc));

	un->un_gvalid = TRUE;		/* "it's here..." */

	track_capacity =  (un->un_g.dkg_nhead * un->un_g.dkg_nsect);
	computed_capacity = (un->un_g.dkg_ncyl * track_capacity);
	if (un->un_g.dkg_acyl) {	/* we may have > 1 alts cylinder */
		computed_capacity += (track_capacity * un->un_g.dkg_acyl);
	}

	if (un->un_capacity > 0) {	/* data from read capacity */
		if (computed_capacity > un->un_capacity &&
		    un->un_state == SD_STATE_NORMAL) {
			if (ISCD(un)) { /* trust the label */
#if defined(_SUNOS_VTOC_8)
				for (index = 0; index < NDKMAP; index++) {
					part_end =
						l->dkl_nhead * l->dkl_nsect *
						l->dkl_map[index].dkl_cylno +
						l->dkl_map[index].dkl_nblk - 1;
					if ((l->dkl_map[index].dkl_nblk) &&
						(part_end > un->un_capacity)) {
						un->un_gvalid = FALSE;
						break;
					}
				}
#elif defined(_SUNOS_VTOC_16)
				vpartp = &(l->dkl_vtoc.v_part[0]);
				for (index = 0; index < NDKMAP; index++,
				    vpartp++) {
					part_end = vpartp->p_start +
					    vpartp->p_size;
					if (vpartp->p_size > 0 &&
					    part_end > un->un_capacity) {
						un->un_gvalid = FALSE;
						break;
					}
				}
#else
#error "No VTOC format defined."
#endif
			} else {
				cbuf.capacity = (uint_t)-1;
				cbuf.lbasize = (uint_t)-1;
				mutex_exit(SD_MUTEX);
				err = sd_read_capacity(un, &cbuf);
				mutex_enter(SD_MUTEX);
				if (err == 0) {
					un->un_capacity = cbuf.capacity;
					un->un_lbasize = cbuf.lbasize;
				}
				if (computed_capacity > un->un_capacity) {
					scsi_log(SD_DEVINFO, sd_label, CE_WARN,
					    badlab, "bad geometry");
					scsi_log(SD_DEVINFO, sd_label, CE_CONT,
					    geom, computed_capacity,
					    un->un_capacity);
					un->un_gvalid = FALSE;
					label_rc = FALSE;
				}
			}
#if defined(_SUNOS_VTOC_8)
		/*
		 * we can't let this happen on drives that are subdivided
		 * into logical disks (i.e., that have an fdisk table).
		 * The un_capacity field should always hold the full media
		 * size in sectors, period.  This code would overwrite
		 * un_capacity with the size of the Solaris fdisk partition.
		 */
		} else {
			un->un_capacity = computed_capacity;
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, geom,
			    computed_capacity, un->un_capacity);
#endif	/* defined(_SUNOS_VTOC_8) */
		}
	} else {
		/*
		 * We have a situation where the target didn't give us a
		 * good 'read capacity' command answer, yet there appears
		 * to be a valid label. In this case, we'll
		 * fake the capacity.
		 */
		un->un_capacity = computed_capacity;
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_uselabel: (label "
	    "geometry)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "ncyl: %d\tacyl: %d\tnhead: "
	    "%d\tnsect: %d\n", un->un_g.dkg_ncyl, un->un_g.dkg_acyl,
	    un->un_g.dkg_nhead, un->un_g.dkg_nsect);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "lbasize: %d\tcapacity: "
	    "%d\tintrlv: %d\trpm: %d\n", un->un_lbasize, un->un_capacity,
	    un->un_g.dkg_intrlv, un->un_g.dkg_rpm);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "wrt_reinstr: %d\trd_rein"
	    "str: %d\n", un->un_g.dkg_write_reinstruct,
	    un->un_g.dkg_read_reinstruct);

	/*
	 * XXX: Use Mode Sense to determine things like
	 *	rpm, geometry, from SCSI-2 compliant
	 *	peripherals
	 */
	if (un->un_g.dkg_rpm == 0)
		un->un_g.dkg_rpm = 3600;

	/*
	 * the 16-slice vtoc includes the asciilabel
	 */
#if defined(_SUNOS_VTOC_8)
	bcopy(l->dkl_asciilabel, un->un_asciilabel, LEN_DKL_ASCII);
#endif	/* defined(_SUNOS_VTOC_8) */

	mutex_exit(SD_MUTEX);

	return (label_rc);
}


/*
 * Unix Entry Points
 */

/* ARGSUSED3 */
static int
sdopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	register dev_t dev = *dev_p;
	register int rval = EIO;
	register int partmask;
	register int nodelay = (flag & (FNDELAY | FNONBLOCK));
	int i;
	char kstatname[KSTAT_STRLEN];

	GET_SOFT_STATE(dev);

	if (otyp >= OTYPCNT) {
		return (EINVAL);
	}

	partmask = 1 << part;

	/*
	 * We use a semaphore here in order to serialize
	 * open and close requests on the device.
	 */
	sema_p(&un->un_semoclose);

	mutex_enter(SD_MUTEX);


	/*
	 * All device accesses go thru sdstrategy() where we check
	 * on suspend status but there could be a scsi_poll command,
	 * which bypasses sdstrategy(), so we need to check PM_SUSPEND
	 * status and call ddi_dev_is_needed().
	 */
	if (un->un_state == SD_STATE_PM_SUSPENDED) {
		mutex_exit(SD_MUTEX);
		if (ddi_dev_is_needed(SD_DEVINFO, 0, 1) !=
		    DDI_SUCCESS) {
			mutex_enter(SD_MUTEX);
			rval = EIO;
			goto done;
		}
		mutex_enter(SD_MUTEX);
	}

	/*
	 * set make_sd_cmd() flags and stat_size here since these
	 * are unlikely to change
	 */
	un->un_cmd_flags = 0;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sdopen un=%p\n",
		(void *)un);
	/*
	 * check for previous exclusive open
	 */
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"exclopen=%x, flag=%x, regopen=%x\n",
		un->un_exclopen, flag, un->un_ocmap.regopen[otyp]);

	if (un->un_exclopen & (partmask)) {
failed_exclusive:
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"exclusive open fails\n");
		rval = EBUSY;
		goto done;
	}

	if (flag & FEXCL) {
		int i;
		if (un->un_ocmap.lyropen[part]) {
			goto failed_exclusive;
		}
		for (i = 0; i < (OTYPCNT - 1); i++) {
			if (un->un_ocmap.regopen[i] & (partmask)) {
				goto failed_exclusive;
			}
		}
	}
	if (ISREMOVABLE(un)) {
		if (flag & FWRITE) {
			mutex_exit(SD_MUTEX);
			if (ISCD(un) || sd_check_wp(dev, SD_GRP1_2_CDBS(un))) {
				sema_v(&un->un_semoclose);
				return (EROFS);
			}
			mutex_enter(SD_MUTEX);
		}
	}

	if (!nodelay) {
		mutex_exit(SD_MUTEX);
		rval = sd_ready_and_valid(dev, un);
		mutex_enter(SD_MUTEX);
		/*
		 * Fail if device is not ready or if the number of disk
		 * blocks is zero or negative for non CD devices.
		 */
		if (rval || (!ISCD(un) && un->un_map[part].dkl_nblk <= 0)) {
			if (rval == SD_TUR_FAILED && ISREMOVABLE(un)) {
				rval = ENXIO;
			} else {
				rval = EIO;
			}
			goto done;
		}
#if defined(__i386) || defined(__ia64)
	} else {
		un->un_gvalid = FALSE;
#endif	/* __i386 || __ia64 */
	}

	if (otyp == OTYP_LYR) {
		un->un_ocmap.lyropen[part]++;
	} else {
		un->un_ocmap.regopen[otyp] |= partmask;
	}

	/*
	 * set up open and exclusive open flags
	 */
	if (flag & FEXCL) {
		un->un_exclopen |= (partmask);
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "open of part %d type %d\n",
		part, otyp);

	mutex_exit(SD_MUTEX);

	/*
	 * only create kstats for disks, CD kstats created in sdattach
	 */
	LOCK_LINTED
	_NOTE(NO_COMPETING_THREADS_NOW);
	if (!(ISREMOVABLE(un))) {
		if (un->un_stats == (kstat_t *)0) {
			un->un_stats = kstat_create("sd", instance,
					NULL, "disk", KSTAT_TYPE_IO, 1,
					KSTAT_FLAG_PERSISTENT);
			if (un->un_stats) {
				un->un_stats->ks_lock = SD_MUTEX;
				kstat_install(un->un_stats);
			}
			/*
			 * set up partition statistics for each partition
			 * with number of blocks > 0
			*/
			for (i = 0; i < NSDMAP; i++) {
				if ((un->un_pstats[i] == (kstat_t *)0) &&
				    (un->un_map[i].dkl_nblk != 0)) {
					(void) sprintf(kstatname, "sd%d,%s\0",
					    instance, sd_minor_data[i].name);
					un->un_pstats[i] = kstat_create("sd",
					    instance, kstatname, "partition",
					    KSTAT_TYPE_IO, 1,
						KSTAT_FLAG_PERSISTENT);
					if (un->un_pstats[i]) {
					    un->un_pstats[i]->ks_lock =
							SD_MUTEX;
					    kstat_install(un->un_pstats[i]);
					}
				}
			}
			/*
			 * set up error kstats
			 */
			(void) sd_create_errstats(un, instance);
		}
	}
	LOCK_LINTED
	_NOTE(COMPETING_THREADS_NOW);

	sema_v(&un->un_semoclose);
	return (0);

done:
	mutex_exit(SD_MUTEX);
	sema_v(&un->un_semoclose);
	return (rval);

}

/*
 * Test if disk is ready and has a valid geometry.
 */
static int
sd_ready_and_valid(dev_t dev, struct scsi_disk *un)
{
	register int rval = SD_READY_VALID;
	register int error;
	auto struct scsi_capacity capbuf;
	struct sd_errstats *stp;

	mutex_enter(SD_MUTEX);
	if (ISREMOVABLE(un) || ((!ISREMOVABLE(un)) && (un->un_capacity < 0))) {
		if (!ISREMOVABLE(un)) {
			if (un->un_ncmds == 0) {
				(void) sd_unit_ready(dev);
			}
		} else {
			if (sd_unit_ready(dev) != 0) {
				rval = SD_TUR_FAILED;
				goto done;
			}
		}

		if (un->un_gvalid == FALSE || (int)un->un_capacity < 0 ||
		    un->un_lbasize < 0) {

			mutex_exit(SD_MUTEX);
			error = sd_read_capacity(un, &capbuf);
			mutex_enter(SD_MUTEX);


			if (error) {
				rval = EIO;
				un->un_gvalid = FALSE;
				goto done;
			} else {
				un->un_lbasize = capbuf.lbasize;
				un->un_capacity = capbuf.capacity;
			}
		}
	} else {
		/*
		 * do a test unit ready to clear any unit attention
		 * from non-cd devices.
		 * note: some TQ devices get hung (ie. discon. cmd
		 * timeouts) when they receive a TUR when the queue
		 * is non-empty therefore, only issue a TUR if no
		 * cmds outstanding
		 */
		if (un->un_ncmds == 0) {
			(void) sd_unit_ready(dev);
		}
	}

	/*
	 * If device is not yet ready here, inform it is offline
	 */
	if (un->un_state == SD_STATE_NORMAL) {
		error = sd_unit_ready(dev);
		if (error != 0 && error != EACCES) {
			rval = SD_TUR_FAILED;
			sd_offline(un, 1);
			goto done;
		}
	}

	if (un->un_format_in_progress == 0) {
		if ((error = sd_validate_geometry(un, SLEEP_FUNC)) != 0) {
			/*
			 * We don't check the validity of geometry for
			 * CDROMs. Also we assume we have a good label
			 * even if sd_validate_geometry returned ENOMEM.
			 */
			if (!ISCD(un) && error != ENOMEM) {
				rval = error;
				goto done;
			}
		}
		/*
		 * sd_validate_geometry returned TRUE, but that doesn't
		 * necessarily mean un_gvalid == TRUE.
		 */
	}

#ifdef DOESNTWORK /* on eliteII, see 1118607 */
	/*
	 * check to see if this disk is write protected,
	 * if it is and we have not set read-only, then fail
	 */
	mutex_exit(SD_MUTEX);
	if ((flag & FWRITE) && (sd_check_wp(dev))) {
		mutex_enter(SD_MUTEX);
		New_state(un, SD_STATE_CLOSED);
		goto done;
	}
	mutex_enter(SD_MUTEX);
#endif

	/*
	 * If this is a removable media device, try and send
	 * a PREVENT MEDIA REMOVAL command, but don't get upset
	 * if it fails.
	 * For a CD, however, it is an error
	 */
	if (ISREMOVABLE(un)) {
		mutex_exit(SD_MUTEX);
		error = sd_lock_unlock(dev, SD_REMOVAL_PREVENT);
		mutex_enter(SD_MUTEX);
		if (ISCD(un) && error != 0) {
			rval = EIO;
			goto done;
		}
	}

	/*
	 * the state has changed, inform the media watch routines
	 */
	un->un_mediastate = DKIO_INSERTED;
	cv_broadcast(&un->un_state_cv);

done:
	/*
	 * Initialize the capacity kstat value, if no media previously
	 * (capacity kstat is 0) and a media has been inserted
	 * (un_capacity > 0).
	 * This is a more generic way then checking for ISREMOVABLE.
	 */
	if (un->un_errstats) {
		stp = (struct sd_errstats *)un->un_errstats->ks_data;
		if (stp->sd_capacity.value.ui64 == 0 && un->un_capacity > 0) {
			stp->sd_capacity.value.ui64 =
				(uint64_t)((uint64_t)un->un_capacity *
				DEV_BSIZE);
		}
	}
	mutex_exit(SD_MUTEX);
	return (rval);
}

/*ARGSUSED*/
static int
sdclose(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	register uchar_t *cp;
	register int syncacherr = 0;
	int nodelay = (flag & (FNDELAY | FNONBLOCK));
	int i;

	GET_SOFT_STATE(dev);


	if (otyp >= OTYPCNT)
		return (ENXIO);

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "close of part %d type %d\n",
	    part, otyp);
	sema_p(&un->un_semoclose);

	mutex_enter(SD_MUTEX);

	if (un->un_exclopen & (1<<part)) {
		un->un_exclopen &= ~(1<<part);
	}

	if ((un->un_state == SD_STATE_SUSPENDED) ||
	    (un->un_state == SD_STATE_PM_SUSPENDED)) {
		if (un->un_power_level == 0) {
			mutex_exit(SD_MUTEX);
			if (ddi_dev_is_needed(SD_DEVINFO, 0, 1)
			    != DDI_SUCCESS) {
				return (EIO);
			}
			mutex_enter(SD_MUTEX);
		}
	}
	(void) pm_idle_component(SD_DEVINFO, 0);

	if (otyp == OTYP_LYR) {
		un->un_ocmap.lyropen[part] -= 1;
	} else {
		un->un_ocmap.regopen[otyp] &= ~(1<<part);
	}

	cp = &un->un_ocmap.chkd[0];
	while (cp < &un->un_ocmap.chkd[OCSIZE]) {
		if (*cp != (uchar_t)0) {
			break;
		}
		cp++;
	}

	if (cp == &un->un_ocmap.chkd[OCSIZE]) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "last close\n");
		un->ebp_enabled = 0;
		un->un_rqs_state = 0;
		if (un->un_state == SD_STATE_OFFLINE) {
			sd_offline(un, 1);
		} else {
			int rval;

			/*
			 * Synchronize cache not required for CDROM drives.
			 */
			if (!ISCD(un) && !ISPXRE(un)) {
				mutex_exit(SD_MUTEX);
				if (sd_synchronize_cache(dev) != 0) {
					syncacherr = EIO;
				}
				mutex_enter(SD_MUTEX);
			}

			/*
			 * If this is a removable media device, try and send
			 * an ALLOW MEDIA REMOVAL command, but don't get upset
			 * if it fails.
			 *
			 * For now, if it is a removable media device,
			 * invalidate the geometry.
			 *
			 * XXX: Later investigate whether or not it
			 *	would be a good idea to invalidate
			 *	the geometry for all devices.
			 */

			if (ISREMOVABLE(un)) {
				mutex_exit(SD_MUTEX);
				rval = sd_lock_unlock(dev, SD_REMOVAL_ALLOW);
				if (ISCD(un) && rval != 0 && (!nodelay)) {
					/*
					 * If we are returning error in close
					 * might as well invalidate the
					 * geometry.
					 */
					mutex_enter(SD_MUTEX);
					sd_ejected(un);
					mutex_exit(SD_MUTEX);
					sema_v(&un->un_semoclose);
					return (ENXIO);
				}
				mutex_enter(SD_MUTEX);
			}

			if (ISREMOVABLE(un)) {
				sd_ejected(un);
			}
		}
		/*
		 * dont remove REMOVABLE media stats - won't be recreated
		 *
		 */
		if (!(ISREMOVABLE(un))) {
			LOCK_LINTED
			_NOTE(NO_COMPETING_THREADS_NOW);
			mutex_exit(SD_MUTEX);
			if (un->un_stats) {
				kstat_delete(un->un_stats);
				un->un_stats = 0;
			}
			for (i = 0; i < NSDMAP; i++) {
				if (un->un_pstats[i]) {
					kstat_delete(un->un_pstats[i]);
					un->un_pstats[i] = (kstat_t *)0;
				}
			}
			if (un->un_errstats) {
				kstat_delete(un->un_errstats);
				un->un_errstats = (kstat_t *)0;
			}
			mutex_enter(SD_MUTEX);
			LOCK_LINTED
			_NOTE(COMPETING_THREADS_NOW);
		}
	}
	mutex_exit(SD_MUTEX);
	sema_v(&un->un_semoclose);
	return (syncacherr);
}

void
sd_offline(struct scsi_disk *un, int bechatty)
{
	ASSERT(mutex_owned(SD_MUTEX));

	if (bechatty && !ISPXRE(un))
		scsi_log(SD_DEVINFO, sd_label, CE_WARN, "offline\n");

	un->un_gvalid = FALSE;
}

static int
sd_not_ready(struct scsi_disk *un, struct scsi_pkt *pkt)
{
	int rval = JUST_RETURN;

	ASSERT(mutex_owned(SD_MUTEX));

	switch (SD_RQSENSE->es_add_code) {
	case 0x04:
		/*
		 * disk drives frequently refuse to spin up which
		 * results in a very long land in format without
		 * warning messages.
		 */
		if ((((sd_error_level < SCSI_ERR_RETRYABLE) ||
		    (PKT_GET_RETRY_CNT(pkt) == sd_reset_retry_count)) &&
		    (un->un_restart_timeid == 0)) &&
		    (SD_RQSENSE->es_qual_code == 0)) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "logical unit not ready, resetting disk\n");
			sd_reset_disk(un, pkt);
		}
		/*
		 * If the drive is already in the process of being ready,
		 * we don't need to do anything.
		 * (This shows up in CD-ROMs which takes long time to
		 * to read TOC following a power cycle/reset)
		 */
		if (SD_RQSENSE->es_qual_code != 1) {
			/*
			 * do not call restart unit immediately since more
			 * requests may come back from the target with check
			 * condition the start motor cmd would just hit
			 * the busy condition (contingent allegiance)
			 * sd_decode_sense() will execute a timeout() on
			 * sdrestart with the same delay, so don't increase
			 * this delay.
			 */
			if (pkt->pkt_cdbp[0] != SCMD_START_STOP) {
				/*
				 * If the original cmd was SCMD_START_STOP
				 * then don't initiate another one via
				 * sd_restart_unit
				 */
				if (un->un_restart_timeid) {
					/*
					 * If start unit is already sent,
					 * don't send another.
					 */
					if (sddebug) {
						cmn_err(CE_NOTE,
				"restart : already issued to : 0x%x : 0x%x\n",
						un->un_sd->sd_address.a_target,
						un->un_sd->sd_address.a_lun);
					}
				} else if (!ISPXRE(un)) {
					/*
					 * Do not start unit for PXRE
					 */
					un->un_restart_timeid =
					    timeout(sd_restart_unit, un,
					    SD_BSY_TIMEOUT/2);
				}
			}
		}
		break;
	case 0x05:
		if (sd_error_level < SCSI_ERR_RETRYABLE)
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "unit does not respond to selection\n");
		break;
	case 0x3a:
		if (sd_error_level >= SCSI_ERR_FATAL)
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "Caddy not inserted in drive\n");
		/*
		 * Invalidate capacity
		 */
		sd_ejected(un);
		un->un_mediastate = DKIO_EJECTED;
		cv_broadcast(&un->un_state_cv);

		/*
		 * Force no more retries
		 *
		 */
		PKT_SET_RETRY_CNT(pkt, CD_NOT_READY_RETRY_COUNT);
		rval = SD_MEDIUM_NOT_PRESENT;
		break;
	default:
		if (sd_error_level < SCSI_ERR_RETRYABLE)
			scsi_log(SD_DEVINFO, sd_label, CE_NOTE,
			    "Unit not Ready. Additional sense code 0x%x\n",
				SD_RQSENSE->es_add_code);
		break;
	}
	return (rval);
}

/*
 * Invalidate geometry information after detecting an ejected* media.
 */
static void
sd_ejected(register struct scsi_disk *un)
{
	struct sd_errstats *stp;
	ASSERT(mutex_owned(SD_MUTEX));

	un->un_capacity = -1;
	un->un_lbasize = -1;
	un->un_gvalid = FALSE;

	if (un->un_errstats) {
		stp = (struct sd_errstats *)un->un_errstats->ks_data;
		stp->sd_capacity.value.ui64 = 0;
	}
}

/*
 * restart disk. this function is called thru timeout(9F) and it
 * would be bad practice to sleep till completion in the timeout
 * thread. therefore, use the callback function to clean up
 * we don't care here whether transport succeeds or not; it will get
 * retried again later
 */
static void
sd_restart_unit(void *arg)
{
	register struct scsi_disk *un = arg;
	register struct scsi_pkt *pkt;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_restart_unit\n");

	if (un->un_format_in_progress) {
		/*
		 * Some unformatted drives report not ready error,
		 * no need to restart if format has been initiated.
		 */
error:
		mutex_enter(SD_MUTEX);
		un->un_restart_timeid = 0;
		mutex_exit(SD_MUTEX);
		return;
	}

	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL, NULL,
	    CDB_GROUP0, un->un_cmd_stat_size, 0, 0, NULL_FUNC, NULL);
	if (!pkt) {
		goto error;
	}
	mutex_enter(SD_MUTEX);
	un->un_restart_timeid = 0;
	un->un_ncmds++;
	un->un_start_stop_issued = 1;
	mutex_exit(SD_MUTEX);

	pkt->pkt_time = 90;	/* this is twice what we require */
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_START_STOP, 0, 1, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	pkt->pkt_flags = un->un_tagflags;
	pkt->pkt_comp = sd_restart_unit_callback;
	pkt->pkt_private = (opaque_t)un;
	if (scsi_transport(pkt) != TRAN_ACCEPT) {
		mutex_enter(SD_MUTEX);
		un->un_start_stop_issued = 0;
		un->un_ncmds--;
		mutex_exit(SD_MUTEX);
		scsi_destroy_pkt(pkt);
	}
}

static void
sd_restart_unit_callback(struct scsi_pkt *pkt)
{
	struct scsi_disk *un = (struct scsi_disk *)pkt->pkt_private;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_restart_unit_callback\n");

	if (SCBP(pkt)->sts_chk) {
		if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
			(void) sd_clear_cont_alleg(un, un->un_rqs);
			mutex_enter(SD_MUTEX);
		} else {
			struct scsi_arq_status *arqstat;
			arqstat = (struct scsi_arq_status *)(pkt->pkt_scbp);

			mutex_enter(SD_MUTEX);
			bcopy(&arqstat->sts_sensedata,
				SD_RQSENSE, SENSE_LENGTH);
		}
		sd_errmsg(un, pkt, SCSI_ERR_RETRYABLE, 0);
	} else {
		mutex_enter(SD_MUTEX);
	}
	un->un_start_stop_issued = 0;
	un->un_ncmds--;
	mutex_exit(SD_MUTEX);
	scsi_destroy_pkt(pkt);
}

/*
 * Given the device number return the devinfo pointer
 * from the scsi_device structure.
 */
/*ARGSUSED*/
static int
sdinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register dev_t dev;
	register struct scsi_disk *un;
	register int instance, error;


	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = SDUNIT(dev);
		if ((un = ddi_get_soft_state(sd_state, instance)) == NULL)
			return (DDI_FAILURE);
		*result = (void *) SD_DEVINFO;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		instance = SDUNIT(dev);
		*result = (void *)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * property operation routine.	return the number of blocks for the partition
 * in question or forward the request to the propery facilities.
 */
static int
sd_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int nblocks, length, km_flags;
	caddr_t buffer;
	struct scsi_disk *un;
	int instance;

	if (dev != DDI_DEV_T_ANY)
		instance = SDUNIT(dev);
	else
		instance = ddi_get_instance(dip);

	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL)
		return (DDI_PROP_NOT_FOUND);


	if (strcmp(name, "nblocks") == 0) {
		if (un->un_gvalid == FALSE) {
			return (DDI_PROP_NOT_FOUND);
		}
		mutex_enter(SD_MUTEX);
		nblocks = (int)un->un_map[SDPART(dev)].dkl_nblk;
		mutex_exit(SD_MUTEX);

		/*
		* get callers length set return length.
		*/
		length = *lengthp;		/* Get callers length */
		*lengthp = sizeof (int);	/* Set callers length */

		/*
		* If length only request or prop length == 0, get out now.
		* (Just return length, no value at this level.)
		*/
		if (prop_op == PROP_LEN)  {
			*lengthp = sizeof (int);
			return (DDI_PROP_SUCCESS);
		}

		/*
		* Allocate buffer, if required.	 Either way,
		* set `buffer' variable.
		*/
		switch (prop_op)  {

		case PROP_LEN_AND_VAL_ALLOC:

			km_flags = KM_NOSLEEP;

			if (mod_flags & DDI_PROP_CANSLEEP)
				km_flags = KM_SLEEP;

			buffer = kmem_alloc((size_t)sizeof (int), km_flags);
			if (buffer == NULL)  {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    "no mem for property\n");
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer; /* Set callers buf ptr */
			break;

		case PROP_LEN_AND_VAL_BUF:

			if (sizeof (int) > length)
			return (DDI_PROP_BUF_TOO_SMALL);

			buffer = valuep; /* get callers buf ptr */
			break;
		}
		*((int *)buffer) = nblocks;
		return (DDI_PROP_SUCCESS);
	}

	/*
	* not mine pass it on.
	*/
	return (ddi_prop_op(dev, dip, prop_op, mod_flags,
		name, valuep, lengthp));
}

/*
 * These routines perform raw i/o operations.
 */
/*ARGSUSED*/
static void
sduscsimin(struct buf *bp)
{
	/*
	 * do not break up because the CDB count would then
	 * be incorrect and data underruns would result (incomplete
	 * read/writes which would be retried and then failed, see
	 * sdintr().
	 */
}


/*
 * Xscsi_poll()
 *
 * This routine is a variation of the SCSA DDI scsi_poll(9f) function.
 * The DDI version of this routine doesn't correctly retry the
 * HBA and target device busy conditions. Because the DDI scsi_poll() also
 * conflates all the failure conditions into a single return value (-1),
 * it's not possible to simply place a wrapper around the DDI scsi_poll(9f)
 * routine which retries just the busy conditions. Therefore, I've take
 * the whole DDI scsi_poll(9F) routine and copied into this driver and
 * made the necessary fixes here. Since this version of the function is
 * probably usefull in other targer drivers, at some future time we
 * should consider whether this version should be added to the DDI
 * (either as new function or an improved version of the existing function).
 *
 */

int Xscsi_poll_busycnt = 0;

static int
Xscsi_poll(struct scsi_pkt *pkt)
{
	int busy_count, rval = -1, savef;
	int savet;
	void (*savec)();

	/*
	 * save old flags..
	 */
	savef = pkt->pkt_flags;
	savec = pkt->pkt_comp;
	savet = pkt->pkt_time;

	pkt->pkt_flags |= FLAG_NOINTR;

	/*
	 * XXX there is nothing in the SCSA spec that states that we should not
	 * do a callback for polled cmds; however, removing this will break sd
	 * and probably other target drivers
	 */
	pkt->pkt_comp = 0;

	/*
	 * we don't like a polled command without timeout.
	 * 60 seconds seems long enough.
	 */
	if (pkt->pkt_time == 0) {
		pkt->pkt_time = SCSI_POLL_TIMEOUT;
	}

	busy_count = 0;
	for (;;) {
		int	rc;

		if ((rc = scsi_transport(pkt)) == TRAN_BUSY) {
			/*
			 * loop until done if no loop limit
			 * or check the loop limit and fail if over the limit
			 */
			if (Xscsi_poll_busycnt != 0 &&
			    ++busy_count >= Xscsi_poll_busycnt) {
				break;
			}
			drv_usecwait(1000000);
			continue;
		}
		if (rc != TRAN_ACCEPT) {
			break;
		}
		if (pkt->pkt_reason == CMD_INCOMPLETE && pkt->pkt_state == 0) {
			drv_usecwait(1000000);

		} else if (pkt->pkt_reason != CMD_CMPLT) {
			break;

		} else if (((*pkt->pkt_scbp) & STATUS_MASK) == STATUS_BUSY ||
			    ((*pkt->pkt_scbp) & STATUS_MASK) == STATUS_QFULL) {
			drv_usecwait(1000000);

		} else {
			rval = 0;
			break;
		}

		/* loop until done if no loop limit */
		if (Xscsi_poll_busycnt == 0)
			continue;

		/* check the loop limit */
		if (++busy_count >= Xscsi_poll_busycnt)
			break;
	}

	pkt->pkt_flags = savef;
	pkt->pkt_comp = savec;
	pkt->pkt_time = savet;
	return (rval);
}



int
sd_scsi_poll(struct scsi_disk *un, struct scsi_pkt *pkt)
{
	if (scsi_ifgetcap(&pkt->pkt_address, "tagged-qing", 1) == 1) {
		pkt->pkt_flags |= un->un_tagflags;
		pkt->pkt_flags &= ~FLAG_NODISCON;
	}
	return (Xscsi_poll(pkt));
}


int
sd_clear_cont_alleg(struct scsi_disk *un, struct scsi_pkt *pkt)
{
	int	ret;
	ASSERT(pkt == un->un_rqs);

	/*
	 * The request sense should be sent as an untagged command,
	 * otherwise we might get busy error from disk. So we make
	 * a direct call to scsi_poll since sd_scsi_poll uses tags.
	 */
	sema_p(&un->un_rqs_sema);

	/*
	 * Take the mutex which ensures that the other thread is
	 * done with decoding the request sense data
	 */
	mutex_enter(SD_MUTEX);
	mutex_exit(SD_MUTEX);

	ret = Xscsi_poll(pkt);
	sema_v(&un->un_rqs_sema);
	return (ret);
}


static void
sdmin(struct buf *bp)
{
	register struct scsi_disk *un;
	register int instance;
	register minor_t minor = getminor(bp->b_edev);
	instance = (int)(minor >> SDUNIT_SHIFT);
	un = ddi_get_soft_state(sd_state, instance);

	if (bp->b_bcount > un->un_max_xfer_size)
		bp->b_bcount = un->un_max_xfer_size;
}


/* ARGSUSED2 */
static int
sdread(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	register int secmask;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	if (!ISCD(un) && (un->un_gvalid == FALSE)) {
		if ((sd_ready_and_valid(dev, un)) != SD_READY_VALID)
			return (EIO);
	}
	secmask = XDEV_BSIZE - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "file offset not modulo %d\n",
		    XDEV_BSIZE);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "transfer length not modulo %d\n", XDEV_BSIZE);
		return (EINVAL);
	}
	return (physio(sdstrategy, (struct buf *)0, dev, B_READ, sdmin, uio));
}

/* ARGSUSED2 */
static int
sdaread(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	register int secmask;
	struct uio *uio = aio->aio_uio;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	if (!ISCD(un) && (un->un_gvalid == FALSE)) {
		if ((sd_ready_and_valid(dev, un)) != SD_READY_VALID)
			return (EIO);
	}
	secmask = XDEV_BSIZE - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "file offset not modulo %d\n",
		    XDEV_BSIZE);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "transfer length not modulo %d\n", XDEV_BSIZE);
		return (EINVAL);
	}
	return (aphysio(sdstrategy, anocancel, dev, B_READ, sdmin, aio));
}

/* ARGSUSED2 */
static int
sdwrite(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	register int secmask;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	if (!ISCD(un) && (un->un_gvalid == FALSE)) {
		if ((sd_ready_and_valid(dev, un)) != SD_READY_VALID)
			return (EIO);
	}
	secmask = XDEV_BSIZE - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "file offset not modulo %d\n",
		    XDEV_BSIZE);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "transfer length not modulo %d\n", XDEV_BSIZE);
		return (EINVAL);
	}
	return (physio(sdstrategy, (struct buf *)0, dev, B_WRITE, sdmin, uio));
}

/* ARGSUSED2 */
static int
sdawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	register int secmask;
	struct uio *uio = aio->aio_uio;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	if (!ISCD(un) && (un->un_gvalid == FALSE)) {
		if ((sd_ready_and_valid(dev, un)) != SD_READY_VALID)
			return (EIO);
	}
	secmask = XDEV_BSIZE - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "file offset not modulo %d\n",
		    XDEV_BSIZE);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "transfer length not modulo %d\n", XDEV_BSIZE);
		return (EINVAL);
	}
	return (aphysio(sdstrategy, anocancel, dev, B_WRITE, sdmin, aio));
}

/*
 * strategy routine
 */
static int
sdstrategy(struct buf *bp)
{
	register struct scsi_disk *un;
	register struct diskhd *dp;
	int	i;
	register minor_t minor = getminor(bp->b_edev);
	int	 part = minor & SDPART_MASK;
	struct dk_map *lp;
	register daddr_t bn;
	struct buf *nbp;
	long	secnt, count;
	size_t	resid = 0;

	if (((un = ddi_get_soft_state(sd_state,
	    minor >> SDUNIT_SHIFT)) == NULL) ||
	    (un->un_state == SD_STATE_DUMPING)) {
		SET_BP_ERROR(bp, ((un) ? ENXIO : EIO));
error:
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return (0);
	}


	TRACE_2(TR_FAC_SCSI, TR_SDSTRATEGY_START,
	    "sdstrategy_start: bp %x ncmds %d", bp, un->un_ncmds);

	/*
	 * For a CD, a write is an error
	 */

	if (ISCD(un) && (bp != un->un_sbufp) && !(bp->b_flags & B_READ)) {
		SET_BP_ERROR(bp, EIO);
		goto error;
	}

	/*
	 * mark busy so we won't get powered down again while
	 * trying to resume.
	 * Note that there must be one pm_idle_components() for each
	 * pm_busy_components().
	 */
	(void) pm_busy_component(SD_DEVINFO, 0);

	mutex_enter(SD_MUTEX);
	while ((un->un_state == SD_STATE_SUSPENDED) ||
	    (un->un_state == SD_STATE_PM_SUSPENDED)) {

		/*
		 * Commands may sneak in while we released the mutex in
		 * DDI_SUSPEND, we should block new commands.
		 */
		while (un->un_state == SD_STATE_SUSPENDED) {
			cv_wait(&un->un_suspend_cv, SD_MUTEX);
		}
		/*
		 * drop mutex because ddi_dev_is_needed()
		 * will result in a call to sdpower()
		 */
		if (un->un_state == SD_STATE_PM_SUSPENDED) {
			mutex_exit(SD_MUTEX);
			if (ddi_dev_is_needed(SD_DEVINFO, 0, 1) !=
			    DDI_SUCCESS) {
				SET_BP_ERROR(bp, EIO);
				goto error;
			}
			mutex_enter(SD_MUTEX);
		}
	}
	mutex_exit(SD_MUTEX);

	bp->b_flags &= ~(B_DONE|B_ERROR);
	bp->b_resid = 0;
	bp->av_forw = 0;

	/*
	 * usb:
	 * the USB nexus driver does not support direct dma into the "bp"
	 */
	if ((un->un_isusb != -1)) {
		bp_mapin(bp);
	}

	if (bp != un->un_sbufp) {
		if (un->un_gvalid == TRUE) {
validated:
			lp = &un->un_map[minor & SDPART_MASK];
			bn = dkblock(bp);

			SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "dkblock(bp) is %ld\n", bn);

			i = 0;
			if (bn < 0) {
				i = -1;
			} else if (!ISCD(un) &&
				(bn & ((1 << un->un_blkshf) - 1))) {
				/*
				 * making sure starting block is aligned
				 * to native block size. We only support
				 * non-aligned i/o to cd drives.
				 *
				 */
				i = -1;
			} else if (bn >= lp->dkl_nblk) {
				/*
				 * For proper comparison, file system block
				 * number has to be scaled to actual CD
				 * transfer size.
				 * Since all the CDROM operations
				 * that have Sun Labels are in the correct
				 * block size this will work for CD's.	This
				 * will have to change when we have different
				 * sector sizes.
				 *
				 * if bn == lp->dkl_nblk,
				 * Not an error, resid == count
				 */
				if (bn > lp->dkl_nblk) {
					i = -1;
				} else {
					i = 1;
				}
			} else if (bp->b_bcount & (XDEV_BSIZE - 1)) {
				i = -1;
			} else {
				/*
				 * Make sure we don't run off the end of
				 * a partition.
				 *
				 * Put this test here so that if we
				 * have to transfer less than b_bcount
				 * we allocate a new local buffer to handle
				 * that.
				 * Note that if we would have adjusted
				 * b_bcount without allocating a new buffer,
				 * we would have had problems with
				 * hba drivers that use bp_mapin during
				 * tran_init_pkt.
				 */
				secnt = (bp->b_bcount +
					(DEV_BSIZE - 1)) >> DEV_BSHIFT;
				count = MIN(secnt, (lp->dkl_nblk - bn));
				if (count != secnt) {
					if (count >= 0) {
						/*
						 * We have an overrun
						 */
						resid = (secnt - count)
							<< DEV_BSHIFT;
						SD_DEBUG2(SD_DEVINFO, sd_label,
						    SCSI_DEBUG,
						    "overrun by %ld sectors\n",
						    secnt - count);
						if (resid) {
						    if ((bp->b_bcount - resid)
							& (XDEV_BSIZE - 1))
								i = -1;
						    else {
							bp = sd_overrun(bp,
									resid);
						    }
						}
					} else {
						SD_DEBUG2(SD_DEVINFO, sd_label,
						    SCSI_DEBUG,
				"I/O attempted beyond the end of partition");
						i = -1;
					}
				}
				/*
				 * sort by absolute block number.
				 */
				bp->b_resid = bn;
				if (un->un_lbasize == DEV_BSIZE) {
					bp->b_resid += un->un_offset[part];
				}

				/*
				 * zero out av_back - this will be a signal
				 * to sdstart to go and fetch the resources
				 */
				bp->av_back = NO_PKT_ALLOCATED;
			}

			/*
			 * Check to see whether or not we are done
			 * (with or without errors).
			 */

			if (i != 0) {
				if (i < 0) {
					bp->b_flags |= B_ERROR;
				}
				goto error;
			}


			if (un->un_lbasize != DEV_BSIZE) {
				if (nbp = sd_nodevbsize_blksize(bp, un)) {
					bp = nbp;
					bp->b_resid = dkblock(bp);
				}
			}
		} else {
			/*
			 * opened in NDELAY/NONBLOCK mode?
			 * Check if disk is ready and has a valid geometry
			 */
			if ((sd_ready_and_valid(bp->b_edev, un)
			    == SD_READY_VALID)) {
				goto validated;
			} else {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"i/o to invalid geometry\n");
				SET_BP_ERROR(bp, EIO);
				goto error;
			}
		}
	}

	/*
	 * We are doing it a bit non-standard. That is, the
	 * head of the b_actf chain is *not* the active command-
	 * it is just the head of the wait queue. The reason
	 * we do this is that the head of the b_actf chain is
	 * guaranteed to not be moved by disksort(), so that
	 * our restart command (pointed to by
	 * b_forw) and the head of the wait queue (b_actf) can
	 * have resources granted without it getting lost in
	 * the queue at some later point (where we would have
	 * to go and look for it).
	 */
	mutex_enter(SD_MUTEX);

	SD_DO_KSTATS(un, kstat_waitq_enter, bp);

	dp = &un->un_utab;

	if (dp->b_actf == NULL) {
		dp->b_actf = bp;
		dp->b_actl = bp;
	} else {
		TRACE_3(TR_FAC_SCSI, TR_SDSTRATEGY_DISKSORT_START,
		"sdstrategy_disksort_start: dp %x bp %x un_ncmds %d",
		    dp, bp, un->un_ncmds);
		disksort(dp, bp);
		TRACE_0(TR_FAC_SCSI, TR_SDSTRATEGY_DISKSORT_END,
		    "sdstrategy_disksort_end");
	}


	ASSERT(un->un_ncmds >= 0);
	ASSERT(un->un_throttle >= 0);
	if ((un->un_ncmds < un->un_throttle) && (dp->b_forw == NULL)) {
		sdstart(un);
	} else if (BP_HAS_NO_PKT(dp->b_actf)) {
		struct buf *cmd_bp;

		cmd_bp = dp->b_actf;
		cmd_bp->av_back = ALLOCATING_PKT;
		mutex_exit(SD_MUTEX);
		/*
		 * try and map this one
		 */
		TRACE_0(TR_FAC_SCSI, TR_SDSTRATEGY_SMALL_WINDOW_START,
		    "sdstrategy_small_window_call (begin)");

		if (make_sd_cmd(un, cmd_bp, NULL_FUNC) == PKT_WAS_TOO_SMALL) {

			mutex_enter(SD_MUTEX);

			ASSERT(cmd_bp == un->un_sbufp);

			/* remove from active queue */

			dp->b_actf = cmd_bp->b_actf;
			cmd_bp->b_actf = 0;

			cmd_bp->av_back = NO_PKT_ALLOCATED;
			cmd_bp->b_resid = cmd_bp->b_bcount;
			SD_DO_KSTATS(un, kstat_waitq_exit, cmd_bp);

			mutex_exit(SD_MUTEX);

			biodone(cmd_bp);
			(void) pm_idle_component(SD_DEVINFO, 0);
		}

		TRACE_0(TR_FAC_SCSI, TR_SDSTRATEGY_SMALL_WINDOW_END,
		    "sdstrategy_small_window_call (end)");

		/*
		 * there is a small window where the active cmd
		 * completes before make_sd_cmd returns.
		 * consequently, this cmd never gets started so
		 * we start it from here
		 */
		mutex_enter(SD_MUTEX);
		if ((un->un_ncmds < un->un_throttle) &&
		    (dp->b_forw == NULL)) {
			sdstart(un);
		}
	}
	mutex_exit(SD_MUTEX);

done:
	TRACE_0(TR_FAC_SCSI, TR_SDSTRATEGY_END, "sdstrategy_end");
	return (0);
}

struct buf *
sd_shadow_iodone(struct buf *bp)
{
	struct buf *obp;
	caddr_t src_addr;
	offset_t edatap, oedatap;
	size_t copy_count, begin_byte;
	struct scsi_disk *un;
	int partition = SDPART(bp->b_edev);

	un = ddi_get_soft_state(sd_state, SDUNIT(bp->b_edev));

	begin_byte = dkblock(bp) * un->un_lbasize;
	if (bp->b_iodone != (int (*)())sd_shadow_iodone) {
		bp->b_blkno = begin_byte >> DEV_BSHIFT;
		bp->b_blkno -= un->un_offset[partition];
		return (bp);
	}

	obp = bp->b_private;

	edatap = begin_byte + bp->b_bcount - bp->b_resid;
	oedatap = (dkblock(obp) + un->un_offset[partition]) << DEV_BSHIFT;
	src_addr = bp->b_un.b_addr + oedatap - begin_byte;
	oedatap +=  obp->b_bcount;
	copy_count = obp->b_bcount;

	obp->b_resid = 0;
	if (edatap < oedatap) {
		if ((oedatap - edatap) > obp->b_bcount) {
			/*
			 * we could not read any portion of
			 * the original request (obp)
			 */
			obp->b_resid = obp->b_bcount;
		} else {
			obp->b_resid = oedatap - edatap;
		}
		copy_count -= obp->b_resid;
		obp->b_error = bp->b_error;
		obp->b_oerror = bp->b_oerror;
	}
	bcopy(src_addr, obp->b_un.b_addr, copy_count);

	kmem_free(bp->b_un.b_addr, bp->b_bcount);
	freerbuf(bp);

	return (obp);
}

/*
 * We get into picture only if secsize != lbasize
 * We allocate a local buffer and issue a shadow
 * command covering the original command and
 * aligned at the lbasize and multiple of lbasize.
 */
struct buf *
sd_nodevbsize_blksize(struct buf *bp, struct scsi_disk *un)
{
	struct buf *nbp;
	daddr_t begin_blk, end_blk;
	size_t begin_byte, end_byte, count;
	unsigned long blkno, eblk;
	int partition = SDPART(bp->b_edev);
	struct dk_map *lp = &un->un_map[partition];

	mutex_enter(SD_MUTEX);
	if (un->un_lbasize != DEV_BSIZE) {
		blkno = un->un_offset[partition] + dkblock(bp);
		begin_byte = blkno << DEV_BSHIFT;
		eblk = dkblock(bp) + (bp->b_bcount >> DEV_BSHIFT);

		if ((begin_byte % un->un_lbasize) ||
			(bp->b_bcount % un->un_lbasize) ||
			(eblk > lp->dkl_nblk)) {

			/* We never reach this point for ROD devices, since */
			/* we do not support non-block-aligned transfers */

			mutex_exit(SD_MUTEX);

			/*
			 * Note that the check for overrun is already done
			 * in sdstrategy, and necessary actions are taken
			 * for it before getting here.
			 * At this point there would be no resid.
			 */
			count = bp->b_bcount;

			if (bp->b_flags & (B_PAGEIO|B_PHYS)) {
				bp_mapin(bp);
			}

			end_byte = begin_byte +
				count + un->un_lbasize - 1;
			begin_blk = begin_byte / un->un_lbasize;
			end_blk = end_byte / un->un_lbasize;

			count = (end_blk - begin_blk) * un->un_lbasize;

			nbp = getrbuf(KM_SLEEP);

			nbp->b_un.b_addr = kmem_zalloc(count, KM_SLEEP);
			nbp->b_bcount = count;
			nbp->b_flags |= B_READ;
			nbp->b_dev = bp->b_dev;
			nbp->b_blkno = begin_blk;
			nbp->b_iodone = (int (*)())sd_shadow_iodone;
			nbp->b_private = bp;
			nbp->b_edev = bp->b_edev;
			nbp->b_resid = 0;
			return (nbp);
		}
		bp->b_blkno = begin_byte / un->un_lbasize;
	}
	mutex_exit(SD_MUTEX);
	return (NULL);
}


/*
 * This function is called if the request expanded over the end
 * of partition. A local buffer is allocated  with a count of
 * original request subtracted by the overrun amount.
 */
struct buf *
sd_overrun(struct buf *bp, size_t resid)
{
	size_t count;
	struct buf *nbp;

	if (bp->b_flags & (B_PAGEIO|B_PHYS)) {
		bp_mapin(bp);
	}
	nbp = getrbuf(KM_SLEEP);

	ASSERT(bp->b_bcount >= resid);

	count = bp->b_bcount - resid;
	nbp->b_un.b_addr = kmem_zalloc(count, KM_SLEEP);
	nbp->b_bcount = count;
	nbp->b_flags = bp->b_flags;
	nbp->b_dev = bp->b_dev;
	nbp->b_blkno = bp->b_blkno;
	nbp->b_iodone = (int (*)()) sd_overrun_iodone;
	nbp->b_private = bp;
	nbp->b_edev = bp->b_edev;
	return (nbp);
}


/*
 * This is the iodone function for the buf allocated with sd_overrun().
 */
struct buf *
sd_overrun_iodone(struct buf *bp)
{
	struct buf *obp;
	size_t copy_count;

	obp = bp->b_private;

	copy_count = bp->b_bcount - bp->b_resid;
	bcopy(bp->b_un.b_addr, obp->b_un.b_addr, copy_count);
	obp->b_resid = obp->b_bcount - copy_count;
	obp->b_error = bp->b_error;
	obp->b_oerror = bp->b_oerror;

	kmem_free(bp->b_un.b_addr, bp->b_bcount);
	freerbuf(bp);

	return (obp);
}


/*
 * Unit start and Completion
 * NOTE: we assume that the caller has at least checked for:
 *		(un->un_ncmds < un->un_throttle)
 *	if not, there is no real harm done, scsi_transport() will
 *	return BUSY
 */
static void
sdstart(struct scsi_disk *un)
{
	register int status;
	register struct buf *bp;
	register struct diskhd *dp;
	register uchar_t state = un->un_last_state;

	TRACE_1(TR_FAC_SCSI, TR_SDSTART_START, "sdstart_start: un %x", un);

retry:
	ASSERT(mutex_owned(SD_MUTEX));

	dp = &un->un_utab;
	if (((bp = dp->b_actf) == NULL) ||
		(bp->av_back == ALLOCATING_PKT) ||
		(dp->b_forw != NULL) ||
		(un->un_state == SD_STATE_DUMPING)) {
		TRACE_0(TR_FAC_SCSI, TR_SDSTART_NO_WORK_END,
		    "sdstart_end (no work)");
		return;
	}

	/*
	 * remove from active queue
	 */
	dp->b_actf = bp->b_actf;
	bp->b_actf = 0;

	/*
	 * increment ncmds before calling scsi_transport because sdintr
	 * may be called before we return from scsi_transport!
	 */
	un->un_ncmds++;

	/*
	 * If measuring stats, mark exit from wait queue and
	 * entrance into run 'queue' if and only if we are
	 * going to actually start a command.
	 * Normally the bp already has a packet at this point
	 */
	SD_DO_KSTATS(un, kstat_waitq_to_runq, bp);

	/*
	 * we assume that things go well and transition to NORMAL
	 */
	New_state(un, SD_STATE_NORMAL);

	mutex_exit(SD_MUTEX);

	if (BP_HAS_NO_PKT(bp)) {
		if (make_sd_cmd(un, bp, sdrunout) == PKT_WAS_TOO_SMALL) {

			mutex_enter(SD_MUTEX);

			ASSERT(bp == un->un_sbufp);

			/* restore old state */

			un->un_state = un->un_last_state;
			un->un_last_state = state;

			SD_DO_KSTATS(un, kstat_runq_exit, bp);
			un->un_ncmds--;

			bp->av_back = NO_PKT_ALLOCATED;
			bp->b_resid = bp->b_bcount;

			mutex_exit(SD_MUTEX);

			biodone(bp);

			(void) pm_idle_component(SD_DEVINFO, 0);

			mutex_enter(SD_MUTEX);

			if ((un->un_ncmds < un->un_throttle) &&
			    (dp->b_forw == NULL)) {
				goto retry;
			} else {
				goto done;
			}
		}

		if (BP_HAS_NO_PKT(bp) && !(bp->b_flags & B_ERROR)) {
			mutex_enter(SD_MUTEX);
			SD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);

			bp->b_actf = dp->b_actf;
			dp->b_actf = bp;
			New_state(un, SD_STATE_RWAIT);
			un->un_ncmds--;

			TRACE_0(TR_FAC_SCSI, TR_SDSTART_NO_RESOURCES_END,
				"sdstart_end (No Resources)");
			goto done;

		} else if (bp->b_flags & B_ERROR) {
			mutex_enter(SD_MUTEX);
			SD_DO_KSTATS(un, kstat_runq_exit, bp);

			un->un_ncmds--;
			bp->b_resid = bp->b_bcount;
			if (bp->b_error == 0) {
				SET_BP_ERROR(bp, EIO);
			}

			/*
			 * restore old state
			 */
			un->un_state = un->un_last_state;
			un->un_last_state = state;

			mutex_exit(SD_MUTEX);

			if (un->un_lbasize != DEV_BSIZE) {
				bp = sd_shadow_iodone(bp);
			}

			if (bp->b_iodone == (int (*)())sd_overrun_iodone) {
				bp = sd_overrun_iodone(bp);
			}

			biodone(bp);

			(void) pm_idle_component(SD_DEVINFO, 0);

			mutex_enter(SD_MUTEX);
			if (un->un_state == SD_STATE_SUSPENDED) {
				cv_broadcast(&un->un_disk_busy_cv);
			}

			if ((un->un_ncmds < un->un_throttle) &&
			    (dp->b_forw == NULL)) {
				goto retry;
			} else {
				goto done;
			}
		}
	}


	/*
	 * Restore resid from the packet, b_resid had been the
	 * disksort key.
	 */
	bp->b_resid = BP_PKT(bp)->pkt_resid;

	BP_PKT(bp)->pkt_resid = 0;

	/*
	 * We used to check whether or not to try and link commands here.
	 * Since we have found that there is no performance improvement
	 * for linked commands, this has not made much sense.
	 */
	if ((status = scsi_transport(BP_PKT(bp))) != TRAN_ACCEPT) {
		mutex_enter(SD_MUTEX);
		un->un_ncmds--;
		if (status == TRAN_BUSY) {
			SD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
			sd_handle_tran_busy(bp, dp, un);
		} else {
			SD_DO_ERRSTATS(un, sd_transerrs);
			SD_DO_KSTATS(un, kstat_runq_exit, bp);
			mutex_exit(SD_MUTEX);

			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "transport rejected (%d)\n",
			    status);
			SET_BP_ERROR(bp, EIO);
			bp->b_resid = bp->b_bcount;
			if (bp != un->un_sbufp) {
				scsi_destroy_pkt(BP_PKT(bp));
			}

			if (un->un_lbasize != DEV_BSIZE) {
				bp = sd_shadow_iodone(bp);
			}

			if (bp->b_iodone == (int (*)())sd_overrun_iodone) {
				bp = sd_overrun_iodone(bp);
			}

			biodone(bp);
			(void) pm_idle_component(SD_DEVINFO, 0);

			mutex_enter(SD_MUTEX);
			if (un->un_state == SD_STATE_SUSPENDED) {
				cv_broadcast(&un->un_disk_busy_cv);
			}
			if ((un->un_ncmds < un->un_throttle) &&
				(dp->b_forw == NULL)) {
					goto retry;
			}
		}
	} else {
		mutex_enter(SD_MUTEX);

		if (dp->b_actf && BP_HAS_NO_PKT(dp->b_actf)) {
			struct buf *cmd_bp;

			cmd_bp = dp->b_actf;
			cmd_bp->av_back = ALLOCATING_PKT;
			mutex_exit(SD_MUTEX);
			/*
			 * try and map this one
			 */
			TRACE_0(TR_FAC_SCSI, TR_SDSTART_SMALL_WINDOW_START,
			    "sdstart_small_window_start");

			if (make_sd_cmd(un, cmd_bp, NULL_FUNC) ==
							PKT_WAS_TOO_SMALL) {

				mutex_enter(SD_MUTEX);

				ASSERT(cmd_bp == un->un_sbufp);

				/* remove from active queue */

				dp->b_actf = cmd_bp->b_actf;
				cmd_bp->b_actf = 0;

				SD_DO_KSTATS(un, kstat_waitq_exit, cmd_bp);
				un->un_ncmds--;

				cmd_bp->av_back = NO_PKT_ALLOCATED;
				cmd_bp->b_resid = cmd_bp->b_bcount;

				mutex_exit(SD_MUTEX);

				biodone(cmd_bp);
				(void) pm_idle_component(SD_DEVINFO, 0);

			}

			TRACE_0(TR_FAC_SCSI, TR_SDSTART_SMALL_WINDOW_END,
			    "sdstart_small_window_end");
			/*
			 * there is a small window where the active cmd
			 * completes before make_sd_cmd returns.
			 * consequently, this cmd never gets started so
			 * we start it from here
			 */
			mutex_enter(SD_MUTEX);
			if ((un->un_ncmds < un->un_throttle) &&
			    (dp->b_forw == NULL)) {
				goto retry;
			}
		}
	}

done:
	ASSERT(mutex_owned(SD_MUTEX));
	TRACE_0(TR_FAC_SCSI, TR_SDSTART_END, "sdstart_end");
}

/*
 * make_sd_cmd: create a pkt
 */
static struct buf *
make_sd_cmd(struct scsi_disk *un, struct buf *bp, int (*func)())
{
	auto int com;
	auto size_t count;
	register struct scsi_pkt *pkt;
	register struct sd_pkt_private  *sdpp;
	register int flags, tval;
	int cdbsize;
	uchar_t ebpbit = 0;

	TRACE_3(TR_FAC_SCSI, TR_MAKE_SD_CMD_START,
	    "make_sd_cmd_start: un %x bp %x un_ncmds %d",
	    un, bp, un->un_ncmds);


	flags = un->un_cmd_flags;

#ifdef B_ORDER
	/*
	 * if ordered cmd flag set, force this tagged cmd to be
	 * ordered too.	 For untagged I/O, disksort takes care if
	 * things.
	 * XXX drives cannot handle this reliably so don't enable
	 * this flag
	 */
	if ((bp->b_flags & B_ORDER) && (flags & FLAG_TAGMASK))
		flags |= (flags & ~FLAG_TAGMASK) | FLAG_OTAG;
#endif

	if (bp != un->un_sbufp) {
		register int partition = SDPART(bp->b_edev);
		daddr_t blkno;
		int  cmd_flags;

		/*
		 * If cdrom & lbasize != DEV_BSIZE
		 * end of partition check already done in
		 * sd_nodevbsize_blksize
		 */
		blkno = dkblock(bp);
		if (un->un_lbasize != DEV_BSIZE) {
			count = bp->b_bcount / un->un_lbasize;
			cmd_flags = PKT_CONSISTENT;
		} else {
			/*
			 * Adjust block number to absolute
			 */
			blkno += un->un_offset[partition];
			count = (bp->b_bcount + (DEV_BSIZE - 1)) >> DEV_BSHIFT;
			cmd_flags = 0;
		}

		/*
		 * Allocate a scsi packet.
		 * We are called in this case with
		 * the disk mutex held, but func is
		 * either NULL_FUNC or sdrunout,
		 * so we do not have to release
		 * the disk mutex here.
		 */

		if (((blkno + count - 1) >= (2 << 20)) || (count > 0xff) ||
		    SD_GRP1_2_CDBS(un) || ISROD(un))
			cdbsize = CDB_GROUP1;
		else
			cdbsize = CDB_GROUP0;

		if (ISREMOVABLE(un))
		    cdbsize = (count > (size_t)0xffff) ? CDB_GROUP5 :
					    CDB_GROUP1;
		TRACE_0(TR_FAC_SCSI, TR_MAKE_SD_CMD_INIT_PKT_START,
		    "make_sd_cmd_init_pkt_call (begin)");
		pkt = scsi_init_pkt(ROUTE, NULL, bp, cdbsize,
		    un->un_cmd_stat_size, PP_LEN, cmd_flags | PKT_DMA_PARTIAL,
		    func, (caddr_t)un);
		TRACE_1(TR_FAC_SCSI, TR_MAKE_SD_CMD_INIT_PKT_END,
		    "make_sd_cmd_init_pkt_call (end): pkt %x", pkt);
		if (!pkt) {
			bp->av_back = NO_PKT_ALLOCATED;
			TRACE_0(TR_FAC_SCSI,
			    TR_MAKE_SD_CMD_NO_PKT_ALLOCATED1_END,
			    "make_sd_cmd_end (NO_PKT_ALLOCATED1)");
			return (NO_PKT_ALLOCATED);
		}

		/*
		 * Keep track of the resid of the DMA resources
		 * allocated and the total transferred per cmd.
		 */
		sdpp = PKT_PRIVATE(pkt);
		sdpp->sdpp_dma_resid = pkt->pkt_resid;
		sdpp->sdpp_cdblen = cdbsize;
		if (pkt->pkt_resid != 0) {
			count -= (pkt->pkt_resid >> un->un_secdiv);
		}

		if (bp->b_flags & B_READ) {
			com = SCMD_READ;
		} else {
			com = SCMD_WRITE;
			if ((ISROD(un)) && (un->ebp_enabled))
				ebpbit = 0x04;
		}

		/*
		 * Save the resid in the packet, temporarily until
		 * we transport the command.
		 * The resid should really be zero since we have overrun
		 * functions now.
		 */
		pkt->pkt_resid = 0;

		/*
		 * XXX: This needs to be reworked based upon lbasize
		 *
		 * If the block number or the sector count exceeds the
		 * capabilities of a Group 0 command, shift over to a
		 * Group 1 command. We don't blindly use use Group 1
		 * commands because a) some drives (CDC Wren IVs) get a
		 * bit confused, and b)* there is probably a fair amount
		 * of speed difference for a target to receive and decode
		 * a 10 byte command instead of a 6 byte command.
		 *
		 * the xfer time difference of 6 vs 10 byte CDBs is
		 * still significant so this code is still worthwhile
		 */

		if (cdbsize == CDB_GROUP1) {
			com |= SCMD_GROUP1;
			(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				com, (int)blkno, count, 0);
			FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
			pkt->pkt_cdbp[1] |= ebpbit;
		} else if (cdbsize == CDB_GROUP5) {
			com |= SCMD_GROUP5;
			(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				com, (int)blkno, count, 0);
			FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		} else {
			(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				com, (int)blkno, count, 0);
			FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		}
		pkt->pkt_flags = flags | un->un_tagflags;
		tval = un->un_cmd_timeout;
	} else {
		struct uscsi_cmd *scmd = (struct uscsi_cmd *)bp->b_forw;

		/*
		* Set options.
		*/
		if ((scmd->uscsi_flags & USCSI_SILENT) && !(DEBUGGING)) {
			flags |= FLAG_SILENT;
		}
		if (scmd->uscsi_flags & USCSI_ISOLATE)
			flags |= FLAG_ISOLATE;
		if (scmd->uscsi_flags & USCSI_DIAGNOSE)
			flags |= FLAG_DIAGNOSE;

		/*
		* Set the pkt flags here so we save time later.
		*/
		if (scmd->uscsi_flags & USCSI_HEAD)
			flags |= FLAG_HEAD;
		if (scmd->uscsi_flags & USCSI_NOINTR)
			flags |= FLAG_NOINTR;

		/*
		 * For tagged queueing, things get a bit complicated.
		 * Check first for head of que and last for ordered que.
		 * If neither head nor order, use the default driver tag flags.
		 */

		if ((scmd->uscsi_flags & USCSI_NOTAG) == 0) {
			if (scmd->uscsi_flags & USCSI_HTAG)
				flags |= FLAG_HTAG;
			else if (scmd->uscsi_flags & USCSI_OTAG)
				flags |= FLAG_OTAG;
			else
				flags |= un->un_tagflags & FLAG_TAGMASK;
		}
		if (scmd->uscsi_flags & USCSI_NODISCON)
			flags = (flags & ~FLAG_TAGMASK) | FLAG_NODISCON;

		TRACE_0(TR_FAC_SCSI, TR_MAKE_SD_CMD_INIT_PKT_SBUF_START,
		    "make_sd_cmd_init_pkt_sbuf_call (begin)");

		pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
		    (bp->b_bcount)? bp: NULL,
		    scmd->uscsi_cdblen, un->un_cmd_stat_size, PP_LEN,
		    PKT_DMA_PARTIAL, func, (caddr_t)un);

		TRACE_1(TR_FAC_SCSI, TR_MAKE_SD_CMD_INIT_PKT_SBUF_END,
		    "make_sd_cmd_init_pkt_sbuf_call (end): pkt %x", pkt);

		if (!pkt) {
			bp->av_back = NO_PKT_ALLOCATED;
			TRACE_0(TR_FAC_SCSI,
			    TR_MAKE_SD_CMD_NO_PKT_ALLOCATED2_END,
			    "make_sd_cmd_end (NO_PKT_ALLOCATED2)");
			return (NO_PKT_ALLOCATED);
		}


		if ((bp->b_bcount) && (pkt->pkt_resid != 0)) {

			/*
			 * Could not get all resources it needs
			 * to be set in one xfer. This for the case
			 * of a USCSI command where the buffer requested
			 * can not be setup without DMA break up
			 * at this point of time, and we do not want
			 * to do a  DMA break up for a USCSI command.
			 */

			/*
			 * For the case where bp is still on active queue
			 * we can not set the av_back to NO_PKT_ALLOCATED
			 * since there is a chance that another make_sd_cmd
			 * would be tried on this buf before the
			 * caller of current make_sd_cmd has a chance to
			 * process the return code and remove from the
			 * active queue.
			 */

			scsi_destroy_pkt(pkt);
			return (PKT_WAS_TOO_SMALL);
		}
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				scmd->uscsi_cdb[0], 0, 0, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		pkt->pkt_flags = flags;
		bcopy(scmd->uscsi_cdb,
			(caddr_t)pkt->pkt_cdbp, scmd->uscsi_cdblen);
		if (scmd->uscsi_timeout == 0)
			/*
			 * some CD cmds take a long time so increase the
			 * the standard timeout
			 */
			tval = ((ISCD(un))? 2: 1) * un->un_cmd_timeout;
		else
			tval = scmd->uscsi_timeout;
	}

	pkt->pkt_comp = sdintr;
	pkt->pkt_time = tval;
	PKT_SET_BP(pkt, bp);
	bp->av_back = (struct buf *)pkt;

	TRACE_0(TR_FAC_SCSI, TR_MAKE_SD_CMD_END, "make_sd_cmd_end");
	return ((struct buf *)pkt);
}

/*
 * Command completion processing
 */
static void
sdintr(struct scsi_pkt *pkt)
{
	register struct scsi_disk *un;
	register struct buf *bp;
	int action;
	int status;

	bp = PKT_GET_BP(pkt);
	un = ddi_get_soft_state(sd_state, SDUNIT(bp->b_edev));

	TRACE_1(TR_FAC_SCSI, TR_SDINTR_START, "sdintr_start: un %x", un);
	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "sdintr\n");

	mutex_enter(SD_MUTEX);
	un->un_ncmds--;
	SD_DO_KSTATS(un, kstat_runq_exit, bp);
	ASSERT(un->un_ncmds >= 0);

	un->un_rqs_state &= ~(SD_RQS_ERROR);

	/*
	 * do most common case first
	 */
	if ((pkt->pkt_reason == CMD_CMPLT) &&
	    (SCBP_C(pkt) == 0) &&
	    ((pkt->pkt_flags & FLAG_SENSING) == 0)) {
		/*
		 * Get the low order bits of the command byte
		 */
		int com = GETCMD((union scsi_cdb *)pkt->pkt_cdbp);

		if (un->un_state == SD_STATE_OFFLINE) {
			un->un_state = un->un_last_state;
			scsi_log(SD_DEVINFO, sd_label, CE_NOTE, diskokay);
		}
		/*
		 * If the command is a read or a write, and we have
		 * a non-zero pkt_resid, that is an error. We should
		 * attempt to retry the operation if possible.
		 */
		action = COMMAND_DONE;
		if (pkt->pkt_resid && (com == SCMD_READ || com == SCMD_WRITE)) {
			SD_DO_ERRSTATS(un, sd_harderrs);
			if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
				PKT_INCR_RETRY_CNT(pkt, 1);
				action = QUE_COMMAND;
			} else {
				/*
				 * if we have exhausted retries
				 * a command with a residual is in error in
				 * this case.
				 */
				action = COMMAND_DONE_ERROR;
			}
			if (!(pkt->pkt_flags & FLAG_SILENT)) {
				scsi_log(SD_DEVINFO, sd_label,
				    CE_WARN, "incomplete %s- %s\n",
				    (bp->b_flags & B_READ)? "read" : "write",
				    (action == QUE_COMMAND)? "retrying" :
				    "giving up");
			}
		}

		/*
		 * pkt_resid will reflect, at this point, a residual
		 * of how many bytes left to be transferred there were
		 * from the last scsi command with this packet.
		 * Add this and dma_resid (amount left to be done for
		 * dma break up) to b_resid, the amount this driver
		 * could not see to transfer, to get the total number
		 * of bytes not transfered.
		 *
		 */

		if (action != QUE_COMMAND) {
			bp->b_resid += pkt->pkt_resid +
				PKT_PRIVATE(pkt)->sdpp_dma_resid;
		}

	} else if ((pkt->pkt_reason != CMD_CMPLT) &&
		(pkt->pkt_flags & FLAG_SENSING)) {
		/*
		 * We were running a REQUEST SENSE which failed; rerun
		 * the original command, if possible
		 * Release the semaphore for req sense pkt
		 */
		SD_DO_ERRSTATS(un, sd_harderrs);
		sema_v(&un->un_rqs_sema);
		pkt = BP_PKT(bp);
		pkt->pkt_flags &= ~FLAG_SENSING;
		if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			action = QUE_COMMAND;
		} else {
			action = COMMAND_DONE_ERROR;
		}

	} else if (pkt->pkt_reason != CMD_CMPLT) {

		action = sd_handle_incomplete(un, bp);

	} else if (un->un_arq_enabled &&
	    (pkt->pkt_state & STATE_ARQ_DONE)) {
		/*
		 * the transport layer successfully completed an autorqsense
		 */
		action = sd_handle_autosense(un, bp);

	} else if (pkt->pkt_flags & FLAG_SENSING) {
		/*
		 * We were running a REQUEST SENSE. Find out what to do next.
		 * Release the semaphore for req sense pkt
		 */
		pkt = BP_PKT(bp);
		pkt->pkt_flags &= ~FLAG_SENSING;
		sema_v(&un->un_rqs_sema);
		action = sd_handle_sense(un, bp);
	} else {
		/*
		 * check to see if the status bits were okay.
		 */
		action = sd_check_error(un, bp);
	}

	/*
	 * If we are suspended, then return command to head of the queue
	 * since we don't want to start more commands.
	 */
	if (((un->un_state == SD_STATE_SUSPENDED) ||
	    (un->un_state == SD_STATE_DUMPING)) &&
	    (action == QUE_SENSE || action == QUE_COMMAND)) {
		if (action == QUE_COMMAND) {
			struct diskhd *dp = &un->un_utab;
			int part = getminor(bp->b_edev) & SDPART_MASK;
			BP_PKT(bp)->pkt_resid = bp->b_resid;
			bp->b_resid = dkblock(bp);
			if (!ISCD(un)) {
				bp->b_resid += un->un_offset[part];
			}
			bp->b_actf = dp->b_actf;
			dp->b_actf = bp;
			if (dp->b_forw == bp) {
				dp->b_forw = NULL;
			}
			action = JUST_RETURN;
		} else {
			/* We are SUSPENDED, fail the command for QUE_SENSE */
			action = COMMAND_DONE_ERROR;
		}
	}

	/*
	 * save pkt reason; consecutive failures are not reported unless
	 * fatal
	 * do not reset last_pkt_reason when the cmd was retried and
	 * succeeded because
	 * there maybe more commands comming back with last_pkt_reason
	 */
	if ((un->un_last_pkt_reason != pkt->pkt_reason) &&
	    ((pkt->pkt_reason != CMD_CMPLT) ||
	    (PKT_GET_RETRY_CNT(pkt) == 0))) {
		un->un_last_pkt_reason = pkt->pkt_reason;
	}

	switch (action) {
	case COMMAND_DONE_ERROR:
error:
		if (bp->b_resid == 0) {
			bp->b_resid = bp->b_bcount;
		}
		if (bp->b_error == 0) {
			SET_BP_ERROR(bp, EIO);
		}
		bp->b_flags |= B_ERROR;

		if (!(un->un_rqs_state & SD_RQS_ERROR) &&
			(bp->b_error == EIO)) {
			un->un_rqs_state &= ~(SD_RQS_VALID);
		}
		/*FALLTHROUGH*/
	case COMMAND_DONE:
		if (bp != un->un_sbufp && geterror(bp) == 0 &&
			PKT_PRIVATE(pkt)->sdpp_dma_resid != 0 &&
			pkt->pkt_resid == 0) {

			if (sd_setup_next_xfer(un, bp, pkt, PKT_PRIVATE(pkt))) {

				/*
				 * succeed setting up next portion of cmd
				 * transfer, try sending it
				 */
				goto queue_next;
			}
			/*
			 * Error setting up next portion of cmd transfer
			 */

		}
		sddone_and_mutex_exit(un, bp);

		TRACE_0(TR_FAC_SCSI, TR_SDINTR_COMMAND_DONE_END,
		    "sdintr_end (COMMAND_DONE)");
		return;

	case QUE_SENSE:
		if (un->un_ncmds >= un->un_throttle) {
			/*
			 * we can't do request sense now, so queue up
			 * the original cmd
			 */
			sd_requeue_cmd(un, bp, SD_BSY_TIMEOUT);
			SD_DO_KSTATS(un, kstat_waitq_enter, bp);

			mutex_exit(SD_MUTEX);
			return;
		}
		pkt->pkt_flags |= FLAG_SENSING;
		SD_DO_KSTATS(un, kstat_runq_enter, bp);

		/*
		 * We grab a semaphore before using the request pkt.
		 */
		mutex_exit(SD_MUTEX);
		sema_p(&un->un_rqs_sema);
		mutex_enter(SD_MUTEX);
		PKT_SET_BP(un->un_rqs, bp);
		bzero(SD_RQSENSE, SENSE_LENGTH);
		un->un_ncmds++;
		mutex_exit(SD_MUTEX);
		if ((status = scsi_transport(un->un_rqs)) != TRAN_ACCEPT) {
			/*
			 * this should never return BUSY; it should go to head
			 * of the queue
			 */
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "transport of request sense fails (%x)\n", status);
			mutex_enter(SD_MUTEX);
			SD_DO_KSTATS(un, kstat_runq_exit, bp);
			un->un_ncmds--;
			Restore_state(un);
			sema_v(&un->un_rqs_sema);
			goto error;
		}
		break;

	case QUE_COMMAND:

queue_next:
		if (un->un_ncmds >= un->un_throttle) {
			/*
			 * restart command
			 */
			sd_requeue_cmd(un, bp, SD_BSY_TIMEOUT);
			SD_DO_KSTATS(un, kstat_waitq_enter, bp);

			mutex_exit(SD_MUTEX);
			goto exit;
		}

		/*
		 * Delayed retry if un_err_delay is set
		 */
		if (un->un_err_delay) {
			sd_requeue_cmd(un, bp, (clock_t)un->un_err_delay);
			SD_DO_KSTATS(un, kstat_waitq_enter, bp);
			mutex_exit(SD_MUTEX);
			goto exit;
		}

		un->un_ncmds++;
		SD_DO_KSTATS(un, kstat_runq_enter, bp);
		mutex_exit(SD_MUTEX);
		if ((status = scsi_transport(BP_PKT(bp))) != TRAN_ACCEPT) {
			register struct diskhd *dp = &un->un_utab;
			mutex_enter(SD_MUTEX);
			un->un_ncmds--;
			if (status == TRAN_BUSY) {
				SD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
				sd_handle_tran_busy(bp, dp, un);
				mutex_exit(SD_MUTEX);
				goto exit;
			}
			SD_DO_ERRSTATS(un, sd_transerrs);
			SD_DO_KSTATS(un, kstat_runq_exit, bp);

			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "requeue of command fails (%x)\n", status);
			SET_BP_ERROR(bp, EIO);
			bp->b_resid = bp->b_bcount;

			sddone_and_mutex_exit(un, bp);
			goto exit;
		}
		break;

	case JUST_RETURN:
	default:
		SD_DO_KSTATS(un, kstat_waitq_enter, bp);
		mutex_exit(SD_MUTEX);
		break;
	}

exit:
	TRACE_0(TR_FAC_SCSI, TR_SDINTR_END, "sdintr_end");
}


/*
 * Restart command if it is the active one, else put it on the wait
 * Queue to be started later.
 */

static void
sd_requeue_cmd(struct scsi_disk *un, struct buf *bp, clock_t tval)
{
	register struct diskhd *dp = &un->un_utab;

	/*
	 * since we are restarting the command, reset the pkt_resid
	 * to contain the originial b_resid value
	 */
	BP_PKT(bp)->pkt_resid = bp->b_resid;

	/*
	 * restart if it is the active command, else
	 * put it on the wait Queue
	 */

	if ((dp->b_forw == NULL) || (dp->b_forw == bp)) {
		dp->b_forw = bp;
		un->un_reissued_timeid = timeout(sdrestart, un, tval);
	} else if (dp->b_forw != bp) {
		/*
		 * put it back on the queue
		 */
		if (bp != un->un_sbufp) {
			register int part =
				(getminor(bp->b_edev)) & SDPART_MASK;
			/*
			 * Now as we step thru' sdrestart() again,
			 * b_resid instead of holding some junk
			 * i.e pkt_resid, could better hold absolute
			 * block number - so that other buffers going
			 * thru' disksort() aren't 'too badly' affected.
			 */
			bp->b_resid = dkblock(bp);
			if (un->un_lbasize == DEV_BSIZE) {
				bp->b_resid += un->un_offset[part];
			}
		}
		bp->b_actf = dp->b_actf;
		dp->b_actf = bp;
	}
}

/*
 * Done with a command.
 */
static void
sddone_and_mutex_exit(struct scsi_disk *un, register struct buf *bp)
{
	register struct diskhd *dp;

	TRACE_1(TR_FAC_SCSI, TR_SDDONE_START, "sddone_start: un %x", un);

	_NOTE(LOCK_RELEASED_AS_SIDE_EFFECT(&un->un_sd->sd_mutex));

	dp = &un->un_utab;
	if (bp == dp->b_forw) {
		dp->b_forw = NULL;
	}

	if (un->un_stats) {
		size_t n_done = bp->b_bcount - bp->b_resid;
		if (bp->b_flags & B_READ) {
			IOSP->reads++;
			IOSP->nread += n_done;
		} else {
			IOSP->writes++;
			IOSP->nwritten += n_done;
		}
	}
	if (IO_PARTITION_STATS) {
		size_t n_done = bp->b_bcount - bp->b_resid;
		if (bp->b_flags & B_READ) {
			IOSP_PARTITION->reads++;
			IOSP_PARTITION->nread += n_done;
		} else {
			IOSP_PARTITION->writes++;
			IOSP_PARTITION->nwritten += n_done;
		}
	}

	/*
	 * Start the next one before releasing resources on this one
	 * unless a suspend has been requested.
	 */
	if (un->un_state == SD_STATE_SUSPENDED) {
		cv_broadcast(&un->un_disk_busy_cv);
	} else if (dp->b_actf && (un->un_ncmds < un->un_throttle) &&
	    (dp->b_forw == NULL)) {
		sdstart(un);
	}

	mutex_exit(SD_MUTEX);

	if (bp != un->un_sbufp) {
		scsi_destroy_pkt(BP_PKT(bp));
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "regular done: resid %ld\n", bp->b_resid);
	} else {
#ifdef lint
		bp = bp;
#endif /* lint */
		ASSERT(un->un_sbuf_busy);
	}
	TRACE_0(TR_FAC_SCSI, TR_SDDONE_BIODONE_CALL, "sddone_biodone_call");


	if (un->un_lbasize != DEV_BSIZE) {
		bp = sd_shadow_iodone(bp);
	}

	if (bp->b_iodone == (int (*)())sd_overrun_iodone) {
		bp = sd_overrun_iodone(bp);
	}

	biodone(bp);

	(void) pm_idle_component(SD_DEVINFO, 0);

	TRACE_0(TR_FAC_SCSI, TR_SDDONE_END, "sddone end");
}


/*
 * reset the disk unless the transport layer has already
 * cleared the problem
 */
#define	C1	(STAT_BUS_RESET|STAT_DEV_RESET|STAT_ABORTED)
static void
sd_reset_disk(struct scsi_disk *un, struct scsi_pkt *pkt)
{
	int reset_retval = 0;

	if ((pkt->pkt_statistics & C1) == 0) {

		mutex_exit(SD_MUTEX);

		if (un->un_allow_bus_device_reset) {
			reset_retval = scsi_reset(ROUTE, RESET_TARGET);
		}

		if (reset_retval == 0) {
			(void) scsi_reset(ROUTE, RESET_ALL);
		}

		mutex_enter(SD_MUTEX);
	}
}

static int
sd_handle_incomplete(struct scsi_disk *un, struct buf *bp)
{
	static char *fail = "SCSI transport failed: reason '%s': %s\n";
	static char *notresp = "disk not responding to selection\n";
	int rval = COMMAND_DONE_ERROR;
	register struct scsi_pkt *pkt = BP_PKT(bp);
	int be_chatty = ((un->un_state != SD_STATE_SUSPENDED) &&
	    (un->un_state != SD_STATE_PM_SUSPENDED)) &&
	    (bp != un->un_sbufp || !(pkt->pkt_flags & FLAG_SILENT));
	int perr = (pkt->pkt_statistics & STAT_PERR);

	ASSERT(mutex_owned(SD_MUTEX));

	switch (pkt->pkt_reason) {

	case CMD_UNX_BUS_FREE:
		/*
		 * If we had a parity error that caused the target to
		 * drop BSY*, don't be chatty about it.
		 */

		SD_DO_ERRSTATS(un, sd_harderrs);
		if (perr && be_chatty) {
			be_chatty = 0;
		}
		break;
	case CMD_TAG_REJECT:
		SD_DO_ERRSTATS(un, sd_harderrs);
		pkt->pkt_flags = 0;
		un->un_tagflags = 0;
		if (un->un_options & SD_QUEUEING) {
			un->un_throttle = min(un->un_throttle, 3);
		} else {
			un->un_throttle = 1;
		}
		mutex_exit(SD_MUTEX);
		(void) scsi_ifsetcap(ROUTE, "tagged-qing", 0, 1);
		mutex_enter(SD_MUTEX);
		rval = QUE_COMMAND;
		break;
	case CMD_TRAN_ERR:
		/*
		 * If a parity error was detected, there is no need to
		 * do a bus device reset.  Just re-submit the pkt.
		 */
		SD_DO_ERRSTATS(un, sd_harderrs);
		if (perr) {
			break;
		}
		/*FALLTHROUGH*/
	case CMD_INCOMPLETE:
		/*
		 * selection did not complete; we don't want to go
		 * thru a bus reset for this reason
		 */
		if (pkt->pkt_state == STATE_GOT_BUS) {
			break;
		}
		/*FALLTHROUGH*/
	case CMD_TIMEOUT:
	default:
		/*
		 * the target may still be running the	command,
		 * so we should try and reset that target.
		 */
		SD_DO_ERRSTATS(un, sd_transerrs);
		sd_reset_disk(un, pkt);
		break;
	}

	if ((pkt->pkt_reason == CMD_RESET) || (pkt->pkt_statistics &
	    (STAT_BUS_RESET | STAT_DEV_RESET))) {
		if ((un->un_resvd_status & SD_RESERVE) == SD_RESERVE) {
			un->un_resvd_status |=
			    (SD_LOST_RESERVE | SD_WANT_RESERVE);
			SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "Lost Reservation\n");
		}
	}

	/*
	 * If pkt_reason is CMD_RESET/ABORTED, chances are that this pkt got
	 * reset/aborted because another disk on this bus caused it.
	 * The disk that caused it, should get CMD_TIMEOUT with pkt_statistics
	 * of STAT_TIMEOUT/STAT_DEV_RESET
	 */
	if ((pkt->pkt_reason == CMD_RESET) ||(pkt->pkt_reason == CMD_ABORTED)) {
		if ((int)PKT_GET_VICTIM_RETRY_CNT(pkt)
				< SD_VICTIM_RETRY_COUNT) {
			PKT_INCR_VICTIM_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
		}
	}

	if (bp == un->un_sbufp && (pkt->pkt_flags & FLAG_ISOLATE)) {
		rval = COMMAND_DONE_ERROR;
	} else {
		if ((rval == COMMAND_DONE_ERROR) &&
			((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count)) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
		}
	}

	if (pkt->pkt_reason == CMD_INCOMPLETE && rval == COMMAND_DONE_ERROR) {
		/*
		 * Looks like someone turned off this shoebox.
		 */
		if (un->un_state != SD_STATE_OFFLINE) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN, notresp);
			New_state(un, SD_STATE_OFFLINE);
		}
	} else if (be_chatty && !ISPXRE(un)) {
		/*
		 * suppress messages if they are all the same pkt reason;
		 * with TQ, many (up to 256) are returned with the same
		 * pkt_reason
		 * if we are in panic, then suppress the retry messages
		 */
		int in_panic = ddi_in_panic();
		if (!in_panic || (rval == COMMAND_DONE_ERROR)) {
			if ((pkt->pkt_reason != un->un_last_pkt_reason) ||
			    (rval == COMMAND_DONE_ERROR) ||
			    (sd_error_level == SCSI_ERR_ALL)) {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN, fail,
				    perr ? "parity error" :
					scsi_rname(pkt->pkt_reason),
				    (rval == COMMAND_DONE_ERROR) ?
				    "giving up": "retrying command");
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				    "retrycount=%x\n",
				    PKT_GET_RETRY_CNT(pkt));
			}
		}
	}
	return (rval);
}

static int
sd_handle_sense(struct scsi_disk *un, struct buf *bp)
{
	struct scsi_pkt *rqpkt;

	ASSERT(mutex_owned(SD_MUTEX));

	rqpkt = un->un_rqs;

	/*
	 * reset the retry count in rqsense packet
	 */
	PKT_SET_RETRY_CNT(rqpkt, 0);

	return (sd_decode_sense(un, bp, SCBP(rqpkt),
	    rqpkt->pkt_state, (uchar_t)rqpkt->pkt_resid));
}

static int
sd_handle_autosense(struct scsi_disk *un, struct buf *bp)
{
	struct scsi_pkt *pkt = BP_PKT(bp);
	struct scsi_arq_status *arqstat;

	ASSERT(mutex_owned(SD_MUTEX));

	ASSERT(pkt != 0);
	arqstat = (struct scsi_arq_status *)(pkt->pkt_scbp);

	/*
	 * if ARQ failed, then retry original cmd
	 */
	if (arqstat->sts_rqpkt_reason != CMD_CMPLT) {
		int rval;

		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    "auto request sense failed (reason=%s)\n",
		    scsi_rname(arqstat->sts_rqpkt_reason));

		sd_reset_disk(un, pkt);

		if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
		} else {
			rval = COMMAND_DONE_ERROR;
		}
		return (rval);
	}

	bcopy(&arqstat->sts_sensedata, SD_RQSENSE, SENSE_LENGTH);

	return (sd_decode_sense(un, bp, &arqstat->sts_rqpkt_status,
	    arqstat->sts_rqpkt_state, arqstat->sts_rqpkt_resid));
}


static int
sd_decode_sense(struct scsi_disk *un, struct buf *bp,
    struct scsi_status *statusp, ulong_t state, uchar_t resid)
{
	struct scsi_pkt *pkt;
	int rval = COMMAND_DONE_ERROR;
	int severity;
	size_t amt, i;
	char *p;
	static char *hex = " 0x%x";
	char *rq_err_msg = 0;
	int partition = SDPART(bp->b_edev);
	daddr_t req_blkno;
	int	pfa_reported = 0;

	ASSERT(mutex_owned(SD_MUTEX));

	if (bp != 0) {
		pkt = BP_PKT(bp);
	} else {
		pkt = 0;
	}
	/*
	 * For uscsi commands, squirrel away a copy of the
	 * results of the Request Sense
	 */
	if (bp == un->un_sbufp) {
		struct uscsi_cmd *ucmd = (struct uscsi_cmd *)bp->b_forw;
		ucmd->uscsi_rqstatus = *(uchar_t *)statusp;
		if (ucmd->uscsi_rqlen && un->un_srqbufp) {
			ucmd->uscsi_rqresid = resid;
			/*
			 * uscsi_rqlen is not going to be more than
			 * SENSE_LENGTH
			 */
			bcopy(SD_RQSENSE, un->un_srqbufp,
						ucmd->uscsi_rqlen);
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_decode_sense: stat=0x%x resid=0x%x\n",
				ucmd->uscsi_rqstatus, ucmd->uscsi_rqresid);
		}
	}

	if (STATUS_SCBP_C(statusp) == STATUS_RESERVATION_CONFLICT) {
		return (sd_handle_resv_conflict(un, bp));
	} else if (statusp->sts_busy) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    "Busy Status on REQUEST SENSE\n");
		if (pkt && (int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			/*
			 * restart command
			 */
			sd_requeue_cmd(un, bp, SD_BSY_TIMEOUT);
			rval = JUST_RETURN;
		}
		return (rval);
	}

	if (statusp->sts_chk) {
		/*
		 * If the request sense failed, for any of a number
		 * of reasons, allow the original command to be
		 * retried.  Only log error on our last gasp.
		 */
		rq_err_msg = "Check Condition on REQUEST SENSE\n";
sense_failed:
		if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
		} else {
			if (rq_err_msg) {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    rq_err_msg);
			}
		}
		return (rval);
	}

	amt = (int)(SENSE_LENGTH - resid);
	if ((state & STATE_XFERRED_DATA) == 0 || amt == 0) {
		rq_err_msg = "Request Sense couldn't get sense data\n";
		goto sense_failed;
	}

	/*
	 * Now, check to see whether we got enough sense data to make any
	 * sense out if it (heh-heh).
	 */

	if (amt < SUN_MIN_SENSE_LENGTH) {
		rq_err_msg = "Not enough sense information\n";
		goto sense_failed;
	}

	if (SD_RQSENSE->es_class != CLASS_EXTENDED_SENSE) {
		if (!(pkt->pkt_flags & FLAG_SILENT)) {
			static char tmp[8];
			static char buf[128];
			p = (char *)SD_RQSENSE;
			mutex_enter(&sd_log_mutex);
			(void) strcpy(buf, "undecodable sense information:");
			for (i = 0; i < amt; i++) {
				(void) sprintf(tmp, hex, *(p++)&0xff);
				(void) strcpy(&buf[strlen(buf)], tmp);
			}
			i = strlen(buf);
			(void) strcpy(&buf[i], "-(assumed fatal)\n");
			scsi_log(SD_DEVINFO, sd_label, CE_WARN, buf);
			mutex_exit(&sd_log_mutex);
		}
		return (rval);
	}

	/*
	 * use absolute block number for request block
	 */
	req_blkno = dkblock(bp);
	if (un->un_lbasize == DEV_BSIZE) {
		if (un->un_gvalid == TRUE) {
			req_blkno += un->un_offset[partition];
		} else {
			req_blkno += un->un_solaris_offset;
		}
	}

	if (SD_RQSENSE->es_valid) {
		un->un_err_blkno =
		    (SD_RQSENSE->es_info_1 << 24) |
		    (SD_RQSENSE->es_info_2 << 16) |
		    (SD_RQSENSE->es_info_3 << 8)  |
		    (SD_RQSENSE->es_info_4);
		/*
		 * for USCSI commands we are better off using the error
		 * block no. as the requested block no. This is the best
		 * we can estimate.
		 */
		if ((bp == un->un_sbufp) &&
		    (pkt && !(pkt->pkt_flags & FLAG_SILENT))) {
			req_blkno = un->un_err_blkno;
		}
	} else {
		/*
		 * With the valid bit not being set, we have
		 * to figure out by hand as close as possible
		 * what the real block number might have been
		 */
		un->un_err_blkno = req_blkno;
	}

	if (DEBUGGING || sd_error_level == SCSI_ERR_ALL) {
		if (pkt) {
			clean_print(SD_DEVINFO, sd_label, CE_NOTE,
			    "Failed CDB", (char *)pkt->pkt_cdbp, CDB_SIZE);
			clean_print(SD_DEVINFO, sd_label, CE_NOTE,
			    "Sense Data", (char *)SD_RQSENSE, SENSE_LENGTH);
		}
	}

	/*
	 * Handle deferred error case:
	 * - If sense key is KEY_RECOVERABLE_ERROR or
	 *   sense key is KEY_NO_SENSE and ILI bit is not set
	 *   the current command will be retried
	 * - otherwise the current command returns with error
	 */

	if (SD_RQSENSE->es_code == SD_DEFERRED_ERROR) {
		switch (SD_RQSENSE->es_key) {
		case KEY_NO_SENSE:
			if (SD_RQSENSE->es_ili) {
				severity = SCSI_ERR_FATAL;
				rval = COMMAND_DONE_ERROR;
			}
			/* FALLTHROUGH */
		case KEY_RECOVERABLE_ERROR:
			severity = SCSI_ERR_RETRYABLE;
			rval = QUE_COMMAND;
			break;
		default:
			severity = SCSI_ERR_FATAL;
			rval = COMMAND_DONE_ERROR;
			break;
		}

		goto def_errdone;
	}

	switch (SD_RQSENSE->es_key) {
	case KEY_NOT_READY:
		/*
		 * There are different requirements for CDROMs and disks
		 * for the  number of retries. If CD-ROM is giving this,
		 * probably it is reading TOC and is in the process of
		 * getting ready, and we should keep on trying for long time
		 * to make sure that all types of media
		 * is taken in account (for some of them drive takes long
		 * time to read TOC). For disks we do not want to
		 * retry this too many times; this causes long
		 * hangs in format when the drive refuses to spin up (very
		 * common failure)
		 */

		/*
		 * update error stats after first Not Ready error. Disks
		 * may have been powered down and may need to be restarted,
		 * For CDROMs, report not ready errors only if Media is
		 * present.
		 */
		if ((ISCD(un) && un->un_gvalid == TRUE) ||
		    ((int)PKT_GET_RETRY_CNT(pkt) > 0)) {
			SD_DO_ERRSTATS(un, sd_harderrs);
			SD_DO_ERRSTATS(un, sd_rq_ntrdy_err);
		}

		/*
		 * If a disk is pulled out of some third party devices,
		 * they return a check condition of Not Ready with
		 * ASC, ASCQ of 0x04, 0x03. It means LUN not ready, manual
		 * intervention required. For this condition we should not
		 * retry. Otherwise each command would retry along with
		 * issuing start stop commands and there will be lots of
		 * Not ready start stop messages. Also it takes lot of
		 * time to fail the device.
		 */

		if (!ISCD(un) && (SD_RQSENSE->es_add_code == 0x04) &&
		    (SD_RQSENSE->es_qual_code == 0x03)) {
			rval = COMMAND_DONE_ERROR;
			severity = SCSI_ERR_FATAL;
			break;
		}

		if (pkt &&
		    (int)PKT_GET_RETRY_CNT(pkt) < un->un_notready_retry_count) {
			/*
			 * If we get a not-ready indication, wait a bit and
			 * try it again. Some drives pump this out for about
			 * 2-3 seconds after a reset. If caddy is not inserted
			 * or CD is not present, we should just return.
			 */
			if (sd_not_ready(un, pkt) == SD_MEDIUM_NOT_PRESENT) {
				rval = COMMAND_DONE_ERROR;
			} else {
				PKT_INCR_RETRY_CNT(pkt, 1);

				severity = SCSI_ERR_RETRYABLE;
				/*
				 * restart command
				 */
				sd_requeue_cmd(un, bp,
					(clock_t)un->un_notrdy_delay);
				rval = JUST_RETURN;
			}
			return (rval);
		} else {
			rval = COMMAND_DONE_ERROR;
			severity = SCSI_ERR_FATAL;
		}
		break;

	case KEY_RECOVERABLE_ERROR:
		severity = SCSI_ERR_RECOVERED;
		if (SD_RQSENSE->es_add_code == 0x5d && sd_report_pfa) {
			SD_DO_ERRSTATS(un, sd_rq_pfa_err);
			pfa_reported = 1;
			severity = SCSI_ERR_INFO;
		} else {
			SD_DO_ERRSTATS(un, sd_softerrs);
			SD_DO_ERRSTATS(un, sd_rq_recov_err);
		}
		if (pkt->pkt_resid == 0) {
			rval = COMMAND_DONE;
			break;
		}
		goto retry;
	case KEY_NO_SENSE:
		SD_DO_ERRSTATS(un, sd_softerrs);
		goto retry;

	case KEY_MEDIUM_ERROR:
		SD_DO_ERRSTATS(un, sd_rq_media_err);
		/*FALLTHROUGH*/
	case KEY_HARDWARE_ERROR:
		if (sd_reset_retry_count && pkt &&
		    (int)PKT_GET_RETRY_CNT(pkt) == sd_reset_retry_count) {
			mutex_exit(SD_MUTEX);
			if (un->un_allow_bus_device_reset) {
				(void) scsi_reset(ROUTE, RESET_TARGET);
			}
			mutex_enter(SD_MUTEX);
		}
		/*FALLTHROUGH*/
	case KEY_ABORTED_COMMAND:
		SD_DO_ERRSTATS(un, sd_harderrs);
retry:
		if (pkt && (int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
			severity = SCSI_ERR_RETRYABLE;
			break;
		}
		goto errdone;
	case KEY_ILLEGAL_REQUEST:
		SD_DO_ERRSTATS(un, sd_softerrs);
		SD_DO_ERRSTATS(un, sd_rq_illrq_err);
		severity = SCSI_ERR_INFO;
		rval = COMMAND_DONE_ERROR;
		break;
	case KEY_MISCOMPARE:
	case KEY_VOLUME_OVERFLOW:
	case KEY_WRITE_PROTECT:
	case KEY_BLANK_CHECK:
errdone:
		severity = SCSI_ERR_FATAL;
		rval = COMMAND_DONE_ERROR;
		break;

	case KEY_UNIT_ATTENTION:
		rval = QUE_COMMAND;
		severity = SCSI_ERR_INFO;
		if (SD_RQSENSE->es_add_code == 0x5d && sd_report_pfa) {
			SD_DO_ERRSTATS(un, sd_rq_pfa_err);
			pfa_reported = 1;
			goto retry;
		}
		if (ISREMOVABLE(un)) {
			if (sd_handle_ua(un)) {
				SD_DO_ERRSTATS(un, sd_harderrs);
				SD_DO_ERRSTATS(un, sd_rq_nodev_err);
				rval = COMMAND_DONE_ERROR;
				severity = SCSI_ERR_FATAL;
				break;
			}
		} else {
			SD_DO_ERRSTATS(un, sd_harderrs);
			SD_DO_ERRSTATS(un, sd_rq_nodev_err);
		}
		if (SD_RQSENSE->es_add_code == 0x29 &&
		    (un->un_resvd_status & SD_RESERVE) == SD_RESERVE) {
			un->un_resvd_status |=
				(SD_LOST_RESERVE | SD_WANT_RESERVE);
			SD_DEBUG(SD_DEVINFO, sd_label, CE_WARN,
			    "sd_decode_sense: Lost Reservation\n");
		}
		break;

	default:
		/*
		 * Undecoded sense key.	 Try retries and hope
		 * that will fix the problem.  Otherwise, we're
		 * dead.
		 */
		SD_DO_ERRSTATS(un, sd_harderrs);
		if (pkt && !(pkt->pkt_flags & FLAG_SILENT)) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "Unhandled Sense Key '%s\n",
			    sense_keys[SD_RQSENSE->es_key]);
		}
		if (pkt && (int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			severity = SCSI_ERR_RETRYABLE;
			rval = QUE_COMMAND;
		} else {
			severity = SCSI_ERR_FATAL;
			rval = COMMAND_DONE_ERROR;
		}
	}

def_errdone:
	if (rval == QUE_COMMAND && bp == un->un_sbufp &&
	    pkt && (pkt->pkt_flags & FLAG_DIAGNOSE)) {
		rval = COMMAND_DONE_ERROR;
	}

	if (pfa_reported ||
	    ((bp == un->un_sbufp) &&
		((pkt && pkt->pkt_flags & FLAG_SILENT) == 0)) ||
	    ((bp != un->un_sbufp) &&
		(DEBUGGING || (severity >= sd_error_level)))) {
		sd_errmsg(un, pkt, severity, req_blkno);
	}

	if ((rval == COMMAND_DONE_ERROR) &&
		((bp->b_error == 0) || (bp->b_error == EIO))) {
		if ((un->un_rqs_state == 0) ||
			(un->un_rqs_state & SD_RQS_READ)) {
			un->un_rqs_state &= ~(SD_RQS_READ);
		} else {
			un->un_rqs_state |= SD_RQS_OVR;
		}
		un->un_rqs_state |= (SD_RQS_ERROR | SD_RQS_VALID);
		bcopy(SD_RQSENSE, un->un_uscsi_rqs_buf, SENSE_LENGTH);
	}

	return (rval);
}

struct scsi_asq_key_strings additional_codes[] = {
	0x81, 0, "Logical Unit is Reserved",
	0x85, 0, "Audio Address Not Valid",
	0xb6, 0, "Media Load Mechanism Failed",
	0xB9, 0, "Audio Play Operation Aborted",
	0xbf, 0, "Buffer Overflow for Read All Subcodes Command",
	0x53, 2, "Medium removal prevented",
	0x6f, 0, "Authentication failed during key exchange",
	0x6f, 1, "Key not present",
	0x6f, 2, "Key not established",
	0x6f, 3, "Read without proper authentication",
	0x6f, 4, "Mismatched region to this logical unit",
	0x6f, 5, "Region reset count error",
	0xffff, 0x0, NULL
};

static void
sd_errmsg(struct scsi_disk *un, struct scsi_pkt *pkt,
    int severity, daddr_t blkno)
{
	if (ISPXRE(un))
		return;
	if (ISCD(un)) {
		scsi_vu_errmsg(SD_SCSI_DEVP, pkt, sd_label, severity,
			blkno, un->un_err_blkno, scsi_cmds,
			SD_RQSENSE, additional_codes, NULL);
	} else {
		scsi_vu_errmsg(SD_SCSI_DEVP, pkt, sd_label, severity,
			blkno, un->un_err_blkno, scsi_cmds,
			SD_RQSENSE, NULL, NULL);
	}
}

static int
sd_check_error(struct scsi_disk *un, struct buf *bp)
{
	struct diskhd *dp = &un->un_utab;
	register struct scsi_pkt *pkt = BP_PKT(bp);
	int	action;
	int	reset_retval;

	TRACE_0(TR_FAC_SCSI, TR_SD_CHECK_ERROR_START, "sd_check_error_start");
	ASSERT(mutex_owned(SD_MUTEX));

	if (SCBP_C(pkt) == STATUS_RESERVATION_CONFLICT) {
		return (sd_handle_resv_conflict(un, bp));
	} else if (SCBP(pkt)->sts_busy) {
		if (SCBP(pkt)->sts_scsi2) {
			/*
			 * Queue Full status
			 *
			 * We will get this if the HBA doesn't handle queue full
			 * condition. This is mainly for third parties because
			 * Sun's HBA handle queue full and target will never
			 * see this case.
			 *
			 * This handling is similar to TRAN_BUSY handling.
			 *
			 * If there are some command already in the transport,
			 * then the queue full is because the queue for this
			 * nexus is actually full. If there are no commands in
			 * the transport then the queue full is because
			 * some other initiator or lun is consuming all the
			 * resources at the target.
			 */
			sd_handle_tran_busy(bp, dp, un);
			action = JUST_RETURN;
		} else {
			if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
				PKT_INCR_RETRY_CNT(pkt, 1);
				/*
				 * restart command
				 */
				sd_requeue_cmd(un, bp,
					(clock_t)un->un_bsy_delay);
				action = JUST_RETURN;
			} else {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    "device busy too long\n");
				mutex_exit(SD_MUTEX);
				reset_retval = 0;
				if (un->un_allow_bus_device_reset) {
					reset_retval =
					    scsi_reset(ROUTE, RESET_TARGET);
				}
				if (reset_retval != 0) {
					/* RESET_TARGET allowed and worked */
					action = QUE_COMMAND;
				} else if (scsi_reset(ROUTE, RESET_ALL)) {
					action = QUE_COMMAND;
				} else {
					action = COMMAND_DONE_ERROR;
				}
				mutex_enter(SD_MUTEX);
			}
		}
	} else if (SCBP(pkt)->sts_chk) {
		if ((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			if (!un->un_arq_enabled) {
				action = QUE_SENSE;
			} else {
				action = QUE_COMMAND;
			}
		} else {
			action = COMMAND_DONE_ERROR;
		}
	}
	TRACE_0(TR_FAC_SCSI, TR_SD_CHECK_ERROR_END, "sd_check_error_end");
	return (action);
}


/*
 *	System Crash Dump routine
 */

#define	NDUMP_RETRIES	5

static int
sddump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk)
{
	struct dk_map *lp;
	struct scsi_pkt *pkt;
	int i, pflag;
	struct buf local, *bp;
	int err;
	daddr_t oblkno;
	ssize_t dma_resid;
	GET_SOFT_STATE(dev);

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*un))

	if (un->un_gvalid != TRUE || ISCD(un))
		return (ENXIO);

	lp = &un->un_map[part];
	if (blkno+nblk > lp->dkl_nblk) {
		return (EINVAL);
	}

	/*
	 * The first time through, reset the specific target device.
	 * Do a REQUEST SENSE if required.
	 */

	/* SunBug 1222170 */
	pflag = FLAG_NOINTR;

	un->un_rqs->pkt_flags |= pflag;

	if ((un->un_state == SD_STATE_SUSPENDED) ||
	    (un->un_state == SD_STATE_PM_SUSPENDED)) {
		if (un->un_power_level == 0) {
			if (ddi_dev_is_needed(SD_DEVINFO, 0, 1)
			    != DDI_SUCCESS) {
				return (EIO);
			}
		}
	}

	/*
	 * When cpr calls sddump, we know that sd is in a
	 * a good state, so no bus reset is required
	 */
	un->un_throttle = 0;

	if ((un->un_state != SD_STATE_SUSPENDED) &&
		(un->un_state != SD_STATE_DUMPING)) {

		New_state(un, SD_STATE_DUMPING);

		/*
		 * Reset the bus. I'd like to not have to do this,
		 * but this is the safest thing to do...
		 */

		if (scsi_reset(ROUTE, RESET_ALL) == 0) {
			return (EIO);
		}

		(void) sd_clear_cont_alleg(un, un->un_rqs);
	}


	/*
	 * It should be safe to call the allocator here without
	 * worrying about being locked for DVMA mapping because
	 * the address we're passed is already a DVMA mapping
	 *
	 * We are also not going to worry about semaphore ownership
	 * in the dump buffer. Dumping is single threaded at present.
	 */

	bp = &local;
	bzero(bp, sizeof (*bp));
	bp->b_flags = B_BUSY;
	bp->b_un.b_addr = addr;
	bp->b_bcount = nblk << DEV_BSHIFT;
	bp->b_resid = 0;

	pkt = NULL;

	dma_resid = bp->b_bcount;

	oblkno = blkno;

	while (dma_resid != 0) {

		/* Try setting up the packet */
		for (i = 0; i < NDUMP_RETRIES; i++) {
			bp->b_flags &= ~B_ERROR;

			blkno = oblkno +
				(bp->b_bcount - dma_resid >> DEV_BSHIFT) +
				un->un_offset[part];
			nblk = (dma_resid) >> DEV_BSHIFT;

			if (un->un_secdiv != DEV_BSHIFT) {
				nblk >>= (un->un_secdiv - DEV_BSHIFT);
			}

			if (pkt == NULL) {
				if ((pkt = scsi_init_pkt(ROUTE, NULL, bp,
					    CDB_GROUP1,
					    un->un_cmd_stat_size, PP_LEN,
					    PKT_DMA_PARTIAL,
					    NULL_FUNC, NULL)) != NULL) {
					break;
				}
			} else {
				/* set up next io for dma break up */
				if (scsi_init_pkt(ROUTE, pkt, bp, 0, 0, 0, 0,
					NULL_FUNC, NULL) != NULL) {
					break;
				}
			}
			if (i == 0) {
				if (bp->b_flags & B_ERROR) {
					scsi_log(SD_DEVINFO, sd_label, CE_WARN,
						"no resources for dumping; "
						"error code: 0x%x, retrying",
						geterror(bp));
				} else {
					scsi_log(SD_DEVINFO, sd_label, CE_WARN,
						"no resources for dumping; "
						"retrying");
				}
			} else if (i != (NDUMP_RETRIES - 1)) {
				if (bp->b_flags & B_ERROR) {
					scsi_log(SD_DEVINFO, sd_label, CE_CONT,
					"no resources for dumping; error code:"
					" 0x%x, "
					"retrying\n", geterror(bp));
				}
			} else {
				if (bp->b_flags & B_ERROR) {
					scsi_log(SD_DEVINFO, sd_label, CE_CONT,
						"no resources for dumping; "
						"error code: 0x%x, retries "
						"failed, "
						"giving up.\n", geterror(bp));
				} else {
					scsi_log(SD_DEVINFO, sd_label, CE_CONT,
						"no resources for dumping; "
						"retries failed, giving up.\n");
				}
				if (pkt != NULL)
					scsi_destroy_pkt(pkt);
				return (EIO);
			}
			drv_usecwait(10000);
		}

		dma_resid = pkt->pkt_resid;

		if (dma_resid != 0)
			nblk -= (dma_resid >> un->un_secdiv);

		pkt->pkt_resid = 0;

		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
				SCMD_WRITE_G1, (int)blkno, nblk, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		pkt->pkt_flags = pflag;


		/* try sending the packet */
		for (err = EIO, i = 0; i < NDUMP_RETRIES && err == EIO; i++) {
			if (sd_scsi_poll(un, pkt) == 0) {
				switch (SCBP_C(pkt)) {
				case STATUS_GOOD:
					if (pkt->pkt_resid == 0) {
						err = 0;
					}
					break;
				case STATUS_CHECK:
					if (((pkt->pkt_state & STATE_ARQ_DONE)
									== 0)) {
						(void) sd_clear_cont_alleg(un,
						    un->un_rqs);
					}
					break;
				case STATUS_BUSY:
					/*
					 * In sddump; we don't care about
					 * sd_avoid_bus_device_reset anymore
					 */
					(void) scsi_reset(ROUTE, RESET_TARGET);
					(void) sd_clear_cont_alleg(un,
						    un->un_rqs);
					break;
				default:
					mutex_enter(SD_MUTEX);
					sd_reset_disk(un, pkt);
					mutex_exit(SD_MUTEX);
					break;
				}
			} else if (i > NDUMP_RETRIES/2) {
				(void) scsi_reset(ROUTE, RESET_ALL);
				(void) sd_clear_cont_alleg(un, un->un_rqs);
			}

		}
		if ((i == NDUMP_RETRIES) && (err != 0)) {
			break;
		}
	}
	scsi_destroy_pkt(pkt);
	return (err);
}

/*
 * This routine implements the ioctl calls.  It is called
 * from the device switch at normal priority.
 */
/* ARGSUSED3 */
static int
sdioctl(dev_t dev, int cmd, intptr_t arg, int flag,
	cred_t *cred_p, int *rval_p)
{
	int data[512 / (sizeof (int))];
	struct dk_cinfo *info;
	struct vtoc vtoc;
	struct uscsi_cmd *scmd;
	char cdb[CDB_GROUP0];
	int i, err;
	enum uio_seg uioseg;
	enum dkio_state state;
	int nodelay = (flag & (FNDELAY | FNONBLOCK));
	int geom_validated = 0;

	GET_SOFT_STATE(dev);

	/*
	 * All device accesses go thru sdstrategy where we check
	 * on suspend status
	 */

	bzero(data, sizeof (data));

	if (un->un_gvalid != TRUE && nodelay) {
		switch (cmd) {
		case CDROMPAUSE:
			/* FALLTHROUGH */
		case CDROMRESUME:
			/* FALLTHROUGH */
		case CDROMPLAYMSF:
			/* FALLTHROUGH */
		case CDROMPLAYTRKIND:
			/* FALLTHROUGH */
		case CDROMREADTOCHDR:
			/* FALLTHROUGH */
		case CDROMREADTOCENTRY:
			/* FALLTHROUGH */
		case CDROMSTOP:
			/* FALLTHROUGH */
		case CDROMSTART:
			/* FALLTHROUGH */
		case CDROMVOLCTRL:
			/* FALLTHROUGH */
		case CDROMSUBCHNL:
			/* FALLTHROUGH */
		case CDROMREADMODE2:
			/* FALLTHROUGH */
		case CDROMREADMODE1:
			/* FALLTHROUGH */
#ifdef CDROMREADOFFSET
		case CDROMREADOFFSET:
			/* FALLTHROUGH */
#endif
		case CDROMSBLKMODE:
			/* FALLTHROUGH */
		case CDROMGBLKMODE:
			/* FALLTHROUGH */
		case CDROMGDRVSPEED:
			/* FALLTHROUGH */
		case CDROMSDRVSPEED:
			/* FALLTHROUGH */
		case CDROMCDDA:
			/* FALLTHROUGH */
		case CDROMCDXA:
			/* FALLTHROUGH */
		case CDROMSUBCODE:
			/*
			 * We are learning to be consistent
			 * in our return values.
			 */
			if (!ISCD(un)) {
				return (ENOTTY);
			}
			break;

		case FDEJECT:
			/* FALLTHROUGH */
		case DKIOCEJECT:
			/* FALLTHROUGH */
		case CDROMEJECT:
			if (!ISREMOVABLE(un)) {
				return (ENOTTY);
			}
			break;

		case DKIOCSVTOC:
			mutex_enter(SD_MUTEX);
			if (un->un_ncmds == 0) {
				if ((err = sd_unit_ready(dev)) != 0) {
					mutex_exit(SD_MUTEX);
					return (err);
				}
			}
			mutex_exit(SD_MUTEX);
			/* FALLTHROUGH */
		case DKIOCREMOVABLE:
			/* FALLTHROUGH */
		case DKIOCINFO:
			/* FALLTHROUGH */
		case DKIOCGMEDIAINFO:
			/* FALLTHROUGH */
		case MHIOCSTATUS:
			/* FALLTHROUGH */
		case MHIOCTKOWN:
			/* FALLTHROUGH */
		case MHIOCRELEASE:
			/* FALLTHROUGH */
		case MHIOCENFAILFAST:
			/* FALLTHROUGH */
		case USCSICMD:
			/* FALLTHROUGH */
		case USCSIGETRQS:
			goto skip_ready_valid;

		default:
			break;
		}

		if ((err = sd_ready_and_valid(dev, un)) != SD_READY_VALID) {
			switch (cmd) {
			case DKIOCSTATE:
				/* FALLTHROUGH */
			case DKIOCSVTOC:
				/* FALLTHROUGH */
			case CDROMGDRVSPEED:
				/* FALLTHROUGH */
			case CDROMSDRVSPEED:
				break;

			case FDEJECT:	/* for eject command */
				/* FALLTHROUGH */
			case DKIOCEJECT:
				/* FALLTHROUGH */
			case CDROMEJECT:
				/* FALLTHROUGH */
			case DKIOCSGEOM:
				/* FALLTHROUGH */
			case DKIOCREMOVABLE:
				/* FALLTHROUGH */
			case DKIOCSAPART:
				if (err != SD_TUR_FAILED) {
					break;
				}
				/* FALLTHROUGH */
			default:
				if (ISREMOVABLE(un) &&
				    (err == SD_TUR_FAILED)) {
					err = ENXIO;
				} else {
					err = EIO;
				}
				return (err);
			}
		}
		geom_validated = 1;
	}

skip_ready_valid:

	switch (cmd) {

#ifdef SDDEBUG
/*
 * Following ioctl are for testing RESET/ABORTS
 */
#define	DKIOCRESET	(DKIOC|14)
#define	DKIOCABORT	(DKIOC|15)

	case DKIOCRESET:
		if (ddi_copyin((caddr_t)arg, data, sizeof (int), flag))
			return (EFAULT);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"DKIOCRESET: data = 0x%lx\n", data[0]);
		if (scsi_reset(ROUTE, data[0])) {
			return (0);
		} else {
			return (EIO);
		}
	case DKIOCABORT:
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"DKIOCABORT:\n");
		if (scsi_abort(ROUTE, (struct scsi_pkt *)0)) {
			return (0);
		} else {
			return (EIO);
		}
#endif

	case DKIOCINFO:
		/*
		 * Controller Information
		 */
		info = (struct dk_cinfo *)data;
		switch (un->un_ctype) {
		case CTYPE_MD21:
			info->dki_ctype = DKC_MD21;
			break;
		case CTYPE_CDROM:
			info->dki_ctype = DKC_CDROM;
			break;
		default:
			info->dki_ctype = DKC_SCSI_CCS;
			break;
		}
		info->dki_cnum = ddi_get_instance(ddi_get_parent(SD_DEVINFO));
		(void) strcpy(info->dki_cname,
		    ddi_get_name(ddi_get_parent(SD_DEVINFO)));
		/*
		 * Unit Information
		 */
		info->dki_unit = ddi_get_instance(SD_DEVINFO);
		info->dki_slave = (Tgt(SD_SCSI_DEVP)<<3) | Lun(SD_SCSI_DEVP);
		(void) strcpy(info->dki_dname, ddi_get_name(SD_DEVINFO));
		info->dki_flags = DKI_FMTVOL;
		info->dki_partition = SDPART(dev);

		/*
		 * Max Transfer size of this device in blocks
		 */
		info->dki_maxtransfer = un->un_max_xfer_size / DEV_BSIZE;

		/*
		 * We can't get from here to there yet
		 */
		info->dki_addr = 0;
		info->dki_space = 0;
		info->dki_prio = 0;
		info->dki_vec = 0;

		i = sizeof (struct dk_cinfo);
		if (ddi_copyout(data, (caddr_t)arg, i, flag))
			return (EFAULT);
		else
			return (0);
	case DKIOCGMEDIAINFO:
		return (sd_get_media_info(dev, (caddr_t)arg, flag));

	case DKIOCGGEOM:
	/*
	 * Return the geometry of the specified unit.
	 */
	{
		struct dk_geom *tmpdkg = (struct dk_geom *)data;

		if (un->un_solaris_size == 0)
			return (EIO);
		mutex_enter(SD_MUTEX);
		if (geom_validated == 0) {
			/*
			 * sd_validate_geometry does not spin a disk up
			 * if it was spun down. We need to make sure it
			 * is ready.
			 */
			if (un->un_ncmds == 0) {
				if ((err = sd_unit_ready(dev)) != 0) {
					mutex_exit(SD_MUTEX);
					return (err);
				}
			}
			if ((err = sd_validate_geometry(un, SLEEP_FUNC)) != 0) {
				mutex_exit(SD_MUTEX);
				return (err);
			}
		}
		i = sizeof (struct dk_geom);

		bcopy(&un->un_g, tmpdkg, i);

		mutex_exit(SD_MUTEX);

		if (tmpdkg->dkg_write_reinstruct == 0)
			tmpdkg->dkg_write_reinstruct =
			    (int)((int)(tmpdkg->dkg_nsect * tmpdkg->dkg_rpm *
			    sd_rot_delay) / (int)60000);


		err = ddi_copyout(tmpdkg, (caddr_t)arg, i, flag);

		if (err)
			return (EFAULT);
		else
			return (0);
	}

	case DKIOCSGEOM:
	/*
	 * Set the geometry of the specified unit.
	 */
		if (un->un_solaris_size == 0)
			return (EIO);
		i = sizeof (struct dk_geom);
		if (ddi_copyin((caddr_t)arg, data, i, flag))
			return (EFAULT);

		mutex_enter(SD_MUTEX);
		bcopy(data, &un->un_g, i);
		for (i = 0; i < NDKMAP; i++) {
			struct dk_map *lp  = &un->un_map[i];
			un->un_offset[i] =
			    un->un_g.dkg_nhead *
			    un->un_g.dkg_nsect *
			    lp->dkl_cylno;
			un->un_offset[i] += un->un_solaris_offset;
		}
		mutex_exit(SD_MUTEX);
		return (0);

	case DKIOCGAPART:
	/*
	 * Return the map for all logical partitions.
	 */
		if (un->un_solaris_size == 0)
			return (EIO);
		mutex_enter(SD_MUTEX);
#if defined(__i386) || defined(__ia64)
		if (geom_validated == 0) {
			(void) sd_validate_geometry(un, SLEEP_FUNC);
			if (un->un_gvalid != TRUE) {
				mutex_exit(SD_MUTEX);
				return (EINVAL);
			}
		}
#endif	/* defined(i386) */

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct dk_map32 dk_map32[NDKMAP];

			for (i = 0; i < NDKMAP; i++) {
				dk_map32[i].dkl_cylno =
					un->un_map[i].dkl_cylno;
				dk_map32[i].dkl_nblk =
					un->un_map[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyout(dk_map32, (caddr_t)arg, i, flag)) {
				mutex_exit(SD_MUTEX);
				return (EFAULT);
			}
			mutex_exit(SD_MUTEX);
			return (0);
		}

		case DDI_MODEL_NONE:
			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyout(un->un_map, (caddr_t)arg, i, flag)) {
				mutex_exit(SD_MUTEX);
				return (EFAULT);
			}
			mutex_exit(SD_MUTEX);
			return (0);
		}
#else /* ! _MULTI_DATAMODEL */
		i = NDKMAP * sizeof (struct dk_map);
		if (ddi_copyout(un->un_map, (caddr_t)arg, i, flag)) {
			mutex_exit(SD_MUTEX);
			return (EFAULT);
		} else {
			mutex_exit(SD_MUTEX);
			return (0);
		}
#endif /* _MULTI_DATAMODEL */

	case DKIOCSAPART: {
		struct dk_map dk_map[NDKMAP];

	/*
	 * Set the map for all logical partitions.  We lock
	 * the priority just to make sure an interrupt doesn't
	 * come in while the map is half updated.
	 */
		if (un->un_solaris_size == 0)
			return (EIO);

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct dk_map32 dk_map32[NDKMAP];

			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyin((caddr_t)arg, dk_map32, i, flag))
				return (EFAULT);
			for (i = 0; i < NDKMAP; i++) {
				dk_map[i].dkl_cylno =
					dk_map32[i].dkl_cylno;
				dk_map[i].dkl_nblk =
					dk_map32[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map);
			break;
		}

		case DDI_MODEL_NONE:
			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyin((caddr_t)arg, dk_map, i, flag))
				return (EFAULT);
			break;
		}
#else /* ! _MULTI_DATAMODEL */
		i = NDKMAP * sizeof (struct dk_map);
		if (ddi_copyin((caddr_t)arg, dk_map, i, flag))
			return (EFAULT);
#endif /* _MULTI_DATAMODEL */

		mutex_enter(SD_MUTEX);
		bcopy(dk_map, un->un_map, i);
		{
			int spc = un->un_g.dkg_nhead * un->un_g.dkg_nsect;
			struct dk_map *lp = (struct dk_map *)&(un->un_map);
			ulong_t *offsetp = (ulong_t *)&(un->un_offset);
			struct dkl_partition *vp =
			    (struct dkl_partition *)&(un->un_vtoc);

			for (i = 0; i < NDKMAP; i++, lp++, offsetp++, vp++) {
				*offsetp = spc * lp->dkl_cylno;
#if defined(_SUNOS_VTOC_16)
				vp->p_start = *offsetp;
				vp->p_size = lp->dkl_nblk;
#endif	/* defined(_SUNOS_VTOC_16) */

				*offsetp += un->un_solaris_offset;
			}
		}
		mutex_exit(SD_MUTEX);
		return (0);
	}

	case DKIOCGVTOC:
		/*
		 * Get the label (vtoc, geometry and partition map) directly
		 * from the disk, in case if it got modified by another host
		 * sharing the disk in a multi initiator configuration.
		 */
		if (un->un_solaris_size == 0)
			return (EIO);
		mutex_enter(SD_MUTEX);
		if (geom_validated == 0) {
			if (un->un_ncmds == 0) {
				if ((err = sd_unit_ready(dev)) != 0) {
					mutex_exit(SD_MUTEX);
					return (err);
				}
			}
			if ((err = sd_validate_geometry(un, SLEEP_FUNC)) != 0) {
				mutex_exit(SD_MUTEX);
				return (err);
			}
		}

#if defined(_SUNOS_VTOC_8)
		sd_build_user_vtoc(un, &vtoc);
		mutex_exit(SD_MUTEX);

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct vtoc32 vtoc32;

			vtoctovtoc32(vtoc, vtoc32);
			if (ddi_copyout(&vtoc32, (void *)arg,
			    sizeof (struct vtoc32), flag))
				return (EFAULT);
			break;
		}

		case DDI_MODEL_NONE:
			if (ddi_copyout(&vtoc, (void *)arg,
			    sizeof (struct vtoc), flag))
				return (EFAULT);
			break;
		}
		return (0);
#else /* ! _MULTI_DATAMODEL */
		i = sizeof (struct vtoc);
		if (ddi_copyout(&vtoc, (caddr_t)arg, i, flag))
#endif /* _MULTI_DATAMODEL */

#elif defined(_SUNOS_VTOC_16)
		mutex_exit(SD_MUTEX);

		if (ddi_copyout(&(un->un_vtoc), (caddr_t)arg,
		    sizeof (struct vtoc), flag))
#else
#error "No VTOC format defined."
#endif
			return (EFAULT);
		else
			return (0);

	case DKIOCSVTOC:
		if ((un->un_solaris_size == 0) || (un->un_lbasize != DEV_BSIZE))
			return (EIO);

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct vtoc32 vtoc32;

			if (ddi_copyin((const void *)arg, &vtoc32,
			    sizeof (struct vtoc32), flag))
				return (EFAULT);
			vtoc32tovtoc(vtoc32, vtoc);
			break;
		}

		case DDI_MODEL_NONE:
			if (ddi_copyin((const void *)arg, &vtoc,
			    sizeof (struct vtoc), flag))
				return (EFAULT);
			break;
		}
#else /* ! _MULTI_DATAMODEL */
		if (ddi_copyin((const void *)arg, &vtoc,
		    sizeof (struct vtoc), flag))
			return (EFAULT);
#endif /* _MULTI_DATAMODEL */

		mutex_enter(SD_MUTEX);
		if (un->un_g.dkg_ncyl == 0) {
			mutex_exit(SD_MUTEX);
			return (EINVAL);
		}
		if ((i = sd_build_label_vtoc(un, &vtoc)) == 0) {
			if ((i = sd_write_label(dev)) == 0) {
				(void) sd_validate_geometry(un, SLEEP_FUNC);
			}
		}
		if (!ISREMOVABLE(un) && !ISCD(un) &&
		    (un->un_devid == NULL) &&
		    (un->un_options & SD_NOSERIAL)) {
			/* Get fab'd devid */
			if (sd_get_devid(un) != NULL)
				/* create fab'd devid */
				(void) sd_create_devid(un);
			if (un->un_devid)
			    (void) ddi_devid_register(SD_DEVINFO,
				un->un_devid);
		}
		mutex_exit(SD_MUTEX);
		return (i);

	case DKIOCG_PHYGEOM:
		{
		/*
		 * Return the driver's notion of the media's physical geometry.
		 */

#if defined(__sparc)
		break;
#elif defined(__i386) || defined(__ia64)
			struct dk_geom *dkgp = (struct dk_geom *)data;

			mutex_enter(SD_MUTEX);
			dkgp->dkg_ncyl	= un->un_pgeom.g_ncyl;
			dkgp->dkg_acyl	= un->un_pgeom.g_acyl;
			dkgp->dkg_pcyl	= dkgp->dkg_ncyl + dkgp->dkg_acyl;
			dkgp->dkg_nhead	= un->un_pgeom.g_nhead;
			dkgp->dkg_nsect	= un->un_pgeom.g_nsect;

			if (ddi_copyout((caddr_t)data, (caddr_t)arg,
			    sizeof (struct dk_geom), flag)) {
				mutex_exit(SD_MUTEX);
				return (EFAULT);
			} else {
				mutex_exit(SD_MUTEX);
				return (0);
			}
#else
#error One of __sparc or __i386 or __ia64 must be defined.
#endif
		}

	case DKIOCG_VIRTGEOM:
		{
		/*
		 * Return the driver's notion of the media's logical geometry.
		 */
#if defined(__sparc)
		break;
#elif defined(__i386) || defined(__ia64)
			struct dk_geom *dkgp = (struct dk_geom *)data;

			mutex_enter(SD_MUTEX);
			/*
			 * If there is no HBA geometry available, or
			 * if the HBA returned us something that doesn't
			 * really fit into an Int 13/function 8 geometry
			 * result, just fail the ioctl.  See PSARC 1998/313.
			 */
			if (un->un_lgeom.g_nhead == 0 ||
			    un->un_lgeom.g_nsect == 0 ||
			    un->un_lgeom.g_ncyl > 1024) {
				mutex_exit(SD_MUTEX);
				return (EINVAL);
			}
			dkgp->dkg_ncyl	= un->un_lgeom.g_ncyl;
			dkgp->dkg_acyl	= un->un_lgeom.g_acyl;
			dkgp->dkg_pcyl	= dkgp->dkg_ncyl + dkgp->dkg_acyl;
			dkgp->dkg_nhead	= un->un_lgeom.g_nhead;
			dkgp->dkg_nsect	= un->un_lgeom.g_nsect;

			if (ddi_copyout((caddr_t)data, (caddr_t)arg,
			    sizeof (struct dk_geom), flag)) {
				mutex_exit(SD_MUTEX);
				return (EFAULT);
			} else {
				mutex_exit(SD_MUTEX);
				return (0);
			}
#else
#error One of __sparc or __i386 or __ia64 must be defined.
#endif
		}

	case DKIOCADDBAD:
		break;

	case DKIOCPARTINFO:
		{
		/*
		 * return parameters describing the selected disk slice.
		 */
#if defined(__sparc)
		break;
#elif defined(__i386) || defined(__ia64)
			struct part_info p;
			int part;

			part = SDPART(dev);

			/* don't check un_solaris_size for pN */
			if (part < P0_RAW_DISK && un->un_solaris_size == 0)
				return (EIO);

			p.p_start = (daddr_t)un->un_offset[part];
			p.p_length = (int)un->un_map[part].dkl_nblk;
			if (ddi_copyout(&p, (caddr_t)arg, sizeof (p),
			    flag))
				return (EFAULT);
			else
				return (0);
#else
#error One of __sparc or __i386 or __ia64 must be defined.
#endif
		}

	case DKIOCLOCK:
		return (sd_lock_unlock(dev, SD_REMOVAL_PREVENT));

	case DKIOCUNLOCK:
		return (sd_lock_unlock(dev, SD_REMOVAL_ALLOW));

	case DKIOCSTATE:
		if (ddi_copyin((caddr_t)arg, &state, sizeof (int),
		    flag))
			return (EFAULT);

		if ((i = sd_check_media(dev, state))) {
			return (i);
		}

		if (ddi_copyout(&un->un_mediastate, (caddr_t)arg,
		    sizeof (int), flag)) {
				return (EFAULT);
		}
		return (0);

	case DKIOCREMOVABLE:
		if (ISREMOVABLE(un)) {
			i = 1;
		} else {
			i = 0;
		}
		if (ddi_copyout(&i, (caddr_t)arg, sizeof (int),
		    flag)) {
			return (EFAULT);
		}
		return (0);

	/*
	 * The Following Four Ioctl's are needed by the HADF Folks
	 */
	case MHIOCENFAILFAST:
		if ((i = drv_priv(cred_p)) != EPERM) {
			int	mh_time;

			if (ddi_copyin((caddr_t)arg,
				&mh_time, sizeof (int), flag))
				return (EFAULT);
			if (mh_time) {
				mutex_enter(SD_MUTEX);
				un->un_resvd_status |= SD_FAILFAST;
				mutex_exit(SD_MUTEX);
				/*
				 * If mh_time is INT_MAX, then this
				 * ioctl is being used for SCSI-3 PGR
				 * purposes, and we don't need to spawn
				 * watch thread.
				 */
				if (mh_time != INT_MAX) {
					i = sd_check_mhd(dev, mh_time);
				}
			} else {
				(void) sd_check_mhd(dev, 0);
				mutex_enter(SD_MUTEX);
				un->un_resvd_status &= ~SD_FAILFAST;
				mutex_exit(SD_MUTEX);
			}
		}
		return (i);

	case MHIOCTKOWN:
		if ((i = drv_priv(cred_p)) != EPERM) {
			struct	mhioctkown	*p = NULL;

			if (arg != NULL) {
				if (ddi_copyin((caddr_t)arg, (caddr_t)data,
				    sizeof (struct mhioctkown), flag))
					return (EFAULT);
				p = (struct mhioctkown *)data;
			}
			i = sd_take_ownership(dev, p);
			mutex_enter(SD_MUTEX);
			if (i == 0) {
				un->un_resvd_status |= SD_RESERVE;
				reinstate_resv_delay = (p != NULL &&
				    p->reinstate_resv_delay != 0) ?
				    p->reinstate_resv_delay * 1000 :
				    SD_REINSTATE_RESV_DELAY;
				/*
				 * Give the scsi_watch routine interval set by
				 * the MHIOCENFAILFAST ioctl precedence here.
				 */
				if ((un->un_resvd_status & SD_FAILFAST) == 0) {
					mutex_exit(SD_MUTEX);
					(void) sd_check_mhd(dev,
					    reinstate_resv_delay/1000);
				} else {
					mutex_exit(SD_MUTEX);
				}
				(void) scsi_reset_notify(ROUTE,
				    SCSI_RESET_NOTIFY,
				    sd_mhd_reset_notify_cb, (caddr_t)un);
			} else {
				un->un_resvd_status &= ~SD_RESERVE;
				mutex_exit(SD_MUTEX);
			}
		}
		return (i);

	case MHIOCRELEASE:
	{
		int		resvd_status_save;
		timeout_id_t	resvd_timeid_save;
		if ((i = drv_priv(cred_p)) != EPERM) {

			mutex_enter(SD_MUTEX);
			resvd_status_save = un->un_resvd_status;
			un->un_resvd_status &= ~(SD_RESERVE | SD_LOST_RESERVE |
						SD_WANT_RESERVE);
			if (un->un_resvd_timeid) {
				resvd_timeid_save = un->un_resvd_timeid;
				un->un_resvd_timeid = 0;
				mutex_exit(SD_MUTEX);
				(void) untimeout(resvd_timeid_save);
			} else {
				mutex_exit(SD_MUTEX);
			}
			/*
			 * destroy any pending timeout thread that may
			 * be attempting to reinstate reservation on
			 * this device.
			 */
			sd_timeout_destroy(dev);

			if ((i = sd_reserve_release(dev, SD_RELEASE)) == 0) {
				mutex_enter(SD_MUTEX);
				if ((un->un_mhd_token) &&
				    ((un->un_resvd_status & SD_FAILFAST)
				    == 0)) {
					mutex_exit(SD_MUTEX);
					(void) sd_check_mhd(dev, 0);
				} else {
					mutex_exit(SD_MUTEX);
				}
				(void) scsi_reset_notify(ROUTE,
					SCSI_RESET_CANCEL,
					sd_mhd_reset_notify_cb, (caddr_t)un);
			} else {
				/*
				 * XXX sd_mhd_watch_cb() will restart the
				 * resvd recover timeout thread.
				 */
				mutex_enter(SD_MUTEX);
				un->un_resvd_status = resvd_status_save;
				mutex_exit(SD_MUTEX);
			}
		}
		return (i);
	}
	case MHIOCSTATUS:
		if ((i = drv_priv(cred_p)) != EPERM) {
			mutex_enter(SD_MUTEX);
			i = sd_unit_ready(dev);
			mutex_exit(SD_MUTEX);
			if (i != 0 && i != EACCES) {
				return (i);
			} else if (i == EACCES) {
				*rval_p = 1;
			}
			return (0);
		}
		return (i);

	case MHIOCQRESERVE:
		if ((i = drv_priv(cred_p)) != EPERM) {
			i = sd_reserve_release(dev, SD_RESERVE);
		}
		return (i);

	case MHIOCREREGISTERDEVID:
		if (drv_priv(cred_p) == EPERM)
			return (EPERM);
		if (ISREMOVABLE(un) || ISCD(un))
			return (ENOTTY);
		i = 0;
		mutex_enter(SD_MUTEX);
		if ((un->un_devid == NULL) &&
		    (un->un_options & SD_NOSERIAL)) {
			if ((i = sd_read_deviceid(un)) == NULL)
			    (void) ddi_devid_register(SD_DEVINFO, un->un_devid);
			else if (i == EINVAL) {
			    if (sd_create_devid(un) != NULL) {
				i = 0;
				(void) ddi_devid_register(SD_DEVINFO,
					un->un_devid);
			    }
			}
		/*
		 * After we change sd to get WWN's, we can retrieve the
		 * WWN again here.
		 * } else if (!(un->un_options & SD_NOSERIAL)) {
		 *  (void) ddi_devid_unregister(SD_DEVINFO);
		 *  (void) ddi_devid_register(SD_DEVINFO, un->un_devid);
		 */
		}
		mutex_exit(SD_MUTEX);
		return (i);

	case MHIOCGRP_INKEYS:
		if (((i = drv_priv(cred_p)) != EPERM) && arg != NULL) {
			mhioc_inkeys_t inkeys;
			if (un->un_reservation_type == SD_SCSI2_RESERVATION) {
				return (ENOTSUP);
			}
#ifdef _MULTI_DATAMODEL
			switch (ddi_model_convert_from(flag & FMODELS)) {
				case DDI_MODEL_ILP32: {
					struct mhioc_inkeys32 {
						uint32_t	generation;
						caddr32_t	li;
					} inkeys32;

					if (ddi_copyin((void *)arg,
					    (void *)&inkeys32,
					    sizeof (struct mhioc_inkeys32),
					    flag)) {
						return (EFAULT);
					}
					inkeys.li =
					    (mhioc_key_list_t *)inkeys32.li;
					i = sd_prin(dev, SD_READ_KEYS,
					    (void *)&inkeys, flag);
					if (i) {
						return (i);
					}
					inkeys32.generation =
					    inkeys.generation;
					if (ddi_copyout((void *)&inkeys32,
					    (void *)arg,
					    sizeof (struct mhioc_inkeys32),
					    flag)) {
						return (EFAULT);
					}
				}
				break;
				case DDI_MODEL_NONE:
					if (ddi_copyin((void *)arg,
					    (void *)&inkeys,
					    sizeof (mhioc_inkeys_t), flag)) {
						return (EFAULT);
					}
					i = sd_prin(dev, SD_READ_KEYS,
					    (void *)&inkeys, flag);
					if (i) {
						return (i);
					}
					if (ddi_copyout((void *)&inkeys,
					    (void *)arg,
					    sizeof (mhioc_inkeys_t), flag)) {
						return (EFAULT);
					}
				break;
			}
#else /* ! _MULTI_DATAMODEL */
			if (ddi_copyin((void *)arg, (void *)&inkeys,
			    sizeof (mhioc_inkeys_t), flag)) {
				return (EFAULT);
			}
			i = sd_prin(dev, SD_READ_KEYS,
			    (void *)&inkeys, flag);
			if (i) {
				return (i);
			}
			if (ddi_copyout((void *)&inkeys, (void *)arg,
			    sizeof (mhioc_inkeys_t), flag)) {
				return (EFAULT);
			}
#endif /* _MULTI_DATAMODEL */
		}
		return (i);

	case MHIOCGRP_INRESV:
		if (((i = drv_priv(cred_p)) != EPERM) && arg != NULL) {
			mhioc_inresvs_t inresvs;
			if (un->un_reservation_type == SD_SCSI2_RESERVATION) {
				return (ENOTSUP);
			}
#ifdef _MULTI_DATAMODEL
			switch (ddi_model_convert_from(flag & FMODELS)) {
				case DDI_MODEL_ILP32: {
					struct mhioc_inresvs32 {
						uint32_t	generation;
						caddr32_t	li;
					} inresvs32;

					if (ddi_copyin((void *)arg,
					    (void *)&inresvs32,
					    sizeof (struct mhioc_inresvs32),
					    flag)) {
						return (EFAULT);
					}
					inresvs.li =
					    (mhioc_resv_desc_list_t *)
					    inresvs32.li;
					i = sd_prin(dev, SD_READ_RESV,
					    (void *)&inresvs, flag);
					if (i) {
						return (i);
					}
					inresvs32.generation =
					    inresvs.generation;
					if (ddi_copyout((void *)&inresvs32,
					    (void *)arg,
					    sizeof (struct mhioc_inresvs32),
					    flag)) {
						return (EFAULT);
					}
				}
				break;
				case DDI_MODEL_NONE:
					if (ddi_copyin((void *)arg,
					    (void *)&inresvs,
					    sizeof (mhioc_inresvs_t),
					    flag)) {
						return (EFAULT);
					}
					i = sd_prin(dev, SD_READ_RESV,
					    (void *)&inresvs, flag);
					if (i) {
						return (i);
					}
					if (ddi_copyout((caddr_t)&inresvs,
					    (caddr_t)arg,
					    sizeof (mhioc_inresvs_t),
					    flag)) {
						return (EFAULT);
					}
				break;
			}
#else /* ! _MULTI_DATAMODEL */
			if (ddi_copyin((caddr_t)arg, (caddr_t)&inresvs,
			    sizeof (mhioc_inresvs_t), flag)) {
				return (EFAULT);
			}
			i = sd_prin(dev, SD_READ_RESV,
			    (void *)&inresvs, flag);
			if (i) {
				return (i);
			}
			if (ddi_copyout((caddr_t)&inresvs, (caddr_t)arg,
			    sizeof (mhioc_inresvs_t), flag)) {
				return (EFAULT);
			}
#endif /* ! _MULTI_DATAMODEL */
		}
		return (i);
	case MHIOCGRP_REGISTER:
		if ((i = drv_priv(cred_p)) != EPERM) {
			if (un->un_reservation_type == SD_SCSI2_RESERVATION) {
				return (ENOTSUP);
			}
			if (arg != NULL) {
				mhioc_register_t *p;
				if (ddi_copyin((caddr_t)arg, (caddr_t)data,
				    sizeof (mhioc_register_t), flag)) {
					return (EFAULT);
				}
				p = (mhioc_register_t *)data;
				i = sd_prout(dev, SD_SCSI3_REGISTER,
				    (void *)p);
			}
		}
		return (i);

	case MHIOCGRP_RESERVE:
		if ((i = drv_priv(cred_p)) != EPERM) {
			if (un->un_reservation_type == SD_SCSI2_RESERVATION) {
				return (ENOTSUP);
			}
			if (arg != NULL) {
				mhioc_resv_desc_t *p;
				if (ddi_copyin((caddr_t)arg, (caddr_t)data,
				    sizeof (mhioc_resv_desc_t), flag)) {
					return (EFAULT);
				}
				p = (mhioc_resv_desc_t *)data;
				i = sd_prout(dev, SD_SCSI3_RESERVE,
				    (void *)p);
			}
		}
		return (i);

	case MHIOCGRP_PREEMPTANDABORT:
		if ((i = drv_priv(cred_p)) != EPERM) {
			if (un->un_reservation_type == SD_SCSI2_RESERVATION) {
				return (ENOTSUP);
			}
			if (arg != NULL) {
				mhioc_preemptandabort_t *p;
				if (ddi_copyin((caddr_t)arg, (caddr_t)data,
				    sizeof (mhioc_preemptandabort_t), flag)) {
					return (EFAULT);
				}
				p = (mhioc_preemptandabort_t *)data;
				i = sd_prout(dev, SD_SCSI3_PREEMPTANDABORT,
				    (void *)p);
			}
		}
		return (i);

	case USCSICMD: {
#ifdef _MULTI_DATAMODEL
		/*
		 * For use when a 32 bit app makes a call into a
		 * 64 bit ioctl
		 */
		struct uscsi_cmd32	uscsi_cmd_32_for_64;
		struct uscsi_cmd32	*ucmd32 = &uscsi_cmd_32_for_64;
		model_t			model;

#endif /* _MULTI_DATAMODEL */

		if (drv_priv(cred_p) != 0) {
			return (EPERM);
		}

		/*
		 * Run a generic ucsi.h command.
		 */
		scmd = (struct uscsi_cmd *)data;

#ifdef _MULTI_DATAMODEL
		switch (model = ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32:
		{
			if (ddi_copyin((caddr_t)arg, ucmd32,
			    sizeof (*ucmd32), flag)) {
				return (EFAULT);
			}
			/*
			 * Convert the ILP32 uscsi data from the
			 * application to LP64 for internal use.
			 */
			uscsi_cmd32touscsi_cmd(ucmd32, scmd);
			break;
		}
		case DDI_MODEL_NONE:
			if (ddi_copyin((caddr_t)arg, scmd, sizeof (*scmd),
			    flag)) {
				return (EFAULT);
			}
			break;
		}

#else /* ! _MULTI_DATAMODEL */
		if (ddi_copyin((caddr_t)arg, scmd, sizeof (*scmd), flag)) {
			return (EFAULT);
		}
#endif /* _MULTI_DATAMODEL */

		scmd->uscsi_flags &= ~USCSI_NOINTR;
		uioseg = (flag & FKIOCTL) ? UIO_SYSSPACE : UIO_USERSPACE;

		if (un->un_format_in_progress) {
			return (EAGAIN);
		}

		/*
		 * Gotta do the ddi_copyin() here on the uscsi_cdb so that
		 * we will have a valid cdb[0] to test.
		 */
		if ((ddi_copyin(scmd->uscsi_cdb, cdb, CDB_GROUP0, flag) == 0) &&
		    (cdb[0] == SCMD_FORMAT)) {
			mutex_enter(SD_MUTEX);
			un->un_format_in_progress = 1;
			mutex_exit(SD_MUTEX);
			i = sdioctl_cmd(dev, scmd, uioseg, uioseg, uioseg);
			mutex_enter(SD_MUTEX);
			un->un_format_in_progress = 0;
			mutex_exit(SD_MUTEX);
		} else {
			/*
			 * It's OK to fall into here even if the ddi_copyin()
			 * on the uscsi_cdb above fails, because sdioctl_cmd()
			 * does this same copyin and will return the EFAULT
			 * if it fails.
			 */
			i = sdioctl_cmd(dev, scmd, uioseg, uioseg, uioseg);
		}

#ifdef _MULTI_DATAMODEL
		switch (model) {
		case DDI_MODEL_ILP32:
			/*
			 * Convert back to ILP32 before copyout to the
			 * application
			 */
			uscsi_cmdtouscsi_cmd32(scmd, ucmd32);

			if (ddi_copyout(ucmd32, (caddr_t)arg,
			    sizeof (*ucmd32), flag)) {
				if (i != 0)
					i = EFAULT;
			}
			break;

		case DDI_MODEL_NONE:
			if (ddi_copyout(scmd, (caddr_t)arg, sizeof (*scmd),
			    flag)) {
				if (i != 0)
					i = EFAULT;
			}
			break;
		}
#else /* ! _MULTI_DATAMODE */
		if (ddi_copyout(scmd, (caddr_t)arg, sizeof (*scmd),
		    flag)) {
			if (i != 0)
				i = EFAULT;
		}
#endif /* _MULTI_DATAMODEL */

		return (i);
	}

	case CDROMPAUSE:
	case CDROMRESUME:	/* no data passed */

		if (!ISCD(un))
			break;
		return (sr_pause_resume(dev, cmd));

	case CDROMPLAYMSF:

		if (!ISCD(un))
			break;
		return (sr_play_msf(dev, (caddr_t)arg, flag));

	case CDROMPLAYTRKIND:

		/*
		 * not supported on ATAPI CD drives,
		 * use CDROMPLAYMSF instead
		 */
		if (!ISCD(un) || SD_IS_ATAPI(un))
			break;
		return (sr_play_trkind(dev, (caddr_t)arg, flag));

	case CDROMREADTOCHDR:

		if (!ISCD(un))
			break;
		return (sr_read_tochdr(dev, (caddr_t)arg, flag));

	case CDROMREADTOCENTRY:

		if (!ISCD(un))
			break;
		return (sr_read_tocentry(dev, (caddr_t)arg, flag));

	case CDROMSTOP:		/* no data passed */

		if (!ISCD(un))
			break;
		return (sd_start_stop(dev, SD_STOP));

	case CDROMSTART:	/* no data passed */

		if (!ISCD(un))
			break;
		return (sd_start_stop(dev, SD_START));

	case FDEJECT:	/* for eject command */
	case DKIOCEJECT:
	case CDROMEJECT:

		if (!ISREMOVABLE(un))
			break;
		return (sd_eject(dev));

	case CDROMVOLCTRL:

		if (!ISCD(un))
			break;
		return (sr_volume_ctrl(dev, (caddr_t)arg, flag));

	case CDROMSUBCHNL:

		if (!ISCD(un))
			break;
		return (sr_read_subchannel(dev, (caddr_t)arg, flag));

	case CDROMREADMODE2:

		if (!ISCD(un))
			break;
		/*
		 * If the drive supports READ CD use that instead of
		 * switch the LBA size via a MODE SELECT Block Descriptor
		 */
		if (SD_READ_CD(un))
			return (sr_read_cd_mode2(dev, (caddr_t)arg, flag));
		return (sr_read_mode2(dev, (caddr_t)arg, flag));

	case CDROMREADMODE1:

		if (!ISCD(un))
			break;
		return (sr_read_mode1(dev, (caddr_t)arg, flag));

#ifdef CDROMREADOFFSET
	case CDROMREADOFFSET:

		if (!ISCD(un))
			break;
		return (sr_read_sony_session_offset(dev, (caddr_t)arg, flag));
#endif

	case CDROMSBLKMODE:
		/*
		 * There is no means of changing block size in case of atapi
		 * drives, thus return ENOTTY if drive type is atapi
		 */
		if (SD_IS_ATAPI(un))
			break;
		mutex_enter(SD_MUTEX);
		if ((!(un->un_exclopen & (1<<part))) || (un->un_ncmds > 0)) {
			mutex_exit(SD_MUTEX);
			return (EINVAL);
		}
		mutex_exit(SD_MUTEX);
		/*FALLTHROUGH*/

	case CDROMGBLKMODE:
		/*
		 * READ commands to an ATAPI CD driver are required to
		 * always transfer 2k of CDROM data. Therefore changing the
		 * LBA size via a MODE SELECT Block Descriptor to
		 * access the other recording modes isn't supported
		 */
		if (!ISCD(un) || SD_IS_ATAPI(un))
			break;
		return (sr_change_blkmode(dev, cmd, arg, flag));

	case CDROMGDRVSPEED:
	case CDROMSDRVSPEED:
		if (!ISCD(un))
			break;
		if (SD_IS_ATAPI(un))
			return (sr_set_speed(dev, cmd, arg, flag));
		return (sr_change_speed(dev, cmd, arg, flag));

	case CDROMCDDA:
		if (!ISCD(un))
			break;
		return (sr_read_cdda(dev, (caddr_t)arg, flag));

	case CDROMCDXA:
		if (!ISCD(un))
			break;
		return (sr_read_cdxa(dev, (caddr_t)arg, flag));

	case CDROMSUBCODE:
		if (!ISCD(un))
			break;
		return (sr_read_all_subcodes(dev, (caddr_t)arg, flag));

	/* 2 IOCTLs to control whether writes to a ROD-device */
	/* are done with Erase Bypass Bit (EBP) set or unset.  */

	case DKIOC_EBP_ENABLE:
		if (!ISROD(un))
			return (EINVAL);
		un->ebp_enabled = 1;
		return (0);

	case DKIOC_EBP_DISABLE:
		if (!ISROD(un))
			return (EINVAL);
		un->ebp_enabled = 0;
		return (0);

	case USCSIGETRQS:
	{
		struct uscsi_rqs	urqs;
		struct uscsi_rqs	*urqs_ptr = &urqs;
		ushort_t		len;

		if (ddi_copyin((caddr_t)arg, urqs_ptr,
		    sizeof (struct uscsi_rqs), flag)) {
		    return (EFAULT);
		}
		urqs_ptr->rqs_flags = (int)un->un_rqs_state &
		    (SD_RQS_OVR | SD_RQS_VALID);

		if (urqs_ptr->rqs_buflen <= SENSE_LENGTH) {
		    len = urqs_ptr->rqs_buflen;
		    urqs_ptr->rqs_resid = 0;
		} else {
		    len = SENSE_LENGTH;
		    urqs_ptr->rqs_resid = urqs_ptr->rqs_buflen
			    - SENSE_LENGTH;
		}

		un->un_rqs_state |= SD_RQS_READ;
		un->un_rqs_state &= ~(SD_RQS_OVR);

		if (ddi_copyout(&urqs, (caddr_t)arg,
		    sizeof (urqs), flag)) {
		    return (EFAULT);
		}

		if (ddi_copyout(un->un_uscsi_rqs_buf,
		    (caddr_t)urqs_ptr->rqs_bufaddr, len, flag)) {
		    return (EFAULT);
		}

		return (0);
	}

	default:
		break;
	}
	return (ENOTTY);
}

/*
 * Run a command for user (from sdioctl) or from someone else in the driver.
 *
 * space is for address space of cdb
 * addr_flag is for address space of the buffer
 */
static int
sdioctl_cmd(dev_t dev, struct uscsi_cmd *in,
    enum uio_seg cdbspace, enum uio_seg dataspace, enum uio_seg rqbufspace)
{
	register struct buf *bp;
	struct scsi_pkt *pkt;
	struct uscsi_cmd *scmd;
	caddr_t cdb;
	int err, rw;
	int rqlen;
	char *krqbuf = NULL;
	int flags = 0;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	/*
	 * Is this a request to reset the bus?
	 * If so, we need go no further.
	 */
	if (in->uscsi_flags & (USCSI_RESET|USCSI_RESET_ALL)) {
		int flag = ((in->uscsi_flags & USCSI_RESET_ALL)) ?
			RESET_ALL : RESET_TARGET;
		err = (scsi_reset(ROUTE, flag)) ? 0 : EIO;
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"reset %s %s\n",
			(flag == RESET_ALL) ? "all" : "target",
			(err == 0) ? "ok" : "failed");
		return (err);
	}

	/*
	 * In order to not worry about where the uscsi structure
	 * came from (or where the cdb it points to came from)
	 * we're going to make kmem_alloc'd copies of them
	 * here. This will also allow reference to the data
	 * they contain long after this process has gone to
	 * sleep and its kernel stack has been unmapped, etc.
	 */

	scmd = in;

	/*
	 * First do some sanity checks for USCSI commands
	 */
	if (scmd->uscsi_cdblen <= 0) {
		return (EINVAL);
	}

	/*
	 * now make copies of the uscsi structure
	 */
	cdb = kmem_zalloc((size_t)scmd->uscsi_cdblen, KM_SLEEP);
	if (cdbspace == UIO_SYSSPACE) {
		flags |= FKIOCTL;
	}
	if (ddi_copyin(scmd->uscsi_cdb, cdb, (uint_t)scmd->uscsi_cdblen,
		flags)) {
		kmem_free(cdb, (size_t)scmd->uscsi_cdblen);
		return (EFAULT);
	}

	scmd = kmem_alloc(sizeof (*scmd), KM_SLEEP);
	bcopy(in, scmd, sizeof (*scmd));
	scmd->uscsi_cdb = cdb;
	rw = (scmd->uscsi_flags & USCSI_READ) ? B_READ : B_WRITE;

	/*
	 * Get the 'special' buffer...
	 */
	mutex_enter(SD_MUTEX);
	while (un->un_sbuf_busy) {
		if (cv_wait_sig(&un->un_sbuf_cv, SD_MUTEX) == 0) {
			kmem_free(scmd->uscsi_cdb, (size_t)scmd->uscsi_cdblen);
			kmem_free(scmd, sizeof (*scmd));
			mutex_exit(SD_MUTEX);
			return (EINTR);
		}
	}
	un->un_sbuf_busy = 1;
	bp = un->un_sbufp;
	mutex_exit(SD_MUTEX);

	/*
	 * Initialize Request Sense buffering, if requested.
	 * For user processes, allocate a kernel copy of the sense buffer.
	 * Note that in->uscsi_rqlen is still intact, and is used later
	 * for sizing the number of rqsense bytes to return to the caller.
	 */
	if ((scmd->uscsi_flags & USCSI_RQENABLE) &&
			scmd->uscsi_rqlen && scmd->uscsi_rqbuf) {
		krqbuf = kmem_zalloc(SENSE_LENGTH, KM_SLEEP);
		if (rqbufspace == UIO_USERSPACE) {
			un->un_srqbufp = krqbuf;
			scmd->uscsi_rqlen = SENSE_LENGTH;
			scmd->uscsi_rqresid = SENSE_LENGTH;
		} else {
			uchar_t rlen = min(SENSE_LENGTH, scmd->uscsi_rqlen);
			scmd->uscsi_rqlen = rlen;
			scmd->uscsi_rqresid = rlen;
			un->un_srqbufp = krqbuf;
		}
	} else {
		scmd->uscsi_rqlen = 0;
		scmd->uscsi_rqresid = 0;
	}

	/*
	 * Force asynchronous mode, if necessary.  Doing this here
	 * has the unfortunate effect of running other queued
	 * commands async also, but since the main purpose of this
	 * capability is downloading new drive firmware, we can
	 * probably live with it.
	 */
	if (scmd->uscsi_flags & USCSI_ASYNC) {
		if (scsi_ifgetcap(ROUTE, "synchronous", 1) == 1) {
			if (scsi_ifsetcap(ROUTE, "synchronous", 0, 1) == 1) {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
					"forced async ok\n");
			} else {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
					"forced async failed\n");
				err = EINVAL;
				goto done;
			}
		}
	}

	/*
	 * Re-enable synchronous mode, if requested
	 */
	if (scmd->uscsi_flags & USCSI_SYNC) {
		if (scsi_ifgetcap(ROUTE, "synchronous", 1) == 0) {
			int i = scsi_ifsetcap(ROUTE, "synchronous", 1, 1);
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"re-enabled sync %s\n",
				(i == 1) ? "ok" : "failed");
		}
	}

	/*
	 * If we're going to do actual I/O, let physio do all the right things
	 */
	if (scmd->uscsi_buflen) {
		auto struct iovec aiov;
		auto struct uio auio;
		register struct uio *uio = &auio;

		bzero(&auio, sizeof (struct uio));
		bzero(&aiov, sizeof (struct iovec));
		aiov.iov_base = scmd->uscsi_bufaddr;
		aiov.iov_len = scmd->uscsi_buflen;
		uio->uio_iov = &aiov;

		uio->uio_iovcnt = 1;
		uio->uio_resid = scmd->uscsi_buflen;
		uio->uio_segflg = dataspace;
		uio->uio_loffset = 0;
		uio->uio_fmode = 0;

		/*
		 * Let physio do the rest...
		 */
		bp->av_back = NO_PKT_ALLOCATED;
		bp->b_forw = (struct buf *)scmd;
		err = physio(sdstrategy, bp, dev, rw, sduscsimin, uio);

	} else {
		/*
		* We have to mimic what physio would do here! Argh!
		*/
		bp->av_back = NO_PKT_ALLOCATED;
		bp->b_forw = (struct buf *)scmd;
		bp->b_flags = B_BUSY | rw;
		bp->b_edev = dev;
		bp->b_dev = cmpdev(dev);	/* maybe unnecessary */
		bp->b_bcount = bp->b_blkno = 0;
		(void) sdstrategy(bp);
		err = biowait(bp);
	}

done:
	/*
	 * get the status block, if any, and
	 * release any resources that we had.
	 */

	in->uscsi_status = 0;
	rqlen = scmd->uscsi_rqlen - scmd->uscsi_rqresid;
	rqlen = min(((int)in->uscsi_rqlen), rqlen);

	in->uscsi_resid = bp->b_resid;

	if ((pkt = BP_PKT(bp)) != NULL) {
		in->uscsi_status = SCBP_C(pkt);
		in->uscsi_resid = bp->b_resid;
		scsi_destroy_pkt(pkt);
		bp->av_back = NO_PKT_ALLOCATED;
		/*
		 * Update the Request Sense status and resid
		 */
		in->uscsi_rqresid = in->uscsi_rqlen - rqlen;
		in->uscsi_rqstatus = scmd->uscsi_rqstatus;
		/*
		 * Copy out the sense data
		 */
		if (in->uscsi_rqbuf && rqlen) {
			if (ddi_copyout(krqbuf, in->uscsi_rqbuf, rqlen,
				(rqbufspace == UIO_USERSPACE) ? 0 : FKIOCTL)) {
				err = EFAULT;
			}
		}
	}


	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "sdioctl_cmd status = 0x%x\n", scmd->uscsi_status);
	if (DEBUGGING && rqlen != 0) {
		int i, n, len;
		char *data = krqbuf;
		scsi_log(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"  rqstatus=0x%x  rqlen=0x%x  rqresid=0x%x\n",
			in->uscsi_rqstatus, in->uscsi_rqlen,
			in->uscsi_rqresid);
		len = (int)in->uscsi_rqlen - in->uscsi_rqresid;
		for (i = 0; i < len; i += 16) {
			n = min(16, len-i);
			clean_print(SD_DEVINFO, sd_label, CE_NOTE,
				"  ", &data[i], n);
		}
	}

	/*
	 * Tell anybody who cares that the buffer is now free
	 */
	mutex_enter(SD_MUTEX);
	un->un_sbuf_busy = 0;
	un->un_srqbufp = NULL;
	cv_signal(&un->un_sbuf_cv);
	mutex_exit(SD_MUTEX);
	if (krqbuf) {
		kmem_free(krqbuf, SENSE_LENGTH);
	}
	kmem_free(scmd->uscsi_cdb, (size_t)scmd->uscsi_cdblen);
	kmem_free(scmd, sizeof (*scmd));
	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "sdioctl_cmd returns %d\n", err);
	return (err);
}

/*
 * sdrunout:
 *	the callback function for resource allocation
 *
 * XXX it would be preferable that sdrunout() scans the whole
 *	list for possible candidates for sdstart(); this avoids
 *	that a bp at the head of the list whose request cannot be
 *	satisfied is retried again and again
 */
/*ARGSUSED*/
static int
sdrunout(caddr_t arg)
{
	register int serviced;
	register struct scsi_disk *un;
	register struct diskhd *dp;

	TRACE_1(TR_FAC_SCSI, TR_SDRUNOUT_START, "sdrunout_start: arg %x", arg);
	serviced = 1;

	un = (struct scsi_disk *)arg;
	dp = &un->un_utab;

	/*
	 * We now support passing a structure to the callback
	 * routine.
	 */
	ASSERT(un != NULL);
	mutex_enter(SD_MUTEX);
	if ((un->un_ncmds < un->un_throttle) && (dp->b_forw == NULL)) {
		sdstart(un);
	}
	if (un->un_state == SD_STATE_RWAIT) {
		serviced = 0;
	}
	mutex_exit(SD_MUTEX);
	TRACE_1(TR_FAC_SCSI, TR_SDRUNOUT_END,
	    "sdrunout_end: serviced %d", serviced);
	return (serviced);
}


/*
 * This routine called to see whether unit is (still) there. Must not
 * be called when un->un_sbufp is in use, and must not be called with
 * an unattached disk. Soft state of disk is restored to what it was
 * upon entry- up to caller to set the correct state.
 *
 * We enter with the disk mutex held.
 */

static int
sd_unit_ready(dev_t dev)
{
	auto struct uscsi_cmd scmd, *com = &scmd;
	auto char cmdblk[CDB_GROUP0];
	int error;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	/*
	 * Now that we protect the special buffer with
	 * a mutex, we could probably do a mutex_tryenter
	 * on it here and return failure if it were held...
	 */

	ASSERT(mutex_owned(SD_MUTEX));

	bzero(cmdblk, CDB_GROUP0);
	bzero(com, sizeof (struct uscsi_cmd));
	cmdblk[0] = (char)SCMD_TEST_UNIT_READY;
	com->uscsi_flags = USCSI_SILENT|USCSI_WRITE;
	com->uscsi_cdb = cmdblk;
	com->uscsi_cdblen = CDB_GROUP0;

	mutex_exit(SD_MUTEX);
	error = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	if (error && com->uscsi_status == STATUS_RESERVATION_CONFLICT)
		error = EACCES;
	mutex_enter(SD_MUTEX);
	return (error);
}

/*
 * This routine starts the drive, stops the drive or ejects the disc
 */
static int
sd_start_stop(dev_t dev, int  mode)
{
	int r;
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP0];

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	bzero(cdb, CDB_GROUP0);
	cdb[0] = SCMD_START_STOP;
	cdb[4] = (char)(mode & 0xff);

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;
	com->uscsi_timeout = 90;

	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (r);
}

/*
 * This routine ejects the CDROM disc
 */
static int
sd_eject(dev_t dev)
{
	int	err;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	if (un->un_state == SD_STATE_OFFLINE)
		return (ENXIO);

	/* first, unlocks the eject */
	if ((err = sd_lock_unlock(dev, SD_REMOVAL_ALLOW)) != 0) {
		return (err);
	}

	/* then ejects the disc */
	if ((err = (sd_start_stop(dev, SD_EJECT))) == 0) {
		mutex_enter(SD_MUTEX);
		sd_ejected(un);
		un->un_mediastate = DKIO_EJECTED;
		cv_broadcast(&un->un_state_cv);
		mutex_exit(SD_MUTEX);
	}
	return (err);
}

/*
 * Check Write Protection of a Removable Media Disk
 * We issue a mode sense for page 2, disconnect page, and only want
 * the header; byte 2, bit 8 is the write protect bit
 */
#define	WRITE_PROTECT 0x80

static int
sd_check_wp(dev_t dev, int use_group2)
{
	auto struct uscsi_cmd scmd, *com = &scmd;
	auto char cmdblk[CDB_GROUP2];
	caddr_t header;
	int rval;
	uchar_t	device_specific;
	uchar_t	cdblen;
	int	hdrlen;

	bzero(cmdblk, sizeof (cmdblk));
	if (use_group2) {
		/*
		 * ATAPI devices don't support MODEPAGE_DISCO_RECO.
		 * All devices support MODEPAGE_ERR_RECOVER, use it instead.
		 */
		hdrlen = MODE_HEADER_LENGTH_GRP2;
		cdblen = CDB_GROUP2;
		cmdblk[0] = SCMD_MODE_SENSE2;
		cmdblk[2] = MODEPAGE_ERR_RECOVER;
		cmdblk[8] = MODE_HEADER_LENGTH_GRP2;
	} else {
		hdrlen = MODE_HEADER_LENGTH;
		cdblen = CDB_GROUP0;
		cmdblk[0] = SCMD_MODE_SENSE;
		cmdblk[2] = MODEPAGE_DISCO_RECO;
		cmdblk[4] = MODE_HEADER_LENGTH;
	}

	header =  kmem_zalloc((size_t)hdrlen, KM_SLEEP);
	bzero(com, sizeof (struct uscsi_cmd));
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;
	com->uscsi_cdb = cmdblk;
	com->uscsi_bufaddr = header;
	com->uscsi_cdblen = cdblen;
	com->uscsi_buflen = hdrlen;
	com->uscsi_timeout = 15;

	if (sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
	    UIO_SYSSPACE) != 0) {
		/*
		 * Write protect mode sense failed - oh well,
		 * not all disks understand this query.. so presume
		 * those that don't are writable anyway.
		 */
		rval = 0;

	} else {
		if (use_group2)
			device_specific = ((struct mode_header_grp2 *)header)->
						device_specific;
		else
			device_specific = ((struct mode_header *)header)->
						device_specific;

		if (device_specific & WRITE_PROTECT)
			rval = 1;
		else
			rval = 0;
	}

	kmem_free(header, hdrlen);
	return (rval);
}

/*
 * Disable read and write caching by tweeking WCE (write cache enable) and
 * RCD (read cache disable) bits in mode page 8 (MODEPAGE_CACHING).
 */
static int
sd_disable_caching(struct scsi_disk *un)
{
	caddr_t header;
	int	use_group2;
	size_t	buflen;
	int	mode_header_length;
	int	bd_len;
	struct mode_caching *mode_caching_page;
	int rval = 0;
	dev_t dev = makedevice(
	    ddi_name_to_major(ddi_get_name(SD_DEVINFO)),
	    ddi_get_instance(SD_DEVINFO) << SDUNIT_SHIFT);

	if (SD_GRP1_2_CDBS(un)) {
		mode_header_length = MODE_HEADER_LENGTH_GRP2;
		use_group2 = TRUE;
	} else {
		mode_header_length = MODE_HEADER_LENGTH;
		use_group2 = FALSE;
	}
	buflen = mode_header_length + MODE_BLK_DESC_LENGTH +
	    sizeof (struct mode_caching);

	/*
	 * Allocate memory for the retrieved mode page and its headers.  Set a
	 * pointer to the page itself.
	 */
	header = kmem_zalloc(buflen, KM_SLEEP);

	/*
	 * Do a test unit ready, otherwise a mode sense may not work if this
	 * is the first command sent to the device after boot.  The mutex is
	 * only to meet the assertion in the routine.
	 */
	mutex_enter(SD_MUTEX);
	(void) sd_unit_ready(dev);
	mutex_exit(SD_MUTEX);

	/*
	 * Get the information from the device.
	 */
	if (sd_mode_sense(dev, MODEPAGE_CACHING, header, buflen,
	    use_group2) != 0) {
		rval = 1;
		kmem_free(header, buflen);
		return (rval);
	}

	/*
	 * Determine size of Block Descriptors in order to locate
	 * the mode page data. ATAPI devices return 0, SCSI devices
	 * should return MODE_BLK_DESC_LENGTH.
	 */
	if (SD_GRP1_2_CDBS(un)) {
		register struct mode_header_grp2 *mhp;
		mhp = (struct mode_header_grp2 *)header;
		bd_len  = (mhp->bdesc_length_hi << 8) | mhp->bdesc_length_lo;
	} else {
		bd_len = ((struct mode_header *)header)->bdesc_length;

	}
	ASSERT(bd_len <= MODE_BLK_DESC_LENGTH);
	mode_caching_page = (struct mode_caching *)(header +
				mode_header_length + bd_len);

	/*
	 * Check the relevant bits on successful mode sense.  Tweek as needed.
	 */
	if ((mode_caching_page->wce) || !(mode_caching_page->rcd)) {

		/*
		 * Read or write caching is enabled.  Disable both of them.
		 */
		mode_caching_page->wce = 0;
		mode_caching_page->rcd = 1;

		/*
		 * Clear reserved bits before mode select.
		 */
		mode_caching_page->mode_page.ps = 0;

		/*
		 * Clear out mode header for mode select.
		 * The rest of the retrieved page will be reused.
		 */
		bzero(header, mode_header_length);

		/*
		 * Change the cache page to disable all caching.
		 */
		if (sd_mode_select(dev, header, buflen, SD_SAVE_PAGE,
		    use_group2) != 0) {
			rval = 1;
		}
	}

	kmem_free(header, buflen);
	return (rval);
}

/*
 * Lock or Unlock the door for removable media devices
 */

static int
sd_lock_unlock(dev_t dev, int flag)
{
	auto struct uscsi_cmd scmd, *com = &scmd;
	auto char cmdblk[CDB_GROUP0];

	bzero(cmdblk, CDB_GROUP0);
	bzero(com, sizeof (struct uscsi_cmd));
	cmdblk[0] = SCMD_DOORLOCK;
	cmdblk[4] = flag;
	com->uscsi_flags = USCSI_SILENT|USCSI_WRITE;
	com->uscsi_cdb = cmdblk;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_timeout = 15;

	return (sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE));

}

static int
sd_reserve_release(dev_t dev, int cmd)
{
	struct uscsi_cmd		uscsi_cmd;
	register struct uscsi_cmd	*com = &uscsi_cmd;
	register int			rval;
	char				cdb[CDB_GROUP0];

	GET_SOFT_STATE(dev);

#ifdef lint
	part = part;
#endif /* lint */

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_reserve_release: Entering ...\n");

	bzero(cdb, CDB_GROUP0);
	if (cmd == SD_RELEASE) {
		cdb[0] = SCMD_RELEASE;
	} else {
		cdb[0] = SCMD_RESERVE;
	}

	bzero(com, sizeof (struct uscsi_cmd));
	com->uscsi_flags = USCSI_SILENT|USCSI_WRITE;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_timeout = 5;

	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_SYSSPACE, UIO_SYSSPACE);
	if (rval && com->uscsi_status == STATUS_RESERVATION_CONFLICT)
		rval = EACCES;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_reserve_release: rval(1)=%d\n", rval);

	if (cmd == SD_TKOWN && rval != 0) {
		if (scsi_reset(ROUTE, RESET_TARGET) == 0) {
			if (scsi_reset(ROUTE, RESET_ALL) == 0) {
				return (EIO);
			}
		}

		bzero(com, sizeof (struct uscsi_cmd));
		com->uscsi_flags = USCSI_SILENT|USCSI_WRITE;
		com->uscsi_cdb = cdb;
		com->uscsi_cdblen = CDB_GROUP0;
		com->uscsi_timeout = 5;

		rval = sdioctl_cmd(dev, com,
			UIO_SYSSPACE, UIO_SYSSPACE, UIO_SYSSPACE);
		if (rval && com->uscsi_status == STATUS_RESERVATION_CONFLICT)
			rval = EACCES;

		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"sd_reserve_release: rval(2)=%d\n", rval);
	}
	return (rval);
}

/*
 * sd_take_ownership routine implements an algorithm to achieve a stable
 * reservation on the disk and makes sure that other host loses in
 * its attempts of re-reservation.
 * min_ownership_delay is the minimum amount of time for which the disk
 * must be reserved continuously devoid of resets before the MHIOCTKOWN
 * ioctl will return success.  max_ownership_delay indicates the amount of
 * time by which the take ownership should succeed or timeout with an error.
 */
static int
sd_take_ownership(dev_t	dev, struct mhioctkown *p)
{
	int	rval;
	int	err;
	clock_t	start_time;	/* starting time of this algorithm */
	clock_t	end_time;	/* time limit for giving up */
	clock_t	ownership_time;	/* time limit indicating stable ownership */
	clock_t	current_time;
	clock_t	previous_current_time;
	int	reservation_count = 0;
	int	min_ownership_delay =  6000000; /* in usec */
	int	max_ownership_delay = 30000000; /* in usec */

	GET_SOFT_STATE(dev);

#ifdef lint
	part = part;
#endif /* lint */

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_take_ownership: Entering ...\n");

	if ((rval = sd_reserve_release(dev, SD_TKOWN)) != 0) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"sd_take_ownership: return(1)=%d\n", rval);
		return (rval);
	}

	mutex_enter(SD_MUTEX);
	un->un_resvd_status |= SD_RESERVE;
	un->un_resvd_status &= ~(SD_LOST_RESERVE | SD_WANT_RESERVE |
		SD_RESERVATION_CONFLICT);
	mutex_exit(SD_MUTEX);

	if (p != NULL) {
		if (p->min_ownership_delay != 0)
			min_ownership_delay = p->min_ownership_delay * 1000;
		if (p->max_ownership_delay != 0)
			max_ownership_delay = p->max_ownership_delay * 1000;
	}
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_take_ownership: min, "
	    "max delays: %d, %d\n", min_ownership_delay, max_ownership_delay);

	(void) drv_getparm(LBOLT, &start_time);
	current_time = start_time;
	ownership_time = current_time + drv_usectohz(min_ownership_delay);
	end_time = start_time + drv_usectohz(max_ownership_delay);

	while (current_time - end_time < 0) {
		delay(drv_usectohz(500000));

		if ((err = sd_reserve_release(dev, SD_RESERVE)) != 0) {
			if ((sd_reserve_release(dev, SD_RESERVE)) != 0) {
				mutex_enter(SD_MUTEX);
				rval = (un->un_resvd_status &
				    SD_RESERVATION_CONFLICT) ? EACCES : EIO;
				mutex_exit(SD_MUTEX);
				break;
			}
		}
		previous_current_time = current_time;
		(void) drv_getparm(LBOLT, &current_time);
		mutex_enter(SD_MUTEX);
		if (err || (un->un_resvd_status & SD_LOST_RESERVE)) {
			(void) drv_getparm(LBOLT, &ownership_time);
			ownership_time += drv_usectohz(min_ownership_delay);
			reservation_count = 0;
		} else {
			reservation_count++;
		}
		un->un_resvd_status |= SD_RESERVE;
		un->un_resvd_status &= ~(SD_LOST_RESERVE | SD_WANT_RESERVE);
		mutex_exit(SD_MUTEX);

		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_take_ownership: ticks for loop iteration=%ld, "
		    "reservation=%s\n", (current_time - previous_current_time),
		    reservation_count ? "ok" : "reclaimed");

		if (current_time - ownership_time >= 0 &&
		    reservation_count >= 4) {
			rval = 0; /* Wow! Achieved a stable ownership */
			break;
		}
		if (current_time - end_time >= 0) {
			rval = EACCES; /* No ownership in max possible time */
			break;
		}
	}
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_take_ownership: return(2)=%d\n", rval);
	return (rval);
}

/*
 * flush any outstanding writes.
 */
static int
sd_synchronize_cache(dev_t dev)
{
	struct uscsi_cmd	uscsi_cmd;
	register struct uscsi_cmd	*com = &uscsi_cmd;
	char	cdb[CDB_GROUP1];
	int	error;
	char	rqbuf[SENSE_LENGTH];

	GET_SOFT_STATE(dev)
#ifdef lint
	part = part;
#endif /* lint */

	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_SYNCHRONIZE_CACHE;

	bzero(com, sizeof (struct uscsi_cmd));
	com->uscsi_flags = USCSI_SILENT|USCSI_WRITE|USCSI_RQENABLE;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_timeout = 240;
	com->uscsi_rqbuf = rqbuf;
	com->uscsi_rqlen = SENSE_LENGTH;

	error = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);

	if (error) {
		if ((com->uscsi_status == STATUS_RESERVATION_CONFLICT) ||
		((com->uscsi_status == STATUS_CHECK) &&
		((com->uscsi_rqlen - (char)com->uscsi_rqresid) > 0) &&
		((struct scsi_extended_sense *)rqbuf)->es_key ==
		KEY_ILLEGAL_REQUEST)) {
			error = 0;
		} else if (ISREMOVABLE(un)) {
			/*
			 * Ignore error if the media is not present.
			 */
			mutex_enter(SD_MUTEX);
			if (sd_unit_ready(dev)) {
				error = 0;
			}
			mutex_exit(SD_MUTEX);
		}
	}

	if (error) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"!sd_synchronize_cache failed (%d)\n", error);
	}

	return (error);
}

/*
 * Re-initialize the unit geometry in case of
 * power down or media change
 */
static int
sd_handle_mchange(struct scsi_disk *un)
{
	int rval = 0;
	struct scsi_capacity cbuf;
	struct scsi_pkt *pkt;
	caddr_t cdb;
	struct sd_errstats *stp;

	ASSERT(mutex_owned(SD_MUTEX));
	/*
	 * If not a CD or other removeable
	 * media device, do not have to do this
	 */
	if (!(ISREMOVABLE(un)))
		return (0);

	mutex_exit(SD_MUTEX);
	if ((rval = sd_read_capacity(un, &cbuf)) == 0) {
		mutex_enter(SD_MUTEX);
		un->un_capacity = cbuf.capacity;
		un->un_lbasize = cbuf.lbasize;
		stp = (struct sd_errstats *)un->un_errstats->ks_data;
		stp->sd_capacity.value.ui64 =
			(uint64_t)((uint64_t)un->un_capacity * DEV_BSIZE);
	} else {
		mutex_enter(SD_MUTEX);
		return (rval);
	}

	un->un_gvalid = FALSE;
	(void) sd_validate_geometry(un, NULL_FUNC);

	if (!un->un_gvalid) {
		rval = ENXIO;
		return (rval);
	}

	/*
	 * try to lock the door
	 */
	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "locking the drive door\n");
	pkt = scsi_init_pkt(ROUTE, NULL, NULL, CDB_GROUP0,
	    un->un_cmd_stat_size, PP_LEN, 0, NULL_FUNC, NULL);

	if (!pkt) {
		return (ENOMEM);
	}
	mutex_exit(SD_MUTEX);

	cdb = (caddr_t)CDBP(pkt);
	bzero(cdb, CDB_GROUP0);
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_DOORLOCK, 0, 0, 0);
	FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
	pkt->pkt_flags = FLAG_NOINTR;
	cdb[4] = SD_REMOVAL_PREVENT;
	if (sd_scsi_poll(un, pkt) || SCBP_C(pkt) != STATUS_GOOD) {
		/*
		 * Throw away Request Sense, we should probably check why
		 * we have a problem here.
		 */
		if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
			(void) sd_clear_cont_alleg(un, un->un_rqs);
		}
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "Prevent/Allow Medium Removal command failed\n");
		scsi_destroy_pkt(pkt);
		mutex_enter(SD_MUTEX);
		return (EIO);
	}
	scsi_destroy_pkt(pkt);

	mutex_enter(SD_MUTEX);
	return (rval);
}



/*
 * Handle unit attention in case of
 * media change or power down
 * returns: 0 on success, an error code on error.
 */
static int
sd_handle_ua(struct scsi_disk *un)
{
	int err = 0;
	int retry_count = 0;
	int retry_limit = SD_UNIT_ATTENTION_RETRY/10;

	ASSERT(mutex_owned(SD_MUTEX));
	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sd_handle_ua\n");

	/*
	 * When a reset is issued on a CDROM, it takes
	 * a long time to recover. First few attempts
	 * to read capacity and  other things related
	 * to handling unit attention fail
	 * (with a ASC 0x4 and ASCQ 0x1). In that
	 * case we want to do enough retries and we want to
	 * limit the retries in other cases of
	 * genuine failures like no media in drive.
	 */
	if (SD_RQSENSE->es_add_code == 0x29 ||
	    SD_RQSENSE->es_add_code == 0x28) {
		while (retry_count++ < retry_limit) {
			if ((err = sd_handle_mchange(un)) == 0) {
				break;
			}
			if (err == EAGAIN) {
				retry_limit = SD_UNIT_ATTENTION_RETRY;
			}
			drv_usecwait(500000);
		}
	}
	return (err);
}


/*
 * restart a cmd from timeout() context
 *
 * the cmd is expected to be in un_utab.b_forw. If this pointer is non-zero
 * a restart timeout request has been issued and no new timeouts should
 * be requested. b_forw is reset when the cmd eventually completes in
 * sddone_and_mutex_exit()
 */
static void
sdrestart(void *arg)
{
	struct scsi_disk *un = arg;
	struct buf *bp;
	int status;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sdrestart\n");

	mutex_enter(SD_MUTEX);
	un->un_reissued_timeid = 0;
	bp = un->un_utab.b_forw;
	if (bp) {
		register struct scsi_pkt *pkt = BP_PKT(bp);

		un->un_ncmds++;
		SD_DO_KSTATS(un, kstat_waitq_to_runq, bp);

		mutex_exit(SD_MUTEX);
		ASSERT(((pkt->pkt_flags & FLAG_SENSING) == 0) &&
			(pkt != un->un_rqs));

		pkt->pkt_flags |= FLAG_HEAD;

		bp->b_resid = pkt->pkt_resid;
		if ((status = scsi_transport(pkt)) != TRAN_ACCEPT) {
			mutex_enter(SD_MUTEX);
			SD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
			un->un_ncmds--;
			if (status == TRAN_BUSY) {
				if (un->un_throttle > 1) {
					ASSERT(un->un_ncmds >= 0);
					if (un->un_ncmds == 0) {
						un->un_throttle = 1;
					} else {
						un->un_throttle = un->un_ncmds;
					}
					if (un->un_reset_throttle_timeid == 0) {
						un->un_reset_throttle_timeid =
						timeout(sd_reset_throttle,
						un,
						sd_reset_throttle_timeout *
						drv_usectohz(1000000));
					}
					un->un_reissued_timeid =
					    timeout(sdrestart, un,
					    SD_BSY_TIMEOUT);
				} else {
					un->un_reissued_timeid =
					    timeout(sdrestart, un,
					    SD_BSY_TIMEOUT/500);
				}
				mutex_exit(SD_MUTEX);
				return;
			}
			SD_DO_ERRSTATS(un, sd_transerrs);
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "sdrestart transport failed (%x)\n", status);
			bp->b_resid = bp->b_bcount;
			SET_BP_ERROR(bp, EIO);

			SD_DO_KSTATS(un, kstat_waitq_exit, bp);
			sddone_and_mutex_exit(un, bp);
			return;
		}
		mutex_enter(SD_MUTEX);
	}
	mutex_exit(SD_MUTEX);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sdrestart done\n");
}

/*
 * This routine gets called to reset the throttle to its saved
 * value wheneven we lower the throttle.
 */
static void
sd_reset_throttle(void *arg)
{
	struct scsi_disk *un = arg;
	register struct diskhd *dp;

	mutex_enter(SD_MUTEX);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_reset_throttle: reseting the throttle from 0x%x to 0x%x\n",
		un->un_throttle, un->un_save_throttle);

	un->un_reset_throttle_timeid = 0;
	un->un_throttle = un->un_save_throttle;
	dp = &un->un_utab;

	/*
	 * start any commands that didn't start while throttling.
	 */
	if (dp->b_actf && (un->un_ncmds < un->un_throttle) &&
	    (dp->b_forw == NULL)) {
		sdstart(un);
	}
	mutex_exit(SD_MUTEX);
}


/*
 * This routine handles the case when a TRAN_BUSY is
 * returned by HBA.
 *
 * If there are some commands already in the transport, the
 * bp can be put back on queue and it will
 * be retried when the queue is emptied after command
 * completes. But if there is no command in the tranport
 * and it still return busy, we have to retry the command
 * after some time like 10ms.
 */
static void
sd_handle_tran_busy(struct buf *bp, struct diskhd *dp, struct scsi_disk *un)
{
	ASSERT(mutex_owned(SD_MUTEX));
	/*
	 * restart command
	 */
	sd_requeue_cmd(un, bp, SD_BSY_TIMEOUT/500);
	if (dp->b_forw != bp) {
		ASSERT(un->un_throttle >= 0);
		un->un_throttle = un->un_ncmds;
		if (un->un_reset_throttle_timeid == 0) {
			un->un_reset_throttle_timeid =
				timeout(sd_reset_throttle, un,
				sd_reset_throttle_timeout *
				drv_usectohz(1000000));
		}
	}
}

/*
 * sd_check_media():
 * Check periodically the media using scsi_watch service; this service
 * calls back after TUR and possibly request sense
 * the callback handler (sd_media_watch_cb()) decodes the request sense data
 * (if any)
 */
static int sd_media_watch_cb();

static int
sd_check_media(dev_t dev, enum dkio_state state)
{
	register struct scsi_disk *un;
	register int instance;
	register opaque_t token = NULL;
	int rval = 0;
	enum dkio_state prev_state;
	struct sd_errstats *stp;

	instance = SDUNIT(dev);
	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL) {
		return (ENXIO);
	}

	mutex_enter(SD_MUTEX);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_check_media: state=%x, mediastate=%x\n",
		state, un->un_mediastate);

	prev_state = un->un_mediastate;

	/*
	 * is there anything to do?
	 */
	if (state == un->un_mediastate || un->un_mediastate == DKIO_NONE) {
		/*
		 * submit the request to the scsi_watch service;
		 * scsi_media_watch_cb() does the real work
		 */
		mutex_exit(SD_MUTEX);
		token = scsi_watch_request_submit(SD_SCSI_DEVP,
			sd_check_media_time, SENSE_LENGTH,
			sd_media_watch_cb, (caddr_t)dev);
		mutex_enter(SD_MUTEX);
		if (token == NULL) {
			rval = EAGAIN;
			goto done;
		}

		/*
		 * if a prior request had been made, this will be the same
		 * token, as scsi_watch was designed that way.
		 */
		un->un_swr_token = token;
		un->un_specified_mediastate = state;

		/*
		 * now wait for media change
		 * we will not be signalled unless mediastate == state but it
		 * still better to test for this condition, since there
		 * is a 2 sec cv_broadcast delay when
		 *  mediastate == DKIO_INSERTED
		 */
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"waiting for media state change\n");
		while (un->un_mediastate == state) {
			if (cv_wait_sig(&un->un_state_cv, SD_MUTEX) == 0) {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"waiting for media state was interrupted\n");
				rval = EINTR;
				goto done;
			}
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "received signal, state=%x\n", un->un_mediastate);
		}
	}

	/*
	 * invalidate geometry
	 */
	if (prev_state == DKIO_INSERTED && un->un_mediastate == DKIO_EJECTED)
		sd_ejected(un);
	if (un->un_mediastate == DKIO_INSERTED && prev_state != DKIO_INSERTED) {
		auto struct scsi_capacity capbuf;

		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"media inserted\n");
		mutex_exit(SD_MUTEX);
		if ((rval = sd_read_capacity(un, &capbuf))) {
			mutex_enter(SD_MUTEX);
			goto done;
		} else {
			mutex_enter(SD_MUTEX);
		}

		un->un_capacity = capbuf.capacity;
		un->un_lbasize = capbuf.lbasize;
		un->un_gvalid = FALSE;
		(void) sd_validate_geometry(un, SLEEP_FUNC);
		mutex_exit(SD_MUTEX);
		rval = sd_lock_unlock(dev, SD_REMOVAL_PREVENT);
		mutex_enter(SD_MUTEX);
		goto done;
	}
done:
	if (token) {
		mutex_exit(SD_MUTEX);
		(void) scsi_watch_request_terminate(token,
				SCSI_WATCH_TERMINATE_WAIT);
		mutex_enter(SD_MUTEX);
		un->un_swr_token = (opaque_t)NULL;
	}

	/* Update iostat capacity, if media has been inserted */
	if (un->un_errstats) {
		stp = (struct sd_errstats *)un->un_errstats->ks_data;
		if (stp->sd_capacity.value.ui64 == 0 && un->un_capacity > 0) {
			stp->sd_capacity.value.ui64 =
				(uint64_t)((uint64_t)un->un_capacity *
				DEV_BSIZE);
		}
	}
	mutex_exit(SD_MUTEX);

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_check_media: done\n");
	return (rval);
}

/*
 * delayed cv_broadcast to allow for target to recover
 * from media insertion
 */
static void
sd_delayed_cv_broadcast(void *arg)
{
	struct scsi_disk *un = arg;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"delayed cv_broadcast\n");

	mutex_enter(SD_MUTEX);
	un->un_dcvb_timeid = 0;
	cv_broadcast(&un->un_state_cv);
	mutex_exit(SD_MUTEX);
}


/*
 * sd_media_watch_cb() is called by scsi_watch_thread for
 * verifying the request sense data (if any)
 */
#define	MEDIA_ACCESS_DELAY 2000000

static int
sd_media_watch_cb(caddr_t arg, struct scsi_watch_result *resultp)
{
	register struct scsi_status *statusp = resultp->statusp;
	register struct scsi_extended_sense *sensep = resultp->sensep;
	uchar_t actual_sense_length = resultp->actual_sense_length;
	register struct scsi_disk *un;
	enum dkio_state state = DKIO_NONE;
	register int instance;
	dev_t dev = (dev_t)arg;

	instance = SDUNIT(dev);
	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL) {
		return (-1);
	}

	mutex_enter(SD_MUTEX);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_media_watch_cb: status=%x, sensep=%p, len=%x\n",
			*((char *)statusp), (void *)sensep,
			actual_sense_length);

	/*
	 * if there was a check condition then sensep points to valid
	 * sense data
	 * if status was not a check condition but a reservation or busy
	 * status then the new state is DKIO_NONE
	 */
	if (sensep) {
		if (actual_sense_length >= (SENSE_LENGTH - 2)) {
			if (sensep->es_key == KEY_UNIT_ATTENTION) {
				if (sensep->es_add_code == 0x28) {
					state = DKIO_INSERTED;
				}
			} else if (sensep->es_add_code == 0x3a) {
				state = DKIO_EJECTED;
			}
		}
	} else if ((*((char *)statusp) == STATUS_GOOD) &&
		(resultp->pkt->pkt_reason == CMD_CMPLT)) {
		state = DKIO_INSERTED;
	}
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"state=%x, specified=%x\n",
		state, un->un_specified_mediastate);

	/*
	 * now signal the waiting thread if this is *not* the specified state;
	 * delay the signal if the state is DKIO_INSERTED
	 * to allow the target to recover
	 */
	if (state != un->un_specified_mediastate) {
		un->un_mediastate = state;
		if (state == DKIO_INSERTED) {
			/*
			 * delay the signal to give the drive a chance
			 * to do what it apparently needs to do
			 */
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"delayed cv_broadcast\n");
			if (un->un_dcvb_timeid == 0) {
				un->un_dcvb_timeid =
					timeout(sd_delayed_cv_broadcast, un,
				drv_usectohz((clock_t)MEDIA_ACCESS_DELAY));
			}
		} else {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"immediate cv_broadcast\n");
			cv_broadcast(&un->un_state_cv);
		}
	}
	mutex_exit(SD_MUTEX);
	return (0);
}

/*
 * multi-host disk watch
 * sd_check_mhd() sets up a request; sd_mhd_watch_cb() does the real work
 */

static int
sd_check_mhd(dev_t dev, int interval)
{
	register struct scsi_disk *un;
	register int instance;
	register opaque_t token;

	instance = SDUNIT(dev);
	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL) {
		return (ENXIO);
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_check_mhd: Entering(interval=%x) ...\n", interval);

	if (interval == 0) {
		mutex_enter(SD_MUTEX);
		if (un->un_mhd_token) {
			token = un->un_mhd_token;
			un->un_mhd_token = NULL;
			mutex_exit(SD_MUTEX);
			(void) scsi_watch_request_terminate(token,
					SCSI_WATCH_TERMINATE_WAIT);
			mutex_enter(SD_MUTEX);
		} else {
			mutex_exit(SD_MUTEX);
			return (0);
		}
		/*
		 * If the device is required to hold reservation while
		 * disabling failfast, we need to restart the scsi_watch
		 * routine with an interval of reinstate_resv_delay.
		 */
		if (un->un_resvd_status & SD_RESERVE) {
			interval = reinstate_resv_delay/1000;
		} else {
			mutex_exit(SD_MUTEX);
			return (0);
		}
		mutex_exit(SD_MUTEX);
	}

	/*
	 * adjust minimum time interval to 1 second,
	 * and convert from msecs to usecs
	 */
	if (interval > 0 && interval < 1000)
		interval = 1000;
	interval *= 1000;

	/*
	 * submit the request to the scsi_watch service
	 */
	token = scsi_watch_request_submit(SD_SCSI_DEVP,
		interval, SENSE_LENGTH, sd_mhd_watch_cb, (caddr_t)dev);
	if (token == NULL) {
		return (EAGAIN);
	}

	/*
	 * save token for termination later on
	 */
	mutex_enter(SD_MUTEX);
	un->un_mhd_token = token;
	mutex_exit(SD_MUTEX);
	return (0);
}

static int
sd_mhd_watch_cb(caddr_t arg, struct scsi_watch_result *resultp)
{
	struct scsi_status		*statusp = resultp->statusp;
	struct scsi_extended_sense	*sensep = resultp->sensep;
	struct scsi_pkt			*pkt = resultp->pkt;
	struct scsi_disk		*un;
	int				instance;
	uchar_t				actual_sense_length =
						resultp->actual_sense_length;
	dev_t				dev = (dev_t)arg;

	instance = SDUNIT(dev);
	if ((un = ddi_get_soft_state(sd_state, instance)) == NULL) {
		return (-1);
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_mhd_watch_cb: reason '%s', status '%s'\n",
		scsi_rname(pkt->pkt_reason),
		sd_sname(*((unsigned char *)statusp)));

	if (pkt->pkt_reason != CMD_CMPLT) {
		sd_mhd_watch_incomplete(un, pkt);
		return (0);

	} else if (*((unsigned char *)statusp) != STATUS_GOOD) {
		if (*((unsigned char *)statusp)
			== STATUS_RESERVATION_CONFLICT) {
			mutex_enter(SD_MUTEX);
			if ((un->un_resvd_status & SD_FAILFAST) &&
			    (sd_failfast_enable)) {
				cmn_err(CE_PANIC, "Reservation Conflict\n");
			}
			SD_DEBUG(SD_DEVINFO, sd_label,
				SCSI_DEBUG,
				"sd_mhd_watch_cb: "
				"Reservation Conflict\n");
			un->un_resvd_status |= SD_RESERVATION_CONFLICT;
			mutex_exit(SD_MUTEX);
		}
	}

	if (sensep) {
		if (actual_sense_length >= (SENSE_LENGTH - 2)) {
			mutex_enter(SD_MUTEX);
			if (sensep->es_add_code == 0x29 &&
				(un->un_resvd_status & SD_RESERVE)) {
				un->un_resvd_status |=
					(SD_LOST_RESERVE | SD_WANT_RESERVE);
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
					"sd_mhd_watch_cb: Lost Reservation\n");
			}
		} else {
			goto done;
		}
	} else {
		mutex_enter(SD_MUTEX);
	}

	if ((un->un_resvd_status & SD_RESERVE) &&
		(un->un_resvd_status & SD_LOST_RESERVE)) {
		if (un->un_resvd_status & SD_WANT_RESERVE) {
			/*
			 * Reschedule the timeout, a new reset occured in
			 * between last probe and this one.
			 * This will help other host if it is taking over disk
			 */
			if (un->un_resvd_timeid) {
				mutex_exit(SD_MUTEX);
				(void) untimeout(un->un_resvd_timeid);
				mutex_enter(SD_MUTEX);
				un->un_resvd_timeid = 0;
			}
			un->un_resvd_status &= ~SD_WANT_RESERVE;

		}
		if (un->un_resvd_timeid == 0) {
			un->un_resvd_timeid = timeout(sd_mhd_resvd_recover,
			    (void *)dev, drv_usectohz(reinstate_resv_delay));
		}
	}
	mutex_exit(SD_MUTEX);
done:
	return (0);
}

static void
sd_mhd_watch_incomplete(struct scsi_disk *un, struct scsi_pkt *pkt)
{
	int	be_chatty = (!(pkt->pkt_flags & FLAG_SILENT));
	int	perr = (pkt->pkt_statistics & STAT_PERR);
	int	reset_retval;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_mhd_watch_incomplete: Entering ...\n");

	mutex_enter(SD_MUTEX);

	if (un->un_state == SD_STATE_DUMPING) {
		mutex_exit(SD_MUTEX);
		return;
	}

	switch (pkt->pkt_reason) {
	case CMD_UNX_BUS_FREE:
		if (perr && be_chatty)
			be_chatty = 0;
		break;

	case CMD_TAG_REJECT:
		pkt->pkt_flags = 0;
		un->un_tagflags = 0;
		if (un->un_options & SD_QUEUEING) {
			un->un_throttle = min(un->un_throttle, 3);
		} else {
			un->un_throttle = 1;
		}
		mutex_exit(SD_MUTEX);
		(void) scsi_ifsetcap(ROUTE, "tagged-qing", 0, 1);
		mutex_enter(SD_MUTEX);
		break;

	case CMD_INCOMPLETE:
		/*
		 * selection did not complete; we don't want to go
		 * thru a bus reset for this reason
		 */
		if (pkt->pkt_state == STATE_GOT_BUS) {
			break;
		}
		/*FALLTHROUGH*/

	case CMD_TIMEOUT:
	default:
		/*
		 * the target may still be running the	command,
		 * so we should try and reset that target.
		 */
		if ((pkt->pkt_statistics &
			(STAT_BUS_RESET|STAT_DEV_RESET|STAT_ABORTED)) == 0) {
			mutex_exit(SD_MUTEX);
			reset_retval = 0;
			if (un->un_allow_bus_device_reset) {
				reset_retval = scsi_reset(ROUTE, RESET_TARGET);
			}
			if (reset_retval == 0) {
				(void) scsi_reset(ROUTE, RESET_ALL);
			}
			mutex_enter(SD_MUTEX);
		}
		break;
	}

	if ((pkt->pkt_reason == CMD_RESET) || (pkt->pkt_statistics &
	    (STAT_BUS_RESET | STAT_DEV_RESET))) {
		if ((un->un_resvd_status & SD_RESERVE) == SD_RESERVE) {
			un->un_resvd_status |=
			    (SD_LOST_RESERVE | SD_WANT_RESERVE);
			SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
			    "Lost Reservation\n");
		}
	}
	if (pkt->pkt_state == STATE_GOT_BUS) {

		/*
		 * Looks like someone turned off this shoebox.
		 */
		if (un->un_state != SD_STATE_OFFLINE) {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_mhd_watch_incomplete: "
				"Disk not responding to selection\n");
			New_state(un, SD_STATE_OFFLINE);
		}
	} else if (be_chatty) {

		/*
		 * suppress messages if they are all the same pkt reason;
		 * with TQ, many (up to 256) are returned with the same
		 * pkt_reason
		 */
		if ((pkt->pkt_reason != un->un_last_pkt_reason) ||
			(sd_error_level == SCSI_ERR_ALL)) {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_mhd_watch_incomplete: "
				"SCSI transport failed: reason '%s'\n",
				scsi_rname(pkt->pkt_reason));
		}
	}
	un->un_last_pkt_reason = pkt->pkt_reason;
	mutex_exit(SD_MUTEX);
}

static void
sd_mhd_resvd_recover(void *arg)
{
	dev_t				dev = (dev_t)arg;
	register struct scsi_disk	*un;
	register int			instance;
	struct sd_thr_request		*sd_treq;
	struct sd_thr_request		*sd_cur, *sd_prev;
	int				already_there = 0;



	instance = SDUNIT(dev);
	un = ddi_get_soft_state(sd_state, instance);

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		"sd_mhd_resvd_recover: Entering ...\n");

	mutex_enter(SD_MUTEX);
	un->un_resvd_timeid = 0;
	if (un->un_resvd_status & SD_WANT_RESERVE) {
		/*
		 * It is not appropriate to do a reserve, there was a reset
		 * recently. Allow the sd_mhd_watch_cb callback function to
		 * notice this and reschedule the timeout for reservation.
		 */
		mutex_exit(SD_MUTEX);
		return;
	}

	mutex_exit(SD_MUTEX);

	/*
	 * Add this device to the sd_timeout_request structure and
	 * the sd_timeout_thread should take care of the rest.
	 */
	sd_treq = (struct sd_thr_request *)
			kmem_zalloc(sizeof (struct sd_thr_request),
					KM_SLEEP);
	sd_treq->sd_thr_req_next = NULL;
	sd_treq->dev = dev;
	mutex_enter(&sd_tr.sd_timeout_mutex);
	if (sd_tr.sd_thr_req_head == NULL) {
		sd_tr.sd_thr_req_head = sd_treq;
	} else {
		sd_cur = sd_prev = sd_tr.sd_thr_req_head;
		for (; sd_cur != NULL;
		    sd_cur = sd_cur->sd_thr_req_next) {
			if (sd_cur->dev == dev) {
				/*
				 * already in Queue so don't log
				 * another request for the device
				 */
				already_there = 1;
				break;
			}
			sd_prev = sd_cur;
		}
		if (!already_there) {
			SD_DEBUG3(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"logging request for %lx\n", dev);
			sd_prev->sd_thr_req_next = sd_treq;
		} else {
			kmem_free(sd_treq, sizeof (struct sd_thr_request));
		}
	}

	/*
	 * create a kernel thread to do all the dirty work for us and
	 * free up the timeout thread. We shouldn't be blocking the
	 * the timeout thread while we go away to do the reserve etc.
	 */
	if (sd_tr.sd_timeout_thread == (kthread_t *)0) {
		if (sd_create_timeout_thread(sd_timeout_thread)) {
			/*
			 * we have already marked the device has lost
			 * reservation so we just return since there is
			 * little we can do. we just wait for the watch
			 * thread to get us back here to retry creating
			 * the thread.
			 */
			SD_DEBUG3(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_create_timeout thread failed\n");
			mutex_exit(&sd_tr.sd_timeout_mutex);
			return;
		}
	}

	/*
	 * Tell the sd_timeout_thread that it has work to do
	 */
	cv_signal(&sd_tr.sd_timeout_cv);
	mutex_exit(&sd_tr.sd_timeout_mutex);
}

static int
sd_create_timeout_thread(void (*func)())
{
	register int rval = 0;

	ASSERT(mutex_owned(&sd_tr.sd_timeout_mutex));
	if (sd_tr.sd_timeout_thread == (kthread_t *)0) {
		sd_tr.sd_timeout_thread = thread_create((caddr_t)NULL, 0,
			func, (caddr_t)0, 0, &p0, TS_RUN, v.v_maxsyspri - 2);
		/*
		 * If thread create fails we probably we return failure
		 */
		if (sd_tr.sd_timeout_thread == (kthread_t *)0) {
			if (sddebug == 3) {
				cmn_err(CE_NOTE,
					"sd_create_timeout_thread - failed\n");
			}
			rval = 1;
		}
	}
	return (rval);
}

static void
sd_timeout_thread()
{
	register struct	scsi_disk *un;
	register int	instance;
	register minor_t	minor;
	register struct sd_thr_request *sd_mhreq;

	/*
	 * wait for work
	 */
	mutex_enter(&sd_tr.sd_timeout_mutex);
	if (sd_tr.sd_thr_req_head == NULL) {
		cv_wait(&sd_tr.sd_timeout_cv, &sd_tr.sd_timeout_mutex);
	}

	/*
	 * loop while we have work
	 */
	while ((sd_tr.sd_thr_cur_req = sd_tr.sd_thr_req_head) != NULL) {
		minor = getminor(sd_tr.sd_thr_cur_req->dev);
		instance = (int)(minor >> SDUNIT_SHIFT);

		if ((un = ddi_get_soft_state(sd_state,
		    instance)) == NULL) {
			/*
			 * softstate structure is NULL so just
			 * dequeue the request and continue
			 */
			sd_tr.sd_thr_req_head =
					sd_tr.sd_thr_cur_req->sd_thr_req_next;
			kmem_free(sd_tr.sd_thr_cur_req,
				sizeof (struct sd_thr_request));
			continue;
		}

		/*
		 * dequeue the request
		 */
		sd_mhreq = sd_tr.sd_thr_cur_req;
		sd_tr.sd_thr_req_head = sd_tr.sd_thr_cur_req->sd_thr_req_next;
		mutex_exit(&sd_tr.sd_timeout_mutex);

		/*
		 * reclaim reservation only if SD_RESERVE is still
		 * set. There may have been a call to MHIOCRELEASE
		 * before we got here.
		 */
		mutex_enter(SD_MUTEX);
		if ((un->un_resvd_status & SD_RESERVE) == SD_RESERVE) {
			/*
			 * Note that we clear the SD_LOST_RESERVE flag
			 * before reclaiming reservation. If we do this
			 * after the call to sd_reserve_release we may
			 * fail to recognize a reservation loss in the
			 * window between pkt completion of reserve cmd
			 * and mutex_enter below.
			 */
			un->un_resvd_status &= ~SD_LOST_RESERVE;
			mutex_exit(SD_MUTEX);

			if (sd_reserve_release(sd_mhreq->dev,
			    SD_RESERVE) == 0) {
				mutex_enter(SD_MUTEX);
				un->un_resvd_status |= SD_RESERVE;
				mutex_exit(SD_MUTEX);
				SD_DEBUG3(SD_DEVINFO, sd_label,
					SCSI_DEBUG,
					"sd_timeout_thread: "
					"Reservation Recovered\n");
			} else {
				mutex_enter(SD_MUTEX);
				un->un_resvd_status |= SD_LOST_RESERVE;
				mutex_exit(SD_MUTEX);
				SD_DEBUG3(SD_DEVINFO, sd_label,
					SCSI_DEBUG,
					"sd_timeout_thread: Failed "
					"Reservation Recovery\n");
			}
		} else {
			mutex_exit(SD_MUTEX);
		}
		mutex_enter(&sd_tr.sd_timeout_mutex);
		ASSERT(sd_mhreq == sd_tr.sd_thr_cur_req);
		kmem_free(sd_mhreq, sizeof (struct sd_thr_request));
		sd_mhreq = sd_tr.sd_thr_cur_req = NULL;
		/*
		 * wakeup the destroy thread if anyone is waiting on
		 * us to complete.
		 */
		cv_signal(&sd_tr.sd_inprocess_cv);
		SD_DEBUG3(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"in timeout thread cv_signalling current request \n");
	}

	/*
	 * cleanup the sd_tr structure now that this thread will not exist
	 */
	ASSERT(sd_tr.sd_thr_req_head == NULL);
	ASSERT(sd_tr.sd_thr_cur_req == NULL);
	sd_tr.sd_timeout_thread = (kthread_t *)0;
	mutex_exit(&sd_tr.sd_timeout_mutex);
	thread_exit();
}

static void
sd_timeout_destroy(dev_t dev)
{
	struct sd_thr_request *sd_mhreq;
	struct sd_thr_request *sd_prev;

	/*
	 * destroy a request from list
	 */
	mutex_enter(&sd_tr.sd_timeout_mutex);
	if (sd_tr.sd_thr_cur_req && sd_tr.sd_thr_cur_req->dev == dev) {
		/*
		 * We are attempting to reinstate reservation for
		 * this device. We wait for sd_reserve_release()
		 * to return before we return.
		 */
		cv_wait(&sd_tr.sd_inprocess_cv, &sd_tr.sd_timeout_mutex);
	} else {
		sd_prev = sd_mhreq = sd_tr.sd_thr_req_head;
		for (; sd_mhreq != NULL; sd_mhreq = sd_mhreq->sd_thr_req_next) {
			if (sd_mhreq->dev == dev) {
				break;
			}
			sd_prev = sd_mhreq;
		}
		if (sd_mhreq != NULL) {
			sd_prev->sd_thr_req_next = sd_mhreq->sd_thr_req_next;
			kmem_free(sd_mhreq, sizeof (struct sd_thr_request));
		}
	}
	mutex_exit(&sd_tr.sd_timeout_mutex);
}


static void
sd_mhd_reset_notify_cb(caddr_t arg)
{
	struct scsi_disk	*un = (struct scsi_disk *)arg;

	mutex_enter(SD_MUTEX);
	if ((un->un_resvd_status & SD_RESERVE) == SD_RESERVE) {
		un->un_resvd_status |= (SD_LOST_RESERVE | SD_WANT_RESERVE);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_mhd_reset_notify_cb: Lost Reservation\n");
	}
	mutex_exit(SD_MUTEX);
}

static int
sd_handle_resv_conflict(struct scsi_disk *un, struct buf *bp)
{
	register struct scsi_pkt	*pkt;
	register int			action = COMMAND_DONE_ERROR;

	ASSERT(mutex_owned(SD_MUTEX));

	if (bp) {
		pkt = BP_PKT(bp);
	} else {
		return (COMMAND_DONE_ERROR);
	}

	/*
	 * If the command was PRIN/PROUT then  reservation conflict
	 * could be due to various reasons like, incorrect keys,
	 * not registered or not reserved etc. So, we return EACCES
	 * to the caller.
	 *
	 */
	if (un->un_reservation_type == SD_SCSI3_RESERVATION) {
		int cmd =
		    ((union scsi_cdb *)(pkt->pkt_cdbp))->cdb_un.cmd;
		if (cmd == SD_SCMD_PRIN || cmd == SD_SCMD_PROUT) {
			SET_BP_ERROR(bp, EACCES);
			return (COMMAND_DONE_ERROR);
		}
	}

	if (un->un_resvd_status & SD_FAILFAST) {
		if (sd_failfast_enable) {
			cmn_err(CE_PANIC, "Reservation Conflict\n");
		}
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"sd_handle_resv_conflict: "
			"Disk Reserved\n");
		SET_BP_ERROR(bp, EACCES);
	} else {
		clock_t	tval = 2;

		/*
		 * 1147670: retry only if sd_retry_on_reservation_conflict
		 * property is set (default is 1). Retry will not succeed
		 * on a disk reserved by another initiator. HA systems
		 * may reset this via sd.conf to avoid these retries.
		 */
		if (((int)PKT_GET_RETRY_CNT(pkt) < sd_retry_count) &&
		    (sd_retry_on_reservation_conflict != 0)) {

			PKT_INCR_RETRY_CNT(pkt, 1);
			/*
			 * restart command
			 */
			sd_requeue_cmd(un, bp, tval);
			action = JUST_RETURN;
		} else {
			SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"sd_handle_resv_conflict: "
				"Device Reserved \n");
		}
	}
	un->un_resvd_status |= SD_RESERVATION_CONFLICT;
	return (action);
}

/*
 * sd_check_pr routine sends SCSI-3 PRIN command to the
 * device in the poll mode. This routine is used find
 * out if the device supports scsi-3 type of reservation.
 */
static void
sd_check_pr(struct scsi_disk *un)
{
	struct scsi_pkt *pkt;
	caddr_t cdb;
	int i;
	struct buf *bp;

	bp = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
		MHIOC_RESV_KEY_SIZE, B_READ, NULL_FUNC, NULL);
	if (!bp) {
		return;
	}
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
	    bp, CDB_GROUP1, sizeof (struct scsi_arq_status), PP_LEN,
	    PKT_CONSISTENT, SLEEP_FUNC, NULL);
	if (!pkt) {
		scsi_free_consistent_buf(bp);
		return;
	}

	cdb = (caddr_t)CDBP(pkt);
	bzero(cdb, CDB_GROUP1);
	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
	    SD_SCMD_PRIN, 0, 0, 0);
	cdb[1] = SD_READ_KEYS;
	cdb[8] = MHIOC_RESV_KEY_SIZE;
	pkt->pkt_flags |= FLAG_SILENT;

	for (i = 0; i < 3; i++) {
		(void) sd_scsi_poll(un, pkt);
		if (SCBP(pkt)->sts_chk) {
			if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
				(void) sd_clear_cont_alleg(un, un->un_rqs);
			} else {
				struct scsi_arq_status *arqstat;
				struct scsi_extended_sense *sensep;
				arqstat =
				    (struct scsi_arq_status *)(pkt->pkt_scbp);
				sensep = &arqstat->sts_sensedata;
				if (sensep->es_key == KEY_ILLEGAL_REQUEST) {
					mutex_enter(SD_MUTEX);
					un->un_reservation_type =
					    SD_SCSI2_RESERVATION;
					mutex_exit(SD_MUTEX);
					break;
				}
			}
		} else {
			mutex_enter(SD_MUTEX);
			un->un_reservation_type = SD_SCSI3_RESERVATION;
			mutex_exit(SD_MUTEX);
			break;
		}
	}
	scsi_destroy_pkt(pkt);
	scsi_free_consistent_buf(bp);
}

/*
 * sd_prin routine sends SCSI-3 PRIN command to the device for
 * reading keys and the reservations. This routine also
 * takes care of the MULTI_DATAMODEL for 32/64 bit support.
 */
#define	SCSI3_RESV_DESC_LEN	16
static int
sd_prin(dev_t dev, int cmd, void  *p, int flag)
{
	struct uscsi_cmd 		uscsi_cmd;
	register struct uscsi_cmd	*com = &uscsi_cmd;
	register int			rval;
	char				cdb[CDB_GROUP1];
	caddr_t				*buf;

	GET_SOFT_STATE(dev);

#ifdef lint
	part = part;
#endif

	if ((buf = kmem_zalloc(sd_max_nodes * MHIOC_RESV_KEY_SIZE,
	    KM_SLEEP)) == NULL) {
		return (ENOMEM);
	}
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SD_SCMD_PRIN;
	cdb[1] = cmd;
	cdb[8] = sd_max_nodes * MHIOC_RESV_KEY_SIZE;

	bzero(com, sizeof (struct uscsi_cmd));

	com->uscsi_flags = USCSI_READ;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = (caddr_t)buf;
	com->uscsi_buflen = sd_max_nodes * MHIOC_RESV_KEY_SIZE;
	com->uscsi_timeout = sd_io_time;


	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_SYSSPACE, UIO_SYSSPACE);

	if (rval && com->uscsi_status == STATUS_RESERVATION_CONFLICT) {
		rval = EACCES;
	} else if (!rval) {
		if (cmd == SD_READ_KEYS) {
			sd_prin_readkeys_t *in;
			mhioc_inkeys_t *ptr;
			mhioc_key_list_t li;
			in = (sd_prin_readkeys_t *)buf;
			ptr = (mhioc_inkeys_t *)p;
			ptr->generation = in->generation;
			bzero(&li, sizeof (mhioc_key_list_t));
#ifdef _MULTI_DATAMODEL
			switch (ddi_model_convert_from(flag & FMODELS)) {
			case DDI_MODEL_ILP32: {
				struct mhioc_key_list32 {
					uint32_t	listsize;
					uint32_t	listlen;
					caddr32_t	list;
				} li32;


				if (ddi_copyin((void *)ptr->li, (void *)&li32,
				    sizeof (struct mhioc_key_list32), flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyin: mhioc_key_list32_t\n");
					rval = EFAULT;
					goto done;
				}
				li32.listlen = in->len / MHIOC_RESV_KEY_SIZE;
				if (ddi_copyout((void *)&li32, (void *)ptr->li,
				    sizeof (struct mhioc_key_list32), flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout:mhioc_key_list32_t\n");
					rval = EFAULT;
					goto done;
				}
				if (ddi_copyout((void *)&in->keylist,
				    (void *)li32.list,
				    min(li32.listlen * MHIOC_RESV_KEY_SIZE,
				    li32.listsize * MHIOC_RESV_KEY_SIZE),
				    flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout: keylist32\n");
					rval = EFAULT;
					goto done;
				}
			}
			break;
			case DDI_MODEL_NONE:

				if (ddi_copyin((void *)ptr->li, (void *)&li,
				    sizeof (mhioc_key_list_t), flag)) {
					SD_DEBUG(SD_DEVINFO,
					    sd_label, SCSI_DEBUG,
					    "ddi_copyin: mhioc_key_list_t\n");
					rval = EFAULT;
					goto done;
				}
				li.listlen = in->len / MHIOC_RESV_KEY_SIZE;
				if (ddi_copyout((void *)&li, (void *)ptr->li,
				    sizeof (mhioc_key_list_t), flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout: mhioc_key_list_t\n");
					rval = EFAULT;
					goto done;
				}
				if (ddi_copyout((void *)&in->keylist,
				    (void *)li.list,
				    min(li.listlen * MHIOC_RESV_KEY_SIZE,
				    li.listsize * MHIOC_RESV_KEY_SIZE), flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout: keylist\n");
					rval = EFAULT;
					goto done;
				}
			break;
			}
#else /* ! _MULTI_DATAMODEL */

			if (ddi_copyin((void *)ptr->li, (void *)&li,
			    sizeof (mhioc_key_list_t), flag)) {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"ddi_copyin: mhioc_key_list_t\n");
				rval = EFAULT;
				goto done;
			}
			li.listlen = in->len / MHIOC_RESV_KEY_SIZE;
			if (ddi_copyout((void *)&li, (void *)ptr->li,
			    sizeof (mhioc_key_list_t), flag)) {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"ddi_copyout: mhioc_key_list_t\n");
				rval = EFAULT;
				goto done;
			}
			if (ddi_copyout((void *)&in->keylist, (void *)li.list,
			    min(li.listlen * MHIOC_RESV_KEY_SIZE,
			    li.listsize * MHIOC_RESV_KEY_SIZE), flag)) {
				SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
				"ddi_copyout: keylist\n");
				rval = EFAULT;
				goto done;
			}
#endif /* _MULTI_DATAMODEL */
		} else {
			int i;
			sd_prin_readresv_t *in;
			sd_readresv_desc_t *readresv_ptr;
			mhioc_resv_desc_list_t resvlist;
			mhioc_resv_desc_t resvdesc;
			mhioc_inresvs_t *ptr;
			in = (sd_prin_readresv_t *)buf;
			ptr = (mhioc_inresvs_t *)p;
			ptr->generation = in->generation;
#ifdef _MULTI_DATAMODEL
			switch (ddi_model_convert_from(flag & FMODELS)) {
			case DDI_MODEL_ILP32: {
				struct mhioc_resv_desc_list32 {
					uint32_t	listsize;
					uint32_t	listlen;
					caddr32_t	list;
				} resvlist32;
				if (ddi_copyin((void *)ptr->li,
				    (void *)&resvlist32,
				    sizeof (struct mhioc_resv_desc_list32),
				    flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyin:"
					    "mhioc_resv_desc_list_t\n");
					rval = EFAULT;
					goto done;
				}
				resvlist32.listlen =
				    in->len / SCSI3_RESV_DESC_LEN;
				if (ddi_copyout((void *)&resvlist32,
				    (void *)ptr->li,
				    sizeof (struct mhioc_resv_desc_list32),
				    flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout:"
					    "mhioc_resv_desc_list_t\n");
					rval = EFAULT;
					goto done;
				}
				readresv_ptr = (sd_readresv_desc_t *)
				    &in->readresv_desc;
				for (i = 0; i < min(resvlist32.listlen,
				    resvlist32.listsize); i++) {
					bcopy(&readresv_ptr->resvkey,
					    &resvdesc.key, MHIOC_RESV_KEY_SIZE);
					resvdesc.type = readresv_ptr->type;
					resvdesc.scope = readresv_ptr->scope;
					resvdesc.scope_specific_addr =
					    readresv_ptr->scope_specific_addr;
					if (ddi_copyout((void *)&resvdesc,
					    (void *)(resvlist32.list +
					    i * sizeof (mhioc_resv_desc_t)),
					    sizeof (mhioc_resv_desc_t), flag)) {
						SD_DEBUG(SD_DEVINFO,
						    sd_label, SCSI_DEBUG,
						    "ddi_copyout: resvlist\n");
						rval = EFAULT;
						goto done;
					}
					readresv_ptr++;
				}
			}
			break;
			case DDI_MODEL_NONE:
				if (ddi_copyin((void *)ptr->li,
				    (void *)&resvlist,
				    sizeof (mhioc_resv_desc_list_t),
				    flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyin:"
					    "mhioc_resv_desc_list_t\n");
					rval = EFAULT;
					goto done;
				}
				resvlist.listlen =
				    in->len / SCSI3_RESV_DESC_LEN;
				if (ddi_copyout((void *)&resvlist,
				    (void *)ptr->li,
				    sizeof (mhioc_resv_desc_list_t),
				    flag)) {
					SD_DEBUG(SD_DEVINFO,
					    sd_label, SCSI_DEBUG,
					    "ddi_copyout:"
					    "mhioc_resv_desc_list_t\n");
					rval = EFAULT;
					goto done;
				}
				readresv_ptr = (sd_readresv_desc_t *)
				    &in->readresv_desc;
				for (i = 0; i < min(resvlist.listlen,
				    resvlist.listsize); i++) {
					bcopy(&readresv_ptr->resvkey,
					    &resvdesc.key, MHIOC_RESV_KEY_SIZE);
					resvdesc.type = readresv_ptr->type;
					resvdesc.scope = readresv_ptr->scope;
					resvdesc.scope_specific_addr =
					    readresv_ptr->scope_specific_addr;
					if (ddi_copyout((void *)&resvdesc,
					    (void *)((uint64_t)resvlist.list +
					    i * sizeof (mhioc_resv_desc_t)),
					    sizeof (mhioc_resv_desc_t), flag)) {
						SD_DEBUG(SD_DEVINFO,
						    sd_label, SCSI_DEBUG,
						    "ddi_copyout: resvlist\n");
						rval = EFAULT;
						goto done;
					}
					readresv_ptr++;
				}
			break;
			}
#else /* ! _MULTI_DATAMODEL */
			if (ddi_copyin(ptr->li, &resvlist,
			    sizeof (mhioc_resv_desc_list_t), flag)) {
				SD_DEBUG(SD_DEVINFO, sd_label,
				    SCSI_DEBUG,
				    "ddi_copyin:"
				    "mhioc_resv_desc_list_t\n");
				rval = EFAULT;
				goto done;
			}
			resvlist.listlen =
			    in->len / SCSI3_RESV_DESC_LEN;
			if (ddi_copyout((void *)&resvlist,
			    (void *)ptr->li,
			    sizeof (mhioc_resv_desc_list_t), flag)) {
				SD_DEBUG(SD_DEVINFO, sd_label,
				    SCSI_DEBUG,
				    "ddi_copyout:"
				    "mhioc_resv_desc_list_t\n");
				rval = EFAULT;
				goto done;
			}
			readresv_ptr = (sd_readresv_desc_t *)
			    &in->readresv_desc;
			for (i = 0; i < min(resvlist.listlen,
			    resvlist.listsize); i++) {
				bcopy(&readresv_ptr->resvkey,
				    &resvdesc.key, MHIOC_RESV_KEY_SIZE);
				resvdesc.type = readresv_ptr->type;
				resvdesc.scope = readresv_ptr->scope;
				resvdesc.scope_specific_addr =
				    readresv_ptr->scope_specific_addr;
				if (ddi_copyout((void *)&resvdesc,
				    (void *)((uint32_t)resvlist.list +
				    i * sizeof (mhioc_resv_desc_t)),
				    sizeof (mhioc_resv_desc_t), flag)) {
					SD_DEBUG(SD_DEVINFO, sd_label,
					    SCSI_DEBUG,
					    "ddi_copyout: resvlist\n");
					rval = EFAULT;
					goto done;
				}
				readresv_ptr++;
			}
#endif /* ! _MULTI_DATAMODEL */
		}
	}
done:
	kmem_free(buf, sd_max_nodes * MHIOC_RESV_KEY_SIZE);
	return (rval);
}
/*
 * sd_rout routine sends SCSI-3 PROUT commands to the device.
 */
static int
sd_prout(dev_t dev, int cmd, void * p)
{
	struct uscsi_cmd		uscsi_cmd;
	struct scsi_extended_sense 	sense;
	register struct uscsi_cmd	*com = &uscsi_cmd;
	sd_prout_t			*prout;
	register int			rval;
	char				cdb[CDB_GROUP1];

	if ((prout = kmem_zalloc(sizeof (sd_prout_t),
	    KM_SLEEP)) == NULL) {
		return (ENOMEM);
	}
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SD_SCMD_PROUT;
	cdb[1] = cmd;
	cdb[8] = 0x18;
	bzero(com, sizeof (struct uscsi_cmd));
	com->uscsi_flags = USCSI_RQENABLE|USCSI_WRITE;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = (char *)prout;
	com->uscsi_buflen = 0x18;
	com->uscsi_timeout = sd_io_time;
	com->uscsi_rqbuf = (caddr_t)&sense;

	switch (cmd) {
		case SD_SCSI3_REGISTER:
		{
			mhioc_register_t *ptr;
			ptr = (mhioc_register_t *)p;
			bcopy(ptr->oldkey.key,
			    prout->res_key, MHIOC_RESV_KEY_SIZE);
			bcopy(ptr->newkey.key,
			    prout->service_key, MHIOC_RESV_KEY_SIZE);
			prout->aptpl = ptr->aptpl;
		}
			break;
		case SD_SCSI3_RESERVE:
		case SD_SCSI3_RELEASE:
		{
			mhioc_resv_desc_t *ptr;
			ptr = (mhioc_resv_desc_t *)p;
			bcopy(ptr->key.key,
			    prout->res_key, MHIOC_RESV_KEY_SIZE);
			prout->scope_address = ptr->scope_specific_addr;
			cdb[2] = ptr->type;
		}
			break;
		case SD_SCSI3_PREEMPTANDABORT:
		{
			mhioc_preemptandabort_t *ptr;
			ptr = (mhioc_preemptandabort_t *)p;
			bcopy(ptr->resvdesc.key.key,
			    prout->res_key, MHIOC_RESV_KEY_SIZE);
			bcopy(ptr->victim_key.key,
			    prout->service_key, MHIOC_RESV_KEY_SIZE);
			prout->scope_address =
			    ptr->resvdesc.scope_specific_addr;
			cdb[2] = ptr->resvdesc.type;
			com->uscsi_flags |= USCSI_HEAD;
		}
			break;
		default:
			break;
	}
	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_SYSSPACE, UIO_SYSSPACE);

	if (rval && com->uscsi_status == STATUS_RESERVATION_CONFLICT) {
		rval = EACCES;
	}

	kmem_free(prout, sizeof (sd_prout_t));
	return (rval);
}

/*
 * CD rom functions
 */

/*
 * This routine does a pause or resume to the cdrom player. Only affect
 * audio play operation.
 */
static int
sr_pause_resume(dev_t dev, int mode)
{
	int	r;
	char	cdb[CDB_GROUP1];
	register struct uscsi_cmd *com;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_PAUSE_RESUME;
	if (mode == CDROMRESUME)
		cdb[8] = 1;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;
	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (r);
}

/*
 * This routine plays audio by msf
 */
static int
sr_play_msf(dev_t dev, caddr_t data, int flag)
{
	int r;
	char	cdb[CDB_GROUP1];
	struct cdrom_msf		msf_struct;
	register struct cdrom_msf	*msf = &msf_struct;
	register struct uscsi_cmd *com;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	if (ddi_copyin(data, msf, sizeof (struct cdrom_msf),
	    flag))
		return (EFAULT);

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_PLAYAUDIO_MSF;
	if (SD_PLAYMSF_BCD(un)) {
		cdb[3] = BYTE_TO_BCD(msf->cdmsf_min0);
		cdb[4] = BYTE_TO_BCD(msf->cdmsf_sec0);
		cdb[5] = BYTE_TO_BCD(msf->cdmsf_frame0);
		cdb[6] = BYTE_TO_BCD(msf->cdmsf_min1);
		cdb[7] = BYTE_TO_BCD(msf->cdmsf_sec1);
		cdb[8] = BYTE_TO_BCD(msf->cdmsf_frame1);
	} else {
		cdb[3] = msf->cdmsf_min0;
		cdb[4] = msf->cdmsf_sec0;
		cdb[5] = msf->cdmsf_frame0;
		cdb[6] = msf->cdmsf_min1;
		cdb[7] = msf->cdmsf_sec1;
		cdb[8] = msf->cdmsf_frame1;
	}
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;
	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (r);
}

/*
 * This routine plays audio by track/index
 */
static int
sr_play_trkind(dev_t dev, caddr_t data, int flag)
{
	int r;
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	struct cdrom_ti		ti_struct;
	register struct cdrom_ti	*ti = &ti_struct;

	if (ddi_copyin(data, ti, sizeof (struct cdrom_ti),
	    flag))
		return (EFAULT);
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_PLAYAUDIO_TI;
	cdb[4] = ti->cdti_trk0;
	cdb[5] = ti->cdti_ind0;
	cdb[7] = ti->cdti_trk1;
	cdb[8] = ti->cdti_ind1;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;
	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (r);
}

#ifdef FIXEDFIRMWARE
/*
 * This routine control the audio output volume
 */
static int
sr_volume_ctrl(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP2];
	struct cdrom_volctrl	volume;
	struct cdrom_volctrl	*vol = &volume;
	caddr_t buffer;
	int	rtn;
	struct scsi_disk *un;
	int	hdrlen;
	uchar_t	cdblen;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_volume_ctrl\n");

	if (ddi_copyin(data, vol, sizeof (struct cdrom_volctrl), flag))
		return (EFAULT);

	bzero(cdb, sizeof (cdb));
	if (SD_GRP1_2_CDBS(un)) {
		hdrlen = MODE_HEADER_LENGTH_GRP2;
		cdblen = CDB_GROUP2;
		cdb[0] = SCMD_MODE_SELECT2;
		cdb[1] = 0x10;
		cdb[8] = hdrlen + 16;
		buffer = kmem_zalloc((size_t)hdrlen + 16, KM_SLEEP);
		buffer[1] = hdrlen + 16;
	} else {
		hdrlen = MODE_HEADER_LENGTH;
		cdblen = CDB_GROUP0;
		cdb[0] = SCMD_MODE_SELECT;
		cdb[4] = hdrlen + 16;
		buffer = kmem_zalloc((size_t)hdrlen + 16, KM_SLEEP);
	}

	/*
	 * fill in the input data. Set the output channel 0, 1 to
	 * output port 0, 1 respestively. Set output channel 2, 3 to
	 * mute. The function only adjust the output volume for channel
	 * 0 and 1.
	 */
	buffer[hdrlen + 0] = 0xe;
	buffer[hdrlen + 1] = 0xe;
	buffer[hdrlen + 2] = 0x4;	/* set the immediate bit to 1 */
	buffer[hdrlen + 8] = 0x01;
	buffer[hdrlen + 9] = vol->channel0;
	buffer[hdrlen + 10] = 0x02;
	buffer[hdrlen + 11] = vol->channel1;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = cdblen;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = hdrlen + 16;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(buffer, 20);
	kmem_free(com, sizeof (*com));
	return (rtn);
}
#else
/*
 * This routine control the audio output volume
 */
static int
sr_volume_ctrl(dev_t dev, caddr_t  data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	struct cdrom_volctrl	volume;
	struct cdrom_volctrl	*vol = &volume;
	caddr_t buffer;
	int	rtn;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_volume_ctrl\n");

	if (ddi_copyin(data, vol, sizeof (struct cdrom_volctrl), flag))
		return (EFAULT);

	buffer = kmem_zalloc((size_t)18, KM_SLEEP);
	bzero(cdb, CDB_GROUP1);

	cdb[0] = 0xc9;	/* vendor unique command */
	cdb[8] = 0x12;

	/*
	 * fill in the input data. Set the output channel 0, 1 to
	 * output port 0, 1 respestively. Set output channel 2, 3 to
	 * mute. The function only adjust the output volume for channel
	 * 0 and 1.
	 */
	buffer[10] = 0x01;
	buffer[11] = vol->channel0;
	buffer[12] = 0x02;
	buffer[13] = vol->channel1;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = 0x12;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;
	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(buffer, 18);
	kmem_free(com, sizeof (*com));
	return (rtn);
}
#endif FIXEDFIRMWARE


static int
sr_read_subchannel(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	caddr_t buffer;
	int	rtn;
	struct	cdrom_subchnl	subchanel;
	struct	cdrom_subchnl	*subchnl = &subchanel;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_subchannel.\n");

	if (ddi_copyin(data, subchnl, sizeof (struct cdrom_subchnl), flag))
		return (EFAULT);

	buffer = kmem_zalloc((size_t)16, KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_READ_SUBCHANNEL;
	cdb[1] = (subchnl->cdsc_format & CDROM_LBA) ? 0 : 0x02;
	/*
	 * set the Q bit in byte 2 to 1.
	 */
	cdb[2] = 0x40;
	/*
	 * This byte (byte 3) specifies the return data format. Proposed
	 * by Sony. To be added to SCSI-2 Rev 10b
	 * Setting it to one tells it to return time-data format
	 */
	cdb[3] = 0x01;
	cdb[8] = 0x10;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = 0x10;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	subchnl->cdsc_audiostatus = buffer[1];
	subchnl->cdsc_trk = buffer[6];
	subchnl->cdsc_ind = buffer[7];
	subchnl->cdsc_adr = buffer[5] & 0xF0;
	subchnl->cdsc_ctrl = buffer[5] & 0x0F;
	if (subchnl->cdsc_format & CDROM_LBA) {
		subchnl->cdsc_absaddr.lba =
		    ((uchar_t)buffer[8] << 24) +
		    ((uchar_t)buffer[9] << 16) +
		    ((uchar_t)buffer[10] << 8) +
		    ((uchar_t)buffer[11]);
		subchnl->cdsc_reladdr.lba =
		    ((uchar_t)buffer[12] << 24) +
		    ((uchar_t)buffer[13] << 16) +
		    ((uchar_t)buffer[14] << 8) +
		    ((uchar_t)buffer[15]);
	} else if (SD_READSUB_BCD(un)) {
		subchnl->cdsc_absaddr.msf.minute = BCD_TO_BYTE(buffer[9]);
		subchnl->cdsc_absaddr.msf.second = BCD_TO_BYTE(buffer[10]);
		subchnl->cdsc_absaddr.msf.frame = BCD_TO_BYTE(buffer[11]);
		subchnl->cdsc_reladdr.msf.minute = BCD_TO_BYTE(buffer[13]);
		subchnl->cdsc_reladdr.msf.second = BCD_TO_BYTE(buffer[14]);
		subchnl->cdsc_reladdr.msf.frame = BCD_TO_BYTE(buffer[15]);
	} else {
		subchnl->cdsc_absaddr.msf.minute = buffer[9];
		subchnl->cdsc_absaddr.msf.second = buffer[10];
		subchnl->cdsc_absaddr.msf.frame = buffer[11];
		subchnl->cdsc_reladdr.msf.minute = buffer[13];
		subchnl->cdsc_reladdr.msf.second = buffer[14];
		subchnl->cdsc_reladdr.msf.frame = buffer[15];
	}
	kmem_free(buffer, 16);
	kmem_free(com, sizeof (*com));
	if (ddi_copyout(subchnl, data, sizeof (struct cdrom_subchnl), flag))
		return (EFAULT);
	return (rtn);
}


static int
sr_read_mode2(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	uchar_t	cdb[CDB_GROUP0];
	int	rtn;
	auto struct cdrom_read mode2_struct;
	register struct	 cdrom_read *mode2 = &mode2_struct;
	register struct scsi_disk *un;
#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_read32	cdrom_read32;
	struct cdrom_read32	*cdrd32 = &cdrom_read32;
#endif /* _MULTI_DATAMODEL */

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_mode2.\n");

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdrd32,
		    sizeof (*cdrd32), flag)) {
			return (EFAULT);
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_read32tocdrom_read(cdrd32, mode2);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, mode2, sizeof (*mode2), flag)) {
			return (EFAULT);
		}
		break;
	}

#else /* ! _MULTI_DATAMODEL */
	if (ddi_copyin(data, mode2, sizeof (*mode2), flag)) {
		return (EFAULT);
	}
#endif /* _MULTI_DATAMODEL */

	mode2->cdread_lba >>= 2;

	bzero(cdb, CDB_GROUP0);
	cdb[0] = SCMD_READ;
	cdb[1] = (uchar_t)((mode2->cdread_lba >> 16) & 0XFF);
	cdb[2] = (uchar_t)((mode2->cdread_lba >> 8) & 0xFF);
	cdb[3] = (uchar_t)(mode2->cdread_lba & 0xFF);
	cdb[4] = mode2->cdread_buflen / 2336;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = (caddr_t)cdb;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_bufaddr = mode2->cdread_bufaddr;
	com->uscsi_buflen = mode2->cdread_buflen;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	mutex_enter(SD_MUTEX);
	if (sr_sector_mode(dev, 2) == 0) {
		mutex_exit(SD_MUTEX);
		rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_USERSPACE,
			UIO_SYSSPACE);
		mutex_enter(SD_MUTEX);
		if (sr_sector_mode(dev, 0) != 0) {
			scsi_log(SD_DEVINFO, sd_label,
			    CE_WARN, "can't to switch back to mode 1\n");
			if (rtn == 0)
				rtn = EIO;
		}
	}
	mutex_exit(SD_MUTEX);
	kmem_free(com, sizeof (*com));
	return (rtn);
}


static int
sr_read_cd_mode2(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	uchar_t	cdb[CDB_GROUP5];
	int	rtn;
	auto struct cdrom_read mode2_struct;
	register struct	 cdrom_read *mode2 = &mode2_struct;
	register struct scsi_disk *un;
	int	nblocks;
#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_read32	cdrom_read32;
	struct cdrom_read32	*cdrd32 = &cdrom_read32;
#endif /* _MULTI_DATAMODEL */

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_cd_mode2.\n");

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdrd32,
		    sizeof (*cdrd32), flag)) {
			return (EFAULT);
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_read32tocdrom_read(cdrd32, mode2);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, mode2, sizeof (*mode2), flag)) {
			return (EFAULT);
		}
		break;
	}

#else /* ! _MULTI_DATAMODEL */
	if (ddi_copyin(data, mode2, sizeof (*mode2), flag)) {
		return (EFAULT);
	}
#endif /* _MULTI_DATAMODEL */

	bzero(cdb, sizeof (cdb));
	if (SD_READ_CD_XD4(un))
		cdb[0] = SCMD_READ_CDD4;
	else
		cdb[0] = SCMD_READ_CD;

	/*
	 * set exepected sector type to:
	 * 	2336 byte, Mode 2, Yellow Book sector
	 */
	cdb[1] = CDROM_SECTOR_TYPE_MODE2 << 2;

	/* set the start address */
	cdb[2] = (uchar_t)((mode2->cdread_lba >> 24) & 0XFF);
	cdb[3] = (uchar_t)((mode2->cdread_lba >> 16) & 0XFF);
	cdb[4] = (uchar_t)((mode2->cdread_lba >> 8) & 0xFF);
	cdb[5] = (uchar_t)(mode2->cdread_lba & 0xFF);

	/* set the transfer length */
	nblocks = mode2->cdread_buflen / 2336;
	cdb[6] = (uchar_t)(nblocks >> 16);
	cdb[7] = (uchar_t)(nblocks >> 8);
	cdb[8] = (uchar_t)nblocks;

	/* set the filter bits */
	cdb[9] = CDROM_READ_CD_USERDATA;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = (caddr_t)cdb;
	com->uscsi_cdblen = sizeof (cdb);
	com->uscsi_bufaddr = mode2->cdread_bufaddr;
	com->uscsi_buflen = mode2->cdread_buflen;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_USERSPACE, UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (rtn);
}

static int
sr_read_mode1(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	uchar_t	cdb[CDB_GROUP1];
	int	rtn;
	struct	cdrom_read	mode1_struct;
	struct	cdrom_read	*mode1 = &mode1_struct;
	struct scsi_disk *un;
#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_read32	cdrom_read32;
	struct cdrom_read32	*cdrd32 = &cdrom_read32;
#endif /* _MULTI_DATAMODEL */

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_mode1.\n");

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdrd32,
		    sizeof (*cdrd32), flag)) {
			return (EFAULT);
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_read32tocdrom_read(cdrd32, mode1);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, mode1,
		    sizeof (struct cdrom_read), flag))
		return (EFAULT);
	}

#else /* ! _MULTI_DATAMODEL */
	if (ddi_copyin(data, mode1, sizeof (struct cdrom_read), flag))
		return (EFAULT);
#endif /* _MULTI_DATAMODEL */

	bzero(cdb, sizeof (cdb));
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	if (SD_GRP1_2_CDBS(un)) {
		cdb[0] = SCMD_READ_G1;
		FORMG1ADDR((union scsi_cdb *)&cdb, mode1->cdread_lba);
		FORMG1COUNT((union scsi_cdb *)&cdb,
			    (mode1->cdread_buflen >> 11));
	} else {
		cdb[0] = SCMD_READ;
		FORMG0ADDR((union scsi_cdb *)&cdb, mode1->cdread_lba);
		FORMG0COUNT((union scsi_cdb *)&cdb,
			    (mode1->cdread_buflen >> 11));
	}

	com->uscsi_cdb = (caddr_t)cdb;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_bufaddr = mode1->cdread_bufaddr;
	com->uscsi_buflen = mode1->cdread_buflen;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_USERSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (rtn);
}


static int
sr_read_tochdr(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	caddr_t buffer;
	int	rtn;
	struct cdrom_tochdr	header;
	struct cdrom_tochdr	*hdr = &header;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_tochdr.\n");


	buffer = kmem_zalloc((size_t)4, KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_READ_TOC;
	cdb[6] = 0x00;
	/*
	 * byte 7, 8 are the allocation length. In this case, it is 4 bytes.
	 */
	cdb[8] = 0x04;
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = 0x04;
	com->uscsi_timeout = 300;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	if (SD_READ_TOC_TRK_BCD(un)) {
		hdr->cdth_trk0 = BCD_TO_BYTE(buffer[2]);
		hdr->cdth_trk1 = BCD_TO_BYTE(buffer[3]);
	} else {
		hdr->cdth_trk0 = buffer[2];
		hdr->cdth_trk1 = buffer[3];
	}
	kmem_free(buffer, 4);
	kmem_free(com, sizeof (*com));
	if (ddi_copyout(hdr, data, sizeof (struct cdrom_tochdr), flag))
		return (EFAULT);
	return (rtn);
}

#ifdef CDROMREADOFFSET

#define	SONY_SESSION_OFFSET_LEN 12
#define	SONY_SESSION_OFFSET_KEY 0x40
#define	SONY_SESSION_OFFSET_VALID	0x0a

static int
sr_read_sony_session_offset(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	caddr_t buffer;
	int	rtn;
	int	session_offset;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "in sr_read_multi_session_offset.\n");

	buffer = kmem_zalloc((size_t)SONY_SESSION_OFFSET_LEN, KM_SLEEP);
	bzero(cdb, CDB_GROUP1);
	cdb[0] = SCMD_READ_TOC;
	/*
	 * byte 7, 8 are the allocation length. In this case, it is 4 bytes.
	 */
	cdb[8] = SONY_SESSION_OFFSET_LEN;
	cdb[9] = SONY_SESSION_OFFSET_KEY;
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = SONY_SESSION_OFFSET_LEN;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
	    UIO_SYSSPACE);
	session_offset = 0;
	if (buffer[1] == SONY_SESSION_OFFSET_VALID) {
		session_offset = ((uchar_t)buffer[8] << 24) +
		    ((uchar_t)buffer[9] << 16) +
		    ((uchar_t)buffer[10] << 8) +
		    ((uchar_t)buffer[11]);
		if (un->un_lbasize == CDROM_BLK_512) {
			session_offset >>= 2;
		} else if (un->un_lbasize == CDROM_BLK_1024) {
			session_offset >>= 1;
		}
	}
	kmem_free(buffer, SONY_SESSION_OFFSET_LEN);
	kmem_free(com, sizeof (*com));

	if (ddi_copyout(&session_offset, data, sizeof (int), flag))
		return (EFAULT);

	return (rtn);
}
#endif

/*
 * This routine read the toc of the disc and returns the information
 * of a particular track. The track number is specified by the ioctl
 * caller.
 */
static int
sr_read_tocentry(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd *com;
	char	cdb[CDB_GROUP1];
	struct cdrom_tocentry	toc_entry;
	struct cdrom_tocentry	*entry = &toc_entry;
	caddr_t buffer;
	int	rtn, rtn1;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_read_tocentry.\n");

	if (ddi_copyin(data, entry, sizeof (struct cdrom_tocentry), flag))
		return (EFAULT);

	if (!(entry->cdte_format & (CDROM_LBA | CDROM_MSF))) {
		return (EINVAL);
	}

	if (entry->cdte_track == 0) {
		return (EINVAL);
	}

	buffer = kmem_zalloc((size_t)12, KM_SLEEP);
	bzero(cdb, sizeof (cdb));

	cdb[0] = SCMD_READ_TOC;
	/* set the MSF bit of byte one */
	cdb[1] = (entry->cdte_format & CDROM_LBA) ? 0 : 2;

	if (SD_READ_TOC_TRK_BCD(un))
		cdb[6] = BYTE_TO_BCD(entry->cdte_track);
	else
		cdb[6] = entry->cdte_track;

	/*
	 * byte 7, 8 are the allocation length. In this case, it is 4 + 8
	 * = 12 bytes, since we only need one entry.
	 */
	cdb[8] = 0x0C;
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = buffer;
	com->uscsi_buflen = 0x0C;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	entry->cdte_adr = (buffer[5] & 0xF0) >> 4;
	entry->cdte_ctrl = (buffer[5] & 0x0F);
	if (entry->cdte_format & CDROM_LBA) {
		entry->cdte_addr.lba =
		    ((uchar_t)buffer[8] << 24) +
		    ((uchar_t)buffer[9] << 16) +
		    ((uchar_t)buffer[10] << 8) +
		    ((uchar_t)buffer[11]);
	} else if (SD_READ_TOC_ADDR_BCD(un)) {
		entry->cdte_addr.msf.minute = BCD_TO_BYTE(buffer[9]);
		entry->cdte_addr.msf.second = BCD_TO_BYTE(buffer[10]);
		entry->cdte_addr.msf.frame = BCD_TO_BYTE(buffer[11]);
		/*
		 * Get LBA for READ HEADER
		 */
		cdb[1] = 0;
		rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
			UIO_SYSSPACE);
	} else {
		entry->cdte_addr.msf.minute = buffer[9];
		entry->cdte_addr.msf.second = buffer[10];
		entry->cdte_addr.msf.frame = buffer[11];
		/*
		 * Get LBA for READ HEADER
		 */
		cdb[1] = 0;
		rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
			UIO_SYSSPACE);
	}

	if (rtn) {
		kmem_free(buffer, 12);
		kmem_free(com, sizeof (*com));
		return (rtn);
	}

	/*
	 * Now do a readheader to determine which data mode it is in.
	 * ...If the track is a data track
	 */
	if ((entry->cdte_ctrl & CDROM_DATA_TRACK) &&
	    (entry->cdte_track != CDROM_LEADOUT)) {
		bzero(cdb, sizeof (cdb));
		cdb[0] = SCMD_READ_HEADER;
		cdb[2] = buffer[8];
		cdb[3] = buffer[9];
		cdb[4] = buffer[10];
		cdb[5] = buffer[11];
		cdb[8] = 0x08;
		com->uscsi_buflen = 0x08;

		rtn1 = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
			UIO_SYSSPACE);
		if (rtn1) {
			kmem_free(buffer, 12);
			kmem_free(com, sizeof (*com));
			return (rtn1);
		}
		entry->cdte_datamode = buffer[0];

	} else {
		entry->cdte_datamode = (uchar_t)-1;
	}

	kmem_free(buffer, 12);
	kmem_free(com, sizeof (*com));

	if (ddi_copyout(entry, data, sizeof (struct cdrom_tocentry), flag))
		return (EFAULT);

	return (rtn);
}

#define	PROFILE_HEADER_LEN	8

static int
sd_get_media_info(dev_t dev, caddr_t data, int flag)
{
	struct uscsi_cmd *com;
	struct dk_minfo media_info;
	struct scsi_disk *un;
	struct scsi_capacity cap;
	char cdb[CDB_GROUP1];
	char *out_data, *rqbuf;
	int rtn, retval = 0;
	diskaddr_t media_capacity;
	struct scsi_inquiry *sinq;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sd_get_media_info.\n");

	bzero(cdb, sizeof (cdb));
	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	out_data = kmem_zalloc(PROFILE_HEADER_LEN, KM_SLEEP);
	rqbuf = kmem_zalloc(SENSE_LENGTH, KM_SLEEP);

	/* Do a TUR first */
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP0;
	com->uscsi_timeout = sd_io_time;
	com->uscsi_rqbuf = rqbuf;
	com->uscsi_rqlen = SENSE_LENGTH;
	com->uscsi_flags = USCSI_RQENABLE|USCSI_SILENT;
	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	if (rtn && (com->uscsi_status == STATUS_CHECK) &&
		(com->uscsi_rqstatus == STATUS_GOOD) &&
		(rqbuf[2] == KEY_NOT_READY) && (rqbuf[12] == 0x3a)) {

		retval = ENXIO;
		goto sd_mi_done;
	}

	/* Now get configuration data */
	bzero(com, sizeof (*com));
	bzero(rqbuf, SENSE_LENGTH);
	cdb[0] = SCMD_GET_CONFIGURATION;
	cdb[1] = 2;
	cdb[8] = PROFILE_HEADER_LEN;
	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP1;
	com->uscsi_bufaddr = out_data;
	com->uscsi_buflen = PROFILE_HEADER_LEN;
	com->uscsi_timeout = sd_io_time;
	com->uscsi_rqbuf = rqbuf;
	com->uscsi_rqlen = SENSE_LENGTH;
	com->uscsi_flags = USCSI_RQENABLE|USCSI_SILENT|USCSI_READ;

	rtn = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);

	if (rtn) {
		if ((com->uscsi_status == STATUS_CHECK) &&
				(com->uscsi_rqstatus == STATUS_GOOD)) {
			if ((rqbuf[2] != KEY_ILLEGAL_REQUEST) ||
						(rqbuf[12] != 0x20)) {
				retval = EIO;
				goto sd_mi_done;
			}
		}
		sinq = un->un_sd->sd_inq;

		if (ISCD(un)) {
			media_info.dki_media_type = DK_CDROM;
		} else if (sinq->inq_qual == 0) {
			if ((bcmp(sinq->inq_vid, "IOMEGA", 6) == 0) ||
				(bcmp(sinq->inq_vid, "iomega", 6) == 0)) {
				if ((bcmp(sinq->inq_pid, "ZIP", 3) == 0)) {
					media_info.dki_media_type = DK_ZIP;
				} else if (
					(bcmp(sinq->inq_pid, "jaz", 3) == 0)) {
					media_info.dki_media_type = DK_JAZ;
				} else {
					media_info.dki_media_type =
								DK_FIXED_DISK;
				}
			} else {
				media_info.dki_media_type = DK_FIXED_DISK;
			}
		} else {
			media_info.dki_media_type = DK_UNKNOWN;
		}
	} else {
		media_info.dki_media_type = out_data[6];
		media_info.dki_media_type <<= 8;
		media_info.dki_media_type |= out_data[7];
	}

	retval = sd_read_capacity(un, &cap);
	if (retval)
		goto sd_mi_done;
	media_info.dki_lbsize = cap.lbasize;
	media_capacity = cap.capacity;
	/*
	 * sd_read_capacity() reports capacity in DEV_BSIZE chunks.
	 * so we need to convert it into cap.lbasize chunks.
	 */
	media_capacity *= DEV_BSIZE;
	media_capacity /= cap.lbasize;
	media_info.dki_capacity = media_capacity;

	if (ddi_copyout(&media_info, data, sizeof (struct dk_minfo), flag)) {
		retval = EFAULT;
		/* Put goto. Anybody might add some code below in future */
		goto sd_mi_done;
	}
sd_mi_done:;
	kmem_free(com, sizeof (*com));
	kmem_free(out_data, PROFILE_HEADER_LEN);
	kmem_free(rqbuf, SENSE_LENGTH);
	return (retval);
}

int
sd_mode_sense(dev_t dev, int page, char *page_data, int page_size,
    int use_group2)
{

	int r;
	char	cdb[CDB_GROUP2];
	register struct uscsi_cmd *com;
	uchar_t	cdblen;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	bzero(cdb, sizeof (cdb));
	if (use_group2) {
		cdblen = CDB_GROUP2;
		cdb[0] = SCMD_MODE_SENSE2;
		cdb[7] = (char)(page_size >> 8);
		cdb[8] = (char)page_size;
	} else {
		cdblen = CDB_GROUP0;
		cdb[0] = SCMD_MODE_SENSE;
		cdb[4] = (char)page_size;
	}

	cdb[2] = (char)page;

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = cdblen;
	com->uscsi_bufaddr = page_data;
	com->uscsi_buflen = page_size;
	com->uscsi_flags = USCSI_DIAGNOSE | USCSI_SILENT |
			    USCSI_READ;

	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (r);
}

/*
 * This routine does a mode select for the cdrom
 */
static int
sd_mode_select(dev_t dev, char *page_data, int page_size, int save_page,
    int use_group2)
{

	int r;
	char	cdb[CDB_GROUP1];
	register struct uscsi_cmd *com;
	uchar_t	cdblen;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	/*
	 * then, do a mode select to set what ever info
	 */
	bzero(cdb, sizeof (cdb));
	if (use_group2) {
		cdblen = CDB_GROUP2;
		cdb[0] = SCMD_MODE_SELECT2;
		cdb[7] = (char)(page_size >> 8);
		cdb[8] = (char)page_size;
	} else {
		cdblen = CDB_GROUP0;
		cdb[0] = SCMD_MODE_SELECT;
		cdb[4] = (char)page_size;
	}

	cdb[1] = 0x10;		/* set PF bit for many third party drives */
	if (save_page == SD_SAVE_PAGE) {
		cdb[1] |= 0x01;	/* set the savepage(SP) bit in the cdb */
	}

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = cdblen;
	com->uscsi_bufaddr = page_data;
	com->uscsi_buflen = page_size;
	com->uscsi_flags = USCSI_DIAGNOSE | USCSI_SILENT | USCSI_WRITE;

	r = sdioctl_cmd(dev, com, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);

	kmem_free(com, sizeof (*com));
	return (r);
}

#define	BUFLEN_CDROM_MODE_ERR_RECOV 	20

static int
sr_change_blkmode(dev_t dev, int cmd, intptr_t data, int flag)
{
	int		current_bsize;
	int		rval = EINVAL;
	register char	*sense = NULL;
	register char	*select = NULL;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	sense = kmem_zalloc(BUFLEN_CDROM_MODE_ERR_RECOV, KM_SLEEP);
	if ((rval = sd_mode_sense(dev, 0x1, sense,
	    BUFLEN_CDROM_MODE_ERR_RECOV, FALSE)) != 0) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_change_blkmode: Mode Sense Failed\n");
		kmem_free(sense, BUFLEN_CDROM_MODE_ERR_RECOV);
		return (rval);
	}
	current_bsize = (uchar_t)sense[9] << 16 | (uchar_t)sense[10] << 8 |
				(uchar_t)sense[11];

	switch (cmd) {
	case CDROMGBLKMODE:
		if (ddi_copyout(&current_bsize, (caddr_t)data,
			sizeof (int), flag))
			rval = EFAULT;
		break;

	case CDROMSBLKMODE:
		switch (data) {
		case CDROM_BLK_512:
		case CDROM_BLK_1024:
		case CDROM_BLK_2048:
		case CDROM_BLK_2056:
		case CDROM_BLK_2336:
		case CDROM_BLK_2340:
		case CDROM_BLK_2352:
		case CDROM_BLK_2368:
		case CDROM_BLK_2448:
		case CDROM_BLK_2646:
		case CDROM_BLK_2647:
			break;

		default:
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_change_blkmode: "
				"Block Size '%ld' Not Supported\n", data);
			kmem_free(sense, BUFLEN_CDROM_MODE_ERR_RECOV);
			return (EINVAL);
		}

		if (current_bsize == data) {
			break;
		}

		select = kmem_zalloc(BUFLEN_CDROM_MODE_ERR_RECOV, KM_SLEEP);
		select[3] = 0x08;
		select[9] = (char)(((data) & 0x00ff0000) >> 16);
		select[10] = (char)(((data) & 0x0000ff00) >> 8);
		select[11] = (char)((data) & 0x000000ff);
		select[12] = 0x1;
		select[13] = 0x06;
		select[14] = sense[14];
		select[15] = sense[15];

		if ((rval = sd_mode_select(dev, select,
		    BUFLEN_CDROM_MODE_ERR_RECOV, SD_DONTSAVE_PAGE,
		    FALSE)) != 0) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_change_blkmode: Mode Select Failed\n");
			select[9] = ((current_bsize) & 0x00ff0000) >> 16;
			select[10] = ((current_bsize) & 0x0000ff00) >> 8;
			select[11] = (current_bsize) & 0x000000ff;
			(void) sd_mode_select(dev, select,
			    BUFLEN_CDROM_MODE_ERR_RECOV, SD_DONTSAVE_PAGE,
			    FALSE);
			break;
		}
		un->un_lbasize = (int)data;

		break;

	default:

		/*
		 * should not reach here,
		 * but check anyway
		 */
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_change_blkmode: Command '%x' Not Supported\n", cmd);
		break;
	}

	if (select) {
		kmem_free(select, BUFLEN_CDROM_MODE_ERR_RECOV);
	}
	if (sense) {
		kmem_free(sense, BUFLEN_CDROM_MODE_ERR_RECOV);
	}
	return (rval);
}

/*
 * The following 16 is:  (sizeof (mode_header) + sizeof (block_descriptor) +
 *  sizeof (mode_speed)).
 */
#define	BUFLEN_CDROM_MODE_SPEED		16

static int
sr_change_speed(dev_t dev, int cmd, intptr_t data, int flag)
{
	int		current_speed;
	int		rval = EINVAL;
	register char	*sense = NULL;
	register char	*select = NULL;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	sense = kmem_zalloc(BUFLEN_CDROM_MODE_SPEED, KM_SLEEP);
	if ((rval = sd_mode_sense(dev, CDROM_MODE_SPEED, sense,
				BUFLEN_CDROM_MODE_SPEED, FALSE)) != 0) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_change_speed: Mode Sense Failed\n");
		kmem_free(sense, BUFLEN_CDROM_MODE_SPEED);
		return (rval);
	}
	current_speed = (uchar_t)sense[14];

	switch (cmd) {
	case CDROMGDRVSPEED:
		if (current_speed == 0x2) {
			current_speed = CDROM_TWELVE_SPEED;
		}
		if (ddi_copyout(&current_speed, (void *)data,
			sizeof (int), flag))
			rval = EFAULT;
		break;

	case CDROMSDRVSPEED:
		switch ((uchar_t)data) {
		case CDROM_TWELVE_SPEED:
			data = 0x2;
			/*FALLTHROUGH*/
		case CDROM_NORMAL_SPEED:
		case CDROM_DOUBLE_SPEED:
		case CDROM_QUAD_SPEED:
		case CDROM_MAXIMUM_SPEED:
			break;

		default:
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_change_speed: "
				"Drive Speed '%d' Not Supported\n",
				(uchar_t)data);
			kmem_free(sense, BUFLEN_CDROM_MODE_SPEED);
			return (EINVAL);
		}

		if (current_speed == data) {
			break;
		}

		select = kmem_zalloc(BUFLEN_CDROM_MODE_SPEED, KM_SLEEP);
		select[3] = 0x08;
		select[9] = sense[9];
		select[10] = sense[10];
		select[11] = sense[11];
		select[12] = CDROM_MODE_SPEED;
		select[13] = 0x02;
		select[14] = (uchar_t)data;
		select[15] = sense[15];

		if ((rval = sd_mode_select(dev, select,
		    BUFLEN_CDROM_MODE_SPEED, SD_DONTSAVE_PAGE, FALSE)) != 0) {
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_drive_speed: Mode Select Failed\n");
			select[14] = (uchar_t)current_speed;
			(void) sd_mode_select(dev, select,
			    BUFLEN_CDROM_MODE_SPEED, SD_DONTSAVE_PAGE, FALSE);
			break;
		}

		break;

	default:

		/*
		 * should not reach here,
		 * but check anyway
		 */
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_change_speed: Command '%x' Not Supported\n", cmd);
		break;
	}

	if (select) {
		kmem_free(select, BUFLEN_CDROM_MODE_SPEED);
	}
	if (sense) {
		kmem_free(sense, BUFLEN_CDROM_MODE_SPEED);
	}
	return (rval);
}


/*
 * sr_set_speed()
 *
 *	Sets the transfer rate of an ATAPI CD drive.
 *
 * NOTE:
 *
 * Currently, ATAPI CD drives that are SFF-8020 compliant support this
 * CDB. Some SCSI CD drives that are SCSI-3 MMC compliant may also
 * support this drive. At this time it appears that SCSI-3 MMC-2
 * may specify that a host will no longer be able to determine or
 * change a CD drive's transfer rate. If that happens we need to
 * decide whether to deprecate this ioctl.
 *
 */

/*
 * CD-ROM Capabilities and Mechanical Status page
 */
#define	CDROM_MODE_CAP	0x2a

/*
 * The following is:  (sizeof (mode_header) + sizeof (block_descriptor) +
 *			sizeof (cdrom_capabilities)).
 */
#define	BUFLEN_CDROM_MODE_CAP	\
	(MODE_HEADER_LENGTH_GRP2 + MODE_BLK_DESC_LENGTH + 20)

/*
 * The cdio(7) man page says 1x is 150 KB/sec, but the MMC and ATAPI
 * specs say 1x is 176 KB/sec. This difference is caused by whether
 * you assume a sector contains 2048 bytes or 2352 bytes.
 */
#define	SD_SPEED_1X	150

static int
sr_set_speed(dev_t dev, int cmd, intptr_t data, int flag)
{
	struct uscsi_cmd *com;
	char	cdb[CDB_GROUP5];
	caddr_t	buffer;
	int	current_speed = 0;
	int	max_speed = 0;
	int	rval;
	int	rtncode;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "in sr_set_speed.\n");

	buffer = kmem_zalloc(BUFLEN_CDROM_MODE_CAP, KM_SLEEP);
	if ((rval = sd_mode_sense(dev, CDROM_MODE_CAP, buffer,
	    BUFLEN_CDROM_MODE_CAP, TRUE)) != 0) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_set_speed: Mode Sense Failed\n");
	} else {
		register struct mode_header_grp2 *mhp;
		register uchar_t	*datap;
		register int	bd_len;

		mhp = (struct mode_header_grp2 *)buffer;
		bd_len = (mhp->bdesc_length_hi << 8) | mhp->bdesc_length_lo;

		/* only handle 0 or 1 block descriptors */
		ASSERT(bd_len <= MODE_BLK_DESC_LENGTH);

		datap = (uchar_t *)(buffer + MODE_HEADER_LENGTH_GRP2 + bd_len);
		current_speed = (datap[14] << 8) | datap[15];
		max_speed = (datap[8] << 8) | datap[9];
	}
	kmem_free(buffer, BUFLEN_CDROM_MODE_CAP);

	if (cmd == CDROMGDRVSPEED) {

		if (current_speed == max_speed)
			rtncode = CDROM_MAXIMUM_SPEED;
		else if (current_speed >= 12 * SD_SPEED_1X)
			rtncode = CDROM_TWELVE_SPEED;
		else if (current_speed >= 4 * SD_SPEED_1X)
			rtncode = CDROM_QUAD_SPEED;
		else if (current_speed >= 2 * SD_SPEED_1X)
			rtncode = CDROM_DOUBLE_SPEED;
		else
			rtncode = CDROM_NORMAL_SPEED;

		if (ddi_copyout(&rtncode, (caddr_t)data,
				sizeof (int), flag))
			return (EFAULT);
		return (0);
	}

	if (cmd != CDROMSDRVSPEED) {
		/*
		 * should not reach here,
		 * but check anyway
		 */
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_change_speed: Command '%x' Not Supported\n", cmd);
		return (EINVAL);
	}

	/*
	 * Convert the speed code to KB/sec
	 */
	switch ((uchar_t)data) {
	case CDROM_NORMAL_SPEED:
		current_speed = SD_SPEED_1X;
		break;

	case CDROM_DOUBLE_SPEED:
		current_speed = 2 * SD_SPEED_1X;
		break;

	case CDROM_QUAD_SPEED:
		current_speed = 4 * SD_SPEED_1X;
		break;

	case CDROM_TWELVE_SPEED:
		current_speed = 12 * SD_SPEED_1X;
		break;

	case CDROM_MAXIMUM_SPEED:
		current_speed = 0xffff;
		break;
	}

	/*
	 * Validity check the request against the drive's max speed.
	 * Skip this check if the user request maximum warp, or
	 * if the Mode Sense failed or if the drive
	 * reports its max as 0.
	 */
	if (current_speed != 0xffff && max_speed != 0) {
		if (current_speed > max_speed)
			return (EINVAL);
	}

	bzero(cdb, sizeof (cdb));
	cdb[0] = (char)SCMD_SET_CDROM_SPEED;
	cdb[2] = (uchar_t)(current_speed >> 8);
	cdb[3] = (uchar_t)current_speed;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);
	com->uscsi_cdb = (caddr_t)cdb;
	com->uscsi_cdblen = sizeof (cdb);
	com->uscsi_bufaddr = NULL;
	com->uscsi_buflen = 0;
	com->uscsi_flags = USCSI_DIAGNOSE|USCSI_SILENT;

	rval = sdioctl_cmd(dev, com, UIO_SYSSPACE, 0, UIO_SYSSPACE);
	kmem_free(com, sizeof (*com));
	return (rval);
}

static int
sr_read_cdda(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd	*com;
	register struct cdrom_cdda	*cdda;
	int				rval;
	int				buflen;
	char				cdb[CDB_GROUP5];
#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_cdda32	cdrom_cdda32;
	struct cdrom_cdda32	*cdda32 = &cdrom_cdda32;
#endif /* _MULTI_DATAMODEL */

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	cdda = (struct cdrom_cdda *)
		kmem_zalloc(sizeof (struct cdrom_cdda), KM_SLEEP);

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdda32,
		    sizeof (*cdda32), flag)) {
			goto cdda_fault;
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_cdda32tocdrom_cdda(cdda32, cdda);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, cdda,
		    sizeof (struct cdrom_cdda), flag)) {
cdda_fault:
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_read_cdda: ddi_copyin Failed\n");
			kmem_free(cdda, sizeof (struct cdrom_cdda));
			return (EFAULT);
		}
		break;
	}

#else /* ! _MULTI_DATAMODEL */
	if (ddi_copyin(data, cdda, sizeof (struct cdrom_cdda), flag)) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_read_cdda: ddi_copyin Failed\n");
		kmem_free(cdda, sizeof (struct cdrom_cdda));
		return (EFAULT);
	}
#endif /* _MULTI_DATAMODEL */

	switch (cdda->cdda_subcode) {
	case CDROM_DA_NO_SUBCODE:
		buflen = CDROM_BLK_2352 * cdda->cdda_length;
		break;

	case CDROM_DA_SUBQ:
		buflen = CDROM_BLK_2368 * cdda->cdda_length;
		break;

	case CDROM_DA_ALL_SUBCODE:
		buflen = CDROM_BLK_2448 * cdda->cdda_length;
		break;

	case CDROM_DA_SUBCODE_ONLY:
		buflen = CDROM_BLK_SUBCODE * cdda->cdda_length;
		break;

	default:
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_read_cdda: Sucode '0x%x' Not Supported\n",
			cdda->cdda_subcode);
		kmem_free(cdda, sizeof (struct cdrom_cdda));
		return (EINVAL);
	}

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	bzero(cdb, CDB_GROUP5);
	if (SD_IS_ATAPI(un)) {
		cdb[0] = (char)SCMD_READ_CD;
		cdb[1] = 0x04;
		cdb[2] = (((cdda->cdda_addr) & 0xff000000) >> 24);
		cdb[3] = (((cdda->cdda_addr) & 0x00ff0000) >> 16);
		cdb[4] = (((cdda->cdda_addr) & 0x0000ff00) >> 8);
		cdb[5] = ((cdda->cdda_addr) & 0x000000ff);
		cdb[6] = (((cdda->cdda_length) & 0x00ff0000) >> 16);
		cdb[7] = (((cdda->cdda_length) & 0x0000ff00) >> 8);
		cdb[8] = ((cdda->cdda_length) & 0x000000ff);
		cdb[9] = 0x10;
		cdb[10] = cdda->cdda_subcode;
	} else {
		cdb[0] = (char)SCMD_READ_CDDA;
		cdb[2] = (((cdda->cdda_addr) & 0xff000000) >> 24);
		cdb[3] = (((cdda->cdda_addr) & 0x00ff0000) >> 16);
		cdb[4] = (((cdda->cdda_addr) & 0x0000ff00) >> 8);
		cdb[5] = ((cdda->cdda_addr) & 0x000000ff);
		cdb[6] = (((cdda->cdda_length) & 0xff000000) >> 24);
		cdb[7] = (((cdda->cdda_length) & 0x00ff0000) >> 16);
		cdb[8] = (((cdda->cdda_length) & 0x0000ff00) >> 8);
		cdb[9] = ((cdda->cdda_length) & 0x000000ff);
		cdb[10] = cdda->cdda_subcode;
	}

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP5;
	com->uscsi_bufaddr = (caddr_t)cdda->cdda_data;
	com->uscsi_buflen = buflen;
	com->uscsi_flags = USCSI_SILENT|USCSI_READ;

	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_USERSPACE, UIO_SYSSPACE);

	if (com->uscsi_resid)
		rval = EIO;

	kmem_free(cdda, sizeof (struct cdrom_cdda));
	kmem_free(com, sizeof (*com));
	return (rval);
}

static int
sr_read_cdxa(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd	*com;
	register struct cdrom_cdxa	*cdxa;
	int				rval;
	int				buflen;
	char				cdb[CDB_GROUP5];

#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_cdxa32		cdrom_cdxa32;
	struct cdrom_cdxa32		*cdxa32 = &cdrom_cdxa32;
#endif /* _MULTI_DATAMODEL */

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	cdxa = kmem_zalloc(sizeof (struct cdrom_cdxa), KM_SLEEP);

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdxa32,
		    sizeof (*cdxa32), flag)) {
			goto cdxa_fault;
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_cdxa32tocdrom_cdxa(cdxa32, cdxa);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, cdxa,
		    sizeof (struct cdrom_cdxa), flag)) {
cdxa_fault:
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				"sr_read_cdxa: ddi_copyin Failed\n");
			kmem_free(cdxa, sizeof (struct cdrom_cdxa));
			return (EFAULT);
		}
		break;
	}

#else /* ! _MULTI_DATAMODEL */
	if (ddi_copyin(data, cdxa, sizeof (struct cdrom_cdxa), flag)) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_read_cdxa: ddi_copyin Failed\n");
		kmem_free(cdxa, sizeof (struct cdrom_cdxa));
		return (EFAULT);
	}
#endif /* _MULTI_DATAMODEL */

	switch (cdxa->cdxa_format) {
	case CDROM_XA_DATA:
		buflen = CDROM_BLK_2048 * cdxa->cdxa_length;
		break;

	case CDROM_XA_SECTOR_DATA:
		buflen = CDROM_BLK_2352 * cdxa->cdxa_length;
		break;

	case CDROM_XA_DATA_W_ERROR:
		buflen = CDROM_BLK_2646 * cdxa->cdxa_length;
		break;

	default:
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_read_cdxa: Format '0x%x' Not Supported\n",
			cdxa->cdxa_format);
		kmem_free(cdxa, sizeof (struct cdrom_cdxa));
		return (EINVAL);
	}

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	bzero(cdb, CDB_GROUP5);
	cdb[0] = (char)SCMD_READ_CDXA;
	cdb[2] = (((cdxa->cdxa_addr) & 0xff000000) >> 24);
	cdb[3] = (((cdxa->cdxa_addr) & 0x00ff0000) >> 16);
	cdb[4] = (((cdxa->cdxa_addr) & 0x0000ff00) >> 8);
	cdb[5] = ((cdxa->cdxa_addr) & 0x000000ff);
	cdb[6] = (((cdxa->cdxa_length) & 0xff000000) >> 24);
	cdb[7] = (((cdxa->cdxa_length) & 0x00ff0000) >> 16);
	cdb[8] = (((cdxa->cdxa_length) & 0x0000ff00) >> 8);
	cdb[9] = ((cdxa->cdxa_length) & 0x000000ff);
	cdb[10] = cdxa->cdxa_format;

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP5;
	com->uscsi_bufaddr = (caddr_t)cdxa->cdxa_data;
	com->uscsi_buflen = buflen;
	com->uscsi_flags = USCSI_SILENT|USCSI_READ;

	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_USERSPACE, UIO_SYSSPACE);

	kmem_free(cdxa, sizeof (struct cdrom_cdxa));
	kmem_free(com, sizeof (*com));
	return (rval);
}

static int
sr_read_all_subcodes(dev_t dev, caddr_t data, int flag)
{
	register struct uscsi_cmd	*com;
	register struct cdrom_subcode	*subcode;
	int				rval;
	int				buflen;
	char				cdb[CDB_GROUP5];

#ifdef _MULTI_DATAMODEL
	/*
	 * To support ILP32 applications in an LP64 world
	 */
	struct cdrom_subcode32		cdrom_subcode32;
	struct cdrom_subcode32		*cdsc32 = &cdrom_subcode32;
#endif /* _MULTI_DATAMODEL */

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */

	subcode = kmem_zalloc(sizeof (struct cdrom_subcode), KM_SLEEP);

#ifdef _MULTI_DATAMODEL
	switch (ddi_model_convert_from(flag & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		if (ddi_copyin(data, cdsc32,
		    sizeof (*cdsc32), flag)) {
			goto subcode_fault;
		}
		/*
		 * Convert the ILP32 uscsi data from the
		 * application to LP64 for internal use.
		 */
		cdrom_subcode32tocdrom_subcode(cdsc32, subcode);
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin(data, subcode,
		    sizeof (struct cdrom_subcode), flag)) {
subcode_fault:
			scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "sr_read_all_subcodes: ddi_copyin Failed\n");
			kmem_free(subcode, sizeof (struct cdrom_subcode));
			return (EFAULT);
		}
		break;
	}

#else /* ! _MULTI_DATAMODEL */

	if (ddi_copyin(data, subcode, sizeof (struct cdrom_subcode), flag)) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			"sr_read_all_subcodes: ddi_copyin Failed\n");
		kmem_free(subcode, sizeof (struct cdrom_subcode));
		return (EFAULT);
	}
#endif /* _MULTI_DATAMODEL */

	buflen = CDROM_BLK_SUBCODE * subcode->cdsc_length;

	com = kmem_zalloc(sizeof (*com), KM_SLEEP);

	bzero(cdb, CDB_GROUP5);
	cdb[0] = (char)SCMD_READ_ALL_SUBCODES;
	cdb[6] = (((subcode->cdsc_length) & 0xff000000) >> 24);
	cdb[7] = (((subcode->cdsc_length) & 0x00ff0000) >> 16);
	cdb[8] = (((subcode->cdsc_length) & 0x0000ff00) >> 8);
	cdb[9] = ((subcode->cdsc_length) & 0x000000ff);

	com->uscsi_cdb = cdb;
	com->uscsi_cdblen = CDB_GROUP5;
	com->uscsi_bufaddr = (caddr_t)subcode->cdsc_addr;
	com->uscsi_buflen = buflen;
	com->uscsi_flags = USCSI_SILENT|USCSI_READ;

	rval = sdioctl_cmd(dev, com,
		UIO_SYSSPACE, UIO_USERSPACE, UIO_SYSSPACE);

	kmem_free(subcode, sizeof (struct cdrom_subcode));
	kmem_free(com, sizeof (*com));
	return (rval);
}

/*
 * This routine sets the drive to read 512 bytes per sector
 */
static int
sr_sector_mode(dev_t dev, int mode)
{
	int r;
	register char *sense, *select;
	struct scsi_disk *un;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	ASSERT(mutex_owned(SD_MUTEX));

	mutex_exit(SD_MUTEX);

	sense = kmem_zalloc(20, KM_SLEEP);
	if ((r = sd_mode_sense(dev, 0x81, sense, 20, FALSE)) != 0) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sr_sector_mode: Mode Sense failed\n");
		kmem_free(sense, 20);
		mutex_enter(SD_MUTEX);
		return (r);
	}
	select = kmem_zalloc(20, KM_SLEEP);

	select[3] = 0x08;

	if (mode == 2) {
		select[10] = 0x09;
		select[11] = 0x20;
		select[12] = 0x01;
		select[13] = 0x06;
		select[14] = sense[14] | 0x01;
		select[15] = sense[15];
	} else {
		select[10] = ((DEV_BSIZE >> 8) & 0xff);
		select[11] = (DEV_BSIZE & 0xff);
		select[12] = 0x01;
		select[13] = 0x06;
		select[14] = sense[14];
		select[15] = sense[15];
	}

	if ((r = sd_mode_select(dev, select, 20, SD_DONTSAVE_PAGE,
				FALSE)) != 0) {
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sr_sector_mode: Mode Select failed\n");
		kmem_free(select, 20);
		kmem_free(sense, 20);
		mutex_enter(SD_MUTEX);
		return (r);
	}
	kmem_free(sense, 20);
	kmem_free(select, 20);
	mutex_enter(SD_MUTEX);
	return (0);
}

#if defined(_SUNOS_VTOC_8)
static void
sd_build_user_vtoc(struct scsi_disk *un, struct vtoc *vtoc)
{

	int i, nblks;
	struct dk_map2	*lpart;
	struct dk_map	*lmap;
	struct partition *vpart;

	ASSERT(mutex_owned(SD_MUTEX));

	/*
	 * Return vtoc structure fields in the provided VTOC area, addressed
	 * by *vtoc.
	 *
	 */

	bzero(vtoc, sizeof (struct vtoc));

	vtoc->v_bootinfo[0] = un->un_vtoc.v_bootinfo[0];
	vtoc->v_bootinfo[1] = un->un_vtoc.v_bootinfo[1];
	vtoc->v_bootinfo[2] = un->un_vtoc.v_bootinfo[2];

	vtoc->v_sanity		= VTOC_SANE;
	vtoc->v_version		= un->un_vtoc.v_version;

	bcopy(un->un_vtoc.v_volume, vtoc->v_volume, LEN_DKL_VVOL);

	vtoc->v_sectorsz = DEV_BSIZE;
	vtoc->v_nparts = un->un_vtoc.v_nparts;

	bcopy((void *)un->un_vtoc.v_reserved, (void *)vtoc->v_reserved,
	    sizeof (un->un_vtoc.v_reserved));
	/*
	 * Convert partitioning information.
	 *
	 * Note the conversion from starting cylinder number
	 * to starting sector number.
	 */
	lmap = un->un_map;
	lpart = un->un_vtoc.v_part;
	vpart = vtoc->v_part;

	nblks = un->un_g.dkg_nsect * un->un_g.dkg_nhead;

	for (i = 0; i < V_NUMPAR; i++) {
		vpart->p_tag	= lpart->p_tag;
		vpart->p_flag	= lpart->p_flag;
		vpart->p_start	= lmap->dkl_cylno * nblks;
		vpart->p_size	= lmap->dkl_nblk;

		lmap++;
		lpart++;
		vpart++;
	}

	bcopy(un->un_vtoc.v_timestamp, vtoc->timestamp,
		sizeof (vtoc->timestamp));

	bcopy(un->un_asciilabel, vtoc->v_asciilabel, LEN_DKL_ASCII);

}
#endif	/* defined(_SUNOS_VTOC_8) */

static int
sd_build_label_vtoc(struct scsi_disk *un, struct vtoc *vtoc)
{

	struct dk_map		*lmap;
	struct partition	*vpart;
	int			nblks;
#if defined(_SUNOS_VTOC_8)
	int			ncyl;
#endif	/* defined(_SUNOS_VTOC_8) */
	int			i;

#if defined(_SUNOS_VTOC_8)
	struct dk_map2		*lpart;
#endif	/* defined(_SUNOS_VTOC_8) */

	ASSERT(mutex_owned(SD_MUTEX));

	/*
	 * Sanity-check the vtoc
	 */
	if (vtoc->v_sanity != VTOC_SANE || vtoc->v_sectorsz != DEV_BSIZE ||
			vtoc->v_nparts != V_NUMPAR) {
		return (EINVAL);
	}

	if ((nblks = un->un_g.dkg_nsect * un->un_g.dkg_nhead) == 0) {
		return (EINVAL);
	}

#if defined(_SUNOS_VTOC_8)
	vpart = vtoc->v_part;
	for (i = 0; i < V_NUMPAR; i++) {
		if ((vpart->p_start % nblks) != 0)
			return (EINVAL);
		ncyl = vpart->p_start / nblks;
		ncyl += vpart->p_size / nblks;
		if ((vpart->p_size % nblks) != 0)
			ncyl++;
		if (ncyl > (int)un->un_g.dkg_ncyl)
			return (EINVAL);
		vpart++;
	}
#endif	/* defined(_SUNOS_VTOC_8) */

	/*
	 * Put appropriate vtoc structure fields into the disk label
	 *
	 */

#if defined(_SUNOS_VTOC_16)
	bcopy(vtoc, &(un->un_vtoc), sizeof (*vtoc));

	/*
	 * in the 16-slice vtoc, starting sectors are expressed in
	 * numbers *relative* to the start of the Solaris fdisk partition.
	 */
	lmap = un->un_map;
	vpart = vtoc->v_part;

	for (i = 0; i < (int)vtoc->v_nparts; i++, lmap++, vpart++) {
		lmap->dkl_cylno = vpart->p_start / nblks;
		lmap->dkl_nblk = vpart->p_size;
	}

#elif defined(_SUNOS_VTOC_8)
	/*
	 * Put appropriate vtoc structure fields into the disk label
	 *
	 */
	un->un_vtoc.v_bootinfo[0] = (uint32_t)vtoc->v_bootinfo[0];
	un->un_vtoc.v_bootinfo[1] = (uint32_t)vtoc->v_bootinfo[1];
	un->un_vtoc.v_bootinfo[2] = (uint32_t)vtoc->v_bootinfo[2];

	un->un_vtoc.v_sanity = (uint32_t)vtoc->v_sanity;
	un->un_vtoc.v_version = (uint32_t)vtoc->v_version;

	bcopy(vtoc->v_volume, un->un_vtoc.v_volume, LEN_DKL_VVOL);

	un->un_vtoc.v_nparts = vtoc->v_nparts;

	bcopy((void *)vtoc->v_reserved, (void *)un->un_vtoc.v_reserved,
	    sizeof (vtoc->v_reserved));

	/*
	 * Note the conversion from starting sector number
	 * to starting cylinder number.
	 * Return error if division results in a remainder.
	 */
	lmap = un->un_map;
	lpart = un->un_vtoc.v_part;
	vpart = vtoc->v_part;

	for (i = 0; i < (int)vtoc->v_nparts; i++) {
		lpart->p_tag  = vpart->p_tag;
		lpart->p_flag = vpart->p_flag;
		lmap->dkl_cylno = vpart->p_start / nblks;
		lmap->dkl_nblk = vpart->p_size;

		lmap++;
		lpart++;
		vpart++;
	}

	bcopy(vtoc->timestamp, un->un_vtoc.v_timestamp,
		sizeof (vtoc->timestamp));

	bcopy(vtoc->v_asciilabel, un->un_asciilabel, LEN_DKL_ASCII);
#else
#error "No VTOC format defined."
#endif
	return (0);
}

/*
 * Disk geometry macros
 *
 *	spc:		sectors per cylinder
 *	chs2bn:		cyl/head/sector to block number
 */
#define	spc(l)		(((l)->dkl_nhead*(l)->dkl_nsect)-(l)->dkl_apc)

#define	chs2bn(l, c, h, s)	\
			((daddr_t)((c)*spc(l)+(h)*(l)->dkl_nsect+(s)))


static int
sd_write_label(dev_t dev)
{
	int i, status;
	int sec, blk, head, cyl;
	short sum, *sp;
	register struct scsi_disk *un;
	struct dk_label *dkl;
	struct uscsi_cmd	ucmd;
	union scsi_cdb cdb;
	uint_t			label_addr;

	if ((un = ddi_get_soft_state(sd_state, SDUNIT(dev))) == NULL ||
		(un->un_state == SD_STATE_OFFLINE))
		return (ENXIO);

	ASSERT(mutex_owned(SD_MUTEX));

	if ((dkl = kmem_zalloc(sizeof (struct dk_label),
		KM_NOSLEEP)) == NULL) {
		return (ENOMEM);
	}

	bcopy(&un->un_vtoc, &(dkl->dkl_vtoc), sizeof (struct dk_vtoc));

	dkl->dkl_rpm	= un->un_g.dkg_rpm;
	dkl->dkl_pcyl	= un->un_g.dkg_pcyl;
	dkl->dkl_apc	= un->un_g.dkg_apc;
	dkl->dkl_intrlv = un->un_g.dkg_intrlv;
	dkl->dkl_ncyl	= un->un_g.dkg_ncyl;
	dkl->dkl_acyl	= un->un_g.dkg_acyl;
	dkl->dkl_nhead	= un->un_g.dkg_nhead;
	dkl->dkl_nsect	= un->un_g.dkg_nsect;

#if defined(_SUNOS_VTOC_8)
	dkl->dkl_obs1	= un->un_g.dkg_obs1;
	dkl->dkl_obs2	= un->un_g.dkg_obs2;
	dkl->dkl_obs3	= un->un_g.dkg_obs3;

	for (i = 0; i < NDKMAP; i++) {
		dkl->dkl_map[i].dkl_cylno =
			un->un_map[i].dkl_cylno;
		dkl->dkl_map[i].dkl_nblk =
			un->un_map[i].dkl_nblk;
	}
	bcopy(un->un_asciilabel, dkl->dkl_asciilabel, LEN_DKL_ASCII);

#elif defined(_SUNOS_VTOC_16)
	dkl->dkl_skew	= un->un_dkg_skew;

#else
#error "No VTOC format defined."
#endif

	dkl->dkl_magic			= DKL_MAGIC;
	dkl->dkl_write_reinstruct	= un->un_g.dkg_write_reinstruct;
	dkl->dkl_read_reinstruct	= un->un_g.dkg_read_reinstruct;

	/*
	 * Construct checksum for the new disk label
	 */
	sum = 0;
	sp = (short *)dkl;
	i = sizeof (struct dk_label)/sizeof (short);
	while (i--) {
		sum ^= *sp++;
	}
	dkl->dkl_cksum = sum;

	/*
	 * Do the USCSI Write here
	 */

	/*
	 * Write the primary label at block 0 of the solaris partition.
	 */
	label_addr = un->un_solaris_offset + DK_LABEL_LOC;

	/*
	 * Write the Primary label using the USCSI interface
	 * Write at Block 0.
	 * Build and execute the uscsi ioctl.  We build a group0
	 * or group1 command as necessary, since some targets
	 * may not support group1 commands.
	 */
	bzero(&ucmd, sizeof (ucmd));
	bzero(&cdb, sizeof (union scsi_cdb));
	ucmd.uscsi_flags = USCSI_WRITE;

	if ((label_addr >= (2 << 20)) || SD_GRP1_2_CDBS(un)) {
		cdb.scc_cmd = SCMD_WRITE_G1;
		FORMG1ADDR(&cdb, label_addr);
		FORMG1COUNT(&cdb, (uchar_t)1);
		ucmd.uscsi_cdblen = CDB_GROUP1;
	} else {
		cdb.scc_cmd = SCMD_WRITE;
		FORMG0ADDR(&cdb, label_addr);
		FORMG0COUNT(&cdb, (uchar_t)1);
		ucmd.uscsi_cdblen = CDB_GROUP0;
	}

	ucmd.uscsi_cdb = (caddr_t)&cdb;


	ucmd.uscsi_bufaddr = (caddr_t)dkl;
	ucmd.uscsi_buflen = DEV_BSIZE;
	ucmd.uscsi_flags |= USCSI_SILENT;
	mutex_exit(SD_MUTEX);
	status = sdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE,
		UIO_SYSSPACE);
	mutex_enter(SD_MUTEX);
	if (status != 0) {
		kmem_free(dkl, sizeof (struct dk_label));
		return (status);
	}



	/*
	 * Calculate where the backup labels go.  They are always on
	 * the last alternate cylinder, but some older drives put them
	 * on head 2 instead of the last head.	They are always on the
	 * first 5 odd sectors of the appropriate track.
	 *
	 * We have no choice at this point, but to believe that the
	 * disk label is valid.	 Use the geometry of the disk
	 * as described in the label.
	 */
	cyl = dkl->dkl_ncyl + dkl->dkl_acyl - 1;
	head = dkl->dkl_nhead-1;

	/*
	 * Write and verify the backup labels.
	 */
	for (sec = 1; sec < 5 * 2 + 1; sec += 2) {
		bzero(&cdb, sizeof (union scsi_cdb));
		blk = chs2bn(dkl, cyl, head, sec);
		blk += un->un_solaris_offset;

		if ((blk >= (2 << 20)) || SD_GRP1_2_CDBS(un)) {
			cdb.scc_cmd = SCMD_WRITE_G1;
			FORMG1ADDR(&cdb, blk);
			FORMG1COUNT(&cdb, 1);
			ucmd.uscsi_cdblen = CDB_GROUP1;
		} else {
			cdb.scc_cmd = SCMD_WRITE;
			FORMG0ADDR(&cdb, blk);
			FORMG0COUNT(&cdb, (uchar_t)1);
			ucmd.uscsi_cdblen = CDB_GROUP0;
		}
		mutex_exit(SD_MUTEX);
		status = sdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE,
			UIO_SYSSPACE);
		mutex_enter(SD_MUTEX);
		if (status != 0) {
			kmem_free(dkl, sizeof (struct dk_label));
			return (status);
		}
	}

	kmem_free(dkl, sizeof (struct dk_label));
	return (0);

}


static void
clean_print(dev_info_t *dev, char *label, uint_t level,
	char *title, char *data, int len)
{
	int	i;
	char	buf[256];

	(void) sprintf(buf, "%s:", title);
	for (i = 0; i < len; i++) {
		(void) sprintf(&buf[strlen(buf)], "0x%x ", (data[i] & 0xff));
	}
	(void) sprintf(&buf[strlen(buf)], "\n");

	scsi_log(dev, label, level, "%s", buf);
}

/*
 * Print a piece of inquiry data- cleaned up for non-printable characters
 * and stopping at the first space character after the beginning of the
 * passed string;
 */

static void
inq_fill(char *p, int l, char *s)
{
	register unsigned i = 0;
	char c;

	while (i++ < l) {
		if ((c = *p++) < ' ' || c >= 0177) {
			c = '*';
		} else if (i != 1 && c == ' ') {
			break;
		}
		*s++ = c;
	}
	*s++ = 0;
}

char *
sd_sname(uchar_t	status)
{
	switch (status & STATUS_MASK) {
	case STATUS_GOOD:
		return ("good status");

	case STATUS_CHECK:
		return ("check condition");

	case STATUS_MET:
		return ("condition met");

	case STATUS_BUSY:
		return ("busy");

	case STATUS_INTERMEDIATE:
		return ("intermediate");

	case STATUS_INTERMEDIATE_MET:
		return ("intermediate - condition met");

	case STATUS_RESERVATION_CONFLICT:
		return ("reservation_conflict");

	case STATUS_TERMINATED:
		return ("command terminated");

	case STATUS_QFULL:
		return ("queue full");

	default:
		return ("<unknown status>");
	}
}

/*
 * Create device error kstats
 */
static int
sd_create_errstats(struct scsi_disk *un, int instance)
{

	char kstatname[KSTAT_STRLEN];

	if (un->un_errstats == (kstat_t *)0) {
		(void) sprintf(kstatname, "sd%d,err", instance);
		un->un_errstats = kstat_create("sderr", instance, kstatname,
			"device_error", KSTAT_TYPE_NAMED,
			sizeof (struct sd_errstats) / sizeof (kstat_named_t),
			KSTAT_FLAG_PERSISTENT);

		if (un->un_errstats) {
			struct sd_errstats	*stp;

			stp = (struct sd_errstats *)un->un_errstats->ks_data;
			kstat_named_init(&stp->sd_softerrs, "Soft Errors",
				KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_harderrs, "Hard Errors",
				KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_transerrs,
				"Transport Errors", KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_vid, "Vendor",
				KSTAT_DATA_CHAR);
			kstat_named_init(&stp->sd_pid, "Product",
				KSTAT_DATA_CHAR);
			kstat_named_init(&stp->sd_revision, "Revision",
				KSTAT_DATA_CHAR);
			kstat_named_init(&stp->sd_serial, "Serial No",
				KSTAT_DATA_CHAR);
			kstat_named_init(&stp->sd_capacity, "Size",
				KSTAT_DATA_ULONGLONG);
			kstat_named_init(&stp->sd_rq_media_err, "Media Error",
				KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_rq_ntrdy_err,
				"Device Not Ready", KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_rq_nodev_err, "No Device",
				KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_rq_recov_err, "Recoverable",
				KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_rq_illrq_err,
				"Illegal Request", KSTAT_DATA_UINT32);
			kstat_named_init(&stp->sd_rq_pfa_err,
				"Predictive Failure Analysis",
				KSTAT_DATA_UINT32);
			un->un_errstats->ks_private = un;
			un->un_errstats->ks_update = nulldev;
			kstat_install(un->un_errstats);
			/*
			 * Fill in the static data
			 */
			(void) strncpy(&stp->sd_vid.value.c[0],
					SD_INQUIRY->inq_vid, 8);
			(void) strncpy(&stp->sd_pid.value.c[0],
					SD_INQUIRY->inq_pid, 16);
			(void) strncpy(&stp->sd_revision.value.c[0],
					SD_INQUIRY->inq_revision, 4);
			(void) strncpy(&stp->sd_serial.value.c[0],
					(void *)&un->un_serial_num_buf[0], 16);

			if (un->un_capacity < 0) {
				/*
				 * Set capacity to 0 for no media.
				 * The ON sd driver sets it to un_capacity (-1)
				 * which results in incorect capacity beign
				 * displayed for 'iostat -E'.
				 */
				stp->sd_capacity.value.ui64 = 0;
			} else {
				/*
				 * Multiply un_capacity by DEV_BSIZE to get
				 * capacity. "un_capacity" already multiplied by
				 * (un_lbasize / DEV_BSIZE) in sd_read_capacity.
				 */
				stp->sd_capacity.value.ui64 =
					(uint64_t)((uint64_t)un->un_capacity *
					DEV_BSIZE);
			}
		}
	}
	return (0);
}


uint_t
sd_stoh_int(uchar_t *p)
{
	return ((p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3]);
}

ushort_t
sd_stoh_short(uchar_t *p)
{
	return ((p[0] << 8) + p[1]);
}

uint_t
sd_letoh_int(uchar_t *p)
{
	return ((p[3] << 24) + (p[2] << 16) + (p[1] << 8) + p[0]);
}

ushort_t
sd_letoh_short(uchar_t *p)
{
	return ((p[1] << 8) + p[0]);
}
static int
sd_setup_next_xfer(struct scsi_disk *un, register struct buf *bp,
	struct scsi_pkt *pkt, struct sd_pkt_private *sdpp)
{
	register ssize_t		secnt;
	register daddr_t	blkno;
	register ssize_t		resid;
	int			partition = SDPART(bp->b_edev);
	size_t			overrun;
	int			com;

	ASSERT(bp != un->un_sbufp);

	ASSERT(pkt->pkt_resid == 0);

	/*
	 * Calculate next block number and amount to be transferred
	 */
	resid = sdpp->sdpp_dma_resid;

	bp->b_resid -= resid;

	overrun = bp->b_resid;
	bp->b_bcount -= overrun;

	blkno = dkblock(bp) + ((bp->b_bcount - resid) >> un->un_secdiv);
	secnt = resid >> un->un_secdiv;

	/*
	 * Adjust block number to absolute.
	 * This should not be done for non 512 cdroms. cdrom_blksize
	 * routine has already taken care of that.
	 */
	if (un->un_lbasize == DEV_BSIZE) {
		blkno += un->un_offset[partition];
	}

	/*
	 * Move pkt to the next portion of the xfer.
	 * func is NULL_FUNC so we do not have to release
	 * the disk mutex here.
	 */

	if (scsi_init_pkt(ROUTE, pkt, bp, 0, 0, 0, 0,
			NULL_FUNC, NULL) == NULL) {
		/*
		 * Error setting up next portion of cmd transfer.
		 * Something is definitely very wrong and this
		 * should not happen.
		 */
		bp->b_bcount += overrun;
		bp->b_resid += resid;
		bp->b_flags |= B_ERROR;

		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    "Error setting up next portion"
		    "of DMA transfer\n");

		return (0);
	}

	bp->b_bcount += overrun;

	/*
	 * Adjust things if there is still more DMA resid
	 */
	sdpp->sdpp_dma_resid = pkt->pkt_resid;
	if (pkt->pkt_resid != 0) {
		secnt -= (pkt->pkt_resid >> un->un_secdiv);
	}

	com = (bp->b_flags & B_READ) ? SCMD_READ : SCMD_WRITE;

	switch (sdpp->sdpp_cdblen) {
	case CDB_GROUP0:
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			com, (int)blkno, (int)secnt, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		break;

	case CDB_GROUP1:
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			com | SCMD_GROUP1, (int)blkno, (int)secnt, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		break;

	default:

		/* Should not happen */
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
		    "Error setting up next portion"
		    "of DMA transfer. Invalid CDB length: %d\n",
		    sdpp->sdpp_cdblen);

		return (0);

	}

	pkt->pkt_resid = 0;

	return (1);
}


/*
 * Compare the vendor+product ID from the sd_disk_table to the
 * response from the drive. Treat every occurrence of one or blanks
 * the same as a single blank.
 *
 */

static int
sd_blank_cmp(struct scsi_disk *un, char *device_id)
{
	char	*p1 = device_id;
	char	*p2 = SD_INQUIRY->inq_vid;
	int	cnt = sizeof (SD_INQUIRY->inq_vid) +
		    sizeof (SD_INQUIRY->inq_pid);

	/*
	 * Note: string p1 is terminated by a NUL but string p2 isn't.
	 * the end of p2 is determined by cnt.
	 */

	/*LINTED*/
	while (1) {
		/*
		 * skip over any extra blanks in both strings
		 */
		while (*p1 != '\0' && *p1 == ' ')
			p1++;

		while (cnt != 0 && *p2 == ' ') {
			p2++;
			cnt--;
		}

		/*
		 * compare the two strings
		 */

		if (cnt == 0 || *p1 != *p2)
			break;

		while (cnt > 0 && *p1 == *p2) {
			p1++;
			p2++;
			cnt--;
		}

	}

	/* return TRUE if both strings match */
	return ((*p1 == '\0' && cnt == 0) ? TRUE : FALSE);
}

void
sd_print_label_state(struct scsi_disk *un, char *pstr)
{
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "current location: %s\n", pstr);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "gvalid: %d   un_capacity: %d\n",
	    (int)un->un_gvalid, un->un_capacity);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
	    "lgeom.g_capacity: %ld\tpgeom.g_capacity: %ld\n",
	    un->un_lgeom.g_capacity, un->un_pgeom.g_capacity);

}
