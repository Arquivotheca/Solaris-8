/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)ldaplist.c 1.2     99/10/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <strings.h>
#include "../../../lib/libsldap/common/ns_sldap.h"

extern char *set_filter(char **, char *);
extern char *set_filter_publickey(char **, char *, int);
extern void _printResult(ns_ldap_result_t *);

int listflag = 0;

void
usage(char *msg) {
	if (msg)
		fprintf(stderr, "%s\n", msg);

	fprintf(stderr,
	gettext(
	"ldaplist [-lvh] [<database> [<key>] ...]\n"
	"\tOptions:\n"
	"\t    -l list all the attributes found in entry.\n"
	"\t       By default, it lists only the DNs.\n"
	"\t    -d list attributes for the database instead of its entries\n"
	"\t    -v print out the LDAP search filter.\n"
	"\t    -h list the database mappings.\n"
	"\t<database> is the database to be searched in.  Standard system\n"
	"\tdatabases are:\n"
	"\t\tpassword, group, hosts, ethers, networks, netmasks,\n"
	"\t\trpc, bootparams, protocols, services, netgroup, auto_*.\n"
	"\tNon-standard system databases can be specified as follows:\n"
	"\t\tby specific container: ou=<dbname> or\n"
	"\t\tby default container: <dbname>.  In this case, 'nismapname'\n"
	"\t\twill be used, thus mapping this to nismapname=<dbname>.\n"
	"\t<key> is the key to search in the database.  For the standard\n"
	"\tdatabases, the search type for the key is predefined.  You can\n"
	"\toverride this by specifying <type>=<key>.\n"));
	exit(1);
}


/* returns 0=success, 1=error */
int
list(char *database, char *ldapfilter, char **ldapattribute, char **err)
{
	ns_ldap_result_t	*result;
	ns_ldap_error_t	*errorp;
	int		rc;
	char		buf[500];

	*err = NULL;
	buf[0] = '\0';
	rc = __ns_ldap_list(database, (const char *)ldapfilter,
		(const char **)ldapattribute, NULL, NULL, listflag,
		&result, &errorp, NULL, NULL);
	if (rc != NS_LDAP_SUCCESS) {
		char *p;
		(void) __ns_ldap_err2str(rc, &p);
		if (errorp && errorp->message) {
			sprintf(buf, "%s (%s)", p, errorp->message);
			__ns_ldap_freeError(&errorp);
		} else
			sprintf(buf, "%s", p);
		*err = strdup(buf);
		return (rc);
	}

	_printResult(result);
	__ns_ldap_freeResult(&result);
	return (0);
}


int
switch_err(int rc)
{
	switch (rc) {
	case NS_LDAP_SUCCESS:
		return (0);
	case NS_LDAP_NOTFOUND:
		return (1);
	}
	return (2);
}

main(int argc, char **argv)
{

	extern char *optarg;
	extern int optind;
	char	*database = NULL;
	char	*ldapfilter = NULL;
	char	*attribute = "dn";
	char	**key = NULL;
	char	**ldapattribute = NULL;
	char 	*buffer[100];
	char	*err = NULL;
	char	*p;
	int	index = 1;
	int	c, repeat = 1;
	int	rc;
	int	verbose = 0;

	while ((c = getopt(argc, argv, "dhvl")) != EOF) {
		switch (c) {
		case 'd':
			listflag |= NS_LDAP_SCOPE_BASE;
			break;
		case 'h':
			printMapping();
			exit(0);
		case 'l':
			attribute = "NULL";
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage("Invalid option");
		}
	}
	if ((c = argc - optind) > 0)
		database = argv[optind++];
	if ((--c) > 0)
		key = &argv[optind];

	/* build the attribute array */
	if (strncasecmp(attribute, "NULL", 4) == 0)
		ldapattribute = NULL;
	else {
		buffer[0] = strdup(attribute);
		while ((p = strchr(attribute, ',')) != NULL) {
			buffer[index++] = attribute = p + 1;
			*p = '\0';
		}
		buffer[index] = NULL;
		ldapattribute = buffer;
	}

	/* build the filter */
	if (database && (strcasecmp(database, "publickey") == NULL)) {
		/* user publickey lookup */
		char *err1 = NULL;
		int  rc1;

		rc = rc1 = -1;
		ldapfilter = set_filter_publickey(key, database, 0);
		if (ldapfilter) {
			if (verbose) {
				fprintf(stdout, gettext("+++ database=%s\n"),
					(database ? database : "NULL"));
				fprintf(stdout, gettext("+++ filter=%s\n"),
					(ldapfilter ? ldapfilter : "NULL"));
			}
			rc = list("passwd", ldapfilter, ldapattribute, &err);
		}
		/* hosts publickey lookup */
		ldapfilter = set_filter_publickey(key, database, 1);
		if (ldapfilter) {
			if (verbose) {
				fprintf(stdout, gettext("+++ database=%s\n"),
					(database ? database : "NULL"));
				fprintf(stdout, gettext("+++ filter=%s\n"),
					(ldapfilter ? ldapfilter : "NULL"));
			}
			rc1 = list("hosts", ldapfilter, ldapattribute, &err1);
		}
		if (rc == -1 && rc1 == -1) {
			/* this should never happen */
			fprintf(stderr,
			    gettext("ldaplist: invalid publickey lookup\n"));
			rc = 2;
		} if (rc != 0 && rc1 != 0) {
			fprintf(stderr, "ldaplist: %s\n", (err ? err : err1));
			if (rc == -1)
				rc = rc1;
		} else
			rc = 0;
		exit(switch_err(rc));
	}

	/*
	 * we set the search filter to (objectclass=*) when we want
	 * to list the directory attribute instead of the entries
	 * (the -d option).
	 */
	if (((ldapfilter = set_filter(key, database)) == NULL) ||
			(listflag == NS_LDAP_SCOPE_BASE))
		ldapfilter = "objectclass=*";

	if (verbose) {
		fprintf(stdout, gettext("+++ database=%s\n"),
			(database ? database : "NULL"));
		fprintf(stdout, gettext("+++ filter=%s\n"),
			(ldapfilter ? ldapfilter : "NULL"));
	}
	if (rc = list(database, ldapfilter, ldapattribute, &err))
		fprintf(stderr, gettext("ldaplist: %s\n"), err);
	exit(switch_err(rc));
}
