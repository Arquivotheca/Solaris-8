/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)bindresvport.c	1.10	99/04/27 SMI"	/* SVr4.0 1.2	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *     Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988.1989,1999  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

/*
 * XXX This routine should be changed to use
 * ND_CHECK_RESERVED_PORT and ND_SET_RESERVED_PORT
 * which can be invoked via netdir_options.
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/nettype.h>
#include <stropts.h>
#include <string.h>
#include <tiuser.h>
#include <unistd.h>

#define	STARTPORT 600
#define	ENDPORT (IPPORT_RESERVED - 1)
#define	NPORTS	(ENDPORT - STARTPORT + 1)

/*
 * The argument is a client handle for a UDP connection.
 * Unbind its transport endpoint from the existing port
 * and rebind it to a reserved port.
 */
__clnt_bindresvport(cl)
	CLIENT *cl;
{
	int fd;
	int res;
	short port;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	extern int errno;
	/* extern int t_errno; */
	struct t_bind *tbind, *tres;
	int i;
	bool_t	ipv6_fl = FALSE;
	struct netconfig *nconf;

	/* make sure it's a UDP connection */
	nconf = getnetconfigent(cl->cl_netid);
	if (nconf == NULL)
		return (-1);
	if ((nconf->nc_semantics != NC_TPI_CLTS) ||
		(strcmp(nconf->nc_protofmly, NC_INET) &&
		strcmp(nconf->nc_protofmly, NC_INET)) ||
		strcmp(nconf->nc_proto, NC_UDP)) {
		freenetconfigent(nconf);
		return (0);	/* not udp - don't need resv port */
	}
	if (strcmp(nconf->nc_protofmly, NC_INET6) == 0)
		ipv6_fl = TRUE;
	freenetconfigent(nconf);

	/* reserved ports are for superusers only */
	if (geteuid()) {
		errno = EACCES;
		return (-1);
	}

	if (!clnt_control(cl, CLGET_FD, (char *)&fd)) {
		return (-1);
	}

	/* If fd is already bound - unbind it */
	if (t_getstate(fd) != T_UNBND) {
		while ((t_unbind(fd) < 0) && (t_errno == TLOOK)) {
			/*
			 * If there is a message queued to this descriptor,
			 * remove it.
			 */
			struct strbuf ctl[1], data[1];
			char ctlbuf[sizeof (union T_primitives) + 32];
			char databuf[256];
			int flags;

			ctl->maxlen = sizeof (ctlbuf);
			ctl->buf = ctlbuf;
			data->maxlen = sizeof (databuf);
			data->buf = databuf;
			flags = 0;
			if (getmsg(fd, ctl, data, &flags) < 0)
				return (-1);

		}
		if (t_getstate(fd) != T_UNBND)
			return (-1);
	}

	tbind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if (tbind == NULL) {
		if (t_errno == TBADF)
			errno = EBADF;
		return (-1);
	}
	tres = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if (tres == NULL) {
		(void) t_free((char *)tbind, T_BIND);
		return (-1);
	}

	(void) memset((char *)tbind->addr.buf, 0, tbind->addr.len);
	/* warning: this sockaddr_in is truncated to 8 bytes */

	if (ipv6_fl == TRUE) {
		sin6 = (struct sockaddr_in6 *)tbind->addr.buf;
		sin6->sin6_family = AF_INET6;
	} else {
		sin = (struct sockaddr_in *)tbind->addr.buf;
		sin->sin_family = AF_INET;
	}

	tbind->qlen = 0;
	tbind->addr.len = tbind->addr.maxlen;

	/*
	 * Need to find a reserved port in the interval
	 * STARTPORT - ENDPORT.  Choose a random starting
	 * place in the interval based on the process pid
	 * and sequentially search the ports for one
	 * that is available.
	 */
	port = (getpid() % NPORTS) + STARTPORT;

	for (i = 0; i < NPORTS; i++) {
		sin->sin_port = htons(port++);
		if (port > ENDPORT)
			port = STARTPORT;
		/*
		 * Try to bind to the requested address.  If
		 * the call to t_bind succeeds, then we need
		 * to make sure that the address that we bound
		 * to was the address that we requested.  If it
		 * was, then we are done.  If not, we fake an
		 * EADDRINUSE error by setting res, t_errno,
		 * and errno to indicate that a bind failure
		 * occurred.  Otherwise, if the t_bind call
		 * failed, we check to see whether it makes
		 * sense to continue trying to t_bind requests.
		 */
		res = t_bind(fd, tbind, tres);
		if (res == 0) {
			if (memcmp(tbind->addr.buf, tres->addr.buf,
					(int)tres->addr.len) == 0)
				break;
			(void) t_unbind(fd);
			res = -1;
			t_errno = TSYSERR;
			errno = EADDRINUSE;
		} else if (t_errno != TSYSERR || errno != EADDRINUSE)
			break;
	}

	(void) t_free((char *)tbind, T_BIND);
	(void) t_free((char *)tres,  T_BIND);
	return (res);
}
