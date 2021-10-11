/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 *	(c) 1986-1991,1994-1996  Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef	_NFS_EXPORT_H
#define	_NFS_EXPORT_H

#pragma ident	"@(#)export.h	1.44	99/01/20 SMI"
/*	export.h 1.7 88/08/19 SMI */

#include <nfs/nfs_sec.h>
#include <rpcsvc/nfsauth_prot.h>
#include <sys/vnode.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct secinfo {
	seconfig_t	s_secinfo;	/* /etc/nfssec.conf entry */
	unsigned int	s_flags;	/* flags (see below) */
	int 		s_window;	/* window */
	int		s_rootcnt;	/* count of root names */
	caddr_t		*s_rootnames;	/* array of root names */
					/* they are strings for AUTH_DES and */
					/* rpc_gss_principal_t for RPCSEC_GSS */
};

#ifdef _SYSCALL32
struct secinfo32 {
	seconfig32_t	s_secinfo;	/* /etc/nfssec.conf entry */
	uint32_t	s_flags;	/* flags (see below) */
	int32_t 	s_window;	/* window */
	int32_t		s_rootcnt;	/* count of root names */
	caddr32_t	s_rootnames;	/* array of root names */
					/* they are strings for AUTH_DES and */
					/* rpc_gss_principal_t for RPCSEC_GSS */
};
#endif /* _SYSCALL32 */

/*
 * security negotiation related
 */

#define	SEC_QUERY	0x01	/* query sec modes */

struct sec_ol {
	int		sec_flags;	/* security nego flags */
	uint_t		sec_index;	/* index into sec flavor array */
};

/*
 * Per-mode flags
 */

#define	M_RO	0x01		/* exported ro to all */
#define	M_ROL	0x02		/* exported ro to all listed */
#define	M_RW	0x04		/* exported rw to all */
#define	M_RWL	0x08		/* exported ro to all listed */
#define	M_ROOT	0x10		/* root list is defined */

/*
 * The export information passed to exportfs() (Version 2)
 */
#define	EX_CURRENT_VERSION 2	/* current version of exportdata struct */

struct exportdata {
	int		ex_version;	/* structure version */
	char		*ex_path;	/* exported path */
	size_t		ex_pathlen;	/* path length */
	int		ex_flags;	/* flags */
	unsigned int	ex_anon;	/* uid for unauthenticated requests */
	int		ex_seccnt;	/* count of security modes */
	struct secinfo	*ex_secinfo;	/* security mode info */
	char		*ex_index;	/* index file for public filesystem */
	char		*ex_log_buffer;	/* path to logging buffer file */
	size_t		ex_log_bufferlen;	/* buffer file path len */
	char		*ex_tag;	/* tag used to identify log config */
	size_t		ex_taglen;	/* tag length */
};

#ifdef _SYSCALL32
struct exportdata32 {
	int32_t		ex_version;	/* structure version */
	caddr32_t	ex_path;	/* exported path */
	int32_t		ex_pathlen;	/* path length */
	int32_t		ex_flags;	/* flags */
	uint32_t	ex_anon;	/* uid for unauthenticated requests */
	int32_t		ex_seccnt;	/* count of security modes */
	caddr32_t	ex_secinfo;	/* security mode info */
	caddr32_t	ex_index;	/* index file for public filesystem */
	caddr32_t	ex_log_buffer;	/* path to logging buffer file */
	int32_t		ex_log_bufferlen;	/* buffer file path len */
	caddr32_t	ex_tag;		/* tag used to identify log config */
	int32_t		ex_taglen;	/* tag length */
};
#endif /* _SYSCALL32 */

/*
 * exported vfs flags.
 */

#define	EX_NOSUID	0x01	/* exported with unsetable set[ug]ids */
#define	EX_ACLOK	0x02	/* exported with maximal access if acl exists */
#define	EX_PUBLIC	0x04	/* exported with public filehandle */
#define	EX_NOSUB	0x08	/* no nfs_getfh or MCL below export point */
#define	EX_INDEX	0x10	/* exported with index file specified */
#define	EX_LOG		0x20	/* logging enabled */
#define	EX_LOG_ALLOPS	0x40	/* logging of all RPC operations enabled */
				/* by default only operations which affect */
				/* transaction logging are enabled */

#ifdef	_KERNEL

/*
 * An authorization cache entry
 */
struct auth_cache {
	struct netbuf		auth_addr;
	int			auth_flavor;
	int			auth_access;
	time_t			auth_time;
	struct auth_cache	*auth_next;
};

#define	AUTH_TABLESIZE	32

/*
 * Structure containing log file meta-data.
 */
struct log_file {
	unsigned int	lf_flags;	/* flags (see below) */
	int		lf_writers;	/* outstanding writers */
	int		lf_refcnt;	/* references to this struct */
	caddr_t		lf_path;	/* buffer file location */
	vnode_t		*lf_vp;		/* vnode for the buffer file */
	kmutex_t	lf_lock;
	kcondvar_t	lf_cv_waiters;
};

/*
 * log_file and log_buffer flags.
 */
#define	L_WAITING	0x01		/* flush of in-core data to stable */
					/* storage in progress */
#define	L_PRINTED	0x02		/* error message printed to console */
#define	L_ERROR		0x04		/* error condition detected */

/*
 * The logging buffer information.
 * This structure may be shared by multiple exportinfo structures,
 * if they share the same buffer file.
 * This structure contains the basic information about the buffer, such
 * as it's location in the filesystem.
 *
 * 'lb_lock' protects all the fields in this structure except for 'lb_path',
 * and 'lb_next'.
 * 'lb_path' is a write-once/read-many field which needs no locking, it is
 * set before the structure is linked to any exportinfo structure.
 * 'lb_next' is protected by the log_buffer_list_lock.
 */
struct log_buffer {
	unsigned int	lb_flags;	/* L_ONLIST set? */
	int		lb_refcnt;	/* references to this struct */
	unsigned int	lb_rec_id;	/* used to generate unique id */
	caddr_t		lb_path;	/* buffer file pathname */
	struct log_file	*lb_logfile;	/* points to log_file structure */
	kmutex_t	lb_lock;
	struct log_buffer	*lb_next;
	kcondvar_t	lb_cv_waiters;
	caddr_t		lb_records;	/* linked list of records to write */
	int		lb_num_recs;	/* # of records to write */
	ssize_t		lb_size_queued; /* number of bytes queued for write */
};

#define	LOG_BUFFER_HOLD(lbp)	{ \
	mutex_enter(&(lbp)->lb_lock); \
	(lbp)->lb_refcnt++; \
	mutex_exit(&(lbp)->lb_lock); \
}

#define	LOG_BUFFER_RELE(lbp)	{ \
	log_buffer_rele(lbp); \
}

#define	EXPTABLESIZE	16

/*
 * A node associated with an export entry on the
 * list of exported filesystems.
 *
 * The exportinfo structure is protected by the exi_lock.
 * You must have the writer lock to delete an exportinfo
 * structure from the list.
 */

struct exportinfo {
	struct exportdata	exi_export;
	fsid_t			exi_fsid;
	struct fid		exi_fid;
	struct exportinfo	*exi_hash;
	fhandle_t		exi_fh;
	krwlock_t		exi_cache_lock;
	vnode_t			*exi_vp;
	struct auth_cache	*exi_cache[AUTH_TABLESIZE];
	struct log_buffer	*exi_logbuffer;
};

#define	EQFSID(fsidp1, fsidp2)	\
	(((fsidp1)->val[0] == (fsidp2)->val[0]) && \
	    ((fsidp1)->val[1] == (fsidp2)->val[1]))

#define	EQFID(fidp1, fidp2)	\
	((fidp1)->fid_len == (fidp2)->fid_len && \
	    nfs_fhbcmp((char *)(fidp1)->fid_data, (char *)(fidp2)->fid_data, \
	    (uint_t)(fidp1)->fid_len) == 0)

#define	exportmatch(exi, fsid, fid)	\
	(EQFSID(&(exi)->exi_fsid, (fsid)) && EQFID(&(exi)->exi_fid, (fid)))

/*
 * Returns true iff exported filesystem is read-only to the given host.
 *
 * Note:  this macro should be as fast as possible since it's called
 * on each NFS modification request.
 */
#define	rdonly(exi, req)  (nfsauth_access(exi, req) & NFSAUTH_RO)

extern int	nfs_fhhash(fsid_t *, fid_t *);
extern int	nfs_fhbcmp(char *, char *, int);
extern int	nfs_exportinit(void);
extern void	nfs_exportfini(void);
extern int	chk_clnt_sec(struct exportinfo *, struct svc_req *req);
extern int	makefh(fhandle_t *, struct vnode *, struct exportinfo *);
extern int	makefh_ol(fhandle_t *, struct exportinfo *, uint_t);
extern int	makefh3(nfs_fh3 *, struct vnode *, struct exportinfo *);
extern int	makefh3_ol(nfs_fh3 *, struct exportinfo *, uint_t);
extern vnode_t *nfs_fhtovp(fhandle_t *, struct exportinfo *);
extern vnode_t *nfs3_fhtovp(nfs_fh3 *, struct exportinfo *);
extern vnode_t *lm_fhtovp(fhandle_t *fh);
extern vnode_t *lm_nfs3_fhtovp(nfs_fh3 *fh);
extern struct	exportinfo *findexport(fsid_t *, struct fid *);
extern struct	exportinfo *checkexport(fsid_t *, struct fid *);
extern void	export_rw_exit(void);
extern struct exportinfo *nfs_vptoexi(vnode_t *, vnode_t *, cred_t *, int *);

/*
 * "public" and default (root) location for public filehandle
 */
extern struct exportinfo *exi_public, *exi_root;
extern fhandle_t nullfh2;	/* for comparing V2 filehandles */

/*
 * Two macros for identifying public filehandles.
 * A v2 public filehandle is 32 zero bytes.
 * A v3 public filehandle is zero length.
 */
#define	PUBLIC_FH2(fh) \
	((fh)->fh_fsid.val[1] == 0 && \
	bcmp((fh), &nullfh2, sizeof (fhandle_t)) == 0)

#define	PUBLIC_FH3(fh) \
	((fh)->fh3_length == 0)

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_EXPORT_H */
