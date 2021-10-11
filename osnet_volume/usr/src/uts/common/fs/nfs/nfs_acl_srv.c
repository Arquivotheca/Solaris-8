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
 *	(c) 1986-1992, 1994, 1997-1998  Sun Microsystems, Inc.
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)nfs_acl_srv.c	1.18	98/07/23 SMI"
/* SVr4.0 1.21 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/siginfo.h>
#include <sys/tiuser.h>
#include <sys/statvfs.h>
#include <sys/t_kuser.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/acl.h>
#include <sys/dirent.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/unistd.h>
#include <sys/vtrace.h>
#include <sys/mode.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/svc.h>
#include <rpc/xdr.h>

#include <nfs/nfs.h>
#include <nfs/export.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_acl.h>

/*
 * These are the interface routines for the server side of the
 * NFS ACL server.  See the NFS ACL protocol specification
 * for a description of this interface.
 */

/* ARGSUSED */
void
acl2_getacl(GETACL2args *args, GETACL2res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t va;

	vp = nfs_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFSERR_STALE;
		return;
	}

	if (vp->v_op->vop_getsecattr == NULL) {
		VN_RELE(vp);
		resp->status = NFSERR_OPNOTSUPP;
		return;
	}

	bzero((caddr_t)&resp->resok.acl, sizeof (resp->resok.acl));

	resp->resok.acl.vsa_mask = args->mask;
	error = VOP_GETSECATTR(vp, &resp->resok.acl, 0, cr);
	if (error) {
		VN_RELE(vp);
		resp->status = puterrno(error);
		return;
	}

	va.va_mask = AT_ALL;
	error = VOP_GETATTR(vp, &va, 0, cr);

	VN_RELE(vp);

	/* check for overflowed values */
	if (!error) {
		error = vattr_to_nattr(&va, &resp->resok.attr);
	}
	if (error) {
		resp->status = puterrno(error);
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
		return;
	}

	resp->status = NFS_OK;
	if (!(args->mask & NA_ACL)) {
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		resp->resok.acl.vsa_aclentp = NULL;
	}
	if (!(args->mask & NA_DFACL)) {
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
		resp->resok.acl.vsa_dfaclentp = NULL;
	}
}
fhandle_t *
acl2_getacl_getfh(GETACL2args *args)
{

	return (&args->fh);
}
void
acl2_getacl_free(GETACL2res *resp)
{

	if (resp->status == NFS_OK) {
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
	}
}

/* ARGSUSED */
void
acl2_setacl(SETACL2args *args, SETACL2res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t va;

	vp = nfs_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFSERR_STALE;
		return;
	}

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		resp->status = NFSERR_ROFS;
		return;
	}

	if (vp->v_op->vop_setsecattr == NULL) {
		VN_RELE(vp);
		resp->status = NFSERR_OPNOTSUPP;
		return;
	}

	VOP_RWLOCK(vp, 1);
	error = VOP_SETSECATTR(vp, &args->acl, 0, cr);
	if (error) {
		VOP_RWUNLOCK(vp, 1);
		VN_RELE(vp);
		resp->status = puterrno(error);
		return;
	}

	va.va_mask = AT_ALL;
	error = VOP_GETATTR(vp, &va, 0, cr);

	VOP_RWUNLOCK(vp, 1);
	VN_RELE(vp);

	/* check for overflowed values */
	if (!error) {
		error = vattr_to_nattr(&va, &resp->resok.attr);
	}
	if (error) {
		resp->status = puterrno(error);
		return;
	}

	resp->status = NFS_OK;
}
fhandle_t *
acl2_setacl_getfh(SETACL2args *args)
{

	return (&args->fh);
}

/* ARGSUSED */
void
acl2_getattr(GETATTR2args *args, GETATTR2res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t va;

	vp = nfs_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFSERR_STALE;
		return;
	}

	va.va_mask = AT_ALL;
	error = VOP_GETATTR(vp, &va, 0, cr);

	VN_RELE(vp);

	/* check for overflowed values */
	if (!error) {
		error = vattr_to_nattr(&va, &resp->resok.attr);
	}
	if (error) {
		resp->status = puterrno(error);
		return;
	}

	resp->status = NFS_OK;
}

fhandle_t *
acl2_getattr_getfh(GETATTR2args *args)
{

	return (&args->fh);
}

/* ARGSUSED */
void
acl2_access(ACCESS2args *args, ACCESS2res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t va;
	int checkwriteperm;

	vp = nfs_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFSERR_STALE;
		return;
	}

	/*
	 * If the file system is exported read only, it is not appropriate
	 * to check write permissions for regular files and directories.
	 * Special files are interpreted by the client, so the underlying
	 * permissions are sent back to the client for interpretation.
	 */
	if (rdonly(exi, req) && (vp->v_type == VREG || vp->v_type == VDIR))
		checkwriteperm = 0;
	else
		checkwriteperm = 1;

	/*
	 * We need the mode so that we can correctly determine access
	 * permissions relative to a mandatory lock file.  Access to
	 * mandatory lock files is denied on the server, so it might
	 * as well be reflected to the server during the open.
	 */
	va.va_mask = AT_MODE;
	error = VOP_GETATTR(vp, &va, 0, cr);
	if (error) {
		VN_RELE(vp);
		resp->status = puterrno(error);
		return;
	}

	resp->resok.access = 0;

	if (args->access & ACCESS2_READ) {
		error = VOP_ACCESS(vp, VREAD, 0, cr);
		if (!error && !MANDLOCK(vp, va.va_mode))
			resp->resok.access |= ACCESS2_READ;
	}
	if ((args->access & ACCESS2_LOOKUP) && vp->v_type == VDIR) {
		error = VOP_ACCESS(vp, VEXEC, 0, cr);
		if (!error)
			resp->resok.access |= ACCESS2_LOOKUP;
	}
	if (checkwriteperm &&
	    (args->access & (ACCESS2_MODIFY|ACCESS2_EXTEND))) {
		error = VOP_ACCESS(vp, VWRITE, 0, cr);
		if (!error && !MANDLOCK(vp, va.va_mode))
			resp->resok.access |=
			    (args->access & (ACCESS2_MODIFY|ACCESS2_EXTEND));
	}
	if (checkwriteperm &&
	    (args->access & ACCESS2_DELETE) && (vp->v_type == VDIR)) {
		error = VOP_ACCESS(vp, VWRITE, 0, cr);
		if (!error)
			resp->resok.access |= ACCESS2_DELETE;
	}
	if (args->access & ACCESS2_EXECUTE) {
		error = VOP_ACCESS(vp, VEXEC, 0, cr);
		if (!error && !MANDLOCK(vp, va.va_mode))
			resp->resok.access |= ACCESS2_EXECUTE;
	}

	va.va_mask = AT_ALL;
	error = VOP_GETATTR(vp, &va, 0, cr);

	VN_RELE(vp);

	/* check for overflowed values */
	if (!error) {
		error = vattr_to_nattr(&va, &resp->resok.attr);
	}
	if (error) {
		resp->status = puterrno(error);
		return;
	}

	resp->status = NFS_OK;
}
fhandle_t *
acl2_access_getfh(ACCESS2args *args)
{

	return (&args->fh);
}

/* ARGSUSED */
void
acl3_getacl(GETACL3args *args, GETACL3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t *vap;
	vattr_t va;

	vp = nfs3_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		vattr_to_post_op_attr(NULL, &resp->resfail.attr);
		return;
	}

#ifdef DEBUG
	if (rfs3_do_post_op_attr) {
		va.va_mask = AT_ALL;
		vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
	} else
		vap = NULL;
#else
	va.va_mask = AT_ALL;
	vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
#endif

	if (vp->v_op->vop_getsecattr == NULL) {
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTSUPP;
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

	bzero((caddr_t)&resp->resok.acl, sizeof (resp->resok.acl));

	resp->resok.acl.vsa_mask = args->mask;
	error = VOP_GETSECATTR(vp, &resp->resok.acl, 0, cr);
	if (error) {
		VN_RELE(vp);
		resp->status = puterrno3(error);
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

#ifdef DEBUG
	if (rfs3_do_post_op_attr) {
		va.va_mask = AT_ALL;
		vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
	} else
		vap = NULL;
#else
	va.va_mask = AT_ALL;
	vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
#endif

	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

	resp->status = NFS3_OK;
	vattr_to_post_op_attr(vap, &resp->resok.attr);
	if (!(args->mask & NA_ACL)) {
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		resp->resok.acl.vsa_aclentp = NULL;
	}
	if (!(args->mask & NA_DFACL)) {
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
		resp->resok.acl.vsa_dfaclentp = NULL;
	}
}
fhandle_t *
acl3_getacl_getfh(GETACL3args *args)
{

	return ((fhandle_t *)&args->fh.fh3_u.nfs_fh3_i.fh3_i);
}
void
acl3_getacl_free(GETACL3res *resp)
{

	if (resp->status == NFS3_OK) {
		if (resp->resok.acl.vsa_aclcnt > 0 &&
		    resp->resok.acl.vsa_aclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_aclentp,
			    resp->resok.acl.vsa_aclcnt * sizeof (aclent_t));
		}
		if (resp->resok.acl.vsa_dfaclcnt > 0 &&
		    resp->resok.acl.vsa_dfaclentp != NULL) {
			kmem_free((caddr_t)resp->resok.acl.vsa_dfaclentp,
			    resp->resok.acl.vsa_dfaclcnt * sizeof (aclent_t));
		}
	}
}

/* ARGSUSED */
void
acl3_setacl(SETACL3args *args, SETACL3res *resp, struct exportinfo *exi,
	struct svc_req *req, cred_t *cr)
{
	int error;
	vnode_t *vp;
	vattr_t *vap;
	vattr_t va;

	vp = nfs3_fhtovp(&args->fh, exi);
	if (vp == NULL) {
		resp->status = NFS3ERR_STALE;
		vattr_to_post_op_attr(NULL, &resp->resfail.attr);
		return;
	}

#ifdef DEBUG
	if (rfs3_do_post_op_attr) {
		va.va_mask = AT_ALL;
		vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
	} else
		vap = NULL;
#else
	va.va_mask = AT_ALL;
	vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
#endif

	if (rdonly(exi, req) || (vp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		VN_RELE(vp);
		resp->status = NFS3ERR_ROFS;
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

	if (vp->v_op->vop_getsecattr == NULL) {
		VN_RELE(vp);
		resp->status = NFS3ERR_NOTSUPP;
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

	VOP_RWLOCK(vp, 1);
	error = VOP_SETSECATTR(vp, &args->acl, 0, cr);
	if (error) {
		VOP_RWUNLOCK(vp, 1);
		VN_RELE(vp);
		resp->status = puterrno3(error);
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

#ifdef DEBUG
	if (rfs3_do_post_op_attr) {
		va.va_mask = AT_ALL;
		vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
	} else
		vap = NULL;
#else
	va.va_mask = AT_ALL;
	vap = VOP_GETATTR(vp, &va, 0, cr) ? NULL : &va;
#endif

	VOP_RWUNLOCK(vp, 1);
	VN_RELE(vp);

	if (error) {
		resp->status = puterrno3(error);
		vattr_to_post_op_attr(vap, &resp->resfail.attr);
		return;
	}

	resp->status = NFS3_OK;
	vattr_to_post_op_attr(vap, &resp->resok.attr);
}
fhandle_t *
acl3_setacl_getfh(SETACL3args *args)
{

	return ((fhandle_t *)&args->fh.fh3_u.nfs_fh3_i.fh3_i);
}
