/*
 * Adapted for use by the boot program.
 *
 * auth_unix.c, Implements UNIX style authentication parameters.
 *
 * Copyright (C) 1984, 1996-1999, Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * The system is very weak.  The client uses no encryption for its
 * credentials and only sends null verifiers.  The server sends backs
 * null verifiers or optionally a verifier that suggests a new short hand
 * for the credentials.
 *
 */

#pragma ident	"@(#)auth_unix.c	1.18	99/02/23 SMI" /* from 1.25 90/03/30 SunOS 4.1  */

#include <sys/sysmacros.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/auth_unix.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/bootdebug.h>

static struct auth_ops *authunix_ops();
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

static void marshal_new_auth(AUTH *);

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * Static allocation
 */
AUTH auth_buf;					/* in create */
struct audata audata_buf;			/* ditto */
char auth_base[MAX_AUTH_BYTES + sizeof (uint32_t)];	/* ditto */

/*
 * Create a unix style authenticator.
 * Returns an auth handle with the given stuff in it.
 */
AUTH *
authunix_create(char *machname, uid_t uid, gid_t gid, int len, gid_t *aup_gids)
{
	struct authunix_parms aup;
	XDR xdrs;
	AUTH *auth;
	struct audata *au;
	char *buf = (char *)IALIGN(&auth_base[0]);
	extern struct opaque_auth _null_auth; /* defined in rpc_prot.c */

	/*
	 * set up auth handle to use STATIC storage.
	 */

	auth = &auth_buf;
	au = &audata_buf;

	/* setup authenticator. */
	auth->ah_ops = authunix_ops();
	auth->ah_private = (caddr_t)au;

	/* structure copies */
	auth->ah_verf = au->au_shcred = _null_auth;

	au->au_shfaults = 0;

	/*
	 * fill in param struct from the given params
	 */
	aup.aup_time = prom_gettime() / 1000;
	aup.aup_machname = machname;
	aup.aup_uid = uid;
	aup.aup_gid = gid;
	aup.aup_len = (u_int)len;
	aup.aup_gids = (gid_t *)aup_gids;

	/*
	 * Serialize the parameters into origcred
	 */
	xdrmem_create(&xdrs, buf, MAX_AUTH_BYTES, XDR_ENCODE);
	if (!xdr_authunix_parms(&xdrs, &aup)) {
		dprintf("authunix_create:  xdr_authunix_parms failed");
		return ((AUTH *)0);
	}
	au->au_origcred.oa_length = XDR_GETPOS(&xdrs);
	au->au_origcred.oa_flavor = (u_int)AUTH_UNIX;
	au->au_origcred.oa_base = buf;


	/*
	 * set auth handle to reflect new cred.
	 */
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
	return (auth);
}

/*
 * authunix operations
 */

/* ARGSUSED */
static void
authunix_nextverf(AUTH *auth)
{
}

/* ARGSUSED */
static bool_t
authunix_marshal(AUTH *auth, XDR *xdrs, cred_t *cr)
{
	struct audata *au = AUTH_PRIVATE(auth);

	return (XDR_PUTBYTES(xdrs, au->au_marshed, au->au_mpos));
}

static bool_t
authunix_validate(AUTH *auth, struct opaque_auth *verf)
{
	struct audata *au;
	XDR xdrs;

	if (verf->oa_flavor == AUTH_SHORT) {
		au = AUTH_PRIVATE(auth);


		xdrmem_create(&xdrs, verf->oa_base, verf->oa_length,
		    XDR_DECODE);

		if (xdr_opaque_auth(&xdrs, &au->au_shcred)) {
			auth->ah_cred = au->au_shcred;
		} else {
			xdrs.x_op = XDR_FREE;
			(void) xdr_opaque_auth(&xdrs, &au->au_shcred);
			au->au_shcred.oa_base = 0;
			auth->ah_cred = au->au_origcred;
		}
		marshal_new_auth(auth);
	}

	return (TRUE);
}

/*ARGSUSED*/
static bool_t
authunix_refresh(AUTH *auth, struct rpc_msg *msg, cred_t *cr)
{
	struct audata *au = AUTH_PRIVATE(auth);
	struct authunix_parms aup;
	XDR xdrs;
	int stat;

	if (auth->ah_cred.oa_base == au->au_origcred.oa_base) {
		/* there is no hope.  Punt */
		return (FALSE);
	}
	au->au_shfaults ++;

	/* first deserialize the creds back into a struct authunix_parms */
	aup.aup_machname = (char *)0;
	aup.aup_gids = (gid_t *)0;
	xdrmem_create(&xdrs, au->au_origcred.oa_base,
			au->au_origcred.oa_length, XDR_DECODE);
	stat = xdr_authunix_parms(&xdrs, &aup);
	if (!stat)
		goto done;

	/* update the time and serialize in place */
	aup.aup_time = (prom_gettime() / 1000);
	xdrs.x_op = XDR_ENCODE;
	XDR_SETPOS(&xdrs, 0);
	stat = xdr_authunix_parms(&xdrs, &aup);
	if (!stat)
		goto done;
	auth->ah_cred = au->au_origcred;
	marshal_new_auth(auth);
done:
	/* free the struct authunix_parms created by deserializing */
	xdrs.x_op = XDR_FREE;
	(void) xdr_authunix_parms(&xdrs, &aup);
	XDR_DESTROY(&xdrs);
	return (stat);
}

static void
authunix_destroy(AUTH *auth)
{
	struct audata *au = AUTH_PRIVATE(auth);

	/* simply bzero, the buffers are static. */
	bzero(au->au_origcred.oa_base, au->au_origcred.oa_length);
	bzero(auth->ah_private, sizeof (struct audata));
	bzero(auth, sizeof (*auth));
}

/*
 * Marshals (pre-serializes) an auth struct.
 * sets private data, au_marshed and au_mpos
 */
static void
marshal_new_auth(AUTH *auth)
{
	XDR xdr_stream;
	XDR *xdrs = &xdr_stream;
	struct audata *au = AUTH_PRIVATE(auth);

	xdrmem_create(xdrs, au->au_marshed, MAX_AUTH_BYTES, XDR_ENCODE);
	if ((!xdr_opaque_auth(xdrs, &(auth->ah_cred))) ||
	    (!xdr_opaque_auth(xdrs, &(auth->ah_verf)))) {
		dprintf("marshal_new_auth - Fatal marshalling problem");
	} else {
		au->au_mpos = XDR_GETPOS(xdrs);
	}
	XDR_DESTROY(xdrs);
}


static struct auth_ops *
authunix_ops(void)
{
	static struct auth_ops ops;

	if (ops.ah_nextverf == 0) {
		ops.ah_nextverf = authunix_nextverf;
		ops.ah_marshal = authunix_marshal;
		ops.ah_validate = authunix_validate;
		ops.ah_refresh = authunix_refresh;
		ops.ah_destroy = authunix_destroy;
	}
	return (&ops);
}

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authunix_parms(XDR *xdrs, struct authunix_parms *p)
{
	if (xdr_u_int(xdrs, &(p->aup_time)) &&
	    xdr_string(xdrs, &(p->aup_machname), MAX_MACHINE_NAME) &&
	    xdr_int(xdrs, (int *)&(p->aup_uid)) &&
	    xdr_int(xdrs, (int *)&(p->aup_gid)) &&
	    xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
		    &(p->aup_len), NGRPS, sizeof (int), xdr_int)) {
		return (TRUE);
	}
	return (FALSE);
}
