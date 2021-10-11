/*
 * Copyright (c) 1994, 1995, 1996, 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)ns_bsd_addr.c	1.8	99/08/23 SMI"

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
#include <print/misc.h>

/*
 *	Manipulate bsd_addr structures
 */
static ns_bsd_addr_t *
bsd_addr_create(const char *server, const char *printer, const char *extension)
{
	ns_bsd_addr_t *addr = NULL;

	if (server != NULL) {
		addr = (ns_bsd_addr_t *)malloc(sizeof (*addr));
		(void) memset(addr, 0, sizeof (*addr));
		addr->printer = (char *)printer;
		addr->server = (char *)server;
		addr->extension = (char *)extension;
	}

	return (addr);
}

static char *
bsd_addr_to_string(const ns_bsd_addr_t *addr)
{
	char buf[BUFSIZ];

	if ((addr == NULL) || (addr->server == NULL))
		return (NULL);

	if (snprintf(buf, sizeof (buf), "%s", addr->server) >= sizeof (buf)) {
		syslog(LOG_ERR, "bsd_addr_to_string: buffer overflow");
		return (NULL);
	}

	if ((addr->printer != NULL) || (addr->extension != NULL))
		(void) strlcat(buf, ",", sizeof (buf));
	if (addr->printer != NULL)
		if (strlcat(buf, addr->printer, sizeof (buf)) >= sizeof (buf)) {
			syslog(LOG_ERR, "bsd_addr_to_string: buffer overflow");
			return (NULL);
		}
	if (addr->extension != NULL) {
		(void) strlcat(buf, ",", sizeof (buf));
		if (strlcat(buf, addr->extension, sizeof (buf))
						>= sizeof (buf)) {
			syslog(LOG_ERR, "bsd_addr_to_string: buffer overflow");
			return (NULL);
		}
	}

	return (strdup(buf));
}

ns_bsd_addr_t *
string_to_bsd_addr(const char *string)
{
	char **list, *tmp, *printer = NULL, *extension = NULL;

	if (string == NULL)
		return (NULL);

	tmp = strdup(string);
	list = strsplit(tmp, ",");

	if (list[1] != NULL) {
		printer = list[1];
		if (list[2] != NULL)
			extension = list[2];
	}

	return (bsd_addr_create(list[0], printer, extension));
}

static char *
list_to_string(const char **list)
{
	char buf[BUFSIZ];

	if ((list == NULL) || (*list == NULL))
		return (NULL);

	if (snprintf(buf, sizeof (buf), "%s", *list) >= sizeof (buf)) {
		syslog(LOG_ERR, "list_to_string: buffer overflow");
		return (NULL);
	}

	while (*++list != NULL) {
		(void) strlcat(buf, ",", sizeof (buf));
		if (strlcat(buf, *list, sizeof (buf)) >= sizeof (buf)) {
			syslog(LOG_ERR, "list_to_string: buffer overflow");
			return (NULL);
		}
	}

	return (strdup(buf));
}

static char *
internal_list_to_string(const ns_printer_t **list)
{
	char buf[BUFSIZ];

	if ((list == NULL) || (*list == NULL))
		return (NULL);

	if (snprintf(buf, sizeof (buf), "%s", (*list)->name) >= sizeof (buf)) {
		syslog(LOG_ERR, "internal_list_to_string:buffer overflow");
		return (NULL);
	}

	while (*++list != NULL) {
		(void) strlcat(buf, ",", sizeof (buf));
		if (strlcat(buf, (*list)->name, sizeof (buf)) >= sizeof (buf)) {
			syslog(LOG_ERR,
				"internal_list_to_string:buffer overflow");
			return (NULL);
		}
	}

	return (strdup(buf));
}


char *
value_to_string(const char *key, void *value)
{
	char *string = NULL;

	if ((key != NULL) && (value != NULL)) {
		if (strcmp(key, NS_KEY_BSDADDR) == 0) {
			string = bsd_addr_to_string(value);
		} else if ((strcmp(key, NS_KEY_ALL) == 0) ||
			    (strcmp(key, NS_KEY_GROUP) == 0)) {
			string = list_to_string(value);
		} else if (strcmp(key, NS_KEY_LIST) == 0) {
			string = internal_list_to_string(value);
		} else {
			string = strdup((char *)value);
		}
	}

	return (string);
}


void *
string_to_value(const char *key, char *string)
{
	void *value = NULL;

	if ((key != NULL) && (string != NULL) && (string[0] != NULL)) {
		if (strcmp(key, NS_KEY_BSDADDR) == 0) {
			value = (void *)string_to_bsd_addr(string);
		} else if ((strcmp(key, NS_KEY_ALL) == 0) ||
			    (strcmp(key, NS_KEY_GROUP) == 0)) {
			value = (void *)strsplit(string, ",");
		} else {
			value = (void *)string;
		}
	}

	return (value);
}


/*
 * This routine parses POSIX style names (server:printer[:conformance]).
 * the conformance isn't part of a POSIX style name, but it doesn't interfere
 * and it provides a nice direct access to the conformance field.
 */
ns_printer_t *
posix_name(const char *name)
{
	ns_printer_t *printer = NULL;

	if ((name != NULL) && (strchr(name, ':') != NULL)) {
		ns_kvp_t **kvp_list, *kvp;
		char *addr = strdup(name);
		char *tmp;

		for (tmp = addr; *tmp != NULL; tmp++)
			if (*tmp == ':')
				*tmp = ',';

		if ((kvp = (ns_kvp_t *)ns_kvp_create(NS_KEY_BSDADDR, addr))
		    == NULL)
			return (NULL);

		if ((kvp_list = (ns_kvp_t **)list_append(NULL, kvp)) == NULL)
			return (NULL);

		printer = (ns_printer_t *) ns_printer_create(strdup(name), NULL,
							    "posix", kvp_list);
	}

	return (printer);
}


/*
 * FUNCTION:
 *	int ns_bsd_addr_cmp(ns_bsd_addr_t *at, ns_bsd_addr_t *a2)
 * INPUTS:
 *	ns_bsd_addr_t *a1 - a bsd addr
 *	ns_bsd_addr_t *21 - another bsd addr
 * DESCRIPTION:
 *	This functions compare 2 bsd_addr structures to determine if the
 *	information in them is the name.
 */
static int
ns_bsd_addr_cmp(ns_bsd_addr_t *a1, ns_bsd_addr_t *a2)
{
	int rc;

	if ((a1 == NULL) || (a2 == NULL))
		return (1);

	if ((rc = strcmp(a1->server, a2->server)) != 0)
		return (rc);

	if ((a1->printer == NULL) || (a2->printer == NULL))
		return (a1->printer != a2->printer);

	return (strcmp(a1->printer, a2->printer));
}


/*
 * FUNCTION:
 *	ns_bsd_addr_t *ns_bsd_addr_get_name(char *name)
 * INPUTS:
 *	char *name - name of printer to get address for
 * OUTPUTS:
 *	ns_bsd_addr_t *(return) - the address of the printer
 * DESCRIPTION:
 *	This function will get the BSD address of the printer specified.
 *	it fills in the printer name if none is specified in the "name service"
 *	as a convenience to calling functions.
 */
ns_bsd_addr_t *
ns_bsd_addr_get_name(char *name)
{
	ns_printer_t *printer;
	ns_bsd_addr_t *addr = NULL;

	endprinterentry();
	if ((printer = ns_printer_get_name(name, NULL)) != NULL) {
		addr = ns_get_value(NS_KEY_BSDADDR, printer);

		if (addr != NULL && addr->printer == NULL)
			addr->printer = strdup(printer->name);
		if (addr != NULL)
			addr->pname = strdup(printer->name);
	}

	return (addr);
}


/*
 * FUNCTION:
 *	ns_bsd_addr_t **ns_bsd_addr_get_list()
 * OUTPUT:
 *	ns_bsd_addr_t **(return) - a list of bsd addresses for all printers
 *				   in all "name services"
 * DESCRIPTION:
 *	This function will gather a list of all printer addresses in all
 *	of the "name services".  All redundancy is removed.
 */
ns_bsd_addr_t **
ns_bsd_addr_get_list(int unique)
{
	ns_printer_t **printers;
	ns_bsd_addr_t **list = NULL;

	for (printers = ns_printer_get_list(NULL);
			printers != NULL && *printers != NULL; printers++) {
		ns_bsd_addr_t *addr;

		if (strcmp(NS_NAME_ALL, (*printers)->name) == 0)
			continue;

		if ((addr = ns_get_value(NS_KEY_BSDADDR, *printers)) != NULL) {
			if (addr->printer == NULL)
				addr->printer = strdup((*printers)->name);
			addr->pname = strdup((*printers)->name);
		}

		if (unique == UNIQUE)
			list =
			    (ns_bsd_addr_t **)list_append_unique((void **)list,
				(void *)addr, (COMP_T)ns_bsd_addr_cmp);
		else
			list = (ns_bsd_addr_t **)list_append((void **)list,
					(void *)addr);
	}

	return (list);
}




/*
 * FUNCTION:
 *	ns_bsd_addr_t **ns_bsd_addr_get_list()
 * OUTPUT:
 *	ns_bsd_addr_t **(return) - a list of bsd addresses for "_all" printers
 *				   in the "name service"
 * DESCRIPTION:
 *	This function will use the "_all" entry to find a list of printers and
 *	addresses. The "default" printer is also added to the list.
 *	All redundancy is removed.
 */
ns_bsd_addr_t **
ns_bsd_addr_get_all(int unique)
{
	ns_printer_t *printer;
	ns_bsd_addr_t **list = NULL;
	char **printers;
	char *def = NULL;

	if (((def = (char *)getenv("PRINTER")) == NULL) &&
	    ((def = (char *)getenv("LPDEST")) == NULL))
		def = NS_NAME_DEFAULT;

	list = (ns_bsd_addr_t **)list_append((void **)list,
			(void *)ns_bsd_addr_get_name(def));

	endprinterentry();
	if ((printer = ns_printer_get_name(NS_NAME_ALL, NULL)) == NULL)
		return (ns_bsd_addr_get_list(unique));

	for (printers = (char **)ns_get_value(NS_KEY_ALL, printer);
			printers != NULL && *printers != NULL; printers++) {
		ns_bsd_addr_t *addr;

		addr = ns_bsd_addr_get_name(*printers);
		if (addr != NULL)
			addr->pname = *printers;
		if (unique == UNIQUE)
			list =
			    (ns_bsd_addr_t **)list_append_unique((void **)list,
				(void *)addr, (COMP_T)ns_bsd_addr_cmp);
		else
			list = (ns_bsd_addr_t **)list_append((void **)list,
					(void *)addr);
	}

	return (list);
}

ns_bsd_addr_t *
ns_bsd_addr_get_default()
{
	char *def = NULL;
	ns_bsd_addr_t *addr;

	if (((def = (char *)getenv("PRINTER")) == NULL) &&
	    ((def = (char *)getenv("LPDEST")) == NULL)) {
		def = NS_NAME_DEFAULT;
		addr = ns_bsd_addr_get_name(def);
		if (addr != NULL) {
			addr->pname = def;
			return (addr);
		}
	}

	return (NULL);
}
