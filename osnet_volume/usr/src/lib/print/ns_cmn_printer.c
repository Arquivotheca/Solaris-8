/*
 * Copyright (c) 1994, 1995, 1996, 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)ns_cmn_printer.c	1.5	99/08/23 SMI"

/*LINTLIBRARY*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#include <print/ns.h>
#include <print/list.h>

extern void ns_kvp_destroy(ns_kvp_t *);

/*
 *	Commonly Used routines...
 */

/*
 * FUNCTION:
 *	printer_create(const char *key, const void *value)
 * INPUT(S):
 *	const char *key
 *		- key for key/value pair
 *	const void *value
 *		- value for key/value pair
 * OUTPUT(S):
 *	ns_printer_t * (return value)
 *		- pointer to structure containing the key/value pair
 * DESCRIPTION:
 */
ns_printer_t *
ns_printer_create(char *name, char **aliases, char *source,
			ns_kvp_t **attributes)
{
	ns_printer_t *printer;

	if ((printer = (ns_printer_t *)malloc(sizeof (*printer))) != NULL) {
		printer->name = (char *)name;
		printer->aliases = (char **)aliases;
		printer->source = (char *)source;
		printer->attributes = (ns_kvp_t **)attributes;
	}
	return (printer);
}


static int
ns_strcmp(char *s1, char *s2)
{
	return (strcmp(s1, s2) != 0);
}


/*
 * FUNCTION:
 *	ns_printer_match_name(const ns_printer_t *printer, const char *name)
 * INPUT(S):
 *	const ns_printer_t *printer
 *		- key/value pair to check
 *	const char *key
 *		- key for matching
 * OUTPUT(S):
 *	int (return value)
 *		- 0 if matched
 * DESCRIPTION:
 */
int
ns_printer_match_name(ns_printer_t *printer, const char *name)
{
	if ((printer == NULL) || (printer->name == NULL) || (name == NULL))
		return (-1);

	if ((strcmp(printer->name, name) == 0) ||
	    (list_locate((void **)printer->aliases,
			(COMP_T)ns_strcmp, (void *)name) != NULL))
		return (0);

	return (-1);
}


static void
_ns_append_printer_name(const char *name, va_list ap)
{
	char *buf = va_arg(ap, char *);
	int bufsize = va_arg(ap, int);

	if (name == NULL)
		return;

	(void) strlcat(buf, name, bufsize);
	(void) strlcat(buf, "|", bufsize);
}

/*
 * FUNCTION:
 *	char *ns_printer_name_list(const ns_printer_t *printer)
 * INPUT:
 *	const ns_printer_t *printer - printer object to generate list from
 * OUTPUT:
 *	char * (return) - a newly allocated string containing the names of
 *			  the printer
 */
char *
ns_printer_name_list(const ns_printer_t *printer)
{
	char buf[BUFSIZ];

	if ((printer == NULL) || (printer->name == NULL))
		return (NULL);

	if (snprintf(buf, sizeof (buf), "%s|", printer->name) >= sizeof (buf)) {
		syslog(LOG_ERR, "ns_printer_name:buffer overflow");
		return (NULL);
	}

	list_iterate((void **)printer->aliases,
		(VFUNC_T)_ns_append_printer_name, buf, sizeof (buf));

	buf[strlen(buf) - 1] = (char)NULL;

	return (strdup(buf));
}
