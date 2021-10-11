/*
 * Copyright (c) 1986 - 1991,1999 by Sun Microsystems, Inc.
 */

#ident	"@(#)check_bound.c	1.18	99/04/27 SMI"

#ifndef lint
static	char sccsid[] = "@(#)check_bound.c 1.11 89/04/21 Copyr 1989 Sun Micro";
#endif

/*
 * check_bound.c
 * Checks to see whether the program is still bound to the
 * claimed address and returns the univeral merged address
 *
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <netconfig.h>
#include <netdir.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include "rpcbind.h"
#include <string.h>
/* the following just to get my address */
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct fdlist {
	int fd;
	struct netconfig *nconf;
	struct fdlist *next;
	int check_binding;
};

static struct fdlist *fdhead;	/* Link list of the check fd's */
static struct fdlist *fdtail;
static char *nullstring = "";

/*
 * Returns 1 if the given address is bound for the given addr & transport
 * For all error cases, we assume that the address is bound
 * Returns 0 for success.
 */
static bool_t
check_bound(fdl, uaddr)
	struct fdlist *fdl;	/* My FD list */
	char *uaddr;		/* the universal address */
{
	int fd;
	struct netbuf *na;
	struct t_bind taddr, *baddr;
	int ans;

	if (fdl->check_binding == FALSE)
		return (TRUE);

	na = uaddr2taddr(fdl->nconf, uaddr);
	if (!na)
		return (TRUE); /* punt, should never happen */

	fd = fdl->fd;
	taddr.addr = *na;
	taddr.qlen = 1;
	baddr = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if (baddr == NULL) {
		netdir_free((char *)na, ND_ADDR);
		return (TRUE);
	}
	if (t_bind(fd, &taddr, baddr) != 0) {
		netdir_free((char *)na, ND_ADDR);
		(void) t_free((char *)baddr, T_BIND);
		return (TRUE);
	}
	ans = memcmp(taddr.addr.buf, baddr->addr.buf, baddr->addr.len);
	netdir_free((char *)na, ND_ADDR);
	(void) t_free((char *)baddr, T_BIND);
	if (t_unbind(fd) != 0) {
		/* Bad fd. Purge this fd */
		(void) t_close(fd);
		fdl->fd = t_open(fdl->nconf->nc_device, O_RDWR, NULL);
		if (fdl->fd == -1)
			fdl->check_binding = FALSE;
	}
	return (ans == 0 ? FALSE : TRUE);
}

/*
 * Keep open one more file descriptor for this transport, which
 * will be used to determine whether the given service is up
 * or not by trying to bind to the registered address.
 * We are ignoring errors here. It trashes taddr and baddr;
 * but that perhaps should not matter.
 *
 * We check for the following conditions:
 *	1. Is it possible for t_bind to fail in the case where
 *		we bind to an already bound address and have any
 *		other error number besides TNOADDR.
 *	2. If a address is specified in bind addr, can I bind to
 *		the same address.
 *	3. If NULL is specified in bind addr, can I bind to the
 *		address to which the fd finally got bound.
 */
int
add_bndlist(nconf, taddr, baddr)
	struct netconfig *nconf;
	struct t_bind *taddr, *baddr;
{
	int fd;
	struct fdlist *fdl;
	struct netconfig *newnconf;
	struct t_info tinfo;
	struct t_bind tmpaddr;

	newnconf = getnetconfigent(nconf->nc_netid);
	if (newnconf == NULL)
		return (-1);
	fdl = (struct fdlist *)malloc((uint_t)sizeof (struct fdlist));
	if (fdl == NULL) {
		freenetconfigent(newnconf);
		syslog(LOG_ERR, "no memory!");
		return (-1);
	}
	fdl->nconf = newnconf;
	fdl->next = NULL;
	if (fdhead == NULL) {
		fdhead = fdl;
		fdtail = fdl;
	} else {
		fdtail->next = fdl;
		fdtail = fdl;
	}
	fdl->check_binding = FALSE;
	if ((fdl->fd = t_open(nconf->nc_device, O_RDWR, &tinfo)) < 0) {
		/*
		 * Note that we haven't dequeued this entry nor have we freed
		 * the netconfig structure.
		 */
		if (debugging) {
			fprintf(stderr,
			    "%s: add_bndlist cannot open connection: %s",
			    nconf->nc_netid, t_errlist[t_errno]);
		}
		return (-1);
	}

	/* Set the qlen only for cots transports */
	switch (tinfo.servtype) {
	case T_COTS:
	case T_COTS_ORD:
		taddr->qlen = 1;
		break;
	case T_CLTS:
		taddr->qlen = 0;
		break;
	default:
		goto error;
	}

	if (t_bind(fdl->fd, taddr, baddr) != 0) {
		if (t_errno == TNOADDR) {
			fdl->check_binding = TRUE;
			return (0);	/* All is fine */
		}
		/* Perhaps condition #1 */
		if (debugging) {
			fprintf(stderr, "%s: add_bndlist cannot bind (1): %s",
				nconf->nc_netid, t_errlist[t_errno]);
		}
		goto not_bound;
	}

	/* Condition #2 */
	if (!memcmp(taddr->addr.buf, baddr->addr.buf,
		(int)baddr->addr.len)) {
#ifdef BIND_DEBUG
		fprintf(stderr, "Condition #2\n");
#endif
		goto not_bound;
	}

	/* Condition #3 */
	t_unbind(fdl->fd);
	/* Set the qlen only for cots transports */
	switch (tinfo.servtype) {
	case T_COTS:
	case T_COTS_ORD:
		tmpaddr.qlen = 1;
		break;
	case T_CLTS:
		tmpaddr.qlen = 0;
		break;
	default:
		goto error;
	}
	tmpaddr.addr.len = tmpaddr.addr.maxlen = 0;
	tmpaddr.addr.buf = NULL;
	if (t_bind(fdl->fd, &tmpaddr, taddr) != 0) {
		if (debugging) {
			fprintf(stderr, "%s: add_bndlist cannot bind (2): %s",
				nconf->nc_netid, t_errlist[t_errno]);
		}
		goto error;
	}
	/* Now fdl->fd is bound to a transport chosen address */
	if ((fd = t_open(nconf->nc_device, O_RDWR, &tinfo)) < 0) {
		if (debugging) {
			fprintf(stderr,
				"%s: add_bndlist cannot open connection: %s",
				nconf->nc_netid, t_errlist[t_errno]);
		}
		goto error;
	}
	if (t_bind(fd, taddr, baddr) != 0) {
		if (t_errno == TNOADDR) {
			/*
			 * This transport is schizo.  Previously it handled a
			 * request to bind to an already bound transport by
			 * returning a different bind address, and now it's
			 * returning a TNOADDR for essentially the same
			 * request.  The spec may allow this behavior, so
			 * we'll just assume we can't do bind checking with
			 * this transport.
			 */
			goto not_bound;
		}
		if (debugging) {
			fprintf(stderr, "%s: add_bndlist cannot bind (3): %s",
				nconf->nc_netid, t_errlist[t_errno]);
		}
		t_close(fd);
		goto error;
	}
	t_close(fd);
	if (!memcmp(taddr->addr.buf, baddr->addr.buf,
		(int)baddr->addr.len)) {
		switch (tinfo.servtype) {
		case T_COTS:
		case T_COTS_ORD:
			if (baddr->qlen == 1) {
#ifdef BIND_DEBUG
				fprintf(stderr, "Condition #3\n");
#endif
				goto not_bound;
			}
			break;
		case T_CLTS:
#ifdef BIND_DEBUG
			fprintf(stderr, "Condition #3\n");
#endif
			goto not_bound;
		default:
			goto error;
		}
	}

	t_unbind(fdl->fd);
	fdl->check_binding = TRUE;
	return (0);

not_bound:
	t_close(fdl->fd);
	fdl->fd = -1;
	return (1);

error:
	t_close(fdl->fd);
	fdl->fd = -1;
	return (-1);
}

bool_t
is_bound(netid, uaddr)
	char *netid;
	char *uaddr;
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (TRUE);
	return (check_bound(fdl, uaddr));
}

/*
 * Returns NULL if there was some system error.
 * Returns "" if the address was not bound, i.e the server crashed.
 * Returns the merged address otherwise.
 */
char *
mergeaddr(xprt, netid, uaddr, saddr)
	SVCXPRT *xprt;
	char *netid;
	char *uaddr;
	char *saddr;
{
	struct fdlist *fdl;
	struct nd_mergearg ma;
	int stat;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	if (check_bound(fdl, uaddr) == FALSE)
		/* that server died */
		return (nullstring);
	/*
	 * If saddr is not NULL, the remote client may have included the
	 * address by which it contacted us.  Use that for the "client" uaddr,
	 * otherwise use the info from the SVCXPRT.
	 */
	if (saddr != NULL) {
		ma.c_uaddr = saddr;
	} else {

		/* retrieve the client's address */
		ma.c_uaddr = taddr2uaddr(fdl->nconf, svc_getrpccaller(xprt));
		if (ma.c_uaddr == NULL) {
			syslog(LOG_ERR, "taddr2uaddr failed for %s: %s",
				fdl->nconf->nc_netid, netdir_sperror());
			return (NULL);
		}

	}
#ifdef ND_DEBUG
	if (saddr == NULL) {
		fprintf(stderr, "mergeaddr: client uaddr = %s\n", ma.c_uaddr);
	} else {
		fprintf(stderr, "mergeaddr: contact uaddr = %s\n", ma.c_uaddr);
	}
#endif
	ma.s_uaddr = uaddr;
	stat = netdir_options(fdl->nconf, ND_MERGEADDR, 0, (char *)&ma);
	if (saddr == NULL) {
		free(ma.c_uaddr);
	}
	if (stat) {
		syslog(LOG_ERR, "netdir_merge failed for %s: %s",
			fdl->nconf->nc_netid, netdir_sperror());
		return (NULL);
	}
#ifdef ND_DEBUG
	fprintf(stderr, "mergeaddr: uaddr = %s, merged uaddr = %s\n",
				uaddr, ma.m_uaddr);
#endif
	return (ma.m_uaddr);
}

/*
 * Returns a netconf structure from its internal list.  This
 * structure should not be freed.
 */
struct netconfig *
rpcbind_get_conf(netid)
	char *netid;
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	return (fdl->nconf);
}

#ifdef BIND_DEBUG
syslog(a, msg, b, c, d)
	int a;
	char *msg;
	caddr_t b, c, d;
{
	char buf[1024];

	sprintf(buf, msg, b, c, d);
	fprintf(stderr, "Syslog: %s\n", buf);
}
#endif
