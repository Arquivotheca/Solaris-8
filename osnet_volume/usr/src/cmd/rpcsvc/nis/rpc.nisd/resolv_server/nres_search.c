/*
 * Copyright (c) 1993,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nres_search.c	1.5	98/03/16 SMI"

/* Taken from 4.1.3 ypserv resolver code. */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "nres.h"
#include "prnt.h"

static void nres_querydomain(char *, char *, char *);
static char *nres_hostalias(char *);

nres_search(struct nres *block)
{
	register char	*cp, *domain;
	int		n;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (-1);

	block->retries = 0;	/* start clock */
	if (block->search_index < 0)
		return (-1);
	/* only try exact match for reverse cases */
	if (block->reverse) {
		(void) nres_querydomain(block->name, (char *)NULL,
							block->search_name);
		block->search_index = -1;
		return (0);
	}

	for (cp = block->name, n = 0; *cp; cp++)
		if (*cp == '.')
			n++;
	/* n indicates the presence of trailing dots */

	if (block->search_index == 0) {
		if (n == 0 && (cp = nres_hostalias(block->name))) {
			(void) strncpy(block->search_name, cp, 2 * MAXDNAME);
			block->search_index = -1; /* if hostalias try 1 name */
			return (0);
		}
	}
	if ((n == 0 || *--cp != '.') && (_res.options & RES_DEFNAMES)) {
		domain = _res.dnsrch[block->search_index];
		if (domain) {
			(void) nres_querydomain(block->name, domain,
							block->search_name);
			block->search_index++;
			return (0);
		}
	}
	if (n) {
		(void) nres_querydomain(block->name, (char *)NULL,
							block->search_name);
		block->search_index = -1;
		return (0);
	}
	block->search_index = -1;
	return (-1);
}

/*
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
static void
nres_querydomain(char *name, char *domain, char *nbuf)
{
	int		n;

	if (domain == NULL) {
		/*
		 * Check for trailing '.'; copy without '.' if present.
		 */
		n = strlen(name) - 1;
		if (name[n] == '.') {
			(void) memcpy(nbuf, name, n);
			nbuf[n] = '\0';
		} else
			(void) strcpy(nbuf, name);
	} else
		(void) sprintf(nbuf, "%.*s.%.*s",
			MAXDNAME, name, MAXDNAME, domain);

	prnt(P_INFO, "nres_querydomain(, %s).\n", nbuf);
}

static char *
nres_hostalias(char *name)
{
	register char  *C1, *C2;
	FILE		*fp;
	char		*file;
	char		buf[BUFSIZ];
	static char	abuf[MAXDNAME];

	file = getenv("HOSTALIASES");
	if (file == NULL || (fp = fopen(file, "r")) == NULL)
		return (NULL);
	buf[sizeof (buf) - 1] = '\0';
	while (fgets(buf, sizeof (buf), fp)) {
		for (C1 = buf; *C1 && !isspace(*C1); ++C1);
		if (!*C1)
			break;
		*C1 = '\0';
		if (!strcasecmp(buf, name)) {
			while (isspace(*++C1));
			if (!*C1)
				break;
			for (C2 = C1 + 1; *C2 && !isspace(*C2); ++C2);
			abuf[sizeof (abuf) - 1] = *C2 = '\0';
			(void) strncpy(abuf, C1, sizeof (abuf) - 1);
			(void) fclose(fp);
			return (abuf);
		}
	}
	(void) fclose(fp);
	return (NULL);
}
