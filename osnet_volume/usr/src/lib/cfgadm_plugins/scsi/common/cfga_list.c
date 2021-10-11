/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfga_list.c	1.2	99/10/05 SMI"

#include "cfga_scsi.h"

/* Structure for walking the tree */
typedef struct {
	apid_t		*apidp;
	char		*hba_logp;
	ldata_list_t	*listp;
	scfga_cmd_t	cmd;
	cfga_stat_t	chld_config;
	cfga_stat_t	hba_rstate;
	scfga_ret_t	ret;
	int		l_errno;
} scfga_list_t;

typedef struct {
	uint_t itype;
	const char *ntype;
	const char *name;
} scfga_devtype_t;

/* The TYPE field is parseable and should not contain spaces */
#define	SCFGA_BUS_TYPE		"scsi-bus"

/* Function prototypes */
static scfga_ret_t postprocess_list_data(const ldata_list_t *listp,
    scfga_cmd_t cmd, cfga_stat_t chld_config, int *np);
static int stat_dev(di_node_t node, void *arg);
static scfga_ret_t do_stat_bus(scfga_list_t *lap, int limited_stat);
static int get_bus_state(di_node_t node, void *arg);

static scfga_ret_t do_stat_dev(const di_node_t node, const char *nodepath,
    scfga_list_t *lap, int limited_stat);
static scfga_ret_t uscsi_inq(di_node_t node, const char *nodepath,
    cfga_list_data_t *clp);
static cfga_stat_t bus_devinfo_to_recep_state(uint_t bus_di_state);
static cfga_stat_t dev_devinfo_to_occupant_state(uint_t dev_di_state);
static const char *map_device_type(const char *nodetype,
    struct scsi_inquiry *inqp);
static void get_hw_info(di_node_t node, cfga_list_data_t *clp);


static scfga_devtype_t device_list[] = {
	{ DTYPE_DIRECT,		DDI_NT_BLOCK_CHAN,	"disk"},
	{ DTYPE_DIRECT,		DDI_NT_BLOCK,		"disk"},
	{ DTYPE_DIRECT,		DDI_NT_BLOCK_WWN,	"disk"},
	{ DTYPE_DIRECT,		DDI_NT_BLOCK_FABRIC,	"disk"},
	{ DTYPE_SEQUENTIAL,	DDI_NT_TAPE,		"tape"},
	{ DTYPE_PRINTER,	NULL,			"printer"},
	{ DTYPE_PROCESSOR,	NULL,			"processor"},
	{ DTYPE_WORM,		NULL,			"WORM"},
	{ DTYPE_RODIRECT,	DDI_NT_CD_CHAN,		"CD-ROM"},
	{ DTYPE_RODIRECT,	DDI_NT_CD,		"CD-ROM"},
	{ DTYPE_SCANNER,	NULL,			"scanner"},
	{ DTYPE_OPTICAL,	NULL,			"optical"},
	{ DTYPE_CHANGER,	NULL,			"med-changer"},
	{ DTYPE_COMM,		NULL,			"comm-device"},
	{ DTYPE_ARRAY_CTRL,	NULL,			"array-ctrl"},
	{ DTYPE_ESI,		NULL,			"ESI"}
};

#define	N_DEVICE_TYPES	(sizeof (device_list) / sizeof (device_list[0]))

scfga_ret_t
do_list(
	apid_t *apidp,
	scfga_cmd_t cmd,
	ldata_list_t **llpp,
	int *nelemp,
	char **errstring)
{
	int n = -1, l_errno = 0, limited_stat = 0;
	walkarg_t u;
	scfga_list_t larg = {NULL};
	scfga_ret_t ret;


	assert(apidp->hba_phys != NULL && apidp->path != NULL);

	if (*llpp != NULL || *nelemp != 0) {
		return (SCFGA_ERR);
	}

	/* Create the HBA logid (also base component of logical ap_id) */
	ret = make_hba_logid(apidp->hba_phys, &larg.hba_logp, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_LIST, 0);
		return (SCFGA_ERR);
	}

	assert(larg.hba_logp != NULL);

	larg.cmd = cmd;
	larg.apidp = apidp;
	larg.hba_rstate = CFGA_STAT_NONE;


	/* For all list commands, the bus needs to be stat'ed */
	if (larg.cmd == SCFGA_STAT_DEV) {
		limited_stat = 1; /* We need only bus state */
	} else {
		limited_stat = 0; /* Do a complete stat */
	}

	if ((ret = do_stat_bus(&larg, limited_stat)) != SCFGA_OK) {
		cfga_err(errstring, larg.l_errno, ERR_LIST, 0);
		goto out;
	}

#ifdef DEBUG
	if (limited_stat) {
		assert(larg.listp == NULL);
	} else {
		assert(larg.listp != NULL);
	}
#endif

	/* Assume that the bus has no configured children */
	larg.chld_config = CFGA_STAT_UNCONFIGURED;

	/*
	 * If stat'ing a specific device, we don't know if it exists yet.
	 * If stat'ing a bus or a bus and child devices, we have at least the
	 * bus stat data at this point.
	 */
	if (larg.cmd == SCFGA_STAT_DEV) {
		larg.ret = SCFGA_APID_NOEXIST;
	} else {
		larg.ret = SCFGA_OK;
	}

	/* we need to stat at least 1 device for all commands */
	u.node_args.flags = DI_WALK_CLDFIRST;
	u.node_args.fcn = stat_dev;

	/*
	 * Subtree is ALWAYS rooted at the HBA (not at the device) as
	 * otherwise deadlock may occur if bus is disconnected.
	 */
	ret = walk_tree(apidp->hba_phys, &larg, DINFOCPYALL, &u,
	    SCFGA_WALK_NODE, &larg.l_errno);

	if (ret != SCFGA_OK || (ret = larg.ret) != SCFGA_OK) {
		if (ret != SCFGA_APID_NOEXIST) {
			cfga_err(errstring, larg.l_errno, ERR_LIST, 0);
		}
		goto out;
	}

	assert(larg.listp != NULL);

	n = 0;
	ret = postprocess_list_data(larg.listp, cmd, larg.chld_config, &n);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, 0, ERR_LIST, 0);
		ret = SCFGA_LIB_ERR;
		goto out;
	}

	*nelemp = n;
	*llpp = larg.listp;
	ret = SCFGA_OK;
	/* FALLTHROUGH */
out:
	if (ret != SCFGA_OK) list_free(&larg.listp);
	S_FREE(larg.hba_logp);
	return (ret);
}

static scfga_ret_t
postprocess_list_data(
	const ldata_list_t *listp,
	scfga_cmd_t cmd,
	cfga_stat_t chld_config,
	int *np)
{
	ldata_list_t *tmplp = NULL;
	cfga_list_data_t *hba_ldatap = NULL;
	int i;


	*np = 0;

	if (listp == NULL) {
		return (SCFGA_ERR);
	}

	hba_ldatap = NULL;
	tmplp = (ldata_list_t *)listp;
	for (i = 0; tmplp != NULL; tmplp = tmplp->next) {
		i++;
		if (GET_DYN(tmplp->ldata.ap_phys_id) == NULL) {
			/* A bus stat data */
			assert(GET_DYN(tmplp->ldata.ap_log_id) == NULL);
			hba_ldatap = &tmplp->ldata;
#ifdef DEBUG
		} else {
			assert(GET_DYN(tmplp->ldata.ap_log_id) != NULL);
#endif
		}
	}

	switch (cmd) {
	case SCFGA_STAT_DEV:
		if (i != 1 || hba_ldatap != NULL) {
			return (SCFGA_LIB_ERR);
		}
		break;
	case SCFGA_STAT_BUS:
		if (i != 1 || hba_ldatap == NULL) {
			return (SCFGA_LIB_ERR);
		}
		break;
	case SCFGA_STAT_ALL:
		if (i < 1 || hba_ldatap == NULL) {
			return (SCFGA_LIB_ERR);
		}
		break;
	default:
		return (SCFGA_LIB_ERR);
	}

	*np = i;

	/* Fill in the occupant (child) state. */
	if (hba_ldatap != NULL) {
		hba_ldatap->ap_o_state = chld_config;
	}
	return (SCFGA_OK);
}

static int
stat_dev(di_node_t node, void *arg)
{
	scfga_list_t *lap = NULL;
	char *devfsp = NULL, *nodepath = NULL;
	size_t len = 0;
	int limited_stat = 0, match_minor, rv;
	scfga_ret_t ret;

	lap = (scfga_list_t *)arg;

	/* Skip stub nodes */
	if (IS_STUB_NODE(node)) {
		return (DI_WALK_CONTINUE);
	}

	/* Skip partial nodes */
	if (!known_state(node)) {
		return (DI_WALK_CONTINUE);
	}

	devfsp = di_devfs_path(node);
	if (devfsp == NULL) {
		rv = DI_WALK_CONTINUE;
		goto out;
	}

	len = strlen(DEVICES_DIR) + strlen(devfsp) + 1;

	nodepath = calloc(1, len);
	if (nodepath == NULL) {
		lap->l_errno = errno;
		lap->ret = SCFGA_LIB_ERR;
		rv = DI_WALK_TERMINATE;
		goto out;
	}

	(void) snprintf(nodepath, len, "%s%s", DEVICES_DIR, devfsp);

	/* Skip node if it is HBA */
	match_minor = 0;
	if (!dev_cmp(lap->apidp->hba_phys, nodepath, match_minor)) {
		rv = DI_WALK_CONTINUE;
		goto out;
	}

	/* If stat'ing a specific device, is this that device */
	if (lap->cmd == SCFGA_STAT_DEV) {
		assert(lap->apidp->path != NULL);
		if (dev_cmp(lap->apidp->path, nodepath, match_minor)) {
			rv = DI_WALK_CONTINUE;
			goto out;
		}
	}

	/*
	 * If stat'ing a bus only, we look at device nodes only to get
	 * bus configuration status. So a limited stat will suffice.
	 */
	if (lap->cmd == SCFGA_STAT_BUS) {
		limited_stat = 1;
	} else {
		limited_stat = 0;
	}

	/*
	 * Ignore errors if stat'ing a bus or listing all
	 */
	ret = do_stat_dev(node, nodepath, lap, limited_stat);
	if (ret != SCFGA_OK) {
		if (lap->cmd == SCFGA_STAT_DEV) {
			lap->ret = ret;
			rv = DI_WALK_TERMINATE;
		} else {
			rv = DI_WALK_CONTINUE;
		}
		goto out;
	}

	/* Are we done ? */
	rv = DI_WALK_CONTINUE;
	if (lap->cmd == SCFGA_STAT_BUS &&
	    lap->chld_config == CFGA_STAT_CONFIGURED) {
		rv = DI_WALK_TERMINATE;
	} else if (lap->cmd == SCFGA_STAT_DEV) {
		/*
		 * If stat'ing a specific device, we are done at this point.
		 */
		lap->ret = SCFGA_OK;
		rv = DI_WALK_TERMINATE;
	}

	/*FALLTHRU*/
out:
	S_FREE(nodepath);
	if (devfsp != NULL) di_devfs_path_free(devfsp);
	return (rv);
}


static scfga_ret_t
do_stat_bus(scfga_list_t *lap, int limited_stat)
{
	cfga_list_data_t *clp = NULL;
	ldata_list_t *listp = NULL;
	int l_errno = 0;
	uint_t devinfo_state = 0;
	walkarg_t u;
	scfga_ret_t ret;

	assert(lap->hba_logp != NULL);

	/* Get bus state */
	u.node_args.flags = 0;
	u.node_args.fcn = get_bus_state;

	ret = walk_tree(lap->apidp->hba_phys, &devinfo_state, DINFOPROP, &u,
	    SCFGA_WALK_NODE, &l_errno);
	if (ret == SCFGA_OK) {
		lap->hba_rstate = bus_devinfo_to_recep_state(devinfo_state);
	} else {
		lap->hba_rstate = CFGA_STAT_NONE;
	}

	if (limited_stat) {
		/* We only want to know bus(receptacle) connect status */
		return (SCFGA_OK);
	}

	listp = calloc(1, sizeof (ldata_list_t));
	if (listp == NULL) {
		lap->l_errno = errno;
		return (SCFGA_LIB_ERR);
	}

	clp = &listp->ldata;

	(void) snprintf(clp->ap_log_id, sizeof (clp->ap_log_id), "%s",
	    lap->hba_logp);
	(void) snprintf(clp->ap_phys_id, sizeof (clp->ap_phys_id), "%s",
	    lap->apidp->hba_phys);

	clp->ap_class[0] = '\0';	/* Filled by libcfgadm */
	clp->ap_r_state = lap->hba_rstate;
	clp->ap_o_state = CFGA_STAT_NONE; /* filled in later by the plug-in */
	clp->ap_cond = CFGA_COND_UNKNOWN;
	clp->ap_busy = 0;
	clp->ap_status_time = (time_t)-1;
	clp->ap_info[0] = '\0';

	(void) snprintf(clp->ap_type, sizeof (clp->ap_type), "%s",
	    SCFGA_BUS_TYPE);

	/* Link it in */
	listp->next = lap->listp;
	lap->listp = listp;

	return (SCFGA_OK);
}

static int
get_bus_state(di_node_t node, void *arg)
{
	uint_t *di_statep = (uint_t *)arg;

	*di_statep = di_state(node);

	return (DI_WALK_TERMINATE);
}

static scfga_ret_t
do_stat_dev(
	const di_node_t node,
	const char *nodepath,
	scfga_list_t *lap,
	int limited_stat)
{
	uint_t dctl_state = 0, devinfo_state = 0;
	char *dyncomp = NULL;
	cfga_list_data_t *clp = NULL;
	cfga_busy_t busy;
	ldata_list_t *listp = NULL;
	int l_errno = 0;
	cfga_stat_t ostate;
	scfga_ret_t ret;

	assert(lap->apidp->hba_phys != NULL);
	assert(lap->hba_logp != NULL);

	devinfo_state = di_state(node);
	ostate = dev_devinfo_to_occupant_state(devinfo_state);

	/*
	 * NOTE: The framework cannot currently detect layered driver
	 * opens, so the busy indicator is not very reliable. Also,
	 * non-root users will not be able to determine busy
	 * status (libdevice needs root permissions).
	 * This should probably be fixed by adding a DI_BUSY to the di_state()
	 * routine in libdevinfo.
	 */
	if (devctl_cmd(nodepath, SCFGA_DEV_GETSTATE, &dctl_state,
	    &l_errno) == SCFGA_OK) {
		busy = ((dctl_state & DEVICE_BUSY) == DEVICE_BUSY) ? 1 : 0;
	} else {
		busy = 0;
	}

	/* If child device is configured, record it */
	if (ostate == CFGA_STAT_CONFIGURED) {
		lap->chld_config = CFGA_STAT_CONFIGURED;
	}

	if (limited_stat) {
		/* We only want to know device config state */
		return (SCFGA_OK);
	}

	listp = calloc(1, sizeof (ldata_list_t));
	if (listp == NULL) {
		lap->l_errno = errno;
		return (SCFGA_LIB_ERR);
	}

	clp = &listp->ldata;

	/* Create the dynamic component */
	ret = make_dyncomp(node, nodepath, &dyncomp, &lap->l_errno);
	if (ret != SCFGA_OK) {
		S_FREE(listp);
		return (ret);
	}

	assert(dyncomp != NULL);

	/* Create logical and physical ap_id */
	(void) snprintf(clp->ap_log_id, sizeof (clp->ap_log_id), "%s%s%s",
	    lap->hba_logp, DYN_SEP, dyncomp);

	(void) snprintf(clp->ap_phys_id, sizeof (clp->ap_phys_id), "%s%s%s",
	    lap->apidp->hba_phys, DYN_SEP, dyncomp);

	S_FREE(dyncomp);

	clp->ap_class[0] = '\0'; /* Filled in by libcfgadm */
	clp->ap_r_state = lap->hba_rstate;
	clp->ap_o_state = ostate;
	clp->ap_cond = CFGA_COND_UNKNOWN;
	clp->ap_busy = busy;
	clp->ap_status_time = (time_t)-1;

	/* Cannot do inquiry if bus is not connected */
	if (clp->ap_r_state != CFGA_STAT_CONNECTED ||
	    uscsi_inq(node, nodepath, clp) != SCFGA_OK) {
		get_hw_info(node, clp);
	}

	/* Link it in */
	listp->next = lap->listp;
	lap->listp = listp;

	return (SCFGA_OK);
}

/*
 * Called if inquiry fails.
 */
static void
get_hw_info(di_node_t node, cfga_list_data_t *clp)
{
	const char *emsg = GET_MSG_STR(ERR_UNAVAILABLE);
	di_minor_t minor;
	const char *dtype;

	/*
	 * No inquiry information available
	 */
	(void) snprintf(clp->ap_info, sizeof (clp->ap_info), "%s", S_STR(emsg));

	/*
	 * Use minor nodetype to determine the type of device
	 */
	dtype = NULL;
	minor = di_minor_next(node, DI_MINOR_NIL);
	for (; minor != DI_MINOR_NIL; minor = di_minor_next(node, minor)) {
		dtype = map_device_type(di_minor_nodetype(minor), NULL);
		if (dtype != NULL) {
			break;
		}
	}

	if (dtype == NULL) {
		dtype = S_STR(emsg);
	}

	(void) snprintf(clp->ap_type, sizeof (clp->ap_type), "%s", dtype);
}


#define	INQ_ALLOC_MAX	255

/* Based on a similar routine in the format command */
static scfga_ret_t
uscsi_inq(di_node_t node, const char *nodepath, cfga_list_data_t *clp)
{
	struct uscsi_cmd	ucmd;
	union scsi_cdb		scdb;
	struct scsi_inquiry	*inqp = NULL;
	int rv = -1;
	const int inqbuflen = INQ_ALLOC_MAX;
	const size_t physlen = MAXPATHLEN;
	char *cp = NULL;
	di_minor_t minor;
	int fd = -1, len = 0;
	char *inqbuf, *physpath, *min_name = NULL;
	scfga_ret_t ret;

	assert(inqbuflen >= sizeof (struct scsi_inquiry));


	inqbuf = calloc(1, inqbuflen);
	physpath = calloc(1, physlen);
	if (inqbuf == NULL || physpath == NULL) {
		S_FREE(inqbuf);
		S_FREE(physpath);
		return (SCFGA_LIB_ERR);
	}

	(void) memset((char *)&ucmd, 0, sizeof (struct uscsi_cmd));
	(void) memset((char *)&scdb, 0, sizeof (union scsi_cdb));
	(void) memset(inqbuf, 0, inqbuflen);

	/* Initialize command descriptor block */
	scdb.cdb_un.cmd = SCMD_INQUIRY;

	/*
	 * Set allocation length (buffer space alloc'ed by initiator
	 * for data from target. Max of 255 bytes)
	 */
	assert(inqbuflen <= ~((uchar_t)0));
	FORMG0COUNT(&scdb, (uchar_t)inqbuflen);

	/* Initialize the uscsi_cmd structure */
	ucmd.uscsi_cdb = (caddr_t)&scdb;	/* cdb structure */
	ucmd.uscsi_cdblen = CDB_GROUP0;		/* cdb size for GROUP0 cmd */
	ucmd.uscsi_bufaddr = (caddr_t)inqbuf;   /* buf for inq data */
	ucmd.uscsi_buflen = inqbuflen;

	ucmd.uscsi_flags =  USCSI_ISOLATE | /* Isolate from any other cmds. */
			    USCSI_DIAGNOSE| /* Fail on any error */
			    USCSI_SILENT  | /* Don't emit err msg  */
			    USCSI_READ;	    /* We want data from target */
	ucmd.uscsi_timeout = 5;		    /* 5 sec timeout (correct ?) */

	/* Use any character special file for inquiry */
	(void) snprintf(physpath, physlen, "%s:", nodepath);

	cp = physpath + strlen(physpath);
	len = physlen - strlen(physpath);

	fd = -1;
	minor = di_minor_next(node, DI_MINOR_NIL);
	for (; minor != DI_MINOR_NIL; minor = di_minor_next(node, minor)) {

		if (di_minor_spectype(minor) != S_IFCHR)
			continue;
		if ((min_name = di_minor_name(minor)) == NULL)
			continue;

		(void) snprintf(cp, len, "%s", min_name);

		if ((fd = open(physpath, O_RDONLY|O_NDELAY)) != -1) {
			break;
		}
	}

	if (fd == -1) {
		ret = SCFGA_ERR;
		goto out;
	}

	rv = ioctl(fd, USCSICMD, &ucmd);
	(void) close(fd);

	if (rv != 0 || ucmd.uscsi_status != 0)	{
		ret = SCFGA_ERR;
		goto out;
	}

	inqp = (struct scsi_inquiry *)inqbuf;

	/*
	 * Fill in type information
	 */
	cp = (char *)map_device_type(NULL, inqp);
	if (cp == NULL) {
		cp = (char *)GET_MSG_STR(ERR_UNAVAILABLE);
	}
	(void) snprintf(clp->ap_type, sizeof (clp->ap_type), "%s", S_STR(cp));

	/*
	 * Fill in vendor and product ID.
	 * Note: The information in the inquiry data may not be
	 * NULL terminated
	 */
	len = sizeof (clp->ap_info);
	(void) snprintf(clp->ap_info, len, "%.*s %.*s", sizeof (inqp->inq_vid),
	    inqp->inq_vid, sizeof (inqp->inq_pid), inqp->inq_pid);

	ret = SCFGA_OK;
	/* FALLTHROUGH */
out:
	S_FREE(physpath);
	S_FREE(inqbuf);
	return (ret);
}

static const char *
map_device_type(const char *nodetype, struct scsi_inquiry *inqp)
{
	const char *name, *ucp = "unknown";
	uint_t itype;
	int i;

	if (inqp != NULL) {
		/*
		 * Use information from inquiry
		 */
		itype = inqp->inq_dtype & DTYPE_MASK;

		name = NULL;
		for (i = 0; i < N_DEVICE_TYPES; i++) {
			if (device_list[i].itype == DTYPE_UNKNOWN)
				continue;
			if (itype == device_list[i].itype) {
				name = device_list[i].name;
				break;
			}
		}
		if (name == NULL)
			name = ucp;

		switch (inqp->inq_dtype & ~DTYPE_MASK) {
			case DPQ_SUPPORTED:
			case DPQ_NEVER:
				name = ucp;
				break;
			case DPQ_POSSIBLE:
			case DPQ_VUNIQ:
			default:
				break;
		}
		return (name);

	} else if (nodetype != NULL) {
		for (i = 0; i < N_DEVICE_TYPES; i++) {
			if (device_list[i].ntype == NULL)
				continue;
			if (strcmp(nodetype, device_list[i].ntype) == 0) {
				return (device_list[i].name);
			}
		}
		return (ucp);
	} else {
		return (NULL);
	}
}

/* Transform list data to stat data */
scfga_ret_t
list_ext_postprocess(
	ldata_list_t		**llpp,
	int			nelem,
	cfga_list_data_t	**ap_id_list,
	int			*nlistp,
	char			**errstring)
{
	cfga_list_data_t *ldatap = NULL;
	ldata_list_t *tmplp = NULL;
	int i = -1;

	*ap_id_list = NULL;
	*nlistp = 0;

	if (*llpp == NULL || nelem < 0) {
		return (SCFGA_LIB_ERR);
	}

	if (nelem == 0) {
		return (SCFGA_APID_NOEXIST);
	}

	ldatap = calloc(nelem, sizeof (cfga_list_data_t));
	if (ldatap == NULL) {
		cfga_err(errstring, errno, ERR_LIST, 0);
		return (SCFGA_LIB_ERR);
	}

	/* Extract the list_data structures from the linked list */
	tmplp = *llpp;
	for (i = 0; i < nelem && tmplp != NULL; i++) {
		ldatap[i] = tmplp->ldata;
		tmplp = tmplp->next;
	}

	if (i < nelem || tmplp != NULL) {
		S_FREE(ldatap);
		return (SCFGA_LIB_ERR);
	}

	*nlistp = nelem;
	*ap_id_list = ldatap;

	return (SCFGA_OK);
}

/*
 * Convert bus state to receptacle state
 */
static cfga_stat_t
bus_devinfo_to_recep_state(uint_t bus_di_state)
{
	cfga_stat_t rs;

	switch (bus_di_state) {
	case DI_BUS_QUIESCED:
	case DI_BUS_DOWN:
		rs = CFGA_STAT_DISCONNECTED;
		break;
	/*
	 * NOTE: An explicit flag for active should probably be added to
	 * libdevinfo.
	 */
	default:
		rs = CFGA_STAT_CONNECTED;
		break;
	}

	return (rs);
}

/*
 * Convert device state to occupant state
 */
static cfga_stat_t
dev_devinfo_to_occupant_state(uint_t dev_di_state)
{
	/* Driver attached ? */
	if ((dev_di_state & DI_DRIVER_DETACHED) != DI_DRIVER_DETACHED) {
		return (CFGA_STAT_CONFIGURED);
	}

	if ((dev_di_state & DI_DEVICE_OFFLINE) == DI_DEVICE_OFFLINE ||
	    (dev_di_state & DI_DEVICE_DOWN) == DI_DEVICE_DOWN) {
		return (CFGA_STAT_UNCONFIGURED);
	} else {
		return (CFGA_STAT_NONE);
	}
}
