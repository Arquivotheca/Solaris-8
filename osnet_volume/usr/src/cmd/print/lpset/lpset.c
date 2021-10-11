/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lpset.c	1.17	99/07/12 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <locale.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif
#include <pwd.h>

#include <print/ns.h>
#include <print/misc.h>
#include <print/list.h>

extern char *optarg;
extern int optind, opterr, optopt;
extern char *getenv(const char *);


static void
Usage(char *name)
{
	(void) fprintf(stderr,
		gettext("Usage: %s [-n files | nisplus | xfn ] [-x] "
			"[-a key=value] [-d key] (printer)\n"),
		name);
	exit(1);
}


/*
 *  main() calls the appropriate routine to parse the command line arguments
 *	and then calls the local remove routine, followed by the remote remove
 *	routine to remove jobs.
 */
int
main(int ac, char *av[])
{
	int result = 0;
	int delete_printer = 0;
	int c;
	char	*program = NULL,
		*printer = NULL,
		*ins = NULL,
		*ons = "files";
	char	**changes = NULL;
	ns_printer_t *printer_obj = NULL;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (ac < 2)
		Usage(program);

	while ((c = getopt(ac, av, "a:d:n:r:x")) != EOF)
		switch (c) {
		case 'd':
			if (strchr(optarg, '=') != NULL)
				Usage(program);
			/* FALLTHRU */
		case 'a':
			changes = (char **)list_append((void**)changes,
						(void *)strdup(optarg));
			break;
		case 'n':
			ons = optarg;
			break;
		case 'r':
			ins = optarg;
			break;
		case 'x':
			delete_printer++;
			break;
		default:
			Usage(program);
		}

	if (optind != ac-1)
		Usage(program);

	printer = av[optind];

	if (strchr(printer, ':') != NULL) {
		(void) fprintf(stderr, gettext(
			"POSIX-Style names are not valid destinations (%s)\n"),
			printer);
		return (1);
	}

	ins = normalize_ns_name(ins);
	ons = normalize_ns_name(ons);
	if (ins == NULL)
		ins = ons;

	/* check / set the name service for writing */
	if (strcasecmp("user", ons) == 0) {
		(void) setuid(getuid());
		ons = "user";
	} else if (strcasecmp("files", ons) == 0) {
		struct passwd *pw = getpwnam("lp");
		uid_t uid, lpuid = 0;

		if (pw != NULL)
			lpuid = pw->pw_uid;
		uid = getuid();
		if ((uid != 0) && (uid != lpuid)) {
			int len;
			gid_t list[NGROUPS_MAX];


			len = getgroups(sizeof (list), list);
			if (len == -1) {
				(void) fprintf(stderr, gettext(
					"Call to getgroups failed with "
					"errno %d\n"), errno);
				return (1);
			}

			for (; len >= 0; len--)
				if (list[len] == 14)
					break;

			if (len == -1) {
				(void) fprintf(stderr, gettext(
				    "Permission denied: not in group 14\n"));
				return (1);
			}
		}
		ons = "files";
	} else if (strcasecmp("nisplus", ons) == 0) {
		ons = "nisplus";
	} else if (strcasecmp("xfn", ons) == 0) {
		ons = "xfn";
	} else {
		(void) fprintf(stderr,
			gettext("%s is not a supported name service.\n"),
			ons);
		return (1);
	}

	/* get the printer object */
	if ((printer_obj = ns_printer_get_name(printer, ins)) == NULL) {
		if (delete_printer != 0) {
			(void) fprintf(stderr, gettext("%s: unknown printer\n"),
				printer);
			return (1);
		}
		printer_obj = (ns_printer_t *)malloc(sizeof (*printer_obj));
		(void) memset(printer_obj, '\0', sizeof (*printer_obj));
		printer_obj->name = strdup(printer);
	}
	printer_obj->source = ons;

	/* make the changes to it */
	while (changes != NULL && *changes != NULL) {
		int has_equals = (strchr(*changes, '=') != NULL);
		char *p, *key = NULL,
		     *value = NULL;

		key = *(changes++);

		for (p = key; ((p != NULL) && (*p != NULL)); p++)
			if (*p == '=') {
				*p = NULL;
				value = ++p;
				break;
			} else if (*p == '\\')
				p++;

		if ((value != NULL) && (*value == NULL))
			value = NULL;

		if ((key != NULL) && (key[0] != NULL)) {
			if ((value == NULL) &&
			    (ns_get_value(key, printer_obj) == NULL) &&
			    (has_equals == 0)) {
				fprintf(stderr,
					gettext("%s: unknown attribute\n"),
					key);
				result = 1;
			} else
			(void) ns_set_value_from_string(key, value,
				printer_obj);
		}
	}
	if (delete_printer != 0)
		printer_obj->attributes = NULL;

	/* write it back */
	if (ns_printer_put(printer_obj) != 0) {
		(void) fprintf(stderr,
				gettext("Failed to write into %s database\n"),
				ons);
		result = 1;
	}

	return (result);
}
