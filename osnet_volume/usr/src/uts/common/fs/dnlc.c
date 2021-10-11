/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989 AT&T.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)dnlc.c	1.37	99/02/23 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/dnlc.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/bitmap.h>
#include <sys/var.h>

/*
 * Directory name lookup cache.
 * Based on code originally done by Robert Elz at Melbourne.
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  Each hash chain is ordered by LRU
 * Cache is indexed by hash value obtained from (vp, name)
 * where the vp refers to the directory containing the name.
 */

/*
 * NC_HASHAVELEN is the average length desired for this chain, from
 * which the size of the nc_hash table is derived at create time.
 *
 * NC_MOVETOFRONT is the move-to-front threshold: if the hash lookup
 * depth exceeds this value, we move the looked-up entry to the front of
 * its hash chain.  The idea is to make sure that the most frequently
 * accessed entries are found most quickly (by keeping them near the
 * front of their hash chains).
 */
#define	NC_HASHAVELEN	2
#define	NC_MOVETOFRONT	2

/*
 * Private extended DNLC bean counters.  Both this and ncstats will
 * be replaced with an improved set of named kstats in the future.
 * Please do not develop tools which rely on this set of counters.
 */
struct xncstats {
	uint64_t xnc_pick_free;	/* # enters which got a free ncache */
	uint64_t xnc_pick_heur; /* # enters which got ncache w/ NULL vpages */
	uint64_t xnc_pick_last;	/* # enters which got last ncache on chain */
	uint64_t xnc_purge_cnt;	/* # ncache entries purge by dnlc_purge*() */
	uint64_t xnc_purge_all;	/* # dnlc_purge() calls */
	uint64_t xnc_purge_vp;	/* # dnlc_purge_vp() calls */
	uint64_t xnc_purge_vfs;	/* # dnlc_purge_vfsp() calls */
	uint64_t xnc_purge_fs1;	/* # dnlc_fs_purge1() calls */
};

/*
 * Hash table of name cache entries for fast lookup, dynamically
 * allocated at startup.
 */
struct nc_hash	{
	struct ncache	*hash_next;
	struct ncache	*hash_prev;
	kmutex_t	hash_lock;
} *nc_hash;

/*
 * Rotors. Used to select entries on a round-robin basis.
 */
static struct ncache *dnlc_purge_rotor;
static struct nc_hash *dnlc_free_rotor;

/*
 * Freelist. Entries with no vp's associated with them
 */
static struct ncache *dnlc_freelist;
static kmutex_t	dnlc_free_lock;

/*
 * The name cache itself, dynamically allocated at startup.
 */

/*
 * # of dnlc entries (uninitialized)
 *
 * the initial value was chosen as being
 * a random string of bits, probably not
 * normally chosen by a systems administrator
 */
int ncsize = 0xbadcafe;
static struct ncache *ncache;
static int nc_hashsz;		/* size of hash table */

/*
 * This table holds a list of vnodes to be released without holding other
 * locks.
 *
 * We don't want to hold any dnlc locks while vn_rele()
 * performs any I/O (e.g. nfs_inactive).
 */
static vnode_t	**nc_rele;	/* array of vnode_t ptrs */
static int	nc_rsize;	/* size of nc_rele */
static kmutex_t	nc_rele_lock;	/* protects nc_rele */

struct ncstats ncstats;
struct xncstats xncstats;

/*
 * Private DNLC interposition hooks.  These will allow us to trace
 * DNLC activity from a driver and simulate alternate algorithms.
 */
void (*dnlc_enter_hook)(vnode_t *, char *, vnode_t *, cred_t *) =
	(void (*)(vnode_t *, char *, vnode_t *, cred_t *))nulldev;

void (*dnlc_update_hook)(vnode_t *, char *, vnode_t *, cred_t *) =
	(void (*)(vnode_t *, char *, vnode_t *, cred_t *))nulldev;

void (*dnlc_lookup_hook)(vnode_t *, char *, cred_t *) =
	(void (*)(vnode_t *, char *, cred_t *))nulldev;

void (*dnlc_remove_hook)(vnode_t *, char *) =
	(void (*)(vnode_t *, char *))nulldev;

void (*dnlc_purge_hook)(void) = (void (*)(void))nulldev;
void (*dnlc_purge_vp_hook)(vnode_t *) = (void (*)(vnode_t *))nulldev;
void (*dnlc_purge_vfsp_hook)(vfs_t *, int) = (void (*)(vfs_t *, int))nulldev;
void (*dnlc_purge_fs_hook)(vnodeops_t *) = (void (*)(vnodeops_t *))nulldev;

static int doingcache = 1;

static void		nc_inshash(struct ncache *, struct nc_hash *);
static void		nc_rmhash(struct ncache *);
static void		nc_move_to_front(struct nc_hash *, struct ncache *);
static void		dnlc_free(struct ncache *);
static struct ncache	*dnlc_get(void);
static struct ncache	*dnlc_search(vnode_t *, char *, int, int, cred_t *);

/*
 * The dnlc hashing function.
 * 'hash' and 'namlen' must be l-values.
 */
#define	DNLC_HASH(name, vp, hash, namlen)			\
	{							\
		char Xc, *Xcp;					\
		hash = (int)vp >> 8;				\
		for (Xcp = (name); (Xc = *Xcp) != 0; Xcp++)	\
			hash = (hash << 4) + hash + Xc;		\
		namlen = Xcp - (name);				\
	}

/*
 * Initialize the directory cache.
 * Put all the entries on the LRU chain and clear out the hash links.
 */
void
dnlc_init()
{
	struct nc_hash *hp;
	int i;

	if (ncsize == 0xbadcafe)
		ncsize = 4 * (v.v_proc + maxusers) + 320;	/* DNLC size */

	/*
	 * Compute hash size rounding to the next power of two.
	 */
	nc_hashsz = ncsize / NC_HASHAVELEN;
	nc_hashsz = 1 << highbit(nc_hashsz);
	nc_rsize = 2 * ncsize;	/* each ncache entry holds 2 vnodes */
	if (ncsize <= 0) {
		doingcache = 0;
		ncsize = 0;
		cmn_err(CE_NOTE, "name cache (dnlc) disabled");
		return;
	}
	ncache = kmem_zalloc(ncsize * sizeof (*ncache), KM_SLEEP);
	nc_hash = kmem_zalloc(nc_hashsz * sizeof (*nc_hash), KM_SLEEP);
	nc_rele = kmem_zalloc(nc_rsize * sizeof (vnode_t *), KM_SLEEP);

	for (i = 0; i < nc_hashsz; i++) {
		hp = (struct nc_hash *)&nc_hash[i];
		mutex_init(&hp->hash_lock, NULL, MUTEX_DEFAULT, NULL);
		hp->hash_next = (struct ncache *)hp;
		hp->hash_prev = (struct ncache *)hp;
	}
	/*
	 * Put all of the entries on the freelist
	 */
	for (i = 0; i < ncsize; i++) {
		dnlc_free(&ncache[i]);
	}
	/*
	 * Initialize rotors
	 */
	dnlc_purge_rotor = &ncache[0];
	dnlc_free_rotor = &nc_hash[0];
}

/*
 * Add a name to the directory cache.
 */
void
dnlc_enter(vnode_t *dp, char *name, vnode_t *vp, cred_t *cred)
{
	int namlen;
	struct ncache *ncp;
	struct nc_hash *hp;
	int hash;

	dnlc_enter_hook(dp, name, vp, cred);

	TRACE_0(TR_FAC_NFS, TR_DNLC_ENTER_START,
		"dnlc_enter_start:");

	if (!doingcache) {
		TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
			"dnlc_enter_end:(%S) %d",
			"not caching", 0);
		return;
	}

	DNLC_HASH(name, dp, hash, namlen);
	/*
	 * Get a free dnlc entry. Assume the entry won't be in the cache
	 * and initialize it now
	 */
	if ((ncp = dnlc_get()) == NULL)
		return;
	ncp->dp = dp;
	VN_HOLD(dp);
	ncp->vp = vp;
	VN_HOLD(vp);
	ncp->namlen = namlen;
	ncp->name = kmem_alloc(ncp->namlen, KM_SLEEP);
	bcopy(name, ncp->name, ncp->namlen);
	ncp->cred = cred;
	ncp->hash = hash;
	if (cred)
		crhold(cred);
	hp = &nc_hash[hash & (nc_hashsz - 1)];

	mutex_enter(&hp->hash_lock);
	if (dnlc_search(dp, name, namlen, hash, cred) != NULL) {
		mutex_exit(&hp->hash_lock);
		ncstats.dbl_enters++;
		VN_RELE(dp);
		VN_RELE(vp);
		dnlc_free(ncp);		/* crfree done here */
		TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
			"dnlc_enter_end:(%S) %d",
			"dbl enter", ncstats.dbl_enters);
		return;
	}
	/*
	 * Insert back into the hash chain.
	 */
	nc_inshash(ncp, hp);
	mutex_exit(&hp->hash_lock);
	ncstats.enters++;
	TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
		"dnlc_enter_end:(%S) %d",
		"done", ncstats.enters);
}

/*
 * Add a name to the directory cache.
 *
 * This function is basically identical with
 * dnlc_enter().  The difference is that when the
 * desired dnlc entry is found, the vnode in the
 * ncache is compared with the vnode passed in.
 *
 * If they are not equal then the ncache is
 * updated with the passed in vnode.  Otherwise
 * it just frees up the newly allocated dnlc entry.
 */
void
dnlc_update(vnode_t *dp, char *name, vnode_t *vp, cred_t *cred)
{
	int namlen;
	struct ncache *ncp;
	struct ncache *tcp;
	vnode_t *tvp;
	struct nc_hash *hp;
	int hash;

	dnlc_update_hook(dp, name, vp, cred);

	TRACE_0(TR_FAC_NFS, TR_DNLC_ENTER_START,
		"dnlc_update_start:");

	if (!doingcache) {
		TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
			"dnlc_update_end:(%S) %d",
			"not caching", 0);
		return;
	}

	DNLC_HASH(name, dp, hash, namlen);
	/*
	 * Get a free dnlc entry. Assume the entry won't be in the cache
	 * and initialize it now
	 */
	if ((ncp = dnlc_get()) == NULL)
		return;
	ncp->dp = dp;
	VN_HOLD(dp);
	ncp->vp = vp;
	VN_HOLD(vp);

	ncp->namlen = namlen;
	ncp->name = kmem_alloc(ncp->namlen, KM_SLEEP);
	bcopy(name, ncp->name, ncp->namlen);
	ncp->cred = cred;
	ncp->hash = hash;
	if (cred)
		crhold(cred);
	hp = &nc_hash[hash & (nc_hashsz - 1)];

	mutex_enter(&hp->hash_lock);
	if ((tcp = dnlc_search(dp, name, namlen, hash, cred)) != NULL) {
		if (tcp->vp != vp) {
			tvp = tcp->vp;
			tcp->vp = vp;
			mutex_exit(&hp->hash_lock);
			VN_RELE(tvp);
			ncstats.enters++;
			TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
				"dnlc_update_end:(%S) %d",
				"done", ncstats.enters);
		} else {
			mutex_exit(&hp->hash_lock);
			VN_RELE(vp);
			ncstats.dbl_enters++;
			TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
				"dnlc_update_end:(%S) %d",
				"dbl enter", ncstats.dbl_enters);
		}
		VN_RELE(dp);
		dnlc_free(ncp);		/* crfree done here */
		return;
	}
	/*
	 * insert the new entry, since it is not in dnlc yet
	 */
	nc_inshash(ncp, hp);
	mutex_exit(&hp->hash_lock);
	ncstats.enters++;
	TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
		"dnlc_update_end:(%S) %d",
		"done", ncstats.enters);
}

/*
 * Look up a name in the directory name cache.
 *
 * Return a doubly-held vnode if found: one hold so that it may
 * remain in the cache for other users, the other hold so that
 * the cache is not re-cycled and the identity of the vnode is
 * lost before the caller can use the vnode.
 */
vnode_t *
dnlc_lookup(vnode_t *dp, char *name, cred_t *cred)
{
	int namlen, hash, depth;
	struct ncache *ncp;
	struct nc_hash *hp;
	vnode_t *vp;

	dnlc_lookup_hook(dp, name, cred);

	TRACE_2(TR_FAC_NFS, TR_DNLC_LOOKUP_START,
		"dnlc_lookup_start:dp %x name %s", dp, name);

	if (!doingcache) {
		TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
			"dnlc_lookup_end:%S %d vp %x name %s",
			"not_caching", 0, NULL, name);
		return (NULL);
	}

	DNLC_HASH(name, dp, hash, namlen);

	depth = 0;
	hp = &nc_hash[hash & (nc_hashsz - 1)];
	mutex_enter(&hp->hash_lock);

	for (ncp = hp->hash_next; ncp != (struct ncache *)hp;
	    ncp = ncp->hash_next) {
		depth++;
		if (ncp->hash == hash &&	/* fast signature check */
		    ncp->dp == dp &&
		    ncp->namlen == namlen &&
		    ncp->cred == cred &&
		    bcmp(ncp->name, name, namlen) == 0) {
			/*
			 * Move this entry to the head of its hash chain
			 * if it's not already close.
			 */
			if (depth > NC_MOVETOFRONT)
				nc_move_to_front(hp, ncp);
			/*
			 * Put a hold on the vnode now so it's identity
			 * can't change before the caller has a chance to
			 * put a hold on it.
			 */
			vp = ncp->vp;
			VN_HOLD(vp);
			mutex_exit(&hp->hash_lock);
			ncstats.hits++;
			TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
				"dnlc_lookup_end:%S %d vp %x name %s",
				"hit", ncstats.hits, vp, name);
			return (vp);
		}
	}

	mutex_exit(&hp->hash_lock);
	ncstats.misses++;
	TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
		"dnlc_lookup_end:%S %d vp %x name %s", "miss", ncstats.misses,
	    NULL, name);
	return (NULL);
}

/*
 * Remove an entry in the directory name cache.
 */
void
dnlc_remove(vnode_t *dp, char *name)
{
	int namlen;
	struct ncache *ncp;
	struct nc_hash *hp;
	int hash;

	dnlc_remove_hook(dp, name);

	if (!doingcache)
		return;
	DNLC_HASH(name, dp, hash, namlen);
	hp = &nc_hash[hash & (nc_hashsz - 1)];

	mutex_enter(&hp->hash_lock);
	while (ncp = dnlc_search(dp, name, namlen, hash, ANYCRED)) {
		vnode_t	*vp = ncp->vp;
		vnode_t	*dp = ncp->dp;

		/*
		 * Put it on the freelist
		 */
		nc_rmhash(ncp);
		mutex_exit(&hp->hash_lock);
		dnlc_free(ncp);

		VN_RELE(vp);
		VN_RELE(dp);
		mutex_enter(&hp->hash_lock);
	}
	mutex_exit(&hp->hash_lock);
}


/*
 * Purge the entire cache.
 */
void
dnlc_purge()
{
	struct nc_hash *nch;
	struct ncache *ncp;
	int	index = 0;
	int	i;

	dnlc_purge_hook();

	if (!doingcache)
		return;

	ncstats.purges++;
	xncstats.xnc_purge_all++;

	mutex_enter(&nc_rele_lock);

	for (nch = nc_hash; nch < &nc_hash[nc_hashsz]; nch++) {
		mutex_enter(&nch->hash_lock);
		ncp = nch->hash_next;
		while (ncp != (struct ncache *)nch) {
			struct ncache *np;

			np = ncp->hash_next;
			nc_rele[index++] = ncp->vp;
			nc_rele[index++] = ncp->dp;

			nc_rmhash(ncp);
			dnlc_free(ncp);
			ncp = np;
			xncstats.xnc_purge_cnt++;
		}
		mutex_exit(&nch->hash_lock);
	}

	/* Release holds on all the vnodes now */
	for (i = 0; i < index; i++) {
		VN_RELE(nc_rele[i]);
		nc_rele[i] = NULL;
	}
	mutex_exit(&nc_rele_lock);
}

/*
 * Purge any cache entries referencing a vnode.
 */
void
dnlc_purge_vp(vnode_t *vp)
{
	struct nc_hash *nch;
	struct ncache *ncp;
	int	index = 0;
	int	i;

	dnlc_purge_vp_hook(vp);

	if (!doingcache)
		return;

	ncstats.purges++;
	xncstats.xnc_purge_vp++;

	mutex_enter(&nc_rele_lock);

	for (nch = nc_hash; nch < &nc_hash[nc_hashsz]; nch++) {
		mutex_enter(&nch->hash_lock);
		ncp = nch->hash_next;
		while (ncp != (struct ncache *)nch) {
			struct ncache *np;

			np = ncp->hash_next;
			if (ncp->dp == vp || ncp->vp == vp) {
				nc_rele[index++] = ncp->vp;
				nc_rele[index++] = ncp->dp;
				nc_rmhash(ncp);
				dnlc_free(ncp);
				xncstats.xnc_purge_cnt++;
			}
			ncp = np;
		}
		mutex_exit(&nch->hash_lock);
	}

	/* Release holds on all the vnodes now */
	for (i = 0; i < index; i++) {
		VN_RELE(nc_rele[i]);
		nc_rele[i] = NULL;
	}
	mutex_exit(&nc_rele_lock);
}

/*
 * Purge cache entries referencing a vfsp.  Caller supplies a count
 * of entries to purge; up to that many will be freed.  A count of
 * zero indicates that all such entries should be purged.  Returns
 * the number of entries that were purged.
 */
int
dnlc_purge_vfsp(struct vfs *vfsp, int count)
{
	struct nc_hash *nch;
	struct ncache *ncp;
	int n = 0;
	int	index = 0;
	int	i;

	dnlc_purge_vfsp_hook(vfsp, count);

	if (!doingcache)
		return (0);

	ncstats.purges++;
	xncstats.xnc_purge_vfs++;

	mutex_enter(&nc_rele_lock);

	for (nch = nc_hash; nch < &nc_hash[nc_hashsz]; nch++) {
		mutex_enter(&nch->hash_lock);
		ncp = nch->hash_next;
		while (ncp != (struct ncache *)nch) {
			struct ncache *np;

			np = ncp->hash_next;
			if ((ncp->dp != NULL && ncp->dp->v_vfsp == vfsp) ||
			    (ncp->vp != NULL && ncp->vp->v_vfsp == vfsp)) {
				n++;
				nc_rele[index++] = ncp->vp;
				nc_rele[index++] = ncp->dp;
				nc_rmhash(ncp);
				dnlc_free(ncp);
				xncstats.xnc_purge_cnt++;
				if (count != 0 && n >= count)
					break;
			}
			ncp = np;
		}
		mutex_exit(&nch->hash_lock);
	}

	/* Release holds on all the vnodes now */
	for (i = 0; i < index; i++) {
		VN_RELE(nc_rele[i]);
		nc_rele[i] = NULL;
	}
	mutex_exit(&nc_rele_lock);
	return (n);
}

/*
 * Purge 1 entry from the dnlc that is part of the filesystem(s)
 * represented by 'vop'. The purpose of this routine is to allow
 * users of the dnlc to free a vnode that is being held by the dnlc.
 *
 * If we find a vnode that we release which will result in
 * freeing the underlying vnode (count was 1), return 1, 0
 * if no appropriate vnodes found.
 *
 * XXX vop is not the 'right' identifier for a filesystem.
 */
int
dnlc_fs_purge1(struct vnodeops *vop)
{
	struct ncache *current;
	struct ncache *finish;
	struct nc_hash *hp;
	int	hash;

	dnlc_purge_fs_hook(vop);

	if (!doingcache)
		return (0);

	xncstats.xnc_purge_fs1++;

	/*
	 * Scan the list of dnlc entries looking for a likely
	 * candidate. Since the ncache doesn't go away (kmem_free)
	 * we can scan the list without locks.
	 */
	current = finish = dnlc_purge_rotor;

	do {
		vnode_t	*vp;

		vp = current->vp;
		if (vp && vp->v_op == vop && vp->v_pages == NULL &&
		    vp->v_count == 1) {
			/*
			 * Looks like a good choice. Make sure everything
			 * still looks valid after getting the approriate
			 * lock. We only care about entries that are
			 * in the hash table (have an identity), hence
			 * we use the hash pointers as the check.
			 */
			hash = current->hash;
			hp = &nc_hash[hash & (nc_hashsz - 1)];

			mutex_enter(&hp->hash_lock);
			vp = current->vp;
			if (hash == current->hash && current->hash_next &&
			    vp && vp->v_op == vop && vp->v_pages == NULL &&
			    vp->v_count == 1) {
				vnode_t *dp;

				dp = current->dp;
				nc_rmhash(current);
				mutex_exit(&hp->hash_lock);
				dnlc_free(current);
				VN_RELE(vp)
				VN_RELE(dp);
				if (++current == &ncache[ncsize])
					current = ncache;
				dnlc_purge_rotor = current;
				xncstats.xnc_purge_cnt++;
				return (1);
			}
			mutex_exit(&hp->hash_lock);
		}
		current++;	/* next */
		if (current == &ncache[ncsize])
			current = ncache;
	} while (current != finish);
	return (0);
}

/*
 * Utility routine to search for a cache entry. Return a locked
 * ncache entry if found, NULL otherwise.
 */
static struct ncache *
dnlc_search(vnode_t *dp, char *name, int namlen, int hash, cred_t *cred)
{
	struct nc_hash *hp;
	struct ncache *ncp;

	hp = &nc_hash[hash & (nc_hashsz - 1)];

	for (ncp = hp->hash_next; ncp != (struct ncache *)hp;
	    ncp = ncp->hash_next) {
		if (ncp->hash == hash &&
		    ncp->dp == dp &&
		    ncp->namlen == namlen &&
		    (cred == ANYCRED || ncp->cred == cred) &&
		    bcmp(ncp->name, name, namlen) == 0)
			return (ncp);
	}
	return (NULL);
}

/*
 * Name cache hash list insertion and deletion routines.  These should
 * probably be recoded in assembly language for speed.
 */

/*
 * Insert entry at the front of the list
 */
static void
nc_inshash(struct ncache *ncp, struct nc_hash *hp)
{
	ncp->hash_next = hp->hash_next;
	ncp->hash_prev = (struct ncache *)hp;
	hp->hash_next->hash_prev = ncp;
	hp->hash_next = ncp;
}

static void
nc_rmhash(struct ncache *ncp)
{
	ncp->hash_prev->hash_next = ncp->hash_next;
	ncp->hash_next->hash_prev = ncp->hash_prev;
	ncp->hash_prev = NULL;
	ncp->hash_next = NULL;
}

static void
nc_move_to_front(struct nc_hash *hp, struct ncache *ncp)
{
	struct ncache *next = ncp->hash_next;
	struct ncache *prev = ncp->hash_prev;

	prev->hash_next = next;
	next->hash_prev = prev;

	ncp->hash_next = next = hp->hash_next;
	ncp->hash_prev = (struct ncache *)hp;
	next->hash_prev = ncp;
	hp->hash_next = ncp;

	ncstats.move_to_front++;
}

/*
 * Find an available entry to use.
 */
static struct ncache *
dnlc_get()
{
	struct ncache *ncp;
	struct nc_hash *hp;
	struct nc_hash *end;
	struct vnode *vp;

	/*
	 * Try getting an entry that has no identity. These entries
	 * are *not* in the hash chains
	 */
	if (dnlc_freelist) {
		mutex_enter(&dnlc_free_lock);
		if ((ncp = dnlc_freelist) != NULL) {
			dnlc_freelist = ncp->next_free;
			ncp->next_free = NULL;
			mutex_exit(&dnlc_free_lock);
			xncstats.xnc_pick_free++;
			return (ncp);
		}
		mutex_exit(&dnlc_free_lock);
	}
	/*
	 * Steal an entry
	 */
	hp = end = dnlc_free_rotor;
	do {
		if (++hp == &nc_hash[nc_hashsz])
			hp = nc_hash;
		dnlc_free_rotor = hp;
		if (hp->hash_next == (struct ncache *)hp)
			continue;
		mutex_enter(&hp->hash_lock);
		for (ncp = hp->hash_prev;
		    ncp != (struct ncache *)hp;
		    ncp = ncp->hash_prev) {
			vp = ncp->vp;
			if ((vp->v_pages == NULL) && (vp->v_count == 1))
				break;
		}

		if (ncp == (struct ncache *)hp) {
			ncp = hp->hash_prev;
			xncstats.xnc_pick_last++;
		} else
			xncstats.xnc_pick_heur++;

		if (ncp != (struct ncache *)hp) {
			vnode_t	*sdp = ncp->dp;
			vnode_t	*svp = ncp->vp;

			/*
			 * Remove from hash chain. Caller
			 * must initialize all fields
			 */
			nc_rmhash(ncp);
			mutex_exit(&hp->hash_lock);
			if (ncp->cred != NULL) {
				crfree(ncp->cred);
			}
			kmem_free(ncp->name, ncp->namlen);
			VN_RELE(sdp);
			VN_RELE(svp);
			return (ncp);
		}
		mutex_exit(&hp->hash_lock);
	} while (hp != end);
	return (NULL);
}

/*
 * Put an entry with no identity on the freelist.
 */
static void
dnlc_free(struct ncache *ncp)
{
	if (ncp->cred != NOCRED) {
		crfree(ncp->cred);
		ncp->cred = NOCRED;
	}
	ncp->dp = NULL;
	ncp->vp = NULL;
	kmem_free(ncp->name, ncp->namlen);
	mutex_enter(&dnlc_free_lock);
	ncp->next_free = dnlc_freelist;
	dnlc_freelist = ncp;
	mutex_exit(&dnlc_free_lock);
}
