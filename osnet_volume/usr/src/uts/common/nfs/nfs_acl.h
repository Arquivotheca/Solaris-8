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
 *	Copyright (c) 1986-1991,1994,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _NFS_NFS_ACL_H
#define	_NFS_NFS_ACL_H

#pragma ident	"@(#)nfs_acl.h	1.8	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	NFS_ACL_MAX_ENTRIES	1024

typedef ushort_t o_mode;

struct aclent {
	int type;
	uid32_t id;
	o_mode perm;
};
typedef struct aclent aclent;

#define	NA_USER_OBJ	0x1
#define	NA_USER		0x2
#define	NA_GROUP_OBJ	0x4
#define	NA_GROUP	0x8
#define	NA_CLASS_OBJ	0x10
#define	NA_OTHER_OBJ	0x20
#define	NA_ACL_DEFAULT	0x1000

#define	NA_READ		0x4
#define	NA_WRITE	0x2
#define	NA_EXEC		0x1

struct secattr {
	uint32 mask;
	int aclcnt;
	struct {
		uint_t aclent_len;
		aclent *aclent_val;
	} aclent;
	int dfaclcnt;
	struct {
		uint_t dfaclent_len;
		aclent *dfaclent_val;
	} dfaclent;
};
typedef struct secattr secattr;

#define	NA_ACL		0x1
#define	NA_ACLCNT	0x2
#define	NA_DFACL	0x4
#define	NA_DFACLCNT	0x8

struct GETACL2args {
	fhandle_t fh;
	uint32 mask;
};
typedef struct GETACL2args GETACL2args;

struct GETACL2resok {
	struct nfsfattr attr;
	vsecattr_t acl;
};
typedef struct GETACL2resok GETACL2resok;

struct GETACL2res {
	enum nfsstat status;
	union {
		GETACL2resok ok;
	} res_u;
};
typedef struct GETACL2res GETACL2res;

struct SETACL2args {
	fhandle_t fh;
	vsecattr_t acl;
};
typedef struct SETACL2args SETACL2args;

struct SETACL2resok {
	struct nfsfattr attr;
};
typedef struct SETACL2resok SETACL2resok;

struct SETACL2res {
	enum nfsstat status;
	union {
		SETACL2resok ok;
	} res_u;
};
typedef struct SETACL2res SETACL2res;

struct GETATTR2args {
	fhandle_t fh;
};
typedef struct GETATTR2args GETATTR2args;

struct GETATTR2resok {
	struct nfsfattr attr;
};
typedef struct GETATTR2resok GETATTR2resok;

struct GETATTR2res {
	enum nfsstat status;
	union {
		GETATTR2resok ok;
	} res_u;
};
typedef struct GETATTR2res GETATTR2res;

struct ACCESS2args {
	fhandle_t fh;
	uint32 access;
};
typedef struct ACCESS2args ACCESS2args;

#define	ACCESS2_READ	0x1
#define	ACCESS2_LOOKUP	0x2
#define	ACCESS2_MODIFY	0x4
#define	ACCESS2_EXTEND	0x8
#define	ACCESS2_DELETE	0x10
#define	ACCESS2_EXECUTE	0x20

struct ACCESS2resok {
	struct nfsfattr attr;
	uint32 access;
};
typedef struct ACCESS2resok ACCESS2resok;

struct ACCESS2res {
	enum nfsstat status;
	union {
		ACCESS2resok ok;
	} res_u;
};
typedef struct ACCESS2res ACCESS2res;

struct GETACL3args {
	nfs_fh3 fh;
	uint32 mask;
};
typedef struct GETACL3args GETACL3args;

struct GETACL3resok {
	post_op_attr attr;
	vsecattr_t acl;
};
typedef struct GETACL3resok GETACL3resok;

struct GETACL3resfail {
	post_op_attr attr;
};
typedef struct GETACL3resfail GETACL3resfail;

struct GETACL3res {
	nfsstat3 status;
	union {
		GETACL3resok ok;
		GETACL3resfail fail;
	} res_u;
};
typedef struct GETACL3res GETACL3res;

struct SETACL3args {
	nfs_fh3 fh;
	vsecattr_t acl;
};
typedef struct SETACL3args SETACL3args;

struct SETACL3resok {
	post_op_attr attr;
};
typedef struct SETACL3resok SETACL3resok;

struct SETACL3resfail {
	post_op_attr attr;
};
typedef struct SETACL3resfail SETACL3resfail;

struct SETACL3res {
	nfsstat3 status;
	union {
		SETACL3resok ok;
		SETACL3resfail fail;
	} res_u;
};
typedef struct SETACL3res SETACL3res;

#define	NFS_ACL_PROGRAM	((rpcprog_t)(100227))
#define	NFS_ACL_VERSMIN	((rpcvers_t)(2))
#define	NFS_ACL_VERSMAX	((rpcvers_t)(3))

#define	NFS_ACL_V2		((rpcvers_t)(2))
#define	ACLPROC2_NULL		((rpcproc_t)(0))
#define	ACLPROC2_GETACL		((rpcproc_t)(1))
#define	ACLPROC2_SETACL		((rpcproc_t)(2))
#define	ACLPROC2_GETATTR	((rpcproc_t)(3))
#define	ACLPROC2_ACCESS		((rpcproc_t)(4))

#define	NFS_ACL_V3		((rpcvers_t)(3))
#define	ACLPROC3_NULL		((rpcproc_t)(0))
#define	ACLPROC3_GETACL		((rpcproc_t)(1))
#define	ACLPROC3_SETACL		((rpcproc_t)(2))

#ifdef _KERNEL
/* the xdr functions */
extern bool_t xdr_uid(XDR *, uid32_t *);
extern bool_t xdr_o_mode(XDR *, o_mode *);
extern bool_t xdr_aclent(XDR *, aclent_t *);
extern bool_t xdr_secattr(XDR *, vsecattr_t *);

extern bool_t xdr_GETACL2args(XDR *, GETACL2args *);
extern bool_t xdr_fastGETACL2args(XDR *, GETACL2args **);
extern bool_t xdr_GETACL2resok(XDR *, GETACL2resok *);
extern bool_t xdr_GETACL2res(XDR *, GETACL2res *);
extern bool_t xdr_SETACL2args(XDR *, SETACL2args *);
extern bool_t xdr_SETACL2resok(XDR *, SETACL2resok *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastSETACL2resok(XDR *, SETACL2resok *);
#endif
extern bool_t xdr_SETACL2res(XDR *, SETACL2res *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastSETACL2res(XDR *, SETACL2res *);
#endif
extern bool_t xdr_GETATTR2args(XDR *, GETATTR2args *);
extern bool_t xdr_fastGETATTR2args(XDR *, GETATTR2args **);
extern bool_t xdr_GETATTR2resok(XDR *, GETATTR2resok *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastGETATTR2resok(XDR *, GETATTR2resok *);
#endif
extern bool_t xdr_GETATTR2res(XDR *, GETATTR2res *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastGETATTR2res(XDR *, GETATTR2res *);
#endif
extern bool_t xdr_ACCESS2args(XDR *, ACCESS2args *);
extern bool_t xdr_fastACCESS2args(XDR *, ACCESS2args **);
extern bool_t xdr_ACCESS2resok(XDR *, ACCESS2resok *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastACCESS2resok(XDR *, ACCESS2resok *);
#endif
extern bool_t xdr_ACCESS2res(XDR *, ACCESS2res *);
#ifdef _LITTLE_ENDIAN
extern bool_t xdr_fastACCESS2res(XDR *, ACCESS2res *);
#endif

extern bool_t xdr_GETACL3args(XDR *, GETACL3args *);
extern bool_t xdr_GETACL3resok(XDR *, GETACL3resok *);
extern bool_t xdr_GETACL3resfail(XDR *, GETACL3resfail *);
extern bool_t xdr_GETACL3res(XDR *, GETACL3res *);
extern bool_t xdr_SETACL3args(XDR *, SETACL3args *);
extern bool_t xdr_SETACL3resok(XDR *, SETACL3resok *);
extern bool_t xdr_SETACL3resfail(XDR *, SETACL3resfail *);
extern bool_t xdr_SETACL3res(XDR *, SETACL3res *);
#endif

#ifdef _KERNEL
/* the service procedures */
extern void acl2_getacl(GETACL2args *, GETACL2res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl2_getacl_getfh(GETACL2args *);
extern void acl2_getacl_free(GETACL2res *);
extern void acl2_setacl(SETACL2args *, SETACL2res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl2_setacl_getfh(SETACL2args *);
extern void acl2_getattr(GETATTR2args *, GETATTR2res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl2_getattr_getfh(GETATTR2args *);
extern void acl2_access(ACCESS2args *, ACCESS2res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl2_access_getfh(ACCESS2args *);
extern void acl3_getacl(GETACL3args *, GETACL3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl3_getacl_getfh(GETACL3args *);
extern void acl3_getacl_free(GETACL3res *);
extern void acl3_setacl(SETACL3args *, SETACL3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *acl3_setacl_getfh(SETACL3args *);
#endif

#ifdef _KERNEL
/* the client side procedures */
extern int acl_getacl2(vnode_t *, vsecattr_t *, int, cred_t *);
extern int acl_setacl2(vnode_t *, vsecattr_t *, int, cred_t *);
extern int acl_getattr2_otw(vnode_t *, vattr_t *, cred_t *);
extern int acl_access2(vnode_t *, int, int, cred_t *);
extern int acl_getacl3(vnode_t *, vsecattr_t *, int, cred_t *);
extern int acl_setacl3(vnode_t *, vsecattr_t *, int, cred_t *);
extern int acl2call(mntinfo_t *, rpcproc_t, xdrproc_t, caddr_t, xdrproc_t,
			caddr_t, cred_t *, int *, enum nfsstat *, int,
			failinfo_t *);
extern int acl3call(mntinfo_t *, rpcproc_t, xdrproc_t, caddr_t, xdrproc_t,
			caddr_t, cred_t *, int *, nfsstat3 *, int,
			failinfo_t *);
extern void nfs_acl_free(vsecattr_t *);
#endif

#ifdef _KERNEL
/* server and client data structures */
extern kstat_named_t	*aclproccnt_v2_ptr;
extern uint_t		aclproccnt_v2_ndata;
extern kstat_named_t	*aclproccnt_v3_ptr;
extern uint_t		aclproccnt_v3_ndata;

extern kstat_named_t	*aclreqcnt_v2_ptr;
extern uint_t		aclreqcnt_v2_ndata;
extern char		*aclnames_v2[];
extern char		acl_call_type_v2[];
extern char		acl_ss_call_type_v2[];
extern char		acl_timer_type_v2[];
extern kstat_named_t	*aclreqcnt_v3_ptr;
extern uint_t		aclreqcnt_v3_ndata;
extern char		*aclnames_v3[];
extern char		acl_call_type_v3[];
extern char		acl_ss_call_type_v3[];
extern char		acl_timer_type_v3[];
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_NFS_ACL_H */
