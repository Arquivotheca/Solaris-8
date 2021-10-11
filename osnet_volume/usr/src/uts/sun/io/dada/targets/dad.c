/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dad.c	1.65	99/06/17 SMI"

/*
 * Direct Attached  disk driver for SPARC machines.
 */

/*
 * Includes, Declarations and Local Data
 */
#include <sys/dada/dada.h>
#include <sys/dkbad.h>
#include <sys/dklabel.h>
#include <sys/dkio.h>
#include <sys/cdio.h>
#include <sys/vtoc.h>
#include <sys/dada/targets/daddef.h>
#include <sys/dada/targets/dadpriv.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kstat.h>
#include <sys/vtrace.h>
#include <sys/aio_req.h>
#include <sys/note.h>

/*
 * Global Error Levels for Error Reporting
 */
int dcd_error_level	= DCD_ERR_RETRYABLE;

/*
 * Local Static Data
 */

static int dcd_io_time		= DCD_IO_TIME;
static int dcd_retry_count	= DCD_RETRY_COUNT;
#ifndef lint
static int dcd_report_pfa = 1;
#endif
static int dcd_rot_delay = 4;
static int dcd_poll_busycnt = DCD_POLL_TIMEOUT;

/*
 * Local Function Prototypes
 */

static int dcdopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p);
static int dcdclose(dev_t dev, int flag, int otyp, cred_t *cred_p);
static int dcdstrategy(struct buf *bp);
static int dcddump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk);
static int dcdioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int dcdread(dev_t dev, struct uio *uio, cred_t *cred_p);
static int dcdwrite(dev_t dev, struct uio *uio, cred_t *cred_p);
static int dcd_prop_op(dev_t, dev_info_t *, ddi_prop_op_t, int,
    char *, caddr_t, int *);
static int dcdaread(dev_t dev, struct aio_req *aio, cred_t *cred_p);
static int dcdawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p);


static void dcd_free_softstate(struct dcd_disk *un, dev_info_t *devi);
static int dcd_doattach(dev_info_t *devi, int (*f)());
static int dcd_validate_geometry(struct dcd_disk *un, int (*f)());
static int dcd_uselabel(struct dcd_disk *un, struct dk_label *l);
static ddi_devid_t dcd_get_devid(struct dcd_disk *un);
static ddi_devid_t  dcd_create_devid(struct dcd_disk *un);
static int dcd_read_deviceid(struct dcd_disk *un);
static int dcd_write_deviceid(struct dcd_disk *un);
static int dcd_poll(struct dcd_pkt *pkt);
static char *dcd_rname(int reason);
static void dcd_fake_geometry(struct dcd_disk *un);

static void dcdmin(struct buf *bp);

static int dcdioctl_cmd(dev_t, struct udcd_cmd *,
	enum uio_seg, enum uio_seg);

static void dcdstart(struct dcd_disk *un);
static void dcddone_and_mutex_exit(struct dcd_disk *un, struct buf *bp);
static void make_dcd_cmd(struct dcd_disk *un, struct buf *bp, int (*f)());
static void dcdudcdmin(struct buf *bp);

static int dcdrunout(caddr_t);
static int dcd_check_wp(dev_t dev);
static int dcd_unit_ready(dev_t dev);
static void dcd_handle_tran_busy(struct buf *bp, struct diskhd *dp,
				struct dcd_disk *un);
static void dcdintr(struct dcd_pkt *pkt);
static int dcd_handle_incomplete(struct dcd_disk *un, struct buf *bp);
static void dcd_offline(struct dcd_disk *un, int bechatty);
static int dcd_ready_and_valid(dev_t dev, struct dcd_disk *un);
static void dcd_reset_disk(struct dcd_disk *un, struct dcd_pkt *pkt);
static void dcd_translate(struct dadkio_status32 *statp, struct udcd_cmd *cmdp);

/*
 * Vtoc Control
 */
static void dcd_build_user_vtoc(struct dcd_disk *un, struct vtoc *vtoc);
static int dcd_build_label_vtoc(struct dcd_disk *un, struct vtoc *vtoc);
static int dcd_write_label(dev_t dev);

/*
 * Error and Logging Functions
 */
#ifndef lint
static void clean_print(dev_info_t *dev, char *label, uint_t level,
	char *title, char *data, int len);
static void dcdrestart(void *arg);
#endif /* lint */

static int dcd_check_error(struct dcd_disk *un, struct buf *bp);

/*
 * Error statistics create/update functions
 */
static int dcd_create_errstats(struct dcd_disk *, int);



extern void dcd_log(dev_info_t *, char *, uint_t, const char *, ...);
extern void makecommand(struct dcd_pkt *, int, uchar_t, uint32_t,
			uchar_t, uint32_t, uchar_t, uchar_t);


/*
 * Configuration Routines
 */
static int dcdinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int dcdprobe(dev_info_t *devi);
static int dcdattach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int dcddetach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int dcd_dr_detach(dev_info_t *devi);
static int dcdpower(dev_info_t *devi, int component, int level);

static void *dcd_state;
static int dcd_max_instance;
static char *dcd_label = "dad";

static char *diskokay = "disk okay\n";

#if DEBUG || lint
#define	DCDDEBUG
#endif

int dcd_test_flag = 0;
/*
 * Debugging macros
 */
#ifdef	DCDDEBUG
static int dcddebug = 0;
#define	DEBUGGING	(dcddebug > 1)
#define	DAD_DEBUG	if (dcddebug == 1) dcd_log
#define	DAD_DEBUG2	if (dcddebug > 1) dcd_log
#else	/* DCDDEBUG */
#define	dcddebug		(0)
#define	DEBUGGING	(0)
#define	DAD_DEBUG	if (0) dcd_log
#define	DAD_DEBUG2	if (0) dcd_log
#endif

/*
 * we use pkt_private area for storing bp and retry_count
 * XXX: Really is this usefull.
 */
struct dcd_pkt_private {
	struct buf	*dcdpp_bp;
	short		 dcdpp_retry_count;
	short		 dcdpp_victim_retry_count;
};


_NOTE(SCHEME_PROTECTS_DATA("Unique per pkt", dcd_pkt_private buf))

#define	PP_LEN	(sizeof (struct dcd_pkt_private))

#define	PKT_SET_BP(pkt, bp)	\
	((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_bp = bp
#define	PKT_GET_BP(pkt) \
	(((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_bp)


#define	PKT_SET_RETRY_CNT(pkt, n) \
	((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_retry_count = n

#define	PKT_GET_RETRY_CNT(pkt) \
	(((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_retry_count)

#define	PKT_INCR_RETRY_CNT(pkt, n) \
	((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_retry_count += n

#define	PKT_SET_VICTIM_RETRY_CNT(pkt, n) \
	((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_victim_retry_count \
			= n

#define	PKT_GET_VICTIM_RETRY_CNT(pkt) \
	(((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_victim_retry_count)
#define	PKT_INCR_VICTIM_RETRY_CNT(pkt, n) \
	((struct dcd_pkt_private *)pkt->pkt_private)->dcdpp_victim_retry_count \
			+= n

#define	SD_VICTIM_RETRY_COUNT		(dcd_victim_retry_count)
#define	DISK_NOT_READY_RETRY_COUNT	(dcd_retry_count / 2)

static struct driver_minor_data {
	char	*name;
	int	minor;
	int	type;
} dcd_minor_data[] = {
	{"a", 0, S_IFBLK},
	{"b", 1, S_IFBLK},
	{"c", 2, S_IFBLK},
	{"d", 3, S_IFBLK},
	{"e", 4, S_IFBLK},
	{"f", 5, S_IFBLK},
	{"g", 6, S_IFBLK},
	{"h", 7, S_IFBLK},
	{"a,raw", 0, S_IFCHR},
	{"b,raw", 1, S_IFCHR},
	{"c,raw", 2, S_IFCHR},
	{"d,raw", 3, S_IFCHR},
	{"e,raw", 4, S_IFCHR},
	{"f,raw", 5, S_IFCHR},
	{"g,raw", 6, S_IFCHR},
	{"h,raw", 7, S_IFCHR},
	{0}
};

/*
 * Urk!
 */
#define	SET_BP_ERROR(bp, err)	\
	(bp)->b_oerror = (err), bioerror(bp, err);

#define	IOSP			KSTAT_IO_PTR(un->un_stats)
#define	IO_PARTITION_STATS	un->un_pstats[DCDPART(bp->b_edev)]
#define	IOSP_PARTITION		KSTAT_IO_PTR(IO_PARTITION_STATS)

#define	DCD_DO_KSTATS(un, kstat_function, bp) \
	ASSERT(mutex_owned(DCD_MUTEX)); \
	if (bp != un->un_sbufp) { \
		if (un->un_stats) { \
			kstat_function(IOSP); \
		} \
		if (IO_PARTITION_STATS) { \
			kstat_function(IOSP_PARTITION); \
		} \
	}

#define	DCD_DO_ERRSTATS(un, x) \
	if (un->un_errstats) { \
		struct dcd_errstats *dtp; \
		dtp = (struct dcd_errstats *)un->un_errstats->ks_data; \
		dtp->x.value.ui32++; \
	}

#define	GET_SOFT_STATE(dev)						\
	register struct dcd_disk *un;					\
	register int instance, part;					\
	register minor_t minor = getminor(dev);				\
									\
	part = minor & DCDPART_MASK;					\
	instance = minor >> DCDUNIT_SHIFT;				\
	if ((un = ddi_get_soft_state(dcd_state, instance)) == NULL)	\
		return (ENXIO);

#define	LOGICAL_BLOCK_ALIGN(blkno, blknoshift) \
		(((blkno) & ((1 << (blknoshift)) - 1)) == 0)


/*
 * Configuration Data
 */

/*
 * Device driver ops vector
 */

static struct cb_ops dcd_cb_ops = {

	dcdopen,			/* open */
	dcdclose,		/* close */
	dcdstrategy,		/* strategy */
	nodev,			/* print */
	dcddump,			/* dump */
	dcdread,			/* read */
	dcdwrite,		/* write */
	dcdioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	dcd_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_64BIT | D_MP | D_NEW,	/* Driver compatibility flag */
	CB_REV,			/* cb_rev */
	dcdaread, 		/* async I/O read entry point */
	dcdawrite		/* async I/O write entry point */
};

static struct dev_ops dcd_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	dcdinfo,			/* info */
	nulldev,		/* identify */
	dcdprobe,		/* probe */
	dcdattach,		/* attach */
	dcddetach,		/* detach */
	nodev,			/* reset */
	&dcd_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	dcdpower			/* power */
};


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"DAD Disk Driver 1.65",	/* Name of the module. */
	&dcd_ops,	/* driver ops */
};



static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * the dcd_attach_mutex only protects dcd_max_instance in multi-threaded
 * attach situations
 */
static kmutex_t dcd_attach_mutex;

char _depends_on[] = "misc/dada";

int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&dcd_state, sizeof (struct dcd_disk),
	    DCD_MAXUNIT)) != 0)
		return (e);

	mutex_init(&dcd_attach_mutex, NULL, MUTEX_DRIVER, NULL);
	e = mod_install(&modlinkage);
	if (e != 0) {
		mutex_destroy(&dcd_attach_mutex);
		ddi_soft_state_fini(&dcd_state);
		return (e);
	}

	return (e);
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	ddi_soft_state_fini(&dcd_state);
	mutex_destroy(&dcd_attach_mutex);

	return (e);
}

int
_info(struct modinfo *modinfop)
{

	return (mod_info(&modlinkage, modinfop));
}

static int
dcdprobe(dev_info_t *devi)
{
	register struct dcd_device *devp;
	register int rval = DDI_PROBE_PARTIAL;
	int instance;

	devp = (struct dcd_device *)ddi_get_driver_private(devi);
	instance = ddi_get_instance(devi);

	/*
	 * Keep a count of how many disks (ie. highest instance no) we have
	 * XXX currently not used but maybe useful later again
	 */
	mutex_enter(&dcd_attach_mutex);
	if (instance > dcd_max_instance)
		dcd_max_instance = instance;
	mutex_exit(&dcd_attach_mutex);

	DAD_DEBUG2(devp->dcd_dev, dcd_label, DCD_DEBUG,
		    "dcdprobe:\n");

	if (ddi_get_soft_state(dcd_state, instance) != NULL)
		return (DDI_PROBE_PARTIAL);

	/*
	 * Turn around and call utility probe routine
	 * to see whether we actually have a disk at
	 */

	DAD_DEBUG2(devp->dcd_dev, dcd_label, DCD_DEBUG,
	    "sdprobe: %x\n", dcd_probe(devp, NULL_FUNC));

	switch (dcd_probe(devp, NULL_FUNC)) {
	default:
	case DCDPROBE_NORESP:
	case DCDPROBE_NONCCS:
	case DCDPROBE_NOMEM:
	case DCDPROBE_FAILURE:
	case DCDPROBE_BUSY:
		break;

	case DCDPROBE_EXISTS:
			/*
			 * Check whether it is a ATA device and then
			 * return  SUCCESS.
			 */
			DAD_DEBUG2(devp->dcd_dev, dcd_label, DCD_DEBUG,
				"config %x\n", devp->dcd_ident->dcd_config);
			if ((devp->dcd_ident->dcd_config & ATAPI_DEVICE) == 0) {
				if (devp->dcd_ident->dcd_config &
					ATANON_REMOVABLE) {
					rval = DDI_PROBE_SUCCESS;
				} else
					rval = DDI_PROBE_FAILURE;
			} else {
				rval = DDI_PROBE_FAILURE;
			}
			break;
	}
	dcd_unprobe(devp);

	DAD_DEBUG2(devp->dcd_dev, dcd_label, DCD_DEBUG,
		"dcdprobe returns %x\n", rval);

	return (rval);
}


/*ARGSUSED*/
static int
dcdattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	register struct dcd_device *devp;
	struct driver_minor_data *dmdp;
	struct dcd_disk *un;
	char *node_type;
	char name[48];
	struct diskhd *dp;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);


	devp = (struct dcd_device *)ddi_get_driver_private(devi);
	instance = ddi_get_instance(devi);
	DAD_DEBUG2(devp->dcd_dev, dcd_label, DCD_DEBUG, "Attach Started\n");

	switch (cmd) {
		case DDI_ATTACH:
			break;

		case DDI_RESUME:
		case DDI_PM_RESUME:
			if (!(un = ddi_get_soft_state(dcd_state, instance)))
				return (DDI_FAILURE);
			mutex_enter(DCD_MUTEX);
			if (un->un_state != DCD_STATE_SUSPENDED) {
				mutex_exit(DCD_MUTEX);
				return (DDI_SUCCESS);
			}
			un->un_state = un->un_last_state;
			un->un_throttle = 2;

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
				dcdstart(un);
			}

			mutex_exit(DCD_MUTEX);
			return (DDI_SUCCESS);

		default:
			return (DDI_FAILURE);
	}

	if (dcd_doattach(devi, SLEEP_FUNC) == DDI_FAILURE) {
		return (DDI_FAILURE);
	}

	if (!(un = (struct dcd_disk *)
	    ddi_get_soft_state(dcd_state, instance))) {
		return (DDI_FAILURE);
	}
	devp->dcd_private = (ataopaque_t)un;

	for (dmdp = dcd_minor_data; dmdp->name != NULL; dmdp++) {
		node_type = DDI_NT_BLOCK_CHAN;
		(void) sprintf(name, "%s", dmdp->name);
		if (ddi_create_minor_node(devi, name, dmdp->type,
		    (instance << DCDUNIT_SHIFT) | dmdp->minor,
		    node_type, NULL) == DDI_FAILURE) {
			/*
			 * Free Resouces allocated in sd_doattach
			 */
			dcd_free_softstate(un, devi);
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
		if (pm_idle_component(devi, 0) == DDI_FAILURE) {
			return (DDI_FAILURE);
		}
		pm_set_normal_power(devi, 0, 1);
	} else {
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
	    strlen("needs-suspend-resume") + 1);

	ddi_report_dev(devi);

	un->un_gvalid = 0;

	/*
	 * Taking the SD_MUTEX isn't necessary in this context, but
	 * the devid functions release the SD_MUTEX. We take the
	 * SD_MUTEX here in order to ensure that the devid functions
	 * can function coherently in other contexts.
	 */
	mutex_enter(DCD_MUTEX);

	(void) dcd_validate_geometry(un, NULL_FUNC);

	if (dcd_get_devid(un) == NULL) {
		/* Create the fab'd devid */
		(void) dcd_create_devid(un);
	}

	mutex_exit(DCD_MUTEX);


	return (DDI_SUCCESS);
}

static void
dcd_free_softstate(struct dcd_disk *un, dev_info_t *devi)
{
	struct dcd_device		*devp;
	int instance = ddi_get_instance(devi);

	devp = (struct dcd_device *)ddi_get_driver_private(devi);

	if (un) {
		sema_destroy(&un->un_semoclose);
		cv_destroy(&un->un_sbuf_cv);
		cv_destroy(&un->un_state_cv);

		/*
		 * Deallocate command packet resources.
		 */
		if (un->un_sbufp)
			freerbuf(un->un_sbufp);
		if (un->un_dp) {
			kmem_free((caddr_t)un->un_dp, sizeof (*un->un_dp));
		}

		/*
		 * Delete kstats. Kstats for non CD devices are deleted
		 * in sdclose.
		 */
		if (un->un_stats) {
			kstat_delete(un->un_stats);
		}

	}

	/*
	 * Cleanup scsi_device resources.
	 */
	ddi_soft_state_free(dcd_state, instance);
	devp->dcd_private = (ataopaque_t)0;
	/* unprobe scsi device */
	dcd_unprobe(devp);

	/* Remove properties created during attach */
	ddi_prop_remove_all(devi);
	ddi_remove_minor_node(devi, NULL);
}

static int
dcddetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance, delay_count;
	struct dcd_disk *un;
	register struct diskhd *dp;

	instance = ddi_get_instance(devi);

	if (!(un = ddi_get_soft_state(dcd_state, instance)))
		return (DDI_FAILURE);

	switch (cmd) {
		case DDI_DETACH:
			return (dcd_dr_detach(devi));

		case DDI_SUSPEND:
			mutex_enter(DCD_MUTEX);
			if (un->un_state == DCD_STATE_SUSPENDED) {
				mutex_exit(DCD_MUTEX);
				return (DDI_SUCCESS);
			}
			un->un_throttle = 0;
			New_state(un, DCD_STATE_SUSPENDED);

			delay_count = 10;
			while ((delay_count --) &&
				(un->un_ncmds > 0)) {
				mutex_exit(DCD_MUTEX);
				delay(10);
				mutex_enter(DCD_MUTEX);
			}

			if (un->un_ncmds > 0) {
				mutex_exit(DCD_MUTEX);
				if (dcd_reset(ROUTE, RESET_ALL) == 0) {
					mutex_enter(DCD_MUTEX);
					un->un_state = un->un_last_state;
					mutex_exit(DCD_MUTEX);
					return (DDI_FAILURE);
				}
			} else {
				mutex_exit(DCD_MUTEX);
			}

			return (DDI_SUCCESS);

		case DDI_PM_SUSPEND:
			mutex_enter(DCD_MUTEX);
			dp = &un->un_utab;
			if ((un->un_state == DCD_STATE_SUSPENDED) ||
					dp->b_forw || dp->b_actf ||
					un->un_ncmds) {
				mutex_exit(DCD_MUTEX);
				return (DDI_FAILURE);
			}
			un->un_throttle = 0;
			un->un_state = DCD_STATE_SUSPENDED;
			mutex_exit(DCD_MUTEX);
			return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
dcd_dr_detach(dev_info_t *devi)
{
	struct dcd_device		*devp;
	struct dcd_disk		*un;

	/*
	 * Get scsi_device structure for this instance.
	 */
	if (!(devp = (struct dcd_device *)ddi_get_driver_private(devi))) {
		return (DDI_FAILURE);
	}

	/*
	 * Get dcd_disk structure containing target 'private' information
	 */
	un = (struct dcd_disk *)devp->dcd_private;

	/*
	 * Verify there are NO outstanding commands issued to this device.
	 * ie, un_ncmds == 0.
	 * It's possible to have outstanding commands through the physio
	 * code path, even though everything's closed.
	 */
	_NOTE(COMPETING_THREADS_NOW);
	mutex_enter(DCD_MUTEX);
	if (un->un_ncmds) {
		mutex_exit(DCD_MUTEX);
		_NOTE(NO_COMPETING_THREADS_NOW);
		return (DDI_FAILURE);
	}
	mutex_exit(DCD_MUTEX);

	_NOTE(NO_COMPETING_THREADS_NOW);

	/*
	 * at this point there are no competing threads anymore
	 * release active MT locks and all device resources.
	 */
	dcd_free_softstate(un, devi);

	return (DDI_SUCCESS);
}

static int
dcdpower(dev_info_t *devi, int component, int level)
{
	int		instance;
	struct dcd_disk *un;

	instance = ddi_get_instance(devi);

	if (!(un = ddi_get_soft_state(dcd_state, instance)) ||
	    (0 > level) || (level > 1) || component != 0)
		return (DDI_FAILURE);
	ASSERT(un->un_state == DCD_STATE_SUSPENDED);
	(void) makedevice(ddi_name_to_major("dad"), instance<<3);
	/* XXX:Here you can do some no disk access command */
	return (DDI_SUCCESS);
}

static int
dcd_doattach(dev_info_t *devi, int (*canwait)())
{
	struct dcd_device *devp;
	auto struct dcd_capacity cbuf;
	register struct dcd_disk *un = (struct dcd_disk *)0;
	int instance;
	int km_flags = (canwait != NULL_FUNC)? KM_SLEEP : KM_NOSLEEP;
	int	rval;
	char *prop_template = "target%x-dcd-options";
	int options;
	char	prop_str[32];
	int 	target;
	uint32_t	no_of_lbasec;


	devp = (struct dcd_device *)ddi_get_driver_private(devi);

	/*
	 * Call the routine scsi_probe to do some of the dirty work.
	 * If the INQUIRY command succeeds, the field sd_inq in the
	 * device structure will be filled in. The sd_sense structure
	 * will also be allocated.
	 */

	switch (dcd_probe(devp, canwait)) {
	default:
		return (DDI_FAILURE);

	case DCDPROBE_EXISTS:
		if ((devp->dcd_ident->dcd_config & ATAPI_DEVICE) == 0) {
			if (devp->dcd_ident->dcd_config & ATANON_REMOVABLE) {
					rval = DDI_SUCCESS;
			} else {
				rval = DDI_FAILURE;
				goto error;
			}
		} else {
			rval = DDI_FAILURE;
			goto error;
		}

	}


	instance = ddi_get_instance(devp->dcd_dev);

	if (ddi_soft_state_zalloc(dcd_state, instance) != DDI_SUCCESS) {
		rval = DDI_FAILURE;
		goto error;
	}

	un = ddi_get_soft_state(dcd_state, instance);

	un->un_sbufp = getrbuf(km_flags);
	if (un->un_sbufp == (struct buf *)NULL) {
		rval = DDI_FAILURE;
		goto error;
	}


	un->un_dcd = devp;

	sema_init(&un->un_semoclose, 1, NULL, SEMA_DRIVER, NULL);
	cv_init(&un->un_sbuf_cv, NULL, CV_DRIVER, NULL);
	cv_init(&un->un_state_cv, NULL, CV_DRIVER, NULL);

	if (un->un_dp == 0) {
		/*
		 * Assume CCS drive, assume parity, but call
		 * it a CDROM if it is a RODIRECT device.
		 */
		un->un_dp = (struct dcd_drivetype *)
		    kmem_zalloc(sizeof (struct dcd_drivetype), km_flags);
		if (!un->un_dp) {
			rval = DDI_FAILURE;
			goto error;
		}
		if ((devp->dcd_ident->dcd_config & ATAPI_DEVICE) == 0) {
			if (devp->dcd_ident->dcd_config & ATANON_REMOVABLE) {
				un->un_dp->ctype = CTYPE_DISK;
			}
		} else  {
			rval = DDI_FAILURE;
			goto error;
		}
		un->un_dp->name = "CCS";
		un->un_dp->options = 0;
	}

	/*
	 * Allow I/O requests at un_secsize offset in multiple of un_secsize.
	 */
	un->un_secsize = DEV_BSIZE;

	/*
	 * If the device is not a removable media device, make sure that
	 * that the device is ready, by issuing the another identify but
	 * not needed. Get the capacity from identify data and store here.
	 */

	cbuf.lbasize = DEV_BSIZE;
	cbuf.ncyls  =  devp->dcd_ident->dcd_fixcyls;
	cbuf.heads  =  devp->dcd_ident->dcd_heads;
	cbuf.sectors = devp->dcd_ident->dcd_sectors;
	cbuf.capacity = (uint32_t)cbuf.ncyls * cbuf.heads * cbuf.sectors;

	no_of_lbasec = devp->dcd_ident->dcd_addrsec[1];
	no_of_lbasec = no_of_lbasec << 16;
	no_of_lbasec = no_of_lbasec | devp->dcd_ident->dcd_addrsec[0];
	if (no_of_lbasec > cbuf.capacity) {
		cbuf.capacity = no_of_lbasec;
	}

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "Geometry Data\n");
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "cyls %x, heads %x",
		cbuf.ncyls, cbuf.heads);
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "sectors %x,",
		cbuf.sectors);
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "capacity %x\n",
		cbuf.capacity);

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			    "dcdprobe: drive selected\n");

	if (cbuf.capacity != ((uint32_t)-1)) {
		un->un_capacity = cbuf.capacity;
		un->un_diskcapacity = cbuf.capacity;
		un->un_lbasize = cbuf.lbasize;
	}


	/*
	 * Check for the property target<n>-dcd-options to find the option
	 * set by the HBA driver for this target so that we can set the
	 * Unit structure variable so that we can send commands accordingly.
	 */
	target = devp->dcd_address->a_target;
	(void) sprintf(prop_str, prop_template, target);
	options = ddi_prop_get_int(DDI_DEV_T_ANY, devi, DDI_PROP_NOTPROM,
			prop_str, -1);
	if (options < 0) {
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"No per target properties");
	} else {
		if ((options & DCD_DMA_MODE) == DCD_DMA_MODE) {
			un->un_dp->options |= DMA_SUPPORTTED;
			un->un_dp->dma_mode = (options >> 3) & 0x03;
			DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
				"mode %x\n", un->un_dp->dma_mode);
		} else {
			un->un_dp->options &= ~DMA_SUPPORTTED;
			un->un_dp->pio_mode = options & 0x7;
			if (options & DCD_BLOCK_MODE)
				un->un_dp->options |= BLOCK_MODE;
			DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
				"mode %x\n", un->un_dp->pio_mode);
		}
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"options %x,", un->un_dp->options);
	}

	un->un_throttle = 2;
	/*
	 * set default max_xfer_size - This should depend on whether the
	 * Block mode is supported by the device or not.
	 */
	un->un_max_xfer_size = MAX_ATA_XFER_SIZE;

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"dcd_doattach returns good\n");

	return (rval);

error:
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "dcd_doattach failed\n");
	dcd_free_softstate(un, devi);
	return (rval);
}


#ifdef NOTNEEDED
/*
 * This routine is used to set the block mode of operation by issuing the
 * Set Block mode ata command with the maximum block mode possible
 */
dcd_set_multiple(struct dcd_disk *un)
{
	int status;
	struct udcd_cmd ucmd;
	struct dcd_cmd cdb;
	dev_t	dev;


	/* Zero all the required structure */
	(void) bzero((caddr_t)&ucmd, sizeof (ucmd));

	(void) bzero((caddr_t)&cdb, sizeof (struct dcd_cmd));

	cdb.cmd = ATA_SET_MULTIPLE;
	/*
	 * Here we should pass what needs to go into sector count REGISTER.
	 * Eventhough this field indicates the number of bytes to read we
	 * need to specify the block factor in terms of bytes so that it
	 * will be programmed by the HBA driver into the sector count register.
	 */
	cdb.size = un->un_lbasize * un->un_dp->block_factor;

	cdb.sector_num.lba_num = 0;
	cdb.address_mode = ADD_LBA_MODE;
	cdb.direction = NO_DATA_XFER;

	ucmd.udcd_flags = 0;
	ucmd.udcd_cmd = &cdb;
	ucmd.udcd_bufaddr = NULL;
	ucmd.udcd_buflen = 0;
	ucmd.udcd_flags |= UDCD_SILENT;

	dev = makedevice(ddi_name_to_major(ddi_get_name(DCD_DEVINFO)),
		ddi_get_instance(DCD_DEVINFO) << DCDUNIT_SHIFT);


	status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);

	return (status);
}
/*
 * The following routine is used only for setting the transfer mode
 * and it is not designed for transferring any other features subcommand.
 */
dcd_set_features(struct dcd_disk *un, uchar_t mode)
{
	int status;
	struct udcd_cmd ucmd;
	struct dcd_cmd cdb;
	dev_t	dev;


	/* Zero all the required structure */
	(void) bzero((caddr_t)&ucmd, sizeof (ucmd));

	(void) bzero((caddr_t)&cdb, sizeof (struct dcd_cmd));

	cdb.cmd = ATA_SET_FEATURES;
	/*
	 * Here we need to pass what needs to go into the sector count register
	 * But in the case of set features command the value taken in the
	 * sector count register depends what type of subcommand is
	 * passed in the features register. Since we have defined th size to
	 * be the size in bytes in this context it doesnot indicate bytes
	 * instead it indicates the mode to be programmed.
	 */
	cdb.size = un->un_lbasize * mode;

	cdb.sector_num.lba_num = 0;
	cdb.address_mode = ADD_LBA_MODE;
	cdb.direction = NO_DATA_XFER;
	cdb.features = ATA_FEATURE_SET_MODE;
	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"size %x, features %x, cmd %x\n",
		cdb.size, cdb.features, cdb.cmd);

	ucmd.udcd_flags = 0;
	ucmd.udcd_cmd = &cdb;
	ucmd.udcd_bufaddr = NULL;
	ucmd.udcd_buflen = 0;
	ucmd.udcd_flags |= UDCD_SILENT;

	dev = makedevice(ddi_name_to_major(ddi_get_name(DCD_DEVINFO)),
		ddi_get_instance(DCD_DEVINFO) << DCDUNIT_SHIFT);


	status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);

	return (status);
}
#endif

/*
 * Validate the geometry for this disk, e.g.,
 * see whether it has a valid label.
 */
static int
dcd_validate_geometry(struct dcd_disk *un, int (*f)())
{
	register struct dcd_pkt *pkt;
	register int secsize = 0;
	auto struct dk_label *dkl;
	auto char *label = 0;
	struct	dcd_device *devp;
	int count;
	int secdiv;
	int label_error = 0;
	int gvalid = un->un_gvalid;
	unsigned char com;

	ASSERT(mutex_owned(DCD_MUTEX));
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"dcd_validate_geometry: started \n");

	if (un->un_lbasize < 0) {
		return (DCD_BAD_LABEL);
	}

	secsize = un->un_secsize;

	/*
	 * take a log base 2 of sector size (sorry)
	 */
	for (secdiv = 0; secsize = secsize >> 1; secdiv++)
		;
	un->un_secdiv = secdiv;

	/*
	 * Only DIRECT ACCESS devices will have Sun labels.
	 * CD's supposedly have a Sun label, too
	 */

	devp = un->un_dcd;

	if (((devp->dcd_ident->dcd_config & ATAPI_DEVICE) == 0) &&
		(devp->dcd_ident->dcd_config & ATANON_REMOVABLE)) {
		int i;
		struct buf *bp;
		bp = dcd_alloc_consistent_buf(ROUTE,
		    (struct buf *)NULL, un->un_lbasize, B_READ, f, NULL);
		if (!bp) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    "no bp for disk label\n");
			return (DCD_NO_MEM_FOR_LABEL);
		}
		pkt = dcd_init_pkt(ROUTE, (struct dcd_pkt *)NULL,
		    bp, (uint32_t)sizeof (struct dcd_cmd), 2, PP_LEN,
		    PKT_CONSISTENT, f, NULL);
		if (!pkt) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    "no memory for disk label\n");
			dcd_free_consistent_buf(bp);
			return (DCD_NO_MEM_FOR_LABEL);
		}

		*((caddr_t *)&dkl) = bp->b_un.b_addr;

		bzero((caddr_t)dkl,  un->un_lbasize);

		if ((un->un_dp->options & DMA_SUPPORTTED) == DMA_SUPPORTTED) {
				com = ATA_READ_DMA;
		} else {
			if (un->un_dp->options & BLOCK_MODE)
				com = ATA_READ_MULTIPLE;
			else
				com = ATA_READ;
		}
		(void) makecommand(pkt, 0, com, 0, ADD_LBA_MODE,
			un->un_lbasize, DATA_READ, 0);
		/*
		 * Ok, it's ready - try to read and use the label.
		 */
		mutex_exit(DCD_MUTEX);
		for (i = 0; i < 3; i++) {
			if (dcd_poll(pkt) ||
			    SCBP_C(pkt) != STATUS_GOOD ||
			    (pkt->pkt_state & STATE_XFERRED_DATA) == 0 ||
			    (pkt->pkt_resid != 0)) {
				DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
				"Status %x, state %x, resid %x\n",
				SCBP_C(pkt), pkt->pkt_state, pkt->pkt_resid);
				label_error = DCD_BAD_LABEL;
			} else {
				/*
				 * dcd_uselabel will establish
				 * that the geometry is valid
				 */
				label_error = dcd_uselabel(un, dkl);
				break;
			}
		}
		mutex_enter(DCD_MUTEX);
		label = (char *)un->un_asciilabel;
		dcd_destroy_pkt(pkt);
		dcd_free_consistent_buf(bp);
	} else if (un->un_capacity < 0) {
		return (DCD_BAD_LABEL);
	}

	if (un->un_state == DCD_STATE_NORMAL && gvalid == 0) {
		/*
		 * Print out a message indicating who and what we are.
		 * We do this only when we happen to really validate the
		 * geometry. We may call sd_validate_geometry() at other
		 * times, eg, ioctl()'s like Get VTOC in which case we
		 * dont want to print the label.
		 * If the geometry is valid, print the label string,
		 * else print vendor and product info, if available
		 */
		if (un->un_gvalid) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_CONT,
				"?<%s>\n", label);
		}
	}

	for (count = 0; count < NDKMAP; count++) {
		struct dk_map32 *lp  = &un->un_map[count];
		un->un_offset[count] =
		    un->un_g.dkg_nhead *
		    un->un_g.dkg_nsect *
		    lp->dkl_cylno;
	}

	/*
	 * take a log base 2 of logical block size
	 */
	secsize = un->un_lbasize;
	for (secdiv = 0; secsize = secsize >> 1; secdiv++)
		;
	un->un_lbadiv = secdiv;

	/*
	 * take a log base 2 of the multiple of DEV_BSIZE blocks that
	 * make up one logical block
	 */
	secsize = un->un_lbasize >> DEV_BSHIFT;
	for (secdiv = 0; secsize = secsize >> 1; secdiv++)
		;
	un->un_blknoshift = secdiv;

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"dcd_validate_geometry : returns %x\n", label_error);
	return (label_error);
}

/*
 * Check the label for righteousity, and snarf yummos from validated label.
 * Marks the geometry of the unit as being valid.
 */

static int
dcd_uselabel(struct dcd_disk *un, struct dk_label *l)
{
	static char *geom = "Label says %d blocks, Drive says %d blocks\n";
	static char *badlab = "corrupt label - %s\n";
	short *sp, sum, count;
	int	capacity;
	int	label_error = 0;

	/*
	 * Check magic number of the label
	 */
	if (l->dkl_magic != DKL_MAGIC) {
		if (un->un_state == DCD_STATE_NORMAL)
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN, badlab,
			    "wrong magic number");
		return (DCD_BAD_LABEL);
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
		if (un->un_state == DCD_STATE_NORMAL) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN, badlab,
			    "label checksum failed");
		}
		return (DCD_BAD_LABEL);
	}

	mutex_enter(DCD_MUTEX);
	/*
	 * Fill in disk geometry from label.
	 */
	un->un_g.dkg_ncyl = l->dkl_ncyl;
	un->un_g.dkg_acyl = l->dkl_acyl;
	un->un_g.dkg_bcyl = 0;
	un->un_g.dkg_nhead = l->dkl_nhead;
	un->un_g.dkg_bhead = l->dkl_bhead;
	un->un_g.dkg_nsect = l->dkl_nsect;
	un->un_g.dkg_gap1 = l->dkl_gap1;
	un->un_g.dkg_gap2 = l->dkl_gap2;
	un->un_g.dkg_intrlv = l->dkl_intrlv;
	un->un_g.dkg_pcyl = l->dkl_pcyl;
	un->un_g.dkg_rpm = l->dkl_rpm;

	/*
	 * The Read and Write reinstruct values may not be vaild
	 * for older disks.
	 */
	un->un_g.dkg_read_reinstruct = l->dkl_read_reinstruct;
	un->un_g.dkg_write_reinstruct = l->dkl_write_reinstruct;

	/*
	 * If labels don't have pcyl in them, make a guess at it.
	 * The right thing, of course, to do, is to do a MODE SENSE
	 * on the Rigid Disk Geometry mode page and check the
	 * information with that. Won't work for non-CCS though.
	 */

	if (un->un_g.dkg_pcyl == 0)
		un->un_g.dkg_pcyl = un->un_g.dkg_ncyl + un->un_g.dkg_acyl;

	/*
	 * Fill in partition table.
	 */

	bcopy((caddr_t)l->dkl_map, (caddr_t)un->un_map,
	    NDKMAP * sizeof (struct dk_map32));

	/*
	 * Fill in VTOC Structure.
	 */
	bcopy((caddr_t)&l->dkl_vtoc, (caddr_t)&un->un_vtoc,
	    sizeof (struct dk_vtoc));

	un->un_gvalid = 1;			/* "it's here..." */

	capacity = un->un_g.dkg_ncyl * un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	if (un->un_g.dkg_acyl)
		capacity +=  (un->un_g.dkg_nhead * un->un_g.dkg_nsect);

	if (un->un_capacity > 0) {
		if (capacity > un->un_diskcapacity &&
		    un->un_state == DCD_STATE_NORMAL) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    badlab, "bad geometry");
			dcd_log(DCD_DEVINFO, dcd_label, CE_CONT,
			    (const char *) geom, capacity, un->un_capacity);
			un->un_gvalid = 0;
			label_error = DCD_BAD_LABEL;
		} else {
			un->un_capacity = capacity;
			DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, geom,
			    capacity, un->un_capacity);
		}
	} else {
		/*
		 * We have a situation where the target didn't give us a
		 * good 'read capacity' command answer, yet there appears
		 * to be a valid label. In this case, we'll
		 * fake the capacity.
		 */
		un->un_capacity = capacity;
	}

	bcopy((caddr_t)l->dkl_asciilabel,
	    (caddr_t)un->un_asciilabel, LEN_DKL_ASCII);
	mutex_exit(DCD_MUTEX);

	return (label_error);
}


/*
 * Unix Entry Points
 */

/* ARGSUSED3 */
static int
dcdopen(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	register dev_t dev = *dev_p;
	register int rval = EIO;
	register int partmask;
	register int nodelay = (flag & (FNDELAY | FNONBLOCK));
	int i;
	char kstatname[KSTAT_STRLEN];

	GET_SOFT_STATE(dev);

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"Inside Open flag %x, otyp %x\n", flag, otyp);

	if (otyp >= OTYPCNT) {
		return (EINVAL);
	}

	partmask = 1 << part;

	/*
	 * We use a semaphore here in order to serialize
	 * open and close requests on the device.
	 */
	sema_p(&un->un_semoclose);

	mutex_enter(DCD_MUTEX);

	if ((un->un_state & DCD_STATE_FATAL) == DCD_STATE_FATAL) {
		rval = ENXIO;
		goto done;
	}
	if (un->un_state == DCD_STATE_SUSPENDED) {
		mutex_exit(DCD_MUTEX);
		(void) ddi_dev_is_needed(DCD_DEVINFO, 0, 1);
		mutex_enter(DCD_MUTEX);
	}

	/*
	 * set make_sd_cmd() flags and stat_size here since these
	 * are unlikely to change
	 */
	un->un_cmd_flags = 0;

	un->un_cmd_stat_size = 2;

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG, "sdopen un=%x\n", un);
	/*
	 * check for previous exclusive open
	 */
	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"exclopen=%x, flag=%x, regopen=%x\n",
		un->un_exclopen, flag, un->un_ocmap.regopen[otyp]);
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"Exclusive open flag %x, partmask %x\n",
		un->un_exclopen, partmask);

	if (un->un_exclopen & (partmask)) {
failed_exclusive:
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
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
	if (flag & FWRITE) {
		mutex_exit(DCD_MUTEX);
			if (dcd_check_wp(dev)) {
				sema_v(&un->un_semoclose);
				return (EROFS);
			}
			mutex_enter(DCD_MUTEX);
	}

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"Check Write Protect handled\n");

	if (!nodelay) {
		mutex_exit(DCD_MUTEX);
		if ((rval = dcd_ready_and_valid(dev, un)) != 0) {
			rval = EIO;
		}
		mutex_enter(DCD_MUTEX);
		/*
		 * Fail if device is not ready or if the number of disk
		 * blocks is zero or negative for non CD devices.
		 */
		if (rval || (un->un_map[part].dkl_nblk <= 0)) {
			rval = EIO;
			goto done;
		}
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


	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"open of part %d type %d\n",
		part, otyp);

	mutex_exit(DCD_MUTEX);

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"Kstats getting updated\n");
	/*
	 * only create kstats for disks, CD kstats created in sdattach
	 */
	_NOTE(NO_COMPETING_THREADS_NOW);
	if (un->un_stats == (kstat_t *)0) {
		un->un_stats = kstat_create("dad", instance,
				NULL, "disk", KSTAT_TYPE_IO, 1,
				KSTAT_FLAG_PERSISTENT);
		if (un->un_stats) {
			un->un_stats->ks_lock = DCD_MUTEX;
			kstat_install(un->un_stats);
		}
		/*
		 * set up partition statistics for each partition
		 * with number of blocks > 0
		*/
		for (i = 0; i < NDKMAP; i++) {
			if ((un->un_pstats[i] == (kstat_t *)0) &&
			    (un->un_map[i].dkl_nblk != 0)) {
				(void) sprintf(kstatname, "dad%d,%s\0",
				    instance, dcd_minor_data[i].name);
				un->un_pstats[i] = kstat_create("dad",
					instance, kstatname, "partition",
					KSTAT_TYPE_IO, 1,
					KSTAT_FLAG_PERSISTENT);
				if (un->un_pstats[i]) {
				    un->un_pstats[i]->ks_lock =
						DCD_MUTEX;
				    kstat_install(un->un_pstats[i]);
				}
			}
		}
		/*
		 * set up error kstats
		 */
		(void) dcd_create_errstats(un, instance);
	}
	_NOTE(COMPETING_THREADS_NOW);

	sema_v(&un->un_semoclose);
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "Open success\n");
	return (0);

done:
	mutex_exit(DCD_MUTEX);
	sema_v(&un->un_semoclose);
	return (rval);

}

/*
 * Test if disk is ready and has a valid geometry.
 */
static int
dcd_ready_and_valid(dev_t dev, struct dcd_disk *un)
{
	register int rval = 1;
	int g_error = 0;

	mutex_enter(DCD_MUTEX);
	/*
	 * cmds outstanding
	 */
	if (un->un_ncmds == 0) {
		(void) dcd_unit_ready(dev);
	}

	/*
	 * If device is not yet ready here, inform it is offline
	 */
	if (un->un_state == DCD_STATE_NORMAL) {
		rval = dcd_unit_ready(dev);
		if (rval != 0 && rval != EACCES) {
			dcd_offline(un, 1);
			goto done;
		}
	}

	if (un->un_format_in_progress == 0) {
		g_error = dcd_validate_geometry(un, SLEEP_FUNC);
	}

	/*
	 * check if geometry was valid. We don't check the validity of
	 * geometry for CDROMS.
	 */

	if (g_error == DCD_BAD_LABEL) {
		rval = 1;
		goto done;
	}


	/*
	 * the state has changed, inform the media watch routines
	 */
	un->un_mediastate = DKIO_INSERTED;
	cv_broadcast(&un->un_state_cv);
	rval = 0;

done:
	mutex_exit(DCD_MUTEX);
	return (rval);
}


/*ARGSUSED*/
static int
dcdclose(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	register uchar_t *cp;
	int i;

	GET_SOFT_STATE(dev);


	if (otyp >= OTYPCNT)
		return (ENXIO);

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"close of part %d type %d\n",
	    part, otyp);
	sema_p(&un->un_semoclose);

	mutex_enter(DCD_MUTEX);

	if (un->un_exclopen & (1<<part)) {
		un->un_exclopen &= ~(1<<part);
	}
	if (un->un_state == DCD_STATE_SUSPENDED) {
		mutex_exit(DCD_MUTEX);
		(void) ddi_dev_is_needed(DCD_DEVINFO, 0, 1);
		mutex_enter(DCD_MUTEX);
	}
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
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG, "last close\n");
		if (un->un_state == DCD_STATE_OFFLINE) {
			dcd_offline(un, 1);
		}
		_NOTE(NO_COMPETING_THREADS_NOW);
		mutex_exit(DCD_MUTEX);
		if (un->un_stats) {
			kstat_delete(un->un_stats);
			un->un_stats = 0;
		}
		for (i = 0; i < NDKMAP; i++) {
			if (un->un_pstats[i]) {
				kstat_delete(un->un_pstats[i]);
				un->un_pstats[i] = (kstat_t *)0;
			}
		}

		if (un->un_errstats) {
			kstat_delete(un->un_errstats);
			un->un_errstats = (kstat_t *)0;
		}
		mutex_enter(DCD_MUTEX);

		_NOTE(COMPETING_THREADS_NOW);
	}
	mutex_exit(DCD_MUTEX);
	sema_v(&un->un_semoclose);
	return (0);
}

static void
dcd_offline(struct dcd_disk *un, int bechatty)
{
	ASSERT(mutex_owned(DCD_MUTEX));

	if (bechatty)
		dcd_log(DCD_DEVINFO, dcd_label, CE_WARN, "offline\n");

	un->un_gvalid = 0;
}

/*
 * Given the device number return the devinfo pointer
 * from the scsi_device structure.
 */
/*ARGSUSED*/
static int
dcdinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register dev_t dev;
	register struct dcd_disk *un;
	register int instance, error;


	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = DCDUNIT(dev);
		if ((un = ddi_get_soft_state(dcd_state, instance)) == NULL)
			return (DDI_FAILURE);
		*result = (void *) DCD_DEVINFO;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		instance = DCDUNIT(dev);
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
dcd_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int nblocks, length, km_flags;
	caddr_t buffer;
	struct dcd_disk *un;
	int instance;

	if (dev != DDI_DEV_T_ANY)
		instance = DCDUNIT(dev);
	else
		instance = ddi_get_instance(dip);

	if ((un = ddi_get_soft_state(dcd_state, instance)) == NULL)
		return (DDI_PROP_NOT_FOUND);


	if (strcmp(name, "nblocks") == 0) {
		if (!un->un_gvalid) {
			return (DDI_PROP_NOT_FOUND);
		}
		mutex_enter(DCD_MUTEX);
		nblocks = (int)un->un_map[DCDPART(dev)].dkl_nblk;
		mutex_exit(DCD_MUTEX);

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

			buffer = (caddr_t)kmem_alloc((size_t)sizeof (int),
			km_flags);
			if (buffer == NULL)  {
				dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				    "no mem for property\n");
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer; /* Set callers buf ptr */
			break;

		case PROP_LEN_AND_VAL_BUF:

			if (sizeof (int) > (length))
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
void
dcduscsimin(struct buf *bp)
{

}


static void
dcdmin(struct buf *bp)
{
	register struct dcd_disk *un;
	register int instance;
	register minor_t minor = getminor(bp->b_edev);
	instance = minor >> DCDUNIT_SHIFT;
	un = ddi_get_soft_state(dcd_state, instance);

	if (bp->b_bcount > un->un_max_xfer_size)
		bp->b_bcount = un->un_max_xfer_size;
}


/* ARGSUSED2 */
static int
dcdread(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	register int secmask;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	secmask = un->un_secsize - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "file offset not modulo %d\n",
		    un->un_secsize);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "transfer length not modulo %d\n", un->un_secsize);
		return (EINVAL);
	}
	return (physio(dcdstrategy, (struct buf *)0, dev, B_READ, dcdmin, uio));
}

/* ARGSUSED2 */
static int
dcdaread(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	register int secmask;
	struct uio *uio = aio->aio_uio;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	secmask = un->un_secsize - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "file offset not modulo %d\n",
		    un->un_secsize);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "transfer length not modulo %d\n", un->un_secsize);
		return (EINVAL);
	}
	return (aphysio(dcdstrategy, anocancel, dev, B_READ, dcdmin, aio));
}

/* ARGSUSED2 */
static int
dcdwrite(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	register int secmask;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	secmask = un->un_secsize - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "file offset not modulo %d\n",
		    un->un_secsize);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "transfer length not modulo %d\n", un->un_secsize);
		return (EINVAL);
	}
	return (physio(dcdstrategy, (struct buf *)0, dev,
				B_WRITE, dcdmin, uio));
}

/* ARGSUSED2 */
static int
dcdawrite(dev_t dev, struct aio_req *aio, cred_t *cred_p)
{
	register int secmask;
	struct uio *uio = aio->aio_uio;
	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
#endif /* lint */
	secmask = un->un_secsize - 1;

	if (uio->uio_loffset & ((offset_t)(secmask))) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "file offset not modulo %d\n",
		    un->un_secsize);
		return (EINVAL);
	} else if (uio->uio_iov->iov_len & (secmask)) {
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "transfer length not modulo %d\n", un->un_secsize);
		return (EINVAL);
	}
	return (aphysio(dcdstrategy, anocancel, dev, B_WRITE, dcdmin, aio));
}

/*
 * strategy routine
 */
static int
dcdstrategy(struct buf *bp)
{
	register struct dcd_disk *un;
	register struct diskhd *dp;
	register i;
	register minor_t minor = getminor(bp->b_edev);
	int	 part = minor & DCDPART_MASK;
	struct dk_map32 *lp;
	register daddr32_t bn;

	if ((un = ddi_get_soft_state(dcd_state,
	    minor >> DCDUNIT_SHIFT)) == NULL ||
	    un->un_state == DCD_STATE_DUMPING ||
		((un->un_state  & DCD_STATE_FATAL) == DCD_STATE_FATAL)) {
		SET_BP_ERROR(bp, ((un) ? ENXIO : EIO));
error:
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		if (un)
			(void) pm_idle_component(DCD_DEVINFO, 0);
		return (0);
	}


	TRACE_2(TR_FAC_DADA, TR_DCDSTRATEGY_START,
	    "dcdstrategy_start: bp %x ncmds %d", bp, un->un_ncmds);

	/*
	 * mark busy so we won't get powered down again while
	 * trying to resume.
	 * Note that there must be one pm_idle_components() for each
	 * pm_busy_components().
	 */
	(void) pm_busy_component(DCD_DEVINFO, 0);



	if (un->un_state == DCD_STATE_SUSPENDED && bp != un->un_sbufp) {
		(void) ddi_dev_is_needed(DCD_DEVINFO, 0, 1);
	}

	bp->b_flags &= ~(B_DONE|B_ERROR);
	bp->b_resid = 0;
	bp->av_forw = 0;

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"bp->b_bcount %x\n", bp->b_bcount);

	if (bp != un->un_sbufp) {
		if (un->un_gvalid) {
validated:
			lp = &un->un_map[minor & DCDPART_MASK];
			bn = dkblock(bp);

			DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			    "dkblock(bp) is %d\n", bn);

			i = 0;
			if (bn < 0) {
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
			} else if (bp->b_bcount & (un->un_secsize-1)) {
				/*
				 * This should really be:
				 *
				 * ... if (bp->b_bcount & (un->un_lbasize-1))
				 *
				 */
				i = -1;
			} else {
				if (!bp->b_bcount) {
					printf("Waring : Zero read or Write\n");
					goto error;
				}
				/*
				 * sort by absolute block number.
				 */
				bp->b_resid = bn;
				bp->b_resid += un->un_offset[part];
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
		} else {
			/*
			 * opened in NDELAY/NONBLOCK mode?
			 * Check if disk is ready and has a valid geometry
			 */
			if (dcd_ready_and_valid(bp->b_edev, un) == 0) {
				goto validated;
			} else {
				dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				"i/o to invalid geometry\n");
				SET_BP_ERROR(bp, EIO);
				goto error;
			}
		}
	} else {
		struct udcd_cmd *tscmdp;
		struct dcd_cmd *tcmdp;
		/*
		 * This indicates that it is a special buffer
		 * This could be a udcd-cmd and hence call bp_mapin just
		 * in case that it could be a PIO command issued.
		 */
		tscmdp = (struct udcd_cmd *)bp->b_forw;
		tcmdp = tscmdp->udcd_cmd;
		if ((tcmdp->cmd != 0xc8) && (tcmdp->cmd != 0xc9) &&
			(tcmdp->cmd != 0xca) && (tcmdp->cmd != 0xcb) &&
			(tcmdp->cmd != 0xee)) {
			bp_mapin(bp);
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
	mutex_enter(DCD_MUTEX);

	DCD_DO_KSTATS(un, kstat_waitq_enter, bp);

	dp = &un->un_utab;

	if (dp->b_actf == NULL) {
		dp->b_actf = bp;
		dp->b_actl = bp;
	} else if ((un->un_state == DCD_STATE_SUSPENDED) &&
			bp == un->un_sbufp) {
		bp->b_actf = dp->b_actf;
		dp->b_actf = bp;
	} else {
		TRACE_3(TR_FAC_DADA, TR_DCDSTRATEGY_DISKSORT_START,
		"dcdstrategy_disksort_start: dp %x bp %x un_ncmds %d",
		    dp, bp, un->un_ncmds);
		disksort(dp, bp);
		TRACE_0(TR_FAC_DADA, TR_DCDSTRATEGY_DISKSORT_END,
		    "dcdstrategy_disksort_end");
	}

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"ncmd %x , throttle %x, forw %x\n",
		un->un_ncmds, un->un_throttle, dp->b_forw);
	ASSERT(un->un_ncmds >= 0);
	ASSERT(un->un_throttle >= 0);
	if ((un->un_ncmds < un->un_throttle) && (dp->b_forw == NULL)) {
		dcdstart(un);
	} else if (BP_HAS_NO_PKT(dp->b_actf)) {
		struct buf *cmd_bp;

		cmd_bp = dp->b_actf;
		cmd_bp->av_back = ALLOCATING_PKT;
		mutex_exit(DCD_MUTEX);
		/*
		 * try and map this one
		 */
		TRACE_0(TR_FAC_DADA, TR_DCDSTRATEGY_SMALL_WINDOW_START,
		    "dcdstrategy_small_window_call (begin)");

		make_dcd_cmd(un, cmd_bp, NULL_FUNC);

		TRACE_0(TR_FAC_DADA, TR_DCDSTRATEGY_SMALL_WINDOW_END,
		    "dcdstrategy_small_window_call (end)");

		/*
		 * there is a small window where the active cmd
		 * completes before make_sd_cmd returns.
		 * consequently, this cmd never gets started so
		 * we start it from here
		 */
		mutex_enter(DCD_MUTEX);
		if ((un->un_ncmds < un->un_throttle) &&
		    (dp->b_forw == NULL)) {
			dcdstart(un);
		}
	}
	mutex_exit(DCD_MUTEX);

done:
	TRACE_0(TR_FAC_DADA, TR_DCDSTRATEGY_END, "dcdstrategy_end");
	return (0);
}


/*
 * Unit start and Completion
 * NOTE: we assume that the caller has at least checked for:
 *		(un->un_ncmds < un->un_throttle)
 *	if not, there is no real harm done, scsi_transport() will
 *	return BUSY
 */
static void
dcdstart(struct dcd_disk *un)
{
	register int status, sort_key;
	register struct buf *bp;
	register struct diskhd *dp;
	register uchar_t state = un->un_last_state;

	TRACE_1(TR_FAC_DADA, TR_DCDSTART_START, "dcdstart_start: un %x", un);

retry:
	ASSERT(mutex_owned(DCD_MUTEX));

	dp = &un->un_utab;
	if (((bp = dp->b_actf) == NULL) ||
		(bp->av_back == ALLOCATING_PKT) ||
		(dp->b_forw != NULL)) {
		TRACE_0(TR_FAC_DADA, TR_DCDSTART_NO_WORK_END,
		    "dcdstart_end (no work)");
		return;
	}

	/*
	 * remove from active queue
	 */
	dp->b_actf = bp->b_actf;
	bp->b_actf = 0;

	if (un->un_state == DCD_STATE_SUSPENDED && (bp != un->un_sbufp)) {
		mutex_exit(DCD_MUTEX);
		(void) ddi_dev_is_needed(DCD_DEVINFO, 0, 1);
		mutex_enter(DCD_MUTEX);
	}

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
	DCD_DO_KSTATS(un, kstat_waitq_to_runq, bp);

	mutex_exit(DCD_MUTEX);

	if (BP_HAS_NO_PKT(bp)) {
		make_dcd_cmd(un, bp, dcdrunout);
		if (BP_HAS_NO_PKT(bp) && !(bp->b_flags & B_ERROR)) {
			mutex_enter(DCD_MUTEX);
			DCD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);

			bp->b_actf = dp->b_actf;
			dp->b_actf = bp;
			New_state(un, DCD_STATE_RWAIT);
			un->un_ncmds--;

			TRACE_0(TR_FAC_DADA, TR_DCDSTART_NO_RESOURCES_END,
				"dcdstart_end (No Resources)");
			goto done;

		} else if (bp->b_flags & B_ERROR) {
			mutex_enter(DCD_MUTEX);
			DCD_DO_KSTATS(un, kstat_runq_exit, bp);

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

			mutex_exit(DCD_MUTEX);

			biodone(bp);
			(void) pm_idle_component(DCD_DEVINFO, 0);
			mutex_enter(DCD_MUTEX);
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
	sort_key = bp->b_resid;
	bp->b_resid = BP_PKT(bp)->pkt_resid;
	BP_PKT(bp)->pkt_resid = 0;

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"bp->b_resid %x, pkt_resid %x\n",
		bp->b_resid, BP_PKT(bp)->pkt_resid);

	/*
	 * We used to check whether or not to try and link commands here.
	 * Since we have found that there is no performance improvement
	 * for linked commands, this has not made much sense.
	 */
	if ((status = dcd_transport((struct dcd_pkt *)BP_PKT(bp)))
			    != TRAN_ACCEPT) {
		mutex_enter(DCD_MUTEX);
		un->un_ncmds--;
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"transport returned %\n", status);
		if (status == TRAN_BUSY) {
			DCD_DO_ERRSTATS(un, dcd_transerrs);
			DCD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
			dcd_handle_tran_busy(bp, dp, un);
			if (un->un_ncmds > 0) {
				bp->b_resid = sort_key;
			}
		} else {
			DCD_DO_KSTATS(un, kstat_runq_exit, bp);
			mutex_exit(DCD_MUTEX);

			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    "transport rejected (%d)\n",
			    status);
			SET_BP_ERROR(bp, EIO);
			bp->b_resid = bp->b_bcount;
			if (bp != un->un_sbufp) {
				dcd_destroy_pkt(BP_PKT(bp));
			}
			biodone(bp);
			(void) pm_idle_component(DCD_DEVINFO, 0);

			mutex_enter(DCD_MUTEX);
			if ((un->un_ncmds < un->un_throttle) &&
				(dp->b_forw == NULL)) {
					goto retry;
			}
		}
	} else {
		mutex_enter(DCD_MUTEX);

		if (dp->b_actf && BP_HAS_NO_PKT(dp->b_actf)) {
			struct buf *cmd_bp;

			cmd_bp = dp->b_actf;
			cmd_bp->av_back = ALLOCATING_PKT;
			mutex_exit(DCD_MUTEX);
			/*
			 * try and map this one
			 */
			TRACE_0(TR_FAC_DADA, TR_DCASTART_SMALL_WINDOW_START,
			    "sdstart_small_window_start");

			make_dcd_cmd(un, cmd_bp, NULL_FUNC);

			TRACE_0(TR_FAC_DADA, TR_DCDSTART_SMALL_WINDOW_END,
			    "dcdstart_small_window_end");
			/*
			 * there is a small window where the active cmd
			 * completes before make_sd_cmd returns.
			 * consequently, this cmd never gets started so
			 * we start it from here
			 */
			mutex_enter(DCD_MUTEX);
			if ((un->un_ncmds < un->un_throttle) &&
			    (dp->b_forw == NULL)) {
				goto retry;
			}
		}
	}

done:
	ASSERT(mutex_owned(DCD_MUTEX));
	TRACE_0(TR_FAC_DADA, TR_DCDSTART_END, "dcdstart_end");
}

/*
 * make_dcd_cmd: create a pkt
 */
static void
make_dcd_cmd(struct dcd_disk *un, struct buf *bp, int (*func)())
{
	auto int count, com, direction;
	register struct dcd_pkt *pkt;
	register int flags, tval;

	_NOTE(DATA_READABLE_WITHOUT_LOCK(dcd_disk::un_dp))
	TRACE_3(TR_FAC_DADA, TR_MAKE_DCD_CMD_START,
	    "make_dcd_cmd_start: un %x bp %x un_ncmds %d",
	    un, bp, un->un_ncmds);


	flags = un->un_cmd_flags;

	if (bp != un->un_sbufp) {
		register int partition = DCDPART(bp->b_edev);
		struct dk_map32 *lp = &un->un_map[partition];
		long secnt;
		uint32_t blkno;
		int dkl_nblk, delta;
		long resid;

		dkl_nblk = lp->dkl_nblk;

		/*
		 * Make sure we don't run off the end of a partition.
		 *
		 * Put this test here so that we can adjust b_count
		 * to accurately reflect the actual amount we are
		 * goint to transfer.
		 */

		/*
		 * First, compute partition-relative block number
		 */
		blkno = dkblock(bp);
		secnt = (bp->b_bcount + (un->un_secsize - 1)) >> un->un_secdiv;
		count = MIN(secnt, dkl_nblk - blkno);
		if (count != secnt) {
			/*
			 * We have an overrun
			 */
			resid = (secnt - count) << un->un_secdiv;
			DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			    "overrun by %d sectors\n",
			    secnt - count);
			bp->b_bcount -= resid;
		} else {
			resid = 0;
		}

		/*
		 * Adjust block number to absolute
		 */
		delta = un->un_offset[partition];
		blkno += delta;

		mutex_enter(DCD_MUTEX);
		/*
		 * This is for devices having block size different from
		 * from DEV_BSIZE (e.g. 2K CDROMs).
		 */
		if (un->un_lbasize != un->un_secsize) {
			blkno >>= un->un_blknoshift;
			count >>= un->un_blknoshift;
		}
		mutex_exit(DCD_MUTEX);

		TRACE_0(TR_FAC_DADA, TR_MAKE_DCD_CMD_INIT_PKT_START,
		    "make_dcd_cmd_init_pkt_call (begin)");
		pkt = dcd_init_pkt(ROUTE, NULL, bp,
		    (uint32_t)sizeof (struct dcd_cmd),
		    un->un_cmd_stat_size, PP_LEN, PKT_CONSISTENT,
		    func, (caddr_t)un);
		TRACE_1(TR_FAC_DADA, TR_MAKE_DCD_CMD_INIT_PKT_END,
		    "make_dcd_cmd_init_pkt_call (end): pkt %x", pkt);
		if (!pkt) {
			bp->b_bcount += resid;
			bp->av_back = NO_PKT_ALLOCATED;
			TRACE_0(TR_FAC_DADA,
			    TR_MAKE_DCD_CMD_NO_PKT_ALLOCATED1_END,
			    "make_dcd_cmd_end (NO_PKT_ALLOCATED1)");
			return;
		}
		if (bp->b_flags & B_READ) {
			if ((un->un_dp->options & DMA_SUPPORTTED) ==
				DMA_SUPPORTTED) {
				com = ATA_READ_DMA;
			} else {
				if (un->un_dp->options & BLOCK_MODE)
					com = ATA_READ_MULTIPLE;
				else
					com = ATA_READ;
			}
			direction = DATA_READ;
		} else {
			if ((un->un_dp->options & DMA_SUPPORTTED) ==
				DMA_SUPPORTTED) {
				com = ATA_WRITE_DMA;
			} else {
				if (un->un_dp->options & BLOCK_MODE)
					com = ATA_WRITE_MULTIPLE;
				else
					com = ATA_WRITE;
			}
			direction = DATA_WRITE;
		}

		/*
		 * Save the resid in the packet, temporarily until
		 * we transport the command.
		 */
		pkt->pkt_resid = resid;

		makecommand(pkt, flags, com, blkno, ADD_LBA_MODE,
			bp->b_bcount, direction, 0);
		tval = dcd_io_time;
	} else {

		struct udcd_cmd *scmd =
				(struct udcd_cmd *)bp->b_forw;

		/*
		 * set options
		 */
		if ((scmd->udcd_flags & UDCD_SILENT) && !(DEBUGGING)) {
			flags |= FLAG_SILENT;
		}
		if (scmd->udcd_flags & UDCD_ISOLATE)
			flags |= FLAG_ISOLATE;
		if (scmd->udcd_flags &  UDCD_DIAGNOSE)
			flags |= FLAG_DIAGNOSE;

		if (scmd->udcd_flags & UDCD_NOINTR)
			flags |= FLAG_NOINTR;

		pkt = dcd_init_pkt(ROUTE, (struct dcd_pkt *)NULL,
			(bp->b_bcount)? bp: NULL,
			(uint32_t)sizeof (struct dcd_cmd),
			2, PP_LEN, PKT_CONSISTENT, func, (caddr_t)un);

		if (!pkt) {
			bp->av_back = NO_PKT_ALLOCATED;
			return;
		}

		makecommand(pkt, 0, scmd->udcd_cmd->cmd,
			scmd->udcd_cmd->sector_num.lba_num,
			scmd->udcd_cmd->address_mode,
			scmd->udcd_cmd->size,
			scmd->udcd_cmd->direction, scmd->udcd_cmd->features);
		if (scmd->udcd_timeout == 0)
			tval = dcd_io_time;
		else
			tval = scmd->udcd_timeout;
		/* UDAD interface should be decided. */
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			    "udcd interface\n");
	}

	pkt->pkt_comp = dcdintr;
	pkt->pkt_time = tval;
	PKT_SET_BP(pkt, bp);
	bp->av_back = (struct buf *)pkt;

	TRACE_0(TR_FAC_DADA, TR_MAKE_DCD_CMD_END, "make_dcd_cmd_end");
}

/*
 * Command completion processing
 */
static void
dcdintr(struct dcd_pkt *pkt)
{
	register struct dcd_disk *un;
	register struct buf *bp;
	register action;
	int status;

	bp = PKT_GET_BP(pkt);
	un = ddi_get_soft_state(dcd_state, DCDUNIT(bp->b_edev));

	TRACE_1(TR_FAC_DADA, TR_DCDINTR_START, "dcdintr_start: un %x", un);
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG, "dcdintr\n");

	mutex_enter(DCD_MUTEX);
	un->un_ncmds--;
	DCD_DO_KSTATS(un, kstat_runq_exit, bp);
	ASSERT(un->un_ncmds >= 0);

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"reason %x and Status %x\n", pkt->pkt_reason, SCBP_C(pkt));

	/*
	 * do most common case first
	 */
	if ((pkt->pkt_reason == CMD_CMPLT) &&
	    (SCBP_C(pkt) == 0)) {
		int com = GETATACMD((struct dcd_cmd *)pkt->pkt_cdbp);

		if (un->un_state == DCD_STATE_OFFLINE) {
			un->un_state = un->un_last_state;
			dcd_log(DCD_DEVINFO, dcd_label, CE_NOTE,
				(const char *) diskokay);
		}
		/*
		 * If the command is a read or a write, and we have
		 * a non-zero pkt_resid, that is an error. We should
		 * attempt to retry the operation if possible.
		 */
		action = COMMAND_DONE;
		if (pkt->pkt_resid && (com == ATA_READ || com == ATA_WRITE)) {
			DCD_DO_ERRSTATS(un, dcd_harderrs);
			if ((int)PKT_GET_RETRY_CNT(pkt) < dcd_retry_count) {
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
			dcd_log(DCD_DEVINFO, dcd_label,
			    CE_WARN, "incomplete %s- %s\n",
			    (bp->b_flags & B_READ)? "read" : "write",
			    (action == QUE_COMMAND)? "retrying" :
			    "giving up");
		}

		/*
		 * pkt_resid will reflect, at this point, a residual
		 * of how many bytes left to be transferred there were
		 * from the actual scsi command. Add this to b_resid i.e
		 * the amount this driver could not see to transfer,
		 * to get the total number of bytes not transfered.
		 */
		if (action != QUE_COMMAND) {
			bp->b_resid += pkt->pkt_resid;
		}

	} else if (pkt->pkt_reason != CMD_CMPLT) {
		action = dcd_handle_incomplete(un, bp);
	}

	/*
	 * If we are in the middle of syncing or dumping, we have got
	 * here because dcd_transport has called us explictly after
	 * completing the command in a polled mode. We don't want to
	 * have a recursive call into dcd_transport again.
	 */
	if (ddi_in_panic() && (action == QUE_COMMAND)) {
		action = COMMAND_DONE_ERROR;
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
		/*FALLTHROUGH*/
	case COMMAND_DONE:
		dcddone_and_mutex_exit(un, bp);

		TRACE_0(TR_FAC_DADA, TR_DCDINTR_COMMAND_DONE_END,
		    "dcdintr_end (COMMAND_DONE)");
		return;

	case QUE_COMMAND:
		if (un->un_ncmds >= un->un_throttle) {
			register struct diskhd *dp = &un->un_utab;

			bp->b_actf = dp->b_actf;
			dp->b_actf = bp;

			DCD_DO_KSTATS(un, kstat_waitq_enter, bp);

			mutex_exit(DCD_MUTEX);
			goto exit;
		}

		un->un_ncmds++;
		/* reset the pkt reason again */
		pkt->pkt_reason = 0;
		DCD_DO_KSTATS(un, kstat_runq_enter, bp);
		mutex_exit(DCD_MUTEX);
		if ((status = dcd_transport(BP_PKT(bp))) != TRAN_ACCEPT) {
			register struct diskhd *dp = &un->un_utab;

			mutex_enter(DCD_MUTEX);
			un->un_ncmds--;
			if (status == TRAN_BUSY) {
				DCD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
				dcd_handle_tran_busy(bp, dp, un);
				mutex_exit(DCD_MUTEX);
				goto exit;
			}
			DCD_DO_ERRSTATS(un, dcd_transerrs);
			DCD_DO_KSTATS(un, kstat_runq_exit, bp);

			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    "requeue of command fails (%x)\n", status);
			SET_BP_ERROR(bp, EIO);
			bp->b_resid = bp->b_bcount;

			dcddone_and_mutex_exit(un, bp);
			goto exit;
		}
		break;

	case JUST_RETURN:
	default:
		DCD_DO_KSTATS(un, kstat_waitq_enter, bp);
		mutex_exit(DCD_MUTEX);
		break;
	}

exit:
	TRACE_0(TR_FAC_DADA, TR_DCDINTR_END, "dcdintr_end");
}


/*
 * Done with a command.
 */
static void
dcddone_and_mutex_exit(struct dcd_disk *un, register struct buf *bp)
{
	register struct diskhd *dp;

	TRACE_1(TR_FAC_DADA, TR_DCDONE_START, "dcddone_start: un %x", un);

	_NOTE(LOCK_RELEASED_AS_SIDE_EFFECT(&un->un_dcd->dcd_mutex));

	dp = &un->un_utab;
	if (bp == dp->b_forw) {
		dp->b_forw = NULL;
	}

	if (un->un_stats) {
		ulong_t n_done = bp->b_bcount - bp->b_resid;
		if (bp->b_flags & B_READ) {
			IOSP->reads++;
			IOSP->nread += n_done;
		} else {
			IOSP->writes++;
			IOSP->nwritten += n_done;
		}
	}
	if (IO_PARTITION_STATS) {
		ulong_t n_done = bp->b_bcount - bp->b_resid;
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
	 */
	if (dp->b_actf && (un->un_ncmds < un->un_throttle) &&
	    (dp->b_forw == NULL && un->un_state != DCD_STATE_SUSPENDED)) {
		dcdstart(un);
	}

	mutex_exit(DCD_MUTEX);

	if (bp != un->un_sbufp) {
		dcd_destroy_pkt(BP_PKT(bp));
		DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		    "regular done: resid %d\n", bp->b_resid);
	} else {
		ASSERT(un->un_sbuf_busy);
	}
	TRACE_0(TR_FAC_DADA, TR_DCDDONE_BIODONE_CALL, "dcddone_biodone_call");

	biodone(bp);

	(void) pm_idle_component(DCD_DEVINFO, 0);

	TRACE_0(TR_FAC_DADA, TR_DCDDONE_END, "dcddone end");
}


/*
 * reset the disk unless the transport layer has already
 * cleared the problem
 */
#define	C1	(STAT_ATA_BUS_RESET|STAT_ATA_DEV_RESET|STAT_ATA_ABORTED)
static void
dcd_reset_disk(struct dcd_disk *un, struct dcd_pkt *pkt)
{

	if ((pkt->pkt_statistics & C1) == 0) {
		mutex_exit(DCD_MUTEX);
		if (!dcd_reset(ROUTE, RESET_ALL)) {
				DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
					"Reset failed");
		}
		mutex_enter(DCD_MUTEX);
	}
}

static int
dcd_handle_incomplete(struct dcd_disk *un, struct buf *bp)
{
	static char *fail = "ATA transport failed: reason '%s': %s\n";
	static char *notresp = "disk not responding to selection\n";
	register rval = COMMAND_DONE_ERROR;
	register action = COMMAND_SOFT_ERROR;
	register struct dcd_pkt *pkt = BP_PKT(bp);
	int be_chatty = (un->un_state != DCD_STATE_SUSPENDED) &&
	    (bp != un->un_sbufp || !(pkt->pkt_flags & FLAG_SILENT));

	ASSERT(mutex_owned(DCD_MUTEX));

	switch (pkt->pkt_reason) {

	case CMD_TIMEOUT:
		/*
		 * This Indicates the already the HBA would  have reset
		 * so Just indicate to retry the command
		 */
		break;

	case CMD_INCOMPLETE:
		action = dcd_check_error(un, bp);
		DCD_DO_ERRSTATS(un, dcd_transerrs);
		(void) dcd_reset_disk(un, pkt);
		break;

	case CMD_FATAL:
		/*
		 * Something drastic has gone wrong
		 */
		break;
	case CMD_DMA_DERR:
	case CMD_DATA_OVR:
		/* FALLTHROUGH */

	default:
		/*
		 * the target may still be running the	command,
		 * so we should try and reset that target.
		 */
		DCD_DO_ERRSTATS(un, dcd_transerrs);
		if ((pkt->pkt_reason != CMD_RESET) &&
			(pkt->pkt_reason != CMD_ABORTED)) {
			(void) dcd_reset_disk(un, pkt);
		}
		break;
	}

	/*
	* If pkt_reason is CMD_RESET/ABORTED, chances are that this pkt got
	* reset/aborted because another disk on this bus caused it.
	* The disk that caused it, should get CMD_TIMEOUT with pkt_statistics
	* of STAT_TIMEOUT/STAT_DEV_RESET
	*/
	if ((pkt->pkt_reason == CMD_RESET) ||(pkt->pkt_reason == CMD_ABORTED)) {
		/* To be written : XXX */
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
				    "Command aborted\n");

	}

	if (bp == un->un_sbufp && (pkt->pkt_flags & FLAG_ISOLATE)) {
		rval = COMMAND_DONE_ERROR;
	} else {
		if ((rval == COMMAND_DONE_ERROR) &&
			(action == COMMAND_SOFT_ERROR) &&
			((int)PKT_GET_RETRY_CNT(pkt) < dcd_retry_count)) {
			PKT_INCR_RETRY_CNT(pkt, 1);
			rval = QUE_COMMAND;
		}
	}

	if (pkt->pkt_reason == CMD_INCOMPLETE && rval == COMMAND_DONE_ERROR) {
		/*
		 * Looks like someone turned off this shoebox.
		 */
		if (un->un_state != DCD_STATE_OFFLINE) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			(const char *) notresp);
			New_state(un, DCD_STATE_OFFLINE);
		}
	} else if (pkt->pkt_reason == CMD_FATAL) {
		/*
		 * Suppressing the following message for the time being
		 * dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
		 * (const char *) notresp);
		 */
		PKT_INCR_RETRY_CNT(pkt, 6);
		rval = COMMAND_DONE_ERROR;
		un->un_state |= DCD_STATE_FATAL;
	} else if (be_chatty) {
		int in_panic = ddi_in_panic();
		if (!in_panic || (rval == COMMAND_DONE_ERROR)) {
			if (((pkt->pkt_reason != un->un_last_pkt_reason) &&
			    (pkt->pkt_reason != CMD_RESET)) ||
			    (rval == COMMAND_DONE_ERROR) ||
			    (dcd_error_level == DCD_ERR_ALL)) {
				dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				    fail, dcd_rname(pkt->pkt_reason),
				    (rval == COMMAND_DONE_ERROR) ?
				    "giving up": "retrying command");
				DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
				    "retrycount=%x\n",
				    PKT_GET_RETRY_CNT(pkt));
			}
		}
	}
error:
	return (rval);
}

static int
dcd_check_error(struct dcd_disk *un, struct buf *bp)
{
	struct diskhd *dp = &un->un_utab;
	register struct dcd_pkt *pkt = BP_PKT(bp);
	register rval = 0;
	unsigned char status;
	unsigned char error;

	TRACE_0(TR_FAC_DADA, TR_DCD_CHECK_ERROR_START, "dcd_check_error_start");
	ASSERT(mutex_owned(DCD_MUTEX));

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"Pkt: 0x%x dp: 0x%x\n", pkt, dp);

	/*
	 * Here we need to check status first and then if error is indicated
	 * Then the error register.
	 */

	status = (pkt->pkt_scbp)[0];
	if ((status & STATUS_ATA_DWF) == STATUS_ATA_DWF) {
		/*
		 * There has been a Device Fault  - reason for such error
		 * is vendor specific
		 * Action to be taken is - Indicate error and reset device.
		 */

		dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			"Device Fault\n");
		rval = COMMAND_HARD_ERROR;
	} else if ((status & STATUS_ATA_CORR) == STATUS_ATA_CORR) {

		/*
		 * The sector read or written is marginal and hence ECC
		 * Correction has been applied. Indicate to repair
		 * Here we need to probably re-assign based on the badblock
		 * mapping.
		 */

		dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			"Soft Error on block %x\n",
			((struct dcd_cmd *)pkt->pkt_cdbp)->sector_num.lba_num);
		rval = COMMAND_SOFT_ERROR;
	} else if ((status & STATUS_ATA_ERR) == STATUS_ATA_ERR) {
		error = pkt->pkt_scbp[1];
		if ((error &  ERR_AMNF) == ERR_AMNF) {
			/* Address make not found */
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				"Address Mark Not Found");
		} else if ((error & ERR_TKONF) == ERR_TKONF) {
			/* Track 0 Not found */
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				"Track 0 Not found \n");
		} else if ((error & ERR_ABORT) == ERR_ABORT) {
			/* Aborted Command */
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				" Aborted Command \n");
		} else if ((error & ERR_IDNF) == ERR_IDNF) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				" ID not found \n");
		} else if ((error &  ERR_UNC) == ERR_UNC) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				"Uncorrectable data Error: Block %x\n",
		((struct dcd_cmd *)pkt->pkt_cdbp)->sector_num.lba_num);
		} else if ((error & ERR_BBK) == ERR_BBK) {
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
				"Bad block detected: Block %x\n",
			((struct dcd_cmd *)pkt->pkt_cdbp)->sector_num.lba_num);
		}
		rval = COMMAND_HARD_ERROR;
	}

	TRACE_0(TR_FAC_DADA, TR_DCD_CHECK_ERROR_END, "dcd_check_error_end");
	return (rval);
}


/*
 *	System Crash Dump routine
 */

#define	NDUMP_RETRIES	5

static int
dcddump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk)
{
	struct dk_map32 *lp;
	struct dcd_pkt *pkt;
	register i;
	struct buf local, *bp;
	int err;
	unsigned char com;
	GET_SOFT_STATE(dev);

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*un))

	if ((!un->un_gvalid) ||
			((un->un_state & DCD_STATE_FATAL) == DCD_STATE_FATAL))
		return (ENXIO);


	lp = &un->un_map[part];
	if (blkno+nblk > lp->dkl_nblk) {
		return (EINVAL);
	}


	/*
	 * When cpr calls sddump, we know that sd is in a
	 * a good state, so no bus reset is required
	 */
	un->un_throttle = 0;

	if ((un->un_state != DCD_STATE_SUSPENDED) &&
		(un->un_state != DCD_STATE_DUMPING)) {

		New_state(un, DCD_STATE_DUMPING);

		/*
		 * Reset the bus. I'd like to not have to do this,
		 * but this is the safest thing to do...
		 */

		if (dcd_reset(ROUTE, RESET_ALL) == 0) {
			return (EIO);
		}

	}

	blkno += (lp->dkl_cylno * un->un_g.dkg_nhead * un->un_g.dkg_nsect);

	/*
	 * It should be safe to call the allocator here without
	 * worrying about being locked for DVMA mapping because
	 * the address we're passed is already a DVMA mapping
	 *
	 * We are also not going to worry about semaphore ownership
	 * in the dump buffer. Dumping is single threaded at present.
	 */

	bp = &local;
	bzero((caddr_t)bp, sizeof (*bp));
	bp->b_flags = B_BUSY;
	bp->b_un.b_addr = addr;
	bp->b_bcount = nblk << DEV_BSHIFT;
	bp->b_resid = 0;

	for (i = 0; i < NDUMP_RETRIES; i++) {
		bp->b_flags &= ~B_ERROR;
		if ((pkt = dcd_init_pkt(ROUTE, NULL, bp,
			(uint32_t)sizeof (struct dcd_cmd), 2, PP_LEN,
				PKT_CONSISTENT, NULL_FUNC, NULL)) != NULL) {
			break;
		}
		if (i == 0) {
			if (bp->b_flags & B_ERROR) {
				dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
					"no resources for dumping; "
					"error code: 0x%x, retrying",
					geterror(bp));
			} else {
				dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
					"no resources for dumping; retrying");
			}
		} else if (i != (NDUMP_RETRIES - 1)) {
			if (bp->b_flags & B_ERROR) {
				dcd_log(DCD_DEVINFO, dcd_label, CE_CONT,
				"no resources for dumping; error code: 0x%x, "
				"retrying\n", geterror(bp));
			}
		} else {
			if (bp->b_flags & B_ERROR) {
				dcd_log(DCD_DEVINFO, dcd_label, CE_CONT,
					"no resources for dumping; "
					"error code: 0x%x, retries failed, "
					"giving up.\n", geterror(bp));
			} else {
				dcd_log(DCD_DEVINFO, dcd_label, CE_CONT,
					"no resources for dumping; "
					"retries failed, giving up.\n");
			}
			return (EIO);
		}
		delay(10);
	}
	if ((un->un_dp->options & DMA_SUPPORTTED) == DMA_SUPPORTTED) {
		com = ATA_WRITE_DMA;
	} else {
		if (un->un_dp->options & BLOCK_MODE)
			com = ATA_WRITE_MULTIPLE;
		else
			com = ATA_WRITE;
	}

	makecommand(pkt, 0, com, blkno, ADD_LBA_MODE,
		    (int)nblk*un->un_secsize, DATA_WRITE, 0);

	for (err = EIO, i = 0; i < NDUMP_RETRIES && err == EIO; i++) {

		if (dcd_poll(pkt) == 0) {
			switch (SCBP_C(pkt)) {
			case STATUS_GOOD:
				if (pkt->pkt_resid == 0) {
					err = 0;
				}
				break;
			case STATUS_ATA_BUSY:
				(void) dcd_reset(ROUTE, RESET_TARGET);
				break;
			default:
				mutex_enter(DCD_MUTEX);
				(void) dcd_reset_disk(un, pkt);
				mutex_exit(DCD_MUTEX);
				break;
			}
		} else if (i > NDUMP_RETRIES/2) {
			(void) dcd_reset(ROUTE, RESET_ALL);
		}

	}
	dcd_destroy_pkt(pkt);
	return (err);
}

/*
 * This routine implements the ioctl calls.  It is called
 * from the device switch at normal priority.
 */
/* ARGSUSED3 */
static int
dcdioctl(dev_t dev, int cmd, intptr_t arg, int flag,
	cred_t *cred_p, int *rval_p)
{
	auto int32_t data[512 / (sizeof (int32_t))];
	struct dk_cinfo *info;
	struct dk_minfo media_info;
	struct vtoc vtoc;
	struct udcd_cmd *scmd;
	int i, err;
	ushort_t write_reinstruct;
	enum uio_seg uioseg = 0;
	enum dkio_state state = 0;
#ifdef _MULTI_DATAMODEL
	struct dadkio_rwcmd rwcmd;
#endif
	struct dadkio_rwcmd32 rwcmd32;
	struct dcd_cmd dcdcmd;

	GET_SOFT_STATE(dev);
#ifdef lint
	part = part;
	state = state;
	uioseg = uioseg;
#endif  /* lint */

	if (un->un_state == DCD_STATE_SUSPENDED)
		(void) ddi_dev_is_needed(DCD_DEVINFO, 0, 1);

	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"dcd_ioctl : cmd %x, arg %x\n", cmd, arg);

	bzero((caddr_t)data, sizeof (data));

	switch (cmd) {

#ifdef DCDDEBUG
/*
 * Following ioctl are for testing RESET/ABORTS
 */
#define	DKIOCRESET	(DKIOC|14)
#define	DKIOCABORT	(DKIOC|15)

	case DKIOCRESET:
		if (ddi_copyin((caddr_t)arg, (caddr_t)data, 4, flag))
			return (EFAULT);
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"DKIOCRESET: data = 0x%x\n", data[0]);
		if (dcd_reset(ROUTE, data[0])) {
			return (0);
		} else {
			return (EIO);
		}
	case DKIOCABORT:
		DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG,
			"DKIOCABORT:\n");
		if (dcd_abort(ROUTE, (struct dcd_pkt *)0)) {
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
		mutex_enter(DCD_MUTEX);
		switch (un->un_dp->ctype) {
		default:
			info->dki_ctype = DKC_DIRECT;
			break;
		}
		mutex_exit(DCD_MUTEX);
		info->dki_cnum = ddi_get_instance(ddi_get_parent(DCD_DEVINFO));
		(void) strcpy(info->dki_cname,
		    ddi_get_name(ddi_get_parent(DCD_DEVINFO)));
		/*
		 * Unit Information
		 */
		info->dki_unit = ddi_get_instance(DCD_DEVINFO);
		info->dki_slave = (Tgt(DCD_DCD_DEVP)<<3);
		(void) strcpy(info->dki_dname, ddi_get_name(DCD_DEVINFO));
		info->dki_flags = DKI_FMTVOL;
		info->dki_partition = DCDPART(dev);

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
		if (ddi_copyout((caddr_t)data, (caddr_t)arg, i, flag))
			return (EFAULT);
		else
			return (0);

	case DKIOCGMEDIAINFO:
		/*
		 * As dad target driver is used for IDE disks only
		 * Can keep the return value hardcoded to FIXED_DISK
		 */
		media_info.dki_media_type = DK_FIXED_DISK;

		mutex_enter(DCD_MUTEX);
		media_info.dki_lbsize = un->un_lbasize;
		media_info.dki_capacity = un->un_capacity;
		mutex_exit(DCD_MUTEX);

		if (ddi_copyout(&media_info, (caddr_t)arg,
			sizeof (struct dk_minfo), flag))
			return (EFAULT);
		else
			return (0);

	case DKIOCGGEOM:
	/*
	 * Return the geometry of the specified unit.
	 */
		mutex_enter(DCD_MUTEX);
		if (un->un_ncmds == 0) {
			if ((err = dcd_unit_ready(dev)) != 0) {
				mutex_exit(DCD_MUTEX);
				return (err);
			}
		}

		if (dcd_validate_geometry(un, SLEEP_FUNC) != 0) {
			/* Fake the geometry and return success */
			dcd_fake_geometry(un);
		}
		write_reinstruct = un->un_g.dkg_write_reinstruct;

		i = sizeof (struct dk_geom);

		if (un->un_g.dkg_write_reinstruct == 0)
			un->un_g.dkg_write_reinstruct =
			    (int)((int)(un->un_g.dkg_nsect * un->un_g.dkg_rpm *
			    dcd_rot_delay) / (int)60000);

		mutex_exit(DCD_MUTEX);
		/*
		 * This could page fault and could cause a call to our
		 * strategy and which would wait on the DCD_MUTEX thereby
		 * causing the page fault.
		 * Hence moved the ddi_copyout out of the mutex.
		 */
		err = ddi_copyout((caddr_t)&un->un_g, (caddr_t)arg, i, flag);

		mutex_enter(DCD_MUTEX);
		un->un_g.dkg_write_reinstruct = write_reinstruct;

		mutex_exit(DCD_MUTEX);


		if (err)
			return (EFAULT);
		else
			return (0);

	case DKIOCSGEOM:
	/*
	 * Set the geometry of the specified unit.
	 */

		i = sizeof (struct dk_geom);
		if (ddi_copyin((caddr_t)arg, (caddr_t)data, i, flag))
			return (EFAULT);
		mutex_enter(DCD_MUTEX);
		bcopy((caddr_t)data, (caddr_t)&un->un_g, i);
		for (i = 0; i < NDKMAP; i++) {
			struct dk_map32 *lp  = &un->un_map[i];
			un->un_offset[i] =
			    un->un_g.dkg_nhead *
			    un->un_g.dkg_nsect *
			    lp->dkl_cylno;
		}
		mutex_exit(DCD_MUTEX);
		return (0);

	case DKIOCGAPART:
	/*
	 * Return the map for all logical partitions.
	 */

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_NONE: {
			struct dk_map dk_map[NDKMAP];

			for (i = 0; i < NDKMAP; i++) {
				dk_map[i].dkl_cylno = (daddr_t)
							un->un_map[i].dkl_cylno;
				dk_map[i].dkl_nblk = (daddr_t)
							un->un_map[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyout(dk_map, (caddr_t)arg, i, flag))
				return (EFAULT);
			return (0);

		}

		case DDI_MODEL_ILP32:
			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyout((caddr_t)un->un_map,
					(caddr_t)arg, i, flag))
				return (EFAULT);
			else
				return (0);

		}
#else
		i = NDKMAP * sizeof (struct dk_map32);
		if (ddi_copyout(un->un_map, (caddr_t)arg, i, flag))
			return (EFAULT);
		else
			return (0);
#endif
	case DKIOCSAPART:
#ifdef _MULTI_DATAMODEL
	/*
	 * Set the map for all logical partitions.  We lock
	 * the priority just to make sure an interrupt doesn't
	 * come in while the map is half updated.
	 */
		{
		struct dk_map dk_map[NDKMAP];
		struct dk_map32 dk_map32[NDKMAP];
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_NONE: {

			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyin((caddr_t)arg, dk_map, i, flag))
				return (EFAULT);
			for (i = 0; i < NDKMAP; i++) {
				dk_map32[i].dkl_cylno =
					(uint32_t)dk_map[i].dkl_cylno;
				dk_map32[i].dkl_nblk =
					(uint32_t)dk_map[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map32);
			break;
		}

		case DDI_MODEL_ILP32:
			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyin((caddr_t)arg, dk_map32, i, flag))
				return (EFAULT);
			break;
		}
		mutex_enter(DCD_MUTEX);
		bcopy((caddr_t)dk_map32, (caddr_t)un->un_map, i);
#else /* ! _MULTI_DATAMODEL */
		i = NDKMAP * sizeof (struct dk_map32);
		if (ddi_copyin((caddr_t)arg, (caddr_t)data, i, flag))
			return (EFAULT);

		mutex_enter(DCD_MUTEX);
		bcopy((caddr_t)data, (caddr_t)un->un_map, i);
#endif /*  _MULTI_DATAMODEL */
		for (i = 0; i < NDKMAP; i++) {
			struct dk_map32 *lp  = &un->un_map[i];
			un->un_offset[i] =
			    un->un_g.dkg_nhead *
			    un->un_g.dkg_nsect *
			    lp->dkl_cylno;
		}
		mutex_exit(DCD_MUTEX);
		return (0);
#ifdef _MULTI_DATAMODEL
		}
#endif
	case DKIOCGVTOC:
		/*
		 * Get the label (vtoc, geometry and partition map) directly
		 * from the disk, in case if it got modified by another host
		 * sharing the disk in a multi initiator configuration.
		 */
		mutex_enter(DCD_MUTEX);
		if (un->un_ncmds == 0) {
			if ((err = dcd_unit_ready(dev)) != 0) {
				mutex_exit(DCD_MUTEX);
				return (err);
			}
		}
		if (dcd_validate_geometry(un, SLEEP_FUNC) != 0) {
			dcd_fake_geometry(un);
		}
		dcd_build_user_vtoc(un, &vtoc);
		mutex_exit(DCD_MUTEX);
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
#else
		i = sizeof (struct vtoc);
		if (ddi_copyout((caddr_t)&vtoc, (caddr_t)arg, i, flag))
			return (EFAULT);
		else
#endif
			return (0);

	case DKIOCSVTOC:
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

		mutex_enter(DCD_MUTEX);
		if (un->un_g.dkg_ncyl == 0) {
			mutex_exit(DCD_MUTEX);
			printf("Ncyl is zero\n");
			return (EINVAL);
		}
		if ((i = dcd_build_label_vtoc(un, &vtoc)) == 0) {
			if ((i = dcd_write_label(dev)) == 0) {
				(void) dcd_validate_geometry(un, SLEEP_FUNC);
			}
		}
		if (un->un_devid == NULL) {
			/* Get fab'd devid */
			if (dcd_get_devid(un) == NULL)
				/* create fab'd devid */
				(void) dcd_create_devid(un);
		}
		mutex_exit(DCD_MUTEX);
		return (i);

	case DIOCTL_RWCMD:
		if (drv_priv(cred_p) != 0) {
			return (EPERM);
		}

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_NONE:
			if (ddi_copyin((caddr_t)arg, (caddr_t)&rwcmd,
				sizeof (struct dadkio_rwcmd), flag)) {
				return (EFAULT);
			}
			rwcmd32.cmd = rwcmd.cmd;
			rwcmd32.flags = rwcmd.flags;
			rwcmd32.blkaddr = rwcmd.blkaddr;
			rwcmd32.buflen = rwcmd.buflen;
			rwcmd32.bufaddr = (caddr32_t)rwcmd.bufaddr;
			break;
		case DDI_MODEL_ILP32:
			if (ddi_copyin((caddr_t)arg, (caddr_t)&rwcmd32,
				sizeof (struct dadkio_rwcmd32), flag)) {
				return (EFAULT);
			}
			break;
		}
#else
		if (ddi_copyin((caddr_t)arg, (caddr_t)&rwcmd32,
			sizeof (struct dadkio_rwcmd32), flag)) {
			return (EFAULT);
		}
#endif
		mutex_enter(DCD_MUTEX);

		uioseg  = UIO_SYSSPACE;
		scmd = (struct udcd_cmd *)data;
		scmd->udcd_cmd = &dcdcmd;
		/*
		 * Convert the dadkio_rwcmd structure to udcd_cmd so that
		 * it can take the normal path to get the io done
		 */
		if (rwcmd32.cmd == DADKIO_RWCMD_READ) {
			if ((un->un_dp->options & DMA_SUPPORTTED) ==
				DMA_SUPPORTTED)
				scmd->udcd_cmd->cmd = ATA_READ_DMA;
			else
				scmd->udcd_cmd->cmd = ATA_READ;
			scmd->udcd_cmd->address_mode = ADD_LBA_MODE;
			scmd->udcd_cmd->direction = DATA_READ;
			scmd->udcd_flags |= UDCD_READ|UDCD_SILENT;
		} else if (rwcmd32.cmd == DADKIO_RWCMD_WRITE) {
			if ((un->un_dp->options & DMA_SUPPORTTED) ==
					DMA_SUPPORTTED)
				scmd->udcd_cmd->cmd = ATA_WRITE_DMA;
			else
				scmd->udcd_cmd->cmd = ATA_WRITE;
			scmd->udcd_cmd->direction = DATA_WRITE;
			scmd->udcd_flags |= UDCD_WRITE|UDCD_SILENT;
		} else {
			mutex_exit(DCD_MUTEX);
			return (EINVAL);
		}

		scmd->udcd_cmd->address_mode = ADD_LBA_MODE;
		scmd->udcd_cmd->features = 0;
		scmd->udcd_cmd->size = rwcmd32.buflen;
		scmd->udcd_cmd->sector_num.lba_num = rwcmd32.blkaddr;
		scmd->udcd_bufaddr = (caddr_t)rwcmd32.bufaddr;
		scmd->udcd_buflen = rwcmd32.buflen;
		scmd->udcd_timeout = (ushort_t)dcd_io_time;
		scmd->udcd_resid = 0ULL;
		scmd->udcd_status = 0;
		scmd->udcd_error_reg = 0;
		scmd->udcd_status_reg = 0;

		mutex_exit(DCD_MUTEX);

		i = dcdioctl_cmd(dev, scmd, UIO_SYSSPACE, UIO_USERSPACE);
		mutex_enter(DCD_MUTEX);
		/*
		 * After return convert the status from scmd to
		 * dadkio_status
		 */
		(void) dcd_translate(&(rwcmd32.status), scmd);
		rwcmd32.status.resid = scmd->udcd_resid;
		mutex_exit(DCD_MUTEX);

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_NONE:
			{
			int counter;
			rwcmd.status.status = rwcmd32.status.status;
			rwcmd.status.resid  = rwcmd32.status.resid;
			rwcmd.status.failed_blk_is_valid =
				rwcmd32.status.failed_blk_is_valid;
			rwcmd.status.failed_blk = rwcmd32.status.failed_blk;
			rwcmd.status.fru_code_is_valid =
				rwcmd32.status.fru_code_is_valid;
			rwcmd.status.fru_code =
				rwcmd32.status.fru_code;
			for (counter = 0;
				counter < DADKIO_ERROR_INFO_LEN; counter++)
				rwcmd.status.add_error_info[counter] =
				rwcmd32.status.add_error_info[counter];
			}
			/* Copy out the result back to the user program */
			if (ddi_copyout((caddr_t)&rwcmd, (caddr_t)arg,
				sizeof (struct dadkio_rwcmd), flag)) {
				if (i != 0) {
					i = EFAULT;
				}
			}
			break;
		case DDI_MODEL_ILP32:
			/* Copy out the result back to the user program */
			if (ddi_copyout((caddr_t)&rwcmd32, (caddr_t)arg,
				sizeof (struct dadkio_rwcmd32), flag)) {
				if (i != 0) {
					i = EFAULT;
				}
			}
			break;
		}
#else
		/* Copy out the result back to the user program  */
		if (ddi_copyout((caddr_t)&rwcmd32, (caddr_t)arg,
			sizeof (struct dadkio_rwcmd32), flag)) {
			if (i != 0)
				i = EFAULT;
		}
#endif
		return (i);

	case UDCDCMD:
		if (drv_priv(cred_p) != 0) {
			return (EPERM);
		}

		scmd = (struct udcd_cmd *)data;
		if (ddi_copyin((caddr_t)arg, (caddr_t)scmd,
				    sizeof (*scmd), flag)) {
			return (EFAULT);
		}

		uioseg = (flag & FKIOCTL)? UIO_SYSSPACE: UIO_USERSPACE;

		i = dcdioctl_cmd(dev, scmd, uioseg, uioseg);
		if (ddi_copyout((caddr_t)scmd, (caddr_t)arg,
				sizeof (*scmd), flag)) {
			if (i != 0)
				i = EFAULT;
		}

		return (i);

	default:
		break;
	}
	return (ENOTTY);
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
dcdrunout(caddr_t arg)
{
	register int serviced;
	register struct dcd_disk *un;
	register struct diskhd *dp;

	TRACE_1(TR_FAC_DADA, TR_DCDRUNOUT_START, "dcdrunout_start: arg %x",
			    arg);
	serviced = 1;

	un = (struct dcd_disk *)arg;
	dp = &un->un_utab;

	/*
	 * We now support passing a structure to the callback
	 * routine.
	 */
	ASSERT(un != NULL);
	mutex_enter(DCD_MUTEX);
	if ((un->un_ncmds < un->un_throttle) && (dp->b_forw == NULL)) {
		dcdstart(un);
	}
	if (un->un_state == DCD_STATE_RWAIT) {
		serviced = 0;
	}
	mutex_exit(DCD_MUTEX);
	TRACE_1(TR_FAC_DADA, TR_DCDRUNOUT_END,
	    "dcdrunout_end: serviced %d", serviced);
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

/* ARGSUSED0 */
static int
dcd_unit_ready(dev_t dev)
{
#ifndef lint
	auto struct udcd_cmd dcmd, *com = &dcmd;
	auto struct dcd_cmd cmdblk;
#endif
	int error;
#ifndef lint
	GET_SOFT_STATE(dev);
#endif

	/*
	 * Now that we protect the special buffer with
	 * a mutex, we could probably do a mutex_tryenter
	 * on it here and return failure if it were held...
	 */

	error = 0;
	return (error);
}

/* ARGSUSED0 */
int
dcdioctl_cmd(dev_t devp, struct udcd_cmd *in, enum uio_seg cdbspace,
	enum uio_seg dataspace)
{

	register struct buf *bp;
	struct	udcd_cmd *scmd;
	struct dcd_pkt *pkt;
	int	err, rw;
	caddr_t	cdb;
	int	flags = 0;

	GET_SOFT_STATE(devp);

#ifdef lint
	part = part;
#endif

	/*
	 * Is this a request to reset the bus?
	 * if so, we need to do reseting.
	 */

	if (in->udcd_flags & UDCD_RESET) {
		int flag = RESET_TARGET;
		err = dcd_reset(ROUTE, flag) ? 0: EIO;
		return (err);
	}

	scmd = in;


	/* Do some sanity checks */
	if (scmd->udcd_buflen <= 0) {
		if (scmd->udcd_flags & (UDCD_READ | UDCD_WRITE)) {
			return (EINVAL);
		} else {
			scmd->udcd_buflen = 0;
		}

	}

	/* Make a copy of the dcd_cmd passed  */
	cdb = kmem_zalloc(sizeof (struct dcd_cmd), KM_SLEEP);
	if (cdbspace == UIO_SYSSPACE) {
		flags |= FKIOCTL;
	}

	if (ddi_copyin((void *)scmd->udcd_cmd, cdb, sizeof (struct dcd_cmd),
				flags)) {
		kmem_free(cdb, sizeof (struct dcd_cmd));
		return (EFAULT);
	}
	scmd = (struct udcd_cmd *)kmem_alloc(sizeof (*scmd),
			    KM_SLEEP);
	bcopy((caddr_t)in, (caddr_t)scmd, sizeof (*scmd));
	scmd->udcd_cmd = (struct dcd_cmd *)cdb;
	rw = (scmd->udcd_flags & UDCD_READ) ? B_READ: B_WRITE;


	/*
	 * Get the special buffer
	 */

	mutex_enter(DCD_MUTEX);
	while (un->un_sbuf_busy) {
		if (cv_wait_sig(&un->un_sbuf_cv, DCD_MUTEX) == 0) {
			kmem_free(scmd->udcd_cmd,
					sizeof (struct dcd_cmd));
			kmem_free((caddr_t)scmd, sizeof (*scmd));
			mutex_exit(DCD_MUTEX);
			return (EINTR);
		}
	}

	un->un_sbuf_busy = 1;
	bp  = un->un_sbufp;
	mutex_exit(DCD_MUTEX);


	/*
	 * If we are going to do actual I/O, let physio do all the
	 * things
	 */
	DAD_DEBUG2(DCD_DEVINFO, dcd_label, DCD_DEBUG,
		"dcdioctl_cmd : buflen %x\n", scmd->udcd_buflen);

	if (scmd->udcd_buflen) {
		auto struct iovec aiov;
		auto struct uio auio;
		register struct uio *uio = & auio;


		bzero((caddr_t)&auio, sizeof (struct uio));
		bzero((caddr_t)&aiov, sizeof (struct iovec));

		aiov.iov_base = scmd->udcd_bufaddr;
		aiov.iov_len = scmd->udcd_buflen;

		uio->uio_iov = &aiov;
		uio->uio_iovcnt = 1;
		uio->uio_resid = scmd->udcd_buflen;
		uio->uio_segflg = dataspace;
		uio->uio_loffset = 0;
		uio->uio_fmode = 0;

		/*
		 * Let physio do the rest...
		 */
		bp->av_back = NO_PKT_ALLOCATED;
		bp->b_forw = (struct buf *)scmd;
		err = physio(dcdstrategy, bp, devp, rw, dcdudcdmin, uio);
	} else {
		/*
		 * We have to mimic what physio would do here.
		 */
		bp->av_back = NO_PKT_ALLOCATED;
		bp->b_forw = (struct buf *)scmd;
		bp->b_flags = B_BUSY | rw;
		bp->b_edev = devp;
		bp->b_dev = cmpdev(devp);
		bp->b_bcount = bp->b_blkno = 0;
		(void) dcdstrategy(bp);
		err = biowait(bp);
	}


done:
	if ((pkt = BP_PKT(bp)) != NULL) {
		bp->av_back = NO_PKT_ALLOCATED;
		/* we need to update the completion status of udcd command */
		in->udcd_resid = bp->b_resid;
		in->udcd_status_reg = SCBP_C(pkt);
		/* XXX: we need to give error_reg also */
		dcd_destroy_pkt(pkt);
	}
	/*
	 *Tell anybody who cares that the buffer is now free
	 */
	mutex_enter(DCD_MUTEX);
	un->un_sbuf_busy = 0;
	cv_signal(&un->un_sbuf_cv);
	mutex_exit(DCD_MUTEX);

	kmem_free(scmd->udcd_cmd, sizeof (struct dcd_cmd));
	kmem_free((caddr_t)scmd, sizeof (*scmd));
	return (err);
}

static void
dcdudcdmin(struct buf *bp)
{

#ifdef lint
	bp = bp;
#endif

}

/*
 * restart a cmd from timeout() context
 *
 * the cmd is expected to be in un_utab.b_forw. If this pointer is non-zero
 * a restart timeout request has been issued and no new timeouts should
 * be requested. b_forw is reset when the cmd eventually completes in
 * sddone_and_mutex_exit()
 */
void
dcdrestart(void *arg)
{
	struct dcd_disk *un = (struct dcd_disk *)arg;
	register struct buf *bp;
	int status;

	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG, "dcdrestart\n");

	mutex_enter(DCD_MUTEX);
	bp = un->un_utab.b_forw;
	if (bp) {
		un->un_ncmds++;
		DCD_DO_KSTATS(un, kstat_waitq_to_runq, bp);
	}


	if (bp) {
		register struct dcd_pkt *pkt = BP_PKT(bp);

		mutex_exit(DCD_MUTEX);

		pkt->pkt_flags = 0;

		if ((status = dcd_transport(pkt)) != TRAN_ACCEPT) {
			mutex_enter(DCD_MUTEX);
			DCD_DO_KSTATS(un, kstat_runq_back_to_waitq, bp);
			un->un_ncmds--;
			if (status == TRAN_BUSY) {
				/* XXX : To be checked */
				/*
				if (un->un_throttle > 1) {
					ASSERT(un->un_ncmds >= 0);
					un->un_throttle = un->un_ncmds;
				}
				 */
				un->un_reissued_timeid =
				timeout(dcdrestart, (caddr_t)un,
					    DCD_BSY_TIMEOUT/500);
				mutex_exit(DCD_MUTEX);
				return;
			}
			DCD_DO_ERRSTATS(un, dcd_transerrs);
			dcd_log(DCD_DEVINFO, dcd_label, CE_WARN,
			    "dcdrestart transport failed (%x)\n", status);
			bp->b_resid = bp->b_bcount;
			SET_BP_ERROR(bp, EIO);

			DCD_DO_KSTATS(un, kstat_waitq_exit, bp);
			un->un_reissued_timeid = 0L;
			dcddone_and_mutex_exit(un, bp);
			return;
		}
		mutex_enter(DCD_MUTEX);
	}
	un->un_reissued_timeid = 0L;
	mutex_exit(DCD_MUTEX);
	DAD_DEBUG(DCD_DEVINFO, dcd_label, DCD_DEBUG, "dcdrestart done\n");
}

/*
 * This routine gets called to reset the throttle to its saved
 * value wheneven we lower the throttle.
 */
void
dcd_reset_throttle(caddr_t arg)
{
	struct dcd_disk *un = (struct dcd_disk *)arg;
	register struct diskhd *dp;

	mutex_enter(DCD_MUTEX);
	dp = &un->un_utab;

	/*
	 * start any commands that didn't start while throttling.
	 */
	if (dp->b_actf && (un->un_ncmds < un->un_throttle) &&
	    (dp->b_forw == NULL)) {
		dcdstart(un);
	}
	mutex_exit(DCD_MUTEX);
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
/* ARGSUSED0 */
static void
dcd_handle_tran_busy(struct buf *bp, struct diskhd *dp, struct dcd_disk *un)
{
	ASSERT(mutex_owned(DCD_MUTEX));


	if (dp->b_forw == NULL || dp->b_forw == bp) {
		dp->b_forw = bp;
	} else if (dp->b_forw != bp) {
		bp->b_actf = dp->b_actf;
		dp->b_actf = bp;

	}
	if (!un->un_reissued_timeid) {
		un->un_reissued_timeid =
			timeout(dcdrestart, (caddr_t)un, DCD_BSY_TIMEOUT/500);
	}
}

static void
dcd_build_user_vtoc(struct dcd_disk *un, struct vtoc *vtoc)
{

	int i;
	int nblks;
	struct dk_map2 *lpart;
	struct dk_map32	*lmap;
	struct partition *vpart;

	ASSERT(mutex_owned(DCD_MUTEX));

	/*
	 * Return vtoc structure fields in the provided VTOC area, addressed
	 * by *vtoc.
	 *
	 */

	bzero((caddr_t)vtoc, sizeof (struct vtoc));

	vtoc->v_bootinfo[0] = un->un_vtoc.v_bootinfo[0];
	vtoc->v_bootinfo[1] = un->un_vtoc.v_bootinfo[1];
	vtoc->v_bootinfo[2] = un->un_vtoc.v_bootinfo[2];

	vtoc->v_sanity		= VTOC_SANE;
	vtoc->v_version		= un->un_vtoc.v_version;

	bcopy((caddr_t)un->un_vtoc.v_volume, (caddr_t)vtoc->v_volume,
	    LEN_DKL_VVOL);

	vtoc->v_sectorsz = DEV_BSIZE;
	vtoc->v_nparts = un->un_vtoc.v_nparts;

	bcopy((caddr_t)un->un_vtoc.v_reserved, (caddr_t)vtoc->v_reserved,
	    sizeof (vtoc->v_reserved));
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

	bcopy((caddr_t)un->un_vtoc.v_timestamp, (caddr_t)vtoc->timestamp,
	    sizeof (vtoc->timestamp));

	bcopy((caddr_t)un->un_asciilabel, (caddr_t)vtoc->v_asciilabel,
	    LEN_DKL_ASCII);

}

static int
dcd_build_label_vtoc(struct dcd_disk *un, struct vtoc *vtoc)
{

	struct dk_map32		*lmap;
	struct dk_map2		*lpart;
	struct partition	*vpart;
	int			nblks;
	int			ncyl;
	int			i;

	ASSERT(mutex_owned(DCD_MUTEX));

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

	vpart = vtoc->v_part;
	for (i = 0; i < V_NUMPAR; i++) {

		if ((vpart->p_start % nblks) != 0)
			return (EINVAL);
		ncyl = vpart->p_start / nblks;
		ncyl += vpart->p_size / nblks;
		if ((vpart->p_size % nblks) != 0)
			ncyl++;
		if (ncyl > (int32_t)un->un_g.dkg_ncyl)
			return (EINVAL);
		vpart++;
	}


	/*
	 * Put appropriate vtoc structure fields into the disk label
	 *
	 */

	un->un_vtoc.v_bootinfo[0] = (uint32_t)vtoc->v_bootinfo[0];
	un->un_vtoc.v_bootinfo[1] = (uint32_t)vtoc->v_bootinfo[1];
	un->un_vtoc.v_bootinfo[2] = (uint32_t)vtoc->v_bootinfo[2];

	un->un_vtoc.v_sanity = vtoc->v_sanity;
	un->un_vtoc.v_version = vtoc->v_version;

	bcopy((caddr_t)vtoc->v_volume, (caddr_t)un->un_vtoc.v_volume,
	    LEN_DKL_VVOL);

	un->un_vtoc.v_nparts = vtoc->v_nparts;

	bcopy((caddr_t)vtoc->v_reserved, (caddr_t)un->un_vtoc.v_reserved,
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

	bcopy((caddr_t)vtoc->timestamp, (caddr_t)un->un_vtoc.v_timestamp,
	    sizeof (vtoc->timestamp));

	bcopy((caddr_t)vtoc->v_asciilabel, (caddr_t)un->un_asciilabel,
	    LEN_DKL_ASCII);

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
dcd_write_label(dev_t dev)
{
	int i, status;
	int sec, blk, head, cyl;
	short sum, *sp;
	register struct dcd_disk *un;
	caddr_t buffer;
	struct dk_label *dkl;
	struct udcd_cmd	ucmd;
	struct dcd_cmd cdb;

	if ((un = ddi_get_soft_state(dcd_state, DCDUNIT(dev))) == NULL ||
		(un->un_state == DCD_STATE_OFFLINE))
		return (ENXIO);

	ASSERT(mutex_owned(DCD_MUTEX));

	buffer = kmem_zalloc(sizeof (struct dk_label), KM_SLEEP);
	dkl = (struct dk_label *)buffer;
	bzero((caddr_t)dkl, sizeof (struct dk_label));

	bcopy((caddr_t)un->un_asciilabel, (caddr_t)dkl->dkl_asciilabel,
	    LEN_DKL_ASCII);
	bcopy((caddr_t)&un->un_vtoc, (caddr_t)&(dkl->dkl_vtoc),
	    sizeof (struct dk_vtoc));

	dkl->dkl_rpm	= un->un_g.dkg_rpm;
	dkl->dkl_pcyl	= un->un_g.dkg_pcyl;
	dkl->dkl_apc	= un->un_g.dkg_apc;
	dkl->dkl_obs1	= un->un_g.dkg_obs1;
	dkl->dkl_obs2	= un->un_g.dkg_obs2;
	dkl->dkl_intrlv = un->un_g.dkg_intrlv;
	dkl->dkl_ncyl	= un->un_g.dkg_ncyl;
	dkl->dkl_acyl	= un->un_g.dkg_acyl;
	dkl->dkl_nhead	= un->un_g.dkg_nhead;
	dkl->dkl_nsect	= un->un_g.dkg_nsect;
	dkl->dkl_obs3	= un->un_g.dkg_obs3;

	bcopy((caddr_t)un->un_map, (caddr_t)dkl->dkl_map,
	    NDKMAP * sizeof (struct dk_map32));

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
	 * Write the Primary label using the USCSI interface
	 * Write at Block 0.
	 * Build and execute the uscsi ioctl.  We build a group0
	 * or group1 command as necessary, since some targets
	 * may not support group1 commands.
	 */
	(void) bzero((caddr_t)&ucmd, sizeof (ucmd));
	(void) bzero((caddr_t)&cdb, sizeof (struct dcd_cmd));
	if ((un->un_dp->options & DMA_SUPPORTTED) ==
		DMA_SUPPORTTED) {
		cdb.cmd = ATA_WRITE_DMA;
	} else {
		if (un->un_dp->options & BLOCK_MODE)
			cdb.cmd = ATA_WRITE_MULTIPLE;
		else
			cdb.cmd = ATA_WRITE;
	}
	cdb.size = un->un_secsize;
	cdb.sector_num.lba_num = 0;
	cdb.address_mode = ADD_LBA_MODE;
	cdb.direction = DATA_WRITE;

	ucmd.udcd_flags = UDCD_WRITE;
	ucmd.udcd_cmd =  &cdb;
	ucmd.udcd_bufaddr = (caddr_t)dkl;
	ucmd.udcd_buflen = un->un_secsize;
	ucmd.udcd_flags |= UDCD_SILENT;
	mutex_exit(DCD_MUTEX);
	status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);
	mutex_enter(DCD_MUTEX);
	if (status != 0) {
		kmem_free(buffer, sizeof (struct dk_label));
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
		blk = chs2bn(dkl, cyl, head, sec);
		if ((un->un_dp->options & DMA_SUPPORTTED) ==
			DMA_SUPPORTTED) {
			cdb.cmd = ATA_WRITE_DMA;
		} else {
			if (un->un_dp->options & BLOCK_MODE)
				cdb.cmd = ATA_WRITE_MULTIPLE;
			else
				cdb.cmd = ATA_WRITE;
		}
		cdb.size = un->un_secsize;
		cdb.sector_num.lba_num = blk;
		cdb.address_mode = ADD_LBA_MODE;
		cdb.direction = DATA_WRITE;

		mutex_exit(DCD_MUTEX);
		status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);
		mutex_enter(DCD_MUTEX);
		if (status != 0) {
			kmem_free(buffer, sizeof (struct dk_label));
			return (status);
		}
	}

	kmem_free(buffer, sizeof (struct dk_label));
	return (0);

}

static int
dcd_write_deviceid(struct dcd_disk *un)
{

	int 	status;
	daddr_t	spc, blk, head, cyl;
	struct udcd_cmd ucmd;
	struct dcd_cmd cdb;
	struct dk_devid	*dkdevid;
	uint_t *ip, chksum;
	int	i;
	dev_t	dev;

	if (un->un_g.dkg_acyl < 2)
		return (EINVAL);

	/* Next to last cylinder is used */
	cyl = un->un_g.dkg_ncyl;
	spc = un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	head = un->un_g.dkg_nhead -1;
	blk = (cyl *(spc - un->un_g.dkg_apc)) +
		(head *un->un_g.dkg_nsect) + 1;

	/* Allocate the buffer */
	dkdevid = kmem_zalloc(un->un_secsize, KM_SLEEP);

	/* Fill in the revision */
	dkdevid->dkd_rev_hi = DK_DEVID_REV_MSB;
	dkdevid->dkd_rev_lo = DK_DEVID_REV_LSB;

	/* Copy in the device id */
	bcopy(un->un_devid, &dkdevid->dkd_devid,
		ddi_devid_sizeof(un->un_devid));

	/* Calculate the chksum */
	chksum = 0;
	ip = (uint_t *)dkdevid;
	for (i = 0; i < ((un->un_secsize - sizeof (int))/sizeof (int)); i++)
		chksum ^= ip[i];

	/* Fill in the checksum */
	DKD_FORMCHKSUM(chksum, dkdevid);

	(void) bzero((caddr_t)&ucmd, sizeof (ucmd));
	(void) bzero((caddr_t)&cdb, sizeof (struct dcd_cmd));

	if ((un->un_dp->options & DMA_SUPPORTTED) ==
		DMA_SUPPORTTED) {
		cdb.cmd = ATA_WRITE_DMA;
	} else {
		if (un->un_dp->options & BLOCK_MODE)
			cdb.cmd = ATA_WRITE_MULTIPLE;
		else
			cdb.cmd = ATA_WRITE;
	}
	cdb.size = un->un_secsize;
	cdb.sector_num.lba_num = blk;
	cdb.address_mode = ADD_LBA_MODE;
	cdb.direction = DATA_WRITE;

	ucmd.udcd_flags = UDCD_WRITE;
	ucmd.udcd_cmd =  &cdb;
	ucmd.udcd_bufaddr = (caddr_t)dkdevid;
	ucmd.udcd_buflen = un->un_secsize;
	ucmd.udcd_flags |= UDCD_SILENT;
	dev = makedevice(ddi_name_to_major(ddi_get_name(DCD_DEVINFO)),
		ddi_get_instance(DCD_DEVINFO) << DCDUNIT_SHIFT);
	mutex_exit(DCD_MUTEX);
	status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);
	mutex_enter(DCD_MUTEX);

	kmem_free(dkdevid, un->un_secsize);
	return (status);
}

static int
dcd_read_deviceid(struct dcd_disk *un)
{

	int status;
	daddr_t	spc, blk, head, cyl;
	struct udcd_cmd ucmd;
	struct dcd_cmd cdb;
	struct dk_devid *dkdevid;
	uint_t *ip;
	int chksum;
	int i, sz;
	dev_t dev;


	if (un->un_g.dkg_acyl < 2)
		return (EINVAL);


	/* Next to last cylinder is used */

	cyl = un->un_g.dkg_ncyl;
	spc = un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	head = un->un_g.dkg_nhead - 1;
	blk = (cyl * (spc - un->un_g.dkg_apc)) +
		(head * un->un_g.dkg_nsect) + 1;

	dkdevid = kmem_alloc(un->un_secsize, KM_SLEEP);

	(void) bzero((caddr_t)&ucmd, sizeof (ucmd));
	(void) bzero((caddr_t)&cdb, sizeof (cdb));

	if ((un->un_dp->options & DMA_SUPPORTTED) == DMA_SUPPORTTED) {
			cdb.cmd = ATA_READ_DMA;
	} else {
		if (un->un_dp->options & BLOCK_MODE)
			cdb.cmd = ATA_READ_MULTIPLE;
		else
			cdb.cmd = ATA_READ;
	}
	cdb.size = un->un_secsize;
	cdb.sector_num.lba_num = blk;
	cdb.address_mode = ADD_LBA_MODE;
	cdb.direction = DATA_READ;

	ucmd.udcd_flags = UDCD_READ;
	ucmd.udcd_cmd =  &cdb;
	ucmd.udcd_bufaddr = (caddr_t)dkdevid;
	ucmd.udcd_buflen = un->un_secsize;
	ucmd.udcd_flags |= UDCD_SILENT;
	dev = makedevice(ddi_name_to_major(ddi_get_name(DCD_DEVINFO)),
		ddi_get_instance(DCD_DEVINFO) << DCDUNIT_SHIFT);
	mutex_exit(DCD_MUTEX);
	status = dcdioctl_cmd(dev, &ucmd, UIO_SYSSPACE, UIO_SYSSPACE);
	mutex_enter(DCD_MUTEX);

	if (status != 0) {
		kmem_free((caddr_t)dkdevid, un->un_secsize);
		return (status);
	}

	/* Validate the revision */

	if ((dkdevid->dkd_rev_hi != DK_DEVID_REV_MSB) ||
	    (dkdevid->dkd_rev_lo != DK_DEVID_REV_LSB)) {
		kmem_free((caddr_t)dkdevid, un->un_secsize);
		return (EINVAL);
	}

	/* Calculate the checksum */
	chksum = 0;
	ip = (uint_t *)dkdevid;
	for (i = 0; i < ((un->un_secsize - sizeof (int))/sizeof (int)); i++)
		chksum ^= ip[i];

	/* Compare the checksums */

	if (DKD_GETCHKSUM(dkdevid) != chksum) {
		kmem_free((caddr_t)dkdevid, un->un_secsize);
		return (EINVAL);
	}

	/* VAlidate the device id */
	if (ddi_devid_valid((ddi_devid_t)&dkdevid->dkd_devid)
		!= DDI_SUCCESS) {
		kmem_free((caddr_t)dkdevid, un->un_secsize);
		return (EINVAL);
	}

	/* return a copy of the device id */
	sz = ddi_devid_sizeof((ddi_devid_t)&dkdevid->dkd_devid);
	un->un_devid = (ddi_devid_t)kmem_alloc(sz, KM_SLEEP);
	bcopy(&dkdevid->dkd_devid, un->un_devid, sz);
	kmem_free((caddr_t)dkdevid, un->un_secsize);

	return (0);
}


static ddi_devid_t
dcd_get_devid(struct dcd_disk *un)
{

	/* If registered, return that value */
	if (un->un_devid != NULL)
		return (un->un_devid);

	if (dcd_read_deviceid(un))
		return (NULL);

	(void) ddi_devid_register(DCD_DEVINFO, un->un_devid);
	return (un->un_devid);
}


static ddi_devid_t
dcd_create_devid(struct dcd_disk *un)
{
	if (ddi_devid_init(DCD_DEVINFO, DEVID_FAB, 0, NULL, (ddi_devid_t *)
		&un->un_devid)
		== DDI_FAILURE)
		return (NULL);

	if (dcd_write_deviceid(un)) {
		ddi_devid_free(un->un_devid);
		un->un_devid = NULL;
		return (NULL);
	}

	(void) ddi_devid_register(DCD_DEVINFO, un->un_devid);
	return (un->un_devid);
}


#ifndef lint
void
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

	dcd_log(dev, label, level, "%s", buf);
}
#endif /* Not lint */

#ifndef lint
/*
 * Print a piece of inquiry data- cleaned up for non-printable characters
 * and stopping at the first space character after the beginning of the
 * passed string;
 */

void
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
#endif /* Not lint */

char *
dcd_sname(
	uchar_t	status)
{
	switch (status & STATUS_ATA_MASK) {
	case STATUS_GOOD:
		return ("good status");

	case STATUS_ATA_BUSY:
		return ("busy");

	default:
		return ("<unknown status>");
	}
}

/* ARGSUSED0 */
char *
dcd_rname(int reason)
{
	static char *rnames[] = {
		"cmplt",
		"incomplete",
		"dma_derr",
		"tran_err",
		"reset",
		"aborted",
		"timeout",
		"data_ovr",
	};
	if (reason > CMD_DATA_OVR) {
		return ("<unknown reason>");
	} else {
		return (rnames[reason]);
	}
}



/* ARGSUSED0 */
int
dcd_check_wp(dev_t dev)
{

	return (0);
}

/*
 * Create device error kstats
 */
static int
dcd_create_errstats(struct dcd_disk *un, int instance)
{

	char kstatname[KSTAT_STRLEN];

	if (un->un_errstats == (kstat_t *)0) {
		(void) sprintf(kstatname, "dad%d,error", instance);
		un->un_errstats = kstat_create("daderror", instance, kstatname,
			"device_error", KSTAT_TYPE_NAMED,
			sizeof (struct dcd_errstats)/ sizeof (kstat_named_t),
			KSTAT_FLAG_PERSISTENT);

		if (un->un_errstats) {
			struct dcd_errstats *dtp;

			dtp = (struct dcd_errstats *)un->un_errstats->ks_data;
			kstat_named_init(&dtp->dcd_softerrs, "Soft Errors",
				KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_harderrs, "Hard Errors",
				KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_transerrs,
				"Transport Errors", KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_model, "Model",
				KSTAT_DATA_CHAR);
			kstat_named_init(&dtp->dcd_revision, "Revision",
				KSTAT_DATA_CHAR);
			kstat_named_init(&dtp->dcd_serial, "Serial No",
				KSTAT_DATA_CHAR);
			kstat_named_init(&dtp->dcd_capacity, "Size",
				KSTAT_DATA_ULONGLONG);
			kstat_named_init(&dtp->dcd_rq_media_err, "Media Error",
				KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_rq_ntrdy_err,
				"Device Not Ready", KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_rq_nodev_err, " No Device",
				KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_rq_recov_err, "Recoverable",
				KSTAT_DATA_UINT32);
			kstat_named_init(&dtp->dcd_rq_illrq_err,
				"Illegal Request", KSTAT_DATA_UINT32);

			un->un_errstats->ks_private = un;
			un->un_errstats->ks_update = nulldev;
			kstat_install(un->un_errstats);

			(void) strncpy(&dtp->dcd_model.value.c[0],
					un->un_dcd->dcd_ident->dcd_model, 16);
			(void) strncpy(&dtp->dcd_serial.value.c[0],
					un->un_dcd->dcd_ident->dcd_drvser, 16);
			(void) strncpy(&dtp->dcd_revision.value.c[0],
					un->un_dcd->dcd_ident->dcd_fw, 8);
			dtp->dcd_capacity.value.ui64 =
					(uint64_t)((uint64_t)un->un_capacity *
						(uint64_t)un->un_lbasize);
		}
	}
	return (0);
}


/*
 * This has been moved from DADA layer as this doenot do anything other than
 * retrying the command when there is busy or it doesnot complete
 */
int
dcd_poll(struct dcd_pkt *pkt)
{
	register	busy_count, rval = -1, savef;
	clock_t	savet;
	void	(*savec)();


	/*
	 * Save old flags
	 */
	savef = pkt->pkt_flags;
	savec = pkt->pkt_comp;
	savet = pkt->pkt_time;

	pkt->pkt_flags |= FLAG_NOINTR;


	/*
	 * Set the Pkt_comp to NULL
	 */

	pkt->pkt_comp = 0;

	/*
	 * Set the Pkt time for the polled command
	 */
	if (pkt->pkt_time == 0) {
		pkt->pkt_time = DCD_POLL_TIMEOUT;
	}


	/* Now transport the command */
	for (busy_count = 0; busy_count < dcd_poll_busycnt; busy_count++) {
		if ((rval = dcd_transport(pkt)) == TRAN_ACCEPT) {
			if (pkt->pkt_reason == CMD_INCOMPLETE &&
				pkt->pkt_state == 0) {
				delay(100);
			} else if (pkt->pkt_reason  == CMD_CMPLT) {
				rval = 0;
				break;
			}
		}
		if (rval == TRAN_BUSY)  {
			delay(100);
			continue;
		}
	}

	pkt->pkt_flags = savef;
	pkt->pkt_comp = savec;
	pkt->pkt_time = savet;
	return (rval);
}


void
dcd_translate(struct dadkio_status32 *statp, struct udcd_cmd *cmdp)
{


	if (cmdp->udcd_status_reg & STATUS_ATA_BUSY)
		statp->status = DADKIO_STAT_NOT_READY;
	else if (cmdp->udcd_status_reg & STATUS_ATA_DWF)
		statp->status = DADKIO_STAT_HARDWARE_ERROR;
	else if (cmdp->udcd_status_reg & STATUS_ATA_CORR)
		statp->status = DADKIO_STAT_SOFT_ERROR;
	else if (cmdp->udcd_status_reg & STATUS_ATA_ERR) {
		/*
		 * The error register is valid only when BSY and DRQ not set
		 * Assumed that HBA has checked this before it gives the data
		 */
		if (cmdp->udcd_error_reg & ERR_AMNF)
			statp->status = DADKIO_STAT_NOT_FORMATTED;
		else if (cmdp->udcd_error_reg & ERR_TKONF)
			statp->status = DADKIO_STAT_NOT_FORMATTED;
		else if (cmdp->udcd_error_reg & ERR_ABORT)
			statp->status = DADKIO_STAT_ILLEGAL_REQUEST;
		else if (cmdp->udcd_error_reg & ERR_IDNF)
			statp->status = DADKIO_STAT_NOT_FORMATTED;
		else if (cmdp->udcd_error_reg & ERR_UNC)
			statp->status = DADKIO_STAT_BUS_ERROR;
		else if (cmdp->udcd_error_reg & ERR_BBK)
			statp->status = DADKIO_STAT_MEDIUM_ERROR;
	} else
		statp->status = DADKIO_STAT_NO_ERROR;
}


void
dcd_fake_geometry(struct dcd_disk *un)
{

	struct dcd_device *devp;
	struct dk_map32 lmap[V_NUMPAR];
	char buf[256];
	uint32_t	no_of_lbasec, capacity;


	devp = un->un_dcd;
	/*
	 * Fill in disk geometry from label.
	 */
	un->un_g.dkg_ncyl = devp->dcd_ident->dcd_fixcyls - 2;
	un->un_g.dkg_acyl = 2;
	un->un_g.dkg_bcyl = 0;
	un->un_g.dkg_nhead = devp->dcd_ident->dcd_heads;
	un->un_g.dkg_bhead = 0;
	un->un_g.dkg_nsect = devp->dcd_ident->dcd_sectors;
	un->un_g.dkg_gap1 = 0;
	un->un_g.dkg_gap2 = 0;
	un->un_g.dkg_intrlv = 0;
	un->un_g.dkg_pcyl =  devp->dcd_ident->dcd_fixcyls;
	un->un_g.dkg_rpm = 5400;

	/*
	 * The Read and Write reinstruct values may not be vaild
	 * for older disks.
	 */
	un->un_g.dkg_read_reinstruct = 0;
	un->un_g.dkg_write_reinstruct = 0;

	/*
	 * Fill in partition table.
	 */
	bzero((caddr_t)lmap,  NDKMAP*sizeof (struct dk_map32));

	no_of_lbasec = devp->dcd_ident->dcd_addrsec[1];
	no_of_lbasec = no_of_lbasec << 16;
	no_of_lbasec = no_of_lbasec | devp->dcd_ident->dcd_addrsec[0];
	no_of_lbasec = no_of_lbasec - (2*un->un_g.dkg_nhead*un->un_g.dkg_nsect);
	capacity = un->un_g.dkg_ncyl*un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	if (no_of_lbasec > capacity) {
		capacity = no_of_lbasec;
		un->un_g.dkg_ncyl = (capacity) /
			(un->un_g.dkg_nhead * un->un_g.dkg_nsect);
		un->un_g.dkg_pcyl = un->un_g.dkg_ncyl + 2;
	}

	lmap[2].dkl_cylno = 0;
	lmap[2].dkl_nblk =
		un->un_g.dkg_ncyl*un->un_g.dkg_nhead * un->un_g.dkg_nsect;
	bcopy((caddr_t)lmap, (caddr_t)un->un_map,
	    NDKMAP * sizeof (struct dk_map32));

	(void) strncpy(buf, devp->dcd_ident->dcd_model, 25);
	buf[25] = '\0';
	(void) sprintf(&buf[strlen(buf)], " cyl %d", un->un_g.dkg_ncyl);
	(void) sprintf(&buf[strlen(buf)], " alt %d",  un->un_g.dkg_acyl);
	(void) sprintf(&buf[strlen(buf)], " hd %d", un->un_g.dkg_nhead);
	(void) sprintf(&buf[strlen(buf)], " sec %d", un->un_g.dkg_nsect);
	(void) bcopy((caddr_t)buf, (caddr_t)un->un_asciilabel, strlen(buf));

	/*
	 * Fill in the essential parts in the vtoc
	 */

	un->un_vtoc.v_nparts = 8;
	/* Specify the full disk partition alone */
	un->un_vtoc.v_part[2].p_tag = V_BACKUP;
	un->un_vtoc.v_part[2].p_flag =  V_UNMNT;
}
