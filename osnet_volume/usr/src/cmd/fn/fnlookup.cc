/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnlookup.cc	1.5	96/03/31 SMI"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libintl.h> // gettext()

#include <xfn/xfn.hh>

static char	*target_name = 0;
static unsigned	target_detail = 0;
static unsigned follow_link = 0;
static unsigned int authoritative = 0;

// Options:
// -L	If the composite name is bound to an XFN link, lookup the
//	reference that the link references, rather than the link itself.
// -v	run in verbose mode
// -A   go to authoritative source for information
//
void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr, "%s:\t%s [-vLA] %s\n", gettext("Usage"), cmd,
		gettext("composite_name"));
	exit(1);
}

void
process_cmd_line(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "ALv")) != -1) {
		switch (c) {
		case 'v' :
			target_detail = 2;
			break;
		case 'L':
			follow_link = 1;
			break;
		case 'A':
			authoritative = 1;
			break;
		case '?':
			default :
			usage(argv[0], "invalid option");
		}
	}
	if (optind < argc)
		target_name = argv[optind++];
	else
		usage(argv[0], "missing composite name");

	if (optind < argc)
		usage(argv[0], "too many arguments");
}


// returns 1 on failure and 0 on success

int
lookup_and_print(FN_ctx *ctx, const FN_string &name)
{
	FN_status status;
	FN_ref* ref = 0;
	FN_string *desc;

	if (follow_link)
		ref = ctx->lookup(name, status);
	else
		ref = ctx->lookup_link(name, status);
	if (ref) {
		desc = ref->description(target_detail);
		printf("%s",
		    desc? (char *)(desc->str()) : gettext("No reference"));
		delete ref;
		delete desc;
		return (0);
	} else {
		desc = status.description();
		fprintf(stderr, "%s '%s' %s: %s\n",
			gettext("Lookup of"),
			(char *)(name.str()),
			gettext("failed"),
			desc ? (char *)(desc->str()) :
			gettext("No status description"));
		delete desc;
		return (1);
	}
}

int
main(int argc, char **argv)
{
	process_cmd_line(argc, argv);

	FN_status status;
	FN_ctx* ctx = FN_ctx::from_initial(authoritative, status);

	if (ctx == 0) {
		fprintf(stderr, "%s %s\n",
			gettext("Unable to get initial context!"),
			(char *)(status.description()->str()));
		exit(1);
	}

	int exit_status = lookup_and_print(ctx, (unsigned char *)target_name);
	delete ctx;
	exit(exit_status);
}
