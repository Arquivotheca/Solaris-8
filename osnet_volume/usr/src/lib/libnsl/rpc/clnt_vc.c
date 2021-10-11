/*
 * Copyright (c) 1986-1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)clnt_vc.c	1.49	99/05/25 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_vc.c 1.19 89/03/16 Copyr 1988 Sun Micro";
#endif

/*
 * clnt_vc.c
 *
 * Implements a connectionful client side RPC.
 *
 * Connectionful RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer. The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent. The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that
 * is the server side should be aware that a call is batched and not produce
 * any return message. Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 */


#include "rpc_mt.h"
#include <assert.h>
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <sys/mkdev.h>
#include <sys/poll.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>

#define	MCALL_MSG_SIZE 24
#ifndef MIN
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

extern int _sigfillset();
extern int __rpc_timeval_to_msec();
extern int __rpc_compress_pollfd(int, pollfd_t *, pollfd_t *);
extern bool_t xdr_opaque_auth();
extern bool_t __rpc_gss_wrap();
extern bool_t __rpc_gss_unwrap();

static struct clnt_ops	*clnt_vc_ops();
#ifdef __STDC__
static int		read_vc(void *, caddr_t, int);
static int		write_vc(void *, caddr_t, int);
#else
static int		read_vc();
static int		write_vc();
#endif
static int		t_rcvall();
static bool_t		time_not_ok();
static bool_t		set_up_connection();

extern int lock_value;

/*
 * Lock table handle used by various MT sync. routines
 */
static mutex_t	vctbl_lock = DEFAULTMUTEX;
static void	*vctbl = NULL;

static const char clnt_vc_errstr[] = "%s : %s";
static const char clnt_vc_str[] = "clnt_vc_create";
static const char clnt_read_vc_str[] = "read_vc";
static const char __no_mem_str[] = "out of memory";

/*
 * Private data structure
 */
struct ct_data {
	int		ct_fd;		/* connection's fd */
	bool_t		ct_closeit;	/* close it on destroy */
	int		ct_tsdu;	/* size of tsdu */
	int		ct_wait;	/* wait interval in milliseconds */
	bool_t		ct_waitset;	/* wait set by clnt_control? */
	struct netbuf	ct_addr;	/* remote addr */
	struct rpc_err	ct_error;
	char		ct_mcall[MCALL_MSG_SIZE]; /* marshalled callmsg */
	uint_t		ct_mpos;	/* pos after marshal */
	XDR		ct_xdrs;	/* XDR stream */
};

/*
 * Create a client handle for a connection.
 * Default options are set, which the user can change using clnt_control()'s.
 * The rpc/vc package does buffering similar to stdio, so the client
 * must pick send and receive buffer sizes, 0 => use the default.
 * NB: fd is copied into a private area.
 * NB: The rpch->cl_auth is set null authentication. Caller may wish to
 * set this something more useful.
 *
 * fd should be open and bound.
 */
CLIENT *
clnt_vc_create(fd, svcaddr, prog, vers, sendsz, recvsz)
	register int fd;		/* open file descriptor */
	struct netbuf *svcaddr;		/* servers address */
	rpcprog_t prog;			/* program number */
	rpcvers_t vers;			/* version number */
	uint_t sendsz;			/* buffer recv size */
	uint_t recvsz;			/* buffer send size */
{
	CLIENT *cl;			/* client handle */
	register struct ct_data *ct;	/* private data */
	struct timeval now;
	struct rpc_msg call_msg;
	struct t_info tinfo;
	sigset_t mask, newmask;

	trace5(TR_clnt_vc_create, 0, prog, vers, sendsz, recvsz);

	cl = (CLIENT *)mem_alloc(sizeof (*cl));
	ct = (struct ct_data *)mem_alloc(sizeof (*ct));
	if ((cl == (CLIENT *)NULL) || (ct == (struct ct_data *)NULL)) {
		(void) syslog(LOG_ERR, clnt_vc_errstr,
				clnt_vc_str, __no_mem_str);
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		goto err;
	}
	ct->ct_addr.buf = NULL;

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&vctbl_lock);

	if ((vctbl == NULL) && ((vctbl = rpc_fd_init()) == NULL)) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		mutex_unlock(&vctbl_lock);
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		goto err;
	}

	if (set_up_connection(fd, svcaddr, ct) == FALSE) {
		mutex_unlock(&vctbl_lock);
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		goto err;
	}
	mutex_unlock(&vctbl_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);

	/*
	 * Set up other members of private data struct
	 */
	ct->ct_fd = fd;
	/*
	 * The actual value will be set by clnt_call or clnt_control
	 */
	ct->ct_wait = 30000;
	ct->ct_waitset = FALSE;
	/*
	 * By default, closeit is always FALSE. It is users responsibility
	 * to do a t_close on it, else the user may use clnt_control
	 * to let clnt_destroy do it for him/her.
	 */
	ct->ct_closeit = FALSE;

	/*
	 * Initialize call message
	 */
	(void) gettimeofday(&now, (struct timezone *)0);
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;

	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_mcall, MCALL_MSG_SIZE, XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		goto err;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	if (t_getinfo(fd, &tinfo) == -1) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_terrno = t_errno;
		rpc_createerr.cf_error.re_errno = 0;
		goto err;
	}
	/*
	 * Find the receive and the send size
	 */
	sendsz = __rpc_get_t_size((int)sendsz, tinfo.tsdu);
	recvsz = __rpc_get_t_size((int)recvsz, tinfo.tsdu);
	if ((sendsz == 0) || (recvsz == 0)) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_terrno = 0;
		rpc_createerr.cf_error.re_errno = 0;
		goto err;
	}
	ct->ct_tsdu = tinfo.tsdu;
	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz, (caddr_t)ct,
			read_vc, write_vc);
	cl->cl_ops = clnt_vc_ops();
	cl->cl_private = (caddr_t)ct;
	cl->cl_auth = authnone_create();
	cl->cl_tp = (char *)NULL;
	cl->cl_netid = (char *)NULL;
	trace3(TR_clnt_vc_create, 1, prog, vers);
	return (cl);

err:
	if (cl) {
		if (ct) {
			if (ct->ct_addr.len)
				mem_free(ct->ct_addr.buf, ct->ct_addr.len);
			mem_free((caddr_t)ct, sizeof (struct ct_data));
		}
		mem_free((caddr_t)cl, sizeof (CLIENT));
	}
	trace3(TR_clnt_vc_create, 1, prog, vers);
	return ((CLIENT *)NULL);
}

static bool_t
set_up_connection(fd, svcaddr, ct)
	register int fd;
	struct netbuf *svcaddr;		/* servers address */
	register struct ct_data *ct;
{
	int state;
	struct t_call sndcallstr, *rcvcall;
	int nconnect;
	bool_t connected, do_rcv_connect;

	ct->ct_addr.len = 0;
	state = t_getstate(fd);
	if (state == -1) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_errno = 0;
		rpc_createerr.cf_error.re_terrno = t_errno;
		return (FALSE);
	}

#ifdef DEBUG
	fprintf(stderr, "set_up_connection: state = %d\n", state);
#endif
	switch (state) {
	case T_IDLE:
		if (svcaddr == (struct netbuf *)NULL) {
			rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
			return (FALSE);
		}
		/*
		 * Connect only if state is IDLE and svcaddr known
		 */
/* LINTED pointer alignment */
		rcvcall = (struct t_call *)t_alloc(fd, T_CALL, T_OPT|T_ADDR);
		if (rcvcall == NULL) {
			rpc_createerr.cf_stat = RPC_TLIERROR;
			rpc_createerr.cf_error.re_terrno = t_errno;
			rpc_createerr.cf_error.re_errno = errno;
			return (FALSE);
		}
		rcvcall->udata.maxlen = 0;
		sndcallstr.addr = *svcaddr;
		sndcallstr.opt.len = 0;
		sndcallstr.udata.len = 0;
		/*
		 * Even NULL could have sufficed for rcvcall, because
		 * the address returned is same for all cases except
		 * for the gateway case, and hence required.
		 */
		connected = FALSE;
		do_rcv_connect = FALSE;
		for (nconnect = 0; nconnect < 3; nconnect++) {
			if (t_connect(fd, &sndcallstr, rcvcall) != -1) {
				connected = TRUE;
				break;
			}
			if (!(t_errno == TSYSERR && errno == EINTR)) {
				break;
			}
			if ((state = t_getstate(fd)) == T_OUTCON) {
				do_rcv_connect = TRUE;
				break;
			}
			if (state != T_IDLE)
				break;
		}
		if (do_rcv_connect) {
			do {
				if (t_rcvconnect(fd, rcvcall) != -1) {
					connected = TRUE;
					break;
				}
			} while (t_errno == TSYSERR && errno == EINTR);
		}
		if (!connected) {
			rpc_createerr.cf_stat = RPC_TLIERROR;
			rpc_createerr.cf_error.re_terrno = t_errno;
			rpc_createerr.cf_error.re_errno = errno;
			(void) t_free((char *)rcvcall, T_CALL);
#ifdef DEBUG
			fprintf(stderr, "clnt_vc: t_connect error %d\n",
				rpc_createerr.cf_error.re_terrno);
#endif
			return (FALSE);
		}

		/* Free old area if allocated */
		if (ct->ct_addr.buf)
			free(ct->ct_addr.buf);
		ct->ct_addr = rcvcall->addr;	/* To get the new address */
		/* So that address buf does not get freed */
		rcvcall->addr.buf = NULL;
		(void) t_free((char *)rcvcall, T_CALL);
		break;
	case T_DATAXFER:
	case T_OUTCON:
		if (svcaddr == (struct netbuf *)NULL) {
			/*
			 * svcaddr could also be NULL in cases where the
			 * client is already bound and connected.
			 */
			ct->ct_addr.len = 0;
		} else {
			ct->ct_addr.buf = malloc(svcaddr->len);
			if (ct->ct_addr.buf == (char *)NULL) {
				(void) syslog(LOG_ERR, clnt_vc_errstr,
					clnt_vc_str, __no_mem_str);
				rpc_createerr.cf_stat = RPC_SYSTEMERROR;
				rpc_createerr.cf_error.re_errno = errno;
				rpc_createerr.cf_error.re_terrno = 0;
				return (FALSE);
			}
			(void) memcpy(ct->ct_addr.buf, svcaddr->buf,
					(int)svcaddr->len);
			ct->ct_addr.len = ct->ct_addr.maxlen = svcaddr->len;
		}
		break;
	default:
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (FALSE);
	}
	return (TRUE);
}

static enum clnt_stat
clnt_vc_call(cl, proc, xdr_args, args_ptr, xdr_results, results_ptr, timeout)
	register CLIENT *cl;
	rpcproc_t proc;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
	xdrproc_t xdr_results;
	caddr_t results_ptr;
	struct timeval timeout;
{
/* LINTED pointer alignment */
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	uint32_t x_id;
/* LINTED pointer alignment */
	uint32_t *msg_x_id = (uint32_t *)(ct->ct_mcall);	/* yuk */
	register bool_t shipnow;
	int refreshes = 2;
	sigset_t  mask, newmask;

	trace3(TR_clnt_vc_call, 0, cl, proc);

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	if (rpc_fd_lock(vctbl, ct->ct_fd)) {
		rpc_callerr.re_status = RPC_FAILED;
		rpc_callerr.re_errno = errno;
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		return (RPC_FAILED);
	}

	if (!ct->ct_waitset) {
		/* If time is not within limits, we ignore it. */
		if (time_not_ok(&timeout) == FALSE)
			ct->ct_wait = __rpc_timeval_to_msec(&timeout);
	} else {
		timeout.tv_sec = (ct->ct_wait / 1000);
		timeout.tv_usec = (ct->ct_wait % 1000) * 1000;
	}

	shipnow = ((xdr_results == (xdrproc_t)0) && (timeout.tv_sec == 0) &&
	    (timeout.tv_usec == 0)) ? FALSE : TRUE;
call_again:
	xdrs->x_op = XDR_ENCODE;
	rpc_callerr.re_status = RPC_SUCCESS;
	/*
	 * Due to little endian byte order, it is necessary to convert to host
	 * format before decrementing xid.
	 */
	x_id = ntohl(*msg_x_id) - 1;
	*msg_x_id = htonl(x_id);

	if (cl->cl_auth->ah_cred.oa_flavor != RPCSEC_GSS) {
		if ((! XDR_PUTBYTES(xdrs, ct->ct_mcall, ct->ct_mpos)) ||
		    (! XDR_PUTINT32(xdrs, (int32_t *)&proc)) ||
		    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
		    (! xdr_args(xdrs, args_ptr))) {
			if (rpc_callerr.re_status == RPC_SUCCESS)
				rpc_callerr.re_status = RPC_CANTENCODEARGS;
			(void) xdrrec_endofrecord(xdrs, TRUE);
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_call, 1, cl, proc);
			return (rpc_callerr.re_status);
		}
	} else {
/* LINTED pointer alignment */
		uint32_t *u = (uint32_t *)&ct->ct_mcall[ct->ct_mpos];
		IXDR_PUT_U_INT32(u, proc);
		if (!__rpc_gss_wrap(cl->cl_auth, ct->ct_mcall,
		    ((char *)u) - ct->ct_mcall, xdrs, xdr_args, args_ptr)) {
			if (rpc_callerr.re_status == RPC_SUCCESS)
				rpc_callerr.re_status = RPC_CANTENCODEARGS;
			(void) xdrrec_endofrecord(xdrs, TRUE);
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_call, 1, cl, proc);
			return (rpc_callerr.re_status);
		}
	}
	if (! xdrrec_endofrecord(xdrs, shipnow)) {
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_call, 1, cl, proc);
		return (rpc_callerr.re_status = RPC_CANTSEND);
	}
	if (! shipnow) {
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_call, 1, cl, proc);
		return (RPC_SUCCESS);
	}
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_call, 1, cl, proc);
		return (rpc_callerr.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	/*CONSTANTCONDITION*/
	while (TRUE) {
		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = NULL;
		reply_msg.acpted_rply.ar_results.proc = (xdrproc_t)xdr_void;
		if (! xdrrec_skiprecord(xdrs)) {
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_call, 1, cl, proc);
			return (rpc_callerr.re_status);
		}
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, &reply_msg)) {
			if (rpc_callerr.re_status == RPC_SUCCESS)
				continue;
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_call, 1, cl, proc);
			return (rpc_callerr.re_status);
		}
		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
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
		} else if (cl->cl_auth->ah_cred.oa_flavor != RPCSEC_GSS) {
			if (!(*xdr_results)(xdrs, results_ptr)) {
				if (rpc_callerr.re_status == RPC_SUCCESS)
				    rpc_callerr.re_status = RPC_CANTDECODERES;
			}
		} else if (!__rpc_gss_unwrap(cl->cl_auth, xdrs, xdr_results,
							results_ptr)) {
			if (rpc_callerr.re_status == RPC_SUCCESS)
				rpc_callerr.re_status = RPC_CANTDECODERES;
		}
	}	/* end successful completion */
	/*
	 * If unsuccesful AND error is an authentication error
	 * then refresh credentials and try again, else break
	 */
	else if (rpc_callerr.re_status == RPC_AUTHERROR) {
		/* maybe our credentials need to be refreshed ... */
		if (refreshes-- && AUTH_REFRESH(cl->cl_auth, &reply_msg))
			goto call_again;
	} /* end of unsuccessful completion */
	/* free verifier ... */
	if (reply_msg.rm_reply.rp_stat == MSG_ACCEPTED &&
			reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
		xdrs->x_op = XDR_FREE;
		(void) xdr_opaque_auth(xdrs, &(reply_msg.acpted_rply.ar_verf));
	}
	fd_releaselock(ct->ct_fd, mask, vctbl);
	trace3(TR_clnt_vc_call, 1, cl, proc);
	return (rpc_callerr.re_status);
}

static void
clnt_vc_geterr(cl, errp)
	CLIENT *cl;
	struct rpc_err *errp;
{
	trace2(TR_clnt_vc_geterr, 0, cl);
	*errp = rpc_callerr;
	trace2(TR_clnt_vc_geterr, 1, cl);
}

static bool_t
clnt_vc_freeres(cl, xdr_res, res_ptr)
	CLIENT *cl;
	xdrproc_t xdr_res;
	caddr_t res_ptr;
{
/* LINTED pointer alignment */
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);
	bool_t dummy;
	sigset_t  mask, newmask;

	trace2(TR_clnt_vc_freeres, 0, cl);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	rpc_fd_lock(vctbl, ct->ct_fd);
	xdrs->x_op = XDR_FREE;
	dummy = (*xdr_res)(xdrs, res_ptr);
	fd_releaselock(ct->ct_fd, mask, vctbl);
	trace2(TR_clnt_vc_freeres, 1, cl);
	return (dummy);
}

static void
clnt_vc_abort(void)
{
	trace1(TR_clnt_vc_abort, 0);
	trace1(TR_clnt_vc_abort, 1);
}

/*ARGSUSED*/
static bool_t
clnt_vc_control(cl, request, info)
	CLIENT *cl;
	int request;
	char *info;
{
	bool_t ret;
/* LINTED pointer alignment */
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	sigset_t  mask, newmask;

	trace3(TR_clnt_vc_control, 0, cl, request);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);

	if (rpc_fd_lock(vctbl, ct->ct_fd)) {
		thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		return (RPC_FAILED);
	}

	switch (request) {
	case CLSET_FD_CLOSE:
		ct->ct_closeit = TRUE;
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		ct->ct_closeit = FALSE;
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (TRUE);
	}

	/* for other requests which use info */
	if (info == NULL) {
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
/* LINTED pointer alignment */
		if (time_not_ok((struct timeval *)info)) {
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_control, 1, cl, request);
			return (FALSE);
		}
/* LINTED pointer alignment */
		ct->ct_wait = __rpc_timeval_to_msec((struct timeval *)info);
		ct->ct_waitset = TRUE;
		break;
	case CLGET_TIMEOUT:
/* LINTED pointer alignment */
		((struct timeval *)info)->tv_sec = ct->ct_wait / 1000;
/* LINTED pointer alignment */
		((struct timeval *)info)->tv_usec =
			(ct->ct_wait % 1000) * 1000;
		break;
	case CLGET_SERVER_ADDR:	/* For compatibility only */
		(void) memcpy(info, ct->ct_addr.buf, (int)ct->ct_addr.len);
		break;
	case CLGET_FD:
/* LINTED pointer alignment */
		*(int *)info = ct->ct_fd;
		break;
	case CLGET_SVC_ADDR:
		/* The caller should not free this memory area */
/* LINTED pointer alignment */
		*(struct netbuf *)info = ct->ct_addr;
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
#ifdef undef
		/*
		 * XXX: once the t_snddis(), followed by t_connect() starts to
		 * work, this ifdef should be removed.  CLIENT handle reuse
		 * would then be possible for COTS as well.
		 */
		if (t_snddis(ct->ct_fd, NULL) == -1) {
			rpc_createerr.cf_stat = RPC_TLIERROR;
			rpc_createerr.cf_error.re_terrno = t_errno;
			rpc_createerr.cf_error.re_errno = errno;
			fd_releaselock(ct->ct_fd, mask, vctbl);
			trace3(TR_clnt_vc_control, 1, cl, request);
			return (FALSE);
		}
		ret = set_up_connection(ct->ct_fd, (struct netbuf *)info, ct));
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (ret);
#else
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (FALSE);
#endif
	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure
		 * This will get the xid of the PREVIOUS call
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)ct->ct_mcall);
		break;
	case CLSET_XID:
		/* This will set the xid of the NEXT call */
/* LINTED pointer alignment */
		*(uint32_t *)ct->ct_mcall =  htonl(*(uint32_t *)info + 1);
		/* increment by 1 as clnt_vc_call() decrements once */
		break;
	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)(ct->ct_mcall +
						4 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_VERS:
/* LINTED pointer alignment */
		*(uint32_t *)(ct->ct_mcall + 4 * BYTES_PER_XDR_UNIT) =
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
		*(uint32_t *)info = ntohl(*(uint32_t *)(ct->ct_mcall +
						3 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_PROG:
/* LINTED pointer alignment */
		*(uint32_t *)(ct->ct_mcall + 3 * BYTES_PER_XDR_UNIT) =
/* LINTED pointer alignment */
			htonl(*(uint32_t *)info);
		break;

	default:
		fd_releaselock(ct->ct_fd, mask, vctbl);
		trace3(TR_clnt_vc_control, 1, cl, request);
		return (FALSE);
	}
	fd_releaselock(ct->ct_fd, mask, vctbl);
	trace3(TR_clnt_vc_control, 1, cl, request);
	return (TRUE);
}

static void
clnt_vc_destroy(cl)
	CLIENT *cl;
{
/* LINTED pointer alignment */
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	int ct_fd = ct->ct_fd;
	sigset_t mask, newmask;

	trace2(TR_clnt_vc_destroy, 0, cl);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	rpc_fd_lock(vctbl, ct_fd);
	if (ct->ct_closeit)
		(void) t_close(ct_fd);
	XDR_DESTROY(&(ct->ct_xdrs));
	if (ct->ct_addr.buf)
		(void) free(ct->ct_addr.buf);
	mem_free((caddr_t)ct, sizeof (struct ct_data));
	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) +1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) +1);
	mem_free((caddr_t)cl, sizeof (CLIENT));
	fd_releaselock(ct_fd, mask, vctbl);
	trace2(TR_clnt_vc_destroy, 1, cl);
}

/*
 * Interface between xdr serializer and vc connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
read_vc(ct_tmp, buf, len)
	void *ct_tmp;
	caddr_t buf;
	register int len;
{
	struct pollfd *pfdp = (struct pollfd *)NULL;
	static int npfd = 0; /* total number of pfdp allocated */
	static struct pollfd *pfdp_main = (struct pollfd *)NULL;
	register struct ct_data *ct = ct_tmp;
	static thread_key_t pfdp_key;
	int main_thread;
	extern mutex_t tsd_lock;
	sigset_t mask, newmask;
	struct timeval starttime;
	struct timeval curtime;
	struct timeval time_waited;
	struct timeval timeout;
	int poll_time;
	int delta;

	trace2(TR_read_vc, 0, len);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);

	if (len == 0) {
		trace2(TR_read_vc, 1, len);
		return (0);
	}
	if ((main_thread = _thr_main())) {
			pfdp = pfdp_main;
	} else {
		if (pfdp_key == 0) {
			thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
			mutex_lock(&tsd_lock);
			if (pfdp_key == 0)
				thr_keycreate(&pfdp_key, free);
			mutex_unlock(&tsd_lock);
			thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		}
		thr_getspecific(pfdp_key, (void **) &pfdp);
	}
	if (pfdp == (struct pollfd *)NULL) {
		/* allocate just one first */
		pfdp = (struct pollfd *)malloc(
			sizeof (struct pollfd));
		if (pfdp == (struct pollfd *)NULL) {
			(void) syslog(LOG_ERR, clnt_vc_errstr,
				clnt_read_vc_str, __no_mem_str);
			rpc_callerr.re_status = RPC_SYSTEMERROR;
			rpc_callerr.re_errno = errno;
			rpc_callerr.re_terrno = 0;
			trace2(TR_read_vc, 1, len);
			return (-1);
		}
		npfd = 1;
		if (main_thread)
			pfdp_main = pfdp;
		else
			thr_setspecific(pfdp_key, (void *) pfdp);
	}
	/*
	 *	N.B.:  slot 0 in the pollfd array is reserved for the file
	 *	descriptor we're really interested in (as opposed to the
	 *	callback descriptors).
	 */
	pfdp[0].fd = ct->ct_fd;
	pfdp[0].events = MASKVAL;
	pfdp[0].revents = 0;
	poll_time = ct->ct_wait;
	if (gettimeofday(&starttime,
	    (struct timezone *) NULL) == -1) {
		syslog(LOG_ERR, "Unable to get time of day\n");
		return (-1);
	}

	/*CONSTANTCONDITION*/
	while (TRUE) {
		extern void (*_svc_getreqset_proc)();
		extern pollfd_t *svc_pollfd;
		extern int svc_max_pollfd;
		int fds;

	/* VARIABLES PROTECTED BY svc_fd_lock: svc_pollfd */

		if (_svc_getreqset_proc) {
			thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
			rw_rdlock(&svc_fd_lock);

			/* now allocate pfdp to svc_max_pollfd +1 */
			if (npfd != (svc_max_pollfd + 1)) {
				struct pollfd *tmp_pfdp = pfdp;

				tmp_pfdp = realloc(pfdp,
						sizeof (struct pollfd) *
						(svc_max_pollfd + 1));
				if (tmp_pfdp == NULL) {
					rw_unlock(&svc_fd_lock);
					thr_sigsetmask(SIG_SETMASK, &(mask),
									NULL);
					trace2(TR_read_vc, 1, len);
					return (-1);
				}

				pfdp = tmp_pfdp;
				npfd = svc_max_pollfd + 1;

				/*
				 * If it's main thread, make sure the
				 * main_pfdp is re-set the new pfdp
				 */
				if (main_thread)
					pfdp_main = pfdp;
				else
					thr_setspecific(pfdp_key, (void *)pfdp);
			}
			(void) memcpy(&pfdp[1], svc_pollfd,
				sizeof (struct pollfd) * svc_max_pollfd);

			rw_unlock(&svc_fd_lock);
			thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
		} else
			npfd = 1;	/* don't forget about pfdp[0] */

		switch (fds = poll(pfdp, npfd, poll_time)) {
		case 0:
			rpc_callerr.re_status = RPC_TIMEDOUT;
			trace2(TR_read_vc, 1, len);
			return (-1);

		case -1:
			if (errno != EINTR)
				continue;
			else {
				/*
				 * interrupted by another signal,
				 * update time_waited
				 */

				if (gettimeofday(&curtime,
				(struct timezone *) NULL) == -1) {
					syslog(LOG_ERR,
						"Unable to get time of day\n");
					errno = 0;
					continue;
				};
				delta = (curtime.tv_sec -
						starttime.tv_sec) * 1000 +
					(curtime.tv_usec -
						starttime.tv_usec) / 1000;
				poll_time -= delta;
				if (poll_time < 0) {
					rpc_callerr.re_status =
						RPC_TIMEDOUT;
					errno = 0;
					trace2(TR_read_vc, 1, len);
					return (-1);
				} else {
					errno = 0; /* reset it */
					continue;
				}
			}
		}

		if (pfdp[0].revents == 0) {
			/* must be for server side of the house */
			(*_svc_getreqset_proc)(&pfdp[1], fds);
			continue;	/* do poll again */
		}

		if (pfdp[0].revents & POLLNVAL) {
			rpc_callerr.re_status = RPC_CANTRECV;
			/*
			 *	Note:  we're faking errno here because we
			 *	previously would have expected select() to
			 *	return -1 with errno EBADF.  Poll(BA_OS)
			 *	returns 0 and sets the POLLNVAL revents flag
			 *	instead.
			 */
			rpc_callerr.re_errno = errno = EBADF;
			trace2(TR_read_vc, 1, len);
			return (-1);
		}

		if (pfdp[0].revents & (POLLERR | POLLHUP)) {
			rpc_callerr.re_status = RPC_CANTRECV;
			rpc_callerr.re_errno = errno = EPIPE;
			trace2(TR_read_vc, 1, len);
			return (-1);
		}
		break;
	}

	switch (len = t_rcvall(ct->ct_fd, buf, len)) {
	case 0:
		/* premature eof */
		rpc_callerr.re_errno = ENOLINK;
		rpc_callerr.re_terrno = 0;
		rpc_callerr.re_status = RPC_CANTRECV;
		len = -1;	/* it's really an error */
		break;

	case -1:
		rpc_callerr.re_terrno = t_errno;
		rpc_callerr.re_errno = 0;
		rpc_callerr.re_status = RPC_CANTRECV;
		break;
	}
	trace2(TR_read_vc, 1, len);
	return (len);
}

static int
write_vc(ct_tmp, buf, len)
	void *ct_tmp;
	caddr_t buf;
	int len;
{
	register int i, cnt;
	struct ct_data *ct = ct_tmp;
	int flag;
	int maxsz;
	sigset_t mask, newmask;

	trace2(TR_write_vc, 0, len);

	maxsz = ct->ct_tsdu;
	if ((maxsz == 0) || (maxsz == -1)) {
		if ((len = t_snd(ct->ct_fd, buf, (unsigned)len, 0)) == -1) {
			rpc_callerr.re_terrno = t_errno;
			rpc_callerr.re_errno = 0;
			rpc_callerr.re_status = RPC_CANTSEND;
		}
		trace2(TR_write_vc, 1, len);
		return (len);
	}

	/*
	 * This for those transports which have a max size for data.
	 */
	for (cnt = len, i = 0; cnt > 0; cnt -= i, buf += i) {
		flag = cnt > maxsz ? T_MORE : 0;
		if ((i = t_snd(ct->ct_fd, buf, (unsigned)MIN(cnt, maxsz),
				flag)) == -1) {
			rpc_callerr.re_terrno = t_errno;
			rpc_callerr.re_errno = 0;
			rpc_callerr.re_status = RPC_CANTSEND;
			trace2(TR_write_vc, 1, len);
			return (-1);
		}
	}
	trace2(TR_write_vc, 1, len);
	return (len);
}

/*
 * Receive the required bytes of data, even if it is fragmented.
 */
static int
t_rcvall(fd, buf, len)
	int fd;
	char *buf;
	int len;
{
	int moreflag;
	int final = 0;
	int res;

	trace3(TR_t_rcvall, 0, fd, len);
	do {
		moreflag = 0;
		res = t_rcv(fd, buf, (unsigned)len, &moreflag);
		if (res == -1) {
			if (t_errno == TLOOK)
				switch (t_look(fd)) {
				case T_DISCONNECT:
					t_rcvdis(fd, NULL);
					t_snddis(fd, NULL);
					trace3(TR_t_rcvall, 1, fd, len);
					return (-1);
				case T_ORDREL:
				/* Received orderly release indication */
					t_rcvrel(fd);
				/* Send orderly release indicator */
					(void) t_sndrel(fd);
					trace3(TR_t_rcvall, 1, fd, len);
					return (-1);
				default:
					trace3(TR_t_rcvall, 1, fd, len);
					return (-1);
				}
		} else if (res == 0) {
			trace3(TR_t_rcvall, 1, fd, len);
			return (0);
		}
		final += res;
		buf += res;
		len -= res;
	} while ((len > 0) && (moreflag & T_MORE));
	trace3(TR_t_rcvall, 1, fd, len);
	return (final);
}

static struct clnt_ops *
clnt_vc_ops(void)
{
	static struct clnt_ops ops;
	extern mutex_t	ops_lock;
	sigset_t mask, newmask;

	/* VARIABLES PROTECTED BY ops_lock: ops */

	trace1(TR_clnt_vc_ops, 0);
	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_vc_call;
		ops.cl_abort = clnt_vc_abort;
		ops.cl_geterr = clnt_vc_geterr;
		ops.cl_freeres = clnt_vc_freeres;
		ops.cl_destroy = clnt_vc_destroy;
		ops.cl_control = clnt_vc_control;
	}
	mutex_unlock(&ops_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
	trace1(TR_clnt_vc_ops, 1);
	return (&ops);
}

/*
 * Make sure that the time is not garbage.   -1 value is disallowed.
 * Note this is different from time_not_ok in clnt_dg.c
 */
static bool_t
time_not_ok(t)
	struct timeval *t;
{
	trace1(TR_time_not_ok, 0);
	trace1(TR_time_not_ok, 1);
	return (t->tv_sec <= -1 || t->tv_sec > 100000000 ||
		t->tv_usec <= -1 || t->tv_usec > 1000000);
}
