/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)ns.c	1.17	99/08/23 SMI"

/*LINTLIBRARY*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <nss_dbdefs.h>
#include <syslog.h>

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>


/*
 * because legacy code can use a variety of values for various name
 * services, this routine is needed to "normalize" them.
 */
char *
normalize_ns_name(char *ns)
{
	if (ns == NULL)
		return (NULL);
	else if ((strcasecmp(ns, "files") == 0) ||
			(strcasecmp(ns, "system") == 0) ||
			(strcasecmp(ns, "etc") == 0))
		return ("files");
	else if (strcasecmp(ns, "nis") == 0)
		return ("nis");
	else if ((strcasecmp(ns, "nisplus") == 0) ||
			(strcasecmp(ns, "nis+") == 0))
		return ("nisplus");
	else if ((strcasecmp(ns, "xfn") == 0) ||
			(strcasecmp(ns, "fns") == 0))
		return ("xfn");
	else
		return (ns);
}


/*
 * FUNCTION:
 *	void ns_printer_destroy(ns_printer_t *printer)
 * INPUT:
 *	ns_printer_t *printer - a pointer to the printer "object" to destroy
 * DESCRIPTION:
 *	This function will free all of the memory associated with a printer
 *	object.  It does this by walking the structure ad freeing everything
 *	underneath it, with the exception of the object source field.  This
 *	field is not filled in with newly allocated space when it is
 *	generated
 */
void
ns_printer_destroy(ns_printer_t *printer)
{
	if (printer != NULL) {
		if (printer->attributes != NULL) {	/* attributes */
			extern void ns_kvp_destroy(ns_kvp_t *);

			list_iterate((void **)printer->attributes,
				(VFUNC_T)ns_kvp_destroy);
			free(printer->attributes);
		}
		if (printer->aliases != NULL) {		/* aliases */
			free(printer->aliases);
		}
		if (printer->name != NULL)		/* primary name */
			free(printer->name);
		free(printer);
	}
}


/*
 * FUNCTION:
 *	ns_printer_t **ns_printer_get_list()
 * OUTPUT:
 *	ns_printer_t ** (return value) - an array of pointers to printer
 *					 objects.
 * DESCRIPTION:
 *	This function will return a list of all printer objects found in every
 *	configuration interface.
 */
ns_printer_t **
ns_printer_get_list(const char *ns)
{
	char	    buf[NSS_LINELEN_PRINTERS];
	ns_printer_t    **printer_list = NULL;

	(void) setprinterentry(0, (char *)ns);

	ns = normalize_ns_name((char *)ns);
	while (getprinterentry(buf, sizeof (buf), (char *)ns) == 0) {
		ns_printer_t *printer =
			(ns_printer_t *)_cvt_nss_entry_to_printer(buf, NULL);

		printer_list = (ns_printer_t **)list_append(
					(void **)printer_list,
					(void *)printer);
	}

	(void) endprinterentry();

	return (printer_list);
}


/*
 * This function looks for the named printer in the supplied
 * name service (ns), or the name services configured under
 * the nsswitch.
 */
ns_printer_t *
ns_printer_get_name(const char *name, const char *ns)
{
	ns_printer_t *result = NULL;
	char buf[NSS_LINELEN_PRINTERS];

	if ((result = (ns_printer_t *)posix_name(name)) != NULL)
		return (result);

	ns = normalize_ns_name((char *)ns);
	if (getprinterbyname((char *)name, buf, sizeof (buf), (char *)ns) == 0)
		result = (ns_printer_t *)_cvt_nss_entry_to_printer(buf, NULL);

	return (result);
}


/*
 * FUNCTION:
 *	int ns_printer_put(const ns_printer_t *printer)
 * INPUT:
 *	const ns_printer_t *printer - a printer object
 * DESCRIPTION:
 *	This function attempts to put the data in the printer object back
 *	to the "name service" specified in the source field of the object.
 */
int
ns_printer_put(const ns_printer_t *printer)
{
	char func[32];
	int (*fpt)();

	if ((printer == NULL) || (printer->source == NULL))
		return (-1);

	if (snprintf(func, sizeof (func), "%s_put_printer",
		normalize_ns_name(printer->source)) >= sizeof (func)) {
			syslog(LOG_ERR, "ns_printer_put: buffer overflow");
			return (-1);
	}

	if ((fpt = (int (*)())dlsym(RTLD_DEFAULT, func)) != NULL)
		return ((*fpt)(printer));

	return (-1);
}
