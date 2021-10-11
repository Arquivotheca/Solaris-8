/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tdb_agent.c	1.1	99/10/14 SMI"

/*
 * This file contains most of the functionality
 * required to support libthread_db.
 */

#include "liblwp.h"

/*
 * The threading statistics structure filled in when
 * libthread_db enables statistics gathering.
 */
static td_ta_stats_t tdb_stats;

int tdb_stats_enabled;		/* Is statistics gathering enabled? */

/*
 * The set of globally enabled events to report to libthread_db.
 */
td_thr_events_t tdb_ev_global_mask;

void
tdb_event_ready(void) {}

void
tdb_event_sleep(void) {}

void
tdb_event_switchto(void) {}

void
tdb_event_switchfrom(void) {}

void
tdb_event_lock_try(void) {}

void
tdb_event_catchsig(void) {}

void
tdb_event_idle(void) {}

void
tdb_event_create(void) {}

void
tdb_event_death(void) {}

void
tdb_event_preempt(void) {}

void
tdb_event_pri_inherit(void) {}

void
tdb_event_reap(void) {}

void
tdb_event_concurrency(void) {}

void
tdb_event_timeout(void) {}

/*
 * tdb_register_sync is set to REGISTER_SYNC_ENABLE by a debugger to empty
 * the table and then enable synchronization object registration.
 * tdb_register_sync is set to REGISTER_SYNC_DISABLE by a debugger to empty
 * the table and then disable synchronization object registration.
 */
register_sync_t tdb_register_sync;

/*
 * Pointer to the hash table of sync_addr_t descriptors.
 * This holds the addresses of all of the synchronization variables
 * that the library has seen since tracking was enabled by a debugger.
 */
static	uint64_t	*tdb_sync_addr_hash;

/*
 * The number of entries in the hash table.
 */
uint_t tdb_register_count;

/*
 * The free list of sync_addr_t descriptors.
 * When the free list is used up, it is replenished using zmap().
 * sync_addr_t descriptors are never freed, though they may be
 * removed from the hash table and returned to the free list.
 */
static	tdb_sync_stats_t	*sync_addr_free;
static	tdb_sync_stats_t	*sync_addr_last;
static	size_t			tdb_sync_alloc;

/* protects all of synchronization object hash table data structures */
static	mutex_t	tdb_hash_lock = DEFAULTMUTEX;

/* special tdb_sync_stats structure reserved for tdb_hash_lock */
static	tdb_sync_stats_t	tdb_hash_lock_stats;

#if TDB_HASH_SHIFT != 15
#error "this is all broken because TDB_HASH_SHIFT is not 15"
#endif

static uint_t
tdb_addr_hash(void *addr)
{
	/*
	 * This knows for a fact that the hash table has
	 * 32K entries; that is, that TDB_HASH_SHIFT is 15.
	 */
#ifdef	_LP64
	uint64_t value60 = ((uintptr_t)addr >> 4);	/* 60 bits */
	uint32_t value30 = (value60 >> 30) ^ (value60 & 0x3fffffff);
#else
	uint32_t value30 = ((uintptr_t)addr >> 2);	/* 30 bits */
#endif
	return ((value30 >> 15) ^ (value30 & 0x7fff));
}

static int hash_alloc_failed;

static tdb_sync_stats_t *
alloc_sync_addr(void *addr)
{
	tdb_sync_stats_t *sap;

	ASSERT(_mutex_held(&tdb_hash_lock));

	if ((sap = sync_addr_free) == NULL) {
		void *vaddr;
		int i;

		/*
		 * Don't keep trying after zmap() has already failed.
		 */
		if (hash_alloc_failed)
			return (NULL);

		tdb_sync_alloc *= 2;	/* double the allocation each time */
		if ((vaddr = zmap(NULL,
		    tdb_sync_alloc * sizeof (tdb_sync_stats_t),
		    PROT_READ|PROT_WRITE, MAP_PRIVATE)) == MAP_FAILED) {
			hash_alloc_failed = 1;
			return (NULL);
		}
		sap = sync_addr_free = vaddr;
		for (i = 1; i < tdb_sync_alloc; sap++, i++)
			sap->next = (uint64_t)(sap + 1);
		sap->next = (uint64_t)0;
		sync_addr_last = sap;

		sap = sync_addr_free;
	}

	sync_addr_free = (tdb_sync_stats_t *)sap->next;
	sap->next = (uint64_t)0;
	sap->sync_addr = (uint64_t)addr;
	(void) _memset(&sap->un, 0, sizeof (sap->un));
	return (sap);
}

static void
initialize_sync_hash()
{
	uint64_t	*addr_hash;
	tdb_sync_stats_t *sap;
	void *vaddr;
	int i;

	if (hash_alloc_failed)
		return;
	lmutex_lock(&tdb_hash_lock);
	if (tdb_register_sync == REGISTER_SYNC_DISABLE) {
		/*
		 * There is no point allocating the hash table
		 * if we are disabling registration.
		 */
		tdb_register_sync = REGISTER_SYNC_OFF;
		lmutex_unlock(&tdb_hash_lock);
		return;
	}
	if (tdb_sync_addr_hash != NULL || hash_alloc_failed) {
		lmutex_unlock(&tdb_hash_lock);
		return;
	}
	tdb_sync_alloc = 2*1024; /* start with a free list of 2k elements */
	if ((vaddr = zmap(NULL,
	    TDB_HASH_SIZE * sizeof (uint64_t) +
	    tdb_sync_alloc * sizeof (tdb_sync_stats_t),
	    PROT_READ|PROT_WRITE, MAP_PRIVATE)) == MAP_FAILED) {
		hash_alloc_failed = 1;
		return;
	}
	addr_hash = vaddr;

	/* initialize the free list */
	sync_addr_free = (tdb_sync_stats_t *)&addr_hash[TDB_HASH_SIZE];
	for (sap = sync_addr_free, i = 1; i < tdb_sync_alloc; sap++, i++)
		sap->next = (uint64_t)(sap + 1);
	sap->next = (uint64_t)0;
	sync_addr_last = sap;

	/* insert &tdb_hash_lock itself into the new (empty) table */
	tdb_hash_lock_stats.next = (uint64_t)0;
	tdb_hash_lock_stats.sync_addr = (uint64_t)&tdb_hash_lock;
	addr_hash[tdb_addr_hash(&tdb_hash_lock)] =
		(uint64_t)&tdb_hash_lock_stats;

	/* assign to tdb_sync_addr_hash only after fully initialized */
	tdb_sync_addr_hash = addr_hash;
	tdb_register_count = 1;
	lmutex_unlock(&tdb_hash_lock);
}

tdb_sync_stats_t *
tdb_sync_obj_register(void *addr)
{
	uint64_t *sapp;
	tdb_sync_stats_t *sap;
	int i;

	/*
	 * On the first time through, initialize the hash table and free list.
	 */
	if (tdb_sync_addr_hash == NULL) {
		if (addr == (void *)&tdb_hash_lock)	/* avoid recursion */
			return (&tdb_hash_lock_stats);
		initialize_sync_hash();
		if (tdb_sync_addr_hash == NULL) {	/* utter failure */
			tdb_register_sync = REGISTER_SYNC_OFF;
			return (NULL);
		}
	}

	sapp = &tdb_sync_addr_hash[tdb_addr_hash(addr)];
	if (tdb_register_sync == REGISTER_SYNC_ON) {
		/*
		 * Look up an address in the synchronization object hash table.
		 * No lock is required since it can only deliver a false
		 * negative, in which case we fall into the locked case below.
		 */
		for (sap = (tdb_sync_stats_t *)*sapp; sap != NULL;
		    sap = (tdb_sync_stats_t *)sap->next) {
			if (sap->sync_addr == (uint64_t)addr)
				return (sap);
		}
	}

	/*
	 * The search with no lock held failed or a special action is required.
	 * Grab tdb_hash_lock to do special actions and/or get a precise result.
	 */
	if (addr == (void *)&tdb_hash_lock)	/* avoid recursion */
		return (&tdb_hash_lock_stats);
	lmutex_lock(&tdb_hash_lock);

	switch (tdb_register_sync) {
	case REGISTER_SYNC_ON:
		break;
	case REGISTER_SYNC_OFF:
		lmutex_unlock(&tdb_hash_lock);
		return (NULL);
	default:
		/*
		 * For all debugger actions, first zero out the
		 * statistics block of every element in the hash table.
		 */
		for (i = 0; i < TDB_HASH_SIZE; i++)
			for (sap = (tdb_sync_stats_t *)tdb_sync_addr_hash[i];
			    sap != NULL; sap = (tdb_sync_stats_t *)sap->next)
				(void) _memset(&sap->un, 0, sizeof (sap->un));

		switch (tdb_register_sync) {
		case REGISTER_SYNC_ENABLE:
			tdb_register_sync = REGISTER_SYNC_ON;
			break;
		case REGISTER_SYNC_DISABLE:
		default:
			tdb_register_sync = REGISTER_SYNC_OFF;
			lmutex_unlock(&tdb_hash_lock);
			return (NULL);
		}
		break;
	}

	/*
	 * Perform the search while holding tdb_hash_lock.
	 * Keep track of the insertion point.
	 */
	while ((sap = (tdb_sync_stats_t *)*sapp) != NULL) {
		if (sap->sync_addr == (uint64_t)addr) {
			lmutex_unlock(&tdb_hash_lock);
			return (sap);
		}
		sapp = &sap->next;
	}

	/*
	 * The search failed again.  Insert a new element.
	 */
	if ((sap = alloc_sync_addr(addr)) != NULL) {
		*sapp = (uint64_t)sap;
		tdb_register_count++;
	}

	lmutex_unlock(&tdb_hash_lock);
	return (sap);
}

void
tdb_sync_obj_deregister(void *addr)
{
	uint64_t *sapp;
	tdb_sync_stats_t *sap;

	/*
	 * tdb_hash_lock is never destroyed.
	 */
	ASSERT(addr != &tdb_hash_lock);
	lmutex_lock(&tdb_hash_lock);
	if (tdb_sync_addr_hash == NULL) {
		lmutex_unlock(&tdb_hash_lock);
		return;
	}

	sapp = &tdb_sync_addr_hash[tdb_addr_hash(addr)];
	while ((sap = (tdb_sync_stats_t *)*sapp) != NULL) {
		if (sap->sync_addr == (uint64_t)addr) {
			/* remove it from the hash table */
			*sapp = sap->next;
			tdb_register_count--;
			/* clear it */
			sap->next = (uint64_t)0;
			sap->sync_addr = (uint64_t)0;
			/* insert it on the tail of the free list */
			if (sync_addr_free == NULL)
				sync_addr_free = sync_addr_last = sap;
			else {
				sync_addr_last->next = (uint64_t)sap;
				sync_addr_last = sap;
			}
			break;
		}
		sapp = &sap->next;
	}
	lmutex_unlock(&tdb_hash_lock);
}

/*
 * Return a mutex statistics block for the given mutex.
 */
tdb_mutex_stats_t *
tdb_mutex_stats(mutex_t *mp)
{
	tdb_sync_stats_t *tssp;

	mp->mutex_magic = MUTEX_MAGIC;
	if ((tssp = tdb_sync_obj_register(mp)) == NULL)
		return (NULL);
	tssp->un.type = TDB_MUTEX;
	return (&tssp->un.mutex);
}

/*
 * Return a condvar statistics block for the given condvar.
 */
tdb_cond_stats_t *
tdb_cond_stats(cond_t *cvp)
{
	tdb_sync_stats_t *tssp;

	cvp->cond_magic = COND_MAGIC;
	if ((tssp = tdb_sync_obj_register(cvp)) == NULL)
		return (NULL);
	tssp->un.type = TDB_COND;
	return (&tssp->un.cond);
}

/*
 * Return an rwlock statistics block for the given rwlock.
 */
tdb_rwlock_stats_t *
tdb_rwlock_stats(rwlock_t *rwlp)
{
	tdb_sync_stats_t *tssp;

	rwlp->magic = RWL_MAGIC;
	if ((tssp = tdb_sync_obj_register(rwlp)) == NULL)
		return (NULL);
	tssp->un.type = TDB_RWLOCK;
	return (&tssp->un.rwlock);
}

/*
 * Return a semaphore statistics block for the given semaphore.
 */
tdb_sema_stats_t *
tdb_sema_stats(sema_t *sp)
{
	tdb_sync_stats_t *tssp;

	sp->magic = SEMA_MAGIC;
	if ((tssp = tdb_sync_obj_register(sp)) == NULL)
		return (NULL);
	tssp->un.type = TDB_SEMA;
	return (&tssp->un.sema);
}

/*
 * The set of compile-time constant data of interest to libthread_db;
 * read by libthread_db only once.
 */
lwp_initial_data_t lwp_invar_data = {
	(psaddr_t)&tdb_stats,
	(psaddr_t)&tdb_stats_enabled,
	(psaddr_t)&tdb_ev_global_mask,
	(psaddr_t)&tdb_sync_addr_hash,
	(psaddr_t)&tdb_register_sync,
	(psaddr_t)&nthreads,
	(psaddr_t)&all_lwps,
	(psaddr_t)&ulwp_one,
	(psaddr_t)&tsd_common,
	(psaddr_t)&hash_table,
	tdb_event_ready,
	tdb_event_sleep,
	tdb_event_switchto,
	tdb_event_switchfrom,
	tdb_event_lock_try,
	tdb_event_catchsig,
	tdb_event_idle,
	tdb_event_create,
	tdb_event_death,
	tdb_event_preempt,
	tdb_event_pri_inherit,
	tdb_event_reap,
	tdb_event_concurrency,
	tdb_event_timeout
};
