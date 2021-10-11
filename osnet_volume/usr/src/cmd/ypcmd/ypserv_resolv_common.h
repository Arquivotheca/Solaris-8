/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RESOLV_COMMON_H
#define	_RESOLV_COMMON_H

#pragma ident	"@(#)ypserv_resolv_common.h	1.6	99/04/27 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definitions common to ypserv, rpc.nisd_resolv and rpc.resolv code.
 * Stolen from rpcbind and is used to access xprt->xp_p2 fields.
 */

#define	YPDNSPROC4	1L
#define	YPDNSPROC6	2L

#ifdef TDRPC	/* ****** 4.1 ******** */

#define	xdrproc_t bool
#define	GETCALLER(xprt)	svc_getcaller(xprt)
#define	SETCALLER(xprt, addrp)	*(svc_getcaller(xprt)) = *addrp;
struct bogus_data {
	u_int   su_iosz;
	u_long  su_xid;
	XDR	su_xdrs;	/* XDR handle */
	char    su_verfbody[MAX_AUTH_BYTES];    /* verifier body */
	char	*su_cache;	/* cached data, NULL if no cache */
};
#define	getbogus_data(xprt) ((struct bogus_data *) (xprt->xp_p2))

#else		/* ****** 5.x ******** */

#define	MAX_UADDR	25
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
#define	getbogus_data(xprt) ((struct bogus_data *) (xprt->xp_p2))

#endif		/* ****** end ******** */


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

extern u_long svc_getxid(SVCXPRT *xprt);
extern bool_t xdr_ypfwdreq_key4(XDR *, struct ypfwdreq_key4 *);
extern bool_t xdr_ypfwdreq_key6(XDR *, struct ypfwdreq_key6 *);

#ifdef __cplusplus
}
#endif

#endif	/* _RESOLV_COMMON_H */
