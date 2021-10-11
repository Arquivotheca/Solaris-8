/*
 * Copyright (c) 1990,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RAC_PRIVATE_H
#define	_RAC_PRIVATE_H

#pragma ident	"@(#)rac_private.h	1.3	98/06/17 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/netconfig.h>
#include <rpc/rpc.h>

/*
 *	These defines, and the following structure represent RAC implementation
 *	details and may not be relied upon in the future.
 */
#define	CLRAC_DROP	999	/* drop previous call and destroy RAC handle */
#define	CLRAC_POLL	998	/* check status of asynchronous call */
#define	CLRAC_RECV	997	/* receive results of a previous rac_send() */
#define	CLRAC_SEND	996	/* initiate asynchronous call and return */

struct rac_send_req {
	rpcproc_t 	proc;
	xdrproc_t	xargs;
	void		*argsp;
	xdrproc_t	xresults;
	void		*resultsp;
	struct timeval	timeout;
	void		*handle;	/* result of rac_send */
};


#ifndef MIN
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

extern void		free_pollinfo(void *);
extern void		free_pkt(void *);
extern uint32_t		ri_to_xid(void *);
extern int		pkt_vc_read(void **, char *, int);
extern void		*pkt_vc_poll(int, void **);
extern void		xdrrec_resetinput(XDR *);
extern struct netbuf	*__rpcb_findaddr(rpcprog_t, rpcvers_t,
    const struct netconfig *, const char *, CLIENT **);

#ifdef __cplusplus
}
#endif

#endif /* _RAC_PRIVATE_H */
