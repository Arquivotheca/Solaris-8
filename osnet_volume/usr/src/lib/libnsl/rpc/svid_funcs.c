/*
 * Copyright 1991, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)svid_funcs.c	1.8	97/12/17 SMI"

/*
 *	These functions are documented in the SVID as being part of libnsl.
 *	They are also defined as macros in various RPC header files.  To
 *	ensure that these interfaces exist as functions, we've created this
 *	(we hope unused) file.
 */

#include <rpc/rpc.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <synch.h>

/* LINTLIBRARY */

#undef	auth_destroy
#undef	clnt_call
#undef	clnt_control
#undef	clnt_destroy
#undef	clnt_freeres
#undef	clnt_geterr
#undef	svc_destroy
#undef	svc_freeargs
#undef	svc_getargs
#undef	svc_getrpccaller
#undef	xdr_destroy
#undef	xdr_getpos
#undef	xdr_inline
#undef	xdr_setpos

extern int __svc_versquiet_get();
extern void __svc_versquiet_off();
extern void __svc_versquiet_on();

void
auth_destroy(auth)
	AUTH	*auth;
{
	trace1(TR_auth_destroy, 0);
	((*((auth)->ah_ops->ah_destroy))(auth));
	trace1(TR_auth_destroy, 1);
}

enum clnt_stat
clnt_call(cl, proc, xargs, argsp, xres, resp, timeout)
	CLIENT		*cl;
	uint32_t	proc;
	xdrproc_t	xargs;
	caddr_t		argsp;
	xdrproc_t	xres;
	caddr_t		resp;
	struct timeval	timeout;
{
	enum clnt_stat dummy;

	trace2(TR_clnt_call, 0, proc);
	dummy = (*(cl)->cl_ops->cl_call)(cl, proc, xargs, argsp, xres, resp,
		timeout);
	trace2(TR_clnt_call, 1, proc);
	return (dummy);
}

bool_t
clnt_control(cl, rq, in)
	CLIENT	*cl;
	u_int	rq;
	char	*in;
{
	bool_t dummy;

	trace2(TR_clnt_control, 0, rq);
	dummy = (*(cl)->cl_ops->cl_control)(cl, rq, in);
	trace2(TR_clnt_control, 1, rq);
	return (dummy);
}


void
clnt_destroy(cl)
	CLIENT	*cl;
{
	trace2(TR_clnt_destroy, 0, cl);
	((*(cl)->cl_ops->cl_destroy)(cl));
	trace2(TR_clnt_destroy, 1, cl);
}

bool_t
clnt_freeres(cl, xres, resp)
	CLIENT		*cl;
	xdrproc_t	xres;
	caddr_t		resp;
{
	bool_t dummy;

	trace2(TR_clnt_freeres, 0, cl);
	dummy = (*(cl)->cl_ops->cl_freeres)(cl, xres, resp);
	trace2(TR_clnt_freeres, 1, cl);
	return (dummy);
}

void
clnt_geterr(cl, errp)
	CLIENT		*cl;
	struct rpc_err	*errp;
{
	trace2(TR_clnt_geterr, 0, cl);
	(*(cl)->cl_ops->cl_geterr)(cl, errp);
	trace2(TR_clnt_geterr, 1, cl);
}

bool_t
svc_control(xprt, rq, in)
	SVCXPRT		*xprt;
	const u_int	rq;
	void		*in;
{
	bool_t retval;

	trace2(TR_svc_control, 0, rq);
	switch (rq) {
	case SVCGET_VERSQUIET:
		*((int *) in) = __svc_versquiet_get(xprt);
		retval = TRUE;
		break;

	case SVCSET_VERSQUIET:
		if (*((int *) in) == 0)
			__svc_versquiet_off(xprt);
		else
			__svc_versquiet_on(xprt);
		retval = TRUE;
		break;

	default:
		retval = (*(xprt)->xp_ops->xp_control)(xprt, rq, in);
	}
	trace3(TR_svc_control, 1, rq, retval);
	return (retval);
}

void
svc_destroy(xprt)
	SVCXPRT	*xprt;
{
	trace1(TR_svc_destroy, 0);
	(*(xprt)->xp_ops->xp_destroy)(xprt);
	trace1(TR_svc_destroy, 1);
}

bool_t
svc_freeargs(xprt, xargs, argsp)
	SVCXPRT		*xprt;
	xdrproc_t	xargs;
	char		*argsp;
{
	bool_t dummy;

	trace1(TR_svc_freeargs, 0);
	dummy = (*(xprt)->xp_ops->xp_freeargs)(xprt, xargs, argsp);
	trace1(TR_svc_freeargs, 1);
	return (dummy);
}

bool_t
svc_getargs(xprt, xargs, argsp)
	SVCXPRT		*xprt;
	xdrproc_t	xargs;
	char		*argsp;
{
	bool_t dummy;

	trace1(TR_svc_getargs, 0);
	dummy = (*(xprt)->xp_ops->xp_getargs)(xprt, xargs, argsp);
	trace1(TR_svc_getargs, 1);
	return (dummy);
}

struct netbuf *
svc_getrpccaller(xprt)
	SVCXPRT	*xprt;
{
	struct netbuf *dummy;

	trace1(TR_svc_getrpccaller, 0);
	dummy = &(xprt)->xp_rtaddr;
	trace1(TR_svc_getrpccaller, 1);
	return (dummy);
}

void
xdr_destroy(xdrs)
	XDR	*xdrs;
{
	trace1(TR_xdr_destroy, 0);
	(*(xdrs)->x_ops->x_destroy)(xdrs);
	trace1(TR_xdr_destroy, 1);
}

u_int
xdr_getpos(xdrs)
	XDR	*xdrs;
{
	u_int dummy;

	trace1(TR_xdr_getpos, 0);
	dummy = (*(xdrs)->x_ops->x_getpostn)(xdrs);
	trace1(TR_xdr_getpos, 1);
	return (dummy);
}

rpc_inline_t *
xdr_inline(xdrs, len)
	XDR	*xdrs;
	int	len;
{
	rpc_inline_t *dummy;

	trace2(TR_xdr_inline, 0, len);
	dummy = (*(xdrs)->x_ops->x_inline)(xdrs, len);
	trace2(TR_xdr_inline, 1, len);
	return (dummy);
}

bool_t
xdr_setpos(xdrs, pos)
	XDR	*xdrs;
	u_int	pos;
{
	bool_t dummy;

	trace2(TR_xdr_setpos, 0, pos);
	dummy = (*(xdrs)->x_ops->x_setpostn)(xdrs, pos);
	trace2(TR_xdr_setpos, 1, pos);
	return (dummy);
}
