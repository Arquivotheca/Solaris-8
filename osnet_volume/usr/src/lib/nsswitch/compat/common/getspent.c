/*
 *	getspent.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 * lib/nsswitch/compat/getspent.c -- name-service-switch backend for getspnam()
 *   It looks in /etc/shadow; if it finds shadow entries there that begin
 *   with "+" or "-", it consults other services.  By default it uses NIS (YP),
 *   but the user can override this with a "passwd_compat" entry in
 *   /etc/nsswitch.conf, e.g.
 *			passwd_compat: nisplus
 * The main criterion for this code is that it behave in the same way as
 * the code for getpwnam() and friends (in getpwent.c).  Note that it uses
 * the same nsswitch.conf entry, not a separate entry for "shadow_compat".
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

#pragma ident	"@(#)getspent.c	1.11	97/08/12 SMI"

#include <shadow.h>
#include <string.h>
#include <stdlib.h>
#include "compat_common.h"

static DEFINE_NSS_DB_ROOT(db_root);

void
_nss_initf_shadow_compat(p)
	nss_db_params_t	*p;
{
	p->name		  = NSS_DBNAM_SHADOW;
	p->config_name	  = NSS_DBNAM_PASSWD_COMPAT;
	p->default_config = NSS_DEFCONF_PASSWD_COMPAT;
}

static const char *
get_spnamp(argp)
	nss_XbyY_args_t		*argp;
{
	struct spwd		*s = (struct spwd *)argp->returnval;

	return (s->sp_namp);
}

static int
check_spnamp(argp)
	nss_XbyY_args_t		*argp;
{
	struct spwd		*s = (struct spwd *)argp->returnval;

	return (strcmp(s->sp_namp, argp->key.name) == 0);
}

static nss_status_t
getbyname(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_compat_XY_all(be, argp, check_spnamp,
				NSS_DBOP_SHADOW_BYNAME));
}

/*ARGSUSED*/
static int
merge_spents(be, argp, fields)
	compat_backend_ptr_t	be;
	nss_XbyY_args_t		*argp;
	const char		**fields;
{
	struct spwd		*sp	= (struct spwd *)argp->buf.result;

	/*
	 * Don't allow overriding of the username;  apart from that,
	 *   anything is fair game.
	 */

	if (fields[1] != 0) {
		size_t	namelen = strlen(sp->sp_namp) + 1;
		size_t	passlen = strlen(fields[1])   + 1;

		/* ===> Probably merits an explanation... */
		if (namelen + passlen > argp->buf.buflen) {
			return (NSS_STR_PARSE_ERANGE);
		}
		if (sp->sp_namp != argp->buf.buffer) {
			memmove(argp->buf.buffer, sp->sp_namp, namelen);
			sp->sp_namp = argp->buf.buffer;
		}
		memcpy(argp->buf.buffer + namelen, fields[1], passlen);
	}

#define	override(field, longp)				\
	if ((field) != 0) {				\
		char	*end;				\
		long	val = strtol(field, &end, 10);	\
							\
		if (*end == '\0') {			\
			*(longp) = val;			\
		} else {				\
			return (NSS_STR_PARSE_PARSE);	\
		}					\
	}

	/* do not override last changed date, it never gets reset. */
	/* override(fields[2], &sp->sp_lstchg); */
	override(fields[3], &sp->sp_min);
	override(fields[4], &sp->sp_max);
	override(fields[5], &sp->sp_warn);
	override(fields[6], &sp->sp_inact);
	override(fields[7], &sp->sp_expire);
	override(fields[8], &sp->sp_flag);
	return (NSS_STR_PARSE_SUCCESS);
}

static compat_backend_op_t shadow_ops[] = {
	_nss_compat_destr,
	_nss_compat_endent,
	_nss_compat_setent,
	_nss_compat_getent,
	getbyname
};

/*ARGSUSED*/
nss_backend_t *
_nss_compat_shadow_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_compat_constr(shadow_ops,
				sizeof (shadow_ops) / sizeof (shadow_ops[0]),
				SHADOW,
				NSS_LINELEN_SHADOW,
				&db_root,
				_nss_initf_shadow_compat,
				1,
				get_spnamp,
				merge_spents));
}
