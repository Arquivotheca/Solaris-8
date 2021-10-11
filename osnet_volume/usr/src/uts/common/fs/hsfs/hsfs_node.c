/*
 * Copyright (c) 1990,1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)hsfs_node.c 1.46     98/06/12 SMI"

/*
 * Directory operations for High Sierra filesystem
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/dnlc.h>
#include <sys/cmn_err.h>

#include <sys/kmem.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>
#include <vm/page.h>

#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/hsfs_spec.h>
#include <sys/fs/hsfs_isospec.h>
#include <sys/fs/hsfs_node.h>
#include <sys/fs/hsfs_impl.h>
#include <sys/fs/hsfs_susp.h>
#include <sys/fs/hsfs_rrip.h>

#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <fs/fs_subr.h>

#define	ISVDEV(t) ((t) == VCHR || (t) == VBLK || (t) == VFIFO)

/*
 * This macro expects a name that ends in '.' and returns TRUE if the
 * name is not "." or ".."
 */
#define	CAN_TRUNCATE_DOT(name, namelen)	\
		(namelen > 1 && (namelen > 2 || name[0] != '.'))

enum dirblock_result { FOUND_ENTRY, WENT_PAST, HIT_END };

extern struct vnodeops hsfs_vnodeops;

/*
 * These values determine whether we will try to read a file or dir;
 * they may be patched via /etc/system to allow users to read
 * record-oriented files.
 */
int ide_prohibited = IDE_PROHIBITED;
int hde_prohibited = HDE_PROHIBITED;

/*
 * This variable determines if the HSFS code will use the
 * directory name lookup cache. The default is for the cache to be used.
 */
static int hsfs_use_dnlc = 1;

/*
 * This variable determines whether strict ISO-9660 directory ordering
 * is to be assumed.  If false (which it is by default), then when
 * searching a directory of an ISO-9660 disk, we do not expect the
 * entries to be sorted (as the spec requires), and so cannot terminate
 * the search early.  Unfortunately, some vendors are producing
 * non-compliant disks.  This variable exists to revert to the old
 * behavior in case someone relies on this. This option is expected to be
 * removed at some point in the future.
 *
 * Use "set hsfs:strict_iso9660_ordering = 1" in /etc/system to override.
 */
static int strict_iso9660_ordering = 0;

static void hs_addfreeb(struct hstable *htp, struct hsnode *hp);
static void hs_addfreef(struct hstable *htp, struct hsnode *hp);
static int nmcmp(char *a, char *b, int len, int is_rrip);
static enum dirblock_result process_dirblock(struct fbuf *fbp, u_int *offset,
	u_int last_offset, char *nm, int nmlen, struct hsfs *fsp,
	struct hsnode *dhp, struct vnode *dvp, struct vnode **vpp,
	int *error, int is_rrip);
static int strip_trailing(struct hsfs *fsp, char *nm, int len);
static int uppercase_cp(char *from, char *to, int size);

/*
 * hs_access
 * Return 0 if the desired access may be granted.
 * Otherwise return error code.
 */
int
hs_access(struct vnode *vp, mode_t m, struct cred *cred)
{
	register struct hsnode *hp;
	int	error = 0;

	/*
	 * Write access cannot be granted for a read-only medium
	 */
	if ((m & VWRITE) && !ISVDEV(vp->v_type))
		return (EROFS);

	if (cred->cr_uid == 0)
		return (0);		/* super-user always gets access */

	hp = VTOH(vp);

	/*
	 * XXX - For now, use volume protections.
	 *  Also, always grant EXEC access for directories
	 *  if READ access is granted.
	 */
	if ((vp->v_type == VDIR) && (m & VEXEC)) {
		m &= ~VEXEC;
		m |= VREAD;
	}
	if (cred->cr_uid != hp->hs_dirent.uid) {
		m >>= 3;
		if (!groupmember((uid_t)hp->hs_dirent.gid, cred))
			m >>= 3;
	}
	if ((m & hp->hs_dirent.mode) != m)
		error = EACCES;
	return (error);
}

#if ((HS_HASHSIZE & (HS_HASHSIZE - 1)) == 0)
#define	HS_HASH(l)	((uintptr_t)(l) & (HS_HASHSIZE - 1))
#else
#define	HS_HASH(l)	((uintptr_t)(l) % HS_HASHSIZE)
#endif
#define	HS_HPASH(hp)	HS_HASH((hp)->hs_nodeid)

int nhsnode = 0;	/* # hsnodes to allocate for each filesystem */

/*
 * initialize incore hsnode table size
 *
 */
struct hstable *
hs_inithstbl(struct vfs *vfsp)
{
	register struct hstable *htp;
	size_t size;
	int i;
	int nohsnode;

	/*
	 * If the number of hsnodes has been specified in
	 * /etc/system, use that value, else allocate incore
	 * hsnode space based on memory size:
	 * minimum 16 K for 4M machine, 64K for others.
	 *
	 * XXX -- this really should be done a per-fs basis.
	 */
	if (nhsnode)
		size = nhsnode * sizeof (struct hsnode);
	else {
		size = (physmem <= 512) ? HS_HSTABLESIZE : 4 * HS_HSTABLESIZE;
		nhsnode = size / sizeof (struct hsnode);
	}

	htp = kmem_alloc(size, KM_SLEEP);
	htp->hs_vfsp = vfsp;
	htp->hs_tablesize = (int)size;
	for (i = 0; i < HS_HASHSIZE; i++)
		htp->hshash[i] = NULL;
	htp->hsfree_f = NULL;
	htp->hsfree_b = NULL;
	htp->hs_nohsnode = nohsnode = (int)(size -
	    sizeof (struct hstable) +
	    sizeof (struct hsnode)) /
	    sizeof (struct hsnode);

	mutex_init(&htp->hsfree_lock, NULL, MUTEX_DEFAULT, NULL);
	rw_init(&htp->hshash_lock, NULL, RW_DEFAULT, NULL);

	for (i = 0; i < nohsnode; i++) {
		htp->hs_node[i].hs_dirent.ext_lbn = 0;
		htp->hs_node[i].hs_hash = NULL;
		htp->hs_node[i].hs_freef = NULL;
		htp->hs_node[i].hs_freeb = NULL;
		htp->hs_node[i].hs_nodeid = 0;
		htp->hs_node[i].hs_flags = 0;
		htp->hs_node[i].hs_dirent.sym_link = (char *)NULL;
		hs_addfreeb(htp, &htp->hs_node[i]);
	}

	/*
	 * could initialize more stuff in this routine (e.g. vnode)
	 * do it next time...
	 */

	return (htp);
}

/*
 * free up incore hsnode table
 *
 */
void
hs_freehstbl(struct vfs *vfsp)
{
	register struct hstable *htp;

	htp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_hstbl;

	mutex_destroy(&htp->hsfree_lock);
	rw_destroy(&htp->hshash_lock);
	kmem_free(htp, (size_t)htp->hs_tablesize);
}

/*
 * Add an hsnode to the end of the free list.
 */
static void
hs_addfreeb(struct hstable *htp, struct hsnode *hp)
{
	register struct hsnode *ep;

	mutex_enter(&htp->hsfree_lock);
	ep = htp->hsfree_b;
	htp->hsfree_b = hp;		/* hp is the last entry in free list */
	hp->hs_freef = NULL;
	hp->hs_freeb = ep;		/* point at previous last entry */
	if (ep == NULL)
		htp->hsfree_f = hp;	/* hp is only entry in free list */
	else
		ep->hs_freef = hp;	/* point previous last entry at hp */

	mutex_exit(&htp->hsfree_lock);
}

/*
 * Add an hsnode to the front of the free list.
 */
static void
hs_addfreef(struct hstable *htp, struct hsnode *hp)
{
	register struct hsnode *ep;

	mutex_enter(&htp->hsfree_lock);
	ep = htp->hsfree_f;
	htp->hsfree_f = hp;		/* hp is the first entry in free list */
	hp->hs_freeb = NULL;
	hp->hs_freef = ep;		/* point at previous last entry */
	if (ep == NULL)
		htp->hsfree_b = hp;	/* hp is only entry in free list */
	else
		ep->hs_freeb = hp;	/* point previous last entry at hp */

	mutex_exit(&htp->hsfree_lock);
}

/*
 * Get an hsnode from the front of the free list.
 * Must be called with write hshash_lock held.
 */
static struct hsnode *
hs_getfree(struct hstable *htp)
{
	register struct hsnode *hp, **tp;

	ASSERT(RW_WRITE_HELD(&htp->hshash_lock));

	mutex_enter(&htp->hsfree_lock);
	hp = htp->hsfree_f;
	if (hp != NULL) {
		htp->hsfree_f = hp->hs_freef;
		if (htp->hsfree_f != NULL)
			htp->hsfree_f->hs_freeb = NULL;
		else
			htp->hsfree_b = NULL;
	}
	mutex_exit(&htp->hsfree_lock);
	if (hp == NULL)
		return (NULL);

	for (tp = &htp->hshash[HS_HPASH(hp)]; *tp != NULL;
		tp = &(*tp)->hs_hash) {
		if (*tp == hp) {
			struct vnode *vp;

			vp = HTOV(hp);	/* XXX need to VN_HOLD() here? */

			/*
			 * file is no longer reference, destroy all old pages
			 */
			if (vp->v_pages != NULL)
				/*
				 * pvn_vplist_dirty will abort all old pages
				 */
				(void) pvn_vplist_dirty(vp, (u_offset_t)0,
				hsfs_putapage, B_INVAL, (struct cred *)NULL);
			*tp = hp->hs_hash;
			break;
		}
	}

	return (hp);
}

/*
 * Remove an hsnode from the free list.
 */
static void
hs_remfree(struct hstable *htp, struct hsnode *hp)
{
	mutex_enter(&htp->hsfree_lock);
	if (hp->hs_freef != NULL)
		hp->hs_freef->hs_freeb = hp->hs_freeb;
	else
		htp->hsfree_b = hp->hs_freeb;
	if (hp->hs_freeb != NULL)
		hp->hs_freeb->hs_freef = hp->hs_freef;
	else
		htp->hsfree_f = hp->hs_freef;
	mutex_exit(&htp->hsfree_lock);
}

/*
 * Look for hsnode in hash list.
 * Check equality of fsid and nodeid.
 * If found, reactivate it if inactive.
 * Must be entered with hshash_lock held.
 */
struct vnode *
hs_findhash(u_long nodeid, struct vfs *vfsp)
{
	register struct hsnode *tp;
	register struct hstable *htp;

	htp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_hstbl;

	ASSERT(RW_LOCK_HELD(&htp->hshash_lock));

	for (tp = htp->hshash[HS_HASH(nodeid)]; tp != NULL; tp = tp->hs_hash) {
		if (tp->hs_nodeid == nodeid) {
			struct vnode *vp;

			mutex_enter(&tp->hs_contents_lock);
			vp = HTOV(tp);
			VN_HOLD(vp);
			if ((tp->hs_flags & HREF) == 0) {
				tp->hs_flags |= HREF;
				/*
				 * reactivating a free hsnode:
				 * remove from free list
				 */
				hs_remfree(htp, tp);
			}
			mutex_exit(&tp->hs_contents_lock);
			return (vp);
		}
	}
	return (NULL);
}

static void
hs_addhash(struct hstable *htp, struct hsnode *hp)
{
	register u_long hashno;

	ASSERT(RW_WRITE_HELD(&htp->hshash_lock));

	hashno = HS_HPASH(hp);
	hp->hs_hash = htp->hshash[hashno];
	htp->hshash[hashno] = hp;
}

/* destroy all old pages */
int
hs_synchash(struct vfs *vfsp)
{
	register struct hstable *htp;
	register int i;
	register struct hsnode *hp;
	int busy = 0;
	register struct vnode *vp, *rvp;

	htp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_hstbl;
	rvp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_rootvp;
	/* make sure no one can come in */
	rw_enter(&htp->hshash_lock, RW_WRITER);
	for (i = 0; i < HS_HASHSIZE; i++) {
		for (hp = htp->hshash[i]; hp != NULL; hp = hp->hs_hash) {
			vp = HTOV(hp);
			if ((hp->hs_flags & HREF) && (vp != rvp ||
				(vp == rvp && vp->v_count > 1))) {
				busy = 1;
				continue;
			}
			if (vp->v_pages != NULL)
				(void) pvn_vplist_dirty(vp, (u_offset_t)0,
				hsfs_putapage, B_INVAL, (struct cred *)NULL);
		}
	}
	if (busy) {
		rw_exit(&htp->hshash_lock);
		return (busy);
	}

	/* now destroy all hsnode content locks and sym_link buffers */
	for (i = 0; i < HS_HASHSIZE; i++) {
		for (hp = htp->hshash[i]; hp != NULL; hp = hp->hs_hash) {
			vp = HTOV(hp);
			if (vp != rvp)
				mutex_destroy(&hp->hs_contents_lock);
			if (hp->hs_dirent.sym_link != (char *)NULL) {
				kmem_free(hp->hs_dirent.sym_link,
					(size_t)(hp->hs_dirent.ext_size + 1));
				hp->hs_dirent.sym_link = (char *)NULL;
			}

		}
	}

	rw_exit(&htp->hshash_lock);
	return (0);
}


enum hs_try { HS_FIRST_TRY, HS_SECOND_TRY, HS_NO_MORE };

/*
 * Release hsnode's from the DNLC.
 * Returns 1 if successful in releasing some hsnodes, 0 otherwise
 * Although returning 1 does not guarantee that hsnodes of the desired
 * file system have been released, returning 0 does guarantee that no
 * hsnodes of the desired file system were held by the DNLC.
 *
 * Note that releasing a hsnode from the DNLC does not mean that the hsnode
 * is available; someone else may be VN_HOLD'ing it.
 *
 * The algorithm depends on which try this is.
 * For the first try, we try two methods of getting a hsnode.
 * The first method uses dnlc_fs_purge1 to release hsnodes from the DNLC.
 * Unfortunately, dnlc_fs_purge1 releases hsnodes based on file system *type*,
 * so no hsnodes for the particular file system may have been released.
 * The other problem with dnlc_fs_purge1 is that it only releases
 * vnodes that are referenced only by the DNLC *and* don't own any pages.
 * Therefore, we may not get any hsnodes from this method.
 * The second method uses the dnlc_purge_vfsp to release *all* hsnodes
 * of the specified file system. If this method fails, this means we
 * are out of hsnodes.
 * For the second try, we employ only the second method.
 *
 * The reason for making two tries is that dnlc_fs_purge1 is good at
 * throwing away uninteresting hsnodes, while dnlc_purge_vfsp unconditionally
 * releases all hsnodes of the specified file system.
 *
 * XXX: Some generic way of telling the DNLC what vnodes to release is required.
 */
static int
hs_dnlc_get(struct hstable *htp, enum hs_try try)
{
	unsigned int released_hsnodes = 0;

	if (try == HS_FIRST_TRY) {
		while (dnlc_fs_purge1(&hsfs_vnodeops))
			released_hsnodes++;
		/*
		 * Note that the released hsnodes may not belong to this
		 * file system. This means we may be called again and
		 * that time we will use the dnlc_purge_vfsp to get hsnodes.
		 * This may happen anyway, in the case that we release just
		 * one hsnode of this file system but another thread
		 * manages to get it before we do.
		 */
		if (released_hsnodes)
			return (1);
	}

	released_hsnodes = dnlc_purge_vfsp(htp->hs_vfsp, 0);
	return ((int)released_hsnodes > 0);
}


/*
 * hs_makenode
 *
 * Construct an hsnode.
 * Caller specifies the directory entry, the block number and offset
 * of the directory entry, and the vfs pointer.
 * note: off is the sector offset, not lbn offset
 * if NULL is returned implies file system hsnode table full
 */
struct vnode *
hs_makenode(
	struct hs_direntry *dp,
	u_int lbn,
	u_int off,
	struct vfs *vfsp)
{
	register struct hsnode *hp;
	register struct vnode *vp;
	register struct hstable *htp;
	struct hs_volume *hvp;
	struct vnode *newvp;
	struct hsfs *fsp;
	u_long nodeid;

	fsp = VFS_TO_HSFS(vfsp);
	htp = fsp->hsfs_hstbl;

	/*
	 * Construct the nodeid: in the case of a directory
	 * entry, this should point to the canonical dirent, the "."
	 * directory entry for the directory.  This dirent is pointed
	 * to by all directory entries for that dir (including the ".")
	 * entry itself.
	 * In the case of a file, simply point to the dirent for that
	 * file (there are no hard links in Rock Ridge, so there's no
	 * need to determine what the canonical dirent is.
	 */
	if (dp->type == VDIR) {
		lbn = dp->ext_lbn;
		off = 0;
	}

	/*
	 * Normalize lbn and off before creating a nodeid
	 * and before storing them in a hs_node structure
	 */
	hvp = &fsp->hsfs_vol;
	lbn += off >> hvp->lbn_shift;
	off &= hvp->lbn_maxoffset;
	nodeid = MAKE_NODEID(lbn, off, vfsp);

	/* look for hsnode in cache first */

	rw_enter(&htp->hshash_lock, RW_READER);

	if ((vp = hs_findhash(nodeid, vfsp)) == NULL) {

		/*
		 * Not in cache.  However, someone else may have come
		 * to the same conclusion and just put one in.	Upgrade
		 * our lock to a write lock and look again.
		 */
		rw_exit(&htp->hshash_lock);
		rw_enter(&htp->hshash_lock, RW_WRITER);

		if ((vp = hs_findhash(nodeid, vfsp)) == NULL) {
			/*
			 * Now we are really sure that the hsnode is not
			 * in the cache.  Get one off freelist or else
			 * allocate one.
			 */
			enum hs_try try = HS_FIRST_TRY;

			for (;;) {
				if (hp = hs_getfree(htp))
					break;
				rw_exit(&htp->hshash_lock);

				/*
				 * Quit if
				 *   a) we don't use the DNLC, or
				 *   b) we do use the DNLC but we have already
				 *	tried to get a hsnode from the DNLC, or
				 *   c) we can't get any hsnodes from the DNLC
				 *
				 * Note that we must release the hshash_lock
				 * because hsfs_inactive will be invoked if
				 * any hsnodes are released and hsfs_inactive
				 * acquires that lock.
				 */
				if (!hsfs_use_dnlc ||
					try == HS_NO_MORE ||
					hs_dnlc_get(htp, try) == 0) {
					cmn_err(CE_NOTE,
			"hsfs: hsnode table full, %d nodes allocated,",
						nhsnode);
					cmn_err(CE_CONT,
			"hsfs: consider increasing # of hsnodes using\n");
					cmn_err(CE_CONT,
			"hsfs: 'set hsfs:nhsnode=num' in /etc/system\n");
					cmn_err(CE_CONT,
						"hsfs: and rebooting.\n");
					return (NULL);
				}
				rw_enter(&htp->hshash_lock, RW_WRITER);
				try++;
			}

			/*
			 * Holding hshash_lock, will
			 * kmem_free screw us up?
			 */

			if (hp->hs_dirent.sym_link != (char *)NULL) {
				kmem_free(hp->hs_dirent.sym_link,
					(size_t)(hp->hs_dirent.ext_size + 1));
				hp->hs_dirent.sym_link = (char *)NULL;
			}

			if (hp->hs_dirent.ext_lbn == 0) {
				bzero((caddr_t)hp, sizeof (*hp));
				mutex_init(&hp->hs_contents_lock,
				    NULL, MUTEX_DEFAULT, NULL);
			}
			else
				bzero((caddr_t)hp, sizeof (*hp));
			bcopy((caddr_t)dp, (caddr_t)&hp->hs_dirent,
				sizeof (*dp));
			/*
			 * We've just copied this pointer into hs_dirent,
			 * and don't want 2 references to same symlink.
			 */
			dp->sym_link = (char *)NULL;
			/*
			 * No need to hold any lock because hsnode is not
			 * in the hash chain.
			 */

			hp->hs_dir_lbn = lbn;
			hp->hs_dir_off = off;
			hp->hs_nodeid = nodeid;
			hp->hs_offset = 0;
			hp->hs_mapcnt = 0;	/* just want to be safe */
			hp->hs_vcode = 0;	/* ditto */
			hp->hs_flags = HREF;
			if (off > HS_SECTOR_SIZE)
				cmn_err(CE_WARN, "hs_makenode: bad offset");

			/* initialize for VDIR */
			hp->hs_ptbl_idx = NULL;
			vp = HTOV(hp);
			VN_INIT(vp, vfsp, dp->type, dp->r_dev);
			vp->v_op = &hsfs_vnodeops;
			vp->v_data = (caddr_t)hp;

			/*
			 * if it's a device, call specvp
			 */
			if (ISVDEV(vp->v_type)) {
				rw_exit(&htp->hshash_lock);
				newvp = specvp(vp, vp->v_rdev, vp->v_type,
						CRED());
				if (newvp == NULL)
				    cmn_err(CE_NOTE,
					"hs_makenode: specvp failed");
				VN_RELE(vp);
				return (newvp);
			}
			/*
			 * if this is a swap device, mark it as such
			 */
			if ((hp->hs_dirent.mode & ISVTX) &&
			    !(hp->hs_dirent.mode & (IEXEC | IFDIR)))
				vp->v_flag |= VISSWAP;
			else
				vp->v_flag &= ~VISSWAP;


			hs_addhash(htp, hp);

		}
	}

	if (dp->sym_link != (char *)NULL) {
		kmem_free(dp->sym_link, (size_t)(dp->ext_size + 1));
		dp->sym_link = (char *)NULL;
	}

	rw_exit(&htp->hshash_lock);
	return (vp);
}

/*
 * hs_freenode
 *
 * Deactivate an hsnode.
 * Leave it on the hash list but put it on the free list.
 * if the vnode does not have any pages, put in front of free list
 * else put in back of the free list
 *
 */
void
hs_freenode(
	struct hsnode *hp,
	struct vfs *vfsp,
	int nopage)		/* 1 if no page, 0 otherwise */
{
	struct hstable *htp;

	htp = ((struct hsfs *)VFS_TO_HSFS(vfsp))->hsfs_hstbl;
	if (nopage)
		hs_addfreef(htp, hp); /* add to front of free list */
	else
		hs_addfreeb(htp, hp); /* add to back of free list */
}

/*
 * hs_remakenode
 *
 * Reconstruct a vnode given the location of its directory entry.
 * Caller specifies the the block number and offset
 * of the directory entry, and the vfs pointer.
 * Returns an error code or 0.
 */
int
hs_remakenode(u_int lbn, u_int off, struct vfs *vfsp,
    struct vnode **vpp)
{
	struct buf *secbp;
	struct hsfs *fsp;
	u_int secno;
	u_char *dirp;
	struct hs_direntry hd;
	int error;

	/* Convert to sector and offset */
	fsp = VFS_TO_HSFS(vfsp);
	if (off > HS_SECTOR_SIZE) {
		cmn_err(CE_WARN, "hs_remakenode: bad offset");
		error = EINVAL;
		goto end;
	}
	secno = LBN_TO_SEC(lbn, vfsp);
	secbp = bread(fsp->hsfs_devvp->v_rdev, secno * 4, HS_SECTOR_SIZE);

	error = geterror(secbp);
	if (error != 0) {
		cmn_err(CE_NOTE, "hs_remakenode: bread: error=(%d)", error);
		goto end;
	}

	dirp = (u_char *)secbp->b_un.b_addr;
	error = hs_parsedir(fsp, &dirp[off], &hd, (char *)NULL, (int *)NULL);
	if (!error) {
		*vpp = hs_makenode(&hd, lbn, off, vfsp);
		if (*vpp == NULL)
			error = ENFILE;
	}

end:
	brelse(secbp);
	return (error);
}


/*
 * hs_dirlook
 *
 * Look for a given name in a given directory.
 * If found, construct an hsnode for it.
 */
int
hs_dirlook(
	struct vnode	*dvp,
	char		*name,
	int		namlen,		/* length of 'name' */
	struct vnode	**vpp,
	struct cred	*cred)
{
	register struct hsnode *dhp;
	struct hsfs	*fsp;
	int		error = 0;
	u_int		offset;		/* real offset in directory */
	u_int		last_offset;	/* last index into current dir block */
	char		*cmpname;	/* case-folded name */
	int		cmpname_size;	/* how much memory we allocate for it */
	int		cmpnamelen;
	int		adhoc_search;	/* did we start at begin of dir? */
	int		end;
	u_int		hsoffset;
	struct fbuf	*fbp;
	int		bytes_wanted;
	int		dirsiz;
	int		is_rrip;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if (error = hs_access(dvp, (mode_t)VEXEC, cred))
		return (error);

	if (hsfs_use_dnlc && (*vpp = dnlc_lookup(dvp, name, NOCRED)))
		return (0);

	dhp = VTOH(dvp);
	fsp = VFS_TO_HSFS(dvp->v_vfsp);

	cmpname_size = (int)(fsp->hsfs_namemax + 1);
	cmpname = kmem_alloc((size_t)cmpname_size, KM_SLEEP);

	is_rrip = IS_RRIP_IMPLEMENTED(fsp);

	if (namlen >= cmpname_size)
		namlen = cmpname_size - 1;
	/*
	 * For the purposes of comparing the name against dir entries,
	 * fold it to upper case.
	 */
	if (is_rrip) {
		(void) strcpy(cmpname, name);
		cmpnamelen = namlen;
	} else {
		/*
		 * If we don't consider a trailing dot as part of the filename,
		 * remove it from the specified name
		 */
		if ((fsp->hsfs_flags & HSFSMNT_NOTRAILDOT) &&
			name[namlen-1] == '.' &&
				CAN_TRUNCATE_DOT(name, namlen))
			name[--namlen] = '\0';
		cmpnamelen = hs_uppercase_copy(name, cmpname, namlen);
	}

	/* make sure dirent is filled up with all info */
	if (dhp->hs_dirent.ext_size == 0)
		hs_filldirent(dvp, &dhp->hs_dirent);

	/*
	 * No lock is needed - hs_offset is used as starting
	 * point for searching the directory.
	 */
	offset = dhp->hs_offset;
	hsoffset = offset;
	adhoc_search = (offset != 0);

	end = dhp->hs_dirent.ext_size;
	dirsiz = end;

tryagain:

	while (offset < end) {

		if ((offset & MAXBMASK) + MAXBSIZE > dirsiz)
			bytes_wanted = dirsiz - (offset & MAXBMASK);
		else
			bytes_wanted = MAXBSIZE;

		error = fbread(dvp, (offset_t)(offset & MAXBMASK),
			(unsigned int)bytes_wanted, S_READ, &fbp);
		if (error)
			goto done;

		last_offset = (offset & MAXBMASK) + fbp->fb_count - 1;

#define	rel_offset(offset) ((offset) & MAXBOFFSET)  /* index into cur blk */

		switch (process_dirblock(fbp, &offset,
					last_offset, cmpname,
					cmpnamelen, fsp, dhp, dvp,
					vpp, &error, is_rrip)) {
		case FOUND_ENTRY:
			/* found an entry, either correct or not */
			goto done;

		case WENT_PAST:
			/*
			 * If we get here we know we didn't find it on the
			 * first pass. If adhoc_search, then we started a
			 * bit into the dir, and need to wrap around and
			 * search the first entries.  If not, then we started
			 * at the beginning and didn't find it.
			 */
			if (adhoc_search) {
				offset = 0;
				end = hsoffset;
				adhoc_search = 0;
				goto tryagain;
			}
			error = ENOENT;
			goto done;

		case HIT_END:
			goto tryagain;
		}
	}
	/*
	 * End of all dir blocks, didn't find entry.
	 */
	if (adhoc_search) {
		offset = 0;
		end = hsoffset;
		adhoc_search = 0;
		goto tryagain;
	}
	error = ENOENT;
done:
	/*
	 * If we found the entry, add it to the DNLC
	 * If the entry is a device file (assuming we support Rock Ridge),
	 * we enter the device vnode to the cache since that is what
	 * is in *vpp.
	 * That is ok since the CD-ROM is read-only, so (dvp,name) will
	 * always point to the same device.
	 */
	if (hsfs_use_dnlc && !error)
		dnlc_enter(dvp, name, *vpp, NOCRED);

	kmem_free(cmpname, (size_t)cmpname_size);

	return (error);
}

/*
 * hs_parsedir
 *
 * Parse a Directory Record into an hs_direntry structure.
 * High Sierra and ISO directory are almost the same
 * except the flag and date
 */
int
hs_parsedir(
	struct hsfs		*fsp,
	u_char			*dirp,
	struct hs_direntry	*hdp,
	char			*dnp,
	int			*dnlen)
{
	char	*on_disk_name;
	int	on_disk_namelen;
	u_char	flags;
	int	namelen;
	int	error;
	int	name_change_flag = 0;	/* set if name was gotten in SUA */

	hdp->ext_lbn = HDE_EXT_LBN(dirp);
	hdp->ext_size = HDE_EXT_SIZE(dirp);
	hdp->xar_len = HDE_XAR_LEN(dirp);
	hdp->intlf_sz = HDE_INTRLV_SIZE(dirp);
	hdp->intlf_sk = HDE_INTRLV_SKIP(dirp);
	hdp->sym_link = (char *)NULL;

	if (fsp->hsfs_vol_type == HS_VOL_TYPE_HS) {
		flags = HDE_FLAGS(dirp);
		hs_parse_dirdate(HDE_cdate(dirp), &hdp->cdate);
		hs_parse_dirdate(HDE_cdate(dirp), &hdp->adate);
		hs_parse_dirdate(HDE_cdate(dirp), &hdp->mdate);
		if ((flags & hde_prohibited) == 0) {
			/*
			 * Skip files with the associated bit set.
			 */
			if (flags & HDE_ASSOCIATED)
				return (EAGAIN);
			hdp->type = VREG;
			hdp->mode = IFREG;
			hdp->nlink = 1;
		} else if ((flags & hde_prohibited) == HDE_DIRECTORY) {
			hdp->type = VDIR;
			hdp->mode = IFDIR;
			hdp->nlink = 2;
		} else {
			hs_log_bogus_disk_warning(fsp,
				HSFS_ERR_UNSUP_TYPE, flags);
			return (EINVAL);
		}
		hdp->uid = fsp -> hsfs_vol.vol_uid;
		hdp->gid = fsp -> hsfs_vol.vol_gid;
		hdp->mode = hdp-> mode | (fsp -> hsfs_vol.vol_prot & 0777);
	} else if (fsp->hsfs_vol_type == HS_VOL_TYPE_ISO) {
		flags = IDE_FLAGS(dirp);
		hs_parse_dirdate(IDE_cdate(dirp), &hdp->cdate);
		hs_parse_dirdate(IDE_cdate(dirp), &hdp->adate);
		hs_parse_dirdate(IDE_cdate(dirp), &hdp->mdate);

		if ((flags & ide_prohibited) == 0) {
			/*
			 * Skip files with the associated bit set.
			 */
			if (flags & IDE_ASSOCIATED)
				return (EAGAIN);
			hdp->type = VREG;
			hdp->mode = IFREG;
			hdp->nlink = 1;
		} else if ((flags & ide_prohibited) == IDE_DIRECTORY) {
			hdp->type = VDIR;
			hdp->mode = IFDIR;
			hdp->nlink = 2;
		} else {
			hs_log_bogus_disk_warning(fsp,
				HSFS_ERR_UNSUP_TYPE, flags);
			return (EINVAL);
		}
		hdp->uid = fsp -> hsfs_vol.vol_uid;
		hdp->gid = fsp -> hsfs_vol.vol_gid;
		hdp->mode = hdp-> mode | (fsp -> hsfs_vol.vol_prot & 0777);

		/*
		 * Having this all filled in, let's see if we have any
		 * SUA susp to look at.
		 */
		if (IS_SUSP_IMPLEMENTED(fsp)) {
			error = parse_sua((u_char *)dnp, dnlen,
					&name_change_flag, dirp, hdp, fsp,
					(u_char *)NULL, NULL);
			if (error) {
				if (hdp->sym_link) {
					kmem_free(hdp->sym_link,
						(size_t)(hdp->ext_size + 1));
					hdp->sym_link = (char *)NULL;
				}
				return (error);
			}
		}
	}
	hdp->xar_prot = (HDE_PROTECTION & flags) != 0;

#if dontskip
	if (hdp->xar_len > 0) {
		cmn_err(CE_NOTE, "hsfs: extended attributes not supported");
		return (EINVAL);
	}
#endif

	/* check interleaf size and skip factor */
	/* must both be zero or non-zero */
	if (hdp->intlf_sz + hdp->intlf_sk) {
		if ((hdp->intlf_sz == 0) || (hdp->intlf_sk == 0)) {
			cmn_err(CE_NOTE,
				"hsfs: interleaf size or skip factor error");
			return (EINVAL);
		}
		if (hdp->ext_size == 0) {
			cmn_err(CE_NOTE,
			    "hsfs: interleaving specified on zero length file");
			return (EINVAL);
		}
	}

	if (HDE_VOL_SET(dirp) != 1) {
		if (fsp->hsfs_vol.vol_set_size != 1 &&
		    fsp->hsfs_vol.vol_set_size != HDE_VOL_SET(dirp)) {
			cmn_err(CE_NOTE, "hsfs: multivolume file?");
			return (EINVAL);
		}
	}

	/*
	 * If the name changed, then the NM field for RRIP was hit and
	 * we should not copy the name again, just return.
	 */
	if (NAME_HAS_CHANGED(name_change_flag))
		return (0);

	/* return the pointer to the directory name and its length */
	on_disk_name = (char *)HDE_name(dirp);
	on_disk_namelen = (int)HDE_NAME_LEN(dirp);

	if (((int)(fsp->hsfs_namemax) > 0) &&
			(on_disk_namelen > (int)(fsp->hsfs_namemax))) {
		hs_log_bogus_disk_warning(fsp, HSFS_ERR_BAD_FILE_LEN, 0);
		on_disk_namelen = fsp->hsfs_namemax;
	}
	if (dnp != NULL) {
		namelen = hs_namecopy(on_disk_name, dnp, on_disk_namelen,
		    fsp->hsfs_flags);
		if ((fsp->hsfs_flags & HSFSMNT_NOTRAILDOT) &&
		    dnp[ namelen-1 ] == '.' && CAN_TRUNCATE_DOT(dnp, namelen))
			dnp[ --namelen ] = '\0';
	} else
		namelen = on_disk_namelen;
	if (dnlen != NULL)
		*dnlen = namelen;

	return (0);
}

/*
 * hs_namecopy
 *
 * Parse a file/directory name into UNIX form.
 * Delete trailing blanks, upper-to-lower case, add NULL terminator.
 * Returns the (possibly new) length.
 */
int
hs_namecopy(char *from, char *to, int size, u_long flags)
{
	register u_int i;
	register u_char c;
	register int lastspace;
	register int maplc;

	/* special handling for '.' and '..' */
	if (size == 1) {
		if (*from == '\0') {
			*to++ = '.';
			*to = '\0';
			return (1);
		} else if (*from == '\1') {
			*to++ = '.';
			*to++ = '.';
			*to = '\0';
			return (2);
		}
	}

	maplc = (flags & HSFSMNT_NOMAPLCASE) == 0;
	for (i = 0, lastspace = -1; i < size; i++) {
		c = from[i] & 0x7f;
		if (c == ';')
			break;
		if (c <= ' ') {
			if (lastspace == -1)
				lastspace = i;
		} else
			lastspace = -1;
		if (maplc && (c >= 'A') && (c <= 'Z'))
			c += 'a' - 'A';
		to[i] = c;
	}
	if (lastspace != -1)
		i = lastspace;
	to[i] = '\0';
	return (i);
}

/*
 * map a filename to upper case;
 * return 1 if found lowercase character
 */
static int
uppercase_cp(char *from, char *to, int size)
{
	register u_int i;
	register u_char c;
	register u_char had_lc = 0;

	for (i = 0; i < size; i++) {
		c = *from++;
		if ((c >= 'a') && (c <= 'z')) {
			c -= ('a' - 'A');
			had_lc = 1;
		}
		*to++ = c;
	}
	return (had_lc);
}

/*
 * hs_uppercase_copy
 *
 * Convert a UNIX-style name into its HSFS equivalent.
 * Map to upper case.
 * Returns the (possibly new) length.
 */
int
hs_uppercase_copy(char *from, char *to, int size)
{
	register u_int i;
	register u_char c;

	/* special handling for '.' and '..' */

	if (size == 1 && *from == '.') {
		*to = '\0';
		return (1);
	} else if (size == 2 && *from == '.' && *(from+1) == '.') {
		*to = '\1';
		return (1);
	}

	for (i = 0; i < size; i++) {
		c = *from++;
		if ((c >= 'a') && (c <= 'z'))
			c = c - 'a' + 'A';
		*to++ = c;
	}
	return (size);
}

void
hs_filldirent(struct vnode *vp, struct hs_direntry *hdp)
{
	register struct buf *secbp;
	u_int	secno;
	u_int	secoff;
	struct hsfs *fsp;
	u_char *secp;
	int	error;

	if (vp->v_type != VDIR) {
		cmn_err(CE_WARN, "hsfs_filldirent: vp (0x%p) not a directory",
			(void *)vp);
		return;
	}

	fsp = VFS_TO_HSFS(vp ->v_vfsp);
	secno = LBN_TO_SEC(hdp->ext_lbn+hdp->xar_len, vp->v_vfsp);
	secoff = LBN_TO_BYTE(hdp->ext_lbn+hdp->xar_len, vp->v_vfsp) &
			MAXHSOFFSET;
	secbp = bread(fsp->hsfs_devvp->v_rdev, secno * 4, HS_SECTOR_SIZE);
	error = geterror(secbp);
	if (error != 0) {
		cmn_err(CE_NOTE, "hs_filldirent: bread: error=(%d)", error);
		goto end;
	}

	secp = (u_char *)secbp->b_un.b_addr;

	/* quick check */
	if (hdp->ext_lbn != HDE_EXT_LBN(&secp[secoff])) {
		cmn_err(CE_NOTE, "hsfs_filldirent: dirent not match");
		/* keep on going */
	}
	(void) hs_parsedir(fsp, &secp[secoff], hdp, (char *)NULL, (int *)NULL);

end:
	brelse(secbp);
}

/*
 * Look through a directory block for a matching entry.
 * Note: this routine does an fbrelse() on the buffer passed in.
 */
static enum dirblock_result
process_dirblock(
	struct fbuf	*fbp,		/* buffer containing dirblk */
	u_int		*offset,	/* lower index */
	u_int		last_offset,	/* upper index */
	char		*nm,		/* upcase nm to compare against */
	int		nmlen,		/* length of name */
	struct hsfs	*fsp,
	struct hsnode	*dhp,
	struct vnode	*dvp,
	struct vnode	**vpp,
	int		*error,		/* return value: errno */
	int		is_rrip)	/* 1 if rock ridge is implemented */
{
	u_char		*blkp = (u_char *)fbp->fb_addr; /* dir block */
	char		*dname;		/* name in directory entry */
	int		dnamelen;	/* length of name */
	struct hs_direntry hd;
	int		hdlen;
	register u_char *dirp;	/* the directory entry */
	int		res;
	int		parsedir_res;
	size_t		rrip_name_size;
	int		rr_namelen;
	char		*rrip_name_str = NULL;
	char		*rrip_tmp_name = NULL;
	enum dirblock_result err = 0;
	int 		did_fbrelse = 0;
	char		uppercase_name[ISO_FILE_NAMELEN];

	/* return after performing cleanup-on-exit */
#define	PD_return(retval)	{ err = retval; goto do_ret; }

	if (is_rrip) {
		rrip_name_size = RRIP_FILE_NAMELEN + 1;
		rrip_name_str = kmem_alloc(rrip_name_size, KM_SLEEP);
		rrip_tmp_name = kmem_alloc(rrip_name_size, KM_SLEEP);
		rrip_name_str[0] = '\0';
		rrip_tmp_name[0] = '\0';
	}

	while (*offset < last_offset) {

		/*
		 * Directory Entries cannot span sectors.
		 * Unused bytes at the end of each sector are zeroed.
		 * Therefore, detect this condition when the size
		 * field of the directory entry is zero.
		 */
		hdlen = (int)((u_char)
				HDE_DIR_LEN(&blkp[rel_offset(*offset)]));
		if (hdlen == 0) {
			/* advance to next sector boundary */
			*offset = (*offset & MAXHSMASK) + HS_SECTOR_SIZE;

			if (*offset > last_offset) {
				/* end of block */
				PD_return(HIT_END)
			} else
				continue;
		}

		/*
		 * Zero out the hd to read new directory
		 */
		bzero(&hd, sizeof (hd));

		/*
		 * Just ignore invalid directory entries.
		 * XXX - maybe hs_parsedir() will detect EXISTENCE bit
		 */
		dirp = &blkp[rel_offset(*offset)];
		dname = (char *)HDE_name(dirp);
		dnamelen = (int)((u_char)HDE_NAME_LEN(dirp));
		if (dnamelen > (int)(fsp->hsfs_namemax)) {
			hs_log_bogus_disk_warning(fsp,
				HSFS_ERR_BAD_FILE_LEN, 0);
			dnamelen = fsp->hsfs_namemax;
		}

		/*
		 * If the rock ridge is implemented, then we copy the name
		 * from the SUA area to rrip_name_str. If no Alternate
		 * name is found, then use the uppercase NM in the
		 * rrip_name_str char array.
		 */
		if (is_rrip) {

			rrip_name_str[0] = '\0';
			rr_namelen = rrip_namecopy(nm, &rrip_name_str[0],
			    &rrip_tmp_name[0], dirp, fsp, &hd);
			if (hd.sym_link) {
				kmem_free(hd.sym_link,
				    (size_t)(hd.ext_size+1));
				hd.sym_link = (char *)NULL;
			}

			if (rr_namelen != -1) {
				dname = (char *)&rrip_name_str[0];
				dnamelen = rr_namelen;
			}
		}

		if (!is_rrip || rr_namelen == -1) {
			/* use iso name instead */

			register int i;
			/*
			 * make sure that we get rid of ';' in the dname of
			 * an iso direntry, as we should have no knowledge
			 * of file versions.
			 */

			for (i = dnamelen - 1;
			    (dname[i] != ';') && (i > 0);
			    i--)
				continue;

			if (dname[i] == ';')
				dnamelen = i;
			else
				dnamelen = strip_trailing(fsp, dname, dnamelen);

			if (uppercase_cp(dname, uppercase_name, dnamelen))
				hs_log_bogus_disk_warning(fsp,
					HSFS_ERR_LOWER_CASE_NM, 0);
			dname = uppercase_name;
			if (! is_rrip &&
				(fsp->hsfs_flags & HSFSMNT_NOTRAILDOT) &&
					dname[ dnamelen-1 ] == '.' &&
					CAN_TRUNCATE_DOT(dname, dnamelen))
				dname[ --dnamelen ] = '\0';
		}

		/*
		 * Quickly screen for a non-matching entry, but not for RRIP.
		 * This test doesn't work for lowercase vs. uppercase names.
		 */

		/* if we saw a lower case name we can't do this test either */
		if (strict_iso9660_ordering && !is_rrip &&
			!HSFS_HAVE_LOWER_CASE(fsp) && *nm < *dname) {
			RESTORE_NM(rrip_tmp_name, nm);
			PD_return(WENT_PAST)
		}

		if (*nm != *dname || nmlen != dnamelen) {
			/* look at next dir entry */
			RESTORE_NM(rrip_tmp_name, nm);
			*offset += hdlen;
			continue;
		}

		if ((res = nmcmp(dname, nm, nmlen, is_rrip)) == 0) {
			/* name matches */
			parsedir_res =
				hs_parsedir(fsp, dirp, &hd, (char *)NULL,
					    (int *)NULL);
			if (!parsedir_res) {
				u_int lbn;	/* logical block number */

				lbn = dhp->hs_dirent.ext_lbn +
					dhp->hs_dirent.xar_len;
				/*
				 * Need to do an fbrelse() on the buffer,
				 * as hs_makenode() may try to acquire
				 * hs_hashlock, which may not be required
				 * while a page is locked.
				 */
				fbrelse(fbp, S_READ);
				did_fbrelse = 1;
				*vpp = hs_makenode(&hd, lbn,
						*offset, dvp->v_vfsp);
				if (*vpp == NULL) {
					*error = ENFILE;
					RESTORE_NM(rrip_tmp_name, nm);
					PD_return(FOUND_ENTRY)
				}

				dhp->hs_offset = *offset;
				RESTORE_NM(rrip_tmp_name, nm);
				PD_return(FOUND_ENTRY)
			} else if (parsedir_res != EAGAIN) {
				/* improper dir entry */
				*error = parsedir_res;
				RESTORE_NM(rrip_tmp_name, nm);
				PD_return(FOUND_ENTRY)
			}
		} else if (strict_iso9660_ordering && !is_rrip &&
			!HSFS_HAVE_LOWER_CASE(fsp) && res < 0) {
			/* name < dir entry */
			RESTORE_NM(rrip_tmp_name, nm);
			PD_return(WENT_PAST)
		}
		/*
		 * name > dir entry,
		 * look at next one.
		 */
		*offset += hdlen;
		RESTORE_NM(rrip_tmp_name, nm);
	}
	PD_return(HIT_END)

do_ret:
	if (rrip_name_str)
		kmem_free(rrip_name_str, rrip_name_size);
	if (rrip_tmp_name)
		kmem_free(rrip_tmp_name, rrip_name_size);
	if (! did_fbrelse)
		fbrelse(fbp, S_READ);
	return (err);
#undef PD_return
}


/*
 * Compare the names, returning < 0 if a < b,
 * 0 if a == b, and > 0 if a > b.
 */
static int
nmcmp(char *a, char *b, int len, int is_rrip)
{
	while (len--) {
		if (*a == *b) {
			b++; a++;
		} else {
			/* if file version, stop */
			if (! is_rrip && ((*a == ';') && (*b == '\0')))
				return (0);
			return ((u_char)*b - (u_char)*a);
		}
	}
	return (0);
}

/*
 * Strip trailing nulls or spaces from the name;
 * return adjusted length.  If we find such junk,
 * log a non-conformant disk message.
 */
static int
strip_trailing(struct hsfs *fsp, char *nm, int len)
{
	register char *c;
	register int trailing_junk = 0;

	for (c = nm + len - 1; c > nm; c--) {
		if (*c == ' ' || *c == '\0')
			trailing_junk = 1;
		else
			break;
	}

	if (trailing_junk)
		hs_log_bogus_disk_warning(fsp, HSFS_ERR_TRAILING_JUNK, 0);

	return ((int)(c - nm + 1));
}
