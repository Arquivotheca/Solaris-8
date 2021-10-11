/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * NOT a DDI compliant Sun Fibre Channel port driver(fp)
 *
 */
#pragma ident	"@(#)fp.c	1.7	99/11/04 SMI"

#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/thread.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/callb.h>
#include <sys/fibre-channel/fcio.h>
#include <sys/fibre-channel/fc.h>
#include <sys/fibre-channel/impl/fc_ulpif.h>
#include <sys/fibre-channel/impl/fc_portif.h>
#include <sys/fibre-channel/impl/fc_fcaif.h>
#include <sys/fibre-channel/impl/fp.h>

char _depends_on[] = "misc/fctl";

extern int did_table_size;
extern int pwwn_table_size;
extern int modrootloaded;

static struct cb_ops fp_cb_ops = {
	fp_open,			/* open */
	fp_close,			/* close */
	nodev,				/* strategy */
	nodev,				/* print */
	nodev,				/* dump */
	nodev,				/* read */
	nodev,				/* write */
	fp_ioctl,			/* ioctl */
	nodev,				/* devmap */
	nodev,				/* mmap */
	nodev,				/* segmap */
	nochpoll,			/* chpoll */
	ddi_prop_op,			/* cb_prop_op */
	0,				/* streamtab */
	D_NEW | D_MP | D_HOTPLUG,	/* cb_flag */
	CB_REV,				/* rev */
	nodev,				/* aread */
	nodev				/* awrite */
};

static struct dev_ops fp_ops = {
	DEVO_REV,	/* build revision */
	0,		/* reference count */
	fp_getinfo,	/* getinfo */
	nulldev,	/* identify - Obsoleted */
	nulldev,	/* probe */
	fp_attach,	/* attach */
	fp_detach,	/* detach */
	nodev,		/* reset */
	&fp_cb_ops,	/* cb_ops */
	NULL,		/* bus_ops */
	ddi_power	/* power */
};

static struct modldrv modldrv = {
	&mod_driverops,				/* Type of Module */
	"Sun Fibre Channel Port Driver v1.7 ", 	/* Module Name */
	&fp_ops					/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,	/* Rev of the loadable modules system */
	&modldrv,	/* NULL terminated list of */
	NULL		/* Linkage structures */
};

static uint16_t ns_reg_cmds[] = {
	NS_RPN_ID,
	NS_RNN_ID,
	NS_RCS_ID,
	NS_RPT_ID
};

struct fp_xlat {
	uchar_t	xlat_state;
	int	xlat_rval;
} fp_xlat [] = {
	{ FC_PKT_SUCCESS,	FC_SUCCESS },
	{ FC_PKT_REMOTE_STOP,	FC_FAILURE },
	{ FC_PKT_LOCAL_RJT,	FC_FAILURE },
	{ FC_PKT_NPORT_RJT,	FC_ELS_PREJECT },
	{ FC_PKT_FABRIC_RJT,	FC_ELS_FREJECT },
	{ FC_PKT_LOCAL_BSY,	FC_TRAN_BUSY },
	{ FC_PKT_TRAN_BSY,	FC_TRAN_BUSY },
	{ FC_PKT_NPORT_BSY,	FC_PBUSY },
	{ FC_PKT_FABRIC_BSY,	FC_FBUSY },
	{ FC_PKT_LS_RJT,	FC_FAILURE },
	{ FC_PKT_BA_RJT,	FC_FAILURE },
	{ FC_PKT_TIMEOUT,	FC_FAILURE },
	{ FC_PKT_TRAN_ERROR,	FC_TRANSPORT_ERROR },
	{ FC_PKT_FAILURE,	FC_FAILURE },
	{ FC_PKT_PORT_OFFLINE,	FC_OFFLINE }
};

static uchar_t fp_valid_alpas[] = {
	0x01, 0x02, 0x04, 0x08, 0x0F, 0x10, 0x17, 0x18, 0x1B,
	0x1D, 0x1E, 0x1F, 0x23, 0x25, 0x26, 0x27, 0x29, 0x2A,
	0x2B, 0x2C, 0x2D, 0x2E, 0x31, 0x32, 0x33, 0x34, 0x35,
	0x36, 0x39, 0x3A, 0x3C, 0x43, 0x45, 0x46, 0x47, 0x49,
	0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x51, 0x52, 0x53, 0x54,
	0x55, 0x56, 0x59, 0x5A, 0x5C, 0x63, 0x65, 0x66, 0x67,
	0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x71, 0x72, 0x73,
	0x74, 0x75, 0x76, 0x79, 0x7A, 0x7C, 0x80, 0x81, 0x82,
	0x84, 0x88, 0x8F, 0x90, 0x97, 0x98, 0x9B, 0x9D, 0x9E,
	0x9F, 0xA3, 0xA5, 0xA6, 0xA7, 0xA9, 0xAA, 0xAB, 0xAC,
	0xAD, 0xAE, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB9,
	0xBA, 0xBC, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB,
	0xCC, 0xCD, 0xCE, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
	0xD9, 0xDA, 0xDC, 0xE0, 0xE1, 0xE2, 0xE4, 0xE8, 0xEF
};

static struct fp_perms {
	uint16_t	fp_ioctl_cmd;
	uchar_t		fp_open_flag;
} fp_perm_list [] = {
	{ FCIO_GET_NUM_DEVS, 		FP_OPEN },
	{ FCIO_GET_DEV_LIST,		FP_OPEN },
	{ FCIO_GET_SYM_PNAME,		FP_OPEN },
	{ FCIO_GET_SYM_NNAME,		FP_OPEN },
	{ FCIO_SET_SYM_PNAME,		FP_EXCL },
	{ FCIO_SET_SYM_NNAME,		FP_EXCL },
	{ FCIO_GET_LOGI_PARAMS,		FP_OPEN },
	{ FCIO_DEV_LOGIN,		FP_EXCL },
	{ FCIO_DEV_LOGOUT,		FP_EXCL },
	{ FCIO_GET_STATE,		FP_OPEN },
	{ FCIO_DEV_REMOVE,		FP_EXCL },
	{ FCIO_GET_FCODE_REV,		FP_EXCL },
	{ FCIO_GET_FW_REV,		FP_EXCL },
	{ FCIO_GET_DUMP_SIZE,		FP_EXCL },
	{ FCIO_FORCE_DUMP,		FP_EXCL },
	{ FCIO_GET_DUMP,		FP_EXCL },
	{ FCIO_GET_TOPOLOGY,		FP_EXCL },
	{ FCIO_RESET_LINK,		FP_EXCL },
	{ FCIO_RESET_HARD,		FP_EXCL },
	{ FCIO_RESET_HARD_CORE,		FP_EXCL },
	{ FCIO_DIAG,			FP_EXCL },
	{ FCIO_NS,			FP_EXCL },
	{ FCIO_DOWNLOAD_FW,		FP_EXCL },
	{ FCIO_DOWNLOAD_FCODE,		FP_EXCL },
	{ FCIO_LINK_STATUS,		FP_OPEN },
	{ FCIO_GET_HOST_PARAMS,		FP_OPEN }
};

#ifdef	FP_TNF_ENABLED

TNF_DECLARE_RECORD(la_wwn_t, tnf_wwn_t);
TNF_DECLARE_RECORD(fc_porttype_t, tnf_porttype_t);

TNF_DEFINE_RECORD_4(la_wwn_t, tnf_wwn_t,
    tnf_uint,	w.wwn_hi,
    tnf_uint,	w.nport_id,
    tnf_uint,	w.naa_id,
    tnf_uint,	w.wwn_lo)

TNF_DEFINE_RECORD_2(fc_porttype_t, tnf_porttype_t,
    tnf_int,	rsvd,
    tnf_int,	port_type)

#endif /* FP_TNF_ENABLED */

static uchar_t fp_verbosity = (FP_WARNING_MESSAGES | FP_FATAL_MESSAGES);
static uint32_t fp_options = 0;

static int fp_retry_delay = FP_RETRY_DELAY;	/* retry after this delay */
static int fp_retry_count = FP_RETRY_COUNT;	/* number of retries */
static int fp_offline_ticker = FP_OFFLINE_TICKER; /* seconds */

static clock_t	fp_retry_ticks;
static clock_t	fp_offline_ticks;

static void *fp_state;
static int fp_retry_ticker;
static uint32_t fp_unsol_buf_count = FP_UNSOL_BUF_COUNT;
static uint32_t fp_unsol_buf_size = FP_UNSOL_BUF_SIZE;


int
_init(void)
{
	int ret;

	/*
	 * Tick every second when there are commands to retry.
	 * This is hard coded in the driver and it isn't a
	 * tunable and so shouldn't be. It should tick at the
	 * least granular value of pkt_timeout (which is one
	 * second)
	 */
	fp_retry_ticker = 1;

	fp_retry_ticks = drv_usectohz(fp_retry_ticker * 1000 * 1000);
	fp_offline_ticks = drv_usectohz(fp_offline_ticker * 1000 * 1000);

	ret = ddi_soft_state_init(&fp_state, sizeof (struct fc_port), 8);
	if (ret != 0) {
		return (ret);
	}

	FP_TNF_INIT((&modlinkage));

	if ((ret = mod_install(&modlinkage)) != 0) {
		FP_TNF_FINI((&modlinkage));
		ddi_soft_state_fini(&fp_state);
	}

	return (ret);
}


int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&modlinkage)) == 0) {
		FP_TNF_FINI((&modlinkage));
		ddi_soft_state_fini(&fp_state);
	}

	return (ret);
}


int
_info(struct modinfo *modinfo)
{
	return (mod_info(&modlinkage, modinfo));
}


/*
 * fp_attach:
 *
 * If the cmd is DDI_ATTACH, the fp_attach_handler function
 * will take care of handling ULP port attaches. All other
 * cases are handled within this function itself.
 *
 */
static int
fp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int rval;

	switch (cmd) {
	case DDI_ATTACH:
		rval = fp_attach_handler(dip);
		break;

	case DDI_PM_RESUME:
		rval = fp_pm_resume_handler(dip);
		break;

	case DDI_RESUME:
		rval = fp_resume_handler(dip);
		break;

	default:
		rval = DDI_FAILURE;
		break;
	}
	return (rval);
}


static int
fp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int 			rval = DDI_FAILURE;
	fc_port_t 		*port;
	ddi_attach_cmd_t 	converse;

	port = (fc_port_t *)ddi_get_soft_state(fp_state, ddi_get_instance(dip));
	if (port == NULL) {
		return (DDI_FAILURE);
	}

	mutex_enter(&port->fp_mutex);
	if (port->fp_ulp_attach) {
		mutex_exit(&port->fp_mutex);
		return (DDI_FAILURE);
	}

	switch (cmd) {
	case DDI_DETACH:
		if (port->fp_job_head || port->fp_task != FP_TASK_IDLE) {
			mutex_exit(&port->fp_mutex);
			return (DDI_FAILURE);
		}
		mutex_exit(&port->fp_mutex);
		converse = DDI_ATTACH;
		if (fctl_detach_ulps(port, cmd, &modlinkage) != FC_SUCCESS) {
			rval = DDI_FAILURE;
			break;
		}
		rval = fp_detach_handler(port);
		break;

	case DDI_PM_SUSPEND:
		mutex_exit(&port->fp_mutex);
		converse = DDI_PM_RESUME;
		if (fctl_detach_ulps(port, cmd, &modlinkage) != FC_SUCCESS) {
			rval = DDI_FAILURE;
			break;
		}
		rval = fp_pm_suspend_handler(port);
		break;

	case DDI_SUSPEND:
		mutex_exit(&port->fp_mutex);
		converse = DDI_RESUME;
		if (fctl_detach_ulps(port, cmd, &modlinkage) != FC_SUCCESS) {
			rval = DDI_FAILURE;
			break;
		}
		rval = fp_suspend_handler(port);
		break;

	default:
		mutex_exit(&port->fp_mutex);
		break;
	}

	if (rval != DDI_SUCCESS) {
		fctl_attach_ulps(port, converse, &modlinkage);
	}

	return (rval);
}


/*
 * fp_getinfo:
 *   Given the device number, return either the
 *   dev_info_t pointer or the instance number.
 */
/* ARGSUSED */
static int
fp_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
	void *arg, void **result)
{
	int 		rval;
	int 		instance;
	fc_port_t 	*port;

	rval = DDI_SUCCESS;
	instance = getminor((dev_t)arg);

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((port = ddi_get_soft_state(fp_state, instance)) == NULL) {
			rval = DDI_FAILURE;
			break;
		}
		*result = (void *)port->fp_port_dip;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		break;

	default:
		rval = DDI_FAILURE;
		break;
	}

	return (rval);
}


static int
fp_open(dev_t *devp, int flag, int otype, cred_t *credp)
{
	int 		instance;
	fc_port_t 	*port;

	if (otype != OTYP_CHR) {
		return (EINVAL);
	}

	/*
	 * This is not a toy to play with. Allow only powerful
	 * users (hopefully knowledgeable) to access the port
	 * (A hacker potentially could download a sick binary
	 * file into FCA)
	 */
	if (drv_priv(credp)) {
		return (EPERM);
	}

	instance = (int)getminor(*devp);

	port = (fc_port_t *)ddi_get_soft_state(fp_state, instance);
	if (port == NULL) {
		return (ENXIO);
	}

	mutex_enter(&port->fp_mutex);
	if (port->fp_flag & FP_EXCL) {
		/*
		 * It is already open for exclusive access.
		 * So shut the door on this caller.
		 */
		mutex_exit(&port->fp_mutex);
		return (EBUSY);
	}

	if (flag & FEXCL) {
		if (port->fp_flag & FP_OPEN) {
			/*
			 * Exclusive operation not possible
			 * as it is already opened
			 */
			mutex_exit(&port->fp_mutex);
			return (EBUSY);
		}
		port->fp_flag |= FP_EXCL;
	}
	port->fp_flag |= FP_OPEN;
	mutex_exit(&port->fp_mutex);

	return (0);
}


/*
 * The driver close entry point is called on the last close()
 * of a device. So it is perfectly alright to just clobber the
 * open flag and reset it to idle (instead of having to reset
 * each flag bits). For any confusion, check out close(9E).
 */

/* ARGSUSED */
static int
fp_close(dev_t dev, int flag, int otype, cred_t *credp)
{
	int 		instance;
	fc_port_t 	*port;

	if (otype != OTYP_CHR) {
		return (EINVAL);
	}

	instance = (int)getminor(dev);

	port = (fc_port_t *)ddi_get_soft_state(fp_state, instance);
	if (port == NULL) {
		return (ENXIO);
	}

	mutex_enter(&port->fp_mutex);
	if ((port->fp_flag & FP_OPEN) == 0) {
		mutex_exit(&port->fp_mutex);
		return (ENODEV);
	}
	port->fp_flag = FP_IDLE;
	mutex_exit(&port->fp_mutex);

	return (0);
}


/* ARGSUSED */
static int
fp_ioctl(dev_t dev, int cmd, intptr_t data, int mode, cred_t *credp, int *rval)
{
	int 		instance;
	fcio_t		fcio;
	fc_port_t 	*port;

	instance = (int)getminor(dev);

	port = (fc_port_t *)ddi_get_soft_state(fp_state, instance);
	if (port == NULL) {
		return (ENXIO);
	}

	mutex_enter(&port->fp_mutex);
	if ((port->fp_flag & FP_OPEN) == 0) {
		mutex_exit(&port->fp_mutex);
		return (ENXIO);
	}
	mutex_exit(&port->fp_mutex);

	switch (cmd) {
	case FCIO_CMD:
	{
#ifdef	_MULTI_DATAMODEL
		switch (ddi_model_convert_from(mode & FMODELS)) {
		case DDI_MODEL_ILP32:
		{
			struct fcio32 fcio32;

			if (ddi_copyin((void *)data, (void *)&fcio32,
			    sizeof (struct fcio32), mode)) {
				return (EFAULT);
			}
			fcio.fcio_xfer = fcio32.fcio_xfer;
			fcio.fcio_cmd = fcio32.fcio_cmd;
			fcio.fcio_flags = fcio32.fcio_flags;
			fcio.fcio_cmd_flags = fcio32.fcio_cmd_flags;
			fcio.fcio_ilen = (size_t)fcio32.fcio_ilen;
			fcio.fcio_ibuf = (caddr_t)fcio32.fcio_ibuf;
			fcio.fcio_olen = (size_t)fcio32.fcio_olen;
			fcio.fcio_obuf = (caddr_t)fcio32.fcio_obuf;
			fcio.fcio_alen = (size_t)fcio32.fcio_alen;
			fcio.fcio_abuf = (caddr_t)fcio32.fcio_abuf;
			fcio.fcio_errno = fcio32.fcio_errno;
			break;
		}
		case DDI_MODEL_NONE:
			if (ddi_copyin((void *)data, (void *)&fcio,
			    sizeof (fcio_t), mode)) {
				return (EFAULT);
			}
			break;
		}
#else	/* _MULTI_DATAMODEL */
		if (ddi_copyin((void *)data, (void *)&fcio,
		sizeof (fcio_t), mode)) {
			return (EFAULT);
		}
#endif	/* _MULTI_DATAMODEL */
		return (fp_fciocmd(port, data, mode, &fcio));
	}

	default:
		return (fctl_ulp_port_ioctl(port, dev, cmd,
		    data, mode, credp, rval));
	}
}


/*
 * Perform port attach
 */
static int
fp_attach_handler(dev_info_t *dip)
{
	int			rval;
	int			instance;
	int 			port_num;
	int			port_len;
	char 			name[30];
	fp_cmd_t		*pkt;
	uint32_t		ub_count;
	fc_port_t 		*port;
	kthread_t		*thread;
	job_request_t 		*job;
	struct kmem_cache 	*cache;

	instance = ddi_get_instance(dip);
	port_len = sizeof (port_num);

	rval = ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "port",
	    (caddr_t)&port_num, &port_len);

	if (rval != DDI_SUCCESS) {
		cmn_err(CE_WARN, "fp(%d): No port property in devinfo",
		    instance);
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(dip, "devctl", S_IFCHR,
	    instance, DDI_NT_NEXUS, 0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "fp(%d): failed to create minor node",
		    instance);
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(fp_state, instance) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "fp(%d): failed to alloc soft state",
		    instance);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	port = (fc_port_t *)ddi_get_soft_state(fp_state, instance);

	port->fp_instance = instance;
	port->fp_ulp_attach = 1;
	port->fp_port_num = port_num;
	port->fp_verbose = fp_verbosity;
	port->fp_options = fp_options;

	FP_SET_TASK(port, FP_TASK_PORT_STARTUP);

	port->fp_fca_dip = ddi_get_parent(dip);
	port->fp_port_dip = dip;
	port->fp_fca_tran = (fc_fca_tran_t *)ddi_get_driver_private(
	    port->fp_fca_dip);

	mutex_init(&port->fp_mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&port->fp_cv, NULL, CV_DRIVER, NULL);
	cv_init(&port->fp_attach_cv, NULL, CV_DRIVER, NULL);

	(void) sprintf(name, "fp%d_cache", instance);
	cache = kmem_cache_create(name, FCA_PKT_SIZE(port) +
	    sizeof (fp_cmd_t), 8, fp_cache_constructor,
	    fp_cache_destructor, NULL, (void *)port, NULL, 0);
	if (cache == NULL) {
		fp_attach_handler_CLEANUP(port, instance);
		return (DDI_FAILURE);
	}
	FP_PKT_CACHE(port) = cache;

	port->fp_did_table = kmem_zalloc(did_table_size *
	    sizeof (struct d_id_hash), KM_SLEEP);

	port->fp_pwwn_table = kmem_zalloc(pwwn_table_size *
	    sizeof (struct pwwn_hash), KM_SLEEP);

	thread = thread_create((caddr_t)NULL, 0, fp_job_handler,
	    (caddr_t)port, 0, &p0, TS_RUN, v.v_maxsyspri - 2);
	if (thread == NULL) {
		cmn_err(CE_WARN, "fp(%d): thread_create() failed", instance);
		fp_attach_handler_CLEANUP(port, instance);
		return (DDI_FAILURE);
	}

	mutex_enter(&port->fp_mutex);
	/*
	 * Bind the callbacks with the FCA; This will open the gate
	 * for asynchronous callbacks - From here on mutexes must be
	 * held. Hold the port driver mutex as the callbacks are
	 * bound until the service parameters are properly filled
	 * in (in order to be able to properly respond to unsolicited
	 * ELS requests)
	 */
	ASSERT(port->fp_fca_tran->fca_bind_port != NULL);
	if (fp_bind_callbacks(port) != DDI_SUCCESS) {
		mutex_exit(&port->fp_mutex);
		fp_attach_handler_CLEANUP(port, instance);
		return (DDI_FAILURE);
	}
	ASSERT(port->fp_fca_handle != NULL);
	mutex_exit(&port->fp_mutex);

	pkt = fp_alloc_pkt(port, sizeof (la_els_logi_t), sizeof (la_els_logi_t),
	    KM_SLEEP);
	if (pkt == NULL) {
		cmn_err(CE_WARN, "fp(%d): failed to allocate ELS packet",
		    instance);
		fp_attach_handler_CLEANUP(port, instance);
		return (DDI_FAILURE);
	}

	mutex_enter(&port->fp_mutex);
	port->fp_thread = thread;
	port->fp_els_resp_pkt = pkt;
	mutex_exit(&port->fp_mutex);

	/*
	 * Determine the count of unsolicited buffers this FCA can support
	 */
	fp_retrieve_caps(port);

	/*
	 * Allocate unsolicited buffer tokens
	 */
	if (port->fp_ub_count) {
		ub_count = port->fp_ub_count;
		port->fp_ub_tokens = kmem_zalloc(ub_count *
		    sizeof (*port->fp_ub_tokens), KM_SLEEP);
		/*
		 * Do not fail the attach if unsolicited buffer allocation
		 * fails; Just try to get along with whatever the FCA can do.
		 */
		if (fc_ulp_uballoc(port, &ub_count, fp_unsol_buf_size,
		    FC_TYPE_EXTENDED_LS, port->fp_ub_tokens) !=
		    FC_SUCCESS || ub_count != port->fp_ub_count) {
			cmn_err(CE_WARN, "fp(%d): failed to allocate "
			    " Unsolicited buffers. proceeding with attach...",
			    instance);
			kmem_free(port->fp_ub_tokens,
			    sizeof (*port->fp_ub_tokens) * port->fp_ub_count);
			port->fp_ub_tokens = NULL;


		}
	}

	fp_load_ulp_modules(dip, port);

	/*
	 * fctl maintains a list of all port handles, so
	 * help fctl add this one to its list now.
	 */
	mutex_enter(&port->fp_mutex);
	fctl_add_port(port);
	mutex_exit(&port->fp_mutex);

	/*
	 * Allow DDI_SUSPEND and DDI_RESUME
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
	    "pm-hardware-state", "needs-suspend-resume",
	    strlen("needs-suspend-resume") + 1);

	/* Defer ONLINE processing work if the port isn't ONLINE yet */
	if (FC_PORT_STATE_MASK(port->fp_bind_state) == FC_STATE_OFFLINE) {
		fp_startup_done((opaque_t)port, FC_PKT_SUCCESS);
	} else {
		job = fctl_alloc_job(JOB_PORT_STARTUP, JOB_TYPE_FCTL_ASYNC,
		    fp_startup_done, (opaque_t)port, KM_SLEEP);
		fctl_enque_job(port, job);
	}

	mutex_enter(&port->fp_mutex);
	while (port->fp_ulp_attach) {
		cv_wait(&port->fp_attach_cv, &port->fp_mutex);
	}
	mutex_exit(&port->fp_mutex);

	ddi_report_dev(dip);

	return (DDI_SUCCESS);
}


static int
fp_resume_handler(dev_info_t *dip)
{
	int		rval;
	fc_port_t 	*port;

	port = (fc_port_t *)ddi_get_soft_state(fp_state, ddi_get_instance(dip));
	ASSERT(port != NULL);

#ifdef	DEBUG
	mutex_enter(&port->fp_mutex);
	ASSERT(port->fp_soft_state & FP_SOFT_SUSPEND);
	mutex_exit(&port->fp_mutex);
#endif /* DEBUG */

	/*
	 * If the port was power suspended, no need
	 * to resume until power resumed.
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_soft_state & FP_SOFT_PM_SUSPEND) {
		port->fp_soft_state &= ~FP_SOFT_SUSPEND;
		mutex_exit(&port->fp_mutex);
		return (DDI_SUCCESS);
	}
	port->fp_soft_state &= ~FP_SOFT_SUSPEND;

	mutex_exit(&port->fp_mutex);
	rval = fp_resume_all(port, DDI_RESUME);
	if (rval != DDI_SUCCESS) {
		mutex_enter(&port->fp_mutex);
		port->fp_soft_state |= FP_SOFT_SUSPEND;
		mutex_exit(&port->fp_mutex);
	}

	return (rval);
}


static int
fp_pm_resume_handler(dev_info_t *dip)
{
	int		rval;
	fc_port_t 	*port;

	port = (fc_port_t *)ddi_get_soft_state(fp_state, ddi_get_instance(dip));
	ASSERT(port != NULL);

#ifdef	DEBUG
	mutex_enter(&port->fp_mutex);
	ASSERT((port->fp_soft_state & FP_SOFT_SUSPEND) == 0);
	ASSERT(port->fp_soft_state & FP_SOFT_PM_SUSPEND);
	mutex_exit(&port->fp_mutex);
#endif /* DEBUG */

	mutex_enter(&port->fp_mutex);
	port->fp_soft_state &= ~FP_SOFT_PM_SUSPEND;
	mutex_exit(&port->fp_mutex);

	rval = fp_resume_all(port, DDI_PM_RESUME);
	if (rval != DDI_SUCCESS) {
		mutex_enter(&port->fp_mutex);
		port->fp_soft_state |= FP_SOFT_PM_SUSPEND;
		mutex_exit(&port->fp_mutex);
	}

	return (rval);
}


/*
 * It is important to note that the power may possibly be removed between
 * SUSPEND and the ensuing RESUME operation. In such a context the underlying
 * FC port hardware would have gone through an OFFLINE to ONLINE transition
 * (hardware state). In this case, the port driver may need to rediscover the
 * topology, perform LOGINs, register with the name server again and perform
 * any such port initialization procedures. To perform LOGINs, the driver could
 * use the port device handle to see if a LOGIN needs to be performed and use
 * the D_ID and WWN in it. The LOGINs may fail (if the hardware is reconfigured
 * or removed) which will be reflected in the map the ULPs will see.
 */
static int
fp_resume_all(fc_port_t *port, ddi_attach_cmd_t cmd)
{
	mutex_enter(&port->fp_mutex);
	if (fp_bind_callbacks(port) != DDI_SUCCESS) {
		mutex_exit(&port->fp_mutex);
		return (DDI_FAILURE);
	}

	/*
	 * If there are commands queued for delayed retry, instead of
	 * working the hard way to figure out which ones are good for
	 * restart and which ones not (ELSs are definitely not good
	 * as the port will go through a new round of discovery now)
	 * just flush them out.
	 */
	if (port->fp_restore & FP_RESTORE_WAIT_TIMEOUT) {
		fp_cmd_t	*cmd;

		port->fp_restore &= ~FP_RESTORE_WAIT_TIMEOUT;

		mutex_exit(&port->fp_mutex);
		while ((cmd = fp_deque_cmd(port)) != NULL) {
			cmd->cmd_pkt.pkt_state = FC_PKT_TRAN_ERROR;
			fp_iodone(cmd);
		}
		mutex_enter(&port->fp_mutex);
	}

	if (FC_PORT_STATE_MASK(port->fp_bind_state) == FC_STATE_OFFLINE) {
		if ((port->fp_restore & FP_RESTORE_OFFLINE_TIMEOUT) ||
		    port->fp_dev_count) {
			timeout_id_t 	tid;

			port->fp_restore &= ~FP_RESTORE_OFFLINE_TIMEOUT;
			mutex_exit(&port->fp_mutex);
			tid = timeout(fp_offline_timeout, (caddr_t)port,
			    fp_offline_ticks);
			mutex_enter(&port->fp_mutex);
			port->fp_offline_tid = tid;
		}
		fp_attach_ulps(port, cmd);
	} else {
		struct job_request *job;
		void (*cb) (opaque_t, uchar_t);

		/*
		 * If an OFFLINE timer was running at the time of
		 * suspending, there is no need to restart it as
		 * the port is ONLINE now.
		 */
		port->fp_restore &= ~FP_RESTORE_OFFLINE_TIMEOUT;
		if (port->fp_statec_busy == 0) {
			port->fp_soft_state |= FP_SOFT_IN_STATEC_CB;
		}
		port->fp_statec_busy++;
		mutex_exit(&port->fp_mutex);

		if (cmd == DDI_RESUME) {
			cb = fp_resume_done;
		} else {
			ASSERT(cmd == DDI_PM_RESUME);
			cb = fp_pm_resume_done;
		}

		job = fctl_alloc_job(JOB_PORT_ONLINE, JOB_TYPE_FCTL_ASYNC |
		    JOB_CANCEL_ULP_NOTIFICATION, cb, (opaque_t)port, KM_SLEEP);

		fctl_enque_job(port, job);

		mutex_enter(&port->fp_mutex);
	}
	mutex_exit(&port->fp_mutex);

	return (DDI_SUCCESS);
}


/* ARGSUSED */
static void
fp_pm_resume_done(opaque_t arg, uchar_t result)
{
	fc_port_t *port = (fc_port_t *)arg;

	mutex_enter(&port->fp_mutex);
	FP_RESTORE_TASK(port);
	mutex_exit(&port->fp_mutex);

	fctl_remove_oldies(port);

	fp_attach_ulps(port, DDI_PM_RESUME);
	FP_TNF_PROBE_1((fp_pm_resume_done, "fp_dr_trace", "FP DR",
	    tnf_opaque, port_ptr, port));
}


/* ARGSUSED */
static void
fp_resume_done(opaque_t arg, uchar_t result)
{
	fc_port_t *port = (fc_port_t *)arg;

	mutex_enter(&port->fp_mutex);
	FP_RESTORE_TASK(port);
	mutex_exit(&port->fp_mutex);

	fctl_remove_oldies(port);

	fp_attach_ulps(port, DDI_RESUME);
	FP_TNF_PROBE_1((fp_resume_done, "fp_dr_trace", "FP DR",
	    tnf_opaque, port_ptr, port));
}


/*
 * At this time, there shouldn't be any I/O requests on this port.
 * But the unsolicited callbacks from the underlying FCA port need
 * to be handled very carefully. The steps followed to handle the
 * DDI_DETACH are:
 *	+ 	Grab the port driver mutex, check if the unsolicited
 *		callback is currently under processing. If true, fail
 *		the DDI_DETACH request by printing a message; If false
 *		mark the DDI_DETACH as under progress, so that any
 *		further unsolicited callbacks get bounced.
 *	+	Perform PRLO/LOGO if necessary, cleanup all the data
 *		structures.
 *	+	Get the job_handler thread to gracefully exit.
 *	+	Unregister callbacks with the FCA port.
 *	+	Now that some peace is found, notify all the ULPs of
 *		DDI_DETACH request (using ulp_port_detach entry point)
 *	+	Free all mutexes, semaphores, conditional variables.
 *	+	Free the soft state, return success.
 *
 * Important considerations:
 *		Port driver de-registeres state change and unsolicited
 *		callbacks before taking up the task of notfying ULPs
 *		and peforming PRLO and LOGOs.
 *
 *		A port may go offline at the time PRLO/LOGO is being
 *		requested. It is expected of all FCA drivers to fail
 *		such requests either immediately with a FC_OFFLINE
 *		return code to fc_fca_transport() or return the packet
 *		asynchronously with pkt state set to FC_PKT_PORT_OFFLINE
 */
static int
fp_detach_handler(fc_port_t *port)
{
	int		count;
	job_request_t 	*job;
	uint32_t	delay_count;

	/*
	 * In a Fabric topology with many host ports connected to
	 * a switch, another detaching instance of fp might have
	 * triggered a LOGO (which is an unsolicited request to
	 * this instance). So in order to be able to successfully
	 * detach by taking care of such cases a delay of about
	 * 30 seconds is introduced.
	 */
	delay_count = 0;
	mutex_enter(&port->fp_mutex);
	while ((port->fp_soft_state & FP_SOFT_IN_STATEC_CB) ||
	    (port->fp_soft_state & FP_SOFT_IN_UNSOL_CB) &&
	    (delay_count < 30)) {
		mutex_exit(&port->fp_mutex);
		delay_count++;
		delay(drv_usectohz(1000000));
		mutex_enter(&port->fp_mutex);
	}

	if (delay_count == 30) {
		mutex_exit(&port->fp_mutex);
		cmn_err(CE_WARN, "fp(%d): FCA callback in progress: "
		    " Failing detach", port->fp_instance);
		return (DDI_FAILURE);
	}

	port->fp_soft_state |= FP_SOFT_IN_DETACH;
	mutex_exit(&port->fp_mutex);

	job = fctl_alloc_job(JOB_PORT_SHUTDOWN, 0, NULL,
	    (opaque_t)port, KM_SLEEP);

	fctl_enque_job(port, job);
	fctl_jobwait(job);
	if (job->job_result != FC_SUCCESS) {
		fctl_dealloc_job(job);
		mutex_enter(&port->fp_mutex);
		port->fp_soft_state &= ~FP_SOFT_IN_DETACH;
		mutex_exit(&port->fp_mutex);
		cmn_err(CE_WARN, "fp(%d): Can't shutdown port "
		    " Failing detach", port->fp_instance);
		return (DDI_FAILURE);
	}
	fctl_dealloc_job(job);

	ddi_prop_remove_all(port->fp_port_dip);

	ddi_remove_minor_node(port->fp_port_dip, NULL);

	fctl_remove_port(port);

	fp_free_pkt(port->fp_els_resp_pkt);

	if (port->fp_ub_tokens) {
		if (fc_ulp_ubfree(port, port->fp_ub_count,
		    port->fp_ub_tokens) != FC_SUCCESS) {
			cmn_err(CE_WARN, "fp(%d): couldn't free "
			    " unsolicited buffers", port->fp_instance);
		}
		kmem_free(port->fp_ub_tokens,
		    sizeof (*port->fp_ub_tokens) * port->fp_ub_count);
		port->fp_ub_tokens = NULL;
	}

	mutex_enter(&port->fp_mutex);
	if (FP_PKT_CACHE(port) != NULL) {
		kmem_cache_destroy(FP_PKT_CACHE(port));
	}

	port->fp_fca_tran->fca_unbind_port(port->fp_fca_handle);

	if (port->fp_did_table) {
		kmem_free(port->fp_did_table, did_table_size *
			sizeof (struct d_id_hash));
	}

	if (port->fp_pwwn_table) {
		kmem_free(port->fp_pwwn_table, pwwn_table_size *
		    sizeof (struct pwwn_hash));
	}

	for (count = 0; count < port->fp_ulp_nload; count++) {
		if (port->fp_ulp_majors[count] == (major_t)-1) {
			continue;
		}
		ddi_rele_driver(port->fp_ulp_majors[count]);
	}

	mutex_exit(&port->fp_mutex);
	mutex_destroy(&port->fp_mutex);
	cv_destroy(&port->fp_attach_cv);
	cv_destroy(&port->fp_cv);
	ddi_soft_state_free(fp_state, port->fp_instance);

	return (DDI_SUCCESS);
}


/*
 * Steps to perform DDI_SUSPEND operation on a FC port
 *
 *	- If already suspended return DDI_FAILURE
 *	- If already power-suspended return DDI_SUCCESS
 *	- If an unsolicited callback or state change handling is in
 *	    in progress, throw a warning message, return DDI_FAILURE
 *	- Cancel timeouts
 *	- SUSPEND the job_handler thread (means do nothing as it is
 *          taken care of by the CPR frame work)
 */
static int
fp_suspend_handler(fc_port_t *port)
{
	uint32_t	delay_count;

	mutex_enter(&port->fp_mutex);

	/*
	 * The following should never happen, but
	 * let the driver be more defensive here
	 */
	if (port->fp_soft_state & FP_SOFT_SUSPEND) {
		mutex_exit(&port->fp_mutex);
		return (DDI_FAILURE);
	}

	/*
	 * If the port is already power suspended, there
	 * is nothing else to do, So return DDI_SUCCESS,
	 * but mark the SUSPEND bit in the soft state
	 * before leaving.
	 */
	if (port->fp_soft_state & FP_SOFT_PM_SUSPEND) {
		port->fp_soft_state |= FP_SOFT_SUSPEND;
		mutex_exit(&port->fp_mutex);
		return (DDI_SUCCESS);
	}

	/*
	 * Check if an unsolicited callback or state change handling is
	 * in progress. If true, fail the suspend operation; also throw
	 * a warning message notifying the failure. Note that Sun PCI
	 * hotplug spec recommends messages in cases of failure (but
	 * not flooding the console)
	 *
	 * Busy waiting for a short interval (500 millisecond ?) to see
	 * if the callback processing completes may be another idea. Since
	 * most of the callback processing involves a lot of work, it
	 * is safe to just fail the SUSPEND operation. It is definitely
	 * not bad to fail the SUSPEND operation if the driver is busy.
	 */
	delay_count = 0;
	while ((port->fp_soft_state & FP_SOFT_IN_STATEC_CB) ||
	    (port->fp_soft_state & FP_SOFT_IN_UNSOL_CB) &&
	    (delay_count < 30)) {
		mutex_exit(&port->fp_mutex);
		delay_count++;
		delay(drv_usectohz(1000000));
		mutex_enter(&port->fp_mutex);
	}

	if (delay_count == 30) {
		mutex_exit(&port->fp_mutex);
		cmn_err(CE_WARN, "fp(%d): FCA callback in progress: "
		    " Failing suspend", port->fp_instance);
		return (DDI_FAILURE);
	}

	port->fp_soft_state |= FP_SOFT_SUSPEND;

	fp_suspend_all(port);
	mutex_exit(&port->fp_mutex);

	return (DDI_SUCCESS);
}


static int
fp_pm_suspend_handler(fc_port_t *port)
{
	mutex_enter(&port->fp_mutex);

	/*
	 * DDI_PM_SUSPEND followed by a DDI_SUSPEND should
	 * never happen; If it does return DDI_SUCCESS
	 */
	if (port->fp_soft_state & FP_SOFT_SUSPEND) {
		port->fp_soft_state |= FP_SOFT_PM_SUSPEND;
		mutex_exit(&port->fp_mutex);
		return (DDI_SUCCESS);
	}

	/*
	 * If the port is already power suspended, there
	 * is nothing else to do, So return DDI_SUCCESS,
	 */
	if (port->fp_soft_state & FP_SOFT_PM_SUSPEND) {
		mutex_exit(&port->fp_mutex);
		return (DDI_SUCCESS);
	}

	/*
	 * Check if an unsolicited callback or state change handling
	 * is in progress. If true, fail the PM suspend operation.
	 * But don't print a message unless the verbosity of the
	 * driver desires otherwise.
	 */
	if ((port->fp_soft_state & FP_SOFT_IN_STATEC_CB) ||
	    (port->fp_soft_state & FP_SOFT_IN_UNSOL_CB)) {
		mutex_exit(&port->fp_mutex);
		fp_printf(port, CE_WARN, FP_LOG_ONLY, 0, NULL,
		    "Unsolicited callback in progress: Failing PM_SUSPEND");
		return (DDI_FAILURE);
	}
	port->fp_soft_state |= FP_SOFT_PM_SUSPEND;
	fp_suspend_all(port);
	mutex_exit(&port->fp_mutex);

	return (DDI_SUCCESS);
}


static void
fp_suspend_all(fc_port_t *port)
{
	int			index;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	if (port->fp_wait_tid != 0) {
		timeout_id_t 	tid;

		tid = port->fp_wait_tid;
		port->fp_wait_tid = (timeout_id_t)NULL;
		mutex_exit(&port->fp_mutex);
		(void) untimeout(tid);
		mutex_enter(&port->fp_mutex);
		port->fp_restore |= FP_RESTORE_WAIT_TIMEOUT;
	}

	if (port->fp_offline_tid) {
		timeout_id_t 	tid;

		tid = port->fp_offline_tid;
		port->fp_offline_tid = (timeout_id_t)NULL;
		mutex_exit(&port->fp_mutex);
		(void) untimeout(tid);
		mutex_enter(&port->fp_mutex);
		port->fp_restore |= FP_RESTORE_OFFLINE_TIMEOUT;
	}
	mutex_exit(&port->fp_mutex);
	port->fp_fca_tran->fca_unbind_port(port->fp_fca_handle);
	mutex_enter(&port->fp_mutex);

	/*
	 * Mark all devices as OLD, and reset the LOGIN state as well
	 * (this will force the ULPs to perform a LOGIN after calling
	 * fc_portgetmap() during RESUME/PM_RESUME)
	 */
	for (index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;
		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			fp_port_device_offline(pd);
			fctl_delist_did_table(port, pd);
			pd->pd_state = PORT_DEVICE_VALID;
			pd->pd_count = 0;
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}
}


/*
 * The cache constructor routine allocates DMA handles for both
 * command and responses (Most of the ELSs used have both command
 * and responses so it is strongly desired to move them to cache
 * constructor routine)
 */
static int
fp_cache_constructor(void *buf, void *cdarg, int kmflags)
{
	int 		(*cb) (caddr_t);
	fc_packet_t 	*pkt;
	fp_cmd_t 	*cmd = (fp_cmd_t *)buf;
	fc_port_t 	*port = (fc_port_t *)cdarg;

	cb = (kmflags == KM_SLEEP) ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT;

	cmd->cmd_next = NULL;
	cmd->cmd_flags = 0;
	cmd->cmd_dflags = 0;
	cmd->cmd_job = NULL;
	FP_CMD_TO_PORT(cmd) = port;

	pkt = FP_CMD_TO_PKT(cmd);

	if (ddi_dma_alloc_handle(port->fp_fca_dip,
	    port->fp_fca_tran->fca_dma_attr, cb, NULL,
	    &pkt->pkt_cmd_dma) != DDI_SUCCESS) {
		return (FP_FAILURE);
	}

	if (ddi_dma_alloc_handle(port->fp_fca_dip,
	    port->fp_fca_tran->fca_dma_attr, cb, NULL,
	    &pkt->pkt_resp_dma) != DDI_SUCCESS) {
		ddi_dma_free_handle(&pkt->pkt_cmd_dma);
		return (FP_FAILURE);
	}

	pkt->pkt_cmd_acc = pkt->pkt_resp_acc = NULL;
	pkt->pkt_fca_private = (caddr_t)buf + sizeof (fp_cmd_t);

	/*
	 * Get FCA initialize its packet private fields
	 */
	if (fc_ulp_init_packet((opaque_t)port, pkt, kmflags) != FC_SUCCESS) {
		ddi_dma_free_handle(&pkt->pkt_resp_dma);
		ddi_dma_free_handle(&pkt->pkt_cmd_dma);
		return (FP_FAILURE);
	}

	return (FP_SUCCESS);
}


static void
fp_cache_destructor(void *buf, void *cdarg)
{
	int		rval;
	fp_cmd_t 	*cmd = (fp_cmd_t *)buf;
	fc_port_t 	*port = (fc_port_t *)cdarg;
	fc_packet_t	*pkt;

	pkt = FP_CMD_TO_PKT(cmd);
	if (pkt->pkt_cmd_dma) {
		ddi_dma_free_handle(&pkt->pkt_cmd_dma);
	}

	if (pkt->pkt_resp_dma) {
		ddi_dma_free_handle(&pkt->pkt_resp_dma);
	}

	rval = fc_ulp_uninit_packet((opaque_t)port, pkt);
	ASSERT(rval == FC_SUCCESS);
}


/*
 * Packet allocation for ELS and any other port driver commands
 *
 *	Some ELSs like FLOGI and PLOGI are critical for topology and
 *	device discovery and a system's inability to allocate memory
 *	or DVMA resources while performing some of these critical ELSs
 *	cause a lot of problem. While memory allocation failures are
 *	rare, DVMA resource failures are common as the applications
 *	are becoming more and more powerful on huge servers.  So it
 *	is desirable to have a framework support to reserve a fragment
 *	of DVMA. So until this is fixed the correct way, the suffering
 *	is huge whenever a LIP happens at a time DVMA resources are
 *	drained out completely - So an attempt needs to be made to
 *	KM_SLEEP while requesting for these resources, hoping that
 *	the requests won't hang forever.
 */
static fp_cmd_t *
fp_alloc_pkt(fc_port_t *port, int cmd_len, int resp_len, int kmflags)
{
	int		rval;
	ulong_t		real_len;
	uint_t		num_cookie;
	fp_cmd_t 	*cmd;
	fc_packet_t	*pkt;
	int 		(*cb) (caddr_t);

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	cb = (kmflags == KM_SLEEP) ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT;

	cmd = (fp_cmd_t *)kmem_cache_alloc(FP_PKT_CACHE(port), kmflags);
	if (cmd == NULL) {
		return (cmd);
	}
	cmd->cmd_ulp_pkt = NULL;
	cmd->cmd_flags = 0;
	pkt = FP_CMD_TO_PKT(cmd);
	ASSERT(cmd->cmd_dflags == 0);

	/*
	 * zero out important pkt fields
	 */
	pkt->pkt_datalen = 0;
	pkt->pkt_data = NULL;
	pkt->pkt_state = 0;
	pkt->pkt_action = 0;
	pkt->pkt_reason = 0;
	pkt->pkt_expln = 0;
	pkt->pkt_pd = NULL;

	if (cmd_len) {
		ASSERT(pkt->pkt_cmd_dma != NULL);

		rval = ddi_dma_mem_alloc(pkt->pkt_cmd_dma, cmd_len,
		    port->fp_fca_tran->fca_acc_attr, DDI_DMA_CONSISTENT,
		    cb, NULL, (caddr_t *)&pkt->pkt_cmd, &real_len,
		    &pkt->pkt_cmd_acc);

		if (rval != DDI_SUCCESS) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
		cmd->cmd_dflags |= FP_CMD_VALID_DMA_MEM;

		if (real_len < cmd_len) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}

		rval = ddi_dma_addr_bind_handle(pkt->pkt_cmd_dma, NULL,
		    pkt->pkt_cmd, real_len, DDI_DMA_WRITE |
		    DDI_DMA_CONSISTENT, cb, NULL,
		    &pkt->pkt_cmd_cookie, &num_cookie);

		if (rval != DDI_DMA_MAPPED) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
		cmd->cmd_dflags |= FP_CMD_VALID_DMA_BIND;

		if (num_cookie != 1) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
	}

	if (resp_len) {
		ASSERT(pkt->pkt_resp_dma != NULL);

		rval = ddi_dma_mem_alloc(pkt->pkt_resp_dma, resp_len,
		    port->fp_fca_tran->fca_acc_attr,
		    DDI_DMA_CONSISTENT, cb, NULL,
		    (caddr_t *)&pkt->pkt_resp, &real_len,
		    &pkt->pkt_resp_acc);

		if (rval != DDI_SUCCESS) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
		cmd->cmd_dflags |= FP_RESP_VALID_DMA_MEM;

		if (real_len < resp_len) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}

		rval = ddi_dma_addr_bind_handle(pkt->pkt_resp_dma, NULL,
		    pkt->pkt_resp, real_len, DDI_DMA_READ |
		    DDI_DMA_CONSISTENT, cb, NULL,
		    &pkt->pkt_resp_cookie, &num_cookie);

		if (rval != DDI_DMA_MAPPED) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
		cmd->cmd_dflags |= FP_RESP_VALID_DMA_BIND;

		if (num_cookie != 1) {
			fp_alloc_pkt_CLEANUP(port, cmd);
			return (NULL);
		}
	}

	FP_INIT_CMD_RESP(pkt, cmd_len, resp_len);
	pkt->pkt_ulp_private = (opaque_t)cmd;

	return (cmd);
}


static void
fp_free_pkt(fp_cmd_t *cmd)
{
	fc_port_t 	*port;
	fc_packet_t 	*pkt;

	ASSERT(!MUTEX_HELD(&cmd->cmd_port->fp_mutex));

	cmd->cmd_next = NULL;
	cmd->cmd_job = NULL;
	pkt = FP_CMD_TO_PKT(cmd);
	pkt->pkt_ulp_private = 0;
	pkt->pkt_tran_flags = 0;
	pkt->pkt_tran_type = 0;
	port = FP_CMD_TO_PORT(cmd);

	fp_free_dma(cmd);
	kmem_cache_free(FP_PKT_CACHE(port), (void *)cmd);
}


static void
fp_free_dma(fp_cmd_t *cmd)
{
	fc_packet_t *pkt = FP_CMD_TO_PKT(cmd);

	pkt->pkt_cmdlen = 0;
	pkt->pkt_rsplen = 0;
	pkt->pkt_tran_type = 0;
	pkt->pkt_tran_flags = 0;

	if (cmd->cmd_dflags & FP_CMD_VALID_DMA_BIND) {
		(void) ddi_dma_unbind_handle(pkt->pkt_cmd_dma);
	}

	if (cmd->cmd_dflags & FP_CMD_VALID_DMA_MEM) {
		if (pkt->pkt_cmd_acc) {
			ddi_dma_mem_free(&pkt->pkt_cmd_acc);
		}
	}

	if (cmd->cmd_dflags & FP_RESP_VALID_DMA_BIND) {
		(void) ddi_dma_unbind_handle(pkt->pkt_resp_dma);
	}

	if (cmd->cmd_dflags & FP_RESP_VALID_DMA_MEM) {
		if (pkt->pkt_resp_acc) {
			ddi_dma_mem_free(&pkt->pkt_resp_acc);
		}
	}
	cmd->cmd_dflags = 0;
}


static void
fp_job_handler(fc_port_t *port)
{
	int			rval;
	uint32_t		*d_id;
	callb_cpr_t		cpr_info;
	fc_port_device_t 	*pd;
	job_request_t 		*job;

#if !defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, cpr_info))
#endif /* lint */

#ifndef	__lock_lint
	CALLB_CPR_INIT(&cpr_info, &port->fp_mutex,
	    callb_generic_cpr, "fp_job_handler");
#endif /* __lock_lint */

	for (;;) {
		mutex_enter(&port->fp_mutex);
		CALLB_CPR_SAFE_BEGIN(&cpr_info);

		while (port->fp_job_head == NULL) {
			cv_wait(&port->fp_cv, &port->fp_mutex);
		}
		CALLB_CPR_SAFE_END(&cpr_info, &port->fp_mutex);

		job = fctl_deque_job(port);

		switch (job->job_code) {
		case JOB_PORT_SHUTDOWN:
			if (fp_port_shutdown(port, job) != FC_SUCCESS) {
				mutex_exit(&port->fp_mutex);
				break;
			}
			fp_job_handler_EXIT(port, cpr_info);
			/* NOTREACHED */

		case JOB_ATTACH_ULP:
			mutex_exit(&port->fp_mutex);
			FP_ATTACH_ULPS(port);
			job->job_result = FC_SUCCESS;
			fctl_jobdone(job);
			break;

		case JOB_ULP_NOTIFY:
		{
			uint32_t statec;

			statec = job->job_ulp_listlen;
			if (statec == FC_STATE_RESET_REQUESTED) {
				FP_SET_TASK(port, FP_TASK_OFFLINE);
				fp_port_offline(port, 0);
				FP_RESTORE_TASK(port);
			}
			FC_STATEC_DONE(port);
			mutex_exit(&port->fp_mutex);

			job->job_result = fp_ulp_notify(port, statec, KM_SLEEP);
			fctl_jobdone(job);
			break;
		}

		case JOB_PLOGI_ONE:
			mutex_exit(&port->fp_mutex);
			d_id = (uint32_t *)job->job_private;
			pd = fctl_get_port_device_by_did(port, *d_id);

			if (pd) {
				mutex_enter(&pd->pd_mutex);
				if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
					pd->pd_count++;
					mutex_exit(&pd->pd_mutex);
					job->job_result = FC_SUCCESS;
					fctl_jobdone(job);
					break;
				}
				mutex_exit(&pd->pd_mutex);
			} else {
				mutex_enter(&port->fp_mutex);
				if (FC_IS_TOP_SWITCH(port->fp_topology)) {
					mutex_exit(&port->fp_mutex);
					pd = fp_create_port_device_by_ns(port,
					    *d_id, KM_SLEEP);
					if (pd == NULL) {
						job->job_result = FC_FAILURE;
						fctl_jobdone(job);
						break;
					}
				} else {
					mutex_exit(&port->fp_mutex);
				}
			}

			job->job_flags |= JOB_TYPE_FP_ASYNC;
			FCTL_SET_JOB_COUNTER(job, 1);
			rval = fp_port_login(port, *d_id, job,
			    FP_CMD_PLOGI_RETAIN, KM_SLEEP, pd, NULL);

			if (rval != FC_SUCCESS) {
				job->job_result = rval;
				fctl_jobdone(job);
			}
			break;

		case JOB_LOGO_ONE:
		{
			fc_port_device_t *pd;

#ifndef	__lock_lint
			ASSERT(job->job_counter > 0);
#endif /* __lock_lint */

			pd = (fc_port_device_t *)job->job_ulp_pkts;

			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
				mutex_exit(&pd->pd_mutex);
				job->job_result = FC_LOGINREQ;
				mutex_exit(&port->fp_mutex);
				fctl_jobdone(job);
				break;
			}
			if (pd->pd_count > 1) {
				pd->pd_count--;
				mutex_exit(&pd->pd_mutex);
				job->job_result = FC_SUCCESS;
				mutex_exit(&port->fp_mutex);
				fctl_jobdone(job);
				break;
			}
			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);
			job->job_flags |= JOB_TYPE_FP_ASYNC;
			(void) fp_logout(port, pd, job);
			break;
		}

		case JOB_PORT_GETMAP:
		case JOB_PORT_GETMAP_PLOGI_ALL:
		{
			FP_SET_TASK(port, FP_TASK_GETMAP);

			switch (port->fp_topology) {
			case FC_TOP_PRIVATE_LOOP:
				FCTL_SET_JOB_COUNTER(job, 1);
				fp_get_loopmap(port, job);
				mutex_exit(&port->fp_mutex);
				fp_jobwait(job);
				if (job->job_result == FC_SUCCESS) {
					fctl_fillout_map(port,
					    (fc_portmap_t **)job->job_private,
					    (uint32_t *)job->job_arg, 1);
				}
				fctl_jobdone(job);
				mutex_enter(&port->fp_mutex);
				break;

			case FC_TOP_PUBLIC_LOOP:
			case FC_TOP_FABRIC:
				mutex_exit(&port->fp_mutex);
				FCTL_SET_JOB_COUNTER(job, 1);
				job->job_result = fp_ns_getmap(port,
				    job, (fc_portmap_t **)job->job_private,
				    (uint32_t *)job->job_arg);
				fctl_jobdone(job);
				mutex_enter(&port->fp_mutex);
				break;

			case FC_TOP_PT_PT:
			default:
				mutex_exit(&port->fp_mutex);
				fctl_jobdone(job);
				mutex_enter(&port->fp_mutex);
				break;
			}
			FP_RESTORE_TASK(port);
			mutex_exit(&port->fp_mutex);
			break;
		}

		case JOB_PORT_OFFLINE:
		{
			FP_SET_TASK(port, FP_TASK_OFFLINE);

			if (port->fp_statec_busy > 2) {
				job->job_flags |= JOB_CANCEL_ULP_NOTIFICATION;
				fp_port_offline(port, 0);
				FC_STATEC_DONE(port);
			} else {
				fp_port_offline(port, 1);
			}

			FP_RESTORE_TASK(port);
			mutex_exit(&port->fp_mutex);
			fctl_jobdone(job);
			break;
		}

		case JOB_PORT_STARTUP:
		{
			if ((rval = fp_port_startup(port, job)) != FC_SUCCESS) {
				if (port->fp_statec_busy) {
					mutex_exit(&port->fp_mutex);
					break;
				}
				mutex_exit(&port->fp_mutex);

				fp_printf(port, CE_WARN, FP_LOG_ONLY, rval,
				    NULL, "Topology discovery failed");
				break;
			}

			/*
			 * Attempt building device handles in case
			 * of private Loop.
			 */
			if (port->fp_topology == FC_TOP_PRIVATE_LOOP) {
				FCTL_SET_JOB_COUNTER(job, 1);
				fp_get_loopmap(port, job);
				mutex_exit(&port->fp_mutex);
				fp_jobwait(job);
				mutex_enter(&port->fp_mutex);
				if (port->fp_lilp_map.lilp_magic < MAGIC_LIRP) {
					ASSERT(port->fp_total_devices == 0);
					port->fp_total_devices =
					    port->fp_dev_count;
				}
			}
			mutex_exit(&port->fp_mutex);
			fctl_jobdone(job);
			break;
		}

		case JOB_PORT_ONLINE:
		{
			char 	*newtop;
			char 	*oldtop;

			/*
			 * Bail out early if there are a lot of
			 * state changes in the pipeline
			 */
			if (port->fp_statec_busy > 1) {
				FC_STATEC_DONE(port);
				mutex_exit(&port->fp_mutex);
				fctl_jobdone(job);
				break;
			}

			switch (port->fp_topology) {
			case FC_TOP_PRIVATE_LOOP:
				oldtop = "Private Loop";
				break;

			case FC_TOP_PUBLIC_LOOP:
				oldtop = "Public Loop";
				break;

			case FC_TOP_PT_PT:
				oldtop = "Point to Point";
				break;

			case FC_TOP_FABRIC:
				oldtop = "Fabric";
				break;

			default:
				oldtop = NULL;
				break;
			}

			FP_SET_TASK(port, FP_TASK_ONLINE);
			if ((rval = fp_port_startup(port, job)) != FC_SUCCESS) {
				FP_RESTORE_TASK(port);
				if (port->fp_statec_busy > 1) {
					FC_STATEC_DONE(port);
					mutex_exit(&port->fp_mutex);
					break;
				}

				port->fp_state = FC_STATE_OFFLINE;
				fp_printf(port, CE_WARN, FP_LOG_ONLY, rval,
				    NULL, "Topology discovery failed");

				FC_STATEC_DONE(port);

				if (port->fp_offline_tid == NULL) {
					timeout_id_t	tid;

					mutex_exit(&port->fp_mutex);
					tid = timeout(fp_offline_timeout,
					    (caddr_t)port, fp_offline_ticks);
					mutex_enter(&port->fp_mutex);
					port->fp_offline_tid = tid;
				}
				mutex_exit(&port->fp_mutex);
				break;
			}

			switch (port->fp_topology) {
			case FC_TOP_PRIVATE_LOOP:
				newtop = "Private Loop";
				break;

			case FC_TOP_PUBLIC_LOOP:
				newtop = "Public Loop";
				break;

			case FC_TOP_PT_PT:
				newtop = "Point to Point";
				break;

			case FC_TOP_FABRIC:
				newtop = "Fabric";
				break;

			default:
				newtop = NULL;
				break;
			}

			if (oldtop && newtop && strcmp(oldtop, newtop)) {
				fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
				    "Change in FC Topology old = %s new = %s",
				    oldtop, newtop);
			}

			switch (port->fp_topology) {
			case FC_TOP_PRIVATE_LOOP:
				mutex_exit(&port->fp_mutex);
				fp_loop_online(port, job);
				break;

			case FC_TOP_PUBLIC_LOOP:
				/* FALLTHROUGH */
			case FC_TOP_FABRIC:
				fp_fabric_online(port, job);
				mutex_exit(&port->fp_mutex);
				break;

			case FC_TOP_PT_PT:
				fp_pt_pt_online(port, job);
				mutex_exit(&port->fp_mutex);
				break;

			default:
				if (--port->fp_statec_busy == 0) {
					port->fp_soft_state &=
					    ~FP_SOFT_IN_STATEC_CB;
				} else {
					/*
					 * Watch curiously at what the next
					 * state transition can do.
					 */
					mutex_exit(&port->fp_mutex);
					break;
				}

				fp_printf(port, CE_WARN, FP_LOG_ONLY, 0, NULL,
				    "Topology Unknown, Offlining the port..");

				port->fp_state = FC_STATE_OFFLINE;
				if (port->fp_offline_tid == NULL) {
					timeout_id_t	tid;

					mutex_exit(&port->fp_mutex);
					tid = timeout(fp_offline_timeout,
					    (caddr_t)port, fp_offline_ticks);
					mutex_enter(&port->fp_mutex);
					port->fp_offline_tid = tid;
				}
				mutex_exit(&port->fp_mutex);
				break;
			}
			mutex_enter(&port->fp_mutex);
			FP_RESTORE_TASK(port);
			mutex_exit(&port->fp_mutex);
			fctl_jobdone(job);
			break;
		}

		case JOB_PLOGI_GROUP:
		{
			mutex_exit(&port->fp_mutex);
			fp_plogi_group(port, job);
			break;
		}

		case JOB_UNSOL_REQUEST:
		{
			mutex_exit(&port->fp_mutex);
			fp_handle_unsol_buf(port,
			    (fc_unsol_buf_t *)job->job_private, job);
			fctl_dealloc_job(job);
			break;
		}

		case JOB_NS_CMD:
		{
			fctl_ns_req_t *ns_cmd;

			mutex_exit(&port->fp_mutex);

			job->job_flags |= JOB_TYPE_FP_ASYNC;
			ns_cmd = (fctl_ns_req_t *)job->job_private;
			if (ns_cmd->ns_cmd_code < NS_GA_NXT ||
			    ns_cmd->ns_cmd_code > NS_DA_ID) {
				job->job_result = FC_BADCMD;
				fctl_jobdone(job);
				break;
			}

			if (FC_IS_CMD_A_REG(ns_cmd->ns_cmd_code)) {
				if (ns_cmd->ns_pd != NULL) {
					job->job_result = FC_BADOBJECT;
					fctl_jobdone(job);
					break;
				}

				rval = fp_ns_reg(port, ns_cmd->ns_pd,
				    ns_cmd->ns_cmd_code, job, 0, KM_SLEEP);

				FCTL_SET_JOB_COUNTER(job, 1);
				if (rval != FC_SUCCESS) {
					job->job_result = rval;
					fctl_jobdone(job);
				}
				break;
			}
			job->job_result = FC_SUCCESS;

			FCTL_SET_JOB_COUNTER(job, 1);
			rval = fp_ns_query(port, ns_cmd, job, 0, KM_SLEEP);
			if (rval != FC_SUCCESS) {
				fctl_jobdone(job);
			}
			break;
		}

		case JOB_LINK_RESET:
		{
			la_wwn_t *pwwn;
			uint32_t topology;

			pwwn = (la_wwn_t *)job->job_private;
			ASSERT(pwwn != NULL);

			topology = port->fp_topology;
			mutex_exit(&port->fp_mutex);

			if (fctl_is_wwn_zero(pwwn) == FC_SUCCESS ||
			    topology == FC_TOP_PRIVATE_LOOP) {
				job->job_flags |= JOB_TYPE_FP_ASYNC;
				rval = port->fp_fca_tran->fca_reset(
				    port->fp_fca_handle, FC_FCA_LINK_RESET);
				job->job_result = rval;
				fp_jobdone(job);
			} else {
				ASSERT((job->job_flags &
				    JOB_TYPE_FP_ASYNC) == 0);

				if (FC_IS_TOP_SWITCH(topology)) {
					rval = fp_remote_lip(port, pwwn,
					    KM_SLEEP, job);
				} else {
					rval = FC_FAILURE;
				}
				if (rval != FC_SUCCESS) {
					job->job_result = rval;
					fctl_jobdone(job);
				}
			}
			break;
		}

		default:
			mutex_exit(&port->fp_mutex);
			job->job_result = FC_BADCMD;
			fctl_jobdone(job);
			break;
		}
	}
	/* NOTREACHED */
}


static int
fp_port_startup(fc_port_t *port, job_request_t *job)
{
	int		rval;
	uint32_t	state;
	uint32_t	src_id;
	fc_lilpmap_t 	*lilp_map;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	FP_TNF_RELEASE_LOCK((&port->fp_mutex));
	FP_TNF_PROBE_2((fp_port_startup, "fp_startup_trace",
	    "Entering fp_port_startup",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));
	FP_TNF_HOLD_LOCK((&port->fp_mutex));

	port->fp_topology = FC_TOP_UNKNOWN;
	state = FC_PORT_STATE_MASK(port->fp_state);

	if (state == FC_STATE_OFFLINE) {
		port->fp_port_type.port_type = FC_NS_PORT_UNKNOWN;
		job->job_result = FC_OFFLINE;
		mutex_exit(&port->fp_mutex);
		fctl_jobdone(job);
		mutex_enter(&port->fp_mutex);
		return (FC_OFFLINE);
	}

	if (state == FC_STATE_LOOP) {
		port->fp_port_type.port_type = FC_NS_PORT_NL;
		mutex_exit(&port->fp_mutex);

		lilp_map = &port->fp_lilp_map;
		if ((rval = fp_get_lilpmap(port, lilp_map)) != FC_SUCCESS) {
			job->job_result = FC_FAILURE;
			fctl_jobdone(job);
			fp_printf(port, CE_WARN, FP_LOG_ONLY, rval, NULL,
			    "LILP map Invalid or not present");
			mutex_enter(&port->fp_mutex);
			return (FC_FAILURE);
		}

		if (lilp_map->lilp_length == 0) {
			job->job_result = FC_NO_MAP;
			fctl_jobdone(job);
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    "LILP map length zero");
			mutex_enter(&port->fp_mutex);
			return (FC_NO_MAP);
		}
		src_id = lilp_map->lilp_myalpa & 0xFF;
	} else {
		port->fp_port_type.port_type = FC_NS_PORT_N;
		mutex_exit(&port->fp_mutex);
		src_id = 0;
	}

	FCTL_SET_JOB_COUNTER(job, 1);
	job->job_result = FC_SUCCESS;

	if ((rval = fp_fabric_login(port, src_id, job, FP_CMD_PLOGI_DONT_CARE,
	    KM_SLEEP)) != FC_SUCCESS) {
		port->fp_port_type.port_type = FC_NS_PORT_UNKNOWN;
		job->job_result = FC_FAILURE;
		fctl_jobdone(job);

		mutex_enter(&port->fp_mutex);
		if (port->fp_statec_busy == 1) {
			mutex_exit(&port->fp_mutex);
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, rval, NULL,
			    "Couldn't transport FLOGI");
			mutex_enter(&port->fp_mutex);
		}
		return (FC_FAILURE);
	}
	fp_jobwait(job);

	mutex_enter(&port->fp_mutex);
	if (job->job_result == FC_SUCCESS) {
		if (FC_IS_TOP_SWITCH(port->fp_topology)) {
			mutex_exit(&port->fp_mutex);
			fp_ns_init(port, job, KM_SLEEP);
			mutex_enter(&port->fp_mutex);
		}
	} else {
		if (state == FC_STATE_LOOP) {
			port->fp_topology = FC_TOP_PRIVATE_LOOP;
			port->FP_PORT_ID = port->fp_lilp_map.lilp_myalpa & 0xFF;
		}
	}

	FP_TNF_RELEASE_LOCK((&port->fp_mutex));
	FP_TNF_PROBE_2((fp_port_startup, "fp_startup_trace",
	    "Exiting fp_port_startup",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));
	FP_TNF_HOLD_LOCK((&port->fp_mutex));

	return (FC_SUCCESS);
}


/* ARGSUSED */
static void
fp_startup_done(opaque_t arg, uchar_t result)
{
	fc_port_t *port = (fc_port_t *)arg;

	mutex_enter(&port->fp_mutex);
	FP_RESTORE_TASK(port);
	mutex_exit(&port->fp_mutex);

	fp_attach_ulps(port, DDI_ATTACH);
	FP_TNF_PROBE_1((fp_startup_done, "fp_startup_trace",
	    "fp_startup almost complete",
	    tnf_opaque, port_ptr, port));
}


/*
 * Returning a value that isn't equal to ZERO will
 * result in the callback being rescheduled again.
 */
static int
fp_ulp_port_attach(caddr_t arg)
{
	fp_soft_attach_t *att = (fp_soft_attach_t *)arg;

	FP_TNF_PROBE_2((fp_ulp_port_attach, "fp_startup_trace",
	    "port attach of ULPs begin",
	    tnf_opaque, 	port_ptr, 	att->att_port,
	    tnf_int, 		command, 	att->att_cmd));

	fctl_attach_ulps(att->att_port, att->att_cmd, &modlinkage);

	FP_TNF_PROBE_2((fp_ulp_port_attach, "fp_startup_trace",
	    "port attach of ULPs end",
	    tnf_opaque, 	port_ptr, 	att->att_port,
	    tnf_int, 		command, 	att->att_cmd));

	mutex_enter(&att->att_port->fp_mutex);
	att->att_port->fp_ulp_attach = 0;
	cv_signal(&att->att_port->fp_attach_cv);
	mutex_exit(&att->att_port->fp_mutex);

	kmem_free(att, sizeof (fp_soft_attach_t));

	return (1);
}


static int
fp_sendcmd(fc_port_t *port, fp_cmd_t *cmd, opaque_t fca_handle)
{
	int rval;

	mutex_enter(&port->fp_mutex);
	if (port->fp_statec_busy > 1) {
		/*
		 * This means there is more than one state change
		 * at this point of time - Since they are processed
		 * serially, any processing of the current one should
		 * be failed, failed and move up in processing the next
		 */
		cmd->cmd_pkt.pkt_state = FC_PKT_TRAN_BSY;
		if (cmd->cmd_job) {
			/*
			 * A state change that is going to be invalidated
			 * by another one already in the port driver's queue
			 * need not go up to all ULPs. This will minimize
			 * needless processing and ripples in ULP modules
			 */
			cmd->cmd_job->job_flags |= JOB_CANCEL_ULP_NOTIFICATION;
		}
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}

	if (FC_PORT_STATE_MASK(port->fp_state) == FC_STATE_OFFLINE) {
		cmd->cmd_pkt.pkt_state = FC_PKT_PORT_OFFLINE;
		mutex_exit(&port->fp_mutex);
		return (FC_OFFLINE);
	}
	mutex_exit(&port->fp_mutex);

	rval = cmd->cmd_transport(fca_handle, FP_CMD_TO_PKT(cmd));
	if (rval != FC_SUCCESS) {
		if (rval == FC_TRAN_BUSY) {
			cmd->cmd_retry_interval = fp_retry_delay;
			rval = fp_retry_cmd(FP_CMD_TO_PKT(cmd));
			if (rval == FC_FAILURE) {
				cmd->cmd_pkt.pkt_state = FC_PKT_TRAN_BSY;
			}
		}
	}

	return (rval);
}


/*
 * Each time a timeout kicks in, walk the wait queue, decrement the
 * the retry_interval, when the retry_interval becomes less than
 * or equal to zero, re-transport the command: If the re-transport
 * fails with BUSY, enqueue the command in the wait queue.
 *
 * In order to prevent looping forever because of commands enqueued
 * from within this function itself, save the current tail pointer
 * (in cur_tail) and exit the loop after serving this command.
 */
static void
fp_resendcmd(void *port_handle)
{
	int		rval;
	fc_port_t	*port;
	fp_cmd_t 	*cmd;
	fp_cmd_t	*cur_tail;

	port = (fc_port_t *)port_handle;
	mutex_enter(&port->fp_mutex);
	cur_tail = port->fp_wait_tail;
	mutex_exit(&port->fp_mutex);

	while ((cmd = fp_deque_cmd(port)) != NULL) {
		cmd->cmd_retry_interval -= fp_retry_ticker;
		if (cmd->cmd_retry_interval <= 0) {
			rval = cmd->cmd_transport(port->fp_fca_handle,
			    FP_CMD_TO_PKT(cmd));

			if (rval != FC_SUCCESS) {
				if (cmd->cmd_pkt.pkt_state == FC_PKT_TRAN_BSY) {
					if (--cmd->cmd_retry_count) {
						fp_enque_cmd(port, cmd);
						if (cmd == cur_tail) {
							break;
						}
						continue;
					}
					cmd->cmd_pkt.pkt_state =
					    FC_PKT_TRAN_BSY;
				} else {
					cmd->cmd_pkt.pkt_state =
					    FC_PKT_TRAN_ERROR;
				}
				cmd->cmd_pkt.pkt_reason = 0;
				fp_iodone(cmd);
			}
		} else {
			fp_enque_cmd(port, cmd);
		}

		if (cmd == cur_tail) {
			break;
		}
	}

	mutex_enter(&port->fp_mutex);
	if (port->fp_wait_head) {
		timeout_id_t tid;

		mutex_exit(&port->fp_mutex);
		tid = timeout(fp_resendcmd, (caddr_t)port,
		    fp_retry_ticks);
		mutex_enter(&port->fp_mutex);
		port->fp_wait_tid = tid;
	} else {
		port->fp_wait_tid = NULL;
	}
	mutex_exit(&port->fp_mutex);
}


/*
 * Handle Local, Fabric, N_Port, Transport (whatever that means) BUSY here.
 *
 * Yes, as you can see below, cmd_retry_count is used here too.  That means
 * the retries for BUSY are less if there were transport failures (transport
 * failure means fca_transport failure). The goal is not to exceed overall
 * retries set in the cmd_retry_count (whatever may be the reason for retry)
 *
 * Return Values:
 *	FC_SUCCESS
 *	FC_FAILURE
 */
static int
fp_retry_cmd(fc_packet_t *pkt)
{
	fp_cmd_t *cmd;

	cmd = FP_PKT_TO_CMD(pkt);

	if (--cmd->cmd_retry_count) {
		fp_enque_cmd(FP_CMD_TO_PORT(cmd), cmd);
		return (FC_SUCCESS);
	} else {
		return (FC_FAILURE);
	}
}


static void
fp_enque_cmd(fc_port_t *port, fp_cmd_t *cmd)
{
	timeout_id_t tid;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	if (port->fp_wait_tail) {
		port->fp_wait_tail->cmd_next = cmd;
		port->fp_wait_tail = cmd;
	} else {
		ASSERT(port->fp_wait_head == NULL);
		port->fp_wait_head = port->fp_wait_tail = cmd;
		if (port->fp_wait_tid == NULL) {
			mutex_exit(&port->fp_mutex);
			tid = timeout(fp_resendcmd, (caddr_t)port,
			    fp_retry_ticks);
			mutex_enter(&port->fp_mutex);
			port->fp_wait_tid = tid;
		}
	}
	mutex_exit(&port->fp_mutex);
}


static int
fp_handle_reject(fc_packet_t *pkt)
{
	int 		rval = FC_FAILURE;
	uchar_t		next_class;
	fp_cmd_t 	*cmd;

	cmd = FP_PKT_TO_CMD(pkt);

	if (pkt->pkt_reason != FC_REASON_CLASS_NOT_SUPP) {
		if (pkt->pkt_reason == FC_REASON_QFULL) {
			cmd->cmd_retry_interval = fp_retry_delay;
			rval = fp_retry_cmd(pkt);
		}

		return (rval);
	}

	next_class = fp_get_nextclass(FP_CMD_TO_PORT(cmd),
	    FC_TRAN_CLASS(pkt->pkt_tran_flags));

	if (next_class == FC_TRAN_CLASS_INVALID) {
		return (rval);
	}
	pkt->pkt_tran_flags = FC_TRAN_INTR | next_class;
	pkt->pkt_tran_type = FC_PKT_EXCHANGE;

	rval = fp_sendcmd(FP_CMD_TO_PORT(cmd), cmd,
	    cmd->cmd_port->fp_fca_handle);

	if (rval != FC_SUCCESS) {
		pkt->pkt_state = FC_PKT_TRAN_ERROR;
	}

	return (rval);
}


static uchar_t
fp_get_nextclass(fc_port_t *port, uchar_t cur_class)
{
	uchar_t next_class;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	switch (cur_class) {
	case FC_TRAN_CLASS_INVALID:
		if (port->fp_cos & FC_NS_CLASS1) {
			next_class = FC_TRAN_CLASS1;
			break;
		}
		/* FALLTHROUGH */

	case FC_TRAN_CLASS1:
		if (port->fp_cos & FC_NS_CLASS2) {
			next_class = FC_TRAN_CLASS2;
			break;
		}
		/* FALLTHROUGH */

	case FC_TRAN_CLASS2:
		if (port->fp_cos & FC_NS_CLASS3) {
			next_class = FC_TRAN_CLASS3;
			break;
		}
		/* FALLTHROUGH */

	case FC_TRAN_CLASS3:
	default:
		next_class = FC_TRAN_CLASS_INVALID;
		break;
	}

	return (next_class);
}


static int
fp_is_class_supported(uint32_t cos, uchar_t tran_class)
{
	int rval;

	switch (tran_class) {
	case FC_TRAN_CLASS1:
		if (cos & FC_NS_CLASS1) {
			rval = FC_SUCCESS;
		} else {
			rval = FC_FAILURE;
		}
		break;

	case FC_TRAN_CLASS2:
		if (cos & FC_NS_CLASS2) {
			rval = FC_SUCCESS;
		} else {
			rval = FC_FAILURE;
		}
		break;

	case FC_TRAN_CLASS3:
		if (cos & FC_NS_CLASS3) {
			rval = FC_SUCCESS;
		} else {
			rval = FC_FAILURE;
		}
		break;

	default:
		rval = FC_FAILURE;
		break;
	}

	return (rval);
}


static fp_cmd_t *
fp_deque_cmd(fc_port_t *port)
{
	fp_cmd_t *cmd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	if (port->fp_wait_head == NULL) {
		/*
		 * To avoid races, NULL the fp_wait_tid as
		 * we are about to exit the timeout thread.
		 */
		port->fp_wait_tid = NULL;
		mutex_exit(&port->fp_mutex);
		return (NULL);
	}

	cmd = port->fp_wait_head;
	port->fp_wait_head = cmd->cmd_next;
	cmd->cmd_next = NULL;

	if (port->fp_wait_head == NULL) {
		port->fp_wait_tail = NULL;
	}
	mutex_exit(&port->fp_mutex);

	return (cmd);
}


static void
fp_jobwait(job_request_t *job)
{
	sema_p(&job->job_port_sema);
}


int
fp_state_to_rval(uchar_t state)
{
	int count;

	for (count = 0; count < sizeof (fp_xlat) /
	sizeof (fp_xlat[0]); count++) {
		if (fp_xlat[count].xlat_state == state) {
			return (fp_xlat[count].xlat_rval);
		}
	}

	return (FC_FAILURE);
}


/*
 * For Synchronous I/O requests, the caller is
 * expected to do fctl_jobdone(if necessary)
 *
 * We want to preserve atleast one failure in the
 * job_result if it happens.
 *
 */
static void
fp_iodone(fp_cmd_t *cmd)
{
	int		reset = 0;
	job_request_t 	*job = cmd->cmd_job;

	ASSERT(job != NULL);
	ASSERT(cmd->cmd_port != NULL);
	ASSERT(FP_CMD_TO_PKT(cmd) != NULL);

	mutex_enter(&job->job_mutex);
	if (job->job_result == FC_SUCCESS) {
		job->job_result = fp_state_to_rval(cmd->cmd_pkt.pkt_state);
	}

	if (job->job_flags & JOB_TYPE_FP_ASYNC) {
		fc_packet_t	*ulp_pkt = cmd->cmd_ulp_pkt;

		mutex_exit(&job->job_mutex);
		if (ulp_pkt) {
			reset++;
			if (cmd->cmd_pkt.pkt_pd) {
				mutex_enter(&cmd->cmd_pkt.pkt_pd->pd_mutex);
				cmd->cmd_pkt.pkt_pd->pd_flags = PD_IDLE;
				mutex_exit(&cmd->cmd_pkt.pkt_pd->pd_mutex);
			}

			if (cmd->cmd_flags & FP_CMD_DELDEV_ON_ERROR &&
			    FP_IS_PKT_ERROR(ulp_pkt)) {
				fc_port_device_t *pd = cmd->cmd_pkt.pkt_pd;

				if (pd) {
					fc_port_t 	*port;
					fc_device_t	*node;

					port = cmd->cmd_port;
					mutex_enter(&pd->pd_mutex);
					pd->pd_state = PORT_DEVICE_INVALID;
					node = pd->pd_device;
					mutex_exit(&pd->pd_mutex);

					ASSERT(node != NULL);
					ASSERT(port != NULL);
					fctl_remove_port_device(port, pd);
					fctl_remove_device(node);

					ulp_pkt->pkt_pd = NULL;
				}
			}
			ulp_pkt->pkt_comp(ulp_pkt);
		}
	} else {
		mutex_exit(&job->job_mutex);
	}

	if (!reset && cmd->cmd_pkt.pkt_pd) {
		mutex_enter(&cmd->cmd_pkt.pkt_pd->pd_mutex);
		cmd->cmd_pkt.pkt_pd->pd_flags = PD_IDLE;
		mutex_exit(&cmd->cmd_pkt.pkt_pd->pd_mutex);
	}

	fp_jobdone(job);
	fp_free_pkt(cmd);
}


/*
 * Job completion handler
 */
static void
fp_jobdone(job_request_t *job)
{
	mutex_enter(&job->job_mutex);
	ASSERT(job->job_counter > 0);
	if (--job->job_counter == 0) {
		if (job->job_ulp_pkts) {
			ASSERT(job->job_ulp_listlen > 0);
			kmem_free(job->job_ulp_pkts,
			    sizeof (fc_packet_t *) * job->job_ulp_listlen);
		}
		if (job->job_flags & JOB_TYPE_FP_ASYNC) {
			mutex_exit(&job->job_mutex);
			fctl_jobdone(job);
		} else {
			mutex_exit(&job->job_mutex);
			sema_v(&job->job_port_sema);
			return;
		}
	} else {
		mutex_exit(&job->job_mutex);
	}
}


/*
 * Perform Shutdown of a port
 */
static int
fp_port_shutdown(fc_port_t *port, job_request_t *job)
{
	int			index;
	int			count;
	int			flags;
	fp_cmd_t 		*cmd;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	job->job_result = FC_SUCCESS;

	/*
	 * Softints aren't cancelable, so wait for a second
	 * to complete; if it doesn't, fail the port shutdown
	 * operation.
	 */
	if (port->fp_softid) {
		fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
		    "waiting for softints to complete...");

		mutex_exit(&port->fp_mutex);
		delay(drv_usectohz(FP_ONE_SECOND));
		mutex_enter(&port->fp_mutex);
		if (port->fp_softid) {
			job->job_result = FC_FAILURE;
			mutex_exit(&port->fp_mutex);
			fctl_jobdone(job);
			mutex_enter(&port->fp_mutex);
			return (FC_FAILURE);
		}
	}

	if (port->fp_wait_tid) {
		timeout_id_t tid;

		tid = port->fp_wait_tid;
		port->fp_wait_tid = NULL;
		mutex_exit(&port->fp_mutex);
		(void) untimeout(tid);
	} else {
		mutex_exit(&port->fp_mutex);
	}

	/*
	 * While we cancel the timeout, let's also return the
	 * the outstanding requests back to the callers.
	 */
	while ((cmd = fp_deque_cmd(port)) != NULL) {
		ASSERT(cmd->cmd_job != NULL);
		cmd->cmd_job->job_result = FC_OFFLINE;
		fp_iodone(cmd);
	}

	/*
	 * Gracefully LOGO with all the devices logged in.
	 */
	mutex_enter(&port->fp_mutex);

	for (count = index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;
		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				count++;
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}

	if (job->job_flags & JOB_TYPE_FP_ASYNC) {
		flags = job->job_flags;
		job->job_flags &= ~JOB_TYPE_FP_ASYNC;
	} else {
		flags = 0;
	}
	if (count) {
		FCTL_SET_JOB_COUNTER(job, count);
		for (index = 0; index < pwwn_table_size; index++) {
			head = &port->fp_pwwn_table[index];
			pd = head->pwwn_head;
			while (pd != NULL) {
				mutex_enter(&pd->pd_mutex);
				if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
					ASSERT(pd->pd_count > 0);
					/*
					 * Force the couter to ONE in order
					 * for us to really send LOGO els.
					 */
					pd->pd_count = 1;
					mutex_exit(&pd->pd_mutex);
					mutex_exit(&port->fp_mutex);
					(void) fp_logout(port, pd, job);
					mutex_enter(&port->fp_mutex);
				} else {
					mutex_exit(&pd->pd_mutex);
				}
				pd = pd->pd_wwn_hnext;
			}
		}
		mutex_exit(&port->fp_mutex);
		fp_jobwait(job);
	} else {
		mutex_exit(&port->fp_mutex);
	}

	if (job->job_result != FC_SUCCESS) {
		fp_printf(port, CE_WARN, FP_LOG_ONLY, 0, NULL,
		    "Can't logout all devices. Proceeding with"
		    " port shutdown");
		job->job_result = FC_SUCCESS;
	}

	fctl_remall(port);

	mutex_enter(&port->fp_mutex);
	if (FC_IS_TOP_SWITCH(port->fp_topology)) {
		mutex_exit(&port->fp_mutex);
		fp_ns_fini(port, job);
	} else {
		mutex_exit(&port->fp_mutex);
	}

	if (flags) {
		job->job_flags = flags;
	}

	fctl_jobdone(job);
	mutex_enter(&port->fp_mutex);

	return (FC_SUCCESS);
}


/*
 * Build the port driver's data structures based on the AL_PA list
 */
static void
fp_get_loopmap(fc_port_t *port, job_request_t *job)
{
	int 			rval;
	int			flag;
	int 			count;
	uint32_t		d_id;
	fc_port_device_t 	*pd;
	fc_lilpmap_t 		*lilp_map;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	if (FC_PORT_STATE_MASK(port->fp_state) == FC_STATE_OFFLINE) {
		job->job_result = FC_OFFLINE;
		mutex_exit(&port->fp_mutex);
		fp_jobdone(job);
		mutex_enter(&port->fp_mutex);
		return;
	}

	if (port->fp_lilp_map.lilp_length == 0) {
		mutex_exit(&port->fp_mutex);
		job->job_result = FC_NO_MAP;
		fp_jobdone(job);
		mutex_enter(&port->fp_mutex);
		return;
	}
	mutex_exit(&port->fp_mutex);

	lilp_map = &port->fp_lilp_map;
	FCTL_SET_JOB_COUNTER(job, lilp_map->lilp_length);

	if (job->job_code == JOB_PORT_GETMAP_PLOGI_ALL) {
		flag = FP_CMD_PLOGI_RETAIN;
	} else {
		flag = FP_CMD_PLOGI_DONT_CARE;
	}

	for (count = 0; count < lilp_map->lilp_length; count++) {
		d_id = lilp_map->lilp_alpalist[count];

		if (d_id == (lilp_map->lilp_myalpa & 0xFF)) {
			fp_jobdone(job);
			continue;
		}

		pd = fctl_get_port_device_by_did(port, d_id);
		if (pd) {
			mutex_enter(&pd->pd_mutex);
			if (flag == FP_CMD_PLOGI_DONT_CARE ||
			    pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				mutex_exit(&pd->pd_mutex);
				fp_jobdone(job);
				continue;
			}
			mutex_exit(&pd->pd_mutex);
		}

		rval = fp_port_login(port, d_id, job, flag,
		    KM_SLEEP, pd, NULL);
		if (rval != FC_SUCCESS) {
			fp_jobdone(job);
		}
	}
	mutex_enter(&port->fp_mutex);
}


/*
 * Perform loop ONLINE processing
 */
static void
fp_loop_online(fc_port_t *port, job_request_t *job)
{
	int			count;
	int			rval;
	uint32_t		d_id;
	uint32_t		listlen;
	fc_lilpmap_t		*lilp_map;
	fc_port_device_t	*pd;
	fc_portmap_t		*changelist;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	FP_TNF_PROBE_2((fp_loop_online, "fp_startup_trace",
	    "fp_loop_online Begin",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));

	lilp_map = &port->fp_lilp_map;

	if (lilp_map->lilp_length) {
		mutex_enter(&port->fp_mutex);
		if (port->fp_soft_state & FP_SOFT_IN_FCA_RESET) {
			port->fp_soft_state &= ~FP_SOFT_IN_FCA_RESET;
			mutex_exit(&port->fp_mutex);
			delay(drv_usectohz(PLDA_RR_TOV * 1000 * 1000));
		} else {
			mutex_exit(&port->fp_mutex);
		}

		FCTL_SET_JOB_COUNTER(job, lilp_map->lilp_length);

		for (count = 0; count < lilp_map->lilp_length; count++) {
			d_id = lilp_map->lilp_alpalist[count];

			if (d_id == (lilp_map->lilp_myalpa & 0xFF)) {
				fp_jobdone(job);
				continue;
			}

			pd = fctl_get_port_device_by_did(port, d_id);
			if (pd != NULL) {
#ifdef	DEBUG
				mutex_enter(&pd->pd_mutex);
				if (pd->pd_recepient == PD_PLOGI_INITIATOR) {
					ASSERT(pd->pd_type != PORT_DEVICE_OLD);
				}
				mutex_exit(&pd->pd_mutex);
#endif /* DEBUG */
				fp_jobdone(job);
				continue;
			}

			rval = fp_port_login(port, d_id, job,
			    FP_CMD_PLOGI_DONT_CARE, KM_SLEEP, pd, NULL);

			if (rval != FC_SUCCESS) {
				fp_jobdone(job);
			}
		}
		fp_jobwait(job);
	}
	listlen = 0;
	changelist = NULL;

	if ((job->job_flags & JOB_CANCEL_ULP_NOTIFICATION) == 0) {
		mutex_enter(&port->fp_mutex);
		ASSERT(port->fp_statec_busy > 0);
		if (port->fp_statec_busy == 1) {
			mutex_exit(&port->fp_mutex);
			fctl_fillout_map(port, &changelist, &listlen, 1);

			mutex_enter(&port->fp_mutex);
			if (port->fp_lilp_map.lilp_magic < MAGIC_LIRP) {
				ASSERT(port->fp_total_devices == 0);
				port->fp_total_devices = port->fp_dev_count;
			}
		} else {
			job->job_flags |= JOB_CANCEL_ULP_NOTIFICATION;
		}
	} else {
		mutex_enter(&port->fp_mutex);
	}
	mutex_exit(&port->fp_mutex);

	if ((job->job_flags & JOB_CANCEL_ULP_NOTIFICATION) == 0) {
		(void) fp_ulp_statec_cb(port, FC_STATE_ONLINE, changelist,
		    listlen, listlen, KM_SLEEP);
	} else {
		mutex_enter(&port->fp_mutex);
		FC_STATEC_DONE(port);
		ASSERT(changelist == NULL && listlen == 0);
		mutex_exit(&port->fp_mutex);
	}

	FP_TNF_PROBE_2((fp_loop_online, "fp_startup_trace",
	    "fp_loop_online End",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));
}


/*
 * Get an Arbitrated Loop map from the underlying FCA
 */
static int
fp_get_lilpmap(fc_port_t *port, fc_lilpmap_t *lilp_map)
{
	int rval;

	FP_TNF_PROBE_2((fp_get_lilpmap, "fp_startup_trace",
	    "fp_get_lilpmap Begin",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	lilp_map, 	lilp_map));

	bzero((caddr_t)lilp_map, sizeof (fc_lilpmap_t));
	rval = port->fp_fca_tran->fca_getmap(port->fp_fca_handle, lilp_map);
	lilp_map->lilp_magic &= 0xFF;	/* Ignore upper byte */

	if (rval != FC_SUCCESS) {
		rval = FC_NO_MAP;
	} else if (lilp_map->lilp_length == 0 &&
	    (lilp_map->lilp_magic >= MAGIC_LISM &&
	    lilp_map->lilp_magic < MAGIC_LIRP)) {
		uchar_t lilp_length;

		/*
		 * Since the map length is zero, provide all
		 * the valid AL_PAs for NL_ports discovery.
		 */
		lilp_length = sizeof (fp_valid_alpas) /
		    sizeof (fp_valid_alpas[0]);
		lilp_map->lilp_length = lilp_length;
		bcopy(fp_valid_alpas, lilp_map->lilp_alpalist,
		    lilp_length);
	} else {
		rval = fp_validate_lilp_map(lilp_map);

		if (rval == FC_SUCCESS) {
			mutex_enter(&port->fp_mutex);
			port->fp_total_devices = lilp_map->lilp_length - 1;
			mutex_exit(&port->fp_mutex);
		}
	}

	mutex_enter(&port->fp_mutex);
	if (rval != FC_SUCCESS && !(port->fp_soft_state & FP_SOFT_BAD_LINK)) {
		port->fp_soft_state |= FP_SOFT_BAD_LINK;
		mutex_exit(&port->fp_mutex);

		if (port->fp_fca_tran->fca_reset(port->fp_fca_handle,
		    FC_FCA_RESET_CORE) != FC_SUCCESS) {
			fp_printf(port, CE_WARN, FP_LOG_ONLY, 0, NULL,
			    "FCA reset failed after LILP map was found"
			    " to be invalid");
		}
	} else if (rval == FC_SUCCESS) {
		port->fp_soft_state &= ~FP_SOFT_BAD_LINK;
		mutex_exit(&port->fp_mutex);
	} else {
		mutex_exit(&port->fp_mutex);
	}

	FP_TNF_PROBE_2((fp_get_lilpmap, "fp_startup_trace",
	    "fp_get_lilpmap End",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	lilp_map, 	lilp_map));

	return (rval);
}


/*
 * Perform Fabric Login:
 *
 * Return Values:
 *		FC_SUCCESS
 *		FC_FAILURE
 *		FC_NOMEM
 *		FC_TRANSPORT_ERROR
 *		and a lot others defined in fc_error.h
 */
static int
fp_fabric_login(fc_port_t *port, uint32_t s_id, job_request_t *job,
    int flag, int sleep)
{
	int		rval;
	fp_cmd_t 	*cmd;
	uchar_t 	class;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	FP_TNF_PROBE_2((fp_fabric_login, "fp_startup_trace",
	    "fp_fabric_login Begin",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));

	class = fp_get_nextclass(port, FC_TRAN_CLASS_INVALID);
	if (class == FC_TRAN_CLASS_INVALID) {
		return (FC_ELS_BAD);
	}

	cmd = fp_alloc_pkt(port, sizeof (la_els_logi_t),
	    sizeof (la_els_logi_t), sleep);
	if (cmd == NULL) {
		return (FC_NOMEM);
	}

	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    flag, fp_retry_count, NULL);

	fp_xlogi_init(port, cmd, s_id, 0xFFFFFE, fp_flogi_intr,
	    job, LA_ELS_FLOGI);

	rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
	if (rval != FC_SUCCESS) {
		fp_free_pkt(cmd);
	}

	FP_TNF_PROBE_2((fp_fabric_login, "fp_startup_trace",
	    "fp_fabric_login transport End",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));

	return (rval);
}


/*
 * In some scenarios such as private loop device discovery period
 * the port_device data structure isn't allocated. The allocation
 * is done when the PLOGI is successful. In some other scenarios
 * such as Fabric topology, the port_device is already created
 * and initialized with appropriate values (as the NS provides
 * them)
 */
static int
fp_port_login(fc_port_t *port, uint32_t d_id, job_request_t *job, int cmd_flag,
    int sleep, fc_port_device_t *pd, fc_packet_t *ulp_pkt)
{
	uchar_t class;
	fp_cmd_t *cmd;
	uint32_t src_id;

#ifdef	DEBUG
	if (pd == NULL) {
		ASSERT(fctl_get_port_device_by_did(port, d_id) == NULL);
	}
#endif /* DEBUG */

	class = fp_get_nextclass(port, FC_TRAN_CLASS_INVALID);
	if (class == FC_TRAN_CLASS_INVALID) {
		return (FC_ELS_BAD);
	}

	cmd = fp_alloc_pkt(port, sizeof (la_els_logi_t),
	    sizeof (la_els_logi_t), sleep);
	if (cmd == NULL) {
		return (FC_NOMEM);
	}
	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    cmd_flag, fp_retry_count, ulp_pkt);

	mutex_enter(&port->fp_mutex);
	src_id = port->FP_PORT_ID;
	mutex_exit(&port->fp_mutex);

	fp_xlogi_init(port, cmd, src_id, d_id, fp_plogi_intr,
	    job, LA_ELS_PLOGI);

	(FP_CMD_TO_PKT(cmd))->pkt_pd = pd;

	if (pd) {
		mutex_enter(&pd->pd_mutex);
		pd->pd_flags = PD_ELS_IN_PROGRESS;
		mutex_exit(&pd->pd_mutex);
	}

	if (fp_sendcmd(port, cmd, port->fp_fca_handle) != FC_SUCCESS) {
		if (pd) {
			mutex_enter(&pd->pd_mutex);
			pd->pd_flags = PD_IDLE;
			mutex_exit(&pd->pd_mutex);
		}

		fp_iodone(cmd);
	}

	return (FC_SUCCESS);
}


/*
 * Register the LOGIN parameters with a port device
 */
static void
fp_register_login(ddi_acc_handle_t *handle, fc_port_device_t *pd,
    la_els_logi_t *acc, uchar_t class)
{
	fc_device_t *node;

	ASSERT(pd != NULL);

	mutex_enter(&pd->pd_mutex);
	node = pd->pd_device;
	if (pd->pd_count == 0) {
		pd->pd_count++;
	}

	if (handle) {
		FP_CP_IN(*handle, &acc->common_service, &pd->pd_csp,
		    sizeof (acc->common_service));
		FP_CP_IN(*handle, &acc->class_1, &pd->pd_clsp1,
		    sizeof (acc->class_1));
		FP_CP_IN(*handle, &acc->class_2, &pd->pd_clsp2,
		    sizeof (acc->class_2));
		FP_CP_IN(*handle, &acc->class_3, &pd->pd_clsp3,
		    sizeof (acc->class_3));
	} else {
		pd->pd_csp = acc->common_service;
		pd->pd_clsp1 = acc->class_1;
		pd->pd_clsp2 = acc->class_2;
		pd->pd_clsp3 = acc->class_3;
	}

	pd->pd_state = PORT_DEVICE_LOGGED_IN;
	pd->pd_login_class = class;
	mutex_exit(&pd->pd_mutex);

	mutex_enter(&node->fd_mutex);
	if (handle) {
		FP_CP_IN(*handle, acc->vendor_version, node->fd_vv,
		    sizeof (node->fd_vv));
	} else {
		bcopy(acc->vendor_version, node->fd_vv, sizeof (node->fd_vv));
	}
	mutex_exit(&node->fd_mutex);
}


/*
 * Mark a port device OFFLINE
 */
static void
fp_port_device_offline(fc_port_device_t *pd)
{
	ASSERT(MUTEX_HELD(&pd->pd_mutex));
	if (pd->pd_count) {
		bzero((caddr_t)&pd->pd_csp, sizeof (struct common_service));
		bzero((caddr_t)&pd->pd_clsp1, sizeof (struct service_param));
		bzero((caddr_t)&pd->pd_clsp2, sizeof (struct service_param));
		bzero((caddr_t)&pd->pd_clsp3, sizeof (struct service_param));
		pd->pd_login_class = 0;
	}
	pd->pd_type = PORT_DEVICE_OLD;
	pd->pd_tolerance = 0;
}


/*
 * Deregistration of a port device
 */
static void
fp_unregister_login(fc_port_device_t *pd)
{
	fc_device_t *node;

	ASSERT(pd != NULL);

	mutex_enter(&pd->pd_mutex);
	pd->pd_count = 0;
	bzero((caddr_t)&pd->pd_csp, sizeof (struct common_service));
	bzero((caddr_t)&pd->pd_clsp1, sizeof (struct service_param));
	bzero((caddr_t)&pd->pd_clsp2, sizeof (struct service_param));
	bzero((caddr_t)&pd->pd_clsp3, sizeof (struct service_param));

	pd->pd_state = PORT_DEVICE_VALID;
	pd->pd_login_class = 0;
	node = pd->pd_device;
	mutex_exit(&pd->pd_mutex);

	mutex_enter(&node->fd_mutex);
	bzero(node->fd_vv, sizeof (node->fd_vv));
	mutex_exit(&node->fd_mutex);
}


/*
 * Handle OFFLINE state of an FCA port
 */
static void
fp_port_offline(fc_port_t *port, int notify)
{
	int			index;
	timeout_id_t		tid;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	for (index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;
		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_recepient == PD_PLOGI_INITIATOR) {
				fp_port_device_offline(pd);
				fctl_delist_did_table(port, pd);
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}
	port->fp_total_devices = 0;

	if (notify) {
		/*
		 * Decrement the statec busy counter as we
		 * are almost done with handling the state
		 * change
		 */
		ASSERT(port->fp_statec_busy > 0);
		FC_STATEC_DONE(port);
		mutex_exit(&port->fp_mutex);
		(void) fp_ulp_statec_cb(port, FC_STATE_OFFLINE, NULL,
		    0, 0, KM_SLEEP);
		mutex_enter(&port->fp_mutex);
	}

	if (port->fp_offline_tid) {
		/*
		 * weird, anyway, ignore and move on..
		 */
		return;
	}
	mutex_exit(&port->fp_mutex);

	tid = timeout(fp_offline_timeout, (caddr_t)port,
	    fp_offline_ticks);
	mutex_enter(&port->fp_mutex);
	port->fp_offline_tid = tid;
}


/*
 * Offline devices and send up a state change notification to ULPs
 */
static void
fp_offline_timeout(void *port_handle)
{
	fc_port_t 	*port = (fc_port_t *)port_handle;
	uint32_t	listlen = 0;
	fc_portmap_t 	*changelist = NULL;

	mutex_enter(&port->fp_mutex);
	port->fp_offline_tid = NULL;

	if (FC_PORT_STATE_MASK(port->fp_state) != FC_STATE_OFFLINE ||
	    port->fp_dev_count == 0) {
		mutex_exit(&port->fp_mutex);
		return;
	}
	mutex_exit(&port->fp_mutex);

	fp_printf(port, CE_WARN, FP_LOG_ONLY, NULL, 0, "OFFLINE timeout");

	fctl_fillout_map(port, &changelist, &listlen, 1);
	(void) fp_ulp_statec_cb(port, FC_STATE_OFFLINE, changelist,
	    listlen, listlen, KM_SLEEP);
}


/*
 * Perform general purpose ELS request initialization
 */
static void
fp_els_init(fp_cmd_t *cmd, uint32_t s_id, uint32_t d_id,
    void (*comp) (), job_request_t *job)
{
	fc_packet_t *pkt;

	pkt = FP_CMD_TO_PKT(cmd);
	cmd->cmd_job = job;

	pkt->pkt_cmd_fhdr.r_ctl = R_CTL_ELS_REQ;
	pkt->pkt_cmd_fhdr.d_id = d_id;
	pkt->pkt_cmd_fhdr.s_id = s_id;
	pkt->pkt_cmd_fhdr.type = FC_TYPE_EXTENDED_LS;
	pkt->pkt_cmd_fhdr.f_ctl = F_CTL_SEQ_INITIATIVE | F_CTL_FIRST_SEQ;
	pkt->pkt_cmd_fhdr.seq_id = 0;
	pkt->pkt_cmd_fhdr.df_ctl  = 0;
	pkt->pkt_cmd_fhdr.seq_cnt = 0;
	pkt->pkt_cmd_fhdr.ox_id = 0xffff;
	pkt->pkt_cmd_fhdr.rx_id = 0xffff;
	pkt->pkt_cmd_fhdr.ro = 0;
	pkt->pkt_cmd_fhdr.rsvd = 0;
	pkt->pkt_comp = comp;
	pkt->pkt_timeout = FP_ELS_TIMEOUT;
}


/*
 * Initialize PLOGI/FLOGI ELS request
 */
static void
fp_xlogi_init(fc_port_t *port, fp_cmd_t *cmd, uint32_t s_id,
    uint32_t d_id, void (*intr) (), job_request_t *job, uchar_t ls_code)
{
	ls_code_t	payload;

	fp_els_init(cmd, s_id, d_id, intr, job);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;

	payload.ls_code = ls_code;
	payload.mbz = 0;

	FP_CP_OUT(cmd->cmd_pkt.pkt_cmd_acc, &port->fp_service_params,
	    cmd->cmd_pkt.pkt_cmd, sizeof (port->fp_service_params));

	FP_CP_OUT(cmd->cmd_pkt.pkt_cmd_acc, &payload,
	    cmd->cmd_pkt.pkt_cmd, sizeof (payload));
}


/*
 * Initialize LOGO ELS request
 */
static void
fp_logo_init(fc_port_device_t *pd, fp_cmd_t *cmd, job_request_t *job)
{
	fc_port_t	*port;
	fc_packet_t	*pkt;
	la_els_logo_t 	payload;

	port = pd->pd_port;
	pkt = FP_CMD_TO_PKT(cmd);
	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	fp_els_init(cmd, port->FP_PORT_ID, pd->PD_PORT_ID, fp_logo_intr, job);

	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	pkt->pkt_pd = pd;
	pkt->pkt_tran_flags = FC_TRAN_INTR | pd->pd_login_class;
	pkt->pkt_tran_type = FC_PKT_EXCHANGE;

	payload.LS_CODE = LA_ELS_LOGO;
	payload.MBZ = 0;
	payload.nport_ww_name = port->fp_service_params.nport_ww_name;
	payload.nport_id = port->fp_port_id;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Initialize RLS ELS request
 */
static void
fp_rls_init(fc_port_device_t *pd, fp_cmd_t *cmd, job_request_t *job)
{
	fc_port_t	*port;
	fc_packet_t	*pkt;
	la_els_rls_t 	payload;

	port = pd->pd_port;
	pkt = FP_CMD_TO_PKT(cmd);
	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	fp_els_init(cmd, port->FP_PORT_ID, pd->PD_PORT_ID, fp_rls_intr, job);

	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	pkt->pkt_pd = pd;
	pkt->pkt_tran_flags = FC_TRAN_INTR | pd->pd_login_class;
	pkt->pkt_tran_type = FC_PKT_EXCHANGE;

	payload.LS_CODE = LA_ELS_RLS;
	payload.MBZ = 0;
	payload.rls_portid = port->fp_port_id;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Initialize an ADISC ELS request
 */
static void
fp_adisc_init(fc_port_device_t *pd, fp_cmd_t *cmd, job_request_t *job)
{
	fc_port_t 	*port;
	fc_packet_t	*pkt;
	la_els_adisc_t 	payload;

	ASSERT(MUTEX_HELD(&pd->pd_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_port->fp_mutex));

	port = pd->pd_port;
	pkt = FP_CMD_TO_PKT(cmd);

	fp_els_init(cmd, port->FP_PORT_ID, pd->PD_PORT_ID, fp_adisc_intr, job);

	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	pkt->pkt_tran_flags = FC_TRAN_INTR | pd->pd_login_class;
	pkt->pkt_tran_type = FC_PKT_EXCHANGE;
	pkt->pkt_pd = pd;

	payload.LS_CODE = LA_ELS_ADISC;
	payload.MBZ = 0;
	payload.nport_id = port->fp_port_id;
	payload.port_wwn = port->fp_service_params.nport_ww_name;
	payload.node_wwn = port->fp_service_params.node_ww_name;
	payload.hard_addr = port->fp_hard_addr;

	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Send up a state change notification to ULPs
 */
static int
fp_ulp_statec_cb(fc_port_t *port, uint32_t state, fc_portmap_t *changelist,
    uint32_t listlen, uint32_t alloc_len, int sleep)
{
	fc_port_clist_t *clist;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	clist = kmem_zalloc(sizeof (*clist), sleep);
	if (clist == NULL) {
		kmem_free(changelist, alloc_len * sizeof (*changelist));
		return (FC_NOMEM);
	}
	clist->clist_state = state;

	mutex_enter(&port->fp_mutex);
	clist->clist_flags = port->fp_topology;
	mutex_exit(&port->fp_mutex);

	clist->clist_port = (opaque_t)port;
	clist->clist_len = listlen;
	clist->clist_size = alloc_len;
	clist->clist_map = changelist;

#ifdef	DEBUG
	/*
	 * Sanity check for presence of OLD devices in the hash lists
	 */
	if (clist->clist_size) {
		int 			count;
		fc_port_device_t	*pd;

		ASSERT(clist->clist_map != NULL);
		for (count = 0; count < clist->clist_len; count++) {
			if (clist->clist_map[count].map_state ==
			    PORT_DEVICE_INVALID) {
				la_wwn_t 	pwwn;
				fc_portid_t 	d_id;

				pd = clist->clist_map[count].map_pd;
				ASSERT(pd != NULL);

				mutex_enter(&pd->pd_mutex);
				pwwn = pd->pd_port_name;
				d_id = pd->pd_port_id;
				mutex_exit(&pd->pd_mutex);

				pd = fctl_get_port_device_by_pwwn(port, &pwwn);
				ASSERT(pd != clist->clist_map[count].map_pd);

				pd = fctl_get_port_device_by_did(port,
				    d_id.port_id);
				ASSERT(pd != clist->clist_map[count].map_pd);
			}
		}
	}
#endif /* DEBUG */

	ddi_set_callback(fctl_ulp_statec_cb, (caddr_t)clist, &port->fp_softid);
	ddi_run_callback(&port->fp_softid);

	FP_TNF_PROBE_3((fp_ulp_statec_cb, "fp_startup_trace",
	    "fp_ulp_statec fired",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_uint32, 	state, 		state,
	    tnf_uint32, 	length, 	listlen));

	return (FC_SUCCESS);
}


/*
 * Send up a FC_STATE_DEVICE_CHANGE state notification to ULPs
 */
static int
fp_ulp_devc_cb(fc_port_t *port, fc_portmap_t *changelist,
    uint32_t listlen, uint32_t alloc_len, int sleep)
{
	fc_port_clist_t *clist;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	clist = kmem_zalloc(sizeof (*clist), sleep);
	if (clist == NULL) {
		kmem_free(changelist, alloc_len * sizeof (*changelist));
		return (FC_NOMEM);
	}

	clist->clist_state = FC_STATE_DEVICE_CHANGE;

	mutex_enter(&port->fp_mutex);
	clist->clist_flags = port->fp_topology;
	mutex_exit(&port->fp_mutex);

	clist->clist_port = (opaque_t)port;
	clist->clist_len = listlen;
	clist->clist_size = alloc_len;
	clist->clist_map = changelist;

#ifdef	DEBUG
	/*
	 * Sanity check for presence of OLD devices in the hash lists
	 */
	if (clist->clist_size) {
		int 			count;
		fc_port_device_t	*pd;

		ASSERT(clist->clist_map != NULL);
		for (count = 0; count < clist->clist_len; count++) {
			if (clist->clist_map[count].map_flags ==
			    PORT_DEVICE_OLD) {
				la_wwn_t 	pwwn;
				fc_portid_t 	d_id;

				pd = clist->clist_map[count].map_pd;
				ASSERT(pd != NULL);

				mutex_enter(&pd->pd_mutex);
				pwwn = pd->pd_port_name;
				d_id = pd->pd_port_id;
				mutex_exit(&pd->pd_mutex);

				pd = fctl_get_port_device_by_pwwn(port, &pwwn);
				ASSERT(pd != clist->clist_map[count].map_pd);

				pd = fctl_get_port_device_by_did(port,
				    d_id.port_id);
				ASSERT(pd != clist->clist_map[count].map_pd);
			}
		}
	}
#endif /* DEBUG */

	ddi_set_callback(fctl_ulp_statec_cb, (caddr_t)clist, &port->fp_softid);
	ddi_run_callback(&port->fp_softid);

	FP_TNF_PROBE_2((fp_ulp_devc_cb, "fp_startup_trace",
	    "fp_ulp_devc fired",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_uint32, 	length, 	listlen));

	return (FC_SUCCESS);
}


/*
 * Perform PLOGI to the group of devices for ULPs
 */
static void
fp_plogi_group(fc_port_t *port, job_request_t *job)
{
	int 			count;
	int			rval;
	uint32_t		listlen;
	uint32_t		done;
	uint32_t		d_id;
	fc_device_t		*node;
	fc_port_device_t 	*pd;
	fc_packet_t		*ulp_pkt;
	la_els_logi_t		*els_data;

	FP_TNF_PROBE_2((fp_plogi_group, "fp_startup_trace",
	    "fp_plogi_group begin",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));

	done = 0;
	job->job_flags |= JOB_TYPE_FP_ASYNC;
	listlen = job->job_ulp_listlen;
	FCTL_SET_JOB_COUNTER(job, job->job_ulp_listlen);

	for (count = 0; count < listlen; count++) {
		ASSERT(job->job_ulp_pkts[count]->pkt_rsplen >=
		    sizeof (la_els_logi_t));

		ulp_pkt = job->job_ulp_pkts[count];
		pd = ulp_pkt->pkt_pd;
		if (pd == NULL) {
			if (ulp_pkt->pkt_state == FC_PKT_LOCAL_RJT) {
				done++;
				ulp_pkt->pkt_comp(ulp_pkt);
				fp_jobdone(job);
			} else {
				/* reset later */
				ulp_pkt->pkt_state = FC_PKT_FAILURE;
			}
			continue;
		}
		mutex_enter(&pd->pd_mutex);
		if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
			done++;
			els_data = (la_els_logi_t *)ulp_pkt->pkt_resp;

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &pd->pd_csp,
			    &els_data->common_service, sizeof (pd->pd_csp));

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &pd->pd_port_name,
			    &els_data->nport_ww_name,
			    sizeof (pd->pd_port_name));

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &pd->pd_clsp1,
			    &els_data->class_1, sizeof (pd->pd_clsp1));

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &pd->pd_clsp2,
			    &els_data->class_2, sizeof (pd->pd_clsp2));

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &pd->pd_clsp3,
			    &els_data->class_3, sizeof (pd->pd_clsp3));

			node = pd->pd_device;
			pd->pd_count++;
			mutex_exit(&pd->pd_mutex);

			mutex_enter(&node->fd_mutex);
			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &node->fd_node_name,
			    &els_data->node_ww_name,
			    sizeof (node->fd_node_name));

			FP_CP_OUT(ulp_pkt->pkt_resp_acc, &node->fd_vv,
			    &els_data->vendor_version,
			    sizeof (node->fd_vv));
			mutex_exit(&node->fd_mutex);
			ulp_pkt->pkt_state = FC_PKT_SUCCESS;
		} else if (ulp_pkt->pkt_state == FC_PKT_ELS_IN_PROGRESS) {
			done++;
			ulp_pkt->pkt_state = FC_PKT_ELS_IN_PROGRESS;
			mutex_exit(&pd->pd_mutex);
		} else {
			ASSERT(pd->pd_flags == PD_ELS_IN_PROGRESS);

			ulp_pkt->pkt_state = FC_PKT_FAILURE; /* reset later */
			mutex_exit(&pd->pd_mutex);
		}
		if (ulp_pkt->pkt_state != FC_PKT_FAILURE) {
			ulp_pkt->pkt_comp(ulp_pkt);
			fp_jobdone(job);
		}
	}

	if (done == listlen) {
		return;
	}

	for (count = 0; count < listlen; count++) {
		ulp_pkt = job->job_ulp_pkts[count];
		if (ulp_pkt->pkt_state == FC_PKT_FAILURE) {
			int cmd_flags = FP_CMD_PLOGI_RETAIN;

			pd = ulp_pkt->pkt_pd;
			if (pd != NULL) {
				mutex_enter(&pd->pd_mutex);
				d_id = pd->PD_PORT_ID;
				mutex_exit(&pd->pd_mutex);
			} else {
				d_id = ulp_pkt->pkt_cmd_fhdr.d_id;
#ifdef	DEBUG
				pd = fctl_get_port_device_by_did(port, d_id);
				ASSERT(pd == NULL);
#endif /* DEBUG */
				/*
				 * In the Fabric topology, use NS to create
				 * port device, and if that fails still try
				 * with PLOGI - which will make yet another
				 * attempt to create after successful PLOGI
				 */
				mutex_enter(&port->fp_mutex);
				if (FC_IS_TOP_SWITCH(port->fp_topology)) {
					mutex_exit(&port->fp_mutex);
					pd = fp_create_port_device_by_ns(port,
					    d_id, KM_SLEEP);
					if (pd) {
						cmd_flags |=
						    FP_CMD_DELDEV_ON_ERROR;
					}
				} else {
					mutex_exit(&port->fp_mutex);
				}
				ulp_pkt->pkt_pd = pd;
			}
			rval = fp_port_login(port, d_id, job, cmd_flags,
			    KM_SLEEP, pd, ulp_pkt);

			if (rval != FC_SUCCESS) {
				ulp_pkt->pkt_state = FC_PKT_FAILURE;
				ulp_pkt->pkt_comp(ulp_pkt);

				if (cmd_flags & FP_CMD_DELDEV_ON_ERROR) {
					ASSERT(pd != NULL);

					mutex_enter(&pd->pd_mutex);
					node = pd->pd_device;
					mutex_exit(&pd->pd_mutex);

					ASSERT(node != NULL);

					fctl_remove_port_device(port, pd);
					fctl_remove_device(node);
					ulp_pkt->pkt_pd = NULL;
				}
				fp_jobdone(job);
			}
		}
	}
	FP_TNF_PROBE_2((fp_plogi_group, "fp_startup_trace",
	    "fp_plogi_group transport end",
	    tnf_opaque, 	port_ptr, 	port,
	    tnf_opaque, 	job_ptr, 	job));
}


/*
 * Name server request initialization
 */
static void
fp_ns_init(fc_port_t *port, job_request_t *job, int sleep)
{
	int rval;
	int count;
	int size;

	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	FCTL_SET_JOB_COUNTER(job, 1);
	job->job_result = FC_SUCCESS;

	rval = fp_port_login(port, 0xFFFFFC,
	    job, FP_CMD_PLOGI_RETAIN, KM_SLEEP, NULL, NULL);

	if (rval != FC_SUCCESS) {
		mutex_enter(&port->fp_mutex);
		port->fp_topology = FC_TOP_NO_NS;
		mutex_exit(&port->fp_mutex);
		return;
	}
	fp_jobwait(job);

	if (job->job_result != FC_SUCCESS) {
		mutex_enter(&port->fp_mutex);
		port->fp_topology = FC_TOP_NO_NS;
		mutex_exit(&port->fp_mutex);
		return;
	}

	/*
	 * At this time, we'll do NS registration for objects in the
	 * ns_reg_cmds (see top of this file) array.
	 *
	 * Each time a ULP module registers with the transport, the
	 * appropriate fc4 bit is set fc4 types and registered with
	 * the NS for this support. Also, ULPs and FC admin utilities
	 * may do registration for objects like IP address, symbolic
	 * port/node name, Initial process associator at run time.
	 */
	size = sizeof (ns_reg_cmds) / sizeof (ns_reg_cmds[0]);

	FCTL_SET_JOB_COUNTER(job, size);
	job->job_result = FC_SUCCESS;

	for (count = 0; count < size; count++) {
		if (fp_ns_reg(port, NULL, ns_reg_cmds[count],
		    job, 0, sleep) != FC_SUCCESS) {
			fp_jobdone(job);
		}
	}
	if (size) {
		fp_jobwait(job);
	}

	job->job_result = FC_SUCCESS;
	(void) fp_ns_get_devcount(port, job, KM_SLEEP);

	FCTL_SET_JOB_COUNTER(job, 1);

	if (fp_ns_scr(port, job, FC_SCR_FULL_REGISTRATION,
	    sleep) == FC_SUCCESS) {
		fp_jobwait(job);
	}
}


/*
 * Name server finish:
 *	Unregister for RSCNs
 * 	Unregister all the host port objects in the Name Server
 * 	Perform LOGO with the NS;
 */
static void
fp_ns_fini(fc_port_t *port, job_request_t *job)
{
	fp_cmd_t 	*cmd;
	uchar_t		class;
	uint32_t	s_id;
	fc_packet_t	*pkt;
	la_els_logo_t 	payload;

	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	FCTL_SET_JOB_COUNTER(job, 1);
	if (fp_ns_scr(port, job, FC_SCR_CLEAR_REGISTRATION,
	    KM_SLEEP) == FC_SUCCESS) {
		fp_jobwait(job);
	}

	FCTL_SET_JOB_COUNTER(job, 1);
	if (fp_ns_reg(port, NULL, NS_DA_ID, job, 0, KM_SLEEP) != FC_SUCCESS) {
		fp_jobdone(job);
	}
	fp_jobwait(job);

	FCTL_SET_JOB_COUNTER(job, 1);

	cmd = fp_alloc_pkt(port, sizeof (la_els_logo_t),
		FP_PORT_IDENTIFIER_LEN, KM_SLEEP);
	pkt = FP_CMD_TO_PKT(cmd);

	mutex_enter(&port->fp_mutex);
	class = port->fp_ns_login_class;
	s_id = port->FP_PORT_ID;
	payload.nport_id = port->fp_port_id;
	mutex_exit(&port->fp_mutex);

	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    FP_CMD_PLOGI_DONT_CARE, 1, NULL);

	fp_els_init(cmd, s_id, 0xFFFFFC, fp_logo_intr, job);

	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	pkt->pkt_pd = NULL;

	payload.LS_CODE = LA_ELS_LOGO;
	payload.MBZ = 0;
	payload.nport_ww_name = port->fp_service_params.nport_ww_name;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));

	if (fp_sendcmd(port, cmd, port->fp_fca_handle) != FC_SUCCESS) {
		fp_iodone(cmd);
	}
	fp_jobwait(job);
}


/*
 * NS Registration function.
 *
 *	It should be seriously noted that FC-GS-2 currently doesn't supprt
 *	an Object Registration by a D_ID other than the owner of the object.
 *	What we are aiming at currently is to at least allow Symbolic Node/Port
 *	Name registration for any N_Port Identifier by the host software.
 *
 *	Anyway, if the second argument (fc_port_device_t *) is NULL, this
 *	function treats the request as Host NS Object.
 */
static int
fp_ns_reg(fc_port_t *port, fc_port_device_t *pd, uint16_t cmd_code,
    job_request_t *job, int polled, int sleep)
{
	int 		rval;
	fc_portid_t	s_id;
	fc_packet_t 	*pkt;
	fp_cmd_t 	*cmd;

	if (pd == NULL) {
		mutex_enter(&port->fp_mutex);
		s_id = port->fp_port_id;
		mutex_exit(&port->fp_mutex);
	} else {
		mutex_enter(&pd->pd_mutex);
		s_id = pd->pd_port_id;
		mutex_exit(&pd->pd_mutex);
	}

	if (polled) {
		FCTL_SET_JOB_COUNTER(job, 1);
	}

	switch (cmd_code) {
	case NS_RPN_ID:
	case NS_RNN_ID:
	{
		ns_rxn_req_t rxn;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_rxn_req_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			rxn.rxn_xname = (cmd_code == NS_RPN_ID) ?
			    (port->fp_service_params.nport_ww_name) :
			    (port->fp_service_params.node_ww_name);
		} else {
			if (cmd_code == NS_RPN_ID) {
				mutex_enter(&pd->pd_mutex);
				rxn.rxn_xname = pd->pd_port_name;
				mutex_exit(&pd->pd_mutex);
			} else {
				fc_device_t *node;

				mutex_enter(&pd->pd_mutex);
				node = pd->pd_device;
				mutex_exit(&pd->pd_mutex);

				mutex_enter(&node->fd_mutex);
				rxn.rxn_xname = node->fd_node_name;
				mutex_exit(&node->fd_mutex);
			}
		}
		rxn.rxn_port_id = s_id;
		FP_CP_OUT(pkt->pkt_cmd_acc, &rxn, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rxn));
		break;
	}

	case NS_RCS_ID:
	{
		ns_rcos_t rcos;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_rcos_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			rcos.rcos_cos = port->fp_cos;
		} else {
			mutex_enter(&pd->pd_mutex);
			rcos.rcos_cos = pd->pd_cos;
			mutex_exit(&pd->pd_mutex);
		}
		rcos.rcos_port_id = s_id;
		FP_CP_OUT(pkt->pkt_cmd_acc, &rcos, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rcos));
		break;
	}

	case NS_RFT_ID:
	{
		ns_rfc_type_t rfc;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_rfc_type_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			bcopy(port->fp_fc4_types, rfc.rfc_types,
			    sizeof (port->fp_fc4_types));
			mutex_exit(&port->fp_mutex);
		} else {
			mutex_enter(&pd->pd_mutex);
			bcopy(pd->pd_fc4types, rfc.rfc_types,
			    sizeof (pd->pd_fc4types));
			mutex_exit(&pd->pd_mutex);
		}
		rfc.rfc_port_id = s_id;

		FP_CP_OUT(pkt->pkt_cmd_acc, &rfc, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rfc));
		break;
	}

	case NS_RSPN_ID:
	{
		uchar_t 	name_len;
		int 		pl_size;
		ns_spn_t 	spn;

		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			name_len = port->fp_sym_port_namelen;
			mutex_exit(&port->fp_mutex);
		} else {
			mutex_enter(&pd->pd_mutex);
			name_len = pd->pd_spn_len;
			mutex_exit(&pd->pd_mutex);
		}

		pl_size = sizeof (ns_spn_t) + name_len;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) + pl_size,
			sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}

		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);

		pkt = &cmd->cmd_pkt;

		spn.spn_port_id = s_id;
		spn.spn_len = name_len;
		FP_CP_OUT(pkt->pkt_cmd_acc, &spn, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (spn));

		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			FP_CP_OUT(pkt->pkt_cmd_acc, port->fp_sym_port_name,
			    pkt->pkt_cmd + sizeof (fc_ct_header_t) +
			    sizeof (spn), name_len);
			mutex_exit(&port->fp_mutex);
		} else {
			mutex_enter(&pd->pd_mutex);
			FP_CP_OUT(pkt->pkt_cmd_acc, pd->pd_spn,
			    pkt->pkt_cmd + sizeof (fc_ct_header_t) +
			    sizeof (spn), name_len);
			mutex_exit(&pd->pd_mutex);
		}
		break;
	}

	case NS_RPT_ID:
	{
		ns_rpt_t rpt;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_rpt_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			rpt.rpt_type = port->fp_port_type;
		} else {
			mutex_enter(&pd->pd_mutex);
			rpt.rpt_type = pd->pd_porttype;
			mutex_exit(&pd->pd_mutex);
		}
		rpt.rpt_port_id = s_id;
		FP_CP_OUT(pkt->pkt_cmd_acc, &rpt, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rpt));
		break;
	}

	case NS_RIP_NN:
	{
		ns_rip_t rip;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_rip_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			rip.rip_node_name =
			    port->fp_service_params.node_ww_name;
			bcopy(port->fp_ip_addr, rip.rip_ip_addr,
			    sizeof (port->fp_ip_addr));
		} else {
			fc_device_t *node;

			/*
			 * The most correct implementation should have the
			 * IP address in the fc_device structure; I believe
			 * Node WWN and IP address should have one to one
			 * correlation (but guess what this is changing in
			 * FC-GS-2 latest draft)
			 */
			mutex_enter(&pd->pd_mutex);
			node = pd->pd_device;
			bcopy(pd->pd_ip_addr, rip.rip_ip_addr,
			    sizeof (pd->pd_ip_addr));
			mutex_exit(&pd->pd_mutex);

			mutex_enter(&node->fd_mutex);
			rip.rip_node_name = node->fd_node_name;
			mutex_exit(&node->fd_mutex);
		}
		FP_CP_OUT(pkt->pkt_cmd_acc, &rip, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rip));
		break;
	}

	case NS_RIPA_NN:
	{
		ns_ipa_t ipa;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_ipa_t), sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		if (pd == NULL) {
			ipa.ipa_node_name =
			    port->fp_service_params.node_ww_name;
			bcopy(port->fp_ipa, ipa.ipa_value,
			    sizeof (port->fp_ipa));
		} else {
			fc_device_t *node;

			mutex_enter(&pd->pd_mutex);
			node = pd->pd_device;
			mutex_exit(&pd->pd_mutex);

			mutex_enter(&node->fd_mutex);
			ipa.ipa_node_name = node->fd_node_name;
			bcopy(node->fd_ipa, ipa.ipa_value,
			    sizeof (node->fd_ipa));
			mutex_exit(&node->fd_mutex);
		}
		FP_CP_OUT(pkt->pkt_cmd_acc, &ipa, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (ipa));
		break;
	}

	case NS_RSNN_NN:
	{
		uchar_t 	name_len;
		int 		pl_size;
		ns_snn_t 	snn;
		fc_device_t	*node = NULL;

		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			name_len = port->fp_sym_node_namelen;
			mutex_exit(&port->fp_mutex);
		} else {
			mutex_enter(&pd->pd_mutex);
			node = pd->pd_device;
			mutex_exit(&pd->pd_mutex);

			mutex_enter(&node->fd_mutex);
			name_len = node->fd_snn_len;
			mutex_exit(&node->fd_mutex);
		}

		pl_size = sizeof (ns_snn_t) + name_len;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
			pl_size, sizeof (fc_reg_resp_t), sleep);
		if (cmd == NULL) {
			return (FC_NOMEM);
		}
		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);

		pkt = &cmd->cmd_pkt;

		snn.snn_len = name_len;

		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			FP_CP_OUT(pkt->pkt_cmd_acc, port->fp_sym_node_name,
			    pkt->pkt_cmd + sizeof (fc_ct_header_t) +
			    sizeof (snn), name_len);
			mutex_exit(&port->fp_mutex);
		} else {
			ASSERT(node != NULL);
			mutex_enter(&node->fd_mutex);
			FP_CP_OUT(pkt->pkt_cmd_acc, node->fd_snn,
			    pkt->pkt_cmd + sizeof (fc_ct_header_t) +
			    sizeof (snn), name_len);
			mutex_exit(&node->fd_mutex);
		}
		FP_CP_OUT(pkt->pkt_cmd_acc, &snn, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (snn));
		break;
	}

	case NS_DA_ID:
	{
		ns_remall_t rall;

		cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
		    sizeof (ns_remall_t), sizeof (fc_reg_resp_t), sleep);

		if (cmd == NULL) {
			return (FC_NOMEM);
		}

		fp_ct_init(port, cmd, NULL, cmd_code, NULL, 0, 0, job);
		pkt = &cmd->cmd_pkt;

		rall.rem_port_id = s_id;
		FP_CP_OUT(pkt->pkt_cmd_acc, &rall, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), sizeof (rall));
		break;
	}

	default:
		return (FC_FAILURE);
	}

	rval = fp_sendcmd(port, cmd, port->fp_fca_handle);

	if (rval != FC_SUCCESS) {
		job->job_result = rval;
		fp_iodone(cmd);
	}

	if (polled) {
		ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);
		fp_jobwait(job);
	} else {
		rval = FC_SUCCESS;
	}

	return (rval);
}


/*
 * Common interrupt handler
 */
static int
fp_common_intr(fc_packet_t *pkt, int iodone)
{
	int		rval = FC_FAILURE;
	fp_cmd_t 	*cmd;

	cmd = FP_PKT_TO_CMD(pkt);

	switch (pkt->pkt_state) {
	case FC_PKT_LOCAL_BSY:
	case FC_PKT_FABRIC_BSY:
	case FC_PKT_NPORT_BSY:
	case FC_PKT_TIMEOUT:
		cmd->cmd_retry_interval = (pkt->pkt_state ==
		    FC_PKT_TIMEOUT) ? 0 : fp_retry_delay;
		rval = fp_retry_cmd(pkt);
		break;

	case FC_PKT_FABRIC_RJT:
	case FC_PKT_NPORT_RJT:
	case FC_PKT_LOCAL_RJT:
	case FC_PKT_LS_RJT:
		rval = fp_handle_reject(pkt);
		break;

	default:
		if (pkt->pkt_resp_resid) {
			cmd->cmd_retry_interval = 0;
			rval = fp_retry_cmd(pkt);
		}
		break;
	}

	if (rval != FC_SUCCESS && iodone) {
		fp_iodone(cmd);
		rval = FC_SUCCESS;
	}

	return (rval);
}


/*
 * Some not so long winding theory on point to point topology:
 *
 *	In the ACC payload, if the D_ID is ZERO and the common service
 *	parameters indicate N_Port, then the topology is POINT TO POINT.
 *
 *	In a point to point topology with an N_Port, during Fabric Login,
 *	the destination N_Port will check with our WWN and decide if it
 * 	needs to issue PLOGI or not. That means, FLOGI could potentially
 *	trigger an unsolicited PLOGI from an N_Port. The Unsolicited
 *	PLOGI creates the device handles.
 *
 *	Assuming that the host port WWN is greater than the other N_Port
 *	WWN, then we become the master (be aware that this isn't the word
 *	used in the FC standards) and initiate the PLOGI.
 *
 */
static void
fp_flogi_intr(fc_packet_t *pkt)
{
	int			state;
	int			f_port;
	uint32_t		s_id;
	uint32_t		d_id;
	fp_cmd_t 		*cmd;
	fc_port_t 		*port;
	la_wwn_t		*swwn;
	la_wwn_t		dwwn;
	la_wwn_t		nwwn;
	fc_port_device_t	*pd;
	la_els_logi_t 		*acc;
	com_svc_t		csp;
	ls_code_t		resp;

	cmd = FP_PKT_TO_CMD(pkt);
	port = FP_CMD_TO_PORT(cmd);

	FP_TNF_PROBE_1((fp_flogi_intr, "fp_startup_trace",
	    "fp_flogi_intr", tnf_opaque, port_ptr, port));

	if (FP_IS_PKT_ERROR(pkt)) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	acc = (la_els_logi_t *)pkt->pkt_resp;
	FP_CP_IN(pkt->pkt_resp_acc, acc, &resp, sizeof (resp));

	ASSERT(resp.ls_code == LA_ELS_ACC);
	if (resp.ls_code != LA_ELS_ACC) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	FP_CP_IN(pkt->pkt_resp_acc, &acc->common_service, &csp, sizeof (csp));
	f_port = FP_IS_F_PORT(csp.cmn_features) ? 1 : 0;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	state = FC_PORT_STATE_MASK(port->fp_state);
	mutex_exit(&port->fp_mutex);

	if (pkt->pkt_resp_fhdr.d_id == 0) {
		if (f_port == 0 && state != FC_STATE_LOOP) {
			swwn = &port->fp_service_params.nport_ww_name;
			FP_CP_IN(pkt->pkt_resp_acc, &acc->nport_ww_name,
			    &dwwn, sizeof (la_wwn_t));
			FP_CP_IN(pkt->pkt_resp_acc, &acc->node_ww_name,
			    &nwwn, sizeof (la_wwn_t));

			mutex_enter(&port->fp_mutex);
			port->fp_topology = FC_TOP_PT_PT;
			port->fp_total_devices = 1;
			if (fctl_wwn_cmp(swwn, &dwwn) >= 0) {
				port->fp_ptpt_master = 1;
				/*
				 * Let us choose 'X' as S_ID and 'Y'
				 * as D_ID and that'll work; hopefully
				 * If not, it will get changed.
				 */
				s_id = port->fp_instance + FP_DEFAULT_SID;
				d_id = port->fp_instance + FP_DEFAULT_DID;
				port->FP_PORT_ID = s_id;
				mutex_exit(&port->fp_mutex);

				pd = fctl_create_port_device(port,
				    &nwwn, &dwwn, d_id, PD_PLOGI_INITIATOR,
				    KM_NOSLEEP);
				if (pd == NULL) {
					fp_printf(port, CE_NOTE, FP_LOG_ONLY,
					    NULL, 0, "couldn't create device"
					    " d_id=%X", d_id);
					fp_iodone(cmd);
					return;
				}

				FP_INIT_CMD(cmd, pkt->pkt_tran_flags,
				    pkt->pkt_tran_type, FP_CMD_PLOGI_RETAIN,
				    fp_retry_count, cmd->cmd_ulp_pkt);

				fp_xlogi_init(port, cmd, s_id, d_id,
				    fp_plogi_intr, cmd->cmd_job, LA_ELS_PLOGI);

				(FP_CMD_TO_PKT(cmd))->pkt_pd = pd;

				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) == FC_SUCCESS) {
					return;
				}
			} else {
				/*
				 * The device handles will be created when the
				 * unsolicited PLOGI is completed successfully
				 */
				port->fp_ptpt_master = 0;
				mutex_exit(&port->fp_mutex);
			}
		}
		pkt->pkt_state = FC_PKT_FAILURE;
	} else {
		if (f_port) {
			mutex_enter(&port->fp_mutex);
			if (state == FC_STATE_LOOP) {
				port->fp_topology = FC_TOP_PUBLIC_LOOP;
			} else {
				port->fp_topology = FC_TOP_FABRIC;
			}
			port->FP_PORT_ID = pkt->pkt_resp_fhdr.d_id;
			mutex_exit(&port->fp_mutex);
		} else {
			pkt->pkt_state = FC_PKT_FAILURE;
		}
	}
	fp_iodone(cmd);
}


/*
 * Handle solicited PLOGI response
 */
static void
fp_plogi_intr(fc_packet_t *pkt)
{
	int			rval;
	int			nl_port;
	uint32_t		d_id;
	fp_cmd_t 		*cmd;
	la_els_logi_t		*acc;
	fc_port_t		*port;
	fc_port_device_t 	*pd;
	la_wwn_t		nwwn;
	la_wwn_t		pwwn;
	ls_code_t		resp;

	nl_port = 0;
	cmd = FP_PKT_TO_CMD(pkt);
	port = FP_CMD_TO_PORT(cmd);
	d_id = pkt->pkt_cmd_fhdr.d_id;

	if (FP_IS_PKT_ERROR(pkt)) {
		int skip_msg = 0;

		if (cmd->cmd_ulp_pkt) {
			cmd->cmd_ulp_pkt->pkt_state = pkt->pkt_state;
			cmd->cmd_ulp_pkt->pkt_reason = pkt->pkt_reason;
			cmd->cmd_ulp_pkt->pkt_action = pkt->pkt_action;
			cmd->cmd_ulp_pkt->pkt_expln = pkt->pkt_expln;
		}

		/*
		 * If an unsolicited cross login already created
		 * a device speed up the discovery by not retrying
		 * the command mindlessly.
		 */
		if (pkt->pkt_pd == NULL &&
		    fctl_get_port_device_by_did(port, d_id) != NULL) {
			fp_iodone(cmd);
			return;
		}

		if (fp_common_intr(pkt, 0) == FC_SUCCESS) {
			return;
		}

		if ((pd = fctl_get_port_device_by_did(port, d_id)) != NULL) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				skip_msg++;
			}
			mutex_exit(&pd->pd_mutex);
		}

		mutex_enter(&port->fp_mutex);
		if (skip_msg == 0 && port->fp_statec_busy == 1 &&
		    pkt->pkt_reason != FC_REASON_FCAL_OPN_FAIL) {
			mutex_exit(&port->fp_mutex);
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, pkt,
			    "PLOGI to %x failed", d_id);
		} else {
			mutex_exit(&port->fp_mutex);
		}

		fp_iodone(cmd);
		return;
	}

	acc = (la_els_logi_t *)pkt->pkt_resp;
	FP_CP_IN(pkt->pkt_resp_acc, acc, &resp, sizeof (resp));

	ASSERT(resp.ls_code == LA_ELS_ACC);
	if (resp.ls_code != LA_ELS_ACC) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	if (d_id == 0xFFFFFC || d_id == 0xFFFFFD) {
		mutex_enter(&port->fp_mutex);
		port->fp_ns_login_class = FC_TRAN_CLASS(pkt->pkt_tran_flags);
		mutex_exit(&port->fp_mutex);
		fp_iodone(cmd);
		return;
	}

	ASSERT(acc == (la_els_logi_t *)pkt->pkt_resp);
	FP_CP_IN(pkt->pkt_resp_acc, &acc->nport_ww_name, &pwwn,
	    sizeof (la_wwn_t));

	FP_CP_IN(pkt->pkt_resp_acc, &acc->node_ww_name, &nwwn,
	    sizeof (la_wwn_t));

	if ((pd = pkt->pkt_pd) == NULL) {
		pd = fctl_get_port_device_by_pwwn(port, &pwwn);
		if (pd == NULL) {
			pd = fctl_create_port_device(port,
			    &nwwn, &pwwn, d_id, PD_PLOGI_INITIATOR,
			    KM_NOSLEEP);
			if (pd == NULL) {
				fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
				    "couldn't create port device handles"
				    " d_id=%x", d_id);
				fp_iodone(cmd);
				return;
			}
		} else {
			fc_port_device_t *tmp_pd;

			tmp_pd = fctl_get_port_device_by_did(port, d_id);
			if (tmp_pd != NULL) {
				fp_iodone(cmd);
				return;
			}

			mutex_enter(&port->fp_mutex);
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				cmd->cmd_flags |= FP_CMD_PLOGI_RETAIN;
			}
			if (pd->pd_type != PORT_DEVICE_NEW) {
				if (pd->PD_PORT_ID != d_id) {
					pd->pd_type = PORT_DEVICE_CHANGED;
					pd->PD_PORT_ID = d_id;
				} else {
					pd->pd_type = PORT_DEVICE_NOCHANGE;
				}
			}
			fctl_enlist_did_table(port, pd);

			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);
		}
		ASSERT(pd != NULL);
	} else {
		mutex_enter(&pd->pd_mutex);
		if (fctl_wwn_cmp(&pd->pd_port_name, &pwwn) == 0) {
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				pd->pd_type = PORT_DEVICE_NOCHANGE;
			}
		} else {
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				pd->pd_type = PORT_DEVICE_CHANGED;
			}
			pd->pd_port_name = pwwn;
		}
		if (pd->pd_porttype.port_type == FC_NS_PORT_NL) {
			nl_port = 1;
		}
		mutex_exit(&pd->pd_mutex);
	}
	fp_register_login(&pkt->pkt_resp_acc, pd, acc,
	    FC_TRAN_CLASS(pkt->pkt_tran_flags));

	if (cmd->cmd_ulp_pkt) {
		cmd->cmd_ulp_pkt->pkt_state = pkt->pkt_state;
		cmd->cmd_ulp_pkt->pkt_action = pkt->pkt_action;
		cmd->cmd_ulp_pkt->pkt_expln = pkt->pkt_expln;
		if (cmd->cmd_ulp_pkt->pkt_pd == NULL) {
			cmd->cmd_ulp_pkt->pkt_pd = pd;
		}
		bcopy((caddr_t)&pkt->pkt_resp_fhdr,
		    (caddr_t)&cmd->cmd_ulp_pkt->pkt_resp_fhdr,
		    sizeof (fc_frame_hdr_t));
		bcopy((caddr_t)pkt->pkt_resp,
		    (caddr_t)cmd->cmd_ulp_pkt->pkt_resp,
		    sizeof (la_els_logi_t));
	}
	pkt->pkt_pd = pd;

	mutex_enter(&port->fp_mutex);
	if (port->fp_topology == FC_TOP_PRIVATE_LOOP || nl_port) {
		mutex_enter(&pd->pd_mutex);
		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, cmd->cmd_flags, fp_retry_count,
		    cmd->cmd_ulp_pkt);
		fp_adisc_init(pd, cmd, cmd->cmd_job);
		FP_INIT_CMD_RESP(pkt, sizeof (la_els_adisc_t),
			sizeof (la_els_adisc_t));
		mutex_exit(&pd->pd_mutex);
		mutex_exit(&port->fp_mutex);
		rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
		if (rval == FC_SUCCESS) {
			return;
		}
	} else {
		mutex_exit(&port->fp_mutex);
	}

	if ((cmd->cmd_flags & FP_CMD_PLOGI_RETAIN) == 0) {
		mutex_enter(&port->fp_mutex);

		mutex_enter(&pd->pd_mutex);
		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, cmd->cmd_flags, fp_retry_count,
		    cmd->cmd_ulp_pkt);
		fp_logo_init(pd, cmd, cmd->cmd_job);
		FP_INIT_CMD_RESP(pkt, sizeof (la_els_logo_t),
		    FP_PORT_IDENTIFIER_LEN);
		mutex_exit(&pd->pd_mutex);

		mutex_exit(&port->fp_mutex);
		rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
		if (rval == FC_SUCCESS) {
			return;
		}

		mutex_enter(&port->fp_mutex);
		if (port->fp_statec_busy == 1) {
			mutex_exit(&port->fp_mutex);

			fp_printf(port, CE_NOTE, FP_LOG_ONLY, rval, NULL,
			    "unable to perform LOGO d_id=%x", d_id);
		} else {
			mutex_exit(&port->fp_mutex);
		}
	}
	fp_iodone(cmd);
}


/*
 * Handle solicited ADISC response
 */
static void
fp_adisc_intr(fc_packet_t *pkt)
{
	int			rval;
	fp_cmd_t		*cmd;
	fc_port_t		*port;
	fc_port_device_t	*pd;
	la_els_adisc_t 		*acc;
	ls_code_t		resp;
	fc_hardaddr_t		ha;

	pd = pkt->pkt_pd;
	cmd = FP_PKT_TO_CMD(pkt);
	port = FP_CMD_TO_PORT(cmd);

	ASSERT(pd != NULL && port != NULL && cmd != NULL);

	if (pkt->pkt_state == FC_PKT_SUCCESS && pkt->pkt_resp_resid == 0) {
		acc = (la_els_adisc_t *)pkt->pkt_resp;
		FP_CP_IN(pkt->pkt_resp_acc, acc, &resp, sizeof (resp));

		if (resp.ls_code == LA_ELS_ACC) {
			int	is_private;

			FP_CP_IN(pkt->pkt_resp_acc, &acc->hard_addr,
			    &ha, sizeof (ha));

			mutex_enter(&port->fp_mutex);
			is_private = (port->fp_topology ==
			    FC_TOP_PRIVATE_LOOP) ? 1 : 0;
			mutex_exit(&port->fp_mutex);

			mutex_enter(&pd->pd_mutex);
			if (pd->pd_type != PORT_DEVICE_NEW) {
				if (pd->PD_HARD_ADDR == ha.hard_addr) {
					pd->pd_type = PORT_DEVICE_NOCHANGE;
				} else {
					pd->pd_type = PORT_DEVICE_CHANGED;
				}
			}

			if (is_private && (ha.hard_addr &&
			    pd->PD_PORT_ID != ha.hard_addr)) {
				char ww_name[17];

				fctl_wwn_to_str(&pd->pd_port_name, ww_name);

				fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
				    "NL_Port Identifier %x doesn't match"
				    " with Hard Address %x, Will use Port"
				    " WWN %s", pd->PD_PORT_ID, ha.hard_addr,
				    ww_name);

				pd->PD_HARD_ADDR = 0;
			} else {
				pd->PD_HARD_ADDR = ha.hard_addr;
			}
			mutex_exit(&pd->pd_mutex);
		} else {
			if (fp_common_intr(pkt, 0) == FC_SUCCESS) {
				return;
			}
		}
	} else {
		if (fp_common_intr(pkt, 0) == FC_SUCCESS) {
			return;
		}

		mutex_enter(&port->fp_mutex);
		if (port->fp_statec_busy == 1) {
			mutex_exit(&port->fp_mutex);
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, pkt,
			    "ADISC to %x failed", pkt->pkt_cmd_fhdr.d_id);
		} else {
			mutex_exit(&port->fp_mutex);
		}
	}

	if ((cmd->cmd_flags & FP_CMD_PLOGI_RETAIN) == 0) {
		mutex_enter(&port->fp_mutex);

		mutex_enter(&pd->pd_mutex);
		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, cmd->cmd_flags, fp_retry_count,
		    cmd->cmd_ulp_pkt);
		fp_logo_init(pd, cmd, cmd->cmd_job);
		FP_INIT_CMD_RESP(pkt, sizeof (la_els_logo_t),
		    FP_PORT_IDENTIFIER_LEN);
		mutex_exit(&pd->pd_mutex);

		mutex_exit(&port->fp_mutex);
		rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
		if (rval == FC_SUCCESS) {
			return;
		}

		mutex_enter(&port->fp_mutex);
		if (port->fp_statec_busy == 1) {
			mutex_exit(&port->fp_mutex);

			fp_printf(port, CE_NOTE, FP_LOG_ONLY, rval, NULL,
			    "unable to perform LOGO d_id=%x",
			    pkt->pkt_cmd_fhdr.d_id);
		} else {
			mutex_exit(&port->fp_mutex);
		}
	}

	fp_iodone(cmd);
}


/*
 * Handle solicited LOGO response
 */
static void
fp_logo_intr(fc_packet_t *pkt)
{
	ls_code_t	resp;

	FP_CP_IN(pkt->pkt_resp_acc, pkt->pkt_resp, &resp, sizeof (resp));
	if (FP_IS_PKT_ERROR(pkt)) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	ASSERT(resp.ls_code == LA_ELS_ACC);
	if (resp.ls_code != LA_ELS_ACC) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	if (pkt->pkt_pd != NULL) {
		fp_unregister_login(pkt->pkt_pd);
	}
	fp_iodone(FP_PKT_TO_CMD(pkt));
}


/*
 * Handle solicited RLS response
 */
static void
fp_rls_intr(fc_packet_t *pkt)
{
	ls_code_t		resp;
	job_request_t		*job;
	fp_cmd_t		*cmd;
	la_els_rls_acc_t	*acc;

	FP_CP_IN(pkt->pkt_resp_acc, pkt->pkt_resp, &resp, sizeof (resp));
	cmd = FP_PKT_TO_CMD(pkt);
	job = cmd->cmd_job;
	ASSERT(job->job_private != NULL);

	/* If failure or LS_RJT then retry the packet, if needed */
	if (FP_IS_PKT_ERROR(pkt) || resp.ls_code != LA_ELS_ACC) {
		job->job_result = fp_common_intr(pkt, 1);
		return;
	}

	/* Save link error status block in memory allocated in ioctl code */
	acc = (la_els_rls_acc_t *)pkt->pkt_resp;
	FP_CP_IN(pkt->pkt_resp_acc, &acc->rls_link_params, job->job_private,
	    sizeof (fc_rls_acc_t));

	/* wakeup the ioctl thread and free the pkt */
	fp_iodone(cmd);
}


/*
 * A solicited command completion interrupt (mostly for commands
 * that require almost no post processing such as SCR ELS)
 */
static void
fp_intr(fc_packet_t *pkt)
{
	if (FP_IS_PKT_ERROR(pkt)) {
		(void) fp_common_intr(pkt, 1);
		return;
	}
	fp_iodone(FP_PKT_TO_CMD(pkt));
}


/*
 * Handle the underlying port's state change
 */
static void
fp_statec_cb(opaque_t port_handle, uint32_t state)
{
	fc_port_t 	*port = (fc_port_t *)port_handle;
	job_request_t 	*job;

	/*
	 * If it is not possible to process the callbacks
	 * just drop the callback on the floor; Don't bother
	 * to do something that isn't safe at this time
	 */
	mutex_enter(&port->fp_mutex);
	if (FP_MUST_DROP_CALLBACKS(port) || FP_ALREADY_IN_STATE(port, state)) {
		mutex_exit(&port->fp_mutex);
		return;
	}
	if (port->fp_statec_busy == 0) {
		port->fp_soft_state |= FP_SOFT_IN_STATEC_CB;
	}
#ifdef	DEBUG
	else {
		ASSERT(port->fp_soft_state & FP_SOFT_IN_STATEC_CB);
	}
#endif /* DEBUG */

	port->fp_statec_busy++;

	/*
	 * For now, force the trusted method of device authentication (by
	 * PLOGI) when LIPs do not involve OFFLINE to ONLINE transition.
	 */
	if (FC_PORT_STATE_MASK(state) == FC_STATE_LIP ||
	    FC_PORT_STATE_MASK(state) == FC_STATE_LIP_LBIT_SET) {
		state = FC_PORT_SPEED_MASK(port->fp_state) | FC_STATE_LOOP;
		fp_port_offline(port, 0);
	}
	mutex_exit(&port->fp_mutex);

	switch (FC_PORT_STATE_MASK(state)) {
	case FC_STATE_OFFLINE:
		job = fctl_alloc_job(JOB_PORT_OFFLINE,
		    JOB_TYPE_FCTL_ASYNC, NULL, NULL, KM_NOSLEEP);
		if (job == NULL) {
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    " fp_statec_cb() couldn't submit a job "
			    " to the thread: failing..");
			mutex_enter(&port->fp_mutex);
			FC_STATEC_DONE(port);
			mutex_exit(&port->fp_mutex);
			return;
		}

		mutex_enter(&port->fp_mutex);
		port->fp_state = state;
		mutex_exit(&port->fp_mutex);

		fctl_enque_job(port, job);
		break;

	case FC_STATE_ONLINE:
	case FC_STATE_LOOP:
		mutex_enter(&port->fp_mutex);
		port->fp_state = state;

		if (port->fp_offline_tid) {
			timeout_id_t tid;

			tid = port->fp_offline_tid;
			port->fp_offline_tid = NULL;
			mutex_exit(&port->fp_mutex);
			(void) untimeout(tid);
		} else {
			mutex_exit(&port->fp_mutex);
		}

		job = fctl_alloc_job(JOB_PORT_ONLINE,
		    JOB_TYPE_FCTL_ASYNC, NULL, NULL, KM_NOSLEEP);
		if (job == NULL) {
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    "fp_statec_cb() couldn't submit a job "
			    "to the thread: failing..");

			mutex_enter(&port->fp_mutex);
			FC_STATEC_DONE(port);
			mutex_exit(&port->fp_mutex);
			return;
		}
		fctl_enque_job(port, job);
		break;

	case FC_STATE_RESET_REQUESTED:
		mutex_enter(&port->fp_mutex);
		port->fp_state = FC_STATE_OFFLINE;
		port->fp_soft_state |= FP_SOFT_IN_FCA_RESET;
		mutex_exit(&port->fp_mutex);
		/* FALLTHROUGH */

	case FC_STATE_RESET:
		job = fctl_alloc_job(JOB_ULP_NOTIFY,
		    JOB_TYPE_FCTL_ASYNC, NULL, NULL, KM_NOSLEEP);
		if (job == NULL) {
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    "fp_statec_cb() couldn't submit a job"
			    " to the thread: failing..");

			mutex_enter(&port->fp_mutex);
			FC_STATEC_DONE(port);
			mutex_exit(&port->fp_mutex);
			return;
		}

		/* squeeze into some field in the job structure */
		job->job_ulp_listlen = FC_PORT_STATE_MASK(state);
		fctl_enque_job(port, job);
		break;

	case FC_STATE_NAMESERVICE:
		/* FALLTHROUGH */
	default:
		mutex_enter(&port->fp_mutex);
		FC_STATEC_DONE(port);
		mutex_exit(&port->fp_mutex);
		break;
	}
}


/*
 * Register with the Name Server for RSCNs
 */
static int
fp_ns_scr(fc_port_t *port, job_request_t *job, uchar_t scr_func, int sleep)
{
	int		rval;
	uint32_t	s_id;
	uchar_t		class;
	fc_scr_req_t 	payload;
	fp_cmd_t 	*cmd;
	fc_packet_t 	*pkt;

	mutex_enter(&port->fp_mutex);
	s_id = port->FP_PORT_ID;
	class = port->fp_ns_login_class;
	mutex_exit(&port->fp_mutex);

	cmd = fp_alloc_pkt(port, sizeof (fc_scr_req_t),
	    sizeof (fc_scr_resp_t), sleep);
	if (cmd == NULL) {
		return (FC_NOMEM);
	}

	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    FP_CMD_CFLAG_UNDEFINED, fp_retry_count, NULL);

	pkt = &cmd->cmd_pkt;
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	fp_els_init(cmd, s_id, 0xFFFFFD, fp_intr, job);

	payload.LS_CODE = LA_ELS_SCR;
	payload.MBZ = 0;
	payload.scr_rsvd = 0;
	payload.scr_func = scr_func;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));

	FCTL_SET_JOB_COUNTER(job, 1);

	rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
	if (rval != FC_SUCCESS) {
		fp_iodone(cmd);
	}

	return (FC_SUCCESS);
}


/*
 * There are basically two methods to determine the total number of
 * devices out in the NS database; Reading the details of the two
 * methods described below, it shouldn't be hard to identify which
 * of the two methods is better.
 *
 *	Method 1.
 *		Iteratively issue GANs until all ports identifiers are walked
 *
 *	Method 2.
 *		Issue GID_PT (get port Identifiers) with Maximum residual
 *		field in the request CT HEADER set to accommodate only the
 *		CT HEADER in the response frame. And if FC-GS2 has been
 *		carefully read, the NS here has a chance to FS_ACC the
 *		request and indicate the residual size in the FS_ACC.
 *
 *	Method 2 is wonderful, although it's not mandatory for the NS
 *	to update the Maximum/Residual Field as can be seen in 4.3.1.6
 *	(note with particular care the use of the auxillary verb 'may')
 *
 */
static int
fp_ns_get_devcount(fc_port_t *port, job_request_t *job, int sleep)
{
	int		flags;
	int		rval;
	uint32_t	src_id;
	fctl_ns_req_t	*ns_cmd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	port->fp_total_devices = 0;
	src_id = port->FP_PORT_ID;
	mutex_exit(&port->fp_mutex);

	if (port->fp_options & FP_NS_SMART_COUNT) {
		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pt_t),
		    0, 0, (FCTL_NS_GET_DEV_COUNT | FCTL_NS_NO_DATA_BUF),
		    sleep);

		if (ns_cmd == NULL) {
			return (FC_NOMEM);
		}
		ns_cmd->ns_cmd_code = NS_GID_PT;
		FCTL_NS_GID_PT_INIT(ns_cmd->ns_cmd_buf,
		    0x7F); /* All port types */
	} else {
		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gan_t),
		    sizeof (ns_resp_gan_t), 0, (FCTL_NS_GET_DEV_COUNT |
		    FCTL_NS_NO_DATA_BUF), sleep);

		if (ns_cmd == NULL) {
			return (FC_NOMEM);
		}
		ns_cmd->ns_gan_index = 0;
		ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
		ns_cmd->ns_cmd_code = NS_GA_NXT;
		ns_cmd->ns_gan_max = 0xFFFF;

		FCTL_NS_GAN_INIT(ns_cmd->ns_cmd_buf, src_id);
	}

	flags = job->job_flags;
	job->job_flags &= ~JOB_TYPE_FP_ASYNC;
	FCTL_SET_JOB_COUNTER(job, 1);
	rval = fp_ns_query(port, ns_cmd, job, 1, sleep);
	job->job_flags = flags;

	if (port->fp_options & FP_NS_SMART_COUNT) {
		uint16_t max_resid;

		max_resid = ns_cmd->ns_resp_hdr.ct_aiusize;
		if (ns_cmd->ns_resp_hdr.ct_cmdrsp == FS_ACC_IU && max_resid) {
			max_resid /= (FP_PORT_IDENTIFIER_LEN);
			mutex_enter(&port->fp_mutex);
			port->fp_total_devices = max_resid;
			mutex_exit(&port->fp_mutex);
		}
	}
	fctl_free_ns_cmd(ns_cmd);

	return (rval);
}


/*
 * One heck of a function to serve userland.
 */
static int
fp_fciocmd(fc_port_t *port, intptr_t data, int mode, fcio_t *fcio)
{
	int 		rval = 0;
	uint32_t	ret;
	uchar_t		open_flag;
	job_request_t 	*job;

	mutex_enter(&port->fp_mutex);
	if (port->fp_soft_state & FP_SOFT_IN_STATEC_CB) {
		fcio->fcio_errno = FC_STATEC_BUSY;
		mutex_exit(&port->fp_mutex);
		rval = EAGAIN;
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		return (rval);
	}
	open_flag = port->fp_flag;
	mutex_exit(&port->fp_mutex);

	if (fp_check_perms(open_flag, fcio->fcio_cmd) != FC_SUCCESS) {
		fcio->fcio_errno = FC_FAILURE;
		rval = EACCES;
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		return (rval);
	}

	/*
	 * If an exclusive open was demanded during open, don't let
	 * either innocuous or devil threads to share the file
	 * descriptor and fire down exclusive access commands
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_flag & FP_EXCL) {
		if (port->fp_flag & FP_EXCL_BUSY) {
			mutex_exit(&port->fp_mutex);
			fcio->fcio_errno = FC_FAILURE;
			return (EBUSY);
		}
		port->fp_flag |= FP_EXCL_BUSY;
	}
	mutex_exit(&port->fp_mutex);

	switch (fcio->fcio_cmd) {
	case FCIO_GET_HOST_PARAMS:
	{
		fc_port_dev_t	*val;

		if (fcio->fcio_olen != sizeof (*val) ||
		    fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}

		val = kmem_zalloc(sizeof (*val), KM_SLEEP);

		mutex_enter(&port->fp_mutex);
		val->dev_did = port->fp_port_id;
		val->dev_hard_addr = port->fp_hard_addr;
		val->dev_pwwn = port->fp_service_params.nport_ww_name;
		val->dev_nwwn = port->fp_service_params.node_ww_name;
		bcopy(port->fp_fc4_types, val->dev_type,
		    sizeof (port->fp_fc4_types));
		mutex_exit(&port->fp_mutex);

		if (fp_copyout((void *)val, (void *)fcio->fcio_obuf,
		    fcio->fcio_olen, mode) == 0) {
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		} else {
			rval = EFAULT;
		}
		kmem_free(val, sizeof (*val));
		break;
	}

	case FCIO_GET_NUM_DEVS:
	{
		int num_devices;

		if (fcio->fcio_olen != sizeof (num_devices) ||
		    fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}

		mutex_enter(&port->fp_mutex);
		switch (port->fp_topology) {
		case FC_TOP_PRIVATE_LOOP:
		case FC_TOP_PT_PT:
			num_devices = port->fp_total_devices;
			fcio->fcio_errno = FC_SUCCESS;
			break;

		case FC_TOP_PUBLIC_LOOP:
		case FC_TOP_FABRIC:
			mutex_exit(&port->fp_mutex);
			job = fctl_alloc_job(JOB_NS_CMD, 0, NULL,
			    NULL, KM_SLEEP);
			ASSERT(job != NULL);

			/*
			 * In FC-GS-2 the Name Server doesn't send out
			 * RSCNs for any Name Server Database updates
			 * When it is finally fixed there is no need
			 * to probe as below and should be removed.
			 */
			(void) fp_ns_get_devcount(port, job, KM_SLEEP);
			fctl_dealloc_job(job);

			mutex_enter(&port->fp_mutex);
			num_devices = port->fp_total_devices;
			fcio->fcio_errno = FC_SUCCESS;
			break;

		case FC_TOP_NO_NS:
			/* FALLTHROUGH */
		case FC_TOP_UNKNOWN:
			/* FALLTHROUGH */
		default:
			num_devices = 0;
			fcio->fcio_errno = FC_SUCCESS;
			break;
		}
		mutex_exit(&port->fp_mutex);

		if (fp_copyout((void *)&num_devices,
		    (void *)fcio->fcio_obuf, fcio->fcio_olen,
		    mode) == 0) {
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		} else {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_GET_DEV_LIST:
	{
		int num_devices;
		int new_count;
		int map_size;

		if (fcio->fcio_xfer != FCIO_XFER_READ ||
		    fcio->fcio_alen != sizeof (new_count)) {
			rval = EINVAL;
			break;
		}

		num_devices = fcio->fcio_olen / sizeof (fc_port_dev_t);

		mutex_enter(&port->fp_mutex);
		if (num_devices < port->fp_total_devices) {
			fcio->fcio_errno = FC_TOOMANY;
			new_count = port->fp_total_devices;
			mutex_exit(&port->fp_mutex);

			if (fp_copyout((void *)&new_count,
			    (void *)fcio->fcio_abuf,
			    sizeof (new_count), mode)) {
				rval = EFAULT;
				break;
			}

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
				break;
			}
			rval = EINVAL;
			break;
		}

		if (port->fp_total_devices <= 0) {
			fcio->fcio_errno = FC_NO_MAP;
			new_count = port->fp_total_devices;
			mutex_exit(&port->fp_mutex);

			if (fp_copyout((void *)&new_count,
			    (void *)fcio->fcio_abuf,
			    sizeof (new_count), mode)) {
				rval = EFAULT;
				break;
			}

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
				break;
			}
			rval = EINVAL;
			break;
		}

		switch (port->fp_topology) {
		case FC_TOP_PRIVATE_LOOP:
			if (fp_fillout_loopmap(port, fcio,
			    mode) != FC_SUCCESS) {
				rval = EFAULT;
				break;
			}
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;

		case FC_TOP_PT_PT:
			break;

		case FC_TOP_PUBLIC_LOOP:
		case FC_TOP_FABRIC:
		{
			fctl_ns_req_t *ns_cmd;

			map_size = sizeof (fc_port_dev_t) *
			    port->fp_total_devices;
			mutex_exit(&port->fp_mutex);

			ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gan_t),
			    sizeof (ns_resp_gan_t), map_size,
			    (FCTL_NS_FILL_NS_MAP | FCTL_NS_BUF_IS_USERLAND),
			    KM_SLEEP);
			ASSERT(ns_cmd != NULL);

			ns_cmd->ns_gan_index = 0;
			ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
			ns_cmd->ns_cmd_code = NS_GA_NXT;
			ns_cmd->ns_gan_max = map_size / sizeof (fc_port_dev_t);

			job = fctl_alloc_job(JOB_PORT_GETMAP, 0, NULL,
			    NULL, KM_SLEEP);
			ASSERT(job != NULL);

			ret = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);

			if (rval != FC_SUCCESS ||
			    job->job_result != FC_SUCCESS) {
				fctl_free_ns_cmd(ns_cmd);

				fcio->fcio_errno = job->job_result;
				new_count = 0;
				if (fp_copyout((void *)&new_count,
				    (void *)fcio->fcio_abuf,
				    sizeof (new_count), mode)) {
					fctl_dealloc_job(job);
					mutex_enter(&port->fp_mutex);
					rval = EFAULT;
					break;
				}

				if (fp_fcio_copyout(fcio, data, mode)) {
					fctl_dealloc_job(job);
					mutex_enter(&port->fp_mutex);
					rval = EFAULT;
					break;
				}
				rval = EIO;
				mutex_enter(&port->fp_mutex);
				break;
			}
			fctl_dealloc_job(job);

			new_count = ns_cmd->ns_gan_index;
			if (fp_copyout((void *)&new_count,
			    (void *)fcio->fcio_abuf, sizeof (new_count),
			    mode)) {
				rval = EFAULT;
				fctl_free_ns_cmd(ns_cmd);
				mutex_enter(&port->fp_mutex);
				break;
			}

			if (fp_copyout((void *)ns_cmd->ns_data_buf,
			    (void *)fcio->fcio_obuf, sizeof (fc_port_dev_t) *
			    ns_cmd->ns_gan_index, mode)) {
				rval = EFAULT;
				fctl_free_ns_cmd(ns_cmd);
				mutex_enter(&port->fp_mutex);
				break;
			}
			fctl_free_ns_cmd(ns_cmd);

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			mutex_enter(&port->fp_mutex);
			break;
		}

		case FC_TOP_NO_NS:
			/* FALLTHROUGH */
		case FC_TOP_UNKNOWN:
			/* FALLTHROUGH */
		default:
			fcio->fcio_errno = FC_NO_MAP;
			num_devices = port->fp_total_devices;

			if (fp_copyout((void *)&new_count,
			    (void *)fcio->fcio_abuf,
			    sizeof (new_count), mode)) {
				rval = EFAULT;
				break;
			}

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
				break;
			}
			rval = EINVAL;
			break;
		}
		mutex_exit(&port->fp_mutex);
		break;
	}

	case FCIO_GET_SYM_PNAME:
	{
		rval = ENOTSUP;
		break;

	}

	case FCIO_GET_SYM_NNAME:
	{
		rval = ENOTSUP;
		break;

	}

	case FCIO_SET_SYM_PNAME:
	{
		rval = ENOTSUP;
		break;

	}

	case FCIO_SET_SYM_NNAME:
	{
		rval = ENOTSUP;
		break;

	}

	case FCIO_GET_LOGI_PARAMS:
	{
		la_wwn_t		pwwn;
		la_wwn_t		*my_pwwn;
		la_els_logi_t		*params;
		fc_device_t		*node;
		fc_port_device_t	*pd;

		if (fcio->fcio_ilen != sizeof (la_wwn_t) ||
		    fcio->fcio_olen != sizeof (la_els_logi_t) ||
		    (fcio->fcio_xfer & FCIO_XFER_READ) == 0 ||
		    (fcio->fcio_xfer & FCIO_XFER_WRITE) == 0) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn, sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}

		pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
		if (pd == NULL) {
			mutex_enter(&port->fp_mutex);
			my_pwwn = &port->fp_service_params.nport_ww_name;
			mutex_exit(&port->fp_mutex);

			if (fctl_wwn_cmp(&pwwn, my_pwwn) != 0) {
				rval = ENXIO;
				break;
			}

			params = kmem_zalloc(sizeof (*params), KM_SLEEP);
			mutex_enter(&port->fp_mutex);
			*params = port->fp_service_params;
			mutex_exit(&port->fp_mutex);
		} else {
			params = kmem_zalloc(sizeof (*params), KM_SLEEP);

			mutex_enter(&pd->pd_mutex);
			params->MBZ = params->LS_CODE = 0;
			params->common_service = pd->pd_csp;
			params->nport_ww_name = pd->pd_port_name;
			params->class_1 = pd->pd_clsp1;
			params->class_2 = pd->pd_clsp2;
			params->class_3 = pd->pd_clsp3;
			node = pd->pd_device;
			mutex_exit(&pd->pd_mutex);

			bzero(params->reserved, sizeof (params->reserved));

			mutex_enter(&node->fd_mutex);
			bcopy(node->fd_vv, params->vendor_version,
			    sizeof (node->fd_vv));
			params->node_ww_name = node->fd_node_name;
			mutex_exit(&node->fd_mutex);

			fctl_release_port_device(pd);
		}

		if (ddi_copyout((void *)params, (void *)fcio->fcio_obuf,
		    sizeof (*params), mode)) {
			rval = EFAULT;
		}
		kmem_free(params, sizeof (*params));
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_DEV_LOGIN:
	{
		uint32_t 		d_id;
		la_wwn_t		pwwn;
		fc_port_device_t 	*pd = NULL;
		fctl_ns_req_t		*ns_cmd;
		fc_portmap_t		*changelist;

		if (fcio->fcio_ilen != sizeof (la_wwn_t) ||
		    fcio->fcio_xfer != FCIO_XFER_WRITE) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn, sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}
		job = fctl_alloc_job(JOB_PLOGI_ONE, 0, NULL, NULL, KM_SLEEP);

		mutex_enter(&port->fp_mutex);
		if (FC_IS_TOP_SWITCH(port->fp_topology)) {
			mutex_exit(&port->fp_mutex);
			FCTL_SET_JOB_COUNTER(job, 1);
			job->job_result = FC_SUCCESS;

			ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
			    sizeof (ns_resp_gid_pn_t),
			    sizeof (ns_resp_gid_pn_t),
			    FCTL_NS_BUF_IS_USERLAND, KM_SLEEP);
			ASSERT(ns_cmd != NULL);

			ns_cmd->ns_cmd_code = NS_GID_PN;
			FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_buf, (&pwwn));

			ret = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);

			if (ret != FC_SUCCESS || job->job_result !=
			    FC_SUCCESS) {
				if (ret != FC_SUCCESS) {
					fcio->fcio_errno = ret;
				} else {
					fcio->fcio_errno = job->job_result;
				}
				fctl_free_ns_cmd(ns_cmd);
				fctl_dealloc_job(job);
				rval = EIO;
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
				break;
			}
			d_id = ((ns_resp_gid_pn_t *)
			    ns_cmd->ns_data_buf)->NS_PID;
			fctl_free_ns_cmd(ns_cmd);
		} else {
			mutex_exit(&port->fp_mutex);

			pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
			if (pd == NULL) {
				fcio->fcio_errno = FC_BADWWN;
				fctl_dealloc_job(job);
				rval = EIO;
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
				break;
			}

			mutex_enter(&pd->pd_mutex);
			d_id = pd->PD_PORT_ID;
			mutex_exit(&pd->pd_mutex);
		}

		FCTL_SET_JOB_COUNTER(job, 1);
		job->job_private = &d_id;

		fctl_enque_job(port, job);
		fctl_jobwait(job);
		fcio->fcio_errno = job->job_result;

		if (pd) {
			fctl_release_port_device(pd);
		}

		if (job->job_result != FC_SUCCESS) {
			rval = EIO;
		} else {
			pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
			ASSERT(pd != NULL);

			changelist = (fc_portmap_t *)kmem_zalloc(
			    sizeof (*changelist), KM_SLEEP);
			ASSERT(changelist != NULL);

			fctl_copy_portmap(changelist, pd);
			changelist->map_flags = PORT_DEVICE_USER_ADD;
			(void) fp_ulp_devc_cb(port, changelist, 1, 1, KM_SLEEP);

			mutex_enter(&pd->pd_mutex);
			pd->pd_type = PORT_DEVICE_NOCHANGE;
			mutex_exit(&pd->pd_mutex);

			fctl_release_port_device(pd);
		}

		fctl_dealloc_job(job);
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_DEV_LOGOUT:
	{
		la_wwn_t		pwwn;
		fp_cmd_t		*cmd;
		fc_portmap_t		*changelist;
		fc_port_device_t 	*pd;

		if (fcio->fcio_ilen != sizeof (la_wwn_t) ||
		    fcio->fcio_xfer != FCIO_XFER_WRITE) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn, sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}

		pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
		if (pd == NULL) {
			fcio->fcio_errno = FC_BADWWN;
			rval = ENXIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		mutex_enter(&pd->pd_mutex);
		if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
			fcio->fcio_errno = FC_LOGINREQ;
			mutex_exit(&pd->pd_mutex);

			fctl_release_port_device(pd);
			rval = EINVAL;
			break;
		}
		ASSERT(pd->pd_count >= 1);

		if (pd->pd_count > 1) {
			pd->pd_count--;
			fcio->fcio_errno = FC_SUCCESS;
			mutex_exit(&pd->pd_mutex);

			changelist = (fc_portmap_t *)kmem_zalloc(
			    sizeof (*changelist), KM_SLEEP);
			ASSERT(changelist != NULL);

			fctl_copy_portmap(changelist, pd);
			changelist->map_flags = PORT_DEVICE_USER_REMOVE;

			fctl_release_port_device(pd);

			(void) fp_ulp_devc_cb(port, changelist, 1, 1, KM_SLEEP);
			break;
		}
		mutex_exit(&pd->pd_mutex);

		job = fctl_alloc_job(JOB_LOGO_ONE, JOB_TYPE_FP_ASYNC,
		    NULL, NULL, KM_SLEEP);
		ASSERT(job != NULL);

		FCTL_SET_JOB_COUNTER(job, 1);

		cmd = fp_alloc_pkt(port, sizeof (la_els_logo_t),
		    FP_PORT_IDENTIFIER_LEN, KM_SLEEP);
		if (cmd == NULL) {
			fcio->fcio_errno = FC_NOMEM;
			rval = ENOMEM;
			fctl_release_port_device(pd);

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		mutex_enter(&port->fp_mutex);
		mutex_enter(&pd->pd_mutex);

		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, FP_CMD_PLOGI_DONT_CARE, 1, NULL);
		fp_logo_init(pd, cmd, job);

		mutex_exit(&pd->pd_mutex);
		mutex_exit(&port->fp_mutex);
		if (fp_sendcmd(port, cmd, port->fp_fca_handle) == FC_SUCCESS) {
			fctl_jobwait(job);

			fcio->fcio_errno = job->job_result;
			if (job->job_result == FC_SUCCESS) {
				ASSERT(pd != NULL);

				changelist = (fc_portmap_t *)kmem_zalloc(
				    sizeof (*changelist), KM_SLEEP);
				ASSERT(changelist != NULL);

				fctl_copy_portmap(changelist, pd);
				changelist->map_flags = PORT_DEVICE_USER_REMOVE;

				fctl_release_port_device(pd);

				(void) fp_ulp_devc_cb(port, changelist,
				    1, 1, KM_SLEEP);
			} else {
				fctl_release_port_device(pd);
				rval = EIO;
			}
		} else {
			fp_free_pkt(cmd);
			fctl_release_port_device(pd);
		}

		fctl_dealloc_job(job);
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_GET_STATE:
	{
		la_wwn_t		pwwn;
		uint32_t		state;
		fc_port_device_t 	*pd;

		if (fcio->fcio_ilen != sizeof (la_wwn_t) ||
		    fcio->fcio_olen != sizeof (state) ||
		    (fcio->fcio_xfer & FCIO_XFER_WRITE) == 0 ||
		    (fcio->fcio_xfer & FCIO_XFER_READ) == 0) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn, sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}

		pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
		if (pd == NULL) {
			fcio->fcio_errno = FC_BADWWN;
			rval = ENXIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		mutex_enter(&pd->pd_mutex);
		state = pd->pd_state;
		mutex_exit(&pd->pd_mutex);

		fctl_release_port_device(pd);

		if (ddi_copyout((void *)&state, (void *)fcio->fcio_obuf,
		    sizeof (state), mode) == 0) {
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		} else {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_DEV_REMOVE:
	{
		la_wwn_t	pwwn;
		fp_cmd_t	*cmd;
		fc_portmap_t	*changelist;
		fc_port_device_t *pd;

		if (fcio->fcio_ilen != sizeof (la_wwn_t) ||
		    fcio->fcio_xfer != FCIO_XFER_WRITE) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn, sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}

		pd = fctl_hold_port_device_by_pwwn(port, &pwwn);
		if (pd == NULL) {
			rval = ENXIO;
			fcio->fcio_errno = FC_BADWWN;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		mutex_enter(&pd->pd_mutex);
		if (pd->pd_held > 1) {
			mutex_exit(&pd->pd_mutex);

			rval = EBUSY;
			fcio->fcio_errno = FC_FAILURE;
			fctl_release_port_device(pd);

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
			ASSERT(pd->pd_count == 0);

			pd->pd_type = PORT_DEVICE_OLD;
			pd->pd_state = PORT_DEVICE_INVALID;
			mutex_exit(&pd->pd_mutex);

			fctl_release_port_device(pd);

			mutex_enter(&port->fp_mutex);
			mutex_enter(&pd->pd_mutex);

			fctl_delist_did_table(port, pd);
			fctl_delist_pwwn_table(port, pd);

			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);

			changelist = (fc_portmap_t *)kmem_zalloc(
			    sizeof (*changelist), KM_SLEEP);
			ASSERT(changelist != NULL);

			fctl_copy_portmap(changelist, pd);
			changelist->map_flags = PORT_DEVICE_USER_REMOVE;
			(void) fp_ulp_devc_cb(port, changelist, 1, 1, KM_SLEEP);
			break;
		}
		ASSERT(pd->pd_count >= 1);

		if (pd->pd_count > 1) {
			fcio->fcio_errno = FC_FAILURE;
			mutex_exit(&pd->pd_mutex);

			fctl_release_port_device(pd);
			rval = EBUSY;
			break;
		}
		mutex_exit(&pd->pd_mutex);

		job = fctl_alloc_job(JOB_LOGO_ONE, JOB_TYPE_FP_ASYNC,
		    NULL, NULL, KM_NOSLEEP);
		if (job == NULL) {
			rval = ENOMEM;
			fctl_release_port_device(pd);
			break;
		}
		FCTL_SET_JOB_COUNTER(job, 1);

		cmd = fp_alloc_pkt(port, sizeof (la_els_logo_t),
		    FP_PORT_IDENTIFIER_LEN, KM_SLEEP);
		if (cmd == NULL) {
			rval = ENOMEM;
			fctl_release_port_device(pd);
			break;
		}

		mutex_enter(&port->fp_mutex);
		mutex_enter(&pd->pd_mutex);

		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, FP_CMD_PLOGI_DONT_CARE, 1, NULL);
		fp_logo_init(pd, cmd, job);

		mutex_exit(&pd->pd_mutex);
		mutex_exit(&port->fp_mutex);
		if ((ret = fp_sendcmd(port, cmd, port->fp_fca_handle))
		    == FC_SUCCESS) {
			fctl_jobwait(job);
			if (job->job_result == FC_SUCCESS) {
				ASSERT(pd != NULL);

				mutex_enter(&port->fp_mutex);
				mutex_enter(&pd->pd_mutex);

				pd->pd_type = PORT_DEVICE_OLD;
				pd->pd_state = PORT_DEVICE_INVALID;
				fctl_delist_did_table(port, pd);
				fctl_delist_pwwn_table(port, pd);

				mutex_exit(&pd->pd_mutex);
				mutex_exit(&port->fp_mutex);

				changelist = (fc_portmap_t *)kmem_zalloc(
				    sizeof (*changelist), KM_SLEEP);
				ASSERT(changelist != NULL);

				fctl_copy_portmap(changelist, pd);
				changelist->map_pd = pd;
				changelist->map_flags = PORT_DEVICE_USER_REMOVE;

				fctl_release_port_device(pd);

				(void) fp_ulp_devc_cb(port, changelist,
				    1, 1, KM_SLEEP);
			} else {
				rval = EIO;
				fcio->fcio_errno = job->job_result;
				fctl_release_port_device(pd);

				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			}
		} else {
			fcio->fcio_errno = ret;
			fp_free_pkt(cmd);
			fctl_release_port_device(pd);

			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}

		fctl_dealloc_job(job);
		break;
	}

	case FCIO_GET_FCODE_REV:
	{
		caddr_t 	fcode_rev;
		fc_fca_pm_t	pm;

		if (fcio->fcio_olen < FC_FCODE_REV_SIZE ||
		    fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}
		bzero((caddr_t)&pm, sizeof (pm));

		fcode_rev = kmem_zalloc(fcio->fcio_olen, KM_SLEEP);
		ASSERT(fcode_rev != NULL);

		pm.pm_cmd_flags = FC_FCA_PM_READ;
		pm.pm_cmd_code = FC_PORT_GET_FCODE_REV;
		pm.pm_data_len = fcio->fcio_olen;
		pm.pm_data_buf = fcode_rev;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret == FC_SUCCESS) {
			if (ddi_copyout((void *)fcode_rev,
			    (void *)fcio->fcio_obuf,
			    fcio->fcio_olen, mode) == 0) {
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			} else {
				rval = EFAULT;
			}
		} else {
			rval = EIO;
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		kmem_free(fcode_rev, fcio->fcio_olen);
		break;
	}

	case FCIO_GET_FW_REV:
	{
		caddr_t 	fw_rev;
		fc_fca_pm_t	pm;

		if (fcio->fcio_olen < FC_FW_REV_SIZE ||
		    fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}
		bzero((caddr_t)&pm, sizeof (pm));

		fw_rev = kmem_zalloc(fcio->fcio_olen, KM_SLEEP);
		ASSERT(fw_rev != NULL);

		pm.pm_cmd_flags = FC_FCA_PM_READ;
		pm.pm_cmd_code = FC_PORT_GET_FW_REV;
		pm.pm_data_len = fcio->fcio_olen;
		pm.pm_data_buf = fw_rev;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret == FC_SUCCESS) {
			if (ddi_copyout((void *)fw_rev,
			    (void *)fcio->fcio_obuf,
			    fcio->fcio_olen, mode) == 0) {
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			} else {
				rval = EFAULT;
			}
		} else {
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			rval = EIO;
		}
		kmem_free(fw_rev, fcio->fcio_olen);
		break;
	}

	case FCIO_GET_DUMP_SIZE:
	{
		uint32_t 	dump_size;
		fc_fca_pm_t	pm;

		if (fcio->fcio_olen != sizeof (dump_size) ||
		    fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}
		bzero((caddr_t)&pm, sizeof (pm));
		pm.pm_cmd_flags = FC_FCA_PM_READ;
		pm.pm_cmd_code = FC_PORT_GET_DUMP_SIZE;
		pm.pm_data_len = sizeof (dump_size);
		pm.pm_data_buf = (caddr_t)&dump_size;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret == FC_SUCCESS) {
			if (ddi_copyout((void *)&dump_size,
			    (void *)fcio->fcio_obuf, sizeof (dump_size),
			    mode) == 0) {
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			} else {
				rval = EFAULT;
			}
		} else {
			fcio->fcio_errno = ret;
			rval = EIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		break;
	}

	case FCIO_DOWNLOAD_FW:
	{
		caddr_t		firmware;
		fc_fca_pm_t	pm;

		if (fcio->fcio_ilen <= 0 ||
		    fcio->fcio_xfer != FCIO_XFER_WRITE) {
			rval = EINVAL;
			break;
		}

		firmware = kmem_zalloc(fcio->fcio_ilen, KM_SLEEP);
		if (ddi_copyin(fcio->fcio_ibuf, firmware,
		    fcio->fcio_ilen, mode)) {
			rval = EFAULT;
			break;
		}

		bzero((caddr_t)&pm, sizeof (pm));
		pm.pm_cmd_flags = FC_FCA_PM_WRITE;
		pm.pm_cmd_code = FC_PORT_DOWNLOAD_FW;
		pm.pm_data_len = fcio->fcio_ilen;
		pm.pm_data_buf = firmware;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		kmem_free(firmware, fcio->fcio_ilen);

		if (ret != FC_SUCCESS) {
			fcio->fcio_errno = ret;
			rval = EIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		break;
	}

	case FCIO_DOWNLOAD_FCODE:
	{
		caddr_t		fcode;
		fc_fca_pm_t	pm;

		if (fcio->fcio_ilen <= 0 ||
		    fcio->fcio_xfer != FCIO_XFER_WRITE) {
			rval = EINVAL;
			break;
		}

		fcode = kmem_zalloc(fcio->fcio_ilen, KM_SLEEP);
		if (ddi_copyin(fcio->fcio_ibuf, fcode,
		    fcio->fcio_ilen, mode)) {
			rval = EFAULT;
			break;
		}

		bzero((caddr_t)&pm, sizeof (pm));
		pm.pm_cmd_flags = FC_FCA_PM_WRITE;
		pm.pm_cmd_code = FC_PORT_DOWNLOAD_FCODE;
		pm.pm_data_len = fcio->fcio_ilen;
		pm.pm_data_buf = fcode;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		kmem_free(fcode, fcio->fcio_ilen);

		if (ret != FC_SUCCESS) {
			fcio->fcio_errno = ret;
			rval = EIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		break;
	}

	case FCIO_FORCE_DUMP:
		ret = port->fp_fca_tran->fca_reset(
			port->fp_fca_handle, FC_FCA_CORE);

		if (ret != FC_SUCCESS) {
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			rval = EIO;
		}
		break;

	case FCIO_GET_DUMP:
	{
		caddr_t		dump;
		uint32_t 	dump_size;
		fc_fca_pm_t	pm;

		if (fcio->fcio_xfer != FCIO_XFER_READ) {
			rval = EINVAL;
			break;
		}
		bzero((caddr_t)&pm, sizeof (pm));

		pm.pm_cmd_flags = FC_FCA_PM_READ;
		pm.pm_cmd_code = FC_PORT_GET_DUMP_SIZE;
		pm.pm_data_len = sizeof (dump_size);
		pm.pm_data_buf = (caddr_t)&dump_size;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret != FC_SUCCESS) {
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			rval = EIO;
			break;
		}
		if (fcio->fcio_olen != dump_size) {
			fcio->fcio_errno = FC_NOMEM;
			rval = EINVAL;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		dump = kmem_zalloc(dump_size, KM_SLEEP);
		ASSERT(dump != NULL);

		bzero((caddr_t)&pm, sizeof (pm));
		pm.pm_cmd_flags = FC_FCA_PM_READ;
		pm.pm_cmd_code = FC_PORT_GET_DUMP;
		pm.pm_data_len = dump_size;
		pm.pm_data_buf = dump;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret == FC_SUCCESS) {
			if (ddi_copyout((void *)dump, (void *)fcio->fcio_obuf,
			    dump_size, mode) == 0) {
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			} else {
				rval = EFAULT;
			}
		} else {
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			rval = EIO;
		}
		kmem_free(dump, dump_size);
		break;
	}

	case FCIO_GET_TOPOLOGY:
	{
		uint32_t user_topology;

		if (fcio->fcio_xfer != FCIO_XFER_READ ||
		    fcio->fcio_olen != sizeof (user_topology)) {
			rval = EINVAL;
			break;
		}

		mutex_enter(&port->fp_mutex);
		user_topology = port->fp_topology;
		mutex_exit(&port->fp_mutex);

		if (ddi_copyout((void *)&user_topology,
		    (void *)fcio->fcio_obuf, sizeof (user_topology),
		    mode)) {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_RESET_LINK:
	{
		la_wwn_t pwwn;

		/*
		 * Look at the output buffer field; if this field has zero
		 * bytes then attempt to reset the local link/loop. If the
		 * fcio_ibuf field points to a WWN, see if it's an NL_Port,
		 * and if yes, determine the LFA and reset the remote LIP
		 * by LINIT ELS.
		 */

		if (fcio->fcio_xfer != FCIO_XFER_WRITE ||
		    fcio->fcio_ilen != sizeof (pwwn)) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &pwwn,
		    sizeof (pwwn), mode)) {
			rval = EFAULT;
			break;
		}

		mutex_enter(&port->fp_mutex);
		if (port->fp_soft_state & FP_SOFT_IN_LINK_RESET) {
			mutex_exit(&port->fp_mutex);
			break;
		}
		port->fp_soft_state |= FP_SOFT_IN_LINK_RESET;
		mutex_exit(&port->fp_mutex);

		job = fctl_alloc_job(JOB_LINK_RESET, 0, NULL, NULL, KM_SLEEP);
		if (job == NULL) {
			rval = ENOMEM;
			break;
		}
		FCTL_SET_JOB_COUNTER(job, 1);
		job->job_private = (void *)&pwwn;

		fctl_enque_job(port, job);
		fctl_jobwait(job);

		mutex_enter(&port->fp_mutex);
		port->fp_soft_state &= ~FP_SOFT_IN_LINK_RESET;
		mutex_exit(&port->fp_mutex);

		if (job->job_result != FC_SUCCESS) {
			fcio->fcio_errno = job->job_result;
			rval = EIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		fctl_dealloc_job(job);
		break;
	}

	case FCIO_RESET_HARD:
		ret = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_RESET);
		if (ret != FC_SUCCESS) {
			fcio->fcio_errno = ret;
			rval = EIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		break;

	case FCIO_RESET_HARD_CORE:
		ret = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_RESET_CORE);
		if (ret != FC_SUCCESS) {
			rval = EIO;
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		break;

	case FCIO_DIAG:
	{
		uint32_t 	diag_code;
		uint32_t	diag_result;
		caddr_t		diag_data;
		fc_fca_pm_t	pm;

		if (fcio->fcio_ilen != sizeof (diag_code)) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin(fcio->fcio_ibuf, &diag_code,
		    sizeof (diag_code), mode)) {
			rval = EFAULT;
			break;
		}
		bzero((caddr_t)&pm, sizeof (pm));
		diag_data = NULL;

		if (fcio->fcio_alen > 0) {
			diag_data = kmem_zalloc(fcio->fcio_alen, KM_SLEEP);
			if (ddi_copyin(fcio->fcio_abuf, diag_data,
			    fcio->fcio_alen, mode)) {
				kmem_free(diag_data, fcio->fcio_alen);
				rval = EFAULT;
				break;
			}
			pm.pm_cmd_flags = FC_FCA_PM_RW;
			pm.pm_data_len = fcio->fcio_alen;
			pm.pm_data_buf = fcio->fcio_abuf;
		} else {
			pm.pm_cmd_flags = FC_FCA_PM_READ;
		}

		pm.pm_cmd_code = FC_PORT_DIAG;
		pm.pm_cmd_buf = (caddr_t)&diag_code;
		pm.pm_cmd_len = sizeof (diag_code);
		pm.pm_stat_len = sizeof (diag_result);
		pm.pm_stat_buf = (caddr_t)&diag_result;

		ret = port->fp_fca_tran->fca_port_manage(
		    port->fp_fca_handle, &pm);

		if (ret != FC_SUCCESS) {
			rval = EIO;
			fcio->fcio_errno = ret;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
		}
		if (diag_data != NULL) {
			kmem_free(diag_data, fcio->fcio_alen);
		}
		break;
	}

	case FCIO_LINK_STATUS:
	{
		fc_portid_t		rls_req;
		fc_rls_acc_t		*rls_acc;
		fc_fca_pm_t		pm;
		uint32_t		dest, src_id;
		fp_cmd_t		*cmd;
		fc_port_device_t	*pd;

		/* validate parameters */
		if (fcio->fcio_ilen != sizeof (fc_portid_t) ||
		    fcio->fcio_olen != sizeof (fc_rls_acc_t) ||
		    fcio->fcio_xfer != FCIO_XFER_RW) {
			rval = EINVAL;
			break;
		}

		if ((fcio->fcio_cmd_flags != FCIO_CFLAGS_RLS_DEST_FPORT) &&
		    (fcio->fcio_cmd_flags != FCIO_CFLAGS_RLS_DEST_NPORT)) {
			rval = EINVAL;
			break;
		}

		if (ddi_copyin((void *)fcio->fcio_ibuf, (void *)&rls_req,
		    sizeof (fc_portid_t), mode)) {
			rval = EFAULT;
			break;
		}


		/* Determine the destination of the RLS frame */
		if (fcio->fcio_cmd_flags == FCIO_CFLAGS_RLS_DEST_FPORT) {
			dest = FS_FABRIC_F_PORT;
		} else {
			dest = rls_req.port_id;
		}

		mutex_enter(&port->fp_mutex);
		src_id = port->FP_PORT_ID;
		mutex_exit(&port->fp_mutex);

		/* If dest is zero OR same as FCA ID, then use port_manage() */
		if (dest == 0 || dest == src_id) {

			/* Allocate memory for link error status block */
			rls_acc = kmem_zalloc(sizeof (*rls_acc), KM_SLEEP);
			ASSERT(rls_acc != NULL);

			/* Prepare the port management structure */
			bzero((caddr_t)&pm, sizeof (pm));

			pm.pm_cmd_flags = FC_FCA_PM_READ;
			pm.pm_cmd_code  = FC_PORT_RLS;
			pm.pm_data_len  = sizeof (*rls_acc);
			pm.pm_data_buf  = (caddr_t)rls_acc;

			/* Get the adapter's link error status block */
			ret = port->fp_fca_tran->fca_port_manage(
			    port->fp_fca_handle, &pm);

			if (ret == FC_SUCCESS) {
				/* xfer link status block to userland */
				if (ddi_copyout((void *)rls_acc,
				    (void *)fcio->fcio_obuf,
				    sizeof (*rls_acc), mode) == 0) {
					if (fp_fcio_copyout(fcio, data,
					    mode)) {
						rval = EFAULT;
					}
				} else {
					rval = EFAULT;
				}
			} else {
				rval = EIO;
				fcio->fcio_errno = ret;
				if (fp_fcio_copyout(fcio, data, mode)) {
					rval = EFAULT;
				}
			}

			kmem_free(rls_acc, sizeof (*rls_acc));

			/* ioctl handling is over */
			break;
		}

		/*
		 * Send RLS to the destination port.
		 * Having RLS frame destination is as FPORT is not yet
		 * supported and will be implemented in future, if needed.
		 * Following call to get "pd" will fail if dest is FPORT
		 */
		pd = fctl_hold_port_device_by_did(port, dest);
		if (pd == NULL) {
			fcio->fcio_errno = FC_BADOBJECT;
			rval = ENXIO;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		mutex_enter(&pd->pd_mutex);
		if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
			mutex_exit(&pd->pd_mutex);
			fctl_release_port_device(pd);

			fcio->fcio_errno = FC_LOGINREQ;
			rval = EINVAL;
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}
		ASSERT(pd->pd_count >= 1);
		mutex_exit(&pd->pd_mutex);

		/*
		 * Allocate job structure and set job_code as DUMMY,
		 * because we will not go thorugh the job thread.
		 * Instead fp_sendcmd() is called directly here.
		 */
		job = fctl_alloc_job(JOB_DUMMY, JOB_TYPE_FP_ASYNC,
		    NULL, NULL, KM_SLEEP);
		ASSERT(job != NULL);

		FCTL_SET_JOB_COUNTER(job, 1);

		cmd = fp_alloc_pkt(port, sizeof (la_els_rls_t),
		    sizeof (la_els_rls_acc_t), KM_SLEEP);
		if (cmd == NULL) {
			fcio->fcio_errno = FC_NOMEM;
			rval = ENOMEM;

			fctl_release_port_device(pd);

			fctl_dealloc_job(job);
			if (fp_fcio_copyout(fcio, data, mode)) {
				rval = EFAULT;
			}
			break;
		}

		/* Allocate memory for link error status block */
		rls_acc = kmem_zalloc(sizeof (*rls_acc), KM_SLEEP);
		ASSERT(rls_acc != NULL);

		mutex_enter(&port->fp_mutex);
		mutex_enter(&pd->pd_mutex);

		FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
		    FC_PKT_EXCHANGE, FP_CMD_CFLAG_UNDEFINED, 1, NULL);

		fp_rls_init(pd, cmd, job);

		job->job_private = (void *)rls_acc;

		mutex_exit(&pd->pd_mutex);
		mutex_exit(&port->fp_mutex);

		if (fp_sendcmd(port, cmd, port->fp_fca_handle) == FC_SUCCESS) {
			fctl_jobwait(job);

			fcio->fcio_errno = job->job_result;
			if (job->job_result == FC_SUCCESS) {
				ASSERT(pd != NULL);
				/*
				 * link error status block is now available.
				 * Copy it to userland
				 */
				ASSERT(job->job_private == (void *)rls_acc);
				if (ddi_copyout((void *)rls_acc,
				    (void *)fcio->fcio_obuf,
				    sizeof (*rls_acc), mode) == 0) {
					if (fp_fcio_copyout(fcio, data,
					    mode)) {
						rval = EFAULT;
					}
				} else {
					rval = EFAULT;
				}
			} else {
				rval = EIO;
			}
		} else {
			rval = EIO;
			fp_free_pkt(cmd);
		}

		fctl_release_port_device(pd);
		fctl_dealloc_job(job);
		kmem_free(rls_acc, sizeof (*rls_acc));

		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		break;
	}

	case FCIO_NS:
	{
		fc_ns_cmd_t 	*ns_req;
		fctl_ns_req_t 	*ns_cmd;

		if (fcio->fcio_ilen != sizeof (*ns_req)) {
			rval = EINVAL;
			break;
		}

		ns_req = kmem_zalloc(sizeof (*ns_req), KM_SLEEP);
		ASSERT(ns_req != NULL);

		if (ddi_copyin(fcio->fcio_ibuf, ns_req,
		    sizeof (fc_ns_cmd_t), mode)) {
			rval = EFAULT;
			break;
		}

		if (ns_req->ns_req_len <= 0) {
			rval = EINVAL;
			kmem_free(ns_req, sizeof (*ns_req));
			break;
		}

		job = fctl_alloc_job(JOB_NS_CMD, 0, NULL, NULL, KM_SLEEP);
		ASSERT(job != NULL);

		ns_cmd = fctl_alloc_ns_cmd(ns_req->ns_req_len,
		    ns_req->ns_resp_len, ns_req->ns_resp_len,
		    FCTL_NS_FILL_NS_MAP, KM_SLEEP);
		ASSERT(ns_cmd != NULL);
		ns_cmd->ns_cmd_code = ns_req->ns_cmd;

		if (ns_cmd->ns_cmd_code == NS_GA_NXT) {
			ns_cmd->ns_gan_max = 1;
			ns_cmd->ns_gan_index = 0;
			ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
		}

		if (ddi_copyin(ns_req->ns_req_payload,
		    ns_cmd->ns_cmd_buf, ns_req->ns_req_len, mode)) {
			rval = EFAULT;
			fp_fcio_ns_CLEANUP();
			break;
		}

		job->job_private = (void *)ns_cmd;
		fctl_enque_job(port, job);
		fctl_jobwait(job);
		rval = job->job_result;

		if (rval == FC_SUCCESS) {
			if (ns_req->ns_resp_len) {
				if (ddi_copyout(ns_cmd->ns_data_buf,
				    ns_req->ns_resp_payload,
				    ns_cmd->ns_data_len, mode)) {
					rval = EFAULT;
					fp_fcio_ns_CLEANUP();
					break;
				}
			}
		} else {
			rval = EIO;
		}
		ns_req->ns_resp_hdr = ns_cmd->ns_resp_hdr;
		fp_fcio_ns_CLEANUP();
		if (fp_fcio_copyout(fcio, data, mode)) {
			rval = EFAULT;
		}
		break;
	}

	default:
		rval = ENOTTY;
		break;
	}

	/*
	 * If set, reset the EXCL busy bit to
	 * receive other exclusive access commands
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_flag & FP_EXCL_BUSY) {
		port->fp_flag &= ~FP_EXCL_BUSY;
	}
	mutex_exit(&port->fp_mutex);

	return (rval);
}


/*
 * This function assumes that the response length
 * is same regardless of data model (LP32 or LP64)
 * which is true for all the ioctls currently
 * supported.
 */
static int
fp_copyout(void *from, void *to, size_t len, int mode)
{
	return (ddi_copyout(from, to, len, mode));
}


/*
 * Copy out to userland
 */
static int
fp_fcio_copyout(fcio_t *fcio, intptr_t data, int mode)
{
	int rval;

#ifdef	_MULTI_DATAMODEL
	switch (ddi_model_convert_from(mode & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		struct fcio32 fcio32;

		fcio32.fcio_xfer = fcio->fcio_xfer;
		fcio32.fcio_cmd = fcio->fcio_cmd;
		fcio32.fcio_flags = fcio->fcio_flags;
		fcio32.fcio_cmd_flags = fcio->fcio_cmd_flags;
		fcio32.fcio_ilen = fcio->fcio_ilen;
		fcio32.fcio_ibuf = (caddr32_t)fcio->fcio_ibuf;
		fcio32.fcio_olen = fcio->fcio_olen;
		fcio32.fcio_obuf = (caddr32_t)fcio->fcio_obuf;
		fcio32.fcio_alen = fcio->fcio_alen;
		fcio32.fcio_abuf = (caddr32_t)fcio->fcio_abuf;
		fcio32.fcio_errno = fcio->fcio_errno;

		rval = ddi_copyout((void *)&fcio32, (void *)data,
		    sizeof (struct fcio32), mode);
		break;
	}
	case DDI_MODEL_NONE:
		rval = ddi_copyout((void *)fcio, (void *)data,
		    sizeof (fcio_t), mode);
		break;
	}
#else	/* _MULTI_DATAMODEL */
	rval = ddi_copyout((void *)fcio, (void *)data, sizeof (fcio_t), mode);
#endif	/* _MULTI_DATAMODEL */

	return (rval);
}


/*
 * Handle Point to Point ONLINE
 */
/* ARGSUSED */
static void
fp_pt_pt_online(fc_port_t *port, job_request_t *job)
{
	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(port->fp_statec_busy > 0);
	FC_STATEC_DONE(port);
}


/*
 * Handle Fabric ONLINE
 */
static void
fp_fabric_online(fc_port_t *port, job_request_t *job)
{
	int			index;
	int			rval;
	int			count;
	uint32_t		d_id;
	uint32_t		listlen;
	struct pwwn_hash	*head;
	fc_port_device_t	*pd;
	fc_portmap_t		*changelist;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(FC_IS_TOP_SWITCH(port->fp_topology));
	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	/*
	 * Walk the Port WWN hash table, reestablish LOGIN
	 * if a LOGIN is already performed on a particular
	 * device; Any failure to LOGIN should mark the
	 * port device OLD.
	 */
	for (index = count = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;

		while (pd != NULL) {
			/*
			 * Don't count in the port devices that are new
			 * Sounds like a hack ? May be not, as devices
			 * show up asynchrnously - Rather ancor switch
			 * does a PLOGI to the host while the Fabric Login
			 * is in progress.
			 */
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN &&
			    pd->pd_type != PORT_DEVICE_NEW &&
			    pd->pd_recepient == PD_PLOGI_INITIATOR) {
				count++;
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}

	/*
	 * Check if orphans are showing up now
	 */
	if (port->fp_orphan_count) {
		fctl_ns_req_t	*ns_cmd;
		fc_orphan_t 	*orp;
		fc_orphan_t	*norp = NULL;
		fc_orphan_t	*prev = NULL;

		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
		    sizeof (ns_resp_gid_pn_t), sizeof (ns_resp_gid_pn_t),
		    0, KM_SLEEP);

		ASSERT(ns_cmd != NULL);
		ns_cmd->ns_cmd_code = NS_GID_PN;

		for (orp = port->fp_orphan_list; orp; orp = norp) {
			norp = orp->orp_next;
			mutex_exit(&port->fp_mutex);
			orp->orp_nscan++;

			FCTL_SET_JOB_COUNTER(job, 1);
			job->job_result = FC_SUCCESS;

			FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_buf, &orp->orp_pwwn);
			((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->NS_PID = 0;
			((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->pid.rsvd = 0;

			rval = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);
			if (rval == FC_SUCCESS) {
				d_id = ((ns_resp_gid_pn_t *)
				    ns_cmd->ns_data_buf)->NS_PID;
				pd = fp_create_port_device_by_ns(port,
				    d_id, KM_SLEEP);
				if (pd != NULL) {
					mutex_enter(&port->fp_mutex);
					if (prev) {
						prev->orp_next = orp->orp_next;
					} else {
						ASSERT(orp ==
						    port->fp_orphan_list);
						port->fp_orphan_list =
						    orp->orp_next;
					}
					port->fp_orphan_count--;
					mutex_exit(&port->fp_mutex);
					kmem_free(orp, sizeof (*orp));
					count++;
				} else {
					prev = orp;
				}
			} else {
				if (orp->orp_nscan == FC_ORPHAN_SCAN_LIMIT) {
					char ww_name[17];

					fctl_wwn_to_str(&orp->orp_pwwn,
					    ww_name);

					fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0,
					    NULL,
					    " Port WWN %s removed from orphan"
					    " list after %d scans", ww_name,
					    orp->orp_nscan);

					mutex_enter(&port->fp_mutex);
					if (prev) {
						prev->orp_next = orp->orp_next;
					} else {
						ASSERT(orp ==
						    port->fp_orphan_list);
						port->fp_orphan_list =
						    orp->orp_next;
					}
					port->fp_orphan_count--;
					mutex_exit(&port->fp_mutex);

					kmem_free(orp, sizeof (*orp));
				} else {
					prev = orp;
				}
			}
			mutex_enter(&port->fp_mutex);
		}

		if (ns_cmd) {
			fctl_free_ns_cmd(ns_cmd);
		}
	}

	listlen = 0;
	changelist = NULL;
	if (count) {
		if (port->fp_soft_state & FP_SOFT_IN_FCA_RESET) {
			port->fp_soft_state &= ~FP_SOFT_IN_FCA_RESET;
			mutex_exit(&port->fp_mutex);
			delay(drv_usectohz(FLA_RR_TOV * 1000 * 1000));
			mutex_enter(&port->fp_mutex);
		}

		FCTL_SET_JOB_COUNTER(job, count);
		for (index = 0; index < pwwn_table_size; index++) {
			head = &port->fp_pwwn_table[index];
			pd = head->pwwn_head;

			while (pd != NULL) {
				mutex_enter(&pd->pd_mutex);
				if (pd->pd_state == PORT_DEVICE_LOGGED_IN &&
				    pd->pd_type != PORT_DEVICE_NEW &&
				    pd->pd_recepient == PD_PLOGI_INITIATOR) {
					d_id = pd->PD_PORT_ID;
					/*
					 * Explicitly mark all devices OLD;
					 * successful PLOGI should reset this
					 * to either NO_CHANGE or CHANGED.
					 */
					pd->pd_type = PORT_DEVICE_OLD;
					mutex_exit(&pd->pd_mutex);
					mutex_exit(&port->fp_mutex);
					rval = fp_port_login(port, d_id,
					    job, FP_CMD_PLOGI_RETAIN,
					    KM_SLEEP, NULL, NULL);

					if (rval != FC_SUCCESS) {
						fp_jobdone(job);
					}
					mutex_enter(&port->fp_mutex);
				} else {
					mutex_exit(&pd->pd_mutex);
					mutex_exit(&port->fp_mutex);
					rval = fp_ns_validate_device(port, pd,
					    job, 0, KM_SLEEP);
					if (rval != FC_SUCCESS) {
						fp_jobdone(job);
					}
					mutex_enter(&port->fp_mutex);
				}
				pd = pd->pd_wwn_hnext;
			}
		}
		mutex_exit(&port->fp_mutex);
		fp_jobwait(job);
		mutex_enter(&port->fp_mutex);

		ASSERT(port->fp_statec_busy > 0);
		if ((job->job_flags & JOB_CANCEL_ULP_NOTIFICATION) == 0) {
			if (port->fp_statec_busy > 1) {
				job->job_flags |= JOB_CANCEL_ULP_NOTIFICATION;
			} else {
				mutex_exit(&port->fp_mutex);
				fctl_fillout_map(port, &changelist,
				    &listlen, 1);
				mutex_enter(&port->fp_mutex);
			}
		}
		mutex_exit(&port->fp_mutex);
	} else {
		ASSERT(port->fp_statec_busy > 0);
		if (port->fp_statec_busy > 1) {
			job->job_flags |= JOB_CANCEL_ULP_NOTIFICATION;
		}
		mutex_exit(&port->fp_mutex);
	}

	if ((job->job_flags & JOB_CANCEL_ULP_NOTIFICATION) == 0) {
		(void) fp_ulp_statec_cb(port, FC_STATE_ONLINE, changelist,
		    listlen, listlen, KM_SLEEP);
		mutex_enter(&port->fp_mutex);
	} else {
		ASSERT(changelist == NULL && listlen == 0);
		mutex_enter(&port->fp_mutex);
		FC_STATEC_DONE(port);
	}
}


/*
 * Fill out device list for userland ioctl in private loop
 */
static int
fp_fillout_loopmap(fc_port_t *port, fcio_t *fcio, int mode)
{
	int			rval;
	int			count;
	int			index;
	int			num_devices;
	fc_device_t		*node;
	fc_port_dev_t 		*devlist;
	fc_port_device_t 	*pd;
	struct pwwn_hash 	*head;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	num_devices = fcio->fcio_olen / sizeof (fc_port_dev_t);
	if (port->fp_total_devices > port->fp_dev_count &&
	    num_devices >= port->fp_total_devices) {
		job_request_t 	*job;

		mutex_exit(&port->fp_mutex);
		job = fctl_alloc_job(JOB_PORT_GETMAP, 0, NULL, NULL, KM_SLEEP);
		FCTL_SET_JOB_COUNTER(job, 1);

		mutex_enter(&port->fp_mutex);
		fp_get_loopmap(port, job);
		mutex_exit(&port->fp_mutex);

		fp_jobwait(job);
		fctl_dealloc_job(job);
	} else {
		mutex_exit(&port->fp_mutex);
	}
	devlist = kmem_zalloc(sizeof (*devlist) * num_devices, KM_SLEEP);

	mutex_enter(&port->fp_mutex);
	for (count = index = 0; index < pwwn_table_size &&
	    count < num_devices; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;

		while (pd != NULL && count < num_devices) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_INVALID) {
				mutex_exit(&pd->pd_mutex);
				pd = pd->pd_wwn_hnext;
				continue;
			}

			devlist[count].dev_state = pd->pd_state;
			devlist[count].dev_hard_addr = pd->pd_hard_addr;
			devlist[count].dev_did = pd->pd_port_id;
			bcopy((caddr_t)pd->pd_fc4types,
			    (caddr_t)devlist[count].dev_type,
			    sizeof (pd->pd_fc4types));

			bcopy((caddr_t)&pd->pd_port_name,
			    (caddr_t)&devlist[count].dev_pwwn,
			    sizeof (la_wwn_t));

			node = pd->pd_device;
			mutex_exit(&pd->pd_mutex);

			if (node) {
				mutex_enter(&node->fd_mutex);
				bcopy((caddr_t)&node->fd_node_name,
				    (caddr_t)&devlist[count].dev_nwwn,
				    sizeof (la_wwn_t));
				mutex_exit(&node->fd_mutex);
			}
			pd = pd->pd_wwn_hnext;
			count++;
		}
	}

	if (fp_copyout((void *)devlist, (void *)fcio->fcio_obuf,
	    sizeof (fc_port_dev_t) * num_devices, mode)) {
		rval = FC_FAILURE;
	} else {
		rval = FC_SUCCESS;
	}
	kmem_free(devlist, sizeof (*devlist) * num_devices);

	return (rval);
}


/*
 * Completion function for responses to unsolicited commands
 */
static void
fp_unsol_intr(fc_packet_t *pkt)
{
	fp_cmd_t 	*cmd;
	fc_port_t 	*port;

	cmd = FP_PKT_TO_CMD(pkt);
	port = FP_CMD_TO_PORT(cmd);

	if (cmd == port->fp_els_resp_pkt) {
		FP_UNLOCK_EXPEDITED_ELS_PKT(port);
		return;
	}

	fp_free_pkt(cmd);
}


/*
 * solicited LINIT ELS completion function
 */
static void
fp_linit_intr(fc_packet_t *pkt)
{
	fp_cmd_t		*cmd;
	job_request_t		*job;
	fc_linit_resp_t		acc;

	if (FP_IS_PKT_ERROR(pkt)) {
		(void) fp_common_intr(pkt, 1);
		return;
	}

	cmd = FP_PKT_TO_CMD(pkt);
	job = cmd->cmd_job;

	FP_CP_IN(pkt->pkt_resp_acc, pkt->pkt_resp + sizeof (fc_ct_header_t),
	    &acc, sizeof (acc));

	if (acc.status != FC_LINIT_SUCCESS) {
		job->job_result = FC_FAILURE;
	} else {
		job->job_result = FC_SUCCESS;
	}
	fp_iodone(cmd);
}


/*
 * Decode the unsolicited request; For FC-4 Device and Link data frames
 * notify the registered ULP of this FC-4 type right here. For Unsolicited
 * ELS requests, submit a request to the job_handler thread to work on it.
 * The intent is to act quickly on the FC-4 unsolicited link and data frames
 * and save much of the interrupt time processing of unsolicited ELS requests
 * and hand it off to the job_handler thread.
 */
static void
fp_unsol_cb(opaque_t port_handle, fc_unsol_buf_t *buf, uint32_t type)
{
	uchar_t			r_ctl;
	uchar_t			ls_code;
	uint32_t		s_id;
	fp_cmd_t		*cmd;
	fc_port_t 		*port;
	job_request_t		*job;
	fc_port_device_t 	*pd;

	port = (fc_port_t *)port_handle;

	FP_TNF_PROBE_5((fp_unsol_cb, "fp_unsol_trace", "'UNSOL CB.1'",
	    tnf_int,	Dr_Instance,	port->fp_instance,
	    tnf_int,	S_ID,		buf->ub_frame.s_id,
	    tnf_int,	D_ID,		buf->ub_frame.d_id,
	    tnf_int,	Type,		buf->ub_frame.type,
	    tnf_int,	F_CTL,		buf->ub_frame.f_ctl));

	FP_TNF_PROBE_5((fp_unsol_cb, "fp_unsol_trace", "'UNSOL CB.2'",
	    tnf_int,	Seq_ID,		buf->ub_frame.seq_id,
	    tnf_int,	df_ctl,		buf->ub_frame.df_ctl,
	    tnf_int,	Seq_cnt,	buf->ub_frame.seq_cnt,
	    tnf_int,	ox_ID,		buf->ub_frame.ox_id,
	    tnf_int,	rx_ID,		buf->ub_frame.rx_id));

	FP_TNF_PROBE_1((fp_unsol_cb, "fp_unsol_trace", "'UNSOL CB.3'",
	    tnf_int,	RO,		buf->ub_frame.ro));

	if (type & 0x80000000) {
		/*
		 * Huh ? Nothing much can be done without
		 * a valid buffer. So just exit.
		 */
		return;
	}
	/*
	 * If the unsolicited interrupts arrive while it isn't
	 * safe to handle unsolicited callbacks; Drop them, yes,
	 * drop them on the floor
	 */
	mutex_enter(&port->fp_mutex);
	port->fp_active_ubs++;
	if (FP_MUST_DROP_CALLBACKS(port)) {
		mutex_exit(&port->fp_mutex);
		FC_RELEASE_AN_UB(port, buf);
		return;
	}

	r_ctl = buf->ub_frame.r_ctl;
	s_id = buf->ub_frame.s_id;
	if (port->fp_active_ubs == 1) {
		port->fp_soft_state |= FP_SOFT_IN_UNSOL_CB;
	}

	if (r_ctl == R_CTL_ELS_REQ && buf->ub_buffer[0] == LA_ELS_LOGO &&
	    port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		FC_RELEASE_AN_UB(port, buf);
		return;
	}

	if (port->fp_els_resp_pkt_busy == 0) {
		if (r_ctl == R_CTL_ELS_REQ) {
			ls_code = buf->ub_buffer[0];

			switch (ls_code) {
			case LA_ELS_PLOGI:
			case LA_ELS_FLOGI:
				FP_LOCK_EXPEDITED_ELS_PKT(port);
				mutex_exit(&port->fp_mutex);
				fp_i_handle_unsol_els(port, buf);
				FC_RELEASE_AN_UB(port, buf);
				return;

			default:
				break;
			}
		}
	}
	mutex_exit(&port->fp_mutex);

	switch (r_ctl & R_CTL_ROUTING) {
	case R_CTL_DEVICE_DATA:
		/*
		 * If the unsolicited buffer is a CT IU,
		 * have the job_handler thread work on it.
		 */
		if (buf->ub_frame.type == FC_TYPE_FC_SERVICES) {
			break;
		}
		/* FALLTHROUGH */

	case R_CTL_FC4_SVC:
		/*
		 * If a LOGIN isn't performed before this request
		 * shut the door on this port with a reply that a
		 * LOGIN is required
		 */
		pd = fctl_get_port_device_by_did(port, s_id);
		if (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_state == PORT_DEVICE_LOGGED_IN) {
				mutex_exit(&pd->pd_mutex);
				fctl_ulp_unsol_cb(port, buf, type);
				return;
			}
			mutex_exit(&pd->pd_mutex);
		}

		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
			    0, KM_NOSLEEP);
			if (cmd != NULL) {
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_LOGIN_REQUIRED, NULL);

				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) != FC_SUCCESS) {
					fp_free_pkt(cmd);
				}
			}
		}
		FC_RELEASE_AN_UB(port, buf);
		return;

	default:
		break;
	}

	/*
	 * Submit a Request to the job_handler thread to work
	 * on the unsolicited request. The potential side effect
	 * of this is that the unsolicited buffer takes a little
	 * longer to get released but we save interrupt time in
	 * the bargain.
	 */
	job = fctl_alloc_job(JOB_UNSOL_REQUEST, 0, NULL, NULL, KM_NOSLEEP);
	if (job == NULL) {
		fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL, "fp_unsol_cb() "
		    "couldn't submit a job to the thread, failing..");

		FC_RELEASE_AN_UB(port, buf);
		return;
	}
	job->job_private = (void *)buf;
	fctl_enque_job(port, job);
}


/*
 * Handle unsolicited requests
 */
static void
fp_handle_unsol_buf(fc_port_t *port, fc_unsol_buf_t *buf, job_request_t *job)
{
	uchar_t			r_ctl;
	uchar_t			ls_code;
	uint32_t		s_id;
	fp_cmd_t		*cmd;
	fc_port_device_t 	*pd;

	r_ctl = buf->ub_frame.r_ctl;
	s_id = buf->ub_frame.s_id;

	switch (r_ctl & R_CTL_ROUTING) {
	case R_CTL_EXTENDED_SVC:
		if (r_ctl == R_CTL_ELS_REQ) {
			ls_code = buf->ub_buffer[0];
			switch (ls_code) {
			case LA_ELS_LOGO:
			case LA_ELS_ADISC:
				pd = fctl_get_port_device_by_did(port, s_id);
				if (pd == NULL) {
					fp_handle_unsol_buf_INVALID_REQUEST(
					    port, buf, job);
				} else {
					if (ls_code == LA_ELS_LOGO) {
						fp_handle_unsol_logo(port, buf,
						    pd, job);
					} else {
						fp_handle_unsol_adisc(port, buf,
						    pd, job);
					}
				}
				break;

			case LA_ELS_PLOGI:
				fp_handle_unsol_plogi(port, buf, job, KM_SLEEP);
				break;

			case LA_ELS_FLOGI:
				fp_handle_unsol_flogi(port, buf, job, KM_SLEEP);
				break;

			case LA_ELS_RSCN:
				fp_handle_unsol_rscn(port, buf, job, KM_SLEEP);
				break;

			default:
				fctl_ulp_unsol_cb(port, buf,
				    buf->ub_frame.type);
				return;
			}
		}
		break;

	case R_CTL_BASIC_SVC:
		/*
		 * The unsolicited basic link services could be ABTS
		 * and RMC (Or even a NOP). Just BA_RJT them until
		 * such time there arises a need to handle them more
		 * carefully.
		 */
		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_ba_rjt_t),
			    0, KM_SLEEP);
			if (cmd != NULL) {
				fp_ba_rjt_init(port, cmd, buf, job);
				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) != FC_SUCCESS) {
					fp_free_pkt(cmd);
				}
			}
		}
		break;

	case R_CTL_LINK_CTL:
		/*
		 * Turn deaf ear on unsolicited link control frames.
		 * Typical unsolicited link control Frame is an LCR
		 * (to reset End to End credit to the default login
		 * value and abort current sequences for all classes)
		 * An intelligent microcode/firmware should handle
		 * this transparently at its level and not pass all
		 * the way up here.
		 *
		 * Possible responses to LCR are R_RDY, F_RJT, P_RJT
		 * or F_BSY. P_RJT is chosen to be the most appropriate
		 * at this time.
		 */
		/* FALLTHROUGH */

	case R_CTL_DEVICE_DATA:
		/*
		 * Mostly this is of type FC_TYPE_FC_SERVICES.
		 * As we don't like any Unsolicited FC services
		 * requests, we would do well to RJT them as
		 * well.
		 */
		/* FALLTHROUGH */

	default:
		/*
		 * Just reject everything else as an invalid request.
		 */
		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
			    0, KM_SLEEP);
			if (cmd != NULL) {
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_INVALID_LINK_CTRL, job);

				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) != FC_SUCCESS) {
					fp_free_pkt(cmd);
				}
			}
		}
		break;
	}
	FC_RELEASE_AN_UB(port, buf);
}


/*
 * Prepare a BA_RJT and send it over.
 */
static void
fp_ba_rjt_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job)
{
	fc_packet_t	*pkt;
	la_ba_rjt_t 	payload;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	FP_INIT_CMD(cmd, buf->ub_class, FC_PKT_OUTBOUND,
	    FP_CMD_CFLAG_UNDEFINED, 1, NULL);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	cmd->cmd_job = job;

	pkt = FP_CMD_TO_PKT(cmd);

	fp_unsol_resp_init(pkt, buf, R_CTL_LS_BA_RJT, FC_TYPE_BASIC_LS);

	payload.reserved = 0;
	payload.reason_code = FC_REASON_CMD_UNSUPPORTED;
	payload.explanation = FC_EXPLN_NONE;
	payload.vendor = 0;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Prepare an LS_RJT and send it over
 */
static void
fp_els_rjt_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    uchar_t action, uchar_t reason, job_request_t *job)
{
	fc_packet_t	*pkt;
	la_els_rjt_t 	payload;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	FP_INIT_CMD(cmd, buf->ub_class, FC_PKT_OUTBOUND,
	    FP_CMD_CFLAG_UNDEFINED, 1, NULL);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	cmd->cmd_job = job;

	pkt = FP_CMD_TO_PKT(cmd);

	fp_unsol_resp_init(pkt, buf, R_CTL_ELS_RSP, FC_TYPE_EXTENDED_LS);

	payload.LS_CODE = LA_ELS_RJT;
	payload.MBZ = 0;
	payload.action = action;
	payload.reason = reason;
	payload.reserved = 0;
	payload.vu = 0;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Prepare an ACC response to an ELS request
 */
static void
fp_els_acc_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job)
{
	fc_packet_t	*pkt;
	ls_code_t	payload;

	FP_INIT_CMD(cmd, buf->ub_class, FC_PKT_OUTBOUND,
	    FP_CMD_CFLAG_UNDEFINED, 1, NULL);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	cmd->cmd_job = job;

	pkt = FP_CMD_TO_PKT(cmd);

	fp_unsol_resp_init(pkt, buf, R_CTL_ELS_RSP, FC_TYPE_EXTENDED_LS);

	payload.ls_code = LA_ELS_ACC;
	payload.mbz = 0;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Unsolicited LOGO handler
 */
static void
fp_handle_unsol_logo(fc_port_t *port, fc_unsol_buf_t *buf,
    fc_port_device_t *pd, job_request_t *job)
{
	int		busy;
	int		rval;
	int		retain;
	fp_cmd_t	*cmd;
	fc_portmap_t 	*listptr;

	mutex_enter(&port->fp_mutex);
	busy = port->fp_statec_busy;
	mutex_exit(&port->fp_mutex);

	mutex_enter(&pd->pd_mutex);
	pd->pd_tolerance++;
	if (!busy) {
		if (pd->pd_state != PORT_DEVICE_LOGGED_IN ||
		    pd->pd_state == PORT_DEVICE_INVALID ||
		    pd->pd_flags == PD_ELS_IN_PROGRESS ||
		    pd->pd_type == PORT_DEVICE_OLD || pd->pd_held) {
			busy++;
		}
	}

	if (busy) {
		mutex_exit(&pd->pd_mutex);
		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
			    0, KM_SLEEP);
			if (cmd != NULL) {
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_INVALID_LINK_CTRL, job);

				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) != FC_SUCCESS) {
					fp_free_pkt(cmd);
				}
			}
		}
	} else {
		retain = (pd->pd_recepient == PD_PLOGI_INITIATOR) ? 1 : 0;
		if (pd->pd_tolerance >= FC_LOGO_TOLERANCE_LIMIT) {
			retain = 0;
			pd->pd_state = PORT_DEVICE_INVALID;
		}
		mutex_exit(&pd->pd_mutex);

		cmd = fp_alloc_pkt(port, FP_PORT_IDENTIFIER_LEN, 0, KM_SLEEP);
		if (cmd == NULL) {
			return;
		}

		fp_els_acc_init(port, cmd, buf, job);

		rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
		if (rval != FC_SUCCESS) {
			fp_free_pkt(cmd);
			return;
		}

		listptr = kmem_zalloc(sizeof (fc_portmap_t), KM_SLEEP);

		if (retain) {
			fp_unregister_login(pd);
			fctl_copy_portmap(listptr, pd);
		} else {
			fp_fillout_old_map(listptr, pd);
			listptr->map_flags = PORT_DEVICE_OLD;
		}

		(void) fp_ulp_devc_cb(port, listptr, 1, 1, KM_SLEEP);
	}
}


/*
 * Perform general purpose preparation of a response to an unsolicited request
 */
static void
fp_unsol_resp_init(fc_packet_t *pkt, fc_unsol_buf_t *buf,
    uchar_t r_ctl, uchar_t type)
{
	pkt->pkt_cmd_fhdr.r_ctl = r_ctl;
	pkt->pkt_cmd_fhdr.d_id = buf->ub_frame.s_id;
	pkt->pkt_cmd_fhdr.s_id = buf->ub_frame.d_id;
	pkt->pkt_cmd_fhdr.type = type;
	pkt->pkt_cmd_fhdr.f_ctl = F_CTL_LAST_SEQ | F_CTL_XCHG_CONTEXT;
	pkt->pkt_cmd_fhdr.seq_id = buf->ub_frame.seq_id;
	pkt->pkt_cmd_fhdr.df_ctl  = buf->ub_frame.df_ctl;
	pkt->pkt_cmd_fhdr.seq_cnt = buf->ub_frame.seq_cnt;
	pkt->pkt_cmd_fhdr.ox_id = buf->ub_frame.ox_id;
	pkt->pkt_cmd_fhdr.rx_id = buf->ub_frame.rx_id;
	pkt->pkt_cmd_fhdr.ro = 0;
	pkt->pkt_cmd_fhdr.rsvd = 0;
	pkt->pkt_comp = fp_unsol_intr;
	pkt->pkt_timeout = FP_ELS_TIMEOUT;
	pkt->pkt_pd = NULL;
}


/*
 * Immediate handling of unsolicited FLOGI and PLOGI requests. In the
 * early development days of public loop soc+ firmware, numerous problems
 * were encountered (the details are undocumented and history now) which
 * led to the birth of this function.
 *
 * If a pre-allocated unsolicited response packet is free, send out an
 * immediate response, otherwise submit the request to the port thread
 * to do the deferred processing.
 */
static void
fp_i_handle_unsol_els(fc_port_t *port, fc_unsol_buf_t *buf)
{
	int			sent;
	int 			f_port;
	int			do_acc;
	fp_cmd_t 		*cmd;
	la_els_logi_t 		*payload;
	fc_port_device_t	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	cmd = port->fp_els_resp_pkt;

	mutex_enter(&port->fp_mutex);
	do_acc = (port->fp_statec_busy == 0) ? 1 : 0;
	mutex_exit(&port->fp_mutex);

	switch (buf->ub_buffer[0]) {
	case LA_ELS_PLOGI:
	{
		int small;

		payload = (la_els_logi_t *)buf->ub_buffer;

		f_port = FP_IS_F_PORT(payload->
		    common_service.cmn_features) ? 1 : 0;

		small = fctl_wwn_cmp(&port->fp_service_params.nport_ww_name,
		    &payload->nport_ww_name);
		pd = fctl_get_port_device_by_pwwn(port,
		    &payload->nport_ww_name);
		if (pd) {
			mutex_enter(&pd->pd_mutex);
			sent = (pd->pd_flags == PD_ELS_IN_PROGRESS) ? 1 : 0;
			/*
			 * Most likely this means a cross login is in
			 * progress or a device about to be yanked out.
			 * Don't accept until the port device disappears
			 * from the PWWN hash list.
			 */
			if (pd->pd_type == PORT_DEVICE_OLD) {
				sent = 1;
			}
			mutex_exit(&pd->pd_mutex);
		} else {
			sent = 0;
		}

		/*
		 * To avoid Login collisions, accept only if my WWN
		 * is smaller than the requestor (A curious side note
		 * would be that this rule may not satisfy the PLOGIs
		 * initiated by the switch from not-so-well known
		 * ports such as 0xFFFC41)
		 */
		if (((f_port == 0 && small < 0) || (small > 0 && do_acc) ||
		    FC_MUST_ACCEPT_D_ID(buf->ub_frame.s_id)) && sent == 0) {
			if (fp_is_class_supported(port->fp_cos,
			    buf->ub_class) == FC_FAILURE) {
				if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
					cmd->cmd_pkt.pkt_cmdlen =
					    sizeof (la_els_rjt_t);
					cmd->cmd_pkt.pkt_rsplen = 0;
					fp_els_rjt_init(port, cmd, buf,
					    FC_ACTION_NON_RETRYABLE,
					    FC_REASON_CLASS_NOT_SUPP, NULL);
				} else {
					FP_UNLOCK_EXPEDITED_ELS_PKT(port);
					return;
				}
			} else {
				cmd->cmd_pkt.pkt_cmdlen =
				    sizeof (la_els_logi_t);
				cmd->cmd_pkt.pkt_rsplen = 0;
				/*
				 * Sometime later, we should validate
				 * the service parameters instead of
				 * just accepting it.
				 */
				fp_login_acc_init(port, cmd, buf,
				    NULL, KM_NOSLEEP);
				if (small > 0 && do_acc) {
					pd = fctl_get_port_device_by_pwwn(
					    port, &payload->nport_ww_name);
					if (pd) {
						mutex_enter(&pd->pd_mutex);
						pd->pd_recepient =
						    PD_PLOGI_INITIATOR;
						mutex_exit(&pd->pd_mutex);
					}
				}
			}
		} else {
			if (FP_IS_CLASS_1_OR_2(buf->ub_class) ||
			    port->fp_options & FP_SEND_RJT) {
				cmd->cmd_pkt.pkt_cmdlen = sizeof (la_els_rjt_t);
				cmd->cmd_pkt.pkt_rsplen = 0;
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_LOGICAL_BSY, NULL);
			} else {
				FP_UNLOCK_EXPEDITED_ELS_PKT(port);
				return;
			}
		}
		break;
	}

	case LA_ELS_FLOGI:
		if (fp_is_class_supported(port->fp_cos,
		    buf->ub_class) == FC_FAILURE) {
			if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
				cmd->cmd_pkt.pkt_cmdlen = sizeof (la_els_rjt_t);
				cmd->cmd_pkt.pkt_rsplen = 0;
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_CLASS_NOT_SUPP, NULL);
			} else {
				FP_UNLOCK_EXPEDITED_ELS_PKT(port);
				return;
			}
		} else {
			mutex_enter(&port->fp_mutex);
			if (FC_PORT_STATE_MASK(port->fp_state) !=
			    FC_STATE_ONLINE || (port->FP_PORT_ID &&
			    buf->ub_frame.s_id == port->FP_PORT_ID)) {
				mutex_exit(&port->fp_mutex);
				if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
					cmd->cmd_pkt.pkt_cmdlen =
					    sizeof (la_els_rjt_t);
					cmd->cmd_pkt.pkt_rsplen = 0;
					fp_els_rjt_init(port, cmd, buf,
					    FC_ACTION_NON_RETRYABLE,
					    FC_REASON_INVALID_LINK_CTRL,
					    NULL);
				} else {
					FP_UNLOCK_EXPEDITED_ELS_PKT(port);
					return;
				}
			} else {
				mutex_exit(&port->fp_mutex);
				cmd->cmd_pkt.pkt_cmdlen =
				    sizeof (la_els_logi_t);
				cmd->cmd_pkt.pkt_rsplen = 0;
				/*
				 * Let's not aggresively validate the N_Port's
				 * service parameters until PLOGI. Suffice it
				 * to give a hint that we are an N_Port and we
				 * are game to some serious stuff here.
				 */
				fp_login_acc_init(port, cmd, buf,
				    NULL, KM_NOSLEEP);
			}
		}
		break;

	default:
		return;
	}

	if ((fp_sendcmd(port, cmd, port->fp_fca_handle)) != FC_SUCCESS) {
		FP_UNLOCK_EXPEDITED_ELS_PKT(port);
	}
}


/*
 * Handle unsolicited PLOGI request
 */
static void
fp_handle_unsol_plogi(fc_port_t *port, fc_unsol_buf_t *buf,
    job_request_t *job, int sleep)
{
	int			sent;
	int			small;
	int 			f_port;
	int			do_acc;
	fp_cmd_t		*cmd;
	la_wwn_t 		*swwn;
	la_wwn_t		*dwwn;
	la_els_logi_t 		*payload;
	fc_port_device_t	*pd;

	payload = (la_els_logi_t *)buf->ub_buffer;
	f_port = FP_IS_F_PORT(payload->common_service.cmn_features) ? 1 : 0;

	mutex_enter(&port->fp_mutex);
	do_acc = (port->fp_statec_busy == 0) ? 1 : 0;
	mutex_exit(&port->fp_mutex);

	swwn = &port->fp_service_params.nport_ww_name;
	dwwn = &payload->nport_ww_name;
	pd = fctl_get_port_device_by_pwwn(port, dwwn);
	if (pd) {
		mutex_enter(&pd->pd_mutex);
		sent = (pd->pd_flags == PD_ELS_IN_PROGRESS) ? 1 : 0;
		/*
		 * Most likely this means a cross login is in
		 * progress or a device about to be yanked out.
		 * Don't accept until the port device disappears
		 * from the PWWN hash list.
		 */
		if (pd->pd_type == PORT_DEVICE_OLD) {
			sent = 1;
		}
		mutex_exit(&pd->pd_mutex);
	} else {
		sent = 0;
	}

	/*
	 * Avoid Login collisions by accepting only if my WWN is smaller.
	 *
	 * A side note: There is no need to start a PLOGI from this end in
	 *	this context if login isn't going to be accepted for the
	 * 	above reason as either a LIP (in private loop), RSCN (in
	 *	fabric topology), or an FLOGI (in point to point - Huh ?
	 *	check FC-PH) would normally drive the PLOGI from this end.
	 *	At this point of time there is no need for an inbound PLOGI
	 *	to kick an outbound PLOGI when it is going to be rejected
	 *	for the reason of WWN being smaller. However it isn't hard
	 *	to do that either (when such a need arises, start a timer
	 *	for a duration that extends beyond a normal device discovery
	 *	time and check if an outbound PLOGI did go before that, if
	 *	none fire one)
	 *
	 *	Unfortunately, as it turned out, during booting, it is possible
	 *	to miss another initiator in the same loop as port driver
	 * 	instances are serially attached. While preserving the above
	 * 	comments for belly laughs, please kick an outbound PLOGI in
	 *	a non-switch environment (which is a pt pt betwen N_Ports or
	 *	a private loop)
	 *
	 *	While preserving the above comments for amusement, send an
	 *	ACC if the PLOGI is going to be rejected for WWN being smaller
	 *	when no discovery is in progress at this end. Turn around
	 *	and make the port device as the PLOGI initiator, so that
	 *	during subsequent link/loop initialization, this end drives
	 *	the PLOGI (Infact both ends do in this particular case, but
	 *	only one wins)
	 *
	 * Make sure the PLOGIs initiated by the switch from not-so-well-known
	 * ports (such as 0xFFFC41) are accepted too.
	 */
	small = fctl_wwn_cmp(swwn, dwwn);
	if (((f_port == 0 && small < 0) || (small > 0 && do_acc) ||
	    FC_MUST_ACCEPT_D_ID(buf->ub_frame.s_id)) && sent == 0) {
		if (fp_is_class_supported(port->fp_cos,
		    buf->ub_class) == FC_FAILURE) {
			if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
				cmd = fp_alloc_pkt(port,
				    sizeof (la_els_logi_t), 0, sleep);
				if (cmd == NULL) {
					return;
				}
				cmd->cmd_pkt.pkt_cmdlen = sizeof (la_els_rjt_t);
				cmd->cmd_pkt.pkt_rsplen = 0;
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_CLASS_NOT_SUPP, job);
			}
		} else {
			cmd = fp_alloc_pkt(port, sizeof (la_els_logi_t),
			    0, sleep);
			if (cmd == NULL) {
				return;
			}
			cmd->cmd_pkt.pkt_cmdlen = sizeof (la_els_logi_t);
			cmd->cmd_pkt.pkt_rsplen = 0;
			/*
			 * Sometime later, we should validate the service
			 * parameters instead of just accepting it.
			 */
			fp_login_acc_init(port, cmd, buf, job, KM_SLEEP);
			if (small > 0 && do_acc) {
				pd = fctl_get_port_device_by_pwwn(port, dwwn);
				if (pd) {
					mutex_enter(&pd->pd_mutex);
					pd->pd_recepient = PD_PLOGI_INITIATOR;
					mutex_exit(&pd->pd_mutex);
				}
			}
		}
	} else {
		if (FP_IS_CLASS_1_OR_2(buf->ub_class) ||
		    port->fp_options & FP_SEND_RJT) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_logi_t),
			    0, sleep);
			if (cmd == NULL) {
				return;
			}
			cmd->cmd_pkt.pkt_cmdlen = sizeof (la_els_rjt_t);
			cmd->cmd_pkt.pkt_rsplen = 0;
			/*
			 * Send out Logical busy to indicate
			 * the detection of PLOGI collision
			 */
			fp_els_rjt_init(port, cmd, buf,
			    FC_ACTION_NON_RETRYABLE,
			    FC_REASON_LOGICAL_BSY, job);
		} else {
			return;
		}
	}

	if (fp_sendcmd(port, cmd, port->fp_fca_handle) != FC_SUCCESS) {
		fp_free_pkt(cmd);
	}
}


/*
 * Handle mischeivous turning over of our own FLOGI requests back to
 * us by the SOC+ microcode. In otherwords, look at the class of such
 * bone headed requests, if 1 or 2, bluntly P_RJT them, if 3 drop them
 * on the floor
 */
static void
fp_handle_unsol_flogi(fc_port_t *port, fc_unsol_buf_t *buf,
	job_request_t *job, int sleep)
{
	uint32_t	state;
	uint32_t	s_id;
	fp_cmd_t 	*cmd;

	if (fp_is_class_supported(port->fp_cos, buf->ub_class) == FC_FAILURE) {
		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
			    0, sleep);
			if (cmd == NULL) {
				return;
			}
			fp_els_rjt_init(port, cmd, buf,
			    FC_ACTION_NON_RETRYABLE,
			    FC_REASON_CLASS_NOT_SUPP, job);
		} else {
			return;
		}
	} else {
		mutex_enter(&port->fp_mutex);
		state = FC_PORT_STATE_MASK(port->fp_state);
		s_id = port->FP_PORT_ID;
		mutex_exit(&port->fp_mutex);

		if (state != FC_STATE_ONLINE ||
		    (s_id && buf->ub_frame.s_id == s_id)) {
			if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
				cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
				    0, sleep);
				if (cmd == NULL) {
					return;
				}
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_INVALID_LINK_CTRL, job);
			} else {
				return;
			}
		} else {
			cmd = fp_alloc_pkt(port, sizeof (la_els_logi_t),
			    0, sleep);
			if (cmd == NULL) {
				return;
			}
			/*
			 * Let's not aggresively validate the N_Port's
			 * service parameters until PLOGI. Suffice it
			 * to give a hint that we are an N_Port and we
			 * are game to some serious stuff here.
			 */
			fp_login_acc_init(port, cmd, buf, job, KM_SLEEP);
		}
	}

	if (fp_sendcmd(port, cmd, port->fp_fca_handle) != FC_SUCCESS) {
		fp_free_pkt(cmd);
	}
}


/*
 * Perform PLOGI accept
 */
static void
fp_login_acc_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job, int sleep)
{
	fc_packet_t	*pkt;
	fc_portmap_t 	*listptr;
	la_els_logi_t	payload;

	/*
	 * If we are sending ACC to PLOGI and we haven't already
	 * create port and node device handles, let's create them
	 * here.
	 */
	if (buf->ub_buffer[0] == LA_ELS_PLOGI) {
		fc_port_device_t	*pd;
		la_els_logi_t 		*req;

		req = (la_els_logi_t *)buf->ub_buffer;
		pd = fctl_create_port_device(port, &req->node_ww_name,
		    &req->nport_ww_name, buf->ub_frame.s_id,
		    PD_PLOGI_RECEPIENT, sleep);
		if (pd == NULL) {
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    "couldn't create port device d_id=%x",
			    buf->ub_frame.s_id);
		} else {
			/*
			 * usoc currently returns PLOGIs inline and
			 * the maximum buffer size is 60 bytes or so.
			 * So attempt not to look beyond what is in
			 * the unsolicited buffer
			 */
			if (buf->ub_bufsize >= sizeof (la_els_logi_t)) {
				fp_register_login(NULL, pd, req, buf->ub_class);
			} else {
				mutex_enter(&pd->pd_mutex);
				if (pd->pd_count == 0) {
					pd->pd_count++;
				}
				pd->pd_state = PORT_DEVICE_LOGGED_IN;
				pd->pd_login_class = buf->ub_class;
				mutex_exit(&pd->pd_mutex);
			}
			listptr = kmem_zalloc(sizeof (fc_portmap_t), sleep);
			if (listptr != NULL) {
				fctl_copy_portmap(listptr, pd);
				(void) fp_ulp_devc_cb(port, listptr,
				    1, 1, sleep);
			}
		}
	}

	FP_INIT_CMD(cmd, buf->ub_class, FC_PKT_OUTBOUND,
	    FP_CMD_CFLAG_UNDEFINED, 1, NULL);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	cmd->cmd_job = job;

	pkt = FP_CMD_TO_PKT(cmd);

	fp_unsol_resp_init(pkt, buf, R_CTL_ELS_RSP, FC_TYPE_EXTENDED_LS);

	payload = port->fp_service_params;
	payload.LS_CODE = LA_ELS_ACC;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Handle RSCNs
 */
static void
fp_handle_unsol_rscn(fc_port_t *port, fc_unsol_buf_t *buf,
	job_request_t *job, int sleep)
{
	fp_cmd_t		*cmd;
	uint32_t		count;
	int			listindex;
	int16_t			len;
	fc_rscn_t		*payload;
	fc_portmap_t 		*listptr;
	fctl_ns_req_t		*ns_cmd;
	fc_affected_id_t 	*page;

	cmd = fp_alloc_pkt(port, FP_PORT_IDENTIFIER_LEN, 0, sleep);
	if (cmd != NULL) {
		fp_els_acc_init(port, cmd, buf, job);
		if (fp_sendcmd(port, cmd, port->fp_fca_handle) != FC_SUCCESS) {
			fp_free_pkt(cmd);
		}
	}

	payload = (fc_rscn_t *)buf->ub_buffer;
	ASSERT(payload->rscn_code == LA_ELS_RSCN);
	ASSERT(payload->rscn_len == FP_PORT_IDENTIFIER_LEN);

	len = payload->rscn_payload_len - FP_PORT_IDENTIFIER_LEN;

	if (len <= 0) {
		return;
	}

	ASSERT((len & 0x3) == 0);	/* Must be power of 4 */
	count = (len >> 2) << 1;	/* number of pages multiplied by 2 */

	listptr = kmem_zalloc(sizeof (fc_portmap_t) * count, sleep);
	page = (fc_affected_id_t *)(buf->ub_buffer + sizeof (fc_rscn_t));

	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gpn_id_t),
	    sizeof (ns_resp_gpn_id_t), sizeof (ns_resp_gpn_id_t),
	    0, sleep);
	if (ns_cmd == NULL) {
		kmem_free(listptr, sizeof (fc_portmap_t) * count);
		return;
	}

	ns_cmd->ns_cmd_code = NS_GPN_ID;

	for (listindex = 0; len; len -= FP_PORT_IDENTIFIER_LEN, page++) {
		/*
		 * Query the NS to get the Port WWN for this
		 * affected D_ID.
		 */
		switch (page->aff_format & FC_RSCN_ADDRESS_MASK) {
		case FC_RSCN_PORT_ADDRESS:
			fp_validate_rscn_page(port, page, job, ns_cmd,
			    listptr, &listindex, sleep);
			break;

		case FC_RSCN_AREA_ADDRESS:
			/* FALLTHROUGH */
		case FC_RSCN_DOMAIN_ADDRESS:
			fp_validate_area_domain(port, page->aff_d_id,
			    job, sleep);
			break;

		default:
			break;
		}
	}

	if (ns_cmd) {
		fctl_free_ns_cmd(ns_cmd);
	}

	if (listindex) {
		(void) fp_ulp_devc_cb(port, listptr, listindex, count, sleep);
	} else {
		kmem_free(listptr, sizeof (fc_portmap_t) * count);
	}
}


/*
 * Fill out old map for ULPs
 */
static void
fp_fillout_old_map(fc_portmap_t *map, fc_port_device_t *pd)
{
	int		is_switch;
	int		initiator;
	fc_port_t	*port;

	mutex_enter(&pd->pd_mutex);
	port = pd->pd_port;
	mutex_exit(&pd->pd_mutex);

	mutex_enter(&port->fp_mutex);
	mutex_enter(&pd->pd_mutex);

	pd->pd_state = PORT_DEVICE_INVALID;
	pd->pd_type = PORT_DEVICE_OLD;
	initiator = (pd->pd_recepient == PD_PLOGI_INITIATOR) ? 1 : 0;
	is_switch = FC_IS_TOP_SWITCH(port->fp_topology);

	fctl_delist_did_table(port, pd);
	fctl_delist_pwwn_table(port, pd);

	mutex_exit(&pd->pd_mutex);
	mutex_exit(&port->fp_mutex);

	ASSERT(port != NULL);
	if (port && initiator && is_switch) {
		(void) fctl_add_orphan(port, pd, KM_NOSLEEP);
	}
	fctl_copy_portmap(map, pd);
	map->map_pd = pd;
}


/*
 * Fillout Changed Map for ULPs
 */
static void
fp_fillout_changed_map(fc_portmap_t *map, fc_port_device_t *pd,
    uint32_t *new_did, la_wwn_t *new_pwwn)
{
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	pd->pd_type = PORT_DEVICE_CHANGED;
	if (new_did) {
		pd->PD_PORT_ID = *new_did;
	}
	if (new_pwwn) {
		pd->pd_port_name = *new_pwwn;
	}
	mutex_exit(&pd->pd_mutex);

	fctl_copy_portmap(map, pd);

	mutex_enter(&pd->pd_mutex);
	pd->pd_type = PORT_DEVICE_NOCHANGE;
}


/*
 * Fillout New Name Server map
 */
static void
fp_fillout_new_nsmap(fc_port_t *port, ddi_acc_handle_t *handle,
    fc_portmap_t *port_map, ns_resp_gan_t *gan_resp, uint32_t d_id)
{
	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	if (handle) {
		FP_CP_IN(*handle, &gan_resp->gan_pwwn, &port_map->map_pwwn,
		    sizeof (gan_resp->gan_pwwn));
		FP_CP_IN(*handle, &gan_resp->gan_nwwn, &port_map->map_nwwn,
		    sizeof (gan_resp->gan_nwwn));
		FP_CP_IN(*handle, gan_resp->gan_fc4types,
		    port_map->map_fc4_types, sizeof (gan_resp->gan_fc4types));
	} else {
		bcopy(&gan_resp->gan_pwwn, &port_map->map_pwwn,
		    sizeof (gan_resp->gan_pwwn));
		bcopy(&gan_resp->gan_nwwn, &port_map->map_nwwn,
		    sizeof (gan_resp->gan_nwwn));
		bcopy(gan_resp->gan_fc4types, port_map->map_fc4_types,
		    sizeof (gan_resp->gan_fc4types));
	}
	port_map->map_did.port_id = d_id;
	port_map->map_did.rsvd = 0;
	port_map->MAP_HARD_ADDR = 0;
	port_map->MAP_HARD_RSVD = 0;
	port_map->map_state = PORT_DEVICE_INVALID;
	port_map->map_flags = PORT_DEVICE_NEW;
	port_map->map_pd = NULL;

	ASSERT(port != NULL);
	(void) fctl_remove_if_orphan(port, &port_map->map_pwwn);
}


/*
 * Perform LINIT ELS
 */
static int
fp_remote_lip(fc_port_t *port, la_wwn_t *pwwn, int sleep, job_request_t *job)
{
	int			rval;
	uint32_t		d_id;
	uint32_t		s_id;
	uint32_t		lfa;
	uchar_t			class;
	uint32_t		ret;
	fp_cmd_t		*cmd;
	fc_packet_t		*pkt;
	fc_linit_req_t		payload;
	fc_port_device_t 	*pd;

	rval = 0;

	ASSERT(job != NULL);
	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

	pd = fctl_get_port_device_by_pwwn(port, pwwn);
	if (pd == NULL) {
		fctl_ns_req_t *ns_cmd;

		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
		    sizeof (ns_resp_gid_pn_t), sizeof (ns_resp_gid_pn_t),
		    0, sleep);

		if (ns_cmd == NULL) {
			return (FC_NOMEM);
		}
		job->job_result = FC_SUCCESS;
		ns_cmd->ns_cmd_code = NS_GID_PN;
		FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_code, pwwn);

		ret = fp_ns_query(port, ns_cmd, job, 1, sleep);
		if (ret != FC_SUCCESS || job->job_result != FC_SUCCESS) {
			fctl_free_ns_cmd(ns_cmd);
			return (FC_FAILURE);
		}
		bcopy(ns_cmd->ns_data_buf, (caddr_t)&d_id, sizeof (d_id));
		d_id = ((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->NS_PID;
		fctl_free_ns_cmd(ns_cmd);
		lfa = d_id & 0xFFFF00;
	} else {
		mutex_enter(&pd->pd_mutex);
		switch (pd->pd_porttype.port_type) {
		case FC_NS_PORT_NL:
		case FC_NS_PORT_F_NL:
		case FC_NS_PORT_FL:
			lfa = pd->PD_PORT_ID & 0xFFFF00;
			break;

		default:
			mutex_exit(&pd->pd_mutex);
			return (FC_FAILURE);
		}
		mutex_exit(&pd->pd_mutex);
	}

	mutex_enter(&port->fp_mutex);
	s_id = port->FP_PORT_ID;
	class = port->fp_ns_login_class;
	mutex_exit(&port->fp_mutex);

	cmd = fp_alloc_pkt(port, sizeof (fc_linit_req_t),
	    sizeof (fc_linit_resp_t), sleep);
	if (cmd == NULL) {
		return (FC_NOMEM);
	}

	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    FP_CMD_CFLAG_UNDEFINED, fp_retry_count, NULL);

	pkt = &cmd->cmd_pkt;
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	fp_els_init(cmd, s_id, lfa, fp_linit_intr, job);

	payload.LS_CODE = LA_ELS_LINIT;
	payload.MBZ = 0;
	payload.rsvd = 0;
	payload.func = 0;	/* Fabric determines the best way */
	payload.lip_b3 = 0;	/* clueless */
	payload.lip_b4 = 0;	/* clueless again */
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));

	FCTL_SET_JOB_COUNTER(job, 1);

	ret = fp_sendcmd(port, cmd, port->fp_fca_handle);
	if (ret == FC_SUCCESS) {
		fp_jobwait(job);
		rval = job->job_result;
	} else {
		rval = FC_FAILURE;
		fp_free_pkt(cmd);
	}

	return (rval);
}


/*
 * Fill out the device handles with GAN response
 */
static void
fp_stuff_device_with_gan(ddi_acc_handle_t *handle, fc_port_device_t *pd,
    ns_resp_gan_t *gan_resp)
{
	fc_device_t 	*node;
	fc_porttype_t	type;

	ASSERT(pd != NULL);
	ASSERT(handle != NULL);

	FP_TNF_PROBE_5((fp_stuff_device_with_gan, "fp_ns_trace",
	    "'PD stuffing'",
	    tnf_int, 	port_ID, 	gan_resp->gan_type_id.rsvd,
	    tnf_char, 	sym_len, 	gan_resp->gan_spnlen,
	    tnf_string,	name,		gan_resp->gan_spname,
	    tnf_char,	fc4type,	gan_resp->gan_fc4types[0],
	    tnf_opaque,	port_Handle,	pd));

	mutex_enter(&pd->pd_mutex);
	FP_CP_IN(*handle, &gan_resp->gan_type_id, &type, sizeof (type));
	pd->pd_porttype.port_type = type.port_type;
	pd->pd_porttype.rsvd = 0;

	pd->pd_spn_len = gan_resp->gan_spnlen;
	if (pd->pd_spn_len) {
		FP_CP_IN(*handle, gan_resp->gan_spname, pd->pd_spn,
		    pd->pd_spn_len);
	}

	FP_CP_IN(*handle, gan_resp->gan_ip, pd->pd_ip_addr,
	    sizeof (pd->pd_ip_addr));
	FP_CP_IN(*handle, &gan_resp->gan_cos, &pd->pd_cos, sizeof (pd->pd_cos));
	FP_CP_IN(*handle, gan_resp->gan_fc4types, pd->pd_fc4types,
	    sizeof (pd->pd_fc4types));
	node = pd->pd_device;
	mutex_exit(&pd->pd_mutex);

	mutex_enter(&node->fd_mutex);
	FP_CP_IN(*handle, gan_resp->gan_ipa, node->fd_ipa,
	    sizeof (node->fd_ipa));
	node->fd_snn_len = gan_resp->gan_snnlen;
	if (node->fd_snn_len) {
		FP_CP_IN(*handle, gan_resp->gan_snname, node->fd_snn,
		    node->fd_snn_len);
	}
	mutex_exit(&node->fd_mutex);
}


/*
 * Handles all NS Queries (also means that this function
 * doesn't handle NS object registration)
 */
static int
fp_ns_query(fc_port_t *port, fctl_ns_req_t *ns_cmd, job_request_t *job,
    int polled, int sleep)
{
	int 		rval;
	fp_cmd_t 	*cmd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	if (ns_cmd->ns_cmd_size == 0) {
		return (FC_FAILURE);
	}

	cmd = fp_alloc_pkt(port, sizeof (fc_ct_header_t) +
	    ns_cmd->ns_cmd_size, sizeof (fc_ct_header_t) +
	    ns_cmd->ns_resp_size, sleep);
	if (cmd == NULL) {
		return (FC_NOMEM);
	}

	FP_CT_INIT(port, cmd, ns_cmd, job);

	if (polled) {
		FCTL_SET_JOB_COUNTER(job, 1);
		ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);
	}
	rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
	if (rval != FC_SUCCESS) {
		job->job_result = rval;
		fp_iodone(cmd);
		if (polled == 0) {
			/*
			 * Return FC_SUCCESS to indicate that
			 * fp_iodone is performed already.
			 */
			rval = FC_SUCCESS;
		}
	}

	if (polled) {
		fp_jobwait(job);
		rval = job->job_result;
	}

	return (rval);
}


/*
 * Initialize Common Transport request
 */
static void
fp_ct_init(fc_port_t *port, fp_cmd_t *cmd, fctl_ns_req_t *ns_cmd,
	uint16_t cmd_code, caddr_t cmd_buf, uint16_t cmd_len,
	uint16_t resp_len, job_request_t *job)
{
	uint32_t	s_id;
	uchar_t		class;
	fc_packet_t 	*pkt;
	fc_ct_header_t 	ct;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	s_id = port->FP_PORT_ID;
	class = port->fp_ns_login_class;
	mutex_exit(&port->fp_mutex);

	cmd->cmd_job = job;
	cmd->cmd_private = ns_cmd;
	pkt = &cmd->cmd_pkt;

	ct.ct_rev = CT_REV;
	ct.ct_inid = 0;
	ct.ct_fcstype = FCSTYPE_DIRECTORY;
	ct.ct_fcssubtype = FCSSUB_NAME;
	ct.ct_options = 0;
	ct.ct_reserved1 = 0;
	ct.ct_cmdrsp = cmd_code;
	ct.ct_aiusize = resp_len + sizeof (fc_ct_header_t);
	ct.ct_reserved2 = 0;
	ct.ct_reason = 0;
	ct.ct_expln = 0;
	ct.ct_vendor = 0;

	FP_CP_OUT(pkt->pkt_cmd_acc, &ct, pkt->pkt_cmd, sizeof (ct));

	pkt->pkt_cmd_fhdr.r_ctl = R_CTL_UNSOL_CONTROL;
	pkt->pkt_cmd_fhdr.d_id = 0xFFFFFC;
	pkt->pkt_cmd_fhdr.s_id = s_id;
	pkt->pkt_cmd_fhdr.type = FC_TYPE_FC_SERVICES;
	pkt->pkt_cmd_fhdr.f_ctl = F_CTL_SEQ_INITIATIVE |
	    F_CTL_FIRST_SEQ | F_CTL_END_SEQ;
	pkt->pkt_cmd_fhdr.seq_id = 0;
	pkt->pkt_cmd_fhdr.df_ctl  = 0;
	pkt->pkt_cmd_fhdr.seq_cnt = 0;
	pkt->pkt_cmd_fhdr.ox_id = 0xffff;
	pkt->pkt_cmd_fhdr.rx_id = 0xffff;
	pkt->pkt_cmd_fhdr.ro = 0;
	pkt->pkt_cmd_fhdr.rsvd = 0;

	pkt->pkt_pd = NULL;
	pkt->pkt_comp = fp_ns_intr;
	pkt->pkt_ulp_private = (opaque_t)cmd;
	pkt->pkt_timeout = FP_NS_TIMEOUT;

	if (cmd_buf) {
		FP_CP_OUT(pkt->pkt_cmd_acc, cmd_buf, pkt->pkt_cmd +
		    sizeof (fc_ct_header_t), cmd_len);
	}
	cmd->cmd_transport = port->fp_fca_tran->fca_transport;
	FP_INIT_CMD(cmd, FC_TRAN_INTR | class, FC_PKT_EXCHANGE,
	    FP_CMD_PLOGI_DONT_CARE, fp_retry_count, NULL);
}


/*
 * Name Server request interrupt routine
 */
static void
fp_ns_intr(fc_packet_t *pkt)
{
	fc_ct_header_t 	resp_hdr;
	fc_ct_header_t 	cmd_hdr;
	fctl_ns_req_t	*ns_cmd;

	FP_CP_IN(pkt->pkt_cmd_acc, pkt->pkt_cmd, &cmd_hdr, sizeof (cmd_hdr));

	if (!FP_IS_PKT_ERROR(pkt)) {
		FP_CP_IN(pkt->pkt_resp_acc, pkt->pkt_resp, &resp_hdr,
		    sizeof (resp_hdr));

		ns_cmd = (fctl_ns_req_t *)(FP_PKT_TO_CMD(pkt)->cmd_private);
		if (ns_cmd) {
			/*
			 * Always copy out the response CT_HDR
			 */
			bcopy(&resp_hdr, &ns_cmd->ns_resp_hdr,
			    sizeof (resp_hdr));
		}

		if (resp_hdr.ct_cmdrsp == FS_RJT_IU) {
			pkt->pkt_state = FC_PKT_FS_RJT;
			pkt->pkt_reason = resp_hdr.ct_reason;
			pkt->pkt_expln = resp_hdr.ct_expln;
		}
	}

	if (FP_IS_PKT_ERROR(pkt)) {
		if (ns_cmd->ns_flags & FCTL_NS_VALIDATE_PD) {
			ASSERT(ns_cmd->ns_pd != NULL);

			/* Mark it OLD if not already done */
			mutex_enter(&ns_cmd->ns_pd->pd_mutex);
			ns_cmd->ns_pd->pd_type = PORT_DEVICE_OLD;
			mutex_exit(&ns_cmd->ns_pd->pd_mutex);
		}

		if (ns_cmd->ns_flags & FCTL_NS_ASYNC_REQUEST) {
			fctl_free_ns_cmd(ns_cmd);
			FP_PKT_TO_CMD(pkt)->cmd_private = NULL;
		}
		(void) fp_common_intr(pkt, 1);

		return;
	}

	ASSERT(resp_hdr.ct_cmdrsp == FS_ACC_IU);

	if (cmd_hdr.ct_cmdrsp == NS_GA_NXT) {
		fp_gan_handler(pkt, ns_cmd);
		return;
	}

	if (cmd_hdr.ct_cmdrsp >= NS_GPN_ID &&
	    cmd_hdr.ct_cmdrsp <= NS_GID_PT) {
		if (ns_cmd) {
			if ((ns_cmd->ns_flags & FCTL_NS_NO_DATA_BUF) == 0) {
				fp_ns_query_handler(pkt, ns_cmd);
				return;
			}
		}
	}

	fp_iodone(FP_PKT_TO_CMD(pkt));
}


/*
 * Process NS_GAN response
 */
static void
fp_gan_handler(fc_packet_t *pkt, fctl_ns_req_t *ns_cmd)
{
	int			my_did;
	fc_portid_t		d_id;
	fp_cmd_t		*cmd;
	fc_port_t		*port;
	fc_port_device_t	*pd;
	ns_req_gan_t		gan_req;
	ns_resp_gan_t		*gan_resp;

	ASSERT(ns_cmd != NULL);

	cmd = FP_PKT_TO_CMD(pkt);
	port = FP_CMD_TO_PORT(cmd);

	gan_resp = (ns_resp_gan_t *)(pkt->pkt_resp + sizeof (fc_ct_header_t));
	FP_CP_IN(pkt->pkt_resp_acc, &gan_resp->gan_type_id, &d_id,
	    sizeof (d_id));

	/*
	 * In this case the reserved field isn't reserved in reality
	 * it actually represents the port type - So zero it while
	 * dealing with Port Identifiers.
	 */
	d_id.rsvd = 0;
	pd = fctl_get_port_device_by_did(port, d_id.port_id);
	if (ns_cmd->ns_gan_sid == d_id.port_id) {
		/*
		 * We've come a full circle; time to get out.
		 */
		fp_iodone(cmd);
		return;
	}

	if (ns_cmd->ns_gan_sid == FCTL_GAN_START_ID) {
		ns_cmd->ns_gan_sid = d_id.port_id;
	}

	mutex_enter(&port->fp_mutex);
	my_did = (d_id.port_id == port->FP_PORT_ID) ? 1 : 0;
	mutex_exit(&port->fp_mutex);

	FP_TNF_PROBE_2((fp_gan_handler, "fp_ns_trace", "'Gan response'",
	    tnf_int, 		port_ID,	d_id.port_id,
	    tnf_opaque,		host_Port,	port));

	if (my_did == 0) {
		la_wwn_t pwwn;
		la_wwn_t nwwn;

		FP_TNF_PROBE_5((fp_gan_handler, "fp_ns_trace", "'Gan Details'",
		    tnf_int,		port_ID,	d_id.port_id,
		    tnf_porttype_t,	type_ID,	&gan_resp->gan_type_id,
		    tnf_wwn_t,		PWWN,		&gan_resp->gan_pwwn,
		    tnf_wwn_t,		NWWN,		&gan_resp->gan_nwwn,
		    tnf_opaque,		host_Port,	port));

		FP_CP_IN(pkt->pkt_resp_acc, &gan_resp->gan_nwwn, &nwwn,
		    sizeof (nwwn));

		FP_CP_IN(pkt->pkt_resp_acc, &gan_resp->gan_pwwn, &pwwn,
		    sizeof (pwwn));

		if (ns_cmd->ns_flags & FCTL_NS_CREATE_DEVICE && pd == NULL) {
			pd = fctl_create_port_device(port, &nwwn, &pwwn,
			    d_id.port_id, PD_PLOGI_INITIATOR, KM_NOSLEEP);
			if (pd != NULL) {
				fp_stuff_device_with_gan(&pkt->pkt_resp_acc,
				    pd, gan_resp);
			}
		}

		if (ns_cmd->ns_flags & FCTL_NS_GET_DEV_COUNT) {
			mutex_enter(&port->fp_mutex);
			port->fp_total_devices++;
			mutex_exit(&port->fp_mutex);
		}

		if (ns_cmd->ns_flags & FCTL_NS_FILL_NS_MAP) {
			ASSERT((ns_cmd->ns_flags & FCTL_NS_NO_DATA_BUF) == 0);

			if (ns_cmd->ns_flags & FCTL_NS_BUF_IS_USERLAND) {
				fc_port_dev_t *userbuf;

				userbuf = ((fc_port_dev_t *)
				    ns_cmd->ns_data_buf) +
				    ns_cmd->ns_gan_index++;

				userbuf->dev_did = d_id;
				FP_CP_IN(pkt->pkt_resp_acc,
				    gan_resp->gan_fc4types, userbuf->dev_type,
				    sizeof (userbuf->dev_type));
				userbuf->dev_nwwn = nwwn;
				userbuf->dev_pwwn = pwwn;

				if (pd != NULL) {
					mutex_enter(&pd->pd_mutex);
					userbuf->dev_state = pd->pd_state;
					userbuf->dev_hard_addr =
					    pd->pd_hard_addr;
					mutex_exit(&pd->pd_mutex);
				} else {
					userbuf->dev_state =
					    PORT_DEVICE_INVALID;
				}
			} else if (ns_cmd->ns_flags &
			    FCTL_NS_BUF_IS_FC_PORTMAP) {
				fc_portmap_t *map;

				map = ((fc_portmap_t *)
				    ns_cmd->ns_data_buf) +
				    ns_cmd->ns_gan_index++;
				/*
				 * First fill it like any new map
				 * and update the port device info
				 * below.
				 */
				fp_fillout_new_nsmap(port, &pkt->pkt_resp_acc,
				    map, gan_resp, d_id.port_id);
				if (pd != NULL) {
					fctl_copy_portmap(map, pd);
				} else {
					map->map_state = PORT_DEVICE_INVALID;
					map->map_flags = PORT_DEVICE_NOCHANGE;
				}
			} else {
				caddr_t dst_ptr;

				dst_ptr = ns_cmd->ns_data_buf +
				    (NS_GAN_RESP_LEN) *
				    ns_cmd->ns_gan_index++;

				FP_CP_IN(pkt->pkt_resp_acc, gan_resp, dst_ptr,
				    NS_GAN_RESP_LEN);
			}
		} else {
			ns_cmd->ns_gan_index++;
		}
		if (ns_cmd->ns_gan_index >= ns_cmd->ns_gan_max) {
			fp_iodone(cmd);
			return;
		}
	}

	gan_req.pid = d_id;
	FP_CP_OUT(pkt->pkt_cmd_acc, &gan_req, pkt->pkt_cmd +
	    sizeof (fc_ct_header_t), sizeof (gan_req));

	if (cmd->cmd_transport(port->fp_fca_handle, pkt) != FC_SUCCESS) {
		pkt->pkt_state = FC_PKT_TRAN_ERROR;
		fp_iodone(cmd);
	}
}


/*
 * Handle NS Query interrupt
 */
static void
fp_ns_query_handler(fc_packet_t *pkt, fctl_ns_req_t *ns_cmd)
{
	fp_cmd_t 	*cmd;
	caddr_t 	src_ptr;
	uint32_t 	xfer_len;

	cmd = FP_PKT_TO_CMD(pkt);

	xfer_len = ns_cmd->ns_resp_size;

	if (xfer_len <= ns_cmd->ns_data_len) {
		src_ptr = (caddr_t)pkt->pkt_resp + sizeof (fc_ct_header_t);
		FP_CP_IN(pkt->pkt_resp_acc, src_ptr, ns_cmd->ns_data_buf,
		    xfer_len);
	}

	if (ns_cmd->ns_flags & FCTL_NS_VALIDATE_PD) {
		ASSERT(ns_cmd->ns_pd != NULL);

		mutex_enter(&ns_cmd->ns_pd->pd_mutex);
		if (ns_cmd->ns_pd->pd_type == PORT_DEVICE_OLD) {
			ns_cmd->ns_pd->pd_type = PORT_DEVICE_NOCHANGE;
		}
		mutex_exit(&ns_cmd->ns_pd->pd_mutex);
	}

	if (ns_cmd->ns_flags & FCTL_NS_ASYNC_REQUEST) {
		fctl_free_ns_cmd(ns_cmd);
		FP_PKT_TO_CMD(pkt)->cmd_private = NULL;
	}
	fp_iodone(cmd);
}


/*
 * Handle unsolicited ADISC ELS request
 */
static void
fp_handle_unsol_adisc(fc_port_t *port, fc_unsol_buf_t *buf,
    fc_port_device_t *pd, job_request_t *job)
{
	int		rval;
	fp_cmd_t	*cmd;

	mutex_enter(&pd->pd_mutex);
	if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
		mutex_exit(&pd->pd_mutex);
		if (FP_IS_CLASS_1_OR_2(buf->ub_class)) {
			cmd = fp_alloc_pkt(port, sizeof (la_els_rjt_t),
			    0, KM_SLEEP);
			if (cmd != NULL) {
				fp_els_rjt_init(port, cmd, buf,
				    FC_ACTION_NON_RETRYABLE,
				    FC_REASON_INVALID_LINK_CTRL, job);

				if (fp_sendcmd(port, cmd,
				    port->fp_fca_handle) != FC_SUCCESS) {
					fp_free_pkt(cmd);
				}
			}
		}
	} else {
		mutex_exit(&pd->pd_mutex);
		/*
		 * Yes, yes, we don't have a hard address. But we
		 * we should still respond. Huh ? Visit 21.19.2
		 * of FC-PH-2 which essentially says that if an
		 * NL_Port doesn't have a hard address, or if a port
		 * does not have FC-AL capability, it shall report
		 * zeroes in this field.
		 */
		cmd = fp_alloc_pkt(port, sizeof (la_els_adisc_t),
		    0, KM_SLEEP);
		if (cmd == NULL) {
			return;
		}
		fp_adisc_acc_init(port, cmd, buf, job);
		rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
		if (rval != FC_SUCCESS) {
			fp_free_pkt(cmd);
		}
	}
}


/*
 * Initialize ADISC response.
 */
static void
fp_adisc_acc_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job)
{
	fc_packet_t	*pkt;
	la_els_adisc_t	payload;

	FP_INIT_CMD(cmd, buf->ub_class, FC_PKT_OUTBOUND,
		FP_CMD_CFLAG_UNDEFINED, 1, NULL);
	cmd->cmd_transport = port->fp_fca_tran->fca_els_send;
	cmd->cmd_job = job;

	pkt = FP_CMD_TO_PKT(cmd);

	fp_unsol_resp_init(pkt, buf, R_CTL_ELS_RSP, FC_TYPE_EXTENDED_LS);

	payload.LS_CODE = LA_ELS_ACC;
	payload.MBZ = 0;

	mutex_enter(&port->fp_mutex);
	payload.nport_id = port->fp_port_id;
	payload.hard_addr = port->fp_hard_addr;
	mutex_exit(&port->fp_mutex);

	payload.port_wwn = port->fp_service_params.nport_ww_name;
	payload.node_wwn = port->fp_service_params.node_ww_name;
	FP_CP_OUT(pkt->pkt_cmd_acc, &payload, pkt->pkt_cmd, sizeof (payload));
}


/*
 * Hold and Install the requested ULP drivers
 */
static void
fp_load_ulp_modules(dev_info_t *dip, fc_port_t *port)
{
	int		len;
	int		count;
	int		data_len;
	major_t		ulp_major;
	caddr_t		ulp_name;
	caddr_t		data_ptr;
	caddr_t		data_buf;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	data_buf = NULL;
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "load-ulp-list",
	    (caddr_t)&data_buf, &data_len) != DDI_PROP_SUCCESS) {

		return;
	}

	len = strlen(data_buf);
	port->fp_ulp_nload = fctl_atoi(data_buf, 10);
	port->fp_ulp_majors = kmem_zalloc(
	    sizeof (major_t) * port->fp_ulp_nload, KM_SLEEP);

	data_ptr = data_buf + len + 1;
	for (count = 0; count < port->fp_ulp_nload; count++) {
		len = strlen(data_ptr) + 1;
		ulp_name = kmem_zalloc(len, KM_SLEEP);
		bcopy(data_ptr, ulp_name, len);

		ulp_major = ddi_name_to_major(ulp_name);
		port->fp_ulp_majors[count] = ulp_major;

		if (ulp_major != (major_t)-1) {
			/*
			 * Don't get into the trouble of attaching drivers
			 * if we are in the boot process as there are some
			 * genuine problems.
			 */
			if (FP_IS_SYSTEM_BOOTING) {
				if (modload("drv", ulp_name) < 0) {
					fp_printf(port, CE_NOTE, FP_LOG_ONLY,
					    0, NULL, "failed to load %s",
					    ulp_name);
				}
				port->fp_ulp_majors[count] = (major_t)-1;
			} else {
				if (ddi_hold_installed_driver(ulp_major) ==
				    NULL) {
					port->fp_ulp_majors[count] =
					    (major_t)-1;
					fp_printf(port, CE_NOTE, FP_LOG_ONLY,
					    0, NULL, "failed to hold %s",
					    ulp_name);
				}
			}
		} else {
			fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
			    "%s isn't a valid driver", ulp_name);
		}
		kmem_free(ulp_name, len);
		data_ptr += len + 1;	/* Skip comma */
	}

	/*
	 * Free the memory allocated by DDI
	 */
	if (data_buf != NULL) {
		kmem_free(data_buf, data_len);
	}
}


/*
 * Perform LOGO operation
 */
static int
fp_logout(fc_port_t *port, fc_port_device_t *pd, job_request_t *job)
{
	int		rval;
	fp_cmd_t 	*cmd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));
	ASSERT(!MUTEX_HELD(&pd->pd_mutex));

	cmd = fp_alloc_pkt(port, sizeof (la_els_logo_t),
	    FP_PORT_IDENTIFIER_LEN, KM_SLEEP);

	mutex_enter(&port->fp_mutex);
	mutex_enter(&pd->pd_mutex);

	ASSERT(pd->pd_state == PORT_DEVICE_LOGGED_IN);
	ASSERT(pd->pd_count == 1);

	FP_INIT_CMD(cmd, FC_TRAN_INTR | pd->pd_login_class,
	    FC_PKT_EXCHANGE, 0, 1, NULL);
	fp_logo_init(pd, cmd, job);
	mutex_exit(&pd->pd_mutex);
	mutex_exit(&port->fp_mutex);

	rval = fp_sendcmd(port, cmd, port->fp_fca_handle);
	if (rval != FC_SUCCESS) {
		fp_iodone(cmd);
	}

	return (rval);
}


/*
 * Perform Port attach callbacks to registered ULPs
 */
static void
fp_attach_ulps(fc_port_t *port, ddi_attach_cmd_t cmd)
{
	fp_soft_attach_t *att;

	att = kmem_zalloc(sizeof (*att), KM_SLEEP);
	att->att_cmd = cmd;
	att->att_port = port;

	ddi_set_callback(fp_ulp_port_attach, (caddr_t)att, &port->fp_softid);
	ddi_run_callback(&port->fp_softid);
}


/*
 * Notify the ULP of the state change
 */
static int
fp_ulp_notify(fc_port_t *port, uint32_t statec, int sleep)
{
	fc_port_clist_t *clist;

	clist = kmem_zalloc(sizeof (*clist), sleep);
	if (clist == NULL) {
		return (FC_NOMEM);
	}

	clist->clist_state = statec;

	mutex_enter(&port->fp_mutex);
	clist->clist_flags = port->fp_topology;
	mutex_exit(&port->fp_mutex);

	clist->clist_port = (opaque_t)port;
	clist->clist_len = 0;
	clist->clist_size = 0;
	clist->clist_map = NULL;

	ddi_set_callback(fctl_ulp_statec_cb, (caddr_t)clist, &port->fp_softid);
	ddi_run_callback(&port->fp_softid);

	return (FC_SUCCESS);
}


/*
 * Get name server map
 */
static int
fp_ns_getmap(fc_port_t *port, job_request_t *job, fc_portmap_t **map,
    uint32_t *len)
{
	int ret;
	fctl_ns_req_t *ns_cmd;

	/*
	 * Don't let the allocator do anything for response;
	 * we have have buffer ready to fillout.
	 */
	ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gan_t),
	    sizeof (ns_resp_gan_t), 0, (FCTL_NS_FILL_NS_MAP |
	    FCTL_NS_BUF_IS_FC_PORTMAP), KM_SLEEP);

	ns_cmd->ns_data_len = sizeof (**map) * (*len);
	ns_cmd->ns_data_buf = (caddr_t)*map;

	ASSERT(ns_cmd != NULL);

	ns_cmd->ns_gan_index = 0;
	ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
	ns_cmd->ns_cmd_code = NS_GA_NXT;
	ns_cmd->ns_gan_max = *len;

	ret = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);

	if (ns_cmd->ns_gan_index != *len) {
		*len = ns_cmd->ns_gan_index;
	}
	ns_cmd->ns_data_len = 0;
	ns_cmd->ns_data_buf = NULL;
	fctl_free_ns_cmd(ns_cmd);

	return (ret);
}


/*
 * Create a Port device handle in Fabric topology by using NS services
 */
static fc_port_device_t *
fp_create_port_device_by_ns(fc_port_t *port, uint32_t d_id, int sleep)
{
	int			rval;
	job_request_t 		*job;
	fctl_ns_req_t 		*ns_cmd;
	fc_port_device_t	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	FP_TNF_PROBE_2((fp_create_port_device_by_ns, "fp_ns_trace",
	    "'PD creation begin'",
	    tnf_int,		port_ID,	d_id,
	    tnf_opaque,		host_Port,	port));

#ifdef	DEBUG
	mutex_enter(&port->fp_mutex);
	ASSERT(FC_IS_TOP_SWITCH(port->fp_topology));
	mutex_exit(&port->fp_mutex);
#endif /* DEBUG */

	job = fctl_alloc_job(JOB_NS_CMD, 0, NULL, (opaque_t)port, sleep);
	if (job == NULL) {
		return (NULL);
	}

	ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gan_t),
	    sizeof (ns_resp_gan_t), 0, (FCTL_NS_CREATE_DEVICE |
	    FCTL_NS_NO_DATA_BUF), sleep);
	if (ns_cmd == NULL) {
		return (NULL);
	}

	job->job_result = FC_SUCCESS;
	ns_cmd->ns_gan_max = 1;
	ns_cmd->ns_cmd_code = NS_GA_NXT;
	ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
	FCTL_NS_GAN_INIT(ns_cmd->ns_cmd_buf, d_id - 1);

	ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);
	rval = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);
	fctl_free_ns_cmd(ns_cmd);

	if (rval != FC_SUCCESS || job->job_result != FC_SUCCESS) {
		fctl_dealloc_job(job);
		return (NULL);
	}
	fctl_dealloc_job(job);

	pd = fctl_get_port_device_by_did(port, d_id);

	FP_TNF_PROBE_3((fp_create_port_device_by_ns, "fp_ns_trace",
	    "'PD creation end'",
	    tnf_int,		port_ID,	d_id,
	    tnf_opaque,		host_Port,	port,
	    tnf_opaque,		pd_Handle,	pd));

	return (pd);
}


/*
 * Check for the permissions on an ioctl command. If it is required to have an
 * EXCLUSIVE open performed, return a FAILURE to just shut the door on it. If
 * the ioctl command isn't in one of the list built, shut the door on that too.
 *
 * 	Certain ioctls perform hardware accesses in FCA drivers, and it needs
 *	to be made sure that users open the port for an exclusive access while
 *	performing those operations.
 *
 * 	This can prevent a causual user from inflicting damage on the port by
 *	sending these ioctls from multiple processes/threads (there is no good
 *	reason why one would need to do that) without actually realizing how
 *	expensive such commands could turn out to be.
 *
 *	It is also important to note that, even with an exclsive access,
 *	multiple threads can share the same file descriptor and fire down
 *	commands in parallel. To prevent that the driver needs to make sure
 *	that such commands aren't in progress already. This is taken care of
 *	in the FP_EXCL_BUSY bit of fp_flag.
 */
static int
fp_check_perms(uchar_t open_flag, uint16_t ioctl_cmd)
{
	int ret;
	int count;

	ret = FC_FAILURE;
	for (count = 0; count < sizeof (fp_perm_list) /
	    sizeof (fp_perm_list[0]); count++) {
		if (fp_perm_list[count].fp_ioctl_cmd == ioctl_cmd) {
			if (fp_perm_list[count].fp_open_flag & open_flag) {
				ret = FC_SUCCESS;
			}
			break;
		}
	}

	return (ret);
}


/*
 * Bind Port driver's unsolicited, state change callbacks
 */
static int
fp_bind_callbacks(fc_port_t *port)
{
	uchar_t			*class_ptr;
	fc_fca_bind_info_t	bind_info;
	fc_fca_port_info_t	*port_info;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	/*
	 * fca_bind_port returns the FCA port handle. If
	 * the port number isn't supported it returns NULL.
	 * It also sets up callback in the FCA for various
	 * things like state change, ELS etc..
	 */
	bind_info.port_statec_cb = fp_statec_cb;
	bind_info.port_unsol_cb = fp_unsol_cb;
	bind_info.port_num = port->fp_port_num;
	bind_info.port_handle = (opaque_t)port;

	mutex_exit(&port->fp_mutex);
	port_info = kmem_zalloc(sizeof (*port_info), KM_SLEEP);

	mutex_enter(&port->fp_mutex);
	port->fp_fca_handle = port->fp_fca_tran->fca_bind_port(
	    port->fp_fca_dip, port_info, &bind_info);
	if (port->fp_fca_handle == NULL) {
		kmem_free(port_info, sizeof (*port_info));
		return (DDI_FAILURE);
	}
	port->fp_bind_state = port->fp_state = port_info->pi_port_state;
	port->fp_service_params = port_info->pi_login_params;
	port->fp_hard_addr = port_info->pi_hard_addr;

	/* zero out the normally unused fields right away */
	port->fp_service_params.MBZ = 0;
	port->fp_service_params.LS_CODE = 0;
	bzero(&port->fp_service_params.reserved,
	    sizeof (port->fp_service_params.reserved));

	class_ptr = &port_info->pi_login_params.class_1.data[0];
	port->fp_cos |= (*class_ptr & 0x80) ? FC_NS_CLASS1 : 0;

	class_ptr = &port_info->pi_login_params.class_2.data[0];
	port->fp_cos |= (*class_ptr & 0x80) ? FC_NS_CLASS2 : 0;

	class_ptr = &port_info->pi_login_params.class_3.data[0];
	port->fp_cos |= (*class_ptr & 0x80) ? FC_NS_CLASS3 : 0;

	kmem_free(port_info, sizeof (*port_info));

	return (DDI_SUCCESS);
}


/*
 * Retrieve FCA capabilities
 */
static void
fp_retrieve_caps(fc_port_t *port)
{
	int			rval;
	int 			ub_count;
	fc_reset_action_t	action;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	rval = port->fp_fca_tran->fca_get_cap(port->fp_fca_handle,
	    FC_CAP_UNSOL_BUF, &ub_count);

	switch (rval) {
	case FC_CAP_FOUND:
	case FC_CAP_SETTABLE:
		switch (ub_count) {
		case 0:
			break;

		case -1:
			ub_count = fp_unsol_buf_count;
			break;

		default:
			/* 1/4th of total buffers is my share */
			ub_count =
			    (ub_count / port->fp_fca_tran->fca_numports) >> 2;
			break;
		}
		break;

	default:
		ub_count = 0;
		break;
	}

	mutex_enter(&port->fp_mutex);
	port->fp_ub_count = ub_count;
	mutex_exit(&port->fp_mutex);

	rval = port->fp_fca_tran->fca_get_cap(port->fp_fca_handle,
	    FC_CAP_POST_RESET_BEHAVIOR, &action);

	switch (rval) {
	case FC_CAP_FOUND:
	case FC_CAP_SETTABLE:
		switch (action) {
		case FC_RESET_RETURN_NONE:
		case FC_RESET_RETURN_ALL:
		case FC_RESET_RETURN_OUTSTANDING:
			break;

		default:
			action = FC_RESET_RETURN_NONE;
			break;
		}
		break;

	default:
		action = FC_RESET_RETURN_NONE;
		break;
	}
	mutex_enter(&port->fp_mutex);
	port->fp_reset_action = action;
	mutex_exit(&port->fp_mutex);
}


/*
 * Handle Domain, Area changes in the Fabric.
 */
static void
fp_validate_area_domain(fc_port_t *port, uint32_t id, job_request_t *job,
    int sleep)
{
	int			rval;
	int			send;
	int			count;
	int			index;
	int			listindex;
	int			login;
	int			job_flags;
	uint32_t		d_id;
	fctl_ns_req_t		*ns_cmd;
	fc_portmap_t		*list;
	fc_orphan_t 		*orp;
	fc_orphan_t		*norp;
	fc_orphan_t		*prev;
	fc_port_device_t 	*pd;
	fc_port_device_t 	*next;
	struct pwwn_hash 	*head;

	mutex_enter(&port->fp_mutex);
	for (count = index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;
		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if ((pd->PD_PORT_ID & id) == id &&
			    pd->pd_recepient == PD_PLOGI_INITIATOR) {
				count++;
				pd->pd_type = PORT_DEVICE_OLD;
				pd->pd_flags = PD_ELS_IN_PROGRESS;
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}
	count += port->fp_orphan_count;
	if (port->fp_orphan_count) {
		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
		    sizeof (ns_resp_gid_pn_t), sizeof (ns_resp_gid_pn_t),
		    0, sleep);
		if (ns_cmd != NULL) {
			ns_cmd->ns_cmd_code = NS_GID_PN;
		}
	} else {
		ns_cmd = NULL;
	}
	mutex_exit(&port->fp_mutex);

	if (count == 0) {
		return;
	}

	list = kmem_zalloc(sizeof (*list) * count, sleep);
	if (list == NULL) {
		fp_printf(port, CE_NOTE, FP_LOG_ONLY, 0, NULL,
		    " Not enough memory to service RSCNs,"
		    " continuing...");

		return;
	}

	mutex_enter(&port->fp_mutex);
	for (listindex = 0, index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;

		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if ((pd->PD_PORT_ID & id) == id &&
			    pd->pd_recepient == PD_PLOGI_INITIATOR) {
				mutex_exit(&pd->pd_mutex);
				fctl_copy_portmap(list + listindex++, pd);
				mutex_enter(&pd->pd_mutex);
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}

	FCTL_SET_JOB_COUNTER(job, listindex);
	job_flags = job->job_flags;
	job->job_flags |= JOB_TYPE_FP_ASYNC;

	for (index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;

		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			next = pd->pd_wwn_hnext;
			d_id = pd->PD_PORT_ID;
			login = (pd->pd_state == PORT_DEVICE_LOGGED_IN) ? 1 : 0;
			send = (pd->pd_recepient == PD_PLOGI_INITIATOR) ? 1 : 0;

			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);

			if ((d_id & id) == id && send) {
				if (login) {
					rval = fp_port_login(port, d_id, job,
					    FP_CMD_PLOGI_RETAIN, sleep, pd,
					    NULL);
					if (rval != FC_SUCCESS) {
						job->job_result = rval;
						fp_jobdone(job);
					}
				} else {
					rval = fp_ns_validate_device(port, pd,
					    job, 0, sleep);
					if (rval != FC_SUCCESS) {
						fp_jobdone(job);
					}
					mutex_enter(&pd->pd_mutex);
					pd->pd_flags = PD_IDLE;
					mutex_exit(&pd->pd_mutex);
				}
			}
			pd = next;
			mutex_enter(&port->fp_mutex);
		}
	}
	mutex_exit(&port->fp_mutex);

	if (listindex) {
		fctl_jobwait(job);
	}
	job->job_flags = job_flags;

	/*
	 * Orphan list validation.
	 */
	mutex_enter(&port->fp_mutex);
	for (prev = NULL, orp = port->fp_orphan_list; ns_cmd &&
	    port->fp_orphan_count && orp != NULL; orp = norp) {
		norp = orp->orp_next;
		mutex_exit(&port->fp_mutex);

		FCTL_SET_JOB_COUNTER(job, 1);
		job->job_result = FC_SUCCESS;
		ASSERT((job->job_flags & JOB_TYPE_FP_ASYNC) == 0);

		FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_buf, &orp->orp_pwwn);
		((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->NS_PID = 0;
		((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->pid.rsvd = 0;

		rval = fp_ns_query(port, ns_cmd, job, 1, KM_SLEEP);
		if (rval == FC_SUCCESS) {
			d_id = ((ns_resp_gid_pn_t *)
			    ns_cmd->ns_data_buf)->NS_PID;
			pd = fp_create_port_device_by_ns(port, d_id, KM_SLEEP);
			if (pd != NULL) {
				mutex_enter(&port->fp_mutex);
				if (prev) {
					prev->orp_next = orp->orp_next;
				} else {
					ASSERT(orp == port->fp_orphan_list);
					port->fp_orphan_list = orp->orp_next;
				}
				port->fp_orphan_count--;
				mutex_exit(&port->fp_mutex);

				kmem_free(orp, sizeof (*orp));
				fctl_copy_portmap(list + listindex++, pd);
			} else {
				prev = orp;
			}
		} else {
			prev = orp;
		}
		mutex_enter(&port->fp_mutex);
	}
	mutex_exit(&port->fp_mutex);

	for (index = 0; index < listindex; index++) {
		pd = list[index].map_pd;
		ASSERT(pd != NULL);

		/*
		 * Update PLOGI results; For NS validation
		 * of orphan list, it is redundant
		 */
		fctl_copy_portmap(list + index, pd);

		mutex_enter(&pd->pd_mutex);
		if (pd->pd_type == PORT_DEVICE_OLD) {
			int initiator;

			pd->pd_flags = PD_IDLE;
			initiator = (pd->pd_recepient ==
			    PD_PLOGI_INITIATOR) ? 1 : 0;

			mutex_exit(&pd->pd_mutex);

			mutex_enter(&port->fp_mutex);
			mutex_enter(&pd->pd_mutex);

			pd->pd_state = PORT_DEVICE_INVALID;
			fctl_delist_did_table(port, pd);
			fctl_delist_pwwn_table(port, pd);

			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);

			if (initiator) {
				(void) fctl_add_orphan(port, pd, sleep);
			}
			list[index].map_pd = pd;
		} else {
			ASSERT(pd->pd_flags == PD_IDLE);
			mutex_exit(&pd->pd_mutex);
		}
	}

	if (ns_cmd) {
		fctl_free_ns_cmd(ns_cmd);
	}
	if (listindex) {
		(void) fp_ulp_devc_cb(port, list, listindex, count, sleep);
	} else {
		kmem_free(list, sizeof (*list) * count);
	}
}


/*
 * Work hard to make sense out of an RSCN page.
 */
static void
fp_validate_rscn_page(fc_port_t *port, fc_affected_id_t *page,
    job_request_t *job, fctl_ns_req_t *ns_cmd, fc_portmap_t *listptr,
    int *listindex, int sleep)
{
	int			rval;
	la_wwn_t		pwwn;
	fc_port_device_t	*pwwn_pd;
	fc_port_device_t	*did_pd;

	FCTL_NS_GPN_ID_INIT(ns_cmd->ns_cmd_buf, page->aff_d_id);
	bzero(ns_cmd->ns_data_buf, sizeof (la_wwn_t));
	rval = fp_ns_query(port, ns_cmd, job, 1, sleep);
	pwwn = ((ns_resp_gpn_id_t *)ns_cmd->ns_data_buf)->pwwn;

	if (rval != FC_SUCCESS || fctl_is_wwn_zero(&pwwn) == FC_SUCCESS) {
		/*
		 * What this means is that the D_ID
		 * disappeared from the Fabric.
		 */
		did_pd = fctl_get_port_device_by_did(port, page->aff_d_id);
		if (did_pd == NULL) {
			return;
		}
		fp_fillout_old_map(listptr + (*listindex)++, did_pd);

		return;
	}

	did_pd = fctl_get_port_device_by_did(port, page->aff_d_id);
	pwwn_pd = fctl_get_port_device_by_pwwn(port, &pwwn);

	if (did_pd != NULL && pwwn_pd != NULL && did_pd == pwwn_pd) {
		/*
		 * There is no change. Do PLOGI again and add it to
		 * ULP portmap baggage and return. Note: When RSCNs
		 * arrive with per page states, the need for PLOGI
		 * can be determined correctly.
		 */
		mutex_enter(&pwwn_pd->pd_mutex);
		pwwn_pd->pd_type = PORT_DEVICE_NOCHANGE;
		mutex_exit(&pwwn_pd->pd_mutex);
		fctl_copy_portmap(listptr + (*listindex)++, pwwn_pd);

		mutex_enter(&pwwn_pd->pd_mutex);
		if (pwwn_pd->pd_state == PORT_DEVICE_LOGGED_IN) {
			mutex_exit(&pwwn_pd->pd_mutex);
			rval = fp_port_login(port, page->aff_d_id, job,
			    FP_CMD_PLOGI_RETAIN, sleep, pwwn_pd, NULL);
			if (rval == FC_SUCCESS) {
				fp_jobwait(job);
				if (job->job_result != FC_SUCCESS) {
					fp_fillout_old_map(listptr +
					    *listindex - 1, pwwn_pd);
				} else {
					fp_fillout_old_map(listptr +
					    *listindex - 1, pwwn_pd);
				}
			}
		} else {
			mutex_exit(&pwwn_pd->pd_mutex);
		}

		return;
	}

	if (did_pd == NULL && pwwn_pd == NULL) {
		/*
		 * Hunt down the orphan list before giving up.
		 */
		mutex_enter(&port->fp_mutex);
		if (port->fp_orphan_count) {
			fc_orphan_t 	*orp;
			fc_orphan_t	*norp = NULL;
			fc_orphan_t	*prev = NULL;

			for (orp = port->fp_orphan_list; orp; orp = norp) {
				norp = orp->orp_next;

				if (fctl_wwn_cmp(&orp->orp_pwwn, &pwwn) != 0) {
					continue;
				}

				mutex_exit(&port->fp_mutex);
				pwwn_pd = fp_create_port_device_by_ns(port,
				    page->aff_d_id, sleep);

				if (pwwn_pd != NULL) {
					fctl_copy_portmap(listptr +
					    (*listindex)++, pwwn_pd);

					mutex_enter(&port->fp_mutex);
					if (prev) {
						prev->orp_next = orp->orp_next;
					} else {
						ASSERT(orp ==
						    port->fp_orphan_list);
						port->fp_orphan_list =
						    orp->orp_next;
					}
					port->fp_orphan_count--;
					kmem_free(orp, sizeof (*orp));
				} else {
					mutex_enter(&port->fp_mutex);
				}
				break;
			}
			mutex_exit(&port->fp_mutex);
		} else {
			mutex_exit(&port->fp_mutex);
		}

		return;
	}

	if (pwwn_pd != NULL && did_pd == NULL) {
		uint32_t d_id = page->aff_d_id;

		/*
		 * What this means is there is a new D_ID for this
		 * Port WWN. Take out the port device off D_ID
		 * list and put it back with a new D_ID. Perform
		 * PLOGI if already logged in.
		 */
		mutex_enter(&port->fp_mutex);
		mutex_enter(&pwwn_pd->pd_mutex);

		fctl_delist_did_table(port, pwwn_pd);
		fp_fillout_changed_map(listptr + (*listindex)++, pwwn_pd,
		    &d_id, NULL);
		fctl_enlist_did_table(port, pwwn_pd);

		if (pwwn_pd->pd_state == PORT_DEVICE_LOGGED_IN) {
			mutex_exit(&pwwn_pd->pd_mutex);
			mutex_exit(&port->fp_mutex);

			rval = fp_port_login(port, page->aff_d_id, job,
			    FP_CMD_PLOGI_RETAIN, sleep, pwwn_pd, NULL);
			if (rval == FC_SUCCESS) {
				fp_jobwait(job);
				if (job->job_result != FC_SUCCESS) {
					fp_fillout_old_map(listptr +
					    *listindex - 1, pwwn_pd);
				} else {
					fp_fillout_old_map(listptr +
					    *listindex - 1, pwwn_pd);
				}
			}
		} else {
			mutex_exit(&pwwn_pd->pd_mutex);
			mutex_exit(&port->fp_mutex);
		}

		return;
	}

	if (pwwn_pd == NULL && did_pd != NULL) {
		/*
		 * What this means is that there is a new Port WWN for this
		 * D_ID. Take out the port device off Port WWN list and put
		 * it back with the new Port WWN. Perform PLOGI if already
		 * logged in.
		 */
		mutex_enter(&port->fp_mutex);
		mutex_enter(&did_pd->pd_mutex);

		fctl_delist_pwwn_table(port, did_pd);
		fp_fillout_changed_map(listptr + (*listindex)++, did_pd,
		    NULL, &pwwn);
		fctl_enlist_pwwn_table(port, did_pd);

		if (did_pd->pd_state == PORT_DEVICE_LOGGED_IN) {
			mutex_exit(&did_pd->pd_mutex);
			mutex_exit(&port->fp_mutex);

			rval = fp_port_login(port, page->aff_d_id, job,
			    FP_CMD_PLOGI_RETAIN, sleep, did_pd, NULL);
			if (rval == FC_SUCCESS) {
				fp_jobwait(job);
				if (job->job_result != FC_SUCCESS) {
					fp_fillout_old_map(listptr +
					    *listindex - 1, did_pd);
				}
			} else {
				fp_fillout_old_map(listptr + *listindex - 1,
				    did_pd);
			}
		} else {
			mutex_exit(&did_pd->pd_mutex);
			mutex_exit(&port->fp_mutex);
		}

		return;
	}

	/*
	 * A weird case of Port WWN and D_ID existence but not matching up
	 * between them. Trust your instincts - Take the port device handle
	 * off Port WWN list, fix it with new Port WWN and put it back, In
	 * the mean time mark the port device corresponding to the old port
	 * WWN as OLD.
	 */
	mutex_enter(&port->fp_mutex);
	mutex_enter(&pwwn_pd->pd_mutex);

	pwwn_pd->pd_type = PORT_DEVICE_OLD;
	pwwn_pd->pd_state = PORT_DEVICE_INVALID;
	fctl_delist_did_table(port, pwwn_pd);
	fctl_delist_pwwn_table(port, pwwn_pd);

	mutex_exit(&pwwn_pd->pd_mutex);
	mutex_exit(&port->fp_mutex);

	fctl_copy_portmap(listptr + (*listindex)++, pwwn_pd);

	mutex_enter(&port->fp_mutex);
	mutex_enter(&did_pd->pd_mutex);

	fctl_delist_pwwn_table(port, did_pd);
	fp_fillout_changed_map(listptr + (*listindex)++, did_pd, NULL, &pwwn);
	fctl_enlist_pwwn_table(port, did_pd);

	if (did_pd->pd_state == PORT_DEVICE_LOGGED_IN) {
		mutex_exit(&did_pd->pd_mutex);
		mutex_exit(&port->fp_mutex);

		rval = fp_port_login(port, page->aff_d_id, job,
		    FP_CMD_PLOGI_RETAIN, sleep, pwwn_pd, NULL);
		if (rval == FC_SUCCESS) {
			fp_jobwait(job);
			if (job->job_result != FC_SUCCESS) {
				fp_fillout_old_map(listptr +
				    *listindex - 1, pwwn_pd);
			}
		} else {
			fp_fillout_old_map(listptr + *listindex - 1, did_pd);
		}
	} else {
		mutex_exit(&did_pd->pd_mutex);
		mutex_exit(&port->fp_mutex);
	}
}


static int
fp_ns_validate_device(fc_port_t *port, fc_port_device_t *pd,
    job_request_t *job, int polled, int sleep)
{
	la_wwn_t	pwwn;
	uint32_t	flags;
	fctl_ns_req_t	*ns_cmd;

	flags = FCTL_NS_VALIDATE_PD | ((polled) ? 0: FCTL_NS_ASYNC_REQUEST);
	ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
	    sizeof (ns_resp_gid_pn_t), sizeof (ns_resp_gid_pn_t),
	    flags, sleep);
	if (ns_cmd == NULL) {
		return (FC_NOMEM);
	}

	mutex_enter(&pd->pd_mutex);
	pwwn = pd->pd_port_name;
	mutex_exit(&pd->pd_mutex);

	ns_cmd->ns_cmd_code = NS_GID_PN;
	ns_cmd->ns_pd = pd;
	FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_buf, &pwwn);
	((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->NS_PID = 0;
	((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->pid.rsvd = 0;

	return (fp_ns_query(port, ns_cmd, job, polled, sleep));
}


static int
fp_validate_lilp_map(fc_lilpmap_t *lilp_map)
{
	int	count;

	if (lilp_map->lilp_length == 0) {
		return (FC_FAILURE);
	}

	for (count = 0; count < lilp_map->lilp_length; count++) {
		if (fp_is_valid_alpa(lilp_map->lilp_alpalist[count]) !=
		    FC_SUCCESS) {
			return (FC_FAILURE);
		}
	}

	return (FC_SUCCESS);
}


static int
fp_is_valid_alpa(uchar_t al_pa)
{
	int 	count;

	for (count = 0; count < sizeof (fp_valid_alpas); count++) {
		if (al_pa == fp_valid_alpas[count] || al_pa == 0) {
			return (FC_SUCCESS);
		}
	}

	return (FC_FAILURE);
}


static void
fp_printf(fc_port_t *port, int level, fp_mesg_dest_t dest, int fc_errno,
    fc_packet_t *pkt, const char *fmt, ...)
{
	caddr_t		buf;
	va_list		ap;

	switch (level) {
	case CE_NOTE:
		if (FP_CAN_PRINT_WARNINGS(port) == 0) {
			return;
		}
		break;

	case CE_WARN:
		if (FP_CAN_PRINT_FATALS(port) == 0) {
			return;
		}
		break;
	}

	buf = kmem_zalloc(256, KM_NOSLEEP);
	if (buf == NULL) {
		return;
	}

	(void) sprintf(buf, "fp(%d): ", port->fp_instance);

	va_start(ap, fmt);
	(void) vsprintf(buf + strlen(buf), fmt, ap);
	va_end(ap);

	if (fc_errno) {
		char *errmsg;

		(void) fc_ulp_error(fc_errno, &errmsg);
		(void) sprintf(buf + strlen(buf), " FC Error=%s", errmsg);
	} else {
		if (pkt) {
			caddr_t	state, reason, action, expln;

			(void) fc_ulp_pkt_error(pkt, &state, &reason,
			    &action, &expln);

			(void) sprintf(buf + strlen(buf),
			    " state=%s, reason=%s", state, reason);

			if (pkt->pkt_resp_resid) {
				(void) sprintf(buf + strlen(buf),
				    " resp resid=%x\n", pkt->pkt_resp_resid);
			}
		}
	}

	switch (dest) {
	case FP_CONSOLE_ONLY:
		cmn_err(level, "^%s", buf);
		break;

	case FP_LOG_ONLY:
		cmn_err(level, "!%s", buf);
		break;

	default:
		cmn_err(level, "%s", buf);
		break;
	}

	kmem_free(buf, 256);
}
