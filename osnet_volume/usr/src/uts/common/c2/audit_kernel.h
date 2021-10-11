/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * File history:
 * @(#)audit_data.h 2.18 92/02/24 SMI; SunOS CMW
 * @(#)audit_data.h 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * This file contains the basic auditing control structure definitions.
 */

#ifndef _BSM_AUDIT_KERNEL_H
#define	_BSM_AUDIT_KERNEL_H

#pragma ident	"@(#)audit_kernel.h	1.24	99/10/14 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This table contains the mapping from the system call ID to a corresponding
 * audit event.
 *
 *   au_init() is a function called at the beginning of the system call that
 *   performs any necessary setup/processing. It maps the call into the
 *   appropriate event, depending on the system call arguments. It is called
 *   by audit_start() from trap.c .
 *
 *   au_event is the audit event associated with the system call. Most of the
 *   time it will map directly from the system call i.e. There is one system
 *   call associated with the event. In some cases, such as shmsys, or open,
 *   the au_start() function will map the system call to more than one event,
 *   depending on the system call arguments.
 *
 *   au_start() is a function that provides per system call processing at the
 *   beginning of a system call. It is mainly concerned with preseving the
 *   audit record components that may be altered so that we can determine
 *   what the original paramater was before as well as after the system call.
 *   It is possible that au_start() may be taken away. It might be cleaner to
 *   define flags in au_ctrl to save a designated argument. For the moment we
 *   support both mechanisms, however the use of au_start() will be reviewed
 *   for 4.1.1 and CMW and ZEUS to see if such a general method is justified.
 *
 *   au_finish() is a function that provides per system call processing at the
 *   completion of a system call. In certain circumstances, the type of audit
 *   event depends on intermidiate results during the processing of the system
 *   call. It is called in audit_finish() from trap.c .
 *
 *   au_ctrl is a control vector that indicates what processing might have to
 *   be performed, even if there is no auditing for this system call. At
 *   present this is mostly for path processing for chmod, chroot. We need to
 *   process the path information in vfs_lookup, even when we are not auditing
 *   the system call in the case of chdir and chroot.
 */
/*
 * Defines for au_ctrl
 */
#define	S2E_SP  0x001	/* save path for later use */
#define	S2E_MLD 0x002	/* only one lookup per system call */
#define	S2E_NPT 0x004	/* force no path in audit record */

/*
 * At present, we are using the audit classes imbedded with in the kernel. Each
 * event has a bit mask determining which classes the event is associated.
 * The table audit_e2s maps the audit event ID to the audit state.
 *
 * Note that this may change radically. If we use a bit vector for the audit
 * class, we can allow granularity at the event ID for each user. In this
 * case, the vector would be determined at user level and passed to the kernel
 * via the setaudit system call.
 */

/*
 * The crd structure hold the current root and directory path names for the
 * process. Whenever a chdir or chroot occures, the paths contained in the
 * structure will be update. The reference count minimizes data copies since
 * the process's current directory changes very seldom.
 *
 * Note that the string length may not match the buffer length since we
 * canonicalize the pathnames to prevent them from growing without bound.
 */
struct cwrd {
	int	cwrd_ref;	/* reference count */
	uint_t   cwrd_ldbuf;	/* length of directory buffer */
	uint_t   cwrd_dirlen;	/* length of directory path (+ \0) */
	caddr_t cwrd_dir;	/* directory */
	uint_t   cwrd_lrbuf;	/* length of root buffer */
	uint_t   cwrd_rootlen;	/* length of root path (+ \0) */
	caddr_t cwrd_root;	/* root directory */
};

/*
 * The structure of the terminal ID within the kernel is different from the
 * terminal ID in user space. It is a combination of port and IP address.
 */

struct au_termid {
	dev_t	at_port;
	uint_t	at_type;
	uint_t	at_addr[4];
};
typedef struct au_termid au_termid_t;

/*
 * The structure p_audit_data hangs off of the process structure. It contains
 * all of the audit information necessary to manage the audit record generation
 * for each process.
 *
 */

struct p_audit_data {
	au_id_t		pad_auid;	/* audit ID */
	au_mask_t	pad_mask;	/* audit state */
	au_termid_t	pad_termid;	/* terminal ID */
	au_asid_t	pad_asid;	/* audit session id */
	struct cwrd	*pad_cwrd;	/* current working root/cwd paths */
	kmutex_t	pad_lock;	/* lock pad data during changes */
	struct p_audit_data *next;	/* next empty structure */
};
typedef struct p_audit_data p_audit_data_t;

/*
 * Defines for pad_ctrl
 */
#define	PAD_SAVPATH 	0x00000001	/* save path for further processing */
#define	PAD_MLD		0x00000002	/* system call involves MLD */
#define	PAD_NOPATH  	0x00000004	/* force no paths in audit record */
#define	PAD_ABSPATH 	0x00000008	/* path from lookup is absolute */
#define	PAD_NOATTRB 	0x00000010	/* do not automatically add attribute */
#define	PAD_SUSEROK 	0x00000020	/* suser() success */
#define	PAD_SUSERNO 	0x00000040	/* suser() failed */
#define	PAD_LFLOAT  	0x00000080	/* Label float */
#define	PAD_NOAUDIT 	0x00000100	/* discard audit record */
#define	PAD_PATHFND 	0x00000200	/* found path, don't retry lookup */
#define	PAD_SPRIV   	0x00000400	/* succ priv use. extra audit_finish */
#define	PAD_FPRIV   	0x00000800	/* fail priv use. extra audit_finish */
#define	PAD_SMAC    	0x00001000	/* succ mac use. extra audit_finish */
#define	PAD_FMAC    	0x00002000	/* fail mac use. extra audit_finish */
#define	PAD_AUDITME 	0x00004000	/* audit me because of NFS operation */
#define	PAD_IFLOAT  	0x00008000	/* label floated. extra audit_finish */
#define	PAD_TRUE_CREATE 0x00010000	/* true create, file not found */
#define	PAD_CORE	0x00020000	/* save attribute during core dump */

/*
 * The structure t_audit_data hangs off of the thread structure. It contains
 * all of the audit information necessary to manage the audit record generation
 * for each thread.
 *
 */

struct t_audit_data {
	kthread_id_t  tad_thread;	/* DEBUG pointer to parent thread */
	unsigned int  tad_scid;		/* system call ID for finish */
	short	tad_event;	/* event for audit record */
	short	tad_evmod;	/* event modifier for audit record */
	int	tad_ctrl;	/* audit control/status flags */
	int	tad_flag;	/* to audit or not to audit */
	uint_t	tad_pathlen;	/* saved length from vfs_lookup */
	caddr_t tad_path;	/* saved path from vfs_lookup */
	struct vnode *tad_vn;	/* saved inode from vfs_lookup */
	caddr_t tad_ad;		/* base of accumulated audit data */
	struct t_audit_data *next;	/* next empty structure */
};
typedef struct t_audit_data t_audit_data_t;

/*
 * The f_audit_data structure hangs off of the file structure. It contains
 * three fields of data. The audit ID, the audit state, and a path name.
 */

struct f_audit_data {
	kthread_id_t	fad_thread;	/* DEBUG creating thread */
	au_id_t		fad_auid;	/* audit ID */
	au_mask_t	fad_mask;	/* audit state */
	au_termid_t	fad_termid;	/* terminal ID */
	int		fad_flags;	/* audit control flags */
	uint_t		fad_lpbuf;	/* length of path buffer */
	uint_t		fad_pathlen;	/* path length */
	caddr_t		fad_path;	/* saved path from vfs_lookup */
	struct f_audit_data *next;	/* next empty structure */
};
typedef struct f_audit_data f_audit_data_t;

#define	FAD_READ	0x0001		/* read system call seen */
#define	FAD_WRITE	0x0002		/* write system call seen */

#define	P2A(p)	(p->p_audit_data)
#define	T2A(t)	(t->t_audit_data)
#define	U2A(u)	(curthread->t_audit_data)
#define	F2A(f)	(f->f_audit_data)

#define	u_ad    (((struct t_audit_data *)U2A(u))->tad_ad)
#define	ad_ctrl (((struct t_audit_data *)U2A(u))->tad_ctrl)
#define	ad_flag (((struct t_audit_data *)U2A(u))->tad_flag)

struct au_buff {
	caddr_t	buf;
	struct au_buff	*next_buf;
	struct au_buff	*next_rec;
	ushort_t	rec_len;
	uchar_t		len;
	uchar_t		flag;
};

typedef struct au_buff au_buff_t;

#define	AU_BUFSIZE	128		/* buffer size for the buffer pool */
#define	AU_PAGE		32 * AU_BUFSIZE	/* allocate 32 buffers at a time */
#define	ONPAGE		32		/* number of buffer in the page */
#define	AU_MAXMEM	(2048 * 1024)

/* and 32 control structs */
#define	AU_CNTL		32 * sizeof (au_buff_t)

/*
 * Kernel audit queue structure.
 */
struct audit_queue {
	au_buff_t *head;	/* head of queue */
	au_buff_t *tail;	/* tail of queue */
	ssize_t	cnt;		/* number elements on queue */
	size_t	hiwater;	/* high water mark to block */
	size_t	lowater;	/* low water mark to restart */
	size_t	bufsz;		/* audit trail write buffer size */
	size_t	buflen;		/* audit trail buffer length in use */
	clock_t	delay;		/* delay before flushing queue */
	int	wt_block;	/* writer is blocked (1) */
	int	rd_block;	/* reader is blocked (1) */
	kmutex_t lock;		/* mutex lock for queue modification */
	kcondvar_t write_cv;	/* sleep structure for write block */
	kcondvar_t read_cv;	/* sleep structure for read block */
};

union rval;
struct audit_s2e {
	au_event_t (*au_init)(au_event_t);
				/* convert au_event to real audit event ID */

	int au_event;		/* default audit event for this system call */
	void (*au_start)(struct t_audit_data *);
				/* pre-system call audit processing */
	void (*au_finish)(struct t_audit_data *, int, union rval *);
				/* post-system call audit processing */
	int au_ctrl;		/* control flags for auditing actions */
};

extern struct audit_s2e audit_s2e[];

/*
 * Kernel auditing external variables
 */
extern au_state_t audit_ets[];
extern int au_wait;
extern caddr_t *aproc;
extern caddr_t *afile;

/*
 * crd support routines
 */
struct cwrd *getcw();
void bsm_cwincr(struct cwrd *);
void bsm_cwfree(struct cwrd *);

/* Memory cache statistics */
struct au_list_stat {
	int called;
	int hit;
	int miss;
	int free;
	int size;
};

#define	THREAD_HIT	au_tlist_stat.called++; au_tlist_stat.hit++; \
			au_tlist_stat.size--;
#define	THREAD_MISS	au_tlist_stat.called++; au_tlist_stat.miss++;
#define	THREAD_FREE	au_tlist_stat.free++;

#define	PROC_HIT	au_plist_stat.called++; au_plist_stat.hit++; \
			au_plist_stat.size--;
#define	PROC_MISS	au_plist_stat.called++; au_plist_stat.miss++;
#define	PROC_FREE	au_plist_stat.free++;

#define	FILE_HIT	au_flist_stat.called++; au_flist_stat.hit++; \
			au_flist_stat.size--;
#define	FILE_MISS	au_flist_stat.called++; au_flist_stat.miss++;
#define	FILE_FREE	au_flist_stat.free++;

#define	QUEUE_HIT	au_mem_stat.called++; au_mem_stat.hit++; \
			au_mem_stat.size--;
#define	QUEUE_MISS	au_mem_stat.called++; au_mem_stat.miss++;
#define	QUEUE_FREE	au_mem_stat.size++; au_mem_stat.free++;

#define	TLIST_SIZE	30;
#define	FLIST_SIZE	30;
#define	PLIST_SIZE	30;


/*
 * debug information for kernel
 */

#if defined(C2_DEBUG) || defined(lint)

int audit_debug;
int c2_debug;
void addtrace(char *, ...);
#define	dprintf(a, f)		{if (audit_debug & a) printf  f; }
#define	call_debug(a)		{if (c2_debug & a) debug_enter((char *)NULL); }
#define	ADDTRACE(a, b, c, d, e, f, g)	addtrace(a, b, c, d, e, f, g)

#else

#define	dprintf(a, f)
#define	call_debug(a)
#define	ADDTRACE(a, b, c, d, e, f, g)

#endif

#ifdef	_KERNEL
au_buff_t *au_get_buff(int), *au_free_buff(au_buff_t *);
void printf(const char *, ...);
#endif


/*
 * Macros for type conversion
 */

/* au_membuf head, to typed data */
#define	memtod(x, t)	((t)x->buf)

/* au_membuf types */
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */

/* flags to au_memget */
#define	DONTWAIT	0
#define	WAIT		1

#ifdef __cplusplus
}
#endif

#endif /* _BSM_AUDIT_KERNEL_H */
