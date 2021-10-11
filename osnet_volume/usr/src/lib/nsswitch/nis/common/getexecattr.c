/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getexecattr.c	1.1	99/06/14 SMI"


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <exec_attr.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include "nis_common.h"


/* extern from nis_common.c */
extern void massage_netdb(const char **, int *);
/* externs from libnsl */
extern int _exec_checkid(char *, const char *, const char *);
extern int _doexeclist(nss_XbyY_args_t *);
extern void _free_execstr(execstr_t *);


typedef int (*nis_do_all_cback_t) (int,
	char *,
	int,
	char *,
	int,
	void *);

typedef struct _nis_XbyY_data {
	int	netdb;
	const char *filter;
	nss_status_t nss_stat;
	nis_backend_ptr_t be;
	nss_XbyY_args_t *argp;
	nis_do_all_cback_t cb_func;
	nis_XY_check_func check_func;
	nis_do_all_func_t iter_func;
} nis_XbyY_data;


static int
check_match(nss_XbyY_args_t * argp)
{
	int		status = GET_NEXT;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*type = _priv_exec->type;
	const char	*id = _priv_exec->id;
	const char	*policy = _priv_exec->policy;
	execstr_t	*exec = (execstr_t *)(argp->returnval);

	if ((policy != NULL) && (strcmp(exec->policy, policy) != 0)) {
		return (FALSE);
	}
	if ((name != NULL) && (strcmp(exec->name, name) != 0)) {
		return (FALSE);
	}
	if ((type != NULL) && (strcmp(exec->type, type) != 0)) {
		return (FALSE);
	}
	if ((id != NULL) && (strcmp(exec->id, id) != 0)) {
		status = _exec_checkid(exec->id, id, type);
		if (status == GET_NEXT) {
			return (FALSE);
		}
	}
	return (TRUE);
}


static  nss_status_t
_exec_nis_iter(const char *instr, int instr_len, void *data)
{
	int		parse_stat;
	int		list = OK;
	nis_XbyY_data	*xy_data = (nis_XbyY_data *)data;
	nss_XbyY_args_t	*argp = xy_data->argp;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);

	argp->h_errno = ATTR_NOT_FOUND;
	if (xy_data->netdb != NULL) {
		massage_netdb(&instr, &instr_len);
	}
	parse_stat = (*argp->str2ent) (instr,
	    instr_len,
	    argp->buf.result,
	    argp->buf.buffer,
	    argp->buf.buflen);
	switch (parse_stat) {
	case NSS_STR_PARSE_SUCCESS:
		argp->returnval = argp->buf.result;
		if ((*xy_data->check_func) (argp)) {
			xy_data->nss_stat = NSS_SUCCESS;
			argp->h_errno = ATTR_FOUND;
			if (_priv_exec->search_flag == GET_ALL) {
				if ((list = _doexeclist(argp)) == NOT_OK) {
					xy_data->nss_stat = NSS_UNAVAIL;
				}
			}
		} else {
			xy_data->nss_stat = NSS_NOTFOUND;
			argp->returnval = 0;
		}
		break;
	case NSS_STR_PARSE_ERANGE:
		/*
		 * If we got here because (*str2ent)() found that the buffer
		 * wasn't big enough, maybe we should quit and return erange.
		 * Instead we'll keep looking and eventually return "not found"
		 * -- it's a bug, but not an earth-shattering one.
		 */
		argp->erange = 1;	/* <== Is this a good idea? */
		xy_data->nss_stat = NSS_NOTFOUND;
		break;
	default:
		_free_execstr(_priv_exec->head_exec);
		xy_data->nss_stat = NSS_UNAVAIL;
		argp->h_errno = ATTR_NO_RECOVERY;
		argp->returnval = NULL;
		break;
	}	/* (parsestat == NSS_STR_PARSE_PARSE) won't happen ! */

	return (xy_data->nss_stat);
}


static int
_exec_nis_cb(int instatus,
    char *inkey,
    int inkeylen,
    char *inval,
    int invallen,
    void *indata)
{
	int		stat = GET_NEXT;
	char		*filter = NULL;
	char		*key = NULL;
	nss_status_t	res;
	nis_XbyY_data	*xy_data = (nis_XbyY_data *)indata;
	nss_XbyY_args_t	*argp = xy_data->argp;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);

	if (instatus != YP_TRUE) {
		return (GET_NEXT);	/* yp_all may decide otherwise... */
	}
	if ((filter = strdup(xy_data->filter)) != NULL) {
		key = strtok(filter, KV_TOKEN_DELIMIT);
	}
	/*
	 * Optimization:  if the entry doesn't contain the
	 * filter string then it can't be the entry we want, so
	 * don't bother looking more closely at it.
	 */
	if (key == NULL) {
		if ((xy_data->filter != NULL) &&
		    (strstr(inval, xy_data->filter) == NULL)) {
			return (GET_NEXT);
		}
	} else {
		do {
			if (strstr(inval, key) != NULL) {
				break;
			}
		} while ((key = strtok(NULL, KV_TOKEN_DELIMIT)) != NULL);
		if (key == NULL) {
			return (GET_NEXT);
		}
	}
	/*
	 * yp_all does not null terminate the entry it
	 * retrieves from the map, unlike yp_match. so
	 * we do it explicitly here.
	 */
	inval[invallen] = '\0';
	res = (*xy_data->iter_func) (inval, invallen, xy_data);

	switch (res) {
	case NSS_UNAVAIL:
		stat = GET_NO_MORE;
		break;
	case NSS_NOTFOUND:
		stat = GET_NEXT;
		break;
	case NSS_SUCCESS:
		if (_priv_exec->search_flag == GET_ONE) {
			stat = GET_NO_MORE;
		}
		break;
	}
	return (stat);
}


/*
 *
 * _exec_nis_XY_all proceeds on the same lines as nss_nis_XY_all,
 * except that it gives us flexibility to control the callback and
 * iteration functions.
 *
 */
static  nss_status_t
_exec_nis_XY_all(nis_XbyY_data * xy_data)
{
	int			ypall_stat = 0;
	nss_XbyY_args_t		*argp = xy_data->argp;
	struct ypall_callback	cback;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);

	cback.foreach = xy_data->cb_func;
	cback.data = (void *)(xy_data);
	/*
	 * yp_all returns only when the transaction is completed
	 * (successfully/unsuccessfully), or the iter_func decides that it does
	 * not want to see any more key-value pairs.
	 */
	ypall_stat = yp_all((char *)(xy_data->be->domain),
	    (char *)(xy_data->be->enum_map),
	    &cback);

	switch (ypall_stat) {
	case NULL:
		if (_priv_exec->search_flag == GET_ALL) {
			argp->buf.result = _priv_exec->head_exec;
			argp->returnval = argp->buf.result;
		}
		return (xy_data->nss_stat);
	case YPERR_BUSY:	/* Probably never get this, but... */
		return (NSS_TRYAGAIN);
	default:
		_free_execstr(_priv_exec->head_exec);
		argp->returnval = NULL;
		argp->h_errno = ATTR_NO_RECOVERY;
		return (NSS_UNAVAIL);
	}
}


static  nss_status_t
getbynam(nis_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nis_XbyY_data	xy_data;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;

	xy_data.netdb = 1;
	xy_data.filter = name;
	xy_data.be = be;
	xy_data.argp = argp;
	xy_data.cb_func = _exec_nis_cb;
	xy_data.check_func = check_match;
	xy_data.iter_func = _exec_nis_iter;

	res = _exec_nis_XY_all(&xy_data);

	return (res);
}

static  nss_status_t
getbyid(nis_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nis_XbyY_data	xy_data;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*id = _priv_exec->id;

	xy_data.netdb = 1;
	xy_data.filter = id;
	xy_data.be = be;
	xy_data.argp = argp;
	xy_data.cb_func = _exec_nis_cb;
	xy_data.check_func = check_match;
	xy_data.iter_func = _exec_nis_iter;

	res = _exec_nis_XY_all(&xy_data);

	return (res);
}


static  nss_status_t
getbynameid(nis_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nis_XbyY_data	xy_data;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*id = _priv_exec->id;
	char		key[MAX_INPUT];

	if (snprintf(key, MAX_INPUT, "%s%s%s", name, KV_TOKEN_DELIMIT, id) >=
	    MAX_INPUT) {
		return (NSS_NOTFOUND);
	}
	xy_data.netdb = 1;
	xy_data.filter = key;
	xy_data.be = be;
	xy_data.argp = argp;
	xy_data.cb_func = _exec_nis_cb;
	xy_data.check_func = check_match;
	xy_data.iter_func = _exec_nis_iter;

	res = _exec_nis_XY_all(&xy_data);

	return (res);
}


static nis_backend_op_t execattr_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbynam,
	getbyid,
	getbynameid
};

nss_backend_t *
_nss_nis_exec_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5,
    const char *dummy6,
    const char *dummy7)
{
	return (_nss_nis_constr(execattr_ops,
		sizeof (execattr_ops)/sizeof (execattr_ops[0]),
		NIS_MAP_EXECATTR));
}
