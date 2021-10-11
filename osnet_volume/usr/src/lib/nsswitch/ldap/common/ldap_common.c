/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ldap_common.c 1.3     99/09/21 SMI"

#include "ldap_common.h"
#include <malloc.h>
#include <synch.h>
#include <syslog.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <thread.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>

/* getent attributes filters */
#define	_F_GETALIASENT		"(objectClass=rfc822MailGroup)"
#define	_F_GETAUTHNAME		"(objectClass=SolarisAuthAttr)"
#define	_F_GETAUUSERNAME	"(objectClass=SolarisAuditUser)"
#define	_F_GETEXECNAME		"(objectClass=SolarisExecAttr)"
#define	_F_GETGRENT		"(objectClass=posixGroup)"
#define	_F_GETHOSTENT		"(objectClass=ipHost)"
#define	_F_GETNETENT		"(objectClass=ipNetwork)"
#define	_F_GETPROFNAME		"(objectClass=SolarisProfAttr)"
#define	_F_GETPROTOENT		"(objectClass=ipProtocol)"
#define	_F_GETPWENT		"(objectClass=posixAccount)"
#define	_F_GETRPCENT		"(objectClass=oncRpc)"
#define	_F_GETSERVENT		"(objectClass=ipService)"
#define	_F_GETSPENT		"(objectclass=shadowAccount)"
#define	_F_GETUSERNAME		"(objectClass=SolarisUserAttr)"


static struct gettablefilter {
	char *tablename;
	char *tablefilter;
} gettablefilterent[] = {
	{(char *)_PASSWD,	(char *)_F_GETPWENT},
	{(char *)_SHADOW,	(char *)_F_GETSPENT},
	{(char *)_GROUP,	(char *)_F_GETGRENT},
	{(char *)_HOSTS,	(char *)_F_GETHOSTENT},
	{(char *)_NETWORKS,	(char *)_F_GETNETENT},
	{(char *)_PROTOCOLS,	(char *)_F_GETPROTOENT},
	{(char *)_RPC,		(char *)_F_GETRPCENT},
	{(char *)_ALIASES,	(char *)_F_GETALIASENT},
	{(char *)_SERVICES,	(char *)_F_GETSERVENT},
	{(char *)_AUUSER,	(char *)_F_GETAUUSERNAME},
	{(char *)_AUTHATTR,	(char *)_F_GETAUTHNAME},
	{(char *)_EXECATTR,	(char *)_F_GETEXECNAME},
	{(char *)_PROFATTR,	(char *)_F_GETPROFNAME},
	{(char *)_USERATTR,	(char *)_F_GETUSERNAME},
	{(char *)NULL,		(char *)NULL}
};


static nss_status_t
switch_err(int rc, ns_ldap_error_t *error)
{
	switch (rc) {
	    case NS_LDAP_SUCCESS:
		return (NSS_SUCCESS);

	    case NS_LDAP_NOTFOUND:
		return (NSS_NOTFOUND);

	    case NS_LDAP_PARTIAL:
		return (NSS_TRYAGAIN);

	    case NS_LDAP_INTERNAL:
		    if (error && (error->status == LDAP_SERVER_DOWN ||
				error->status == LDAP_TIMEOUT))
			    return (NSS_TRYAGAIN);
		    else
			    return (NSS_UNAVAIL);

	    default:
		return (NSS_UNAVAIL);
	}
}

nss_status_t
_nss_ldap_lookup(ldap_backend_ptr be, nss_XbyY_args_t *argp,
		char *database, char *searchfilter, char *domain)
{
	int		callbackstat = 0;
	ns_ldap_error_t	*error;
	int		rc;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_lookup]\n");
	(void) fprintf(stdout, "\tsearchfilter: %s\n", searchfilter);
	(void) fprintf(stdout, "\tdatabase: %s\n", database);
#endif	DEBUG

	if ((rc = __ns_ldap_list(database, searchfilter, be->attrs, domain,
	    NULL, 0, &be->result, &error, NULL, NULL)) != NS_LDAP_SUCCESS) {
		argp->returnval = 0;
		rc = switch_err(rc, error);
		(void) __ns_ldap_freeError(&error);
		return (rc);
	}
	/* callback function */
	if ((callbackstat =
		    be->ldapobj2ent(be, argp)) == NSS_STR_PARSE_SUCCESS) {
		argp->returnval = argp->buf.result;
		return ((nss_status_t)NSS_SUCCESS);
	}
	(void) __ns_ldap_freeResult(&be->result);

	/* error */
	if (callbackstat == NSS_STR_PARSE_PARSE) {
		argp->returnval = 0;
		return ((nss_status_t)NSS_NOTFOUND);
	}
	if (callbackstat == NSS_STR_PARSE_ERANGE) {
		argp->erange = 1;
		return ((nss_status_t)NSS_NOTFOUND);
	}

	return ((nss_status_t)NSS_UNAVAIL);
}


/*
 *  This function is similar to _nss_ldap_lookup except it does not
 *  do a callback.  It is only used by getnetgrent.c
 */

nss_status_t
_nss_ldap_nocb_lookup(ldap_backend_ptr be, nss_XbyY_args_t *argp,
		char *database, char *searchfilter, char *domain)
{
	ns_ldap_error_t	*error = NULL;
	int		rc;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_nocb_lookup]\n");
	(void) fprintf(stdout, "\tsearchfilter: %s\n", searchfilter);
	(void) fprintf(stdout, "\tdatabase: %s\n", database);
#endif	DEBUG

	if ((rc = __ns_ldap_list(database, searchfilter, be->attrs, domain,
	    NULL, 0, &be->result, &error, NULL, NULL)) != NS_LDAP_SUCCESS) {
		argp->returnval = 0;
		rc = switch_err(rc, error);
		(void) __ns_ldap_freeError(&error);
		return (rc);
	}

	return ((nss_status_t)NSS_SUCCESS);
}


/*
 *
 */

void
_clean_ldap_backend(ldap_backend_ptr be)
{
	ns_ldap_error_t *error;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _clean_ldap_backend]\n");
#endif	DEBUG

	if (be->tablename != NULL)
		free(be->tablename);
	if (be->result != NULL)
		(void) __ns_ldap_freeResult(&be->result);
	if (be->enumcookie != NULL)
		(void) __ns_ldap_endEntry(&be->enumcookie, &error);
	if (be->toglue != NULL) {
		free(be->toglue);
		be->toglue = NULL;
	}
	free(be);
}


/*
 * _nss_ldap_destr will free all smalloc'ed variable strings and structures
 * before exiting this nsswitch shared backend library. This function is
 * called before returning control back to nsswitch.
 */

/*ARGSUSED1*/
nss_status_t
_nss_ldap_destr(ldap_backend_ptr be, void *a)
{

#ifdef DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_destr]\n");
#endif DEBUG

	(void) _clean_ldap_backend(be);

	return ((nss_status_t)NSS_SUCCESS);
}


/*
 * _nss_ldap_setent called before _nss_ldap_getent. This function is
 * required by POSIX.
 */

nss_status_t
_nss_ldap_setent(ldap_backend_ptr be, void *a)
{
	struct gettablefilter	*gtf;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_setent]\n");
#endif DEBUG

	if (be->setcalled == 1)
		(void) _nss_ldap_endent(be, a);
	be->filter = NULL;
	for (gtf = gettablefilterent; gtf->tablename != (char *)NULL; gtf++) {
		if (strcmp(gtf->tablename, be->tablename))
			continue;
		be->filter = (char *)gtf->tablefilter;
		break;
	}

	be->setcalled = 1;
	be->enumcookie = NULL;
	be->result = NULL;
	return ((nss_status_t)NSS_SUCCESS);
}


/*
 * _nss_ldap_endent called after _nss_ldap_getent. This function is
 * required by POSIX.
 */

/*ARGSUSED1*/
nss_status_t
_nss_ldap_endent(ldap_backend_ptr be, void *a)
{
	ns_ldap_error_t	*error;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_endent]\n");
#endif DEBUG

	be->setcalled = 0;
	be->filter = NULL;
	if (be->enumcookie != NULL) {
		(void) __ns_ldap_endEntry(&be->enumcookie, &error);
		(void) __ns_ldap_freeError(&error);
	}
	if (be->result != NULL) {
		(void) __ns_ldap_freeResult(&be->result);
	}

	return ((nss_status_t)NSS_SUCCESS);
}


/*
 *
 */

nss_status_t
_nss_ldap_getent(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	ns_ldap_error_t	*error = NULL;
	int		parsestat = 0;
	int		retcode = 0;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_getent]\n");
#endif	DEBUG

	if (be->setcalled == 0)
		(void) _nss_ldap_setent(be, a);

	if (be->enumcookie == NULL) {
		retcode = __ns_ldap_firstEntry(be->tablename,
		be->filter, be->attrs, NULL, NULL, 0, &be->enumcookie,
		&be->result, &error);
	} else {
		retcode = __ns_ldap_nextEntry(be->enumcookie, &be->result,
					&error);
	}
	if (retcode == NS_LDAP_NOTFOUND) {
		return (switch_err(retcode, error));
	} else if (retcode != NS_LDAP_SUCCESS) {
		retcode = switch_err(retcode, error);
		(void) __ns_ldap_freeError(&error);
		(void) _nss_ldap_endent(be, a);
		return (retcode);
	} else {
		if ((parsestat = be->ldapobj2ent(be, argp))
			== NSS_STR_PARSE_SUCCESS) {
			be->result = NULL;
			argp->returnval = argp->buf.result;
			return ((nss_status_t)NSS_SUCCESS);
		}
		be->result = NULL;
		if (parsestat == NSS_STR_PARSE_PARSE) {
			argp->returnval = 0;
			(void) _nss_ldap_endent(be, a);
			return ((nss_status_t)NSS_NOTFOUND);
		}

		if (parsestat == NSS_STR_PARSE_ERANGE) {
			argp->erange = 1;
			(void) _nss_ldap_endent(be, a);
			return ((nss_status_t)NSS_NOTFOUND);
		}
	}

	return ((nss_status_t)NSS_SUCCESS);
}


/*
 *
 */

nss_backend_t *
_nss_ldap_constr(ldap_backend_op_t ops[], int nops, char *tablename,
		const char **attrs, fnf ldapobj2ent)
{
	ldap_backend_ptr	be;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[ldap_common.c: _nss_ldap_constr]\n");
#endif	DEBUG

	if ((be = (ldap_backend_ptr) malloc(sizeof (*be))) == 0)
		return (0);
	be->ops = ops;
	be->nops = (nss_dbop_t)nops;
	be->tablename = (char *)strdup(tablename);
	be->attrs = attrs;
	be->result = NULL;
	be->ldapobj2ent = ldapobj2ent;
	be->setcalled = 0;
	be->filter = NULL;
	be->enumcookie = NULL;
	be->netgroup = NULL;
	be->toglue = NULL;
	(void) memset((void *)&be->all_members, 0, sizeof (struct netgrouptab));
	be->next_member = NULL;

	return ((nss_backend_t *)be);
}


/*
 *
 */
int
chophostdomain(char *string, char *host, char *domain)
{
	char	*dot;

	if (string == NULL)
		return (-1);

	if ((dot = strchr(string, '.')) == NULL) {
		return (0);
	}
	*dot = '\0';
	strcpy(host, string);
	strcpy(domain, ++dot);

	return (0);
}


/*
 *
 */
int
propersubdomain(char *domain, char *subdomain)
{
	char	*tmp;
	int	domainlen, subdomainlen;

	/* sanity check */
	if (domain == NULL || subdomain == NULL)
		return (-1);

	/* is afterdot a substring of domain? */
	if ((tmp = strstr(domain, subdomain)) == NULL)
		return (-1);
	/* if tmp is not pointing to beginning of domain */
	if (tmp != domain)
		return (-1);

	domainlen = strlen(domain);
	subdomainlen = strlen(subdomain);

	if (domainlen == subdomainlen)
		return (1);

	if (subdomainlen > domainlen)
		return (-1);

	if (*(domain + subdomainlen) != '.')
		return (-1);

	return (1);
}
