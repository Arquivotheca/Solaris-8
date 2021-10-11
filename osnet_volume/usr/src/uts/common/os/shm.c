/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)shm.c	1.89	99/11/22 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1996, 1999  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Inter-Process Communication Shared Memory Facility.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/user.h>
#include <sys/ipc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/prsystm.h>
#include <sys/sysmacros.h>
#include <sys/shm.h>
#include <sys/tuneable.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/swap.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/lwpchan_impl.h>

#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <vm/seg_vn.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/vpage.h>

#include <c2/audit.h>

#ifndef sun
extern struct shmid_ds	shmem[];	/* shared memory headers */
#endif /* !sun */

static int kshmdt(proc_t *pp, caddr_t addr);
static int shmem_lock(struct anon_map *amp);
static void shmem_unlock(struct anon_map *amp, uint_t lck);
static void sa_add(struct proc *pp, caddr_t addr, size_t len,
	struct anon_map *amp, struct sptinfo *sinfp, ulong_t flags, int id);
static void shm_rm_amp(struct anon_map *amp, uint_t lckflag);

/* changes for ISM */
#include <vm/seg_spt.h>

/* hook in /etc/system - only for internal testing purpose */
int share_page_table;
int ism_off;

/*
 * Protects all shm data except amp's
 */
static kmutex_t shm_lock;
static kmutex_t *shmem_locks;

/*
 * The following variables shminfo_* are there so that the
 * elements of the data structure shminfo can be tuned
 * (if necessary) using the /etc/system file syntax for
 * tuning of integer data types.
 */
size_t	shminfo_shmmax = 1048576;	/* max shared memory segment size */
size_t	shminfo_shmmin = 1;		/* min shared memory segment size */
int	shminfo_shmmni = 100;		/* # of shared memory identifiers */
int	shminfo_shmseg = 6;		/* segments per process		  */

/*
 * Argument vectors for the various flavors of shmsys().
 */

#define	SHMAT	0
#define	SHMCTL	1
#define	SHMDT	2
#define	SHMGET	3

#include <sys/modctl.h>
#include <sys/syscall.h>

static uintptr_t shmsys(int, uintptr_t, uintptr_t, uintptr_t);

#ifdef	_SYSCALL32_IMPL

static struct sysent ipcshm_sysent = {
	4,
	SE_ARGC | SE_NOUNLOAD | SE_64RVAL,
	(int (*)())shmsys
};

static struct sysent ipcshm_sysent32 = {
	4,
	SE_ARGC | SE_NOUNLOAD | SE_32RVAL1,
	(int (*)())shmsys
};

#else	/* _SYSCALL32_IMPL */

static struct sysent ipcshm_sysent = {
	4,
	SE_ARGC | SE_NOUNLOAD | SE_32RVAL1,
	(int (*)())shmsys
};

#endif	/* _SYSCALL32_IMPL */

/*
 * Module linkage information for the kernel.
 */
static struct modlsys modlsys = {
	&mod_syscallops, "System V shared memory", &ipcshm_sysent
};

#ifdef	_SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32, "32-bit System V shared memory", &ipcshm_sysent32
};
#endif	/* _SYSCALL32_IMPL */

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef	_SYSCALL32_IMPL
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
	 * shminfo_* are inited above to default values
	 * These values can be tuned if need be using the
	 * integer tuning capabilities in the /etc/system file.
	 */
	shminfo.shmmax = shminfo_shmmax;
	shminfo.shmmin = shminfo_shmmin;
	shminfo.shmmni = shminfo_shmmni;
	shminfo.shmseg = shminfo_shmseg;


	/*
	 * 1213443: u.u_nshmseg is now defined as a short.
	 */
	if (shminfo.shmseg >= SHRT_MAX) {
		cmn_err(CE_NOTE, "shminfo.shmseg limited to %d", SHRT_MAX);
		shminfo.shmseg = SHRT_MAX;
	}

	/*
	 * Don't use more than 25% of the available kernel memory
	 */
	mavail = (uint64_t)kmem_maxavail() / 4;
	if ((uint64_t)shminfo.shmmni * sizeof (struct shmid_ds) +
	    (uint64_t)shminfo.shmmni * sizeof (kmutex_t) > mavail) {
		cmn_err(CE_WARN,
		    "shmsys: can't load module, too much memory requested");
		return (ENOMEM);
	}

	mutex_init(&shm_lock, NULL, MUTEX_DEFAULT, NULL);

	ASSERT(shmem == NULL);
	ASSERT(shmem_locks == NULL);
	shmem = kmem_zalloc(shminfo.shmmni * sizeof (struct shmid_ds),
		KM_SLEEP);
	shmem_locks = kmem_zalloc(shminfo.shmmni * sizeof (kmutex_t), KM_SLEEP);

	for (i = 0; i < shminfo.shmmni; i++)
		mutex_init(&shmem_locks[i], NULL, MUTEX_DEFAULT, NULL);

	if ((retval = mod_install(&modlinkage)) == 0)
		return (0);

	for (i = 0; i < shminfo.shmmni; i++)
		mutex_destroy(&shmem_locks[i]);

	kmem_free(shmem, shminfo.shmmni * sizeof (struct shmid_ds));
	kmem_free(shmem_locks, shminfo.shmmni * sizeof (kmutex_t));

	shmem = NULL;
	shmem_locks = NULL;

	mutex_destroy(&shm_lock);

	return (retval);
}

int
_fini(void)
{
	/*
	 * shm segment lives after the process that created it is gone.
	 * There are many ways that shm segment can be implicitly used
	 * even when the shm id is removed!  Shared memory segment may hold
	 * important system resources like swap space and anon-maps...
	 * Furthermore, processes can fork with as_map(),
	 * as_unmap(), and async_io can be done to the shared memory segment.
	 * We need a global counting scheme (outside of shmsys code) to keep
	 * track of all these implicit references.  When ALL these references
	 * are gone, we can then safely allow unloading of the shmsys module.
	 *
	 * Before that scheme is available and tested for all known problems,
	 * we simply return EBUSY.
	 */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Convert user supplied shmid into a ptr to the associated
 * shared memory header.
 */
static kmutex_t *
shmconv_lock(int s, struct shmid_ds **spp)
{
	struct shmid_ds *sp;	/* ptr to associated header */
	int	index;
	kmutex_t	*lock;

	if (s < 0)
		return (NULL);

	index = s % shminfo.shmmni;
	lock = &shmem_locks[index];
	sp = &shmem[index];
	mutex_enter(&shm_lock);
	mutex_enter(lock);
	if (!(sp->shm_perm.mode & IPC_ALLOC) ||
	    s / shminfo.shmmni != sp->shm_perm.seq) {
		mutex_exit(lock);
		mutex_exit(&shm_lock);
		return (NULL);
	}
	mutex_exit(&shm_lock);
#ifdef C2_AUDIT
	if (audit_active)
		audit_ipc(AT_IPC_SHM, s, (void *)sp);
#endif
	*spp = sp;
	return (lock);
}

/*
 * Shmat (attach shared segment) system call.
 */
static int
shmat(int shmid, caddr_t uaddr, int uflags, uintptr_t *rvp)
{
	struct shmid_ds *sp;	/* shared memory header ptr */
	size_t	size;
	int	error = 0;
	proc_t *pp = curproc;
	struct as *as = pp->p_as;
	struct segvn_crargs	crargs;	/* segvn create arguments */
	kmutex_t	*lock;
	struct seg 	*segspt = NULL;
	caddr_t		addr = uaddr;
	int		flags = uflags;
	uchar_t		prot = PROT_ALL;
	int result;

	if ((lock = shmconv_lock(shmid, &sp)) == NULL)
		return (EINVAL);
	if (error = ipcaccess(&sp->shm_perm, SHM_R, CRED()))
		goto errret;
	if (spt_on(flags) &&
	    (error = ipcaccess(&sp->shm_perm, SHM_W, CRED())))
		goto errret;
	if ((flags & SHM_RDONLY) == 0 &&
	    (error = ipcaccess(&sp->shm_perm, SHM_W, CRED())))
		goto errret;

	mutex_enter(&pp->p_lock);
	if (PTOU(pp)->u_nshmseg >= shminfo.shmseg) {
		error = EMFILE;
		mutex_exit(&pp->p_lock);
		goto errret;
	}
	mutex_exit(&pp->p_lock);

	if (ism_off)
		flags = flags & ~SHM_SHARE_MMU;

	mutex_enter(&sp->shm_amp->lock);
	size = sp->shm_amp->size;
	mutex_exit(&sp->shm_amp->lock);

	/* somewhere to record spt info for final detach */
	if (sp->shm_sptinfo == NULL)
		sp->shm_sptinfo = kmem_zalloc(sizeof (sptinfo_t), KM_SLEEP);

	as_rangelock(as);

	if (spt_on(flags)) {
		/*
		 * Handle ISM
		 */
		uint_t	n;
		size_t	share_size, ssize;
		struct	sptshm_data ssd;

		n = page_num_pagesizes();
		if (n < 2) { /* large pages aren't supported */
			as_rangeunlock(as);
			error = EINVAL;
			goto errret;
		}
		share_size = page_get_pagesize(n - 1);
		if (share_size == 0) {
			as_rangeunlock(as);
			error = EINVAL;
			goto errret;
		}
		size = roundup(size, share_size);
		if (addr == 0) {
			/*
			 * Add in another share_size so we know there
			 * is a share_size aligned address in the segment
			 * returned by map_addr()
			 */
			ssize = size + share_size;
			map_addr(&addr, ssize, 0ll, 1, 0);
			if (addr == NULL) {
				as_rangeunlock(as);
				error = ENOMEM;
				goto errret;
			}
			addr = (caddr_t)roundup((uintptr_t)addr, share_size);
		} else {
			/* Use the user-supplied attach address */
			caddr_t base;
			size_t len;

			/*
			 * Check that the address range
			 *  1) is properly aligned
			 *  2) is correct in unix terms
			 *  3) is within an unmapped address segment
			 */
			base = addr;
			len = size;		/* use spt aligned size */
			/* XXX - in SunOS, is sp->shm_segsz */
			if ((uintptr_t)base & (share_size - 1)) {
				error = EINVAL;
				as_rangeunlock(as);
				goto errret;
			}
			result = valid_usr_range(base, len, prot, as,
			    as->a_userlimit);
			if (result == RANGE_BADPROT) {
				/*
				 * We try to accomodate processors which
				 * may not support execute permissions on
				 * all ISM segments by trying the check
				 * again but without PROT_EXEC.
				 */
				prot &= ~PROT_EXEC;
				result = valid_usr_range(base, len, prot, as,
				    as->a_userlimit);
			}
			if (result != RANGE_OKAY ||
			    as_gap(as, len, &base, &len, AH_LO,
			    (caddr_t)NULL) != 0) {
				error = EINVAL;
				as_rangeunlock(as);
				goto errret;
			}
		}
		if (!isspt(sp)) {
			error = sptcreate(size, &segspt, sp->shm_amp, prot);
			if (error) {
				as_rangeunlock(as);
				goto errret;
			}
			sp->shm_sptinfo->sptas = segspt->s_as;
			sp->shm_sptseg = segspt;
			sp->shm_sptprot = prot;
			sp->shm_lkcnt = 0;
		} else if ((prot & sp->shm_sptprot) != sp->shm_sptprot) {
			/*
			 * Ensure we're attaching to an ISM segment with
			 * fewer or equal permissions than what we're
			 * allowed.  Fail if the segment has more
			 * permissions than what we're allowed.
			 */
			error = EACCES;
			as_rangeunlock(as);
			goto errret;
		}

		ssd.sptseg = sp->shm_sptseg;
		ssd.sptas = sp->shm_sptinfo->sptas;
		ssd.amp = sp->shm_amp;
		error = as_map(as, addr, size, segspt_shmattach, &ssd);
		if (error == 0)
			sp->shm_cnattch++; /* keep count of ISM attaches */
	} else {
		/*
		 * Normal case.
		 */
		if (flags & SHM_RDONLY)
			prot &= ~PROT_WRITE;

		if (addr == 0) {
			/* Let the system pick the attach address */
			map_addr(&addr, size, 0ll, 1, 0);
			if (addr == NULL) {
				as_rangeunlock(as);
				error = ENOMEM;
				goto errret;
			}
		} else {
			/* Use the user-supplied attach address */
			caddr_t base;
			size_t len;

			if (flags & SHM_RND)
				addr = (caddr_t)((uintptr_t)addr &
				    ~(SHMLBA - 1));
			/*
			 * Check that the address range
			 *  1) is properly aligned
			 *  2) is correct in unix terms
			 *  3) is within an unmapped address segment
			 */
			base = addr;
			len = size;		/* use aligned size */
			/* XXX - in SunOS, is sp->shm_segsz */
			if ((uintptr_t)base & PAGEOFFSET) {
				error = EINVAL;
				as_rangeunlock(as);
				goto errret;
			}
			result = valid_usr_range(base, len, prot, as,
			    as->a_userlimit);
			if (result == RANGE_BADPROT) {
				prot &= ~PROT_EXEC;
				result = valid_usr_range(base, len, prot, as,
				    as->a_userlimit);
			}
			if (result != RANGE_OKAY ||
			    as_gap(as, len, &base, &len,
			    AH_LO, (caddr_t)NULL) != 0) {
				error = EINVAL;
				as_rangeunlock(as);
				goto errret;
			}
		}

		/* Initialize the create arguments and map the segment */
		crargs = *(struct segvn_crargs *)zfod_argsp;
		crargs.offset = 0;
		crargs.type = MAP_SHARED;
		crargs.amp = sp->shm_amp;
		crargs.prot = prot;
		crargs.maxprot = crargs.prot;
		crargs.flags = 0;

		error = as_map(as, addr, size, /* XXX - sp->shm_segsz */
		    segvn_create, &crargs);
	}

	as_rangeunlock(as);
	if (error)
		goto errret;

	/* record shmem range for the detach */
	sa_add(pp, addr, (size_t)size, sp->shm_amp, sp->shm_sptinfo,
	    spt_on(flags) ? SHMSA_ISM : 0, shmid);

	mutex_enter(&sp->shm_amp->lock);
	sp->shm_amp->refcnt++;		/* keep amp until shmdt and IPC_RMID */

	*rvp = (uintptr_t)addr;

	sp->shm_atime = hrestime.tv_sec;
	sp->shm_lpid = pp->p_pid;
	mutex_exit(&sp->shm_amp->lock);
errret:
	mutex_exit(lock);
	return (error);
}


/*
 * Shmctl system call.
 */
/* ARGSUSED */
static int
shmctl(int shmid, int cmd, struct shmid_ds *uds)
{
	struct shmid_ds		*sp;	/* shared memory header ptr */
	struct o_shmid_ds32 	ods32;	/* for SVR3 IPC_O_SET */
	STRUCT_DECL(shmid_ds, ds);	/* for SVR4 IPC_SET */
	int			error = 0;
	struct cred 		*cr;
	int			cnt;
	kmutex_t		*lock;
	model_t			mdl = get_udatamodel();

	STRUCT_INIT(ds, mdl);

	if ((lock = shmconv_lock(shmid, &sp)) == NULL) {
		return (EINVAL);
	}

	cr = CRED();
	switch (cmd) {

	/* Remove shared memory identifier. */
	case IPC_O_RMID:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		/* FALLTHROUGH */
	case IPC_RMID:
		if (cr->cr_uid != sp->shm_perm.uid &&
		    cr->cr_uid != sp->shm_perm.cuid &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		sp->shm_perm.seq++;
		if ((int)sp->shm_perm.seq * shminfo.shmmni + (sp - shmem) < 0)
			sp->shm_perm.seq = 0;

		/*
		 * When we created the shared memory segment,
		 * we set the refcnt to 2. When we shmat (and shmfork)
		 * we bump the refcnt.  We decrement it in
		 * kshmdt (called from shmdt and shmexit),
		 * and in IPC_RMID.  Thus we use a refcnt
		 * of 1 to mean that there are no more references.
		 * We do this so that the anon_map will not
		 * go away until we are ready, even if a process
		 * munmaps it's shared memory.
		 */
		mutex_enter(&sp->shm_amp->lock);
		cnt = --sp->shm_amp->refcnt;
		mutex_exit(&sp->shm_amp->lock);
		if (cnt == 1) {		/* if no attachments */
			if (isspt(sp))
				sptdestroy(sp->shm_sptinfo->sptas,
				    sp->shm_amp);
			if (sp->shm_sptinfo)
				kmem_free(sp->shm_sptinfo, sizeof (sptinfo_t));
			shm_rm_amp(sp->shm_amp, sp->shm_lkcnt);
		}
		sp->shm_lkcnt = 0;
		sp->shm_segsz = (size_t)0;
		sp->shm_amp = NULL;
		sp->shm_sptinfo = NULL;
		sp->shm_perm.mode = 0;
		error = 0;
		break;

	/* Set ownership and permissions. */
	case IPC_O_SET:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		if (cr->cr_uid != sp->shm_perm.uid &&
		    cr->cr_uid != sp->shm_perm.cuid &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		if (copyin(uds, &ods32, sizeof (ods32))) {
			error = EFAULT;
			break;
		}
		if (ods32.shm_perm.uid >= USHRT_MAX ||
		    ods32.shm_perm.gid >= USHRT_MAX) {
			error = EOVERFLOW;
			break;
		}
		if (ods32.shm_perm.uid > MAXUID ||
		    ods32.shm_perm.gid > MAXUID) {
			error = EINVAL;
			break;
		}
		sp->shm_perm.uid = ods32.shm_perm.uid;
		sp->shm_perm.gid = ods32.shm_perm.gid;
		sp->shm_perm.mode =
		    (ods32.shm_perm.mode & 0777) |
		    (sp->shm_perm.mode & ~0777);
		sp->shm_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_MSG, (void *)sp);
		}
#endif
		break;

	case IPC_SET:
		if (cr->cr_uid != sp->shm_perm.uid &&
		    cr->cr_uid != sp->shm_perm.cuid &&
		    !suser(cr)) {
			error = EPERM;
			break;
		}
		if (copyin(uds, STRUCT_BUF(ds), STRUCT_SIZE(ds))) {
			error = EFAULT;
			break;
		}
		if (STRUCT_FGET(ds, shm_perm.uid) < (uid_t)0 ||
		    STRUCT_FGET(ds, shm_perm.uid) > MAXUID ||
		    STRUCT_FGET(ds, shm_perm.gid) < (gid_t)0 ||
		    STRUCT_FGET(ds, shm_perm.gid) > MAXUID) {
			error = EINVAL;
			break;
		}
		sp->shm_perm.uid = STRUCT_FGET(ds, shm_perm.uid);
		sp->shm_perm.gid = STRUCT_FGET(ds, shm_perm.gid);
		sp->shm_perm.mode =
		    (STRUCT_FGET(ds, shm_perm.mode) & 0777) |
		    (sp->shm_perm.mode & ~0777);
		sp->shm_ctime = hrestime.tv_sec;
#ifdef C2_AUDIT
		if (audit_active) {
			audit_ipcget(AT_IPC_MSG, (void *)sp);
		}
#endif
		break;

	/* Get shared memory data structure. */
	case IPC_O_STAT:
		if (mdl != DATAMODEL_ILP32) {
			error = EINVAL;
			break;
		}
		if (error = ipcaccess(&sp->shm_perm, SHM_R, cr))
			break;

		/*
		 * We set refcnt to 2 in shmget.
		 * It is bumped twice for every attach.
		 */
		mutex_enter(&sp->shm_amp->lock);
		sp->shm_nattch = (sp->shm_amp->refcnt >> 1) - 1;
		mutex_exit(&sp->shm_amp->lock);

		/*
		 * copy expanded shmid_ds struct to SVR3 o_shmid_ds.
		 * The o_shmid_ds data structure supports SVR3 applications.
		 * EFT applications use struct shmid_ds.
		 */
		if (sp->shm_perm.uid > USHRT_MAX ||
		    sp->shm_perm.gid > USHRT_MAX ||
		    sp->shm_perm.cuid > USHRT_MAX ||
		    sp->shm_perm.cgid > USHRT_MAX ||
		    sp->shm_perm.seq > USHRT_MAX ||
		    sp->shm_lpid > SHRT_MAX ||
		    sp->shm_cpid > SHRT_MAX ||
		    sp->shm_nattch > USHRT_MAX ||
		    sp->shm_cnattch > USHRT_MAX) {
			error = EOVERFLOW;
			break;
		}
		ods32.shm_perm.uid = (o_uid_t)sp->shm_perm.uid;
		ods32.shm_perm.gid = (o_gid_t)sp->shm_perm.gid;
		ods32.shm_perm.cuid = (o_uid_t)sp->shm_perm.cuid;
		ods32.shm_perm.cgid = (o_gid_t)sp->shm_perm.cgid;
		ods32.shm_perm.mode = (o_mode_t)sp->shm_perm.mode;
		ods32.shm_perm.seq = (ushort_t)sp->shm_perm.seq;
		ods32.shm_perm.key = sp->shm_perm.key;
		ods32.shm_segsz = sp->shm_segsz;
		ods32.shm_amp = NULL;		/* kernel addr */
		ods32.shm_lkcnt = sp->shm_lkcnt;
		ods32.shm_pad[0] = 0; 		/* clear SVR3 reserve pad */
		ods32.shm_pad[1] = 0;
		ods32.shm_lpid = (o_pid_t)sp->shm_lpid;
		ods32.shm_cpid = (o_pid_t)sp->shm_cpid;
		ods32.shm_nattch = (ushort_t)sp->shm_nattch;
		ods32.shm_cnattch = (ushort_t)sp->shm_cnattch;
		ods32.shm_atime = sp->shm_atime;
		ods32.shm_dtime = sp->shm_dtime;
		ods32.shm_ctime = sp->shm_ctime;

		if (copyout(&ods32, uds, sizeof (ods32)))
			error = EFAULT;
		break;

	case IPC_STAT:
		if (error = ipcaccess(&sp->shm_perm, SHM_R, cr))
			break;

		/*
		 * We set refcnt to 2 in shmget.
		 * It is bumped twice for every attach.
		 */
		mutex_enter(&sp->shm_amp->lock);
		sp->shm_nattch = (sp->shm_amp->refcnt >> 1) - 1;
		mutex_exit(&sp->shm_amp->lock);

		STRUCT_FSET(ds, shm_perm.uid, sp->shm_perm.uid);
		STRUCT_FSET(ds, shm_perm.gid, sp->shm_perm.gid);
		STRUCT_FSET(ds, shm_perm.cuid, sp->shm_perm.cuid);
		STRUCT_FSET(ds, shm_perm.cgid, sp->shm_perm.cgid);
		STRUCT_FSET(ds, shm_perm.mode, sp->shm_perm.mode);
		STRUCT_FSET(ds, shm_perm.seq, sp->shm_perm.seq);
		STRUCT_FSET(ds, shm_perm.key, sp->shm_perm.key);
		STRUCT_FSET(ds, shm_segsz, sp->shm_segsz);
		STRUCT_FSETP(ds, shm_amp, NULL);	/* kernel addr */
		STRUCT_FSET(ds, shm_lkcnt, sp->shm_lkcnt);
		STRUCT_FSET(ds, shm_lpid, sp->shm_lpid);
		STRUCT_FSET(ds, shm_cpid, sp->shm_cpid);
		STRUCT_FSET(ds, shm_nattch, sp->shm_nattch);
		STRUCT_FSET(ds, shm_cnattch, sp->shm_cnattch);
		STRUCT_FSET(ds, shm_atime, sp->shm_atime);
		STRUCT_FSET(ds, shm_dtime, sp->shm_dtime);
		STRUCT_FSET(ds, shm_ctime, sp->shm_ctime);

		if (copyout(STRUCT_BUF(ds), uds, STRUCT_SIZE(ds)))
			error = EFAULT;
		break;

	/* Lock segment in memory */
	case SHM_LOCK:
		if (!suser(cr)) {
			error = EPERM;
			break;
		}
		if (!isspt(sp) && (sp->shm_lkcnt++ == 0)) {
			if (error = shmem_lock(sp->shm_amp)) {
			    mutex_enter(&sp->shm_amp->lock);
			    cmn_err(CE_NOTE,
				"shmctl - couldn't lock %ld pages into memory",
			    sp->shm_amp->size);
			    mutex_exit(&sp->shm_amp->lock);
			    error = ENOMEM;
			    sp->shm_lkcnt--;
			    shmem_unlock(sp->shm_amp, 0);
			}
		}
		break;

	/* Unlock segment */
	case SHM_UNLOCK:
		if (!suser(cr)) {
			error = EPERM;
			break;
		}
		if (!isspt(sp)) {
			if (sp->shm_lkcnt && (--sp->shm_lkcnt == 0)) {
				shmem_unlock(sp->shm_amp, 1);
			}
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	mutex_exit(lock);

	return (error);
}

/*
 * Detach shared memory segment.
 */
/*ARGSUSED1*/
static int
shmdt(caddr_t addr)
{
	proc_t	*pp = curproc;
	int	rc;

	rc = kshmdt(pp, addr);

	return (rc);
}

static int
kshmdt(proc_t *pp, caddr_t addr)
{
	struct shmid_ds	*sp;
	struct anon_map	*amp;
	segacct_t 	*sap, **sapp;
	struct as 	*as;
	struct sptinfo	*sptinfo;
	kmutex_t	*lock;
	size_t		len;
	int		cnt;
	ulong_t		flags;
	int		id;

	/*
	 * Is addr a shared memory segment?
	 */
	mutex_enter(&pp->p_lock);
	prbarrier(pp);		/* block /proc.  See shmgetid(), below. */

	for (sapp = (segacct_t **)&pp->p_segacct; (sap = *sapp) != NULL;
	    sapp = &sap->sa_next)
		if (sap->sa_addr == addr)
			break;
	if (sap == NULL)  {
		mutex_exit(&pp->p_lock);
		return (EINVAL);
	}

	as = sap->sa_sptinfo->sptas;
	sptinfo = sap->sa_sptinfo;
	len = sap->sa_len;
	amp = sap->sa_amp;
	flags = sap->sa_flags;
	id = sap->sa_id;

	*sapp = sap->sa_next;

	kmem_free(sap, sizeof (segacct_t));

	ASSERT(PTOU(pp)->u_nshmseg > 0);
	PTOU(pp)->u_nshmseg--;

	mutex_exit(&pp->p_lock);

	/*
	 * discard lwpchan mappings.
	 */
	mutex_enter(&pp->p_lcp_mutexinitlock);
	if (pp->p_lcp)
		lwpchan_delete_mapping(pp->p_lcp, addr, addr + len);

	(void) as_unmap(pp->p_as, addr, len);

	mutex_exit(&pp->p_lcp_mutexinitlock);

	/*
	 * If this segment was attached intimately, down the per shmid
	 * ISM count.
	 */
	if (flags & SHMSA_ISM) {
		sp = &shmem[id % shminfo.shmmni];
		lock = &shmem_locks[id % shminfo.shmmni];
		mutex_enter(lock);
		if (sp->shm_amp == amp) {
			sp->shm_cnattch--;
		}
		mutex_exit(lock);
	}

	/*
	 * We increment refcnt for every shmat
	 * (and shmfork) and decrement for every
	 * detach (shmdt and shmexit).
	 * If the refcnt is now 1, there are no
	 * more references, and the IPC_RMID has
	 * been done.
	 */
	mutex_enter(&amp->lock);
	cnt = --amp->refcnt;
	mutex_exit(&amp->lock);
	if (cnt == 1) {
		if (as) 	/* isspt(sp) */
			sptdestroy(as, amp);
		if (sptinfo)
			kmem_free(sptinfo, sizeof (sptinfo_t));
		shm_rm_amp(amp, 0);
		return (0);
	}

	/*
	 * Find shmem anon_map ptr in system-wide table.
	 * If not found, IPC_RMID has already been done.
	 */
	for (sp = shmem, lock = shmem_locks; sp < &shmem[shminfo.shmmni];
	    sp++, lock++) {
		mutex_enter(lock);
		if (sp->shm_amp == amp) {
			sp->shm_dtime = hrestime.tv_sec;
			sp->shm_lpid = pp->p_pid;
			mutex_exit(lock);
			break;
		}
		mutex_exit(lock);
	}

	return (0);
}

/*
 * Shmget (create new shmem) system call.
 */
static int
shmget(key_t key, size_t size, int shmflg, uintptr_t *rvp)
{
	struct shmid_ds	*sp;		/* shared memory header ptr */
	size_t		npages; 	/* how many pages */
	int		s;		/* ipcget status */
	int		error = 0;
	int		index;
	kmutex_t	*lock;

	mutex_enter(&shm_lock);
	if (error = ipcget(key, shmflg, (struct ipc_perm *)shmem,
	    shminfo.shmmni, sizeof (*sp), &s, (struct ipc_perm **)&sp)) {
		mutex_exit(&shm_lock);
		return (error);
	}
	index = (int)(sp - shmem);
	lock = &shmem_locks[index];
	mutex_enter(lock);
	mutex_exit(&shm_lock);

	if (s) {

		/*
		 * We don't create this shared memory segment if
		 * address space size of the calling process is
		 * going to be wrapped around.
		 */
		if (curproc->p_as->a_size > (ULONG_MAX - size)) {
			sp->shm_perm.mode = 0;
			mutex_exit(lock);
			return (ENOMEM);
		}

		/*
		 * This is a new shared memory segment.
		 * Allocate an anon_map structure and anon array and
		 * finish initialization.
		 */
		if (size < shminfo.shmmin || size > shminfo.shmmax) {
			sp->shm_perm.mode = 0;
			mutex_exit(lock);
			return (EINVAL);
		}

		/*
		 * Fail if we cannot get anon space.
		 */
		if (anon_resv(size) == 0) {
			sp->shm_perm.mode = 0;
			mutex_exit(lock);
			return (ENOMEM);
		}

		/*
		 * Get number of pages required by this segment (round up).
		 */
		npages = btopr(size);

		sp->shm_amp = (struct anon_map *)
		    kmem_zalloc(sizeof (struct anon_map), KM_SLEEP);
		mutex_enter(&sp->shm_amp->lock);
		sp->shm_amp->ahp = anon_create(npages, ANON_SLEEP);
		sp->shm_amp->swresv = sp->shm_amp->size = ptob(npages);
		/*
		 * We set the refcnt to 2 so that the anon_map
		 * will stay around even if we IPC_RMID
		 * and as_unmap (instead of shmdt) the shm.
		 * In that case we catch this in kshmdt,
		 * and free up the anon_map there.
		 */
		sp->shm_amp->refcnt = 2;
		mutex_exit(&sp->shm_amp->lock);

		/*
		 * Store the original user's requested size, in bytes,
		 * rather than the page-aligned size.  The former is
		 * used for IPC_STAT and shmget() lookups.  The latter
		 * is saved in the anon_map structure and is used for
		 * calls to the vm layer.
		 */

		sp->shm_segsz = size;
		sp->shm_atime = sp->shm_dtime = 0;
		sp->shm_ctime = hrestime.tv_sec;
		sp->shm_lpid = (pid_t)0;
		sp->shm_cpid = curproc->p_pid;
		sp->shm_cnattch = 0;

	} else {
		/*
		 * Found an existing segment.  Check size
		 */
		if (size && size > sp->shm_segsz) {
			mutex_exit(lock);
			return (EINVAL);
		}
	}

#ifdef C2_AUDIT
	if (audit_active) {
		audit_ipcget(AT_IPC_MSG, (void *)sp);
	}
#endif
	*rvp = (uintptr_t)(sp->shm_perm.seq * shminfo.shmmni + (sp - shmem));

	mutex_exit(lock);
	return (0);
}

/*
 * System entry point for shmat, shmctl, shmdt, and shmget system calls.
 */
static uintptr_t
shmsys(int opcode, uintptr_t a0, uintptr_t a1, uintptr_t a2)
{
	int	error;
	uintptr_t r_val = 0;

	switch (opcode) {
	case SHMAT:
		error = shmat((int)a0, (caddr_t)a1, (int)a2, &r_val);
		break;
	case SHMCTL:
		error = shmctl((int)a0, (int)a1, (struct shmid_ds *)a2);
		break;
	case SHMDT:
		error = shmdt((caddr_t)a0);
		break;
	case SHMGET:
		error = shmget((key_t)a0, (size_t)a1, (int)a2, &r_val);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (error)
		return ((uintptr_t)set_errno(error));

	return (r_val);
}

/*
 * add this record to the segacct list.
 */
static void
sa_add(
	struct proc *pp,
	caddr_t addr,
	size_t len,
	struct anon_map *amp,
	struct sptinfo 	*sinfp,
	ulong_t flags,
	int id)
{
	segacct_t *nsap, **sapp;

	nsap = kmem_alloc(sizeof (segacct_t), KM_SLEEP);

	nsap->sa_addr = addr;
	nsap->sa_len  = len;
	nsap->sa_amp  = amp;
	nsap->sa_sptinfo = sinfp;
	nsap->sa_flags = flags;
	nsap->sa_id = id;

	/* add this to the sorted list */
	mutex_enter(&pp->p_lock);
	prbarrier(pp);		/* block /proc.  See shmgetid(), below. */

	sapp = (segacct_t **)&pp->p_segacct;
	while ((*sapp != NULL) && ((*sapp)->sa_addr < addr))
		sapp = &((*sapp)->sa_next);

	ASSERT((*sapp == NULL) || ((*sapp)->sa_addr >= addr));
	nsap->sa_next = *sapp;
	*sapp = nsap;

	PTOU(pp)->u_nshmseg++;
	mutex_exit(&pp->p_lock);
}

/*
 * Duplicate parents segacct records in child.
 */
void
shmfork(struct proc *ppp,	/* parent proc pointer */
	struct proc *cpp)	/* childs proc pointer */
{
	segacct_t *sap;
	struct shmid_ds *sp;
	kmutex_t *mp;

	/*
	 * We are the only lwp running in the parent so nobody can
	 * mess with our p_segacct list.  Thus it is safe to traverse
	 * the list without holding p_lock.  This is essential because
	 * we can't hold p_lock during a KM_SLEEP allocation.
	 */
	PTOU(cpp)->u_nshmseg = 0;
	sap = (segacct_t *)ppp->p_segacct;
	while (sap != NULL) {
		sa_add(cpp, sap->sa_addr, sap->sa_len, sap->sa_amp,
		    sap->sa_sptinfo, sap->sa_flags, sap->sa_id);
		/* increment for every shmat */
		mutex_enter(&sap->sa_amp->lock);
		sap->sa_amp->refcnt++;
		mutex_exit(&sap->sa_amp->lock);
		if (sap->sa_flags & SHMSA_ISM) {
			sp = &shmem[sap->sa_id % shminfo.shmmni];
			mp = &shmem_locks[sap->sa_id % shminfo.shmmni];
			mutex_enter(mp);
			if (sp->shm_amp == sap->sa_amp) {
				sp->shm_cnattch++;
			}
			mutex_exit(mp);
		}
		sap = sap->sa_next;
	}
}

/*
 * Detach shared memory segments from process doing exit.
 */
void
shmexit(struct proc *pp)
{
	/*
	 * We don't need to grap the p_lock here; all other
	 * threads are defunct and this process is exiting
	 * and kshmdt() is proof against /proc.
	 */
	while (pp->p_segacct != NULL)
		(void) kshmdt(pp, ((segacct_t *)pp->p_segacct)->sa_addr);
	ASSERT(PTOU(pp)->u_nshmseg == 0);
}

/*
 * At this time pages should be in memory, so just lock them.
 */

static void
lock_again(size_t npages, struct anon_map *amp)
{
	struct anon *ap;
	struct page *pp;
	struct vnode *vp;
	anoff_t off;
	ulong_t anon_idx;

	mutex_enter(&amp->lock);

	for (anon_idx = 0; npages != 0; anon_idx++, npages--) {
		ap = anon_get_ptr(amp->ahp, anon_idx);
		swap_xlate(ap, &vp, &off);

		pp = page_lookup(vp, (u_offset_t)off, SE_SHARED);
		if (pp == NULL)
			cmn_err(CE_PANIC, "lock_again: page not in the system");
		(void) page_pp_lock(pp, 0, 0);
		page_unlock(pp);
	}
	mutex_exit(&amp->lock);
}

/* check if this segment is already locked. */
/*ARGSUSED*/
static int
check_locked(struct as *as, struct segvn_data *svd, size_t npages)
{
	struct vpage *vpp = svd->vpage;
	size_t i;
	if (svd->vpage == NULL)
		return (0);		/* unlocked */

	SEGVN_LOCK_ENTER(as, &svd->lock, RW_READER);
	for (i = 0; i < npages; i++, vpp++) {
		if (VPP_ISPPLOCK(vpp) == 0) {
			SEGVN_LOCK_EXIT(as, &svd->lock);
			return (1);	/* partially locked */
		}
	}
	SEGVN_LOCK_EXIT(as, &svd->lock);
	return (2);			/* locked */
}



/*
 * Attach the share memory segment to process
 * address space and lock the pages.
 */

static int
shmem_lock(struct anon_map *amp)
{
	size_t npages = btopr(amp->size);
	struct seg *seg;
	struct as *as;
	struct segvn_crargs crargs;
	struct segvn_data *svd;
	proc_t *p = curproc;
	caddr_t addr;
	uint_t lckflag, error, ret;

	as = p->p_as;
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	/* check if shared memory is already attached */
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		svd = (struct segvn_data *)seg->s_data;
		if ((seg->s_ops == &segvn_ops) && (svd->amp == amp) &&
		    (amp->size == seg->s_size)) {
			switch (ret = check_locked(as, svd, npages)) {
			case 0:			/* unlocked */
				AS_LOCK_EXIT(as, &as->a_lock);
				if ((error = as_ctl(as, seg->s_base,
				    seg->s_size, MC_LOCK, 0, 0, NULL, 0)) == 0)
					lock_again(npages, amp);
				(void) as_ctl(as, seg->s_base, seg->s_size,
				    MC_UNLOCK, 0, 0, NULL, NULL);
				return (error);
			case 1:			/* partially locked */
				break;
			case 2:			/* locked */
				AS_LOCK_EXIT(as, &as->a_lock);
				lock_again(npages, amp);
				return (0);
			default:
				cmn_err(CE_WARN, "shmem_lock: deflt %d", ret);
				break;
			}
		}
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* attach shm segment to our address space */

	as_rangelock(as);
	map_addr(&addr, amp->size, 0ll, 1, 0);
	if (addr == NULL) {
		as_rangeunlock(as);
		return (ENOMEM);
	}

	/* Initialize the create arguments and map the segment */
	crargs = *(struct segvn_crargs *)zfod_argsp;	/* structure copy */
	crargs.offset = (u_offset_t)0;
	crargs.type = MAP_SHARED;
	crargs.amp = amp;
	crargs.prot = PROT_ALL;
	crargs.maxprot = crargs.prot;
	crargs.flags = 0;

	mutex_enter(&as->a_contents);
	if (!AS_ISPGLCK(as)) {
		lckflag = 1;
		AS_SETPGLCK(as);
	}
	mutex_exit(&as->a_contents);
	error = as_map(as, addr, amp->size, segvn_create, &crargs);
	as_rangeunlock(as);
	if (!error) {
		lock_again(npages, amp);
		(void) as_unmap(as, addr, amp->size);
	}
	if (lckflag) {
		mutex_enter(&as->a_contents);
		AS_CLRPGLCK(as);
		mutex_exit(&as->a_contents);
	}
	return (error);
}


/* Unlock shared memory */

static void
shmem_unlock(struct anon_map *amp, uint_t lck)
{
	struct anon *ap;
	pgcnt_t npages = btopr(amp->size);
	struct vnode *vp;
	struct page *pp;
	anoff_t off;
	ulong_t anon_idx;

	for (anon_idx = 0; anon_idx < npages; anon_idx++) {

		if ((ap = anon_get_ptr(amp->ahp, anon_idx)) == NULL) {
			if (lck)
				cmn_err(CE_PANIC, "shmem_unlock: null app");
			continue;
		}
		swap_xlate(ap, &vp, &off);
		pp = page_lookup(vp, off, SE_SHARED);
		if (pp == NULL) {
			if (lck)
				cmn_err(CE_PANIC,
				    "shmem_unlock: page not in the system");
			continue;
		}
		if (pp->p_lckcnt) {
			page_pp_unlock(pp, 0, 0);
		}
		page_unlock(pp);
	}
}

/*
 * We call this routine when we have
 * removed all references to this amp.
 * This means all shmdt's and the
 * IPC_RMID have been done.
 */
static void
shm_rm_amp(struct anon_map *amp, uint_t lckflag)
{
	/*
	 * If we are finally deleting the
	 * shared memory, and if no one did
	 * the SHM_UNLOCK, we must do it now.
	 */
	shmem_unlock(amp, lckflag);

	/*
	 * Free up the anon_map.
	 */
	anon_free(amp->ahp, 0, amp->size);
	anon_unresv(amp->swresv);
	anon_release(amp->ahp, btopr(amp->size));
	kmem_free(amp, sizeof (struct anon_map));
}

/*
 * Return the shared memory id for the process's virtual address.
 * Return -1 on failure (addr not within a SysV shared memory segment).
 *
 * shmgetid() is called from code in /proc with the process locked but
 * with pp->p_lock not held.  The address space lock is held, so we
 * cannot grab pp->p_lock here due to lock-ordering constraints.
 * Because of all this, modifications to the p_segacct list must only
 * be made after calling prbarrier() to ensure the process is not locked.
 * See kshmdt() and sa_add(), above.
 */
int
shmgetid(proc_t *pp, caddr_t addr)
{
	segacct_t *sap;
	int shmid = -1;

	ASSERT(MUTEX_NOT_HELD(&pp->p_lock));
	ASSERT(pp->p_flag & SPRLOCK);

	for (sap = (segacct_t *)pp->p_segacct; sap != NULL; sap = sap->sa_next)
		if (sap->sa_addr <= addr && addr < sap->sa_addr + sap->sa_len)
			break;

	if (sap != NULL) {
		/*
		 * Find shmem anon_map ptr in system-wide table.
		 * If not found, IPC_RMID has already been done.
		 */
		struct anon_map *amp = sap->sa_amp;
		struct shmid_ds	*sp;
		for (sp = shmem; sp < &shmem[shminfo.shmmni]; sp++) {
			if (sp->shm_amp == amp) {
				shmid = sp->shm_perm.seq * shminfo.shmmni
					+ (sp - shmem);
				break;
			}
		}
	}

	return (shmid);
}
