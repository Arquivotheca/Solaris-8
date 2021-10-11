/*
 *	nis_callback.c
 *
 *	Copyright (c) 1988-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)nis_callback.c	1.26	98/10/30 SMI"

/*
 *	nis_callback.c
 *
 *	This module contains the functions that implement the callback
 *	facility. They are RPC library dependent.
 *
 * 	These callback functions set up and run the callback
 * 	facility of NIS+. The idea is simple, a psuedo service is created
 * 	by the client and registered with the portmapper. The program number
 * 	for that service is included in the request as is the principal
 * 	name of the _host_ where the request is being made. The server
 * 	then does rpc calls to that service to return results.
 */

#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <tiuser.h>
#include <netdir.h>
#include <sys/netconfig.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <rpc/types.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/nis_callback.h>
#include <thread.h>
#include <synch.h>
#include "nis_clnt.h"
#include "nis_local.h"

#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

#define	CB_MAXENDPOINTS 16

/*
 * Multi-threaded and Reentrant callback support:
 *
 * Eventually we would like to support simultaneous callbacks from
 * multiple threads and callbacks within call backs.  At the moment we
 * don't/can't.  Much of the problem lies with the rpc system, which
 * does not support either multi-threaded or reentrant calls from rpc
 * servers.  Also note that the use of thread global data in this file
 * precludes reentrant callbacks.  Since these things cannot easily be
 * made to work, we use a mutex lock to ensure that only one use of
 * callbacks is taking place at a time.  The lock must be held around
 * all calls to __nis_core_lookup and nis_dump_r which do callbacks,
 * as those functions are the ones which call __nis_init_callback and
 * __nis_run_callback.  The lock is defined here.
 */
mutex_t __nis_callback_lock = DEFAULTMUTEX;


/*
 * __cbdata is the internal state which the callback routines maintain
 * for clients. It is stored as a structure so that multithreaded clients
 * may eventually keep a static copy of this on their Thread local storage.
 */
static struct callback_data {
	nis_server 	cbhost;
	char		pkey_data[1024];
	endpoint	cbendp[CB_MAXENDPOINTS];
	SVCXPRT		*cbsvc[CB_MAXENDPOINTS];
	bool_t		complete;
	int		results;
	pid_t		cbpid;
	nis_error	cberror;
	void		*cbuser;
	int		(*cback)();
};

/*
 * Static function prototypes.
 */
static void
__do_callback(struct svc_req *, SVCXPRT *);

static bool_t
__callback_stub(cback_data *, struct svc_req *, struct callback_data *, int *);

static bool_t
__callback_finish(void *, struct svc_req *, struct callback_data *, int *);

static bool_t
__callback_error(nis_error *, struct svc_req *, struct callback_data *, int *);

static char *__get_clnt_uaddr(CLIENT *);

static thread_key_t cbdata_key;
static struct callback_data __cbdata_main;

/*
 * Callback functions. These functions set up and run the callback
 * facility of NIS. The idea is simple, a psuedo service is created
 * by the client and registered with the portmapper. The program number
 * for that service is included in the request as is the principal
 * name of the _host_ where the request is being made. The server
 * then does rpc calls to that service to return results.
 */


static struct callback_data *my_cbdata(void)
{
	struct callback_data *__cbdata;

	if (_thr_main())
		return (&__cbdata_main);
	else
		thr_getspecific(cbdata_key, (void **) &__cbdata);
	return (__cbdata);
}

static void destroy_cbdata(void * cbdata)
{
	if (cbdata)
		free(cbdata);
}

static char *
__get_clnt_uaddr(CLIENT	*cl)
{
	struct netconfig	*nc;
	struct netbuf		addr;
	char			*uaddr;

	nc = getnetconfigent(cl->cl_netid);
	if (! nc)
		return (NULL);
	(void) clnt_control(cl, CLGET_SVC_ADDR, (char *)&addr);
	uaddr = taddr2uaddr(nc, &addr);
	freenetconfigent(nc);
	return (uaddr);
}

int
__nis_destroy_callback(void)
{
	struct callback_data *__cbdata;
	__cbdata = my_cbdata();

	if (!__cbdata)
		return (0);

	if (__cbdata->cbsvc[0]) {
		svc_destroy(__cbdata->cbsvc[0]);
		__cbdata->cbsvc[0] = NULL;
	}
	free(__cbdata);
	__cbdata = NULL;
	return (1);
}

/*
 * __nis_init_callback()
 * This function will initialize an RPC service handle for the
 * NIS client if one doesn't already exist. This handle uses a
 * "COTS" connection in a TIRPC.
 * The server will either fork, or generate a thread to send us
 * data and if the connection breaks we want to know (that means
 * the server died and we have to return an error). It returns
 * an endpoint if successful and NULL if it fails.
 *
 * NOTE : since we send the server the complete endpoint, including
 * universal address, transport, and family, it doesn't need to contact
 * our portmapper to find out what port we are using. Consequently we
 * don't bother registering with the portmapper, this saves us from having
 * to determine a unique program number.
 */
nis_server *
__nis_init_callback(
	CLIENT	*svc_clnt,	/* Client handle pointing at the service */
	int	(*cbfunc)(),	/* Callback function			 */
	void	*userdata)	/* Userdata, stuffed away for later	 */
{
	int			nep; 	/* number of endpoints */
	struct callback_data *__cbdata;
	struct netconfig	*nc;
	struct nd_mergearg	ma;
	void			*nch;

	if (cbfunc == NULL)
		return (NULL);

	if (_thr_main())
		__cbdata = &__cbdata_main;
	else
		__cbdata = (struct callback_data *)
			thr_get_storage(&cbdata_key, 0, destroy_cbdata);

	/* Check to see if we already have a service handle */
	if (__cbdata && (__cbdata->cbsvc[0] != NULL) &&
	    (__cbdata->cbpid == getpid()))  {
		__cbdata->cback = cbfunc;
		__cbdata->cbuser = userdata;
		__cbdata->results = 0;
		__cbdata->complete = FALSE;
		return (&(__cbdata->cbhost));
	}

	/* Nope, then let's create one... */

	if (__cbdata == NULL) {
		__cbdata = (struct callback_data *)calloc(1,
						sizeof (struct callback_data));
		if (!_thr_main())
			thr_setspecific(cbdata_key, __cbdata);
		ASSERT(my_cbdata() != NULL);
	}
	if (! __cbdata) {
		syslog(LOG_ERR, "__nis_init_callback: Client out of memory.");
		return (NULL);
	}

	__cbdata->cback = cbfunc;
	__cbdata->cbuser = userdata;
	__cbdata->cbpid = getpid();
	__cbdata->results = 0;
	__cbdata->complete = FALSE;
	__cbdata->cbhost.ep.ep_val = &(__cbdata->cbendp[0]);

	/* callbacks are not authenticated, so do minimal srv description. */
	__cbdata->cbhost.name = strdup((char *)nis_local_principal());
	__cbdata->cbhost.key_type = NIS_PK_NONE;
	__cbdata->cbhost.pkey.n_bytes = NULL;
	__cbdata->cbhost.pkey.n_len = 0;

	/* Create the service handle(s) */
	/*
	 * This gets a bit tricky. Because we don't know which transport
	 * the service will choose to call us back on, we have something
	 * of a delimma in picking the correct one here. Because of this
	 * we pick all of the likely ones and pass them on to the remote
	 * server and let it figure it out.
	 * XXX Use the same one as we have a client handle for XXX
	 */
	nch = (void *)setnetconfig();
	nep = 0;
	while (nch && ((nc = (struct netconfig *)getnetconfig(nch)) != NULL) &&
		    (nep == 0)) {

		/* Step 0. XXX see if it is the same netid */
		if (strcmp(nc->nc_netid, svc_clnt->cl_netid) != 0)
			continue;

		/* Step 1. Check to see if it is a virtual circuit transport. */
		if ((nc->nc_semantics != NC_TPI_COTS) &&
		    (nc->nc_semantics != NC_TPI_COTS_ORD))
			continue;

		/* Step 2. Try to create a service transport handle. */
		__cbdata->cbsvc[nep] = svc_tli_create(RPC_ANYFD, nc, NULL,
								128, 8192);
		if (! __cbdata->cbsvc[nep]) {
			syslog(LOG_WARNING,
				"__nis_init_callback: Can't create SVCXPRT.");
			continue;
		}

		/*
		 * When handling a callback, we don't want to impose
		 * any restrictions on request message size, and since
		 * we're not a general purpose server creating new
		 * connections, we don't need the non-blocking code
		 * either. Make sure those features are turned off
		 * for this connection.
		 *
		 * For some reason, the function prototype for
		 * svc_control(3N) is visible in <rpc/svc.h> (which we
		 * get via <rpc/rpc.h>) only when _KERNEL is defined.
		 * Since the SC5.x sparcv9 compiler is picky about it,
		 * we declare svc_control explicitly here.
		 */
		{
			extern bool_t svc_control(SVCXPRT *, uint_t, void *);
			int connmaxrec = 0;
			(void) svc_control(__cbdata->cbsvc[nep],
					SVCSET_CONNMAXREC, &connmaxrec);
		}

		/*
		 * This merge code works because we know the netids match
		 * if we want to use a connectionless transport for the
		 * initial call and a connection oriented one for the
		 * callback this won't work. Argh.! XXX
		 */
		ma.s_uaddr = taddr2uaddr(nc,
					&(__cbdata->cbsvc[nep]->xp_ltaddr));
		if (!ma.s_uaddr) {
			syslog(LOG_WARNING,
		    "__nis_init_callback: Can't get uaddr for %s transport.",
				    nc->nc_netid);
			continue;
		}
		ma.c_uaddr = __get_clnt_uaddr(svc_clnt);
		ma.m_uaddr = NULL;
		(void) netdir_options(nc, ND_MERGEADDR, 0, (void *)&ma);
		free(ma.s_uaddr);
		free(ma.c_uaddr);

		/* Step 3. Register it */
		(void) svc_reg(__cbdata->cbsvc[nep], CB_PROG, 1,
		    __do_callback, NULL);

		/* Step 4. Fill in the endpoint structure. */
		__cbdata->cbendp[nep].uaddr = ma.m_uaddr;
		__cbdata->cbendp[nep].family = strdup(nc->nc_protofmly);
		__cbdata->cbendp[nep].proto = strdup(nc->nc_proto);
		nep++;
	}
	(void) endnetconfig(nch);

	__cbdata->cbhost.ep.ep_len = nep;
	if (__cbdata->cbsvc[0] == NULL) {
		syslog(LOG_ERR,
			"__nis_init_callback: cannot create callback service.");
		return (NULL);
	}
	return (&(__cbdata->cbhost));
}

/*
 * Stub to handle requests...
 * Note, as an optimization the server may return us more than one object.
 * This stub will feed them to the callback function one at a time.
 */
static bool_t
__callback_stub(
	cback_data	*argp,
	struct svc_req	*rqstp,
	struct callback_data  *__cbdata,
	int		*results_ptr)
{
	int		i;
	char		buf[1024];

#ifdef lint
	argp = argp;
	rqstp = rqstp;
#endif /* lint */
	*results_ptr = 0;
	for (i = 0; (i < argp->entries.entries_len) && (!(*results_ptr)); i++) {
		(void) strcpy(buf, argp->entries.entries_val[i]->zo_name);
		(void) strcat(buf, ".");
		(void) strcat(buf, argp->entries.entries_val[i]->zo_domain);
		*results_ptr = (*(__cbdata->cback))(buf,
				argp->entries.entries_val[i], __cbdata->cbuser);
	}
	return (1); /* please do reply */
}

static bool_t
__callback_finish(
	void		*argp,
	struct svc_req	*rqstp,
	struct callback_data  *__cbdata,
	int		*results_ptr) /* not used */
{
#ifdef lint
	argp = argp;
	rqstp = rqstp;
	results_ptr = results_ptr;
#endif /* lint */
	__cbdata->cberror = NIS_SUCCESS;
	__cbdata->complete = TRUE;
	return (0); /* don't attempt a reply */
}

static bool_t
__callback_error(argp, rqstp, __cbdata, results_ptr)
	nis_error	*argp;
	struct svc_req	*rqstp;
	struct callback_data  *__cbdata;
	int 	*results_ptr;
{
#ifdef lint
	rqstp = rqstp;
	results_ptr = results_ptr;
#endif /* lint */
	__cbdata->cberror = *argp;
	__cbdata->complete = TRUE;
	return (1);  /* non-zero => please do a reply */
}

/*
 * __nis_run_callback()
 *
 * This is the other function exported by this module. The function
 * duplicates the behaviour of svc_run() for regular rpc services,
 * however it has the additional benefit that it monitors
 * the state of the connection and if it goes away, it terminates
 * the service and returns. Finally, when it returns, it returns
 * the number of times the callback function was called for this
 * session, or -1 if the session ended erroneously.
 */
int
__nis_run_callback(
	netobj		*srvid,		/* Server's netobj		*/
	rpcproc_t	srvproc,	/* RPC to call to check up 	*/
	struct timeval	*timeout,	/* User's timeout		*/
	CLIENT		*myserv)	/* Server talking to us 	*/
{
	enum clnt_stat	cs;
	struct timeval	tv, cbtv;
	bool_t	is_up; /* is_up is TRUE if the server answers us */
	struct callback_data  *__cbdata;
	int nfds = 0;
	int pollret;
	struct pollfd *svc_pollset = 0;
	extern rwlock_t svc_fd_lock;	/* def'd in RPC lib (mt_misc.c) */

	__cbdata = my_cbdata();

	cbtv.tv_sec = NIS_CBACK_TIMEOUT;
	cbtv.tv_usec = 0;
	if (timeout)
		tv = *timeout;
	else {
		/* Default timeout when timeout is null */
		tv.tv_sec = NIS_CBACK_TIMEOUT;
		tv.tv_usec = 0;
	}
	while (! __cbdata->complete) {
		rw_rdlock(&svc_fd_lock);	/* acquire svc_fdset lock */
		if (nfds != svc_max_pollfd) {
			svc_pollset = realloc(svc_pollset,
					sizeof (pollfd_t) * svc_max_pollfd);
			nfds = svc_max_pollfd;
		}

		if (nfds == 0) {
			rw_unlock(&svc_fd_lock);
			break;	/* None waiting, hence return */
		}

		(void) memcpy(svc_pollset, svc_pollfd,
			sizeof (pollfd_t) * svc_max_pollfd);
		rw_unlock(&svc_fd_lock);

		switch (pollret = poll(svc_pollset, nfds,
		    __rpc_timeval_to_msec(&tv))) {
		case -1:
			/*
			 * We exit on any error other than EBADF.  For all
			 * other errors, we return a callback error.
			 */
			if (errno != EBADF) {
				continue;
			}
			syslog(LOG_ERR, "callback: - select failed: %m");
			if (svc_pollset != 0)
				free(svc_pollset);
			return (- NIS_CBERROR);
		case 0:
			/*
			 * possible data race condition
			 */
			if (__cbdata->complete) {
				syslog(LOG_INFO,
		"__run_callback: data race condition detected and avoided.");
				break;
			}

			/*
			 * Check to see if the thread servicing us is still
			 * alive
			 */

			cs = clnt_call(myserv, srvproc,
						xdr_netobj, (char *)srvid,
						xdr_bool, (char *)&is_up,
						cbtv);

			if (cs != RPC_SUCCESS || !is_up) {
				if (svc_pollset != 0)
					free(svc_pollset);
				return (- NIS_CBERROR);
			}
			break;
		default:
			svc_getreq_poll(svc_pollset, pollret);
		}
	}
	if (svc_pollset != 0)
		free(svc_pollset);
	if (__cbdata->cberror) {
		return (0 - __cbdata->cberror);	/* return error results */
	} else
		return (__cbdata->results);	/* Return success (>= 0) */
}

/*
 * __do_callback()
 *
 * This is the dispatcher routine for the callback service. It is
 * very simple as you can see.
 */
static void
__do_callback(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		cback_data 	callback_recieve_1_arg;
		nis_error	callback_error_1_arg;
	} argument;
	int  	result;
	bool_t  do_reply;
	bool_t (*xdr_argument)(), (*xdr_result)();
	bool_t (*local)();
	struct callback_data *__cbdata;


	__cbdata = my_cbdata();
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case CBPROC_RECEIVE:
		xdr_argument = xdr_cback_data;
		xdr_result = xdr_bool;
		local = __callback_stub;
		(__cbdata->results)++; /* Count callback */
		break;

	case CBPROC_FINISH:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = __callback_finish;
		break;

	case CBPROC_ERROR:
		xdr_argument = xdr_nis_error;
		xdr_result = xdr_void;
		local = __callback_error;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		return;
	}
	do_reply = (*local)(&argument, rqstp, __cbdata, &result);
	if (do_reply && !svc_sendreply(transp, xdr_result, (char *)&result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (char *)&argument)) {
		syslog(LOG_WARNING, "unable to free arguments");
	}
}
