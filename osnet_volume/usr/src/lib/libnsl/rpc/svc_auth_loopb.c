/*
 * Copyright (c) 1997, by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)svc_auth_loopb.c	1.3	97/12/17 SMI"

/*
 * svc_auth_loopb.c
 * Handles the loopback UNIX flavor authentication parameters on the
 * service side of rpc.
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <syslog.h>
#include <sys/types.h>

/*
 * Loopback system (Unix) longhand authenticator
 */
enum auth_stat
__svcauth_loopback(struct svc_req *rqst, struct rpc_msg *msg)
{
	enum auth_stat stat;
	XDR xdrs;
	struct authsys_parms *aup;
	rpc_inline_t *buf;
	struct area {
		struct authsys_parms area_aup;
		char area_machname[MAX_MACHINE_NAME+1];
		gid_t area_gids[NGRPS_LOOPBACK];
	} *area;
	size_t auth_len;
	size_t str_len, gid_len;
	int i;

	area = (struct area *)rqst->rq_clntcred;
	aup = &area->area_aup;
	aup->aup_machname = area->area_machname;
	aup->aup_gids = area->area_gids;
	auth_len = (size_t)msg->rm_call.cb_cred.oa_length;
	xdrmem_create(&xdrs, msg->rm_call.cb_cred.oa_base, auth_len,
	    XDR_DECODE);
	buf = XDR_INLINE(&xdrs, auth_len);
	if (buf != NULL) {
		aup->aup_time = IXDR_GET_INT32(buf);
		str_len = IXDR_GET_U_INT32(buf);
		if (str_len > MAX_MACHINE_NAME) {
			stat = AUTH_BADCRED;
			goto done;
		}
		(void) memcpy(aup->aup_machname, buf, str_len);
		aup->aup_machname[str_len] = 0;
		str_len = RNDUP(str_len);
		buf += str_len / sizeof (int);
		aup->aup_uid = IXDR_GET_INT32(buf);
		aup->aup_gid = IXDR_GET_INT32(buf);
		gid_len = IXDR_GET_U_INT32(buf);
		if (gid_len > NGRPS_LOOPBACK) {
			stat = AUTH_BADCRED;
			goto done;
		}
		aup->aup_len = gid_len;
		for (i = 0; i < gid_len; i++) {
			aup->aup_gids[i] = (gid_t)IXDR_GET_INT32(buf);
		}
		/*
		 * five is the smallest unix credentials structure -
		 * timestamp, hostname len (0), uid, gid, and gids len (0).
		 */
		if ((5 + gid_len) * BYTES_PER_XDR_UNIT + str_len > auth_len) {
#ifdef	KERNEL
			printf("bad auth_len gid %lu str %lu auth %lu",
			    gid_len, str_len, auth_len);
#else
			(void) syslog(LOG_ERR,
			    "bad auth_len gid %lu str %lu auth %lu",
			    gid_len, str_len, auth_len);
#endif
			stat = AUTH_BADCRED;
			goto done;
		}
	} else if (!xdr_authloopback_parms(&xdrs, aup)) {
		xdrs.x_op = XDR_FREE;
		(void) xdr_authloopback_parms(&xdrs, aup);
		stat = AUTH_BADCRED;
		goto done;
	}
	rqst->rq_xprt->xp_verf.oa_flavor = AUTH_NULL;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	stat = AUTH_OK;
done:
	XDR_DESTROY(&xdrs);
	return (stat);
}
