/*
 * @(#)audit_syscalls.c 2.20 92/02/24 SMI; SunOS CMW
 * @(#)audit_syscalls.c 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * This file contaings the auditing system call code.
 *
 * Note that audit debugging is currently enabled. This will be turned off at
 * beta ship.
 */

/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)audit_syscalls.c	1.54	99/10/18 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/session.h>	/* for session structure (auditctl(2) */
#include <sys/kmem.h>		/* for KM_SLEEP */
#include <sys/cred.h>		/* for cred */
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/pathname.h>
#include <sys/acct.h>
#include <sys/stropts.h>
#include <sys/exec.h>
#include <sys/thread.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/disp.h>
#include <sys/kobj.h>
#include <sys/sysmacros.h>

#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>

#define	CLEAR_VAL	-1

extern kmutex_t  au_stat_lock;
extern kmutex_t  au_fstat_lock;
extern kmutex_t au_seq_lock;
extern kmutex_t checktime_lock;
extern kcondvar_t checktime_cv;
extern kmutex_t au_open_lock;
extern kmutex_t au_svc_lock;
extern kmutex_t pidlock;

extern int audit_active;
extern struct p_audit_data *padata;
extern struct audit_queue au_queue;
extern time_t	au_checktime;	/* time of last space check */
extern int naevent;

extern int audit_load;		/* defined in audit_start.c */

int		au_auditstate;	/* current audit state */
int		audit_policy;	/* audit policy in force */
int		audit_count;	/* count of audit records */
au_stat_t	audit_statistics; /* audit subsystem statistics */
int		audit_rec_size = 0x8000; /* maximum user audit record size */
au_fstat_t	audit_file_stat; /* record keeping for setfsize */
unsigned int	au_min_file_sz = 0x80000; /* minumum audit file size */
int		au_dont_stop = 0;
clock_t		au_resid = 15;	/* wait .15 sec before droping a rec */
id_t		au_tscid;

static int svc_busy = 0;

static int getauid(caddr_t);
static int setauid(caddr_t);
static int getaudit(caddr_t);
static int getaudit_addr(caddr_t, int);
static int setaudit(caddr_t);
static int setaudit_addr(caddr_t, int);
static int auditsvc(int, int);
static int auditctl(int, caddr_t, int, rval_t *);
static int audit_modsysent(char *, int, int (*)());

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
#include "sys/syscall.h"

static struct sysent auditsysent = {
	6,
	0,
	_auditsys
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_syscallops;

static struct modlsys modlsys = {
	&mod_syscallops, "C2 system call", &auditsysent
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsys, 0
};

int
_init()
{
	int retval;

	if (audit_load == 0)
		return (-1);

	/*
	 * We are going to do an ugly thing here.
	 *  Because auditsys is already defined as a regular
	 *  syscall we have to change the definition for syscall
	 *  auditsys. Basically or in the SE_LOADABLE flag for
	 *  auditsys. We no have a static loadable syscall. Also
	 *  create an rw_lock.
	 */

	if ((audit_modsysent("c2audit",
		SE_LOADABLE|SE_NOUNLOAD,
		_auditsys)) == -1)
		return (-1);

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	return (0);
}

int
_fini()
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_auditsys(struct auditcalls *uap, rval_t *rvp)
{
	int result = 0;

	switch (uap->code) {
	case BSM_GETAUID:
		if (audit_active)
			result = getauid((caddr_t)uap->a1);
		break;
	case BSM_SETAUID:
		if (audit_active)
			result = setauid((caddr_t)uap->a1);
		break;
	case BSM_GETAUDIT:
		if (audit_active)
			result = getaudit((caddr_t)uap->a1);
		break;
	case BSM_GETAUDIT_ADDR:
		if (audit_active)
			result = getaudit_addr((caddr_t)uap->a1, (int)uap->a2);
		break;
	case BSM_SETAUDIT:
		if (audit_active)
			result = setaudit((caddr_t)uap->a1);
		break;
	case BSM_SETAUDIT_ADDR:
		if (audit_active)
			result = setaudit_addr((caddr_t)uap->a1, (int)uap->a2);
		break;
	case BSM_AUDIT:
		if (audit_active)
			result = audit((caddr_t)uap->a1, (int)uap->a2);
		break;
	case BSM_AUDITSVC:
		if (audit_active)
			result = auditsvc((int)uap->a1, (int)uap->a2);
		break;
	case BSM_AUDITON:
	case BSM_AUDITCTL:
		result = auditctl((int)uap->a1,
				    (caddr_t)uap->a2,
				    (int)uap->a3, rvp);
		break;
	default:
		return (EINVAL);
	}
	rvp->r_vals = result;
	return (result);
}

/*
 * Return the audit user ID for the current process.  Currently only
 * the superuser may see the audit id.  That may change.  If the
 * process audit data structure doesn't exit, we will panic.
 * If copyout is unsucessful return EFAULT.
 */
static int
getauid(caddr_t auid_p)
{
	au_id_t    auid;
	struct p_audit_data *pad;

	if (!suser(CRED()))
		return (EPERM);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	mutex_enter(&(pad->pad_lock));
	auid = pad->pad_auid;
	mutex_exit(&(pad->pad_lock));

	if (copyout(&auid, auid_p, sizeof (au_id_t)))
		return (EFAULT);

	return (0);
}

/*
 * Set the audit userid, for a process.  This can only be changed by
 * super-user.  The audit userid is inherited across forks & execs.
 * Passed in a pointer to the au_id_t, if there is no per process
 * audit data structure, we are hosed, panic the system.
 * if copyin is unsuccessfull return EFAULT.
 */
static int
setauid(caddr_t auid_p)
{
	struct  p_audit_data *pad;
	au_id_t	auid;

	if (!suser(CRED()))
		return (EPERM);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}
	if (copyin(auid_p, &auid, sizeof (au_id_t))) {
		return (EFAULT);
	}
	mutex_enter(&(pad->pad_lock));
	pad->pad_auid = auid;
	mutex_exit(&(pad->pad_lock));
	return (0);
}

/*
 * Get the audit state information from the current process.
 * Panic if there is no per process audit data.
 * Return EFAULT if copyout fails.
 */
static int
getaudit(caddr_t info_p)
{
	STRUCT_DECL(auditinfo, info);
	struct  p_audit_data *pad;
	model_t	model;

	if (!suser(CRED()))
		return (EPERM);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	/* trying to read a process with an IPv6 address? */
	if (pad->pad_termid.at_type == AU_IPv6)
		return (EOVERFLOW);

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	mutex_enter(&(pad->pad_lock));
	STRUCT_FSET(info, ai_auid, pad->pad_auid);
	STRUCT_FSET(info, ai_mask, pad->pad_mask);
	STRUCT_FSET(info, ai_termid.port, DEVCMPL(pad->pad_termid.at_port));
	STRUCT_FSET(info, ai_termid.machine, pad->pad_termid.at_addr[0]);
	STRUCT_FSET(info, ai_asid, pad->pad_asid);
	mutex_exit(&(pad->pad_lock));

	if (copyout(STRUCT_BUF(info), info_p, STRUCT_SIZE(info)))
		return (EFAULT);

	return (0);
}

/*
 * Get the audit state information from the current process.
 * Panic if there is no per process audit data.
 * Return EFAULT if copyout fails.
 */
static int
getaudit_addr(caddr_t info_p, int len)
{
	STRUCT_DECL(auditinfo_addr, info);
	struct  p_audit_data *pad;
	model_t	model;

	if (!suser(CRED()))
		return (EPERM);

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	if (len < STRUCT_SIZE(info))
		return (EOVERFLOW);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	mutex_enter(&(pad->pad_lock));
	STRUCT_FSET(info, ai_auid, pad->pad_auid);
	STRUCT_FSET(info, ai_mask, pad->pad_mask);
	STRUCT_FSET(info, ai_termid.at_port, DEVCMPL(pad->pad_termid.at_port));
	STRUCT_FSET(info, ai_termid.at_type, pad->pad_termid.at_type);
	STRUCT_FSET(info, ai_termid.at_addr[0], pad->pad_termid.at_addr[0]);
	STRUCT_FSET(info, ai_termid.at_addr[1], pad->pad_termid.at_addr[1]);
	STRUCT_FSET(info, ai_termid.at_addr[2], pad->pad_termid.at_addr[2]);
	STRUCT_FSET(info, ai_termid.at_addr[3], pad->pad_termid.at_addr[3]);
	STRUCT_FSET(info, ai_asid, pad->pad_asid);
	mutex_exit(&(pad->pad_lock));

	if (copyout(STRUCT_BUF(info), info_p, STRUCT_SIZE(info)))
		return (EFAULT);

	return (0);
}

/*
 * Set the audit state information for the current process.
 * Panic if there is no per process audit data.
 * Return EFAULT if copyout fails.
 */
static int
setaudit(caddr_t info_p)
{
	STRUCT_DECL(auditinfo, info);
	struct p_audit_data *pad;
	model_t	model;

	if (!suser(CRED()))
		return (EPERM);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	if (copyin(info_p, STRUCT_BUF(info), STRUCT_SIZE(info)))
		return (EFAULT);

		/* Set audit mask, id, termid and session id as specified */
	mutex_enter(&(pad->pad_lock));
	pad->pad_auid = STRUCT_FGET(info, ai_auid);
	pad->pad_termid.at_port = DEVEXPL(STRUCT_FGET(info, ai_termid.port));
	pad->pad_termid.at_type = AU_IPv4;
	pad->pad_termid.at_addr[0] = STRUCT_FGET(info, ai_termid.machine);
	pad->pad_asid = STRUCT_FGET(info, ai_asid);
	pad->pad_mask = STRUCT_FGET(info, ai_mask);
	mutex_exit(&(pad->pad_lock));

	return (0);
}

/*
 * Set the audit state information for the current process.
 * Panic if there is no per process audit data.
 * Return EFAULT if copyout fails.
 */
static int
setaudit_addr(caddr_t info_p, int len)
{
	STRUCT_DECL(auditinfo_addr, info);
	struct p_audit_data *pad;
	model_t	model;
	int i;
	int type;

	if (!suser(CRED()))
		return (EPERM);

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	if (len < STRUCT_SIZE(info))
		return (EOVERFLOW);

	if (copyin(info_p, STRUCT_BUF(info), STRUCT_SIZE(info)))
		return (EFAULT);

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	type = STRUCT_FGET(info, ai_termid.at_type);
	if ((type != AU_IPv4) && (type != AU_IPv6))
		return (EINVAL);

	/* Set audit mask, id, termid and session id as specified */
	mutex_enter(&(pad->pad_lock));
	pad->pad_auid = STRUCT_FGET(info, ai_auid);
	pad->pad_mask = STRUCT_FGET(info, ai_mask);
	pad->pad_termid.at_port = DEVEXPL(STRUCT_FGET(info, ai_termid.at_port));
	pad->pad_termid.at_type = type;
	bzero(&pad->pad_termid.at_addr[0], sizeof (pad->pad_termid.at_addr));
	for (i = 0; i < (type/sizeof (int)); i++)
		pad->pad_termid.at_addr[i] =
			STRUCT_FGET(info, ai_termid.at_addr[i]);

	if (pad->pad_termid.at_type == AU_IPv6 &&
	    IN6_IS_ADDR_V4MAPPED(((in6_addr_t *)pad->pad_termid.at_addr))) {
		pad->pad_termid.at_type = AU_IPv4;
		pad->pad_termid.at_addr[0] = pad->pad_termid.at_addr[3];
		pad->pad_termid.at_addr[1] = 0;
		pad->pad_termid.at_addr[2] = 0;
		pad->pad_termid.at_addr[3] = 0;
	}


	pad->pad_asid = STRUCT_FGET(info, ai_asid);
	mutex_exit(&(pad->pad_lock));

	return (0);
}

/*
 * The audit system call. Trust what the user has sent down and save it
 * away in the audit file. User passes a complete audit record and its
 * length.  We will fill in the time stamp, check the header and the length
 * Put a trailer and a sequence token if policy.
 * In the future length might become size_t instead of an int.
 */
int
audit(caddr_t record, int length)
{
	char	c, *rewind;
	int 	count, l;
#ifdef NOT_SLOPPY
	short junk;
#endif
	token_t *m, *n, *s, *ad;
	adr_t adr;

	if (au_auditstate != AUC_AUDITING) /* If the audit state off return */
		return (0);
	if (!suser(CRED()))		/* Only root can audit */
		return (EPERM);
	if (length > audit_rec_size)	/* Max user record size is 32K */
		return (E2BIG);

	/* Read in user's audit record */
	count = length;
	m = n = s = ad = NULL;
	while (count) {
		m = au_getclr(au_wait);
		if (!s)
			s = n = m;
		else {
			n->next_buf = m;
			n = m;
		}
		l = MIN(count, AU_BUFSIZE);
		if (copyin(record, memtod((au_buff_t *)m, caddr_t),
			(size_t)l)) {
				/* copyin failed release au_membuf */
				au_free_rec((au_buff_t *)s);
				return (EFAULT);
		}
		record += l;
		count -= l;
		m->len = (u_char) l;
	}

	/* Now attach the entire thing to ad */

	au_write((caddr_t *)&(ad), s);

	/* Record is in, now to validate it */

	adr_start(&adr, memtod((au_buff_t *)s, char *));
	rewind = adr_getchar(&adr, &c);
	if (c != AUT_HEADER32 && c != AUT_HEADER64) {
		/* Header is wrong fix it */
		adr.adr_now = rewind;
#if (defined(_LP64) && (!defined(_SYSCALL32)))
		c = AUT_HEADER64;
		length += 2 * sizeof (int64_t) - 2 * sizeof (int32_t);
#else
		c = AUT_HEADER32;
#endif
		adr_char(&adr, &c, 1);	/* Fix the header it's important */
	}
#if (defined(_LP64) && (!defined(_SYSCALL32)))
	else if (c == AUT_HEADER32) {
		c = AUT_HEADER64;
		length += 2 * sizeof (int64_t) - 2 * sizeof (int32_t);
	}
#endif

	if (audit_policy&AUDIT_SEQ) {
		m = au_to_seq();
		length += au_token_size(m);
		au_write((caddr_t *)&(ad), m);
	}

	if (audit_policy&AUDIT_TRAIL) {
		length += 7; 		/* trailer token is 7 bytes long */
		au_write((caddr_t *)&(ad), au_to_trailer(length));
	}

	adr_int32(&adr, (int32_t *)&length, 1);	/* put in the record length */

	rewind = adr_getchar(&adr, &c);
	if (c != TOKEN_VERSION) { /* Version is wrong, lets be nice, fix it */
		adr.adr_now = rewind;
		c = TOKEN_VERSION;
		adr_char(&adr, &c, 1);
	}

#ifdef NOT_SLOPPY
	rewind = adr_getshort(&adr, &junk); /* skip past event ID */
	rewind = adr_getshort(&adr, &junk); /* skip past event ID modifier */
#else
	adr.adr_now += 4;
#endif

	/* Put in time and date stamp */
#if (defined(_LP64) && (!defined(_SYSCALL32)))
	adr_int64(&adr, (int64_t *)&hrestime, 2);
#else
	{
		struct {
			uint32_t sec;
			uint32_t usec;
		} tv;

		tv.sec = (uint32_t)hrestime.tv_sec;
		tv.usec = (uint32_t)hrestime.tv_nsec;
		adr_int32(&adr, (int32_t *)&tv, 2);
	}
#endif

	/* We are done  put it on the queue */
	AS_INC(as_generated, 1);
	AS_INC(as_audit, 1);
	if (audit_sync_block()) {
		/* to many record on the queue, release au_membuf */
		au_free_rec((au_buff_t *)s);
		return (0);
	}

	AS_INC(as_totalsize, length);
	AS_INC(as_enqueue, 1);
	au_enqueue(s);

	/* kick the reader awake */

	mutex_enter(&au_queue.lock);
	if (au_queue.rd_block && au_queue.cnt > au_queue.lowater)
		cv_broadcast(&au_queue.read_cv);
	mutex_exit(&au_queue.lock);

	return (0);
}

/* ARGSUSED */
void
audit_dont_stop(void *arg)
{
	au_dont_stop = 1;
	mutex_enter(&au_queue.lock);
	cv_broadcast(&au_queue.write_cv);
	mutex_exit(&au_queue.lock);
}

static int
rsuser(struct cred *cr)
{
	if ((intptr_t)cr == -1)
		panic("rsuser: bad cred pointer");
	if (cr->cr_ruid == 0) {
		u.u_acflag |= ASU;
		return (1);
	}
	return (0);
}

static int
auditsvc(int fd, int limit)
{
	struct file *fp;
	struct vnode *vp;
	int error = 0;

	if (!suser(CRED()) && !rsuser(CRED()))
		return (EPERM);

	if (limit < 0 || au_auditstate != AUC_AUDITING)
		return (EINVAL);


	/*
	 * Prevent a second audit daemon from running this code
	 */
	mutex_enter(&au_svc_lock);
	if (svc_busy) {
		mutex_exit(&au_svc_lock);
		return (EBUSY);
	}
	svc_busy = 1;
	mutex_exit(&au_svc_lock);

	/*
	 * convert file pointer to file descriptor
	 *   Note: fd ref count incremented here.
	 */
	if ((fp = (struct file *)getf(fd)) == NULL) {
		mutex_enter(&au_svc_lock);
		svc_busy = 0;
		mutex_exit(&au_svc_lock);
		return (EBADF);
	}

	vp = fp->f_vnode;

	audit_file_stat.af_currsz = 0;
	au_dont_stop = 0;
	(void) getcid("TS", &au_tscid);

	/*
	 * Wait for work, until a signal arrives,
	 * or until auditing is disabled.
	 */
	while (!error) {
	    if (au_auditstate == AUC_AUDITING) {
		mutex_enter(&au_queue.lock);
		    /* nothing on the audit queue */
		while (au_isqueued() == 0) {
			/* safety check. kick writer awake */
		    if (au_queue.wt_block)
			cv_broadcast(&au_queue.write_cv);
			/* sleep waiting for things to to */
		    au_queue.rd_block = 1;
		    AS_INC(as_rblocked, 1);
		    if (!cv_wait_sig(&au_queue.read_cv, &au_queue.lock)) {
				/* interrupted system call */
			au_queue.rd_block = 0;
			mutex_exit(&au_queue.lock);
			error = (au_auditstate == AUC_AUDITING) ?
				    EINTR : EINVAL;
			mutex_enter(&au_svc_lock);
			svc_busy = 0;
			mutex_exit(&au_svc_lock);

		/* decrement file descriptor reference count */
			releasef(fd);
			(void) timeout(audit_dont_stop, NULL, au_resid);
			return (error);
		    }
		    au_queue.rd_block = 0;
		}
		mutex_exit(&au_queue.lock);

			/* do as much as we can */
		error = au_doio(vp, limit);

		/* if we ran out of space, be sure to fire off timeout */
		if (error == ENOSPC)
			(void) timeout(audit_dont_stop, NULL, au_resid);

	    } else	/* auditing turned off while we slept */
		break;
	}

	/*
	 * decrement file descriptor reference count
	 */
	releasef(fd);

	/*
	 * If auditing has been disabled quit processing
	 */
	if (au_auditstate != AUC_AUDITING)
		error = EINVAL;

	mutex_enter(&au_svc_lock);
	svc_busy = 0;
	mutex_exit(&au_svc_lock);

	return (error);
}


/*
 * Get the global policy flag
 */

static int
getpolicy(caddr_t data)
{
	if (copyout(&audit_policy, data, sizeof (int)))
		return (EFAULT);
	return (0);
}

/*
 * Set the global policy flag
 */

static int
setpolicy(caddr_t data)
{
	int	policy;

	if (copyin(data, &policy, sizeof (int)))
		return (EFAULT);
	if (policy & ~(AUDIT_CNT | AUDIT_AHLT | AUDIT_ARGV| AUDIT_ARGE|
			AUDIT_PASSWD | AUDIT_SEQ | AUDIT_WINDATA|
			AUDIT_USER | AUDIT_GROUP| AUDIT_TRAIL| AUDIT_PATH))
		return (EINVAL);

	audit_policy = policy;
	return (0);
}

static int
getkmask(caddr_t data)
{
	if (copyout(&padata->pad_mask, data, sizeof (au_mask_t)))
		return (EFAULT);
	return (0);
}

static int
setkmask(caddr_t data)
{
	au_mask_t	mask;

	if (copyin(data, &mask, sizeof (au_mask_t)))
		return (EFAULT);

	padata->pad_mask = mask;
	return (0);
}

static int
getkaudit(caddr_t info_p, int len)
{
	STRUCT_DECL(auditinfo_addr, info);
	model_t model;

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	if (len < STRUCT_SIZE(info))
		return (EOVERFLOW);

	STRUCT_FSET(info, ai_auid, padata->pad_auid);
	STRUCT_FSET(info, ai_mask, padata->pad_mask);
	STRUCT_FSET(info, ai_termid.at_port,
			DEVCMPL(padata->pad_termid.at_port));
	STRUCT_FSET(info, ai_termid.at_type, padata->pad_termid.at_type);
	STRUCT_FSET(info, ai_termid.at_addr[0], padata->pad_termid.at_addr[0]);
	STRUCT_FSET(info, ai_termid.at_addr[1], padata->pad_termid.at_addr[1]);
	STRUCT_FSET(info, ai_termid.at_addr[2], padata->pad_termid.at_addr[2]);
	STRUCT_FSET(info, ai_termid.at_addr[3], padata->pad_termid.at_addr[3]);
	STRUCT_FSET(info, ai_asid, padata->pad_asid);

	if (copyout(STRUCT_BUF(info), info_p, STRUCT_SIZE(info)))
		return (EFAULT);

	return (0);
}

static int
setkaudit(caddr_t info_p, int len)
{
	STRUCT_DECL(auditinfo_addr, info);
	model_t model;

	model = get_udatamodel();
	STRUCT_INIT(info, model);

	if (len < STRUCT_SIZE(info))
		return (EOVERFLOW);

	if (copyin(info_p, STRUCT_BUF(info), STRUCT_SIZE(info)))
		return (EFAULT);

	if ((STRUCT_FGET(info, ai_termid.at_type) != AU_IPv4) &&
	    (STRUCT_FGET(info, ai_termid.at_type) != AU_IPv6))
		return (EINVAL);

	/* Set audit mask, termid and session id as specified */
	padata->pad_auid = STRUCT_FGET(info, ai_auid);
	padata->pad_mask = STRUCT_FGET(info, ai_mask);
	padata->pad_termid.at_port =
		DEVEXPL(STRUCT_FGET(info, ai_termid.at_port));
	padata->pad_termid.at_type = STRUCT_FGET(info, ai_termid.at_type);
	bzero(&padata->pad_termid.at_addr[0],
		sizeof (padata->pad_termid.at_addr));
	padata->pad_termid.at_addr[0] = STRUCT_FGET(info, ai_termid.at_addr[0]);
	padata->pad_termid.at_addr[1] = STRUCT_FGET(info, ai_termid.at_addr[1]);
	padata->pad_termid.at_addr[2] = STRUCT_FGET(info, ai_termid.at_addr[2]);
	padata->pad_termid.at_addr[3] = STRUCT_FGET(info, ai_termid.at_addr[3]);
	padata->pad_asid = STRUCT_FGET(info, ai_asid);

	if (padata->pad_termid.at_type == AU_IPv6 &&
	    IN6_IS_ADDR_V4MAPPED(((in6_addr_t *)padata->pad_termid.at_addr))) {
		padata->pad_termid.at_type = AU_IPv4;
		padata->pad_termid.at_addr[0] = padata->pad_termid.at_addr[3];
		padata->pad_termid.at_addr[1] = 0;
		padata->pad_termid.at_addr[2] = 0;
		padata->pad_termid.at_addr[3] = 0;
	}


	return (0);
}

static int
getqctrl(caddr_t data)
{
	STRUCT_DECL(au_qctrl, qctrl);
	STRUCT_INIT(qctrl, get_udatamodel());

	mutex_enter(&au_queue.lock);
	STRUCT_FSET(qctrl, aq_hiwater, au_queue.hiwater);
	STRUCT_FSET(qctrl, aq_lowater, au_queue.lowater);
	STRUCT_FSET(qctrl, aq_bufsz, au_queue.bufsz);
	STRUCT_FSET(qctrl, aq_delay, au_queue.delay);
	mutex_exit(&au_queue.lock);

	if (copyout(STRUCT_BUF(qctrl), data, STRUCT_SIZE(qctrl)))
		return (EFAULT);

	return (0);
}

static int
setqctrl(caddr_t data)
{
	struct au_qctrl qctrl_tmp;
	STRUCT_DECL(au_qctrl, qctrl);
	STRUCT_INIT(qctrl, get_udatamodel());

	if (copyin(data, STRUCT_BUF(qctrl), STRUCT_SIZE(qctrl)))
		return (EFAULT);

	qctrl_tmp.aq_hiwater = (size_t)STRUCT_FGET(qctrl, aq_hiwater);
	qctrl_tmp.aq_lowater = (size_t)STRUCT_FGET(qctrl, aq_lowater);
	qctrl_tmp.aq_bufsz = (size_t)STRUCT_FGET(qctrl, aq_bufsz);
	qctrl_tmp.aq_delay = (clock_t)STRUCT_FGET(qctrl, aq_delay);

	/* enforce sane values */

	if (qctrl_tmp.aq_hiwater <= qctrl_tmp.aq_lowater)
		return (EINVAL);

	if (qctrl_tmp.aq_hiwater < AQ_LOWATER)
		return (EINVAL);

	if (qctrl_tmp.aq_hiwater > AQ_MAXHIGH)
		return (EINVAL);

	if (qctrl_tmp.aq_bufsz < AQ_BUFSZ)
		qctrl_tmp.aq_bufsz = 0;

	if (qctrl_tmp.aq_bufsz > AQ_MAXBUFSZ)
		return (EINVAL);

	if (qctrl_tmp.aq_delay == 0)
		return (EINVAL);

	if (qctrl_tmp.aq_delay > AQ_MAXDELAY)
		return (EINVAL);

	/* update everything at once so things are consistant */
	mutex_enter(&au_queue.lock);
	au_queue.hiwater = qctrl_tmp.aq_hiwater;
	au_queue.lowater = qctrl_tmp.aq_lowater;
	au_queue.bufsz = qctrl_tmp.aq_bufsz;
	au_queue.delay = qctrl_tmp.aq_delay;
	mutex_exit(&au_queue.lock);

	return (0);
}

static int
getcwd(caddr_t data, int length)
{
	struct p_audit_data *pad;

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	mutex_enter(&(pad->pad_lock));

	if (pad->pad_cwrd->cwrd_dirlen > length) {
		mutex_exit(&(pad->pad_lock));
		return (E2BIG);
	}

	if (copyout(pad->pad_cwrd->cwrd_dir, data,
			pad->pad_cwrd->cwrd_dirlen)) {
		mutex_exit(&(pad->pad_lock));
		return (EFAULT);
	}

	mutex_exit(&(pad->pad_lock));
	return (0);
}

static int
getcar(caddr_t data, int length)
{
	struct p_audit_data *pad;

	if ((pad = (struct p_audit_data *)P2A(curproc)) == NULL) {
		cmn_err(CE_PANIC, "Process has no pad");
	}

	mutex_enter(&(pad->pad_lock));

	if (pad->pad_cwrd->cwrd_rootlen > length) {
		mutex_exit(&(pad->pad_lock));
		return (E2BIG);
	}

	if (copyout(pad->pad_cwrd->cwrd_root, data,
			pad->pad_cwrd->cwrd_rootlen)) {
		mutex_exit(&(pad->pad_lock));
		return (EFAULT);
	}

	mutex_exit(&(pad->pad_lock));
	return (0);
}

static int
getstat(caddr_t data)
{
	au_stat_t au_stat;

	mutex_enter(&au_stat_lock);
	bcopy(&audit_statistics, &au_stat, sizeof (au_stat_t));
	mutex_exit(&au_stat_lock);

	if (copyout((caddr_t)&au_stat, data, sizeof (au_stat_t)))
		return (EFAULT);
	return (0);
}

static int
setstat(caddr_t data)
{
	au_stat_t au_stat;

	if (copyin(data, &au_stat, sizeof (au_stat_t)))
		return (EFAULT);

	mutex_enter(&au_stat_lock);
	if (au_stat.as_generated == CLEAR_VAL)
		audit_statistics.as_generated = 0;
	if (au_stat.as_nonattrib == CLEAR_VAL)
		audit_statistics.as_nonattrib = 0;
	if (au_stat.as_kernel == CLEAR_VAL)
		audit_statistics.as_kernel = 0;
	if (au_stat.as_audit == CLEAR_VAL)
		audit_statistics.as_audit = 0;
	if (au_stat.as_auditctl == CLEAR_VAL)
		audit_statistics.as_auditctl = 0;
	if (au_stat.as_enqueue == CLEAR_VAL)
		audit_statistics.as_enqueue = 0;
	if (au_stat.as_written == CLEAR_VAL)
		audit_statistics.as_written = 0;
	if (au_stat.as_wblocked == CLEAR_VAL)
		audit_statistics.as_wblocked = 0;
	if (au_stat.as_rblocked == CLEAR_VAL)
		audit_statistics.as_rblocked = 0;
	if (au_stat.as_dropped == CLEAR_VAL)
		audit_statistics.as_dropped = 0;
	if (au_stat.as_totalsize == CLEAR_VAL)
		audit_statistics.as_totalsize = 0;
	mutex_exit(&au_stat_lock);

	return (0);

}

static int
setumask(caddr_t data)
{
	STRUCT_DECL(auditinfo, user_info);
	struct proc *p;
	struct p_audit_data *pad;
	model_t	model;

	model = get_udatamodel();
	STRUCT_INIT(user_info, model);

	if (copyin(data, STRUCT_BUF(user_info), STRUCT_SIZE(user_info)))
		return (EFAULT);

	mutex_enter(&pidlock);	/* lock the process queue against updates */

	for (p = practive; p != NULL; p = p->p_next) {
		pad = (struct p_audit_data *)P2A(p);
		if (!pad) continue;
		mutex_enter(&(pad->pad_lock));
		if (pad->pad_auid == STRUCT_FGET(user_info, ai_auid)) {
			pad->pad_mask = STRUCT_FGET(user_info, ai_mask);
		}
		mutex_exit(&(pad->pad_lock));
	}
	mutex_exit(&pidlock);

	return (0);
}

static int
setsmask(caddr_t data)
{
	STRUCT_DECL(auditinfo, user_info);
	struct proc *p;
	struct p_audit_data *pad;
	model_t	model;

	model = get_udatamodel();
	STRUCT_INIT(user_info, model);

	if (copyin(data, STRUCT_BUF(user_info), STRUCT_SIZE(user_info)))
		return (EFAULT);

	mutex_enter(&pidlock);	/* lock the process queue against updates */

	for (p = practive; p != NULL; p = p->p_next) {
		pad = (struct p_audit_data *)P2A(p);
		if (!pad) continue;
		mutex_enter(&(pad->pad_lock));
		if (pad->pad_asid == STRUCT_FGET(user_info, ai_asid)) {
			pad->pad_mask = STRUCT_FGET(user_info, ai_mask);
		}
		mutex_exit(&(pad->pad_lock));
	}
	mutex_exit(&pidlock);

	return (0);
}

/*
 * Get the current audit state of the system
 */

static int
getcond(caddr_t data)
{
	if (copyout(&au_auditstate, data, sizeof (int)))
		return (EFAULT);
	return (0);
}

/*
 * Set the current audit state of the system to on (AUC_AUDITING) or
 * off (AUC_NOAUDIT), return the previous audit state.
 */
/* ARGSUSED */
static int
setcond(caddr_t data, rval_t *rvp)
{
	int	auditstate, audit_state_saved;
	if (copyin(data, &auditstate, sizeof (int)))
		return (EFAULT);

	rvp->r_val1 = au_auditstate;

	audit_state_saved = au_auditstate;
	switch (auditstate) {
	case AUC_AUDITING:		/* Turn auditing on */
		if (au_auditstate == AUC_AUDITING)
			break;
		au_auditstate = AUC_AUDITING;

		/* Initialize global values */

		mutex_enter(&checktime_lock);
		au_checktime = (time_t)0;
		mutex_exit(&checktime_lock);
		break;
	case AUC_NOAUDIT:		/* Turn auditing off */
		if (au_auditstate == AUC_NOAUDIT)
			break;
		au_auditstate = AUC_NOAUDIT;

		/* clear out the audit queue */

		if (au_queue.wt_block)
			cv_broadcast(&au_queue.write_cv);
		break;
	default:
		return (EINVAL);
	}
	if (copyout(&audit_state_saved, data, sizeof (int)))
		return (EFAULT);
	return (0);
}

static int
getclass(caddr_t data)
{
	au_evclass_map_t event;

	if (copyin(data, &event, sizeof (au_evclass_map_t)))
		return (EFAULT);

	if (event.ec_number < 0 || event.ec_number > (naevent-1))
		return (EINVAL);

	event.ec_class = audit_ets[event.ec_number];

	if (copyout(&event, data, sizeof (au_evclass_map_t)))
		return (EFAULT);

	return (0);
}

static int
setclass(caddr_t data)
{
	au_evclass_map_t event;

	if (copyin(data, &event, sizeof (au_evclass_map_t)))
		return (EFAULT);

	if (event.ec_number < 0 || event.ec_number > (naevent-1))
		return (EINVAL);

	audit_ets[event.ec_number] = event.ec_class;

	return (0);
}

static int
getpinfo(caddr_t data)
{
	STRUCT_DECL(auditpinfo, apinfo);
	proc_t *proc;
	struct p_audit_data *pad;
	model_t	model;

	model = get_udatamodel();
	STRUCT_INIT(apinfo, model);

	if (copyin(data, STRUCT_BUF(apinfo), STRUCT_SIZE(apinfo)))
		return (EFAULT);

	mutex_enter(&pidlock);
	if ((proc = prfind(STRUCT_FGET(apinfo, ap_pid))) == NULL) {
		mutex_exit(&pidlock);
		return (EINVAL);
	}
	mutex_enter(&proc->p_lock);	/* so process doesn't go away */
	mutex_exit(&pidlock);

	pad = (struct p_audit_data *)proc->p_audit_data;
#ifdef NOTDEF
	ASSERT(pad != NULL);
#endif
	if (!pad) {
		mutex_exit(&proc->p_lock);
		return (EINVAL);
	}

	/* designated process has an ipv6 address? */
	if (pad->pad_termid.at_type == AU_IPv6) {
		mutex_exit(&proc->p_lock);
		return (EOVERFLOW);
	}

	mutex_enter(&(pad->pad_lock));
	STRUCT_FSET(apinfo, ap_auid, pad->pad_auid);
	STRUCT_FSET(apinfo, ap_asid, pad->pad_asid);
	STRUCT_FSET(apinfo, ap_termid.port, DEVCMPL(pad->pad_termid.at_port));
	STRUCT_FSET(apinfo, ap_termid.machine, pad->pad_termid.at_addr[0]);
	STRUCT_FSET(apinfo, ap_mask, pad->pad_mask);
	mutex_exit(&(pad->pad_lock));
	mutex_exit(&proc->p_lock);

	if (copyout(STRUCT_BUF(apinfo), data, STRUCT_SIZE(apinfo)))
		return (EFAULT);

	return (0);
}

static int
getpinfo_addr(caddr_t data, int len)
{
	STRUCT_DECL(auditpinfo_addr, apinfo);
	proc_t *proc;
	struct p_audit_data *pad;
	model_t	model;

	model = get_udatamodel();
	STRUCT_INIT(apinfo, model);

	if (len < STRUCT_SIZE(apinfo))
		return (EOVERFLOW);

	if (copyin(data, STRUCT_BUF(apinfo), STRUCT_SIZE(apinfo)))
		return (EFAULT);

	mutex_enter(&pidlock);
	if ((proc = prfind(STRUCT_FGET(apinfo, ap_pid))) == NULL) {
		mutex_exit(&pidlock);
		return (EINVAL);
	}
	mutex_enter(&proc->p_lock);	/* so process doesn't go away */
	mutex_exit(&pidlock);

	pad = (struct p_audit_data *)proc->p_audit_data;
#ifdef NOTDEF
	ASSERT(pad != NULL);
#endif
	if (!pad) {
		mutex_exit(&proc->p_lock);
		return (EINVAL);
	}

	mutex_enter(&(pad->pad_lock));
	STRUCT_FSET(apinfo, ap_auid, pad->pad_auid);
	STRUCT_FSET(apinfo, ap_asid, pad->pad_asid);
	STRUCT_FSET(apinfo, ap_termid.at_port,
		DEVCMPL(pad->pad_termid.at_port));
	STRUCT_FSET(apinfo, ap_termid.at_type, pad->pad_termid.at_type);
	STRUCT_FSET(apinfo, ap_termid.at_addr[0], pad->pad_termid.at_addr[0]);
	STRUCT_FSET(apinfo, ap_termid.at_addr[1], pad->pad_termid.at_addr[1]);
	STRUCT_FSET(apinfo, ap_termid.at_addr[2], pad->pad_termid.at_addr[2]);
	STRUCT_FSET(apinfo, ap_termid.at_addr[3], pad->pad_termid.at_addr[3]);
	STRUCT_FSET(apinfo, ap_mask, pad->pad_mask);
	mutex_exit(&(pad->pad_lock));
	mutex_exit(&proc->p_lock);

	if (copyout(STRUCT_BUF(apinfo), data, STRUCT_SIZE(apinfo)))
		return (EFAULT);

	return (0);
}

static int
setpmask(caddr_t data)
{
	STRUCT_DECL(auditpinfo, apinfo);
	proc_t *proc;
	struct p_audit_data *pad;
	model_t	model;

	model = get_udatamodel();
	STRUCT_INIT(apinfo, model);

	if (copyin(data, STRUCT_BUF(apinfo), STRUCT_SIZE(apinfo)))
		return (EFAULT);

	mutex_enter(&pidlock);
	if ((proc = prfind(STRUCT_FGET(apinfo, ap_pid))) == NULL) {
		mutex_exit(&pidlock);
		return (EINVAL);
	}
	mutex_enter(&proc->p_lock);	/* so process doesn't go away */
	mutex_exit(&pidlock);

	pad = (struct p_audit_data *)proc->p_audit_data;
#ifdef NOTDEF
	ASSERT(pad != NULL);
#endif
	if (!pad) {
		mutex_exit(&proc->p_lock);
		return (EINVAL);
	}

	mutex_enter(&(pad->pad_lock));
	pad->pad_mask = STRUCT_FGET(apinfo, ap_mask);
	mutex_exit(&(pad->pad_lock));
	mutex_exit(&proc->p_lock);

	return (0);
}

static int
getfsize(caddr_t data)
{
	au_fstat_t fstat;

	mutex_enter(&au_fstat_lock);
	fstat.af_filesz = audit_file_stat.af_filesz;
	fstat.af_currsz = audit_file_stat.af_currsz;
	mutex_exit(&au_fstat_lock);

	if (copyout(&fstat, data, sizeof (au_fstat_t)))
		return (EFAULT);

	return (0);
}

static int
setfsize(caddr_t data)
{
	au_fstat_t fstat;

	if (copyin(data, &fstat, sizeof (au_fstat_t)))
		return (EFAULT);

	if (fstat.af_filesz < au_min_file_sz)
		return (EINVAL);

	mutex_enter(&au_fstat_lock);
	audit_file_stat.af_filesz = fstat.af_filesz;
	mutex_exit(&au_fstat_lock);

	return (0);
}
/*
 * The out of control system call
 * This is audit kitchen sink aka auditadm, aka auditon
 */
static int
auditctl(
	int	cmd,
	caddr_t data,
	int	length,
	rval_t	*rvp)
{
	int result;

	if (!audit_active)
		return (EINVAL);

	if (!suser(CRED()))
		return (EPERM);

	switch (cmd) {
	case A_GETPOLICY:
		result = getpolicy(data);
		break;
	case A_SETPOLICY:
		result = setpolicy(data);
		break;
	case A_GETKMASK:
		result = getkmask(data);
		break;
	case A_SETKMASK:
		result = setkmask(data);
		break;
	case A_GETKAUDIT:
		result = getkaudit(data, length);
		break;
	case A_SETKAUDIT:
		result = setkaudit(data, length);
		break;
	case A_GETQCTRL:
		result = getqctrl(data);
		break;
	case A_SETQCTRL:
		result = setqctrl(data);
		break;
	case A_GETCWD:
		result = getcwd(data, length);
		break;
	case A_GETCAR:
		result = getcar(data, length);
		break;
	case A_GETSTAT:
		result = getstat(data);
		break;
	case A_SETSTAT:
		result = setstat(data);
		break;
	case A_SETUMASK:
		result = setumask(data);
		break;
	case A_SETSMASK:
		result = setsmask(data);
		break;
	case A_GETCOND:
		result = getcond(data);
		break;
	case A_SETCOND:
		result = setcond(data, rvp);
		break;
	case A_GETCLASS:
		result = getclass(data);
		break;
	case A_SETCLASS:
		result = setclass(data);
		break;
	case A_GETPINFO:
		result = getpinfo(data);
		break;
	case A_GETPINFO_ADDR:
		result = getpinfo_addr(data, length);
		break;
	case A_SETPMASK:
		result = setpmask(data);
		break;
	case A_SETFSIZE:
		result = setfsize(data);
		break;
	case A_GETFSIZE:
		result = getfsize(data);
		break;
	default:
		result = EINVAL;
		break;
	}
	return (result);
}


static int
audit_modsysent(char *modname, int flags, int (*func)())
{
	struct sysent *sysp;
	int sysnum;
	krwlock_t *kl;

	if ((sysnum = mod_getsysnum(modname)) == -1) {
		cmn_err(CE_WARN, "system call missing from bind file");
		return (-1);
	}

	kl = (krwlock_t *)kobj_zalloc(sizeof (krwlock_t), KM_SLEEP);

	sysp = &sysent[sysnum];
	sysp->sy_narg = 2;
#ifdef _LP64
	sysp->sy_flags = (unsigned short)flags;
#else
	sysp->sy_flags = (unsigned char)flags;
#endif
	sysp->sy_call = func;
	sysp->sy_lock = kl;

#ifdef _SYSCALL32_IMPL
	sysp = &sysent32[sysnum];
	sysp->sy_narg = 2;
	sysp->sy_flags = (unsigned short)flags;
	sysp->sy_call = func;
	sysp->sy_lock = kl;

#endif
	rw_init(sysp->sy_lock, NULL, RW_DEFAULT, NULL);

	return (0);
}
