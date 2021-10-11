/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getexecattr.c	1.2	99/09/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <string.h>
#include <strings.h>
#include <sys/systeminfo.h>
#include <exec_attr.h>

/* externs from parse.c */
extern char *_strtok_escape(char *, char *, char **);
extern char *_strdup_null(char *);

execstr_t * _dup_execstr(execstr_t *);
void _free_execstr(execstr_t *);

static int execattr_stayopen = 0;

/*
 * Unsynchronized, but it affects only efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_execattr(nss_db_params_t * p)
{
	trace1(TR__nss_initf_execattr, 0);
	p->name = NSS_DBNAM_EXECATTR;
	p->config_name    = NSS_DBNAM_PROFATTR; /* use config for "prof_attr" */
	p->default_config = NSS_DEFCONF_EXECATTR;
	trace1(TR__nss_initf_execattr, 1);
}


/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ... The structure
 * pointer passed in is a structure in the caller's space wherein the field
 * pointers would be set to areas in the buffer if need be. instring and buffer
 * should be separate areas.
 */
int
str2execattr(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	char		*last = (char *)NULL;
	char		*sep = KV_TOKEN_DELIMIT;
	char		*empty = KV_EMPTY;
	execstr_t	*exec = (execstr_t *)ent;

	if (exec == NULL) {
		return(NSS_STR_PARSE_PARSE);
	}

	trace3(TR_str2execattr, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2execattr, 1, lenstr, buflen);
		return (NSS_STR_PARSE_PARSE);
	}
	if (lenstr >= buflen) {
		trace3(TR_str2execattr, 1, lenstr, buflen);
		return (NSS_STR_PARSE_ERANGE);
	}
	strncpy(buffer, instr, buflen);
	/*
	 * Remove newline that nis (yp_match) puts at the
	 * end of the entry it retrieves from the map.
	 */
	if (buffer[lenstr] == '\n') {
		buffer[lenstr] = '\0';
	}

	exec->name = _strtok_escape(buffer, sep, &last);
	exec->policy = _strtok_escape(NULL, sep, &last);
	exec->type = _strtok_escape(NULL, sep, &last);
	exec->res1 = _strtok_escape(NULL, sep, &last);
	exec->res2 = _strtok_escape(NULL, sep, &last);
	exec->id = _strtok_escape(NULL, sep, &last);
	exec->attr = _strtok_escape(NULL, sep, &last);
	exec->next = (execstr_t *)NULL;

	return (NSS_STR_PARSE_SUCCESS);
}


void
_setexecattr(void)
{
	trace1(TR_setexecattr, 0);
	execattr_stayopen = 0;
	nss_setent(&db_root, _nss_initf_execattr, &context);
	trace1(TR_setexecattr, 0);
}


void
_endexecattr(void)
{
	trace1(TR_endexecattr, 0);
	execattr_stayopen = 0;
	nss_endent(&db_root, _nss_initf_execattr, &context);
	nss_delete(&db_root);
	trace1(TR_endexecattr, 0);
}


execstr_t *
_getexecattr(execstr_t * result, char *buffer, int buflen, int *errnop)
{
	nss_status_t    res;
	nss_XbyY_args_t arg;

	trace2(TR_getexecattr, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2execattr);
	res = nss_getent(&db_root, _nss_initf_execattr, &context, &arg);
	arg.status = res;
	*errnop = arg.h_errno;
	trace2(TR_getexecattr, 1, buflen);

	return ((execstr_t *)NSS_XbyY_FINI(&arg));
}

execstr_t *
_getexecprof(char *name,
    char *type,
    char *id,
    int search_flag,
    execstr_t *result,
    char *buffer,
    int buflen,
    int *errnop)
{
	int		getby_flag;
	char		policy_buf[BUFSIZ];
	const char	*empty = (const char *)NULL;
	nss_status_t	res;
	nss_XbyY_args_t	arg;
	_priv_execattr	_priv_exec;

	trace2(TR_getexecprof, 0, _buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2execattr);

	_priv_exec.name = (name == NULL) ? empty : (const char *)name;
	_priv_exec.type = (type == NULL) ? empty : (const char *)type;
	_priv_exec.id = (id == NULL) ? empty : (const char *)id;
#ifdef SI_SECPOLICY
	if ((status = sysinfo(SI_SECPOLICY, policy_buf, BUFSIZ)) == -1)
#endif	/* SI_SECPOLICY */
	strncpy(policy_buf, DEFAULT_POLICY, BUFSIZ);
	_priv_exec.policy = policy_buf;
	_priv_exec.search_flag = search_flag;
	_priv_exec.head_exec = (execstr_t *)NULL;
	_priv_exec.prev_exec = (execstr_t *)NULL;

	if ((name != NULL) && (id != NULL)) {
		getby_flag = NSS_DBOP_EXECATTR_BYNAMEID;
	} else if (name != NULL) {
		getby_flag = NSS_DBOP_EXECATTR_BYNAME;
	} else if (id != NULL) {
		getby_flag = NSS_DBOP_EXECATTR_BYID;
	}

	arg.key.attrp = &(_priv_exec);
	arg.stayopen = execattr_stayopen;
	res = nss_search(&db_root, _nss_initf_execattr, getby_flag, &arg);
	if ((arg.returnval == 0) &&
	    (getby_flag == NSS_DBOP_EXECATTR_BYNAMEID)) {
		/*
		 * searching by name:id will return error if database
		 * has a wildcard for id. search only for name then
		 * and match the closest id. see _exec_checkid.
		 */
		getby_flag = NSS_DBOP_EXECATTR_BYNAME;
		res = nss_search(&db_root, _nss_initf_execattr, getby_flag,
		    &arg);
	}
	arg.status = res;
	*errnop = arg.h_errno;
	trace2(TR_getexecprof, 1, buflen);

	return ((execstr_t *)NSS_XbyY_FINI(&arg));
}


int
_doexeclist(nss_XbyY_args_t *argp)
{
	int		list_stat = OK;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	execstr_t	*exec = (execstr_t *)((argp->buf.result));

	if (_priv_exec->head_exec == NULL) {
		_priv_exec->head_exec = _dup_execstr(exec);
		if (_priv_exec->head_exec == NULL) {
			list_stat = NOT_OK;
		} else {
			_priv_exec->prev_exec = _priv_exec->head_exec;
		}
	} else {
		_priv_exec->prev_exec->next = _dup_execstr(exec);
		if (_priv_exec->prev_exec->next == NULL) {
			list_stat = NOT_OK;
		} else {
			_priv_exec->prev_exec = _priv_exec->prev_exec->next;
		}
	}
	memset(argp->buf.buffer, NULL, argp->buf.buflen);
	if (list_stat == NOT_OK) {
		_free_execstr(_priv_exec->head_exec);
		argp->h_errno = ATTR_NO_RECOVERY;
		argp->buf.result = NULL;
		argp->returnval = argp->buf.result;
	}

	return (list_stat);

}


/*
 * Looks for either an exact match or a generic one, of the command/class id
 * string in instr.
 * Eg., /bin/ls or any command in /bin or just * for KV_COMMAND type, and
 * com.sun.login.remote or com.sun.login.* for KV_JAVA_CLASS type.
 */
int
_exec_checkid(char *instr, const char *id, const char *type)
{
	int	status = GET_NEXT;
	char	c_id = '/';
	char	*tmpid = (char *)NULL;
	char	*pchar = (char *)NULL;

	if (type == NULL) {
		return (status);
	}
	if ((tmpid = strdup(id)) == NULL) {
		return (status);
	}
	if ((strcmp(type, KV_ACTION) == 0) ||
	    (strcmp(type, KV_JAVA_METHOD) == 0)) {
		if (strcmp(instr, KV_WILDCARD) == 0) {
			status = GET_NO_MORE;
		}
	} else if (strcmp(type, KV_COMMAND) == 0) {
		c_id = '/';
	} else if (strcmp(type, KV_JAVA_CLASS) == 0) {
		c_id = '.';
	}
	if ((pchar = rindex(tmpid, c_id)) != NULL) {
		*(++pchar) = KV_WILDCHAR;
		*(++pchar) = '\0';
		if (strcmp(instr, tmpid) == 0) {
			status = GET_NO_MORE;
		} else if (strcmp(instr, KV_WILDCARD) == 0) {
			status = GET_NO_MORE;
		}
	}
	if (tmpid != NULL) {
		free(tmpid);
	}

	return (status);
}


execstr_t *
_dup_execstr(execstr_t * old_exec)
{
	execstr_t *new_exec = (execstr_t *)NULL;

	if (old_exec == NULL) {
		return ((execstr_t *)NULL);
	}
	if ((new_exec = (execstr_t *)malloc(sizeof (execstr_t))) != NULL) {
		new_exec->name = _strdup_null(old_exec->name);
		new_exec->type = _strdup_null(old_exec->type);
		new_exec->policy = _strdup_null(old_exec->policy);
		new_exec->res1 = _strdup_null(old_exec->res1);
		new_exec->res2 = _strdup_null(old_exec->res2);
		new_exec->id = _strdup_null(old_exec->id);
		new_exec->attr = _strdup_null(old_exec->attr);
		new_exec->next = old_exec->next;
	}
	return (new_exec);
}

void
_free_execstr(execstr_t * exec)
{
	if (exec != NULL) {
		free(exec->name);
		free(exec->type);
		free(exec->policy);
		free(exec->res1);
		free(exec->res2);
		free(exec->id);
		free(exec->attr);
		_free_execstr(exec->next);
		free(exec);
	}
}
