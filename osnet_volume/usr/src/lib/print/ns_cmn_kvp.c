/*
 * Copyright (c) 1994, 1995, 1996, 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)ns_cmn_kvp.c	1.10	98/07/22 SMI"

/*LINTLIBRARY*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>

#include <print/ns.h>
#include <print/list.h>

/*
 *	Commonly Used routines...
 */

/*
 * FUNCTION:
 *	kvp_create(const char *key, const void *value)
 * INPUT(S):
 *	const char *key
 *		- key for key/value pair
 *	const void *value
 *		- value for key/value pair
 * OUTPUT(S):
 *	ns_kvp_t * (return value)
 *		- pointer to structure containing the key/value pair
 * DESCRIPTION:
 */
ns_kvp_t *
ns_kvp_create(const char *key, const char *value)
{
	ns_kvp_t *kvp;

	if ((kvp = (ns_kvp_t *)malloc(sizeof (*kvp))) != NULL) {
		kvp->key = strdup(key);
		kvp->value = (char *)value;
	}
	return (kvp);
}

void
ns_kvp_destroy(ns_kvp_t *kvp)
{
	if (kvp != NULL) {
		if (kvp->key != NULL)
			free(kvp->key);
		if (kvp->value != NULL)
			free(kvp->value);
		free(kvp);
	}
}




/*
 * FUNCTION:
 *	ns_kvp_match_key(const ns_kvp_t *kvp, const char *key)
 * INPUT(S):
 *	const ns_kvp_t *kvp
 *		- key/value pair to check
 *	const char *key
 *		- key for matching
 * OUTPUT(S):
 *	int (return value)
 *		- 0 if matched
 * DESCRIPTION:
 */
static int
ns_kvp_match_key(const ns_kvp_t *kvp, char *key)
{
	if ((kvp != NULL) && (kvp->key != NULL) && (key != NULL))
		return (strcmp(kvp->key, key));
	return (-1);
}


/*
 * FUNCTION:
 *	ns_r_get_value(const char *key, const ns_printer_t *printer)
 * INPUT(S):
 *	const char *key
 *		- key for matching
 *	const ns_printer_t *printer
 *		- printer to glean this from
 * OUTPUT(S):
 *	char * (return value)
 *		- NULL, if not matched
 * DESCRIPTION:
 */
static void *
ns_r_get_value(const char *key, const ns_printer_t *printer, int level)
{
	ns_kvp_t *kvp, **attrs;

	if ((key == NULL) || (printer == NULL) ||
	    (printer->attributes == NULL))
		return (NULL);

	if (level++ == 16)
		return (NULL);

	/* find it right here */
	if ((kvp = list_locate((void **)printer->attributes,
			(COMP_T)ns_kvp_match_key, (void *)key)) != NULL) {
		void *value = string_to_value(key, kvp->value);

		/* fill in an empty printer for a bsdaddr */
		if (strcmp(key, NS_KEY_BSDADDR) == 0) {
			ns_bsd_addr_t *addr = value;

			if (addr->printer == NULL)
				addr->printer = strdup(printer->name);
		}
		return (value);
	}

	/* find it in a child */
	for (attrs = printer->attributes; attrs != NULL && *attrs != NULL;
	    attrs++) {
		void *value = NULL;

		if ((strcmp((*attrs)->key, NS_KEY_ALL) == 0) ||
		    (strcmp((*attrs)->key, NS_KEY_GROUP) == 0)) {
			char **printers;

			for (printers = string_to_value((*attrs)->key,
						(*attrs)->value);
			    printers != NULL && *printers != NULL; printers++) {
				ns_printer_t *printer =
					ns_printer_get_name(*printers, NULL);

				if ((value = ns_r_get_value(key, printer,
							    level)) != NULL)
					return (value);
				ns_printer_destroy(printer);
			}
		} else if (strcmp((*attrs)->key, NS_KEY_LIST) == 0) {
			ns_printer_t **printers;

			for (printers = string_to_value((*attrs)->key,
						(*attrs)->value);
			    printers != NULL && *printers != NULL; printers++) {
				if ((value = ns_r_get_value(key, *printers,
							    level)) != NULL)
					return (value);
			}
		} else if (strcmp((*attrs)->key, NS_KEY_USE) == 0) {
			char *string = NULL;
			ns_printer_t *printer =
				ns_printer_get_name((*attrs)->value, NULL);
			if ((value = ns_r_get_value(key, printer,
					level)) != NULL)
				string = value_to_string(string, value);
			if (string != NULL)
				value = string_to_value(key, string);
			ns_printer_destroy(printer);
		}

		if (value != NULL)
			return (value);
	}

	return (NULL);
}


/*
 * ns_get_value() gets the value of the passed in attribute from the passed
 * in printer structure.  The value is returned in a converted format.
 */
void *
ns_get_value(const char *key, const ns_printer_t *printer)
{
	return (ns_r_get_value(key, printer, 0));
}


/*
 * ns_get_value_string() gets the value of the key passed in from the
 * printer structure passed in.  The results is an ascii string.
 */
char *
ns_get_value_string(const char *key, const ns_printer_t *printer)
{
	return ((char *)value_to_string(key, ns_get_value(key, printer)));
}


/*
 * ns_set_value() sets the passed in kvp in the passed in printer structure,
 * This is done by converting the value to a string first.
 */
int
ns_set_value(const char *key, const void *value, ns_printer_t *printer)
{
	return (ns_set_value_from_string(key,
			value_to_string(key, (void *)value), printer));
}


/*
 * ns_set_value_from_string() sets the passed in kvp in the passed in printer
 * structure.
 */
int
ns_set_value_from_string(const char *key, const char *string,
			ns_printer_t *printer)
{
	if (printer == NULL)
		return (-1);

	if (key == NULL)
		list_iterate((void **)printer->attributes,
				(VFUNC_T)ns_kvp_destroy);
	else {
		ns_kvp_t *kvp;

		if ((kvp = list_locate((void **)printer->attributes,
					(COMP_T)ns_kvp_match_key,
					(void *)key)) == NULL) {
			kvp = (ns_kvp_t *)malloc(sizeof (*kvp));
			kvp->key = strdup(key);
			printer->attributes = (ns_kvp_t **)
				list_append((void **)printer->attributes, kvp);
		}
		if (string != NULL)
			kvp->value = strdup(string);
		else
			kvp->value = NULL;
	}

	return (0);
}
