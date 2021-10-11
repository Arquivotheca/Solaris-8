/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#include <rpc/rpc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include "yp_b.h"
#define	bzero(a, b) (void) memset(a, 0, b)
#define	YPBIND_ERR_ERR 1		/* Internal error */
#define	YPBIND_ERR_NOSERV 2		/* No bound server for passed domain */
#define	YPBIND_ERR_RESC 3		/* System resource allocation failure */
#define	YPBIND_ERR_NODOMAIN 4		/* Domain doesn't exist */

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

void *
ypbindproc_null_3(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	trace2(TR_ypbindproc_null_3, 0, clnt);
	bzero((char *)&res, sizeof (res));
	if (clnt_call(clnt, YPBINDPROC_NULL, xdr_void,
		argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		trace1(TR_ypbindproc_null_3, 1);
		return (NULL);
	}
	trace1(TR_ypbindproc_null_3, 1);
	return ((void *)&res);
}

ypbind_resp *
ypbindproc_domain_3(argp, clnt)
	ypbind_domain *argp;
	CLIENT *clnt;
{
	static ypbind_resp res;

	trace2(TR_ypbindproc_domain_3, 0, clnt);
	bzero((char *)&res, sizeof (res));
	if (clnt_call(clnt, YPBINDPROC_DOMAIN,
		xdr_ypbind_domain, (char *)argp, xdr_ypbind_resp,
		(char *)&res, TIMEOUT) != RPC_SUCCESS) {
		trace1(TR_ypbindproc_domain_3, 1);
		return (NULL);
	}
	trace1(TR_ypbindproc_domain_3, 1);
	return (&res);
}

void *
ypbindproc_setdom_3(argp, clnt)
	ypbind_setdom *argp;
	CLIENT *clnt;
{
	static char res;

	trace2(TR_ypbindproc_setdom_3, 0, clnt);
	bzero((char *)&res, sizeof (res));
	if (clnt_call(clnt, YPBINDPROC_SETDOM,
		xdr_ypbind_setdom, (char *)argp, xdr_void, &res,
		TIMEOUT) != RPC_SUCCESS) {
		trace1(TR_ypbindproc_setdom_3, 1);
		return (NULL);
	}
	trace1(TR_ypbindproc_setdom_3, 1);
	return ((void *)&res);
}
