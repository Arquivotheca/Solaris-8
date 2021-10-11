/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)rpcbind.h 1.4 90/04/12 SMI" */

/*
 * rpcbind.h
 * The common header declarations
 */

#ifndef rpcbind_h
#define	rpcbind_h

#ifdef PORTMAP
#include <rpc/pmap_prot.h>
#endif
#include <rpc/rpcb_prot.h>

extern int debugging;
extern int doabort;
extern rpcblist_ptr list_rbl;	/* A list of version 3 & 4 rpcbind services */
extern char *loopback_dg;	/* CLTS loopback transport, for set/unset */
extern char *loopback_vc;	/* COTS loopback transport, for set/unset */
extern char *loopback_vc_ord;	/* COTS_ORD loopback transport, for set/unset */

#ifdef PORTMAP
extern pmaplist *list_pml;	/* A list of version 2 rpcbind services */
extern char *udptrans;		/* Name of UDP transport */
extern char *tcptrans;		/* Name of TCP transport */
extern char *udp_uaddr;		/* Universal UDP address */
extern char *tcp_uaddr;		/* Universal TCP address */
#endif

extern char *mergeaddr();
extern int add_bndlist();
extern int create_rmtcall_fd();
extern bool_t is_bound();
extern void my_svc_run();

/* Statistics gathering functions */
extern void rpcbs_procinfo();
extern void rpcbs_set();
extern void rpcbs_unset();
extern void rpcbs_getaddr();
extern void rpcbs_rmtcall();
extern rpcb_stat_byvers *rpcbproc_getstat();

extern struct netconfig *rpcbind_get_conf();
extern void rpcbind_abort();

/* Common functions shared between versions */
extern void rpcbproc_callit_com();
extern bool_t *rpcbproc_set_com();
extern bool_t *rpcbproc_unset_com();
extern u_long *rpcbproc_gettime_com();
extern struct netbuf *rpcbproc_uaddr2taddr_com();
extern char **rpcbproc_taddr2uaddr_com();
extern char **rpcbproc_getaddr_com();

/* For different getaddr semantics */
#define	RPCB_ALLVERS 0
#define	RPCB_ONEVERS 1

#endif /* rpcbind_h */
