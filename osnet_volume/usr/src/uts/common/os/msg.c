/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)msg.c	1.46	99/04/14 SMI"

/*
 * Inter-Process Communication Message Facility.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <c2/audit.h>

/*
 *	ATTENTION: All ye who enter here
 *	As an optimization, all global data items are declared in space.c
 *	When this module get unloaded, the data stays around. The data
 * 	is only allocated on first load.
 *
 *	XXX	Unloading disabled - there's more to leaving state
 *		lying around in the system than not freeing global data.
 */

/*
 * The following variables msginfo_* are there so that the
 * elements of the data structure msginfo can be tuned
 * (if necessary) using the /etc/system file syntax for
 * tuning of integer data types.
 */
size_t	msginfo_msgmax = 2048;	/* max message size */
size_t	msginfo_msgmnb = 4096;	/* max # bytes on queue */
int	msginfo_msgmni = 50;	/* # of message queue identifiers */
int	msginfo_msgtql = 40;	/* # of system message headers */

int	msginfo_msgssz = 8;	/* (obsolete) */
int	msginfo_msgmap = 0;	/* (obsolete) */
ushort_t msginfo_msgseg = 1024;	/* (obsolete) */

static kmutex_t msg_lock;	/* protects msg mechanism data structures */
static kcondvar_t msgfp_cv;

#include <sys/modctl.h>
#include <sys/syscall.h>

static ssize_t msgsys(int opcode, uintptr_t a0, uintptr_t a1, uintptr_t a2,
	uintptr_t a4, uintptr_t a5);

static struct sysent ipcmsg_sysent = {
	6,
#ifdef	_LP64
	SE_ARGC | SE_NOUNLOAD | SE_64RVAL,
#else
	SE_ARGC | SE_NOUNLOAD | SE_32RVAL1,
#endif
	(int (*)())msgsys
};

#ifdef	_SYSCALL32_IMPL

static ssize32_t msgsys32(int opcode, uint32_t a0, uint32_t a1, uint32_t a2,
	uint32_t a4, uint32_t a5);

static struct sysent ipcmsg_sysent32 = {
	6,
	SE_ARGC | SE_NOUNLOAD | SE_32RVAL1,
	(int (*)())msgsys32
};

#endif	/* _SYSCALL32_IMPL */

/*
 * Module linkage information for the kernel.
 */
static struct modlsys modlsys = {
	&mod_syscallops, "System V message facility", &ipcmsg_sysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32, "32-bit System V message facility", &ipcmsg_sysent32
};
#endif

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
	NULL
};

char _depends_on[] = "misc/ipc";	/* ipcaccess, ipcget */

int
_init(void)
{
	int retval;
	int		i;	/* loop control */
	struct msg	*mp;	/* ptr to msg being linked */
	struct msglock	*lp;	/* ptr to mutex being init'd */
	uint64_t	mavail;

	/*
	 * msginfo_* are inited in param.c to default values
	 * These values can be tuned if need be using the
	 * integer tuning capabilities in the /etc/system file.
	 */
	msginfo.msgmax = msginfo_msgmax;
	msginfo.msgmnb = msginfo_msgmnb;
	msginfo.msgmni = msginfo_msgmni;
	msginfo.msgtql = msginfo_msgtql;

	/*
	 * Don't use more than 25% of the available kernel memory
	 */
	mavail = (uint64_t)kmem_maxavail() / 4;
	if ((uint64_t)msginfo.msgmni * sizeof (struct msqid_ds) +
	    (uint64_t)msginfo.msgmni * sizeof (struct msglock) +
	    (uint64_t)msginfo.msgtql * sizeof (struct msg) > mavail) {
		cmn_err(CE_WARN,
		    "msgsys: can't load module, too much memory requested");
		return (ENOMEM);
	}

	mutex_init(&msg_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&msgfp_cv, NULL, CV_DEFAULT, NULL);

	ASSERT(msgh == NULL);
	ASSERT(msgque == NULL);
	ASSERT(msglock == NULL);

	msgh = kmem_zalloc(msginfo.msgtql * sizeof (struct msg), KM_SLEEP);
	msgque = kmem_zalloc(msginfo.msgmni * sizeof (struct msqid_ds),
	    KM_SLEEP);
	msglock = kmem_zalloc(msginfo.msgmni * sizeof (struct msglock),
	    KM_SLEEP);

	for (i = 0, lp = msglock; ++i < msginfo.msgmni; lp++)
		mutex_init(&lp->msglock_lock, NULL, MUTEX_DEFAULT, NULL);
	for (i = 0, mp = msgfp = msgh; ++i < msginfo.msgtql; mp++)
		mp->msg_next = mp + 1;

	if ((retval = mod_install(&modlinkage)) == 0)
		return (0);

	for (i = 0, lp = msglock; ++i < msginfo.msgmni; lp++)
		mutex_destroy(&lp->msglock_lock);
	kmem_free(msgh, msginfo.msgtql * sizeof (struct msg));
	kmem_free(msgque, msginfo.msgmni * sizeof (struct msqid_ds));
	kmem_free(msglock, msginfo.msgmni * sizeof (struct msglock));

	msgh = NULL;
	msgque = NULL;
	msglock = NULL;

	cv_destroy(&msgfp_cv);
	mutex_destroy(&msg_lock);

	return (retval);
}

/*
 * See comments in shm.c.
 */
int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Argument vectors for the various flavors of msgsys().
 */

#define	MSGGET	0
#define	MSGCTL	1
#define	MSGRCV	2
#define	MSGSND	3


/*
 * msgfree - Free up space and message header, relink pointers on q,
 * and wakeup anyone waiting for resources.
 *
 * called with q locked, gets and releases msg_lock
 */
static void
msgfree(
	struct msqid_ds	*qp, /* ptr to q of mesg being freed */
	struct msg		*pmp, /* ptr to mp's predecessor */
	struct msg		*mp) /* ptr to msg being freed */
{
	/* Unlink message from the q. */
	if (pmp == NULL)
		qp->msg_first = mp->msg_next;
	else
		pmp->msg_next = mp->msg_next;
	if (mp->msg_next == NULL)
		qp->msg_last = pmp;
	qp->msg_qnum--;
	if (qp->msg_perm.mode & MSG_WWAIT) {
		qp->msg_perm.mode &= ~MSG_WWAIT;
		cv_broadcast(&qp->msg_cv);
	}

	/* Free up message text. */
	if (mp->msg_addr)
		kmem_free(mp->msg_addr, mp->msg_size);

	/* Free up header */
	mutex_enter(&msg_lock);
	mp->msg_next = msgfp;
	cv_broadcast(&msgfp_cv);
	msgfp = mp;
	mutex_exit(&msg_lock);
}

/*
 * msgconv - Convert a user supplied message queue id into a ptr to a
 * msqid_ds structure.
 */
static int
msgconv(int id, struct msqid_ds **qpp)
{
	struct msqid_ds	*qp;	/* ptr to associated q slot */
	struct msglock		*lockp;	/* ptr to lock.		*/


	if (id < 0)
		return (EINVAL);

	qp = &msgque[id % msginfo.msgmni];
	lockp = MSGLOCK(qp);
	mutex_enter(&lockp->msglock_lock);
	if ((qp->msg_perm.mode & IPC_ALLOC) == 0 ||
	    (id / msginfo.msgmni) != qp->msg_perm.seq) {
		mutex_exit(&lockp->msglock_lock);
		return (EINVAL);

	}
#ifdef C2_AUDIT
	if (audit_active)
		audit_ipc(AT_IPC_MSG, id, (void *)qp);
#endif
	*qpp = qp;
	return (0);
}

/*
 * msgctl - Msgctl system call.
 *
 *  gets q lock (via msgconv), releases before return.
 *  may call users of msg_lock
 */
static int
msgctl(int msgid, int cmd, struct msqid_ds *buf)
{
	struct o_msqid_ds32	ods32;		/* SVR3 queue work area */
	STRUCT_DECL(msqid_ds, ds);		/* SVR4 queue work area */
	struct msqid_ds		*qp;		/* ptr to associated q */
	struct msglock		*lockp;
	int			error;
	struct	cred		*cr;
	model_t	mdl = get_udatamodel();

	STRUCT_INIT(ds, mdl);

	/*
	 * get msqid_ds for this msgid
	 */
	if (error = msgconv(msgid, &qp)) {
		return (set_errno(error));
	}

	lockp = MSGLOCK(qp);

	cr = CRED();
	switch (cmd) {
	case IPC_O_RMID:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		/* FALLTHROUGH */
	case IPC_RMID:
		/*
		 * IPC_RMID is for use with expanded msqid_ds struct -
		 * msqid_ds not currently used with this command
		 */
		if (cr->cr_uid != qp->msg_perm.uid &&
		    cr->cr_uid != qp->msg_perm.cuid &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		while (qp->msg_first)
			msgfree(qp, (struct msg *)NULL, qp->msg_first);
		qp->msg_cbytes = 0;
		if (msgid + msginfo.msgmni < 0)
			qp->msg_perm.seq = 0;
		else
			qp->msg_perm.seq++;
		if (qp->msg_perm.mode & MSG_RWAIT)
			cv_broadcast(&qp->msg_qnum_cv);
		if (qp->msg_perm.mode & MSG_WWAIT)
			cv_broadcast(&qp->msg_cv);
		qp->msg_perm.mode = 0;
		break;

	case IPC_O_SET:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		if (cr->cr_uid != qp->msg_perm.uid &&
		    cr->cr_uid != qp->msg_perm.cuid && !suser(cr)) {
			error = EPERM;
			break;
		}
		if (copyin(buf, &ods32, sizeof (ods32))) {
			error = EFAULT;
			break;
		}
		if (ods32.msg_qbytes > qp->msg_qbytes && !suser(cr)) {
			error = EPERM;
			break;
		}
		if (ods32.msg_perm.uid >= USHRT_MAX ||
		    ods32.msg_perm.gid >= USHRT_MAX) {
			error = EOVERFLOW;
			break;
		}
		if (ods32.msg_perm.uid > MAXUID ||
		    ods32.msg_perm.gid > MAXUID) {
			error = EINVAL;
			break;
		}
		qp->msg_perm.uid = ods32.msg_perm.uid;
		qp->msg_perm.gid = ods32.msg_perm.gid;
		qp->msg_perm.mode =
		    (qp->msg_perm.mode & ~0777) |
		    (ods32.msg_perm.mode & 0777);
		qp->msg_qbytes = ods32.msg_qbytes;
		qp->msg_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_MSG, (void *)qp);
		}
#endif
		break;

	case IPC_SET:
		if (cr->cr_uid != qp->msg_perm.uid &&
		    cr->cr_uid != qp->msg_perm.cuid &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		if (copyin(buf, STRUCT_BUF(ds), STRUCT_SIZE(ds))) {
			error = EFAULT;
			break;
		}
		if (STRUCT_FGET(ds, msg_qbytes) > qp->msg_qbytes &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		if (STRUCT_FGET(ds, msg_perm.uid) < (uid_t)0 ||
		    STRUCT_FGET(ds, msg_perm.uid) > MAXUID ||
		    STRUCT_FGET(ds, msg_perm.gid) < (gid_t)0 ||
		    STRUCT_FGET(ds, msg_perm.gid) > MAXUID) {
			error = EINVAL;
			break;
		}
		qp->msg_perm.uid = STRUCT_FGET(ds, msg_perm.uid);
		qp->msg_perm.gid = STRUCT_FGET(ds, msg_perm.gid);
		qp->msg_perm.mode =
		    (qp->msg_perm.mode & ~0777) |
		    (STRUCT_FGET(ds, msg_perm.mode) & 0777);
		qp->msg_qbytes = STRUCT_FGET(ds, msg_qbytes);
		qp->msg_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_MSG, (void *)qp);
		}
#endif
		break;

	case IPC_O_STAT:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		if (error = ipcaccess(&qp->msg_perm, MSG_R, cr))
			break;

		/*
		 * copy expanded msqid_ds struct to SVR3 msqid_ds structure -
		 * support for non-eft applications.
		 * Check whether SVR4 values are too large to store into an SVR3
		 * msqid_ds structure.
		 */
		if ((unsigned)qp->msg_perm.uid > USHRT_MAX ||
		    (unsigned)qp->msg_perm.gid > USHRT_MAX ||
		    (unsigned)qp->msg_perm.cuid > USHRT_MAX ||
		    (unsigned)qp->msg_perm.cgid > USHRT_MAX ||
		    (unsigned)qp->msg_perm.seq > USHRT_MAX ||
		    (unsigned)qp->msg_cbytes > USHRT_MAX ||
		    (unsigned)qp->msg_qnum > USHRT_MAX ||
		    (unsigned)qp->msg_qbytes > USHRT_MAX ||
		    (unsigned)qp->msg_lspid > SHRT_MAX ||
		    (unsigned)qp->msg_lrpid > SHRT_MAX) {
				error = EOVERFLOW;
				break;
		}
		ods32.msg_perm.uid = (o_uid_t)qp->msg_perm.uid;
		ods32.msg_perm.gid = (o_gid_t)qp->msg_perm.gid;
		ods32.msg_perm.cuid = (o_uid_t)qp->msg_perm.cuid;
		ods32.msg_perm.cgid = (o_gid_t)qp->msg_perm.cgid;
		ods32.msg_perm.mode = (o_mode_t)qp->msg_perm.mode;
		ods32.msg_perm.seq = (ushort_t)qp->msg_perm.seq;
		ods32.msg_perm.key = qp->msg_perm.key;
		ods32.msg_first = NULL; 	/* kernel addr */
		ods32.msg_last = NULL;
		ods32.msg_cbytes = (ushort_t)qp->msg_cbytes;
		ods32.msg_qnum = (ushort_t)qp->msg_qnum;
		ods32.msg_qbytes = (ushort_t)qp->msg_qbytes;
		ods32.msg_lspid = (o_pid_t)qp->msg_lspid;
		ods32.msg_lrpid = (o_pid_t)qp->msg_lrpid;
		ods32.msg_stime = qp->msg_stime;
		ods32.msg_rtime = qp->msg_rtime;
		ods32.msg_ctime = qp->msg_ctime;

		if (copyout(&ods32, buf, sizeof (ods32))) {
			error = EFAULT;
			break;
		}
		break;

	case IPC_STAT:
		if (error = ipcaccess(&qp->msg_perm, MSG_R, cr))
			break;

		STRUCT_FSET(ds, msg_perm.uid, qp->msg_perm.uid);
		STRUCT_FSET(ds, msg_perm.gid, qp->msg_perm.gid);
		STRUCT_FSET(ds, msg_perm.cuid, qp->msg_perm.cuid);
		STRUCT_FSET(ds, msg_perm.cgid, qp->msg_perm.cgid);
		STRUCT_FSET(ds, msg_perm.mode, qp->msg_perm.mode);
		STRUCT_FSET(ds, msg_perm.seq, qp->msg_perm.seq);
		STRUCT_FSET(ds, msg_perm.key, qp->msg_perm.key);
		STRUCT_FSETP(ds, msg_first, NULL); 	/* kernel addr */
		STRUCT_FSETP(ds, msg_last, NULL);
		STRUCT_FSET(ds, msg_cbytes, qp->msg_cbytes);
		STRUCT_FSET(ds, msg_qnum, qp->msg_qnum);
		STRUCT_FSET(ds, msg_qbytes, qp->msg_qbytes);
		STRUCT_FSET(ds, msg_lspid, qp->msg_lspid);
		STRUCT_FSET(ds, msg_lrpid, qp->msg_lrpid);
		STRUCT_FSET(ds, msg_stime, qp->msg_stime);
		STRUCT_FSET(ds, msg_rtime, qp->msg_rtime);
		STRUCT_FSET(ds, msg_ctime, qp->msg_ctime);

		if (copyout(STRUCT_BUF(ds), buf, STRUCT_SIZE(ds))) {
			error = EFAULT;
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	mutex_exit(&lockp->msglock_lock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*
 * msgget - Msgget system call.
 *
 * allocation of new msgqid controlled by msg_lock
 */
static int
msgget(key_t key, int msgflg)
{
	struct msqid_ds		*qp;	/* ptr to associated q */
	int			s;	/* ipcget status return */
	int			error;

	mutex_enter(&msg_lock);
	if (error = ipcget(key, msgflg, (struct ipc_perm *)msgque,
	    msginfo.msgmni, sizeof (*qp), &s, (struct ipc_perm **)&qp)) {
		mutex_exit(&msg_lock);
		return (set_errno(error));
	}

	if (s) {
		/* This is a new queue.  Finish initialization. */
		qp->msg_first = qp->msg_last = NULL;
		qp->msg_qnum = 0;
		qp->msg_qbytes = msginfo.msgmnb;
		qp->msg_lspid = qp->msg_lrpid = 0;
		qp->msg_stime = qp->msg_rtime = 0;
		qp->msg_ctime = hrestime.tv_sec;

	}
#ifdef C2_AUDIT
	if (audit_active) {
		audit_ipcget(AT_IPC_MSG, (void *)qp);
	}
#endif
	mutex_exit(&msg_lock);
	return ((int)(qp->msg_perm.seq * msginfo.msgmni + (qp - msgque)));
}

/*
 * msgrcv - Msgrcv system call.
 */
static ssize_t
msgrcv(int msqid, struct ipcmsgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	struct msg		*mp;	/* ptr to msg on q */
	struct msg		*pmp;	/* ptr to mp's predecessor */
	struct msg		*smp;	/* ptr to best msg on q */
	struct msg		*spmp;	/* ptr to smp's predecessor */
	struct msqid_ds		*qp;	/* ptr to associated q */
	struct msglock		*lockp;
	size_t			xtsz;	/* transfer byte count */
	int			error = 0;
	STRUCT_HANDLE(ipcmsgbuf, umsgp);
	model_t			mdl = get_udatamodel();

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.msg, 1);	/* bump msg send/rcv count */
	if (error = msgconv(msqid, &qp))
		return ((ssize_t)set_errno(error));
	lockp = MSGLOCK(qp);
	if (error = ipcaccess(&qp->msg_perm, MSG_R, CRED()))
		goto msgrcv_out;
	smp = spmp = NULL;
	STRUCT_SET_HANDLE(umsgp, mdl, msgp);

findmsg:

	pmp = NULL;
	mp = qp->msg_first;
	if (msgtyp == 0) {
		smp = mp;
	} else {
		for (; mp; pmp = mp, mp = mp->msg_next) {
			if (msgtyp > 0) {
				if (msgtyp != mp->msg_type)
					continue;
				smp = mp;
				spmp = pmp;
				break;
			}
			if (mp->msg_type <= -msgtyp) {
				if (smp && smp->msg_type <= mp->msg_type)
					continue;
				smp = mp;
				spmp = pmp;
			}
		}
	}

	if (smp) {
		if (msgsz < smp->msg_size) {
			if ((msgflg & MSG_NOERROR) == 0) {
				error = E2BIG;
				goto msgrcv_out;
			} else {
				xtsz = msgsz;
			}
		} else {
			xtsz = smp->msg_size;
		}
		if (mdl == DATAMODEL_NATIVE) {
			if (copyout(&smp->msg_type, msgp,
			    sizeof (smp->msg_type))) {
				error = EFAULT;
				goto msgrcv_out;
			}
		} else {
			/*
			 * 32-bit callers need an imploded msg type.
			 */
			int32_t	msg_type32 = smp->msg_type;

			if (copyout(&msg_type32, msgp, sizeof (msg_type32))) {
				error = EFAULT;
				goto msgrcv_out;
			}
		}

		if (xtsz && copyout(smp->msg_addr, STRUCT_FADDR(umsgp, mtext),
		    xtsz)) {
			error = EFAULT;
			goto msgrcv_out;
		}
		qp->msg_cbytes -= smp->msg_size;
		qp->msg_lrpid = ttoproc(curthread)->p_pid;
		qp->msg_rtime = hrestime.tv_sec;
		msgfree(qp, spmp, smp);
		goto msgrcv_out;
	}
	if (msgflg & IPC_NOWAIT) {
		error = ENOMSG;
		goto msgrcv_out;
	}
	qp->msg_perm.mode |= MSG_RWAIT;

	/* Wait for new message  */
	if (!cv_wait_sig(&qp->msg_qnum_cv, &lockp->msglock_lock))  {
		mutex_exit(&lockp->msglock_lock);
		return ((ssize_t)set_errno(EINTR));
	}

	/* Check for msg q deleted or reallocated */
	if ((qp->msg_perm.mode & IPC_ALLOC) == 0 ||
	    (msqid / msginfo.msgmni) != qp->msg_perm.seq)  {
		mutex_exit(&lockp->msglock_lock);
		return ((ssize_t)set_errno(EIDRM));
	}
	goto findmsg;

msgrcv_out:

	mutex_exit(&lockp->msglock_lock);
	if (error)
		return ((ssize_t)set_errno(error));
	return ((ssize_t)xtsz);
}

/*
 * msgsnd - Msgsnd system call.
 */
static int
msgsnd(int msqid, struct ipcmsgbuf *msgp, size_t msgsz, int msgflg)
{
	struct msqid_ds		*qp;	/* ptr to associated q */
	struct msg		*mp;	/* ptr to allocated msg hdr */
	struct msglock		*lockp;
	struct msqid_ds		*qp1;	/* alternate q ptr */
	long			type;	/* msg type. Sigh, stuck with long */
	int			error = 0;
	model_t			mdl = get_udatamodel();
	void			*addr = NULL;
	STRUCT_HANDLE(ipcmsgbuf, umsgp);

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.msg, 1);	/* bump msg send/rcv count */

	if (msgsz > msginfo.msgmax)
		return (set_errno(EINVAL));

	addr = kmem_zalloc(msgsz, KM_SLEEP);

	if (error = msgconv(msqid, &qp))
		goto msgsnd_out_unlocked;

	lockp = MSGLOCK(qp);
	if (error = ipcaccess(&qp->msg_perm, MSG_W, CRED()))
		goto msgsnd_out;

	STRUCT_SET_HANDLE(umsgp, mdl, msgp);

	if (mdl == DATAMODEL_NATIVE) {
		if (copyin(msgp, &type, sizeof (type))) {
			error = EFAULT;
			goto msgsnd_out;
		}
	} else {
		int32_t	type32;
		if (copyin(msgp, &type32, sizeof (type32))) {
			error = EFAULT;
			goto msgsnd_out;
		}
		type = type32;
	}

	if (type < 1) {
		error = EINVAL;
		goto msgsnd_out;
	}
getres:

	/*
	 * Allocate space on q, message header, & buffer space.
	 */
	if (msgsz > qp->msg_qbytes - qp->msg_cbytes) {
		if (msgflg & IPC_NOWAIT) {
			error = EAGAIN;
			goto msgsnd_out;
		}
		qp->msg_perm.mode |= MSG_WWAIT;

		if (!cv_wait_sig(&qp->msg_cv, &lockp->msglock_lock)) {
			mutex_exit(&lockp->msglock_lock);
			if (error = msgconv(msqid, &qp1)) {
				error = EIDRM;
				goto msgsnd_out_unlocked;
			}
			error = EINTR;
			qp->msg_perm.mode &= ~MSG_WWAIT;
			cv_broadcast(&qp->msg_cv);
			goto msgsnd_out;
		}

		/*
		 * check for q removed or reallocated
		 */
		if ((qp->msg_perm.mode & IPC_ALLOC) == 0 ||
		    (msqid / msginfo.msgmni) != qp->msg_perm.seq) {
			mutex_exit(&lockp->msglock_lock);
			error = EIDRM;
			goto msgsnd_out_unlocked;
		}
		goto getres;
	}
	mutex_enter(&msg_lock);
	if (msgfp == NULL) {
		if (msgflg & IPC_NOWAIT) {
			error = EAGAIN;
			mutex_exit(&msg_lock);
			goto msgsnd_out;
		}
		mutex_exit(&lockp->msglock_lock);
		if (!cv_wait_sig(&msgfp_cv, &msg_lock)) {
			mutex_exit(&msg_lock);
			if (error = msgconv(msqid, &qp1))
				goto msgsnd_out_unlocked;
			error = EINTR;
			if (qp1 != qp) {
				lockp = MSGLOCK(qp1);
				mutex_exit(&lockp->msglock_lock);
				goto msgsnd_out_unlocked;
			}
			goto msgsnd_out;
		}
		mutex_exit(&msg_lock);
		if (msgconv(msqid, &qp) != 0) {
			error = EIDRM;
			goto msgsnd_out_unlocked;
		}
		goto getres;
	}

	mp = msgfp;
	msgfp = mp->msg_next;
	mutex_exit(&msg_lock);

	/*
	 * Everything is available, copy in text and put msg on q.
	 */
	if (msgsz && copyin(STRUCT_FADDR(umsgp, mtext), addr, msgsz)) {
		error = EFAULT;
		goto msgsnd_out1;
	}
	qp->msg_qnum++;
	qp->msg_cbytes += msgsz;
	qp->msg_lspid = curproc->p_pid;
	qp->msg_stime = hrestime.tv_sec;
	mp->msg_next = NULL;
	mp->msg_type = type;
	mp->msg_addr = addr;
	mp->msg_size = msgsz;
	if (qp->msg_last == NULL)
		qp->msg_first = qp->msg_last = mp;
	else {
		qp->msg_last->msg_next = mp;
		qp->msg_last = mp;
	}
	if (qp->msg_perm.mode & MSG_RWAIT) {
		qp->msg_perm.mode &= ~MSG_RWAIT;
		cv_broadcast(&qp->msg_qnum_cv);
	}
	goto msgsnd_out;

msgsnd_out1:

	mutex_enter(&msg_lock);
	mp->msg_next = msgfp;
	cv_broadcast(&msgfp_cv);
	msgfp = mp;
	mutex_exit(&msg_lock);

msgsnd_out:

	mutex_exit(&lockp->msglock_lock);

msgsnd_out_unlocked:

	if (error) {
		kmem_free(addr, msgsz);
		return (set_errno(error));
	}
	return (0);
}

/*
 * msgsys - System entry point for msgctl, msgget, msgrcv, and msgsnd
 * system calls.
 */
static ssize_t
msgsys(int opcode, uintptr_t a1, uintptr_t a2, uintptr_t a3,
	uintptr_t a4, uintptr_t a5)
{
	ssize_t error;

	switch (opcode) {
	case MSGGET:
		error = msgget((key_t)a1, (int)a2);
		break;
	case MSGCTL:
		error = msgctl((int)a1, (int)a2, (struct msqid_ds *)a3);
		break;
	case MSGRCV:
		error = msgrcv((int)a1, (struct ipcmsgbuf *)a2,
		    (size_t)a3, (long)a4, (int)a5);
		break;
	case MSGSND:
		error = msgsnd((int)a1, (struct ipcmsgbuf *)a2,
		    (size_t)a3, (int)a4);
		break;
	default:
		error = set_errno(EINVAL);
		break;
	}

	return (error);
}

#ifdef	_SYSCALL32_IMPL
/*
 * msgsys32 - System entry point for msgctl, msgget, msgrcv, and msgsnd
 * system calls for 32-bit callers on LP64 kernel.
 */
static ssize32_t
msgsys32(int opcode, uint32_t a1, uint32_t a2, uint32_t a3,
	uint32_t a4, uint32_t a5)
{
	ssize_t error;

	switch (opcode) {
	case MSGGET:
		error = msgget((key_t)a1, (int)a2);
		break;
	case MSGCTL:
		error = msgctl((int)a1, (int)a2, (struct msqid_ds *)a3);
		break;
	case MSGRCV:
		error = msgrcv((int)a1, (struct ipcmsgbuf *)a2,
		    (size_t)a3, (long)(int32_t)a4, (int)a5);
		break;
	case MSGSND:
		error = msgsnd((int)a1, (struct ipcmsgbuf *)a2,
		    (size_t)(int32_t)a3, (int)a4);
		break;
	default:
		error = set_errno(EINVAL);
		break;
	}

	return (error);
}
#endif	/* SYSCALL32_IMPL */
