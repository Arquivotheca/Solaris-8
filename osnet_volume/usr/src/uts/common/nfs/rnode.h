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
 *	Copyright (c) 1986-1991,1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_NFS_RNODE_H
#define	_NFS_RNODE_H

#pragma ident	"@(#)rnode.h	1.69	99/06/22 SMI"	/* SVr4.0 1.3	*/
/*	rnode.h 1.23 88/08/19 SMI */

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum nfs_access_type {
	NFS_ACCESS_UNKNOWN,
	NFS_ACCESS_ALLOWED,
	NFS_ACCESS_DENIED
} nfs_access_type_t;

typedef struct acache {
	uint32_t known;
	uint32_t allowed;
	struct rnode *rnode;
	cred_t *cred;
	struct acache *next;
	struct acache *list;
} acache_t;

#define	NFS_FHANDLE_LEN	64

typedef struct nfs_fhandle {
	int fh_len;
	char fh_buf[NFS_FHANDLE_LEN];
} nfs_fhandle;

typedef struct rddir_cache {
	lloff_t _cookie;	/* cookie used to find this cache entry */
	lloff_t _ncookie;	/* cookie used to find the next cache entry */
	char *entries;		/* buffer containing dirent entries */
	int eof;		/* EOF reached after this request */
	int entlen;		/* size of dirent entries in buf */
	int buflen;		/* size of the buffer used to store entries */
	int flags;		/* control flags, see below */
	kcondvar_t cv;		/* cv for blocking */
	int error;		/* error from RPC operation */
	struct rddir_cache *next;	/* ptr to next entry in cache */
} rddir_cache;

#define	nfs_cookie	_cookie._p._l
#define	nfs_ncookie	_ncookie._p._l
#define	nfs3_cookie	_cookie._f
#define	nfs3_ncookie	_ncookie._f

#define	RDDIR		0x1	/* readdir operation in progress */
#define	RDDIRWAIT	0x2	/* waiting on readdir in progress */
#define	RDDIRREQ	0x4	/* a new readdir is required */

typedef struct symlink_cache {
	char *contents;		/* contents of the symbolic link */
	int len;		/* length of the contents */
	int size;		/* size of the allocated buffer */
} symlink_cache;

typedef struct commit {
	page_t *c_pages;	/* list of pages to commit */
	offset3 c_commbase;	/* base offset to do commit from */
	count3 c_commlen;	/* len to commit */
	kcondvar_t c_cv;	/* condvar for waiting for commit */
} commit_t;

/*
 * The various values for the commit states.  These are stored in
 * the p_fsdata byte in the page struct.
 */
#define	C_NOCOMMIT	0	/* no commit is required */
#define	C_COMMIT	1	/* a commit is required so do it now */
#define	C_DELAYCOMMIT	2	/* a commit is required, but can be delayed */

/*
 * The lock manager holds state making it possible for the client
 * and server to be out of sync.  For example, if the response from
 * the server granting a lock request is lost, the server will think
 * the lock is granted and the client will think the lock is lost.
 * To deal with this, a list of processes for which the client is
 * not sure if the server holds a lock is attached to the rnode.
 * When such a process closes the rnode, an unlock request is sent
 * to the server to unlock the entire file.
 *
 * The list is kept as a singularly linked NULL terminated list.
 * Because it is  only added to under extreme error conditions, the
 * list shouldn't get very big.  DEBUG kernels print a console warning
 * when the number of entries on a list go beyond nfs_lmpl_high_water
 * an  arbitrary number defined in nfs_add_locking_id()
 */
#define	RLMPL_PID	1
#define	RLMPL_OWNER	2
typedef struct lock_manager_pid_list {
	int lmpl_type;
	pid_t lmpl_pid;
	union {
		pid_t _pid;
		struct {
			int len;
			char *owner;
		} _own;
	} un;
	struct lock_manager_pid_list *lmpl_next;
} lmpl_t;

#define	lmpl_opid un._pid
#define	lmpl_own_len un._own.len
#define	lmpl_owner un._own.owner

/*
 * A homegrown reader/writer lock implementation.  It addresses
 * two requirements not addressed by the system primitives.  They
 * are that the `enter" operation is optionally interruptible and
 * that that they can be re`enter'ed by writers without deadlock.
 */
typedef struct nfs_rwlock {
	int count;
	int waiters;
	kthread_t *owner;
	kmutex_t lock;
	kcondvar_t cv;
} nfs_rwlock_t;

/*
 * Remote file information structure.
 *
 * The rnode is the "inode" for remote files.  It contains all the
 * information necessary to handle remote file on the client side.
 *
 * Note on file sizes:  we keep two file sizes in the rnode: the size
 * according to the client (r_size) and the size according to the server
 * (r_attr.va_size).  They can differ because we modify r_size during a
 * write system call (nfs_rdwr), before the write request goes over the
 * wire (before the file is actually modified on the server).  If an OTW
 * request occurs before the cached data is written to the server the file
 * size returned from the server (r_attr.va_size) may not match r_size.
 * r_size is the one we use, in general.  r_attr.va_size is only used to
 * determine whether or not our cached data is valid.
 *
 * Each rnode has 3 locks associated with it (not including the rnode
 * hash table and free list locks):
 *
 *	r_rwlock:	Serializes nfs_write and nfs_setattr requests
 *			and allows nfs_read requests to proceed in parallel.
 *			Serializes reads/updates to directories.
 *
 *	r_lkserlock:	Serializes lock requests with map, write, and
 *			readahead operations.
 *
 *	r_statelock:	Protects all fields in the rnode except for
 *			those listed below.  This lock is intented
 *			to be held for relatively short periods of
 *			time (not accross entire putpage operations,
 *			for example).
 *
 * The following members are protected by the mutex nfs_rtable_lock:
 *	r_freef
 *	r_freeb
 *	r_hash
 *
 * Note: r_modaddr is only accessed when the r_statelock mutex is held.
 *	Its value is also controlled via r_rwlock.  It is assumed that
 *	there will be only 1 writer active at a time, so it safe to
 *	set r_modaddr and release r_statelock as long as the r_rwlock
 *	writer lock is held.
 *
 * 64-bit offsets: the code formerly assumed that atomic reads of
 * r_size were safe and reliable; on 32-bit architectures, this is
 * not true since an intervening bus cycle from another processor
 * could update half of the size field.  The r_statelock must now
 * be held whenever any kind of access of r_size is made.
 *
 * Lock ordering:
 * 	r_rwlock > r_lkserlock > r_statelock
 */
struct exportinfo;	/* defined in nfs/export.h */
struct servinfo;	/* defined in nfs/nfs_clnt.h */
struct failinfo;	/* defined in nfs/nfs_clnt.h */
struct mntinfo;		/* defined in nfs/nfs_clnt.h */

typedef struct rnode {
	struct rnode	*r_freef;	/* free list forward pointer */
	struct rnode	*r_freeb;	/* free list back pointer */
	struct rnode	*r_hash;	/* rnode hash chain */
	vnode_t		r_vnode;	/* vnode for remote file */
	nfs_rwlock_t	r_rwlock;	/* serializes write/setattr requests */
	nfs_rwlock_t	r_lkserlock;	/* serialize lock with other ops */
	kmutex_t	r_statelock;	/* protects (most of) rnode contents */
	nfs_fhandle	r_fh;		/* file handle */
	struct servinfo	*r_server;	/* current server */
	char		*r_path;	/* path to this rnode */
	u_offset_t	r_nextr;	/* next byte read offset (read-ahead) */
	ushort_t	r_flags;	/* flags, see below */
	short		r_error;	/* async write error */
	cred_t		*r_cred;	/* current credentials */
	cred_t		*r_unlcred;	/* unlinked credentials */
	char		*r_unlname;	/* unlinked file name */
	vnode_t		*r_unldvp;	/* parent dir of unlinked file */
	len_t		r_size;		/* client's view of file size */
	struct vattr	r_attr;		/* cached vnode attributes */
	time_t		r_attrtime;	/* time attributes become invalid */
	time_t		r_mtime;	/* client time file last modified */
	long		r_mapcnt;	/* count of mmapped pages */
	uint_t		r_count;	/* # of refs not reflect in v_count */
	uint_t		r_awcount;	/* # of outstanding async write */
	kcondvar_t	r_cv;		/* condvar for blocked threads */
	int		r_seq;		/* sequence number for attr changes */
	int		(*r_putapage)	/* address of putapage routine */
		(vnode_t *, page_t *, u_offset_t *, size_t *, int, cred_t *);
	rddir_cache	*r_dir;		/* cache of readdir responses */
	rddir_cache	*r_direof;	/* pointer to the EOF entry */
	symlink_cache	r_symlink;	/* cached readlink response */
	writeverf3	r_verf;		/* version 3 write verifier */
	u_offset_t	r_modaddr;	/* address for page in writerp */
	commit_t	r_commit;	/* commit information */
	u_offset_t	r_truncaddr;	/* base for truncate operation */
	vsecattr_t	*r_secattr;	/* cached security attributes (acls) */
	cookieverf3	r_cookieverf;	/* version 3 readdir cookie verifier */
	lmpl_t		*r_lmpl;	/* pids that may be holding locks */
	nfs3_pathconf_info *r_pathconf;	/* cached pathconf information */
	acache_t	*r_acache;	/* list of access cache entries */
} rnode_t;

/*
 * Flags
 */
#define	REOF		0x1	/* EOF encountered on readdir */
#define	RDIRTY		0x2	/* dirty pages from write operation */
#define	RDONTWRITE	0x4	/* don't even attempt to write */
#define	RMODINPROGRESS	0x8	/* page modification happening */
#define	RTRUNCATE	0x10	/* truncating, don't commit */
#define	RHAVEVERF	0x20	/* have a write verifier to compare against */
#define	RCOMMIT		0x40	/* commit in progress */
#define	RCOMMITWAIT	0x80	/* someone is waiting to do a commit */
#define	RHASHED		0x100	/* rnode is in hash queues */
#define	ROUTOFSPACE	0x200	/* an out of space error has happened */
#define	RSERIALIZE	0x400	/* serialize otw getattrs */
#define	RPURGECACHE	0x800	/* need a page cache flush */
#define	RDIRECTIO	0x1000	/* bypass the buffer cache */
/*
 * Convert between vnode and rnode
 */
#define	RTOV(rp)	(&(rp)->r_vnode)
#define	VTOR(vp)	((rnode_t *)((vp)->v_data))

#define	VTOFH(vp)	(RTOFH(VTOR(vp)))
#define	RTOFH(rp)	((fhandle_t *)(&(rp)->r_fh.fh_buf))
#define	VTOFH3(vp)	(RTOFH3(VTOR(vp)))
#define	RTOFH3(rp)	((nfs_fh3 *)(&(rp)->r_fh))

#ifdef _KERNEL
extern void	nfs_async_readahead(vnode_t *, u_offset_t, caddr_t,
				struct seg *, cred_t *,
				void (*)(vnode_t *, u_offset_t,
				caddr_t, struct seg *, cred_t *));
extern int	nfs_async_putapage(vnode_t *, page_t *, u_offset_t, size_t,
				int, cred_t *, int (*)(vnode_t *, page_t *,
				u_offset_t, size_t, int, cred_t *));
extern int	nfs_async_pageio(vnode_t *, page_t *, u_offset_t, size_t,
				int, cred_t *, int (*)(vnode_t *, page_t *,
				u_offset_t, size_t, int, cred_t *));
extern void	nfs_async_readdir(vnode_t *, rddir_cache *,
				cred_t *, int (*)(vnode_t *,
				rddir_cache *, cred_t *));
extern void	nfs_async_commit(vnode_t *, page_t *, offset3, count3,
				cred_t *, void (*)(vnode_t *, page_t *,
				offset3, count3, cred_t *));
extern int	writerp(rnode_t *, caddr_t, int, struct uio *);
extern int	nfs_putpages(vnode_t *, u_offset_t, size_t, int, cred_t *);
extern void	nfs_invalidate_pages(vnode_t *, u_offset_t, cred_t *);
extern int	rfs2call(struct mntinfo *, rpcproc_t, xdrproc_t, caddr_t,
			xdrproc_t, caddr_t, cred_t *, int *, enum nfsstat *,
			int, struct failinfo *);
extern int	rfs3call(struct mntinfo *, rpcproc_t, xdrproc_t, caddr_t,
			xdrproc_t, caddr_t, cred_t *, int *, nfsstat3 *,
			int, struct failinfo *);
extern vnode_t	*makenfsnode(fhandle_t *, struct nfsfattr *, struct vfs *,
			cred_t *, char *, char *);
extern vnode_t	*makenfs3node(nfs_fh3 *, fattr3 *, struct vfs *, cred_t *,
			char *, char *);
extern void	rp_addfree(rnode_t *, cred_t *);
extern void	destroy_rnodes(rnode_t *, cred_t *);
extern void	rp_addhash(rnode_t *);
extern void	rp_rmhash(rnode_t *);
extern int	check_rtable(struct vfs *);
extern void	destroy_rtable(struct vfs *, cred_t *);
extern void	rflush(struct vfs *, cred_t *);
extern nfs_access_type_t nfs_access_check(rnode_t *, uint32_t, cred_t *);
extern void	nfs_access_cache(rnode_t *rp, uint32_t, uint32_t, cred_t *);
extern int	nfs_access_purge_rp(rnode_t *);
extern int	nfs_putapage(vnode_t *, page_t *, u_offset_t *, size_t *,
			int, cred_t *);
extern int	nfs3_putapage(vnode_t *, page_t *, u_offset_t *, size_t *,
			int, cred_t *);
extern void	nfs_printfhandle(nfs_fhandle *);
extern void	nfs_write_error(vnode_t *, int, cred_t *);
#ifdef DEBUG
extern rddir_cache	*rddir_cache_alloc(size_t, int);
extern void		rddir_cache_free(void *, size_t);
extern char		*symlink_cache_alloc(size_t, int);
extern void		symlink_cache_free(void *, size_t);
#endif
extern int	nfs_rw_enter_sig(nfs_rwlock_t *, krw_t, int);
extern int	nfs_rw_tryenter(nfs_rwlock_t *, krw_t);
extern void	nfs_rw_exit(nfs_rwlock_t *);
extern int	nfs_rw_lock_held(nfs_rwlock_t *, krw_t);
extern void	nfs_rw_init(nfs_rwlock_t *, char *, krw_type_t, void *);
extern void	nfs_rw_destroy(nfs_rwlock_t *);
extern int	nfs_directio(vnode_t *, int, cred_t *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_RNODE_H */
