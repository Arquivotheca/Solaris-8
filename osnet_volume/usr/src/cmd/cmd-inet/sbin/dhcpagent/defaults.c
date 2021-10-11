/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)defaults.c	1.5	99/09/01 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <deflt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dhcpmsg.h>
#include <stdio.h>

#include "defaults.h"

enum df_type { DF_BOOL, DF_INTEGER, DF_STRING, DF_OCTET };

struct dhcp_default {

	const char	*df_name;	/* parameter name */
	const char	*df_default;	/* default value */
	enum df_type	df_type;	/* datatype of parameter */
	int		df_min;		/* min value if type DF_INTEGER */
	int		df_max;		/* max value if type DF_INTEGER */
};

/*
 * note: keep in the same order as tunable parameter constants in defaults.h
 */

static struct dhcp_default defaults[] = {

	{ "RELEASE_ON_SIGTERM=",  "0",		DF_BOOL,	0,   0	  },
	{ "IGNORE_FAILED_ARP=",	  "1",		DF_BOOL,	0,   0	  },
	{ "OFFER_WAIT=",	  "3",		DF_INTEGER,	1,   20	  },
	{ "ARP_WAIT=",		  "1000",	DF_INTEGER,	100, 4000 },
	{ "CLIENT_ID=",		  NULL,		DF_OCTET,	0,   0	  },
	{ "PARAM_REQUEST_LIST=",  NULL,		DF_STRING,	0,   0    }
};

/*
 * df_get_string(): gets the string value of a given user-tunable parameter
 *
 *   input: const char *: the interface the parameter applies to
 *	    unsigned int: the parameter number to look up
 *  output: const char *: the parameter's value, or default if not set
 *			  (must be copied by caller to be kept)
 */

const char *
df_get_string(const char *if_name, unsigned int p)
{
	const char	*value;
	char		param[256];
	int		flags;

	if (p >= (sizeof (defaults) / sizeof (*defaults)))
		return (NULL);

	/*
	 * if we can't open a defaults file, use the hardwired
	 * defaults.  if we can, then if we can find the parameter
	 * `if_name:param', then use that value; otherwise, if we
	 * can't find that, then use `param'; otherwise, use the
	 * hardwired defaults.
	 */

	if (defopen(DHCP_AGENT_DEFAULTS) != 0) {
		dhcpmsg(MSG_WARNING, "df_get_string: cannot open "
		    DHCP_AGENT_DEFAULTS ", using default values");
		return (defaults[p].df_default);
	}

	/* ignore case */
	flags = defcntl(DC_GETFLAGS, 0);
	(void) defcntl(DC_SETFLAGS, TURNOFF(flags, DC_CASE));

	(void) snprintf(param, sizeof (param), "%s.%s", if_name,
	    defaults[p].df_name);

	value = defread(param);
	if (value == NULL) {
		(void) strlcpy(param, defaults[p].df_name, sizeof (param));
		value = defread(param);
		if (value == NULL)
			value = defaults[p].df_default;
	}

	(void) defopen(NULL);		/* closes DHCP_AGENT_DEFAULTS */
	return (value);
}

/*
 * df_get_octet(): gets the integer value of a given user-tunable parameter
 *
 *   input: const char *: the interface the parameter applies to
 *	    unsigned int: the parameter number to look up
 *	    unsigned int *: the length of the returned value
 *  output: uchar_t *: a pointer to byte array (default value if not set)
 *		       (must be copied by caller to be kept)
 */

uchar_t *
df_get_octet(const char *if_name, unsigned int p, unsigned int *len)
{
	const char	*value;
	static uchar_t	octet_value[256]; /* as big as defread() returns */

	if (p >= (sizeof (defaults) / sizeof (*defaults)))
		return (NULL);

	value = df_get_string(if_name, p);
	if (value == NULL)
		goto do_default;

	if (strncasecmp("0x", value, 2) != 0) {
		*len = strlen(value);			/* no NUL */
		return ((uchar_t *)value);
	}

	/* skip past the 0x and convert the value to binary */
	value += 2;
	*len = sizeof (octet_value);
	if (ascii_to_octet((char *)value, strlen(value), octet_value,
	    (int *)len) != 0) {
		dhcpmsg(MSG_WARNING, "df_get_octet: cannot convert value "
		    "for parameter `%s', using default", defaults[p].df_name);
		goto do_default;
	}
	return (octet_value);

do_default:
	if (defaults[p].df_default == NULL) {
		*len = 0;
		return (NULL);
	}

	*len = strlen(defaults[p].df_default);		/* no NUL */
	return ((uchar_t *)defaults[p].df_default);
}

/*
 * df_get_int(): gets the integer value of a given user-tunable parameter
 *
 *   input: const char *: the interface the parameter applies to
 *	    unsigned int: the parameter number to look up
 *  output: int: the parameter's value, or default if not set
 */

int
df_get_int(const char *if_name, unsigned int p)
{
	const char	*value;
	int		value_int;

	if (p >= (sizeof (defaults) / sizeof (*defaults)))
		return (0);

	value = df_get_string(if_name, p);
	if (value == NULL || !isdigit(*value))
		goto failure;

	value_int = atoi(value);
	if (value_int > defaults[p].df_max || value_int < defaults[p].df_min)
		goto failure;

	return (value_int);

failure:
	dhcpmsg(MSG_WARNING, "df_get_int: parameter `%s' is not between %d and "
	    "%d, defaulting to `%s'", defaults[p].df_name, defaults[p].df_min,
	    defaults[p].df_max, defaults[p].df_default);
	return (atoi(defaults[p].df_default));
}

/*
 * df_get_bool(): gets the boolean value of a given user-tunable parameter
 *
 *   input: const char *: the interface the parameter applies to
 *	    unsigned int: the parameter number to look up
 *  output: boolean_t: B_TRUE if true, B_FALSE if false, default if not set
 */

boolean_t
df_get_bool(const char *if_name, unsigned int p)
{
	const char	*value;

	if (p >= (sizeof (defaults) / sizeof (*defaults)))
		return (0);

	value = df_get_string(if_name, p);
	if (value != NULL) {

		if (strcasecmp(value, "true") == 0 ||
		    strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0)
			return (B_TRUE);

		if (strcasecmp(value, "false") == 0 ||
		    strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0)
			return (B_FALSE);
	}

	dhcpmsg(MSG_WARNING, "df_get_bool: parameter `%s' has invalid value "
	    "`%s', defaulting to `%s'", defaults[p].df_name,
	    value ? value : "NULL", defaults[p].df_default);

	return ((atoi(defaults[p].df_default) == 0) ? B_FALSE : B_TRUE);
}
