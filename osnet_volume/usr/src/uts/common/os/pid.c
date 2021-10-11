/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pid.c	1.41	99/11/18 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/prsystm.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <c2/audit.h>
#include <sys/rce.h>

/* directory entries for /proc */
union procent {
	proc_t *pe_proc;
	union procent *pe_next;
};

struct pid pid0 = {
	0,		/* pid_prinactive */
	1,		/* pid_pgorphaned */
	0,		/* pid_padding	*/
	0,		/* pid_prslot	*/
	0,		/* pid_id	*/
	NULL,		/* pid_pglink	*/
	NULL,		/* pid_link	*/
	3		/* pid_ref	*/
};

static int pid_hashlen = 4;	/* desired average hash chain length */
static int pid_hashsz;		/* number of buckets in the hash table */

#define	HASHPID(pid)	(pidhash[((pid)&(pid_hashsz-1))])

extern uint_t nproc;
extern struct kmem_cache *process_cache;
static void	upcount_init(void);

kmutex_t	pidlock;	/* global process lock */
kmutex_t	pr_pidlock;	/* /proc global process lock */
kcondvar_t	*pr_pid_cv;	/* for /proc, one per process slot */
struct plock	*proc_lock;	/* persistent array of p_lock's */

static kmutex_t	pidlinklock;
static struct pid **pidhash;
static pid_t minpid;
static pid_t mpid;
static union procent *procdir;
static union procent *procentfree;

static struct pid *
pid_lookup(pid_t pid)
{
	struct pid *pidp;

	ASSERT(MUTEX_HELD(&pidlinklock));

	for (pidp = HASHPID(pid); pidp; pidp = pidp->pid_link) {
		if (pidp->pid_id == pid) {
			ASSERT(pidp->pid_ref > 0);
			break;
		}
	}
	return (pidp);
}

void
pid_setmin(void)
{
	if (jump_pid && jump_pid > mpid)
		minpid = mpid = jump_pid;
	else
		minpid = mpid + 1;
}

/*
 * This function assigns a pid for use in a fork request.  It allocates
 * a pid structure, tries to find an empty slot in the proc table,
 * and selects the process id.
 *
 * pid_assign() returns the new pid on success, -1 on failure.
 */
pid_t
pid_assign(proc_t *prp)
{
	struct pid *pidp;
	union procent *pep;
	pid_t newpid, startpid;

	pidp = kmem_zalloc(sizeof (struct pid), KM_SLEEP);

	mutex_enter(&pidlinklock);
	if ((pep = procentfree) == NULL) {
		/*
		 * ran out of /proc directory entries
		 */
		goto failed;
	}

	/*
	 * Allocate a pid
	 */
	startpid = mpid;
	do  {
		newpid = (++mpid == maxpid ? mpid = minpid : mpid);
	} while (pid_lookup(newpid) && newpid != startpid);

	if (newpid == startpid && pid_lookup(newpid)) {
		/* couldn't find a free pid */
		goto failed;
	}

	procentfree = pep->pe_next;
	pep->pe_proc = prp;
	prp->p_pidp = pidp;

	/*
	 * Put pid into the pid hash table.
	 */
	pidp->pid_link = HASHPID(newpid);
	HASHPID(newpid) = pidp;
	pidp->pid_ref = 1;
	pidp->pid_id = newpid;
	pidp->pid_prslot = pep - procdir;
	prp->p_lockp = &proc_lock[pidp->pid_prslot];
	mutex_exit(&pidlinklock);

	return (newpid);

failed:
	mutex_exit(&pidlinklock);
	kmem_free(pidp, sizeof (struct pid));
	return (-1);
}

/*
 * decrement the reference count for pid
 */
int
pid_rele(struct pid *pidp)
{
	struct pid **pidpp;

	mutex_enter(&pidlinklock);
	ASSERT(pidp != &pid0);

	pidpp = &HASHPID(pidp->pid_id);
	for (;;) {
		ASSERT(*pidpp != NULL);
		if (*pidpp == pidp)
			break;
		pidpp = &(*pidpp)->pid_link;
	}

	*pidpp = pidp->pid_link;
	mutex_exit(&pidlinklock);

	kmem_free(pidp, sizeof (*pidp));
	return (0);
}

void
proc_entry_free(struct pid *pidp)
{
	mutex_enter(&pidlinklock);
	pidp->pid_prinactive = 1;
	procdir[pidp->pid_prslot].pe_next = procentfree;
	procentfree = &procdir[pidp->pid_prslot];
	mutex_exit(&pidlinklock);
}

void
pid_exit(proc_t *prp)
{
	struct pid *pidp;

	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * Exit process group.  If it is NULL, it's because fork failed
	 * before calling pgjoin().
	 */
	ASSERT(prp->p_pgidp != NULL || prp->p_stat == SIDL);
	if (prp->p_pgidp != NULL)
		pgexit(prp);

	/*
	 * SRM hook: allows to release resources that SRM allocated when a
	 * process was forked; to do final accounting for the user if necessary.
	 * SRM_PROCDESTROY() is called at the freeing of every pid for which a
	 * successful SRM_PROCCREATE() call was made.
	 * SRM_PROCCREATE() has been called for every process since boot except
	 * process 0.
	 * SRM_EXIT() has already been called for this process.
	 * The process slot number is still allocated to the process and won't
	 * be reusable until at least after SRM_PROCDESTROY returns.
	 * The process is a zombie: it has no threads (however they may not
	 * have all left their respective scheduling classes if lingering on
	 * a deathrow); curthread and curproc refer to some other thread and
	 * process.
	 */
	SRM_PROCDESTROY(prp);

	SESS_RELE(prp->p_sessp);

	pidp = prp->p_pidp;

	proc_entry_free(pidp);

#ifdef C2_AUDIT
	if (audit_active)
		audit_pfree(prp);
#endif

	if (practive == prp) {
		practive = prp->p_next;
	}

	if (prp->p_next) {
		prp->p_next->p_prev = prp->p_prev;
	}
	if (prp->p_prev) {
		prp->p_prev->p_next = prp->p_next;
	}

	PID_RELE(pidp);

	mutex_destroy(&prp->p_crlock);
	kmem_cache_free(process_cache, prp);
	nproc--;
}

/*
 * find a process given its process ID
 */
proc_t *
prfind(pid_t pid)
{
	struct pid *pidp;

	ASSERT(MUTEX_HELD(&pidlock));

	mutex_enter(&pidlinklock);
	pidp = pid_lookup(pid);
	mutex_exit(&pidlinklock);
	if (pidp != NULL && pidp->pid_prinactive == 0)
		return (procdir[pidp->pid_prslot].pe_proc);
	return (NULL);
}

/*
 * return the list of processes in whose process group ID is 'pgid',
 * or NULL, if no such process group
 */
proc_t *
pgfind(pid_t pgid)
{
	struct pid *pidp;

	ASSERT(MUTEX_HELD(&pidlock));

	mutex_enter(&pidlinklock);
	pidp = pid_lookup(pgid);
	mutex_exit(&pidlinklock);
	if (pidp != NULL)
		return (pidp->pid_pglink);
	return (NULL);
}

/*
 * If pid exists, find its proc, acquire its p_lock and mark it SPRLOCK.
 * Returns the proc pointer on success, NULL on failure.  sprlock() is
 * really just a stripped-down version of pr_p_lock() to allow practive
 * walkers like dofusers() and dumpsys() to synchronize with /proc.
 */
proc_t *
sprlock(pid_t pid)
{
	proc_t *p;
	kmutex_t *mp;

	for (;;) {
		mutex_enter(&pidlock);
		if ((p = prfind(pid)) == NULL) {
			mutex_exit(&pidlock);
			return (NULL);
		}
		/*
		 * p_lock is persistent, but p itself is not -- it could
		 * vanish during cv_wait().  Load p->p_lock now so we can
		 * drop it after cv_wait() without referencing p.
		 */
		mp = &p->p_lock;
		mutex_enter(mp);
		mutex_exit(&pidlock);
		/*
		 * If the process is in some half-baked state, fail.
		 */
		if (p->p_stat == SZOMB || p->p_stat == SIDL ||
		    p->p_lwpcnt == 0 || (p->p_flag & EXITLWPS)) {
			mutex_exit(mp);
			return (NULL);
		}
		if (panicstr)
			return (p);
		if (!(p->p_flag & SPRLOCK))
			break;
		cv_wait(&pr_pid_cv[p->p_slot], mp);
		mutex_exit(mp);
	}
	p->p_flag |= SPRLOCK;
	THREAD_KPRI_REQUEST();
	return (p);
}

void
sprunlock(proc_t *p)
{
	if (panicstr) {
		mutex_exit(&p->p_lock);
		return;
	}

	ASSERT(p->p_flag & SPRLOCK);
	ASSERT(MUTEX_HELD(&p->p_lock));

	cv_signal(&pr_pid_cv[p->p_slot]);
	p->p_flag &= ~SPRLOCK;
	mutex_exit(&p->p_lock);
	THREAD_KPRI_RELEASE();
}

void
pid_init(void)
{
	int i;

	pid_hashsz = 1 << highbit(v.v_proc / pid_hashlen);

	pidhash = kmem_zalloc(sizeof (struct pid *) * pid_hashsz, KM_SLEEP);
	procdir = kmem_alloc(sizeof (union procent) * v.v_proc, KM_SLEEP);
	pr_pid_cv = kmem_zalloc(sizeof (kcondvar_t) * v.v_proc, KM_SLEEP);
	proc_lock = kmem_zalloc(sizeof (struct plock) * v.v_proc, KM_SLEEP);

	nproc = 1;
	practive = proc_sched;
	proc_sched->p_next = NULL;
	procdir[0].pe_proc = proc_sched;

	procentfree = &procdir[1];
	for (i = 1; i < v.v_proc - 1; i++)
		procdir[i].pe_next = &procdir[i+1];
	procdir[i].pe_next = NULL;

	HASHPID(0) = &pid0;

	upcount_init();
}

proc_t *
pid_entry(int slot)
{
	union procent *pep;
	proc_t *prp;

	ASSERT(MUTEX_HELD(&pidlock));
	ASSERT(slot >= 0 && slot < v.v_proc);

	pep = procdir[slot].pe_next;
	if (pep >= procdir && pep < &procdir[v.v_proc])
		return (NULL);
	prp = procdir[slot].pe_proc;
	if (prp != 0 && prp->p_stat == SIDL)
		return (NULL);
	return (prp);
}

/*
 * Send the specified signal to all processes whose process group ID is
 * equal to 'pgid'
 */

void
signal(pid_t pgid, int sig)
{
	struct pid *pidp;
	proc_t *prp;

	mutex_enter(&pidlock);
	mutex_enter(&pidlinklock);
	if (pgid == 0 || (pidp = pid_lookup(pgid)) == NULL) {
		mutex_exit(&pidlinklock);
		mutex_exit(&pidlock);
		return;
	}
	mutex_exit(&pidlinklock);
	for (prp = pidp->pid_pglink; prp; prp = prp->p_pglink) {
		mutex_enter(&prp->p_lock);
		sigtoproc(prp, NULL, sig);
		mutex_exit(&prp->p_lock);
	}
	mutex_exit(&pidlock);
}

/*
 * Send the specified signal to the specified process
 */

void
prsignal(struct pid *pidp, int sig)
{
	if (!(pidp->pid_prinactive))
		psignal(procdir[pidp->pid_prslot].pe_proc, sig);
}

#include <sys/sunddi.h>

/*
 * DDI/DKI interfaces for drivers to send signals to processes
 */

/*
 * obtain an opaque reference to a process for signaling
 */
void *
proc_ref(void)
{
	struct pid *pidp;

	mutex_enter(&pidlock);
	pidp = curproc->p_pidp;
	PID_HOLD(pidp);
	mutex_exit(&pidlock);

	return (pidp);
}

/*
 * release a reference to a process
 * - a process can exit even if a driver has a reference to it
 * - one proc_unref for every proc_ref
 */
void
proc_unref(void *pref)
{
	mutex_enter(&pidlock);
	PID_RELE((struct pid *)pref);
	mutex_exit(&pidlock);
}

/*
 * send a signal to a process
 *
 * - send the process the signal
 * - if the process went away, return a -1
 * - if the process is still there return 0
 */
int
proc_signal(void *pref, int sig)
{
	struct pid *pidp = pref;

	prsignal(pidp, sig);
	return (pidp->pid_prinactive ? -1 : 0);
}


static struct upcount	**upc_hash;	/* a boot time allocated array */
static ulong_t		upc_hashmask;
#define	UPC_HASH(x)	((ulong_t)(x) & upc_hashmask)

/*
 * Get us off the ground.  Called once at boot.
 */
void
upcount_init(void)
{
	ulong_t	upc_hashsize;

	/*
	 * An entry per MB of memory is our current guess
	 */
	/*
	 * 2^20 is a meg, so shifting right by 20 - PAGESHIFT
	 * converts pages to megs (without overflowing a u_int
	 * if you have more than 4G of memory, like ptob(physmem)/1M
	 * would).
	 */
	upc_hashsize = (1 << highbit(physmem >> (20 - PAGESHIFT)));
	upc_hashmask = upc_hashsize - 1;
	upc_hash = kmem_zalloc(upc_hashsize * sizeof (struct upcount *),
	    KM_SLEEP);
}

/*
 * Increment the number of processes associated with a given uid.
 */
void
upcount_inc(uid_t uid)
{
	struct upcount	**upc, **hupc;
	struct upcount	*new;

	ASSERT(MUTEX_HELD(&pidlock));
	new = NULL;
	hupc = &upc_hash[UPC_HASH(uid)];
top:
	upc = hupc;
	while ((*upc) != NULL) {
		if ((*upc)->up_uid == uid) {
			(*upc)->up_count++;
			if (new) {
				/*
				 * did not need `new' afterall.
				 */
				kmem_free(new, sizeof (*new));
			}
			return;
		}
		upc = &(*upc)->up_next;
	}

	/*
	 * There is no entry for this uid.
	 * Allocate one.  If we have to drop pidlock, check
	 * again.
	 */
	if (new == NULL) {
		new = (struct upcount *)kmem_alloc(sizeof (*new), KM_NOSLEEP);
		if (new == NULL) {
			mutex_exit(&pidlock);
			new = (struct upcount *)kmem_alloc(sizeof (*new),
			    KM_SLEEP);
			mutex_enter(&pidlock);
			goto top;
		}
	}


	/*
	 * On the assumption that a new user is going to do some
	 * more forks, put the new upcount structure on the front.
	 */
	upc = hupc;

	new->up_uid = uid;
	new->up_count = 1;
	new->up_next = *upc;

	*upc = new;
}

/*
 * Decrement the number of processes a given uid has.
 */
void
upcount_dec(uid_t uid)
{
	struct	upcount **upc;
	struct	upcount *done;

	ASSERT(MUTEX_HELD(&pidlock));

	upc = &upc_hash[UPC_HASH(uid)];
	while ((*upc) != NULL) {
		if ((*upc)->up_uid == uid) {
			(*upc)->up_count--;
			if ((*upc)->up_count == 0) {
				done = *upc;
				*upc = (*upc)->up_next;
				kmem_free(done, sizeof (*done));
			}
			return;
		}
		upc = &(*upc)->up_next;
	}
	cmn_err(CE_PANIC, "decr_upcount-off the end");
}

/*
 * Returns the number of processes a uid has.
 * Non-existent uid's are assumed to have no processes.
 */
int
upcount_get(uid_t uid)
{
	struct	upcount *upc;

	ASSERT(MUTEX_HELD(&pidlock));

	upc = upc_hash[UPC_HASH(uid)];
	while (upc != NULL) {
		if (upc->up_uid == uid) {
			return (upc->up_count);
		}
		upc = upc->up_next;
	}
	return (0);
}
