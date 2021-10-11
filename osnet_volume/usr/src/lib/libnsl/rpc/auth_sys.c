/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)auth_sys.c	1.25	97/04/29 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)auth_sys.c 1.34 89/02/28 Copyr 1984 Sun Micro";
#endif

/*
 * auth_sys.c, Implements UNIX (system) style authentication parameters.
 *
 * The system is very weak.  The client uses no encryption for its
 * credentials and only sends null verifiers.  The server sends backs
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 *
 */
#include "rpc_mt.h"
#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#else
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif /* _KERNEL */

#include <rpc/types.h>
#include <rpc/trace.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_sys.h>
#include <synch.h>

extern int gethostname();
extern bool_t xdr_opaque_auth();

static struct auth_ops *authsys_ops();

/*
 * This struct is pointed to by the ah_private field of an auth_handle.
 */
struct audata {
	struct opaque_auth	au_origcred;	/* original credentials */
	struct opaque_auth	au_shcred;	/* short hand cred */
	u_int			au_shfaults;	/* short hand cache faults */
	char			au_marshed[MAX_AUTH_BYTES];
	u_int			au_mpos;	/* xdr pos at end of marshed */
};
#define	AUTH_PRIVATE(auth)	((struct audata *)auth->ah_private)

static void marshal_new_auth();

static const char auth_sys_str[] = "%s : %s";
static const char authsys_create_str[] = "authsys_create";
static const char __no_mem_auth[] = "out of memory";

#ifndef KERNEL
/*
 * Create a (sys) unix style authenticator.
 * Returns an auth handle with the given stuff in it.
 */
AUTH *
authsys_create(const char *machname, uid_t uid, gid_t gid,
	register int len, const gid_t *aup_gids)
{
	struct authsys_parms aup;
	char mymem[MAX_AUTH_BYTES];
	struct timeval now;
	XDR xdrs;
	register AUTH *auth;
	register struct audata *au;

	trace2(TR_authsys_create, 0, len);
	/*
	 * Allocate and set up auth handle
	 */
	auth = (AUTH *)mem_alloc(sizeof (*auth));
	if (auth == NULL) {
		(void) syslog(LOG_ERR, auth_sys_str, authsys_create_str,
			__no_mem_auth);
		trace1(TR_authsys_create, 1);
		return (NULL);
	}
	au = (struct audata *)mem_alloc(sizeof (*au));
	if (au == NULL) {
		(void) syslog(LOG_ERR, auth_sys_str, authsys_create_str,
			__no_mem_auth);
		(void) mem_free((char *) auth, sizeof (*auth));
		trace1(TR_authsys_create, 1);
		return (NULL);
	}
	auth->ah_ops = authsys_ops();
	auth->ah_private = (caddr_t)au;
	auth->ah_verf = au->au_shcred = _null_auth;
	au->au_shfaults = 0;

	/*
	 * fill in param struct from the given params
	 */
	(void) gettimeofday(&now,  (struct timezone *)0);
	aup.aup_time = now.tv_sec;
	aup.aup_machname = (char *) machname;
	aup.aup_uid = uid;
	aup.aup_gid = gid;
	aup.aup_len = (u_int)len;
	aup.aup_gids = (gid_t *) aup_gids;

	/*
	 * Serialize the parameters into origcred
	 */
	xdrmem_create(&xdrs, mymem, MAX_AUTH_BYTES, XDR_ENCODE);
	if (! xdr_authsys_parms(&xdrs, &aup)) {
		(void) syslog(LOG_ERR, auth_sys_str, authsys_create_str,
			":  xdr_authsys_parms failed");
		trace1(TR_authsys_create, 1);
		return (NULL);
	}
	au->au_origcred.oa_length = len = XDR_GETPOS(&xdrs);
	au->au_origcred.oa_flavor = AUTH_SYS;
	if ((au->au_origcred.oa_base =
		(caddr_t) mem_alloc((u_int) len)) == NULL) {
		(void) syslog(LOG_ERR, auth_sys_str, authsys_create_str,
			__no_mem_auth);
		(void) mem_free((char *) au, sizeof (*au));
		(void) mem_free((char *) auth, sizeof (*auth));
		trace1(TR_authsys_create, 1);
		return (NULL);
	}
	(void) memcpy(au->au_origcred.oa_base, mymem, (u_int)len);

	/*
	 * set auth handle to reflect new cred.
	 */
	auth->ah_cred = au->au_origcred;
	(void) marshal_new_auth(auth);
	trace1(TR_authsys_create, 1);
	return (auth);
}

/*
 * Returns an auth handle with parameters determined by doing lots of
 * syscalls.
 */

static const char authsys_def_str[] =
	"authsys_create_default:  get%s failed:  %m";

AUTH *
authsys_create_default(void)
{
	register int len;
	char machname[MAX_MACHINE_NAME + 1];
	register uid_t uid;
	register gid_t gid;
	gid_t gids[NGRPS];
	AUTH *dummy;

	trace1(TR_authsys_create_default, 0);
	if (gethostname(machname, MAX_MACHINE_NAME) == -1) {
		(void) syslog(LOG_ERR, authsys_def_str, "hostname");
		trace1(TR_authsys_create_default, 1);
		return (NULL);
	}
	machname[MAX_MACHINE_NAME] = 0;
	uid = geteuid();
	gid = getegid();
	if ((len = getgroups(NGRPS, gids)) < 0) {
		(void) syslog(LOG_ERR, authsys_def_str, "groups");
		trace2(TR_authsys_create_default, 1, len);
		return (NULL);
	}
	dummy = authsys_create(machname, uid, gid, len, gids);
	trace2(TR_authsys_create_default, 1, len);
	return (dummy);
}
#endif /* !KERNEL */

/*
 * authsys operations
 */

/*ARGSUSED*/
static void
authsys_nextverf(AUTH *auth)
{
	trace1(TR_authsys_nextverf, 0);
	/* no action necessary */
	trace1(TR_authsys_nextverf, 1);
}

static bool_t
authsys_marshal(AUTH *auth, XDR *xdrs)
{
/* LINTED pointer alignment */
	register struct audata *au = AUTH_PRIVATE(auth);
	bool_t dummy;

	trace1(TR_authsys_marshal, 0);
	dummy  = XDR_PUTBYTES(xdrs, au->au_marshed, au->au_mpos);
	trace1(TR_authsys_marshal, 1);
	return (dummy);
}

static bool_t
authsys_validate(register AUTH *auth, struct opaque_auth *verf)
{
	register struct audata *au;
	XDR xdrs;

	trace1(TR_authsys_validate, 0);
	if (verf->oa_flavor == AUTH_SHORT) {
/* LINTED pointer alignment */
		au = AUTH_PRIVATE(auth);
		xdrmem_create(&xdrs, verf->oa_base,
			verf->oa_length, XDR_DECODE);

		if (au->au_shcred.oa_base != NULL) {
			mem_free(au->au_shcred.oa_base,
			    au->au_shcred.oa_length);
			au->au_shcred.oa_base = NULL;
		}
		if (xdr_opaque_auth(&xdrs, &au->au_shcred)) {
			auth->ah_cred = au->au_shcred;
		} else {
			xdrs.x_op = XDR_FREE;
			(void) xdr_opaque_auth(&xdrs, &au->au_shcred);
			au->au_shcred.oa_base = NULL;
			auth->ah_cred = au->au_origcred;
		}
		(void) marshal_new_auth(auth);
	}
	trace1(TR_authsys_validate, 1);
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authsys_refresh(register AUTH *auth, void *dummy)
{
/* LINTED pointer alignment */
	register struct audata *au = AUTH_PRIVATE(auth);
	struct authsys_parms aup;
#ifndef KERNEL
	struct timeval now;
#endif
	XDR xdrs;
	register int stat;

	trace1(TR_authsys_refresh, 0);
	if (auth->ah_cred.oa_base == au->au_origcred.oa_base) {
		/* there is no hope.  Punt */
		trace1(TR_authsys_refresh, 1);
		return (FALSE);
	}
	au->au_shfaults ++;

	/* first deserialize the creds back into a struct authsys_parms */
	aup.aup_machname = NULL;
	aup.aup_gids = (gid_t *)NULL;
	xdrmem_create(&xdrs, au->au_origcred.oa_base,
	    au->au_origcred.oa_length, XDR_DECODE);
	stat = xdr_authsys_parms(&xdrs, &aup);
	if (! stat)
		goto done;

	/* update the time and serialize in place */
#ifdef KERNEL
	aup.aup_time = time.tv_sec;
#else
	(void) gettimeofday(&now, (struct timezone *)0);
	aup.aup_time = now.tv_sec;
#endif
	xdrs.x_op = XDR_ENCODE;
	XDR_SETPOS(&xdrs, 0);
	stat = xdr_authsys_parms(&xdrs, &aup);
	if (! stat)
		goto done;
	auth->ah_cred = au->au_origcred;
	(void) marshal_new_auth(auth);
done:
	/* free the struct authsys_parms created by deserializing */
	xdrs.x_op = XDR_FREE;
	(void) xdr_authsys_parms(&xdrs, &aup);
	XDR_DESTROY(&xdrs);
	trace1(TR_authsys_refresh, 1);
	return (stat);
}

static void
authsys_destroy(register AUTH *auth)
{
/* LINTED pointer alignment */
	register struct audata *au = AUTH_PRIVATE(auth);

	trace1(TR_authsys_destroy, 0);
	mem_free(au->au_origcred.oa_base, au->au_origcred.oa_length);
	if (au->au_shcred.oa_base != NULL)
		mem_free(au->au_shcred.oa_base, au->au_shcred.oa_length);
	mem_free(auth->ah_private, sizeof (struct audata));
	if (auth->ah_verf.oa_base != NULL)
		mem_free(auth->ah_verf.oa_base, auth->ah_verf.oa_length);
	mem_free((caddr_t)auth, sizeof (*auth));
	trace1(TR_authsys_destroy, 1);
}

/*
 * Marshals (pre-serializes) an auth struct.
 * sets private data, au_marshed and au_mpos
 */

static const char marshal_new_auth_str[] =
		"marshal_new_auth - Fatal marshalling problem";
static void
marshal_new_auth(register AUTH *auth)
{
	XDR		xdr_stream;
	register XDR	*xdrs = &xdr_stream;
/* LINTED pointer alignment */
	register struct audata *au = AUTH_PRIVATE(auth);

	trace1(TR_marshal_new_auth, 0);
	xdrmem_create(xdrs, au->au_marshed, MAX_AUTH_BYTES, XDR_ENCODE);
	if ((! xdr_opaque_auth(xdrs, &(auth->ah_cred))) ||
	    (! xdr_opaque_auth(xdrs, &(auth->ah_verf)))) {
#ifdef KERNEL
		printf(marshal_new_auth_str);
#else
		(void) syslog(LOG_ERR, marshal_new_auth_str);

#endif
	} else {
		au->au_mpos = XDR_GETPOS(xdrs);
	}
	XDR_DESTROY(xdrs);
	trace1(TR_marshal_new_auth, 1);
}

static struct auth_ops *
authsys_ops(void)
{
	static struct auth_ops ops;
	extern mutex_t ops_lock;

	/* VARIABLES PROTECTED BY ops_lock: ops */

	trace1(TR_authsys_ops, 0);
	mutex_lock(&ops_lock);
	if (ops.ah_nextverf == NULL) {
		ops.ah_nextverf = authsys_nextverf;
		ops.ah_marshal = authsys_marshal;
		ops.ah_validate = authsys_validate;
		ops.ah_refresh = authsys_refresh;
		ops.ah_destroy = authsys_destroy;
	}
	mutex_unlock(&ops_lock);
	trace1(TR_authsys_ops, 1);
	return (&ops);
}
