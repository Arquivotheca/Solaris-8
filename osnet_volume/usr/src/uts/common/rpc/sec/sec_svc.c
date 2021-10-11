/*
 * Copyright (c) 1994-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sec_svc.c 1.27	99/07/18	SMI"

/*
 *  sec_svc.c, Server-side rpc security interface.
 */
#ifdef _KERNEL
#include <sys/param.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <rpc/types.h>
#include <netinet/in.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <sys/tiuser.h>
#include <sys/tihdr.h>
#include <sys/t_kuser.h>
#include <sys/cmn_err.h>
#include <rpc/auth_des.h>
#include <rpc/auth_sys.h>
#include <rpc/rpcsec_gss.h>
#include <rpc/svc_auth.h>
#include <rpc/svc.h>
#else
#include <rpc/rpc.h>
#endif


enum auth_stat _svcauth_null(struct svc_req *, struct rpc_msg *);

/*
 *  NO-OP server wrap/unwrap svc_authany_ops using no-op svc_authany_wrap().
 */
/* ARGSUSED */
static int
svc_authany_wrap(SVCAUTH *auth, XDR *xdrs, xdrproc_t xfunc, caddr_t xwhere)
{
	return (*xfunc)(xdrs, xwhere);
}

struct svc_auth_ops svc_authany_ops = {
	svc_authany_wrap,
	svc_authany_wrap
};


/*
 * The call rpc message, msg has been obtained from the wire.  The msg contains
 * the raw form of credentials and verifiers.  authenticate returns AUTH_OK
 * if the msg is successfully authenticated.  If AUTH_OK then the routine also
 * does the following things:
 * set rqst->rq_xprt->verf to the appropriate response verifier;
 * sets rqst->rq_client_cred to the "cooked" form of the credentials.
 *
 * NB: rqst->rq_cxprt->verf must be pre-alloctaed;
 * its length is set appropriately.
 *
 * The caller still owns and is responsible for msg->u.cmb.cred and
 * msg->u.cmb.verf.  The authentication system retains ownership of
 * rqst->rq_client_cred, the cooked credentials.
 *
 * There is an assumption that any flavor less than AUTH_NULL is
 * invalid.
 */
enum auth_stat
sec_svc_msg(struct svc_req *rqst, struct rpc_msg *msg, bool_t *no_dispatch)
{
	int cred_flavor;

	rqst->rq_cred = msg->rm_call.cb_cred;
	rqst->rq_xprt->xp_verf.oa_flavor = _null_auth.oa_flavor;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	/*
	 * Init the xp_auth to be no-op for all the flavors.
	 * Flavor specific routines will revise this when appropriate.
	 */
	rqst->rq_xprt->xp_auth.svc_ah_ops = svc_authany_ops;
	rqst->rq_xprt->xp_auth.svc_ah_private = NULL;
	*no_dispatch = FALSE;

	cred_flavor = rqst->rq_cred.oa_flavor;

	switch (cred_flavor) {
	case AUTH_NULL:
		rqst->rq_xprt->xp_cookie = (void *) AUTH_NULL;
		return (_svcauth_null(rqst, msg));

	case AUTH_UNIX:
		rqst->rq_xprt->xp_cookie = (void *) AUTH_UNIX;
		return (_svcauth_unix(rqst, msg));

	case AUTH_SHORT:
		rqst->rq_xprt->xp_cookie = (void *) AUTH_SHORT;
		return (_svcauth_short(rqst, msg));

	case AUTH_DES:
		rqst->rq_xprt->xp_cookie = (void *) AUTH_DES;
		return (_svcauth_des(rqst, msg));

	case RPCSEC_GSS:
		/*
		 * RPCSEC_GSS flavor routine takes an additional
		 * boolean parameter that gets set to TRUE when
		 * the call is not to be dispatched to the server.
		 */
		return (__svcrpcsec_gss(rqst, msg, no_dispatch));
	}
	return (AUTH_REJECTEDCRED);
}

/*
 *  sec_svc_getcred() gets unix cred of incoming security rpc requests.
 *  It also returns the prinicipal name and a cookie which is application
 *  dependent e.g. for nfs, it is the pseudo flavor.
 *
 *  return 0 on failure
 */
int
sec_svc_getcred(struct svc_req *req, uid_t *uid, gid_t *gid,
    short *ngroups, gid_t *groups, caddr_t *principal, int *secmod)
{
	struct authunix_parms *aup;
	struct authdes_cred *adc;
	int flavor, stat;
	rpc_gss_rawcred_t *rcred;
	rpc_gss_ucred_t	*ucred;
	void	*cookie;

	stat = 1;
	flavor = req->rq_cred.oa_flavor;

	*principal = NULL;
	switch (flavor) {
	case AUTH_UNIX:
		*secmod = AUTH_UNIX;
		aup = (struct authunix_parms *)req->rq_clntcred;
		*uid = aup->aup_uid;
		*gid = aup->aup_gid;
		*ngroups = aup->aup_len;
		bcopy(aup->aup_gids, groups, aup->aup_len * sizeof (groups[0]));
		break;

	case AUTH_NONE:
		*secmod = AUTH_NONE;
		break;

	case AUTH_DES:
		*secmod = AUTH_DES;
		adc = (struct authdes_cred *)req->rq_clntcred;
		stat = authdes_getucred(adc, uid, gid, ngroups,
					groups);
		*principal = adc->adc_fullname.name;
		break;

	case RPCSEC_GSS:
		stat = rpc_gss_getcred(req, &rcred, &ucred, &cookie);
		*secmod = (int) cookie;	/* XXX64 */
		if (ucred != NULL) {
			*uid = ucred->uid;
			*gid = ucred->gid;
			*ngroups = ucred->gidlen;
			bcopy((caddr_t)ucred->gidlist, (caddr_t)groups,
				ucred->gidlen * sizeof (groups[0]));
		} else {
			*uid = UID_NOBODY;
			*gid = GID_NOBODY;
			*ngroups = 0;
		}
		*principal = (caddr_t) rcred->client_principal;
		break;

	default:
		stat = 0;
		break;
	}

	return (stat);
}


/* ARGSUSED */
enum auth_stat
_svcauth_null(struct svc_req *rqst, struct rpc_msg *msg)
{
	return (AUTH_OK);
}


/*
 *  Load root principal names from user space to kernel space.
 *
 *  flavor - security flavor
 *  count - number of principal names to be loaded
 *  proots - address of the array of root names.
 *		input is the array address in the user space,
 *		output is the kernel address.
 *
 *  return 0 on failure.
 */
int
sec_svc_loadrootnames(int flavor, int count, caddr_t **proots, model_t model)
{
	caddr_t *roots, *oroots, root;
	char netname[MAXNETNAMELEN+1];
	struct rpc_gss_principal gsstmp, *gssname;
	u_int i, j;
	size_t len, allocsz, oallocsz;

#ifdef lint
	model = model;
#endif

	/*
	 * Get list of names from user space
	 */
	allocsz = count * sizeof (caddr_t);
	oallocsz = count * SIZEOF_PTR(model);

	/*
	 * And now copy each individual principal name
	 */
	switch (flavor) {
	case AUTH_DES:
		roots = kmem_zalloc(allocsz, KM_SLEEP);
		oroots = kmem_alloc(oallocsz, KM_SLEEP);

		if (copyin(*proots, oroots, oallocsz))
			goto done;

		for (i = 0; i < count; i++) {
			/*
			 * copyinstr copies the complete string (including the
			 * NULL) and returns the len with the NULL byte
			 * included in the calculation as long as the max
			 * length is not exceeded.
			 */
#ifdef _SYSCALL32_IMPL
			if (model != DATAMODEL_NATIVE) {
				caddr32_t *tmp;

				tmp = (caddr32_t *)oroots;
				root = (caddr_t)tmp[i];
			} else
#endif
			root = oroots[i];
			if (copyinstr(root, netname, sizeof (netname), &len)) {
				for (j = 0; j < i; j++) {
					if (roots[j] != NULL)
						kmem_free(roots[j],
						    strlen(roots[j]) + 1);
				}
				goto done;
			}
			roots[i] = kmem_alloc(len, KM_SLEEP);
			bcopy(netname, roots[i], len);
		}
		kmem_free(oroots, oallocsz);
		*proots = roots;
		return (1);

	case RPCSEC_GSS:
		roots = kmem_alloc(allocsz, KM_SLEEP);
		oroots = kmem_alloc(oallocsz, KM_SLEEP);

		if (copyin(*proots, oroots, oallocsz))
			goto done;

		for (i = 0; i < count; i++) {
#ifdef _SYSCALL32_IMPL
			if (model != DATAMODEL_NATIVE) {
				caddr32_t *tmp;

				tmp = (caddr32_t *)oroots;
				root = (caddr_t)tmp[i];
			} else
#endif
				root = oroots[i];

			if (copyin(root, &gsstmp, sizeof (gsstmp))) {
				kmem_free(oroots, oallocsz);
				goto gssfreeup;
			}
			len = sizeof (gsstmp.len) + gsstmp.len;
			gssname = kmem_alloc(len, KM_SLEEP);
			if (copyin(root, gssname, len)) {
				kmem_free(gssname, len);
				kmem_free(oroots, oallocsz);
				goto gssfreeup;
			}
			roots[i] = (caddr_t)gssname;
		}
		kmem_free(oroots, oallocsz);
		*proots = roots;
		return (1);

	default:
		return (0);
	}

gssfreeup:
	for (j = 0; j < i; j++) {
		if (roots[j] != NULL) {
			gssname = (rpc_gss_principal_t)roots[j];
			kmem_free(roots[j], gssname->len +
			    sizeof (gssname->len));
		}
	}
done:
	kmem_free(roots, allocsz);
	return (0);
}


/*
 * Figure out everything we allocated in a root principal name list in
 * order to free it up.
 */
void
sec_svc_freerootnames(int flavor, int count, caddr_t *proots)
{
	int i;
	rpc_gss_principal_t gssname;

	switch (flavor) {
	case AUTH_DES:
		for (i = 0; i < count; i++)
			if (proots[i] != NULL)
				kmem_free(proots[i], strlen(proots[i]) + 1);
		break;

	case RPCSEC_GSS:
		for (i = 0; i < count; i++) {
			if (proots[i] == NULL)
				continue;
			gssname = (rpc_gss_principal_t) proots[i];
			kmem_free(proots[i], gssname->len + sizeof (int));
		}
		break;

	}
	kmem_free(proots, count * sizeof (caddr_t));
}

/*
 * Check if the  given principal name is in the root principal list
 */
bool_t
sec_svc_inrootlist(int flavor, caddr_t rootname, int count, caddr_t *roots)
{
	int i, tmp_len;
	rpc_gss_principal_t gssp, tmp_gssp;
	size_t namelen;

	switch (flavor) {
	case AUTH_DES:
		namelen = strlen(rootname) + 1;
		for (i = 0; i < count; i++)
			if (bcmp(rootname, roots[i], namelen) == 0)
				return (TRUE);
		break;

	case RPCSEC_GSS:
		gssp = (rpc_gss_principal_t) rootname;
		namelen = gssp->len;
		for (i = 0; i < count; i++) {
			tmp_gssp = (rpc_gss_principal_t) roots[i];
			tmp_len = tmp_gssp->len;
			if ((namelen == tmp_len) &&
			    (bcmp(&gssp->name[0],
			    &tmp_gssp->name[0], namelen) == 0))
				return (TRUE);
		}
		break;
	}
	return (FALSE);
}

/*
 * Miscellaneout "control" functions manipulating global RPC security
 * attributes for server applications.
 */
bool_t
sec_svc_control(u_int cmd, void *argp)
{
	bool_t result = FALSE;		/* be paranoid */

	switch (cmd) {
	case RPC_SVC_SET_GSS_CALLBACK:
		result = rpc_gss_set_callback((rpc_gss_callback_t *) argp);
		break;
	default:
		cmn_err(CE_WARN, "sec_svc_control: bad command (%d)", cmd);
		result = FALSE;
		break;
	}

	return (result);
}
