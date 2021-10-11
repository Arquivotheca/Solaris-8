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
 *  	Copyright (c) 1986-1991,1994-1999 by Sun Microsystems, Inc.
 *  	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident	"@(#)nfs_client.c	1.134	99/11/11 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/dnlc.h>
#include <sys/vmsystm.h>
#include <sys/flock.h>
#include <sys/share.h>
#include <sys/cmn_err.h>
#include <sys/tiuser.h>
#include <sys/sysmacros.h>
#include <sys/callb.h>
#include <sys/acl.h>
#include <sys/kstat.h>
#include <sys/signal.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/nfs_acl.h>
#include <nfs/lm.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>

static int	nfs_getattr_cache(vnode_t *, struct vattr *);
static int	nfs_remove_locking_id(vnode_t *, int, char *, char *, int *);

/* Debugging flag for PC file shares. */
extern int	share_debug;

/*
 * Attributes caching:
 *
 * Attributes are cached in the rnode in struct vattr form.
 * There is a time associated with the cached attributes (r_attrtime)
 * which tells whether the attributes are valid. The time is initialized
 * to the difference between current time and the modify time of the vnode
 * when new attributes are cached. This allows the attributes for
 * files that have changed recently to be timed out sooner than for files
 * that have not changed for a long time. There are minimum and maximum
 * timeout values that can be set per mount point.
 */

/*
 * Validate caches by checking cached attributes. If they have timed out,
 * then get new attributes from the server.  As a side effect, cache
 * invalidation is done if the attributes have changed.
 */
int
nfs_validate_caches(vnode_t *vp, cred_t *cr)
{
	int error;
	rnode_t *rp;
	struct vattr va;

	if (ATTRCACHE_VALID(vp))
		return (0);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RSERIALIZE) {
		k_sigset_t smask;
		sigintr(&smask, VTOMI(vp)->mi_flags & MI_INT);
		if (!cv_wait_sig(&rp->r_cv, &rp->r_statelock)) {
			sigunintr(&smask);
			mutex_exit(&rp->r_statelock);
			return (EINTR);
		}
		sigunintr(&smask);
	}
	if (ATTRCACHE_VALID(vp)) {
		mutex_exit(&rp->r_statelock);
		return (0);
	}
	rp->r_flags |= RSERIALIZE;
	mutex_exit(&rp->r_statelock);

	va.va_mask = AT_ALL;
	error = nfs_getattr_otw(vp, &va, cr);
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~RSERIALIZE;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);
	return (error);
}

/*
 * Validate caches by checking cached attributes. If they have timed out,
 * then get new attributes from the server.  As a side effect, cache
 * invalidation is done if the attributes have changed.
 */
int
nfs3_validate_caches(vnode_t *vp, cred_t *cr)
{
	int error;
	rnode_t *rp;
	struct vattr va;

	if (ATTRCACHE_VALID(vp))
		return (0);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RSERIALIZE) {
		k_sigset_t smask;
		sigintr(&smask, VTOMI(vp)->mi_flags & MI_INT);
		if (!cv_wait_sig(&rp->r_cv, &rp->r_statelock)) {
			sigunintr(&smask);
			mutex_exit(&rp->r_statelock);
			return (EINTR);
		}
		sigunintr(&smask);
	}
	if (ATTRCACHE_VALID(vp)) {
		mutex_exit(&rp->r_statelock);
		return (0);
	}
	rp->r_flags |= RSERIALIZE;
	mutex_exit(&rp->r_statelock);

	va.va_mask = AT_ALL;
	error = nfs3_getattr_otw(vp, &va, cr);
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~RSERIALIZE;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);
	return (error);
}

/*
 * Purge all of the various NFS `data' caches.
 */
void
nfs_purge_caches(vnode_t *vp, cred_t *cr)
{
	rnode_t *rp;
	char *contents;
	int size;
	int error;

	/*
	 * Purge the DNLC for any entries which refer to this file.
	 */
	if (vp->v_count > 1)
		dnlc_purge_vp(vp);

	/*
	 * Clear any readdir state bits and purge the readlink response cache.
	 */
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~REOF;
	contents = rp->r_symlink.contents;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	mutex_exit(&rp->r_statelock);

	if (contents != NULL) {
#ifdef DEBUG
		symlink_cache_free((void *)contents, size);
#else
		kmem_free(contents, size);
#endif
	}

	/*
	 * Flush the page cache.
	 */
	if (vp->v_pages != NULL) {
		error = VOP_PUTPAGE(vp, (u_offset_t)0, 0, B_INVAL, cr);
		if (error && (error == ENOSPC || error == EDQUOT)) {
			mutex_enter(&rp->r_statelock);
			if (!rp->r_error)
				rp->r_error = error;
			mutex_exit(&rp->r_statelock);
		}
	}

	/*
	 * Flush the readdir response cache.
	 */
	if (rp->r_dir != NULL)
		nfs_purge_rddir_cache(vp);
}

/*
 * Purge the readdir cache of all entries which are not currently
 * being filled.
 */
void
nfs_purge_rddir_cache(vnode_t *vp)
{
	rnode_t *rp;
	rddir_cache *rdc;
	rddir_cache *prdc;

	rp = VTOR(vp);
top:
	mutex_enter(&rp->r_statelock);
	rdc = rp->r_dir;
	prdc = NULL;
	while (rdc != NULL) {
		if (rdc->flags & RDDIR) {
			prdc = rdc;
			rdc = rdc->next;
			continue;
		}
		/*
		 * It is incorrect to have RDDIR cleared and
		 * RDDIRWAIT set.
		 */
		ASSERT(!(rdc->flags & RDDIRWAIT));
		if (prdc == NULL)
			rp->r_dir = rdc->next;
		else
			prdc->next = rdc->next;
		if (rp->r_direof == rdc)
			rp->r_direof = NULL;
		mutex_exit(&rp->r_statelock);
		if (rdc->entries != NULL)
			kmem_free(rdc->entries, rdc->buflen);
		cv_destroy(&rdc->cv);
#ifdef DEBUG
		rddir_cache_free((void *)rdc, sizeof (*rdc));
#else
		kmem_free(rdc, sizeof (*rdc));
#endif
		goto top;
	}
	mutex_exit(&rp->r_statelock);
}

/*
 * Check the attribute cache to see if the new attributes match
 * those cached.  If they do, the various `data' caches are
 * considered to be good.  Otherwise, the `data' caches are
 * purged.  If the ctime in the attributes has changed, then
 * purge the access cache.
 */
void
nfs_cache_check(vnode_t *vp, timestruc_t ctime, timestruc_t mtime,
	len_t fsize, int *seqp, cred_t *cr)
{
	rnode_t *rp;
	vsecattr_t *vsp;
	int mtime_changed = 0;
	int ctime_changed = 0;

	ASSERT(seqp != NULL);

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	if (!CACHE_VALID(rp, mtime, fsize)) {
		/*
		 * 1256315: if RMODINPROGRESS is set, we were
		 * trying to uiomove stuff in from user space
		 * (see * writerp()) but are here now, likely as
		 * a result of a page fault.  To avoid a watchdog,
		 * we'd like to defer the general cache purge until
		 * the uiomove is complete in writerp, so we set
		 * another flag instead.
		 */
		if (rp->r_flags & RMODINPROGRESS)
			rp->r_flags |= RPURGECACHE;
		else
			mtime_changed = 1;
	}
	if (rp->r_attr.va_ctime.tv_sec != ctime.tv_sec ||
	    rp->r_attr.va_ctime.tv_nsec != ctime.tv_nsec)
		ctime_changed = 1;
	*seqp = rp->r_seq;
	mutex_exit(&rp->r_statelock);

	if (mtime_changed)
		nfs_purge_caches(vp, cr);

	if (ctime_changed) {
		(void) nfs_access_purge_rp(rp);
		if (rp->r_secattr != NULL) {
			mutex_enter(&rp->r_statelock);
			vsp = rp->r_secattr;
			rp->r_secattr = NULL;
			mutex_exit(&rp->r_statelock);
			if (vsp != NULL)
				nfs_acl_free(vsp);
		}
	}
}

/*
 * Do a cache check and possible purge using the wcc_attr.
 * These are the attributes of the file gotten `before'
 * a modify operation on an object.  They are used to
 * determine whether the object was modified by some
 * client other than this one.  Note that the server is
 * counted as a client in this sense.
 */
void
nfs3_cache_check(vnode_t *vp, wcc_attr *wcap, int *seqp, cred_t *cr)
{
	timestruc_t ctime;
	timestruc_t mtime;
	len_t fsize;

	ctime.tv_sec = wcap->ctime.seconds;
	ctime.tv_nsec = wcap->ctime.nseconds;
	mtime.tv_sec = wcap->mtime.seconds;
	mtime.tv_nsec = wcap->mtime.nseconds;
	fsize = wcap->size;

	nfs_cache_check(vp, ctime, mtime, fsize, seqp, cr);
}

/*
 * Check the cache based on the fattr3 attributes.  These are
 * generally the post operation attributes because they are a
 * complete set of the attributes which pass over the network.
 */
void
nfs3_cache_check_fattr3(vnode_t *vp, fattr3 *na, int *seqp, cred_t *cr)
{
	timestruc_t ctime;
	timestruc_t mtime;
	len_t fsize;

	ctime.tv_sec = na->ctime.seconds;
	ctime.tv_nsec = na->ctime.nseconds;
	mtime.tv_sec = na->mtime.seconds;
	mtime.tv_nsec = na->mtime.nseconds;
	fsize = na->size;

	nfs_cache_check(vp, ctime, mtime, fsize, seqp, cr);
}

/*
 * Do a cache check based on the post-operation attributes.
 * Then make them the new cached attributes.  If no attributes
 * were returned, then mark the attributes as timed out.
 */
void
nfs3_cache_post_op_attr(vnode_t *vp, post_op_attr *poap, cred_t *cr)
{
	int seq;

	if (poap->attributes) {
		nfs3_cache_check_fattr3(vp, &poap->attr, &seq, cr);
		nfs3_attrcache(vp, &poap->attr, seq);
	} else {
		PURGE_ATTRCACHE(vp);
	}
}

/*
 * Do a cache check based on the weak cache consistency attributes.
 * These consist of a small set of pre-operation attributes and the
 * full set of post-operation attributes.
 *
 * If we are given the pre-operation attributes, then use them to
 * check the validity of the various caches.  Then, if we got the
 * post-operation attributes, make them the new cached attributes.
 * If we didn't get the post-operation attributes, then mark the
 * attribute cache as timed out so that the next reference will
 * cause a GETATTR to the server to refresh with the current
 * attributes.
 *
 * Otherwise, if we didn't get the pre-operation attributes, but
 * we did get the post-operation attributes, then use these
 * attributes to check the validity of the various caches.  This
 * will probably cause a flush of the caches because if the
 * operation succeeded, the attributes of the object were changed
 * in some way from the old post-operation attributes.  This
 * should be okay because it is the safe thing to do.  After
 * checking the data caches, then we make these the new cached
 * attributes.
 *
 * Otherwise, we didn't get either the pre- or post-operation
 * attributes.  Simply mark the attribute cache as timed out so
 * the next reference will cause a GETATTR to the server to
 * refresh with the current attributes.
 */
void
nfs3_cache_wcc_data(vnode_t *vp, wcc_data *wccp, cred_t *cr)
{
	int seq;

	if (wccp->before.attributes) {
		nfs3_cache_check(vp, &wccp->before.attr, &seq, cr);
		if (wccp->after.attributes)
			nfs3_attrcache(vp, &wccp->after.attr, seq);
		else {
			PURGE_ATTRCACHE(vp);
		}
	} else if (wccp->after.attributes) {
		nfs3_cache_check_fattr3(vp, &wccp->after.attr, &seq, cr);
		nfs3_attrcache(vp, &wccp->after.attr, seq);
	} else {
		PURGE_ATTRCACHE(vp);
	}
}

void
nfs3_check_wcc_data(vnode_t *vp, wcc_data *wccp)
{
	int seq;
	timestruc_t ctime;
	timestruc_t mtime;
	u_offset_t fsize;
	rnode_t *rp;
	int mtime_changed;
	int ctime_changed;
	vsecattr_t *vsp;

	if (!wccp->before.attributes && !wccp->after.attributes) {
		PURGE_ATTRCACHE(vp);
		return;
	}
	if (wccp->before.attributes) {
		ctime.tv_sec = wccp->before.attr.ctime.seconds;
		ctime.tv_nsec = wccp->before.attr.ctime.nseconds;
		mtime.tv_sec = wccp->before.attr.mtime.seconds;
		mtime.tv_nsec = wccp->before.attr.mtime.nseconds;
		fsize = wccp->before.attr.size;
	} else {
		ctime.tv_sec = wccp->after.attr.ctime.seconds;
		ctime.tv_nsec = wccp->after.attr.ctime.nseconds;
		mtime.tv_sec = wccp->after.attr.mtime.seconds;
		mtime.tv_nsec = wccp->after.attr.mtime.nseconds;
		fsize = wccp->after.attr.size;
	}
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	if (!CACHE_VALID(rp, mtime, fsize))
		mtime_changed = 1;
	else
		mtime_changed = 0;
	if (rp->r_attr.va_ctime.tv_sec != ctime.tv_sec ||
	    rp->r_attr.va_ctime.tv_nsec != ctime.tv_nsec)
		ctime_changed = 1;
	else
		ctime_changed = 0;
	seq = rp->r_seq;
	mutex_exit(&rp->r_statelock);
	if (ctime_changed) {
		(void) nfs_access_purge_rp(rp);
		if (rp->r_secattr != NULL) {
			mutex_enter(&rp->r_statelock);
			vsp = rp->r_secattr;
			rp->r_secattr = NULL;
			mutex_exit(&rp->r_statelock);
			if (vsp != NULL)
				nfs_acl_free(vsp);
		}
	}
	if (mtime_changed || !wccp->after.attributes) {
		/*
		 * We purge the cache if post attrs weren't
		 * returned. This will result in a cache purge
		 * (and an attr fetch). But this is the safe
		 * thing to do.
		 */
		PURGE_ATTRCACHE(vp);
	} else
		nfs3_attrcache(vp, &wccp->after.attr, seq);
}

/*
 * Set attributes cache for given vnode using nfsattr.
 */
void
nfs_attrcache(vnode_t *vp, struct nfsfattr *na, int seq)
{
	struct vattr va;

	if (!nattr_to_vattr(vp, na, &va))
		nfs_attrcache_va(vp, &va, seq);
	else
		PURGE_ATTRCACHE(vp);
}

/*
 * Set attributes cache for given vnode using fattr3.
 */
void
nfs3_attrcache(vnode_t *vp, fattr3 *na, int seq)
{
	struct vattr va;

	if (!fattr3_to_vattr(vp, na, &va))
		nfs_attrcache_va(vp, &va, seq);
	else
		PURGE_ATTRCACHE(vp);
}

/*
 * Set attributes cache for given vnode using virtual attributes.
 *
 * Set the timeout value on the attribute cache and fill it
 * with the passed in attributes.
 */
void
nfs_attrcache_va(vnode_t *vp, struct vattr *va, int seq)
{
	rnode_t *rp;
	mntinfo_t *mi;
	int delta;

	rp = VTOR(vp);

	mutex_enter(&rp->r_statelock);
	if (rp->r_seq != seq) {
		mutex_exit(&rp->r_statelock);
		return;
	}

	mi = VTOMI(vp);
	if ((mi->mi_flags & MI_NOAC) || (vp->v_flag & VNOCACHE))
		delta = 0;
	else {
		/*
		 * Delta is the number of seconds that we will cache
		 * attributes of the file.  It is based on the number
		 * of seconds since the last time that we detected a
		 * a change (i.e. files that changed recently are
		 * likely to change soon), but there is a minimum and
		 * a maximum for regular files and for directories.
		 *
		 * Using the time since last change was detected
		 * eliminates the direct comparison or calculation
		 * using mixed client and server times.  NFS does
		 * not make any assumptions regarding the client
		 * and server clocks being synchronized.
		 */
		if (va->va_mtime.tv_sec != rp->r_attr.va_mtime.tv_sec) {
			rp->r_mtime = hrestime.tv_sec;
			delta = 0;
		} else
			delta = hrestime.tv_sec - rp->r_mtime;

		if (vp->v_type == VDIR) {
			if (delta < mi->mi_acdirmin)
				delta = mi->mi_acdirmin;
			else if (delta > mi->mi_acdirmax)
				delta = mi->mi_acdirmax;
		} else {
			if (delta < mi->mi_acregmin)
				delta = mi->mi_acregmin;
			else if (delta > mi->mi_acregmax)
				delta = mi->mi_acregmax;
		}
	}
	rp->r_attrtime = hrestime.tv_sec + delta;
	rp->r_attr = *va;
	rp->r_seq++;
	/*
	 * The real criteria for updating r_size should be if the
	 * file has grown on the server or if the client has not
	 * modified the file.
	 */
	if (rp->r_size != va->va_size &&
	    (rp->r_size < va->va_size ||
	    vp->v_pages == NULL ||
	    (!(rp->r_flags & RDIRTY) && rp->r_count == 0)))
		rp->r_size = va->va_size;
	mutex_exit(&rp->r_statelock);
}

/*
 * Fill in attribute from the cache.
 * If valid, then return 0 to indicate that no error occurred,
 * otherwise return 1 to indicate that an error occurred.
 */
static int
nfs_getattr_cache(vnode_t *vp, struct vattr *vap)
{
	rnode_t *rp;

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	if (ATTRCACHE_VALID(vp)) {
		/*
		 * Cached attributes are valid
		 */
		*vap = rp->r_attr;
		mutex_exit(&rp->r_statelock);
		return (0);
	}
	mutex_exit(&rp->r_statelock);
	return (1);
}

/*
 * Get attributes over-the-wire and update attributes cache
 * if no error occurred in the over-the-wire operation.
 * Return 0 if successful, otherwise error.
 */
#ifdef DEBUG
static int nfs_getattr_otw_misses = 0;
#endif

int
nfs_getattr_otw(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	struct nfsattrstat ns;
	int douprintf;
	mntinfo_t *mi;
	int seq;
	rnode_t *rp;
	failinfo_t fi;

	mi = VTOMI(vp);
	fi.vp = vp;
	fi.fhp = NULL;		/* no need to update, filehandle not copied */
	fi.copyproc = nfscopyfh;
	fi.lookupproc = nfslookup;

	if (mi->mi_flags & MI_ACL) {
		error = acl_getattr2_otw(vp, vap, cr);
		if (mi->mi_flags & MI_ACL)
			return (error);
	}

	rp = VTOR(vp);
doit:
	seq = rp->r_seq;

	douprintf = 1;

	error = rfs2call(mi, RFS_GETATTR,
			xdr_fhandle, (caddr_t)VTOFH(vp),
			xdr_attrstat, (caddr_t)&ns, cr,
			&douprintf, &ns.ns_status, 0, &fi);

	if (!error) {
		error = geterrno(ns.ns_status);
		if (!error) {
			if (rp->r_seq != seq) {
#ifdef DEBUG
				nfs_getattr_otw_misses++;
#endif
				goto doit;
			}
			error = nattr_to_vattr(vp, &ns.ns_attr, vap);
			if (!error) {
				nfs_cache_check(vp, vap->va_ctime,
					vap->va_mtime, vap->va_size, &seq, cr);
				nfs_attrcache_va(vp, vap, seq);
			}
		} else {
			PURGE_STALE_FH(error, vp, cr);
		}
	}

	return (error);
}

/*
 * Return either cached ot remote attributes. If get remote attr
 * use them to check and invalidate caches, then cache the new attributes.
 */
int
nfsgetattr(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	rnode_t *rp;

	/*
	 * If we've got cached attributes, we're done, otherwise go
	 * to the server to get attributes, which will update the cache
	 * in the process.
	 */
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RSERIALIZE) {
		k_sigset_t smask;
		sigintr(&smask, VTOMI(vp)->mi_flags & MI_INT);
		if (!cv_wait_sig(&rp->r_cv, &rp->r_statelock)) {
			sigunintr(&smask);
			mutex_exit(&rp->r_statelock);
			return (EINTR);
		}
		sigunintr(&smask);
	}
	rp->r_flags |= RSERIALIZE;
	mutex_exit(&rp->r_statelock);

	error = nfs_getattr_cache(vp, vap);
	if (error) {
		error = nfs_getattr_otw(vp, vap, cr);
	}

	mutex_enter(&rp->r_statelock);
	/* Return the client's view of file size */
	vap->va_size = rp->r_size;
	rp->r_flags &= ~RSERIALIZE;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);

	return (error);
}

/*
 * Get attributes over-the-wire and update attributes cache
 * if no error occurred in the over-the-wire operation.
 * Return 0 if successful, otherwise error.
 */
#ifdef DEBUG
static int nfs3_getattr_otw_misses = 0;
#endif
int
nfs3_getattr_otw(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	GETATTR3args args;
	GETATTR3res res;
	int douprintf;
	int seq;
	rnode_t *rp;
	failinfo_t fi;

	args.object = *VTOFH3(vp);
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.object;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	rp = VTOR(vp);
doit:
	seq = rp->r_seq;

	douprintf = 1;

	error = rfs3call(VTOMI(vp), NFSPROC3_GETATTR,
	    xdr_nfs_fh3, (caddr_t)&args,
	    xdr_GETATTR3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, &fi);

	if (!error) {
		error = geterrno3(res.status);
		if (!error) {
			if (rp->r_seq != seq) {
#ifdef DEBUG
				nfs3_getattr_otw_misses++;
#endif
				goto doit;
			}
			error = fattr3_to_vattr(vp, &res.resok.obj_attributes,
					vap);
			if (!error) {
				nfs_cache_check(vp, vap->va_ctime,
					vap->va_mtime, vap->va_size, &seq, cr);
				nfs_attrcache_va(vp, vap, seq);
			}
		} else {
			PURGE_STALE_FH(error, vp, cr);
		}
	}

	return (error);
}

/*
 * Return either cached or remote attributes. If get remote attr
 * use them to check and invalidate caches, then cache the new attributes.
 */
int
nfs3getattr(vnode_t *vp, struct vattr *vap, cred_t *cr)
{
	int error;
	rnode_t *rp;

	/*
	 * If we've got cached attributes, we're done, otherwise go
	 * to the server to get attributes, which will update the cache
	 * in the process.
	 */
	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RSERIALIZE) {
		k_sigset_t smask;
		sigintr(&smask, VTOMI(vp)->mi_flags & MI_INT);
		if (!cv_wait_sig(&rp->r_cv, &rp->r_statelock)) {
			sigunintr(&smask);
			mutex_exit(&rp->r_statelock);
			return (EINTR);
		}
		sigunintr(&smask);
	}
	rp->r_flags |= RSERIALIZE;
	mutex_exit(&rp->r_statelock);

	error = nfs_getattr_cache(vp, vap);
	if (error)
		error = nfs3_getattr_otw(vp, vap, cr);

	mutex_enter(&rp->r_statelock);
	/* Return the client's view of file size */
	vap->va_size = rp->r_size;
	rp->r_flags &= ~RSERIALIZE;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);

	return (error);
}

vtype_t nf_to_vt[] = {
	VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK
};
/*
 * Convert NFS Version 2 over the network attributes to the local
 * virtual attributes.  The mapping between the UID_NOBODY/GID_NOBODY
 * network representation and the local representation is done here.
 * Returns 0 for success, error if failed due to overflow.
 */
int
nattr_to_vattr(vnode_t *vp, struct nfsfattr *na, struct vattr *vap)
{
	/* overflow in time attributes? */
	IF_NOT_NFS_TIME_OK(NFS2_FATTR_TIME_OK(na),
		return (EOVERFLOW));

	if (na->na_type < NFNON || na->na_type > NFSOC)
		vap->va_type = VBAD;
	else
		vap->va_type = nf_to_vt[na->na_type];
	vap->va_mode = na->na_mode;
	vap->va_uid = (na->na_uid == NFS_UID_NOBODY) ? UID_NOBODY : na->na_uid;
	vap->va_gid = (na->na_gid == NFS_GID_NOBODY) ? GID_NOBODY : na->na_gid;
	vap->va_fsid = vp->v_vfsp->vfs_dev;
	vap->va_nodeid = na->na_nodeid;
	vap->va_nlink = na->na_nlink;
	vap->va_size = na->na_size;	/* keep for cache validation */
	/*
	 * nfs protocol defines times as unsigned so don't extend sign,
	 * unless sysadmin set nfs_allow_preepoch_time.
	 */
	NFS_TIME_T_CONVERT(vap->va_atime.tv_sec, na->na_atime.tv_sec);
	vap->va_atime.tv_nsec = (uint32_t)(na->na_atime.tv_usec * 1000);
	NFS_TIME_T_CONVERT(vap->va_mtime.tv_sec, na->na_mtime.tv_sec);
	vap->va_mtime.tv_nsec = (uint32_t)(na->na_mtime.tv_usec * 1000);
	NFS_TIME_T_CONVERT(vap->va_ctime.tv_sec, na->na_ctime.tv_sec);
	vap->va_ctime.tv_nsec = (uint32_t)(na->na_ctime.tv_usec * 1000);
	/*
	 * Shannon's law - uncompress the received dev_t
	 * if the top half of is zero indicating a response
	 * from an `older style' OS. Except for when it is a
	 * `new style' OS sending the maj device of zero,
	 * in which case the algorithm still works because the
	 * fact that it is a new style server
	 * is hidden by the minor device not being greater
	 * than 255 (a requirement in this case).
	 */
	if ((na->na_rdev & 0xffff0000) == 0)
		vap->va_rdev = nfsv2_expdev(na->na_rdev);
	else
		vap->va_rdev = expldev(na->na_rdev);

	vap->va_nblocks = na->na_blocks;
	switch (na->na_type) {
	case NFBLK:
		vap->va_blksize = DEV_BSIZE;
		break;

	case NFCHR:
		vap->va_blksize = MAXBSIZE;
		break;

	case NFSOC:
	default:
		vap->va_blksize = na->na_blocksize;
		break;
	}
	/*
	 * This bit of ugliness is a hack to preserve the
	 * over-the-wire protocols for named-pipe vnodes.
	 * It remaps the special over-the-wire type to the
	 * VFIFO type. (see note in nfs.h)
	 */
	if (NA_ISFIFO(na)) {
		vap->va_type = VFIFO;
		vap->va_mode = (vap->va_mode & ~S_IFMT) | S_IFIFO;
		vap->va_rdev = 0;
		vap->va_blksize = na->na_blocksize;
	}
	vap->va_vcode = 0;
	return (0);
}

/*
 * Convert NFS Version 3 over the network attributes to the local
 * virtual attributes.  The mapping between the UID_NOBODY/GID_NOBODY
 * network representation and the local representation is done here.
 */
vtype_t nf3_to_vt[] = {
	VBAD, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO
};

int
fattr3_to_vattr(vnode_t *vp, fattr3 *na, struct vattr *vap)
{
	/* overflow in time attributes? */
	IF_NOT_NFS_TIME_OK(NFS3_FATTR_TIME_OK(na),
		return (EOVERFLOW));
	if (!NFS3_SIZE_OK(na->size))
		/* file too big */
		return (EFBIG);

	if (na->type < NF3REG || na->type > NF3FIFO)
		vap->va_type = VBAD;
	else
		vap->va_type = nf3_to_vt[na->type];
	vap->va_mode = na->mode;
	vap->va_uid = (na->uid == NFS_UID_NOBODY) ? UID_NOBODY : (uid_t)na->uid;
	vap->va_gid = (na->gid == NFS_GID_NOBODY) ? GID_NOBODY : (gid_t)na->gid;
	vap->va_fsid = vp->v_vfsp->vfs_dev;
	vap->va_nodeid = na->fileid;
	vap->va_nlink = na->nlink;
	vap->va_size = na->size;

	/*
	 * nfs protocol defines times as unsigned so don't extend sign,
	 * unless sysadmin set nfs_allow_preepoch_time.
	 */
	NFS_TIME_T_CONVERT(vap->va_atime.tv_sec, na->atime.seconds);
	vap->va_atime.tv_nsec = (uint32_t)na->atime.nseconds;
	NFS_TIME_T_CONVERT(vap->va_mtime.tv_sec, na->mtime.seconds);
	vap->va_mtime.tv_nsec = (uint32_t)na->mtime.nseconds;
	NFS_TIME_T_CONVERT(vap->va_ctime.tv_sec, na->ctime.seconds);
	vap->va_ctime.tv_nsec = (uint32_t)na->ctime.nseconds;

	switch (na->type) {
	case NF3BLK:
		vap->va_rdev = makedevice(na->rdev.specdata1,
					na->rdev.specdata2);
		vap->va_blksize = DEV_BSIZE;
		vap->va_nblocks = 0;
		break;
	case NF3CHR:
		vap->va_rdev = makedevice(na->rdev.specdata1,
					na->rdev.specdata2);
		vap->va_blksize = MAXBSIZE;
		vap->va_nblocks = 0;
		break;
	case NF3REG:
	case NF3DIR:
	case NF3LNK:
		vap->va_rdev = 0;
		vap->va_blksize = MAXBSIZE;
		vap->va_nblocks = (u_longlong_t)
		    ((na->used + (size3)DEV_BSIZE - (size3)1) /
		    (size3)DEV_BSIZE);
		break;
	case NF3SOCK:
	case NF3FIFO:
	default:
		vap->va_rdev = 0;
		vap->va_blksize = MAXBSIZE;
		vap->va_nblocks = 0;
		break;
	}
	vap->va_vcode = 0;
	return (0);
}

/*
 * Asynchronous I/O parameters.  nfs_async_threads is the high-water mark
 * for the demand-based allocation of async threads per-mount.  The
 * nfs_async_timeout is the amount of time a thread will live after it
 * becomes idle, unless new I/O requests are received before the thread
 * dies.  See nfs_async_putpage and nfs_async_start.
 */

#define	NFS_ASYNC_TIMEOUT	(60 * 1 * hz)	/* 1 minute */

int nfs_async_timeout = -1;	/* uninitialized */

int nfs_maxasynccount = 100;	/* max number of async reqs */

static void	nfs_async_start(struct vfs *);

void
nfs_async_readahead(vnode_t *vp, u_offset_t blkoff, caddr_t addr,
	struct seg *seg, cred_t *cr, void (*readahead)(vnode_t *,
	u_offset_t, caddr_t, struct seg *, cred_t *))
{
	rnode_t *rp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;

	rp = VTOR(vp);
	ASSERT(rp->r_freef == NULL);

	mi = VTOMI(vp);

	/*
	 * If memory on the system is starting to get tight, don't
	 * do readahead because this will just use memory that we
	 * don't want to tie up.  In any case, queue to at least
	 * nfs_maxasynccount requests.
	 */
	if (mi->mi_async_count > nfs_maxasynccount &&
	    freemem < lotsfree + pages_before_pager)
		return;

	/*
	 * If addr falls in a different segment, don't bother doing readahead.
	 */
	if (addr >= seg->s_base + seg->s_size)
		return;

	/*
	 * If we can't allocate a request structure, punt on the readahead.
	 */
	if ((args = kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL)
		return;

	args->a_next = NULL;
#ifdef DEBUG
	args->a_queuer = curthread;
#endif
	VN_HOLD(vp);

	/*
	 * If a lock operation is pending, don't initiate any new
	 * readaheads.  Otherwise, bump r_count to indicate the new
	 * asynchronous I/O.
	 */

	if (!nfs_rw_tryenter(&rp->r_lkserlock, RW_READER)) {
		VN_RELE(vp);
		kmem_free(args, sizeof (*args));
		return;
	}
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	mutex_exit(&rp->r_statelock);
	nfs_rw_exit(&rp->r_lkserlock);

	args->a_vp = vp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_READ_AHEAD;
	args->a_nfs_readahead = readahead;
	args->a_nfs_blkoff = blkoff;
	args->a_nfs_seg = seg;
	args->a_nfs_addr = addr;

	mutex_enter(&mi->mi_async_lock);

	/*
	 * If asyncio has been disabled, don't bother readahead.
	 */
	if (mi->mi_max_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		goto noasync;
	}

	/*
	 * Check if we should create an async thread.  If we can't and
	 * there aren't any threads already running, punt on the readahead.
	 */
	if (mi->mi_threads < mi->mi_max_threads) {
		if (thread_create(NULL, NULL, nfs_async_start,
		    (caddr_t)vp->v_vfsp, 0, &p0, TS_RUN, 60) == NULL) {
			if (mi->mi_threads == 0) {
				mutex_exit(&mi->mi_async_lock);
				goto noasync;
			}
		} else
			mi->mi_threads++;
	}

	/*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs[NFS_READ_AHEAD] == NULL) {
		mi->mi_async_reqs[NFS_READ_AHEAD] = args;
		mi->mi_async_tail[NFS_READ_AHEAD] = args;
	} else {
		mi->mi_async_tail[NFS_READ_AHEAD]->a_next = args;
		mi->mi_async_tail[NFS_READ_AHEAD] = args;
	}
	mi->mi_async_count++;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	cv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return;

noasync:
	mutex_enter(&rp->r_statelock);
	rp->r_count--;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);
	VN_RELE(vp);
	crfree(cr);
	kmem_free(args, sizeof (*args));
}

int
nfs_async_putapage(vnode_t *vp, page_t *pp, u_offset_t off, size_t len,
	int flags, cred_t *cr, int (*putapage)(vnode_t *, page_t *,
	u_offset_t, size_t, int, cred_t *))
{
	rnode_t *rp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;

	ASSERT(flags & B_ASYNC);
	ASSERT(vp->v_vfsp != NULL);

	rp = VTOR(vp);
	ASSERT(rp->r_count > 0);

	mi = VTOMI(vp);

	/*
	 * If memory on the system is starting to get tight, do
	 * the request synchronously to reduce the requirements
	 * to process the request and to get the request done as
	 * quickly as possible.  In any case, queue to at least
	 * nfs_maxasynccount requests.
	 */
	if (mi->mi_async_count > nfs_maxasynccount &&
	    freemem < lotsfree + pages_before_pager) {
		args = NULL;
		goto noasync;
	}

	/*
	 * If we can't allocate a request structure, do the putpage
	 * operation synchronously in this thread's context.
	 */
	if ((args = kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL)
		goto noasync;

	args->a_next = NULL;
#ifdef DEBUG
	args->a_queuer = curthread;
#endif
	VN_HOLD(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	rp->r_awcount++;
	mutex_exit(&rp->r_statelock);
	args->a_vp = vp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_PUTAPAGE;
	args->a_nfs_putapage = putapage;
	args->a_nfs_pp = pp;
	args->a_nfs_off = off;
	args->a_nfs_len = (uint_t)len;
	args->a_nfs_flags = flags;

	mutex_enter(&mi->mi_async_lock);

	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (mi->mi_max_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		goto noasync;
	}

	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the i/o
	 * synchronously.
	 */
	if (mi->mi_threads < mi->mi_max_threads) {
		if (thread_create(NULL, NULL, nfs_async_start,
		    (caddr_t)vp->v_vfsp, 0, &p0, TS_RUN, 60) == NULL) {
			if (mi->mi_threads == 0) {
				mutex_exit(&mi->mi_async_lock);
				goto noasync;
			}
		} else
			mi->mi_threads++;
	}

	/*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs[NFS_PUTAPAGE] == NULL) {
		mi->mi_async_reqs[NFS_PUTAPAGE] = args;
		mi->mi_async_tail[NFS_PUTAPAGE] = args;
	} else {
		mi->mi_async_tail[NFS_PUTAPAGE]->a_next = args;
		mi->mi_async_tail[NFS_PUTAPAGE] = args;
	}
	mi->mi_async_count++;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	cv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return (0);

noasync:
	if (args != NULL) {
		mutex_enter(&rp->r_statelock);
		rp->r_count--;
		rp->r_awcount--;
		cv_broadcast(&rp->r_cv);
		mutex_exit(&rp->r_statelock);
		VN_RELE(vp);
		crfree(cr);
		kmem_free(args, sizeof (*args));
	}
	/*
	 * If we get here in the context of the pageout thread,
	 * we refuse to do a sync write, because this may hang
	 * pageout (and the machine). In this case, we just
	 * re-mark the page as dirty and punt on the page.
	 */
	if (curproc == proc_pageout) {
		/*
		 * Make sure B_FORCE isn't set.  We can re-mark the
		 * pages as dirty and unlock the pages in one swoop by
		 * passing in B_ERROR to pvn_write_done().  However,
		 * we should make sure B_FORCE isn't set - we don't
		 * want the page tossed before it gets written out.
		 */
		if (flags & B_FORCE)
			flags &= ~(B_INVAL | B_FORCE);
		pvn_write_done(pp, flags | B_ERROR);
		return (0);
	}
	return ((*putapage)(vp, pp, off, len, flags, cr));
}

int
nfs_async_pageio(vnode_t *vp, page_t *pp, u_offset_t io_off, size_t io_len,
	int flags, cred_t *cr, int (*pageio)(vnode_t *, page_t *, u_offset_t,
	size_t, int, cred_t *))
{
	rnode_t *rp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;

	ASSERT(flags & B_ASYNC);
	ASSERT(vp->v_vfsp != NULL);

	rp = VTOR(vp);
	ASSERT(rp->r_count > 0);

	mi = VTOMI(vp);

	/*
	 * If memory on the system is starting to get tight, do
	 * the request synchronously to reduce the requirements
	 * to process the request and to get the request done as
	 * quickly as possible.  In any case, queue to at least
	 * nfs_maxasynccount requests.
	 */
	if (mi->mi_async_count > nfs_maxasynccount &&
	    freemem < lotsfree + pages_before_pager) {
		args = NULL;
		goto noasync;
	}

	/* . . . or if we can't allocate a request structure . . . */
	if ((args = kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL)
		goto noasync;

	args->a_next = NULL;
#ifdef DEBUG
	args->a_queuer = curthread;
#endif
	VN_HOLD(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	rp->r_awcount++;
	mutex_exit(&rp->r_statelock);
	args->a_vp = vp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_PAGEIO;
	args->a_nfs_pageio = pageio;
	args->a_nfs_pp = pp;
	args->a_nfs_off = io_off;
	args->a_nfs_len = (uint_t)io_len;
	args->a_nfs_flags = flags;

	mutex_enter(&mi->mi_async_lock);

	/* if asyncio has been disabled . . . */
	if (mi->mi_max_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		goto noasync;
	}

	/* . . . or if we should but can't create a thread. */
	if (mi->mi_threads < mi->mi_max_threads) {
		if (thread_create(NULL, NULL, nfs_async_start,
		    (caddr_t)vp->v_vfsp, 0, &p0, TS_RUN, 60) == NULL) {
			if (mi->mi_threads == 0) {
				mutex_exit(&mi->mi_async_lock);
				goto noasync;
			}
		} else
			mi->mi_threads++;
	}

	/*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs[NFS_PAGEIO] == NULL) {
		mi->mi_async_reqs[NFS_PAGEIO] = args;
		mi->mi_async_tail[NFS_PAGEIO] = args;
	} else {
		mi->mi_async_tail[NFS_PAGEIO]->a_next = args;
		mi->mi_async_tail[NFS_PAGEIO] = args;
	}
	mi->mi_async_count++;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	cv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return (0);

noasync:
	if (args != NULL) {
		mutex_enter(&rp->r_statelock);
		rp->r_count--;
		rp->r_awcount--;
		cv_broadcast(&rp->r_cv);
		mutex_exit(&rp->r_statelock);
		VN_RELE(vp);
		crfree(cr);
		kmem_free(args, sizeof (*args));
	}

	/*
	 * If we can't do it ASYNC, for reads we do nothing (but cleanup
	 * the page list), for writes we do it synchronously, except for
	 * proc_pageout as described below.
	 */
	if (flags & B_READ) {
		pvn_read_done(pp, flags | B_ERROR);
		return (0);
	}

	/*
	 * If we get here in the context of the pageout thread,
	 * we refuse to do a sync write, because this may hang
	 * pageout (and the machine). In this case, we just
	 * re-mark the page as dirty and punt on the page.
	 */
	if (curproc == proc_pageout) {
		/*
		 * Make sure B_FORCE isn't set.  We can re-mark the
		 * pages as dirty and unlock the pages in one swoop by
		 * passing in B_ERROR to pvn_write_done().  However,
		 * we should make sure B_FORCE isn't set - we don't
		 * want the page tossed before it gets written out.
		 */
		if (flags & B_FORCE)
			flags &= ~(B_INVAL | B_FORCE);
		pvn_write_done(pp, flags | B_ERROR);
		return (0);
	}

	return ((*pageio)(vp, pp, io_off, io_len, flags, cr));
}

void
nfs_async_readdir(vnode_t *vp, rddir_cache *rdc, cred_t *cr,
	int (*readdir)(vnode_t *, rddir_cache *, cred_t *))
{
	rnode_t *rp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;

	rp = VTOR(vp);
	ASSERT(rp->r_freef == NULL);

	mi = VTOMI(vp);

	/*
	 * If memory on the system is starting to get tight, do
	 * the request synchronously to reduce the requirements
	 * to process the request and to get the request done as
	 * quickly as possible.  In any case, queue to at least
	 * nfs_maxasynccount requests.
	 */
	if (mi->mi_async_count > nfs_maxasynccount &&
	    freemem < lotsfree + pages_before_pager) {
		args = NULL;
		goto noasync;
	}

	/*
	 * If we can't allocate a request structure, do the readdir
	 * operation synchronously in this thread's context.
	 */
	if ((args = kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL)
		goto noasync;

	args->a_next = NULL;
#ifdef DEBUG
	args->a_queuer = curthread;
#endif
	VN_HOLD(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	mutex_exit(&rp->r_statelock);
	args->a_vp = vp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_READDIR;
	args->a_nfs_readdir = readdir;
	args->a_nfs_rdc = rdc;

	mutex_enter(&mi->mi_async_lock);

	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (mi->mi_max_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		goto noasync;
	}

	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the
	 * readdir synchronously.
	 */
	if (mi->mi_threads < mi->mi_max_threads) {
		if (thread_create(NULL, NULL, nfs_async_start,
		    (caddr_t)vp->v_vfsp, 0, &p0, TS_RUN, 60) == NULL) {
			if (mi->mi_threads == 0) {
				mutex_exit(&mi->mi_async_lock);
				goto noasync;
			}
		} else
			mi->mi_threads++;
	}

	/*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs[NFS_READDIR] == NULL) {
		mi->mi_async_reqs[NFS_READDIR] = args;
		mi->mi_async_tail[NFS_READDIR] = args;
	} else {
		mi->mi_async_tail[NFS_READDIR]->a_next = args;
		mi->mi_async_tail[NFS_READDIR] = args;
	}
	mi->mi_async_count++;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	cv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return;

noasync:
	if (args != NULL) {
		mutex_enter(&rp->r_statelock);
		rp->r_count--;
		cv_broadcast(&rp->r_cv);
		mutex_exit(&rp->r_statelock);
		VN_RELE(vp);
		crfree(cr);
		kmem_free(args, sizeof (*args));
	}

	rdc->entries = NULL;
	mutex_enter(&rp->r_statelock);
	ASSERT(rdc->flags & RDDIR);
	rdc->flags &= ~RDDIR;
	rdc->flags |= RDDIRREQ;
	/*
	 * Check the flag to see if RDDIRWAIT is set. If RDDIRWAIT
	 * is set, wakeup the thread sleeping in cv_wait_sig().
	 * The woken up thread will reset the flag to RDDIR and will
	 * continue with the readdir opeartion. This wil also ensure
	 * that the thread in purge_rddir_cache() will not try to
	 * purge the cached entry.
	 */
	if (rdc->flags & RDDIRWAIT) {
		rdc->flags &= ~RDDIRWAIT;
		cv_broadcast(&rdc->cv);
	}
	mutex_exit(&rp->r_statelock);
}

void
nfs_async_commit(vnode_t *vp, page_t *plist, offset3 offset, count3 count,
	cred_t *cr, void (*commit)(vnode_t *, page_t *, offset3, count3,
	cred_t *))
{
	rnode_t *rp;
	mntinfo_t *mi;
	struct nfs_async_reqs *args;
	page_t *pp;

	rp = VTOR(vp);
	mi = VTOMI(vp);

	/*
	 * If memory on the system is starting to get tight, do
	 * the request synchronously to reduce the requirements
	 * to process the request and to get the request done as
	 * quickly as possible.  In any case, queue to at least
	 * nfs_maxasynccount requests.
	 */
	if (mi->mi_async_count > nfs_maxasynccount &&
	    freemem < lotsfree + pages_before_pager) {
		args = NULL;
		goto noasync;
	}

	/*
	 * If we can't allocate a request structure, do the commit
	 * operation synchronously in this thread's context.
	 */
	if ((args = kmem_alloc(sizeof (*args), KM_NOSLEEP)) == NULL)
		goto noasync;

	args->a_next = NULL;
#ifdef DEBUG
	args->a_queuer = curthread;
#endif
	VN_HOLD(vp);
	mutex_enter(&rp->r_statelock);
	rp->r_count++;
	mutex_exit(&rp->r_statelock);
	args->a_vp = vp;
	ASSERT(cr != NULL);
	crhold(cr);
	args->a_cred = cr;
	args->a_io = NFS_COMMIT;
	args->a_nfs_commit = commit;
	args->a_nfs_plist = plist;
	args->a_nfs_offset = offset;
	args->a_nfs_count = count;

	mutex_enter(&mi->mi_async_lock);

	/*
	 * If asyncio has been disabled, then make a synchronous request.
	 */
	if (mi->mi_max_threads == 0) {
		mutex_exit(&mi->mi_async_lock);
		goto noasync;
	}

	/*
	 * Check if we should create an async thread.  If we can't
	 * and there aren't any threads already running, do the
	 * readdir synchronously.
	 */
	if (mi->mi_threads < mi->mi_max_threads) {
		if (thread_create(NULL, NULL, nfs_async_start,
		    (caddr_t)vp->v_vfsp, 0, &p0, TS_RUN, 60) == NULL) {
			if (mi->mi_threads == 0) {
				mutex_exit(&mi->mi_async_lock);
				goto noasync;
			}
		} else
			mi->mi_threads++;
	}

	/*
	 * Link request structure into the async list and
	 * wakeup async thread to do the i/o.
	 */
	if (mi->mi_async_reqs[NFS_COMMIT] == NULL) {
		mi->mi_async_reqs[NFS_COMMIT] = args;
		mi->mi_async_tail[NFS_COMMIT] = args;
	} else {
		mi->mi_async_tail[NFS_COMMIT]->a_next = args;
		mi->mi_async_tail[NFS_COMMIT] = args;
	}
	mi->mi_async_count++;

	if (mi->mi_io_kstats) {
		mutex_enter(&mi->mi_lock);
		kstat_waitq_enter(KSTAT_IO_PTR(mi->mi_io_kstats));
		mutex_exit(&mi->mi_lock);
	}

	cv_signal(&mi->mi_async_reqs_cv);
	mutex_exit(&mi->mi_async_lock);
	return;

noasync:
	if (args != NULL) {
		mutex_enter(&rp->r_statelock);
		rp->r_count--;
		cv_broadcast(&rp->r_cv);
		mutex_exit(&rp->r_statelock);
		VN_RELE(vp);
		crfree(cr);
		kmem_free(args, sizeof (*args));
	}

	if (curproc == proc_pageout) {
		while (plist != NULL) {
			pp = plist;
			page_sub(&plist, pp);
			pp->p_fsdata = C_COMMIT;
			page_unlock(pp);
		}
		return;
	}
	(*commit)(vp, plist, offset, count, cr);
}

/*
 * The async queues for each mounted file system are arranged as a
 * set of queues, one for each async i/o type.  Requests are taken
 * from the queues in a round-robin fashion.  A number of consecutive
 * requests are taken from each queue before moving on to the next
 * queue.  This functionality may allow the NFS Version 2 server to do
 * write clustering, even if the client is mixing writes and reads
 * because it will take multiple write requests from the queue
 * before processing any of the other async i/o types.
 *
 * XXX The nfs_async_start thread is unsafe in the light of the present
 * model defined by cpr to suspend the system. Specifically over the
 * wire calls are cpr-unsafe. The thread should be reevaluated in
 * case of future updates to the cpr model.
 */
static void
nfs_async_start(struct vfs *vfsp)
{
	struct nfs_async_reqs *args;
	mntinfo_t *mi = VFTOMI(vfsp);
	clock_t time_left = 1;
	callb_cpr_t cprinfo;
	rnode_t *rp;
	int i;

	VFS_HOLD(vfsp);
	/*
	 * Dynamic initialization of nfs_async_timeout to allow nfs to be
	 * built in an implementation independent manner.
	 */
	if (nfs_async_timeout == -1)
		nfs_async_timeout = NFS_ASYNC_TIMEOUT;

	CALLB_CPR_INIT(&cprinfo, &mi->mi_async_lock, callb_generic_cpr, "nas");

	mutex_enter(&mi->mi_async_lock);
	for (;;) {
		/*
		 * Find the next queue containing an entry.  We start
		 * at the current queue pointer and then round robin
		 * through all of them until we either find a non-empty
		 * queue or have looked through all of them.
		 */
		for (i = 0; i < NFS_ASYNC_TYPES; i++) {
			args = *mi->mi_async_curr;
			if (args != NULL)
				break;
			mi->mi_async_curr++;
			if (mi->mi_async_curr ==
			    &mi->mi_async_reqs[NFS_ASYNC_TYPES])
				mi->mi_async_curr = &mi->mi_async_reqs[0];
		}
		/*
		 * If we didn't find a entry, then block until woken up
		 * again and then look through the queues again.
		 */
		if (args == NULL) {
			/*
			 * Exiting is considered to be safe for CPR as well
			 */
			CALLB_CPR_SAFE_BEGIN(&cprinfo);

			/*
			 * Wakeup thread waiting to unmount the file
			 * system only if all async threads are inactive.
			 *
			 * If we've timed-out and there's nothing to do,
			 * then get rid of this thread.
			 */
			if (mi->mi_max_threads == 0 || time_left <= 0) {
				if (--mi->mi_threads == 0)
					cv_signal(&mi->mi_async_cv);
				CALLB_CPR_EXIT(&cprinfo);
				VFS_RELE(vfsp);
				thread_exit();
				/* NOTREACHED */
			}
			time_left = cv_timedwait(&mi->mi_async_reqs_cv,
			    &mi->mi_async_lock, nfs_async_timeout + lbolt);

			CALLB_CPR_SAFE_END(&cprinfo, &mi->mi_async_lock);

			continue;
		}
		/*
		 * Remove the request from the async queue and then
		 * update the current async request queue pointer.  If
		 * the current queue is empty or we have removed enough
		 * consecutive entries from it, then reset the counter
		 * for this queue and then move the current pointer to
		 * the next queue.
		 */
		*mi->mi_async_curr = args->a_next;
		if (*mi->mi_async_curr == NULL ||
		    --mi->mi_async_clusters[args->a_io] == 0) {
			mi->mi_async_clusters[args->a_io] =
						mi->mi_async_init_clusters;
			mi->mi_async_curr++;
			if (mi->mi_async_curr ==
			    &mi->mi_async_reqs[NFS_ASYNC_TYPES])
				mi->mi_async_curr = &mi->mi_async_reqs[0];
		}
		mi->mi_async_count--;

		if (mi->mi_io_kstats) {
			mutex_enter(&mi->mi_lock);
			kstat_waitq_exit(KSTAT_IO_PTR(mi->mi_io_kstats));
			mutex_exit(&mi->mi_lock);
		}

		mutex_exit(&mi->mi_async_lock);

		/*
		 * Obtain arguments from the async request structure.
		 */
		if (args->a_io == NFS_READ_AHEAD && mi->mi_max_threads > 0) {
			(*args->a_nfs_readahead)(args->a_vp, args->a_nfs_blkoff,
					args->a_nfs_addr, args->a_nfs_seg,
					args->a_cred);
		} else if (args->a_io == NFS_PUTAPAGE) {
			(void) (*args->a_nfs_putapage)(args->a_vp,
					args->a_nfs_pp, args->a_nfs_off,
					args->a_nfs_len, args->a_nfs_flags,
					args->a_cred);
		} else if (args->a_io == NFS_PAGEIO) {
			(void) (*args->a_nfs_pageio)(args->a_vp,
					args->a_nfs_pp, args->a_nfs_off,
					args->a_nfs_len, args->a_nfs_flags,
					args->a_cred);
		} else if (args->a_io == NFS_READDIR) {
			(void) ((*args->a_nfs_readdir)(args->a_vp,
					args->a_nfs_rdc, args->a_cred));
		} else if (args->a_io == NFS_COMMIT) {
			(*args->a_nfs_commit)(args->a_vp, args->a_nfs_plist,
					args->a_nfs_offset, args->a_nfs_count,
					args->a_cred);
		}

		/*
		 * Now, release the vnode and free the credentials
		 * structure.
		 */
		rp = VTOR(args->a_vp);
		mutex_enter(&rp->r_statelock);
		rp->r_count--;
		if (args->a_io == NFS_PUTAPAGE || args->a_io == NFS_PAGEIO)
			rp->r_awcount--;
		cv_broadcast(&rp->r_cv);
		mutex_exit(&rp->r_statelock);
		VN_RELE(args->a_vp);
		crfree(args->a_cred);
		kmem_free(args, sizeof (*args));

		/*
		 * Reacquire the mutex because it will be needed above.
		 */
		mutex_enter(&mi->mi_async_lock);
	}
}

void
nfs_async_stop(struct vfs *vfsp)
{
	mntinfo_t *mi = VFTOMI(vfsp);

	/*
	 * Wait for all outstanding putpage operations to complete.
	 */
	mutex_enter(&mi->mi_async_lock);
	mi->mi_max_threads = 0;
	cv_broadcast(&mi->mi_async_reqs_cv);
	while (mi->mi_threads != 0)
		cv_wait(&mi->mi_async_cv, &mi->mi_async_lock);
	mutex_exit(&mi->mi_async_lock);
}

/*
 * nfs_async_stop_sig:
 * Wait for all outstanding putpage operation to complete. If a signal
 * is deliver we will abort and return non-zero. If we can put all the
 * pages we will return 0. This routine is called from nfs_unmount and
 * nfs3_unmount to make these operations interruptable.
 */
int
nfs_async_stop_sig(struct vfs *vfsp)
{
	mntinfo_t *mi = VFTOMI(vfsp);
	ushort_t omax;
	int rval;

	/*
	 * Wait for all outstanding putpage operations to complete.
	 */
	mutex_enter(&mi->mi_async_lock);
	omax = mi->mi_max_threads;
	mi->mi_max_threads = 0;
	cv_broadcast(&mi->mi_async_reqs_cv);
	while (mi->mi_threads != 0)
		if (!cv_wait_sig(&mi->mi_async_cv, &mi->mi_async_lock))
		    break;
	rval = mi->mi_threads != 0; /* Interrupted */
	if (rval)
	    mi->mi_max_threads = omax;
	mutex_exit(&mi->mi_async_lock);

	return (rval);
}

int
writerp(rnode_t *rp, caddr_t base, int tcount, struct uio *uio)
{
	int pagecreate;
	int n;
	int saved_n;
	caddr_t saved_base;
	u_offset_t offset;
	int error;
	int sm_error;
	int purge;

	ASSERT(tcount <= MAXBSIZE && tcount <= uio->uio_resid);
	ASSERT(((uintptr_t)base & MAXBOFFSET) + tcount <= MAXBSIZE);
	ASSERT(nfs_rw_lock_held(&rp->r_rwlock, RW_WRITER));

	/*
	 * Move bytes in at most PAGESIZE chunks. We must avoid
	 * spanning pages in uiomove() because page faults may cause
	 * the cache to be invalidated out from under us. The r_size is not
	 * updated until after the uiomove. If we push the last page of a
	 * file before r_size is correct, we will lose the data written past
	 * the current (and invalid) r_size.
	 */
	do {
		offset = uio->uio_loffset;
		pagecreate = 0;

		/*
		 * n is the number of bytes required to satisfy the request
		 *   or the number of bytes to fill out the page.
		 */
		n = (int)MIN((PAGESIZE - ((uintptr_t)base & PAGEOFFSET)),
		    tcount);

		/*
		 * Check to see if we can skip reading in the page
		 * and just allocate the memory.  We can do this
		 * if we are going to rewrite the entire mapping
		 * or if we are going to write to or beyond the current
		 * end of file from the beginning of the mapping.
		 *
		 * The read of r_size is now protected by r_statelock.
		 */
		mutex_enter(&rp->r_statelock);
		if (((uintptr_t)base & PAGEOFFSET) == 0 && (n == PAGESIZE ||
		    ((offset + n) >= rp->r_size))) {
			/*
			 * The last argument tells segmap_pagecreate() to
			 * always lock the page, as opposed to sometimes
			 * returning with the page locked. This way we avoid a
			 * fault on the ensuing uiomove(), but also
			 * more importantly (to fix bug 1094402) we can
			 * call segmap_fault() to unlock the page in all
			 * cases. An alternative would be to modify
			 * segmap_pagecreate() to tell us when it is
			 * locking a page, but that's a fairly major
			 * interface change.
			 */
			mutex_exit(&rp->r_statelock);
			(void) segmap_pagecreate(segkmap, base, (uint_t)n, 1);
			pagecreate = 1;
			saved_base = base;
			saved_n = n;
		} else
			mutex_exit(&rp->r_statelock);

		/*
		 * The number of bytes of data in the last page can not
		 * be accurately be determined while page is being
		 * uiomove'd to and the size of the file being updated.
		 * Thus, inform threads which need to know accurately
		 * how much data is in the last page of the file.  They
		 * will not do the i/o immediately, but will arrange for
		 * the i/o to happen later when this modify operation
		 * will have finished.
		 */
		ASSERT(!(rp->r_flags & RMODINPROGRESS));
		mutex_enter(&rp->r_statelock);
		rp->r_flags |= RMODINPROGRESS;
		rp->r_modaddr = (offset & MAXBMASK);
		mutex_exit(&rp->r_statelock);

		error = uiomove(base, n, UIO_WRITE, uio);

		/*
		 * r_size is the maximum number of
		 * bytes known to be in the file.
		 * Make sure it is at least as high as the
		 * first unwritten byte pointed to by uio_loffset.
		 */
		mutex_enter(&rp->r_statelock);
		if (rp->r_size < uio->uio_loffset)
			rp->r_size = uio->uio_loffset;
		purge = rp->r_flags & RPURGECACHE;
		rp->r_flags &= ~(RMODINPROGRESS | RPURGECACHE);
		rp->r_flags |= RDIRTY;
		mutex_exit(&rp->r_statelock);

		/* n = # of bytes written */
		n = (int)(uio->uio_loffset - offset);
		base += n;
		tcount -= n;
		/*
		 * If we created pages w/o initializing them completely,
		 * we need to zero the part that wasn't set up.
		 * This happens on a most EOF write cases and if
		 * we had some sort of error during the uiomove.
		 */
		if (pagecreate) {
			if ((uio->uio_loffset & PAGEOFFSET) || n == 0)
				(void) kzero(base, PAGESIZE - n);

			/*
			 * For bug 1094402: segmap_pagecreate locks
			 * page. Unlock it. This also unlocks the pages
			 * allocated by page_create_va() in segmap_pagecreate()
			 */
			sm_error = segmap_fault(kas.a_hat, segkmap,
			    saved_base, saved_n, F_SOFTUNLOCK, S_WRITE);

			if (error == 0)
				error = sm_error;
		}
		/*
		 * 1256315: RPURGECACHE dodges a watchdog by letting
		 * this cache flush happen now, rather than several
		 * frames down a page fault call stack below uiomove.
		 */
		if (purge)
			nfs_purge_caches(RTOV(rp), CRED());

	} while (tcount > 0 && error == 0);

	return (error);
}

int
nfs_putpages(vnode_t *vp, u_offset_t off, size_t len, int flags, cred_t *cr)
{
	rnode_t *rp;
	page_t *pp;
	u_offset_t eoff;
	u_offset_t io_off;
	size_t io_len;
	int error;
	int rdirty;
	int err;

	rp = VTOR(vp);
	ASSERT(rp->r_count > 0);

	if (vp->v_pages == NULL) {
		if (rp->r_flags & RDIRTY) {
			mutex_enter(&rp->r_statelock);
			rp->r_flags &= ~RDIRTY;
			mutex_exit(&rp->r_statelock);
		}
		return (0);
	}

	ASSERT(vp->v_type != VCHR);

	/*
	 * If ROUTOFSPACE is set, then all writes turn into B_INVAL
	 * writes.  B_FORCE is set to force the VM system to actually
	 * invalidate the pages, even if the i/o failed.  The pages
	 * need to get invalidated because they can't be written out
	 * because there isn't any space left on either the server's
	 * file system or in the user's disk quota.  The B_FREE bit
	 * is cleared to avoid confusion as to whether this is a
	 * request to place the page on the freelist or to destroy
	 * it.
	 */
	if ((rp->r_flags & ROUTOFSPACE) ||
	    (vp->v_vfsp->vfs_flag & VFS_UNMOUNTED))
		flags = (flags & ~B_FREE) | B_INVAL | B_FORCE;

	if (len == 0) {
		/*
		 * If doing a full file operation, then clear the
		 * RDIRTY bit.  If a page gets dirtied while the flush
		 * is happening, then RDIRTY will get set again.  The
		 * RDIRTY bit must get cleared before the flush so that
		 * we don't lose this information.
		 */
		if (off == (u_offset_t)0 && (rp->r_flags & RDIRTY)) {
			mutex_enter(&rp->r_statelock);
			rdirty = (rp->r_flags & RDIRTY);
			rp->r_flags &= ~RDIRTY;
			mutex_exit(&rp->r_statelock);
		} else
			rdirty = 0;

		/*
		 * Search the entire vp list for pages >= off, and flush
		 * the dirty pages.
		 */
		error = pvn_vplist_dirty(vp, off, rp->r_putapage,
					flags, cr);

		/*
		 * If an error occured and the file was marked as dirty
		 * before and we aren't forcibly invalidating pages, then
		 * reset the RDIRTY flag.
		 */
		if (error && rdirty &&
		    (flags & (B_INVAL | B_FORCE)) != (B_INVAL | B_FORCE)) {
			mutex_enter(&rp->r_statelock);
			rp->r_flags |= RDIRTY;
			mutex_exit(&rp->r_statelock);
		}
	} else {
		/*
		 * Do a range from [off...off + len) looking for pages
		 * to deal with.
		 */
		error = 0;
#ifdef lint
		io_len = 0;
#endif
		eoff = off + len;
		mutex_enter(&rp->r_statelock);
		for (io_off = off; io_off < eoff && io_off < rp->r_size;
		    io_off += io_len) {
			mutex_exit(&rp->r_statelock);
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || !(flags & B_ASYNC)) {
				pp = page_lookup(vp, io_off,
				    (flags & (B_INVAL | B_FREE)) ?
				    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, io_off,
				    (flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || !pvn_getdirty(pp, flags))
				io_len = PAGESIZE;
			else {
				err = (*rp->r_putapage)(vp, pp, &io_off,
				    &io_len, flags, cr);
				if (!error)
					error = err;
				/*
				 * "io_off" and "io_len" are returned as
				 * the range of pages we actually wrote.
				 * This allows us to skip ahead more quickly
				 * since several pages may've been dealt
				 * with by this iteration of the loop.
				 */
			}
			mutex_enter(&rp->r_statelock);
		}
		mutex_exit(&rp->r_statelock);
	}

	return (error);
}

void
nfs_invalidate_pages(vnode_t *vp, u_offset_t off, cred_t *cr)
{
	rnode_t *rp;

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);
	while (rp->r_flags & RTRUNCATE)
		cv_wait(&rp->r_cv, &rp->r_statelock);
	rp->r_flags |= RTRUNCATE;
	if (off == (u_offset_t)0) {
		rp->r_flags &= ~RDIRTY;
		if (!(rp->r_flags & RDONTWRITE))
			rp->r_error = 0;
	}
	rp->r_truncaddr = off;
	mutex_exit(&rp->r_statelock);
	(void) pvn_vplist_dirty(vp, off, rp->r_putapage,
		B_INVAL | B_TRUNC, cr);
	mutex_enter(&rp->r_statelock);
	rp->r_flags &= ~RTRUNCATE;
	cv_broadcast(&rp->r_cv);
	mutex_exit(&rp->r_statelock);
}

static int nfs_write_error_to_cons_only = 0;
#define	MSG(x)	(nfs_write_error_to_cons_only ? (x) : (x) + 1)

/*
 * Print a file handle
 */
void
nfs_printfhandle(nfs_fhandle *fhp)
{
	int *ip;
	char *buf;
	size_t bufsize;
	char *cp;

	/*
	 * 13 == "(file handle:"
	 * maximum of NFS_FHANDLE / sizeof (*ip) elements in fh_buf times
	 *	1 == ' '
	 *	8 == maximum strlen of "%x"
	 * 3 == ")\n\0"
	 */
	bufsize = 13 + ((NFS_FHANDLE_LEN / sizeof (*ip)) * (1 + 8)) + 3;
	buf = kmem_alloc(bufsize, KM_NOSLEEP);
	if (buf == NULL)
		return;

	cp = buf;
	(void) strcpy(cp, "(file handle:");
	while (*cp != '\0')
		cp++;
	for (ip = (int *)fhp->fh_buf;
	    ip < (int *)&fhp->fh_buf[fhp->fh_len];
	    ip++) {
		(void) sprintf(cp, " %x", *ip);
		while (*cp != '\0')
			cp++;
	}
	(void) strcpy(cp, ")\n");

	cmn_err(CE_CONT, MSG("^%s"), buf);

	kmem_free(buf, bufsize);
}

/*
 * Notify the system administrator that an NFS write error has
 * occurred.
 */

/* seconds between ENOSPC/EDQUOT messages */
static clock_t nfs_write_error_interval = 5;

void
nfs_write_error(vnode_t *vp, int error, cred_t *cr)
{
	mntinfo_t *mi;

	mi = VTOMI(vp);
	/*
	 * In case of forced unmount, do not print any messages
	 * since it can flood the console with error messages.
	 */
	if (mi->mi_vfsp->vfs_flag & VFS_UNMOUNTED)
		return;

	/*
	 * No use in flooding the console with ENOSPC
	 * messages from the same file system.
	 */
	if ((error != ENOSPC && error != EDQUOT) ||
	    lbolt - mi->mi_printftime > 0) {
#ifdef DEBUG
		nfs_perror(error, "NFS%ld write error on host %s: %m.\n",
		    mi->mi_vers, VTOR(vp)->r_server->sv_hostname, NULL);
#else
		nfs_perror(error, "NFS write error on host %s: %m.\n",
		    VTOR(vp)->r_server->sv_hostname, NULL);
#endif
		if (error == ENOSPC || error == EDQUOT) {
			cmn_err(CE_CONT,
			    MSG("^File: userid=%d, groupid=%d\n"),
			    cr->cr_uid, cr->cr_gid);
			if (curthread->t_cred->cr_uid != cr->cr_uid ||
			    curthread->t_cred->cr_gid != cr->cr_gid) {
				cmn_err(CE_CONT,
				    MSG("^User: userid=%d, groupid=%d\n"),
				    curthread->t_cred->cr_uid,
				    curthread->t_cred->cr_gid);
			}
			mi->mi_printftime = lbolt +
			    nfs_write_error_interval * hz;
		}
		nfs_printfhandle(&VTOR(vp)->r_fh);
#ifdef DEBUG
		if (error == EACCES) {
			cmn_err(CE_CONT, MSG("^nfs_bio: cred is%s kcred\n"),
			    cr == kcred ? "" : " not");
		}
#endif
	}
}

/*
 * NFS Client initialization routine.  This routine should only be called
 * once.  It performs the following tasks:
 *	- Initalize all global locks
 * 	- Call sub-initialization routines (localize access to variables)
 */
int
nfs_clntinit(void)
{
#ifdef DEBUG
	static boolean_t nfs_clntup = B_FALSE;
#endif
	int error;

#ifdef DEBUG
	ASSERT(nfs_clntup == B_FALSE);
#endif

	error = nfs_subrinit();
	if (error)
		return (error);

	error = nfs_vfsinit();
	if (error) {
		/*
		 * Cleanup nfs_subrinit() work
		 */
		nfs_subrfini();
		return (error);
	}

#ifdef DEBUG
	nfs_clntup = B_TRUE;
#endif
	return (0);
}

/*
 * This routine is only called if the NFS Client has been initialized but
 * the module failed to be installed. This routine will cleanup the previously
 * allocated/initialized work.
 */
void
nfs_clntfini(void)
{
	nfs_subrfini();
	nfs_vfsfini();
}

/*
 * nfs_lockrelease:
 *
 * Release any locks on the given vnode that are held by the current
 * process.
 */
void
nfs_lockrelease(vnode_t *vp, int flag, offset_t offset, cred_t *cr)
{
	flock64_t ld;
	struct shrlock shr;
	char *buf;
	int remote_lock_possible;
	int ret;

	ASSERT((uintptr_t)vp > KERNELBASE);

	/*
	 * Generate an explicit unlock operation for the entire file.  As a
	 * partial optimization, only generate the unlock if there is a
	 * lock registered for the file.  We could check whether this
	 * particular process has any locks on the file, but that would
	 * require the local locking code to provide yet another query
	 * routine.  Note that no explicit synchronization is needed here.
	 * At worst, flk_has_remote_locks() will return a false positive,
	 * in which case the unlock call wastes time but doesn't harm
	 * correctness.
	 *
	 * In addition, an unlock request is generated if the process
	 * is listed as possibly having a lock on the file because the
	 * server and client lock managers may have gotten out of sync.
	 * N.B. It is important to make sure nfs_remove_locking_id() is
	 * called here even if flk_has_remote_locks(vp) reports true.
	 * If it is not called and there is an entry on the process id
	 * list, that entry will never get removed.
	 */
	remote_lock_possible = nfs_remove_locking_id(vp, RLMPL_PID,
	    (char *)&(ttoproc(curthread)->p_pid), NULL, NULL);
	if (remote_lock_possible || flk_has_remote_locks(vp)) {
		ld.l_type = F_UNLCK;	/* set to unlock entire file */
		ld.l_whence = 0;	/* unlock from start of file */
		ld.l_start = 0;
		ld.l_len = 0;		/* do entire file */
		ret = VOP_FRLOCK(vp, F_SETLK, &ld, flag, offset, cr);

		if (ret != 0) {
			/*
			 * If VOP_FRLOCK fails, make sure we unregister
			 * local locks before we continue.
			 */
			ld.l_pid = ttoproc(curthread)->p_pid;
			lm_register_lock_locally(vp, NULL, &ld, flag, offset);
#ifdef DEBUG
			nfs_perror(ret,
			    "NFS lock release error on vp %p: %m.\n",
			    (void *)vp, NULL);
#endif
		}

		/*
		 * The call to VOP_FRLOCK may put the pid back on the
		 * list.  We need to remove it.
		 */
		(void) nfs_remove_locking_id(vp, RLMPL_PID,
		    (char *)&(ttoproc(curthread)->p_pid), NULL, NULL);
	}

	/*
	 * As long as the vp has a share matching our pid,
	 * pluck it off and unshare it.  There are circumstances in
	 * which the call to nfs_remove_locking_id() may put the
	 * owner back on the list, in which case we simply do a
	 * redundant and harmless unshare.
	 */
	buf = kmem_alloc(MAX_SHR_OWNER_LEN, KM_SLEEP);
	while (nfs_remove_locking_id(vp, RLMPL_OWNER,
	    (char *)NULL, buf, &shr.s_own_len)) {
		shr.s_owner = buf;
		shr.s_access = 0;
		shr.s_deny = 0;
		shr.s_sysid = 0;
		shr.s_pid = curproc->p_pid;

		ret = VOP_SHRLOCK(vp, F_UNSHARE, &shr, flag);
#ifdef DEBUG
		if (ret != 0) {
			nfs_perror(ret,
			    "NFS share release error on vp %p: %m.\n",
			    (void *)vp, NULL);
		}
#endif
	}
	kmem_free(buf, MAX_SHR_OWNER_LEN);
}

/*
 * nfs_lockcompletion:
 *
 * If the vnode has a lock that makes it unsafe to cache the file, mark it
 * as non cachable (set VNOCACHE bit).
 */

void
nfs_lockcompletion(vnode_t *vp, int cmd)
{
	rnode_t *rp = VTOR(vp);

	ASSERT(nfs_rw_lock_held(&rp->r_lkserlock, RW_WRITER));

	if (cmd == F_SETLK || cmd == F_SETLKW) {
		if (!lm_safemap(vp)) {
			mutex_enter(&vp->v_lock);
			vp->v_flag |= VNOCACHE;
			mutex_exit(&vp->v_lock);
		} else {
			mutex_enter(&vp->v_lock);
			vp->v_flag &= ~VNOCACHE;
			mutex_exit(&vp->v_lock);
		}
	}
}

/*
 * The lock manager holds state making it possible for the client
 * and server to be out of sync.  For example, if the response from
 * the server granting a lock request is lost, the server will think
 * the lock is granted and the client will think the lock is lost.
 * The client can tell when it is not positive if it is in sync with
 * the server.
 *
 * To deal with this, a list of processes for which the client is
 * not sure if the server holds a lock is attached to the rnode.
 * When such a process closes the rnode, an unlock request is sent
 * to the server to unlock the entire file.
 *
 * The list is kept as a singularly linked NULL terminated list.
 * Because it is only added to under extreme error conditions, the
 * list shouldn't get very big.  DEBUG kernels print a message if
 * the list gets bigger than nfs_lmpl_high_water.  This is arbitrarily
 * choosen to be 8, but can be tuned at runtime.
 */
#ifdef DEBUG
/* int nfs_lmpl_high_water = 8; */
int nfs_lmpl_high_water = 128;
int nfs_cnt_add_locking_id = 0;
int nfs_len_add_locking_id = 0;
#endif /* DEBUG */

/*
 * Record that the nfs lock manager server may be holding a lock on
 * a vnode for a process.
 *
 * Because the nfs lock manager server holds state, it is possible
 * for the server to get out of sync with the client.  This routine is called
 * from the client when it is no longer sure if the server is in sync
 * with the client.  nfs_lockrelease() will then notice this and send
 * an unlock request when the file is closed
 */
void
nfs_add_locking_id(vnode_t *vp, pid_t pid, int type, char *id, int len)
{
	rnode_t *rp;
	lmpl_t *new;
	lmpl_t *cur;
	lmpl_t **lmplp;
#ifdef DEBUG
	int list_len = 1;
#endif /* DEBUG */

#ifdef DEBUG
	++nfs_cnt_add_locking_id;
#endif /* DEBUG */
	/*
	 * allocate new lmpl_t now so we don't sleep
	 * later after grabbing mutexes
	 */
	ASSERT(len < MAX_SHR_OWNER_LEN);
	new = kmem_alloc(sizeof (*new), KM_SLEEP);
	new->lmpl_type = type;
	new->lmpl_pid = pid;
	new->lmpl_owner = kmem_alloc(len, KM_SLEEP);
	bcopy(id, new->lmpl_owner, len);
	new->lmpl_own_len = len;
	new->lmpl_next = (lmpl_t *)NULL;
#ifdef DEBUG
	if (type == RLMPL_PID) {
		ASSERT(len == sizeof (pid_t));
		ASSERT(pid == *(pid_t *)new->lmpl_owner);
	} else {
		ASSERT(type == RLMPL_OWNER);
	}
#endif

	rp = VTOR(vp);
	mutex_enter(&rp->r_statelock);

	/*
	 * Add this id to the list for this rnode only if the
	 * rnode is active and the id is not already there.
	 */
	ASSERT(rp->r_flags & RHASHED);
	lmplp = &(rp->r_lmpl);
	for (cur = rp->r_lmpl; cur != (lmpl_t *)NULL; cur = cur->lmpl_next) {
		if (cur->lmpl_pid == pid &&
		    cur->lmpl_type == type &&
		    cur->lmpl_own_len == len &&
		    bcmp(cur->lmpl_owner, new->lmpl_owner, len) == 0) {
			kmem_free(new->lmpl_owner, len);
			kmem_free(new, sizeof (*new));
			break;
		}
		lmplp = &cur->lmpl_next;
#ifdef DEBUG
		++list_len;
#endif /* DEBUG */
	}
	if (cur == (lmpl_t *)NULL) {
		*lmplp = new;
#ifdef DEBUG
		if (list_len > nfs_len_add_locking_id) {
			nfs_len_add_locking_id = list_len;
		}
		if (list_len > nfs_lmpl_high_water) {
			cmn_err(CE_WARN, "nfs_add_locking_id: long list "
			    "vp=%p is %d", (void *)vp, list_len);
		}
#endif /* DEBUG */
	}

#ifdef DEBUG
	if (share_debug) {
		int nitems = 0;
		int npids = 0;
		int nowners = 0;

		/*
		 * Count the number of things left on r_lmpl after the remove.
		 */
		for (cur = rp->r_lmpl; cur != (lmpl_t *)NULL;
		    cur = cur->lmpl_next) {
			nitems++;
			if (cur->lmpl_type == RLMPL_PID) {
				npids++;
			} else if (cur->lmpl_type == RLMPL_OWNER) {
				nowners++;
			} else {
				cmn_err(CE_PANIC, "nfs_add_locking_id: "
				    "unrecognised lmpl_type %d",
				    cur->lmpl_type);
			}
		}

		cmn_err(CE_CONT, "nfs_add_locking_id(%s): %d PIDs + %d "
		    "OWNs = %d items left on r_lmpl\n",
		    (type == RLMPL_PID) ? "P" : "O", npids, nowners, nitems);
	}
#endif

	mutex_exit(&rp->r_statelock);
}

/*
 * Remove an id from the lock manager id list.
 *
 * If the id is not in the list return 0.  If it was found and
 * removed, return 1.
 */
static int
nfs_remove_locking_id(vnode_t *vp, int type, char *id, char *rid, int *rlen)
{
	lmpl_t *cur;
	lmpl_t **lmplp;
	rnode_t *rp;
	int rv = 0;

	ASSERT(type == RLMPL_PID || type == RLMPL_OWNER);

	rp = VTOR(vp);

	mutex_enter(&rp->r_statelock);
	ASSERT(rp->r_flags & RHASHED);
	lmplp = &(rp->r_lmpl);

	/*
	 * Search through the list and remove the entry for this id
	 * if it is there.  The special case id == NULL allows removal
	 * of the first share on the r_lmpl list belonging to the
	 * current process (if any), without regard to further details
	 * of its identity.
	 */
	for (cur = rp->r_lmpl; cur != (lmpl_t *)NULL; cur = cur->lmpl_next) {
		if (cur->lmpl_type == type &&
		    cur->lmpl_pid == curproc->p_pid &&
		    (id == (char *)NULL ||
		    bcmp(cur->lmpl_owner, id, cur->lmpl_own_len) == 0)) {
			*lmplp = cur->lmpl_next;
			ASSERT(cur->lmpl_own_len < MAX_SHR_OWNER_LEN);
			if (rid != NULL) {
				bcopy(cur->lmpl_owner, rid, cur->lmpl_own_len);
				*rlen = cur->lmpl_own_len;
			}
			kmem_free(cur->lmpl_owner, cur->lmpl_own_len);
			kmem_free(cur, sizeof (*cur));
			rv = 1;
			break;
		}
		lmplp = &cur->lmpl_next;
	}

#ifdef DEBUG
	if (share_debug) {
		int nitems = 0;
		int npids = 0;
		int nowners = 0;

		/*
		 * Count the number of things left on r_lmpl after the remove.
		 */
		for (cur = rp->r_lmpl; cur != (lmpl_t *)NULL;
				cur = cur->lmpl_next) {
			nitems++;
			if (cur->lmpl_type == RLMPL_PID) {
				npids++;
			} else if (cur->lmpl_type == RLMPL_OWNER) {
				nowners++;
			} else {
				cmn_err(CE_PANIC,
					"nrli: unrecognised lmpl_type %d",
					cur->lmpl_type);
			}
		}

		cmn_err(CE_CONT,
		"nrli(%s): %d PIDs + %d OWNs = %d items left on r_lmpl\n",
			(type == RLMPL_PID) ? "P" : "O",
			npids,
			nowners,
			nitems);
	}
#endif

	mutex_exit(&rp->r_statelock);
	return (rv);
}

void
nfs_free_mi(mntinfo_t *mi)
{
	if (mi->mi_io_kstats)
		kstat_delete(mi->mi_io_kstats);
	if (mi->mi_ro_kstats)
		kstat_delete(mi->mi_ro_kstats);
	mutex_destroy(&mi->mi_lock);
	mutex_destroy(&mi->mi_async_lock);
	cv_destroy(&mi->mi_failover_cv);
	cv_destroy(&mi->mi_async_reqs_cv);
	cv_destroy(&mi->mi_async_cv);
	kmem_free(mi, sizeof (*mi));
}

static int
mnt_kstat_update(kstat_t *ksp, int rw)
{
	mntinfo_t *mi = (mntinfo_t *)ksp->ks_private;
	struct mntinfo_kstat *mik = (struct mntinfo_kstat *)ksp->ks_data;
	int i;

	/* this is a read-only kstat. Bail out on a write */
	if (rw == KSTAT_WRITE)
		return (EACCES);

	/* Lock VFS to prevent unmount during update */
	vfs_lock_wait(mi->mi_vfsp);

	/* If mi_curr_serv is null, bail; the FS was unmounted as we waited. */
	if (mi->mi_curr_serv == NULL) {
		vfs_unlock(mi->mi_vfsp);
		return (ENOENT);
	}

	(void) strcpy(mik->mik_proto, mi->mi_curr_serv->sv_knconf->knc_proto);

	mik->mik_vers = (uint32_t)mi->mi_vers;
	mik->mik_flags = mi->mi_flags;
	mik->mik_secmod = mi->mi_curr_serv->sv_secdata->secmod;
	mik->mik_curread = (uint32_t)mi->mi_curread;
	mik->mik_curwrite = (uint32_t)mi->mi_curwrite;
	mik->mik_retrans = mi->mi_retrans;
	mik->mik_timeo = mi->mi_timeo;
	mik->mik_acregmin = mi->mi_acregmin;
	mik->mik_acregmax = mi->mi_acregmax;
	mik->mik_acdirmin = mi->mi_acdirmin;
	mik->mik_acdirmax = mi->mi_acdirmax;

	for (i = 0; i < NFS_CALLTYPES + 1; i++) {
		mik->mik_timers[i].srtt = (uint32_t)mi->mi_timers[i].rt_srtt;
		mik->mik_timers[i].deviate =
		    (uint32_t)mi->mi_timers[i].rt_deviate;
		mik->mik_timers[i].rtxcur =
		    (uint32_t)mi->mi_timers[i].rt_rtxcur;
	}

	mik->mik_noresponse = (uint32_t)mi->mi_noresponse;
	mik->mik_failover = (uint32_t)mi->mi_failover;
	mik->mik_remap = (uint32_t)mi->mi_remap;

	(void) strcpy(mik->mik_curserver, mi->mi_curr_serv->sv_hostname);

	vfs_unlock(mi->mi_vfsp);

	return (0);
}

void
nfs_mnt_kstat_init(struct vfs *vfsp)
{
	mntinfo_t *mi = VFTOMI(vfsp);

	mi->mi_io_kstats = kstat_create("nfs", getminor(vfsp->vfs_dev),
	    NULL, "nfs", KSTAT_TYPE_IO, 1, 0);
	if (mi->mi_io_kstats) {
		mi->mi_io_kstats->ks_lock = &mi->mi_lock;
		kstat_install(mi->mi_io_kstats);
	}

	if ((mi->mi_ro_kstats = kstat_create("nfs", getminor(vfsp->vfs_dev),
	    "mntinfo", "misc", KSTAT_TYPE_RAW,
	    sizeof (struct mntinfo_kstat), 0)) != NULL) {
		mi->mi_ro_kstats->ks_update = mnt_kstat_update;
		mi->mi_ro_kstats->ks_private = (void *)mi;
		kstat_install(mi->mi_ro_kstats);
	}
}
