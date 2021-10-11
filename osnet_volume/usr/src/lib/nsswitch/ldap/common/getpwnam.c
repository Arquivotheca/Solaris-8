/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getpwnam.c 1.2     99/10/22 SMI"

#include <pwd.h>
#include "ldap_common.h"

/* passwd attributes filters */
#define	_PWD_CN			"cn"
#define	_PWD_UID		"uid"
#define	_PWD_USERPASSWORD	"userpassword"
#define	_PWD_UIDNUMBER		"uidnumber"
#define	_PWD_GIDNUMBER		"gidnumber"
#define	_PWD_GECOS		"gecos"
#define	_PWD_DESCRIPTION	"description"
#define	_PWD_HOMEDIRECTORY	"homedirectory"
#define	_PWD_LOGINSHELL		"loginshell"


#define	_F_GETPWNAM		"(&(objectClass=posixAccount)(uid=%s))"
#define	_F_GETPWUID		"(&(objectClass=posixAccount)(uidNumber=%ld))"

static const char *pwd_attrs[] = {
	_PWD_CN,
	_PWD_UID,
	_PWD_UIDNUMBER,
	_PWD_GIDNUMBER,
	_PWD_GECOS,
	_PWD_DESCRIPTION,
	_PWD_HOMEDIRECTORY,
	_PWD_LOGINSHELL,
	(char *)NULL
};


/*
 * _nss_ldap_passwd2ent is the data marshaling method for the passwd getXbyY
 * (e.g., getbyuid(), getbyname(), getpwent()) backend processes. This method is
 * called after a successful ldap search has been performed. This method will
 * parse the ldap search values into struct passwd = argp->buf.buffer which
 * the frontend process expects. Three error conditions are expected and
 * returned to nsswitch.
 */

static int
_nss_ldap_passwd2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i = 0;
	int		nss_result;
	int		buflen = (int)0;
	unsigned long	len = 0L;
	char		*buffer = (char *)NULL;
	char		*ceiling = (char *)NULL;
	char		*nullstring = (char *)NULL;
	struct passwd	*pwd = (struct passwd *)NULL;
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: _nss_ldap_passwd2ent]\n");
#endif	DEBUG

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_pwd2ent;
	}
	pwd = (struct passwd *)argp->buf.result;
	ceiling = buffer + buflen;
	nullstring = (buffer + (buflen - 1));

	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_pwd2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_pwd2ent;
		}
		if (strcasecmp(attrptr->attrname, _PWD_UID) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_pwd2ent;
			}
			pwd->pw_name = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_pwd2ent;
			}
			(void) strcpy(pwd->pw_name, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PWD_UIDNUMBER) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_pwd2ent;
			}
			pwd->pw_uid = strtol(attrptr->attrvalue[0],
					    (char **)NULL, 10);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PWD_GIDNUMBER) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_pwd2ent;
			}
			pwd->pw_gid = strtol(attrptr->attrvalue[0],
					    (char **)NULL, 10);
			continue;
		}
		if ((strcasecmp(attrptr->attrname, _PWD_GECOS) == 0) &&
		    (attrptr->value_count > 0)) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				pwd->pw_gecos = nullstring;
			} else {
				pwd->pw_gecos = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_pwd2ent;
				}
				(void) strcpy(pwd->pw_gecos,
					    attrptr->attrvalue[0]);
			}
			continue;
		}
		if ((strcasecmp(attrptr->attrname, _PWD_HOMEDIRECTORY) == 0) &&
		    (attrptr->value_count > 0)) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				pwd->pw_dir = nullstring;
			} else {
				pwd->pw_dir = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_pwd2ent;
				}
				(void) strcpy(pwd->pw_dir,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if ((strcasecmp(attrptr->attrname, _PWD_LOGINSHELL) == 0) &&
		    (attrptr->value_count > 0)) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				pwd->pw_shell = nullstring;
			} else {
				pwd->pw_shell = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_pwd2ent;
				}
				(void) strcpy(pwd->pw_shell,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
	}
	pwd->pw_age = nullstring;
	pwd->pw_comment = nullstring;
	pwd->pw_passwd = nullstring;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: _nss_ldap_passwd2ent]\n");
	(void) fprintf(stdout, "       pw_name: [%s]\n", pwd->pw_name);
	(void) fprintf(stdout, "        pw_uid: [%ld]\n", pwd->pw_uid);
	(void) fprintf(stdout, "        pw_gid: [%ld]\n", pwd->pw_gid);
	(void) fprintf(stdout, "      pw_gecos: [%s]\n", pwd->pw_gecos);
	(void) fprintf(stdout, "        pw_dir: [%s]\n", pwd->pw_dir);
	(void) fprintf(stdout, "      pw_shell: [%s]\n", pwd->pw_shell);
#endif	DEBUG

result_pwd2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


/*
 * getbyname gets a passwd entry by uid name. This function constructs an ldap
 * search filter using the name invocation parameter and the getpwnam search
 * filter defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into struct passwd for the frontend
 * process. The function _nss_ldap_passwd2ent performs the data marshaling.
 */

static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: getbyname]\n");
#endif	DEBUG

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETPWNAM, argp->key.name) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_PASSWD, searchfilter, NULL));
}


/*
 * getbyuid gets a passwd entry by uid number. This function constructs an ldap
 * search filter using the uid invocation parameter and the getpwuid search
 * filter defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into struct passwd for the frontend
 * process. The function _nss_ldap_passwd2ent performs the data marshaling.
 */

static nss_status_t
getbyuid(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: getbyuid]\n");
#endif	DEBUG

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETPWUID, (long)argp->key.uid) < 0)
		return ((nss_status_t) NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_PASSWD, searchfilter, NULL));
}

static ldap_backend_op_t passwd_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbyname,
	getbyuid
};


/*
 * _nss_ldap_passwd_constr is where life begins. This function calls the
 * generic ldap constructor function to define and build the abstract
 * data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_passwd_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: _nss_ldap_passwd_constr]\n");
#endif	DEBUG

	return ((nss_backend_t *)_nss_ldap_constr(passwd_ops,
		    sizeof (passwd_ops)/sizeof (passwd_ops[0]),
		    _PASSWD, pwd_attrs, _nss_ldap_passwd2ent));
}
