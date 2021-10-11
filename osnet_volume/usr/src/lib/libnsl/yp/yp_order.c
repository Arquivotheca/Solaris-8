/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#pragma	ident	"@(#)yp_order.c	1.14	97/10/30 SMI"

#define	NULL 0
#include <rpc/rpc.h>
#include "yp_b.h"
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int doorder(char *, char *, struct dom_binding *, struct timeval,
    unsigned long *);


/*
 * This checks parameters, and implements the outer "until binding success"
 * loop.
 */
int
yp_order (domain, map, order)
	char *domain;
	char *map;
	unsigned long *order;
{
	size_t domlen;
	size_t maplen;
	int reason;
	struct dom_binding *pdomb;

	trace1(TR_yp_order, 0);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_order, 1);
		return (YPERR_BADARGS);
	}

	domlen = strlen(domain);
	maplen = strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP) ||
	    (order == NULL)) {
		trace1(TR_yp_order, 1);
		return (YPERR_BADARGS);
	}

	for (;;) {

		if (reason = __yp_dobind(domain, &pdomb)) {
			trace1(TR_yp_order, 1);
			return (reason);
		}

		if (pdomb->dom_binding->ypbind_hi_vers >= YPVERS) {

			reason = doorder(domain, map, pdomb, _ypserv_timeout,
			    order);

			__yp_rel_binding(pdomb);
			if (reason == YPERR_RPC) {
				yp_unbind(domain);
				(void) _sleep(_ypsleeptime);
			} else {
				break;
			}
		} else {
			__yp_rel_binding(pdomb);
			trace1(TR_yp_order, 1);
			return (YPERR_VERS);
		}
	}

	trace1(TR_yp_order, 1);
	return (reason);

}

/*
 * This talks v3 to ypserv
 */
static int
doorder (domain, map, pdomb, timeout, order)
	char *domain;
	char *map;
	struct dom_binding *pdomb;
	struct timeval timeout;
	unsigned long *order;
{
	struct ypreq_nokey req;
	struct ypresp_order resp;
	unsigned int retval = 0;

	trace1(TR_doorder, 0);
	req.domain = domain;
	req.map = map;
	(void) memset((char *)&resp, 0, sizeof (struct ypresp_order));

	/*
	 * Do the get_order request.  If the rpc call failed, return with
	 * status from this point.
	 */

	if (clnt_call(pdomb->dom_client, YPPROC_ORDER,
			(xdrproc_t)xdr_ypreq_nokey,
		    (char *)&req, (xdrproc_t) xdr_ypresp_order, (char *)&resp,
		    timeout) != RPC_SUCCESS) {
		trace1(TR_doorder, 1);
		return (YPERR_RPC);
	}

	/* See if the request succeeded */

	if (resp.status != YP_TRUE) {
		retval = ypprot_err(resp.status);
	}

	*order = (unsigned long)resp.ordernum;
	CLNT_FREERES(pdomb->dom_client,
		(xdrproc_t)xdr_ypresp_order, (char *)&resp);
	trace1(TR_doorder, 1);
	return (retval);

}
