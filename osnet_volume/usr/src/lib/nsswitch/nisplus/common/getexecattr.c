/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getexecattr.c	1.2	99/09/23 SMI"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <exec_attr.h>
#include "nisplus_common.h"
#include "nisplus_tables.h"


extern nis_result *__nis_list_localcb(nis_name, u_int, int (*) (), void *);
/* externs from libnsl */
extern int _exec_checkid(char *, const char *, const char *);
extern int _doexeclist(nss_XbyY_args_t *);
extern void _free_execstr(execstr_t *);


typedef struct __exec_nisplus_args {
	nss_status_t		*resp;
	nss_XbyY_args_t		*argp;
	nisplus_backend_t	*be;
} _exec_nisplus_args;


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


static nss_status_t
_exec_process_val(_exec_nisplus_args * eargp, nis_object * obj)
{
	int			parse_stat;
	int			list = OK;
	nss_status_t		res;
	nss_XbyY_args_t		*argp = eargp->argp;
	nisplus_backend_t	*be = eargp->be;
	_priv_execattr *_priv_exec = (_priv_execattr *)(argp->key.attrp);

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: _exec_process_val]\n");
#endif	/* DEBUG */

	argp->h_errno = ATTR_NOT_FOUND;
	parse_stat = (be->obj2ent) (1, obj, argp);	/* passing one obj */
	switch (parse_stat) {
	case NSS_STR_PARSE_SUCCESS:
		argp->returnval = argp->buf.result;
		res = NSS_SUCCESS;
		argp->h_errno = ATTR_FOUND;
		if (_priv_exec->search_flag == GET_ALL) {
			list = _doexeclist(argp);
		}
		break;
	case NSS_STR_PARSE_ERANGE:
		argp->returnval = NULL;
		argp->erange = 1;
		res = NSS_NOTFOUND; /* We won't find this otherwise, anyway */
		break;
	case NSS_STR_PARSE_PARSE:
		argp->returnval = NULL;
		res = NSS_NOTFOUND;
		break;
	default:
		_free_execstr(_priv_exec->head_exec);
		res = NSS_UNAVAIL;
		argp->h_errno = ATTR_NO_RECOVERY;
		argp->returnval = NULL;
		break;
	}

	return (res);
}


static int
_check_match(nis_name table, nis_object * obj, void *eargs)
{
	int			len,
				parse_stat;
	int			status = GET_NEXT;
	int			got_entry = TRUE;
	char			*p, *val;
	struct entry_col	*ecol;
	nss_status_t		res;
	_exec_nisplus_args	*eargp = (_exec_nisplus_args *)eargs;
	nss_XbyY_args_t		*argp = eargp->argp;
	nisplus_backend_t	*be = eargp->be;
	_priv_execattr *_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char		*name = _priv_exec->name;
	const char		*type = _priv_exec->type;
	const char		*id = _priv_exec->id;
	const char		*policy = _priv_exec->policy;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: _check_match]\n");
#endif	/* DEBUG */

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ ||
	    obj->EN_data.en_cols.en_cols_len < EXECATTR_COL) {
		/*
		 * found one bad entry. try the next one.
		 */
		return (status);
	}
	ecol = obj->EN_data.en_cols.en_cols_val;
	if (policy != NULL) {
		/*
		 * check policy
		 */
		EC_SET(ecol, EXECATTR_NDX_POLICY, len, val);
		if (len == NULL) {
			return (status);
		}
		if (strcmp(val, policy) == 0) {
			got_entry = TRUE;
		}
	}
	if ((got_entry == TRUE) && (type != NULL)) {
		/*
		 * check type
		 */
		EC_SET(ecol, EXECATTR_NDX_TYPE, len, val);
		if (len == NULL) {
			return (status);
		}
		got_entry = (strcmp(val, type)) ? FALSE : TRUE;
	}
	if ((got_entry == TRUE) && (id != NULL)) {
		/*
		 * check for closest id
		 */
		EC_SET(ecol, EXECATTR_NDX_ID, len, val);
		if (len == NULL) {
			return (status);
		}
		if (strcmp(val, id) == 0) {
			got_entry = TRUE;
		} else {
#ifdef	DEBUG
		(void) fprintf(stdout, "\n[getexecattr.c: _exec_checkid]\n");
#endif	/* DEBUG */
			status = _exec_checkid(val, id, type);
			got_entry = (status == GET_NO_MORE) ? TRUE : FALSE;
		}
	}
	if (got_entry == TRUE) {
		res = _exec_process_val(eargp, obj);
		*(eargp->resp) = res;
		switch (res) {
		case NSS_UNAVAIL:
			status = GET_NO_MORE;
			break;
		case NSS_NOTFOUND:
			status = GET_NEXT;
			break;
		case NSS_SUCCESS:
			if (_priv_exec->search_flag == GET_ALL) {
				status = GET_NEXT;
			}
			break;
		default:
			status = GET_NO_MORE;
			break;
		}
	}

	return (status);
}


static nss_status_t
_exec_nisplus_lookup(nisplus_backend_t * be,
    nss_XbyY_args_t * argp,
    const char *column1,
    const char *key1,
    const char *column2,
    const char *key2)
{
	register int		i, status;
	char			key[MAX_INPUT];
	nis_object		*obj;
	nis_result		*r;
	nss_status_t		res = NSS_NOTFOUND;
	_exec_nisplus_args	eargs;
	_priv_execattr *_priv_exec = (_priv_execattr *)(argp->key.attrp);

	eargs.argp = argp;
	eargs.be = be;
	eargs.resp = &res;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: _exec_nisplus_lookup]\n");
#endif	/* DEBUG */

	if (column2 == NULL) {
		if (snprintf(key, MAX_INPUT, "[%s=%s]%s",
		    column1, key1, be->table_name) >= MAX_INPUT) {
			return (NSS_NOTFOUND);
		}
	} else if (snprintf(key, MAX_INPUT, "[%s=%s,%s=%s]%s",
	    column1, key1, column2, key2, be->table_name) >= MAX_INPUT) {
		return (NSS_NOTFOUND);
	}
	r = __nis_list_localcb(key,
	    NIS_LIST_COMMON | ALL_RESULTS | HARD_LOOKUP,
	    _check_match,
	    (void *)&eargs);
	if (r == NULL) {
		nis_freeresult(r);
	}
	if ((res == NSS_SUCCESS) && (_priv_exec->search_flag == GET_ALL)) {
		argp->buf.result = _priv_exec->head_exec;
		argp->returnval = argp->buf.result;
#ifdef	DEBUG
		_print_execstr(_priv_exec->head_exec);
#endif	/* DEBUG */
	}

	return (res);
}


static nss_status_t
getbyname(nisplus_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbyname]\n");
#endif	/* DEBUG */

	res = _exec_nisplus_lookup(be, argp,
	    EXECATTR_TAG_NAME, name, NULL, NULL);

	return (res);
}


static nss_status_t
getbyid(nisplus_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*id = _priv_exec->id;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbyid]\n");
#endif	/* DEBUG */

	res = _exec_nisplus_lookup(be, argp,
	    EXECATTR_TAG_ID, id, NULL, NULL);

	return (res);
}



static nss_status_t
getbynameid(nisplus_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*id = _priv_exec->id;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getexecattr.c: getbynameid]\n");
#endif	/* DEBUG */

	res = _exec_nisplus_lookup(be, argp,
	    EXECATTR_TAG_NAME, name, EXECATTR_TAG_ID, id);

	return (res);
}


/*
 * place the results from the nis_object structure into argp->buf.result
 * Returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 */
static int
nis_object2execstr(int nobj, nis_object * obj, nss_XbyY_args_t * argp)
{
	int			len;
	int			buflen = argp->buf.buflen;
	char			*limit, *val, *endnum, *nullstring;
	char			*buffer = NULL;
	char			*empty = "";
	execstr_t		*exec = NULL;
	struct entry_col	*ecol;

	limit = argp->buf.buffer + buflen;
	exec = (execstr_t *)argp->buf.result;
	buffer = argp->buf.buffer;

	if ((buffer == NULL) || (exec == NULL)) {
		return (NSS_STR_PARSE_PARSE);
	}

	/*
	 * If we got more than one nis_object, we just ignore object(s) except
	 * the first. Although it should never have happened.
	 *
	 * ASSUMPTION: All the columns in the NIS+ tables are null terminated.
	 */
	if (obj->zo_data.zo_type != ENTRY_OBJ ||
	    obj->EN_data.en_cols.en_cols_len < EXECATTR_COL) {
		/* namespace/table/object is curdled */
		return (NSS_STR_PARSE_PARSE);
	}
	ecol = obj->EN_data.en_cols.en_cols_val;

	/*
	 * execstr->name: profile name
	 */
	EC_SET(ecol, EXECATTR_NDX_NAME, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->name = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->name, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->type: exec type
	 */
	EC_SET(ecol, EXECATTR_NDX_TYPE, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->type = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->type, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->policy
	 */
	EC_SET(ecol, EXECATTR_NDX_POLICY, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->policy = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->policy, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->res1: reserved field 1
	 */
	EC_SET(ecol, EXECATTR_NDX_RES1, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->res1 = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->res1, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->res2: reserved field 2
	 */
	EC_SET(ecol, EXECATTR_NDX_RES2, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->res2 = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->res2, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->id: unique id
	 */
	EC_SET(ecol, EXECATTR_NDX_ID, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->id = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->id, val);
	nullstring = (buffer - 1);

	/*
	 * execstr->attrs: key-value pairs of attributes
	 */
	EC_SET(ecol, EXECATTR_NDX_ATTR, len, val);
	if (len < 1 || (*val == '\0')) {
		val = empty;
	}
	exec->attr = buffer;
	buffer += len;
	if (buffer >= limit) {
		return (NSS_STR_PARSE_ERANGE);
	}
	strcpy(exec->attr, val);
	nullstring = (buffer - 1);

	exec->next = (execstr_t *)NULL;

	return (NSS_STR_PARSE_SUCCESS);
}

static nisplus_backend_op_t execattr_ops[] = {
	_nss_nisplus_destr,
	_nss_nisplus_endent,
	_nss_nisplus_setent,
	_nss_nisplus_getent,
	getbyname,
	getbyid,
	getbynameid
};

nss_backend_t  *
_nss_nisplus_exec_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5,
    const char *dummy6,
    const char *dummy7)
{
	return (_nss_nisplus_constr(execattr_ops,
		sizeof (execattr_ops)/sizeof (execattr_ops[0]),
		EXECATTR_TBLNAME,
		nis_object2execstr));
}
