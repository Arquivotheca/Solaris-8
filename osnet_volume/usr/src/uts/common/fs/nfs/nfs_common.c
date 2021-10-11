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
 *	(c) 1986-1991,1994,1995,1996  Sun Microsystems, Inc.
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)nfs_common.c	1.67	98/12/03 SMI"	/* SVr4.0 1.7 */

/*	nfs_common.c 1.4 88/09/19 SMI	*/

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/bootconf.h>
#include <fs/fs_subr.h>
#include <rpc/types.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/mount.h>
#include <nfs/nfssys.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/file.h>
#include <sys/fcntl.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/ddi.h>

/*
 * The psuedo NFS filesystem to allow diskless booting to dynamically
 * mount either a NFS V2 or NFS V3 filesystem.  This only implements
 * the VFS_MOUNTROOT op and is only intended to be used by the
 * diskless booting code until the real root filesystem is mounted.
 * Nothing else should ever call this!
 *
 * The strategy is that if the initial rootfs type is set to "nfsdyn"
 * by loadrootmodules() this filesystem is called to mount the
 * root filesystem.  It first attempts to mount a V3 filesystem
 * and if that fails due to an RPC version mismatch it tries V2.
 * once the real mount succeeds the vfsops and rootfs name are changed
 * to reflect the real filesystem type.
 */
static int nfsdyninit(struct vfssw *, int);
static int nfsdyn_mountroot(vfs_t *, whymountroot_t);

struct vfsops nfsdyn_vfsops = {
	fs_nosys,	/* mount */
	fs_nosys,	/* unmount */
	fs_nosys,	/* root */
	fs_nosys,	/* statvfs */
	fs_sync,	/* sync */
	fs_nosys,	/* vget */
	nfsdyn_mountroot,
	fs_nosys	/* swapvp */
};

/*
 * This mutex is used to serialize initialzation of the NFS version
 * independent kstat structures.  These need to get initialized once
 * and only once, can happen in either one of two places, so the
 * initialization support needs to be serialized.
 */
kmutex_t nfs_kstat_lock;

/*
 * Server statistics.  These are defined here, rather than in the server
 * code, so that they can be referenced before the nfssrv kmod is loaded.
 */

static kstat_named_t svstat[] = {
	{ "calls",	KSTAT_DATA_UINT64 },
	{ "badcalls",	KSTAT_DATA_UINT64 },
};

kstat_named_t *svstat_ptr = svstat;
uint_t svstat_ndata = sizeof (svstat) / sizeof (kstat_named_t);

static kstat_named_t aclproccnt_v2[] = {
	{ "null",	KSTAT_DATA_UINT64 },
	{ "getacl",	KSTAT_DATA_UINT64 },
	{ "setacl",	KSTAT_DATA_UINT64 },
	{ "getattr",	KSTAT_DATA_UINT64 },
	{ "access",	KSTAT_DATA_UINT64 }
};

kstat_named_t *aclproccnt_v2_ptr = aclproccnt_v2;
uint_t aclproccnt_v2_ndata = sizeof (aclproccnt_v2) / sizeof (kstat_named_t);

static kstat_named_t aclproccnt_v3[] = {
	{ "null",	KSTAT_DATA_UINT64 },
	{ "getacl",	KSTAT_DATA_UINT64 },
	{ "setacl",	KSTAT_DATA_UINT64 }
};

kstat_named_t *aclproccnt_v3_ptr = aclproccnt_v3;
uint_t aclproccnt_v3_ndata = sizeof (aclproccnt_v3) / sizeof (kstat_named_t);

static kstat_named_t rfsproccnt_v2[] = {
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

kstat_named_t *rfsproccnt_v2_ptr = rfsproccnt_v2;
uint_t rfsproccnt_v2_ndata = sizeof (rfsproccnt_v2) / sizeof (kstat_named_t);

kstat_named_t rfsproccnt_v3[] = {
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

kstat_named_t *rfsproccnt_v3_ptr = rfsproccnt_v3;
uint_t rfsproccnt_v3_ndata = sizeof (rfsproccnt_v3) / sizeof (kstat_named_t);

/*
 * The following data structures are used to configure the NFS
 * system call, the NFS Version 2 client VFS, and the NFS Version
 * 3 client VFS into the system.
 */

/*
 * The NFS system call.
 */
static struct sysent nfssysent = {
	2,
	SE_32RVAL1 | SE_ARGC, /* nfssys() returns one 32-bit result in rval1 */
	nfssys
};

static struct modlsys modlsys = {
	&mod_syscallops,
	"NFS syscall, client, and common",
	&nfssysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32,
	"NFS syscall, client, and common (32-bit)",
	&nfssysent
};
#endif /* _SYSCALL32_IMPL */

/*
 * The NFS Dynamic client VFS.
 */
static struct vfssw vfw = {
	"nfsdyn",
	nfsdyninit,
	&nfsdyn_vfsops,
	0
};

static struct modlfs modlfs = {
	&mod_fsops,
	"network filesystem",
	&vfw
};

/*
 * The NFS Version 2 client VFS.
 */
static struct vfssw vfw2 = {
	"nfs",
	nfsinit,
	&nfs_vfsops,
	0
};

static struct modlfs modlfs2 = {
	&mod_fsops,
	"network filesystem version 2",
	&vfw2
};

/*
 * The NFS Version 3 client VFS.
 */
static struct vfssw vfw3 = {
	"nfs3",
	nfs3init,
	&nfs3_vfsops,
	0
};

static struct modlfs modlfs3 = {
	&mod_fsops,
	"network filesystem version 3",
	&vfw3
};

/*
 * We have too many linkage structures so we define our own XXX
 */
struct modlinkage_big {
	int		ml_rev;		/* rev of loadable modules system */
	void		*ml_linkage[6];	/* NULL terminated list of */
					/* linkage structures */
};

/*
 * All of the module configuration linkages required to configure
 * the system call and client VFS's into the system.
 */
static struct modlinkage_big modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
	&modlfs,
	&modlfs2,
	&modlfs3,
	NULL
};

/*
 * specfs - for getfsname only??
 * rpcmod - too many symbols to build stubs for them all
 */
char _depends_on[] = "fs/specfs strmod/rpcmod misc/rpcsec";

/*
 * This routine is invoked automatically when the kernel module
 * containing this routine is loaded.  This allows module specific
 * initialization to be done when the module is loaded.
 */
int
_init(void)
{
	int status;

	if ((status = nfs_clntinit()) != 0) {
		cmn_err(CE_WARN, "_init: nfs_clntinit failed");
		return (status);
	}

	mutex_init(&nfs_kstat_lock, NULL, MUTEX_DEFAULT, NULL);

	status = mod_install((struct modlinkage *)&modlinkage);
	if (status) {
		/*
		 * Failed to install module, cleanup previous
		 * initialization work.
		 */
		nfs_clntfini();

		/*
		 * Clean up work performed indirectly by mod_installfs()
		 * as a result of our call to mod_install().
		 */
		nfs3fini();
		nfsfini();
		mutex_destroy(&nfs_kstat_lock);
	}

	return (status);
}

int
_fini(void)
{
	/* Don't allow module to be unloaded */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info((struct modlinkage *)&modlinkage, modinfop));
}

/*
 * General utilities
 */

/*
 * Returns the prefered transfer size in bytes based on
 * what network interfaces are available.
 */
int
nfstsize(void)
{
	/*
	 * For the moment, just return NFS_MAXDATA until we can query the
	 * appropriate transport.
	 */
	return (NFS_MAXDATA);
}

/*
 * Returns the prefered transfer size in bytes based on
 * what network interfaces are available.
 */

static int nfs3_max_transfer_size = 32 * 1024;

int
nfs3tsize(void)
{
	/*
	 * For the moment, just return nfs3_max_transfer_size until we
	 * can query the appropriate transport.
	 */
	return (nfs3_max_transfer_size);
}

/* ARGSUSED */
static int
nfsdyninit(struct vfssw *vswp, int fstyp)
{
	vswp->vsw_vfsops = &nfsdyn_vfsops;
	return (0);
}

/* ARGSUSED */
static int
nfsdyn_mountroot(vfs_t *vfsp, whymountroot_t why)
{
	char root_hostname[SYS_NMLN+1];
	struct servinfo *svp;
	int error;
	char *root_path;
	struct pathname pn;
	char *name;
	static char token[10];
	struct nfs_args args;		/* nfs mount arguments */

	bzero(&args, sizeof (args));

	/* do this BEFORE getfile which causes xid stamps to be initialized */
	clkset(-1L);		/* hack for now - until we get time svc? */

	if (why == ROOT_REMOUNT) {
		/*
		 * Shouldn't happen.
		 */
		panic("nfs3_mountroot: why == ROOT_REMOUNT\n");
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

	vfsp->vfs_op = &nfs3_vfsops;
	args.addr = &svp->sv_addr;
	args.fh = (char *)&svp->sv_fhandle;
	args.knconf = svp->sv_knconf;
	args.hostname = root_hostname;

	if (error = mount_root(*name ? name : "root", root_path,
	    NFS_V3, &args)) {
		if (error != EPROTONOSUPPORT) {
			nfs_cmn_err(error, CE_WARN,
			    "nfsdyn_mountroot: NFS3 mount_root failed: %m");
			sv_free(svp);
			pn_free(&pn);
			vfsp->vfs_op = &nfsdyn_vfsops;
			return (error);
		}

		bzero(&args, sizeof (args));
		args.addr = &svp->sv_addr;
		args.fh = (char *)&svp->sv_fhandle.fh_buf;
		args.knconf = svp->sv_knconf;
		args.hostname = root_hostname;

		vfsp->vfs_op = &nfs_vfsops;

		if (error = mount_root(*name ? name : "root", root_path,
		    NFS_VERSION, &args)) {
			nfs_cmn_err(error, CE_WARN,
			    "nfsdyn_mountroot: NFS2 mount_root failed: %m");
			sv_free(svp);
			pn_free(&pn);
			vfsp->vfs_op = &nfsdyn_vfsops;
			return (error);
		}
	}
	sv_free(svp);
	pn_free(&pn);
	return (VFS_MOUNTROOT(vfsp, why));
}

int
nfs_setopts(vnode_t *vp, model_t model, struct nfs_args *buf)
{
	mntinfo_t *mi;			/* mount info, pointed at by vfs */
	STRUCT_HANDLE(nfs_args, args);
	int flags;

#ifdef lint
	model = model;
#endif

	STRUCT_SET_HANDLE(args, model, buf);

	flags = STRUCT_FGET(args, flags);

	/*
	 * Set option fields in mount info record
	 */
	mi = VTOMI(vp);

	if (flags & NFSMNT_NOAC) {
		mi->mi_flags |= MI_NOAC;
		PURGE_ATTRCACHE(vp);
	}
	if (flags & NFSMNT_NOCTO)
		mi->mi_flags |= MI_NOCTO;
	if (flags & NFSMNT_LLOCK)
		mi->mi_flags |= MI_LLOCK;
	if (flags & NFSMNT_GRPID)
		mi->mi_flags |= MI_GRPID;
	if (flags & NFSMNT_RETRANS) {
		if (STRUCT_FGET(args, retrans) < 0)
			return (EINVAL);
		mi->mi_retrans = STRUCT_FGET(args, retrans);
	}
	if (flags & NFSMNT_TIMEO) {
		if (STRUCT_FGET(args, timeo) <= 0)
			return (EINVAL);
		mi->mi_timeo = STRUCT_FGET(args, timeo);
		/*
		 * The following scales the standard deviation and
		 * and current retransmission timer to match the
		 * initial value for the timeout specified.
		 */
		mi->mi_timers[NFS_CALLTYPES].rt_deviate =
		    (mi->mi_timeo * hz * 2) / 5;
		mi->mi_timers[NFS_CALLTYPES].rt_rtxcur =
		    mi->mi_timeo * hz / 10;
	}
	if (flags & NFSMNT_RSIZE) {
		if (STRUCT_FGET(args, rsize) <= 0)
			return (EINVAL);
		mi->mi_tsize = MIN(mi->mi_tsize, STRUCT_FGET(args, rsize));
		mi->mi_curread = mi->mi_tsize;
	}
	if (flags & NFSMNT_WSIZE) {
		if (STRUCT_FGET(args, wsize) <= 0)
			return (EINVAL);
		mi->mi_stsize = MIN(mi->mi_stsize, STRUCT_FGET(args, wsize));
		mi->mi_curwrite = mi->mi_stsize;
	}
	if (flags & NFSMNT_ACREGMIN) {
		if (STRUCT_FGET(args, acregmin) < 0)
			mi->mi_acregmin = ACMINMAX;
		else
			mi->mi_acregmin = MIN(STRUCT_FGET(args, acregmin),
			    ACMINMAX);
	}
	if (flags & NFSMNT_ACREGMAX) {
		if (STRUCT_FGET(args, acregmax) < 0)
			mi->mi_acregmax = ACMAXMAX;
		else
			mi->mi_acregmax = MIN(STRUCT_FGET(args, acregmax),
			    ACMAXMAX);
	}
	if (flags & NFSMNT_ACDIRMIN) {
		if (STRUCT_FGET(args, acdirmin) < 0)
			mi->mi_acdirmin = ACMINMAX;
		else
			mi->mi_acdirmin = MIN(STRUCT_FGET(args, acdirmin),
			    ACMINMAX);
	}
	if (flags & NFSMNT_ACDIRMAX) {
		if (STRUCT_FGET(args, acdirmax) < 0)
			mi->mi_acdirmax = ACMAXMAX;
		else
			mi->mi_acdirmax = MIN(STRUCT_FGET(args, acdirmax),
			    ACMAXMAX);
	}

	if (flags & NFSMNT_LOOPBACK)
		mi->mi_flags |= MI_LOOPBACK;

	return (0);
}

/*
 * Set or Clear direct I/O flag
 * VOP_RWLOCK() is held for write access to prevent a race condition
 * which would occur if a process is in the middle of a write when
 * directio flag gets set. It is possible that all pages may not get flushed.
 */

/* ARGSUSED */
int
nfs_directio(vnode_t *vp, int cmd, cred_t *cr)
{
	int	error = 0;
	rnode_t	*rp;

	rp = VTOR(vp);

	if (cmd == DIRECTIO_ON) {

		/*
		 * Can't enable directio if the file has been mmap'd.
		 */
		if (rp->r_mapcnt > 0)
			return (EAGAIN);

		/*
		 * Flush the page cache.
		 */

		VOP_RWLOCK(vp, 1);

		if (vp->v_pages != NULL &&
		    ((rp->r_flags & RDIRTY) || rp->r_awcount > 0)) {

		    error = VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, B_INVAL, cr);
		    if (error) {
			if (error == ENOSPC || error == EDQUOT) {
			    mutex_enter(&rp->r_statelock);
			    if (!rp->r_error)
				rp->r_error = error;
			    mutex_exit(&rp->r_statelock);
			}
			VOP_RWUNLOCK(vp, 1);
			return (error);
		    }
		}

		mutex_enter(&rp->r_statelock);
		rp->r_flags |= RDIRECTIO;
		mutex_exit(&rp->r_statelock);
		VOP_RWUNLOCK(vp, 1);
		return (0);
	}

	if (cmd == DIRECTIO_OFF) {
		mutex_enter(&rp->r_statelock);
		rp->r_flags &= ~RDIRECTIO;	/* disable direct mode */
		mutex_exit(&rp->r_statelock);
		return (0);
	}

	return (EINVAL);
}
