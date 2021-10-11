/*
 * 	selfcheck.c
 *      Copyright (c) 1999 Sun Microsystems Inc.
 *      All Rights Reserved.
 */

#pragma ident	"@(#)selfcheck.c	1.5	99/04/27 SMI"

#include <errno.h>
#include <syslog.h>

#include <strings.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>

int
self_check(hostname)
	char *hostname;
{
	int s, res = 0;
	struct sioc_addrreq areq;

	struct hostent *hostinfo;
	int family;
	int flags;
	int error_num;
	char **hostptr;

	struct sockaddr_in6 ipv6addr;

	family = AF_INET6;
	flags = AI_DEFAULT;

	if ((s = socket(family, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "self_check: socket: %m");
		return (0);
	}

	if ((hostinfo = getipnodebyname(hostname, family, flags,
	    &error_num)) == NULL) {

		if (error_num == TRY_AGAIN)
			syslog(LOG_DEBUG,
			    "self_check: unknown host: %s (try again later)\n",
			    hostname);
		else
			syslog(LOG_DEBUG,
			    "self_check: unknown host: %s\n", hostname);

		(void) close(s);
		return (0);
	}

	for (hostptr = hostinfo->h_addr_list; *hostptr; hostptr++) {
		bzero(&ipv6addr, sizeof (ipv6addr));
		ipv6addr.sin6_family = AF_INET6;
		ipv6addr.sin6_addr = *((struct in6_addr *)(*hostptr));
		memcpy(&areq.sa_addr, (void *)&ipv6addr, sizeof (ipv6addr));
		areq.sa_res = -1;
		(void) ioctl(s, SIOCTMYADDR, (caddr_t)&areq);
		if (areq.sa_res == 1) {
			res = 1;
			break;
		}
	}

	freehostent(hostinfo);

	(void) close(s);
	return (res);
}

#define	MAXIFS	32

/*
 * create an ifconf structure that represents all the interfaces
 * configured for this host.  Two buffers are allcated here:
 *	lifc - the ifconf structure returned
 *	lifc->lifc_buf - the list of ifreq structures
 * Both of the buffers must be freed by the calling routine.
 * A NULL pointer is returned upon failure.  In this case any
 * data that was allocated before the failure has already been
 * freed.
 */
struct lifconf *
getmyaddrs()
{
	int sock;
	struct lifnum lifn;
	int numifs;
	char *buf;
	struct lifconf *lifc;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "statd:getmyaddrs socket: %m");
		return ((struct lifconf *)NULL);
	}

	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;

	if (ioctl(sock, SIOCGLIFNUM, (char *)&lifn) < 0) {
		syslog(LOG_ERR,
		"statd:getmyaddrs, get number of interfaces, error: %m");
		numifs = MAXIFS;
	}

	numifs = lifn.lifn_count;

	lifc = (struct lifconf *)malloc(sizeof (struct lifconf));
	lifc = malloc(sizeof (struct lifconf));
	if (lifc == NULL) {
		syslog(LOG_ERR,
			"statd:getmyaddrs, malloc for lifconf failed: %m");
		(void) close(sock);
		return ((struct lifconf *)NULL);
	}
	buf = (char *)malloc(numifs * sizeof (struct lifreq));
	if (buf == NULL) {
		syslog(LOG_ERR,
			"statd:getmyaddrs, malloc for lifreq failed: %m");
		(void) close(sock);
		free(lifc);
		return ((struct lifconf *)NULL);
	}

	lifc->lifc_family = AF_UNSPEC;
	lifc->lifc_flags = 0;
	lifc->lifc_buf = buf;
	lifc->lifc_len = numifs * sizeof (struct lifreq);

	if (ioctl(sock, SIOCGLIFCONF, (char *)lifc) < 0) {
		syslog(LOG_ERR, "statd:getmyaddrs, SIOCGLIFCONF, error: %m");
		(void) close(sock);
		free(buf);
		free(lifc);
		return ((struct lifconf *)NULL);
	}

	(void) close(sock);

	return (lifc);
}

int
Is_ipv6present(void)
{
	int sock;
	struct lifnum lifn;

	sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sock < 0)
		return (0);

	lifn.lifn_family = AF_INET6;
	lifn.lifn_flags = 0;
	if (ioctl(sock, SIOCGLIFNUM, (char *)&lifn) < 0) {
		close(sock);
		return (0);
	}
	close(sock);
	if (lifn.lifn_count == 0)
		return (0);
	return (1);
}
