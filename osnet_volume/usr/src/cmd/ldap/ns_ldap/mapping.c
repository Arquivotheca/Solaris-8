/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)mapping.c 1.2     99/10/06 SMI"

#include <ctype.h>
#include <libintl.h>
#include <strings.h>
#include <stdio.h>


#define	MAXLINE	2000
#define	SAME	0

struct mapping {
	char *database;
	char *def_type;
	char *objectclass;
	char *actual_db;
};

#define	AUTOMOUNTER	0
#define	PUBLICKEY	1

static struct mapping maplist[] = {
	{"automount", "cn", "nisObject", NULL},
	{"publickey", "uidnumber", "niskeyobject", "passwd"},
	{"publickey", "cn", "niskeyobject", "host"},
	{"bootparams", "cn", "bootableDevice", NULL},
	{"ethers", "cn", "ieee802Device", NULL},
	{"group", "cn", "posixgroup", NULL},
	{"hosts", "cn", "iphost", NULL},
	{"ipnodes", "cn", "iphost", NULL},
	{"netgroup", "cn", "nisnetgroup", NULL},
	{"netmasks", "ipnetworknumber", "ipnetwork", NULL},
	{"networks", "ipnetworknumber", "ipnetwork", NULL},
	{"passwd", "uid", "posixaccount", NULL},
	{"protocols", "cn", "ipprotocol", NULL},
	{"rpc", "cn", "oncrpc", NULL},
	{"services", "cn", "ipservice", NULL},
	{"aliases", "cn", "mailGroup", NULL},
	{NULL, NULL, NULL, NULL}
};


void
printMapping()
{
	int	i;

	fprintf(stdout,
		gettext("database       default type        objectclass\n"));
	fprintf(stdout,
		gettext("=============  =================   =============\n"));
	for (i = 0; maplist[i].database != NULL; i++) {
		fprintf(stdout, "%-15s%-20s%s\n", maplist[i].database,
			maplist[i].def_type, maplist[i].objectclass);
	}
}


char *
set_keys(char **key, char *attrtype)
{
	char	*keyeq = NULL;
	static char	keyfilter[MAXLINE];
	char	typeeq[100];
	char	buf[100];
	char	*k, **karray;

	if (!key || !key[0])	/* should never contain NULL string */
		return (NULL);

	if (attrtype) {
		strcpy(typeeq, attrtype);
		strcat(typeeq, "=");
	}

	keyfilter[0] = '\0';
	if (key[1])
		strcat(keyfilter, "(|");
	karray = key;
	while (k = *karray) {
		keyeq = strchr(k, '=');
		sprintf(buf, "(%s%s)", (keyeq ? "" : typeeq), k);
		if (strlen(buf) + strlen(keyfilter) >= MAXLINE) {
			fprintf(stdout,
				gettext("***ERROR: ldapfilter too long\n"));
			exit(2);
		}
		strcat(keyfilter, buf);
		karray++;
	}
	if (key[1])
		strcat(keyfilter, ")");
	return (keyfilter);
}


/*
 * A special set_key routine for to handle public keys.
 * If the key starts with a digiti, view it as a user id.
 * Otherwise, view it as a hostname.
 * It returns: -1 no keys defined, 0 key defined but none for type
 *		specified, n>0 number of matches found.
 */
int
set_keys_publickey(char **key, char *attrtype, int type, char **ret)
{
	char	*keyeq = NULL;
	static char	keyfilter[MAXLINE];
	char	pre_filter[MAXLINE];
	char	buf[100];
	char	*k, **karray;
	int	count = 0;

	if (!key || !key[0]) {	/* should never contain NULL string */
		*ret = NULL;
		return (-1);
	}

	keyfilter[0] = '\0';
	pre_filter[0] = '\0';
	karray = key;
	while (k = *karray) {
		keyeq = strchr(k, '=');
		if (keyeq)
			sprintf(buf, "(%s)", k);
		else {
			if (type == 0 && isdigit(*k)) {
				/* user type keys */
				sprintf(buf, "(%s=%s)", attrtype, k);
			} else if (type == 1 && (!isdigit(*k))) {
				/* hosts type keys */
				sprintf(buf, "(%s=%s)", attrtype, k);
			} else {
				karray++;
				continue;
			}
		}
		if (strlen(buf) + strlen(pre_filter) >= MAXLINE) {
			fprintf(stdout,
				gettext("***ERROR: ldapfilter too long\n"));
			exit(2);
		}
		strcat(pre_filter, buf);
		karray++;
		count++;
	}
	if (count > 1) {
		if (strlen(pre_filter) + 4 >= MAXLINE) {
			fprintf(stdout,
				gettext("***ERROR: ldapfilter too long\n"));
			exit(2);
		}
		strcat(keyfilter, "(|");
		strcat(keyfilter, pre_filter);
		strcat(keyfilter, ")");
		*ret = keyfilter;
	} else
		*ret = pre_filter;
	return (count);
}

/*
 * publickey specific set_filter
 * type 0 -> check for user publickeys
 * type 1 -> check for hosts publickeys
 */
char *
set_filter_publickey(char **key, char *database, int type)
{
	static	char filter[MAXLINE];
	char	*keyfilter;
	int	i, rc;

	filter[0] = '\0';
	if (!database) {
		return (NULL);
	}
	if (strcasecmp(database, maplist[PUBLICKEY].database) == SAME) {
		rc = set_keys_publickey(key,
				maplist[PUBLICKEY + type].def_type, type,
				&keyfilter);
		switch (rc) {
		case -1:
			sprintf(filter, "objectclass=%s",
				maplist[PUBLICKEY].objectclass);
			break;
		case 0:
			return (NULL);
		default:
			sprintf(filter, "(&(objectclass=%s)%s)",
				maplist[PUBLICKEY].objectclass, keyfilter);
		}
	} else {
		if ((keyfilter = set_keys(key, "cn")) == NULL)
			sprintf(filter, "objectclass=*");
		else
			sprintf(filter, "%s", keyfilter);
	}
#ifdef DEBUG
	fprintf(stdout, "set_filter: filter=\"%s\"\n", filter);
#endif DEBUG
	return (filter);
}


/* generic set_filter */
char *
set_filter(char **key, char *database)
{
	static	char filter[MAXLINE];
	char	*keyfilter;
	int	i;

	filter[0] = '\0';
	if (!database) {
		return (NULL);
	}
	/*
	 * starts at 3 to skip over automounter and publickey databases.
	 * These databases will be handled separately later.
	 */
	for (i = 3; maplist[i].database != NULL; i++) {
		if (strcasecmp(database, maplist[i].database) == SAME) {
			if ((keyfilter = set_keys(key, maplist[i].def_type))
							== NULL)
				sprintf(filter, "objectclass=%s",
					maplist[i].objectclass);
			else
				sprintf(filter, "(&(objectclass=%s)%s)",
					maplist[i].objectclass, keyfilter);
#ifdef DEBUG
	fprintf(stdout, "set_filter: filter=\"%s\"\n", filter);
#endif DEBUG
			return (filter);
		}
	}

	/* special cases for automounter and publickey databases */
	if (strncasecmp(database, "auto_", 5) == SAME) {
		if ((keyfilter = set_keys(key, maplist[AUTOMOUNTER].def_type))
					!= NULL) {
			sprintf(filter, "(&(objectclass=%s)(nisMapName=%s)%s)",
			maplist[AUTOMOUNTER].objectclass, database, keyfilter);
		} else {
			sprintf(filter, "(&(objectclass=%s)(nisMapName=%s))",
			maplist[AUTOMOUNTER].objectclass, database);
		}
	} else {
		if ((keyfilter = set_keys(key, "cn")) == NULL)
			sprintf(filter, "objectclass=*");
		else
			sprintf(filter, "%s", keyfilter);
	}

#ifdef DEBUG
	fprintf(stdout, "set_filter: filter=\"%s\"\n", filter);
#endif DEBUG
	return (filter);
}
