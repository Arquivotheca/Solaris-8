/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauthattr.c	1.1	99/04/07 SMI"

#include <stdlib.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <string.h>
#include <auth_attr.h>


/* externs from parse.c */
extern char *_strtok_escape(char *, char *, char **);

static int authattr_stayopen = 0;
/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_authattr(nss_db_params_t *p)
{
	trace1(TR__nss_initf_authattr, 0);
	p->name = NSS_DBNAM_AUTHATTR;
	p->default_config = NSS_DEFCONF_AUTHATTR;
	trace1(TR__nss_initf_authattr, 1);
}


/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
str2authattr(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	char		*str = (char *)NULL;
	char		*last = (char *)NULL;
	char		*sep = KV_TOKEN_DELIMIT;
	authstr_t	*auth = (authstr_t *)ent;

	trace3(TR_str2authattr, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2authattr, 1, lenstr, buflen);
		return (NSS_STR_PARSE_PARSE);
	}
	if (lenstr >= buflen) {
		trace3(TR_str2authattr, 1, lenstr, buflen);
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

	auth->name = _strtok_escape(buffer, sep, &last);
	auth->res1 = _strtok_escape(NULL, sep, &last);
	auth->res2 = _strtok_escape(NULL, sep, &last);
	auth->short_desc = _strtok_escape(NULL, sep, &last);
	auth->long_desc = _strtok_escape(NULL, sep, &last);
	auth->attr = _strtok_escape(NULL, sep, &last);

	return (0);
}


void
_setauthattr(void)
{
	trace1(TR_setauthattr, 0);
	authattr_stayopen = 0;
	nss_setent(&db_root, _nss_initf_authattr, &context);
	trace1(TR_setauthattr, 0);
}


void
_endauthattr(void)
{
	trace1(TR_endauthattr, 0);
	authattr_stayopen = 0;
	nss_endent(&db_root, _nss_initf_authattr, &context);
	nss_delete(&db_root);
	trace1(TR_endauthattr, 0);
}


authstr_t *
_getauthattr(authstr_t *result, char *buffer, int buflen, int *h_errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t res;

	trace2(TR_getauthattr, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2authattr);
	res = nss_getent(&db_root, _nss_initf_authattr, &context, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR_getauthattr, 1, buflen);
	return (authstr_t *) NSS_XbyY_FINI(&arg);
}


authstr_t *
_getauthnam(const char *name, authstr_t *result, char *buffer, int buflen,
    int *errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t    res;

	trace2(TR_getauthnam, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2authattr);
	arg.key.name = name;
	arg.stayopen = authattr_stayopen;
	res = nss_search(&db_root, _nss_initf_authattr,
	    NSS_DBOP_AUTHATTR_BYNAME, &arg);
	arg.status = res;
	*errnop = arg.h_errno;
	trace2(TR_getauthnam, 1, buflen);
	return ((authstr_t *)NSS_XbyY_FINI(&arg));
}
