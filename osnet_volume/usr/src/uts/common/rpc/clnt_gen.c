/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)clnt_gen.c	1.36	99/10/11 SMI" /* SVr4.0 1.10	*/

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	Copyright (c) 1986-1989, 1997,1999 by Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <rpc/types.h>
#include <netinet/in.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <rpc/svc.h>
#include <rpc/xdr.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/stream.h>
#include <sys/tihdr.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>

#define	NC_INET		"inet"

#define	MAX_PRIV	(IPPORT_RESERVED-1)
#define	MIN_PRIV	(IPPORT_RESERVED/2)

ushort_t clnt_udp_last_used = MIN_PRIV;
ushort_t clnt_tcp_last_used = MIN_PRIV;

/*
 * PSARC 1999/553-01 Contract Private Interface
 * clnt_tli_kcreate
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-1999-553-01@sun.com
 */
int
clnt_tli_kcreate(
	struct knetconfig	*config,
	struct netbuf		*svcaddr,	/* Servers address */
	rpcprog_t		prog,		/* Program number */
	rpcvers_t		vers,		/* Version number */
	uint_t			max_msgsize,
	int			retries,
	struct cred		*cred,
	CLIENT			**ncl)
{
	CLIENT			*cl;		/* Client handle */
	TIUSER			*tiptr;
	struct cred		*tmpcred;
	struct cred		*savecred;
	int			error;
	int			family = AF_UNSPEC;

	error = 0;
	cl = NULL;

	RPCLOG(8, "clnt_tli_kcreate: prog %x", prog);
	RPCLOG(8, ", vers %d", vers);
	RPCLOG(8, ", knc_semantics %d", config->knc_semantics);
	RPCLOG(8, ", knc_protofmly %s", config->knc_protofmly);
	RPCLOG(8, ", knc_proto %s\n", config->knc_proto);

	if (config == NULL || config->knc_protofmly == NULL || ncl == NULL) {
		RPCLOG0(1, "clnt_tli_kcreate: bad config or handle\n");
		return (EINVAL);
	}

	switch (config->knc_semantics) {
	case NC_TPI_CLTS:
		/* the transport should be opened as root */
		savecred = CRED();
		tmpcred = crdup(savecred);
		tmpcred->cr_uid = 0;
		error = t_kopen(NULL, config->knc_rdev,
		    FREAD|FWRITE|FNDELAY, &tiptr, tmpcred);
		crfree(tmpcred);
		if (error) {
			RPCLOG(1, "clnt_tli_kcreate: t_kopen: %d\n", error);
			return (error);
		}

		/*
		 *	must bind the endpoint.
		 */
		if (strcmp(config->knc_protofmly, NC_INET) == 0 ||
			strcmp(config->knc_protofmly, NC_INET6) == 0) {
			while ((error =
			    bindresvport(tiptr, NULL, NULL, FALSE)) != 0) {
				RPCLOG(1,
				"clnt_tli_kcreate: bindresvport error %d\n",
				    error);
				if (error == EPROTO) {
					(void) t_kclose(tiptr, 1);
					tmpcred = crdup(savecred);
					tmpcred->cr_uid = 0;
					error = t_kopen(NULL, config->knc_rdev,
					    FREAD|FWRITE|FNDELAY, &tiptr,
					    tmpcred);
					crfree(tmpcred);
					if (error) {
						RPCLOG(1,
					"clnt_tli_kcreate: t_kopen: %d\n",
						    error);
						return (error);
					}
					continue;
				}
				(void) delay(hz);
			}
		} else	{
			if ((error = t_kbind(tiptr, NULL, NULL)) != 0) {
				RPCLOG(1,
				"clnt_tli_kcreate: t_kbind: %d\n", error);
				(void) t_kclose(tiptr, 1);
				return (error);
			}
		}

		error = clnt_clts_kcreate(tiptr, config->knc_rdev,
		    svcaddr, prog, vers, retries, cred, &cl);
		if (error != 0) {
			RPCLOG(1,
			"clnt_tli_kcreate: clnt_clts_kcreate failed error %d\n",
			    error);
			(void) t_kclose(tiptr, 1);
			return (error);
		}
		break;

	case NC_TPI_COTS:
	case NC_TPI_COTS_ORD:
		RPCLOG0(8, "clnt_tli_kcreate: COTS selected\n");
		if (strcmp(config->knc_protofmly, NC_INET) == 0)
			family = AF_INET;
		else if (strcmp(config->knc_protofmly, NC_INET6) == 0)
			family = AF_INET6;
		error = clnt_cots_kcreate(config->knc_rdev, svcaddr, family,
		    prog, vers, max_msgsize, cred, &cl);
		if (error != 0) {
			RPCLOG(1,
			"clnt_tli_kcreate: clnt_cots_kcreate failed error %d\n",
			error);
			return (error);
		}
		break;

	default:
		error = EINVAL;
		RPCLOG(1, "clnt_tli_kcreate: Bad service type %d\n",
		    config->knc_semantics);
		return (error);
	}
	*ncl = cl;
	return (0);
}

/*
 * "Kinit" a client handle by calling the appropriate cots or clts routine.
 *
 * PSARC 1999/553-01 Contract Private Interface
 * clnt_tli_kinit
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-1999-553-01@sun.com
 */
int
clnt_tli_kinit(
	CLIENT		*h,
	struct knetconfig *config,
	struct netbuf	*addr,
	uint_t		max_msgsize,
	int		retries,
	struct cred	*cred)
{
	int error = 0;
	int family = AF_UNSPEC;

	switch (config->knc_semantics) {
	case NC_TPI_CLTS:
		clnt_clts_kinit(h, addr, retries, cred);
		break;
	case NC_TPI_COTS:
	case NC_TPI_COTS_ORD:
		RPCLOG0(2, "clnt_tli_kinit: COTS selected\n");
		if (strcmp(config->knc_protofmly, NC_INET) == 0)
			family = AF_INET;
		else if (strcmp(config->knc_protofmly, NC_INET6) == 0)
			family = AF_INET6;
		clnt_cots_kinit(h, config->knc_rdev, family,
		    addr, max_msgsize, cred);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}


/*
 * try to bind to a reserved port
 */
int
bindresvport(
	TIUSER		*tiptr,
	struct netbuf	*addr,
	struct netbuf	*bound_addr,
	bool_t		tcp)
{
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;
	bool_t 			ipv6_flag = 0;
	int			i;
	struct t_bind		*req;
	struct t_bind		*ret;
	int			error;
	bool_t			loop_twice;
	int			start;
	int			stop;
	ushort_t			*last_used;

	if ((error = t_kalloc(tiptr, T_BIND, T_ADDR, (char **)&req)) != 0) {
		RPCLOG(1, "bindresvport: t_kalloc %d\n", error);
		return (error);
	}

	if ((error = t_kalloc(tiptr, T_BIND, T_ADDR, (char **)&ret)) != 0) {
		RPCLOG(1, "bindresvport: t_kalloc %d\n", error);
		(void) t_kfree(tiptr, (char *)req, T_BIND);
		return (error);
	}

	/* now separate IPv4 and IPv6 by looking at len of tiptr.addr */
	if (tiptr->tp_info.addr == sizeof (struct sockaddr_in6)) {
		/* it's IPv6 */
		ipv6_flag = 1;
		sin6 = (struct sockaddr_in6 *)req->addr.buf;
		sin6->sin6_family = AF_INET6;
		bzero((char *)&sin6->sin6_addr, sizeof (struct in6_addr));
		req->addr.len = sizeof (struct sockaddr_in6);
	} else {
		/* LINTED pointer alignment */
		sin = (struct sockaddr_in *)req->addr.buf;
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		req->addr.len = sizeof (struct sockaddr_in);
	}

	/*
	 * Caller wants to bind to a specific port, so don't bother with the
	 * loop that binds to the next free one.
	 */
	if (addr) {

		if (ipv6_flag)
			sin6->sin6_port =
				((struct sockaddr_in6 *)addr->buf)->sin6_port;
		else
			sin->sin_port =
				((struct sockaddr_in *)addr->buf)->sin_port;
		RPCLOG(8, "bindresvport: calling t_kbind tiptr = %p\n",
		    (void *)tiptr);
		if ((error = t_kbind(tiptr, req, ret)) != 0) {
			RPCLOG(1, "bindresvport: t_kbind: %d\n", error);
			/*
			 * The unbind is called in case the bind failed
			 * with an EINTR potentially leaving the
			 * transport in bound state.
			 */
			if (error == EINTR)
				(void) t_kunbind(tiptr);
		} else
		if (bcmp(req->addr.buf, ret->addr.buf, ret->addr.len) != 0) {
			RPCLOG0(1, "bindresvport: bcmp error\n");
			(void) t_kunbind(tiptr);
			error = EADDRINUSE;
		}
	} else {
		if (tcp)
			last_used = &clnt_tcp_last_used;
		else
			last_used = &clnt_udp_last_used;
		error = EADDRINUSE;
		stop = MIN_PRIV;

		start = (*last_used == MIN_PRIV ? MAX_PRIV : *last_used - 1);
		loop_twice = (start < MAX_PRIV ? TRUE : FALSE);

bindresvport_again:;

		for (i = start;
		    (error == EADDRINUSE || error == EADDRNOTAVAIL) &&
		    i >= stop; i--) {
			if (ipv6_flag)
				sin6->sin6_port = htons(i);
			else
				sin->sin_port = htons(i);
			RPCLOG(8, "bindresvport: calling t_kbind tiptr = 0%p\n",
			    (void *)tiptr);
			if ((error = t_kbind(tiptr, req, ret)) != 0) {
				RPCLOG(1, "bindresvport: t_kbind: %d\n", error);
				/*
				 * The unbind is called in case the bind failed
				 * with an EINTR potentially leaving the
				 * transport in bound state.
				 */
				if (error == EINTR)
					(void) t_kunbind(tiptr);
			} else if (bcmp(req->addr.buf, ret->addr.buf,
			    ret->addr.len) != 0) {
				RPCLOG0(1, "bindresvport: bcmp error\n");
				(void) t_kunbind(tiptr);
				error = EADDRINUSE;
			} else
				error = 0;
		}
		if (!error) {
			if (ipv6_flag) {
				RPCLOG(8, "bindresvport: port assigned %d\n",
					sin6->sin6_port);
				*last_used = ntohs(sin6->sin6_port);
			} else {
				RPCLOG(8, "bindresvport: port assigned %d\n",
					sin->sin_port);
				*last_used = ntohs(sin->sin_port);
			}
		} else if (loop_twice) {
			loop_twice = FALSE;
			start = MAX_PRIV;
			stop = *last_used + 1;
			goto bindresvport_again;
		}
	}

	if (!error && bound_addr) {
		bcopy(ret->addr.buf, bound_addr->buf, ret->addr.len);
		bound_addr->len = ret->addr.len;
	}
	(void) t_kfree(tiptr, (char *)req, T_BIND);
	(void) t_kfree(tiptr, (char *)ret, T_BIND);
	return (error);
}
