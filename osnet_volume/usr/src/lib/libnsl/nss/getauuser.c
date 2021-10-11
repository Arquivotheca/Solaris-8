/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getauuser.c	1.2	99/09/22 SMI"

#include <stdlib.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <string.h>
#include <bsm/libbsm.h>
#include <secdb.h>


/* externs from parse.c */
extern char *_strtok_escape(char *, char *, char **);

static int auuser_stayopen;

/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);


void
_nss_initf_auuser(nss_db_params_t *p)
{
	trace1(TR__nss_initf_auuser, 0);
	p->name	= NSS_DBNAM_AUDITUSER;
	p->config_name    = NSS_DBNAM_PASSWD;  /* use config for "passwd" */
	p->default_config = NSS_DEFCONF_AUDITUSER;
	trace1(TR__nss_initf_auuser, 1);
}


/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
str2auuser(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	char		*last = (char *)NULL;
	char		*sep = KV_TOKEN_DELIMIT;
	au_user_str_t	*au_user = (au_user_str_t *)ent;

	trace3(TR_str2auuser, 0, lenstr, buflen);
	if ((instr >= buffer && (buffer + buflen) > instr) ||
	    (buffer >= instr && (instr + lenstr) > buffer)) {
		trace3(TR_str2auuser, 1, lenstr, buflen);
		return (NSS_STR_PARSE_PARSE);
	}
	if (lenstr >= buflen) {
		trace3(TR_str2auuser, 1, lenstr, buflen);
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

	au_user->au_name = _strtok_escape(buffer, sep, &last);
	au_user->au_always = _strtok_escape(NULL, sep, &last);
	au_user->au_never = _strtok_escape(NULL, sep, &last);

	return (0);
}


void
_setauuser(void)
{
	trace1(TR_setauuser, 0);
	auuser_stayopen = 0;
	nss_setent(&db_root, _nss_initf_auuser, &context);
	trace1(TR_setauuser, 0);
}


_endauuser(void)
{
	trace1(TR_endauuser, 0);
	auuser_stayopen = 0;
	nss_endent(&db_root, _nss_initf_auuser, &context);
	nss_delete(&db_root);
	trace1(TR_endauuser, 0);
	return (0);
}


au_user_str_t *
_getauuserent(au_user_str_t *result, char *buffer, int buflen, int *h_errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t    res;

	trace2(TR_getauuser, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2auuser);
	res = nss_getent(&db_root, _nss_initf_auuser, &context, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR_getauuser, 1, buflen);
	return (au_user_str_t *) NSS_XbyY_FINI(&arg);
}


au_user_str_t *
_getauusernam(const char *name, au_user_str_t *result, char *buffer,
    int buflen, int *errnop)
{
	nss_XbyY_args_t arg;
	nss_status_t    res;

	trace2(TR_getauusernam, 0, buflen);
	if (result == NULL) {
		*errnop = AUDITUSER_PARSE_ERANGE;
		return (NULL);
	}
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2auuser);
	arg.key.name = name;
	arg.stayopen = auuser_stayopen;
	arg.h_errno = AUDITUSER_NOT_FOUND;
	res = nss_search(&db_root, _nss_initf_auuser,
	    NSS_DBOP_AUDITUSER_BYNAME, &arg);
	arg.status = res;
	*errnop = arg.h_errno;
	trace2(TR_getauusernam, 1, buflen);
	return ((au_user_str_t *)NSS_XbyY_FINI(&arg));
}
