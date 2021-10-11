/*
 * Copyright (c) 1986-1991, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * svc_soc.h, Server-side remote procedure call interface.
 *
 * All the following declarations are only for backward compatibility
 * with SUNOS 4.0.
 */

#ifndef _RPC_SVC_SOC_H
#define	_RPC_SVC_SOC_H

#pragma ident	"@(#)svc_soc.h	1.18	99/08/13 SMI"

#ifndef _KERNEL

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Approved way of getting address of caller
 */
#define	svc_getcaller(x)	((struct sockaddr_in *)(x)->xp_rtaddr.buf)

/*
 * Service registration and unregistration.
 *
 * svc_register(xprt, prog, vers, dispatch, protocol)
 * svc_unregister(prog, vers);
 */
#ifdef __STDC__
extern bool_t svc_register(SVCXPRT *, rpcprog_t, rpcvers_t,
    void (*)(struct svc_req *, SVCXPRT *), int);
extern void svc_unregister(rpcprog_t, rpcvers_t);

/*
 * Memory based rpc for testing and timing.
 */
extern SVCXPRT *svcraw_create(void);

/*
 * Udp based rpc. For compatibility reasons
 */
extern SVCXPRT *svcudp_create(int);
extern SVCXPRT *svcudp_bufcreate(int, uint_t, uint_t);

/*
 * Tcp based rpc.
 */
extern SVCXPRT *svctcp_create(int, uint_t, uint_t);
extern SVCXPRT *svcfd_create(int, uint_t, uint_t);

/*
 * For connectionless kind of transport. Obsoleted by rpc_reg()
 *
 * registerrpc(prognum, versnum, procnum, progname, inproc, outproc)
 *      rpcprog_t prognum;
 *      rpcvers_t versnum;
 *      rpcproc_t procnum;
 *      char *(*progname)();
 *      xdrproc_t inproc, outproc;
 */
extern int registerrpc(rpcprog_t, rpcvers_t, rpcproc_t, char *(*)(),
				xdrproc_t, xdrproc_t);
#else	/* __STDC__ */
extern bool_t svc_register();
extern void svc_unregister();
extern SVCXPRT *svcraw_create();
extern SVCXPRT *svcudp_create();
extern SVCXPRT *svcudp_bufcreate();
extern SVCXPRT *svctcp_create();
extern SVCXPRT *svcfd_create();
extern int registerrpc();
#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif
#endif	/* _KERNEL */

#endif /* !_RPC_SVC_SOC_H */
