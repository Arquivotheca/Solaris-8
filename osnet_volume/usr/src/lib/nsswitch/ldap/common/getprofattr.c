/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getprofattr.c	1.2	99/10/22 SMI"

#include <secdb.h>
#include <prof_attr.h>
#include "ldap_common.h"


/* prof_attr attributes filters */
#define	_PROF_NAME		"cn"
#define	_PROF_RES1		"SolarisAttrReserved1"
#define	_PROF_RES2		"SolarisAttrReserved2"
#define	_PROF_DESC		"SolarisAttrLongDesc"
#define	_PROF_ATTRS		"SolarisAttrKeyValue"
#define	_PROF_GETPROFNAME	"(&(objectClass=SolarisProfAttr)(cn=%s))"


static const char *prof_attrs[] = {
	_PROF_NAME,
	_PROF_RES1,
	_PROF_RES2,
	_PROF_DESC,
	_PROF_ATTRS,
	(char *)NULL
};


static int
_nss_ldap_prof2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int			i, nss_result;
	int			buflen = (int)0;
	unsigned long		len = 0L;
	char			*nullstring = (char *)NULL;
	char			*buffer = (char *)NULL;
	char			*ceiling = (char *)NULL;
	profstr_t		*prof = (profstr_t *)NULL;
	ns_ldap_attr_t		*attrptr;
	ns_ldap_result_t	*result = be->result;

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_prof2ent;
	}
	prof = (profstr_t *)(argp->buf.result);
	ceiling = buffer + buflen;
	prof->name = (char *)NULL;
	prof->res1 = (char *)NULL;
	prof->res2 = (char *)NULL;
	prof->desc = (char *)NULL;
	prof->attr = (char *)NULL;
	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(argp->buf.buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_prof2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_prof2ent;
		}
		if (strcasecmp(attrptr->attrname, _PROF_NAME) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_prof2ent;
			}
			prof->name = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_prof2ent;
			}
			(void) strcpy(prof->name, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PROF_RES1) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				prof->res1 = nullstring;
			} else {
				prof->res1 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_prof2ent;
				}
				(void) strcpy(prof->res1,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PROF_RES2) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				prof->res2 = nullstring;
			} else {
				prof->res2 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_prof2ent;
				}
				(void) strcpy(prof->res2,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PROF_DESC) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				prof->desc = nullstring;
			} else {
				prof->desc = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_prof2ent;
				}
				(void) strcpy(prof->desc,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _PROF_ATTRS) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				prof->attr = nullstring;
			} else {
				prof->attr = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_prof2ent;
				}
				(void) strcpy(prof->attr,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
	}

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getprofattr.c: _nss_ldap_prof2ent]\n");
	(void) fprintf(stdout, "      prof-name: [%s]\n", prof->name);
	if (prof->res1 != (char *)NULL) {
		(void) fprintf(stdout, "      res1: [%s]\n", prof->res1);
	}
	if (prof->res2 != (char *)NULL) {
		(void) fprintf(stdout, "      res2: [%s]\n", prof->res2);
	}
	if (prof->desc != (char *)NULL) {
		(void) fprintf(stdout, "      desc: [%s]\n", prof->desc);
	}
	if (prof->attr != (char *)NULL) {
		(void) fprintf(stdout, "      attr: [%s]\n", prof->attr);
	}
#endif	/* DEBUG */

result_prof2ent:
	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	char		searchfilter[SEARCHFILTERLEN];
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getprofattr.c: getbyname]\n");
#endif	/* DEBUG */

	if (snprintf(searchfilter, SEARCHFILTERLEN, _PROF_GETPROFNAME,
	    argp->key.name) < 0) {
		return ((nss_status_t)NSS_NOTFOUND);
	}
	return (_nss_ldap_lookup(be, argp, _PROFATTR, searchfilter, NULL));
} 


static ldap_backend_op_t profattr_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbyname
};


nss_backend_t *
_nss_ldap_prof_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
#ifdef	DEBUG
	(void) fprintf(stdout,
	    "\n[getprofattr.c: _nss_ldap_prof_attr_constr]\n");
#endif
	return ((nss_backend_t *)_nss_ldap_constr(profattr_ops,
		sizeof (profattr_ops)/sizeof (profattr_ops[0]), _PROFATTR,
		prof_attrs, _nss_ldap_prof2ent));
}
