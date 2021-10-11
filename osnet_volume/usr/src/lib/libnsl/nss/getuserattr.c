/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getuserattr.c	1.2	99/09/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <string.h>
#include <user_attr.h>

/* externs from libc */
extern void _nss_XbyY_fgets(FILE *, nss_XbyY_args_t *);
/* externs from parse.c */
extern char *_strtok_escape(char *, char *, char **);


static int userattr_stayopen;

/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);


void
_nss_initf_userattr(nss_db_params_t *p)
{
	trace1(TR__nss_initf_userattr, 0);
	p->name = NSS_DBNAM_USERATTR;
	p->config_name    = NSS_DBNAM_PASSWD; /* use config for "passwd" */
	p->default_config = NSS_DEFCONF_USERATTR;
	trace1(TR__nss_initf_userattr, 1);
}


/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
str2userattr(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	char		*str = (char *)NULL;
	char		*last = (char *)NULL;
	char		*sep = KV_TOKEN_DELIMIT;
	userstr_t	*user = (userstr_t *)ent;

	trace3(TR_str2userattr, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2userattr, 1, lenstr, buflen);
		return (NSS_STR_PARSE_PARSE);
	}
	if (lenstr >= buflen) {
		trace3(TR_str2userattr, 1, lenstr, buflen);
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

	user->name = _strtok_escape(buffer, sep, &last);
	user->qualifier = _strtok_escape(NULL, sep, &last);
	user->res1 = _strtok_escape(NULL, sep, &last);
	user->res2 = _strtok_escape(NULL, sep, &last);
	user->attr = _strtok_escape(NULL, sep, &last);

	return (0);
}


void
_setuserattr(void)
{
	trace1(TR_setuserattr, 0);
	userattr_stayopen = 0;
	nss_setent(&db_root, _nss_initf_userattr, &context);
	trace1(TR_setuserattr, 0);
}


void
_enduserattr(void)
{
	trace1(TR_enduserattr, 0);
	userattr_stayopen = 0;
	nss_endent(&db_root, _nss_initf_userattr, &context);
	nss_delete(&db_root);
	trace1(TR_enduserattr, 0);
}


userstr_t *
_getuserattr(userstr_t *result, char *buffer, int buflen, int *h_errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t    res;

	trace2(TR_getuserattr, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2userattr);
	res = nss_getent(&db_root, _nss_initf_userattr, &context, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR_getuserattr, 1, buflen);
	return ((userstr_t *)NSS_XbyY_FINI(&arg));
}


userstr_t *
_fgetuserattr(FILE *f, userstr_t *result, char *buffer, int buflen)
{
	nss_XbyY_args_t arg;

	trace2(TR_getuserattr, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2userattr);
	_nss_XbyY_fgets(f, &arg);
	trace2(TR_getuserattr, 1, buflen);
	return ((userstr_t *)NSS_XbyY_FINI(&arg));
}



userstr_t *
_getusernam(const char *name, userstr_t *result, char *buffer, int buflen,
    int *errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t    res;

	trace2(TR_getusernam, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2userattr);
	arg.key.name = name;
	arg.stayopen = userattr_stayopen;
	res = nss_search(&db_root, _nss_initf_userattr,
	    NSS_DBOP_USERATTR_BYNAME, &arg);
	arg.status = res;
	*errnop = arg.h_errno;
	trace2(TR_getusernam, 1, buflen);
	return ((userstr_t *)NSS_XbyY_FINI(&arg));
}
