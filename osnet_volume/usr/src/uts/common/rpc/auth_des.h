/*
 * Copyright (c) 1986-1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RPC_AUTH_DES_H
#define	_RPC_AUTH_DES_H

#pragma ident	"@(#)auth_des.h	1.19	99/07/20 SMI"

/*
 * auth_des.h, Protocol for DES style authentication for RPC
 *
 */

#ifdef _KERNEL
#include <rpc/svc.h>
#endif /* _KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * There are two kinds of "names": fullnames and nicknames
 */
enum authdes_namekind {
	ADN_FULLNAME,
	ADN_NICKNAME
};

/*
 * A fullname contains the network name of the client,
 * a conversation key and the window
 */
struct authdes_fullname {
	char *name;	/* network name of client, up to MAXNETNAMELEN */
	des_block key;	/* conversation key */
	uint32_t window;	/* associated window */
};


/*
 * A credential
 */
struct authdes_cred {
	enum authdes_namekind adc_namekind;
	struct authdes_fullname adc_fullname;
	uint32_t adc_nickname;
};

/*
 * A des authentication verifier
 */
struct authdes_verf {
	union {
		struct timeval adv_ctime;	/* clear time */
		des_block adv_xtime;		/* crypt time */
	} adv_time_u;
	uint32_t adv_int_u;
};

/*
 * des authentication verifier: client variety
 *
 * adv_timestamp is the current time.
 * adv_winverf is the credential window + 1.
 * Both are encrypted using the conversation key.
 */
#define	adv_timestamp	adv_time_u.adv_ctime
#define	adv_xtimestamp	adv_time_u.adv_xtime
#define	adv_winverf	adv_int_u

/*
 * des authentication verifier: server variety
 *
 * adv_timeverf is the client's timestamp + client's window
 * adv_nickname is the server's nickname for the client.
 * adv_timeverf is encrypted using the conversation key.
 */
#define	adv_timeverf	adv_time_u.adv_ctime
#define	adv_xtimeverf	adv_time_u.adv_xtime
#define	adv_nickname	adv_int_u

/*
 * Map a des credential into a unix cred.
 *
 *  authdes_getucred(adc, uid, gid, grouplen, groups)
 *	struct authdes_cred *adc;
 *	uid_t *uid;
 *	gid_t *gid;
 *	short *grouplen;
 *	gid_t *groups;
 *
 */

#ifdef __STDC__
extern int	authdes_getucred(const struct authdes_cred *,
			uid_t *, gid_t *, short *, gid_t *);
#else
extern int	authdes_getucred();
#endif

#ifndef _KERNEL
#ifdef __STDC__
extern int	getpublickey(const char *, char *);
extern int	getsecretkey(const char *, char *, const char *);
#else
extern int	getpublickey();
extern int	getsecretkey();
#endif
#endif

#ifdef _KERNEL

#ifdef __STDC__
extern int	authdes_create(char *, uint_t, struct netbuf *,
			struct knetconfig *, des_block *, int, AUTH **);
extern bool_t	xdr_authdes_cred(XDR *, struct authdes_cred *);
extern bool_t	xdr_authdes_verf(XDR *, struct authdes_verf *);
extern int	rtime(struct knetconfig *, struct netbuf *, int,
			struct timeval *, struct timeval *);
extern enum clnt_stat kgetnetname(char *);
extern enum auth_stat _svcauth_des(struct svc_req *, struct rpc_msg *);
#else
extern int	authdes_create();
extern bool_t	xdr_authdes_cred();
extern bool_t	xdr_authdes_verf();
extern int	rtime();
extern enum clnt_stat	kgetnetname();
extern enum auth_stat _svcauth_des();
#endif

extern kmutex_t	authdes_ops_lock;

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _RPC_AUTH_DES_H */
