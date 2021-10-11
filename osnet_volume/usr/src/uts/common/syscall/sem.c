/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sem.c	1.58	99/10/04 SMI"

/*
 * Inter-Process Communication Semaphore Facility.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vmem.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/var.h>
#include <sys/cmn_err.h>

#include <c2/audit.h>

/*
 * Protects all sem data structures.
 */
static kmutex_t sem_lock;
static kmutex_t	*sema_locks;

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
 * The following variables seminfo_* are there so that the
 * elements of the data structure seminfo can be tuned
 * (if necessary) using the /etc/system file syntax for
 * tuning of integer data types.
 */
int seminfo_semmap = 10;	/* (obsolete) */
int seminfo_semmni = 10;	/* # of semaphore identifiers */
int seminfo_semmns = 60;	/* # of semaphores in system */
int seminfo_semmnu = 30;	/* # of undo structures in system */
int seminfo_semmsl = 25;	/* max # of semaphores per id */
int seminfo_semopm = 10;	/* max # of operations per semop call */
int seminfo_semume = 10;	/* max # of undo entries per process */
int seminfo_semusz = 96;	/* size in bytes of undo structure */
int seminfo_semvmx = 32767;	/* semaphore maximum value */
int seminfo_semaem = 16384;	/* adjust on exit max value */

static vmem_t *sem_arena;

/*
 * Each semid is made up of:
 *	16 bit index into sema array
 *	16 bit sequence number
 */
#define	SEMA_INDEX_MAX	0xffff
#define	SEMA_INDEX_MASK	0xffff
#define	SEMA_SEQ_MAX	0x7fff
#define	SEMA_SEQ_MASK	0x7fff
#define	SEMA_SEQ_SHIFT	16

/* define macro to round up to integer size  from machdep.c */
#define	INTSZ(X)	howmany((X), sizeof (int))

/*
 * Private functions
 */
static int semconv(int);
static kmutex_t *semconv_lock(int, struct semid_ds **);

static int semsys(int opcode, uintptr_t a0, uintptr_t a1,
	    uintptr_t a2, uintptr_t a3);

#include <sys/modctl.h>
#include <sys/syscall.h>

static struct sysent ipcsem_sysent = {
	5,
	SE_NOUNLOAD | SE_ARGC | SE_32RVAL1,
	semsys
};

/*
 * Module linkage information for the kernel.
 */
static struct modlsys modlsys = {
	&mod_syscallops, "System V semaphore facility", &ipcsem_sysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32, "32-bit System V semaphore facility", &ipcsem_sysent
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
	int i;
	uint64_t mavail;

	/*
	 * seminfo_* are inited to default values above.
	 * These values can be tuned if need be using the
	 * integer tuning capabilities in the /etc/system file.
	 */
	if ((seminfo.semmni = seminfo_semmni) > SEMA_INDEX_MAX) {
		seminfo.semmni = SEMA_INDEX_MAX;
		cmn_err(CE_WARN, "seminit: seminfo_semmni too large. Using %d",
			SEMA_INDEX_MAX);
	}

	seminfo.semmns = seminfo_semmns;
	seminfo.semmnu = seminfo_semmnu;
	seminfo.semmsl = seminfo_semmsl;
	seminfo.semopm = seminfo_semopm;
	seminfo.semume = seminfo_semume;
	seminfo.semvmx = seminfo_semvmx;
	seminfo.semaem = seminfo_semaem;
	seminfo.semusz = sizeof (struct sem_undo) +
				seminfo.semume * sizeof (struct undo);

	/*
	 * Don't use more than 25% of the available kernel memory
	 */
	mavail = (uint64_t)kmem_maxavail() / 4;
	if (((uint64_t)seminfo.semmns * sizeof (struct sem) +
	    (uint64_t)seminfo.semmni * sizeof (struct semid_ds) +
	    (uint64_t)seminfo.semmni * sizeof (kmutex_t) +
	    (uint64_t)v.v_proc * sizeof (struct sem_undo *) +
	    INTSZ((uint64_t)seminfo.semusz * seminfo.semmnu) * sizeof (int))
		> mavail) {
		cmn_err(CE_WARN,
		    "semsys: can't load module, too much memory requested");
		return (ENOMEM);
	}

	ASSERT(sem_arena == NULL);
	sem_arena = vmem_create("semaphore", (void *)1, seminfo.semmns,
	    1, NULL, NULL, NULL, 0, VM_SLEEP);

	mutex_init(&sem_lock, NULL, MUTEX_DEFAULT, NULL);

	ASSERT(sem == NULL);
	ASSERT(sema == NULL);
	ASSERT(sem_undo == NULL);
	ASSERT(semu == NULL);
	ASSERT(sema_locks == NULL);

	sem = kmem_zalloc(seminfo.semmns * sizeof (struct sem), KM_SLEEP);
	sema = kmem_zalloc(seminfo.semmni * sizeof (struct semid_ds), KM_SLEEP);
	sem_undo = kmem_zalloc(v.v_proc * sizeof (struct sem_undo *), KM_SLEEP);
	semu = kmem_zalloc((INTSZ(seminfo.semusz * seminfo.semmnu) *
		sizeof (int)), KM_SLEEP);
	sema_locks = kmem_zalloc(seminfo.semmni * sizeof (kmutex_t), KM_SLEEP);

	/*
	 * link all free undo structures together
	 */
	ASSERT(semfup == NULL);
	semfup = (struct sem_undo *)semu;
	for (i = 0; i < seminfo.semmnu - 1; i++) {
		semfup->un_np =
		    (struct sem_undo *)((uintptr_t)semfup + seminfo.semusz);
		semfup = semfup->un_np;
	}
	semfup->un_np = NULL;
	semfup = (struct sem_undo *)semu;

	for (i = 0; i < seminfo.semmni; i++)
		mutex_init(&sema_locks[i], NULL, MUTEX_DEFAULT, NULL);

	if ((retval = mod_install(&modlinkage)) == 0)
		return (0);

	for (i = 0; i < seminfo.semmni; i++)
		mutex_destroy(&sema_locks[i]);

	kmem_free(sem, seminfo.semmns * sizeof (struct sem));
	kmem_free(sema, seminfo.semmni * sizeof (struct semid_ds));
	kmem_free(sem_undo, v.v_proc * sizeof (struct sem_undo *));
	kmem_free(semu, (INTSZ(seminfo.semusz * seminfo.semmnu) *
		sizeof (int)));
	kmem_free(sema_locks, seminfo.semmni * sizeof (kmutex_t));

	sem = NULL;
	sema = NULL;
	sem_undo = NULL;
	semu = NULL;
	sema_locks = NULL;
	semfup = NULL;

	vmem_destroy(sem_arena);
	sem_arena = NULL;

	mutex_destroy(&sem_lock);

	return (retval);
}

/*
 * See comments in shm.c
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
 * Argument vectors for the various flavors of semsys().
 */

#define	SEMCTL	0
#define	SEMGET	1
#define	SEMOP	2

/*
 * semaoe - Create or update adjust on exit entry.
 */
static int
semaoe(
	short	val,	/* operation value to be adjusted on exit */
	int	id,	/* semid */
	ushort	num)	/* semaphore number */
{
	struct undo		*uup;	/* ptr to entry to update */
	struct undo		*uup2;	/* ptr to move entry */
	struct sem_undo		*up;	/* ptr to process undo struct */
	struct sem_undo		*up2;	/* ptr to undo list */
	int			i;	/* loop control */
	int			found;	/* matching entry found flag */
	proc_t			*p;

	if (val == 0)
		return (0);
	if (val > seminfo.semaem || val < -seminfo.semaem)
		return (ERANGE);

	/*
	 * If this process doesn't yet have any undo structures,
	 * get a free one and initialize it.
	 */
	mutex_enter(&sem_lock);
	p = curproc;
	if ((up = sem_undo[p->p_slot]) == NULL)
		if ((up = semfup) == NULL) {
			mutex_exit(&sem_lock);
			return (ENOSPC);
		} else {
			semfup = up->un_np;
			up->un_np = NULL;
			sem_undo[p->p_slot] = up;
		}
	/*
	 * Check for an existing undo structure for this semaphore.
	 */
	for (uup = up->un_ent, found = i = 0; i < up->un_cnt; i++) {
		if (uup->un_id < id ||
		    (uup->un_id == id && uup->un_num < num)) {
			uup++;
			continue;
		}
		if (uup->un_id == id && uup->un_num == num)
			found = 1;
		break;
	}
	/*
	 * If this is a new undo structure, link it into the active undo list.
	 * Then look for the undo entry insertion point, keeping the entries
	 * sorted by id; if necessary, shuffle remaining entries up.
	 */
	if (!found) {
		if (up->un_cnt >= seminfo.semume) {
			mutex_exit(&sem_lock);
			return (EINVAL);
		}
		if (up->un_cnt == 0) {
			up->un_np = semunp;
			semunp = up;
		}
		uup2 = &up->un_ent[up->un_cnt++];
		while (uup2-- > uup)
			*(uup2 + 1) = *uup2;
		/* Insert a new undo entry, then done */
		uup->un_id = id;
		uup->un_num = num;
		uup->un_aoe = -val;
		mutex_exit(&sem_lock);
		return (0);
	}
	/*
	 * If an entry already existed for this semaphore, adjust it.
	 */
	uup->un_aoe -= val;
	if (uup->un_aoe > seminfo.semaem || uup->un_aoe < -seminfo.semaem) {
		uup->un_aoe += val;
		mutex_exit(&sem_lock);
		return (ERANGE);
	}
	/*
	 * If new adjust-on-exit value is zero, remove this undo entry
	 * from the list.  If new undo entry count is zero, remove this
	 * undo structure from the active undo list; however, retain this
	 * structure for the current process until semexit().
	 */
	if (uup->un_aoe == 0) {
		uup2 = &up->un_ent[--(up->un_cnt)];
		while (uup++ < uup2)
			*(uup - 1) = *uup;
		if (up->un_cnt == 0) {

			/* Remove process from undo list. */
			if (semunp == up)
				semunp = up->un_np;
			else {
				for (up2 = semunp; up2 != NULL;
				    up2 = up2->un_np) {
					if (up2->un_np == up) {
						up2->un_np = up->un_np;
						break;
					}
				}
			}
			up->un_np = NULL;
		}
	}
	mutex_exit(&sem_lock);
	return (0);
}

/*
 * semunrm - Undo entry remover.
 *
 * This routine is called to clear all undo entries for a set of semaphores
 * that are being removed from the system or are being reset by SETVAL or
 * SETVALS commands to semctl.
 */
static void
semunrm(
	int	id,	/* semid */
	ushort	low,	/* lowest semaphore being changed */
	ushort	high)	/* highest semaphore being changed */
{
	struct sem_undo	*pp;	/* ptr to predecessor to p */
	struct	sem_undo	*p;	/* ptr to current entry */
	struct undo		*up;	/* ptr to undo entry */
	int			i;	/* loop control */
	int			j;	/* loop control */

	pp = NULL;
	mutex_enter(&sem_lock);
	p = semunp;
	while (p != NULL) {

		/* Search through current structure for matching entries. */
		for (up = p->un_ent, i = 0; i < p->un_cnt; /* CSTYLED */) {
			if (id < up->un_id)
				break;
			if (id > up->un_id || low > up->un_num) {
				up++;
				i++;
				continue;
			}
			if (high < up->un_num)
				break;
			/* squeeze out one entry */
			for (j = i; ++j < p->un_cnt; /* CSTYLED */)
				p->un_ent[j - 1] = p->un_ent[j];
			p->un_cnt--;
		}

		/* Reset pointers for next round. */
		if (p->un_cnt == 0) {

			/* Remove from linked list. */
			if (pp == NULL) {
				semunp = p->un_np;
				p->un_np = NULL;
				p = semunp;
			} else {
				pp->un_np = p->un_np;
				p->un_np = NULL;
				p = pp->un_np;
			}
		} else {
			pp = p;
			p = p->un_np;
		}
	}
	mutex_exit(&sem_lock);
}

/*
 * semundo - Undo work done up to finding an operation that can't be done.
 */
static void
semundo(
	struct sembuf	*op,	/* first operation that was done ptr */
	int		n,	/* # of operations that were done */
	int		id,	/* semaphore id */
	struct semid_ds *sp)	/* semaphore data structure ptr */
{
	struct sem	*semp;	/* semaphore ptr */

	for (op += n - 1; n--; op--) {
		if (op->sem_op == 0)
			continue;
		semp = &sp->sem_base[op->sem_num];
		semp->semval -= op->sem_op;
		if (op->sem_flg & SEM_UNDO)
			(void) semaoe(-op->sem_op, id, op->sem_num);
	}
}

/*
 * semconv_lock - Convert user supplied semid into a ptr to the associated
 * semaphore header.
 *
 * Return the associated semaphore lock (held). NULL on errors.
 */
static kmutex_t *
semconv_lock(
	int	s,	/* semid */
	struct semid_ds	**spp)	/* semaphore header to be returned */
{
	struct semid_ds	*sp;	/* ptr to associated header */
	int	index;
	int	sequence;

	index = s & SEMA_INDEX_MASK;
	if (index >= seminfo.semmni)
		return (NULL);
	sequence = (s >> SEMA_SEQ_SHIFT) & SEMA_SEQ_MASK;
	sp = &sema[index];
	mutex_enter(&sema_locks[index]);
	if ((sp->sem_perm.mode & IPC_ALLOC) == 0 ||
	    sequence != sp->sem_perm.seq) {
		mutex_exit(&sema_locks[index]);
		return (NULL);
	}
#ifdef C2_AUDIT
	if (audit_active)
		audit_ipc(AT_IPC_SEM, s, sp);
#endif
	*spp = sp;
	return (&sema_locks[index]);
}

/*
 * Return 0 if semaphore really exists, EINVAL otherwise.
 *
 * We assume the semid_ds is already locked.
 */
static int
semconv(int s)				/* semid */
{
	struct semid_ds	*sp;	/* ptr to associated header */
	uint	sequence;
	int	index;

	sequence = (s >> SEMA_SEQ_SHIFT) & SEMA_SEQ_MASK;
	index = s & SEMA_INDEX_MASK;
	if (index >= seminfo.semmni)
		return (EINVAL);
	sp = &sema[index];

	ASSERT(MUTEX_HELD(&sema_locks[s & SEMA_INDEX_MASK]));
	if ((sp->sem_perm.mode & IPC_ALLOC) == 0 ||
	    sequence != sp->sem_perm.seq) {
		return (EINVAL);
	}
	return (0);
}

/*
 * semctl - Semctl system call.
 */
static int
semctl(int semid, uint semnum, int cmd, uintptr_t arg)
{
	struct	semid_ds	*sp;	/* ptr to semaphore header */
	struct sem		*p;	/* ptr to semaphore */
	unsigned int		i;	/* loop control */
	int			error = 0;
	int			retval = 0;
	struct cred		*cr;
	kmutex_t		*lock;
	model_t			mdl = get_udatamodel();
	struct o_semid_ds32	osid32;
	STRUCT_DECL(semid_ds, sid);

	cr = CRED();

	STRUCT_INIT(sid, mdl);

	if ((lock = semconv_lock(semid, &sp)) == NULL)
		return (set_errno(EINVAL));
	switch (cmd) {

	/* Remove semaphore set. */
	case IPC_O_RMID:
		if (mdl != DATAMODEL_ILP32) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		/* FALLTHROUGH */
	case IPC_RMID:
		if (cr->cr_uid != sp->sem_perm.uid &&
		    cr->cr_uid != sp->sem_perm.cuid && !suser(cr)) {
			mutex_exit(lock);
			return (set_errno(EPERM));
		}
		semunrm(semid, 0, sp->sem_nsems);
		for (i = sp->sem_nsems, p = sp->sem_base; i--; p++) {
			p->semval = p->sempid = 0;
			if (p->semncnt) {
				cv_broadcast(&p->semncnt_cv);
				p->semncnt = 0;
			}
			if (p->semzcnt) {
				cv_broadcast(&p->semzcnt_cv);
				p->semzcnt = 0;
			}
		}
		vmem_free(sem_arena, (caddr_t)(sp->sem_base - sem) + 1,
		    sp->sem_nsems);

		if (((semid >> SEMA_SEQ_SHIFT) & SEMA_SEQ_MASK) ==
		    SEMA_SEQ_MAX)
			sp->sem_perm.seq = 0;
		else
			sp->sem_perm.seq++;
		sp->sem_perm.mode = 0;
		mutex_exit(lock);
		return (0);

	/* Set ownership and permissions. */
	case IPC_SET:  {

		if (cr->cr_uid != sp->sem_perm.uid &&
		    cr->cr_uid != sp->sem_perm.cuid && !suser(cr)) {
			mutex_exit(lock);
			return (set_errno(EPERM));
		}

		if (copyin((void *)arg, STRUCT_BUF(sid), STRUCT_SIZE(sid))) {
			mutex_exit(lock);
			return (set_errno(EFAULT));
		}
		if (STRUCT_FGET(sid, sem_perm.uid) < 0 ||
		    STRUCT_FGET(sid, sem_perm.uid) > MAXUID ||
		    STRUCT_FGET(sid, sem_perm.gid) < 0 ||
		    STRUCT_FGET(sid, sem_perm.gid) > MAXUID) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		sp->sem_perm.uid = STRUCT_FGET(sid, sem_perm.uid);
		sp->sem_perm.gid = STRUCT_FGET(sid, sem_perm.gid);
		sp->sem_perm.mode = STRUCT_FGET(sid, sem_perm.mode)
		    & 0777 | IPC_ALLOC;
		sp->sem_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_SEM, sp);
		}
#endif
		mutex_exit(lock);
		return (0);
	    }

	case IPC_O_SET: {
		if (mdl != DATAMODEL_ILP32) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}

		if (cr->cr_uid != sp->sem_perm.uid &&
		    cr->cr_uid != sp->sem_perm.cuid && !suser(cr)) {
			mutex_exit(lock);
			return (set_errno(EPERM));
		}
		if (copyin((void *)arg, &osid32, sizeof (osid32))) {
			mutex_exit(lock);
			return (set_errno(EFAULT));
		}
		/* XXX	Iff MAXUID > USHRT_MAX this becomes a bit silly */
		if (osid32.sem_perm.uid > MAXUID ||
		    osid32.sem_perm.gid > MAXUID) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		sp->sem_perm.uid = osid32.sem_perm.uid;
		sp->sem_perm.gid = osid32.sem_perm.gid;
		sp->sem_perm.mode = osid32.sem_perm.mode & 0777 | IPC_ALLOC;
		sp->sem_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_SEM, sp);
		}
#endif
		mutex_exit(lock);
		return (0);
	    }

	/* Get semaphore data structure. */
	case IPC_O_STAT: {
		if (mdl != DATAMODEL_ILP32) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}

		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}

		/*
		 * copy expanded semid_ds structure to an SVR3 semid_ds version.
		 * Check whether SVR4 values are too large to store into an SVR3
		 * semid_ds structure.
		 */

		if ((unsigned)sp->sem_perm.uid > USHRT_MAX ||
		    (unsigned)sp->sem_perm.gid > USHRT_MAX ||
		    (unsigned)sp->sem_perm.cuid > USHRT_MAX ||
		    (unsigned)sp->sem_perm.cgid > USHRT_MAX ||
		    (unsigned)sp->sem_perm.seq > USHRT_MAX) {
			mutex_exit(lock);
			return (set_errno(EOVERFLOW));
		}

		osid32.sem_perm.uid = (o_uid_t)sp->sem_perm.uid;
		osid32.sem_perm.gid = (o_gid_t)sp->sem_perm.gid;
		osid32.sem_perm.cuid = (o_uid_t)sp->sem_perm.cuid;
		osid32.sem_perm.cgid = (o_gid_t)sp->sem_perm.cgid;
		osid32.sem_perm.mode = (o_mode_t)sp->sem_perm.mode;
		osid32.sem_perm.seq = (ushort)sp->sem_perm.seq;
		osid32.sem_perm.key = sp->sem_perm.key;

		osid32.sem_base = NULL;		/* kernel addr */
		osid32.sem_nsems = sp->sem_nsems;
		osid32.sem_otime = sp->sem_otime;
		osid32.sem_ctime = sp->sem_ctime;
		mutex_exit(lock);

		if (copyout(&osid32, (void *)arg, sizeof (osid32)))
			return (set_errno(EFAULT));
		return (0);
	    }

	case IPC_STAT:
		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}

		STRUCT_FSET(sid, sem_perm.uid, sp->sem_perm.uid);
		STRUCT_FSET(sid, sem_perm.gid,  sp->sem_perm.gid);
		STRUCT_FSET(sid, sem_perm.cuid,  sp->sem_perm.cuid);
		STRUCT_FSET(sid, sem_perm.cgid,  sp->sem_perm.cgid);
		STRUCT_FSET(sid, sem_perm.mode,  sp->sem_perm.mode);
		STRUCT_FSET(sid, sem_perm.seq,  sp->sem_perm.seq);
		STRUCT_FSET(sid, sem_perm.key, sp->sem_perm.key);

		STRUCT_FSETP(sid, sem_base, NULL);	/* kernel addr */
		STRUCT_FSET(sid, sem_nsems, sp->sem_nsems);
		STRUCT_FSET(sid, sem_otime, sp->sem_otime);
		STRUCT_FSET(sid, sem_ctime, sp->sem_ctime);
		STRUCT_FSET(sid, sem_binary, sp->sem_binary);

		if (copyout(STRUCT_BUF(sid), (void *)arg, STRUCT_SIZE(sid))) {
			mutex_exit(lock);
			return (set_errno(EFAULT));
		}
		mutex_exit(lock);
		return (0);

	/* Get # of processes sleeping for greater semval. */
	case GETNCNT:
		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		if (semnum >= sp->sem_nsems) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		retval = sp->sem_base[semnum].semncnt;
		mutex_exit(lock);
		return (retval);

	/* Get pid of last process to operate on semaphore. */
	case GETPID:
		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		if (semnum >= sp->sem_nsems) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		retval = sp->sem_base[semnum].sempid;
		mutex_exit(lock);
		return (retval);

	/* Get semval of one semaphore. */
	case GETVAL:
		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		if (semnum >= sp->sem_nsems) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		retval = sp->sem_base[semnum].semval;
		mutex_exit(lock);
		return (retval);

	/* Get all semvals in set. */
	case GETALL: {

		ushort_t *semvals = (ushort_t *)arg;

		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}

		for (i = sp->sem_nsems, p = sp->sem_base; i--; p++, semvals++) {
			if (copyout(&p->semval, semvals, sizeof (p->semval))) {
				mutex_exit(lock);
				return (set_errno(EFAULT));
			}
		}
		mutex_exit(lock);
		return (0);
	}

	/* Get # of processes sleeping for semval to become zero. */
	case GETZCNT:
		if (error = ipcaccess(&sp->sem_perm, SEM_R, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		if (semnum >= sp->sem_nsems) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		retval = sp->sem_base[semnum].semzcnt;
		mutex_exit(lock);
		return (retval);

	/* Set semval of one semaphore. */
	case SETVAL:
		if (error = ipcaccess(&sp->sem_perm, SEM_A, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		if (semnum >= sp->sem_nsems) {
			mutex_exit(lock);
			return (set_errno(EINVAL));
		}
		if ((uint_t)arg > seminfo.semvmx) {
			mutex_exit(lock);
			return (set_errno(ERANGE));
		}
		p = &sp->sem_base[semnum];
		if ((p->semval = (ushort)arg) != 0) {
			if (p->semncnt) {
				p->semncnt = 0;
				cv_broadcast(&p->semncnt_cv);
			}
		} else if (p->semzcnt) {
			p->semzcnt = 0;
			cv_broadcast(&p->semzcnt_cv);
		}
		p->sempid = curproc->p_pid;
		semunrm(semid, (ushort)semnum, (ushort)semnum);
		mutex_exit(lock);
		return (0);

	/* Set semvals of all semaphores in set. */
	case SETALL: {
		ushort_t *vals;
		size_t vsize;

		if (error = ipcaccess(&sp->sem_perm, SEM_A, cr)) {
			mutex_exit(lock);
			return (set_errno(error));
		}
		/* allocate space to hold all semaphore values */
		vsize = sp->sem_nsems * sizeof (*vals);
		vals = kmem_alloc(vsize, KM_SLEEP);
			/* XXX - correct call? */

		if (copyin((void *)arg, vals, vsize)) {
			error = set_errno(EFAULT);
			goto seterr;
		}
		for (i = 0; i < sp->sem_nsems; /* CSTYLED */)
			if (vals[i++] > seminfo.semvmx) {
				error = set_errno(ERANGE);
				goto seterr;
			}
		semunrm(semid, 0, sp->sem_nsems);
		for (i = 0, p = sp->sem_base; i < sp->sem_nsems;
		    (p++)->sempid = curproc->p_pid) {
			if ((p->semval = vals[i++]) != 0) {
				if (p->semncnt) {
					p->semncnt = 0;
					cv_broadcast(&p->semncnt_cv);
				}
			} else if (p->semzcnt) {
				p->semzcnt = 0;
				cv_broadcast(&p->semzcnt_cv);
			}
		}
seterr:
		mutex_exit(lock);
		kmem_free(vals, vsize);
		return (error);
	}

	default:
		mutex_exit(lock);
		return (set_errno(EINVAL));
	}

	/* NOTREACHED */
}

/*
 * semexit - Called by exit() to clean up on process exit.
 */
void
semexit(void)
{
	struct sem_undo		*up;	/* process undo struct ptr */
	struct sem_undo		*p;	/* undo struct ptr */
	struct semid_ds		*sp;	/* semid being undone ptr */
	int			i;	/* loop control */
	int			v;	/* adjusted value */
	struct sem		*semp;	/* semaphore ptr */

	if (sem_undo == NULL || (up = sem_undo[curproc->p_slot]) == NULL)
		return;

	if (up->un_cnt == 0) {
		mutex_enter(&sem_lock);
		up->un_np = semfup;
		semfup = up;
		sem_undo[curproc->p_slot] = NULL;
		mutex_exit(&sem_lock);
		return;
	}
	for (i = up->un_cnt; i--; /* CSTYLED */) {
		kmutex_t	*lock;

		if ((lock = semconv_lock(up->un_ent[i].un_id, &sp)) == NULL)
			continue;

		semp = &sp->sem_base[up->un_ent[i].un_num];
		v = (int)(semp->semval + up->un_ent[i].un_aoe);

		if (v < 0 || v > seminfo.semvmx) {
			mutex_exit(lock);
			continue;
		}
		semp->semval = (ushort) v;
		if (v == 0 && semp->semzcnt) {
			semp->semzcnt = 0;
			cv_broadcast(&semp->semzcnt_cv);
		}
		if (up->un_ent[i].un_aoe > 0 && semp->semncnt) {
			semp->semncnt = 0;
			cv_broadcast(&semp->semncnt_cv);
		}
		mutex_exit(lock);
	}
	mutex_enter(&sem_lock);
	up->un_cnt = 0;
	if (semunp == up) {
		semunp = up->un_np;
	} else {
		for (p = semunp; p != NULL; p = p->un_np) {
			if (p->un_np == up) {
				p->un_np = up->un_np;
				break;
			}
		}
	}
	up->un_np = semfup;
	semfup = up;
	sem_undo[curproc->p_slot] = NULL;
	mutex_exit(&sem_lock);
}

/*
 * semget - Semget system call.
 */
static int
semget(key_t key, int nsems, int semflg)
{
	struct semid_ds	*sp;			/* semaphore header ptr */
	ulong		i;			/* temp */
	int		s;			/* ipcget status return */
	int		error;

	if (error = ipcget(key, semflg, (struct ipc_perm *)sema,
	    seminfo.semmni, sizeof (*sp), &s, (struct ipc_perm **)&sp))
		return (set_errno(error));

	if (s) {
		/* This is a new semaphore set.  Finish initialization. */
		if (nsems <= 0 || nsems > seminfo.semmsl) {
			sp->sem_perm.mode = 0;
			return (set_errno(EINVAL));
		}
		i = (ulong_t)vmem_alloc(sem_arena, nsems, VM_NOSLEEP);
		if (i == 0) {
			sp->sem_perm.mode = 0;
			return (set_errno(ENOSPC));
		}
		/* point to first semaphore */
		sp->sem_base = &sem[i - 1];
		sp->sem_binary = 1;
		sp->sem_nsems = (ushort)nsems;
		sp->sem_ctime = hrestime.tv_sec;
		sp->sem_otime = 0;

	} else if (nsems && sp->sem_nsems < (unsigned)nsems)
		return (set_errno(EINVAL));

#ifdef C2_AUDIT
	if (audit_active) {
		audit_ipcget(AT_IPC_SEM, sp);
	}
#endif
	/* sem_perm.seq incremented by semctl() when semaphore group released */
	return ((sp->sem_perm.seq << SEMA_SEQ_SHIFT) + (sp - sema));
}

/*
 * semop - Semop system call.
 */
static int
semop(int semid, struct sembuf *sops, size_t nsops)
{
	struct sembuf	*op;	/* ptr to operation */
	int		i;	/* loop control */
	struct semid_ds	*sp;	/* ptr to associated header */
	struct sem	*semp;	/* ptr to semaphore */
	int 		again, error = 0;
	struct sembuf	*uops;	/* ptr to copy of user ops */
	struct sembuf 	x_sem;	/* avoid kmem_alloc's */
	kmutex_t	*lock;
	int		binary_type;

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.sema, 1); /* bump semaphore op count */
	if (nsops > seminfo.semopm)
		return (set_errno(E2BIG));

	/* allocate space to hold all semaphore ops */
	if (nsops == 1)
		uops = &x_sem;
	else if (nsops == 0)
		return (0);
	else
		uops = kmem_alloc(nsops * sizeof (*uops), KM_SLEEP);

	if (copyin(sops, uops, nsops * sizeof (*op))) {
		error = EFAULT;
		goto semoperr2;
	}

	if ((lock = semconv_lock(semid, &sp)) == NULL) {
		error = EINVAL;
		goto semoperr2;
	}

	/* Verify that sem #s are in range and permissions are granted. */
	for (i = 0, op = uops; i++ < nsops; op++) {
		if (error = ipcaccess(&sp->sem_perm,
		    op->sem_op ? SEM_A : SEM_R, CRED()))
			goto semoperr;
		if (op->sem_num >= sp->sem_nsems) {
			error = EFBIG;
			goto semoperr;
		}
	}
	again = 0;
check:
	/*
	 * Loop waiting for the operations to be satisfied atomically.
	 * Actually, do the operations and undo them if a wait is needed
	 * or an error is detected.
	 */
	if (again) {
		/* Verify that the semaphores haven't been removed. */
		if (semconv(semid) != 0) {
			error = EIDRM;
			goto semoperr;
		}
	}
	again = 1;

	/* Determine if the semaphore is a simple binary semaphore */
	binary_type = sp->sem_binary && (sp->sem_nsems == 1);

	for (i = 0, op = uops; i < nsops; i++, op++) {
		semp = &sp->sem_base[op->sem_num];
		if (op->sem_op > 0) {	/* i.e. sema_v */
			int	signal = 0;

			if (op->sem_op + (int)semp->semval > seminfo.semvmx ||
			    ((op->sem_flg & SEM_UNDO) &&
			    (error = semaoe(op->sem_op, semid,
			    op->sem_num)))) {
				if (i)
					semundo(uops, i, semid, sp);
				if (error == 0)
					error = ERANGE;	/* XXX - not in ours */
				goto semoperr;
			}
			semp->semval += op->sem_op;
			/*
			 * If we are only incrementing the semaphore value
			 * by one on a binary semaphore, we can cv_signal.
			 */
			if (op->sem_op == 1 && binary_type)
				signal = 1;
			if (semp->semncnt) {
				if (signal) {
					semp->semncnt -= 1;
					cv_signal(&semp->semncnt_cv);
				} else {
					semp->semncnt = 0;
					cv_broadcast(&semp->semncnt_cv);
				}
			}
			if (semp->semzcnt && !semp->semval) {
				if (signal) {
					semp->semzcnt -= 1;
					cv_signal(&semp->semzcnt_cv);
				} else {
					semp->semzcnt = 0;
					cv_broadcast(&semp->semzcnt_cv);
				}
			}
			continue;
		}
		if (op->sem_op < 0) {	/* i.e. sema_p */
			if (semp->semval >= (unsigned)(-op->sem_op)) {
				if ((op->sem_flg & SEM_UNDO) &&
				    (error = semaoe(op->sem_op, semid,
				    op->sem_num))) {
					if (i)
						semundo(uops, i, semid, sp);
					goto semoperr;
				}
				semp->semval += op->sem_op;
				if (semp->semzcnt && !semp->semval) {
					semp->semzcnt = 0;
					cv_broadcast(&semp->semzcnt_cv);
				}
				continue;
			}
			if (i)
				semundo(uops, i, semid, sp);
			if (op->sem_flg & IPC_NOWAIT) {
				error = EAGAIN;
				goto semoperr;
			}

			semp->semncnt++;
			/*
			 * Mark the semaphore set as not a binary type
			 * if we are decrementing the value by more than 1.
			 *
			 * V operations will resort to cv_broadcast
			 * for this set because there are too many weird
			 * cases that have to be caught.
			 */
			if (op->sem_op < -1)
				sp->sem_binary = 0;
			if (!cv_wait_sig(&semp->semncnt_cv, lock)) {
				if ((semp->semncnt)-- <= 1) {
					semp->semncnt = 0;
					cv_broadcast(&semp->semncnt_cv);
				}
				error = EINTR;
				goto semoperr;
			}
			goto check;
		}
		if (semp->semval) {
			if (i)
				semundo(uops, i, semid, sp);
			if (op->sem_flg & IPC_NOWAIT) {
				error = EAGAIN;
				goto semoperr;
			}

			semp->semzcnt++;
			if (!cv_wait_sig(&semp->semzcnt_cv, lock)) {
				if ((semp->semzcnt)-- <= 1) {
					semp->semzcnt = 0;
					cv_broadcast(&semp->semzcnt_cv);
				}
				error = EINTR;
				goto semoperr;
			}
			goto check;
		}
	}

	/* All operations succeeded.  Update sempid for accessed semaphores. */
	for (i = 0, op = uops; i++ < nsops;
	    sp->sem_base[(op++)->sem_num].sempid = curproc->p_pid)
		;
	sp->sem_otime = hrestime.tv_sec;
	mutex_exit(lock);
	/* Before leaving, deallocate the buffer that held the user semops */
	if (nsops != 1)
		kmem_free(uops, sizeof (*uops) * nsops);
	return (0);
/*
 * Error return labels.
 */
semoperr:
	mutex_exit(lock);
semoperr2:
	/* Before leaving, deallocate the buffer that held the user semops */
	if (nsops != 1)
		kmem_free(uops, sizeof (*uops) * nsops);
	return (set_errno(error));
}

/*
 * semsys - System entry point for semctl, semget, and semop system calls.
 */
static int
semsys(int opcode, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
	int error;

	switch (opcode) {
	case SEMCTL:
		error = semctl((int)a1, (uint)a2, (int)a3, a4);
		break;
	case SEMGET:
		mutex_enter(&sem_lock);
		error = semget((key_t)a1, (int)a2, (int)a3);
		mutex_exit(&sem_lock);
		break;
	case SEMOP:
		error = semop((int)a1, (struct sembuf *)a2, (size_t)a3);
		break;
	default:
		error = set_errno(EINVAL);
		break;
	}
	return (error);
}
