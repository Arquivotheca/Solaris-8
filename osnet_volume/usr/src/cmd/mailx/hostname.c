/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)hostname.c	1.11	98/08/06 SMI"	/* from SVr4.0 1.3.2.3 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Code to figure out what host we are on.
 */

#include "rcv.h"
#include "configdefs.h"
#include <sys/utsname.h>
#include <locale.h>

#define	MAILCNFG	"/etc/mail/mailcnfg"

char host[64];
char domain[128];
/*
 * Initialize the network name of the current host.
 */
void 
inithost(void)
{
	register struct netmach *np;
	struct utsname name;
	char *fp;

	xsetenv(MAILCNFG);
	if (fp = xgetenv("CLUSTER")) {
		nstrcpy(host, sizeof (host), fp);
	} else {
		uname(&name);
		nstrcpy(host, sizeof (host), name.nodename);
	}
	snprintf(domain, sizeof (domain), "%s%s", host, maildomain());
	for (np = netmach; np->nt_machine != 0; np++)
		if (strcmp(np->nt_machine, EMPTY) == 0)
			break;
	if (np->nt_machine == 0) {
		printf(
		    gettext("Cannot find empty slot for dynamic host entry\n"));
		exit(1);
	}
	np->nt_machine = host;
	np++;
	np->nt_machine = domain;
	if (debug) fprintf(stderr, "host '%s', domain '%s'\n", host, domain);
}
