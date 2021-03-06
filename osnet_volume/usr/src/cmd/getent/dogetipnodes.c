#ident	"@(#)dogetipnodes.c	1.3	99/11/16 SMI"

/*
 * Copyright (c) 1994-1999, by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include "getent.h"

static int
puthostent(const struct hostent *hp, FILE *fp)
{
	char **p;
	int rc = 0;
	char obuf[INET6_ADDRSTRLEN];

	if (hp == NULL) {
		return (1);
	}

	for (p = hp->h_addr_list; *p != 0; p++) {
		void		*addr;
		struct in_addr	in4;
		int		af;
		const char	*res;
		char **q;

		if (hp->h_addrtype == AF_INET6) {
			if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)*p)) {
				IN6_V4MAPPED_TO_INADDR((struct in6_addr *)*p,
							&in4);
				af = AF_INET;
				addr = &in4;
			} else {
				af = AF_INET6;
				addr = *p;
			}
		} else {
			af = AF_INET;
			addr = *p;
		}
		res = inet_ntop(af, addr, obuf, sizeof (obuf));
		if (res == 0) {
			rc = 1;
			continue;
		}
		if (fprintf(fp, "%s\t%s", res, hp->h_name) == EOF)
			rc = 1;
		for (q = hp->h_aliases; q && *q; q++) {
			if (fprintf(fp, " %s", *q) == EOF)
				rc = 1;
		}
		if (putc('\n', fp) == EOF)
			rc = 1;
	}
	return (rc);
}

/*
 * getipnodebyname/addr - get entries from ipnodes database
 */
int
dogetipnodes(const char **list)
{
	struct hostent *hp;
	int rc = EXC_SUCCESS;
	struct in6_addr in6;
	struct in_addr	in4;
	int		af, len;
	void		*addr;
	int err_ret;

	if (list == NULL || *list == NULL) {
		(void) fprintf(stdout,
				"Enumeration not supported on ipnodes\n");
	} else {
		for (; *list != NULL; list++) {
			if (strchr(*list, ':') != 0) {
				af = AF_INET6;
				len = sizeof (in6);
				addr = &in6;
			} else {
				af = AF_INET;
				len = sizeof (in4);
				addr = &in4;
			}
			if (inet_pton(af, *list, addr) == 1)
				hp = getipnodebyaddr(addr, len, af, &err_ret);
			else
				hp = getipnodebyname(
						*list,
						AF_INET6,
						AI_V4MAPPED|AI_ALL,
						&err_ret);
			if (hp == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) puthostent(hp, stdout);
		}
	}

	return (rc);
}
