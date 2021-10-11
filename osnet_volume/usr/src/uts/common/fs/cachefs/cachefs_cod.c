/*
 * Copyright (c) 1996-1997, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident "@(#)cachefs_cod.c   1.35     97/11/03 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/tiuser.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <netinet/in.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/bootconf.h>
#include <sys/modctl.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/fs/cachefs_fs.h>

#define	C_CACHE_VALID(TOKEN_MTIME, NEW_MTIME)   \
	((TOKEN_MTIME.tv_sec == NEW_MTIME.tv_sec) && \
		(TOKEN_MTIME.tv_nsec == NEW_MTIME.tv_nsec))

extern timestruc_t hrestime;

static int
c_cod_init_cached_object(fscache_t *fscp, cnode_t *cp, vattr_t *vap,
    cred_t *cr)
{
	int error;
	cachefs_metadata_t *mdp = &cp->c_metadata;

	ASSERT(cr != NULL);
	ASSERT(MUTEX_HELD(&cp->c_statelock));

	/* if attributes not passed in then get them */
	if (vap == NULL) {
		/* if not connected then cannot get attrs */
		if ((fscp->fs_cdconnected != CFS_CD_CONNECTED) ||
		    (fscp->fs_backvfsp == NULL))
			return (ETIMEDOUT);

		/* get backvp if necessary */
		if (cp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, cp);
			if (error)
				return (error);
		}

		/* get the attributes */
		cp->c_attr.va_mask = AT_ALL;
		error = VOP_GETATTR(cp->c_backvp, &cp->c_attr, 0, cr);
		if (error)
			return (error);
	} else {
		/* copy passed in attributes into the cnode */
		cp->c_attr = *vap;
	}

	cp->c_size = cp->c_attr.va_size;
	mdp->md_x_time = fscp->fs_cod_time;
	mdp->md_consttype = CFS_FS_CONST_CODCONST;
	cp->c_flags |= CN_UPDATED;
	return (0);
}

static int
c_cod_check_cached_object(struct fscache *fscp, struct cnode *cp,
    int verify_what, cred_t *cr)
{
	struct vattr attrs;
	int fail = 0, backhit = 0;
	int error = 0;
	cachefs_metadata_t *mdp = &cp->c_metadata;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("c_cod_check_cached_object: ENTER cp %p\n", cp);
#endif
	ASSERT(cr);
	ASSERT(MUTEX_HELD(&cp->c_statelock));

	/* nothing to do if not connected */
	if ((fscp->fs_cdconnected != CFS_CD_CONNECTED) ||
	    (fscp->fs_backvfsp == NULL))
		goto out;

	/* done if do not have to check and cod button has not been pushed */
	if (((verify_what & C_BACK_CHECK) == 0) &&
	    (C_CACHE_VALID(mdp->md_x_time, fscp->fs_cod_time)) &&
	    ((mdp->md_flags & MD_NEEDATTRS) == 0))
		goto out;

	/* get backvp if necessary */
	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error)
			goto out;
	}

	/* get the file attributes from the back fs */
	attrs.va_mask = AT_ALL;
	error = VOP_GETATTR(cp->c_backvp, &attrs, 0, cr);
	backhit = 1;
	if (error)
		goto out;

	/* if the mtime or size of the file has changed */
	if ((!C_CACHE_VALID(mdp->md_vattr.va_mtime, attrs.va_mtime) ||
	    (cp->c_size != attrs.va_size)) &&
	    ((mdp->md_flags & MD_NEEDATTRS) == 0)) {
		fail = 1;
		if (CTOV(cp)->v_pages) {
			mutex_exit(&cp->c_statelock);
			error = cachefs_putpage_common(CTOV(cp),
			    (offset_t)0, 0, B_INVAL, cr);
			mutex_enter(&cp->c_statelock);
			if (CFS_TIMEOUT(fscp, error))
				goto out;
			error = 0;
		}
		cachefs_inval_object(cp);
		if ((CTOV(cp))->v_type == VREG) {
			attrs.va_mask = AT_ALL;
			error = VOP_GETATTR(cp->c_backvp, &attrs, 0, cr);
			if (error)
				goto out;
		}
		if (CTOV(cp)->v_pages == NULL) {
			cp->c_size = attrs.va_size;
#ifdef CFSDEBUG
		} else {
			CFS_DEBUG(CFSDEBUG_VOPS)
				printf("c_cod_check: v_pages not null\n");
#endif
		}
	}

	/* toss cached acl info if ctime changed */
	if (!C_CACHE_VALID(mdp->md_vattr.va_ctime, attrs.va_ctime)) {
		cachefs_purgeacl(cp);
	}

	cp->c_attr = attrs;
	if (attrs.va_size > cp->c_size)
		cp->c_size = attrs.va_size;
	mdp->md_x_time = fscp->fs_cod_time;
	mdp->md_flags &= ~MD_NEEDATTRS;
	cachefs_cnode_setlocalstats(cp);
	cp->c_flags |= CN_UPDATED;

out:
	if (backhit != 0) {
		if (fail != 0)
			fscp->fs_stats.st_fails++;
		else
			fscp->fs_stats.st_passes++;
	}

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("c_cod_check_cached_object: EXIT\n");
#endif

	return (error);
}

/*ARGSUSED*/
static void
c_cod_modify_cached_object(struct fscache *fscp, struct cnode *cp, cred_t *cr)
{
	struct vattr attrs;
	int error = 0;
	nlink_t nlink;
	cachefs_metadata_t *mdp = &cp->c_metadata;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	ASSERT(fscp->fs_cdconnected == CFS_CD_CONNECTED);
	ASSERT(fscp->fs_backvfsp);

	fscp->fs_stats.st_modifies++;

	/* from now on, make sure we're using the server's idea of time */
	mdp->md_flags &= ~(MD_LOCALCTIME | MD_LOCALMTIME);
	mdp->md_flags |= MD_NEEDATTRS;

	/* if in write-around mode, make sure file is nocached */
	if (CFS_ISFS_WRITE_AROUND(fscp)) {
		if ((cp->c_flags & CN_NOCACHE) == 0)
			cachefs_nocache(cp);

		/*
		 * If a directory, then defer getting the new attributes
		 * until requested.  Might be a little bit faster this way.
		 */
		if (CTOV(cp)->v_type == VDIR)
			goto out;
	}

	/* get the new mtime so the next call to check_cobject does not fail */
	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error) {
			mdp->md_vattr.va_mtime.tv_sec = 0;
			goto out;
		}
	}
	attrs.va_mask = AT_ALL;
	ASSERT(cp->c_backvp != NULL);
	error = VOP_GETATTR(cp->c_backvp, &attrs, 0, cr);
	if (error) {
		mdp->md_vattr.va_mtime.tv_sec = 0;
		goto out;
	}
	nlink = cp->c_attr.va_nlink;
	cp->c_attr = attrs;
	cp->c_attr.va_nlink = nlink;
	if ((attrs.va_size > cp->c_size) || (CTOV(cp)->v_pages == NULL))
		cp->c_size = attrs.va_size;
	mdp->md_flags &= ~MD_NEEDATTRS;
	cachefs_cnode_setlocalstats(cp);
out:
	cp->c_flags |= CN_UPDATED;
}

/*ARGSUSED*/
static void
c_cod_invalidate_cached_object(struct fscache *fscp, struct cnode *cp,
    cred_t *cr)
{
	cachefs_metadata_t *mdp = &cp->c_metadata;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	mdp->md_vattr.va_mtime.tv_sec = 0;
	mdp->md_flags |= MD_NEEDATTRS;
	cp->c_flags |= CN_UPDATED;
}

/*ARGSUSED*/
static void
c_cod_convert_cached_object(struct fscache *fscp, struct cnode *cp,
    cred_t *cr)
{
	cachefs_metadata_t *mdp = &cp->c_metadata;

	ASSERT(MUTEX_HELD(&cp->c_statelock));
	mdp->md_flags |= MD_NEEDATTRS;
	mdp->md_consttype = CFS_FS_CONST_CODCONST;
	cp->c_flags |= CN_UPDATED;
}

struct cachefsops codcfsops = {
	c_cod_init_cached_object,
	c_cod_check_cached_object,
	c_cod_modify_cached_object,
	c_cod_invalidate_cached_object,
	c_cod_convert_cached_object
};
