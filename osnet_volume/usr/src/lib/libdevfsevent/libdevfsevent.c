#ident  "@(#)libdevfsevent.c 1.1     98/07/11 SMI"
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <door.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdevfsevent.h>

/*
 * This is the internal definition of the opaque type 'event_filter_t'
 * which is really just a placeholder for pointers the user process
 * passes to these routines, pointers which actually refer to structs
 * of type libevent_filter_t. Internally, pointers to libevent_filter_t
 * are called "ihandles" to distinguish them from pointers to struct
 * event_filter_t, which are called "handles", but really they are
 * interchangeable, the only difference being whether or not they are
 * cast to the opaque type event_filter_t, which is the only type
 * the user process sees.
 */
typedef struct event_filter {
	int service_door;
	eventd_service_t req;
	door_arg_t arg;
	time_t tval;
	log_event_request_t *request;
} libevent_filter_t;

typedef struct log_event_info {
	char *event_class;
	char *event_type;
} log_event_info_t;


static void
free_request_t(log_event_request_t *rp)
{
	log_event_request_t *saverp;

	while (rp) {
		saverp = rp->next;
		if (rp->event_class)
			free(rp->event_class);
		if (rp->event_type)
			free(rp->event_type);
		free(rp);
		rp = saverp;
	}
}

static log_event_request_t *
dup_request_t(log_event_request_t *src)
{
	log_event_request_t  *rp, *dup_rp, *prev, *head;

	prev = NULL;
	head = NULL;
	for (rp = src; rp != NULL; rp = rp->next) {
		if ((dup_rp = calloc(1,
			sizeof (log_event_request_t))) == NULL) {
			free_request_t(head);
			return (NULL);
		}
		if (prev) {
			prev->next = dup_rp;
			prev = prev->next;
		} else {
			/*
			 * beginning of list
			 */
			head = prev = dup_rp;
		}
		if (rp->event_class) {
			if ((dup_rp->event_class =
				malloc(strlen(rp->event_class)+1)) == NULL) {
				free_request_t(head);
				return (NULL);
			}
			strcpy(dup_rp->event_class, rp->event_class);
		}
		if (rp->event_type) {
			if ((dup_rp->event_type =
				malloc(strlen(rp->event_type)+1)) == NULL) {
				free_request_t(head);
				return (NULL);
			}
			strcpy(dup_rp->event_type, rp->event_type);
		}
	}
	return (head);
}

/*
 * Userland event logger interface function.
 *
 * Returns -1 if message not delivered. With errno set to cause of error.
 * Returns 0 for success.
 */

int
log_event(int argc, log_event_tuple_t tuples[])
{

	char	*bufptr;
	int	nbytes = 1, attr_len, val_len;
	int	i;
	int	service_door;
	time_t	tval;
	eventd_service_t	req;
	door_arg_t	door_arg;

	/*
	 * take the passed array of tuples and copy their components into
	 * a single format string separated by semicolons such that the
	 * string passed to userland listener looks like:
	 *
	 * "attr=val;attr2=val2;attr3=val3;..."
	 *
	 * prepend the format string with a timestamp in an ascii rep. of
	 * unsigned long. This can be used by userland listener for conversion
	 * to a time_t value and subsequently used as an argument to ctime(3).
	 *
	 * The .attr and .val fields of each array member must be non-NULL.
	 */
	(void) time(&tval);

	/*
	 * prepend ascii-fied timestamp.
	 */
	sprintf(req.buf, "%u;", tval);
	bufptr = req.buf + strlen(req.buf);
	for (i = 0; i < argc; i++) {
		if (tuples[i].attr == NULL || tuples[i].val == NULL) {
			errno = EINVAL;
			return (-1);
		}
		attr_len = strlen(tuples[i].attr);
		val_len = strlen(tuples[i].val);
		if (nbytes + attr_len + val_len + 1 > LOGEVENT_BUFSIZE) {
			*bufptr = (char)'\0';
			break;
		}
		nbytes += attr_len + 1 + val_len;
		strcpy(bufptr, tuples[i].attr);
		bufptr += attr_len;
		*bufptr++ = '=';
		strcpy(bufptr, tuples[i].val);
		bufptr += val_len;
		if (i + 1 <  argc) {
			/*
			 * if more args, add a semicolon
			 */
			*bufptr = ';';
			bufptr++;
			nbytes++;
		}
	}
	if ((service_door = open(LOGEVENT_DOOR_SERVICES, O_RDONLY, 0)) == -1) {
		errno = ESRCH;
		return (-1);
	}

	door_arg.rbuf = (char *)&req;
	door_arg.data_ptr = (char *)&req;
	door_arg.rsize = sizeof (eventd_service_t);
	door_arg.data_size = sizeof (eventd_service_t);
	door_arg.desc_ptr = NULL;
	door_arg.desc_num = 0;

	req.pid = (int)getpid();
	req.event_id = -1;
	req.service_code = EVENTD_LOGMSG;

	if (door_call(service_door, &door_arg) == -1) {
		close(service_door);

		switch (errno) {
		case EINTR:
		case EBADF:
			break;

		default:
			break;
		}
		return (-1);
	}

	switch (req.retcode) {
	case 0:
		/*
		 * Successfully sent message
		 */
		break;
	default:
		errno = req.retcode;
		close(service_door);
		return (-1);
	}
	close(service_door);
	return (0);
}

event_filter_t *
request_log_event(event_filter_t *old_handle, log_event_request_t *request)
{
	libevent_filter_t *new_handle;
	int my_pid;

	if (request == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	if (old_handle != NULL) {
		/*
		 * we should have a door descriptor already,
		 * so just free the handle request structures
		 */
		free_request_t(old_handle->request);
		new_handle = (libevent_filter_t *)old_handle;
	} else {
		/* allocate a handle */
		if ((new_handle = malloc(sizeof (libevent_filter_t))) == NULL) {
			errno = ENOMEM;
			return (NULL);
		}

		/* Get the door descriptor */
		if ((new_handle->service_door =
		    open(LOGEVENT_DOOR_SERVICES, O_RDONLY, 0)) == -1) {
			errno = ESRCH;
			free(new_handle);
			return (NULL);
		}

		my_pid = (int)getpid();
		new_handle->arg.rbuf = (char *)&new_handle->req;
		new_handle->arg.data_ptr = (char *)&new_handle->req;
		new_handle->arg.rsize = sizeof (eventd_service_t);
		new_handle->arg.data_size = sizeof (eventd_service_t);
		new_handle->arg.desc_ptr = NULL;
		new_handle->arg.desc_num = 0;

		new_handle->req.pid = my_pid;
		new_handle->req.event_id = -1;
		new_handle->req.service_code = EVENTD_REGISTER;
		new_handle->req.buf[0] = '\0';

		if (door_call(new_handle->service_door, &new_handle->arg)
									== -1) {
			close(new_handle->service_door);

			switch (errno) {
			case EINTR:
			case EBADF:
				break;

			default:
				break;
			}
			free(new_handle);
			return (NULL);
		}

		switch (new_handle->req.retcode) {
		case 0:
			/*
			 * Successful registration.
			 */
			break;
		default:
			close(new_handle->service_door);
			free(new_handle);
			return (NULL);
		}
	}

	/*
	 * Copy the list of request structures, so the user doesn't
	 * have to preserve the original list
	 */
	if ((new_handle->request = dup_request_t(request)) == NULL) {
		close(new_handle->service_door);
		free(new_handle);
		errno = ENOMEM;
		return (NULL);
	}

	/*
	 * return the handle
	 */
	return ((event_filter_t *)new_handle);
}


/*
 * Unregister with the event manager
 */
int
cancel_log_event_request(event_filter_t *handle)
{
	int retval;
	libevent_filter_t *ihandle = handle;

	if (handle == NULL) {
		errno = EINVAL;
		return (-1);
	}

	ihandle->req.service_code = EVENTD_UNREGISTER;
	ihandle->req.buf[0] = '\0';

	retval = 0;
	if (door_call(ihandle->service_door, &ihandle->arg) == -1) {
		errno = EIO;
		retval = -1;
	}
	close(ihandle->service_door);

	/* free all the data structures associated with handle */
	free_request_t(handle->request);
	free(handle);
	return (retval);
}

/*
 * Get the next tuple from the event message
 */
int
get_log_event_tuple(log_event_tuple_t *tuple, char **lasts)
{
	if ((tuple == NULL) || (lasts == NULL)) {
		errno = EINVAL;
		return (-1);
	}

	/* let strtok_r() do all the work */
	if ((tuple->attr = strtok_r((char *)NULL, TUPLES_CONNECTOR, lasts))
								== NULL) {
		return (1);	/* no tuples remain */
	}

	if ((tuple->val = strtok_r((char *)NULL, TUPLES_SEPARATOR, lasts))
								== NULL) {
		errno = EINVAL;	/* buffer is munged */
		return (-1);
	}
	return (0);		/* tuple is OK */
}

/*
 * Parse the incoming message for criteria to match with
 */
static void
get_message_info(libevent_filter_t *ihandle, char *buf, log_event_info_t
	*info)
{
	char *next, *tp;
	log_event_tuple_t tuple;

	info->event_class = NULL;
	info->event_type = NULL;

	/*
	 * convert and save the time stamp
	 */
	tp = strtok_r(ihandle->req.buf, TUPLES_SEPARATOR, &next);
	ihandle->tval = (time_t)atol(tp);

	/*
	 * copy the message to the user-supplied buffer.
	 */
	strcpy(buf, next);

	/* parse the tuples, looking for special info fields */
	while (get_log_event_tuple(&tuple, &next) == 0) {
		if (strcmp(tuple.attr, LOGEVENT_CLASS) == 0)
			info->event_class = tuple.val;
		else if (strcmp(tuple.attr, LOGEVENT_TYPE) == 0)
			info->event_type = tuple.val;
	}
}

/*
 * Check to see if there's a match between user's criteria and current message
 */
static int
match_request(libevent_filter_t *ihandle, char *buf)
{
	log_event_request_t	*rp;
	log_event_info_t	message_info;

	get_message_info(ihandle, buf, &message_info);

	/*
	 * Check for match, or wild card value
	 * Possible matches for each field are:
	 *
	 * 	NULL in requested field matches anything
	 *	Non-existent field in message matches if requested field is NULL
	 *	If both strings exist, they must match exactly
	 */

	for (rp = ihandle->request; rp != NULL; rp = rp->next) {
		if (((rp->event_class == NULL) ||
		    ((message_info.event_class != NULL) &&
		    (strcmp(rp->event_class,
			message_info.event_class) == 0))) &&
		    ((rp->event_type == NULL) ||
		    ((message_info.event_type != NULL) &&
		    (strcmp(rp->event_type, message_info.event_type) == 0))))

			return (1);
	}

	return (0);
}

/*
 *  Return the next available log event message in user-supplied buffer
 */
int
get_log_event(event_filter_t *handle, char *buf, time_t *tstamp)
{
	libevent_filter_t *ihandle = handle;

	if ((handle == NULL) || (buf == NULL)) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 *  Get message from event daemon
	 *  keep retrying if matching criteria isn't met or
	 *  eventd returns EAGAIN
	 *  Other errors indicate bad problems
	 */
	for (;;) {
		ihandle->req.service_code = EVENTD_GETMSG;
		ihandle->req.buf[0] = '\0';

		if (door_call(ihandle->service_door, &ihandle->arg) == -1) {
			switch (errno) {
			case EINTR:
				break;
			case EBADF:
				errno = ESRCH;
				break;
			default:
				errno = EIO;
				break;
			}
			return (-1);
		}

		switch (ihandle->req.retcode) {
		case 0:
			/*
			 * successfully returned message, check for match
			 */
			if (match_request(ihandle, buf)) {
				*tstamp = ihandle->tval;
				return (0);
			}
			break;

		case EAGAIN:
			break;

		default:
			errno = EIO;
			return (-1);
		}
	}
	/* NOT REACHED */
}

#ifdef LIBEVENT_DEBUG
#include <varargs.h>
#define	MAXARGS	5
void
logcons(va_alist)
va_dcl
{
	va_list ap;
	int numargs, i;
	char *args[MAXARGS];
	static FILE	*conslog = NULL;

	if (!conslog)
		if ((conslog = fopen("/dev/console", "w")) == (FILE *)NULL) {
			fprintf(stderr,
				"libevent: can't open console device.\n");
			return;
		}

	/* get the arguments */
	va_start(ap);
	numargs = va_arg(ap, int);
	for (i = 0; i < min(numargs, MAXARGS); i++)
		args[i] = va_arg(ap, char *);
	va_end(ap);

	/* log the message */
	fprintf(conslog, args[0], args[1], args[2], args[3], args[4]);
}
#endif
