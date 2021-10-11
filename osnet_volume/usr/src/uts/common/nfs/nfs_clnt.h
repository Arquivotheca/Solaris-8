/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	Copyright (c) 1986-1991,1996-1997,1999 by Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef	_NFS_NFS_CLNT_H
#define	_NFS_NFS_CLNT_H

#pragma ident	"@(#)nfs_clnt.h	1.65	99/09/29 SMI"
/* SVr4.0 1.8	*/
/*	nfs_clnt.h 2.28 88/08/19 SMI	*/

#include <sys/utsname.h>
#include <sys/kstat.h>
#include <vm/page.h>
#include <sys/thread.h>
#include <nfs/rnode.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	HOSTNAMESZ	32
#define	ACREGMIN	3	/* min secs to hold cached file attr */
#define	ACREGMAX	60	/* max secs to hold cached file attr */
#define	ACDIRMIN	30	/* min secs to hold cached dir attr */
#define	ACDIRMAX	60	/* max secs to hold cached dir attr */
#define	ACMINMAX	3600	/* 1 hr is longest min timeout */
#define	ACMAXMAX	36000	/* 10 hr is longest max timeout */

#define	NFS_CALLTYPES	3	/* Lookups, Reads, Writes */

/*
 * rfscall() flags
 */
#define	RFSCALL_SOFT	0x00000001	/* Do op as if fs was soft-mounted */

/*
 * Fake errno passed back from rfscall to indicate transfer size adjustment
 */
#define	ENFS_TRYAGAIN	999

/*
 * The NFS specific async_reqs structure.
 */

enum iotype {
	NFS_READ_AHEAD,
	NFS_PUTAPAGE,
	NFS_PAGEIO,
	NFS_READDIR,
	NFS_COMMIT
};
#define	NFS_ASYNC_TYPES	(NFS_COMMIT + 1)

struct nfs_async_read_req {
	void (*readahead)();		/* pointer to readahead function */
	u_offset_t blkoff;		/* offset in file */
	struct seg *seg;		/* segment to do i/o to */
	caddr_t addr;			/* address to do i/o to */
};

struct nfs_pageio_req {
	int (*pageio)();		/* pointer to pageio function */
	page_t *pp;			/* page list */
	u_offset_t io_off;		/* offset in file */
	uint_t io_len;			/* size of request */
	int flags;
};

struct nfs_readdir_req {
	int (*readdir)();		/* pointer to readdir function */
	struct rddir_cache *rdc;	/* pointer to cache entry to fill */
};

struct nfs_commit_req {
	void (*commit)();		/* pointer to commit function */
	page_t *plist;			/* page list */
	offset3 offset;			/* starting offset */
	count3 count;			/* size of range to be commited */
};

struct nfs_async_reqs {
	struct nfs_async_reqs *a_next;	/* pointer to next arg struct */
#ifdef DEBUG
	kthread_t *a_queuer;		/* thread id of queueing thread */
#endif
	struct vnode *a_vp;		/* vnode pointer */
	struct cred *a_cred;		/* cred pointer */
	enum iotype a_io;		/* i/o type */
	union {
		struct nfs_async_read_req a_read_args;
		struct nfs_pageio_req a_pageio_args;
		struct nfs_readdir_req a_readdir_args;
		struct nfs_commit_req a_commit_args;
	} a_args;
};

#define	a_nfs_readahead a_args.a_read_args.readahead
#define	a_nfs_blkoff a_args.a_read_args.blkoff
#define	a_nfs_seg a_args.a_read_args.seg
#define	a_nfs_addr a_args.a_read_args.addr

#define	a_nfs_putapage a_args.a_pageio_args.pageio
#define	a_nfs_pageio a_args.a_pageio_args.pageio
#define	a_nfs_pp a_args.a_pageio_args.pp
#define	a_nfs_off a_args.a_pageio_args.io_off
#define	a_nfs_len a_args.a_pageio_args.io_len
#define	a_nfs_flags a_args.a_pageio_args.flags

#define	a_nfs_readdir a_args.a_readdir_args.readdir
#define	a_nfs_rdc a_args.a_readdir_args.rdc

#define	a_nfs_commit a_args.a_commit_args.commit
#define	a_nfs_plist a_args.a_commit_args.plist
#define	a_nfs_offset a_args.a_commit_args.offset
#define	a_nfs_count a_args.a_commit_args.count

/*
 * Failover information, passed opaquely through rfscall()
 */
typedef struct failinfo {
	struct vnode	*vp;
	caddr_t		fhp;
	void (*copyproc)(caddr_t, vnode_t *);
	int (*lookupproc)(vnode_t *, char *, vnode_t **, struct pathname *,
			int, vnode_t *, struct cred *, int);
} failinfo_t;

/*
 * Static server information
 */
typedef struct servinfo {
	struct knetconfig *sv_knconf;   /* bound TLI fd */
	struct netbuf	sv_addr;	/* server's address */
	nfs_fhandle	sv_fhandle;	/* this server's filehandle */
	struct sec_data *sv_secdata;	/* security data for rpcsec module */
	char	*sv_hostname;		/* server's hostname */
	int	sv_hostnamelen;		/* server's hostname length */
	struct servinfo	*sv_next;	/* next in list */
} servinfo_t;

/*
 * NFS private data per mounted file system
 *	The mi_lock mutex protects the following fields:
 *		mi_flags
 *		mi_printed
 *		mi_down
 *		mi_stsize
 *		mi_curread
 *		mi_curwrite
 *		mi_timers
 *		mi_curr_serv
 *		mi_readers
 *		mi_klmconfig
 *
 *	Normally the netconfig information for the mount comes from
 *	mi_curr_serv and mi_klmconfig is NULL.  If NLM calls need to use a
 *	different transport, mi_klmconfig contains the necessary netconfig
 *	information.
 */
typedef struct mntinfo {
	kmutex_t	mi_lock;	/* protects mntinfo fields */
	struct servinfo *mi_servers;    /* server list */
	struct servinfo *mi_curr_serv;  /* current server */
	kcondvar_t	mi_failover_cv;	/* failover synchronization */
	int		mi_readers;	/* failover - users of mi_curr_serv */
	struct vfs	*mi_vfsp;	/* back pointer to vfs */
	enum vtype	mi_type;	/* file type of the root vnode */
	uint_t		mi_flags;	/* see below */
	int		mi_tsize;	/* transfer size (bytes) */
					/* really read size */
	int		mi_stsize;	/* server's max transfer size (bytes) */
					/* really write size */
	int		mi_timeo;	/* inital timeout in 10th sec */
	int		mi_retrans;	/* times to retry request */
	uint_t		mi_acregmin;	/* min secs to hold cached file attr */
	uint_t		mi_acregmax;	/* max secs to hold cached file attr */
	uint_t		mi_acdirmin;	/* min secs to hold cached dir attr */
	uint_t		mi_acdirmax;	/* max secs to hold cached dir attr */
	len_t		mi_maxfilesize; /* for pathconf _PC_FILESIZEBITS */
	/*
	 * Extra fields for congestion control, one per NFS call type,
	 * plus one global one.
	 */
	struct rpc_timers mi_timers[NFS_CALLTYPES+1];
	int		mi_curread;	/* current read size */
	int		mi_curwrite;	/* current write size */
	/*
	 * async I/O management
	 */
	struct nfs_async_reqs *mi_async_reqs[NFS_ASYNC_TYPES];
	struct nfs_async_reqs *mi_async_tail[NFS_ASYNC_TYPES];
	struct nfs_async_reqs **mi_async_curr;	/* current async queue */
	uint_t		mi_async_clusters[NFS_ASYNC_TYPES];
	uint_t		mi_async_init_clusters;
	kcondvar_t	mi_async_reqs_cv;
	ushort_t	mi_threads;	/* number of active async threads */
	ushort_t	mi_max_threads;	/* max number of async threads */
	kcondvar_t	mi_async_cv;
	uint_t		mi_async_count;	/* number of entries on async list */
	kmutex_t	mi_async_lock;	/* lock to protect async list */
	/*
	 * Other stuff
	 */
	struct pathcnf *mi_pathconf;	/* static pathconf kludge */
	rpcprog_t	mi_prog;	/* RPC program number */
	rpcvers_t	mi_vers;	/* RPC program version number */
	char		**mi_rfsnames;	/* mapping to proc names */
	kstat_named_t	*mi_reqs;	/* count of requests */
	char		*mi_call_type;	/* dynamic retrans call types */
	char		*mi_ss_call_type;	/* semisoft call type */
	char		*mi_timer_type;	/* dynamic retrans timer types */
	clock_t		mi_printftime;	/* last error printf time */
	/*
	 * ACL entries
	 */
	char		**mi_aclnames;	/* mapping to proc names */
	kstat_named_t	*mi_aclreqs;	/* count of acl requests */
	char		*mi_acl_call_type; /* dynamic retrans call types */
	char		*mi_acl_ss_call_type; /* semisoft call types */
	char		*mi_acl_timer_type; /* dynamic retrans timer types */
	/*
	 * Client Side Failover stats
	 */
	uint_t		mi_noresponse;	/* server not responding count */
	uint_t		mi_failover; 	/* failover to new server count */
	uint_t		mi_remap;	/* remap to new server count */
	/*
	 * Kstat statistics
	 */
	struct kstat	*mi_io_kstats;
	struct kstat	*mi_ro_kstats;
	struct knetconfig *mi_klmconfig;
} mntinfo_t;

/*
 * vfs pointer to mount info
 */
#define	VFTOMI(vfsp)	((mntinfo_t *)((vfsp)->vfs_data))

/*
 * vnode pointer to mount info
 */
#define	VTOMI(vp)	((mntinfo_t *)(((vp)->v_vfsp)->vfs_data))

/*
 * The values for mi_flags.
 */
#define	MI_HARD		0x1		/* hard or soft mount */
#define	MI_PRINTED	0x2		/* not responding message printed */
#define	MI_INT		0x4		/* interrupts allowed on hard mount */
#define	MI_DOWN		0x8		/* server is down */
#define	MI_NOAC		0x10		/* don't cache attributes */
#define	MI_NOCTO	0x20		/* no close-to-open consistency */
#define	MI_DYNAMIC	0x40		/* dynamic transfer size adjustment */
#define	MI_LLOCK	0x80		/* local locking only (no lockmgr) */
#define	MI_GRPID	0x100		/* System V group id inheritance */
#define	MI_RPCTIMESYNC	0x200		/* RPC time sync */
#define	MI_LINK		0x400		/* server supports link */
#define	MI_SYMLINK	0x800		/* server supports symlink */
#define	MI_READDIR	0x1000		/* use readdir instead of readdirplus */
#define	MI_ACL		0x2000		/* server supports NFS_ACL */
#define	MI_BINDINPROG	0x4000		/* binding to server is changing */
#define	MI_LOOPBACK	0x8000		/* Set if this is a loopback mount */
#define	MI_SEMISOFT	0x10000		/* soft reads, hard modify */
#define	MI_NOPRINT	0x20000		/* don't print messages */
#define	MI_DIRECTIO	0x40000		/* do direct I/O */

/*
 * Read-only mntinfo statistics
 */
struct mntinfo_kstat {
	char		mik_proto[KNC_STRSIZE];
	uint32_t	mik_vers;
	uint_t		mik_flags;
	uint_t		mik_secmod;
	uint32_t	mik_curread;
	uint32_t	mik_curwrite;
	int		mik_timeo;
	int		mik_retrans;
	uint_t		mik_acregmin;
	uint_t		mik_acregmax;
	uint_t		mik_acdirmin;
	uint_t		mik_acdirmax;
	struct {
		uint32_t srtt;
		uint32_t deviate;
		uint32_t rtxcur;
	} mik_timers[NFS_CALLTYPES+1];
	uint32_t	mik_noresponse;
	uint32_t	mik_failover;
	uint32_t	mik_remap;
	char		mik_curserver[SYS_NMLN];
};

/*
 * Mark cached attributes as timed out
 *
 * The caller must not be holding the rnode r_statelock mutex.
 */
#define	PURGE_ATTRCACHE(vp)	{				\
	rnode_t *rp = VTOR(vp);					\
	mutex_enter(&rp->r_statelock);				\
	rp->r_attrtime = hrestime.tv_sec;			\
	rp->r_seq++;						\
	mutex_exit(&rp->r_statelock);				\
}

/*
 * Is the attribute cache valid?
 */
#define	ATTRCACHE_VALID(vp)	hrestime.tv_sec < VTOR(vp)->r_attrtime

/*
 * If returned error is ESTALE flush all caches.
 */
#define	PURGE_STALE_FH(errno, vp, cr)				\
	if ((errno) == ESTALE) {				\
		struct rnode *rp = VTOR(vp);			\
		mutex_enter(&rp->r_statelock);			\
		rp->r_flags |= RDONTWRITE;			\
		if (!rp->r_error)				\
			rp->r_error = errno;			\
		mutex_exit(&rp->r_statelock);			\
		if ((vp)->v_pages != NULL)			\
			nfs_invalidate_pages((vp), (u_offset_t)0, (cr)); \
		nfs_purge_caches((vp), (cr));			\
	}

/*
 * Is cache valid?
 * Swap is always valid, if no attributes (attrtime == 0) or
 * if mtime matches cached mtime it is valid
 * NOTE: mtime is now a timestruc_t.
 * Caller should be holding the rnode r_statelock mutex.
 */
#define	CACHE_VALID(rp, mtime, fsize)				\
	((RTOV(rp)->v_flag & VISSWAP) == VISSWAP ||		\
	(((mtime).tv_sec == (rp)->r_attr.va_mtime.tv_sec &&	\
	(mtime).tv_nsec == (rp)->r_attr.va_mtime.tv_nsec) &&	\
	((fsize) == (rp)->r_attr.va_size)))

/*
 * Structure to identify owner of a PC file share reservation.
 */
struct nfs_owner {
	int	magic;		/* magic uniquifying number */
	char	hname[16];	/* first 16 bytes of hostname */
	char	lowner[8];	/* local owner from fcntl */
};

/*
 * Values for magic.
 */
#define	NFS_OWNER_MAGIC	0x1D81E

/*
 * Short hand for checking to see whether the file system was mounted
 * interruptible or not.
 */
#define	INTR(vp)	(VTOMI(vp)->mi_flags & MI_INT)

#ifdef _KERNEL
extern void	nfs_free_mi(mntinfo_t *);
extern void	nfs_mnt_kstat_init(struct vfs *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_NFS_CLNT_H */
