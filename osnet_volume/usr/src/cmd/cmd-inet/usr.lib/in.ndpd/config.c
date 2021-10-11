/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)config.c	1.1	99/03/21 SMI"

#include "defs.h"
#include "tables.h"

/*
 * Parse the config file which consists of entries of the form:
 *	ifdefault	[<variable> <value>]*
 *	prefixdefault	[<variable> <value>]*
 *	if <ifname>	[<variable> <value>]*
 *	prefix <prefix>/<length> <ifname>	[<variable> <value>]*
 *
 * All "ifdefault" and "prefixdefault" entries must preceed any
 * "if" and "prefix" entries.
 *
 * Values (such as expiry dates) which contain white space
 * can be quoted with single or double quotes.
 */

typedef	boolean_t	(*pfb_t)(char *str, uint_t *resp);

struct configinfo {
	char	*ci_name;
	uint_t	ci_min;		/* 0: no min check */
	uint_t	ci_max;		/* MAXUINT: no max check */
	uint_t	ci_default;
	uint_t	ci_index;	/* Into result array */
	pfb_t	ci_parsefunc;	/* Parse function returns -1 on failure */
};

enum config_type { CONFIG_IF, CONFIG_PREFIX};
typedef enum config_type config_type_t;

static void set_protocol_defaults(void);
static void print_defaults(void);
static boolean_t parse_default(config_type_t type, struct configinfo *list,
		    char *argvec[], int argcount,
		    struct confvar *defaults, char *line, int lineno);
static boolean_t parse_if(struct configinfo *list,
		    char *argvec[], int argcount,
		    char *line, int lineno);
static boolean_t parse_prefix(struct configinfo *list,
		    char *argvec[], int argcount,
		    char *line, int lineno);

static boolean_t parse_onoff(char *str, uint_t *resp);	/* boolean */
static boolean_t parse_int(char *str, uint_t *resp);	/* integer */
static boolean_t parse_ms(char *str, uint_t *resp);	/* milliseconds */
static boolean_t parse_s(char *str, uint_t *resp);	/* seconds */
static boolean_t parse_date(char *str, uint_t *resp);	/* date format */

#define	MAXUINT	0xffffffffU

/*
 * Per interface configuration variables.
 * Min, max, and default values are from RFC 2461.
 */
static struct configinfo iflist[] = {
	/* Name, Min, Max, Default, Index */
	{ "DupAddrDetectTransmits", 0, 100, 1, I_DupAddrDetectTransmits,
	parse_int },
	{ "AdvSendAdvertisements", 0, 1, 0, I_AdvSendAdvertisements,
	parse_onoff },
	{ "MaxRtrAdvInterval", 4, 1800, 600, I_MaxRtrAdvInterval, parse_s },
	{ "MinRtrAdvInterval", 3, 1350, 200, I_MinRtrAdvInterval, parse_s },
	/*
	 * No greater than .75 * MaxRtrAdvInterval.
	 * Default: 0.33 * MaxRtrAdvInterval
	 */
	{ "AdvManagedFlag", 0, 1, 0, I_AdvManagedFlag, parse_onoff },
	{ "AdvOtherConfigFlag", 0, 1, 0, I_AdvOtherConfigFlag, parse_onoff },
	{ "AdvLinkMTU", IPV6_MIN_MTU, 65535, 0, I_AdvLinkMTU, parse_int },
	{ "AdvReachableTime", 0, 3600000, 0, I_AdvReachableTime, parse_ms },
	{ "AdvRetransTimer", 0, MAXUINT, 0, I_AdvRetransTimer, parse_ms },
	{ "AdvCurHopLimit", 0, 255, 0, I_AdvCurHopLimit, parse_int },
	{ "AdvDefaultLifetime", 0, 9000, 1800, I_AdvDefaultLifetime, parse_s },
	/*
	 * MUST be either zero or between MaxRtrAdvInterval and 9000 seconds.
	 * Default: 3 * MaxRtrAdvInterval
	 */
	{ NULL, 0, 0, 0, 0 }
};

/*
 * Per prefix: AdvPrefixList configuration variables.
 * Min, max, and default values are from RFC 2461.
 */
static struct configinfo prefixlist[] = {
	/* Name, Min, Max, Default, Index */
	{ "AdvValidLifetime", 0, MAXUINT, 2592000, I_AdvValidLifetime,
	parse_s },
	{ "AdvOnLinkFlag", 0, 1, 1, I_AdvOnLinkFlag, parse_onoff },
	{ "AdvPreferredLifetime", 0, MAXUINT, 604800, I_AdvPreferredLifetime,
	parse_s},
	{ "AdvAutonomousFlag", 0, 1, 1, I_AdvAutonomousFlag, parse_onoff },
	{ "AdvValidExpiration", 0, MAXUINT, 0, I_AdvValidExpiration,
	parse_date },
	{ "AdvPreferredExpiration", 0, MAXUINT, 0, I_AdvPreferredExpiration,
	parse_date},
	{ NULL, 0, 0, 0, 0 },
};

/*
 * Data structures used to merge above protocol defaults
 * with defaults specified in the configuration file.
 */
static struct confvar ifdefaults[I_IFSIZE];
static struct confvar prefixdefaults[I_PREFIXSIZE];


int
parse_config(char *config_file, boolean_t file_required)
{
	FILE *fp;
	char line[MAXLINELEN];
	char pline[MAXLINELEN];
	int argcount;
	char *argvec[MAXARGSPERLINE];
	int defaultdone = 0;	/* Set when first non-default command found */
	int lineno = 0;

	if (debug & D_CONFIG)
		logdebug("parse_config()\n");

	set_protocol_defaults();
	if (debug & D_DEFAULTS)
		print_defaults();

	fp = fopen(config_file, "r");
	if (fp == NULL) {
		if (errno == ENOENT && !file_required)
			return (0);
		logperror(config_file);
		return (-1);
	}
	while (readline(fp, line, sizeof (line), &lineno) != 0) {
		(void) strncpy(pline, line, sizeof (pline));
		pline[sizeof (pline) - 1] = '\0';	/* NULL terminate */
		argcount = parse_line(pline, argvec,
		    sizeof (argvec) / sizeof (argvec[0]), lineno);
		if (debug & D_PARSE) {
			int i;

			logdebug("scanned %d args\n", argcount);
			for (i = 0; i < argcount; i++)
				logdebug("arg[%d]: %s\n", i, argvec[i]);
		}
		if (argcount == 0) {
			/* Empty line - or comment only line */
			continue;
		}
		if (strcmp(argvec[0], "ifdefault") == 0) {
			if (defaultdone) {
				logerr("ifdefault after non-default "
				    "command");
				logerr("line %d: <%s>\n", lineno, line);
				continue;
			}
			if (!parse_default(CONFIG_IF, iflist,
			    argvec+1, argcount-1,
			    ifdefaults, line, lineno))
				return (-1);
		} else if (strcmp(argvec[0], "prefixdefault") == 0) {
			if (defaultdone) {
				logerr("prefixdefault after non-default "
				    "command");
				logerr("line %d: <%s>\n", lineno, line);
				continue;
			}
			if (!parse_default(CONFIG_PREFIX, prefixlist,
			    argvec+1, argcount-1, prefixdefaults,
			    line, lineno))
				return (-1);
		} else if (strcmp(argvec[0], "if") == 0) {
			defaultdone = 1;
			if (!parse_if(iflist, argvec+1, argcount-1,
			    line, lineno))
				return (-1);
		} else if (strcmp(argvec[0], "prefix") == 0) {
			defaultdone = 1;
			if (!parse_prefix(prefixlist, argvec+1, argcount-1,
			    line, lineno))
				return (-1);
		} else {
			logerr("Unknown command in line %d: <%s>\n",
			    lineno, argvec[0]);
			return (-1);
		}
	}
	(void) fclose(fp);
	if (debug & D_DEFAULTS)
		print_defaults();

	return (0);
}

/*
 * Extract the defaults from the configinfo tables to initialize
 * the ifdefaults and prefixdefaults arrays.
 * The arrays are needed to track which defaults have been changed
 * by the config file.
 */
static void
set_protocol_defaults(void)
{
	struct configinfo *cip;

	if (debug & D_DEFAULTS)
		logdebug("extract_protocol_defaults\n");
	for (cip = iflist; cip->ci_name != NULL; cip++) {
		ifdefaults[cip->ci_index].cf_value = cip->ci_default;
		ifdefaults[cip->ci_index].cf_notdefault = _B_FALSE;
	}
	for (cip = prefixlist; cip->ci_name != NULL; cip++) {
		prefixdefaults[cip->ci_index].cf_value = cip->ci_default;
		prefixdefaults[cip->ci_index].cf_notdefault = _B_FALSE;
	}
}

void
print_iflist(struct confvar *confvar)
{
	struct configinfo *cip;

	for (cip = iflist; cip->ci_name != NULL; cip++) {
		logdebug("\t%s min %d max %d def %d value %d set %d\n",
		    cip->ci_name, cip->ci_min, cip->ci_max, cip->ci_default,
		    confvar[cip->ci_index].cf_value,
		    confvar[cip->ci_index].cf_notdefault);
	}
}

void
print_prefixlist(struct confvar *confvar)
{
	struct configinfo *cip;

	for (cip = prefixlist; cip->ci_name != NULL; cip++) {
		logdebug("\t%s min %d max %d def %d value %d set %d\n",
		    cip->ci_name, cip->ci_min, cip->ci_max, cip->ci_default,
		    confvar[cip->ci_index].cf_value,
		    confvar[cip->ci_index].cf_notdefault);
	}
}


static void
print_defaults(void)
{
	logdebug("Default interface variables:\n");
	print_iflist(ifdefaults);
	logdebug("Default prefix variables:\n");
	print_prefixlist(prefixdefaults);
}

/*
 * Read from fp. Handle \ at the end of the line by joining lines together.
 * Return 0 on EOF.
 * If linenop is not NULL increment it for each line that is read.
 */
int
readline(FILE *fp, char *line, int length, int *linenop)
{
	int got = 0;

retry:
	errno = 0;
	if (fgets(line, length, fp) == NULL) {
		if (errno == EINTR)
			goto retry;
		if (got != 0)
			return (1);
		else
			return (0);
	}
	if (linenop != NULL)
		(*linenop)++;
	got = strlen(line);
	/* Look for trailing \. Note that fgets includes the linefeed. */
	if (got >= 2 && line[got-2] == '\\') {
		/* Skip \ and LF */
		line += got - 2;
		length -= got - 2;
		goto retry;
	}
	/* Remove the trailing linefeed */
	if (got > 0)
		line[got-1] = '\0';

	return (1);
}

/*
 * Parse a line splitting it off at whitspace characters.
 * Modifies the content of the string by inserting NULLs.
 * If more arguments than fits in argvec/argcount then ignore the last.
 * Returns argcount.
 * Handles single quotes and double quotes.
 */
int
parse_line(char *line, char *argvec[], int argcount, int lineno)
{
	int i = 0;
	char *cp;
	boolean_t insingle_quote = _B_FALSE;
	boolean_t indouble_quote = _B_FALSE;

	/* Truncate at the beginning of a comment */
	cp = strchr(line, '#');
	if (cp != NULL)
		*cp = '\0';

	/* CONSTCOND */
	while (1) {
		/* Skip any whitespace */
		while (isspace(*line) && *line != '\0')
			line++;

		if (*line == '\'') {
			line++;
			if (*line == '\0')
				return (i);
			insingle_quote = _B_TRUE;
		} else if (*line == '"') {
			line++;
			if (*line == '\0')
				return (i);
			indouble_quote = _B_TRUE;
		}
		argvec[i] = line;
		if (*line == '\0')
			return (i);
		i++;
		/* Skip until next whitespace or end of quoted text */
		if (insingle_quote) {
			while (*line != '\'' && *line != '\0')
				line++;
			if (*line == '\'') {
				*line = ' ';
			} else {
				/* Handle missing quote at end */
				i--;
				logerr("Missing end quote on line %d - "
				    "ignoring <%s>\n",
				    lineno, argvec[i]);
				return (i);
			}
			insingle_quote = _B_FALSE;
		} else if (indouble_quote) {
			while (*line != '"' && *line != '\0')
				line++;
			if (*line == '"') {
				*line = ' ';
			} else {
				/* Handle missing quote at end */
				i--;
				logerr("Missing end quote on line %d - "
				    "ignoring <%s>\n",
				    lineno, argvec[i]);
				return (i);
			}
			indouble_quote = _B_FALSE;
		} else {
			while (!isspace(*line) && *line != '\0')
				line++;
		}
		if (*line != '\0') {
			/* Break off argument */
			*line++ = '\0';
		}
		if (i > argcount)
			return (argcount);
	}
	/* NOTREACHED */
}

/*
 * Returns true if ok; otherwise false.
 */
static boolean_t
parse_var_value(config_type_t type, struct configinfo *list,
    char *varstr, char *valstr,
    struct confvar *confvar, char *line, int lineno)
{
	struct configinfo *cip;
	uint_t val;

	if (debug & D_CONFIG) {
		logdebug("parse_var_value(%d, %s, %s)\n",
		    (int)type, varstr, valstr);
	}

	for (cip = list; cip->ci_name != NULL; cip++) {
		if (strcasecmp(cip->ci_name, varstr) == 0)
			break;
	}
	if (cip->ci_name == NULL) {
		logerr("Unknown variable <%s> on line %d <%s>\n",
		    varstr, lineno, line);
		return (_B_FALSE);
	}
	if (!(*cip->ci_parsefunc)(valstr, &val)) {
		logerr("Bad value <%s> on line %d <%s>\n",
		    valstr, lineno, line);
		return (_B_FALSE);
	}
	if (cip->ci_min != 0 && val < cip->ci_min) {
		logerr("Value %s is below minimum %d for %s on line %d <%s>\n",
		    valstr, cip->ci_min, varstr, lineno, line);
		return (_B_FALSE);
	}
	if (cip->ci_max != MAXUINT && val > cip->ci_max) {
		logerr("Value %s is above maximum %d for %s on line %d <%s>\n",
		    valstr, cip->ci_max, varstr, lineno, line);
		return (_B_FALSE);
	}
	/* Check against dynamic/relative limits */
	if (type == CONFIG_IF) {
		if (cip->ci_index == I_MinRtrAdvInterval &&
		    confvar[I_MaxRtrAdvInterval].cf_notdefault &&
		    val > confvar[I_MaxRtrAdvInterval].cf_value * 0.75) {
			logerr("MinRtrAdvInterval exceeds .75 * "
			    "MaxRtrAdvInterval (%u) on line %d <%s>\n",
			    confvar[I_MaxRtrAdvInterval].cf_value,
			    lineno, line);
			return (_B_FALSE);
		}
		if (cip->ci_index == I_MaxRtrAdvInterval &&
		    confvar[I_MinRtrAdvInterval].cf_notdefault &&
		    confvar[I_MinRtrAdvInterval].cf_value > val * 0.75) {
			logerr("MinRtrAdvInterval (%u) exceeds .75 * "
			    "MaxRtrAdvInterval on line %d <%s>\n",
			    confvar[I_MinRtrAdvInterval].cf_value,
			    lineno, line);
			return (_B_FALSE);
		}
		if (cip->ci_index == I_AdvDefaultLifetime &&
		    confvar[I_MaxRtrAdvInterval].cf_notdefault &&
		    val != 0 &&
		    val < confvar[I_MaxRtrAdvInterval].cf_value) {
			logerr("AdvDefaultLifetime is not between "
			    "MaxRtrAdrInterval (%u) and 9000 seconds "
			    "on line %d <%s>\n",
			    confvar[I_MaxRtrAdvInterval].cf_value,
			    lineno, line);
			return (_B_FALSE);
		}
		if (cip->ci_index == I_MaxRtrAdvInterval &&
		    confvar[I_AdvDefaultLifetime].cf_notdefault &&
		    confvar[I_AdvDefaultLifetime].cf_value < val) {
			logerr("AdvDefaultLifetime (%u) is not between "
			    "MaxRtrAdrInterval and 9000 seconds "
			    "on line %d <%s>\n",
			    confvar[I_AdvDefaultLifetime].cf_value,
			    lineno, line);
			return (_B_FALSE);

		}
	}
	confvar[cip->ci_index].cf_value = val;
	confvar[cip->ci_index].cf_notdefault = _B_TRUE;

	/* Derive dynamic/relative variables based on this one */
	if (type == CONFIG_IF) {
		if (cip->ci_index == I_MaxRtrAdvInterval &&
		    !confvar[I_MinRtrAdvInterval].cf_notdefault)
			confvar[I_MinRtrAdvInterval].cf_value = val / 3;
		if (cip->ci_index == I_MaxRtrAdvInterval &&
		    !confvar[I_AdvDefaultLifetime].cf_notdefault)
		    confvar[I_AdvDefaultLifetime].cf_value = 3 * val;
	}
	return (_B_TRUE);
}

/*
 * Split up the line into <variable> <value> pairs
 * Returns true if ok; otherwise false.
 */
static boolean_t
parse_default(config_type_t type, struct configinfo *list,
    char *argvec[], int argcount,
    struct confvar *defaults, char *line, int lineno)
{
	if (debug & D_CONFIG)
		logdebug("parse_default: argc %d\n", argcount);
	while (argcount >= 2) {
		if (!parse_var_value(type, list, argvec[0], argvec[1],
		    defaults, line, lineno))
			return (_B_FALSE);

		argcount -= 2;
		argvec += 2;
	}
	if (argcount != 0) {
		logerr("Trailing text <%s> on line %d\n",
		    argvec[0], lineno);
		return (_B_FALSE);
	}
	return (_B_TRUE);
}

/*
 * Returns true if ok; otherwise false.
 */
static boolean_t
parse_if(struct configinfo *list, char *argvec[], int argcount,
    char *line, int lineno)
{
	char *ifname;
	struct phyint *pi;

	if (debug & D_CONFIG)
		logdebug("parse_if: argc %d\n", argcount);

	if (argcount < 1) {
		logerr("Missing interface name on line %d <%s>\n",
		    lineno, line);
		return (_B_FALSE);
	}
	ifname = argvec[0];
	argvec++;
	argcount--;

	pi = phyint_lookup(ifname);
	if (pi == NULL) {
		int i;

		pi = phyint_create(ifname);
		if (pi == NULL) {
			logerr("parse_if: out of memory\n");
			return (_B_FALSE);
		}
		/*
		 * Copy the defaults from the default array.
		 * Do not copy the cf_notdefault fields since these have not
		 * been explicitly set for the phyint.
		 */
		for (i = 0; i < I_IFSIZE; i++)
			pi->pi_config[i].cf_value = ifdefaults[i].cf_value;
	}

	while (argcount >= 2) {
		if (!parse_var_value(CONFIG_IF, list, argvec[0], argvec[1],
		    pi->pi_config, line, lineno))
			return (_B_FALSE);

		argcount -= 2;
		argvec += 2;
	}
	if (argcount != 0) {
		logerr("Trailing text <%s> on line %d\n",
		    argvec[0], lineno);
		return (_B_FALSE);
	}
	return (_B_TRUE);
}

/*
 * Returns true if ok; otherwise false.
 */
static boolean_t
parse_prefix(struct configinfo *list, char *argvec[], int argcount,
    char *line, int lineno)
{
	char *ifname, *prefix;
	struct phyint *pi;
	struct prefix *pr;
	struct in6_addr in6;
	int prefixlen;

	if (debug & D_CONFIG)
		logdebug("parse_prefix: argc %d\n", argcount);

	if (argcount < 2) {
		logerr("Missing prefix and/or interface name on line %d <%s>\n",
		    lineno, line);
		return (_B_FALSE);
	}
	prefix = argvec[0];
	ifname = argvec[1];
	argvec += 2;
	argcount -= 2;

	prefixlen = parse_addrprefix(prefix, &in6);
	if (prefixlen == -1) {
		logerr("Bad prefix %s on line %d <%s>\n",
		    prefix, lineno, line);
		return (_B_FALSE);
	}

	pi = phyint_lookup(ifname);
	if (pi == NULL) {
		int i;

		pi = phyint_create(ifname);
		if (pi == NULL) {
			logerr("parse_prefix: out of memory\n");
			return (_B_FALSE);
		}
		/*
		 * Copy the defaults from the default array.
		 * Do not copy the cf_notdefault fields since these have not
		 * been explicitly set for the phyint.
		 */
		for (i = 0; i < I_IFSIZE; i++)
			pi->pi_config[i].cf_value = ifdefaults[i].cf_value;
	}
	pr = prefix_lookup(pi, in6, prefixlen);
	if (pr == NULL) {
		int i;

		pr = prefix_create(pi, in6, prefixlen);
		if (pr == NULL) {
			logerr("parse_prefix: out of memory\n");
			return (_B_FALSE);
		}
		/*
		 * Copy the defaults from the default array.
		 * Do not copy the cf_notdefault fields since these have not
		 * been explicitly set for the phyint.
		 */
		for (i = 0; i < I_PREFIXSIZE; i++)
			pr->pr_config[i].cf_value = prefixdefaults[i].cf_value;
	}

	while (argcount >= 2) {
		if (!parse_var_value(CONFIG_PREFIX, list, argvec[0], argvec[1],
		    pr->pr_config, line, lineno))
			return (_B_FALSE);

		argcount -= 2;
		argvec += 2;
	}
	if (argcount != 0) {
		logerr("Trailing text <%s> on line %d\n",
		    argvec[0], lineno);
		return (_B_FALSE);
	}
	return (_B_TRUE);
}

/*
 * Returns true if ok (and *resp updated) and false if failed.
 */
static boolean_t
parse_onoff(char *str, uint_t *resp)
{
	if (strcasecmp(str, "on") == 0) {
		*resp = 1;
		return (_B_TRUE);
	}
	if (strcasecmp(str, "off") == 0) {
		*resp = 0;
		return (_B_TRUE);
	}
	if (strcasecmp(str, "true") == 0) {
		*resp = 1;
		return (_B_TRUE);
	}
	if (strcasecmp(str, "false") == 0) {
		*resp = 0;
		return (_B_TRUE);
	}
	if (parse_int(str, resp)) {
		if (*resp == 0 || *resp == 1)
			return (_B_TRUE);
	}
	return (_B_FALSE);
}

/*
 * Returns true if ok (and *resp updated) and false if failed.
 */
static boolean_t
parse_int(char *str, uint_t *resp)
{
	char *end;
	int res;

	res = strtoul(str, &end, 0);
	if (end == str)
		return (_B_FALSE);
	*resp = res;
	return (_B_TRUE);
}

/*
 * Parse something with a unit of millseconds.
 * Regognizes the suffixes "ms", "s", "m", "h", and "d".
 *
 * Returns true if ok (and *resp updated) and false if failed.
 */
static boolean_t
parse_ms(char *str, uint_t *resp)
{
	/* Look at the last and next to last character */
	char *cp, *last, *nlast;
	char str2[BUFSIZ];	/* For local modification */
	int multiplier = 1;

	(void) strncpy(str2, str, sizeof (str2));
	str2[sizeof (str2) - 1] = '\0';

	last = str2;
	nlast = NULL;
	for (cp = str2; *cp != '\0'; cp++) {
		nlast = last;
		last = cp;
	}
	if (debug & D_PARSE) {
		logdebug("parse_ms: last <%c> nlast <%c>\n",
		    (last != NULL ? *last : ' '),
		    (nlast != NULL ? *nlast : ' '));
	}
	switch (*last) {
	case 'd':
		multiplier *= 24;
		/* FALLTHRU */
	case 'h':
		multiplier *= 60;
		/* FALLTHRU */
	case 'm':
		multiplier *= 60;
		*last = '\0';
		multiplier *= 1000;	/* Convert to milliseconds */
		break;
	case 's':
		/* Could be "ms" or "s" */
		if (nlast != NULL && *nlast == 'm') {
			/* "ms" */
			*nlast = '\0';
		} else {
			*last = '\0';
			multiplier *= 1000;	/* Convert to milliseconds */
		}
		break;
	}

	if (!parse_int(str2, resp))
		return (_B_FALSE);

	*resp *= multiplier;
	return (_B_TRUE);
}

/*
 * Parse something with a unit of seconds.
 * Regognizes the suffixes "s", "m", "h", and "d".
 *
 * Returns true if ok (and *resp updated) and false if failed.
 */
static boolean_t
parse_s(char *str, uint_t *resp)
{
	/* Look at the last character */
	char *cp, *last;
	char str2[BUFSIZ];	/* For local modification */
	int multiplier = 1;

	(void) strncpy(str2, str, sizeof (str2));
	str2[sizeof (str2) - 1] = '\0';

	last = str2;
	for (cp = str2; *cp != '\0'; cp++) {
		last = cp;
	}
	if (debug & D_PARSE) {
		logdebug("parse_s: last <%c>\n",
		    (last != NULL ? *last : ' '));
	}
	switch (*last) {
	case 'd':
		multiplier *= 24;
		/* FALLTHRU */
	case 'h':
		multiplier *= 60;
		/* FALLTHRU */
	case 'm':
		multiplier *= 60;
		/* FALLTHRU */
	case 's':
		*last = '\0';
		break;
	}
	if (!parse_int(str2, resp))
		return (_B_FALSE);

	*resp *= multiplier;
	return (_B_TRUE);
}

/*
 * Return prefixlen (0 to 128) if ok; -1 if failed.
 */
int
parse_addrprefix(char *strin, struct in6_addr *in6)
{
	char str[BUFSIZ];	/* Local copy for modification */
	int prefixlen;
	char *cp;
	char *end;

	(void) strncpy(str, strin, sizeof (str));
	str[sizeof (str) - 1] = '\0';

	cp = strchr(str, '/');
	if (cp == NULL)
		return (-1);
	*cp = '\0';
	cp++;

	prefixlen = strtol(cp, &end, 10);
	if (cp == end)
		return (-1);

	if (prefixlen < 0 || prefixlen > IPV6_ABITS)
		return (-1);

	if (inet_pton(AF_INET6, str, in6) != 1)
		return (-1);

	return (prefixlen);
}

/*
 * Parse an absolute date using a datemsk config file.
 * Return the difference (measured in seconds) between that date/time and
 * the current date/time.
 * If the date has passed return zero.
 *
 * Returns true if ok (and *resp updated) and false if failed.
 * XXX Due to getdate limitations can not exceed year 2038.
 */
static boolean_t
parse_date(char *str, uint_t *resp)
{
	struct tm *tm;
	struct timeval tvs;
	time_t time, ntime;

	if (getenv("DATEMSK") == NULL) {
		(void) putenv("DATEMSK=/etc/inet/datemsk.ndpd");
	}

	if (gettimeofday(&tvs, NULL) < 0) {
		logperror("gettimeofday");
		return (_B_FALSE);
	}
	time = tvs.tv_sec;
	tm = getdate(str);
	if (tm == NULL) {
		logerr("Bad date <%s> (error %d)\n",
		    str, getdate_err);
		return (_B_FALSE);
	}

	ntime = mktime(tm);

	if (debug & D_PARSE) {
		char buf[BUFSIZ];

		(void) strftime(buf, sizeof (buf), "%Y-%m-%d %R %Z", tm);
		logdebug("parse_date: <%s>, delta %d seconds\n", buf,
		    ntime - time);
	}
	if (ntime < time) {
		logerr("Date in the past <%s>\n", str);
		*resp = 0;
		return (_B_TRUE);
	}
	*resp = (ntime - time);
	return (_B_TRUE);
}
