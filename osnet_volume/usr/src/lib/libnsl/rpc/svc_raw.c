/*
 * Copyright (c) 1984-1996,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)svc_raw.c	1.27	99/07/19 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)svc_raw.c 1.25 89/01/31 Copyr 1984 Sun Micro";
#endif

/*
 * svc_raw.c,   This a toy for simple testing and timing.
 * Interface to create an rpc client and server in the same UNIX process.
 * This lets us similate rpc and get rpc (round trip) overhead, without
 * any interference from the kernal.
 *
 */

#include "rpc_mt.h"
#include <rpc/rpc.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <rpc/raw.h>
#include <syslog.h>

#ifndef UDPMSGSIZE
#define	UDPMSGSIZE 8800
#endif

/*
 * This is the "network" that we will be moving data over
 */
static struct svc_raw_private {
	char	*raw_buf;	/* should be shared with the cl handle */
	SVCXPRT	*server;
	XDR	xdr_stream;
	char	verf_body[MAX_AUTH_BYTES];
} *svc_raw_private;

static struct xp_ops *svc_raw_ops();
extern char *calloc();
extern void free();
extern mutex_t	svcraw_lock;



SVCXPRT *
svc_raw_create()
{
	register struct svc_raw_private *srp;
	bool_t flag1 = FALSE, flag2 = FALSE;

/* VARIABLES PROTECTED BY svcraw_lock: svc_raw_private, srp */
	trace1(TR_svc_raw_create, 0);
	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
/* LINTED pointer alignment */
		srp = (struct svc_raw_private *)calloc(1, sizeof (*srp));
		if (srp == NULL) {
			syslog(LOG_ERR, "svc_raw_create: out of memory");
			mutex_unlock(&svcraw_lock);
			trace1(TR_svc_raw_create, 1);
			return ((SVCXPRT *)NULL);
		}
		flag1 = TRUE;
		if (_rawcombuf == NULL) {
			_rawcombuf = (char *)calloc(UDPMSGSIZE, sizeof (char));
			if (_rawcombuf == NULL) {
				free((char *)srp);
				syslog(LOG_ERR, "svc_raw_create: "
					"out of memory");
				mutex_unlock(&svcraw_lock);
				trace1(TR_svc_raw_create, 1);
				return ((SVCXPRT *)NULL);
			}
			flag2 = TRUE;
		}
		srp->raw_buf = _rawcombuf; /* Share it with the client */
		svc_raw_private = srp;
	}
	if ((srp->server = svc_xprt_alloc()) == NULL) {
		if (flag2)
			free(svc_raw_private->raw_buf);
		if (flag1)
			free(svc_raw_private);
		mutex_unlock(&svcraw_lock);
		trace1(TR_svc_raw_create, 1);
		return ((SVCXPRT *)NULL);
	}
	/*
	 * By convention, using FD_SETSIZE as the psuedo file descriptor
	 */
	srp->server->xp_fd = FD_SETSIZE;
	srp->server->xp_port = 0;
	srp->server->xp_ops = svc_raw_ops();
	srp->server->xp_verf.oa_base = srp->verf_body;
	xdrmem_create(&srp->xdr_stream, srp->raw_buf, UDPMSGSIZE, XDR_DECODE);
	xprt_register(srp->server);
	mutex_unlock(&svcraw_lock);
	trace1(TR_svc_raw_create, 1);
	return (srp->server);
}

/*ARGSUSED*/
static enum xprt_stat
svc_raw_stat(xprt)
SVCXPRT *xprt; /* args needed to satisfy ANSI-C typechecking */
{
	trace1(TR_svc_raw_stat, 0);
	trace1(TR_svc_raw_stat, 1);
	return (XPRT_IDLE);
}

/*ARGSUSED*/
static bool_t
svc_raw_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	register struct svc_raw_private *srp;
	register XDR *xdrs;

	trace1(TR_svc_raw_recv, 0);
	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		trace1(TR_svc_raw_recv, 1);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_DECODE;
	(void) XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg)) {
		trace1(TR_svc_raw_recv, 1);
		return (FALSE);
	}
	trace1(TR_svc_raw_recv, 1);
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
svc_raw_reply(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	register struct svc_raw_private *srp;
	register XDR *xdrs;

	trace1(TR_svc_raw_reply, 0);
	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		trace1(TR_svc_raw_reply, 1);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_ENCODE;
	(void) XDR_SETPOS(xdrs, 0);
	if (! xdr_replymsg(xdrs, msg)) {
		trace1(TR_svc_raw_reply, 1);
		return (FALSE);
	}
	(void) XDR_GETPOS(xdrs);  /* called just for overhead */
	trace1(TR_svc_raw_reply, 1);
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
svc_raw_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	register struct svc_raw_private *srp;
	bool_t dummy1;

	trace1(TR_svc_raw_getargs, 0);
	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		trace1(TR_svc_raw_getargs, 1);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);
	dummy1 = (*xdr_args)(&srp->xdr_stream, args_ptr);
	trace1(TR_svc_raw_getargs, 1);
	return (dummy1);
}

/*ARGSUSED*/
static bool_t
svc_raw_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	register struct svc_raw_private *srp;
	register XDR *xdrs;
	bool_t dummy2;

	trace1(TR_svc_raw_freeargs, 0);
	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		trace1(TR_svc_raw_freeargs, 1);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_FREE;
	dummy2 = (*xdr_args)(xdrs, args_ptr);
	trace1(TR_svc_raw_freeargs, 1);
	return (dummy2);
}

/*ARGSUSED*/
static void
svc_raw_destroy(xprt)
SVCXPRT *xprt;
{
	trace1(TR_svc_raw_destroy, 0);
	trace1(TR_svc_raw_destroy, 1);
}

/*ARGSUSED*/
static bool_t
svc_raw_control(xprt, rq, in)
	register SVCXPRT *xprt;
	const uint_t	rq;
	void		*in;
{
	trace3(TR_svc_raw_control, 0, xprt, rq);
	switch (rq) {
	case SVCGET_XID: /* fall through for now */
	default:
		trace1(TR_svc_raw_control, 1);
		return (FALSE);
	}
}

static struct xp_ops *
svc_raw_ops()
{
	static struct xp_ops ops;
	extern mutex_t ops_lock;

/* VARIABLES PROTECTED BY ops_lock: ops */

	trace1(TR_svc_raw_ops, 0);
	mutex_lock(&ops_lock);
	if (ops.xp_recv == NULL) {
		ops.xp_recv = svc_raw_recv;
		ops.xp_stat = svc_raw_stat;
		ops.xp_getargs = svc_raw_getargs;
		ops.xp_reply = svc_raw_reply;
		ops.xp_freeargs = svc_raw_freeargs;
		ops.xp_destroy = svc_raw_destroy;
		ops.xp_control = svc_raw_control;
	}
	mutex_unlock(&ops_lock);
	trace1(TR_svc_raw_ops, 1);
	return (&ops);
}
