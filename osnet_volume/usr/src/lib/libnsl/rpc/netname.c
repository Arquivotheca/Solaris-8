/*
 * ==== hack-attack:  possibly MT-safe but definitely not MT-hot.
 * ==== turn this into a real switch frontend and backends
 *
 * Well, at least the API doesn't involve pointers-to-static.
 */

#ident	"@(#)netname.c	1.32	99/07/19 SMI"

/*
 * Copyright (c) 1986-1992,1993-1995,1997-1999 by Sun Microsystems Inc.
 */

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)netname.c 1.2 89/03/10 Copyr 1986 Sun Micro";
#endif

/*
 * netname utility routines (getnetname, user2netname, host2netname).
 *
 * Convert from unix names (uid, gid) to network wide names.
 * This module is operating system dependent!
 * What we define here will work with any unix system that has adopted
 * the Sun NIS domain architecture.
 */

#undef NIS

#include "rpc_mt.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/nis_dhext.h>
#include <nsswitch.h>
#include <syslog.h>

#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN 256
#endif
#ifndef NGROUPS
#define	NGROUPS 16
#endif

/*
 * the value for NOBODY_UID is set by the SVID. The following define also
 * appears in netnamer.c
 */

#define	NOBODY_UID 60001

extern int __nis_principal();
extern int getdomainname();
extern char *strncpy();
extern int key_call();
#define	OPSYS_LEN 4
#define	PKTABLE_LEN 12
static const char *OPSYS = "unix";
static const char *PKTABLE  = "cred.org_dir";


/*
 * default publickey policy:
 *	publickey: nis [NOTFOUND = return] files
 */


/*	NSW_NOTSUCCESS  NSW_NOTFOUND   NSW_UNAVAIL    NSW_TRYAGAIN */
#define	DEF_ACTION {__NSW_RETURN, __NSW_RETURN, __NSW_CONTINUE, __NSW_CONTINUE}

static struct __nsw_lookup lookup_files = {"files", DEF_ACTION, NULL, NULL},
		lookup_nis = {"nis", DEF_ACTION, NULL, &lookup_files};
static struct __nsw_switchconfig publickey_default =
			{0, "publickey", 2, &lookup_nis};

static mutex_t serialize_netname = DEFAULTMUTEX;

/*
 * Convert unix cred to network-name using nisplus
 * nisplus cred table has the following format:
 *
 * cname	auth_type auth_name	public  private
 * ----------------------------------------------------------
 * nisname	DES	  netname	pubkey  private_key
 * nisname	LOCAL	  uid		gidlist
 *
 * Obtain netname given <uid,domain>.
 * 0.  If domain is NULL (indicating local domain), first try to get
 *	netname from keyserv (keylogin sets this).
 * 1.  Get the nisplus principal name from the LOCAL entry of the cred
 *	table in the specified domain (the local domain if domain is NULL).
 * 2.  Using the principal name, lookup the DES entry and extract netname.
 */

static
user2netname_nisplus(err, netname, uid, domain)
	int *err;
	char netname[MAXNETNAMELEN + 1];
	uid_t uid;
	char *domain;
{
	key_netstres kres;
	nis_result *nres;
	int len;
	uid_t my_uid;
	char principal[NIS_MAXNAMELEN+1];
	char buf[NIS_MAXNAMELEN+1];
	int status;
	mechanism_t **mechs;
	char auth_type[MECH_MAXATNAME+1];

	trace1(TR_user2netname_nisplus, 0);

	my_uid = geteuid();

	if (my_uid == uid && domain == NULL) {
		/*
		 * Look up the keyserv interface routines to see if
		 * netname is stored there.
		 */
		kres.key_netstres_u.knet.st_netname = NULL;
		if (key_call((rpcproc_t)KEY_NET_GET, xdr_void, (char *) NULL,
				xdr_key_netstres, (char *) &kres) &&
		    kres.status == KEY_SUCCESS) {
			len = strlen(kres.key_netstres_u.knet.st_netname);
			(void) strncpy(netname,
				kres.key_netstres_u.knet.st_netname,
				len +1);
			free(kres.key_netstres_u.knet.st_netname);
			trace1(TR_user2netname_nisplus, 1);
			*err = __NSW_SUCCESS;
			return (1);
		}
	}


	/*
	 * 1.  Determine user's nis+ principal name.
	 *
	 * If domain is specified, we want to look up the uid in the
	 * specified domain to determine the user's principal name.
	 * Otherwise, get principal name from local directory.
	 */
	if (domain == NULL)
		domain = nis_local_directory();
	/*
	 * Don't use nis_local_principal here because we want to
	 * catch the TRYAGAIN case so that we handle it properly.
	 */
	status = __nis_principal(principal, uid, domain);

	if (status != NIS_SUCCESS && status != NIS_S_SUCCESS) {
		switch (status) {
		case NIS_NOTFOUND:
		case NIS_PARTIAL:
		case NIS_NOSUCHNAME:
		case NIS_NOSUCHTABLE:
			*err = __NSW_NOTFOUND;
			break;
		case NIS_S_NOTFOUND:
		case NIS_TRYAGAIN:
			*err = __NSW_TRYAGAIN;
			syslog(LOG_ERR,
				"user2netname: (nis+ lookup): %s\n",
				nis_sperrno(status));
			break;
		default:
			*err = __NSW_UNAVAIL;
			syslog(LOG_ERR,
				"user2netname: (nis+ lookup): %s\n",
				nis_sperrno(status));
		}

		trace1(TR_user2netname_nisplus, 1);
		return (0);
	}

	/*
	 * 2.  use nis+ principal name to get netname by getting a PK entry.
	 *
	 * (Use NOAUTH to prevent recursion.)
	 */
	domain = nis_domain_of(principal);
	if ((strlen(principal)+strlen(domain)+PKTABLE_LEN+ 28) >
		    (size_t) NIS_MAXNAMELEN) {
		*err = __NSW_UNAVAIL;
		trace1(TR_user2netname_nisplus, 1);
		return (0);
	}

	if (mechs = __nis_get_mechanisms(FALSE)) {
		mechanism_t **mpp;

		/*
		 * Loop thru mechanism types till we find one in the
		 * cred table for this user.
		 */
		for (mpp = mechs; *mpp; mpp++) {
			mechanism_t *mp = *mpp;

			if (AUTH_DES_COMPAT_CHK(mp)) {
				__nis_release_mechanisms(mechs);
				goto try_auth_des;
			}
			if (! VALID_MECH_ENTRY(mp))
				continue;

			if (! __nis_mechalias2authtype(mp->alias, auth_type,
							sizeof (auth_type)))
				continue;

			(void) sprintf(buf,
				"[cname=\"%s\",auth_type=\"%s\"],%s.%s",
				principal, auth_type, PKTABLE, domain);
			if (buf[strlen(buf)-1] != '.')
				(void) strcat(buf, ".");

			nres = nis_list(buf,
				USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS+FOLLOW_PATH,
				NULL, NULL);

			/*
			 * If the entry is not found, let's try the next one,
			 * else it's success or a serious enough NIS+ err
			 * to bail on.
			 */
			if (nres->status != NIS_NOTFOUND)
				break;
		}
	} else {
	try_auth_des:
		/*
		 * No valid mechs exist or the AUTH_DES compat entry was
		 * found in the security cf.
		 */
		(void) sprintf(buf, "[cname=\"%s\",auth_type=DES],%s.%s",
			principal, PKTABLE, domain);
		if (buf[strlen(buf)-1] != '.')
			(void) strcat(buf, ".");

		nres = nis_list(buf,
				USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS+FOLLOW_PATH,
				NULL, NULL);
	}

	switch (nres->status) {
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;   /* go and do something useful */
	case NIS_NOTFOUND:
	case NIS_PARTIAL:
	case NIS_NOSUCHNAME:
	case NIS_NOSUCHTABLE:
		*err = __NSW_NOTFOUND;
		nis_freeresult(nres);
		trace1(TR_user2netname_nisplus, 1);
		return (0);
	case NIS_S_NOTFOUND:
	case NIS_TRYAGAIN:
		*err = __NSW_TRYAGAIN;
		syslog(LOG_ERR,
			"user2netname: (nis+ lookup): %s\n",
			nis_sperrno(nres->status));
		nis_freeresult(nres);
		trace1(TR_user2netname_nisplus, 1);
		return (0);
	default:
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR, "user2netname: (nis+ lookup): %s\n",
			nis_sperrno(nres->status));
		nis_freeresult(nres);
		trace1(TR_user2netname_nisplus, 1);
		return (0);
	}

	if (nres->objects.objects_len > 1) {
		/*
		 * Principal with more than one entry for this mech type?
		 * Something wrong with cred table. Should be unique.
		 * Warn user and continue.
		 */
		syslog(LOG_ALERT,
			"user2netname: %s entry for %s not unique",
			auth_type, principal);
	}

	len = ENTRY_LEN(nres->objects.objects_val, 2);
	if (len > MAXNETNAMELEN) {
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR, "user2netname: netname of '%s' too long",
			principal);
		nis_freeresult(nres);
		trace1(TR_user2netname_nisplus, 1);
		return (0);
	}
	(void) strncpy(netname, ENTRY_VAL(nres->objects.objects_val, 2), len);
	netname[len] = '\0';
	nis_freeresult(nres);
	*err = __NSW_SUCCESS;
	trace1(TR_user2netname_nisplus, 1);
	return (1);
}

#define	MAXIPRINT	(11)	/* max length of printed integer */

/*
 * Convert unix cred to network-name by concatenating the
 * 3 pieces of information <opsys type> <uid> <domain>.
 */

static
user2netname_nis(err, netname, uid, domain)
	int *err;
	char netname[MAXNETNAMELEN + 1];
	uid_t uid;
	char *domain;
{
	register int i;
	char *dfltdom;
	if (domain == NULL) {
		if (__rpc_get_default_domain(&dfltdom) != 0) {
			trace1(TR_user2netname_nis, 1);
			*err = __NSW_UNAVAIL;
			return (0);
		}
		domain = dfltdom;
	}
	if ((strlen(domain) + OPSYS_LEN + 3 + MAXIPRINT) >
						(size_t) MAXNETNAMELEN) {
		trace1(TR_user2netname_nis, 1);
		*err = __NSW_UNAVAIL;
		return (0);
	}
	(void) sprintf(netname, "%s.%d@%s", OPSYS, (int)uid, domain);
	i = strlen(netname);
	if (netname[i-1] == '.')
		netname[i-1] = '\0';
	trace1(TR_user2netname_nis, 1);
	*err = __NSW_SUCCESS;
	return (1);
}

/*
 * Figure out my fully qualified network name
 */
getnetname(name)
	char name[MAXNETNAMELEN + 1];
{
	uid_t uid;
	int dummy;

	trace1(TR_getnetname, 0);

	uid = geteuid();
	if (uid == 0)
		dummy = host2netname(name, (char *) NULL, (char *) NULL);
	else
		dummy = user2netname(name, uid, (char *) NULL);

	trace1(TR_getnetname, 1);
	return (dummy);
}


/*
 * Figure out the fully qualified network name for the given uid.
 * This is a private interface.
 */
int
__getnetnamebyuid(name, uid)
char name[MAXNETNAMELEN + 1];
uid_t	uid;
{
	int dummy;

	trace1(TR___getnetnamebyuid, 0);

	if (uid == 0)
		dummy = host2netname(name, (char *) NULL, (char *) NULL);
	else
		dummy = user2netname(name, uid, (char *) NULL);

	trace1(TR___getnetnamebyuid, 1);
	return (dummy);
}

/*
 * Convert unix cred to network-name
 *
 * It uses the publickey policy in the /etc/nsswitch.conf file
 * (Unless the netname is "nobody", which is special cased).
 * If there is no publickey policy in /etc/nsswitch.conf,
 * the default publickey policy is used, which is
 *	publickey: nis [NOTFOUND=return] files
 * Note that for the non-nisplus case, there is no failover
 * so only the first entry would be relevant for those cases.
 */
user2netname(netname, uid, domain)
	char netname[MAXNETNAMELEN + 1];
	const uid_t uid;
	const char *domain;
{
	struct __nsw_switchconfig *conf;
	struct __nsw_lookup *look;
	int needfree = 1, res = 0;
	enum __nsw_parse_err perr;
	int err;

	trace1(TR_user2netname, 0);

	/*
	 * Take care of the special case of "nobody". If the uid is
	 * the value assigned by the SVID for nobody, return the string
	 * "nobody".
	 */

	if (uid == NOBODY_UID) {
		(void) strcpy(netname, "nobody");
		return (1);
	}

	netname[0] = '\0';  /* make null first (no need for memset) */

	mutex_lock(&serialize_netname);

	conf = __nsw_getconfig("publickey", &perr);
	if (! conf) {
		conf = &publickey_default;
		needfree = 0;
	}

	for (look = conf->lookups; look; look = look->next) {
		if (strcmp(look->service_name, "nisplus") == 0)
			res = user2netname_nisplus(&err,
						netname, uid, (char *) domain);
		/* ldap, nis, and files all do the same thing. */
		else if (strcmp(look->service_name, "ldap") == 0 ||
			strcmp(look->service_name, "nis") == 0 ||
			strcmp(look->service_name, "files") == 0)
			res = user2netname_nis(&err,
				netname, uid, (char *) domain);
		else {
			syslog(LOG_INFO,
				"user2netname: unknown nameservice \
					for publickey info '%s'\n",
				look->service_name);
			err = __NSW_UNAVAIL;
		}
		switch (look->actions[err]) {
			case __NSW_CONTINUE :
				break;
			case __NSW_RETURN :
				if (needfree)
					__nsw_freeconfig(conf);
				mutex_unlock(&serialize_netname);
				trace1(TR_user2netname, 1);
				return (res);
			default :
				syslog(LOG_ERR,
			"user2netname: Unknown action for nameservice '%s'",
			look->service_name);
			}
	}
	if (needfree)
		__nsw_freeconfig(conf);
	mutex_unlock(&serialize_netname);
	trace1(TR_user2netname, 1);
	return (0);
}


/*
 * Convert host to network-name
 * This routine returns following netnames given the host and domain
 * arguments defined below: (domainname=y.z)
 *	  Arguments
 *	host	domain		netname
 *	----	------		-------
 *	-	-		unix.m@y.z (hostname=m)
 *	-	a.b		unix.m@a.b (hostname=m)
 *	-	-		unix.m@y.z (hostname=m.w.x)
 *	-	a.b		unix.m@a.b (hostname=m.w.x)
 *	h	-		unix.h@y.z
 *	h	a.b		unix.h@a.b
 *	h.w.x	-		unix.h@w.x
 *	h.w.x	a.b		unix.h@a.b
 */
host2netname(netname, host, domain)
	char netname[MAXNETNAMELEN + 1];
	const char *host;
	const char *domain;
{
	char *p;
	char hostname[MAXHOSTNAMELEN + 1];
	char domainname[MAXHOSTNAMELEN + 1];
	char *dot_in_host;
	register int i;
	size_t len;

	trace1(TR_host2netname, 0);

	netname[0] = '\0';  /* make null first (no need for memset) */

	if (host == NULL) {
		(void) strncpy(hostname, nis_local_host(), sizeof (hostname));
		p = (char *)strchr(hostname, '.');
		if (p) {
			*p++ = '\0';
			/* if no domain passed, use tail of nis_local_host() */
			if (domain == NULL) {
				domain = p;
			}
		}
	} else {
		len = strlen(host);
		if (len >= sizeof (hostname)) {
			return (0);
		}
		(void) strcpy(hostname, host);
	}

	dot_in_host = (char *)strchr(hostname, '.');
	if (domain == NULL) {
		p = dot_in_host;
		if (p) {
			p = (char *)nis_domain_of(hostname);
			len = strlen(p);
			if (len >= sizeof (domainname)) {
				return (0);
			}
			(void) strcpy(domainname, p);
		} else {
			domainname[0] = NULL;
			if (getdomainname(domainname, MAXHOSTNAMELEN) < 0)
				return (0);
		}
	} else {
		len = strlen(domain);
		if (len >= sizeof (domainname)) {
			return (0);
		}
		(void) strcpy(domainname, domain);
	}

	i = strlen(domainname);
	if (i == 0)
		/* No domainname */
		return (0);
	if (domainname[i - 1] == '.')
		domainname[i - 1] = 0;

	if (dot_in_host) {  /* strip off rest of name */
		*dot_in_host = '\0';
	}

	if ((strlen(domainname) + strlen(hostname) + OPSYS_LEN + 3)
	    > (size_t) MAXNETNAMELEN) {
		trace1(TR_host2netname, 1);
		return (0);
	}

	(void) sprintf(netname, "%s.%s@%s", OPSYS, hostname, domainname);
	trace1(TR_host2netname, 1);
	return (1);
}
