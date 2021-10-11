/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RESOLV_COMMON_H
#define	_RESOLV_COMMON_H

#pragma ident	"@(#)resolv_common.h	1.7	99/04/27 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Definitions common to rpc.nisd resolv and rpc.resolv code.
 * Stolen from rpcbind and is used to access xprt->xp_p2 fields.
 */

#define	YPDNSPROC4	1L
#define	YPDNSPROC6	2L

#define	MAX_UADDR	52
#define	GETCALLER(xprt)	svc_getrpccaller(xprt)
#define	SETCALLER(xprt, nbufp)	xprt->xp_rtaddr.len = nbufp->len; \
			memcpy(xprt->xp_rtaddr.buf, nbufp->buf, nbufp->len);
#define	MAX_OPT_WORDS   32
#define	RPC_BUF_MAX	32768
struct bogus_data {
	/* XXX: optbuf should be the first field, used by ti_opts.c code */
	struct  netbuf optbuf;			/* netbuf for options */
	long    opts[MAX_OPT_WORDS];		/* options */
	u_int   su_iosz;			/* size of send.recv buffer */
	u_long  su_xid;				/* transaction id */
	XDR	su_xdrs;			/* XDR handle */
	char    su_verfbody[MAX_AUTH_BYTES];    /* verifier body */
	char	*su_cache;			/* cached data, NULL if none */
	struct t_unitdata	su_tudata;	/* tu_data for recv */
};
#define	getbogus_data(xprt) ((struct bogus_data *)(xprt->xp_p2))


struct ypfwdreq_key4 {
	char *map;
	datum keydat;
	unsigned long xid;
	unsigned long ip;
	unsigned short port;
};

struct ypfwdreq_key6 {
	char		*map;
	datum		keydat;
	unsigned long	xid;
	uint32_t	*addr;
	in_port_t	port;
};

extern ulong_t svc_getxid(SVCXPRT *);
extern bool_t xdr_ypfwdreq_key4(XDR *, struct ypfwdreq_key4 *);
extern bool_t xdr_ypfwdreq_key6(XDR *, struct ypfwdreq_key6 *);

#ifdef __cplusplus
}
#endif

#endif	/* _RESOLV_COMMON_H */
