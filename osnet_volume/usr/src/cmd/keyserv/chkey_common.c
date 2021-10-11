/*
 *	chkey_common.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)chkey_common.c	1.15	99/07/07 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <pwd.h>
#include <shadow.h>
#include <netdb.h>
#include <mp.h>
#include <rpcsvc/nis.h>
#include <rpc/key_prot.h>
#include <nsswitch.h>
#include <ns_sldap.h>


extern char *crypt();
extern long random();
extern char *getpass();
extern char *program_name;
static const char *CRED_TABLE = "cred.org_dir";

#define	ROOTKEY_FILE	"/etc/.rootkey"

#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN 256
#endif

#define	PK_FILES	1
#define	PK_YP		2
#define	PK_NISPLUS	3
#define	PK_LDAP		4



/* ************************ switch functions *************************** */

/*	NSW_NOTSUCCESS  NSW_NOTFOUND   NSW_UNAVAIL    NSW_TRYAGAIN */
#define	DEF_ACTION {__NSW_RETURN, __NSW_RETURN, __NSW_CONTINUE, __NSW_CONTINUE}

static struct __nsw_lookup lookup_files = {"files", DEF_ACTION, NULL, NULL},
		lookup_nis = {"nis", DEF_ACTION, NULL, &lookup_files};
static struct __nsw_switchconfig publickey_default =
			{0, "publickey", 2, &lookup_nis};


char *
switch_policy_str(struct __nsw_switchconfig *conf)
{
	struct __nsw_lookup *look;
	static char policy[256];
	int previous = 0;

	memset((char *)policy, 0, 256);

	for (look = conf->lookups; look; look = look->next) {
		if (previous)
			strcat(policy, " ");
		strcat(policy, look->service_name);
		previous = 1;
	}
	return (policy);
}

int
no_switch_policy(struct __nsw_switchconfig *conf)
{
	return (conf == NULL || conf->lookups == NULL);
}

is_switch_policy(struct __nsw_switchconfig *conf, char *target)
{
	return (conf &&
		conf->lookups &&
		strcmp(conf->lookups->service_name, target) == 0 &&
		conf->lookups->next == NULL);
}

char *
first_and_only_switch_policy(char *policy,
		    struct __nsw_switchconfig *default_conf,
		    char *head_msg)
{
	struct __nsw_switchconfig *conf;
	enum __nsw_parse_err perr;
	int policy_correct = 1;
	char *target_service = 0;
	int use_default = 0;

	if (default_conf == 0)
		default_conf = &publickey_default;

	conf = __nsw_getconfig(policy, &perr);
	if (no_switch_policy(conf)) {
		use_default = 1;
		conf = default_conf;
	}

	target_service = conf->lookups->service_name;

	if (conf->lookups->next != NULL) {
		policy_correct = 0;
		if (use_default) {
			(void) fprintf(stderr,
			"\n%s\n There is no publickey entry in %s.\n",
				head_msg, __NSW_CONFIG_FILE);
			(void) fprintf(stderr,
			"The default publickey policy is \"publickey: %s\".\n",
			switch_policy_str(default_conf));
		} else
			(void) fprintf(stderr,
		"\n%s\nThe publickey entry in %s is \"publickey: %s\".\n",
			head_msg, __NSW_CONFIG_FILE,
			switch_policy_str(conf));
	}

	if (policy_correct == 0)
		(void) fprintf(stderr,
	"I cannot figure out which publickey database you want to update.\n");
	if (!use_default && conf)
		__nsw_freeconfig(conf);

	if (policy_correct)
		return (target_service);
	else
		return (0);
}



int
check_switch_policy(char *policy, char *target_service,
		    struct __nsw_switchconfig *default_conf,
		    char *head_msg, char *tail_msg)
{
	struct __nsw_switchconfig *conf;
	enum __nsw_parse_err perr;
	int policy_correct = 1;

	if (default_conf == 0)
		default_conf = &publickey_default;

	conf = __nsw_getconfig(policy, &perr);
	if (no_switch_policy(conf)) {
		if (!is_switch_policy(default_conf, target_service)) {
			(void) fprintf(stderr,
				"\n%s\nThere is no publickey entry in %s.\n",
				head_msg, __NSW_CONFIG_FILE);
			(void) fprintf(stderr,
			"The default publickey policy is \"publickey: %s\".\n",
			switch_policy_str(default_conf));
			policy_correct = 0;
		}
	} else if (!is_switch_policy(conf, target_service)) {
		(void) fprintf(stderr,
		"\n%s\nThe publickey entry in %s is \"publickey: %s\".\n",
			head_msg, __NSW_CONFIG_FILE,
			switch_policy_str(conf));
		policy_correct = 0;
	}
	/* should we exit ? */
	if (policy_correct == 0)
		(void) fprintf(stderr,
		"It should be \"publickey: %s\"%s\n\n",
			target_service, tail_msg);
	if (conf)
		__nsw_freeconfig(conf);

	return (policy_correct);
}

int
get_pk_source(char *pk_service)
{
	int db = 0, got_from_switch = 0;

	/* No service specified, try to figure out from switch */
	if (pk_service == 0) {
		pk_service = first_and_only_switch_policy("publickey", 0,
				"ERROR:");
		if (pk_service == 0)
			return (0);
		(void) fprintf(stdout,
			"Updating %s publickey database.\n",
			pk_service);
		got_from_switch = 1;
	}

	if (strcmp(pk_service, "ldap") == 0)
		db = PK_LDAP;
	else if (strcmp(pk_service, "nisplus") == 0)
		db = PK_NISPLUS;
	else if (strcmp(pk_service, "nis") == 0)
		db = PK_YP;
	else if (strcmp(pk_service, "files") == 0)
		db = PK_FILES;
	else return (0);

	/*
	 * If we didn't get service name from switch, check switch
	 * and print warning about it source of publickeys if not unique
	 */
	if (got_from_switch == 0)
		check_switch_policy("publickey", pk_service, 0, "WARNING:",
				    db == PK_FILES ? "" :
			    "; add 'files' if you want the 'nobody' key.");


	return (db); /* all passed */
}


/* ***************************** keylogin stuff *************************** */
int
keylogin(char *netname, char *secret)
{
	struct key_netstarg netst;

	netst.st_pub_key[0] = 0;
	memcpy(netst.st_priv_key, secret, HEXKEYBYTES);
	netst.st_netname = netname;

#ifdef NFS_AUTH
	nra.authtype = AUTH_DES;	/* only revoke DES creds */
	nra.uid = getuid();		/* use the real uid */
	if (_nfssys(NFS_REVAUTH, &nra) < 0) {
		perror("Warning: NFS credentials not destroyed");
		err = 1;
	}
#endif NFS_AUTH


	/* do actual key login */
	if (key_setnet(&netst) < 0) {
		(void) fprintf(stderr,
			"Could not set %s's secret key\n", netname);
		(void) fprintf(stderr, "May be the keyserv is down?\n");
		return (0);
	}

	return (1);
}

/* write unencrypted secret key into root key file */
write_rootkey(char *secret)
{
	char sbuf[HEXKEYBYTES+2];
	int fd, len;

	strcpy(sbuf, secret);
	strcat(sbuf, "\n");
	len = strlen(sbuf);
	sbuf[len] = '\0';
	unlink(ROOTKEY_FILE);
	if ((fd = open(ROOTKEY_FILE, O_WRONLY+O_CREAT, 0600)) != -1) {
		write(fd, sbuf, len+1);
		close(fd);
		(void) fprintf(stdout, "Wrote secret key into %s\n",
			ROOTKEY_FILE);
	} else {
		(void) fprintf(stderr, "Could not open %s for update\n",
			ROOTKEY_FILE);
		perror(ROOTKEY_FILE);
	}
}

/* ****************************** nisplus stuff ************************* */
/* most of it gotten from nisaddcred */


/* Check that someone else don't have the same auth information already */
static
nis_error
auth_exists(char *princname, char *auth_name, char *auth_type, char *domain)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result	*res;
	nis_error status;
	char *foundprinc;

	(void) sprintf(sname, "[auth_name=%s,auth_type=%s],%s.%s",
		auth_name, auth_type, CRED_TABLE, domain);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");
	/* Don't want FOLLOW_PATH here */
	res = nis_list(sname,
		MASTER_ONLY+USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS,
		NULL, NULL);

	status = res->status;
	switch (res->status) {
	case NIS_NOTFOUND:
		break;
	case NIS_TRYAGAIN :
		(void) fprintf(stderr,
			"%s: NIS+ server busy, try again later.\n",
			program_name);
		exit(1);
	case NIS_PERMISSION :
		(void) fprintf(stderr,
		"%s: insufficient permission to look up old credentials.\n",
			program_name);
		exit(1);
	case NIS_SUCCESS:
		foundprinc = ENTRY_VAL(res->objects.objects_val, 0);
		if (nis_dir_cmp(foundprinc, princname) != SAME_NAME) {
			(void) fprintf(stderr,
	"%s: %s credentials with auth_name '%s' already belong to '%s'.\n",
			program_name, auth_type, auth_name, foundprinc);
			exit(1);
		}
		break;
	default:
		(void) fprintf(stderr,
			"%s: error looking at cred table, NIS+ error: %s\n",
			program_name, nis_sperrno(res->status));
		exit(1);
	}
	nis_freeresult(res);
	return (status);
}

/* Returns 0 if check fails; 1 if successful. */
static int
sanity_checks(nis_princ, netname, domain)
char	*nis_princ,
	*netname,
	*domain;
{
	char	netdomainaux[MAXHOSTNAMELEN+1];
	char	*princdomain, *netdomain;
	int	len;

	/* Sanity check 0. Do we have a nis+ principal name to work with? */
	if (nis_princ == NULL) {
		(void) fprintf(stderr,
		"%s: you must create a \"LOCAL\" credential for '%s' first.\n",
			program_name, netname);
		(void) fprintf(stderr, "\tSee nisaddcred(1).\n");
		return (0);
	}

	/* Sanity check 0.5.  NIS+ principal names must be dotted. */
	len = strlen(nis_princ);
	if (nis_princ[len-1] != '.') {
		(void) fprintf(stderr,
		"%s: invalid principal name: '%s' (forgot ending dot?).\n",
			program_name, nis_princ);
		return (0);
	}

	/* Sanity check 1.  We only deal with one type of netnames. */
	if (strncmp(netname, "unix", 4) != 0) {
		(void) fprintf(stderr,
			"%s: unrecognized netname type: '%s'.\n",
			program_name, netname);
		return (0);
	}

	/* Sanity check 2.  Should only add DES cred in home domain. */
	princdomain = nis_domain_of(nis_princ);
	if (strcasecmp(princdomain, domain) != 0) {
		(void) fprintf(stderr,
"%s: domain of principal '%s' does not match destination domain '%s'.\n",
			nis_princ, domain);
		(void) fprintf(stderr,
	"Should only add DES credential of principal in its home domain\n");
		return (0);
	}

	/*
	 * Sanity check 3:  Make sure netname's domain same as principal's
	 * and don't have extraneous dot at the end.
	 */
	netdomain = (char *)strchr(netname, '@');
	if (! netdomain || netname[strlen(netname)-1] == '.') {
		(void) fprintf(stderr, "%s: invalid netname: '%s'. \n",
			program_name, netname);
		return (0);
	}
	netdomain++; /* skip '@' */
	strcpy(netdomainaux, netdomain);
	if (netdomainaux[strlen(netdomainaux)-1] != '.')
		strcat(netdomainaux, ".");

	if (strcasecmp(princdomain, netdomainaux) != 0) {
		(void) fprintf(stderr,
	"%s: domain of netname %s should be same as that of principal %s\n",
			program_name, netname, nis_princ);
		return (0);
	}

	/* Another principal owns same credentials? (exits if that happens) */
	(void) auth_exists(nis_princ, netname, "DES", domain);

	return (1); /* all passed */
}


/*
 * Similar to nis_local_principal in "nis_subr.c" except
 * this gets the results from the MASTER_ONLY and no FOLLOW_PATH.
 * We only want the master because we'll be making updates there,
 * and also the replicas may not have seen the 'nisaddacred local'
 * that may have just occurred.
 * Returns NULL if not found.
 */
char *
get_nisplus_principal(directory, uid)
char	*directory;
uid_t	uid;
{
	nis_result	*res;
	char		buf[NIS_MAXNAMELEN + 1];
	int		status;
	static	char	principal_name[NIS_MAXNAMELEN + 1];

	if (uid == 0)
		return (nis_local_host());

	(void) sprintf(buf, "[auth_name=%d,auth_type=LOCAL],%s.%s",
		uid, CRED_TABLE, directory);

	if (buf[strlen(buf)-1] != '.')
		strcat(buf, ".");

	res = nis_list(buf,
		MASTER_ONLY+USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS,
		NULL, NULL);

	if (res == NULL) {
		(void) fprintf(stderr,
			"%s: unable to get result from NIS+ server.",
			program_name);
		exit(1);
	}
	switch (res->status) {
	case NIS_SUCCESS:
		if (res->objects.objects_len > 1) {
			/*
			 * More than one principal with same uid?
			 * something wrong with cred table. Should be unique
			 * Warn user and continue.
			 */
			(void) fprintf(stderr,
			"%s: LOCAL entry for %d in directory %s not unique",
				program_name, uid, directory);
		}
		strcpy(principal_name,
			ENTRY_VAL(res->objects.objects_val, 0));
		nis_freeresult(res);
		return (principal_name);

	case NIS_NOTFOUND:
		nis_freeresult(res);
		return (NULL);

	case NIS_TRYAGAIN :
		(void) fprintf(stderr,
			"%s: NIS+ server busy, try again later.\n",
			program_name);
		exit(1);

	case NIS_PERMISSION :
		(void) fprintf(stderr,
			"%s: insufficient permission to update credentials.\n",
			program_name);
		exit(1);

	default:
		(void) fprintf(stderr,
			"%s: error talking to server, NIS+ error: %s.\n",
			program_name, nis_sperrno(res->status));
		exit(1);
	}
	return (NULL);
}



/* Check whether this principal already has this type of credentials */
static nis_error
cred_exists(char *nisprinc, char *flavor, char *domain)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result	*res;
	nis_error status;

	(void) sprintf(sname, "[cname=\"%s\",auth_type=%s],%s.%s",
		nisprinc, flavor, CRED_TABLE, domain);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	/* Don't want FOLLOW_PATH here */
	res = nis_list(sname,
		MASTER_ONLY+USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS,
		NULL, NULL);

	status = res->status;
	switch (status) {
	case NIS_NOTFOUND:
		break;
	case NIS_TRYAGAIN:
		(void) fprintf(stderr,
			"%s: NIS+ server busy, try again later.\n",
			program_name);
		exit(1);
	case NIS_PERMISSION:
		(void) fprintf(stderr,
		"%s: insufficient permission to look at credentials table\n",
			program_name);
		exit(1);
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;
	default:
		(void) fprintf(stderr,
			"%s: error looking at cred table, NIS+ error: %s\n",
			program_name, nis_sperrno(res->status));
		exit(1);
	}
	nis_freeresult(res);
	return (status);
}


/* Returns 0 if operation fails; 1 if successful. */
static
int
modify_cred_obj(obj, domain)
	char *domain;
	nis_object *obj;
{
	int status = 0;
	int flags = 0;
	char sname[NIS_MAXNAMELEN+1];
	nis_result	*res;

	(void) sprintf(sname, "%s.%s", CRED_TABLE, domain);
	res = nis_modify_entry(sname, obj, 0);
	switch (res->status) {
	case NIS_TRYAGAIN :
		(void) fprintf(stderr,
			"%s: NIS+ server busy, try again later.\n",
			program_name);
		exit(1);
	case NIS_PERMISSION :
		(void) fprintf(stderr,
			"%s: insufficient permission to update credentials.\n",
			program_name);
		exit(1);
	case NIS_SUCCESS :
		status = 1;
		break;
	default:
		(void) fprintf(stderr,
			"%s: error modifying credential, NIS+ error: %s.\n",
			program_name, nis_sperrno(res->status));
		exit(1);
	}
	nis_freeresult(res);
	return (status);
}


static
int
add_cred_obj(obj, domain)
	char *domain;
	nis_object *obj;
{
	int status = 0;
	int flags = 0;
	char sname[NIS_MAXNAMELEN+1];
	nis_result	*res;

	/* Assume check for cred_exists performed already */

	(void) sprintf(sname, "%s.%s", CRED_TABLE, domain);
	res = nis_add_entry(sname, obj, 0);
	switch (res->status) {
	case NIS_TRYAGAIN :
		(void) fprintf(stderr,
			"%s: NIS+ server busy, try again later.\n",
			program_name);
		exit(1);
	case NIS_PERMISSION :
		(void) fprintf(stderr,
			"%s: insufficient permission to update credentials.\n",
			program_name);
		exit(1);
	case NIS_SUCCESS :
		status = 1;
		break;
	default:
		(void) fprintf(stderr,
			"%s: error creating credential, NIS+ error: %s.\n",
			program_name, nis_sperrno(res->status));
		exit(1);
	}
	nis_freeresult(res);
	return (status);
}

nis_object *
init_entry()
{
	static nis_object	obj;
	static entry_col	cred_data[10];
	entry_obj		*eo;

	memset((char *)(&obj), 0, sizeof (obj));
	memset((char *)(cred_data), 0, sizeof (entry_col) * 10);

	obj.zo_name = "cred";
	obj.zo_group = "";
	obj.zo_ttl = 43200;
	obj.zo_data.zo_type = NIS_ENTRY_OBJ;
	eo = &(obj.EN_data);
	eo->en_type = "cred_tbl";
	eo->en_cols.en_cols_val = cred_data;
	eo->en_cols.en_cols_len = 5;
	cred_data[4].ec_flags |= EN_CRYPT;
	return (&obj);
}


static char	*attrFilter[] = {
	"objectclass",
	"nispublickey",
	"nissecretkey",
	(char *)NULL
};


/* Determines if there is a NisKeyObject objectclass in a given entry */
static int
ldap_keyobj_exist(ns_ldap_entry_t *entry)
{
	char		**fattrs;

	fattrs = __ns_ldap_getAttr(entry, "objectClass");

	while (fattrs) {
		if (strcasecmp("NisKeyObject", *fattrs) == 0)
			return (1);
		fattrs++;
	}

	return (0);
}


static char *keyAttrs[] = {
	"nispublickey",
	"nissecretkey",
	NULL
};

/*
 * Replace or append new attribute value(s) to an attribute.
 * Don't care about memory leaks, because program is short running.
 */

static int
ldap_attr_mod(ns_ldap_entry_t *entry,
	    char *mechname,
	    char *public,
	    ns_ldap_attr_t **pkeyattrs,
	    char *crypt,
	    ns_ldap_attr_t **ckeyattrs)
{
	char		**alist[2];
	char		*keys[2];

	char		*mechfilter;
	int		mechfilterlen;
	int		q = 0;
	int		i, j;
	int		keycount[] = {0, 0};
	ns_ldap_attr_t	*attrs;

	keys[0] = public;
	keys[1] = crypt;

	mechfilter = (char *)malloc(strlen(mechname) + 3);
	if (mechfilter == NULL)
		return (0);
	sprintf(mechfilter, "{%s}", mechname);
	mechfilterlen = strlen(mechfilter);

	for (q = 0; keyAttrs[q] != NULL; q++) {
		int		found = 0;

		for (i = 0; i < entry->attr_count; i++) {
			int		rep = 0;
			ns_ldap_attr_t	*attr = entry->attr_pair[i];
			char		*name = attr->attrname;
			int		count = 0;

			if (strcasecmp(keyAttrs[q], name) == 0) {
				found++;
				count = attr->value_count;
		alist[q] = (char **)malloc(sizeof (char *) * (count + 1));
				if (alist[q] == NULL)
					return (0);
				alist[q][attr->value_count] = NULL;
				for (j = 0; j < attr->value_count; j++) {
					char	*val = attr->attrvalue[j];
					if (strncasecmp(val, mechfilter,
							mechfilterlen) == 0) {
						/* Replace entry */
						rep++;
						alist[q][j] = keys[q];
					} else
						alist[q][j] = val;
					++keycount[q];
				}
				if (!rep) {
					/* Add entry to list */
					alist[q] = (char **)realloc(alist[q],
					    sizeof (char *) * (count + 2));
					if (alist[q] == NULL)
						return (0);
					alist[q][attr->value_count + 1] = NULL;
					alist[q][attr->value_count] = keys[q];
					++keycount[q];
				}
			}
		}
		if (!found) {
			/* Attribute does not exist, add entry anyways */
			alist[q] = (char **)malloc(sizeof (char *) * 2);
			if (alist[q] == NULL)
				return (0);
			alist[q][0] = keys[q];
			alist[q][1] = NULL;
			++keycount[q];
		}
	}
	if ((attrs = (ns_ldap_attr_t *)calloc(1,
			sizeof (ns_ldap_attr_t))) == NULL)
		return(0);
	attrs->attrname = "nisPublicKey";
	attrs->attrvalue = alist[0];
	attrs->value_count = keycount[0];
	*pkeyattrs = attrs;

	if ((attrs = (ns_ldap_attr_t *)calloc(1,
			sizeof (ns_ldap_attr_t))) == NULL)
		return(0);
	attrs->attrname = "nisSecretKey";
	attrs->attrvalue = alist[1];
	attrs->value_count = keycount[1];
	*ckeyattrs = attrs;
	return (1);
}


/*
 * Update LDAP nisplublickey entry with new key information via SLDAP.
 * Don't care about memory leaks, because program is a short running
 * process.
 */
int
ldap_update(char *mechname,
	    char *netname,
	    char *public,
	    char *crypt,
	    char *passwd)
{
	char		*netnamecpy;
	char		*id;
	char		*domain;
	char		*dn;
	char		*db;
	char		*filter;
	Auth_t		*authp;
	ns_ldap_error_t	*errorp;
	void		**paramVal = NULL;
	char		*ldap_pw;
	char		*pkeyatval, *ckeyatval;
	ns_ldap_result_t	*res;
	char		*msg;
	int		rc;
	ns_ldap_attr_t	*pattrs, *cattrs;

	/* Generate DN */
	if ((netnamecpy = strdup(netname)) == NULL)
		return (0);
	if (((id = strchr(netnamecpy, '.')) == NULL) ||
	    ((domain = strchr(netnamecpy, '@')) == NULL))
		return (0);
	else {
		*domain++ = '\0';
		*id++ = '\0';

		id = strdup(id);
		domain = strdup(domain);
		free(netnamecpy);
	}

	if (isdigit(*id)) {
		/* We be user. */
		__ns_ldap_uid2dn(id, domain, &dn, NULL, &errorp);
		if (dn == NULL) {
			fprintf(stderr, "Could not obtain LDAP dn\n");
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
		db = "passwd";
		filter = (char *)malloc(strlen(id) + 13);
		if (filter)
			sprintf(filter, "(uidnumber=%s)", id);
		else {
			fprintf(stderr, "Can not allocate filter buffer.\n");
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
	} else {
		/* We be host. */
		__ns_ldap_host2dn(id, domain, &dn, NULL, &errorp);
		if (dn == NULL) {
			fprintf(stderr, "Could not obtain LDAP dn\n");
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}

		db = "hosts";
		filter = (char *)malloc(strlen(id) + 6);
		if (filter)
			sprintf(filter, "(cn=%s)", id);
		else {
			fprintf(stderr, "Can not allocate filter buffer.\n");
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
	}

	/* Generate Auth Structure */
	if ((authp = (Auth_t *)calloc(1, sizeof (Auth_t))) == NULL) {
		fprintf(stderr, "Can not allocate LDAP Auth structure.\n");
		fprintf(stderr, "%s: key-pair(s) unchanged.\n", program_name);
		exit(1);
	}

	__ns_ldap_getParam(NULL, NS_LDAP_AUTH_P, &paramVal, &errorp);
	if (paramVal) {
		authp->type = *(int *)(*paramVal);
		(void) __ns_ldap_freeParam(&paramVal);

		if (authp->type == NS_LDAP_AUTH_SIMPLE ||
		    authp->type == NS_LDAP_AUTH_SASL_CRAM_MD5) {
			if (__ns_ldap_getParam(NULL,
					    NS_LDAP_TRANSPORT_SEC_P,
					    &paramVal, &errorp) !=
			    NS_LDAP_SUCCESS) {
				fprintf(stderr,
					"LDAP configuration error (auth)\n");
				fprintf(stderr, "%s: key-pair(s) unchanged.\n",
					program_name);
				exit(1);
			}

			if (paramVal)
				authp->security = *(int *)(*paramVal);
			else {
				fprintf(stderr,
					"LDAP configuration error (auth)\n");
				fprintf(stderr, "%s: key-pair(s) unchanged.\n",
					program_name);
				exit(1);
			}

			authp->cred.unix_cred.userID = strdup(dn);
			if (passwd)
				authp->cred.unix_cred.passwd = strdup(passwd);
			else {
				char	*prompt =
					"Please enter LDAP bind password:";

				ldap_pw = getpass(prompt);
				if (!ldap_pw) {
					fprintf(stderr,
						"%s: key-pair(s) unchanged.\n",
						program_name);
					exit(1);
				}
				authp->cred.unix_cred.passwd = ldap_pw;
			}
		}
	} else {
		fprintf(stderr, "LDAP memory error (auth)\n");
		fprintf(stderr, "%s: key-pair(s) unchanged.\n", program_name);
		exit(1);
	}

	/* Construct attribute values */
	pkeyatval = (char *)malloc(strlen(mechname) + strlen(public) + 3);
	if (pkeyatval == NULL) {
		fprintf(stderr, "LDAP memory error (pkeyatval)\n");
		fprintf(stderr, "%s: key-pair(s) unchanged.\n", program_name);
		exit(1);
	}
	sprintf(pkeyatval, "{%s}%s", mechname, public);
	ckeyatval = (char *)malloc(strlen(mechname) + strlen(crypt) + 3);
	if (ckeyatval == NULL) {
		fprintf(stderr, "LDAP memory error (pkeyatval)\n");
		fprintf(stderr, "%s: key-pair(s) unchanged.\n", program_name);
		exit(1);
	}
	sprintf(ckeyatval, "{%s}%s", mechname, crypt);

	/* Does entry exist? */
	if ((__ns_ldap_list(db, filter, (const char **)attrFilter,
			    NULL, NULL, 0, &res, &errorp,
			    NULL, NULL) == NS_LDAP_SUCCESS) && res == NULL) {
		fprintf(stderr, "LDAP entry does not exist.\n");
		fprintf(stderr, "%s: key-pair(s) unchanged.\n", program_name);
		exit(1);
	}

	/* Entry exists, modify attributes for public and secret keys */

	/* Is there a NisKeyObject in entry? */
	if (!ldap_keyobj_exist(&res->entry[0])) {
		/* Add NisKeyObject objectclass and the keys */
		char	**newattr;
		ns_ldap_attr_t	*attrs[4]; /* objectclass, pk, sk, NULL */

		/* set objectclass */
		newattr = (char **)calloc(2, sizeof (char *));
		newattr[0] = "NisKeyObject";
		newattr[1] = NULL;
		if ((attrs[0] = (ns_ldap_attr_t *)calloc(1,
				sizeof (ns_ldap_attr_t))) == NULL) {
			fprintf(stderr, "Memory allocation failed\n");	
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
		attrs[0]->attrname = "objectClass";
		attrs[0]->attrvalue = newattr;
		attrs[0]->value_count = 1;

		/* set publickey */
		newattr = (char **)calloc(2, sizeof (char *));
		newattr[0] = pkeyatval;
		newattr[1] = NULL;
		if ((attrs[1] = (ns_ldap_attr_t *)calloc(1,
				sizeof (ns_ldap_attr_t))) == NULL) {
			fprintf(stderr, "Memory allocation failed\n");	
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
		attrs[1]->attrname = "nisPublicKey";
		attrs[1]->attrvalue = newattr;
		attrs[1]->value_count = 1;

		/* set privatekey */
		newattr = (char **)calloc(2, sizeof (char *));
		newattr[0] = ckeyatval;
		newattr[1] = NULL;
		if ((attrs[2] = (ns_ldap_attr_t *)calloc(1,
				sizeof (ns_ldap_attr_t))) == NULL) {
			fprintf(stderr, "Memory allocation failed\n");	
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
		attrs[2]->attrname = "nisSecretKey";
		attrs[2]->attrvalue = newattr;
		attrs[2]->value_count = 1;

		/* terminator */
		attrs[3] = NULL;

		rc = __ns_ldap_addAttr(dn,
				    (const ns_ldap_attr_t * const *)attrs,
				    authp,
				    NULL,
				    &errorp);
		if (rc != NS_LDAP_SUCCESS) {
			__ns_ldap_err2str(rc, &msg);
			fprintf(stderr, "LDAP objectclass update error: %s\n",
				msg);
			fprintf(stderr, "%s: key-pair(s) unchanged.\n",
				program_name);
			exit(1);
		}
	} else {
		/* object class already exists, replace keys */
		ns_ldap_attr_t	*attrs[4]; /* objectclass, pk, sk, NULL */

		if (!ldap_attr_mod(&res->entry[0], mechname,
				pkeyatval, &pattrs,
				ckeyatval, &cattrs)) {
			fprintf(stderr,
				"Could not generate LDAP attribute list.\n");
			fprintf(stderr,
				"%s: key-pair(s) unchanged.\n", program_name);
			exit(1);
		}

		attrs[0] = pattrs;
		attrs[1] = cattrs;
		attrs[2] = NULL;

		rc = __ns_ldap_repAttr(dn,
				(const ns_ldap_attr_t * const *)attrs,
				authp, NULL, &errorp);
		if (rc != NS_LDAP_SUCCESS) {
			__ns_ldap_err2str(rc, &msg);
			fprintf(stderr,
				"LDAP publickey update error: %s\n", msg);
			fprintf(stderr,
				"%s: key-pair(s) unchanged.\n", program_name);
			exit(1);
		}
	}
	return (0);
}


/* Returns 0 if successful; -1 if failure. (expected by setpublicmap). */

int
nisplus_update(char *netname, char *public, char *secret, nis_name nis_princ)
{
	nis_object	*obj = init_entry();
	char	*domain, *netdomain;
	char	netdomainaux[MAXHOSTNAMELEN + 1];
	int status, addition;

	/*
	 * we take the domain given in the netname & the principal
	 * name if they match otherwise the local domain.
	 */

	netdomain = (char *)strchr(netname, '@');
	if (! netdomain) {
		(void) fprintf(stderr, "%s: invalid netname: '%s'. \n",
			program_name, netname);
		return (0);
	}
	netdomain++; /* skip '@' */
	strcpy(netdomainaux, netdomain);
	if (netdomainaux[strlen(netdomainaux) - 1] != '.')
		strcat(netdomainaux, ".");

	domain = nis_domain_of(nis_princ);
	if (strcasecmp(domain, netdomainaux) != 0)
		domain = nis_local_directory();

	if (sanity_checks(nis_princ, netname, domain) == 0)
		return (-1);

	addition = (cred_exists(nis_princ, "DES", domain) == NIS_NOTFOUND);

	/* Now we have a key pair, build up the cred entry */
	ENTRY_VAL(obj, 0) = nis_princ;
	ENTRY_LEN(obj, 0) = strlen(nis_princ) + 1;

	ENTRY_VAL(obj, 1) = "DES";
	ENTRY_LEN(obj, 1) = 4;

	ENTRY_VAL(obj, 2) = netname;
	ENTRY_LEN(obj, 2) = strlen(netname) + 1;

	ENTRY_VAL(obj, 3) = public;
	ENTRY_LEN(obj, 3) = strlen(public) + 1;

	ENTRY_VAL(obj, 4) = secret;
	ENTRY_LEN(obj, 4) = strlen(secret) + 1;

	if (addition) {
		obj->zo_owner = nis_princ;
		obj->zo_group = nis_local_group();
		obj->zo_domain = domain;
		/* owner: r, group: rmcd */
		obj->zo_access = ((NIS_READ_ACC<<16)|
				(NIS_READ_ACC|NIS_MODIFY_ACC|NIS_CREATE_ACC|
					NIS_DESTROY_ACC)<<8);
		status = add_cred_obj(obj, domain);
	} else {
		obj->EN_data.en_cols.en_cols_val[3].ec_flags |= EN_MODIFIED;
		obj->EN_data.en_cols.en_cols_val[4].ec_flags |= EN_MODIFIED;
		status = modify_cred_obj(obj, domain);
	}

	return (status == 1 ? 0 : -1);
}
