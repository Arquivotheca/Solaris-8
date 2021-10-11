/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnselect.cc	1.2	98/01/15 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <xfn/fnselect.hh>

#define	LINE 80
static const char *option = "-D";
static const char *fns_config_file = FNS_CONFIG_FILE;
static const char *ns_prefix = FNS_NS_PREFIX;
static char *program;

static void usage(const char *cmd, const char *msg = 0, char *arg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s%s\n", cmd,
			msg, (arg == 0) ? "" : arg);
	fprintf(stderr, "%s:\t%s [ -D ] [default|nisplus|nis|files]\n",
		gettext("Usage"), cmd);
	exit(1);
}

static void cmdError(const char *cmd, const char *msg,
		    const char *arg = 0, const char *arg2 = 0)
{
	fprintf(stderr, "%s: %s", cmd, msg);
	if (arg != 0)
		fprintf(stderr, " %s", arg);
	if (arg2 != 0)
		fprintf(stderr, " %s", arg2);
	fprintf(stderr, "\n");
	exit(1);
}


static int FNSP_match_ns(const char *line, const char *name)
{
	int len = strlen(name);
	return ((strncasecmp(line, name, len) == 0) &&
		(line[len] == ' ' || line[len] == '\t'));
}

static void update_ns_file(const char *ns, int ns_type)
{
	char fns_config_bak[LINE];
	sprintf(fns_config_bak, "%s.bak", fns_config_file);

	FILE *rf, *wf;
	if ((rf = fopen(fns_config_file, "r")) == 0)
		cmdError(program,
		    gettext("Unable to open file:"), fns_config_file);

	if ((wf = fopen(fns_config_bak, "w")) == 0)
		cmdError(program,
		    gettext("Unable to open file:"), fns_config_bak);

	// Copy the lines from *rf* file to *wf* file, omitting ns lines
	char line[256];
	while (fgets(line, sizeof (line), rf)) {
		if (!FNSP_match_ns(line, ns_prefix))
			fputs(line, wf);
	}
	fclose(rf);

	// Add ns line if it is not 'default'
	if (ns_type != FNSP_default_ns)
		fprintf(wf, "%s\t%s\n", ns_prefix, ns);
	fclose(wf);

	if (rename(fns_config_bak, fns_config_file) < 0)
		cmdError(program,
		    gettext("Unable to rename file:"),
		    fns_config_bak, fns_config_file);
}


main(int argc, char **argv)
{
	// Internationalization
	setlocale(LC_ALL, "");

	program = argv[0];

	// Either -D option specified or naming service specfied
	// Not both
	if (argc > 2)
		usage(program, gettext("Too many arguments"));
	int ns;

	// asking which naming service has been explicitly configured
	if (argc == 1) {
		ns = fnselect_from_config_file();
		printf("%s\n", FNSP_naming_service_name(ns));
		exit(0);
	}

	// asking which naming service is actually being used
	if (strcmp(argv[1], option) == 0) {
		ns = fnselect_from_probe();
		if ((ns == FNSP_default_ns) ||
		    (ns == FNSP_unknown_ns))
			ns = FNSP_nisplus_ns;
		printf("%s\n", FNSP_naming_service_name(ns));
		exit(0);
	}

	// Beyond this point, request is to explicitly configure ns
	// First, we must do some sanity checks

	// Check if the user has root permissions
	if (geteuid() != 0)
		cmdError(program, gettext("No Permission"));

	// Check that naming system is one that we support
	if ((ns = FNSP_get_naming_service_type(argv[1])) == FNSP_unknown_ns)
		usage(program,
		    gettext("Unknown naming service: "), argv[1]);

	// Check if the config file exists
	struct stat buffer;
	if (stat(fns_config_file, &buffer) == 0) {
		update_ns_file(argv[1], ns);
		exit(0);
	}

	// Record default usage only if not specify 'default'
	if (ns != FNSP_default_ns) {
		// Config file does not exist, create a new one
		FILE *wf;
		if ((wf = fopen(fns_config_file, "w")) != 0) {
			fprintf(wf, "%s\t%s\n", ns_prefix, argv[1]);
			fclose(wf);
			exit(0);
		} else
			cmdError(program,
			    gettext("Unable to open file: "),
			    fns_config_file);
	}
	exit(1);
}
