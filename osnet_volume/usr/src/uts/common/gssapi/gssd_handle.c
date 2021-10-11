/*
 * Copyright (c) 1995,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gssd_handle.c	1.11	97/10/27 SMI"

/*
 *  Kernel code to obtain client handle to gssd server
 */

#include <sys/types.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssd_prot.h>
#include <gssapi/kgssapi_defs.h>

#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/utsname.h>

#define	GSSD_RETRY 5

static	enum clnt_stat	last_stat;
	kmutex_t	gssrpcb_lock;

void
killgssd_handle(client)
	CLIENT *client;
{
	struct rpc_err rpcerr;
	CLNT_GETERR(client, &rpcerr);
	last_stat = rpcerr.re_status;

	AUTH_DESTROY(client->cl_auth);
	CLNT_DESTROY(client);
}

CLIENT *
getgssd_handle()
{
	struct vnode			*vp;
	int error;
	static struct netbuf netaddr;
	static struct knetconfig config;
	CLIENT *clnt;
	char *gssname;
	enum clnt_stat stat;
	struct netbuf tmpaddr;

	/*
	 * Cribbed from kerb_krpc.c. Really should do the config set up
	 * in the _init routine.
	 */
	if (config.knc_rdev == 0) {
		if ((error = lookupname("/dev/ticotsord", UIO_SYSSPACE,
					    FOLLOW, NULLVPP, &vp)) != 0) {
			GSSLOG(1, "getgssd_handle: lookupname: %d\n", error);
			return (NULL);
		}
		config.knc_rdev = vp->v_rdev;
		config.knc_protofmly = loopback_name;
		VN_RELE(vp);
		config.knc_semantics = NC_TPI_COTS_ORD;
	}

	/*
	 * Contact rpcbind to get gssd's address only
	 * once and re-use the address.
	 */
	mutex_enter(&gssrpcb_lock);
	if (netaddr.len == 0 || last_stat != RPC_SUCCESS) {
		/* Set up netaddr to be <nodename>. */
		netaddr.len = strlen(utsname.nodename) + 1;
		if (netaddr.buf != (char *)NULL)
			kmem_free(netaddr.buf, netaddr.maxlen);
		gssname = kmem_zalloc(netaddr.len, KM_SLEEP);

		(void) strncpy(gssname, utsname.nodename, netaddr.len-1);

		/* Append "." to end of gssname */
		(void) strncpy(gssname+(netaddr.len-1), ".", 1);
		netaddr.buf = gssname;
		netaddr.maxlen = netaddr.len;

		/* Get address of gssd from rpcbind */
		stat = rpcbind_getaddr(&config, GSSPROG, GSSVERS, &netaddr);
		if (stat != RPC_SUCCESS) {
			kmem_free(netaddr.buf, netaddr.maxlen);
			netaddr.buf = (char *)NULL;
			netaddr.len = netaddr.maxlen = 0;
			mutex_exit(&gssrpcb_lock);
			return (NULL);
		}
	}

	/*
	 * Copy the netaddr information into a tmp location to
	 * be used by clnt_tli_kcreate.  The purpose of this
	 * is for MT race condition (ie. netaddr being modified
	 * while it is being used.)
	 */
	tmpaddr.buf = kmem_zalloc(netaddr.maxlen, KM_SLEEP);
	bcopy(netaddr.buf, tmpaddr.buf, netaddr.maxlen);
	tmpaddr.maxlen = netaddr.maxlen;
	tmpaddr.len = netaddr.len;

	mutex_exit(&gssrpcb_lock);

	error = clnt_tli_kcreate(&config, &tmpaddr, GSSPROG,
		GSSVERS, 0, GSSD_RETRY, kcred, &clnt);

	kmem_free(tmpaddr.buf, tmpaddr.maxlen);

	if (error != 0) {
		GSSLOG(1,
		"getgssd_handle: clnt_tli_kcreate: error %d\n", error);
		return (NULL);
	}

	return (clnt);
}
