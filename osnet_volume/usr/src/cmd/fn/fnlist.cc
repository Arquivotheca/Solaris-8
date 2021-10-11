/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnlist.cc	1.5	96/03/31 SMI"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libintl.h> /* gettext() */

#include <xfn/xfn.hh>

static int show_bindings = 0;
static char *target_name = 0;
static unsigned target_detail = 0;
static unsigned int authoritative = 0;

void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr, "%s:\t%s [-lvA][%s]\n",
		gettext("Usage"), cmd, gettext("composite_name"));
	exit(1);
}

void
process_cmd_line(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "Alv")) != -1) {
		switch (c) {
		case 'l' :
			show_bindings = 1;
			break;
		case 'v' :
			target_detail = 2;
			show_bindings = 1;
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

	if (optind < argc)
		usage(argv[0], "too many arguments");
}



// returns 1 on failure and 0 on success
int
list_and_print(FN_ctx *ctx, const FN_string &name)
{
	FN_status status;
	FN_namelist* nl;

	if (nl = (ctx->list_names(name, status))) {
		FN_string *n;
		while (n = nl->next(status)) {
			printf("%s\n", (char *)(n->str()));
			delete n;
		}
		delete nl;
		if (!status.is_success()) {
			fprintf(stderr, "%s: %s\n", gettext("Error"),
			    (char *)(status.description()->str()));
			return (1);
		}
	} else {
		fprintf(stderr, "%s: %s\n", gettext("Error"),
		    (char *)(status.description()->str()));
		return (1);
	}
	return (0);
}

// returns 1 on failure and 0 on success

int
list_bindings_and_print(FN_ctx *ctx, const FN_string &name)
{
	FN_status status;
	FN_bindinglist *bl;

	if (bl = (ctx->list_bindings(name, status))) {
		FN_string *desc;
		FN_string *n;
		FN_ref *ref;
		while (n = bl->next(&ref, status)) {
			desc = ref->description(target_detail);
			printf("%s: %s\n", gettext("name"),
			    (char *)(n->str()));
			delete n;
			if (desc) {
				printf("%s", (char *)(desc->str()));
				delete desc;
			}
			delete ref;
			putchar('\n');
		}
		delete bl;
		if (!status.is_success()) {
			fprintf(stderr, "%s: %s\n", gettext("Error"),
			    (char *)(status.description()->str()));
			return (1);
		}
	} else {
		fprintf(stderr, "%s: %s\n", gettext("Error"),
		    (char *)(status.description()->str()));
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	int result;

	process_cmd_line(argc, argv);
	FN_status status;
	FN_ctx *ctx = FN_ctx::from_initial(authoritative, status);

	if (ctx == 0) {
		fprintf(stderr,
			"%s %s\n", gettext("Unable to get initial context!"),
			(char *)(status.description()->str()));
		exit(1);
	}

	if (target_name == 0)
		target_name = "";

	if (show_bindings)
		result = list_bindings_and_print(ctx,
		    (unsigned char *)target_name);
	else
		result =  list_and_print(ctx, (unsigned char *)target_name);
	delete ctx;
	return (result);
}
