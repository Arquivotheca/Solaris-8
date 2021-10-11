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
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1986-1989,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ufs_dir.c	2.132	99/11/05 SMI"

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
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/mount.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_quota.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <vm/seg.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>

/*
 * A virgin directory.
 */
static struct dirtemplate mastertemplate = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

#define	LDIRSIZ(len) \
	((sizeof (struct direct) - (MAXNAMLEN + 1)) + ((len + 1 + 3) &~ 3))

#ifdef DEBUG
int dirchk = 1;
#else /* !DEBUG */
int dirchk = 0;
#endif /* DEBUG */
static void dirbad();
static int blkatoff();
static int ufs_dircheckpath();
static int ufs_dircheckforname();
static int ufs_dirrename();
static int ufs_dirmakeinode();
static int ufs_diraddentry();
static int ufs_dirempty();
static int ufs_dirfixdotdot();
static int dirprepareentry();
static int ufs_dirmakedirect();
static int dirbadname();
static int dirmangled();

/*
 * Look for a given name in a directory.  On successful return, *ipp
 * will point to the VN_HELD inode.
 */
int
ufs_dirlook(dp, namep, ipp, cr, skipdnlc)
	register struct inode *dp;
	register char *namep;
	register struct inode **ipp;
	struct cred *cr;
	int skipdnlc;
{
	struct fbuf *fbp;		/* a buffer of directory entries */
	register struct direct *ep;	/* the current directory entry */
	register struct inode *ip;
	struct vnode *vp;
	int entryoffsetinblock;		/* offset of ep in addr's buffer */
	int numdirpasses;		/* strategy for directory search */
	off_t endsearch;		/* offset to end directory search */
	int namlen;			/* length of name */
	off_t	offset;
	off_t	start_off;		/* starting offset from middle search */
	int err;
	int doingchk;
	ino_t chkino;
	register int i;

restart:
	fbp = NULL;
	entryoffsetinblock = 0;
	doingchk = 0;
	chkino = 0;

	/*
	 * Check accessibility of directory.
	 */
	if ((dp->i_mode & IFMT) != IFDIR)
		return (ENOTDIR);
	if (err = ufs_iaccess(dp, IEXEC, cr))
		return (err);

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
	if (!skipdnlc && (vp = dnlc_lookup(ITOV(dp), namep, NOCRED))) {
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

recheck:
	/*
	 * Take care to look at dp->i_diroff only once, as it
	 * may be changing due to other threads/cpus.
	 */
	offset = dp->i_diroff;
	if (offset > dp->i_size) {
		offset = 0;
	}
	if (offset == 0) {
		numdirpasses = 1;
	} else {
		start_off = offset;

		entryoffsetinblock = blkoff(dp->i_fs, offset);
		if (entryoffsetinblock != 0) {
			err = blkatoff(dp, offset, (char **)0, &fbp);
			if (err)
				goto bad;
		}
		numdirpasses = 2;
	}
	endsearch = roundup(dp->i_size, DIRBLKSIZ);
	namlen = strlen(namep);

searchloop:
	while (offset < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if (blkoff(dp->i_fs, offset) == 0) {
			if (fbp != NULL) {
				fbrelse(fbp, S_OTHER);
			}
			err = blkatoff(dp, offset, (char **)0, &fbp);
			if (err)
				goto bad;
			entryoffsetinblock = 0;
		}

		/*
		 * If the offset to the next entry is invalid or if the
		 * next entry is a zero length record or if the record
		 * length is invalid, then skip to the next directory
		 * block.  Complete validation checks are done if the
		 * record length is invalid.
		 *
		 * Full validation checks are slow so they are disabled
		 * by default.  Complete checks can be run by patching
		 * "dirchk" to be true.
		 *
		 * We have to check the validity of entryoffsetinblock
		 * here because it can be set to i_diroff above.
		 */
		ep = (struct direct *)(fbp->fb_addr + entryoffsetinblock);
		if ((entryoffsetinblock & 0x3) || ep->d_reclen == 0 ||
		    (dirchk || (ep->d_reclen & 0x3)) &&
		    dirmangled(dp, ep, entryoffsetinblock, offset)) {
			i = DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1));
			offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * Check for a name match.
		 * We have the parent inode read locked with i_rwlock.
		 */
		if (ep->d_ino && ep->d_namlen == namlen &&
		    *namep == *ep->d_name &&	/* fast chk 1st chr */
		    bcmp(namep, ep->d_name, (int)ep->d_namlen) == 0) {
			ino_t ep_ino;

			/*
			 * We have to release the fbp early here to avoid
			 * a possible deadlock situation where we have the
			 * fbp and want the directory inode and someone doing
			 * a ufs_direnter has the directory inode and wants the
			 * fbp.  XXX - is this still needed?
			 */
			ep_ino = (ino_t)ep->d_ino;
#if 1
			if (fbp == 0)
				printf("ufs_dirlook:fbrelse: invalid fbp=0\n");
#endif
			fbrelse(fbp, S_OTHER);
			fbp = NULL;

			/*
			 * Atomic update (read lock held)
			 */
			dp->i_diroff = offset;

			if (namlen == 2 && namep[0] == '.' && namep[1] == '.') {
				struct timeval32 omtime;

				if (doingchk) {
					/*
					 * if the inumber didn't change
					 * continue with already found inode.
					 */
					if (ep_ino == chkino)
						goto checkok;
					else {
						VN_RELE(ITOV(*ipp));
						rw_exit(&dp->i_rwlock);
						goto restart;
					}
				}
				/*
				 * release the lock on the dir we are searching
				 * to avoid a deadlock when grabbing the
				 * i_contents lock in ufs_iget().
				 */
				omtime = dp->i_mtime;
				rw_exit(&dp->i_rwlock);
				rw_enter(&dp->i_ufsvfs->vfs_dqrwlock,
						RW_READER);
				err = ufs_iget(dp->i_vfs, ep_ino, ipp, cr);
				rw_exit(&dp->i_ufsvfs->vfs_dqrwlock);
				rw_enter(&dp->i_rwlock, RW_READER);
				if (err)
					goto bad;
				/*
				 * Since we released the lock on the directory,
				 * we must check that the same inode is still
				 * the ".." entry for this directory.
				 */
				/*CSTYLED*/
				if (timercmp(&omtime, &dp->i_mtime, !=)) {
					/*
					 * Modification time changed on the
					 * directory, we must go check if
					 * the inumber changed for ".."
					 */
					doingchk = 1;
					chkino = ep_ino;
					entryoffsetinblock = 0;
					goto recheck;
				}
			} else if (dp->i_number == ep_ino) {
				struct vnode *vp = ITOV(dp);
				VN_HOLD(vp);	/* want ourself, "." */
				*ipp = dp;
			} else {
				rw_enter(&dp->i_ufsvfs->vfs_dqrwlock,
						RW_READER);
				err = ufs_iget(dp->i_vfs, ep_ino, ipp, cr);
				rw_exit(&dp->i_ufsvfs->vfs_dqrwlock);
				if (err)
					goto bad;
			}
checkok:
			ip = *ipp;
			dnlc_enter(ITOV(dp), namep, ITOV(ip), NOCRED);
			rw_exit(&dp->i_rwlock);
			return (0);
		}
		offset += ep->d_reclen;
		entryoffsetinblock += ep->d_reclen;
	}
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		offset = 0;
		endsearch = start_off;
		goto searchloop;
	}
	err = ENOENT;
bad:
	if (fbp)
		fbrelse(fbp, S_OTHER);
	rw_exit(&dp->i_rwlock);
	return (err);
}

/*
 * If "dircheckforname" fails to find an entry with the given name, this
 * structure holds state for "ufs_direnter" as to where there is space to put
 * an entry with that name.
 * If "dircheckforname" finds an entry with the given name, this structure
 * holds state for "dirrename" and "ufs_dirremove" as to where the entry is.
 * "status" indicates what "dircheckforname" found:
 *	NONE		name not found, large enough free slot not found,
 *			can't make large enough free slot by compacting entries
 *	COMPACT		name not found, large enough free slot not found,
 *			can make large enough free slot by compacting entries
 *	FOUND		name not found, large enough free slot found
 *	EXIST		name found
 * If "dircheckforname" fails due to an error, this structure is not filled in.
 *
 * After "dircheckforname" succeeds the values are:
 *	status	offset		size		fbp, ep
 *	------	------		----		-------
 *	NONE	end of dir	needed		not valid
 *	COMPACT	start of area	of area		not valid
 *	FOUND	start of entry	of ent		not valid
 *	EXIST	start if entry	of prev ent	valid
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
 * On success, "dirprepareentry" makes "fbp" and "ep" valid.
 */
struct slot {
	enum	{NONE, COMPACT, FOUND, EXIST} status;
	off_t	offset;		/* offset of area with free space */
	int	size;		/* size of area at slotoffset */
	struct	fbuf *fbp;	/* dir buf where slot is */
	struct direct *ep;	/* pointer to slot */
	off_t	endoff;		/* last useful location found in search */
};

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
ufs_direnter(tdp, namep, op, sdp, sip, vap, ipp, cr)
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
		return (ufs_fault(ITOV(tdp), "ufs_direnter: bad namelen == 0"));

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
			 * ufs_dirlook will acquire the i_rwlock
			 */
			rw_exit(&tdp->i_rwlock);
			if (err = ufs_dirlook(tdp, namep, ipp, cr, 0)) {
				rw_enter(&tdp->i_rwlock, RW_WRITER);
				return (err);
			}
			rw_enter(&tdp->i_rwlock, RW_WRITER);
		}
		return (EEXIST);
	}
	tip = NULL;
	slot.status = NONE;
	slot.fbp = NULL;
	/*
	 * For link and rename lock the source entry and check the link count
	 * to see if it has been removed while it was unlocked.  If not, we
	 * increment the link count and force the inode to disk to make sure
	 * that it is there before any directory entry that points to it.
	 */
	if (op == DE_LINK || op == DE_RENAME) {
		/*
		 * We are about to push the inode to disk. We make sure
		 * that the inode's data blocks are flushed first so the
		 * inode and it's data blocks are always in sync.  This
		 * adds some robustness in in the event of a power failure
		 * or panic where sync fails. If we panic before the
		 * inode is updated, then the inode still refers to the
		 * old data blocks (or none for a new file). If we panic
		 * after the inode is updated, then the inode refers to
		 * the new data blocks.
		 *
		 * We do this before grabbing the i_contents lock because
		 * ufs_syncip() will want that lock. We could do the data
		 * syncing after the removal checks, but upon return from
		 * the data sync we would have to repeat the removal
		 * checks.
		 */
		if (err = TRANS_SYNCIP(sip, 0, I_DSYNC, TOP_FSYNC)) {
			return (err);
		}

		rw_enter(&sip->i_contents, RW_WRITER);
		if (sip->i_nlink == 0) {
			rw_exit(&sip->i_contents);
			return (ENOENT);
		}
		if (sip->i_nlink == MAXLINK) {
			rw_exit(&sip->i_contents);
			return (EMLINK);
		}

		/*
		 * Sync the indirect blocks associated with the file
		 * for the same reasons as described above.  Since this
		 * call wants the i_contents lock held for it we can do
		 * this here with no extra work.
		 */
		if (err = ufs_sync_indir(sip)) {
			rw_exit(&sip->i_contents);
			return (err);
		}

		sip->i_nlink++;
		TRANS_INODE(sip->i_ufsvfs, sip);
		sip->i_flag |= ICHG;
		ufs_iupdat(sip, 1);
		rw_exit(&sip->i_contents);
	}
	/*
	 * If target directory has not been removed, then we can consider
	 * allowing file to be created.
	 */
	if (tdp->i_nlink == 0) {
		err = ENOENT;
		goto out2;
	}
	/*
	 * Check accessibility of directory.
	 */
	if ((tdp->i_mode & IFMT) != IFDIR) {
		err = ENOTDIR;
		goto out2;
	}
	/*
	 * Execute access is required to search the directory.
	 */
	if (err = ufs_iaccess(tdp, IEXEC, cr)) {
		goto out2;
	}

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
			goto out2;
		}
		rw_enter(&sip->i_contents, RW_READER);
		if ((sip->i_mode & IFMT) == IFDIR && sdp != tdp) {
			ino_t	inum;

			if ((err = ufs_iaccess(sip, IWRITE, cr))) {
				rw_exit(&sip->i_contents);
				goto out2;
			}
			inum = sip->i_number;
			rw_exit(&sip->i_contents);
			if ((err = ufs_dircheckpath(inum, tdp, cr))) {
				goto out2;
			}
		} else
			rw_exit(&sip->i_contents);
	}
	/*
	 * Search for the entry. Return VN_HELD tip if found.
	 */
	rw_enter(&tdp->i_ufsvfs->vfs_dqrwlock, RW_READER);
	rw_enter(&tdp->i_contents, RW_WRITER);
	if (err = ufs_dircheckforname(tdp, namep, namlen, &slot, &tip, cr)) {
		goto out;
	}

	if (tip) {
		switch (op) {
		case DE_CREATE:
		case DE_MKDIR:
			if (ipp) {
				*ipp = tip;
				err = EEXIST;
			} else {
				VN_RELE(ITOV(tip));
			}
			break;

		case DE_RENAME:
			err = ufs_dirrename(sdp, sip, tdp, namep,
			    tip, &slot, cr);
			/*
			 * Release and potentially delete tip below to
			 * avoid deadlock with ufs_delete() - quota problem?.
			 */
			break;

		case DE_LINK:
			/*
			 * Can't link to an existing file.
			 */
			VN_RELE(ITOV(tip));
			err = EEXIST;
			break;
		}
	} else {
		/*
		 * The entry does not exist. Check write permission in
		 * directory to see if entry can be created.
		 */
		if (err = ufs_iaccess(tdp, IWRITE, cr))
			goto out;
		if (op == DE_CREATE || op == DE_MKDIR) {
			/*
			 * Make new inode and directory entry as required.
			 */
			if (err = ufs_dirmakeinode(tdp, &sip, vap, op, cr))
				goto out;
		}
		if (err = ufs_diraddentry(tdp, namep, op,
		    namlen, &slot, sip, sdp, cr)) {
			if (op == DE_CREATE || op == DE_MKDIR) {
				/*
				 * Unmake the inode we just made.
				 */
				rw_enter(&sip->i_contents, RW_WRITER);
				if ((sip->i_mode & IFMT) == IFDIR) {
					tdp->i_nlink--;
					ufs_setreclaim(tdp);
					tdp->i_flag |= ICHG;
					TRANS_INODE(tdp->i_ufsvfs, tdp);
					ITIMES_NOLOCK(tdp);
				}
				sip->i_nlink = 0;
				ufs_setreclaim(sip);
				TRANS_INODE(sip->i_ufsvfs, sip);
				sip->i_flag |= ICHG;
				ITIMES_NOLOCK(sip);
				rw_exit(&sip->i_contents);
				VN_RELE(ITOV(sip));
				sip = NULL;
			}
		} else if (ipp) {
			*ipp = sip;
		} else if (op == DE_CREATE || op == DE_MKDIR) {
			VN_RELE(ITOV(sip));
		}
	}

out:
	ASSERT(RW_WRITE_HELD(&tdp->i_contents));

	if (slot.fbp)
		fbrelse(slot.fbp, S_OTHER);

	rw_exit(&tdp->i_contents);
	rw_exit(&tdp->i_ufsvfs->vfs_dqrwlock);

	/*
	 * If we renamed a file over the top of an existing file
	 * (or tried to), then release and delete (or just release)
	 * the inode after we drop vfs_dqrwlock to avoid deadlock
	 * since ufs_delete() grabs vfs_dqrwlock as reader.
	 */
	if (op == DE_RENAME && tip) {
		VN_RELE(ITOV(tip));
	}

out2:
	if (err && (op == DE_LINK || op == DE_RENAME)) {
		/*
		 * Undo bumped link count.
		 */
		rw_enter(&sip->i_contents, RW_WRITER);
		sip->i_nlink--;
		ufs_setreclaim(sip);
		TRANS_INODE(sip->i_ufsvfs, sip);
		sip->i_flag |= ICHG;
		ITIMES_NOLOCK(sip);
		rw_exit(&sip->i_contents);
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
ufs_dircheckforname(tdp, namep, namlen, slotp, ipp, cr)
	register struct inode *tdp;	/* inode of directory being checked */
	char *namep;			/* name we're checking for */
	register int namlen;		/* length of name */
	register struct slot *slotp;	/* slot structure */
	struct inode **ipp;		/* return inode if we find one */
	struct cred *cr;
{
	off_t dirsize;			/* size of the directory */
	struct fbuf *fbp;		/* pointer to directory block */
	register int entryoffsetinblk;	/* offset of ep in fbp's buffer */
	int slotfreespace;		/* free space in block */
	register struct direct *ep;	/* directory entry */
	register off_t offset;		/* offset in the directory */
	register off_t last_offset;	/* last offset */
	off_t enduseful;		/* pointer past last used dir slot */
	int i;				/* length of mangled entry */
	int needed;
	int err;

	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&tdp->i_contents));
	fbp = NULL;
	entryoffsetinblk = 0;
	needed = (int)LDIRSIZ(namlen);
	/*
	 * No point in using i_diroff since we must search whole directory
	 */
	dirsize = roundup(tdp->i_size, DIRBLKSIZ);
	enduseful = 0;
	offset = last_offset = 0;
	while (offset < dirsize) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if (blkoff(tdp->i_fs, offset) == 0) {
			if (fbp != NULL)
				fbrelse(fbp, S_OTHER);

			err = blkatoff(tdp, offset, (char **)0, &fbp);
			if (err)
				return (err);
			entryoffsetinblk = 0;
		}
		/*
		 * If still looking for a slot, and at a DIRBLKSIZ
		 * boundary, have to start looking for free space
		 * again.
		 */
		if (slotp->status == NONE &&
		    (entryoffsetinblk&(DIRBLKSIZ-1)) == 0) {
			slotp->offset = -1;
			slotfreespace = 0;
		}
		/*
		 * If the next entry is a zero length record or if the
		 * record length is invalid, then skip to the next
		 * directory block.  Complete validation checks are
		 * done if the record length is invalid.
		 *
		 * Full validation checks are slow so they are disabled
		 * by default.  Complete checks can be run by patching
		 * "dirchk" to be true.
		 *
		 * We do not have to check the validity of
		 * entryoffsetinblk here because it starts out as zero
		 * and is only incremented by d_reclen values that we
		 * validate here.
		 */
		ep = (struct direct *)(fbp->fb_addr + entryoffsetinblk);
		if (ep->d_reclen == 0 ||
		    (dirchk || (ep->d_reclen & 0x3)) &&
		    dirmangled(tdp, ep, entryoffsetinblk, offset)) {
			i = DIRBLKSIZ - (entryoffsetinblk & (DIRBLKSIZ - 1));
			offset += i;
			entryoffsetinblk += i;
			continue;
		}
		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (slotp->status != FOUND) {
			int size = ep->d_reclen;

			if (ep->d_ino != 0)
				size -= DIRSIZ(ep);
			if (size > 0) {
				if (size >= needed) {
					slotp->status = FOUND;
					slotp->offset = offset;
					slotp->size = ep->d_reclen;
				} else if (slotp->status == NONE) {
					slotfreespace += size;
					if (slotp->offset == -1)
						slotp->offset = offset;
					if (slotfreespace >= needed) {
						slotp->status = COMPACT;
						slotp->size = (int)(offset
						    + ep->d_reclen -
						    slotp->offset);
					}
				}
			}
		}
		/*
		 * Check for a name match.
		 */
		if (ep->d_ino && ep->d_namlen == namlen &&
		    *namep == *ep->d_name &&	/* fast chk 1st char */
		    bcmp(namep, ep->d_name, namlen) == 0) {

			tdp->i_diroff = offset;

			if (tdp->i_number == ep->d_ino) {
				struct vnode *vp = ITOV(tdp);

				*ipp = tdp;	/* we want ourself, ie "." */
				VN_HOLD(vp);
			} else {
				err = ufs_iget(tdp->i_vfs, (ino_t)ep->d_ino,
					    ipp, cr);
				if (err) {
					fbrelse(fbp, S_OTHER);
					return (err);
				}
			}
			slotp->status = EXIST;
			slotp->offset = offset;
			slotp->size = (int)(offset - last_offset);
			slotp->fbp = fbp;
			slotp->ep = ep;
			slotp->endoff = 0;
			return (0);
		}
		last_offset = offset;
		offset += ep->d_reclen;
		entryoffsetinblk += ep->d_reclen;
		if (ep->d_ino)
			enduseful = offset;
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
		slotp->size = DIRBLKSIZ;
		slotp->endoff = 0;
	} else {
		/*
		 * We found a slot, and will return an indication of where that
		 * slot is, as any new directory entry will be put there.
		 * Since that slot will become a useful entry, if the last
		 * useful entry we found was before this one, update the offset
		 * of the last useful entry.
		 */
		if (enduseful < slotp->offset + slotp->size)
			enduseful = slotp->offset + slotp->size;
		slotp->endoff = roundup(enduseful, DIRBLKSIZ);
	}
	*ipp = (struct inode *)NULL;
	return (0);
}

/*
 * Rename the entry in the directory tdp so that it points to
 * sip instead of tip.
 */
static int
ufs_dirrename(sdp, sip, tdp, namep, tip, slotp, cr)
	register struct inode *sdp;	/* parent directory of source */
	register struct inode *sip;	/* source inode */
	register struct inode *tdp;	/* parent directory of target */
	char *namep;			/* entry we are trying to change */
	struct inode *tip;		/* locked target inode */
	struct slot *slotp;		/* slot for entry */
	struct cred *cr;		/* credentials */
{
	int err = 0;
	int doingdirectory;

	ASSERT(sdp->i_ufsvfs != NULL);
	ASSERT(MUTEX_HELD(&sdp->i_ufsvfs->vfs_rename_lock));
	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&tdp->i_contents));
	/*
	 * Short circuit rename of something to itself.
	 */
	if (sip->i_number == tip->i_number) {
		err = ESAME;		/* special KLUDGE error code */
		return (err);
	}
	/*
	 * Everything is protected under the vfs_rename_lock so the ordering
	 * of i_contents locks doesn't matter here.
	 */
	rw_enter(&sip->i_contents, RW_READER);
	rw_enter(&tip->i_contents, RW_READER);
	/*
	 * Check that everything is on the same filesystem.
	 */
	if ((tip->i_vnode.v_vfsp != tdp->i_vnode.v_vfsp) ||
	    (tip->i_vnode.v_vfsp != sip->i_vnode.v_vfsp)) {
		err = EXDEV;		/* XXX archaic */
		goto out;
	}
	/*
	 * Must have write permission to rewrite target entry.
	 * Perform additional checks for sticky directories.
	 */
	if ((err = ufs_iaccess(tdp, IWRITE, cr)) != 0 ||
	    (err = ufs_sticky_remove_access(tdp, tip, cr)) != 0)
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
			err = EISDIR;
			goto out;
		}
		/*
		 * vn_vfslock will prevent mounts from using the directory until
		 * we are done.
		 */
		if (vn_vfslock(ITOV(tip))) {
			err = EBUSY;
			goto out;
		}
		if (ITOV(tip)->v_vfsmountedhere) {
			vn_vfsunlock(ITOV(tip));
			err = EBUSY;
			goto out;
		}
		if (!ufs_dirempty(tip, tdp->i_number, cr) || tip->i_nlink > 2) {
			vn_vfsunlock(ITOV(tip));
			err = EEXIST;	/* SIGH should be ENOTEMPTY */
			goto out;
		}
	} else if (doingdirectory) {
		err = ENOTDIR;
		goto out;
	}

	/*
	 * Rewrite the inode pointer for target name entry
	 * from the target inode (ip) to the source inode (sip).
	 * This prevents the target entry from disappearing
	 * during a crash. Mark the directory inode to reflect the changes.
	 */
	dnlc_remove(ITOV(tdp), namep);
	slotp->ep->d_ino = (int32_t)sip->i_number;
	dnlc_enter(ITOV(tdp), namep, ITOV(sip), NOCRED);

	err = TRANS_DIR(tdp, slotp->offset);
	if (err)
		fbrelse(slotp->fbp, S_OTHER);
	else
		err = ufs_fbwrite(slotp->fbp, tdp);

	slotp->fbp = NULL;
	if (err) {
		if (doingdirectory)
			vn_vfsunlock(ITOV(tip));
		goto out;
	}

	/*
	 * Upgrade to write lock on tip
	 */
	if (rw_tryupgrade(&tip->i_contents) == 0) {
		rw_exit(&tip->i_contents);
		rw_enter(&tip->i_contents, RW_WRITER);
	}

	TRANS_INODE(tdp->i_ufsvfs, tdp);
	tdp->i_flag |= IUPD|ICHG;
	ITIMES_NOLOCK(tdp);
	/*
	 * Decrement the link count of the target inode.
	 * Fix the ".." entry in sip to point to dp.
	 * This is done after the new entry is on the disk.
	 */
	tip->i_nlink--;
	TRANS_INODE(tip->i_ufsvfs, tip);
	tip->i_flag |= ICHG;
	ITIMES_NOLOCK(tip);
	if (doingdirectory) {
		/*
		 * The entry for tip no longer exists so I can unlock the
		 * vfslock.
		 */
		vn_vfsunlock(ITOV(tip));
		/*
		 * Decrement target link count once more if it was a directory.
		 */
		if (--tip->i_nlink != 0) {
			err = ufs_fault(ITOV(tip),
		    "ufs_direnter: target directory link count != 0 (%s)",
			    tip->i_fs->fs_fsmnt);
			rw_exit(&tip->i_contents);
			return (err);
		}
		TRANS_INODE(tip->i_ufsvfs, tip);
		ufs_setreclaim(tip);
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
		ufs_setreclaim(tdp);
		TRANS_INODE(tdp->i_ufsvfs, tdp);
		tdp->i_flag |= ICHG;
		ITIMES_NOLOCK(tdp);
		if (sdp != tdp) {
			rw_exit(&tip->i_contents);
			rw_exit(&sip->i_contents);
			err = ufs_dirfixdotdot(sip, sdp, tdp);
			return (err);
		}
	} else
		ufs_setreclaim(tip);
out:
	rw_exit(&tip->i_contents);
	rw_exit(&sip->i_contents);
	return (err);
}

/*
 * Fix the ".." entry of the child directory so that it points
 * to the new parent directory instead of the old one.  Routine
 * assumes that dp is a directory and that all the inodes are on
 * the same file system.
 */
static int
ufs_dirfixdotdot(dp, opdp, npdp)
	register struct inode *dp;	/* child directory */
	register struct inode *opdp;	/* old parent directory */
	register struct inode *npdp;	/* new parent directory */
{
	struct fbuf *fbp;
	struct dirtemplate *dirp;
	int err;

	ASSERT(RW_WRITE_HELD(&npdp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&npdp->i_contents));
	err = blkatoff(dp, (off_t)0, (char **)&dirp, &fbp);

	if (err || dp->i_nlink == 0 ||
	    dp->i_size < sizeof (struct dirtemplate))
		goto bad;
	if (dirp->dotdot_ino == npdp->i_number)	/* Just a no-op. */
		goto bad;
	if (dirp->dotdot_namlen != 2 ||
	    dirp->dotdot_name[0] != '.' ||
	    dirp->dotdot_name[1] != '.') {	/* Sanity check. */
		dirbad(dp, "mangled .. entry", (off_t)0);
		err = ENOTDIR;
		goto bad;
	}

	/*
	 * Increment the link count in the new parent inode and force it out.
	 */
	if (npdp->i_nlink == MAXLINK) {
		err = EMLINK;
		goto bad;
	}
	npdp->i_nlink++;
	TRANS_INODE(npdp->i_ufsvfs, npdp);
	npdp->i_flag |= ICHG;
	ufs_iupdat(npdp, 1);

	/*
	 * Rewrite the child ".." entry and force it out.
	 */
	dnlc_remove(ITOV(dp), "..");
	dirp->dotdot_ino = (uint32_t)npdp->i_number;
	dnlc_enter(ITOV(dp), "..", ITOV(npdp), NOCRED);

	err = TRANS_DIR(dp, 0);
	if (err)
		fbrelse(fbp, S_OTHER);
	else
		err = ufs_fbwrite(fbp, dp);

	fbp = NULL;
	if (err)
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
			ufs_setreclaim(opdp);
			TRANS_INODE(opdp->i_ufsvfs, opdp);
			opdp->i_flag |= ICHG;
			ufs_iupdat(opdp, 1);
		}
		rw_exit(&opdp->i_contents);
	}
	return (0);

bad:
	if (fbp)
		fbrelse(fbp, S_OTHER);
	return (err);
}

/*
 * Enter the file sip in the directory tdp with name namep.
 */
static int
ufs_diraddentry(tdp, namep, op, namlen, slotp, sip, sdp, cr)
	struct inode *tdp;
	char *namep;
	enum de_op op;
	int namlen;
	struct slot *slotp;
	struct inode *sip;
	struct inode *sdp;
	struct cred *cr;
{
	int err;

	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&tdp->i_contents));
	/*
	 * Prepare a new entry.  If the caller has not supplied an
	 * existing inode, make a new one.
	 */
	err = dirprepareentry(tdp, slotp, cr);
	if (err)
		return (err);
	/*
	 * Check inode to be linked to see if it is in the
	 * same filesystem.
	 */
	if (tdp->i_vnode.v_vfsp != sip->i_vnode.v_vfsp) {
		err = EXDEV;
		goto bad;
	}
	if ((op == DE_RENAME) && (sip->i_mode & IFMT) == IFDIR) {
		err = ufs_dirfixdotdot(sip, sdp, tdp);
		if (err)
			goto bad;
	}

	/*
	 * Fill in entry data.
	 */
	slotp->ep->d_namlen = (ushort_t)namlen;
	(void) strncpy(slotp->ep->d_name, namep, (size_t)((namlen + 4) & ~3));
	slotp->ep->d_ino = (uint32_t)sip->i_number;
	dnlc_enter(ITOV(tdp), namep, ITOV(sip), NOCRED);

	/*
	 * Write out the directory entry.
	 */
	err = TRANS_DIR(tdp, slotp->offset);
	if (err)
		fbrelse(slotp->fbp, S_OTHER);
	else
		err = ufs_fbwrite(slotp->fbp, tdp);

	slotp->fbp = NULL;
	/*
	 * If this is a rename of a directory, then we have already
	 * fixed the ".." entry to refer to the new parent. If err
	 * is true at this point, we have failed to update the new
	 * parent to refer to the renamed directory.
	 * XXX - we need to unwind the ".." fix.
	 */
	if (err)
		return (err);

	/*
	 * Mark the directory inode to reflect the changes.
	 * Truncate the directory to chop off blocks of empty entries.
	 */
	TRANS_INODE(tdp->i_ufsvfs, tdp);
	tdp->i_flag |= IUPD|ICHG;
	tdp->i_diroff = 0;
	ITIMES_NOLOCK(tdp);

	if (slotp->endoff && slotp->endoff < tdp->i_size) {
		if (!TRANS_ISTRANS(tdp->i_ufsvfs)) {
			(void) ufs_itrunc(tdp, (u_offset_t)slotp->endoff, 0,
						cr);
		}
	}
	return (0);

bad:
	/*
	 * Clear out entry prepared by dirprepareent.  At this point in time
	 * the directory block has been compacted, so go ahead and write it
	 * out now instead of compacting again later.
	 * robgXXX this may be a performance hit?
	 */
	slotp->ep->d_ino = 0;
	slotp->ep->d_namlen = 0;

	/*
	 * Don't touch err so we don't clobber the real error that got us here.
	 */
	if (TRANS_DIR(tdp, slotp->offset))
		fbrelse(slotp->fbp, S_OTHER);
	else
		(void) ufs_fbwrite(slotp->fbp, tdp);
	slotp->fbp = NULL;
	return (err);
}

/*
 * Prepare a directory slot to receive an entry.
 */
static int
dirprepareentry(dp, slotp, cr)
	register struct inode *dp;	/* directory we are working in */
	register struct slot *slotp;	/* available slot info */
	struct cred *cr;
{
	register ushort_t slotfreespace;
	register ushort_t dsize;
	register int loc;
	register struct direct *ep, *nep;
	char *dirbuf;
	off_t entryend;
	int err;

	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&dp->i_contents));
	/*
	 * If we didn't find a slot, then indicate that the
	 * new slot belongs at the end of the directory.
	 * If we found a slot, then the new entry can be
	 * put at slotp->offset.
	 */
	entryend = slotp->offset + slotp->size;
	if (slotp->status == NONE) {
		if (slotp->offset & (DIRBLKSIZ - 1)) {
			err = ufs_fault(ITOV(dp),
	"dirprepareentry: new block slotp->offset & (DIRBLKSIZ-1): %ld (%s)",
					    slotp->offset & (DIRBLKSIZ-1),
					    dp->i_fs->fs_fsmnt);
			return (err);
		}
		if (DIRBLKSIZ > dp->i_fs->fs_fsize) {
			err = ufs_fault(ITOV(dp),
"dirprepareentry: bad fs_fsize, DIRBLKSIZ: %d > dp->i_fs->fs_fsize: %d (%s)",
				DIRBLKSIZ, dp->i_fs->fs_fsize,
				dp->i_fs->fs_fsmnt);
			return (err);
		}
		/*
		 * Allocate the new block.
		 */
		err = BMAPALLOC(dp, (u_offset_t)slotp->offset,
		    (int)(blkoff(dp->i_fs, slotp->offset) + DIRBLKSIZ), cr);
		if (err) {
			return (err);
		}
		dp->i_size = entryend;
		TRANS_INODE(dp->i_ufsvfs, dp);
		dp->i_flag |= IUPD|ICHG|IATTCHG;
		ITIMES_NOLOCK(dp);
	} else if (entryend > dp->i_size) {
		/*
		 * Adjust directory size, if needed. This should never
		 * push the size past a new multiple of DIRBLKSIZ.
		 * This is an artifact of the old (4.2BSD) way of initializing
		 * directory sizes to be less than DIRBLKSIZ.
		 */
		dp->i_size = roundup(entryend, DIRBLKSIZ);
		TRANS_INODE(dp->i_ufsvfs, dp);
		dp->i_flag |= IUPD|ICHG|IATTCHG;
		ITIMES_NOLOCK(dp);
	}

	/*
	 * Get the block containing the space for the new directory entry.
	 */
	err = blkatoff(dp, slotp->offset, (char **)&slotp->ep, &slotp->fbp);
	if (err)
		return (err);

	ep = slotp->ep;
	switch (slotp->status) {
	case NONE:
		/*
		 * No space in the directory. slotp->offset will be on a
		 * directory block boundary and we will write the new entry
		 * into a fresh block.
		 */
		ep->d_reclen = DIRBLKSIZ;
		break;

	case FOUND:
	case COMPACT:
		/*
		 * Found space for the new entry
		 * in the range slotp->offset to slotp->offset + slotp->size
		 * in the directory.  To use this space, we have to compact
		 * the entries located there, by copying them together towards
		 * the beginning of the block, leaving the free space in
		 * one usable chunk at the end.
		 */
		dirbuf = (char *)ep;
		dsize = (ushort_t)DIRSIZ(ep);
		slotfreespace = ep->d_reclen - dsize;
		loc = ep->d_reclen;
		while (loc < slotp->size) {
			nep = (struct direct *)(dirbuf + loc);
			if (ep->d_ino) {
				/* trim the existing slot */
				ep->d_reclen = dsize;
				ep = (struct direct *)((char *)ep + dsize);
			} else {
				/* overwrite; nothing there; header is ours */
				slotfreespace += dsize;
			}
			dsize = (ushort_t)DIRSIZ(nep);
			slotfreespace += nep->d_reclen - dsize;
			loc += nep->d_reclen;
			bcopy((caddr_t)nep, (caddr_t)ep, (size_t)dsize);
		}
		/*
		 * Update the pointer fields in the previous entry (if any).
		 * At this point, ep is the last entry in the range
		 * slotp->offset to slotp->offset + slotp->size.
		 * Slotfreespace is the now unallocated space after the
		 * ep entry that resulted from copying entries above.
		 */
		if (ep->d_ino == 0) {
			ep->d_reclen = slotfreespace + dsize;
		} else {
			ep->d_reclen = dsize;
			ep = (struct direct *)((char *)ep + dsize);
			ep->d_reclen = slotfreespace;
		}
		break;

	default:
		err = ufs_fault(ITOV(dp),
			    "dirprepareentry: invalid slot status: 0x%x (%s)",
			    slotp->status,
			    dp->i_fs->fs_fsmnt);
		return (err);
	}
	slotp->ep = ep;
	return (0);
}

/*
 * Allocate and initialize a new inode that will go into directory tdp.
 */
static int
ufs_dirmakeinode(
	struct inode *tdp,
	struct inode **ipp,
	struct vattr *vap,
	enum de_op op,
	struct cred *cr)
{
	struct inode *ip;
	enum vtype type;
	int imode;			/* mode and format as in inode */
	ino_t ipref;
	int err;

	ASSERT(vap != NULL);
	ASSERT(op == DE_CREATE || op == DE_MKDIR);
	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));
	ASSERT(RW_WRITE_HELD(&tdp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&tdp->i_contents));
	/*
	 * Allocate a new inode.
	 */
	type = vap->va_type;
	if (type == VDIR) {
		ipref = dirpref(tdp->i_ufsvfs);
	} else {
		ipref = tdp->i_number;
	}
	imode = MAKEIMODE(type, vap->va_mode);
	err = ufs_ialloc(tdp, ipref, imode, &ip, cr);
	if (err)
		return (err);
	/*
	 * We don't need to grab vfs_dqrwlock here because it is held
	 * in ufs_direnter() above us.
	 */
	ASSERT(RW_READ_HELD(&ip->i_ufsvfs->vfs_dqrwlock));
	rw_enter(&ip->i_contents, RW_WRITER);
	if (ip->i_dquot != NULL) {
		err = ufs_fault(ITOV(ip),
	    "ufs_dirmakeinode, ufs_direnter: ip->i_dquot != NULL: dquot (%s)",
				    tdp->i_fs->fs_fsmnt);
		rw_exit(&ip->i_contents);
		return (err);
	}
	ip->i_mode = (o_mode_t)imode;
	if (type == VBLK || type == VCHR) {
		dev_t d = vap->va_rdev;
		dev32_t dev32;

		/*
		 * Don't allow a special file to be created with a
		 * dev_t that cannot be represented by this filesystem
		 * format on disk.
		 */
		if (!cmpldev(&dev32, d)) {
			err = EOVERFLOW;
			goto fail;
		}

		ip->i_vnode.v_rdev = ip->i_rdev = d;

		if (dev32 & ~((O_MAXMAJ << L_BITSMINOR32) | O_MAXMIN)) {
			ip->i_ordev = dev32; /* can't use old format */
		} else {
			ip->i_ordev = cmpdev(d);
		}
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
	} else
		ip->i_gid = (tdp->i_mode & ISGID) ? tdp->i_gid : cr->cr_gid;

	/*
	 * For SunOS 5.0->5.4, the lines below read:
	 *
	 * ip->i_suid = (ip->i_uid > MAXUID) ? UID_LONG : ip->i_uid;
	 * ip->i_sgid = (ip->i_gid > MAXUID) ? GID_LONG : ip->i_gid;
	 *
	 * where MAXUID was set to 60002.  See notes on this in ufs_inode.c
	 */
	ip->i_suid = (ulong_t)ip->i_uid > (ulong_t)USHRT_MAX ?
		UID_LONG : ip->i_uid;
	ip->i_sgid = (ulong_t)ip->i_gid > (ulong_t)USHRT_MAX ?
		GID_LONG : ip->i_gid;

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

	if (((vap->va_mask & AT_ATIME) && TIMESPEC_OVERFLOW(&vap->va_atime)) ||
	    ((vap->va_mask & AT_MTIME) && TIMESPEC_OVERFLOW(&vap->va_mtime))) {
		err = EOVERFLOW;
		goto fail;
	}

	ip->i_dquot = getinoquota(ip);
	if (op == DE_MKDIR) {
		err = ufs_dirmakedirect(ip, tdp, cr);
		if (err)
			goto fail;
	}

	/*
	 * generate the shadow inode and attach it to the new object
	 */
	ASSERT((tdp->i_shadow && tdp->i_ufs_acl) ||
	    (!tdp->i_shadow && !tdp->i_ufs_acl));
	if (tdp->i_shadow && tdp->i_ufs_acl) {
		err = ufs_si_inherit(ip, tdp, ip->i_mode, cr);
		if (err) {
			if (op == DE_MKDIR) {
				/*
				 * clean up parent directory
				 *
				 * tdp->i_contents already locked from
				 * ufs_direnter()
				 */
				tdp->i_nlink--;
				TRANS_INODE(tdp->i_ufsvfs, tdp);
				tdp->i_flag |= ICHG;
				ufs_iupdat(tdp, 1);
			}
			goto fail;
		}
	}

	/*
	 * If the passed in attributes contain atime and/or mtime
	 * settings, then use them instead of using the current
	 * high resolution time.
	 */
	if (vap->va_mask & (AT_MTIME|AT_ATIME)) {
		if (vap->va_mask & AT_ATIME) {
			ip->i_atime.tv_sec = vap->va_atime.tv_sec;
			ip->i_atime.tv_usec = vap->va_atime.tv_nsec / 1000;
			ip->i_flag &= ~IACC;
		} else
			ip->i_flag |= IACC;
		if (vap->va_mask & AT_MTIME) {
			ip->i_mtime.tv_sec = vap->va_mtime.tv_sec;
			ip->i_mtime.tv_usec = vap->va_mtime.tv_nsec / 1000;
			if (hrestime.tv_sec > TIME32_MAX) {
				/*
				 * In 2038, ctime sticks forever..
				 */
				ip->i_ctime.tv_sec = TIME32_MAX;
				ip->i_ctime.tv_usec = 0;
			} else {
				ip->i_ctime.tv_sec = hrestime.tv_sec;
				ip->i_ctime.tv_usec = hrestime.tv_nsec / 1000;
			}
			ip->i_flag &= ~(IUPD|ICHG);
			ip->i_flag |= IMODTIME;
		} else
			ip->i_flag |= IUPD|ICHG;
		ip->i_flag |= IMOD;
	} else
		ip->i_flag |= IACC|IUPD|ICHG;
	/*
	 * push inode before it's name appears in a directory
	 */
	TRANS_INODE(ip->i_ufsvfs, ip);
	ufs_iupdat(ip, 1);
	*ipp = ip;
	rw_exit(&ip->i_contents);
	return (0);

fail:
	/* Throw away inode we just allocated. */
	ip->i_nlink = 0;
	ufs_setreclaim(ip);
	TRANS_INODE(ip->i_ufsvfs, ip);
	ip->i_flag |= ICHG;
	ITIMES_NOLOCK(ip);
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
	return (err);
}

/*
 * Write a prototype directory into the empty inode ip, whose parent is dp.
 */
static int
ufs_dirmakedirect(ip, dp, cr)
	register struct inode *ip;		/* new directory */
	register struct inode *dp;		/* parent directory */
	struct cred *cr;
{
	int err;
	register struct dirtemplate *dirp;
	struct fbuf *fbp;

	ASSERT(RW_WRITE_HELD(&ip->i_contents));
	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	ASSERT(RW_WRITE_HELD(&dp->i_contents));
	/*
	 * Allocate space for the directory we're creating.
	 */
	err = BMAPALLOC(ip, (u_offset_t)0, DIRBLKSIZ, cr);
	if (err)
		return (err);
	if (DIRBLKSIZ > dp->i_fs->fs_fsize) {
		err = ufs_fault(ITOV(dp),
"ufs_dirmakedirect: bad fs_fsize, DIRBLKSIZ: %d > dp->i_fs->fs_fsize: %d (%s)",
					DIRBLKSIZ, dp->i_fs->fs_fsize,
					dp->i_fs->fs_fsmnt);
		return (err);
	}
	ip->i_size = DIRBLKSIZ;
	TRANS_INODE(ip->i_ufsvfs, ip);
	ip->i_flag |= IUPD|ICHG|IATTCHG;
	ITIMES_NOLOCK(ip);
	/*
	 * Update the tdp link count and write out the change.
	 * This reflects the ".." entry we'll soon write.
	 */
	if (dp->i_nlink == MAXLINK)
		return (EMLINK);
	dp->i_nlink++;
	TRANS_INODE(dp->i_ufsvfs, dp);
	dp->i_flag |= ICHG;
	ufs_iupdat(dp, 1);
	/*
	 * Initialize directory with "."
	 * and ".." from static template.
	 *
	 * Since the parent directory is locked, we don't have to
	 * worry about anything changing when we drop the write
	 * lock on (ip).
	 *
	 */
	err = fbread(ITOV(ip), (offset_t)0, (uint_t)ip->i_fs->fs_fsize,
	    S_READ, &fbp);

	if (err) {
		goto fail;
	}
	dirp = (struct dirtemplate *)fbp->fb_addr;
	/*
	 * Now initialize the directory we're creating
	 * with the "." and ".." entries.
	 */
	*dirp = mastertemplate;			/* structure assignment */
	dirp->dot_ino = (uint32_t)ip->i_number;
	dirp->dotdot_ino = (uint32_t)dp->i_number;

	err = TRANS_DIR(ip, 0);
	if (err) {
		fbrelse(fbp, S_OTHER);
		goto fail;
	}

	err = ufs_fbwrite(fbp, ip);
	if (err) {
		goto fail;
	}

	return (0);

fail:
	dp->i_nlink--;
	TRANS_INODE(dp->i_ufsvfs, dp);
	dp->i_flag |= ICHG;
	ufs_iupdat(dp, 1);
	return (err);
}

/*
 * Delete a directory entry.  If oip is nonzero the entry is checked
 * to make sure it still reflects oip.
 */
int
ufs_dirremove(dp, namep, oip, cdir, op, cr)
	register struct inode *dp;
	char *namep;
	struct inode *oip;
	struct vnode *cdir;
	enum dr_op op;
	struct cred *cr;
{
	register struct direct *ep;
	struct direct *pep;
	struct inode *ip;
	int namlen;
	struct slot slot;
	int err = 0;
	int mode;

	namlen = (int)strlen(namep);
	if (namlen == 0)
		return (ufs_fault(ITOV(dp), "ufs_dirremove: namlen == 0"));
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

	ASSERT(RW_WRITE_HELD(&dp->i_rwlock));
	/*
	 * Check accessibility of directory.
	 */
	if ((dp->i_mode & IFMT) != IFDIR) {
		return (ENOTDIR);
	}

	/*
	 * Execute access is required to search the directory.
	 * Access for write is interpreted as allowing
	 * deletion of files in the directory.
	 */
	if (err = ufs_iaccess(dp, IEXEC|IWRITE, cr)) {
		return (err);
	}

	ip = NULL;
	slot.fbp = NULL;
	slot.status = FOUND;	/* don't need to look for empty slot */
	rw_enter(&dp->i_ufsvfs->vfs_dqrwlock, RW_READER);
	rw_enter(&dp->i_contents, RW_WRITER);
	if (err = ufs_dircheckforname(dp, namep, namlen, &slot, &ip, cr))
		goto out_novfs;
	if (ip == NULL) {
		err = ENOENT;
		goto out_novfs;
	}
	if (oip && oip != ip) {
		err = ENOENT;
		goto out_novfs;
	}

	if ((mode = ip->i_mode & IFMT) == IFDIR) {
		/*
		 * vn_vfslock() prevents races between mount and rmdir.
		 */
		if (vn_vfslock(ITOV(ip))) {
			err = EBUSY;
			goto out_novfs;
		}
		if (ITOV(ip)->v_vfsmountedhere != NULL && op != DR_RENAME) {
			err = EBUSY;
			goto out;
		}
		/*
		 * If we are removing a directory, get a lock on it.
		 * If the directory is empty, it will stay empty until
		 * we can remove it.
		 */
		rw_enter(&ip->i_rwlock, RW_READER);
	}
	rw_enter(&ip->i_contents, RW_READER);

	/*
	 * Now check the restrictions that apply on sticky directories.
	 */
	if ((err = ufs_sticky_remove_access(dp, ip, cr)) != 0) {
		rw_exit(&ip->i_contents);
		if (mode == IFDIR)
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
			err = EINVAL;
		else if ((ip->i_mode & IFMT) != IFDIR)
			err = ENOTDIR;
		else if ((ip->i_nlink != 2) ||
		    !ufs_dirempty(ip, dp->i_number, cr)) {
			err = EEXIST;	/* SIGH should be ENOTEMPTY */
		}
		if (err) {
			rw_exit(&ip->i_contents);
			if (mode == IFDIR)
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
			err = EPERM;
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
	/*
	 * If the entry isn't the first in the directory, we must reclaim
	 * the space of the now empty record by adding the record size
	 * to the size of the previous entry.
	 */
	ep = slot.ep;
	ep->d_ino = 0;
	if ((slot.offset & (DIRBLKSIZ - 1)) != 0) {
		/*
		 * Collapse new free space into previous entry.
		 */
		pep = (struct direct *)((char *)ep - slot.size);
		pep->d_reclen += ep->d_reclen;
	}
	err = TRANS_DIR(dp, slot.offset);
	if (err)
		fbrelse(slot.fbp, S_OTHER);
	else
		err = ufs_fbwrite(slot.fbp, dp);
	slot.fbp = NULL;

	/*
	 * If we were removing a directory, it is 'gone' now so we can
	 * unlock it.
	 */
	if (mode == IFDIR)
		rw_exit(&ip->i_rwlock);

	if (err)
		goto out;

	rw_enter(&ip->i_contents, RW_WRITER);

	dp->i_flag |= IUPD|ICHG;
	ip->i_flag |= ICHG;

	TRANS_INODE(dp->i_ufsvfs, dp);
	TRANS_INODE(ip->i_ufsvfs, ip);
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
			ufs_setreclaim(dp);
			dnlc_remove(ITOV(ip), ".");
			dnlc_remove(ITOV(ip), "..");
		} else
			ip->i_nlink--;
		ufs_setreclaim(ip);
	}
	ITIMES_NOLOCK(dp);
	ITIMES_NOLOCK(ip);
	rw_exit(&ip->i_contents);
out:
	if (mode == IFDIR)
		vn_vfsunlock(ITOV(ip));
out_novfs:
	ASSERT(RW_WRITE_HELD(&dp->i_contents));

	if (slot.fbp)
		fbrelse(slot.fbp, S_OTHER);

	rw_exit(&dp->i_contents);
	rw_exit(&dp->i_ufsvfs->vfs_dqrwlock);

	/*
	 * Release (and delete) the inode after we drop vfs_dqrwlock to
	 * avoid deadlock since ufs_delete() grabs vfs_dqrwlock as reader.
	 */
	if (ip)
		VN_RELE(ITOV(ip));

	return (err);
}

/*
 * Return buffer with contents of block "offset"
 * from the beginning of directory "ip".  If "res"
 * is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 *
 */
static int
blkatoff(ip, offset, res, fbpp)
	struct inode *ip;
	off_t offset;
	char **res;
	struct fbuf **fbpp;
{
	register struct fs *fs;
	struct fbuf *fbp;
	daddr_t lbn;
	uint_t bsize;
	int err;

	CPU_STAT_ADD_K(cpu_sysinfo.ufsdirblk, 1);
	fs = ip->i_fs;
	lbn = (daddr_t)lblkno(fs, offset);
	bsize = (uint_t)blksize(fs, ip, lbn);
	err = fbread(ITOV(ip), (offset_t)(offset & fs->fs_bmask),
			bsize, S_READ, &fbp);
	if (err) {
		*fbpp = (struct fbuf *)NULL;
		return (err);
	}
	if (res)
		*res = fbp->fb_addr + blkoff(fs, offset);
	*fbpp = fbp;
	return (0);
}

/*
 * Do consistency checking:
 *	record length must be multiple of 4
 *	entry must fit in rest of its DIRBLKSIZ block
 *	record must be large enough to contain entry
 *	name is not longer than MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 * NOTE: record length must not be zero (should be checked previously).
 *       This routine is only called if dirchk is true.
 *       It would be nice to set the FSBAD flag in the super-block when
 *       this routine fails so that a fsck is forced on next reboot,
 *       but locking is a problem.
 */
static int
dirmangled(dp, ep, entryoffsetinblock, offset)
	register struct inode *dp;
	register struct direct *ep;
	int entryoffsetinblock;
	off_t offset;
{
	register int i;

	i = DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1));
	if ((ep->d_reclen & 0x3) != 0 || (int)ep->d_reclen > i ||
	    (uint_t)ep->d_reclen < DIRSIZ(ep) || ep->d_namlen > MAXNAMLEN ||
	    ep->d_ino && dirbadname(ep->d_name, (int)ep->d_namlen)) {
		dirbad(dp, "mangled entry", offset);
		return (1);
	}
	return (0);
}

static void
dirbad(struct inode *ip, char *how, off_t offset)
{
	cmn_err(CE_NOTE, "%s: bad dir ino %d at offset %ld: %s",
	    ip->i_fs->fs_fsmnt, (int)ip->i_number, offset, how);
}

static int
dirbadname(char *sp, int l)
{
	while (l--) {			/* check for nulls */
		if (*sp++ == '\0') {
			return (1);
		}
	}
	return (*sp);			/* check for terminating null */
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
ufs_dirempty(ip, parentino, cr)
	register struct inode *ip;
	ino_t parentino;
	struct cred *cr;
{
	register offset_t off;
	struct dirtemplate dbuf;
	register struct direct *dp = (struct direct *)&dbuf;
	int err, count;
	int empty = 1;	/* Assume it's empty */
#define	MINDIRSIZ (sizeof (struct dirtemplate) / 2)

	ASSERT(RW_LOCK_HELD(&ip->i_contents));

	ASSERT(ip->i_size <= (offset_t)MAXOFF_T);
	for (off = 0; off < ip->i_size; off += dp->d_reclen) {
		err = ufs_rdwri(UIO_READ, FREAD, ip, (caddr_t)dp,
		    (ssize_t)MINDIRSIZ, off, UIO_SYSSPACE, &count, cr);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (err || count != 0 || dp->d_reclen == 0) {
			empty = 0;
			break;
		}
		/* skip empty entries */
		if (dp->d_ino == 0)
			continue;
		/* accept only "." and ".." */
		if (dp->d_namlen > 2 || dp->d_name[0] != '.') {
			empty = 0;
			break;
		}
		/*
		 * At this point d_namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (dp->d_namlen == 1)
			continue;
		if (dp->d_name[1] == '.' && (ino_t)dp->d_ino == parentino)
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
ufs_dircheckpath(source_ino, target, cr)
	ino_t source_ino;
	struct inode *target;
	struct cred *cr;
{
	struct fbuf *fbp;
	struct dirtemplate *dirp;
	register struct inode *ip;
	struct ufsvfs *ufsvfsp;
	struct inode *tip;
	ino_t dotdotino;
	int err = 0;

	ASSERT(target->i_ufsvfs != NULL);
	ASSERT(MUTEX_HELD(&target->i_ufsvfs->vfs_rename_lock));
	ASSERT(RW_WRITE_HELD(&target->i_rwlock));

	ip = target;
	if (ip->i_number == source_ino) {
		err = EINVAL;
		goto out;
	}
	if (ip->i_number == UFSROOTINO)
		goto out;
	/*
	 * Search back through the directory tree, using the ".." entries.
	 * Fail any attempt to move a directory into an ancestor directory.
	 */
	fbp = NULL;
	for (;;) {
		struct vfs	*vfs;

		err = blkatoff(ip, (off_t)0, (char **)&dirp, &fbp);
		if (err)
			break;
		if (((ip->i_mode & IFMT) != IFDIR) || ip->i_nlink == 0 ||
		    ip->i_size < sizeof (struct dirtemplate)) {
			dirbad(ip, "bad size, unlinked or not dir", (off_t)0);
			err = ENOTDIR;
			break;
		}
		if (dirp->dotdot_namlen != 2 ||
		    dirp->dotdot_name[0] != '.' ||
		    dirp->dotdot_name[1] != '.') {
			dirbad(ip, "mangled .. entry", (off_t)0);
			err = ENOTDIR;		/* Sanity check */
			break;
		}
		dotdotino = (ino_t)dirp->dotdot_ino;
		if (dotdotino == source_ino) {
			err = EINVAL;
			break;
		}
		if (dotdotino == UFSROOTINO)
			break;
		if (fbp) {
			fbrelse(fbp, S_OTHER);
			fbp = NULL;
		}
		vfs = ip->i_vfs;
		ufsvfsp = ip->i_ufsvfs;

		if (ip != target) {
			rw_exit(&ip->i_rwlock);
			VN_RELE(ITOV(ip));
		}
		/*
		 * Race to get the inode.
		 */
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
		if (err = ufs_iget(vfs, dotdotino, &tip, cr)) {
			rw_exit(&ufsvfsp->vfs_dqrwlock);
			ip = NULL;
			break;
		}
		rw_exit(&ufsvfsp->vfs_dqrwlock);
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
			VN_RELE(ITOV(ip));
		}
	}
	return (err);
}
