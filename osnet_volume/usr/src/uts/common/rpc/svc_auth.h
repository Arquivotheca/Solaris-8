/*
 * Copyright (c) 1986-1993,1995,1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_SVCAUTH_H
#define	_RPC_SVCAUTH_H

#pragma ident	"@(#)svc_auth.h	1.19	98/01/06 SMI"

/*	 svc_auth.h 1.10 88/10/25 SMI	*/

/*
 * svc_auth.h, Service side of rpc authentication.
 */
#include <rpc/rpcsec_gss.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Server side authenticator
 */
#ifdef _KERNEL
/*
 * Copy of GSS parameters, needed for MT operation
 */
typedef struct {
	bool_t			established;
	rpc_gss_service_t	service;
	uint_t			qop_rcvd;
	void			*context;
	uint_t			seq_num;
} svc_rpc_gss_parms_t;

/*
 * sec_svc_control() commands
 */
#define	RPC_SVC_SET_GSS_CALLBACK	1  /* set rpcsec_gss callback routine */
extern bool_t sec_svc_control();

/*
 * Interface to server-side authentication flavors, may change on
 * each request.
 */
typedef struct {
	struct svc_auth_ops {
		int		(*svc_ah_wrap)();
		int		(*svc_ah_unwrap)();
	} svc_ah_ops;
	caddr_t			svc_ah_private;
	svc_rpc_gss_parms_t	svc_gss_parms;
	rpc_gss_rawcred_t	raw_cred;
} SVCAUTH;

#define	SVCAUTH_GSSPARMS(auth)  ((svc_rpc_gss_parms_t *)&(auth)->svc_gss_parms)

/*
 * Auth flavors can now apply a transformation in addition to simple XDR
 * on the body of a call/response in ways that depend on the flavor being
 * used.  These interfaces provide a generic interface between the
 * internal RPC frame and the auth flavor specific code to allow the
 * auth flavor to encode (WRAP) or decode (UNWRAP) the body.
 */
#define	SVCAUTH_WRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops.svc_ah_wrap))(auth, xdrs, xfunc, xwhere))
#define	SVCAUTH_UNWRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops.svc_ah_unwrap))(auth, xdrs, xfunc, xwhere))

/*
 * Server side authenticator
 */
#ifdef __STDC__
extern enum auth_stat sec_svc_msg(struct svc_req *, struct rpc_msg *,
				bool_t *);
#else
extern enum auth_stat sec_svc_msg();
#endif /* __STDC__ */

#else

#ifdef __STDC__
extern enum auth_stat __gss_authenticate(struct svc_req *, struct rpc_msg *,
				bool_t *);
extern enum auth_stat __authenticate(struct svc_req *, struct rpc_msg *);
#else
extern enum auth_stat __gss_authenticate();
extern enum auth_stat __authenticate();
#endif /* __STDC__ */

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _RPC_SVCAUTH_H */
