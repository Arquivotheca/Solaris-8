/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getuserattr.c	1.2	99/10/22 SMI"

#include <secdb.h>
#include <user_attr.h>
#include "ldap_common.h"


/* user_attr attributes filters */
#define	_USER_NAME		"uid"
#define	_USER_QUALIFIER		"SolarisUserQualifier"
#define	_USER_RES1		"SolarisAttrReserved1"
#define	_USER_RES2		"SolarisAttrReserved2"
#define	_USER_ATTRS		"SolarisAttrKeyValue"
#define	_USER_GETUSERNAME	\
	"(&(objectClass=SolarisUserAttr)(uid=%s))"


static const char *user_attrs[] = {
	_USER_NAME,
	_USER_QUALIFIER,
	_USER_RES1,
	_USER_RES2,
	_USER_ATTRS,
	(char *)NULL
};


static int
_nss_ldap_user2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int			i, nss_result;
	int			buflen = (int)0;
	unsigned long		len = 0L;
	char			*nullstring = (char *)NULL;
	char			*buffer = (char *)NULL;
	char			*ceiling = (char *)NULL;
	userstr_t		*user = (userstr_t *)NULL;
	ns_ldap_attr_t		*attrptr;
	ns_ldap_result_t	*result = be->result;

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_user2ent;
	}
	user = (userstr_t *)(argp->buf.result);
	ceiling = buffer + buflen;
	user->name = (char *)NULL;
	user->qualifier = (char *)NULL;
	user->res1 = (char *)NULL;
	user->res2 = (char *)NULL;
	user->attr = (char *)NULL;
	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(argp->buf.buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_user2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_user2ent;
		}
		if (strcasecmp(attrptr->attrname, _USER_NAME) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_user2ent;
			}
			user->name = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_user2ent;
			}
			(void) strcpy(user->name, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _USER_QUALIFIER) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				user->qualifier = nullstring;
			} else {
				user->qualifier = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_user2ent;
				}
				(void) strcpy(user->qualifier,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _USER_RES1) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				user->res1 = nullstring;
			} else {
				user->res1 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_user2ent;
				}
				(void) strcpy(user->res1,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _USER_RES2) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				user->res2 = nullstring;
			} else {
				user->res2 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_user2ent;
				}
				(void) strcpy(user->res2,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _USER_ATTRS) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				user->attr = nullstring;
			} else {
				user->attr = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_user2ent;
				}
				(void) strcpy(user->attr,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
	}

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getuserattr.c: _nss_ldap_user2ent]\n");
	(void) fprintf(stdout, "      user-name: [%s]\n", user->name);
	if (user->qualifier != (char *)NULL) {
		(void) fprintf(stdout, "      qualifier: [%s]\n",
		    user->qualifier);
	}
	if (user->res1 != (char *)NULL) {
		(void) fprintf(stdout, "      res1: [%s]\n", user->res1);
	}
	if (user->res2 != (char *)NULL) {
		(void) fprintf(stdout, "      res2: [%s]\n", user->res2);
	}
	if (user->attr != (char *)NULL) {
		(void) fprintf(stdout, "      attr: [%s]\n", user->attr);
	}
#endif	/* DEBUG */

result_user2ent:
	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	char		searchfilter[SEARCHFILTERLEN];
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getuserattr.c: getbyname]\n");
#endif	/* DEBUG */

	if (snprintf(searchfilter, SEARCHFILTERLEN, _USER_GETUSERNAME,
	    argp->key.name) < 0) {
		return ((nss_status_t)NSS_NOTFOUND);
	}
	return (_nss_ldap_lookup(be, argp, _USERATTR, searchfilter, NULL));
} 


static ldap_backend_op_t userattr_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbyname
};


nss_backend_t *
_nss_ldap_user_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
#ifdef	DEBUG
	(void) fprintf(stdout,
	    "\n[getuserattr.c: _nss_ldap_user_attr_constr]\n");
#endif
	return ((nss_backend_t *)_nss_ldap_constr(userattr_ops,
		sizeof (userattr_ops)/sizeof (userattr_ops[0]), _USERATTR,
		user_attrs, _nss_ldap_user2ent));
}
