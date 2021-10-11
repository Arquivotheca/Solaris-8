/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_SVC_MT_H
#define	_RPC_SVC_MT_H

#pragma ident	"@(#)svc_mt.h	1.11	99/09/22 SMI"

#include <synch.h>		/* needed for mutex_t declaration */

/*
 * Private service definitions
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SVC flags
 */
#define	SVC_VERSQUIET	0x0001	/* keep quiet about version mismatch */
#define	SVC_DEFUNCT	0x0002	/* xprt is defunct, release asap */
#define	SVC_DGRAM	0x0004	/* datagram type */
#define	SVC_RENDEZVOUS	0x0008	/* rendezvous */
#define	SVC_CONNECTION	0x000c	/* connection */
#define	SVC_DOOR	0x0010	/* door ipc */
#define	SVC_TYPE_MASK	0x001c	/* type mask */
#define	SVC_FAILED	0x0020	/* send/receive failed, used for VC */
#define	SVC_ARGS_CHECK	0x0040	/* flag to check for argument completion */

#define	svc_flags(xprt)		(SVCEXT(xprt)->flags)
#define	version_keepquiet(xprt)	(svc_flags(xprt) & SVC_VERSQUIET)
#define	svc_defunct(xprt)	((svc_flags(xprt) & SVC_DEFUNCT) ? TRUE : FALSE)
#define	svc_failed(xprt)	((svc_flags(xprt) & SVC_FAILED) ? TRUE : FALSE)
#define	svc_type(xprt)		(svc_flags(xprt) & SVC_TYPE_MASK)
#define	svc_send_mutex(xprt)	(SVCEXT(xprt)->send_mutex)


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
 * Interface to server-side authentication flavors, may vary with
 * each request.
 *
 * NOTE: This structure is part of an interface, and must not change.
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

/*
 * The xp_p3 field the the service handle points to the SVCXPRT_EXT
 * extension structure.
 */
typedef struct svcxprt_list_t {
	struct svcxprt_list_t	*next;
	SVCXPRT			*xprt;
} SVCXPRT_LIST;

typedef struct svcxprt_ext_t {
	int		flags;		/* VERSQUIET, DEFUNCT flag */
	SVCXPRT		*parent;	/* points to parent (NULL in parent) */

	struct rpc_msg	*msg;		/* message */
	struct svc_req	*req;		/* request */
	char		*cred_area;	/* auth work area */
	int		refcnt;		/* number of parent references */
	SVCXPRT_LIST	*my_xlist;	/* list header for this copy */
	mutex_t		send_mutex;	/* for sequencing sends */
	SVCAUTH		xp_auth;	/* flavor of current request */
} SVCXPRT_EXT;

#define	SVCEXT(xprt)		((SVCXPRT_EXT *)((xprt)->xp_p3))
#define	SVC_XP_AUTH(xprt)	(SVCEXT(xprt)->xp_auth)

#define	SVCAUTH_WRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops.svc_ah_wrap))(auth, xdrs, xfunc, xwhere))
#define	SVCAUTH_UNWRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops.svc_ah_unwrap))(auth, xdrs, xfunc, xwhere))

/*
 * Door function pointers.
 */
typedef struct {
	int		(*door_create)();
	int		(*door_revoke)();
	int		(*door_info)();
	int		(*door_cred)();
	int	(*door_call)();
	int	(*door_return)();
} rpc_doorcalls_t;

/*
 * Global/module private data and functions
 */
extern SVCXPRT **svc_xports;
extern XDR **svc_xdrs;
extern int svc_mt_mode;
extern mutex_t svc_thr_mutex;
extern cond_t svc_thr_fdwait;
extern int svc_nfds;
extern int svc_nfds_set;
extern int svc_max_fd;
extern mutex_t svc_mutex;
extern mutex_t svc_exit_mutex;
extern int svc_pipe[2];
extern bool_t svc_polling;

SVCXPRT *svc_xprt_alloc();
SVCXPRT *svc_dg_xprtcopy();
SVCXPRT *svc_vc_xprtcopy();
SVCXPRT *svc_fd_xprtcopy();
SVCXPRT *svc_copy();
void svc_xprt_free();
void svc_xprt_destroy();
void svc_dg_xprtfree();
void svc_vc_xprtfree();
void svc_fd_xprtfree();
void svc_door_xprtfree();
void svc_args_done();
void _svc_dg_destroy_private();
void _svc_vc_destroy_private();
void _svc_destroy_private();

extern mutex_t rpc_door_mutex;
extern rpc_doorcalls_t rpcdrc;
bool_t rpc_doorcalls_init();

#define	rpc_door_create		(*rpcdrc.door_create)
#define	rpc_door_revoke		(*rpcdrc.door_revoke)
#define	rpc_door_info		(*rpcdrc.door_info)
#define	rpc_door_cred		(*rpcdrc.door_cred)
#define	rpc_door_call		(*rpcdrc.door_call)
#define	rpc_door_return		(*rpcdrc.door_return)

#define	RPC_DOOR_DIR		"/var/run/rpc_door"
#define	RPC_DOOR_RENDEZVOUS	"/var/run/rpc_door/rpc_%d.%d"

#ifdef __cplusplus
}
#endif

#endif /* !_RPC_SVC_MT_H */
