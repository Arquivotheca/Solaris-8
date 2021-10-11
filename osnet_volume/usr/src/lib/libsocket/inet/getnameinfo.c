
/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getnameinfo.c	1.2	99/10/11 SMI"

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <nss_dbdefs.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define	sa2sin(x)	((struct sockaddr_in *)(x))
#define	sa2sin6(x)	((struct sockaddr_in6 *)(x))

#define	AI_MASK	(NI_NOFQDN | NI_NUMERICHOST | NI_NAMEREQD | NI_NUMERICSERV | \
    NI_DGRAM)

static const char *_inet_ntop_native();
/*
 * getnameinfo:
 *
 * Purpose:
 *   Routine for performing Address-to-nodename in a
 *   protocol-independent fashion.
 * Description:
 *   This function looks up an IP address and port number provided
 *   by the caller in the name service database and returns the nodename
 *   and servname respectively in the buffers provided by the caller.
 * Input Parameters:
 *   sa      - points to either a sockaddr_in structure (for
 *             IPv4) or a sockaddr_in6 structure (for IPv6).
 *   salen   - length of the sockaddr_in or sockaddr_in6 structure.
 *   hostlen - length of caller supplied "host" buffer
 *   servlen - length of caller supplied "serv" buffer
 *   flags   - changes default actions based on setting.
 *       Possible settings for "flags":
 *       NI_NOFQDN - Always return nodename portion of the fully-qualified
 *                   domain name (FQDN).
 *       NI_NUMERICHOST - Always return numeric form of the host's
 *			  address.
 *       NI_NAMEREQD - If hostname cannot be located in database,
 *                     don't return numeric form of address - return
 *                     an error instead.
 *       NI_NUMERICSERV - Always return numeric form of the service address
 *                        instead of its name.
 *       NI_DGRAM - Specifies that the service is a datagram service, and
 *                  causes getservbyport() to be called with a second
 *                  argument of "udp" instead of its default "tcp".
 * Output Parameters:
 *   host - return the nodename associcated with the IP address in the
 *          buffer pointed to by the "host" argument.
 *   serv - return the service name associated with the port number
 *          in the buffer pointed to by the "serv" argument.
 * Return Value:
 *   This function indicates successful completion by a zero return
 *   value; a non-zero return value indicates failure.
 */
int
getnameinfo(const struct sockaddr *sa, socklen_t salen,
	    char *host, size_t hostlen,
	    char *serv, size_t servlen, int flags)
{
	char		*addr;
	size_t		alen, slen;
	in_port_t	port;
	int		errnum;

	/* Verify correctness of buffer lengths */
	if ((hostlen == 0) && (servlen == 0))
		return (EAI_FAIL);
	/* Verify correctness of possible flag settings */
	if ((flags != 0) && (flags & ~AI_MASK))
		return (EAI_BADFLAGS);
	if (sa == NULL)
		return (EAI_ADDRFAMILY);
	switch (sa->sa_family) {
	case AF_INET:
		addr = (char *)&sa2sin(sa)->sin_addr;
		alen = sizeof (struct in_addr);
		slen = sizeof (struct sockaddr_in);
		port = (sa2sin(sa)->sin_port); /* network byte order */
		break;
	case AF_INET6:
		addr = (char *)&sa2sin6(sa)->sin6_addr;
		alen = sizeof (struct in6_addr);
		slen = sizeof (struct sockaddr_in6);
		port = (sa2sin6(sa)->sin6_port); /* network byte order */
		break;
	default:
		return (EAI_FAMILY);
	}
	if (salen != slen)
		return (EAI_FAIL);
	/*
	 * Case 1: if Caller sets hostlen != 0, then
	 * fill in "host" buffer that user passed in
	 * with appropriate text string.
	 */
	if (hostlen != 0) {
		if (flags & NI_NUMERICHOST) {
			/* Caller wants the host's numeric address */
			if (inet_ntop(sa->sa_family, addr,
			    host, hostlen) == NULL)
				return (EAI_SYSTEM);
		} else {
			struct hostent	*hp;

			/* Caller wants the name of host */
			hp = getipnodebyaddr(addr, alen, sa->sa_family,
			    &errnum);
			if (hp != NULL) {
				if (flags & NI_NOFQDN) {
					char *dot;
					/*
					 * Caller doesn't want fully-qualified
					 * name.
					 */
					dot = strchr(hp->h_name, '.');
					if (dot != NULL)
						*dot = '\0';
				}
				if (strlen(hp->h_name) + 1 > hostlen) {
					freehostent(hp);
					return (EAI_MEMORY);
				}
				(void) strcpy(host, hp->h_name);
				freehostent(hp);
			} else {
				/*
				 * Host's name cannot be located in the name
				 * service database. If NI_NAMEREQD is set,
				 * return error; otherwise, return host's
				 * numeric address.
				 */
				if (flags & NI_NAMEREQD) {
					switch (errnum) {
					case HOST_NOT_FOUND:
						return (EAI_NONAME);
					case TRY_AGAIN:
						return (EAI_AGAIN);
					case NO_RECOVERY:
						return (EAI_FAIL);
					case NO_ADDRESS:
						return (EAI_NODATA);
					default:
						return (EAI_SYSTEM);
					}
				}
				if (_inet_ntop_native(sa->sa_family, addr,
				    host, hostlen) == NULL)
					return (EAI_SYSTEM);
			}
		}
	}
	/*
	 * Case 2: if Caller sets servlen != 0, then
	 * fill in "serv" buffer that user passed in
	 * with appropriate text string.
	 */
	if (servlen != 0) {
		char port_buf[10];
		int portlen;

		if (flags & NI_NUMERICSERV) {
			/* Caller wants the textual form of the port number */
			portlen = snprintf(port_buf, sizeof (port_buf), "%hu",
			    ntohs(port));
			if (servlen < portlen + 1)
				return (EAI_MEMORY);
			(void) strcpy(serv, port_buf);
		} else {
			struct servent	*sp;
			/*
			 * Caller wants the name of the service.
			 * If NI_DGRAM is set, get service name for
			 * specified port for udp.
			 */
			sp = getservbyport(port,
				flags & NI_DGRAM ? "udp" : "tcp");
			if (sp != NULL) {
				if (servlen < strlen(sp->s_name) + 1)
					return (EAI_MEMORY);
				(void) strcpy(serv, sp->s_name);
			} else {
				/*
				 * if service is not in the name server's
				 * database, fill buffer with numeric form for
				 * port number.
				 */
				portlen = snprintf(port_buf, sizeof (port_buf),
				    "%hu", ntohs(port));
				if (servlen < portlen + 1)
					return (EAI_MEMORY);
				(void) strcpy(serv, port_buf);
			}
		}
	}
	return (0);
}

/*
 * This is a wrapper function for inet_ntop(). In case the af is AF_INET6
 * and the address pointed by src is a IPv4-mapped IPv6 address, it
 * returns printable IPv4 address, not IPv4-mapped IPv6 address. In other cases
 * it behaves just like inet_ntop().
 */
static const char *
_inet_ntop_native(int af, const void *src, char *dst, size_t size)
{
	struct in_addr src4;
	const char *result;

	if (af == AF_INET6) {
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)src)) {
			IN6_V4MAPPED_TO_INADDR((struct in6_addr *)src, &src4);
			result = inet_ntop(AF_INET, &src4, dst, size);
		} else {
			result = inet_ntop(AF_INET6, src, dst, size);
		}
	} else {
		result = inet_ntop(af, src, dst, size);
	}

	return (result);
}
