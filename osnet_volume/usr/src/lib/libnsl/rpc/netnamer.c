/*
 * ==== hack-attack:  possibly MT-safe but definitely not MT-hot.
 * ==== turn this into a real switch frontend and backends
 *
 * Well, at least the API doesn't involve pointers-to-static.
 */

#ident	"@(#)netnamer.c	1.25	99/07/01 SMI"
/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)netnamer.c 1.4 89/03/20 Copyr 1986 Sun Micro";
#endif

/*
 * netname utility routines convert from netnames to unix names (uid, gid)
 *
 * This module is operating system dependent!
 * What we define here will work with any unix system that has adopted
 * the Sun NIS domain architecture.
 */

#undef NIS
#include "rpc_mt.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>
#include <nsswitch.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/ypclnt.h>

static const char    OPSYS[]	= "unix";
static const char    NETIDFILE[] = "/etc/netid";
static const char    NETID[]	= "netid.byname";
static const char    PKTABLE[]  = "cred.org_dir";
#define	PKTABLE_LEN 12
#define	OPSYS_LEN 4

#ifndef NGROUPS
#define	NGROUPS 16
#endif

/*
 * the value for NOBODY_UID is set by the SVID. The following define also
 * appears in netname.c
 */

#define	NOBODY_UID 60001

/*
 *	default publickey policy:
 *		publickey: nis [NOTFOUND = return] files
 */


/*		NSW_NOTSUCCESS  NSW_NOTFOUND   NSW_UNAVAIL    NSW_TRYAGAIN */
#define	DEF_ACTION {__NSW_RETURN, __NSW_RETURN, __NSW_CONTINUE, __NSW_CONTINUE}

static struct __nsw_lookup lookup_files = {"files", DEF_ACTION, NULL, NULL},
		lookup_nis = {"nis", DEF_ACTION, NULL, &lookup_files};
static struct __nsw_switchconfig publickey_default =
			{0, "publickey", 2, &lookup_nis};

static mutex_t serialize_netname_r = DEFAULTMUTEX;

struct netid_userdata {
	uid_t	*uidp;
	gid_t	*gidp;
	int	*gidlenp;
	gid_t	*gidlist;
};

static int
parse_uid(s, argp)
	char *s;
	struct netid_userdata *argp;
{
	uid_t	u;

	trace1(TR_parse_uid, 0);

	if (!s || !isdigit(*s)) {
		syslog(LOG_ERR,
			"netname2user: expecting uid '%s'", s);
		trace1(TR_parse_uid, 1);
		return (__NSW_NOTFOUND); /* xxx need a better error */
	}

	/* Fetch the uid */
	u = (uid_t)(atoi(s));

	if (u == 0) {
		syslog(LOG_ERR, "netname2user: should not have uid 0");
		trace1(TR_parse_uid, 1);
		return (__NSW_NOTFOUND);
	}
	*(argp->uidp) = u;
	trace1(TR_parse_uid, 1);
	return (__NSW_SUCCESS);
}


/* parse a comma separated gid list */
static int
parse_gidlist(p, argp)
	char *p;
	struct netid_userdata *argp;
{
	int len;
	gid_t	g;

	trace1(TR_parse_gidlist, 0);
	if (! p || (! isdigit(*p))) {
		syslog(LOG_ERR,
			"netname2user: missing group id list in '%s'.",
			p);
		trace1(TR_parse_gidlist, 1);
		return (__NSW_NOTFOUND);
	}

	g = (gid_t)(atoi(p));
	*(argp->gidp) = g;

	len = 0;
	while (p = strchr(p, ','))
		argp->gidlist[len++] = (gid_t) atoi(++p);
	*(argp->gidlenp) = len;
	trace1(TR_parse_gidlist, 1);
	return (__NSW_SUCCESS);
}


/*
 * parse_netid_str()
 *
 * Parse uid and group information from the passed string.
 *
 * The format of the string passed is
 * 	uid:gid,grp,grp, ...
 *
 */
static int
parse_netid_str(s, argp)
	char	*s;
	struct netid_userdata *argp;
{
	register char	*p;
	int	err;

	trace1(TR_parse_netid_str, 0);

	/* get uid */
	err = parse_uid(s, argp);
	if (err != __NSW_SUCCESS) {
		trace1(TR_parse_netid_str, 1);
		return (err);
	}


	/* Now get the group list */
	p = strchr(s, ':');
	if (!p) {
		syslog(LOG_ERR,
			"netname2user: missing group id list in '%s'",
			s);
		trace1(TR_parse_netid_str, 1);
		return (__NSW_NOTFOUND);
	}
	++p;			/* skip ':' */
	err = parse_gidlist(p, argp);
	trace1(TR_parse_netid_str, 1);
	return (err);
}

static int
parse_uid_gidlist(ustr, gstr, argp)
	char *ustr;
	char *gstr;
	struct netid_userdata *argp;
{
	int	err;

	trace1(TR_parse_uid_gidlist, 0);

	/* get uid */
	err = parse_uid(ustr, argp);
	if (err != __NSW_SUCCESS) {
		trace1(TR_parse_uid_gidlist, 1);
		return (err);
	}

	/* Now get the group list */
	err = parse_gidlist(gstr, argp);
	trace1(TR_parse_uid_gidlist, 1);
	return (err);
}


/*
 * netname2user_files()
 *
 * This routine fetches the netid information from the "files" nameservice.
 * ie /etc/netid.
 */
static int
netname2user_files(err, netname, argp)
int *err;
char *netname;
struct netid_userdata *argp;
{
	char 	buf[512];	/* one line from the file */
	char	*name;
	char	*value;
	char 	*res;
	FILE 	*fd;

	trace1(TR_netname2user_files, 0);
	fd = fopen(NETIDFILE, "r");
	if (fd == (FILE *) 0) {
		*err = __NSW_UNAVAIL;
		trace1(TR_netname2user_files, 1);
		return (0);
	}
	/*
	 * for each line in the file parse it appropriately
	 * file format is :
	 *	netid	uid:grp,grp,grp # for users
	 *	netid	0:hostname	# for hosts
	 */
	while (! feof(fd)) {
		res = fgets(buf, 512, fd);
		if (res == NULL)
			break;

		/* Skip comments and blank lines */
		if ((*res == '#') || (*res == '\n'))
			continue;

		name = &(buf[0]);
		while (isspace(*name))
			name++;
		if (*name == '\0')	/* blank line continue */
			continue;
		value = name;		/* will contain the value eventually */
		while (! isspace(*value))
			value++;
		if (*value == '\0') {
			syslog(LOG_WARNING,
				"netname2user: badly formatted line in %s.",
				NETIDFILE);
			continue;
		}
		*value++ = '\0'; /* nul terminate the name */

		if (strcasecmp(name, netname) == 0) {
			fclose(fd);
			while (isspace(*value))
				value++;
			*err = parse_netid_str(value, argp);
			trace1(TR_netname2user_files, 1);
			return (*err == __NSW_SUCCESS);
		}
	}
	fclose(fd);
	*err = __NSW_NOTFOUND;
	trace1(TR_netname2user_files, 1);
	return (0);
}

/*
 * netname2user_nis()
 *
 * This function reads the netid from the NIS (YP) nameservice.
 */
static int
netname2user_nis(err, netname, argp)
	int *err;
	char *netname;
	struct netid_userdata *argp;
{
	char *domain;
	int yperr;
	char *lookup;
	int len;

	trace1(TR_netname2user_nis, 0);
	domain = strchr(netname, '@');
	if (! domain) {
		*err = __NSW_UNAVAIL;
		trace1(TR_netname2user_nis, 1);
		return (0);
	}

	/* Point past the '@' character */
	domain++;
	lookup = NULL;
	yperr = yp_match(domain, (char *) NETID, netname, strlen(netname),
			&lookup, &len);
	switch (yperr) {
		case 0:
			break; /* the successful case */

		default :
			/*
			 *  XXX not sure about yp_match semantics.
			 * should err be set to NOTFOUND here?
			 */
			*err = __NSW_UNAVAIL;
			trace1(TR_netname2user_nis, 1);
			return (0);
	}
	if (lookup) {
		lookup[len] = '\0';
		*err = parse_netid_str(lookup, argp);
		free(lookup);
		trace1(TR_netname2user_nis, 1);
		return (*err == __NSW_SUCCESS);
	} else {
		trace1(TR_netname2user_nis, 1);
		*err = __NSW_NOTFOUND;
		return (0);
	}
}

/*
 * Obtain user information (uid, gidlist) from nisplus.
 * What we're trying to do here is to map a netname into
 * local unix information (uid, gids), relevant in
 * the *local* domain.
 *
 *	 cname   auth_type auth_name public  private
 * ----------------------------------------------------------
 *	nisname   DES     netname   pubkey  prikey
 *	nisname   LOCAL   uid       gidlist
 *
 * 1.  Find out which 'home' domain to look for user's DES entry.
 *	This is gotten from the domain part of the netname.
 * 2.  Get the nisplus principal name from the DES entry in the cred
 *	table of user's home domain.
 * 3.  Use the nisplus principal name and search in the cred table of
 *	the *local* directory for the LOCAL entry.
 *
 * Note that we need this translation of netname to <uid,gidlist> to be
 * secure, so we *must* use authenticated connections.
*/
static int
netname2user_nisplus(err, netname, argp)
	int *err;
	char *netname;
	struct netid_userdata *argp;
{
	char *domain;
	nis_result *res;
	char	sname[NIS_MAXNAMELEN+1]; /*  search criteria + table name */
	char	principal[NIS_MAXNAMELEN+1];
	int len;

	trace1(TR_netname2user_nisplus, 0);


	/* 1.  Get home domain of user. */
	domain = strchr(netname, '@');
	if (! domain) {
		*err = __NSW_UNAVAIL;
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}
	domain++;  /* skip '@' */


	/* 2.  Get user's nisplus principal name.  */
	if ((strlen(netname)+strlen(domain)+PKTABLE_LEN+32) >
		(size_t) NIS_MAXNAMELEN) {
		*err = __NSW_UNAVAIL;
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}
	sprintf(sname, "[auth_name=\"%s\",auth_type=DES],%s.%s",
		netname, PKTABLE, domain);
	if (sname[strlen(sname) - 1] != '.')
		strcat(sname, ".");

	/* must use authenticated call here */
	/* XXX but we cant, for now. XXX */
	res = nis_list(sname, USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS+FOLLOW_PATH,
	    NULL, NULL);
	switch (res->status) {
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;   /* go and do something useful */
	case NIS_NOTFOUND:
	case NIS_PARTIAL:
	case NIS_NOSUCHNAME:
	case NIS_NOSUCHTABLE:
		*err = __NSW_NOTFOUND;
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	case NIS_S_NOTFOUND:
	case NIS_TRYAGAIN:
		*err = __NSW_TRYAGAIN;
		syslog(LOG_ERR,
			"netname2user: (nis+ lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	default:
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR, "netname2user: (nis+ lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}

	if (res->objects.objects_len > 1) {
		/*
		 * A netname belonging to more than one principal?
		 * Something wrong with cred table. should be unique.
		 * Warn user and continue.
		 */
		syslog(LOG_ALERT,
			"netname2user: DES entry for %s in \
			directory %s not unique",
			netname, domain);
	}

	len = ENTRY_LEN(res->objects.objects_val, 0);
	strncpy(principal, ENTRY_VAL(res->objects.objects_val, 0), len);
	principal[len] = '\0';
	nis_freeresult(res);

	if (principal[0] == '\0') {
		*err = __NSW_UNAVAIL;
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}

	/*
	 *	3.  Use principal name to look up uid/gid information in
	 *	LOCAL entry in **local** cred table.
	 */
	domain = nis_local_directory();
	if ((strlen(principal)+strlen(domain)+PKTABLE_LEN+30) >
		(size_t) NIS_MAXNAMELEN) {
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR, "netname2user: principal name '%s' too long",
			principal);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}
	sprintf(sname, "[cname=\"%s\",auth_type=LOCAL],%s.%s",
		principal, PKTABLE, domain);
	if (sname[strlen(sname) - 1] != '.')
		strcat(sname, ".");

	/* must use authenticated call here */
	/* XXX but we cant, for now. XXX */
	res = nis_list(sname, USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS+FOLLOW_PATH,
	    NULL, NULL);
	switch (res->status) {
	case NIS_NOTFOUND:
	case NIS_PARTIAL:
	case NIS_NOSUCHNAME:
	case NIS_NOSUCHTABLE:
		*err = __NSW_NOTFOUND;
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	case NIS_S_NOTFOUND:
	case NIS_TRYAGAIN:
		*err = __NSW_TRYAGAIN;
		syslog(LOG_ERR,
			"netname2user: (nis+ lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;   /* go and do something useful */
	default:
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR, "netname2user: (nis+ lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		trace1(TR_netname2user_nisplus, 1);
		return (0);
	}

	if (res->objects.objects_len > 1) {
		/*
		 * A principal can have more than one LOCAL entry?
		 * Something wrong with cred table.
		 * Warn user and continue.
		 */
		syslog(LOG_ALERT,
			"netname2user: LOCAL entry for %s in\
				directory %s not unique",
			netname, domain);
	}
	/* nisname	LOCAL	uid 	grp,grp,grp */
	*err = parse_uid_gidlist(ENTRY_VAL(res->objects.objects_val, 2),
					/* uid */
			ENTRY_VAL(res->objects.objects_val, 3), /* gids */
			argp);
	nis_freeresult(res);
	trace1(TR_netname2user_nisplus, 1);
	return (*err == __NSW_SUCCESS);
}

/*
 * Convert network-name into unix credential
 */
netname2user(netname, uidp, gidp, gidlenp, gidlist)
	const char netname[MAXNETNAMELEN + 1];
	uid_t *uidp;
	gid_t *gidp;
	int *gidlenp;
	gid_t *gidlist;
{
	struct __nsw_switchconfig *conf;
	struct __nsw_lookup *look;
	enum __nsw_parse_err perr;
	int needfree = 1, res;
	struct netid_userdata argp;
	int err;

	trace1(TR_netname2user, 0);

	/*
	 * Take care of the special case of nobody. Compare the netname
	 * to the string "nobody". If they are equal, return the SVID
	 * standard value for nobody.
	 */

	if (strcmp(netname, "nobody") == 0) {
		*uidp = NOBODY_UID;
		*gidp = NOBODY_UID;
		*gidlenp = 0;
		return (1);
	}

	/*
	 * First we do some generic sanity checks on the name we were
	 * passed. This lets us assume they are correct in the backends.
	 *
	 * NOTE: this code only recognizes names of the form :
	 *		unix.UID@domainname
	 */
	if (strncmp(netname, OPSYS, OPSYS_LEN) != 0) {
		trace1(TR_netname2user, 1);
		return (0);
	}
	if (! isdigit(netname[OPSYS_LEN+1])) { /* check for uid string */
		trace1(TR_netname2user, 1);
		return (0);
	}

	argp.uidp = uidp;
	argp.gidp = gidp;
	argp.gidlenp = gidlenp;
	argp.gidlist = gidlist;
	mutex_lock(&serialize_netname_r);

	conf = __nsw_getconfig("publickey", &perr);
	if (! conf) {
		conf = &publickey_default;
		needfree = 0;
	} else
		needfree = 1; /* free the config structure */

	for (look = conf->lookups; look; look = look->next) {
		if (strcmp(look->service_name, "nisplus") == 0)
			res = netname2user_nisplus(&err,
						(char *) netname, &argp);
		else if (strcmp(look->service_name, "nis") == 0)
			res = netname2user_nis(&err, (char *) netname, &argp);
		else if (strcmp(look->service_name, "files") == 0)
			res = netname2user_files(&err, (char *) netname, &argp);
		else {
			syslog(LOG_INFO,
		"netname2user: unknown nameservice for publickey info '%s'\n",
						look->service_name);
			err = __NSW_UNAVAIL;
		}
		switch (look->actions[err]) {
			case __NSW_CONTINUE :
				break;
			case __NSW_RETURN :
				if (needfree)
					__nsw_freeconfig(conf);
				mutex_unlock(&serialize_netname_r);
				trace1(TR_netname2user, 1);
				return (res);
			default :
				syslog(LOG_ERR,
			"netname2user: Unknown action for nameservice '%s'",
							look->service_name);
		}
	}
	if (needfree)
		__nsw_freeconfig(conf);
	mutex_unlock(&serialize_netname_r);
	trace1(TR_netname2user, 1);
	return (0);
}

/*
 * Convert network-name to hostname (fully qualified)
 * NOTE: this code only recognizes names of the form :
 *		unix.HOST@domainname
 *
 * This is very simple.  Since the netname is of the form:
 *	unix.host@domainname
 * We just construct the hostname using information from the domainname.
 */
netname2host(netname, hostname, hostlen)
	const char netname[MAXNETNAMELEN + 1];
	char *hostname;
	int hostlen;
{
	char *p, *domainname;
	int len, dlen;

	trace1(TR_netname2host, 0);

	if (!netname) {
		syslog(LOG_ERR, "netname2host: null netname");
		goto bad_exit;
	}

	if (strncmp(netname, OPSYS, OPSYS_LEN) != 0)
		goto bad_netname;
	p = (char *) netname + OPSYS_LEN;	/* skip OPSYS part */
	if (*p != '.')
		goto bad_netname;
	++p;				/* skip '.' */

	domainname = strchr(p, '@');	/* get domain name */
	if (domainname == 0)
		goto bad_netname;

	len = domainname - p;		/* host sits between '.' and '@' */
	domainname++;			/* skip '@' sign */

	if (len <= 0)
		goto bad_netname;

	if (hostlen < len) {
		syslog(LOG_ERR,
			"netname2host: insufficient space for hostname");
		goto bad_exit;
	}

	if (isdigit(*p))		/* don't want uid here */
		goto bad_netname;

	if (*p == '\0')			/* check for null hostname */
		goto bad_netname;

	strncpy(hostname, p, len);

	/* make into fully qualified hostname by concatenating domain part */
	dlen = strlen(domainname);
	if (hostlen < (len + dlen + 2)) {
		syslog(LOG_ERR,
			"netname2host: insufficient space for hostname");
		goto bad_exit;
	}

	hostname[len] = '.';
	strncpy(hostname+len+1, domainname, dlen);
	hostname[len+dlen+1] = '\0';

	trace1(TR_netname2host, 1);
	return (1);


bad_netname:
	syslog(LOG_ERR, "netname2host: invalid host netname %s", netname);

bad_exit:
	hostname[0] = '\0';
	trace1(TR_netname2host, 1);
	return (0);
}
