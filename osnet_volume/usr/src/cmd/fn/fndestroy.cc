/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fndestroy.cc	1.3	96/03/31 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h> // gettext()

#include <xfn/xfn.hh>

#define	AUTHORITATIVE 1

void
usage(char *cmd)
{
	fprintf(stderr, "%s:\t%s %s\n", gettext("Usage"), cmd,
		gettext("composite_name"));
	exit(1);
}

// returns 1 on failure and 0 on success

int
destroy(FN_ctx *ctx, const FN_string &name)
{
	FN_status status;

	if (ctx->destroy_subcontext(name, status) == 0) {
		fprintf(stderr, "%s '%s' %s: %s\n",
			gettext("Destroy of"),
			(char *)(name.str()),
			gettext("failed"),
			(char *)(status.description()->str()));
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		usage(argv[0]);

	FN_status status;
	FN_ctx *ctx = FN_ctx::from_initial(AUTHORITATIVE, status);

	if (ctx == 0) {
		fprintf(stderr, "%s %s\n",
			gettext("Unable to get initial context!"),
			(char *)(status.description()->str()));
		exit(1);
	}

	int exit_status = destroy(ctx, (unsigned char *)argv[1]);
	delete ctx;
	exit(exit_status);
}
