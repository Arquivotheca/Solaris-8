/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getent.c	1.9	99/03/21 SMI"

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include "getent.h"

static const char *cmdname;

struct table {
	char	*name;			/* name of the table */
	int	(*func)(const char **);	/* function to do the lookup */
};

static struct table t[] = {
	{ "passwd",	dogetpw },
	{ "group",	dogetgr },
	{ "hosts",	dogethost },
	{ "ipnodes",	dogetipnodes },
	{ "services",	dogetserv },
	{ "protocols",	dogetproto },
	{ "ethers",	dogetethers },
	{ "networks",	dogetnet },
	{ "netmasks",	dogetnetmask },
	{ NULL,		NULL }
};

static	void usage(void);

main(int argc, const char **argv)
{
	struct table *p;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif

	(void) textdomain(TEXT_DOMAIN);

	cmdname = argv[0];

	if (argc < 2)
		usage();

	for (p = t; p->name != NULL; p++) {
		if (strcmp(argv[1], p->name) == 0) {
			int rc;

			rc = (*p->func)(&argv[2]);
			switch (rc) {
			case EXC_SYNTAX:
				(void) fprintf(stderr,
					gettext("Syntax error\n"));
				break;
			case EXC_ENUM_NOT_SUPPORTED:
				(void) fprintf(stderr,
	gettext("Enumeration not supported on %s\n"), argv[1]);
				break;
			case EXC_NAME_NOT_FOUND:
				break;
			}
			exit(rc);
		}
	}
	(void) fprintf(stderr, gettext("Unknown database: %s\n"), argv[1]);
	usage();
	/* NOTREACHED */
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    gettext("usage: %s database [ key ... ]\n"), cmdname);
	exit(EXC_SYNTAX);
}
