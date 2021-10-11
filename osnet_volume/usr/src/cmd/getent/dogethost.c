#ident	"@(#)dogethost.c	1.4	94/04/27 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
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

	if (hp == NULL) {
		return (1);
	}

	for (p = hp->h_addr_list; *p != 0; p++) {
		struct in_addr in;
		char **q;

		(void) memcpy((char *)&in.s_addr, *p, sizeof (in));
		if (fprintf(fp, "%s\t%s",
			inet_ntoa(in), hp->h_name) == EOF)
			rc = 1;
		for (q = hp->h_aliases; *q != 0; q++) {
			if (fprintf(fp, " %s", *q) == EOF)
				rc = 1;
		}
		if (putc('\n', fp) == EOF)
			rc = 1;
	}
	return (rc);
}

/*
 * gethostbyname/addr - get entries from hosts database
 */
int
dogethost(const char **list)
{
	struct hostent *hp;
	int rc = EXC_SUCCESS;

	if (list == NULL || *list == NULL) {
		while ((hp = gethostent()) != NULL)
			(void) puthostent(hp, stdout);
	} else {
		for (; *list != NULL; list++) {
			long addr = inet_addr(*list);
			if (addr != -1)
				hp = gethostbyaddr((char *)&addr,
					sizeof (addr), AF_INET);
			else
				hp = gethostbyname(*list);
			if (hp == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) puthostent(hp, stdout);
		}
	}

	return (rc);
}
