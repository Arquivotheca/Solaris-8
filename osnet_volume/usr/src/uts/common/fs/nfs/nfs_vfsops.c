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

#pragma ident "@(#)nfs_vfsops.c 1.114	99/08/25 SMI"
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

static int pathconf_get(struct mntinfo *, model_t, struct nfs_args *);
static void pathconf_rele(struct mntinfo *);

kstat_t *nfs_client_kstat;
kstat_t *nfs_server_kstat;

static kstat_named_t rfsreqcnt_v2[] = {
	{ "null",	KSTAT_DATA_UINT64 },
	{ "getattr",	KSTAT_DATA_UINT64 },
	{ "setattr",	KSTAT_DATA_UINT64 },
	{ "root",	KSTAT_DATA_UINT64 },
	{ "lookup",	KSTAT_DATA_UINT64 },
	{ "readlink",	KSTAT_DATA_UINT64 },
	{ "read",	KSTAT_DATA_UINT64 },
	{ "wrcache",	KSTAT_DATA_UINT64 },
	{ "write",	KSTAT_DATA_UINT64 },
	{ "create",	KSTAT_DATA_UINT64 },
	{ "remove",	KSTAT_DATA_UINT64 },
	{ "rename",	KSTAT_DATA_UINT64 },
	{ "link",	KSTAT_DATA_UINT64 },
	{ "symlink",	KSTAT_DATA_UINT64 },
	{ "mkdir",	KSTAT_DATA_UINT64 },
	{ "rmdir",	KSTAT_DATA_UINT64 },
	{ "readdir",	KSTAT_DATA_UINT64 },
	{ "statfs",	KSTAT_DATA_UINT64 }
};
static kstat_named_t *rfsreqcnt_v2_ptr = rfsreqcnt_v2;
static uint_t rfsreqcnt_v2_ndata = sizeof (rfsreqcnt_v2) /
	sizeof (kstat_named_t);

static char *rfsnames_v2[] = {
	"null", "getattr", "setattr", "unused", "lookup", "readlink", "read",
	"unused", "write", "create", "remove", "rename", "link", "symlink",
	"mkdir", "rmdir", "readdir", "fsstat"
};

/*
 * This table maps from NFS protocol number into call type.
 * Zero means a "Lookup" type call
 * One  means a "Read" type call
 * Two  means a "Write" type call
 * This is used to select a default time-out.
 */
static char call_type_v2[] = {
	0, 0, 1, 0, 0, 0, 1,
	0, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 0
};

/*
 * Similar table, but to determine which timer to use
 * (only real reads and writes!)
 */
static char timer_type_v2[] = {
	0, 0, 0, 0, 0, 0, 1,
	0, 2, 0, 0, 0, 0, 0,
	0, 0, 1, 0
};

/*
 * This table maps from NFS protocol number into a call type
 * for the semisoft mount option.
 * Zero means do not repeat operation.
 * One  means repeat.
 */
static char ss_call_type_v2[] = {
	0, 0, 1, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1,
	1, 1, 0, 0
};

/*
 * nfs vfs operations.
 */
static int	nfs_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static int	nfs_unmount(vfs_t *, int, cred_t *);
static int	nfs_root(vfs_t *, vnode_t **);
static int	nfs_statvfs(vfs_t *, struct statvfs64 *);
static int	nfs_sync(vfs_t *, short, cred_t *);
static int	nfs_vget(vfs_t *, vnode_t **, fid_t *);
static int	nfs_mountroot(vfs_t *, whymountroot_t);
static void	nfs_freevfs(vfs_t *);

static int	nfsrootvp(vnode_t **, vfs_t *, struct servinfo *,
		    int, cred_t *);

struct vfsops nfs_vfsops = {
	nfs_mount,
	nfs_unmount,
	nfs_root,
	nfs_statvfs,
	nfs_sync,
	nfs_vget,
	nfs_mountroot,
	fs_nosys,
	nfs_freevfs
};

vnode_t nfs_notfound;

/*
 * Initialize the vfs structure
 */

int nfsfstyp;

int
nfsinit(struct vfssw *vswp, int fstyp)
{
	vnode_t *vp;
	kstat_t *rfsproccnt_v2_kstat;
	kstat_t *rfsreqcnt_v2_kstat;
	kstat_t *aclproccnt_v2_kstat;
	kstat_t *aclreqcnt_v2_kstat;

	vswp->vsw_vfsops = &nfs_vfsops;
	nfsfstyp = fstyp;

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

	if ((rfsproccnt_v2_kstat = kstat_create("nfs", 0, "rfsproccnt_v2",
	    "misc", KSTAT_TYPE_NAMED, rfsproccnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsproccnt_v2_kstat->ks_data = (void *)rfsproccnt_v2_ptr;
		kstat_install(rfsproccnt_v2_kstat);
	}
	if ((rfsreqcnt_v2_kstat = kstat_create("nfs", 0, "rfsreqcnt_v2",
	    "misc", KSTAT_TYPE_NAMED, rfsreqcnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsreqcnt_v2_kstat->ks_data = (void *)rfsreqcnt_v2_ptr;
		kstat_install(rfsreqcnt_v2_kstat);
	}
	if ((aclproccnt_v2_kstat = kstat_create("nfs_acl", 0, "aclproccnt_v2",
	    "misc", KSTAT_TYPE_NAMED, aclproccnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclproccnt_v2_kstat->ks_data = (void *)aclproccnt_v2_ptr;
		kstat_install(aclproccnt_v2_kstat);
	}
	if ((aclreqcnt_v2_kstat = kstat_create("nfs_acl", 0, "aclreqcnt_v2",
	    "misc", KSTAT_TYPE_NAMED, aclreqcnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclreqcnt_v2_kstat->ks_data = (void *)aclreqcnt_v2_ptr;
		kstat_install(aclreqcnt_v2_kstat);
	}

	vp = &nfs_notfound;
	bzero(vp, sizeof (*vp));
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	vp->v_count = 1;
	vp->v_op = &nfs_vnodeops;

	return (0);
}

void
nfsfini(void)
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
	ksp = kstat_lookup_byname("nfs", 0, "rfsproccnt_v2");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs", 0, "rfsreqcnt_v2");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs_acl", 0, "aclproccnt_v2");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("nfs_acl", 0, "aclreqcnt_v2");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	vp = &nfs_notfound;
	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
}

/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
static int
nfs_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
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
	 *
	 * Remounts need to save the pathconf information.
	 * Part of the infamous static kludge.
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
		return (pathconf_get((struct mntinfo *)vfsp->vfs_data,
		    get_udatamodel(), STRUCT_BUF(args)));
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
	if (copyin(STRUCT_FGETP(args, fh), &(svp->sv_fhandle.fh_buf),
	    NFS_FHSIZE)) {
		error = EFAULT;
		goto errout;
	}
	svp->sv_fhandle.fh_len = NFS_FHSIZE;

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
	 * The NFS Version 2 mount uses GETATTR and STATFS procedures.
	 * The server does not care if these procedures have the proper
	 * authentication flavor, so if mount retries using AUTH_NONE
	 * that does not require a credential setup for root then the
	 * automounter would work without requiring root to be
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
	error = nfsrootvp(&rtvp, vfsp, svp_head, flags, cr);

	if (error)
		goto errout;

	/*
	 * Set option fields in the mount info record
	 */
	mi = VTOMI(rtvp);

	if (svp_head->sv_next)
		mi->mi_flags |= MI_LLOCK;

	error = nfs_setopts(rtvp, get_udatamodel(), STRUCT_BUF(args));
	if (!error) {
		/* static pathconf kludge */
		error = pathconf_get(mi, get_udatamodel(), STRUCT_BUF(args));
	}

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

/*
 * The pathconf information is kept on a linked list of kmem_alloc'ed
 * structs. We search the list & add a new struct iff there is no other
 * struct with the same information.
 * See sys/pathconf.h for ``the rest of the story.''
 */
static struct pathcnf *allpc = NULL;

static int
pathconf_get(struct mntinfo *mi, model_t model, struct nfs_args *args)
{
	struct pathcnf *p;
	struct pathcnf pc;
	STRUCT_DECL(pathcnf, pc_tmp);
	STRUCT_HANDLE(nfs_args, ap);
	int i;

#ifdef lint
	model = model;
#endif

	STRUCT_INIT(pc_tmp, model);
	STRUCT_SET_HANDLE(ap, model, args);

	if (mi->mi_pathconf != NULL) {
		pathconf_rele(mi);
		mi->mi_pathconf = NULL;
	}
	if ((STRUCT_FGET(ap, flags) & NFSMNT_POSIX) &&
	    STRUCT_FGETP(ap, pathconf) != NULL) {
		if (copyin(STRUCT_FGETP(ap, pathconf), STRUCT_BUF(pc_tmp),
		    STRUCT_SIZE(pc_tmp)))
			return (EFAULT);
		if (_PC_ISSET(_PC_ERROR, STRUCT_FGET(pc_tmp, pc_mask)))
			return (EINVAL);

		pc.pc_link_max = STRUCT_FGET(pc_tmp, pc_link_max);
		pc.pc_max_canon = STRUCT_FGET(pc_tmp, pc_max_canon);
		pc.pc_max_input = STRUCT_FGET(pc_tmp, pc_max_input);
		pc.pc_name_max = STRUCT_FGET(pc_tmp, pc_name_max);
		pc.pc_path_max = STRUCT_FGET(pc_tmp, pc_path_max);
		pc.pc_pipe_buf = STRUCT_FGET(pc_tmp, pc_pipe_buf);
		pc.pc_vdisable = STRUCT_FGET(pc_tmp, pc_vdisable);
		pc.pc_xxx = STRUCT_FGET(pc_tmp, pc_xxx);
		for (i = 0; i < _PC_N; i++)
			pc.pc_mask[i] = STRUCT_FGET(pc_tmp, pc_mask[i]);

		for (p = allpc; p != NULL; p = p->pc_next) {
			if (PCCMP(p, &pc) == 0)
				break;
		}
		if (p != NULL) {
			mi->mi_pathconf = p;
			p->pc_refcnt++;
		} else {
			p = kmem_alloc(sizeof (*p), KM_SLEEP);
			*p = pc;
			p->pc_next = allpc;
			p->pc_refcnt = 1;
			allpc = mi->mi_pathconf = p;
		}
	}
	return (0);
}

/*
 * release the static pathconf information
 */
static void
pathconf_rele(struct mntinfo *mi)
{
	if (mi->mi_pathconf != NULL) {
		if (--mi->mi_pathconf->pc_refcnt == 0) {
			struct pathcnf *p;
			struct pathcnf *p2;

			p2 = p = allpc;
			while (p != NULL && p != mi->mi_pathconf) {
				p2 = p;
				p = p->pc_next;
			}
			if (p == NULL)
				cmn_err(CE_PANIC, "mi->pathconf");
			if (p == allpc)
				allpc = p->pc_next;
			else
				p2->pc_next = p->pc_next;
			kmem_free(p, sizeof (*p));
			mi->mi_pathconf = NULL;
		}
	}
}

static int nfs_dynamic = 1;	/* global variable to enable dynamic retrans. */
static u_short nfs_max_threads = 8;	/* max number of active async threads */
static u_int nfs_async_clusters = 1;	/* # of reqs from each async queue */
static u_int nfs_cots_timeo = NFS_COTS_TIMEO;

static int
nfsrootvp(vnode_t **rtvpp, vfs_t *vfsp, struct servinfo *svp,
	int flags, cred_t *cr)
{
	vnode_t *rtvp;
	mntinfo_t *mi;
	dev_t nfs_dev;
	struct vattr va;
	int error;
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
		mi->mi_timeo = nfs_cots_timeo;
	else
		mi->mi_timeo = NFS_TIMEO;
	mi->mi_prog = NFS_PROGRAM;
	mi->mi_vers = NFS_VERSION;
	mi->mi_rfsnames = rfsnames_v2;
	mi->mi_reqs = rfsreqcnt_v2;
	mi->mi_call_type = call_type_v2;
	mi->mi_ss_call_type = ss_call_type_v2;
	mi->mi_timer_type = timer_type_v2;
	mi->mi_aclnames = aclnames_v2;
	mi->mi_aclreqs = aclreqcnt_v2_ptr;
	mi->mi_acl_call_type = acl_call_type_v2;
	mi->mi_acl_ss_call_type = acl_ss_call_type_v2;
	mi->mi_acl_timer_type = acl_timer_type_v2;
	cv_init(&mi->mi_failover_cv, NULL, CV_DEFAULT, NULL);
	mi->mi_servers = svp;
	mi->mi_curr_serv = svp;
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;

	if (nfs_dynamic)
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
	vfs_make_fsid(&vfsp->vfs_fsid, nfs_dev, nfsfstyp);
	vfsp->vfs_data = (caddr_t)mi;
	vfsp->vfs_fstype = nfsfstyp;
	vfsp->vfs_bsize = NFS_MAXDATA;

	/*
	 * Initialize fields used to support async putpage operations.
	 */
	for (i = 0; i < NFS_ASYNC_TYPES; i++)
		mi->mi_async_clusters[i] = nfs_async_clusters;
	mi->mi_async_init_clusters = nfs_async_clusters;
	mi->mi_async_curr = &mi->mi_async_reqs[0];
	mi->mi_max_threads = nfs_max_threads;
	mutex_init(&mi->mi_async_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&mi->mi_async_reqs_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&mi->mi_async_cv, NULL, CV_DEFAULT, NULL);

	mi->mi_vfsp = vfsp;

	/*
	 * Make the root vnode, use it to get attributes,
	 * then remake it with the attributes.
	 */
	rtvp = makenfsnode((fhandle_t *)svp->sv_fhandle.fh_buf,
	    NULL, vfsp, cr, NULL, NULL);

	va.va_mask = AT_ALL;
	error = nfsgetattr(rtvp, &va, cr);
	if (error)
		goto bad;
	rtvp->v_type = va.va_type;

	/*
	 * Poll every server to get the filesystem stats; we're
	 * only interested in the server's transfer size, and we
	 * want the minimum.
	 *
	 * While we're looping, we'll turn off AUTH_F_TRYNONE,
	 * which is only for the mount operation.
	 */

	mi->mi_tsize = MIN(NFS_MAXDATA, nfstsize());
	mi->mi_stsize = MIN(NFS_MAXDATA, nfstsize());

	for (svp = mi->mi_servers; svp != NULL; svp = svp->sv_next) {
		struct nfsstatfs fs;
		int douprintf;

		douprintf = 1;
		mi->mi_curr_serv = svp;

		error = rfs2call(mi, RFS_STATFS,
			xdr_fhandle, (caddr_t)svp->sv_fhandle.fh_buf,
			xdr_statfs, (caddr_t)&fs, CRED(), &douprintf,
			&fs.fs_status, 0, NULL);
		if (error)
			goto bad;
		mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
		svp->sv_secdata->flags &= ~AUTH_F_TRYNONE;
	}
	mi->mi_curr_serv = mi->mi_servers;
	mi->mi_curread = mi->mi_tsize;
	mi->mi_curwrite = mi->mi_stsize;

	/*
	 * Initialize kstats
	 */
	nfs_mnt_kstat_init(vfsp);

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
nfs_unmount(vfs_t *vfsp, int flag, cred_t *cr)
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
nfs_root(vfs_t *vfsp, vnode_t **vpp)
{
	mntinfo_t *mi;
	vnode_t *vp;

	mi = VFTOMI(vfsp);

	vp = makenfsnode((fhandle_t *)mi->mi_curr_serv->sv_fhandle.fh_buf,
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
nfs_statvfs(vfs_t *vfsp, struct statvfs64 *sbp)
{
	int error;
	mntinfo_t *mi;
	struct nfsstatfs fs;
	int douprintf;
	failinfo_t fi;
	vnode_t *vp;

	error = nfs_root(vfsp, &vp);
	if (error)
		return (error);

	mi = VFTOMI(vfsp);
	douprintf = 1;
	fi.vp = vp;
	fi.fhp = NULL;		/* no need to update, filehandle not copied */
	fi.copyproc = nfscopyfh;
	fi.lookupproc = nfslookup;

	error = rfs2call(mi, RFS_STATFS,
			xdr_fhandle, (caddr_t)VTOFH(vp),
			xdr_statfs, (caddr_t)&fs, CRED(), &douprintf,
			&fs.fs_status, 0, &fi);

	if (!error) {
		error = geterrno(fs.fs_status);
		if (!error) {
			mutex_enter(&mi->mi_lock);
			if (mi->mi_stsize) {
				mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
			} else {
				mi->mi_stsize = fs.fs_tsize;
				mi->mi_curwrite = mi->mi_stsize;
			}
			mutex_exit(&mi->mi_lock);
			sbp->f_bsize = fs.fs_bsize;
			sbp->f_frsize = fs.fs_bsize;
			sbp->f_blocks = (fsblkcnt64_t)fs.fs_blocks;
			sbp->f_bfree = (fsblkcnt64_t)fs.fs_bfree;
			/*
			 * Some servers may return negative available
			 * block counts.  They may do this because they
			 * calculate the number of available blocks by
			 * subtracting the number of used blocks from
			 * the total number of blocks modified by the
			 * minimum free value.  For example, if the
			 * minumum free percentage is 10 and the file
			 * system is greater than 90 percent full, then
			 * 90 percent of the total blocks minus the
			 * actual number of used blocks may be a
			 * negative number.
			 *
			 * In this case, we need to sign extend the
			 * negative number through the assignment from
			 * the 32 bit bavail count to the 64 bit bavail
			 * count.
			 *
			 * We need to be able to discern between there
			 * just being a lot of available blocks on the
			 * file system and the case described above.
			 * We are making the assumption that it does
			 * not make sense to have more available blocks
			 * than there are free blocks.  So, if there
			 * are, then we treat the number as if it were
			 * a negative number and arrange to have it
			 * sign extended when it is converted from 32
			 * bits to 64 bits.
			 */
			if (fs.fs_bavail <= fs.fs_bfree)
				sbp->f_bavail = (fsblkcnt64_t)fs.fs_bavail;
			else {
				sbp->f_bavail =
					(fsblkcnt64_t)((long)fs.fs_bavail);
			}
			sbp->f_files = (fsfilcnt64_t)-1;
			sbp->f_ffree = (fsfilcnt64_t)-1;
			sbp->f_favail = (fsfilcnt64_t)-1;
			sbp->f_fsid = (unsigned long)vfsp->vfs_fsid.val[0];
			(void) strncpy(sbp->f_basetype,
				vfssw[vfsp->vfs_fstype].vsw_name, FSTYPSZ);
			sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
			sbp->f_namemax = (uint32_t)-1;
		} else {
			PURGE_STALE_FH(error, vp, CRED());
		}
	}

	VN_RELE(vp);

	return (error);
}

static kmutex_t nfs_syncbusy;

/*
 * Flush dirty nfs files for file system vfsp.
 * If vfsp == NULL, all nfs files are flushed.
 */
/* ARGSUSED */
static int
nfs_sync(vfs_t *vfsp, short flag, cred_t *cr)
{
	if (!(flag & SYNC_ATTR) && mutex_tryenter(&nfs_syncbusy) != 0) {
		rflush(vfsp, cr);
		mutex_exit(&nfs_syncbusy);
	}
	return (0);
}

/* ARGSUSED */
static int
nfs_vget(vfs_t *vfsp, vnode_t **vpp, fid_t *fidp)
{
	int error;
	vnode_t *vp;
	struct vattr va;
	struct nfs_fid *nfsfidp = (struct nfs_fid *)fidp;

	if (fidp->fid_len != (sizeof (*nfsfidp) - sizeof (short))) {
#ifdef DEBUG
		cmn_err(CE_WARN, "nfs_vget: bad fid len, %d/%d", fidp->fid_len,
		    (int)(sizeof (*nfsfidp) - sizeof (short)));
#endif
		*vpp = NULL;
		return (ESTALE);
	}

	vp = makenfsnode((fhandle_t *)(nfsfidp->nf_data), NULL, vfsp, CRED(),
	    NULL, NULL);

	if (vp->v_type == VNON) {
		va.va_mask = AT_ALL;
		error = nfsgetattr(vp, &va, CRED());
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
nfs_mountroot(vfs_t *vfsp, whymountroot_t why)
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
		panic("nfs_mountroot: why == ROOT_REMOUNT");
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
	args.fh = (char *)&svp->sv_fhandle.fh_buf;
	args.knconf = svp->sv_knconf;
	args.hostname = root_hostname;
	if (error = mount_root(*name ? name : "root", root_path, NFS_VERSION,
	    &args)) {
		nfs_cmn_err(error, CE_WARN,
		    "nfs_mountroot: mount_root failed: %m");
		sv_free(svp);
		pn_free(&pn);
		return (error);
	}
	svp->sv_fhandle.fh_len = NFS_FHSIZE;
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

	error = nfsrootvp(&rtvp, vfsp, svp, args.flags, cr);

	crfree(cr);

	if (error) {
		pn_free(&pn);
		goto errout;
	}

	error = nfs_setopts(rtvp, DATAMODEL_NATIVE, &args);
	if (error) {
		nfs_cmn_err(error, CE_WARN,
		    "nfs_mountroot: invalid root mount options");
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
nfs_vfsinit(void)
{
	mutex_init(&nfs_syncbusy, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

void
nfs_vfsfini(void)
{
	mutex_destroy(&nfs_syncbusy);
}

void
nfs_freevfs(vfs_t *vfsp)
{
	mntinfo_t *mi;
	servinfo_t *svp;

	/* free up the resources */
	mi = VFTOMI(vfsp);
	pathconf_rele(mi);
	svp = mi->mi_servers;
	mi->mi_servers = mi->mi_curr_serv = NULL;
	sv_free(svp);
	nfs_free_mi(mi);
}
