/*
 *	getpwent.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 * lib/nsswitch/compat/getpwent.c -- name-service-switch backend for getpwnam()
 *   et al that does 4.x compatibility.  It looks in /etc/passwd; if it finds
 *   passwd entries there that begin with "+" or "-", it consults other
 *   services.  By default it uses NIS (YP), but the user can override this
 *   with a "passwd_compat" entry in /etc/nsswitch.conf, e.g.
 *			passwd_compat: nisplus
 *
 * This code tries to produce the same results as the 4.x code, even when
 *   the latter seems ill thought-out (mostly in the handling of netgroups,
 *   "-", and the combination thereof).  Bug-compatible, in other words.
 *   Though we do try to be more reasonable about the format of "+" and "-"
 *   entries here, i.e. you don't have to pad them with spurious colons and
 *   bogus uid/gid values.
 *
 * Caveats:
 *    -	More than one source may be specified, with the usual switch semantics,
 *	but having multiple sources here is definitely odd.
 *    -	People who recursively specify "compat" deserve what they get.
 *    -	Entries that begin with "+@" or "-@" are interpreted using
 *	getnetgrent() and innetgr(), which use the "netgroup" entry in
 *	/etc/nsswitch.conf.  If the sources for "passwd_compat" and "netgroup"
 *	differ, everything should work fine, but the semantics will be pretty
 *	confusing.
 */

#pragma ident	"@(#)getpwent.c	1.11	97/08/12 SMI"

#include <pwd.h>
#include <shadow.h>		/* For PASSWD (pathname to passwd file) */
#include <stdlib.h>
#include <strings.h>
#include "compat_common.h"

static DEFINE_NSS_DB_ROOT(db_root);

void
_nss_initf_passwd_compat(p)
	nss_db_params_t	*p;
{
	p->name		  = NSS_DBNAM_PASSWD;
	p->config_name	  = NSS_DBNAM_PASSWD_COMPAT;
	p->default_config = NSS_DEFCONF_PASSWD_COMPAT;
}

static const char *
get_pwname(argp)
	nss_XbyY_args_t		*argp;
{
	struct passwd		*p = (struct passwd *)argp->returnval;

	return (p->pw_name);
}

static int
check_pwname(argp)
	nss_XbyY_args_t		*argp;
{
	struct passwd		*p = (struct passwd *)argp->returnval;

	return (strcmp(p->pw_name, argp->key.name) == 0);
}

static nss_status_t
getbyname(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_compat_XY_all(be, argp,
				check_pwname, NSS_DBOP_PASSWD_BYNAME));
}

static int
check_pwuid(argp)
	nss_XbyY_args_t		*argp;
{
	struct passwd		*p = (struct passwd *)argp->returnval;

	return (p->pw_uid == argp->key.uid);
}

static nss_status_t
getbyuid(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_compat_XY_all(be, argp,
				check_pwuid, NSS_DBOP_PASSWD_BYUID));
}

/*ARGSUSED*/
static int
merge_pwents(be, argp, fields)
	compat_backend_ptr_t	be;
	nss_XbyY_args_t		*argp;
	const char		**fields;
{
	struct passwd		*pw	= (struct passwd *)argp->buf.result;
	char			*buf	= malloc(NSS_LINELEN_PASSWD);
	char			*s;
	int			parsestat;

	if (buf == 0) {
		return (NSS_STR_PARSE_PARSE);
		/* Really "out of memory", but PARSE_PARSE will have to do */
	}
	/*
	 * Don't allow overriding of
	 *	- username
	 *	- uid
	 *	- gid
	 * That's what the SunOS 4.x code did;  who are we to question it...
	 */
	s = buf;
	sprintf(s, "%s:", pw->pw_name);
	s += strlen(s);
	if (fields[1] != 0) {
		strcpy(s, fields[1]);
	} else {
		strcpy(s, pw->pw_passwd);
		if (pw->pw_age != 0) {
			s += strlen(s);
/* ====> Does this do the right thing? */
			sprintf(s, ",%s", pw->pw_age);
		}
	}
	s += strlen(s);
	sprintf(s, ":%d:%d:%s:%s:%s",
		pw->pw_uid,
		pw->pw_gid,
		fields[4] != 0 ? fields[4] : pw->pw_gecos,
		fields[5] != 0 ? fields[5] : pw->pw_dir,
		fields[6] != 0 ? fields[6] : pw->pw_shell);
	s += strlen(s);
	parsestat = (*argp->str2ent)(buf, s - buf,
				    argp->buf.result,
				    argp->buf.buffer,
				    argp->buf.buflen);
	free(buf);
	return (parsestat);
}

static compat_backend_op_t passwd_ops[] = {
	_nss_compat_destr,
	_nss_compat_endent,
	_nss_compat_setent,
	_nss_compat_getent,
	getbyname,
	getbyuid
};

/*ARGSUSED*/
nss_backend_t *
_nss_compat_passwd_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_compat_constr(passwd_ops,
				sizeof (passwd_ops) / sizeof (passwd_ops[0]),
				PASSWD,
				NSS_LINELEN_PASSWD,
				&db_root,
				_nss_initf_passwd_compat,
				1,
				get_pwname,
				merge_pwents));
}
