/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/*
 * rpcent.h,
 * For converting rpc program numbers to names etc.
 *
 */

#ifndef _RPC_RPCENT_H
#define	_RPC_RPCENT_H

#pragma ident	"@(#)rpcent.h	1.15	94/10/04 SMI"
/*	@(#)rpcent.h 1.1 88/12/06 SMI	*/

#ifdef	__cplusplus
extern "C" {
#endif

struct rpcent {
	char	*r_name;	/* name of server for this rpc program */
	char	**r_aliases;	/* alias list */
	int	r_number;	/* rpc program number */
};

#ifdef __STDC__
extern struct rpcent *getrpcbyname_r
		(const char *,	  struct rpcent *, char *, int);
extern struct rpcent *getrpcbynumber_r
		(int,		  struct rpcent *, char *, int);
extern struct rpcent *getrpcent_r(struct rpcent *, char *, int);

/* Old interfaces that return a pointer to a static area;  MT-unsafe */
extern struct rpcent *getrpcbyname(const char *);
extern struct rpcent *getrpcbynumber(const int);
extern struct rpcent *getrpcent(void);
extern void setrpcent(const int);
extern void endrpcent(void);
#else
extern struct rpcent *getrpcbyname_r();
extern struct rpcent *getrpcbynumber_r();
extern struct rpcent *getrpcent_r();

/* Old interfaces that return a pointer to a static area;  MT-unsafe */
extern struct rpcent *getrpcbyname();
extern struct rpcent *getrpcbynumber();
extern struct rpcent *getrpcent();
extern void setrpcent();
extern void endrpcent();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _RPC_RPCENT_H */
