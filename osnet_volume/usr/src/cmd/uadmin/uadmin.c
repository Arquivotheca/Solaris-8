/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uadmin.c	1.14	99/08/25 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/uadmin.h>
#include <bsm/libbsm.h>

static char *Usage = "Usage: %s cmd fcn [mdep]\n";

extern int audit_uadmin_setup(int, char **);
extern int audit_uadmin_success();

int
main(int argc, char *argv[])
{
	int cmd, fcn;
	uintptr_t mdep = NULL;
	sigset_t set;

	if (argc < 3 || argc > 4) {
		(void) fprintf(stderr, Usage, argv[0]);
		return (1);
	}

	(void) audit_uadmin_setup(argc, argv);

	(void) sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, NULL);

	cmd = atoi(argv[1]);
	fcn = atoi(argv[2]);
	if (argc == 4) {	/* mdep argument given */
		if (cmd != A_REBOOT && cmd != A_SHUTDOWN && cmd != A_DUMP) {
			(void) fprintf(stderr, "%s: mdep argument not "
			    "allowed for this cmd value\n", argv[0]);
			(void) fprintf(stderr, Usage, argv[0]);
			return (1);
		} else {
			mdep = (uintptr_t)argv[3];
		}
	}

	if (geteuid() == 0 && audit_uadmin_success() == -1)
		(void) fprintf(stderr, "%s: can't turn off auditd\n", argv[0]);

	if (uadmin(cmd, fcn, mdep) < 0) {
		perror("uadmin");
		return (1);
	}

	return (0);
}
