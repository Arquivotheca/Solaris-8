/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_trace.c	1.1	99/07/07 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	OPT_INT		1
#define	OPT_STRING	2
#define	OPT_FILE	3

int	__ldap_debug_file = 2;
int	__ldap_debug_api;
int	__ldap_debug_ldap;
int	__ldap_debug_servers;

struct option {
	char	*name;
	int	type;
	void	*address;
};

static struct option options[] = {
	{ "debug_file", OPT_FILE, &__ldap_debug_file },
	{ "debug_api", OPT_INT, &__ldap_debug_api },
	{ "debug_ldap", OPT_INT, &__ldap_debug_servers },
	{ 0, 0, 0 },
};

extern int __ns_ldap_raise_fd(int);

static void
set_option(char *name, char *val)
{
	struct option *opt;
	int		n;
	char		*p;
	int		fd;

	for (opt = options; opt->name; opt++) {
		if (strcasecmp(name, opt->name) == 0) {
			switch (opt->type) {
			    case OPT_STRING:
				p = strdup(val);
				*((char **)opt->address) = p;
				break;
			    case OPT_INT:
				if (strcmp(val, "") == 0)
					n = 1;
				else
					n = atoi(val);
				*((int *)opt->address) = n;
				break;
			    case OPT_FILE:
				fd = open(val, O_WRONLY | O_CREAT, 0644);
				fd = __ns_ldap_raise_fd(fd);
				*((int *)opt->address) = fd;
				break;
			}
			break;
		}
	}
}

void
get_environment()
{
	char	*p;
	char	*base;
	char	optname[100];
	char	optval[100];

	p = getenv("LDAP_OPTIONS");
	if (p == NULL)
		return;

	while (*p) {
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;
		base = p;
		while (*p && *p != '=' && !isspace(*p))
			p++;
		(void) strncpy(optname, base, p - base);
		optname[p - base] = '\0';
		if (*p == '=') {
			p++;
			base = p;
			while (*p && !isspace(*p))
				p++;
			(void) strncpy(optval, base, p - base);
			optval[p - base] = '\0';
		} else {
			optval[0] = '\0';
		}
		set_option(optname, optval);
	}

#ifdef DEBUG
	fprintf(stderr, "debug_api: %d\n", __ldap_debug_api);
	fprintf(stderr, "debug_ldap: %d\n", __ldap_debug_ldap);
	fprintf(stderr, "debug_servers: %d\n", __ldap_debug_servers);
#endif
}
