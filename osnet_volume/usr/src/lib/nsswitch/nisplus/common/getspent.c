/*
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *  nisplus/getspent.c: implementations of getspnam(), getspent(), setspent(),
 *  endspent() for NIS+.  We keep the shadow information in a column
 *  ("shadow") of the same table that stores vanilla passwd information.
 */

#pragma ident	"@(#)getspent.c	1.16	00/01/07 SMI"

#include <sys/types.h>
#include <shadow.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread.h>
#include "nisplus_common.h"
#include "nisplus_tables.h"

extern int key_secretkey_is_set_g();

/*
 * bugid 4301477:
 * We lock NIS+/getspnam() so there is only one at a time,
 * So applications which link with libthread can now call
 * getspnam() (or UNIX pam_authenticate() which calls getspnam)
 * in a Secure NIS+ environment (as per CERT Advisory 96.10).
 * This is usually not a problem as login/su/dtlogin are single
 * threaded, note dtlogin is now linked with libthread (bugid 4263325)
 * which is why this bug exists (Note thr_main() check was removed)
 */
extern int _mutex_lock(mutex_t *mp);
extern int _mutex_unlock(mutex_t *mp);

static mutex_t  one_lane = DEFAULTMUTEX;


static nss_status_t
getbynam(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp	= (nss_XbyY_args_t *) a;
	struct spwd		*sp	= (struct spwd *) argp->buf.result;
	int			buflen	= argp->buf.buflen;
	nss_status_t		status;
	const char		*username;
	uid_t			orig_uid;
	uid_t			entry_uid;
	struct spwd		save_sp;
	char			*save_buf;

	/* part of fix for bugid 4301477 */
	_mutex_lock(&one_lane);

	/*
	 * There is a dirty little private protocol with the nis_object2ent()
	 * routine below:  it gives us back a uid in the argp->key.uid
	 * field.  Since "key" is a union, and we're using key.name,
	 * we save/restore it in case anyone cares.
	 */
	username = argp->key.name;

	status = _nss_nisplus_lookup(be, argp, PW_TAG_NAME, username);

	/*
	 * passwd.org_dir may have its access rights set up so that
	 * the passwd field can only be read by the user whom
	 * the entry describes.  If we get an *NP* in the password
	 * field we should try to get it again as the user.  If not,
	 * we return now.
	 */

	/* fix for bugid 4301477 DELETED if (_thr_main() != -1) goto out; */

	if (!(status == NSS_SUCCESS && argp->returnval != 0 &&
	    sp->sp_pwdp != 0 && strcmp(sp->sp_pwdp, "*NP*") == 0))
		goto out;

	/* Get our current euid and that of the entry */
	orig_uid = geteuid();
	entry_uid = argp->key.uid;

	/*
	 * If the entry uid differs from our own euid, set our euid to
	 * the entry uid and try the lookup again.
	 */

	if ((entry_uid != orig_uid) && (seteuid(entry_uid) != -1)) {
		/*
		 * Do the second lookup only if secretkey is set for
		 * this euid, otherwise it will be pointless.  Also,
		 * make sure we can allocate space to save the old
		 * results.
		 */
		if (key_secretkey_is_set_g(0, 0) &&
		    ((save_buf = (char *) malloc(buflen)) != 0)) {

			/* Save the old results in case the new lookup fails */
			memcpy(save_buf, argp->buf.buffer, buflen);
			save_sp = *sp;

			/* Do the lookup (this time as the user). */
			status = _nss_nisplus_lookup(be, argp, PW_TAG_NAME,
						    username);

			/* If it failed, restore the old results */
			if (status != NSS_SUCCESS) {
				memcpy(argp->buf.buffer, save_buf, buflen);
				*sp = save_sp;
				status = NSS_SUCCESS;
			}

			free(save_buf);
		}

		/* Set uid back */
		seteuid(orig_uid);
	}

out:
	/* end of fix for bugid 4301477 unlock NIS+/getspnam() */
	_mutex_unlock(&one_lane);

	argp->key.name = username;
	return (status);
}

/*
 * place the results from the nis_object structure into argp->buf.result
 * Returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 */
/*ARGSUSED*/
static int
nis_object2ent(nobj, obj, argp)
	int		nobj;
	nis_object	*obj;
	nss_XbyY_args_t	*argp;
{
	struct spwd	*sp	= (struct spwd *) argp->buf.result;
	char		*buffer	= argp->buf.buffer;
	int		buflen	= argp->buf.buflen;
	char		*limit	= buffer + buflen;

	struct entry_col *ecol;
	char		*val;
	int		len;

	char		*endnum;
	uid_t		uid;
	char		*p;
	long		x;

	/*
	 * If we got more than one nis_object, we just ignore it.
	 * Although it should never have happened.
	 *
	 * ASSUMPTION: All the columns in the NIS+ tables are
	 * null terminated.
	 */

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ ||
		obj->EN_data.en_cols.en_cols_len < PW_COL) {
		/* namespace/table/object is curdled */
		return (NSS_STR_PARSE_PARSE);
	}
	ecol = obj->EN_data.en_cols.en_cols_val;

	/*
	 * sp_namp: user name
	 */
	EC_SET(ecol, PW_NDX_NAME, len, val);
	if (len < 2)
		return (NSS_STR_PARSE_PARSE);
	sp->sp_namp = buffer;
	buffer += len;
	if (buffer >= limit)
		return (NSS_STR_PARSE_ERANGE);
	strcpy(sp->sp_namp, val);

	/*
	 * sp_pwdp: password
	 */
	EC_SET(ecol, PW_NDX_PASSWD, len, val);
	if (len < 2) {
		/*
		 * don't return NULL pointer, lot of stupid programs
		 * out there.
		 */
		*buffer = '\0';
		sp->sp_pwdp = buffer++;
		if (buffer >= limit)
			return (NSS_STR_PARSE_ERANGE);
	} else {
		sp->sp_pwdp = buffer;
		buffer += len;
		if (buffer >= limit)
			return (NSS_STR_PARSE_ERANGE);
		strcpy(sp->sp_pwdp, val);
	}

	/*
	 * get uid
	 */
	EC_SET(ecol, PW_NDX_UID, len, val);
	if (len < 2)
		return (NSS_STR_PARSE_PARSE);
	uid = strtol(val, &endnum, 10);
	if (*endnum != 0)
		return (NSS_STR_PARSE_PARSE);
	/*
	 * See discussion of private protocol in getbynam() above.
	 *   Note that we also end up doing this if we're called from
	 *   _nss_nisplus_getent(), but that's OK -- when we're doing
	 *   enumerations we don't care what's in the argp->key union.
	 */
	argp->key.uid = uid;

	/*
	 * Default values
	 */
	sp->sp_lstchg = -1;
	sp->sp_min = -1;
	sp->sp_max = -1;
	sp->sp_warn = -1;
	sp->sp_inact = -1;
	sp->sp_expire = -1;
	sp->sp_flag = 0;

	/*
	 * shadow information
	 *
	 * We will be lenient to no shadow field or a shadow field
	 * with less than the desired number of ":" separated longs.
	 * XXX - should we be more strict ?
	 */
	EC_SET(ecol, PW_NDX_SHADOW, len, val);

	if (len < 2)
		return (NSS_STR_PARSE_SUCCESS);

	/*
	 * Parse val for the aging fields (quickly, they might die)
	 */

	limit = val + len;
	p = val;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_lstchg = (int) x;
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_min = (int) x;
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_max = (int) x;
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_warn = (int) x;
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p) {
		sp->sp_inact = (int) x;
	}
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != ':' || endnum >= limit)
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_expire = (int) x;
	p = endnum + 1;

	x = strtol(p, &endnum, 10);
	if (*endnum != '\0' && *endnum != ':')
		return (NSS_STR_PARSE_SUCCESS);
	if (endnum != p)
		sp->sp_flag = (int) x;

	return (NSS_STR_PARSE_SUCCESS);
}

static nisplus_backend_op_t sp_ops[] = {
	_nss_nisplus_destr,
	_nss_nisplus_endent,
	_nss_nisplus_setent,
	_nss_nisplus_getent,
	getbynam
};

/*ARGSUSED*/
nss_backend_t *
_nss_nisplus_shadow_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nisplus_constr(sp_ops,
				    sizeof (sp_ops) / sizeof (sp_ops[0]),
				    PW_TBLNAME, nis_object2ent));
}
