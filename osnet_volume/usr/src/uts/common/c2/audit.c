/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * @(#)audit.c 2.28 92/03/04 SMI; SunOS CMW
 * @(#)audit.c 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * This file contains the audit hook support code for auditing.
 */

#pragma ident	"@(#)audit.c	1.109	99/12/06 SMI"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/stropts.h>
#include <sys/systm.h>
#include <sys/pathname.h>
#include <sys/syscall.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/kmem.h>		/* for KM_SLEEP */
#include <sys/socket.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/thread.h>
#include <netinet/in.h>
#include <c2/audit.h>		/* needs to be included before user.h */
#include <c2/audit_kernel.h>	/* for M_DONTWAIT */
#include <c2/audit_kevents.h>
#include <c2/audit_record.h>
#include <sys/strsubr.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <sys/model.h>		/* for model_t */

void add_return_token(caddr_t *, unsigned int scid, int err, int rval);

extern int audit_active;
extern int audit_policy;
extern int au_auditstate;
extern kmutex_t  cwrd_lock;
extern kmutex_t  au_stat_lock;

extern kmutex_t  au_tlist_lock;
extern kmutex_t  au_flist_lock;
extern kmutex_t  au_plist_lock;
extern int tlist_size;
extern int flist_size;
extern int plist_size;
extern p_audit_data_t *au_plist;
extern t_audit_data_t *au_tlist;
extern f_audit_data_t *au_flist;
extern struct au_list_stat au_plist_stat;
extern struct au_list_stat au_tlist_stat;
extern struct au_list_stat au_flist_stat;

extern t_audit_data_t *au_tlist;

extern struct p_audit_data *pad0;
extern struct t_audit_data *tad0;

#define	ROOT	1	/* copy only ROOT path */
#define	DIR	2	/* copy only DIR  path */
/*
 * the following defines are unique per module
 */



/*
 * ROUTINE:	AUDIT_NEWPROC
 * PURPOSE:	initialize the child p_audit_data structure
 * CALLBY:	GETPROC
 * NOTE:	All threads for the parent process are locked at this point.
 *		We are essentially running singled threaded for this reason.
 *		GETPROC is called when system creates a new process.
 *		By the time AUDIT_NEWPROC is called, the child proc
 *		structure has already been initialized. What we need
 *		to do is to allocate the child p_audit_data and
 *		initialize it with the content of current parent process.
 */

void
audit_newproc(struct proc *cp)	/* initialized child proc structure */
{
	p_audit_data_t *pad;	/* child process audit data */
	p_audit_data_t *opad;	/* parent process audit data */

	dprintf(4, ("audit_newproc(%p)\n", (void *)cp));
	call_debug(4);

	/* child p_audit_data must be null for a new process */
	ASSERT(P2A(cp) == (caddr_t)0);

	mutex_enter(&au_plist_lock);
	if ((pad = au_plist) == NULL) {
		PROC_MISS;
		AS_INC(as_memused, sizeof (struct p_audit_data));
		pad = kmem_zalloc(sizeof (struct p_audit_data), KM_SLEEP);
	} else {
		PROC_HIT;
		au_plist = pad->next;
		bzero(pad, sizeof (struct p_audit_data));
	}
	mutex_exit(&au_plist_lock);

	P2A(cp) = (caddr_t)pad;

	opad = (struct p_audit_data *)P2A(curproc);

	dprintf(4, ("pad: %p opad: %p child pid: %x", (void *)pad, (void *)opad,
		cp->p_pid));
	call_debug(4);

#ifdef NOTDEF
		/*
		 * sanity checks to ensure everything has been cleaned up.
		 * Note: only need this when pad's are statically allocated
		 *	array of structures that are reused. Only applicable
		 *	to 4.1 systems.
		 */
	ASSERT(pad->pad_cwrd != 0);
#endif

		/*
		 * copy the audit data. Note that all threads of current
		 *   process have been "held". Thus there is no race condition
		 *   here with mutiple threads trying to alter the cwrd
		 *   structure (such as releasing it).
		 *
		 *   We still want to hold things since auditon() [A_SETUMASK,
		 *   A_SETSMASK] could be walking through the processes to
		 *   update things.
		 *
		 */
	mutex_enter(&opad->pad_lock);	/* lock opad structure during copy */
	*pad = *opad; 			/* copy parent's process audit data */
	mutex_exit(&opad->pad_lock);	/* current proc will keep cwrd open */

		/*
		 * lock other process that reference cwrd. Note that all
		 *   threads of current process have been locked. Thus there
		 *   is no race condition with a thread doing an exit while
		 *   we are doing a fork and causing pad_cwrd to be released.
		 *   The parent process will keep the cwrd "open" while we
		 *   copy it. We want to lock the cred while we adjust it's
		 *   reference count.
		 */
	mutex_enter(&cwrd_lock);	/* lock cwrd when we "copy" it */
	bsm_cwincr(pad->pad_cwrd);	/* increment cwd reference count */
	mutex_exit(&cwrd_lock);

		/*
		 * We are still single threaded so we can touch things here
		 */
	mutex_init(&pad->pad_lock, NULL, MUTEX_DEFAULT, NULL);

	ADDTRACE("[%p] audit_newproc cp: %p pad: %p t: %p tad: %p",
	    (void *)cp, (void *)pad, (void *)curthread, (void *)T2A(curthread),
	    0, 0);

		/*
		 * finish auditing of parent here so that it will be done
		 * before child has a chance to run. We include the child
		 * pid since the return value in the return token is a dummy
		 * one and contains no useful information (it is included to
		 * make the audit record structure consistant).
		 *
		 * tad_flag is set if auditing is on
		 */
	if (((t_audit_data_t *)T2A(curthread))->tad_flag)
		au_uwrite(au_to_arg32(0, "child PID", (uint32_t)cp->p_pid));

	/*
	 * finish up audit record generation here because child process
	 * is set to run before parent process. We distinguish here
	 * between FORK, FORK1, or VFORK by the saved system call ID.
	 */
	audit_finish(0, ((t_audit_data_t *)T2A(curthread))->tad_scid, 0, 0);

	dprintf(4, ("exit audit_newproc\n"));
	call_debug(4);
}

/*
 * ROUTINE:	AUDIT_PFREE
 * PURPOSE:	deallocate the per-process udit data structure
 * CALLBY:	EXIT
 *		FORK_FAIL
 * NOTE:	all lwp except current one have stopped in EXITLWP
 * 		why we are single threaded?
 *		. all lwp except current one have stopped in EXITLWP.
 *		should cwrd_lock be made per process group instead of global ?
 */
void
audit_pfree(struct proc *p)		/* proc structure to be freed */

{	/* AUDIT_PFREE */

	p_audit_data_t *pad;

	pad = (struct p_audit_data *)P2A(p);

	dprintf(4, ("audit_pfree(%p) pad: %p\n", (void *)p, (void *)pad));
	call_debug(4);

	/* better be a per process audit data structure */
	ASSERT(pad != (p_audit_data_t *)0);

	if (pad == pad0) {
		dprintf(0x40, ("freeing pad0\n"));
		return;
	}

	/* deallocate all auditing resources for this process */
	mutex_enter(&cwrd_lock);	/* lock cwrd during cwrd being freed */
	bsm_cwfree(pad->pad_cwrd);
	mutex_exit(&cwrd_lock);

	/*
	 * free per process audit data. No lock needed since we're
	 * single threaded here in exitlwp.
	 */

	mutex_enter(&au_plist_lock);
	if (au_plist_stat.size > plist_size) {
		/* free per proc audit data */
		AS_DEC(as_memused, sizeof (struct p_audit_data));
		kmem_free(pad, sizeof (struct p_audit_data));
		PROC_FREE;
	} else {
		/* put it on the list */
		pad->next = au_plist;
		au_plist = pad;
		au_plist_stat.size++;
	}
	mutex_exit(&au_plist_lock);

		/* no longer any per process audit structure off of process */
	p->p_audit_data = 0;

	ADDTRACE("[%x] audit_pfree p: %p pad: %p size: %x",
		p, pad, sizeof (struct p_audit_data), 0, 0, 0);
	dprintf(4, ("exit audit_pfree\n"));
	call_debug(4);
}

/*
 * ROUTINE:	AUDIT_THREAD_CREATE
 * PURPOSE:	allocate per-process thread audit data structure
 * CALLBY:	THREAD_CREATE
 * NOTE:	only the stack and thread links have been setup at this point
 *		We are single threaded in this routine.
 * TODO:
 * QUESTION:
 */

void
audit_thread_create(kthread_id_t t, int state)
{
	t_audit_data_t *tad;	/* per-thread audit data */

	dprintf(8, ("audit_thread_create(%p,%x)\n", (void *)t, state));
	call_debug(8);

	/* the per-thread audit data structure must be null */
	ASSERT(T2A(t) == (caddr_t)0);

	mutex_enter(&au_tlist_lock);
	if ((tad = au_tlist) == NULL) {
		THREAD_MISS;
		AS_INC(as_memused, sizeof (struct t_audit_data));

		/* allocate per process thread audit data */
		if ((tad = kmem_zalloc(sizeof (struct t_audit_data),
		    state == TS_ONPROC ? KM_NOSLEEP : KM_SLEEP)) == NULL) {
			cmn_err(CE_PANIC, "AUDIT_THREAD_CREATE: out of memory");
		}
	} else {
		THREAD_HIT;
		au_tlist = tad->next;
		bzero(tad, sizeof (struct t_audit_data));
	}
	mutex_exit(&au_tlist_lock);

	T2A(t) = (caddr_t)tad;	/* set up thread audit data ptr */
	tad->tad_ad = NULL;	/* base of accumulated audit data */
	tad->tad_thread = t;	/* back ptr to thread: DEBUG */

	ADDTRACE("[%p] audit_thread_create t: %p tad: %p proc: %p",
		(void *)t, (void *)tad, (void *)curproc, 0, 0, 0);
	dprintf(8, ("exit audit_thread_create: %p\n", (void *)T2A(t)));
	call_debug(8);
}

/*
 * ROUTINE:	AUDIT_THREAD_FREE
 * PURPOSE:	free the per-thread audit data structure
 * CALLBY:	THREAD_FREE
 * NOTE:	most thread data is clear after return
 * TODO:
 * QUESTION:	who clears the tad_path
 */

void
audit_thread_free(kthread_id_t t)
{
	t_audit_data_t *tad;

	dprintf(8, ("audit_thread_free(%p)\n", (void *)t));
	call_debug(8);

	tad = (t_audit_data_t *)T2A(t);

	ADDTRACE("[%x] audit_thread_free t: %p tad: %p proc: %p",
	    (void *)t, (void *)tad, (void *)curproc, 0, 0, 0);
	dprintf(8, ("tad: %p\n", (void *)tad));
	call_debug(8);

	/* thread audit data must still be set */
#ifdef NOT_DEF
	ASSERT(tad != (t_audit_data_t *)0);
#endif
	if (tad == tad0) {
		dprintf(0x40, ("freeing t0\n"));
		return;
	}

	if (tad == NULL) {
		dprintf(0x40, ("audit_thread is null(%p)\n", (void *)t));
		return;
	}

	/* must not have any audit record residual */
	if (tad->tad_ad != NULL) {
		printf("thread %p audit_data %p\n", (void *)t, (void *)tad);
		cmn_err(CE_PANIC, "AUDIT_THREAD_FREE: audit record not empty");
	}

	/* saved path must be empty */
	if (tad->tad_pathlen || tad->tad_path) {
		printf("thread: %p audit_data %p\n", (void *)t, (void *)tad);
		cmn_err(CE_PANIC, "AUDIT_THREAD_FREE: saved path not empty");
	}

	mutex_enter(&au_tlist_lock);
	if (au_tlist_stat.size > tlist_size) {
		/* free per thread audit data */
		AS_DEC(as_memused, sizeof (struct t_audit_data));
		kmem_free(tad, sizeof (struct t_audit_data));
		THREAD_FREE;
	} else {
		/* put it on the list */
		tad->next = au_tlist;
		au_tlist = tad;
		au_tlist_stat.size++;
	}
	mutex_exit(&au_tlist_lock);
	t->t_audit_data = 0;

	ADDTRACE("[%x] audit_thread_free t: %p tad: %p proc: %p",
		(void *)t, (void *)tad, (void *)curproc, 0, 0, 0);
	dprintf(8, ("exit audit_thread_free\n"));
	call_debug(8);

	return;

}	/* AUDIT_THREAD_FREE */







/*
 * ROUTINE:	AUDIT_SAVEPATH
 * PURPOSE:
 * CALLBY:	LOOKUPPN
 *
 * NOTE:	We have reached the end of a path in fs/lookup.c.
 *		We get two pieces of information here:
 *		the vnode of the last component (vp) and
 *		the status of the last access (flag).
 * TODO:
 * QUESTION:
 */

/*ARGSUSED*/
int
audit_savepath(pnp, vp, flag, cr)

	struct pathname *pnp;		/* pathname to lookup */
	struct vnode *vp;		/* vnode of the last component */
	int    flag;			/* status of the last access */
	cred_t *cr;			/* cred of requestor */

{	/* AUDIT_SAVEPATH */

	char *pp;	/* pointer to path */
	char *sp;	/* saved initial pp */
	int len;	/* length of archived name */
	uint_t len_crwd;	/* length of CWD or CRD */
	char *p;	/* pointer to CWD or CRD string */
	t_audit_data_t *tad;	/* current thread */
	p_audit_data_t *pad;	/* current process */

	dprintf(0x10, ("audit_savepath(%p, %p, %x)\n", (void *)pnp,
	    (void *)vp, flag));
	call_debug(0x10);

	tad = (t_audit_data_t *)U2A(u);
	ASSERT(tad != (t_audit_data_t *)0);
	pad = (p_audit_data_t *)P2A(curproc);
	ASSERT(pad != (p_audit_data_t *)0);

	dprintf(0x10, ("pad: %p tad: %p tad_flag %x tad_ctrl %x\n",
		(void *)pad, (void *)tad, tad->tad_flag, tad->tad_ctrl));
	call_debug(0x10);

	/*
	 * this event being audited or do we need path information
	 * later? This might be for a chdir/chroot or open (add path
	 * to file pointer. If the path has already been found for an
	 * open/creat then we don't need to process the path.
	 *
	 * S2E_SP (PAD_SAVPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	chroot, chdir, open, creat system call processing. It determines
	 *	if audit_savepath() will discard the path or we need it later.
	 * PAD_PATHFND means path already included in this audit record. It
	 *	is used in cases where multiple path lookups are done per
	 *	system call. The policy flag, AUDIT_PATH, controls if multiple
	 *	paths are allowed.
	 * S2E_NPT (PAD_NOPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	exit processing to inhibit any paths that may be added due to
	 *	closes.
	 */
	if ((tad->tad_flag == 0 && !(tad->tad_ctrl & PAD_SAVPATH)) ||
		((tad->tad_ctrl & PAD_PATHFND) &&
		!(audit_policy & AUDIT_PATH)) ||
		(tad->tad_ctrl & PAD_NOPATH)) {
			return (0);
	}

	/* length of path processed */
	len = (pnp->pn_path - pnp->pn_buf) + 1;		/* +1 for new / */

	/* adjust for CWD or CRD */
	mutex_enter(&pad->pad_lock);
	if (tad->tad_ctrl & PAD_ABSPATH) {
		len_crwd = pad->pad_cwrd->cwrd_rootlen;
		p = pad->pad_cwrd->cwrd_root;
	} else {
		len_crwd = pad->pad_cwrd->cwrd_dirlen;
		p = pad->pad_cwrd->cwrd_dir;
	}

	/* for case where multiple lookups in one syscall (rename) */
	tad->tad_ctrl &= ~PAD_ABSPATH;

	/* adjust for length of saved string (might be 0) */
	len = len + len_crwd + tad->tad_pathlen;

	/* get an expanded buffer to hold the anchored path */
	AS_INC(as_memused, len);
	sp = kmem_alloc((uint_t)len, KM_SLEEP);

	/* anchor path with string from cwrd structure */
	bcopy(p, sp, len_crwd-1);		/* -1 for end NULL */
	mutex_exit(&pad->pad_lock);

	/* advance pointer to end of CWD or CRD */
	pp = &sp[len_crwd-1];			/* -1 for end NULL */
	*pp++ = '/';

	/* add relative path (if there is one) */
	if (tad->tad_pathlen) {
		/* -1 for end NULL */
		bcopy(tad->tad_path, pp, tad->tad_pathlen-1);
		AS_DEC(as_memused, tad->tad_pathlen);
		kmem_free(tad->tad_path, tad->tad_pathlen);
		pp += tad->tad_pathlen-1;	/* -1 for end NULL */
		*pp++ = '/';
		tad->tad_path = NULL;
		tad->tad_pathlen = 0;
		tad->tad_vn = (struct vnode *)0;
	}
	dprintf(0x10, ("sp %p pp %p len %d\n", (void *)sp, (void *)pp, len));
	call_debug(0x10);

	/* now add string of processed path */
	bcopy(pnp->pn_buf, pp, (uint_t)(pnp->pn_path - pnp->pn_buf));
	if (len < 1) {
		printf("pnp: %p tad: %p len: %d\n", (void *)pnp, (void *)tad,
		    len);
		cmn_err(CE_PANIC, "AUDIT_SAVEPATH:  negative path length");
	}
	sp[len-1] = '\0';

	tad->tad_pathlen = len;
	tad->tad_path = sp;
	tad->tad_vn = vp;

	/*
	 * are we auditing only if error, or if it is not open or create
	 * otherwise audit_setf will do it
	 */

#ifdef NOTDEF
	if (tad->tad_flag && (flag || (tad->tad_scid != SYS_open &&
		tad->tad_scid != SYS_creat))) {
#endif
	if (tad->tad_flag) {
		dprintf(0x10, ("pathsave-before au_to_path: %p", (void *)tad));
		call_debug(0x10);
		if (flag && (tad->tad_scid == SYS_open ||
			tad->tad_scid == SYS_open64 ||
			tad->tad_scid == SYS_creat ||
			tad->tad_scid == SYS_creat64)) {
			tad->tad_ctrl |= PAD_TRUE_CREATE;
		}

		/* add token to audit record for this name */
		au_uwrite(au_to_path(tad->tad_path, tad->tad_pathlen));
		dprintf(0x10, ("pathsave-after au_to_path: %p", (void *)tad));
		call_debug(0x10);

		if (vp) { /* add the attributes of the object */
			/*
			 * only capture attributes when there is no error
			 * lookup will not return the vnode of the failing
			 * component.
			 *
			 * if there was a lookup error, then don't add
			 * attribute. if lookup in vn_create(),
			 * then don't add attribute,
			 * it will be added at end of vn_create().
			 */
			if (!flag && !(tad->tad_ctrl & PAD_NOATTRB))
				audit_attributes(vp);
			dprintf(0x10, ("pathsave-after attributes: %p",
					(void *)tad));
			call_debug(0x10);
		}
	}

	/* free up space if we're not going to save path (open, crate) */
	if ((tad->tad_ctrl & PAD_SAVPATH) == 0) {
		if (tad->tad_pathlen) {
			AS_DEC(as_memused, tad->tad_pathlen);
			kmem_free(tad->tad_path, tad->tad_pathlen);
			tad->tad_pathlen = 0;
			tad->tad_path = (caddr_t)0;
			tad->tad_vn = (struct vnode *)0;
		}
	}
	if (tad->tad_ctrl & PAD_MLD)
		tad->tad_ctrl |= PAD_PATHFND;

	dprintf(0x10, ("end audit_savepath: path %p", (void *)tad->tad_path));
	call_debug(0x10);

	return (0);

}	/* AUDIT_SAVEPATH */






/*ARGSUSED*/

/*
 * ROUTINE:	AUDIT_ADDCOMPONENT
 * PURPOSE:	extend the path by the component accepted
 * CALLBY:	LOOKUPPN
 * NOTE:	This function is called only when there is an error in
 *		parsing a path component
 * TODO:	Add the error component to audit record
 * QUESTION:	what is this for
 */

void
audit_addcomponent(pnp)

	struct pathname *pnp;

{	/* AUDIT_ADDCOMPONENT */

	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);
	/*
	 * S2E_SP (PAD_SAVPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	chroot, chdir, open, creat system call processing. It determines
	 *	if audit_savepath() will discard the path or we need it later.
	 * PAD_PATHFND means path already included in this audit record. It
	 *	is used in cases where multiple path lookups are done per
	 *	system call. The policy flag, AUDIT_PATH, controls if multiple
	 *	paths are allowed.
	 * S2E_NPT (PAD_NOPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	exit processing to inhibit any paths that may be added due to
	 *	closes.
	 */
	if ((tad->tad_flag == 0 && !(tad->tad_ctrl & PAD_SAVPATH)) ||
		((tad->tad_ctrl & PAD_PATHFND) &&
		!(audit_policy & AUDIT_PATH)) ||
		(tad->tad_ctrl & PAD_NOPATH)) {
			return;
	}

	return;

}	/* AUDIT_ADDCOMPONENT */








/*
 * ROUTINE:	AUDIT_ANCHORPATH
 * PURPOSE:
 * CALLBY:	LOOKUPPN
 * NOTE:
 * anchor path at "/". We have seen a symbolic link or entering for the
 * first time we will throw away any saved path if path is anchored.
 *
 * flag = 0, path is relative.
 * flag = 1, path is absolute. Free any saved path and set flag to PAD_ABSPATH.
 *
 * If the (new) path is absolute, then we have to throw away whatever we have
 * already accumulated since it is being superceeded by new path which is
 * anchored at the root.
 *		Note that if the path is relative, this function does nothing
 * TODO:
 * QUESTION:
 */
/*ARGSUSED*/
void
audit_anchorpath(struct pathname *pnp, int flag)
{
	t_audit_data_t *tad;

	dprintf(0x10, ("audit_anchorpath(%p,%x)\n", (void *)pnp, flag));
	call_debug(0x10);

	tad = (t_audit_data_t *)U2A(u);

	dprintf(0x10, ("tad: %p\n", (void *)tad));
	call_debug(0x10);

	/*
	 * this event being audited or do we need path information
	 * later? This might be for a chdir/chroot or open (add path
	 * to file pointer. If the path has already been found for an
	 * open/creat then we don't need to process the path.
	 *
	 * S2E_SP (PAD_SAVPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	chroot, chdir, open, creat system call processing. It determines
	 *	if audit_savepath() will discard the path or we need it later.
	 * PAD_PATHFND means path already included in this audit record. It
	 *	is used in cases where multiple path lookups are done per
	 *	system call. The policy flag, AUDIT_PATH, controls if multiple
	 *	paths are allowed.
	 * S2E_NPT (PAD_NOPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	exit processing to inhibit any paths that may be added due to
	 *	closes.
	 */
	if ((tad->tad_flag == 0 && !(tad->tad_ctrl & PAD_SAVPATH)) ||
		((tad->tad_ctrl & PAD_PATHFND) &&
		!(audit_policy & AUDIT_PATH)) ||
		(tad->tad_ctrl & PAD_NOPATH)) {
			return;
	}
	dprintf(0x10, ("audit_anchorpath\n"));
	call_debug(0x10);

	if (flag) {
		tad->tad_ctrl |= PAD_ABSPATH;
		if (tad->tad_pathlen) {
			AS_DEC(as_memused, tad->tad_pathlen);
			kmem_free(tad->tad_path, tad->tad_pathlen);
			tad->tad_pathlen = 0;
			tad->tad_path = (caddr_t)0;
			tad->tad_vn = (struct vnode *)0;
		}
	}
	dprintf(0x10, ("exit audit_anchorpath\n"));
	call_debug(0x10);
}


/*
 * symbolic link. Save previous components.
 *
 * the path seen so far looks like this
 *
 *  +-----------------------+----------------+
 *  | path processed so far | remaining path |
 *  +-----------------------+----------------+
 *  \-----------------------/
 *	save this string if
 *	symbolic link relative
 *	(but don't include  symlink component)
 */

/*ARGSUSED*/


/*
 * ROUTINE:	AUDIT_SYMLINK
 * PURPOSE:
 * CALLBY:	LOOKUPPN
 * NOTE:
 * TODO:
 * QUESTION:
 */
void
audit_symlink(struct pathname *pnp, struct pathname *sympath)
{
	char *pp;	/* pointer to path */
	char *sp;	/* saved initial pp */
	char *cp;	/* start of symlink path */
	uint_t len;	/* length of archived name */
	uint_t len_path;	/* processed path before symlink */
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);

	/*
	 * this event being audited or do we need path information
	 * later? This might be for a chdir/chroot or open (add path
	 * to file pointer. If the path has already been found for an
	 * open/creat then we don't need to process the path.
	 *
	 * S2E_SP (PAD_SAVPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	chroot, chdir, open, creat system call processing. It determines
	 *	if audit_savepath() will discard the path or we need it later.
	 * PAD_PATHFND means path already included in this audit record. It
	 *	is used in cases where multiple path lookups are done per
	 *	system call. The policy flag, AUDIT_PATH, controls if multiple
	 *	paths are allowed.
	 * S2E_NPT (PAD_NOPATH) flag comes from audit_s2e[].au_ctrl. Used with
	 *	exit processing to inhibit any paths that may be added due to
	 *	closes.
	 */
	if ((tad->tad_flag == 0 && !(tad->tad_ctrl & PAD_SAVPATH)) ||
		((tad->tad_ctrl & PAD_PATHFND) &&
		!(audit_policy & AUDIT_PATH)) || (tad->tad_ctrl & PAD_NOPATH)) {
			return;
	}
	dprintf(0x10, ("audit_symlink(%p,%p) tad: %p\n", (void *)pnp,
	    (void *)sympath, (void *)tad));
	call_debug(0x10);

	/*
	 * if symbolic link is anchored at / then do nothing.
	 * When we cycle back to begin: in lookuppn() we will
	 * call audit_anchorpath() with a flag indicating if the
	 * path is anchored at / or is relative. We will release
	 * any saved path at that point.
	 *
	 * Note In the event that an error occurs in pn_combine then
	 * we want to remain pointing at the component that caused the
	 * path to overflow the pnp structure.
	 */
	if (sympath->pn_buf[0] == '/')
		return;

	/* backup over last component */
	cp = pnp->pn_path;
	while (*--cp != '/' && cp > pnp->pn_buf)
		;

	len = (len_path = cp - pnp->pn_buf) + 1;	/* +1 for new / */

	dprintf(0x10, ("symlink-component: %p cp %p\n", (void *)pnp,
	    (void *)cp));
	dprintf(0x10, ("first len: %d first len_path: %d\n", len, len_path));
	call_debug(0x10);

	/* is there anything to add to the saved path?? */
	if (len_path) {
		if (tad->tad_pathlen) { /* expand p_audit_data path buffer */
			len += tad->tad_pathlen; /* path already has NULL */
			AS_INC(as_memused, len);
			sp = kmem_alloc(len, KM_SLEEP);
			/* -1 for end NULL */
			bcopy(tad->tad_path, sp, tad->tad_pathlen-1);
			AS_DEC(as_memused, tad->tad_pathlen);
			kmem_free(tad->tad_path, tad->tad_pathlen);
			pp = &sp[tad->tad_pathlen-1];
			tad->tad_path = NULL;
			tad->tad_pathlen = 0;
			tad->tad_vn = (struct vnode *)NULL;
		} else {
			len += 1;	/* for terminating NULL */
			AS_INC(as_memused, len);
			sp = pp = kmem_alloc(len, KM_SLEEP);
		}
		*pp++ = '/';		/* add / then add string */

		dprintf(0x10, ("sp %p pp %p len %d len_path %d\n",
			(void *)sp, (void *)pp, len, len_path));
		call_debug(0x10);

		bcopy(pnp->pn_buf, pp, len_path);
		sp[len-1] = '\0';

		tad->tad_pathlen = len;
		tad->tad_path = sp;
		tad->tad_vn = (struct vnode *)0;
	}
	dprintf(0x10, ("end audit_symlink\n"));
	call_debug(0x10);
}


/*
 * ROUTINE:	AUDIT_ATTRIBUTES
 * PURPOSE:	Audit the attributes so we can tell why the error occured
 * CALLBY:	AUDIT_SAVEPATH
 *		AUDIT_VNCREATE_FINISH
 *		AUS_FCHOWN...audit_event.c...audit_path.c
 * NOTE:
 * TODO:
 * QUESTION:
 */

void
audit_attributes(vp)

	struct vnode *vp;

{	/* AUDIT_ATTRIBUTES */

	struct vattr attr;

	if (vp) {
		attr.va_mask = AT_ALL;
		if (VOP_GETATTR(vp, &attr, 0, CRED()) == 0) {
			au_uwrite(au_to_attr(&attr));
		}
	}

	return;

}	/* AUDIT_ATTRIBUTES */









/*
 * ROUTINE:	AUDIT_FALLOC
 * PURPOSE:	allocating a new file structure
 * CALLBY:	FALLOC
 * NOTE:	file structure already initialized
 * TODO:
 * QUESTION:
 */

void
audit_falloc(fp)

	struct file *fp;

{	/* AUDIT_FALLOC */

	f_audit_data_t *fad;
	p_audit_data_t *pad;

	dprintf(0x20, ("audit_falloc(%p)\n", (void *)fp));
	call_debug(0x20);

	/* allocate per file audit structure if there a'int any */
	ASSERT(F2A(fp) == (caddr_t)0);

	mutex_enter(&au_flist_lock);
	if ((fad = au_flist) == NULL) {
		FILE_MISS;
		AS_INC(as_memused, sizeof (struct f_audit_data));
		fad = kmem_zalloc(sizeof (struct f_audit_data), KM_SLEEP);
	} else {
		FILE_HIT;
		au_flist = fad->next;
		bzero(fad, sizeof (struct f_audit_data));
	}
	mutex_exit(&au_flist_lock);
	F2A(fp) = (caddr_t)fad;

	pad = (p_audit_data_t *)P2A(curproc);
	fad->fad_thread = curthread; 	/* file audit data back ptr; DEBUG */

	mutex_enter(&pad->pad_lock);
	fad->fad_auid   = pad->pad_auid;
	fad->fad_mask  = pad->pad_mask;
	fad->fad_termid = pad->pad_termid;
	mutex_exit(&pad->pad_lock);

#ifdef NOTDEF
	/*
	 * only needed for 4.1 kernels where the per file data structure is
	 * statically allocated.
	 */
	ASSERT(fad->fad_path == (f_audit_data_t *)0);
#endif
	ADDTRACE("[%x] falloc fp: %p fad: %p tad: %p",
		(void *)fp, (void *)fad, (void *)T2A(curthread), 0, 0, 0);
}

/*
 * ROUTINE:	AUDIT_UNFALLOC
 * PURPOSE:	deallocate file audit data structure
 * CALLBY:	CLOSEF
 *		UNFALLOC
 * NOTE:
 * TODO:
 * QUESTION:
 */

void
audit_unfalloc(struct file *fp)
{
	f_audit_data_t *fad;

	dprintf(0x20, ("audit_unfalloc(%p)\n", (void *)fp));
	call_debug(0x20);

	fad = (f_audit_data_t *)F2A(fp);

	if (!fad) {
#ifdef NOTDEF
		printf("Warning Danger Will Robinson, fad is zero\n");
#endif
		return;
	}
	if (fad->fad_lpbuf) {
		AS_DEC(as_memused, fad->fad_lpbuf);
		kmem_free(fad->fad_path, fad->fad_lpbuf);
	}
	mutex_enter(&au_flist_lock);
	if (au_flist_stat.size > flist_size) {
		/* free per file audit data */
		AS_DEC(as_memused, sizeof (struct f_audit_data));
		kmem_free(fad, sizeof (struct f_audit_data));
		FILE_FREE;
	} else {
		/* put it on the list */
		fad->next = au_flist;
		au_flist = fad;
		au_flist_stat.size++;
	}
	mutex_exit(&au_flist_lock);
	fp->f_audit_data = 0;

	ADDTRACE("[%x] unfalloc fp: %p fad: %p", (void *)fp, (void *)fad,
		0, 0, 0, 0);
}

/*
 * ROUTINE:	AUDIT_EXIT
 * PURPOSE:
 * CALLBY:	EXIT
 * NOTE:
 * TODO:
 * QUESTION:	why cmw code as offset by 2 but not here
 */
/* ARGSUSED */
void
audit_exit(int code, int what)
{
	struct t_audit_data *tad;
	tad = (struct t_audit_data *)U2A(u);

	/*
	 * tad_scid will be set by audit_start even if we are not auditing
	 * the event.
	 */
	if (tad->tad_scid == SYS_exit) {
		/*
		 * if we are auditing the exit system call, then complete
		 * audit record generation (no return from system call).
		 */
		if (tad->tad_flag && tad->tad_event == AUE_EXIT)
			audit_finish(0, SYS_exit, 0, 0);
		return;
	}

	/*
	 * Anyone auditing the system call that was aborted?
	 */
	if (tad->tad_flag) {
		au_uwrite(au_to_text("event aborted"));
		audit_finish(0, tad->tad_scid, 0, 0);
	}

	/*
	 * Generate an audit record for process exit if preselected.
	 */
	(void) audit_start(0, SYS_exit, 0, 0);
	audit_finish(0, SYS_exit, 0, 0);
}

/*
 * ROUTINE:	AUDIT_CORE_START
 * PURPOSE:
 * CALLBY: 	PSIG
 * NOTE:
 * TODO:
 */
void
audit_core_start(int sig)
{
	au_event_t event;
	au_state_t estate;
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);

	ASSERT(tad != (t_audit_data_t *)0);

	ASSERT(tad->tad_scid == 0);
	ASSERT(tad->tad_event == 0);
	ASSERT(tad->tad_evmod == 0);
	ASSERT(tad->tad_ctrl == 0);
	ASSERT(tad->tad_flag == 0);
	ASSERT(tad->tad_pathlen == 0);
	ASSERT(tad->tad_path == NULL);

	dprintf(2, ("audit_core_start: tad: %p\n", (void *)tad));
	call_debug(2)

	/* get basic event for system call */
	event = AUE_CORE;
	estate = audit_ets[event];

	dprintf(2, ("audit_core_start: event: %x ctrl %x estate %x\n",
		event, 0, estate));
	call_debug(2);

	ADDTRACE("[%x] audit_core_start type %x scid %x error %x "
	    "tad %p event %x", 0, -1, 0, (void *)tad, event, 0);

	if ((tad->tad_flag = auditme(tad, estate)) == 0)
		return;

	/* reset the flags for non-user attributable events */
	tad->tad_ctrl   = PAD_CORE;
	tad->tad_scid   = 0;

	if (au_auditstate != AUC_AUDITING) {
		tad->tad_flag = 0;
		tad->tad_ctrl = 0;
		dprintf(2, ("audit_core_start done; audit off\n"));
		call_debug(2);
		return;
	}

	tad->tad_event  = event;
	tad->tad_evmod  = 0;

	dprintf(2, ("audit_core_start processing\n"));
	call_debug(2);

	if (tad->tad_ad != NULL)
		cmn_err(CE_PANIC, "AUDIT_CORE_START: tad_ad != NULL");
	au_write(&(u_ad), au_to_arg32(1, "signal", (uint32_t)sig));
	dprintf(2, ("audit_core_start done; tad %p\n", (void *)tad));
	call_debug(2);
}

/*
 * ROUTINE:	AUDIT_CORE_FINISH
 * PURPOSE:
 * CALLBY:	PSIG
 * NOTE:
 * TODO:
 * QUESTION:
 */

/*ARGSUSED*/
void
audit_core_finish(int code)
{
	int flag;
	t_audit_data_t *tad;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	dprintf(8, ("audit_core_finish(%x)\n", code));

	tad = (t_audit_data_t *)U2A(u);

	ASSERT(tad != (t_audit_data_t *)0);

	if ((flag = tad->tad_flag) == 0) {
		tad->tad_event = 0;
		tad->tad_evmod = 0;
		tad->tad_ctrl  = 0;
		ASSERT(tad->tad_pathlen == 0);
		ASSERT(tad->tad_path == NULL);
		return;
	}
	tad->tad_flag = 0;

	/* kludge for error 0, should use `code==CLD_DUMPED' instead */
	if (flag = audit_success(tad, 0)) {
		/* Add a process token */
		uid  = curproc->p_cred->cr_uid;
		gid  = curproc->p_cred->cr_gid;
		ruid = curproc->p_cred->cr_ruid;
		rgid = curproc->p_cred->cr_rgid;
		pid  = curproc->p_pid;
		pad  = (struct p_audit_data *)P2A(curproc);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		au_write(&(u_ad),
			au_to_subject(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));

		/* Add an optional group token */
		if (audit_policy & AUDIT_GROUP)
			au_write(&(u_ad), au_to_groups(curproc));

		/* Add a return token (should use f argument) */
		add_return_token((caddr_t *)&(u_ad), tad->tad_scid, 0, 0);

		AS_INC(as_generated, 1);
		AS_INC(as_kernel, 1);

		if (audit_sync_block())
			flag = 0;

		/* Add an optional sequence token */
		if ((audit_policy & AUDIT_SEQ) && flag)
			au_write(&(u_ad), au_to_seq());
	}
	/* Close up everything */
	au_close(&(u_ad), flag, tad->tad_event, tad->tad_evmod);

	/* free up any space remaining with the path's */
	if (tad->tad_pathlen) {
		AS_DEC(as_memused, tad->tad_pathlen);
		kmem_free(tad->tad_path, tad->tad_pathlen);
		tad->tad_pathlen = 0;
		tad->tad_path = (caddr_t)0;
		tad->tad_vn = (struct vnode *)0;
	}
	tad->tad_event = 0;
	tad->tad_evmod = 0;
	tad->tad_ctrl  = 0;
}

/*ARGSUSED*/
void
audit_stropen(struct vnode *vp, dev_t *devp, int flag, cred_t *crp)
{
}

/*ARGSUSED*/
void
audit_strclose(struct vnode *vp, int flag, cred_t *crp)
{
}

/*ARGSUSED*/
void
audit_strioctl(struct vnode *vp, int cmd, intptr_t arg, int flag,
    int copyflag, cred_t *crp, int *rvalp)
{
}


/*ARGSUSED*/
void
audit_strgetmsg(struct vnode *vp, struct strbuf *mctl, struct strbuf *mdata,
    unsigned char *pri, int *flag, int fmode)
{
	struct stdata *stp;
	t_audit_data_t *tad = (t_audit_data_t *)U2A(u);

	ASSERT(tad != (t_audit_data_t *)0);

	stp = vp->v_stream;

	/* lock stdata from audit_sock */
	mutex_enter(&stp->sd_lock);

	/* proceed ONLY if user is being audited */
	if (!tad->tad_flag) {
		/*
		 * this is so we will not add audit data onto
		 * a thread that is not being audited.
		 */
		stp->sd_t_audit_data = (caddr_t)0;
		mutex_exit(&stp->sd_lock);
		return;
	}

	stp->sd_t_audit_data = (caddr_t)curthread;
	mutex_exit(&stp->sd_lock);
}

/*ARGSUSED*/
void
audit_strputmsg(struct vnode *vp, struct strbuf *mctl, struct strbuf *mdata,
    unsigned char pri, int flag, int fmode)
{
	struct stdata *stp;
	t_audit_data_t *tad = (t_audit_data_t *)U2A(u);

	ASSERT(tad != (t_audit_data_t *)0);

	stp = vp->v_stream;

	/* lock stdata from audit_sock */
	mutex_enter(&stp->sd_lock);

	/* proceed ONLY if user is being audited */
	if (!tad->tad_flag) {
		/*
		 * this is so we will not add audit data onto
		 * a thread that is not being audited.
		 */
		stp->sd_t_audit_data = (caddr_t)0;
		mutex_exit(&stp->sd_lock);
		return;
	}

	stp->sd_t_audit_data = (caddr_t)curthread;
	mutex_exit(&stp->sd_lock);
}

/*
 * ROUTINE:	AUDIT_CLOSEF
 * PURPOSE:
 * CALLBY:	CLOSEF
 * NOTE:
 * release per file audit resources when file structure is being released.
 *
 * IMPORTANT NOTE: Since we generate an audit record here, we may sleep
 *	on the audit queue if it becomes full. This means
 *	audit_closef can not be called when f_count == 0. Since
 *	f_count == 0 indicates the file structure is free, another
 *	process could attempt to use the file while we were still
 *	asleep waiting on the audit queue. This would cause the
 *	per file audit data to be corrupted when we finally do
 *	wakeup.
 * TODO:
 * QUESTION:
 */

void
audit_closef(fp)

	struct file *fp;

{	/* AUDIT_CLOSEF */


	f_audit_data_t *fad;
	p_audit_data_t *pad;
	t_audit_data_t *tad;
	int success;
	au_state_t estate;
	struct vnode *vp;
	token_t *ad = NULL;
	int flag;
	struct vattr attr;
	short evmod = 0;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	fad = (f_audit_data_t *)F2A(fp);
	estate = audit_ets[AUE_CLOSE];
	tad = (t_audit_data_t *)U2A(u);
	pad = (p_audit_data_t *)P2A(curproc);

	dprintf(0x20, ("audit_closef(%p)\n", (void *)fp));
	dprintf(0x20, ("fad: %p tad: %p pad: %p\n",
		(void *)fad, (void *)tad, (void *)pad));
	call_debug(0x20);
	ADDTRACE("[%x] audit_closef fp: %p fad: %p event %x t: %p tad: %p",
		(void *)fp, (void *)fad, tad->tad_event, (void *)curthread,
		(void *)T2A(curthread), 0);


	/* audit record already generated by system call envelope */
	if (tad->tad_event == AUE_CLOSE) {
		/* so close audit event will have bits set */
		tad->tad_evmod |= (short)fad->fad_flags;
		return;
	}

	mutex_enter(&pad->pad_lock);
	success = pad->pad_mask.as_success & estate;
	mutex_exit(&pad->pad_lock);

	dprintf(0x20, ("success %x\n", success));
	dprintf(0x20, ("pad/tad/fad %p %p %p\n", (void *)pad, (void *)tad,
		(void *)fad));
	call_debug(0x20);

	if ((flag = success) != 0) {
		evmod = (short)fad->fad_flags;
		if (fad->fad_lpbuf) {
			au_write((caddr_t *)&(ad), au_to_path(
				fad->fad_path, fad->fad_pathlen));
		} else {
#ifdef _LP64
			au_write((caddr_t *)&(ad), au_to_arg64(
				1, "no path: fp", (uint64_t)fp));
#else
			au_write((caddr_t *)&(ad), au_to_arg32(
				1, "no path: fp", (uint32_t)fp));
#endif
		}

		/*
		 * can't use audit_attributes here since
		 * we use a private audit area to build the audit
		 * record instead of the one off the thread.
		 */

		if ((vp = fp->f_vnode) != NULL) {
			attr.va_mask = AT_ALL;
			if (VOP_GETATTR(vp, &attr, 0, CRED()) == 0) {
				au_write((caddr_t *)&(ad),
						au_to_attr(&attr));
			}
		}

		/* add a process token */
		uid  = curproc->p_cred->cr_uid;
		gid  = curproc->p_cred->cr_gid;
		ruid = curproc->p_cred->cr_ruid;
		rgid = curproc->p_cred->cr_rgid;
		pid  = curproc->p_pid;
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		au_write((caddr_t *)&(ad),
			au_to_subject(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
		/* add an optional group token */
		if (audit_policy & AUDIT_GROUP)
			au_write((caddr_t *)&(ad), au_to_groups(curproc));

		/* add a return token */
		add_return_token((caddr_t *)&(ad), tad->tad_scid, 0, 0);

		AS_INC(as_generated, 1);
		AS_INC(as_kernel, 1);

		/* handle high water mark */
		if (audit_sync_block())
			flag = 0;

		/* add an optional sequence token */
		if ((audit_policy & AUDIT_SEQ) && flag)
			au_write((caddr_t *)&(ad), au_to_seq());

		/*
		 * Close up everything
		 * Note: path space recovery handled by normal system
		 * call envelope if not at last close.
		 * Note there is no failure at this point since
		 *   this represents closes due to exit of process,
		 *   thus we always indicate successful closes.
		 */
		au_close((caddr_t *)&(ad), flag, AUE_CLOSE, evmod);
	}
}

/*
 * ROUTINE:	AUDIT_SET
 * PURPOSE:	Audit the file path and file attributes.
 * CALLBY:	SETF
 * NOTE:	SETF associate a file pointer with user area's open files.
 * TODO:
 * call audit_finish directly ???
 * QUESTION:
 */

/*ARGSUSED*/
void
audit_setf(file_t *fp, int fd)
{
	f_audit_data_t *fad;
	t_audit_data_t *tad;
#ifdef NOTDEF
	vnode_t *vp;
	struct vattr attr;


#endif
	ADDTRACE("[%x] audit_setf fd: %x fp: %p t: %p tad: %p",
		fd, (void *)fp, (void *)curthread,
		(void *)T2A(curthread), 0, 0);

	if (fp == NULL)
		return;

	tad = (t_audit_data_t *)T2A(curthread);
	fad = (f_audit_data_t *)F2A(fp);

	if (!(tad->tad_scid == SYS_open || tad->tad_scid == SYS_creat ||
		tad->tad_scid == SYS_open64 || tad->tad_scid == SYS_creat64))
		return;

	/* no path */
	if (tad->tad_path == 0)
		return;

#ifdef NOTDEF
	if (fad->fad_lpbuf) {
		cmn_err(CE_PANIC,
		"AUDIT_SETF: path already allocated to file audit data");
	}
#endif

	/* assign path information associated with file audit data */
	fad->fad_lpbuf   = tad->tad_pathlen;
	fad->fad_pathlen = tad->tad_pathlen;
	fad->fad_path    = tad->tad_path;
	tad->tad_pathlen = 0;
	tad->tad_path = NULL;
	tad->tad_vn = (struct vnode *)0;

	if (!(tad->tad_ctrl & PAD_TRUE_CREATE)) {

	/* adjust event type */
		switch (tad->tad_event) {
			case AUE_OPEN_RC:
				tad->tad_event = AUE_OPEN_R;
				break;
			case AUE_OPEN_RTC:
				tad->tad_event = AUE_OPEN_RT;
				break;
			case AUE_OPEN_WC:
				tad->tad_event = AUE_OPEN_W;
				break;
			case AUE_OPEN_WTC:
				tad->tad_event = AUE_OPEN_WT;
				break;
			case AUE_OPEN_RWC:
				tad->tad_event = AUE_OPEN_RW;
				break;
			case AUE_OPEN_RWTC:
				tad->tad_event = AUE_OPEN_RWT;
				break;
			default:
				break;
		}
#ifdef NOTDEF
		if (tad->tad_flag != 0) {
			if (fad->fad_lpbuf)
				au_uwrite(au_to_path(
					fad->fad_path, fad->fad_pathlen));
			else {
#ifdef _LP64
				au_uwrite(au_to_arg64(
					1, "no path: fp", (uint64_t)fp));
#else
				au_uwrite(au_to_arg32(
					1, "no path: fp", (uint32_t)fp));
#endif
			}

			if ((vp = fp->f_vnode) != NULL) {
				attr.va_mask = AT_ALL;
				if (VOP_GETATTR(vp, &attr, 0, CRED()) == 0) {
					au_uwrite(au_to_attr(&attr));
				}
			}
		}
#endif
	}
}


/*
 * ROUTINE:	AUDIT_COPEN
 * PURPOSE:
 * CALLBY:	COPEN
 * NOTE:
 * TODO:
 * QUESTION:
 */
/*ARGSUSED*/
void
audit_copen(int fd, file_t *fp, vnode_t *vp)
{}

void
audit_ipc(int type, int id, void *vp)
{
	/* if not auditing this event, then do nothing */
	if (ad_flag == 0)
		return;

	switch (type) {
	case AT_IPC_MSG:
		au_uwrite(au_to_ipc(AT_IPC_MSG, id));
		au_uwrite(au_to_ipc_perm(&(((struct msqid_ds *)vp)->msg_perm)));
		break;
	case AT_IPC_SEM:
		au_uwrite(au_to_ipc(AT_IPC_SEM, id));
		au_uwrite(au_to_ipc_perm(&(((struct semid_ds *)vp)->sem_perm)));
		break;
	case AT_IPC_SHM:
		au_uwrite(au_to_ipc(AT_IPC_SHM, id));
		au_uwrite(au_to_ipc_perm(&(((struct shmid_ds *)vp)->shm_perm)));
		break;
	}
}

void
audit_ipcget(int type, void *vp)
{
	/* if not auditing this event, then do nothing */
	if (ad_flag == 0)
		return;

	switch (type) {
	case NULL:
		au_uwrite(au_to_ipc_perm((struct ipc_perm *)vp));
		break;
	case AT_IPC_MSG:
		au_uwrite(au_to_ipc_perm(&(((struct msqid_ds *)vp)->msg_perm)));
		break;
	case AT_IPC_SEM:
		au_uwrite(au_to_ipc_perm(&(((struct semid_ds *)vp)->sem_perm)));
		break;
	case AT_IPC_SHM:
		au_uwrite(au_to_ipc_perm(&(((struct shmid_ds *)vp)->shm_perm)));
		break;
	}
}

/*
 * ROUTINE:	AUDIT_REBOOT
 * PURPOSE:
 * CALLBY:
 * NOTE:
 * At this point we know that the system call reboot will not return. We thus
 * have to complete the audit record generation and put it onto the queue.
 * This might be fairly useless if the auditing daemon is already dead....
 * TODO:
 * QUESTION:	who calls audit_reboot
 */

void
audit_reboot()
{
	int flag = 1;
	t_audit_data_t *tad;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	tad = (t_audit_data_t *)U2A(u);

	/* if not auditing this event, then do nothing */
	if (tad->tad_flag == 0)
		return;

	/* do preselection on success/failure */
	if (flag = audit_success(tad, 0)) {
		/* add a process token */
		uid  = curproc->p_cred->cr_uid;
		gid  = curproc->p_cred->cr_gid;
		ruid = curproc->p_cred->cr_ruid;
		rgid = curproc->p_cred->cr_rgid;
		pid  = curproc->p_pid;
		pad  = (struct p_audit_data *)P2A(curproc);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		au_uwrite(au_to_subject(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));

		if (audit_policy & AUDIT_GROUP)
			/* add an optional group token */
			au_uwrite(au_to_groups(curproc));

		/* add a return token */
		add_return_token((caddr_t *)&(u_ad), tad->tad_scid, 0, 0);

		AS_INC(as_generated, 1);
		AS_INC(as_kernel, 1);

		/* add an optional sequence token */
		if (audit_policy & AUDIT_SEQ)
			au_uwrite(au_to_seq());
	}

	/*
	 * Flow control useless here since we're going
	 * to drop everything in the queue anyway. Why
	 * block and wait. There aint anyone left alive to
	 * read the records remaining anyway.
	 */

	/* Close up everything */
	au_close(&(u_ad), flag, tad->tad_event, tad->tad_evmod);
}

/*
 * ROUTINE:	AUDIT_VNCREATE_START
 * PURPOSE:	set flag so path name lookup in create will not add attribute
 * CALLBY:	VN_CREATE
 * NOTE:
 * TODO:
 * QUESTION:
 */

void
audit_vncreate_start()
{
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);
	tad->tad_ctrl |= PAD_NOATTRB;

	ADDTRACE("[%x] audit_vncreate_start tad: %x", tad, 0, 0, 0, 0, 0);
}

/*
 * ROUTINE:	AUDIT_VNCREATE_FINISH
 * PURPOSE:
 * CALLBY:	VN_CREATE
 * NOTE:
 * TODO:
 * QUESTION:
 */
void
audit_vncreate_finish(struct vnode *vp, int error)
{
	t_audit_data_t *tad;

	if (error)
		return;

	tad = (t_audit_data_t *)U2A(u);

	/* if not auditing this event, then do nothing */
	if (tad->tad_flag == 0)
		return;

	if (tad->tad_ctrl & PAD_TRUE_CREATE) {
		audit_attributes(vp);
	}

	if (tad->tad_ctrl & PAD_CORE) {
		audit_attributes(vp);
		tad->tad_ctrl &= ~PAD_CORE;
	}

	if (!error && ((tad->tad_event == AUE_MKNOD) ||
			(tad->tad_event == AUE_MKDIR))) {
		audit_attributes(vp);
	}

	/* for case where multiple lookups in one syscall (rename) */
	tad->tad_ctrl &= ~PAD_NOATTRB;

	ADDTRACE("[%x] audit_vncreate_finish tad: %p vp: %p error: %x",
		tad, vp, error, 0, 0, 0);

}	/* AUDIT_VNCREATE_FINISH */








/*
 * ROUTINE:	AUDIT_EXEC
 * PURPOSE:	Records the function arguments and environment variables
 * CALLBY:	EXEC_ARGS
 * NOTE:
 * TODO:
 * QUESTION:
 */

/*ARGSUSED*/
void
audit_exec(
	const char *argstr,	/* argument strings */
	const char *envstr,	/* environment strings */
	ssize_t argc,		/* total # arguments */
	ssize_t envc)		/* total # environment variables */
{
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);

	/* if not auditing this event, then do nothing */
	if (!tad->tad_flag)
		return;

	/* return if not interested in argv or environment variables */
	if (!(audit_policy & (AUDIT_ARGV|AUDIT_ARGE)))
		return;

	if (audit_policy & AUDIT_ARGV) {
		au_uwrite(au_to_exec_args(argstr, argc));
	}

	if (audit_policy & AUDIT_ARGE) {
		au_uwrite(au_to_exec_env(envstr, envc));
	}
}

/*
 * ROUTINE:	AUDIT_ENTERPROM
 * PURPOSE:
 * CALLBY:	KBDINPUT
 *		ZSA_XSINT
 * NOTE:
 * TODO:
 * QUESTION:	still use spl?
 */
void
audit_enterprom(int flg)
{
	token_t *ad = NULL;
	extern 	int au_wait;
	int 	au_oldwait;
	int 	flag = 1;
	p_audit_data_t *pad;
	au_state_t estate;
	int success, failure;
	extern p_audit_data_t *padata;

	estate = audit_ets[AUE_ENTERPROM];
	pad = padata;
	success = pad->pad_mask.as_success & estate;
	failure = pad->pad_mask.as_failure & estate;

	if (!(success || failure))
		return;

	/* don't wait for audit records */
	/* Some sort of locking is probably needed here */
	au_oldwait = au_wait;
	au_wait = DONTWAIT;

	if (flg)
		au_write((caddr_t *)&(ad), au_to_text("monitor PROM"));
	else
		au_write((caddr_t *)&(ad), au_to_text("kadb"));

	AS_INC(as_generated, 1);
	AS_INC(as_nonattrib, 1);

	if (audit_async_block())
		flag = 0;

	/* Add an optional sequence token */
	if ((audit_policy&AUDIT_SEQ) && flag)
		au_write((caddr_t *)&(ad), au_to_seq());

	au_close((caddr_t *)&(ad), flag, AUE_ENTERPROM, PAD_NONATTR);

	/* Some sort of locking is probably needed here */
	au_wait = au_oldwait;
}


/*
 * ROUTINE:	AUDIT_EXITPROM
 * PURPOSE:
 * CALLBY:	KBDINPUT
 *		ZSA_XSINT
 * NOTE:
 * TODO:
 * QUESTION:	still use spl?
 */

void
audit_exitprom(int flg)
{
	int 	au_oldwait;
	token_t	*ad = NULL;
	int 	success, failure;
	int 	flag = 1;
	extern int au_wait;
	au_state_t estate;
	extern p_audit_data_t *padata;
	p_audit_data_t *pad;

	estate = audit_ets[AUE_EXITPROM];
	pad = padata;
	success = pad->pad_mask.as_success & estate;
	failure = pad->pad_mask.as_failure & estate;
	if (!(success || failure))
		return;

	/* some sort of locking is probably needed here */
	au_oldwait = au_wait;
	au_wait = DONTWAIT;

	if (flg)
		au_write((caddr_t *)&(ad), au_to_text("monitor PROM"));
	else
		au_write((caddr_t *)&(ad), au_to_text("kadb"));

	AS_INC(as_generated, 1);
	AS_INC(as_nonattrib, 1);

	if (audit_async_block())
		flag = 0;

	/* Add an optional sequence token */
	if ((audit_policy & AUDIT_SEQ) && flag)
		au_write((caddr_t *)&(ad), au_to_seq());

	au_close((caddr_t *)&(ad), flag, AUE_EXITPROM, PAD_NONATTR);

	/* some sort of locking is probably needed here */
	au_wait = au_oldwait;
}

/*
 * hook for suser.
 */
/*
 * ROUTINE:	AUDIT_SUSER
 * PURPOSE:
 * CALLBY:	SUSER
 * NOTE:
 * TODO:
 * QUESTION:	what is above comment to get rid of below for?
 */

void
audit_suser(int flg)
{
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)U2A(u);
	if (flg)
		tad->tad_ctrl |= PAD_SUSEROK;
	else
		tad->tad_ctrl |= PAD_SUSERNO;
}

struct fcntla {
	int fdes;
	int cmd;
	intptr_t arg;
};

/*
 * ROUTINE:	AUDIT_C2_REVOKE
 * PURPOSE:
 * CALLBY:	FCNTL
 * NOTE:
 * TODO:
 * QUESTION:	are we keeping this func
 */

/*ARGSUSED*/
int
audit_c2_revoke(struct fcntla *uap, rval_t *rvp)
{
	return (0);
}


/*
 * ROUTINE:	AUDIT_CHDIREC
 * PURPOSE:
 * CALLBY:	CHDIREC
 * NOTE:	The main function of CHDIREC
 * TODO:	Move the audit_chdirec hook above the VN_RELE in vncalls.c
 * QUESTION:
 */

/*ARGSUSED*/
void
audit_chdirec(vnode_t *vp, vnode_t **vpp)
{
	int		chdir;
	int		fchdir;
	uint_t		e;
	char 		*sp;
	struct cwrd	*cwd;
	struct file	*fp;
	f_audit_data_t *fad;
	p_audit_data_t *pad = (p_audit_data_t *)P2A(curproc);
	t_audit_data_t *tad = (t_audit_data_t *)T2A(curthread);

	struct a {
		long fd;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;


	if ((tad->tad_scid == SYS_chdir) || (tad->tad_scid == SYS_chroot)) {
		chdir = tad->tad_scid == SYS_chdir;
		if (tad->tad_pathlen) {
			e = audit_fixpath(tad->tad_path, tad->tad_pathlen);
			mutex_enter(&pad->pad_lock);
			mutex_enter(&cwrd_lock);
			cwd = cwdup(pad->pad_cwrd, (chdir) ? ROOT : DIR);
			bsm_cwfree(pad->pad_cwrd);
			mutex_exit(&cwrd_lock);

			if (chdir) {
				pad->pad_cwrd = cwd;
				pad->pad_cwrd->cwrd_ldbuf  = tad->tad_pathlen;
				pad->pad_cwrd->cwrd_dirlen = e;
				pad->pad_cwrd->cwrd_dir    = tad->tad_path;
			} else {	/* SYS_chroot */
				pad->pad_cwrd = cwd;
				pad->pad_cwrd->cwrd_lrbuf = tad->tad_pathlen;
				pad->pad_cwrd->cwrd_rootlen = e;
				pad->pad_cwrd->cwrd_root = tad->tad_path;
			}
			mutex_exit(&pad->pad_lock);

			tad->tad_pathlen = 0;
			tad->tad_path = (caddr_t)0;
			tad->tad_vn = (struct vnode *)0;
		}
	} else if ((tad->tad_scid == SYS_fchdir) ||
		(tad->tad_scid == SYS_fchroot)) {
		fchdir = tad->tad_scid == SYS_fchdir;
		if ((fp = (struct file *)getf(uap->fd)) == NULL)
			return;
		fad = (f_audit_data_t *)F2A(fp);
		if (fad->fad_pathlen) {
			if (fad->fad_lpbuf == fad->fad_pathlen)
				e = audit_fixpath(
					fad->fad_path, fad->fad_pathlen);
			else
				e = fad->fad_pathlen;

			AS_INC(as_memused, e);
			sp = kmem_alloc(e, KM_SLEEP);
			bcopy(fad->fad_path, sp, e);

			mutex_enter(&pad->pad_lock);
			mutex_enter(&cwrd_lock);
			cwd = cwdup(pad->pad_cwrd, (fchdir) ? ROOT : DIR);
			bsm_cwfree(pad->pad_cwrd);
			mutex_exit(&cwrd_lock);

			pad->pad_cwrd = cwd;
			if (fchdir) {
				pad->pad_cwrd->cwrd_ldbuf  = e;
				pad->pad_cwrd->cwrd_dirlen = e;
				pad->pad_cwrd->cwrd_dir    = sp;
			} else {	/* SYS_fchroot */
				pad->pad_cwrd->cwrd_lrbuf   = e;
				pad->pad_cwrd->cwrd_rootlen = e;
				pad->pad_cwrd->cwrd_root    = sp;
			}
			mutex_exit(&pad->pad_lock);

			/* indicate any path compression */
			fad->fad_pathlen = e;
			if (tad->tad_flag) {
				struct vnode *vp;
				au_uwrite(au_to_path(
					fad->fad_path, fad->fad_pathlen));
				vp = fp->f_vnode;
				audit_attributes(vp);
			}
		}
		releasef(uap->fd);
	}
}

/*
 * ROUTINE:	AUDIT_GETF
 * PURPOSE:
 * CALLBY:	GETF_INTERNAL
 * NOTE:	The main function of GETF_INTERNAL is to associate a given
 *		file descriptor with a file structure and increment the
 *		file pointer reference count.
 * TODO:	remove pass in of fpp.
increment a reference count so that even if a thread with same process delete
the same object, it will not panic our system
 * QUESTION:
where to decrement the f_count?????????????????
seems like I need to set a flag if f_count incrmented through audit_getf
 */

/*ARGSUSED*/
int
audit_getf(int fd)
{
#ifdef NOTYET
	t_audit_data_t *tad;

	tad = (t_audit_data_t *)T2A(curthread);

	if (!(tad->tad_scid == SYS_open || tad->tad_scid == SYS_creat))
		return;
#endif
	return (0);
}

/*
 *	Audit hook for stream based socket and tli request.
 *	Note that we do not have user context while executing
 *	this code so we had to record them earlier during the
 *	putmsg/getmsg to figure out which user we are dealing with.
 */

/*ARGSUSED*/
void
audit_sock(
	int type,	/* type of tihdr.h header requests */
	queue_t *q,	/* contains the process and thread audit data */
	mblk_t *mp,	/* contains the tihdr.h header structures */
	int from)	/* timod or sockmod request */
{
	int32_t    len;
	int32_t    offset;
	struct sockaddr_in *sock_data;
	struct T_conn_req *conn_req;
	struct T_conn_ind *conn_ind;
	struct T_unitdata_req *unitdata_req;
	struct T_unitdata_ind *unitdata_ind;
	au_state_t estate;
	p_audit_data_t *pad;
	t_audit_data_t *tad;
	caddr_t saved_thread_ptr;

	if (q->q_stream == NULL)
		return;
	mutex_enter(&q->q_stream->sd_lock);
	/* are we being audited */
	saved_thread_ptr = q->q_stream->sd_t_audit_data;
	/* no pointer to thread, nothing to do */
	if (saved_thread_ptr == NULL) {
		mutex_exit(&q->q_stream->sd_lock);
		return;
	}
	/* only allow one addition of a record token */
	q->q_stream->sd_t_audit_data = (caddr_t)0;
	/*
	 * thread is not the one being audited, then nothing to do
	 * This could be the stream thread handling the module
	 * service routine. In this case, the context for the audit
	 * record can no longer be assumed. Simplest to just drop
	 * the operation.
	 */
	if (curthread != (kthread_id_t)saved_thread_ptr) {
		mutex_exit(&q->q_stream->sd_lock);
		return;
	}
	if (curthread->t_sysnum >= SYS_so_socket &&
	    curthread->t_sysnum <= SYS_sockconfig) {
		mutex_exit(&q->q_stream->sd_lock);
		return;
	}
	mutex_exit(&q->q_stream->sd_lock);
	/*
	 * we know that the thread that did the put/getmsg is the
	 * one running. Now we can get the TAD and see if we should
	 * add an audit token.
	 */
	tad = (t_audit_data_t *)U2A(u);

	/* proceed ONLY if user is being audited */
	if (!tad->tad_flag)
		return;
	pad = (p_audit_data_t *)P2A(tad->tad_thread->t_procp);
	if (pad == NULL)
		return;
	/*
	 * Figure out the type of stream networking request here.
	 * Note that getmsg and putmsg are always preselected
	 * because during the beginning of the system call we have
	 * not yet figure out which of the socket or tli request
	 * we are looking at until we are here. So we need to check
	 * against that specific request and reset the type of event.
	 */
	switch (type) {
	case T_CONN_REQ:	/* connection request */
		conn_req = (struct T_conn_req *)mp->b_rptr;
		if (conn_req->DEST_offset < sizeof (struct T_conn_req))
			return;
		offset = conn_req->DEST_offset;
		len = conn_req->DEST_length;
		estate = audit_ets[AUE_SOCKCONNECT];
		if (pad->pad_mask.as_success & estate ||
			pad->pad_mask.as_failure & estate) {
			tad->tad_event = AUE_SOCKCONNECT;
			break;
		} else {
			return;
		}
	case T_CONN_IND:	 /* connectionless receive request */
		conn_ind = (struct T_conn_ind *)mp->b_rptr;
		if (conn_ind->SRC_offset < sizeof (struct T_conn_ind))
			return;
		offset = conn_ind->SRC_offset;
		len = conn_ind->SRC_length;
		estate = audit_ets[AUE_SOCKACCEPT];
		if (pad->pad_mask.as_success & estate ||
			pad->pad_mask.as_failure & estate) {
			tad->tad_event = AUE_SOCKACCEPT;
			break;
		} else {
			return;
		}
	case T_UNITDATA_REQ:	 /* connectionless send request */
		unitdata_req = (struct T_unitdata_req *)mp->b_rptr;
		if (unitdata_req->DEST_offset < sizeof (struct T_unitdata_req))
			return;
		offset = unitdata_req->DEST_offset;
		len = unitdata_req->DEST_length;
		estate = audit_ets[AUE_SOCKSEND];
		if (pad->pad_mask.as_success & estate ||
			pad->pad_mask.as_failure & estate) {
			tad->tad_event = AUE_SOCKSEND;
			break;
		} else {
			return;
		}
	case T_UNITDATA_IND:	 /* connectionless receive request */
		unitdata_ind = (struct T_unitdata_ind *)mp->b_rptr;
		if (unitdata_ind->SRC_offset < sizeof (struct T_unitdata_ind))
			return;
		offset = unitdata_ind->SRC_offset;
		len = unitdata_ind->SRC_length;
		estate = audit_ets[AUE_SOCKRECEIVE];
		if (pad->pad_mask.as_success & estate ||
			pad->pad_mask.as_failure & estate) {
			tad->tad_event = AUE_SOCKRECEIVE;
			break;
		} else {
			return;
		}
	default:
		return;
	}

	/*
	 * we are only interested in tcp stream connections,
	 * not unix domain stuff
	 */
	if ((len < 0) || (len > sizeof (struct sockaddr_in))) {
		tad->tad_event = AUE_GETMSG;
		return;
	}
	/* skip over TPI header and point to the ip address */
	sock_data = (struct sockaddr_in *)((char *)mp->b_rptr + offset);

	switch (sock_data->sin_family) {
	case AF_INET:
		au_write(&(tad->tad_ad), au_to_sock_inet(sock_data));
		break;
	default:	/* reset to AUE_PUTMSG if not a inet request */
		tad->tad_event = AUE_GETMSG;
		break;
	}
}

void
audit_lookupname()
{
}

/*ARGSUSED*/
int
audit_pathcomp(struct pathname *pnp, vnode_t *cvp, cred_t *cr)
{
	return (0);
}

void
add_return_token(caddr_t *ad, unsigned int scid, int err, int rval)
{
	unsigned int sy_flags;

#ifdef _SYSCALL32_IMPL
	if (lwp_getdatamodel(
		ttolwp(curthread)) == DATAMODEL_NATIVE)
		sy_flags = sysent[scid].sy_flags & SE_RVAL_MASK;
	else
		sy_flags = sysent32[scid].sy_flags & SE_RVAL_MASK;
#else
		sy_flags = sysent[scid].sy_flags & SE_RVAL_MASK;
#endif

	if (sy_flags == SE_32RVAL1)
		au_write(ad, au_to_return32(err, (int32_t)rval));

	if (sy_flags == (SE_32RVAL2|SE_32RVAL1))
		au_write(ad, au_to_return32(err, (int32_t)rval));

	if (sy_flags == SE_64RVAL)
		au_write(ad, au_to_return64(err, (int64_t)rval));
}

/*ARGSUSED*/
void
audit_fdsend(fd, fp, error)
	int fd;
	struct file *fp;
	int error;		/* ignore for now */
{
	t_audit_data_t *tad;	/* current thread */
	f_audit_data_t *fad;	/* per file audit structure */
	struct vnode *vp;	/* for file attributes */

	/* is this system call being audited */
	tad = (t_audit_data_t *)U2A(u);
	ASSERT(tad != (t_audit_data_t *)0);
	if (!tad->tad_flag)
		return;

	fad = (f_audit_data_t *)F2A(fp);

	/* add path and file attributes */
	if (fad != NULL && fad->fad_lpbuf) {
		au_uwrite(au_to_arg32(0, "send fd", (uint32_t)fd));
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(0, "send fd", (uint32_t)fd));
#ifdef _LP64
		au_uwrite(au_to_arg64(0, "no path", (uint64_t)fp));
#else
		au_uwrite(au_to_arg32(0, "no path", (uint32_t)fp));
#endif
	}
	vp = fp->f_vnode;	/* include vnode attributes */
	audit_attributes(vp);
}

/*ARGSUSED*/
void
audit_fdrecv(fd, fp)
	int fd;
	struct file *fp;
{
	t_audit_data_t *tad;	/* current thread */
	f_audit_data_t *fad;	/* per file audit structure */
	struct vnode *vp;	/* for file attributes */

	/* is this system call being audited */
	tad = (t_audit_data_t *)U2A(u);
	ASSERT(tad != (t_audit_data_t *)0);
	if (!tad->tad_flag)
		return;

	fad = (f_audit_data_t *)F2A(fp);

	/* add path and file attributes */
	if (fad != NULL && fad->fad_lpbuf) {
		au_uwrite(au_to_arg32(0, "recv fd", (uint32_t)fd));
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(0, "recv fd", (uint32_t)fd));
#ifdef _LP64
		au_uwrite(au_to_arg64(0, "no path", (uint64_t)fp));
#else
		au_uwrite(au_to_arg32(0, "no path", (uint32_t)fp));
#endif
	}
	vp = fp->f_vnode;	/* include vnode attributes */
	audit_attributes(vp);
}
