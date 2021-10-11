/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)log_event.c	1.3	99/05/20 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/callb.h>
#include <sys/devfs_log_event.h>
#include <sys/modctl.h>

/* for doors */
#include <sys/pathname.h>
#include <sys/door.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/fs/snode.h>

/*
 * log_event msg queue struct
 */
typedef struct log_eventq {
	char			pending;
	char			busy;
	short			size;
	struct log_eventq	*next;
	log_event_upcall_arg_t	msg;
} log_eventq_t;


/*
 * Debug stuff
 */
#ifdef DEBUG
static int log_event_debug = 0;
#define	LOG_DEBUG(args)  if (log_event_debug) cmn_err args
#define	LOG_DEBUG1(args)  if (log_event_debug > 1) cmn_err args
#else
#define	LOG_DEBUG(args)
#define	LOG_DEBUG1(args)
#endif

/*
 * Local static vars
 */
static log_eventq_t *log_eventq_head = NULL;
static log_eventq_t *log_eventq_tail = NULL;
static struct kmem_cache *log_event_cache;

/* log event delivery flag */
#define	LOGEVENT_DELIVERY_HOLD	0
#define	LOGEVENT_DELIVERY_OK	1
#define	LOGEVENT_DELIVERY_DUMP	2

static int log_event_delivery = LOGEVENT_DELIVERY_HOLD;

static char *logevent_door_upcall_filename = NULL;
static int logevent_door_upcall_filename_size;

static door_handle_t event_door = NULL;		/* Door for upcalls */

/*
 * async thread-related variables
 */
static kmutex_t log_event_mutex;
static kcondvar_t log_event_cv;
static kthread_id_t async_thread = NULL;

static int
log_event_upcall_lookup()
{
	int	error;

	ASSERT(mutex_owned(&log_event_mutex));

	if (event_door) {	/* Release our previous hold (if any) */
		door_ki_rele(event_door);
	}

	event_door = NULL;

	/*
	 * Locate the door used for upcalls
	 */
	if ((error =
	    door_ki_open(logevent_door_upcall_filename, &event_door)) != 0) {
		return (error);
	}

	return (0);
}

/*
 * Perform the upcall, check for rebinding errors
 * Assumes event to be a char buffer of LOGEVENT_BUFSIZE.
 * This buffer is reused to contain the result code returned by the upcall.
 */
static int
log_event_upcall(log_event_upcall_arg_t *msg)
{
	door_arg_t arg, save_arg;
	int	error;

	ASSERT(mutex_owned(&log_event_mutex));

	if ((log_event_delivery == LOGEVENT_DELIVERY_HOLD) ||
	    (logevent_door_upcall_filename == NULL)) {
		return (EAGAIN);
	}

	arg.rbuf = (char *)msg;
	arg.data_ptr = (char *)msg;
	arg.rsize = sizeof (log_event_upcall_arg_t);
	arg.data_size = sizeof (log_event_upcall_arg_t);
	arg.desc_ptr = NULL;
	arg.desc_num = 0;
	save_arg = arg;
	if ((event_door == NULL) &&
	    ((error = log_event_upcall_lookup()) != 0)) {
		LOG_DEBUG((CE_CONT,
		    "log_event_upcall: event_door error (%d)\n", error));

		return (error);
	}

	LOG_DEBUG((CE_CONT, "log_event_upcall:\n\t%s\n", msg->buf));

	/* release the mutex as door_ki_upcall() might block */
	mutex_exit(&log_event_mutex);

	if ((error = door_ki_upcall(event_door, &arg)) == EBADF) {
		LOG_DEBUG1((CE_CONT, "log_event_upcall: rebinding\n"));

		mutex_enter(&log_event_mutex);

		/* Server may have died. Try rebinding */
		if ((error = log_event_upcall_lookup()) == 0) {

			LOG_DEBUG((CE_CONT, "log_event_upcall: retrying\n"));

			arg = save_arg;

			mutex_exit(&log_event_mutex);

			error = door_ki_upcall(event_door, &arg);

		} else {
			LOG_DEBUG((CE_CONT, "log_event_upcall: error\n"));

			return (error);
		}
	}
	mutex_enter(&log_event_mutex);

	LOG_DEBUG1((CE_CONT, "log_event_upcall:\n\t"
		"error=%d rptr1=%p rptr2=%p dptr2=%p ret1=%x ret2=%x\n",
		error, (void *)msg, (void *)arg.rbuf,
		(void *)arg.data_ptr,
		*((int *)(arg.rbuf)), *((int *)(arg.data_ptr))));

	if (!error) {
		/*
		 * upcall was successfully executed. Check return code.
		 */
		error = *((int *)(arg.rbuf));
	}


	return (error);
}


/*
 * event delivery thread
 */
static void
log_event_deliver()
{
	log_eventq_t *q;
	int upcall_err;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &log_event_mutex, callb_generic_cpr,
				"logevent");

	mutex_enter(&log_event_mutex);

	for (;;) {
		/*
		 * take a look at the queue the first time
		 * through the loop at startup to deliver any buffered
		 * events.
		 */
		LOG_DEBUG1((CE_CONT, "log_event_deliver: "
			"head = %p, tail = %p\n",
			(void *)log_eventq_head, (void *)log_eventq_tail));

		q = log_eventq_head;
		upcall_err = 0;

		while (q->pending) {
			LOG_DEBUG1((CE_CONT,
			    "log_event_deliver: q = %p, pending %d\n",
			    (void *)q, q->pending));
			/*
			 * this is tricky:
			 * we release the mutex before the upcall
			 * so the list might change while being
			 * blocked in door_ki_upcall(). Since new
			 * log_eventq's might be added or empty
			 * log_eventq's might be filled. The busy
			 * flag prevents the current log_eventq to
			 * be piggybacked.
			 */
			q->busy = 1;

			if ((upcall_err =
			    log_event_upcall(&(q->msg))) != 0) {
				q->busy = 0;
				break;
			}
			q->busy = 0;
			q->pending = 0;
			if (q == log_eventq_tail) {
				break;
			}
			log_eventq_head = q = q->next;
		}

		switch (upcall_err) {
		case 0:
			/*
			 * Success. The queue is empty.
			 */
			break;
		case EAGAIN:
			/*
			 * Couldn't acquire lock during upcall
			 * without blocking or delivery is on hold.
			 * Don't do anything, the userland daemon
			 * will retry.
			 */
			LOG_DEBUG((CE_CONT, "log_event_deliver: EAGAIN\n"));
			break;
		default:
			LOG_DEBUG((CE_CONT, "log_event_deliver: "
				"upcall err %d\n", upcall_err));
			break;
		}

		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		cv_wait(&log_event_cv, &log_event_mutex);
		CALLB_CPR_SAFE_END(&cprinfo, &log_event_mutex);
	}
}

/*
 * Allocate and initialize log_event data structures.
 */
void
log_event_init()
{
	mutex_init(&log_event_mutex, NULL, MUTEX_DRIVER, (void *)NULL);

	cv_init(&log_event_cv, NULL, CV_DEFAULT, NULL);

	/* initialize kmem cache for log_eventq */
	log_event_cache = kmem_cache_create("log_event_cache",
		sizeof (log_eventq_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	/* allocate first log_eventq entry */
	log_eventq_head = kmem_cache_alloc(log_event_cache, KM_SLEEP);
	bzero(log_eventq_head, sizeof (log_eventq_t));
	log_eventq_head->next = log_eventq_tail = log_eventq_head;
}

/*
 * Returns the total size of a given log_event message.
 */
static int
log_event_msglen(int ntuples, log_event_tuple_t tuples[])
{
	int i, attr_len, val_len, nbytes = 1;

	for (i = 0; i < ntuples; i++) {
		ASSERT(tuples[i].attr != NULL && tuples[i].val != NULL);
		attr_len = strlen(tuples[i].attr);
		val_len = strlen(tuples[i].val);
		if ((nbytes + attr_len + val_len + 1) > LOGEVENT_BUFSIZE) {
			return (-1);
		}
		nbytes += attr_len + 1 + val_len;
		if ((i + 1) <  ntuples) {
			/*
			 * if more args, add a semicolon
			 */
			nbytes++;
		}
	}
	return (nbytes);
}

/*
 * Find space on the log_eventq list for a msgsize message starting
 * at log_eventq_tail.
 * Piggyback into pending entry after insert the '|' separator.
 * Allocate new eventq structs as necessary.
 * Update pending field of returned eventq.
 * Returns NULL if allocation fails.
 */
static char *
log_eventq_getbuf(int msgsize, int flag)
{
	log_eventq_t *q, *new_q;
	char *bufptr;

	ASSERT(mutex_owned(&log_event_mutex));

	new_q = NULL;

	/*
	 * We should only go thru this loop twice:
	 * If all log_eventq's are in use then release the
	 * mutex and allocate a new entry. The list may
	 * change while we have released the mutex so we have
	 * to check again starting from the tail. If a new
	 * entry was allocated, it was inserted after the tail
	 */
	for (;;) {
		q = log_eventq_tail;

		if (!q->pending) {
			/*
			 * buffer is empty, our work here is done.
			 */
			break;

		} else if (q->pending && !q->busy) {
			/*
			 * We've got a non-empty/non-busy buffer,
			 * Check to see we fit.
			 */
			if ((q->size + msgsize + 1) <= LOGEVENT_BUFSIZE) {
				q->msg.buf[q->size-1] = '|';
				bufptr = &(q->msg.buf[q->size]);
				q->size += msgsize;

				return (bufptr);
			}
		}
		if ((q->next == log_eventq_head) || q->next->pending) {
			/*
			 * The queue is full.  Try to allocate and append
			 * from kmem cache.
			 * Release the mutex before potentially going
			 * to sleep (currently the normal case)
			 */
			mutex_exit(&log_event_mutex);
			if ((new_q = kmem_cache_alloc(
			    log_event_cache,
			    (flag ? KM_NOSLEEP:KM_SLEEP))) != NULL) {

				bzero(new_q, sizeof (log_eventq_t));
				mutex_enter(&log_event_mutex);

				/*
				 * insert after current tail which
				 * may have changed
				 */
				new_q->next = log_eventq_tail->next;
				log_eventq_tail->next = new_q;

				/* check again starting from tail */
				continue;
			} else {

				mutex_enter(&log_event_mutex);
				return (NULL);
			}
		} else {
			log_eventq_tail = q = log_eventq_tail->next;
			break;
		}
	}

	q->pending = 1;
	q->size = msgsize;
	return (q->msg.buf);
}

/*
 * i_ddi_log_event_flushq:
 *	create log_event_deliver thread if necessary or
 *	wake it up
 */
/*ARGSUSED*/
void
i_ddi_log_event_flushq(int cmd, uint_t flag)
{
	mutex_enter(&log_event_mutex);
	if (!async_thread) {
		async_thread = thread_create(NULL, 0, log_event_deliver,
					0, 0, &p0, TS_RUN, 60);
	}
	if (cmd == MODEVENTS_FLUSH_DUMP) {
		/* dump all future events */
		log_event_delivery = LOGEVENT_DELIVERY_DUMP;
	} else if (cmd == MODEVENTS_FLUSH) {
		log_event_delivery = LOGEVENT_DELIVERY_OK;
	}

	cv_signal(&log_event_cv);
	mutex_exit(&log_event_mutex);
}

/*
 * i_ddi_log_event:
 *	Kernel event logger interface function.
 *
 * Returns DDI_FAILURE if a message is too long or buf allocation failed
 * Returns DDI_SUCCESS otherwise.
 */
int
i_ddi_log_event(int argc, log_event_tuple_t tuples[], int flag)
{

	char	*bufptr, *buf;
	char	timestr[32];
	int	msglen, timelen;
	int	nbytes = 1, attr_len, val_len;
	int	i;
	time_t	time;

	if (log_event_delivery == LOGEVENT_DELIVERY_DUMP) {
		return (DDI_SUCCESS);
	}

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
	time = ddi_get_time();
	numtos(time, timestr);
	timelen = strlen(timestr) + 1;
	msglen = log_event_msglen(argc, tuples);
	if ((msglen < 0) || (msglen + timelen) > LOGEVENT_BUFSIZE) {

		return (DDI_FAILURE);
	} else {
		msglen += timelen;
	}

	mutex_enter(&log_event_mutex);
	if ((buf = bufptr = log_eventq_getbuf(msglen, flag)) == NULL) {
		cv_signal(&log_event_cv);
		mutex_exit(&log_event_mutex);
		LOG_DEBUG((CE_WARN, "i_ddi_log_event alloc failure\n"));

		return (DDI_FAILURE);
	}

	/*
	 * prepend ascii-fied timestamp.
	 */
	(void) sprintf(bufptr, "%s;", timestr);
	bufptr += timelen;
	*bufptr = (char)'\0';
	for (i = 0; i < argc; i++) {
		ASSERT(tuples[i].attr != NULL && tuples[i].val != NULL);
		attr_len = strlen(tuples[i].attr);
		val_len = strlen(tuples[i].val);
		nbytes += attr_len + 1 + val_len;
		(void) strcpy(bufptr, tuples[i].attr);
		bufptr += attr_len;
		*bufptr++ = '=';
		(void) strcpy(bufptr, tuples[i].val);
		bufptr += val_len;
		if ((i + 1) <  argc) {
			/*
			 * if more args, add a semicolon
			 */
			*bufptr = ';';
			bufptr++;
			nbytes++;
		} else {
			/*
			 * This is the last attr=val pair
			 */
			*bufptr = (char)'\0';
		}
	}

	LOG_DEBUG1((CE_CONT, "i_ddi_log_event: argc=%d buf=%p:\n\t%s\n",
			argc, (void *)buf, buf));

	cv_signal(&log_event_cv);
	mutex_exit(&log_event_mutex);

	return (DDI_SUCCESS);
}

#ifdef FORTESTING
int
i_ddi_log_event(int argc, log_event_tuple_t tuples[], int flag)
{
	int i;

	for (i = 0; i < 30; i++) {
		ii_ddi_log_event(argc, tuples, flag);
	}
	return (DDI_SUCCESS);
}
#endif

/*ARGSUSED1*/
int
i_ddi_log_event_filename(char *file, uint_t flag)
{
	int rval = 0;

	mutex_enter(&log_event_mutex);
	if (logevent_door_upcall_filename) {
		kmem_free(logevent_door_upcall_filename,
			logevent_door_upcall_filename_size);
		if (event_door) {
			door_ki_rele(event_door);
			event_door = NULL;
		}
	}
	logevent_door_upcall_filename_size = strlen(file) + 1;
	logevent_door_upcall_filename = kmem_alloc(
		logevent_door_upcall_filename_size, KM_SLEEP);
	(void) strcpy(logevent_door_upcall_filename, file);
	mutex_exit(&log_event_mutex);

	return (rval);
}
