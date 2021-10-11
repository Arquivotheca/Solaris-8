/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs_auth.c	1.27	98/08/10 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/utsname.h>
#include <sys/debug.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/export.h>

#include <rpcsvc/nfsauth_prot.h>

#define	EQADDR(a1, a2)  \
	(bcmp((char *)(a1)->buf, (char *)(a2)->buf, (a1)->len) == 0 && \
	(a1)->len == (a2)->len)

static struct knetconfig auth_knconf;
static kmutex_t ch_list_lock;
static struct kmem_cache *exi_cache_handle;
static void exi_cache_reclaim(void *);
static void exi_cache_trim(struct exportinfo *exi);

extern struct exportinfo *exptable[];
extern krwlock_t exported_lock;

int nfsauth_cache_hit;
int nfsauth_cache_miss;
int nfsauth_cache_reclaim;

/*
 * The following code implements a simple
 * client handle cache for the nfsauth
 * service.
 */
struct ch_entry {
	CLIENT		*ch_handle;
	struct ch_entry	*ch_next;
};

static struct ch_entry *ch_list;

int nfsauth_ch_cache;
int nfsauth_ch_cache_max = 16;
static kmutex_t nfsauth_ch_cache_lock;

void
nfsauth_init(void)
{
	vnode_t *kvp;
	int error;

	/*
	 * Setup netconfig.
	 * Assume a connectionless loopback transport.
	 */
	if ((error = lookupname("/dev/ticlts", UIO_SYSSPACE, FOLLOW,
		NULLVPP, &kvp)) != 0) {
		cmn_err(CE_CONT, "nfsauth: lookupname: %d\n", error);
		return;
	}

	auth_knconf.knc_rdev = kvp->v_rdev;
	auth_knconf.knc_protofmly = NC_LOOPBACK;
	auth_knconf.knc_semantics = NC_TPI_CLTS;
	VN_RELE(kvp);

	/*
	 * Initialize client handle cache mutex
	 */
	mutex_init(&ch_list_lock, NULL, MUTEX_DEFAULT, NULL);
	/*
	 * Initialize client handle cache count mutex
	 */
	mutex_init(&nfsauth_ch_cache_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Allocate nfsauth cache handle
	 */
	exi_cache_handle = kmem_cache_create("exi_cache_handle",
		sizeof (struct auth_cache), 0, NULL, NULL,
		exi_cache_reclaim, NULL, NULL, 0);
}

/*
 * Finalization routine for nfsauth. It is important to call this routine
 * before destroying the exported_lock.
 */
void
nfsauth_fini(void)
{
	/*
	 * Deallocate nfsauth cache handle
	 */
	kmem_cache_destroy(exi_cache_handle);

	mutex_destroy(&ch_list_lock);
	mutex_destroy(&nfsauth_ch_cache_lock);
}


/*
 * Get a client handle from the cache.
 * If none there - then create one.
 */
static int
nfsauth_clget(CLIENT **chp)
{
	int retrans = 1;
	int error;
	char addrbuf[SYS_NMLN+16];
	struct ch_entry *current;
	struct netbuf addr;

	(void) strcpy(addrbuf, utsname.nodename);
	(void) strcat(addrbuf, ".nfsauth");
	addr.buf = addrbuf;
	addr.len = (u_int) strlen(addrbuf);
	addr.maxlen = addr.len;

	mutex_enter(&ch_list_lock);
	/*
	 * If there's already a client handle
	 * on the list then use it.
	 */
	if (ch_list) {
		current = ch_list;
		ch_list = ch_list->ch_next;
		mutex_exit(&ch_list_lock);
		*chp = current->ch_handle;
		kmem_free(current, sizeof (*current));

		/*
		 * Initialize the client handle
		 */
		error = clnt_tli_kinit(*chp, &auth_knconf, &addr,
				0, retrans, CRED());
		if (error)
			cmn_err(CE_WARN, "nfsauth: clnt_tli_kinit: error %d",
				error);
		return (error);
	}

	/*
	 * Limit the number of client handles
	 * that can be created.
	 */
	if (nfsauth_ch_cache >= nfsauth_ch_cache_max) {
		mutex_exit(&ch_list_lock);
		return (EAGAIN);
	}

	nfsauth_ch_cache++;	/* protected by ch_list_lock */

	mutex_exit(&ch_list_lock);


	/*
	 * Create a new client handle.
	 */
	error = clnt_tli_kcreate(&auth_knconf, &addr,
		NFSAUTH_PROG, NFSAUTH_VERS, 0, retrans, CRED(), chp);

	if (error) {
		cmn_err(CE_WARN, "nfsauth: clnt_tli_kcreate: error %d",
			error);
		mutex_enter(&nfsauth_ch_cache_lock);
		nfsauth_ch_cache--;
		mutex_exit(&nfsauth_ch_cache_lock);
	}

	return (error);
}

/*
 * Stash a client handle in the cache
 */
static void
nfsauth_clput(CLIENT *cl)
{
	struct ch_entry *chp;

	chp = kmem_alloc(sizeof (*chp), KM_NOSLEEP);
	if (chp == NULL) {
		auth_destroy(cl->cl_auth);
		clnt_destroy(cl);

		mutex_enter(&nfsauth_ch_cache_lock);
		nfsauth_ch_cache--;
		mutex_exit(&nfsauth_ch_cache_lock);

		return;
	}

	chp->ch_handle = cl;
	mutex_enter(&ch_list_lock);
	chp->ch_next = ch_list;
	ch_list = chp;
	mutex_exit(&ch_list_lock);
}

/*
 * Convert the address in a netbuf to
 * a hash index for the auth_cache table.
 */
static int
hash(struct netbuf *a)
{
	int i, h = 0;

	for (i = 0; i < a->len; i++)
		h ^= a->buf[i];

	return (h & (AUTH_TABLESIZE - 1));
}

/*
 * Mask out the components of an
 * address that do not identify
 * a host. For socket addresses the
 * masking gets rid of the port number.
 */
static void
addrmask(struct netbuf *addr, struct netbuf *mask)
{
	int i;

	for (i = 0; i < addr->len; i++)
		addr->buf[i] &= mask->buf[i];
}

int
nfsauth_access(struct exportinfo *exi, struct svc_req *req)
{
	struct netbuf addr, *claddr;
	struct auth_cache **head, *ap;
	CLIENT *clnt;
	struct auth_req request;
	struct auth_res result;
	enum clnt_stat rpcstat;
	int access, mapaccess;
	struct timeval timout;
	struct secinfo *sp;
	int i, flavor, perm;
	int authnone_entry = -1;
	static time_t exi_msg = 0;

	/*
	 *  Get the nfs flavor number from xprt.
	 */
	flavor = (int)req->rq_xprt->xp_cookie;

	/*
	 * First check the access restrictions on the filesystem.  If
	 * there are no lists associated with this flavor then there's no
	 * need to make an expensive call to the nfsauth service or to
	 * cache anything.
	 */

	ASSERT(RW_READ_HELD(&exported_lock));

	sp = exi->exi_export.ex_secinfo;
	for (i = 0; i < exi->exi_export.ex_seccnt; i++) {
		if (flavor != sp[i].s_secinfo.sc_nfsnum) {
			if (sp[i].s_secinfo.sc_nfsnum == AUTH_NONE)
				authnone_entry = i;
			continue;
		}
		break;
	}

	mapaccess = 0;

	if (i < exi->exi_export.ex_seccnt) {
		perm = sp[i].s_flags;
	} else if (authnone_entry != -1) {
		flavor = AUTH_NONE;
		mapaccess = NFSAUTH_MAPNONE;
		perm = sp[authnone_entry].s_flags;
	} else {
		/* flavor not valid for this export */
		return (NFSAUTH_DENIED);
	}

	/*
	 * Optimize if there are no lists
	 */
	if ((perm & M_ROOT) == 0) {
		if (perm == M_RO)
			return (mapaccess | NFSAUTH_RO);
		if (perm == M_RW)
			return (mapaccess | NFSAUTH_RW);
	}

	/*
	 * M_ROOT was set, or we had an ro= and/or rw= list.
	 *
	 * Now check whether this client already
	 * has an entry for this flavor in the cache
	 * for this export.
	 * Get the caller's address, mask off the
	 * parts of the address that do not identify
	 * the host (port number, etc), and then hash
	 * it to find the chain of cache entries.
	 */

	claddr = svc_getrpccaller(req->rq_xprt);
	addr = *claddr;
	addr.buf = mem_alloc(addr.len);
	bcopy(claddr->buf, addr.buf, claddr->len);
	addrmask(&addr, svc_getaddrmask(req->rq_xprt));
	head = &exi->exi_cache[hash(&addr)];

	rw_enter(&exi->exi_cache_lock, RW_READER);
	for (ap = *head; ap; ap = ap->auth_next) {
		if (EQADDR(&addr, &ap->auth_addr) && flavor == ap->auth_flavor)
			break;
	}
	if (ap) {				/* cache hit */
		access = ap->auth_access;
		ap->auth_time = hrestime.tv_sec;
		nfsauth_cache_hit++;
	}

	rw_exit(&exi->exi_cache_lock);

	if (ap) {
		kmem_free(addr.buf, addr.len);
		return (access | mapaccess);
	}

	nfsauth_cache_miss++;

	/*
	 * No entry in the cache for this client/flavor
	 * so we need to call the nfsauth service in the
	 * mount daemon.
	 */

	if (nfsauth_clget(&clnt)) {
		kmem_free(addr.buf, addr.len);
		return (NFSAUTH_DROP);
	}

	timout.tv_sec = 10;
	timout.tv_usec = 0;

	request.req_client.n_len = addr.len;
	request.req_client.n_bytes = addr.buf;
	request.req_netid = svc_getnetid(req->rq_xprt);
	request.req_path = exi->exi_export.ex_path;
	request.req_flavor = flavor;

	rpcstat = clnt_call(clnt, NFSAUTH_ACCESS,
		(xdrproc_t)xdr_auth_req, (caddr_t)&request,
		(xdrproc_t)xdr_auth_res, (caddr_t)&result,
		timout);

	nfsauth_clput(clnt);

	switch (rpcstat) {
	case RPC_SUCCESS:
		access = result.auth_perm;
		break;
	case RPC_INTR:
		kmem_free(addr.buf, addr.len);
		return (NFSAUTH_DROP);
	case RPC_TIMEDOUT:
		/*
		 * Show messages no more than once per minute
		 */
		if ((exi_msg + 60) < hrestime.tv_sec) {
			exi_msg = hrestime.tv_sec;
			cmn_err(CE_WARN, "nfsauth: mountd not responding");
		}
		kmem_free(addr.buf, addr.len);
		return (NFSAUTH_DROP);
	default:
		/*
		 * Show messages no more than once per minute
		 */
		if ((exi_msg + 60) < hrestime.tv_sec) {
			exi_msg = hrestime.tv_sec;
			cmn_err(CE_WARN, "nfsauth: %s",
				clnt_sperrno(rpcstat));
		}
		kmem_free(addr.buf, addr.len);
		return (NFSAUTH_DROP);
	}

	/*
	 * Now cache the result on the cache chain
	 * for this export (if there's enough memory)
	 */
	ap = kmem_cache_alloc(exi_cache_handle, KM_NOSLEEP);
	if (ap) {
		ap->auth_addr = addr;
		ap->auth_flavor = flavor;
		ap->auth_access = access;
		ap->auth_time = hrestime.tv_sec;
		rw_enter(&exi->exi_cache_lock, RW_WRITER);
		ap->auth_next = *head;
		*head = ap;
		rw_exit(&exi->exi_cache_lock);
	} else {
		kmem_free(addr.buf, addr.len);
	}

	return (access | mapaccess);
}

/*
 * Free the nfsauth cache for a given export
 */
void
nfsauth_cache_free(struct exportinfo *exi)
{
	int i;
	struct auth_cache *p, *next;

	for (i = 0; i < AUTH_TABLESIZE; i++) {
		for (p = exi->exi_cache[i]; p; p = next) {
			kmem_free(p->auth_addr.buf, p->auth_addr.len);
			next = p->auth_next;
			kmem_cache_free(exi_cache_handle, (void *)p);
		}
	}
}

/*
 * Called by the kernel memory allocator when
 * memory is low. Free unused cache entries.
 * If that's not enough, the VM system will
 * call again for some more.
 */
/*ARGSUSED*/
void
exi_cache_reclaim(void *cdrarg)
{
	int i;
	struct exportinfo *exi;

	rw_enter(&exported_lock, RW_READER);

	for (i = 0; i < EXPTABLESIZE; i++) {
		for (exi = exptable[i]; exi; exi = exi->exi_hash) {
			exi_cache_trim(exi);
		}
	}
	nfsauth_cache_reclaim++;

	rw_exit(&exported_lock);
}

/*
 * Don't reclaim entries until they've been
 * in the cache for at least exi_cache_time
 * seconds.
 */
time_t exi_cache_time = 60 * 60;

void
exi_cache_trim(struct exportinfo *exi)
{
	struct auth_cache *p;
	struct auth_cache *prev, *next;
	int i;
	long stale_time = hrestime.tv_sec - exi_cache_time;

	rw_enter(&exi->exi_cache_lock, RW_WRITER);

	for (i = 0; i < AUTH_TABLESIZE; i++) {

		/*
		 * Free entries that have not been
		 * used for exi_cache_time seconds.
		 */
		prev = NULL;
		for (p = exi->exi_cache[i]; p; p = next) {
			next = p->auth_next;
			if (p->auth_time > stale_time) {
				prev = p;
				continue;
			}

			kmem_free(p->auth_addr.buf, p->auth_addr.len);
			kmem_cache_free(exi_cache_handle, (void *)p);
			if (prev == NULL)
				exi->exi_cache[i] = next;
			else
				prev->auth_next = next;
		}
	}

	rw_exit(&exi->exi_cache_lock);
}
