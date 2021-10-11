/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getexecattr.c	1.2	99/10/22 SMI"

#include <secdb.h>
#include <exec_attr.h>
#include "ldap_common.h"


/* exec_attr attributes filters */
#define	ISWILD(x)		(x == NULL) ? "*" : x
#define	_EXEC_NAME		"cn"
#define	_EXEC_POLICY		"SolarisKernelSecurityPolicy"
#define	_EXEC_TYPE		"SolarisProfileType"
#define	_EXEC_RES1		"SolarisAttrRes1"
#define	_EXEC_RES2		"SolarisAttrRes2"
#define	_EXEC_ID		"SolarisProfileId"
#define	_EXEC_ATTRS		"SolarisAttrKeyValue"
#define	_EXEC_GETEXECNAME	"(&(objectClass=SolarisExecAttr)(cn=%s)"\
				"(SolarisKernelSecurityPolicy=%s)"\
				"(SolarisProfileType=%s))"
#define	_EXEC_GETEXECID		"(&(objectClass=SolarisExecAttr)"\
				"(SolarisProfileId=%s)"\
				"(SolarisKernelSecurityPolicy=%s)"\
				"(SolarisProfileType=%s))"
#define	_EXEC_GETEXECNAMEID	"(&(objectClass=SolarisExecAttr)"\
				"(cn=%s)"\
				"(SolarisProfileId=%s)"\
				"(SolarisKernelSecurityPolicy=%s)"\
				"(SolarisProfileType=%s))"


/* from libnsl */
extern int _exec_checkid(char *, const char *, const char *);
extern int _doexeclist(nss_XbyY_args_t *);
extern void _free_execstr(execstr_t *);


static const char *exec_attrs[] = {
	_EXEC_NAME,
	_EXEC_POLICY,
	_EXEC_TYPE,
	_EXEC_RES1,
	_EXEC_RES2,
	_EXEC_ID,
	_EXEC_ATTRS,
	(char *)NULL
};


#ifdef	DEBUG
static void
_print_execstr(execstr_t *exec)
{

	(void) fprintf(stdout, "      exec-name: [%s]\n", exec->name);
	if (exec->policy != (char *)NULL) {
		(void) fprintf(stdout, "      policy: [%s]\n", exec->policy);
	}
	if (exec->type != (char *)NULL) {
		(void) fprintf(stdout, "      type: [%s]\n", exec->type);
	}
	if (exec->res1 != (char *)NULL) {
		(void) fprintf(stdout, "      res1: [%s]\n", exec->res1);
	}
	if (exec->res2 != (char *)NULL) {
		(void) fprintf(stdout, "      res2: [%s]\n", exec->res2);
	}
	if (exec->id != (char *)NULL) {
		(void) fprintf(stdout, "      id: [%s]\n", exec->id);
	}
	if (exec->attr != (char *)NULL) {
		(void) fprintf(stdout, "      attr: [%s]\n", exec->attr);
	}
	if (exec->next != (execstr_t *)NULL) {
		(void) fprintf(stdout, "      next: [%s]\n", exec->next->name);
		(void) fprintf(stdout, "\n");
		_print_execstr(exec->next);
	}
}
#endif	/* DEBUG */


static int
_exec_ldap_exec2ent(ns_ldap_entry_t *entry, nss_XbyY_args_t *argp)
{

	int			i;
	unsigned long		len = 0L;
	int			buflen = (int)0;
	char			*nullstring = (char *)NULL;
	char			*buffer = (char *)NULL;
	char			*ceiling = (char *)NULL;
	execstr_t		*exec = (execstr_t *)NULL;
	ns_ldap_attr_t		*attrptr;

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	(void) memset(argp->buf.buffer, 0, buflen);
	exec = (execstr_t *)(argp->buf.result);
	ceiling = buffer + buflen;
	exec->name = (char *)NULL;
	exec->policy = (char *)NULL;
	exec->type = (char *)NULL;
	exec->res1 = (char *)NULL;
	exec->res2 = (char *)NULL;
	exec->id = (char *)NULL;
	exec->attr = (char *)NULL;

	for (i = 0; i < entry->attr_count; i++) {
		attrptr = entry->attr_pair[i];
		if (attrptr == NULL) {
			return ((int)NSS_STR_PARSE_PARSE);
		}
		if (strcasecmp(attrptr->attrname, _EXEC_NAME) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				return ((int)NSS_STR_PARSE_PARSE);
			}
			exec->name = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				return ((int)NSS_STR_PARSE_ERANGE);
			}
			(void) strcpy(exec->name, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_POLICY) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				exec->policy = nullstring;
			} else {
				exec->policy = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->policy,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_TYPE) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				exec->type = nullstring;
			} else {
				exec->type = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->type,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_RES1) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				exec->res1 = nullstring;
			} else {
				exec->res1 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->res1,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_RES2) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				exec->res2 = nullstring;
			} else {
				exec->res2 = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->res2,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_ID) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (attrptr->attrvalue[0] == '\0') {
				exec->id = nullstring;
			} else {
				exec->id = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->id, attrptr->attrvalue[0]);
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _EXEC_ATTRS) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				exec->attr = nullstring;
			} else {
				exec->attr = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					return ((int)NSS_STR_PARSE_ERANGE);
				}
				(void) strcpy(exec->attr,
				    attrptr->attrvalue[0]);
			}
			continue;
		}
	}

	exec->next = (execstr_t *)NULL;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: _exec_ldap_exec2ent]\n");
	_print_execstr(exec);
#endif	/* DEBUG */

	return ((int)NSS_STR_PARSE_SUCCESS);
}


/*
 * place the results from ldap object structure into argp->buf.result
 * returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 */
static int
_nss_ldap_exec2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int			status = (int)NSS_STR_PARSE_SUCCESS;
	ns_ldap_entry_t		*entry;
	ns_ldap_result_t	*result = be->result;

	if (!argp->buf.result) {
		status = (int)NSS_STR_PARSE_ERANGE;
		goto result_exec2ent;
	}

	for (entry = result->entry; entry != NULL; entry = entry->next) {
		status = _exec_ldap_exec2ent(entry, argp);
		if (status != NSS_STR_PARSE_SUCCESS) {
			goto result_exec2ent;
		}
	}

result_exec2ent:
	(void) __ns_ldap_freeResult(&be->result);
	return (status);
}


static nss_status_t
_exec_process_val(ns_ldap_entry_t *entry, nss_XbyY_args_t *argp)
{
	int 		list, status;
	nss_status_t	nss_stat;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);

	status = _exec_ldap_exec2ent(entry, argp);
	switch (status) {
	case NSS_STR_PARSE_SUCCESS:
		argp->returnval = argp->buf.result;
		nss_stat = NSS_SUCCESS;
		argp->h_errno = ATTR_FOUND;
		if (_priv_exec->search_flag == GET_ALL) {
			list = _doexeclist(argp);
		}
		break;
	case NSS_STR_PARSE_ERANGE:
		argp->returnval = NULL;
		argp->erange = 1;
		nss_stat = NSS_NOTFOUND;
		break;
	case NSS_STR_PARSE_PARSE:
		argp->returnval = NULL;
		nss_stat = NSS_NOTFOUND;
		break;
	default:
		_free_execstr(_priv_exec->head_exec);
		argp->h_errno = ATTR_NO_RECOVERY;
		argp->returnval = NULL;
		nss_stat = NSS_UNAVAIL;
		break;
	}

	return (nss_stat);
}


static nss_status_t
_check_match(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int			i, status;
	unsigned long		len = 0L;
	int			got_entry = FALSE;
	nss_status_t		nss_stat = NSS_UNAVAIL;
	ns_ldap_attr_t		*attrptr;
	ns_ldap_entry_t		*entry;
	ns_ldap_result_t	*result = be->result;
	_priv_execattr *_priv_exec = (_priv_execattr *)(argp->key.attrp);

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: _check_match]\n");
#endif	/* DEBUG */

	argp->h_errno = ATTR_NO_RECOVERY;
	argp->returnval = NULL;
	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		goto result_check_match;
	}
	if (_priv_exec->id == NULL) {
		/*
		 * don't check for closest match of id when searching
		 * when id is not specified.
		 */
		got_entry = TRUE;
	}
	for (entry = result->entry; entry != NULL; entry = entry->next) {
		for (i = 0; i < entry->attr_count; i++) {
			attrptr = entry->attr_pair[i];
			if (attrptr == NULL) {
				goto result_check_match;
			}
			if ((got_entry == FALSE) &&
			    (strcasecmp(attrptr->attrname, _EXEC_ID) == 0)) {
				/*
				 * check for the closest match of id.
				 */
				len = strlen(attrptr->attrvalue[0]);
				if (attrptr->attrvalue[0] == '\0') {
					goto result_check_match;
				}
				if (strcmp(attrptr->attrvalue[0],
				    _priv_exec->id) == 0) {
					got_entry = TRUE;
					break;
				} else {
					status =
					    _exec_checkid(attrptr->attrvalue[0],
						_priv_exec->id,
						_priv_exec->type);
					if (status == GET_NO_MORE) {
						got_entry = TRUE;
						break;
					}
				}
			}
		}
		if (got_entry == TRUE) {
			if (_priv_exec->id != NULL) {
				got_entry = FALSE;
			}
			nss_stat = _exec_process_val(entry, argp);
			if (nss_stat != NSS_SUCCESS) {
				break;
			} else if ((nss_stat == NSS_SUCCESS) &&
			    (_priv_exec->search_flag != GET_ALL)) {
				break;
			}
		}
	}
	if ((nss_stat == NSS_SUCCESS) &&
	    (_priv_exec->search_flag == GET_ALL)) {
		argp->buf.result = _priv_exec->head_exec;
		argp->returnval = argp->buf.result;
#ifdef DEBUG
		_print_execstr(_priv_exec->head_exec);
#endif	/* DEBUG */
	}

result_check_match:
	(void) __ns_ldap_freeResult(&be->result);
	return (nss_stat);
}


static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	char		searchfilter[SEARCHFILTERLEN];
	nss_status_t	nss_stat;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*policy = _priv_exec->policy;
	const char	*type = _priv_exec->type;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbyname]\n");
#endif	/* DEBUG */

	if (snprintf(searchfilter, SEARCHFILTERLEN, _EXEC_GETEXECNAME,
	    ISWILD(name), ISWILD(policy), ISWILD(type)) < 0) {
		return ((nss_status_t)NSS_NOTFOUND);
	}
	nss_stat = _nss_ldap_nocb_lookup(be,
	    argp, _EXECATTR, searchfilter, NULL);
	if (nss_stat != (nss_status_t)NSS_SUCCESS) {
		return (nss_stat);
	}
	nss_stat = _check_match(be, argp);
	if ((nss_stat != (nss_status_t)NSS_SUCCESS) &&
	    (_priv_exec->search_flag == GET_ALL)) {
		_free_execstr(_priv_exec->head_exec);
	}

	return (nss_stat);
} 


static nss_status_t
getbyid(ldap_backend_ptr be, void *a)
{
	char		searchfilter[SEARCHFILTERLEN];
	nss_status_t	nss_stat;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*id = _priv_exec->id;
	const char	*policy = _priv_exec->policy;
	const char	*type = _priv_exec->type;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbyid]\n");
#endif	/* DEBUG */

	if (snprintf(searchfilter, SEARCHFILTERLEN, _EXEC_GETEXECID,
	    ISWILD(id), ISWILD(policy), ISWILD(type)) < 0) {
		return ((nss_status_t)NSS_NOTFOUND);
	}
	nss_stat = _nss_ldap_nocb_lookup(be,
	    argp, _EXECATTR, searchfilter, NULL);
	if (nss_stat != (nss_status_t)NSS_SUCCESS) {
		return (nss_stat);
	}
	nss_stat = _check_match(be, argp);
	if ((nss_stat != (nss_status_t)NSS_SUCCESS) &&
	    (_priv_exec->search_flag == GET_ALL)) {
		_free_execstr(_priv_exec->head_exec);
	}

	return (nss_stat);
} 


static nss_status_t
getbynameid(ldap_backend_ptr be, void *a)
{
	char		searchfilter[SEARCHFILTERLEN];
	nss_status_t	nss_stat;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*id = _priv_exec->id;
	const char	*policy = _priv_exec->policy;
	const char	*type = _priv_exec->type;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbynameid]\n");
#endif	/* DEBUG */

	if (snprintf(searchfilter, SEARCHFILTERLEN, _EXEC_GETEXECNAMEID,
	    ISWILD(name), ISWILD(id), ISWILD(policy), ISWILD(type)) < 0) {
		return ((nss_status_t)NSS_NOTFOUND);
	}
	nss_stat = _nss_ldap_nocb_lookup(be,
	    argp, _EXECATTR, searchfilter, NULL);
	if (nss_stat != (nss_status_t)NSS_SUCCESS) {
		return (nss_stat);
	}
	nss_stat = _check_match(be, argp);
	if ((nss_stat != (nss_status_t)NSS_SUCCESS) &&
	    (_priv_exec->search_flag == GET_ALL)) {
		_free_execstr(_priv_exec->head_exec);
	}

	return (nss_stat);
} 


static ldap_backend_op_t execattr_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbyname,
	getbyid,
	getbynameid
};


nss_backend_t *
_nss_ldap_exec_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5,
    const char *dummy6,
    const char *dummy7)
{
#ifdef	DEBUG
	(void) fprintf(stdout,
	    "\n[getexecattr.c: _nss_ldap_exec_attr_constr]\n");
#endif
	return ((nss_backend_t *)_nss_ldap_constr(execattr_ops,
		sizeof (execattr_ops)/sizeof (execattr_ops[0]), _EXECATTR,
		exec_attrs, _nss_ldap_exec2ent));
}
