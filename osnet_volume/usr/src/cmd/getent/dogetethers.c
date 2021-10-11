#ident	"@(#)dogetethers.c	1.5	99/03/03 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "getent.h"

extern char *ether_ntoa(const struct ether_addr *e);
extern struct ether_addr *ether_aton(const char *s);
extern int ether_ntohost(char *hostname, const struct ether_addr *e);
extern int ether_hostton(const char *hostname, struct ether_addr *e);

static int
putethers(const char *hostname, const struct ether_addr *e, FILE *fp)
{
	if (hostname == NULL || e == NULL)
		return (EXC_SYNTAX);

	if (fprintf(fp, "%-20s %s\n", hostname, ether_ntoa(e)) == EOF)
		return (EXC_SYNTAX); /* for lack of a better error code */
	return (EXC_SUCCESS);
}

/*
 * ether_ntohost/hostton - get entries from ethers database
 */
int
dogetethers(const char **list)
{
	int rc = EXC_SUCCESS;

	if (list == NULL || *list == NULL) {
		rc = EXC_ENUM_NOT_SUPPORTED;
	} else {
		for (; *list != NULL; list++) {
			struct ether_addr ea;
			struct ether_addr *e;
			char hostname[MAXHOSTNAMELEN + 1];
			char *hp;
			int	retval;

			if ((e = ether_aton((char *)*list)) != NULL) {
				hp = hostname;
				retval = ether_ntohost(hp, e);
			} else {
				hp = (char *)*list;
				e = &ea;
				retval = ether_hostton(hp, e);
			}
			if (retval != 0)
				rc = EXC_NAME_NOT_FOUND;
			else
				rc = putethers(hp, e, stdout);
		}
	}

	return (rc);
}
