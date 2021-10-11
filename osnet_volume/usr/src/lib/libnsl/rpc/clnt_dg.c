/*
 * Copyright (c) 1986-1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)clnt_dg.c	1.48	99/12/17 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_dg.c 1.19 89/03/16 Copyr 1988 Sun Micro";
#endif

/*
 * Implements a connectionless client side RPC.
 */


#include <assert.h>

#include "rpc_mt.h"
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <errno.h>
#include <sys/poll.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/kstat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>

#define	RPC_MAX_BACKOFF		30 /* seconds */

extern int _sigfillset();
extern int __rpc_timeval_to_msec();
extern bool_t xdr_opaque_auth();
extern bool_t __rpc_gss_unwrap();

static struct clnt_ops *clnt_dg_ops();
static bool_t time_not_ok();
static int _set_options(int fd, int optname, int optval);

/*
 *	This machinery implements per-fd locks for MT-safety.  It is not
 *	sufficient to do per-CLIENT handle locks for MT-safety because a
 *	user may create more than one CLIENT handle with the same fd behind
 *	it.
 *
 *	The current implementation holds locks across the entire RPC and reply,
 *	including retransmissions.  Yes, this is silly, and as soon as this
 *	code is proven to work, this should be the first thing fixed.  One step
 *	at a time.
 */
extern int lock_value;
/*
 * FD Lock handle used by various MT sync. routines
 */
static mutex_t dgtbl_lock = DEFAULTMUTEX;
static	void	*dgtbl = NULL;

static const char mem_err_clnt_dg[] = "clnt_dg_create: out of memory";


#define	MCALL_MSG_SIZE 24

/*
 * Private data kept per client handle
 */
struct cu_data {
	int			cu_fd;		/* connections fd */
	bool_t			cu_closeit;	/* opened by library */
	struct netbuf		cu_raddr;	/* remote address */
	struct timeval		cu_wait;	/* retransmit interval */
	struct timeval		cu_total;	/* total time for the call */
	struct rpc_err		cu_error;
	struct t_unitdata	*cu_tr_data;
	XDR			cu_outxdrs;
	char			*cu_outbuf_start;
	char			cu_outbuf[MCALL_MSG_SIZE];
	uint_t			cu_xdrpos;
	uint_t			cu_sendsz;	/* send size */
	uint_t			cu_recvsz;	/* recv size */
	struct pollfd		pfdp;
	char			cu_inbuf[1];
};

static int _rcv_unitdata_err(struct cu_data *cu);

/*
 * Connection less client creation returns with client handle parameters.
 * Default options are set, which the user can change using clnt_control().
 * fd should be open and bound.
 * NB: The rpch->cl_auth is initialized to null authentication.
 * 	Caller may wish to set this something more useful.
 *
 * sendsz and recvsz are the maximum allowable packet sizes that can be
 * sent and received. Normally they are the same, but they can be
 * changed to improve the program efficiency and buffer allocation.
 * If they are 0, use the transport default.
 *
 * If svcaddr is NULL, returns NULL.
 */
CLIENT *
clnt_dg_create(fd, svcaddr, program, version, sendsz, recvsz)
	int fd;				/* open file descriptor */
	struct netbuf *svcaddr;		/* servers address */
	rpcprog_t program;		/* program number */
	rpcvers_t version;		/* version number */
	uint_t sendsz;			/* buffer recv size */
	uint_t recvsz;			/* buffer send size */
{
	CLIENT *cl = NULL;			/* client handle */
	register struct cu_data *cu = NULL;	/* private data */
	struct t_unitdata *tr_data;
	struct t_info tinfo;
	struct timeval now;
	struct rpc_msg call_msg;
	sigset_t mask, newmask;

	trace5(TR_clnt_dg_create, 0, program, version, sendsz, recvsz);

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&dgtbl_lock);
	if ((dgtbl == NULL) && ((dgtbl = rpc_fd_init()) == NULL)) {
		mutex_unlock(&dgtbl_lock);
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		goto err1;
	}
	mutex_unlock(&dgtbl_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);

	if (svcaddr == (struct netbuf *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		trace3(TR_clnt_dg_create, 1, program, version);
		return ((CLIENT *)NULL);
	}
	if (t_getinfo(fd, &tinfo) == -1) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_errno = 0;
		rpc_createerr.cf_error.re_terrno = t_errno;
		trace3(TR_clnt_dg_create, 1, program, version);
		return ((CLIENT *)NULL);
	}
	/*
	 * Setup to rcv datagram error, we ignore any errors returned from
	 * _set_options() as SO_DGRAM_ERRIND is only relevant to udp/udp6
	 * transports and this point in the code we only know that we are
	 * using a connection less transport.
	 */
	if (tinfo.servtype == T_CLTS)
		(void) _set_options(fd, SO_DGRAM_ERRIND, 1);
	/*
	 * Find the receive and the send size
	 */
	sendsz = __rpc_get_t_size((int)sendsz, tinfo.tsdu);
	recvsz = __rpc_get_t_size((int)recvsz, tinfo.tsdu);
	if ((sendsz == 0) || (recvsz == 0)) {
		rpc_createerr.cf_stat = RPC_TLIERROR; /* XXX */
		rpc_createerr.cf_error.re_errno = 0;
		rpc_createerr.cf_error.re_terrno = 0;
		trace3(TR_clnt_dg_create, 1, program, version);
		return ((CLIENT *)NULL);
	}

	if ((cl = (CLIENT *)mem_alloc(sizeof (CLIENT))) == (CLIENT *)NULL)
		goto err1;
	/*
	 * Should be multiple of 4 for XDR.
	 */
	sendsz = ((sendsz + 3) / 4) * 4;
	recvsz = ((recvsz + 3) / 4) * 4;
	cu = (struct cu_data *)mem_alloc(sizeof (*cu) + sendsz + recvsz);
	if (cu == (struct cu_data *)NULL)
		goto err1;
	if ((cu->cu_raddr.buf = mem_alloc(svcaddr->len)) == NULL)
		goto err1;
	(void) memcpy(cu->cu_raddr.buf, svcaddr->buf, (int)svcaddr->len);
	cu->cu_raddr.len = cu->cu_raddr.maxlen = svcaddr->len;
	cu->cu_outbuf_start = &cu->cu_inbuf[recvsz];
	/* Other values can also be set through clnt_control() */
	cu->cu_wait.tv_sec = 15;	/* heuristically chosen */
	cu->cu_wait.tv_usec = 0;
	cu->cu_total.tv_sec = -1;
	cu->cu_total.tv_usec = -1;
	cu->cu_sendsz = sendsz;
	cu->cu_recvsz = recvsz;
	(void) gettimeofday(&now, (struct timezone *)NULL);
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&(cu->cu_outxdrs), cu->cu_outbuf, sendsz, XDR_ENCODE);
	if (! xdr_callhdr(&(cu->cu_outxdrs), &call_msg)) {
		rpc_createerr.cf_stat = RPC_CANTENCODEARGS;  /* XXX */
		rpc_createerr.cf_error.re_errno = 0;
		rpc_createerr.cf_error.re_terrno = 0;
		goto err2;
	}
	cu->cu_xdrpos = XDR_GETPOS(&(cu->cu_outxdrs));
	XDR_DESTROY(&(cu->cu_outxdrs));
	xdrmem_create(&(cu->cu_outxdrs), cu->cu_outbuf_start, sendsz,
								XDR_ENCODE);
/* LINTED pointer alignment */
	tr_data = (struct t_unitdata *)t_alloc(fd,
				T_UNITDATA, T_ADDR | T_OPT);
	if (tr_data == (struct t_unitdata *)NULL) {
		goto err1;
	}
	tr_data->udata.maxlen = cu->cu_recvsz;
	tr_data->udata.buf = cu->cu_inbuf;
	cu->cu_tr_data = tr_data;

	/*
	 * By default, closeit is always FALSE. It is users responsibility
	 * to do a t_close on it, else the user may use clnt_control
	 * to let clnt_destroy do it for him/her.
	 */
	cu->cu_closeit = FALSE;
	cu->cu_fd = fd;
	cl->cl_ops = clnt_dg_ops();
	cl->cl_private = (caddr_t)cu;
	cl->cl_auth = authnone_create();
	cl->cl_tp = (char *)NULL;
	cl->cl_netid = (char *)NULL;
	cu->pfdp.fd = cu->cu_fd;
	cu->pfdp.events = MASKVAL;
	trace3(TR_clnt_dg_create, 1, program, version);
	return (cl);
err1:
	(void) syslog(LOG_ERR, mem_err_clnt_dg);
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
	rpc_createerr.cf_error.re_terrno = 0;
err2:
	if (cl) {
		mem_free((caddr_t)cl, sizeof (CLIENT));
		if (cu) {
			mem_free(cu->cu_raddr.buf, cu->cu_raddr.len);
			mem_free((caddr_t)cu, sizeof (*cu) + sendsz + recvsz);
		}
	}
	trace3(TR_clnt_dg_create, 1, program, version);
	return ((CLIENT *)NULL);
}

static enum clnt_stat
clnt_dg_call(cl, proc, xargs, argsp, xresults, resultsp, utimeout)
	register CLIENT	*cl;		/* client handle */
	rpcproc_t	proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	struct timeval	utimeout;	/* seconds to wait before giving up */
{
/* LINTED pointer alignment */
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	register XDR *xdrs;
	register int outlen;
	struct rpc_msg reply_msg;
	XDR reply_xdrs;
	struct timeval time_waited;
	bool_t ok;
	int nrefreshes = 2;		/* number of times to refresh cred */
	struct timeval timeout;
	struct timeval retransmit_time;
	struct timeval poll_time;
	struct timeval startime, curtime;
	struct t_unitdata tu_data;
	int res;			/* result of operations */
	sigset_t mask, newmask;
	uint32_t x_id;

	trace3(TR_clnt_dg_call, 0, cl, proc);

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	if (rpc_fd_lock(dgtbl, cu->cu_fd)) {
		rpc_callerr.re_status = RPC_FAILED;
		rpc_callerr.re_errno = errno;
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		return (RPC_FAILED);
	}

	if (cu->cu_total.tv_usec == -1) {
		timeout = utimeout;	/* use supplied timeout */
	} else {
		timeout = cu->cu_total;	/* use default timeout */
	}

	time_waited.tv_sec = 0;
	time_waited.tv_usec = 0;
	retransmit_time = cu->cu_wait;

	tu_data.addr = cu->cu_raddr;

call_again:
	xdrs = &(cu->cu_outxdrs);
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
/* LINTED pointer alignment */
	/*
	 * Due to little endian byte order, it is necessary to convert to host
	 * format before incrementing xid.
	 */
	x_id = ntohl(*(uint32_t *)(cu->cu_outbuf)) + 1;		/* set XID */
	*(uint32_t *)cu->cu_outbuf = htonl(x_id);

	if (cl->cl_auth->ah_cred.oa_flavor != RPCSEC_GSS) {
		if ((! XDR_PUTBYTES(xdrs, cu->cu_outbuf, cu->cu_xdrpos)) ||
				(! XDR_PUTINT32(xdrs, (int32_t *)&proc)) ||
				(! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
				(! xargs(xdrs, argsp))) {
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_call, 1, cl);
			return (rpc_callerr.re_status = RPC_CANTENCODEARGS);
		}
	} else {
/* LINTED pointer alignment */
		uint32_t *u = (uint32_t *)&cu->cu_outbuf[cu->cu_xdrpos];
		IXDR_PUT_U_INT32(u, proc);
		if (!__rpc_gss_wrap(cl->cl_auth, cu->cu_outbuf,
		    ((char *)u) - cu->cu_outbuf, xdrs, xargs, argsp)) {
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_call, 1, cl);
			return (rpc_callerr.re_status = RPC_CANTENCODEARGS);
		}
	}
	outlen = (int)XDR_GETPOS(xdrs);

send_again:
	tu_data.udata.buf = cu->cu_outbuf_start;
	tu_data.udata.len = outlen;
	tu_data.opt.len = 0;
	if (t_sndudata(cu->cu_fd, &tu_data) == -1) {
		rpc_callerr.re_terrno = t_errno;
		rpc_callerr.re_errno = errno;
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_call, 1, cl);
		return (rpc_callerr.re_status = RPC_CANTSEND);
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_call, 1, cl);
		return (rpc_callerr.re_status = RPC_TIMEDOUT);
	}
	/*
	 * sub-optimal code appears here because we have
	 * some clock time to spare while the packets are in flight.
	 * (We assume that this is actually only executed once.)
	 */
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = NULL;
	reply_msg.acpted_rply.ar_results.proc = xdr_void;

	/*
	 * Set polling time so that we don't wait for
	 * longer than specified by the total time to wait,
	 * or the retransmit time.
	 */
	poll_time.tv_sec = timeout.tv_sec - time_waited.tv_sec;
	poll_time.tv_usec = timeout.tv_usec - time_waited.tv_usec;
	while (poll_time.tv_usec < 0) {
		poll_time.tv_usec += 1000000;
		poll_time.tv_sec--;
	}

	if (poll_time.tv_sec < 0 || (poll_time.tv_sec == 0 &&
					poll_time.tv_usec == 0)) {
		/*
		 * this could happen if time_waited >= timeout
		 */
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_call, 1, cl);
		return (rpc_callerr.re_status = RPC_TIMEDOUT);
	}

	if (poll_time.tv_sec > retransmit_time.tv_sec ||
			(poll_time.tv_sec == retransmit_time.tv_sec &&
				poll_time.tv_usec > retransmit_time.tv_usec))
		poll_time = retransmit_time;


	for (;;) {

		(void) gettimeofday(&startime, NULL);

		switch (poll(&cu->pfdp, 1,
				__rpc_timeval_to_msec(&poll_time))) {
		case -1:
			if (errno != EINTR && errno != EAGAIN) {
				rpc_callerr.re_errno = errno;
				rpc_callerr.re_terrno = 0;
				fd_releaselock(cu->cu_fd, mask, dgtbl);
				trace2(TR_clnt_dg_call, 1, cl);
				return (rpc_callerr.re_status = RPC_CANTRECV);
			}
			/*FALLTHROUGH*/

		case 0:
			/*
			 * update time waited
			 */
			(void) gettimeofday(&curtime, NULL);
			time_waited.tv_sec += curtime.tv_sec - startime.tv_sec;
			time_waited.tv_usec += curtime.tv_usec -
							startime.tv_usec;
			while (time_waited.tv_usec >= 1000000) {
				time_waited.tv_usec -= 1000000;
				time_waited.tv_sec++;
			}
			while (time_waited.tv_usec < 0) {
				time_waited.tv_usec += 1000000;
				time_waited.tv_sec--;
			}

			/*
			 * decrement time left to poll by same amount
			 */
			poll_time.tv_sec -= curtime.tv_sec - startime.tv_sec;
			poll_time.tv_usec -= curtime.tv_usec - startime.tv_usec;
			while (poll_time.tv_usec >= 1000000) {
				poll_time.tv_usec -= 1000000;
				poll_time.tv_sec++;
			}
			while (poll_time.tv_usec < 0) {
				poll_time.tv_usec += 1000000;
				poll_time.tv_sec--;
			}

			/*
			 * if there's time left to poll, poll again
			 */
			if (poll_time.tv_sec > 0 ||
					(poll_time.tv_sec == 0 &&
						poll_time.tv_usec > 0))
				continue;

			/*
			 * if there's more time left, retransmit;
			 * otherwise, return timeout error
			 */
			if (time_waited.tv_sec < timeout.tv_sec ||
				(time_waited.tv_sec == timeout.tv_sec &&
				    time_waited.tv_usec < timeout.tv_usec)) {
				/*
				 * update retransmit_time
				 */
				retransmit_time.tv_usec *= 2;
				retransmit_time.tv_sec *= 2;
				while (retransmit_time.tv_usec >= 1000000) {
					retransmit_time.tv_usec -= 1000000;
					retransmit_time.tv_sec++;
				}
				if (retransmit_time.tv_sec >= RPC_MAX_BACKOFF) {
					retransmit_time.tv_sec =
							RPC_MAX_BACKOFF;
					retransmit_time.tv_usec = 0;
				}
				/*
				 * redo AUTH_MARSHAL if AUTH_DES or RPCSEC_GSS.
				 */
				if (cl->cl_auth->ah_cred.oa_flavor ==
					AUTH_DES ||
					cl->cl_auth->ah_cred.oa_flavor ==
					RPCSEC_GSS)
					goto call_again;
				else
					goto send_again;
			}
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_call, 1, cl);
			return (rpc_callerr.re_status = RPC_TIMEDOUT);

		default:
			break;
		}

		if (cu->pfdp.revents & POLLNVAL || (cu->pfdp.revents == 0)) {
			rpc_callerr.re_status = RPC_CANTRECV;
			/*
			 *	Note:  we're faking errno here because we
			 *	previously would have expected select() to
			 *	return -1 with errno EBADF.  Poll(BA_OS)
			 *	returns 0 and sets the POLLNVAL revents flag
			 *	instead.
			 */
			rpc_callerr.re_errno = errno = EBADF;
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_call, 1, cl);
			return (-1);
		}

		/* We have some data now */
		do {
			int moreflag;		/* flag indicating more data */

			moreflag = 0;
			if (errno == EINTR) {
				/*
				 * Must make sure errno was not already
				 * EINTR in case t_rcvudata() returns -1.
				 * This way will only stay in the loop
				 * if getmsg() sets errno to EINTR.
				 */
				errno = 0;
			}
			res = t_rcvudata(cu->cu_fd, cu->cu_tr_data, &moreflag);
			if (moreflag & T_MORE) {
				/*
				 * Drop this packet. I aint got any
				 * more space.
				 */
				res = -1;
				/* I should not really be doing this */
				errno = 0;
				/*
				 * XXX: Not really Buffer overflow in the
				 * sense of TLI.
				 */
				t_errno = TBUFOVFLW;
			}
		} while (res < 0 && errno == EINTR);
		if (res < 0) {
			int err, errnoflag = FALSE;
#ifdef sun
			if (errno == EWOULDBLOCK)
#else
			if (errno == EAGAIN)
#endif
				continue;
			if (t_errno == TLOOK) {
				if ((err = _rcv_unitdata_err(cu)) == 0)
					continue;
				else if (err == 1)
					errnoflag = TRUE;
			} else {
				rpc_callerr.re_terrno = t_errno;
			}
			if (errnoflag == FALSE)
				rpc_callerr.re_errno = errno;
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_call, 1, cl);
			return (rpc_callerr.re_status = RPC_CANTRECV);
		}
		if (cu->cu_tr_data->udata.len < (uint_t)sizeof (uint32_t))
			continue;
		/* see if reply transaction id matches sent id */
/* LINTED pointer alignment */
		if (*((uint32_t *)(cu->cu_inbuf)) !=
				*((uint32_t *)(cu->cu_outbuf)))
			continue;
		/* we now assume we have the proper reply */
		break;
	}

	/*
	 * now decode and validate the response
	 */

	xdrmem_create(&reply_xdrs, cu->cu_inbuf,
		(uint_t)cu->cu_tr_data->udata.len, XDR_DECODE);
	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	/* XDR_DESTROY(&reply_xdrs);	save a few cycles on noop destroy */
	if (ok) {
		if ((reply_msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
			(reply_msg.acpted_rply.ar_stat == SUCCESS))
			rpc_callerr.re_status = RPC_SUCCESS;
		else
			__seterr_reply(&reply_msg, &(rpc_callerr));

		if (rpc_callerr.re_status == RPC_SUCCESS) {
			if (! AUTH_VALIDATE(cl->cl_auth,
					    &reply_msg.acpted_rply.ar_verf)) {
				rpc_callerr.re_status = RPC_AUTHERROR;
				rpc_callerr.re_why = AUTH_INVALIDRESP;
			} else if (cl->cl_auth->ah_cred.oa_flavor !=
								RPCSEC_GSS) {
				if (!(*xresults)(&reply_xdrs, resultsp)) {
				    if (rpc_callerr.re_status == RPC_SUCCESS)
					rpc_callerr.re_status =
							RPC_CANTDECODERES;
				}
			} else if (!__rpc_gss_unwrap(cl->cl_auth, &reply_xdrs,
						xresults, resultsp)) {
				if (rpc_callerr.re_status == RPC_SUCCESS)
				    rpc_callerr.re_status = RPC_CANTDECODERES;
			}
		}		/* end successful completion */
		/*
		 * If unsuccesful AND error is an authentication error
		 * then refresh credentials and try again, else break
		 */
		else if (rpc_callerr.re_status == RPC_AUTHERROR)
			/* maybe our credentials need to be refreshed ... */
			if (nrefreshes > 0 &&
					AUTH_REFRESH(cl->cl_auth, &reply_msg)) {
				nrefreshes--;
				goto call_again;
			}
		/* end of unsuccessful completion */
		/* free verifier */
		if (reply_msg.rm_reply.rp_stat == MSG_ACCEPTED &&
				reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void) xdr_opaque_auth(xdrs,
					&(reply_msg.acpted_rply.ar_verf));
		}
	}	/* end of valid reply message */
	else {
		rpc_callerr.re_status = RPC_CANTDECODERES;

	}
	fd_releaselock(cu->cu_fd, mask, dgtbl);
	trace2(TR_clnt_dg_call, 1, cl);
	return (rpc_callerr.re_status);
}

static void
clnt_dg_geterr(CLIENT *cl, struct rpc_err *errp)
{
/* LINTED pointer alignment */
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;

	trace2(TR_clnt_dg_geterr, 0, cl);
	*errp = rpc_callerr;
	trace2(TR_clnt_dg_geterr, 1, cl);
}

static bool_t
clnt_dg_freeres(CLIENT *cl, xdrproc_t xdr_res, caddr_t res_ptr)
{
/* LINTED pointer alignment */
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	register XDR *xdrs = &(cu->cu_outxdrs);
	bool_t dummy;
	sigset_t mask, newmask;

	trace2(TR_clnt_dg_freeres, 0, cl);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	rpc_fd_lock(dgtbl, cu->cu_fd);
	xdrs->x_op = XDR_FREE;
	dummy = (*xdr_res)(xdrs, res_ptr);
	fd_releaselock(cu->cu_fd, mask, dgtbl);
	trace2(TR_clnt_dg_freeres, 1, cl);
	return (dummy);
}

static void
clnt_dg_abort(/* h */)
	/* CLIENT *h; */
{
	trace1(TR_clnt_dg_abort, 0);
	trace1(TR_clnt_dg_abort, 1);
}

static bool_t
clnt_dg_control(CLIENT *cl, int request, char *info)
{
/* LINTED pointer alignment */
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	struct netbuf *addr;
	sigset_t mask, newmask;
	trace3(TR_clnt_dg_control, 0, cl, request);

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	if (rpc_fd_lock(dgtbl, cu->cu_fd)) {
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		return (RPC_FAILED);
	}

	switch (request) {
	case CLSET_FD_CLOSE:
		cu->cu_closeit = TRUE;
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_control, 1, cl);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		cu->cu_closeit = FALSE;
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_control, 1, cl);
		return (TRUE);
	}

	/* for other requests which use info */
	if (info == NULL) {
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_control, 1, cl);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
/* LINTED pointer alignment */
		if (time_not_ok((struct timeval *)info)) {
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_control, 1, cl);
			return (FALSE);
		}
/* LINTED pointer alignment */
		cu->cu_total = *(struct timeval *)info;
		break;
	case CLGET_TIMEOUT:
/* LINTED pointer alignment */
		*(struct timeval *)info = cu->cu_total;
		break;
	case CLGET_SERVER_ADDR:		/* Give him the fd address */
		/* Now obsolete. Only for backword compatibility */
		(void) memcpy(info, cu->cu_raddr.buf, (int)cu->cu_raddr.len);
		break;
	case CLSET_RETRY_TIMEOUT:
/* LINTED pointer alignment */
		if (time_not_ok((struct timeval *)info)) {
			fd_releaselock(cu->cu_fd, mask, dgtbl);
			trace2(TR_clnt_dg_control, 1, cl);
			return (FALSE);
		}
/* LINTED pointer alignment */
		cu->cu_wait = *(struct timeval *)info;
		break;
	case CLGET_RETRY_TIMEOUT:
/* LINTED pointer alignment */
		*(struct timeval *)info = cu->cu_wait;
		break;
	case CLGET_FD:
/* LINTED pointer alignment */
		*(int *)info = cu->cu_fd;
		break;
	case CLGET_SVC_ADDR:
/* LINTED pointer alignment */
		*(struct netbuf *)info = cu->cu_raddr;
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
/* LINTED pointer alignment */
		addr = (struct netbuf *)info;
		if (cu->cu_raddr.maxlen < addr->len) {
			mem_free(cu->cu_raddr.buf, cu->cu_raddr.maxlen);
			if ((cu->cu_raddr.buf = mem_alloc(addr->len)) == NULL) {
				fd_releaselock(cu->cu_fd, mask, dgtbl);
				trace2(TR_clnt_dg_control, 1, cl);
				return (FALSE);
			}
			cu->cu_raddr.maxlen = addr->len;
		}
		cu->cu_raddr.len = addr->len;
		(void) memcpy(cu->cu_raddr.buf, addr->buf, addr->len);
		break;
	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure *.
		 * This will get the xid of the PREVIOUS call
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)cu->cu_outbuf);
		break;

	case CLSET_XID:
		/* This will set the xid of the NEXT call */
/* LINTED pointer alignment */
		*(uint32_t *)cu->cu_outbuf =  htonl(*(uint32_t *)info - 1);
		/* decrement by 1 as clnt_dg_call() increments once */
		break;

	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)(cu->cu_outbuf +
						    4 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_VERS:
/* LINTED pointer alignment */
		*(uint32_t *)(cu->cu_outbuf + 4 * BYTES_PER_XDR_UNIT) =
/* LINTED pointer alignment */
			htonl(*(uint32_t *)info);
		break;

	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the fourth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)(cu->cu_outbuf +
						    3 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_PROG:
/* LINTED pointer alignment */
		*(uint32_t *)(cu->cu_outbuf + 3 * BYTES_PER_XDR_UNIT) =
/* LINTED pointer alignment */
			htonl(*(uint32_t *)info);
		break;

	default:
		fd_releaselock(cu->cu_fd, mask, dgtbl);
		trace2(TR_clnt_dg_control, 1, cl);
		return (FALSE);
	}
	fd_releaselock(cu->cu_fd, mask, dgtbl);
	trace2(TR_clnt_dg_control, 1, cl);
	return (TRUE);
}

static void
clnt_dg_destroy(CLIENT *cl)
{
/* LINTED pointer alignment */
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	int cu_fd = cu->cu_fd;
	sigset_t mask, newmask;

	trace2(TR_clnt_dg_destroy, 0, cl);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	rpc_fd_lock(dgtbl, cu_fd);
	if (cu->cu_closeit)
		(void) t_close(cu_fd);
	XDR_DESTROY(&(cu->cu_outxdrs));
	cu->cu_tr_data->udata.buf = NULL;
	(void) t_free((char *)cu->cu_tr_data, T_UNITDATA);
	mem_free(cu->cu_raddr.buf, cu->cu_raddr.len);
	mem_free((caddr_t)cu,
		(sizeof (*cu) + cu->cu_sendsz + cu->cu_recvsz));
	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) +1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) +1);
	mem_free((caddr_t)cl, sizeof (CLIENT));
	fd_releaselock(cu_fd, mask, dgtbl);
	trace2(TR_clnt_dg_destroy, 1, cl);
}

static struct clnt_ops *
clnt_dg_ops(void)
{
	static struct clnt_ops ops;
	extern mutex_t	ops_lock;
	sigset_t mask, newmask;

/* VARIABLES PROTECTED BY ops_lock: ops */

	trace1(TR_clnt_dg_ops, 0);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_dg_call;
		ops.cl_abort = clnt_dg_abort;
		ops.cl_geterr = clnt_dg_geterr;
		ops.cl_freeres = clnt_dg_freeres;
		ops.cl_destroy = clnt_dg_destroy;
		ops.cl_control = clnt_dg_control;
	}
	mutex_unlock(&ops_lock);
	thr_sigsetmask(SIG_SETMASK, &mask, NULL);
	trace1(TR_clnt_dg_ops, 1);
	return (&ops);
}

/*
 * Make sure that the time is not garbage.  -1 value is allowed.
 */
static bool_t
time_not_ok(struct timeval *t)
{
	trace1(TR_time_not_ok, 0);
	trace1(TR_time_not_ok, 1);
	return (t->tv_sec < -1 || t->tv_sec > 100000000 ||
		t->tv_usec < -1 || t->tv_usec > 1000000);
}

#define	__DGRAMINDSZ 128

static int
_set_options(int fd, int optname, int optval)
{
	struct t_optmgmt oreq, ores;
	struct opthdr *topt;
	int *ip;
	int optsz;
	char buf[__DGRAMINDSZ];


	switch (optname) {
	case SO_DONTLINGER: {
			struct linger *ling;
			ling = (struct linger *)
				(buf + sizeof (struct opthdr));
			ling->l_onoff = 0;
			optsz = sizeof (struct linger);
			break;
		}

	case SO_LINGER: {
			struct linger *ling;
			ling = (struct linger *)
				(buf + sizeof (struct opthdr));
			ling->l_onoff = 1;
			ling->l_linger = (int)optval;
			optsz = sizeof (struct linger);
			break;
		}
	case SO_DEBUG:
	case SO_KEEPALIVE:
	case SO_DONTROUTE:
	case SO_USELOOPBACK:
	case SO_REUSEADDR:
	case SO_DGRAM_ERRIND:
		ip = (int *)(buf + sizeof (struct opthdr));
		*ip = optval;
		optsz = sizeof (int);
		break;
	default:
		return (-1);
	}

	topt = (struct opthdr *)buf;
	topt->level =  SOL_SOCKET;
	topt->name = optname;
	topt->len = optsz;
	oreq.flags = T_NEGOTIATE;
	oreq.opt.len = sizeof (struct opthdr) + optsz;
	oreq.opt.buf = buf;

	ores.flags = 0;
	ores.opt.buf = buf;
	ores.opt.maxlen = __DGRAMINDSZ;
	if (t_optmgmt(fd, &oreq, &ores) < 0 ||
		ores.flags != T_SUCCESS) {
		return (-1);
	}
	return (0);
}

/*
 * Receive a unit data error indication.
 * Below even when t_alloc() fails we pass uderr=NULL to t_rcvuderr()
 * so as to just clear the error indication.
 */

static int
_rcv_unitdata_err(struct cu_data *cu)
{
	int old;
	struct t_uderr *uderr;

	old = t_errno;
	uderr = (struct t_uderr *)
		t_alloc(cu->cu_fd, T_UDERROR, T_ADDR);

	if (t_rcvuderr(cu->cu_fd, uderr) == 0) {
		if (uderr == NULL)
			return (0);

		if (uderr->addr.len != cu->cu_raddr.len ||
		    (memcmp(uderr->addr.buf, cu->cu_raddr.buf,
		    cu->cu_raddr.len))) {
			t_free((char *)uderr, T_UDERROR);
			return (0);
		}
		rpc_callerr.re_errno = uderr->error;
		rpc_callerr.re_terrno = TSYSERR;
		t_free((char *)uderr, T_UDERROR);
		return (1);
	} else {
		rpc_callerr.re_terrno = old;
		if (uderr)
			t_free((char *)uderr, T_UDERROR);
		return (-1);
	}
}
