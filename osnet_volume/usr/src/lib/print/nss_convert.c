/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)nss_convert.c	1.5	99/08/23 SMI"

/*
 * This file contains routines necessary to convert a string buffer into
 * a printer object.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>

#define	ESCAPE_CHARS	"\\\n=:"	/* \, \n, =, : */

/*
 * Just like strncat(3C), but escapes the supplied characters.
 * This allows the escape character '\' and seperators to be part of the
 * keys or values.
 */
char *
strncat_escaped(char *d, char *s, int len, char *escape)
{
	char *t = d;

	while ((*t != NULL) && (len > 0))
		len--, t++;

	if (escape == NULL)
		escape = "\\";

	while ((*s != NULL) && (len > 0)) {
		if (strchr(escape, *s) != NULL)
			len--, *t++ = '\\';
		len--, *t++ = *s++;
	}
	*t = NULL;

	return (d);
}



char *
_cvt_printer_to_entry(ns_printer_t *printer, char *buf, int buflen)
{
	int i, len;
	int bufferok = 1;

	memset(buf, NULL, buflen);

	if ((printer == NULL) || (printer->attributes == NULL))
		return (NULL);

	if (snprintf(buf, buflen, "%s", printer->name) >= buflen) {
		memset(buf, NULL, buflen);
		syslog(LOG_ERR, "_cvt_printer_to_entry: buffer overflow");
		return (NULL);
	}

	if ((printer->aliases != NULL) && (printer->aliases[0] != NULL)) {
		char **alias = printer->aliases;

		while (*alias != NULL) {
			strlcat(buf, "|", buflen);
			strncat_escaped(buf, *alias++, buflen, ESCAPE_CHARS);
		}
	}

	if (strlcat(buf, ":", buflen) >= buflen) {
		memset(buf, NULL, buflen);
		syslog(LOG_ERR, "_cvt_printer_to_entry: buffer overflow");
		return (NULL);
	}

	len = strlen(buf);

	for (i = 0; printer->attributes[i] != NULL && bufferok; i++) {
		ns_kvp_t *kvp = printer->attributes[i];

		if (kvp->value == NULL)
			continue;
		strlcat(buf, "\\\n\t:", buflen);
		strncat_escaped(buf, kvp->key, buflen, ESCAPE_CHARS);
		strlcat(buf, "=", buflen);
		strncat_escaped(buf, kvp->value, buflen, ESCAPE_CHARS);
		if (strlcat(buf, ":", buflen) >= buflen) {
			bufferok = 0;
		}
	}

	if (!bufferok) {
		memset(buf, NULL, buflen);
		syslog(LOG_ERR, "_cvt_printer_to_entry: buffer overflow");
		return (NULL);
	}

	if (strlen(buf) == len) {	/* there were no attributes */
		memset(buf, NULL, buflen);
		buf = NULL;
	}

	return (buf);
}


ns_printer_t *
_cvt_nss_entry_to_printer(char *entry, char *ns)
{
	char	*name = NULL,
		*key = NULL,
		**aliases = NULL,
		*cp,
		buf[BUFSIZ];
	int in_namelist = 1, buf_pos = 0;
	ns_printer_t *printer = NULL;

	if (entry == NULL)
		return (NULL);

	memset(buf, NULL, sizeof (buf));
	for (cp = entry; *cp != NULL; cp++) {
		switch (*cp) {
		case ':':	/* end of kvp */
			if (in_namelist != 0) {
				if (name == NULL)
					name = strdup(buf);
				else
					aliases = (char **)list_append(
							(void **)aliases,
							(void *)strdup(buf));
				printer = (ns_printer_t *)ns_printer_create(
						name, aliases, ns, NULL);
				in_namelist = 0;
			} else if (key != NULL)
				ns_set_value_from_string(key, buf, printer);
			memset(buf, NULL, sizeof (buf));
			buf_pos = 0;
			key = NULL;
			break;
		case '=':	/* kvp seperator */
			if (key == NULL) {
				key = strdup(buf);
				memset(buf, NULL, sizeof (buf));
				buf_pos = 0;
			} else
				buf[buf_pos++] = *cp;
			break;
		case '|':	/* namelist seperator */
			if (in_namelist != 0) {
				if (name == NULL)
					name = strdup(buf);
				else
					aliases = (char **)list_append(
							(void **)aliases,
							(void *)strdup(buf));
				memset(buf, NULL, sizeof (buf));
				buf_pos = 0;
			} else	/* add it to the buffer */
				buf[buf_pos++] = *cp;
			break;
		case '\\':	/* escape char */
			buf[buf_pos++] = *(++cp);
			break;
		default:
			buf[buf_pos++] = *cp;
		}

	}

	if (key != NULL)
		ns_set_value_from_string(key, buf, printer);

	return (printer);
}
