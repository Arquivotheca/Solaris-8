/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnunbind.cc	1.3	96/03/31 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <libintl.h> // gettext()

#include <xfn/xfn.hh>

void
usage(char *cmd)
{
	fprintf(stderr, "%s:\t%s %s\n",
		gettext("Usage"), cmd, gettext("composite_name"));
	exit(1);
}


// returns 1 on failure and 0 on success
int
unbind(FN_ctx *ctx, const FN_string &name)
{
	FN_status status;

	if (ctx->unbind(name, status) == 0) {
		fprintf(stderr, "%s '%s' failed: %s\n",
			gettext("Unbind of"),
			name.str(),
			status.description()->str());
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	unsigned int authoritative = 1;
	if (argc != 2)
		usage(argv[0]);

	FN_status status;
	FN_ctx* ctx = FN_ctx::from_initial(authoritative, status);

	if (ctx == 0) {
		fprintf(stderr, "%s %s\n",
			gettext("Unable to get initial context!"),
			status.description()->str());
		exit(1);
	}

	int exit_status = unbind(ctx, (unsigned char *)(argv[1]));
	delete ctx;
	exit(exit_status);
}
