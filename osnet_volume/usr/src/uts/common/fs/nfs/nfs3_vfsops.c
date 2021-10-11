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
 *	Copyright (c) 1986-1991,1994-1999, by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident	"@(#)nfs3_vfsops.c	1.105	99/08/25 SMI"
/* SVr4.0 1.16 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/mkdev.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/bootconf.h>
#include <sys/modctl.h>
#include <sys/acl.h>
#include <sys/flock.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/mount.h>
#include <nfs/nfs_acl.h>

#include <fs/fs_subr.h>

/*
 * From rpcsec module (common/rpcsec).
 */
extern int sec_clnt_loadinfo(struct sec_data *, struct sec_data **, model_t);
extern void sec_clnt_freeinfo(struct sec_data *);

static kstat_named_t rfsreqcnt_v3[] = {
	{ "null",	KSTAT_DATA_UINT64 },
	{ "getattr",	KSTAT_DATA_UINT64 },
	{ "setattr",	KSTAT_DATA_UINT64 },
	{ "lookup",	KSTAT_DATA_UINT64 },
	{ "access",	KSTAT_DATA_UINT64 },
	{ "readlink",	KSTAT_DATA_UINT64 },
	{ "read",	KSTAT_DATA_UINT64 },
	{ "write",	KSTAT_DATA_UINT64 },
	{ "create",	KSTAT_DATA_UINT64 },
	{ "mkdir",	KSTAT_DATA_UINT64 },
	{ "symlink",	KSTAT_DATA_UINT64 },
	{ "mknod",	KSTAT_DATA_UINT64 },
	{ "remove",	KSTAT_DATA_UINT64 },
	{ "rmdir",	KSTAT_DATA_UINT64 },
	{ "rename",	KSTAT_DATA_UINT64 },
	{ "link",	KSTAT_DATA_UINT64 },
	{ "readdir",	KSTAT_DATA_UINT64 },
	{ "readdirplus", KSTAT_DATA_UINT64 },
	{ "fsstat",	KSTAT_DATA_UINT64 },
	{ "fsinfo",	KSTAT_DATA_UINT64 },
	{ "pathconf",	KSTAT_DATA_UINT64 },
	{ "commit",	KSTAT_DATA_UINT64 }
};
static kstat_named_t *rfsreqcnt_v3_ptr = rfsreqcnt_v3;
static uint_t rfsreqcnt_v3_ndata = sizeof (rfsreqcnt_v3) /
    sizeof (kstat_named_t);

static char *rfsnames_v3[] = {
	"null", "getattr", "setattr", "lookup", "access", "readlink", "read",
	"write", "create", "mkdir", "symlink", "mknod", "remove", "rmdir",
	"rename", "link", "readdir", "readdirplus", "fsstat", "fsinfo",
	"pathconf", "commit"
};

/*
 * This table maps from NFS protocol number into call type.
 * Zero means a "Lookup" type call
 * One  means a "Read" type call
 * Two  means a "Write" type call
 * This is used to select a default time-out.
 */
static char call_type_v3[] = {
	0, 0, 1, 0, 0, 0, 1,
	2, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 2, 0, 0, 0,
	2 };

/*
 * Similar table, but to determine which timer to use
 * (only real reads and writes!)
 */
static char timer_type_v3[] = {
	0, 0, 0, 0, 0, 0, 1,
	2, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 0, 0, 0,
	0 };

/*
 * This table maps from NFS protocol number into a call type
 * for the semisoft mount option.
 * Zero means do not repeat operation.
 * One  means repeat.
 */
static char ss_call_type_v3[] = {
	0, 0, 1, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1,
	1, 1, 0, 0, 0, 0, 0,
	1 };

/*
 * nfs3 vfs operations.
 */
static int	nfs3_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static int	nfs3_unmount(vfs_t *, int, cred_t *);
static int	nfs3_root(vfs_t *, vnode_t **);
static int	nfs3_statvfs(vfs_t *, struct statvfs64 *);
static int	nfs3_sync(vfs_t *, short, cred_t *);
static int	nfs3_vget(vfs_t *, vnode_t **, fid_t *);
static int	nfs3_mountroot(vfs_t *, whymountroot_t);
static void	nfs3_freevfs(vfs_t *);

static int	nfs3rootvp(vnode_t **, vfs_t *, struct servinfo *,
		    int, cred_t *);

struct vfsops nfs3_vfsops = {
	nfs3_mount,
	nfs3_unmount,
	nfs3_root,
	nfs3_statvfs,
	nfs3_sync,
	nfs3_vget,
	nfs3_mountroot,
	fs_nosys,
	nfs3_freevfs
};

vnode_t nfs3_notfound;

/*
 * Initialize the vfs structure
 */

static int nfs3fstyp;

int
nfs3init(struct vfssw *vswp, int fstyp)
{
	vnode_t *vp;
	kstat_t *rfsproccnt_v3_kstat;
	kstat_t *rfsreqcnt_v3_kstat;
	kstat_t *aclproccnt_v3_kstat;
	kstat_t *aclreqcnt_v3_kstat;

	vswp->vsw_vfsops = &nfs3_vfsops;
	nfs3fstyp = fstyp;

	mutex_enter(&nfs_kstat_lock);
	if (nfs_client_kstat == NULL) {
		if ((nfs_client_kstat = kstat_create("nfs", 0, "nfs_client",
		    "misc", KSTAT_TYPE_NAMED, clstat_ndata,
		    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
			nfs_client_kstat->ks_data = (void *)clstat_ptr;
			kstat_install(nfs_client_kstat);
		}
	}

	if (nfs_server_kstat == NULL) {
		if ((nfs_server_kstat = kstat_create("nfs", 0, "nfs_server",
		    "misc", KSTAT_TYPE_NAMED, svstat_ndata,
		    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
			nfs_server_kstat->ks_data = (void *)svstat_ptr;
			kstat_install(nfs_server_kstat);
		}
	}
	mutex_exit(&nfs_kstat_lock);

	if ((rfsproccnt_v3_kstat = kstat_create("nfs", 0, "rfsproccnt_v3",
	    "misc", KSTAT_TYPE_NAMED, rfsproccnt_v3_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsproccnt_v3_kstat->ks_data = (void *)rfsproccnt_v3_ptr;
		kstat_install(rfsproccnt_v3_kstat);
	}
	if ((rfsreqcnt_v3_kstat = kstat_create("nfs", 0, "rfsreqcnt_v3",
	    "misc", KSTAT_TYPE_NAMED, rfsreqcnt_v3_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsreqcnt_v3_kstat->ks_data = (void *)rfsreqcnt_v3_ptr;
		kstat_install(rfsreqcnt_v3_kstat);
	}
	if ((aclproccnt_v3_kstat = kstat_create("nfs_acl", 0, "aclproccnt_v3",
	    "misc", KSTAT_TYPE_NAMED, aclproccnt_v3_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclproccnt_v3_kstat->ks_data = (void *)aclproccnt_v3_ptr;
		kstat_install(aclproccnt_v3_kstat);
	}
	if ((aclreqcnt_v3_kstat = kstat_create("nfs_acl", 0, "aclreqcnt_v3",
	    "misc", KSTAT_TYPE_NAMED, aclreqcnt_v3_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclreqcnt_v3_kstat->ks_data = (void *)aclreqcnt_v3_ptr;
		kstat_install(aclreqcnt_v3_kstat);
	}

	vp = &nfs3_notfound;
	bzero(vp, sizeof (*vp));
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	vp->v_count = 1;
	vp->v_op = &nfs3_vnodeops;

	return (0);
}

void
nfs3fini(void)
{
	kstat_t *ksp;
	vnode_t *vp;

	if (nfs_client_kstat) {
		kstat_delete(nfs_client_kstat);
		nfs_client_kstat = NULL;
	}

	if (nfs_server_kstat) {
		kstat_delete(nfs_server_kstat);
		nfs_server_kstat = NULL;
	}

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs", 0, "rfsproccnt_v3");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs", 0, "rfsreqcnt_v3");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs_acl", 0, "aclproccnt_v3");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs_acl", 0, "aclreqcnt_v3");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	vp = &nfs3_notfound;
	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
}

/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
static int
nfs3_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	char *data = uap->dataptr;
	int error;
	vnode_t *rtvp;			/* the server's root */
	mntinfo_t *mi;			/* mount info, pointed at by vfs */
	size_t hlen;			/* length of hostname */
	size_t nlen;			/* length of netname */
	char netname[SYS_NMLN];		/* server's netname */
	struct netbuf addr;		/* server's address */
	struct netbuf syncaddr;		/* AUTH_DES time sync addr */
	struct knetconfig *knconf;	/* transport knetconfig structure */
	rnode_t *rp;
	struct servinfo *svp;		/* nfs server info */
	struct servinfo *svp_prev = NULL; /* previous nfs server info */
	struct servinfo *svp_head;	/* first nfs server info */
	struct sec_data *secdata;	/* security data */
	STRUCT_DECL(nfs_args, args);	/* nfs mount arguments */
	STRUCT_DECL(knetconfig, knconf_tmp);
	STRUCT_DECL(netbuf, addr_tmp);
	int flags;
	char *p, *pf;

	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * get arguments
	 *
	 * nfs_args is now versioned and is extensible, so
	 * uap->datalen might be different from sizeof (args)
	 * in a compatible situation.
	 */
more:
	STRUCT_INIT(args, get_udatamodel());
	bzero(STRUCT_BUF(args), SIZEOF_STRUCT(nfs_args, DATAMODEL_NATIVE));
	if (copyin(data, STRUCT_BUF(args), MIN(uap->datalen,
	    STRUCT_SIZE(args))))
		return (EFAULT);

	flags = STRUCT_FGET(args, flags);

	/*
	 * If the request changes the locking type, disallow the remount,
	 * because it's questionable whether we can transfer the
	 * locking state correctly.
	 */
	if (uap->flags & MS_REMOUNT) {
		if ((mi = VFTOMI(vfsp)) != NULL) {
			u_int new_mi_llock;
			u_int old_mi_llock;

			new_mi_llock = (flags & NFSMNT_LLOCK) ? 1 : 0;
			old_mi_llock = (mi->mi_flags & MI_LLOCK) ? 1 : 0;
			if (old_mi_llock != new_mi_llock)
				return (EBUSY);
		}
		return (0);
	}

	mutex_enter(&mvp->v_lock);
	if (!(uap->flags & MS_OVERLAY) &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/* make sure things are zeroed for errout: */
	rtvp = NULL;
	mi = NULL;
	addr.buf = NULL;
	syncaddr.buf = NULL;
	secdata = NULL;

	/*
	 * A valid knetconfig structure is required.
	 */
	if (!(flags & NFSMNT_KNCONF))
		return (EINVAL);

	/*
	 * Allocate a servinfo struct.
	 */
	svp = kmem_zalloc(sizeof (*svp), KM_SLEEP);
	if (svp_prev)
		svp_prev->sv_next = svp;
	else
		svp_head = svp;
	svp_prev = svp;

	/*
	 * Allocate space for a knetconfig structure and
	 * its strings and copy in from user-land.
	 */
	knconf = kmem_zalloc(sizeof (*knconf), KM_SLEEP);
	svp->sv_knconf = knconf;
	STRUCT_INIT(knconf_tmp, get_udatamodel());
	if (copyin(STRUCT_FGETP(args, knconf), STRUCT_BUF(knconf_tmp),
	    STRUCT_SIZE(knconf_tmp))) {
		sv_free(svp_head);
		return (EFAULT);
	}

	knconf->knc_semantics = STRUCT_FGET(knconf_tmp, knc_semantics);
	knconf->knc_protofmly = STRUCT_FGETP(knconf_tmp, knc_protofmly);
	knconf->knc_proto = STRUCT_FGETP(knconf_tmp, knc_proto);
	if (get_udatamodel() != DATAMODEL_LP64) {
		knconf->knc_rdev = expldev(STRUCT_FGET(knconf_tmp, knc_rdev));
	} else {
		knconf->knc_rdev = STRUCT_FGET(knconf_tmp, knc_rdev);
	}

	pf = kmem_alloc(KNC_STRSIZE, KM_SLEEP);
	p = kmem_alloc(KNC_STRSIZE, KM_SLEEP);
	error = copyinstr(knconf->knc_protofmly, pf, KNC_STRSIZE, NULL);
	if (error) {
		kmem_free(pf, KNC_STRSIZE);
		kmem_free(p, KNC_STRSIZE);
		sv_free(svp_head);
		return (error);
	}
	error = copyinstr(knconf->knc_proto, p, KNC_STRSIZE, NULL);
	if (error) {
		kmem_free(pf, KNC_STRSIZE);
		kmem_free(p, KNC_STRSIZE);
		sv_free(svp_head);
		return (error);
	}
	knconf->knc_protofmly = pf;
	knconf->knc_proto = p;

	/*
	 * Get server address
	 */
	STRUCT_INIT(addr_tmp, get_udatamodel());
	if (copyin(STRUCT_FGETP(args, addr), STRUCT_BUF(addr_tmp),
	    STRUCT_SIZE(addr_tmp))) {
		addr.buf = NULL;
		error = EFAULT;
	} else {
		char *userbufptr;

		userbufptr = addr.buf = STRUCT_FGETP(addr_tmp, buf);
		addr.len = STRUCT_FGET(addr_tmp, len);
		addr.buf = kmem_alloc(addr.len, KM_SLEEP);
		addr.maxlen = addr.len;
		if (copyin(userbufptr, addr.buf, addr.len))
			error = EFAULT;
	}
	svp->sv_addr = addr;
	if (error)
		goto errout;

	/*
	 * Get the root fhandle
	 */
	if (copyin(STRUCT_FGETP(args, fh), &svp->sv_fhandle,
	    sizeof (svp->sv_fhandle))) {
		error = EFAULT;
		goto errout;
	}

	/*
	 * Check the root fhandle length
	 */
	if (svp->sv_fhandle.fh_len > NFS3_FHSIZE) {
		error = EINVAL;
#ifdef DEBUG
		cmn_err(CE_WARN, "nfs3_mount: got an invalid fhandle. "
		    "fh_len = %d", svp->sv_fhandle.fh_len);
		svp->sv_fhandle.fh_len = NFS_FHANDLE_LEN;
		nfs_printfhandle(&svp->sv_fhandle);
#endif
		goto errout;
	}

	/*
	 * Get server's hostname
	 */
	if (flags & NFSMNT_HOSTNAME) {
		error = copyinstr(STRUCT_FGETP(args, hostname),
		    netname, sizeof (netname), &hlen);
		if (error)
			goto errout;
	} else {
		char *p = "unknown-host";
		hlen = strlen(p) + 1;
		(void) strcpy(netname, p);
	}
	svp->sv_hostnamelen = hlen;
	svp->sv_hostname = kmem_alloc(svp->sv_hostnamelen, KM_SLEEP);
	(void) strcpy(svp->sv_hostname, netname);

	/*
	 * Get the extention data which has the new security data structure.
	 */
	if (flags & NFSMNT_NEWARGS) {
		switch (STRUCT_FGET(args, nfs_args_ext)) {
		case NFS_ARGS_EXTA:
		case NFS_ARGS_EXTB:
			/*
			 * Indicating the application is using the new
			 * sec_data structure to pass in the security
			 * data.
			 */
			if (STRUCT_FGETP(args,
			    nfs_ext_u.nfs_extA.secdata) == NULL) {
				error = EINVAL;
			} else {
				error = sec_clnt_loadinfo(
				    (struct sec_data *)STRUCT_FGETP(args,
					nfs_ext_u.nfs_extA.secdata),
				    &secdata, get_udatamodel());
			}
			break;

		default:
			error = EINVAL;
			break;
		}
	} else if (flags & NFSMNT_SECURE) {
		/*
		 * Keep this for backward compatibility to support
		 * NFSMNT_SECURE/NFSMNT_RPCTIMESYNC flags.
		 */
		if (STRUCT_FGETP(args, syncaddr) == NULL) {
			error = EINVAL;
		} else {
			/*
			 * get time sync address.
			 */
			if (copyin(STRUCT_FGETP(args, syncaddr), &addr_tmp,
			    STRUCT_SIZE(addr_tmp))) {
				syncaddr.buf = NULL;
				error = EFAULT;
			} else {
				char *userbufptr;

				userbufptr = syncaddr.buf =
				    STRUCT_FGETP(addr_tmp, buf);
				syncaddr.len =
				    STRUCT_FGET(addr_tmp, len);
				syncaddr.buf = kmem_alloc(syncaddr.len,
				    KM_SLEEP);
				syncaddr.maxlen = syncaddr.len;

				if (copyin(userbufptr, syncaddr.buf,
				    syncaddr.len))
					error = EFAULT;
			}

			/*
			 * get server's netname
			 */
			if (!error) {
				error = copyinstr(STRUCT_FGETP(args, netname),
				    netname, sizeof (netname), &nlen);
				netname[nlen] = '\0';
			}

			if (error && syncaddr.buf != NULL) {
				kmem_free(syncaddr.buf, syncaddr.len);
				syncaddr.buf = NULL;
			}
		}

		/*
		 * Move security related data to the sec_data structure.
		 */
		if (!error) {
			dh_k4_clntdata_t *data;
			char *pf, *p;

			secdata = kmem_alloc(sizeof (*secdata), KM_SLEEP);
			if (flags & NFSMNT_RPCTIMESYNC)
				secdata->flags |= AUTH_F_RPCTIMESYNC;
			data = kmem_alloc(sizeof (*data), KM_SLEEP);
			data->syncaddr = syncaddr;

			/*
			 * duplicate the knconf information for the
			 * new opaque data.
			 */
			data->knconf = kmem_alloc(sizeof (*knconf), KM_SLEEP);
			*data->knconf = *knconf;
			pf = kmem_alloc(KNC_STRSIZE, KM_SLEEP);
			p = kmem_alloc(KNC_STRSIZE, KM_SLEEP);
			bcopy(knconf->knc_protofmly, pf, KNC_STRSIZE);
			bcopy(knconf->knc_proto, pf, KNC_STRSIZE);
			data->knconf->knc_protofmly = pf;
			data->knconf->knc_proto = p;

			/* move server netname to the sec_data structure */
			if (nlen != 0) {
				data->netname = kmem_alloc(nlen, KM_SLEEP);
				bcopy(netname, data->netname, nlen);
				data->netnamelen = (int)nlen;
			}
			secdata->secmod = secdata->rpcflavor = AUTH_DES;
			secdata->data = (caddr_t)data;
		}
	} else {
		secdata = kmem_alloc(sizeof (*secdata), KM_SLEEP);
		secdata->secmod = secdata->rpcflavor = AUTH_UNIX;
		secdata->data = NULL;
	}
	svp->sv_secdata = secdata;
	if (error)
		goto errout;

	/*
	 * See bug 1180236.
	 * If mount secure failed, we will fall back to AUTH_NONE
	 * and try again.  nfs3rootvp() will turn this back off.
	 *
	 * The NFS Version 3 mount uses the FSINFO and GETATTR
	 * procedures.  The server should not care if these procedures
	 * have the proper security flavor, so if mount retries using
	 * AUTH_NONE that does not require a credential setup for root
	 * then the automounter would work without requiring root to be
	 * keylogged into AUTH_DES.
	 */
	if (secdata->rpcflavor != AUTH_UNIX &&
	    secdata->rpcflavor != AUTH_LOOPBACK)
		secdata->flags |= AUTH_F_TRYNONE;

	/*
	 * Failover support:
	 *
	 * We may have a linked list of nfs_args structures,
	 * which means the user is looking for failover.  If
	 * the mount is either not "read-only" or "soft",
	 * we want to bail out with EINVAL.
	 */
	if (STRUCT_FGET(args, nfs_args_ext) == NFS_ARGS_EXTB &&
	    STRUCT_FGETP(args, nfs_ext_u.nfs_extB.next) != NULL) {
		if (uap->flags & MS_RDONLY && !(flags & NFSMNT_SOFT)) {
			data = (char *)STRUCT_FGETP(args,
			    nfs_ext_u.nfs_extB.next);
			goto more;
		}
		error = EINVAL;
		goto errout;
	}

	/*
	 * Get root vnode.
	 */
	error = nfs3rootvp(&rtvp, vfsp, svp_head, flags, cr);

	if (error)
		goto errout;

	/*
	 * Set option fields in the mount info record
	 */
	mi = VTOMI(rtvp);

	if (svp_head->sv_next)
		mi->mi_flags |= MI_LLOCK;

	error = nfs_setopts(rtvp, get_udatamodel(), STRUCT_BUF(args));

errout:
	if (error) {
		if (rtvp != NULL) {
			rp = VTOR(rtvp);
			mutex_enter(&nfs_rtable_lock);
			if (rp->r_flags & RHASHED)
				rp_rmhash(rp);
			mutex_exit(&nfs_rtable_lock);
		}
		sv_free(svp_head);
		if (mi != NULL) {
			nfs_async_stop(vfsp);
			nfs_free_mi(mi);
		}
	}

	if (rtvp != NULL)
		VN_RELE(rtvp);

	return (error);
}

static int nfs3_dynamic = 0;	/* global variable to enable dynamic retrans. */
static u_short nfs3_max_threads = 8;	/* max number of active async threads */
static u_int nfs3_bsize = 32 * 1024;	/* client `block' size */
static u_int nfs3_async_clusters = 1;	/* # of reqs from each async queue */
static u_int nfs3_cots_timeo = NFS_COTS_TIMEO;

static int
nfs3rootvp(vnode_t **rtvpp, vfs_t *vfsp, struct servinfo *svp,
	int flags, cred_t *cr)
{
	vnode_t *rtvp;
	mntinfo_t *mi;
	dev_t nfs_dev;
	struct vattr va;
	struct FSINFO3args args;
	struct FSINFO3res res;
	int error;
	int douprintf;
	rnode_t *rp;
	int i;

	ASSERT(cr->cr_ref != 0);

	/*
	 * Create a mount record and link it to the vfs struct.
	 */
	mi = kmem_zalloc(sizeof (*mi), KM_SLEEP);
	mutex_init(&mi->mi_lock, NULL, MUTEX_DEFAULT, NULL);
	mi->mi_flags = MI_ACL;
	if (!(flags & NFSMNT_SOFT))
		mi->mi_flags |= MI_HARD;
	if ((flags & NFSMNT_SEMISOFT))
		mi->mi_flags |= MI_SEMISOFT;
	if ((flags & NFSMNT_NOPRINT))
		mi->mi_flags |= MI_NOPRINT;
	if (flags & NFSMNT_INT)
		mi->mi_flags |= MI_INT;
	mi->mi_retrans = NFS_RETRIES;
	if (svp->sv_knconf->knc_semantics == NC_TPI_COTS_ORD ||
	    svp->sv_knconf->knc_semantics == NC_TPI_COTS)
		mi->mi_timeo = nfs3_cots_timeo;
	else
		mi->mi_timeo = NFS_TIMEO;
	mi->mi_prog = NFS_PROGRAM;
	mi->mi_vers = NFS_V3;
	mi->mi_rfsnames = rfsnames_v3;
	mi->mi_reqs = rfsreqcnt_v3;
	mi->mi_call_type = call_type_v3;
	mi->mi_ss_call_type = ss_call_type_v3;
	mi->mi_timer_type = timer_type_v3;
	mi->mi_aclnames = aclnames_v3;
	mi->mi_aclreqs = aclreqcnt_v3_ptr;
	mi->mi_acl_call_type = acl_call_type_v3;
	mi->mi_acl_ss_call_type = acl_ss_call_type_v3;
	mi->mi_acl_timer_type = acl_timer_type_v3;
	cv_init(&mi->mi_failover_cv, NULL, CV_DEFAULT, NULL);
	mi->mi_servers = svp;
	mi->mi_curr_serv = svp;
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;

	if (nfs3_dynamic)
		mi->mi_flags |= MI_DYNAMIC;

	if (flags & NFSMNT_DIRECTIO)
		mi->mi_flags |= MI_DIRECTIO;

	/*
	 * Make a vfs struct for nfs.  We do this here instead of below
	 * because rtvp needs a vfs before we can do a getattr on it.
	 *
	 * Assign a unique device id to the mount
	 */
	mutex_enter(&nfs_minor_lock);
	do {
		nfs_minor = (nfs_minor + 1) & MAXMIN32;
		nfs_dev = makedevice(nfs_major, nfs_minor);
	} while (vfs_devismounted(nfs_dev));
	mutex_exit(&nfs_minor_lock);

	vfsp->vfs_dev = nfs_dev;
	vfs_make_fsid(&vfsp->vfs_fsid, nfs_dev, nfs3fstyp);
	vfsp->vfs_data = (caddr_t)mi;
	vfsp->vfs_fstype = nfsfstyp;
	vfsp->vfs_bsize = nfs3_bsize;

	/*
	 * Initialize fields used to support async putpage operations.
	 */
	for (i = 0; i < NFS_ASYNC_TYPES; i++)
		mi->mi_async_clusters[i] = nfs3_async_clusters;
	mi->mi_async_init_clusters = nfs3_async_clusters;
	mi->mi_async_curr = &mi->mi_async_reqs[0];
	mi->mi_max_threads = nfs3_max_threads;
	mutex_init(&mi->mi_async_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&mi->mi_async_reqs_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&mi->mi_async_cv, NULL, CV_DEFAULT, NULL);

	mi->mi_vfsp = vfsp;

	/*
	 * Make the root vnode, use it to get attributes,
	 * then remake it with the attributes.
	 */
	rtvp = makenfs3node((nfs_fh3 *)&svp->sv_fhandle,
	    NULL, vfsp, cr, NULL, NULL);

	/*
	 * Make the FSINFO calls, primarily at this point to
	 * determine the transfer size.  For client failover,
	 * we'll want this to be the minimum bid from any
	 * server, so that we don't overrun stated limits.
	 *
	 * While we're looping, we'll turn off AUTH_F_TRYNONE,
	 * which is only for the mount operation.
	 */

	mi->mi_tsize = nfs3tsize();
	mi->mi_stsize = nfs3tsize();

	for (svp = mi->mi_servers; svp != NULL; svp = svp->sv_next) {
		douprintf = 1;
		mi->mi_curr_serv = svp;
		args.fsroot = *(nfs_fh3 *)&svp->sv_fhandle;
		error = rfs3call(mi, NFSPROC3_FSINFO,
		    xdr_nfs_fh3, (caddr_t)&args,
		    xdr_FSINFO3res, (caddr_t)&res, cr,
		    &douprintf, &res.status, 0, NULL);
		if (error)
			goto bad;
		error = geterrno3(res.status);
		if (error)
			goto bad;

		/* get type of root node */
		if (res.resok.obj_attributes.attributes) {
			if (res.resok.obj_attributes.attr.type < NF3REG ||
			    res.resok.obj_attributes.attr.type > NF3FIFO) {
#ifdef DEBUG
				cmn_err(CE_WARN,
			    "NFS3 server %s returned a bad file type for root",
				    svp->sv_hostname);
#else
				cmn_err(CE_WARN,
			    "NFS server %s returned a bad file type for root",
				    svp->sv_hostname);
#endif
				error = EINVAL;
				goto bad;
			} else {
				if (rtvp->v_type != VNON &&
		rtvp->v_type != nf3_to_vt[res.resok.obj_attributes.attr.type]) {
#ifdef DEBUG
					cmn_err(CE_WARN,
		"NFS3 server %s returned a different file type for root",
					    svp->sv_hostname);
#else
					cmn_err(CE_WARN,
		"NFS server %s returned a different file type for root",
					    svp->sv_hostname);
#endif
					error = EINVAL;
					goto bad;
				}
				rtvp->v_type =
				nf3_to_vt[res.resok.obj_attributes.attr.type];
			}
		}

		if (res.resok.rtpref != 0)
			mi->mi_tsize = MIN(res.resok.rtpref, mi->mi_tsize);
		else if (res.resok.rtmax != 0)
			mi->mi_tsize = MIN(res.resok.rtmax, mi->mi_tsize);
		else {
#ifdef DEBUG
			cmn_err(CE_WARN,
			    "NFS3 server %s returned 0 for read transfer sizes",
			    svp->sv_hostname);
#else
			cmn_err(CE_WARN,
			    "NFS server %s returned 0 for transfer sizes",
			    svp->sv_hostname);
#endif
			error = EIO;
			goto bad;
		}
		if (res.resok.wtpref != 0)
			mi->mi_stsize = MIN(res.resok.wtpref, mi->mi_stsize);
		else if (res.resok.wtmax != 0)
			mi->mi_stsize = MIN(res.resok.wtmax, mi->mi_stsize);
		else {
#ifdef DEBUG
			cmn_err(CE_WARN,
			"NFS3 server %s returned 0 for write transfer sizes",
			    svp->sv_hostname);
#else
			cmn_err(CE_WARN,
			"NFS server %s returned 0 for write transfer sizes",
			    svp->sv_hostname);
#endif
			error = EIO;
			goto bad;
		}

		/*
		 * These signal the ability of the server to create
		 * hard links and symbolic links, so they really
		 * aren't relevant if there is more than one server.
		 * We'll set them here, though it probably looks odd.
		 */
		if (res.resok.properties & FSF3_LINK)
			mi->mi_flags |= MI_LINK;
		if (res.resok.properties & FSF3_SYMLINK)
			mi->mi_flags |= MI_SYMLINK;

		/* Pick up smallest non-zero maxfilesize value */
		if (res.resok.maxfilesize) {
			if (mi->mi_maxfilesize) {
				mi->mi_maxfilesize = MIN(mi->mi_maxfilesize,
							res.resok.maxfilesize);
			} else
				mi->mi_maxfilesize = res.resok.maxfilesize;
		}

		/*
		 * AUTH_F_TRYNONE is only for the mount operation,
		 * so turn it back off.
		 */
		svp->sv_secdata->flags &= ~AUTH_F_TRYNONE;
	}
	mi->mi_curr_serv = mi->mi_servers;
	mi->mi_curread = mi->mi_tsize;
	mi->mi_curwrite = mi->mi_stsize;

	/*
	 * Initialize kstats
	 */
	nfs_mnt_kstat_init(vfsp);

	/* If we didn't get a type, get one now */
	if (rtvp->v_type == VNON) {
		va.va_mask = AT_ALL;
		error = nfs3getattr(rtvp, &va, cr);
		if (error)
			goto bad;
		rtvp->v_type = va.va_type;
	}

	mi->mi_type = rtvp->v_type;

	*rtvpp = rtvp;
	return (0);
bad:
	/*
	 * An error occurred somewhere, need to clean up...
	 * We need to release our reference to the root vnode and
	 * destroy the mntinfo struct that we just created.
	 */
	rp = VTOR(rtvp);
	mutex_enter(&nfs_rtable_lock);
	if (rp->r_flags & RHASHED)
		rp_rmhash(rp);
	mutex_exit(&nfs_rtable_lock);
	VN_RELE(rtvp);
	nfs_async_stop(vfsp);
	nfs_free_mi(mi);
	*rtvpp = NULL;
	return (error);
}

/*
 * vfs operations
 */
static int
nfs3_unmount(vfs_t *vfsp, int flag, cred_t *cr)
{
	mntinfo_t *mi;
	u_short omax;

	if (!suser(cr))
		return (EPERM);

	mi = VFTOMI(vfsp);
	if (flag & MS_FORCE) {
		vfsp->vfs_flag |= VFS_UNMOUNTED;
		mutex_enter(&nfs_rtable_lock);
		destroy_rtable(vfsp, cr);
		mutex_exit(&nfs_rtable_lock);
		return (0);
	}
	/*
	 * Wait until all asynchronous putpage operations on
	 * this file system are complete before flushing rnodes
	 * from the cache.
	 */
	omax = mi->mi_max_threads;
	if (nfs_async_stop_sig(vfsp)) {
		return (EINTR);
	}
	rflush(vfsp, cr);
	mutex_enter(&nfs_rtable_lock);
	/*
	 * If there are any active vnodes on this file system,
	 * then the file system is busy and can't be umounted.
	 */
	if (check_rtable(vfsp)) {
		mutex_exit(&nfs_rtable_lock);
		mutex_enter(&mi->mi_async_lock);
		mi->mi_max_threads = omax;
		mutex_exit(&mi->mi_async_lock);
		return (EBUSY);
	}
	/*
	 * Destroy all rnodes belonging to this file system from the
	 * rnode hash queues and purge any resources allocated to
	 * them.
	 */
	destroy_rtable(vfsp, cr);
	mutex_exit(&nfs_rtable_lock);
	return (0);
}

/*
 * find root of nfs
 */
static int
nfs3_root(vfs_t *vfsp, vnode_t **vpp)
{
	mntinfo_t *mi;
	vnode_t *vp;

	mi = VFTOMI(vfsp);

	vp = makenfs3node((nfs_fh3 *)&mi->mi_curr_serv->sv_fhandle,
	    NULL, vfsp, CRED(), NULL, NULL);

	ASSERT(vp->v_type == VNON || vp->v_type == mi->mi_type);

	vp->v_type = mi->mi_type;

	*vpp = vp;

	return (0);
}

/*
 * Get file system statistics.
 */
static int
nfs3_statvfs(vfs_t *vfsp, struct statvfs64 *sbp)
{
	int error;
	struct mntinfo *mi;
	struct FSSTAT3args args;
	struct FSSTAT3res res;
	int douprintf;
	failinfo_t fi;
	vnode_t *vp;
	cred_t *cr;

	error = nfs3_root(vfsp, &vp);
	if (error)
		return (error);

	mi = VFTOMI(vfsp);
	cr = CRED();

	args.fsroot = *VTOFH3(vp);
	fi.vp = vp;
	fi.fhp = (caddr_t)&args.fsroot;
	fi.copyproc = nfs3copyfh;
	fi.lookupproc = nfs3lookup;

	douprintf = 1;
	error = rfs3call(mi, NFSPROC3_FSSTAT,
	    xdr_nfs_fh3, (caddr_t)&args,
	    xdr_FSSTAT3res, (caddr_t)&res, cr,
	    &douprintf, &res.status, 0, &fi);

	if (error) {
		VN_RELE(vp);
		return (error);
	}

	error = geterrno3(res.status);
	if (!error) {
		nfs3_cache_post_op_attr(vp, &res.resok.obj_attributes, cr);
		sbp->f_bsize = MAXBSIZE;
		sbp->f_frsize = DEV_BSIZE;
		/*
		 * Allow -1 fields to pass through unconverted.  These
		 * indicate "don't know" fields.
		 */
		if (res.resok.tbytes == (size3)-1)
			sbp->f_blocks = (fsblkcnt64_t)res.resok.tbytes;
		else {
			sbp->f_blocks = (fsblkcnt64_t)
			    (res.resok.tbytes / DEV_BSIZE);
		}
		if (res.resok.fbytes == (size3)-1)
			sbp->f_bfree = (fsblkcnt64_t)res.resok.fbytes;
		else {
			sbp->f_bfree = (fsblkcnt64_t)
			    (res.resok.fbytes / DEV_BSIZE);
		}
		if (res.resok.abytes == (size3)-1)
			sbp->f_bavail = (fsblkcnt64_t)res.resok.abytes;
		else {
			sbp->f_bavail = (fsblkcnt64_t)
			    (res.resok.abytes / DEV_BSIZE);
		}
		sbp->f_files = (fsfilcnt64_t)res.resok.tfiles;
		sbp->f_ffree = (fsfilcnt64_t)res.resok.ffiles;
		sbp->f_favail = (fsfilcnt64_t)res.resok.afiles;
		sbp->f_fsid = (unsigned long)vfsp->vfs_fsid.val[0];
		(void) strncpy(sbp->f_basetype,
		    vfssw[vfsp->vfs_fstype].vsw_name, FSTYPSZ);
		sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
		sbp->f_namemax = (u_long)-1;
	} else {
		nfs3_cache_post_op_attr(vp, &res.resfail.obj_attributes, cr);
		PURGE_STALE_FH(error, vp, cr);
	}

	VN_RELE(vp);

	return (error);
}

static kmutex_t nfs3_syncbusy;

/*
 * Flush dirty nfs files for file system vfsp.
 * If vfsp == NULL, all nfs files are flushed.
 */
/* ARGSUSED */
static int
nfs3_sync(vfs_t *vfsp, short flag, cred_t *cr)
{
	if (!(flag & SYNC_ATTR) && mutex_tryenter(&nfs3_syncbusy) != 0) {
		rflush(vfsp, cr);
		mutex_exit(&nfs3_syncbusy);
	}
	return (0);
}

/* ARGSUSED */
static int
nfs3_vget(vfs_t *vfsp, vnode_t **vpp, fid_t *fidp)
{
	int error;
	nfs_fh3 fh;
	vnode_t *vp;
	struct vattr va;

	if (fidp->fid_len > NFS3_FHSIZE) {
		*vpp = NULL;
		return (ESTALE);
	}

	fh.fh3_length = fidp->fid_len;
	bcopy(fidp->fid_data, fh.fh3_u.data, fh.fh3_length);

	vp = makenfs3node(&fh, NULL, vfsp, CRED(), NULL, NULL);

	if (vp->v_type == VNON) {
		va.va_mask = AT_ALL;
		error = nfs3getattr(vp, &va, CRED());
		if (error) {
			VN_RELE(vp);
			*vpp = NULL;
			return (error);
		}
		vp->v_type = va.va_type;
	}

	*vpp = vp;

	return (0);
}

/* ARGSUSED */
static int
nfs3_mountroot(vfs_t *vfsp, whymountroot_t why)
{
	vnode_t *rtvp;
	char root_hostname[SYS_NMLN+1];
	struct servinfo *svp;
	int error;
	size_t size;
	char *root_path;
	struct pathname pn;
	char *name;
	cred_t *cr;
	mntinfo_t *mi;
	struct nfs_args args;		/* nfs mount arguments */
	static char token[10];

	bzero(&args, sizeof (args));

	/* do this BEFORE getfile which causes xid stamps to be initialized */
	clkset(-1L);		/* hack for now - until we get time svc? */

	if (why == ROOT_REMOUNT) {
		/*
		 * Shouldn't happen.
		 */
		panic("nfs3_mountroot: why == ROOT_REMOUNT");
	}

	if (why == ROOT_UNMOUNT) {
		/*
		 * Nothing to do for NFS.
		 */
		return (0);
	}

	/*
	 * why == ROOT_INIT
	 */

	name = token;
	*name = 0;
	(void) getfsname("root", name);

	pn_alloc(&pn);
	root_path = pn.pn_path;

	svp = kmem_zalloc(sizeof (*svp), KM_SLEEP);
	svp->sv_knconf = kmem_zalloc(sizeof (*svp->sv_knconf), KM_SLEEP);
	svp->sv_knconf->knc_protofmly = kmem_alloc(KNC_STRSIZE, KM_SLEEP);
	svp->sv_knconf->knc_proto = kmem_alloc(KNC_STRSIZE, KM_SLEEP);

	/*
	 * Get server address
	 * Get the root fhandle
	 * Get server's transport
	 * Get server's hostname
	 * Get options
	 */
	args.addr = &svp->sv_addr;
	args.fh = (char *)&svp->sv_fhandle;
	args.knconf = svp->sv_knconf;
	args.hostname = root_hostname;
	if (error = mount_root(*name ? name : "root", root_path, NFS_V3,
	    &args)) {
		if (error == EPROTONOSUPPORT)
			nfs_cmn_err(error, CE_WARN, "nfs3_mountroot: "
			    "mount_root failed: server doesn't support NFS V3");
		else
			nfs_cmn_err(error, CE_WARN,
			    "nfs3_mountroot: mount_root failed: %m");
		sv_free(svp);
		pn_free(&pn);
		return (error);
	}
	svp->sv_hostnamelen = (int)(strlen(root_hostname) + 1);
	svp->sv_hostname = kmem_alloc(svp->sv_hostnamelen, KM_SLEEP);
	(void) strcpy(svp->sv_hostname, root_hostname);

	/*
	 * Force root partition to always be mounted with AUTH_UNIX for now
	 */
	svp->sv_secdata = kmem_alloc(sizeof (*svp->sv_secdata), KM_SLEEP);
	svp->sv_secdata->secmod = AUTH_UNIX;
	svp->sv_secdata->rpcflavor = AUTH_UNIX;
	svp->sv_secdata->data = NULL;

	cr = crgetcred();
	rtvp = NULL;

	error = nfs3rootvp(&rtvp, vfsp, svp, args.flags, cr);

	crfree(cr);

	if (error) {
		pn_free(&pn);
		goto errout;
	}

	error = nfs_setopts(rtvp, DATAMODEL_NATIVE, &args);
	if (error) {
		nfs_cmn_err(error, CE_WARN,
		    "nfs3_mountroot: invalid root mount options");
		pn_free(&pn);
		goto errout;
	}

	/*
	 * Set default attribute timeouts if cache-only client XXX
	 */
	if (why == ROOT_BACKMOUNT) {
		/* cache-only client */
		mi = VFTOMI(vfsp);
		mi->mi_acregmin = ACREGMIN;
		mi->mi_acregmax = ACREGMAX;
		mi->mi_acdirmin = ACDIRMIN;
		mi->mi_acdirmax = ACDIRMAX;
	}

	(void) vfs_lock_wait(vfsp);

	if (why != ROOT_BACKMOUNT)
		vfs_add(NULL, vfsp, 0);

	vfs_unlock(vfsp);

	size = strlen(svp->sv_hostname);
	(void) strcpy(rootfs.bo_name, svp->sv_hostname);
	rootfs.bo_name[size] = ':';
	(void) strcpy(&rootfs.bo_name[size + 1], root_path);

	pn_free(&pn);

errout:
	if (error) {
		sv_free(svp);
		nfs_async_stop(vfsp);
	}

	if (rtvp != NULL)
		VN_RELE(rtvp);

	return (error);
}

/*
 * Initialization routine for VFS routines.  Should only be called once
 */
int
nfs3_vfsinit(void)
{
	mutex_init(&nfs3_syncbusy, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

void
nfs3_vfsfini(void)
{
	mutex_destroy(&nfs3_syncbusy);
}

void
nfs3_freevfs(vfs_t *vfsp)
{
	mntinfo_t *mi;
	servinfo_t *svp;

	/* free up the resources */
	mi = VFTOMI(vfsp);
	svp = mi->mi_servers;
	mi->mi_servers = mi->mi_curr_serv = NULL;
	sv_free(svp);
	nfs_free_mi(mi);
}
