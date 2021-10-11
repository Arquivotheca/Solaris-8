/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rcm_event.c	1.1	99/08/10 SMI"

#include <door.h>
#include <assert.h>
#include <sys/acl.h>
#include <sys/stat.h>
#include <librcm_event.h>

#include "rcm_impl.h"

/*
 * Event handling routine
 */

#define	RCM_NOTIFY	0
#define	RCM_GETINFO	1
#define	RCM_REQUEST	2
#define	RCM_EFAULT	3
#define	RCM_EPERM	4
#define	RCM_EINVAL	5

/*
 * data packing errors
 */
#define	ERR_APPEND_STRINGS	1
#define	ERR_APPEND_INTS		2
#define	ERR_APPEND_BYTES	3

static sys_event_t *process_event(sys_event_t *, int);
static sys_event_t *generate_reply_event(int, int, rcm_info_t *);

/*
 * Top level function for event service
 */
void
event_service(void **data, size_t *datalen)
{
	int seq_num;
	sys_event_t *ev = (sys_event_t *)*data;

	assert(SE_CLASS(ev) == EC_RCM);

	rcm_log_message(RCM_TRACE1, "receive event class %d, type %d\n",
	    SE_CLASS(ev), SE_TYPE(ev));

	if (SE_TYPE(ev) == ET_RCM_NOOP) {
		*data = NULL;
		*datalen = 0;
		return;
	}

	/*
	 * Go increment thread count. Before daemon is fully initialized,
	 * the event processing blocks inside this function.
	 */
	seq_num = rcmd_thr_incr(SE_TYPE(ev));

	ev = process_event(ev, seq_num);
	assert(ev != NULL);

	/*
	 * Decrement thread count
	 */
	rcmd_thr_decr();

out:
	*data = ev;
	*datalen = SE_SIZE(ev);
	rcm_log_message(RCM_TRACE1, "reply event size = %d\n", *datalen);
}

/*
 * Actually processes events; returns a reply event
 */
static sys_event_t *
process_event(sys_event_t *ev, int seq_num)
{
	pid_t pid;
	uint_t flag;
	uchar_t *pidp = NULL;
	int *flagp = NULL;
	timespec_t *interval = NULL;
	char *modname, *rsrcname;

	int error;
	int rcm_type = RCM_NOTIFY;
	rcm_info_t *info = NULL;

	rcm_log_message(RCM_TRACE2, "servicing event type=%d\n", SE_TYPE(ev));

	/*
	 * Extract data from events. Not all data apply to every events.
	 * Sanity checkes are performed inside switch.
	 */
	(void) se_lookup_strings(ev, RCM_RSRCNAME, &rsrcname);
	(void) se_lookup_strings(ev, RCM_CLIENT_MODNAME, &modname);
	(void) se_lookup_bytes(ev, RCM_CLIENT_ID, &pidp);
	(void) se_lookup_ints(ev, RCM_REQUEST_FLAG, &flagp);
	(void) se_lookup_bytes(ev, RCM_SUSPEND_INTERVAL, (uchar_t **)&interval);

	/*LINTED*/
	pid = pidp ? *(pid_t *)pidp : 0;
	flag = flagp ? *(uint_t *)flagp : 0;

	switch (SE_TYPE(ev)) {
	case ET_RCM_SUSPEND:
		if (rsrcname == NULL) {
			goto faildata;
		}
		rcm_type = RCM_REQUEST;
		error = process_resource_suspend(rsrcname, pid, flag, seq_num,
		    interval, &info);
		break;

	case ET_RCM_RESUME:
		if (rsrcname == NULL) {
			goto faildata;
		}
		error = notify_resource_resume(rsrcname, pid, flag, seq_num,
		    &info);
		break;

	case ET_RCM_OFFLINE:
		if (rsrcname == NULL) {
			goto faildata;
		}
		rcm_type = RCM_REQUEST;
		error = process_resource_offline(rsrcname, pid, flag, seq_num,
		    &info);
		break;

	case ET_RCM_ONLINE:
		if (rsrcname == NULL) {
			goto faildata;
		}
		error = notify_resource_online(rsrcname, pid, flag, seq_num,
		    &info);
		break;

	case ET_RCM_REMOVE:
		if (rsrcname == NULL) {
			goto faildata;
		}
		error = notify_resource_remove(rsrcname, pid, flag, seq_num,
		    &info);
		break;

	case ET_RCM_REGIS_RESOURCE:
		if (modname == NULL || rsrcname == NULL) {
			goto faildata;
		}
		rcm_type = RCM_REQUEST;
		error = add_resource_client(modname, rsrcname, pid, flag,
		    &info);
		break;

	case ET_RCM_UNREGIS_RESOURCE:
		if (modname == NULL || rsrcname == NULL) {
			goto faildata;
		}
		error = remove_resource_client(modname, rsrcname, pid, flag);
		break;

	case ET_RCM_GET_INFO:
		if ((rsrcname == NULL) &&
		    ((flag & (RCM_DR_OPERATION | RCM_MOD_INFO)) == 0)) {
			goto faildata;
		}
		rcm_type = RCM_GETINFO;
		error = get_resource_info(rsrcname, flag, seq_num, &info);

		if (error == EINVAL) {
			rcm_log_message(RCM_DEBUG,
			    "invalid argument in get info request\n",
			    SE_CLASS(ev), SE_TYPE(ev));

			return (generate_reply_event(RCM_EINVAL, 0, NULL));
		}
		break;

	default:
		rcm_log_message(RCM_WARNING, gettext("unknown event type %d\n"),
		    SE_TYPE(ev));
		return (generate_reply_event(RCM_EFAULT, 0, NULL));
	}

	rcm_log_message(RCM_TRACE2, "finish processing event 0x%x\n",
	    SE_ID(ev));
	return (generate_reply_event(rcm_type, error, info));

faildata:
	rcm_log_message(RCM_WARNING, gettext("data error in event type %d\n"),
	    SE_TYPE(ev));

	return (generate_reply_event(RCM_EFAULT, 0, NULL));
}


/*
 * Generate reply event from resource registration information
 */
static sys_event_t *
generate_reply_event(int rcm_type, int error, rcm_info_t *info)
{
	int err = 0;
	int event_type;
	sys_event_t *ev;
	rcm_info_t *tmp;

	rcm_log_message(RCM_TRACE4, "generating reply event\n");

	/*
	 * Translate rcm_type & error into an event type
	 */
	switch (rcm_type) {
	case RCM_NOTIFY:
		switch (error) {
		case EAGAIN:
			/*
			 * Ask librcm to try again
			 */
			event_type = ET_RCM_EAGAIN;
			break;

		case ENOENT:
			event_type = ET_RCM_ENOENT;
			break;

		case RCM_SUCCESS:
			event_type = ET_RCM_NOTIFY_DONE;
			break;

		default:
			event_type = ET_RCM_NOTIFY_FAIL;
			break;
		}
		break;

	case RCM_GETINFO:
		event_type = ET_RCM_INFO;
		break;

	case RCM_REQUEST:
		switch (error) {
		case RCM_SUCCESS:
			event_type = ET_RCM_REQ_GRANTED;
			break;

		case RCM_CONFLICT:
			event_type = ET_RCM_REQ_CONFLICT;
			break;

		case EALREADY:
			event_type = ET_RCM_EALREADY;
			break;

		default:
			event_type = ET_RCM_REQ_DENIED;
			break;
		}
		break;

	case RCM_EPERM:
		event_type = ET_RCM_EPERM;
		break;

	case RCM_EINVAL:
		event_type = ET_RCM_EINVAL;
		break;

	case RCM_EFAULT:
	default:	/* paranoia */
		event_type = ET_RCM_EFAULT;
		break;
	}

	/*
	 * Generate event
	 */
	ev = se_alloc(EC_RCM, event_type, 0);
	if (ev == NULL) {
		rcm_log_message(RCM_ERROR, gettext("se_alloc failed: %s\n"),
		    strerror(errno));
		rcmd_exit(errno);
	}

	/*
	 * Append info, if any, to event
	 */
	tmp = info;
	while (tmp) {
		err = se_append_strings(ev, RCM_RSRCNAME, tmp->rsrcname, 1);
		free(tmp->rsrcname);
		if (err != 0) {
			err = ERR_APPEND_STRINGS;
			break;
		}

		err = se_append_ints(ev, RCM_RSRCSTATE, &tmp->state, 1);
		if (err != 0) {
			err = ERR_APPEND_INTS;
			break;
		}

		if (tmp->modname) {
			err = se_append_strings(ev, RCM_CLIENT_MODNAME,
			    tmp->modname, 1);
			free(tmp->modname);
			if (err != 0) {
				err = ERR_APPEND_STRINGS;
				break;
			}
		}
		if (tmp->info) {
			err = se_append_strings(ev, RCM_CLIENT_INFO,
			    tmp->info, 1);
			free(tmp->info);
			if (err != 0) {
				err = ERR_APPEND_STRINGS;
				break;
			}
		}
		if (tmp->pid != 0) {
			err = se_append_bytes(ev, RCM_CLIENT_ID,
			    (uchar_t *)&tmp->pid, sizeof (tmp->pid));
			if (err != 0) {
				err = ERR_APPEND_BYTES;
				break;
			}
		}

		err = se_append_bytes(ev, RCM_SEQ_NUM,
		    (uchar_t *)&tmp->seq_num, sizeof (tmp->seq_num));
		if (err != 0) {
			err = ERR_APPEND_BYTES;
			break;
		}

		info = tmp->next;
		free(tmp);
		tmp = info;
	}

	switch (err) {
	case 0:		/* no error */
		return (se_end_of_data(ev));

	case ERR_APPEND_STRINGS:
		rcm_log_message(RCM_ERROR, gettext(
		    "se_append_strings failed: %s\n"), strerror(errno));
		break;

	case ERR_APPEND_INTS:
		rcm_log_message(RCM_ERROR, gettext(
		    "se_append_ints failed: %s\n"), strerror(errno));
		break;

	case ERR_APPEND_BYTES:
		rcm_log_message(RCM_ERROR, gettext(
		    "se_append_bytes failed: %s\n"), strerror(errno));
		break;
	}

	rcmd_exit(errno);	/* exit daemon on error */
	/*NOTREACHED*/
}
