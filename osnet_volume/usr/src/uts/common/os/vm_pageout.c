/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

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
 * 	(c) 1986,1987,1988,1989,1990, 1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_pageout.c	1.125	99/07/16 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vm.h>
#include <sys/vmparam.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/strsubr.h>
#include <sys/callb.h>
#include <sys/tnf_probe.h>
#include <sys/mem_cage.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_kmem.h>

static int checkpage(page_t *, int);

/*
 * The following parameters control operation of the page replacement
 * algorithm.  They are initialized to 0, and then computed at boot time
 * based on the size of the system.  If they are patched non-zero in
 * a loaded vmunix they are left alone and may thus be changed per system
 * using adb on the loaded system.
 */
pgcnt_t		slowscan = 0;
pgcnt_t		fastscan = 0;

static pgcnt_t	handspreadpages = 0;
static int	loopfraction = 2;
static pgcnt_t	looppages;
static int	min_percent_cpu = 4;
static int	max_percent_cpu = 80;
static pgcnt_t	maxfastscan = 0;
static pgcnt_t	maxslowscan = 100;

pgcnt_t	maxpgio = 0;
pgcnt_t	minfree = 0;
pgcnt_t	desfree = 0;
pgcnt_t	lotsfree = 0;
pgcnt_t	needfree = 0;
pgcnt_t	throttlefree = 0;
pgcnt_t	pageout_reserve = 0;
pgcnt_t	cachefree = 0;
pgcnt_t	dyncachefree = 0;
int	priority_paging = 0;

pgcnt_t	deficit;
pgcnt_t	nscan;
pgcnt_t	desscan;

static clock_t	pageout_lbolt;
static clock_t	pageout_ticks;	/* number of ticks to be used by pageout */
static clock_t	min_pageout_ticks;		/* the fewest we allow */
static clock_t	max_pageout_ticks;		/* the most we allow */

static uint_t	reset_hands;

/*
 * The size of the clock loop.
 */
#define	LOOPPAGES	total_pages

/*
 * Set up the paging constants for the clock algorithm.
 * Called after the system is initialized and the amount of memory
 * and number of paging devices is known.
 *
 * lotsfree is 1/64 of memory, but at least 512K.
 * desfree is 1/2 of lotsfree.
 * minfree is 1/2 of desfree.
 *
 * Note: to revert to the paging algorithm of Solaris 2.4/2.5, set:
 *
 *	lotsfree = btop(512K)
 *	desfree = btop(200K)
 *	minfree = btop(100K)
 *	throttlefree = INT_MIN
 *	max_percent_cpu = 4
 */
void
setupclock(int recalc)
{

	static spgcnt_t init_lfree, init_dfree, init_mfree;
	static spgcnt_t init_tfree, init_preserve, init_mpgio;
	static spgcnt_t init_mfscan, init_fscan, init_sscan, init_hspages;
	static spgcnt_t init_cachefree;

	looppages = LOOPPAGES;

	/*
	 * setupclock can now be called to recalculate the paging
	 * parameters in the case of dynamic addition of memory.
	 * So to make sure we make the proper calculations, if such a
	 * situation should arise, we save away the initial values
	 * of each parameter so we can recall them when needed. This
	 * way we don't loose the settings an admin might have made
	 * through the /etc/system file.
	 */

	if (!recalc) {
		init_lfree = lotsfree;
		init_cachefree = cachefree;
		init_dfree = desfree;
		init_mfree = minfree;
		init_tfree = throttlefree;
		init_preserve = pageout_reserve;
		init_mpgio = maxpgio;
		init_mfscan = maxfastscan;
		init_fscan = fastscan;
		init_sscan = slowscan;
		init_hspages = handspreadpages;
	}

	/*
	 * Set up thresholds for paging:
	 */

	/*
	 * Lotsfree is threshold where paging daemon turns on.
	 */
	if (init_lfree == 0 || init_lfree >= looppages)
		lotsfree = MAX(looppages / 64, btop(512 * 1024));
	else
		lotsfree = init_lfree;

	/*
	 * Cachefree is maximum that dyncachefree can grow to. Cachefree
	 * is the point at which the scanner steals fs pages from the
	 * cachelist.
	 */
	if (priority_paging == 0) {
		cachefree = lotsfree;
	} else {
		/*
		 * If cachefree is not set in /etc/system or
		 * it is set to an invalid value, we use the
		 * default 2 * lotsfree.
		 */
		if ((init_cachefree == 0) || (init_cachefree < lotsfree))
			cachefree = 2 * lotsfree;
		else
			cachefree = init_cachefree;
	}

	dyncachefree = cachefree;

	/*
	 * Desfree is amount of memory desired free.
	 * If less than this for extended period, start swapping.
	 */
	if (init_dfree == 0 || init_dfree >= lotsfree)
		desfree = lotsfree / 2;
	else
		desfree = init_dfree;

	/*
	 * Minfree is minimal amount of free memory which is tolerable.
	 */
	if (init_mfree == 0 || init_mfree >= desfree)
		minfree = desfree / 2;
	else
		minfree = init_mfree;

	/*
	 * Throttlefree is the point at which we start throttling
	 * PG_WAIT requests until enough memory becomes available.
	 */
	if (init_tfree == 0 || init_tfree >= desfree)
		throttlefree = minfree;
	else
		throttlefree = init_tfree;

	/*
	 * Pageout_reserve is the number of pages that we keep in
	 * stock for pageout's own use.  Having a few such pages
	 * provides insurance against system deadlock due to
	 * pageout needing pages.  When freemem < pageout_reserve,
	 * non-blocking allocations are denied to any threads
	 * other than pageout and sched.  (At some point we might
	 * want to consider a per-thread flag like T_PUSHING_PAGES
	 * to indicate that a thread is part of the page-pushing
	 * dance (e.g. an interrupt thread) and thus is entitled
	 * to the same special dispensation we accord pageout.)
	 */
	if (init_preserve == 0 || init_preserve >= throttlefree)
		pageout_reserve = throttlefree / 2;
	else
		pageout_reserve = init_preserve;

	/*
	 * Maxpgio thresholds how much paging is acceptable.
	 * This figures that 2/3 busy on an arm is all that is
	 * tolerable for paging.  We assume one operation per disk rev.
	 *
	 * XXX - Does not account for multiple swap devices.
	 */
	if (init_mpgio == 0)
		maxpgio = (DISKRPM * 2) / 3;
	else
		maxpgio = init_mpgio;

	/*
	 * The clock scan rate varies between fastscan and slowscan
	 * based on the amount of free memory available.  Fastscan
	 * rate should be set based on the number pages that can be
	 * scanned per sec using ~10% of processor time.  Since this
	 * value depends on the processor, MMU, Mhz etc., it is
	 * difficult to determine it in a generic manner for all
	 * architectures.
	 *
	 * Instead of trying to determine the number of pages scanned
	 * per sec for every processor, fastscan is set to be the smaller
	 * of 1/2 of memory or MAXHANDSPREADPAGES and the sampling
	 * time is limited to ~4% of processor time.
	 *
	 * Setting fastscan to be 1/2 of memory allows pageout to scan
	 * all of memory in ~2 secs.  This implies that user pages not
	 * accessed within 1 sec (assuming, handspreadpages == fastscan)
	 * can be reclaimed when free memory is very low.  Stealing pages
	 * not accessed within 1 sec seems reasonable and ensures that
	 * active user processes don't thrash.
	 *
	 * Smaller values of fastscan result in scanning fewer pages
	 * every second and consequently pageout may not be able to free
	 * sufficient memory to maintain the minimum threshold.  Larger
	 * values of fastscan result in scanning a lot more pages which
	 * could lead to thrashing and higher CPU usage.
	 *
	 * Fastscan needs to be limited to a maximum value and should not
	 * scale with memory to prevent pageout from consuming too much
	 * time for scanning on slow CPU's and avoid thrashing, as a
	 * result of scanning too many pages, on faster CPU's.
	 * The value of 64 Meg was chosen for MAXHANDSPREADPAGES
	 * (the upper bound for fastscan) based on the average number
	 * of pages that can potentially be scanned in ~1 sec (using ~4%
	 * of the CPU) on some of the following machines that currently
	 * run Solaris 2.x:
	 *
	 *			average memory scanned in ~1 sec
	 *
	 *	25 Mhz SS1+:		23 Meg
	 *	LX:			37 Meg
	 *	50 Mhz SC2000:		68 Meg
	 *
	 *	40 Mhz 486:		26 Meg
	 *	66 Mhz 486:		42 Meg
	 *
	 * When free memory falls just below lotsfree, the scan rate
	 * goes from 0 to slowscan (i.e., pageout starts running).  This
	 * transition needs to be smooth and is achieved by ensuring that
	 * pageout scans a small number of pages to satisfy the transient
	 * memory demand.  This is set to not exceed 100 pages/sec (25 per
	 * wakeup) since scanning that many pages has no noticible impact
	 * on system performance.
	 *
	 * In addition to setting fastscan and slowscan, pageout is
	 * limited to using ~4% of the CPU.  This results in increasing
	 * the time taken to scan all of memory, which in turn means that
	 * user processes have a better opportunity of preventing their
	 * pages from being stolen.  This has a positive effect on
	 * interactive and overall system performance when memory demand
	 * is high.
	 *
	 * Thus, the rate at which pages are scanned for replacement will
	 * vary linearly between slowscan and the number of pages that
	 * can be scanned using ~4% of processor time instead of varying
	 * linearly between slowscan and fastscan.
	 *
	 * Also, the processor time used by pageout will vary from ~1%
	 * at slowscan to ~4% at fastscan instead of varying between
	 * ~1% at slowscan and ~10% at fastscan.
	 *
	 * The values chosen for the various VM parameters (fastscan,
	 * handspreadpages, etc) are not universally true for all machines,
	 * but appear to be a good rule of thumb for the machines we've
	 * tested.  They have the following ranges:
	 *
	 *	cpu speed:	20 to 70 Mhz
	 *	page size:	4K to 8K
	 *	memory size:	16M to 5G
	 *	page scan rate:	4000 - 17400 4K pages per sec
	 *
	 * The values need to be re-examined for machines which don't
	 * fall into the various ranges (e.g., slower or faster CPUs,
	 * smaller or larger pagesizes etc) shown above.
	 *
	 * On an MP machine, pageout is often unable to maintain the
	 * minimum paging thresholds under heavy load.  This is due to
	 * the fact that user processes running on other CPU's can be
	 * dirtying memory at a much faster pace than pageout can find
	 * pages to free.  The memory demands could be met by enabling
	 * more than one CPU to run the clock algorithm in such a manner
	 * that the various clock hands don't overlap.  This also makes
	 * it more difficult to determine the values for fastscan, slowscan
	 * and handspreadpages.
	 *
	 * The swapper is currently used to free up memory when pageout
	 * is unable to meet memory demands by swapping out processes.
	 * In addition to freeing up memory, swapping also reduces the
	 * demand for memory by preventing user processes from running
	 * and thereby consuming memory.
	 */
	if (init_mfscan == 0)
		maxfastscan = MAXHANDSPREADPAGES;
	else
		maxfastscan = init_mfscan;
	if (init_fscan == 0)
		fastscan = MIN(looppages / loopfraction, maxfastscan);
	else
		fastscan = init_fscan;
	if (fastscan > looppages / loopfraction)
		fastscan = looppages / loopfraction;

	/*
	 * Set slow scan time to 1/10 the fast scan time, but
	 * not to exceed maxslowscan.
	 */
	if (init_sscan == 0)
		slowscan = MIN(fastscan / 10, maxslowscan);
	else
		slowscan = init_sscan;
	if (slowscan > fastscan / 2)
		slowscan = fastscan / 2;

	/*
	 * Handspreadpages is distance (in pages) between front and back
	 * pageout daemon hands.  The amount of time to reclaim a page
	 * once pageout examines it increases with this distance and
	 * decreases as the scan rate rises. It must be < the amount
	 * of pageable memory.
	 *
	 * Since pageout is limited to ~4% of the CPU, setting handspreadpages
	 * to be "fastscan" results in the front hand being a few secs
	 * (varies based on the processor speed) ahead of the back hand
	 * at fastscan rates.  This distance can be further reduced, if
	 * necessary, by increasing the processor time used by pageout
	 * to be more than ~4% and preferrably not more than ~10%.
	 *
	 * As a result, user processes have a much better chance of
	 * referencing their pages before the back hand examines them.
	 * This also significantly lowers the number of reclaims from
	 * the freelist since pageout does not end up freeing pages which
	 * may be referenced a sec later.
	 */
	if (init_hspages == 0)
		handspreadpages = fastscan;
	else
		handspreadpages = init_hspages;

	/*
	 * Make sure that back hand follows front hand by at least
	 * 1/RATETOSCHEDPAGING seconds.  Without this test, it is possible
	 * for the back hand to look at a page during the same wakeup of
	 * the pageout daemon in which the front hand cleared its ref bit.
	 */
	if (handspreadpages >= looppages)
		handspreadpages = looppages - 1;

	/*
	 * If we have been called to recalculate the parameters,
	 * set a flag to re-evaluate the clock hand pointers.
	 */
	if (recalc)
		reset_hands = 1;
}

/*
 * Pageout scheduling.
 *
 * Schedpaging controls the rate at which the page out daemon runs by
 * setting the global variables nscan and desscan RATETOSCHEDPAGING
 * times a second.  Nscan records the number of pages pageout has examined
 * in its current pass; schedpaging resets this value to zero each time
 * it runs.  Desscan records the number of pages pageout should examine
 * in its next pass; schedpaging sets this value based on the amount of
 * currently available memory.
 */

#define	RATETOSCHEDPAGING	4		/* hz that is */

static kmutex_t	pageout_mutex;	/* held while pageout or schedpaging running */

/*
 * Pool of available async pageout putpage requests.
 */
static struct async_reqs *push_req;
static struct async_reqs *req_freelist;	/* available req structs */
static struct async_reqs *push_list;	/* pending reqs */
static kmutex_t push_lock;		/* protects req pool */
static kcondvar_t push_cv;

static int async_list_size = 256;	/* number of async request structs */

static void pageout_scanner(void);

/*
 * If a page is being shared more than "po_share" times
 * then leave it alone- don't page it out.
 */
#define	MIN_PO_SHARE	(8)
#define	MAX_PO_SHARE	((MIN_PO_SHARE) << 24)
u_long	po_share = MIN_PO_SHARE;

/*
 * Schedule rate for paging.
 * Rate is linear interpolation between
 * slowscan with lotsfree and fastscan when out of memory.
 */
static void
schedpaging(void *arg)
{
	spgcnt_t vavail;

	if (freemem < lotsfree + needfree + kmem_reapahead)
		kmem_reap();

	if (freemem < lotsfree + needfree + seg_preapahead)
		seg_preap();

	if (kcage_on && (kcage_freemem < kcage_desfree || kcage_needfree))
		kcage_cageout_wakeup();

	if (mutex_tryenter(&pageout_mutex)) {
		/* pageout() not running */
		nscan = 0;
		vavail = freemem - deficit;
		if (vavail < 0)
			vavail = 0;
		if (vavail > dyncachefree)
			vavail = dyncachefree;

		/*
		 * Fix for 1161438 (CRS SPR# 73922).  All variables
		 * in the original calculation for desscan were 32 bit signed
		 * ints.  As freemem approaches 0x0 on a system with 1 Gig or
		 * more * of memory, the caculation can overflow.  When this
		 * happens, desscan becomes negative and pageout_scanner()
		 * stops paging out.
		 */
		if (needfree) {
			desscan = fastscan / RATETOSCHEDPAGING;
		} else {
			spgcnt_t faststmp, slowstmp, result;

			slowstmp = slowscan * vavail;
			faststmp = fastscan * (dyncachefree - vavail);
			result = (slowstmp + faststmp) /
				nz(dyncachefree) / RATETOSCHEDPAGING;
			desscan = (pgcnt_t)result;
		}

		pageout_ticks = min_pageout_ticks + (dyncachefree - vavail) *
		    (max_pageout_ticks - min_pageout_ticks) / nz(dyncachefree);

		if (freemem < dyncachefree + needfree) {
			TRACE_1(TR_FAC_VM, TR_PAGEOUT_CV_SIGNAL,
				"pageout_cv_signal:freemem %ld", freemem);
			cv_signal(&proc_pageout->p_cv);
		} else {
			/*
			 * There are enough free pages, no need to
			 * kick the scanner thread.  And next time
			 * around, keep more of the `highly shared'
			 * pages.
			 */
			cv_signal_pageout();
			if (po_share > MIN_PO_SHARE) {
				po_share >>= 1;
			}
		}
		mutex_exit(&pageout_mutex);
	}

	/*
	 * run streams bufcalls, if necessary
	 */

	if ((strbcalls.bc_head || run_queues) && !strbcflag &&
		kmem_avail() > 0 &&
		mutex_tryenter(&service_queue)) {
			strbcflag = 1;
			cv_signal(&services_to_run);
			mutex_exit(&service_queue);
	}
	(void) timeout(schedpaging, arg, hz / RATETOSCHEDPAGING);
}

pgcnt_t		pushes;
ulong_t		push_list_size;		/* # of requests on pageout queue */

#define	FRONT	1
#define	BACK	2

int dopageout = 1;	/* must be non-zero to turn page stealing on */

/*
 * The page out daemon, which runs as process 2.
 *
 * As long as there are at least lotsfree pages,
 * this process is not run.  When the number of free
 * pages stays in the range desfree to lotsfree,
 * this daemon runs through the pages in the loop
 * at a rate determined in schedpaging().  Pageout manages
 * two hands on the clock.  The front hand moves through
 * memory, clearing the reference bit,
 * and stealing pages from procs that are over maxrss.
 * The back hand travels a distance behind the front hand,
 * freeing the pages that have not been referenced in the time
 * since the front hand passed.  If modified, they are pushed to
 * swap before being freed.
 *
 * There are 2 threads that act on behalf of the pageout process.
 * One thread scans pages (pageout_scanner) and frees them up if
 * they don't require any VOP_PUTPAGE operation. If a page must be
 * written back to its backing store, the request is put on a list
 * and the other (pageout) thread is signaled. The pageout thread
 * grabs VOP_PUTPAGE requests from the list, and processes them.
 * Some filesystems may require resources for the VOP_PUTPAGE
 * operations (like memory) and hence can block the pageout
 * thread, but the scanner thread can still operate. There is still
 * no gaurentee that memory deadlocks cannot occur.
 *
 * For now, this thing is in very rough form.
 */
void
pageout()
{
	struct async_reqs *arg;
	pri_t pageout_pri;
	int i;
	pgcnt_t max_pushes;
	callb_cpr_t cprinfo;

	proc_pageout = ttoproc(curthread);
	proc_pageout->p_cstime = 0;
	proc_pageout->p_stime =  0;
	proc_pageout->p_cutime =  0;
	proc_pageout->p_utime = 0;
	bcopy("pageout", u.u_psargs, 8);
	bcopy("pageout", u.u_comm, 7);

	/*
	 * Create pageout scanner thread
	 */
	mutex_init(&pageout_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&push_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Allocate and initialize the async request structures
	 * for pageout.
	 */
	push_req = (struct async_reqs *)
	    kmem_zalloc(async_list_size * sizeof (struct async_reqs), KM_SLEEP);

	req_freelist = push_req;
	for (i = 0; i < async_list_size - 1; i++)
		push_req[i].a_next = &push_req[i + 1];

	pageout_pri = curthread->t_pri;
	if (pageout_init(pageout_scanner, proc_pageout,
		pageout_pri - 1) == NULL)
		cmn_err(CE_PANIC, "Pageout initialisation failed");

	/*
	 * kick off pageout scheduler.
	 */
	schedpaging(NULL);

	/*
	 * Create kernel cage thread.
	 * The kernel cage thread is started under the pageout process
	 * to take advantage of the less restricted page allocation
	 * in page_create_throttle().
	 */
	kcage_cageout_init();

	/*
	 * Limit pushes to avoid saturating pageout devices.
	 */
	max_pushes = maxpgio / RATETOSCHEDPAGING;
	CALLB_CPR_INIT(&cprinfo, &push_lock, callb_generic_cpr, "pageout");

	for (;;) {
		mutex_enter(&push_lock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		while ((arg = push_list) == NULL || pushes > max_pushes) {
			cv_wait(&push_cv, &push_lock);
			pushes = 0;
		}
		CALLB_CPR_SAFE_END(&cprinfo, &push_lock);
		push_list = arg->a_next;
		arg->a_next = NULL;
		mutex_exit(&push_lock);

		if (VOP_PUTPAGE(arg->a_vp, (offset_t)arg->a_off,
			arg->a_len, arg->a_flags,
			    arg->a_cred) == 0) {
			pushes++;
		}

		/* vp held by check_page */
		VN_RELE(arg->a_vp);

		mutex_enter(&push_lock);
		arg->a_next = req_freelist;	/* back on freelist */
		req_freelist = arg;
		push_list_size--;
		mutex_exit(&push_lock);
	}
}

static pgcnt_t cachepages;

/*
 * Kernel thread that scans pages looking for ones to free
 */
static void
pageout_scanner(void)
{
	struct page *fronthand, *backhand;
	int count;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &pageout_mutex, callb_generic_cpr, "poscan");
	mutex_enter(&pageout_mutex);

	/*
	 * The restart case does not attempt to point the hands at roughly
	 * the right point on the assumption that after one circuit things
	 * will have settled down - and restarts shouldn't be that often.
	 */

	/*
	 * Set the two clock hands to be separated by a reasonable amount,
	 * but no more than 360 degrees apart.
	 */
	backhand = page_first();
	if (handspreadpages >= total_pages)
		fronthand = page_nextn(backhand, total_pages - 1);
	else
		fronthand = page_nextn(backhand, handspreadpages);

	min_pageout_ticks = MAX(1,
	    ((hz * min_percent_cpu) / 100) / RATETOSCHEDPAGING);
	max_pageout_ticks = MAX(min_pageout_ticks,
	    ((hz * max_percent_cpu) / 100) / RATETOSCHEDPAGING);

	cachepages = 0;
loop:
	cv_signal_pageout();

	CALLB_CPR_SAFE_BEGIN(&cprinfo);
	cv_wait(&proc_pageout->p_cv, &pageout_mutex);
	CALLB_CPR_SAFE_END(&cprinfo, &pageout_mutex);

	if (!dopageout)
		goto loop;

	if (reset_hands) {
		reset_hands = 0;

		backhand = page_first();
		if (handspreadpages >= total_pages)
			fronthand = page_nextn(backhand, total_pages - 1);
		else
			fronthand = page_nextn(backhand, handspreadpages);
	}

	CPU_STAT_ADDQ(CPU, cpu_vminfo.pgrrun, 1);
	count = 0;

	TRACE_4(TR_FAC_VM, TR_PAGEOUT_START,
		"pageout_start:freemem %ld lotsfree %ld nscan %ld desscan %ld",
		freemem, lotsfree, nscan, desscan);

	/* Kernel probe */
	TNF_PROBE_2(pageout_scan_start, "vm pagedaemon", /* CSTYLED */,
		tnf_ulong, pages_free, freemem,
		tnf_ulong, pages_needed, needfree);

	pageout_lbolt = lbolt + pageout_ticks;

	while ((pageout_lbolt - lbolt > 0) &&
	    (nscan < desscan) && (freemem < dyncachefree + needfree)) {
		int rvfront, rvback;

		/*
		 * If checkpage manages to add a page to the free list,
		 * we give ourselves another couple of trips around the loop.
		 */
		if ((rvfront = checkpage(fronthand, FRONT)) == 1)
			count = 0;
		if ((rvback = checkpage(backhand, BACK)) == 1)
			count = 0;

		/*
		 * protected by pageout_mutex instead of cpu_stat_lock
		 */
		CPU_STAT_ADDQ(CPU, cpu_vminfo.scan, 1);

		/*
		 * Don't include ineligible pages in the number scanned.
		 */
		if (rvfront != -1 || rvback != -1)
			nscan++;

		backhand = page_next(backhand);

		/*
		 * backhand update and wraparound check are done separately
		 * because lint barks when it finds an empty "if" body
		 */

#ifdef TRACE
		if (backhand == page_first()) {
			TRACE_2(TR_FAC_VM, TR_PAGEOUT_HAND_WRAP,
				"pageout_hand_wrap:freemem %ld whichhand %d",
				freemem, BACK);
		}
#endif /* TRACE */
		if ((fronthand = page_next(fronthand)) == page_first())	{
			TRACE_2(TR_FAC_VM, TR_PAGEOUT_HAND_WRAP,
				"pageout_hand_wrap:freemem %ld whichhand %d",
				freemem, FRONT);

			/*
			 * dyncachfree floats between lotsfree and cachefree.
			 *
			 * dyncachefree starts at lotsfree. If there is
			 * plenty of fs pages in memory, then we can
			 * increase dyncachefree. Likewise if there are
			 * too few fs pages, then we decrement dyncachefree.
			 */

			if (cachepages > (cachefree - lotsfree))
				dyncachefree = cachefree;
			else
				dyncachefree = lotsfree;

			cachepages = 0;

			/*
			 * protected by pageout_mutex instead of cpu_stat_lock
			 */
			CPU_STAT_ADDQ(CPU, cpu_vminfo.rev, 1);
			if (++count > 1) {
				/*
				 * Extremely unlikely, but it happens.
				 * We went around the loop at least once
				 * and didn't get far enough.
				 * If we are still skipping `highly shared'
				 * pages, skip fewer of them.  Otherwise,
				 * give up till the next clock tick.
				 */
				if (po_share < MAX_PO_SHARE) {
					po_share <<= 1;
				} else {
					/*
					 * Really a "goto loop", but
					 * if someone is TRACing or
					 * TNF_PROBE_ing, at least
					 * make records to show
					 * where we are.
					 */
					break;
				}
			}
		}
	}

	TRACE_5(TR_FAC_VM, TR_PAGEOUT_END,
		"pageout_end:freemem %ld lots %ld nscan %ld des %ld count %d",
		freemem, lotsfree, nscan, desscan, count);

	/* Kernel probe */
	TNF_PROBE_2(pageout_scan_end, "vm pagedaemon", /* CSTYLED */,
		tnf_ulong, pages_scanned, nscan,
		tnf_ulong, pages_free, freemem);

	goto loop;
}

/*
 * Look at the page at hand.  If it is locked (e.g., for physical i/o),
 * system (u., page table) or free, then leave it alone.  Otherwise,
 * if we are running the front hand, turn off the page's reference bit.
 * If the proc is over maxrss, we take it.  If running the back hand,
 * check whether the page has been reclaimed.  If not, free the page,
 * pushing it to disk first if necessary.
 *
 * Return values:
 *	-1 if the page is not a candidate at all,
 *	 0 if not freed, or
 *	 1 if we freed it.
 */
static int
checkpage(struct page *pp, int whichhand)
{
	int ppattr;
	int isfs = 0;
	int isexec = 0;

	/*
	 * Skip pages:
	 * 	- associated with the kernel vnode since
	 *	    they are always "exclusively" locked.
	 *	- that are free
	 *	- that are shared more than po_share'd times
	 *	- its already locked
	 *
	 * NOTE:  These optimizations assume that reads are atomic.
	 */

	if ((pp->p_vnode == &kvp) ||
	    (PP_ISFREE(pp)) ||
	    (hat_page_getshare(pp) > po_share) || PAGE_LOCKED(pp)) {
		return (-1);
	}

	if (!page_trylock(pp, SE_EXCL)) {
		/*
		 * Skip the page if we can't acquire the "exclusive" lock.
		 */
		return (-1);
	} else if (PP_ISFREE(pp)) {
		/*
		 * It became free between the above check and our actually
		 * locking the page.  Oh, well there will be other pages.
		 */
		page_unlock(pp);
		return (-1);
	}

	/*
	 * Reject pages that cannot be freed. The page_struct_lock
	 * need not be acquired to examine these
	 * fields since the page has an "exclusive" lock.
	 */
	if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
		page_unlock(pp);
		return (-1);
	}

	/*
	 * Maintain statistics for what we are freeing
	 */

	if (pp->p_vnode != NULL) {
		if (pp->p_vnode->v_flag & VVMEXEC)
			isexec = 1;

		if (!IS_SWAPVP(pp->p_vnode))
			isfs = 1;
	}

	/*
	 * Keep a count of how many fs pages per rev
	 */
	if ((whichhand == BACK) && isfs && !isexec)
		cachepages++;

	/*
	 * XXX - Where do we simulate reference bits for
	 * stupid machines (like the vax) that don't have them?
	 *
	 * hat_pagesync will turn off ref and mod bits loaded
	 * into the hardware (front hand only).
	 */
	if (whichhand == FRONT) {
		ppattr = hat_pagesync(pp, HAT_SYNC_ZERORM);
	} else {
		ppattr = hat_pagesync(pp, HAT_SYNC_DONTZERO |
			HAT_SYNC_STOPON_REF);
	}

recheck:
	/*
	 * If page is referenced; make unreferenced but reclaimable.
	 * If this page is not referenced, then it must be reclaimable
	 * and we can add it to the free list.
	 */
	if (ppattr & P_REF) {
		TRACE_4(TR_FAC_VM, TR_PAGEOUT_ISREF,
		    "pageout_isref:pp %p vp %p offset %llx whichhand %d",
		    (void *)pp, (void *)pp->p_vnode, pp->p_offset, whichhand);
		if (whichhand == FRONT) {
			/*
			 * Checking of rss or madvise flags needed here...
			 *
			 * If not "well-behaved", fall through into the code
			 * for not referenced.
			 */
			hat_clrref(pp);
		}
		/*
		 * Somebody referenced the page since the front
		 * hand went by, so it's not a candidate for
		 * freeing up.
		 */
		page_unlock(pp);
		return (0);
	}

	/*
	 * If the freemem is > lotsfree and < dyncachefree then
	 * we don't need to free application, executable or
	 * shared libraries yet
	 */
	if (((freemem > lotsfree) && (freemem < dyncachefree)) &&
	    (isexec || !isfs)) {
		page_unlock(pp);
		return (0);
	}

	/*
	 * If the page is currently dirty, we have to arrange
	 * to have it cleaned before it can be freed.
	 *
	 * XXX - ASSERT(pp->p_vnode != NULL);
	 */
	if ((ppattr & P_MOD) && pp->p_vnode) {
		struct vnode *vp = pp->p_vnode;
		u_offset_t offset = pp->p_offset;

		/*
		 * XXX - Test for process being swapped out or about to exit?
		 * [Can't get back to process(es) using the page.]
		 */

		/*
		 * Hold the vnode before releasing the page lock to
		 * prevent it from being freed and re-used by some
		 * other thread.
		 */
		VN_HOLD(vp);
		page_unlock(pp);

		/*
		 * Queue i/o request for the pageout thread.
		 */
		if (!queue_io_request(vp, offset)) {
			VN_RELE(vp);
			return (0);
		}
		return (1);
	}

	/*
	 * Now we unload all the translations,
	 * and put the page back on to the free list.
	 * If the page was used (referenced or modified) after
	 * the pagesync but before it was unloaded we catch it
	 * and handle the page properly.
	 */
	TRACE_5(TR_FAC_VM, TR_PAGEOUT_FREE,
		"pageout_free:pp %p vp %p offset %llx whichhand %d freemem %ld",
		pp, pp->p_vnode, pp->p_offset, whichhand, freemem);
	(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);
	ppattr = hat_page_getattr(pp, P_MOD | P_REF);
	if ((ppattr & P_REF) || ((ppattr & P_MOD) && pp->p_vnode))
		goto recheck;

	/*LINTED: constant in conditional context*/
	VN_DISPOSE(pp, B_FREE, 0, kcred);

	CPU_STAT_ADD_K(cpu_vminfo.dfree, 1);

	if (isfs) {
		if (isexec) {
			CPU_STAT_ADD_K(cpu_vminfo.execfree, 1);
		} else {
			CPU_STAT_ADD_K(cpu_vminfo.fsfree, 1);
		}
	} else {
		CPU_STAT_ADD_K(cpu_vminfo.anonfree, 1);
	}

	return (1);		/* freed a page! */
}

/*
 * Queue async i/o request from pageout_scanner and segment swapout
 * routines on one common list.  This ensures that pageout devices (swap)
 * are not saturated by pageout_scanner or swapout requests.
 * The pageout thread empties this list by initiating i/o operations.
 */
int
queue_io_request(vnode_t *vp, u_offset_t off)
{
	struct async_reqs *arg;

	/*
	 * If we cannot allocate an async request struct,
	 * skip this page.
	 */
	mutex_enter(&push_lock);
	if ((arg = req_freelist) == NULL) {
		mutex_exit(&push_lock);
		return (0);
	}
	req_freelist = arg->a_next;		/* adjust freelist */
	push_list_size++;

	arg->a_vp = vp;
	arg->a_off = off;
	arg->a_len = PAGESIZE;
	arg->a_flags = B_ASYNC | B_FREE;
	arg->a_cred = kcred;		/* always held */

	/*
	 * Add to list of pending write requests.
	 */
	arg->a_next = push_list;
	push_list = arg;

	if (req_freelist == NULL) {
		/*
		 * No free async requests left. The lock is held so we
		 * might as well signal the pusher thread now.
		 */
		cv_signal(&push_cv);
	}
	mutex_exit(&push_lock);
	return (1);
}

/*
 * Wakeup pageout to initiate i/o if push_list is not empty.
 */
void
cv_signal_pageout()
{
	if (push_list != NULL) {
		mutex_enter(&push_lock);
		cv_signal(&push_cv);
		mutex_exit(&push_lock);
	}
}
