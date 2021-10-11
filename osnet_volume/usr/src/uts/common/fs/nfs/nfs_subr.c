/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	Copyright (c) 1986-1989,1994-1999 by Sun Microsystems, Inc.
 *  	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident "@(#)nfs_subr.c 1.211	99/10/25 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/tiuser.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/session.h>
#include <sys/dnlc.h>
#include <sys/bitmap.h>
#include <sys/acl.h>
#include <sys/ddi.h>
#include <sys/pathname.h>
#include <sys/flock.h>
#include <sys/dirent.h>
#include <sys/flock.h>
#include <sys/callb.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/nfs_acl.h>

/*
 * Mutex to protect the following variables:
 *	rtable
 *	r_hash	(in rnode)
 *	rtablecnt
 *	rpfreelist
 *	r_freef	(in rnode)
 *	r_freeb	(in rnode)
 *	rnew
 */
kmutex_t nfs_rtable_lock;

static rnode_t **rtable;
#ifdef DEBUG
static int *rtablecnt;
#endif
static rnode_t *rpfreelist = NULL;
static int rnew = 0;
static int nrnode = 0;
static int rtablesize;
static int rtablemask;
static int hashlen = 4;
static struct kmem_cache *rnode_cache;

/*
 * Mutex to protect the following variables:
 *	nfs_major
 *	nfs_minor
 */
kmutex_t nfs_minor_lock;
int nfs_major;
int nfs_minor;

/* Do we allow preepoch (negative) time values otw? */
bool_t nfs_allow_preepoch_time = FALSE;	/* default: do not allow preepoch */

/*
 * Access cache
 */
static krwlock_t nfs_acache_lock;

static acache_t **acache;
#ifdef DEBUG
static int *acachecnt;
#endif
static int nacache;	/* used strictly to size the number of hash queues */
static int acachesize;
static int acachemask;
static struct kmem_cache *acache_cache;

#define	ACACHE_SHIFT_BITS	4

/*
 * Client side utilities
 */

/*
 * client side statistics
 */
static struct {
	kstat_named_t	calls;			/* client requests */
	kstat_named_t	badcalls;		/* rpc failures */
	kstat_named_t	clgets;			/* client handle gets */
	kstat_named_t	cltoomany;		/* Client handle cache misses */
#ifdef DEBUG
	kstat_named_t	nrnode;			/* number of allocated rnodes */
	kstat_named_t	access;			/* size of access cache */
	kstat_named_t	dirent;			/* size of readdir cache */
	kstat_named_t	symlink;		/* size of symlink cache */
	kstat_named_t	reclaim;		/* number of reclaims */
	kstat_named_t	f_reclaim;		/* number of free reclaims */
	kstat_named_t	a_reclaim;		/* number of active reclaims */
	kstat_named_t	r_reclaim;		/* number of rnode reclaims */
	kstat_named_t	noresponse;		/* server not responding cnt */
	kstat_named_t	failover;		/* server failover count */
	kstat_named_t	remap;			/* server remap count */
	kstat_named_t	rpath;			/* bytes used to store rpaths */
#endif
} clstat = {
	{ "calls",	KSTAT_DATA_UINT64 },
	{ "badcalls",	KSTAT_DATA_UINT64 },
	{ "clgets",	KSTAT_DATA_UINT64 },
	{ "cltoomany",	KSTAT_DATA_UINT64 },
#ifdef DEBUG
	{ "nrnode",	KSTAT_DATA_UINT64 },
	{ "access",	KSTAT_DATA_UINT64 },
	{ "dirent",	KSTAT_DATA_UINT64 },
	{ "symlink",	KSTAT_DATA_UINT64 },
	{ "reclaim",	KSTAT_DATA_UINT64 },
	{ "f_reclaim",	KSTAT_DATA_UINT64 },
	{ "a_reclaim",	KSTAT_DATA_UINT64 },
	{ "r_reclaim",	KSTAT_DATA_UINT64 },
	{ "noresponse",	KSTAT_DATA_UINT64 },
	{ "failover",	KSTAT_DATA_UINT64 },
	{ "remap",	KSTAT_DATA_UINT64 },
	{ "r_path",	KSTAT_DATA_UINT64 },
#endif
};

kstat_named_t *clstat_ptr = (kstat_named_t *)&clstat;
uint_t	clstat_ndata = sizeof (clstat) / sizeof (kstat_named_t);

#define	MAXCLIENTS	16

struct chtab {
	uint_t ch_timesused;
	bool_t ch_inuse;
	rpcprog_t ch_prog;
	rpcvers_t ch_vers;
	dev_t ch_dev;
	char *ch_protofmly;
	CLIENT *ch_client;
	struct chtab *ch_list;
	struct chtab *ch_next;
};

static struct chtab *chtable = NULL;

/*
 * chtable_lock protects chtable[].
 */
static kmutex_t chtable_lock;

/*
 * Some servers do not properly update the attributes of the
 * directory when changes are made.  To allow interoperability
 * with these broken servers, the nfs_disable_rddir_cache
 * parameter must be set in /etc/system
 */
int nfs_disable_rddir_cache = 0;

static int	clget(mntinfo_t *, servinfo_t *, cred_t *, CLIENT **,
		    struct chtab **, rpcprog_t);
static int	acl_clget(mntinfo_t *, servinfo_t *, cred_t *, CLIENT **,
		    struct chtab **);
static int	nfs_clget(mntinfo_t *, servinfo_t *, cred_t *, CLIENT **,
		    struct chtab **);
static void	clfree(CLIENT *, struct chtab *);
static int	nfs_feedback(int, int, mntinfo_t *);
static int	rfscall(mntinfo_t *, rpcproc_t, xdrproc_t, caddr_t, xdrproc_t,
		    caddr_t, cred_t *, int *, enum clnt_stat *, int,
		    failinfo_t *);
static int	aclcall(mntinfo_t *, rpcproc_t, xdrproc_t, caddr_t, xdrproc_t,
		    caddr_t, cred_t *, int *, int, failinfo_t *);
static void	rinactive(rnode_t *, cred_t *);
static int	rtablehash(nfs_fhandle *);
static vnode_t	*make_rnode(nfs_fhandle *, struct vfs *, struct vnodeops *,
		    int (*)(vnode_t *, page_t *, u_offset_t *, size_t *,
		    int, cred_t *), int *, cred_t *, char *, char *);
static void	rp_rmfree(rnode_t *);
static rnode_t	*rfind(nfs_fhandle *, struct vfs *);
static int	nfs_free_data_reclaim(rnode_t *, int);
static int	nfs_active_data_reclaim(rnode_t *);
static int	nfs_free_reclaim(void);
static int	nfs_active_reclaim(void);
static int	nfs_rnode_reclaim(void);
static void	nfs_reclaim(void *);
static int	failover_safe(failinfo_t *);
static void	failover_newserver(mntinfo_t *mi);
static void	failover_thread(mntinfo_t *mi);
static int	failover_wait(mntinfo_t *);
static int	failover_remap(failinfo_t *);
static int	failover_lookup(char *, vnode_t *,
		    int (*)(vnode_t *, char *, vnode_t **,
			struct pathname *, int, vnode_t *, cred_t *, int),
		    vnode_t **);
static void	nfs_free_r_path(rnode_t *);

/*
 * from rpcsec module (common/rpcsec)
 */
extern int sec_clnt_geth(CLIENT *, struct sec_data *, cred_t *, AUTH **);
extern void sec_clnt_freeh(AUTH *);
extern void sec_clnt_freeinfo(struct sec_data *);

/*
 * Common handle get program for NFS and NFS ACL client.
 */
static int
clget(mntinfo_t *mi, servinfo_t *svp, cred_t *cr, CLIENT **newcl,
    struct chtab **chp, rpcprog_t prog)
{
	struct chtab *ch;
	int retrans;
	CLIENT *client;
	int error;
	int nhl;
	struct chtab **plistp;
	int readsize;

	if (newcl == NULL || chp == NULL)
		return (EINVAL);

	*newcl = NULL;
	*chp = NULL;

	/*
	 * Set read buffer size to rsize
	 * and add room for RPC headers.
	 */
	readsize = mi->mi_tsize;
	if (readsize != 0)
		readsize += (RPC_MAXDATASIZE - NFS_MAXDATA);

	/*
	 * If soft mount and server is down just try once.
	 * meaning: do not retransmit.
	 */
	if (!(mi->mi_flags & MI_HARD) && (mi->mi_flags & MI_DOWN))
		retrans = 0;
	else
		retrans = mi->mi_retrans;

	/*
	 * Find an unused handle or create one
	 */
	clstat.clgets.value.ui64++;
	mutex_enter(&chtable_lock);
	plistp = &chtable;
	for (ch = chtable; ch != NULL; ch = ch->ch_next) {
		if (ch->ch_prog == prog &&
		    ch->ch_vers == mi->mi_vers &&
		    ch->ch_dev == svp->sv_knconf->knc_rdev &&
		    (strcmp(ch->ch_protofmly,
			svp->sv_knconf->knc_protofmly) == 0))
			break;
		plistp = &ch->ch_next;
	}
	if (ch != NULL) {
		for (nhl = 1; ch != NULL; ch = ch->ch_list, nhl++) {
			if (!ch->ch_inuse) {
				ch->ch_inuse = TRUE;
				mutex_exit(&chtable_lock);
				if (ch->ch_client == NULL) {
					error = clnt_tli_kcreate(
					    svp->sv_knconf, &svp->sv_addr,
					    prog, mi->mi_vers, readsize,
					    retrans, cr, &ch->ch_client);
					if (error != 0) {
						ch->ch_inuse = FALSE;
						nfs_cmn_err(error, CE_WARN,
				"clget: null client in chtable, ch=%p: %m\n",
						    ch);
						return (error);
					}
					(void) CLNT_CONTROL(ch->ch_client,
					    CLSET_PROGRESS, NULL);
					auth_destroy(ch->ch_client->cl_auth);
				} else {
					(void) clnt_tli_kinit(ch->ch_client,
					    svp->sv_knconf, &svp->sv_addr,
					    readsize, retrans, cr);
				}
				error = sec_clnt_geth(ch->ch_client,
				    svp->sv_secdata, cr,
				    &ch->ch_client->cl_auth);
				if (error || ch->ch_client->cl_auth == NULL) {
					CLNT_DESTROY(ch->ch_client);
					ch->ch_client = NULL;
					ch->ch_inuse = FALSE;
					return ((error != 0) ? error : EINTR);
				}
				ch->ch_timesused++;
				*newcl = ch->ch_client;
				ASSERT(ch->ch_client->cl_nosignal == FALSE);
				*chp = ch;
				return (0);
			}
			plistp = &ch->ch_list;
		}
		if (nhl == MAXCLIENTS) {
			mutex_exit(&chtable_lock);
			goto toomany;
		}
	}
	ch = kmem_alloc(sizeof (*ch), KM_SLEEP);
	ch->ch_timesused = 0;
	ch->ch_inuse = TRUE;
	ch->ch_prog = prog;
	ch->ch_vers = mi->mi_vers;
	ch->ch_dev = svp->sv_knconf->knc_rdev;
	ch->ch_protofmly = (char *)kmem_alloc(
		strlen(svp->sv_knconf->knc_protofmly)+1, KM_SLEEP);
	(void) strcpy(ch->ch_protofmly, svp->sv_knconf->knc_protofmly);
	ch->ch_client = NULL;
	ch->ch_list = NULL;
	ch->ch_next = NULL;
	*plistp = ch;
	mutex_exit(&chtable_lock);

	error = clnt_tli_kcreate(svp->sv_knconf, &svp->sv_addr, prog,
	    mi->mi_vers, readsize, retrans, cr, &ch->ch_client);
	if (error != 0) {
		ch->ch_inuse = FALSE;
		/*
		 * Warning is unnecessary if error is EINTR.
		 */
		if (error != EINTR) {
			nfs_cmn_err(error, CE_WARN,
			    "clget: couldn't create handle: %m\n");
		}
		return (error);
	}
	(void) CLNT_CONTROL(ch->ch_client, CLSET_PROGRESS, NULL);
	auth_destroy(ch->ch_client->cl_auth);
	error = sec_clnt_geth(ch->ch_client, svp->sv_secdata, cr,
	    &ch->ch_client->cl_auth);
	if (error || ch->ch_client->cl_auth == NULL) {
		CLNT_DESTROY(ch->ch_client);
		ch->ch_client = NULL;
		ch->ch_inuse = FALSE;
		return ((error != 0) ? error : EINTR);
	}
	ch->ch_timesused++;
	*newcl = ch->ch_client;
	ASSERT(ch->ch_client->cl_nosignal == FALSE);
	*chp = ch;
	return (0);

	/*
	 * If we got here there are no available handles
	 * To avoid deadlock, don't wait, but just grab another
	 */
toomany:
	clstat.cltoomany.value.ui64++;
	error = clnt_tli_kcreate(svp->sv_knconf, &svp->sv_addr, prog,
	    mi->mi_vers, readsize, retrans, cr, &client);
	if (error != 0) {
		/*
		 * Warning is unnecessary if error is EINTR.
		 */
		if (error != EINTR) {
			nfs_cmn_err(error, CE_WARN,
			    "clget: couldn't create handle: %m\n");
		}
		return (error);
	}
	(void) CLNT_CONTROL(client, CLSET_PROGRESS, NULL);
	auth_destroy(client->cl_auth);	 /* XXX */
	error = sec_clnt_geth(client, svp->sv_secdata, cr, &client->cl_auth);
	if (error || client->cl_auth == NULL) {
		CLNT_DESTROY(client);
		return ((error != 0) ? error : EINTR);
	}
	*newcl = client;
	ASSERT(client->cl_nosignal == FALSE);
	return (0);
}

static int
acl_clget(mntinfo_t *mi, servinfo_t *svp, cred_t *cr, CLIENT **newcl,
    struct chtab **chp)
{

	return (clget(mi, svp, cr, newcl, chp, NFS_ACL_PROGRAM));
}

static int
nfs_clget(mntinfo_t *mi, servinfo_t *svp, cred_t *cr, CLIENT **newcl,
    struct chtab **chp)
{

	return (clget(mi, svp, cr, newcl, chp, mi->mi_prog));
}

static void
clfree(CLIENT *cl, struct chtab *ch)
{

	if (cl->cl_auth != NULL) {
		sec_clnt_freeh(cl->cl_auth);
		cl->cl_auth = NULL;
	}

	if (ch != NULL) {
		ch->ch_inuse = FALSE;
		return;
	}

	/* destroy any extra allocated above MAXCLIENTS */
	CLNT_DESTROY(cl);
}

/*
 * Minimum time-out values indexed by call type
 * These units are in "eights" of a second to avoid multiplies
 */
static unsigned int minimum_timeo[] = {
	6, 7, 10
};

/*
 * Back off for retransmission timeout, MAXTIMO is in hz of a sec
 */
#define	MAXTIMO	(20*hz)
#define	backoff(tim)	(((tim) < MAXTIMO) ? dobackoff(tim) : (tim))
#define	dobackoff(tim)	((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

#define	MIN_NFS_TSIZE 512	/* minimum "chunk" of NFS IO */
#define	REDUCE_NFS_TIME (hz/2)	/* rtxcur we try to keep under */
#define	INCREASE_NFS_TIME (hz/3*8) /* srtt we try to keep under (scaled*8) */

/*
 * Function called when rfscall notices that we have been
 * re-transmitting, or when we get a response without retransmissions.
 * Return 1 if the transfer size was adjusted down - 0 if no change.
 */
static int
nfs_feedback(int flag, int which, mntinfo_t *mi)
{
	int kind;
	int r = 0;

	mutex_enter(&mi->mi_lock);
	if (flag == FEEDBACK_REXMIT1) {
		if (mi->mi_timers[NFS_CALLTYPES].rt_rtxcur != 0 &&
		    mi->mi_timers[NFS_CALLTYPES].rt_rtxcur < REDUCE_NFS_TIME)
			goto done;
		if (mi->mi_curread > MIN_NFS_TSIZE) {
			mi->mi_curread /= 2;
			if (mi->mi_curread < MIN_NFS_TSIZE)
				mi->mi_curread = MIN_NFS_TSIZE;
			r = 1;
		}

		if (mi->mi_curwrite > MIN_NFS_TSIZE) {
			mi->mi_curwrite /= 2;
			if (mi->mi_curwrite < MIN_NFS_TSIZE)
				mi->mi_curwrite = MIN_NFS_TSIZE;
			r = 1;
		}
	} else if (flag == FEEDBACK_OK) {
		kind = mi->mi_timer_type[which];
		if (kind == 0 ||
		    mi->mi_timers[kind].rt_srtt >= INCREASE_NFS_TIME)
			goto done;
		if (kind == 1) {
			if (mi->mi_curread >= mi->mi_tsize)
				goto done;
			mi->mi_curread +=  MIN_NFS_TSIZE;
			if (mi->mi_curread > mi->mi_tsize/2)
				mi->mi_curread = mi->mi_tsize;
		} else if (kind == 2) {
			if (mi->mi_curwrite >= mi->mi_stsize)
				goto done;
			mi->mi_curwrite += MIN_NFS_TSIZE;
			if (mi->mi_curwrite > mi->mi_stsize/2)
				mi->mi_curwrite = mi->mi_stsize;
		}
	}
done:
	mutex_exit(&mi->mi_lock);
	return (r);
}

#ifdef DEBUG
static int rfs2call_hits = 0;
static int rfs2call_misses = 0;
#endif

int
rfs2call(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    enum nfsstat *statusp, int flags, failinfo_t *fi)
{
	int rpcerror;
	enum clnt_stat rpc_status;

	ASSERT(statusp != NULL);

	rpcerror = rfscall(mi, which, xdrargs, argsp, xdrres, resp,
	    cr, douprintf, &rpc_status, flags, fi);
	if (!rpcerror) {
		/*
		 * Boy is this a kludge!  If the reply status is
		 * NFSERR_EACCES, it may be because we are root
		 * (no root net access).  Check the real uid, if
		 * it isn't root make that the uid instead and
		 * retry the call.
		 */
		if (*statusp == NFSERR_ACCES &&
		    cr->cr_uid == 0 && cr->cr_ruid != 0) {
#ifdef DEBUG
			rfs2call_hits++;
#endif
			cr = crdup(cr);
			cr->cr_uid = cr->cr_ruid;
			rpcerror = rfscall(mi, which, xdrargs, argsp, xdrres,
			    resp, cr, douprintf, NULL, flags, fi);
			crfree(cr);
#ifdef DEBUG
			if (*statusp == NFSERR_ACCES)
				rfs2call_misses++;
#endif
		}
	} else if (rpc_status == RPC_PROCUNAVAIL) {
		*statusp = NFSERR_OPNOTSUPP;
		rpcerror = 0;
	}

	return (rpcerror);
}

#define	NFS3_JUKEBOX_DELAY	10 * hz

static clock_t nfs3_jukebox_delay = 0;

#ifdef DEBUG
static int rfs3call_hits = 0;
static int rfs3call_misses = 0;
#endif

int
rfs3call(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    nfsstat3 *statusp, int flags, failinfo_t *fi)
{
	int rpcerror;
	int user_informed;

	user_informed = 0;
	do {
		rpcerror = rfscall(mi, which, xdrargs, argsp, xdrres, resp,
		    cr, douprintf, NULL, flags, fi);
		if (!rpcerror) {
			if (*statusp == NFS3ERR_JUKEBOX) {
				if (!user_informed) {
					user_informed = 1;
					uprintf(
		"file temporarily unavailable on the server, retrying...\n");
				}
				delay(nfs3_jukebox_delay);
			}
			/*
			 * Boy is this a kludge!  If the reply status is
			 * NFS3ERR_EACCES, it may be because we are root
			 * (no root net access).  Check the real uid, if
			 * it isn't root make that the uid instead and
			 * retry the call.
			 */
			else if (*statusp == NFS3ERR_ACCES &&
			    cr->cr_uid == 0 && cr->cr_ruid != 0) {
#ifdef DEBUG
				rfs3call_hits++;
#endif
				cr = crdup(cr);
				cr->cr_uid = cr->cr_ruid;
				rpcerror = rfscall(mi, which, xdrargs, argsp,
				    xdrres, resp, cr, douprintf,
				    NULL, flags, fi);
				crfree(cr);
#ifdef DEBUG
				if (*statusp == NFS3ERR_ACCES)
					rfs3call_misses++;
#endif
			}
		}
	} while (!rpcerror && *statusp == NFS3ERR_JUKEBOX);

	return (rpcerror);
}

#define	VALID_FH(fi)	(VTOR(fi->vp)->r_server == VTOMI(fi->vp)->mi_curr_serv)
#define	INC_READERS(mi)		{ \
	mi->mi_readers++; \
}
#define	DEC_READERS(mi)		{ \
	mi->mi_readers--; \
	if (mi->mi_readers == 0) \
		cv_broadcast(&mi->mi_failover_cv); \
}
#define	FAILOVER_MOUNT(mi)	(mi->mi_servers->sv_next)

static int
rfscall(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    enum clnt_stat *rpc_status, int flags, failinfo_t *fi)
{
	CLIENT *client;
	struct chtab *ch;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	int timeo;		/* in units of hz */
	int my_rsize, my_wsize;
	bool_t tryagain;
	k_sigset_t smask;
	servinfo_t *svp;
#ifdef DEBUG
	char *bufp;
#endif

	TRACE_2(TR_FAC_NFS, TR_RFSCALL_START,
		"rfscall_start:which %d server %x",
		which, &mi->mi_curr_serv->sv_addr);

	clstat.calls.value.ui64++;
	mi->mi_reqs[which].value.ui64++;

	rpcerr.re_status = RPC_SUCCESS;

	/*
	 * In case of forced unmount, return EIO.
	 */

	if (mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED) {
		rpcerr.re_status = RPC_FAILED;
		rpcerr.re_errno = EIO;
		return (rpcerr.re_errno);
	}

	/*
	 * Remember the transfer sizes in case
	 * nfs_feedback changes them underneath us.
	 */
	my_rsize = mi->mi_curread;
	my_wsize = mi->mi_curwrite;

	/*
	 * NFS client failover support
	 *
	 * If this rnode is not in sync with the current server (VALID_FH),
	 * we'd like to do a remap to get in sync.  We can be interrupted
	 * in failover_remap(), and if so we'll bail.  Otherwise, we'll
	 * use the best info we have to try the RPC.  Part of that is
	 * unconditionally updating the filehandle copy kept for V3.
	 *
	 * Locking: INC_READERS/DEC_READERS is a poor man's interrruptible
	 * rw_enter(); we're trying to keep the current server from being
	 * changed on us until we're done with the remapping and have a
	 * matching client handle.  We don't want to sending a filehandle
	 * to the wrong host.
	 */
failoverretry:
	if (FAILOVER_MOUNT(mi)) {
		mutex_enter(&mi->mi_lock);
		if (!(flags & RFSCALL_SOFT) && failover_safe(fi)) {
			if (failover_wait(mi)) {
				mutex_exit(&mi->mi_lock);
				return (EINTR);
			}
		}
		INC_READERS(mi);
		mutex_exit(&mi->mi_lock);
		if (fi) {
			if (!VALID_FH(fi) &&
			    !(flags & RFSCALL_SOFT) && failover_safe(fi)) {
				int remaperr;

				remaperr = failover_remap(fi);
				if (remaperr != 0) {
#ifdef DEBUG
					if (remaperr != EINTR)
						nfs_cmn_err(remaperr, CE_WARN,
					    "rfscall couldn't failover: %m");
#endif
					mutex_enter(&mi->mi_lock);
					DEC_READERS(mi);
					mutex_exit(&mi->mi_lock);
					return (remaperr);
				}
			}
			if (fi->fhp && fi->copyproc)
				(*fi->copyproc)(fi->fhp, fi->vp);
		}
	}

	/*
	 * clget() calls clnt_tli_kinit() which clears the xid, so we
	 * are guaranteed to reprocess the retry as a new request.
	 */
	svp = mi->mi_curr_serv;
	rpcerr.re_errno = nfs_clget(mi, svp, cr, &client, &ch);
	if (FAILOVER_MOUNT(mi)) {
		mutex_enter(&mi->mi_lock);
		DEC_READERS(mi);
		mutex_exit(&mi->mi_lock);
	}
	if (rpcerr.re_errno != 0)
		return (rpcerr.re_errno);

	if (svp->sv_knconf->knc_semantics == NC_TPI_COTS_ORD ||
	    svp->sv_knconf->knc_semantics == NC_TPI_COTS) {
		timeo = (mi->mi_timeo * hz) / 10;
	} else {
		mutex_enter(&mi->mi_lock);
		timeo = CLNT_SETTIMERS(client,
		    &(mi->mi_timers[mi->mi_timer_type[which]]),
		    &(mi->mi_timers[NFS_CALLTYPES]),
		    (minimum_timeo[mi->mi_call_type[which]]*hz)>>3,
		    (void (*)())NULL, (caddr_t)mi, 0);
		mutex_exit(&mi->mi_lock);
	}

	/*
	 * If hard mounted fs, retry call forever unless hard error occurs.
	 */
	do {
		tryagain = FALSE;

		if (mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED) {
			status = RPC_FAILED;
			rpcerr.re_status = RPC_FAILED;
			rpcerr.re_errno = EIO;
			break;
		}

		TICK_TO_TIMEVAL(timeo, &wait);

		/*
		 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
		 * and SIGTERM. (Preserving the existing masks).
		 * Mask out SIGINT if mount option nointr is specified.
		 */
		sigintr(&smask, (int)mi->mi_flags & MI_INT);
		if (!(mi->mi_flags & MI_INT))
			client->cl_nosignal = TRUE;

		status = CLNT_CALL(client, which, xdrargs, argsp,
		    xdrres, resp, wait);

		if (!(mi->mi_flags & MI_INT))
			client->cl_nosignal = FALSE;
		/*
		 * restore original signal mask
		 */
		sigunintr(&smask);

		switch (status) {
		case RPC_SUCCESS:
			(void) nfs_feedback(FEEDBACK_OK, which, mi);
			break;

		case RPC_INTR:
			/*
			 * There is no way to recover from this error,
			 * even if mount option nointr is specified.
			 * SIGKILL, for example, cannot be blocked.
			 */
			rpcerr.re_status = RPC_INTR;
			rpcerr.re_errno = EINTR;
			break;

		case RPC_UDERROR:

			/*
			 * If the NFS server is local (vold)
			 * and it goes away we get RPC_UDERROR
			 * and not RPC_INTR as we never sleep.
			 * following code will get us out of that
			 * loop if the right signal is pending.
			 * ISSIG() is used to check for pending
			 * signal.
			 */

			sigintr(&smask, (int)mi->mi_flags & MI_INT);
			if ((mi->mi_flags & MI_INT) &&
				ISSIG(curthread, JUSTLOOKING)) {

				rpcerr.re_status = RPC_INTR;
				rpcerr.re_errno = EINTR;
				/*
				 * restore original signal mask
				 */
				sigunintr(&smask);
				break;
			}

			/*
			 * else restore original signal mask
			 * and fall through
			 */
			sigunintr(&smask);

			/*FALLTHROUGH*/
		default:		/* probably RPC_TIMEDOUT */
			if (IS_UNRECOVERABLE_RPC(status))
				break;

			/*
			 * increment server not responding count
			 */
			mutex_enter(&mi->mi_lock);
			mi->mi_noresponse++;
			mutex_exit(&mi->mi_lock);
#ifdef DEBUG
			clstat.noresponse.value.ui64++;
#endif

			if (!(mi->mi_flags & MI_HARD)) {
				if (!(mi->mi_flags & MI_SEMISOFT) ||
				    (mi->mi_ss_call_type[which] == 0))
					break;
			}
			if (flags & RFSCALL_SOFT)
				break;

			/*
			 * NFS client failover support
			 *
			 * If the current server just failed us, we'll
			 * start the process of finding a new server.
			 * After that, we can just retry.
			 */
			if (FAILOVER_MOUNT(mi) && failover_safe(fi)) {
				if (svp == mi->mi_curr_serv)
					failover_newserver(mi);
				clfree(client, NULL);
				goto failoverretry;
			}

			tryagain = TRUE;

			/*
			 * The call is in progress (over COTS).
			 * Try the CLNT_CALL again, but don't
			 * print a noisy error message.
			 */
			if (status == RPC_INPROGRESS)
				break;

			timeo = backoff(timeo);
			mutex_enter(&mi->mi_lock);
			if (!(mi->mi_flags & MI_PRINTED)) {
				mi->mi_flags |= MI_PRINTED;
				mutex_exit(&mi->mi_lock);
#ifdef DEBUG
				printf(
			"NFS%d server %s not responding still trying\n",
				    mi->mi_vers, svp->sv_hostname);
#else
				printf(
			"NFS server %s not responding still trying\n",
				    svp->sv_hostname);
#endif
			} else
				mutex_exit(&mi->mi_lock);
			if (*douprintf && curproc->p_sessp->s_vp != NULL) {
				*douprintf = 0;
				if (!(mi->mi_flags & MI_NOPRINT))
#ifdef DEBUG
					uprintf(
			    "NFS%d server %s not responding still trying\n",
					    mi->mi_vers, svp->sv_hostname);
#else
					uprintf(
			    "NFS server %s not responding still trying\n",
					    svp->sv_hostname);
#endif
			}

			/*
			 * If doing dynamic adjustment of transfer
			 * size and if it's a read or write call
			 * and if the transfer size changed while
			 * retransmitting or if the feedback routine
			 * changed the transfer size,
			 * then exit rfscall so that the transfer
			 * size can be adjusted at the vnops level.
			 */
			if ((mi->mi_flags & MI_DYNAMIC) &&
			    mi->mi_timer_type[which] != 0 &&
			    (mi->mi_curread != my_rsize ||
			    mi->mi_curwrite != my_wsize ||
			    nfs_feedback(FEEDBACK_REXMIT1, which, mi))) {
				/*
				 * On read or write calls, return
				 * back to the vnode ops level if
				 * the transfer size changed.
				 */
				clfree(client, ch);
				return (ENFS_TRYAGAIN);
			}
		}
	} while (tryagain);

	if (status != RPC_SUCCESS) {
		/*
		 * Let soft mounts use the timed out message.
		 */
		if (status == RPC_INPROGRESS)
			status = RPC_TIMEDOUT;
		clstat.badcalls.value.ui64++;
		if (status != RPC_INTR) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags |= MI_DOWN;
			mutex_exit(&mi->mi_lock);
			CLNT_GETERR(client, &rpcerr);
#ifdef DEBUG
			printf("NFS%d %s failed for %s\n",
			    mi->mi_vers, mi->mi_rfsnames[which],
			    (bufp = clnt_sperror(client, svp->sv_hostname)));
			kmem_free(bufp, MAXPATHLEN);
			if (curproc->p_sessp->s_vp != NULL) {
				if (!(mi->mi_flags & MI_NOPRINT)) {
					uprintf("NFS%d %s failed for %s\n",
					mi->mi_vers, mi->mi_rfsnames[which],
					(bufp = clnt_sperror(client,
					    svp->sv_hostname)));
					kmem_free(bufp, MAXPATHLEN);
				}
			}
#else
			printf("NFS %s failed for server %s: error %d (%s)\n",
			    mi->mi_rfsnames[which], svp->sv_hostname,
			    status, clnt_sperrno(status));
			if (curproc->p_sessp->s_vp != NULL) {
				if (!(mi->mi_flags & MI_NOPRINT)) {
					uprintf(
				"NFS %s failed for server %s: error %d (%s)\n",
					    mi->mi_rfsnames[which],
					    svp->sv_hostname, status,
					    clnt_sperrno(status));
				}
			}
#endif
			/*
			 * when CLNT_CALL() fails with RPC_AUTHERROR,
			 * re_errno is set appropriately depending on
			 * the authentication error
			 */
			if (status == RPC_VERSMISMATCH ||
			    status == RPC_PROGVERSMISMATCH)
				rpcerr.re_errno = EIO;
		}
	} else {
		/*
		 * Test the value of mi_down and mi_printed without
		 * holding the mi_lock mutex.  If they are both zero,
		 * then it is okay to skip the down and printed
		 * processing.  This saves on a mutex_enter and
		 * mutex_exit pair for a normal, successful RPC.
		 * This was just complete overhead.
		 */
		if (mi->mi_flags & (MI_DOWN | MI_PRINTED)) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags &= ~MI_DOWN;
			if (mi->mi_flags & MI_PRINTED) {
				mi->mi_flags &= ~MI_PRINTED;
				mutex_exit(&mi->mi_lock);
#ifdef DEBUG
			if (!(mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED))
				printf("NFS%d server %s ok\n",
				    mi->mi_vers, svp->sv_hostname);
#else
			if (!(mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED))
				printf("NFS server %s ok\n", svp->sv_hostname);
#endif
			} else
				mutex_exit(&mi->mi_lock);
		}

		if (*douprintf == 0) {
			if (!(mi->mi_flags & MI_NOPRINT))
#ifdef DEBUG
				if (!(mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED))
					uprintf("NFS%d server %s ok\n",
					    mi->mi_vers, svp->sv_hostname);
#else
			if (!(mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED))
				uprintf("NFS server %s ok\n", svp->sv_hostname);
#endif
			*douprintf = 1;
		}
	}

	clfree(client, ch);

	ASSERT(rpcerr.re_status == RPC_SUCCESS || rpcerr.re_errno != 0);

	if (rpc_status != NULL)
		*rpc_status = rpcerr.re_status;

	TRACE_1(TR_FAC_NFS, TR_RFSCALL_END, "rfscall_end:errno %d",
	    rpcerr.re_errno);

	return (rpcerr.re_errno);
}

#ifdef DEBUG
static int acl2call_hits = 0;
static int acl2call_misses = 0;
#endif

int
acl2call(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    enum nfsstat *statusp, int flags, failinfo_t *fi)
{
	int rpcerror;

	rpcerror = aclcall(mi, which, xdrargs, argsp, xdrres, resp,
	    cr, douprintf, flags, fi);
	if (!rpcerror) {
		/*
		 * Boy is this a kludge!  If the reply status is
		 * NFSERR_EACCES, it may be because we are root
		 * (no root net access).  Check the real uid, if
		 * it isn't root make that the uid instead and
		 * retry the call.
		 */
		if (*statusp == NFSERR_ACCES &&
		    cr->cr_uid == 0 && cr->cr_ruid != 0) {
#ifdef DEBUG
			acl2call_hits++;
#endif
			cr = crdup(cr);
			cr->cr_uid = cr->cr_ruid;
			rpcerror = aclcall(mi, which, xdrargs, argsp, xdrres,
			    resp, cr, douprintf, flags, fi);
			crfree(cr);
#ifdef DEBUG
			if (*statusp == NFSERR_ACCES)
				acl2call_misses++;
#endif
		}
	}

	return (rpcerror);
}

#ifdef DEBUG
static int acl3call_hits = 0;
static int acl3call_misses = 0;
#endif

int
acl3call(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    nfsstat3 *statusp, int flags, failinfo_t *fi)
{
	int rpcerror;
	int user_informed;

	user_informed = 0;
	do {
		rpcerror = aclcall(mi, which, xdrargs, argsp, xdrres, resp,
		    cr, douprintf, flags, fi);
		if (!rpcerror) {
			if (*statusp == NFS3ERR_JUKEBOX) {
				if (!user_informed) {
					user_informed = 1;
					uprintf(
		"file temporarily unavailable on the server, retrying...\n");
				}
				delay(nfs3_jukebox_delay);
			}
			/*
			 * Boy is this a kludge!  If the reply status is
			 * NFS3ERR_EACCES, it may be because we are root
			 * (no root net access).  Check the real uid, if
			 * it isn't root make that the uid instead and
			 * retry the call.
			 */
			else if (*statusp == NFS3ERR_ACCES &&
			    cr->cr_uid == 0 && cr->cr_ruid != 0) {
#ifdef DEBUG
				acl3call_hits++;
#endif
				cr = crdup(cr);
				cr->cr_uid = cr->cr_ruid;
				rpcerror = aclcall(mi, which, xdrargs, argsp,
				    xdrres, resp, cr, douprintf, flags, fi);
				crfree(cr);
#ifdef DEBUG
				if (*statusp == NFS3ERR_ACCES)
					acl3call_misses++;
#endif
			}
		}
	} while (!rpcerror && *statusp == NFS3ERR_JUKEBOX);

	return (rpcerror);
}

static int
aclcall(mntinfo_t *mi, rpcproc_t which, xdrproc_t xdrargs, caddr_t argsp,
    xdrproc_t xdrres, caddr_t resp, cred_t *cr, int *douprintf,
    int flags, failinfo_t *fi)
{
	CLIENT *client;
	struct chtab *ch;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	int timeo;		/* in units of hz */
#if 0 /* notyet */
	int my_rsize, my_wsize;
#endif
	bool_t tryagain;
	k_sigset_t smask;
	servinfo_t *svp;
#ifdef DEBUG
	char *bufp;
#endif

#if 0 /* notyet */
	TRACE_2(TR_FAC_NFS, TR_RFSCALL_START,
		"rfscall_start:which %d server %x",
		which, &mi->mi_curr_serv->sv_addr);
#endif

	clstat.calls.value.ui64++;
	mi->mi_aclreqs[which].value.ui64++;

	rpcerr.re_status = RPC_SUCCESS;

#if 0 /* notyet */
	/*
	 * Remember the transfer sizes in case
	 * nfs_feedback changes them underneath us.
	 */
	my_rsize = mi->mi_curread;
	my_wsize = mi->mi_curwrite;
#endif

	/*
	 * NFS client failover support
	 *
	 * If this rnode is not in sync with the current server (VALID_FH),
	 * we'd like to do a remap to get in sync.  We can be interrupted
	 * in failover_remap(), and if so we'll bail.  Otherwise, we'll
	 * use the best info we have to try the RPC.  Part of that is
	 * unconditionally updating the filehandle copy kept for V3.
	 *
	 * Locking: INC_READERS/DEC_READERS is a poor man's interrruptible
	 * rw_enter(); we're trying to keep the current server from being
	 * changed on us until we're done with the remapping and have a
	 * matching client handle.  We don't want to sending a filehandle
	 * to the wrong host.
	 */
failoverretry:
	if (FAILOVER_MOUNT(mi)) {
		mutex_enter(&mi->mi_lock);
		if (!(flags & RFSCALL_SOFT) && failover_safe(fi)) {
			if (failover_wait(mi)) {
				mutex_exit(&mi->mi_lock);
				return (EINTR);
			}
		}
		INC_READERS(mi);
		mutex_exit(&mi->mi_lock);
		if (fi) {
			if (!VALID_FH(fi) &&
			    !(flags & RFSCALL_SOFT) && failover_safe(fi)) {
				int remaperr;

				remaperr = failover_remap(fi);
				if (remaperr != 0) {
#ifdef DEBUG
					if (remaperr != EINTR)
						nfs_cmn_err(remaperr, CE_WARN,
					    "aclcall couldn't failover: %m");
#endif
					mutex_enter(&mi->mi_lock);
					DEC_READERS(mi);
					mutex_exit(&mi->mi_lock);
					return (remaperr);
				}
			}
			if (fi->fhp && fi->copyproc)
				(*fi->copyproc)(fi->fhp, fi->vp);
		}
	}

	/*
	 * acl_clget() calls clnt_tli_kinit() which clears the xid, so we
	 * are guaranteed to reprocess the retry as a new request.
	 */
	svp = mi->mi_curr_serv;
	rpcerr.re_errno = acl_clget(mi, svp, cr, &client, &ch);
	if (FAILOVER_MOUNT(mi)) {
		mutex_enter(&mi->mi_lock);
		DEC_READERS(mi);
		mutex_exit(&mi->mi_lock);
	}
	if (rpcerr.re_errno != 0)
		return (rpcerr.re_errno);

	if (svp->sv_knconf->knc_semantics == NC_TPI_COTS_ORD ||
	    svp->sv_knconf->knc_semantics == NC_TPI_COTS) {
		timeo = (mi->mi_timeo * hz) / 10;
	} else {
		mutex_enter(&mi->mi_lock);
		timeo = CLNT_SETTIMERS(client,
		    &(mi->mi_timers[mi->mi_acl_timer_type[which]]),
		    &(mi->mi_timers[NFS_CALLTYPES]),
		    (minimum_timeo[mi->mi_acl_call_type[which]]*hz)>>3,
		    (void (*)()) 0, (caddr_t)mi, 0);
		mutex_exit(&mi->mi_lock);
	}

	/*
	 * If hard mounted fs, retry call forever unless hard error occurs.
	 */

	do {
		tryagain = FALSE;

		TICK_TO_TIMEVAL(timeo, &wait);

		/*
		 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
		 * and SIGTERM. (Preserving the existing masks).
		 * Mask out SIGINT if mount option nointr is specified.
		 */
		sigintr(&smask, (int)mi->mi_flags & MI_INT);
		if (!(mi->mi_flags & MI_INT))
			client->cl_nosignal = TRUE;

		status = CLNT_CALL(client, which, xdrargs, argsp,
		    xdrres, resp, wait);

		if (!(mi->mi_flags & MI_INT))
			client->cl_nosignal = FALSE;
		/*
		 * restore original signal mask
		 */
		sigunintr(&smask);

		switch (status) {
		case RPC_SUCCESS:
#if 0 /* notyet */
			(void) nfs_feedback(FEEDBACK_OK, which, mi);
#endif
			break;

		/*
		 * Unfortunately, there are servers in the world which
		 * are not coded correctly.  They are not prepared to
		 * handle RPC requests to the NFS port which are not
		 * NFS requests.  Thus, they may try to process the
		 * NFS_ACL request as if it were an NFS request.  This
		 * does not work.  Generally, an error will be generated
		 * on the client because it will not be able to decode
		 * the response from the server.  However, it seems
		 * possible that the server may not be able to decode
		 * the arguments.  Thus, the criteria for deciding
		 * whether the server supports NFS_ACL or not is whether
		 * the following RPC errors are returned from CLNT_CALL.
		 */
		case RPC_CANTDECODERES:
		case RPC_PROGUNAVAIL:
		case RPC_CANTDECODEARGS:
			mutex_enter(&mi->mi_lock);
			mi->mi_flags &= ~MI_ACL;
			mutex_exit(&mi->mi_lock);
			break;

		case RPC_INTR:
			/*
			 * There is no way to recover from this error,
			 * even if mount option nointr is specified.
			 * SIGKILL, for example, cannot be blocked.
			 */
			rpcerr.re_status = RPC_INTR;
			rpcerr.re_errno = EINTR;
			break;

		default:		/* probably RPC_TIMEDOUT */
			if (IS_UNRECOVERABLE_RPC(status))
				break;

			/*
			 * increment server not responding count
			 */
			mutex_enter(&mi->mi_lock);
			mi->mi_noresponse++;
			mutex_exit(&mi->mi_lock);
#ifdef DEBUG
			clstat.noresponse.value.ui64++;
#endif

			if (!(mi->mi_flags & MI_HARD)) {
				if (!(mi->mi_flags & MI_SEMISOFT) ||
				    (mi->mi_acl_ss_call_type[which] == 0))
					break;
			}
			if (flags & RFSCALL_SOFT)
				break;

			/*
			 * NFS client failover support
			 *
			 * If the current server just failed us, we'll
			 * start the process of finding a new server.
			 * After that, we can just retry.
			 */
			if (FAILOVER_MOUNT(mi) && failover_safe(fi)) {
				if (svp == mi->mi_curr_serv)
					failover_newserver(mi);
				clfree(client, NULL);
				goto failoverretry;
			}

			tryagain = TRUE;

			/*
			 * The call is in progress (over COTS).
			 * Try the CLNT_CALL again, but don't
			 * print a noisy error message.
			 */
			if (status == RPC_INPROGRESS)
				break;

			timeo = backoff(timeo);
			mutex_enter(&mi->mi_lock);
			if (!(mi->mi_flags & MI_PRINTED)) {
				mi->mi_flags |= MI_PRINTED;
				mutex_exit(&mi->mi_lock);
#ifdef DEBUG
				printf(
			"NFS_ACL%d server %s not responding still trying\n",
				    mi->mi_vers, svp->sv_hostname);
#else
				printf(
			    "NFS server %s not responding still trying\n",
				    svp->sv_hostname);
#endif
			} else
				mutex_exit(&mi->mi_lock);
			if (*douprintf && curproc->p_sessp->s_vp != NULL) {
				*douprintf = 0;
				if (!(mi->mi_flags & MI_NOPRINT))
#ifdef DEBUG
					uprintf(
			"NFS_ACL%d server %s not responding still trying\n",
					    mi->mi_vers, svp->sv_hostname);
#else
					uprintf(
			    "NFS server %s not responding still trying\n",
					    svp->sv_hostname);
#endif
			}

#if 0 /* notyet */
			/*
			 * If doing dynamic adjustment of transfer
			 * size and if it's a read or write call
			 * and if the transfer size changed while
			 * retransmitting or if the feedback routine
			 * changed the transfer size,
			 * then exit rfscall so that the transfer
			 * size can be adjusted at the vnops level.
			 */
			if ((mi->mi_flags & MI_DYNAMIC) &&
			    mi->mi_acl_timer_type[which] != 0 &&
			    (mi->mi_curread != my_rsize ||
			    mi->mi_curwrite != my_wsize ||
			    nfs_feedback(FEEDBACK_REXMIT1, which, mi))) {
				/*
				 * On read or write calls, return
				 * back to the vnode ops level if
				 * the transfer size changed.
				 */
				clfree(client, ch);
				return (ENFS_TRYAGAIN);
			}
#endif
		}
	} while (tryagain);

	if (status != RPC_SUCCESS) {
		/*
		 * Let soft mounts use the timed out message.
		 */
		if (status == RPC_INPROGRESS)
			status = RPC_TIMEDOUT;
		clstat.badcalls.value.ui64++;
		if (status == RPC_CANTDECODERES ||
		    status == RPC_PROGUNAVAIL ||
		    status == RPC_CANTDECODEARGS)
			CLNT_GETERR(client, &rpcerr);
		else if (status != RPC_INTR) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags |= MI_DOWN;
			mutex_exit(&mi->mi_lock);
			CLNT_GETERR(client, &rpcerr);
#ifdef DEBUG
			printf("NFS_ACL%d %s failed for %s\n",
			    mi->mi_vers, mi->mi_aclnames[which],
			    (bufp = clnt_sperror(client, svp->sv_hostname)));
			kmem_free(bufp, MAXPATHLEN);
			if (curproc->p_sessp->s_vp != NULL) {
				if (!(mi->mi_flags & MI_NOPRINT)) {
					uprintf("NFS_ACL%d %s failed for %s\n",
					mi->mi_vers, mi->mi_aclnames[which],
					(bufp = clnt_sperror(client,
					    svp->sv_hostname)));
					kmem_free(bufp, MAXPATHLEN);
				}
			}
#else
			printf("NFS %s failed for server %s: error %d (%s)\n",
			    mi->mi_aclnames[which], svp->sv_hostname,
			    status, clnt_sperrno(status));
			if (curproc->p_sessp->s_vp != NULL) {
				if (!(mi->mi_flags & MI_NOPRINT))
					uprintf(
				"NFS %s failed for server %s: error %d (%s)\n",
					    mi->mi_aclnames[which],
					    svp->sv_hostname, status,
					    clnt_sperrno(status));
			}
#endif
			/*
			 * when CLNT_CALL() fails with RPC_AUTHERROR,
			 * re_errno is set appropriately depending on
			 * the authentication error
			 */
			if (status == RPC_VERSMISMATCH ||
			    status == RPC_PROGVERSMISMATCH)
				rpcerr.re_errno = EIO;
		}
	} else {
		/*
		 * Test the value of mi_down and mi_printed without
		 * holding the mi_lock mutex.  If they are both zero,
		 * then it is okay to skip the down and printed
		 * processing.  This saves on a mutex_enter and
		 * mutex_exit pair for a normal, successful RPC.
		 * This was just complete overhead.
		 */
		if (mi->mi_flags & (MI_DOWN | MI_PRINTED)) {
			mutex_enter(&mi->mi_lock);
			mi->mi_flags &= ~MI_DOWN;
			if (mi->mi_flags & MI_PRINTED) {
				mi->mi_flags &= ~MI_PRINTED;
				mutex_exit(&mi->mi_lock);
#ifdef DEBUG
				printf("NFS_ACL%d server %s ok\n", mi->mi_vers,
				    svp->sv_hostname);
#else
				printf("NFS server %s ok\n", svp->sv_hostname);
#endif
			} else
				mutex_exit(&mi->mi_lock);
		}

		if (*douprintf == 0) {
			if (!(mi->mi_flags & MI_NOPRINT))
#ifdef DEBUG
				uprintf("NFS_ACL%d server %s ok\n",
				    mi->mi_vers, svp->sv_hostname);
#else
				uprintf("NFS server %s ok\n", svp->sv_hostname);
#endif
			*douprintf = 1;
		}
	}

	clfree(client, ch);

	ASSERT(rpcerr.re_status == RPC_SUCCESS || rpcerr.re_errno != 0);

#if 0 /* notyet */
	TRACE_1(TR_FAC_NFS, TR_RFSCALL_END, "rfscall_end:errno %d",
	    rpcerr.re_errno);
#endif

	return (rpcerr.re_errno);
}

int
vattr_to_sattr(struct vattr *vap, struct nfssattr *sa)
{
	uint_t mask = vap->va_mask;

	if (!(mask & AT_MODE))
		sa->sa_mode = (uint32_t)-1;
	else
		sa->sa_mode = vap->va_mode;
	if (!(mask & AT_UID))
		sa->sa_uid = (uint32_t)-1;
	else
		sa->sa_uid = (uint32_t)vap->va_uid;
	if (!(mask & AT_GID))
		sa->sa_gid = (uint32_t)-1;
	else
		sa->sa_gid = (uint32_t)vap->va_gid;
	if (!(mask & AT_SIZE))
		sa->sa_size = (uint32_t)-1;
	else
		sa->sa_size = (uint32_t)vap->va_size;
	if (!(mask & AT_ATIME))
		sa->sa_atime.tv_sec = sa->sa_atime.tv_usec = (int32_t)-1;
	else {
		/* check time validity */
		if (! NFS_TIME_T_OK(vap->va_atime.tv_sec)) {
			return (EOVERFLOW);
		}
		sa->sa_atime.tv_sec = vap->va_atime.tv_sec;
		sa->sa_atime.tv_usec = vap->va_atime.tv_nsec / 1000;
	}
	if (!(mask & AT_MTIME))
		sa->sa_mtime.tv_sec = sa->sa_mtime.tv_usec = (int32_t)-1;
	else {
		/* check time validity */
		if (! NFS_TIME_T_OK(vap->va_mtime.tv_sec)) {
			return (EOVERFLOW);
		}
		sa->sa_mtime.tv_sec = vap->va_mtime.tv_sec;
		sa->sa_mtime.tv_usec = vap->va_mtime.tv_nsec / 1000;
	}
	return (0);
}

int
vattr_to_sattr3(struct vattr *vap, sattr3 *sa)
{
	uint_t mask = vap->va_mask;

	if (!(mask & AT_MODE))
		sa->mode.set_it = FALSE;
	else {
		sa->mode.set_it = TRUE;
		sa->mode.mode = (mode3)vap->va_mode;
	}
	if (!(mask & AT_UID))
		sa->uid.set_it = FALSE;
	else {
		sa->uid.set_it = TRUE;
		sa->uid.uid = (uid3)vap->va_uid;
	}
	if (!(mask & AT_GID))
		sa->gid.set_it = FALSE;
	else {
		sa->gid.set_it = TRUE;
		sa->gid.gid = (gid3)vap->va_gid;
	}
	if (!(mask & AT_SIZE))
		sa->size.set_it = FALSE;
	else {
		sa->size.set_it = TRUE;
		sa->size.size = (size3)vap->va_size;
	}
	if (!(mask & AT_ATIME))
		sa->atime.set_it = DONT_CHANGE;
	else {
		/* check time validity */
		if (! NFS_TIME_T_OK(vap->va_atime.tv_sec)) {
			return (EOVERFLOW);
		}
		sa->atime.set_it = SET_TO_CLIENT_TIME;
		sa->atime.atime.seconds = (uint32)vap->va_atime.tv_sec;
		sa->atime.atime.nseconds = (uint32)vap->va_atime.tv_nsec;
	}
	if (!(mask & AT_MTIME))
		sa->mtime.set_it = DONT_CHANGE;
	else {
		/* check time validity */
		if (! NFS_TIME_T_OK(vap->va_mtime.tv_sec)) {
			return (EOVERFLOW);
		}
		sa->mtime.set_it = SET_TO_CLIENT_TIME;
		sa->mtime.mtime.seconds = (uint32)vap->va_mtime.tv_sec;
		sa->mtime.mtime.nseconds = (uint32)vap->va_mtime.tv_nsec;
	}
	return (0);
}

void
setdiropargs(struct nfsdiropargs *da, char *nm, vnode_t *dvp)
{

	da->da_fhandle = VTOFH(dvp);
	da->da_name = nm;
	da->da_flags = 0;
}

void
setdiropargs3(diropargs3 *da, char *nm, vnode_t *dvp)
{

	da->dirp = VTOFH3(dvp);
	da->name = nm;
}

int
setdirgid(vnode_t *dvp, gid_t *gidp, cred_t *cr)
{
	int error;
	rnode_t *rp;
	struct vattr va;

	va.va_mask = AT_MODE | AT_GID;
	error = VOP_GETATTR(dvp, &va, 0, cr);
	if (error)
		return (error);

	/*
	 * To determine the expected group-id of the created file:
	 *  1)	If the filesystem was not mounted with the Old-BSD-compatible
	 *	GRPID option, and the directory's set-gid bit is clear,
	 *	then use the process's gid.
	 *  2)	Otherwise, set the group-id to the gid of the parent directory.
	 */
	rp = VTOR(dvp);
	mutex_enter(&rp->r_statelock);
	if (!(VTOMI(dvp)->mi_flags & MI_GRPID) && !(va.va_mode & VSGID))
		*gidp = cr->cr_gid;
	else
		*gidp = va.va_gid;
	mutex_exit(&rp->r_statelock);
	return (0);
}

int
setdirmode(vnode_t *dvp, mode_t *omp, cred_t *cr)
{
	int error;
	struct vattr va;

	va.va_mask = AT_MODE;
	error = VOP_GETATTR(dvp, &va, 0, cr);
	if (error)
		return (error);

	/*
	 * Modify the expected mode (om) so that the set-gid bit matches
	 * that of the parent directory (dvp).
	 */
	if (va.va_mode & VSGID)
		*omp |= VSGID;
	else
		*omp &= ~VSGID;
	return (0);
}

/*
 * Free the resources associated with an rnode.
 *
 * There are no special mutex requirements for this routine.  The
 * nfs_rtable_lock can be held, but is not required.  The routine
 * does make the asumption that r_statelock in the rnode is not
 * held on entry to this routine.
 */
static void
rinactive(rnode_t *rp, cred_t *cr)
{
	vnode_t *vp;
	cred_t *cred;
	rddir_cache *rdc, *nrdc;
	char *contents;
	int size;
	vsecattr_t *vsp;
	int error;
	nfs3_pathconf_info *info;

	/*
	 * Before freeing anything, wait until all asynchronous
	 * activity is done on this rnode.  This will allow all
	 * asynchronous read ahead and write behind i/o's to
	 * finish.
	 */
	mutex_enter(&rp->r_statelock);
	while (rp->r_count > 0)
		cv_wait(&rp->r_cv, &rp->r_statelock);
	mutex_exit(&rp->r_statelock);

	/*
	 * Flush and invalidate all pages associated with the vnode.
	 */
	vp = RTOV(rp);
	if (vp->v_pages != NULL) {
		ASSERT(vp->v_type != VCHR);
		if ((rp->r_flags & RDIRTY) && !rp->r_error) {
			error = VOP_PUTPAGE(vp, (u_offset_t)0, 0, 0, cr);
			if (error && (error == ENOSPC || error == EDQUOT)) {
				mutex_enter(&rp->r_statelock);
				if (!rp->r_error)
					rp->r_error = error;
				mutex_exit(&rp->r_statelock);
			}
		}
		nfs_invalidate_pages(vp, (u_offset_t)0, cr);
	}

	/*
	 * Free any held credentials and caches which may be associated
	 * with this rnode.
	 */
	mutex_enter(&rp->r_statelock);
	cred = rp->r_cred;
	rp->r_cred = NULL;
	rdc = rp->r_dir;
	rp->r_dir = NULL;
	rp->r_direof = NULL;
	contents = rp->r_symlink.contents;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	vsp = rp->r_secattr;
	rp->r_secattr = NULL;
	info = rp->r_pathconf;
	rp->r_pathconf = NULL;
	mutex_exit(&rp->r_statelock);

	/*
	 * Free the held credential.
	 */
	if (cred != NULL)
		crfree(cred);

	/*
	 * Free the access cache entries.
	 */
	(void) nfs_access_purge_rp(rp);

	/*
	 * Free the readdir cache entries.
	 */
	while (rdc != NULL) {
		nrdc = rdc->next;
		mutex_enter(&rp->r_statelock);
		while (rdc->flags & RDDIR) {
			rdc->flags |= RDDIRWAIT;
			cv_wait(&rdc->cv, &rp->r_statelock);
		}
		/*
		 * It is caller's responsibilty to clear the
		 * RDDIRWAIT bits since it is incorrect to
		 * have RDDIRWAIT set while we are freeing
		 * the readdir cache entires.
		 */
		ASSERT(!(rdc->flags & (RDDIR | RDDIRWAIT)));
		mutex_exit(&rp->r_statelock);
		if (rdc->entries != NULL)
			kmem_free(rdc->entries, rdc->buflen);
		cv_destroy(&rdc->cv);
#ifdef DEBUG
		rddir_cache_free((void *)rdc, sizeof (*rdc));
#else
		kmem_free(rdc, sizeof (*rdc));
#endif
		rdc = nrdc;
	}

	/*
	 * Free the symbolic link cache.
	 */
	if (contents != NULL) {
#ifdef DEBUG
		symlink_cache_free((void *)contents, size);
#else
		kmem_free(contents, size);
#endif
	}

	/*
	 * Free any cached ACL.
	 */
	if (vsp != NULL)
		nfs_acl_free(vsp);

	/*
	 * Free any cached pathconf information.
	 */
	if (info != NULL)
		kmem_free(info, sizeof (*info));
}

/*
 * Return a vnode for the given NFS Version 2 file handle.
 * If no rnode exists for this fhandle, create one and put it
 * into the hash queues.  If the rnode for this fhandle
 * already exists, return it.
 */
vnode_t *
makenfsnode(fhandle_t *fh, struct nfsfattr *attr, struct vfs *vfsp,
    cred_t *cr, char *dnm, char *nm)
{
	int newnode;
	vnode_t *vp;
	nfs_fhandle nfh;
	int seq;
	bool_t	err = TRUE;

	nfh.fh_len = NFS_FHSIZE;
	bcopy(fh, nfh.fh_buf, NFS_FHSIZE);
	mutex_enter(&nfs_rtable_lock);
	vp = make_rnode(&nfh, vfsp, &nfs_vnodeops, nfs_putapage,
	    &newnode, cr, dnm, nm);
	/* don't add to attrcache if time overflow */
	if (attr != NULL) {
	    IF_NFS_TIME_OK(NFS2_TIME_OK(attr->na_ctime.tv_sec) &&
			NFS2_TIME_OK(attr->na_mtime.tv_sec),
	    {
		err = FALSE;
		if (!newnode) {
			timestruc_t ctime;
			timestruc_t mtime;

			mutex_exit(&nfs_rtable_lock);
			/*
			 * nfs protocol defines times as unsigned so don't
			 * extend sign,
			 * unless sysadmin set nfs_allow_preepoch_time.
			 */
			NFS_TIME_T_CONVERT(ctime.tv_sec, attr->na_ctime.tv_sec);
			ctime.tv_nsec = (uint32_t)(attr->na_ctime.tv_usec*1000);
			NFS_TIME_T_CONVERT(mtime.tv_sec, attr->na_mtime.tv_sec);
			mtime.tv_nsec = (uint32_t)(attr->na_mtime.tv_usec*1000);
			nfs_cache_check(vp, ctime, mtime,
			    (len_t)attr->na_size, &seq, cr);
			nfs_attrcache(vp, attr, seq);
		} else {
			if (attr->na_type < NFNON || attr->na_type > NFSOC)
				vp->v_type = VBAD;
			else
				vp->v_type = n2v_type(attr);
			/*
			 * A translation here seems to be necessary
			 * because this function can be called
			 * with `attr' that has come from the wire,
			 * and been operated on by vattr_to_nattr().
			 * See nfsrootvp()->VOP_GETTATTR()->nfsgetattr()
			 * ->nfs_getattr_otw()->rfscall()->vattr_to_nattr()
			 * ->makenfsnode().
			 */
			if ((attr->na_rdev & 0xffff0000) == 0)
				vp->v_rdev = nfsv2_expdev(attr->na_rdev);
			else
				vp->v_rdev = expldev(n2v_rdev(attr));
			ASSERT(VTOR(vp)->r_seq == 0);
			nfs_attrcache(vp, attr, 0);
			mutex_exit(&nfs_rtable_lock);
		}
	    })
	}
	if (err) {
		if (newnode) {
			PURGE_ATTRCACHE(vp);
		}
		mutex_exit(&nfs_rtable_lock);
	}

	return (vp);
}

/*
 * Return a vnode for the given NFS Version 3 file handle.
 * If no rnode exists for this fhandle, create one and put it
 * into the hash queues.  If the rnode for this fhandle
 * already exists, return it.
 */
vnode_t *
makenfs3node(nfs_fh3 *fh, fattr3 *attr, struct vfs *vfsp, cred_t *cr,
    char *dnm, char *nm)
{
	int newnode;
	vnode_t *vp;
	int seq;

	mutex_enter(&nfs_rtable_lock);
	vp = make_rnode((nfs_fhandle *)fh, vfsp, &nfs3_vnodeops, nfs3_putapage,
	    &newnode, cr, dnm, nm);
	/*
	 * don't add to attrcache if time overflow, but
	 * no need to check because xdr_post_op_attr checks this, and all
	 * callers to this routine either use attr=NULL or post-attrs.
	 */
	if (attr != NULL) {
		if (!newnode) {
			mutex_exit(&nfs_rtable_lock);
			nfs3_cache_check_fattr3(vp, attr, &seq, cr);
			nfs3_attrcache(vp, attr, seq);
		} else {
			if (attr->type < NF3REG || attr->type > NF3FIFO)
				vp->v_type = VBAD;
			else
				vp->v_type = nf3_to_vt[attr->type];
			vp->v_rdev = makedevice(attr->rdev.specdata1,
			    attr->rdev.specdata2);
			ASSERT(VTOR(vp)->r_seq == 0);
			nfs3_attrcache(vp, attr, 0);
			mutex_exit(&nfs_rtable_lock);
		}
	} else {
		if (newnode) {
			PURGE_ATTRCACHE(vp);
		}
		mutex_exit(&nfs_rtable_lock);
	}

	return (vp);
}

static int
rtablehash(nfs_fhandle *fh)
{
	int sum;
	char *cp, *ecp;

	cp = fh->fh_buf;
	ecp = &fh->fh_buf[fh->fh_len];
	sum = 0;
	while (cp < ecp)
		sum += *cp++;
	return (sum & rtablemask);
}

static vnode_t *
make_rnode(nfs_fhandle *fh, struct vfs *vfsp, struct vnodeops *vops,
    int (*putapage)(vnode_t *, page_t *, u_offset_t *, size_t *, int, cred_t *),
    int *newnode, cred_t *cr, char *dnm, char *nm)
{
	rnode_t *rp;
	rnode_t *trp;
	vnode_t *vp;
	mntinfo_t *mi;
	nfs_fhandle *rootfh;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));

	mi = VFTOMI(vfsp);
start:
	if ((rp = rfind(fh, vfsp)) != NULL) {
		vp = RTOV(rp);
		rootfh = &rp->r_server->sv_fhandle;
		if (rootfh->fh_len == fh->fh_len &&
		    bcmp(rootfh->fh_buf, fh->fh_buf, fh->fh_len) == 0) {
			if (!(vp->v_flag & VROOT)) {
				mutex_enter(&vp->v_lock);
				vp->v_flag |= VROOT;
				mutex_exit(&vp->v_lock);
			}
		}
		*newnode = 0;
		return (vp);
	}
	if (rpfreelist != NULL && rnew >= nrnode) {
		rp = rpfreelist;
		rp_rmfree(rp);
		if (rp->r_flags & RHASHED)
			rp_rmhash(rp);
		vp = RTOV(rp);
		VN_HOLD(vp);
		mutex_exit(&nfs_rtable_lock);
		rinactive(rp, cr);
		mutex_enter(&nfs_rtable_lock);

		mutex_enter(&vp->v_lock);
		if (vp->v_count > 1) {
			vp->v_count--;
			mutex_exit(&vp->v_lock);
			goto start;
		}
		mutex_exit(&vp->v_lock);

		/*
		 * There is a race condition if someone else
		 * alloc's the rnode while we're asleep, so we
		 * check again and recover if found.
		 */
		if ((trp = rfind(fh, vfsp)) != NULL) {
			vp = RTOV(trp);
			rootfh = &trp->r_server->sv_fhandle;
			if (rootfh->fh_len == fh->fh_len &&
			    bcmp(rootfh->fh_buf, fh->fh_buf, fh->fh_len) == 0) {
				if (!(vp->v_flag & VROOT)) {
					mutex_enter(&vp->v_lock);
					vp->v_flag |= VROOT;
					mutex_exit(&vp->v_lock);
				}
			}
			*newnode = 0;
			rp_addfree(rp, cr);
			return (vp);
		}

		/*
		 * destroy old locks before bzero'ing and
		 * recreating the locks below.
		 */
		nfs_rw_destroy(&rp->r_rwlock);
		mutex_destroy(&rp->r_statelock);
		cv_destroy(&rp->r_cv);
		cv_destroy(&rp->r_commit.c_cv);
		nfs_free_r_path(rp);
		mutex_destroy(&vp->v_lock);
		cv_destroy(&vp->v_cv);
		/*
		 * Make sure that if rnode is recycled then
		 * VFS count is decremented properly before
		 * reuse.
		 */
		VFS_RELE(vp->v_vfsp);
	} else {
		rp = kmem_cache_alloc(rnode_cache, KM_NOSLEEP);
		if (rp == NULL) {
			mutex_exit(&nfs_rtable_lock);
			rp = kmem_cache_alloc(rnode_cache, KM_SLEEP);
			mutex_enter(&nfs_rtable_lock);
			/*
			 * There is a race condition if someone else
			 * alloc's the rnode while we're asleep, so we
			 * check again and recover if found.
			 */
			if ((trp = rfind(fh, vfsp)) != NULL) {
				vp = RTOV(trp);
				rootfh = &trp->r_server->sv_fhandle;
				if (rootfh->fh_len == fh->fh_len &&
				    bcmp(rootfh->fh_buf, fh->fh_buf,
					    fh->fh_len) == 0) {
					if (!(vp->v_flag & VROOT)) {
						mutex_enter(&vp->v_lock);
						vp->v_flag |= VROOT;
						mutex_exit(&vp->v_lock);
					}
				}
				*newnode = 0;
				kmem_cache_free(rnode_cache, (void *)rp);
				return (vp);
			}
		}
		rnew++;
#ifdef DEBUG
		clstat.nrnode.value.ui64++;
#endif
		vp = RTOV(rp);
	}
	bzero(rp, sizeof (*rp));
	nfs_rw_init(&rp->r_rwlock, NULL, RW_DEFAULT, NULL);
	nfs_rw_init(&rp->r_lkserlock, NULL, RW_DEFAULT, NULL);
	mutex_init(&rp->r_statelock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&rp->r_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&rp->r_commit.c_cv, NULL, CV_DEFAULT, NULL);
	rp->r_fh.fh_len = fh->fh_len;
	bcopy(fh->fh_buf, rp->r_fh.fh_buf, fh->fh_len);
	rp->r_server = mi->mi_curr_serv;

	if (FAILOVER_MOUNT(mi)) {
		/*
		 * If replicated servers, stash pathnames
		 */
		if (dnm != NULL && nm != NULL) {
			char *s, *p;
			uint_t len;

			len = (uint_t)(strlen(dnm) + strlen(nm) + 2);
			rp->r_path = kmem_alloc(len, KM_SLEEP);
#ifdef DEBUG
			clstat.rpath.value.ui64 += len;
#endif
			s = rp->r_path;
			for (p = dnm; *p; p++)
				*s++ = *p;
			*s++ = '/';
			for (p = nm; *p; p++)
				*s++ = *p;
			*s = '\0';
		} else {
			/* special case for root */
			rp->r_path = kmem_alloc(2, KM_SLEEP);
#ifdef DEBUG
			clstat.rpath.value.ui64 += 2;
#endif
			*rp->r_path = '.';
			*(rp->r_path + 1) = '\0';
		}
	}
	VFS_HOLD(vfsp);
	rp->r_putapage = putapage;
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	vp->v_count = 1;
	vp->v_op = vops;
	vp->v_data = (caddr_t)rp;
	vp->v_vfsp = vfsp;
	vp->v_type = VNON;
	rootfh = &rp->r_server->sv_fhandle;
	if (rootfh->fh_len == fh->fh_len &&
	    bcmp(rootfh->fh_buf, fh->fh_buf, fh->fh_len) == 0)
		vp->v_flag = VROOT;
	rp_addhash(rp);
	*newnode = 1;
	return (vp);
}

void
nfs_free_r_path(rnode_t *rp)
{
	char *path;
	size_t len;

	path = rp->r_path;
	if (path) {
		rp->r_path = NULL;
		len = strlen(path) + 1;
		kmem_free(path, len);
#ifdef DEBUG
		clstat.rpath.value.ui64 -= len;
#endif
	}
}

/*
 * Put an rnode on the free list.
 *
 * The caller must be holding nfs_rtable_lock.
 *
 * Rnodes which were allocated above and beyond the normal limit
 * are immediately freed.
 */
void
rp_addfree(rnode_t *rp, cred_t *cr)
{
	vnode_t *vp;
	struct vfs *vfsp;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));

	vp = RTOV(rp);
	ASSERT(vp->v_count >= 1);
	vfsp = vp->v_vfsp;

	/*
	 * If someone else has grabbed a reference to this vnode
	 * or if this rnode is already on the freelist, then just
	 * release our reference to the vnode.  The rnode can be
	 * on the freelist already because it is possible for an
	 * rnode to be free, but have modified pages associated
	 * with it.  Thus, the vnode for the page could be held
	 * and then released causing a call to this routine via
	 * the file system inactive routine.
	 */
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1 || rp->r_freef != NULL) {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		return;
	}

	/*
	 * If we have too many rnodes allocated and there are no
	 * references to this rnode, or if the rnode is no longer
	 * accessible by it does not reside in the hash queues,
	 * or if an i/o error occurred while writing to the file,
	 * then just free it instead of putting it on the rnode
	 * freelist.  nfs_rtable_lock should not be held while
	 * freeing the resources associated with this rnode because
	 * some of the freeing operations may take a long time.
	 * Thus, nfs_rtable_lock is dropped while freeing the resources
	 * and then reacquired.
	 */
	if (((rnew > nrnode || !(rp->r_flags & RHASHED) || rp->r_error ||
		(vfsp->vfs_flag & VFS_UNMOUNTED)) && rp->r_count == 0)) {
		mutex_exit(&vp->v_lock);
		if (rp->r_flags & RHASHED)
			rp_rmhash(rp);
		mutex_exit(&nfs_rtable_lock);
		rinactive(rp, cr);
		mutex_enter(&nfs_rtable_lock);
		/*
		 * Recheck the vnode reference count.  We need to
		 * make sure that another reference has not been
		 * acquired while we were not holding v_lock.  The
		 * rnode is not in the rnode hash queues, so the
		 * only way for a reference to have been acquired
		 * is for a VOP_PUTPAGE because the rnode was marked
		 * with RDIRTY or for a modified page.  This
		 * reference may have been acquired before our call
		 * to rinactive.  The i/o may have been completed,
		 * thus allowing rinactive to complete, but the
		 * reference to the vnode may not have been released
		 * yet.  In any case, the rnode can not be destroyed
		 * until the other references to this vnode have been
		 * released.  The other references will take care of
		 * either destroying the rnode or placing it on the
		 * rnode freelist.  If there are no other references,
		 * then the rnode may be safely destroyed.
		 */
		mutex_enter(&vp->v_lock);
		if (vp->v_count-- > 1) {
			mutex_exit(&vp->v_lock);
			return;
		}
		ASSERT(vp->v_count == 0);
		ASSERT(rp->r_count == 0);
		ASSERT(rp->r_lmpl == NULL);
		ASSERT(rp->r_mapcnt == 0);
		mutex_exit(&vp->v_lock);
		VFS_RELE(vfsp);
		rnew--;
#ifdef DEBUG
		clstat.nrnode.value.ui64--;
#endif
		nfs_rw_destroy(&rp->r_rwlock);
		nfs_rw_destroy(&rp->r_lkserlock);
		mutex_destroy(&rp->r_statelock);
		cv_destroy(&rp->r_cv);
		cv_destroy(&rp->r_commit.c_cv);
		nfs_free_r_path(rp);
		mutex_destroy(&vp->v_lock);
		cv_destroy(&vp->v_cv);
		kmem_cache_free(rnode_cache, rp);
		return;
	}

	/*
	 * The vnode is not currently referenced by anyone else,
	 * so release this reference and place the rnode on the
	 * freelist.  If there is no cached data or metadata for
	 * this file, then put the rnode on the front of the
	 * freelist so that it will be reused before other rnodes
	 * which may have cached data or metadata associated with
	 * them.
	 */
	ASSERT(rp->r_lmpl == NULL);
	ASSERT(rp->r_mapcnt == 0);
	vp->v_count--;
	mutex_exit(&vp->v_lock);
	if (rpfreelist == NULL) {
		rp->r_freef = rp;
		rp->r_freeb = rp;
		rpfreelist = rp;
	} else {
		rp->r_freef = rpfreelist;
		rp->r_freeb = rpfreelist->r_freeb;
		rpfreelist->r_freeb->r_freef = rp;
		rpfreelist->r_freeb = rp;
		if (vp->v_pages == NULL &&
		    rp->r_dir == NULL &&
		    rp->r_symlink.contents == NULL &&
		    rp->r_secattr == NULL &&
		    rp->r_pathconf == NULL)
			rpfreelist = rp;
	}
}

/*
 * Remove an rnode from the free list.
 *
 * The caller must be holding the nfs_rtable_lock and the rnode
 * must be on the freelist.
 */
static void
rp_rmfree(rnode_t *rp)
{

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));
	ASSERT(rp->r_freef != NULL && rp->r_freeb != NULL);

	if (rp == rpfreelist) {
		rpfreelist = rp->r_freef;
		if (rp == rpfreelist)
			rpfreelist = NULL;
	}
	rp->r_freeb->r_freef = rp->r_freef;
	rp->r_freef->r_freeb = rp->r_freeb;
	rp->r_freef = rp->r_freeb = NULL;
}

/*
 * Put a rnode in the hash table.
 *
 * The caller must be holding the nfs_rtable_lock.
 */
void
rp_addhash(rnode_t *rp)
{
	int hash;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));
	ASSERT(!(rp->r_flags & RHASHED));

	hash = rtablehash(&rp->r_fh);
	rp->r_hash = rtable[hash];
	rtable[hash] = rp;
#ifdef DEBUG
	rtablecnt[hash]++;
#endif
	mutex_enter(&rp->r_statelock);
	rp->r_flags |= RHASHED;
	mutex_exit(&rp->r_statelock);
}

/*
 * Remove a rnode from the hash table.
 *
 * The caller must be holding the nfs_rtable_lock.
 */
void
rp_rmhash(rnode_t *rp)
{
	int hash;
	rnode_t *rt;
	rnode_t *rtprev = NULL;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));
	ASSERT(rp->r_flags & RHASHED);

	hash = rtablehash(&rp->r_fh);
	rt = rtable[hash];
	while (rt != NULL) {
		if (rt == rp) {
			if (rtprev == NULL)
				rtable[hash] = rt->r_hash;
			else
				rtprev->r_hash = rt->r_hash;
#ifdef DEBUG
			rtablecnt[hash]--;
#endif
			mutex_enter(&rp->r_statelock);
			rp->r_flags &= ~RHASHED;
			mutex_exit(&rp->r_statelock);
			return;
		}
		rtprev = rt;
		rt = rt->r_hash;
	}
	cmn_err(CE_PANIC, "rp_rmhash: rnode not in hash queue");
}

/*
 * Lookup a rnode by fhandle.
 *
 * The caller must be holding the nfs_rtable_lock.
 */
static rnode_t *
rfind(nfs_fhandle *fh, struct vfs *vfsp)
{
	rnode_t *rp;
	vnode_t *vp;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));

	rp = rtable[rtablehash(fh)];
	while (rp != NULL) {
		vp = RTOV(rp);
		if (vp->v_vfsp == vfsp &&
		    rp->r_fh.fh_len == fh->fh_len &&
		    bcmp(rp->r_fh.fh_buf, fh->fh_buf, fh->fh_len) == 0) {
			VN_HOLD(vp);
			/*
			 * remove rnode from free list, if necessary.
			 */
			if (rp->r_freef != NULL)
				rp_rmfree(rp);
			return (rp);
		}
		rp = rp->r_hash;
	}
	return (NULL);
}

/*
 * Return 1 if there is a active vnode belonging to this vfs in the
 * rtable cache.
 *
 * The caller must be holding the nfs_rtable_lock.
 */
int
check_rtable(struct vfs *vfsp)
{
	rnode_t **rpp, **erpp, *rp;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));

	erpp = &rtable[rtablesize];
	for (rpp = rtable; rpp < erpp; rpp++) {
		for (rp = *rpp; rp != NULL; rp = rp->r_hash) {
			if (RTOV(rp)->v_vfsp == vfsp) {
				if (rp->r_freef == NULL ||
				    (rp->r_flags & RDIRTY) ||
				    rp->r_count > 0)
					return (1);
			}
		}
	}
	return (0);
}

/*
 * Destroy inactive vnodes from the hash queues which
 * belong to this vfs. All of the vnodes should be inactive.
 * It is essential that we destory all rnodes in case of
 * forced unmount as well as in normal unmount case.
 */

void
destroy_rtable(struct vfs *vfsp, cred_t *cr)
{
	rnode_t **rpp, **erpp, *rp, *rpprev, *r_hash, *rlist;

	rlist = NULL;

	ASSERT(MUTEX_HELD(&nfs_rtable_lock));
	erpp = &rtable[rtablesize];
	for (rpp = rtable; rpp < erpp; rpp++) {
		rpprev = NULL;
		for (rp = *rpp; rp != NULL; rp = r_hash) {
			/* save the hash pointer before assigning */
			r_hash = rp->r_hash;
			if ((RTOV(rp)->v_vfsp == vfsp) &&
				(RTOV(rp)->v_count == 0)) {
				if (rpprev == NULL)
					*rpp = rp->r_hash;
				else
					rpprev->r_hash = rp->r_hash;
#ifdef DEBUG
				rtablecnt[rpp - rtable]--;
#endif
				rp->r_hash = rlist;
				rlist = rp;
				/*
				 * remove rnode from free list, if necessary.
				 */
				if (rp->r_freef != NULL) {
					rp_rmfree(rp);
				}
				/*
				 * The rnodes will be destroyed
				 * by taking them off freelist,
				 * Marking them as unhashed and
				 * destorying them.
				 */
				mutex_enter(&rp->r_statelock);
				rp->r_flags &= ~RHASHED;
				mutex_exit(&rp->r_statelock);
			} else
				rpprev = rp;
		}
	}

	for (rp = rlist; rp != NULL; rp = rlist) {
		rlist = rp->r_hash;
		destroy_rnodes(rp, cr);
	}

}

/*
 * This routine destroys all the resources of rnode and
 * finally rnode itself.
 */

void
destroy_rnodes(rnode_t *rp, cred_t *cr)
{
	vnode_t *vp;
	vfs_t *vfsp;


	vp = RTOV(rp);
	vfsp = vp->v_vfsp;

	ASSERT(vp->v_count == 0);
	ASSERT(rp->r_count == 0);
	ASSERT(rp->r_lmpl == NULL);
	ASSERT(rp->r_mapcnt == 0);
	rnew--;
#ifdef DEBUG
	clstat.nrnode.value.ui64--;
#endif
	rinactive(rp, cr);
	nfs_rw_destroy(&rp->r_rwlock);
	nfs_rw_destroy(&rp->r_lkserlock);
	mutex_destroy(&rp->r_statelock);
	cv_destroy(&rp->r_cv);
	cv_destroy(&rp->r_commit.c_cv);
	nfs_free_r_path(rp);
	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
	kmem_cache_free(rnode_cache, rp);
	VFS_RELE(vfsp);
}

/*
 * Flush all vnodes in this (or every) vfs.
 * Used by nfs_sync and by nfs_unmount.
 */
void
rflush(struct vfs *vfsp, cred_t *cr)
{
	rnode_t **rpp, **erpp, *rp;
	vnode_t *vp, **vplist;
	int num, cnt;

	/*
	 * Check to see whether there is anything to do.
	 */
	num = rnew;
	if (num == 0)
		return;

	/*
	 * Allocate a slot for all currently active rnodes on the
	 * supposition that they all may need flushing.
	 */
	vplist = kmem_alloc(num * sizeof (*vplist), KM_SLEEP);
	cnt = 0;

	/*
	 * Walk the hash queues looking for rnodes with page
	 * lists associated with them.  Make a list of these
	 * files.
	 */
	erpp = &rtable[rtablesize];
	mutex_enter(&nfs_rtable_lock);
	for (rpp = rtable; rpp < erpp; rpp++) {
		for (rp = *rpp; rp != NULL; rp = rp->r_hash) {
			vp = RTOV(rp);
			/*
			 * Don't bother sync'ing a vp if it
			 * is part of virtual swap device or
			 * if VFS is read-only
			 */
			if (IS_SWAPVP(vp) ||
			    (vp->v_vfsp->vfs_flag & VFS_RDONLY))
				continue;
			/*
			 * If flushing all mounted file systems or
			 * the vnode belongs to this vfs, has pages
			 * and is marked as either dirty or mmap'd,
			 * hold and add this vnode to the list of
			 * vnodes to flush.
			 */
			if ((vfsp == NULL || vp->v_vfsp == vfsp) &&
			    vp->v_pages != NULL &&
			    ((rp->r_flags & RDIRTY) || rp->r_mapcnt > 0)) {
				VN_HOLD(vp);
				vplist[cnt++] = vp;
				if (cnt == num)
					goto toomany;
			}
		}
	}
toomany:
	mutex_exit(&nfs_rtable_lock);

	/*
	 * Flush and release all of the files on the list.
	 */
	while (cnt-- > 0) {
		vp = vplist[cnt];
		(void) VOP_PUTPAGE(vp, (u_offset_t)0, 0, B_ASYNC, cr);
		VN_RELE(vp);
	}

	/*
	 * Free the space allocated to hold the list.
	 */
	kmem_free(vplist, num * sizeof (*vplist));
}

static int
acachehash(rnode_t *rp, cred_t *cred)
{
	int sum;

	sum = (intptr_t)rp + (intptr_t)cred;
	sum >>= ACACHE_SHIFT_BITS;
	return (sum & acachemask);
}

#ifdef DEBUG
static int nfs_access_cache_hits = 0;
static int nfs_access_cache_misses = 0;
#endif

nfs_access_type_t
nfs_access_check(rnode_t *rp, uint32_t acc, cred_t *cr)
{
	acache_t *ap;
	nfs_access_type_t all;

	if (rp->r_acache != NULL) {
		rw_enter(&nfs_acache_lock, RW_READER);
		ap = acache[acachehash(rp, cr)];
		while (ap != NULL) {
			if (crcmp(ap->cred, cr) == 0 && ap->rnode == rp) {
				if ((ap->known & acc) == acc) {
#ifdef DEBUG
					nfs_access_cache_hits++;
#endif
					if ((ap->allowed & acc) == acc)
						all = NFS_ACCESS_ALLOWED;
					else
						all = NFS_ACCESS_DENIED;
				} else {
#ifdef DEBUG
					nfs_access_cache_misses++;
#endif
					all = NFS_ACCESS_UNKNOWN;
				}
				rw_exit(&nfs_acache_lock);
				return (all);
			}
			ap = ap->next;
		}
		rw_exit(&nfs_acache_lock);
	}

#ifdef DEBUG
	nfs_access_cache_misses++;
#endif
	return (NFS_ACCESS_UNKNOWN);
}

void
nfs_access_cache(rnode_t *rp, uint32_t acc, uint32_t resacc, cred_t *cr)
{
	acache_t *ap;
	int hash;

	hash = acachehash(rp, cr);

	rw_enter(&nfs_acache_lock, RW_WRITER);

	if (rp->r_acache != NULL) {
		ap = acache[hash];
		while (ap != NULL) {
			if (crcmp(ap->cred, cr) == 0 && ap->rnode == rp) {
				ap->known |= acc;
				ap->allowed &= ~acc;
				ap->allowed |= resacc;
				rw_exit(&nfs_acache_lock);
				return;
			}
			ap = ap->next;
		}
	}

	ap = kmem_cache_alloc(acache_cache, KM_NOSLEEP);
	if (ap != NULL) {
#ifdef DEBUG
		clstat.access.value.ui64++;
#endif
		ap->known = acc;
		ap->allowed = resacc;
		ap->rnode = rp;
		crhold(cr);
		ap->cred = cr;
		ap->next = acache[hash];
		acache[hash] = ap;
#ifdef DEBUG
		acachecnt[hash]++;
#endif
		ap->list = rp->r_acache;
		rp->r_acache = ap;
	}

	rw_exit(&nfs_acache_lock);
}

int
nfs_access_purge_rp(rnode_t *rp)
{
	acache_t *ap, *apprev;
	int freed;
	int hash;

	/*
	 * If there aren't any cached entries, then there is nothing
	 * to free.
	 */
	if (rp->r_acache == NULL)
		return (0);

	rw_enter(&nfs_acache_lock, RW_WRITER);

	/*
	 * Loop through each entry in the list pointed to in the
	 * rnode.  Remove each of these entries from the hash
	 * queue that it is on and remove it from the list in
	 * the rnode.
	 */
	freed = 0;
	while (rp->r_acache != NULL) {
		apprev = NULL;
		hash = acachehash(rp, rp->r_acache->cred);
		ap = acache[hash];
		while (ap != NULL) {
			if (ap == rp->r_acache) {
				if (apprev == NULL)
					acache[hash] = ap->next;
				else
					apprev->next = ap->next;
				rp->r_acache = ap->list;
#ifdef DEBUG
				acachecnt[hash]--;
#endif
				crfree(ap->cred);
				kmem_cache_free(acache_cache, ap);
				freed = 1;
#ifdef DEBUG
				clstat.access.value.ui64--;
#endif
				break;
			} else {
				apprev = ap;
				ap = ap->next;
			}
		}
		if (ap == NULL) {
			cmn_err(CE_PANIC,
			    "nfs_access_purge_rp: entry not in hash queue");
		}
	}

	rw_exit(&nfs_acache_lock);

	return (freed);
}

static const char prefix[] = ".nfs";

static kmutex_t newnum_lock;

int
newnum(void)
{
	static uint_t newnum = 0;
	uint_t id;

	mutex_enter(&newnum_lock);
	if (newnum == 0)
		newnum = hrestime.tv_sec & 0xffff;
	id = newnum++;
	mutex_exit(&newnum_lock);
	return (id);
}

char *
newname(void)
{
	char *news;
	char *s;
	const char *p;
	uint_t id;

	id = newnum();
	news = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	s = news;
	p = prefix;
	while (*p != '\0')
		*s++ = *p++;
	while (id != 0) {
		*s++ = "0123456789ABCDEF"[id & 0x0f];
		id >>= 4;
	}
	*s = '\0';
	return (news);
}

int
nfs_atoi(char *cp)
{
	int n;

	n = 0;
	while (*cp != '\0') {
		n = n * 10 + (*cp - '0');
		cp++;
	}

	return (n);
}

int
nfs_subrinit(void)
{
	ulong_t nrnode_max;

	/*
	 * Allocate and initialize the rnode hash queues
	 */
	if (nrnode <= 0)
		nrnode = ncsize;
	nrnode_max = (ulong_t)((kmem_maxavail() >> 2) / sizeof (struct rnode));
	if (nrnode > nrnode_max || (nrnode == 0 && ncsize == 0)) {
		cmn_err(CE_NOTE, "setting nrnode to max value of %ld",
				nrnode_max);
		nrnode = nrnode_max;
	}
	rtablesize = 1 << (highbit(nrnode / hashlen) - 1);
	rtablemask = rtablesize - 1;
	rtable = kmem_zalloc(rtablesize * sizeof (*rtable), KM_SLEEP);
#ifdef DEBUG
	rtablecnt = kmem_zalloc(rtablesize * sizeof (*rtablecnt), KM_SLEEP);
#endif
	rnode_cache = kmem_cache_create("rnode_cache", sizeof (rnode_t),
	    0, NULL, NULL, nfs_reclaim, NULL, NULL, 0);

	/*
	 * Allocate and initialize the access cache
	 */
	if (nacache == 0)
		nacache = nrnode;
	acachesize = 1 << (highbit(nacache / hashlen) - 1);
	acachemask = acachesize - 1;
	acache = kmem_zalloc(acachesize * sizeof (*acache), KM_SLEEP);
#ifdef DEBUG
	acachecnt = kmem_zalloc(acachesize * sizeof (*acachecnt), KM_SLEEP);
#endif
	acache_cache = kmem_cache_create("nfs_access_cache",
	    sizeof (acache_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	/*
	 * Initialize the various mutexes and reader/writer locks
	 */
	mutex_init(&chtable_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&nfs_rtable_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&newnum_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&nfs_minor_lock, NULL, MUTEX_DEFAULT, NULL);

	rw_init(&nfs_acache_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * Assign unique major number for all nfs mounts
	 */
	if ((nfs_major = getudev()) == -1) {
		cmn_err(CE_WARN, "nfs: init: can't get unique device number");
		nfs_major = 0;
	}
	nfs_minor = 0;

	if (nfs3_jukebox_delay == 0)
		nfs3_jukebox_delay = NFS3_JUKEBOX_DELAY;

	return (0);
}

void
nfs_subrfini(void)
{

	/*
	 * Deallocate the rnode hash queues
	 */
	kmem_cache_destroy(rnode_cache);
#ifdef DEBUG
	kmem_free(rtablecnt, rtablesize * sizeof (*rtablecnt));
#endif
	kmem_free(rtable, rtablesize * sizeof (*rtable));

	/*
	 * Deallocated the access cache
	 */
	kmem_cache_destroy(acache_cache);
#ifdef DEBUG
	kmem_free(acachecnt, acachesize * sizeof (*acachecnt));
#endif
	kmem_free(acache, acachesize * sizeof (*acache));

	/*
	 * Destroy the various mutexes and reader/writer locks
	 */
	mutex_destroy(&chtable_lock);
	mutex_destroy(&nfs_rtable_lock);
	mutex_destroy(&newnum_lock);
	mutex_destroy(&nfs_minor_lock);

	rw_destroy(&nfs_acache_lock);
}

enum nfsstat
puterrno(int error)
{

	switch (error) {
	case EOPNOTSUPP:
		return (NFSERR_OPNOTSUPP);
	case ENAMETOOLONG:
		return (NFSERR_NAMETOOLONG);
	case ENOTEMPTY:
		return (NFSERR_NOTEMPTY);
	case EDQUOT:
		return (NFSERR_DQUOT);
	case ESTALE:
		return (NFSERR_STALE);
	case EREMOTE:
		return (NFSERR_REMOTE);
	case ENOSYS:
		return (NFSERR_OPNOTSUPP);
	case EOVERFLOW:
		return (NFSERR_INVAL);
	default:
		return ((enum nfsstat)error);
	}
	/* NOTREACHED */
}

int
geterrno(enum nfsstat status)
{

	switch (status) {
	case NFSERR_OPNOTSUPP:
		return (EOPNOTSUPP);
	case NFSERR_NAMETOOLONG:
		return (ENAMETOOLONG);
	case NFSERR_NOTEMPTY:
		return (ENOTEMPTY);
	case NFSERR_DQUOT:
		return (EDQUOT);
	case NFSERR_STALE:
		return (ESTALE);
	case NFSERR_REMOTE:
		return (EREMOTE);
	case NFSERR_WFLUSH:
		return (EIO);
	default:
		return ((int)status);
	}
	/* NOTREACHED */
}

enum nfsstat3
puterrno3(int error)
{

#ifdef DEBUG
	switch (error) {
	case 0:
		return (NFS3_OK);
	case EPERM:
		return (NFS3ERR_PERM);
	case ENOENT:
		return (NFS3ERR_NOENT);
	case EIO:
		return (NFS3ERR_IO);
	case ENXIO:
		return (NFS3ERR_NXIO);
	case EACCES:
		return (NFS3ERR_ACCES);
	case EEXIST:
		return (NFS3ERR_EXIST);
	case EXDEV:
		return (NFS3ERR_XDEV);
	case ENODEV:
		return (NFS3ERR_NODEV);
	case ENOTDIR:
		return (NFS3ERR_NOTDIR);
	case EISDIR:
		return (NFS3ERR_ISDIR);
	case EINVAL:
		return (NFS3ERR_INVAL);
	case EFBIG:
		return (NFS3ERR_FBIG);
	case ENOSPC:
		return (NFS3ERR_NOSPC);
	case EROFS:
		return (NFS3ERR_ROFS);
	case EMLINK:
		return (NFS3ERR_MLINK);
	case ENAMETOOLONG:
		return (NFS3ERR_NAMETOOLONG);
	case ENOTEMPTY:
		return (NFS3ERR_NOTEMPTY);
	case EDQUOT:
		return (NFS3ERR_DQUOT);
	case ESTALE:
		return (NFS3ERR_STALE);
	case EREMOTE:
		return (NFS3ERR_REMOTE);
	case EOPNOTSUPP:
		return (NFS3ERR_NOTSUPP);
	case EOVERFLOW:
		return (NFS3ERR_INVAL);
	default:
		cmn_err(CE_WARN, "puterrno3: got error %d", error);
		return ((enum nfsstat3)error);
	}
#else
	switch (error) {
	case ENAMETOOLONG:
		return (NFS3ERR_NAMETOOLONG);
	case ENOTEMPTY:
		return (NFS3ERR_NOTEMPTY);
	case EDQUOT:
		return (NFS3ERR_DQUOT);
	case ESTALE:
		return (NFS3ERR_STALE);
	case EOPNOTSUPP:
		return (NFS3ERR_NOTSUPP);
	case EREMOTE:
		return (NFS3ERR_REMOTE);
	case EOVERFLOW:
		return (NFS3ERR_INVAL);
	default:
		return ((enum nfsstat3)error);
	}
#endif
}

int
geterrno3(enum nfsstat3 status)
{

#ifdef DEBUG
	switch (status) {
	case NFS3_OK:
		return (0);
	case NFS3ERR_PERM:
		return (EPERM);
	case NFS3ERR_NOENT:
		return (ENOENT);
	case NFS3ERR_IO:
		return (EIO);
	case NFS3ERR_NXIO:
		return (ENXIO);
	case NFS3ERR_ACCES:
		return (EACCES);
	case NFS3ERR_EXIST:
		return (EEXIST);
	case NFS3ERR_XDEV:
		return (EXDEV);
	case NFS3ERR_NODEV:
		return (ENODEV);
	case NFS3ERR_NOTDIR:
		return (ENOTDIR);
	case NFS3ERR_ISDIR:
		return (EISDIR);
	case NFS3ERR_INVAL:
		return (EINVAL);
	case NFS3ERR_FBIG:
		return (EFBIG);
	case NFS3ERR_NOSPC:
		return (ENOSPC);
	case NFS3ERR_ROFS:
		return (EROFS);
	case NFS3ERR_MLINK:
		return (EMLINK);
	case NFS3ERR_NAMETOOLONG:
		return (ENAMETOOLONG);
	case NFS3ERR_NOTEMPTY:
		return (ENOTEMPTY);
	case NFS3ERR_DQUOT:
		return (EDQUOT);
	case NFS3ERR_STALE:
		return (ESTALE);
	case NFS3ERR_REMOTE:
		return (EREMOTE);
	case NFS3ERR_BADHANDLE:
		return (ESTALE);
	case NFS3ERR_NOT_SYNC:
		return (EINVAL);
	case NFS3ERR_BAD_COOKIE:
		return (EINVAL);
	case NFS3ERR_NOTSUPP:
		return (EOPNOTSUPP);
	case NFS3ERR_TOOSMALL:
		return (EINVAL);
	case NFS3ERR_SERVERFAULT:
		return (EIO);
	case NFS3ERR_BADTYPE:
		return (EINVAL);
	case NFS3ERR_JUKEBOX:
		return (ENXIO);
	default:
		cmn_err(CE_WARN, "geterrno3: got status %d", status);
		return ((int)status);
	}
#else
	switch (status) {
	case NFS3ERR_NAMETOOLONG:
		return (ENAMETOOLONG);
	case NFS3ERR_NOTEMPTY:
		return (ENOTEMPTY);
	case NFS3ERR_DQUOT:
		return (EDQUOT);
	case NFS3ERR_STALE:
	case NFS3ERR_BADHANDLE:
		return (ESTALE);
	case NFS3ERR_NOTSUPP:
		return (EOPNOTSUPP);
	case NFS3ERR_REMOTE:
		return (EREMOTE);
	case NFS3ERR_NOT_SYNC:
	case NFS3ERR_BAD_COOKIE:
	case NFS3ERR_TOOSMALL:
	case NFS3ERR_BADTYPE:
		return (EINVAL);
	case NFS3ERR_SERVERFAULT:
		return (EIO);
	default:
		return ((int)status);
	}
#endif
}

#ifdef DEBUG
rddir_cache *
rddir_cache_alloc(size_t size, int flags)
{
	rddir_cache *rc;

	rc = kmem_alloc(size, flags);
	if (rc != NULL)
		clstat.dirent.value.ui64++;
	return (rc);
}

void
rddir_cache_free(void *addr, size_t size)
{

	clstat.dirent.value.ui64--;
	kmem_free(addr, size);
}

char *
symlink_cache_alloc(size_t size, int flags)
{
	char *rc;

	rc = kmem_alloc(size, flags);
	if (rc != NULL)
		clstat.symlink.value.ui64++;
	return (rc);
}

void
symlink_cache_free(void *addr, size_t size)
{

	clstat.symlink.value.ui64--;
	kmem_free(addr, size);
}
#endif

static int
nfs_free_data_reclaim(rnode_t *rp, int noblock)
{
	rddir_cache *rdc, *nrdc;
	char *contents;
	int size;
	vsecattr_t *vsp;
	nfs3_pathconf_info *info;
	int freed;
	cred_t *cred;

	/*
	 * Free any held credentials and caches which
	 * may be associated with this rnode.
	 */
	mutex_enter(&rp->r_statelock);
	cred = rp->r_cred;
	rp->r_cred = NULL;
	rdc = rp->r_dir;
	/*
	 * If we aren't going to wait for each busy readdir cache entry,
	 * leave these fields alone.
	 */
	if (!noblock) {
		rp->r_dir = NULL;
		rp->r_direof = NULL;
	}
	contents = rp->r_symlink.contents;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	vsp = rp->r_secattr;
	rp->r_secattr = NULL;
	info = rp->r_pathconf;
	rp->r_pathconf = NULL;
	mutex_exit(&rp->r_statelock);

	if (cred != NULL)
		crfree(cred);

	/*
	 * Free the access cache entries.
	 */
	freed = nfs_access_purge_rp(rp);

	if (rdc == NULL &&
	    contents == NULL &&
	    vsp == NULL &&
	    info == NULL)
		return (freed);

	/*
	 * Free the readdir cache entries, incompletely if we can't block.
	 */
	if (noblock) {
		if (rdc != NULL)
			nfs_purge_rddir_cache(RTOV(rp));
	} else {
		while (rdc != NULL) {
			nrdc = rdc->next;
			mutex_enter(&rp->r_statelock);
			while (rdc->flags & RDDIR) {
				rdc->flags |= RDDIRWAIT;
				cv_wait(&rdc->cv, &rp->r_statelock);
			}
			/*
			 * It is caller's responsibilty to clear the
			 * RDDIRWAIT bits since it is incorrect to
			 * have RDDIRWAIT set while we are freeing
			 * the readdir cache entires.
			 */
			ASSERT(!(rdc->flags & (RDDIR | RDDIRWAIT)));
			mutex_exit(&rp->r_statelock);
			if (rdc->entries != NULL)
				kmem_free(rdc->entries, rdc->buflen);
			cv_destroy(&rdc->cv);
#ifdef DEBUG
			rddir_cache_free((void *)rdc, sizeof (*rdc));
#else
			kmem_free(rdc, sizeof (*rdc));
#endif
			rdc = nrdc;
		}
	}

	/*
	 * Free the symbolic link cache.
	 */
	if (contents != NULL) {
#ifdef DEBUG
		symlink_cache_free((void *)contents, size);
#else
		kmem_free(contents, size);
#endif
	}

	/*
	 * Free any cached ACL.
	 */
	if (vsp != NULL)
		nfs_acl_free(vsp);

	/*
	 * Free any cached pathconf information.
	 */
	if (info != NULL)
		kmem_free(info, sizeof (*info));

	return (1);
}

static int
nfs_active_data_reclaim(rnode_t *rp)
{
	char *contents;
	int size;
	vsecattr_t *vsp;
	nfs3_pathconf_info *info;
	int freed;

	/*
	 * Free any held credentials and caches which
	 * may be associated with this rnode.
	 */
	if (!mutex_tryenter(&rp->r_statelock))
		return (0);
	contents = rp->r_symlink.contents;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	vsp = rp->r_secattr;
	rp->r_secattr = NULL;
	info = rp->r_pathconf;
	rp->r_pathconf = NULL;
	mutex_exit(&rp->r_statelock);

	/*
	 * Free the access cache entries.
	 */
	freed = nfs_access_purge_rp(rp);

	if (contents == NULL &&
	    vsp == NULL &&
	    rp->r_dir == NULL &&
	    info == NULL)
		return (freed);

	/*
	 * Free the symbolic link cache.
	 */
	if (contents != NULL) {
#ifdef DEBUG
		symlink_cache_free((void *)contents, size);
#else
		kmem_free(contents, size);
#endif
	}

	/*
	 * Free any cached ACL.
	 */
	if (vsp != NULL)
		nfs_acl_free(vsp);

	if (rp->r_dir != NULL)
		nfs_purge_rddir_cache(RTOV(rp));

	/*
	 * Free any cached pathconf information.
	 */
	if (info != NULL)
		kmem_free(info, sizeof (*info));

	return (1);
}

static int
nfs_free_reclaim(void)
{
	int freed;
	rnode_t *rp;

#ifdef DEBUG
	clstat.f_reclaim.value.ui64++;
#endif
	freed = 0;
	mutex_enter(&nfs_rtable_lock);
	rp = rpfreelist;
	if (rp != NULL) {
		do {
			if (nfs_free_data_reclaim(rp, 0))
				freed = 1;
		} while ((rp = rp->r_freef) != rpfreelist);
	}
	mutex_exit(&nfs_rtable_lock);
	return (freed);
}

static int
nfs_active_reclaim(void)
{
	int freed;
	rnode_t **rpp, **erpp, *rp;

#ifdef DEBUG
	clstat.a_reclaim.value.ui64++;
#endif
	freed = 0;
	erpp = &rtable[rtablesize];
	mutex_enter(&nfs_rtable_lock);
	for (rpp = rtable; rpp < erpp; rpp++) {
		for (rp = *rpp; rp != NULL; rp = rp->r_hash) {
			if (nfs_active_data_reclaim(rp))
				freed = 1;
		}
	}
	mutex_exit(&nfs_rtable_lock);
	return (freed);
}

static int
nfs_rnode_reclaim(void)
{
	int freed;
	rnode_t *rp;
	vnode_t *vp;

#ifdef DEBUG
	clstat.r_reclaim.value.ui64++;
#endif
	mutex_enter(&nfs_rtable_lock);
	if (rpfreelist == NULL) {
		mutex_exit(&nfs_rtable_lock);
		return (0);
	}
	freed = 0;
	while ((rp = rpfreelist) != NULL) {
		rp_rmfree(rp);
		if (rp->r_flags & RHASHED)
			rp_rmhash(rp);
		vp = RTOV(rp);
		VN_HOLD(vp);
		mutex_exit(&nfs_rtable_lock);
		rinactive(rp, CRED());
		mutex_enter(&nfs_rtable_lock);
		/*
		 * Recheck the reference count to make sure that
		 * someone else didn't pick up a reference.  If no one
		 * did, then complete the rest of the processing to
		 * free the rnode.  Otherwise, just release our
		 * reference and let the other thread complete the
		 * processing to free the rnode.
		 *
		 * This code needs to resemble the code in rp_addfree
		 * which can destroy the rnode on last reference and
		 * the code in make_rnode which removes rnodes from
		 * the freelist and recycles them.
		 */
		mutex_enter(&vp->v_lock);
		if (vp->v_count-- == 1) {
			ASSERT(rp->r_count == 0);
			ASSERT(rp->r_lmpl == NULL);
			ASSERT(rp->r_mapcnt == 0);
			ASSERT(!(rp->r_flags & RHASHED));
			ASSERT(rp->r_freef == NULL && rp->r_freeb == NULL);
			mutex_exit(&vp->v_lock);
			nfs_rw_destroy(&rp->r_rwlock);
			nfs_rw_destroy(&rp->r_lkserlock);
			mutex_destroy(&rp->r_statelock);
			cv_destroy(&rp->r_cv);
			cv_destroy(&rp->r_commit.c_cv);
			nfs_free_r_path(rp);
			mutex_destroy(&vp->v_lock);
			cv_destroy(&vp->v_cv);
			rnew--;
#ifdef DEBUG
			clstat.nrnode.value.ui64--;
#endif
			kmem_cache_free(rnode_cache, rp);
			freed = 1;
		} else
			mutex_exit(&vp->v_lock);
	}
	mutex_exit(&nfs_rtable_lock);
	return (freed);
}

/*ARGSUSED*/
static void
nfs_reclaim(void *cdrarg)
{

#ifdef DEBUG
	clstat.reclaim.value.ui64++;
#endif
	if (nfs_free_reclaim())
		return;

	if (nfs_active_reclaim())
		return;

	(void) nfs_rnode_reclaim();
}

/*
 * NFS client failover support
 *
 * Routines to copy filehandles
 */
void
nfscopyfh(caddr_t fhp, vnode_t *vp)
{
	fhandle_t *dest = (fhandle_t *)fhp;

	if (dest != NULL)
		*dest = *VTOFH(vp);
}

void
nfs3copyfh(caddr_t fhp, vnode_t *vp)
{
	nfs_fh3 *dest = (nfs_fh3 *)fhp;

	if (dest != NULL)
		*dest = *VTOFH3(vp);
}

/*
 * NFS client failover support
 *
 * failover_safe() will test various conditions to ensure that
 * failover is permitted for this vnode.  It will be denied
 * if:
 *	1) the operation in progress does not support failover (NULL fi)
 *	2) there are no available replicas (NULL mi_servers->sv_next)
 *	3) any locks are outstanding on this file
 */
static int
failover_safe(failinfo_t *fi)
{

	/*
	 * Does this op permit failover?
	 */
	if (fi == NULL || fi->vp == NULL)
		return (0);

	/*
	 * Are there any alternates to failover to?
	 */
	if (VTOMI(fi->vp)->mi_servers->sv_next == NULL)
		return (0);

	/*
	 * Disable check; we've forced local locking
	 *
	 * if (flk_has_remote_locks(fi->vp))
	 *	return (0);
	 */

	/*
	 * If we have no partial path, we can't do anything
	 */
	if (VTOR(fi->vp)->r_path == NULL)
		return (0);

	return (1);
}

#include <sys/thread.h>

/*
 * NFS client failover support
 *
 * failover_newserver() will start a search for a new server,
 * preferably by starting an async thread to do the work.  If
 * someone is already doing this (recognizable by MI_BINDINPROG
 * being set), it will simply return and the calling thread
 * will queue on the mi_failover_cv condition variable.
 */
static void
failover_newserver(mntinfo_t *mi)
{

	/*
	 * Check if someone else is doing this already
	 */
	mutex_enter(&mi->mi_lock);
	if (mi->mi_flags & MI_BINDINPROG) {
		mutex_exit(&mi->mi_lock);
		return;
	}
	mi->mi_flags |= MI_BINDINPROG;

	/*
	 * Need to hold the vfs struct so that it can't be released
	 * while the failover thread is selecting a new server.
	 */
	VFS_HOLD(mi->mi_vfsp);

	/*
	 * Start a thread to do the real searching.  We have to
	 * get this done before anyone access the filesystem,
	 * so we'll loop with a delay.
	 */
	while (thread_create(NULL, NULL, failover_thread, (caddr_t)mi,
	    0, &p0, TS_RUN, 60) == NULL) {
		delay(hz);
	}
	mutex_exit(&mi->mi_lock);
}

/*
 * NFS client failover support
 *
 * failover_thread() will find a new server to replace the one
 * currently in use, wake up other threads waiting on this mount
 * point, and die.  It will start at the head of the server list
 * and poll servers until it finds one with an NFS server which is
 * registered and responds to a NULL procedure ping.
 *
 * XXX failover_thread is unsafe within the scope of the
 * present model defined for cpr to suspend the system.
 * Specifically, over-the-wire calls made by the thread
 * are unsafe. The thread needs to be reevaluated in case of
 * future updates to the cpr suspend model.
 */
static void
failover_thread(mntinfo_t *mi)
{
	servinfo_t *s, *svp = NULL;
	CLIENT *cl;
	enum clnt_stat status;
	struct timeval tv;
	int error;
	int oncethru = 0;
	callb_cpr_t cprinfo;
	rnode_t *rp;

	/*
	 * Its safe to piggyback on the mi_lock since failover_newserver()
	 * code guarantees that there will be only one failover thread
	 * per mountinfo at any instance.
	 */
	CALLB_CPR_INIT(&cprinfo, &mi->mi_lock, callb_generic_cpr,
	    "failover_thread");

	mutex_enter(&mi->mi_lock);
	CALLB_CPR_SAFE_BEGIN(&cprinfo);
	while (mi->mi_readers)
		cv_wait(&mi->mi_failover_cv, &mi->mi_lock);
	CALLB_CPR_SAFE_END(&cprinfo, &mi->mi_lock);
	mutex_exit(&mi->mi_lock);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	/*
	 * Ping the null NFS procedure of every server in
	 * the list until one responds.  We always start
	 * at the head of the list and always skip the one
	 * that is current, since it's caused us a problem.
	 */
	while (svp == NULL) {
		for (svp = mi->mi_servers; svp; svp = svp->sv_next) {
			if (!oncethru && svp == mi->mi_curr_serv)
				continue;

			/*
			 * If the file system was forcibly umounted
			 * while trying to do a failover, then just
			 * give up on the failover.  It won't matter
			 * what the server is.
			 */
			if (mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED) {
				svp = NULL;
				goto done;
			}

			error = clnt_tli_kcreate(svp->sv_knconf, &svp->sv_addr,
			    NFS_PROGRAM, NFS_VERSION, 0, 1, CRED(), &cl);
			if (error)
				continue;

			if (!(mi->mi_flags & MI_INT))
				cl->cl_nosignal = TRUE;
			status = CLNT_CALL(cl, RFS_NULL, xdr_void, NULL,
			    xdr_void, NULL, tv);
			if (!(mi->mi_flags & MI_INT))
				cl->cl_nosignal = FALSE;
			AUTH_DESTROY(cl->cl_auth);
			CLNT_DESTROY(cl);
			if (status == RPC_SUCCESS) {
				if (svp == mi->mi_curr_serv) {
#ifdef DEBUG
					cmn_err(CE_NOTE,
			"NFS%d: failing over: selecting original server %s",
					    mi->mi_vers, svp->sv_hostname);
#else
					cmn_err(CE_NOTE,
			"NFS: failing over: selecting original server %s",
					    svp->sv_hostname);
#endif
				} else {
#ifdef DEBUG
					cmn_err(CE_NOTE,
				    "NFS%d: failing over from %s to %s",
					    mi->mi_vers,
					    mi->mi_curr_serv->sv_hostname,
					    svp->sv_hostname);
#else
					cmn_err(CE_NOTE,
				    "NFS: failing over from %s to %s",
					    mi->mi_curr_serv->sv_hostname,
					    svp->sv_hostname);
#endif
				}
				break;
			}
		}

		if (svp == NULL) {
			if (!oncethru) {
#ifdef DEBUG
				printf("NFS%d servers ", mi->mi_vers);
#else
				printf("NFS servers ");
#endif
				for (s = mi->mi_servers; s; s = s->sv_next)
					printf("%s%s", s->sv_hostname,
					    s->sv_next ? "," : " ");
				printf("not responding still trying\n");
				oncethru = 1;
			}
			mutex_enter(&mi->mi_lock);
			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			mutex_exit(&mi->mi_lock);
			delay(hz);
			mutex_enter(&mi->mi_lock);
			CALLB_CPR_SAFE_END(&cprinfo, &mi->mi_lock);
			mutex_exit(&mi->mi_lock);
		}
	}

	if (oncethru) {
#ifdef DEBUG
		printf("NFS%d servers ", mi->mi_vers);
#else
		printf("NFS servers ");
#endif
		for (s = mi->mi_servers; s; s = s->sv_next)
			printf("%s%s", s->sv_hostname, s->sv_next ? "," : " ");
		printf("ok\n");
	}

	if (svp != mi->mi_curr_serv) {
		(void) dnlc_purge_vfsp(mi->mi_vfsp, 0);
		mutex_enter(&nfs_rtable_lock);
		rp = rfind(&mi->mi_curr_serv->sv_fhandle, mi->mi_vfsp);
		if (rp != NULL) {
			if (rp->r_flags & RHASHED)
				rp_rmhash(rp);
			rp->r_server = svp;
			rp->r_fh = svp->sv_fhandle;
			rp_addhash(rp);
			(void) nfs_free_data_reclaim(rp, 1);
			mutex_exit(&nfs_rtable_lock);
			VN_RELE(RTOV(rp));
		} else
			mutex_exit(&nfs_rtable_lock);
	}

done:
	mutex_enter(&mi->mi_lock);
	mi->mi_flags &= ~MI_BINDINPROG;
	if (svp != NULL) {
		mi->mi_curr_serv = svp;
		mi->mi_failover++;
#ifdef DEBUG
	clstat.failover.value.ui64++;
#endif
	}
	cv_broadcast(&mi->mi_failover_cv);
	CALLB_CPR_EXIT(&cprinfo);
	VFS_RELE(mi->mi_vfsp);
	thread_exit();
	/* NOTREACHED */
}

/*
 * NFS client failover support
 *
 * failover_wait() will put the thread to sleep until MI_BINDINPROG
 * is cleared, meaning that failover is complete.  Called with
 * mi_lock mutex held.
 */
static int
failover_wait(mntinfo_t *mi)
{
	k_sigset_t smask;

	/*
	 * If someone else is hunting for a living server,
	 * sleep until it's done.  After our sleep, we may
	 * be bound to the right server and get off cheaply.
	 */
	while (mi->mi_flags & MI_BINDINPROG) {
		/*
		 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
		 * and SIGTERM. (Preserving the existing masks).
		 * Mask out SIGINT if mount option nointr is specified.
		 */
		sigintr(&smask, (int)mi->mi_flags & MI_INT);
		if (!cv_wait_sig(&mi->mi_failover_cv, &mi->mi_lock)) {
			/*
			 * restore original signal mask
			 */
			sigunintr(&smask);
			return (EINTR);
		}
		/*
		 * restore original signal mask
		 */
		sigunintr(&smask);
	}
	return (0);
}

/*
 * NFS client failover support
 *
 * failover_remap() will do a partial pathname lookup and find the
 * desired vnode on the current server.  The interim vnode will be
 * discarded after we pilfer the new filehandle.
 *
 * Side effects:
 * - This routine will also update the filehandle in the args structure
 *    pointed to by the fi->fhp pointer if it is non-NULL.
 */

static int
failover_remap(failinfo_t *fi)
{
	vnode_t *vp, *nvp, *rootvp;
	rnode_t *rp, *nrp;
	mntinfo_t *mi;
	int error;

	/*
	 * Sanity check
	 */
	if (fi == NULL || fi->vp == NULL || fi->lookupproc == NULL)
		return (EINVAL);
	vp = fi->vp;
	rp = VTOR(vp);
	mi = VTOMI(vp);

	if (!(vp->v_flag & VROOT)) {
		/*
		 * Given the root fh, use the path stored in
		 * the rnode to find the fh for the new server.
		 */
		error = VFS_ROOT(mi->mi_vfsp, &rootvp);
		if (error)
			return (error);

		error = failover_lookup(rp->r_path, rootvp,
		    fi->lookupproc, &nvp);

		VN_RELE(rootvp);

		if (error)
			return (error);

		/*
		 * If we found the same rnode, we're done now
		 */
		if (nvp == vp) {
			VN_RELE(nvp);
			return (0);
		}

		/*
		 * Try to make it so that no one else will find this
		 * vnode because it is just a temporary to hold the
		 * new file handle until that file handle can be
		 * copied to the original vnode/rnode.
		 */
		nrp = VTOR(nvp);
		mutex_enter(&nfs_rtable_lock);
		if (nrp->r_flags & RHASHED)
			rp_rmhash(nrp);

		/*
		 * As a heuristic check on the validity of the new
		 * file, check that the size and type match against
		 * that we remember from the old version.
		 */
		if (rp->r_size != nrp->r_size || vp->v_type != nvp->v_type) {
			cmn_err(CE_WARN,
			    "NFS replicas %s and %s: file %s not same.",
			    rp->r_server->sv_hostname,
			    nrp->r_server->sv_hostname, rp->r_path);
			mutex_exit(&nfs_rtable_lock);
			VN_RELE(nvp);
			return (EINVAL);
		}

		/*
		 * snarf the filehandle from the new rnode
		 * then release it, again while updating the
		 * hash queues for the rnode.
		 */
		if (rp->r_flags & RHASHED)
			rp_rmhash(rp);
		rp->r_server = mi->mi_curr_serv;
		rp->r_fh = nrp->r_fh;
		rp_addhash(rp);
		/*
		 * Copy the attributes from the new rnode to the old
		 * rnode.  This will help to reduce unnecessary page
		 * cache flushes.
		 */
		rp->r_attr = nrp->r_attr;
		rp->r_attrtime = nrp->r_attrtime;
		rp->r_mtime = nrp->r_mtime;
		(void) nfs_free_data_reclaim(rp, 1);
		mutex_exit(&nfs_rtable_lock);
		VN_RELE(nvp);
	}

	/*
	 * Update successful failover remap count
	 */
	mutex_enter(&mi->mi_lock);
	mi->mi_remap++;
	mutex_exit(&mi->mi_lock);
#ifdef DEBUG
	clstat.remap.value.ui64++;
#endif

	/*
	 * If we have a copied filehandle to update, do it now.
	 */
	if (fi->fhp != NULL && fi->copyproc != NULL)
		(*fi->copyproc)(fi->fhp, vp);

	return (0);
}

/*
 * NFS client failover support
 *
 * We want a simple pathname lookup routine to parse the pieces
 * of path in rp->r_path.  We know that the path was a created
 * as rnodes were made, so we know we have only to deal with
 * paths that look like:
 *	dir1/dir2/dir3/file
 * Any evidence of anything like .., symlinks, and ENOTDIR
 * are hard errors, because they mean something in this filesystem
 * is different from the one we came from, or has changed under
 * us in some way.  If this is true, we want the failure.
 */
static int
failover_lookup(char *path, vnode_t *root,
    int (*lookupproc)(vnode_t *, char *, vnode_t **, struct pathname *, int,
	vnode_t *, cred_t *, int),
    vnode_t **new)
{
	vnode_t *dvp, *nvp;
	int error = EINVAL;
	char *s, *p, *tmppath;
	size_t len;

	/* Make local copy of path */
	len = strlen(path) + 1;
	tmppath = kmem_alloc(len, KM_SLEEP);
	(void) strcpy(tmppath, path);
	s = tmppath;

	dvp = root;
	VN_HOLD(dvp);

	do {
		p = strchr(s, '/');
		if (p != NULL)
			*p = '\0';
		error = (*lookupproc)(dvp, s, &nvp, NULL, 0, NULL, CRED(),
		    RFSCALL_SOFT);
		if (p != NULL)
			*p++ = '/';
		if (error) {
			kmem_free(tmppath, len);
			return (error);
		}
		s = p;
		VN_RELE(dvp);
		dvp = nvp;
	} while (p != NULL);

	if (nvp != NULL && new != NULL)
		*new = nvp;
	kmem_free(tmppath, len);
	return (0);
}

/*
 * NFS client failover support
 *
 * sv_free() frees the malloc'd portion of a "servinfo_t".
 */
void
sv_free(servinfo_t *svp)
{
	servinfo_t *next;
	struct knetconfig *knconf;

	while (svp != NULL) {
		next = svp->sv_next;
		if (svp->sv_secdata)
			sec_clnt_freeinfo(svp->sv_secdata);
		if (svp->sv_hostname && svp->sv_hostnamelen > 0)
			kmem_free(svp->sv_hostname, svp->sv_hostnamelen);
		knconf = svp->sv_knconf;
		if (knconf != NULL) {
			if (knconf->knc_protofmly != NULL)
				kmem_free(knconf->knc_protofmly, KNC_STRSIZE);
			if (knconf->knc_proto != NULL)
				kmem_free(knconf->knc_proto, KNC_STRSIZE);
			kmem_free(knconf, sizeof (*knconf));
		}
		if (svp->sv_addr.buf != NULL && svp->sv_addr.maxlen != 0)
			kmem_free(svp->sv_addr.buf, svp->sv_addr.maxlen);
		kmem_free(svp, sizeof (*svp));
		svp = next;
	}
}

/*
 * The following routine was used only by the WabiGetfh()
 * routine of the Wabi driver to obtain filehandles
 * for locking in Solaris 2.5. It is replaced by an
 * improved fcntl() interface in 2.6.
 *
 * This stub allows older SunPC/Wabi applications
 * to continue to run, but without network locking/sharing.
 */
#if 0 /* notdef */
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <sys/cmn_err.h>
#endif

/*ARGSUSED*/
int
wabi_makefhandle(int fd, struct nfs_fhandle *fh, int *vers)
{

	return (EREMOTE);
}

int
nfs_rw_enter_sig(nfs_rwlock_t *l, krw_t rw, int intr)
{

	mutex_enter(&l->lock);

	/*
	 * If this is a nested enter, then allow it.  There
	 * must be as many exits as enters through.
	 */
	if (l->owner == curthread) {
		/* lock is held for writing by current thread */
		ASSERT(rw == RW_READER || rw == RW_WRITER);
		l->count--;
	} else if (rw == RW_READER) {
		/*
		 * While there is a writer active or writers waiting,
		 * then wait for them to finish up and move on.  Then,
		 * increment the count to indicate that a reader is
		 * active.
		 */
		while (l->count < 0 || l->waiters > 0) {
			if (intr) {
				if (!cv_wait_sig(&l->cv, &l->lock)) {
					mutex_exit(&l->lock);
					return (EINTR);
				}
			} else
				cv_wait(&l->cv, &l->lock);
		}
		l->count++;
	} else {
		ASSERT(rw == RW_WRITER);
		/*
		 * While there are readers active or a writer
		 * active, then wait for all of the readers
		 * to finish or for the writer to finish.
		 * Then, set the owner field to curthread and
		 * decrement count to indicate that a writer
		 * is active.
		 */
		while (l->count > 0 || l->owner != NULL) {
			l->waiters++;
			if (intr) {
				if (!cv_wait_sig(&l->cv, &l->lock)) {
					l->waiters--;
					cv_broadcast(&l->cv);
					mutex_exit(&l->lock);
					return (EINTR);
				}
			} else
				cv_wait(&l->cv, &l->lock);
			l->waiters--;
		}
		l->owner = curthread;
		l->count--;
	}

	mutex_exit(&l->lock);

	return (0);
}

/*
 * If the lock is available, obtain it and return non-zero.  If there is
 * already a conflicting lock, return 0 immediately.
 */

int
nfs_rw_tryenter(nfs_rwlock_t *l, krw_t rw)
{
	mutex_enter(&l->lock);

	/*
	 * If this is a nested enter, then allow it.  There
	 * must be as many exits as enters through.
	 */
	if (l->owner == curthread) {
		/* lock is held for writing by current thread */
		ASSERT(rw == RW_READER || rw == RW_WRITER);
		l->count--;
	} else if (rw == RW_READER) {
		/*
		 * If there is a writer active or writers waiting, deny the
		 * lock.  Otherwise, bump the count of readers.
		 */
		if (l->count < 0 || l->waiters > 0) {
			mutex_exit(&l->lock);
			return (0);
		}
		l->count++;
	} else {
		ASSERT(rw == RW_WRITER);
		/*
		 * If there are readers active or a writer active, deny the
		 * lock.  Otherwise, set the owner field to curthread and
		 * decrement count to indicate that a writer is active.
		 */
		if (l->count > 0 || l->owner != NULL) {
			mutex_exit(&l->lock);
			return (0);
		}
		l->owner = curthread;
		l->count--;
	}

	mutex_exit(&l->lock);

	return (1);
}

void
nfs_rw_exit(nfs_rwlock_t *l)
{

	mutex_enter(&l->lock);
	/*
	 * If this is releasing a writer lock, then increment count to
	 * indicate that there is one less writer active.  If this was
	 * the last of possibly nested writer locks, then clear the owner
	 * field as well to indicate that there is no writer active
	 * and wakeup any possible waiting writers or readers.
	 *
	 * If releasing a reader lock, then just decrement count to
	 * indicate that there is one less reader active.  If this was
	 * the last active reader and there are writer(s) waiting,
	 * then wake up the first.
	 */
	if (l->owner != NULL) {
		ASSERT(l->owner == curthread);
		l->count++;
		if (l->count == 0) {
			l->owner = NULL;
			cv_broadcast(&l->cv);
		}
	} else {
		l->count--;
		if (l->count == 0 && l->waiters > 0)
			cv_broadcast(&l->cv);
	}
	mutex_exit(&l->lock);
}

int
nfs_rw_lock_held(nfs_rwlock_t *l, krw_t rw)
{

	if (rw == RW_READER)
		return (l->count > 0);
	ASSERT(rw == RW_WRITER);
	return (l->count < 0);
}

/* ARGSUSED */
void
nfs_rw_init(nfs_rwlock_t *l, char *name, krw_type_t type, void *arg)
{

	mutex_init(&l->lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&l->cv, NULL, CV_DEFAULT, NULL);
}

void
nfs_rw_destroy(nfs_rwlock_t *l)
{

	mutex_destroy(&l->lock);
	cv_destroy(&l->cv);
}
