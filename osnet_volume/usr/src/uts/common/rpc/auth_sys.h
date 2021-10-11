/*
 * Copyright (c) 1986-1991,1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * auth_sys.h, Protocol for UNIX style authentication parameters for RPC
 */

#ifndef	_RPC_AUTH_SYS_H
#define	_RPC_AUTH_SYS_H

#pragma ident	"@(#)auth_sys.h	1.22	98/01/06 SMI"

/*
 * The system is very weak.  The client uses no encryption for  it
 * credentials and only sends null verifiers.  The server sends backs
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* The machine name is part of a credential; it may not exceed 255 bytes */
#define	 MAX_MACHINE_NAME 255

/* gids compose part of a credential; there may not be more than 16 of them */
#define	 NGRPS 16

/* gids compose part of a credential; there may not be more than 64 of them */
#define	 NGRPS_LOOPBACK 64

/*
 * "sys" (Old UNIX) style credentials.
 */
struct authsys_parms {
	uint_t	 aup_time;
	char	*aup_machname;
	uid_t	 aup_uid;
	gid_t	 aup_gid;
	uint_t	 aup_len;
	gid_t	*aup_gids;
};
/* For backward compatibility */
#define	 authunix_parms authsys_parms

#ifdef __STDC__
extern bool_t xdr_authsys_parms(XDR *, struct authsys_parms *);
extern bool_t xdr_authloopback_parms(XDR *, struct authsys_parms *);
#else
extern bool_t xdr_authsys_parms();
extern bool_t xdr_authloopback_parms();
#endif


/* For backward compatibility */
#define	xdr_authunix_parms(xdrs, p) xdr_authsys_parms(xdrs, p)

/*
 * If a response verifier has flavor AUTH_SHORT, then the body of
 * the response verifier encapsulates the following structure;
 * again it is serialized in the obvious fashion.
 */
struct short_hand_verf {
	struct opaque_auth new_cred;
};

struct svc_req;

extern bool_t xdr_gid_t(XDR *, gid_t *ip);
extern bool_t xdr_uid_t(XDR *, gid_t *ip);

#ifdef _KERNEL
extern bool_t xdr_authkern(XDR *);
extern bool_t xdr_authloopback(XDR *);
extern enum auth_stat _svcauth_unix(struct svc_req *, struct rpc_msg *);
extern enum auth_stat _svcauth_short(struct svc_req *, struct rpc_msg *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPC_AUTH_SYS_H */
