/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 *	Copyright (c) 1986-1992,1994,1995,1996 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident	"@(#)nfs_server.c	1.141	99/10/21 SMI"
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
#include <sys/dirent.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/unistd.h>
#include <sys/vtrace.h>
#include <sys/mode.h>
#include <sys/acl.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/auth_des.h>
#include <rpc/svc.h>
#include <rpc/xdr.h>

#include <nfs/nfs.h>
#include <nfs/export.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_acl.h>
#include <nfs/nfs_log.h>

#include <rpcsvc/nfsauth_prot.h>

#include <sys/modctl.h>

const char *kinet_ntop6(uchar_t *, char *, size_t);

/*
 * Module linkage information.
 */
char _depends_on[] = "fs/nfs strmod/rpcmod misc/rpcsec";

static struct modlmisc modlmisc = {
	&mod_miscops, "NFS server module"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	int status;

	if ((status = nfs_srvinit()) != 0) {
		cmn_err(CE_WARN, "_init: nfs_srvinit failed");
		return (status);
	}

	status = mod_install((struct modlinkage *)&modlinkage);
	if (status != 0) {
		/*
		 * Could not load module, cleanup previous
		 * initialization work.
		 */
		nfs_srvfini();
	}

	return (status);
}

int
_fini()
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * RPC dispatch table
 * Indexed by version, proc
 */

struct rpcdisp {
	void	  (*dis_proc)();	/* proc to call */
	xdrproc_t dis_xdrargs;		/* xdr routine to get args */
	xdrproc_t dis_fastxdrargs;	/* `fast' xdr routine to get args */
	int	  dis_argsz;		/* sizeof args */
	xdrproc_t dis_xdrres;		/* xdr routine to put results */
	xdrproc_t dis_fastxdrres;	/* `fast' xdr routine to put results */
	int	  dis_ressz;		/* size of results */
	void	  (*dis_resfree)();	/* frees space allocated by proc */
	int	  dis_flags;		/* flags, see below */
	fhandle_t *(*dis_getfh)();	/* returns the fhandle for the req */
};

#define	RPC_IDEMPOTENT	0x1	/* idempotent or not */
/*
 * Be very careful about which NFS procedures get the RPC_ALLOWANON bit.
 * Right now, it this bit is on, we ignore the results of per NFS request
 * access control.
 */
#define	RPC_ALLOWANON	0x2	/* allow anonymous access */
#define	RPC_MAPRESP	0x4	/* use mapped response buffer */
#define	RPC_AVOIDWORK	0x8	/* do work avoidance for dups */
#define	RPC_PUBLICFH_OK	0x10	/* allow use of public filehandle */

/*
 * PUBLICFH_CHECK() checks if the dispatch routine supports
 * RPC_PUBLICFH_OK, if the filesystem is exported public, and if the
 * incoming request is using the public filehandle. The check duplicates
 * the exportmatch() call done in findexport()/checkexport(), and we
 * should consider modifying those routines to avoid the duplication. For
 * now, we optimize by calling exportmatch() only after checking that the
 * dispatch routine supports RPC_PUBLICFH_OK, and if the filesystem is
 * explicitly exported public (i.e., not the placeholder).
 */
#define	PUBLICFH_CHECK(disp, exi, fh) \
		((disp->dis_flags & RPC_PUBLICFH_OK) && \
		((exi->exi_export.ex_flags & EX_PUBLIC) || \
		(exi == exi_public && exportmatch(exi_root, \
		&fh->fh_fsid, (fid_t *)&fh->fh_xlen))))

struct rpc_disptable {
	int dis_nprocs;
#ifdef TRACE
	char **dis_procnames;
#endif
	kstat_named_t **dis_proccntp;
	struct rpcdisp *dis_table;
};

static void	rpc_null(caddr_t *, caddr_t *);
static void	rfs_error(caddr_t *, caddr_t *);
static void	nullfree(void);
static void	rfs_dispatch(struct svc_req *, SVCXPRT *);
static void	acl_dispatch(struct svc_req *, SVCXPRT *);
static void	common_dispatch(struct svc_req *, SVCXPRT *,
		rpcvers_t, rpcvers_t, char *,
		struct rpc_disptable *);
static	int	checkauth(struct exportinfo *, struct svc_req *, cred_t *, int,
			bool_t);
static char	*client_name(struct svc_req *req);
static char	*client_addr(struct svc_req *req, char *buf);
extern	int	sec_svc_getcred(struct svc_req *, uid_t *, gid_t *,
			short *, gid_t *, char **, int *);
extern	bool_t	sec_svc_inrootlist(int, caddr_t, int, caddr_t *);


#define	NFSLOG_COPY_NETBUF(exi, xprt, nb)	{		\
	(nb)->maxlen = (xprt)->xp_rtaddr.maxlen;		\
	(nb)->len = (xprt)->xp_rtaddr.len;			\
	(nb)->buf = kmem_alloc((nb)->len, KM_SLEEP);		\
	bcopy((xprt)->xp_rtaddr.buf, (nb)->buf, (nb)->len);	\
	}

/*
 * Public Filehandle common nfs routines
 */
static int	MCLpath(char **);
static void	URLparse(char *);
static int	nfs_check_vpexi(vnode_t *p, vnode_t *, cred_t *,
			struct exportinfo **);

/*
 * NFS callout table.
 * This table is used by svc_getreq() to dispatch a request with
 * a given prog/vers pair to an appropriate service provider
 * dispatch routine.
 */
static SVC_CALLOUT __nfs_sc[] = {
	{ NFS_PROGRAM,	   NFS_VERSMIN,	    NFS_VERSMAX,	rfs_dispatch },
	{ NFS_ACL_PROGRAM, NFS_ACL_VERSMIN, NFS_ACL_VERSMAX,	acl_dispatch }
};

static SVC_CALLOUT_TABLE nfs_sct = {
	sizeof (__nfs_sc) / sizeof (__nfs_sc[0]), FALSE, __nfs_sc
};

/*
 * NFS Server system call.
 * Does all of the work of running a NFS server.
 * uap->fd is the fd of an open transport provider
 */
int
nfs_svc(struct nfs_svc_args *arg, model_t model)
{
	file_t *fp;
	SVCMASTERXPRT *xprt;
	int error;
	int readsize;
	char buf[KNC_STRSIZE];
	size_t len;
	STRUCT_HANDLE(nfs_svc_args, uap);
	struct netbuf addrmask;

#ifdef lint
	model = model;		/* STRUCT macros don't always refer to it */
#endif

	STRUCT_SET_HANDLE(uap, model, arg);

	if (!suser(CRED()))
		return (EPERM);

	if ((fp = getf(STRUCT_FGET(uap, fd))) == NULL)
		return (EBADF);

	/*
	 * Set read buffer size to rsize
	 * and add room for RPC headers.
	 */
	readsize = nfs3tsize() + (RPC_MAXDATASIZE - NFS_MAXDATA);
	if (readsize < RPC_MAXDATASIZE)
		readsize = RPC_MAXDATASIZE;

	error = copyinstr((const char *)STRUCT_FGETP(uap, netid), buf,
	    KNC_STRSIZE, &len);
	if (error) {
		releasef(STRUCT_FGET(uap, fd));
		return (error);
	}

	addrmask.len = STRUCT_FGET(uap, addrmask.len);
	addrmask.maxlen = STRUCT_FGET(uap, addrmask.maxlen);
	addrmask.buf = kmem_alloc(addrmask.maxlen, KM_SLEEP);
	error = copyin(STRUCT_FGETP(uap, addrmask.buf), addrmask.buf,
	    addrmask.len);
	if (error) {
		releasef(STRUCT_FGET(uap, fd));
		kmem_free(addrmask.buf, addrmask.maxlen);
		return (error);
	}

	/* Create a transport handle. */
	error = svc_tli_kcreate(fp, readsize, buf, &addrmask, &xprt,
				&nfs_sct, NULL, NFS_SVCPOOL_ID, TRUE);
	if (error)
		kmem_free(addrmask.buf, addrmask.maxlen);

	releasef(STRUCT_FGET(uap, fd));
	return (error);
}

/* ARGSUSED */
static void
rpc_null(caddr_t *argp, caddr_t *resp)
{
}

/* ARGSUSED */
static void
rfs_error(caddr_t *argp, caddr_t *resp)
{
	/* return (EOPNOTSUPP); */
}

static void
nullfree(void)
{
}

#ifdef TRACE
static char *rfscallnames_v2[] = {
	"RFS2_NULL",
	"RFS2_GETATTR",
	"RFS2_SETATTR",
	"RFS2_ROOT",
	"RFS2_LOOKUP",
	"RFS2_READLINK",
	"RFS2_READ",
	"RFS2_WRITECACHE",
	"RFS2_WRITE",
	"RFS2_CREATE",
	"RFS2_REMOVE",
	"RFS2_RENAME",
	"RFS2_LINK",
	"RFS2_SYMLINK",
	"RFS2_MKDIR",
	"RFS2_RMDIR",
	"RFS2_READDIR",
	"RFS2_STATFS"
};
#endif

static struct rpcdisp rfsdisptab_v2[] = {
	/*
	 * NFS VERSION 2
	 */

	/* RFS_NULL = 0 */
	{rpc_null,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* RFS_GETATTR = 1 */
	{rfs_getattr,
	    xdr_fhandle, xdr_fastfhandle, sizeof (fhandle_t),
	    xdr_attrstat, xdr_fastattrstat, sizeof (struct nfsattrstat),
	    nullfree, RPC_IDEMPOTENT|RPC_ALLOWANON|RPC_MAPRESP,
	    rfs_getattr_getfh},

	/* RFS_SETATTR = 2 */
	{rfs_setattr,
	    xdr_saargs, xdr_fastsaargs, sizeof (struct nfssaargs),
	    xdr_attrstat, xdr_fastattrstat, sizeof (struct nfsattrstat),
	    nullfree, RPC_MAPRESP,
	    rfs_setattr_getfh},

	/* RFS_ROOT = 3 *** NO LONGER SUPPORTED *** */
	{rfs_error,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* RFS_LOOKUP = 4 */
	{rfs_lookup,
	    xdr_diropargs, xdr_fastdiropargs, sizeof (struct nfsdiropargs),
	    xdr_diropres, xdr_fastdiropres, sizeof (struct nfsdiropres),
	    nullfree, RPC_IDEMPOTENT|RPC_MAPRESP|RPC_PUBLICFH_OK,
	    rfs_lookup_getfh},

	/* RFS_READLINK = 5 */
	{rfs_readlink,
	    xdr_fhandle, xdr_fastfhandle, sizeof (fhandle_t),
	    xdr_rdlnres, NULL_xdrproc_t, sizeof (struct nfsrdlnres),
	    rfs_rlfree, RPC_IDEMPOTENT,
	    rfs_readlink_getfh},

	/* RFS_READ = 6 */
	{rfs_read,
	    xdr_readargs, xdr_fastreadargs, sizeof (struct nfsreadargs),
	    xdr_rdresult, NULL_xdrproc_t, sizeof (struct nfsrdresult),
	    nullfree, RPC_IDEMPOTENT,
	    rfs_read_getfh},

	/* RFS_WRITECACHE = 7 *** NO LONGER SUPPORTED *** */
	{rfs_error,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* RFS_WRITE = 8 */
	{rfs_write,
	    xdr_writeargs, xdr_fastwriteargs, sizeof (struct nfswriteargs),
	    xdr_attrstat, xdr_fastattrstat, sizeof (struct nfsattrstat),
	    nullfree, RPC_MAPRESP,
	    rfs_write_getfh},

	/* RFS_CREATE = 9 */
	{rfs_create,
	    xdr_creatargs, xdr_fastcreatargs, sizeof (struct nfscreatargs),
	    xdr_diropres, xdr_fastdiropres, sizeof (struct nfsdiropres),
	    nullfree, RPC_MAPRESP,
	    rfs_create_getfh},

	/* RFS_REMOVE = 10 */
	{rfs_remove,
	    xdr_diropargs, xdr_fastdiropargs, sizeof (struct nfsdiropargs),
#ifdef _LITTLE_ENDIAN
	    xdr_enum, xdr_fastenum, sizeof (enum nfsstat),
#else
	    xdr_enum, NULL_xdrproc_t, sizeof (enum nfsstat),
#endif
	    nullfree, RPC_MAPRESP,
	    rfs_remove_getfh},

	/* RFS_RENAME = 11 */
	{rfs_rename,
	    xdr_rnmargs, xdr_fastrnmargs, sizeof (struct nfsrnmargs),
#ifdef _LITTLE_ENDIAN
	    xdr_enum, xdr_fastenum, sizeof (enum nfsstat),
#else
	    xdr_enum, NULL_xdrproc_t, sizeof (enum nfsstat),
#endif
	    nullfree, RPC_MAPRESP,
	    rfs_rename_getfh},

	/* RFS_LINK = 12 */
	{rfs_link,
	    xdr_linkargs, xdr_fastlinkargs, sizeof (struct nfslinkargs),
#ifdef _LITTLE_ENDIAN
	    xdr_enum, xdr_fastenum, sizeof (enum nfsstat),
#else
	    xdr_enum, NULL_xdrproc_t, sizeof (enum nfsstat),
#endif
	    nullfree, RPC_MAPRESP,
	    rfs_link_getfh},

	/* RFS_SYMLINK = 13 */
	{rfs_symlink,
	    xdr_slargs, xdr_fastslargs, sizeof (struct nfsslargs),
#ifdef _LITTLE_ENDIAN
	    xdr_enum, xdr_fastenum, sizeof (enum nfsstat),
#else
	    xdr_enum, NULL_xdrproc_t, sizeof (enum nfsstat),
#endif
	    nullfree, RPC_MAPRESP,
	    rfs_symlink_getfh},

	/* RFS_MKDIR = 14 */
	{rfs_mkdir,
	    xdr_creatargs, xdr_fastcreatargs, sizeof (struct nfscreatargs),
	    xdr_diropres, xdr_fastdiropres, sizeof (struct nfsdiropres),
	    nullfree, RPC_MAPRESP,
	    rfs_mkdir_getfh},

	/* RFS_RMDIR = 15 */
	{rfs_rmdir,
	    xdr_diropargs, xdr_fastdiropargs, sizeof (struct nfsdiropargs),
#ifdef _LITTLE_ENDIAN
	    xdr_enum, xdr_fastenum, sizeof (enum nfsstat),
#else
	    xdr_enum, NULL_xdrproc_t, sizeof (enum nfsstat),
#endif
	    nullfree, RPC_MAPRESP,
	    rfs_rmdir_getfh},

	/* RFS_READDIR = 16 */
	{rfs_readdir,
	    xdr_rddirargs, xdr_fastrddirargs, sizeof (struct nfsrddirargs),
	    xdr_putrddirres, NULL_xdrproc_t, sizeof (struct nfsrddirres),
	    rfs_rddirfree, RPC_IDEMPOTENT,
	    rfs_readdir_getfh},

	/* RFS_STATFS = 17 */
	{rfs_statfs,
	    xdr_fhandle, xdr_fastfhandle, sizeof (fhandle_t),
	    xdr_statfs, xdr_faststatfs, sizeof (struct nfsstatfs),
	    nullfree, RPC_IDEMPOTENT|RPC_ALLOWANON|RPC_MAPRESP,
	    rfs_statfs_getfh},
};

#ifdef TRACE
static char *rfscallnames_v3[] = {
	"RFS3_NULL",
	"RFS3_GETATTR",
	"RFS3_SETATTR",
	"RFS3_LOOKUP",
	"RFS3_ACCESS",
	"RFS3_READLINK",
	"RFS3_READ",
	"RFS3_WRITE",
	"RFS3_CREATE",
	"RFS3_MKDIR",
	"RFS3_SYMLINK",
	"RFS3_MKNOD",
	"RFS3_REMOVE",
	"RFS3_RMDIR",
	"RFS3_RENAME",
	"RFS3_LINK",
	"RFS3_READDIR",
	"RFS3_READDIRPLUS",
	"RFS3_FSSTAT",
	"RFS3_FSINFO",
	"RFS3_PATHCONF",
	"RFS3_COMMIT"
};
#endif

static struct rpcdisp rfsdisptab_v3[] = {
	/*
	 * NFS VERSION 3
	 */

	/* RFS_NULL = 0 */
	{rpc_null,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* RFS3_GETATTR = 1 */
	{rfs3_getattr,
	    xdr_nfs_fh3, xdr_fastnfs_fh3, sizeof (GETATTR3args),
	    xdr_GETATTR3res, NULL_xdrproc_t, sizeof (GETATTR3res),
	    nullfree, (RPC_IDEMPOTENT | RPC_ALLOWANON),
	    rfs3_getattr_getfh},

	/* RFS3_SETATTR = 2 */
	{rfs3_setattr,
	    xdr_SETATTR3args, NULL_xdrproc_t, sizeof (SETATTR3args),
	    xdr_SETATTR3res, NULL_xdrproc_t, sizeof (SETATTR3res),
	    nullfree, 0,
	    rfs3_setattr_getfh},

	/* RFS3_LOOKUP = 3 */
	{rfs3_lookup,
	    xdr_diropargs3, xdr_fastdiropargs3, sizeof (LOOKUP3args),
	    xdr_LOOKUP3res, NULL_xdrproc_t, sizeof (LOOKUP3res),
	    nullfree, (RPC_IDEMPOTENT | RPC_PUBLICFH_OK),
	    rfs3_lookup_getfh},

	/* RFS3_ACCESS = 4 */
	{rfs3_access,
	    xdr_ACCESS3args, NULL_xdrproc_t, sizeof (ACCESS3args),
	    xdr_ACCESS3res, NULL_xdrproc_t, sizeof (ACCESS3res),
	    nullfree, RPC_IDEMPOTENT,
	    rfs3_access_getfh},

	/* RFS3_READLINK = 5 */
	{rfs3_readlink,
	    xdr_nfs_fh3, xdr_fastnfs_fh3, sizeof (READLINK3args),
	    xdr_READLINK3res, NULL_xdrproc_t, sizeof (READLINK3res),
	    rfs3_readlink_free, RPC_IDEMPOTENT,
	    rfs3_readlink_getfh},

	/* RFS3_READ = 6 */
	{rfs3_read,
	    xdr_READ3args, NULL_xdrproc_t, sizeof (READ3args),
	    xdr_READ3res, NULL_xdrproc_t, sizeof (READ3res),
	    nullfree, RPC_IDEMPOTENT,
	    rfs3_read_getfh},

	/* RFS3_WRITE = 7 */
	{rfs3_write,
	    xdr_WRITE3args, NULL_xdrproc_t, sizeof (WRITE3args),
	    xdr_WRITE3res, NULL_xdrproc_t, sizeof (WRITE3res),
	    nullfree, 0,
	    rfs3_write_getfh},

	/* RFS3_CREATE = 8 */
	{rfs3_create,
	    xdr_CREATE3args, NULL_xdrproc_t, sizeof (CREATE3args),
	    xdr_CREATE3res, NULL_xdrproc_t, sizeof (CREATE3res),
	    nullfree, 0,
	    rfs3_create_getfh},

	/* RFS3_MKDIR = 9 */
	{rfs3_mkdir,
	    xdr_MKDIR3args, NULL_xdrproc_t, sizeof (MKDIR3args),
	    xdr_MKDIR3res, NULL_xdrproc_t, sizeof (MKDIR3res),
	    nullfree, 0,
	    rfs3_mkdir_getfh},

	/* RFS3_SYMLINK = 10 */
	{rfs3_symlink,
	    xdr_SYMLINK3args, NULL_xdrproc_t, sizeof (SYMLINK3args),
	    xdr_SYMLINK3res, NULL_xdrproc_t, sizeof (SYMLINK3res),
	    nullfree, 0,
	    rfs3_symlink_getfh},

	/* RFS3_MKNOD = 11 */
	{rfs3_mknod,
	    xdr_MKNOD3args, NULL_xdrproc_t, sizeof (MKNOD3args),
	    xdr_MKNOD3res, NULL_xdrproc_t, sizeof (MKNOD3res),
	    nullfree, 0,
	    rfs3_mknod_getfh},

	/* RFS3_REMOVE = 12 */
	{rfs3_remove,
	    xdr_diropargs3, xdr_fastdiropargs3, sizeof (REMOVE3args),
	    xdr_REMOVE3res, NULL_xdrproc_t, sizeof (REMOVE3res),
	    nullfree, 0,
	    rfs3_remove_getfh},

	/* RFS3_RMDIR = 13 */
	{rfs3_rmdir,
	    xdr_diropargs3, xdr_fastdiropargs3, sizeof (RMDIR3args),
	    xdr_RMDIR3res, NULL_xdrproc_t, sizeof (RMDIR3res),
	    nullfree, 0,
	    rfs3_rmdir_getfh},

	/* RFS3_RENAME = 14 */
	{rfs3_rename,
	    xdr_RENAME3args, NULL_xdrproc_t, sizeof (RENAME3args),
	    xdr_RENAME3res, NULL_xdrproc_t, sizeof (RENAME3res),
	    nullfree, 0,
	    rfs3_rename_getfh},

	/* RFS3_LINK = 15 */
	{rfs3_link,
	    xdr_LINK3args, NULL_xdrproc_t, sizeof (LINK3args),
	    xdr_LINK3res, NULL_xdrproc_t, sizeof (LINK3res),
	    nullfree, 0,
	    rfs3_link_getfh},

	/* RFS3_READDIR = 16 */
	{rfs3_readdir,
	    xdr_READDIR3args, NULL_xdrproc_t, sizeof (READDIR3args),
	    xdr_READDIR3res, NULL_xdrproc_t, sizeof (READDIR3res),
	    rfs3_readdir_free, RPC_IDEMPOTENT,
	    rfs3_readdir_getfh},

	/* RFS3_READDIRPLUS = 17 */
	{rfs3_readdirplus,
	    xdr_READDIRPLUS3args, NULL_xdrproc_t, sizeof (READDIRPLUS3args),
	    xdr_READDIRPLUS3res, NULL_xdrproc_t, sizeof (READDIRPLUS3res),
	    rfs3_readdirplus_free, RPC_AVOIDWORK,
	    rfs3_readdirplus_getfh},

	/* RFS3_FSSTAT = 18 */
	{rfs3_fsstat,
	    xdr_nfs_fh3, xdr_fastnfs_fh3, sizeof (FSSTAT3args),
	    xdr_FSSTAT3res, NULL_xdrproc_t, sizeof (FSSTAT3res),
	    nullfree, RPC_IDEMPOTENT,
	    rfs3_fsstat_getfh},

	/* RFS3_FSINFO = 19 */
	{rfs3_fsinfo,
	    xdr_nfs_fh3, xdr_fastnfs_fh3, sizeof (FSINFO3args),
	    xdr_FSINFO3res, NULL_xdrproc_t, sizeof (FSINFO3res),
	    nullfree, RPC_IDEMPOTENT|RPC_ALLOWANON,
	    rfs3_fsinfo_getfh},

	/* RFS3_PATHCONF = 20 */
	{rfs3_pathconf,
	    xdr_nfs_fh3, xdr_fastnfs_fh3, sizeof (PATHCONF3args),
	    xdr_PATHCONF3res, NULL_xdrproc_t, sizeof (PATHCONF3res),
	    nullfree, RPC_IDEMPOTENT,
	    rfs3_pathconf_getfh},

	/* RFS3_COMMIT = 21 */
	{rfs3_commit,
	    xdr_COMMIT3args, NULL_xdrproc_t, sizeof (COMMIT3args),
	    xdr_COMMIT3res, NULL_xdrproc_t, sizeof (COMMIT3res),
	    nullfree, RPC_IDEMPOTENT,
	    rfs3_commit_getfh},
};

union rfs_args {
	/*
	 * NFS VERSION 2
	 */

	/* RFS_NULL = 0 */

	/* RFS_GETATTR = 1 */
	fhandle_t nfs2_getattr_args;

	/* RFS_SETATTR = 2 */
	struct nfssaargs nfs2_setattr_args;

	/* RFS_ROOT = 3 *** NO LONGER SUPPORTED *** */

	/* RFS_LOOKUP = 4 */
	struct nfsdiropargs nfs2_lookup_args;

	/* RFS_READLINK = 5 */
	fhandle_t nfs2_readlink_args;

	/* RFS_READ = 6 */
	struct nfsreadargs nfs2_read_args;

	/* RFS_WRITECACHE = 7 *** NO LONGER SUPPORTED *** */

	/* RFS_WRITE = 8 */
	struct nfswriteargs nfs2_write_args;

	/* RFS_CREATE = 9 */
	struct nfscreatargs nfs2_create_args;

	/* RFS_REMOVE = 10 */
	struct nfsdiropargs nfs2_remove_args;

	/* RFS_RENAME = 11 */
	struct nfsrnmargs nfs2_rename_args;

	/* RFS_LINK = 12 */
	struct nfslinkargs nfs2_link_args;

	/* RFS_SYMLINK = 13 */
	struct nfsslargs nfs2_symlink_args;

	/* RFS_MKDIR = 14 */
	struct nfscreatargs nfs2_mkdir_args;

	/* RFS_RMDIR = 15 */
	struct nfsdiropargs nfs2_rmdir_args;

	/* RFS_READDIR = 16 */
	struct nfsrddirargs nfs2_readdir_args;

	/* RFS_STATFS = 17 */
	fhandle_t nfs2_statfs_args;

	/*
	 * NFS VERSION 3
	 */

	/* RFS_NULL = 0 */

	/* RFS3_GETATTR = 1 */
	GETATTR3args nfs3_getattr_args;

	/* RFS3_SETATTR = 2 */
	SETATTR3args nfs3_setattr_args;

	/* RFS3_LOOKUP = 3 */
	LOOKUP3args nfs3_lookup_args;

	/* RFS3_ACCESS = 4 */
	ACCESS3args nfs3_access_args;

	/* RFS3_READLINK = 5 */
	READLINK3args nfs3_readlink_args;

	/* RFS3_READ = 6 */
	READ3args nfs3_read_args;

	/* RFS3_WRITE = 7 */
	WRITE3args nfs3_write_args;

	/* RFS3_CREATE = 8 */
	CREATE3args nfs3_create_args;

	/* RFS3_MKDIR = 9 */
	MKDIR3args nfs3_mkdir_args;

	/* RFS3_SYMLINK = 10 */
	SYMLINK3args nfs3_symlink_args;

	/* RFS3_MKNOD = 11 */
	MKNOD3args nfs3_mknod_args;

	/* RFS3_REMOVE = 12 */
	REMOVE3args nfs3_remove_args;

	/* RFS3_RMDIR = 13 */
	RMDIR3args nfs3_rmdir_args;

	/* RFS3_RENAME = 14 */
	RENAME3args nfs3_rename_args;

	/* RFS3_LINK = 15 */
	LINK3args nfs3_link_args;

	/* RFS3_READDIR = 16 */
	READDIR3args nfs3_readdir_args;

	/* RFS3_READDIRPLUS = 17 */
	READDIRPLUS3args nfs3_readdirplus_args;

	/* RFS3_FSSTAT = 18 */
	FSSTAT3args nfs3_fsstat_args;

	/* RFS3_FSINFO = 19 */
	FSINFO3args nfs3_fsinfo_args;

	/* RFS3_PATHCONF = 20 */
	PATHCONF3args nfs3_pathconf_args;

	/* RFS3_COMMIT = 21 */
	COMMIT3args nfs3_commit_args;
};

union rfs_res {
	/*
	 * NFS VERSION 2
	 */

	/* RFS_NULL = 0 */

	/* RFS_GETATTR = 1 */
	struct nfsattrstat nfs2_getattr_res;

	/* RFS_SETATTR = 2 */
	struct nfsattrstat nfs2_setattr_res;

	/* RFS_ROOT = 3 *** NO LONGER SUPPORTED *** */

	/* RFS_LOOKUP = 4 */
	struct nfsdiropres nfs2_lookup_res;

	/* RFS_READLINK = 5 */
	struct nfsrdlnres nfs2_readlink_res;

	/* RFS_READ = 6 */
	struct nfsrdresult nfs2_read_res;

	/* RFS_WRITECACHE = 7 *** NO LONGER SUPPORTED *** */

	/* RFS_WRITE = 8 */
	struct nfsattrstat nfs2_write_res;

	/* RFS_CREATE = 9 */
	struct nfsdiropres nfs2_create_res;

	/* RFS_REMOVE = 10 */
	enum nfsstat nfs2_remove_res;

	/* RFS_RENAME = 11 */
	enum nfsstat nfs2_rename_res;

	/* RFS_LINK = 12 */
	enum nfsstat nfs2_link_res;

	/* RFS_SYMLINK = 13 */
	enum nfsstat nfs2_symlink_res;

	/* RFS_MKDIR = 14 */
	struct nfsdiropres nfs2_mkdir_res;

	/* RFS_RMDIR = 15 */
	enum nfsstat nfs2_rmdir_res;

	/* RFS_READDIR = 16 */
	struct nfsrddirres nfs2_readdir_res;

	/* RFS_STATFS = 17 */
	struct nfsstatfs nfs2_statfs_res;

	/*
	 * NFS VERSION 3
	 */

	/* RFS_NULL = 0 */

	/* RFS3_GETATTR = 1 */
	GETATTR3res nfs3_getattr_res;

	/* RFS3_SETATTR = 2 */
	SETATTR3res nfs3_setattr_res;

	/* RFS3_LOOKUP = 3 */
	LOOKUP3res nfs3_lookup_res;

	/* RFS3_ACCESS = 4 */
	ACCESS3res nfs3_access_res;

	/* RFS3_READLINK = 5 */
	READLINK3res nfs3_readlink_res;

	/* RFS3_READ = 6 */
	READ3res nfs3_read_res;

	/* RFS3_WRITE = 7 */
	WRITE3res nfs3_write_res;

	/* RFS3_CREATE = 8 */
	CREATE3res nfs3_create_res;

	/* RFS3_MKDIR = 9 */
	MKDIR3res nfs3_mkdir_res;

	/* RFS3_SYMLINK = 10 */
	SYMLINK3res nfs3_symlink_res;

	/* RFS3_MKNOD = 11 */
	MKNOD3res nfs3_mknod_res;

	/* RFS3_REMOVE = 12 */
	REMOVE3res nfs3_remove_res;

	/* RFS3_RMDIR = 13 */
	RMDIR3res nfs3_rmdir_res;

	/* RFS3_RENAME = 14 */
	RENAME3res nfs3_rename_res;

	/* RFS3_LINK = 15 */
	LINK3res nfs3_link_res;

	/* RFS3_READDIR = 16 */
	READDIR3res nfs3_readdir_res;

	/* RFS3_READDIRPLUS = 17 */
	READDIRPLUS3res nfs3_readdirplus_res;

	/* RFS3_FSSTAT = 18 */
	FSSTAT3res nfs3_fsstat_res;

	/* RFS3_FSINFO = 19 */
	FSINFO3res nfs3_fsinfo_res;

	/* RFS3_PATHCONF = 20 */
	PATHCONF3res nfs3_pathconf_res;

	/* RFS3_COMMIT = 21 */
	COMMIT3res nfs3_commit_res;
};

static struct rpc_disptable rfs_disptable[] = {
	{sizeof (rfsdisptab_v2) / sizeof (rfsdisptab_v2[0]),
#ifdef TRACE
	    rfscallnames_v2,
#endif
	    &rfsproccnt_v2_ptr, rfsdisptab_v2},
	{sizeof (rfsdisptab_v3) / sizeof (rfsdisptab_v3[0]),
#ifdef TRACE
	    rfscallnames_v3,
#endif
	    &rfsproccnt_v3_ptr, rfsdisptab_v3},
};

/*
 * If nfs_portmon is set, then clients are required to use privileged
 * ports (ports < IPPORT_RESERVED) in order to get NFS services.
 *
 * N.B.: this attempt to carry forward the already ill-conceived notion
 * of privileged ports for TCP/UDP is really quite ineffectual.  Not only
 * is it transport-dependent, it's laughably easy to spoof.  If you're
 * really interested in security, you must start with secure RPC instead.
 */
static int nfs_portmon = 0;

#ifdef TRACE
struct udp_data {
	uint32_t ud_xid;			/* id */
	mblk_t	*ud_resp;			/* buffer for response */
	mblk_t	*ud_inmp;			/* mblk chain of request */
	XDR	ud_xdrin;			/* input xdr stream */
	XDR	ud_xdrout;			/* output xdr stream */
};
#define	REQTOXID(req)   ((struct udp_data *)((req)->rq_xprt->xp_p2buf))->ud_xid
#endif

#ifdef DEBUG
static int cred_hits = 0;
static int cred_misses = 0;
#endif


#ifdef DEBUG
/*
 * Debug code to allow disabling of rfs_dispatch() use of
 * fastxdrargs() and fastxdrres() calls for testing purposes.
 */
static int rfs_no_fast_xdrargs = 0;
static int rfs_no_fast_xdrres = 0;
#endif

union acl_args {
	/*
	 * ACL VERSION 2
	 */

	/* ACL2_NULL = 0 */

	/* ACL2_GETACL = 1 */
	GETACL2args acl2_getacl_args;

	/* ACL2_SETACL = 2 */
	SETACL2args acl2_setacl_args;

	/* ACL2_GETATTR = 3 */
	GETATTR2args acl2_getattr_args;

	/* ACL2_ACCESS = 3 */
	ACCESS2args acl2_access_args;

	/*
	 * ACL VERSION 3
	 */

	/* ACL3_NULL = 0 */

	/* ACL3_GETACL = 1 */
	GETACL3args acl3_getacl_args;

	/* ACL3_SETACL = 2 */
	SETACL3args acl3_setacl;
};

union acl_res {
	/*
	 * ACL VERSION 2
	 */

	/* ACL2_NULL = 0 */

	/* ACL2_GETACL = 1 */
	GETACL2res acl2_getacl_res;

	/* ACL2_SETACL = 2 */
	SETACL2res acl2_setacl_res;

	/* ACL2_GETATTR = 3 */
	GETATTR2res acl2_getattr_res;

	/* ACL2_ACCESS = 3 */
	ACCESS2res acl2_access_res;

	/*
	 * ACL VERSION 3
	 */

	/* ACL3_NULL = 0 */

	/* ACL3_GETACL = 1 */
	GETACL3res acl3_getacl_res;

	/* ACL3_SETACL = 2 */
	SETACL3res acl3_setacl;
};

static bool_t
auth_tooweak(struct svc_req *req, char *res)
{

	if (req->rq_vers == NFS_VERSION && req->rq_proc == RFS_LOOKUP) {
		struct nfsdiropres *dr = (struct nfsdiropres *)res;
		if (dr->dr_status == WNFSERR_CLNT_FLAVOR)
			return (TRUE);
	} else if (req->rq_vers == NFS_V3 && req->rq_proc == NFSPROC3_LOOKUP) {
		LOOKUP3res *resp = (LOOKUP3res *)res;
		if (resp->status == WNFSERR_CLNT_FLAVOR)
			return (TRUE);
	}
	return (FALSE);
}

static void
common_dispatch(struct svc_req *req, SVCXPRT *xprt, rpcvers_t min_vers,
		rpcvers_t max_vers, char *pgmname,
		struct rpc_disptable *disptable)
{
	int which;
	rpcvers_t vers;
	char *args;
	union {
			union rfs_args ra;
			union acl_args aa;
		} args_buf;
	char *res;
	union {
			union rfs_res rr;
			union acl_res ar;
		} res_buf;
	struct rpcdisp *disp = NULL;
	cred_t *cr;
	int error = 0;
	int anon_ok;
	struct exportinfo *exi = NULL;
	unsigned int nfslog_rec_id;
	int dupstat;
	struct dupreq *dr;
	int authres;
	bool_t publicfh_ok = FALSE;
	enum_t auth_flavor;
	struct netbuf	nb;
	bool_t logging_enabled = FALSE;
	struct exportinfo *nfslog_exi = NULL;

	vers = req->rq_vers;
	if (vers < min_vers || vers > max_vers) {
		TRACE_3(TR_FAC_NFS, TR_CMN_DISPATCH_START,
			"common_dispatch_start:(%S) proc_num %d xid %x",
			"bad version", (int)vers, 0);
		svcerr_progvers(req->rq_xprt, min_vers, max_vers);
		error++;
		cmn_err(CE_NOTE, "%s: bad version number %u", pgmname, vers);
		goto done;
	}
	vers -= min_vers;

	which = req->rq_proc;
	if (which < 0 || which >= disptable[(int)vers].dis_nprocs) {
		TRACE_3(TR_FAC_NFS, TR_CMN_DISPATCH_START,
			"common_dispatch_start:(%S) proc_num %d xid %x",
			"bad proc", which, 0);
		svcerr_noproc(req->rq_xprt);
		error++;
		cmn_err(CE_NOTE, "%s: bad proc number %d", pgmname, which);
		goto done;
	}
	TRACE_3(TR_FAC_NFS, TR_CMN_DISPATCH_START,
		"common_dispatch_start:(%S) proc_num %d xid %x",
		disptable[(int)vers].dis_procnames[which], which,
		REQTOXID(req));

	(*(disptable[(int)vers].dis_proccntp))[which].value.ui64++;

	disp = &disptable[(int)vers].dis_table[which];

	auth_flavor = req->rq_cred.oa_flavor;
	/*
	 * Deserialize into the args struct.
	 */

	args = (char *)&args_buf;
	TRACE_0(TR_FAC_NFS, TR_SVC_GETARGS_START,
		"svc_getargs_start:");
#ifdef DEBUG
	if (rfs_no_fast_xdrargs || (auth_flavor == RPCSEC_GSS) ||
	    disp->dis_fastxdrargs == NULL_xdrproc_t ||
	    !SVC_GETARGS(xprt, disp->dis_fastxdrargs, (char *)&args)) {
#else
	if ((auth_flavor == RPCSEC_GSS) ||
	    disp->dis_fastxdrargs == NULL_xdrproc_t ||
	    !SVC_GETARGS(xprt, disp->dis_fastxdrargs, (char *)&args)) {
#endif
		bzero(args, disp->dis_argsz);
		if (!SVC_GETARGS(xprt, disp->dis_xdrargs, args)) {
			TRACE_1(TR_FAC_NFS, TR_SVC_GETARGS_END,
				"svc_getargs_end:(%S)", "bad");
			svcerr_decode(xprt);
			error++;
			cmn_err(CE_NOTE, "%s: bad getargs for %u/%d",
			    pgmname, vers + min_vers, which);
			goto done;
		}
	}
	TRACE_1(TR_FAC_NFS, TR_SVC_GETARGS_END,
		"svc_getargs_end:(%S)", "good");

	/*
	 * Find export information and check authentication,
	 * setting the credential if everything is ok.
	 */
	if (disp->dis_getfh != NULL) {
		fhandle_t *fh;

		fh = (*disp->dis_getfh)(args);

		/*
		 * Fix for bug 1038302 - corbin
		 * There is a problem here if anonymous access is
		 * disallowed.  If the current request is part of the
		 * client's mount process for the requested filesystem,
		 * then it will carry root (uid 0) credentials on it, and
		 * will be denied by checkauth if that client does not
		 * have explicit root=0 permission.  This will cause the
		 * client's mount operation to fail.  As a work-around,
		 * we check here to see if the request is a getattr or
		 * statfs operation on the exported vnode itself, and
		 * pass a flag to checkauth with the result of this test.
		 *
		 * The filehandle refers to the mountpoint itself if
		 * the fh_data and fh_xdata portions of the filehandle
		 * are equal.
		 *
		 * Added anon_ok argument to checkauth().
		 */

		if ((disp->dis_flags & RPC_ALLOWANON) &&
		    EQFID((fid_t *)&fh->fh_len, (fid_t *)&fh->fh_xlen))
			anon_ok = 1;
		else
			anon_ok = 0;

		cr = xprt->xp_cred;
		ASSERT(cr != NULL);
#ifdef DEBUG
		if (cr->cr_ref != 1) {
			crfree(cr);
			cr = crget();
			xprt->xp_cred = cr;
			cred_misses++;
		} else
			cred_hits++;
#else
		if (cr->cr_ref != 1) {
			crfree(cr);
			cr = crget();
			xprt->xp_cred = cr;
		}
#endif

		TRACE_0(TR_FAC_NFS, TR_FINDEXPORT_START,
			"findexport_start:");
		exi = findexport(&fh->fh_fsid, (fid_t *)&fh->fh_xlen);
		TRACE_0(TR_FAC_NFS, TR_FINDEXPORT_END,
			"findexport_end:");
		if (exi != NULL) {
			publicfh_ok = PUBLICFH_CHECK(disp, exi, fh);
			authres = checkauth(exi, req, cr, anon_ok, publicfh_ok);
			/*
			 * authres >  0: authentication OK - proceed
			 * authres == 0: authentication weak - return error
			 * authres <  0: authentication timeout - drop
			 */
			if (authres <= 0) {
				if (authres == 0) {
					svcerr_weakauth(xprt);
					error++;
				}

				goto done;
			}
		}
	} else
		cr = NULL;

	if ((disp->dis_flags & RPC_MAPRESP) && (auth_flavor != RPCSEC_GSS)) {
		res = (char *)SVC_GETRES(xprt, disp->dis_ressz);
		if (res == NULL)
			res = (char *)&res_buf;
	} else
		res = (char *)&res_buf;

	if (!(disp->dis_flags & RPC_IDEMPOTENT)) {
		dupstat = SVC_DUP(xprt, req, res, disp->dis_ressz, &dr);

		switch (dupstat) {
		case DUP_ERROR:
			svcerr_systemerr(xprt);
			error++;
			goto done;
			/* NOTREACHED */
		case DUP_INPROGRESS:
			if (res != (char *)&res_buf)
				SVC_FREERES(xprt);
			error++;
			goto done;
			/* NOTREACHED */
		case DUP_NEW:
		case DUP_DROP:
			curthread->t_flag |= T_DONTPEND;
			TRACE_3(TR_FAC_NFS, TR_CMN_PROC_START,
				"cmn_proc_start:(%S) proc_num %d xid %x",
				disptable[(int)vers].dis_procnames[which],
				which, REQTOXID(req));
			(*disp->dis_proc)(args, res, exi, req, cr);
			TRACE_0(TR_FAC_NFS, TR_CMN_PROC_END,
				"cmn_proc_end:");
			curthread->t_flag &= ~T_DONTPEND;
			if (curthread->t_flag & T_WOULDBLOCK) {
				curthread->t_flag &= ~T_WOULDBLOCK;
				SVC_DUPDONE(xprt, dr, res, disp->dis_ressz,
				    DUP_DROP);
				if (res != (char *)&res_buf)
					SVC_FREERES(xprt);
				error++;
				goto done;
			}
			if (disp->dis_flags & RPC_AVOIDWORK) {
				SVC_DUPDONE(xprt, dr, res, disp->dis_ressz,
				    DUP_DROP);
			} else {
				SVC_DUPDONE(xprt, dr, res, disp->dis_ressz,
				    DUP_DONE);
			}
			break;
		case DUP_DONE:
			break;
		}

	} else {
		curthread->t_flag |= T_DONTPEND;
		TRACE_3(TR_FAC_NFS, TR_CMN_PROC_START,
			"cmn__proc_start:(%S) proc_num %d xid %x",
			disptable[(int)vers].dis_procnames[which], which,
			REQTOXID(req));
		(*disp->dis_proc)(args, res, exi, req, cr);
		TRACE_0(TR_FAC_NFS, TR_CMN_PROC_END,
			"cmn_proc_end:");
		curthread->t_flag &= ~T_DONTPEND;
		if (curthread->t_flag & T_WOULDBLOCK) {
			curthread->t_flag &= ~T_WOULDBLOCK;
			if (res != (char *)&res_buf)
				SVC_FREERES(xprt);
			error++;
			goto done;
		}
	}

	if (auth_tooweak(req, res)) {
		svcerr_weakauth(xprt);
		error++;
		goto done;
	}

	/*
	 * Check to see if logging has been enabled on the server.
	 * If so, then obtain the export info struct to be used for
	 * the later writing of the log record.  This is done for
	 * the case that a lookup is done across a non-logged public
	 * file system.
	 */
	if (nfslog_buffer_list != NULL) {
		nfslog_exi = nfslog_get_exi(exi, req, res, &nfslog_rec_id);
		/*
		 * Is logging enabled?
		 */
		logging_enabled = (nfslog_exi != NULL);

		/*
		 * Copy the netbuf for logging purposes, before it is
		 * freed by svc_sendreply().
		 */
		if (logging_enabled) {
			NFSLOG_COPY_NETBUF(nfslog_exi, xprt, &nb);
			/*
			 * If RPC_MAPRESP flag set (i.e. in V2 ops) the
			 * res gets copied directly into the mbuf and
			 * may be freed soon after the sendreply. So we
			 * must copy it here to a safe place...
			 */
			if (res != (char *)&res_buf) {
				bcopy(res, (char *)&res_buf, disp->dis_ressz);
			}
		}
	}

	/*
	 * Serialize and send results struct
	 */
	TRACE_0(TR_FAC_NFS, TR_SVC_SENDREPLY_START,
		"svc_sendreply_start:");
#ifdef DEBUG
	if (rfs_no_fast_xdrres == 0 && res != (char *)&res_buf) {
#else
	if (res != (char *)&res_buf) {
#endif
		if (!svc_sendreply(xprt, disp->dis_fastxdrres, res)) {
			cmn_err(CE_NOTE, "%s: bad sendreply", pgmname);
			error++;
		}
	} else {
		if (!svc_sendreply(xprt, disp->dis_xdrres, res)) {
			cmn_err(CE_NOTE, "%s: bad sendreply", pgmname);
			error++;
		}
	}
	TRACE_0(TR_FAC_NFS, TR_SVC_SENDREPLY_END,
		"svc_sendreply_end:");

	/*
	 * Log if needed
	 */
	if (logging_enabled) {
		nfslog_write_record(nfslog_exi, req, args, (char *)&res_buf,
			cr, &nb, nfslog_rec_id, NFSLOG_ONE_BUFFER);
		kmem_free((&nb)->buf, (&nb)->len);
	}

	/*
	 * Free results struct
	 */
	if (disp->dis_resfree != nullfree) {
		TRACE_0(TR_FAC_NFS, TR_SVC_FREERES_START,
			"svc_freeres_start:");
		(*disp->dis_resfree)(res);
		TRACE_0(TR_FAC_NFS, TR_SVC_FREERES_END,
			"svc_freeres_end:");
	}

done:
	/*
	 * Free arguments struct
	 */
	TRACE_0(TR_FAC_NFS, TR_SVC_FREEARGS_START,
		"svc_freeargs_start:");
	if (disp) {
		if (!SVC_FREEARGS(xprt, disp->dis_xdrargs, args)) {
			cmn_err(CE_NOTE, "%s: bad freeargs", pgmname);
			error++;
		}
	} else {
		if (!SVC_FREEARGS(xprt, (xdrproc_t)0, (caddr_t)0)) {
			cmn_err(CE_NOTE, "%s: bad freeargs", pgmname);
			error++;
		}
	}

	TRACE_0(TR_FAC_NFS, TR_SVC_FREEARGS_END,
		"svc_freeargs_end:");

	if (exi != NULL)
		export_rw_exit();

	svstat_ptr[NFS_BADCALLS].value.ui64 += error;
	svstat_ptr[NFS_CALLS].value.ui64++;


	TRACE_1(TR_FAC_NFS, TR_CMN_DISPATCH_END,
		"common_dispatch_end:proc_num %d",
		which);
}

static void
rfs_dispatch(struct svc_req *req, SVCXPRT *xprt)
{
	common_dispatch(req, xprt, NFS_VERSMIN, NFS_VERSMAX,
		"nfs_server", rfs_disptable);
}

#ifdef TRACE
static char *aclcallnames_v2[] = {
	"ACL2_NULL",
	"ACL2_GETACL",
	"ACL2_SETACL",
	"ACL2_GETATTR",
	"ACL2_ACCESS"
};
#endif

static struct rpcdisp acldisptab_v2[] = {
	/*
	 * ACL VERSION 2
	 */

	/* ACL2_NULL = 0 */
	{rpc_null,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* ACL2_GETACL = 1 */
	{acl2_getacl,
	    xdr_GETACL2args, xdr_fastGETACL2args, sizeof (GETACL2args),
	    xdr_GETACL2res, NULL_xdrproc_t, sizeof (GETACL2res),
	    acl2_getacl_free, RPC_IDEMPOTENT,
	    acl2_getacl_getfh},

	/* ACL2_SETACL = 2 */
	{acl2_setacl,
	    xdr_SETACL2args, NULL_xdrproc_t, sizeof (SETACL2args),
#ifdef _LITTLE_ENDIAN
	    xdr_SETACL2res, xdr_fastSETACL2res, sizeof (SETACL2res),
#else
	    xdr_SETACL2res, NULL_xdrproc_t, sizeof (SETACL2res),
#endif
	    nullfree, RPC_MAPRESP,
	    acl2_setacl_getfh},

	/* ACL2_GETATTR = 3 */
	{acl2_getattr,
	    xdr_GETATTR2args, xdr_fastGETATTR2args, sizeof (GETATTR2args),
#ifdef _LITTLE_ENDIAN
	    xdr_GETATTR2res, xdr_fastGETATTR2res, sizeof (GETATTR2res),
#else
	    xdr_GETATTR2res, NULL_xdrproc_t, sizeof (GETATTR2res),
#endif
	    nullfree, RPC_IDEMPOTENT|RPC_ALLOWANON|RPC_MAPRESP,
	    acl2_getattr_getfh},

	/* ACL2_ACCESS = 4 */
	{acl2_access,
	    xdr_ACCESS2args, xdr_fastACCESS2args, sizeof (ACCESS2args),
#ifdef _LITTLE_ENDIAN
	    xdr_ACCESS2res, xdr_fastACCESS2res, sizeof (ACCESS2res),
#else
	    xdr_ACCESS2res, NULL_xdrproc_t, sizeof (ACCESS2res),
#endif
	    nullfree, RPC_IDEMPOTENT|RPC_MAPRESP,
	    acl2_access_getfh},
};

#ifdef TRACE
static char *aclcallnames_v3[] = {
	"ACL3_NULL",
	"ACL3_GETACL",
	"ACL3_SETACL"
};
#endif

static struct rpcdisp acldisptab_v3[] = {
	/*
	 * ACL VERSION 3
	 */

	/* ACL3_NULL = 0 */
	{rpc_null,
	    xdr_void, NULL_xdrproc_t, 0,
	    xdr_void, NULL_xdrproc_t, 0,
	    nullfree, RPC_IDEMPOTENT,
	    0},

	/* ACL3_GETACL = 1 */
	{acl3_getacl,
	    xdr_GETACL3args, NULL_xdrproc_t, sizeof (GETACL3args),
	    xdr_GETACL3res, NULL_xdrproc_t, sizeof (GETACL3res),
	    acl3_getacl_free, RPC_IDEMPOTENT,
	    acl3_getacl_getfh},

	/* ACL3_SETACL = 2 */
	{acl3_setacl,
	    xdr_SETACL3args, NULL_xdrproc_t, sizeof (SETACL3args),
	    xdr_SETACL3res, NULL_xdrproc_t, sizeof (SETACL3res),
	    nullfree, 0,
	    acl3_setacl_getfh},
};

static struct rpc_disptable acl_disptable[] = {
	{sizeof (acldisptab_v2) / sizeof (acldisptab_v2[0]),
#ifdef TRACE
		aclcallnames_v2,
#endif
		&aclproccnt_v2_ptr, acldisptab_v2},
	{sizeof (acldisptab_v3) / sizeof (acldisptab_v3[0]),
#ifdef TRACE
		aclcallnames_v3,
#endif
		&aclproccnt_v3_ptr, acldisptab_v3},
};

static void
acl_dispatch(struct svc_req *req, SVCXPRT *xprt)
{
	common_dispatch(req, xprt, NFS_ACL_VERSMIN, NFS_ACL_VERSMAX,
		"acl_server", acl_disptable);
}

int
checkwin(int flavor, int window, struct svc_req *req)
{
	struct authdes_cred *adc;

	switch (flavor) {
	case AUTH_DES:
		adc = (struct authdes_cred *)req->rq_clntcred;
		if (adc->adc_fullname.window > window)
			return (0);
		break;

	default:
		break;
	}
	return (1);
}


/*
 * This routine is now based on the new design framework from NFS/KerbV5
 * phase 1 project.  Case: PSARC/1994/364
 *
 * checkauth() will check the access permission against the export
 * information.  Then map root uid/gid to appropriate uid/gid.
 */


static int
checkauth(struct exportinfo *exi, struct svc_req *req, cred_t *cr, int anon_ok,
    bool_t publicfh_ok)
{
	int i, nfsflavor, rpcflavor, stat, access;
	short grouplen;
	struct secinfo *secp;
	caddr_t principal;
	char buf[INET6_ADDRSTRLEN]; /* to hold both IPv4 and IPv6 addr */

	/*
	 *	Check for privileged port number
	 *	N.B.:  this assumes that we know the format of a netbuf.
	 */
	if (nfs_portmon) {
		struct sockaddr *ca;
		ca = (struct sockaddr *)svc_getrpccaller(req->rq_xprt)->buf;

		if ((ca->sa_family == AF_INET &&
		    ntohs(((struct sockaddr_in *)ca)->sin_port) >=
		    IPPORT_RESERVED) ||
		    (ca->sa_family == AF_INET6 &&
		    ntohs(((struct sockaddr_in6 *)ca)->sin6_port) >=
		    IPPORT_RESERVED)) {
			cmn_err(CE_NOTE,
			    "nfs_server: client %s%ssent NFS request from "
			    "unprivileged port",
			    client_name(req), client_addr(req, buf));
			return (0);
		}
	}

	/*
	 *  return 1 on success or 0 on failure
	 */
	stat = sec_svc_getcred(req, &cr->cr_uid, &cr->cr_gid,
	    &grouplen, (gid_t *)&cr->cr_groups[0],
	    &principal, &nfsflavor);

	/*
	 * Short circuit checkauth() on operations that support the
	 * public filehandle, and if the request for that operation
	 * is using the public filehandle. Note that we must call
	 * sec_svc_getcred() first so that xp_cookie is set to the
	 * right value. Normally xp_cookie is just the RPC flavor
	 * of the the request, but in the case of RPCSEC_GSS it
	 * could be a pseudo flavor.
	 */
	if (publicfh_ok)
		return (1);

	rpcflavor = req->rq_cred.oa_flavor;
	/*
	 * Check if the auth flavor is valid for this export
	 */
	access = nfsauth_access(exi, req);
	if (access & NFSAUTH_DROP)
		return (-1);	/* drop the request */

	if (access & NFSAUTH_DENIED) {
		/*
		 * If anon_ok == 1 and we got NFSAUTH_DENIED, it was
		 * probably due to the flavor not matching during the
		 * the mount attempt. So map the flavor to AUTH_NONE
		 * so that the credentials get mapped to the anonymous
		 * user.
		 */
		if (anon_ok == 1)
			rpcflavor = AUTH_NONE;
		else
			return (0);	/* deny access */

	} else if (access & NFSAUTH_MAPNONE) {
		/*
		 * Access was granted even though the flavor mismatched
		 * because AUTH_NONE was one of the exported flavors.
		 */
		rpcflavor = AUTH_NONE;
	}


	switch (rpcflavor) {
	case AUTH_NONE:
		cr->cr_uid = exi->exi_export.ex_anon;
		cr->cr_gid = exi->exi_export.ex_anon;
		cr->cr_ngroups = 0;
		break;

	case AUTH_UNIX:
		if (stat && (cr->cr_uid == 0) && !(access & NFSAUTH_ROOT)) {
			cr->cr_uid = exi->exi_export.ex_anon;
			cr->cr_gid = exi->exi_export.ex_anon;
			cr->cr_ngroups = 0;
		} else {
			cr->cr_ngroups = grouplen;
		}
		break;

	default:
		/*
		 *  Find the secinfo structure.  We should be able
		 *  to find it by the time we reach here.
		 *  nfsauth_access() has done the checking.
		 */
		secp = NULL;
		for (i = 0; i < exi->exi_export.ex_seccnt; i++) {
			if (exi->exi_export.ex_secinfo[i].s_secinfo.sc_nfsnum ==
			    nfsflavor) {
				secp = &exi->exi_export.ex_secinfo[i];
				break;
			}
		}

		if (!secp) {
			cmn_err(CE_NOTE, "nfs_server: client %s%shad "
			    "no secinfo data for flavor %d",
			    client_name(req), client_addr(req, buf),
			    nfsflavor);
			return (0);
		}

		if (!checkwin(rpcflavor, secp->s_window, req)) {
			cmn_err(CE_NOTE,
			    "nfs_server: client %s%sused invalid "
			    "auth window value",
			    client_name(req), client_addr(req, buf));
			return (0);
		}

		/*
		 *  XXX this is from old semantics: sec_svc_getcred returns
		 *  0 if the request is coming from a root user.
		 *  Waiting for bug 1180912 to be fixed in order to redo
		 *  the logic.
		 */
		if (!stat) {
			if (principal && sec_svc_inrootlist(rpcflavor,
			    principal, secp->s_rootcnt, secp->s_rootnames)) {
				cr->cr_uid = 0;
			} else {
				cr->cr_uid = exi->exi_export.ex_anon;
			}
			cr->cr_gid = exi->exi_export.ex_anon;
			grouplen = 0;
		} else if ((cr->cr_uid == 0) && principal &&
		    !sec_svc_inrootlist(rpcflavor, principal,
		    secp->s_rootcnt, secp->s_rootnames)) {
			cr->cr_uid = cr->cr_gid = exi->exi_export.ex_anon;
			grouplen = 0;
		}
		cr->cr_ngroups = grouplen;
		break;
	} /* switch on rpcflavor */

	/*
	 * Even if anon access is disallowed via ex_anon == -1, we allow
	 * this access if anon_ok is set.  So set creds to the default
	 * "nobody" id.
	 */
	if (cr->cr_uid == (uid_t)-1) {
		if (anon_ok == 0) {
			cmn_err(CE_NOTE,
			    "nfs_server: client %s%ssent wrong "
			    "authentication for %s",
			    client_name(req), client_addr(req, buf),
			    exi->exi_export.ex_path ?
			    exi->exi_export.ex_path : "?");
			return (0);
		}

		cr->cr_uid = UID_NOBODY;
		cr->cr_gid = GID_NOBODY;
	}

	/*
	 * Set real UID/GID to effective UID/GID
	 * corbin 6/19/90 - Fix bug 1029628
	 */
	cr->cr_ruid = cr->cr_uid;
	cr->cr_rgid = cr->cr_gid;

	return (1);
}

static char *
client_name(struct svc_req *req)
{
	char *hostname = NULL;

	/*
	 * If it's a Unix cred then use the
	 * hostname from the credential.
	 */
	if (req->rq_cred.oa_flavor == AUTH_UNIX) {
		hostname = ((struct authunix_parms *)
		    req->rq_clntcred)->aup_machname;
	}
	if (hostname == NULL)
		hostname = "";

	return (hostname);
}

static char *
client_addr(struct svc_req *req, char *buf)
{
	struct sockaddr *ca;
	uchar_t *b;
	char *frontspace = "";

	/*
	 * We assume we are called in tandem with client_name and the
	 * format string looks like "...client %s%sblah blah..."
	 *
	 * If it's a Unix cred then client_name returned
	 * a host name, so we need insert a space between host name
	 * and IP address.
	 */
	if (req->rq_cred.oa_flavor == AUTH_UNIX)
		frontspace = " ";

	/*
	 * Convert the caller's IP address to a dotted string
	 */
	ca = (struct sockaddr *)svc_getrpccaller(req->rq_xprt)->buf;

	if (ca->sa_family == AF_INET) {
	    b = (uchar_t *)&((struct sockaddr_in *)ca)->sin_addr;
	    (void) sprintf(buf, "%s(%d.%d.%d.%d) ", frontspace,
		b[0] & 0xFF, b[1] & 0xFF, b[2] & 0xFF, b[3] & 0xFF);
	} else if (ca->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)ca;
		(void) kinet_ntop6((uchar_t *)&sin6->sin6_addr,
				buf, INET6_ADDRSTRLEN);

	} else {

		/*
		 * No IP address to print. If there was a host name
		 * printed, then we print a space.
		 */
		(void) sprintf(buf, frontspace);
	}

	return (buf);
}

/*
 * NFS Server initialization routine.  This routine should only be called
 * once.  It performs the following tasks:
 *	- Call sub-initialization routines (localize access to variables)
 *	- Initialize all locks
 *	- initialize the version 3 write verifier
 */
int
nfs_srvinit(void)
{
	int error;

	error = nfs_exportinit();
	if (error != 0)
		return (error);

	rfs_srvrinit();
	rfs3_srvrinit();
	nfsauth_init();

	return (0);
}

/*
 * NFS Server finalization routine. This routine is called to cleanup the
 * initialization work previously performed if the NFS server module could
 * not be loaded correctly.
 */
void
nfs_srvfini(void)
{
	nfsauth_fini();
	rfs3_srvrfini();
	rfs_srvrfini();
	nfs_exportfini();
}

void
mblk_to_iov(mblk_t *m, struct iovec *iovp)
{
	while (m != NULL) {
		iovp->iov_base = (caddr_t)m->b_rptr;
		iovp->iov_len = (m->b_wptr - m->b_rptr);
		iovp++;
		m = m->b_cont;
	}
}

/*
 * Common code between NFS Version 2 and NFS Version 3 for the public
 * filehandle multicomponent lookups.
 */

/*
 * Public filehandle evaluation of a multi-component lookup, following
 * symbolic links, if necessary. This may result in a vnode in another
 * filesystem, which is OK as long as the other filesystem is exported.
 */
int
rfs_publicfh_mclookup(char *p, vnode_t *dvp, cred_t *cr, vnode_t **vpp,
    struct exportinfo **exi, struct sec_ol *sec)
{
	int pathflag;
	vnode_t *mc_dvp = NULL;
	vnode_t *realvp;
	int error;

	/*
	 * check if the given path is a url or native path. Since p is
	 * modified by MCLpath(), it may be empty after returning from
	 * there, and should be checked.
	 */
	if ((pathflag = MCLpath(&p)) == -1)
		return (EIO);

	/*
	 * If pathflag is SECURITY_QUERY, turn the SEC_QUERY bit
	 * on in sec->sec_flags. This bit will later serve as an
	 * indication in makefh_ol() or makefh3_ol() to overload the
	 * filehandle to contain the sec modes used by the server for
	 * the path.
	 */
	if (pathflag == SECURITY_QUERY) {
		if ((sec->sec_index = (uint_t)(*p)) > 0) {
			sec->sec_flags |= SEC_QUERY;
			p++;
			if ((pathflag = MCLpath(&p)) == -1)
				return (EIO);
		} else {
			cmn_err(CE_NOTE,
			    "nfs_server: invalid security index %d, "
			    "violating WebNFS SNEGO protocol.", sec->sec_index);
			return (EIO);
		}
	}

	if (p[0] == '\0') {
		error = ENOENT;
		goto publicfh_done;
	}

	error = rfs_pathname(p, &mc_dvp, vpp, dvp, cr, pathflag);

	/*
	 * If name resolves to "/" we get EINVAL since we asked for
	 * the vnode of the directory that the file is in. Try again
	 * with NULL directory vnode.
	 */
	if (error == EINVAL) {
		error = rfs_pathname(p, NULL, vpp, dvp, cr, pathflag);
		if (!error) {
			ASSERT(*vpp != NULL);
			if ((*vpp)->v_type == VDIR) {
				VN_HOLD(*vpp);
				mc_dvp = *vpp;
			} else {
				/*
				 * This should not happen, the filesystem is
				 * in an inconsistent state. Fail the lookup
				 * at this point.
				 */
				VN_RELE(*vpp);
				error = EINVAL;
			}
		}
	}

	if (error)
		goto publicfh_done;

	if (*vpp == NULL) {
		error = ENOENT;
		goto publicfh_done;
	}

	ASSERT(mc_dvp != NULL);
	ASSERT(*vpp != NULL);

	if ((*vpp)->v_type == VDIR) {
		do {
			/*
			 * *vpp may be an AutoFS node, so we perform
			 * a VOP_ACCESS() to trigger the mount of the intended
			 * filesystem, so we can perform the lookup in the
			 * intended filesystem.
			 */
			(void) VOP_ACCESS(*vpp, 0, 0, cr);

			/*
			 * If vnode is covered, lets get the
			 * the topmost vnode.
			 */
			if ((*vpp)->v_vfsmountedhere != NULL) {
				error = traverse(vpp);
				if (error) {
					VN_RELE(*vpp);
					goto publicfh_done;
				}
			}

			if (VOP_REALVP(*vpp, &realvp) == 0 && realvp != *vpp) {
				/*
				 * If realvp is different from *vpp
				 * then release our reference on *vpp, so that
				 * the export access check be performed on the
				 * real filesystem instead.
				 */
				VN_HOLD(realvp);
				VN_RELE(*vpp);
				*vpp = realvp;
			} else
			    break;
		/* LINTED */
		} while (TRUE);

		/*
		 * Let nfs_vptexi() figure what the real parent is.
		 */
		VN_RELE(mc_dvp);
		mc_dvp = NULL;

	} else {
		/*
		 * If vnode is covered, lets get the
		 * the topmost vnode.
		 */
		if (mc_dvp->v_vfsmountedhere != NULL) {
			error = traverse(&mc_dvp);
			if (error) {
			    VN_RELE(*vpp);
			    goto publicfh_done;
			}
		}

		if (VOP_REALVP(mc_dvp, &realvp) == 0 && realvp != mc_dvp) {
			/*
			 * *vpp is a file, obtain realvp of the parent
			 * directory vnode.
			 */
			VN_HOLD(realvp);
			VN_RELE(mc_dvp);
			mc_dvp = realvp;
		}
	}

	/*
	 * The pathname may take us from the public filesystem to another.
	 * If that's the case then just set the exportinfo to the new export
	 * and build filehandle for it. Thanks to per-access checking there's
	 * no security issues with doing this. If the client is not allowed
	 * access to this new export then it will get an access error when it
	 * tries to use the filehandle
	 */
	if (error = nfs_check_vpexi(mc_dvp, *vpp, kcred, exi)) {
		VN_RELE(*vpp);
		goto publicfh_done;
	}

	/*
	 * Do a lookup for the index file. We know the index option doesn't
	 * allow paths through handling in the share command, so mc_dvp will
	 * be the parent for the index file vnode, if its present. Use
	 * temporary pointers to preserve and reuse the vnode pointers of the
	 * original directory in case there's no index file. Note that the
	 * index file is a native path, and should not be interpreted by
	 * the URL parser in rfs_pathname()
	 */
	if (((*exi)->exi_export.ex_flags & EX_INDEX) &&
	    ((*vpp)->v_type == VDIR) && (pathflag == URLPATH)) {
		vnode_t *tvp, *tmc_dvp;	/* temporary vnode pointers */

		tmc_dvp = mc_dvp;
		mc_dvp = tvp = *vpp;

		error = rfs_pathname((*exi)->exi_export.ex_index, NULL, vpp,
		    mc_dvp, cr, NATIVEPATH);

		if (error == ENOENT) {
			*vpp = tvp;
			mc_dvp = tmc_dvp;
			error = 0;
		} else {	/* ok or error other than ENOENT */
			if (tmc_dvp)
				VN_RELE(tmc_dvp);
			if (error)
				goto publicfh_done;

			/*
			 * Found a valid vp for index "filename". Sanity check
			 * for odd case where a directory is provided as index
			 * option argument and leads us to another filesystem
			 */
			if (error = nfs_check_vpexi(mc_dvp, *vpp, kcred, exi)) {
				VN_RELE(*vpp);
				goto publicfh_done;
			}
		}
	}

publicfh_done:
	if (mc_dvp)
		VN_RELE(mc_dvp);

	return (error);
}

/*
 * Very rarely are pathnames > 64 bytes, hence allocate space on
 * the stack for that rather then kmem_alloc it.
 */

#define	TYPICALMAXPATHLEN	64

/*
 * Evaluate a multi-component path
 */
int
rfs_pathname(
	char *path,			/* pathname to evaluate */
	vnode_t **dirvpp,		/* ret for ptr to parent dir vnode */
	vnode_t **compvpp,		/* ret for ptr to component vnode */
	vnode_t *startdvp,		/* starting vnode */
	cred_t *cr,			/* user's credential */
	int pathflag)			/* flag to identify path, e.g. URL */
{
	char namebuf[TYPICALMAXPATHLEN + 4]; /* +4 because of bug 1170077 */
	struct pathname pn;
	int error;

	/*
	 * If pathname starts with '/', then set startdvp to root.
	 */
	if (*path == '/') {
		while (*path == '/')
			path++;

		startdvp = rootdir;
	}

	pn.pn_buf = namebuf;
	pn.pn_path = namebuf;
	pn.pn_pathlen = 0;
	pn.pn_bufsize = TYPICALMAXPATHLEN;

	error = copystr(path, namebuf, TYPICALMAXPATHLEN, &pn.pn_pathlen);
	pn.pn_pathlen--; 		/* don't count the null byte */

	if (error == 0) {
		/*
		 * Call the URL parser for URL paths to modify the original
		 * string to handle any '%' encoded characters that exist.
		 * Done here to avoid an extra bcopy in the lookup.
		 * We need to be careful about pathlen's. We know that
		 * rfs_pathname() is called with a non-empty path. However,
		 * it could be emptied due to the path simply being all /'s,
		 * which is valid to proceed with the lookup, or due to the
		 * URL parser finding an encoded null character at the
		 * beginning of path which should not proceed with the lookup.
		 */
		if (pn.pn_pathlen != 0 && pathflag == URLPATH) {
			URLparse(namebuf);
			if ((pn.pn_pathlen = strlen(namebuf)) == 0)
				return (ENOENT);
		}
		VN_HOLD(startdvp);
		error = lookuppnvp(&pn, NULL, NO_FOLLOW, dirvpp, compvpp,
		    rootdir, startdvp, cr);
	}
	if (error == ENAMETOOLONG) {
		/*
		 * This thread used a pathname > TYPICALMAXPATHLEN
		 * bytes long.
		 */
		if (error = pn_get(path, UIO_SYSSPACE, &pn))
			return (error);
		if (pn.pn_pathlen != 0 && pathflag == URLPATH) {
			/*
			 * XXX For URL paths, we muck with the string
			 * allocated in pn_get() and rely on the pn_buf and
			 * pn_path pointing to the same place, and on the
			 * current pn_free() routine * freeing exactly the
			 * same MAXPATHLEN as was allocated in pn_get()
			 */
			URLparse(pn.pn_buf);
			if ((pn.pn_pathlen = strlen(pn.pn_buf)) == 0)
				return (ENOENT);
		}
		VN_HOLD(startdvp);
		error = lookuppnvp(&pn, NULL, NO_FOLLOW, dirvpp, compvpp,
		    rootdir, startdvp, cr);
		pn_free(&pn);
	}

	return (error);
}

/*
 * Adapt the multicomponent lookup path depending on the pathtype
 */
static int
MCLpath(char **path)
{
	unsigned char c = (unsigned char)**path;

	/*
	 * If the MCL path is between 0x20 and 0x7E (graphic printable
	 * character of the US-ASCII coded character set), its a URL path,
	 * per RFC 1738.
	 */
	if (c >= 0x20 && c <= 0x7E)
		return (URLPATH);

	/*
	 * If the first octet of the MCL path is not an ASCII character
	 * then it must be interpreted as a tag value that describes the
	 * format of the remaining octets of the MCL path.
	 *
	 * If the first octet of the MCL path is 0x81 it is a query
	 * for the security info.
	 */
	switch (c) {
	case 0x80:	/* native path, i.e. MCL via mount protocol */
		(*path)++;
		return (NATIVEPATH);
	case 0x81:	/* security query */
		(*path)++;
		return (SECURITY_QUERY);
	default:
		return (-1);
	}
}

#define	fromhex(c)  ((c >= '0' && c <= '9') ? (c - '0') : \
			((c >= 'A' && c <= 'F') ? (c - 'A' + 10) :\
			((c >= 'a' && c <= 'f') ? (c - 'a' + 10) : 0)))

/*
 * The implementation of URLparse gaurantees that the final string will
 * fit in the original one. Replaces '%' occurrences followed by 2 characters
 * with its corresponding hexadecimal character.
 */
static void
URLparse(char *str)
{
	char *p, *q;

	p = q = str;
	while (*p) {
		*q = *p;
		if (*p++ == '%') {
			if (*p) {
				*q = fromhex(*p) * 16;
				p++;
				if (*p) {
					*q += fromhex(*p);
					p++;
				}
			}
		}
		q++;
	}
	*q = '\0';
}


/*
 * Get the export information for the lookup vnode, and verify its
 * useable.
 */
static int
nfs_check_vpexi(vnode_t *mc_dvp, vnode_t *vp, cred_t *cr,
    struct exportinfo **exi)
{
	int walk;
	int error = 0;

	*exi = nfs_vptoexi(mc_dvp, vp, cr, &walk);
	if (*exi == NULL)
		error = EACCES;
	else {
		/*
		 * If nosub is set for this export then
		 * a lookup relative to the public fh
		 * must not terminate below the
		 * exported directory.
		 */
		if ((*exi)->exi_export.ex_flags & EX_NOSUB && walk > 0)
			error = EACCES;
	}

	return (error);
}
