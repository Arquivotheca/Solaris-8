/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cfga_utils.c 1.4     99/08/10 SMI"

#include "cfga_scsi.h"

/*
 * This file contains helper routines for the SCSI plugin
 */

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

typedef struct strlist {
	const char *str;
	struct strlist *next;
} strlist_t;

typedef	struct {
	scfga_ret_t scsi_err;
	cfga_err_t  cfga_err;
} errcvt_t;

typedef struct {
	scfga_cmd_t cmd;
	int type;
	int (*fcn)(const devctl_hdl_t);
} set_state_cmd_t;

typedef struct {
	scfga_cmd_t cmd;
	int type;
	int (*state_fcn)(const devctl_hdl_t, uint_t *);
} get_state_cmd_t;

/* defines for nftw() */
#define	NFTW_DEPTH	1
#define	NFTW_CONTINUE	0
#define	NFTW_TERMINATE	1
#define	NFTW_ERROR	-1

/* Function prototypes */
static int do_recurse_dev(const char *path, const struct stat *sbuf,
    int type, struct FTW *ftwp);
static scfga_recur_t lookup_dev(const char *lpath, void *arg);
static char *pathdup(const char *path, int *l_errnop);
static void msg_common(char **err_msgpp, int append_newline, int l_errno,
    va_list ap);

/* Globals */
struct {
	mutex_t mp;
	void *arg;
	scfga_recur_t (*fcn)(const char *lpath, void *arg);
} nftw_arg = {DEFAULTMUTEX};

/*
 * The string table contains most of the strings used by the scsi cfgadm plugin.
 * All strings which are to be internationalized must be in this table.
 * Some strings which are not internationalized are also included here.
 * Arguments to messages are NOT internationalized.
 */
msgcvt_t str_tbl[] = {

/*
 * The first element (ERR_UNKNOWN) MUST always be present in the array.
 */
#define	UNKNOWN_ERR_IDX		0	/* Keep the index in sync */


/* msg_code	num_args, I18N	msg_string				*/

/* ERRORS */
{ERR_UNKNOWN,		0, 1,	"unknown error"},
{ERR_OP_FAILED,		0, 1,	"operation failed"},
{ERR_CMD_INVAL,		0, 1,	"invalid command"},
{ERR_NOT_BUSAPID,	0, 1,	"not a SCSI bus apid"},
{ERR_APID_INVAL,	0, 1,	"invalid SCSI ap_id"},
{ERR_NOT_BUSOP,		0, 1,	"operation not supported for SCSI bus"},
{ERR_NOT_DEVOP,		0, 1,	"operation not supported for SCSI device"},
{ERR_UNAVAILABLE,	0, 1,	"unavailable"},
{ERR_CTRLR_CRIT,	0, 1,	"critical partition controlled by SCSI HBA"},
{ERR_BUS_GETSTATE,	0, 1,	"failed to get state for SCSI bus"},
{ERR_BUS_NOTCONNECTED,	0, 1,	"SCSI bus not connected"},
{ERR_BUS_CONNECTED,	0, 1,	"SCSI bus not disconnected"},
{ERR_BUS_QUIESCE,	0, 1,	"SCSI bus quiesce failed"},
{ERR_BUS_UNQUIESCE,	0, 1,	"SCSI bus unquiesce failed"},
{ERR_BUS_CONFIGURE,	0, 1,	"failed to configure devices on SCSI bus"},
{ERR_BUS_UNCONFIGURE,	0, 1,	"failed to unconfigure SCSI bus"},
{ERR_DEV_CONFIGURE,	0, 1,	"failed to configure SCSI device"},
{ERR_DEV_UNCONFIGURE,	0, 1,	"failed to unconfigure SCSI device"},
{ERR_DEV_REMOVE,	0, 1,	"remove operation failed"},
{ERR_DEV_REPLACE,	0, 1,	"replace operation failed"},
{ERR_DEV_INSERT,	0, 1,	"insert operation failed"},
{ERR_DEV_GETSTATE,	0, 1,	"failed to get state for SCSI device"},
{ERR_RESET,		0, 1,	"reset failed"},
{ERR_LIST,		0, 1,	"list operation failed"},
{ERR_SIG_STATE,		0, 1,	"could not restore signal disposition"},
{ERR_MAYBE_BUSY,	0, 1,	"device may be busy"},
{ERR_BUS_DEV_MISMATCH,	0, 1,	"mismatched SCSI bus and device"},

/* Errors with arguments */
{ERRARG_OPT_INVAL,	1, 1,	"invalid option: "},
{ERRARG_HWCMD_INVAL,	1, 1,	"invalid command: "},
{ERRARG_DEVINFO,	1, 1,	"libdevinfo failed on path: "},

/* RCM Errors */
{ERR_RCM_HANDLE,	0, 1,	"cannot get RCM handle"},
{ERRARG_RCM_SUSPEND,	1, 1,	"failed to suspend: "},
{ERRARG_RCM_RESUME,	1, 1,	"failed to resume: "},
{ERRARG_RCM_OFFLINE,	1, 1,	"failed to offline: "},
{ERRARG_RCM_ONLINE,	1, 1,	"failed to online: "},
{ERRARG_RCM_REMOVE,	1, 1,	"failed to remove: "},
{ERRARG_RCM_INFO,	1, 1,	"failed to query: "},

/* Commands */
{CMD_INSERT_DEV,	0, 0,	"insert_device"},
{CMD_REMOVE_DEV,	0, 0,	"remove_device"},
{CMD_REPLACE_DEV,	0, 0,	"replace_device"},
{CMD_RESET_DEV,		0, 0,	"reset_device"},
{CMD_RESET_BUS,		0, 0,	"reset_bus"},
{CMD_RESET_ALL,		0, 0,	"reset_all"},

/* help messages */
{MSG_HELP_HDR,		0, 1,	"\nSCSI specific commands and options:\n"},
{MSG_HELP_USAGE,	0, 0,	"\t-x insert_device ap_id [ap_id... ]\n"
				"\t-x remove_device ap_id [ap_id... ]\n"
				"\t-x replace_device ap_id [ap_id... ]\n"
				"\t-x reset_device ap_id [ap_id... ]\n"
				"\t-x reset_bus ap_id [ap_id... ]\n"
				"\t-x reset_all ap_id [ap_id... ]\n"},

/* hotplug messages */
{MSG_INSDEV,		1, 1,	"Adding device to SCSI HBA: "},
{MSG_RMDEV,		1, 1,	"Removing SCSI device: "},
{MSG_REPLDEV,		1, 1,	"Replacing SCSI device: "},

/* Hotplugging confirmation prompts */
{CONF_QUIESCE_1,	1, 1,
	"This operation will suspend activity on SCSI bus: "},

{CONF_QUIESCE_2,	0, 1,	"\nContinue"},

{CONF_UNQUIESCE,	0, 1,
	"SCSI bus quiesced successfully.\n"
	"It is now safe to proceed with hotplug operation."
	"\nEnter y if operation is complete or n to abort"},

/* Misc. */
{WARN_DISCONNECT,	0, 1,
	"WARNING: Disconnecting critical partitions may cause system hang."
	"\nContinue"}
};


#define	N_STRS	(sizeof (str_tbl) / sizeof (str_tbl[0]))

#define	GET_MSG_NARGS(i)	(str_tbl[msg_idx(i)].nargs)
#define	GET_MSG_INTL(i)		(str_tbl[msg_idx(i)].intl)

static errcvt_t err_cvt_tbl[] = {
	{ SCFGA_OK,		CFGA_OK			},
	{ SCFGA_LIB_ERR,	CFGA_LIB_ERROR		},
	{ SCFGA_APID_NOEXIST,	CFGA_APID_NOEXIST	},
	{ SCFGA_NACK,		CFGA_NACK		},
	{ SCFGA_BUSY,		CFGA_BUSY		},
	{ SCFGA_SYSTEM_BUSY,	CFGA_SYSTEM_BUSY	},
	{ SCFGA_OPNOTSUPP,	CFGA_OPNOTSUPP		},
	{ SCFGA_PRIV,		CFGA_PRIV		},
	{ SCFGA_UNKNOWN_ERR,	CFGA_ERROR		},
	{ SCFGA_ERR,		CFGA_ERROR		}
};

#define	N_ERR_CVT_TBL	(sizeof (err_cvt_tbl)/sizeof (err_cvt_tbl[0]))

#define	DEV_OP	0
#define	BUS_OP	1
static set_state_cmd_t set_state_cmds[] = {

{ SCFGA_BUS_QUIESCE,		BUS_OP,		devctl_bus_quiesce	},
{ SCFGA_BUS_UNQUIESCE,		BUS_OP,		devctl_bus_unquiesce	},
{ SCFGA_BUS_CONFIGURE,		BUS_OP,		devctl_bus_configure	},
{ SCFGA_BUS_UNCONFIGURE, 	BUS_OP,		devctl_bus_unconfigure	},
{ SCFGA_RESET_BUS,		BUS_OP,		devctl_bus_reset	},
{ SCFGA_RESET_ALL, 		BUS_OP,		devctl_bus_resetall	},
{ SCFGA_DEV_CONFIGURE,		DEV_OP,		devctl_device_online	},
{ SCFGA_DEV_UNCONFIGURE,	DEV_OP,		devctl_device_offline	},
{ SCFGA_DEV_REMOVE,		DEV_OP,		devctl_device_remove	},
{ SCFGA_RESET_DEV,		DEV_OP,		devctl_device_reset	}

};

#define	N_SET_STATE_CMDS (sizeof (set_state_cmds)/sizeof (set_state_cmds[0]))

static get_state_cmd_t get_state_cmds[] = {
{ SCFGA_BUS_GETSTATE,		BUS_OP,		devctl_bus_getstate	},
{ SCFGA_DEV_GETSTATE,		DEV_OP,		devctl_device_getstate	}
};

#define	N_GET_STATE_CMDS (sizeof (get_state_cmds)/sizeof (get_state_cmds[0]))

/* Order is important. Earlier directories are searched first */
static const char *dev_dir_hints[] = {
	CFGA_DEV_DIR,
	DEV_RMT,
	DEV_DSK,
	DEV_RDSK,
	DEV_DIR
};

#define	N_DEV_DIR_HINTS	(sizeof (dev_dir_hints) / sizeof (dev_dir_hints[0]))

/*
 * SCSI hardware specific commands
 */
static hw_cmd_t hw_cmds[] = {
	/* Command string	Command ID		Function	*/

	{ CMD_INSERT_DEV,	SCFGA_INSERT_DEV,	dev_insert	},
	{ CMD_REMOVE_DEV,	SCFGA_REMOVE_DEV,	dev_remove	},
	{ CMD_REPLACE_DEV,	SCFGA_REPLACE_DEV,	dev_replace	},
	{ CMD_RESET_DEV,	SCFGA_RESET_DEV,	reset_common	},
	{ CMD_RESET_BUS,	SCFGA_RESET_BUS,	reset_common	},
	{ CMD_RESET_ALL,	SCFGA_RESET_ALL,	reset_common	},
};
#define	N_HW_CMDS (sizeof (hw_cmds) / sizeof (hw_cmds[0]))


/*
 * Routine to search the /dev directory or a subtree of /dev.
 * If the entire /dev hierarchy is to be searched, the most likely directories
 * are searched first.
 */
scfga_ret_t
recurse_dev(
	const char	*basedir,
	void		*arg,
	scfga_recur_t (*fcn)(const char *lpath, void *arg))
{
	int i, rv = NFTW_ERROR;

	(void) mutex_lock(&nftw_arg.mp);

	nftw_arg.arg = arg;
	nftw_arg.fcn = fcn;

	if (strcmp(basedir, DEV_DIR)) {
		errno = 0;
		rv = nftw(basedir, do_recurse_dev, NFTW_DEPTH, FTW_PHYS);
		goto out;
	}

	/*
	 * Search certain selected subdirectories first if basedir == "/dev".
	 * Ignore errors as some of these directories may not exist.
	 */
	for (i = 0; i < N_DEV_DIR_HINTS; i++) {
		errno = 0;
		if ((rv = nftw(dev_dir_hints[i], do_recurse_dev, NFTW_DEPTH,
		    FTW_PHYS)) == NFTW_TERMINATE) {
			break;
		}
	}

	/*FALLTHRU*/
out:
	(void) mutex_unlock(&nftw_arg.mp);
	return (rv == NFTW_ERROR ? SCFGA_ERR : SCFGA_OK);
}

/*ARGSUSED*/
static int
do_recurse_dev(
	const char *path,
	const struct stat *sbuf,
	int type,
	struct FTW *ftwp)
{
	/* We want only VALID symlinks */
	if (type != FTW_SL) {
		return (NFTW_CONTINUE);
	}

	assert(nftw_arg.fcn != NULL);

	if (nftw_arg.fcn(path, nftw_arg.arg) == SCFGA_TERMINATE) {
		/* terminate prematurely, but may not be error */
		errno = 0;
		return (NFTW_TERMINATE);
	} else {
		return (NFTW_CONTINUE);
	}
}

cfga_err_t
err_cvt(scfga_ret_t s_err)
{
	int i;

	for (i = 0; i < N_ERR_CVT_TBL; i++) {
		if (err_cvt_tbl[i].scsi_err == s_err) {
			return (err_cvt_tbl[i].cfga_err);
		}
	}

	return (CFGA_ERROR);
}

/*
 * Removes duplicate slashes from a pathname and any trailing slashes.
 * Returns "/" if input is "/"
 */
static char *
pathdup(const char *path, int *l_errnop)
{
	int prev_was_slash = 0;
	char c, *dp = NULL, *dup = NULL;
	const char *sp = NULL;

	*l_errnop = 0;

	if (path == NULL) {
		return (NULL);
	}

	if ((dup = calloc(1, strlen(path) + 1)) == NULL) {
		*l_errnop = errno;
		return (NULL);
	}

	prev_was_slash = 0;
	for (sp = path, dp = dup; (c = *sp) != '\0'; sp++) {
		if (!prev_was_slash || c != '/') {
			*dp++ = c;
		}
		if (c == '/') {
			prev_was_slash = 1;
		} else {
			prev_was_slash = 0;
		}
	}

	/* Remove trailing slash except if it is the first char */
	if (prev_was_slash && dp != dup && dp - 1 != dup) {
		*(--dp) = '\0';
	} else {
		*dp = '\0';
	}

	return (dup);
}

scfga_ret_t
apidt_create(const char *ap_id, apid_t *apidp, char **errstring)
{
	char *hba_phys = NULL, *dyn = NULL;
	char *dyncomp = NULL, *path = NULL;
	int l_errno = 0;
	size_t len = 0;
	scfga_ret_t ret;

	if ((hba_phys = pathdup(ap_id, &l_errno)) == NULL) {
		cfga_err(errstring, l_errno, ERR_OP_FAILED, 0);
		return (SCFGA_LIB_ERR);
	}

	/* Extract the base(hba) and dynamic(device) component if any */
	dyncomp = NULL;
	if ((dyn = GET_DYN(hba_phys)) != NULL) {
		len = strlen(DYN_TO_DYNCOMP(dyn)) + 1;
		dyncomp = calloc(1, len);
		if (dyncomp == NULL) {
			cfga_err(errstring, errno, ERR_OP_FAILED, 0);
			ret = SCFGA_LIB_ERR;
			goto err;
		}
		(void) strcpy(dyncomp, DYN_TO_DYNCOMP(dyn));

		/* Remove the dynamic component from the base */
		*dyn = '\0';
	}

	/* Create the path */
	if ((ret = apid_to_path(hba_phys, dyncomp, &path, &l_errno))
	    != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_OP_FAILED, 0);
		goto err;
	}

	assert(path != NULL);
	assert(hba_phys != NULL);

	apidp->hba_phys = hba_phys;
	apidp->dyncomp = dyncomp;
	apidp->path = path;
	apidp->flags = 0;

	return (SCFGA_OK);

err:
	S_FREE(hba_phys);
	S_FREE(dyncomp);
	S_FREE(path);
	return (ret);
}

void
apidt_free(apid_t *apidp)
{
	if (apidp == NULL)
		return;

	S_FREE(apidp->hba_phys);
	S_FREE(apidp->dyncomp);
	S_FREE(apidp->path);
}

scfga_ret_t
walk_tree(
	const char	*physpath,
	void		*arg,
	uint_t		init_flags,
	walkarg_t	*up,
	scfga_cmd_t	cmd,
	int		*l_errnop)
{
	int rv;
	di_node_t root;
	char *root_path, *cp = NULL;
	size_t len;
	scfga_ret_t ret;

	*l_errnop = 0;

	if ((root_path = strdup(physpath)) == NULL) {
		*l_errnop = errno;
		return (SCFGA_LIB_ERR);
	}

	/* Fix up path for di_init() */
	len = strlen(DEVICES_DIR);
	if (strncmp(root_path, DEVICES_DIR SLASH,
	    len + strlen(SLASH)) == 0) {
		cp = root_path + len;
		(void) memmove(root_path, cp, strlen(cp) + 1);
	} else if (*root_path != '/') {
		*l_errnop = 0;
		ret = SCFGA_ERR;
		goto out;
	}

	/* Remove dynamic component if any */
	if ((cp = GET_DYN(root_path)) != NULL) {
		*cp = '\0';
	}

	/* Remove minor name if any */
	if ((cp = strrchr(root_path, ':')) != NULL) {
		*cp = '\0';
	}

	/* Get a snapshot */
	if ((root = di_init(root_path, init_flags)) == DI_NODE_NIL) {
		*l_errnop = errno;
		ret = SCFGA_LIB_ERR;
		goto out;
	}

	/* Walk the tree */
	errno = 0;
	if (cmd == SCFGA_WALK_NODE) {
		rv = di_walk_node(root, up->node_args.flags, arg,
		    up->node_args.fcn);
	} else {
		assert(cmd == SCFGA_WALK_MINOR);
		rv = di_walk_minor(root, up->minor_args.nodetype, 0, arg,
		    up->minor_args.fcn);
	}

	if (rv != 0) {
		*l_errnop = errno;
		ret = SCFGA_LIB_ERR;
	} else {
		*l_errnop = 0;
		ret = SCFGA_OK;
	}

	di_fini(root);

	/*FALLTHRU*/
out:
	S_FREE(root_path);
	return (ret);
}

scfga_ret_t
invoke_cmd(const char *func, apid_t *apidtp, prompt_t *prp, char **errstring)
{
	int i;

	for (i = 0; i < N_HW_CMDS; i++) {
		if (strcmp(func, GET_MSG_STR(hw_cmds[i].str_id)) == 0) {
			return (hw_cmds[i].fcn(hw_cmds[i].cmd, apidtp,
			    prp, errstring));
		}
	}

	cfga_err(errstring, 0, ERRARG_HWCMD_INVAL, func, 0);
	return (SCFGA_ERR);
}

int
msg_idx(msgid_t msgid)
{
	int idx = 0;

	/* The string table index and the error id may or may not be same */
	if (msgid >= 0 && msgid <= N_STRS - 1 &&
	    str_tbl[msgid].msgid == msgid) {
		idx = msgid;
	} else {
		for (idx = 0; idx < N_STRS; idx++) {
			if (str_tbl[idx].msgid == msgid)
				break;
		}
		if (idx >= N_STRS) {
			idx =  UNKNOWN_ERR_IDX;
		}
	}

	return (idx);
}

/*
 * cfga_err() accepts a variable number of message IDs and constructs
 * a corresponding error string which is returned via the errstring argument.
 * cfga_err() calls dgettext() to internationalize proper messages.
 * May be called with a NULL argument.
 */
void
cfga_err(char **errstring, int l_errno, ...)
{
	va_list ap;
	int append_newline = 0;

	if (errstring == NULL || *errstring != NULL) {
		return;
	}

	/*
	 * Don't append a newline, the application (for example cfgadm)
	 * should do that.
	 */
	append_newline = 0;

	va_start(ap, l_errno);
	msg_common(errstring, append_newline, l_errno, ap);
	va_end(ap);
}

/*
 * This routine accepts a variable number of message IDs and constructs
 * a corresponding message string which is printed via the message print
 * routine argument.
 */
void
cfga_msg(struct cfga_msg *msgp, ...)
{
	char *p = NULL;
	int append_newline = 0, l_errno = 0;
	va_list ap;

	if (msgp == NULL || msgp->message_routine == NULL) {
		return;
	}

	/* Append a newline after message */
	append_newline = 1;
	l_errno = 0;

	va_start(ap, msgp);
	msg_common(&p, append_newline, l_errno, ap);
	va_end(ap);

	(void) (*msgp->message_routine)(msgp->appdata_ptr, p);

	S_FREE(p);
}

/*
 * Get internationalized string corresponding to message id
 * Caller must free the memory allocated.
 */
char *
cfga_str(int append_newline, ...)
{
	char *p = NULL;
	int l_errno = 0;
	va_list ap;

	va_start(ap, append_newline);
	msg_common(&p, append_newline, l_errno, ap);
	va_end(ap);

	return (p);
}

static void
msg_common(char **msgpp, int append_newline, int l_errno, va_list ap)
{
	int a = 0;
	size_t len = 0;
	int i = 0, n = 0;
	char *s = NULL, *t = NULL;
	strlist_t dummy;
	strlist_t *savep = NULL, *sp = NULL, *tailp = NULL;

	if (*msgpp != NULL) {
		return;
	}

	dummy.next = NULL;
	tailp = &dummy;
	for (len = 0; (a = va_arg(ap, int)) != 0; ) {
		n = GET_MSG_NARGS(a); /* 0 implies no additional args */
		for (i = 0; i <= n; i++) {
			sp = calloc(1, sizeof (*sp));
			if (sp == NULL) {
				goto out;
			}
			if (i == 0 && GET_MSG_INTL(a)) {
				sp->str = dgettext(TEXT_DOMAIN, GET_MSG_STR(a));
			} else if (i == 0) {
				sp->str = GET_MSG_STR(a);
			} else {
				sp->str = va_arg(ap, char *);
			}
			len += (strlen(sp->str));
			sp->next = NULL;
			tailp->next = sp;
			tailp = sp;
		}
	}

	len += 1;	/* terminating NULL */

	s = t = NULL;
	if (l_errno) {
		s = dgettext(TEXT_DOMAIN, ": ");
		t = S_STR(strerror(l_errno));
		if (s != NULL && t != NULL) {
			len += strlen(s) + strlen(t);
		}
	}

	if (append_newline) {
		len++;
	}

	if ((*msgpp = calloc(1, len)) == NULL) {
		goto out;
	}

	**msgpp = '\0';
	for (sp = dummy.next; sp != NULL; sp = sp->next) {
		(void) strcat(*msgpp, sp->str);
	}

	if (s != NULL && t != NULL) {
		(void) strcat(*msgpp, s);
		(void) strcat(*msgpp, t);
	}

	if (append_newline) {
		(void) strcat(*msgpp, dgettext(TEXT_DOMAIN, "\n"));
	}

	/* FALLTHROUGH */
out:
	sp = dummy.next;
	while (sp != NULL) {
		savep = sp->next;
		S_FREE(sp);
		sp = savep;
	}
}

scfga_ret_t
devctl_cmd(
	const char	*physpath,
	scfga_cmd_t	cmd,
	uint_t		*statep,
	int		*l_errnop)
{
	int rv = -1, i, type;
	devctl_hdl_t hdl = NULL;
	char *cp = NULL, *path = NULL;
	int (*func)(const devctl_hdl_t);
	int (*state_func)(const devctl_hdl_t, uint_t *);

	*l_errnop = 0;

	if (statep != NULL) *statep = 0;

	func = NULL;
	state_func = NULL;
	type = 0;

	for (i = 0; i < N_GET_STATE_CMDS; i++) {
		if (get_state_cmds[i].cmd == cmd) {
			state_func = get_state_cmds[i].state_fcn;
			type = get_state_cmds[i].type;
			assert(statep != NULL);
			break;
		}
	}

	if (state_func == NULL) {
		for (i = 0; i < N_SET_STATE_CMDS; i++) {
			if (set_state_cmds[i].cmd == cmd) {
				func = set_state_cmds[i].fcn;
				type = set_state_cmds[i].type;
				assert(statep == NULL);
				break;
			}
		}
	}

	assert(type == BUS_OP || type == DEV_OP);

	if (func == NULL && state_func == NULL) {
		return (SCFGA_ERR);
	}

	/*
	 * Fix up path for calling devctl.
	 */
	if ((path = strdup(physpath)) == NULL) {
		*l_errnop = errno;
		return (SCFGA_LIB_ERR);
	}

	/* Remove dynamic component if any */
	if ((cp = GET_DYN(path)) != NULL) {
		*cp = '\0';
	}

	/* Remove minor name */
	if ((cp = strrchr(path, ':')) != NULL) {
		*cp = '\0';
	}

	errno = 0;

	if (type == BUS_OP) {
		hdl = devctl_bus_acquire(path, 0);
	} else {
		hdl = devctl_device_acquire(path, 0);
	}
	*l_errnop = errno;

	S_FREE(path);

	if (hdl == NULL) {
		return (SCFGA_ERR);
	}

	errno = 0;
	/* Only getstate functions require a second argument */
	if (func != NULL && statep == NULL) {
		rv = func(hdl);
		*l_errnop = errno;
	} else if (state_func != NULL && statep != NULL) {
		rv = state_func(hdl, statep);
		*l_errnop = errno;
	} else {
		rv = -1;
		*l_errnop = 0;
	}

	devctl_release(hdl);

	return ((rv == -1) ? SCFGA_ERR : SCFGA_OK);
}

/*
 * Is device in a known state ? (One of BUSY, ONLINE, OFFLINE)
 *	BUSY --> One or more device special files are open. Implies online
 *	ONLINE --> driver attached
 *	OFFLINE --> CF1 with offline flag set.
 *	UNKNOWN --> None of the above
 */
int
known_state(di_node_t node)
{
	uint_t state;

	state = di_state(node);

	/*
	 * CF1 without offline flag set is considered unknown state.
	 * We are in a known state if either CF2 (driver attached) or
	 * offline.
	 */
	if ((state & DI_DEVICE_OFFLINE) == DI_DEVICE_OFFLINE ||
		(state & DI_DRIVER_DETACHED) != DI_DRIVER_DETACHED) {
		return (1);
	}

	return (0);
}

void
list_free(ldata_list_t **llpp)
{
	ldata_list_t *lp, *olp;

	lp = *llpp;
	while (lp != NULL) {
		olp = lp;
		lp = olp->next;
		S_FREE(olp);
	}

	*llpp = NULL;
}

/*
 * Obtain the devlink from a /devices path
 */
scfga_ret_t
physpath_to_devlink(
	const char *basedir,
	char *phys,
	char **logpp,
	int *l_errnop,
	int match_minor)
{
	pathm_t pmt = {NULL};
	scfga_ret_t ret;

	pmt.phys = phys;
	pmt.ret = SCFGA_NO_REC;
	pmt.match_minor = match_minor;

	/*
	 * Search the /dev hierarchy starting at basedir.
	 */
	ret = recurse_dev(basedir, &pmt, lookup_dev);
	if (ret == SCFGA_OK && (ret = pmt.ret) == SCFGA_OK) {
		assert(pmt.log != NULL);
		*logpp  = pmt.log;
	} else {
		if (pmt.log != NULL) {
			S_FREE(pmt.log);
		}

		*logpp = NULL;
		*l_errnop = pmt.l_errno;
	}

	return (ret);
}

static scfga_recur_t
lookup_dev(const char *lpath, void *arg)
{
	char ppath[PATH_MAX];
	pathm_t *pmtp = (pathm_t *)arg;

	if (realpath(lpath, ppath) == NULL) {
		return (SCFGA_CONTINUE);
	}

	ppath[sizeof (ppath) - 1] = '\0';

	/* Is this the physical path we are looking for */
	if (dev_cmp(ppath, pmtp->phys, pmtp->match_minor))  {
		return (SCFGA_CONTINUE);
	}

	if ((pmtp->log = strdup(lpath)) == NULL) {
		pmtp->l_errno = errno;
		pmtp->ret = SCFGA_LIB_ERR;
	} else {
		pmtp->ret = SCFGA_OK;
	}

	return (SCFGA_TERMINATE);
}

/* Compare HBA physical ap_id and device path */
int
hba_dev_cmp(const char *hba, const char *devpath)
{
	char *cp = NULL;
	int rv;
	size_t hba_len, dev_len;
	char l_hba[MAXPATHLEN], l_dev[MAXPATHLEN];

	(void) snprintf(l_hba, sizeof (l_hba), "%s", hba);
	(void) snprintf(l_dev, sizeof (l_dev), "%s", devpath);

	/* Remove dynamic component if any */
	if ((cp = GET_DYN(l_hba)) != NULL) {
		*cp = '\0';
	}

	if ((cp = GET_DYN(l_dev)) != NULL) {
		*cp = '\0';
	}


	/* Remove minor names */
	if ((cp = strrchr(l_hba, ':')) != NULL) {
		*cp = '\0';
	}

	if ((cp = strrchr(l_dev, ':')) != NULL) {
		*cp = '\0';
	}

	hba_len = strlen(l_hba);
	dev_len = strlen(l_dev);

	/* Check if HBA path is component of device path */
	if (rv = strncmp(l_hba, l_dev, hba_len)) {
		return (rv);
	}

	/* devpath must have '/' and 1 char in addition to hba path */
	if (dev_len >= hba_len + 2 && l_dev[hba_len] == '/') {
		return (0);
	} else {
		return (-1);
	}
}

int
dev_cmp(const char *dev1, const char *dev2, int match_minor)
{
	char l_dev1[MAXPATHLEN], l_dev2[MAXPATHLEN];
	char *mn1, *mn2;
	int rv;

	(void) snprintf(l_dev1, sizeof (l_dev1), "%s", dev1);
	(void) snprintf(l_dev2, sizeof (l_dev2), "%s", dev2);

	if ((mn1 = GET_DYN(l_dev1)) != NULL) {
		*mn1 = '\0';
	}

	if ((mn2 = GET_DYN(l_dev2)) != NULL) {
		*mn2 = '\0';
	}

	/* Separate out the minor names */
	if ((mn1 = strrchr(l_dev1, ':')) != NULL) {
		*mn1++ = '\0';
	}

	if ((mn2 = strrchr(l_dev2, ':')) != NULL) {
		*mn2++ = '\0';
	}

	if ((rv = strcmp(l_dev1, l_dev2)) != 0 || !match_minor) {
		return (rv);
	}

	/*
	 * Compare minor names
	 */
	if (mn1 == NULL && mn2 == NULL) {
		return (0);
	} else if (mn1 == NULL) {
		return (-1);
	} else if (mn2 == NULL) {
		return (1);
	} else {
		return (strcmp(mn1, mn2));
	}
}
