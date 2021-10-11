/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 *
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 */

#ident	"@(#)tcpip.c	1.22	99/03/21 SMI"

#if !defined(lint) && defined(SCCSIDS)
static  char sccsid[] = "@(#)tcpip.c 1.5 89/10/09  Copyr 1989 Sun Micro";
#endif

/*
 * TCP/IP name to address translation routines. These routines are written
 * to the getXXXbyYYY() interface that the BSD routines use. This allows
 * us to simply rewrite those routines to get various flavors of translation
 * routines. Thus while they look like they have socket dependencies (the
 * sockaddr_in structures) in fact this is simply the internal netbuf
 * representation that the TCP and UDP transport providers use.
 */

/*
 * #ifdef WEIRDNESS:
 * -DPIC flag is used by ./Makefile and ../switch/Makefile
 * to build tcpip.so and switch.so respectively. The latter
 * Makefile also uses -DSWITCH flag, along with
 * -DPIC to build switch.so, and without -DPIC to build libswitch.a.
 * The .a's have library specific entry points (e.g. switch_netdir_getbyname)
 * instead of the generic _netdir_** entry points in the .so'es in order
 * to distinguish them in the same static binary so we can fake the
 * dlopen/dlsym functionality without using the dynamic linker. This is
 * useful to build a few disaster recovery static commands like mv, rcp
 * and tar (they want to use the /etc/netconfig and /etc/nsswitch.conf
 * files for dynamic name service selection without having to use libdl.so
 * or ld.so.1). Take a look at /usr/src/lib/libdl_stubs for further
 * explanation.
 *
 * WARNING: If you change the way any of these flags are defined or used,
 * thou shalt find out *all* the commands in the source tree that are completely
 * static and make sure they build from scratch (without any residue libraries
 * in /proto/usr/lib and after a make install from here) lest thou shalt incur
 * the wrath of the build czars.
 */

#include <mtlib.h>
#include <thread.h>
#include <synch.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/byteorder.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <tiuser.h>
#include <netconfig.h>
#include <netdir.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <nss_dbdefs.h>


#if defined(SWITCH)
extern struct hostent *_switch_gethostbyname_r(const char *,
					       struct hostent *, char *, int,
					       int *h_errno);
extern struct hostent *_switch_gethostbyaddr_r(const void *, int, int,
					       struct hostent *, char *, int,
					       int *h_errno);
extern struct servent *_switch_getservbyname_r(const char *, const char *,
					       struct servent *, char *, int);
extern struct servent *_switch_getservbyport_r(int,          const char *,
					       struct servent *, char *, int);
extern struct hostent *_switch_files_gethostbyname(const char *);

#define GETHOSTBUF(host_buf)					\
	NSS_XbyY_ALLOC(&host_buf, sizeof (struct hostent), NSS_BUFLEN_HOSTS)
#define GETSERVBUF(serv_buf)					\
	NSS_XbyY_ALLOC(&serv_buf, sizeof (struct servent), NSS_BUFLEN_SERVICES)

#else

extern struct hostent *_files_gethostbyname(const char *);
extern struct hostent *_files_gethostbyaddr(const void *, int, int);
extern struct servent *_files_getservbyname(const char *, const char *);
extern struct servent *_files_getservbyport(int,          const char *);

#endif

/* Serializes access to all routines in file_db.c */
static mutex_t	one_lane = DEFAULTMUTEX;

static char 	*localaddr[] = {"\000\000\000\000", NULL};

static struct hostent localent = {
		"INADDR_ANY",
		NULL,
		AF_INET,
		4,
		localaddr
};

static char 	*connectaddr[] = {"\177\000\000\001", NULL};

/*
 * This is the same as 127.0.0.1. Works both as the local connect
 * address and a translation for the lookup request for "localhost".
 */

static struct hostent connectent = {
		"localhost",
		NULL,
		AF_INET,
		4,
		connectaddr
};

#define	MAXBCAST	10
#define	MAXIFS 32
#define	UDP "/dev/udp"

struct ifinfo {
	struct in_addr addr, netmask;
};

static struct ifinfo *
get_local_info()
{
	int numifs;
	char	*buf;
	struct	ifconf ifc;
	struct	ifreq ifreq, *ifr;
	int fd;
	struct ifinfo *localinfo;
	int i, n;
	struct in_addr netmask;
	struct sockaddr_in *sin;

	if ((fd = open(UDP, O_RDONLY)) < 0) {
		(void) syslog(LOG_ERR,
	"n2a get_local_info: open to get interface configuration: %m");
		_nderror = ND_OPEN;
		return ((struct ifinfo *)NULL);
	}

#ifdef SIOCGIFNUM
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		perror("ioctl (get number of interfaces)");
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	buf = (char *)malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		(void) syslog(LOG_ERR, "n2a get_local_info: malloc failed: %m");
		close(fd);
		_nderror = ND_NOMEM;
		return ((struct ifinfo *)NULL);
	}
	ifc.ifc_len = numifs * sizeof (struct ifreq);
	ifc.ifc_buf = buf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
		(void) syslog(LOG_ERR,
		"n2a get_local_info: ioctl (get interface configuration): %m");
		close(fd);
		free(buf);
		_nderror = ND_SYSTEM;
		return ((struct ifinfo *)NULL);
	}
	ifr = (struct ifreq *)buf;
	numifs = ifc.ifc_len/sizeof (struct ifreq);
	localinfo = (struct ifinfo *)malloc((numifs + 1) *
					    sizeof (struct ifinfo));
	if (localinfo == NULL) {
		(void) syslog(LOG_ERR, "n2a get_local_info: malloc failed: %m");
		close(fd);
		free(buf);
		_nderror = ND_SYSTEM;
		return ((struct ifinfo *)NULL);
	}

	for (i = 0, n = numifs; n > 0; n--, ifr++) {
		u_int ifrflags;

		ifreq = *ifr;
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
			"n2a get_local_info: ioctl (get interface flags): %m");
			continue;
		}
		ifrflags = ifreq.ifr_flags;
		if (((ifrflags & IFF_UP) == 0) ||
			(ifr->ifr_addr.sa_family != AF_INET))
			continue;

		if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
	"n2a get_local_info: ioctl (get interface netmask): %m");
			continue;
		}
		netmask = ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr;

		if (ioctl(fd, SIOCGIFADDR, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
	"n2a get_local_info: ioctl (get interface address): %m");
			continue;
		}
		sin = (struct sockaddr_in *) &ifreq.ifr_addr;

		localinfo[i].addr = sin->sin_addr;
		localinfo[i].netmask = netmask;
		i++;
	}
	localinfo[i].addr.s_addr = 0;

	free(buf);
	close(fd);
	return (localinfo);
}

static int
islocal(localinfo, addr)
struct ifinfo *localinfo;
struct in_addr addr;
{
	struct ifinfo *lp;

	if (!localinfo)
	    return (0);

	for (lp = localinfo; lp->addr.s_addr; lp++) {
		if ((addr.s_addr & lp->netmask.s_addr) ==
		    (lp->addr.s_addr & lp->netmask.s_addr)) {
			return (1);
		}
	}
	return (0);
}

/*
 * This routine is the "internal" TCP/IP routine that will build a
 * host/service pair into one or more netbufs depending on how many
 * addresses the host has in the host table.
 * If the hostname is HOST_SELF_BIND, we return 0.0.0.0 so that the
 * binding can be contacted through all interfaces.
 * If the hostname is HOST_SELF_CONNECT, we return 127.0.0.1 so that the
 * address can be connected to locally.
 * If the hostname is HOST_ANY, we return no addresses because IP doesn't
 * know how to specify a service without a host.
 * And finally if we specify HOST_BROADCAST then we ask a tli fd to tell
 * us what the broadcast addresses are for any udp interfaces on this
 * machine.
 */

struct nd_addrlist *
#if defined(PIC)
_netdir_getbyname(tp, serv)
#elif defined(SWITCH)
switch_netdir_getbyname(tp, serv)
#else
tcp_netdir_getbyname(tp, serv)
#endif
	struct netconfig *tp;
	struct nd_hostserv *serv;
{
	struct hostent	*he;
	struct hostent	h_broadcast;
	struct nd_addrlist *result;
	struct netbuf	*na;
	char		**t;
	struct sockaddr_in	*sa;
	int		num;
	int		server_port;
	char		*baddrlist[MAXBCAST + 1];
	struct in_addr	inaddrs[MAXBCAST];
	struct ifinfo   *localinfo;
	int		localif;

#ifdef	SWITCH
	nss_XbyY_buf_t	*b = 0;
#endif
	if (!serv || !tp) {
		_nderror = ND_BADARG;
		return (NULL);
	}
	_nderror = ND_OK;	/* assume success */

	/* NULL is not allowed, that returns no answer */
	if (! (serv->h_host)) {
		_nderror = ND_NOHOST;
		return (NULL);
	}

	/*
	 * Find the port number for the service. We look for some
	 * special cases first and on failing go into getservbyname().
	 * The special cases :
	 * 	NULL - 0 port number.
	 *	rpcbind - The rpcbind's address
	 *	A number - We don't have a name just a number so use it
	 */
#ifndef	SWITCH
	mutex_lock(&one_lane);
#endif
	if (!(serv->h_serv)) {
		server_port = htons(0);
	} else if (strcmp(serv->h_serv, "rpcbind") == 0) {
		server_port = htons(111);	/* Hard coded */
	} else if (strspn(serv->h_serv, "0123456789")
			== strlen(serv->h_serv)) {
		/* It's a port number */
		server_port = htons(atoi(serv->h_serv));
	} else {
		struct servent		*se;
		char			*proto =
			(strcmp(tp->nc_proto, NC_TCP) == 0) ? "tcp" : "udp";
#ifdef	SWITCH
		GETSERVBUF(b);		/* <==== should check for success */
		se = _switch_getservbyname_r(serv->h_serv, proto,
					     b->result, b->buffer, b->buflen);
#else
		se = _files_getservbyname(serv->h_serv, proto);
#endif

		if (!se) {
#ifdef	SWITCH
			_nss_XbyY_buf_free(b);
#else
			mutex_unlock(&one_lane);
#endif
			_nderror = ND_NOSERV;
			return (NULL);
		}
		server_port = se->s_port;
#ifdef	SWITCH
		_nss_XbyY_buf_free(b);
		b = 0;
#endif
	}

	if ((strcmp(serv->h_host, HOST_SELF_BIND) == 0)) {
		he = &localent;
	} else if ((strcmp(serv->h_host, HOST_SELF_CONNECT) == 0)) {
		he = &connectent;
	} else if ((strcmp(serv->h_host, "localhost") == 0)) {
		he = &connectent;
	} else if ((strcmp(serv->h_host, HOST_BROADCAST) == 0)) {
		int bnets, i;

		memset((char *)inaddrs, 0, sizeof (struct in_addr) * MAXBCAST);
		bnets = getbroadcastnets(tp, inaddrs);
		if (bnets == 0) {
#ifndef	SWITCH
			mutex_unlock(&one_lane);
#endif
			return (NULL);
		}
		he = &h_broadcast;
		he->h_name = "broadcast";
		he->h_aliases = NULL;
		he->h_addrtype = AF_INET;
		he->h_length = 4;
		for (i = 0; i < bnets; i++)
			baddrlist[i] = (char *)&inaddrs[i];
		baddrlist[i] = NULL;
		he->h_addr_list = baddrlist;
	} else {
#ifdef	SWITCH
		int			dummy;

		GETHOSTBUF(b);		/* <==== should check for success */
		he = _switch_gethostbyname_r(serv->h_host,
					     b->result, b->buffer, b->buflen,
					     &dummy);
#else
		he = _files_gethostbyname(serv->h_host);
#endif
	}

	if (!he) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(b);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOHOST;
		return (NULL);
	}

	result = (struct nd_addrlist *)(malloc(sizeof (struct nd_addrlist)));
	if (!result) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(b);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOMEM;
		return (NULL);
	}

	/* Count the number of addresses we have */
	for (num = 0, t = he->h_addr_list; *t; t++, num++)
			;

	result->n_cnt = num;
	result->n_addrs = (struct netbuf *)
				(calloc(num, sizeof (struct netbuf)));
	if (!result->n_addrs) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(b);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOMEM;
		return (NULL);
	}

	localinfo = get_local_info();

	/* build up netbuf structs for all addresses - locals first */
	na = result->n_addrs;
	for (localif = 1; localif >= 0; localif--) {
		for (t = he->h_addr_list; *t; t++) {
			/*
			 * first pass gets only local addresses,
			 * second gets only nonlocal addresses
			 */
			if ((localif && !islocal(localinfo, *t)) ||
			    (!localif && islocal(localinfo, *t)))
				continue;

			sa = (struct sockaddr_in *)calloc(1, sizeof (*sa));
			if (!sa) {
				if (localinfo)
					free(localinfo);
#ifdef	SWITCH
				_nss_XbyY_buf_free(b);
#else
				mutex_unlock(&one_lane);
#endif
				_nderror = ND_NOMEM;
				return (NULL);
			}
			/* Vendor specific, that is why it's hard coded here */
			na->maxlen = sizeof (struct sockaddr_in);
			na->len = sizeof (struct sockaddr_in);
			na->buf = (char *)sa;
			sa->sin_family = AF_INET;
			sa->sin_port = server_port;
			memcpy((char *)&(sa->sin_addr), *t,
				sizeof (sa->sin_addr));
			na++;
		}
	}
	if (localinfo)
	    free(localinfo);
#ifdef	SWITCH
	_nss_XbyY_buf_free(b);
#else
	mutex_unlock(&one_lane);
#endif
	return (result);
}

/*
 * This routine is the "internal" TCP/IP routine that will build a
 * host/service pair from the netbuf passed. Currently it only
 * allows one answer, it should, in fact allow several.
 */
struct nd_hostservlist *
#if defined(PIC)
_netdir_getbyaddr(tp, addr)
#elif defined(SWITCH)
switch_netdir_getbyaddr(tp, addr)
#else
tcp_netdir_getbyaddr(tp, addr)
#endif
	struct netconfig	*tp;
	struct netbuf		*addr;
{
	struct sockaddr_in	*sa;		/* TCP/IP temporaries */
	struct servent		*se;
	struct hostent		*he;
	struct nd_hostservlist	*result;	/* Final result		*/
	struct nd_hostserv	*hs;		/* Pointer to the array */
	int			servs, hosts;	/* # of hosts, services */
	char			**hn, **sn;	/* host, service names */
	int			i, j;		/* some counters	*/
#ifdef	SWITCH
	nss_XbyY_buf_t		*hb = 0;	/* Buffer for hostent	*/
	nss_XbyY_buf_t		*sb = 0;	/* Buffer for servent	*/
#endif

	if (!addr || !tp) {
		_nderror = ND_BADARG;
		return (NULL);
	}
	_nderror = ND_OK; /* assume success */

	/* XXX how much should we trust this ? */
	sa = (struct sockaddr_in *)(addr->buf);

#ifndef	SWITCH
	mutex_lock(&one_lane);
#endif
	if (sa->sin_addr.s_addr == INADDR_LOOPBACK)
		he = &connectent;
	else if (sa->sin_addr.s_addr == INADDR_ANY)
		/*
		 * XXX
		 * Nobody is going to really do this lookup, except
		 * getservbyport() that calls netdir_getbyaddr() and
		 * want to tell it to fail if the service port lookup
		 * is not successful. Obviously, this is a hack !
		 */
		he = &localent;
	else {
#if defined(SWITCH)
		int			dummy;
	/*
	 * Call the _switch_gethostbyaddr_r that uses the "switch"
	 * policy to do name service lookups.
	 */
		GETHOSTBUF(hb);		/* <==== should check for success */
		he = _switch_gethostbyaddr_r(&(sa->sin_addr.s_addr),
					     4, sa->sin_family,
					     hb->result, hb->buffer,
					     hb->buflen, &dummy);
#else
		he = _files_gethostbyaddr(&(sa->sin_addr.s_addr),
					  4, sa->sin_family);
#endif
	}
	/* first determine the host */
	if (!he) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(hb);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOHOST;
		return (NULL);
	}

	/* Now determine the service */
	if (sa->sin_port == 0) {
		/*
		 * The port number 0 is a reserved port for both UDP & TCP.
		 * We are assuming that this is used to just get
		 * the host name and to bypass the service name.
		 */
		servs = 1;
		se = NULL;
	} else {
		char			*proto =
			(strcmp(tp->nc_proto, NC_TCP) == 0) ? "tcp" : "udp";
#if defined(SWITCH)
		GETSERVBUF(sb);		/* <==== should check for success */

		se = _switch_getservbyport_r(sa->sin_port, proto,
					     sb->result, sb->buffer,
					     sb->buflen);
#else
		se = _files_getservbyport(sa->sin_port, proto);
#endif
		if (!se && (sa->sin_addr.s_addr == INADDR_ANY)) {
			/*
			 * A special case, to help getservbyport() to
			 * continue service port lookup (if it failed
			 * in one naming service).
			 */
#ifdef	SWITCH
			_nss_XbyY_buf_free(hb);
			_nss_XbyY_buf_free(sb);
#else
			mutex_unlock(&one_lane);
#endif
			_nderror = ND_NOSERV;
			return (NULL);
		}
		if (!se) {
			/* It is not a well known service */
			servs = 1;
		}
	}

	/* now build the result for the client */
	result = (struct nd_hostservlist *)
			malloc(sizeof (struct nd_hostservlist));
	if (!result) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(hb);
		_nss_XbyY_buf_free(sb);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOMEM;
		return (NULL);
	}

	/*
	 * We initialize the counters to 1 rather than zero because
	 * we have to count the "official" name as well as the aliases.
	 */
	for (hn = he->h_aliases, hosts = 1; hn && *hn; hn++, hosts++)
		;

	if (se)
		for (sn = se->s_aliases, servs = 1; sn && *sn; sn++, servs++)
			;

	hs = (struct nd_hostserv *)calloc(hosts * servs,
			sizeof (struct nd_hostserv));
	if (!hs) {
#ifdef	SWITCH
		_nss_XbyY_buf_free(hb);
		_nss_XbyY_buf_free(sb);
#else
		mutex_unlock(&one_lane);
#endif
		_nderror = ND_NOMEM;
		free((void *)result);
		return (NULL);
	}

	result->h_cnt	= servs * hosts;
	result->h_hostservs = hs;

	/* Now build the list of answers */

	for (i = 0, hn = he->h_aliases; i < hosts; i++) {
		sn = se ? se->s_aliases : NULL;

		for (j = 0; j < servs; j++) {
			if (! i)
				hs->h_host = strdup(he->h_name);
			else
				hs->h_host = strdup(*hn);
			if (! j) {
				if (se) {
					hs->h_serv = strdup(se->s_name);
				} else {
					/* Convert to a number string */
					char stmp[16];

					sprintf(stmp, "%d", sa->sin_port);
					hs->h_serv = strdup(stmp);
				}
			} else {
				hs->h_serv = strdup(*sn++);
			}

			if (!(hs->h_host) || !(hs->h_serv)) {
#ifdef	SWITCH
				_nss_XbyY_buf_free(hb);
				_nss_XbyY_buf_free(sb);
#else
				mutex_unlock(&one_lane);
#endif
				_nderror = ND_NOMEM;
				free((void *)result->h_hostservs);
				free((void *)result);
				return (NULL);
			}
			hs ++;
		}
		if (i)
			hn++;
	}
#ifdef	SWITCH
	_nss_XbyY_buf_free(hb);
	_nss_XbyY_buf_free(sb);
#else
	mutex_unlock(&one_lane);
#endif

	return (result);
}

/*
 * This internal routine will merge one of those "universal" addresses
 * to the one which will make sense to the remote caller.
 */
static char *
_netdir_mergeaddr(tp, ruaddr, uaddr)
	struct netconfig	*tp;	/* the transport provider */
	char			*ruaddr; /* remote uaddr of the caller */
	char			*uaddr;	/* the address */
{
	char	tmp[32], *cp, *buf;
	struct	ifconf ifc;
	struct	ifreq ifreq, *ifr;
	int		fd, numifs, n, i, j;
	struct	in_addr clientaddr, bestmatch, netmask;
	struct	sockaddr_in *sin;

	if (!uaddr || !ruaddr || !tp) {
		_nderror = ND_BADARG;
		return ((char *)NULL);
	}
	if (strncmp(ruaddr, "0.0.0.0.", strlen("0.0.0.0.")) == 0)
		/* thats me: return the way it is */
		return (strdup(uaddr));

	/*
	 * Convert remote uaddr into an in_addr so that we can compare
	 * to it.  Shave off last two dotted-decimal values.
	 */
	for (cp = ruaddr, j = 0; j < 4; j++, cp++)
		cp = strchr(cp, '.');

	if(cp != NULL)
		*--cp = '\0';	/* null out the dot after the IP addr */

	clientaddr.s_addr = inet_addr(ruaddr);

#ifdef DEBUG
	printf("client's address is %s and %s\n",
		ruaddr, inet_ntoa(clientaddr));
#endif

	*cp = '.';	/* Put the dot back in the IP addr */

	if ((fd = open(UDP, O_RDONLY)) < 0) {
		(void) syslog(LOG_ERR,
	"n2a_mergeaddr: open to get interface configuration: %m");
		_nderror = ND_OPEN;
		return ((char *)NULL);
	}

#ifdef SIOCGIFNUM
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		perror("ioctl (get number of interfaces)");
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	buf = (char *)malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		(void) syslog(LOG_ERR, "n2a_mergeaddr: malloc failed: %m");
		close(fd);
		_nderror = ND_NOMEM;
		return ((char *)NULL);
	}
	ifc.ifc_len = numifs * sizeof (struct ifreq);
	ifc.ifc_buf = buf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
		(void) syslog(LOG_ERR,
		"n2a_mergeaddr: ioctl (get interface configuration): %m");
		close(fd);
		free(buf);
		_nderror = ND_SYSTEM;
		return ((char *)NULL);
	}
	ifr = (struct ifreq *)buf;
	numifs = ifc.ifc_len/sizeof (struct ifreq);

	bestmatch.s_addr = 0;
	for (i = 0, n = numifs; n > 0; n--, ifr++) {
		u_int ifrflags;

		ifreq = *ifr;
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
			"n2a_mergeaddr: ioctl (get interface flags): %m");
			continue;
		}
		ifrflags = ifreq.ifr_flags;
		if (((ifrflags & IFF_UP) == 0) ||
			(ifr->ifr_addr.sa_family != AF_INET))
			continue;

		if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
			"n2a_mergeaddr: ioctl (get interface netmask): %m");
			continue;
		}
		netmask = ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr;

		if (ioctl(fd, SIOCGIFADDR, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
			"n2a_mergeaddr: ioctl (get interface address): %m");
			continue;
		}
		sin = (struct sockaddr_in *) &ifreq.ifr_addr;

#ifdef DEBUG
	printf("mergeaddr: interface %s\n", inet_ntoa(sin->sin_addr));
	printf("mergeaddr: netmask %s\n", inet_ntoa(netmask));
#endif

		if ((sin->sin_addr.s_addr & netmask.s_addr) ==
		    (clientaddr.s_addr & netmask.s_addr)) {
			bestmatch = sin->sin_addr;
#ifdef DEBUG
	printf("mergeaddr: found bestmatch %s\n", inet_ntoa(bestmatch));
	printf("mergeaddr: for netmask %s\n", inet_ntoa(netmask));
#endif
		}

		/*
		 * Save the first non-loopback address as a backup
		 * in case the netmask comparison does not give us
		 * the best address.
		 */
		if ((bestmatch.s_addr == 0) &&
		    ((ifrflags & IFF_LOOPBACK) == 0)) {
			bestmatch = sin->sin_addr;
#ifdef DEBUG
	printf("mergeaddr: default bestmatch %s\n", inet_ntoa(bestmatch));
#endif
		}
	}
	free(buf);
	close(fd);

	if (bestmatch.s_addr)
		_nderror = ND_OK;
	else {
		_nderror = ND_NOHOST;
		return ((char *)NULL);
	}

	/* prepare the reply */
	memset(tmp, '\0', sizeof (tmp));

	/* reply consists of the IP addr of the closest interface */
	strcpy (tmp, inet_ntoa(bestmatch));

	/*
	 * ... and the port number part (last two dotted-decimal values)
	 * of uaddr
	 */
	for (cp = uaddr, j = 0; j < 4; j++, cp++)
		cp = strchr(cp, '.');
	strcat(tmp, --cp);

	return (strdup(tmp));
}


int
#if defined(PIC)
_netdir_options(tp, opts, fd, par)
#elif defined(SWITCH)
switch_netdir_options(tp, opts, fd, par)
#else
tcp_netdir_options(tp, opts, fd, par)
#endif
	struct netconfig *tp;
	int opts;
	int fd;
	char *par;
{
	struct nd_mergearg *ma;

	switch (opts) {
	case ND_SET_BROADCAST:
		/* Every one is allowed to broadcast without asking */
		return (ND_OK);
	case ND_SET_RESERVEDPORT:	/* bind to a resered port */
		return (bindresvport(fd, (struct netbuf *)par));
	case ND_CHECK_RESERVEDPORT:	/* check if reserved prot */
		return (checkresvport((struct netbuf *)par));
	case ND_MERGEADDR:	/* Merge two addresses */
		ma = (struct nd_mergearg *)(par);
		ma->m_uaddr = _netdir_mergeaddr(tp, ma->c_uaddr, ma->s_uaddr);
		return (_nderror);
	default:
		return (ND_NOCTRL);
	}
}


/*
 * This internal routine will convert a TCP/IP internal format address
 * into a "universal" format address. In our case it prints out the
 * decimal dot equivalent. h1.h2.h3.h4.p1.p2 where h1-h4 are the host
 * address and p1-p2 are the port number.
 */
char *
#if defined(PIC)
_taddr2uaddr(tp, addr)
#elif defined(SWITCH)
switch_taddr2uaddr(tp, addr)
#else
tcp_taddr2uaddr(tp, addr)
#endif
	struct netconfig	*tp;	/* the transport provider */
	struct netbuf		*addr;	/* the netbuf struct */
{
	struct sockaddr_in	*sa;	/* our internal format */
	char			tmp[32];
	unsigned short		myport;

	if (!addr || !tp) {
		_nderror = ND_BADARG;
		return (NULL);
	}
	sa = (struct sockaddr_in *)(addr->buf);
	myport = ntohs(sa->sin_port);
	inet_ntoa_r(sa->sin_addr, tmp);
	sprintf(tmp + strlen(tmp), ".%d.%d", myport >> 8, myport & 255);
	return (strdup(tmp));	/* Doesn't return static data ! */
}

/*
 * This internal routine will convert one of those "universal" addresses
 * to the internal format used by the Sun TLI TCP/IP provider.
 */

struct netbuf *
#if defined(PIC)
_uaddr2taddr(tp, addr)
#elif defined(SWITCH)
switch_uaddr2taddr(tp, addr)
#else
tcp_uaddr2taddr(tp, addr)
#endif
	struct netconfig	*tp;	/* the transport provider */
	char			*addr;	/* the address */
{
	struct sockaddr_in	*sa;
	unsigned long		inaddr;
	unsigned short		inport;
	int			h1, h2, h3, h4, p1, p2;
	struct netbuf		*result;

	if (!addr || !tp) {
		_nderror = ND_BADARG;
		return ((struct netbuf *) 0);
	}
	result = (struct netbuf *) malloc(sizeof (struct netbuf));
	if (!result) {
		_nderror = ND_NOMEM;
		return ((struct netbuf *) 0);
	}

	sa = (struct sockaddr_in *)calloc(1, sizeof (*sa));
	if (!sa) {
		free((void *) result);
		_nderror = ND_NOMEM;
		return ((struct netbuf *) 0);
	}
	result->buf = (char *)(sa);
	result->maxlen = sizeof (struct sockaddr_in);
	result->len = sizeof (struct sockaddr_in);

	/* XXX there is probably a better way to do this. */
	if (sscanf(addr, "%d.%d.%d.%d.%d.%d", &h1, &h2, &h3, &h4,
		&p1, &p2) != 6) {
		free((void *) result);
		_nderror = ND_NO_RECOVERY;
		return ((struct netbuf *) 0);
	}

	/* convert the host address first */
	inaddr = (h1 << 24) + (h2 << 16) + (h3 << 8) + h4;
	sa->sin_addr.s_addr = htonl(inaddr);

	/* convert the port */
	inport = (p1 << 8) + p2;
	sa->sin_port = htons(inport);

	sa->sin_family = AF_INET;

	return (result);
}

#define	MAXIFS	32

static int
getbroadcastnets(tp, addrs)
	struct netconfig *tp;
	struct in_addr *addrs;
{
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct sockaddr_in *sin;
	int fd;
	int n, i, numifs;
	char *buf;
	int	use_loopback = 0;

	_nderror = ND_SYSTEM;
	fd = open(tp->nc_device, O_RDONLY);
	if (fd < 0) {
		(void) syslog(LOG_ERR,
		"broadcast: open to get interface configuration: %m");
		return (0);
	}
#ifdef SIOCGIFNUM
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		perror("ioctl (get number of interfaces)");
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	buf = (char *)malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		(void) syslog(LOG_ERR, "broadcast: malloc failed: %m");
		close(fd);
		return (0);
	}
	ifc.ifc_len = numifs * sizeof (struct ifreq);
	ifc.ifc_buf = buf;
	/*
	 * Ideally, this ioctl should also tell me, how many bytes were
	 * finally allocated, but it doesnt.
	 */
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
		(void) syslog(LOG_ERR,
		"broadcast: ioctl (get interface configuration): %m");
		close(fd);
		return (0);
	}

retry:
	ifr = (struct ifreq *)buf;
	for (i = 0, n = ifc.ifc_len/sizeof (struct ifreq);
		n > 0 && i < MAXBCAST; n--, ifr++) {
		ifreq = *ifr;
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			(void) syslog(LOG_ERR,
			"broadcast: ioctl (get interface flags): %m");
			continue;
		}
		if (!(ifreq.ifr_flags & IFF_UP) ||
		    (ifr->ifr_addr.sa_family != AF_INET))
			continue;
		if (ifreq.ifr_flags & IFF_BROADCAST) {
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			if (ioctl(fd, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				/* May not work with other implementation */
				addrs[i++] = inet_makeaddr(
					inet_netof(sin->sin_addr),
					INADDR_ANY);
			} else {
				addrs[i++] = ((struct sockaddr_in*)
						&ifreq.ifr_addr)->sin_addr;
			}
			continue;
		}
		if (use_loopback && (ifreq.ifr_flags & IFF_LOOPBACK)) {
			sin = (struct sockaddr_in *)&ifr->ifr_addr;
			addrs[i++] = sin->sin_addr;
			continue;
		}
		if (ifreq.ifr_flags & IFF_POINTOPOINT) {
			if (ioctl(fd, SIOCGIFDSTADDR, (char *)&ifreq) < 0)
				continue;
			addrs[i++] = ((struct sockaddr_in*)
				    &ifreq.ifr_addr)->sin_addr;
			continue;
		}
	}
	if (i == 0 && !use_loopback) {
		use_loopback = 1;
		goto retry;
	}
	free(buf);
	close(fd);
	if (i)
		_nderror = ND_OK;
	return (i);
}

static mutex_t	port_lock = DEFAULTMUTEX;
static short	port;

static
bindresvport(fd, addr)
	int fd;
	struct netbuf *addr;
{
	int res;
	struct sockaddr_in myaddr;
	struct sockaddr_in *sin;
	int i;
	struct t_bind tbindstr, *tres;
	struct t_info tinfo;

#define	STARTPORT 600
#define	ENDPORT (IPPORT_RESERVED - 1)
#define	NPORTS	(ENDPORT - STARTPORT + 1)

	_nderror = ND_SYSTEM;
	if (geteuid()) {
		errno = EACCES;
		return (-1);
	}
	if ((i = t_getstate(fd)) != T_UNBND) {
		if (t_errno == TBADF)
			errno = EBADF;
		if (i != -1)
			errno = EISCONN;
		return (-1);
	}
	if (addr == NULL) {
		sin = &myaddr;
		(void) memset((char *)sin, 0, sizeof (*sin));
		sin->sin_family = AF_INET;
	} else {
		sin = (struct sockaddr_in *)addr->buf;
		if (sin->sin_family != AF_INET) {
			errno = EPFNOSUPPORT;
			return (-1);
		}
	}

	res = -1;
	errno = EADDRINUSE;
	/* Transform sockaddr_in to netbuf */
	if (t_getinfo(fd, &tinfo) == -1)
		return (-1);
	tres = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if (tres == NULL) {
		_nderror = ND_NOMEM;
		return (-1);
	}

	tbindstr.qlen = 0; /* Always 0; user should change if he wants to */
	tbindstr.addr.buf = (char *)sin;
	tbindstr.addr.len = tbindstr.addr.maxlen = __rpc_get_a_size(tinfo.addr);
	sin = (struct sockaddr_in *)tbindstr.addr.buf;

	mutex_lock(&port_lock);
	if (port == 0)
		port = (getpid() % NPORTS) + STARTPORT;
	for (i = 0; i < NPORTS && errno == EADDRINUSE; i++) {
		sin->sin_port = htons(port++);
		if (port > ENDPORT)
			port = STARTPORT;
		res = t_bind(fd, &tbindstr, tres);
		if (res == 0) {
			if ((tbindstr.addr.len == tres->addr.len) &&
				(memcmp(tbindstr.addr.buf, tres->addr.buf,
					(int)tres->addr.len) == 0))
				break;
			(void) t_unbind(fd);
		}
	}
	mutex_unlock(&port_lock);

	(void) t_free((char *)tres, T_BIND);
	if (i != NPORTS) {
		_nderror = ND_OK;
	} else {
		_nderror = ND_FAILCTRL;
		res = 1;
	}
	return (res);
}

static
checkresvport(addr)
	struct netbuf *addr;
{
	struct sockaddr_in *sin;
	unsigned short port;

	if (addr == NULL) {
		_nderror = ND_FAILCTRL;
		return (-1);
	}
	sin = (struct sockaddr_in *)(addr->buf);
	port = ntohs(sin->sin_port);
	if (port < IPPORT_RESERVED)
		return (0);
	return (1);
}
