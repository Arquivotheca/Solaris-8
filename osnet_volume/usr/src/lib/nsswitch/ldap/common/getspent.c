/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getspent.c 1.2     99/10/22 SMI"

#include <shadow.h>
#include <stdlib.h>
#include "ldap_common.h"

/* shadow attributes filters */
#define	_S_CN			"cn"
#define	_S_UID			"uid"
#define	_S_USERPASSWORD		"userpassword"
#define	_S_LASTCHANGE		"shadowlastchange"
#define	_S_MIN			"shadowmin"
#define	_S_MAX			"shadowmax"
#define	_S_WARNING		"shadowwarning"
#define	_S_INACTIVE		"shadowinactive"
#define	_S_EXPIRE		"shadowexpire"
#define	_S_FLAG			"shadowflag"

#define	_F_GETSPNAM		"(&(objectClass=shadowAccount)(uid=%s))"

static const char *sp_attrs[] = {
	_S_UID,
	_S_USERPASSWORD,
	_S_LASTCHANGE,
	_S_MIN,
	_S_MAX,
	_S_WARNING,
	_S_INACTIVE,
	_S_EXPIRE,
	_S_FLAG,
	(char *)NULL
};


extern ns_ldap_attr_t *getattr(ns_ldap_result_t *result, int i);

/*
 * _nss_ldap_shadow2ent is the data marshaling method for the passwd getXbyY
 * (e.g., getspnam(), getspent()) backend processes. This method is called after
 * a successful ldap search has been performed. This method will parse the
 * ldap search values into struct spwd = argp->buf.buffer which the frontend
 * process expects. Three error conditions are expected and returned to
 * nsswitch.
 */

static int
_nss_ldap_shadow2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i = 0;
	int		nss_result;
	int		buflen = (int)0;
	unsigned long	len = 0L;
	char		*buffer = (char *)NULL;
	char		*ceiling = (char *)NULL;
	char		*pw_passwd = (char *)NULL;
	char		*nullstring = (char *)NULL;
	char		np[] = "*NP*";
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;
	long		ltmp = (long)0L;
	struct spwd	*spd = (struct spwd *)NULL;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getspent.c: _nss_ldap_shadow2ent]\n");
#endif DEBUG

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_spd2ent;
	}
	spd = (struct spwd *)argp->buf.result;
	ceiling = buffer + buflen;
	nullstring = (buffer + (buflen - 1));

	/* Default values */
	spd->sp_lstchg = -1;	spd->sp_min    = -1;
	spd->sp_max    = -1;	spd->sp_warn   = -1;
	spd->sp_inact  = -1;	spd->sp_expire = -1;
	spd->sp_flag   = 0;	spd->sp_pwdp = NULL;

	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_spd2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (strcasecmp(attrptr->attrname, _S_UID) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_namp = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_spd2ent;
			}
			(void) strcpy(spd->sp_namp, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_USERPASSWORD) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				spd->sp_pwdp = nullstring;
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			pw_passwd = attrptr->attrvalue[0];
			if (pw_passwd) {
				char	*tmp;

				if ((tmp = strstr(pw_passwd, "{crypt}"))
					!= NULL) {
					if (tmp != pw_passwd)
						pw_passwd = np;
					else
						pw_passwd += 7;
				} else if ((tmp = strstr(pw_passwd, "{CRYPT}"))
					!= NULL) {
					if (tmp != pw_passwd)
						pw_passwd = np;
					else
						pw_passwd += 7;
				} else {
					pw_passwd = np;
				}
			}
			len = (unsigned long)strlen(pw_passwd);
			if (len < 1) {
				spd->sp_pwdp = nullstring;
			} else {
				spd->sp_pwdp = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_spd2ent;
				}
			}
			(void) strcpy(spd->sp_pwdp, pw_passwd);
		}
		if (strcasecmp(attrptr->attrname, _S_LASTCHANGE) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_lstchg = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_MIN) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_min = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_MAX) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_max = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_WARNING) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_warn = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_INACTIVE) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_inact = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_EXPIRE) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_expire = (int)ltmp;
			continue;
		}
		if (strcasecmp(attrptr->attrname, _S_FLAG) == 0) {
			if (attrptr->attrvalue[0] == '\0') {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			errno = 0;
			ltmp = strtol(attrptr->attrvalue[0], (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_spd2ent;
			}
			spd->sp_flag = (int)ltmp;
			continue;
		}
	}

	/* we will not allow for an empty password to be */
	/* returned to the front end as this is not a supported */
	/* configuration.  Since we got to this point without */
	/* the password being set, we assume that no password was */
	/* set on the server which is consider a misconfiguration. */
	/* We will proceed and set the password to *NP* as no password */
	/* is not supported */

	if (spd->sp_pwdp == NULL) {
		spd->sp_pwdp = buffer;
		buffer += strlen(np) + 1;
		if (buffer >= ceiling) {
			nss_result = (int)NSS_STR_PARSE_ERANGE;
			goto result_spd2ent;
		}
		strcpy(spd->sp_pwdp, np);
	}


#ifdef DEBUG
	(void) fprintf(stdout, "\n[getspent.c: _nss_ldap_shadow2ent]\n");
	(void) fprintf(stdout, "       sp_namp: [%s]\n", spd->sp_namp);
	(void) fprintf(stdout, "       sp_pwdp: [%s]\n", spd->sp_pwdp);
	(void) fprintf(stdout, "     sp_latchg: [%d]\n", spd->sp_lstchg);
	(void) fprintf(stdout, "        sp_min: [%d]\n", spd->sp_min);
	(void) fprintf(stdout, "        sp_max: [%d]\n", spd->sp_max);
	(void) fprintf(stdout, "       sp_warn: [%d]\n", spd->sp_warn);
	(void) fprintf(stdout, "      sp_inact: [%d]\n", spd->sp_inact);
	(void) fprintf(stdout, "     sp_expire: [%d]\n", spd->sp_expire);
	(void) fprintf(stdout, "       sp_flag: [%d]\n", spd->sp_flag);
#endif DEBUG

result_spd2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}

/*
 * getbynam gets a passwd entry by uid name. This function constructs an ldap
 * search filter using the name invocation parameter and the getspnam search
 * filter defined. Once the filter is constructed we search for a matching
 * entry and marshal the data results into struct shadow for the frontend
 * process. The function _nss_ldap_shadow2ent performs the data marshaling.
 */

static nss_status_t
getbynam(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getspent.c: getbynam]\n");
#endif DEBUG

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETSPNAM, argp->key.name) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return (_nss_ldap_lookup(be, argp, _SHADOW, searchfilter, NULL));
}

static ldap_backend_op_t sp_ops[] = {
    _nss_ldap_destr,
    _nss_ldap_endent,
    _nss_ldap_setent,
    _nss_ldap_getent,
    getbynam
};


/*
 * _nss_ldap_passwd_constr is where life begins. This function calls the
 * generic ldap constructor function to define and build the abstract
 * data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_shadow_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getspent.c: _nss_ldap_shadow_constr]\n");
#endif DEBUG

	return ((nss_backend_t *)_nss_ldap_constr(sp_ops,
		sizeof (sp_ops)/sizeof (sp_ops[0]),
		_SHADOW, sp_attrs, _nss_ldap_shadow2ent));
}
