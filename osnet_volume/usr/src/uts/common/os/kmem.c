/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kmem.c	1.58	99/09/17 SMI"

/*
 * Kernel memory allocator, as described in:
 *
 * Jeff Bonwick,
 * The Slab Allocator: An Object-Caching Kernel Memory Allocator.
 * Proceedings of the Summer 1994 Usenix Conference.
 *
 * See /shared/sac/PSARC/1994/028 for copies of the paper and
 * related design documentation.
 */

#include <sys/kmem_impl.h>
#include <sys/vmem_impl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/vm.h>
#include <sys/proc.h>
#include <sys/tuneable.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/bitmap.h>
#include <sys/atomic.h>
#include <sys/vtrace.h>
#include <sys/kobj.h>
#include <sys/disp.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <sys/log.h>
#include <sys/callb.h>
#include <sys/taskq.h>
#include <sys/modctl.h>
#include <sys/reboot.h>
#include <sys/id32.h>

extern void streams_msg_init(void);

struct kmem_cache_kstat {
	kstat_named_t	kmc_buf_size;
	kstat_named_t	kmc_align;
	kstat_named_t	kmc_chunk_size;
	kstat_named_t	kmc_slab_size;
	kstat_named_t	kmc_alloc;
	kstat_named_t	kmc_alloc_fail;
	kstat_named_t	kmc_free;
	kstat_named_t	kmc_depot_alloc;
	kstat_named_t	kmc_depot_free;
	kstat_named_t	kmc_depot_contention;
	kstat_named_t	kmc_global_alloc;
	kstat_named_t	kmc_global_free;
	kstat_named_t	kmc_buf_constructed;
	kstat_named_t	kmc_buf_avail;
	kstat_named_t	kmc_buf_inuse;
	kstat_named_t	kmc_buf_total;
	kstat_named_t	kmc_buf_max;
	kstat_named_t	kmc_slab_create;
	kstat_named_t	kmc_slab_destroy;
	kstat_named_t	kmc_vmem_source;
	kstat_named_t	kmc_hash_size;
	kstat_named_t	kmc_hash_lookup_depth;
	kstat_named_t	kmc_hash_rescale;
	kstat_named_t	kmc_full_magazines;
	kstat_named_t	kmc_empty_magazines;
	kstat_named_t	kmc_magazine_size;
} kmem_cache_kstat = {
	{ "buf_size",		KSTAT_DATA_ULONG },
	{ "align",		KSTAT_DATA_ULONG },
	{ "chunk_size",		KSTAT_DATA_ULONG },
	{ "slab_size",		KSTAT_DATA_ULONG },
	{ "alloc",		KSTAT_DATA_ULONG },
	{ "alloc_fail",		KSTAT_DATA_ULONG },
	{ "free",		KSTAT_DATA_ULONG },
	{ "depot_alloc",	KSTAT_DATA_ULONG },
	{ "depot_free",		KSTAT_DATA_ULONG },
	{ "depot_contention",	KSTAT_DATA_ULONG },
	{ "global_alloc",	KSTAT_DATA_ULONG },
	{ "global_free",	KSTAT_DATA_ULONG },
	{ "buf_constructed",	KSTAT_DATA_ULONG },
	{ "buf_avail",		KSTAT_DATA_ULONG },
	{ "buf_inuse",		KSTAT_DATA_ULONG },
	{ "buf_total",		KSTAT_DATA_ULONG },
	{ "buf_max",		KSTAT_DATA_ULONG },
	{ "slab_create",	KSTAT_DATA_ULONG },
	{ "slab_destroy",	KSTAT_DATA_ULONG },
	{ "vmem_source",	KSTAT_DATA_ULONG },
	{ "hash_size",		KSTAT_DATA_ULONG },
	{ "hash_lookup_depth",	KSTAT_DATA_ULONG },
	{ "hash_rescale",	KSTAT_DATA_ULONG },
	{ "full_magazines",	KSTAT_DATA_ULONG },
	{ "empty_magazines",	KSTAT_DATA_ULONG },
	{ "magazine_size",	KSTAT_DATA_ULONG },
};

static kmutex_t kmem_cache_kstat_lock;

/*
 * The default set of caches to back kmem_alloc().
 * These sizes should be reevaluated periodically.
 */
static int kmem_alloc_sizes[] = {
	8,
	16,	24,
	32,	40,	48,	56,
	64,	80,	96,	112,
	128,	144,	160,	176,	192,	208,	224,	240,
	256,	320,	384,	448,
	512,	544,	640,	768,	864,	992,
	1120,	1312,	1600,
	2048,	2688,
	4096,
	8192,	12288,
	16384
};

#define	KMEM_MAXBUF	16384

static kmem_cache_t *kmem_alloc_table[KMEM_MAXBUF >> KMEM_ALIGN_SHIFT];

/*
 * The magazine types for fast per-cpu allocation
 */
typedef struct kmem_magazine_type {
	int		mt_magsize;	/* magazine size (number of rounds) */
	int		mt_align;	/* magazine alignment */
	int		mt_minbuf;	/* all smaller buffers qualify */
	int		mt_maxbuf;	/* no larger buffers qualify */
	kmem_cache_t	*mt_cache;
} kmem_magazine_type_t;

kmem_magazine_type_t kmem_magazine_type[] = {
	{ 1,	8,	3200,	65536	},
	{ 3,	16,	256,	32768	},
	{ 7,	32,	64,	16384	},
	{ 15,	64,	0,	8192	},
	{ 31,	64,	0,	4096	},
	{ 47,	64,	0,	2048	},
	{ 63,	64,	0,	1024	},
	{ 95,	64,	0,	512	},
	{ 143,	64,	0,	0	},
};

#define	KMEM_MAGAZINE_SIZE(cp)	((cp)->cache_bufsize / (int)sizeof (void *) - 1)

/*
 * Redzone size encodings for kmem_alloc() / kmem_free().  We encode the
 * allocation size, rather than storing it directly, so that kmem_free()
 * can distinguish frees of the wrong size from redzone violations.
 */
#define	KMEM_SIZE_ENCODE(x)	((UINT_MAX / KMEM_MAXBUF) * (x) + 1)
#define	KMEM_SIZE_DECODE(x)	((x) / (UINT_MAX / KMEM_MAXBUF))
#define	KMEM_SIZE_VALID(x)	((x) % (UINT_MAX / KMEM_MAXBUF) == 1)

uint_t kmem_random;
static uint32_t kmem_reaping;

/*
 * kmem tunables
 */
clock_t kmem_reap_interval;	/* cache reaping rate [15 * HZ ticks] */
int kmem_depot_contention = 3;	/* max failed tryenters per real interval */
pgcnt_t kmem_reapahead = 0;	/* start reaping N pages before pageout */
int kmem_minhash = 512;		/* threshold for hashing (using bufctls) */
int kmem_panic = 1;		/* whether to panic on error */
uint_t kmem_mtbf = UINT_MAX;	/* mean time between injected failures */
size_t kmem_log_size;		/* KMF_AUDIT log size [2% of memory] */
size_t kmem_faillog_size;	/* failure log [4 pages per CPU] */
size_t kmem_slablog_size;	/* slab create log [4 pages per CPU] */
int kmem_logging = 1;		/* kmem_log_enter() override */
int kmem_content_maxsave = 256;	/* KMF_CONTENTS max bytes to log */
size_t kmem_lite_minsize = 0;	/* minimum buffer size for KMF_LITE */
int kmem_lite_maxalign = 1024;	/* maximum buffer alignment for KMF_LITE */
int kmem_self_debug = KMC_NODEBUG; /* set to 0 to have allocator debug itself */

#ifdef DEBUG
#define	KMEM_RANDOM_ALLOCATION_FAILURE(cp, kmflag, beancounter)		\
	kmem_random = (kmem_random * 2416 + 374441) % 1771875;		\
	if ((kmflag & KM_NOSLEEP) && kmem_random < 1771875 / kmem_mtbf) { \
		kmem_log_event(kmem_failure_log, cp, NULL, NULL);	\
		beancounter++;						\
		return (NULL);						\
	}
int kmem_flags = KMF_AUDIT | KMF_DEADBEEF | KMF_REDZONE | KMF_CONTENTS;
#else
#define	KMEM_RANDOM_ALLOCATION_FAILURE(cp, kmflag, beancounter)
int kmem_flags = 0;
#endif
int kmem_ready;

static kmem_cache_t	*kmem_slab_cache;
static kmem_cache_t	*kmem_bufctl_cache;
static kmem_cache_t	*kmem_bufctl_audit_cache;

static kmutex_t		kmem_cache_lock;	/* inter-cache linkage only */
kmem_cache_t		kmem_null_cache;

static taskq_t		*kmem_taskq;
static kmutex_t		kmem_flags_lock;
static vmem_t		*kmem_internal_arena;
static vmem_t		*kmem_cache_arena;
static vmem_t		*kmem_log_arena;
static vmem_t		*kmem_oversize_arena;
static vmem_t		*kmem_default_arena;
static vmem_t		*kmem_va_arena;

kmem_log_header_t	*kmem_transaction_log;
kmem_log_header_t	*kmem_content_log;
kmem_log_header_t	*kmem_failure_log;
kmem_log_header_t	*kmem_slab_log;

#define	KMERR_MODIFIED	0	/* buffer modified while on freelist */
#define	KMERR_REDZONE	1	/* redzone violation (write past end of buf) */
#define	KMERR_DUPFREE	2	/* freed a buffer twice */
#define	KMERR_BADADDR	3	/* freed a bad (unallocated) address */
#define	KMERR_BADBUFTAG	4	/* buftag corrupted */
#define	KMERR_BADBUFCTL	5	/* bufctl corrupted */
#define	KMERR_BADCACHE	6	/* freed a buffer to the wrong cache */
#define	KMERR_BADSIZE	7	/* alloc size != free size */
#define	KMERR_BADBASE	8	/* buffer base address wrong */

#define	KMEM_CPU_CACHE(cp)	\
	(kmem_cpu_cache_t *)((char *)cp + CPU->cpu_cache_offset)

#define	KMEM_TASKQ_DISPATCH(func, cp)	\
	(void) taskq_dispatch(kmem_taskq, (task_func_t *)func, cp, KM_NOSLEEP)

struct {
	hrtime_t	kmp_timestamp;	/* timestamp of panic */
	int		kmp_error;	/* type of kmem error */
	void		*kmp_buffer;	/* buffer that induced panic */
	void		*kmp_realbuf;	/* real start address for buffer */
	kmem_cache_t	*kmp_cache;	/* buffer's cache according to client */
	kmem_cache_t	*kmp_realcache;	/* actual cache containing buffer */
	kmem_slab_t	*kmp_slab;	/* slab accoring to kmem_findslab() */
	kmem_bufctl_t	*kmp_bufctl;	/* bufctl according to buftag */
} kmem_panic_info;

static void
copy_pattern(uint32_t pattern, void *buf_arg, size_t size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf = buf_arg;

	while (buf < bufend - 3) {
		buf[3] = buf[2] = buf[1] = buf[0] = pattern;
		buf += 4;
	}
	while (buf < bufend)
		*buf++ = pattern;
}

static void *
verify_pattern(uint32_t pattern, void *buf_arg, size_t size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf;

	for (buf = buf_arg; buf < bufend; buf++)
		if (*buf != pattern)
			return (buf);
	return (NULL);
}

/*
 * Verifying that memory hasn't been modified while on the freelist (deadbeef)
 * and copying in a known pattern to detect uninitialized data use (baddcafe)
 * are among the most expensive operations in a DEBUG kernel -- typically
 * consuming 10-20% of overall system performance -- so they have to be as
 * fast as possible.  verify_and_copy_pattern() provides a high-performance
 * solution that combines the verify and copy operations and minimizes the
 * total number of branches, load stalls and cache misses.  Most of the
 * optimizations are familiar -- loop unrolling, using bitwise "or" rather
 * than logical "or" to collapse several compaisons down to one, etc.
 *
 * The one thing that's not obvious is the way the main loop terminates.
 * It does not check for buf < bufend because it can safely assume that
 * every buffer has a buftag -- that is, four or more words consisting
 * of a redzone, bufctl pointer, etc -- which can be used as a sentinel.
 * Therefore the main loop terminates when it encounters a pattern
 * match error *or* when it hits the buftag -- whichever comes first.
 * The cleanup loop (while buf < bufend) takes at most 3 iterations
 * to discriminate between clean termination and pattern mismatch.
 */
static void *
verify_and_copy_pattern(uint32_t old, uint32_t new, void *buf_arg, size_t size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf = buf_arg;
	uint32_t tmp0, tmp1, tmp2, tmp3;

	tmp0 = buf[0];
	tmp1 = buf[1];
	tmp2 = buf[2];
	tmp3 = buf[3];
	while ((tmp0 - old | tmp1 - old | tmp2 - old | tmp3 - old) == 0) {
		tmp0 = buf[4];
		tmp1 = buf[5];
		tmp2 = buf[6];
		tmp3 = buf[7];
		buf[0] = new;
		buf[1] = new;
		buf[2] = new;
		buf[3] = new;
		buf += 4;
	}
	while (buf < bufend) {
		if (*buf != old) {
			copy_pattern(old, buf_arg,
				(char *)buf - (char *)buf_arg);
			return (buf);
		}
		*buf++ = new;
	}
	return (NULL);
}

static void
kmem_cache_applyall(void (*func)(kmem_cache_t *), taskq_t *tq)
{
	kmem_cache_t *cp;

	mutex_enter(&kmem_cache_lock);
	for (cp = kmem_null_cache.cache_next; cp != &kmem_null_cache;
	    cp = cp->cache_next)
		if (tq != NULL)
			KMEM_TASKQ_DISPATCH(func, cp);
		else
			func(cp);
	mutex_exit(&kmem_cache_lock);
}

/*
 * Debugging support.  Given a buffer address, find its slab.
 */
static kmem_slab_t *
kmem_findslab(kmem_cache_t *cp, void *buf)
{
	kmem_slab_t *sp;

	mutex_enter(&cp->cache_lock);
	for (sp = cp->cache_nullslab.slab_next;
	    sp != &cp->cache_nullslab; sp = sp->slab_next) {
		if ((uintptr_t)buf - (uintptr_t)sp->slab_base <
		    cp->cache_slabsize) {
			mutex_exit(&cp->cache_lock);
			return (sp);
		}
	}
	mutex_exit(&cp->cache_lock);

	return (NULL);
}

static void
kmem_bufctl_display(kmem_bufctl_audit_t *bcp)
{
	int d;
	timestruc_t ts;

	hrt2ts(kmem_panic_info.kmp_timestamp - bcp->bc_timestamp, &ts);
	printf("thread=%p  time=T-%ld.%09ld  slab=%p  cache: %s\n",
		(void *)bcp->bc_thread, ts.tv_sec, ts.tv_nsec,
		(void *)bcp->bc_slab, bcp->bc_cache->cache_name);
	for (d = 0; d < MIN(bcp->bc_depth, KMEM_STACK_DEPTH); d++) {
		ulong_t off;
		char *sym = kobj_getsymname(bcp->bc_stack[d], &off);
		printf("%s+%lx\n", sym ? sym : "?", off);
	}
}

static void
kmem_error(int error, kmem_cache_t *cparg, void *bufarg)
{
	kmem_buftag_t *btp = NULL;
	kmem_bufctl_t *bcp = NULL;
	kmem_cache_t *cp = cparg;
	kmem_slab_t *sp;
	uint32_t *off;
	void *buf = bufarg;

	kmem_logging = 0;	/* stop logging when a bad thing happens */

	kmem_panic_info.kmp_timestamp = gethrtime();

	sp = kmem_findslab(cp, buf);
	if (sp == NULL) {
		for (cp = kmem_null_cache.cache_prev; cp != &kmem_null_cache;
		    cp = cp->cache_prev) {
			if ((sp = kmem_findslab(cp, buf)) != NULL)
				break;
		}
	}

	if (sp == NULL) {
		cp = NULL;
		bcp = NULL;
		error = KMERR_BADADDR;
	} else {
		if (cp != cparg)
			error = KMERR_BADCACHE;
		else
			buf = (char *)bufarg - ((uintptr_t)bufarg -
			    (uintptr_t)sp->slab_base) % cp->cache_chunksize;
		if (buf != bufarg)
			error = KMERR_BADBASE;
		btp = KMEM_BUFTAG(cp, buf);
		if ((cp->cache_flags & KMF_HASH) &&
		    !(cp->cache_flags & KMF_LITE)) {
			mutex_enter(&cp->cache_lock);
			for (bcp = *KMEM_HASH(cp, buf); bcp; bcp = bcp->bc_next)
				if (bcp->bc_addr == buf)
					break;
			mutex_exit(&cp->cache_lock);
			if (bcp == NULL && (cp->cache_flags & KMF_BUFTAG))
				bcp = btp->bt_bufctl;
			if (kmem_findslab(cp->cache_bufctl_cache, bcp) ==
			    NULL || ((uintptr_t)bcp & (KMEM_ALIGN - 1)) ||
			    bcp->bc_addr != buf) {
				error = KMERR_BADBUFCTL;
				bcp = NULL;
			}
		}
	}

	kmem_panic_info.kmp_error = error;
	kmem_panic_info.kmp_buffer = bufarg;
	kmem_panic_info.kmp_realbuf = buf;
	kmem_panic_info.kmp_cache = cparg;
	kmem_panic_info.kmp_realcache = cp;
	kmem_panic_info.kmp_slab = sp;
	kmem_panic_info.kmp_bufctl = bcp;

	printf("kernel memory allocator: ");

	switch (error) {

	case KMERR_MODIFIED:
		printf("buffer modified after being freed\n");
		off = verify_pattern(KMEM_FREE_PATTERN, buf, cp->cache_offset);
		if (off == NULL)	/* shouldn't happen */
			off = buf;
		printf("modification occurred at offset 0x%lx "
		    "(0x%x replaced by 0x%x)\n",
		    (uintptr_t)off - (uintptr_t)buf, KMEM_FREE_PATTERN, *off);
		break;

	case KMERR_REDZONE:
		printf("redzone violation: write past end of buffer\n");
		break;

	case KMERR_BADADDR:
		printf("invalid free: buffer not in cache\n");
		break;

	case KMERR_DUPFREE:
		printf("duplicate free: buffer freed twice\n");
		break;

	case KMERR_BADBUFTAG:
		printf("boundary tag corrupted\n");
		printf("bcp ^ bxstat = %lx, should be %lx\n",
		    (intptr_t)btp->bt_bufctl ^ btp->bt_bxstat,
		    KMEM_BUFTAG_FREE);
		break;

	case KMERR_BADBUFCTL:
		printf("bufctl corrupted\n");
		break;

	case KMERR_BADCACHE:
		printf("buffer freed to wrong cache\n");
		printf("buffer was allocated from %s,\n", cp->cache_name);
		printf("caller attempting free to %s.\n", cparg->cache_name);
		break;

	case KMERR_BADSIZE:
		printf("bad free: free size (%u) != alloc size (%u)\n",
		    KMEM_SIZE_DECODE(((uint32_t *)btp)[0]),
		    KMEM_SIZE_DECODE(((uint32_t *)btp)[1]));
		break;

	case KMERR_BADBASE:
		printf("bad free: free address (%p) != alloc address (%p)\n",
		    bufarg, buf);
		break;
	}

	printf("buffer=%p  bufctl=%p  cache: %s\n",
	    bufarg, (void *)bcp, cparg->cache_name);

	if (bcp != NULL && (cp->cache_flags & KMF_AUDIT) &&
	    error != KMERR_BADBUFCTL) {
		printf("previous transaction on buffer %p:\n", buf);
		kmem_bufctl_display((kmem_bufctl_audit_t *)bcp);
	}
	if (kmem_panic > 0)
		panic("kernel heap corruption detected");
	if (kmem_panic == 0)
		debug_enter(NULL);
	kmem_logging = 1;	/* resume logging */
}

static kmem_log_header_t *
kmem_log_init(size_t logsize)
{
	kmem_log_header_t *lhp;
	int nchunks = 4 * max_ncpus;
	size_t lhsize = (size_t)&((kmem_log_header_t *)0)->lh_cpu[max_ncpus];
	int i;

	/*
	 * Make sure that lhp->lh_cpu[] is nicely aligned
	 * to prevent false sharing of cache lines.
	 */
	lhsize = P2ROUNDUP(lhsize, KMEM_ALIGN);
	lhp = vmem_xalloc(kmem_log_arena, lhsize, 64, P2NPHASE(lhsize, 64), 0,
	    NULL, NULL, VM_SLEEP);
	bzero(lhp, lhsize);

	mutex_init(&lhp->lh_lock, NULL, MUTEX_DEFAULT, NULL);
	lhp->lh_nchunks = nchunks;
	lhp->lh_chunksize = P2ROUNDUP(logsize / nchunks + 1, PAGESIZE);
	lhp->lh_base = vmem_alloc(kmem_log_arena,
	    lhp->lh_chunksize * nchunks, VM_SLEEP);
	lhp->lh_free = vmem_alloc(kmem_log_arena,
	    nchunks * sizeof (int), VM_SLEEP);
	bzero(lhp->lh_base, lhp->lh_chunksize * nchunks);

	for (i = 0; i < max_ncpus; i++) {
		kmem_cpu_log_header_t *clhp = &lhp->lh_cpu[i];
		mutex_init(&clhp->clh_lock, NULL, MUTEX_DEFAULT, NULL);
		clhp->clh_chunk = i;
	}

	for (i = max_ncpus; i < nchunks; i++)
		lhp->lh_free[i] = i;

	lhp->lh_head = max_ncpus;
	lhp->lh_tail = 0;

	return (lhp);
}

static void *
kmem_log_enter(kmem_log_header_t *lhp, void *data, size_t size)
{
	void *logspace;
	kmem_cpu_log_header_t *clhp = &lhp->lh_cpu[CPU->cpu_seqid];

	if (lhp == NULL || kmem_logging == 0 || panicstr)
		return (NULL);

	mutex_enter(&clhp->clh_lock);
	clhp->clh_hits++;
	if (size > clhp->clh_avail) {
		mutex_enter(&lhp->lh_lock);
		lhp->lh_hits++;
		lhp->lh_free[lhp->lh_tail] = clhp->clh_chunk;
		lhp->lh_tail = (lhp->lh_tail + 1) % lhp->lh_nchunks;
		clhp->clh_chunk = lhp->lh_free[lhp->lh_head];
		lhp->lh_head = (lhp->lh_head + 1) % lhp->lh_nchunks;
		clhp->clh_current = lhp->lh_base +
			clhp->clh_chunk * lhp->lh_chunksize;
		clhp->clh_avail = lhp->lh_chunksize;
		if (size > lhp->lh_chunksize)
			size = lhp->lh_chunksize;
		mutex_exit(&lhp->lh_lock);
	}
	logspace = clhp->clh_current;
	clhp->clh_current += size;
	clhp->clh_avail -= size;
	bcopy(data, logspace, size);
	mutex_exit(&clhp->clh_lock);
	return (logspace);
}

static void
kmem_log_event(kmem_log_header_t *lp, kmem_cache_t *cp,
	kmem_slab_t *sp, void *addr)
{
	kmem_bufctl_audit_t bca;

	bzero(&bca, sizeof (kmem_bufctl_audit_t));
	bca.bc_addr = addr;
	bca.bc_slab = sp;
	bca.bc_cache = cp;
	bca.bc_timestamp = gethrtime();
	bca.bc_thread = curthread;
	bca.bc_depth = getpcstack(bca.bc_stack, KMEM_STACK_DEPTH);
	(void) kmem_log_enter(lp, &bca, sizeof (bca));
}

pgcnt_t
kmem_avail(void)
{
	spgcnt_t rmem = availrmem - tune.t_minarmem;
	spgcnt_t fmem = freemem - minfree;
	pgcnt_t pages_avail = MIN(MAX(MIN(rmem, fmem), 0),
	    1 << (30 - PAGESHIFT));

	return (ptob(pages_avail));
}

/*
 * Return the maximum amount of memory that is (in theory) allocatable
 * from the heap. This may be used as an estimate only since there
 * is no guarentee this space will still be available when an allocation
 * request is made, nor that the space may be allocated in one big request
 * due to kernel heap fragmentation.
 */
size_t
kmem_maxavail(void)
{
	pgcnt_t max_phys;
	size_t max_virt;
	pgcnt_t pages_avail;

	if (availrmem > tune.t_minarmem)
		max_phys = availrmem - tune.t_minarmem;
	else
		max_phys = 0;
	max_virt = btop(vmem_size(heap_arena, VMEM_FREE));
	pages_avail = MIN(max_phys, max_virt);

	return ((size_t)(pages_avail << PAGESHIFT));
}

static kmem_slab_t *
kmem_slab_create(kmem_cache_t *cp, int kmflag)
{
	size_t slabsize = cp->cache_slabsize;
	int chunksize = cp->cache_chunksize;
	int cache_flags = cp->cache_flags;
	int color, chunks;
	char *buf, *base;
	kmem_slab_t *sp;
	kmem_bufctl_t *bcp;
	vmem_t *vmp = cp->cache_arena;

	ASSERT(MUTEX_HELD(&cp->cache_lock));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_START,
		"kmem_slab_create_start:cache %S kmflag %x",
		cp->cache_name, kmflag);

	if ((color = cp->cache_color += cp->cache_align) > cp->cache_maxcolor)
		color = cp->cache_color = 0;

	mutex_exit(&cp->cache_lock);

	/*
	 * There may be multiple kmem_slab_create()s in a stack trace,
	 * so it's important to keep our stack depth under control.
	 * Avoid the vmem_alloc() frame by calling either vmem_xalloc()
	 * or kmem_cache_alloc() as appropriate.
	 */
	if (slabsize > vmp->vm_qcache_max)
		base = vmem_xalloc(vmp, slabsize, vmp->vm_quantum,
		    0, 0, NULL, NULL, kmflag & KM_VMFLAGS);
	else
		base = kmem_cache_alloc(vmp->vm_qcache[(slabsize - 1) >>
		    vmp->vm_qshift], kmflag);

	if (base == NULL)
		goto page_alloc_failure;
	ASSERT(P2PHASE((uintptr_t)base, vmp->vm_quantum) == 0);

	if ((cache_flags & KMF_DEADBEEF) && !(cache_flags & KMF_LITE))
		copy_pattern(KMEM_FREE_PATTERN, base, slabsize);
	else if (!(cp->cache_cflags & KMC_NOTOUCH))
		copy_pattern(KMEM_UNINITIALIZED_PATTERN, base, slabsize);

	if (cache_flags & KMF_HASH) {
		if ((sp = kmem_cache_alloc(kmem_slab_cache, kmflag)) == NULL)
			goto slab_alloc_failure;
		chunks = (slabsize - color) / chunksize;
	} else {
		sp = (kmem_slab_t *)(base + slabsize - sizeof (kmem_slab_t));
		chunks = (slabsize - sizeof (kmem_slab_t) - color) / chunksize;
	}

	sp->slab_cache	= cp;
	sp->slab_head	= NULL;
	sp->slab_refcnt	= 0;
	sp->slab_base	= buf = base + color;
	sp->slab_chunks	= chunks;

	ASSERT(chunks > 0);
	while (chunks-- != 0) {
		if (!(cp->cache_cflags & KMC_NOTOUCH))
			*(uint64_t *)buf = KMEM_FREE_PATTERN_64;
		if (cache_flags & KMF_HASH) {
			bcp = kmem_cache_alloc(cp->cache_bufctl_cache, kmflag);
			if (bcp == NULL)
				goto bufctl_alloc_failure;
			if (cache_flags & KMF_AUDIT)
				bzero(bcp, sizeof (kmem_bufctl_audit_t));
			bcp->bc_addr = buf;
			bcp->bc_slab = sp;
			bcp->bc_cache = cp;
		} else {
			bcp = (kmem_bufctl_t *)(buf + cp->cache_offset);
		}
		if ((cache_flags & KMF_BUFTAG) && !(cache_flags & KMF_LITE)) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			btp->bt_redzone = KMEM_REDZONE_PATTERN_64;
			btp->bt_bufctl = bcp;
			btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_FREE;
		}
		if ((bcp->bc_next = sp->slab_head) == NULL)
			sp->slab_tail = bcp;
		sp->slab_head = bcp;
		buf += chunksize;
	}

	kmem_log_event(kmem_slab_log, cp, sp, base);

	mutex_enter(&cp->cache_lock);

	cp->cache_slab_create++;
	cp->cache_buftotal += sp->slab_chunks;
	if (cp->cache_buftotal > cp->cache_bufmax)
		cp->cache_bufmax = cp->cache_buftotal;

	TRACE_1(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_END,
		"kmem_slab_create_end:slab %p", sp);

	return (sp);

bufctl_alloc_failure:
	while ((bcp = sp->slab_head) != NULL) {
		sp->slab_head = bcp->bc_next;
		kmem_cache_free(cp->cache_bufctl_cache, bcp);
	}
	kmem_cache_free(kmem_slab_cache, sp);

slab_alloc_failure:
	vmem_free(vmp, base, slabsize);

page_alloc_failure:
	kmem_log_event(kmem_failure_log, cp, NULL, NULL);
	mutex_enter(&cp->cache_lock);

	TRACE_1(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_END,
		"kmem_slab_create_end:slab %p", sp);

	return (NULL);
}

static void
kmem_slab_destroy(kmem_cache_t *cp, kmem_slab_t *sp)
{
	vmem_t *vmp = cp->cache_arena;

	ASSERT(MUTEX_HELD(&cp->cache_lock));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_SLAB_DESTROY_START,
		"kmem_slab_destroy_start:cache %S slab %p", cp->cache_name, sp);

	cp->cache_slab_destroy++;
	cp->cache_buftotal -= sp->slab_chunks;

	mutex_exit(&cp->cache_lock);

	vmem_free(vmp, (void *)P2ALIGN((uintptr_t)sp->slab_base,
	    vmp->vm_quantum), cp->cache_slabsize);

	if (cp->cache_flags & KMF_HASH) {
		kmem_bufctl_t *bcp;
		sp->slab_tail->bc_next = NULL;	/* normally a garbage pointer */
		while ((bcp = sp->slab_head) != NULL) {
			sp->slab_head = bcp->bc_next;
			kmem_cache_free(cp->cache_bufctl_cache, bcp);
		}
		kmem_cache_free(kmem_slab_cache, sp);
	}

	mutex_enter(&cp->cache_lock);

	TRACE_0(TR_FAC_KMEM, TR_KMEM_SLAB_DESTROY_END, "kmem_slab_destroy_end");
}

static void
kmem_cache_free_global(kmem_cache_t *cp, void *buf)
{
	kmem_slab_t *sp, *snext, *sprev;
	kmem_bufctl_t *bcp, **prev_bcpp, *old_slab_tail;

	ASSERT(buf != NULL);

	mutex_enter(&cp->cache_lock);
	cp->cache_global_free++;

	if (cp->cache_flags & KMF_HASH) {
		/*
		 * look up buf in allocated-address hash table
		 */
		prev_bcpp = KMEM_HASH(cp, buf);
		while ((bcp = *prev_bcpp) != NULL) {
			if (bcp->bc_addr == buf) {
				*prev_bcpp = bcp->bc_next;
				sp = bcp->bc_slab;
				break;
			}
			cp->cache_lookup_depth++;
			prev_bcpp = &bcp->bc_next;
		}
	} else {
		bcp = (kmem_bufctl_t *)((char *)buf + cp->cache_offset);
		sp = (kmem_slab_t *)P2END((uintptr_t)buf,
		    cp->cache_slabsize) - 1;
	}

	if (bcp == NULL || sp->slab_cache != cp ||
	    (uintptr_t)buf - (uintptr_t)sp->slab_base >= cp->cache_slabsize) {
		mutex_exit(&cp->cache_lock);
		kmem_error(KMERR_BADADDR, cp, buf);
		return;
	}

	old_slab_tail = sp->slab_tail;
	sp->slab_tail = bcp;
	if (old_slab_tail == NULL) {
		/*
		 * Return slab to head of free list
		 */
		sp->slab_head = bcp;
		if ((snext = sp->slab_next) != cp->cache_freelist) {
			snext->slab_prev = sprev = sp->slab_prev;
			sprev->slab_next = snext;
			sp->slab_next = snext = cp->cache_freelist;
			sp->slab_prev = sprev = snext->slab_prev;
			sprev->slab_next = sp;
			snext->slab_prev = sp;
		}
		cp->cache_freelist = sp;
	} else {
		old_slab_tail->bc_next = bcp;
	}
	ASSERT(sp->slab_refcnt >= 1);
	if (--sp->slab_refcnt == 0) {
		/*
		 * There are no outstanding allocations from this slab,
		 * so we can reclaim the memory.
		 */
		snext = sp->slab_next;
		sprev = sp->slab_prev;
		snext->slab_prev = sprev;
		sprev->slab_next = snext;
		if (sp == cp->cache_freelist)
			cp->cache_freelist = snext;
		kmem_slab_destroy(cp, sp);
	}
	mutex_exit(&cp->cache_lock);
}

static void *
kmem_cache_alloc_debug(kmem_cache_t *cp, void *buf, int kmflag)
{
	kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
	kmem_bufctl_t *bcp = btp->bt_bufctl;

	if (btp->bt_bxstat != ((intptr_t)bcp ^ KMEM_BUFTAG_FREE)) {
		kmem_error(KMERR_BADBUFTAG, cp, buf);
		return (buf);
	}
	btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_ALLOC;

	if ((cp->cache_flags & KMF_HASH) != 0 && bcp->bc_addr != buf) {
		kmem_error(KMERR_BADBUFCTL, cp, buf);
		return (buf);
	}
	if ((cp->cache_flags & (KMF_HASH | KMF_REDZONE)) == KMF_REDZONE) {
		/*
		 * If the cache doesn't have external bufctls, freelist linkage
		 * overwrites the first word of the redzone; restore it here.
		 */
		btp->bt_redzone = KMEM_REDZONE_PATTERN_64;
	}
	if (cp->cache_flags & KMF_DEADBEEF) {
		if (verify_and_copy_pattern(KMEM_FREE_PATTERN,
		    KMEM_UNINITIALIZED_PATTERN, buf, cp->cache_offset) != NULL)
			kmem_error(KMERR_MODIFIED, cp, buf);
		if (cp->cache_constructor != NULL && cp->cache_constructor(buf,
		    cp->cache_private, kmflag) != 0) {
			atomic_add_32(&cp->cache_alloc_fail, 1);
			btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_FREE;
			copy_pattern(KMEM_FREE_PATTERN, buf, cp->cache_offset);
			kmem_cache_free_global(cp, buf);
			return (NULL);
		}
	}
	if (cp->cache_flags & KMF_AUDIT) {
		kmem_bufctl_audit_t *bcap = (kmem_bufctl_audit_t *)bcp;
		bcap->bc_timestamp = gethrtime();
		bcap->bc_thread = curthread;
		bcap->bc_depth = getpcstack(bcap->bc_stack, KMEM_STACK_DEPTH);
		bcap->bc_lastlog = kmem_log_enter(kmem_transaction_log,
			bcap, sizeof (kmem_bufctl_audit_t));
	}
	return (buf);
}

static int
kmem_cache_free_debug(kmem_cache_t *cp, void *buf)
{
	kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
	kmem_bufctl_t *bcp = btp->bt_bufctl;
	kmem_slab_t *sp;

	if (btp->bt_bxstat != ((intptr_t)bcp ^ KMEM_BUFTAG_ALLOC)) {
		if (btp->bt_bxstat == ((intptr_t)bcp ^ KMEM_BUFTAG_FREE)) {
			kmem_error(KMERR_DUPFREE, cp, buf);
			return (-1);
		}
		sp = kmem_findslab(cp, buf);
		if (sp == NULL || sp->slab_cache != cp)
			kmem_error(KMERR_BADADDR, cp, buf);
		else
			kmem_error(KMERR_REDZONE, cp, buf);
		return (-1);
	}
	btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_FREE;

	if ((cp->cache_flags & KMF_HASH) != 0 && bcp->bc_addr != buf) {
		kmem_error(KMERR_BADBUFCTL, cp, buf);
		return (-1);
	}
	if (cp->cache_flags & KMF_REDZONE) {
		if (btp->bt_redzone != KMEM_REDZONE_PATTERN_64) {
			kmem_error(KMERR_REDZONE, cp, buf);
			return (-1);
		}
	}
	if (cp->cache_flags & KMF_AUDIT) {
		kmem_bufctl_audit_t *bcap = (kmem_bufctl_audit_t *)bcp;
		bcap->bc_timestamp = gethrtime();
		bcap->bc_thread = curthread;
		bcap->bc_depth = getpcstack(bcap->bc_stack, KMEM_STACK_DEPTH);
		if (cp->cache_flags & KMF_CONTENTS)
			bcap->bc_contents = kmem_log_enter(kmem_content_log,
			    buf, MIN(cp->cache_offset, kmem_content_maxsave));
		bcap->bc_lastlog = kmem_log_enter(kmem_transaction_log,
			bcap, sizeof (kmem_bufctl_audit_t));
	}
	if (cp->cache_flags & KMF_DEADBEEF) {
		if (cp->cache_destructor != NULL)
			cp->cache_destructor(buf, cp->cache_private);
		copy_pattern(KMEM_FREE_PATTERN, buf, cp->cache_offset);
	}

	return (0);
}

void *
kmem_cache_alloc(kmem_cache_t *cp, int kmflag)
{
	void *buf;
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *fmp;
	kmem_slab_t *sp, *snext, *sprev;
	kmem_slab_t *extra_slab;
	kmem_bufctl_t *bcp, **hash_bucket;
	int rounds;

	KMEM_RANDOM_ALLOCATION_FAILURE(cp, kmflag, cp->cache_alloc_fail);

	ccp = KMEM_CPU_CACHE(cp);
	mutex_enter(&ccp->cc_lock);
	for (;;) {
		rounds = ccp->cc_rounds - 1;
		mp = ccp->cc_loaded_mag;
		if (rounds >= 0) {
			ccp->cc_rounds = rounds;
			ccp->cc_alloc++;
			buf = mp->mag_round[rounds];
			mutex_exit(&ccp->cc_lock);
			if (cp->cache_flags & KMF_BUFTAG) {
				kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
				if (!(cp->cache_flags & KMF_LITE))
					return (kmem_cache_alloc_debug(cp, buf,
					    kmflag));
				if (*(uint64_t *)buf != KMEM_FREE_PATTERN_64)
					kmem_error(KMERR_MODIFIED, cp, buf);
				*(uint64_t *)buf = btp->bt_redzone;
				btp->bt_redzone = KMEM_REDZONE_PATTERN_64;
				btp->bt_lastalloc = (uint32_t)caller();
			}
			return (buf);
		}
		if ((fmp = ccp->cc_full_mag) != NULL) {
			ASSERT(ccp->cc_empty_mag == NULL);
			ccp->cc_empty_mag = mp;
			ccp->cc_loaded_mag = fmp;
			ccp->cc_full_mag = NULL;
			ccp->cc_rounds = ccp->cc_magsize;
			continue;
		}
		if (ccp->cc_magsize == 0)
			break;
		if (!mutex_tryenter(&cp->cache_depot_lock)) {
			mutex_enter(&cp->cache_depot_lock);
			cp->cache_depot_contention++;
		}
		if ((fmp = cp->cache_fmag_list) == NULL) {
			mutex_exit(&cp->cache_depot_lock);
			break;
		}
		cp->cache_fmag_list = fmp->mag_next;
		if (--cp->cache_fmag_total < cp->cache_fmag_min)
			cp->cache_fmag_min = cp->cache_fmag_total;
		if (mp != NULL) {
			if (ccp->cc_empty_mag == NULL) {
				ccp->cc_empty_mag = mp;
			} else {
				mp->mag_next = cp->cache_emag_list;
				cp->cache_emag_list = mp;
				cp->cache_emag_total++;
			}
		}
		cp->cache_depot_alloc++;
		mutex_exit(&cp->cache_depot_lock);
		ccp->cc_loaded_mag = fmp;
		ccp->cc_rounds = ccp->cc_magsize;
	}
	mutex_exit(&ccp->cc_lock);

	/*
	 * The magazine layer is empty; allocate from the global layer.
	 */
	extra_slab = NULL;
	mutex_enter(&cp->cache_lock);
	cp->cache_global_alloc++;
	sp = cp->cache_freelist;
	ASSERT(sp->slab_cache == cp);
	if ((bcp = sp->slab_head) == sp->slab_tail) {
		if (bcp == NULL) {
			/*
			 * The freelist is empty.  Create a new slab.
			 */
			if ((sp = kmem_slab_create(cp, kmflag)) == NULL) {
				atomic_add_32(&cp->cache_alloc_fail, 1);
				mutex_exit(&cp->cache_lock);
				return (NULL);
			}
			if (cp->cache_freelist == &cp->cache_nullslab) {
				/*
				 * Add slab to tail of freelist
				 */
				sp->slab_next = snext = &cp->cache_nullslab;
				sp->slab_prev = sprev = snext->slab_prev;
				snext->slab_prev = sp;
				sprev->slab_next = sp;
				cp->cache_freelist = sp;
			} else {
				extra_slab = sp;
				sp = cp->cache_freelist;
			}
		}
		/*
		 * If this is last buf in slab, remove slab from free list
		 */
		if ((bcp = sp->slab_head) == sp->slab_tail) {
			cp->cache_freelist = sp->slab_next;
			sp->slab_tail = NULL;
		}
	}

	sp->slab_head = bcp->bc_next;
	sp->slab_refcnt++;
	ASSERT(sp->slab_refcnt <= sp->slab_chunks);

	if (cp->cache_flags & KMF_HASH) {
		/*
		 * add buf to allocated-address hash table
		 */
		buf = bcp->bc_addr;
		hash_bucket = KMEM_HASH(cp, buf);
		bcp->bc_next = *hash_bucket;
		*hash_bucket = bcp;
	} else {
		buf = (void *)((char *)bcp - cp->cache_offset);
	}

	ASSERT((uintptr_t)buf - (uintptr_t)sp->slab_base < cp->cache_slabsize);

	if (extra_slab)
		kmem_slab_destroy(cp, extra_slab);

	mutex_exit(&cp->cache_lock);

	if (cp->cache_flags & KMF_DEADBEEF) {
		if (cp->cache_flags & KMF_LITE) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			if (*(uint64_t *)buf != KMEM_FREE_PATTERN_64)
				kmem_error(KMERR_MODIFIED, cp, buf);
			btp->bt_redzone = KMEM_REDZONE_PATTERN_64;
		} else {
			return (kmem_cache_alloc_debug(cp, buf, kmflag));
		}
	}

	if (cp->cache_constructor != NULL &&
	    cp->cache_constructor(buf, cp->cache_private, kmflag) != 0) {
		atomic_add_32(&cp->cache_alloc_fail, 1);
		if (cp->cache_flags & KMF_LITE)
			*(uint64_t *)buf = KMEM_FREE_PATTERN_64;
		kmem_cache_free_global(cp, buf);
		return (NULL);
	}

	if ((cp->cache_flags & KMF_BUFTAG) && !(cp->cache_flags & KMF_LITE))
		return (kmem_cache_alloc_debug(cp, buf, kmflag));

	return (buf);
}

void
kmem_cache_free(kmem_cache_t *cp, void *buf)
{
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *emp;
	kmem_cache_t *mcp;
	uint_t rounds;

	if (cp->cache_flags & KMF_BUFTAG) {
		if (!(cp->cache_flags & KMF_LITE)) {
			if (kmem_cache_free_debug(cp, buf))
				return;
		} else {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			if (btp->bt_redzone != KMEM_REDZONE_PATTERN_64) {
				if (*(int32_t *)buf == KMEM_FREE_PATTERN)
					kmem_error(KMERR_DUPFREE, cp, buf);
				else
					kmem_error(KMERR_REDZONE, cp, buf);
				return;
			}
			btp->bt_redzone = *(uint64_t *)buf;
			*(uint64_t *)buf = KMEM_FREE_PATTERN_64;
			btp->bt_lastfree = (uint32_t)caller();
		}
	}

	ccp = KMEM_CPU_CACHE(cp);
	mutex_enter(&ccp->cc_lock);
	for (;;) {
		rounds = (uint_t)ccp->cc_rounds;
		mp = ccp->cc_loaded_mag;
		if (rounds < ccp->cc_magsize) {
			ccp->cc_rounds = rounds + 1;
			ccp->cc_free++;
			mp->mag_round[rounds] = buf;
			mutex_exit(&ccp->cc_lock);
			return;
		}
		if ((emp = ccp->cc_empty_mag) != NULL) {
			ASSERT(ccp->cc_full_mag == NULL);
			ccp->cc_full_mag = mp;
			ccp->cc_loaded_mag = emp;
			ccp->cc_empty_mag = NULL;
			ccp->cc_rounds = 0;
			continue;
		}
		if (ccp->cc_magsize == 0)
			break;
		if (!mutex_tryenter(&cp->cache_depot_lock)) {
			mutex_enter(&cp->cache_depot_lock);
			cp->cache_depot_contention++;
		}
		if ((emp = cp->cache_emag_list) == NULL) {
			/*
			 * There are no magazines in the depot.
			 * Try to allocate a new one.
			 */
			mcp = cp->cache_magazine_cache;
			mutex_exit(&cp->cache_depot_lock);
			mutex_exit(&ccp->cc_lock);
			emp = kmem_cache_alloc(mcp, KM_NOSLEEP);
			mutex_enter(&ccp->cc_lock);
			if (emp && ccp->cc_magsize == KMEM_MAGAZINE_SIZE(mcp)) {
				mutex_enter(&cp->cache_depot_lock);
				ASSERT(mcp == cp->cache_magazine_cache);
				emp->mag_next = cp->cache_emag_list;
				cp->cache_emag_list = emp;
				cp->cache_emag_total++;
				mutex_exit(&cp->cache_depot_lock);
				continue;
			}
			if (emp) {
				mutex_exit(&ccp->cc_lock);
				kmem_cache_free(mcp, emp);
				mutex_enter(&ccp->cc_lock);
			}
			break;
		}
		cp->cache_emag_list = emp->mag_next;
		if (--cp->cache_emag_total < cp->cache_emag_min)
			cp->cache_emag_min = cp->cache_emag_total;
		if (mp != NULL) {
			if (ccp->cc_full_mag == NULL) {
				ccp->cc_full_mag = mp;
			} else {
				mp->mag_next = cp->cache_fmag_list;
				cp->cache_fmag_list = mp;
				cp->cache_fmag_total++;
			}
		}
		cp->cache_depot_free++;
		mutex_exit(&cp->cache_depot_lock);
		ccp->cc_loaded_mag = emp;
		ccp->cc_rounds = 0;
	}
	mutex_exit(&ccp->cc_lock);
	if ((cp->cache_destructor != NULL) &&
	    (cp->cache_flags & (KMF_DEADBEEF | KMF_LITE)) != KMF_DEADBEEF) {
		if (cp->cache_flags & KMF_LITE) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			*(uint64_t *)buf = btp->bt_redzone;
			cp->cache_destructor(buf, cp->cache_private);
			*(uint64_t *)buf = KMEM_FREE_PATTERN_64;
		} else {
			cp->cache_destructor(buf, cp->cache_private);
		}
	}
	kmem_cache_free_global(cp, buf);
}

void *
kmem_zalloc(size_t size, int kmflag)
{
	size_t index = (size - 1) >> KMEM_ALIGN_SHIFT;
	void *buf;

	if (index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		buf = kmem_cache_alloc(cp, kmflag);
		if (buf != NULL) {
			if (cp->cache_flags & KMF_BUFTAG) {
				kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
				((uint8_t *)buf)[size] = KMEM_REDZONE_BYTE;
				((uint32_t *)btp)[1] = KMEM_SIZE_ENCODE(size);
				if (cp->cache_flags & KMF_LITE)
					btp->bt_lastalloc = (uint32_t)caller();
			}
			bzero(buf, size);
		}
	} else {
		buf = kmem_alloc(size, kmflag);
		if (buf != NULL)
			bzero(buf, size);
	}
	return (buf);
}

void *
kmem_alloc(size_t size, int kmflag)
{
	size_t index = (size - 1) >> KMEM_ALIGN_SHIFT;
	void *buf;

	if (index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		buf = kmem_cache_alloc(cp, kmflag);
		if ((cp->cache_flags & KMF_BUFTAG) && buf != NULL) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			((uint8_t *)buf)[size] = KMEM_REDZONE_BYTE;
			((uint32_t *)btp)[1] = KMEM_SIZE_ENCODE(size);
			if (cp->cache_flags & KMF_LITE)
				btp->bt_lastalloc = (uint32_t)caller();
		}
		return (buf);
	}
	if (size == 0)
		return (NULL);
	buf = vmem_alloc(kmem_oversize_arena, size, kmflag & KM_VMFLAGS);
	if (buf == NULL)
		kmem_log_event(kmem_failure_log, NULL, NULL, NULL);
	return (buf);
}

void
kmem_free(void *buf, size_t size)
{
	size_t index = (size - 1) >> KMEM_ALIGN_SHIFT;

	if (index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		if (cp->cache_flags & KMF_BUFTAG) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			uint32_t *ip = (uint32_t *)btp;
			if (ip[1] != KMEM_SIZE_ENCODE(size)) {
				if (*(uint64_t *)buf == KMEM_FREE_PATTERN_64) {
					kmem_error(KMERR_DUPFREE, cp, buf);
					return;
				}
				if (KMEM_SIZE_VALID(ip[1])) {
					ip[0] = KMEM_SIZE_ENCODE(size);
					kmem_error(KMERR_BADSIZE, cp, buf);
				} else {
					kmem_error(KMERR_REDZONE, cp, buf);
				}
				return;
			}
			if (((uint8_t *)buf)[size] != KMEM_REDZONE_BYTE) {
				kmem_error(KMERR_REDZONE, cp, buf);
				return;
			}
			btp->bt_redzone = KMEM_REDZONE_PATTERN_64;
		}
		kmem_cache_free(cp, buf);
	} else {
		if (buf == NULL && size == 0)
			return;
		vmem_free(kmem_oversize_arena, buf, size);
	}
}

static void
kmem_magazine_destroy(kmem_cache_t *cp, kmem_magazine_t *mp, int nrounds)
{
	int round;

	for (round = 0; round < nrounds; round++) {
		void *buf = mp->mag_round[round];
		if (cp->cache_flags & KMF_DEADBEEF) {
			if (cp->cache_flags & KMF_LITE) {
				kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
				if (*(uint64_t *)buf != KMEM_FREE_PATTERN_64)
					kmem_error(KMERR_MODIFIED, cp, buf);
				if (cp->cache_destructor) {
					*(uint64_t *)buf = btp->bt_redzone;
					cp->cache_destructor(buf,
					    cp->cache_private);
					*(uint64_t *)buf = KMEM_FREE_PATTERN_64;
				}
			} else {
				if (verify_pattern(KMEM_FREE_PATTERN, buf,
				    cp->cache_offset) != NULL)
					kmem_error(KMERR_MODIFIED, cp, buf);
			}
		} else if (cp->cache_destructor) {
			cp->cache_destructor(buf, cp->cache_private);
		}
		kmem_cache_free_global(cp, buf);
	}
	kmem_cache_free(cp->cache_magazine_cache, mp);
}

/*
 * Reclaim all unused memory from a cache.
 */
static void
kmem_cache_reap(kmem_cache_t *cp)
{
	int reaplimit;
	kmem_magazine_t *mp;

	/*
	 * Ask the cache's owner to free some memory if possible.
	 * The idea is to handle things like the inode cache, which
	 * typically sits on a bunch of memory that it doesn't truly
	 * *need*.  Reclaim policy is entirely up to the owner; this
	 * callback is just an advisory plea for help.
	 */
	if (cp->cache_reclaim != NULL)
		cp->cache_reclaim(cp->cache_private);

	mutex_enter(&cp->cache_depot_lock);
	reaplimit = MIN(cp->cache_fmag_reaplimit, cp->cache_fmag_min);
	cp->cache_fmag_reaplimit = 0;
	while (--reaplimit >= 0 && (mp = cp->cache_fmag_list) != NULL) {
		cp->cache_fmag_total--;
		cp->cache_fmag_min--;
		cp->cache_fmag_list = mp->mag_next;
		mutex_exit(&cp->cache_depot_lock);
		kmem_magazine_destroy(cp, mp, cp->cache_magazine_size);
		mutex_enter(&cp->cache_depot_lock);
	}
	reaplimit = MIN(cp->cache_emag_reaplimit, cp->cache_emag_min);
	cp->cache_emag_reaplimit = 0;
	while (--reaplimit >= 0 && (mp = cp->cache_emag_list) != NULL) {
		cp->cache_emag_total--;
		cp->cache_emag_min--;
		cp->cache_emag_list = mp->mag_next;
		mutex_exit(&cp->cache_depot_lock);
		kmem_magazine_destroy(cp, mp, 0);
		mutex_enter(&cp->cache_depot_lock);
	}
	mutex_exit(&cp->cache_depot_lock);
}

/*ARGSUSED*/
static void
kmem_reap_timeout(void *dummy)
{
	kmem_reaping = 0;
}

static void
kmem_reap_done(void *dummy)
{
	(void) timeout(kmem_reap_timeout, dummy, kmem_reap_interval);
}

/*
 * Reclaim all unused memory from all caches.  Called from the VM system
 * when memory gets tight.
 */
void
kmem_reap(void)
{
	if (MUTEX_HELD(&kmem_cache_lock) || cas32(&kmem_reaping, 0, 1) != 0)
		return;
	kmem_cache_applyall(kmem_cache_reap, kmem_taskq);

	/*
	 * We use taskq_dispatch() to schedule a timeout to clear
	 * kmem_reaping so that kmem_reap() becomes self-throttling:
	 * we won't reap again until the current reap completes *and*
	 * at least kmem_reap_interval ticks have elapsed.
	 */
	if (!taskq_dispatch(kmem_taskq, kmem_reap_done, NULL, KM_NOSLEEP))
		kmem_reap_done(NULL);
}

/*
 * Purge all magazines from a cache and set its magazine limit to zero.
 * All calls are serialized by the kmem_taskq lock, except for the final
 * call from kmem_cache_destroy().
 */
static void
kmem_cache_magazine_purge(kmem_cache_t *cp)
{
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *fmp, *emp;
	int rounds, magsize, cpu_seqid;

	ASSERT(MUTEX_NOT_HELD(&cp->cache_lock));

	for (cpu_seqid = 0; cpu_seqid < max_ncpus; cpu_seqid++) {
		ccp = &cp->cache_cpu[cpu_seqid];

		mutex_enter(&ccp->cc_lock);
		rounds = ccp->cc_rounds;
		magsize = ccp->cc_magsize;
		mp = ccp->cc_loaded_mag;
		fmp = ccp->cc_full_mag;
		emp = ccp->cc_empty_mag;
		ccp->cc_rounds = -1;
		ccp->cc_magsize = 0;
		ccp->cc_loaded_mag = NULL;
		ccp->cc_full_mag = NULL;
		ccp->cc_empty_mag = NULL;
		mutex_exit(&ccp->cc_lock);

		if (mp)
			kmem_magazine_destroy(cp, mp, rounds);
		if (fmp)
			kmem_magazine_destroy(cp, fmp, magsize);
		if (emp)
			kmem_magazine_destroy(cp, emp, 0);
	}

	mutex_enter(&cp->cache_depot_lock);
	cp->cache_fmag_min = cp->cache_fmag_reaplimit = cp->cache_fmag_total;
	cp->cache_emag_min = cp->cache_emag_reaplimit = cp->cache_emag_total;
	mutex_exit(&cp->cache_depot_lock);

	kmem_cache_reap(cp);
}

static void
kmem_cache_magazine_setup(kmem_cache_t *cp, kmem_cache_t *mcp)
{
	if (cp->cache_flags & KMF_NOMAGAZINE)
		return;

	mutex_enter(&cp->cache_depot_lock);
	cp->cache_magazine_cache = mcp;
	cp->cache_magazine_size = KMEM_MAGAZINE_SIZE(mcp);
	cp->cache_depot_contention_last = cp->cache_depot_contention + INT_MAX;
	mutex_exit(&cp->cache_depot_lock);
}

/*
 * Enable per-cpu magazines on a cache.
 */
static void
kmem_cache_magazine_enable(kmem_cache_t *cp)
{
	int cpu_seqid;

	for (cpu_seqid = 0; cpu_seqid < max_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		mutex_enter(&ccp->cc_lock);
		ccp->cc_magsize = cp->cache_magazine_size;
		mutex_exit(&ccp->cc_lock);
	}

}

/*
 * Recompute a cache's magazine size.  The trade-off is that larger magazines
 * provide a higher transfer rate with the global layer, while smaller
 * magazines reduce memory consumption.  Magazine resizing is an expensive
 * operation; it should not be done frequently.  Changes to the magazine size
 * are serialized by the kmem_taskq lock.
 *
 * Note: at present this only grows the magazine size.  It might be useful
 * to allow shrinkage too.
 */
static void
kmem_magazine_resize(kmem_cache_t *cp)
{
	ASSERT(RW_LOCK_HELD(taskq_lock(kmem_taskq)));

	if (cp->cache_magazine_size < cp->cache_magazine_maxsize) {
		kmem_cache_t *mcp = cp->cache_magazine_cache->cache_next;
		kmem_cache_magazine_purge(cp);
		kmem_cache_magazine_setup(cp, mcp);
		kmem_cache_magazine_enable(cp);
	}
}

/*
 * Rescale a cache's hash table, so that the table size is roughly the
 * cache size.  We want the average lookup time to be extremely small.
 */
static void
kmem_hash_rescale(kmem_cache_t *cp)
{
	kmem_bufctl_t **old_table, **new_table, *bcp;
	size_t old_size, new_size, h;

	ASSERT(RW_LOCK_HELD(taskq_lock(kmem_taskq)));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_HASH_RESCALE_START,
		"kmem_hash_rescale_start:cache %S buftotal %d",
		cp->cache_name, cp->cache_buftotal);

	new_size = MAX(KMEM_HASH_INITIAL,
	    1 << (highbit(3 * cp->cache_buftotal + 4) - 2));
	old_size = cp->cache_hash_mask + 1;

	if ((old_size >> 1) <= new_size && new_size <= (old_size << 1))
		return;

	new_table = kmem_zalloc(new_size * sizeof (void *), KM_NOSLEEP);
	if (new_table == NULL)
		return;

	mutex_enter(&cp->cache_lock);

	old_size = cp->cache_hash_mask + 1;
	old_table = cp->cache_hash_table;

	cp->cache_hash_mask = new_size - 1;
	cp->cache_hash_table = new_table;
	cp->cache_rescale++;

	for (h = 0; h < old_size; h++) {
		bcp = old_table[h];
		while (bcp != NULL) {
			void *addr = bcp->bc_addr;
			kmem_bufctl_t *next_bcp = bcp->bc_next;
			kmem_bufctl_t **hash_bucket = KMEM_HASH(cp, addr);
			bcp->bc_next = *hash_bucket;
			*hash_bucket = bcp;
			bcp = next_bcp;
		}
	}

	mutex_exit(&cp->cache_lock);

	if (old_table != cp->cache_hash0)
		kmem_free(old_table, old_size * sizeof (void *));

	TRACE_0(TR_FAC_KMEM, TR_KMEM_HASH_RESCALE_END, "kmem_hash_rescale_end");
}

/*
 * Perform periodic maintenance on a cache: hash rescaling,
 * depot working-set update, and magazine resizing.
 */
static void
kmem_cache_update(kmem_cache_t *cp)
{
	int need_hash_rescale = 0;
	int need_magazine_resize = 0;

	mutex_enter(&cp->cache_lock);

	/*
	 * If the cache has become much larger or smaller than its hash table,
	 * fire off a request to rescale the hash table.
	 */
	if ((cp->cache_flags & KMF_HASH) &&
	    (cp->cache_buftotal > (cp->cache_hash_mask << 1) ||
	    (cp->cache_buftotal < (cp->cache_hash_mask >> 1) &&
	    cp->cache_hash_mask > KMEM_HASH_INITIAL)))
		need_hash_rescale = 1;

	mutex_enter(&cp->cache_depot_lock);

	/*
	 * Update the depot working set sizes
	 */
	cp->cache_fmag_reaplimit = cp->cache_fmag_min;
	cp->cache_fmag_min = cp->cache_fmag_total;

	cp->cache_emag_reaplimit = cp->cache_emag_min;
	cp->cache_emag_min = cp->cache_emag_total;

	/*
	 * If there's a lot of contention in the depot,
	 * increase the magazine size.
	 */
	if (cp->cache_magazine_size < cp->cache_magazine_maxsize &&
	    cp->cache_depot_contention - cp->cache_depot_contention_last >
	    kmem_depot_contention)
		need_magazine_resize = 1;

	cp->cache_depot_contention_last = cp->cache_depot_contention;

	mutex_exit(&cp->cache_depot_lock);
	mutex_exit(&cp->cache_lock);

	if (need_hash_rescale)
		KMEM_TASKQ_DISPATCH(kmem_hash_rescale, cp);
	if (need_magazine_resize)
		KMEM_TASKQ_DISPATCH(kmem_magazine_resize, cp);
}

static void
kmem_update_timeout(void *dummy)
{
	static void kmem_update(void *);

	(void) timeout(kmem_update, dummy, kmem_reap_interval);
}

static void
kmem_update(void *dummy)
{
	kmem_cache_applyall(kmem_cache_update, NULL);

	/*
	 * We use taskq_dispatch() to reschedule the timeout so that
	 * kmem_update() becomes self-throttling: it won't schedule
	 * new tasks until all previous tasks have completed.
	 */
	if (!taskq_dispatch(kmem_taskq, kmem_update_timeout, dummy, KM_NOSLEEP))
		kmem_update_timeout(NULL);
}

static int
kmem_cache_kstat_update(kstat_t *ksp, int rw)
{
	struct kmem_cache_kstat *kmcp = &kmem_cache_kstat;
	kmem_cache_t *cp = ksp->ks_private;
	kmem_slab_t *sp;
	int cpu_buf_avail;
	int buf_avail = 0;
	int cpu_seqid;

	ASSERT(MUTEX_HELD(&kmem_cache_kstat_lock));

	if (rw == KSTAT_WRITE)
		return (EACCES);

	mutex_enter(&cp->cache_lock);

	kmcp->kmc_alloc_fail.value.ul		= cp->cache_alloc_fail;
	kmcp->kmc_alloc.value.ul		= cp->cache_global_alloc;
	kmcp->kmc_free.value.ul			= cp->cache_global_free;
	kmcp->kmc_global_alloc.value.ul		= cp->cache_global_alloc;
	kmcp->kmc_global_free.value.ul		= cp->cache_global_free;

	for (cpu_seqid = 0; cpu_seqid < max_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];

		mutex_enter(&ccp->cc_lock);

		cpu_buf_avail = 0;
		if (ccp->cc_rounds > 0)
			cpu_buf_avail += ccp->cc_rounds;
		if (ccp->cc_full_mag)
			cpu_buf_avail += ccp->cc_magsize;

		kmcp->kmc_alloc.value.ul	+= ccp->cc_alloc;
		kmcp->kmc_free.value.ul		+= ccp->cc_free;
		buf_avail			+= cpu_buf_avail;

		mutex_exit(&ccp->cc_lock);
	}

	mutex_enter(&cp->cache_depot_lock);

	kmcp->kmc_depot_alloc.value.ul		= cp->cache_depot_alloc;
	kmcp->kmc_depot_free.value.ul		= cp->cache_depot_free;
	kmcp->kmc_depot_contention.value.ul	= cp->cache_depot_contention;
	kmcp->kmc_empty_magazines.value.ul	= cp->cache_emag_total;
	kmcp->kmc_full_magazines.value.ul	= cp->cache_fmag_total;
	kmcp->kmc_magazine_size.value.ul	= cp->cache_magazine_size;

	kmcp->kmc_alloc.value.ul		+= cp->cache_depot_alloc;
	kmcp->kmc_free.value.ul			+= cp->cache_depot_free;
	buf_avail += cp->cache_fmag_total * cp->cache_magazine_size;

	mutex_exit(&cp->cache_depot_lock);

	kmcp->kmc_buf_size.value.ul	= cp->cache_bufsize;
	kmcp->kmc_align.value.ul	= cp->cache_align;
	kmcp->kmc_chunk_size.value.ul	= cp->cache_chunksize;
	kmcp->kmc_slab_size.value.ul	= cp->cache_slabsize;
	if (cp->cache_constructor != NULL && !(cp->cache_flags & KMF_DEADBEEF))
		kmcp->kmc_buf_constructed.value.ul = buf_avail;
	else
		kmcp->kmc_buf_constructed.value.ul = 0;
	for (sp = cp->cache_freelist; sp != &cp->cache_nullslab;
	    sp = sp->slab_next)
		buf_avail += sp->slab_chunks - sp->slab_refcnt;
	kmcp->kmc_buf_avail.value.ul	= buf_avail;
	kmcp->kmc_buf_inuse.value.ul	= cp->cache_buftotal - buf_avail;
	kmcp->kmc_buf_total.value.ul	= cp->cache_buftotal;
	kmcp->kmc_buf_max.value.ul	= cp->cache_bufmax;
	kmcp->kmc_slab_create.value.ul	= cp->cache_slab_create;
	kmcp->kmc_slab_destroy.value.ul	= cp->cache_slab_destroy;
	kmcp->kmc_hash_size.value.ul	= (cp->cache_flags & KMF_HASH) ?
	    cp->cache_hash_mask + 1 : 0;
	kmcp->kmc_hash_lookup_depth.value.ul = cp->cache_lookup_depth;
	kmcp->kmc_hash_rescale.value.ul	= cp->cache_rescale;
	kmcp->kmc_vmem_source.value.ul	= cp->cache_arena->vm_id;

	mutex_exit(&cp->cache_lock);
	return (0);
}

/*
 * Return a named statistic about a particular cache.
 * This shouldn't be called very often, so it's currently designed for
 * simplicity (leverages existing kstat support) rather than efficiency.
 */
ulong_t
kmem_cache_stat(kmem_cache_t *cp, char *name)
{
	int i;
	kstat_t *ksp = cp->cache_kstat;
	kstat_named_t *knp = (kstat_named_t *)&kmem_cache_kstat;
	ulong_t value = 0;

	if (ksp != NULL) {
		mutex_enter(&kmem_cache_kstat_lock);
		(void) kmem_cache_kstat_update(ksp, KSTAT_READ);
		for (i = 0; i < ksp->ks_ndata; i++) {
			if (strcmp(knp[i].name, name) == 0) {
				value = knp[i].value.ul;
				break;
			}
		}
		mutex_exit(&kmem_cache_kstat_lock);
	}
	return (value);
}

static void
kmem_cache_kstat_create(kmem_cache_t *cp)
{
	if ((cp->cache_kstat = kstat_create("unix", 0, cp->cache_name,
	    "kmem_cache", KSTAT_TYPE_NAMED,
	    sizeof (kmem_cache_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL)) != NULL) {
		cp->cache_kstat->ks_data = &kmem_cache_kstat;
		cp->cache_kstat->ks_update = kmem_cache_kstat_update;
		cp->cache_kstat->ks_private = cp;
		cp->cache_kstat->ks_lock = &kmem_cache_kstat_lock;
		kstat_install(cp->cache_kstat);
	}
}

kmem_cache_t *
kmem_cache_create(
	char *name,		/* descriptive name for this cache */
	size_t bufsize,		/* size of the objects it manages */
	int align,		/* required object alignment */
	int (*constructor)(void *, void *, int), /* object constructor */
	void (*destructor)(void *, void *),	/* object destructor */
	void (*reclaim)(void *), /* memory reclaim callback */
	void *private,		/* pass-thru arg for constr/destr/reclaim */
	vmem_t *vmp,		/* vmem source for slab allocation */
	int cflags)		/* cache creation flags */
{
	int cpu_seqid;
	int chunksize;
	kmem_cache_t *cp, *cnext, *cprev;
	kmem_magazine_type_t *mtp;
	size_t csize = KMEM_CACHE_SIZE(max_ncpus);

	if (vmp == NULL)
		vmp = kmem_default_arena;

	/*
	 * Make sure that cp->cache_cpu[] is nicely aligned
	 * to prevent false sharing of cache lines.
	 */
	cp = vmem_xalloc(kmem_cache_arena, csize, KMEM_CPU_CACHE_SIZE,
	    P2NPHASE(csize, KMEM_CPU_CACHE_SIZE), 0, NULL, NULL, VM_SLEEP);
	bzero(cp, csize);

	(void) strncpy(cp->cache_name, name, KMEM_CACHE_NAMELEN);
	mutex_init(&cp->cache_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&cp->cache_depot_lock, NULL, MUTEX_DEFAULT, NULL);

	for (cpu_seqid = 0; cpu_seqid < max_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		mutex_init(&ccp->cc_lock, NULL, MUTEX_DEFAULT, NULL);
		ccp->cc_rounds = -1;	/* no current magazine */
	}

	if (align == 0)
		align = KMEM_ALIGN;

	/*
	 * If we're not at least KMEM_ALIGN aligned, we can't use free
	 * memory to hold bufctl information (because we can't safely
	 * perform word loads and stores on it).
	 */
	if (align < KMEM_ALIGN)
		cflags |= KMC_NOTOUCH;

	ASSERT(!(cflags & KMC_NOHASH) || !(cflags & KMC_NOTOUCH));

	if (!(cflags & (KMC_NODEBUG | KMC_NOTOUCH))) {
		mutex_enter(&kmem_flags_lock);
		if (kmem_flags & KMF_RANDOMIZE)
			kmem_flags = (((kmem_flags | ~KMF_DBFLAGS) + 1) &
			    KMF_DBFLAGS) | KMF_RANDOMIZE;
		cp->cache_flags = kmem_flags | (cflags & KMF_DBFLAGS);
		mutex_exit(&kmem_flags_lock);
		/*
		 * Make sure flags are reasonable
		 */
		if (cp->cache_flags & KMF_LITE) {
			cp->cache_flags |= (KMF_DEADBEEF | KMF_REDZONE);
			cp->cache_flags &= ~(KMF_AUDIT | KMF_CONTENTS);
		}
		if (cp->cache_flags & KMF_PAGEPERBUF)
			cp->cache_flags |= KMF_NOMAGAZINE;
	}

	if (cflags & KMC_NOMAGAZINE)
		cp->cache_flags |= KMF_NOMAGAZINE;

	if (cp->cache_flags & KMF_PAGEPERBUF)
		align = vmp->vm_quantum;

	if ((align & (align - 1)) || align > vmp->vm_quantum)
		panic("kmem_cache_create: bad alignment %d", align);

	if ((cp->cache_flags & KMF_LITE) &&
	    (bufsize < kmem_lite_minsize || align > kmem_lite_maxalign ||
	    P2PHASE(bufsize, kmem_lite_maxalign) == 0))
		cp->cache_flags &= ~(KMF_LITE | KMF_BUFTAG);

	if (align >= KMEM_ALIGN) {
		chunksize = P2ROUNDUP((int)bufsize, KMEM_ALIGN);
		cp->cache_offset = chunksize - KMEM_ALIGN;
	} else {
		ASSERT(!(cp->cache_flags & KMF_BUFTAG));
		chunksize = (int)bufsize;
		cp->cache_offset = chunksize;
	}
	if (cp->cache_flags & KMF_BUFTAG) {
		ASSERT(align >= KMEM_ALIGN);
		cp->cache_offset = chunksize;
		chunksize += (int)sizeof (kmem_buftag_t);
		if (cp->cache_flags & KMF_LITE)
			chunksize -= (int)(sizeof (kmem_bufctl_info_t) -
			    sizeof (kmem_caller_info_t));
	}
	chunksize = P2ROUNDUP(chunksize, align);

	cp->cache_bufsize	= (int)bufsize;
	cp->cache_chunksize	= chunksize;
	cp->cache_align		= align;
	cp->cache_constructor	= constructor;
	cp->cache_destructor	= destructor;
	cp->cache_reclaim	= reclaim;
	cp->cache_private	= private;
	cp->cache_arena		= vmp;
	cp->cache_freelist	= &cp->cache_nullslab;
	cp->cache_cflags	= cflags;
	cp->cache_ncpus		= max_ncpus;
	cp->cache_nullslab.slab_cache = cp;
	cp->cache_nullslab.slab_refcnt = -1;
	cp->cache_nullslab.slab_next = &cp->cache_nullslab;
	cp->cache_nullslab.slab_prev = &cp->cache_nullslab;

	for (mtp = kmem_magazine_type; chunksize <= mtp->mt_maxbuf; mtp++)
		continue;
	cp->cache_magazine_maxsize = mtp->mt_magsize;

	for (mtp = kmem_magazine_type; chunksize <= mtp->mt_minbuf; mtp++)
		continue;

	if (mtp->mt_cache != NULL) {
		kmem_cache_magazine_setup(cp, mtp->mt_cache);
		kmem_cache_magazine_enable(cp);
	}

	if ((cflags & KMC_NOHASH) || (!(cflags & KMC_NOTOUCH) &&
	    (!(cp->cache_flags & KMF_BUFTAG) || (cp->cache_flags & KMF_LITE)) &&
	    chunksize < kmem_minhash)) {
		cp->cache_slabsize = vmp->vm_quantum;
		cp->cache_maxcolor =
		    (cp->cache_slabsize - sizeof (kmem_slab_t)) % chunksize;
		ASSERT(chunksize + sizeof (kmem_slab_t) <= cp->cache_slabsize);
		cp->cache_flags &= ~KMF_AUDIT;
	} else {
		int chunks, bestfit, waste, slabsize;
		int minwaste = INT_MAX;

		for (chunks = 1; chunks <= KMEM_VOID_FRACTION; chunks++) {
			slabsize = P2ROUNDUP(chunksize * chunks,
			    vmp->vm_quantum);
			chunks = slabsize / chunksize;
			waste = (slabsize % chunksize) / chunks;
			if (waste < minwaste) {
				minwaste = waste;
				bestfit = slabsize;
			}
		}
		if (cflags & KMC_QCACHE)
			bestfit = MAX(1 << highbit(3 * vmp->vm_qcache_max), 64);
		cp->cache_slabsize = bestfit;
		cp->cache_maxcolor = bestfit % chunksize;
		cp->cache_flags |= KMF_HASH;
		cp->cache_bufctl_cache = (cp->cache_flags & KMF_AUDIT) ?
		    kmem_bufctl_audit_cache : kmem_bufctl_cache;
	}

	cp->cache_hash_table = cp->cache_hash0;
	cp->cache_hash_mask = KMEM_HASH_INITIAL - 1;
	cp->cache_hash_shift = highbit((ulong_t)chunksize) - 1;

	if (cp->cache_maxcolor >= vmp->vm_quantum)
		cp->cache_maxcolor = vmp->vm_quantum - 1;

	mutex_enter(&kmem_cache_lock);
	cp->cache_next = cnext = &kmem_null_cache;
	cp->cache_prev = cprev = kmem_null_cache.cache_prev;
	cnext->cache_prev = cp;
	cprev->cache_next = cp;
	mutex_exit(&kmem_cache_lock);

	if (kmem_ready) {
		if (kmem_taskq)
			KMEM_TASKQ_DISPATCH(kmem_cache_kstat_create, cp);
		else
			kmem_cache_kstat_create(cp);
	}

	return (cp);
}

void
kmem_cache_destroy(kmem_cache_t *cp)
{
	int cpu_seqid;

	/*
	 * Remove the cache from the global cache list so that no one else
	 * can schedule tasks on its behalf, wait for any pending tasks to
	 * complete, purge the cache, and then destroy it.
	 */
	mutex_enter(&kmem_cache_lock);
	cp->cache_prev->cache_next = cp->cache_next;
	cp->cache_next->cache_prev = cp->cache_prev;
	mutex_exit(&kmem_cache_lock);

	if (kmem_taskq != NULL)
		taskq_wait(kmem_taskq);

	kmem_cache_magazine_purge(cp);

	mutex_enter(&cp->cache_lock);
	if (cp->cache_buftotal != 0)
		cmn_err(CE_WARN, "kmem_cache_destroy: '%s' (%p) not empty",
		    cp->cache_name, (void *)cp);
	cp->cache_reclaim = NULL;
	/*
	 * The cache is now dead.  There should be no further activity.
	 * We enforce this by setting land mines in the constructor and
	 * destructor routines that induce a kernel text fault if invoked.
	 */
	cp->cache_constructor = (int (*)(void *, void *, int))1;
	cp->cache_destructor = (void (*)(void *, void *))2;
	mutex_exit(&cp->cache_lock);

	if (cp->cache_kstat)
		kstat_delete(cp->cache_kstat);

	if (cp->cache_hash_table != cp->cache_hash0)
		kmem_free(cp->cache_hash_table,
		    (cp->cache_hash_mask + 1) * sizeof (void *));

	for (cpu_seqid = 0; cpu_seqid < max_ncpus; cpu_seqid++)
		mutex_destroy(&cp->cache_cpu[cpu_seqid].cc_lock);

	mutex_destroy(&cp->cache_depot_lock);
	mutex_destroy(&cp->cache_lock);

	vmem_free(kmem_cache_arena, cp, KMEM_CACHE_SIZE(max_ncpus));
}

/*ARGSUSED*/
static int
kmem_cpu_setup(cpu_setup_t what, int id, void *arg)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (what == CPU_UNCONFIG) {
		kmem_cache_applyall(kmem_cache_magazine_purge, kmem_taskq);
		kmem_cache_applyall(kmem_cache_magazine_enable, kmem_taskq);
	}
	return (0);
}

static void
kmem_cache_init(int pass)
{
	int i, size;
	kmem_cache_t *cp;
	kmem_magazine_type_t *mtp;
	char name[KMEM_CACHE_NAMELEN + 1];

	for (i = 0; i < sizeof (kmem_magazine_type) / sizeof (*mtp); i++) {
		mtp = &kmem_magazine_type[i];
		(void) sprintf(name, "kmem_magazine_%d", mtp->mt_magsize);
		mtp->mt_cache = kmem_cache_create(name,
			(mtp->mt_magsize + 1) * sizeof (void *),
			mtp->mt_align, NULL, NULL, NULL, NULL,
			kmem_internal_arena,
			kmem_self_debug | KMC_NOHASH | KMC_NOMAGAZINE);
	}

	kmem_slab_cache = kmem_cache_create("kmem_slab_cache",
	    sizeof (kmem_slab_t), 32, NULL, NULL, NULL, NULL,
	    kmem_internal_arena, kmem_self_debug | KMC_NOHASH);

	kmem_bufctl_cache = kmem_cache_create("kmem_bufctl_cache",
	    sizeof (kmem_bufctl_t), 16, NULL, NULL, NULL, NULL,
	    kmem_internal_arena, kmem_self_debug | KMC_NOHASH);

	kmem_bufctl_audit_cache = kmem_cache_create("kmem_bufctl_audit_cache",
	    sizeof (kmem_bufctl_audit_t), 32, NULL, NULL, NULL, NULL,
	    kmem_internal_arena, kmem_self_debug | KMC_NOHASH);

	if (pass == 2) {
		kmem_va_arena = vmem_create("kmem_va", NULL, 0,
		    PAGESIZE, vmem_alloc, vmem_free, heap_arena,
		    8 * PAGESIZE, VM_SLEEP);
		kmem_default_arena = vmem_create("kmem_default", NULL, 0,
		    PAGESIZE, segkmem_alloc, segkmem_free, kmem_va_arena,
		    0, VM_SLEEP);
	}

	/*
	 * Set up the default caches to back kmem_alloc()
	 */
	size = KMEM_ALIGN;
	for (i = 0; i < sizeof (kmem_alloc_sizes) / sizeof (int); i++) {
		int align = KMEM_ALIGN;
		int cache_size = kmem_alloc_sizes[i];
		if ((cache_size & PAGEOFFSET) == 0)
			align = PAGESIZE;
		(void) sprintf(name, "kmem_alloc_%d", cache_size);
		cp = kmem_cache_create(name, cache_size, align,
			NULL, NULL, NULL, NULL, NULL, 0);
		while (size <= cache_size) {
			kmem_alloc_table[(size - 1) >> KMEM_ALIGN_SHIFT] = cp;
			size += KMEM_ALIGN;
		}
	}
}

void
kmem_init(void)
{
	kmem_cache_t *cp;
	int old_kmem_flags = kmem_flags;

	/*
	 * Small-memory systems (< 24 MB) can't handle kmem_flags overhead.
	 */
	if (physmem < btop(24 << 20) && !(old_kmem_flags & KMF_STICKY))
		kmem_flags = 0;

	kmem_null_cache.cache_next = &kmem_null_cache;
	kmem_null_cache.cache_prev = &kmem_null_cache;

	kmem_internal_arena = vmem_create("kmem_internal", NULL, 0, PAGESIZE,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	kmem_default_arena = kmem_internal_arena;

	kmem_cache_arena = vmem_create("kmem_cache", NULL, 0, KMEM_ALIGN,
	    vmem_alloc, vmem_free, kmem_internal_arena, 0, VM_SLEEP);

	kmem_log_arena = vmem_create("kmem_log", NULL, 0, KMEM_ALIGN,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	kmem_oversize_arena = vmem_create("kmem_oversize", NULL, 0, PAGESIZE,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	kmem_null_cache.cache_next = &kmem_null_cache;
	kmem_null_cache.cache_prev = &kmem_null_cache;

	kmem_reap_interval = 15 * hz;

	/*
	 * Use about 2% (1/50) of available memory for
	 * transaction and content logging (if enabled).
	 */
	kmem_log_size = kmem_maxavail() / 50;

	/*
	 * Read /etc/system.  This is a chicken-and-egg problem because
	 * kmem_flags may be set in /etc/system, but mod_read_system_file()
	 * needs to use the allocator.  The simplest solution is to create
	 * all the standard kmem caches, read /etc/system, destroy all the
	 * caches we just created, and then create them all again in light
	 * of the (possibly) new kmem_flags and other kmem tunables.
	 */
	kmem_cache_init(1);

	mod_read_system_file(boothowto & RB_ASKNAME);

	while ((cp = kmem_null_cache.cache_prev) != &kmem_null_cache)
		kmem_cache_destroy(cp);

	if (old_kmem_flags & KMF_STICKY)
		kmem_flags = old_kmem_flags;

	kmem_cache_init(2);

	if (kmem_flags & (KMF_AUDIT | KMF_RANDOMIZE))
		kmem_transaction_log = kmem_log_init(kmem_log_size);

	if (kmem_flags & (KMF_CONTENTS | KMF_RANDOMIZE))
		kmem_content_log = kmem_log_init(kmem_log_size);

	kmem_failure_log = kmem_log_init(kmem_faillog_size);

	kmem_slab_log = kmem_log_init(kmem_slablog_size);

	/*
	 * Initialize STREAMS message caches so allocb() is available.
	 * This allows us to initialize the logging framework (cmn_err(9F),
	 * strlog(9F), etc) so we can start recording messages.
	 */
	streams_msg_init();
	log_init();
	taskq_init();

	/*
	 * Initialize the kstat framework so we can create kstats for
	 * all the caches we just created.
	 */
	kstat_init();

	kmem_cache_applyall(kmem_cache_kstat_create, NULL);

	kmem_ready = 1;

	/*
	 * Initialize the virtual memory allocator's kstats.
	 */
	vmem_kstat_init(kmem_flags & KMF_AUDIT);

	/*
	 * Initialize the platform-specific aligned/DMA memory allocator.
	 */
	ka_init();

	/*
	 * Initialize 32-bit ID cache.
	 */
	id32_init();
}

void
kmem_thread_init(void)
{
	kmem_taskq = taskq_create("kmem_taskq", 1, minclsyspri,
	    200, INT_MAX, 0);
}

void
kmem_mp_init(void)
{
	mutex_enter(&cpu_lock);
	register_cpu_setup_func(kmem_cpu_setup, NULL);
	mutex_exit(&cpu_lock);

	kmem_update_timeout(NULL);
}
