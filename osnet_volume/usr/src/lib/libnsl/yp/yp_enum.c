/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)yp_enum.c	1.17	99/09/20 SMI"

#define	NULL 0
#include <rpc/rpc.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include "yp_b.h"
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern int __yp_dobind_cflookup(char *, struct dom_binding **, int);

static int dofirst(char *, char *, struct dom_binding *, struct timeval,
    char **, int  *, char **, int  *);

static int donext(char *, char *, char *, int, struct dom_binding *,
    struct timeval, char **, int *, char **val, int *);

/*
 * This requests the yp server associated with a given domain to return the
 * first key/value pair from the map data base.  The returned key should be
 * used as an input to the call to ypclnt_next.  This part does the parameter
 * checking, and the do-until-success loop if 'hardlookup' is set.
 */
int
__yp_first_cflookup(
	char *domain,
	char *map,
	char **key,		/* return: key array */
	int  *keylen,		/* return: bytes in key */
	char **val,		/* return: value array */
	int  *vallen,		/* return: bytes in val */
	int  hardlookup)
{
	size_t domlen;
	size_t maplen;
	struct dom_binding *pdomb;
	int reason;

	trace1(TR_yp_first, 0);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_first, 1);
		return (YPERR_BADARGS);
	}

	domlen =  strlen(domain);
	maplen =  strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP)) {
		trace1(TR_yp_first, 1);
		return (YPERR_BADARGS);
	}

	for (;;) {

		if (reason = __yp_dobind_cflookup(domain, &pdomb,
						    hardlookup)) {
			trace1(TR_yp_first, 1);
			return (reason);
		}

		if (pdomb->dom_binding->ypbind_hi_vers == YPVERS) {

			reason = dofirst(domain, map, pdomb, _ypserv_timeout,
			    key, keylen, val, vallen);

			__yp_rel_binding(pdomb);
			if (reason == YPERR_RPC || reason == YPERR_YPSERV ||
			    reason == YPERR_BUSY /* as if */) {
				yp_unbind(domain);
				if (hardlookup)
					(void) _sleep(_ypsleeptime); /* retry */
				else {
					trace1(TR_yp_match, 1);
					return (reason);
				}
			} else
				break;
		} else {
			__yp_rel_binding(pdomb);
			trace1(TR_yp_first, 1);
			return (YPERR_VERS);
		}
	}
	trace1(TR_yp_first, 1);
	return (reason);
}

int
yp_first(
	char *domain,
	char *map,
	char **key,		/* return: key array */
	int  *keylen,		/* return: bytes in key */
	char **val,		/* return: value array */
	int  *vallen)		/* return: bytes in val */
{
	/* traditional yp_firs loops forever until success */
	return (__yp_first_cflookup(domain, map, key, keylen, val, vallen, 1));
}

/*
 * This part of the "get first" interface talks to ypserv.
 */

static int
dofirst(domain, map, pdomb, timeout, key, keylen, val, vallen)
	char *domain;
	char *map;
	struct dom_binding *pdomb;
	struct timeval timeout;
	char **key;
	int  *keylen;
	char **val;
	int  *vallen;

{
	struct ypreq_nokey req;
	struct ypresp_key_val resp;
	unsigned int retval = 0;

	trace1(TR_dofirst, 0);
	req.domain = domain;
	req.map = map;
	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	/*
	 * Do the get first request.  If the rpc call failed, return with status
	 * from this point.
	 */

	(void) memset((char *)&resp, 0, sizeof (struct ypresp_key_val));

	switch (clnt_call(pdomb->dom_client, YPPROC_FIRST,
			(xdrproc_t)xdr_ypreq_nokey,
			(char *)&req, (xdrproc_t)xdr_ypresp_key_val,
			(char *)&resp, timeout)) {
	case RPC_SUCCESS:
		break;
	case RPC_TIMEDOUT:
		trace1(TR_dofirst, 1);
		return (YPERR_YPSERV);
	default:
		trace1(TR_dofirst, 1);
		return (YPERR_RPC);
	}

	/* See if the request succeeded */

	if (resp.status != YP_TRUE) {
		retval = ypprot_err(resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval) {

		if ((*key = malloc((size_t)resp.keydat.dsize + 2)) != NULL) {

			if ((*val = malloc(
			    (size_t)resp.valdat.dsize + 2)) == NULL) {
				free((char *)*key);
				retval = YPERR_RESRC;
			}

		} else {
			retval = YPERR_RESRC;
		}
	}

	/* Copy the returned key and value byte strings into the new memory */

	if (!retval) {
		*keylen = (int)resp.keydat.dsize;
		(void) memcpy(*key, resp.keydat.dptr,
		    (size_t)resp.keydat.dsize);
		(*key)[resp.keydat.dsize] = '\n';
		(*key)[resp.keydat.dsize + 1] = '\0';

		*vallen = (int)resp.valdat.dsize;
		(void) memcpy(*val, resp.valdat.dptr,
		    (size_t)resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}

	CLNT_FREERES(pdomb->dom_client,
		(xdrproc_t)xdr_ypresp_key_val, (char *)&resp);
	trace1(TR_dofirst, 1);
	return (retval);
}

/*
 * This requests the yp server associated with a given domain to return the
 * "next" key/value pair from the map data base.  The input key should be
 * one returned by ypclnt_first or a previous call to ypclnt_next.  The
 * returned key should be used as an input to the next call to ypclnt_next.
 * This part does the parameter checking, and the do-until-success loop.
 * if 'hardlookup' is set.
 */
int
__yp_next_cflookup(
	char *domain,
	char *map,
	char *inkey,
	int  inkeylen,
	char **outkey,		/* return: key array associated with val */
	int  *outkeylen,	/* return: bytes in key */
	char **val,		/* return: value array associated with outkey */
	int  *vallen,		/* return: bytes in val */
	int  hardlookup)
{
	size_t domlen;
	size_t maplen;
	struct dom_binding *pdomb;
	int reason;


	trace1(TR_yp_next, 0);
	if ((map == NULL) || (domain == NULL) || (inkey == NULL)) {
		trace1(TR_yp_next, 1);
		return (YPERR_BADARGS);
	}

	domlen =  strlen(domain);
	maplen =  strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP)) {
		trace1(TR_yp_next, 1);
		return (YPERR_BADARGS);
	}

	for (;;) {
		if (reason = __yp_dobind_cflookup(domain, &pdomb,
						hardlookup)) {
			trace1(TR_yp_next, 1);
			return (reason);
		}

		if (pdomb->dom_binding->ypbind_hi_vers == YPVERS) {

			reason = donext(domain, map, inkey, inkeylen, pdomb,
			    _ypserv_timeout, outkey, outkeylen, val, vallen);

			__yp_rel_binding(pdomb);

			if (reason == YPERR_RPC || reason == YPERR_YPSERV ||
			    reason == YPERR_BUSY /* as if */) {
				yp_unbind(domain);
				if (hardlookup)
					(void) _sleep(_ypsleeptime); /* retry */
				else {
					trace1(TR_yp_match, 1);
					return (reason);
				}
			} else
				break;
		} else {
			__yp_rel_binding(pdomb);
			trace1(TR_yp_next, 1);
			return (YPERR_VERS);
		}
	}

	trace1(TR_yp_next, 1);
	return (reason);
}

int
yp_next(
	char *domain,
	char *map,
	char *inkey,
	int  inkeylen,
	char **outkey,		/* return: key array associated with val */
	int  *outkeylen,	/* return: bytes in key */
	char **val,		/* return: value array associated with outkey */
	int  *vallen)		/* return: bytes in val */
{
	/* traditional yp_next loops forever until success */
	return (__yp_next_cflookup(domain, map, inkey, inkeylen, outkey,
				outkeylen, val, vallen, 1));
}


/*
 * This part of the "get next" interface talks to ypserv.
 */
static int
donext(domain, map, inkey, inkeylen, pdomb, timeout, outkey, outkeylen,
    val, vallen)
	char *domain;
	char *map;
	char *inkey;
	int  inkeylen;
	struct dom_binding *pdomb;
	struct timeval timeout;
	char **outkey;		/* return: key array associated with val */
	int  *outkeylen;	/* return: bytes in key */
	char **val;		/* return: value array associated with outkey */
	int  *vallen;		/* return: bytes in val */

{
	struct ypreq_key req;
	struct ypresp_key_val resp;
	unsigned int retval = 0;

	trace2(TR_donext, 0, inkeylen);
	req.domain = domain;
	req.map = map;
	req.keydat.dptr = inkey;
	req.keydat.dsize = inkeylen;

	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	/*
	 * Do the get next request.  If the rpc call failed, return with status
	 * from this point.
	 */

	switch (clnt_call(pdomb->dom_client,
			YPPROC_NEXT, (xdrproc_t)xdr_ypreq_key, (char *)&req,
			(xdrproc_t)xdr_ypresp_key_val, (char *)&resp,
			timeout)) {
	case RPC_SUCCESS:
		break;
	case RPC_TIMEDOUT:
		trace1(TR_dofirst, 1);
		return (YPERR_YPSERV);
	default:
		trace1(TR_dofirst, 1);
		return (YPERR_RPC);
	}

	/* See if the request succeeded */

	if (resp.status != YP_TRUE) {
		retval = ypprot_err(resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval) {
		if ((*outkey = malloc((size_t)
		    resp.keydat.dsize + 2)) != NULL) {

			if ((*val = malloc((size_t)
			    resp.valdat.dsize + 2)) == NULL) {
				free((char *)*outkey);
				retval = YPERR_RESRC;
			}

		} else {
			retval = YPERR_RESRC;
		}
	}

	/* Copy the returned key and value byte strings into the new memory */

	if (!retval) {
		*outkeylen = (int)resp.keydat.dsize;
		(void) memcpy(*outkey, resp.keydat.dptr,
		    (size_t)resp.keydat.dsize);
		(*outkey)[resp.keydat.dsize] = '\n';
		(*outkey)[resp.keydat.dsize + 1] = '\0';

		*vallen = (int)resp.valdat.dsize;
		(void) memcpy(*val, resp.valdat.dptr,
		    (size_t)resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}

	CLNT_FREERES(pdomb->dom_client, (xdrproc_t)xdr_ypresp_key_val,
		    (char *)&resp);
	trace1(TR_donext, 1);
	return (retval);
}
