/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)winlockio.c	1.43	99/04/14 SMI"

/*
 * This is the lock device driver.
 *
 * We support the following IOCTL's:
 *
 *   GRABPAGEALLOC:
 *	Compatibility with cgsix device driver lockpage ioctls.	 Lockpages
 *	created this way must be an entire page for compatibility with
 *	older software.	 This ioctl allocates a lock context with its own
 *	private lock page.  The unique "cookie" that identifies this lock is
 *	returned.
 *
 *   GRABPAGEFREE:
 *	Compatibility with cgsix device driver lockpage ioctls.	 This
 *	ioctl releases the lock context allocated by GRABPAGEALLOC.
 *
 *   GRABLOCKINFO:
 *	Returns a one-word flag.  '1' means that multiple clients may
 *	access this lock page.	Older device drivers returned '0',
 *	meaning that only two clients could access a lock page.
 *
 *   GRABATTACH:
 *	Not supported.	This ioctl would have grabbed all lock pages
 *	on behalf of the calling program.
 *
 *   WINLOCKALLOC:
 *	Allocate a lock context.  This ioctl accepts a key value.  as
 *	its argument.  If the key is zero, a new lock context is
 *	created, and its "cookie" is returned.	If the key is nonzero,
 *	all existing contexts are checked to see if they match they
 *	key.  If a match is found, its reference count is incremented
 *	and its cookie is returned, otherwise a new context is created
 *	and its cookie is returned.
 *
 *   WINLOCKFREE:
 *	Free a lock context.  This ioctl accepts the cookie of a lock
 *	context and decrements its reference count.  Once the reference
 *	count reaches zero *and* all mappings are released, the lock
 *	context is freed.  When the last context in a lock page is
 *	freed, that lock page is freed as well.
 *
 *   WINLOCKSETTIMEOUT:
 *	Set lock timeout for a context.	 This ioctl accepts the cookie
 *	of a lock context and a timeout value in milliseconds.
 *	Whenever a lock contention occurs, the lock is broken after
 *	the timeout expires.  If the timeout value is zero, locks do
 *	not timeout.  This value will be rounded to the nearest clock
 *	tick, so don't try to use it for real-time control or something.
 *
 *   WINLOCKGETTIMEOUT:
 *	Get lock timeout from a context.
 *
 *   WINLOCKDUMP:
 *	Dump state of this device.
 *
 *
 * How /dev/lock works:
 *
 *   Every lock context consists of two mappings of the client to the lock
 *   page.  These mappings are known as the "lock page" and "unlock page"
 *   to the client.  Only one process at a time is allowed to have a valid
 *   mapping to a lock page.  A client acquires a lock by writing a '1' to
 *   the lock page.  If it does not already have a valid mapping to that
 *   page, the segment driver takes a fault, validates the client mapping
 *   and allows the client to continue.	 The client releases the lock by
 *   writing a '0' to the unlock page.	Again, if it does not have a valid
 *   mapping to the unlock page, the segment driver takes a fault,
 *   validates the mapping, and lets the client continue.  From this point
 *   forward, the client can make as many locks and unlocks as it
 *   wants, without any more attention from the kernel.
 *
 *   If a different process wants to acquire a lock, it takes a page fault
 *   when it writes the '1' to the lock page.  If the segment driver sees
 *   that the lock page contained a zero, then it invalidates the first
 *   client's mapping and continues as above.
 *
 *   If there is already a '1' in the lock page when the second client
 *   tries to access the lock page, then a lock exists.	 The segment
 *   driver sleeps the second client and, if applicable, starts the
 *   timeout on the lock.  The first client's mapping to the unlock page
 *   is invalidated so that the driver will be woken again when the first
 *   client releases the lock.
 *
 *   When the locking client finally writes a '0' to the unlock page, the
 *   segment driver takes another fault.  The client is given a valid
 *   mapping, not to the unlock page, but to the "trash page", and allowed
 *   to continue.  Meanwhile, the sleeping client is given a valid mapping
 *   to the lock page and allowed to continue as well.
 *
 *   A process is allowed to have multiple mappings to the lock page.
 *   This prevents deadlock.
 *
 *
 */

#include <sys/types.h>		/* various type defn's */
#include <sys/debug.h>
#include <sys/param.h>		/* various kernel limits */
#include <sys/time.h>
#include <sys/buf.h>		/* defines buf struct  XXX */
#include <sys/errno.h>
#include <sys/systm.h>		/* defines system stuff  XXX */
#include <sys/kmem.h>		/* defines kmem_alloc() */
#include <sys/conf.h>		/* defines cdevsw */
#include <sys/autoconf.h>	/* defines SP_* constants */
#include <sys/file.h>		/* various file modes, etc. */
#include <sys/uio.h>		/* UIO stuff  XXX */
#include <sys/ioctl.h>
#include <sys/proc.h>		/* defines proc struct */
#include <sys/user.h>		/* defines user struct */
#include <sys/cred.h>		/* defines cred struct */
#include <sys/mman.h>		/* defines mmap(2) parameters */
#include <sys/open.h>		/* defines arguments passed to xx_open/close */
#include <sys/stat.h>		/* defines S_IFCHR */
#include <sys/cmn_err.h>	/* use $!@# cmn_err instead of printf */
#include <sys/vnode.h>		/* defines vnodes */
#include <vm/as.h>		/* defines address-space struct */
#include <vm/seg.h>		/* defines seg struct */
#include <vm/hat.h>		/* defines hat layer */
#include <sys/ddi.h>		/* ddi stuff  XXX */
#include <sys/sunddi.h>		/* ddi stuff  XXX */
#include <sys/ddi_impldefs.h>	/* ???  XXX */
#include <sys/fs/snode.h>	/* ???  XXX */
#include <sys/winlockio.h>	/* defines ioctls */
#include <sys/seg_drv.h>	/* segment driver common code */

#include <vm/page.h>

#define	_STMT(op)	do { op } while (0)

#if LOCKDEBUG >= 1
#define	assert(x) ((x) ? 0 : \
	(cmn_err(CE_WARN, "winlock: YAK! assertion failed \"x\" [line %d]\n", \
	__LINE__), 0))
#else
#define	assert(x)
#endif

#if LOCKDEBUG >= 2
static	int	lock_debug = 0;
/* CSTYLED */
#define	DEBUGF(level, args)	_STMT(if (lock_debug >= (level)) cmn_err args;)
#else
#define	DEBUGF(level, args)
#endif

#define	ID(x)	((x) && (x)->procp ? x->procp->p_pid : -1)
#define	LOCK(lp)	(*(lp)->lockptr)

#define	UNLINK_LIST(start, ptr)	\
	(unlink_list((Randobj *)&(start), (Randobj *)(ptr)))

#define	LOCK_OFFBASE	(0)
#define	MAX_LOCKS	(pagesize/sizeof (int))

#define	LOCKTIME	3	/* seconds */

/*
 * Returns a pointer to the current proc structure.
 */
static struct proc *
getcurproc(void)
{
	struct proc *p;

	if (drv_getparm(UPROCP, &p) == -1)
		return (0);
	return (p);
}

/*
 * These structures describe a lock context.  We now permit multiple
 * clients (not just two) to access a lock page
 *
 * Note: the "next" pointer in these structures must be first.
 *
 * The "cookie" identifies the lock context.  It contains several
 * pieces of information.  The high-order bits are the "offset" that
 * must be provided to mmap(2).  The low-order bits are the byte offset
 * from the beginning of the mapped page to the lock word.  For
 * oldstyle lock pages, the cookie is the address of the private lock
 * page, with no byte offset.  For new style locks, the cookie is the
 * address of the public lock page plus the byte offset.  Note that in
 * both cases, the cookie is the pointer to the lock word.
 */

/*
 * per-process information about locks.  This is the client field of
 * a seg_drv segment.  Note that *two* segments point to this.
 */

typedef struct segproc {
	struct segproc	*next;		/* next client of this lock */
	struct seglock	*lp;		/* associated lock context */
	struct	proc	*procp;		/* proc info */
	caddr_t		lockaddr;	/* TODO: lose this? */
	struct	seg	*locksegp;	/* lock mapping, if any */
	caddr_t		unlockaddr;	/* TODO: lose this? */
	struct	seg	*unlocksegp;	/* unlock mapping, if any */
	struct vnode	*vp;		/* vnode associated with device */
	dev_t		dev;		/* device */
	u_int		flag;		/* see "flag bits" in winlockio.h */
	u_char		prot;		/* current segment prot */
	u_char		maxprot;	/* maximum segment protections */
} SegProc;

/* per-lock-context information */

typedef struct seglock {
	struct seglock	*next;		/* next context */
	int		sleepers;	/* nthreads sleeping on this lock */
	int		alloccount;	/* how many times created? */
	int		mapcount;	/* how many mappings to this page? */
	u_int		cookie;		/* offset into device (cookie) */
	u_int		key;		/* key, if any */
	caddr_t		page;		/* pointer to lock page */
	int		*lockptr;	/* kernel virtual addr of lock */
	clock_t		timeout;	/* sleep time in ticks */
	struct segproc	*clients;	/* list of clients of this lock */
	struct segproc	*owner;		/* current owner of lock */
	kmutex_t	mutex;		/* mutex lock */
	kcondvar_t	locksleep;	/* for sleeping on lock */
} SegLock;

extern int segdrv_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp,
    off_t len, u_int prot, u_int maxprot, u_int flags, struct cred *cred,
    caddr_t client, struct seg_ops *client_ops, int (*client_create)());

typedef	struct randobj { struct randobj *next; } Randobj;


	/*
	 * The trash page is where unwanted writes go
	 * when a process is releasing a lock.
	 */

static	caddr_t trashpage = NULL;	/* for trashing unwanted writes */
static	caddr_t	lockpage = NULL;	/* bunch o' locks */


static	dev_info_t	*winlock_dip;

static	kmutex_t	winlock_mutex;	/* to lock linked list of locks */
static	int	winlock_busy = 0;	/* # open's */
static	u_long	pagesize, pageoffset, pagemask;

	/*
	 * The allocation code tries to allocate from lock_free_list
	 * first, otherwise it uses kmem_zalloc.  On final close, all
	 * locks in lock_free_list are returned to kernel
	 */
static	SegLock	*lock_list = NULL;		/* in-use locks */
static	SegLock	*lock_free_list = NULL;		/* free locks */
static	SegLock	*seglock_findlock();		/* find a lock in lock_list */
static	int	next_lock = 0;			/* next lock cookie */

static	void	winlock_lockmap(struct seg *, caddr_t, caddr_t);
static	void	winlock_unlockmap(struct seg *);

static int give_mapping(SegLock *lp, SegProc *pp);
static SegProc *seglock_findclient(SegLock *lp);
static int seglock_graballoc(intptr_t arg, int oldstyle, int mode);
static int seglock_grabinfo(intptr_t arg, int mode);
static void unlink_list(Randobj *first, Randobj *ptr);
static SegLock *seglock_createlock(int oldstyle);
static SegLock *seglock_findkey(u_int key);
static void seglock_destroylock(SegLock *lp);
static void lock_wakeup(SegLock *lp, int trash);
static void dump_all(void);
static void lock_destroyall(void);
static	faultcode_t
seglock_lockfault(struct seg *seg, caddr_t addr, SegProc *sdp, SegLock *lp);
static void seglock_dump_all(void);
static int seglock_grabfree(intptr_t arg, int mode);
static int seglock_gettimeout(intptr_t arg, int mode);
static int seglock_settimeout(intptr_t arg, int mode);
static void seglock_deleteclient(SegLock *lp, SegProc *pp);
static int seglock_create(struct seg *seg, caddr_t argsp);

static int seglock_dup(struct seg *seg, struct seg *newseg);
static void seglock_free(struct seg *seg);
static faultcode_t seglock_fault(struct hat *hat, struct seg *seg,
		    caddr_t addr, size_t len,
		    enum fault_type type, enum seg_rw rw);
static int seglock_pagelock(struct seg *seg, caddr_t addr, size_t len,
		    struct page ***ppp, enum lock_type type, enum seg_rw rw);
static int seglock_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp);

struct	seg_ops seglock_ops = {
	seglock_dup,	/* dup */
	NULL,		/* unmap */
	seglock_free,	/* free */
	seglock_fault,	/* fault */
	NULL,		/* faulta */
	NULL,		/* setprot */
	NULL,		/* checkprot */
	NULL,		/* kluster */
	(size_t (*)()) NULL,	/* swapout */
	NULL,		/* sync */
	NULL,		/* incore */
	NULL,		/* lockop */
	NULL,		/* getprot */
	NULL,		/* getoffset */
	NULL,		/* gettype */
	NULL,		/* getvp */
	NULL,		/* advise */
	NULL,		/* dump */
	seglock_pagelock,	/* pagelock */
	seglock_getmemid,	/* getmemid */
};

/*ARGSUSED*/
static	int
winlock_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	DEBUGF(1, (CE_CONT, "winlock_attach, devi=%p, cmd=%d, mem used=%d\n",
		devi, (int)cmd, total_memory));
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	winlock_dip = devi;
	if (ddi_create_minor_node(devi, "winlock", S_IFCHR, 0, DDI_PSEUDO, 0)
	    == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		DEBUGF(1, (CE_CONT,
		    "winlock_attach, create_minor_node failed\n"));
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}


/*ARGSUSED*/
static	int
winlock_open(dev_t *dev, int flag, int otyp, cred_t *cred)
{
	DEBUGF(2, (CE_CONT, "winlock_open, flag=%x, mem used=%d\n",
		flag, total_memory));

	/* make sure we don't get called during _fini */
	/* TODO: nuke this? */
	mutex_enter(&winlock_mutex);
	++winlock_busy;
	mutex_exit(&winlock_mutex);

	return (0);
}

/*ARGSUSED*/
int
winlock_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	SegLock	*lp, *next;

	DEBUGF(2, (CE_CONT, "\nwinlock_close(%d), mem used=%d\n",
		getminor(dev), total_memory));

	/* Grab driver-wide lock.  TODO: nuke this? */
	mutex_enter(&winlock_mutex);

	/* implicitly execute a free on all contexts */

	for (lp = lock_list; lp != NULL; lp = next) {
		next = lp->next;
		lp->alloccount = 0;
		if (lp->mapcount <= 0)
			seglock_destroylock(lp);
	}

#if LOCKDEBUG >= 2
	if (lock_debug >= 3)
		dump_all();
#endif
	winlock_busy = 0;
	mutex_exit(&winlock_mutex);
	return (0);
}


/* TODO: Nuke this? */
static	int
winlock_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t dev = (dev_t)arg;
	int instance, error;

#if defined(lint)
	dip = dip;
#endif
	if ((instance = getminor(dev)) >= 1)
		return (DDI_FAILURE);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *) winlock_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}


/*ARGSUSED*/
int
winlock_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *cred, int *rval)
{
	DEBUGF(3, (CE_CONT, "winlockioctl: cmd=%d, arg=0x%p\n", cmd, arg));

	switch (cmd) {
	/*
	 * ioctls that used to be handled by /dev/cgsix.
	 * Need to emulate cgsix behavior.
	 */

	case GRABPAGEALLOC:
		return (seglock_graballoc(arg, 1, mode));
	case GRABLOCKINFO:
		return (seglock_grabinfo(arg, mode));
	case GRABATTACH:
		return (EINVAL); /* GRABATTACH is not supported (never was) */

	case WINLOCKALLOC:
		return (seglock_graballoc(arg, 0, mode));
	case WINLOCKFREE:
	case GRABPAGEFREE:
		return (seglock_grabfree(arg, mode));
	case WINLOCKSETTIMEOUT:
		return (seglock_settimeout(arg, mode));
	case WINLOCKGETTIMEOUT:
		return (seglock_gettimeout(arg, mode));
	case WINLOCKDUMP:
		seglock_dump_all();
		return (0);

#if LOCKDEBUG >= 2
	case 255:
		lock_debug = arg;
		return (0);
#endif

	default:
		return (ENOTTY);
	}
}


/*ARGSUSED*/
int
winlock_segmap(
	dev_t	dev,		/* major:minor */
	off_t	off,		/* cookie from mmap(2) */
	struct as *as,		/* user's address space. */
	caddr_t	*addr,		/* address from mmap(2) */
	off_t	len,		/* length from mmap(2) */
	u_int	prot,		/* user wants this access */
	u_int	maxprot,	/* this is the maximum the user can have */
	u_int	flags,		/* flags from mmap(2) */
	cred_t	*cred)
{
	struct segproc *sdp;
	SegLock *seglock;

	DEBUGF(3, (CE_CONT,
	    "\nwinlock_segmap: dev=%x,off=%x,as=%p,addr=%p,len=%x,flags=%x\n",
	    dev, (u_int)off, as, addr, len, flags));

	if ((u_long)off > (MAX_LOCKS * pagesize)) {
		DEBUGF(1, (CE_CONT, "winlock_segmap: as=%p,*addr=%p,"
			"len=%x,sdp=%p\n", as, *addr, (u_int)len, sdp));
		return (ENXIO);
	}

	if ((seglock = seglock_findlock((u_int)off)) != NULL) {
		if (len != pagesize)	/* TODO: do we need to check this? */
			return (EINVAL);
	} else
		return (ENXIO);

	mutex_enter(&seglock->mutex);

	sdp = seglock_findclient(seglock);
	++seglock->mapcount;

	/* Now, create the mapping in the user's address space. */

	DEBUGF(3, (CE_CONT, "winlock_segmap: as=%p,*addr=%p,len=%x,sdp=%p\n",
	    as, *addr, (u_int)len, sdp));

	mutex_exit(&seglock->mutex);
	return (segdrv_segmap(dev, off, as, addr, len, prot, maxprot,
	    flags, cred, (caddr_t)sdp, &seglock_ops, seglock_create));
}

/*ARGSUSED1*/
static	int
seglock_create(struct seg *seg, caddr_t argsp)
{
	Segdrv_Data *segdat = (Segdrv_Data *)seg->s_data;
	SegProc *sdp = (SegProc *)segdat->client;

	DEBUGF(3, (CE_CONT, "seglock_create: seg:%p, argsp:%p\n", seg, argsp));

	/*
	 * Note: if we're called from unmap due to a split segment, both
	 * new segments will have the same client pointer.  Actually, this
	 * should never happen anyway since all lockpage mappings are only
	 * one page.
	 */

#ifdef	LOCKDEBUG
	if (argsp != NULL) {	/* called from unmap */
		printf("seglock_create called from unmap()\n");
	}
#endif	/* LOCKDEBUG */


	if (sdp->locksegp == NULL) {
		sdp->locksegp = seg;
		sdp->lockaddr = seg->s_base;	/* TODO: nuke this? */
	} else
	if (sdp->unlocksegp == NULL) {
		sdp->unlocksegp = seg;
		sdp->unlockaddr = seg->s_base;	/* TODO: nuke this? */
	} else {
		DEBUGF(1, (CE_CONT,
		    "winlock: YAK! attempt to map a lock context thrice\n"));
		return (ENOMEM);
	}

	return (SEGDRV_CONTINUE);
}

/*
 * duplicate a segment, as in fork()
 * The results of trying to use lock pages after a fork are undefined.
 * In fact, the results of unmapping lock pages after a fork are probably
 * also undefined.  TODO: look into this.
 */

static int
seglock_dup(struct seg *seg, struct seg *newseg)
{
	Segdrv_Data *segdat = (Segdrv_Data *)seg->s_data;
	SegProc *ndp, *sdp = (SegProc *)segdat->client;
	SegLock *lp = sdp->lp;

	DEBUGF(3, (CE_CONT, "\nseglock_dup: seg=%p, newseg=%p, sdp=%p, lp=%p\n",
		seg, newseg, sdp, lp));

	/*
	 * can't use seglock_findclient() since newseg belongs to a process
	 * whose process id hasn't been assigned yet.  Instead, we make
	 * this an orphan segment by allocating a SegProc structure for
	 * it and setting everything to NULL.  The first time there's a
	 * fault on this page, it will be matched up with its parent.
	 */

	mutex_enter(&lp->mutex);
	ndp = kmem_zalloc(sizeof (SegProc), KM_SLEEP);
	segdat = (Segdrv_Data *)newseg->s_data;
	segdat->client = (caddr_t)ndp;
	ndp->lp = lp;
	++lp->mapcount;
	mutex_exit(&lp->mutex);
	return (SEGDRV_CONTINUE);
}


static void
seglock_free(struct seg *seg)
{
	Segdrv_Data	*segdat = (Segdrv_Data *)seg->s_data;
	SegProc	*sdp = (SegProc *)segdat->client;
	SegLock	*lp = sdp->lp;

	DEBUGF(3, (CE_CONT, "\nseglock_free: seg:%p,pid:%d\n",
		seg, getcurproc()->p_pid));
	assert(lp != NULL && sdp != NULL);
	DEBUGF(4, (CE_CONT, "  sdp = %p, lp=%p, lp->owner = %p\n",
		sdp, lp, lp->owner));

	mutex_enter(&lp->mutex);

	/* make sure this process doesn't own any locks */
	if (sdp == lp->owner)
		lock_wakeup(lp, 0);

	if (sdp->procp == NULL) {		/* orphan page */
		DEBUGF(3, (CE_CONT, "  free orphan page\n"));
		kmem_free(sdp, sizeof (*sdp));
	} else {
		if (sdp->locksegp == seg) {
			sdp->locksegp = NULL;
		} else
			if (sdp->unlocksegp == seg) {
				sdp->unlocksegp = NULL;
			} else {
				/*EMPTY*/
				DEBUGF(1, (CE_CONT, "winlock: "
				    "Yak! seg %p not found in sdp %p\n",
				    seg, sdp));
			}

			/* see if this finishes the client structure */
			if (sdp->locksegp == NULL && sdp->unlocksegp == NULL)
				seglock_deleteclient(lp, sdp);
	}

	/* see if this finished the entire lock context */

	if (--lp->mapcount <= 0 && lp->alloccount <= 0) {
		mutex_enter(&winlock_mutex);
		mutex_exit(&lp->mutex);
		seglock_destroylock(lp);
		mutex_exit(&winlock_mutex);
	} else
		mutex_exit(&lp->mutex);
}

/*ARGSUSED*/
static	faultcode_t
seglock_fault(
	struct hat *hat,
	struct seg *seg,
	caddr_t addr,
	size_t len,
	enum fault_type type,
	enum seg_rw rw)
{
	Segdrv_Data	*segdat;
	SegProc	*sdp;
	SegLock	*lp;

	DEBUGF(3, (CE_CONT,
	    "\nseglock_fault: seg = %p, addr = %p, len=%x, type=%d, rw=%d\n",
	    seg, addr, (u_int)len, (int)type, (int)rw));

	if (type == F_PROT || type == F_SOFTLOCK || type == F_SOFTUNLOCK)
		return (SEGDRV_CONTINUE);

	assert(type == F_INVAL);

top:

	segdat = (Segdrv_Data *)seg->s_data;
	sdp = (SegProc *)segdat->client;
	lp = sdp->lp;

	mutex_enter(&lp->mutex);
	if (seglock_lockfault(seg, addr, sdp, lp) != 0) {
		/*
		 * The only case where the return value is non-zero is when an
		 * attempt to obtain the 'as' lock of the thread (not the
		 * current thread whose fault is being resolved here) whose
		 * mapping's have to be unloaded, fails.
		 * We drop the mutex and retry the fault handling
		 */
		mutex_exit(&lp->mutex);
		delay(hz/8);
		goto top;
	}
	mutex_exit(&lp->mutex);
	return (SEGDRV_HANDLED);
}

/*ARGSUSED*/
static int
seglock_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
seglock_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}

	/* INTERNAL ROUTINES START HERE */



static caddr_t
winlock_getpages(size_t npages, u_int flag)
{
	caddr_t	p1;

	p1 = kmem_zalloc(ptob(npages), flag);
	ASSERT(p1 != NULL && !((uintptr_t)p1 & pageoffset));
	return (p1);
}


static void
winlock_freepages(void *ptr, size_t size)
{
	assert(ptr != NULL && !((uintptr_t)ptr & pageoffset));
	kmem_free(ptr, ptob(size));
}


/*
 * search the lock_list list for the specified cookie (mt-safe)
 */
static SegLock *
seglock_findlock(u_int cookie)
{
	SegLock	*lp;

	mutex_enter(&winlock_mutex);
	cookie &= (u_int)pagemask;
	for (lp = lock_list; lp != NULL; lp = lp->next) {
		if (cookie == lp->cookie) {
			mutex_exit(&winlock_mutex);
			return (lp);
		}
	}
	mutex_exit(&winlock_mutex);
	return ((struct seglock *)NULL);
}

/*
 * search the lock_list list for the specified key
 */
static SegLock *
seglock_findkey(u_int key)
{
	SegLock	*lp;

	for (lp = lock_list; lp != NULL; lp = lp->next)
		if (key == lp->key)
			return (lp);
	return ((struct seglock *)NULL);
}


/* search for an item in a linked list and remove it from that list */

static void
unlink_list(Randobj *first, Randobj *ptr)
{
	for (; first != NULL; first = first->next)
		if (first->next == ptr) {
			first->next = ptr->next;
			return;
		}
}


/*
 * Create a new lock context
 */

static SegLock *
seglock_createlock(int oldstyle)
{
	SegLock	*lp;

	DEBUGF(3, (CE_CONT, "seglock_createlock: free_list=%p\n",
		lock_free_list));

	/*
	 * 64b port changes:
	 *
	 * Do not return a kernel address as a cookie. Instead return
	 * (next_lock + 1) * pagesize as page id part of the cookie.
	 *
	 * Allow oldstyle to use lock_free_list just like for
	 * newstyle to avoid running out of cookies before we actually
	 * have more than MAX_LOCKS locks.
	 *
	 * --aguzovsk.
	 */

	if (lock_free_list != NULL) {
		lp = lock_free_list;
		lock_free_list = lp->next;
		if (oldstyle) {
			lp->page = winlock_getpages((size_t)1, KM_SLEEP);
			lp->lockptr = (int *)lp->page;
		} else if (lp->page != lockpage) {
			lp->page = lockpage;
			lp->lockptr = (int *)lockpage +
				(lp->cookie / pagesize) - 1;
		}
	} else if (next_lock >= MAX_LOCKS) {
		return (NULL);
	} else {
		lp = kmem_zalloc(sizeof (SegLock), KM_SLEEP);
		if (oldstyle) {
			DEBUGF(4, (CE_CONT,
				"  allocating oldstyle context at %p\n", lp));
			lp->page = winlock_getpages((size_t)1, KM_SLEEP);
			lp->lockptr = (int *)lp->page;
			lp->cookie = (next_lock + 1) * (u_int)pagesize;
			DEBUGF(4, (CE_CONT, "  allocating oldstyle page at"
				" %p\n", lp->page));
		} else {
			lp->cookie = (next_lock + 1) * (u_int)pagesize;
			lp->page = lockpage;
			lp->lockptr = (int *)lockpage + next_lock;
		}
		++next_lock;
	}

	lp->sleepers = 0;
	lp->alloccount = 1;
	lp->mapcount = 0;
	lp->timeout = LOCKTIME*hz;
	lp->clients = NULL;
	lp->owner = NULL;
	LOCK(lp) = 0;
	mutex_init(&lp->mutex, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&lp->locksleep, NULL, CV_DEFAULT, NULL);
	lp->next = lock_list;
	lock_list = lp;

	DEBUGF(3, (CE_CONT, "seglock_createlock done lp=%p\n", lp));
	return (lp);
}


static void
seglock_destroylock(SegLock *lp)
{
	DEBUGF(3, (CE_CONT, "seglock_destroylock: lp=%p\n", lp));

	{
		SegProc	*pp, *next;

		for (pp = lp->clients; pp != NULL; pp = next) {
			next = pp->next;
			assert(pp->locksegp == NULL && pp->unlocksegp == NULL);
			kmem_free(pp, sizeof (SegProc));
		}
	}

	UNLINK_LIST(lock_list, lp);

	if (lp->page != lockpage) {	/* old-style GRABPAGEALLOC */
		DEBUGF(4, (CE_CONT,
		    "  seglock_destroylock returns oldstyle page at %p\n",
		    lp->page));
		winlock_freepages(lp->page, (size_t)1);
		DEBUGF(4, (CE_CONT,
		    "  seglock_destroylock returns oldstyle context at %p\n",
		    lp));
		lp->next = lock_free_list;
		lock_free_list = lp;
		mutex_destroy(&lp->mutex);
		cv_destroy(&lp->locksleep);
	} else {
		lp->next = lock_free_list;
		lock_free_list = lp;
		mutex_destroy(&lp->mutex);
		cv_destroy(&lp->locksleep);
	}

	if (lock_list == NULL)
		lock_destroyall();

	DEBUGF(3, (CE_CONT, "  seglock_destroylock done\n"));
}



/*
 * search a context's client list for the current process.
 * create an entry if neccessary.
 */

static SegProc *
seglock_findclient(SegLock *lp)
{
	SegProc *pp;

	DEBUGF(3, (CE_CONT, "seglock_findclient: lp=%p\n", lp));

	for (pp = lp->clients; pp != NULL; pp = pp->next)
		if (pp->procp == getcurproc())
			return (pp);

	/* didn't find it, better create it. */
	pp = kmem_zalloc(sizeof (SegProc), KM_SLEEP);
	pp->next = lp->clients;
	lp->clients = pp;
	pp->lp = lp;
	pp->procp = getcurproc();

	DEBUGF(3, (CE_CONT, "  seglock_findclient creates pp=%p\n", pp));

	return (pp);
}

/*
 * search a context's client list for the given client and delete
 */

static void
seglock_deleteclient(SegLock *lp, SegProc *pp)
{
	UNLINK_LIST(lp->clients, pp);

	kmem_free(pp, sizeof (SegProc));
}


	/* IOCTLS START HERE */


static int
seglock_grabinfo(intptr_t arg, int mode)
{
	int i = 1;

	/* multilock now supported */
	(void) ddi_copyout((caddr_t)&i, (caddr_t)arg, sizeof (i), mode);
	return (0);
}



static int
seglock_graballoc(intptr_t arg, int oldstyle, int mode)	/* IOCTL */
{
#if LOCKDEBUG
	int i;
#endif
	struct seglock	*lp = lock_list;
	u_int		key;
	struct		winlockalloc wla;

	DEBUGF(3, (CE_CONT,
		"seglock_graballoc: arg=%p, style=%d\n", arg, oldstyle));

	if (oldstyle) {
		key = 0;
	} else {
		if (ddi_copyin((caddr_t)arg, (caddr_t)&wla, sizeof (wla),
		    mode)) {
			return (EFAULT);
		}
		key = wla.sy_key;
	}


	/* for redirecting unwanted writes */
	mutex_enter(&winlock_mutex);
	if (trashpage == NULL) {
		lockpage = (caddr_t)winlock_getpages((size_t)2, KM_SLEEP);
		trashpage = lockpage + pagesize;
		DEBUGF(4,
		    (CE_CONT, "  allocating lock pages at %p\n", lockpage));
#if LOCKDEBUG
		for (i = 0; i < pagesize/sizeof (int); i++)
			*((int *)lockpage + i) = i;
#endif
	}

	/* is there a key?  Does it match an existing lock? */

	if (key != 0 && (lp = seglock_findkey(key)) != NULL)
		++lp->alloccount;
	else if ((lp = seglock_createlock(oldstyle)) != NULL)
		lp->key = key;
	else {
		mutex_exit(&winlock_mutex);
		return (ENOMEM);
	}


	if (oldstyle) {
		(void) ddi_copyout((caddr_t)&lp->cookie, (caddr_t)arg,
			sizeof (lp->cookie), mode);
	} else {
		wla.sy_ident = lp->cookie +
		    (u_int)((uintptr_t)lp->lockptr & pageoffset);
		(void) ddi_copyout((caddr_t)&wla, (caddr_t)arg,
		    sizeof (wla), mode);
	}

	mutex_exit(&winlock_mutex);
	return (0);
}



static int
seglock_grabfree(intptr_t arg, int mode)	/* IOCTL */
{
	struct seglock	*lp;
	u_int	cookie;

	DEBUGF(3, (CE_CONT, "seglock_grabfree: arg=%p\n", arg));

	if (ddi_copyin((caddr_t)arg, &cookie, sizeof (cookie), mode)
	    != 0) {
		return (EFAULT);
	}
	if ((lp = seglock_findlock(cookie)) == NULL)
		return (EINVAL);

	mutex_enter(&lp->mutex);
	if (--lp->alloccount <= 0 && lp->mapcount <= 0) {
		mutex_enter(&winlock_mutex);
		mutex_exit(&lp->mutex);
		seglock_destroylock(lp);
		mutex_exit(&winlock_mutex);
	} else
		mutex_exit(&lp->mutex);

	return (0);
}

static int
seglock_settimeout(intptr_t arg, int mode)	/* IOCTL */
{
	SegLock		*lp;
	SegProc		*pp;
	struct winlocktimeout		wlt;

	if (ddi_copyin((caddr_t)arg, &wlt, sizeof (wlt), mode) != 0) {
		return (EFAULT);
	}

	if ((lp = seglock_findlock(wlt.sy_ident)) == NULL)
		return (EINVAL);

	mutex_enter(&lp->mutex);
	lp->timeout = MSEC_TO_TICK_ROUNDUP(wlt.sy_timeout);
	pp = seglock_findclient(lp);
	pp->flag = pp->flag & KFLAGS | wlt.sy_flags & UFLAGS;
	mutex_exit(&lp->mutex);

	return (0);
}

static int
seglock_gettimeout(intptr_t arg, int mode)
{
	SegLock		*lp;
	struct winlocktimeout		wlt;

	if (ddi_copyin((caddr_t)arg, &wlt, sizeof (wlt), mode) != 0)
		return (EFAULT);

	if ((lp = seglock_findlock(wlt.sy_ident)) == NULL)
		return (EINVAL);

	wlt.sy_timeout = TICK_TO_MSEC(lp->timeout);
	wlt.sy_flags = seglock_findclient(lp)->flag & UFLAGS;

	(void) ddi_copyout(&wlt, (caddr_t)arg, sizeof (wlt), mode);

	return (0);
}

static void
seglock_dump_all(void)
{
	mutex_enter(&winlock_mutex);
	dump_all();
	mutex_exit(&winlock_mutex);
}




#define	_LOCKMAP(sp, addr, pgaddr)	winlock_lockmap(sp, addr, pgaddr)
#define	_LOCKUNMAP(sp)			winlock_unlockmap(sp)


/*
 * Handle a lock time-out.  This is the BOTTOM half of the driver,
 * be careful.  If either the lock's locksegp or unlocksegp is non-NULL,
 * the corresponding as lock must be held.
 */
static void
seglock_timeout(struct seglock *lp)
{
	SegProc *owner = lp->owner;
	struct seg *locksegp = owner->locksegp, *unlocksegp = owner->unlocksegp;

	ASSERT(MUTEX_HELD(&lp->mutex));
	ASSERT(owner != NULL);
	ASSERT(locksegp == NULL || unlocksegp == NULL ||
	    locksegp->s_as == unlocksegp->s_as);

	ASSERT(locksegp == NULL ||
	    AS_LOCK_HELD(locksegp->s_as, &locksegp->s_as->a_lock));
	ASSERT(unlocksegp == NULL ||
	    AS_LOCK_HELD(unlocksegp->s_as, &unlocksegp->s_as->a_lock));

	cmn_err(CE_CONT,
	    "winlock: lock timed out on process %d\n", ID(lp->owner));

	lock_wakeup(lp, 1);	/* locking process now loses the lock page */
}

/*
 * winlock_lockmap()
 */
static void
winlock_lockmap(struct seg *seg, caddr_t addr, caddr_t pgaddr)
{
	pfn_t pfnum;
	page_t *pp;

	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	pfnum = hat_getkpfnum(pgaddr);
	/*
	 * TODO: The pfn step should not be necessary - need
	 * a hat_getpp() function.
	 */
	pp = page_numtopp_nolock(pfnum);
	ASSERT(pp != NULL ? PAGE_LOCKED(pp) : 1);

	ASSERT(pp != NULL);

	(void) hat_memload(seg->s_as->a_hat, addr, pp,
		PROT_READ|PROT_WRITE|PROT_USER, 0);
}

/*
 * winlock_unlockmap()
 */
static void
winlock_unlockmap(struct seg *seg)
{
	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	(void) hat_unload(seg->s_as->a_hat, seg->s_base,
	    seg->s_size, HAT_UNLOAD);
}

/*
 * Handle lock segment faults here...
 *
 * This is where the magic happens.
 *
 * mutex already held at this point
 */

/* ARGSUSED */
static	faultcode_t
seglock_lockfault(struct seg *seg, caddr_t addr, SegProc *sdp, SegLock *lp)
{
	SegProc	*ndp;
	struct as *oas;
	clock_t	win_lbolt;

	assert(sdp != NULL && lp != NULL);
	DEBUGF(3, (CE_CONT,
		"seglock_lockfault: seg=%p, sdp=%p, lp=%p proc %d\n",
		seg, sdp, lp, ID(sdp)));

#if LOCKDEBUG
	if (sdp == NULL)
		return (FC_MAKE_ERR(EFAULT));

	if (lp == NULL)
		return (FC_MAKE_ERR(EFAULT));
#endif

	/* is this an orphan page?  If so, find it's parent and bind it. */

	if (sdp->procp == NULL) {
		Segdrv_Data *segdat = (Segdrv_Data *)seg->s_data;

		assert(sdp->locksegp == NULL);
		assert(sdp->unlocksegp == NULL);
		ndp = seglock_findclient(lp);
		DEBUGF(3, (CE_CONT,
		    "matching orphan page %p with parent %p\n", sdp, ndp));

		/*
		 * We can assert that our flag is zero because the winlock
		 * driver doesn't currently inherit flags (i.e. SY_NOTIMEOUT)
		 * across a fork().  If this is to change, seglock_dup()
		 * must copy the flags from sdp to ndp, and the below code
		 * should be changed to "ndp->flag |= (sdp->flag & UFLAGS)"
		 * (the UFLAGS mask is necessary to assure that we don't
		 * clobber the existing KFLAGS).
		 */
		ASSERT(sdp->flag == 0);

		if (ndp->locksegp == NULL) {
			ndp->locksegp = seg;
			ndp->lockaddr = seg->s_base;	/* TODO: lose this? */
		} else if (ndp->unlocksegp == NULL) {
			ndp->unlocksegp = seg;
			ndp->unlockaddr = seg->s_base;	/* TODO: lose this? */
		} else {
			/*EMPTY*/
			assert(0);
		}
		kmem_free(sdp, sizeof (*sdp));
		sdp = ndp;
		segdat->client = (caddr_t)ndp;
	}

	/* Got past all the sanity checking.  Take care of the fault. */

	/*
	 * Before reading the lock value in LOCK(lp), we must make sure that
	 * the owner cannot change its value before we change the mappings
	 * or else we could end up either with a hung process or more than
	 * one process thinking they have the lock.
	 */
	if (lp->owner != NULL && sdp != lp->owner) {
		oas = lp->owner->locksegp->s_as;
		if (!rw_tryenter(&oas->a_lock, RW_READER))
			return (1);
		if (lp->owner->flag & LOCKMAP) {
			DEBUGF(4, (CE_CONT, "  owner loses lock mapping\n"));
			assert(lp->owner->locksegp != NULL);
			_LOCKUNMAP(lp->owner->locksegp);
			lp->owner->flag &= ~LOCKMAP;
		}
		if (lp->owner->flag & UNLOCKMAP) {
			DEBUGF(4, (CE_CONT, "  owner loses unlock mapping\n"));
			assert(lp->owner->unlocksegp != NULL);
			_LOCKUNMAP(lp->owner->unlocksegp);
			lp->owner->flag &= ~(TRASHPAGE|UNLOCKMAP);
		}
		AS_LOCK_EXIT(oas, &oas->a_lock);
	}

	if (LOCK(lp) == 0) {		/* if not locked, go ahead */
		DEBUGF(4, (CE_CONT, "  unlocked, calling give_mapping()\n"));
		return (give_mapping(lp, sdp));
	}

#if LOCKDEBUG
	if (seg == sdp->locksegp)		/* TODO: lose these? */
		assert(sdp->lockaddr == addr);
	else if (seg == sdp->unlocksegp)
		assert(sdp->unlockaddr == addr);
#endif LOCKDEBUG
	/*
	 * There's a lock.  This gets ugly.  If the owning process is
	 * trying to write to the unlock page, give it a trashpage mapping
	 * and awaken any sleepers.
	 */

	DEBUGF(4, (CE_CONT, "  locked\n"));

	/* did the owner take a fault? */
	if (sdp == lp->owner) {
		DEBUGF(4, (CE_CONT, "  owner faulted\n"));
		if (seg == sdp->locksegp) {
			/*
			 * owner shouldn't fault on it's own lockpage, but
			 * sometimes the system takes mappings away
			 * asynchronously.
			 */
			DEBUGF(4, (CE_CONT, "  ..on lockpage\n"));
			_LOCKMAP(sdp->locksegp, sdp->lockaddr, lp->page);
			sdp->flag |= LOCKMAP;
		} else if (lp->sleepers == 0) {
			/*
			 * either the sleepers have exited, or this is another
			 * case where the system took our mapping away
			 */
			DEBUGF(4, (CE_CONT, "  ..on unlockpage, no sleeper\n"));
			if (sdp->flag & TRASHPAGE) {
				ASSERT(sdp->unlocksegp->s_as == seg->s_as);
				_LOCKUNMAP(sdp->unlocksegp);
				sdp->flag &= ~TRASHPAGE;
			}
			_LOCKMAP(sdp->unlocksegp, sdp->unlockaddr, lp->page);
			sdp->flag |= UNLOCKMAP;
		} else {
			/* sleeper gets valid mappings */
			DEBUGF(4, (CE_CONT, "  there is a sleeper\n"));
			lock_wakeup(lp, 1);
		}
		return (0);
	}

	/*
	 * A non-owning process tried to write (presumably to the lockpage,
	 * but it doesn't matter); we need to sleep for the lock while there
	 * is an owner.  As soon as the lock appears to be unowned, we exit,
	 * leaving the fault unresolved; the refault will grab the lock
	 * (or potentially sleep again).
	 */
	ASSERT(lp->owner != NULL && lp->sleepers >= 0);

	lp->sleepers++;
	while (lp->owner != NULL) {
		struct segproc *owner = lp->owner;
		int rval;

		if (lp->timeout == 0 || owner->flag & SY_NOTIMEOUT) {
			/*
			 * No timeout has been specified for this lock;
			 * we'll simply sleep on the condition variable.
			 */
			cv_wait(&lp->locksleep, &lp->mutex);
			continue;
		}

		(void) drv_getparm(LBOLT, &win_lbolt);

		/*
		 * A timeout _has_ been specified for this lock.  We need
		 * to wake up and possibly steal this lock if the owner
		 * refuses to let it go.  Note that all sleepers on a lock
		 * with a timeout wait in cv_timedwait(); the sleeper with
		 * the earliest timeout will wakeup, and potentially steal
		 * the lock.  Stealing the lock will cause a broadcast on
		 * the locksleep cv (and thus kick the other timed waiters).
		 */
		rval = cv_timedwait(&lp->locksleep,
		    &lp->mutex, win_lbolt + lp->timeout);

		if (rval > 0 || lp->owner != owner)
			/*
			 * If woke up due to locksleep being kicked (instead
			 * of the timeout going off), or if the lock has
			 * a new owner (or no owner), then we don't need to
			 * steal the lock.
			 */
			continue;

		if (owner->unlocksegp != NULL)
			oas = owner->unlocksegp->s_as;

		if (owner->locksegp != NULL)
			oas = owner->locksegp->s_as;

		ASSERT(oas != seg->s_as);

		if (oas != NULL && !rw_tryenter(&oas->a_lock, RW_READER)) {
			lp->sleepers--;
			return (1);
		}

		(void) seglock_timeout(lp);

		if (oas != NULL)
			AS_LOCK_EXIT(oas, &oas->a_lock);

		ASSERT(lp->owner == NULL);
	}

	lp->sleepers--;

	return (0);
}

/*
 * Utility: give a valid mapping to lock and unlock pages to specified
 * process, if any.  If some other process has mappings, it loses them.
 */

static int
give_mapping(SegLock *lp, SegProc *pp)
{
	struct as *old_as;
	DEBUGF(4, (CE_CONT,
	    "give_mapping: owner=%d, pp=%d\n", ID(lp->owner), ID(pp)));
	/* previous owner, if any, loses all mappings */
	if (lp->owner != NULL && pp != lp->owner) {
		old_as = lp->owner->locksegp->s_as;
		if (!rw_tryenter(&old_as->a_lock, RW_READER))
			return (1);
		if (lp->owner->flag & LOCKMAP) {
			DEBUGF(4, (CE_CONT, "  owner loses lock mapping\n"));
			assert(lp->owner->locksegp != NULL);
			_LOCKUNMAP(lp->owner->locksegp);
			lp->owner->flag &= ~LOCKMAP;
		}
		if (lp->owner->flag & UNLOCKMAP) {
			DEBUGF(4, (CE_CONT, "  owner loses unlock mapping\n"));
			assert(lp->owner->unlocksegp != NULL);
			_LOCKUNMAP(lp->owner->unlocksegp);
			lp->owner->flag &= ~(TRASHPAGE|UNLOCKMAP);
		}
		lp->owner = NULL;
		AS_LOCK_EXIT(old_as, &old_as->a_lock);
	}

	/* we have a new owner now */
	if (pp != NULL) {
		lp->owner = pp;
		if (pp->locksegp != NULL) {
			DEBUGF(4, (CE_CONT,
			    "    new owner %d gets lock mapping\n", ID(pp)));
			_LOCKMAP(pp->locksegp, pp->lockaddr, lp->page);
			pp->flag |= LOCKMAP;
		}
		/*
		 * while we're here, give new owner a valid mapping to unlock
		 * page so we don't get called again.
		 */
		if (pp->unlocksegp != NULL) {
			DEBUGF(4, (CE_CONT,
			    "    new owner %d gets unlock mapping\n", ID(pp)));
			/* blow away any current unlock mappings */
			if (pp->flag & TRASHPAGE) {
				_LOCKUNMAP(pp->unlocksegp);
				pp->flag &= ~TRASHPAGE;
			}

			if (lp->sleepers)
				return (0);

			_LOCKMAP(pp->unlocksegp, pp->unlockaddr, lp->page);
			pp->flag |= UNLOCKMAP;
		}
	}
	return (0);
}

/*
 * Here to wakeup some process that was sleeping on a lock.  If the lock's
 * locksegp or unlocksegp is non-NULL, then its corresponding as lock
 * must be held.
 */
static void
lock_wakeup(SegLock *lp, int trash)
{
	SegProc *owner = lp->owner;
	struct seg *locksegp = owner->locksegp, *unlocksegp = owner->unlocksegp;

	DEBUGF(4, (CE_CONT, "winlock_wakeup: lp=%p, owner=%d\n",
	    lp, ID(lp->owner)));

	ASSERT(MUTEX_HELD(&lp->mutex));
	ASSERT(owner != NULL);
	ASSERT(locksegp == NULL || unlocksegp == NULL ||
	    locksegp->s_as == unlocksegp->s_as);

	ASSERT(locksegp == NULL ||
	    AS_LOCK_HELD(locksegp->s_as, &locksegp->s_as->a_lock));
	ASSERT(unlocksegp == NULL ||
	    AS_LOCK_HELD(unlocksegp->s_as, &unlocksegp->s_as->a_lock));

	/*
	 * owner loses lockpage and unlockpage mappings and gains a
	 * trashpage mapping.
	 */
	if (owner->flag & LOCKMAP) {
		DEBUGF(4, (CE_CONT, "  owner loses lock mapping\n"));
		ASSERT(locksegp != NULL);
		_LOCKUNMAP(locksegp);
		owner->flag &= ~LOCKMAP;
	}
	if (owner->flag & UNLOCKMAP) {
		DEBUGF(4, (CE_CONT, "  owner loses unlock mapping\n"));
		ASSERT(unlocksegp != NULL);
		_LOCKUNMAP(unlocksegp);
		lp->owner->flag &= ~(TRASHPAGE|UNLOCKMAP);
	}
	if (trash && unlocksegp != NULL) {
		/* old owner gets mapping to trash page so it can continue */
		DEBUGF(4, (CE_CONT, "  giving trashpage mapping to owner\n"));
		_LOCKMAP(unlocksegp, owner->unlockaddr, trashpage);
		owner->flag |= (TRASHPAGE|UNLOCKMAP);
	}

	lp->owner = NULL;
	LOCK(lp) = 0;

	if (lp->sleepers) {
		DEBUGF(4, (CE_CONT, "  waking up, lp=%p\n", lp));
		cv_broadcast(&lp->locksleep);
	}
}

/*
 * destroy all allocated memory.
 */

static void
lock_destroyall(void)
{
	SegLock		*lp, *next_lp;

	for (lp = lock_free_list; lp != NULL; ) {
		next_lp =  lp->next;
		DEBUGF(4, (CE_CONT,
		    "lock_destroyall frees unused context at %p\n", lp));
		kmem_free(lp, sizeof (SegLock));
		lp = next_lp;
	}
	lock_free_list = NULL;

	if (lock_list == NULL && lockpage != NULL) {
		DEBUGF(4, (CE_CONT,
		    "lock_destroyall frees lock pages at %p\n", lockpage));
		next_lock = 0;
		winlock_freepages(lockpage, (size_t)2);
		lockpage = trashpage = NULL;
	}
}


static void
dump_all(void)
{
	SegLock	*lp;
#if LOCKDEBUG >= 2
	SegProc	*pp;
#endif

	cmn_err(CE_CONT, "\nID\t\tKEY\tNATTCH\tNMAP\tLOCK\tWAIT\n");

	for (lp = lock_list; lp != NULL; lp = lp->next)
		cmn_err(CE_CONT, "%x\t%x\t%d\t%d\t%c\t%c\n",
		    lp->cookie, lp->key, lp->alloccount, lp->mapcount,
		    lp->lockptr != 0 && LOCK(lp) ? 'Y' : 'N',
		    lp->sleepers ? 'Y' : 'N');

#if LOCKDEBUG >= 2
	if (lock_debug < 3)
		return;

	cmn_err(CE_CONT, "\n");

	for (lp = lock_list; lp != NULL; lp = lp->next) {
		cmn_err(CE_CONT,
		    "lock %p, key=%x, nattch=%d, nmap=%d, lock=%d, wait=%d,\n",
		    lp, lp->key, lp->alloccount, lp->mapcount,
		    lp->lockptr != 0 ? LOCK(lp) : -1, lp->sleepers);

		cmn_err(CE_CONT,
		    "    cookie=%x, page=%p, lockptr=%p, timeout=%ld\n",
		    lp->cookie, lp->page, lp->lockptr, lp->timeout);

		cmn_err(CE_CONT, "    clients=%p, owner=%p\n",
		    lp->clients, lp->owner);

		for (pp = lp->clients; pp != NULL; pp = pp->next) {
			cmn_err(CE_CONT, "  client %p%s, lp=%p, "
			    "flag=%x, procp=%p, pid=%d,\n",
			    pp, pp == lp->owner ? " (owner)" : "",
			    pp->lp, pp->flag, pp->procp, ID(pp));
			cmn_err(CE_CONT, "   lockaddr=%p, locksegp=%p, "
			    "unlockaddr=%p, unlocksegp=%p\n",
			    pp->lockaddr, pp->locksegp,
			    pp->unlockaddr, pp->unlocksegp);
		}
	}
#endif	/* LOCKDEBUG */
}


	/* LOADABLE DRIVER STARTS HERE */

static struct cb_ops	winlock_cb_ops = {
	winlock_open,		/* open */
	winlock_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nulldev,		/* read */
	nulldev,		/* write */
	winlock_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	winlock_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab */
	D_NEW|D_MP		/* Driver compatibility flag */
};


static struct dev_ops	winlock_ops = {
	DEVO_REV,
	0,			/* refcount */
	winlock_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	winlock_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&winlock_cb_ops,	/* driver ops */
	NULL,			/* bus ops */
};


#include <sys/modctl.h>

char	_depends_on[] = "misc/seg_drv";

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Winlock Driver v1.43",	/* Name of the module */
	&winlock_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};



	/* and here's the code that loads us in */

int
_init(void)
{
	int e;

	DEBUGF(1, (CE_CONT, "winlock: compiled %s, %s\n", __TIME__, __DATE__));

	pagesize = ptob(1);
	pageoffset = pagesize - 1;
	pagemask = ~pageoffset;

	mutex_init(&winlock_mutex, NULL, MUTEX_DEFAULT, NULL);

	e = mod_install(&modlinkage);
	DEBUGF(1, (CE_CONT, "  mod_install returns %d\n", e));

	if (e)
		mutex_destroy(&winlock_mutex);

	DEBUGF(1, (CE_CONT, "  _init returns %d\n", e));
	return (e);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


int
_fini(void)
{
	int	e;
	/* verify not still active */

	DEBUGF(1, (CE_CONT, "_fini: lock_list=%p, busy=%d\n",
		lock_list, winlock_busy));

#if LOCKDEBUG
	assert(lock_list == NULL && !winlock_busy);
	if (lock_list != NULL || winlock_busy)
		return (EBUSY);
#endif LOCKDEBUG

	e = mod_remove(&modlinkage);
	if (e == 0)
		mutex_destroy(&winlock_mutex);

	DEBUGF(1, (CE_CONT, "_fini returns %d, mem used=%d\n",
		e, total_memory));
	return (e);
}
