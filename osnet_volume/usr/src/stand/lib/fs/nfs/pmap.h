/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PMAP_H
#define	_PMAP_H

#pragma ident	"@(#)pmap.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	PMAP_STATIC	(2)	/* last statically allocated list entry */
#define	UA_SIZE		(128)	/* max space needed for an universal addr */

extern enum clnt_stat bpmap_rmtcall(rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t,
    caddr_t, xdrproc_t, caddr_t, int, int, struct sockaddr_in *,
    struct sockaddr_in *, u_int);
extern rpcport_t bpmap_getport(rpcprog_t, rpcvers_t, enum clnt_stat *,
	struct sockaddr_in *, struct sockaddr_in *);
extern void bpmap_memfree(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PMAP_H */
