/*
 * Copyright (c) 1996-1999 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)clnt_door.c	1.9	99/07/19 SMI"

/*
 * clnt_doors.c, Client side for doors IPC based RPC.
 */

#include "rpc_mt.h"
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/poll.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/kstat.h>
#include <sys/time.h>
#include <door.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <rpc/svc_mt.h>


extern int _sigfillset();
extern bool_t xdr_opaque_auth();

static struct clnt_ops *clnt_door_ops();

extern int __rpc_default_door_buf_size;
extern int __rpc_min_door_buf_size;
extern int __rpc_max_door_buf_size;

/*
 * Private data kept per client handle
 */
struct cu_data {
	int			cu_fd;		/* door fd */
	bool_t			cu_closeit;	/* close it on destroy */
	struct rpc_err		cu_error;
	u_int			cu_xdrpos;
	u_int			cu_sendsz;	/* send size */
	char			cu_header[32];	/* precreated header */
};

/*
 * Door IPC based client creation routine.
 *
 * NB: The rpch->cl_auth is initialized to null authentication.
 * 	Caller may wish to set this something more useful.
 *
 * sendsz is the maximum allowable packet size that can be sent.
 * 0 will cause default to be used.
 */
CLIENT *
clnt_door_create(program, version, sendsz)
	rpcprog_t		program;	/* program number */
	rpcvers_t		version;	/* version number */
	u_int			sendsz;		/* buffer recv size */
{
	CLIENT			*cl = NULL;	/* client handle */
	struct cu_data		*cu = NULL;	/* private data */
	struct rpc_msg		call_msg;
	char			rendezvous[64];
	int			did;
	struct door_info	info;
	XDR			xdrs;
	struct timeval		now;

	if (rpcdrc.door_create == NULL) {
		if (!rpc_doorcalls_init()) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			rpc_createerr.cf_error.re_terrno = 0;
			return (NULL);
		}
	}

	(void) sprintf(rendezvous, RPC_DOOR_RENDEZVOUS, program, version);
	if ((did = open(rendezvous, O_RDONLY, 0)) < 0) {
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		return (NULL);
	}

	if (rpc_door_info(did, &info) < 0 ||
					(info.di_attributes & DOOR_REVOKED)) {
		close(did);
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		return (NULL);
	}

	/*
	 * Determine send size
	 */
	if (sendsz < __rpc_min_door_buf_size)
		sendsz = __rpc_default_door_buf_size;
	else if (sendsz > __rpc_max_door_buf_size)
		sendsz = __rpc_max_door_buf_size;
	else
		sendsz = RNDUP(sendsz);

	if ((cl = (CLIENT *) mem_alloc(sizeof (CLIENT))) == (CLIENT *)NULL ||
			(cu = (struct cu_data *) mem_alloc(sizeof (*cu))) ==
						(struct cu_data *)NULL) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto err;
	}

	/*
	 * Precreate RPC header for performance reasons.
	 */
	(void) gettimeofday(&now, (struct timezone *)NULL);
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&xdrs, cu->cu_header, sizeof (cu->cu_header), XDR_ENCODE);
	if (!xdr_callhdr(&xdrs, &call_msg)) {
		rpc_createerr.cf_stat = RPC_CANTENCODEARGS;
		rpc_createerr.cf_error.re_errno = 0;
		goto err;
	}
	cu->cu_xdrpos = XDR_GETPOS(&xdrs);

	cu->cu_sendsz = sendsz;
	cu->cu_fd = did;
	cu->cu_closeit = TRUE;
	cl->cl_ops = clnt_door_ops();
	cl->cl_private = (caddr_t)cu;
	cl->cl_auth = authnone_create();
	cl->cl_tp = strdup(rendezvous);
	if (cl->cl_tp == NULL) {
		syslog(LOG_ERR, "clnt_door_create: strdup failed");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto err;
	}
	cl->cl_netid = strdup("door");
	if (cl->cl_netid == NULL) {
		syslog(LOG_ERR, "clnt_door_create: strdup failed");
		if (cl->cl_tp)
			free(cl->cl_tp);
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto err;
	}
	return (cl);
err:
	rpc_createerr.cf_error.re_terrno = 0;
	if (cl) {
		mem_free((caddr_t)cl, sizeof (CLIENT));
		if (cu)
			mem_free((caddr_t)cu, sizeof (*cu));
	}
	close(did);
	return ((CLIENT *)NULL);
}

/* ARGSUSED */
static enum clnt_stat
clnt_door_call(cl, proc, xargs, argsp, xresults, resultsp, utimeout)
	CLIENT		*cl;		/* client handle */
	rpcproc_t	proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	struct timeval	utimeout;	/* seconds to wait before giving up */
{
/* LINTED pointer alignment */
	struct cu_data	*cu = (struct cu_data *)cl->cl_private;
	XDR 		xdrs;
	door_arg_t	params;
	char		*outbuf_ref;
	struct rpc_msg	reply_msg;
	bool_t		need_to_unmap;
	int		nrefreshes = 2;	/* number of times to refresh cred */
	extern int munmap();

	rpc_callerr.re_errno = 0;
	rpc_callerr.re_terrno = 0;

	if ((params.rbuf = (char *) alloca(cu->cu_sendsz)) == NULL) {
		rpc_callerr.re_terrno = 0;
		rpc_callerr.re_errno = errno;
		return (rpc_callerr.re_status = RPC_SYSTEMERROR);
	}
	outbuf_ref = params.rbuf;
	params.rsize = cu->cu_sendsz;
	if ((params.data_ptr = (char *) alloca(cu->cu_sendsz)) == NULL) {
		rpc_callerr.re_terrno = 0;
		rpc_callerr.re_errno = errno;
		return (rpc_callerr.re_status = RPC_SYSTEMERROR);
	}

call_again:
	xdrmem_create(&xdrs, params.data_ptr, cu->cu_sendsz, XDR_ENCODE);
/* LINTED pointer alignment */
	(*(uint32_t *)cu->cu_header)++;	/* increment XID */
	memcpy(params.data_ptr, cu->cu_header, cu->cu_xdrpos);
	XDR_SETPOS(&xdrs, cu->cu_xdrpos);

	if ((!XDR_PUTINT32(&xdrs, (int32_t *)&proc)) ||
				(!AUTH_MARSHALL(cl->cl_auth, &xdrs)) ||
					(!(*xargs)(&xdrs, argsp))) {
		return (rpc_callerr.re_status = RPC_CANTENCODEARGS);
	}
	params.data_size = (int) XDR_GETPOS(&xdrs);

	params.desc_ptr = NULL;
	params.desc_num = 0;
	if (rpc_door_call(cu->cu_fd, &params) < 0) {
		rpc_callerr.re_errno = errno;
		return (rpc_callerr.re_status = RPC_CANTSEND);
	}

	if (params.rbuf == NULL || params.rsize == 0) {
		return (rpc_callerr.re_status = RPC_FAILED);
	}
	need_to_unmap = (params.rbuf != outbuf_ref);

/* LINTED pointer alignment */
	if (*(uint32_t *)params.rbuf != *(uint32_t *)cu->cu_header) {
		rpc_callerr.re_status = RPC_CANTDECODERES;
		goto done;
	}

	xdrmem_create(&xdrs, params.rbuf, params.rsize, XDR_DECODE);
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resultsp;
	reply_msg.acpted_rply.ar_results.proc = xresults;

	if (xdr_replymsg(&xdrs, &reply_msg)) {
		if (reply_msg.rm_reply.rp_stat == MSG_ACCEPTED &&
				reply_msg.acpted_rply.ar_stat == SUCCESS)
			rpc_callerr.re_status = RPC_SUCCESS;
		else
			__seterr_reply(&reply_msg, &rpc_callerr);

		if (rpc_callerr.re_status == RPC_SUCCESS) {
			if (!AUTH_VALIDATE(cl->cl_auth,
					    &reply_msg.acpted_rply.ar_verf)) {
				rpc_callerr.re_status = RPC_AUTHERROR;
				rpc_callerr.re_why = AUTH_INVALIDRESP;
			}
			if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
				xdrs.x_op = XDR_FREE;
				(void) xdr_opaque_auth(&xdrs,
					&(reply_msg.acpted_rply.ar_verf));
			}
		}
		/*
		 * If unsuccesful AND error is an authentication error
		 * then refresh credentials and try again, else break
		 */
		else if (rpc_callerr.re_status == RPC_AUTHERROR) {
			/*
			 * maybe our credentials need to be refreshed ...
			 */
			if (nrefreshes > 0 && AUTH_REFRESH(cl->cl_auth,
				&reply_msg)) {
				nrefreshes--;
				if (need_to_unmap)
					munmap(params.rbuf, params.rsize);
				goto call_again;
			}
		}
	} else
		rpc_callerr.re_status = RPC_CANTDECODERES;

done:
	if (need_to_unmap)
		munmap(params.rbuf, params.rsize);
	return (rpc_callerr.re_status);
}

static void
clnt_door_geterr(cl, errp)
	CLIENT		*cl;
	struct rpc_err	*errp;
{
/* LINTED pointer alignment */
	struct cu_data	*cu = (struct cu_data *)cl->cl_private;

	*errp = rpc_callerr;
}

/* ARGSUSED */
static bool_t
clnt_door_freeres(cl, xdr_res, res_ptr)
	CLIENT		*cl;
	xdrproc_t	xdr_res;
	caddr_t		res_ptr;
{
	XDR		xdrs;

	memset(&xdrs, 0, sizeof (xdrs));
	xdrs.x_op = XDR_FREE;
	return ((*xdr_res)(&xdrs, res_ptr));
}

static void
clnt_door_abort(cl)
	CLIENT		*cl;
{
	cl = cl;
}

static bool_t
clnt_door_control(cl, request, info)
	CLIENT		*cl;
	int		request;
	char		*info;
{
/* LINTED pointer alignment */
	struct cu_data	*cu = (struct cu_data *)cl->cl_private;

	switch (request) {
	case CLSET_FD_CLOSE:
		cu->cu_closeit = TRUE;
		return (TRUE);

	case CLSET_FD_NCLOSE:
		cu->cu_closeit = FALSE;
		return (TRUE);
	}

	/* for other requests which use info */
	if (info == NULL)
		return (FALSE);

	switch (request) {
	case CLGET_FD:
/* LINTED pointer alignment */
		*(int *)info = cu->cu_fd;
		break;

	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure *.
		 * This will get the xid of the PREVIOUS call
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)cu->cu_header);
		break;

	case CLSET_XID:
		/* This will set the xid of the NEXT call */
/* LINTED pointer alignment */
		*(uint32_t *)cu->cu_header =  htonl(*(uint32_t *)info - 1);
		/* decrement by 1 as clnt_door_call() increments once */
		break;

	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
/* LINTED pointer alignment */
		*(uint32_t *)info = ntohl(*(uint32_t *)(cu->cu_header +
						    4 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_VERS:
/* LINTED pointer alignment */
		*(uint32_t *)(cu->cu_header + 4 * BYTES_PER_XDR_UNIT) =
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
		*(uint32_t *)info = ntohl(*(uint32_t *)(cu->cu_header +
						    3 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_PROG:
/* LINTED pointer alignment */
		*(uint32_t *)(cu->cu_header + 3 * BYTES_PER_XDR_UNIT) =
/* LINTED pointer alignment */
			htonl(*(uint32_t *)info);
		break;

	default:
		return (FALSE);
	}
	return (TRUE);
}

static void
clnt_door_destroy(cl)
	CLIENT *cl;
{
/* LINTED pointer alignment */
	struct cu_data	*cu = (struct cu_data *)cl->cl_private;
	int		cu_fd = cu->cu_fd;

	if (cu->cu_closeit)
		close(cu_fd);
	mem_free((caddr_t)cu, sizeof (*cu));
	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) + 1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) + 1);
	mem_free((caddr_t)cl, sizeof (CLIENT));
}

static struct clnt_ops *
clnt_door_ops()
{
	static struct clnt_ops	ops;
	extern mutex_t		ops_lock;
	sigset_t		mask, newmask;

	_sigfillset(&newmask);
	DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_door_call;
		ops.cl_abort = clnt_door_abort;
		ops.cl_geterr = clnt_door_geterr;
		ops.cl_freeres = clnt_door_freeres;
		ops.cl_destroy = clnt_door_destroy;
		ops.cl_control = clnt_door_control;
	}
	mutex_unlock(&ops_lock);
	thr_sigsetmask(SIG_SETMASK, &mask, NULL);
	return (&ops);
}

int
_update_did(cl, vers)
	CLIENT *cl;
	int vers;
{
/* LINTED pointer alignment */
	struct cu_data	*cu = (struct cu_data *)cl->cl_private;
	rpcprog_t prog;
	char rendezvous[64];

	if (cu->cu_fd >= 0)
		close(cu->cu_fd);
/* Make sure that the right door id is used in rpc_door_call. */
	clnt_control(cl, CLGET_PROG, (void *)&prog);
	(void) sprintf(rendezvous, RPC_DOOR_RENDEZVOUS, prog, vers);
	if ((cu->cu_fd = open(rendezvous, O_RDONLY, 0)) < 0) {
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		return (0);
	}
	free(cl->cl_tp);
	cl->cl_tp = strdup(rendezvous);
	if (cl->cl_tp == NULL) {
		syslog(LOG_ERR, "_update_did: strdup failed");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		rpc_createerr.cf_error.re_terrno = 0;
		return (0);
	}
	return (1);
}
