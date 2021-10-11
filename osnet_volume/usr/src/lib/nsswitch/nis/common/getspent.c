/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/getspent.c -- "nis" backend for nsswitch "shadow" database
 */

#pragma ident "@(#)getspent.c	1.11	97/04/17	SMI"

#include <shadow.h>
#include <string.h>
#include "nis_common.h"

/*
 * Most of the information in a struct spwd simply isn't available from the
 * YP maps, we dummy out all the numeric fields and just get sp_namp and
 * sp_pwdp (name and password) from the YP passwd map.  Thus we don't
 * use the str2ent() routine that's passed to us, but instead have our
 * own dummy routine:
 *
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas. Let's not
 * fight over crumbs.
 */
static int
nis_str2spent(instr, lenstr, ent, buffer, buflen)
	const char		*instr;
	int			lenstr;
	void	*ent; /* it is really (struct spwd *) */
	char	*buffer;
	int	buflen;
{
	struct spwd		*spwd	= (struct spwd *)ent;
	char			*p, *q;

	/*
	 * We know that instr != 0 because we're in 'nis', not 'files'
	 */
	if ((p = memchr(instr, ':', lenstr)) == 0) {
		return (NSS_STR_PARSE_PARSE);
	}
	if ((q = memchr(p + 1, ':', lenstr - (p + 1 - instr))) == 0) {
		return (NSS_STR_PARSE_PARSE);
	}
	/* Don't bother checking the rest of the YP passwd entry... */

	if (q + 1 - instr > buflen) {
		return (NSS_STR_PARSE_ERANGE);
	}
	memcpy(buffer, instr, q - instr);
	buffer[p - instr] = '\0';
	buffer[q - instr] = '\0';

	spwd->sp_namp	= buffer;
	spwd->sp_pwdp	= buffer + (p + 1 - instr);
	spwd->sp_lstchg	= -1;
	spwd->sp_min	= -1;
	spwd->sp_max	= -1;
	spwd->sp_warn	= -1;
	spwd->sp_inact	= -1;
	spwd->sp_expire	= -1;
	spwd->sp_flag	= 0;
	return (NSS_STR_PARSE_SUCCESS);
}

typedef int	(*cstr2ent_t)(const char *, int, void *, char *, int);

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	cstr2ent_t		save_c2e;
	nss_status_t		res;
	struct spwd 		*spwd;

	save_c2e	= argp->str2ent;
	argp->str2ent	= nis_str2spent;
	res = _nss_nis_lookup(be, argp, 0, "passwd.byname", argp->key.name, 0);
	spwd = (struct spwd *)argp->buf.result;
	/*
	 * check for the C2 security flag "##" in the passwd field.
	 * If the first 2 chars in the passwd field is "##", get
	 * the user's passwd from passwd.adjunct.byname map.
	 * The lookup to this passwd.adjunct.byname map will only
	 * succeed if the caller's uid is 0 because only root user
	 * can use privilege port.
	 */
	if ((res == NSS_SUCCESS) && (spwd->sp_pwdp) &&
	    (*(spwd->sp_pwdp) == '#') && (*(spwd->sp_pwdp + 1) == '#')) {
		/* get password from passwd.adjunct.byname */
		res = _nss_nis_lookup_rsvdport(be, argp, 0,
						"passwd.adjunct.byname",
						argp->key.name, 0);
	}

	argp->str2ent	= save_c2e;
	return (res);
}

#define	NIS_SP_GETENT

#ifdef	NIS_SP_GETENT

static nss_status_t
getent(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;
	cstr2ent_t		save_c2e;
	nss_status_t		res;
	struct spwd 		*spwd;

	save_c2e	= argp->str2ent;
	argp->str2ent	= nis_str2spent;
	res = _nss_nis_getent_rigid(be, argp);
	spwd = (struct spwd *)argp->buf.result;
	/*
	 * check for the C2 security flag "##" in the passwd field.
	 * If the first 2 chars in the passwd field is "##", get
	 * the user's passwd from passwd.adjunct.byname map.
	 * The lookup to this passwd.adjunct.byname map will only
	 * succeed if the caller's uid is 0 because only root user
	 * can use privilege port.
	 */
	if ((res == NSS_SUCCESS) && (spwd->sp_pwdp) &&
	    (*(spwd->sp_pwdp) == '#') && (*(spwd->sp_pwdp + 1) == '#')) {
		/* get password from passwd.adjunct.byname */
		res = _nss_nis_lookup_rsvdport(be, argp, 0,
					"passwd.adjunct.byname",
					spwd->sp_namp, 0);
	}
	argp->str2ent	= save_c2e;
	return (res);
}

#endif	/* NIS_SP_GETENT */

static nis_backend_op_t shadow_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
#ifdef	NIS_SP_GETENT
	getent,
#else
	0,
#endif	/* NIS_SP_GETENT */
	getbyname
};

nss_backend_t *
_nss_nis_shadow_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(shadow_ops,
				sizeof (shadow_ops) / sizeof (shadow_ops[0]),
				"passwd.byname"));
}
