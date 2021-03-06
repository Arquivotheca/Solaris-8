/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _UFS_PROT_H_RPCGEN
#define	_UFS_PROT_H_RPCGEN

#pragma ident	"@(#)ufs_prot.h	1.11	98/01/06 SMI"

#include <rpc/rpc.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/fs/ufs_fs.h>
#include <sys/types.h>
#include <sys/errno.h>

enum ufsdrc_t {
	UFSDRC_OK = 0,
	UFSDRC_NOENT = ENOENT,
	UFSDRC_PERM = EPERM,
	UFSDRC_INVAL = EINVAL,
	UFSDRC_NOEXEC = ENOEXEC,
	UFSDRC_NODEV = ENODEV,
	UFSDRC_NXIO = ENXIO,
	UFSDRC_BUSY = EBUSY,
	UFSDRC_OPNOTSUP = EOPNOTSUPP,
	UFSDRC_EXECERR = 254,
	UFSDRC_ERR = 255
};
typedef enum ufsdrc_t ufsdrc_t;

struct fs_identity_t {
	dev32_t fs_dev;
	char *fs_name;
};
typedef struct fs_identity_t fs_identity_t;

struct ufsd_repairfs_args_t {
	fs_identity_t ua_fsid;
	uint_t ua_attempts;
};
typedef struct ufsd_repairfs_args_t ufsd_repairfs_args_t;

struct ufsd_repairfs_list_t {
	int ual_listlen;
	ufsd_repairfs_args_t *ual_list;
};
typedef struct ufsd_repairfs_list_t ufsd_repairfs_list_t;

enum ufsd_event_t {
	UFSDEV_NONE = 0,
	UFSDEV_REBOOT = 0 + 1,
	UFSDEV_FSCK = 0 + 2,
	UFSDEV_LOG_OP = 0 + 3
};
typedef enum ufsd_event_t ufsd_event_t;

enum ufsd_boot_type_t {
	UFSDB_NONE = 0,
	UFSDB_CLEAN = 0 + 1,
	UFSDB_POSTPANIC = 0 + 2
};
typedef enum ufsd_boot_type_t ufsd_boot_type_t;

enum ufsd_log_op_t {
	UFSDLO_NONE = 0,
	UFSDLO_COMMIT = 0 + 1,
	UFSDLO_GET = 0 + 2,
	UFSDLO_PUT = 0 + 3,
	UFSDLO_RESET = 0 + 4
};
typedef enum ufsd_log_op_t ufsd_log_op_t;

enum ufsd_fsck_state_t {
	UFSDFS_NONE = 0,
	UFSDFS_DISPATCH = 0 + 1,
	UFSDFS_ERREXIT = 0 + 2,
	UFSDFS_SUCCESS = 0 + 3
};
typedef enum ufsd_fsck_state_t ufsd_fsck_state_t;
#define	UFSD_VARMSGMAX 1024
#define	UFSD_SPAREMSGBYTES 4

struct ufsd_log_data_t {
	int umld_eob;
	int umld_seq;
	struct {
		uint_t umld_buf_len;
		char *umld_buf_val;
	} umld_buf;
};
typedef struct ufsd_log_data_t ufsd_log_data_t;

struct ufsd_log_msg_t {
	ufsd_log_op_t um_lop;
	union {
		ufsd_log_data_t um_logdata;
	} ufsd_log_msg_t_u;
};
typedef struct ufsd_log_msg_t ufsd_log_msg_t;

struct ufsd_msg_vardata_t {
	ufsd_event_t umv_ev;
	union {
		ufsd_boot_type_t umv_b;
		ufsd_fsck_state_t umv_fs;
		ufsd_log_msg_t umv_lm;
	} ufsd_msg_vardata_t_u;
};
typedef struct ufsd_msg_vardata_t ufsd_msg_vardata_t;

struct ufsd_msg_t {
	time32_t um_time;
	uint_t um_from;
	struct {
		uint_t um_spare_len;
		char *um_spare_val;
	} um_spare;
	ufsd_msg_vardata_t um_var;
};
typedef struct ufsd_msg_t ufsd_msg_t;
#define	UFSD_SERVNAME	"ufsd"
#define	xdr_dev_t	xdr_u_int
#define	xdr_time_t	xdr_int
/*
 * Set UFSD_THISVERS to the newest version of the protocol
 * This allows the preprocessor to force an error if the
 * protocol changes, since the kernel xdr routines may need to be
 * recoded.  Note that we can't explicitly set the version to a
 * symbol as rpcgen will then create erroneous routine names.
 */
#define	UFSD_V1			1
#define	UFSD_ORIGVERS		UFSD_V1
#define	UFSD_THISVERS		1

#define	UFSD_PROG ((unsigned long)(100233))
#define	UFSD_VERS ((unsigned long)(1))

#if defined(__STDC__) || defined(__cplusplus)
#define	UFSD_NULL ((unsigned long)(0))
extern  ufsdrc_t *ufsd_null_1(void *, CLIENT *);
extern  ufsdrc_t *ufsd_null_1_svc(void *, struct svc_req *);
#define	UFSD_REPAIRFS ((unsigned long)(1))
extern  ufsdrc_t *ufsd_repairfs_1(ufsd_repairfs_args_t *, CLIENT *);
extern  ufsdrc_t *
		ufsd_repairfs_1_svc(ufsd_repairfs_args_t *, struct svc_req *);
#define	UFSD_REPAIRFSLIST ((unsigned long)(2))
extern  ufsdrc_t *ufsd_repairfslist_1(ufsd_repairfs_list_t *, CLIENT *);
extern  ufsdrc_t *
	ufsd_repairfslist_1_svc(ufsd_repairfs_list_t *, struct svc_req *);
#define	UFSD_SEND ((unsigned long)(3))
extern  ufsdrc_t *ufsd_send_1(ufsd_msg_t *, CLIENT *);
extern  ufsdrc_t *ufsd_send_1_svc(ufsd_msg_t *, struct svc_req *);
#define	UFSD_RECV ((unsigned long)(4))
extern  ufsdrc_t *ufsd_recv_1(ufsd_msg_t *, CLIENT *);
extern  ufsdrc_t *ufsd_recv_1_svc(ufsd_msg_t *, struct svc_req *);
#define	UFSD_EXIT ((unsigned long)(5))
extern  ufsdrc_t *ufsd_exit_1(void *, CLIENT *);
extern  ufsdrc_t *ufsd_exit_1_svc(void *, struct svc_req *);
extern int ufsd_prog_1_freeresult(SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define	UFSD_NULL ((unsigned long)(0))
extern  ufsdrc_t *ufsd_null_1();
extern  ufsdrc_t *ufsd_null_1_svc();
#define	UFSD_REPAIRFS ((unsigned long)(1))
extern  ufsdrc_t *ufsd_repairfs_1();
extern  ufsdrc_t *ufsd_repairfs_1_svc();
#define	UFSD_REPAIRFSLIST ((unsigned long)(2))
extern  ufsdrc_t *ufsd_repairfslist_1();
extern  ufsdrc_t *ufsd_repairfslist_1_svc();
#define	UFSD_SEND ((unsigned long)(3))
extern  ufsdrc_t *ufsd_send_1();
extern  ufsdrc_t *ufsd_send_1_svc();
#define	UFSD_RECV ((unsigned long)(4))
extern  ufsdrc_t *ufsd_recv_1();
extern  ufsdrc_t *ufsd_recv_1_svc();
#define	UFSD_EXIT ((unsigned long)(5))
extern  ufsdrc_t *ufsd_exit_1();
extern  ufsdrc_t *ufsd_exit_1_svc();
extern int ufsd_prog_1_freeresult();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_ufsdrc_t(XDR *, ufsdrc_t *);
extern  bool_t xdr_fs_identity_t(XDR *, fs_identity_t *);
extern  bool_t xdr_ufsd_repairfs_args_t(XDR *, ufsd_repairfs_args_t *);
extern  bool_t xdr_ufsd_repairfs_list_t(XDR *, ufsd_repairfs_list_t *);
extern  bool_t xdr_ufsd_event_t(XDR *, ufsd_event_t *);
extern  bool_t xdr_ufsd_boot_type_t(XDR *, ufsd_boot_type_t *);
extern  bool_t xdr_ufsd_log_op_t(XDR *, ufsd_log_op_t *);
extern  bool_t xdr_ufsd_fsck_state_t(XDR *, ufsd_fsck_state_t *);
extern  bool_t xdr_ufsd_log_data_t(XDR *, ufsd_log_data_t *);
extern  bool_t xdr_ufsd_log_msg_t(XDR *, ufsd_log_msg_t *);
extern  bool_t xdr_ufsd_msg_vardata_t(XDR *, ufsd_msg_vardata_t *);
extern  bool_t xdr_ufsd_msg_t(XDR *, ufsd_msg_t *);

#else /* K&R C */
extern bool_t xdr_ufsdrc_t();
extern bool_t xdr_fs_identity_t();
extern bool_t xdr_ufsd_repairfs_args_t();
extern bool_t xdr_ufsd_repairfs_list_t();
extern bool_t xdr_ufsd_event_t();
extern bool_t xdr_ufsd_boot_type_t();
extern bool_t xdr_ufsd_log_op_t();
extern bool_t xdr_ufsd_fsck_state_t();
extern bool_t xdr_ufsd_log_data_t();
extern bool_t xdr_ufsd_log_msg_t();
extern bool_t xdr_ufsd_msg_vardata_t();
extern bool_t xdr_ufsd_msg_t();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_UFS_PROT_H_RPCGEN */
