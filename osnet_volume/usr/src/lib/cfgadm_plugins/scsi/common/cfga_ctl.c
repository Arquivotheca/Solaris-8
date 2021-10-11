/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)cfga_ctl.c 1.4	99/11/12 SMI"

#include "cfga_scsi.h"

typedef struct {
	mutex_t		mp;
	sig_atomic_t	intr_hint;
	sig_atomic_t	inited;
	sigjmp_buf	sjmpbuf;
	sigset_t	catch_set;	/* set of signals to be caught */
	struct sigaction *sigactp;
} sigstate_t;

#define	ETC_VFSTAB	"/etc/vfstab"

/*
 * Range of valid signals: from 1 to _sys_nsig - 1
 */
#define	SCFGA_NSIGS	_sys_nsig

/* Function prototypes */

static scfga_ret_t quiesce_confirm(apid_t *apidp,
    msgid_t cmd_msg, prompt_t *prp, int *okp, int *l_errnop);
static scfga_ret_t dev_hotplug(apid_t *apidp,
    prompt_t *prp, char **errstring);
static scfga_ret_t init_sigstate(sigstate_t *sigstatep,
    char **errstring);
static scfga_ret_t change_disp(sigstate_t *sp, char **errstring);
static scfga_ret_t restore_disp(sigstate_t *sp, char **errstring);
static int disconnect(struct cfga_confirm *confp);
static int critical_ctrlr(const char *hba_phys);
static cfga_stat_t bus_devctl_to_recep_state(uint_t bus_dc_state);


/* Globals */
static int unhandled_sigs[] = {
	SIGKILL,
	SIGSTOP,
	SIGWAITING,
	SIGCANCEL,
	SIGLWP,
	SIGCHLD		/* generated after the RCM daemon is started */
};

#define	N_NOCATCH_SIGS	(sizeof (unhandled_sigs)/sizeof (unhandled_sigs[0]))

sigstate_t sigstate = {DEFAULTMUTEX};

/*ARGSUSED*/
scfga_ret_t
bus_change_state(
	cfga_cmd_t state_change_cmd,
	apid_t *apidp,
	struct cfga_confirm *confp,
	cfga_flags_t flags,
	char **errstring)
{
	int l_errno = 0, force;
	uint_t state = 0;
	cfga_stat_t bus_state;
	scfga_cmd_t cmd;
	msgid_t errid;
	cfga_stat_t prereq;
	scfga_ret_t ret;

	assert(apidp->path != NULL);
	assert(apidp->hba_phys != NULL);

	/*
	 * No dynamic components allowed
	 */
	if (apidp->dyncomp != NULL) {
		cfga_err(errstring, 0, ERR_NOT_BUSAPID, 0);
		return (SCFGA_ERR);
	}

	/* Get bus state */
	if (devctl_cmd(apidp->path, SCFGA_BUS_GETSTATE, &state,
	    &l_errno) != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_BUS_GETSTATE, 0);
		return (SCFGA_ERR);
	}

	bus_state = bus_devctl_to_recep_state(state);
	force = ((flags & CFGA_FLAG_FORCE) == CFGA_FLAG_FORCE) ? 1 : 0;
	assert(confp->confirm != NULL);

	switch (state_change_cmd) {
	case CFGA_CMD_DISCONNECT:	/* quiesce bus */
		/*
		 * If force flag not specified, check if controller is
		 * critical.
		 */
		if (!force) {
			/*
			 * This check is not foolproof, get user confirmation
			 * if test passes.
			 */
			if (critical_ctrlr(apidp->path)) {
				cfga_err(errstring, 0, ERR_CTRLR_CRIT, 0);
				ret = SCFGA_ERR;
				break;
			} else if (!disconnect(confp)) {
				ret = SCFGA_NACK;
				break;
			}
		}

		cmd = SCFGA_BUS_QUIESCE;
		errid = ERR_BUS_QUIESCE;
		prereq = CFGA_STAT_CONNECTED;

		goto common;

	case CFGA_CMD_CONNECT:		/* unquiesce bus */
		cmd = SCFGA_BUS_UNQUIESCE;
		errid = ERR_BUS_UNQUIESCE;
		prereq = CFGA_STAT_DISCONNECTED;

		goto common;

	case CFGA_CMD_CONFIGURE:
		cmd = SCFGA_BUS_CONFIGURE;
		errid = ERR_BUS_CONFIGURE;
		prereq = CFGA_STAT_CONNECTED;

		goto common;

	case CFGA_CMD_UNCONFIGURE:
		cmd = SCFGA_BUS_UNCONFIGURE;
		errid = ERR_BUS_UNCONFIGURE;
		prereq = CFGA_STAT_CONNECTED;

		/* FALLTHROUGH */
	common:
		if (bus_state != prereq) {
			cfga_err(errstring, 0,
			    (prereq == CFGA_STAT_CONNECTED)
			    ? ERR_BUS_NOTCONNECTED
			    : ERR_BUS_CONNECTED, 0);
			ret = SCFGA_ERR;
			break;
		}

		/*
		 * When quiescing or unconfiguring a bus, first suspend or
		 * offline it through RCM.
		 */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
			if (cmd == SCFGA_BUS_QUIESCE) {
				if ((ret = scsi_rcm_suspend(apidp->path, NULL,
				    errstring, flags)) != SCFGA_OK) {
					break;
				}
			} else if (cmd == SCFGA_BUS_UNCONFIGURE) {
				if ((ret = scsi_rcm_offline(apidp->path,
				    errstring, flags)) != SCFGA_OK) {
					break;
				}
			}
		}

		ret = devctl_cmd(apidp->path, cmd, NULL, &l_errno);
		if (ret != SCFGA_OK) {
			/*
			 * EIO when child devices are busy may confuse user.
			 * So explain it.
			 */
			if (cmd == SCFGA_BUS_UNCONFIGURE && l_errno == EIO)
				errid = ERR_MAYBE_BUSY;

			cfga_err(errstring, l_errno, errid, 0);

			/*
			 * If the bus was suspended in RCM, then cancel the RCM
			 * operation.  Discard RCM failures here because the
			 * devctl's failure is what is most relevant.
			 */
			if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
				if (cmd == SCFGA_BUS_QUIESCE)
					(void) scsi_rcm_resume(apidp->path,
					    NULL, errstring,
					    (flags & (~CFGA_FLAG_FORCE)));
			}

			break;
		}

		/*
		 * When unquiescing or configuring a bus, resume or online it
		 * in RCM when the devctl command is complete.
		 */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
			if (cmd == SCFGA_BUS_UNQUIESCE) {
				ret = scsi_rcm_resume(apidp->path, NULL,
				    errstring, (flags & (~CFGA_FLAG_FORCE)));
			} else if (cmd == SCFGA_BUS_CONFIGURE) {
				ret = scsi_rcm_online(apidp->path, errstring,
				    flags);
			}
		}
		break;

	case CFGA_CMD_LOAD:
	case CFGA_CMD_UNLOAD:
		ret = SCFGA_OPNOTSUPP;
		break;

	default:
		cfga_err(errstring, 0, ERR_CMD_INVAL, 0);
		ret = SCFGA_ERR;
		break;
	}

	return (ret);
}

scfga_ret_t
dev_change_state(
	cfga_cmd_t state_change_cmd,
	apid_t *apidp,
	cfga_flags_t flags,
	char **errstring)
{
	uint_t state = 0;
	int l_errno = 0;
	cfga_stat_t bus_state;
	scfga_cmd_t cmd;
	msgid_t errid;
	scfga_ret_t ret;


	assert(apidp->path != NULL);
	assert(apidp->hba_phys != NULL);


	/*
	 * For a device, dynamic component must be present
	 */
	if (apidp->dyncomp == NULL) {
		cfga_err(errstring, 0, ERR_APID_INVAL, 0);
		return (SCFGA_ERR);
	}

	/* Get bus state */
	if (devctl_cmd(apidp->hba_phys, SCFGA_BUS_GETSTATE, &state,
	    &l_errno) != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_BUS_GETSTATE, 0);
		return (SCFGA_ERR);
	}

	bus_state = bus_devctl_to_recep_state(state);

	switch (state_change_cmd) {
	case CFGA_CMD_CONFIGURE:		/* online device */
		cmd = SCFGA_DEV_CONFIGURE;
		errid = ERR_DEV_CONFIGURE;
		goto common;

	case CFGA_CMD_UNCONFIGURE:		/* offline device */
		cmd = SCFGA_DEV_UNCONFIGURE;
		errid = ERR_DEV_UNCONFIGURE;
		/* FALLTHROUGH */
	common:
		if (bus_state != CFGA_STAT_CONNECTED) {
			cfga_err(errstring, 0, ERR_BUS_NOTCONNECTED, 0);
			ret = SCFGA_ERR;
			break;
		}

		/* When unconfiguring a device, first offline it through RCM. */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
			if (cmd == SCFGA_DEV_UNCONFIGURE) {
				if ((ret = scsi_rcm_offline(apidp->path,
				    errstring, flags)) != SCFGA_OK) {
					break;
				}
			}
		}

		ret = devctl_cmd(apidp->path, cmd, NULL, &l_errno);
		if (ret != SCFGA_OK) {
			cfga_err(errstring, l_errno, errid, 0);

			/*
			 * If an unconfigure fails, cancel the RCM offline.
			 * Discard any RCM failures so that the devctl
			 * failure will still be reported.
			 */
			if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
				if (cmd == SCFGA_DEV_UNCONFIGURE)
					(void) scsi_rcm_online(apidp->path,
					    errstring, flags);
			}
			break;
		}

		/* When configuring a device, online it through RCM. */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
			if (cmd == SCFGA_DEV_CONFIGURE) {
				ret = scsi_rcm_online(apidp->path, errstring,
				    flags);
			}
		}
		break;

	/*
	 * Cannot disconnect/connect individual devices without affecting
	 * other devices on the bus. So we don't support these ops.
	 */
	case CFGA_CMD_DISCONNECT:
	case CFGA_CMD_CONNECT:
		cfga_err(errstring, 0, ERR_NOT_DEVOP, 0);
		ret = SCFGA_ERR;
		break;
	case CFGA_CMD_LOAD:
	case CFGA_CMD_UNLOAD:
		ret = SCFGA_OPNOTSUPP;
		break;
	default:
		cfga_err(errstring, 0, ERR_CMD_INVAL, 0);
		ret = SCFGA_ERR;
		break;
	}

	return (ret);
}

/*ARGSUSED*/
scfga_ret_t
dev_remove(
	scfga_cmd_t cmd,
	apid_t *apidp,
	prompt_t *prp,
	char **errstring)
{
	int proceed, l_errno = 0;
	scfga_ret_t ret;


	assert(apidp->hba_phys != NULL);
	assert(apidp->path != NULL);

	/* device operation only */
	if (apidp->dyncomp == NULL) {
		cfga_err(errstring, 0, ERR_NOT_BUSOP, 0);
		return (SCFGA_ERR);
	}

	proceed = 0;
	ret = quiesce_confirm(apidp, MSG_RMDEV, prp, &proceed, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_DEV_REMOVE, 0);
		return (ret);
	}

	if (!proceed) {
		return (SCFGA_NACK);
	}

	/*
	 * Offline the device in RCM
	 */
	if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
		if ((ret = scsi_rcm_offline(apidp->path, errstring, 0))
		    != SCFGA_OK)
			return (ret);
	}

	/*
	 * Offline the device
	 */
	ret = devctl_cmd(apidp->path, SCFGA_DEV_UNCONFIGURE, NULL, &l_errno);
	if (ret != SCFGA_OK) {

		cfga_err(errstring, l_errno, ERR_DEV_REMOVE, 0);

		/*
		 * Cancel the RCM offline.  Discard the RCM failures so that
		 * the above devctl failure is still reported.
		 */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0)
			(void) scsi_rcm_online(apidp->path, errstring, 0);

		return (ret);
	}

	/* Do the physical removal */
	ret = dev_hotplug(apidp, prp, errstring);

	if (ret == SCFGA_OK) {
		/*
		 * Complete the remove.
		 */
		ret = devctl_cmd(apidp->path, SCFGA_DEV_REMOVE,
		    NULL, &l_errno);
		if (((apidp->flags & FLAG_DISABLE_RCM) == 0) && ret == SCFGA_OK)
			ret = scsi_rcm_remove(apidp->path, errstring, 0);
	} else {
		/*
		 * Restore the device's state in RCM if it wasn't removed.  If
		 * there are any RCM failures, discard them so that the above
		 * dev_hotplug failure is still reported.
		 */
		if (devctl_cmd(apidp->path, SCFGA_DEV_CONFIGURE, NULL, &l_errno)
		    == SCFGA_OK) {
			if ((apidp->flags & FLAG_DISABLE_RCM) == 0)
				(void) scsi_rcm_online(apidp->path, errstring,
				    0);
		}
	}

	return (ret);
}

/*ARGSUSED*/
scfga_ret_t
dev_insert(
	scfga_cmd_t cmd,
	apid_t *apidp,
	prompt_t *prp,
	char **errstring)
{
	int proceed, l_errno = 0;
	scfga_ret_t ret;

	assert(apidp->hba_phys != NULL);
	assert(apidp->path != NULL);

	/* Currently, insert operation only allowed for bus */
	if (apidp->dyncomp != NULL) {
		cfga_err(errstring, 0, ERR_NOT_DEVOP, 0);
		return (SCFGA_ERR);
	}

	proceed = 0;
	ret = quiesce_confirm(apidp, MSG_INSDEV, prp, &proceed, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_DEV_INSERT, 0);
		return (ret);
	}

	if (!proceed) {
		return (SCFGA_NACK);
	}

	/* Do the physical addition */
	ret = dev_hotplug(apidp, prp, errstring);
	if (ret != SCFGA_OK) {
		return (ret);
	}

	/*
	 * Configure bus to online new device(s).
	 * Previously offlined devices will not be onlined.
	 */
	ret = devctl_cmd(apidp->hba_phys, SCFGA_BUS_CONFIGURE, NULL, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_DEV_INSERT, 0);
		return (SCFGA_ERR);
	}

	return (SCFGA_OK);
}

/*ARGSUSED*/
scfga_ret_t
dev_replace(
	scfga_cmd_t cmd,
	apid_t *apidp,
	prompt_t *prp,
	char **errstring)
{
	int proceed, l_errno = 0;
	scfga_ret_t ret, ret2;

	assert(apidp->hba_phys != NULL);
	assert(apidp->path != NULL);

	/* device operation only */
	if (apidp->dyncomp == NULL) {
		cfga_err(errstring, 0, ERR_NOT_BUSOP, 0);
		return (SCFGA_ERR);
	}

	proceed = 0;
	ret = quiesce_confirm(apidp, MSG_REPLDEV, prp, &proceed, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_DEV_REPLACE, 0);
		return (ret);
	}

	if (!proceed) {
		return (SCFGA_NACK);
	}

	/* Offline the device in RCM */
	if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
		if ((ret = scsi_rcm_offline(apidp->path, errstring, 0))
		    != SCFGA_OK)
			return (ret);
	}

	ret = devctl_cmd(apidp->path, SCFGA_DEV_UNCONFIGURE, NULL, &l_errno);
	if (ret != SCFGA_OK) {

		/*
		 * Cancel the RCM offline.  Discard any RCM failures so that
		 * the devctl failure can still be reported.
		 */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0)
			(void) scsi_rcm_online(apidp->path, errstring, 0);

		cfga_err(errstring, l_errno, ERR_DEV_REPLACE, 0);

		return (ret);
	}

	ret = dev_hotplug(apidp, prp, errstring);

	/* Online the replacement, or restore state on error */
	ret2 = devctl_cmd(apidp->path, SCFGA_DEV_CONFIGURE, NULL, &l_errno);

	if (ret2 != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_DEV_REPLACE, 0);
	}

	/*
	 * Remove the replaced device in RCM, or online the device in RCM
	 * to recover.
	 */
	if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
		if (ret == SCFGA_OK)
			ret = scsi_rcm_remove(apidp->path, errstring, 0);
		else if (ret2 == SCFGA_OK)
			ret2 = scsi_rcm_online(apidp->path, errstring, 0);
	}

	return (ret == SCFGA_OK ? ret2 : ret);
}

/*ARGSUSED*/
scfga_ret_t
reset_common(
	scfga_cmd_t cmd,
	apid_t *apidp,
	prompt_t *prp,
	char **errstring)
{
	int l_errno = 0;
	scfga_ret_t ret;


	assert(apidp->path != NULL);
	assert(apidp->hba_phys != NULL);

	switch (cmd) {
	case SCFGA_RESET_DEV:
		if (apidp->dyncomp == NULL) {
			cfga_err(errstring, 0, ERR_NOT_BUSOP, 0);
			return (SCFGA_ERR);
		}
		break;

	case SCFGA_RESET_BUS:
	case SCFGA_RESET_ALL:
		if (apidp->dyncomp != NULL) {
			cfga_err(errstring, 0, ERR_NOT_DEVOP, 0);
			return (SCFGA_ERR);
		}
		break;
	default:
		cfga_err(errstring, 0, ERR_CMD_INVAL, 0);
		return (SCFGA_ERR);
	}

	ret = devctl_cmd(apidp->path, cmd, NULL, &l_errno);
	if (ret != SCFGA_OK) {
		cfga_err(errstring, l_errno, ERR_RESET, 0);
	}

	return (ret);
}

static int
disconnect(struct cfga_confirm *confp)
{
	int ans, append_newline;
	char *cq;

	append_newline = 0;
	cq = cfga_str(append_newline, WARN_DISCONNECT, 0);

	ans = confp->confirm(confp->appdata_ptr, cq);

	S_FREE(cq);

	return (ans == 1);
}

static scfga_ret_t
quiesce_confirm(
	apid_t *apidp,
	msgid_t cmd_msg,
	prompt_t *prp,
	int *okp,
	int *l_errnop)
{
	char *buf = NULL, *hbap = NULL, *cq1 = NULL, *cq2 = NULL;
	char *cp;
	size_t len = 0;
	int i = 0, append_newline;
	scfga_ret_t ret;

	assert(apidp->path != NULL);
	assert(apidp->hba_phys != NULL);

	/*
	 * Try to create HBA logical ap_id.
	 * If that fails use physical path
	 */
	ret = make_hba_logid(apidp->hba_phys, &hbap, &i);
	if (ret != SCFGA_OK) {
		if ((hbap = strdup(apidp->hba_phys)) == NULL) {
			*l_errnop = errno;
			return (SCFGA_LIB_ERR);
		}
		/* If using phys. path, remove minor name */
		if ((cp = strrchr(hbap, ':')) != NULL) {
			*cp = '\0';
		}
	}

	assert(hbap != NULL);

	append_newline = 0;
	cq1 = cfga_str(append_newline, CONF_QUIESCE_1, hbap, 0);
	cq2 = cfga_str(append_newline, CONF_QUIESCE_2, 0);
	len = strlen(cq1) + strlen(cq2) + 1; /* Includes term. NULL */

	if ((buf = calloc(1, len)) == NULL) {
		*l_errnop = errno;
		ret = SCFGA_LIB_ERR;
		S_FREE(cq1);
		S_FREE(cq2);
		goto out;
	}
	(void) strcpy(buf, cq1);
	(void) strcat(buf, cq2);

	S_FREE(cq1);
	S_FREE(cq2);


	/* Remove minor name (if any) from phys path */
	if ((cp = strrchr(apidp->path, ':')) != NULL) {
		*cp = '\0';
	}

	/* describe operation being attempted */
	cfga_msg(prp->msgp, cmd_msg, apidp->path, 0);

	/* Restore minor name */
	if (cp != NULL) {
		*cp = ':';
	}

	/* request permission to quiesce */
	assert(prp->confp != NULL && prp->confp->confirm != NULL);
	*okp = prp->confp->confirm(prp->confp->appdata_ptr, buf);

	ret = SCFGA_OK;
	/*FALLTHRU*/
out:
	S_FREE(buf);
	S_FREE(hbap);
	return (ret);
}

/*ARGSUSED*/
static void
sig_handler(int signum)
{
	if (sigstate.inited) {
		siglongjmp(sigstate.sjmpbuf, signum);
	}
}

static scfga_ret_t
dev_hotplug(
	apid_t *apidp,
	prompt_t *prp,
	char **errstring)
{
	int  l_errno = 0, append_newline = 0;
	char *cp;
	char *cuq = NULL;
	char *bus_path = NULL;
	char *dev_path = NULL;
	scfga_ret_t ret;

	assert(apidp->hba_phys != NULL);
	assert(apidp->path != NULL);

	(void) mutex_lock(&sigstate.mp);

	sigstate.intr_hint = 0;

	if (init_sigstate(&sigstate, errstring) != SCFGA_OK) {
		(void) mutex_unlock(&sigstate.mp);
		return (SCFGA_ERR);
	}

	assert(sigstate.sigactp != NULL);

	/* Save state or return from signal handler */
	if (sigsetjmp(sigstate.sjmpbuf, 1) != 0) {
		/* returning from signal handler */
		ret = SCFGA_NACK;
		goto cleanup;
	}

	/* Jump buffer has been initialized */
	sigstate.inited = 1;

	if (change_disp(&sigstate, errstring) != SCFGA_OK) {
		ret = SCFGA_ERR;
		goto cleanup;
	}

	sigstate.intr_hint = 1; /* If set, MAY indicate an interruption */

	/* Suspend the bus through RCM */
	if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {

		/* The bus_path is the HBA path without its minor */
		if ((bus_path = strdup(apidp->hba_phys)) == NULL) {
			ret = SCFGA_ERR;
			goto cleanup;
		}
		if ((cp = strrchr(bus_path, ':')) != NULL)
			*cp = '\0';

		/*
		 * The dev_path is already initialized to NULL.  If the AP Id
		 * path differs from the HBA path, then the dev_path should
		 * instead be set to the AP Id path without its minor.
		 */
		if (strcmp(apidp->hba_phys, apidp->path) != 0) {
			if ((dev_path = strdup(apidp->path)) == NULL) {
				ret = SCFGA_ERR;
				goto cleanup;
			}
			if ((cp = strrchr(dev_path, ':')) != NULL)
				*cp = '\0';
		}

		if ((ret = scsi_rcm_suspend(bus_path, dev_path, errstring, 0))
		    != SCFGA_OK) {
			sigstate.intr_hint = 0;
			ret = SCFGA_ERR;
			goto cleanup;
		}
	}

	if ((ret = devctl_cmd(apidp->hba_phys, SCFGA_BUS_QUIESCE, NULL,
	    &l_errno)) != SCFGA_OK) {

		/*
		 * If the quiesce failed, then cancel the RCM suspend.  Discard
		 * any RCM failures so that the devctl failure can still be
		 * reported.
		 */
		if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
			(void) scsi_rcm_resume(bus_path, dev_path, errstring,
			    0);
		}

		sigstate.intr_hint = 0;
		cfga_err(errstring, l_errno, ERR_BUS_QUIESCE, 0);
		goto cleanup;
	}

	/* Prompt user and wait for permission to continue */
	append_newline = 0;
	cuq = cfga_str(append_newline, CONF_UNQUIESCE, 0);
	if (prp->confp->confirm(prp->confp->appdata_ptr, cuq) != 1) {
		ret = SCFGA_NACK;
		S_FREE(cuq);
		goto cleanup;
	}
	S_FREE(cuq);
	ret = SCFGA_OK;

	/* FALLTHROUGH */
cleanup:

	sigstate.inited = 0;

	if (sigstate.intr_hint) {
		/*
		 * The unquiesce may fail with EALREADY (which is ok)
		 * or some other error (which is not ok).
		 */
		if (devctl_cmd(apidp->hba_phys, SCFGA_BUS_UNQUIESCE,
		    NULL, &l_errno) != SCFGA_OK && l_errno != EALREADY) {
			cfga_err(errstring, l_errno, ERR_BUS_UNQUIESCE, 0);
			ret = SCFGA_ERR;
		} else {
			/*
			 * Resume the bus through RCM if it successfully
			 * unquiesced.
			 */
			if ((apidp->flags & FLAG_DISABLE_RCM) == 0) {
				(void) scsi_rcm_resume(bus_path, dev_path,
				    errstring, 0);
			}
			sigstate.intr_hint = 0;
		}
	}

	/* Restore state */
	if (restore_disp(&sigstate, errstring) != SCFGA_OK) {
		ret = SCFGA_ERR;
	}

	S_FREE(sigstate.sigactp);
	S_FREE(bus_path);
	S_FREE(dev_path);

	(void) mutex_unlock(&sigstate.mp);

	return (ret);
}

static scfga_ret_t
init_sigstate(
	sigstate_t *sp,
	char **errstring)
{
	int i;
	sigset_t fullset, emptyset;
	scfga_ret_t ret;

	sp->inited = 0;

	/* Initialize  signal sets */
	errno = 0;
	if (sigfillset(&fullset) == -1 || sigemptyset(&emptyset) == -1) {
		ret = SCFGA_ERR;
		goto err;
	}

	sp->catch_set = fullset;
	for (i = 0; i < N_NOCATCH_SIGS; i++) {
		if (sigdelset(&sp->catch_set, unhandled_sigs[i]) == -1) {
			ret = SCFGA_ERR;
			goto err;
		}
	}

	/*
	 * Valid signals are: 1 through SCFGA_NSIGS - 1
	 */
	sp->sigactp = calloc(SCFGA_NSIGS, sizeof (struct sigaction));
	if (sp->sigactp == NULL) {
		ret = SCFGA_LIB_ERR;
		goto err;
	}

	/*
	 * Element 0 contains the new signal handler being installed.
	 * The remaining elements will be used to save the old disposition.
	 * Note: sa_handler and sa_sigaction overlap (union).
	 */
	for (i = 0; i < SCFGA_NSIGS; i++) {
		if (i == 0) {
			sp->sigactp[i].sa_handler = sig_handler;
		} else {
			sp->sigactp[i].sa_handler = SIG_DFL;
		}
		sp->sigactp[i].sa_mask = emptyset;
		sp->sigactp[i].sa_flags = 0;
	}

	return (SCFGA_OK);

err:
	cfga_err(errstring, errno, ERR_OP_FAILED, 0);
	return (ret);
}

static scfga_ret_t
change_disp(sigstate_t *sp, char **errstring)
{
	int i;


	/* Change disposition for all signals which can be caught */
	for (i = 1; i < SCFGA_NSIGS; i++) {
		/* Skip signals which cannot be caught */
		if (!sigismember(&sp->catch_set, i)) {
			continue;
		}
		errno = 0;
		if (sigaction(i, &sp->sigactp[0], &sp->sigactp[i]) == -1) {
			cfga_err(errstring, errno, ERR_OP_FAILED, 0);
			return (SCFGA_ERR);
		}
	}

	return (SCFGA_OK);
}

static scfga_ret_t
restore_disp(sigstate_t *sp, char **errstring)
{
	int i, rv = -1;
	scfga_ret_t ret = SCFGA_OK;


	/* Restore dispositions which were changed */
	for (i = 1; i < SCFGA_NSIGS; i++) {
		if (!sigismember(&sp->catch_set, i)) {
			continue;
		}

		if ((rv = sigaction(i, NULL, &sp->sigactp[0]))  != -1 &&
		    sp->sigactp[0].sa_handler == sig_handler &&
		    (rv = sigaction(i, &sp->sigactp[i], NULL)) != -1) {
			continue;
		}

		/* Check for errors */
		if (rv == -1) {
			cfga_err(errstring, errno, ERR_SIG_STATE, 0);
			ret = SCFGA_ERR;
		}
	}

	return (ret);
}

/*
 * Checks if HBA controls a critical file-system (/, /usr or swap)
 * This routine reads /etc/vfstab and is NOT foolproof.
 * If an error occurs, assumes that controller is NOT critical.
 */
static int
critical_ctrlr(const char *hba_phys)
{
	FILE *fp;
	struct vfstab vfst;
	int vfsret = 1, rv = -1;
	char *bufp;
	const size_t buflen = PATH_MAX;
	char mount[MAXPATHLEN], fstype[MAXPATHLEN], spec[MAXPATHLEN];


	if ((bufp = calloc(1, buflen)) == NULL) {
		return (0);
	}

	fp = NULL;
	if ((fp = fopen(ETC_VFSTAB, "r")) == NULL) {
		rv = 0;
		goto out;
	}

	while ((vfsret = getvfsent(fp, &vfst)) == 0) {

		(void) strcpy(mount, S_STR(vfst.vfs_mountp));
		(void) strcpy(fstype, S_STR(vfst.vfs_fstype));
		(void) strcpy(spec, S_STR(vfst.vfs_special));

		/* Ignore non-critical entries */
		if (strcmp(mount, "/") && strcmp(mount, "/usr") &&
		    strcmp(fstype, "swap")) {
			continue;
		}

		/* get physical path */
		if (realpath(spec, bufp) == NULL) {
			continue;
		}

		/* Check if critical partition is on the HBA */
		if (!(rv = hba_dev_cmp(hba_phys, bufp))) {
			break;
		}
	}

	rv = !vfsret;

	/*FALLTHRU*/
out:
	S_FREE(bufp);
	if (fp != NULL) fclose(fp);
	return (rv);
}

/*
 * Convert bus state to receptacle state
 */
static cfga_stat_t
bus_devctl_to_recep_state(uint_t bus_dc_state)
{
	cfga_stat_t rs;

	switch (bus_dc_state) {
	case BUS_ACTIVE:
		rs = CFGA_STAT_CONNECTED;
		break;
	case BUS_QUIESCED:
	case BUS_SHUTDOWN:
		rs = CFGA_STAT_DISCONNECTED;
		break;
	default:
		rs = CFGA_STAT_NONE;
		break;
	}

	return (rs);
}
