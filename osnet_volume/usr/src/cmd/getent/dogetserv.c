#ident	"@(#)dogetserv.c	1.4	94/01/26 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <netdb.h>
#include "getent.h"

static int
putservent(const struct servent *sp, FILE *fp)
{
	char **p;
	int rc = 0;

	if (sp == NULL) {
		return (1);
	}

	if (fprintf(fp, "%-20s %d/%s",
		    sp->s_name, ntohs(sp->s_port), sp->s_proto) == EOF)
		rc = 1;
	for (p = sp->s_aliases; *p != 0; p++) {
		if (fprintf(fp, " %s", *p) == EOF)
			rc = 1;
	}
	if (putc('\n', fp) == EOF)
		rc = 1;
	return (rc);
}

/*
 * getservbyname/addr - get entries from service database
 * Accepts arguments as:
 *	port/protocol
 *	port
 *	name/protocol
 *	name
 */
int
dogetserv(const char **list)
{
	struct servent *sp;
	int rc = EXC_SUCCESS;

	if (list == NULL || *list == NULL) {
		while ((sp = getservent()) != NULL)
			(void) putservent(sp, stdout);
	} else {
		for (; *list != NULL; list++) {
			int port;
			char key[BUFSIZ];
			const char *protocol = NULL;
			char *cp;

			/* Copy string to avoiding modifying the argument */
			(void) strncpy(key, *list, sizeof (key));
			key[sizeof (key) - 1] = NULL;
			/* Split at a '/' to extract protocol number */
			if ((cp = strchr(key, '/')) != NULL) {
				*cp = NULL;
				protocol = cp + 1;
			}
			port = htons(atoi(key));
			if (port != 0)
				sp = getservbyport(port, protocol);
			else
				sp = getservbyname(key, protocol);
			if (sp == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) putservent(sp, stdout);
		}
	}

	return (rc);
}
