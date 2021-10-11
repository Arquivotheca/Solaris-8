/*
 *	svc_local.c
 *
 *	Copyright (c) 1988-1996,1999 by Sun Microsystems Inc
 *	All rights reserved.
 */

#pragma ident   "@(#)svc_local.c 1.7     99/06/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <syslog.h>
#include <rpc/nettype.h>
#include <netconfig.h>
#include <netdir.h>
#include <tiuser.h>
#include <fcntl.h>
#include <string.h>
#include "mountd.h"


/*
 * The highest level interface for server creation.
 * It tries for all the nettokens in that particular class of token
 * and returns the number of handles it can create and/or find.
 *
 * It creates a link list of all the handles it could create.
 * If svc_create() is called multiple times, it uses the handle
 * created earlier instead of creating a new handle every time.
 *
 * Copied from svc_generic.c and ../keyserv/key_generic.c
 */
int
svc_create_local_service(
	void (*dispatch) (),	/* Dispatch function */
	rpcprog_t prognum,	/* Program number */
	rpcvers_t versnum,	/* Version number */
	char *nettype,		/* Networktype token */
	char *servname)		/* name of the service */
{
	struct xlist {
		SVCXPRT *xprt;		/* Server handle */
		struct xlist *next;	/* Next item */
	} *l;
	static struct xlist *xprtlist;
	int num = 0;
	SVCXPRT *xprt;
	struct netconfig *nconf;
	struct t_bind *bind_addr;
	void *net;
	int fd;
	struct nd_hostserv ns;
	struct nd_addrlist *nas;

	if ((net = __rpc_setconf(nettype)) == 0) {
		syslog(LOG_ERR,
		"svc_create: could not read netconfig database");
		return (0);
	}
	while (nconf = __rpc_getconf(net)) {
		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK))
			continue;
		for (l = xprtlist; l; l = l->next) {
			if (strcmp(l->xprt->xp_netid, nconf->nc_netid) == 0) {
				/* Found an  old  one,  use  it */
				(void) rpcb_unset(prognum, versnum, nconf);
				if (svc_reg(l->xprt, prognum, versnum,
					dispatch, nconf) == FALSE)
					syslog(LOG_ERR,
	    "svc_create: could not register prog %d vers %d on %s",
					    prognum, versnum, nconf->nc_netid);
				else
					num++;
				break;
			}
		}
		if (l)
			continue;
		/* It was not found. Now create a new one */
		if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) < 0) {
			syslog(LOG_ERR,
		"svc_create: %s: cannot open connection: %s",
			    nconf->nc_netid, t_errlist[t_errno]);
			continue;
		}

		/*
		 * Negotiate for returning the uid of the caller.
		 * This should be done before enabling the endpoint for
		 * service via t_bind() (called in svc_tli_create())
		 * so that requests to mountd contain the uid.
		 */
		if (__rpc_negotiate_uid(fd) != 0) {
			syslog(LOG_ERR,
			"Couldn't negotiate for uid with loopback transport %s",
				nconf->nc_netid);
			(void) t_close(fd);
			continue;
		}

		/* LINTED pointer alignment */
		bind_addr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
		if ((bind_addr == NULL)) {
			(void) t_close(fd);
			syslog(LOG_ERR, "svc_create: t_alloc failed\n");
			continue;
		}
		ns.h_host = HOST_SELF;
		ns.h_serv = servname;
		if (!netdir_getbyname(nconf, &ns, &nas)) {
			/* Copy the address */
			bind_addr->addr.len = nas->n_addrs->len;
			(void) memcpy(bind_addr->addr.buf, nas->n_addrs->buf,
				(int)nas->n_addrs->len);
			bind_addr->qlen = 8;
			netdir_free(nas, ND_ADDRLIST);
		} else {
			syslog(LOG_ERR,
	"svc_create: no well known address for %s on transport %s\n",
			    servname, nconf->nc_netid);
			(void) t_free((char *)bind_addr, T_BIND);
			bind_addr = NULL;
		}

		xprt = svc_tli_create(fd, nconf, bind_addr, 0, 0);
		if (bind_addr)
			(void) t_free((char *)bind_addr, T_BIND);
		if (xprt == NULL) {
			(void) t_close(fd);
			(void) syslog(LOG_ERR,
			    "svc_create: svc_tli_create failed\n");
		} else {
			(void) rpcb_unset(prognum, versnum, nconf);
			if (svc_reg(xprt, prognum, versnum,
				dispatch, nconf) == FALSE) {
				(void) syslog(LOG_ERR,
	    "svc_create: could not register prog %d vers %d on %s",
				    prognum, versnum, nconf->nc_netid);
				SVC_DESTROY(xprt);
				continue;
			}
			l = malloc(sizeof (struct xlist));
			if (l == (struct xlist *)NULL) {
				syslog(LOG_ERR,
					    "svc_create: no memory");
				SVC_DESTROY(xprt);
				return (num);
			}
			l->xprt = xprt;
			l->next = xprtlist;
			xprtlist = l;
			num++;
		}
	}
	__rpc_endconf(net);
	return (num);
}
