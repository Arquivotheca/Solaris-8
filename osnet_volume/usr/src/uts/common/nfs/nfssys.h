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
 *	Copyright (c) 1986-1991,1993,1994,1996,1999 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */


#ifndef	_NFS_NFSSYS_H
#define	_NFS_NFSSYS_H

#pragma ident	"@(#)nfssys.h	1.37	99/08/13 SMI"	/* SVr4.0 1.5   */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private definitions for the nfssys system call.
 * Note: <nfs/export.h> and <nfs/nfs.h> must be included before
 * this file.
 */

/*
 * Flavors of nfssys call.  Note that OLD_mumble commands are no longer
 * implemented, but the entries are kept as placeholders for binary
 * compatibility.
 */
enum nfssys_op	{ OLD_NFS_SVC, OLD_ASYNC_DAEMON, EXPORTFS, NFS_GETFH,
    OLD_NFS_CNVT, NFS_REVAUTH, OLD_NFS_FH_TO_FID, OLD_LM_SVC, KILL_LOCKMGR,
    LOG_FLUSH, SVCPOOL_CREATE, NFS_SVC, LM_SVC };

struct nfs_svc_args {
	int		fd;		/* Connection endpoint */
	char		*netid;		/* Identify transport */
	struct netbuf	addrmask;	/* Address mask for host */
};

#ifdef _SYSCALL32
struct nfs_svc_args32 {
	int32_t		fd;		/* Connection endpoint */
	caddr32_t	netid;		/* Identify transport */
	struct netbuf32	addrmask;	/* Address mask for host */
};
#endif

struct exportfs_args {
	char		*dname;
	struct exportdata *uex;
};

#ifdef _SYSCALL32
struct exportfs_args32 {
	caddr32_t	dname;
	caddr32_t	uex;
};
#endif

struct nfs_getfh_args {
	char		*fname;
	fhandle_t	*fhp;
};

#ifdef _SYSCALL32
struct nfs_getfh_args32 {
	caddr32_t	fname;
	caddr32_t	fhp;
};
#endif

struct nfs_revauth_args {
	int		authtype;
	uid_t		uid;
};

#ifdef _SYSCALL32
struct nfs_revauth_args32 {
	int32_t		authtype;
	uid32_t		uid;
};
#endif

/*
 * Arguments for establishing lock manager service.  If you change
 * lm_svc_args, you should increment the version number.  Try to keep
 * supporting one or more old versions of the args, so that old lockd's
 * will work with new kernels.
 */

enum lm_fmly  { LM_INET, LM_INET6, LM_LOOPBACK };
enum lm_proto { LM_TCP, LM_UDP };

struct lm_svc_args {
	int		version;	/* keep this first */
	int		fd;
	enum lm_fmly	n_fmly;		/* protocol family */
	enum lm_proto	n_proto;	/* protocol */
	dev_t		n_rdev;		/* device ID */
	int		debug;		/* debugging level */
	time_t		timout;		/* client handle life (asynch RPCs) */
	int		grace;		/* secs in grace period */
	time_t	retransmittimeout;	/* retransmission interval */
};

#ifdef _SYSCALL32
struct lm_svc_args32 {
	int32_t		version;	/* keep this first */
	int32_t		fd;
	enum lm_fmly	n_fmly;		/* protocol family */
	enum lm_proto	n_proto;	/* protocol */
	dev32_t		n_rdev;		/* device ID */
	int32_t		debug;		/* debugging level */
	time32_t	timout;		/* client handle life (asynch RPCs) */
	int32_t		grace;		/* secs in grace period */
	time32_t	retransmittimeout;	/* retransmission interval */
};
#endif

#define	LM_SVC_CUR_VERS	30		/* current lm_svc_args vers num */

/*
 * Arguments for nfslog flush service.
 */
struct nfsl_flush_args {
	int		version;
	int		directive;
	char		*buff;		/* buffer to flush/rename */
	int		buff_len;	/* includes terminating '\0' */
};

#define	NFSL_FLUSH_ARGS_VERS 1		/* current nfsl_flush_args vers num */

#ifdef _SYSCALL32
struct nfsl_flush_args32 {
	int32_t		version;
	int32_t		directive;
	caddr32_t	buff;		/* buffer to flush/rename */
	int32_t		buff_len;	/* includes terminating '\0' */
};
#endif

#define	NFSL_ALL	0x01		/* Flush all buffers */
#define	NFSL_RENAME	0x02		/* Rename buffer(s) */
#define	NFSL_SYNC	0x04		/* Perform operation synchronously? */

#ifdef _KERNEL
union nfssysargs {
	struct exportfs_args	*exportfs_args_u;	/* exportfs args */
	struct nfs_getfh_args	*nfs_getfh_args_u;	/* nfs_getfh args */
	struct nfs_svc_args	*nfs_svc_args_u;	/* nfs_svc args */
	struct nfs_revauth_args	*nfs_revauth_args_u;	/* nfs_revauth args */
	struct lm_svc_args	*lm_svc_args_u;		/* lm_svc args */
	/* kill_lockmgr args: none */
	struct nfsl_flush_args	*nfsl_flush_args_u;	/* nfsl_flush args */
	struct svcpool_args	*svcpool_args_u;	/* svcpool args */
};

struct nfssysa {
	enum nfssys_op		opcode;	/* operation discriminator */
	union nfssysargs	arg;	/* syscall-specific arg pointer */
};
#define	nfssysarg_exportfs	arg.exportfs_args_u
#define	nfssysarg_getfh		arg.nfs_getfh_args_u
#define	nfssysarg_svc		arg.nfs_svc_args_u
#define	nfssysarg_revauth	arg.nfs_revauth_args_u
#define	nfssysarg_lmsvc		arg.lm_svc_args_u
#define	nfssysarg_nfslflush	arg.nfsl_flush_args_u
#define	nfssysarg_svcpool	arg.svcpool_args_u

#ifdef _SYSCALL32
union nfssysargs32 {
	caddr32_t exportfs_args_u;	/* exportfs args */
	caddr32_t nfs_getfh_args_u;	/* nfs_getfh args */
	caddr32_t nfs_svc_args_u;	/* nfs_svc args */
	caddr32_t nfs_revauth_args_u;	/* nfs_revauth args */
	caddr32_t lm_svc_args_u;	/* lm_svc args */
	/* kill_lockmgr args: none */
	caddr32_t nfsl_flush_args_u;	/* nfsl_flush args */
	caddr32_t svcpool_args_u;
};
struct nfssysa32 {
	enum nfssys_op		opcode;	/* operation discriminator */
	union nfssysargs32	arg;	/* syscall-specific arg pointer */
};
#endif /* _SYSCALL32 */

#endif	/* _KERNEL */

#ifdef _KERNEL

#include <sys/systm.h>		/* for rval_t typedef */

extern int	nfssys(enum nfssys_op opcode, void *arg);
extern int	exportfs(struct exportfs_args *, model_t, cred_t *);
extern int	nfs_getfh(struct nfs_getfh_args *, model_t, cred_t *);
extern int	nfs_svc(struct nfs_svc_args *, model_t);
extern int	nfs_revoke_auth(int, uid_t, cred_t *);
extern int	lm_svc(struct lm_svc_args *uap);
extern int	lm_shutdown(void);
extern int	nfsl_flush(struct nfsl_flush_args *, model_t, cred_t *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_NFSSYS_H */
