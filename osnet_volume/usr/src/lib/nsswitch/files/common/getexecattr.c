/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getexecattr.c	1.1	99/06/11 SMI"

#include <stdlib.h>
#include <exec_attr.h>
#include "files_common.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>

/*
 * files/getexecattr.c -- "files" backend for nsswitch "exec_attr" database
 *
 * _execattr_files_read_line and _execattr_files_XY_all code based on
 * nss_files_read_line and nss_files_XY_all respectively, from files_common.c
 */

/* externs from libnsl */
extern int _exec_checkid(char *, const char *, const char *);
extern int _doexeclist(nss_XbyY_args_t *);
extern int _readbufline(char *, int, char *, int, int *);


typedef int (*_exec_XY_check_func) (nss_XbyY_args_t *);


static int
check_match(nss_XbyY_args_t * argp)
{
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	int		status = GET_NEXT;
	int		got_type;
	int		got_id;
	const char	*name = _priv_exec->name;
	const char	*type = _priv_exec->type;
	const char	*id = _priv_exec->id;
	const char	*policy = _priv_exec->policy;
	execstr_t	*exec = (execstr_t *)argp->returnval;

	got_type = (type == NULL) ? TRUE : FALSE;
	got_id = (id == NULL) ? TRUE : FALSE;
	if ((policy != NULL) && (exec->policy != NULL)) {
		if (strcmp(exec->policy, policy) != 0) {
			return (status);
		}
	}
	if ((name != NULL) && (exec->name != NULL)) {
		switch (strcmp(exec->name, name)) {
		case 0:
			status = GET_NO_MORE;
			break;
		default:
			return (status);
		}
	}
	if ((exec->type != NULL) && (got_type == FALSE)) {
		switch (strcmp(exec->type, type)) {
		case 0:
			got_type = TRUE;
			status = GET_NO_MORE;
			break;
		default:
			status = GET_NEXT;
			return (status);
		}
	}
	if ((exec->id != NULL) && (got_id == FALSE)) {
		if (strcmp(exec->id, id) == 0) {
			got_id = TRUE;
		} else {
			status = _exec_checkid(exec->id, id, type);
			if (status == GET_NO_MORE) {
				got_id = TRUE;
			}
		}
	}
	if ((got_type == FALSE) || (got_id == FALSE)) {
		status = GET_NEXT;
	}

	return (status);
}


static nss_status_t
_exec_files_XY_all(files_backend_ptr_t be,
    nss_XbyY_args_t * argp,
    int netdb,
    const char *filter,
    _exec_XY_check_func check)
{
	int		parse_stat = 0;
	int		lastlen = 0;
	int		exec_fd = 0;
	int		mapsize = NSS_MMAPLEN_EXECATTR;
	int		no_mmap = FALSE;
	int		list = OK;
	char		*key = NULL;
	char		*index = NULL;
	char		*mapbuf = NULL;
	struct stat	f_stat;
	nss_status_t	res;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);

	if ((be->buf == NULL) &&
		((be->buf = (char *)malloc(be->minbuf)) == NULL)) {
		argp->h_errno = ATTR_NO_RECOVERY;
		return (NSS_UNAVAIL);
	}
	if (check || (be->f == NULL)) {
		if ((res = _nss_files_setent(be, 0)) != NSS_SUCCESS) {
			argp->h_errno = ATTR_NO_RECOVERY;
			return (res);
		}
	}
	exec_fd = fileno(be->f);
	if (fstat(exec_fd, &f_stat) == NULL) {
		mapsize = f_stat.st_size;
	}
	mapbuf = (char *)mmap(0,
	    mapsize,
	    PROT_READ,
	    MAP_PRIVATE,
	    exec_fd,
	    (off_t) 0);
	if (mapbuf == (char *)-1) {
		no_mmap = TRUE;
	}
	res = NSS_NOTFOUND;
	while (TRUE) {
		int	linelen = 0;
		int	check_stat = 0;
		char	*instr = be->buf;

		linelen =
		    (no_mmap == TRUE) ?
		    _nss_files_read_line(be->f, instr, be->minbuf) :
		    _readbufline(mapbuf,
			mapsize,
			instr,
			be->minbuf,
			&lastlen);

		if (linelen < 0) {
			/* End of file */
			argp->erange = 0;
			argp->h_errno = (_priv_exec->head_exec == NULL) ?
			    ATTR_NOT_FOUND : ATTR_FOUND;
			argp->returnval = _priv_exec->head_exec;
			break;
		}
		/*
		 * If the entry doesn't contain the filter string then
		 * it can't be the entry we want, so don't bother looking
		 * more closely at it.
		 */
		if ((filter != NULL) &&
		    ((index = strdup(filter)) != NULL) &&
		    ((key = strtok(index, KV_TOKEN_DELIMIT)) != NULL)) {
			do {
				if (strstr(instr, key) != NULL) {
					break;
				}
			} while ((key = strtok(NULL, KV_TOKEN_DELIMIT)) !=
			    NULL);
			if (key == NULL) {
				continue;
			}
		} else if ((filter != NULL) &&
		    (strstr(instr, filter) == NULL)) {
			continue;
		}
		if (netdb) {
			char	*first;
			char	*last;

			if ((last = strchr(instr, '#')) == NULL) {
				last = instr + linelen;
			}
			*last-- = '\0';	/* Nuke '\n' or #comment */

			/*
			 * Skip leading whitespace.  Normally there isn't any,
			 * so it's not worth calling strspn().
			 */
			for (first = instr; isspace(*first); first++) {
				;
			}
			if (*first == '\0') {
				continue;
			}

			/*
			 * Found something non-blank on the line.  Skip back
			 * over any trailing whitespace;  since we know there's
			 * non-whitespace earlier in the line, checking for
			 * termination is easy.
			 */
			while (isspace(*last)) {
				--last;
			}
			linelen = last - first + 1;
			if (first != instr) {
				instr = first;
			}
		}
		argp->returnval = NULL;
		argp->h_errno = ATTR_NOT_FOUND;
		parse_stat = (*argp->str2ent)(instr,
		    linelen,
		    argp->buf.result,
		    argp->buf.buffer,
		    argp->buf.buflen);
		if (parse_stat == NSS_STR_PARSE_SUCCESS) {
			argp->returnval = argp->buf.result;
			if ((check == NULL) || (*check) (argp)) {
				res = NSS_SUCCESS;
				argp->h_errno = ATTR_FOUND;
				if (_priv_exec->search_flag == GET_ONE) {
					break;
				} else if ((list = _doexeclist(argp)) ==
				    NOT_OK) {
					break;
				}
			} else {
				argp->returnval = NULL;
				memset(argp->buf.buffer, NULL,
				    argp->buf.buflen);
			}
		} else if (parse_stat == NSS_STR_PARSE_ERANGE) {
			argp->erange = 1;
		} /* else if (parse_stat == NSS_STR_PARSE_PARSE) don't care ! */
	}

	/*
	 * Always unmmap the file. Its not tied to stayopen.
	 */
	if (no_mmap == FALSE) {
		(void) munmap(mapbuf, mapsize);
	}

	/*
	 * stayopen is set to 0 by default in order to close the opened file.
	 * Some applications may break if it is set to 1.
	 */
	if (check && (argp->stayopen == NULL)) {
		(void) _nss_files_endent(be, 0);
	}
	return (res);
}


static nss_status_t
getbynam(files_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;

	if (_priv_exec->search_flag == GET_ALL) {
		res = _exec_files_XY_all(be, argp, 1, name, check_match);
	} else {
		res = _nss_files_XY_all(be, argp, 1, name, check_match);
		argp->h_errno = (argp->returnval == NULL) ?
			ATTR_NOT_FOUND : ATTR_FOUND;
	}

	return (res);
}


static nss_status_t
getbyid(files_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*id = _priv_exec->id;

	if (_priv_exec->search_flag == GET_ALL) {
		res = _exec_files_XY_all(be, argp, 1, id, check_match);
	} else {
		res = _nss_files_XY_all(be, argp, 1, id, check_match);
		argp->h_errno = (argp->returnval == NULL) ?
			ATTR_NOT_FOUND : ATTR_FOUND;
	}

	return (res);
}


static nss_status_t
getbynameid(files_backend_ptr_t be, void *a)
{
	nss_status_t	res;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	_priv_execattr	*_priv_exec = (_priv_execattr *)(argp->key.attrp);
	const char	*name = _priv_exec->name;
	const char	*id = _priv_exec->id;
	char		key[MAX_INPUT];

	if (snprintf(key, MAX_INPUT, "%s%s%s", name, KV_TOKEN_DELIMIT, id) >=
	    MAX_INPUT) {
		return (NSS_NOTFOUND);
	}

	res = _exec_files_XY_all(be, argp, 1, key, check_match);

	return (res);
}


static files_backend_op_t execattr_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbynam,
	getbyid,
	getbynameid
};

nss_backend_t  *
_nss_files_exec_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5,
    const char *dummy6,
    const char *dummy7)
{
	return (_nss_files_constr(execattr_ops,
		sizeof (execattr_ops)/sizeof (execattr_ops[0]),
		EXECATTR_FILENAME,
		NSS_LINELEN_EXECATTR,
		NULL));
}
