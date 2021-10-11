#ident	"@(#)dogetnet.c	1.7	96/09/26 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
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

/*
 * Print a network number such as 129.144
 */
char *
inet_nettoa(struct in_addr in)
{
	u_long addr = htonl(in.s_addr);
	u_char *up = (u_char *)&addr;
	static char result[256];

	/* Omit leading zeros */
	if (up[0]) {
		(void) sprintf(result, "%d.%d.%d.%d",
		    up[0], up[1], up[2], up[3]);
	} else if (up[1]) {
		(void) sprintf(result, "%d.%d.%d", up[1], up[2], up[3]);
	} else if (up[2]) {
		(void) sprintf(result, "%d.%d", up[2], up[3]);
	} else {
		(void) sprintf(result, "%d", up[3]);
	}
	return (result);
}

static int
putnetent(const struct netent *np, FILE *fp)
{
	char **p;
	int rc = 0;
	struct in_addr in;

	if (np == NULL) {
		return (1);
	}

	in.s_addr = np->n_net;
	if (fprintf(fp, "%-20s %s",
		    np->n_name, inet_nettoa(in)) == EOF)
		rc = 1;
	for (p = np->n_aliases; *p != 0; p++) {
		if (fprintf(fp, " %s", *p) == EOF)
			rc = 1;
	}
	if (putc('\n', fp) == EOF)
		rc = 1;
	return (rc);
}

/*
 * getnetbyname/addr - get entries from network database
 */
int
dogetnet(const char **list)
{
	struct netent *np;
	int rc = EXC_SUCCESS;

	if (list == NULL || *list == NULL) {
		while ((np = getnetent()) != NULL)
			(void) putnetent(np, stdout);
	} else {
		for (; *list != NULL; list++) {
			long addr = inet_network(*list);
			if (addr != -1)
				np = getnetbyaddr(addr, AF_INET);
			else
				np = getnetbyname(*list);
			if (np == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) putnetent(np, stdout);
		}
	}

	return (rc);
}
