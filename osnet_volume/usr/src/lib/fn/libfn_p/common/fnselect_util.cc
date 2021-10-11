/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnselect_util.cc	1.3	97/11/12 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/nis.h>
#include <stdlib.h>
#include <string.h>
#include "fnselect.hh"

#define	LINE 80
static const char *fns_config_file = FNS_CONFIG_FILE;

static const char *fns_default_ns = "default";
static const char *fns_nisplus_ns = "nisplus";
static const char *fns_nis_ns = "nis";
static const char *fns_files_ns = "files";
static const char *ns_prefix = FNS_NS_PREFIX;

static char *FNSP_org_map = "fns_org.ctx";
static char *FNSP_files_map_dir = "/var/fn";
static char *FNSP_lock_file = "fns.lock";

static const char *
enterprise_match(const char *line)
{
	const char *rest = 0;
	size_t len;

	len = strlen(ns_prefix);
	if (strncasecmp(line, ns_prefix, len) == 0) {
		rest = line + strlen(ns_prefix);
		while ((*rest == ' ') ||
		    (*rest == '\t'))
			rest++;
	}
	return (rest);
}

// Returns the name service stored in FNS configuration file
int
fnselect_from_config_file()
{
	struct stat buffer;
	FILE *rf = 0;
	char line[LINE];
	int answer = FNSP_unknown_ns;
	if (stat(fns_config_file, &buffer) != 0)
		return (0);

	if ((rf = fopen(fns_config_file, "r")) == 0) {
		fprintf(stderr, "Unable to open file: %s\n",
		    fns_config_file);
		return (0);
	}

	int found = 0;
	const char *rest;
	char ns[LINE];
	while ((!found) && (fgets(line, sizeof (line), rf))) {
		if ((rest = enterprise_match(line))) {
			found = 1;
			strcpy(ns, rest);
			if (ns[strlen(ns) - 1] == '\n')
				ns[strlen(ns) - 1] = '\0';
			answer = FNSP_get_naming_service_type(ns);
		}
	}
	if (rf)
		fclose(rf);
	return (answer);
}

// Function that return the naming-service used by FNS
// Function called by fnselect with -D option and
// fncreate* commands
int
fnselect()
{
	int answer = fnselect_from_config_file();
	if (answer != FNSP_unknown_ns)
		return (answer);

	nis_result	*res = 0;
	char		domain[NIS_MAXNAMELEN+1];

	sprintf(domain, "ctx_dir.%s", nis_local_directory());
	res = nis_lookup(domain, NO_AUTHINFO | USE_DGRAM);
	if ((res->status == NIS_SUCCESS)) {
		answer = FNSP_nisplus_ns;
	} else if ((sysinfo(SI_SRPC_DOMAIN, domain, NIS_MAXNAMELEN) > 0) &&
	    (yp_bind(domain) == 0)) {
		answer = FNSP_nis_ns;
		yp_unbind(domain);
	} else {
		answer = FNSP_files_ns;
	}

	if (res)
		nis_freeresult(res);
	return (answer);
}

// Return name of naming service that have been determined by probing
// the actual naming services
// FNSP_unknown_ns if nothing has been configured

int
fnselect_from_probe()
{
	int answer = fnselect_from_config_file();
	if (answer != FNSP_unknown_ns)
		return (answer);

	nis_result	*res = 0;
	char tmp[NIS_MAXNAMELEN+1];
	char *domain = tmp;

	// Check whether NIS+ has been configured for FNS
	sprintf(domain, "ctx_dir.%s", nis_local_directory());
	res = nis_lookup(domain, NO_AUTHINFO | USE_DGRAM);
	if ((res->status == NIS_SUCCESS)) {
		nis_freeresult(res);
		return (FNSP_nisplus_ns);
	}
	if (res)
		nis_freeresult(res);

	// Check whether NIS has been configured for FNS
	if ((sysinfo(SI_SRPC_DOMAIN, domain, NIS_MAXNAMELEN) > 0) &&
	    (yp_bind(domain) == 0)) {
		char *nis_master;
		if (yp_master(domain, FNSP_org_map, &nis_master) == 0) {
			free(nis_master);
			yp_unbind(domain);
			return (FNSP_nis_ns);
		}
		yp_unbind(domain);
	}

	// Check whether files has been configured for FNS
	char *mapfile = tmp;
	struct stat buffer;
	sprintf(mapfile, "%s/%s", FNSP_files_map_dir, FNSP_lock_file);
	if (stat(mapfile, &buffer) == 0) {
		return (FNSP_files_ns);
	}
	return (FNSP_unknown_ns);  // nothing has been configured
}

const char *
FNSP_naming_service_name(int ns)
{
	switch (ns) {
	case FNSP_nisplus_ns:
		return (fns_nisplus_ns);
	case FNSP_nis_ns:
		return (fns_nis_ns);
	case FNSP_files_ns:
		return (fns_files_ns);
	default:
		return (fns_default_ns);
	}
}

int
FNSP_get_naming_service_type(const char *ns)
{
	if (strcasecmp(ns, fns_nisplus_ns) == 0)
		return (FNSP_nisplus_ns);
	else if (strcasecmp(ns, fns_nis_ns) == 0)
		return (FNSP_nis_ns);
	else if (strcasecmp(ns, fns_files_ns) == 0)
		return (FNSP_files_ns);
	else if (strcasecmp(ns, fns_default_ns) == 0)
		return (FNSP_default_ns);
	else
		return (FNSP_unknown_ns);
}
