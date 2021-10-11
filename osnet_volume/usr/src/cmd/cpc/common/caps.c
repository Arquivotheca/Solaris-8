/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)caps.c	1.1	99/08/15 SMI"

#define	 __EXTENSIONS__	/* header bug! strtok_r is overly hidden */
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>

#include <libcpc.h>

#include "cpucmds.h"

struct args {
	FILE *fp;
	int colnum;
};

#define	MAX_RHS_COLUMN	76

/*ARGSUSED*/
static void
list_cap(void *arg, int regno, const char *name, uint8_t bits)
{
	struct args *args = arg;

	if ((args->colnum + strlen(name) + 1) > MAX_RHS_COLUMN) {
		(void) fprintf(args->fp, "\n\t\t");
		args->colnum = 16;
	}
	args->colnum += fprintf(args->fp, "%s ", name);
}

int
capabilities(FILE *fp, int cpuver)
{
	struct args _args, *args = &_args;
	char *text, *tok, *cp;
	const char *ccp;
	int i;

	args->fp = fp;

	if (cpuver == -1) {
		(void) fprintf(args->fp, "%s\n",
		    gettext("No CPU performance counters present."));
		return (1);
	}

	if ((ccp = cpc_getcciname(cpuver)) == NULL)
		ccp = "No information available";
	(void) fprintf(args->fp, "\t%s: %s\n\n",
	    gettext("CPU performance counter interface"), ccp);

	(void) fprintf(args->fp, gettext("\tevents\t"));
	args->colnum = 16;

	if ((ccp = cpc_getusage(cpuver)) == NULL)
		ccp = "No information available";
	text = strdup(ccp);
	for (cp = strtok_r(text, " ", &tok);
	    cp != NULL; cp = strtok_r(NULL, " ", &tok)) {
		if ((args->colnum + strlen(cp)) > MAX_RHS_COLUMN) {
			(void) fprintf(args->fp, "\n\t\t\t");
			args->colnum = 24;
		}
		args->colnum += fprintf(args->fp, cp);
	}
	(void) fprintf(args->fp, "\n");
	free(text);

	for (i = 0; i < cpc_getnpic(cpuver); i++) {
		(void) fprintf(args->fp, "\n\tevent%d: ", i);
		args->colnum = 16;
		cpc_walk_names(cpuver, i, args, list_cap);
		(void) fprintf(args->fp, "\n");
	}

	(void) fprintf(args->fp, "\n\t");
	args->colnum = 8;

	if ((ccp = cpc_getcpuref(cpuver)) == NULL)
		ccp = "No information available";
	text = strdup(ccp);
	for (cp = strtok_r(text, " ", &tok);
	    cp != NULL; cp = strtok_r(NULL, " ", &tok)) {
		if ((args->colnum + strlen(cp) + 1) > MAX_RHS_COLUMN) {
			(void) fprintf(args->fp, "\n\t");
			args->colnum = 8;
		}
		args->colnum += fprintf(args->fp, "%s ", cp);
	}
	(void) fprintf(args->fp, "\n");
	free(text);

	return (0);
}
