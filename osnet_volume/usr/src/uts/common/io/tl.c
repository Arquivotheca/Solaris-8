/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tl.c	1.50	99/09/23 SMI"

/*
 *  Multithreaded STREAMS Local Transport Provider
 *
 * This driver provides TLI as well as socket semantics.
 * It goes into socket mode when receiving the TL_IOC_SOCKET ioctl.
 * The socket and TLI modes have separate name spaces (i.e. it is not
 * possible to connect from a socket to a TLI endpoint) - this removes
 * any name space conflicts when binding to socket style transport addresses.
 * The sockets in addition have the following semantic differences:
 *	No support for passing up credentials (TL_SETCRED).
 *
 *	Options are passed through transparently on T_CONN_REQ to T_CONN_IND,
 *	from T_UNITDATA_REQ to T_UNIDATA_IND, and from T_OPTDATA_REQ to
 *	T_OPTDATA_IND.
 *
 *	The T_CONN_CON is generated when processing the T_CONN_REQ i.e. before
 *	a T_CONN_RES is received from the acceptor. This means that a socket
 *	connect will complete before the peer has called accept.
 */


#include	<sys/types.h>
#include	<sys/inttypes.h>
#include	<sys/stream.h>
#include	<sys/stropts.h>
#define	_SUN_TPI_VERSION 2
#include	<sys/tihdr.h>
#include	<sys/strlog.h>
#include	<sys/debug.h>
#include	<sys/cred.h>
#include	<sys/errno.h>
#include	<sys/kmem.h>
#include	<sys/mkdev.h>
#include	<sys/tl.h>
#include	<sys/stat.h>
#include	<sys/conf.h>
#include	<sys/modctl.h>
#include	<sys/strsun.h>
#include	<sys/socket.h>
#include	<sys/xti_xtiopt.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<inet/common.h>
#include	<inet/optcom.h>

/*
 * TBD List
 * 14 Eliminate state changes through table
 * 16. AF_UNIX socket options
 * 17. connect() for ticlts
 * 18. support for"netstat" to show AF_UNIX plus TLI local
 *	transport connections
 * 20. clts flow controls - optimize single sender
 * 21. sanity check to flushing on sending M_ERROR
 */

/*
 * CONSTANT DECLARATIONS
 * --------------------
 */

/*
 * Local declarations
 */
#ifdef TL_DEBUG
#define	NEXTSTATE(EV, ST)	change_and_log_state(EV, ST)
static int32_t change_and_log_state(int32_t, int32_t);
#else
#define	NEXTSTATE(EV, ST)	ti_statetbl[EV][ST]
#endif

#define	NR	127
#define	TL_DEFAULTADDRSZ sizeof (int)
#define	TL_MAXQLEN	32	/* Max conn indications allowed. */
#define	BADSEQNUM	(-1)	/* initial seq number used by T_DICON_IND */
#define	TL_BUFWAIT	(1*hz)	/* wait for allocb buffer timeout */
#define	TL_CLTS_CACHADDR_MAX 32	/* max size of addr cache for clts provider */
#define	TL_TIDUSZ (64*1024)	/* tidu size when "strmsgz" is unlimited (0) */

/*
 * Definitions for module_info
 */
#define		TL_ID		(104)		/* module ID number */
#define		TL_NAME		"tl"		/* module name */
#define		TL_MINPSZ	(0)		/* min packet size */
#define		TL_MAXPSZ	INFPSZ 		/* max packet size ZZZ */
#define		TL_HIWAT	(16*1024)	/* hi water mark */
#define		TL_LOWAT	(256)		/* lo water mark */

/*
 * Definition of minor numbers/modes for new transport provider modes.
 * We view the socket use as a separate mode to get a separate name space.
 */
#define		TL_TICOTS	0	/* connection oriented transport */
#define		TL_TICOTSORD 	1	/* COTS w/ orderly release */
#define		TL_TICLTS 	2	/* connectionless transport */

#define		TL_SOCKET	4	/* Socket */
#define		TL_SOCK_COTS	(TL_SOCKET|TL_TICOTS)
#define		TL_SOCK_COTSORD	(TL_SOCKET|TL_TICOTSORD)
#define		TL_SOCK_CLTS	(TL_SOCKET|TL_TICLTS)

#define		TL_SERVTYPEMASK	0x3	/* To mask off above */
#define		TL_MINOR_START	(TL_SOCK_CLTS + 1) /* start of minor numbers */

/*
 * LOCAL MACROS
 */
#define	T_ALIGN(p)	(((uintptr_t)(p) + (sizeof (t_scalar_t) - 1))\
						&~ (sizeof (t_scalar_t) - 1))

#define	TL_EQADDR(ap1, ap2)	(((ap2)->ta_alen <= 0) || \
		((ap1)->ta_alen != (ap2)->ta_alen) ? 0 : \
		(bcmp((ap1)->ta_abuf, (ap2)->ta_abuf, (ap2)->ta_alen) == 0))

#define	TL_OPTMGMT_REQ_PRIM(x)	((x) == T_OPTMGMT_REQ || \
					(x) == T_SVR4_OPTMGMT_REQ)
/*
 * EXTERNAL VARIABLE DECLARATIONS
 * -----------------------------
 */
/*
 * state table defined in the OS space.c
 */
extern	char	ti_statetbl[TE_NOEVENTS][TS_NOSTATES];

/*
 * used for TIDU size - defined in sun/conf/param.c
 */
extern ssize_t strmsgsz;

/*
 * STREAMS DRIVER ENTRY POINTS PROTOTYPES
 */
static int tl_open(queue_t *, dev_t *, int, int, cred_t *);
static int tl_close(queue_t *, int, cred_t *);
static void tl_wput(queue_t *, mblk_t *);
static void tl_wsrv(queue_t *);
static void tl_rsrv(queue_t *);
static int tl_identify(dev_info_t *);
static int tl_attach(dev_info_t *, ddi_attach_cmd_t);
static int tl_detach(dev_info_t *, ddi_detach_cmd_t);
static int tl_info(dev_info_t *, ddi_info_cmd_t, void *, void **);


/*
 * GLOBAL DATA STRUCTURES AND VARIABLES
 * -----------------------------------
 */

/*
 * Table representing database of all options managed by T_SVR4_OPTMGMT_REQ
 * Currently no such options are managed  but we have harmless dummy
 * options to make things work with some common code we access.
 * Maybe someday we might to do AF_UNIX option management here and
 * then we will need to intialize this array.
 */
opdes_t	tl_opt_arr[] = {
	/* The SO_TYPE is needed for the hack below */
	{
		SO_TYPE,
		SOL_SOCKET,
		OA_R,
		OA_R,
		OP_PASSNEXT,
		sizeof (t_scalar_t),
		0
	},
};

/*
 * Table of all supported levels
 * Note: Some levels (e.g. XTI_GENERIC) may be valid but may not have
 * any supported options so we need this info separately.
 *
 * This is needed only for topmost tpi providers.
 */
optlevel_t	tl_valid_levels_arr[] = {
	XTI_GENERIC,
	SOL_SOCKET,
	TL_PROT_LEVEL
};

#define	TL_VALID_LEVELS_CNT	A_CNT(tl_valid_levels_arr)
/*
 * Current upper bound on the amount of space needed to return all options.
 * Additional options with data size of sizeof(long) are handled automatically.
 * Others need hand job.
 */
#define	TL_MAX_OPT_BUF_LEN						\
		((A_CNT(tl_opt_arr) << 2) +				\
		(A_CNT(tl_opt_arr) * sizeof (struct opthdr)) +		\
		+ 64 + sizeof (struct T_optmgmt_ack))

#define	TL_OPT_ARR_CNT	A_CNT(tl_opt_arr)

/*
 *	transport addr structure
 */
typedef struct tl_addr {
	t_scalar_t	ta_alen;		/* length of abuf */
	char		*ta_abuf;		/* the addr itself */
} tl_addr_t;

/*
 *	transport endpoint structure
 */

typedef struct tl_endpt {
	struct tl_endpt *te_nextp; 	/* next in list of open endpoints */
	struct tl_endpt *te_prevp;	/* prev in list of open endpoints */
	int32_t 	te_state;	/* TPI state of endpoint */
	int32_t		te_servtype;	/* ticots, ticotsord or ticlts */
	int32_t		te_mode;	/* te_servtype + name space */
	tl_addr_t	te_ap;		/* addr bound to this endpt */
	queue_t		*te_rq;		/* stream read queue */
	minor_t		te_minor;	/* minor number */
	t_scalar_t	te_seqno;	/* unique sequence number */
	uint32_t	te_flag;	/* flag field */
	t_uscalar_t	te_qlen;	/* max conn requests */
	t_uscalar_t	te_nicon;	/* count of conn requests */
	struct tl_icon	*te_iconp; 	/* list of conn indications pending */
	struct tl_endpt *te_oconp;	/* outgoing conn request pending */
	struct tl_endpt *te_conp;	/* connected endpt */
	queue_t		*te_backwq;	/* back pointer for qenable-TBD */
	queue_t		*te_flowrq;	/* flow controlled on whom */
	bufcall_id_t	te_bufcid;	/* outstanding bufcall id */
	timeout_id_t	te_timoutid;	/* outstanding timeout id */
	tl_credopt_t	te_cred;	/* endpoint user credentials */
	struct tl_endpt	*te_lastep;	/* ticlts last dest. endpoint */
	tl_addr_t	te_lastdest;	/* ticlts last destination */
	t_uscalar_t	te_acceptor_id;	/* acceptor id for T_CONN_RES */
} tl_endpt_t;

#define	TL_SETCRED	0x1	/* flag to indicate sending of credentials */
#define	TL_CLOSING	0x2	/* flag to indicating we are closing */


/*
 * Data structure used to represent pending connects.
 * Records enough information so that the connecting peer can close
 * before the connection gets accepted.
 *
 * The following comment is not correct for this implementation, because
 * sequence numbers are now just minor numbers, but work around ti_freetep
 * still exists.
 *
 * Since kernel address are used as connection sequence numbers the
 * tl_endpt_t can not be freed until the tl_icon_t structure containing
 * the sequence number is freed (in order to prevent multiple connections
 * with the same seqno). ti_freetep is used for this purpose.
 */
typedef struct tl_icon {
	struct tl_icon *ti_next;
	t_scalar_t	ti_seqno;	/* Sequence number */
	struct tl_endpt *ti_tep;	/* NULL if peer has already closed */
	mblk_t		*ti_mp;		/* b_next list of data + ordrel_ind */
	struct tl_endpt	*ti_freetep;	/* Deferred free of tep */
} tl_icon_t;

static	struct	module_info	tl_minfo = {
	TL_ID,			/* mi_idnum */
	TL_NAME,		/* mi_idname */
	TL_MINPSZ,		/* mi_minpsz */
	TL_MAXPSZ,		/* mi_maxpsz */
	TL_HIWAT,		/* mi_hiwat */
	TL_LOWAT		/* mi_lowat */
};

static	struct	qinit	tl_rinit = {
	NULL,			/* qi_putp */
	(int (*)())tl_rsrv,	/* qi_srvp */
	tl_open,		/* qi_qopen */
	tl_close,		/* qi_qclose */
	NULL,			/* qi_qadmin */
	&tl_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

static	struct	qinit	tl_winit = {
	(int (*)())tl_wput,	/* qi_putp */
	(int (*)())tl_wsrv,	/* qi_srvp */
	NULL,			/* qi_qopen */
	NULL,			/* qi_qclose */
	NULL,			/* qi_qadmin */
	&tl_minfo,		/* qi_minfo */
	NULL			/* qi_mstat */
};

static	struct streamtab	tlinfo = {
	&tl_rinit,		/* st_rdinit */
	&tl_winit,		/* st_wrinit */
	NULL,			/* st_muxrinit */
	NULL			/* st_muxwrinit */
};

static struct cb_ops cb_tl_ops = {
	nulldev,				/* cb_open */
	nulldev,				/* cb_close */
	nodev,					/* cb_strategy */
	nodev,					/* cb_print */
	nodev,					/* cb_dump */
	nodev,					/* cb_read */
	nodev,					/* cb_write */
	nodev,					/* cb_ioctl */
	nodev,					/* cb_devmap */
	nodev,					/* cb_mmap */
	nodev,					/* cb_segmap */
	nochpoll,				/* cb_chpoll */
	ddi_prop_op,				/* cb_prop_op */
	&tlinfo,				/* cb_stream */
	(int)(D_NEW|D_MP|D_MTQPAIR|D_MTOUTPERIM|D_MTOCEXCL)
						/* cb_flag */
};

static struct dev_ops tl_ops  = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	tl_info,		/* devo_getinfo */
	tl_identify,		/* devo_identify */
	nulldev,		/* devo_probe */
	tl_attach,		/* devo_attach */
	tl_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_tl_ops,		/* devo_tl_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module -- pseudo driver here */
	"TPI Local Transport Driver - tl",
	&tl_ops,		/* driver ops */
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/*
 * default address declaration - used for default
 * address generation heuristic
 */
static t_scalar_t	tl_defaultabuf = 0x1000;
static tl_addr_t	tl_defaultaddr = {TL_DEFAULTADDRSZ,
						(char *)&tl_defaultabuf};
/*
 * Templates for response to info request
 * Check sanity of unlimited connect data etc.
 */

#define		TL_CLTS_PROVIDER_FLAG	(XPG4_1|SENDZERO)
#define		TL_COTS_PROVIDER_FLAG	(XPG4_1|SENDZERO)

static struct T_info_ack tl_cots_info_ack =
	{
		T_INFO_ACK,	/* PRIM_type -always T_INFO_ACK */
		T_INFINITE,	/* TSDU size */
		T_INFINITE,	/* ETSDU size */
		T_INFINITE,	/* CDATA_size */
		T_INFINITE,	/* DDATA_size */
		T_INFINITE,	/* ADDR_size  */
		T_INFINITE,	/* OPT_size */
		0,		/* TIDU_size - fill at run time */
		T_COTS,		/* SERV_type */
		-1,		/* CURRENT_state */
		TL_COTS_PROVIDER_FLAG	/* PROVIDER_flag */
	};

static struct T_info_ack tl_clts_info_ack =
	{
		T_INFO_ACK,	/* PRIM_type - always T_INFO_ACK */
		0,		/* TSDU_size - fill at run time */
		-2,		/* ETSDU_size -2 => not supported */
		-2,		/* CDATA_size -2 => not supported */
		-2,		/* DDATA_size  -2 => not supported */
		-1,		/* ADDR_size -1 => unlimited */
		-1,		/* OPT_size */
		0,		/* TIDU_size - fill at run time */
		T_CLTS,		/* SERV_type */
		-1,		/* CURRENT_state */
		TL_CLTS_PROVIDER_FLAG /* PROVIDER_flag */
	};

/*
 * Linked list of open driver streams
 */
static tl_endpt_t *tl_olistp = NULL;

/*
 * Marker for round robin flow control back enabling
 * and a global mutex to protect it. The mutex protects 'tl_flow_rr_head' and
 * 'te_flowrq'.
 */
static tl_endpt_t *tl_flow_rr_head;
static kmutex_t	tl_flow_rr_lock;
static t_scalar_t seq_gen = TL_MINOR_START;


/*
 * private copy of devinfo pointer used in tl_info
 */
static dev_info_t *tl_dip;

/*
 * Debug and test variable ONLY. Turn off T_CONN_IND queueing
 * for sockets.
 */
static int tl_disable_early_connect = 0;

/*
 * INTER MODULE DEPENDENCIES
 * -------------------------
 *
 * This is for sharing common option management code which happens
 * to live in "ip" kernel module only. Needed to do AF_UNIX socket
 * options
 */
char _depends_on[] = "drv/ip";

/*
 * LOCAL FUNCTION PROTOTYPES
 * -------------------------
 */
static int tl_do_proto(queue_t *, mblk_t *);
static int tl_do_pcproto(queue_t *, mblk_t *);
static void tl_do_ioctl(queue_t *, mblk_t *);
static void tl_error_ack(queue_t *, mblk_t *, t_scalar_t, t_scalar_t,
	t_scalar_t);
static void tl_bind(queue_t *, mblk_t *);
static void tl_ok_ack(queue_t *, mblk_t  *mp, t_scalar_t);
static void tl_unbind(queue_t *, mblk_t *);
static void tl_optmgmt(queue_t *, mblk_t *);
static void tl_conn_req(queue_t *, mblk_t *);
static void tl_conn_res(queue_t *, mblk_t *);
static void tl_discon_req(queue_t *, mblk_t *);
static void tl_capability_req(queue_t *, mblk_t *);
static void tl_info_req(queue_t *, mblk_t *);
static void tl_addr_req(queue_t *, mblk_t *);
static void tl_connected_cots_addr_req(queue_t *, mblk_t *);
static int tl_data(queue_t *, mblk_t  *);
static int tl_exdata(queue_t *, mblk_t *);
static int tl_ordrel(queue_t *, mblk_t *);
static int tl_unitdata(queue_t *, mblk_t *);
static void tl_uderr(queue_t *, mblk_t *, t_scalar_t);
static tl_endpt_t *tl_addr_in_use(int32_t, tl_addr_t *);
static int tl_get_any_addr(int32_t, tl_addr_t *, tl_addr_t *);
static void tl_cl_backenable(queue_t *);
static int tl_co_unconnect(tl_endpt_t *);
static mblk_t *tl_reallocb(mblk_t *, ssize_t);
static mblk_t *tl_resizemp(mblk_t *, ssize_t);
static int tl_preallocate(mblk_t *, mblk_t **, mblk_t **, ssize_t);
static void tl_discon_ind(tl_endpt_t *, uint32_t);
static mblk_t *tl_discon_ind_alloc(uint32_t, t_scalar_t);
static mblk_t *tl_ordrel_ind_alloc(void);
static void tl_icon_queue(tl_endpt_t *, tl_icon_t *);
static tl_icon_t **tl_icon_find(tl_endpt_t *, t_scalar_t);
static int tl_icon_queuemsg(tl_endpt_t *, t_scalar_t, mblk_t *);
static int tl_icon_hasprim(tl_endpt_t *, t_scalar_t, t_scalar_t);
static void tl_icon_sendmsgs(tl_endpt_t *, mblk_t **);
static void tl_icon_freemsgs(mblk_t **);
static void tl_merror(queue_t *, mblk_t *, int);
static void tl_fill_option(u_char *, t_uscalar_t, t_uscalar_t, tl_credopt_t *);
static int tl_default_opt(queue_t *, t_uscalar_t, t_uscalar_t, tl_credopt_t *);
static int tl_get_opt(queue_t *, t_uscalar_t, t_uscalar_t, u_char *);
static int tl_set_opt(queue_t *, t_scalar_t, t_uscalar_t, t_uscalar_t,
		t_uscalar_t, u_char *, t_uscalar_t *, u_char *);
static void tl_memrecover(queue_t *, mblk_t *, size_t);


/*
 * Intialize option database object for TL
 */

optdb_obj_t tl_opt_obj = {
	tl_default_opt,		/* TL default value function pointer */
	tl_get_opt,		/* TL get function pointer */
	tl_set_opt,		/* TL set function pointer */
	B_TRUE,			/* TL is tpi provider */
	TL_OPT_ARR_CNT,		/* TL option database count of entries */
	tl_opt_arr,		/* TL option database */
	TL_VALID_LEVELS_CNT,	/* TL valid level count of entries */
	tl_valid_levels_arr	/* TL valid level array */
};


/*
 * LOCAL FUNCTIONS AND DRIVER ENTRY POINTS
 * ---------------------------------------
 */

/*
 * Loadable module routines
 */
int
_init(void)
{
	int	error;

	mutex_init(&tl_flow_rr_lock, NULL, MUTEX_DEFAULT, NULL);
	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&tl_flow_rr_lock);
	}
	return (error);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	mutex_destroy(&tl_flow_rr_lock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Driver Entry Points and Other routines
 */
static int
tl_identify(dev_info_t *devi)
{
	if (strcmp((char *)ddi_get_name(devi), "tl") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
tl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH) {
		/*
		 * DDI_ATTACH is only non-reserved
		 * value of cmd
		 */
		return (DDI_FAILURE);
	}

	tl_dip = devi;
	if (ddi_create_minor_node(devi, "ticots", S_IFCHR,
			TL_TICOTS, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	if (ddi_create_minor_node(devi, "ticotsord", S_IFCHR,
		TL_TICOTSORD, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	if (ddi_create_minor_node(devi, "ticlts", S_IFCHR,
		TL_TICLTS, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
tl_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	ASSERT(tl_olistp == NULL);
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
tl_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{

	int retcode;

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		if (tl_dip == NULL)
			retcode = DDI_FAILURE;
		else {
			*result = (void *)tl_dip;
			retcode = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		retcode = DDI_SUCCESS;
		break;

	default:
		retcode = DDI_FAILURE;
	}
	return (retcode);
}

/* ARGSUSED */
static int
tl_open(queue_t	*rq, dev_t *devp, int oflag, int sflag,	cred_t	*credp)
{
	minor_t minordev;
	tl_credopt_t	*credoptp;
	tl_endpt_t *tep;
	tl_endpt_t *elp, *prev_elp, **prev_nextpp;
	int rc;

	rc = 0;

	/*
	 * Driver is called directly. Both CLONEOPEN and MODOPEN
	 * are illegal
	 */
	if ((sflag == CLONEOPEN) || (sflag == MODOPEN))
		return (ENXIO);

	if (rq->q_ptr)
		goto done;

	tep = kmem_zalloc(sizeof (tl_endpt_t), KM_SLEEP);
	/*
	 * initialize the endpoint - already zeroed
	 */
	tep->te_state = TS_UNBND;

	/*
	 * fill credopt from cred
	 */
	credoptp = &tep->te_cred;
	credoptp->tc_ngroups = credp->cr_ngroups;
	credoptp->tc_uid = credp->cr_uid;
	credoptp->tc_gid = credp->cr_gid;
	credoptp->tc_ruid = credp->cr_ruid;
	credoptp->tc_rgid = credp->cr_rgid;
	credoptp->tc_suid = credp->cr_suid;
	credoptp->tc_sgid = credp->cr_sgid;

	/*
	 * mode of driver is determined by the minor number
	 * used to open it. It can be TL_TICOTS, TL_TICOTSORD
	 * or TL_TICLTS
	 */
	tep->te_mode = getminor(*devp);
	tep->te_servtype = tep->te_mode & TL_SERVTYPEMASK;

	/*
	 * Initialize buffer for addr cache if connectionless provider
	 */
	if (tep->te_servtype == TL_TICLTS) {
		tep->te_lastdest.ta_alen = -1;
		tep->te_lastdest.ta_abuf = kmem_zalloc(TL_CLTS_CACHADDR_MAX,
			KM_SLEEP);
	}

	/*
	 * Find any unused minor number >= TL_MINOR_START (a number greater
	 * than minor numbers this driver can be opened with)
	 */

	minordev = TL_MINOR_START;

	/*
	 * Following code for speed not readability!
	 * - Traverse the ordered list looking for a free minor
	 *   device number. The insert the new node with the new
	 *   minor device number into the list
	 */
	prev_nextpp = &tl_olistp;
	prev_elp = NULL;
	while ((elp = *prev_nextpp) != NULL) {
		if (minordev < elp->te_minor)
			break;
		minordev++;
		prev_nextpp = &elp->te_nextp;
		prev_elp = elp;
	}
	tep->te_minor = minordev;
	tep->te_nextp = *prev_nextpp;
	tep->te_prevp = prev_elp;
	if (elp)
		elp->te_prevp = tep;
	*prev_nextpp = tep;	/* this updates "tl_olistp" if first! */


	*devp = makedevice(getmajor(*devp), minordev);	/* clone the driver */
	if (seq_gen == MAX_INT)
		seq_gen = TL_MINOR_START;
	tep->te_seqno = seq_gen++;
	tep->te_rq = rq;
	rq->q_ptr = WR(rq)->q_ptr = (char *)tep;

#ifdef	_ILP32
	tep->te_acceptor_id = (t_uscalar_t)rq;
#else
	tep->te_acceptor_id = (t_uscalar_t)getminor(*devp);
#endif	/* _ILP32 */
done:
	qprocson(rq);
	return (rc);
}

/* ARGSUSED1 */
static int
tl_close(queue_t *rq, int flag,	cred_t *credp)
{
	tl_endpt_t *tep, *elp;
	queue_t *wq;
	int freetep = 1;

	tep = (tl_endpt_t *)rq->q_ptr;

	ASSERT(tep);

	wq = WR(rq);

	/*
	 * Drain out all messages on queue except for TL_TICOTS where the
	 * abortive release semantics permit discarding of data on close
	 */
	tep->te_flag |= TL_CLOSING;
	if (tep->te_servtype != TL_TICOTS) {
		while (wq->q_first != NULL) {
			tl_wsrv(wq);
			/*
			 * tl_wsrv returning without draining (or discarding
			 * in tl_merror). This can only happen when we are
			 * recovering from allocation failures and have a
			 * bufcall or timeout outstanding on the queue
			 */
			if (wq->q_first != NULL) {
				ASSERT(tep->te_bufcid || tep->te_timoutid);
				qwait(wq);
			}
		}
	}

	wq->q_next = NULL;

	qprocsoff(rq);

	if (tep->te_bufcid)
		qunbufcall(rq, tep->te_bufcid);
	if (tep->te_timoutid)
		(void) quntimeout(rq, tep->te_timoutid);

	rq->q_ptr = wq->q_ptr = NULL;

	if ((tep->te_servtype == TL_TICOTS) ||
	    (tep->te_servtype == TL_TICOTSORD)) {
		/*
		 * connection oriented - unconnect endpoints
		 */
		freetep = tl_co_unconnect(tep);
		if (freetep) {
			/* For debugging purposes clear the state */
			tep->te_state = -1;	/* uninitialized */
		}
	} else {
		/*
		 * Do connectionless specific cleanup
		 */
		kmem_free(tep->te_lastdest.ta_abuf, TL_CLTS_CACHADDR_MAX);
		tep->te_lastdest.ta_alen = -1; /* uninitialized */
		tep->te_lastdest.ta_abuf = NULL;
		/*
		 * traverse list and delete cached references to this
		 * endpoint being closed
		 */
		elp = tl_olistp;
		while (elp) {
			if (elp->te_lastep == tep) {
				elp->te_lastep = NULL;
				elp->te_lastdest.ta_alen = -1;
			}
			elp = elp->te_nextp;
		}
		/*
		 * Backenable anybody that is flow controlled waiting for
		 * this endpoint.
		 */
		tep->te_state = -1;	/* uninitialized */
		tl_cl_backenable(rq);
	}

	ASSERT(tep->te_iconp == NULL);
	if (tep->te_ap.ta_abuf) {
		(void) kmem_free(tep->te_ap.ta_abuf, tep->te_ap.ta_alen);
		tep->te_ap.ta_alen = -1; /* uninitialized */
		tep->te_ap.ta_abuf = NULL;
	}

	/*
	 * disconnect te structure from list of open endpoints
	 */
	if (tep->te_prevp != NULL)
		tep->te_prevp->te_nextp = tep->te_nextp;
	if (tep->te_nextp != NULL)
		tep->te_nextp->te_prevp = tep->te_prevp;
	if (tep == tl_olistp) {
		tl_olistp = tl_olistp->te_nextp;
	}

	/*
	 * Advance tl_flow_rr_head pointer.
	 * Note: Advancing to tail of list (NULL) is fine and handled correctly
	 * in tl_rsrv.
	 * mutex_enter(&tl_flow_rr_lock); needed if no outer perimeter
	 */
	if (tl_flow_rr_head == tep)
		tl_flow_rr_head = tep->te_nextp;
	/*
	 * mutex_exit(&tl_flow_rr_lock); needed if no outer perimeter
	 */
	if (freetep)
		kmem_free(tep, sizeof (tl_endpt_t));
	return (0);
}



static void
tl_wput(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep, *peer_tep;
	ssize_t			msz;
	queue_t			*peer_rq;
	union T_primitives	*prim;
	t_scalar_t		prim_type;

	tep = (tl_endpt_t *)wq->q_ptr;
	msz = MBLKL(mp);

	/*
	 * fastpath for data
	 */
	if (tep->te_servtype != TL_TICLTS) { /* connection oriented */
		peer_tep = tep->te_conp;
		if ((DB_TYPE(mp) == M_DATA) &&
			(tep->te_state == TS_DATA_XFER) &&
			(!wq->q_first) &&
			(peer_tep) &&
			(peer_tep->te_state == TS_DATA_XFER) &&
			(peer_rq = peer_tep->te_rq) &&
			(canputnext(peer_rq))) {
				putnext(peer_rq, mp);
				return;
			}
	} else {		/* connectionless */
		if ((DB_TYPE(mp) == M_PROTO) &&
			(tep->te_state == TS_IDLE) &&
			(msz >= sizeof (struct T_unitdata_req)) &&
			(*(t_scalar_t *)mp->b_rptr == T_UNITDATA_REQ) &&
			(! wq->q_first)) {
			/* send T_UNITDATA_IND to linked endpoint */
			(void) tl_unitdata(wq, mp);
			return;
		}
	}

	switch (DB_TYPE(mp)) {

	case M_DATA:	/* slowpath */
		if (tep->te_servtype == TL_TICLTS) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_wput:M_DATA invalid for ticlts driver"));
			tl_merror(wq, mp, EPROTO);
			return;
		}
		(void) putq(wq, mp);
		break;

	case M_IOCTL:

		tl_do_ioctl(wq, mp);
		break;

	case M_FLUSH:

		/*
		 * do canonical M_FLUSH processing
		 */

		if (*mp->b_rptr & FLUSHW) {
			flushq(wq, FLUSHALL);
			*mp->b_rptr &= ~FLUSHW;
		}
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(wq), FLUSHALL);
			qreply(wq, mp);
		} else
			freemsg(mp);
		break;

	case M_PROTO:
		prim = (union T_primitives *)mp->b_rptr;
		prim_type = prim->type;

		/*
		 * Process TPI option management requests immediately
		 * in put procedure regardless of in-order processing
		 * of already queued messages.
		 * (Note: This driver supports AF_UNIX socket implementation.
		 * Unless we implement this processing, setsockopt() on
		 * socket endpoint will block on flow controlled endpoints
		 * which it should not. That is required for successful
		 * execution of VSU socket tests and is consistent
		 * with BSD socket behavior).
		 */
		if ((msz >= sizeof (prim_type)) &&
		    TL_OPTMGMT_REQ_PRIM(prim_type)) {
			(void) tl_do_proto(wq, mp);
			break;
		}
		/*
		 * process in service procedure if
		 * message already queued (maintain in-order processing) or
		 * short message error or
		 * messages one of certain types
		 */
		if ((wq->q_first) ||
			(msz < sizeof (prim_type)) ||
			(prim_type == T_DATA_REQ) ||
			(prim_type == T_OPTDATA_REQ) ||
			(prim_type == T_EXDATA_REQ) ||
			(prim_type == T_ORDREL_REQ) ||
			(prim_type == T_UNITDATA_REQ)) {
			(void) putq(wq, mp);
			return;
		}
		(void) tl_do_proto(wq, mp);
		break;

	case M_PCPROTO:
		if (wq->q_first) {
			(void) putq(wq, mp);
			return;
		}
		(void) tl_do_pcproto(wq, mp);
		break;

	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:default:unexpected Streams message"));
		freemsg(mp);
		break;
	}
}



static void
tl_wsrv(queue_t *wq)
{
	mblk_t *mp;
	tl_endpt_t *tep;

	tep = (tl_endpt_t *)wq->q_ptr;

	/*
	 * clear flow control pointer
	 * (in case we were enabled after flow control)
	 */
	tep->te_flowrq = NULL;


	while (mp = getq(wq)) {
		switch (DB_TYPE(mp)) {

		case M_DATA:
			if (tl_data(wq, mp) == -1)
				return;
			break;

		case M_PROTO:
			if (tl_do_proto(wq, mp) == -1)
				return;
			break;
		case M_PCPROTO:
			if (tl_do_pcproto(wq, mp) == -1)
				return;
			break;
		default:
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
			    SL_TRACE|SL_ERROR,
			    "tl_wsrv:unexpected message type"));
			freemsg(mp);
			break;
		}
	}
}

static void
tl_rsrv(queue_t *rq)
{
	tl_endpt_t *tep, *peer_tep;

	tep = (tl_endpt_t *)rq->q_ptr;
	ASSERT(tep != NULL);

	/*
	 * enable queue for data transfer
	 */

	if ((tep->te_servtype != TL_TICLTS) &&
		((tep->te_state == TS_DATA_XFER) ||
		(tep->te_state == TS_WIND_ORDREL)||
		(tep->te_state == TS_WREQ_ORDREL))) {
		/*
		 * If unconnect was done (due to a close) the peer might be
		 * gone.
		 */
		if ((peer_tep = tep->te_conp) != NULL)
			(void) qenable(WR(peer_tep->te_rq));
	} else {
		if (tep->te_state == TS_IDLE)
			tl_cl_backenable(rq);
	}
}



static int
tl_do_proto(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	ssize_t			msz;
	int			rc;
	union T_primitives	*prim;

	tep = (tl_endpt_t *)wq->q_ptr;
	msz = MBLKL(mp);
	rc = 0;

	prim = (union T_primitives *)mp->b_rptr;
	if (msz < sizeof (prim->type)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:M_PROTO: bad message"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}

	switch (prim->type) {

	case O_T_BIND_REQ:
	case T_BIND_REQ:
		qwriter(wq, mp, tl_bind, PERIM_OUTER);
		break;

	case T_UNBIND_REQ:
		qwriter(wq, mp, tl_unbind, PERIM_OUTER);
		break;

	case T_CAPABILITY_REQ:
		tl_capability_req(wq, mp);
		break;

	case T_SVR4_OPTMGMT_REQ:
	case T_OPTMGMT_REQ:
		tl_optmgmt(wq, mp);
		break;

	case T_ADDR_REQ:
		tl_addr_req(wq, mp);
		break;

	case T_CONN_REQ:
		ASSERT(tep->te_servtype != TL_TICLTS);
		qwriter(wq, mp, tl_conn_req, PERIM_OUTER);
		break;

	case O_T_CONN_RES:
	case T_CONN_RES:
		ASSERT(tep->te_servtype != TL_TICLTS);
		qwriter(wq, mp, tl_conn_res, PERIM_OUTER);
		break;

	case T_DISCON_REQ:
		ASSERT(tep->te_servtype != TL_TICLTS);
		qwriter(wq, mp, tl_discon_req, PERIM_OUTER);
		break;

	case T_DATA_REQ:
		ASSERT(tep->te_servtype != TL_TICLTS);
		rc = tl_data(wq, mp);
		break;

	case T_OPTDATA_REQ:
		ASSERT(tep->te_servtype != TL_TICLTS);
		rc = tl_data(wq, mp);
		break;

	case T_EXDATA_REQ:
		ASSERT(tep->te_servtype != TL_TICLTS);
		rc = tl_exdata(wq, mp);
		break;

	case T_ORDREL_REQ:
		ASSERT(tep->te_servtype == TL_TICOTSORD);
		rc = tl_ordrel(wq, mp);
		break;

	case T_UNITDATA_REQ:
		ASSERT(tep->te_servtype == TL_TICLTS);
		rc = tl_unitdata(wq, mp);
		break;

	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:default:unknown TPI msg primitive"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	return (rc);
}


static int
tl_do_pcproto(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	ssize_t			msz;
	union T_primitives	*prim;

	tep = (tl_endpt_t *)wq->q_ptr;

	msz = MBLKL(mp);

	prim = (union T_primitives *)mp->b_rptr;
	if (msz < sizeof (prim->type)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:M_PCPROTO: bad message"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	switch (prim->type) {
	case T_CAPABILITY_REQ:
		tl_capability_req(wq, mp);
		break;

	case T_INFO_REQ:
		tl_info_req(wq, mp);
		break;

	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:default:unknown TPI msg primitive"));
		tl_merror(wq, mp, EPROTO);
		break;
	}
	return (0);
}


static void
tl_do_ioctl(queue_t *wq, mblk_t *mp)
{
	struct iocblk *iocbp;
	tl_endpt_t *tep;

	iocbp = (struct iocblk *)mp->b_rptr;
	tep = (tl_endpt_t *)wq->q_ptr;

	switch (iocbp->ioc_cmd) {
	case TL_IOC_CREDOPT:
		/*
		 * Turn on generation of credential options for
		 * T_conn_req, T_conn_con, T_unidata_ind.
		 */
		if (iocbp->ioc_count != sizeof (uint32_t) ||
			mp->b_cont == NULL ||
			((uintptr_t)mp->b_cont->b_rptr &
				(sizeof (uint32_t) - 1)))
			goto iocnak;

		/* The credentials passing does not apply to sockets */
		if (tep->te_mode & TL_SOCKET)
			goto iocnak;

		if (*(uint32_t *)mp->b_cont->b_rptr)
			tep->te_flag |= TL_SETCRED;
		else
			tep->te_flag &= ~TL_SETCRED;
		miocack(wq, mp, 0, 0);
		break;

	case TL_IOC_SOCKET:
		/*
		 * Switch the endpoint to/from socket semantics.
		 */
		if (iocbp->ioc_count != sizeof (uint32_t) ||
			mp->b_cont == (mblk_t *)NULL ||
			((uintptr_t)mp->b_cont->b_rptr &
				(sizeof (uint32_t) - 1)))
			goto iocnak;

		/*
		 * Can't switch to a socket after enabling credentials
		 * passing
		 */
		if (tep->te_flag & TL_SETCRED)
			goto iocnak;

		if (*(uint32_t *)mp->b_cont->b_rptr)
			tep->te_mode |= TL_SOCKET;
		else
			tep->te_mode &= ~TL_SOCKET;

		miocack(wq, mp, 0, 0);
		break;
	default:
iocnak:
		miocnak(wq, mp, 0, EINVAL);
		break;
	}
}


/*
 * send T_ERROR_ACK
 * Note: assumes enough memory or caller passed big enough mp
 *	- no recovery from allocb failures
 */

static void
tl_error_ack(queue_t *wq, mblk_t *mp, t_scalar_t tli_err,
    t_scalar_t unix_err, t_scalar_t type)
{
	tl_endpt_t		*tep;
	size_t			ack_sz;
	struct T_error_ack	*err_ack;
	mblk_t			*ackmp;

	tep = (tl_endpt_t *)wq->q_ptr;
	ack_sz = sizeof (struct T_error_ack);

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	ackmp = tl_resizemp(mp, ack_sz);
	if (! ackmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_error_ack:out of mblk memory"));
		tl_merror(wq, mp, ENOMEM);
		return;
	}
	DB_TYPE(mp) = M_PCPROTO;
	err_ack = (struct T_error_ack *)ackmp->b_rptr;
	err_ack->PRIM_type = T_ERROR_ACK;
	err_ack->ERROR_prim = type;
	err_ack->TLI_error = tli_err;
	err_ack->UNIX_error = unix_err;

	/*
	 * send error ack message
	 */
	qreply(wq, ackmp);
}



/*
 * send T_OK_ACK
 * Note: assumes enough memory or caller passed big enough mp
 *	- no recovery from allocb failures
 */
static void
tl_ok_ack(queue_t *wq, mblk_t *mp, t_scalar_t type)
{
	tl_endpt_t *tep;
	mblk_t *ackmp;
	struct T_ok_ack *ok_ack;

	tep = (tl_endpt_t *)wq->q_ptr;

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	ackmp = tl_resizemp(mp, sizeof (struct T_ok_ack));
	if (! ackmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_ok_ack:allocb failure"));
		tl_merror(wq, mp, ENOMEM);
		return;
	}

	DB_TYPE(ackmp) = M_PCPROTO;
	ok_ack = (struct T_ok_ack *)ackmp->b_rptr;
	ok_ack->PRIM_type  = T_OK_ACK;
	ok_ack->CORRECT_prim = type;

	(void) qreply(wq, ackmp);
}



/*
 * qwriter call back function. Called with qwriter so
 * that concurrent bind operations to same transport address do not
 * succeed
 */
static void
tl_bind(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	struct T_bind_ack	*b_ack;
	struct T_bind_req	*bind;
	mblk_t			*ackmp, *bamp;
	t_uscalar_t		qlen;
	t_scalar_t		alen, aoff;
	tl_addr_t		addr_req;
	u_char			*addr_startp;
	ssize_t			msz, basize;
	t_scalar_t		tli_err, unix_err, save_prim_type, save_state;

	unix_err = 0;
	tep = (tl_endpt_t *)wq->q_ptr;
	bind = (struct T_bind_req *)mp->b_rptr;

	msz = MBLKL(mp);

	save_state = tep->te_state;
	save_prim_type = bind->PRIM_type; /* O_T_BIND_REQ or T_BIND_REQ */

	/*
	 * validate state
	 */
	if (tep->te_state != TS_UNBND) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:bind_request:out of state, state=%d",
			tep->te_state));
		tli_err = TOUTSTATE;
		goto error;
	}
	tep->te_state = NEXTSTATE(TE_BIND_REQ, tep->te_state);


	/*
	 * validate message
	 * Note: dereference fields in struct inside message only
	 * after validating the message length.
	 */
	if (msz < sizeof (struct T_bind_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:tl_bind: invalid message length"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tli_err = TSYSERR; unix_err = EINVAL;
		goto error;
	}
	alen = bind->ADDR_length;
	aoff = bind->ADDR_offset;
	if ((alen > 0) && ((aoff < 0) ||
	    ((ssize_t)(aoff + alen) > msz) || ((aoff + alen) < 0))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:tl_bind: invalid message"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tli_err = TSYSERR; unix_err = EINVAL;
		goto error;
	}
	if ((alen < 0) || (alen > (msz - sizeof (struct T_bind_req)))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:tl_bind: bad addr in  message"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tli_err = TBADADDR;
		goto error;
	}

#ifdef DEBUG
	/*
	 * Mild form of ASSERT()ion to detect broken TPI apps.
	 * if (! assertion)
	 *	log warning;
	 */
	if (! ((alen == 0 && aoff == 0) ||
		(aoff >= (t_scalar_t)(sizeof (struct T_bind_req))))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_wput:tl_bind: addr overlaps TPI message"));
	}
#endif
	/*
	 * negotiate max conn req pending
	 */
	if (tep->te_servtype != TL_TICLTS) { /* connection oriented */
		qlen = bind->CONIND_number;
		if (qlen > TL_MAXQLEN)
			qlen = TL_MAXQLEN;
	}

	if (alen == 0) {
		/*
		 * assign any free address
		 */
		if (! tl_get_any_addr(tep->te_mode, &tep->te_ap, NULL)) {
			(void) (STRLOG(TL_ID, tep->te_minor,
				1, SL_TRACE|SL_ERROR,
				"tl_bind:failed to get buffer for any "
				"address"));
			tli_err = TSYSERR; unix_err = ENOMEM;
			goto error;
		}
	} else {
		addr_req.ta_alen = alen;
		addr_req.ta_abuf = (char *)(mp->b_rptr + aoff);
		if (tl_addr_in_use(tep->te_mode, &addr_req) != NULL) {
			if (save_prim_type == T_BIND_REQ) {
				/*
				 * The bind semantics for this primitive
				 * require a failure if the exact address
				 * requested is busy
				 */
				(void) (STRLOG(TL_ID, tep->te_minor, 1,
					SL_TRACE|SL_ERROR,
					"tl_bind:requested addr is busy"));
				tli_err = TADDRBUSY; unix_err = 0;
				goto error;
			}
			/*
			 * O_T_BIND_REQ semantics say if address if requested
			 * address is busy, bind to any available free address
			 */
			if (! tl_get_any_addr(tep->te_mode, &tep->te_ap,
						&addr_req)) {
				(void) (STRLOG(TL_ID, tep->te_minor, 1,
					SL_TRACE|SL_ERROR,
					"tl_bind:unable to get any addr buf"));
				tli_err = TSYSERR; unix_err = ENOMEM;
				goto error;
			}
		} else {
			/*
			 * requested address is free - copy it to new
			 * buffer and return
			 */
			tep->te_ap.ta_abuf = (char *)
				kmem_alloc(addr_req.ta_alen, KM_NOSLEEP);
			if (tep->te_ap.ta_abuf == NULL) {
				(void) (STRLOG(TL_ID, tep->te_minor, 1,
					SL_TRACE|SL_ERROR,
					"tl_bind:unable to get any addr buf"));
				tli_err = TSYSERR; unix_err = ENOMEM;
				goto error;
			}
			tep->te_ap.ta_alen = addr_req.ta_alen;
			bcopy(addr_req.ta_abuf, tep->te_ap.ta_abuf,
				addr_req.ta_alen);
		}
	}

	/*
	 * prepare T_BIND_ACK TPI message
	 */
	basize = sizeof (struct T_bind_ack) + tep->te_ap.ta_alen;
	bamp = tl_reallocb(mp, basize);
	if (! bamp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_wput:tl_bind: allocb failed"));
		(void) kmem_free(tep->te_ap.ta_abuf, tep->te_ap.ta_alen);
		tep->te_ap.ta_alen = -1;
		tep->te_ap.ta_abuf = NULL;
		/*
		 * roll back state changes
		 */
		tep->te_state = TS_UNBND;
		tl_memrecover(wq, mp, basize);
		return;
	}

	DB_TYPE(bamp) = M_PCPROTO;
	b_ack = (struct T_bind_ack *)bamp->b_rptr;
	b_ack->PRIM_type = T_BIND_ACK;
	b_ack->CONIND_number = qlen;
	b_ack->ADDR_length = tep->te_ap.ta_alen;
	b_ack->ADDR_offset = (t_scalar_t)sizeof (struct T_bind_ack);
	addr_startp = bamp->b_rptr + b_ack->ADDR_offset;
	bcopy(tep->te_ap.ta_abuf, addr_startp, tep->te_ap.ta_alen);

	tep->te_qlen = qlen;

	tep->te_state = NEXTSTATE(TE_BIND_ACK, tep->te_state);
	/*
	 * send T_BIND_ACK message
	 */
	(void) qreply(wq, bamp);
	return;

error:
	ackmp = tl_reallocb(mp, sizeof (struct T_error_ack));
	if (! ackmp) {
		/*
		 * roll back state changes
		 */
		tep->te_state = save_state;
		tl_memrecover(wq, mp, sizeof (struct T_error_ack));
		return;
	}
	tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
	tl_error_ack(wq, ackmp, tli_err, unix_err, save_prim_type);
}


static void
tl_unbind(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t *tep;
	mblk_t *ackmp;
	mblk_t *freemp;

	tep = (tl_endpt_t *)wq->q_ptr;
	/*
	 * preallocate memory for max of T_OK_ACK and T_ERROR_ACK
	 * ==> allocate for T_ERROR_ACK (known max)
	 */
	if (! tl_preallocate(mp, &ackmp, &freemp,
				sizeof (struct T_error_ack))) {
			tl_memrecover(wq, mp, sizeof (struct T_error_ack));
			return;
	}
	/*
	 * memory resources committed
	 * Note: no message validation. T_UNBIND_REQ message is
	 * same size as PRIM_type field so already verified earlier.
	 */

	/*
	 * validate state
	 */
	if (tep->te_state != TS_IDLE) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_UNBIND_REQ:out of state, state=%d",
			tep->te_state));
		tl_error_ack(wq, ackmp, TOUTSTATE, 0, T_UNBIND_REQ);
		freemsg(freemp);
		return;
	}
	tep->te_state = NEXTSTATE(TE_UNBIND_REQ, tep->te_state);

	/*
	 * TPI says on T_UNBIND_REQ:
	 *    send up a M_FLUSH to flush both
	 *    read and write queues
	 */
	(void) putnextctl1(RD(wq), M_FLUSH, FLUSHRW);

	tep->te_qlen = 0;
	(void) kmem_free(tep->te_ap.ta_abuf, tep->te_ap.ta_alen);

	tep->te_ap.ta_alen = -1; /* uninitialized */
	tep->te_ap.ta_abuf = NULL;

	tep->te_state = NEXTSTATE(TE_OK_ACK1, tep->te_state);

	/*
	 * send  T_OK_ACK
	 */
	tl_ok_ack(wq, ackmp, T_UNBIND_REQ);

	freemsg(freemp);
}


/*
 * Option management code from drv/ip is used here
 * Note: TL_PROT_LEVEL/TL_IOC_CREDOPT option is not part of tl_opt_arr
 *	database of options. So optcom_req() will fail T_SVR4_OPTMGMT_REQ.
 *	However, that is what we want as that option is 'unorthodox'
 *	and only valid in T_CONN_IND, T_CONN_CON  and T_UNITDATA_IND
 *	and not in T_SVR4_OPTMGMT_REQ/ACK
 * Note2: use of optcom_req means this routine is an exception to
 *	 recovery from allocb() failures.
 */

static void
tl_optmgmt(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t *tep;
	mblk_t *ackmp;
	union T_primitives *prim;

	tep = (tl_endpt_t *)wq->q_ptr;
	prim = (union T_primitives *)mp->b_rptr;

	/*  all states OK for AF_UNIX options ? */
	if (!(tep->te_mode & TL_SOCKET) && tep->te_state != TS_IDLE &&
	    prim->type == T_SVR4_OPTMGMT_REQ) {
		/*
		 * Broken TLI semantics that options can only be managed
		 * in TS_IDLE state. Needed for Sparc ABI test suite that
		 * tests this TLI (mis)feature using this device driver.
		 */
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_SVR4_OPTMGMT_REQ:out of state, state=%d",
			tep->te_state));
		/*
		 * preallocate memory for T_ERROR_ACK
		 */
		ackmp = allocb(sizeof (struct T_error_ack), BPRI_MED);
		if (! ackmp) {
			tl_memrecover(wq, mp, sizeof (struct T_error_ack));
			return;
		}

		tl_error_ack(wq, ackmp, TOUTSTATE, 0, T_SVR4_OPTMGMT_REQ);
		freemsg(mp);
		return;
	}

	/*
	 * call common option management routine from drv/ip
	 */
	if (prim->type == T_SVR4_OPTMGMT_REQ) {
		svr4_optcom_req(wq, mp,	(int)(tep->te_cred.tc_uid == 0),
		    &tl_opt_obj);
	} else {
		ASSERT(prim->type == T_OPTMGMT_REQ);
		tpi_optcom_req(wq, mp, (int)(tep->te_cred.tc_uid == 0),
		    &tl_opt_obj);
	}
}


/*
 * Handle T_conn_req.
 * If TL_SETCRED generate the credentials options.
 * If this is a socket pass through options unmodified.
 * For sockets generate the T_CONN_CON here instead of
 * waiting for the T_CONN_RES.
 */
static void
tl_conn_req(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	struct T_conn_req	*creq;
	ssize_t			msz;
	t_scalar_t		alen, aoff, olen, ooff,	err;
	tl_endpt_t		*peer_tep;
	u_char			*addr_startp;
	size_t			ci_msz, size;
	mblk_t			*indmp, *freemp, *ackmp, *confmp;
	mblk_t			*mp1;
	mblk_t			*dimp, *cimp;
	struct T_discon_ind	*di;
	struct T_conn_ind	*ci;
	tl_addr_t		dst;
	tl_icon_t		*tip;

	/*
	 * preallocate memory for:
	 * 1. max of T_ERROR_ACK and T_OK_ACK
	 *	==> known max T_ERROR_ACK
	 * 2. max of T_DISCON_IND and T_CONN_IND
	 */
	ackmp = allocb(sizeof (struct T_error_ack), BPRI_MED);
	if (! ackmp) {
		tl_memrecover(wq, mp, sizeof (struct T_error_ack));
		return;
	}
	/*
	 * memory committed for T_OK_ACK/T_ERROR_ACK now
	 * will be committed for T_DISCON_IND/T_CONN_IND later
	 */


	tep = (tl_endpt_t *)wq->q_ptr;
	creq = (struct T_conn_req *)mp->b_rptr;
	msz = MBLKL(mp);

	if (tep->te_state != TS_IDLE) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_CONN_REQ:out of state, state=%d",
			tep->te_state));
		tl_error_ack(wq, ackmp, TOUTSTATE, 0, T_CONN_REQ);
		freemsg(mp);
		return;
	}

	/*
	 * validate the message
	 * Note: dereference fields in struct inside message only
	 * after validating the message length.
	 */
	if (msz < sizeof (struct T_conn_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_req:invalid message length"));
		tl_error_ack(wq, ackmp, TSYSERR, EINVAL, T_CONN_REQ);
		freemsg(mp);
		return;
	}
	alen = creq->DEST_length;
	aoff = creq->DEST_offset;
	olen = creq->OPT_length;
	ooff = creq->OPT_offset;
	if (olen == 0)
		ooff = 0;

	if ((alen > 0 && ((ssize_t)(aoff + alen) > msz || aoff + alen < 0)) ||
	    (olen > 0 && ((ssize_t)(ooff + olen) > msz || ooff + olen < 0)) ||
	    olen < 0 || ooff < 0) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_req:invalid message"));
		tl_error_ack(wq, ackmp, TSYSERR, EINVAL, T_CONN_REQ);
		freemsg(mp);
		return;
	}
	if (alen <= 0 || aoff < 0 ||
	    (ssize_t)alen > msz - sizeof (struct T_conn_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_req:bad addr in message, alen=%d, msz=%ld",
			alen, msz));
		tl_error_ack(wq, ackmp, TBADADDR, 0, T_CONN_REQ);
		freemsg(mp);
		return;
	}
#ifdef DEBUG
	/*
	 * Mild form of ASSERT()ion to detect broken TPI apps.
	 * if (! assertion)
	 *	log warning;
	 */
	if (! (aoff >= (t_scalar_t)sizeof (struct T_conn_req))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_conn_req: addr overlaps TPI message"));
	}
#endif
	if (olen && !(tep->te_mode & TL_SOCKET)) {
		/*
		 * no opts in connect req
		 * supported in this provider except for sockets.
		 */
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_req:options not supported in message"));
		tl_error_ack(wq, ackmp, TBADOPT, 0, T_CONN_REQ);
		freemsg(mp);
		return;
	}

	tep->te_state = NEXTSTATE(TE_CONN_REQ, tep->te_state);
	/*
	 * start business of connecting
	 * send T_OK_ACK only after memory committed
	 */

	err = 0;
	/*
	 * get endpoint to connect to
	 * check that peer with DEST addr is bound to addr
	 * and has CONIND_number > 0
	 */
	dst.ta_alen = alen;
	dst.ta_abuf = (char *)(mp->b_rptr + aoff);

	/*
	 * Verify if remote addr is in use
	 */
	if ((peer_tep = tl_addr_in_use(tep->te_mode, &dst)) == NULL) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_req:no one at connect address"));
		err = ECONNREFUSED;
	} else if (peer_tep->te_nicon >= peer_tep->te_qlen)  {
		/*
		 * validate that number of incoming connection is
		 * not to capacity on destination endpoint
		 */
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE,
			"tl_conn_req: qlen overflow connection refused"));
			err = ECONNREFUSED;
	} else if (!((peer_tep->te_state == TS_IDLE) ||
			(peer_tep->te_state == TS_WRES_CIND))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE,
			"tl_conn_req:peer in bad state"));
		err = ECONNREFUSED;
	}

	/*
	 * preallocate now for T_DISCON_IND or T_CONN_IND
	 */
	if (err)
		size = sizeof (struct T_discon_ind);
	else {
		/*
		 * calculate length of T_CONN_IND message
		 */
		if (peer_tep->te_flag & TL_SETCRED) {
			ooff = 0;
			olen = (t_scalar_t) sizeof (struct opthdr) +
				OPTLEN(sizeof (tl_credopt_t));
			/* 1 option only */
		}
		ci_msz = sizeof (struct T_conn_ind) + tep->te_ap.ta_alen;
		ci_msz = T_ALIGN(ci_msz) + olen;
		size = max(ci_msz, sizeof (struct T_discon_ind));
	}
	/*
	 * Make a copy of the t_conn_req so that we can reference
	 * the options later.
	 */
	mp1 = copyb(mp);
	if (mp1 == NULL) {
		/*
		 * roll back state changes
		 */
		tep->te_state = TS_IDLE;
		tl_memrecover(wq, mp, size);
		freemsg(ackmp);
		return;
	}

	if ((tep->te_mode & TL_SOCKET) && !tl_disable_early_connect) {
		/*
		 * Generate a T_CONN_CON that has the identical address
		 * (and options) as the T_CONN_REQ.
		 * NOTE: assumes that the T_conn_req and T_conn_con structures
		 * are isomorphic.
		 */
		confmp = copyb(mp);
		if (! confmp) {
			/*
			 * roll back state changes
			 */
			tep->te_state = TS_IDLE;
			tl_memrecover(wq, mp, mp->b_wptr - mp->b_rptr);
			freemsg(ackmp);
			freemsg(mp1);
			return;
		}
		((struct T_conn_con *)(confmp->b_rptr))->PRIM_type =
			T_CONN_CON;
	} else {
		confmp = NULL;
	}
	if (! tl_preallocate(mp, &indmp, &freemp, size)) {
		/*
		 * roll back state changes
		 */
		tep->te_state = TS_IDLE;
		tl_memrecover(wq, mp, size);
		freemsg(ackmp);
		freemsg(mp1);
		freemsg(confmp);
		return;
	}

	tip = kmem_alloc(sizeof (*tip), KM_NOSLEEP);
	if (tip == NULL) {
		/*
		 * roll back state changes
		 */
		tep->te_state = TS_IDLE;
		tl_memrecover(wq, mp, sizeof (*tip));
		freemsg(ackmp);
		freemsg(mp1);
		freemsg(confmp);
		freemsg(indmp);
		freemsg(freemp);
		return;
	}
	tip->ti_mp = NULL;
	tip->ti_freetep = NULL;

	/*
	 * memory is now committed for T_DISCON_IND/T_CONN_IND/T_CONN_CON
	 * and tl_icon_t cell.
	 */

	/*
	 * ack validity of request
	 */
	tep->te_state = NEXTSTATE(TE_OK_ACK1, tep->te_state);
	tl_ok_ack(wq, ackmp, T_CONN_REQ);

	/* if validation failed earlier - now send T_DISCON_IND */
	if (err) {
		/*
		 *  prepare T_DISCON_IND message
		 */
		dimp = tl_resizemp(indmp, size);
		if (! dimp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_conn_req:discon_ind:allocb failure"));
			tl_merror(wq, indmp, ENOMEM);
			freemsg(freemp);
			freemsg(mp1);
			freemsg(confmp);
			ASSERT(tip->ti_mp == NULL);
			ASSERT(tip->ti_freetep == NULL);
			kmem_free(tip, sizeof (*tip));
			return;
		}
		if (dimp->b_cont) {
			/* no user data in provider generated discon ind */
			freemsg(dimp->b_cont);
			dimp->b_cont = NULL;
		}

		DB_TYPE(dimp) = M_PROTO;
		di = (struct T_discon_ind *)dimp->b_rptr;
		di->PRIM_type  = T_DISCON_IND;
		di->DISCON_reason = err;
		di->SEQ_number = BADSEQNUM;

		tep->te_state = NEXTSTATE(TE_DISCON_IND1, tep->te_state);
		/*
		 * send T_DISCON_IND message
		 */
		(void) qreply(wq, dimp);
		freemsg(freemp);
		freemsg(mp1);
		freemsg(confmp);
		ASSERT(tip->ti_mp == NULL);
		ASSERT(tip->ti_freetep == NULL);
		kmem_free(tip, sizeof (*tip));
		return;
	}

	/*
	 * prepare message to send T_CONN_IND
	 */


	/*
	 * allocate the message - original data blocks retained
	 * in the returned mblk
	 */
	cimp = tl_resizemp(indmp, size);
	if (! cimp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_conn_req:con_ind:allocb failure"));
		tl_merror(wq, indmp, ENOMEM);
		freemsg(freemp);
		freemsg(mp1);
		freemsg(confmp);
		ASSERT(tip->ti_mp == NULL);
		ASSERT(tip->ti_freetep == NULL);
		kmem_free(tip, sizeof (*tip));
		return;
	}

	DB_TYPE(cimp) = M_PROTO;
	ci = (struct T_conn_ind *)cimp->b_rptr;
	ci->PRIM_type  = T_CONN_IND;
	ci->SRC_offset = (t_scalar_t)sizeof (struct T_conn_ind);
	ci->SRC_length = tep->te_ap.ta_alen;
	ci->SEQ_number = tep->te_seqno;

	addr_startp = indmp->b_rptr + ci->SRC_offset;
	bcopy(tep->te_ap.ta_abuf, addr_startp, tep->te_ap.ta_alen);
	if (peer_tep->te_flag & TL_SETCRED) {
		ci->OPT_offset = (t_scalar_t)T_ALIGN(ci->SRC_offset +
					ci->SRC_length);
		ci->OPT_length = olen; /* because only 1 option */
		tl_fill_option(indmp->b_rptr + ci->OPT_offset,
				(t_uscalar_t)sizeof (tl_credopt_t),
				TL_OPT_PEER_CRED,
				&tep->te_cred);
	} else if (ooff != 0) {
		/* Copy option from T_CONN_REQ */
		ci->OPT_offset = (t_scalar_t)T_ALIGN(ci->SRC_offset +
					ci->SRC_length);
		ci->OPT_length = olen;
		bcopy(mp1->b_rptr + ooff,
			(void *)((uintptr_t)ci + ci->OPT_offset), olen);
	} else {
		ci->OPT_offset = 0;
		ci->OPT_length = 0;
	}
	freemsg(mp1);

	/*
	 * register connection request with server peer
	 * append to list of incoming connections
	 */
	tip->ti_tep = tep;
	tip->ti_seqno = tep->te_seqno;

	tl_icon_queue(peer_tep, tip);

	tep->te_oconp = peer_tep; /* can this be in te_conp ? */

	peer_tep->te_state = NEXTSTATE(TE_CONN_IND, peer_tep->te_state);
	/*
	 * send the T_CONN_IND message
	 */
	putnext(peer_tep->te_rq, cimp);

	/*
	 * Send a T_CONN_CON message for sockets.
	 * Disable the queues until we have reached the correct state!
	 */
	if (confmp != NULL) {
		tep->te_state = NEXTSTATE(TE_CONN_CON, tep->te_state);
		noenable(wq);
		qreply(wq, confmp);
	}
	freemsg(freemp);
}



/*
 * Handle T_conn_res.
 * If TL_SETCRED generate the credentials options.
 * For sockets tl_conn_req has already generated the T_CONN_CON.
 */
static void
tl_conn_res(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep, *sep;
	struct T_conn_res	*cres;
	ssize_t			msz;
	t_scalar_t		olen, ooff, err;
	u_char			*addr_startp;
	tl_endpt_t 		*acc_ep, *cl_ep;
	tl_icon_t		**tipp, *tip;
	size_t			size;
	mblk_t			*ackmp, *respmp, *freemp;
	mblk_t			*dimp, *ccmp;
	struct T_discon_ind	*di;
	struct T_conn_con	*cc;

	/*
	 * preallocate memory for:
	 * 1. max of T_ERROR_ACK and T_OK_ACK
	 *	==> known max T_ERROR_ACK
	 * 2. max of T_DISCON_IND and T_CONN_CON
	 */
	ackmp = allocb(sizeof (struct T_error_ack), BPRI_MED);
	if (! ackmp) {
		tl_memrecover(wq, mp, sizeof (struct T_error_ack));
		return;
	}
	/*
	 * memory committed for T_OK_ACK/T_ERROR_ACK now
	 * will be committed for T_DISCON_IND/T_CONN_CON later
	 */


	tep = (tl_endpt_t *)wq->q_ptr;
	cres = (struct T_conn_res *)mp->b_rptr;
	ASSERT(cres->PRIM_type == T_CONN_RES ||
	    cres->PRIM_type == O_T_CONN_RES);
	msz = MBLKL(mp);

	/*
	 * validate state
	 */
	if (tep->te_state != TS_WRES_CIND) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_CONN_RES:out of state, state=%d",
			tep->te_state));
		tl_error_ack(wq, ackmp, TOUTSTATE, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * validate the message
	 * Note: dereference fields in struct inside message only
	 * after validating the message length.
	 */
	if (msz < sizeof (struct T_conn_res)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_res:invalid message length"));
		tl_error_ack(wq, ackmp, TSYSERR, EINVAL, cres->PRIM_type);
		freemsg(mp);
		return;
	}
	olen = cres->OPT_length;
	ooff = cres->OPT_offset;
	if (((olen > 0) && ((ooff + olen) > msz))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_res:invalid message"));
		tl_error_ack(wq, ackmp, TSYSERR, EINVAL, cres->PRIM_type);
		freemsg(mp);
		return;
	}
	if (olen) {
		/*
		 * no opts in connect res
		 * supported in this provider
		 */
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_conn_res:options not supported in message"));
		tl_error_ack(wq, ackmp, TBADOPT, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	tep->te_state = NEXTSTATE(TE_CONN_RES, tep->te_state);
	/*
	 * find accepting endpoint
	 */
	sep = tl_olistp;

	while (sep) {
		if (sep->te_acceptor_id == cres->ACCEPTOR_id)
			break;
		sep = sep->te_nextp;
	}
	acc_ep = sep;		/* NULL if not found! */

	if (cres->SEQ_number < TL_MINOR_START &&
		cres->SEQ_number >= BADSEQNUM) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE|SL_ERROR,
			"tl_conn_res:remote endpoint sequence number bad"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tl_error_ack(wq, ackmp, TBADSEQ, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * if endpoint does not exist - nack it
	 */
	if (acc_ep == NULL) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE|SL_ERROR,
			"tl_conn_res:bad accepting endpoint"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tl_error_ack(wq, ackmp, TBADF, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * validate that accepting endpoint, if different from listening
	 * has address bound => state is TS_IDLE
	 * TROUBLE in XPG4 !!?
	 */
	if ((tep != acc_ep) && (acc_ep->te_state != TS_IDLE)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE|SL_ERROR,
			"tl_conn_res:accepting endpoint has no address bound,"
			"state=%d", acc_ep->te_state));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tl_error_ack(wq, ackmp, TOUTSTATE, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * validate if accepting endpt same as listening, then
	 * no other incoming connection should be on the queue
	 */

	if ((tep == acc_ep) && (tep->te_nicon > 1)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_conn_res: > 1 conn_ind on listener-acceptor"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tl_error_ack(wq, ackmp, TBADF, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * Mark for deletion, the entry corresponding to client
	 * on list of pending connections made by the listener
	 *  search list to see if client is one of the
	 * recorded as a listener and use "tipp" value later for deleting
	 */
	tipp = tl_icon_find(tep, cres->SEQ_number);
	tip = *tipp;
	if (tip == NULL) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE|SL_ERROR,
			"tl_conn_res:no client in listener list"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, tep->te_state);
		tl_error_ack(wq, ackmp, TBADSEQ, 0, cres->PRIM_type);
		freemsg(mp);
		return;
	}

	/*
	 * If ti_tep is NULL the client has already closed. In this case
	 * the code below will avoid any action on the client side
	 * but complete the server and acceptor state transitions.
	 */

	ASSERT(tip->ti_tep == NULL ||
		tip->ti_tep->te_seqno == cres->SEQ_number);
	cl_ep = tip->ti_tep;

	err = 0;
	if (cl_ep != NULL) {
		/*
		 * validate client state to be TS_WCON_CREQ or TS_DATA_XFER
		 * (latter for sockets only)
		 */
		if (cl_ep->te_state != TS_WCON_CREQ &&
		    (cl_ep->te_state != TS_DATA_XFER &&
		    (cl_ep->te_mode & TL_SOCKET))) {
			err = ECONNREFUSED;
			/*
			 * T_DISCON_IND sent later after committing memory
			 * and acking validity of request
			 */
			(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE,
				"tl_conn_res:peer in bad state"));
		}

		/*
		 * preallocate now for T_DISCON_IND or T_CONN_CONN
		 * ack validity of request (T_OK_ACK) after memory committed
		 */

		if (err)
			size = sizeof (struct T_discon_ind);
		else {
			/*
			 * calculate length of T_CONN_CON message
			 */
			olen = 0;
			if (cl_ep->te_flag & TL_SETCRED) {
				olen = (t_scalar_t)sizeof (struct opthdr) +
					OPTLEN(sizeof (tl_credopt_t));
			}
			size = T_ALIGN(sizeof (struct T_conn_con) +
					acc_ep->te_ap.ta_alen) + olen;
		}
		if (! tl_preallocate(mp, &respmp, &freemp, size)) {
			/*
			 * roll back state changes
			 */
			tep->te_state = TS_WRES_CIND;
			tl_memrecover(wq, mp, size);
			freemsg(ackmp);
			return;
		}
		mp = NULL;
	}

	/*
	 * Now ack validity of request
	 */
	if (tep->te_nicon == 1) {
		if (tep == acc_ep)
			tep->te_state = NEXTSTATE(TE_OK_ACK2, tep->te_state);
		else
			tep->te_state = NEXTSTATE(TE_OK_ACK3, tep->te_state);
	} else
		tep->te_state = NEXTSTATE(TE_OK_ACK4, tep->te_state);

	tl_ok_ack(wq, ackmp, cres->PRIM_type);

	/*
	 * send T_DISCON_IND now if client state validation failed earlier
	 */
	if (err) {
		/*
		 * flush the queues - why always ?
		 */
		(void) putnextctl1(acc_ep->te_rq, M_FLUSH, FLUSHRW);

		dimp = tl_resizemp(respmp, size);
		if (! dimp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_conn_res:con_ind:allocb failure"));
			tl_merror(wq, respmp, ENOMEM);
			freemsg(freemp);
			return;
		}
		if (dimp) {
			/* no user data in provider generated discon ind */
			freemsg(dimp->b_cont);
			dimp->b_cont = NULL;
		}

		DB_TYPE(dimp) = M_PROTO;
		di = (struct T_discon_ind *)dimp->b_rptr;
		di->PRIM_type  = T_DISCON_IND;
		di->DISCON_reason = err;
		di->SEQ_number = BADSEQNUM;

		tep->te_state = NEXTSTATE(TE_DISCON_IND1, tep->te_state);
		/*
		 * send T_DISCON_IND message
		 */
		putnext(acc_ep->te_rq, dimp);
		freemsg(freemp);
		return;
	}

	/*
	 * now start connecting the accepting endpoint
	 */
	if (tep != acc_ep)
		acc_ep->te_state = NEXTSTATE(TE_PASS_CONN, acc_ep->te_state);

	if (cl_ep == NULL) {
		/*
		 * The client has already closed. Send up any queued messages
		 * and change the state accordingly.
		 */
		tl_icon_sendmsgs(acc_ep, &tip->ti_mp);

		/*
		 * remove endpoint from incoming connection
		 * list using "tipp" value saved earlier
		 * delete client from list of incoming connections
		 */
		*tipp = tip->ti_next;
		tip->ti_next = NULL;
		ASSERT(tip->ti_mp == NULL);
		if (tip->ti_freetep != NULL)
			kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
		kmem_free(tip, sizeof (*tip));
		tep->te_nicon--;
		freemsg(mp);
		return;
	} else if (tip->ti_mp != NULL) {
		/*
		 * The client could have queued a T_DISCON_IND which needs
		 * to be sent up.
		 * Note that t_discon_req can not operate the same as
		 * t_data_req since it is not possible for it to putbq
		 * the message and return -1 due to the use of qwriter.
		 */
		tl_icon_sendmsgs(acc_ep, &tip->ti_mp);
	}

	/*
	 * prepare connect confirm T_CONN_CON message
	 */

	/*
	 * allocate the message - original data blocks
	 * retained in the returned mblk
	 */
	ccmp = tl_resizemp(respmp, size);
	if (! ccmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_conn_res:conn_con:allocb failure"));
		tl_merror(wq, respmp, ENOMEM);
		freemsg(freemp);
		return;
	}

	DB_TYPE(ccmp) = M_PROTO;
	cc = (struct T_conn_con *)ccmp->b_rptr;
	cc->PRIM_type  = T_CONN_CON;
	cc->RES_offset = (t_scalar_t)sizeof (struct T_conn_con);
	cc->RES_length = acc_ep->te_ap.ta_alen;
	addr_startp = ccmp->b_rptr + cc->RES_offset;
	bcopy(acc_ep->te_ap.ta_abuf, addr_startp, acc_ep->te_ap.ta_alen);
	if (cl_ep->te_flag & TL_SETCRED) {
		cc->OPT_offset = (t_scalar_t)T_ALIGN(cc->RES_offset +
					cc->RES_length);
		cc->OPT_length = olen;
		tl_fill_option(ccmp->b_rptr + cc->OPT_offset,
				(t_uscalar_t)sizeof (tl_credopt_t),
				TL_OPT_PEER_CRED,
				&acc_ep->te_cred);
	} else {
		cc->OPT_offset = 0;
		cc->OPT_length = 0;
	}

	/*
	 * make connection linking
	 * accepting and client endpoints
	 */
	cl_ep->te_conp = acc_ep;
	acc_ep->te_conp = cl_ep;

	cl_ep->te_oconp = NULL;
	acc_ep->te_oconp = NULL;
	/*
	 * remove endpoint from incoming connection
	 * list using "tipp" value saved earlier
	 * delete client from list of incoming connections
	 */
	*tipp = tip->ti_next;
	tip->ti_next = NULL;	/* break link of deleted node */
	ASSERT(tip->ti_mp == NULL);
	if (tip->ti_freetep != NULL)
		kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
	kmem_free(tip, sizeof (*tip));
	tep->te_nicon--;

	/*
	 * data blocks already linked in reallocb()
	 */

	/*
	 * link queues so that I_SENDFD will work
	 */
	WR(acc_ep->te_rq)->q_next = cl_ep->te_rq;
	WR(cl_ep->te_rq)->q_next = acc_ep->te_rq;

	/*
	 * send T_CONN_CON up on client side unless it was already
	 * done (for a socket). In cases any data or ordrel req has been
	 * queued make sure that the service procedure runs.
	 */
	if ((cl_ep->te_mode & TL_SOCKET) && !tl_disable_early_connect) {
		enableok(WR(cl_ep->te_rq));
		qenable(WR(cl_ep->te_rq));
		freemsg(ccmp);
	} else {
		/*
		 * change client state on TE_CONN_CON event
		 */
		cl_ep->te_state = NEXTSTATE(TE_CONN_CON, cl_ep->te_state);
		putnext(cl_ep->te_rq, ccmp);
	}

	freemsg(freemp);
}




static void
tl_discon_req(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	struct T_discon_req	*dr;
	ssize_t			msz;
	tl_endpt_t		*peer_tep;
	tl_icon_t		**tipp, *tip;
	size_t			size;
	mblk_t			*ackmp, *dimp, *freemp, *respmp;
	struct T_discon_ind	*di;
	t_scalar_t		save_state, new_state;


	/*
	 * preallocate memory for:
	 * 1. max of T_ERROR_ACK and T_OK_ACK
	 *	==> known max T_ERROR_ACK
	 * 2. for  T_DISCON_IND
	 */
	ackmp = allocb(sizeof (struct T_error_ack), BPRI_MED);
	if (! ackmp) {
		tl_memrecover(wq, mp, sizeof (struct T_error_ack));
		return;
	}
	/*
	 * memory committed for T_OK_ACK/T_ERROR_ACK now
	 * will be committed for T_DISCON_IND  later
	 */

	tep = (tl_endpt_t *)wq->q_ptr;
	dr = (struct T_discon_req *)mp->b_rptr;
	msz = MBLKL(mp);

	/*
	 * validate the state
	 */
	save_state = new_state = tep->te_state;
	if (! (save_state >= TS_WCON_CREQ && save_state <= TS_WRES_CIND) &&
	    ! (save_state >= TS_DATA_XFER && save_state <= TS_WREQ_ORDREL)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_DISCON_REQ:out of state, state=%d",
			tep->te_state));
		tl_error_ack(wq, ackmp, TOUTSTATE, 0, T_DISCON_REQ);
		freemsg(mp);
		return;
	}
	/*
	 * Defer committing the state change until it is determined if
	 * the message will be queued with the tl_icon or not.
	 */
	new_state  = NEXTSTATE(TE_DISCON_REQ, tep->te_state);

	/* validate the message */
	if (msz < sizeof (struct T_discon_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_discon_req:invalid message"));
		tep->te_state = NEXTSTATE(TE_ERROR_ACK, new_state);
		tl_error_ack(wq, ackmp, TSYSERR, EINVAL, T_DISCON_REQ);
		freemsg(mp);
		return;
	}

	/*
	 * if server, then validate that client exists
	 * by connection sequence number etc.
	 */
	if (tep->te_nicon > 0) { /* server */

		/*
		 * search server list for disconnect client
		 * - use value of "tipp" later for deletion
		 */
		tipp = tl_icon_find(tep, dr->SEQ_number);
		tip = *tipp;
		if (tip == NULL) {
			(void) (STRLOG(TL_ID, tep->te_minor, 2,
				SL_TRACE|SL_ERROR,
				"tl_discon_req:no disconnect endpoint"));
			tep->te_state = NEXTSTATE(TE_ERROR_ACK, new_state);
			tl_error_ack(wq, ackmp, TBADSEQ, 0, T_DISCON_REQ);
			freemsg(mp);
			return;
		}
		/*
		 * If ti_tep is NULL the client has already closed. In this case
		 * the code below will avoid any action on the client side.
		 */

		ASSERT(tip->ti_tep == NULL ||
			tip->ti_tep->te_seqno == dr->SEQ_number);
		peer_tep = tip->ti_tep;
	}

	/*
	 * preallocate now for T_DISCON_IND
	 * ack validity of request (T_OK_ACK) after memory committed
	 */
	size = sizeof (struct T_discon_ind);
	if (! tl_preallocate(mp, &respmp, &freemp, size)) {
		tl_memrecover(wq, mp, size);
		freemsg(ackmp);
		return;
	}

	/*
	 * prepare message to ack validity of request
	 */
	if (tep->te_nicon == 0)
		new_state = NEXTSTATE(TE_OK_ACK1, new_state);
	else
		if (tep->te_nicon == 1)
			new_state = NEXTSTATE(TE_OK_ACK2, new_state);
		else
			new_state = NEXTSTATE(TE_OK_ACK4, new_state);

	/*
	 * Flushing queues according to TPI. Using the old state.
	 */
	if ((tep->te_nicon <= 1) &&
	    ((save_state == TS_DATA_XFER) ||
	    (save_state == TS_WIND_ORDREL) ||
	    (save_state == TS_WREQ_ORDREL)))
		(void) putnextctl1(RD(wq), M_FLUSH, FLUSHRW);

	/* send T_OK_ACK up  */
	tl_ok_ack(wq, ackmp, T_DISCON_REQ);

	/*
	 * now do disconnect business
	 */
	if (tep->te_nicon > 0) { /* listener */
		if (peer_tep != NULL) {
			/*
			 * disconnect incoming connect request pending to tep
			 */
			if ((dimp = tl_resizemp(respmp, size)) == NULL) {
				(void) (STRLOG(TL_ID, tep->te_minor, 2,
					SL_TRACE|SL_ERROR,
					"tl_discon_req: reallocb failed"));
				tep->te_state = new_state;
				tl_merror(wq, respmp, ENOMEM);
				freemsg(freemp);
				return;
			}
			di = (struct T_discon_ind *)dimp->b_rptr;
			di->SEQ_number = BADSEQNUM;
			save_state = peer_tep->te_state;
			peer_tep->te_state = NEXTSTATE(TE_DISCON_IND1,
							peer_tep->te_state);
			peer_tep->te_oconp = NULL;
			enableok(WR(peer_tep->te_rq));
			qenable(WR(peer_tep->te_rq));
		} else {
			freemsg(respmp);
			dimp = NULL;
		}

		tl_icon_freemsgs(&tip->ti_mp);
		/*
		 * remove endpoint from incoming connection list
		 * using "tipp" value saved earlier
		 * - remove disconnect client from list on server
		 */
		*tipp = tip->ti_next;
		tip->ti_next = NULL;
		ASSERT(tip->ti_mp == NULL);
		if (tip->ti_freetep != NULL)
			kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
		kmem_free(tip, sizeof (*tip));
		tep->te_nicon--;
	} else if ((peer_tep = tep->te_oconp) != NULL) { /* client */
		/*
		 * disconnect an outgoing request pending from tep
		 */

		if ((dimp = tl_resizemp(respmp, size)) == NULL) {
			(void) (STRLOG(TL_ID, tep->te_minor, 2,
				SL_TRACE|SL_ERROR,
				"tl_discon_req: reallocb failed"));
			tep->te_state = new_state;
			tl_merror(wq, respmp, ENOMEM);
			freemsg(freemp);
			return;
		}
		di = (struct T_discon_ind *)dimp->b_rptr;
		DB_TYPE(dimp) = M_PROTO;
		di->PRIM_type  = T_DISCON_IND;
		di->DISCON_reason = ECONNRESET;
		di->SEQ_number = tep->te_seqno;

		/*
		 * If this is a socket the T_DISCON_IND is queued with
		 * the T_CONN_IND. Otherwise the T_CONN_IND is removed
		 * from the list of pending connections.
		 * Note that when te_oconp is set the peer better have
		 * a t_connind_t for the client.
		 */
		tipp = tl_icon_find(peer_tep, tep->te_seqno);
		tip = *tipp;
		ASSERT(tip != NULL);
		ASSERT(tip->ti_tep != NULL);
		ASSERT(tep == tip->ti_tep);
		if ((tep->te_mode & TL_SOCKET) && !tl_disable_early_connect) {
			/*
			 * No need to check TL_CLOSING and
			 * ti_tep == NULL since the T_DISCON_IND
			 * takes precedence over other queued
			 * messages.
			 */
			(void) tl_icon_queuemsg(peer_tep, tep->te_seqno, dimp);
			peer_tep = NULL;
			dimp = NULL;
			/*
			 * Can't clear te_oconp since tl_co_unconnect needs
			 * it as a hint not to free the tep.
			 * Keep the state unchanged since tl_conn_res inspects
			 * it.
			 */
			new_state = tep->te_state;
		} else {
			/* Found - delete it */
			*tipp = tip->ti_next;
			tip->ti_next = NULL;
			ASSERT(tip->ti_mp == NULL);
			if (tip->ti_freetep != NULL)
				kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
			kmem_free(tip, sizeof (*tip));
			save_state = peer_tep->te_state;
			if (peer_tep->te_nicon == 1)
				peer_tep->te_state =
					NEXTSTATE(TE_DISCON_IND2,
						peer_tep->te_state);
			else
				peer_tep->te_state =
				    NEXTSTATE(TE_DISCON_IND3,
						peer_tep->te_state);
			peer_tep->te_nicon--;
			tep->te_oconp = NULL;
		}
	} else if ((peer_tep = tep->te_conp) != NULL) { /* connected! */

		if ((dimp = tl_resizemp(respmp, size)) == NULL) {
			(void) (STRLOG(TL_ID, tep->te_minor, 2,
				SL_TRACE|SL_ERROR,
				"tl_discon_req: reallocb failed"));
			tep->te_state = new_state;
			tl_merror(wq, respmp, ENOMEM);
			freemsg(freemp);
			return;
		}
		di = (struct T_discon_ind *)dimp->b_rptr;
		di->SEQ_number = BADSEQNUM;

		save_state = peer_tep->te_state;
		peer_tep->te_state = NEXTSTATE(TE_DISCON_IND1,
						peer_tep->te_state);
	} else {
		/* Not connected */
		ASSERT(WR(tep->te_rq)->q_next == NULL);
		tep->te_state = new_state;
		freemsg(respmp);
		freemsg(freemp);
		return;
	}

	/* Commit state changes */
	tep->te_state = new_state;

	if (peer_tep == NULL) {
		ASSERT(dimp == NULL);
		goto done;
	}
	/*
	 * Flush queues on peer before sending up
	 * T_DISCON_IND according to TPI
	 */

	if ((save_state == TS_DATA_XFER) ||
	    (save_state == TS_WIND_ORDREL) ||
	    (save_state == TS_WREQ_ORDREL))
		(void) putnextctl1(peer_tep->te_rq, M_FLUSH, FLUSHRW);

	DB_TYPE(dimp) = M_PROTO;
	di->PRIM_type  = T_DISCON_IND;
	di->DISCON_reason = ECONNRESET;

	/*
	 * data blocks already linked into dimp by tl_preallocate()
	 */
	/*
	 * send indication message to peer user module
	 */
	ASSERT(dimp != NULL);
	putnext(peer_tep->te_rq, dimp);
done:
	if (tep->te_conp) {	/* disconnect pointers if connected */
		/*
		 * Messages may be queued on peer's write queue
		 * waiting to be processed by its write service
		 * procedure. Before the pointer to the peer transport
		 * structure is set to NULL, qenable the peer's write
		 * queue so that the queued up messages are processed.
		 */
		if ((save_state == TS_DATA_XFER) ||
		    (save_state == TS_WIND_ORDREL) ||
		    (save_state == TS_WREQ_ORDREL))
			(void) qenable(WR(peer_tep->te_rq));
		tep->te_conp = NULL;
		ASSERT(peer_tep);
		peer_tep->te_conp = NULL;
		/*
		 * unlink the streams
		 */
		(WR(tep->te_rq))->q_next = NULL;
		(WR(peer_tep->te_rq))->q_next = NULL;
	}

	freemsg(freemp);
}


static void
tl_addr_req(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	size_t			ack_sz;
	mblk_t			*ackmp;
	struct T_addr_ack	*taa;

	tep = (tl_endpt_t *)wq->q_ptr;

	/*
	 * Note: T_ADDR_REQ message has only PRIM_type field
	 * so it is already validated earlier.
	 */

	if (tep->te_mode == TL_TICLTS ||
	    tep->te_state > TS_WREQ_ORDREL ||
	    tep->te_state < TS_DATA_XFER) {
		/*
		 * Either connectionless or connection oriented but not
		 * in connected data transfer state or half-closed states.
		 */
		ack_sz = sizeof (struct T_addr_ack);
		if (tep->te_state >= TS_IDLE)
			/* is bound */
			ack_sz += tep->te_ap.ta_alen;
		ackmp = tl_reallocb(mp, ack_sz);
		if (! ackmp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_addr_req: reallocb failed"));
			tl_memrecover(wq, mp, ack_sz);
			return;
		}

		taa = (struct T_addr_ack *)ackmp->b_rptr;

		bzero((char *)taa, sizeof (struct T_addr_ack));

		taa->PRIM_type = T_ADDR_ACK;
		ackmp->b_datap->db_type = M_PCPROTO;
		ackmp->b_wptr = (u_char *)&taa[1];

		if (tep->te_state >= TS_IDLE) {
			/* endpoint is bound */
			taa->LOCADDR_length = tep->te_ap.ta_alen;
			taa->LOCADDR_offset = (t_scalar_t)sizeof (*taa);

			bcopy(tep->te_ap.ta_abuf, ackmp->b_wptr,
				tep->te_ap.ta_alen);
			ackmp->b_wptr += tep->te_ap.ta_alen;
			ASSERT(ackmp->b_wptr <= ackmp->b_datap->db_lim);
		}

		(void) qreply(wq, ackmp);
	} else {
		ASSERT(tep->te_state == TS_DATA_XFER ||
			tep->te_state == TS_WIND_ORDREL ||
			tep->te_state == TS_WREQ_ORDREL);
		/* connection oriented in data transfer */
		tl_connected_cots_addr_req(wq, mp);
	}
}


static void
tl_connected_cots_addr_req(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep, *peer_tep;
	size_t			ack_sz;
	mblk_t			*ackmp;
	struct T_addr_ack	*taa;
	u_char			*addr_startp;

	tep = (tl_endpt_t *)wq->q_ptr;

	ack_sz = sizeof (struct T_addr_ack);
	ack_sz += T_ALIGN(tep->te_ap.ta_alen);
	peer_tep = tep->te_conp;
	ack_sz += peer_tep->te_ap.ta_alen;

	ackmp = tl_reallocb(mp, ack_sz);
	if (! ackmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_connected_cots_addr_req: reallocb failed"));
		tl_memrecover(wq, mp, ack_sz);
		return;
	}

	taa = (struct T_addr_ack *)ackmp->b_rptr;

	bzero((char *)taa, sizeof (struct T_addr_ack));

	taa->PRIM_type = T_ADDR_ACK;
	ackmp->b_datap->db_type = M_PCPROTO;
	ackmp->b_wptr = (u_char *)&taa[1];

	ASSERT(tep->te_state >= TS_IDLE);

	/* endpoint is bound */
	taa->LOCADDR_length = tep->te_ap.ta_alen;
	taa->LOCADDR_offset = (t_scalar_t)sizeof (*taa);

	addr_startp = (u_char *)&taa[1];

	bcopy(tep->te_ap.ta_abuf, addr_startp,
	    tep->te_ap.ta_alen);

	taa->REMADDR_length = peer_tep->te_ap.ta_alen;
	taa->REMADDR_offset = (t_scalar_t)T_ALIGN(taa->LOCADDR_offset +
				    taa->LOCADDR_length);
	addr_startp = ackmp->b_rptr + taa->REMADDR_offset;
	bcopy(peer_tep->te_ap.ta_abuf, addr_startp,
	    peer_tep->te_ap.ta_alen);
	ackmp->b_wptr = (u_char *)ackmp->b_rptr +
	    taa->REMADDR_offset + peer_tep->te_ap.ta_alen;
	ASSERT(ackmp->b_wptr <= ackmp->b_datap->db_lim);

	(void) qreply(wq, ackmp);
}

static void
tl_copy_info(struct T_info_ack *ia, tl_endpt_t *tep)
{
	t_scalar_t		tl_tidusz;

	/*
	 * Deduce TIDU size to use
	 */
	if (strmsgsz)
		tl_tidusz = (t_scalar_t)strmsgsz;
	else {
		/*
		 * Note: "strmsgsz" being 0 has semantics that streams
		 * message sizes can be unlimited. We use a defined constant
		 * instead.
		 */
		tl_tidusz = TL_TIDUSZ;
	}

	if (tep->te_servtype == TL_TICLTS) {
		*ia = tl_clts_info_ack;
		ia->TSDU_size = tl_tidusz; /* TSDU and TIDU size are same */
	} else {
		*ia = tl_cots_info_ack;
		if (tep->te_servtype == TL_TICOTSORD)
			ia->SERV_type = T_COTS_ORD;
	}
	ia->TIDU_size = tl_tidusz;
	ia->CURRENT_state = tep->te_state;
}

/*
 * This routine responds to T_CAPABILITY_REQ messages.  It is called by
 * tl_wput.
 */
static void
tl_capability_req(queue_t *wq, mblk_t *mp)
{
	mblk_t			*ackmp;
	tl_endpt_t		*tep;
	t_uscalar_t		cap_bits1;
	struct T_capability_ack	*tcap;
	unsigned char		db_type;

	tep = (tl_endpt_t *)wq->q_ptr;
	cap_bits1 = ((struct T_capability_req *)mp->b_rptr)->CAP_bits1;
	db_type = mp->b_datap->db_type;

	ackmp = tl_reallocb(mp, sizeof (struct T_capability_ack));
	if (! ackmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_capability_req: reallocb failed"));
		tl_memrecover(wq, mp, sizeof (struct T_capability_ack));
		return;
	}

	DB_TYPE(ackmp) = db_type;
	tcap = (struct T_capability_ack *)mp->b_rptr;
	tcap->PRIM_type = T_CAPABILITY_ACK;
	tcap->CAP_bits1 = 0;

	if (cap_bits1 & TC1_INFO) {
		tl_copy_info(&tcap->INFO_ack, tep);
		tcap->CAP_bits1 |= TC1_INFO;
	}

	if (cap_bits1 & TC1_ACCEPTOR_ID) {
		tcap->ACCEPTOR_id = tep->te_acceptor_id;
		tcap->CAP_bits1 |= TC1_ACCEPTOR_ID;
	}

	qreply(wq, mp);
}

static void
tl_info_req(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	mblk_t			*ackmp;

	tep = (tl_endpt_t *)wq->q_ptr;

	ackmp = tl_reallocb(mp, sizeof (struct T_info_ack));
	if (! ackmp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_info_req: reallocb failed"));
		tl_memrecover(wq, mp, sizeof (struct T_info_ack));
		return;
	}

	/*
	 * fill in T_INFO_ACK contents
	 */
	DB_TYPE(ackmp) = M_PCPROTO;
	tl_copy_info((struct T_info_ack *)ackmp->b_rptr, tep);

	/*
	 * send ack message
	 */
	(void) qreply(wq, ackmp);
}

/*
 * Handle M_DATA, T_data_req and T_optdata_req.
 * If this is a socket pass through T_optdata_req options unmodified.
 */
static int
tl_data(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	union T_primitives	*prim;
	ssize_t			msz;
	tl_endpt_t		*peer_tep;
	queue_t			*peer_rq;

	tep = (tl_endpt_t *)wq->q_ptr;
	prim = (union T_primitives *)mp->b_rptr;
	msz = MBLKL(mp);

	if (tep->te_servtype == TL_TICLTS) {
		(void) (STRLOG(TL_ID, tep->te_minor, 2, SL_TRACE|SL_ERROR,
			"tl_wput:clts:unattached M_DATA"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}

	if (DB_TYPE(mp) == M_PROTO) {
		if (prim->type == T_DATA_REQ &&
		    msz < sizeof (struct T_data_req)) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_data:M_PROTO:invalid message"));
			tl_merror(wq, mp, EPROTO);
			return (-1);
		} else if (prim->type == T_OPTDATA_REQ &&
			    (msz < sizeof (struct T_optdata_req) ||
			    !(tep->te_mode & TL_SOCKET))) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_data:M_PROTO:invalid message"));
			tl_merror(wq, mp, EPROTO);
			return (-1);
		}
	}

	/*
	 * connection oriented provider
	 */
	switch (tep->te_state) {
	case TS_IDLE:
		/*
		 * Other end not here - do nothing.
		 */
		freemsg(mp);
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_data:cots with endpoint idle"));
		return (0);

	case TS_DATA_XFER:
		/* valid states */
		if (tep->te_conp != NULL)
			break;

		if (tep->te_oconp == NULL) {
			tl_merror(wq, mp, EPROTO);
			return (-1);
		}
		/*
		 * For a socket the T_CONN_CON is sent early thus
		 * the peer might not yet have accepted the connection.
		 * If we are closing queue the packet with the T_CONN_IND.
		 * Otherwise defer processing the packet until the peer
		 * accepts the connection.
		 * Note that the queue is noenabled when we go into this
		 * state.
		 */
		if (!(tep->te_flag & TL_CLOSING)) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_data: ocon"));
			(void) putbq(wq, mp);
			return (-1);
		}
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_data: closing socket ocon"));
		if (DB_TYPE(mp) == M_PROTO) {
			/* reuse message block - just change REQ to IND */
			if (prim->type == T_DATA_REQ)
				prim->type = T_DATA_IND;
			else
				prim->type = T_OPTDATA_IND;
		}
		(void) tl_icon_queuemsg(tep->te_oconp, tep->te_seqno, mp);
		return (0);

	case TS_WREQ_ORDREL:
		if (tep->te_conp == NULL) {
			/*
			 * Other end closed - generate discon_ind
			 * with reason 0 to cause an EPIPE but no
			 * read side error on AF_UNIX sockets.
			 */
			freemsg(mp);
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_data: WREQ_ORDREL and no peer"));
			tl_discon_ind(tep, 0);
			return (0);
		}
		break;

	default:
		/* invalid state for event TE_DATA_REQ */
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_data:cots:out of state"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	/*
	 * tep->te_state = NEXTSTATE(TE_DATA_REQ, tep->te_state);
	 * (State stays same on this event)
	 */

	/*
	 * get connected endpoint
	 */
	if ((peer_tep = tep->te_conp) == NULL) {
		freemsg(mp);
		/* Peer closed */
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
			"tl_data: peer gone"));
		return (0);
	}

	peer_rq = peer_tep->te_rq;

	/*
	 * Put it back if flow controlled except when we are closing.
	 * Note: Messages already on queue when we are closing is bounded
	 * so we can ignore flow control.
	 */
	if (! canputnext(peer_rq) && !(tep->te_flag & TL_CLOSING)) {
		(void) putbq(wq, mp);
		return (-1);
	}

	/*
	 * validate peer state
	 */
	switch (peer_tep->te_state) {
	case TS_DATA_XFER:
	case TS_WIND_ORDREL:
		/* valid states */
		break;
	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_data:rx side:invalid state"));
		tl_merror(WR(peer_rq), mp, EPROTO);
		return (0);
	}
	if (DB_TYPE(mp) == M_PROTO) {
		/* reuse message block - just change REQ to IND */
		if (prim->type == T_DATA_REQ)
			prim->type = T_DATA_IND;
		else
			prim->type = T_OPTDATA_IND;
	}
	/*
	 * peer_tep->te_state = NEXTSTATE(TE_DATA_IND, peer_tep->te_state);
	 * (peer state stays same on this event)
	 */
	/*
	 * send data to connected peer
	 */
	putnext(peer_rq, mp);
	return (0);
}



static int
tl_exdata(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	union T_primitives	*prim;
	ssize_t			msz;
	tl_endpt_t		*peer_tep;
	queue_t			*peer_rq;

	tep = (tl_endpt_t *)wq->q_ptr;
	prim = (union T_primitives *)mp->b_rptr;
	msz = MBLKL(mp);


	if (msz < sizeof (struct T_exdata_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_exdata:invalid message"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}

	/*
	 * validate state
	 */
	switch (tep->te_state) {
	case TS_IDLE:
		/*
		 * Other end not here - do nothing.
		 */
		freemsg(mp);
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_exdata:cots with endpoint idle"));
		return (0);

	case TS_DATA_XFER:
		/* valid states */
		if (tep->te_conp != NULL)
			break;

		if (tep->te_oconp == NULL) {
			tl_merror(wq, mp, EPROTO);
			return (-1);
		}
		/*
		 * For a socket the T_CONN_CON is sent early thus
		 * the peer might not yet have accepted the connection.
		 * If we are closing queue the packet with the T_CONN_IND.
		 * Otherwise defer processing the packet until the peer
		 * accepts the connection.
		 * Note that the queue is noenabled when we go into this
		 * state.
		 */
		if (!(tep->te_flag & TL_CLOSING)) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_exdata: ocon"));
			(void) putbq(wq, mp);
			return (-1);
		}
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_exdata: closing socket ocon"));
		prim->type = T_EXDATA_IND;
		(void) tl_icon_queuemsg(tep->te_oconp, tep->te_seqno, mp);
		return (0);

	case TS_WREQ_ORDREL:
		if (tep->te_conp == NULL) {
			/*
			 * Other end closed - generate discon_ind
			 * with reason 0 to cause an EPIPE but no
			 * read side error on AF_UNIX sockets.
			 */
			freemsg(mp);
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_exdata: WREQ_ORDREL and no peer"));
			tl_discon_ind(tep, 0);
			return (0);
		}
		break;

	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_EXDATA_REQ:out of state, state=%d",
			tep->te_state));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	/*
	 * tep->te_state = NEXTSTATE(TE_EXDATA_REQ, tep->te_state);
	 * (state stays same on this event)
	 */

	/*
	 * get connected endpoint
	 */
	if ((peer_tep = tep->te_conp) == NULL) {
		freemsg(mp);
		/* Peer closed */
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
			"tl_exdata: peer gone"));
		return (0);
	}

	peer_rq = peer_tep->te_rq;

	/*
	 * Put it back if flow controlled except when we are closing.
	 * Note: Messages already on queue when we are closing is bounded
	 * so we can ignore flow control.
	 */
	if (! canputnext(peer_rq) && !(tep->te_flag & TL_CLOSING)) {
		(void) putbq(wq, mp);
		return (-1);
	}

	/*
	 * validate state on peer
	 */
	switch (peer_tep->te_state) {
	case TS_DATA_XFER:
	case TS_WIND_ORDREL:
		/* valid states */
		break;
	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_exdata:rx side:invalid state"));
		tl_merror(WR(peer_rq), mp, EPROTO);
		return (0);
	}
	/*
	 * peer_tep->te_state = NEXTSTATE(TE_DATA_IND, peer_tep->te_state);
	 * (peer state stays same on this event)
	 */
	/*
	 * reuse message block
	 */
	prim->type = T_EXDATA_IND;

	/*
	 * send data to connected peer
	 */
	putnext(peer_rq, mp);
	return (0);
}



static int
tl_ordrel(queue_t *wq, mblk_t *mp)
{
	tl_endpt_t		*tep;
	union T_primitives	*prim;
	ssize_t			msz;
	tl_endpt_t		*peer_tep;
	queue_t			*peer_rq;

	tep = (tl_endpt_t *)wq->q_ptr;
	prim = (union T_primitives *)mp->b_rptr;
	msz = MBLKL(mp);

	if (msz < sizeof (struct T_ordrel_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_ordrel:invalid message"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}

	/*
	 * validate state
	 */
	switch (tep->te_state) {
	case TS_DATA_XFER:
	case TS_WREQ_ORDREL:
		/* valid states */
		if (tep->te_conp != NULL)
			break;

		if (tep->te_oconp == NULL)
			break;
		/*
		 * For a socket the T_CONN_CON is sent early thus
		 * the peer might not yet have accepted the connection.
		 * If we are closing queue the packet with the T_CONN_IND.
		 * Otherwise defer processing the packet until the peer
		 * accepts the connection.
		 * Note that the queue is noenabled when we go into this
		 * state.
		 */
		if (!(tep->te_flag & TL_CLOSING)) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_ordlrel: ocon"));
			(void) putbq(wq, mp);
			return (-1);
		}
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_ordlrel: closing socket ocon"));
		prim->type = T_ORDREL_IND;
		(void) tl_icon_queuemsg(tep->te_oconp, tep->te_seqno, mp);
		return (0);

	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_ORDREL_REQ:out of state, state=%d",
			tep->te_state));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	tep->te_state = NEXTSTATE(TE_ORDREL_REQ, tep->te_state);

	/*
	 * get connected endpoint
	 */
	if ((peer_tep = tep->te_conp) == NULL) {
		/* Peer closed */
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
			"tl_ordrel: peer gone"));
		freemsg(mp);
		return (0);
	}

	peer_rq = peer_tep->te_rq;

	/*
	 * Put it back if flow controlled except when we are closing.
	 * Note: Messages already on queue when we are closing is bounded
	 * so we can ignore flow control.
	 */
	if (! canputnext(peer_rq) && !(tep->te_flag & TL_CLOSING)) {
		(void) putbq(wq, mp);
		return (-1);
	}

	/*
	 * validate state on peer
	 */
	switch (peer_tep->te_state) {
	case TS_DATA_XFER:
	case TS_WIND_ORDREL:
		/* valid states */
		break;
	default:
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_ordrel:rx side:invalid state"));
		tl_merror(WR(peer_rq), mp, EPROTO);
		return (0);
	}
	peer_tep->te_state = NEXTSTATE(TE_ORDREL_IND, peer_tep->te_state);

	/*
	 * reuse message block
	 */
	prim->type = T_ORDREL_IND;
	(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
		"tl_ordrel: send ordrel_ind"));

	/*
	 * send data to connected peer
	 */
	putnext(peer_rq, mp);

	return (0);
}


/*
 * Send T_UDERROR_IND. The error should be from the <sys/errno.h> space.
 */
static void
tl_uderr(queue_t *wq, mblk_t *mp, t_scalar_t err)
{
	size_t			err_sz;
	tl_endpt_t		*tep;
	struct T_unitdata_req	*udreq;
	mblk_t			*err_mp;
	t_scalar_t		alen;
	t_scalar_t		olen;
	struct T_uderror_ind	*uderr;
	u_char			*addr_startp;

	err_sz = sizeof (struct T_uderror_ind);
	tep = (tl_endpt_t *)wq->q_ptr;
	udreq = (struct T_unitdata_req *)mp->b_rptr;
	alen = udreq->DEST_length;
	olen = udreq->OPT_length;

	if (alen > 0)
		err_sz = T_ALIGN(err_sz + alen);
	if (olen > 0)
		err_sz += olen;

	err_mp = allocb(err_sz, BPRI_MED);
	if (! err_mp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_uderr:allocb failure"));
		/*
		 * Note: no rollback of state needed as it does
		 * not change in connectionless transport
		 */
		tl_memrecover(wq, mp, err_sz);
		return;
	}

	DB_TYPE(err_mp) = M_PROTO;
	err_mp->b_wptr = err_mp->b_rptr + err_sz;
	uderr = (struct T_uderror_ind *)err_mp->b_rptr;
	uderr->PRIM_type = T_UDERROR_IND;
	uderr->ERROR_type = err;
	uderr->DEST_length = alen;
	uderr->OPT_length = olen;
	if (alen <= 0) {
		uderr->DEST_offset = 0;
	} else {
		uderr->DEST_offset =
			(t_scalar_t)sizeof (struct T_uderror_ind);
		addr_startp  = mp->b_rptr + udreq->DEST_offset;
		bcopy(addr_startp, err_mp->b_rptr + uderr->DEST_offset,
			(size_t)alen);
	}
	if (olen <= 0) {
		uderr->OPT_offset = 0;
	} else {
		uderr->OPT_offset =
			(t_scalar_t)T_ALIGN(sizeof (struct T_uderror_ind) +
						uderr->DEST_length);
		addr_startp  = mp->b_rptr + udreq->OPT_offset;
		bcopy(addr_startp, err_mp->b_rptr+uderr->OPT_offset,
			(size_t)olen);
	}
	freemsg(mp);

	/*
	 * send indication message
	 */
	tep->te_state = NEXTSTATE(TE_UDERROR_IND, tep->te_state);

	qreply(wq, err_mp);
}


/*
 * Handle T_unitdata_req.
 * If TL_SETCRED generate the credentials options.
 * If this is a socket pass through options unmodified.
 */
static int
tl_unitdata(queue_t *wq, mblk_t *mp)
{
	tl_addr_t		destaddr;
	u_char			*addr_startp;
	tl_endpt_t		*tep, *peer_tep;
	struct T_unitdata_ind	*udind;
	struct T_unitdata_req	*udreq;
	ssize_t			msz, ui_sz;
	t_scalar_t		alen, aoff, olen, ooff;

	tep = (tl_endpt_t *)wq->q_ptr;
	udreq = (struct T_unitdata_req *)mp->b_rptr;
	msz = MBLKL(mp);

	/*
	 * validate the state
	 */
	if (tep->te_state != TS_IDLE) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1,
			SL_TRACE|SL_ERROR,
			"tl_wput:T_CONN_REQ:out of state"));
		tl_merror(wq, mp, EPROTO);
		return (-1);
	}
	/*
	 * tep->te_state = NEXTSTATE(TE_UNITDATA_REQ, tep->te_state);
	 * (state does not change on this event)
	 */

	/*
	 * validate the message
	 * Note: dereference fields in struct inside message only
	 * after validating the message length.
	 */
	if (msz < sizeof (struct T_unitdata_req)) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_unitdata:invalid message length"));
		tl_merror(wq, mp, EINVAL);
		return (-1);
	}
	alen = udreq->DEST_length;
	aoff = udreq->DEST_offset;
	olen = udreq->OPT_length;
	ooff = udreq->OPT_offset;
	if (olen == 0)
		ooff = 0;

	if ((alen < 0) ||
	    (aoff < 0) ||
	    ((alen > 0) && ((aoff + alen) > msz)) ||
	    ((ssize_t)alen > (msz - sizeof (struct T_unitdata_req))) ||
	    ((aoff + alen) < 0) ||
	    ((olen > 0) && ((ooff + olen) > msz)) ||
	    (olen < 0) ||
	    (ooff < 0) ||
	    ((ssize_t)olen > (msz - sizeof (struct T_unitdata_req)))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_unitdata:invalid unit data message"));
		tl_merror(wq, mp, EINVAL);
		return (-1);
	}

	/* Options not supported unless it's a socket */
	if (alen == 0 || (olen != 0 && !(tep->te_mode & TL_SOCKET))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
		    "tl_unitdata:option use(unsupported) or zero len addr"));
		tl_uderr(wq, mp, EPROTO);
		return (0);
	}
#ifdef DEBUG
	/*
	 * Mild form of ASSERT()ion to detect broken TPI apps.
	 * if (! assertion)
	 *	log warning;
	 */
	if (! (aoff >= (t_scalar_t)sizeof (struct T_unitdata_req))) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_unitdata:addr overlaps TPI message"));
	}
#endif
	/*
	 * get destination endpoint
	 */
	destaddr.ta_alen = alen;
	destaddr.ta_abuf = (char *)(mp->b_rptr + aoff);

	ASSERT(destaddr.ta_alen > 0);

	if (TL_EQADDR(&destaddr, &tep->te_lastdest)) {
		/*
		 * Same as cached destination
		 */
		peer_tep = tep->te_lastep;
	} else  {
		/*
		 * Not the same as cached destination , need to search
		 * destination in the list of open endpoints
		 */
		if ((peer_tep = tl_addr_in_use(tep->te_mode, &destaddr))
			== NULL) {
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_unitdata:no one at destination address"));
			tl_uderr(wq, mp, ECONNRESET);
			return (0);
		}
		/*
		 * Save the address in cache if it fits, else clear
		 * the cache.
		 */
		if (destaddr.ta_alen <= TL_CLTS_CACHADDR_MAX) {
			tep->te_lastdest.ta_alen = destaddr.ta_alen;
			bcopy(destaddr.ta_abuf, tep->te_lastdest.ta_abuf,
				tep->te_lastdest.ta_alen);
			tep->te_lastep = peer_tep;
		} else {
			tep->te_lastdest.ta_alen = -1;
			tep->te_lastep = NULL;
		}
	}

	if (peer_tep->te_state != TS_IDLE) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_unitdata:provider in invalid state"));
		tl_uderr(wq, mp, EPROTO);
		return (0);
	}

	/*
	 * Hold 'tl_flow_rr_lock' to prevent 'te_flowrq' from being set
	 * after a thread executing in tl_cl_backenable() has walked past this
	 * entry in the list of transport endpoints (ie tl_olistp).
	 */
	mutex_enter(&tl_flow_rr_lock);

	/*
	 * Put it back if flow controlled except when we are closing.
	 * Note: Messages already on queue when we are closing is bounded
	 * so we can ignore flow control.
	 */
	if (!canputnext(peer_tep->te_rq) && !(tep->te_flag & TL_CLOSING)) {
		/* record what we are flow controlled on */
		tep->te_flowrq = peer_tep->te_rq;
		mutex_exit(&tl_flow_rr_lock);
		(void) putbq(wq, mp);
		return (-1);
	}
	mutex_exit(&tl_flow_rr_lock);

	/*
	 * prepare indication message
	 */

	/*
	 * calculate length of message
	 */
	if (peer_tep->te_flag & TL_SETCRED) {
		ASSERT(olen == 0);
		olen = (t_scalar_t)sizeof (struct opthdr) +
				OPTLEN(sizeof (tl_credopt_t));
					/* 1 option only */
	}

	ui_sz = T_ALIGN(sizeof (struct T_unitdata_ind) + tep->te_ap.ta_alen) +
		olen;
	/*
	 * If the unitdata_ind fits and we are not adding options
	 * reuse the udreq mblk.
	 */
	if (msz >= ui_sz && alen >= tep->te_ap.ta_alen &&
	    !(peer_tep->te_flag & TL_SETCRED)) {
		/*
		 * Reuse the original mblk. Leave options in place.
		 */
		udind =  (struct T_unitdata_ind *)mp->b_rptr;
		udind->PRIM_type = T_UNITDATA_IND;
		udind->SRC_length = tep->te_ap.ta_alen;
		addr_startp = mp->b_rptr + udind->SRC_offset;
		bcopy(tep->te_ap.ta_abuf, addr_startp, tep->te_ap.ta_alen);
	} else {
		/* Allocate a new T_unidata_ind message */
		mblk_t *ui_mp;

		ui_mp = allocb(ui_sz, BPRI_MED);
		if (! ui_mp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 4, SL_TRACE,
				"tl_unitdata:allocb failure:message queued"));
			tl_memrecover(wq, mp, ui_sz);
			return (-1);
		}

		/*
		 * fill in T_UNITDATA_IND contents
		 */
		DB_TYPE(ui_mp) = M_PROTO;
		ui_mp->b_wptr = ui_mp->b_rptr + ui_sz;
		udind =  (struct T_unitdata_ind *)ui_mp->b_rptr;
		udind->PRIM_type = T_UNITDATA_IND;
		udind->SRC_offset = (t_scalar_t)sizeof (struct T_unitdata_ind);
		udind->SRC_length = tep->te_ap.ta_alen;
		addr_startp = ui_mp->b_rptr + udind->SRC_offset;
		bcopy(tep->te_ap.ta_abuf, addr_startp, tep->te_ap.ta_alen);
		if (peer_tep->te_flag & TL_SETCRED) {
		    udind->OPT_offset = (t_scalar_t)T_ALIGN(udind->SRC_offset +
						udind->SRC_length);
		    udind->OPT_length = olen; /* because 1 opt only */
		    tl_fill_option(ui_mp->b_rptr + udind->OPT_offset,
				(t_uscalar_t)sizeof (tl_credopt_t),
				TL_OPT_PEER_CRED,
				&tep->te_cred);
		} else {
			udind->OPT_offset =
				(t_scalar_t)T_ALIGN(udind->SRC_offset +
						udind->SRC_length);
			udind->OPT_length = olen;
			bcopy((void *)((uintptr_t)udreq + ooff),
				(void *)((uintptr_t)udind + udind->OPT_offset),
				olen);
		}

		/*
		 * relink data blocks from mp to ui_mp
		 */
		ui_mp->b_cont = mp->b_cont;
		freeb(mp);
		mp = ui_mp;
	}
	/*
	 * send indication message
	 */
	peer_tep->te_state = NEXTSTATE(TE_UNITDATA_IND, peer_tep->te_state);
	putnext(peer_tep->te_rq, mp);
	return (0);
}



/*
 * Check if a given addr is in use.
 * Endpoint ptr returned or NULL if not found.
 * The name space is separate for each mode. This implies that
 * sockets get their own name space.
 */
static tl_endpt_t *
tl_addr_in_use(int32_t mode, tl_addr_t *ap)
{
	tl_endpt_t *tep;

	ASSERT(ap->ta_alen > 0);

	tep = tl_olistp;
	while (tep) {
		/*
		 * compare full address buffers
		 */
		if (mode == tep->te_mode && TL_EQADDR(ap, &tep->te_ap)) {
			/* addr is in use */
			return (tep);
		}
		tep = tep->te_nextp;
	}
	return (NULL);
}


/*
 * Generate a free addr and return it in struct pointed by ap
 * but allocating space for address buffer.
 * The generated address will be at least 4 bytes long and, if req->ta_alen
 * exceeds 4 bytes, be req->ta_alen bytes long.
 *
 * If req->ta_alen is larger than the default alen (4 bytes) the last
 * alen-4 bytes will always be the same as in req.
 *
 * Return 0 for failure.
 * Return non-zero for success.
 */
static int
tl_get_any_addr(int32_t mode, tl_addr_t *res, tl_addr_t *req)
{
	tl_addr_t	*def_tap;
	tl_addr_t	myaddr;
	char		*abuf;
	t_scalar_t	alen;
	uint32_t	loopcnt;	/* Limit loop to 2^32 */

	/*
	 * check if default addr is in use
	 * if it is - bump it and try again
	 */
	def_tap = &tl_defaultaddr;
	if (req == NULL)
		alen = def_tap->ta_alen;
	else
		alen = max(req->ta_alen, def_tap->ta_alen);
	abuf = kmem_zalloc((size_t)alen, KM_NOSLEEP);
	if (abuf == NULL)
		return (0); /* failure return */
	myaddr.ta_alen = alen;
	myaddr.ta_abuf = abuf;
	/* Copy in the address in req */
	if (req != NULL) {
		ASSERT(alen >= req->ta_alen);
		bcopy(req->ta_abuf, myaddr.ta_abuf, (size_t)req->ta_alen);
	}

	loopcnt = 0;
	for (;;) {
		bcopy(def_tap->ta_abuf, myaddr.ta_abuf,
		    (size_t)def_tap->ta_alen);
		if (tl_addr_in_use(mode, &myaddr) != NULL) {
			/*
			 * bump default addr
			 */
			(*(int32_t *)def_tap->ta_abuf)++;
			if (++loopcnt == 0)
				break;
		} else {
			/*
			 * found free address
			 */

			/*
			 * bump default address for next time
			 */
			(*(int32_t *)def_tap->ta_abuf)++;
			res->ta_alen = myaddr.ta_alen;
			res->ta_abuf = myaddr.ta_abuf;
			return (1); /* successful return */
		}
		/* NOTREACHED */
	}

	(void) kmem_free(abuf, (size_t)alen);

	(void) (STRLOG(TL_ID, -1, 1, SL_ERROR,
		"tl_get_any_addr:looped 2^32 times"));
	return (0);
}

/*
 * snarfed from internet mi.c
 * reallocate if necessary, else reuse message block
 */

static mblk_t *
tl_reallocb(mblk_t *mp, ssize_t new_size)
{
	mblk_t	*mp1;

	if (DB_REF(mp) == 1) {
		if ((mp->b_datap->db_lim - mp->b_rptr) >= new_size) {
			mp->b_wptr = mp->b_rptr + new_size;
			return (mp);
		}
		if ((MBLKSIZE(mp)) >= new_size) {
			mp->b_rptr = DB_BASE(mp);
			mp->b_wptr = mp->b_rptr + new_size;
			return (mp);
		}
	}
	if (mp1 = allocb(new_size, BPRI_MED)) {
		mp1->b_wptr = mp1->b_rptr + new_size;
		DB_TYPE(mp1) = DB_TYPE(mp);
		mp1->b_cont = mp->b_cont;
		freeb(mp);
	}
	return (mp1);
}

static mblk_t *
tl_resizemp(mblk_t *mp, ssize_t new_size)
{
	if (! mp || (DB_REF(mp) > 1) || (MBLKSIZE(mp) < new_size))
		return (NULL);

	if ((mp->b_datap->db_lim - mp->b_rptr) >= new_size) {
		mp->b_wptr = mp->b_rptr + new_size;
		return (mp);
	}
	mp->b_rptr = DB_BASE(mp);
	mp->b_wptr = mp->b_rptr + new_size;
	return (mp);
}



static int
tl_preallocate(mblk_t *reusemp, mblk_t **outmpp, mblk_t **freempp, ssize_t size)
{
	if ((reusemp == NULL) || (DB_REF(reusemp) > 1) ||
	    ((ssize_t)MBLKSIZE(reusemp) < size)) {
		if (! (*outmpp = allocb(size, BPRI_MED))) {
			/* out of memory */
			*freempp = (mblk_t *)NULL;
			return (0);
		}
		*freempp = reusemp;
		(*outmpp)->b_cont = reusemp->b_cont;
		reusemp->b_cont = NULL;
	} else {
		*outmpp = reusemp;
		*freempp = NULL;
	}
	return (1);
}

static void
tl_cl_backenable(queue_t *rq)
{
	tl_endpt_t *elp;

	/*
	 * more than one sender could be flow controlled
	 * look for next endpoint on the list flow
	 * controlled on us
	 * Holding the 'tl_flow_rr_lock' prevents a transport endpoint from
	 * being flow controlled on us after we have scanned its entry in the
	 * linked list. Thus preventing it from not being qenabled() at all.
	 */
	mutex_enter(&tl_flow_rr_lock);
	elp = tl_flow_rr_head;
	while (elp) {
		if (elp->te_flowrq == rq) {
			qenable(WR(elp->te_rq));
		}
		elp = elp->te_nextp;
	}
	if (tl_flow_rr_head != tl_olistp) {
		/*
		 * started from middle of list
		 * and reached end of list
		 * Start from beginning again
		 * and search up to starting point
		 */
		elp = tl_olistp;

		/*
		 * 'tl_flow_rr_lock' is also required to retain the sanity of
		 * 'tl_flow_rr_head'. If not for the lock, the tl_flow_rr_head
		 * could change while we are walking thru the list and we may
		 * end up not traversing the entire list.
		 */
		while (elp != tl_flow_rr_head) {
			if (elp->te_flowrq == rq) {
				qenable(WR(elp->te_rq));
			}
			elp = elp->te_nextp;
		}
	}
	/*
	 * Advance tl_flow_rr_head
	 */
	if (tl_flow_rr_head)
		tl_flow_rr_head = tl_flow_rr_head->te_nextp;
	else
		tl_flow_rr_head = tl_olistp;
	mutex_exit(&tl_flow_rr_lock);
}

/*
 * Returns non-zero if the tep structure can be freed (by tl_close).
 * If it can't be freed it is attached to a tl_icon_t structure and
 * will be freed when that structure is freed.
 */
static int
tl_co_unconnect(tl_endpt_t *tep)
{
	tl_endpt_t	*peer_tep;
	tl_icon_t	**tipp, *tip = NULL;
	tl_endpt_t	*cl_tep, *srv_tep;
	mblk_t		*d_mp;
	int		freetep = 1;

	/*
	 * disconnect processing cleanup the te structure
	 */

	if (tep->te_nicon > 0) {
		/*
		 * If incoming requests pending, change state
		 * of clients on disconnect ind event and send
		 * discon_ind pdu to modules above them
		 * for server: all clients get disconnect
		 */

		while ((tip = tep->te_iconp) != NULL) {
			cl_tep = tip->ti_tep;
			if (cl_tep != NULL) {
				d_mp = tl_discon_ind_alloc(ECONNREFUSED,
							BADSEQNUM);
				if (! d_mp) {
					(void) (STRLOG(TL_ID, tep->te_minor, 3,
						SL_TRACE|SL_ERROR,
						"tl_co_unconnect:icmng:"
						"allocb failure"));
					return (freetep);
				}
				cl_tep->te_oconp = NULL;
				cl_tep->te_state = NEXTSTATE(TE_DISCON_IND1,
							cl_tep->te_state);
				enableok(WR(cl_tep->te_rq));
				qenable(WR(cl_tep->te_rq));
				putnext(cl_tep->te_rq, d_mp);
			}
			tep->te_nicon--;
			tep->te_iconp = tip->ti_next;
			tl_icon_freemsgs(&tip->ti_mp);
			ASSERT(tip->ti_mp == NULL);
			if (tip->ti_freetep != NULL)
				kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
			kmem_free(tip, sizeof (*tip));
		}
	} else if ((srv_tep = tep->te_oconp) != NULL) {
		/*
		 * If outgoing request pending, change state
		 * of server on discon ind event
		 */

		if ((tep->te_mode & TL_SOCKET) && !tl_disable_early_connect &&
		    srv_tep->te_servtype == TL_TICOTSORD &&
		    !tl_icon_hasprim(srv_tep, tep->te_seqno, T_ORDREL_IND)) {
			/*
			 * Queue ordrel_ind for server to be picked up
			 * when the connection is accepted.
			 */
			d_mp = tl_ordrel_ind_alloc();
		} else {
			/*
			 * send discon_ind to server
			 */
			d_mp = tl_discon_ind_alloc(ECONNRESET, tep->te_seqno);
		}
		if (! d_mp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_co_unconnect:outgoing:allocb failure"));
			return (freetep);
		}

		/*
		 * If this is a socket the T_DISCON_IND is queued with
		 * the T_CONN_IND. Otherwise the T_CONN_IND is removed
		 * from the list of pending connections.
		 * Note that when te_oconp is set the peer better have
		 * a t_connind_t for the client.
		 */
		tipp = tl_icon_find(srv_tep, tep->te_seqno);
		tip = *tipp;
		ASSERT(tip != NULL);
		ASSERT(tip->ti_tep != NULL);
		ASSERT(tep == tip->ti_tep);
		if ((tep->te_mode & TL_SOCKET) && !tl_disable_early_connect) {
			/*
			 * Queue the message and disassociate the client
			 * endpoint from the t_connind_t since the client
			 * is going away.
			 */
			(void) tl_icon_queuemsg(srv_tep, tep->te_seqno, d_mp);
			d_mp = NULL;
			tip->ti_freetep = tip->ti_tep;
			tip->ti_tep = NULL;
			freetep = 0;
		} else {
			/* Found - delete it */
			*tipp = tip->ti_next;
			tip->ti_next = NULL;
			ASSERT(tip->ti_mp == NULL);
			if (tip->ti_freetep != NULL)
				kmem_free(tip->ti_freetep, sizeof (tl_endpt_t));
			kmem_free(tip, sizeof (*tip));
			/* change state on server */
			if (srv_tep->te_nicon == 1) {
				srv_tep->te_state =
					NEXTSTATE(TE_DISCON_IND2,
						srv_tep->te_state);
			} else {
				srv_tep->te_state =
					NEXTSTATE(TE_DISCON_IND3,
						srv_tep->te_state);
			}
			srv_tep->te_nicon--;
			tep->te_oconp = NULL;
		}
		if (d_mp != NULL) {
			(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
				"tl_co_unconnect:ocon: discon_ind state %d",
				srv_tep->te_state));
			ASSERT(*(uint32_t *)(d_mp->b_rptr) == T_DISCON_IND);
			putnext(srv_tep->te_rq, d_mp);
		}
	} else if ((peer_tep = tep->te_conp) != NULL) {
		/*
		 * unconnect existing connection
		 * If connected, change state of peer on
		 * discon ind event and send discon ind pdu
		 * to module above it
		 */

		if (peer_tep->te_servtype == TL_TICOTSORD &&
		    (peer_tep->te_state == TS_WIND_ORDREL ||
		    peer_tep->te_state == TS_DATA_XFER)) {
			/*
			 * send ordrel ind
			 */
			(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
			"tl_co_unconnect:connected: ordrel_ind state %d->%d",
				peer_tep->te_state,
				NEXTSTATE(TE_ORDREL_IND, peer_tep->te_state)));
			d_mp = tl_ordrel_ind_alloc();
			if (! d_mp) {
				(void) (STRLOG(TL_ID, tep->te_minor, 3,
				    SL_TRACE|SL_ERROR,
				    "tl_co_unconnect:connected:"
				    "allocb failure"));
				/*
				 * Continue with cleaning up peer as
				 * this side may go away with the close
				 */
				qenable(WR(peer_tep->te_rq));
				goto discon_peer;
			}
			peer_tep->te_state =
				NEXTSTATE(TE_ORDREL_IND, peer_tep->te_state);
			putnext(peer_tep->te_rq, d_mp);
			/*
			 * Handle flow control case.
			 * This will generate a t_discon_ind message with
			 * reason 0 if there is data queued on the write
			 * side.
			 */
			qenable(WR(peer_tep->te_rq));
		} else if (peer_tep->te_servtype == TL_TICOTSORD &&
			    peer_tep->te_state == TS_WREQ_ORDREL) {
			/*
			 * Sent an ordrel_ind. We send a discon with
			 * with error 0 to inform that the peer is gone.
			 */
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_co_unconnect: discon in state %d",
				tep->te_state));
			tl_discon_ind(peer_tep, 0);
		} else {
			(void) (STRLOG(TL_ID, tep->te_minor, 3,
				SL_TRACE|SL_ERROR,
				"tl_co_unconnect: state %d", tep->te_state));
			tl_discon_ind(peer_tep, ECONNRESET);
		}

discon_peer:
		/*
		 * Disconnect cross-pointers only for close
		 */
		if (tep->te_flag & TL_CLOSING) {
			peer_tep->te_conp = tep->te_conp = NULL;
			/*
			 * unlink the stream
			 */
			(WR(tep->te_rq))->q_next = NULL;
			(WR(peer_tep->te_rq))->q_next = NULL;
		}
	}
	return (freetep);
}

/*
 * Note: The following routine does not recover from allocb()
 * failures
 * The reason should be from the <sys/errno.h> space.
 */
static void
tl_discon_ind(tl_endpt_t *tep, uint32_t reason)
{
	mblk_t *d_mp;

	/*
	 * flush the queues.
	 */
	flushq(tep->te_rq, FLUSHDATA);
	(void) putnextctl1(tep->te_rq, M_FLUSH, FLUSHRW);

	/*
	 * send discon ind
	 */
	d_mp = tl_discon_ind_alloc(reason, tep->te_seqno);
	if (! d_mp) {
		(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE|SL_ERROR,
			"tl_discon_ind:allocb failure"));
		return;
	}
	(void) (STRLOG(TL_ID, tep->te_minor, 3, SL_TRACE,
		"tl_discon_ind: state %d->%d",
		tep->te_state,
		NEXTSTATE(TE_DISCON_IND1, tep->te_state)));
	tep->te_state = NEXTSTATE(TE_DISCON_IND1, tep->te_state);
	putnext(tep->te_rq, d_mp);
}

/*
 * Note: The following routine does not recover from allocb()
 * failures
 * The reason should be from the <sys/errno.h> space.
 */
static mblk_t
*tl_discon_ind_alloc(uint32_t reason, t_scalar_t seqnum)
{
	mblk_t *mp;
	struct T_discon_ind *tdi;

	if (mp = allocb(sizeof (struct T_discon_ind), BPRI_MED)) {
		DB_TYPE(mp) = M_PROTO;
		mp->b_wptr = mp->b_rptr + sizeof (struct T_discon_ind);
		tdi = (struct T_discon_ind *)mp->b_rptr;
		tdi->PRIM_type = T_DISCON_IND;
		tdi->DISCON_reason = reason;
		tdi->SEQ_number = seqnum;
	}
	return (mp);
}


/*
 * Note: The following routine does not recover from allocb()
 * failures
 */
static mblk_t
*tl_ordrel_ind_alloc(void)
{
	mblk_t *mp;
	struct T_ordrel_ind *toi;

	if (mp = allocb(sizeof (struct T_ordrel_ind), BPRI_MED)) {
		DB_TYPE(mp) = M_PROTO;
		mp->b_wptr = mp->b_rptr + sizeof (struct T_ordrel_ind);
		toi = (struct T_ordrel_ind *)mp->b_rptr;
		toi->PRIM_type = T_ORDREL_IND;
	}
	return (mp);
}


/*
 * Queue a T_CONN_IND for the server in FIFO order.
 */
static void
tl_icon_queue(tl_endpt_t *tep, tl_icon_t *tip)
{
	tl_icon_t **tipp;

	tip->ti_next = NULL;

	/* Find end of list */
	tipp = &tep->te_iconp;
	while (*tipp != NULL)
		tipp = &((*tipp)->ti_next);

	*tipp = tip;
	tep->te_nicon++;
}

/*
 * Lookup the seqno in the list of queued connections.
 * Always returns non-NULL. The caller should use
 * tl_icon_t *tip;
 *	tip = *tl_icon_find(tep, seqno);
 * when doing lookups.
 * However, the caller can use the returned 'tl_icon_t **'
 * to delete the entry after looking it up.
 */
static tl_icon_t **
tl_icon_find(tl_endpt_t *tep, t_scalar_t seqno)
{
	tl_icon_t **tipp, *tip;

	tipp = &tep->te_iconp;
	while ((tip = *tipp) != NULL) {
		if (tip->ti_seqno == seqno)
			break;
		tipp = &tip->ti_next;
	}
	ASSERT(tipp != NULL);
	return (tipp);
}

/*
 * Queue data for a given T_CONN_IND while verifying that redundant
 * messages, such as a T_ORDREL_IND after a T_DISCON_IND, are not queued.
 * Used when the originator of the connection closes.
 */
static int
tl_icon_queuemsg(tl_endpt_t *tep, t_scalar_t seqno, mblk_t *nmp)
{
	tl_icon_t		*tip;
	mblk_t			**mpp, *mp;
	int			prim, nprim;

	if (nmp->b_datap->db_type == M_PROTO)
		nprim = ((union T_primitives *)nmp->b_rptr)->type;
	else
		nprim = -1;	/* M_DATA */

	tip = *tl_icon_find(tep, seqno);
	ASSERT(tip != NULL);
	mpp = &tip->ti_mp;
	while (*mpp != NULL) {
		mp = *mpp;

		if (mp->b_datap->db_type == M_PROTO)
			prim = ((union T_primitives *)mp->b_rptr)->type;
		else
			prim = -1;	/* M_DATA */

		/*
		 * Allow nothing after a T_DISCON_IND
		 */
		if (prim == T_DISCON_IND) {
			freemsg(nmp);
			return (-1);
		}
		/*
		 * Only allow a T_DISCON_IND after an T_ORDREL_IND
		 */
		if (prim == T_ORDREL_IND && nprim != T_DISCON_IND) {
			freemsg(nmp);
			return (-1);
		}
		mpp = &(mp->b_next);
	}
	*mpp = nmp;
	return (0);
}

/*
 * Verify if a certain TPI primitive exists on the connind queue.
 * Use prim -1 for M_DATA.
 * Return non-zero if found.
 */
static int
tl_icon_hasprim(tl_endpt_t *tep, t_scalar_t seqno, t_scalar_t prim)
{
	tl_icon_t		*tip;
	mblk_t			*mp;
	int			prim2;

	tip = *tl_icon_find(tep, seqno);
	ASSERT(tip != NULL);
	mp = tip->ti_mp;
	while (mp != NULL) {
		if (mp->b_datap->db_type == M_PROTO)
			prim2 = ((union T_primitives *)mp->b_rptr)->type;
		else
			prim2 = -1;
		if (prim == prim2)
			return (1);
		mp = mp->b_next;
	}
	return (0);
}

/*
 * Send the b_next mblk chain that has accumulated before the connection
 * was accepted. Perform the necessary state transitions.
 */
static void
tl_icon_sendmsgs(tl_endpt_t *tep, mblk_t **mpp)
{
	mblk_t			*mp;
	union T_primitives	*primp;

	ASSERT(tep->te_state == TS_DATA_XFER);
	ASSERT(tep->te_rq->q_first == NULL);

	while ((mp = *mpp) != NULL) {
		*mpp = mp->b_next;
		mp->b_next = NULL;

		switch (mp->b_datap->db_type) {
		default:
#ifdef DEBUG
			cmn_err(CE_PANIC, "tl_icon_sendmsgs: unknown db_type");
#endif /* DEBUG */
			freemsg(mp);
			break;
		case M_DATA:
			putnext(tep->te_rq, mp);
			break;
		case M_PROTO:
			primp = (union T_primitives *)mp->b_rptr;
			switch (primp->type) {
			case T_UNITDATA_IND:
			case T_DATA_IND:
			case T_OPTDATA_IND:
			case T_EXDATA_IND:
				putnext(tep->te_rq, mp);
				break;
			case T_ORDREL_IND:
				tep->te_state = NEXTSTATE(TE_ORDREL_IND,
							tep->te_state);
				putnext(tep->te_rq, mp);
				break;
			case T_DISCON_IND:
				tep->te_state = NEXTSTATE(TE_DISCON_IND1,
							tep->te_state);
				putnext(tep->te_rq, mp);
				break;
			default:
#ifdef DEBUG
				cmn_err(CE_PANIC,
					"tl_icon_sendmsgs: unknown primitive");
#endif /* DEBUG */
				freemsg(mp);
				break;
			}
			break;
		}
	}
}

/*
 * Free the b_next mblk chain that has accumulated before the connection
 * was accepted.
 */
static void
tl_icon_freemsgs(mblk_t **mpp)
{
	mblk_t			*mp;

	while ((mp = *mpp) != NULL) {
		*mpp = mp->b_next;
		mp->b_next = NULL;
		freemsg(mp);
	}
}

/*
bs * Send M_ERROR
 * Note: assumes caller ensured enough space in mp or enough
 *	memory available. Does not attempt recovery from allocb()
 *	failures
 */

static void
tl_merror(queue_t *wq, mblk_t *mp, int error)
{
	tl_endpt_t *tep;

	tep = (tl_endpt_t *)wq->q_ptr;

	/*
	 * flush all messages on queue. we are shutting
	 * the stream down on fatal error
	 */
	flushq(wq, FLUSHALL);
	if ((tep->te_servtype == TL_TICOTS) ||
	    (tep->te_servtype == TL_TICOTSORD)) {
		/* connection oriented - unconnect endpoints */
		(void) tl_co_unconnect(tep);
	}
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	if ((MBLKSIZE(mp) < 1) || (DB_REF(mp) > 1)) {
		freemsg(mp);
		mp = allocb(1, BPRI_HI);
		if (!mp) {
			(void) (STRLOG(TL_ID, tep->te_minor, 1,
				SL_TRACE|SL_ERROR,
				"tl_merror:M_PROTO: out of memory"));
			return;
		}
	}
	if (mp) {
		DB_TYPE(mp) = M_ERROR;
		mp->b_rptr = DB_BASE(mp);
		*mp->b_rptr = (char)error;
		mp->b_wptr = mp->b_rptr + sizeof (char);
		qreply(wq, mp);
	}
}



static void
tl_fill_option(u_char *buf, t_uscalar_t optlen,
    t_uscalar_t optname, tl_credopt_t *optvalp)
{
	struct opthdr *opt;

	opt = (struct opthdr *)buf;
	opt->len = (t_uscalar_t)OPTLEN(optlen);
	opt->level = TL_PROT_LEVEL;
	opt->name = optname;
	bcopy(optvalp, buf + sizeof (struct opthdr), (size_t)optlen);
}

/* ARGSUSED */
static int
tl_default_opt(queue_t *wq, t_uscalar_t level,
    t_uscalar_t name, tl_credopt_t *ptr)
{
	/* no default value processed in protocol specific code currently */
	return (-1);
}

/* ARGSUSED */
static int
tl_get_opt(queue_t *wq, t_uscalar_t level, t_uscalar_t name, u_char *ptr)
{
	int len;

	len = 0;

	/*
	 * Assumes: option level and name sanity check done elsewhere
	 */

	switch (level) {
	case SOL_SOCKET:
		/*
		 * : TBD fill AF_UNIX socket options
		 */
		break;
	case TL_PROT_LEVEL:
		switch (name) {
		case TL_OPT_PEER_CRED:
			/*
			 * option not supposed to retrieved directly
			 * Only sent in T_CON_{IND,CON}, T_UNITDATA_IND
			 * when some internal flags set by other options
			 * Direct retrieval always designed to fail(ignored)
			 * for this option.
			 */
			break;
		}
	}
	return (len);
}

/* ARGSUSED */
static int
tl_set_opt(
	queue_t		*wq,
	t_scalar_t	mgmt_flags,
	t_uscalar_t	level,
	t_uscalar_t	name,
	t_uscalar_t	inlen,
	u_char		*invalp,
	t_uscalar_t	*outlenp,
	u_char		*outvalp)
{
	int error;

	error = 0;		/* NOERROR */

	/*
	 * Assumes: option level and name sanity checks done elsewhere
	 */

	switch (level) {
	case SOL_SOCKET:
		/*
		 * : TBD fill AF_UNIX socket options and then stop
		 * returning error
		 */
		error = EINVAL;
		break;
	case TL_PROT_LEVEL:
		switch (name) {
		case TL_OPT_PEER_CRED:
			/*
			 * option not supposed to be set directly
			 * Its value in initialized for each endpoint at
			 * driver open time.
			 * Direct setting always designed to fail for this
			 * option.
			 */
			error = EPROTO;
			break;
		}
	}
	return (error);
}


static void
tl_timer(void *arg)
{
	queue_t *wq = arg;
	tl_endpt_t *tep;

	tep = (tl_endpt_t *)wq->q_ptr;
	ASSERT(tep);

	tep->te_timoutid = 0;

	enableok(wq);
	/*
	 * Note: can call wsrv directly here and save context switch
	 * Consider change when qtimeout (not timeout) is active
	 */
	qenable(wq);
}

static void
tl_buffer(void *arg)
{
	queue_t *wq = arg;
	tl_endpt_t *tep;

	tep = (tl_endpt_t *)wq->q_ptr;
	ASSERT(tep);

	tep->te_bufcid = 0;

	enableok(wq);
	/*
	 *  Note: can call wsrv directly here and save context switch
	 * Consider change when qbufcall (not bufcall) is active
	 */
	qenable(wq);
}

static void
tl_memrecover(queue_t *wq, mblk_t *mp, size_t size)
{
	tl_endpt_t *tep;

	tep = (tl_endpt_t *)wq->q_ptr;
	noenable(wq);

	freezestr(wq);
	(void) insq(wq, wq->q_first, mp);
	unfreezestr(wq);

	if (tep->te_bufcid || tep->te_timoutid) {
		(void) (STRLOG(TL_ID, tep->te_minor, 1, SL_TRACE|SL_ERROR,
			"tl_memrecover:recover %p pending", (void *)wq));
		return;
	}

	if (!(tep->te_bufcid = qbufcall(wq, size, BPRI_MED, tl_buffer, wq))) {
		tep->te_timoutid = qtimeout(wq, tl_timer, wq, TL_BUFWAIT);
	}
}

#ifdef TL_DEBUG
static int32_t
change_and_log_state(int32_t ev, int32_t st)
{
	(void) (STRLOG(TL_ID, -1, 1, SL_TRACE,
		"Event= %d, State= %d, Nextstate= %d\n",
		ev, st, ti_statetbl[ev][st]));
	return (ti_statetbl[ev][st]);
}
#endif /* TL_DEBUG */
