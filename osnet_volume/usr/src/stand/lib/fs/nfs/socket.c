/*
 * Copyright (C) 1991-1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * socket.c, Code implementing a simple socket interface.
 */

#pragma ident	"@(#)socket.c	1.1	99/02/22 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/isa_defs.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/salib.h>
#include "socket_inet.h"
#include "ipv4.h"
#include "udp_inet.h"
#include "mac.h"
#include <sys/promif.h>

struct inetboot_socket	sockets[MAXSOCKET];

/*
 * Create an endpoint for network communication. Returns a descriptor.
 *
 * Notes:
 *	Only PF_INET and PF_INET6 communication domains are supported. Within
 * 	this domain, only SOCK_RAW and SOCK_DGRAM types are supported.
 */
int
socket(int domain, int type, int protocol)
{
	static int sock_initialized;
	int i;

	errno = 0;

	if (!sock_initialized) {
		for (i = 0; i < MAXSOCKET; i++)
			sockets[i].type = INETBOOT_UNUSED;
		sock_initialized = TRUE;
	}
	if (domain != AF_INET) {
		errno = EPROTONOSUPPORT;
		return (-1);
	}

	/* Find available socket */
	for (i = 0; i < MAXSOCKET; i++) {
		if (sockets[i].type == INETBOOT_UNUSED)
			break;
	}
	if (i >= MAXSOCKET) {
		errno = EMFILE;	/* No slots left. */
		return (-1);
	}

	/* A case statement in case we ever feel like doing TCP/SOCK_STREAM. */
	switch (type) {
	case SOCK_RAW:
		ipv4_raw_socket(&sockets[i], (uint8_t)protocol);
		break;
	case SOCK_DGRAM:
		udp_socket_init(&sockets[i]);
		break;
	case SOCK_STREAM:
		/* FALLTHRU */
	default:
		errno = EPROTOTYPE;
		break;
	}

	if (errno != 0)
		return (-1);

	/* IPv4 generic initialization. */
	ipv4_socket_init(&sockets[i]);

	/* MAC generic initialization. */
	mac_socket_init(&sockets[i]);

	return (i + SOCKETTYPE);
}

/*
 * The socket options we support are:
 * SO_RCVTIMEO	-	Value is in msecs, and is of uint32_t.
 * SO_DONTROUTE	-	Valid is an int, and is a boolean (nonzero if set).
 */
int
getsockopt(int s, int level, int option, const void *optval, size_t optlen)
{
	int i;

	errno = 0;
	i = FD_TO_SOCKET(s);

	if (sockets[i].type == INETBOOT_UNUSED) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (i < 0 || i >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (level != IPPROTO_IP) {
		errno = ENOPROTOOPT;
		return (-1);
	}
	if (option == SO_RCVTIMEO) {
		if (optlen == sizeof (uint32_t))
			*(uint32_t *)optval = sockets[i].in_timeout;
		else {
			errno = EINVAL;
			return (-1);
		}
	} else if (option == SO_DONTROUTE) {
		if (optlen == sizeof (int)) {
			*(int *)optval = (sockets[i].out_flags & SO_DONTROUTE);
		} else {
			errno = EINVAL;
			return (-1);
		}
	} else {
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

/*
 * The socket options we support are:
 * SO_RECVTIMEO	-	Value is uint32_t msecs.
 * SO_DONTROUTE	-	Value is int boolean (nonzero == TRUE, zero == FALSE).
 */
int
setsockopt(int s, int level, int option, const void *optval, size_t optlen)
{
	int i;

	errno = 0;
	i = FD_TO_SOCKET(s);

	if (sockets[i].type == INETBOOT_UNUSED) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (i < 0 || i >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (level != IPPROTO_IP) {
		errno = ENOPROTOOPT;
		return (-1);
	}
	if (option == SO_RCVTIMEO) {
		if (optlen == sizeof (uint32_t))
			sockets[i].in_timeout = *(uint32_t *)optval;
		else {
			errno = EINVAL;
			return (-1);
		}
	} else if (option == SO_DONTROUTE) {
		if (optlen == sizeof (int))
			if (*(int *)optval)
				sockets[i].out_flags |= SO_DONTROUTE;
			else
				sockets[i].out_flags &= ~SO_DONTROUTE;
		else {
			errno = EINVAL;
			return (-1);
		}
	} else {
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

/*
 * "close" a socket.
 */
int
socket_close(int s)
{
	int i;

	errno = 0;
	i = FD_TO_SOCKET(s);

	if (i < 0 || i >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}

	nuke_grams(&sockets[i].inq);

	bzero((caddr_t)&sockets[i], sizeof (struct inetboot_socket));
	sockets[i].type = INETBOOT_UNUSED;
	return (0);
}

/* Assign a name to an unnamed socket. */
int
bind(int s, const struct sockaddr *name, int namelen)
{
	int	i, k;

	errno = 0;

	i = FD_TO_SOCKET(s);

	if (i < 0 || i >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (sockets[i].type == INETBOOT_UNUSED) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (name == NULL) {
		/* unbind */
		if (sockets[i].bound) {
			bzero((caddr_t)&sockets[i].bind,
			    sizeof (struct sockaddr_in));
			sockets[i].bound = FALSE;
		}
		return (0);
	}
	if (namelen != sizeof (struct sockaddr_in) || name == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (name->sa_family != AF_INET) {
		errno = EAFNOSUPPORT;
		return (-1);
	}
	if (sockets[i].bound) {
		if (bcmp((caddr_t)&sockets[i].bind, (caddr_t)name,
		    namelen) == 0) {
			/* attempt to bind to same address ok... */
			return (0);
		}
		errno = EINVAL;	/* already bound */
		return (-1);
	}
	for (k = 0; k < MAXSOCKET; k++) {
		if (sockets[k].type != INETBOOT_UNUSED && sockets[k].bound) {
			if (bcmp((caddr_t)&sockets[k].bind,
			    (caddr_t)&sockets[i].bind,
			    sizeof (struct sockaddr_in)) == 0) {
				errno = EADDRINUSE;
				return (-1);
			}
		}
	}

	bcopy((caddr_t)name, (caddr_t)&sockets[i].bind, namelen);
	sockets[i].bound = TRUE;

	return (0);
}

/*
 * Receive messages from a connectionless socket. Legal flags are 0 and
 * MSG_DONTWAIT. MSG_WAITALL is not currently supported.
 *
 * Returns length of message for success, -1 if error occurred.
 */
int
recvfrom(int s, char *buf, int len, int flags, struct sockaddr *from,
    int *fromlen)
{
	int			index, i, bytes = 0, datalen;
	struct inetgram		*icp;

	errno = 0;

	index = FD_TO_SOCKET(s);

	if (index < 0 || index >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (sockets[index].type == INETBOOT_UNUSED) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (buf == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if ((flags & ~MSG_DONTWAIT) != 0) {
		errno = EINVAL;
		return (-1);
	}

retry:
	if (sockets[index].inq == NULL) {
		/* Go out and check the wire */
		for (i = MEDIA_LVL; i < APP_LVL; i++) {
			if (sockets[index].input[i] != NULL) {
				if (sockets[index].input[i](index) < 0)
					return (-1);
			}
		}
	}

	while ((icp = sockets[index].inq) != NULL && bytes == 0) {
		if (sockets[index].type == INETBOOT_DGRAM &&
		    icp->igm_level != APP_LVL) {
#ifdef	DEBUG
			printf("recvfrom: unexpected level %d frame found\n",
			    icp->igm_level);
#endif	/* DEBUG */
			del_gram(&sockets[index].inq, icp, TRUE);
		} else {
			if (from != NULL && fromlen != NULL) {
				if (*fromlen > sizeof (icp->igm_saddr))
					*fromlen = sizeof (icp->igm_saddr);
				bcopy((caddr_t)&(icp->igm_saddr), (caddr_t)from,
				    *fromlen);
			}
			/* Length of inetgram includes header */
			switch (sockets[index].type) {
			case INETBOOT_DGRAM:
				datalen = icp->igm_len -
				    sockets[index].headerlen[TRANSPORT_LVL]();
				break;
			case INETBOOT_RAW:
				/* FALLTHRU */
			default:
				datalen = icp->igm_len -
				    sockets[index].headerlen[NETWORK_LVL]();
				break;
			}
			if (len < datalen)
				bytes = len;
			else
				bytes = datalen;
#ifdef	DEBUG
			printf("recvfrom(%d): data: (0x%x,%d)\n", index,
			    icp->igm_datap, bytes);
#endif	/* DEBUG */
			/*
			 * SOCK_STREAM - if tcp, and user asks for less data
			 * than is in the packet, we'll lose the remainder.
			 * Fix if TCP is implemented XXX.
			 */
			bcopy(icp->igm_datap, buf, bytes);
			del_gram(&sockets[index].inq, icp, TRUE);
			break; /* Yup - MSG_WAITALL not implemented */
		}
	}

	if (bytes == 0) {
		if ((flags & MSG_DONTWAIT) == 0)
			goto retry;	/* wait forever */

		/* no data */
		errno = EWOULDBLOCK;
		bytes = -1;
	}

	return (bytes);
}

/*
 * Transmit a message through a connectionless socket.
 *
 * Supported flags: MSG_DONTROUTE or 0.
 */
int
sendto(int s, const char *msg, int len, int flags, const struct sockaddr *to,
    int tolen)
{
	static in_port_t	source_port;
	struct inetgram		oc;
	int			i, l, offset, tlen;

	errno = 0;

	i = FD_TO_SOCKET(s);
	if (i < 0 || i >= MAXSOCKET) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (sockets[i].type == INETBOOT_UNUSED) {
		errno = ENOTSOCK;
		return (-1);
	}
	if (msg == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if ((flags & ~MSG_DONTROUTE) != 0|| to == NULL ||
	    tolen != sizeof (struct sockaddr_in)) {
		errno = EINVAL;
		return (-1);
	}
	if (to->sa_family != AF_INET) {
		errno = EAFNOSUPPORT;
		return (-1);
	}
#ifdef	DEBUG
	{
	struct sockaddr_in *sin = (struct sockaddr_in *)to;
	printf("sendto(%d): msg of length: %d sent to port %d and host: %s\n",
	    i, len, ntohs(sin->sin_port), inet_ntoa(sin->sin_addr));
	}
#endif	/* DEBUG */

	nuke_grams(&sockets[i].inq); /* flush the input queue */

	/* calculate offset for data */
	offset = sockets[i].headerlen[MEDIA_LVL]() +
	    (sockets[i].headerlen[NETWORK_LVL])();

	bzero((caddr_t)&oc, sizeof (oc));
	if (sockets[i].type != INETBOOT_RAW) {
		offset += (sockets[i].headerlen[TRANSPORT_LVL])();
		oc.igm_level = TRANSPORT_LVL;
	} else
		oc.igm_level = NETWORK_LVL;
	oc.igm_oflags = flags;

	bcopy((caddr_t)to, (caddr_t)&oc.igm_saddr, tolen);

	/*
	 * Choose a legal source port if the socket isn't bound and the
	 * the caller hasn't specified one.
	 */
	if (sockets[i].bound == FALSE &&
	    ntohs(((struct sockaddr_in *)to)->sin_port) == 0) {
		if (source_port <= IPPORT_RESERVED ||
		    source_port > IPPORT_USERRESERVED) {
			source_port = IPPORT_RESERVED + 1;
		} else
			++source_port;

		((struct sockaddr_in *)&oc.igm_saddr)->sin_port =
		    htons(source_port);
	}

	/* Round up to 16bit value for checksum purposes */
	if (sockets[i].type == INETBOOT_DGRAM) {
		tlen = ((len + sizeof (uint16_t) - 1) &
		    ~(sizeof (uint16_t) - 1));
	} else
		tlen = len;

	oc.igm_len = tlen + offset;
	if ((oc.igm_bufp = bkmem_zalloc(oc.igm_len)) == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	oc.igm_datap = (caddr_t)(oc.igm_bufp + offset);
	bcopy((caddr_t)msg, oc.igm_datap, len);
	for (l = TRANSPORT_LVL; l >= MEDIA_LVL; l--) {
		if (sockets[i].output[l] != NULL) {
			if (sockets[i].output[l](i, &oc) < 0) {
				bkmem_free(oc.igm_bufp, oc.igm_len);
				if (errno == 0)
					errno = EIO;
				return (-1);
			}
		}
	}
	bkmem_free(oc.igm_bufp, oc.igm_len);
	return (len);
}

/*
 * Returns ptr to the last inetgram in the list, or null if list is null
 */
struct inetgram *
last_gram(struct inetgram *igp)
{
	struct inetgram	*wp;
	for (wp = igp; wp != NULL; wp = wp->igm_next) {
		if (wp->igm_next == NULL)
			return (wp);
	}
	return (NULL);
}

/*
 * Adds an inetgram or list of inetgrams to the end of the list.
 */
void
add_grams(struct inetgram **igpp, struct inetgram *newgp)
{
	struct inetgram	 *wp;

	if (newgp == NULL)
		return;

	if (*igpp == NULL)
		*igpp = newgp;
	else {
		wp = last_gram(*igpp);
		wp->igm_next = newgp;
	}
}

/*
 * Nuke a whole list of grams.
 */
void
nuke_grams(struct inetgram **lgpp)
{
	while (*lgpp != NULL)
		del_gram(lgpp, *lgpp, TRUE);
}

/*
 * Remove the referenced inetgram. List is altered accordingly. Destroy the
 * referenced inetgram if freeit is TRUE.
 */
void
del_gram(struct inetgram **lgpp, struct inetgram *igp, int freeit)
{
	struct inetgram	*wp, *pp = NULL;

	if (lgpp == NULL || igp == NULL)
		return;

	wp = *lgpp;
	while (wp != NULL) {
		if (wp == igp) {
			/* detach wp from the list */
			if (*lgpp == wp)
				*lgpp = (*lgpp)->igm_next;
			else
				pp->igm_next = wp->igm_next;
			igp->igm_next = NULL;

			if (freeit) {
				bkmem_free(igp->igm_bufp, igp->igm_len);
				bkmem_free((caddr_t)igp,
				    sizeof (struct inetgram));
			}
			break;
		}
		pp = wp;
		wp = wp->igm_next;
	}
}

/*
 * simulated sleep() routine..
 */
void
b_sleep(uint32_t secs)
{
	uint32_t end = (secs * 1000) + prom_gettime();

	while (prom_gettime() < end)
		/* Null body */;
}

/*
 * Figure out from the bootpath what kind of network configuration strategy
 * we should use. Returns the network config strategy.
 */
int
get_netconfig_strategy(void)
{
	int	netconfig_type = NCT_DEFAULT;
#if !defined(__i386)
	/* sparc */
#define	ISSPACE(c) (c == ' ' || c == '\t' || c == '\n' || c == '\0')
	char	lbootpath[OBP_MAXPATHLEN];
	char	net_options[NCT_BUFSIZE];
	char	*op, *nop, *sp;

	/* Set the default based upon whether prom DHCP cache exists */
	if (prom_cached_reply(TRUE))
		netconfig_type = NCT_BOOTP_DHCP;
	else
		netconfig_type = NCT_RARP_BOOTPARAMS;

	/*
	 * Scan bootpath for network options, specifically network configuration
	 * protocol to use.
	 */
	for (op = prom_bootpath(), sp = lbootpath; op != NULL && !ISSPACE(*op);
	    sp++, op++)
		*sp = *op;
	*sp = '\0';
	if ((op = strrchr(lbootpath, '/')) == NULL)	/* find last '/' */
		op = lbootpath;
	else
		op++;
	while (*op != ':' && *op != '\0')	/* look for ':' */
		op++;
	if (*op == ':') {
		for (nop = net_options, op++; *op != '/' && !ISSPACE(*op) &&
		    nop < &net_options[NCT_BUFSIZE]; nop++, op++) {
			*nop = *op;
		}
		*nop = '\0';
		if (strcmp(net_options, "bootp") == 0 ||
		    strcmp(net_options, "dhcp") == 0)
			netconfig_type = NCT_BOOTP_DHCP;
		else if (strcmp(net_options, "rarp") == 0)
			netconfig_type = NCT_RARP_BOOTPARAMS;
	}
	return (netconfig_type);
#undef	ISSPACE
#else
	/* i86 */
	extern struct bootops bootops;
	extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
	char	net_config_strategy[MAXNAMELEN];

	/*
	 * Look at net-config-strategy boot property to determine whether DHCP
	 * Bootparams will be used.
	 */
	net_config_strategy[0] = '\0';
	(void) bgetprop(&bootops, "net-config-strategy", net_config_strategy,
	    sizeof (net_config_strategy), 0);

	if (strncmp(net_config_strategy, "rarp", sizeof ("rarp")) == 0) {
		netconfig_type = NCT_RARP_BOOTPARAMS;
	} else if (strncmp(net_config_strategy, "dhcp", sizeof ("dhcp")) == 0 ||
	    strncmp(net_config_strategy, "bootp", sizeof ("bootp")) == 0) {
		netconfig_type = NCT_BOOTP_DHCP;
	} else
		netconfig_type = NCT_DEFAULT;

	return (netconfig_type);
#endif	/* __i386 */
}

#ifdef	_LITTLE_ENDIAN
uint32_t
htonl(uint32_t in)
{
	uint32_t	i;

	i = (uint32_t)((in & (uint32_t)0xff000000) >> 24) +
	    (uint32_t)((in & (uint32_t)0x00ff0000) >> 8) +
	    (uint32_t)((in & (uint32_t)0x0000ff00) << 8) +
	    (uint32_t)((in & (uint32_t)0x000000ff) << 24);
	return (i);
}

uint32_t
ntohl(uint32_t in)
{
	return (htonl(in));
}

uint16_t
htons(uint16_t in)
{
	register int arg = (int)in;
	uint16_t i;

	i = (uint16_t)(((arg & 0xff00) >> 8) & 0xff);
	i |= (uint16_t)((arg & 0xff) << 8);
	return ((uint16_t)i);
}

uint16_t
ntohs(uint16_t in)
{
	return (htons(in));
}

#else	/* _LITTLE_ENDIAN */

#if defined(lint)

uint32_t
htonl(uint32_t in)
{
	return (in);
}

uint32_t
ntohl(uint32_t in)
{
	return (in);
}

uint16_t
htons(uint16_t in)
{
	return (in);
}

uint16_t
ntohs(uint16_t in)
{
	return (in);
}

#endif	/* lint */
#endif	/* _LITTLE_ENDIAN */
