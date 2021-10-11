/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncreate_fs.cc	1.3	96/03/31 SMI"


// fncreate_fs [-r] [-v] -f file_name composite_name
// fncreate_fs [-r] [-v] composite_name [mount_options] [mount_location...]
//
// -r	Replace the bindings in the context with only those specified.
//	Equivalent to destroying the context and then running fncreate_fs
//	without this option.
//
// -v	Verbose mode.  May be repeated to display debugging output.
//
// The standard input format is a sequence of lines of the form:
//
//	 <name> [<options>] [<location>...]
//
// where <name> may be "." or a slash-separated hierarchy (not
// beginning with a slash), and <options> and <location> are as
// described in automount(1M).
//
// Either multiple input lines can be read from an input file, or a
// single input line can be specified on the command line.  In the
// latter case <name> is not given explicitly; it is taken to be ".".
//
// For some amount of compatibility with automount(1M), the following
// input format is also accepted:
//
//	<name>		  [<options>] [<location>...] \
//		<offset1> [<options1>] <location1>... \
//  		<offset2> [<options2>] <location2>... \
//		...
//
// The slash indicates a continuation of a single line.  Each <offset*>
// is a slash-separated hierarchy, and must begin with a slash.  This is
// interpreted as being equivalent to:
//
//	<name>		[<options>] [<location>...]
//	<name><offset1> [<options1>] <location1>...
//  	<name><offset2> [<options2>] <location2>...
//	...
//
// (the first line being omitted if both <options> are <location> are
// omitted).


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <libintl.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <xfn/xfn.hh>
#include "fncreate_fs.hh"


// Command line arguments.

static char	*command;
static bool_t	replace = FALSE;
static char	*composite_name = NULL;

FILE	*infile = NULL;
char	*cmdline_location = NULL;
int	verbose = 0;


// Process the command line arguments.  argc and argv are passed
// from main().

static void process_cmdline(int argc, char *argv[]);


// Print command name, a message (if msg is non-null), and a usage
// description; then exit.

static void usage(const char *msg = NULL);

int
main(int argc, char *argv[])
{
	process_cmdline(argc, argv);

	FN_composite_name *name =
	    composite_name_from_str(composite_name);
	if (name->count() < 2) {
		error(NULL, "Not a file system context: '%s'", composite_name);
	}

	Dir *input = parse_input();
	if (input == NULL) {
		return (-1);
	}

	bool_t nsid;
	FN_ctx *ctx = penultimate_ctx(name, nsid);

	if (replace) {
		destroy(name, ctx, nsid);
	}

	if (nsid) {
		const FN_string fs((unsigned char *)"fs");
		*input->name = fs;
	} else {
		void *iter;
		*input->name = *name->last(iter);
	}
	update_namespace(ctx, input, nsid);
	delete ctx;

	return (0);
}


static void
process_cmdline(int argc, char *argv[])
{
	command = strrchr(argv[0], '/');
	if (command == NULL) {
		command = argv[0];
	} else {
		command++;
	}

	int c;
	while ((c = getopt(argc, argv, "f:rv")) != EOF) {
		switch (c) {
		case 'r':
			replace = TRUE;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			infile = (strcmp(optarg, "-") == 0)
					? stdin
					: fopen(optarg, "r");
			if (infile == NULL) {
				error(NULL, "Could not open input file %s",
					optarg);
			}
			break;
		default:
			usage("Invalid option");
		}
	}

	if (optind == argc) {
		usage();
	}

	composite_name = argv[optind++];

	// Extract location.
	if (optind == argc) {
		if (infile == NULL) {
			cmdline_location = concat("");
		}
	} else {
		if (infile != NULL) {
			usage();
		}
		size_t size = 0;
		int i;
		for (i = optind; i < argc; i++) {
			size += strlen(argv[i]) + 1;
		}
		char *location = cmdline_location = new char[size];
		mem_check(location);
		strcpy(location, argv[optind]);
		location += strlen(argv[optind]);
		for (optind++; optind < argc; optind++) {
			sprintf(location, " %s", argv[optind]);
			location += strlen(argv[optind]) + 1;
		}
	}
}


static void
usage(const char *msg)
{
	if (msg != NULL) {
		fprintf(stderr, "%s: %s\n", command, gettext(msg));
	}
	fprintf(stderr, "%s:\t%s [-r] [-v] -f %s\n",
		gettext("Usage"),
		command,
		gettext("file_name composite_name"));
	fprintf(stderr, "\t%s [-r] [-v] %s\n",
		command,
		gettext("composite_name [mount_options] [mount_location]..."));
	exit(-1);
}


FN_composite_name *
composite_name_from_str(const char *str, bool_t delete_empty)
{
	FN_composite_name *name =
	    new FN_composite_name((const unsigned char *)str);
	mem_check(name);

	// Delete trailing empty component, if any.
	void *iter;
	const FN_string *atom = name->last(iter);
	if ((atom != NULL) && atom->is_empty() && (!name->is_empty())) {
		name->next(iter);
		name->delete_comp(iter);
	}

	// Delete all empty components
	if (delete_empty) {
		for (atom = name->first(iter);
		    atom != NULL;
		    atom = name->next(iter)) {
			if (atom->is_empty()) {
				name->delete_comp(iter);
			}
		}
	}
	return (name);
}


const char *
str(const FN_composite_name *name)
{
	FN_string *string = name->string();
	mem_check(string);
	return (str(string));	// string is never freed.
}


const char *
str(const FN_string *string)
{
	const unsigned char *s = string->str();
	mem_check(s);
	return ((const char *)s);
}


char *
concat(const char *s1, const char *s2, char sep)
{
	if (s1 == NULL) {
		s1 = "";
	}
	if (s2 == NULL) {
		s2 = "";
	}
	char *s = new char[strlen(s1) + 1 + strlen(s2) + 1];
	mem_check(s);
	if (sep != '\0') {
		sprintf(s, "%s%c%s", s1, sep, s2);
	} else {
		sprintf(s, "%s%s", s1, s2);
	}
	return (s);
}


void
mem_check(const void *ptr)
{
	if (ptr == NULL) {
		error(NULL, "Memory allocation failure");
	}
}


void
info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(gettext(fmt), args);
	va_end(args);

	printf("\n");
}


void
error(const FN_status *status, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, gettext(fmt), args);
	va_end(args);

	int statcode = -1;
	if (status != NULL) {
		statcode = status->code();
		char *desc = (char *)(status->description()->str());
		if (desc == NULL) {
			fprintf(stderr, ": %s = %u", gettext("statcode"),
				statcode);
		} else {
			fprintf(stderr, ": %s", desc);
		}
	}
	fprintf(stderr, "\n");
	exit(statcode);
}
