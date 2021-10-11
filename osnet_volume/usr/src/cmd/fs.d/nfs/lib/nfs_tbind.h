/*
 * Copyright (c) 1996, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * nfs_tbind.h, common code for nfsd and lockd
 */

#ifndef	_NFS_TBIND_H
#define	_NFS_TBIND_H

#pragma ident	"@(#)nfs_tbind.h	1.5	99/08/13 SMI"

#include <netconfig.h>
#include <netdir.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Globals which should be initialised by daemon main().
 */
extern  size_t  end_listen_fds;
extern  size_t  num_fds;
extern	int	listen_backlog;
extern	int	(*Mysvc)(int, struct netbuf, struct netconfig *);
extern  int	max_conns_allowed;

/*
 * RPC protocol block.  Useful for passing registration information.
 */
struct protob {
	char *serv;		/* ASCII service name, e.g. "NFS" */
	int versmin;		/* minimum version no. to be registered */
	int versmax;		/* maximum version no. to be registered */
	int program;		/* program no. to be registered */
	struct protob *next;	/* next entry on list */
};

/*
 * Declarations for protocol types and comparison.
 */
#define	NETSELDECL(x)	char *x
#define	NETSELPDECL(x)	char **x
#define	NETSELEQ(x, y)	(strcmp((x), (y)) == 0)

/*
 * nfs library routines
 */
extern int	nfslib_transport_open(struct netconfig *);
extern int	nfslib_bindit(struct netconfig *, struct netbuf **,
			struct nd_hostserv *, int);
extern void	nfslib_log_tli_error(char *, int, struct netconfig *);
extern int	do_all(struct protob *,
			int (*)(int, struct netbuf, struct netconfig *));
extern void	do_one(char *, char *, struct protob *,
			int (*)(int, struct netbuf, struct netconfig *));
extern void	poll_for_action(void);

#ifdef __cplusplus
}
#endif

#endif	/* _NFS_TBIND_H */
