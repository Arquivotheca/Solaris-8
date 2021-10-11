/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1992,1994-1996,1998-1999 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident	"@(#)nfs_srv.c	1.72	99/09/24 SMI"
/* SVr4.0 1.21 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/statvfs.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/dirent.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/mode.h>
#include <sys/acl.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/svc.h>

#include <nfs/nfs.h>
#include <nfs/export.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>

#include <sys/strsubr.h>

extern int nfs2_limit_inum;

#define	BIG(vap)	(((vap)->va_size > (u_longlong_t)MAXOFF32_T) &&  \
			((vap)->va_type == VREG || (vap)->va_type == VDIR) || \
			(((vap)->va_nodeid > MAXOFF32_T) && nfs2_limit_inum))
/*
 * These are the interface routines for the server side of the
 * Network File System.  See the NFS version 2 protocol specification
 * for a description of this interface.
 */

static int	sattr_to_vattr(struct nfssattr *, struct vattr *);
static void	acl_perm(struct vnode *, struct exportinfo *, struct vattr *,
			cred_t *);

/*
 * Some "over the wire" UNIX file types.  These are encoded
 * into the mode.  This needs to be fixed in the next rev.
 */
#define	IFMT		0170000		/* type of file */
#define	IFCHR		0020000		/* character special */
#define	IFBLK		0060000		/* block special */
#define	IFSOCK		0140000		/* socket */

/*
 * Get file attributes.
 * Returns the current attributes of the file with the given fhandle.
 */
/* ARGSUSED */
void
rfs_getattr(fhandle_t *fhp, struct nfsattrstat *ns, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	struct vattr va;

	TRACE_0(TR_FAC_NFS, TR_RFS_GETATTR_START,
		"rfs_getattr_start:");

	vp = nfs_fhtovp(fhp, exi);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_GETATTR_END,
			"rfs_getattr_end:(%S)", "stale");
		return;
	}

	/*
	 * Do the getattr.
	 */
	va.va_mask = AT_ALL;	/* we want all the attributes */
	TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
		"vop_getattr_start:(%K)", callee());
	error = VOP_GETATTR(vp, &va, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
		"vop_getattr_end:");

	/* check for overflows */
	if (!error) {
		acl_perm(vp, exi, &va, cr);
		error = vattr_to_nattr(&va, &ns->ns_attr);
	}

	VN_RELE(vp);

	ns->ns_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_GETATTR_END,
		"rfs_getattr_end:(%S)", "done");
}
fhandle_t *
rfs_getattr_getfh(fhandle_t *fhp)
{
	return (fhp);
}

/*
 * Set file attributes.
 * Sets the attributes of the file with the given fhandle.  Returns
 * the new attributes.
 */
void
rfs_setattr(struct nfssaargs *args, struct nfsattrstat *ns,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	int flag;
	vnode_t *vp;
	struct vattr va;
	struct vattr bva;
	struct flock64 bf;

	TRACE_0(TR_FAC_NFS, TR_RFS_SETATTR_START,
		"rfs_setattr_start:");

	vp = nfs_fhtovp(&args->saa_fh, exi);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_SETATTR_END,
			"rfs_setattr_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		ns->ns_status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_SETATTR_END,
			"rfs_setattr_end:(%S)", "rofs");
		return;
	}

	error = sattr_to_vattr(&args->saa_sa, &va);
	if (error) {
		VN_RELE(vp);
		ns->ns_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_SETATTR_END,
			"rfs_setattr_end:(%S)", "sattr");
		return;
	}

	/*
	 * If the client is requesting a change to the mtime,
	 * but the nanosecond field is set to 1 billion, then
	 * this is a flag to the server that it should set the
	 * atime and mtime fields to the server's current time.
	 * The 1 billion number actually came from the client
	 * as 1 million, but the units in the over the wire
	 * request are microseconds instead of nanoseconds.
	 *
	 * This is an overload of the protocol and should be
	 * documented in the NFS Version 2 protocol specification.
	 */
	if (va.va_mask & AT_MTIME) {
		if (va.va_mtime.tv_nsec == 1000000000) {
			va.va_mtime = hrestime;
			va.va_atime = hrestime;
			va.va_mask |= AT_ATIME;
			flag = 0;
		} else
			flag = ATTR_UTIME;
	} else
		flag = 0;

	/*
	 * If the filesystem is exported with nosuid, then mask off
	 * the setuid and setgid bits.
	 */
	if ((va.va_mask & AT_MODE) && vp->v_type == VREG &&
	    (exi->exi_export.ex_flags & EX_NOSUID))
		va.va_mode &= ~(VSUID | VSGID);

	/*
	 * We need to specially handle size changes because it is
	 * possible for the client to create a file with modes
	 * which indicate read-only, but with the file opened for
	 * writing.  If the client then tries to set the size of
	 * the file, then the normal access checking done in
	 * VOP_SETATTR would prevent the client from doing so,
	 * although it should be legal for it to do so.  To get
	 * around this, we do the access checking for ourselves
	 * and then use VOP_SPACE which doesn't do the access
	 * checking which VOP_SETATTR does. VOP_SPACE can only
	 * operate on VREG files, let VOP_SETATTR handle the other
	 * extremely rare cases.
	 */
	if (vp->v_type == VREG && va.va_mask & AT_SIZE) {
		bva.va_mask = AT_UID;
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(vp, &bva, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");
		if (error) {
			VN_RELE(vp);
			ns->ns_status = puterrno(error);
			TRACE_1(TR_FAC_NFS, TR_RFS_SETATTR_END,
				"rfs_setattr_end:(%S)", "getattr");
			return;
		}
		if (cr->cr_uid == bva.va_uid) {
			va.va_mask &= ~AT_SIZE;
			bf.l_type = F_WRLCK;
			bf.l_whence = 0;
			bf.l_start = (off64_t)va.va_size;
			bf.l_len = 0;
			bf.l_sysid = 0;
			bf.l_pid = 0;
			TRACE_0(TR_FAC_NFS, TR_VOP_SPACE_START,
				"vop_space_start:");
			error = VOP_SPACE(vp, F_FREESP, &bf, FWRITE,
					(offset_t)va.va_size, cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_SPACE_END,
				"vop_space_end:");
		}
	} else
		error = 0;

	/*
	 * Do the setattr.
	 */
	if (!error && va.va_mask) {
		TRACE_0(TR_FAC_NFS, TR_VOP_SETATTR_START,
			"vop_setattr_start:");
		error = VOP_SETATTR(vp, &va, flag, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_SETATTR_END,
			"vop_setattr_end:");
	}

	if (!error) {
		va.va_mask = AT_ALL;	/* get everything */
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(vp, &va, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");

		/* check for overflows */
		if (!error) {
			acl_perm(vp, exi, &va, cr);
			error = vattr_to_nattr(&va, &ns->ns_attr);
		}
	}

	/*
	 * Force modified metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, FNODSYNC, cr);

	VN_RELE(vp);

	ns->ns_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_SETATTR_END,
		"rfs_setattr_end:(%S)", "done");
}
fhandle_t *
rfs_setattr_getfh(struct nfssaargs *args)
{
	return (&args->saa_fh);
}

/*
 * Directory lookup.
 * Returns an fhandle and file attributes for file name in a directory.
 */
/* ARGSUSED */
void
rfs_lookup(struct nfsdiropargs *da, struct nfsdiropres *dr,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *dvp;
	vnode_t *vp;
	struct vattr va;
	fhandle_t *fhp = da->da_fhandle;
	struct sec_ol sec = {0, 0};
	bool_t publicfh_flag = FALSE, auth_weak = FALSE;

	TRACE_0(TR_FAC_NFS, TR_RFS_LOOKUP_START,
		"rfs_lookup_start:");

	/*
	 * Disallow NULL paths
	 */
	if (da->da_name == NULL || *da->da_name == '\0') {
		dr->dr_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_LOOKUP_END,
			"rfs_lookup_end:(%S)", "access");
		return;
	}

	/*
	 * Allow lookups from the root - the default
	 * location of the public filehandle.
	 */
	if (exi != NULL && (exi->exi_export.ex_flags & EX_PUBLIC)) {
		dvp = rootdir;
		VN_HOLD(dvp);
	} else {
		dvp = nfs_fhtovp(fhp, exi);
		if (dvp == NULL) {
			dr->dr_status = NFSERR_STALE;
			TRACE_1(TR_FAC_NFS, TR_RFS_LOOKUP_END,
				"rfs_lookup_end:(%S)", "stale");
			return;
		}
	}

	/*
	 * Fix bug 1064433,
	 * Not allow lookup beyond root.
	 * The ".." refers beyond the root of an exported filesystem
	 * if checkexport() returned non-NULL: fh_data is in exptable.
	 */
	if (strcmp(da->da_name, "..") == 0 &&
	    checkexport(&exi->exi_fsid, (fid_t *)&fhp->fh_len) != NULL) {
		VN_RELE(dvp);
		dr->dr_status = NFSERR_NOENT;
		TRACE_1(TR_FAC_NFS, TR_RFS_LOOKUP_END,
			"rfs_lookup_end:(%S)", "noent");
		return;
	}

	/*
	 * If the public filehandle is used then allow
	 * a multi-component lookup, i.e. evaluate
	 * a pathname and follow symbolic links if
	 * necessary.
	 *
	 * This may result in a vnode in another filesystem
	 * which is OK as long as the filesystem is exported.
	 */
	if (PUBLIC_FH2(fhp)) {
		publicfh_flag = TRUE;
		error = rfs_publicfh_mclookup(da->da_name, dvp, cr, &vp, &exi,
					&sec);
	} else {
		/*
		 * Do a normal single component lookup.
		 */
		TRACE_1(TR_FAC_NFS, TR_VOP_LOOKUP_START,
			"vop_lookup_start:(%K)", callee());
		error = VOP_LOOKUP(dvp, da->da_name, &vp, NULL, 0, NULL, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_LOOKUP_END,
			"vop_lookup_end:");
	}

	if (!error) {
		va.va_mask = AT_ALL;	/* we want everything */
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(vp, &va, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");
		/* check for overflows */
		if (!error) {
			acl_perm(vp, exi, &va, cr);
			error = vattr_to_nattr(&va, &dr->dr_attr);
			if (!error) {
				if (sec.sec_flags & SEC_QUERY)
					error = makefh_ol(&dr->dr_fhandle, exi,
							sec.sec_index);
				else {
					error = makefh(&dr->dr_fhandle, vp,
								exi);
					if (!error && publicfh_flag &&
						!chk_clnt_sec(exi, req))
						auth_weak = TRUE;
				}
			}
		}
		VN_RELE(vp);
	}

	VN_RELE(dvp);

	/*
	 * If it's public fh, no 0x81, and client's flavor is
	 * invalid, set WebNFS status to WNFSERR_CLNT_FLAVOR now.
	 * Then set RPC status to AUTH_TOOWEAK in common_dispatch.
	 */
	if (auth_weak)
		dr->dr_status = (enum nfsstat)WNFSERR_CLNT_FLAVOR;
	else
		dr->dr_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_LOOKUP_END,
		"rfs_lookup_end:(%S)", "done");
}
fhandle_t *
rfs_lookup_getfh(struct nfsdiropargs *da)
{
	return (da->da_fhandle);
}

/*
 * Read symbolic link.
 * Returns the string in the symbolic link at the given fhandle.
 */
/* ARGSUSED */
void
rfs_readlink(fhandle_t *fhp, struct nfsrdlnres *rl, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	struct iovec iov;
	struct uio uio;
	vnode_t *vp;
	struct vattr va;

	TRACE_0(TR_FAC_NFS, TR_RFS_READLINK_START,
		"rfs_readlink_start:");

	vp = nfs_fhtovp(fhp, exi);
	if (vp == NULL) {
		rl->rl_data = NULL;
		rl->rl_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_READLINK_END,
			"rfs_readlink_end:(%S)", "stale");
		return;
	}

	va.va_mask = AT_MODE;
	TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
		"vop_getattr_start:(%K)", callee());
	error = VOP_GETATTR(vp, &va, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
		"vop_getattr_end:");

	if (error) {
		VN_RELE(vp);
		rl->rl_data = NULL;
		rl->rl_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_READLINK_END,
			"rfs_readlink_end:(%S)", "getattr error");
		return;
	}

	if (MANDLOCK(vp, va.va_mode)) {
		VN_RELE(vp);
		rl->rl_data = NULL;
		rl->rl_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_READLINK_END,
			"rfs_readlink_end:(%S)", "access");
		return;
	}

	/*
	 * XNFS and RFC1094 require us to return ENXIO if argument
	 * is not a link. BUGID 1138002.
	 */
	if (vp->v_type != VLNK) {
		VN_RELE(vp);
		rl->rl_data = NULL;
		rl->rl_status = NFSERR_NXIO;
		TRACE_1(TR_FAC_NFS, TR_RFS_READLINK_END,
			"rfs_readlink_end:(%S)", "nxio");
		return;
	}

	/*
	 * Allocate data for pathname.  This will be freed by rfs_rlfree.
	 */
	rl->rl_data = kmem_alloc(NFS_MAXPATHLEN, KM_SLEEP);

	/*
	 * Set up io vector to read sym link data
	 */
	iov.iov_base = rl->rl_data;
	iov.iov_len = NFS_MAXPATHLEN;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_loffset = (offset_t)0;
	uio.uio_resid = NFS_MAXPATHLEN;

	/*
	 * Do the readlink.
	 */
	TRACE_1(TR_FAC_NFS, TR_VOP_READLINK_START,
		"vop_readlink_start:(%K)", callee());
	error = VOP_READLINK(vp, &uio, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_READLINK_END,
		"vop_readlink_end:");

#if 0 /* notyet */
	/*
	 * Don't do this.  It causes local disk writes when just
	 * reading the file and the overhead is deemed larger
	 * than the benefit.
	 */
	/*
	 * Force modified metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, FNODSYNC, cr);
#endif

	VN_RELE(vp);

	rl->rl_count = (uint32_t)(NFS_MAXPATHLEN - uio.uio_resid);

	/*
	 * XNFS and RFC1094 require us to return ENXIO if argument
	 * is not a link. UFS returns EINVAL if this is the case,
	 * so we do the mapping here. BUGID 1138002.
	 */
	if (error == EINVAL)
		rl->rl_status = NFSERR_NXIO;
	else
		rl->rl_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_READLINK_END,
		"rfs_readlink_end:(%S)", "done");
}
fhandle_t *
rfs_readlink_getfh(fhandle_t *fhp)
{
	return (fhp);
}
/*
 * Free data allocated by rfs_readlink
 */
void
rfs_rlfree(struct nfsrdlnres *rl)
{
	if (rl->rl_data != NULL)
		kmem_free(rl->rl_data, NFS_MAXPATHLEN);
}

/*
 * Read data.
 * Returns some data read from the file at the given fhandle.
 */
/* ARGSUSED */
void
rfs_read(struct nfsreadargs *ra, struct nfsrdresult *rr,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	vnode_t *vp;
	int error;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	mblk_t *mp;
	int alloc_err = 0;

	TRACE_0(TR_FAC_NFS, TR_RFS_READ_START,
		"rfs_read_start:");

	vp = nfs_fhtovp(&ra->ra_fhandle, exi);
	if (vp == NULL) {
		rr->rr_data = NULL;
		rr->rr_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "stale");
		return;
	}

	if (vp->v_type != VREG) {
		VN_RELE(vp);
		rr->rr_data = NULL;
		rr->rr_status = NFSERR_ISDIR;
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "isdir");
		return;
	}

	TRACE_1(TR_FAC_NFS, TR_VOP_RWLOCK_START,
		"vop_rwlock_start:(%K)", callee());
	VOP_RWLOCK(vp, 0);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWLOCK_END,
		"vop_rwlock_end:");

	va.va_mask = AT_ALL;
	TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
		"vop_getattr_start:(%K)", callee());
	error = VOP_GETATTR(vp, &va, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
		"vop_getattr_end:");

	if (error) {
		TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
			"vop_rwunlock_start:(%K)", callee());
		VOP_RWUNLOCK(vp, 0);
		TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
			"vop_rwunlock_end:");
		VN_RELE(vp);
		rr->rr_data = NULL;
		rr->rr_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "getattr error");
		return;
	}

	/*
	 * This is a kludge to allow reading of files created
	 * with no read permission.  The owner of the file
	 * is always allowed to read it.
	 */
	if (cr->cr_uid != va.va_uid) {
		TRACE_1(TR_FAC_NFS, TR_VOP_ACCESS_START,
			"vop_access_start:(%K)", callee());
		error = VOP_ACCESS(vp, VREAD, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_ACCESS_END,
			"vop_access_end:");
		if (error) {
			/*
			 * Exec is the same as read over the net because
			 * of demand loading.
			 */
			TRACE_1(TR_FAC_NFS, TR_VOP_ACCESS_START,
				"vop_access_start:(%K)", callee());
			error = VOP_ACCESS(vp, VEXEC, 0, cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_ACCESS_END,
				"vop_access_end:");
		}
		if (error) {
			TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
				"vop_rwunlock_start:(%K)", callee());
			VOP_RWUNLOCK(vp, 0);
			TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
				"vop_rwunlock_end:");
			VN_RELE(vp);
			rr->rr_data = NULL;
			rr->rr_status = puterrno(error);
			TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
				"rfs_read_end:(%S)", "access error");
			return;
		}
	}

	if (MANDLOCK(vp, va.va_mode)) {
		TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
			"vop_rwunlock_start:(%K)", callee());
		VOP_RWUNLOCK(vp, 0);
		TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
			"vop_rwunlock_end:");
		VN_RELE(vp);
		rr->rr_data = NULL;
		rr->rr_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "mand lock");
		return;
	}

	if ((u_offset_t)ra->ra_offset >= va.va_size) {
		rr->rr_count = 0;
		rr->rr_data = NULL;
		/*
		 * In this case, status is NFS_OK, but there is no data
		 * to encode. So set rr_mp to NULL.
		 */
		rr->rr_mp = NULL;
		goto done;
	}

	/*
	 * mp will contain the data to be sent out in the read reply.
	 * This will be freed after the reply has been sent out (by the
	 * driver).
	 * Let's roundup the data to a BYTES_PER_XDR_UNIT multiple, so
	 * that the call to xdrmblk_putmblk() never fails.
	 */
	mp = allocb_wait(RNDUP(ra->ra_count), BPRI_MED, STR_NOSIG,
	    &alloc_err);
	ASSERT(mp != NULL);
	ASSERT(alloc_err == 0);

	rr->rr_mp = mp;

	/*
	 * Set up io vector
	 */
	iov.iov_base = (caddr_t)mp->b_datap->db_base;
	iov.iov_len = ra->ra_count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_loffset = (offset_t)ra->ra_offset;
	uio.uio_resid = ra->ra_count;

	TRACE_0(TR_FAC_NFS, TR_VOP_READ_START,
		"vop_read_start:");
	error = VOP_READ(vp, &uio, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_READ_END,
		"vop_read_end:");

	if (error) {
		freeb(mp);
		TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
			"vop_rwunlock_start:(%K)", callee());
		VOP_RWUNLOCK(vp, 0);
		TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
			"vop_rwunlock_end:");
		VN_RELE(vp);
		rr->rr_data = NULL;
		rr->rr_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "read error");
		return;
	}

	/*
	 * Get attributes again so we can send the latest access
	 * time to the client side for his cache.
	 */
	va.va_mask = AT_ALL;
	TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
		"vop_getattr_start:(%K)", callee());
	error = VOP_GETATTR(vp, &va, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
		"vop_getattr_end:");
	if (error) {
		freeb(mp);
		TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
			"vop_rwunlock_start:(%K)", callee());
		VOP_RWUNLOCK(vp, 0);
		TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
			"vop_rwunlock_end:");
		VN_RELE(vp);
		rr->rr_data = NULL;
		rr->rr_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
			"rfs_read_end:(%S)", "read error");
		return;
	}

	rr->rr_count = (uint32_t)(ra->ra_count - uio.uio_resid);

	rr->rr_data = (char *)mp->b_datap->db_base;

done:
	TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
		"vop_rwunlock_start:(%K)", callee());
	VOP_RWUNLOCK(vp, 0);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
		"vop_rwunlock_end:");

	acl_perm(vp, exi, &va, cr);

	/* check for overflows */
	error = vattr_to_nattr(&va, &rr->rr_attr);

#if 0 /* notyet */
	/*
	 * Don't do this.  It causes local disk writes when just
	 * reading the file and the overhead is deemed larger
	 * than the benefit.
	 */
	/*
	 * Force modified metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, FNODSYNC, cr);
#endif

	VN_RELE(vp);

	rr->rr_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_READ_END,
		"rfs_read_end:(%S)", "done");
}

fhandle_t *
rfs_read_getfh(struct nfsreadargs *ra)
{
	return (&ra->ra_fhandle);
}

#define	MAX_IOVECS	12

#ifdef DEBUG
static int rfs_write_sync_hits = 0;
static int rfs_write_sync_misses = 0;
#endif

/*
 * Write data to file.
 * Returns attributes of a file after writing some data to it.
 *
 * Any changes made here, especially in error handling might have
 * to also be done in rfs_write (which clusters write requests).
 */
void
rfs_write_sync(struct nfswriteargs *wa, struct nfsattrstat *ns,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	rlim64_t rlimit;
	struct vattr va;
	struct uio uio;
	struct iovec iov[MAX_IOVECS];
	mblk_t *m;
	struct iovec *iovp;
	int iovcnt;
	cred_t *savecred;

	TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_START,
		"rfs_write_start:(%S)", "sync");

	vp = nfs_fhtovp(&wa->wa_fhandle, exi);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		ns->ns_status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "rofs");
		return;
	}

	if (vp->v_type != VREG) {
		VN_RELE(vp);
		ns->ns_status = NFSERR_ISDIR;
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "isdir");
		return;
	}

	va.va_mask = AT_UID|AT_MODE;
	TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
		"vop_getattr_start:(%K)", callee());
	error = VOP_GETATTR(vp, &va, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
		"vop_getattr_end:");

	if (error) {
		VN_RELE(vp);
		ns->ns_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "getattr error");
		return;
	}

	if (cr->cr_uid != va.va_uid) {
		/*
		 * This is a kludge to allow writes of files created
		 * with read only permission.  The owner of the file
		 * is always allowed to write it.
		 */
		TRACE_1(TR_FAC_NFS, TR_VOP_ACCESS_START,
			"vop_access_start:(%K)", callee());
		error = VOP_ACCESS(vp, VWRITE, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_ACCESS_END,
			"vop_access_end:");
		if (error) {
			VN_RELE(vp);
			ns->ns_status = puterrno(error);
			TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
				"rfs_write_end:(%S)", "access error");
			return;
		}
	}

	/*
	 * Can't access a mandatory lock file.  This might cause
	 * the NFS service thread to block forever waiting for a
	 * lock to be released that will never be released.
	 */
	if (MANDLOCK(vp, va.va_mode)) {
		VN_RELE(vp);
		ns->ns_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "mand lock");
		return;
	}

	TRACE_1(TR_FAC_NFS, TR_VOP_RWLOCK_START,
		"vop_rwlock_start:(%K)", callee());
	VOP_RWLOCK(vp, 1);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWLOCK_END,
		"vop_rwlock_end:");

	if (wa->wa_data) {
		iov[0].iov_base = wa->wa_data;
		iov[0].iov_len = wa->wa_count;
		uio.uio_iov = iov;
		uio.uio_iovcnt = 1;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_loffset = (offset_t)wa->wa_offset;
		uio.uio_resid = wa->wa_count;
		/*
		 * The limit is checked on the client. We
		 * should allow any size writes here.
		 */
		uio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
		rlimit = uio.uio_llimit - wa->wa_offset;
		if (rlimit < (rlim64_t)uio.uio_resid)
			uio.uio_resid = (uint_t)rlimit;

		/*
		 * for now we assume no append mode
		 */
		TRACE_1(TR_FAC_NFS, TR_VOP_WRITE_START,
			"vop_write_start:(%S)", "sync");
		/*
		 * We're changing creds because VM may fault and we need
		 * the cred of the current thread to be used if quota
		 * checking is enabled.
		 */
		savecred = curthread->t_cred;
		curthread->t_cred = cr;
		error = VOP_WRITE(vp, &uio, FSYNC, cr);
		curthread->t_cred = savecred;
		TRACE_0(TR_FAC_NFS, TR_VOP_WRITE_END,
			"vop_write_end:");
	} else {
		iovcnt = 0;
		for (m = wa->wa_mblk; m; m = m->b_cont)
			iovcnt++;
		if (iovcnt <= MAX_IOVECS) {
#ifdef DEBUG
			rfs_write_sync_hits++;
#endif
			iovp = iov;
		} else {
#ifdef DEBUG
			rfs_write_sync_misses++;
#endif
			iovp = kmem_alloc(sizeof (*iovp) * iovcnt, KM_SLEEP);
		}
		mblk_to_iov(wa->wa_mblk, iovp);
		uio.uio_iov = iovp;
		uio.uio_iovcnt = iovcnt;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_loffset = (offset_t)wa->wa_offset;
		uio.uio_resid = wa->wa_count;
		/*
		 * The limit is checked on the client. We
		 * should allow any size writes here.
		 */
		uio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
		rlimit = uio.uio_llimit - wa->wa_offset;
		if (rlimit < (rlim64_t)uio.uio_resid)
			uio.uio_resid = (uint_t)rlimit;

		/*
		 * For now we assume no append mode.
		 */
		TRACE_1(TR_FAC_NFS, TR_VOP_WRITE_START,
			"vop_write_start:(%S)", "iov sync");
		/*
		 * We're changing creds because VM may fault and we need
		 * the cred of the current thread to be used if quota
		 * checking is enabled.
		 */
		savecred = curthread->t_cred;
		curthread->t_cred = cr;
		error = VOP_WRITE(vp, &uio, FSYNC, cr);
		curthread->t_cred = savecred;
		TRACE_0(TR_FAC_NFS, TR_VOP_WRITE_END,
			"vop_write_end:");

		if (iovp != iov)
			kmem_free(iovp, sizeof (*iovp) * iovcnt);
	}

	TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
		"vop_rwunlock_start:(%K)", callee());
	VOP_RWUNLOCK(vp, 1);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
		"vop_rwunlock_end:");

	if (!error) {
		/*
		 * Get attributes again so we send the latest mod
		 * time to the client side for his cache.
		 */
		va.va_mask = AT_ALL;	/* now we want everything */
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(vp, &va, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");
		/* check for overflows */
		if (!error) {
			acl_perm(vp, exi, &va, cr);
			error = vattr_to_nattr(&va, &ns->ns_attr);
		}
	}

	VN_RELE(vp);

	ns->ns_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
		"rfs_write_end:(%S)", "sync");
}

struct rfs_async_write {
	struct nfswriteargs *wa;
	struct nfsattrstat *ns;
	struct svc_req *req;
	cred_t *cr;
	kthread_t *thread;
	struct rfs_async_write *list;
};

struct rfs_async_write_list {
	fhandle_t *fhp;
	kcondvar_t cv;
	struct rfs_async_write *list;
	struct rfs_async_write_list *next;
};

static struct rfs_async_write_list *rfs_async_write_head = NULL;
static kmutex_t rfs_async_write_lock;
static int rfs_write_async = 1;	/* enables write clustering if == 1 */

#define	MAXCLIOVECS	42
#define	RFSWRITE_INITVAL (enum nfsstat) -1

#ifdef DEBUG
static int rfs_write_hits = 0;
static int rfs_write_misses = 0;
#endif

/*
 * Write data to file.
 * Returns attributes of a file after writing some data to it.
 */
void
rfs_write(struct nfswriteargs *wa, struct nfsattrstat *ns,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	rlim64_t rlimit;
	struct vattr va;
	struct uio uio;
	struct rfs_async_write_list *lp;
	struct rfs_async_write_list *nlp;
	struct rfs_async_write *rp;
	struct rfs_async_write *nrp;
	struct rfs_async_write *trp;
	struct rfs_async_write *lrp;
	int data_written;
	int iovcnt;
	mblk_t *m;
	struct iovec *iovp;
	struct iovec *niovp;
	struct iovec iov[MAXCLIOVECS];
	int count;
	int rcount;
	uint_t off;
	uint_t len;
	struct rfs_async_write nrpsp;
	struct rfs_async_write_list nlpsp;
	ushort_t t_flag;
	cred_t *savecred;

	if (!rfs_write_async) {
		rfs_write_sync(wa, ns, exi, req, cr);
		return;
	}

	TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_START,
		"rfs_write_start:(%S)", "async");

	/*
	 * Initialize status to RFSWRITE_INITVAL instead of 0, since value of 0
	 * is considered an OK.
	 */
	ns->ns_status = RFSWRITE_INITVAL;

	nrp = &nrpsp;
	nrp->wa = wa;
	nrp->ns = ns;
	nrp->req = req;
	nrp->cr = cr;
	nrp->thread = curthread;

	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);

	/*
	 * Look to see if there is already a cluster started
	 * for this file.
	 */
	mutex_enter(&rfs_async_write_lock);
	for (lp = rfs_async_write_head; lp != NULL; lp = lp->next) {
		if (bcmp(&wa->wa_fhandle, lp->fhp,
		    sizeof (fhandle_t)) == 0)
			break;
	}

	/*
	 * If lp is non-NULL, then there is already a cluster
	 * started.  We need to place ourselves in the cluster
	 * list in the right place as determined by starting
	 * offset.
	 */
	if (lp != NULL) {
		rp = lp->list;
		trp = NULL;
		while (rp != NULL && rp->wa->wa_offset < wa->wa_offset) {
			trp = rp;
			rp = rp->list;
		}
		nrp->list = rp;
		if (trp == NULL)
			lp->list = nrp;
		else
			trp->list = nrp;
		while (nrp->ns->ns_status == RFSWRITE_INITVAL)
			cv_wait(&lp->cv, &rfs_async_write_lock);
		mutex_exit(&rfs_async_write_lock);
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "cluster child");
		return;
	}

	/*
	 * No cluster started yet, start one and add ourselves
	 * to the list of clusters.
	 */
	nrp->list = NULL;

	nlp = &nlpsp;
	nlp->fhp = &wa->wa_fhandle;
	cv_init(&nlp->cv, NULL, CV_DEFAULT, NULL);
	nlp->list = nrp;
	nlp->next = NULL;

	if (rfs_async_write_head == NULL) {
		rfs_async_write_head = nlp;
	} else {
		lp = rfs_async_write_head;
		while (lp->next != NULL)
			lp = lp->next;
		lp->next = nlp;
	}
	mutex_exit(&rfs_async_write_lock);

	/*
	 * Convert the file handle common to all of the requests
	 * in this cluster to a vnode.
	 */
	vp = nfs_fhtovp(&wa->wa_fhandle, exi);
	if (vp == NULL) {
		mutex_enter(&rfs_async_write_lock);
		if (rfs_async_write_head == nlp)
			rfs_async_write_head = nlp->next;
		else {
			lp = rfs_async_write_head;
			while (lp->next != nlp)
				lp = lp->next;
			lp->next = nlp->next;
		}
		t_flag = curthread->t_flag & T_WOULDBLOCK;
		for (rp = nlp->list; rp != NULL; rp = rp->list) {
			rp->ns->ns_status = NFSERR_STALE;
			rp->thread->t_flag |= t_flag;
		}
		cv_broadcast(&nlp->cv);
		mutex_exit(&rfs_async_write_lock);
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "stale");
		return;
	}

	/*
	 * Can only write regular files.  Attempts to write any
	 * other file types fail with EISDIR.
	 */
	if (vp->v_type != VREG) {
		VN_RELE(vp);
		mutex_enter(&rfs_async_write_lock);
		if (rfs_async_write_head == nlp)
			rfs_async_write_head = nlp->next;
		else {
			lp = rfs_async_write_head;
			while (lp->next != nlp)
				lp = lp->next;
			lp->next = nlp->next;
		}
		t_flag = curthread->t_flag & T_WOULDBLOCK;
		for (rp = nlp->list; rp != NULL; rp = rp->list) {
			rp->ns->ns_status = NFSERR_ISDIR;
			rp->thread->t_flag |= t_flag;
		}
		cv_broadcast(&nlp->cv);
		mutex_exit(&rfs_async_write_lock);
		TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
			"rfs_write_end:(%S)", "isdir");
		return;
	}

	/*
	 * Lock the file for writing.  This operation provides
	 * the delay which allows clusters to grow.
	 */
	TRACE_1(TR_FAC_NFS, TR_VOP_RWLOCK_START,
		"vop_wrlock_start:(%K)", callee());
	VOP_RWLOCK(vp, 1);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWLOCK_END,
		"vop_wrlock_end");

	/*
	 * Disconnect this cluster from the list of clusters.
	 * The cluster that is being dealt with must be fixed
	 * in size after this point, so there is no reason
	 * to leave it on the list so that new requests can
	 * find it.
	 *
	 * The algorithm is that the first write request will
	 * create a cluster, convert the file handle to a
	 * vnode pointer, and then lock the file for writing.
	 * This request is not likely to be clustered with
	 * any others.  However, the next request will create
	 * a new cluster and be blocked in VOP_RWLOCK while
	 * the first request is being processed.  This delay
	 * will allow more requests to be clustered in this
	 * second cluster.
	 */
	mutex_enter(&rfs_async_write_lock);
	if (rfs_async_write_head == nlp)
		rfs_async_write_head = nlp->next;
	else {
		lp = rfs_async_write_head;
		while (lp->next != nlp)
			lp = lp->next;
		lp->next = nlp->next;
	}
	mutex_exit(&rfs_async_write_lock);

	/*
	 * Step through the list of requests in this cluster.
	 * We need to check permissions to make sure that all
	 * of the requests have sufficient permission to write
	 * the file.  A cluster can be composed of requests
	 * from different clients and different users on each
	 * client.
	 *
	 * As a side effect, we also calculate the size of the
	 * byte range that this cluster encompasses.
	 */
	rp = nlp->list;
	off = rp->wa->wa_offset;
	len = (uint_t)0;
	do {
		if (rdonly(exi, rp->req)) {
			rp->ns->ns_status = NFSERR_ROFS;
			t_flag = curthread->t_flag & T_WOULDBLOCK;
			rp->thread->t_flag |= t_flag;
			continue;
		}

		va.va_mask = AT_UID|AT_MODE;
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(vp, &va, 0, rp->cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");
		if (!error) {
			if (rp->cr->cr_uid != va.va_uid) {
				/*
				 * This is a kludge to allow writes of files
				 * created with read only permission.  The
				 * owner of the file is always allowed to
				 * write it.
				 */
				TRACE_1(TR_FAC_NFS, TR_VOP_ACCESS_START,
					"vop_access_start:(%K)", callee());
				error = VOP_ACCESS(vp, VWRITE, 0, rp->cr);
				TRACE_0(TR_FAC_NFS, TR_VOP_ACCESS_END,
					"vop_access_end:");
			}
			if (!error && MANDLOCK(vp, va.va_mode))
				error = EACCES;
		}
		if (error) {
			rp->ns->ns_status = puterrno(error);
			t_flag = curthread->t_flag & T_WOULDBLOCK;
			rp->thread->t_flag |= t_flag;
			continue;
		}
		if (len < rp->wa->wa_offset + rp->wa->wa_count - off)
			len = rp->wa->wa_offset + rp->wa->wa_count - off;
	} while ((rp = rp->list) != NULL);

	/*
	 * Step through the cluster attempting to gather as many
	 * requests which are contiguous as possible.  These
	 * contiguous requests are handled via one call to VOP_WRITE
	 * instead of different calls to VOP_WRITE.  We also keep
	 * track of the fact that any data was written.
	 */
	rp = nlp->list;
	data_written = 0;
	do {
		/*
		 * Skip any requests which didn't have the right
		 * access permissions.
		 */
		if (rp->ns->ns_status != RFSWRITE_INITVAL) {
			rp = rp->list;
			continue;
		}

		/*
		 * Count the number of iovec's which are required
		 * to handle this set of requests.  One iovec is
		 * needed for each data buffer, whether addressed
		 * by wa_data or by the b_rptr pointers in the
		 * mblk chains.
		 */
		iovcnt = 0;
		lrp = rp;
		for (;;) {
			if (lrp->wa->wa_data)
				iovcnt++;
			else {
				m = lrp->wa->wa_mblk;
				while (m != NULL) {
					iovcnt++;
					m = m->b_cont;
				}
			}
			if (lrp->list == NULL ||
			    lrp->list->ns->ns_status != RFSWRITE_INITVAL ||
			    lrp->wa->wa_offset + lrp->wa->wa_count !=
			    lrp->list->wa->wa_offset) {
				lrp = lrp->list;
				break;
			}
			lrp = lrp->list;
		}

		if (iovcnt <= MAXCLIOVECS) {
#ifdef DEBUG
			rfs_write_hits++;
#endif
			niovp = iov;
		} else {
#ifdef DEBUG
			rfs_write_misses++;
#endif
			niovp = kmem_alloc(sizeof (*niovp) * iovcnt, KM_SLEEP);
		}
		/*
		 * Put together the scatter/gather iovecs.
		 */
		iovp = niovp;
		trp = rp;
		count = 0;
		do {
			if (trp->wa->wa_data) {
				iovp->iov_base = trp->wa->wa_data;
				iovp->iov_len = trp->wa->wa_count;
				iovp++;
			} else {
				m = trp->wa->wa_mblk;
				rcount = trp->wa->wa_count;
				while (m != NULL) {
					iovp->iov_base = (caddr_t)m->b_rptr;
					iovp->iov_len = (m->b_wptr - m->b_rptr);
					rcount -= iovp->iov_len;
					if (rcount < 0)
						iovp->iov_len += rcount;
					iovp++;
					if (rcount <= 0)
						break;
					m = m->b_cont;
				}
			}
			count += trp->wa->wa_count;
			trp = trp->list;
		} while (trp != lrp);

		uio.uio_iov = niovp;
		uio.uio_iovcnt = iovcnt;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_loffset = (offset_t)rp->wa->wa_offset;
		uio.uio_resid = count;
		/*
		 * The limit is checked on the client. We
		 * should allow any size writes here.
		 */
		uio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
		rlimit = uio.uio_llimit - rp->wa->wa_offset;
		if (rlimit < (rlim64_t)uio.uio_resid)
			uio.uio_resid = (uint_t)rlimit;

		/*
		 * For now we assume no append mode.
		 */
		TRACE_1(TR_FAC_NFS, TR_VOP_WRITE_START,
			"vop_write_start:(%S)", "async");
		/*
		 * We're changing creds because VM may fault and we need
		 * the cred of the current thread to be used if quota
		 * checking is enabled.
		 */
		savecred = curthread->t_cred;
		curthread->t_cred = cr;
		error = VOP_WRITE(vp, &uio, 0, rp->cr);
		curthread->t_cred = savecred;
		TRACE_0(TR_FAC_NFS, TR_VOP_WRITE_END,
			"vop_write_end:");

		if (niovp != iov)
			kmem_free(niovp, sizeof (*niovp) * iovcnt);

		if (!error) {
			data_written = 1;
			/*
			 * Get attributes again so we send the latest mod
			 * time to the client side for his cache.
			 */
			va.va_mask = AT_ALL;	/* now we want everything */
			TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
				"vop_getattr_start:(%K)", callee());
			error = VOP_GETATTR(vp, &va, 0, rp->cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
				"vop_getattr_end:");
			if (!error)
				acl_perm(vp, exi, &va, rp->cr);
		}

		/*
		 * Fill in the status responses for each request
		 * which was just handled.  Also, copy the latest
		 * attributes in to the attribute responses if
		 * appropriate.
		 */
		t_flag = curthread->t_flag & T_WOULDBLOCK;
		do {
			rp->thread->t_flag |= t_flag;
			/* check for overflows */
			if (!error) {
				error  = vattr_to_nattr(&va, &rp->ns->ns_attr);
			}
			rp->ns->ns_status = puterrno(error);
			rp = rp->list;
		} while (rp != lrp);
	} while (rp != NULL);

	/*
	 * If any data was written at all, then we need to flush
	 * the data and metadata to stable storage.
	 */
	if (data_written) {
		TRACE_0(TR_FAC_NFS, TR_VOP_PUTPAGE_START,
			"vop_putpage_start:");
		error = VOP_PUTPAGE(vp, (u_offset_t)off, len, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_PUTPAGE_END,
			"vop_putpage_end:");
		if (!error) {
			TRACE_0(TR_FAC_NFS, TR_VOP_FSYNC_START,
				"vop_fsync_start:");
			error = VOP_FSYNC(vp, FNODSYNC, cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_FSYNC_END,
				"vop_fsync_end:");
		}
	}

	TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
		"vop_rwunlock_start:(%K)", callee());
	VOP_RWUNLOCK(vp, 1);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
		"vop_rwunlock_end:");

	VN_RELE(vp);

	t_flag = curthread->t_flag & T_WOULDBLOCK;
	mutex_enter(&rfs_async_write_lock);
	for (rp = nlp->list; rp != NULL; rp = rp->list) {
		if (rp->ns->ns_status == RFSWRITE_INITVAL) {
			rp->ns->ns_status = puterrno(error);
			rp->thread->t_flag |= t_flag;
		}
	}
	cv_broadcast(&nlp->cv);
	mutex_exit(&rfs_async_write_lock);

	TRACE_1(TR_FAC_NFS, TR_RFS_WRITE_END,
		"rfs_write_end:(%S)", "async");
}
fhandle_t *
rfs_write_getfh(struct nfswriteargs *wa)
{
	return (&wa->wa_fhandle);
}

/*
 * Create a file.
 * Creates a file with given attributes and returns those attributes
 * and an fhandle for the new file.
 */
void
rfs_create(struct nfscreatargs *args, struct nfsdiropres *dr,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	struct vattr va;
	vnode_t *vp;
	vnode_t *dvp;
	char *name = args->ca_da.da_name;
	vnode_t *tvp;
	int mode;
	int lookup_ok;

	TRACE_0(TR_FAC_NFS, TR_RFS_CREATE_START,
		"rfs_create_start:");

	/*
	 * Disallow NULL paths
	 */
	if (name == NULL || *name == '\0') {
		dr->dr_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_CREATE_END,
			"rfs_create_end:(%S)", "access");
		return;
	}

	dvp = nfs_fhtovp(args->ca_da.da_fhandle, exi);
	if (dvp == NULL) {
		dr->dr_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_CREATE_END,
			"rfs_create_end:(%S)", "stale");
		return;
	}

	error = sattr_to_vattr(args->ca_sa, &va);
	if (error) {
		dr->dr_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_CREATE_END,
			"rfs_create_end:(%S)", "sattr");
		return;
	}

	/*
	 * Must specify the mode.
	 */
	if (!(va.va_mask & AT_MODE)) {
		VN_RELE(dvp);
		dr->dr_status = NFSERR_INVAL;
		TRACE_1(TR_FAC_NFS, TR_RFS_CREATE_END,
			"rfs_create_end:(%S)", "no mode");
		return;
	}

	/*
	 * This is a completely gross hack to make mknod
	 * work over the wire until we can wack the protocol
	 */
	if ((va.va_mode & IFMT) == IFCHR) {
		if (args->ca_sa->sa_size == (uint_t)NFS_FIFO_DEV)
			va.va_type = VFIFO;	/* xtra kludge for named pipe */
		else {
			va.va_type = VCHR;
			/*
			 * uncompress the received dev_t
			 * if the top half is zero indicating a request
			 * from an `older style' OS.
			 */
			if ((va.va_size & 0xffff0000) == 0)
				va.va_rdev = nfsv2_expdev(va.va_size);
			else
				va.va_rdev = (dev_t)va.va_size;
		}
		va.va_mask &= ~AT_SIZE;
	} else if ((va.va_mode & IFMT) == IFBLK) {
		va.va_type = VBLK;
		/*
		 * uncompress the received dev_t
		 * if the top half is zero indicating a request
		 * from an `older style' OS.
		 */
		if ((va.va_size & 0xffff0000) == 0)
			va.va_rdev = nfsv2_expdev(va.va_size);
		else
			va.va_rdev = (dev_t)va.va_size;
		va.va_mask &= ~AT_SIZE;
	} else if ((va.va_mode & IFMT) == IFSOCK) {
		va.va_type = VSOCK;
	} else
		va.va_type = VREG;
	va.va_mode &= ~IFMT;
	va.va_mask |= AT_TYPE;

	/*
	 * Why was the choice made to use VWRITE as the mode to the
	 * call to VOP_CREATE ? This results in a bug.  When a client
	 * opens a file that already exists and is RDONLY, the second
	 * open fails with an EACESS because of the mode.
	 * bug ID 1054648.
	 */
	lookup_ok = 0;
	mode = VWRITE;
	if (!(va.va_mask & AT_SIZE) || va.va_type != VREG) {
		TRACE_1(TR_FAC_NFS, TR_VOP_LOOKUP_START,
			"vop_lookup_start:(%K)", callee());
		error = VOP_LOOKUP(dvp, name, &tvp, NULL, 0, NULL, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_LOOKUP_END,
			"vop_lookup_end:");
		if (!error) {
			struct vattr at;

			lookup_ok = 1;
			at.va_mask = AT_MODE;
			TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
				"vop_getattr_start:(%K)", callee());
			error = VOP_GETATTR(tvp, &at, 0, cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
				"vop_getattr_end:");
			if (!error)
				mode = (at.va_mode & S_IWUSR) ? VWRITE : VREAD;
			VN_RELE(tvp);
		}
	}

	if (!lookup_ok) {
		if (rdonly(exi, req)) {
			error = EROFS;
		} else if (va.va_type != VREG && va.va_type != VFIFO &&
		    va.va_type != VSOCK && !suser(cr)) {
			error = EPERM;
		} else {
			error = 0;
		}
	}

	if (!error) {
		/*
		 * If filesystem is shared with nosuid the remove any
		 * setuid/setgid bits on create.
		 */
		if (va.va_type == VREG &&
		    exi->exi_export.ex_flags & EX_NOSUID)
			va.va_mode &= ~(VSUID | VSGID);

		TRACE_0(TR_FAC_NFS, TR_VOP_CREATE_START,
			"vop_create_start:");
		error = VOP_CREATE(dvp, name, &va, NONEXCL, mode, &vp, cr, 0);
		TRACE_0(TR_FAC_NFS, TR_VOP_CREATE_END,
			"vop_create_end:");

		if (!error) {
			va.va_mask = AT_ALL;
			TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
				"vop_getattr_start:(%K)", callee());
			error = VOP_GETATTR(vp, &va, 0, cr);
			TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
				"vop_getattr_end:");
			/* check for overflows */
			if (!error) {
				acl_perm(vp, exi, &va, cr);
				error = vattr_to_nattr(&va, &dr->dr_attr);
				if (!error) {
					error = makefh(&dr->dr_fhandle, vp,
							exi);
				}
			}
			/*
			 * Force modified metadata out to stable storage.
			 */
			(void) VOP_FSYNC(vp, FNODSYNC, cr);
			VN_RELE(vp);
		}
	}

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(dvp, 0, cr);

	VN_RELE(dvp);

	dr->dr_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_CREATE_END,
		"rfs_create_end:(%S)", "done");
}
fhandle_t *
rfs_create_getfh(struct nfscreatargs *args)
{
	return (args->ca_da.da_fhandle);
}

/*
 * Remove a file.
 * Remove named file from parent directory.
 */
void
rfs_remove(struct nfsdiropargs *da, enum nfsstat *status,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;

	TRACE_0(TR_FAC_NFS, TR_RFS_REMOVE_START,
		"rfs_remove_start:");

	/*
	 * Disallow NULL paths
	 */
	if (da->da_name == NULL || *da->da_name == '\0') {
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_REMOVE_END,
			"rfs_remove_end:(%S)", "access");
		return;
	}

	vp = nfs_fhtovp(da->da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_REMOVE_END,
			"rfs_remove_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		*status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_REMOVE_END,
			"rfs_remove_end:(%S)", "rofs");
		return;
	}

	TRACE_0(TR_FAC_NFS, TR_VOP_REMOVE_START,
		"vop_remove_start:");
	error = VOP_REMOVE(vp, da->da_name, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_REMOVE_END,
		"vop_remove_end:");

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, 0, cr);

	VN_RELE(vp);

	*status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_REMOVE_END,
		"rfs_remove_end:(%S)", "done");
}
fhandle_t *
rfs_remove_getfh(struct nfsdiropargs *da)
{
	return (da->da_fhandle);
}

/*
 * rename a file
 * Give a file (from) a new name (to).
 */
void
rfs_rename(struct nfsrnmargs *args, enum nfsstat *status,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *fromvp;
	vnode_t *tovp;
	struct exportinfo *to_exi;
	fhandle_t *fh;

	TRACE_0(TR_FAC_NFS, TR_RFS_RENAME_START,
		"rfs_rename_start:");

	fromvp = nfs_fhtovp(args->rna_from.da_fhandle, exi);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "from stale");
		return;
	}

	fh = args->rna_to.da_fhandle;
	to_exi = checkexport(&fh->fh_fsid, (fid_t *)&fh->fh_xlen);
	if (to_exi == NULL) {
		VN_RELE(fromvp);
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "cross device");
		return;
	}

	if (to_exi != exi) {
		VN_RELE(fromvp);
		*status = NFSERR_XDEV;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "from stale");
		return;
	}

	tovp = nfs_fhtovp(args->rna_to.da_fhandle, exi);
	if (tovp == NULL) {
		VN_RELE(fromvp);
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "to stale");
		return;
	}

	if (fromvp->v_type != VDIR || tovp->v_type != VDIR) {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "not dir");
		*status = NFSERR_NOTDIR;
		return;
	}

	/*
	 * Disallow NULL paths
	 */
	if (args->rna_from.da_name == NULL || *args->rna_from.da_name == '\0' ||
	    args->rna_to.da_name == NULL || *args->rna_to.da_name == '\0') {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "access");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		*status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
			"rfs_rename_end:(%S)", "rofs");
		return;
	}

	TRACE_0(TR_FAC_NFS, TR_VOP_RENAME_START,
		"vop_rename_start:");
	error = VOP_RENAME(fromvp, args->rna_from.da_name,
	    tovp, args->rna_to.da_name, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_RENAME_END,
		"vop_rename_end:");

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(tovp, 0, cr);
	(void) VOP_FSYNC(fromvp, 0, cr);

	VN_RELE(tovp);
	VN_RELE(fromvp);

	*status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_RENAME_END,
		"rfs_rename_end:(%S)", "done");
}
fhandle_t *
rfs_rename_getfh(struct nfsrnmargs *args)
{
	return (args->rna_from.da_fhandle);
}

/*
 * Link to a file.
 * Create a file (to) which is a hard link to the given file (from).
 */
void
rfs_link(struct nfslinkargs *args, enum nfsstat *status,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *fromvp;
	vnode_t *tovp;
	struct exportinfo *to_exi;
	fhandle_t *fh;

	TRACE_0(TR_FAC_NFS, TR_RFS_LINK_START,
		"rfs_link_start:");

	fromvp = nfs_fhtovp(args->la_from, exi);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "from stale");
		return;
	}

	fh = args->la_to.da_fhandle;
	to_exi = checkexport(&fh->fh_fsid, (fid_t *)&fh->fh_xlen);
	if (to_exi == NULL) {
		VN_RELE(fromvp);
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "cross device");
		return;
	}

	if (to_exi != exi) {
		VN_RELE(fromvp);
		*status = NFSERR_XDEV;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "cross device");
		return;
	}

	tovp = nfs_fhtovp(args->la_to.da_fhandle, exi);
	if (tovp == NULL) {
		VN_RELE(fromvp);
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "to stale");
		return;
	}

	if (tovp->v_type != VDIR) {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		*status = NFSERR_NOTDIR;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "not dir");
		return;
	}
	/*
	 * Disallow NULL paths
	 */
	if (args->la_to.da_name == NULL || *args->la_to.da_name == '\0') {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "access");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(tovp);
		VN_RELE(fromvp);
		*status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
			"rfs_link_end:(%S)", "rofs");
		return;
	}

	TRACE_0(TR_FAC_NFS, TR_VOP_LINK_START,
		"vop_link_start:");
	error = VOP_LINK(tovp, fromvp, args->la_to.da_name, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_LINK_END,
		"vop_link_end:");

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(tovp, 0, cr);
	(void) VOP_FSYNC(fromvp, FNODSYNC, cr);

	VN_RELE(tovp);
	VN_RELE(fromvp);

	*status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_LINK_END,
		"rfs_link_end:(%S)", "done");
}
fhandle_t *
rfs_link_getfh(struct nfslinkargs *args)
{
	return (args->la_from);
}

/*
 * Symbolicly link to a file.
 * Create a file (to) with the given attributes which is a symbolic link
 * to the given path name (to).
 */
void
rfs_symlink(struct nfsslargs *args, enum nfsstat *status,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	struct vattr va;
	vnode_t *vp;
	vnode_t *svp;
	int lerror;

	TRACE_0(TR_FAC_NFS, TR_RFS_SYMLINK_START,
		"rfs_symlink_start:");

	/*
	 * Disallow NULL paths
	 */
	if (args->sla_from.da_name == NULL || *args->sla_from.da_name == '\0') {
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
			"rfs_symlink_end:(%S)", "access");
		return;
	}

	vp = nfs_fhtovp(args->sla_from.da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
			"rfs_symlink_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		*status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
			"rfs_symlink_end:(%S)", "rofs");
		return;
	}

	error = sattr_to_vattr(args->sla_sa, &va);
	if (error) {
		VN_RELE(vp);
		*status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
			"rfs_symlink_end:(%S)", "sattr");
		return;
	}

	if (!(va.va_mask & AT_MODE)) {
		VN_RELE(vp);
		*status = NFSERR_INVAL;
		TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
			"rfs_symlink_end:(%S)", "no mode");
		return;
	}

	va.va_type = VLNK;
	va.va_mask |= AT_TYPE;

	TRACE_0(TR_FAC_NFS, TR_VOP_SYMLINK_START,
		"vop_symlink_start:");
	error = VOP_SYMLINK(vp, args->sla_from.da_name, &va, args->sla_tnm, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_SYMLINK_END,
		"vop_symlink_end:");

	/*
	 * Force new data and metadata out to stable storage.
	 */
	TRACE_1(TR_FAC_NFS, TR_VOP_LOOKUP_START,
		"vop_lookup_start:(%K)", callee());
	lerror = VOP_LOOKUP(vp, args->sla_from.da_name, &svp, NULL,
	    0, NULL, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_LOOKUP_END,
		"vop_lookup_end:");
	if (!lerror) {
		(void) VOP_FSYNC(svp, 0, cr);
		VN_RELE(svp);
	}

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, 0, cr);

	VN_RELE(vp);

	*status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_SYMLINK_END,
		"rfs_symlink_end:(%S)", "done");
}
fhandle_t *
rfs_symlink_getfh(struct nfsslargs *args)
{
	return (args->sla_from.da_fhandle);
}

/*
 * Make a directory.
 * Create a directory with the given name, parent directory, and attributes.
 * Returns a file handle and attributes for the new directory.
 */
void
rfs_mkdir(struct nfscreatargs *args, struct nfsdiropres *dr,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	struct vattr va;
	vnode_t *dvp;
	vnode_t *vp;
	char *name = args->ca_da.da_name;

	TRACE_0(TR_FAC_NFS, TR_RFS_MKDIR_START,
		"rfs_mkdir_start:");

	/*
	 * Disallow NULL paths
	 */
	if (name == NULL || *name == '\0') {
		dr->dr_status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
			"rfs_mkdir_end:(%S)", "access");
		return;
	}

	vp = nfs_fhtovp(args->ca_da.da_fhandle, exi);
	if (vp == NULL) {
		dr->dr_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
			"rfs_mkdir_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		dr->dr_status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
			"rfs_mkdir_end:(%S)", "rofs");
		return;
	}

	error = sattr_to_vattr(args->ca_sa, &va);
	if (error) {
		VN_RELE(vp);
		dr->dr_status = puterrno(error);
		TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
			"rfs_mkdir_end:(%S)", "sattr");
		return;
	}

	if (!(va.va_mask & AT_MODE)) {
		VN_RELE(vp);
		dr->dr_status = NFSERR_INVAL;
		TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
			"rfs_mkdir_end:(%S)", "no mode");
		return;
	}

	va.va_type = VDIR;
	va.va_mask |= AT_TYPE;

	TRACE_0(TR_FAC_NFS, TR_VOP_MKDIR_START,
		"vop_mkdir_start:");
	error = VOP_MKDIR(vp, name, &va, &dvp, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_MKDIR_END,
		"vop_mkdir_end:");

	if (!error) {
		/*
		 * Attribtutes of the newly created directory should
		 * be returned to the client.
		 */
		va.va_mask = AT_ALL; /* We want everything */
		TRACE_1(TR_FAC_NFS, TR_VOP_GETATTR_START,
			"vop_getattr_start:(%K)", callee());
		error = VOP_GETATTR(dvp, &va, 0, cr);
		TRACE_0(TR_FAC_NFS, TR_VOP_GETATTR_END,
			"vop_getattr_end:");
		/* check for overflows */
		if (!error) {
			acl_perm(vp, exi, &va, cr);
			error = vattr_to_nattr(&va, &dr->dr_attr);
			if (!error) {
				error = makefh(&dr->dr_fhandle, dvp, exi);
			}
		}
		/*
		 * Force new data and metadata out to stable storage.
		 */
		(void) VOP_FSYNC(dvp, 0, cr);
		VN_RELE(dvp);
	}

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, 0, cr);

	VN_RELE(vp);

	dr->dr_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_MKDIR_END,
		"rfs_mkdir_end:(%S)", "done");
}
fhandle_t *
rfs_mkdir_getfh(struct nfscreatargs *args)
{
	return (args->ca_da.da_fhandle);
}

/*
 * Remove a directory.
 * Remove the given directory name from the given parent directory.
 */
void
rfs_rmdir(struct nfsdiropargs *da, enum nfsstat *status,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;

	TRACE_0(TR_FAC_NFS, TR_RFS_RMDIR_START,
		"rfs_rmdir_start:");

	/*
	 * Disallow NULL paths
	 */
	if (da->da_name == NULL || *da->da_name == '\0') {
		*status = NFSERR_ACCES;
		TRACE_1(TR_FAC_NFS, TR_RFS_RMDIR_END,
			"rfs_rmdir_end:(%S)", "access");
		return;
	}

	vp = nfs_fhtovp(da->da_fhandle, exi);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_RMDIR_END,
			"rfs_rmdir_end:(%S)", "stale");
		return;
	}

	if (rdonly(exi, req)) {
		VN_RELE(vp);
		*status = NFSERR_ROFS;
		TRACE_1(TR_FAC_NFS, TR_RFS_RMDIR_END,
			"rfs_rmdir_end:(%S)", "rofs");
		return;
	}

	/*
	 * VOP_RMDIR now takes a new third argument (the current
	 * directory of the process).  That's because someone
	 * wants to return EINVAL if one tries to remove ".".
	 * Of course, NFS servers have no idea what their
	 * clients' current directories are.  We fake it by
	 * supplying a vnode known to exist and illegal to
	 * remove.
	 */
	TRACE_0(TR_FAC_NFS, TR_VOP_RMDIR_START,
		"vop_rmdir_start:");
	error = VOP_RMDIR(vp, da->da_name, rootdir, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_RMDIR_END,
		"vop_rmdir_end:");

	/*
	 * Force modified data and metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, 0, cr);

	VN_RELE(vp);

	/*
	 * System V defines rmdir to return EEXIST, not ENOTEMPTY,
	 * if the directory is not empty.  A System V NFS server
	 * needs to map NFSERR_EXIST to NFSERR_NOTEMPTY to transmit
	 * over the wire.
	 */
	if (error == EEXIST)
		*status = NFSERR_NOTEMPTY;
	else
		*status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_RMDIR_END,
		"rfs_rmdir_end:(%S)", "done");
}
fhandle_t *
rfs_rmdir_getfh(struct nfsdiropargs *da)
{
	return (da->da_fhandle);
}

/* ARGSUSED */
void
rfs_readdir(struct nfsrddirargs *rda, struct nfsrddirres *rd,
	struct exportinfo *exi, struct svc_req *req, cred_t *cr)
{
	int error;
	int iseof;
	struct iovec iov;
	struct uio uio;
	vnode_t *vp;

	TRACE_0(TR_FAC_NFS, TR_RFS_READDIR_START,
		"rfs_readdir_start:");

	vp = nfs_fhtovp(&rda->rda_fh, exi);
	if (vp == NULL) {
		rd->rd_entries = NULL;
		rd->rd_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_READDIR_END,
			"rfs_readdir_end:(%S)", "stale");
		return;
	}

	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		rd->rd_entries = NULL;
		rd->rd_status = NFSERR_NOTDIR;
		TRACE_1(TR_FAC_NFS, TR_RFS_READDIR_END,
			"rfs_readdir_end:(%S)", "notdir");
		return;
	}

	TRACE_1(TR_FAC_NFS, TR_VOP_RWLOCK_START,
		"vop_rwlock_start:(%K)", callee());
	VOP_RWLOCK(vp, 0);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWLOCK_END,
		"vop_rwlock_end:");

	TRACE_1(TR_FAC_NFS, TR_VOP_ACCESS_START,
		"vop_access_start:(%K)", callee());
	error = VOP_ACCESS(vp, VREAD, 0, cr);
	TRACE_0(TR_FAC_NFS, TR_VOP_ACCESS_END,
		"vop_access_end:");
	if (error) {
		rd->rd_entries = NULL;
		goto bad;
	}

	if (rda->rda_count == 0) {
		rd->rd_entries = NULL;
		rd->rd_size = 0;
		rd->rd_eof = FALSE;
		goto bad;
	}

	rda->rda_count = MIN(rda->rda_count, NFS_MAXDATA);

	/*
	 * Allocate data for entries.  This will be freed by rfs_rddirfree.
	 */
	rd->rd_bufsize = (uint_t)rda->rda_count;
	rd->rd_entries = kmem_alloc(rd->rd_bufsize, KM_SLEEP);

	/*
	 * Set up io vector to read directory data
	 */
	iov.iov_base = (caddr_t)rd->rd_entries;
	iov.iov_len = rda->rda_count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_loffset = (offset_t)rda->rda_offset;
	uio.uio_resid = rda->rda_count;

	/*
	 * read directory
	 */
	TRACE_0(TR_FAC_NFS, TR_VOP_READDIR_START,
		"vop_readdir_start:");
	error = VOP_READDIR(vp, &uio, cr, &iseof);
	TRACE_0(TR_FAC_NFS, TR_VOP_READDIR_END,
		"vop_readdir_end:");

	/*
	 * Clean up
	 */
	if (!error) {
		/*
		 * set size and eof
		 */
		if (uio.uio_resid == rda->rda_count) {
			rd->rd_size = 0;
			rd->rd_eof = TRUE;
		} else {
			rd->rd_size = (uint32_t)(rda->rda_count -
			    uio.uio_resid);
			rd->rd_eof = iseof ? TRUE : FALSE;
		}
	}

bad:
	TRACE_1(TR_FAC_NFS, TR_VOP_RWUNLOCK_START,
		"vop_rwunlock_start:(%K)", callee());
	VOP_RWUNLOCK(vp, 0);
	TRACE_0(TR_FAC_NFS, TR_VOP_RWUNLOCK_END,
		"vop_rwunlock_end:");

#if 0 /* notyet */
	/*
	 * Don't do this.  It causes local disk writes when just
	 * reading the file and the overhead is deemed larger
	 * than the benefit.
	 */
	/*
	 * Force modified metadata out to stable storage.
	 */
	(void) VOP_FSYNC(vp, FNODSYNC, cr);
#endif

	VN_RELE(vp);

	rd->rd_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_READDIR_END,
		"rfs_readdir_end:(%S)", "done");
}
fhandle_t *
rfs_readdir_getfh(struct nfsrddirargs *rda)
{
	return (&rda->rda_fh);
}
void
rfs_rddirfree(struct nfsrddirres *rd)
{
	if (rd->rd_entries != NULL)
		kmem_free(rd->rd_entries, rd->rd_bufsize);
}

/* ARGSUSED */
void
rfs_statfs(fhandle_t *fh, struct nfsstatfs *fs, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	struct statvfs64 sb;
	vnode_t *vp;

	TRACE_0(TR_FAC_NFS, TR_RFS_STATFS_START,
		"rfs_statfs_start:");

	vp = nfs_fhtovp(fh, exi);
	if (vp == NULL) {
		fs->fs_status = NFSERR_STALE;
		TRACE_1(TR_FAC_NFS, TR_RFS_STATFS_END,
			"rfs_statfs_end:(%S)", "stale");
		return;
	}

	error = VFS_STATVFS(vp->v_vfsp, &sb);

	if (!error) {
		fs->fs_tsize = nfstsize();
		fs->fs_bsize = sb.f_frsize;
		fs->fs_blocks = sb.f_blocks;
		fs->fs_bfree = sb.f_bfree;
		fs->fs_bavail = sb.f_bavail;
	}

	VN_RELE(vp);

	fs->fs_status = puterrno(error);

	TRACE_1(TR_FAC_NFS, TR_RFS_STATFS_END,
		"rfs_statfs_end:(%S)", "done");
}
fhandle_t *
rfs_statfs_getfh(fhandle_t *fh)
{
	return (fh);
}

static int
sattr_to_vattr(struct nfssattr *sa, struct vattr *vap)
{
	vap->va_mask = 0;

	/*
	 * There was a sign extension bug in some VFS based systems
	 * which stored the mode as a short.  When it would get
	 * assigned to a u_long, no sign extension would occur.
	 * It needed to, but this wasn't noticed because sa_mode
	 * would then get assigned back to the short, thus ignoring
	 * the upper 16 bits of sa_mode.
	 *
	 * To make this implementation work for both broken
	 * clients and good clients, we check for both versions
	 * of the mode.
	 */
	if (sa->sa_mode != (uint32_t)((ushort_t)-1) &&
	    sa->sa_mode != (uint32_t)-1) {
		vap->va_mask |= AT_MODE;
		vap->va_mode = sa->sa_mode;
	}
	if (sa->sa_uid != (uint32_t)-1) {
		vap->va_mask |= AT_UID;
		vap->va_uid = sa->sa_uid;
	}
	if (sa->sa_gid != (uint32_t)-1) {
		vap->va_mask |= AT_GID;
		vap->va_gid = sa->sa_gid;
	}
	if (sa->sa_size != (uint32_t)-1) {
		vap->va_mask |= AT_SIZE;
		vap->va_size = sa->sa_size;
	}
	if (sa->sa_atime.tv_sec != (int32_t)-1 &&
	    sa->sa_atime.tv_usec != (int32_t)-1) {
		/* return error if time overflow */
		IF_NOT_NFS_TIME_OK(NFS2_TIME_OK(sa->sa_atime.tv_sec),
			return (EOVERFLOW));
		vap->va_mask |= AT_ATIME;
		/*
		 * nfs protocol defines times as unsigned so don't extend sign,
		 * unless sysadmin set nfs_allow_preepoch_time.
		 */
		NFS_TIME_T_CONVERT(vap->va_atime.tv_sec, sa->sa_atime.tv_sec);
		vap->va_atime.tv_nsec = (uint32_t)(sa->sa_atime.tv_usec * 1000);
	}
	if (sa->sa_mtime.tv_sec != (int32_t)-1 &&
	    sa->sa_mtime.tv_usec != (int32_t)-1) {
		/* return error if time overflow */
		IF_NOT_NFS_TIME_OK(NFS2_TIME_OK(sa->sa_mtime.tv_sec),
			return (EOVERFLOW));
		vap->va_mask |= AT_MTIME;
		/*
		 * nfs protocol defines times as unsigned so don't extend sign,
		 * unless sysadmin set nfs_allow_preepoch_time.
		 */
		NFS_TIME_T_CONVERT(vap->va_mtime.tv_sec, sa->sa_mtime.tv_sec);
		vap->va_mtime.tv_nsec = (uint32_t)(sa->sa_mtime.tv_usec * 1000);
	}
	return (0);
}

static enum nfsftype vt_to_nf[] = {
	0, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, 0, 0, 0, NFSOC, 0
};

/*
 * check the following fields for overflow: nodeid,
 * size, time. This could be a problem when converting 64-bit LP64 fields
 * into 32-bit ones.
 * Return an error if there is an overflow.
 */
int
vattr_to_nattr(struct vattr *vap, struct nfsfattr *na)
{
	ASSERT(vap->va_type >= VNON && vap->va_type <= VBAD);
	na->na_type = vt_to_nf[vap->va_type];

	if (vap->va_mode == (unsigned short) -1)
		na->na_mode = (uint32_t)-1;
	else
		na->na_mode = VTTOIF(vap->va_type) | vap->va_mode;

	if (vap->va_uid == (unsigned short)(-1))
		na->na_uid = (uint32_t)(-1);
	else if (vap->va_uid == UID_NOBODY)
		na->na_uid = (uint32_t)NFS_UID_NOBODY;
	else
		na->na_uid = vap->va_uid;

	if (vap->va_gid == (unsigned short)(-1))
		na->na_gid = (uint32_t)-1;
	else if (vap->va_gid == GID_NOBODY)
		na->na_gid = (uint32_t)NFS_GID_NOBODY;
	else
		na->na_gid = vap->va_gid;

	/*
	 * do we need to check fsid for overflow?
	 * It is 64-bit in the vattr, but are bigger values supported?
	 *
	 * Check for big files here, instead of at the caller.
	 */
	if (BIG(vap)) {
		return (EFBIG);
	}
	na->na_fsid = vap->va_fsid;
	na->na_nodeid = vap->va_nodeid;
	na->na_nlink = vap->va_nlink;
	na->na_size = vap->va_size;

	/*
	 * If the vnode times overflow the 32-bit times that NFS2
	 * uses on the wire then return an error.
	 */
	if (!NFS_VAP_TIME_OK(vap)) {
		return (EOVERFLOW);
	}
	na->na_atime.tv_sec = vap->va_atime.tv_sec;
	na->na_atime.tv_usec = vap->va_atime.tv_nsec / 1000;

	na->na_mtime.tv_sec = vap->va_mtime.tv_sec;
	na->na_mtime.tv_usec = vap->va_mtime.tv_nsec / 1000;

	na->na_ctime.tv_sec = vap->va_ctime.tv_sec;
	na->na_ctime.tv_usec = vap->va_ctime.tv_nsec / 1000;

	/*
	 * If the dev_t will fit into 16 bits then compress
	 * it, otherwise leave it alone. See comments in
	 * nfs_client.c.
	 */
	if (getminor(vap->va_rdev) <= SO4_MAXMIN &&
	    getmajor(vap->va_rdev) <= SO4_MAXMAJ)
		na->na_rdev = nfsv2_cmpdev(vap->va_rdev);
	else
		(void) cmpldev(&na->na_rdev, vap->va_rdev);

	na->na_blocks = vap->va_nblocks;
	na->na_blocksize = vap->va_blksize;

	/*
	 * This bit of ugliness is a *TEMPORARY* hack to preserve the
	 * over-the-wire protocols for named-pipe vnodes.  It remaps the
	 * VFIFO type to the special over-the-wire type. (see note in nfs.h)
	 *
	 * BUYER BEWARE:
	 *  If you are porting the NFS to a non-Sun server, you probably
	 *  don't want to include the following block of code.  The
	 *  over-the-wire special file types will be changing with the
	 *  NFS Protocol Revision.
	 */
	if (vap->va_type == VFIFO)
		NA_SETFIFO(na);
	return (0);
}

/*
 * acl v2 support: returns approximate permission.
 *	default: returns minimal permission (more restrictive)
 *	aclok: returns maximal permission (less restrictive)
 *	This routine changes the permissions that are alaredy in *va.
 *	If a file has minimal ACL, i.e. aclcnt == MIN_ACL_ENTRIES,
 *	CLASS_OBJ is always the same as GROUP_OBJ entry.
 */
static void
acl_perm(struct vnode *vp, struct exportinfo *exi, struct vattr *va, cred_t *cr)
{
	vsecattr_t	vsa;
	int		aclcnt;
	aclent_t	*aclentp;
	mode_t		mask_perm;
	mode_t		grp_perm;
	mode_t		other_perm;
	mode_t		other_orig;
	int		error;

	/* dont care default acl */
	vsa.vsa_mask = (VSA_ACL | VSA_ACLCNT);
	error = VOP_GETSECATTR(vp, &vsa, 0, cr);

	if (!error) {
		aclcnt = vsa.vsa_aclcnt;
		if (aclcnt > MIN_ACL_ENTRIES) {
			/* non-trivial ACL */
			aclentp = vsa.vsa_aclentp;
			if (exi->exi_export.ex_flags & EX_ACLOK) {
				/* maximal permissions */
				grp_perm = 0;
				other_perm = 0;
				for (; aclcnt > 0; aclcnt--, aclentp++) {
					switch (aclentp->a_type) {
					case USER_OBJ:
						break;
					case USER:
						grp_perm |=
						    aclentp->a_perm << 3;
						other_perm |= aclentp->a_perm;
						break;
					case GROUP_OBJ:
						grp_perm |=
						    aclentp->a_perm << 3;
						break;
					case GROUP:
						other_perm |= aclentp->a_perm;
						break;
					case OTHER_OBJ:
						other_orig = aclentp->a_perm;
						break;
					case CLASS_OBJ:
						mask_perm = aclentp->a_perm;
						break;
					default:
						break;
					}
				}
				grp_perm &= mask_perm << 3;
				other_perm &= mask_perm;
				other_perm |= other_orig;

			} else {
				/* minimal permissions */
				grp_perm = 070;
				other_perm = 07;
				for (; aclcnt > 0; aclcnt--, aclentp++) {
					switch (aclentp->a_type) {
					case USER_OBJ:
						break;
					case USER:
					case CLASS_OBJ:
						grp_perm &=
						    aclentp->a_perm << 3;
						other_perm &=
						    aclentp->a_perm;
						break;
					case GROUP_OBJ:
						grp_perm &=
						    aclentp->a_perm << 3;
						break;
					case GROUP:
						other_perm &=
						    aclentp->a_perm;
						break;
					case OTHER_OBJ:
						other_perm &=
						    aclentp->a_perm;
						break;
					default:
						break;
					}
				}
			}
			/* copy to va */
			va->va_mode &= ~077;
			va->va_mode |= grp_perm | other_perm;
		}
		if (vsa.vsa_aclcnt)
			kmem_free(vsa.vsa_aclentp,
			    vsa.vsa_aclcnt * sizeof (aclent_t));
	}
}

void
rfs_srvrinit(void)
{
	mutex_init(&rfs_async_write_lock, NULL, MUTEX_DEFAULT, NULL);
}

void
rfs_srvrfini(void)
{
	mutex_destroy(&rfs_async_write_lock);
}
