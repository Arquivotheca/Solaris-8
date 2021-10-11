#ident	"@(#)dogetnetmask.c	1.4	96/09/26 SMI"

/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "getent.h"

extern char *inet_nettoa(struct in_addr in);

static int
putnetmask(const struct in_addr key, const struct in_addr netmask, FILE *fp)
{
	char **p;
	int rc = 0;
	struct in_addr net;

	net.s_addr = ntohl(key.s_addr);
	if (fprintf(fp, "%-20s", inet_nettoa(net)) == EOF)
		rc = 1;
	if (fprintf(fp, " %s", inet_ntoa(netmask)) == EOF)
		rc = 1;
	if (putc('\n', fp) == EOF)
		rc = 1;
	return (rc);
}

/*
 * getnetmaskbyaddr - get entries from network database
 */
int
dogetnetmask(const char **list)
{
	int rc = EXC_SUCCESS;
	struct in_addr addr, netmask;

	if (list == NULL || *list == NULL)
		return (EXC_ENUM_NOT_SUPPORTED);

	for (; *list != NULL; list++) {
		addr.s_addr = htonl(inet_network(*list));
		if (addr.s_addr != -1) {
			if (getnetmaskbyaddr(addr, &netmask) == 0) {
				(void) putnetmask(addr, netmask, stdout);
			} else {
				rc = EXC_NAME_NOT_FOUND;
			}
		} else {
			rc = EXC_NAME_NOT_FOUND;
		}
	}

	return (rc);
}
