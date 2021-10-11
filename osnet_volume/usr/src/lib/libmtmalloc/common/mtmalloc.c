/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mtmalloc.c	1.6	98/07/21 SMI"

#include <mtmalloc.h>
#include "mtmalloc_impl.h"
#include <unistd.h>
#include <synch.h>
#include <thread.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

/*
 * To turn on the asserts just compile -DDEBUG
 */
#ifndef	DEBUG
#define	NDEBUG
#endif
#include <assert.h>

/*
 * The MT hot malloc implementation contained herein is designed to complement
 * the libc version of malloc. It is not intended to replace that implementation
 * until we decide that it is ok to break customer apps (Solaris 3.0).
 *
 * The allocator is a power of 2 allocator up to 2^^16. Sizes greater than that
 * go on a singly locked, doubly linked list. There are few allocations of that
 * large size.
 *
 * The basic allocator initializes itself into NCPUS worth of chains of caches.
 * When a memory request is made, the calling thread is vectored into one of
 * NCPUS worth of caches. The mechanism to do this is a simple turnstile. This
 * turnstile is protected by guaranteeing that the rotor does not reference
 * beyond the end of the array.  Note that there is no strong binding
 * between cpus and memory. NCPUS worth of cache is allocated to provide a
 * maximum level of concurrency during an allocation.
 *
 * Once the thread is vectored into one of the list of caches the real
 * allocation of the memory begins. The size is determined to figure out which
 * bucket the allocation should be satisfied from. The management of free
 * buckets is done via a bitmask. A free bucket is represented by a 1. The
 * first free bit represents the first free bucket. The position of the bit,
 * represents the position of the bucket in the arena.
 *
 * When the memory from the arena is handed out, the address of the cache
 * control structure is written in the word preceeding the returned memory.
 * This cache * control address is used during free() to mark the buffer free
 * in the cache control structure.
 *
 * When all available memory in a cache has been depleted, a new chunk of memory
 * is allocated via sbrk(). The new cache is allocated from this chunk of memory
 * and initialized in the function create_cache(). New caches are installed at
 * the front of a singly linked list of the same size memory pools. This helps
 * to ensure that there will tend to be available memory in the beginning of the
 * list.
 *
 * Long linked lists hurt performance. To decrease this effect, there is a
 * tunable, requestsize, that bumps up the sbrk allocation size and thus
 * increases the number of available blocks within an arena.
 *
 * An oversize allocation has two bits of overhead. There is the OVERHEAD
 * used to hold the cache addr, the &oversize_list, plus an oversize_t
 * structure to further describe the block.
 */

#if defined(i386) || defined(__i386)
#include <arpa/inet.h>	/* for htonl() */
#endif

static void * morecore(size_t);
static void setup_caches(void);
static void init_cpu_list(percpu_t *, int32_t);
static void init_caches(percpu_t *, int32_t, caddr_t);
static void create_cache(cache_t *, size_t);
static void * malloc_internal(size_t, percpu_t *);
static void * oversize(size_t);
static void * find_oversize(size_t);
static void add_oversize(oversize_t *);
static void copy_pattern(uint32_t, void *, size_t);
static void * verify_pattern(uint32_t, void *, size_t);

#define	ALIGN(x, a)	((((uintptr_t)(x) + ((uintptr_t)(a) - 1)) \
			& ~((uintptr_t)(a) - 1)))

/* need this to deal with little endianess of x86 */
#if defined(i386) || defined(__i386)
#define	FLIP_EM(x)	htonl((x))
#else
#define	FLIP_EM(x)	(x)
#endif

#define	OVERHEAD	8	/* size needed to write cache addr */
#define	HUNKSIZE	8192	/* just a multiplier */
#define	MAX_CACHED	1<<16	/* 64K is the max */
#define	MINSIZE		9	/* for requestsize, tunable */
#define	MAXSIZE		256	/* arbitrary, big enough, for requestsize */

#define	FREEPATTERN	0xdeadbeef /* debug fill pattern for free buf */
#define	INITPATTERN	0xbaddcafe /* debug fill pattern for new buf */

static long requestsize = MINSIZE; /* 9 pages per cache; tunable; 9 is min */
static int32_t cachesizes[] = {1<<4, 1<<5, 1<<6, 1<<7, 1<<8, 1<<9, 1<<10, 1<<11,
				1<<12, 1<<13, 1<<14, 1<<15, 1<<16};
static int32_t ncaches = sizeof (cachesizes)/sizeof (int32_t);
static int32_t ncpus;
static int32_t debugopt;

static percpu_t *cpu_list;
static oversize_t oversize_list;
static mutex_t oversize_lock;

void *
malloc(size_t bytes)
{
	percpu_t *list_rotor;
	static int32_t cpuptr;
	int32_t	list_index;

	/*
	 * this test is due to linking with libthread.
	 * There are malloc calls prior to this library
	 * being initialized.
	 */
	if (cpu_list == (percpu_t *)NULL)
		setup_caches();

	if (bytes > MAX_CACHED)
		return (oversize(bytes));

	list_index = cpuptr + 1;

	if (list_index >= ncpus)
		list_index = 0;

	cpuptr = list_index;

	assert(list_index < ncpus);
	list_rotor = &cpu_list[list_index];

	return (malloc_internal(bytes, list_rotor));
}

void *
realloc(void * ptr, size_t bytes)
{
	void * new;
	cache_t *cacheptr;
	caddr_t mem;

	if (ptr == NULL)
		return (malloc(bytes));

	if (bytes == 0) {
		free(ptr);
		return (NULL);
	}

	mem = (caddr_t)ptr - OVERHEAD;

	new = malloc(bytes);

	if (new == NULL)
		return (NULL);

	if (*(uintptr_t *)mem == (uintptr_t)&oversize_list) {
		oversize_t *old;

		old = (oversize_t *)(mem - sizeof (oversize_t));
		(void) memcpy(new, ptr, MIN(bytes, old->mt_size));
		free(ptr);
		return (new);
	}

	cacheptr = (cache_t *)mem;

	(void) memcpy(new, ptr, MIN(cacheptr->mt_size - OVERHEAD, bytes));
	free(ptr);

	return (new);
}

void *
calloc(size_t nelem, size_t bytes)
{
	void * ptr;
	size_t size = nelem * bytes;

	ptr = malloc(size);
	if (ptr == NULL)
		return (NULL);
	bzero(ptr, size);

	return (ptr);
}

void
free(void * ptr)
{
	cache_t *cacheptr;
	caddr_t mem;
	int32_t i;
	caddr_t freeblocks;
	uintptr_t offset;
	uchar_t mask;
	int32_t which_bit, num_bytes;

	if (ptr == NULL)
		return;

	mem = (caddr_t)ptr - OVERHEAD;

	if (*(uintptr_t *)mem == (uintptr_t)&oversize_list) {
		oversize_t *big;

		big = (oversize_t *)(mem - sizeof (oversize_t));
		(void) mutex_lock(&oversize_lock);
		if (debugopt & MTDEBUGPATTERN)
			copy_pattern(FREEPATTERN, ptr, big->mt_size);
		add_oversize(big);
		(void) mutex_unlock(&oversize_lock);
		return;
	}

	cacheptr = (cache_t *)*(uintptr_t *)mem;
	freeblocks = cacheptr->mt_freelist;

	/*
	 * This is the distance measured in bits into the arena.
	 * The value of offset is in bytes but there is a 1-1 correlation
	 * between distance into the arena and distance into the
	 * freelist bitmask.
	 */
	offset = mem - cacheptr->mt_arena;

	/*
	 * i is total number of bits to offset into freelist bitmask.
	 */
	i = offset / cacheptr->mt_size;

	num_bytes = i >> 3;

	/*
	 * which_bit is the bit offset into the byte in the freelist.
	 * if our freelist bitmask looks like 0xf3 and we are freeing
	 * block 5 (ie: the 6th block) our mask will be 0xf7 after
	 * the free. Things go left to right that's why the mask is 0x80
	 * and not 0x01.
	 */
	which_bit = i - (num_bytes << 3);

	mask = 0x80 >> which_bit;

	freeblocks += num_bytes;

	if (debugopt & MTDEBUGPATTERN)
		copy_pattern(FREEPATTERN, ptr, cacheptr->mt_size - OVERHEAD);

	(void) mutex_lock(&cacheptr->mt_cache_lock);

	if (*freeblocks & mask && !(debugopt & MTDOUBLEFREE))
		abort();

	*freeblocks |= mask;

	cacheptr->mt_nfree++;
	(void) mutex_unlock(&cacheptr->mt_cache_lock);
}

void
mallocctl(int cmd, long value)
{
	switch (cmd) {

	case MTDOUBLEFREE:
	case MTDEBUGPATTERN:
	case MTINITBUFFER:
		if (value)
			debugopt |= cmd;
		else
			debugopt &= ~cmd;
		break;
	case MTCHUNKSIZE:
		if (value > MINSIZE && value < MAXSIZE)
			requestsize = value;
		break;
	default:
		break;
	}
}

static void
setup_caches(void)
{
	int32_t cache_space_needed;
	size_t newbrk;

	if ((ncpus = sysconf(_SC_NPROCESSORS_CONF)) < 0)
		ncpus = 4; /* decent default value */

	if (sbrk(0) == (void *)-1) {
		perror("sbrk"); /* we're doomed */
		exit(-1);
	}

	cache_space_needed = (ncpus * sizeof (percpu_t)) +
			(ncpus * requestsize * HUNKSIZE * ncaches);

	newbrk = ALIGN(cache_space_needed, HUNKSIZE);
	cpu_list = (percpu_t *)morecore(newbrk);
	if (cpu_list == (percpu_t *)-1) {
		perror("sbrk"); /* we're doomed */
		exit(-1);
	}

	init_cpu_list(cpu_list, ncpus);

	oversize_list.mt_next = oversize_list.mt_prev = &oversize_list;
	oversize_list.mt_addr = NULL;
	oversize_list.mt_size = 0;
}

static void
init_cpu_list(percpu_t *list, int32_t ncpus)
{
	int32_t i;
	caddr_t cache_addr = ((caddr_t)list + (sizeof (percpu_t) * ncpus));

	for (i = 0; i < ncpus; i++) {
		list[i].mt_caches = (cache_t **)cache_addr;
		bzero(&list[i].mt_parent_lock, sizeof (mutex_t));
		cache_addr += ncaches * sizeof (cache_t *);
	}
	init_caches(list, ncpus, cache_addr);
}

static void
init_caches(percpu_t *list, int32_t ncpus, caddr_t addr)
{
	int32_t i, j;
	cache_t ** thiscache, * next;

	for (i = 0; i < ncpus; i++) {
		thiscache = list[i].mt_caches;

		for (j = 0; j < ncaches; j++) {
			next = thiscache[j] = (cache_t *)addr;
			create_cache(next, cachesizes[j] + OVERHEAD);
			addr += next->mt_span + sizeof (cache_t);
		}
	}
}

static void
create_cache(cache_t *cp, size_t size)
{
	long nblocks;
	int32_t chunksize = requestsize;

	bzero(&cp->mt_cache_lock, sizeof (mutex_t));
	cp->mt_size = size;
	cp->mt_freelist = ((caddr_t)cp + sizeof (cache_t));
	cp->mt_span = chunksize * HUNKSIZE - sizeof (cache_t);
	cp->mt_hunks = chunksize;
	/*
	 * rough calculation. We will need to adjust later.
	 */
	nblocks = cp->mt_span / cp->mt_size;
	nblocks >>= 3;
	if (nblocks == 0) { /* less than 8 free blocks in this pool */
		int32_t numblocks = 0;
		long i = cp->mt_span;
		size_t sub = cp->mt_size;
		uchar_t mask = 0;

		while (i > sub) {
			numblocks++;
			i -= sub;
		}
		cp->mt_arena = (caddr_t)ALIGN(cp->mt_freelist + 8, 8);
		cp->mt_nfree = numblocks;
		while (numblocks--) {
			mask |= 0x80 >> numblocks;
		}
		*(cp->mt_freelist) = mask;
	} else {
		cp->mt_arena = (caddr_t)ALIGN((caddr_t)cp->mt_freelist +
			nblocks, 32);
		/* recompute nblocks */
		nblocks = (uintptr_t)((caddr_t)cp->mt_freelist +
			cp->mt_span - cp->mt_arena) / cp->mt_size;
		cp->mt_nfree = ((nblocks >> 3) << 3);
		/* Set everything to free */
		(void) memset(cp->mt_freelist, 0xff, nblocks >> 3);
	}

	if (debugopt & MTDEBUGPATTERN)
		copy_pattern(FREEPATTERN, cp->mt_arena, cp->mt_size);

	cp->mt_next = NULL;
}

static void *
malloc_internal(size_t size, percpu_t *cpuptr)
{
	cache_t *thiscache;
	int32_t i, n = 4;
	uint32_t index;
	uint32_t *freeblocks; /* not a uintptr_t on purpose */
	caddr_t ret;

	while (size > (1 << n))
		n++;

	n -= 4;

	(void) mutex_lock(&cpuptr->mt_parent_lock);
	thiscache = cpuptr->mt_caches[n];
	(void) mutex_lock(&thiscache->mt_cache_lock);

	if (thiscache->mt_nfree == 0) {
		cache_t *newcache, *nextcache, *startcache = thiscache;
		int32_t thisrequest = requestsize;

		while ((nextcache = thiscache) != NULL) {
			if (nextcache->mt_nfree)
				break;
			else
				thiscache = thiscache->mt_next;
		}

		if (!nextcache) {
			newcache = (cache_t *)morecore(thisrequest * HUNKSIZE);

			if (newcache == (cache_t *)-1) {
			    (void *)mutex_unlock(&startcache->mt_cache_lock);
			    (void *)mutex_unlock(&cpuptr->mt_parent_lock);
			    return (NULL);
			}
			create_cache(newcache, startcache->mt_size);
			(void) mutex_unlock(&startcache->mt_cache_lock);
			newcache->mt_next = cpuptr->mt_caches[n];
			cpuptr->mt_caches[n] = newcache;
			thiscache = newcache;
			(void) mutex_lock(&thiscache->mt_cache_lock);
		} else {
			(void) mutex_unlock(&startcache->mt_cache_lock);
			thiscache = nextcache;
			(void) mutex_lock(&thiscache->mt_cache_lock);
		}
	}

	freeblocks = (uint32_t *)thiscache->mt_freelist;
	while (freeblocks < (uint32_t *)thiscache->mt_arena) {
		if (*freeblocks & 0xffffffff)
			break;
		freeblocks++;
		if (freeblocks < (uint32_t *)thiscache->mt_arena &&
			*freeblocks & 0xffffffff)
			break;
		freeblocks++;
		if (freeblocks < (uint32_t *)thiscache->mt_arena &&
			*freeblocks & 0xffffffff)
			break;
		freeblocks++;
		if (freeblocks < (uint32_t *)thiscache->mt_arena &&
			*freeblocks & 0xffffffff)
		break;
		freeblocks++;
	}

	/*
	 * the offset from mt_freelist to freeblocks is the offset into
	 * the arena. Be sure to include the offset into freeblocks
	 * of the bitmask. n is the offset.
	 */
	for (i = 0; i < 32; ) {
		if (FLIP_EM(*freeblocks) & (0x80000000 >> i++))
			break;
		if (FLIP_EM(*freeblocks) & (0x80000000 >> i++))
			break;
		if (FLIP_EM(*freeblocks) & (0x80000000 >> i++))
			break;
		if (FLIP_EM(*freeblocks) & (0x80000000 >> i++))
			break;
	}
	index = 0x80000000 >> --i;


	*freeblocks &= FLIP_EM(~index);

	thiscache->mt_nfree--;

	(void) mutex_unlock(&thiscache->mt_cache_lock);
	(void) mutex_unlock(&cpuptr->mt_parent_lock);

	n = (uintptr_t)(((freeblocks - (uint32_t *)thiscache->mt_freelist) << 5)
		+ i) * thiscache->mt_size;
	/*
	 * Now you have the offset in n, you've changed the free mask
	 * in the freelist. Nothing left to do but find the block
	 * in the arena and put the value of thiscache in the word
	 * ahead of the handed out address and return the memory
	 * back to the user.
	 */
	ret = thiscache->mt_arena + n;

	/* Store the cache addr for this buf. Makes free go fast. */
	*(uintptr_t *)ret = (uintptr_t)thiscache;

	/*
	 * This assert makes sure we don't hand out memory that is not
	 * owned by this cache.
	 */
	assert(ret + thiscache->mt_size <= thiscache->mt_freelist +
		thiscache->mt_span);

	ret += OVERHEAD;

	assert(((uintptr_t)ret & 7) == 0); /* are we 8 bit aligned */

	if (debugopt & MTDEBUGPATTERN)
		if (verify_pattern(FREEPATTERN, ret, size))
			abort();	/* reference after free */

	if (debugopt & MTINITBUFFER)
		copy_pattern(INITPATTERN, ret, size);
	return ((void *)ret);
}

static void *
morecore(size_t bytes)
{
	void * ret;

	if (bytes > LONG_MAX) {
		intptr_t wad;
		/*
		 * The request size is too big. We need to do this in
		 * chunks. Sbrk only takes an int for an arg.
		 */
		if (bytes == ULONG_MAX)
			return ((void *)-1);

		ret = sbrk(0);
		wad = LONG_MAX;
		while (wad > 0) {
			if (sbrk(wad) == (void *)-1) {
				if (ret != sbrk(0))
					(void) sbrk(-LONG_MAX);
				return ((void *)-1);
			}
			bytes -= LONG_MAX;
			wad = bytes;
		}
	} else
		ret = sbrk(bytes);

	return (ret);
}

static void *
oversize(size_t size)
{
	caddr_t ret;
	oversize_t *big;

	/*
	 * The idea with the global lock is that we are sure to
	 * block in the kernel anyway since given an oversize alloc
	 * we are sure to have to call morecore();
	 */
	(void) mutex_lock(&oversize_lock);

	if (ret = (caddr_t)find_oversize(size)) {
		if (debugopt & MTDEBUGPATTERN)
			if (verify_pattern(FREEPATTERN, ret, size))
				abort();	/* reference after free */
		if (debugopt & MTINITBUFFER)
			copy_pattern(INITPATTERN, ret, size);
		(void) mutex_unlock(&oversize_lock);
		assert(((uintptr_t)ret & 7) == 0); /* are we 8 bit aligned */
		return (ret);
	}

	ret = morecore(size + 8 + OVERHEAD + sizeof (oversize_t));
	if (ret == (caddr_t)-1) {
		(void) mutex_unlock(&oversize_lock);
		return (NULL);
	}

	ret = (caddr_t)ALIGN(ret, 8);
	big = (oversize_t *)ret;
	big->mt_prev = NULL;
	big->mt_next = NULL;
	big->mt_size = size;
	ret += sizeof (oversize_t);
	*(uintptr_t *)ret = (uintptr_t)&oversize_list;
	ret += OVERHEAD;
	assert(((uintptr_t)ret & 7) == 0); /* are we 8 bit aligned */
	if (debugopt & MTINITBUFFER)
		copy_pattern(INITPATTERN, ret, size);
	big->mt_addr = ret;

	(void) mutex_unlock(&oversize_lock);
	return ((void *)ret);
}

static void
add_oversize(oversize_t *lp)
{
	oversize_t *wp = oversize_list.mt_next;

	while (wp != &oversize_list && ((lp->mt_size > wp->mt_size)))
		wp = wp->mt_next;

	lp->mt_next = wp;
	lp->mt_prev = wp->mt_prev;
	lp->mt_prev->mt_next = lp;
	lp->mt_next->mt_prev = lp;
}

/*
 * find memory on our list that is at least size big.
 * If we find one that is big enough we break it up
 * and return that address back to the calling client.
 * The leftover piece of that memory is returned to the
 * freelist.
 */
static void *
find_oversize(size_t size)
{
	oversize_t *wp = oversize_list.mt_next;
	caddr_t mem = NULL;

	while (wp != &oversize_list && size > wp->mt_size)
		wp = wp->mt_next;

	if (wp == &oversize_list) /* empty list or nothing big enough */
		return (NULL);

	/* breaking up a chunk of memory */
	if ((long)((wp->mt_size - (size + sizeof (oversize_t) + 16)))
			> MAX_CACHED) {
		caddr_t off;
		oversize_t *np;

		off = wp->mt_addr + size;
		np = (oversize_t *)ALIGN(off, 8);

		np->mt_size = wp->mt_size -
			(size + sizeof (oversize_t) + OVERHEAD);
		wp->mt_size = (size_t)((caddr_t)np - wp->mt_addr);
		np->mt_addr = (caddr_t)np + sizeof (oversize_t) + OVERHEAD;
		np->mt_next = np->mt_prev = NULL;
		if ((long)np->mt_size < 0)
			abort();
		add_oversize(np);
	}
	wp->mt_prev->mt_next = wp->mt_next;
	wp->mt_next->mt_prev = wp->mt_prev;

	mem = (caddr_t)ALIGN(wp->mt_addr, 8);

	return (mem);
}

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
