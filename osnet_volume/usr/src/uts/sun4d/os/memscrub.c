/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memscrub.c	1.17	98/11/30 SMI"

/*
 * sun4d Memory Scrubbing
 *
 * On detection of a correctable memory ECC error, the sun4d hardware
 * returns the corrected data to the requester and re-writes it
 * to memory (DRAM or NVRAM).  So if the correctable error was
 * transient, the read has effectively been cleaned (scrubbed) from memory.
 *
 * Scrubbing thus reduces the likelyhood that multiple transient errors
 * will occur in the same memory word, making uncorrectable errors due
 * to transients less likely.
 *
 * Thus is born the desire that every memory location be periodically
 * accessed.
 *
 * This file implements a memory scrubbing thread.  This scrubber
 * guarantees that all of physical memory is accessed periodically
 * (memscrub_period_sec -- 12 hours).
 *
 * It attempts to do this as unobtrusively as possible.  The thread
 * schedules itself to wake up at an interval such that if it reads
 * memscrub_span_pages (8MB) on each wakeup, it will read all of physical
 * memory in in memscrub_period_sec (12 hours).
 *
 * The scrubber uses the MXCC stream copy hardware to read memory @ 44MB/s,
 * so it reads spans of 8MB in 0.18 seconds.  When it completes a
 * span, if all the CPUs are idle, it reads another span.  Typically it
 * soaks up idle time this way to reach its deadline early -- and sleeps
 * until the next period begins.
 *
 * Maximal Cost Estimate:  5GB @ 44MB/s = 116 seconds spent in 640 wakeups
 * that run for 0.18 seconds at intervals of 67 seconds.
 *
 * In practice, the scrubber finds enough idle time to finish in a few
 * minutes, and sleeps until its 12 hour deadline.
 *
 * The scrubber maintains a private copy of the phys_install memory list
 * to keep track of what memory should be scrubbed.
 *
 * The global routines memscrub_add_span() and memscrub_delete_span() are
 * used to add and delete from this list.  So, for example, the NVSIMM
 * driver can request that a certain range of NVSIMM be added to the
 * scrubbing list, and the ECC code can request that a certain range of
 * memory be excluded from scrubbing.
 *
 * The following parameters can be set via /etc/system
 *
 * memscrub_span_pages = MEMSCRUB_DFL_SPAN_PAGES (8MB)
 * memscrub_period_sec = MEMSCRUB_DFL_PERIOD_SEC (12 hours)
 * memscrub_thread_pri = MEMSCRUB_DFL_THREAD_PRI (0)
 * memscrub_delay_start_sec = (10 seconds)
 * disable_memscrub = (0)
 *
 * the scrubber will exit (or never be started) if it finds the variable
 * "disable_memscrub" set.
 *
 * MEMSCRUB_DFL_SPAN_PAGES (8MB) is based on the guess that 0.18 sec
 * is a "good" amount of minimum time for the thread to run at a time.
 *
 * MEMSCRUB_DFL_PERIOD_SEC (12 hours) is nearly a total guess --
 * twice the frequency the hardware folk estimated would be necessary.
 *
 * MEMSCRUB_DFL_THREAD_PRI (0) is based on the assumption that nearly
 * any other use of the system should be higher priority than scrubbing.
 */

#include <sys/systm.h>		/* timeout, types, t_lock */
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>	/* MIN */
#include <sys/bcopy_if.h>	/* hwbc_scan */
#include <sys/memlist.h>	/* memlist */
#include <sys/kmem.h>		/* KMEM_NOSLEEP */
#include <sys/cpuvar.h>		/* ncpus_online */
#include <sys/debug.h>		/* ASSERTs */

/*
 * Global Routines:
 */
int memscrub_add_span(u_int pfn, u_int pages);
int memscrub_delete_span(u_int pfn, u_int bytes);

/*
 * Global Data:
 */

/*
 * scan all of physical memory at least once every MEMSCRUB_PERIOD_SEC
 */
#define	MEMSCRUB_DFL_PERIOD_SEC	(12 * 60 * 60)	/* 12 hours */

/*
 * scan at least MEMSCRUB_DFL_SPAN_PAGES each iteration
 */
#define	MEMSCRUB_DFL_SPAN_PAGES	((8 * 1024 * 1024) / PAGESIZE)

/*
 * almost anything is higher priority than scrubbing
 */
#define	MEMSCRUB_DFL_THREAD_PRI	0

/*
 * we can patch these defaults in /etc/system if necessary
 */
u_int disable_memscrub = 0;
u_int memscrub_span_pages = MEMSCRUB_DFL_SPAN_PAGES;
u_int memscrub_period_sec = MEMSCRUB_DFL_PERIOD_SEC;
u_int memscrub_thread_pri = MEMSCRUB_DFL_THREAD_PRI;
u_int memscrub_delay_start_sec = 10;

/*
 * Static Routines
 */
static void memscrubber(void);
static int memscrub_verify_span(u_longlong_t *addrp, u_int *pagesp);
static int system_is_idle(void);

/*
 * Static Data
 */

static struct memlist *memscrub_memlist;
static u_int memscrub_phys_pages;

static kcondvar_t memscrub_cv;
static kmutex_t memscrub_lock;
/*
 * memscrub_lock protects memscrub_memlist, memscrub_interval_sec, ...
 */

#ifdef DEBUG
u_int memscrub_cpus_busy;
u_int memscrub_cpus_idle;

u_int memscrub_scans_regular;
u_int memscrub_scans_idle;

u_int memscrub_done_early;
u_int memscrub_early_sec;

u_int memscrub_done_late;
u_int memscrub_late_sec;

u_int memscrub_interval_sec;
#endif DEBUG

/*
 * create memscrub_memlist from phys_install list
 * initialize locks, set memscrub_phys_pages.
 */
int
memscrub_init()
{
	struct memlist *src;

	/*
	 * copy phys_install to memscrub_memlist
	 */
	for (src = phys_install; src; src = src->next) {
		if (memscrub_add_span((u_int) (src->address >> PAGESHIFT),
				(u_int) (src->size >> PAGESHIFT))) {
			disable_memscrub = 1;
			return (-1);
		}
	}

	/*
	 * initialize locks
	 */

	mutex_init(&memscrub_lock, NULL, MUTEX_DRIVER, NULL);

	cv_init(&memscrub_cv, NULL, CV_DRIVER, NULL);

	/*
	 * create memscrubber thread
	 */
	if (thread_create(NULL, PAGESIZE, (void (*)())memscrubber,
	    0, 0, &p0, TS_RUN,  memscrub_thread_pri) == NULL) {
		cmn_err(CE_WARN, "unable to create memscrubber()");
		return (-1);
	}

	return (0);
}

#ifdef MEMSCRUB_DEBUG
void
memscrub_printmemlist(char *title, struct memlist *listp)
{
	struct memlist *list;

	cmn_err(CE_CONT, "%s:\n", title);

	for (list = listp; list; list = list->next) {
		cmn_err(CE_CONT, "addr = 0x%llx, size = 0x%llx\n",
		    list->address, list->size);
	}
}
#endif MEMSCRUB_DEBUG

/* ARGSUSED */
void
memscrub_wakeup(void *arg)
{
	/*
	 * grab mutex to guarantee that our wakeup call
	 * arrives after we go to sleep -- so we can't sleep forever.
	 */
	mutex_enter(&memscrub_lock);
	cv_signal(&memscrub_cv);
	mutex_exit(&memscrub_lock);
}

/*
 * this calculation doesn't account for the time
 * that the actual scan consumes -- so we'd fall
 * slightly behind schedule with this interval_sec.
 * but the idle loop optimization below usually
 * makes us come in way ahead of schedule.
 */

static int
compute_interval_sec()
{
	if (memscrub_phys_pages <= memscrub_span_pages)
		return (memscrub_period_sec);
	else
		return (memscrub_period_sec/
			(memscrub_phys_pages/memscrub_span_pages));
}

void
memscrubber()
{
	u_longlong_t address;
	time_t deadline;
	u_int reached_end = 1;
	int interval_sec = 0;

	if (memscrub_memlist == NULL) {
		cmn_err(CE_WARN, "memscrub_memlist not initialized.");
		goto memscrub_exit;
	}

	address = memscrub_memlist->address;

	deadline = hrestime.tv_sec + memscrub_delay_start_sec;

	for (;;) {
#ifdef DEBUG
		u_int first_time = 1;
#endif DEBUG
		if (disable_memscrub)
			break;

		mutex_enter(&memscrub_lock);

		/*
		 * compute interval_sec
		 */
		interval_sec = compute_interval_sec();

#ifdef DEBUG
		memscrub_interval_sec = interval_sec;
#endif DEBUG

		/*
		 * did we just reach the end of memory?
		 */
		if (reached_end) {
			time_t now = hrestime.tv_sec;

			if (now >= deadline) {
#ifdef DEBUG
				memscrub_done_late++;
				memscrub_late_sec += (now - deadline);
#endif DEBUG
				/*
				 * past deadline, start right away
				 */
				interval_sec = 0;

				deadline = now + memscrub_period_sec;
			} else {
				/*
				 * we finished ahead of schedule.
				 * wait till previous dealine before re-start.
				 */
				interval_sec = deadline - now;
#ifdef DEBUG
				memscrub_done_early++;
				memscrub_early_sec += interval_sec;
#endif DEBUG
				deadline += memscrub_period_sec;
			}
		}

		/*
		 * hit the snooze bar
		 */
		(void) timeout(memscrub_wakeup, NULL, interval_sec * hz);

		/*
		 * go to sleep
		 */
		cv_wait(&memscrub_cv, &memscrub_lock);

		mutex_exit(&memscrub_lock);

		/*
		 * read at least 1 span, and continue until we detect
		 * a non-idle system or we reach the end of memory.
		 */
		do {
			u_int pages = memscrub_span_pages;

			if (disable_memscrub)
				break;

			mutex_enter(&memscrub_lock);
			/*
			 * determine physical address range
			 */
			reached_end = memscrub_verify_span(&address, &pages);
			mutex_exit(&memscrub_lock);

			hwbc_scan(pages * BLOCKS_PER_PAGE, address);

			address = address + ((u_longlong_t)pages * PAGESIZE);
#ifdef DEBUG
			if (first_time) {
				memscrub_scans_regular++;
				first_time = 0;
			} else {
				memscrub_scans_idle++;
			}
#endif DEBUG
		} while (!reached_end && system_is_idle());
	}

memscrub_exit:

	cmn_err(CE_NOTE, "memory scrubber exiting.");

	cv_destroy(&memscrub_cv);

	thread_exit();
}


/*
 * return 1 if we're MP and all the other CPUs are idle
 */
static int
system_is_idle()
{
	int cpu_id;
	int found = 0;

	if (1 == ncpus_online)
		return (0);

	for (cpu_id = 0; cpu_id < NCPU; ++cpu_id) {
		if (!cpu[cpu_id])
			continue;

		found++;

		if (cpu[cpu_id]->cpu_thread != cpu[cpu_id]->cpu_idle_thread) {
			if (CPU->cpu_id == cpu_id &&
			    CPU->cpu_disp.disp_nrunnable == 0)
				continue;
#ifdef DEBUG
			memscrub_cpus_busy++;
#endif DEBUG
			return (0);
		}

		if (found == ncpus)
			break;
	}
#ifdef DEBUG
	memscrub_cpus_idle++;
#endif DEBUG
	return (1);
}

/*
 * condition address and size
 * such that they span legal physical addresses.
 *
 * when appropriate, address will be rounded up to start of next
 * struct memlist, and pages will be rounded down to the end of the
 * memlist size.
 *
 * returns 1 if reached end of list, else returns 0.
 */
static int
memscrub_verify_span(u_longlong_t *addrp, u_int *pagesp)
{
	struct memlist *mlp;
	u_longlong_t address = *addrp;
	u_longlong_t bytes = (u_longlong_t)*pagesp * PAGESIZE;
	u_longlong_t bytes_remaining;
	int reached_end = 0;

	ASSERT(mutex_owned(&memscrub_lock));

	/*
	 * find memlist struct that contains addrp
	 * assumes memlist is sorted by ascending address.
	 */
	for (mlp = memscrub_memlist; mlp; mlp = mlp->next) {
		/*
		 * if before this chunk, round up to beginning
		 */
		if (address < mlp->address) {
			address = mlp->address;
			break;
		}
		/*
		 * if before end of chunk, then we found it
		 */
		if (address < (mlp->address + mlp->size)) {
			break;
		}

		/* else go to next struct memlist */
	}
	/*
	 * if we hit end of list, start at beginning
	 */
	if (mlp == NULL) {
		mlp = memscrub_memlist;
		address = mlp->address;
	}

	/*
	 * now we have legal address, and its mlp, condition bytes
	 */
	bytes_remaining = (mlp->address + mlp->size) - address;

	if (bytes > bytes_remaining)
		bytes = bytes_remaining;

	/*
	 * will this span take us to end of list?
	 */
	if ((mlp->next == NULL) &&
	    ((mlp->address + mlp->size) == (address + bytes)))
		reached_end = 1;

	/* return values */
	*addrp = address;
	*pagesp = bytes/PAGESIZE;

	return (reached_end);
}

/*
 * add a span to the memscrub list
 * add to memscrub_phys_pages
 */
int
memscrub_add_span(u_int pfn, u_int pages)
{
	u_longlong_t address = (u_longlong_t)pfn << PAGESHIFT;
	u_longlong_t bytes = (u_longlong_t)pages << PAGESHIFT;
	struct memlist *dst;
	struct memlist *prev, *next;
	int retval = 0;

	mutex_enter(&memscrub_lock);

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist before", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
	cmn_err(CE_CONT, "memscrub_add_span: address: 0x%llx"
		" size: 0x%llx\n", address, bytes);
#endif MEMSCRUB_DEBUG

	/*
	 * allocate a new struct memlist
	 */

	dst = (struct memlist *)
	    kmem_alloc(sizeof (struct memlist), KM_NOSLEEP);

	if (dst == NULL)
		return (-1);

	dst->address = address;
	dst->size = bytes;

	/*
	 * first insert
	 */
	if (memscrub_memlist == NULL) {
		dst->prev = NULL;
		dst->next = NULL;
		memscrub_memlist = dst;

		goto add_done;
	}

	/*
	 * insert into sorted list
	 */
	for (prev = NULL, next = memscrub_memlist;
	    next;
	    prev = next, next = next->next) {
		if (address > next->address + next->size)
			continue;

		/*
		 * else insert here
		 */

		/*
		 * prepend to next
		 */
		if ((address + bytes) == next->address) {
			kmem_free(dst, sizeof (struct memlist));

			next->address = address;
			next->size += bytes;

			goto add_done;
		}

		/*
		 * append to next
		 */
		if (address == (next->address + next->size)) {
			kmem_free(dst, sizeof (struct memlist));

			if (next->next) {
				/*
				 * don't overlap with next->next
				 */
				if ((address + bytes) > next->next->address) {
					retval = -1;
					goto add_done;
				}
				/*
				 * concatenate next and next->next
				 */
				if ((address + bytes) == next->next->address) {
					struct memlist *mlp = next->next;

					if (next == memscrub_memlist)
						memscrub_memlist = next->next;

					mlp->address = next->address;
					mlp->size += next->size;
					mlp->size += bytes;

					if (next->prev)
						next->prev->next = mlp;
					mlp->prev = next->prev;

					kmem_free(next,
						sizeof (struct memlist));
					goto add_done;
				}
			}

			next->size += bytes;

			goto add_done;
		}

		/*
		 * insert before first
		 */
		if (prev == NULL) {
			dst->prev = NULL;
			dst->next = memscrub_memlist;

			memscrub_memlist = dst;
			next->prev = dst;
			goto add_done;
		} else {
		/*
		 * insert between prev and next
		 */
			/* don't overlap with next */
			if (address + bytes > next->address) {
				retval = -1;
				kmem_free(dst, sizeof (struct memlist));
				goto add_done;
			}

			dst->prev = prev;
			dst->next = next;

			prev->next = dst;
			next->prev = dst;
			goto add_done;
		}
	}	/* end for */

	/*
	 * end of list, prev is valid and next is NULL
	 */
	prev->next = dst;
	dst->prev = prev;
	dst->next = NULL;

add_done:

	if (retval != -1)
		memscrub_phys_pages += pages;


#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist after", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
#endif MEMSCRUB_DEBUG

	mutex_exit(&memscrub_lock);
	return (retval);
}

/*
 * delete a span from the memscrub list
 * subtract from memscrub_phys_pages
 */
int
memscrub_delete_span(u_int pfn, u_int pages)
{
	u_longlong_t address = (u_longlong_t)pfn << PAGESHIFT;
	u_longlong_t bytes = (u_longlong_t)pages << PAGESHIFT;
	struct memlist *dst, *next;
	int retval = 0;

	mutex_enter(&memscrub_lock);

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist Before", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
	cmn_err(CE_CONT, "memscrub_delete_span: 0x%llx 0x%llx\n",
		    address, bytes);
#endif MEMSCRUB_DEBUG

	/*
	 * find struct memlist containing page
	 */
	for (next = memscrub_memlist; next; next = next->next) {
		if ((address >= next->address) &&
		    (address < next->address + next->size))
			break;
	}

	/*
	 * if start address not in list
	 */
	if (next == NULL) {
		retval = -1;
		goto delete_done;
	}

	/*
	 * error if size goes off end of this struct memlist
	 */
	if (address + bytes > next->address + next->size) {
		retval = -1;
		goto delete_done;
	}

	/*
	 * pages at beginning of struct memlist
	 */
	if (address == next->address) {
		/*
		 * if start & size match, delete from list
		 */
		if (bytes == next->size) {
			if (next == memscrub_memlist)
				memscrub_memlist = next->next;
			if (next->prev != NULL)
				next->prev->next = next->next;
			if (next->next != NULL)
				next->next->prev = next->prev;

			kmem_free(next, sizeof (struct memlist));
		} else {
		/*
		 * increment start address by bytes
		 */
			next->address += bytes;
			next->size -= bytes;
		}
		goto delete_done;
	}

	/*
	 * pages at end of struct memlist
	 */
	if (address + bytes == next->address + next->size) {
		/*
		 * decrement size by bytes
		 */
		next->size -= bytes;
		goto delete_done;
	}

	/*
	 * delete a span in the middle of the struct memlist
	 */
	{
		/*
		 * create a new struct memlist
		 */
		dst = (struct memlist *)
		    kmem_alloc(sizeof (struct memlist), KM_NOSLEEP);

		if (dst == NULL) {
			retval = -1;
			goto delete_done;
		}

		/*
		 * existing struct memlist gets address
		 * and size up to pfn
		 */
		dst->address = address + bytes;
		dst->size = (next->address + next->size) - (address + bytes);
		next->size = address - next->address;

		/*
		 * new struct memlist gets address starting
		 * after pfn, until end
		 */

		/*
		 * link in new memlist after old
		 */
		dst->next = next->next;
		dst->prev = next;

		if (next->next != NULL)
			next->next->prev = dst;
		next->next = dst;
	}

delete_done:

	if (retval != -1)
		memscrub_phys_pages -= pages;

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist After", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
#endif MEMSCRUB_DEBUG

	mutex_exit(&memscrub_lock);
	return (retval);
}
