/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Generated using SVR4 version of mount.x.
 */

#ifndef _MOUNT_H_RPCGEN
#define	_MOUNT_H_RPCGEN

#pragma ident	"@(#)mount.h	1.5	99/02/23 SMI"

#include <rpc/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MNTPATHLEN 1024
#define	MNTNAMLEN 255
#define	FHSIZE 32

typedef char fhandle[FHSIZE];
bool_t xdr_fhandle();

struct fhstatus {
	u_int fhs_status;
	union {
		fhandle fhs_fhandle;
	} fhstatus_u;
};
typedef struct fhstatus fhstatus;
bool_t xdr_fhstatus();

typedef char *dirpath;
bool_t xdr_dirpath();

typedef char *name;
bool_t xdr_name();

typedef struct mountbody *mountlist;
bool_t xdr_mountlist();

struct mountbody {
	name ml_hostname;
	dirpath ml_directory;
	mountlist ml_next;
};
typedef struct mountbody mountbody;
bool_t xdr_mountbody();

typedef struct groupnode *groups;
bool_t xdr_groups();

struct groupnode {
	name gr_name;
	groups gr_next;
};
typedef struct groupnode groupnode;
bool_t xdr_groupnode();

typedef struct exportnode *exports;
bool_t xdr_exports();

struct exportnode {
	dirpath ex_dir;
	groups ex_groups;
	exports ex_next;
};
typedef struct exportnode exportnode;
bool_t xdr_exportnode();

#define	MOUNTPROG ((rpcprog_t)100005)
#define	MOUNTVERS ((rpcvers_t)1)
#define	MOUNTPROC_NULL ((rpcproc_t)0)
extern void * mountproc_null_1();
#define	MOUNTPROC_MNT ((rpcproc_t)1)
extern fhstatus * mountproc_mnt_1();
#define	MOUNTPROC_DUMP ((rpcproc_t)2)
extern mountlist * mountproc_dump_1();
#define	MOUNTPROC_UMNT ((rpcproc_t)3)
extern void * mountproc_umnt_1();
#define	MOUNTPROC_UMNTALL ((rpcproc_t)4)
extern void * mountproc_umntall_1();
#define	MOUNTPROC_EXPORT ((rpcproc_t)5)
extern exports * mountproc_export_1();
#define	MOUNTPROC_EXPORTALL ((rpcproc_t)6)
extern exports * mountproc_exportall_1();

#ifdef	__cplusplus
}
#endif

#endif /* !_MOUNT_H_RPCGEN */
