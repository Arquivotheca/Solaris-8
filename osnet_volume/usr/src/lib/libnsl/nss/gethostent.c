/*
 * Copyright (c) 1986-1994, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 *
 * lib/libnsl/nss/gethostent.c
 */

#pragma ident	"@(#)gethostent.c	1.20	99/11/26 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <netinet/in.h>
#include <sys/socket.h>

/*
 * Still just a global.  If you want per-thread h_errno,
 * use the reentrant interfaces (gethostbyname_r et al)
 */
int h_errno;

#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Don't free this, even on an endhostent(), because bitter experience shows
 * that there's production code that does getXXXbyYYY(), then endXXXent(),
 * and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define	GETBUF()	\
	NSS_XbyY_ALLOC(&buffer, sizeof (struct hostent), NSS_BUFLEN_HOSTS)
	/* === ?? set ENOMEM on failure?  */

/* Private interfaces for entry point into the NSS Switch */

struct hostent *getipnodebyaddr();

struct hostent *
gethostbyname(nam)
	const char	*nam;
{
	nss_XbyY_buf_t  *b;
	struct hostent  *res = 0;

	trace1(TR_gethostbyname, 0);
	if ((b = GETBUF()) != 0) {
		res = gethostbyname_r(nam,
		    b->result, b->buffer, b->buflen,
		    &h_errno);
	}
	trace1(TR_gethostbyname, 1);
	return (res);
}

struct hostent *
gethostbyaddr(addr, len, type)
	const char	*addr;
	int		len;
	int		type;
{
	nss_XbyY_buf_t	*b;
	struct hostent	*res = 0;
	char *c;

	trace2(TR_gethostbyaddr, 0, len);
	h_errno = 0;
	if (type == AF_INET6)
		return (getipnodebyaddr(addr, len, type, &h_errno));

	if ((b = GETBUF()) != 0) {
		res = gethostbyaddr_r(addr, len, type,
		    b->result, b->buffer, b->buflen,
		    &h_errno);
	}
	trace2(TR_gethostbyaddr, 1, len);
	return (res);
}

struct hostent *
gethostent()
{
	nss_XbyY_buf_t	*b;
	struct hostent	*res = 0;

	trace1(TR_gethostent, 0);
	if ((b = GETBUF()) != 0) {
		res = gethostent_r(b->result, b->buffer, b->buflen, &h_errno);
	}
	trace1(TR_gethostent, 1);
	return (res);
}

/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
__str2hostent(af, instr, lenstr, ent, buffer, buflen)
	int		af;
	const char	*instr;
	int		lenstr;
	void		*ent;
	char		*buffer;
	int		buflen;
{
	struct hostent	*host	= (struct hostent *)ent;
	const char	*p, *addrstart, *limit;
	int		naddr, i;
	int		addrlen, res;
	char		addrbuf[100];  /* Why 100? */
	struct in_addr	*addrp;
	struct in6_addr	*addrp6;
	char		**addrvec;

	trace3(TR_str2hostent, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2hostent, 1, lenstr, buflen);
		return (NSS_STR_PARSE_PARSE);
	}
	if (af != AF_INET && af != AF_INET6) {
		trace3(TR_str2hostent, 1, lenstr, buflen);
		return (NSS_STR_PARSE_ERANGE);
	}

	/*
	 * The DNS-via-YP code returns multiple lines for a key.
	 * Normal YP return values do not contain newlines (nor do
	 * lines from /etc/hosts or other sources)
	 * We count the number of newlines; this should give us
	 * the number of IP addresses specified.
	 * We'll also call the aliases code and instruct it to
	 * stop at the first newline as the remaining lines will
	 * all contain the same hostname/aliases (no aliases, unfortunately).
	 *
	 * When confronted with a string with embedded newlines,
	 * this code will take the hostname/aliases on the first line
	 * and each of the IP addresses at the start of all lines.
	 * Because the NIS protocol limits return values to 1024 bytes,
	 * we still do not get all addresses.  If you want to fix
	 * that problem, do not look here.
	 */

	p = instr;

	/* Strip trailing newlines */
	while (lenstr > 0 && p[lenstr - 1] == '\n')
		lenstr--;

	naddr = 1;
	limit = p + lenstr;

	for (; p < limit && (p = memchr(p, '\n', limit - p)); p++)
		naddr++;

	/* Allocate space for naddr addresses and h_addr_list */

	if (af == AF_INET6) {
		addrp6 = (struct in6_addr *)ROUND_DOWN(buffer + buflen,
		    sizeof (*addrp6));
		addrp6 -= naddr;
		addrvec = (char **)ROUND_DOWN(addrp6, sizeof (*addrvec));
		addrvec -= naddr + 1;
	} else {
		addrp = (struct in_addr *)ROUND_DOWN(buffer + buflen,
		    sizeof (*addrp));
		addrp -= naddr;
		addrvec = (char **)ROUND_DOWN(addrp, sizeof (*addrvec));
		addrvec -= naddr + 1;
	}

	if ((char *)addrvec < buffer) {
		trace3(TR_str2hostent, 1, lenstr, buflen);
		return (NSS_STR_PARSE_ERANGE);
	}

	/* For each addr, parse and get it */

	p = instr;

	for (i = 0; i < naddr; i ++) {

		limit = memchr(p, '\n', lenstr - (p - instr));
		if (limit == NULL)
			limit = instr + lenstr;

		while (p < limit && isspace(*p)) {
			p++;
		}
		addrstart = p;
		while (p < limit && !isspace(*p)) {
			p++;
		}
		if (p >= limit) {
		    /* Syntax error */
		    trace3(TR_str2hostent, 1, lenstr, buflen);
		    return (NSS_STR_PARSE_ERANGE);
		}
		addrlen = p - addrstart;
		if (addrlen >= sizeof (addrbuf)) {
			/* Syntax error -- supposed IP address is too long */
			trace3(TR_str2hostent, 1, lenstr, buflen);
			return (NSS_STR_PARSE_ERANGE);
		}
		memcpy(addrbuf, addrstart, addrlen);
		addrbuf[addrlen] = '\0';

		if (addrlen > ((af == AF_INET6) ? INET6_ADDRSTRLEN
							: INET_ADDRSTRLEN)) {
			/* Syntax error -- supposed IP address is too long */
			trace3(TR_str2hostent, 4, lenstr, buflen);
			return (NSS_STR_PARSE_ERANGE);
		}
		if (af == AF_INET) {
			/*
			 * inet_pton() doesn't handle d.d.d, d.d, or d formats,
			 * so we must use inet_addr() for IPv4 addresses.
			 */
			addrvec[i] = (char *)&addrp[i];
			if ((addrp[i].s_addr = inet_addr(addrbuf)) == -1) {
				/* Syntax error -- bogus IPv4 address */
				trace3(TR_str2hostent, 4, lenstr, buflen);
				return (NSS_STR_PARSE_ERANGE);
			}
		} else {
			/*
			 * In the case of AF_INET6, we can have both v4 and v6
			 * addresses, so we convert v4's to v4 mapped addresses
			 * and return them as such.
			 */
			addrvec[i] = (char *)&addrp6[i];
			if (strchr(addrbuf, ':') != 0) {
				if (inet_pton(af, addrbuf, &addrp6[i]) != 1) {
					trace3(TR_str2hostent, 4, lenstr,
					    buflen);
					return (NSS_STR_PARSE_ERANGE);
				}
			} else {
				struct in_addr in4;
				if ((in4.s_addr = inet_addr(addrbuf)) == -1) {
					trace3(TR_str2hostent, 4, lenstr,
					    buflen);
					return (NSS_STR_PARSE_ERANGE);
				}
				IN6_INADDR_TO_V4MAPPED(&in4, &addrp6[i]);
			}
		}

		/* First address, this is where we get the hostname + aliases */
		if (i == 0) {
			while (p < limit && isspace(*p)) {
				p++;
			}
			host->h_aliases = _nss_netdb_aliases(p, limit - p,
				buffer, ((char *)addrvec) - buffer);
		}
		if (limit >= instr + lenstr)
			break;
		else
			p = limit + 1;		/* skip NL */
	}

	if (host->h_aliases == 0) {
		/* could be parsing error as well */
		res = NSS_STR_PARSE_ERANGE;
	} else {
		/* Success */
		host->h_name = host->h_aliases[0];
		host->h_aliases++;
		res = NSS_STR_PARSE_SUCCESS;
	}
	/*
	 * If i < naddr, we quit the loop early and addrvec[i+1] needs NULL
	 * otherwise, we ran naddr iterations and addrvec[naddr] needs NULL
	 */
	addrvec[i >= naddr ? naddr : i + 1] = 0;
	if (af == AF_INET6) {
		host->h_length    = sizeof (struct in6_addr);
	} else {
		host->h_length    = sizeof (struct in_addr);
	}
	host->h_addrtype  = af;
	host->h_addr_list = addrvec;

	trace3(TR_str2hostent, 1, lenstr, buflen);
	return (res);
}

#endif	NSS_INCLUDE_UNSAFE
