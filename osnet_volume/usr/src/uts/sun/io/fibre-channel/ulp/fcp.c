/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Fibre Channel SCSI ULP Mapping driver
 */
#pragma ident	"@(#)fcp.c	1.4	99/10/25 SMI"


#if	defined(lint) && !defined(DEBUG)
#define	DEBUG	1
#endif


#include <sys/scsi/scsi.h>
#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/devctl.h>
#include <sys/thread.h>
#include <sys/thread.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/callb.h>
#include <sys/sunndi.h>
#include <sys/console.h>
#ifdef	KSTATS_CODE
#include <sys/kstat.h>
#endif
#include <sys/proc.h>
#include <sys/utsname.h>
#include <sys/scsi/impl/scsi_reset_notify.h>
#include <sys/ndi_impldefs.h>

#include <sys/fibre-channel/fc.h>
#include <sys/fibre-channel/impl/fc_ulpif.h>
#include <sys/fibre-channel/ulp/fcp.h>
#include <sys/fibre-channel/ulp/fcpvar.h>
#include <sys/fibre-channel/ulp/fcp_util.h>

/*
 * Short hand macros
 */
#define	LS_CODE		ls_code.ls_code
#define	MAP_HARD_ADDR	map_hard_addr.hard_addr
#define	MAP_PORT_ID	map_did.port_id
#define	MBZ		ls_code.mbz

char	_depends_on[] = "misc/fctl misc/scsi";

#ifdef	DEBUG
int	ssfcp_debug = SSFCP_DEBUG_DEFAULT_VAL;
int	ssfcp_debug_flag = 0;
#endif

/*
 * this variable can be changed later to cause node creation or
 * deletion on demand -- without this flag being set, drvconfig will
 * have to be run
 */
static int		ssfcp_create_nodes_on_demand = 1;

/* variables global to the fcp module */
struct ssfcp_port	*ssfcp_port_head = NULL;
int			ssfcp_watchdog_init = 0;
int			ssfcp_watchdog_time = 0;

int 		ssfcp_minor_node_created = 0;
/* watchdog timeout (in seconds) */
int			ssfcp_watchdog_timeout = 1;

int			ssfcp_watchdog_tick;
timeout_id_t		ssfcp_watchdog_id;

int			ssfcp_count = 0; /* driver instance count */

/* to wait for the "wwn" property the first time we need it only */
int			ssfcp_first_time = TRUE;

/* to keep track of fact that scsi_hba_init is or is not done */
int			ssfcp_hba_init_done = FALSE;

/* protects above */
kmutex_t			ssfcp_global_mutex;

kcondvar_t		ssfcp_cv;	/* the "init finished" cv */
static int ssfcp_flag; /* Flag for open/close/ioctl calls */
/*
 * used when a DIP is needed for logging and we have none
 *
 * also used for synchronization
 */
dev_info_t		*ssfcp_global_dip = NULL;


/*
 * for soft state
 */
void			*ssfcp_softstate = NULL; /* for soft state */

static int	ssfcp_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int	ssfcp_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static 	void	ssfcp_statec_callback(opaque_t, opaque_t, uint32_t,
			uint32_t, fc_portmap_t *, uint32_t, uint32_t);
static int	ssfcp_els_callback(opaque_t, opaque_t, fc_unsol_buf_t *,
		    uint32_t);
static int	ssfcp_data_callback(opaque_t, opaque_t, fc_unsol_buf_t *,
		    uint32_t);
static int	ssfcp_port_attach(opaque_t, fc_ulp_port_info_t *,
		    ddi_attach_cmd_t, uint32_t);
static int	ssfcp_port_detach(opaque_t, fc_ulp_port_info_t *,
		    ddi_detach_cmd_t);
static int	ssfcp_port_ioctl(opaque_t, opaque_t, dev_t, int,
		    intptr_t, int, cred_t *, int *, uint32_t);
static void 	ssfcp_update_targets(struct ssfcp_port *pptr,
		    fc_portmap_t *dev_list, uint32_t count, uint32_t state);
static int	ssfcp_call_finish_init(struct ssfcp_port *,
			struct ssfcp_tgt *, struct ssfcp_ipkt *);
void		ssfcp_update_state(struct ssfcp_port *, uint32_t);
void		ssfcp_finish_init(struct ssfcp_port *, int);
int		ssfcp_icmd_scsi(struct ssfcp_lun *, struct ssfcp_ipkt *,
		    uchar_t);
void		ssfcp_icmd_free(fc_packet_t *);
void		ssfcp_lfa_update(struct ssfcp_port *, struct scsi_address *,
			uint32_t);
void		ssfcp_update_tgt_state(struct ssfcp_tgt *, int, uint32_t);
void		ssfcp_check_reset_delay(struct ssfcp_port *);
void		ssfcp_abort_all(struct ssfcp_port *, struct ssfcp_tgt *,
			uint32_t);
static int	ssfcp_unsol_prli(struct ssfcp_port *, fc_unsol_buf_t *);
static void	ssfcp_unsol_callback(fc_packet_t *);

static void	ssfcp_unsol_resp_init(fc_packet_t *, fc_unsol_buf_t *,
			uchar_t, uchar_t);

/*
 * forward declarations
 */
static struct ssfcp_port *ssfcp_get_port(opaque_t);

static void	ssfcp_offline_target(struct ssfcp_port *, struct ssfcp_tgt *);
static void	ssfcp_offline_lun(struct ssfcp_lun *);
static void	ssfcp_create_devinfo(struct ssfcp_lun *, uint32_t);

static int	ssfcp_check_reportlun(struct fcp_rsp *, fc_packet_t *);
static void	ssfcp_create_luns(struct ssfcp_tgt *, uint32_t);
static void	ssfcp_reconfigure_luns(void * tgt_handle);
static void	ssfcp_handle_inquiry(fc_packet_t *, struct ssfcp_ipkt *);
static void	ssfcp_handle_reportlun(fc_packet_t *, struct ssfcp_ipkt *);
static struct ssfcp_lun *ssfcp_get_lun(struct ssfcp_tgt *, uchar_t);

static void	ssfcp_device_changed(struct ssfcp_port *, struct ssfcp_tgt *,
		    fc_portmap_t *);

static int	ssfcp_do_disc(struct ssfcp_port *, struct ssfcp_tgt *,
		    struct ssfcp_ipkt *, uchar_t);

static struct ssfcp_tgt *ssfcp_lookup_target(struct ssfcp_port *, uchar_t *);

static void	ssfcp_scsi_callback(fc_packet_t *);
static void	ssfcp_cmd_callback(fc_packet_t *);
static void 	ssfcp_complete_pkt(fc_packet_t *fpkt);
static int	ssfcp_validate_fcp_response(struct fcp_rsp *rsp);
static void	ssfcp_icmd_callback(fc_packet_t *);

static void	ssfcp_handle_devices(struct ssfcp_port *, fc_portmap_t *,
		    uint32_t, int);
static void 	ssfcp_queue_ipkt(struct ssfcp_port *pptr,
		    struct ssfcp_ipkt *icmd);

static struct ssfcp_tgt *ssfcp_alloc_tgt(struct ssfcp_port *pptr,
		    fc_portmap_t *map_entry, int link_cnt);
static struct ssfcp_lun *ssfcp_alloc_lun(struct ssfcp_tgt *ptgt);

static void	ssfcp_log(int, dev_info_t *, const char *, ...);
static int	ssfcp_handle_port_attach(opaque_t, fc_ulp_port_info_t *,
		    uint32_t, int);
static int	ssfcp_handle_port_resume(opaque_t, fc_ulp_port_info_t *,
		    uint32_t, int);
static int	ssfcp_add_cr_pool(struct ssfcp_port *);
static int	ssfcp_cr_alloc(struct ssfcp_port *, struct ssfcp_pkt *,
		    int (*)());
static void	ssfcp_cr_free(struct ssfcp_cr_pool *, struct ssfcp_pkt *);
static void	ssfcp_crpool_free(struct ssfcp_port *);
static int	ssfcp_linkreset(struct ssfcp_port *, struct scsi_address *,
						int sleep);
static int	ssfcp_reset(struct scsi_address *, int);
static struct ssfcp_port *ssfcp_soft_state_unlink(struct ssfcp_port *);
static struct ssfcp_lun *ssfcp_get_lun_from_dip(struct ssfcp_port *,
		dev_info_t *);
static int	ssfcp_pass_to_hp(struct ssfcp_port *, struct ssfcp_lun *, int);
static void 	ssfcp_queue_pkt(struct ssfcp_port *, struct ssfcp_pkt *);
static int	ssfcp_transport(opaque_t, fc_packet_t *, int);

static void	ssfcp_free_targets(struct ssfcp_port *);
#ifndef	lint
static void ssfcp_dealloc_tgt(struct ssfcp_tgt *ptgt);
static void ssfcp_dealloc_lun(struct ssfcp_lun *plun);
#endif /* lint */



ddi_eventcookie_t	ssfcp_insert_eid;
ddi_eventcookie_t	ssfcp_remove_eid;


/*
 * local static variables/defs
 */

static ndi_event_definition_t   ssfcp_event_defs[] = {
	{ SSFCP_EVENT_TAG_INSERT, FCAL_INSERT_EVENT, EPL_KERNEL },
	{ SSFCP_EVENT_TAG_REMOVE, FCAL_REMOVE_EVENT, EPL_INTERRUPT }
};

#define	SSFCP_N_NDI_EVENTS \
	(sizeof (ssfcp_event_defs) / sizeof (ndi_event_definition_t))



/*
 * forward routine declarations
 */

static void	ssfcp_cp_pinfo(struct ssfcp_port *, fc_ulp_port_info_t *);
static int	ssfcp_kmem_cache_constructor(void *, void *, int);
static void	ssfcp_kmem_cache_destructor(void *, void *);
static int	ssfcp_scsi_tgt_init(dev_info_t *, dev_info_t *,
		    scsi_hba_tran_t *, struct scsi_device *);
static void	ssfcp_scsi_tgt_free(dev_info_t *, dev_info_t *,
		    scsi_hba_tran_t *, struct scsi_device *);
static int	ssfcp_getcap(struct scsi_address *, char *, int);
static int	ssfcp_setcap(struct scsi_address *, char *, int, int);
static int	ssfcp_abort(struct scsi_address *, struct scsi_pkt *);
static int	ssfcp_start(struct scsi_address *, struct scsi_pkt *);
static struct scsi_pkt *ssfcp_scsi_init_pkt(struct scsi_address *,
		    struct scsi_pkt *, struct buf *, int, int, int, int,
		    int (*)(), caddr_t);
static void	ssfcp_scsi_destroy_pkt(struct scsi_address *,
		    struct scsi_pkt *);
static int	ssfcp_pkt_alloc_extern(struct ssfcp_port *, struct ssfcp_pkt *,
		    int, int, int);
static void	ssfcp_pkt_destroy_extern(struct ssfcp_port *,
		    struct ssfcp_pkt *);
static void	ssfcp_scsi_dmafree(struct scsi_address *, struct scsi_pkt *);
static void	ssfcp_scsi_sync_pkt(struct scsi_address *, struct scsi_pkt *);
static int	ssfcp_scsi_reset_notify(struct scsi_address *, int,
		    void (*)(caddr_t), caddr_t);
static int	ssfcp_bus_get_eventcookie(dev_info_t *, dev_info_t *, char *,
		    ddi_eventcookie_t *, ddi_plevel_t *,
		    ddi_iblock_cookie_t *);
static int	ssfcp_bus_add_eventcall(dev_info_t *, dev_info_t *,
		    ddi_eventcookie_t, int (*)(), void *);
static int	ssfcp_bus_remove_eventcall(dev_info_t *, dev_info_t *,
		    ddi_eventcookie_t);
static int	ssfcp_bus_post_event(dev_info_t *, dev_info_t *,
		    ddi_eventcookie_t, void *);
static int	ssfcp_scsi_get_name(struct scsi_device *, char *, int);
static int	ssfcp_scsi_get_bus_addr(struct scsi_device *, char *, int);
static void	ssfcp_hp_daemon(void *);
static void	ssfcp_watch(void *);
static struct ssfcp_lun *ssfcp_lookup_lun(struct ssfcp_port *, uchar_t *, int);
static int	ssfcp_prepare_pkt(struct ssfcp_port *, struct ssfcp_pkt *,
		    struct ssfcp_lun *);
static void	ssfcp_fill_ids(struct ssfcp_port *, struct ssfcp_pkt *,
		    struct ssfcp_lun *);
static int	ssfcp_dopoll(struct ssfcp_port *, struct ssfcp_pkt *);
static struct ssfcp_port *ssfcp_dip2port(dev_info_t *);
static void 	ssfcp_retransport_cmd(struct ssfcp_port *pptr,
		struct ssfcp_pkt *cmd);
static int 	ssfcp_reset_target(struct scsi_address *ap);
static void 	ssfcp_fail_cmd(struct ssfcp_pkt *cmd, uchar_t reason,
		uint_t statistics);

static struct ssfcp_ipkt *ssfcp_icmd_alloc(struct ssfcp_port *,
		struct ssfcp_tgt *, uint32_t);
static int ssfcp_open(dev_t *, int, int, cred_t *);
static int ssfcp_close(dev_t, int, int, cred_t *);
static int ssfcp_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static char		ssfcp_name[] = "FCP v1.4";

static struct cb_ops ssfcp_cb_ops = {
	ssfcp_open,			/* open */
	ssfcp_close,			/* close */
	nodev,				/* strategy */
	nodev,				/* print */
	nodev,				/* dump */
	nodev,				/* read */
	nodev,				/* write */
	ssfcp_ioctl,			/* ioctl */
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
static struct dev_ops ssfcp_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	nulldev,		/* identify */
	nulldev,		/* probe */
	ssfcp_attach,		/* attach and detach are mandatory */
	ssfcp_detach,
	nodev,			/* reset */
	&ssfcp_cb_ops,		/* cb_ops */
	NULL,			/* bus_ops */
	NULL,			/* power */
};

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"FCP SCSI Pseudo Driver v1.3",
	&ssfcp_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};


static fc_ulp_modinfo_t ssfcp_modinfo = {
	&ssfcp_modinfo,				/* ulp_handle */
	FCTL_ULP_MODREV_1,			/* ulp_rev */
	FC4_SCSI_FCP,				/* ulp_type */
	ssfcp_name,				/* ulp_name */
	FCP_STATEC_MASK,			/* ulp_statec_mask */
	ssfcp_port_attach,			/* ulp_port_attach */
	ssfcp_port_detach,			/* ulp_port_detach */
	ssfcp_port_ioctl,			/* ulp_port_ioctl */
	ssfcp_els_callback,			/* ulp_els_callback */
	ssfcp_data_callback,			/* ulp_data_callback */
	ssfcp_statec_callback			/* ulp_statec_callback */
};

/*
 * a list of all possible child nodes by name (the name length
 * is also stored for ease of scanning later)
 *
 * XXX: NOTE: this should be built dynamically from properties rather than
 * being statically compiled in
 */
static struct {
	char	*name;
	int	name_len;
	int	dev_type;
} ssfcp_children[] = {
	{"ssd", 3, DTYPE_DIRECT},
	{"st", 2, DTYPE_SEQUENTIAL},
	{"st", 2, DTYPE_CHANGER},
	{"ses", 3, DTYPE_ESI }
};
static const int ssfcp_num_children = (sizeof (ssfcp_children) /
				sizeof (ssfcp_children[0]));
/*
 * for report lun processing
 */

#define	SSFCP_LUN_ADDRESSING		0x02
#define	SSFCP_PD_ADDRESSING		0x00
#define	SSFCP_VOLUME_ADDRESSING		0x01

/*
 * this is used to dummy up a report lun response for cases
 * where the target doesn't support it
 */
static uchar_t ssfcp_dummy_lun[] = {
	0x00,		/* MSB length (length = no of luns * 8) */
	0x00,
	0x00,
	0x08,		/* LSB length */
	0x00,		/* MSB reserved */
	0x00,
	0x00,
	0x00,		/* LSB reserved */
	SSFCP_PD_ADDRESSING,
	0x00,		/* LUN is ZERO at the first level */
	0x00,
	0x00,		/* second level is zero */
	0x00,
	0x00,		/* third level is zero */
	0x00,
	0x00		/* fourth level is zero */
};

/* property strings */
const char	*ssfcp_node_wwn_prop = "node-wwn";
const char	*ssfcp_port_wwn_prop = "port-wwn";
const char	*ssfcp_link_cnt_prop = "link-count";
const char	*ssfcp_target_prop = "target";
const char	*ssfcp_lun_prop = "lun";

#define	NODE_WWN_PROP	(char *)ssfcp_node_wwn_prop
#define	PORT_WWN_PROP	(char *)ssfcp_port_wwn_prop
#define	LINK_CNT_PROP	(char *)ssfcp_link_cnt_prop
#define	TARGET_PROP	(char *)ssfcp_target_prop
#define	LUN_PROP	(char *)ssfcp_lun_prop

static uchar_t ssfcp_alpa_to_switch[] = {
	0x00, 0x7d, 0x7c, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x7a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0x78, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x77, 0x76, 0x00, 0x00, 0x75, 0x00, 0x74,
	0x73, 0x72, 0x00, 0x00, 0x00, 0x71, 0x00, 0x70, 0x6f, 0x6e,
	0x00, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x00, 0x00, 0x67,
	0x66, 0x65, 0x64, 0x63, 0x62, 0x00, 0x00, 0x61, 0x60, 0x00,
	0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x5d,
	0x5c, 0x5b, 0x00, 0x5a, 0x59, 0x58, 0x57, 0x56, 0x55, 0x00,
	0x00, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x00, 0x00, 0x4e,
	0x4d, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4b,
	0x00, 0x4a, 0x49, 0x48, 0x00, 0x47, 0x46, 0x45, 0x44, 0x43,
	0x42, 0x00, 0x00, 0x41, 0x40, 0x3f, 0x3e, 0x3d, 0x3c, 0x00,
	0x00, 0x3b, 0x3a, 0x00, 0x39, 0x00, 0x00, 0x00, 0x38, 0x37,
	0x36, 0x00, 0x35, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x33, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x31, 0x30, 0x00, 0x00, 0x2f, 0x00, 0x2e, 0x2d, 0x2c,
	0x00, 0x00, 0x00, 0x2b, 0x00, 0x2a, 0x29, 0x28, 0x00, 0x27,
	0x26, 0x25, 0x24, 0x23, 0x22, 0x00, 0x00, 0x21, 0x20, 0x1f,
	0x1e, 0x1d, 0x1c, 0x00, 0x00, 0x1b, 0x1a, 0x00, 0x19, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x17, 0x16, 0x15,
	0x00, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x00, 0x00, 0x0e,
	0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x00, 0x00, 0x08, 0x07, 0x00,
	0x06, 0x00, 0x00, 0x00, 0x05, 0x04, 0x03, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

extern pri_t	minclsyspri;
extern char *sense_keys[];

/*
 * The _init(9e) return value should be that of mod_install(9f). Under
 * some circumstances, a failure may not be related mod_install(9f) and
 * one would then require a return value to indicate the failure. Looking
 * at mod_install(9f), it is expected to return 0 for success and non-zero
 * for failure. mod_install(9f) for device drivers, further goes down the
 * calling chain and ends up in ddi_installdrv(), whose return values are
 * DDI_SUCCESS and DDI_FAILURE - There are also other functions in the
 * calling chain of mod_install(9f) which return values like EINVAL and
 * in some even return -1.
 *
 * So if the above paragraph hasn't given enough indication as to what type
 * of implementation that is, it should be made clear that it is not far from
 * being called a very loose usage of return codes. So without much ado,
 * return DDI_FAILURE, for failures not related to mod_install.
 */
int
_init(void)
{
	int rval;

	/*
	 * Allocate soft state and prepare to do ddi_soft_state_zalloc()
	 * before registering with the transport first.
	 */
	if (ddi_soft_state_init(&ssfcp_softstate,
	    sizeof (struct ssfcp_port), SSFCP_INIT_ITEMS) != 0) {
		return (DDI_FAILURE);
	}

	if ((rval = fc_ulp_add(&ssfcp_modinfo)) != FC_SUCCESS) {
		cmn_err(CE_WARN, "fcp: fc_ulp_add failed");
		ddi_soft_state_fini(&ssfcp_softstate);
		return (DDI_FAILURE);
	}

	mutex_init(&ssfcp_global_mutex, NULL, MUTEX_DRIVER, NULL);

	if ((rval = mod_install(&modlinkage)) != 0) {
		(void) fc_ulp_remove(&ssfcp_modinfo);
		mutex_destroy(&ssfcp_global_mutex);
		ddi_soft_state_fini(&ssfcp_softstate);
	}
	return (rval);
}


/*
 * the system is done with us as a driver, so clean up
 */
int
_fini(void)
{
	int rval;

	/*
	 * don't start cleaning up until we know that the module remove
	 * has worked  -- if this works, then we know that each instance
	 * has successfully been DDI_DETACHed
	 */
	if ((rval = mod_remove(&modlinkage)) != 0) {
		return (rval);
	}

	(void) fc_ulp_remove(&ssfcp_modinfo);
	/* ulp removed from the transports knowledge */

	ddi_soft_state_fini(&ssfcp_softstate);
	mutex_destroy(&ssfcp_global_mutex);
	return (rval);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * called for ioctls on the transport's devctl interface, and the transport
 * has passed it to us
 *
 * this will only be called for device control ioctls (i.e. hotplugging stuff)
 *
 * return FC_SUCCESS if we decide to claim the ioctl,
 * else return FC_UNCLAIMED
 *
 * *rval is set iff we decide to claim the ioctl
 */
/*ARGSUSED*/
static int
ssfcp_port_ioctl(opaque_t ulph, opaque_t port_handle, dev_t dev, int cmd,
    intptr_t data, int mode, cred_t *credp, int *rval, uint32_t claimed)
{
	int			retval = FC_UNCLAIMED;	/* return value */
	struct ssfcp_port	*pptr = NULL;		/* our soft state */
	struct devctl_iocdata	*dcp = NULL;		/* for devctl */
	dev_info_t		*cdip;
	char			*ndi_nm;		/* NDI name */
	char			*ndi_addr;		/* NDI addr */
	int			i;


	ASSERT(rval != NULL);

	SSFCP_DEBUG(2, (CE_NOTE, NULL,
	    "ssfcp_port_ioctl(cmd=0x%x, claimed=%d)\n", cmd, claimed));

	/* if already claimed then forget it */
	if (claimed) {
		/*
		 * for now, if this ioctl has already been claimed, then
		 * we just ignore it
		 */
		goto dun;			/* skip it -- no error */
	}

	/* get our port info */
	if ((pptr = ssfcp_get_port(port_handle)) == NULL) {
		ssfcp_log(CE_WARN, NULL,
		    "!fcp:Invalid port handle handle in ioctl");
		*rval = ENXIO;
		goto dun;
	}

	switch (cmd) {
	case DEVCTL_BUS_GETSTATE:
	case DEVCTL_BUS_QUIESCE:
	case DEVCTL_BUS_UNQUIESCE:
	case DEVCTL_BUS_RESET:
	case DEVCTL_BUS_RESETALL:
		if (ndi_dc_allochdl((void *)data, &dcp) != NDI_SUCCESS) {
			/* this shouldn't happen */
			goto dun;
		}

		break;

	case DEVCTL_DEVICE_GETSTATE:
	case DEVCTL_DEVICE_OFFLINE:
	case DEVCTL_DEVICE_ONLINE:
	case DEVCTL_DEVICE_RESET:
		if (ndi_dc_allochdl((void *)data, &dcp) != NDI_SUCCESS) {
			/* this shouldn't happen */
			goto dun;
		}

		ASSERT(dcp != NULL);

		/* ensure we have a name and address */
		if (((ndi_nm = ndi_dc_getname(dcp)) == NULL) ||
		    ((ndi_addr = ndi_dc_getaddr(dcp)) == NULL)) {
			SSFCP_DEBUG(2, (CE_WARN, NULL,
			    "ioctl: can't get name (%s) or addr (%s)\n",
			    ndi_nm ? ndi_nm : "<null ptr>",
			    ndi_addr ? ndi_addr : "<null ptr>"));
			goto dun;
		}


		/* get our child's DIP */
		ASSERT(pptr != NULL);
		if ((cdip = ndi_devi_find(pptr->ssfcpp_dip, ndi_nm,
		    ndi_addr)) == NULL) {
			*rval = ENXIO;
			goto dun;
		}

		/* see which child (if any) this ioctl is for */
		for (i = 0; i < ssfcp_num_children; i++) {
			if (strncmp(ndi_nm, ssfcp_children[i].name,
			    ssfcp_children[i].name_len) == 0) {
				/* we found our child */
				break;
			}
		}
		if (i == ssfcp_num_children) {
			/* no match found -- this ioctl is not ours */
			goto dun;
		}

		break;
	}

	/* this ioctl is ours -- process it */

	retval = FC_SUCCESS;		/* just means we claim the ioctl */

	SSFCP_DEBUG(5, (CE_NOTE, NULL, "ioctl: claiming this one\n"));

	/* handle ioctls now */
	switch (cmd) {
	/*
	 * device control ioctls
	 */
	case DEVCTL_DEVICE_GETSTATE:
		ASSERT(cdip != NULL);
		ASSERT(dcp != NULL);
		if (ndi_dc_return_dev_state(cdip, dcp) != NDI_SUCCESS) {
			*rval = EFAULT;
			goto dun;
		}
		break;

	case DEVCTL_DEVICE_OFFLINE:
		ASSERT(cdip != NULL);
		if ((i = ndi_devi_offline(cdip, 0)) == NDI_BUSY) {
			*rval = EBUSY;
			goto dun;
		}
		if (i != NDI_SUCCESS) {
			*rval = EIO;
			goto dun;
		}
		break;

	case DEVCTL_DEVICE_ONLINE:
		ASSERT(cdip != NULL);
		if (ndi_devi_online(cdip, 0) != NDI_SUCCESS) {
			*rval = EIO;
			goto dun;
		}
		break;

	case DEVCTL_DEVICE_RESET: {
		struct ssfcp_lun	*plun;
		struct scsi_address	ap;

		ASSERT(cdip != NULL);
		ASSERT(pptr != NULL);
		mutex_enter(&pptr->ssfcp_mutex);
		if ((plun = ssfcp_get_lun_from_dip(pptr, cdip)) == NULL) {
			mutex_exit(&pptr->ssfcp_mutex);
			*rval = ENXIO;
			goto dun;
		}
		mutex_exit(&pptr->ssfcp_mutex);

		mutex_enter(&plun->lun_tgt->tgt_mutex);
		if (!(plun->lun_state & SSFCP_LUN_INIT)) {
			mutex_exit(&plun->lun_tgt->tgt_mutex);
			*rval = ENXIO;
			goto dun;
		}
		ap.a_hba_tran = plun->lun_tran;
		ASSERT(pptr->ssfcp_tran != NULL);
		mutex_exit(&plun->lun_tgt->tgt_mutex);

		/*
		 * set up ap so that ssfcp_reset can figure out
		 * which target to reset
		 */
		if (ssfcp_reset(&ap, RESET_TARGET) == FALSE) {
			*rval = EIO;
			goto dun;
		}
		break;
	}

	case DEVCTL_BUS_GETSTATE:
		ASSERT(dcp != NULL);
		ASSERT(pptr != NULL);
		ASSERT(pptr->ssfcpp_dip != NULL);
		if (ndi_dc_return_bus_state(pptr->ssfcpp_dip, dcp) !=
		    NDI_SUCCESS) {
			*rval = EFAULT;
			goto dun;
		}
		break;

	case DEVCTL_BUS_QUIESCE:
	case DEVCTL_BUS_UNQUIESCE:
		*rval = ENOTSUP;
		goto dun;

	case DEVCTL_BUS_RESET:
	case DEVCTL_BUS_RESETALL:
		ASSERT(pptr != NULL);
		(void) ssfcp_linkreset(pptr, NULL,  KM_SLEEP);
		break;

	default:
		*rval = ENOTTY;
		goto dun;
	}

	/* success */
	*rval = 0;

	/* all done -- clean up and return */
dun:
	if (dcp != NULL) {
		ndi_dc_freehdl(dcp);
	}

	return (retval);
}


/*
 * attach the module
 */
static int
ssfcp_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int rval = DDI_FAILURE;


	SSFCP_DEBUG(4, (CE_NOTE, NULL, "fcp module attach: cmd=0x%x\n", cmd));

	switch (cmd) {
	case DDI_ATTACH:
#ifdef	DEBUG
		if (SSFCP_TEST_FLAG(SSFCP_MODUNLOAD_DEBUG)) {
			break;
		}
#endif
		mutex_enter(&ssfcp_global_mutex);
		ssfcp_global_dip = devi;
		mutex_exit(&ssfcp_global_mutex);

		if (ddi_create_minor_node(ssfcp_global_dip, "fcp", S_IFCHR,
		    NULL, DDI_PSEUDO, GLOBAL_DEV) == DDI_FAILURE) {
			cmn_err(CE_WARN, "FCP: Cannot create global"
			    " minor node\n");

			mutex_enter(&ssfcp_global_mutex);
			ssfcp_global_dip = NULL;
			mutex_exit(&ssfcp_global_mutex);

			rval = DDI_FAILURE;
		} else {
			ddi_report_dev(ssfcp_global_dip);
			rval = DDI_SUCCESS;
		}
		break;

	case DDI_RESUME:
	case DDI_PM_RESUME:
		SSFCP_DEBUG(4, (CE_NOTE, NULL,
		    "fcp: DDI_[PM_]RESUME NOT YET IMPLEMENTED\n"));
		break;			/* XXX: NOT YET IMPLEMENTED */

	default:
		/* shouldn't happen */
		SSFCP_DEBUG(4, (CE_WARN, NULL, "fcp module: unknown DDI cmd"));
		break;
	}
	return (rval);
}


/*ARGSUSED*/
static int
ssfcp_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int	res = DDI_FAILURE;		/* default result */

	SSFCP_DEBUG(2, (CE_NOTE, NULL,  "module detach: cmd=0x%x\n", cmd));

	switch (cmd) {
	case DDI_DETACH:
		/*
		 * Add code to check if there are active ports/threads.
		 * If there are any, we will fail, else we will succeed
		 * (there should not be much to clean up)
		 */
		mutex_enter(&ssfcp_global_mutex);
		SSFCP_DEBUG(2, (CE_NOTE, NULL,  "port_head=%p\n",
		    (void *) ssfcp_port_head));
		if (ssfcp_port_head != NULL) {
			mutex_exit(&ssfcp_global_mutex);
			break;
		}

		ddi_remove_minor_node(ssfcp_global_dip, NULL);
		ssfcp_global_dip = NULL;
		mutex_exit(&ssfcp_global_mutex);

		res = DDI_SUCCESS;		/* success */
		break;

	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		/*
		 * suspend our driver -- this should only succeed if
		 * all target/luns are already suspended
		 */
		/*
		 * XXX: add code to suspend the threads
		 *
		 * for now, just fail this. FIXIT
		 */
		break;

	default:
		break;
	}
	SSFCP_DEBUG(2, (CE_NOTE, NULL,  "module detach returning %d\n", res));

	return (res);
}


/* ARGSUSED */
static int
ssfcp_open(dev_t *devp, int flag, int otype, cred_t *credp)
{
	if (otype != OTYP_CHR) {
		return (EINVAL);
	}

	/*
	 * Allow only root to talk;
	 */
	if (drv_priv(credp)) {
		return (EPERM);
	}
	mutex_enter(&ssfcp_global_mutex);
	if (ssfcp_flag & FP_EXCL) {
		/*
		 * We really don't need this right now.
		 * Just put it there for future.
		 */
		mutex_exit(&ssfcp_global_mutex);
		return (EBUSY);
	}
	if (flag & FEXCL) {
		if (ssfcp_flag & FP_OPEN) {
			mutex_exit(&ssfcp_global_mutex);
			return (EBUSY);
		} else
			ssfcp_flag |= (FP_OPEN | FP_EXCL);
	} else {
		ssfcp_flag |= FP_OPEN;
	}
	mutex_exit(&ssfcp_global_mutex);
	return (0);
}


/* ARGSUSED */
static int
ssfcp_close(dev_t dev, int flag, int otype, cred_t *credp)
{

	if (otype != OTYP_CHR) {
		return (EINVAL);
	}


	mutex_enter(&ssfcp_global_mutex);
	if ((ssfcp_flag & FP_OPEN) == 0) {
		mutex_exit(&ssfcp_global_mutex);
		return (ENODEV);
	}
	ssfcp_flag = FP_IDLE;
	mutex_exit(&ssfcp_global_mutex);
	return (0);
}

/* ARGSUSED */
static int
ssfcp_ioctl(dev_t dev, int cmd, intptr_t data,
	int mode, cred_t *credp, int *rval)
{
	int 		i, error;
	uint32_t	link_cnt;
	struct fcp_ioctl	fioctl;
	struct ssfcp_port 	*pptr;
	struct 	device_data *dev_data;
	struct ssfcp_tgt *ptgt = NULL;
	struct ssfcp_lun *plun = NULL;
	la_wwn_t	*wwn_ptr = NULL;

	mutex_enter(&ssfcp_global_mutex);
	if ((ssfcp_flag & FP_OPEN) == 0) {
		mutex_exit(&ssfcp_global_mutex);
		return (ENXIO);
	}
	mutex_exit(&ssfcp_global_mutex);

#ifdef	_MULTI_DATAMODEL
	switch (ddi_model_convert_from(mode & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		struct fcp32_ioctl f32_ioctl;

		if (ddi_copyin((void *)data, (void *)&f32_ioctl,
		    sizeof (struct fcp32_ioctl), mode)) {
			return (EFAULT);
		}
		fioctl.fp_minor = f32_ioctl.fp_minor;
		fioctl.listlen = f32_ioctl.listlen;
		fioctl.list = (caddr_t)f32_ioctl.list;
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyin((void *)data, (void *)&fioctl,
		    sizeof (struct fcp_ioctl), mode)) {
			return (EFAULT);
		}
		break;
	}
#else	/* _MULTI_DATAMODEL */
	if (ddi_copyin((void *)data, (void *)&fioctl,
	sizeof (struct fcp_ioctl), mode)) {
		return (EFAULT);
	}
#endif	/* _MULTI_DATAMODEL */

	/*
	 * Right now we can assume that the minor number matches with
	 * this instance of fp. If this changes we will need to
	 * revisit this logic.
	 */
	mutex_enter(&ssfcp_global_mutex);
	pptr = ssfcp_port_head;
	while (pptr) {
		if (pptr->ssfcpp_instance == (uint32_t)fioctl.fp_minor)
			break;
		else
			pptr = pptr->ssfcp_next;
	}
	mutex_exit(&ssfcp_global_mutex);
	if (pptr == NULL)
		return (ENXIO);

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_state & (SSFCP_STATE_INIT | SSFCP_STATE_OFFLINE)) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (ENXIO);
	}
	if (pptr->ssfcp_state & SSFCP_STATE_ONLINING) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (EAGAIN);
	}
	if ((dev_data = kmem_zalloc(
		(sizeof (struct device_data)) * fioctl.listlen, KM_NOSLEEP))
								== NULL) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (ENOMEM);
	}
	if (ddi_copyin(fioctl.list, dev_data,
		(sizeof (struct device_data)) * fioctl.listlen, mode)) {
		kmem_free(dev_data,
			(sizeof (struct device_data)) * fioctl.listlen);
		mutex_exit(&pptr->ssfcp_mutex);
		return (EFAULT);
	}
	link_cnt = pptr->ssfcp_link_cnt;

	for (i = 0; (i < fioctl.listlen) && (link_cnt == pptr->ssfcp_link_cnt);
	    i++) {
		wwn_ptr = (la_wwn_t *)&(dev_data[i].dev_pwwn);
		dev_data[i].dev0_type = 0x1f;
		dev_data[i].dev_status = ENXIO;

		if ((ptgt = ssfcp_lookup_target(pptr, (uchar_t *)wwn_ptr)) ==
		    NULL) {
			mutex_exit(&pptr->ssfcp_mutex);
			if (fc_ulp_get_port_device(pptr->ssfcpp_handle, wwn_ptr,
			    &error, 0) == NULL) {
				dev_data[i].dev_status = ENODEV;
				mutex_enter(&pptr->ssfcp_mutex);
				continue;
			} else {
				dev_data[i].dev_status = EAGAIN;
				mutex_enter(&pptr->ssfcp_mutex);
				continue;
			}
		} else {
			mutex_enter(&ptgt->tgt_mutex);
			if (ptgt->tgt_state & SSFCP_TGT_OFFLINE) {
				dev_data[i].dev_status = ENXIO;
				mutex_exit(&ptgt->tgt_mutex);
				continue;
			}

			switch (cmd) {
			case FCP_TGT_INQUIRY:
				/*
				 * The reason we give device type of
				 * lun 0 only even though in some
				 * cases(like maxstrat) lun 0 device
				 * type may be 0x3f(invalid) is that
				 * for bridge boxes target will appear
				 * as luns and the first lun could be
				 * a device that utility may not care
				 * about (like a tape device).
				 */
				dev_data[i].dev_lun_cnt = ptgt->tgt_lun_cnt;
				dev_data[i].dev_status = 0;
				if ((plun = ssfcp_get_lun(ptgt, 0)) == NULL) {
					dev_data[i].dev0_type = DTYPE_UNKNOWN;
				} else {
					dev_data[i].dev0_type = plun->lun_type;
				}
				break;

			case FCP_TGT_CREATE:
				if (!FC_TOP_EXTERNAL(pptr->ssfcpp_top)) {
					dev_data[i].dev_status = EINVAL;
					break;
				}

				/*
				 * There is a problem when a user requests
				 * a target to be onlined which is already
				 * onlined. Right now there is no check for
				 * this in the code here but things will work
				 * as the dip != NULL and LUN_INIT flag will
				 * be set.
				 */
				if (!(ptgt->tgt_state &
					SSFCP_TGT_ON_DEMAND)) {
					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					"!TGT_CREATE but no TGT_ON_DEMAND\n");
					dev_data[i].dev_status = ENXIO;
				} else {
					ssfcp_create_luns(ptgt, link_cnt);
					ptgt->tgt_state &= ~(SSFCP_TGT_BUSY
						| SSFCP_TGT_ON_DEMAND);
					dev_data[i].dev_status = 0;
				}
				break;

			case FCP_TGT_DELETE:
				if (!FC_TOP_EXTERNAL(pptr->ssfcpp_top)) {
					dev_data[i].dev_status = EINVAL;
				} else if (!(ptgt->tgt_state &
				    SSFCP_TGT_OFFLINE)) {
					ssfcp_offline_target(pptr, ptgt);
					dev_data[i].dev_status = 0;
				}
				break;

			default:
				ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
				"!Invalid ioctl opcode = 0x%x\n", cmd);
			}
			mutex_exit(&ptgt->tgt_mutex);
		}
	}
	mutex_exit(&pptr->ssfcp_mutex);

	if (ddi_copyout(dev_data, fioctl.list,
		(sizeof (struct device_data)) * fioctl.listlen, mode)) {
		kmem_free(dev_data,
			(sizeof (struct device_data)) * fioctl.listlen);
		return (EFAULT);
	}
	kmem_free(dev_data, (sizeof (struct device_data)) * fioctl.listlen);

#ifdef	_MULTI_DATAMODEL
	switch (ddi_model_convert_from(mode & FMODELS)) {
	case DDI_MODEL_ILP32:
	{
		struct fcp32_ioctl f32_ioctl;

		f32_ioctl.fp_minor = fioctl.fp_minor;
		f32_ioctl.listlen = fioctl.listlen;
		f32_ioctl.list = (caddr32_t)fioctl.list;
		if (ddi_copyout((void *)&f32_ioctl, (void *)data,
		    sizeof (struct fcp32_ioctl), mode)) {
			return (EFAULT);
		}
		break;
	}
	case DDI_MODEL_NONE:
		if (ddi_copyout((void *)&fioctl, (void *)data,
		    sizeof (struct fcp_ioctl), mode)) {
			return (EFAULT);
		}
		break;
	}
#else	/* _MULTI_DATAMODEL */

	if (ddi_copyout((void *)&fioctl, (void *)data,
		sizeof (struct fcp_ioctl), mode)) {
		return (EFAULT);
	}
#endif	/* _MULTI_DATAMODEL */

	return (0);
}



/*
 * called by the transport framework because it wants to resume a suspended
 * port or attach a new one
 */
/*ARGSUSED*/
static int
ssfcp_port_attach(opaque_t ulph, fc_ulp_port_info_t *pinfo,
    ddi_attach_cmd_t cmd,  uint32_t s_id)
{
	int	instance;
	int	res = FC_FAILURE; /* default result */

	ASSERT(pinfo != NULL);

	instance = ddi_get_instance(pinfo->port_dip);

	switch (cmd) {
	case DDI_ATTACH:
		/*
		 * this port instance attaching for the first time (or after
		 * being detached before)
		 */
		if (ssfcp_handle_port_attach(ulph, pinfo, s_id,
		    instance) == DDI_SUCCESS) {
			res = FC_SUCCESS;
		}
		break;

	case DDI_RESUME:
	case DDI_PM_RESUME:
		/*
		 * this port instance was attached and the suspended and
		 * will now be resumed
		 */
		if (ssfcp_handle_port_resume(ulph, pinfo, s_id,
		    instance) == DDI_SUCCESS) {
			res = FC_SUCCESS;
		}
		break;

	default:
		/* shouldn't happen */
		SSFCP_DEBUG(2, (CE_NOTE, pinfo->port_dip,
		    "port_attach: unknown cmdcommand: %d\n", cmd));
		break;
	}

	/* return result */
	SSFCP_DEBUG(4, (CE_NOTE, NULL, "ssfcp_port_attach returning %d\n",
	    res));

	return (res);
}



/*
 * detach or suspend this port instance
 *
 * acquires and releases the global mutex
 *
 * acquires and releases the mutex for this port
 *
 * acquires and releases the hotplug mutex for this port
 */
/*ARGSUSED*/
static int
ssfcp_port_detach(opaque_t ulph, fc_ulp_port_info_t *info,
    ddi_detach_cmd_t cmd)
{
	int			res = FC_SUCCESS; /* default result */
	struct ssfcp_port	*pptr;
	int			instance;

	instance = ddi_get_instance(info->port_dip);
	pptr = ddi_get_soft_state(ssfcp_softstate, instance);

	switch (cmd) {
	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		/*
		 * suspend this instance of the port driver for later
		 * resumption
		 */
		SSFCP_DEBUG(2, (CE_NOTE, info->port_dip,
		    "port suspend called for port %d\n", instance));

		mutex_enter(&pptr->ssfcp_mutex);

		mutex_enter(&pptr->ssfcp_hp_mutex);
		if (pptr->ssfcp_hp_nele) {
			mutex_exit(&pptr->ssfcp_hp_mutex);
			mutex_exit(&pptr->ssfcp_mutex);
			res = FC_FAILURE;
			break;
		}
		mutex_exit(&pptr->ssfcp_hp_mutex);

		pptr->ssfcp_link_cnt++;
		pptr->ssfcp_state |= SSFCP_STATE_SUSPENDED;
		ssfcp_update_state(pptr, (SSFCP_LUN_BUSY | SSFCP_LUN_MARK));

		/*
		 * Let the discovery in progress complete
		 */
		while (pptr->ssfcp_ipkt_cnt ||
		    (pptr->ssfcp_state & SSFCP_STATE_IN_WATCHDOG)) {
			mutex_exit(&pptr->ssfcp_mutex);
			delay(drv_usectohz(1000000));
			mutex_enter(&pptr->ssfcp_mutex);
		}

		pptr->ssfcp_state |= SSFCP_STATE_OFFLINE;
		pptr->ssfcp_tmp_cnt = 0;
		mutex_exit(&pptr->ssfcp_mutex);

		/* kill watch dog timer if we're the last */
		mutex_enter(&ssfcp_global_mutex);
		if (--ssfcp_watchdog_init == 0) {
			timeout_id_t	tid = ssfcp_watchdog_id;

			mutex_exit(&ssfcp_global_mutex);
			(void) untimeout(tid);
		} else {
			mutex_exit(&ssfcp_global_mutex);
		}

		break;				/* failure */

	case DDI_DETACH:
		/*
		 * detach this instance of the port, releasing all
		 * resources
		 */
		SSFCP_DEBUG(2, (CE_NOTE, info->port_dip,
		    "port detach called for port %d\n", instance));

		/*
		 * Let the discovery in progress complete
		 */
		mutex_enter(&pptr->ssfcp_mutex);

		mutex_enter(&pptr->ssfcp_hp_mutex);
		if (pptr->ssfcp_hp_nele) {
			mutex_exit(&pptr->ssfcp_hp_mutex);
			mutex_exit(&pptr->ssfcp_mutex);
			res = FC_FAILURE;
			break;
		}
		mutex_exit(&pptr->ssfcp_hp_mutex);

		pptr->ssfcp_state |= SSFCP_STATE_DETACHING;
		ssfcp_update_state(pptr, (SSFCP_LUN_BUSY | SSFCP_LUN_MARK));
		pptr->ssfcp_link_cnt++;

		while (pptr->ssfcp_ipkt_cnt ||
		    (pptr->ssfcp_state & SSFCP_STATE_IN_WATCHDOG)) {
			mutex_exit(&pptr->ssfcp_mutex);
			delay(drv_usectohz(1000000));
			mutex_enter(&pptr->ssfcp_mutex);
		}
		mutex_exit(&pptr->ssfcp_mutex);

		/* kill watch dog timer if we're the last */
		mutex_enter(&ssfcp_global_mutex);
		if (--ssfcp_watchdog_init == 0) {
			timeout_id_t	tid = ssfcp_watchdog_id;

			mutex_exit(&ssfcp_global_mutex);
			(void) untimeout(tid);
		} else {
			mutex_exit(&ssfcp_global_mutex);
		}

		if (ssfcp_soft_state_unlink(pptr) == NULL) {
			/* no match found -- this shouldn't happen */
			cmn_err(CE_WARN, "fcp(%d): Soft state not found",
			    pptr->ssfcpp_instance);

			break;	/* failure */
		}

		/* signal the hotplug thread to exit */
		mutex_enter(&pptr->ssfcp_hp_mutex);
		pptr->ssfcp_hp_initted = 0;
		cv_signal(&pptr->ssfcp_hp_cv);
		cv_wait(&pptr->ssfcp_hp_cv, &pptr->ssfcp_hp_mutex);
		mutex_exit(&pptr->ssfcp_hp_mutex);

		/*
		 * Unbind and free event set
		 */
		if (pptr->ssfcp_event_hdl) {
			(void) ndi_event_unbind_set(pptr->ssfcp_event_hdl,
			    &pptr->ssfcp_events, NDI_SLEEP);
			(void) ndi_event_free_hdl(pptr->ssfcp_event_hdl);
		}

		if (pptr->ssfcp_event_defs) {
			(void) kmem_free(pptr->ssfcp_event_defs,
			    sizeof (ssfcp_event_defs));
		}

		/* clean up SCSA stuff */
		(void) scsi_hba_detach(pptr->ssfcpp_dip);
		if (pptr->ssfcp_tran != NULL) {
			scsi_hba_tran_free(pptr->ssfcp_tran);
		}

		/* if we are the last port then do final SCSA cleanup */
		mutex_enter(&ssfcp_global_mutex);
		if (ssfcp_hba_init_done && (ssfcp_port_head == NULL)) {
			ssfcp_hba_init_done = FALSE;
			mutex_exit(&ssfcp_global_mutex);
			scsi_hba_fini(&pptr->ssfcpp_linkage);
		} else {
			mutex_exit(&ssfcp_global_mutex);
		}

		/* get rid of cache */
		if (pptr->ssfcp_pkt_cache != NULL) {
			kmem_cache_destroy(pptr->ssfcp_pkt_cache);
		}

		/* free command/response pool */
		ssfcp_crpool_free(pptr);

		/*
		 * Free the lun/target structures and devinfos
		 */
		ssfcp_free_targets(pptr);

#ifdef	KSTATS_CODE
		/* clean up kstats */
		if (pptr->sffcp_ksp != NULL) {
			kstat_delete(pptr->ssfcp_ksp);
		}
#endif

		/* clean up soft state mutexes/condition variables */
		mutex_destroy(&pptr->ssfcp_mutex);
		mutex_destroy(&pptr->ssfcp_cr_mutex);
		mutex_destroy(&pptr->ssfcp_hp_mutex);
		cv_destroy(&pptr->ssfcp_cr_cv);
		cv_destroy(&pptr->ssfcp_hp_cv);

		/* all done with soft state */
		ddi_soft_state_free(ssfcp_softstate, instance);
		break;

	default:
		/* shouldn't happen */
		res = FC_FAILURE;
		break;
	}

	return (res);
}


/*
 * called by transport or internally to handle a port state change
 *
 * should not sleep
 *
 * for now, port speed is ignored
 */
/*ARGSUSED*/
void
ssfcp_statec_callback(opaque_t ulph, opaque_t port_handle,
    uint32_t port_state, uint32_t port_top, fc_portmap_t *devlist,
    uint32_t  dev_cnt, uint32_t port_sid)
{
	struct ssfcp_port	*pptr;
	int			local_mem = 0;

	if ((pptr = ssfcp_get_port(port_handle)) == NULL) {
		ssfcp_log(CE_WARN, NULL,
		    "!Invalid port handle handle in callback");
		return;				/* nothing to work with! */
	}

	SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_statec_callback: port state/sid/dev_cnt = 0x%x/0x%x/%d\n",
	    FC_PORT_STATE_MASK(port_state), port_sid, dev_cnt));

	mutex_enter(&pptr->ssfcp_mutex);

	/*
	 * If a thread is in detach, don't do anything.
	 */
	if (pptr->ssfcp_state & (SSFCP_STATE_DETACHING |
	    SSFCP_STATE_SUSPENDED)) {
		mutex_exit(&pptr->ssfcp_mutex);
		return;
	}

	/*
	 * the transport doesn't allocate or probe unless being
	 * asked to by either the applications or ULPs
	 *
	 * in cases where the port is OFFLINE at the time of port
	 * attach callback and the link comes ONLINE later, for
	 * easier automatic node creation (i.e. without you having to
	 * go out and run the utility to perform LOGINs) the
	 * following conditonal is helpful
	 *
	 * as an optimization, we should also check if the number of
	 * devices created by FCP is zero -- right now, there is no way
	 * to check that, although a simple count of targets created
	 * on a port basis would be easy to implement -- FIXIT
	 */
	pptr->ssfcpp_state = port_state;
	if (FC_PORT_STATE_MASK(port_state) == FC_STATE_ONLINE &&
	    FC_TOP_EXTERNAL(port_top) &&
	    (pptr->ssfcp_state == SSFCP_STATE_OFFLINE ||
		pptr->ssfcp_state == SSFCP_STATE_INIT) && (dev_cnt == 0) &&
	    (devlist == NULL) && !ssfcp_create_nodes_on_demand) {
		/*
		 * we are attached to a switch, port is offline, there
		 * is no device list, and we are not creating nodes on
		 * demand
		 *
		 * get mem for devlist then fill it in
		 */
		if ((devlist = (fc_portmap_t *)kmem_zalloc(
		    sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES,
		    KM_NOSLEEP)) != NULL) {
			dev_cnt = SSFCP_MAX_DEVICES;

			if (fc_ulp_getportmap(pptr->ssfcpp_handle, devlist,
			    &dev_cnt, FC_ULP_PLOGI_PRESERVE) != FC_SUCCESS) {
				/*
				 * can't get portmap? -- perhaps just busy ??
				 */
				kmem_free(devlist, sizeof (fc_portmap_t) *
				    SSFCP_MAX_DEVICES);
				dev_cnt = 0;
				devlist = NULL;
			} else {
				/* keep track of locally allocatedmem */
				local_mem++;
			}
		} else {
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!fcp%d: failed to allocate for portmap",
			    pptr->ssfcpp_instance);
		}
	}

	if (pptr->ssfcpp_sid != port_sid) {
		SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
		    "ssfcp: Port S_ID=0x%x => 0x%x\n", pptr->ssfcpp_sid,
		    port_sid));
		pptr->ssfcpp_sid = port_sid;
	}

	switch (FC_PORT_STATE_MASK(port_state)) {
	case FC_STATE_OFFLINE:
	case FC_STATE_RESET_REQUESTED:
		/*
		 * link has gone from online to offline -- just update the
		 * state of this port to BUSY and MARKed to go offline
		 */
		SSFCP_DEBUG(2, (CE_CONT, pptr->ssfcpp_dip,
		    "link went offline\n"));
		if ((pptr->ssfcp_state & SSFCP_STATE_OFFLINE) && dev_cnt) {
			/*
			 * We were offline a while ago and this one
			 * seems to indicate that the loop has gone
			 * dead forever.
			 */
			pptr->ssfcp_tmp_cnt += dev_cnt;
			pptr->ssfcp_state = SSFCP_STATE_INIT;
			mutex_exit(&pptr->ssfcp_mutex);
			ssfcp_handle_devices(pptr, devlist, dev_cnt,
			    pptr->ssfcp_link_cnt);
		} else {
			pptr->ssfcp_link_cnt++;
			ASSERT(!(pptr->ssfcp_state & SSFCP_STATE_SUSPENDED));
			ssfcp_update_state(pptr, (SSFCP_LUN_BUSY |
			    SSFCP_LUN_MARK));
			pptr->ssfcp_state = SSFCP_STATE_OFFLINE;
			pptr->ssfcp_tmp_cnt = 0;
			mutex_exit(&pptr->ssfcp_mutex);
		}
		break;

	case FC_STATE_ONLINE:
	case FC_STATE_LIP:
	case FC_STATE_LIP_LBIT_SET:
		/*
		 * link has gone from offline to online
		 */

		/*
		 * ASSERT(pptr->ssfcp_tmp_cnt == 0);
		 */

		pptr->ssfcpp_state = port_state;
		pptr->ssfcpp_top = port_top;
		pptr->ssfcp_link_cnt++;
		ssfcp_update_state(pptr, SSFCP_LUN_BUSY | SSFCP_LUN_MARK);
		pptr->ssfcp_state = SSFCP_STATE_ONLINING;
		pptr->ssfcp_tmp_cnt = dev_cnt;
		mutex_exit(&pptr->ssfcp_mutex);

		/*
		 * handle various topologies here
		 */

		switch (port_top) {

		case FC_TOP_FABRIC:
		case FC_TOP_PUBLIC_LOOP:
			/*
			 * for now discover all devices all of the time
			 */
			mutex_enter(&pptr->ssfcp_mutex);
			pptr->ssfcp_state = SSFCP_STATE_ONLINE;
			mutex_exit(&pptr->ssfcp_mutex);

			if (dev_cnt == 0) {
				/* who cares in this case */
				break;
			}

			ssfcp_handle_devices(pptr, devlist, dev_cnt,
			    pptr->ssfcp_link_cnt);
			break;

		case FC_TOP_PRIVATE_LOOP:
		case FC_TOP_PT_PT:
			ssfcp_handle_devices(pptr, devlist, dev_cnt,
			    pptr->ssfcp_link_cnt);
			break;

		default:
			mutex_enter(&pptr->ssfcp_mutex);
			pptr->ssfcp_tmp_cnt -= dev_cnt;
			mutex_exit(&pptr->ssfcp_mutex);

			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!unknown/unsupported topology (0x%x)", port_top);
			break;
		}
		break;

	case FC_STATE_RESET:
		ASSERT(pptr->ssfcp_state == SSFCP_STATE_OFFLINE);
		SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
		    "RESET state, waiting for Offline/Online state_cb\n"));
		mutex_exit(&pptr->ssfcp_mutex);
		break;

	case FC_STATE_DEVICE_CHANGE:
		/*
		 * We come here when an application has requested
		 * Dynamic node creation/deletion in Fabric connectivity.
		 */
		if ((pptr->ssfcp_state == SSFCP_STATE_OFFLINE) ||
			(pptr->ssfcp_state == SSFCP_STATE_INIT)) {
			/*
			 * This case can happen when the FCTL is in the
			 * process of giving us on online and the host on
			 * the other side issues a PLOGI/PLOGO. Ideally
			 * the state changes should be serialized unless
			 * they are opposite (online-offline).
			 * The transport will give us a final state change
			 * so we can ignore this for the time being.
			 */
			mutex_exit(&pptr->ssfcp_mutex);
			break;
		}
		pptr->ssfcp_tmp_cnt += dev_cnt;
		mutex_exit(&pptr->ssfcp_mutex);

		/*
		 * There is another race condition here, where if we were
		 * in ONLINEING state and a devices in the map logs out,
		 * fp will give another state change as DEVICE_CHANGE
		 * and OLD. This will result in that target being offlined.
		 * The pd_handle is freed. If from the first statec callback
		 * we were going to fire a PLOGI/PRLI, the system will
		 * panic in fc_ulp_transport with invalid pd_handle.
		 * The fix is to check for the link_cnt before issueing
		 * any command down.
		 */

		ssfcp_update_targets(pptr, devlist, dev_cnt,
		    SSFCP_LUN_BUSY | SSFCP_LUN_MARK);
		ssfcp_handle_devices(pptr, devlist, dev_cnt,
		    pptr->ssfcp_link_cnt);
		break;

	default:
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!Invalid state change=0x%x", port_state);
		mutex_exit(&pptr->ssfcp_mutex);
		break;
	}

	/* check for cleanup */
	if (local_mem) {
		kmem_free(devlist, sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES);
	}
}


/*
 * called internally when when a port has changed state
 *
 * scan through the supplied device list/map, handling each entry
 *
 * can be called from an interrupt context (via statec_callback) and so
 * should not sleep
 */
static void
ssfcp_handle_devices(struct ssfcp_port *pptr, fc_portmap_t devlist[],
    uint32_t dev_cnt, int link_cnt)
{
	int			i;

	SSFCP_DEBUG(4, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_handle_devices: called for %d dev(s)\n", dev_cnt));

	/* lock the port, since we're going to hack on it */
	mutex_enter(&pptr->ssfcp_mutex);

	/* scan through each entry of the port map */
	for (i = 0; (i < dev_cnt) && (pptr->ssfcp_link_cnt == link_cnt); i++) {
		struct ssfcp_tgt	*ptgt;
		fc_portmap_t		*map_entry;
		int			check_finish_init = 0, user_req = 0;

		/* get a pointer to this map entry */
		map_entry = &(devlist[i]);

		/* get ptr to this map entry in our port's list (if any) */
		ptgt = ssfcp_lookup_target(pptr,
		    (uchar_t *)&(map_entry->map_pwwn));

		if (ptgt) {
			SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
			    "handle_devices: map HA/state/flags = "
			    "0x%x/0x%x/0x%x, tgt=0x%x\n",
			    map_entry->map_hard_addr,
			    map_entry->map_state, map_entry->map_flags,
			    ptgt));
		}

		if (FC_TOP_EXTERNAL(pptr->ssfcpp_top) &&
		    fc_ulp_is_fc4_bit_set(map_entry->map_fc4_types,
		    FC4_SCSI_FCP) != FC_SUCCESS) {
			check_finish_init++;
			goto end_of_loop;
		}

		switch (map_entry->map_flags) {
		case PORT_DEVICE_NOCHANGE: {
			uchar_t		opcode;

			/*
			 * This case is possible where the FCTL has come up
			 * and done discovery before FCP was loaded and
			 * attached. FCTL would have discovered the devices
			 * and later the ULP came online. In this case ULP's
			 * would get PORT_DEVICE_NOCHANGE.
			 */
			if (ptgt == NULL) {
				goto no_target;
			}

			/*
			 * Anything for on demand node creation should be
			 * checked only for EXTERNAL TOPOLOGY.
			 */
			if (FC_TOP_EXTERNAL(pptr->ssfcpp_top) &&
				(!(map_entry->map_state &
				PORT_DEVICE_LOGGED_IN)) &&
				ssfcp_create_nodes_on_demand) {
				check_finish_init++;
				/* skip the switch statment below */
				goto end_of_loop;
			}

			/* we have a target */
			mutex_enter(&ptgt->tgt_mutex);

			/*
			 * The Target is marked OFFLINE internally
			 * in the FCP driver when a PRLI fails
			 * (always happens  if there is another
			 * host on the same loop). So when the next
			 * change happens, and the device is listed
			 * in the map, the update routines don't
			 * set the BUSY and MARK bits as the target
			 * state is marked OFFLINE because of a
			 * previous failure to do PRLI
			 */
			if (ptgt->tgt_state & SSFCP_TGT_OFFLINE) {
				ptgt->tgt_state &= ~SSFCP_TGT_OFFLINE;
				ptgt->tgt_state |= (SSFCP_TGT_BUSY |
				    SSFCP_TGT_MARK);
			}

			if (!(ptgt->tgt_state & SSFCP_TGT_BUSY)) {
				mutex_exit(&ptgt->tgt_mutex);

				ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
				    "!***Duplicate WWN entries*** "
				    " target=%p state=%x", ptgt,
				    ptgt->tgt_state);
				check_finish_init++;
				break;
			}
			/*
			 * This code has been added to take care of a
			 * nasty race condition where if there are two
			 * initiators in the loop and the link goes
			 * offline, the pd_handle can change if the other
			 * host does a logout and login. But in our case
			 * as we are most probably offline(PLDA), we
			 * will ignore the next statec changes for
			 * old and new for this initiator. Ultimately
			 * FCTL will give us no change with an ONLINE.
			 * The pd_handle and the d_id can be changed
			 * under the hood.
			 */
			ptgt->tgt_d_id = map_entry->MAP_PORT_ID;
			ptgt->tgt_hard_addr = map_entry->MAP_HARD_ADDR;
			ptgt->tgt_pd_handle = (opaque_t)map_entry->map_pd;
			mutex_exit(&ptgt->tgt_mutex);
			mutex_exit(&pptr->ssfcp_mutex);
			opcode = (map_entry->map_state &
			    PORT_DEVICE_LOGGED_IN) ? LA_ELS_PRLI :
			    LA_ELS_PLOGI;

			/* discover info about this target */
			if ((ssfcp_do_disc(pptr, ptgt, NULL, opcode)) !=
			    DDI_SUCCESS) {
				/* oh oh -- a problem in discovery */
				mutex_enter(&pptr->ssfcp_mutex);
				if (pptr->ssfcp_link_cnt == link_cnt) {
					check_finish_init++;
				}
			} else {
				mutex_enter(&pptr->ssfcp_mutex);
			}
			break;
		}

		case PORT_DEVICE_USER_ADD: {
			if (ptgt == NULL) {
				/*
				 * This statecb has resulted from a user
				 * land login request. In this case we
				 * should just create the target structure
				 * to do the discovery and inquiry. The dip
				 * for the target node should not be created
				 * right now. When the user issues the
				 * FCP_CREATE for this WWN then we create
				 * the dip and the properties
				 */
				user_req = 1;
				goto no_target;
			} else {
				mutex_enter(&ptgt->tgt_mutex);
				if (ptgt->tgt_state & SSFCP_TGT_OFFLINE) {
					user_req = 1;
					mutex_exit(&ptgt->tgt_mutex);
					goto have_tgt;
				} else if (!(ptgt->tgt_state &
					(SSFCP_TGT_MARK | SSFCP_TGT_BUSY |
						SSFCP_TGT_ON_DEMAND))) {
					/*
					 * This device is already there and
					 * available. Don't need to do
					 * anything.
					 */
					mutex_exit(&ptgt->tgt_mutex);
					check_finish_init++;
					break;
				} else {
					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					    "!Invalid target state = 0x%x\n",
					    ptgt->tgt_state);
					mutex_exit(&ptgt->tgt_mutex);
					check_finish_init++;
					break;
				}
			}
		}
		case PORT_DEVICE_NEW: {
			uchar_t		opcode;

			if (FC_TOP_EXTERNAL(pptr->ssfcpp_top) &&
				(!(map_entry->map_state &
				PORT_DEVICE_LOGGED_IN)) &&
			    ssfcp_create_nodes_on_demand) {
				check_finish_init++;
				/* skip the switch statment below */
				goto end_of_loop;
			}

			if (ptgt != NULL) {
				mutex_enter(&ptgt->tgt_mutex);
				if (!(ptgt->tgt_state & SSFCP_TGT_OFFLINE)) {
					mutex_exit(&ptgt->tgt_mutex);

					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					    "!*Duplicate WWN entries* "
					    " target=%p state=%x", ptgt,
					    ptgt->tgt_state);
					check_finish_init++;
					break;
				}

				/*
				 * This code is added to take care of those
				 * devices that were connected at one point
				 * of time and somebody pulled a cable and
				 * left it out for 2-3 days. The cable was
				 * reconneceted and now the devices need to
				 * reappear. The FCTL would treat these devices
				 * as new but FCP can figure out if the devices
				 * were created by seeing the dip.
				 */
				if (ptgt->tgt_device_created ==
						SSFCP_DEVICE_CREATED) {
					ptgt->tgt_state &= ~SSFCP_TGT_OFFLINE;
					mutex_exit(&ptgt->tgt_mutex);
					/* already have a target */
					goto have_tgt;
				} else {
					mutex_exit(&ptgt->tgt_mutex);
					check_finish_init++;
					break;
				}

			} else {
			/*
			 * Discover the devices for PLDA or PT_PT only.
			 * break is thats not the case.
			 */
				if (FC_TOP_EXTERNAL(pptr->ssfcpp_top)) {
					check_finish_init++;
					break;
				}
			}

no_target:
			/* don't already have a target */
			mutex_exit(&pptr->ssfcp_mutex);
			ptgt = ssfcp_alloc_tgt(pptr, map_entry, link_cnt);
			if (ptgt == NULL) {
				ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
				    "!tgt alloc failed");
				check_finish_init++;
				mutex_enter(&pptr->ssfcp_mutex);
				break;
			} else
				mutex_enter(&pptr->ssfcp_mutex);
			ptgt->tgt_change_cnt = pptr->ssfcp_link_cnt;
have_tgt:
			/*
			 * we have a target -- fill it in
			 */

			mutex_enter(&ptgt->tgt_mutex);

			/* set state to "marked for offline" and "busy" */
			ptgt->tgt_state |= (SSFCP_TGT_MARK | SSFCP_TGT_BUSY);
			if (user_req)
				ptgt->tgt_state |= SSFCP_TGT_ON_DEMAND;

			/* copy map info */
			ptgt->tgt_d_id = map_entry->MAP_PORT_ID;
			ptgt->tgt_hard_addr = map_entry->MAP_HARD_ADDR;
			ptgt->tgt_pd_handle = (opaque_t)map_entry->map_pd;

			/* copy port and node WWNs */
			bcopy(&map_entry->map_nwwn,
					&ptgt->tgt_node_wwn.raw_wwn[0],
			    FC_WWN_SIZE);
			bcopy(&map_entry->map_pwwn,
					&ptgt->tgt_port_wwn.raw_wwn[0],
			    FC_WWN_SIZE);

			/* save cross-ptr */
			ptgt->tgt_port = pptr;

			mutex_exit(&ptgt->tgt_mutex);
			mutex_exit(&pptr->ssfcp_mutex);

			/*
			 * what type of ELS should we perform?
			 *
			 * if we are already logged in, then we do a PRLI,
			 * else we do a PLOGI first (to get logged in)
			 */
			opcode =
			    (map_entry->map_state & PORT_DEVICE_LOGGED_IN) ?
				LA_ELS_PRLI : LA_ELS_PLOGI;

			/* discover info about this target */
			if ((ssfcp_do_disc(pptr, ptgt, NULL, opcode)) !=
			    DDI_SUCCESS) {
				/* oh oh -- a problem in discovery */
				mutex_enter(&pptr->ssfcp_mutex);
				if (pptr->ssfcp_link_cnt == link_cnt) {
					check_finish_init++;
				}
			} else {
				mutex_enter(&pptr->ssfcp_mutex);
			}
			break;
		}

		case PORT_DEVICE_OLD:
		case PORT_DEVICE_USER_REMOVE:
			/*
			 * don't remove the device -- it may return -- so
			 * just offline it
			 */
			if (ptgt != NULL) {
				mutex_exit(&pptr->ssfcp_mutex);
				mutex_enter(&ptgt->tgt_mutex);
				if (!(ptgt->tgt_state & SSFCP_TGT_OFFLINE))
					ssfcp_offline_target(pptr, ptgt);
				mutex_exit(&ptgt->tgt_mutex);
				mutex_enter(&pptr->ssfcp_mutex);
			}
			check_finish_init++;
			break;

		case PORT_DEVICE_CHANGED:
			/* handle generic device changes */
			if (ptgt != NULL) {
				ssfcp_device_changed(pptr, ptgt, map_entry);
				break;
			}
			check_finish_init++;
			break;

		default:
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!Invalid map_flags=0x%x", map_entry->map_flags);
			break;
		}

end_of_loop:
		if (pptr->ssfcp_link_cnt != link_cnt) {
			mutex_exit(&pptr->ssfcp_mutex);
			return;
		}
		if (check_finish_init) {
			/* check to see if we need to finish initialization */
			ASSERT(pptr->ssfcp_tmp_cnt > 0);
			if (--pptr->ssfcp_tmp_cnt == 0) {
				ssfcp_finish_init(pptr, link_cnt);
			}
		}
	}
	mutex_exit(&pptr->ssfcp_mutex);
}

/*
 * called internally to do device discovery, i.e a PLOGI or PRLI
 * (specified by opcode)
 *
 * return DDI_SUCCESS or DDI_FAILURE
 */
static int
ssfcp_do_disc(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt,
    struct ssfcp_ipkt *icmd, uchar_t opcode)
{
	fc_packet_t		*fpkt;
	fc_frame_hdr_t		*hp;
	int			res = DDI_FAILURE; /* default result */
	int			rval = DDI_FAILURE;

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_do_disc: handling 0x%x (%s)\n", opcode,
	    (opcode == LA_ELS_PLOGI) ? "PLOGI" : "PRLI"));

	/* is there already a command packet ?? */
	if (icmd == NULL) {

		/*
		 * no packet yet -- create one with enough data room for
		 * SSFCP_MAX_LUNS LUNs (i.e. 8 x 64 = 256 bytes)
		 */
		if ((icmd = ssfcp_icmd_alloc(pptr, ptgt,
		    sizeof (struct fcp_reportlun_resp))) == NULL) {
			/* this shouldnt' happen */
			return (DDI_FAILURE);
		}

		/* get ptr to fpkt contained in cmd pkt */
		fpkt = icmd->ipkt_fpkt;

		/* fill in fpkt info */
		fpkt->pkt_tran_flags = FC_TRAN_CLASS3 | FC_TRAN_INTR;
		fpkt->pkt_tran_type = FC_PKT_EXCHANGE;
		fpkt->pkt_timeout = SSFCP_ELS_TIMEOUT;

		/* get ptr to frame hdr in fpkt */
		hp = &fpkt->pkt_cmd_fhdr;

		/*
		 * fill in frame hdr
		 */
		hp->r_ctl = R_CTL_ELS_REQ;
		hp->s_id = pptr->ssfcpp_sid;	/* source ID */
		hp->d_id = ptgt->tgt_d_id;	/* dest ID */
		hp->type = FC_TYPE_EXTENDED_LS;
		hp->f_ctl = F_CTL_SEQ_INITIATIVE | F_CTL_FIRST_SEQ;
		hp->seq_id = 0;
		hp->rsvd = 0;
		hp->df_ctl  = 0;
		hp->seq_cnt = 0;
		hp->ox_id = 0xffff;		/* i.e. none */
		hp->rx_id = 0xffff;		/* i.e. none */
		hp->ro = 0;
	} else {
		/* already a packet -- get a pointer to the good part */
		fpkt = icmd->ipkt_fpkt;
	}

	icmd->ipkt_retries = 0;

	/*
	 * at this point we have a filled in cmd pkt
	 *
	 * fill in the respective info, then use the transport to send
	 * the packet
	 *
	 * for a PLOGI call fc_ulp_login(), and
	 * for a PRLI call fc_ulp_issue_els()
	 */
	switch (opcode) {
	case LA_ELS_PLOGI: {
		struct la_els_logi logi;

		bzero(&logi, sizeof (struct la_els_logi));

		hp = &fpkt->pkt_cmd_fhdr;
		hp->r_ctl = R_CTL_ELS_REQ;
		/* fill in PLOGI cmd ELS fields */
		logi.LS_CODE = LA_ELS_PLOGI;
		logi.MBZ = 0;

		SSFCP_CP_OUT((uint8_t *)&logi, fpkt->pkt_cmd,
		    fpkt->pkt_cmd_acc, sizeof (struct la_els_logi));

		icmd->ipkt_opcode = LA_ELS_PLOGI;

		fpkt->pkt_cmdlen = sizeof (la_els_logi_t);
		fpkt->pkt_rsplen = FCP_MAX_RSP_IU_SIZE;
		fpkt->pkt_datalen = 0;		/* no data with this cmd */
		mutex_enter(&ptgt->tgt_mutex);
		if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
			mutex_exit(&ptgt->tgt_mutex);

			if ((rval = fc_ulp_login(pptr->ssfcpp_handle, &fpkt,
			    1)) != FC_SUCCESS) {
				if (rval == FC_STATEC_BUSY || rval ==
				    FC_OFFLINE)  {
					mutex_enter(&pptr->ssfcp_mutex);
					pptr->ssfcp_link_cnt++;
					ssfcp_update_state(pptr,
					    (SSFCP_LUN_BUSY | SSFCP_LUN_MARK));
					pptr->ssfcp_tmp_cnt--;
					mutex_exit(&pptr->ssfcp_mutex);
				}
				ssfcp_icmd_free(fpkt);
				break;		/* failure */
			}
			res = DDI_SUCCESS;	/* success: sent a PLOGI */
		} else {
			mutex_exit(&ptgt->tgt_mutex);
			ssfcp_icmd_free(fpkt);
		}
		break;
	}

	case LA_ELS_PRLI: {
		struct la_els_prli	prli;
		struct fcp_prli		*fprli;

		bzero(&prli, sizeof (struct la_els_prli));

		hp = &fpkt->pkt_cmd_fhdr;
		hp->r_ctl = R_CTL_ELS_REQ;
		/* fill in PRLI cmd ELS fields */
		prli.ls_code = LA_ELS_PRLI;
		prli.page_length = 0x10;	/* huh? */
		prli.payload_length = sizeof (struct la_els_prli);


		icmd->ipkt_opcode = LA_ELS_PRLI;

		fpkt->pkt_cmdlen = sizeof (la_els_prli_t);
		fpkt->pkt_rsplen = FCP_MAX_RSP_IU_SIZE;
		fpkt->pkt_datalen = 0;		/*  no data with this cmd */

		/* get ptr to PRLI service params */
		fprli = (struct fcp_prli *)prli.service_params;

		/* fill in service params */
		fprli->type = 0x08;
		fprli->resvd1 = 0;
		fprli->orig_process_assoc_valid = 0;
		fprli->resp_process_assoc_valid = 0;
		fprli->establish_image_pair = 1;
		fprli->resvd2 = 0;
		fprli->resvd3 = 0;
		fprli->data_overlay_allowed = 0;
		fprli->initiator_fn = 1;
		fprli->target_fn = 0;
		fprli->cmd_data_mixed = 0;
		fprli->data_resp_mixed = 0;
		fprli->read_xfer_rdy_disabled = 1;
		fprli->write_xfer_rdy_disabled = 0;

		SSFCP_CP_OUT((uint8_t *)&prli, fpkt->pkt_cmd,
		    fpkt->pkt_cmd_acc, sizeof (struct la_els_prli));

		/* issue the PRLI request */

		mutex_enter(&ptgt->tgt_mutex);
		if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
			mutex_exit(&ptgt->tgt_mutex);
			if ((rval = fc_ulp_issue_els(pptr->ssfcpp_handle,
			    fpkt)) != FC_SUCCESS) {
				if (rval == FC_STATEC_BUSY ||
				    rval == FC_OFFLINE) {
					mutex_enter(&pptr->ssfcp_mutex);
					pptr->ssfcp_link_cnt++;
					ssfcp_update_state(pptr,
					    (SSFCP_LUN_BUSY | SSFCP_LUN_MARK));
					pptr->ssfcp_tmp_cnt--;
					mutex_exit(&pptr->ssfcp_mutex);
				}
				ssfcp_icmd_free(fpkt);
				break;		/* failure */
			}
			res = DDI_SUCCESS;	/* success: sent a PRLI */
		} else {
			mutex_exit(&ptgt->tgt_mutex);
			ssfcp_icmd_free(fpkt);
		}
		break;
	}

	default:
		ssfcp_log(CE_WARN, NULL, "!invalid ELS opcode=0x%x", opcode);

		ssfcp_icmd_free(fpkt);
		break;				/* failure */
	}

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_do_disc: returning %d\n", res));
	return (res);
}
/*
 * called internally update the state of all of the tgts and each LUN
 * for this port (i.e. each target  known to be attached to this port)
 * if they are not already offline
 *
 * must be called with the port mutex owned
 *
 * acquires and releases the target mutexes for each target attached
 * to this port
 */
void
ssfcp_update_state(struct ssfcp_port *pptr, uint32_t state)
{
	int i;
	struct ssfcp_tgt *ptgt;

	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	for (i = 0; i < SSFCP_NUM_HASH; i++) {
		for (ptgt = pptr->ssfcp_wwn_list[i]; ptgt != NULL;
				ptgt = ptgt->tgt_next) {
			mutex_enter(&ptgt->tgt_mutex);
			ssfcp_update_tgt_state(ptgt, FCP_SET, state);
			ptgt->tgt_change_cnt++;
			mutex_exit(&ptgt->tgt_mutex);
		}
	}
}

void
ssfcp_update_tgt_state(struct ssfcp_tgt *ptgt, int flag, uint32_t state)
{
	struct ssfcp_lun *plun;

	ASSERT(mutex_owned(&ptgt->tgt_mutex));

	if (!(ptgt->tgt_state & SSFCP_TGT_OFFLINE)) {
		flag == FCP_SET ? (ptgt->tgt_state |= state) :
				(ptgt->tgt_state &= ~state);
		for (plun = ptgt->tgt_lun; plun != NULL;
					plun = plun->lun_next) {
			if (!(plun->lun_state & SSFCP_LUN_OFFLINE)) {
				flag == FCP_SET ? (plun->lun_state |= state):
					(plun->lun_state &= ~state);
			}
		}
	}
}
/*
 * get a port based on a port handle
 *
 * acquires and releases the global mutex
 */
static struct ssfcp_port *
ssfcp_get_port(opaque_t port_handle)
{
	struct ssfcp_port *pptr;

	ASSERT(port_handle != NULL);

	mutex_enter(&ssfcp_global_mutex);
	for (pptr = ssfcp_port_head; pptr != NULL; pptr = pptr->ssfcp_next) {
		if (pptr->ssfcpp_handle == port_handle) {
			break;
		}
	}
	mutex_exit(&ssfcp_global_mutex);
	return (pptr);
}

static void
ssfcp_unsol_callback(fc_packet_t *fpkt)
{
	ssfcp_icmd_free(fpkt);
}

/*
 * Perform general purpose preparation of a response to an unsolicited request
 */
static void
ssfcp_unsol_resp_init(fc_packet_t *pkt, fc_unsol_buf_t *buf,
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
	pkt->pkt_comp = ssfcp_unsol_callback;
	pkt->pkt_pd = NULL;
}


/*ARGSUSED*/
static int
ssfcp_els_callback(opaque_t ulph, opaque_t port_handle,
    fc_unsol_buf_t *buf, uint32_t claimed)
{
	uchar_t			r_ctl;
	uchar_t			ls_code;
	struct ssfcp_port	*pptr;

	if ((pptr = ssfcp_get_port(port_handle)) == NULL) {
		return (FC_UNCLAIMED);
	}

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_state & (SSFCP_STATE_DETACHING |
	    SSFCP_STATE_SUSPENDED)) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (FC_UNCLAIMED);
	}
	mutex_exit(&pptr->ssfcp_mutex);

	r_ctl = buf->ub_frame.r_ctl;
	switch (r_ctl & R_CTL_ROUTING) {
	case R_CTL_EXTENDED_SVC:
		if (r_ctl == R_CTL_ELS_REQ) {
			ls_code = buf->ub_buffer[0];
			if ((ls_code == LA_ELS_PRLI) && (claimed == 0)) {
				/*
				 * We really don't care if something fails.
				 * If the PRLI was not sent out, then the
				 * other host will time it out.
				 */
				if (ssfcp_unsol_prli(pptr, buf) == FC_SUCCESS)
					return (FC_SUCCESS);
				else
					return (FC_UNCLAIMED);
			}
		}
		/* FALLTHROUGH */
	default:
		return (FC_UNCLAIMED);
	}
}


/*ARGSUSED*/
static int
ssfcp_unsol_prli(struct ssfcp_port *pptr, fc_unsol_buf_t *buf)
{
	fc_packet_t		*fpkt;
	struct la_els_prli	*prli;
	struct fcp_prli		*fprli;
	struct ssfcp_ipkt	*icmd;

	if ((icmd = ssfcp_icmd_alloc(pptr, NULL,
	    sizeof (struct fcp_reportlun_resp))) == NULL) {
		return (FC_FAILURE);
	}
	fpkt = icmd->ipkt_fpkt;
	fpkt->pkt_tran_flags = FC_TRAN_CLASS3 | FC_TRAN_INTR;
	fpkt->pkt_tran_type = FC_PKT_OUTBOUND;
	fpkt->pkt_timeout = SSFCP_ELS_TIMEOUT;
	fpkt->pkt_cmdlen = sizeof (la_els_prli_t);
	fpkt->pkt_rsplen = FCP_MAX_RSP_IU_SIZE;
	fpkt->pkt_datalen = 0;
	prli = (struct la_els_prli *)fpkt->pkt_cmd;
	fprli = (struct fcp_prli *)prli->service_params;
	prli->ls_code = LA_ELS_ACC;
	prli->page_length = 0x10;
	prli->payload_length = sizeof (struct la_els_prli);

	icmd->ipkt_opcode = LA_ELS_PRLI;

	fprli->type = 0x08;
	fprli->resvd1 = 0;
	fprli->orig_process_assoc_valid = 0;
	fprli->resp_process_assoc_valid = 0;
	fprli->establish_image_pair = 1;
	fprli->resvd2 = 0;
	fprli->resvd3 = 0;
	fprli->data_overlay_allowed = 0;
	fprli->initiator_fn = 1;
	fprli->target_fn = 0;
	fprli->cmd_data_mixed = 0;
	fprli->data_resp_mixed = 0;
	fprli->read_xfer_rdy_disabled = 1;
	fprli->write_xfer_rdy_disabled = 0;
	ssfcp_unsol_resp_init(fpkt, buf, R_CTL_ELS_RSP, FC_TYPE_EXTENDED_LS);
	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt == icmd->ipkt_link_cnt) {
		int rval;
		mutex_exit(&pptr->ssfcp_mutex);
		if ((rval = fc_ulp_issue_els(pptr->ssfcpp_handle, fpkt)) !=
		    FC_SUCCESS) {
			if (rval == FC_STATEC_BUSY || rval == FC_OFFLINE) {
				mutex_enter(&pptr->ssfcp_mutex);
				pptr->ssfcp_link_cnt++;
				ssfcp_update_state(pptr, (SSFCP_LUN_BUSY |
				    SSFCP_LUN_MARK));
				pptr->ssfcp_tmp_cnt--;
				mutex_exit(&pptr->ssfcp_mutex);
			}
			/* Let it timeout */
			ssfcp_icmd_free(fpkt);
			return (FC_FAILURE);
		}

	} else {
		mutex_exit(&pptr->ssfcp_mutex);
		ssfcp_icmd_free(fpkt);
		return (FC_FAILURE);
	}
	(void) fc_ulp_ubrelease(pptr->ssfcpp_handle, 1, &buf->ub_token);
	return (FC_SUCCESS);

}

/*ARGSUSED*/
static int
ssfcp_data_callback(opaque_t ulph, opaque_t port_handle,
    fc_unsol_buf_t *buf, uint32_t claimed)
{
	/* target mode driver will handle these */
	return (FC_UNCLAIMED);
}



/*
 * called internally by either ssfcp_do_disc() or ssfcp_icmd_scsi()
 * to allocate an ssfcp information pkt
 *
 * the callback routine for this packet will be ssfcp_icmd_callback()
 *
 * memory is allocated in no-sleep mode
 */
static struct ssfcp_ipkt *
ssfcp_icmd_alloc(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt,
    uint32_t datalen)
{
	fc_packet_t		*fpkt;
	struct ssfcp_ipkt	*icmd = NULL;
	uint_t			ccount;
	size_t			real_size;
	int			cmd_bound = 0;
	int			rsp_bound = 0;
	int			data_bound = 0;

	/*
	 * allocate a hunk of memory for:
	 *  - an ssfcp_ipkt struct (which we return a ptr to)
	 *  - an fc_packet_t struct
	 *  - port private info (priv_pkt_len bytes long)
	 */
	if ((icmd = (struct ssfcp_ipkt *)kmem_zalloc(
	    sizeof (struct ssfcp_ipkt) + pptr->ssfcpp_priv_pkt_len,
	    KM_NOSLEEP)) == NULL) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!internal packet allocation failed");
		return (NULL);
	}

	/*
	 * initialize the allocated packet
	 */
	icmd->ipkt_next = icmd->ipkt_prev = NULL; /* not linked in yet */
	icmd->ipkt_lun = NULL;			/* huh? */
	mutex_enter(&pptr->ssfcp_mutex);
	icmd->ipkt_port = pptr;
	icmd->ipkt_link_cnt = pptr->ssfcp_link_cnt;
	mutex_exit(&pptr->ssfcp_mutex);

	/* keep track of amt of data to be sent in pkt */
	icmd->ipkt_datalen = datalen;

	/* set up pkt's ptr to the fc_packet_t struct, just after the ipkt */
	icmd->ipkt_fpkt = (fc_packet_t *)(&icmd->ipkt_fc_packet);

	/* set pkt's private ptr to point to cmd pkt */
	icmd->ipkt_fpkt->pkt_ulp_private = (opaque_t)icmd;

	/* set FCA private ptr to memory just beyond */
	icmd->ipkt_fpkt->pkt_fca_private = (opaque_t)
	    ((char *)icmd + sizeof (struct ssfcp_ipkt));

	/* get ptr to fpkt substruct and fill it in */
	fpkt = icmd->ipkt_fpkt;
	if (ptgt != NULL) {
		mutex_enter(&ptgt->tgt_mutex);
		icmd->ipkt_tgt = ptgt;
		icmd->ipkt_change_cnt = ptgt->tgt_change_cnt;
		fpkt->pkt_pd = ptgt->tgt_pd_handle;
		mutex_exit(&ptgt->tgt_mutex);
	}
	fpkt->pkt_comp = ssfcp_icmd_callback;
	fpkt->pkt_tran_flags = (FC_TRAN_CLASS3 | FC_TRAN_INTR);

	/*
	 * set up DMA handle and memory for this cmd packet
	 */
	if (ddi_dma_alloc_handle(pptr->ssfcpp_dip, &pptr->ssfcpp_dma_attr,
	    DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_cmd_dma) != DDI_SUCCESS) {
		goto fail;
	}
	if (ddi_dma_mem_alloc(fpkt->pkt_cmd_dma,
	    sizeof (union ssfcp_internal_cmd), &pptr->ssfcpp_dma_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_cmd,
	    &real_size, &fpkt->pkt_cmd_acc) != DDI_SUCCESS) {
		goto fail;
	}

	/* was DMA mem size gotten less than size asked for/needed ?? */
	if (real_size < sizeof (union ssfcp_internal_cmd)) {
		/* shouldn't happen unless low on memory? */
		goto fail;
	}

	/* bind DMA address and handle together */
	if (ddi_dma_addr_bind_handle(fpkt->pkt_cmd_dma, NULL,
	    fpkt->pkt_cmd, real_size, DDI_DMA_WRITE | DDI_DMA_CONSISTENT,
	    DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_cmd_cookie,
	    &ccount) != DDI_DMA_MAPPED) {
		goto fail;
	}
	cmd_bound++;		/* keep track for cleanup purposes */
	if (ccount != 1) {
		goto fail;	/* shouldn't happen */
	}

	/*
	 * set up DMA handle and memory for the response to this cmd
	 */
	if (ddi_dma_alloc_handle(pptr->ssfcpp_dip, &pptr->ssfcpp_dma_attr,
	    DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_resp_dma) != DDI_SUCCESS) {
		goto fail;
	}
	if (ddi_dma_mem_alloc(fpkt->pkt_resp_dma,
	    sizeof (union ssfcp_internal_rsp), &pptr->ssfcpp_dma_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_resp,
	    &real_size, &fpkt->pkt_resp_acc) != DDI_SUCCESS) {
		goto fail;
	}

	/* was DMA mem size gotten less than size asked for/needed ?? */
	if (real_size < (sizeof (union ssfcp_internal_rsp))) {
		/* shouldn't happen unless low on memory? */
		goto fail;
	}

	/* bind DMA address and handle together */
	if (ddi_dma_addr_bind_handle(fpkt->pkt_resp_dma, NULL,
	    fpkt->pkt_resp, real_size, DDI_DMA_READ | DDI_DMA_CONSISTENT,
	    DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_resp_cookie, &ccount) !=
	    DDI_DMA_MAPPED) {
		goto fail;
	}
	rsp_bound++;		/* keep track for cleanup purposes */
	if (ccount != 1) {
		goto fail;	/* shouldn't happen */
	}

	/* will this packet have any data associted with it ?? */
	if (datalen != 0) {

		/*
		 * set up DMA handle and memory for the data in this packet
		 */
		if (ddi_dma_alloc_handle(pptr->ssfcpp_dip,
		    &pptr->ssfcpp_dma_attr, DDI_DMA_DONTWAIT,
		    NULL, &fpkt->pkt_data_dma) != DDI_SUCCESS) {
			goto fail;
		}
		if (ddi_dma_mem_alloc(fpkt->pkt_data_dma, datalen,
			&pptr->ssfcpp_dma_acc_attr, DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT, NULL, &fpkt->pkt_data,
			&real_size, &fpkt->pkt_data_acc) != DDI_SUCCESS) {
			goto fail;
		}

		/* was DMA mem size gotten < size asked for/needed ?? */
		if (real_size < datalen) {
			/* shouldn't happen unless low on memory? */
			goto fail;
		}

		/* bind DMA address and handle together */
		if (ddi_dma_addr_bind_handle(fpkt->pkt_data_dma,
		    NULL, fpkt->pkt_data, real_size, DDI_DMA_READ |
		    DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL,
		    &fpkt->pkt_data_cookie, &ccount) != DDI_DMA_MAPPED) {
			goto fail;
		}
		data_bound++;		/* keep track for cleanup purposes */
		if (ccount != 1) {
			goto fail;	/* shouldn't happen */
		}
	}

	/* ask transport to do its initialization on this pkt */
	if (fc_ulp_init_packet(pptr->ssfcpp_handle, fpkt,
	    KM_NOSLEEP) != FC_SUCCESS) {
		SSFCP_DEBUG(4, (CE_NOTE, pptr->ssfcpp_dip,
		    "fc_ulp_init_packet failed\n"));
		goto fail;
	}

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_state & (SSFCP_STATE_DETACHING |
	    SSFCP_STATE_SUSPENDED)) {
		int rval;

		mutex_exit(&pptr->ssfcp_mutex);

		rval = fc_ulp_uninit_packet(pptr->ssfcpp_handle, fpkt);
		ASSERT(rval == FC_SUCCESS);

		goto fail;
	}
	pptr->ssfcp_ipkt_cnt++;
	mutex_exit(&pptr->ssfcp_mutex);

	/* return ptr to filled in cmd pkt */
	return (icmd);

	/* a failure -- clean up and return failure value */
fail:
	if (fpkt->pkt_cmd_dma != NULL) {
		if (cmd_bound) {
			(void) ddi_dma_unbind_handle(fpkt->pkt_cmd_dma);
		}
		ddi_dma_free_handle(&fpkt->pkt_cmd_dma);
		if (fpkt->pkt_cmd != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_cmd_acc);
		}
		fpkt->pkt_cmd_dma = NULL;
	}
	if (fpkt->pkt_resp_dma != NULL) {
		if (rsp_bound) {
			(void) ddi_dma_unbind_handle(fpkt->pkt_resp_dma);
		}
		ddi_dma_free_handle(&fpkt->pkt_resp_dma);
		if (fpkt->pkt_resp != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_resp_acc);
		}
		fpkt->pkt_resp_dma = NULL;
	}
	if (fpkt->pkt_data_dma != NULL) {
		if (data_bound) {
			(void) ddi_dma_unbind_handle(fpkt->pkt_data_dma);
		}
		ddi_dma_free_handle(&fpkt->pkt_data_dma);
		if (fpkt->pkt_data != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_data_acc);
		}
		fpkt->pkt_data_dma = NULL;
	}
	kmem_free(icmd, sizeof (struct ssfcp_ipkt) + pptr->ssfcpp_priv_pkt_len);
	return (NULL);			/* please allow us to bail */
}


/*
 * called by ssfcp_handle_devices to find an ssfcp target given
 * a WWN
 */
/* ARGSUSED */
static struct ssfcp_tgt *
ssfcp_lookup_target(struct ssfcp_port *pptr, uchar_t *wwn)
{
	int hash;
	struct ssfcp_tgt *ptgt;


	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	hash = SSFCP_HASH(wwn);

	for (ptgt = pptr->ssfcp_wwn_list[hash];
	    ptgt != NULL;
	    ptgt = ptgt->tgt_next) {
		if (bcmp((caddr_t)wwn, (caddr_t)&ptgt->tgt_port_wwn.raw_wwn[0],
		    sizeof (ptgt->tgt_port_wwn)) == 0) {
			break;
		}
	}
	return (ptgt);
}




/*
 * called internally to free an info cmd pkt
 */
void
ssfcp_icmd_free(fc_packet_t *fpkt)
{
	int rval;
	struct ssfcp_ipkt *icmd = (struct ssfcp_ipkt *)fpkt->pkt_ulp_private;
	struct ssfcp_port *pptr = icmd->ipkt_port;

	rval = fc_ulp_uninit_packet(pptr->ssfcpp_handle, fpkt);
	ASSERT(rval == FC_SUCCESS);

	if (fpkt->pkt_cmd_dma != NULL) {
		(void) ddi_dma_unbind_handle(fpkt->pkt_cmd_dma);
		ddi_dma_free_handle(&fpkt->pkt_cmd_dma);
		if (fpkt->pkt_cmd != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_cmd_acc);
		}
		fpkt->pkt_cmd_dma = NULL;
	}
	if (fpkt->pkt_resp_dma != NULL) {
		(void) ddi_dma_unbind_handle(fpkt->pkt_resp_dma);
		ddi_dma_free_handle(&fpkt->pkt_resp_dma);
		if (fpkt->pkt_resp != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_resp_acc);
		}
		fpkt->pkt_resp_dma = NULL;
	}
	if (fpkt->pkt_data_dma != NULL) {
		(void) ddi_dma_unbind_handle(fpkt->pkt_data_dma);
		ddi_dma_free_handle(&fpkt->pkt_data_dma);
		if (fpkt->pkt_data != NULL) {
			ddi_dma_mem_free(&fpkt->pkt_data_acc);
		}
		fpkt->pkt_data_dma = NULL;
	}
	kmem_free(icmd, sizeof (struct ssfcp_ipkt) + pptr->ssfcpp_priv_pkt_len);

	mutex_enter(&pptr->ssfcp_mutex);
	--pptr->ssfcp_ipkt_cnt;
	mutex_exit(&pptr->ssfcp_mutex);
}


/*
 * the packet completion callback routine for info cmd pkts
 *
 * this means fpkt pts to a response to either a PLOGI or a PRLI
 *
 * if there is an error an attempt is made to call a routine to resend
 * the command that failed
 */
static void
ssfcp_icmd_callback(fc_packet_t *fpkt)
{
	struct ssfcp_ipkt	*icmd;
	struct ssfcp_port	*pptr;
	struct ssfcp_tgt	*ptgt;
	struct la_els_prli	*prli;
	struct la_els_prli	prli_s;
	struct fcp_prli		*fprli;
	struct ssfcp_lun	*plun;
	int			free_pkt = 1;
	int			rval;
	struct la_els_logi	*ptr = (struct la_els_logi *)fpkt->pkt_resp;

	icmd = (struct ssfcp_ipkt *)fpkt->pkt_ulp_private;
	/* get ptrs to the port and target structs for the cmd */
	pptr = icmd->ipkt_port;
	ptgt = icmd->ipkt_tgt;

	/* is this successful reponse to an ELS ?? */
	if ((fpkt->pkt_state == FC_PKT_SUCCESS) &&
	    (ptr->LS_CODE == LA_ELS_ACC)) {

		/* successful response */

		mutex_enter(&ptgt->tgt_mutex);
		if (ptgt->tgt_pd_handle == NULL) {
			/*
			 * in a fabric environment the port device handles
			 * get created only after successful LOGIN into the
			 * transport, so the transport makes this port
			 * device (pd) handle available in this packet, so
			 * save it now
			 */
			ASSERT(fpkt->pkt_pd != NULL);
			ptgt->tgt_pd_handle = fpkt->pkt_pd;
		}
		mutex_exit(&ptgt->tgt_mutex);

		/* which ELS cmd is this response for ?? */
		switch (icmd->ipkt_opcode) {

		case LA_ELS_PLOGI:
			SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
			    "PLOGI to d_id=0x%x succeeded, wwn=%08x%08x\n",
			    ptgt->tgt_d_id,
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[0]),
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[4])));

			if (ssfcp_do_disc(pptr, ptgt, icmd, LA_ELS_PRLI) !=
			    DDI_SUCCESS) {
				free_pkt = 0;	/* pkt already freed */
				goto fail;
			}
			break;

		case LA_ELS_PRLI:
			SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
			    "PRLI to d_id=0x%x succeeded\n", ptgt->tgt_d_id));
			prli = &prli_s;

			SSFCP_CP_IN(fpkt->pkt_resp, prli, fpkt->pkt_resp_acc,
			    sizeof (prli_s));

			fprli = (struct fcp_prli *)prli->service_params;

			if ((fprli->type != 0x08) || (fprli->target_fn != 1)) {
				/*
				 * this FCP device does not support target mode
				 */
				goto fail;
			}

			/* target is no longer offline */
			mutex_enter(&ptgt->tgt_mutex);
			if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt)
				ptgt->tgt_state &= ~(SSFCP_TGT_OFFLINE|
						SSFCP_TGT_MARK);
			else {
				mutex_exit(&ptgt->tgt_mutex);
				goto fail;
			}
			mutex_exit(&ptgt->tgt_mutex);

			/*
			 * lun 0 should always respond to inquiry, so
			 * get the LUN struct for LUN 0
			 *
			 * XXX: currently we deal with first level of
			 * addressing -- when we start supporting 0xC
			 * device types (DTYPE_ARRAY_CTRL, i.e. array
			 * controllers) then need to revisit this logic
			 */
			if ((plun = ssfcp_get_lun(ptgt, 0)) == NULL) {
				/*
				 * no LUN struct for LUN 0 yet exists,
				 * so create one
				 */
				plun = ssfcp_alloc_lun(ptgt);
				if (plun == NULL) {
					ssfcp_log(CE_WARN, NULL,
					    "!lun alloc failed");
					goto fail;
				}
			}

			/* fill in LUN info */
			plun->lun_state |=  (SSFCP_LUN_BUSY|SSFCP_LUN_MARK);
			plun->lun_state &= ~SSFCP_LUN_OFFLINE;
			ptgt->tgt_lun_cnt = 1;
			ptgt->tgt_tmp_cnt = 1;

			/* send Report Lun request to target */
			if (ssfcp_icmd_scsi(plun, icmd, SCMD_REPORT_LUN) !=
			    DDI_SUCCESS) {
				free_pkt = 0;	/* packet already freed */
				goto fail;
			}
			break;

		default:
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!ssfcp_icmd_callback Invalid opcode");
			goto fail;
		}

		return;
	}

	/* this was not a successful response -- there was a problem */

	if (++(icmd->ipkt_retries) >= SSFCP_ELS_RETRIES ||
	    icmd->ipkt_opcode == LA_ELS_PLOGI) {
		char buf[96];
		caddr_t state, reason, action, expln;

		(void) fc_ulp_pkt_error(fpkt, &state, &reason,
		    &action, &expln);

		SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
		    "ssfcp_icmd_callback; D_ID=%x; pkt_state=%x;"
		    " pkt_reason=%x", fpkt->pkt_cmd_fhdr.d_id, fpkt->pkt_state,
		    fpkt->pkt_reason));

		switch (icmd->ipkt_opcode) {
			case LA_ELS_PLOGI:
				(void) sprintf(buf,
				"!PLOGI to d_id=0x%%x failed. State:%%s, "
				"Reason:%%s. Giving up\n");
				break;
			case LA_ELS_PRLI:
				(void) sprintf(buf,
				"!PRLI to d_id=0x%%x failed. State:%%s, "
				"Reason:%%s. Giving up\n");
				break;
		}
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip, buf, ptgt->tgt_d_id,
			state, reason);
		goto fail;
	}

	SSFCP_DEBUG(2, (CE_CONT, pptr->ssfcpp_dip,
	    "ELS 0x%x, retried for d_id=0x%x\n", icmd->ipkt_opcode,
	    ptgt->tgt_d_id));

	/* retry by recalling the routine that orignally queued this packet */

	mutex_enter(&ptgt->tgt_mutex);
	if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
		mutex_exit(&ptgt->tgt_mutex);

		ASSERT(icmd->ipkt_opcode != LA_ELS_PLOGI);

		rval = fc_ulp_issue_els(pptr->ssfcpp_handle, fpkt);
		if (rval != FC_SUCCESS) {
			if (rval == FC_STATEC_BUSY || rval == FC_OFFLINE) {
				mutex_enter(&pptr->ssfcp_mutex);
				pptr->ssfcp_link_cnt++;
				ssfcp_update_state(pptr, (SSFCP_LUN_BUSY |
				    SSFCP_LUN_MARK));
				pptr->ssfcp_tmp_cnt--;
				mutex_exit(&pptr->ssfcp_mutex);
				goto fail;
			}

			ssfcp_log(CE_NOTE, pptr->ssfcpp_dip,
			    "!ELS 0x%x failed to d_id=0x%x\n",
				icmd->ipkt_opcode, ptgt->tgt_d_id);
			goto fail;
		}
	} else {
		mutex_exit(&ptgt->tgt_mutex);
		goto fail;
	}
	return;					/* success */

fail:
	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt == icmd->ipkt_link_cnt) {
		ASSERT(pptr->ssfcp_tmp_cnt > 0);
		if (--pptr->ssfcp_tmp_cnt == 0) {
			ssfcp_finish_init(pptr, pptr->ssfcp_link_cnt);
		}
	}
	mutex_exit(&pptr->ssfcp_mutex);

	if (free_pkt) {
		ssfcp_icmd_free(fpkt);
	}
}


/*
 * called internally to send an info cmd using the transport
 *
 * sends either an INQ or a REPORT_LUN
 *
 * when the packet is completed ssfcp_scsi_callback is called
 */
int
ssfcp_icmd_scsi(struct ssfcp_lun *plun, struct ssfcp_ipkt *icmd,
    uchar_t opcode)
{
	struct ssfcp_tgt	*ptgt;
	struct ssfcp_port	*pptr;
	fc_frame_hdr_t		*hp;
	fc_packet_t		*fpkt;
	struct fcp_cmd		fcmd;

	bzero(&fcmd, sizeof (struct fcp_cmd));

	ASSERT(plun != NULL);
	ptgt = plun->lun_tgt;
	ASSERT(ptgt != NULL);
	pptr = ptgt->tgt_port;

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_icmd_scsi: opcode=0x%x\n", opcode));

	/* do we already have a command struct ?? */
	if (icmd == NULL) {
		/* no struct yet -- create one  and fill it in */
		if ((icmd = ssfcp_icmd_alloc(pptr, ptgt,
		    SSFCP_MAX_LUNS * FCP_LUN_SIZE)) == NULL) {
			return (DDI_FAILURE);
		}
		fpkt = icmd->ipkt_fpkt;
		fpkt->pkt_tran_flags = FC_TRAN_CLASS3 | FC_TRAN_INTR;
	} else {
		/* just get a ptr to the fpkt part of the current cmd struct */
		fpkt = icmd->ipkt_fpkt;
	}

	icmd->ipkt_opcode = opcode;
	icmd->ipkt_lun = plun;

	fpkt->pkt_cmdlen = sizeof (struct fcp_cmd);
	fpkt->pkt_rsplen = FCP_MAX_RSP_IU_SIZE;
	fpkt->pkt_timeout = SSFCP_SCSI_CMD_TIMEOUT;

	hp = &fpkt->pkt_cmd_fhdr;

	hp->s_id = pptr->ssfcpp_sid;
	hp->d_id = ptgt->tgt_d_id;
	hp->r_ctl = R_CTL_COMMAND;
	hp->type = FC_TYPE_SCSI_FCP;
	hp->f_ctl = F_CTL_SEQ_INITIATIVE | F_CTL_FIRST_SEQ;
	hp->rsvd = 0;
	hp->seq_id = 0;
	hp->seq_cnt = 0;
	hp->ox_id = 0xffff;
	hp->rx_id = 0xffff;
	hp->ro = 0;

	bcopy(&(plun->lun_string), &(fcmd.fcp_ent_addr), FCP_LUN_SIZE);

	switch (opcode) {

	case SCMD_INQUIRY:
		fcmd.fcp_cntl.cntl_read_data = 1;
		fcmd.fcp_cntl.cntl_write_data = 0;
		fcmd.fcp_data_len = SUN_INQSIZE;

		fpkt->pkt_tran_type =  FC_PKT_FCP_READ;
		fpkt->pkt_datalen = SUN_INQSIZE;
		fpkt->pkt_comp = ssfcp_scsi_callback;

		((union scsi_cdb *)fcmd.fcp_cdb)->scc_cmd = SCMD_INQUIRY;
		((union scsi_cdb *)fcmd.fcp_cdb)->g0_count0 = SUN_INQSIZE;
		break;

	case SCMD_REPORT_LUN:
		/*
		 * XXX: hard coded to 0x800/2048 = 256 luns x 8 bytes
		 */
		fcmd.fcp_cntl.cntl_read_data = 1;
		fcmd.fcp_cntl.cntl_write_data = 0;
		fcmd.fcp_data_len = SSFCP_MAX_LUNS * FCP_LUN_SIZE;

		fpkt->pkt_datalen = SSFCP_MAX_LUNS * FCP_LUN_SIZE;
		fpkt->pkt_tran_type = FC_PKT_FCP_READ;
		fpkt->pkt_comp = ssfcp_scsi_callback;

		((union scsi_cdb *)fcmd.fcp_cdb)->scc_cmd = SCMD_REPORT_LUN;

		((union scsi_cdb *)fcmd.fcp_cdb)->scc5_count0 = 0;
		((union scsi_cdb *)fcmd.fcp_cdb)->scc5_count1 = 8;
		break;

	default:
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!ssfcp_icmd_scsi Invalid opcode");
		break;
	}

	SSFCP_CP_OUT((uint8_t *)&fcmd, fpkt->pkt_cmd,
	    fpkt->pkt_cmd_acc, sizeof (struct fcp_cmd));

	mutex_enter(&ptgt->tgt_mutex);
	if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
		mutex_exit(&ptgt->tgt_mutex);
		if (ssfcp_transport(pptr->ssfcpp_handle, fpkt,
		    1) != FC_SUCCESS) {
			ssfcp_icmd_free(fpkt);
			return (DDI_FAILURE);
		}
		return (DDI_SUCCESS);
	} else {
		mutex_exit(&ptgt->tgt_mutex);
		ssfcp_icmd_free(fpkt);
		return (DDI_FAILURE);
	}
}


/*
 * called by ssfcp_scsi_callback to check to handle the case where
 * REPORT_LUN returns ILLEGAL REQUEST or a UNIT ATTENTION
 */
static int
ssfcp_check_reportlun(struct fcp_rsp *rsp, fc_packet_t *fpkt)
{
	uchar_t				rqlen;
	struct scsi_extended_sense	sense_info, *sense;
	struct ssfcp_ipkt		*icmd = (struct ssfcp_ipkt *)
					    fpkt->pkt_ulp_private;
	struct ssfcp_tgt		*ptgt = icmd->ipkt_tgt;
	struct ssfcp_port		*pptr = ptgt->tgt_port;

	sense = &sense_info;
	if (!rsp->fcp_u.fcp_status.sense_len_set) {
		/* no need to coninue if sense length is not set */
		return (DDI_SUCCESS);
	}

	/* casting 64-bit integer to 8-bit */
	rqlen = (uchar_t)min(rsp->fcp_sense_len,
	    sizeof (struct scsi_extended_sense));

	if (rqlen < 14) {
		/* no need to coninue if request length isn't long enough */
		return (DDI_SUCCESS);
	}

	SSFCP_CP_IN(fpkt->pkt_resp + sizeof (struct fcp_rsp) +
		rsp->fcp_response_len, sense, fpkt->pkt_resp_acc,
				sizeof (struct scsi_extended_sense));

	if ((sense->es_key == KEY_ILLEGAL_REQUEST) &&
	    (sense->es_add_code == 0x20)) {
		/*
		 * clean up some fields
		 */
		rsp->fcp_u.fcp_status.rsp_len_set = 0;
		rsp->fcp_u.fcp_status.sense_len_set = 0;
		rsp->fcp_u.fcp_status.scsi_status = STATUS_GOOD;

		SSFCP_CP_OUT(ssfcp_dummy_lun, fpkt->pkt_data,
		    fpkt->pkt_data_acc, sizeof (ssfcp_dummy_lun));

		return (DDI_SUCCESS);
	}

	if (sense->es_key == KEY_UNIT_ATTENTION) {
		if (sense->es_add_code == 0x29)
			return (DDI_FAILURE);
	}

	if (sense->es_key == KEY_UNIT_ATTENTION &&
	    sense->es_add_code == 0x3f &&
	    sense->es_qual_code == 0x0e) {
		ssfcp_log(CE_NOTE, pptr->ssfcpp_dip,
		    "!FCP: Report Lun Has Changed\n");

		if (ptgt->tgt_tid == NULL) {
			timeout_id_t	tid;
			tid = timeout(ssfcp_reconfigure_luns,
			    (caddr_t)ptgt,
			    (clock_t)drv_usectohz(1));
			mutex_enter(&ptgt->tgt_mutex);
			ptgt->tgt_tid = tid;
			ptgt->tgt_state |= SSFCP_TGT_BUSY;
			mutex_exit(&ptgt->tgt_mutex);
		}
	}

	SSFCP_DEBUG(2, (CE_CONT, pptr->ssfcpp_dip,
	    "D_ID=%x, sense=%x, status=%x\n",
	    fpkt->pkt_cmd_fhdr.d_id, sense->es_key,
	    rsp->fcp_u.fcp_status.scsi_status));

	return (DDI_SUCCESS);
}


/*
 * set up by ssfcp_icmd_scsi() to be called when a SCSI command
 * is received (i.e. INQUIRY of REPORT_LUN)
 *
 * this may call ssfcp_icmd_scsi() (again) to retry the command (if needed)
 */
static void
ssfcp_scsi_callback(fc_packet_t *fpkt)
{
	struct ssfcp_ipkt	*icmd = (struct ssfcp_ipkt *)
				    fpkt->pkt_ulp_private;
	struct fcp_rsp_info	fcp_rsp_err, *bep;
	struct ssfcp_port	*pptr;
	struct ssfcp_tgt	*ptgt;
	struct ssfcp_lun	*plun;
	struct fcp_rsp		response, *rsp;
	uchar_t			asc, ascq;
	caddr_t			sense_key = NULL;

	rsp = &response;
	bep = &fcp_rsp_err;
	SSFCP_CP_IN(fpkt->pkt_resp, rsp, fpkt->pkt_resp_acc,
	    sizeof (struct fcp_rsp));

	ptgt = icmd->ipkt_tgt;
	plun = icmd->ipkt_lun;
	pptr = ptgt->tgt_port;

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt != icmd->ipkt_link_cnt) {
		mutex_exit(&pptr->ssfcp_mutex);
		goto fail;
	}
	mutex_exit(&pptr->ssfcp_mutex);

	if (fpkt->pkt_state != FC_PKT_SUCCESS) {
		SSFCP_DEBUG(2, (CE_CONT, pptr->ssfcpp_dip,
		    "icmd failed with state=0x%x\n", fpkt->pkt_state));
		goto retry_cmd;
	}

	/* packet was successful */

	/* get a pointer the the response info */
	SSFCP_CP_IN(fpkt->pkt_resp + sizeof (struct fcp_rsp), bep,
	    fpkt->pkt_resp_acc, sizeof (struct fcp_rsp_info));

	if (rsp->fcp_u.fcp_status.scsi_status & STATUS_CHECK) {
		struct scsi_extended_sense 	sense_info;

		if (ssfcp_validate_fcp_response(rsp) != FC_SUCCESS) {
			goto retry_cmd;
		}

		SSFCP_CP_IN(fpkt->pkt_resp + sizeof (struct fcp_rsp) +
		    rsp->fcp_response_len, &sense_info, fpkt->pkt_resp_acc,
		    sizeof (struct scsi_extended_sense));

		if (sense_info.es_key < NUM_SENSE_KEYS + NUM_IMPL_SENSE_KEYS) {
			sense_key = sense_keys[sense_info.es_key];
		} else {
			sense_key = "Undefined";
		}
		asc = sense_info.es_add_code;
		ascq = sense_info.es_qual_code;
	}

	/*
	 * Handle the most likely error case first.
	 */
	if ((icmd->ipkt_opcode == SCMD_REPORT_LUN) &&
		(rsp->fcp_u.fcp_status.scsi_status & STATUS_CHECK)) {
		/*
		 * handle cases where report lun isn't supported by faking
		 * up our own REPORT_LUN response or UNIT ATTENTION
		 */
		if (ssfcp_check_reportlun(rsp, fpkt) == DDI_FAILURE)
			goto retry_cmd;
	}

	/* check for error */
	if (rsp->fcp_u.fcp_status.rsp_len_set &&
	    (bep->rsp_code != FCP_NO_FAILURE)) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!Response bit set for icmd");
		SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
		    "rsp_code=0x%x, rsp_len_set=0x%x\n",
		    bep->rsp_code, rsp->fcp_u.fcp_status.rsp_len_set));
		goto retry_cmd;			/* go try to send cmd */
	}

	/*
	 * response length is not set (implying no error),
	 * or response code is "no failure" (implying we ignore an error)
	 */

	if (rsp->fcp_u.fcp_status.scsi_status & STATUS_BUSY) {
		/*
		 * we got a BUSY or CHECK status returned -- setup the
		 * watch thread to fire down the request
		 */

		/* add this packet to the pkt list for this port */
		ssfcp_queue_ipkt(pptr, icmd);
		return;
	}

	if ((rsp->fcp_u.fcp_status.scsi_status) == STATUS_GOOD) {

		/* got good SCSI status back -- check for initial opcode */

		switch (icmd->ipkt_opcode) {
		case SCMD_INQUIRY:
			SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
			    "!INQUIRY to d_id=0x%x, lun=0x%x succeeded\n",
			    ptgt->tgt_d_id, plun->lun_num));
			ssfcp_handle_inquiry(fpkt, icmd);
			break;

		case SCMD_REPORT_LUN:
			ssfcp_handle_reportlun(fpkt, icmd);
			break;

		default:
			ssfcp_log(CE_WARN, NULL, "!Invalid SCSI opcode");
			goto fail;
		}

		return;		/* success */
	}

	/* try to resend the command */
retry_cmd:
	/* should we retry the command */
	if (++(icmd->ipkt_retries) < SSFCP_ICMD_RETRY_CNT) {
		/* retry the command */
		mutex_enter(&ptgt->tgt_mutex);
		if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
			mutex_exit(&ptgt->tgt_mutex);
			if (ssfcp_transport(pptr->ssfcpp_handle, fpkt,
			    1) == FC_SUCCESS) {
				return;
			}
		} else {
			mutex_exit(&ptgt->tgt_mutex);
		}
		/* resending command failed */
	} else {
		caddr_t buf, state, reason, action, expln;

		buf = kmem_zalloc(256, KM_NOSLEEP);
		if (buf == NULL) {
			goto fail;
		}

		switch (icmd->ipkt_opcode) {
		case SCMD_REPORT_LUN:
			(void) sprintf(buf, "!Report Lun to d_id=0x%%x"
			    " lun=0x%%x failed");
			break;

		case SCMD_INQUIRY:
			(void) sprintf(buf, "!Inquiry to d_id=0x%%x"
			    " lun=0x%%x failed");
			break;
		}

		if (fpkt->pkt_state == FC_PKT_SUCCESS) {
			if (rsp->fcp_u.fcp_status.scsi_status & STATUS_CHECK) {
				ASSERT(sense_key != NULL);

				if (ssfcp_validate_fcp_response(rsp) !=
				    FC_SUCCESS) {
					(void) sprintf(buf + strlen(buf),
					    ": Bad FCP response values"
					    " rsvd1=%%x, rsvd2=%%x,"
					    " sts-rsvd1=%%x, sts-rsvd2=%%x,"
					    " rsplen=%%x, senselen=%%x."
					    " Giving up\n");

					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					    buf, ptgt->tgt_d_id, plun->lun_num,
					    rsp->reserved_0, rsp->reserved_1,
					    rsp->fcp_u.fcp_status.reserved_0,
					    rsp->fcp_u.fcp_status.reserved_1,
					    rsp->fcp_response_len,
					    rsp->fcp_sense_len);
				} else {
					(void) sprintf(buf + strlen(buf),
					    ": sense key=%%s, ASC=%%x,"
					    " ASCQ=%%x. Giving up\n");

					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					    buf, ptgt->tgt_d_id, plun->lun_num,
					    sense_key, asc, ascq);
				}
			} else {
				(void) sprintf(buf + strlen(buf),
				    " : SCSI status=%%x. Giving up\n");

				ssfcp_log(CE_WARN, pptr->ssfcpp_dip, buf,
				    ptgt->tgt_d_id, plun->lun_num,
				    rsp->fcp_u.fcp_status.scsi_status);
			}
		} else {
			(void) fc_ulp_pkt_error(fpkt, &state, &reason,
			    &action, &expln);

			(void) sprintf(buf + strlen(buf), ": State:%%s,"
			    " Reason:%%s. Giving up\n");

			ssfcp_log(CE_WARN, pptr->ssfcpp_dip, buf,
			    ptgt->tgt_d_id, plun->lun_num, state, reason);
		}

		kmem_free(buf, 256);
	}

	/* too many retries or resending the command failed */
fail:
	/* an error */

	(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
	ssfcp_icmd_free(fpkt);
}


/*
 * called by ssfcp_scsi_callback to handle the response to an INQUIRY request
 */
static void
ssfcp_handle_inquiry(fc_packet_t *fpkt, struct ssfcp_ipkt *icmd)
{
	struct ssfcp_port	*pptr;
	struct ssfcp_lun	*plun;
	struct ssfcp_tgt	*ptgt;
	struct scsi_inquiry	inquiry, *ptr;


	ASSERT(icmd != NULL);

	pptr = icmd->ipkt_port;
	ptgt = icmd->ipkt_tgt;
	plun = icmd->ipkt_lun;

	ASSERT(fpkt != NULL);

	ptr = &inquiry;

	SSFCP_CP_IN(fpkt->pkt_data, ptr, fpkt->pkt_data_acc,
			sizeof (struct scsi_inquiry));
	ASSERT(ptr != NULL);

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_handle_inquiry: port=%d, tgt HA=0x%x, dtype=0x%x\n",
	    pptr->ssfcpp_instance, ptgt->tgt_hard_addr, ptr->inq_dtype));

	if ((ptr->inq_dtype != DTYPE_DIRECT) &&
	    (ptr->inq_dtype != DTYPE_ESI) &&
	    (ptr->inq_dtype != DTYPE_CHANGER) &&
	    (ptr->inq_dtype != DTYPE_SEQUENTIAL)) {
		/*
		 * Device type that FCP does not support currently
		 */
		ssfcp_log(CE_CONT, pptr->ssfcpp_dip,
		    "!Target 0x%x: Device type=0x%x not supported\n",
		    ptgt->tgt_d_id, ptr->inq_dtype);
		(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
		ssfcp_icmd_free(icmd->ipkt_fpkt);
		return;
	}

	/* a device that FCP does support */

	/*
	 * Make a case for all the devices that we want to
	 * support like MEDIA CHANGER devices. From the
	 * inquiry data we need to find which level of LUN
	 * addressing is used by the device. FIXIT
	 */

	plun->lun_type = ptr->inq_dtype;	/* save type of device */

	plun->lun_state &= ~(SSFCP_LUN_OFFLINE | SSFCP_LUN_MARK);
	(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
	ssfcp_icmd_free(fpkt);
}


/*
 * called by ssfcp_scsi_callback to handle returned REPORT_LUN data returned
 *
 * Note:
 *
 * Correct handling of REPORT_LUN *isn't* easy (see the SCC-2 standard
 * for details).
 *
 * The following code assumes that the storage device suports "eight byte"
 * LUN addressing and that it supports either Logical Unit or Peripheral
 * device addressing.  Anything else will be incorrectly handled.
 *
 */
static void
ssfcp_handle_reportlun(fc_packet_t *fpkt, struct ssfcp_ipkt *icmd)
{
	int			i;		/* loop index */
	int			num_luns;	/* for saving #luns */
	struct ssfcp_port	*pptr;
	struct ssfcp_tgt	*ptgt;
	struct fcp_reportlun_resp *report_lun;


	pptr = icmd->ipkt_port;
	ptgt = icmd->ipkt_tgt;

	if ((report_lun = kmem_zalloc(sizeof (struct fcp_reportlun_resp),
					KM_NOSLEEP)) == NULL) {
		(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
		return;
	}
	SSFCP_CP_IN(fpkt->pkt_data, report_lun, fpkt->pkt_data_acc,
			sizeof (struct fcp_reportlun_resp));

	/* get number of luns (which is supplied as LUNS * 8) */
	num_luns = report_lun->num_lun >> 3;
	ptgt->tgt_lun_cnt = num_luns;
	ptgt->tgt_tmp_cnt = num_luns;

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_handle_reportlun: port=%d, tgt HA=0x%x, %d LUN(s)\n",
	    pptr->ssfcpp_instance, ptgt->tgt_hard_addr, num_luns));

	/* scan each lun */
	for (i = 0; i < num_luns; i++) {
		uchar_t			*lun_string;
		struct ssfcp_lun	*plun;


		lun_string = (uchar_t *)&(report_lun->lun_string[i]);

		SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
		    "handle_reportlun: LUN ind=%d, LUN=%d, addr=0x%x\n", i,
		    lun_string[1], lun_string[0]));

		switch (lun_string[0]) {
		case SSFCP_LUN_ADDRESSING:
		case SSFCP_PD_ADDRESSING:
			/* see if this LUN is already allocated */
			if ((plun = ssfcp_get_lun(ptgt, lun_string[1])) ==
			    NULL) {
				plun = ssfcp_alloc_lun(ptgt);
				if (plun == NULL) {
					ssfcp_log(CE_WARN, NULL,
					    "!lun alloc failed");
					break;
				}
			}
			bcopy(lun_string, &(plun->lun_string), FCP_LUN_SIZE);
			plun->lun_num = lun_string[1];
			plun->lun_state |= SSFCP_LUN_BUSY | SSFCP_LUN_MARK;
			plun->lun_state &= ~SSFCP_LUN_OFFLINE;

			if (ssfcp_icmd_scsi(plun, NULL,
			    SCMD_INQUIRY) != DDI_SUCCESS) {
				ssfcp_log(CE_WARN, NULL,
				    "!failed to send INQUIRY");
			} else
				continue;
			break;

		case SSFCP_VOLUME_ADDRESSING:
			/* FALLTHROUGH */

		default:
			ssfcp_log(CE_WARN, NULL,
			    "!Unsupported LUN Addressing method "
			    "in response to REPORT_LUN");
			break;
		}

		/*
		 * each time through this loop we should decrement
		 * the tmp_cnt by one -- since we go through this loop
		 * one time for each LUN, the tmp_cnt should never be <=0
		 */
		(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
	}
	/* did we skip the for loop completely ?? */
	if (i == 0) {
		/*
		 * We should never hit this code path; But we
		 * did, cuz, we had a few problems (bugs)
		 */
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			"!FCP: Report Lun Retured No Luns \n");
		(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
	}
	/* clean up */
	kmem_free(report_lun, sizeof (struct fcp_reportlun_resp));
	ssfcp_icmd_free(fpkt);
}


/*
 * called internally to return a LUN given a target and a LUN number
 */
static struct ssfcp_lun *
ssfcp_get_lun(struct ssfcp_tgt *ptgt, uchar_t lun_num)
{
	struct ssfcp_lun	*plun;


	for (plun = ptgt->tgt_lun; plun != NULL; plun = plun->lun_next) {
		if (plun->lun_num == lun_num) {
			return (plun);		/* found it */
		}
	}
	return (NULL);				/* did not find it */
}


/*
 * handle finishing one target for ssfcp_finish_init
 *
 * return true (non-zero) if we want finish_init to continue with the
 * next target
 *
 * called with the port mutex held
 */
static int
ssfcp_finish_tgt(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt, int link_cnt)
{

	ASSERT(pptr != NULL);
	ASSERT(ptgt != NULL);

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "finish_tgt: HA/state = 0x%x/0x%x\n", ptgt->tgt_hard_addr,
	    ptgt->tgt_state));

	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	mutex_enter(&ptgt->tgt_mutex);
	ptgt->tgt_decremented = 0;
	if (!(ptgt->tgt_state & SSFCP_TGT_OFFLINE)) {

		/*
		 * tgt is not offline -- is it marked (i.e. needs
		 * to be offlined) ??
		 */
		if (ptgt->tgt_state & SSFCP_TGT_MARK) {
			/*
			 * this target not offline *and*
			 * marked
			 */
			ssfcp_offline_target(pptr, ptgt);
		} else if (ptgt->tgt_state & SSFCP_TGT_ON_DEMAND) {
			/*
			 * In this case user has requesting on demand node
			 * creation and we should wait for explicit ioctl
			 * to create the node.
			 */
			ASSERT(ptgt->tgt_state & SSFCP_TGT_BUSY);

		} else if (ptgt->tgt_state & SSFCP_TGT_BUSY) {
			/*
			 * tgt is not offline *and* is not marked
			 *
			 * clear the busy flag if it is set
			 */

			ptgt->tgt_state &= ~SSFCP_TGT_BUSY;

			/* create the LUNs */
			ssfcp_create_luns(ptgt, link_cnt);
		}
	}

	if (pptr->ssfcp_link_cnt != link_cnt) {
		/*
		 * oh oh -- another link reset has occurred
		 * while we were in here
		 */
		mutex_exit(&ptgt->tgt_mutex);
		return (0);			/* failure */
	}

	mutex_exit(&ptgt->tgt_mutex);

	/* success */
	return (1);
}


/*
 * this routine is called to finish port initialization
 *
 * Each port has a "temp" counter -- when a state change happens (e.g.
 * port online), the temp count is set to the number of devices in the map.
 * Then, as each device gets "discovered", the temp counter is decremented
 * by one.  When this count reaches zero we know that all of the devices
 * in the map have been discovered (or an error has occurred), so we can
 * then finish initializion -- which is done by this routine (well, this
 * and ssfcp-finish_tgt())
 *
 * acquires and releases the global mutex
 *
 * called with the port mutex owned
 */
void
ssfcp_finish_init(struct ssfcp_port *pptr, int link_cnt)
{
	struct ssfcp_tgt	*ptgt;
	int			i;


	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip, "finish_init: entering\n"));

	/* scan all nodes in our map */
	for (i = 0; i < SSFCP_NUM_HASH; i++) {
		for (ptgt = pptr->ssfcp_wwn_list[i];
		    ptgt != NULL;
		    ptgt = ptgt->tgt_next) {
			if (!ssfcp_finish_tgt(pptr, ptgt, link_cnt)) {
				/* another link reset happening ? */
				return;
			}
		}
	}

	/* port is now online */
	if ((pptr->ssfcp_state & SSFCP_STATE_ONLINING) &&
	    !(pptr->ssfcp_state & (SSFCP_STATE_SUSPENDED |
	    SSFCP_STATE_DETACHING))) {
		pptr->ssfcp_state = SSFCP_STATE_ONLINE;
	}

	/*
	 * check to see if we need to signal the possibly waiting
	 * tgt_init thread
	 */
	mutex_enter(&ssfcp_global_mutex);
	if (--ssfcp_count == 0) {
		/*
		 * we're the last instance that's done initializing, so
		 * signal possibly waiting thread
		 */
		cv_signal(&ssfcp_cv);
	}
	mutex_exit(&ssfcp_global_mutex);
}


/*
 * called from ssfcp_finish_init to create the LUNs for a target
 *
 * called with the port mutex owned
 */
static void
ssfcp_create_luns(struct ssfcp_tgt *ptgt, uint32_t link_cnt)
{
	struct ssfcp_lun	*plun;
	struct ssfcp_port	*pptr;
	uint32_t		tgt_id;


	ASSERT(ptgt != NULL);
	ASSERT(mutex_owned(&ptgt->tgt_mutex));

	/* get the port for this target -- for use below */
	pptr = ptgt->tgt_port;

	ASSERT(pptr != NULL);

	/* scan all LUNs for this target */
	for (plun = ptgt->tgt_lun; plun != NULL; plun = plun->lun_next) {

		/* is lun already offline ?? */
		if (plun->lun_state & SSFCP_LUN_OFFLINE) {
			continue;		/* look at next lun */
		}

		/* is lun already marked to be placed offline ?? */
		if (plun->lun_state & SSFCP_LUN_MARK) {
			SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
			    "ssfcp_create_luns: offlining marked LUN!\n"));
			ssfcp_offline_lun(plun);
			continue;
		}

		/* clean busy flag for this LUN */
		plun->lun_state &= ~SSFCP_LUN_BUSY;

		/* does lun not have a devinfo struct yet ?? */
		if (plun->lun_dip == NULL) {
			/* no devinfo yet -- get one */
			ASSERT(!(plun->lun_state & SSFCP_LUN_INIT));
			ssfcp_create_devinfo(plun, link_cnt);
			if (plun->lun_dip == NULL) {
				ssfcp_log(CE_WARN, NULL,
				    "!Create devinfo failed");
				plun->lun_state = SSFCP_LUN_OFFLINE;
				continue;
			}
		} else {
			/*
			 * We removed the target and lun property in
			 * ssfcp_offline_lun. Recreate them here.
			 */
			if ((!FC_TOP_EXTERNAL(pptr->ssfcpp_top)) &&
				(ptgt->tgt_hard_addr != 0))
				tgt_id = (uint32_t)ssfcp_alpa_to_switch
						[ptgt->tgt_hard_addr];
			else
				tgt_id = ptgt->tgt_hard_addr;

			if (ndi_prop_update_int(DDI_DEV_T_NONE,
				plun->lun_dip, TARGET_PROP, tgt_id)
						!= DDI_PROP_SUCCESS) {
				continue;
			}

			if (ndi_prop_update_int(DDI_DEV_T_NONE,
				plun->lun_dip, LUN_PROP, plun->lun_num)
					!= DDI_PROP_SUCCESS) {
				continue;
			}

			if (ndi_prop_update_int(DDI_DEV_T_NONE, plun->lun_dip,
				LINK_CNT_PROP, link_cnt) != DDI_PROP_SUCCESS) {
				continue;
			}
		}

		/* is lun already being initialized ?? */
		if ((plun->lun_state & SSFCP_LUN_INIT)) {
			continue;
		}

		/* initialize lun */
		plun->lun_state |= SSFCP_LUN_INIT;

		SSFCP_DEBUG(6, (CE_NOTE, pptr->ssfcpp_dip,
		    "create_luns: passing ONLINE elem to HP thread\n"));

		/* pass an ONLINE element to the hotplug thread */
		if (!ssfcp_pass_to_hp(pptr, plun, SSFCP_ONLINE)) {
			uint_t	ndi_count;

			/*
			 * oh oh: couldn't alloc memory -- just online
			 * device ourselves as a last ditch effort
			 */
			mutex_exit(&ptgt->tgt_mutex);

			mutex_enter(&pptr->ssfcp_hp_mutex);
			pptr->ssfcp_hp_nele++;
			mutex_exit(&pptr->ssfcp_hp_mutex);

			i_ndi_block_device_tree_changes(&ndi_count);

			mutex_enter(&plun->lun_mutex);
			if (plun->lun_dip != NULL &&
			    (plun->lun_state & SSFCP_LUN_INIT)) {
				mutex_exit(&plun->lun_mutex);

				(void) ndi_devi_online(plun->lun_dip,
				    NDI_ONLINE_ATTACH);
			} else {
				mutex_exit(&plun->lun_mutex);
			}

			i_ndi_allow_device_tree_changes(ndi_count);

			mutex_enter(&pptr->ssfcp_hp_mutex);
			pptr->ssfcp_hp_nele--;
			mutex_exit(&pptr->ssfcp_hp_mutex);

			mutex_enter(&ptgt->tgt_mutex);
		}

		/* go get next LUN */
	}
}


/*
 * called from ssfcp_creaet_luns to create the devinfo struct for a LUN
 *
 * enter and leave with target mutex held
 */
static void
ssfcp_create_devinfo(struct ssfcp_lun *plun, uint32_t link_cnt)
{
	struct ssfcp_tgt	*ptgt;
	dev_info_t		*cdip = NULL;
	struct ssfcp_port	*pptr;
	int			i;
	uint32_t		tgt_id;


	ASSERT(plun != NULL);
	ptgt = plun->lun_tgt;
	ASSERT(ptgt != NULL);
	ASSERT(mutex_owned(&ptgt->tgt_mutex));

	/* get port for this target */
	pptr = ptgt->tgt_port;

	ASSERT(pptr != NULL);

	/* scan through our devinfo type array, looking for a match */
	for (i = 0; i < ssfcp_num_children; i++) {
		if ((plun->lun_type & 0x1f) == ssfcp_children[i].dev_type) {
			break;			/* we found a match */
		}
	}
	if (i == ssfcp_num_children) {
		/* no match was found */
		ssfcp_log(CE_WARN, NULL, "!invalid device type=0x%x",
		    plun->lun_type);
		return;
	}
	ptgt->tgt_device_created = SSFCP_DEVICE_CREATED;
	mutex_exit(&ptgt->tgt_mutex);

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_create_devinfo: creating node for %s\n",
	    ssfcp_children[i].name));

	/* allocate a devinfo struct */
	if (ndi_devi_alloc(pptr->ssfcpp_dip, ssfcp_children[i].name,
	    DEVI_SID_NODEID, &cdip) != NDI_SUCCESS) {
		return;
	}

	/* set some properties for this node */
	if (ndi_prop_update_byte_array(DDI_DEV_T_NONE,
	    cdip, NODE_WWN_PROP, ptgt->tgt_node_wwn.raw_wwn, FC_WWN_SIZE) !=
	    DDI_PROP_SUCCESS) {
		goto fail;
	}
	if (ndi_prop_update_byte_array(DDI_DEV_T_NONE,
	    cdip, PORT_WWN_PROP, ptgt->tgt_port_wwn.raw_wwn, FC_WWN_SIZE) !=
	    DDI_PROP_SUCCESS) {
		goto fail;
	}
	if (ndi_prop_update_int(DDI_DEV_T_NONE, cdip, LINK_CNT_PROP,
	    link_cnt) != DDI_PROP_SUCCESS) {
		goto fail;
	}

	/*
	 * If there is no hard address - We might have to deal with
	 * that by using WWN - Having said that it is important to
	 * recognize this problem early so ssd can be informed of
	 * the right interconnect type.
	 */
	if ((!FC_TOP_EXTERNAL(pptr->ssfcpp_top)) &&
				(ptgt->tgt_hard_addr != 0))
		tgt_id = (uint32_t)ssfcp_alpa_to_switch[ptgt->tgt_hard_addr];
	else
		tgt_id = ptgt->tgt_hard_addr;

	if (ndi_prop_update_int(DDI_DEV_T_NONE, cdip, TARGET_PROP,
	    tgt_id) != DDI_PROP_SUCCESS) {
		goto fail;
	}

	if (ndi_prop_update_int(DDI_DEV_T_NONE, cdip, LUN_PROP,
	    plun->lun_num) != DDI_PROP_SUCCESS) {
		goto fail;
	}

	/* set the DIP for this LUN */
	mutex_enter(&ptgt->tgt_mutex);
	plun->lun_dip = cdip;
	mutex_exit(&ptgt->tgt_mutex);

	/* attach this devinfo struct to the device tree */
	if (ndi_devi_online(cdip, 0) != DDI_SUCCESS) {
		goto fail;
	}
	mutex_enter(&ptgt->tgt_mutex);
	return;

	/* a failure -- clean up */
fail:
	if (cdip != NULL) {
		(void) ndi_prop_remove(DDI_DEV_T_NONE, cdip,
		    NODE_WWN_PROP);
		(void) ndi_prop_remove(DDI_DEV_T_NONE, cdip,
		    PORT_WWN_PROP);
		(void) ndi_prop_remove(DDI_DEV_T_NONE, cdip,
		    LINK_CNT_PROP);
		(void) ndi_prop_remove(DDI_DEV_T_NONE, cdip,
		    TARGET_PROP);
		(void) ndi_prop_remove(DDI_DEV_T_NONE, cdip,
		    LUN_PROP);
		if (ndi_devi_free(cdip) != NDI_SUCCESS) {
			ssfcp_log(CE_WARN, NULL, "!ndi_devi_free failed");
			plun->lun_dip = NULL;
		}
	}
	mutex_enter(&ptgt->tgt_mutex);
}


/*
 * take a target offline by taking all of its LUNs offline
 */
/*ARGSUSED*/
static void
ssfcp_offline_target(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt)
{
	struct ssfcp_lun	*plun;


	ASSERT(mutex_owned(&ptgt->tgt_mutex));
	ASSERT(!(ptgt->tgt_state & SSFCP_TGT_OFFLINE));

	ptgt->tgt_state = SSFCP_TGT_OFFLINE;
	ptgt->tgt_pd_handle = NULL;
	for (plun = ptgt->tgt_lun; plun != NULL; plun = plun->lun_next) {
		if (!(plun->lun_state & SSFCP_LUN_OFFLINE)) {
			ssfcp_offline_lun(plun);
		}
	}
}


/*
 * take a LUN offline
 *
 * enters and leavs with the target mutex held, releasing it in the process
 *
 * allocates memory in non-sleep mode
 */
static void
ssfcp_offline_lun(struct ssfcp_lun *plun)
{
	struct ssfcp_port	*pptr;
	struct ssfcp_tgt	*ptgt = plun->lun_tgt;
	struct ssfcp_pkt	*cmd;		/* pkt cmd ptr */
	struct ssfcp_pkt	*ncmd = NULL;	/* next pkt ptr */
	struct ssfcp_pkt	*pcmd = NULL;	/* the previous command */
	struct ssfcp_pkt	*head = NULL;	/* head of our list */
	struct ssfcp_pkt	*tail = NULL;	/* tail of our list */
#ifdef	DEBUG
	int			cmds_found = 0;
#endif

	ASSERT(plun != NULL);
	ptgt = plun->lun_tgt;
	ASSERT(ptgt != NULL);

	ASSERT(mutex_owned(&ptgt->tgt_mutex));

	/* get a tmp ptr to the port struct */
	pptr = ptgt->tgt_port;

	ssfcp_log(CE_NOTE, pptr->ssfcpp_dip,
	    "!ssfcp_offline_lun: offlining HA/LUN/state 0x%x/%d/0x%x\n",
	    ptgt->tgt_hard_addr, plun->lun_num, plun->lun_state);

	/* set state to offline and clear the LUN init flag */
	plun->lun_state |= SSFCP_LUN_OFFLINE;
	plun->lun_state &= ~(SSFCP_LUN_INIT|SSFCP_LUN_BUSY|SSFCP_LUN_MARK);

	if (plun->lun_dip == NULL) {
		return;				/* no device to offline */
	}

	/* don't want to hold the mutex for these calls */
	mutex_exit(&ptgt->tgt_mutex);

	/* remove the target and LUN props */
	(void) ndi_prop_remove(DDI_DEV_T_NONE, plun->lun_dip,
	    TARGET_PROP);
	(void) ndi_prop_remove(DDI_DEV_T_NONE, plun->lun_dip,
	    LUN_PROP);

	/* if the flag is set then run callbacks */
	if (plun->lun_state & SSFCP_SCSI_LUN_TGT_INIT) {
		(void) ndi_event_run_callbacks(pptr->ssfcp_event_hdl,
		    plun->lun_dip, ssfcp_remove_eid, NULL, NDI_EVENT_NOPASS);
	}

	/*
	 * scan all of the command pkts for this port, moving pkts that
	 * match our LUN onto our own list (headed by "head")
	 */
	mutex_enter(&pptr->ssfcp_cmd_mutex);
	for (cmd = pptr->ssfcp_pkt_head; cmd != NULL; cmd = ncmd) {
		struct ssfcp_lun *tlun = ADDR2LUN(&cmd->cmd_pkt->pkt_address);

		if (tlun != plun) {
			/* this pkt has a different LUN */
			pcmd = cmd;
			ncmd = cmd->cmd_next;	/* set next command */
			continue;
		}
#ifdef	DEBUG
		cmds_found++;
#endif
		if (pcmd != NULL) {
			ASSERT(pptr->ssfcp_pkt_head != cmd);
			pcmd->cmd_next = cmd->cmd_next;
		} else {
			ASSERT(cmd == pptr->ssfcp_pkt_head);
			pptr->ssfcp_pkt_head = cmd->cmd_next;
		}

		if (cmd == pptr->ssfcp_pkt_tail) {
			pptr->ssfcp_pkt_tail = pcmd;
			if (pcmd)
				pcmd->cmd_next = NULL;
		}

		if (head == NULL) {
			head = tail = cmd;
		} else {
			ASSERT(tail != NULL);

			tail->cmd_next = cmd;
			tail = cmd;
		}
		ncmd = cmd->cmd_next;	/* set next command */
		cmd->cmd_next = NULL;
	}
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	SSFCP_DEBUG(6, (CE_NOTE, pptr->ssfcpp_dip,
	    "offline_lun: %d cmd(s) found\n", cmds_found));

	/*
	 * scan through the pkts we found, invalidating all packets
	 */
	for (cmd = head; cmd != NULL; cmd = ncmd) {
		struct scsi_pkt	*pkt = cmd->cmd_pkt;

		ncmd = cmd->cmd_next;
		ASSERT(pkt != NULL);

		/*
		 * We are going to mark the damn lun offline, Indicate the
		 * target driver not to requeue or retry this command (with
		 * a couple of lies saying that reason is CMD_CMPLT and
		 * status is STATUS_CONDITION_MET - whatever that means)
		 */
		pkt->pkt_reason = CMD_CMPLT;	/* lie */
		*(pkt->pkt_scbp) = STATUS_MET;	/* Whatever */
		pkt->pkt_statistics = 0;
		pkt->pkt_state = 0;

		/* reset cmd flags/state */
		cmd->cmd_flags &= ~CFLAG_IN_QUEUE;
		cmd->cmd_state = SSFCP_PKT_IDLE;

		/* ensure we have a packet completion routine, then call it */
		ASSERT(pkt->pkt_comp != NULL);

		if (pkt->pkt_comp) {
			(*pkt->pkt_comp) (pkt);
		}
	}

	SSFCP_DEBUG(6, (CE_NOTE, pptr->ssfcpp_dip,
	    "offline_lun: passing OFFLINE elem to HP thread\n"));

	/* pass an OFFLINE element to the hotplug thread */

	mutex_enter(&ptgt->tgt_mutex);
	if (!ssfcp_pass_to_hp(pptr, plun, SSFCP_OFFLINE)) {
		uint_t	ndi_count;

		/*
		 * oh oh: couldn't alloc memory -- just offline
		 * device ourselves as a last ditch effort
		 */
		mutex_exit(&ptgt->tgt_mutex);

		mutex_enter(&pptr->ssfcp_hp_mutex);
		pptr->ssfcp_hp_nele++;
		mutex_exit(&pptr->ssfcp_hp_mutex);

		i_ndi_block_device_tree_changes(&ndi_count);

		mutex_enter(&plun->lun_mutex);
		if (plun->lun_dip == NULL ||
		    !(plun->lun_state & SSFCP_LUN_INIT)) {
			mutex_exit(&plun->lun_mutex);

			i_ndi_allow_device_tree_changes(ndi_count);

			mutex_enter(&pptr->ssfcp_hp_mutex);
			pptr->ssfcp_hp_nele--;
			mutex_exit(&pptr->ssfcp_hp_mutex);

			mutex_enter(&ptgt->tgt_mutex);

			return;
		}
		mutex_exit(&plun->lun_mutex);

		(void) ndi_devi_offline(plun->lun_dip, 0);

		i_ndi_allow_device_tree_changes(ndi_count);

		mutex_enter(&pptr->ssfcp_hp_mutex);
		pptr->ssfcp_hp_nele--;
		mutex_exit(&pptr->ssfcp_hp_mutex);

		mutex_enter(&ptgt->tgt_mutex);
	}
}


/*
 * the pkt_comp callback for command packets
 */
static void
ssfcp_cmd_callback(fc_packet_t *fpkt)
{
	struct ssfcp_pkt *cmd = (struct ssfcp_pkt *)fpkt->pkt_ulp_private;
	struct scsi_pkt *pkt = cmd->cmd_pkt;
	struct ssfcp_port *pptr = ADDR2SSFCP(&pkt->pkt_address);

	ASSERT(cmd->cmd_state != SSFCP_PKT_IDLE);

	if (cmd->cmd_state == SSFCP_PKT_IDLE) {
		cmn_err(CE_PANIC, "Packet already completed %p",
		    (void *)cmd);
	}

	/*
	 * Watch thread should be freeing the packet, ignore the pkt.
	 */
	if (cmd->cmd_state == SSFCP_PKT_ABORTING) {
		ssfcp_log(CE_CONT, pptr->ssfcpp_dip,
			"!FCP: Pkt completed while aborting \n");
		return;
	}
	cmd->cmd_state = SSFCP_PKT_IDLE;

	ssfcp_complete_pkt(fpkt);

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	pptr->ssfcp_ncmds--;
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	if (pkt->pkt_comp != NULL) {
		(*pkt->pkt_comp)(pkt);
	}
}


/* a shortcut for defining debug messages below */
#ifdef	DEBUG
#define	SSFCP_DMSG1(s)		msg1 = s
#else
#define	SSFCP_DMSG1(s)		/* do nothing */
#endif

static void
ssfcp_complete_pkt(fc_packet_t *fpkt)
{
	struct ssfcp_pkt *cmd = (struct ssfcp_pkt *)fpkt->pkt_ulp_private;
	struct scsi_pkt *pkt = cmd->cmd_pkt;
	struct ssfcp_port *pptr = ADDR2SSFCP(&pkt->pkt_address);
	struct ssfcp_lun *plun = ADDR2LUN(&pkt->pkt_address);
	struct ssfcp_tgt *ptgt;
	struct fcp_rsp response, *rsp;
#ifdef	DEBUG
	char	*msg1 = NULL;
#endif	/* DEBUG */

	rsp = &response;

	ptgt = plun->lun_tgt;
	ASSERT(ptgt != NULL);

	if (fpkt->pkt_state == FC_PKT_SUCCESS) {
		SSFCP_CP_IN(fpkt->pkt_resp, rsp, fpkt->pkt_resp_acc,
			sizeof (struct fcp_rsp));

		pkt->pkt_state = STATE_GOT_BUS | STATE_GOT_TARGET |
		    STATE_SENT_CMD | STATE_GOT_STATUS;

		pkt->pkt_resid = 0;

		if (cmd->cmd_flags & CFLAG_DMAVALID) {
			pkt->pkt_state |= STATE_XFERRED_DATA;
		}

		if ((pkt->pkt_scbp != NULL) && ((*(pkt->pkt_scbp) =
		    rsp->fcp_u.fcp_status.scsi_status) != STATUS_GOOD)) {
			/*
			 * The next two checks make sure that if there
			 * is no sense data or a valid response and
			 * the command came back with check condition,
			 * the command should be retried.
			 */
			if (!rsp->fcp_u.fcp_status.rsp_len_set &&
			    !rsp->fcp_u.fcp_status.sense_len_set) {
				pkt->pkt_state &= ~STATE_XFERRED_DATA;
				pkt->pkt_resid = cmd->cmd_dmacount;
			}
		}

		/*
		 * Update the transfer resid, if appropriate
		 */
		if (rsp->fcp_u.fcp_status.resid_over ||
		    rsp->fcp_u.fcp_status.resid_under) {
			pkt->pkt_resid = rsp->fcp_resid;
		}

		/*
		 * First see if we got a FCP protocol error.
		 */
		if (rsp->fcp_u.fcp_status.rsp_len_set) {
			struct fcp_rsp_info	fcp_rsp_err, *bep;
			bep = &fcp_rsp_err;

			if (ssfcp_validate_fcp_response(rsp) != FC_SUCCESS) {
				pkt->pkt_reason = CMD_CMPLT;
				*(pkt->pkt_scbp) = STATUS_CHECK;

				ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
				    "!SCSI command to d_id=0x%x lun=0x%x"
				    " failed, Bad FCP response values:"
				    " rsvd1=%x, rsvd2=%x, sts-rsvd1=%x,"
				    " sts-rsvd2=%x, rsplen=%x, senselen=%x\n",
				    ptgt->tgt_d_id, plun->lun_num,
				    rsp->reserved_0, rsp->reserved_1,
				    rsp->fcp_u.fcp_status.reserved_0,
				    rsp->fcp_u.fcp_status.reserved_1,
				    rsp->fcp_response_len, rsp->fcp_sense_len);

				return;
			}

			SSFCP_CP_IN(fpkt->pkt_resp + sizeof (struct fcp_rsp),
				bep, fpkt->pkt_resp_acc,
				sizeof (struct fcp_rsp_info));

			if (bep->rsp_code != FCP_NO_FAILURE) {

				pkt->pkt_reason = CMD_TRAN_ERR;

				switch (bep->rsp_code) {
				case FCP_CMND_INVALID:
					SSFCP_DMSG1("FCP_RSP FCP_CMND "
					    "fields invalid");
					break;
				case FCP_TASK_MGMT_NOT_SUPPTD:
					SSFCP_DMSG1("FCP_RSP Task Management"
					    "Function Not Supported");
					break;
				case FCP_TASK_MGMT_FAILED:
					SSFCP_DMSG1("FCP_RSP Task Management "
					    "Function Failed");
					break;
				case FCP_DATA_RO_MISMATCH:
					SSFCP_DMSG1(
					    "FCP_RSP FCP_DATA RO mismatch"
					    " with FCP_XFER_RDY DATA_RO");
					break;
				case FCP_DL_LEN_MISMATCH:
					SSFCP_DMSG1("FCP_RSP FCP_DATA length "
					    "different than BURST_LEN");
					break;
				default:
					SSFCP_DMSG1(
					    "FCP_RSP invalid RSP_CODE");
					break;
				}
			}
		}

		/*
		 * See if we got a SCSI error with sense data
		 */
		if (rsp->fcp_u.fcp_status.sense_len_set) {
			uchar_t rqlen = (uchar_t)min(rsp->fcp_sense_len,
			    sizeof (struct scsi_extended_sense));
			caddr_t sense = (caddr_t)fpkt->pkt_resp +
				sizeof (struct fcp_rsp) +
				rsp->fcp_response_len;
			struct scsi_arq_status *arq;
			struct scsi_extended_sense *sensep =
				(struct scsi_extended_sense *)sense;

			if (ssfcp_validate_fcp_response(rsp) != FC_SUCCESS) {
				pkt->pkt_reason = CMD_CMPLT;
				*(pkt->pkt_scbp) = STATUS_CHECK;

				ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
				    "!SCSI command to d_id=0x%x lun=0x%x"
				    " failed, Bad FCP response values:"
				    " rsvd1=%x, rsvd2=%x, sts-rsvd1=%x,"
				    " sts-rsvd2=%x, rsplen=%x, senselen=%x\n",
				    ptgt->tgt_d_id, plun->lun_num,
				    rsp->reserved_0, rsp->reserved_1,
				    rsp->fcp_u.fcp_status.reserved_0,
				    rsp->fcp_u.fcp_status.reserved_1,
				    rsp->fcp_response_len, rsp->fcp_sense_len);

				return;
			}

			if (rsp->fcp_u.fcp_status.scsi_status == STATUS_CHECK) {
				timeout_id_t	tid;
				if (sensep->es_key == KEY_UNIT_ATTENTION &&
				    sensep->es_add_code == 0x3f &&
				    sensep->es_qual_code == 0x0e) {
					ssfcp_log(CE_NOTE, pptr->ssfcpp_dip,
					    "!FCP: Report Lun Has Changed\n");
					if (ptgt->tgt_tid == NULL) {
						tid = timeout(
						    ssfcp_reconfigure_luns,
						    (caddr_t)ptgt,
						    drv_usectohz(1));
						mutex_enter(&ptgt->tgt_mutex);
						ptgt->tgt_tid = tid;
						ptgt->tgt_state |=
						    SSFCP_TGT_BUSY;
						mutex_exit(&ptgt->tgt_mutex);
					}
				}
			}

			if ((pkt->pkt_scbp != NULL) &&
			    (cmd->cmd_scblen >=
				sizeof (struct scsi_arq_status))) {
				pkt->pkt_state |= STATE_ARQ_DONE;

				arq = (struct scsi_arq_status *)pkt->pkt_scbp;
				/*
				 * copy out sense information
				 */
				SSFCP_CP_IN(sense, (caddr_t)&arq->sts_sensedata,
				    fpkt->pkt_resp_acc, rqlen);

				arq->sts_rqpkt_resid =
				    sizeof (struct scsi_extended_sense) -
					rqlen;
				*((uchar_t *)&arq->sts_rqpkt_status) =
				    STATUS_GOOD;
				arq->sts_rqpkt_reason = 0;
				arq->sts_rqpkt_statistics = 0;
				arq->sts_rqpkt_state = STATE_GOT_BUS |
				    STATE_GOT_TARGET | STATE_SENT_CMD |
				    STATE_GOT_STATUS | STATE_ARQ_DONE |
				    STATE_XFERRED_DATA;
			}
		}
	} else {
		/*
		 * Work harder to translate errors into target driver
		 * understandable ones. Note with despair that the target
		 * drivers don't decode pkt_state and pkt_reason exhaustively
		 * They resort to using the big hammer most often, which
		 * may not get fixed in the life time of this driver.
		 */
		pkt->pkt_state = 0;
		pkt->pkt_statistics = 0;

		switch (fpkt->pkt_state) {
		case FC_PKT_TRAN_ERROR:
			switch (fpkt->pkt_reason) {
			case FC_REASON_OVERRUN:
				pkt->pkt_reason = CMD_CMD_OVR;
				pkt->pkt_statistics |= STAT_ABORTED;
				break;

			case FC_REASON_XCHG_BSY:
			{
				caddr_t ptr;

				pkt->pkt_reason = CMD_CMPLT;	/* Lie */

				ptr = (caddr_t)pkt->pkt_scbp;
				if (ptr) {
					*ptr = STATUS_BUSY;
				}
				break;
			}

			case FC_REASON_ABORTED:
				pkt->pkt_reason = CMD_TRAN_ERR;
				pkt->pkt_statistics |= STAT_ABORTED;
				break;

			case FC_REASON_ABORT_FAILED:
				pkt->pkt_reason = CMD_ABORT_FAIL;
				break;

			case FC_REASON_NO_SEQ_INIT:
			case FC_REASON_CRC_ERROR:
				pkt->pkt_reason = CMD_TRAN_ERR;
				pkt->pkt_statistics |= STAT_ABORTED;
				break;
			default:
				pkt->pkt_reason = CMD_TRAN_ERR;
				break;
			}
			break;

		case FC_PKT_PORT_OFFLINE:
			SSFCP_DMSG1("Fibre Channel Offline");
			mutex_enter(&ptgt->tgt_mutex);
			if (!(plun->lun_state & SSFCP_LUN_OFFLINE)) {
				plun->lun_state |=
				    (SSFCP_LUN_BUSY | SSFCP_LUN_MARK);
			}
			mutex_exit(&ptgt->tgt_mutex);
			(void) ndi_event_run_callbacks(pptr->ssfcp_event_hdl,
			    plun->lun_dip, ssfcp_remove_eid, NULL,
			    NDI_EVENT_NOPASS);
			pkt->pkt_reason = CMD_TRAN_ERR;
			pkt->pkt_statistics |= STAT_BUS_RESET;
			break;

		case FC_PKT_TRAN_BSY:
			/*
			 * Use the ssd Qfull handling here.
			 */
			*pkt->pkt_scbp = STATUS_INTERMEDIATE;
			pkt->pkt_state = STATE_GOT_BUS;
			break;

		case FC_PKT_TIMEOUT:
			SSFCP_DMSG1("Cmd Timeout");
			pkt->pkt_reason = CMD_TIMEOUT;
			if (fpkt->pkt_reason == FC_REASON_ABORTED) {
				pkt->pkt_statistics |= STAT_ABORTED;
			} else {
				pkt->pkt_statistics |= STAT_TIMEOUT;
			}
			break;

		case FC_PKT_LOCAL_RJT:
			switch (fpkt->pkt_reason) {
			case FC_REASON_OFFLINE:
				mutex_enter(&ptgt->tgt_mutex);
				if (!(plun->lun_state & SSFCP_LUN_OFFLINE)) {
					plun->lun_state |=
					    (SSFCP_LUN_BUSY | SSFCP_LUN_MARK);
				}
				mutex_exit(&ptgt->tgt_mutex);
				(void) ndi_event_run_callbacks(
				    pptr->ssfcp_event_hdl, plun->lun_dip,
				    ssfcp_remove_eid, NULL, NDI_EVENT_NOPASS);
				pkt->pkt_reason = CMD_TRAN_ERR;
				pkt->pkt_statistics |= STAT_BUS_RESET;
				break;

			case FC_REASON_NOMEM:
			case FC_REASON_QFULL:
			{
				caddr_t ptr;

				pkt->pkt_reason = CMD_CMPLT;	/* Lie */
				ptr = (caddr_t)pkt->pkt_scbp;
				if (ptr) {
					*ptr = STATUS_BUSY;
				}
				break;
			}

			case FC_REASON_DMA_ERROR:
				pkt->pkt_reason = CMD_DMA_DERR;
				pkt->pkt_statistics |= STAT_ABORTED;
				break;

			case FC_REASON_NO_CONNECTION:
			case FC_REASON_UNSUPPORTED:
			case FC_REASON_ILLEGAL_REQ:
			case FC_REASON_BAD_SID:
			case FC_REASON_DIAG_BUSY:
			case FC_REASON_FCAL_OPN_FAIL:
			case FC_REASON_BAD_XID:
			default:
				pkt->pkt_reason = CMD_TRAN_ERR;
				pkt->pkt_statistics |= STAT_ABORTED;
				break;

			}
			break;

		case FC_PKT_NPORT_RJT:
		case FC_PKT_FABRIC_RJT:
		case FC_PKT_NPORT_BSY:
		case FC_PKT_FABRIC_BSY:
		default:
			SSFCP_DEBUG(3, (CE_WARN, pptr->ssfcpp_dip,
			    "FC Status 0x%x, reason 0x%x",
			    fpkt->pkt_state, fpkt->pkt_reason));
			pkt->pkt_reason = CMD_TRAN_ERR;
			pkt->pkt_statistics |= STAT_ABORTED;
			break;
		}

		SSFCP_DEBUG(2, (CE_WARN, pptr->ssfcpp_dip,
		    "!FC error on cmd=%p target=0x%x: pkt state=0x%x "
		    " pkt reason=0x%x", cmd, ptgt->tgt_d_id, fpkt->pkt_state,
		    fpkt->pkt_reason));
	}

#ifdef	DEBUG
	if (msg1) {
		SSFCP_DEBUG(2, (CE_WARN, pptr->ssfcpp_dip,
		    "!Transport error on cmd=%p target=0x%x: %s", cmd,
		    pkt->pkt_address.a_target, msg1));
	}
#endif /* DEBUG */
}


static int
ssfcp_validate_fcp_response(struct fcp_rsp *rsp)
{
	if (rsp->reserved_0 || rsp->reserved_1 ||
	    rsp->fcp_u.fcp_status.reserved_0 ||
	    rsp->fcp_u.fcp_status.reserved_1) {
		return (FC_FAILURE);
	}

	if (rsp->fcp_response_len > (FCP_MAX_RSP_IU_SIZE -
	    sizeof (struct fcp_rsp))) {
		return (FC_FAILURE);
	}

	if (rsp->fcp_sense_len > (FCP_MAX_RSP_IU_SIZE -
	    rsp->fcp_response_len - sizeof (struct fcp_rsp))) {
		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}


/*ARGSUSED*/
static void
ssfcp_device_changed(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt,
    fc_portmap_t *map_entry)
{
	/* XXX: NOT YET IMPLEMENTED */
}

static struct ssfcp_lun *
ssfcp_alloc_lun(struct ssfcp_tgt *ptgt)
{
	struct ssfcp_lun *plun;

	plun = kmem_zalloc(sizeof (struct ssfcp_lun), KM_NOSLEEP);
	if (plun != NULL) {
		plun->lun_tgt = ptgt;
		mutex_enter(&ptgt->tgt_mutex);
		plun->lun_next = ptgt->tgt_lun;
		ptgt->tgt_lun = plun;
		mutex_exit(&ptgt->tgt_mutex);
		mutex_init(&plun->lun_mutex, NULL, MUTEX_DRIVER, NULL);
	}
	return (plun);
}


static void
ssfcp_dealloc_lun(struct ssfcp_lun *plun)
{
	if (plun->lun_dip) {
		(void) ndi_prop_remove_all(plun->lun_dip);
		(void) ndi_devi_free(plun->lun_dip);
	}
	mutex_destroy(&plun->lun_mutex);
	kmem_free(plun, sizeof (*plun));
}


static struct ssfcp_tgt *
ssfcp_alloc_tgt(struct ssfcp_port *pptr, fc_portmap_t *map_entry, int link_cnt)
{
	int			hash;
	uchar_t			*wwn;
	struct ssfcp_tgt 	*ptgt;

	ptgt = kmem_zalloc(sizeof (*ptgt), KM_NOSLEEP);

	if (ptgt != NULL) {
		mutex_enter(&pptr->ssfcp_mutex);
		if (link_cnt != pptr->ssfcp_link_cnt) {
			/*
			 * oh oh -- another link reset
			 * in progress -- give up
			 */
			mutex_exit(&pptr->ssfcp_mutex);
			kmem_free(ptgt, sizeof (*ptgt));
			ptgt = NULL;
		} else {
			/* add new target entry to the port's hash list */
			wwn = (uchar_t *)&map_entry->map_pwwn;
			hash = SSFCP_HASH(wwn);

			ptgt->tgt_next = pptr->ssfcp_wwn_list[hash];
			pptr->ssfcp_wwn_list[hash] = ptgt;

			mutex_exit(&pptr->ssfcp_mutex);
			mutex_init(&ptgt->tgt_mutex, NULL, MUTEX_DRIVER, NULL);
		}
	}
	return (ptgt);
}

static void
ssfcp_dealloc_tgt(struct ssfcp_tgt *ptgt)
{
	mutex_destroy(&ptgt->tgt_mutex);
	kmem_free(ptgt, sizeof (*ptgt));
}

static void
ssfcp_queue_ipkt(struct ssfcp_port *pptr, struct ssfcp_ipkt *icmd)
{
	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt != icmd->ipkt_link_cnt) {
		ssfcp_icmd_free(icmd->ipkt_fpkt);
		mutex_exit(&pptr->ssfcp_mutex);
		return;
	}
	icmd->ipkt_timeout = ssfcp_watchdog_time + SSFCP_ICMD_TIMEOUT;
	if (pptr->ssfcp_ipkt_list != NULL) {
		/* add pkt to front of doubly-linked list */
		pptr->ssfcp_ipkt_list->ipkt_prev = icmd;
		icmd->ipkt_next = pptr->ssfcp_ipkt_list;
		pptr->ssfcp_ipkt_list = icmd;
		icmd->ipkt_prev = NULL;
	} else {
		/* this is the first/only pkt on the list */
		pptr->ssfcp_ipkt_list = icmd;
		icmd->ipkt_next = NULL;
		icmd->ipkt_prev = NULL;
	}
	mutex_exit(&pptr->ssfcp_mutex);
}

int
ssfcp_transport(opaque_t port_handle, fc_packet_t *fpkt, int internal)
{
	int 			rval;

	rval = fc_ulp_transport(port_handle, fpkt);

	/*
	 * LUN isn't marked BUSY or OFFLINE, so we got here to transport
	 * a command, if the underlying modules see that there is a state
	 * change, or if a port is OFFLINE, that means, that state change
	 * hasn't reached FCP yet, so re-queue the command for deferred
	 * submission.
	 */
	if (rval == FC_STATEC_BUSY || rval == FC_OFFLINE)  {
		/*
		 * Defer packet re-submission. Life hang is possible on
		 * internal commands if the port driver sends FC_STATEC_BUSY
		 * for ever, but that shouldn't happen in a good environment.
		 * Limiting re-transport for internal commands is probably a
		 * good idea..
		 * A race condition can happen when a port sees barrage of
		 * link transitions offline to online. If the FCTL has
		 * returned FC_STATEC_BUSY or FC_OFFLINE then none of the
		 * internal commands should be queued to do the discovery.
		 * The race condition is when an online comes and FCP starts
		 * its inernal discovery and the link goes offline. It is
		 * possible that the statec_callback has not reached FCP
		 * and FCP is carrying on with its internal discovery.
		 * FC_STATEC_BUSY or FC_OFFLINE will be the first indication
		 * that the link has gone offline. At this point FCP should
		 * drop all the internal commands and wait for the
		 * statec_callback. It will be facilitated by incrementing
		 * ssfcp_link_cnt.
		 *
		 * For external commands, the (FC)pkt_timeout is decremented
		 * by the QUEUE Delay added by our driver, Care is taken to
		 * ensure that it doesn't become zero (zero means no timeout)
		 * If the time expires right inside driver queue itself,
		 * the watch thread will return it to the original caller
		 * indicating that the command has timed-ou.
		 */

		if (internal) {
			struct ssfcp_ipkt	*icmd;
			struct ssfcp_port	*pptr;

			icmd = (struct ssfcp_ipkt *)fpkt->pkt_ulp_private;
			pptr = icmd->ipkt_port;
			mutex_enter(&pptr->ssfcp_mutex);
			pptr->ssfcp_link_cnt++;
			/*
			 * To get the tgt_change_cnt updated.
			 */
			ssfcp_update_state(pptr, (SSFCP_LUN_BUSY |
			    SSFCP_LUN_MARK));
			mutex_exit(&pptr->ssfcp_mutex);
		} else {
			struct ssfcp_pkt *cmd;
			struct ssfcp_port *pptr;

			cmd = (struct ssfcp_pkt *)fpkt->pkt_ulp_private;
			cmd->cmd_state = SSFCP_PKT_IDLE;
			pptr = ADDR2SSFCP(&cmd->cmd_pkt->pkt_address);

			ssfcp_queue_pkt(pptr, cmd);
			rval = FC_SUCCESS;
		}
	}

	return (rval);
}


/*VARARGS3*/
void
ssfcp_log(int level, dev_info_t *dip, const char *fmt, ...)
{
	char		buf[256];
	va_list		ap;

	if (dip == NULL) {
		dip = ssfcp_global_dip;
	}

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	scsi_log(dip, "fcp", level, buf);
}


/*
 * called from ssfcp_port_attach() to attach a port
 *
 * return DDI_* success/failure status
 *
 * acquires and releases the global mutex
 *
 * may acquire and release the hotplug mutex for this port
 *
 * initializes all of the per-port mutexes
 *
 * this routine can sleep
 */
/*ARGSUSED*/
int
ssfcp_handle_port_attach(opaque_t ulph, fc_ulp_port_info_t *pinfo,
    uint32_t s_id, int instance)
{
	int		res = DDI_FAILURE; /* default result */
	scsi_hba_tran_t	*tran;
	int		mutex_initted = FALSE;
	int		hba_attached = FALSE;
	int		soft_state_linked = FALSE;
	int		count_incremented = FALSE;
	struct ssfcp_port *pptr;		/* port state ptr */
	char		cache_name_buf[64];
	fc_portmap_t	*tmp_list = NULL;	/* temp portmap list */
	uint32_t	max_cnt;		/* cnt for portmap */

	/*
	 * this port instance attaching for the first time (or after
	 * being detached before)
	 */
	SSFCP_DEBUG(2, (CE_NOTE, NULL, "port attach: for port %d\n", instance));

	if (ddi_soft_state_zalloc(ssfcp_softstate, instance) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "fcp: Softstate struct alloc failed"
		    "parent dip: %p; instance: %d", (void *)pinfo->port_dip,
		    instance);
		return (res);
	}

	if ((pptr = ddi_get_soft_state(ssfcp_softstate, instance)) == NULL) {
		/* this shouldn't happen */
		cmn_err(CE_WARN, "fcp: bad soft state");
		return (res);
	}

	/*
	 * Make a copy of ulp_port_info as fctl allocates
	 * a temp struct.
	 */
	(void) ssfcp_cp_pinfo(pptr, pinfo);

	pptr->ssfcpp_sid = s_id;
	pptr->ssfcpp_instance = instance;
	pptr->ssfcp_state = SSFCP_STATE_INIT;

	/* set up our cmd/response buffer pool */
	if ((ssfcp_add_cr_pool(pptr) != DDI_SUCCESS)) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: failed to allocate cr pool", instance);
		ddi_soft_state_free(ssfcp_softstate, instance);
		return (res);
	}
	(void) sprintf(cache_name_buf, "fcp%d_cache", instance);

	pptr->ssfcp_pkt_cache = kmem_cache_create(cache_name_buf,
	    pptr->ssfcpp_priv_pkt_len + sizeof (struct ssfcp_pkt),
		8, ssfcp_kmem_cache_constructor,
		ssfcp_kmem_cache_destructor, NULL, (void *)pptr, NULL, 0);
	if (pptr->ssfcp_pkt_cache == NULL) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: failed to allocate ssfcp_pkt cache", instance);
	}

	mutex_init(&pptr->ssfcp_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&pptr->ssfcp_hp_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&pptr->ssfcp_cr_mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&pptr->ssfcp_cr_cv, NULL, CV_DRIVER, NULL);
	cv_init(&pptr->ssfcp_hp_cv, NULL, CV_DRIVER, NULL);
	mutex_initted++;

	/* set up SCSI HBA structs */
	mutex_enter(&ssfcp_global_mutex);
	if (!ssfcp_hba_init_done) {
		ssfcp_hba_init_done = TRUE;
		mutex_exit(&ssfcp_global_mutex);
		/* we're the first port attach -- set up SCSA */
		if (scsi_hba_init(&pptr->ssfcpp_linkage) != 0) {
			scsi_hba_fini(&pptr->ssfcpp_linkage);
			goto fail;
		}
	} else {
		mutex_exit(&ssfcp_global_mutex);
	}

	/* allocate a transport structure */
	if ((tran = scsi_hba_tran_alloc(pptr->ssfcpp_dip, 0)) == NULL) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: scsi_hba_tran_alloc failed", instance);
		goto fail;
	}

	/* link in the transport structure then fill it in */
	pptr->ssfcp_tran = tran;
	tran->tran_hba_private		= pptr;
	tran->tran_tgt_private		= NULL;
	tran->tran_tgt_init		= ssfcp_scsi_tgt_init;
	tran->tran_tgt_probe		= NULL;
	tran->tran_tgt_free		= ssfcp_scsi_tgt_free;
	tran->tran_start		= ssfcp_start;
	tran->tran_abort		= ssfcp_abort;
	tran->tran_reset		= ssfcp_reset;
	tran->tran_getcap		= ssfcp_getcap;
	tran->tran_setcap		= ssfcp_setcap;
	tran->tran_init_pkt		= ssfcp_scsi_init_pkt;
	tran->tran_destroy_pkt		= ssfcp_scsi_destroy_pkt;
	tran->tran_dmafree		= ssfcp_scsi_dmafree;
	tran->tran_sync_pkt		= ssfcp_scsi_sync_pkt;
	tran->tran_reset_notify		= ssfcp_scsi_reset_notify;

	/*
	 * register event notification routines with scsa
	 * ????? to use frits event notification. -- FIXIT
	 */
	tran->tran_get_eventcookie	= ssfcp_bus_get_eventcookie;
	tran->tran_add_eventcall	= ssfcp_bus_add_eventcall;
	tran->tran_remove_eventcall	= ssfcp_bus_remove_eventcall;
	tran->tran_post_event		= ssfcp_bus_post_event;

	/*
	 * allocate an ndi event handle
	 *
	 * first copy ndi_events to a private space since
	 * ndi_event_allochdl() will fill in the event cookies
	 */
	pptr->ssfcp_event_defs = (ndi_event_definition_t *)
		kmem_zalloc(sizeof (ssfcp_event_defs), KM_SLEEP);

	bcopy(ssfcp_event_defs, pptr->ssfcp_event_defs,
	    sizeof (ssfcp_event_defs));

	(void) ndi_event_alloc_hdl(pptr->ssfcpp_dip, NULL,
	    &pptr->ssfcp_event_hdl, NDI_SLEEP);

	pptr->ssfcp_events.ndi_events_version = NDI_EVENTS_REV0;
	pptr->ssfcp_events.ndi_n_events = SSFCP_N_NDI_EVENTS;
	pptr->ssfcp_events.ndi_event_set = pptr->ssfcp_event_defs;

	if (ndi_event_bind_set(pptr->ssfcp_event_hdl,
	    &pptr->ssfcp_events, NDI_SLEEP) != NDI_SUCCESS) {
		goto fail;
	}

	/* set up global event cookies */
	ssfcp_insert_eid = ndi_event_getcookie(FCAL_INSERT_EVENT);
	ssfcp_remove_eid = ndi_event_getcookie(FCAL_REMOVE_EVENT);

	/*
	 * if sun4d or sun4m we do not need to do the following two
	 * steps
	 *
	 * this is because, on these architectures, the WWN property
	 * does not exist
	 *
	 * XXX: is this right ???
	 */
	if ((strcmp(utsname.machine, "sun4d") != 0) ||
	    (strcmp(utsname.machine, "sun4m") != 0)) {
		/* not sun4d or sun4m */
		tran->tran_get_name = ssfcp_scsi_get_name;
		tran->tran_get_bus_addr = ssfcp_scsi_get_bus_addr;
	}

	if (scsi_hba_attach_setup(pptr->ssfcpp_dip,
	    &pptr->ssfcpp_dma_attr, tran,
	    SCSI_HBA_TRAN_CLONE) != DDI_SUCCESS) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: scsi_hba_attach_setup failed", instance);
		goto fail;
	}
	hba_attached++;

#ifdef	KSTATS_CODE
	/* XXX: need to fix this */
	if ((pptr->sf_ksp = kstat_create("fcp", instance, "statistics",
	    "controller", KSTAT_TYPE_RAW, sizeof (struct sf_stats),
	    KSTAT_FLAG_VIRTUAL)) == NULL) {
		cmn_err(CE_WARN, "fcp%d: failed to create kstat", instance);
	} else {
		pptr->ssfcp_stats.version = 1;
		pptr->ssfcp_ksp->ks_data = (void *)&pptr->ssfcp_stats;
		pptr->ssfcp_ksp->ks_private = pptr;
		pptr->ssfcp_ksp->ks_update = ssfcp_kstat_update;
		kstat_install(pptr->ssfcp_ksp);
	}
#endif /* KSTATS_CODE */

	mutex_enter(&ssfcp_global_mutex);

	/* keep track of instances */
	ssfcp_count++;
	count_incremented++;

	/* create our hotplug thread */
	if (thread_create(NULL, DEFAULTSTKSZ, (void (*)())ssfcp_hp_daemon,
	    (caddr_t)pptr, 0, &p0, TS_RUN, minclsyspri) == NULL) {
		mutex_exit(&ssfcp_global_mutex);
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!unable to create hotplug thread\n");
		(void) scsi_hba_detach(pptr->ssfcpp_dip);
		goto fail;
	}
	mutex_enter(&pptr->ssfcp_hp_mutex);
	pptr->ssfcp_hp_initted++;
	mutex_exit(&pptr->ssfcp_hp_mutex);

	/* add this instance (pptr) to the head of the list */
	pptr->ssfcp_next = ssfcp_port_head;
	ssfcp_port_head = pptr;
	soft_state_linked++;

	if (ssfcp_watchdog_init++ == 0) {
		mutex_exit(&ssfcp_global_mutex);
		ssfcp_watchdog_tick = ssfcp_watchdog_timeout *
			drv_usectohz(1000000);
		ssfcp_watchdog_id = timeout(ssfcp_watch, NULL,
		    ssfcp_watchdog_tick);
	} else {
		mutex_exit(&ssfcp_global_mutex);
	}

	/*
	 * At this stage we need to know if this port needs to
	 * behave in in target mode or initiator mode. This
	 * will decide if ssfcp makes the "first
	 * contact". Another factor is if the mode has been
	 * set for ON_DEMAND_NODE_CREATION.  FIXIT
	 */

	/*
	 * Handle various topologies and link states.
	 */
	switch (FC_PORT_STATE_MASK(pptr->ssfcpp_state)) {

	case FC_STATE_OFFLINE:

		/*
		 * we're attaching a port where the link is offline
		 *
		 * Wait for ONLINE, at which time a state
		 * change will cause a statec_callback
		 *
		 * in the mean time, do not do anything
		 */
		res = DDI_SUCCESS;
		break;

	case FC_STATE_ONLINE: {
		if (ssfcp_create_nodes_on_demand &&
		    FC_TOP_EXTERNAL(pptr->ssfcpp_top)) {
			/*
			 * wait for request to create the devices.
			 */
			mutex_enter(&pptr->ssfcp_mutex);
			pptr->ssfcp_state = SSFCP_STATE_ONLINE;
			mutex_exit(&pptr->ssfcp_mutex);

			res = DDI_SUCCESS;
			break;
		}
		/*
		 * discover devices and create nodes (a private
		 * loop or point-to-point)
		 */
		ASSERT(pptr->ssfcpp_top != FC_TOP_UNKNOWN);
		if ((tmp_list = (fc_portmap_t *)kmem_zalloc(
		    sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES,
		    KM_NOSLEEP)) == NULL) {
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!fcp%d: failed to allocate portmap", instance);
			goto fail;
		}
		max_cnt = SSFCP_MAX_DEVICES;
		if ((res = fc_ulp_getportmap(pptr->ssfcpp_handle, tmp_list,
		    &max_cnt, FC_ULP_PLOGI_PRESERVE)) != FC_SUCCESS) {
			/*
			 * this  just means the transport is busy, perhaps
			 * building a portmap
			 *
			 * so, for now, succeed this port attach -- when
			 * the transport has a new map, it'll send us a
			 * state change then
			 */
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!failed to get port map");
			res = DDI_SUCCESS;
			break;			/* go return result */
		}
		/*
		 * let the state change callback do the SCSI device
		 * discovery and create the devinfos
		 */
		ssfcp_statec_callback(ulph, pptr->ssfcpp_handle,
		    pptr->ssfcpp_state, pptr->ssfcpp_top, tmp_list,
		    max_cnt, pptr->ssfcpp_sid);
		res = DDI_SUCCESS;
		break;
	}

	default:
		/* unknown port state */
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: invalid port state at attach=0x%x",
		    instance, pptr->ssfcpp_state);
		break;
	}

	/* free temp list if used */
	if (tmp_list != NULL) {
		kmem_free(tmp_list, sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES);
	}

	/* all done */
	return (res);

	/* a failure we have to clean up after */
fail:
	ssfcp_log(CE_WARN, pptr->ssfcpp_dip, "!failed to attach to port");

	/* signal the hotplug thread to exit (if needed) */
	if (mutex_initted && pptr->ssfcp_hp_initted) {
		mutex_enter(&pptr->ssfcp_hp_mutex);
		/* the hotplug thread is waiting for a signal */
		cv_signal(&pptr->ssfcp_hp_cv);
		cv_wait(&pptr->ssfcp_hp_cv, &pptr->ssfcp_hp_mutex);
		mutex_exit(&pptr->ssfcp_hp_mutex);
		/* the hotplug thread has now exited */
	}

	if (count_incremented) {
		mutex_enter(&ssfcp_global_mutex);
		ssfcp_count--;
		mutex_exit(&ssfcp_global_mutex);
	}
	if (soft_state_linked) {
		/* remove this ssfcp_port from the linked list */
		(void) ssfcp_soft_state_unlink(pptr);
	}
	/* undo SCSI HBA setup */
	if (hba_attached) {
		(void) scsi_hba_detach(pptr->ssfcpp_dip);
		if (pptr->ssfcp_tran != NULL) {
			scsi_hba_tran_free(pptr->ssfcp_tran);
		}
	}

	/* clean up command/response pool */
	ssfcp_crpool_free(pptr);

	/* clean up our cache */
	if (pptr->ssfcp_pkt_cache != NULL) {
		kmem_cache_destroy(pptr->ssfcp_pkt_cache);
	}
#ifdef	KSTATS_CODE
	if (pptr->ssfcp_ksp != NULL) {
		kstat_delete(pptr->ssfcp_ksp);
	}
#endif
	mutex_enter(&ssfcp_global_mutex);
	if (--ssfcp_watchdog_init == 0) {
		timeout_id_t	tid = ssfcp_watchdog_id;

		mutex_exit(&ssfcp_global_mutex);
		(void) untimeout(tid);
	} else {
		mutex_exit(&ssfcp_global_mutex);
	}
	if (mutex_initted) {
		mutex_destroy(&pptr->ssfcp_mutex);
		mutex_destroy(&pptr->ssfcp_cr_mutex);
		mutex_destroy(&pptr->ssfcp_hp_mutex);
		cv_destroy(&pptr->ssfcp_cr_cv);
		cv_destroy(&pptr->ssfcp_hp_cv);
	}
	if (tmp_list != NULL) {
		kmem_free(tmp_list, sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES);
	}

	/* this makes pptr invalid */
	ddi_soft_state_free(ssfcp_softstate, instance);

	return (DDI_FAILURE);
}


/*ARGSUSED*/
static int
ssfcp_kmem_cache_constructor(void *buf, void *arg, int kmflags)
{
	int			(*callback) (caddr_t);
	fc_packet_t		*fpkt;
	struct ssfcp_pkt	*cmd = buf;
	struct ssfcp_port 	*pptr = (struct ssfcp_port *)arg;

	cmd->cmd_pkt = (struct scsi_pkt *)(&cmd->cmd_scsi_pkt);
	cmd->cmd_fp_pkt = (fc_packet_t *)(&cmd->cmd_fc_packet);

	cmd->cmd_pkt->pkt_ha_private = (opaque_t)cmd;

	cmd->cmd_fp_pkt->pkt_ulp_private = (opaque_t)cmd;
	cmd->cmd_fp_pkt->pkt_fca_private = (opaque_t)((caddr_t)cmd +
	    sizeof (struct ssfcp_pkt));

	fpkt = cmd->cmd_fp_pkt;

	callback = (kmflags == KM_SLEEP) ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT;

	if (ssfcp_cr_alloc(pptr, cmd, callback) == DDI_FAILURE) {
		return (1);
	}

	fpkt->pkt_cmdlen = sizeof (struct fcp_cmd);
	fpkt->pkt_rsplen = FCP_MAX_RSP_IU_SIZE;

	if (ddi_dma_alloc_handle(pptr->ssfcpp_dip,
	    &pptr->ssfcpp_dma_attr, callback, NULL,
	    &fpkt->pkt_data_dma) != 0) {
		return (1);
	}

	if (fc_ulp_init_packet(pptr->ssfcpp_handle, cmd->cmd_fp_pkt,
	    kmflags) != FC_SUCCESS) {
		ddi_dma_free_handle(&fpkt->pkt_data_dma);
		ssfcp_cr_free(cmd->cmd_cr_pool, cmd);
		return (1);
	}

	return (0);
}


static void
ssfcp_kmem_cache_destructor(void *buf, void *arg)
{
	int			rval;
	struct ssfcp_pkt	*cmd = buf;
	struct ssfcp_port	*pptr = (struct ssfcp_port *)arg;

	rval = fc_ulp_uninit_packet(pptr->ssfcpp_handle, cmd->cmd_fp_pkt);
	ASSERT(rval == FC_SUCCESS);

	if (cmd->cmd_fp_pkt->pkt_data_dma) {
		ddi_dma_free_handle(&cmd->cmd_fp_pkt->pkt_data_dma);
	}

	if (cmd->cmd_block) {
		ssfcp_cr_free(cmd->cmd_cr_pool, cmd);
	}
}


/*
 * called by the transport to do our own target initialization
 *
 * can acquire and release the global mutex
 */
/* ARGSUSED */
static int
ssfcp_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	struct ssfcp_tgt	*ptgt;
	struct ssfcp_port	*pptr =
		(struct ssfcp_port *)hba_tran->tran_hba_private;
	int			t_len;
	int			lun;
	unsigned int		link_cnt;
	unsigned char		wwn[FC_WWN_SIZE];
	struct ssfcp_lun	*plun;


	ASSERT(pptr != NULL);

	SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
	    "ssfcp_scsi_tgt_init: called for %s (instance %d)\n",
	    ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip)));

	/* get our port WWN property */
	t_len = sizeof (wwn);
	if (ddi_prop_op(DDI_DEV_T_ANY, tgt_dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, PORT_WWN_PROP,
	    (caddr_t)&wwn, &t_len) != DDI_SUCCESS) {
		/* no port WWN property */
		mutex_enter(&ssfcp_global_mutex);
		if (ssfcp_first_time && (ssfcp_count != 0)) {
			clock_t lb;

			ssfcp_first_time = FALSE;	/* only do once */
			/*
			 * wait a pseudo-random amt of time, then give up
			 *
			 * when the last instance of an attached port goes
			 * away it will signal us
			 */
			(void) drv_getparm(LBOLT, &lb);
			(void) cv_timedwait(&ssfcp_cv, &ssfcp_global_mutex,
			    lb + drv_usectohz(SSFCP_INIT_WAIT_TIMEOUT));
		}
		mutex_exit(&ssfcp_global_mutex);
		return (DDI_NOT_WELL_FORMED);
	}

	t_len = sizeof (link_cnt);
	if (ddi_prop_op(DDI_DEV_T_ANY, tgt_dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, LINK_CNT_PROP,
	    (caddr_t)&link_cnt, &t_len) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}
	t_len = sizeof (lun);
	if (ddi_prop_op(DDI_DEV_T_ANY, tgt_dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, LUN_PROP,
	    (caddr_t)&lun, &t_len) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	mutex_enter(&pptr->ssfcp_mutex);
	if ((plun = ssfcp_lookup_lun(pptr, wwn, lun)) == NULL) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (DDI_FAILURE);
	}

	ptgt = plun->lun_tgt;
	mutex_enter(&ptgt->tgt_mutex);

	if (plun->lun_state & SSFCP_SCSI_LUN_TGT_INIT) {
		mutex_exit(&ptgt->tgt_mutex);
		mutex_exit(&pptr->ssfcp_mutex);
		return (DDI_FAILURE);
	}

	hba_tran->tran_tgt_private = plun;
	plun->lun_tran = hba_tran;
	plun->lun_state |= SSFCP_SCSI_LUN_TGT_INIT;

	mutex_exit(&ptgt->tgt_mutex);
	mutex_exit(&pptr->ssfcp_mutex);

	return (DDI_SUCCESS);
}


/* ARGSUSED */
static void
ssfcp_scsi_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	struct ssfcp_lun	*plun = hba_tran->tran_tgt_private;
	struct ssfcp_tgt *ptgt;
#ifdef	DEBUG
	SSFCP_DEBUG(4, (CE_NOTE, NULL,
	    "ssfcp_scsi_tgt_free: called for tran %s%d, dev %s%d\n",
	    ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
	    ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip)));
#endif	/* DEBUG */

	if (plun == NULL) {
		return;				/* no lun to worry about */
	}

	ptgt = plun->lun_tgt;

	ASSERT(ptgt != NULL);

	mutex_enter(&ptgt->tgt_mutex);
	plun->lun_tran = NULL;
	plun->lun_state &= ~SSFCP_SCSI_LUN_TGT_INIT;
	mutex_exit(&ptgt->tgt_mutex);
}


/*
 * called by the transport to start a packet
 *
 * return a transport status value, i.e. TRAN_ACCECPT for success
 */
static int
ssfcp_start(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct ssfcp_port  *pptr = ADDR2SSFCP(ap);
	struct ssfcp_lun *plun = ADDR2LUN(ap);
	struct ssfcp_pkt *cmd = PKT2CMD(pkt);
	int rval;

	/* ensure command isn't already issued */
	ASSERT(cmd->cmd_state != SSFCP_PKT_ISSUED);

	/* prepare the packet */
	if ((rval = ssfcp_prepare_pkt(pptr, cmd, plun)) != TRAN_ACCEPT) {
		return (rval);
	}
	if (plun->lun_state & SSFCP_LUN_OFFLINE) {
		return (TRAN_FATAL_ERROR);
	}
	if (plun->lun_state & SSFCP_LUN_BUSY) {
		/* the LUN is busy */

		/* see if using interrupts is allowed (so queueing'll work) */
		if (pkt->pkt_flags & FLAG_NOINTR) {
			pkt->pkt_resid = 0;
			return (TRAN_BUSY);
		}

		mutex_enter(&pptr->ssfcp_cmd_mutex);
		pptr->ssfcp_ncmds++;
		mutex_exit(&pptr->ssfcp_cmd_mutex);

		/* got queue up the pkt for later */
		ssfcp_queue_pkt(pptr, cmd);
		return (TRAN_ACCEPT);
	}

	/*
	 * if interrupts aren't allowed (e.g. at dump time) then we'll
	 * have to do polled I/O
	 */
	if (pkt->pkt_flags & FLAG_NOINTR) {
		return (ssfcp_dopoll(pptr, cmd));
	}

	/* send the command */
	cmd->cmd_timeout = cmd->cmd_pkt->pkt_time ?
	    ssfcp_watchdog_time + cmd->cmd_pkt->pkt_time : 0;
	cmd->cmd_state = SSFCP_PKT_ISSUED;

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	pptr->ssfcp_ncmds++;
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	rval = ssfcp_transport(pptr->ssfcpp_handle, cmd->cmd_fp_pkt, 0);
	if (rval == FC_SUCCESS) {
		return (TRAN_ACCEPT);
	}

	SSFCP_DEBUG(2, (CE_WARN, pptr->ssfcpp_dip,
	    "ssfcp_transport failed: %x", rval));

	cmd->cmd_state = SSFCP_PKT_IDLE;

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	pptr->ssfcp_ncmds--;
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	/*
	 * For lack of clearer definitions, choose
	 * between TRAN_BUSY and TRAN_FATAL_ERROR.
	 */
	if (rval == FC_TRAN_BUSY) {
		pkt->pkt_resid = 0;
		rval = TRAN_BUSY;
	} else {
		rval = TRAN_FATAL_ERROR;
	}
	return (rval);
}


/*
 * called by the transport abort a packet -- return boolean success
 */
/*ARGSUSED*/
static int
ssfcp_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	return (FALSE);
}


/*
 * It is expected that the underlying adapter (driver/firmware) to return
 * outstanding requests, if any, from its internal Queue, when a Target
 * RESET is sent out to a device.
 *
 * +	Verify if soc+ firmware does this. If not, check with those folks
 *	to see if that can be done. It may be argued that the firmware
 *	shouldn't be peeking at all the nonsense an Upper Layer protocol
 *	sends - But the reality is that most of the firmwares do, including
 *	the soc+ firmware (C'mon you do optimized SCSI reads and writes)
 *
 *	If that is not too convincing, try the next method: on a per-lun
 *	basis, a list of allocated packets are maintained, for each one of
 *	those packets, check if the packet is submited, and if true, send
 *	an abort. If by now, this doesn't sound a tad ugly, it is very
 *	unfortunate.
 *
 *	A step toward beautifying the ugly method is to define an FCA
 * 	property to identify whether it is capable of returning commands
 *	from their internal Queue when a Target Reset is performed. By
 *	using this property the FCP can decide if there is a need to send
 *	aborts for every outstanding packet on a target.
 */
int
ssfcp_reset(struct scsi_address *ap, int level)
{
	int 			rval = 0;
	struct ssfcp_port 	*pptr = ADDR2SSFCP(ap);

	if (level == RESET_ALL) {
		if (ssfcp_linkreset(pptr, ap, KM_NOSLEEP) == FC_SUCCESS) {
			rval = 1;
		}
	} if (level == RESET_TARGET) {
		if (ssfcp_reset_target(ap) == FC_SUCCESS) {
			rval = 1;
		}
	}
	return (rval);
}


/*
 * A target in in many cases in Fibre Channel has a one to one relation
 * with a port identifier (which is also known as D_ID and also as AL_PA
 * in private Loop) On Fibre Channel-to-SCSI bridge boxes a target reset
 * will most likely result in resetting all LUNs (which means a reset will
 * occur on all the SCSI devices connected at the other end of the bridge)
 * That is the latest favorite topic for discussion, for, one can debate as
 * hot as one likes and come up with arguably a best solution to one's
 * satisfaction
 *
 * To stay on track and not digress much, here are the problems stated
 * briefly:
 *
 *	SCSA doesn't define RESET_LUN, It defines RESET_TARGET, but the
 *	target drivers use RESET_TARGET even if their instance is on a
 *	LUN. If that doesn't sound a bit broke, then a better substitute
 *	for word 'broke' is needed.
 *
 *	FCP SCSI (the current spec) only defines RESET TARGET in the
 *	control fields of an FCP_CMND structure. It should have been
 *	fixed right there, giving flexibility to the initiators to
 *	minimize havoc that could be caused by resetting a target.
 */
static int
ssfcp_reset_target(struct scsi_address *ap)
{
	int			rval = FC_FAILURE;
	struct ssfcp_port  	*pptr = ADDR2SSFCP(ap);
	struct ssfcp_lun 	*plun = ADDR2LUN(ap);
	struct ssfcp_tgt 	*ptgt = plun->lun_tgt;
	struct scsi_pkt		*pkt;
	struct ssfcp_pkt	*cmd;
	struct fcp_rsp		*rsp;
	uint32_t		link_cnt;
	struct fcp_rsp_info	*rsp_info;
	struct ssfcp_reset_elem	*p;

	if ((p = kmem_alloc(sizeof (struct ssfcp_reset_elem), KM_NOSLEEP))
						== NULL)
		return (rval);

	mutex_enter(&ptgt->tgt_mutex);
	if (ptgt->tgt_state & (SSFCP_TGT_OFFLINE | SSFCP_TGT_BUSY)) {
		mutex_exit(&ptgt->tgt_mutex);
		kmem_free(p, sizeof (struct ssfcp_reset_elem));
		return (rval);
	}
	mutex_exit(&ptgt->tgt_mutex);

	if ((pkt = ssfcp_scsi_init_pkt(ap, NULL, NULL, 0, 0,
	    0, 0, NULL, 0)) == NULL) {
		kmem_free(p, sizeof (struct ssfcp_reset_elem));
		return (rval);
	}
	pkt->pkt_time = SSFCP_POLL_TIMEOUT;

	/* fill in cmd part of packet */
	cmd = PKT2CMD(pkt);
	cmd->cmd_block->fcp_cntl.cntl_reset = 1;
	cmd->cmd_fp_pkt->pkt_comp = NULL;
	cmd->cmd_pkt->pkt_flags |= FLAG_NOINTR;

	/* prepare a packet for transport */
	if (ssfcp_prepare_pkt(pptr, cmd, plun) != TRAN_ACCEPT) {
		ssfcp_scsi_destroy_pkt(ap, pkt);
		kmem_free(p, sizeof (struct ssfcp_reset_elem));
		return (rval);
	}

	mutex_enter(&ptgt->tgt_mutex);
	ssfcp_update_tgt_state(ptgt, FCP_SET, SSFCP_LUN_BUSY);
	mutex_exit(&ptgt->tgt_mutex);
	mutex_enter(&pptr->ssfcp_mutex);
	link_cnt = pptr->ssfcp_link_cnt;
	mutex_exit(&pptr->ssfcp_mutex);

	/* sumbit the packet */
	if (ssfcp_dopoll(pptr, cmd) == TRAN_ACCEPT) {

		rsp = (struct fcp_rsp *)(cmd->cmd_fp_pkt->pkt_resp);
		rsp_info = (struct fcp_rsp_info *)((caddr_t)rsp +
				sizeof (struct fcp_rsp));
		if (rsp_info->rsp_code == FCP_NO_FAILURE) {
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!FCP: WWN 0x%08x%08x reset successfully\n",
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[0]),
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[4]));
			rval = FC_SUCCESS;
		} else {
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!FCP: Reset to WWN  0x%08x%08x failed, "
			    " rsp_code =0x%x\n",
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[0]),
			    *((int *)&ptgt->tgt_port_wwn.raw_wwn[4]),
			    rsp_info->rsp_code);
		}
	}
	ssfcp_scsi_destroy_pkt(ap, pkt);

	if (rval == FC_FAILURE) {
		mutex_enter(&ptgt->tgt_mutex);
		ssfcp_update_tgt_state(ptgt, FCP_RESET, SSFCP_LUN_BUSY);
		mutex_exit(&ptgt->tgt_mutex);
		kmem_free(p, sizeof (struct ssfcp_reset_elem));
		return (rval);
	}

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt !=  link_cnt) {
		mutex_exit(&pptr->ssfcp_mutex);
		kmem_free(p, sizeof (struct ssfcp_reset_elem));
		return (TRUE);
	}
	p->tgt = ptgt;
	p->link_cnt = link_cnt;
	p->timeout = ssfcp_watchdog_time + SSFCP_RESET_DELAY;
	p->next = pptr->ssfcp_reset_list;
	pptr->ssfcp_reset_list = p;
	mutex_exit(&pptr->ssfcp_mutex);
	return (rval);
}


/*
 * called by ssfcp_getcap and ssfcp_setcap to get and set (respectively)
 * SCSI capabilities
 */
/* ARGSUSED */
static int
ssfcp_commoncap(struct scsi_address *ap, char *cap,
    int val, int tgtonly, int doset)
{
	struct ssfcp_port *pptr = ADDR2SSFCP(ap);
	struct ssfcp_lun *plun = ADDR2LUN(ap);
	struct ssfcp_tgt *ptgt = plun->lun_tgt;
	int cidx;
	int rval = FALSE;

	if (cap == (char *)0) {
		SSFCP_DEBUG(3, (CE_WARN, pptr->ssfcpp_dip,
		    "ssfcp_commoncap: invalid arg"));
		return (rval);
	}

	if ((cidx = scsi_hba_lookup_capstr(cap)) == -1) {
		return (UNDEFINED);
	}

	/*
	 * Process setcap request.
	 */
	if (doset) {
		/*
		 * At present, we can only set binary (0/1) values
		 */
		switch (cidx) {
		case SCSI_CAP_ARQ:
			if (val == 0) {
				rval = FALSE;
			} else {
				rval = TRUE;
			}
			break;

		default:
			SSFCP_DEBUG(4, (CE_WARN, pptr->ssfcpp_dip,
			    "ssfcp_setcap: unsupported %d", cidx));
			rval = UNDEFINED;
			break;
		}

		SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
		    "set cap: cap=%s, val/tgtonly/doset/rval = "
		    "0x%x/0x%x/0x%x/%d\n",
		    cap, val, tgtonly, doset, rval));

	} else {
		/*
		 * Process getcap request.
		 */
		switch (cidx) {
		case SCSI_CAP_DMA_MAX:
			rval = (int)pptr->ssfcpp_dma_attr.dma_attr_maxxfer;
			break;

		case SCSI_CAP_INITIATOR_ID:
			rval = pptr->ssfcpp_sid;
			break;

		case SCSI_CAP_ARQ:
		case SCSI_CAP_RESET_NOTIFICATION:
		case SCSI_CAP_TAGGED_QING:
			rval = TRUE;
			break;

		case SCSI_CAP_SCSI_VERSION:
			rval = 3;
			break;

		case SCSI_CAP_INTERCONNECT_TYPE:
			if (FC_TOP_EXTERNAL(pptr->ssfcpp_top) ||
			    (ptgt->tgt_hard_addr == 0)) {
				rval = INTERCONNECT_FABRIC;
			}
			break;

		default:
			SSFCP_DEBUG(4, (CE_WARN, pptr->ssfcpp_dip,
			    "ssfcp_getcap: unsupported %d", cidx));
			rval = UNDEFINED;
			break;
		}

		SSFCP_DEBUG(5, (CE_NOTE, pptr->ssfcpp_dip,
		    "get cap: cap=%s, val/tgtonly/doset/rval = "
		    "0x%x/0x%x/0x%x/%d\n",
		    cap, val, tgtonly, doset, rval));
	}
	return (rval);
}

/*
 * called by the framework to get a SCSI capability
 */
static int
ssfcp_getcap(struct scsi_address *ap, char *cap, int whom)
{
	return (ssfcp_commoncap(ap, cap, 0, whom, 0));
}


/*
 * called by the framework to set a SCSI capability
 */
static int
ssfcp_setcap(struct scsi_address *ap, char *cap, int value, int whom)
{
	return (ssfcp_commoncap(ap, cap, value, whom, 1));
}


static struct scsi_pkt *
ssfcp_scsi_init_pkt(struct scsi_address *ap, struct scsi_pkt *pkt,
    struct buf *bp, int cmdlen, int statuslen, int tgtlen,
    int flags, int (*callback)(), caddr_t arg)
{
	int kf;
	int failure = FALSE;
	struct ssfcp_pkt *cmd;
	struct ssfcp_port *pptr = ADDR2SSFCP(ap);
	struct ssfcp_lun *plun = ADDR2LUN(ap);
	struct ssfcp_tgt *ptgt = plun->lun_tgt;
	struct ssfcp_pkt	*new_cmd = NULL;
	fc_packet_t	*fpkt;
	fc_frame_hdr_t	*hp;
	int *p;
	struct fcp_cmd fcmd;

	bzero(&fcmd, sizeof (struct fcp_cmd));

	/*
	 * If we've already allocated a pkt once,
	 * this request is for dma allocation only.
	 */
	if (pkt == NULL) {
		/*
		 * First step of ssfcp_scsi_init_pkt: pkt allocation
		 */
		if (cmdlen > FCP_CDB_SIZE) {
			return (NULL);
		}

		kf = (callback == SLEEP_FUNC) ? KM_SLEEP: KM_NOSLEEP;

		cmd = kmem_cache_alloc(pptr->ssfcp_pkt_cache, kf);
		if (cmd != NULL) {
			/*
			 * Selective zeroing of the pkt.
			 */
			cmd->cmd_flags = 0;
			cmd->cmd_forw = NULL;
			cmd->cmd_back = NULL;
			cmd->cmd_next = NULL;
			cmd->cmd_state = SSFCP_PKT_IDLE;
			cmd->cmd_pkt->pkt_scbp = (opaque_t)cmd->cmd_scsi_scb;
			cmd->cmd_pkt->pkt_comp	= NULL;
			cmd->cmd_pkt->pkt_flags	= 0;
			cmd->cmd_pkt->pkt_time	= 0;
			cmd->cmd_pkt->pkt_resid	= 0;
			cmd->cmd_pkt->pkt_reason = 0;
			cmd->cmd_scblen		= statuslen;
			cmd->cmd_privlen	= tgtlen;
			cmd->cmd_pkt->pkt_address = *ap;
			cmd->cmd_pkt->pkt_private = cmd->cmd_pkt_private;

			/* zero pkt_private */
			bzero((caddr_t)cmd->cmd_pkt->pkt_private,
			    sizeof (cmd->cmd_pkt_private));
		} else {
			failure++;
		}

		if (failure || tgtlen > sizeof (cmd->cmd_pkt_private) ||
		    (statuslen > EXTCMDS_STATUS_SIZE)) {
			if (!failure) {
				failure = ssfcp_pkt_alloc_extern(pptr, cmd,
				    tgtlen, statuslen, kf);
			}
			if (failure) {
				return (NULL);
			}
		}

		fpkt = cmd->cmd_fp_pkt;
		cmd->cmd_pkt->pkt_cdbp = cmd->cmd_block->fcp_cdb;

		/* Fill in the Fabric Channel Header */
		hp = &fpkt->pkt_cmd_fhdr;

		hp->r_ctl = R_CTL_COMMAND;
		hp->type = FC_TYPE_SCSI_FCP;
		hp->f_ctl = F_CTL_SEQ_INITIATIVE | F_CTL_FIRST_SEQ;
		hp->rsvd = 0;
		hp->seq_id = 0;
		hp->df_ctl  = 0;
		hp->seq_cnt = 0;
		hp->ox_id = 0xffff;
		hp->rx_id = 0xffff;
		hp->ro = 0;
		*((int32_t *)&cmd->cmd_block->fcp_cntl) = 0;

		/* zero cdb */
		p = (int *)cmd->cmd_pkt->pkt_cdbp;
		*p++	= 0;
		*p++	= 0;
		*p++	= 0;
		*p	= 0;

		/*
		 * A doubly linked list (cmd_forw, cmd_back) is built
		 * out of every allocated packet on a per-lun basis
		 *
		 * The packets are maintained in the list so as to satisfy
		 * scsi_abort() requests. At present (which is unlikely to
		 * change in the future) nobody performs a real scsi_abort
		 * in the SCSI target drivers (as they don't keep the packets
		 * after doing scsi_transport - so they don't know how to
		 * abort a packet other than sending a NULL to abort all
		 * outstanding packets)
		 */
		mutex_enter(&plun->lun_mutex);
		if (plun->lun_pkt_head) {
			ASSERT(plun->lun_pkt_tail != NULL);

			cmd->cmd_back = plun->lun_pkt_tail;
			plun->lun_pkt_tail->cmd_forw = cmd;
			plun->lun_pkt_tail = cmd;
		} else {
			plun->lun_pkt_head = plun->lun_pkt_tail = cmd;
		}
		mutex_exit(&plun->lun_mutex);
		new_cmd = cmd;
	} else {
		cmd = (struct ssfcp_pkt *)pkt->pkt_ha_private;
		fpkt = cmd->cmd_fp_pkt;
	}
	fpkt->pkt_pd = ptgt->tgt_pd_handle;

	/*
	 * Second step of ssfcp_scsi_init_pkt:  dma allocation
	 * Set up dma info
	 */
	if ((bp != NULL) && (bp->b_bcount != 0)) {
		int cmd_flags, dma_flags;
		int rval;
		uint_t dmacookie_count;

		cmd_flags = cmd->cmd_flags;

		if (bp->b_flags & B_READ) {
			cmd_flags &= ~CFLAG_DMASEND;
			dma_flags = DDI_DMA_READ;
		} else {
			cmd_flags |= CFLAG_DMASEND;
			dma_flags = DDI_DMA_WRITE;
		}
		if (flags & PKT_CONSISTENT) {
			cmd_flags |= CFLAG_CMDIOPB;
			dma_flags |= DDI_DMA_CONSISTENT;
		}


		rval = ddi_dma_buf_bind_handle(fpkt->pkt_data_dma, bp,
		    dma_flags, callback, arg, &fpkt->pkt_data_cookie,
		    &dmacookie_count);
dma_failure:
		if (rval != DDI_SUCCESS) {
			SSFCP_DEBUG(4, (CE_CONT, pptr->ssfcpp_dip,
			    "fcp:ddi_dma_buf.. failed\n"));
			switch (rval) {
			case DDI_DMA_NORESOURCES:
				bioerror(bp, 0);
				break;
			case DDI_DMA_BADATTR:
			case DDI_DMA_NOMAPPING:
				bioerror(bp, EFAULT);
				break;
			case DDI_DMA_TOOBIG:
			default:
				bioerror(bp, EINVAL);
				break;
			}
			cmd->cmd_flags = cmd_flags & ~CFLAG_DMAVALID;
			if (new_cmd != NULL) {
				ssfcp_scsi_destroy_pkt(ap, new_cmd->cmd_pkt);
			}
			return (NULL);
		}
		ASSERT(dmacookie_count == 1);
		cmd->cmd_dmacount = bp->b_bcount;
		cmd->cmd_flags = cmd_flags | CFLAG_DMAVALID;

		ASSERT(fpkt->pkt_data_dma != NULL);
	}

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		if (cmd->cmd_flags & CFLAG_DMASEND) {
			fcmd.fcp_cntl.cntl_read_data = 0;
			fcmd.fcp_cntl.cntl_write_data = 1;
			fpkt->pkt_tran_type =  FC_PKT_FCP_WRITE;
		} else {
			fcmd.fcp_cntl.cntl_read_data = 1;
			fcmd.fcp_cntl.cntl_write_data = 0;
			fpkt->pkt_tran_type =  FC_PKT_FCP_READ;
		}

		fpkt->pkt_datalen = fpkt->pkt_data_cookie.dmac_size;
		fcmd.fcp_data_len =  fpkt->pkt_data_cookie.dmac_size;
	} else {
		fcmd.fcp_cntl.cntl_read_data = 0;
		fcmd.fcp_cntl.cntl_write_data = 0;
		fpkt->pkt_tran_type = FC_PKT_EXCHANGE;
		fpkt->pkt_datalen = 0;
		fcmd.fcp_data_len = 0;
	}
	fcmd.fcp_cntl.cntl_qtype = FCP_QTYPE_SIMPLE;
	bcopy(&(plun->lun_string), &(fcmd.fcp_ent_addr), FCP_LUN_SIZE);

	SSFCP_CP_OUT((uint8_t *)&fcmd, cmd->cmd_block, fpkt->pkt_cmd_acc,
	    sizeof (struct fcp_cmd));

	return (cmd->cmd_pkt);
}


static void
ssfcp_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct ssfcp_pkt	*cmd = (struct ssfcp_pkt *)
					    pkt->pkt_ha_private;
	struct ssfcp_port	*pptr = ADDR2SSFCP(ap);
	struct ssfcp_lun	*plun = ADDR2LUN(ap);

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		(void) ddi_dma_unbind_handle(cmd->cmd_fp_pkt->pkt_data_dma);
		cmd->cmd_flags ^= CFLAG_DMAVALID;
	}

	/*
	 * Remove the packet from the per-lun list
	 */
	mutex_enter(&plun->lun_mutex);
	if (cmd->cmd_back) {
		ASSERT(cmd != plun->lun_pkt_head);
		cmd->cmd_back->cmd_forw = cmd->cmd_forw;
	} else {
		ASSERT(cmd == plun->lun_pkt_head);
		plun->lun_pkt_head = cmd->cmd_forw;
	}
	if (cmd->cmd_forw) {
		cmd->cmd_forw->cmd_back = cmd->cmd_back;
	} else {
		ASSERT(cmd == plun->lun_pkt_tail);
		plun->lun_pkt_tail = cmd->cmd_back;
	}
	mutex_exit(&plun->lun_mutex);

	if ((cmd->cmd_flags &
	    (CFLAG_FREE | CFLAG_PRIVEXTERN | CFLAG_SCBEXTERN)) == 0) {
		ASSERT(cmd->cmd_state != SSFCP_PKT_ISSUED);
		cmd->cmd_flags = CFLAG_FREE;
		kmem_cache_free(pptr->ssfcp_pkt_cache, (void *)cmd);
	} else {
		ssfcp_pkt_destroy_extern(pptr, cmd);
	}
}


/* ARGSUSED */
static void
ssfcp_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct ssfcp_pkt *cmd = (struct ssfcp_pkt *)pkt->pkt_ha_private;

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		(void) ddi_dma_unbind_handle(cmd->cmd_fp_pkt->pkt_data_dma);
		cmd->cmd_flags ^= CFLAG_DMAVALID;
	}
}


/*
 * routine for reset notification setup, to register or cancel
 *
 * called by the transport
 *
 */
/*ARGSUSED*/
static int
ssfcp_scsi_reset_notify(struct scsi_address *ap, int flag,
    void (*callback)(caddr_t), caddr_t arg)
{
	struct ssfcp_port *pptr = ADDR2SSFCP(ap);

	return (scsi_hba_reset_notify_setup(ap, flag, callback, arg,
	    &pptr->ssfcp_mutex, &pptr->ssfcp_reset_notify_listf));
}


/* ARGSUSED */
static void
ssfcp_scsi_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	int i;
	struct ssfcp_pkt *cmd =
		(struct ssfcp_pkt *)pkt->pkt_ha_private;

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		i = ddi_dma_sync(cmd->cmd_fp_pkt->pkt_data_dma, 0, 0,
			(cmd->cmd_flags & CFLAG_DMASEND) ?
			DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
		if (i != DDI_SUCCESS) {
			cmn_err(CE_WARN, "fcp : sync pkt failed");
		}
	}
}


/*
 * (de)allocator for non-std size cdb/pkt_private/status
 */
/*ARGSUSED*/
static int
ssfcp_pkt_alloc_extern(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd,
    int tgtlen, int statuslen, int kf)
{
	caddr_t scbp, tgt;
	int failure = 0;
	struct scsi_pkt *pkt = CMD2PKT(cmd);

	tgt = scbp = NULL;

	if (tgtlen > sizeof (cmd->cmd_pkt_private)) {
		if ((tgt = kmem_zalloc(tgtlen, kf)) == NULL) {
			failure++;
		} else {
			cmd->cmd_flags |= CFLAG_PRIVEXTERN;
			pkt->pkt_private = tgt;
		}
	}
	if (statuslen > EXTCMDS_STATUS_SIZE) {
		if ((scbp = kmem_zalloc((size_t)statuslen, kf)) == NULL) {
			failure++;
		} else {
			cmd->cmd_flags |= CFLAG_SCBEXTERN;
			pkt->pkt_scbp = (opaque_t)scbp;
		}
	}
	if (failure != 0) {
		ssfcp_pkt_destroy_extern(pptr, cmd);
	}
	return (failure);
}


static void
ssfcp_pkt_destroy_extern(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);

	if (cmd->cmd_flags & CFLAG_FREE) {
		cmn_err(CE_PANIC,
		    "ssfcp_scsi_impl_pktfree: freeing free packet");
		_NOTE(NOT_REACHED)
		/* NOTREACHED */
	}
	if (cmd->cmd_flags & CFLAG_SCBEXTERN) {
		kmem_free((caddr_t)pkt->pkt_scbp,
		    (size_t)cmd->cmd_scblen);
	}
	if (cmd->cmd_flags & CFLAG_PRIVEXTERN) {
		kmem_free((caddr_t)pkt->pkt_private,
		    (size_t)cmd->cmd_privlen);
	}

	cmd->cmd_flags = CFLAG_FREE;
	kmem_cache_free(pptr->ssfcp_pkt_cache, (void *)cmd);
}


static int
ssfcp_bus_get_eventcookie(dev_info_t *dip, dev_info_t *rdip, char *name,
    ddi_eventcookie_t *event_cookiep, ddi_plevel_t *plevelp,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	struct ssfcp_port *pptr;

	pptr = ssfcp_dip2port(dip);
	ASSERT(pptr != NULL);
	if (pptr == NULL) {
		return (DDI_FAILURE);
	}

	return (ndi_event_retrieve_cookie(pptr->ssfcp_event_hdl, rdip, name,
	    event_cookiep, plevelp, iblock_cookiep, NDI_EVENT_NOPASS));
}


static int
ssfcp_bus_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventid, int (*callback)(), void *arg)
{
	struct ssfcp_port *pptr;

	pptr = ssfcp_dip2port(dip);
	ASSERT(pptr != NULL);
	if (pptr == NULL) {
		return (DDI_FAILURE);
	}

	return (ndi_event_add_callback(pptr->ssfcp_event_hdl, rdip,
	    eventid, callback, arg, NDI_EVENT_NOPASS));
}


static int
ssfcp_bus_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventid)
{
	struct ssfcp_port *pptr;

	pptr = ssfcp_dip2port(dip);
	ASSERT(pptr != NULL);
	if (pptr == NULL) {
		return (DDI_FAILURE);
	}
	return (ndi_event_remove_callback(pptr->ssfcp_event_hdl, rdip,
	    eventid, NDI_EVENT_NOPASS));
}


/*
 * called by the transport to post an event
 */
static int
ssfcp_bus_post_event(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventid, void *impldata)
{
	struct ssfcp_port	*pptr = ssfcp_dip2port(dip);
	struct ssfcp_lun	*plun;
	ddi_eventcookie_t	remove_event =
		ndi_event_getcookie(DDI_DEVI_REMOVE_EVENT);


	SSFCP_DEBUG(4, (CE_NOTE, NULL,
	    "ssfcp_bus_post_event: called for %s%d\n",
	    ddi_get_name(dip), ddi_get_instance(dip)));

	/* is this our event to handle ??? */
	if (bcmp(&eventid, &remove_event, sizeof (ddi_eventcookie_t)) != 0) {
		/* let the framework handle this event */
		return (ndi_post_event(dip, rdip, eventid, impldata));
	}

	/* handle this event ourselves */

	if (pptr == NULL) {
		/* no port ??? */
		return (DDI_EVENT_UNCLAIMED);
	}

	mutex_enter(&pptr->ssfcp_mutex);
	if ((plun = ssfcp_get_lun_from_dip(pptr, rdip)) == NULL) {
		mutex_exit(&pptr->ssfcp_mutex);
		/* can't find this LUN for this port */
		return (DDI_EVENT_UNCLAIMED);
	}
	mutex_exit(&pptr->ssfcp_mutex);

	/* clear LUN dip/state */
	mutex_enter(&(plun->lun_tgt->tgt_mutex));

	mutex_enter(&plun->lun_mutex);
	plun->lun_dip = NULL;
	plun->lun_state &= ~(SSFCP_LUN_INIT | SSFCP_SCSI_LUN_TGT_INIT);
	mutex_exit(&plun->lun_mutex);

	mutex_exit(&(plun->lun_tgt->tgt_mutex));

	return (DDI_EVENT_CLAIMED);
}


/*
 * called by the transport to get the port-wwn and lun
 * properties of this device, and to create a "name" based on them
 *
 * these properties don't exist on sun4d and sun4m
 *
 * return 1 for success else return 0 -- NOTE: always returns success
 *
 * this routine can sleep (in the ddi_prop_op() calls)
 */
/* ARGSUSED */
static int
ssfcp_scsi_get_name(struct scsi_device *sd, char *name, int len)
{
	la_wwn_t	wwn;
	dev_info_t	*tgt_dip;
	int		i;			/* size for ddi_prop_op */
	int		lun;			/* get the lun prop value */
	char		tbuf[(FC_WWN_SIZE*2)+1]; /* temp wwn string buffer */


	ASSERT(sd != NULL);
	ASSERT(name != NULL);

	/* get the port-wwn property */
	tgt_dip = sd->sd_dev;

	ASSERT(tgt_dip != NULL);

	i = FC_WWN_SIZE;
	if (ddi_prop_op(DDI_DEV_T_ANY, tgt_dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, PORT_WWN_PROP,
	    (caddr_t)&wwn, &i) != DDI_SUCCESS) {
		name[0] = '\0';			/* return blank name */
		return (1);			/* no port-wwn prop */
	}

	/* get the lun property */
	i = sizeof (lun);			/* the size to get */
	if (ddi_prop_op(DDI_DEV_T_ANY, sd->sd_dev, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, LUN_PROP,
	    (caddr_t)&lun, &i) != DDI_SUCCESS) {
		name[0] = '\0';			/* return blank name */
		return (1);			/* no lun prop */
	}

	/* we have port-wwn and lun -- translate into a string */
	for (i = 0; i < FC_WWN_SIZE; i++) {
		(void) sprintf(&tbuf[i << 1], "%02x", wwn.raw_wwn[i]);
	}

	/* return resulting name string, i.e. "wWWN,LUN" */
	(void) sprintf(name, "w%s,%x", tbuf, lun);
	return (1);				/* success */
}


/*
 * called by the transport to get the SCSI target id value, returning
 * it in "name"
 *
 * this isn't needed/used on sun4d and sun4m
 *
 * return 1 for success else return 0
 */
/* ARGSUSED */
static int
ssfcp_scsi_get_bus_addr(struct scsi_device *sd, char *name, int len)
{
	struct ssfcp_lun	*plun = ADDR2LUN(&sd->sd_address);
	struct ssfcp_tgt	*ptgt;


	if (plun == NULL) {
		return (0);
	}
	if ((ptgt = plun->lun_tgt) == NULL) {
		return (0);
	}

	(void) sprintf(name, "%x", ptgt->tgt_d_id);
	return (1);				/* success */
}


/*
 * called internally to reset the link where the specified port lives
 */
int
ssfcp_linkreset(struct ssfcp_port *pptr, struct scsi_address *ap, int sleep)
{
	la_wwn_t wwn;
	struct ssfcp_lun *plun;
	struct ssfcp_tgt *ptgt;

	/* disable restart of lip if we're suspended */
	mutex_enter(&pptr->ssfcp_mutex);

	if (pptr->ssfcp_state & SSFCP_STATE_SUSPENDED) {
		mutex_exit(&pptr->ssfcp_mutex);
		SSFCP_DEBUG(2, (CE_CONT, pptr->ssfcpp_dip,
		    "ssfcp_linkreset, ssfcp%d: link reset "
		    "disabled due to DDI_SUSPEND\n",
		    ddi_get_instance(pptr->ssfcpp_dip)));
		return (FC_FAILURE);
	}

	if (pptr->ssfcp_state == SSFCP_STATE_OFFLINE ||
		pptr->ssfcp_state == SSFCP_STATE_ONLINING) {
		mutex_exit(&pptr->ssfcp_mutex);
		return (FC_SUCCESS);
	}

	SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip, "Forcing link reset\n"));


#ifdef	KSTATS_CODE
	/* keep track of link resets */
	pptr->ssfcp_stats.link_reset_count++;
#endif

	/*
	 * If ap == NULL assume local link reset.
	 */

	if (FC_TOP_EXTERNAL(pptr->ssfcpp_top) && (ap != NULL)) {
		plun = ADDR2LUN(ap);
		ptgt = plun->lun_tgt;
		ssfcp_lfa_update(pptr, ap, SSFCP_LUN_BUSY);
		bcopy(&ptgt->tgt_port_wwn.raw_wwn[0], &wwn, sizeof (wwn));
		mutex_exit(&pptr->ssfcp_mutex);
		(void) fc_ulp_linkreset(pptr->ssfcpp_handle, &wwn, sleep);
		mutex_enter(&pptr->ssfcp_mutex);
	}
	/* update state on all targets/luns */
	ssfcp_update_state(pptr, SSFCP_LUN_BUSY);
	pptr->ssfcp_link_cnt++;
	pptr->ssfcp_state = SSFCP_STATE_OFFLINE;
	bzero((caddr_t)&wwn, sizeof (wwn));
	mutex_exit(&pptr->ssfcp_mutex);
	return (fc_ulp_linkreset(pptr->ssfcpp_handle, &wwn, sleep));
}


void
ssfcp_lfa_update(struct ssfcp_port *pptr, struct scsi_address *ap,
    uint32_t state)
{
	int i;
	struct ssfcp_lun *orig_lun = ADDR2LUN(ap);
	struct ssfcp_tgt  *ptgt, *orig_tgt;

	orig_tgt = orig_lun->lun_tgt;
	ASSERT(mutex_owned(&pptr->ssfcp_mutex));
	for (i = 0; i < SSFCP_NUM_HASH; i++) {
		for (ptgt = pptr->ssfcp_wwn_list[i]; ptgt != NULL;
		    ptgt = ptgt->tgt_next) {
				mutex_enter(&ptgt->tgt_mutex);
			if (LFA(orig_tgt->tgt_d_id) == LFA(ptgt->tgt_d_id)) {
				ssfcp_update_tgt_state(ptgt, FCP_SET, state);
			}
			mutex_exit(&ptgt->tgt_mutex);
		}
	}
}


/*
 * called from ssfcp_port_attach() to resume a port
 * return DDI_* success/failure status
 * acquires and releases the global mutex
 * acquires and releases the port mutex
 */
/*ARGSUSED*/
int
ssfcp_handle_port_resume(opaque_t ulph, fc_ulp_port_info_t *pinfo,
    uint32_t s_id, int instance)
{
	int			res = DDI_FAILURE; /* default result */
	struct ssfcp_port	*pptr;		/* port state ptr */
	uint32_t		max_cnt;
	fc_portmap_t		*tmp_list = NULL;

	/*
	 * This is the equivalent function call for DDI_RESUME.
	 *
	 * this port is resuming after being suspended before
	 */

	SSFCP_DEBUG(2, (CE_NOTE, pinfo->port_dip, "port resume: for port %d\n",
	    instance));

	if ((pptr = ddi_get_soft_state(ssfcp_softstate, instance)) ==
	    NULL) {
		/* this shouldn't happen */
		cmn_err(CE_WARN, "fcp: bad soft state");
		return (res);
	}

	/*
	 * Make a copy of ulp_port_info as fctl allocates
	 * a temp struct.
	 */
	(void) ssfcp_cp_pinfo(pptr, pinfo);

	mutex_enter(&pptr->ssfcp_mutex);
	pptr->ssfcp_state &= ~SSFCP_STATE_SUSPENDED;
	mutex_exit(&pptr->ssfcp_mutex);

	pptr->ssfcpp_sid = s_id;
	pptr->ssfcp_state = SSFCP_STATE_INIT;

	mutex_enter(&ssfcp_global_mutex);
	if (ssfcp_watchdog_init++ == 0) {
		mutex_exit(&ssfcp_global_mutex);
		ssfcp_watchdog_tick = ssfcp_watchdog_timeout *
			drv_usectohz(1000000);
		ssfcp_watchdog_id = timeout(ssfcp_watch,
		    NULL, ssfcp_watchdog_tick);
	} else {
		mutex_exit(&ssfcp_global_mutex);
	}

	/*
	 * Handle various topologies and link states.
	 */
	switch (FC_PORT_STATE_MASK(pptr->ssfcpp_state)) {
	case FC_STATE_OFFLINE:
		/*
		 * Wait for ONLINE, at which time a state
		 * change will cause a statec_callback
		 */
		res = DDI_SUCCESS;
		break;

	case FC_STATE_ONLINE:
		if (ssfcp_create_nodes_on_demand &&
		    FC_TOP_EXTERNAL(pptr->ssfcpp_top)) {
			/*
			 * wait for request to create the devices.
			 */
			res = DDI_SUCCESS;
			break;
		}

		/*
		 * discover devices and create nodes (a private
		 * loop or point-to-point)
		 */
		ASSERT(pptr->ssfcpp_top != FC_TOP_UNKNOWN);
		if ((tmp_list = (fc_portmap_t *)kmem_zalloc(
		    (sizeof (fc_portmap_t)) * SSFCP_MAX_DEVICES,
		    KM_NOSLEEP)) == NULL) {
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!fcp%d: failed to allocate portmap", instance);
			break;
		}
		max_cnt = SSFCP_MAX_DEVICES;
		if ((res = fc_ulp_getportmap(pptr->ssfcpp_handle, tmp_list,
		    &max_cnt, FC_ULP_PLOGI_PRESERVE)) != FC_SUCCESS) {
			SSFCP_DEBUG(2, (CE_NOTE,  pptr->ssfcpp_dip,
			    "resume failed getportmap: reason=0x%x\n", res));
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!failed to get port map");
			break;
		}

		/*
		 * do the SCSI device discovery and create
		 * the devinfos
		 */
		ssfcp_statec_callback(ulph, pptr->ssfcpp_handle,
		    pptr->ssfcpp_state, pptr->ssfcpp_top, tmp_list,
		    max_cnt, pptr->ssfcpp_sid);
		res = DDI_SUCCESS;
		break;

	default:
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: invalid port state at attach=0x%x",
		    instance, pptr->ssfcpp_state);
		pptr->ssfcp_state = SSFCP_STATE_OFFLINE;
		break;
	}

	/* free list if used */
	if (tmp_list != NULL) {
		kmem_free(tmp_list, sizeof (fc_portmap_t) * SSFCP_MAX_DEVICES);
	}

	/* all done */
	return (res);
}


static void
ssfcp_cp_pinfo(struct ssfcp_port *pptr, fc_ulp_port_info_t *pinfo)
{
	pptr->ssfcpp_linkage = *pinfo->port_linkage;
	pptr->ssfcpp_dip = pinfo->port_dip;
	pptr->ssfcpp_handle = pinfo->port_handle;
	pptr->ssfcpp_dma_attr = *pinfo->port_dma_attr;
	pptr->ssfcpp_dma_acc_attr = *pinfo->port_acc_attr;
	pptr->ssfcpp_priv_pkt_len = pinfo->port_fca_pkt_size;
	pptr->ssfcpp_max_exch = pinfo->port_fca_max_exch;
	pptr->ssfcpp_state = pinfo->port_state;
	pptr->ssfcpp_top = pinfo->port_flags;
	pptr->ssfcpp_cmds_aborted = pinfo->port_reset_action;
	bcopy(&pinfo->port_nwwn, &pptr->ssfcpp_nwwn, sizeof (la_wwn_t));
	bcopy(&pinfo->port_pwwn, &pptr->ssfcpp_pwwn, sizeof (la_wwn_t));
}


/*
 * add to the cmd/response pool for this port
 */
int
ssfcp_add_cr_pool(struct ssfcp_port *pptr)
{
	int 		cmd_buf_size;
	size_t 		real_cmd_buf_size;
	int 		rsp_buf_size;
	size_t 		real_rsp_buf_size;
	uint_t 		i, ccount;
	struct ssfcp_cr_pool	*pool_ptr;
	struct ssfcp_cr_free_elem *cptr;
	caddr_t	dptr, eptr;
	ddi_dma_cookie_t	cookie;
	int		cmd_bound = 0;
	int		rsp_bound = 0;


	if ((pool_ptr = kmem_zalloc(sizeof (struct ssfcp_cr_pool),
	    KM_NOSLEEP)) == NULL) {
		return (DDI_FAILURE);
	}

	if (ddi_dma_alloc_handle(pptr->ssfcpp_dip, &pptr->ssfcpp_dma_attr,
	    DDI_DMA_DONTWAIT, NULL, &pool_ptr->cmd_dma_handle) !=
	    DDI_SUCCESS) {
		goto fail;
	}
	/*
	 * Get a piece of memory in which to put commands
	 */
	cmd_buf_size = (sizeof (struct fcp_cmd) * SSFCP_ELEMS_IN_POOL + 7) &
	    ~7;
	if (ddi_dma_mem_alloc(pool_ptr->cmd_dma_handle, cmd_buf_size,
	    &pptr->ssfcpp_dma_acc_attr, DDI_DMA_CONSISTENT,
	    DDI_DMA_DONTWAIT, NULL, (caddr_t *)&pool_ptr->cmd_base,
	    &real_cmd_buf_size, &pool_ptr->cmd_acc_handle) != DDI_SUCCESS) {
		goto fail;
	}

	if (ddi_dma_addr_bind_handle(pool_ptr->cmd_dma_handle,
		NULL, pool_ptr->cmd_base, real_cmd_buf_size,
		DDI_DMA_WRITE | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		NULL, &cookie, &ccount) != DDI_DMA_MAPPED) {
		goto fail;
	}
	cmd_bound++;

	if (ccount != 1) {
		goto fail;
	}

	if (ddi_dma_alloc_handle(pptr->ssfcpp_dip, &pptr->ssfcpp_dma_attr,
	    DDI_DMA_DONTWAIT, NULL, &pool_ptr->rsp_dma_handle) !=
	    DDI_SUCCESS) {
		goto fail;
	}
	/*
	 * Get a piece of memory in which to put responses
	 * ???? FCP_MAX_RSP_IU_SIZE is set to 128. FC_SONOMA needs more.
	 */

	rsp_buf_size = FCP_MAX_RSP_IU_SIZE * SSFCP_ELEMS_IN_POOL;
	if (ddi_dma_mem_alloc(pool_ptr->rsp_dma_handle, rsp_buf_size,
	    &(pptr->ssfcpp_dma_acc_attr), DDI_DMA_CONSISTENT,
	    DDI_DMA_DONTWAIT, NULL, (caddr_t *)&pool_ptr->rsp_base,
	    &real_rsp_buf_size, &pool_ptr->rsp_acc_handle) != DDI_SUCCESS) {
		goto fail;
	}

	if (ddi_dma_addr_bind_handle(pool_ptr->rsp_dma_handle,
		NULL, pool_ptr->rsp_base, real_rsp_buf_size,
		DDI_DMA_READ | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		NULL, &cookie, &ccount) != DDI_DMA_MAPPED) {
		goto fail;
	}
	rsp_bound++;

	if (ccount != 1) {
		goto fail;
	}

	/*
	 * Generate a (cmd/rsp structure) free list
	 */
	dptr = (caddr_t)((uintptr_t)(pool_ptr->cmd_base) + 7 & ~7);
	real_cmd_buf_size -= (dptr - pool_ptr->cmd_base);
	eptr = pool_ptr->rsp_base;

	pool_ptr->ntot = min((real_cmd_buf_size / sizeof (struct fcp_cmd)),
	    (real_rsp_buf_size / FCP_MAX_RSP_IU_SIZE));
	pool_ptr->nfree = pool_ptr->ntot;
	pool_ptr->free = (struct ssfcp_cr_free_elem *)pool_ptr->cmd_base;
	pool_ptr->pptr = pptr;

	for (i = 0; i < pool_ptr->ntot; i++) {
		cptr = (struct ssfcp_cr_free_elem *)dptr;
		dptr += sizeof (struct fcp_cmd);
		cptr->next = (struct ssfcp_cr_free_elem *)dptr;
		cptr->rsp = eptr;
		eptr += FCP_MAX_RSP_IU_SIZE;
	}

	/* terminate the list */
	cptr->next = NULL;

	mutex_enter(&pptr->ssfcp_cr_mutex);
	pool_ptr->next = pptr->ssfcp_cr_pool;
	pptr->ssfcp_cr_pool = pool_ptr;
	pptr->ssfcp_cr_pool_cnt++;
	mutex_exit(&pptr->ssfcp_cr_mutex);

	return (DDI_SUCCESS);

fail:
	if (pool_ptr->cmd_dma_handle != NULL) {
		if (cmd_bound) {
			(void) ddi_dma_unbind_handle(pool_ptr->cmd_dma_handle);
		}
		ddi_dma_free_handle(&pool_ptr->cmd_dma_handle);
	}

	if (pool_ptr->rsp_dma_handle != NULL) {
		if (rsp_bound) {
			(void) ddi_dma_unbind_handle(pool_ptr->rsp_dma_handle);
		}
		ddi_dma_free_handle(&pool_ptr->rsp_dma_handle);
	}

	if (pool_ptr->cmd_base != NULL) {
		ddi_dma_mem_free(&pool_ptr->cmd_acc_handle);
	}

	if (pool_ptr->rsp_base != NULL) {
		ddi_dma_mem_free(&pool_ptr->rsp_acc_handle);
	}

	kmem_free((caddr_t)pool_ptr, sizeof (struct ssfcp_cr_pool));
	return (DDI_FAILURE);
}


/*
 * called from ssfcp_scsi_init_pkt to allocate the cmd/response pool
 */
int
ssfcp_cr_alloc(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd, int (*func)())
{
	struct ssfcp_cr_pool		*ptr;
	struct ssfcp_cr_free_elem	*cptr;
	fc_packet_t			*fpkt;

	mutex_enter(&pptr->ssfcp_cr_mutex);
try_again:
	/* find free buffer pair */
	for (ptr = pptr->ssfcp_cr_pool; ptr != NULL; ptr = ptr->next) {
		if (ptr->nfree) {
			ptr->nfree--;
			break;
		}
	}

	/* we found a buffer pair */
	if (ptr != NULL) {

		cptr = ptr->free;
		ptr->free = cptr->next;
		mutex_exit(&pptr->ssfcp_cr_mutex);
		fpkt = cmd->cmd_fp_pkt;
		/*
		 * Get DVMA cookies for the fcp_cmd area.
		 */
		if (ddi_dma_htoc(ptr->cmd_dma_handle,
			(off_t)((caddr_t)cptr - ptr->cmd_base),
			&fpkt->pkt_cmd_cookie) != DDI_SUCCESS) {

			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!htoc failed for cmd");
			return (DDI_FAILURE);
		}
		/*
		 * Get DVMA cookies for the fcp_rsp area.
		 */
		if (ddi_dma_htoc(ptr->rsp_dma_handle,
			(off_t)((caddr_t)cptr->rsp - ptr->rsp_base),
			&fpkt->pkt_resp_cookie) != DDI_SUCCESS) {

			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!htoc failed for rsp");
			return (DDI_FAILURE);
		}
		cmd->cmd_block = (struct fcp_cmd *)cptr;
		fpkt->pkt_cmd = (caddr_t)cptr;
		fpkt->pkt_cmd_dma = ptr->cmd_dma_handle;
		fpkt->pkt_cmd_acc = ptr->cmd_acc_handle;
		fpkt->pkt_resp = cptr->rsp;
		fpkt->pkt_resp_dma = ptr->rsp_dma_handle;
		fpkt->pkt_resp_acc = ptr->rsp_acc_handle;
		cmd->cmd_rsp_block = (struct fcp_rsp *)cptr->rsp;
		cmd->cmd_cr_pool = ptr;
		return (DDI_SUCCESS);

	}

	if (pptr->ssfcp_cr_pool_cnt < SSFCP_CR_POOL_MAX) {
		if (pptr->ssfcp_cr_flag) {
			if (func == SLEEP_FUNC) {
				cv_wait(&pptr->ssfcp_cr_cv,
				    &pptr->ssfcp_cr_mutex);
				goto try_again;
			}
			mutex_exit(&pptr->ssfcp_cr_mutex);
#ifdef	KSTATS_CODE
			pptr->ssfcp_stats.cralloc_failures++;
#endif
			return (DDI_FAILURE);
		}

		pptr->ssfcp_cr_flag = 1;
		mutex_exit(&pptr->ssfcp_cr_mutex);
		if (ssfcp_add_cr_pool(pptr) != DDI_SUCCESS) {
			mutex_enter(&pptr->ssfcp_cr_mutex);
			pptr->ssfcp_cr_flag = 0;
			cv_broadcast(&pptr->ssfcp_cr_cv);
			mutex_exit(&pptr->ssfcp_cr_mutex);
#ifdef	KSTATS_CODE
			pptr->ssfcp_stat.cralloc_failures++;
#endif
			return (DDI_FAILURE);
		}
		mutex_enter(&pptr->ssfcp_cr_mutex);
		pptr->ssfcp_cr_flag = 0;
		cv_broadcast(&pptr->ssfcp_cr_cv);
		goto try_again;
	}

	mutex_exit(&pptr->ssfcp_cr_mutex);
#ifdef	KSTATS_CODE
	pptr->ssfcp_stats.cralloc_failures++;
#endif
	return (DDI_FAILURE);
}


void
ssfcp_cr_free(struct ssfcp_cr_pool *cp, struct ssfcp_pkt *cmd)
{
	struct ssfcp_port *pptr = cp->pptr;
	struct ssfcp_cr_free_elem *elem;

	elem = (struct ssfcp_cr_free_elem *)cmd->cmd_block;
	elem->rsp = (caddr_t)cmd->cmd_rsp_block;

	mutex_enter(&pptr->ssfcp_cr_mutex);
	cp->nfree++;
	ASSERT(cp->nfree <= cp->ntot);

	elem->next = cp->free;
	cp->free = elem;
	mutex_exit(&pptr->ssfcp_cr_mutex);
}


/*
 * free the cmd/response pool for this port
 *
 * acquires and frees the cr pool mutex for this port
 */
void
ssfcp_crpool_free(struct ssfcp_port *pptr)
{
	struct ssfcp_cr_pool *cp, *prev, *next;

	/* scan the cr pool, freeing all entries */
	mutex_enter(&pptr->ssfcp_cr_mutex);

	prev = NULL;
	next = pptr->ssfcp_cr_pool;
	while ((cp = next) != NULL) {
		next = cp->next;

		if (cp->nfree != cp->ntot) {
			cmn_err(CE_WARN, "fcp(%d) CR pool total %d "
			    " free %d don't match", pptr->ssfcpp_instance,
			    cp->ntot, cp->nfree);
		}

		/* free this empty entry and return */
		if (prev != NULL) {
			/* it was in the middle of the list */
			prev->next = cp->next;
		} else {
			/* it was at the head of the list */
			pptr->ssfcp_cr_pool = cp->next;
		}
		pptr->ssfcp_cr_pool_cnt--;

		/* this entry is now unlinked from the list, so free it */

		(void) ddi_dma_unbind_handle(cp->cmd_dma_handle);
		ddi_dma_mem_free(&cp->cmd_acc_handle);
		ddi_dma_free_handle(&cp->cmd_dma_handle);

		(void) ddi_dma_unbind_handle(cp->rsp_dma_handle);
		ddi_dma_mem_free(&cp->rsp_acc_handle);
		ddi_dma_free_handle(&cp->rsp_dma_handle);

		kmem_free((caddr_t)cp, sizeof (struct ssfcp_cr_pool));
	}

	mutex_exit(&pptr->ssfcp_cr_mutex);
}


/*
 * handle hotplug events for our instance
 *
 * this thread created at attach time, and it doesn't exit
 * until signalled to do so (by the ssfcp_hp_initted flag
 * being cleared)
 *
 * each loop, it handles all online and offline events on its queue
 * (there shouldn't be any other kind), then waits to be
 * kicked (by ssfcp_hp_cv) before either going again or stopping
 *
 * side effects: elements on the hotplug list for this instance are
 * removed and handled
 *
 * acquires and releases the hotplug mutex for this port
 */
static void
ssfcp_hp_daemon(void *arg)
{
	struct ssfcp_port *pptr = (struct ssfcp_port *)arg;
	struct ssfcp_hp_elem *elem;
	struct ssfcp_lun *plun;
	uint_t	ndi_count;
	callb_cpr_t cpr_info;

	CALLB_CPR_INIT(&cpr_info, &pptr->ssfcp_hp_mutex,
	    callb_generic_cpr, "ssfcp_hp_daemon");

	/* acquire the hotplug mutex for this instance */
	mutex_enter(&pptr->ssfcp_hp_mutex);
	CALLB_CPR_SAFE_BEGIN(&cpr_info);
loop:
	/* loop until there are no more elements to handle */
	CALLB_CPR_SAFE_END(&cpr_info, &pptr->ssfcp_hp_mutex);
	while (pptr->ssfcp_hp_elem_list != NULL) {

		/* get a pointer to the current hotplug element */
		elem = pptr->ssfcp_hp_elem_list;

		/*
		 * increment list pointer to point to the next element
		 */
		pptr->ssfcp_hp_elem_list = elem->next;

		/* unlink our element from the rest of the list */
		elem->next = NULL;

		/* release the hotplug list mutex for this instance */
		mutex_exit(&pptr->ssfcp_hp_mutex);
		plun = elem->lun;

		/* handle this element -- there should only be 2 kinds */
		switch (elem->what) {
		case SSFCP_ONLINE:
			mutex_enter(&pptr->ssfcp_mutex);
			if (pptr->ssfcp_state & (SSFCP_STATE_SUSPENDED |
			    SSFCP_STATE_DETACHING)) {
				mutex_exit(&pptr->ssfcp_mutex);
				break;
			}
			mutex_exit(&pptr->ssfcp_mutex);

			i_ndi_block_device_tree_changes(&ndi_count);

			mutex_enter(&plun->lun_mutex);
			if (plun->lun_dip == NULL ||
			    !(plun->lun_state & SSFCP_LUN_INIT)) {
				mutex_exit(&plun->lun_mutex);
				i_ndi_allow_device_tree_changes(ndi_count);
				break;
			}
			mutex_exit(&plun->lun_mutex);

			(void) ndi_devi_online(elem->dip, NDI_ONLINE_ATTACH);
			(void) ndi_event_run_callbacks(pptr->ssfcp_event_hdl,
			    plun->lun_dip, ssfcp_insert_eid, NULL,
			    NDI_EVENT_NOPASS);

			i_ndi_allow_device_tree_changes(ndi_count);
			break;

		case SSFCP_OFFLINE:
			/* don't do NDI_DEVI_REMOVE for now */
			mutex_enter(&pptr->ssfcp_mutex);
			if (pptr->ssfcp_state & (SSFCP_STATE_SUSPENDED |
			    SSFCP_STATE_DETACHING)) {
				mutex_exit(&pptr->ssfcp_mutex);
				break;
			}
			mutex_exit(&pptr->ssfcp_mutex);

			i_ndi_block_device_tree_changes(&ndi_count);

			mutex_enter(&plun->lun_mutex);
			if (plun->lun_dip == NULL ||
			    !(plun->lun_state & SSFCP_LUN_INIT)) {
				mutex_exit(&plun->lun_mutex);
				i_ndi_allow_device_tree_changes(ndi_count);

				break;
			}
			mutex_exit(&plun->lun_mutex);

			if (ndi_devi_offline(elem->dip, 0) != NDI_SUCCESS) {
				SSFCP_DEBUG(2, (CE_WARN, pptr->ssfcpp_dip,
				    "ssfcp_hp: target=%d, "
				    "device offline failed",
				    elem->lun->lun_tgt->tgt_d_id));
			} else {
				SSFCP_DEBUG(2, (CE_NOTE, pptr->ssfcpp_dip,
				    "ssfcp_hp: target=%d, "
				    "device offline succeeded\n",
				    elem->lun->lun_tgt->tgt_d_id));
			}

			i_ndi_allow_device_tree_changes(ndi_count);
			break;

		default:
			/* this shouldn't happen */
			ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
			    "!Invalid Hotplug opcode");
			break;
		}
		kmem_free(elem, sizeof (struct ssfcp_hp_elem));
		pptr->ssfcp_hp_nele--;
		mutex_enter(&pptr->ssfcp_hp_mutex);
	}

	/*
	 * wait here before deciding whether to go on or exit
	 *
	 * most of the time should be spent in this wait
	 */
	CALLB_CPR_SAFE_BEGIN(&cpr_info);
	cv_wait(&pptr->ssfcp_hp_cv, &pptr->ssfcp_hp_mutex);

	/* see if we have been signalled to stop (else continue on) */
	if (pptr->ssfcp_hp_initted) {
		goto loop;			/* go process more entries */
	}
	CALLB_CPR_SAFE_END(&cpr_info, &pptr->ssfcp_hp_mutex);

	/* somebody wants us to give up */
	cv_signal(&pptr->ssfcp_hp_cv);	/* signal we are stopping */
	CALLB_CPR_EXIT(&cpr_info);
	thread_exit();			/* no more hotplug thread */
}


/*
 * called when a timeout occurs
 *
 * can be scheduled during an attach or resume (if not already running)
 *
 * one timeout is set up for all ports
 *
 * acquires and releases the global mutex
 */
/*ARGSUSED*/
static void
ssfcp_watch(void *arg)
{
	struct ssfcp_port	*pptr;
	struct ssfcp_ipkt	*icmd;
	struct ssfcp_ipkt	*nicmd;
	struct ssfcp_pkt 	*cmd;
	struct ssfcp_pkt 	*ncmd;
	struct ssfcp_pkt 	*tail;
	struct ssfcp_pkt 	*pcmd;
	struct ssfcp_pkt	*save_head;
	struct ssfcp_port	*save_port;

	/* increment global watchdog time */
	ssfcp_watchdog_time += ssfcp_watchdog_timeout;

	mutex_enter(&ssfcp_global_mutex);

	/* scan each port in our list */
	for (pptr = ssfcp_port_head; pptr != NULL; pptr = pptr->ssfcp_next) {
		save_port = ssfcp_port_head;
		mutex_exit(&ssfcp_global_mutex);

		mutex_enter(&pptr->ssfcp_mutex);
		if (pptr->ssfcp_ipkt_list == NULL &&
		    (pptr->ssfcp_state & (SSFCP_STATE_SUSPENDED |
		    SSFCP_STATE_DETACHING))) {
			mutex_exit(&pptr->ssfcp_mutex);
			SSFCP_DEBUG(3, (CE_CONT, pptr->ssfcpp_dip,
			    "ssfcp_watch,sf%d:throttle disabled "
			    "due to DDI_SUSPEND\n",
			    pptr->ssfcpp_instance));
			mutex_enter(&ssfcp_global_mutex);
			goto end_of_watchdog;
		}
		pptr->ssfcp_state |= SSFCP_STATE_IN_WATCHDOG;
		mutex_exit(&pptr->ssfcp_mutex);

		if (pptr->ssfcp_take_core) {
			(void) fc_ulp_port_reset(pptr->ssfcpp_handle,
			    FC_RESET_DUMP);
		}

		mutex_enter(&pptr->ssfcp_mutex);
		if (pptr->ssfcp_reset_list)
			ssfcp_check_reset_delay(pptr);
		mutex_exit(&pptr->ssfcp_mutex);

		mutex_enter(&pptr->ssfcp_cmd_mutex);
		tail = pptr->ssfcp_pkt_tail;

		for (pcmd = NULL, cmd = pptr->ssfcp_pkt_head; cmd != NULL;
		    cmd = ncmd) {
			ncmd = cmd->cmd_next;
			ASSERT(cmd->cmd_flags & CFLAG_IN_QUEUE);
			/*
			 * SSFCP_INVALID_TIMEOUT will be set for those
			 * command that need to be failed. Mostly those
			 * cmds that could not be queued down for the
			 * "timeout" value. cmd->cmd_timeout is used
			 * to try and requeue the command regularly.
			 */
			if (cmd->cmd_timeout >= ssfcp_watchdog_time) {
				pcmd = cmd;
				goto end_of_loop;
			}

			if (cmd == pptr->ssfcp_pkt_head) {
				ASSERT(pcmd == NULL);
				pptr->ssfcp_pkt_head = cmd->cmd_next;
			} else {
				ASSERT(pcmd != NULL);
				pcmd->cmd_next = cmd->cmd_next;
			}

			if (cmd == pptr->ssfcp_pkt_tail) {
				ASSERT(cmd->cmd_next == NULL);
				pptr->ssfcp_pkt_tail = pcmd;
				if (pcmd)
					pcmd->cmd_next = NULL;
			}
			cmd->cmd_next = NULL;

			/*
			 * save the current head before dropping the
			 * mutex - If the head doesn't remain the
			 * same after re acquiring the mutex, just
			 * bail out and revisit on next tick.
			 *
			 * PS: The tail pointer can change as the commands
			 * get requeued after failure to retransport
			 */
			save_head = pptr->ssfcp_pkt_head;
			mutex_exit(&pptr->ssfcp_cmd_mutex);

			if (cmd->cmd_fp_pkt->pkt_timeout ==
			    SSFCP_INVALID_TIMEOUT) {
				cmd->cmd_state == SSFCP_PKT_ABORTING ?
				    ssfcp_fail_cmd(cmd, CMD_RESET,
				    STAT_DEV_RESET) : ssfcp_fail_cmd(cmd,
				    CMD_TIMEOUT, STAT_ABORTED);
			} else {
				ssfcp_retransport_cmd(pptr, cmd);
			}
			mutex_enter(&pptr->ssfcp_cmd_mutex);
			if (save_head && save_head != pptr->ssfcp_pkt_head) {
				/*
				 * Looks like linked list got changed (mostly
				 * happens when an an OFFLINE LUN code starts
				 * returning overflow queue commands in
				 * parallel. So bail out and revisit during
				 * next tick
				 */
				break;
			}
end_of_loop:
			/*
			 * Scan only upto the previously known tail pointer
			 * to avoid excessive processing - lots of new packets
			 * could have been added to the tail or the old ones
			 * re-queued.
			 */
			if (cmd == tail) {
				break;
			}
		}
		mutex_exit(&pptr->ssfcp_cmd_mutex);

		mutex_enter(&pptr->ssfcp_mutex);
		for (icmd = pptr->ssfcp_ipkt_list; icmd != NULL; icmd = nicmd) {
			struct ssfcp_tgt *ptgt = icmd->ipkt_tgt;
			struct ssfcp_lun *plun = icmd->ipkt_lun;

			nicmd = icmd->ipkt_next;
			if ((icmd->ipkt_timeout != 0) &&
			    (icmd->ipkt_timeout >= ssfcp_watchdog_time)) {
				/* packet has not timed out */
				continue;
			}
			/* time for packet re-transport */
			if (icmd == pptr->ssfcp_ipkt_list) {
				pptr->ssfcp_ipkt_list = icmd->ipkt_next;
				if (pptr->ssfcp_ipkt_list) {
					pptr->ssfcp_ipkt_list->ipkt_prev =
						NULL;
				}
			} else {
				icmd->ipkt_prev->ipkt_next = icmd->ipkt_next;
				if (icmd->ipkt_next) {
					icmd->ipkt_next->ipkt_prev =
						icmd->ipkt_prev;
				}
			}
			icmd->ipkt_next = NULL;
			icmd->ipkt_prev = NULL;
			mutex_exit(&pptr->ssfcp_mutex);

			if (++(icmd->ipkt_retries) < SSFCP_ICMD_RETRY_CNT) {
				mutex_enter(&ptgt->tgt_mutex);
				if (ptgt->tgt_change_cnt ==
						icmd->ipkt_change_cnt) {
					mutex_exit(&ptgt->tgt_mutex);
					if (ssfcp_transport(
						pptr->ssfcpp_handle,
						icmd->ipkt_fpkt, 1)
							== FC_SUCCESS) {
						mutex_enter
							(&pptr->ssfcp_mutex);
						continue;
					}
				} else
					mutex_exit(&ptgt->tgt_mutex);
			} else {
				switch (icmd->ipkt_opcode) {
					case LA_ELS_PLOGI:
						ssfcp_log(CE_WARN,
						    pptr->ssfcpp_dip, "!PLOGI "
						    " to d_id=0x%x failed. "
						    " Retry count: %d\n",
						    ptgt->tgt_d_id,
						    icmd->ipkt_retries);
						break;
					case LA_ELS_PRLI:
						ssfcp_log(CE_WARN,
						    pptr->ssfcpp_dip, "!PRLI to"
						    " d_id=0x%x failed. Retry "
						    "count: %d\n",
						    ptgt->tgt_d_id,
						    icmd->ipkt_retries);
						break;
					case SCMD_INQUIRY:
						ssfcp_log(CE_WARN,
						    pptr->ssfcpp_dip, "!Inquiry"
						    " to d_id=0x%x lun=0x%x "
						    "failed. Retry count:%d\n",
						    ptgt->tgt_d_id,
						    plun->lun_num,
						    icmd->ipkt_retries);
						break;
					case SCMD_REPORT_LUN:
						ssfcp_log(CE_WARN,
						    pptr->ssfcpp_dip, "!Report"
						    " Lun to d_id=0x%x lun=0x%x"
						    " failed. Retry count:%d\n",
						    ptgt->tgt_d_id,
						    plun->lun_num,
						    icmd->ipkt_retries);
					default:
						break;
				}
			}
			(void) ssfcp_call_finish_init(pptr, ptgt, icmd);
			ssfcp_icmd_free(icmd->ipkt_fpkt);
			mutex_enter(&pptr->ssfcp_mutex);
		}

		pptr->ssfcp_state &= ~SSFCP_STATE_IN_WATCHDOG;
		mutex_exit(&pptr->ssfcp_mutex);
		mutex_enter(&ssfcp_global_mutex);

end_of_watchdog:
		/*
		 * Bail out early before getting into trouble
		 */
		if (save_port != ssfcp_port_head) {
			break;
		}
	}

	mutex_exit(&ssfcp_global_mutex);

	/* reschedule timeout to go again */
	ssfcp_watchdog_id = timeout(ssfcp_watch, NULL, ssfcp_watchdog_tick);
}


void
ssfcp_check_reset_delay(struct ssfcp_port *pptr)
{
	struct ssfcp_reset_elem *rp, *tp = NULL;
	uint32_t		link_cnt;
	struct ssfcp_tgt	*ptgt;


	ASSERT(mutex_owned(&pptr->ssfcp_mutex));
	rp = pptr->ssfcp_reset_list;
	tp = (struct ssfcp_reset_elem *)&pptr->ssfcp_reset_list;
	while (rp != NULL) {
		if (rp->timeout >=  ssfcp_watchdog_time && rp->link_cnt ==
		    pptr->ssfcp_link_cnt) {
			tp->next = rp->next;
			mutex_exit(&pptr->ssfcp_mutex);
			ptgt = rp->tgt;
			link_cnt = rp->link_cnt;
			kmem_free(rp, sizeof (struct ssfcp_reset_elem));
			ssfcp_abort_all(pptr, ptgt, link_cnt);
			mutex_enter(&pptr->ssfcp_mutex);
			mutex_enter(&ptgt->tgt_mutex);
			if (link_cnt == pptr->ssfcp_link_cnt)
				ssfcp_update_tgt_state(ptgt, FCP_RESET,
						SSFCP_LUN_BUSY);
			mutex_exit(&ptgt->tgt_mutex);
			tp = (struct ssfcp_reset_elem *)
			    &pptr->ssfcp_reset_list;
			rp = pptr->ssfcp_reset_list;
		} else if (rp->link_cnt != pptr->ssfcp_link_cnt) {
			tp->next = rp->next;
			kmem_free(rp, sizeof (struct ssfcp_reset_elem));
			rp = tp->next;
		} else {
			tp = rp;
			rp = rp->next;
		}
	}

}


void
ssfcp_abort_all(struct ssfcp_port *pptr, struct ssfcp_tgt *ttgt,
					uint32_t link_cnt)
{
	struct ssfcp_pkt *pcmd = NULL, *ncmd = NULL,
			*cmd = NULL, *head = NULL, *tail = NULL;
	struct ssfcp_lun *tlun;
	int rval;

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	for (cmd = pptr->ssfcp_pkt_head; cmd != NULL; cmd = ncmd) {
		struct ssfcp_lun *plun = ADDR2LUN(&cmd->cmd_pkt->pkt_address);
		struct ssfcp_tgt *ptgt = plun->lun_tgt;

		ncmd = cmd->cmd_next;

		if (ptgt != ttgt) {
			pcmd = cmd;
			continue;
		}

		if (pcmd != NULL) {
			ASSERT(pptr->ssfcp_pkt_head != cmd);
			pcmd->cmd_next = ncmd;
		} else {
			ASSERT(cmd == pptr->ssfcp_pkt_head);
			pptr->ssfcp_pkt_head = ncmd;
		}
		if (pptr->ssfcp_pkt_tail == cmd) {
			ASSERT(cmd->cmd_next == NULL);
			pptr->ssfcp_pkt_tail = pcmd;
			if (pcmd != NULL)
				pcmd->cmd_next = NULL;
		}

		if (head == NULL) {
			head = tail = cmd;
		} else {
			ASSERT(tail != NULL);
			tail->cmd_next = cmd;
			tail = cmd;
		}
		cmd->cmd_next = NULL;
	}
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	for (cmd = head; cmd != NULL; cmd = ncmd) {
		struct scsi_pkt *pkt = cmd->cmd_pkt;

		ncmd = cmd->cmd_next;
		ASSERT(pkt != NULL);
		mutex_enter(&pptr->ssfcp_mutex);
		if (link_cnt == pptr->ssfcp_link_cnt) {
			mutex_exit(&pptr->ssfcp_mutex);
			cmd->cmd_flags &= ~CFLAG_IN_QUEUE;
			pkt->pkt_reason = CMD_RESET;
			pkt->pkt_statistics |= STAT_DEV_RESET;
			cmd->cmd_state = SSFCP_PKT_IDLE;
			cmd = cmd->cmd_next;
			if (pkt->pkt_comp) {
				(*pkt->pkt_comp) (pkt);
			}
		} else {
			mutex_exit(&pptr->ssfcp_mutex);
		}
	}

	/*
	 * If the FCA will return all the commands in its queue then our
	 * work is easy, just return.
	 */

	if (pptr->ssfcpp_cmds_aborted == FC_RESET_RETURN_ALL)
		return;

	/*
	 * There are some servere race conditions here.
	 * While we are trying to abort the pkt, it might be completing
	 * so mark it aborted and if the abort does not succeed then
	 * handle it in the watch thread.
	 */
	mutex_enter(&ttgt->tgt_mutex);
	tlun = ttgt->tgt_lun;
	mutex_exit(&ttgt->tgt_mutex);
	while (tlun != NULL) {
		int restart = 0;
		mutex_enter(&tlun->lun_mutex);
		cmd = tlun->lun_pkt_head;
		while (cmd != NULL) {
			if (cmd->cmd_state == SSFCP_PKT_ISSUED) {
				struct scsi_pkt *pkt;
				restart = 1;
				cmd->cmd_state = SSFCP_PKT_ABORTING;
				mutex_exit(&tlun->lun_mutex);
				rval = fc_ulp_abort(pptr->ssfcpp_handle,
					cmd->cmd_fp_pkt, KM_SLEEP);
				if (rval == FC_SUCCESS) {
					pkt = cmd->cmd_pkt;
					pkt->pkt_reason = CMD_RESET;
					pkt->pkt_statistics |= STAT_DEV_RESET;
					cmd->cmd_state = SSFCP_PKT_IDLE;
					if (pkt->pkt_comp) {
						(*pkt->pkt_comp) (pkt);
					}
				} else {

				/*
				 * This part is tricky. The abort failed
				 * and now the command could be completing.
				 * The cmd_state == SSFCP_PKT_ABORTING
				 * should save us in ssfcp_cmd_callback.
				 * If we are already aborting ignore the
				 * command in ssfcp_cmd_callback.
				 * Here we leave this packet for 20 sec
				 * to be aborted in the ssfcp_watch thread.
				 */
					ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
					"!Abort failed after reset with 0x%x",
					rval);
					cmd->cmd_timeout =
					    ssfcp_watchdog_time +
					    cmd->cmd_pkt->pkt_time +
					    SSFCP_FAILED_DELAY;
					cmd->cmd_fp_pkt->pkt_timeout =
						SSFCP_INVALID_TIMEOUT;
				/*
				 * This is a hack, cmd is put in the overflow
				 * queue so that it can be timed out
				 * finaly.
				 */
					cmd->cmd_flags |= CFLAG_IN_QUEUE;
					mutex_enter(&pptr->ssfcp_cmd_mutex);
					if (pptr->ssfcp_pkt_head) {
						ASSERT(pptr->ssfcp_pkt_tail
								!= NULL);
						pptr->ssfcp_pkt_tail->cmd_next
						    = cmd;
						pptr->ssfcp_pkt_tail = cmd;
					} else {
						ASSERT(pptr->ssfcp_pkt_tail
								== NULL);
						pptr->ssfcp_pkt_head =
							pptr->ssfcp_pkt_tail
								= cmd;
					}
					cmd->cmd_next = NULL;
					mutex_exit(&pptr->ssfcp_cmd_mutex);
				}
				mutex_enter(&tlun->lun_mutex);
				cmd = tlun->lun_pkt_head;
			} else {
				cmd = cmd->cmd_forw;
			}
		}
		mutex_exit(&tlun->lun_mutex);
		mutex_enter(&ttgt->tgt_mutex);
		restart == 1 ? (tlun = ttgt->tgt_lun) : (tlun = tlun->lun_next);
		mutex_exit(&ttgt->tgt_mutex);

		mutex_enter(&pptr->ssfcp_mutex);
		if (link_cnt != pptr->ssfcp_link_cnt) {
			mutex_exit(&pptr->ssfcp_mutex);
			return;
		} else
			mutex_exit(&pptr->ssfcp_mutex);
	}
}


/*
 * unlink the soft state, returning the soft state found (if any)
 *
 * acquires and releases the global mutex
 */
struct ssfcp_port *
ssfcp_soft_state_unlink(struct ssfcp_port *pptr)
{
	struct ssfcp_port	*hptr;		/* ptr index */
	struct ssfcp_port	*tptr;		/* prev hptr */

	mutex_enter(&ssfcp_global_mutex);
	for (hptr = ssfcp_port_head, tptr = NULL;
	    hptr != NULL;
	    tptr = hptr, hptr = hptr->ssfcp_next) {
		if (hptr == pptr) {
			/* we found a match -- remove this item */
			if (tptr == NULL) {
				/* we're at the head of the list */
				ssfcp_port_head = hptr->ssfcp_next;
			} else {
				tptr->ssfcp_next = hptr->ssfcp_next;
			}
			break;			/* success */
		}
	}
	mutex_exit(&ssfcp_global_mutex);
	return (hptr);
}


/*
 * called by ssfcp_scsi_hba_tgt_init to find a LUN given a
 * WWN and a LUN number
 */
/* ARGSUSED */
static struct ssfcp_lun *
ssfcp_lookup_lun(struct ssfcp_port *pptr, uchar_t *wwn, int lun)
{
	int hash;
	struct ssfcp_tgt *ptgt;
	struct ssfcp_lun *plun;

	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	hash = SSFCP_HASH(wwn);

	for (ptgt = pptr->ssfcp_wwn_list[hash];
	    ptgt != NULL;
	    ptgt = ptgt->tgt_next) {
		if (bcmp((caddr_t)wwn, (caddr_t)&ptgt->tgt_port_wwn.raw_wwn[0],
		    sizeof (ptgt->tgt_port_wwn)) == 0) {
			mutex_enter(&ptgt->tgt_mutex);
			for (plun = ptgt->tgt_lun;
			    plun != NULL;
			    plun = plun->lun_next) {
				if (plun->lun_num == (uchar_t)lun) {
					mutex_exit(&ptgt->tgt_mutex);
					return (plun);
				}
			}
			mutex_exit(&ptgt->tgt_mutex);
			return (NULL);
		}
	}
	return (NULL);
}


/*
 * prepare a SCSI cmd pkt for ssfcp_start()
 */
static int
ssfcp_prepare_pkt(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd,
    struct ssfcp_lun *plun)
{
	struct fcp_cmd		*fcmd = cmd->cmd_block;
	fc_packet_t		*fpkt = cmd->cmd_fp_pkt;
	struct ssfcp_tgt	*ptgt = plun->lun_tgt;

	cmd->cmd_pkt->pkt_reason = CMD_CMPLT;
	cmd->cmd_pkt->pkt_state = 0;
	cmd->cmd_pkt->pkt_statistics = 0;
	fpkt->pkt_comp = ssfcp_cmd_callback;
	fpkt->pkt_tran_flags = (FC_TRAN_CLASS3 | FC_TRAN_INTR);

	if ((cmd->cmd_pkt->pkt_comp == NULL) &&
	    !(cmd->cmd_pkt->pkt_flags & FLAG_NOINTR)) {
		return (TRAN_BADPKT);
	}

	if (cmd->cmd_pkt->pkt_flags & FLAG_NOINTR) {
		fpkt->pkt_tran_flags |= FC_TRAN_NO_INTR;
		fpkt->pkt_tran_flags &= ~FC_TRAN_INTR;
		fpkt->pkt_comp = NULL;
	}

	if (cmd->cmd_pkt->pkt_time) {
		fpkt->pkt_timeout = cmd->cmd_pkt->pkt_time;
	} else {
		fpkt->pkt_timeout = 5 * 60 * 60;
	}

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		cmd->cmd_pkt->pkt_resid = cmd->cmd_dmacount;
	} else {
		cmd->cmd_pkt->pkt_resid = 0;
	}

	/* set up the Tagged Queuing type */
	if (cmd->cmd_pkt->pkt_flags & FLAG_HTAG) {
		fcmd->fcp_cntl.cntl_qtype = FCP_QTYPE_HEAD_OF_Q;
	} else if (cmd->cmd_pkt->pkt_flags & FLAG_OTAG) {
		fcmd->fcp_cntl.cntl_qtype = FCP_QTYPE_ORDERED;
	}
	fpkt->pkt_pd = ptgt->tgt_pd_handle;

	ssfcp_fill_ids(pptr, cmd, plun);
	return (TRAN_ACCEPT);
}


static void
ssfcp_fill_ids(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd,
					struct ssfcp_lun *plun)
{
	fc_packet_t		*fpkt = cmd->cmd_fp_pkt;
	fc_frame_hdr_t		*hp;
	struct ssfcp_tgt	*ptgt = plun->lun_tgt;


	hp = &fpkt->pkt_cmd_fhdr;
	hp->d_id = ptgt->tgt_d_id;
	hp->s_id = pptr->ssfcpp_sid;
}


/*
 * called to do polled I/O by ssfcp_start()
 *
 * return a transport status value, i.e. TRAN_ACCECPT for success
 */
static int
ssfcp_dopoll(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd)
{
	int	rval;

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	pptr->ssfcp_ncmds++;
	mutex_exit(&pptr->ssfcp_cmd_mutex);

	if (cmd->cmd_fp_pkt->pkt_timeout) {
		cmd->cmd_fp_pkt->pkt_timeout = cmd->cmd_pkt->pkt_time;
	} else {
		cmd->cmd_fp_pkt->pkt_timeout = SSFCP_POLL_TIMEOUT;
	}

	ASSERT(cmd->cmd_fp_pkt->pkt_comp == NULL);

	cmd->cmd_state = SSFCP_PKT_ISSUED;

	rval = fc_ulp_transport(pptr->ssfcpp_handle, cmd->cmd_fp_pkt);

	mutex_enter(&pptr->ssfcp_cmd_mutex);
	pptr->ssfcp_ncmds--;
	mutex_exit(&pptr->ssfcp_cmd_mutex);
	cmd->cmd_state = SSFCP_PKT_IDLE;

	switch (rval) {
	case FC_SUCCESS:
		ssfcp_complete_pkt(cmd->cmd_fp_pkt);
		rval = TRAN_ACCEPT;
		break;

	case FC_TRAN_BUSY:
		rval = TRAN_BUSY;
		cmd->cmd_pkt->pkt_resid = 0;
		break;

	case FC_BADPACKET:
		rval = TRAN_BADPKT;
		break;

	default:
		rval = TRAN_FATAL_ERROR;
		break;
	}

	return (rval);
}


/*
 * called by some of the following transport-called routines to convert
 * a supplied dip ptr to a port struct ptr (i.e. to the soft state)
 */
static struct ssfcp_port *
ssfcp_dip2port(dev_info_t *dip)
{
	int		instance;


	instance = ddi_get_instance(dip);
	return (ddi_get_soft_state(ssfcp_softstate, instance));
}


/*
 * called internally to return a LUN given a dip
 */
struct ssfcp_lun *
ssfcp_get_lun_from_dip(struct ssfcp_port *pptr, dev_info_t *dip)
{
	struct ssfcp_tgt *ptgt;
	struct ssfcp_lun *plun;
	int i;


	ASSERT(mutex_owned(&pptr->ssfcp_mutex));

	for (i = 0; i < SSFCP_NUM_HASH; i++) {
		for (ptgt = pptr->ssfcp_wwn_list[i];
		    ptgt != NULL;
		    ptgt = ptgt->tgt_next) {
			mutex_enter(&ptgt->tgt_mutex);
			for (plun = ptgt->tgt_lun;
			    plun != NULL;
			    plun = plun->lun_next) {
				if (plun->lun_dip == dip) {
					mutex_exit(&ptgt->tgt_mutex);
					return (plun); /* match found */
				}
			}
			mutex_exit(&ptgt->tgt_mutex);
		}
	}
	return (NULL);				/* no LUN found */
}


/*
 * pass an element to the hotplug list, and then
 * kick the hotplug thread
 *
 * return Boolean success, i.e. non-zero if all goes well, else zero on error
 *
 * acquires/releases the hotplug mutex
 *
 * called with the port mutex owned
 *
 * memory acquired in NOSLEEP mode
 */
int
ssfcp_pass_to_hp(struct ssfcp_port *pptr, struct ssfcp_lun *plun, int what)
{
	struct ssfcp_hp_elem	*elem;

	ASSERT(pptr != NULL);
	ASSERT(plun != NULL);
	ASSERT(plun->lun_tgt != NULL);
	ASSERT(mutex_owned(&plun->lun_tgt->tgt_mutex));

	/* create space for a hotplug element */
	if ((elem = kmem_zalloc(sizeof (struct ssfcp_hp_elem), KM_NOSLEEP)) ==
	    NULL) {
		ssfcp_log(CE_WARN, NULL,
		    "!can't allocate memory for hotplug element");
		return (0);			/* failure */
	}

	/* fill in hoplug element */
	elem->lun = plun;
	elem->dip = plun->lun_dip;
	elem->what = what;

	/* place element on hoplug list and kick hotplug thread */
	mutex_enter(&pptr->ssfcp_hp_mutex);
	elem->next = pptr->ssfcp_hp_elem_list;
	pptr->ssfcp_hp_elem_list = elem;
	pptr->ssfcp_hp_nele++;
	cv_signal(&pptr->ssfcp_hp_cv);
	mutex_exit(&pptr->ssfcp_hp_mutex);
	return (1);
}


static void
ssfcp_retransport_cmd(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd)
{
	int			rval;
	struct scsi_address 	*ap;
	struct ssfcp_lun 	*plun;

	ap = &cmd->cmd_pkt->pkt_address;
	plun = ADDR2LUN(ap);

	ASSERT(cmd->cmd_flags & CFLAG_IN_QUEUE);

	cmd->cmd_state = SSFCP_PKT_IDLE;

	if ((plun->lun_state & (SSFCP_LUN_BUSY | SSFCP_LUN_OFFLINE)) == 0) {
		cmd->cmd_state = SSFCP_PKT_ISSUED;

		rval = ssfcp_transport(pptr->ssfcpp_handle, cmd->cmd_fp_pkt, 0);
		if (rval == FC_SUCCESS) {
			return;
		}
		cmd->cmd_state &= ~SSFCP_PKT_ISSUED;
	}
	ssfcp_queue_pkt(pptr, cmd);
}


static void
ssfcp_fail_cmd(struct ssfcp_pkt *cmd, uchar_t reason, uint_t statistics)
{
	ASSERT(cmd->cmd_flags & CFLAG_IN_QUEUE);

	cmd->cmd_flags &= ~CFLAG_IN_QUEUE;
	cmd->cmd_state = SSFCP_PKT_IDLE;

	cmd->cmd_pkt->pkt_reason = reason;
	cmd->cmd_pkt->pkt_state = 0;
	cmd->cmd_pkt->pkt_statistics = statistics;

	if (cmd->cmd_pkt->pkt_comp) {
		(*cmd->cmd_pkt->pkt_comp) (cmd->cmd_pkt);
	}
}


void
ssfcp_queue_pkt(struct ssfcp_port *pptr, struct ssfcp_pkt *cmd)
{
	mutex_enter(&pptr->ssfcp_cmd_mutex);
	cmd->cmd_flags |= CFLAG_IN_QUEUE;
	ASSERT(cmd->cmd_state != SSFCP_PKT_ISSUED);
	cmd->cmd_timeout = ssfcp_watchdog_time + SSFCP_QUEUE_DELAY;

	/*
	 * zero pkt_time means hang around for ever
	 */
	if (cmd->cmd_pkt->pkt_time) {
		if (cmd->cmd_fp_pkt->pkt_timeout > SSFCP_QUEUE_DELAY) {
			cmd->cmd_fp_pkt->pkt_timeout -= SSFCP_QUEUE_DELAY;
		} else {
			/*
			 * Indicate the watch thread to fail the
			 * command by setting it to highest value
			 */
			cmd->cmd_timeout = ssfcp_watchdog_time;
			cmd->cmd_fp_pkt->pkt_timeout = SSFCP_INVALID_TIMEOUT;
		}
	}

	if (pptr->ssfcp_pkt_head) {
		ASSERT(pptr->ssfcp_pkt_tail != NULL);

		pptr->ssfcp_pkt_tail->cmd_next = cmd;
		pptr->ssfcp_pkt_tail = cmd;
	} else {
		ASSERT(pptr->ssfcp_pkt_tail == NULL);

		pptr->ssfcp_pkt_head = pptr->ssfcp_pkt_tail = cmd;
	}
	cmd->cmd_next = NULL;
	mutex_exit(&pptr->ssfcp_cmd_mutex);
}


static void
ssfcp_update_targets(struct ssfcp_port *pptr, fc_portmap_t *dev_list,
    uint32_t count, uint32_t state)
{
	fc_portmap_t		*map_entry;
	struct ssfcp_tgt	*ptgt;

	mutex_enter(&pptr->ssfcp_mutex);
	while (count--) {
		map_entry = &(dev_list[count]);
		ptgt = ssfcp_lookup_target(pptr,
		    (uchar_t *)&(map_entry->map_pwwn));
		if (ptgt == NULL) {
			continue;
		}
		mutex_enter(&ptgt->tgt_mutex);
		ptgt->tgt_change_cnt++;
		ssfcp_update_tgt_state(ptgt, FCP_SET, state);
		mutex_exit(&ptgt->tgt_mutex);
	}
	mutex_exit(&pptr->ssfcp_mutex);
}


static int
ssfcp_call_finish_init(struct ssfcp_port *pptr, struct ssfcp_tgt *ptgt,
					struct ssfcp_ipkt *icmd)
{
	int	rval = SSFCP_NO_CHANGE;

	mutex_enter(&pptr->ssfcp_mutex);
	if (pptr->ssfcp_link_cnt != icmd->ipkt_link_cnt) {
		rval = SSFCP_LINK_CHANGE;
	} else {
		mutex_enter(&ptgt->tgt_mutex);
		if (ptgt->tgt_change_cnt == icmd->ipkt_change_cnt) {
			ASSERT(ptgt->tgt_tmp_cnt > 0);
			if (--ptgt->tgt_tmp_cnt == 0) {
				mutex_exit(&ptgt->tgt_mutex);
				ASSERT(pptr->ssfcp_tmp_cnt > 0);
				if (--pptr->ssfcp_tmp_cnt == 0) {
					ssfcp_finish_init(pptr,
						pptr->ssfcp_link_cnt);
				}
			} else {
				mutex_exit(&ptgt->tgt_mutex);
			}
		} else {
			mutex_exit(&ptgt->tgt_mutex);

			/*
			 * We need this  tgt decremented flag to make sure
			 * that we do --ssfcp_tmp_cnt once for a target.
			 * This function is called multiple times maybe
			 * once for each lun. This should only happen
			 * once for a target.
			 */
			if (!ptgt->tgt_decremented) {
				ptgt->tgt_decremented = 1;
				ASSERT(pptr->ssfcp_tmp_cnt > 0);
				if (--pptr->ssfcp_tmp_cnt == 0) {
					ssfcp_finish_init(pptr,
						pptr->ssfcp_link_cnt);
				}
			}
			rval = SSFCP_DEV_CHANGE;
		}
	}
	mutex_exit(&pptr->ssfcp_mutex);
	return (rval);
}


static void
ssfcp_reconfigure_luns(void * tgt_handle)
{
	struct ssfcp_tgt *ptgt = (struct ssfcp_tgt *)tgt_handle;
	struct ssfcp_port *pptr = ptgt->tgt_port;
	fc_portmap_t *devlist;
	uint32_t	dev_cnt;
	timeout_id_t	tid;

	if ((devlist = (fc_portmap_t *)kmem_zalloc(
	    sizeof (fc_portmap_t) * 1, KM_NOSLEEP)) == NULL) {
		ssfcp_log(CE_WARN, pptr->ssfcpp_dip,
		    "!fcp%d: failed to allocate for portmap",
		    pptr->ssfcpp_instance);
	}

	dev_cnt = 1;
	devlist->map_pd = ptgt->tgt_pd_handle;
	devlist->MAP_HARD_ADDR = ptgt->tgt_hard_addr;
	devlist->MAP_PORT_ID = ptgt->tgt_d_id;

	bcopy(&ptgt->tgt_node_wwn.raw_wwn[0], &devlist->map_nwwn, FC_WWN_SIZE);
	bcopy(&ptgt->tgt_port_wwn.raw_wwn[0], &devlist->map_pwwn, FC_WWN_SIZE);

	devlist->map_state = PORT_DEVICE_LOGGED_IN;
	devlist->map_flags = PORT_DEVICE_NOCHANGE;

	ssfcp_statec_callback(NULL, pptr->ssfcpp_handle, pptr->ssfcpp_state,
	    pptr->ssfcpp_top, devlist, dev_cnt, pptr->ssfcpp_sid);

	if (ptgt->tgt_tid) {
		tid = ptgt->tgt_tid;
		mutex_enter(&ptgt->tgt_mutex);
		ptgt->tgt_tid = NULL;
		mutex_exit(&ptgt->tgt_mutex);
		(void) untimeout(tid);
	}
}


static void
ssfcp_free_targets(struct ssfcp_port *pptr)
{
	int i;
	struct ssfcp_tgt *ptgt;
	struct ssfcp_lun *plun;
	mutex_enter(&pptr->ssfcp_mutex);
	for (i = 0; i < SSFCP_NUM_HASH; i++) {
		ptgt = pptr->ssfcp_wwn_list[i];
		while (ptgt != NULL) {
			struct ssfcp_tgt *next_tgt = ptgt->tgt_next;

			mutex_enter(&ptgt->tgt_mutex);

			plun = ptgt->tgt_lun;
			while (plun != NULL) {
				struct ssfcp_lun *next_lun = plun->lun_next;
				ssfcp_dealloc_lun(plun);

				plun = next_lun;
			}

			mutex_exit(&ptgt->tgt_mutex);
			ssfcp_dealloc_tgt(ptgt);

			ptgt = next_tgt;
		}
	}
	mutex_exit(&pptr->ssfcp_mutex);
}
