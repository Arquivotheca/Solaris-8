
/*
 * Copyright (c) 1986-1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getaddrinfo.c	1.5	99/12/06 SMI"


#include <netdb.h>
#include <arpa/inet.h>
#include <nss_dbdefs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>

extern char *_dgettext(const char *, const char *);

#define	ai2sin(x)	((struct sockaddr_in *)((x)->ai_addr))
#define	ai2sin6(x)	((struct sockaddr_in6 *)((x)->ai_addr))

#define	HOST_BROADCAST	"255.255.255.255"

/*
 * Storage allocation for global variables in6addr_any and
 * in6addr_loopback.  The extern declarations for these
 * variables are defined in <netinet/in.h>.  These two
 * variables could have been defined in any of the "C" files
 * in libsocket. They are defined here with other IPv6
 * related interfaces.
 */
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

/* AI_MASK:  all valid flags for addrinfo */
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST \
	| AI_ADDRCONFIG)
#define	ANY		0
/* function prototypes for used by getaddrinfo() routine */
static int get_addr(int family, const char *hostname, struct addrinfo *aip,
	struct addrinfo *cur, ushort_t port);
static boolean_t str_isnumber(const char *p);

/*
 * getaddrinfo:
 *
 * Purpose:
 *   Routine for performing Address-to-nodename in a
 *   protocol-independent fashion.
 * Description and history of the routine:
 *   Nodename-to-address translation is done in a protocol-
 *   independent fashion using the getaddrinfo() function
 *   that is taken from the IEEE POSIX 1003.1g.
 *
 *   The official specification for this function will be the
 *   final POSIX standard, with the following additional
 *   requirements:
 *
 *   - getaddrinfo() must be thread safe
 *   - The AI_NUMERICHOST is new.
 *   - All fields in socket address structures returned by
 *
 *  getaddrinfo() that are not filled in through an explicit
 *  argument (e.g., sin6_flowinfo and sin_zero) must be set to 0.
 *  (This makes it easier to compare socket address structures).
 *
 * Input Parameters:
 *  nodename  - pointer to null-terminated strings that represents
 *              a hostname or literal ip address (IPv4/IPv6) or this
 *              pointer can be NULL.
 *  servname  - pointer to null-terminated strings that represents
 *              a servicename or literal port number or this
 *              pointer can be NULL.
 *  hints     - optional argument that points to an addrinfo structure
 *              to provide hints on the type of socket that the caller
 *              supports.
 *   Possible setting of the ai_flags member of the hints structure:
 *   AI_PASSIVE -     If set, the caller plans to use the returned socket
 *                    address in a call to bind().  In this case, it the
 *                    nodename argument is NULL, then the IP address portion
 *                    of the socket address structure will be set to
 *                    INADDR_ANY for IPv4 or IN6ADDR_ANY_INIT for IPv6.
 *   AI_PASSIVE -     If not set, then the returned socket address will be
 *                    ready for a call to connect() (for conn-oriented) or
 *                    connect(), sendto(), or sendmsg() (for connectionless).
 *                    In this case, if nodename is NULL, then the IP address
 *                    portion of the socket address structure will be set to
 *                    the loopback address.
 *   AI_CANONNAME -   If set, then upon successful return the ai_canonname
 *                    field of the first addrinfo structure in the linked
 *                    list will point to a NULL-terminated string
 *                    containing the canonical name of the specified nodename.
 *   AI_NUMERICHOST - If set, then a non-NULL nodename string must be a numeric
 *                    host address string.  Otherwise an error of EAI_NONAME
 *                    is returned.  This flag prevents any type of name
 *                    resolution service from being called.
 * Output Parameters:
 *  res       - upon successful return a pointer to a linked list of one
 *              or more addrinfo structures is returned through this
 *              argument.  The caller can process each addrinfo structures
 *              in this list by following the ai_next pointer, until a
 *              NULL pointer is encountered.  In each returned addrinfo
 *              structure the three members ai_family, ai_socktype, and
 *              ai_protocol are corresponding arguments for a call to the
 *              socket() function.  In each addrinfo structure the ai_addr
 *              field points to filled-in socket address structure whose
 *              length is specified by the ai_addrlen member.
 *
 * Return Value:
 *  This function returns 0 upon success or a nonzero error code.  The
 *  following names are nonzero error codes from getaddrinfo(), and are
 *  defined in <netdb.h>.
 *  EAI_ADDRFAMILY - address family not supported
 *  EAI_AGAIN      - DNS temporary failure
 *  EAI_BADFLAGS   - invalid ai_flags
 *  EAI_FAIL       - DNS non-recoverable failure
 *  EAI_FAMILY     - ai_family not supported
 *  EAI_MEMORY     - memory allocation failure
 *  EAI_NODATA     - no address associated with nodename
 *  EAI_NONAME     - host/servname not known
 *  EAI_SERVICE    - servname not supported for ai_socktype
 *  EAI_SOCKTYPE   - ai_socktype not supported
 *  EAI_SYSTEM     - system error in errno
 *
 * Memory Allocation:
 *  All of the information returned by getaddrinfo() is dynamically
 *  allocated:  the addrinfo structures, and the socket address
 *  structures and canonical node name strings pointed to by the
 *  addrinfo structures.
 */


int
getaddrinfo(const char *hostname, const char *servname,
	const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo *cur;
	struct addrinfo *aip;
	struct addrinfo ai;
	int		error;
	ushort_t	port;

	cur = &ai;
	aip = &ai;

	aip->ai_flags = 0;
	aip->ai_family = PF_UNSPEC;
	aip->ai_socktype = 0;
	aip->ai_protocol = 0;
	aip->ai_addrlen = 0;
	aip->ai_canonname = NULL;
	aip->ai_addr = NULL;
	aip->ai_next = NULL;
	port = 0;

	/* if nodename nor servname provided */
	if (hostname == NULL && servname == NULL) {
		*res = NULL;
		return (EAI_NONAME);
	}
	if (hints != NULL) {
		/* check for bad flags in hints */
		if ((hints->ai_flags != 0) && (hints->ai_flags & ~AI_MASK)) {
			*res = NULL;
			return (EAI_BADFLAGS);
		}
		if (hints->ai_family != PF_UNSPEC &&
		    hints->ai_family != PF_INET &&
		    hints->ai_family != PF_INET6) {
			*res = NULL;
			return (EAI_FAMILY);
		}

		(void) memcpy(aip, hints, sizeof (*aip));
		switch (aip->ai_socktype) {
		case ANY:
			switch (aip->ai_protocol) {
			case ANY:
				break;
			case IPPROTO_UDP:
				aip->ai_socktype = SOCK_DGRAM;
				break;
			case IPPROTO_TCP:
				aip->ai_socktype = SOCK_STREAM;
				break;
			default:
				aip->ai_socktype = SOCK_RAW;
				break;
			}
			break;
		case SOCK_RAW:
			break;
		case SOCK_DGRAM:
			aip->ai_protocol = IPPROTO_UDP;
			break;
		case SOCK_STREAM:
			aip->ai_protocol = IPPROTO_TCP;
			break;
		default:
			*res = NULL;
			return (EAI_SOCKTYPE);
		}
	}

	/*
	 *  Get the service.
	 */

	if (servname != NULL) {
		struct servent *sp;
		char *proto = NULL;

		switch (aip->ai_socktype) {
		case ANY:
			proto = NULL;
			break;
		case SOCK_DGRAM:
			proto = "udp";
			break;
		case SOCK_STREAM:
			proto = "tcp";
			break;
		}
		/*
		 * Servname string can be a decimal port number.
		 * If we already know the socket type there is no need
		 * to call getservbyport.
		 */
		if (str_isnumber(servname)) {
			port = htons(atoi(servname));
			if (aip->ai_socktype == ANY &&
			    (sp = getservbyport(port, proto)) == NULL) {
				*res = NULL;
				return (EAI_SERVICE);
			}
		} else {
			if ((sp = getservbyname(servname, proto)) == NULL) {
				*res = NULL;
				return (EAI_SERVICE);
			}
			port = sp->s_port;
		}
		if (aip->ai_socktype == ANY) {
			if (strcmp(sp->s_proto, "udp") == 0) {
				aip->ai_socktype = SOCK_DGRAM;
				aip->ai_protocol = IPPROTO_UDP;
			} else if (strcmp(sp->s_proto, "tcp") == 0) {
				aip->ai_socktype = SOCK_STREAM;
				aip->ai_protocol = IPPROTO_TCP;
			} else {
				*res = NULL;
				errno = EPROTONOSUPPORT;
				return (EAI_SYSTEM);
			}
		}
	}

	/*
	 * hostname is NULL
	 * case 1: AI_PASSIVE bit set : anyaddr 0.0.0.0 or ::
	 * case 2: AI_PASSIVE bit not set : localhost 127.0.0.1 or ::1
	 */

	if (hostname == NULL) {
		struct addrinfo *nai;
		int addrlen;
		char *canonname;

		if (aip->ai_family == PF_INET)
			goto v4only;
		/* create IPv6 addrinfo */
		nai = malloc(sizeof (struct addrinfo));
		if (nai == NULL)
			goto nomem;
		*nai = *aip;
		addrlen = sizeof (struct sockaddr_in6);
		nai->ai_addr = malloc(addrlen);
		if (nai->ai_addr == NULL) {
			freeaddrinfo(nai);
			goto nomem;
		}
		bzero(nai->ai_addr, addrlen);
		nai->ai_addrlen = addrlen;
		nai->ai_family = PF_INET6;
		nai->ai_protocol = 0;
		nai->ai_canonname = NULL;
		if (nai->ai_flags & AI_PASSIVE) {
			ai2sin6(nai)->sin6_addr = in6addr_any;
		} else {
			ai2sin6(nai)->sin6_addr = in6addr_loopback;
			if (nai->ai_flags & AI_CANONNAME) {
				canonname = strdup("loopback");
				if (canonname == NULL) {
					freeaddrinfo(nai);
					goto nomem;
				}
				nai->ai_canonname = canonname;
			}
		}
		ai2sin6(nai)->sin6_family = PF_INET6;
		ai2sin6(nai)->sin6_port = port;
		cur->ai_next = nai;
		cur = nai;
		if (aip->ai_family == PF_INET6) {
			cur->ai_next = NULL;
			goto success;
		}
		/* If address family is PF_UNSPEC or PF_INET */
v4only:
		/* create IPv4 addrinfo */
		nai = malloc(sizeof (struct addrinfo));
		if (nai == NULL)
			goto nomem;
		*nai = *aip;
		addrlen = sizeof (struct sockaddr_in);
		nai->ai_addr = malloc(addrlen);
		if (nai->ai_addr == NULL) {
			freeaddrinfo(nai);
			goto nomem;
		}
		bzero(nai->ai_addr, addrlen);
		nai->ai_addrlen = addrlen;
		nai->ai_family = PF_INET;
		nai->ai_protocol = 0;
		nai->ai_canonname = NULL;
		if (nai->ai_flags & AI_PASSIVE) {
			ai2sin(nai)->sin_addr.s_addr = INADDR_ANY;
		} else {
			ai2sin(nai)->sin_addr.s_addr = INADDR_LOOPBACK;
			if (nai->ai_flags & AI_CANONNAME &&
			    nai->ai_family != PF_UNSPEC) {
				canonname = strdup("loopback");
				if (canonname == NULL) {
					freeaddrinfo(nai);
					goto nomem;
				}
				nai->ai_canonname = canonname;
			}
		}
		ai2sin(nai)->sin_family = PF_INET;
		ai2sin(nai)->sin_port = port;
		cur->ai_next = nai;
		cur = nai;
		cur->ai_next = NULL;
		goto success;
	}

	/* hostname string is a literal address or an alphabetical name */
	error = get_addr(aip->ai_family, hostname, aip, cur, port);
	if (error) {
		*res = NULL;
		return (error);
	}

success:
	*res = aip->ai_next;
	return (0);

nomem:
	return (EAI_MEMORY);
}

static int
get_addr(int family, const char *hostname, struct addrinfo *aip, struct
	addrinfo *cur, ushort_t port)
{
	struct hostent		*hp;
	char 			*hostp;
	char			_hostname[MAXHOSTNAMELEN];
	int			i, errnum;
	struct addrinfo		*nai;
	int			addrlen;
	char			*canonname;
	boolean_t		firsttime = B_TRUE;
	boolean_t		create_v6_addrinfo;
	struct in_addr		v4addr;
	struct in6_addr		v6addr;
	struct in6_addr		*v6addrp;
	int			flags;

	hostp = (char *)hostname;
	/* Check to see if AI_NUMERICHOST bit is set */
	if (aip->ai_flags & AI_NUMERICHOST) {
		if (inet_addr(hostname) != -1 ||
		    strcmp(hostname, HOST_BROADCAST) == 0) {
			/* this is a literal IPv4 address */
			strncpy(_hostname, hostname, sizeof (_hostname));
		} else if (inet_pton(AF_INET6, hostname, &v6addr) > 0) {
				strncpy(_hostname, hostname,
				    sizeof (_hostname));
			} else {
				return (EAI_NONAME);
			}
		_hostname[sizeof (_hostname) -1] = '\0';
		hostp = _hostname;
	}

	flags = aip->ai_flags & AI_ADDRCONFIG;
	/* if hostname argument is literal, name service doesn't get called */
	if (family == PF_UNSPEC) {
		hp = getipnodebyname(hostp, AF_INET6, AI_ALL |
		    flags | AI_V4MAPPED, &errnum);
	} else {
		hp = getipnodebyname(hostp, family, flags, &errnum);
	}

	if (hp == NULL) {
		switch (errnum) {
		case HOST_NOT_FOUND:
			if (family == PF_UNSPEC)
				return (EAI_NONAME);
			else
				return (EAI_ADDRFAMILY);
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

	for (i = 0; hp->h_addr_list[i]; i++) {
		/* Determine if an IPv6 addrinfo structure should be created */
		create_v6_addrinfo = B_TRUE;
		if (hp->h_addrtype == AF_INET6) {
			v6addrp = (struct in6_addr *)hp->h_addr_list[i];
			if (IN6_IS_ADDR_V4MAPPED(v6addrp)) {
				create_v6_addrinfo = B_FALSE;
				IN6_V4MAPPED_TO_INADDR(v6addrp, &v4addr);
			}
		} else	if (hp->h_addrtype == AF_INET) {
			create_v6_addrinfo = B_FALSE;
			(void) memcpy(&v4addr, hp->h_addr_list[i],
			    sizeof (struct in_addr));
			} else {
				return (EAI_SYSTEM);
			}

		if (create_v6_addrinfo) {
			/* create IPv6 addrinfo */
			nai = malloc(sizeof (struct addrinfo));
			if (nai == NULL)
				goto nomem;
			*nai = *aip;
			addrlen = sizeof (struct sockaddr_in6);
			nai->ai_addr = malloc(addrlen);
			if (nai->ai_addr == NULL) {
				freeaddrinfo(nai);
				goto nomem;
			}
			bzero(nai->ai_addr, addrlen);
			nai->ai_addrlen = addrlen;
			nai->ai_family = PF_INET6;
			nai->ai_protocol = 0;

			(void) memcpy(ai2sin6(nai)->sin6_addr.s6_addr,
			    hp->h_addr_list[i], sizeof (struct in6_addr));
			nai->ai_canonname = NULL;
			if ((nai->ai_flags & AI_CANONNAME) && firsttime) {
				canonname = strdup(hp->h_name);
				if (canonname == NULL) {
					freeaddrinfo(nai);
					goto nomem;
				}
				nai->ai_canonname = canonname;
				firsttime = B_FALSE;
			}
			ai2sin6(nai)->sin6_family = PF_INET6;
			ai2sin6(nai)->sin6_port = port;
		} else {
			/* create IPv4 addrinfo */
			nai = malloc(sizeof (struct addrinfo));
			if (nai == NULL)
				goto nomem;
			*nai = *aip;
			addrlen = sizeof (struct sockaddr_in);
			nai->ai_addr = malloc(addrlen);
			if (nai->ai_addr == NULL) {
				freeaddrinfo(nai);
				goto nomem;
			}
			bzero(nai->ai_addr, addrlen);
			nai->ai_addrlen = addrlen;
			nai->ai_family = PF_INET;
			nai->ai_protocol = 0;
			(void) memcpy(&(ai2sin(nai)->sin_addr.s_addr),
			    &v4addr, sizeof (struct in_addr));
			nai->ai_canonname = NULL;
			if (nai->ai_flags & AI_CANONNAME && firsttime) {
				canonname = strdup(hp->h_name);
				if (canonname == NULL) {
					freeaddrinfo(nai);
					goto nomem;
				}
				nai->ai_canonname = canonname;
				firsttime = B_FALSE;
			}
			ai2sin(nai)->sin_family = PF_INET;
			ai2sin(nai)->sin_port = port;
		}

		cur->ai_next = nai;
		cur = nai;
	}
	cur->ai_next = NULL;
	freehostent(hp);
	return (0);

nomem:
	freehostent(hp);
	return (EAI_MEMORY);

}

void
freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		free(ai);
		ai = next;
	} while (ai != NULL);
}

static boolean_t
str_isnumber(const char *p)
{
	char *q = (char *)p;
	while (*q) {
		if (!isdigit(*q))
			return (B_FALSE);
		q++;
	}
	return (B_TRUE);
}
static const char *gai_errlist[] = {
	"Name Translation Error 0 (no error)",
	"address family not supported",		/* 1 EAI_ADDRFAMILY */
	"DNS temporary failure",		/* 2 EAI_AGAIN */
	"invalid ai_flags",			/* 3 EAI_BADFLAGS */
	"DNS non-recoverable failure",		/* 4 EAI_FAIL */
	"ai_family not supported",		/* 5 EAI_FAMILY */
	"memory allocation failure",		/* 6 EAI_MEMORY */
	"no address",				/* 7 EAI_NODATA */
	"host/servname not known",		/* 8 EAI_NONAME */
	"servname not for ai_socktype",		/* 9 EAI_SERVICE */
	"ai_socktype not supported",		/* 10 EAI_SOCKTYPE */
	"system error",				/* 11 EAI_SYSTEM */
};
static int gai_nerr = { sizeof (gai_errlist)/sizeof (gai_errlist[0]) };

char *
gai_strerror(int ecode)
{
	if (ecode < 0)
		return (_dgettext(TEXT_DOMAIN,
		    "Name Translation internal error"));
	else if (ecode < gai_nerr)
		return (_dgettext(TEXT_DOMAIN, gai_errlist[ecode]));
	return (_dgettext(TEXT_DOMAIN, "Unknown name translation error"));
}
