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
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)lookup.c	1.39	99/05/24 SMI"	/* SVr4 1.18	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/proc.h>
#include <sys/vtrace.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <c2/audit.h>

/*
 * Very rarely are pathnames > 64 bytes, hence allocate space on
 * the stack for that rather then kmem_alloc it.
 */

#define	TYPICALMAXPATHLEN	64

/*
 * Lookup the user file name,
 * Handle allocation and freeing of pathname buffer, return error.
 */
int
lookupname(
	char *fnamep,			/* user pathname */
	enum uio_seg seg,		/* addr space that name is in */
	enum symfollow followlink,	/* follow sym links */
	vnode_t **dirvpp,		/* ret for ptr to parent dir vnode */
	vnode_t **compvpp)		/* ret for ptr to component vnode */
{
	char namebuf[TYPICALMAXPATHLEN + 4]; /* +4 because of bug 1170077 */
	struct pathname lookpn;
	int error;

	lookpn.pn_buf = namebuf;
	lookpn.pn_path = namebuf;
	lookpn.pn_pathlen = 0;
	lookpn.pn_bufsize = TYPICALMAXPATHLEN;

	if (seg == UIO_USERSPACE) {
		error = copyinstr(fnamep, namebuf,
		    TYPICALMAXPATHLEN, &lookpn.pn_pathlen);
	} else {
		error = copystr(fnamep, namebuf,
		    TYPICALMAXPATHLEN, &lookpn.pn_pathlen);
	}

	lookpn.pn_pathlen--; 		/* don't count the null byte */

	if (error == 0) {
#ifdef C2_AUDIT
		if (audit_active)
			audit_lookupname();
#endif
		error = lookuppn(&lookpn, NULL, followlink, dirvpp, compvpp);
	}
	if (error == ENAMETOOLONG) {
		/*
		 * Wow! This thread used a pathname > TYPICALMAXPATHLEN bytes
		 * long! Do it the old way.
		 */
		if (error = pn_get(fnamep, seg, &lookpn))
			return (error);
		error = lookuppn(&lookpn, NULL, followlink, dirvpp, compvpp);
		pn_free(&lookpn);
	}

	return (error);
}

int
lookuppn(
	struct pathname *pnp,		/* pathname to lookup */
	struct pathname *rpnp,		/* if non-NULL, return resolved path */
	enum symfollow followlink,	/* (don't) follow sym links */
	vnode_t **dirvpp,		/* ptr for parent vnode */
	vnode_t **compvpp)		/* ptr for entry vnode */
{
	vnode_t *vp;	/* current directory vp */
	vnode_t *rootvp;
	proc_t *pp = curproc;
	int lwptotal = pp->p_lwptotal;

	if (pnp->pn_pathlen == 0)
		return (ENOENT);

	if (lwptotal > 1)
		mutex_enter(&pp->p_lock);	/* for u_rdir and u_cdir */

	if ((rootvp = PTOU(pp)->u_rdir) == NULL)
		rootvp = rootdir;
	else if (rootvp != rootdir)	/* no need to VN_HOLD rootdir */
		VN_HOLD(rootvp);

	/*
	 * If pathname starts with '/', then start search at root.
	 * Otherwise, start search at current directory.
	 */
	if (pnp->pn_path[0] == '/') {
		do {
			pnp->pn_path++;
			pnp->pn_pathlen--;
		} while (pnp->pn_path[0] == '/');
		vp = rootvp;
	} else {
		vp = PTOU(pp)->u_cdir;
	}
	VN_HOLD(vp);

	if (lwptotal > 1)
		mutex_exit(&pp->p_lock);

	return (lookuppnvp(pnp, rpnp, followlink, dirvpp, compvpp,
		rootvp, vp, CRED()));
}

/*
 * Utility function to campare equality of vnodes.
 * Compare the underlying real vnodes, if there are underlying vnodes.
 * This is a more thorough comparison than the VN_CMP() macro provides.
 */
static int
vnode_match(vnode_t *vp1, vnode_t *vp2)
{
	vnode_t *realvp;

	if (VOP_REALVP(vp1, &realvp) == 0)
		vp1 = realvp;
	if (VOP_REALVP(vp2, &realvp) == 0)
		vp2 = realvp;
	return (VN_CMP(vp1, vp2));
}

/*
 * Is 'vp' the system root vnode?
 * 'rootvp' is expected to be the vnode of the current root of theprocess.
 * Do a thorough comparison for a match to root only if we are in a
 * chrooted environment.
 * We determine we are in a chrooted environment by comparing
 * 'rootvp' to 'rootdir'.
 */
#define	VNODE_IS_ROOT(vp, rootvp) \
	(VN_CMP((vp), rootdir) || \
	((rootvp) != rootdir && vnode_match((vp), (rootvp))))

/*
 * Starting at current directory, translate pathname pnp to end.
 * Leave pathname of final component in pnp, return the vnode
 * for the final component in *compvpp, and return the vnode
 * for the parent of the final component in dirvpp.
 *
 * This is the central routine in pathname translation and handles
 * multiple components in pathnames, separating them at /'s.  It also
 * implements mounted file systems and processes symbolic links.
 *
 * vp is the held vnode where the directory search should start.
 */
int
lookuppnvp(
	struct pathname *pnp,		/* pathname to lookup */
	struct pathname *rpnp,		/* if non-NULL, return resolved path */
	enum symfollow followlink,	/* (don't) follow sym links */
	vnode_t **dirvpp,		/* ptr for parent vnode */
	vnode_t **compvpp,		/* ptr for entry vnode */
	vnode_t *rootvp,		/* rootvp */
	vnode_t *vp,			/* directory to start search at */
	cred_t *cr)			/* user's credential */
{
	vnode_t *cvp;	/* current component vp */
	vnode_t *tvp;		/* addressable temp ptr */
	char component[MAXNAMELEN];	/* buffer for component (incl null) */
	int error;
	int nlink;
	int lookup_flags;

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.namei, 1);
	nlink = 0;
	cvp = NULL;
	if (rpnp)
		rpnp->pn_pathlen = 0;
	lookup_flags = dirvpp ? LOOKUP_DIR : 0;
#ifdef C2_AUDIT
	if (audit_active)
		audit_anchorpath(pnp, vp == rootvp);
#endif

	/*
	 * Eliminate any trailing slashes in the pathname.
	 */
	pn_fixslash(pnp);

next:
	/*
	 * Make sure we have a directory.
	 */
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad;
	}

	if (rpnp && VNODE_IS_ROOT(vp, rootvp))
		(void) pn_set(rpnp, "/");

	/*
	 * Process the next component of the pathname.
	 */
	if (error = pn_getcomponent(pnp, component)) {
#ifdef C2_AUDIT
		if (audit_active)
			audit_addcomponent(pnp);
#endif
		goto bad;
	}

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. "/." or ".".
	 */
	if (component[0] == 0) {
		/*
		 * If the caller was interested in the parent then
		 * return an error since we don't have the real parent.
		 */
		if (dirvpp != NULL) {
#ifdef C2_AUDIT
			if (audit_active)	/* end of path */
				(void) audit_savepath(pnp, vp, EINVAL, cr);
#endif
			VN_RELE(vp);
			if (rootvp != rootdir)
				VN_RELE(rootvp);
			return (EINVAL);
		}
#ifdef C2_AUDIT
		if (audit_active)	/* end of path */
			if (error = audit_savepath(pnp, vp, 0, cr)) {
				cvp = NULL;
				goto bad_noaudit;
			}
#endif
		(void) pn_set(pnp, ".");
		if (rpnp && rpnp->pn_pathlen == 0)
			(void) pn_set(rpnp, ".");
		if (compvpp != NULL)
			*compvpp = vp;
		else
			VN_RELE(vp);
		if (rootvp != rootdir)
			VN_RELE(rootvp);
		return (0);
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If we're at the root directory (e.g. after chroot) then
	 *    change ".." to "." so we can't get out of this subtree.
	 * 2. If this vnode is the root of a mounted file system,
	 *    then replace it with the vnode that was mounted on
	 *    so that we take the ".." in the other file system.
	 */
	if (component[0] == '.' && component[1] == '.' && component[2] == 0) {
checkforroot:
		if (VNODE_IS_ROOT(vp, rootvp)) {
			component[1] = '\0';
		} else if (vp->v_flag & VROOT) {
			cvp = vp;
			vp = vp->v_vfsp->vfs_vnodecovered;
			VN_HOLD(vp);
			VN_RELE(cvp);
			goto checkforroot;
		}
	}

	/*
	 * Perform a lookup in the current directory.
	 */
	error = VOP_LOOKUP(vp, component, &tvp, pnp, lookup_flags,
		rootvp, cr);
	cvp = tvp;
	if (error) {
		cvp = NULL;
		/*
		 * On error, return hard error if
		 * (a) we're not at the end of the pathname yet, or
		 * (b) the caller didn't want the parent directory, or
		 * (c) we failed for some reason other than a missing entry.
		 */
		if (pn_pathleft(pnp) || dirvpp == NULL || error != ENOENT)
			goto bad;
#ifdef C2_AUDIT
		if (audit_active) {	/* directory access */
			if (error = audit_savepath(pnp, vp, error, cr))
				goto bad_noaudit;
		}
#endif
		pn_setlast(pnp);
		*dirvpp = vp;
		if (compvpp != NULL)
			*compvpp = NULL;
		if (rootvp != rootdir)
			VN_RELE(rootvp);
		return (0);
	}

	/*
	 * Traverse mount points.
	 * XXX why don't we need to hold a read lock here (call vn_vfsrlock)?
	 * What prevents a concurrent update to v_vfsmountedhere?
	 * 	Possible answer: if mounting, we might not see the mount
	 *	if it is concurrently coming into existence, but that's
	 *	really not much different from the thread running a bit slower.
	 *	If unmounting, we may get into traverse() when we shouldn't,
	 *	but traverse() will catch this case for us.
	 *	(For this to work, fetching v_vfsmountedhere had better
	 *	be atomic!)
	 */
	if (cvp->v_vfsmountedhere != NULL) {
		tvp = cvp;
		if ((error = traverse(&tvp)) != 0) {
			/*
			 * It is required to assign cvp here, because
			 * traverse() will return a held vnode which
			 * may different than the vnode that was passed
			 * in (even in the error case).  If traverse()
			 * changes the vnode it releases the original,
			 * and holds the new one.
			 */
			cvp = tvp;
			goto bad;
		}
		cvp = tvp;
	}

	/*
	 * If we hit a symbolic link and there is more path to be
	 * translated or this operation does not wish to apply
	 * to a link, then place the contents of the link at the
	 * front of the remaining pathname.
	 */
	if (cvp->v_type == VLNK && (followlink == FOLLOW || pn_pathleft(pnp))) {
		struct pathname linkpath;
#ifdef C2_AUDIT
		if (audit_active) {
			if (error = audit_pathcomp(pnp, cvp, cr))
				goto bad;
		}
#endif

		if (++nlink > MAXSYMLINKS) {
			error = ELOOP;
			goto bad;
		}
		pn_alloc(&linkpath);
		if (error = pn_getsymlink(cvp, &linkpath, cr)) {
			pn_free(&linkpath);
			goto bad;
		}

#ifdef C2_AUDIT
		if (audit_active)
			audit_symlink(pnp, &linkpath);
#endif /* C2_AUDIT */

		if (pn_pathleft(&linkpath) == 0)
			(void) pn_set(&linkpath, ".");
		error = pn_insert(pnp, &linkpath);	/* linkpath before pn */
		pn_free(&linkpath);
		if (error)
			goto bad;
		VN_RELE(cvp);
		cvp = NULL;
		if (pnp->pn_pathlen == 0) {
			error = ENOENT;
			goto bad;
		}
		if (pnp->pn_path[0] == '/') {
			do {
				pnp->pn_path++;
				pnp->pn_pathlen--;
			} while (pnp->pn_path[0] == '/');
			VN_RELE(vp);
			vp = rootvp;
			VN_HOLD(vp);
		}
#ifdef C2_AUDIT
		if (audit_active)
			audit_anchorpath(pnp, vp == rootvp);
#endif
		pn_fixslash(pnp);
		goto next;
	}

	/*
	 * If rpnp is non-NULL, remember the resolved path name therein.
	 * Do not include "." components.  Collapse occurrences of
	 * "previous/..", so long as "previous" is not itself "..".
	 * Exhausting rpnp results in error ENAMETOOLONG.
	 */
	if (rpnp && strcmp(component, ".") != 0) {
		size_t len;

		if (strcmp(component, "..") == 0 &&
		    rpnp->pn_pathlen != 0 &&
		    !((rpnp->pn_pathlen > 2 &&
		    strncmp(rpnp->pn_path+rpnp->pn_pathlen-3, "/..", 3) == 0) ||
		    (rpnp->pn_pathlen == 2 &&
		    strncmp(rpnp->pn_path, "..", 2) == 0))) {
			while (rpnp->pn_pathlen &&
			    rpnp->pn_path[rpnp->pn_pathlen-1] != '/')
				rpnp->pn_pathlen--;
			if (rpnp->pn_pathlen > 1)
				rpnp->pn_pathlen--;
			rpnp->pn_path[rpnp->pn_pathlen] = '\0';
		} else {
			if (rpnp->pn_pathlen != 0 &&
			    rpnp->pn_path[rpnp->pn_pathlen-1] != '/')
				rpnp->pn_path[rpnp->pn_pathlen++] = '/';
			error = copystr(component,
			    rpnp->pn_path + rpnp->pn_pathlen,
			    rpnp->pn_bufsize - rpnp->pn_pathlen, &len);
			if (error)	/* copystr() returns ENAMETOOLONG */
				goto bad;
			rpnp->pn_pathlen += (len - 1);
			ASSERT(rpnp->pn_bufsize > rpnp->pn_pathlen);
		}
	}

	/*
	 * If no more components, return last directory (if wanted) and
	 * last component (if wanted).
	 */
	if (pn_pathleft(pnp) == 0) {
		if (dirvpp != NULL) {
			/*
			 * Check that we have the real parent and not
			 * an alias of the last component.
			 */
			if (vnode_match(vp, cvp)) {
#ifdef C2_AUDIT
				if (audit_active)
					(void) audit_savepath(pnp, cvp,
						EINVAL, cr);
#endif
				pn_setlast(pnp);
				VN_RELE(vp);
				VN_RELE(cvp);
				if (rootvp != rootdir)
					VN_RELE(rootvp);
				return (EINVAL);
			}
#ifdef C2_AUDIT
			if (audit_active) {
				if (error = audit_pathcomp(pnp, vp, cr))
					goto bad;
			}
#endif
			*dirvpp = vp;
		} else
			VN_RELE(vp);
#ifdef C2_AUDIT
		if (audit_active) {
			if (error = audit_savepath(pnp, cvp, 0, cr)) {
				VN_RELE(cvp);
				if (rootvp != rootdir)
					VN_RELE(rootvp);
				return (error);
			}
		}
#endif
		pn_setlast(pnp);
		if (rpnp) {
			if (VNODE_IS_ROOT(cvp, rootvp))
				(void) pn_set(rpnp, "/");
			else if (rpnp->pn_pathlen == 0)
				(void) pn_set(rpnp, ".");
		}
		if (compvpp != NULL)
			*compvpp = cvp;
		else
			VN_RELE(cvp);
		if (rootvp != rootdir)
			VN_RELE(rootvp);
		return (0);
	}

#ifdef C2_AUDIT
	if (audit_active) {
		if (error = audit_pathcomp(pnp, cvp, cr))
			goto bad;
	}
#endif

	/*
	 * Skip over slashes from end of last component.
	 */
	while (pnp->pn_path[0] == '/') {
		pnp->pn_path++;
		pnp->pn_pathlen--;
	}

	/*
	 * Searched through another level of directory:
	 * release previous directory handle and save new (result
	 * of lookup) as current directory.
	 */
	VN_RELE(vp);
	vp = cvp;
	cvp = NULL;
	goto next;

bad:
#ifdef C2_AUDIT
	if (audit_active)	/* reached end of path */
		(void) audit_savepath(pnp, cvp, error, cr);
bad_noaudit:
#endif
	/*
	 * Error.  Release vnodes and return.
	 */
	if (cvp)
		VN_RELE(cvp);
	VN_RELE(vp);
	if (rootvp != rootdir)
		VN_RELE(rootvp);
	return (error);
}

/*
 * Traverse a mount point.  Routine accepts a vnode pointer as a reference
 * parameter and performs the indirection, releasing the original vnode.
 */
int
traverse(vnode_t **cvpp)
{
	int error = 0;
	vnode_t *cvp;
	vnode_t *tvp;

	cvp = *cvpp;

	/*
	 * If this vnode is mounted on, then we transparently indirect
	 * to the vnode which is the root of the mounted file system.
	 * Before we do this we must check that an unmount is not in
	 * progress on this vnode.
	 */

	for (;;) {
		error = vn_vfswlock_wait(cvp);

		if (error != 0) {
			/*
			 * BUG 1165736: lookuppn() expects a held
			 * vnode to be returned because it promptly
			 * calls VN_RELE after the error return
			 */
			*cvpp = cvp;
			return (error);
		}

		/*
		 * Reached the end of the mount chain?
		 */
		if (cvp->v_vfsmountedhere == NULL) {
			vn_vfsunlock(cvp);
			break;
		}

		/*
		 * vfs is locked and then the VVFSLOCK on the covered
		 * vnode is released to avoid a deadlock situation
		 */
		vfs_lock_wait(cvp->v_vfsmountedhere);
		vn_vfsunlock(cvp);

		/*
		 * The read lock must be held across the call to VFS_ROOT() to
		 * prevent a concurrent unmount from destroying the vfs.
		 */
		if (error = VFS_ROOT(cvp->v_vfsmountedhere, &tvp)) {
			vfs_unlock(cvp->v_vfsmountedhere);
			break;
		}

		vfs_unlock(cvp->v_vfsmountedhere);
		VN_RELE(cvp);

		cvp = tvp;
	}

	*cvpp = cvp;
	return (error);
}
