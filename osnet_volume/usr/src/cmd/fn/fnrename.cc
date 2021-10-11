/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnrename.cc	1.4	96/03/31 SMI"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libintl.h> // gettext()

#include <xfn/xfn.hh>

static int	arg_bind_supercede = 0;
static int	arg_verbose = 0;
static char	*arg_existing_name = 0;
static char	*arg_new_name = 0;
static char	*arg_target_context_name = 0;

#define	AUTHORITATIVE 1

void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr, "%s:\t%s [-sv] %s\n",
		gettext("Usage"),
		cmd,
	gettext("target_context_name existing_atomic_name  new_atomic_name"));
	exit(1);
}

void
process_cmd_line(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "sv")) != -1) {
		switch (c) {
		case 's' :
			arg_bind_supercede = 1;
			break;
		case 'v' :
			arg_verbose = 1;
			break;
		case '?':
			default :
			usage(argv[0], "invalid option");
		}
	}

	if (optind < argc)
		arg_target_context_name = argv[optind++];
	else
		usage(argv[0], "missing target context name argument");

	if (optind < argc)
		arg_existing_name = argv[optind++];
	else
		usage(argv[0], "missing existing name argument");

	if (optind < argc)
		arg_new_name = argv[optind++];
	else
		usage(argv[0], "missing new name argument");

	if (optind < argc)
		usage(argv[0], "too many arguments");
}

// returns 1 on failure and 0 on success

int
rename(FN_ctx *ctx,
    const FN_string &target_context_name,
    const FN_string &existing_name,
    const FN_string &new_name)
{
	FN_status status;
	FN_string *desc = 0;
	unsigned bind_flags = (arg_bind_supercede? 0: FN_OP_EXCLUSIVE);
	FN_ref *target_ref;
	FN_ctx *target_ctx;

	if (arg_verbose) {
		printf("%s '%s' to '%s' %s '%s':\n",
		    gettext("Renaming"),
		    (char *)existing_name.str(),
		    (char *)new_name.str(),
		    gettext("in context of"),
		    (char *)target_context_name.str());
	}

	if ((target_ref = ctx->lookup(target_context_name, status)) == 0) {
		desc = status.description();
		fprintf(stderr, "%s '%s' %s: %s\n",
			gettext("Lookup of"),
			(char *)(target_context_name.str()),
			gettext("failed"),
			(desc? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		return (1);
	}

	if ((target_ctx =
	    FN_ctx::from_ref(*target_ref, AUTHORITATIVE, status)) == 0) {
		desc = status.description();
		fprintf(stderr, "%s '%s': %s\n",
			gettext("Rename failed; cannot get context handle to"),
			(char *)(target_context_name.str()),
			(desc? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		return (1);
	}

	if (target_ctx->rename(existing_name, new_name, bind_flags,
	    status) == 0) {
		desc = status.description();
		fprintf(stderr, "%s '%s' %s '%s' %s '%s' %s: %s\n",
			gettext("rename of"),
			(char *)(existing_name.str()),
			gettext("to"),
			(char *)(new_name.str()),
			gettext("in context of"),
			(char *)(target_context_name.str()),
			gettext("failed"),
			(desc? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		delete target_ctx;
		return (1);
	}
	delete target_ctx;
	return (0);
}


int
main(int argc, char **argv)
{
	process_cmd_line(argc, argv);

	FN_status status;
	FN_ctx* ctx = FN_ctx::from_initial(AUTHORITATIVE, status);

	if (ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s %s\n",
			gettext("Unable to get initial context!"),
			desc? (char *)(desc->str()):
			gettext("No status description"));
		delete desc;
		exit(1);
	}

	int exit_status = rename(ctx,
	    (unsigned char *)arg_target_context_name,
	    (unsigned char *)arg_existing_name,
	    (unsigned char *)arg_new_name);
	delete ctx;
	exit(exit_status);
}
