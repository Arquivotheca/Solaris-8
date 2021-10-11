/*
 * Copyright (c) 1986-1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 * In addition, portions of such source code were derived from Berkeley
 * 4.4 BSD under license from the Regents of the University of California
 */

#ident	"@(#)inet_ntoa.c	1.14	98/02/12 SMI"

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 *
 * Reentrant interface
 */

#include "rpc_mt.h"
#include <errno.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>


char *
inet_ntoa_r(in, b)
	struct in_addr in;
	char	b[];	/* Assumed >= 18 bytes */
{
	char	*p;

	p = (char *)&in;
#define	UC(b)	(((int)b)&0xff)
	sprintf(b, "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (b);
}

char *
inet_ntoa(in)
	struct in_addr in;
{
	register char *p;
	char *b = (char *)NULL;
	static char b_main[18];
	static thread_key_t ntoa_key;
	extern mutex_t tsd_lock;

	trace1(TR_inet_ntoa, 0);
	if (_thr_main())
		b = b_main;
	else {
		if (ntoa_key == 0) {
			mutex_lock(&tsd_lock);
			if (ntoa_key == 0)
				thr_keycreate(&ntoa_key, free);
			mutex_unlock(&tsd_lock);
		}
		thr_getspecific(ntoa_key, (void **) &b);
		if (b == (char *)NULL) {
			b = (char *)malloc(18);
			if (b == (char *)NULL) {
				trace1(TR_inet_ntoa, 1);
				return ((char *)NULL);
			}
			thr_setspecific(ntoa_key, (void *)b);
		}
	}
	p = (char *)&in;
#define	UC(b)	(((int)b)&0xff)
	sprintf(b, "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	trace1(TR_inet_ntoa, 1);
	return (b);
}

/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */
in_addr_t
inet_addr(cp)
	register char *cp;
{
	register uint32_t val, base, n;
	register char c;
	uint32_t parts[4], *pp = parts;

	trace1(TR_inet_addr, 0);
	if (*cp == '\0')
		return ((uint32_t)-1); /* disallow null string in cp */
again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0') {
		if (*++cp == 'x' || *cp == 'X')
			base = 16, cp++;
		else
			base = 8;
	}
	while ((c = *cp) != NULL) {
		if (isdigit(c)) {
			if ((c - '0') >= base)
			    break;
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if ((pp >= parts + 3) || (val > 0xff)) {
			trace1(TR_inet_addr, 1);
			return ((uint32_t)-1);
		}
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp)) {
		trace1(TR_inet_addr, 1);
		return ((uint32_t)-1);
	}
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		if (parts[1] > 0xffffff)
		    return ((uint32_t)-1);
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		if (parts[2] > 0xffff)
		    return ((uint32_t)-1);
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		if (parts[3] > 0xff)
		    return ((uint32_t)-1);
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		    ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		trace1(TR_inet_addr, 1);
		return ((uint32_t)-1);
	}
	val = htonl(val);
	trace1(TR_inet_addr, 1);
	return (val);
}


/*
 * Return the network number from an internet
 * address; handles class a/b/c network #'s.
 */
inet_netof(in)
	struct in_addr in;
{
	register uint32_t i = ntohl(in.s_addr);

	trace1(TR_inet_netof, 0);
	if (IN_CLASSA(i)) {
		trace1(TR_inet_netof, 1);
		return (((i)&IN_CLASSA_NET) >> IN_CLASSA_NSHIFT);
	} else if (IN_CLASSB(i)) {
		trace1(TR_inet_netof, 1);
		return (((i)&IN_CLASSB_NET) >> IN_CLASSB_NSHIFT);
	} else {
		trace1(TR_inet_netof, 1);
		return (((i)&IN_CLASSC_NET) >> IN_CLASSC_NSHIFT);
	}
}
