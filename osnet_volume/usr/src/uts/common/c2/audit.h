/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * File history:
 * @(#)audit.h 2.16 92/02/29 SMI; SunOS CMW
 * @(#)audit.h 4.2.1.2 91/05/08 SMI; BSM Module
 * @(#)audit_stat.h 2.4 91/08/10 SMI; SunOS CMW
 *
 * This file contains the declarations of the various data structures
 * used by the auditing module(s).
 */

#ifndef	_BSM_AUDIT_H
#define	_BSM_AUDIT_H

#pragma ident	"@(#)audit.h	1.67	99/10/14 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/shm.h>	/* for shmid_ds structure */
#include <sys/sem.h>	/* for semid_ds structure */
#include <sys/msg.h>	/* for msqid_ds structure */

/*
 * Audit conditions, statements reguarding what's to be done with
 * audit records.
 */
#define	AUC_UNSET	0	/* on/off hasn't been decided */
#define	AUC_AUDITING	1	/* auditing is being done */
#define	AUC_NOAUDIT	2	/* auditing is not being done */
#define	AUC_DISABLED	-1	/* audit module loaded but not enabled */

/*
 * The user id -2 is never audited - in fact, a setauid(AU_NOAUDITID)
 * will turn off auditing.
 */
#define	AU_NOAUDITID	-2

/*
 * The classes of audit events
 */

#define	AU_NULL		0x00000000	/* no class */
#define	AU_FREAD	0x00000001	/* Filesystem object read access */
#define	AU_FWRITE	0x00000002	/* Filesystem object write access */
#define	AU_FACCESS	0x00000004	/* Filesystem object attribute access */
#define	AU_FMODIFY	0x00000008	/* Filesystem object attribute mod */
#define	AU_FCREATE	0x00000010	/* Filesystem object creation */
#define	AU_FDELETE	0x00000020	/* Filesystem object deletion */
#define	AU_CLOSE	0x00000040	/* File Close */
#define	AU_PROCESS	0x00000080	/* process events (fork,exec,exit) */
#define	AU_NET		0x00000100	/* network actions (bind,accept) */
#define	AU_IPC		0x00000200	/* System V IPC */
#define	AU_NONAT	0x00000400	/* Non-attributable events */
#define	AU_ADMIN	0x00000800	/* administrative actions */
#define	AU_LOGIN	0x00001000	/* login/logout related events */
#define	AU_TFM		0x00002000	/* Trusted Facility Mgmt. Actions */
#define	AU_APPL		0x00004000	/* Application actions */
#define	AU_SETL		0x00008000	/* set label (SL or IL) actions */
#define	AU_IFLOAT	0x00010000	/* Information label floating */
#define	AU_PRIV		0x00020000	/* Use of Privilege */
#define	AU_MAC_RW	0x00040000	/* MAC read/write failure */
#define	AU_XCONN	0x00080000	/* X Windows connection */
#define	AU_XCREATE	0x00100000	/* X Windows create object */
#define	AU_XDELETE	0x00200000	/* X Windows delete object */
#define	AU_XIFLOAT	0x00400000	/* X Windows IL floating */
#define	AU_XPRIVS	0x00800000	/* X Windows Use of Privilege Success */
#define	AU_XPRIVF	0x01000000	/* X Windows Use of Privilege Failure */
#define	AU_XMOVE	0x02000000	/* X Windows Interwindow Data Moves */
#define	AU_XDACF	0x04000000	/* X Windows DAC violation */
#define	AU_XMACF	0x08000000	/* X Windows MAC violation */
#define	AU_XSECATTR	0x10000000	/* X Security Attribute Change */
#define	AU_IOCTL	0x20000000	/* ioctl */
#define	AU_EXEC		0x40000000	/* exec and exece events */
#define	AU_OTHER	0x80000000u	/* other category */
#define	AU_ALL		0xffffffffu	/* Everything */

/*
 * Defines for event modifier field
 */
#define	PAD_MACUSE	0x0001		/* mac used/not used */
#define	PAD_MACREAD	0x0002		/* mac read   check (1) */
#define	PAD_MACWRITE	0x0004		/* mac write  check (2) */
#define	PAD_MACSEARCH	0x0008		/* mac search check (3) */
#define	PAD_MACKILL	0x0010		/* mac kill   check (4) */
#define	PAD_MACTRACE	0x0020		/* mac trace  check (5) */
#define	PAD_MACIOCTL	0x0040		/* mac ioctl  check (6) */

#define	PAD_MACMASK	0x007e		/* mask to get mac failure mode */

#define	PAD_SPRIVUSE	0x0080		/* successfully used privileged */
#define	PAD_FPRIVUSE	0x0100		/* failed use of privileged */
#define	PAD_NONATTR	0x4000		/* non-attributable event */
#define	PAD_FAILURE	0x8000		/* fail audit event */

/*
 * Some typedefs for the fundamentals
 */
typedef pid_t  au_asid_t;
typedef uint_t  au_class_t;
typedef short au_event_t;
typedef short au_emod_t;
typedef uid_t au_id_t;

/*
 * An audit event mask.
 */
struct au_mask {
	unsigned int	am_success;	/* success bits */
	unsigned int	am_failure;	/* failure bits */
};
typedef struct au_mask au_mask_t;
#define	as_success am_success
#define	as_failure am_failure

/*
 * The structure of the terminal ID (ipv4)
 */
struct au_tid {
	dev_t port;
	uint_t machine;
};

#if defined(_SYSCALL32)
struct au_tid32 {
	uint_t port;
	uint_t machine;
};

typedef struct au_tid32 au_tid32_t;
#endif

typedef struct au_tid au_tid_t;

/*
 * The structure of the terminal ID (ipv6)
 */
struct au_tid_addr {
	dev_t  at_port;
	uint_t at_type;
	uint_t at_addr[4];
};

#if defined(_SYSCALL32)
struct au_tid_addr32 {
	uint_t at_port;
	uint_t at_type;
	uint_t at_addr[4];
};

typedef struct au_tid_addr32 au_tid32_addr_t;
#endif

typedef struct au_tid_addr au_tid_addr_t;

/*
 * at_type values - address length used to identify address type
 */
#define	AU_IPv4 4	/* ipv4 type IP address */
#define	AU_IPv6 16	/* ipv6 type IP address */

/*
 * Compatability with SunOS 4.x BSM module
 *
 * New code should not contain audit_state_t,
 * au_state_t, nor au_termid as these type
 * may go away in future releases.
 *
 * typedef new-5.x-bsm-name old-4.x-bsm-name
 */

typedef au_class_t au_state_t;
typedef au_mask_t audit_state_t;
typedef au_id_t auid_t;
#define	ai_state ai_mask;

/*
 * Opcodes for bsm system calls
 */

#define	BSM_GETAUID		19
#define	BSM_SETAUID		20
#define	BSM_GETAUDIT		21
#define	BSM_SETAUDIT		22
#define	BSM_GETUSERAUDIT	23
#define	BSM_SETUSERAUDIT	24
#define	BSM_AUDIT		25
#define	BSM_AUDITUSER		26
#define	BSM_AUDITSVC		27
#define	BSM_AUDITON		28
#define	BSM_AUDITCTL		29
#define	BSM_GETKERNSTATE	30
#define	BSM_SETKERNSTATE	31
#define	BSM_GETPORTAUDIT	32
#define	BSM_REVOKE		33
#define	BSM_AUDITSTAT		34
#define	BSM_GETAUDIT_ADDR	35
#define	BSM_SETAUDIT_ADDR	36

/*
 * Auditctl(2) commands
 */
#define	A_GETPOLICY	2	/* get audit policy */
#define	A_SETPOLICY	3	/* set audit policy */
#define	A_GETKMASK	4	/* get kernel event preselection mask */
#define	A_SETKMASK	5	/* set kernel event preselection mask */
#define	A_GETQCTRL	6	/* get kernel audit queue ctrl parameters */
#define	A_SETQCTRL	7	/* set kernel audit queue ctrl parameters */
#define	A_GETCWD	8	/* get process current working directory */
#define	A_GETCAR	9	/* get process current active root */
#define	A_GETSTAT	12	/* get audit statistics */
#define	A_SETSTAT	13	/* (re)set audit statistics */
#define	A_SETUMASK	14	/* set preselection mask for procs with auid */
#define	A_SETSMASK	15	/* set preselection mask for procs with asid */
#define	A_GETCOND	20	/* get audit system on/off condition */
#define	A_SETCOND	21	/* set audit system on/off condition */
#define	A_GETCLASS	22	/* get audit event to class mapping */
#define	A_SETCLASS	23	/* set audit event to class mapping */
#define	A_GETPINFO	24	/* get audit info for an arbitrary pid */
#define	A_SETPMASK	25	/* set preselection mask for an given pid */
#define	A_SETFSIZE	26	/* set audit file size */
#define	A_GETFSIZE	27	/* get audit file size */
#define	A_GETPINFO_ADDR	28	/* get audit info for an arbitrary pid */
#define	A_GETKAUDIT	29	/* get kernel audit characteristics */
#define	A_SETKAUDIT	30	/* set kernel audit characteristics */

/*
 * Audit Policy paramaters
 */
#define	AUDIT_CNT	0x0001 /* do NOT sleep undelivered synchronous events */
#define	AUDIT_AHLT	0x0002	/* HALT machine on undelivered async event */
#define	AUDIT_ARGV	0x0004	/* include argv with execv system call events */
#define	AUDIT_ARGE	0x0008	/* include arge with execv system call events */
#define	AUDIT_PASSWD	0x0010	/* include bad password with "login" events */
#define	AUDIT_SEQ	0x0020	/* DEBUG option: include sequence attribute */
#define	AUDIT_WINDATA	0x0040	/* include interwindow moved data */
#define	AUDIT_USER	0x0080	/* make audituser(2) un-privileged */
#define	AUDIT_GROUP	0x0100	/* include group attribute with each record */
#define	AUDIT_TRAIL	0X0200	/* include trailer token */
#define	AUDIT_PATH	0x0400	/* allow multiple paths per event */

/*
 * Kernel audit queue control parameters
 *
 *	audit record recording blocks at hiwater # undelived records
 *	audit record recording resumes at lowwater # undelivered audit records
 *	bufsz determines how big the data xfers will be to the audit trail
 */
struct au_qctrl {
	size_t	aq_hiwater;	/* kernel audit queue, high water mark */
	size_t	aq_lowater;	/* kernel audit queue, low  water mark */
	size_t	aq_bufsz;	/* kernel audit queue, write size to trail */
	clock_t	aq_delay;	/* delay before flushing audit queue */
};

#if defined(_SYSCALL32)
struct au_qctrl32 {
	size32_t	aq_hiwater;
	size32_t	aq_lowater;
	size32_t	aq_bufsz;
	clock32_t	aq_delay;
};
#endif


/*
 * default values of hiwater and lowater (note hi > lo)
 */
#define	AQ_HIWATER  100
#define	AQ_MAXHIGH  10000
#define	AQ_LOWATER  10
#define	AQ_BUFSZ    1024
#define	AQ_MAXBUFSZ 1048576
#define	AQ_DELAY    20
#define	AQ_MAXDELAY 20000

struct auditinfo {
	au_id_t		ai_auid;
	au_mask_t	ai_mask;
	au_tid_t	ai_termid;
	au_asid_t	ai_asid;
};

#if defined(_SYSCALL32)
struct auditinfo32 {
	au_id_t		ai_auid;
	au_mask_t	ai_mask;
	au_tid32_t	ai_termid;
	au_asid_t	ai_asid;
};

typedef struct auditinfo32 auditinfo32_t;
#endif

typedef struct auditinfo auditinfo_t;

struct auditinfo_addr {
	au_id_t		ai_auid;
	au_mask_t	ai_mask;
	au_tid_addr_t	ai_termid;
	au_asid_t	ai_asid;
};

#if defined(_SYSCALL32)
struct auditinfo_addr32 {
	au_id_t		ai_auid;
	au_mask_t	ai_mask;
	au_tid32_addr_t	ai_termid;
	au_asid_t	ai_asid;
};

typedef struct auditinfo_addr32 auditinfo32_addr_t;
#endif

typedef struct auditinfo_addr auditinfo_addr_t;

struct auditpinfo {
	pid_t		ap_pid;
	au_id_t		ap_auid;
	au_mask_t	ap_mask;
	au_tid_t	ap_termid;
	au_asid_t	ap_asid;
};

#if defined(_SYSCALL32)
struct auditpinfo32 {
	pid_t		ap_pid;
	au_id_t		ap_auid;
	au_mask_t	ap_mask;
	au_tid32_t	ap_termid;
	au_asid_t	ap_asid;
};
#endif


struct auditpinfo_addr {
	pid_t		ap_pid;
	au_id_t		ap_auid;
	au_mask_t	ap_mask;
	au_tid_addr_t	ap_termid;
	au_asid_t	ap_asid;
};

#if defined(_SYSCALL32)
struct auditpinfo_addr32 {
	pid_t		ap_pid;
	au_id_t		ap_auid;
	au_mask_t	ap_mask;
	au_tid32_addr_t	ap_termid;
	au_asid_t	ap_asid;
};
#endif


struct au_evclass_map {
	au_event_t	ec_number;
	au_class_t	ec_class;
};
typedef struct au_evclass_map au_evclass_map_t;

/*
 * Audit stat structures (used to be in audit_stat.h
 */

struct audit_stat {
	unsigned int as_version;	/* version of kernel audit code */
	unsigned int as_numevent;	/* number of kernel audit events */
	int as_generated;	/* # records processed */
	int as_nonattrib;	/* # non-attributed records produced */
	int as_kernel;		/* # records produced by kernel */
	int as_audit;		/* # records processed by audit(2) */
	int as_auditctl;	/* # records processed by auditctl(2) */
	int as_enqueue;		/* # records put onto audit queue */
	int as_written;		/* # records written to audit trail */
	int as_wblocked;	/* # times write blked on audit queue */
	int as_rblocked;	/* # times read blked on audit queue */
	int as_dropped;		/* # of dropped audit records */
	int as_totalsize;	/* total number bytes of audit data */
	unsigned int as_memused;	/* amount of memory used by auditing */
};
typedef struct audit_stat au_stat_t;
extern au_stat_t audit_statistics;

/*
 * Secondary stat structure for file size stuff.  The stat structure was
 * not combined to preserve the semantics of the 5.1 - 5.3 A_GETSTAT call
 */
struct audit_fstat {
	unsigned int af_filesz;
	unsigned int af_currsz;
};
typedef struct audit_fstat au_fstat_t;
extern au_fstat_t audit_file_stat;

#define	AS_INC(a, b) {mutex_enter(&au_stat_lock); \
			audit_statistics. /* */a += b; \
			mutex_exit(&au_stat_lock); }
#define	AS_DEC(a, b) {mutex_enter(&au_stat_lock); \
			audit_statistics. /* */a -= b; \
			mutex_exit(&au_stat_lock); }

/*
 * audit token IPC types (shm, sem, msg) [for ipc attribute]
 */

#define	AT_IPC_MSG	((char)1)		/* message IPC id */
#define	AT_IPC_SEM	((char)2)		/* semaphore IPC id */
#define	AT_IPC_SHM	((char)3)		/* shared memory IPC id */

#if defined(_KERNEL)

#ifdef __cplusplus
}
#endif

#include <sys/types.h>
#include <sys/model.h>
#include <sys/proc.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/file.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <netinet/in.h>
#include <c2/audit_kernel.h>
struct fcntla;

#ifdef __cplusplus
extern "C" {
#endif

struct auditcalls {
	long	code;
	long	a1;
	long	a2;
	long	a3;
	long	a4;
	long	a5;
};

int	audit(caddr_t, int);
int	_audit(caddr_t, int);
int	auditsys(struct auditcalls *, union rval *); /* fake stub */
int	_auditsys(struct auditcalls *, union rval *); /* real deal */
void	audit_init(void);
void	audit_newproc(struct proc *);
void	audit_pfree(struct proc *);
void	audit_thread_create(kthread_id_t, int);
void	audit_thread_free(kthread_id_t);
int	audit_savepath(struct pathname *, struct vnode *, int, cred_t *);
void	audit_addcomponent(struct pathname *);
void	audit_anchorpath(struct pathname *, int);
void	audit_symlink(struct pathname *, struct pathname *);
void	audit_attributes(struct vnode *);
void	audit_falloc(struct file *);
void	audit_unfalloc(struct file *);
void	audit_exit(int, int);
void	audit_core_start(int);
void	audit_core_finish(int);
void	audit_stropen(struct vnode *, dev_t *, int, struct cred *);
void	audit_strclose(struct vnode *, int, struct cred *);
void	audit_strioctl(struct vnode *, int, intptr_t, int, int, struct cred *,
		int *);
void	audit_strgetmsg(struct vnode *, struct strbuf *, struct strbuf *,
		unsigned char *, int *, int);
void	audit_strputmsg(struct vnode *, struct strbuf *, struct strbuf *,
		unsigned char, int, int);
void	audit_closef(struct file *);
int	audit_getf(int);
void	audit_setf(struct file *, int);
void	audit_copen(int, struct file *, struct vnode *);
void	audit_reboot(void);
void	audit_vncreate_start(void);
void	audit_vncreate_finish(struct vnode *, int);
void	audit_exec(const char *, const char *, ssize_t, ssize_t);
void	audit_enterprom(int);
void	audit_exitprom(int);
void	audit_suser(int);
void	audit_chdirec(struct vnode *, struct vnode **);
void	audit_sock(int, struct queue *, struct msgb *, int);
void	audit_free(void);
int	audit_start(unsigned int, unsigned int, int, klwp_t *);
void	audit_finish(unsigned int, unsigned int, int, union rval *);
int	audit_sync_block(void);
int	audit_async_block(void);
int	audit_success(struct t_audit_data *, int);
int	auditme(struct t_audit_data *, au_state_t);
uint_t	audit_fixpath(char *, uint_t);
void	audit_ipc(int, int, void *);
void	audit_ipcget(int, void *);
void	audit_lookupname();
int	audit_pathcomp(struct pathname *, vnode_t *, cred_t *);
void	audit_fdsend(int, struct file *, int);
void	audit_fdrecv(int, struct file *);


int	audit_c2_revoke(struct fcntla *, rval_t *);

struct cwrd *getcw(void);
struct cwrd *cwdup(struct cwrd *, int);
void	bsm_cwincr(struct cwrd *);
void	bsm_cwfree(struct cwrd *);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _BSM_AUDIT_H */
