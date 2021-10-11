/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)librcm.c	1.1	99/08/10 SMI"

#include "librcm_impl.h"
#include "librcm_event.h"

#ifdef	DEBUG
static int rcm_debug = 0;
#define	dprintf(args) if (rcm_debug) (void) fprintf args
#else
#define	dprintf(args) /* nothing */
#endif	/* DEBUG */

static int extract_info(sys_event_t *, rcm_info_t **);
static int rcm_daemon_is_alive();
static int rcm_common(int, rcm_handle_t *, char *, uint_t, timespec_t *,
    rcm_info_t **);
static int rcm_direct_call(int, rcm_handle_t *, char *, uint_t, timespec_t *,
    rcm_info_t **);
static int rcm_daemon_call(int, rcm_handle_t *, char *, uint_t, timespec_t *,
    rcm_info_t **);
static sys_event_t *rcm_generate_event(int, rcm_handle_t *, char *, uint_t,
    timespec_t *);
static int rcm_check_permission(void);

/*
 * Allocate a handle structure
 */
/*ARGSUSED2*/
int
rcm_alloc_handle(char *modname, uint_t flag, void *arg, rcm_handle_t **hdp)
{
	rcm_handle_t *hd;
	void *temp;

	if ((hdp == NULL) || (flag & ~RCM_ALLOC_HDL_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	if (rcm_check_permission() == 0) {
		errno = EPERM;
		return (RCM_FAILURE);
	}

	if ((hd = calloc(1, sizeof (*hd))) == NULL) {
		return (RCM_FAILURE);
	}

	if (modname) {
		if ((hd->modname = strdup(modname)) == NULL) {
			free(hd);
			return (RCM_FAILURE);
		}

		if ((temp = rcm_module_open(modname)) == NULL) {
			errno = EINVAL;
			free(hd);
			return (RCM_FAILURE);
		}

		rcm_module_close(temp);
	}

	if (flag & RCM_NOPID) {
		hd->pid = (pid_t)0;
	} else {
		hd->pid = (pid_t)getpid();
	}

	*hdp = hd;
	return (RCM_SUCCESS);
}

/* free handle structure */
int
rcm_free_handle(rcm_handle_t *hd)
{
	if (hd == NULL) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	if (hd->modname) {
		free(hd->modname);
	}

	free(hd);
	return (RCM_SUCCESS);
}


/*
 * Operations which require daemon processing
 */

/* get registration and DR information from rcm_daemon */
int
rcm_get_info(rcm_handle_t *hd, char *rsrcname, uint_t flag, rcm_info_t **infop)
{
	if ((flag & ~RCM_GET_INFO_MASK) || (infop == NULL)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	/*
	 * rsrcname may be NULL if requesting dr operations or modinfo
	 */
	if ((rsrcname == NULL) &&
	    ((flag & RCM_DR_OPERATION|RCM_MOD_INFO) == 0)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_GETINFO, hd, rsrcname, flag, NULL, infop));
}

/* request to offline a resource before DR removal */
int
rcm_request_offline(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    rcm_info_t **infop)
{
	if (flag & ~RCM_REQUEST_MASK) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_OFFLINE, hd, rsrcname, flag, NULL, infop));
}

/* cancel offline request and allow apps to use rsrcname */
int
rcm_notify_online(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    rcm_info_t **infop)
{
	if (flag & ~RCM_NOTIFY_MASK) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_ONLINE, hd, rsrcname, flag, NULL, infop));
}

/* notify that rsrcname has been removed */
int
rcm_notify_remove(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    rcm_info_t **infop)
{
	if ((rsrcname == NULL) || (flag & ~RCM_NOTIFY_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_REMOVE, hd, rsrcname, flag, NULL, infop));
}

/* request for permission to suspend resource of interval time */
int
rcm_request_suspend(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    timespec_t *interval, rcm_info_t **infop)
{
	if ((rsrcname == NULL) || (flag & ~RCM_REQUEST_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	if ((interval == NULL) || (interval->tv_sec < 0) ||
	    (interval->tv_nsec < 0)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_SUSPEND, hd, rsrcname, flag, interval, infop));
}

/* notify apps of the completion of resource suspension */
int
rcm_notify_resume(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    rcm_info_t **infop)
{
	if ((rsrcname == NULL) || (flag & ~RCM_NOTIFY_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_RESUME, hd, rsrcname, flag, NULL, infop));
}

/*
 * Register interest in a resource. This requires a module to exist in module
 * directory. It should be called prior to using a new resource.
 *
 * Registration may be denied if it is presently locked by a DR operation.
 */
int
rcm_register_interest(rcm_handle_t *hd, char *rsrcname, uint_t flag,
    rcm_info_t **infop)
{
	if ((rsrcname == NULL) || (flag & ~RCM_REGISTER_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_REGISTER, hd, rsrcname, flag, NULL, NULL));
}

/* unregister interest in rsrcname */
int
rcm_unregister_interest(rcm_handle_t *hd, char *rsrcname, uint_t flag)
{
	if ((rsrcname == NULL) || (flag & ~RCM_REGISTER_MASK)) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	return (rcm_common(CMD_UNREGISTER, hd, rsrcname, flag, NULL, NULL));
}

/*
 * RCM helper functions exposed to librcm callers.
 */

/* Free linked list of registration info */
void
rcm_free_info(rcm_info_t *info)
{
	while (info) {
		rcm_info_t *tmp = info->next;

		free(info->rsrcname);
		if (info->info)
			free(info->info);
		if (info->modname)
			free(info->modname);
		free(info);

		info = tmp;
	}
}

/* return the next tuple in the info structure */
rcm_info_tuple_t *
rcm_info_next(rcm_info_t *info, rcm_info_tuple_t *tuple)
{
	if (info == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	if (tuple == NULL) {
		return ((rcm_info_tuple_t *)info);
	}
	return ((rcm_info_tuple_t *)tuple->next);
}

/* return resource name */
const char *
rcm_info_rsrc(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	return (tuple->rsrcname);
}

/* return info string in the tuple */
const char *
rcm_info_info(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	return (tuple->info);
}

/* return info string in the tuple */
const char *
rcm_info_modname(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	return (tuple->modname);
}

/* return client pid in the tuple */
pid_t
rcm_info_pid(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return ((pid_t)0);
	}

	return (tuple->pid);
}

/* return client state in the tuple */
int
rcm_info_state(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (RCM_STATE_UNKNOWN);
	}

	return (tuple->state);
}

/*
 * return operation sequence number
 *
 * This is private. Called by rcmctl only for testing purposes.
 */
int
rcm_info_seqnum(rcm_info_tuple_t *tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (-1);
	}

	return (tuple->seq_num);
}


/*
 * The following interfaces are PRIVATE to the RCM framework. They are not
 * declared static because they are called by rcm_daemon.
 */

/* Execute a command in MT safe manner */
int
rcm_exec_cmd(char *cmd)
{
	pid_t pid;

	if ((pid = fork1()) == 0) {
		(void) execlp(cmd, cmd, NULL);
	}

	if (pid == (pid_t)-1) {
		return (-1);
	}

	return (0);
}

/* Append info at the very end */
int
rcm_append_info(rcm_info_t **head, rcm_info_t *info)
{
	rcm_info_t *tuple;

	if (head == NULL) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	if ((tuple = *head) == NULL) {
		*head = info;
		return (RCM_SUCCESS);
	}

	while (tuple->next) {
		tuple = tuple->next;
	}
	tuple->next = info;
	return (RCM_SUCCESS);
}

/* get rcm module directory name */

#define	N_MODULE_DIR	3	/* search 3 directories for modules */
#define	MODULE_DIR_HW	"/usr/platform/%s/lib/rcm/modules/"
#define	MODULE_DIR_GEN	"/usr/lib/rcm/modules/"

char *
rcm_module_dir(uint_t dirnum)
{
	static char dir_name[N_MODULE_DIR][MAXPATHLEN];

	char infobuf[MAXPATHLEN];

	if (dirnum >= N_MODULE_DIR) {
		return (NULL);
	}

	if (dir_name[0][0] == '\0') {
		/*
		 * construct the module directory names
		 */
		if (sysinfo(SI_PLATFORM, infobuf, MAXPATHLEN) == -1) {
			dprintf((stderr, "sysinfo %s\n", strerror(errno)));
			return (NULL);
		} else {
			(void) sprintf(dir_name[0], MODULE_DIR_HW, infobuf);
		}

		if (sysinfo(SI_MACHINE, infobuf, MAXPATHLEN) == -1) {
			dprintf((stderr, "sysinfo %s\n", strerror(errno)));
			return (NULL);
		} else {
			(void) sprintf(dir_name[1], MODULE_DIR_HW, infobuf);
		}

		(void) strcpy(dir_name[2], MODULE_DIR_GEN);
	}

	return (dir_name[dirnum]);
}

/* Locate the module and call dlopen */
void *
rcm_module_open(char *modname)
{
	unsigned i;
	char *dir_name;
	void *dlhandle = NULL;
	char modpath[MAXPATHLEN];

#ifdef DEBUG
	struct stat sbuf;
#endif

	/*
	 * dlopen the module
	 */
	for (i = 0; (dir_name = rcm_module_dir(i)) != NULL; i++) {
		(void) sprintf(modpath, "%s%s%s", dir_name, modname,
		    RCM_MODULE_SUFFIX);

		if ((dlhandle = dlopen(modpath, RTLD_LAZY)) != NULL) {
			return (dlhandle);
		}
#ifdef DEBUG
		if (stat(modpath, &sbuf) == 0) {
			(void) fprintf(stderr, "%s is not a valid module\n",
			    modpath);
		}
#endif
	}

	dprintf((stderr, "module %s not found\n", modname));
	return (NULL);
}

/* dlclose module */
void
rcm_module_close(void *dlhandle)
{
	if (dlclose(dlhandle) == 0)
		return;

	dprintf((stderr, "dlclose: %s\n", dlerror()));
}

/*
 * stub implementation of rcm_log_message allows dlopen of rcm modules
 * to proceed in absence of rcm_daemon.
 *
 * This definition is interposed by the definition in rcm_daemon because of the
 * default search order implemented by the linker and dlsym(). All RCM modules
 * will see the daemon version when loaded by the rcm_daemon.
 */
void
rcm_log_message(int level, char *message, ...)
{
	dprintf((stderr, "rcm_log_message stub\n"));
}


/*
 * Helper functions
 */

/*
 * Common routine for all rcm calls which require daemon processing
 */
static int
rcm_common(int cmd, rcm_handle_t *hd, char *rsrcname, uint_t flag,
    timespec_t *interval, rcm_info_t **infop)
{
	if (hd == NULL) {
		errno = EINVAL;
		return (RCM_FAILURE);
	}

	if (getuid() != 0) {
		errno = EPERM;
		return (RCM_FAILURE);
	}

	/*
	 * Check if handle is allocated by rcm_daemon. If so, this call came
	 * from an RCM module, so we make a direct call into rcm_daemon.
	 */
	if (hd->lrcm_ops != NULL) {
		return (rcm_direct_call(cmd, hd, rsrcname, flag, interval,
		    infop));
	}

	/*
	 * When not called from a RCM module (i.e. no recursion), zero the
	 * pointer just in case caller did not do so. For recursive calls,
	 * we want to append rcm_info_t after infop; zero it may cause
	 * memory leaks.
	 */
	if (infop) {
		*infop = NULL;
	}

	/*
	 * Now call into the daemon.
	 */
	return (rcm_daemon_call(cmd, hd, rsrcname, flag, interval, infop));
}

/*
 * Caller is an RCM module, call directly into rcm_daemon.
 */
static int
rcm_direct_call(int cmd, rcm_handle_t *hd, char *rsrcname, uint_t flag,
    timespec_t *interval, rcm_info_t **infop)
{
	int error;

	librcm_ops_t *ops = (librcm_ops_t *)hd->lrcm_ops;
	switch (cmd) {
	case CMD_GETINFO:
		error = ops->librcm_getinfo(rsrcname, flag, hd->seq_num, infop);
		break;

	case CMD_OFFLINE:
		error = ops->librcm_offline(rsrcname, hd->pid, flag,
		    hd->seq_num, infop);
		break;

	case CMD_ONLINE:
		error = ops->librcm_online(rsrcname, hd->pid, flag,
		    hd->seq_num, infop);
		break;

	case CMD_REMOVE:
		error = ops->librcm_remove(rsrcname, hd->pid, flag,
		    hd->seq_num, infop);
		break;

	case CMD_SUSPEND:
		error = ops->librcm_suspend(rsrcname, hd->pid, flag,
		    hd->seq_num, interval, infop);
		break;

	case CMD_RESUME:
		error = ops->librcm_resume(rsrcname, hd->pid, flag,
		    hd->seq_num, infop);
		break;

	case CMD_REGISTER:
		error = ops->librcm_regis(hd->modname, rsrcname, hd->pid,
		    flag, infop);
		break;

	case CMD_UNREGISTER:
		error = ops->librcm_unregis(hd->modname, rsrcname, hd->pid,
		    flag);
		break;

	default:
		dprintf((stderr, "invalid command: %d\n", cmd));
		error = EFAULT;
	}

	if (error > 0) {
		errno = error;
		error = RCM_FAILURE;
	}
	return (error);
}

/*
 * Call into rcm_daemon door to process the request
 */
static int
rcm_daemon_call(int cmd, rcm_handle_t *hd, char *rsrcname, uint_t flag,
    timespec_t *interval, rcm_info_t **infop)
{
	int error = RCM_SUCCESS;
	int delay = 300;
	int maxdelay = 10000;	/* 10 seconds */
	size_t rsize;
	sys_event_t *ev, *ret;
	int start_daemon;
	rcm_info_t *info = NULL;

	/*
	 * Decide whether to start the daemon
	 */
	switch (cmd) {
	case CMD_GETINFO:
	case CMD_OFFLINE:
	case CMD_ONLINE:
	case CMD_REMOVE:
	case CMD_SUSPEND:
	case CMD_RESUME:
	case CMD_REGISTER:
	case CMD_UNREGISTER:
		start_daemon = 1;
		break;

	default:
		errno = EFAULT;
		return (RCM_FAILURE);
	}

	if (rcm_daemon_is_alive(start_daemon) == 0) {
		/* cannot start daemon */
		errno = EFAULT;
		return (RCM_FAILURE);
	}

	/*
	 * Generate an RCM event for the request
	 */
	if ((ev = rcm_generate_event(cmd, hd, rsrcname, flag, interval))
	    == NULL) {
		dprintf((stderr, "error in event generation\n"));
		errno = EFAULT;
		return (RCM_FAILURE);
	}

	/*
	 * Make the door call and get a return event. We go into a retry loop
	 * when RCM_ET_EAGAIN is returned.
	 */
retry:
	error = get_event_service(RCM_SERVICE_DOOR, (void *)ev,
	    SE_SIZE(ev), (void **)&ret, &rsize);
	if (error != 0) {
		dprintf((stderr, "rcm_daemon call failed: %s\n",
		    strerror(errno)));
		se_free(ev);
		return (RCM_FAILURE);
	}

	assert(ret != NULL);
	assert(rsize != 0);
	assert(SE_CLASS(ret) == EC_RCM);

	if (SE_TYPE(ret) == ET_RCM_EAGAIN) {
		/*
		 * Wait and retry
		 */
		dprintf((stderr, "retry door_call\n"));
		(void) munmap((void *)ret, rsize);

		if (delay > maxdelay) {
			errno = EAGAIN;
			return (RCM_FAILURE);
		}

		(void) poll(NULL, 0, delay);
		delay *= 2;		/* exponential back off */
		goto retry;
	}

	/*
	 * The door call succeeded. Now extract info from returned event.
	 */
	if (extract_info(ret, &info) != 0) {
		dprintf((stderr, "error in extracting event data\n"));
		errno = EFAULT;
		error = RCM_FAILURE;
		goto out;
	}

	if (infop)
		*infop = info;
	else
		rcm_free_info(info);

	switch (SE_TYPE(ret)) {
	case ET_RCM_INFO:
	case ET_RCM_REQ_GRANTED:
	case ET_RCM_NOTIFY_DONE:
		error = RCM_SUCCESS;
		break;

	case ET_RCM_NOTIFY_FAIL:
		error = RCM_FAILURE;
		if (info)
			errno = EBUSY;	/* What's the right code here??? */
		else
			errno = ENOENT;	/* no info, so resource not DR'ed */
		break;

	case ET_RCM_REQ_DENIED:
		error = RCM_FAILURE;
		errno = EBUSY;
		break;

	case ET_RCM_REQ_CONFLICT:
		error = RCM_CONFLICT;
		errno = EDEADLK;
		break;

	case ET_RCM_EFAULT:
		error = RCM_FAILURE;
		errno = EFAULT;
		break;

	case ET_RCM_EPERM:
		error = RCM_FAILURE;
		errno = EPERM;
		break;

	case ET_RCM_EINVAL:
		error = RCM_FAILURE;
		errno = EINVAL;
		break;

	case ET_RCM_EALREADY:
		error = RCM_FAILURE;
		errno = EALREADY;
		break;

	case ET_RCM_ENOENT:
		error = RCM_FAILURE;
		errno = ENOENT;
		break;

	default:
		error = RCM_FAILURE;
		errno = EFAULT;
		dprintf((stderr, "unknown event type from rcm_daemon: %d\n",
		    SE_TYPE(ret)));
		break;
	}

out:
	(void) munmap((void *)ret, rsize);
	dprintf((stderr, "daemon call is done, error = %d\n", error));
	return (error);
}

/*
 * Extract registration info from event data.
 * Return 0 on success and -1 on failure.
 */
static int
extract_info(sys_event_t *ev, rcm_info_t **infop)
{
	rcm_info_t *info = NULL, *prev, *tmp = NULL;
	se_data_tuple_t tuple = NULL;

	while (tuple = se_get_next_tuple(ev, tuple)) {
		pid_t *pid;
		int *state;
		int *seq_num;
		char *str, *name;

		/*
		 * Tuples are always in the order of
		 * RCM_RSRCNAME, RCM_CLIENT_INFO, [RCM_CLIENT_ID]
		 * ...
		 */
		name = se_tuple_name(tuple);
		if (strcmp(name, RCM_RSRCNAME) == 0) {
			(void) se_tuple_strings(tuple, &str);
			tmp = calloc(1, sizeof (*info));
			if (tmp == NULL) {
				dprintf((stderr, "out of memory\n"));
				goto fail;
			}
			tmp->rsrcname = strdup(str);
			if (info == NULL) {
				prev = info = tmp;
			} else {
				prev->next = tmp;
				prev = tmp;
			}
			continue;
		}

		if (tmp == NULL) {
			dprintf((stderr, "event data out of order\n"));
			goto fail;
		}

		if (strcmp(name, RCM_CLIENT_INFO) == 0) {
			(void) se_tuple_strings(tuple, &str);
			tmp->info = strdup(str);
			continue;
		}

		if (strcmp(name, RCM_CLIENT_ID) == 0) {
			(void) se_tuple_bytes(tuple, (uchar_t **)&pid);
			tmp->pid = *pid;
			continue;
		}

		if (strcmp(name, RCM_CLIENT_MODNAME) == 0) {
			(void) se_tuple_strings(tuple, &str);
			tmp->modname = strdup(str);
			continue;
		}

		if (strcmp(name, RCM_SEQ_NUM) == 0) {
			(void) se_tuple_bytes(tuple, (uchar_t **)&seq_num);
			tmp->seq_num = *seq_num;
			continue;
		}

		if (strcmp(name, RCM_RSRCSTATE) == 0) {
			(void) se_tuple_ints(tuple, &state);
			tmp->state = *state;
			continue;
		}

		dprintf((stderr, "unknown tuple name: %s\n", name));
	}
	*infop = info;
	return (0);

fail:
	rcm_free_info(info);
	return (-1);
}

/* Generate an event for communicating with RCM daemon */
static sys_event_t *
rcm_generate_event(int cmd, rcm_handle_t *hd, char *rsrcname, uint_t flag,
    timespec_t *interval)
{
	int class = EC_RCM, type;
	sys_event_t *ev;

	/*
	 * Figure out event type
	 */
	switch (cmd) {
	case CMD_GETINFO:
		type = ET_RCM_GET_INFO;
		break;

	case CMD_OFFLINE:
		type = ET_RCM_OFFLINE;
		break;

	case CMD_ONLINE:
		type = ET_RCM_ONLINE;
		break;

	case CMD_REMOVE:
		type = ET_RCM_REMOVE;
		break;

	case CMD_SUSPEND:
		type = ET_RCM_SUSPEND;
		break;

	case CMD_RESUME:
		type = ET_RCM_RESUME;
		break;

	case CMD_REGISTER:
		type = ET_RCM_REGIS_RESOURCE;
		break;

	case CMD_UNREGISTER:
		type = ET_RCM_UNREGIS_RESOURCE;
		break;

	default:
		dprintf((stderr, "invalid command: %d\n", cmd));
		return (NULL);
	}

	/*
	 * Allocate event
	 */
	ev = se_alloc(class, type, 0);
	if (ev == NULL) {
		dprintf((stderr, "cannot allocate event: %s\n",
		    strerror(errno)));
		return (NULL);
	}

	/*
	 * Append event data
	 */
	if (rsrcname && se_append_strings(ev, RCM_RSRCNAME, rsrcname, 1)) {
		dprintf((stderr, "fail to append RCM_RSRCNAME: %s\n",
		    rsrcname));
		goto fail;
	}
	if (hd->modname &&
	    se_append_strings(ev, RCM_CLIENT_MODNAME, hd->modname, 1)) {
		dprintf((stderr, "fail to append RCM_CLIENT_MODNAME: %s\n",
		    hd->modname));
		goto fail;
	}
	if (hd->pid && se_append_bytes(ev, RCM_CLIENT_ID, (uchar_t *)&hd->pid,
	    sizeof (pid_t))) {
		dprintf((stderr, "fail to append CLIENT_ID: %ld\n", hd->pid));
		goto fail;
	}
	if (flag && se_append_ints(ev, RCM_REQUEST_FLAG, (int *)&flag, 1)) {
		dprintf((stderr, "fail to append REQUEST_FLAG: 0x%x\n", flag));
		goto fail;
	}
	if (interval && se_append_bytes(ev, RCM_SUSPEND_INTERVAL,
	    (uchar_t *)interval, sizeof (*interval))) {
		dprintf((stderr, "fail to append SUSPEND_INTERVAL\n"));
		goto fail;
	}

	ev = se_end_of_data(ev);	/* pack data into contiguous buffer */
	return (ev);

fail:
	se_free(ev);
	return (NULL);
}

/* check if rcm_daemon is up and running */
static int
rcm_daemon_is_alive(int start)
{
	struct stat buf;
	sys_event_t *ev;
	int error;
	int delay = 300;
	const int maxdelay = 10000;	/* 10 sec */

	ev = se_alloc(EC_RCM, ET_RCM_NOOP, 0);
	if (ev == NULL) {
		dprintf((stderr, "cannot allocate event: %s\n",
		    strerror(errno)));
		return (0);
	}

	/*
	 * check the door file and send a dummy event
	 */
	if ((stat(RCM_SERVICE_DOOR, &buf) == 0) &&
	    (get_event_service(RCM_SERVICE_DOOR, (void *)ev, SE_SIZE(ev),
	    NULL, NULL) == 0)) {
		se_free(ev);
		return (1);	/* daemon is alive */
	}

	/*
	 * If not instructed to start daemon, return daemon not alive
	 */
	if (!start) {
		se_free(ev);
		return (0);
	}

	/*
	 * Attempt to start the daemon
	 */
	dprintf((stderr, "exec: %s\n", RCM_DAEMON_START));
	if (rcm_exec_cmd(RCM_DAEMON_START) != 0) {
		dprintf((stderr, "%s failed\n", RCM_DAEMON_START));
		se_free(ev);
		return (0);
	}

	/*
	 * Wait for daemon to respond, timeout at 10 sec
	 */
	while (((error = get_event_service(RCM_SERVICE_DOOR, (void *)ev,
	    SE_SIZE(ev), NULL, NULL)) != 0) &&
	    ((errno == EBADF) || (errno == ESRCH))) {
		if (delay > maxdelay) {
			break;
		}
		(void) poll(NULL, 0, delay);
		delay *= 2;
	}

	return (error == 0);
}

/*
 * Check permission.
 *
 * The policy is root only for now. Need to relax this when interface level
 * is raised.
 */
static int
rcm_check_permission(void)
{
	return (getuid() == 0);
}
