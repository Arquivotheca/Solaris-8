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
 * 	Copyright (c) 1986-1989,1997,1999 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)s5_dir.c	1.14	99/08/10 SMI"

/* from	"ufs_dir.c	2.55	90/01/02 SMI"  */

/*
 * Directory manipulation routines.
 *
 * We manipulating directories, the i_rwlock provides serialization
 * since directories cannot be mmapped. The i_contents lock is redundant.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/dnlc.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fs.h>
#include <sys/mount.h>
#include <sys/fs/s5_fsdir.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <vm/seg.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>

extern kmutex_t	rename_lock;
/*
 * A virgin directory.
 */
static struct dirtemplate mastertemplate = {
	0, ".",
	0, ".."
};

static void dirbad();
static int blkatoff();
static int s5_dircheckpath();
static int s5_dircheckforname();
static int s5_dirrename();
static int s5_dirmakeinode();
static int s5_dirempty();
static int s5_dirfixdotdot();
static int s5_dirprepareentry();
static int s5_dirmakedirect();

/*
 * Look for a given name in a directory.  On successful return, *ipp
 * will point to the VN_HELD inode.
 */
int
s5_dirlook(dp, namep, ipp, cr)
	register struct inode *dp;
	register char *namep;
	register struct inode **ipp;
	struct cred *cr;
{
	struct fbuf *fbp = NULL;	/* a buffer of directory entries */
	register struct direct *ep;	/* the current directory entry */
	register struct inode *ip;
	struct vnode *vp;
	int entryoffsetinblock;		/* offset of ep in addr's buffer */
	off_t endsearch;		/* offset to end directory search */
	int namlen;			/* length of name */
	off_t	offset;
	int error;

	/*
	 * Check accessibility of directory.
	 */
	if ((dp->i_mode & IFMT) != IFDIR)
		return (ENOTDIR);
	if (error = s5_iaccess(dp, IEXEC, cr))
		return (error);

	/*
	 * Null component name is synonym for directory being searched.
	 */
	if (*namep == '\0') {
		struct vnode *vp = ITOV(dp);

		VN_HOLD(vp);
		*ipp = dp;
		return (0);
	}

	/*
	 * Check the directory name lookup cache.
	 */
	if (vp = dnlc_lookup(ITOV(dp), namep, NOCRED)) {
		/* vp is already held from dnlc_lookup */

		*ipp = VTOI(vp);
		return (0);
	}

	/*
	 * Read lock the inode we are searching.  You will notice that we
	 * didn't hold the read lock while searching the dnlc.  This means
	 * that the entry could now be in the dnlc.  This doesn't cause any
	 * problems because dnlc_enter won't add an entry if it is already
	 * there.
	 */
	rw_enter(&dp->i_rwlock, RW_READER);

	offset = 0;
	endsearch = dp->i_size;
	namlen = strlen(namep);
	entryoffsetinblock = 0;		/* offset of ep in addr's buffer */

	while (offset < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if (blkoff(dp->i_s5vfs, offset) == 0) {
			if (fbp != NULL) {
				fbrelse(fbp, S_OTHER);
			}
			error = blkatoff(dp, offset, (char **)0, &fbp);
			if (error)
				goto bad;
			entryoffsetinblock = 0;
		}

		/*
		 * Get pointer to next entry.
		 * check for empty entry
		 */
		ep = (struct direct *)(fbp->fb_addr + entryoffsetinblock);
		if (ep->d_ino == 0) {
			offset += SDSIZ;
			entryoffsetinblock += SDSIZ;
			continue;
		}

		/*
		 * Check for a name match.
		 * We have the parent inode read locked with i_rwlock.
		 * Since s_iget doesn't attempt to grab the i_rwlock
		 * (just i_contents) we don't have to release our hold
		 * on i_rwlock when we traverse upward (..)
		 */
		if (ep->d_ino &&
		    *namep == *ep->d_name &&	/* fast chk 1st chr */
		    bcmp(namep, ep->d_name, (int)namlen) == 0 &&
		    (namlen == DIRSIZ || ep->d_name[namlen] == '\0')) {
			u_long ep_ino;

			/*
			 * We have to release the fbp early here to avoid
			 * a possible deadlock situation where we have the
			 * fbp and want the directory inode and someone doing
			 * a s_direnter has the directory inode and wants the
			 * fbp.  XXX - is this still needed?
			 */
			ep_ino = ep->d_ino;
#if 1
			if (fbp == 0)
				printf("s5_dirlook:fbrelse: invalid fbp=0\n");
#endif
			fbrelse(fbp, S_OTHER);
			fbp = NULL;

			if (namlen == 2 && namep[0] == '.' && namep[1] == '.') {
				error = s5_iget(dp->i_vnode.v_vfsp, ITOF(dp),
				    ep_ino, ipp, cr);
				if (error)
					goto bad;
			} else if (dp->i_number == ep_ino) {
				struct vnode *vp = ITOV(dp);
				VN_HOLD(vp);	/* want ourself, "." */
				*ipp = dp;
			} else {
				error = s5_iget(dp->i_vnode.v_vfsp, ITOF(dp),
				    ep_ino, ipp, cr);
				if (error)
					goto bad;
			}
			ip = *ipp;
			dnlc_enter(ITOV(dp), namep, ITOV(ip), NOCRED);
			rw_exit(&dp->i_rwlock);
			return (0);
		}
		offset += sizeof (struct direct);
		entryoffsetinblock += sizeof (struct direct);
	}
	error = ENOENT;
bad:
	if (fbp)
		fbrelse(fbp, S_OTHER);
	rw_exit(&dp->i_rwlock);
	return (error);
}

/*
 * If "dircheckforname" fails to find an entry with the given name, this
 * structure holds state for "s5_direnter" as to where there is space to put
 * an entry with that name.
 * If "dircheckforname" finds an entry with the given name, this structure
 * holds state for "dirrename" and "s_dirremove" as to where the entry is.
 * "status" indicates what "dircheckforname" found:
 *	NONE		name not found, no space,
 *	COMPACT		not used in s5
 *	FOUND		name not found, large enough free slot found
 *	EXIST		name found
 * If "dircheckforname" fails due to an error, this structure is not filled in.
 *
 * After "dircheckforname" succeeds the values are:
 *	status	offset			fbp, ep
 *	------	------			-------
 *	NONE	end of dir		not valid
 *	FOUND	start of entry		not valid
 *	EXIST	start if entry		valid
 *
 * "endoff" is set to 0 if the an entry with the given name is found, or if no
 * free slot could be found or made; this means that the directory should not
 * be truncated.  If the entry was found, the search terminates so
 * "dircheckforname" didn't find out where the last valid entry in the
 * directory was, so it doesn't know where to cut the directory off; if no free
 * slot could be found or made, the directory has to be extended to make room
 * for the new entry, so there's nothing to cut off.
 * Otherwise, "endoff" is set to the larger of the offset of the last
 * non-empty entry in the directory, or the offset at which the new entry will
 * be placed, whichever is larger.  This is used by "diraddentry"; if a new
 * entry is to be added to the directory, any complete directory blocks at the
 * end of the directory that contain no non-empty entries are lopped off the
 * end, thus shrinking the directory dynamically.
 *
 * On success, "s5_dirprepareentry" makes "fbp" and "ep" valid.
 */
struct slot {
	enum	{NONE, COMPACT, FOUND, EXIST} status;
	off_t	offset;		/* offset of area with free space */
	struct	fbuf *fbp;	/* dir buf where slot is */
	struct direct *ep;	/* pointer to slot */
};

static int s5_diraddentry(struct inode *tdp, char *namep, struct slot *slotp,
	struct inode *sip, struct inode *sdp, struct cred *cr);

/*
 * Write a new directory entry.
 * The directory must not have been removed and must be writable.
 * We distinguish three operations that build a new entry:  creating a file
 * (DE_CREATE), renaming (DE_RENAME) or linking (DE_LINK).  There are five
 * possible cases to consider:
 *
 *	Name
 *	found	op			action
 *	-----	---------------------	--------------------------------------
 *	no	DE_CREATE		create file according to vap and enter
 *	no	DE_LINK or DE_RENAME	enter the file sip
 *	yes	DE_CREATE		error EEXIST *ipp = found file
 *	yes	DE_LINK			error EEXIST
 *	yes	DE_RENAME		remove existing file, enter new file
 */
int
s5_direnter(tdp, namep, op, sdp, sip, vap, ipp, cr)
	register struct inode *tdp;	/* target directory to make entry in */
	register char *namep;		/* name of entry */
	enum de_op op;			/* entry operation */
	register struct inode *sdp;	/* source inode parent if rename */
	struct inode *sip;		/* source inode if link/rename */
	struct vattr *vap;		/* attributes if new inode needed */
	struct inode **ipp;		/* return entered inode here */
	struct cred *cr;		/* user credentials */
{
	struct inode *tip;		/* inode of (existing) target file */
	struct slot slot;		/* slot info to pass around */
	register int namlen;		/* length of name */
	register int err;		/* error number */
	register char *s;

	/* don't allow '/' characters in pathname component */
	for (s = namep, namlen = 0; *s; s++, namlen++)
		if (*s == '/')
			return (EACCES);
	if (namlen == 0)
		cmn_err(CE_PANIC, "bad namelen\n");

	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	/*
	 * If name is "." or ".." then if this is a create look it up
	 * and return EEXIST.  Rename or link TO "." or ".." is forbidden.
	 */
	if (namep[0] == '.' &&
	    (namlen == 1 || (namlen == 2 && namep[1] == '.'))) {
		if (op == DE_RENAME) {
			return (EINVAL);	/* *SIGH* should be ENOTEMPTY */
		}
		if (ipp) {
			/*
			 * s5_dirlook will acquire the i_rwlock
			 */
			rw_exit(&tdp->i_rwlock);
			if (err = s5_dirlook(tdp, namep, ipp, cr)) {
				rw_enter(&tdp->i_rwlock, RW_WRITER);
				return (err);
			}
			rw_enter(&tdp->i_rwlock, RW_WRITER);
		}
		return (EEXIST);
	}
	slot.status = NONE;
	slot.fbp = NULL;
	/*
	 * For link and rename lock the source entry and check the link count
	 * to see if it has been removed while it was unlocked.  If not, we
	 * increment the link count and force the inode to disk to make sure
	 * that it is there before any directory entry that points to it.
	 */
	if (op == DE_LINK || op == DE_RENAME) {
		rw_enter(&sip->i_contents, RW_WRITER);
		if (sip->i_nlink == 0) {
			rw_exit(&sip->i_contents);
			return (ENOENT);
		}
		if (sip->i_nlink == MAXLINK) {
			rw_exit(&sip->i_contents);
			return (EMLINK);
		}
		sip->i_nlink++;
		mutex_enter(&sip->i_tlock);
		sip->i_flag |= ICHG;
		mutex_exit(&sip->i_tlock);
		s5_iupdat(sip, 1);
		rw_exit(&sip->i_contents);
	}
	/*
	 * If target directory has not been removed, then we can consider
	 * allowing file to be created.
	 */
	if (tdp->i_nlink == 0) {
		err = ENOENT;
		goto out;
	}
	/*
	 * Check accessibility of directory.
	 */
	if ((tdp->i_mode & IFMT) != IFDIR) {
		err = ENOTDIR;
		goto out;
	}
	/*
	 * Execute access is required to search the directory.
	 */
	if (err = s5_iaccess(tdp, IEXEC, cr))
		goto out;
	/*
	 * If this is a rename of a directory and the parent is
	 * different (".." must be changed), then the source
	 * directory must not be in the directory hierarchy
	 * above the target, as this would orphan everything
	 * below the source directory.  Also the user must have
	 * write permission in the source so as to be able to
	 * change "..".
	 */
	if (op == DE_RENAME) {
		if (sip == tdp) {
			err = EINVAL;
			goto out;
		}
		rw_enter(&sip->i_contents, RW_READER);
		if ((sip->i_mode & IFMT) == IFDIR && sdp != tdp) {
			ino_t	inum;

			if ((err = s5_iaccess(sip, IWRITE, cr))) {
				rw_exit(&sip->i_contents);
				goto out;
			}
			inum = sip->i_number;
			rw_exit(&sip->i_contents);
			if ((err = s5_dircheckpath(inum, tdp, cr))) {
				goto out;
			}
		} else
			rw_exit(&sip->i_contents);
	}
	/*
	 * Search for the entry. Return VN_HELD tip if found.
	 */
	if (err = s5_dircheckforname(tdp, namep, namlen, &slot, &tip, cr))
		goto out;

	if (tip) {
		switch (op) {
		case DE_CREATE:
		case DE_MKDIR:
			if (ipp) {
				*ipp = tip;
				err = EEXIST;
			} else {
				s5_iput(tip);
			}
			break;

		case DE_RENAME:
			err = s5_dirrename(sdp, sip, tdp, namep,
			    tip, &slot, cr);
			s5_iput(tip);
			break;

		case DE_LINK:
			/*
			 * Can't link to an existing file.
			 */
			s5_iput(tip);
			err = EEXIST;
			break;
		}
	} else {
		/*
		 * The entry does not exist. Check write permission in
		 * directory to see if entry can be created.
		 */
		if (err = s5_iaccess(tdp, IWRITE, cr))
			goto out;
		if (op == DE_CREATE || op == DE_MKDIR) {
			/*
			 * Make new inode and directory entry as required.
			 */
			if (err = s5_dirmakeinode(tdp, &sip, vap, op, cr))
				goto out;
		}
		if (err = s5_diraddentry(tdp, namep, &slot, sip, sdp, cr)) {
			if (op == DE_CREATE || op == DE_MKDIR) {
				/*
				 * Unmake the inode we just made.
				 */
				rw_enter(&sip->i_contents, RW_WRITER);
				if ((sip->i_mode & IFMT) == IFDIR)
					tdp->i_nlink--;
				sip->i_nlink = 0;
				mutex_enter(&sip->i_tlock);
				sip->i_flag |= ICHG;
				mutex_exit(&sip->i_tlock);
				rw_exit(&sip->i_contents);
				irele(sip);
				sip = NULL;
			}
		} else if (ipp) {
			*ipp = sip;
		} else if (op == DE_CREATE || op == DE_MKDIR) {
			irele(sip);
		}
	}

out:
	if (slot.fbp)
		fbrelse(slot.fbp, S_OTHER);
	if (err && (op == DE_LINK || op == DE_RENAME)) {
		/*
		 * Undo bumped link count.
		 */
		rw_enter(&sip->i_contents, RW_WRITER);
		sip->i_nlink--;
		rw_exit(&sip->i_contents);

		mutex_enter(&sip->i_tlock);
		sip->i_flag |= ICHG;
		mutex_exit(&sip->i_tlock);
	}
	return (err);
}

/*
 * Check for the existence of a name in a directory, or else of an empty
 * slot in which an entry may be made.  If the requested name is found,
 * then on return *ipp points at the inode and *offp contains
 * its offset in the directory.  If the name is not found, then *ipp
 * will be NULL and *slotp will contain information about a directory slot in
 * which an entry may be made (either an empty slot, or the first position
 * past the end of the directory).
 * The target directory inode (tdp) is supplied write locked (i_rwlock).
 *
 * This may not be used on "." or "..", but aliases of "." are ok.
 */
static int
s5_dircheckforname(tdp, namep, namlen, slotp, ipp, cr)
	register struct inode *tdp;	/* inode of directory being checked */
	char *namep;			/* name we're checking for */
	register int namlen;		/* length of name */
	register struct slot *slotp;	/* slot structure */
	struct inode **ipp;		/* return inode if we find one */
	struct cred *cr;
{
	int dirsize;			/* size of the directory */
	struct fbuf *fbp;		/* pointer to directory block */
	register int entryoffsetinblk;	/* offset of ep in fbp's buffer */
	register struct direct *ep;	/* directory entry */
	register off_t offset;		/* offset in the directory */
	int err;

	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	fbp = NULL;
	entryoffsetinblk = 0;
	slotp->status = NONE;
	/*
	 * No point in using i_diroff since we must search whole directory
	 */
	dirsize = tdp->i_size;
	offset = 0;
	while (offset < dirsize) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if (blkoff(tdp->i_s5vfs, offset) == 0) {
			if (fbp != NULL)
				fbrelse(fbp, S_OTHER);

			err = blkatoff(tdp, offset, (char **)0, &fbp);
			if (err)
				return (err);
			entryoffsetinblk = 0;
		}
		/*
		 * Get pointer to next entry.
		 */
		ep = (struct direct *)(fbp->fb_addr + entryoffsetinblk);
		if (ep->d_ino == 0) {
			if (slotp->status != FOUND) {
				slotp->status = FOUND;
				slotp->offset = offset;
			}
			offset += sizeof (struct direct);
			entryoffsetinblk += sizeof (struct direct);
			continue;
		}
		/*
		 * Check for a name match.
		 */
		if (*namep == *ep->d_name && /* fast chk 1st char */
		    bcmp(namep, ep->d_name, namlen) == 0 &&
		    (namlen == DIRSIZ || ep->d_name[namlen] == '\0')) {

			if (tdp->i_number == ep->d_ino) {
				struct vnode *vp = ITOV(tdp);

				*ipp = tdp;	/* we want ourself, ie "." */
				VN_HOLD(vp);
			} else {
				err = s5_iget(tdp->i_vnode.v_vfsp, ITOF(tdp),
				    ep->d_ino, ipp, cr);
				if (err) {
					fbrelse(fbp, S_OTHER);
					return (err);
				}
			}
			slotp->status = EXIST;
			slotp->offset = offset;
			slotp->fbp = fbp;
			slotp->ep = ep;
			return (0);
		}
		offset += sizeof (struct direct);
		entryoffsetinblk += sizeof (struct direct);
	}
	if (fbp) {
		fbrelse(fbp, S_OTHER);
	}
	if (slotp->status == NONE) {
		/*
		 * We didn't find a slot; the new directory entry should be put
		 * at the end of the directory.  Return an indication of where
		 * this is, and set "endoff" to zero; since we're going to have
		 * to extend the directory, we're certainly not going to
		 * trucate it.
		 */
		slotp->offset = dirsize;
	}
	*ipp = (struct inode *)NULL;
	return (0);
}

/*
 * Rename the entry in the directory tdp so that it points to
 * sip instead of tip.
 */
static int
s5_dirrename(sdp, sip, tdp, namep, tip, slotp, cr)
	register struct inode *sdp;	/* parent directory of source */
	register struct inode *sip;	/* source inode */
	register struct inode *tdp;	/* parent directory of target */
	char *namep;			/* entry we are trying to change */
	struct inode *tip;		/* locked target inode */
	struct slot *slotp;		/* slot for entry */
	struct cred *cr;		/* credentials */
{
	int error = 0;
	int doingdirectory;

	ASSERT(MUTEX_HELD(&rename_lock));
	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	/*
	 * Everything is protected under the rename_lock so the ordering
	 * of i_contents locks doesn't matter here.
	 */
	rw_enter(&sip->i_contents, RW_READER);
	rw_enter(&tip->i_contents, RW_READER);
	/*
	 * Check that everything is on the same filesystem.
	 */
	if ((tip->i_vnode.v_vfsp != tdp->i_vnode.v_vfsp) ||
	    (tip->i_vnode.v_vfsp != sip->i_vnode.v_vfsp)) {
		error = EXDEV;		/* XXX archaic */
		goto out;
	}
	/*
	 * Short circuit rename of something to itself.
	 */
	if (sip->i_number == tip->i_number) {
		error = ESAME;		/* special KLUDGE error code */
		goto out;
	}
	/*
	 * Must have write permission to rewrite target entry.
	 */
	if (error = s5_iaccess(tdp, IWRITE, cr))
		goto out;
	/*
	 * If the parent directory is "sticky", then the user must own
	 * either the parent directory or the destination of the rename,
	 * or else must have permission to write the destination.
	 * Otherwise the destination may not be changed (except by the
	 * super-user).  This implements append-only directories.
	 */
	if ((tdp->i_mode & ISVTX) && cr->cr_uid != 0 &&
	    cr->cr_uid != tdp->i_uid && cr->cr_uid != tip->i_uid &&
	    (error = s5_iaccess(tip, IWRITE, cr)))
		goto out;

	/*
	 * Ensure source and target are compatible (both directories
	 * or both not directories).  If target is a directory it must
	 * be empty and have no links to it; in addition it must not
	 * be a mount point, and both the source and target must be
	 * writable.
	 */
	doingdirectory = ((sip->i_mode & IFMT) == IFDIR);
	if ((tip->i_mode & IFMT) == IFDIR) {
		if (!doingdirectory) {
			error = EISDIR;
			goto out;
		}
		/*
		 * vn_vfslock will prevent mounts from using the directory until
		 * we are done.
		 */
		if (vn_vfslock(ITOV(tip))) {
			error = EBUSY;
			goto out;
		}
		if (ITOV(tip)->v_vfsmountedhere) {
			vn_vfsunlock(ITOV(tip));
			error = EBUSY;
			goto out;
		}
		if (!s5_dirempty(tip, (ino_t)tdp->i_number, cr) ||
							tip->i_nlink > 2) {
			vn_vfsunlock(ITOV(tip));
			error = EEXIST;	/* SIGH should be ENOTEMPTY */
			goto out;
		}
	} else if (doingdirectory) {
		error = ENOTDIR;
		goto out;
	}

	/*
	 * Rewrite the inode pointer for target name entry
	 * from the target inode (ip) to the source inode (sip).
	 * This prevents the target entry from disappearing
	 * during a crash. Mark the directory inode to reflect the changes.
	 */
	dnlc_remove(ITOV(tdp), namep);
	slotp->ep->d_ino = sip->i_number;
	dnlc_enter(ITOV(tdp), namep, ITOV(sip), NOCRED);

	error = s5_fbwrite(slotp->fbp, tdp);	/* Which ip here? XXXX */

	slotp->fbp = NULL;
	if (error) {
		if (doingdirectory)
			vn_vfsunlock(ITOV(tip));
		goto out;
	}
	/*
	 * Upgrade to write lock on tip
	 */
	rw_exit(&tip->i_contents);
	rw_enter(&tip->i_contents, RW_WRITER);

	mutex_enter(&tdp->i_tlock);
	tdp->i_flag |= IUPD|ICHG;
	mutex_exit(&tdp->i_tlock);
	/*
	 * Decrement the link count of the target inode.
	 * Fix the ".." entry in sip to point to dp.
	 * This is done after the new entry is on the disk.
	 */
	tip->i_nlink--;
	mutex_enter(&tip->i_tlock);
	tip->i_flag |= ICHG;
	mutex_exit(&tip->i_tlock);
	if (doingdirectory) {
		/*
		 * The entry for tip no longer exists so I can unlock the
		 * vfslock.
		 */
		vn_vfsunlock(ITOV(tip));
		/*
		 * Decrement target link count once more if it was a directory.
		 */
		if (--tip->i_nlink != 0)
			cmn_err(CE_PANIC,
			    "s5_direnter: target directory link count");
		(void) s5_itrunc(tip, (u_long)0, cr);
		/*
		 * Renaming a directory with the parent different
		 * requires that ".." be rewritten.  The window is
		 * still there for ".." to be inconsistent, but this
		 * is unavoidable, and a lot shorter than when it was
		 * done in a user process.  We decrement the link
		 * count in the new parent as appropriate to reflect
		 * the just-removed target.  If the parent is the
		 * same, this is appropriate since the original
		 * directory is going away.  If the new parent is
		 * different, dirfixdotdot() will bump the link count
		 * back.
		 */
		tdp->i_nlink--;
		mutex_enter(&tdp->i_tlock);
		tdp->i_flag |= ICHG;
		mutex_exit(&tdp->i_tlock);
		if (sdp != tdp) {
			rw_exit(&tip->i_contents);
			rw_exit(&sip->i_contents);
			error = s5_dirfixdotdot(sip, sdp, tdp);
			return (error);
		}
	}
out:
	rw_exit(&tip->i_contents);
	rw_exit(&sip->i_contents);
	return (error);
}

/*
 * Fix the ".." entry of the child directory so that it points
 * to the new parent directory instead of the old one.  Routine
 * assumes that dp is a directory and that all the inodes are on
 * the same file system.
 */
static int
s5_dirfixdotdot(dp, opdp, npdp)
	register struct inode *dp;	/* child directory */
	register struct inode *opdp;	/* old parent directory */
	register struct inode *npdp;	/* new parent directory */
{
	struct fbuf *fbp;
	struct dirtemplate *dirp;
	int error;

	ASSERT(RW_WRITE_HELD(&npdp->i_rwlock));
	error = blkatoff(dp, (off_t)0, (char **)&dirp, &fbp);

	if (error || dp->i_nlink == 0 ||
	    dp->i_size < sizeof (struct dirtemplate))
		goto bad;
	if (dirp->dotdot_ino == npdp->i_number)	/* Just a no-op. */
		goto bad;
	if (dirp->dotdot_name[0] != '.' || dirp->dotdot_name[1] != '.' ||
	    dirp->dotdot_name[2] != '\0') {	/* Sanity check. */
		dirbad(dp, "mangled .. entry", (off_t)0);
		error = ENOTDIR;
		goto bad;
	}

	/*
	 * Increment the link count in the new parent inode and force it out.
	 */
	npdp->i_nlink++;
	mutex_enter(&npdp->i_tlock);
	npdp->i_flag |= ICHG;
	mutex_exit(&npdp->i_tlock);
	s5_iupdat(npdp, 1);

	/*
	 * Rewrite the child ".." entry and force it out.
	 */
	dnlc_remove(ITOV(dp), "..");
	dirp->dotdot_ino = npdp->i_number;
	dnlc_enter(ITOV(dp), "..", ITOV(npdp), NOCRED);

	error = s5_fbwrite(fbp, dp);

	fbp = NULL;
	if (error)
		goto bad;

	/*
	 * Decrement the link count of the old parent inode and force
	 * it out.  If opdp is NULL, then this is a new directory link;
	 * it has no parent, so we need not do anything.
	 */
	if (opdp != NULL) {
		rw_enter(&opdp->i_contents, RW_WRITER);
		if (opdp->i_nlink != 0) {
			opdp->i_nlink--;
			mutex_enter(&opdp->i_tlock);
			opdp->i_flag |= ICHG;
			mutex_exit(&opdp->i_tlock);
			s5_iupdat(opdp, 1);
		}
		rw_exit(&opdp->i_contents);
	}
	return (0);

bad:
	if (fbp)
		fbrelse(fbp, S_OTHER);
	return (error);
}

/*
 * Enter the file sip in the directory tdp with name namep.
 */
static int
s5_diraddentry(struct inode *tdp, char *namep, struct slot *slotp,
	struct inode *sip, struct inode *sdp, struct cred *cr)
{
	int error;

	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	/*
	 * Prepare a new entry.  If the caller has not supplied an
	 * existing inode, make a new one.
	 */
	error = s5_dirprepareentry(tdp, slotp, cr);
	if (error)
		return (error);
	/*
	 * Check inode to be linked to see if it is in the
	 * same filesystem.
	 */
	if (tdp->i_vnode.v_vfsp != sip->i_vnode.v_vfsp) {
		error = EXDEV;
		goto bad;
	}
	if ((sip->i_mode & IFMT) == IFDIR) {
		error = s5_dirfixdotdot(sip, sdp, tdp);
		if (error)
			goto bad;
	}

	/*
	 * Fill in entry data.
	 */
	(void) strncpy(slotp->ep->d_name, namep, (size_t)DIRSIZ);
	slotp->ep->d_ino = sip->i_number;
	dnlc_enter(ITOV(tdp), namep, ITOV(sip), NOCRED);

	/*
	 * Write out the directory entry.
	 */
	error = s5_fbwrite(slotp->fbp, tdp);

	slotp->fbp = NULL;
	if (error)
		return (error);		/* XXX - already fixed dotdot? */

	/*
	 * Mark the directory inode to reflect the changes.
	 * Truncate the directory to chop off blocks of empty entries.
	 */
	mutex_enter(&tdp->i_tlock);
	tdp->i_flag |= IUPD|ICHG;
	mutex_exit(&tdp->i_tlock);

	return (0);

bad:
	/*
	 * Clear out entry prepared by dirprepareent.
	 */
	slotp->ep->d_ino = 0;
	(void) s5_fbwrite(slotp->fbp, tdp);	/* XXX - is this right? */
	slotp->fbp = NULL;
	return (error);
}

/*
 * Prepare a directory slot to receive an entry.
 */
static int
s5_dirprepareentry(dp, slotp, cr)
	register struct inode *dp;	/* directory we are working in */
	register struct slot *slotp;	/* available slot info */
	struct cred *cr;
{
	off_t entryend;
	int err;

	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	/*
	 * If we didn't find a slot, then indicate that the
	 * new slot belongs at the end of the directory.
	 * If we found a slot, then the new entry can be
	 * put at slotp->offset.
	 */
	entryend = slotp->offset + sizeof (struct direct);
	if (slotp->status == NONE &&
	    blkoff(dp->i_s5vfs, slotp->offset) == 0) {
		rw_enter(&dp->i_contents, RW_WRITER);
		err = BMAPALLOC(dp, slotp->offset,
		    (int)(blkoff(dp->i_s5vfs, slotp->offset) +
		    dp->i_s5vfs->vfs_bsize), cr);
		rw_exit(&dp->i_contents);
		if (err)
			return (err);
	}
	if (entryend > dp->i_size) {
		dp->i_size = entryend;
		mutex_enter(&dp->i_tlock);
		dp->i_flag |= IUPD|ICHG;
		mutex_exit(&dp->i_tlock);
	}

	/*
	 * Get the block containing the space for the new directory entry.
	 */
	err = blkatoff(dp, slotp->offset, (char **)&slotp->ep, &slotp->fbp);
	return (err);
}

/*
 * Allocate and initialize a new inode that will go into directory tdp.
 */
static int
s5_dirmakeinode(
	struct inode *tdp,
	struct inode **ipp,
	struct vattr *vap,
	enum de_op op,
	struct cred *cr)
{
	struct inode *ip;
	register enum vtype type;
	int imode;			/* mode and format as in inode */
	int error;

	ASSERT(vap != NULL);
	ASSERT(op == DE_CREATE || op == DE_MKDIR);
	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));
	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));

	/*
	 * EFT check.  The group check is a bit over-zealous, and would
	 * probably be better handled later - however we catch it here to
	 * avoid ever creating a bogus inode in the first place.
	 */
	if ((u_long)cr->cr_uid > (u_long)USHRT_MAX ||
	    (u_long)cr->cr_gid > (u_long)USHRT_MAX ||
	    ((vap->va_mask & AT_GID) == AT_GID &&
	    (u_long)vap->va_gid > (u_long)USHRT_MAX))
		return (EOVERFLOW);

	if (vap->va_mask & (AT_ATIME|AT_MTIME))
		return (EOPNOTSUPP);

	/*
	 * Allocate a new inode.
	 */
	type = vap->va_type;
	imode = MAKEIMODE(type, vap->va_mode);
	error = s5_ialloc(tdp, &ip, cr);

	if (error)
		return (error);
	rw_enter(&ip->i_contents, RW_WRITER);
	mutex_enter(&ip->i_tlock);
	ip->i_flag |= IACC|IUPD|ICHG;
	mutex_exit(&ip->i_tlock);
	ip->i_mode = (o_mode_t)imode;
	if (type == VBLK || type == VCHR) {
		ip->i_oldrdev = ip->i_vnode.v_rdev = ip->i_rdev = vap->va_rdev;
	}
	ip->i_vnode.v_type = type;
	if (type == VDIR) {
		ip->i_nlink = 2; /* anticipating a call to dirmakedirect */
	} else {
		ip->i_nlink = 1;
	}
	ip->i_uid = cr->cr_uid;
	/*
	 * To determine the group-id of the created file:
	 *   1) If the gid is set in the attribute list (non-Sun & pre-4.0
	 *	clients are not likely to set the gid), then use it if
	 *	the process is super-user, belongs to the target group,
	 *	or the group is the same as the parent directory.
	 *   2) If the filesystem was not mounted with the Old-BSD-compatible
	 *	GRPID option, and the directory's set-gid bit is clear,
	 *	then use the process's gid.
	 *   3) Otherwise, set the group-id to the gid of the parent directory.
	 */
	if ((vap->va_mask & AT_GID) &&
	    ((cr->cr_uid == 0) || (vap->va_gid == tdp->i_gid) ||
	    groupmember(vap->va_gid, cr))) {
		/*
		 * XXX - is this only the case when a 4.0 NFS client, or a
		 * client derived from that code, makes a call over the wire?
		 */
		ip->i_gid = vap->va_gid;
	} else {
		if (tdp->i_mode & ISGID)
			ip->i_gid = tdp->i_gid;
		else
			ip->i_gid = cr->cr_gid;
	}

	/*
	 * If we're creating a directory, and the parent directory has the
	 * set-GID bit set, set it on the new directory.
	 * Otherwise, if the user is neither super-user nor a member of the
	 * file's new group, clear the file's set-GID bit.
	 */
	if (tdp->i_mode & ISGID && type == VDIR)
		ip->i_mode |= ISGID;
	else {
		if ((ip->i_mode & ISGID) &&
		    !groupmember((uid_t)ip->i_gid, cr) && cr->cr_uid != 0)
			ip->i_mode &= ~ISGID;
	}
	/*
	 * Make sure inode goes to disk before directory data and entries
	 * pointing to it.
	 * Then unlock it, since nothing points to it yet.
	 */
	s5_iupdat(ip, 1);

	if (op == DE_MKDIR) {
		error = s5_dirmakedirect(ip, tdp, cr);
	}
	if (error) {
		/* Throw away inode we just allocated. */
		ip->i_nlink = 0;
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= ICHG;
		mutex_exit(&ip->i_tlock);
		rw_exit(&ip->i_contents);
		s5_iput(ip);
	} else {
		rw_exit(&ip->i_contents);
		*ipp = ip;
	}
	return (error);
}

/*
 * Write a prototype directory into the empty inode ip, whose parent is dp.
 */
static int
s5_dirmakedirect(ip, dp, cr)
	register struct inode *ip;		/* new directory */
	register struct inode *dp;		/* parent directory */
	struct cred *cr;
{
	int error;
	register struct dirtemplate *dirp;
	struct fbuf *fbp;

	ASSERT(RW_WRITE_HELD(&ip->i_contents));
	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	/*
	 * Allocate space for the directory we're creating.
	 */
	error = BMAPALLOC(ip, (u_int)0, ip->i_s5vfs->vfs_bsize, cr);
	if (error)
		return (error);
	ip->i_size = sizeof (struct dirtemplate);
	mutex_enter(&ip->i_tlock);
	ip->i_flag |= IUPD|ICHG;
	mutex_exit(&ip->i_tlock);
	/*
	 * Update the tdp link count and write out the change.
	 * This reflects the ".." entry we'll soon write.
	 */
	dp->i_nlink++;
	mutex_enter(&dp->i_tlock);
	dp->i_flag |= ICHG;
	mutex_exit(&dp->i_tlock);
	s5_iupdat(dp, 1);
	/*
	 * Initialize directory with "."
	 * and ".." from static template.
	 *
	 * Since the parent directory is locked, we don't have to
	 * worry about anything changing when we drop the write
	 * lock on (ip).
	 */
	rw_exit(&ip->i_contents);
	error = fbread(ITOV(ip), (offset_t)0, (u_int)ip->i_s5vfs->vfs_bsize,
	    S_OTHER, &fbp);
	if (error) {
		rw_enter(&ip->i_contents, RW_WRITER);
		return (error);
	}
	dirp = (struct dirtemplate *)fbp->fb_addr;
	/*
	 * Now initialize the directory we're creating
	 * with the "." and ".." entries.
	 */
	*dirp = mastertemplate;			/* structure assignment */
	dirp->dot_ino = ip->i_number;
	dirp->dotdot_ino = dp->i_number;

	error = s5_fbwrite(fbp, ip);
	rw_enter(&ip->i_contents, RW_WRITER);
	return (error);
}

/*
 * Delete a directory entry.  If oip is nonzero the entry is checked
 * to make sure it still reflects oip.
 */
int
s5_dirremove(dp, namep, oip, cdir, op, cr)
	register struct inode *dp;
	char *namep;
	struct inode *oip;
	struct vnode *cdir;
	enum dr_op op;
	struct cred *cr;
{
	register struct direct *ep;
	struct inode *ip;
	int namlen;
	struct slot slot;
	int error = 0;

	namlen = strlen(namep);
	if (namlen == 0)
		cmn_err(CE_PANIC, "s5_dirremove");
	/*
	 * return error when removing . and ..
	 */
	if (namep[0] == '.') {
		if (namlen == 1)
			return (EINVAL);
		else if (namlen == 2 && namep[1] == '.')
			{
			return (EEXIST);	/* SIGH should be ENOTEMPTY */
			}
	}

	ip = NULL;
	slot.fbp = NULL;
	/*
	 * Don't bother with i_contents lock since we are
	 * serialized with i_rwlock
	 */
	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	/*
	 * Check accessibility of directory.
	 */
	if ((dp->i_mode & IFMT) != IFDIR) {
		error = ENOTDIR;
		goto out;
	}

	/*
	 * Execute access is required to search the directory.
	 * Access for write is interpreted as allowing
	 * deletion of files in the directory.
	 */
	if (error = s5_iaccess(dp, IEXEC|IWRITE, cr))
		goto out;

	slot.status = FOUND;	/* don't need to look for empty slot */
	if (error = s5_dircheckforname(dp, namep, namlen, &slot, &ip, cr))
		goto out;
	if (ip == NULL) {
		error = ENOENT;
		goto out;
	}
	if (oip && oip != ip) {
		error = ENOENT;
		goto out;
	}

	/*
	 * There used to be a check here to make sure you are not removing a
	 * mounted on dir.  This was no longer correct because s5_iget() does
	 * not cross mount points anymore so the the i_dev fields in the inodes
	 * pointed to by ip and dp will never be different.  There does need
	 * to be a check here though, to eliminate the race between mount and
	 * rmdir (It can also be a race between mount and unlink, if your
	 * kernel allows you to unlink a directory.)  All call to vn_vfslock
	 * is also needed to prevent a race between mount and rmdir.
	 */
	if (vn_vfslock(ITOV(ip))) {
		error = EBUSY;
		goto out;
	}
	if (ITOV(ip)->v_vfsmountedhere != NULL) {
		vn_vfsunlock(ITOV(ip));
		error = EBUSY;
		goto out;
	}
	/*
	 * If we are removing a directory, get a lock on it. If the directory
	 * is empty, it will stay empty until we can remove it.
	 */
	rw_enter(&ip->i_rwlock, RW_READER);
	rw_enter(&ip->i_contents, RW_READER);
	/*
	 * If the parent directory is "sticky", then the user must
	 * own the parent directory or the file in it, or else must
	 * have permission to write the file.  Otherwise it may not
	 * be deleted (except by the super-user).  This implements
	 * append-only directories.
	 */
	if ((dp->i_mode & ISVTX) && cr->cr_uid != 0 &&
	    cr->cr_uid != dp->i_uid && cr->cr_uid != ip->i_uid &&
	    (error = s5_iaccess(ip, IWRITE, cr))) {
		vn_vfsunlock(ITOV(ip));
		rw_exit(&ip->i_contents);
		rw_exit(&ip->i_rwlock);
		goto out;
	}
	if (op == DR_RMDIR) {
		/*
		 * For rmdir(2), some special checks are required.
		 * (a) Don't remove any alias of the parent (e.g. ".").
		 * (b) Don't remove the current directory.
		 * (c) Make sure the entry is (still) a directory.
		 * (d) Make sure the directory is empty.
		 */

		if (dp == ip || ITOV(ip) == cdir)
			error = EINVAL;
		else if ((ip->i_mode & IFMT) != IFDIR)
			error = ENOTDIR;
		else if ((ip->i_nlink != 2) ||
		    !s5_dirempty(ip, (ino_t)dp->i_number, cr)) {
			error = EEXIST;	/* SIGH should be ENOTEMPTY */
		}
		if (error) {
			vn_vfsunlock(ITOV(ip));
			rw_exit(&ip->i_contents);
			rw_exit(&ip->i_rwlock);
			goto out;
		}
	} else if (op == DR_REMOVE)  {
		/*
		 * unlink(2) requires a different check: allow only
		 * the super-user to unlink a directory.
		 */
		struct vnode *vp = ITOV(ip);

		if (vp->v_type == VDIR && !suser(cr)) {
			error = EPERM;
			vn_vfsunlock(vp);
			rw_exit(&ip->i_contents);
			rw_exit(&ip->i_rwlock);
			goto out;
		}
	}
	rw_exit(&ip->i_contents);
	/*
	 * Remove the cache'd entry, if any.
	 */
	dnlc_remove(ITOV(dp), namep);

	ep = slot.ep;
	ep->d_ino = 0;

	error = s5_fbwrite(slot.fbp, dp);
	/*
	 * If we were removing a directory, it is 'gone' now so we can
	 * unlock it.
	 */
	rw_exit(&ip->i_rwlock);

	slot.fbp = NULL;
	mutex_enter(&dp->i_tlock);
	dp->i_flag |= IUPD|ICHG;
	mutex_exit(&dp->i_tlock);
	mutex_enter(&ip->i_tlock);
	ip->i_flag |= ICHG;
	mutex_exit(&ip->i_tlock);

	if (error) {
		vn_vfsunlock(ITOV(ip));
		goto out;
	}
	rw_enter(&ip->i_contents, RW_WRITER);
	/*
	 * Now dispose of the inode.
	 */
	if (ip->i_nlink > 0) {
		if (op == DR_RMDIR && (ip->i_mode & IFMT) == IFDIR) {
			/*
			 * Decrement by 2 because we're trashing the "."
			 * entry as well as removing the entry in dp.
			 * Clear the inode, but there may be other hard
			 * links so don't free the inode.
			 * Decrement the dp linkcount because we're
			 * trashing the ".." entry.
			 */
			ip->i_nlink -= 2;
			dp->i_nlink--;
			dnlc_remove(ITOV(ip), ".");
			dnlc_remove(ITOV(ip), "..");
			(void) s5_itrunc(ip, (u_long)0, cr);
		} else {
			ip->i_nlink--;
		}
	}
	rw_exit(&ip->i_contents);

	vn_vfsunlock(ITOV(ip));
out:
	if (ip) {
		s5_iput(ip);
	}
	if (slot.fbp)
		fbrelse(slot.fbp, S_OTHER);
	return (error);
}

/*
 * Return buffer with contents of block "offset"
 * from the beginning of directory "ip".  If "res"
 * is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 *
 * Since fbread may fault and end up calling s5_getpage(), no
 * lock on the i_contents lock must be held.
 */
static int
blkatoff(ip, offset, res, fbpp)
	struct inode *ip;
	off_t offset;
	char **res;
	struct fbuf **fbpp;
{
	struct fbuf *fbp;
	u_int bsize;
	int err;

	bsize = blksize(ip->i_s5vfs);
	err = fbread(ITOV(ip), (offset_t)(offset & ~(ip->i_s5vfs->vfs_bmask)),
		bsize, S_OTHER, &fbp);
	if (err) {
		*fbpp = (struct fbuf *)NULL;
		return (err);
	}
	if (res)
		*res = fbp->fb_addr + blkoff(ip->i_s5vfs, offset);
	*fbpp = fbp;
	return (0);
}

static void
dirbad(ip, how, offset)
	struct inode *ip;
	char *how;
	off_t offset;
{
	cmn_err(CE_NOTE, "bad dir ino %d at offset %ld: %s\n",
	    ip->i_number, offset, how);
}

/*
 * Check if a directory is empty or not.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct direct.
 *
 * N.B.: does not handle corrupted directories.
 */
static int
s5_dirempty(ip, parentino, cr)
	register struct inode *ip;
	ino_t parentino;
	struct cred *cr;
{
	register off_t off;
	struct dirtemplate dbuf;
	register struct direct *dp = (struct direct *)&dbuf;
	int err, count;
	int empty = 1;	/* Assume it's empty */
#define	MINDIRSIZ (sizeof (struct dirtemplate) / 2)

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	for (off = 0; off < ip->i_size; off += sizeof (struct direct)) {
		err = s5_rdwri(UIO_READ, ip, (caddr_t)dp, (int)MINDIRSIZ,
		    (offset_t)off, UIO_SYSSPACE, &count, cr);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (err || count != 0) {
			empty = 0;
			break;
		}
		/* skip empty entries */
		if (dp->d_ino == 0)
			continue;
		/* accept only "." and ".." */
		if (dp->d_name[0] != '.') {
			empty = 0;
			break;
		}
		/*
		 * At this point only "." and ".." are legal names.
		 *
		 */
		if ((dp->d_name[1] == '\0')		/* CSTYLED */
		||  ((dp->d_name[1] == '.')		/* CSTYLED */
		  && (dp->d_name[2] == '\0')		/* CSTYLED */
		  && (dp->d_ino == parentino)))		/* CSTYLED */
			continue;
		empty = 0;
		break;
	}
	return (empty);
}

/*
 * Check if source directory inode is in the path of the target directory.
 * Target is supplied locked.
 *
 * The source and target inode's should be different upon entry.
 */
static int
s5_dircheckpath(source_ino, target, cr)
	ino_t source_ino;
	struct inode *target;
	struct cred *cr;
{
	struct fbuf *fbp;
	struct dirtemplate *dirp;
	register struct inode *ip;
	struct inode *tip;
	ino_t dotdotino;
	int err = 0;

	ASSERT(MUTEX_HELD(&rename_lock));
	ASSERT(RW_WRITE_HELD(&target->i_rwlock));

	ip = target;
	if (ip->i_number == source_ino) {
		err = EINVAL;
		goto out;
	}
	if (ip->i_number == S5ROOTINO)
		goto out;
	/*
	 * Search back through the directory tree, using the ".." entries.
	 * Fail any attempt to move a directory into an ancestor directory.
	 */
	fbp = NULL;
	for (;;) {
		struct	vfs	*vfs;

		err = blkatoff(ip, (off_t)0, (char **)&dirp, &fbp);
		if (err)
			break;
		if (((ip->i_mode & IFMT) != IFDIR) || ip->i_nlink == 0 ||
		    ip->i_size < sizeof (struct dirtemplate)) {
			dirbad(ip, "bad size, unlinked or not dir", (off_t)0);
			err = ENOTDIR;
			break;
		}
		if (dirp->dotdot_name[0] != '.' ||
		    dirp->dotdot_name[1] != '.') {
			dirbad(ip, "mangled .. entry", (off_t)0);
			err = ENOTDIR;		/* Sanity check */
			break;
		}
		dotdotino = dirp->dotdot_ino;
		if (dotdotino == source_ino) {
			err = EINVAL;
			break;
		}
		if (dotdotino == S5ROOTINO)
			break;
		if (fbp) {
			fbrelse(fbp, S_OTHER);
			fbp = NULL;
		}
		vfs = ip->i_vnode.v_vfsp;
		if (ip != target) {
			rw_exit(&ip->i_rwlock);
			s5_iput(ip);
		}
		/*
		 * Race to get the inode.
		 */
		if (err = s5_iget(vfs, ITOF(ip), dotdotino, &tip, cr)) {
			ip = NULL;
			break;
		}
		ip = tip;
		rw_enter(&ip->i_rwlock, RW_READER);
	}
	if (fbp) {
		fbrelse(fbp, S_OTHER);
	}
out:
	if (ip) {
		if (ip != target) {
			rw_exit(&ip->i_rwlock);
			s5_iput(ip);
		}
	}
	return (err);
}
