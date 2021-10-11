/*
 * Copyright (c) 1986-1996 by Sun Microsystems Inc.
 * All rights reserved.
 */
#ident	"@(#)svc_run.c	1.34	99/12/17 SMI"

/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */
#include "rpc_mt.h"
#include <stdlib.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <syslog.h>
#include <thread.h>
#include <assert.h>
#include <libintl.h>
#include <values.h>

extern const char __nsl_dom[];

extern int __rpc_compress_pollfd(int, pollfd_t *, pollfd_t *);
extern void clear_pollfd(int);
extern void set_pollfd(int);
extern void svc_getreq_poll();
extern void (*__proc_cleanup_cb)();

static void start_threads();
static void create_pipe();
static void clear_pipe();
static int select_next_pollfd();
static SVCXPRT *make_xprt_copy();
static int get_concurrency();
static void _svc_run_mt();
static void _svc_run();

int _svc_prog_dispatch();
static void _svc_done_private();

extern rwlock_t svc_fd_lock;
extern mutex_t	svc_door_mutex;
extern cond_t	svc_door_waitcv;
extern int	svc_ndoorfds;
extern void	__svc_cleanup_door_xprts();
extern void	__svc_free_xprtlist();

/*
 * Maximum fragment size allowed for connection oriented connections.
 * Zero means that no maximum size limit has been requested.
 */
int __rpc_connmaxrec = 0;

/*
 * XXX - eventually, all mutexes and their initializations static
 */

/*
 * Variables used for MT
 */
int svc_mt_mode;		/* multi-threading mode */

int svc_pipe[2];	/* pipe for breaking out of poll: read(0), write(1) */

/* BEGIN PROTECTED BY svc_mutex */

static int svc_thr_max = 16;	/* default maximum number of threads allowed */

static int svc_max_lwps;	/* maximum concurrency */

static int svc_thr_total;	/* current number of threads */

static int svc_thr_active;	/* current number of threads active */

/* circular array of file descriptors with pending data */

#define	CIRCULAR_BUFSIZE	1024

static int svc_pending_fds[CIRCULAR_BUFSIZE+1];	/* fds with pending data */

static int svc_next_pending;			/* next one to be processed */

static int svc_last_pending;			/* last one in list */

static int svc_total_pending;			/* total in list */

static int svc_thr_total_creates;	/* total created - stats */

static int svc_thr_total_create_errors;	/* total create errors - stats */

static int svc_cur_lwps = 1;	/* current number of lwps allocated by server */

static int svc_waiters;		/* number of waiting threads */

/* END PROTECTED BY svc_mutex */

/* BEGIN PROTECTED BY svc_fd_lock: */

int svc_nfds;		/* total number of active file descriptors */

int svc_nfds_set;	/* total number of fd bits set in svc_fdset */

int svc_max_fd = 0;	/* largest active file descriptor */

int svc_npollfds;	/* total number of active pollfds */

int svc_npollfds_set;	/* total number of pollfd set in svc_pollfd */

int svc_max_pollfd;	/* largest active pollfd so far */

int svc_pollfd_allocd;  /* number of pollfd structures allocated */

/* END PROTECTED BY svc_fd_lock: */

/* BEGIN PROTECTED BY svc_thr_mutex */

#define	POLLSET_EXTEND	256
static int svc_pollset_allocd;
static struct pollfd *svc_pollset;
				/*
				 * array of file descriptors currently active
				 */
static int svc_polled;		/* no of fds polled in last poll() - input */

static int svc_pollfds;		/* no of active fds in last poll() - output */

static int svc_next_pollfd;	/* next fd  to processin svc_pollset */

bool_t svc_polling;		/* true if a thread is polling */

/* END PROTECTED BY svc_thr_mutex */

/* BEGIN PROTECTED BY svc_exit_mutex */

static bool_t svc_exit_done = TRUE;

/* END PROTECTED BY svc_exit_mutex */

/*
 * Warlock section
 */

/* VARIABLES PROTECTED BY svc_mutex:
	svc_thr_total, svc_thr_active, svc_pending_fds, svc_next_pending,
	svc_last_pending, svc_total_pending, svc_thr_total_creates,
	svc_thr_total_create_errors, svc_cur_lwps,
	svcxprt_list_t::next, svcxprt_ext_t::my_xlist,
	svc_thr_max, svc_max_lwps, svc_waiters
 */

/* VARIABLES PROTECTED BY svc_fd_lock:
	svc_xports, svc_fdset, svc_nfds, svc_nfds_set, svc_max_fd,
	svc_pollfd, svc_npollfds, svc_npollfds_set, svc_max_pollfd
 */

/* VARIABLES PROTECTED BY svc_thr_mutex:
	svc_pollset, svc_pollfds, svc_next_pollfd, svc_polling
	svc_pollset_allocd, svc_polled
 */

/* VARIABLES PROTECTED BY svc_exit_mutex:
	svc_exit_done
 */

/* VARIABLES READABLE WITHOUT LOCK:
	svc_thr_total, svc_thr_active, svc_thr_total_creates,
	svc_thr_total_create_errors, svc_cur_lwps,
	svc_xports, svc_nfds, svc_nfds_set, svc_max_fd,
	svc_npollfds, svc_npollfds_set, svc_max_pollfd,
	svc_pollfds, svc_next_pollfd, svc_exit_done, svc_polling,
	svc_thr_max, svc_max_lwps, svc_waiters
 */

/* VARIABLES PROTECTED BY "program_logic":
	rpc_msg::, svc_req::, svcxprt_ext_t::flags, svc_mt_mode,
	svcxprt_ext_t::parent
 */

/* LOCK ORDER:
	svc_exit_mutex, svc_thr_mutex, svc_mutex, svc_fd_lock
 */


void
svc_run()
{
	/* NO OTHER THREADS ARE RUNNING */

	svc_exit_done = FALSE;

	while ((svc_npollfds > 0 || svc_ndoorfds > 0) && !svc_exit_done) {
		if (svc_npollfds > 0) {
			switch (svc_mt_mode) {
			case RPC_SVC_MT_NONE:
				_svc_run();
				break;
			default:
				_svc_run_mt();
				break;
			}
			continue;
		}

		mutex_lock(&svc_door_mutex);
		if (svc_ndoorfds > 0)
			cond_wait(&svc_door_waitcv, &svc_door_mutex);
		mutex_unlock(&svc_door_mutex);
	}
}


/*
 *	This function causes svc_run() to exit by destroying all
 *	service handles.
 */
void
svc_exit()
{
	SVCXPRT	*xprt;
	int fd;
	char dummy;

	/* NO LOCKS HELD */

	trace1(TR_svc_exit, 0);
	mutex_lock(&svc_exit_mutex);
	if (svc_exit_done) {
		mutex_unlock(&svc_exit_mutex);
		trace1(TR_svc_exit, 1);
		return;
	}
	svc_exit_done = TRUE;
	for (fd = 0; fd < svc_max_pollfd; fd++) {
		xprt = svc_xports[fd];
		if (xprt) {
			SVC_DESTROY(xprt);
		}
	}
	__svc_free_xprtlist();
	__svc_cleanup_door_xprts();
	mutex_unlock(&svc_exit_mutex);

	if (svc_mt_mode != RPC_SVC_MT_NONE) {
		mutex_lock(&svc_mutex);
		cond_broadcast(&svc_thr_fdwait);
		mutex_unlock(&svc_mutex);

		(void) write(svc_pipe[1], &dummy, sizeof (dummy));
	}

	mutex_lock(&svc_door_mutex);
	cond_signal(&svc_door_waitcv);	/* wake up door dispatching */
	mutex_unlock(&svc_door_mutex);

	trace1(TR_svc_exit, 1);
}


/*
 * this funtion is called with svc_fd_lock and svc_thr_mutex
 */

static int
alloc_pollset(int npollfds)
{
	if (npollfds > svc_pollset_allocd) {
		pollfd_t *tmp;
		do {
			svc_pollset_allocd += POLLSET_EXTEND;
		} while (npollfds > svc_pollset_allocd);
		tmp = realloc(svc_pollset,
				sizeof (pollfd_t) * svc_pollset_allocd);
		if (tmp == NULL) {
			syslog(LOG_ERR, "alloc_pollset: out of memory");
			return (-1);
		}
		svc_pollset = tmp;
	}
	return (0);
}

static void
_svc_run()
{
	int npollfds;
	int i;

	trace1(TR_svc_run, 0);
	while (!svc_exit_done) {
		/*
		 * Check whether there is any server fd on which we may want
		 * to wait.
		 */
		rw_rdlock(&svc_fd_lock);
		if (alloc_pollset(svc_npollfds) == -1)
			break;
		npollfds = __rpc_compress_pollfd(svc_max_pollfd,
			svc_pollfd, svc_pollset);
		rw_unlock(&svc_fd_lock);
		if (npollfds == 0)
			break;	/* None waiting, hence return */

		switch (i = poll(svc_pollset, npollfds, -1)) {
		case -1:
			/*
			 * We ignore all errors, continuing with the assumption
			 * that it was set by the signal handlers (or any
			 * other outside event) and not caused by poll().
			 */
		case 0:
			continue;
		default:
			svc_getreq_poll(svc_pollset, i);
		}
	}
	trace1(TR_svc_run, 1);
}

static void
_svc_run_mt()
{
	int npollfds;
	int n_polled, dispatch;

	static bool_t first_time = TRUE;
	bool_t main_thread = FALSE;
	int n_new;
	int myfd;
	SVCXPRT *parent_xprt, *xprt;

	/*
	 * Server is multi-threaded.  Do "first time" initializations.
	 * Since only one thread exists in the beginning, there's no
	 * need for mutex protection for first time initializations.
	 */
	if (first_time) {
		first_time = FALSE;
		main_thread = TRUE;
		svc_thr_total = 1;	/* this thread */
		svc_next_pending = svc_last_pending = 0;
		svc_max_lwps = get_concurrency(svc_thr_max);

		/*
		 * Create a pipe for waking up the poll, if new
		 * descriptors have been added to svc_fdset.
		 */
		create_pipe();
	}

	/* OTHER THREADS ARE RUNNING */

	if (svc_exit_done)
		return;

	for (;;) {
		/*
		 * svc_thr_mutex prevents more than one thread from
		 * trying to select a descriptor to process further.
		 * svc_thr_mutex is unlocked after a thread selects
		 * a descriptor on which to receive data.  If there are
		 * no such descriptors, the thread will poll with
		 * svc_thr_mutex locked, after unlocking all other
		 * locks.  This prevents more than one thread from
		 * trying to poll at the same time.
		 */
		mutex_lock(&svc_thr_mutex);
		mutex_lock(&svc_mutex);
continue_with_locks:
		myfd = -1;

		/*
		 * Check if there are any descriptors with data pending.
		 */
		if (svc_total_pending > 0) {
			myfd = svc_pending_fds[svc_next_pending++];
			if (svc_next_pending > CIRCULAR_BUFSIZE)
				svc_next_pending = 0;
			svc_total_pending--;
		}

		/*
		 * Get the next active file descriptor to process.
		 */
		if (myfd == -1 && svc_pollfds == 0) {
			/*
			 * svc_pollset is empty; do polling
			 */
			svc_polling = TRUE;

			/*
			 * if there are no file descriptors, return
			 */
			rw_rdlock(&svc_fd_lock);
			if (svc_npollfds == 0 ||
					alloc_pollset(svc_npollfds + 1) == -1) {
				rw_unlock(&svc_fd_lock);
				svc_polling = FALSE;
				svc_thr_total--;
				mutex_unlock(&svc_mutex);
				mutex_unlock(&svc_thr_mutex);
				if (!main_thread) {
					thr_exit(NULL);
					/* NOTREACHED */
				}
				break;
			}

			npollfds = __rpc_compress_pollfd(svc_max_pollfd,
					svc_pollfd, svc_pollset);
			rw_unlock(&svc_fd_lock);

			if (npollfds == 0) {
				/*
				 * There are file descriptors, but none of them
				 * are available for polling.  If this is the
				 * main thread, or if no thread is waiting,
				 * wait on condition variable, otherwise exit.
				 */
				svc_polling = FALSE;
				mutex_unlock(&svc_thr_mutex);
				if ((!main_thread) && svc_waiters > 0) {
					svc_thr_total--;
					mutex_unlock(&svc_mutex);
					thr_exit(NULL);
					/* NOTREACHED */
				}

				while (svc_npollfds_set == 0 &&
					svc_pollfds == 0 &&
					svc_total_pending == 0 &&
							!svc_exit_done) {
					svc_waiters++;
					cond_wait(&svc_thr_fdwait, &svc_mutex);
					svc_waiters--;
				}

				/*
				 * Check exit flag.  If this is not the main
				 * thread, exit.
				 */
				if (svc_exit_done) {
					svc_thr_total--;
					mutex_unlock(&svc_mutex);
					if (!main_thread)
						thr_exit(NULL);
					break;
				}

				mutex_unlock(&svc_mutex);
				continue;
			}

			/*
			 * We're ready to poll.  Always set svc_pipe[0]
			 * as the last one, since the poll will occasionally
			 * need to be interrupted.  Release svc_mutex for
			 * the duration of the poll, but hold on to
			 * svc_thr_mutex, as we don't want any other thread
			 * to do the same.
			 */
			svc_pollset[npollfds].fd = svc_pipe[0];
			svc_pollset[npollfds].events = MASKVAL;

			do {
				mutex_unlock(&svc_mutex);
				n_polled = poll(svc_pollset, npollfds + 1, -1);
				mutex_lock(&svc_mutex);
			} while (n_polled <= 0);
			svc_polling = FALSE;

			/*
			 * If there's data in the pipe, clear it.
			 */
			if (svc_pollset[npollfds].revents) {
				clear_pipe();
				n_polled--;
				svc_pollset[npollfds].revents = 0;
			}
			svc_polled = npollfds;
			svc_pollfds = n_polled;
			svc_next_pollfd = 0;

			/*
			 * Check exit flag.
			 */
			if (svc_exit_done) {
				svc_thr_total--;
				mutex_unlock(&svc_mutex);
				mutex_unlock(&svc_thr_mutex);
				if (!main_thread) {
					thr_exit(NULL);
					/* NOTREACHED */
				}
				break;
			}

			/*
			 * If no descriptor is active, continue.
			 */
			if (svc_pollfds == 0)
				goto continue_with_locks;
		}

		/*
		 * If a file descriptor has already not been selected,
		 * choose a file descriptor.
		 * svc_pollfds and svc_next_pollfd are updated.
		 */
		if (myfd == -1) {
			if ((myfd = select_next_pollfd()) == -1)
				goto continue_with_locks;
		}

		/*
		 * Check to see if new threads need to be started.
		 * Count of threads that could be gainfully employed is
		 * obtained as follows:
		 *	- count 1 for poller
		 *	- count 1 for this request
		 *	- count active file descriptors (svc_pollfds)
		 *	- count pending file descriptors
		 *
		 * (svc_thr_total - svc_thr_active) are already available.
		 * This thread is one of the available threads.
		 *
		 * Number of new threads should not exceed
		 *	(svc_thr_max - svc_thr_total).
		 */
		if (svc_thr_total < svc_thr_max &&
			    svc_mt_mode == RPC_SVC_MT_AUTO && !svc_exit_done) {
			n_new = 1 + 1 + svc_pollfds + svc_total_pending -
					(svc_thr_total - svc_thr_active);
			if (n_new > (svc_thr_max - svc_thr_total))
				n_new = svc_thr_max - svc_thr_total;
			if (n_new > 0)
				start_threads(n_new);
		}

		/*
		 * Get parent xprt.  It is possible for the parent service
		 * handle to be destroyed by now, due to a race condition.
		 * Check for this, and if so, log a warning and go on.
		 */
		parent_xprt = svc_xports[myfd];
		if (parent_xprt == NULL ||
/* LINTED pointer alignment */
		    svc_defunct(parent_xprt) || svc_failed(parent_xprt))
			goto continue_with_locks;

		/*
		 * Make a copy of parent xprt, update svc_fdset.
		 */
		if ((xprt = make_xprt_copy(parent_xprt)) == NULL)
			goto continue_with_locks;

		/*
		 * Keep track of active threads in automatic mode.
		 */
		if (svc_mt_mode == RPC_SVC_MT_AUTO)
			svc_thr_active++;

		/*
		 * Release mutexes so other threads can get going.
		 */
		mutex_unlock(&svc_mutex);
		mutex_unlock(&svc_thr_mutex);

		/*
		 * Process request.
		 */
		{
			struct rpc_msg *msg;
			struct svc_req *r;
			char *cred_area;

/* LINTED pointer alignment */
			msg = SVCEXT(xprt)->msg;
/* LINTED pointer alignment */
			r = SVCEXT(xprt)->req;
/* LINTED pointer alignment */
			cred_area = SVCEXT(xprt)->cred_area;


			msg->rm_call.cb_cred.oa_base = cred_area;
			msg->rm_call.cb_verf.oa_base =
						&(cred_area[MAX_AUTH_BYTES]);
			r->rq_clntcred = &(cred_area[2 * MAX_AUTH_BYTES]);

			/*
			 * receive RPC message
			 */
			if ((dispatch = SVC_RECV(xprt, msg))) {
				if (svc_mt_mode != RPC_SVC_MT_NONE)
/* LINTED pointer alignment */
					svc_flags(xprt) |= SVC_ARGS_CHECK;
				dispatch = _svc_prog_dispatch(xprt, msg, r);

				/*
				 * Call cleanup procedure if set.
				 */
				if (__proc_cleanup_cb != NULL)
					(*__proc_cleanup_cb)(xprt);
			} else
				svc_args_done(xprt);

			/*
			 * Finish up, if automatic mode, or not dispatched.
			 */
			if (svc_mt_mode == RPC_SVC_MT_AUTO || !dispatch) {
/* LINTED pointer alignment */
				if (svc_flags(xprt) & SVC_ARGS_CHECK)
					svc_args_done(xprt);
				mutex_lock(&svc_mutex);
				_svc_done_private(xprt);
				if (svc_mt_mode == RPC_SVC_MT_AUTO) {
					/*
					 * not active any more
					 */
					svc_thr_active--;

					/*
					 * If not main thread, exit unless
					 * there's some immediate work.
					 */
					if (!main_thread &&
						    svc_pollfds <= 0 &&
						    svc_total_pending <= 0 &&
						    (svc_polling ||
							svc_waiters > 0)) {
						svc_thr_total--;
						mutex_unlock(&svc_mutex);
						thr_exit(NULL);
						/* NOTREACHED */
					}
				}
				mutex_unlock(&svc_mutex);
			}
		}

	}
}


/*
 * start_threads() - Start specified number of threads.
 */
static void
start_threads(num_threads)
	int		num_threads;
{
	int		i;
	long		flags;
	thread_t	new_thread;

	assert(MUTEX_HELD(&svc_mutex));

	for (i = 0; i < num_threads; i++) {
		if (svc_cur_lwps < svc_max_lwps)
			flags = THR_DETACHED | THR_NEW_LWP;
		else
			flags = THR_DETACHED;
		if (thr_create(NULL, 0, (void *(*)(void *))_svc_run_mt, NULL,
					flags, &new_thread) == 0) {
			if (flags & THR_NEW_LWP)
				svc_cur_lwps++;
			svc_thr_total++;
			svc_thr_total_creates++;
		} else
			svc_thr_total_create_errors++;
	}
}


/*
 * create_pipe() - create pipe for breaking out of poll.
 */
static void
create_pipe()
{
	if (pipe(svc_pipe) == -1) {
		syslog(LOG_ERR, dgettext(__nsl_dom,
				"RPC: svc could not create pipe - exiting"));
		exit(1);
	}
	if (_fcntl(svc_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
		syslog(LOG_ERR, dgettext(__nsl_dom,
					"RPC: svc pipe error - exiting"));
		exit(1);
	}
	if (_fcntl(svc_pipe[1], F_SETFL, O_NONBLOCK) == -1) {
		syslog(LOG_ERR, dgettext(__nsl_dom,
					"RPC: svc pipe error - exiting"));
		exit(1);
	}
}


/*
 * clear_pipe() - Empty data in pipe.
 */
static void
clear_pipe()
{
	char	buf[16];
	int	i;

	do {
		i = read(svc_pipe[0], buf, sizeof (buf));
	} while (i == sizeof (buf));
}


/*
 * select_next_pollfd() - Select the next active fd in svc_pollset.
 */
static int
select_next_pollfd()
{
	int i;

	assert(MUTEX_HELD(&svc_thr_mutex));
	assert(MUTEX_HELD(&svc_mutex));

	for (i = svc_next_pollfd; svc_pollfds > 0 && i < svc_polled;
							i++) {
		if (svc_pollset[i].revents) {
			svc_pollfds--;
			if (svc_pollset[i].revents & POLLNVAL) {
				/*
				 * This can happen if a descriptor is closed.
				 * This should be already handled.
				 */
				continue;
			}
			svc_next_pollfd = i + 1;
			return (svc_pollset[i].fd);
		}
	}
	svc_next_pollfd = svc_pollfds = 0;
	return (-1);
}


/*
 * make_xprt_copy() - make a copy of the parent xprt.
 * Clear fd bit in svc_fdset.
 */
static SVCXPRT *
make_xprt_copy(parent)
	SVCXPRT	*parent;
{
/* LINTED pointer alignment */
	SVCXPRT_LIST	*xlist = SVCEXT(parent)->my_xlist;
	SVCXPRT_LIST	*xret;
	SVCXPRT		*xprt;
	int		fd = parent->xp_fd;

	assert(MUTEX_HELD(&svc_mutex));

	xret = xlist->next;
	if (xret) {
		xlist->next = xret->next;
		xret->next = NULL;
		xprt = xret->xprt;
/* LINTED pointer alignment */
		svc_flags(xprt) = svc_flags(parent);
	} else
		xprt = svc_copy(parent);

	if (xprt) {
/* LINTED pointer alignment */
		SVCEXT(parent)->refcnt++;
		rw_wrlock(&svc_fd_lock);
		clear_pollfd(fd);
		rw_unlock(&svc_fd_lock);
	}
	return (xprt);
}

/*
 * _svc_done_private() - return copies to library.
 */
static void
_svc_done_private(xprt)
	SVCXPRT		*xprt;
{
	SVCXPRT		*parent;
	SVCXPRT_LIST	*xhead, *xlist;

	assert(MUTEX_HELD(&svc_mutex));

/* LINTED pointer alignment */
	if ((parent = SVCEXT(xprt)->parent) == NULL)
		return;

/* LINTED pointer alignment */
	xhead = SVCEXT(parent)->my_xlist;
/* LINTED pointer alignment */
	xlist = SVCEXT(xprt)->my_xlist;
	xlist->next = xhead->next;
	xhead->next = xlist;
/* LINTED pointer alignment */
	SVCEXT(parent)->refcnt--;
/* LINTED pointer alignment */
	svc_flags(xprt) |= svc_flags(parent);
/* LINTED pointer alignment */
	if (SVCEXT(parent)->refcnt == 0 && (svc_failed(xprt) ||
/* LINTED pointer alignment */
						svc_defunct(xprt)))
		_svc_destroy_private(xprt);
}

void
svc_done(xprt)
	SVCXPRT		*xprt;
{
	if (svc_mt_mode != RPC_SVC_MT_USER)
		return;

	/*
	 * Make sure file descriptor is released in user mode.
	 */
/* LINTED pointer alignment */
	if (svc_flags(xprt) & SVC_ARGS_CHECK)
		svc_args_done(xprt);

	mutex_lock(&svc_mutex);
	_svc_done_private(xprt);
	mutex_unlock(&svc_mutex);
}


/*
 * Mark argument completion.  Release file descriptor.
 */
void
svc_args_done(xprt)
	SVCXPRT	*xprt;
{
	char	dummy;
/* LINTED pointer alignment */
	SVCXPRT	*parent = SVCEXT(xprt)->parent;
	bool_t	wake_up_poller;
	enum	xprt_stat stat;

/* LINTED pointer alignment */
	svc_flags(xprt) |= svc_flags(parent);
/* LINTED pointer alignment */
	svc_flags(xprt) &= ~SVC_ARGS_CHECK;
/* LINTED pointer alignment */
	if (svc_failed(xprt) || svc_defunct(parent))
		return;

/* LINTED pointer alignment */
	if (svc_type(xprt) == SVC_CONNECTION &&
				(stat = SVC_STAT(xprt)) != XPRT_IDLE) {
		if (stat == XPRT_MOREREQS) {
			mutex_lock(&svc_mutex);
			svc_pending_fds[svc_last_pending++] = xprt->xp_fd;
			if (svc_last_pending > CIRCULAR_BUFSIZE)
				svc_last_pending = 0;
			svc_total_pending++;
			mutex_unlock(&svc_mutex);
			wake_up_poller = FALSE;
		} else {
			/*
			 * connection failed
			 */
			return;
		}
	} else {
		rw_wrlock(&svc_fd_lock);
		set_pollfd(xprt->xp_fd);
		rw_unlock(&svc_fd_lock);
		wake_up_poller = TRUE;
	}

	if (!wake_up_poller || !svc_polling) {
		/*
		 * Wake up any waiting threads.
		 */
		mutex_lock(&svc_mutex);
		if (svc_waiters > 0) {
			cond_broadcast(&svc_thr_fdwait);
			mutex_unlock(&svc_mutex);
			return;
		}
		mutex_unlock(&svc_mutex);
	}

	/*
	 * Wake up any polling thread.
	 */
	if (svc_polling)
		(void) write(svc_pipe[1], &dummy, sizeof (dummy));
}


int
__rpc_legal_connmaxrec(int suggested) {
	if (suggested == -1) {
		/* Supply default */
		return (RPC_MAXDATASIZE + 2*sizeof (uint32_t));
	} else if (suggested < 0) {
		return (-1);
	} else if (suggested > 0) {
		/* Round down to multiple of BYTES_PER_XDR_UNIT */
		suggested -= suggested % BYTES_PER_XDR_UNIT;
		/* If possible, allow for two fragment headers */
		if (suggested < MAXINT-(2*sizeof (uint32_t))) {
			/* Allow for two fragment headers */
			suggested += 2 * sizeof (uint32_t);
		} else {
			suggested = MAXINT;
		}
		if (suggested < sizeof (struct rpc_msg)) {
			return (-1);
		}
	}
	return (suggested);
}


bool_t
rpc_control(op, info)
	int		op;
	void		*info;
{
	int		tmp;
	extern int	__rpc_minfd;

	switch (op) {
	case RPC_SVC_MTMODE_SET:
		tmp = *((int *)info);
		if (tmp != RPC_SVC_MT_NONE && tmp != RPC_SVC_MT_AUTO &&
						tmp != RPC_SVC_MT_USER)
			return (FALSE);
		if (svc_mt_mode != RPC_SVC_MT_NONE && svc_mt_mode != tmp)
			return (FALSE);
		svc_mt_mode = tmp;
		return (TRUE);
	case RPC_SVC_MTMODE_GET:
		*((int *)info) = svc_mt_mode;
		return (TRUE);
	case RPC_SVC_THRMAX_SET:
		if ((tmp = *((int *)info)) < 1)
			return (FALSE);
		mutex_lock(&svc_mutex);
		svc_thr_max = tmp;
		svc_max_lwps = get_concurrency(svc_thr_max);
		mutex_unlock(&svc_mutex);
		return (TRUE);
	case RPC_SVC_THRMAX_GET:
		*((int *)info) = svc_thr_max;
		return (TRUE);
	case RPC_SVC_THRTOTAL_GET:
		*((int *)info) = svc_thr_total;
		return (TRUE);
	case RPC_SVC_THRCREATES_GET:
		*((int *)info) = svc_thr_total_creates;
		return (TRUE);
	case RPC_SVC_THRERRORS_GET:
		*((int *)info) = svc_thr_total_create_errors;
		return (TRUE);
	case RPC_SVC_USE_POLLFD:
		if (*((int *)info) && !__rpc_use_pollfd_done) {
			__rpc_use_pollfd_done = 1;
			return (TRUE);
		}
		return (FALSE);
	case __RPC_CLNT_MINFD_SET:
		tmp = *((int *)info);
		if (tmp < 0)
			return (FALSE);
		__rpc_minfd = tmp;
		return (TRUE);
	case __RPC_CLNT_MINFD_GET:
		*((int *)info) = __rpc_minfd;
		return (TRUE);
	case RPC_SVC_CONNMAXREC_SET:
		tmp = __rpc_legal_connmaxrec(*(int *)info);
		if (tmp >= 0) {
			__rpc_connmaxrec = tmp;
			return (TRUE);
		} else {
			return (FALSE);
		}
	case RPC_SVC_CONNMAXREC_GET:
		*((int *)info) = __rpc_connmaxrec;
		return (TRUE);
	default:
		return (FALSE);
	}
}


/*
 * XXX - The following procedure is used to obtain the default concurrency.
 * This may not be right for all applications.  At some point in the future,
 * we may need to change this, and/or augment this with an interface to
 * set concurrency.
 */
static int
get_concurrency(nthreads)
	int	nthreads;
{
	if (nthreads <= 1)
		return (1);
	if (nthreads < 4)
		return (2);
	if (nthreads < 16)
		return (3);
	if (nthreads < 32)
		return (4);
	if (nthreads < 64)
		return (5);
	if (nthreads < 128)
		return (8);
	if (nthreads < 256)
		return (12);
	if (nthreads < 512)
		return (16);
	return (20);
}
