/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

/*
 * YP updater interface
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include "yp_b.h"
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/ypupd.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <stdlib.h>

#define	WINDOW (60*60)
#define	TOTAL_TIMEOUT	300

#ifdef DEBUG
#define	debugging 1
#define	debug(msg)  (void) fprintf(stderr, "%s\n", msg);
#else
#define	debugging 0
#define	debug(msg)
#endif
extern AUTH *authdes_seccreate();

yp_update(domain, map, op, key, keylen, data, datalen)
	char *domain;
	char *map;
	unsigned op;
	char *key;
	int keylen;
	char *data;
	int datalen;
{
	struct ypupdate_args args;
	u_int rslt;
	struct timeval total;
	CLIENT *client;
	char *ypmaster;
	char ypmastername[MAXNETNAMELEN+1];
	enum clnt_stat stat;
	u_int proc;

	trace3(TR_yp_update, 0, keylen, datalen);
	switch (op) {
	case YPOP_DELETE:
		proc = YPU_DELETE;
		break;
	case YPOP_INSERT:
		proc = YPU_INSERT;
		break;
	case YPOP_CHANGE:
		proc = YPU_CHANGE;
		break;
	case YPOP_STORE:
		proc = YPU_STORE;
		break;
	default:
		trace1(TR_yp_update, 1);
		return (YPERR_BADARGS);
	}
	if (yp_master(domain, map, &ypmaster) != 0) {
		debug("no master found");
		trace1(TR_yp_update, 1);
		return (YPERR_BADDB);
	}

	client = clnt_create(ypmaster, YPU_PROG, YPU_VERS, "circuit_n");
	if (client == NULL) {
#ifdef DEBUG
		/* CONSTCOND */
		if (debugging) {
			clnt_pcreateerror("client create failed");
		}
#endif /* DEBUG */
		free(ypmaster);
		trace1(TR_yp_update, 1);
		return (YPERR_RPC);
	}

	if (! host2netname(ypmastername, ypmaster, domain)) {
		clnt_destroy(client);
		free(ypmaster);
		trace1(TR_yp_update, 1);
		return (YPERR_BADARGS);
	}
	client->cl_auth = authdes_seccreate(ypmastername, WINDOW,
				ypmaster, NULL);
	free(ypmaster);
	if (client->cl_auth == NULL) {
		debug("auth create failed");
		clnt_destroy(client);
		trace1(TR_yp_update, 1);
		return (YPERR_RPC);
	}

	args.mapname = map;
	args.key.yp_buf_len = keylen;
	args.key.yp_buf_val = key;
	args.datum.yp_buf_len = datalen;
	args.datum.yp_buf_val = data;

	total.tv_sec = TOTAL_TIMEOUT;
	total.tv_usec = 0;
	clnt_control(client, CLSET_TIMEOUT, (char *)&total);
	stat = clnt_call(client, proc,
		xdr_ypupdate_args, (char *)&args,
		xdr_u_int, (char *)&rslt, total);

	if (stat != RPC_SUCCESS) {
#ifdef DEBUG
		debug("ypupdate RPC call failed");
		/* CONSTCOND */
		if (debugging)
			clnt_perror(client, "ypupdate call failed");
#endif /* DEBUG */
		rslt = YPERR_RPC;
	}
	auth_destroy(client->cl_auth);
	clnt_destroy(client);
	trace1(TR_yp_update, 1);
	return (rslt);
}
