/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thread_db.c	1.2	99/11/02 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liblwp.h>
#include <thread_db.h>
#include <lwp_db.h>

/*
 * Private structures, known only to libthread_db
 */

typedef union {
	mutex_t		lock;
	rwlock_t	rwlock;
	sema_t		semaphore;
	cond_t		condition;
} td_so_un_t;

struct td_thragent {
	rwlock_t		rwlock;
	struct ps_prochandle	*ph_p;
	short			initialized;
	short			sync_tracking;
	int			model;
	psaddr_t		lwp_invar_addr;
	lwp_invar_data_t	lwp_invar;
	psaddr_t		hash_lock_addr;
	psaddr_t		hash_cond_addr;
	psaddr_t		hash_bucket_addr;
	size_t			hash_size;
};

/*
 * This is the name of the structure in libthread containing all
 * the addresses we will need.
 */
#define	TD_LIBTHREAD_NAME	"libthread.so"
#define	TD_INVAR_DATA_NAME	"lwp_invar_data"

td_err_e __td_thr_get_info(td_thrhandle_t *th_p, td_thrinfo_t *ti_p);

td_err_e __td_ta_thr_iter(td_thragent_t *ta_p, td_thr_iter_f *cb,
	void *cbdata_p, td_thr_state_e state, int ti_pri,
	sigset_t *ti_sigmask_p, unsigned ti_user_flags);

/*
 * Utility function to allocate really big things.
 */
void *
zmap(void *addr, size_t len, int prot, int flags)
{
	int fd;
	void *raddr;

#if defined(MAP_ANON)
	/* first try anonymous mapping */
	flags |= MAP_ANON;
	if ((raddr = mmap(addr, len, prot, flags, -1, (off_t)0)) != MAP_FAILED)
		return (raddr);
	flags &= ~MAP_ANON;
#endif	/* MAP_ANON */

	/* fall back to mapping /dev/zero */
	if ((fd = open("/dev/zero", O_RDWR)) < 0)
		raddr = MAP_FAILED;
	else {
		raddr = mmap(addr, len, prot, flags, fd, (off_t)0);
		(void) close(fd);
	}

	return (raddr);
}

/*
 * Initialize libthread_db.
 */
#pragma weak td_init = __td_init
td_err_e
__td_init()
{
	return (TD_OK);
}

/*
 * This function does nothing, and never did.
 * But the symbol is in the ABI, so we can't delete it.
 */
#pragma weak td_log = __td_log
void
__td_log()
{
}

static td_err_e
td_read_hash_parameters(td_thragent_t *ta_p)
{
	struct ps_prochandle *ph_p = ta_p->ph_p;

	if (ta_p->model == PR_MODEL_NATIVE) {
		hash_table_t hash_table;

		if (ps_pread(ph_p, ta_p->lwp_invar.hash_table_addr,
		    &hash_table, sizeof (hash_table)) != PS_OK)
			return (TD_DBERR);
		ta_p->hash_lock_addr = (psaddr_t)hash_table.hash_lock;
		ta_p->hash_cond_addr = (psaddr_t)hash_table.hash_cond;
		ta_p->hash_bucket_addr = (psaddr_t)hash_table.hash_bucket;
		ta_p->hash_size = hash_table.hash_size;
	} else {
#ifdef _SYSCALL32
		hash_table32_t hash_table;

		if (ps_pread(ph_p, ta_p->lwp_invar.hash_table_addr,
		    &hash_table, sizeof (hash_table)) != PS_OK)
			return (TD_DBERR);
		ta_p->hash_lock_addr = (psaddr_t)hash_table.hash_lock;
		ta_p->hash_cond_addr = (psaddr_t)hash_table.hash_cond;
		ta_p->hash_bucket_addr = (psaddr_t)hash_table.hash_bucket;
		ta_p->hash_size = hash_table.hash_size;
#else
		return (TD_DBERR);
#endif
	}
	if (ta_p->hash_size == 1)	/* still single-threaded */
		ta_p->initialized = 1;
	else				/* multi-threaded */
		ta_p->initialized = 2;
	return (TD_OK);
}

static td_err_e
td_read_invar_data(td_thragent_t *ta_p)
{
	struct ps_prochandle *ph_p = ta_p->ph_p;
	ps_err_e db_return;

	switch (ta_p->initialized) {
	case 2:			/* fully initialized */
		return (TD_OK);
	case 1:			/* partially initialized */
		return (td_read_hash_parameters(ta_p));
	}

	/* uninitialized -- do the startup work */
	db_return = ps_pglobal_lookup(ph_p, TD_LIBTHREAD_NAME,
	    TD_INVAR_DATA_NAME, &ta_p->lwp_invar_addr);
	if (db_return == PS_NOSYM)	/* libthread is not linked */
		return (TD_NOLIBTHREAD);
	else if (db_return != PS_OK)
		return (TD_ERR);

	if (ta_p->model == PR_MODEL_NATIVE) {
		/*
		 * Read the invar data into the thread agent structure.
		 */
		if (ps_pread(ph_p, ta_p->lwp_invar_addr,
		    &ta_p->lwp_invar, sizeof (ta_p->lwp_invar)) != PS_OK)
			return (TD_DBERR);
	} else {
#ifdef _SYSCALL32
		/*
		 * Convert the invar data to native data in the thread agent.
		 */
		lwp_invar_data32_t lwp_invar_data;
		caddr32_t *lidp = (caddr32_t *)&lwp_invar_data;
		psaddr_t *ptr = (psaddr_t *)&ta_p->lwp_invar;
		int i;

		if (ps_pread(ph_p, ta_p->lwp_invar_addr,
		    &lwp_invar_data, sizeof (lwp_invar_data)) != PS_OK)
			return (TD_DBERR);
		for (i = 0; i < sizeof (lwp_invar_data) / sizeof (*lidp); i++)
			*ptr++ = (psaddr_t)*lidp++;
#else
		return (TD_DBERR);
#endif	/* _SYSCALL32 */
	}

#ifdef __ia64
	/*
	 * Deal with stupid ia64 function descriptors.
	 * We have to go indirect to get the actual function address.
	 */
	if (ta_p->model == PR_MODEL_NATIVE) {
		int i;
		psaddr_t addr;

		for (i = 0; i < TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1; i++) {
			if (ps_pread(ph_p, ta_p->lwp_invar.tdb_events[i],
			    &addr, sizeof (addr)) == PS_OK)
				ta_p->lwp_invar.tdb_events[i] = addr;
		}
	}
#endif	/* __ia64 */

	return (td_read_hash_parameters(ta_p));
}

#pragma weak ps_kill
#pragma weak ps_lrolltoaddr

/*
 * Allocate a new libthread_db process handle ("thread agent").
 */
#pragma weak td_ta_new = __td_ta_new
td_err_e
__td_ta_new(struct ps_prochandle *ph_p, td_thragent_t **ta_pp)
{
	td_thragent_t *ta_p;
	int model;
	td_err_e return_val = TD_OK;

	if (ph_p == NULL)
		return (TD_BADPH);
	if (ta_pp == NULL)
		return (TD_ERR);
	*ta_pp = NULL;
	if (ps_pstop(ph_p) != PS_OK)
		return (TD_DBERR);
	/*
	 * ps_pdmodel might not be defined if this is an older client.
	 * Make it a weak symbol and test if it exists before calling.
	 */
#pragma weak ps_pdmodel
	if (ps_pdmodel == NULL) {
		model = PR_MODEL_NATIVE;
	} else if (ps_pdmodel(ph_p, &model) != PS_OK) {
		(void) ps_pcontinue(ph_p);
		return (TD_ERR);
	}
	if ((ta_p = malloc(sizeof (*ta_p))) == NULL) {
		(void) ps_pcontinue(ph_p);
		return (TD_MALLOC);
	}

	/*
	 * Initialize the process handle.
	 * Pick up the symbol value we need from the target process.
	 */
	(void) memset(ta_p, 0, sizeof (*ta_p));
	ta_p->ph_p = ph_p;
	(void) rwlock_init(&ta_p->rwlock, USYNC_THREAD, NULL);
	ta_p->model = model;
	return_val = td_read_invar_data(ta_p);

	/*
	 * Because the old libthread_db enabled lock tracking by default,
	 * we must also do it.  However, we do it only if the application
	 * provides the ps_kill() and ps_lrolltoaddr() interfaces.
	 * (dbx provides the ps_kill() and ps_lrolltoaddr() interfaces.)
	 */
	if (return_val == TD_OK) {
		int oldenable;
		int enable = REGISTER_SYNC_ENABLE;
		psaddr_t psaddr = ta_p->lwp_invar.tdb_register_sync_addr;

		if (ps_kill != NULL && ps_lrolltoaddr != NULL) {
			if (ps_pread(ph_p, psaddr,
			    &oldenable, sizeof (oldenable)) != PS_OK ||
			    ps_pwrite(ph_p, psaddr,
			    &enable, sizeof (enable)) != PS_OK)
				return_val = TD_DBERR;
			else if (oldenable != REGISTER_SYNC_OFF)
				ta_p->sync_tracking = 1;
		}
	}

	if (return_val == TD_OK)
		*ta_pp = ta_p;
	else
		free(ta_p);

	(void) ps_pcontinue(ph_p);
	return (return_val);
}

/*
 * Utility function to grab the readers lock and return the process handle,
 * given a libthread process handle.  Performs standard error checking.
 * Returns non-NULL with the lock held, or NULL with the lock not held.
 */
static struct ps_prochandle *
ph_lock_ta(td_thragent_t *ta_p, td_err_e *err)
{
	struct ps_prochandle *ph_p = NULL;
	td_err_e error;

	if (ta_p == NULL) {
		*err = TD_BADTA;
	} else if (rw_rdlock(&ta_p->rwlock) != 0) {	/* can't happen? */
		*err = TD_BADTA;
	} else if ((ph_p = ta_p->ph_p) == NULL) {
		(void) rw_unlock(&ta_p->rwlock);
		*err = TD_BADPH;
	} else if (ta_p->initialized != 2 &&
	    (error = td_read_invar_data(ta_p)) != TD_OK) {
		(void) rw_unlock(&ta_p->rwlock);
		*err = error;
	} else {
		*err = TD_OK;
	}

	return (ph_p);
}

/*
 * Utility function to grab the readers lock and return the process handle,
 * given a libthread thread handle.  Performs standard error checking.
 * Returns non-NULL with the lock held, or NULL with the lock not held.
 */
static struct ps_prochandle *
ph_lock_th(const td_thrhandle_t *th_p, td_err_e *err)
{
	if (th_p == NULL || th_p->th_unique == NULL) {
		*err = TD_BADTH;
		return (NULL);
	}
	return (ph_lock_ta(th_p->th_ta_p, err));
}

/*
 * Utility function to grab the readers lock and return the process handle,
 * given a libthread sync. object handle.  Performs standard error checking.
 * Returns non-NULL with the lock held, or NULL with the lock not held.
 */
static struct ps_prochandle *
ph_lock_sh(const td_synchandle_t *sh_p, td_err_e *err)
{
	if (sh_p == NULL || sh_p->sh_unique == NULL) {
		*err = TD_BADSH;
		return (NULL);
	}
	return (ph_lock_ta(sh_p->sh_ta_p, err));
}

/*
 * Unlock the process handle obtained from ph_lock_*().
 */
static void
ph_unlock(td_thragent_t *ta_p)
{
	(void) rw_unlock(&ta_p->rwlock);
}

/*
 * De-allocate a libthread_db process handle,
 * releasing all related resources.
 *
 * XXX -- This is hopelessly broken ---
 * Storage for thread agent is not deallocated.  The prochandle
 * in the thread agent is set to NULL so that future uses of
 * the thread agent can be detected and an error value returned.
 * All functions in the external user interface that make
 * use of the thread agent are expected
 * to check for a NULL prochandle in the therad agent.
 * All such functions are also expect to use obtain a
 * reader lock on the thread agent while it is using it.
 */
#pragma weak td_ta_delete = __td_ta_delete
td_err_e
__td_ta_delete(td_thragent_t *ta_p)
{
	td_err_e return_val;

	if (ph_lock_ta(ta_p, &return_val) == NULL)
		return (return_val);
	/*
	 * If synch. tracking was disabled when td_ta_new() was called and
	 * if td_ta_sync_tracking_enable() was never called, then disable
	 * synch. tracking (it was enabled by default in td_ta_new()).
	 */
	if (ta_p->sync_tracking == 0 &&
	    ps_kill != NULL && ps_lrolltoaddr != NULL) {
		int enable = REGISTER_SYNC_DISABLE;

		(void) ps_pwrite(ta_p->ph_p,
		    ta_p->lwp_invar.tdb_register_sync_addr,
		    &enable, sizeof (enable));
	}
	ta_p->ph_p = NULL;
	ph_unlock(ta_p);
	return (TD_OK);
}

/*
 * Map a libthread_db process handle to a client process handle.
 * Currently unused by dbx.
 */
#pragma weak td_ta_get_ph = __td_ta_get_ph
td_err_e
__td_ta_get_ph(td_thragent_t *ta_p, struct ps_prochandle **ph_pp)
{
	td_err_e return_val;

	if (ph_pp != NULL)	/* protect stupid callers */
		*ph_pp = NULL;
	if (ph_pp == NULL)
		return (TD_ERR);
	if ((*ph_pp = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	ph_unlock(ta_p);
	return (TD_OK);
}

/*
 * Set the process's suggested concurrency level.
 * This is a no-op in a one-level model.
 * Currently unused by dbx.
 */
#pragma weak td_ta_setconcurrency = __td_ta_setconcurrency
/* ARGSUSED1 */
td_err_e
__td_ta_setconcurrency(const td_thragent_t *ta_p, int level)
{
	if (ta_p == NULL)
		return (TD_BADTA);
	if (ta_p->ph_p == NULL)
		return (TD_BADPH);
	return (TD_OK);
}

/*
 * Get the number of threads in the process.
 */
#pragma weak td_ta_get_nthreads = __td_ta_get_nthreads
td_err_e
__td_ta_get_nthreads(td_thragent_t *ta_p, int *nthread_p)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if (nthread_p == NULL)
		return (TD_ERR);
	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pread(ph_p, ta_p->lwp_invar.nthreads_addr,
	    nthread_p, sizeof (int)) != PS_OK)
		return_val = TD_DBERR;
	ph_unlock(ta_p);
	return (TD_OK);
}

typedef struct {
	thread_t	tid;
	int		found;
	td_thrhandle_t	th;
} td_mapper_param_t;

/*
 * Check the value in data against the thread id.
 * If it matches, return 1 to terminate interations.
 * This function is used by td_ta_map_id2thr() to map a tid to a thread handle.
 */
static int
td_mapper_id2thr(td_thrhandle_t *th_p, td_mapper_param_t *data)
{
	td_thrinfo_t ti;

	if (__td_thr_get_info(th_p, &ti) == TD_OK &&
	    data->tid == ti.ti_tid) {
		data->found = 1;
		data->th = *th_p;
		return (1);
	}
	return (0);
}

/*
 * Given a thread identifier, return the corresponding thread handle.
 */
#pragma weak td_ta_map_id2thr = __td_ta_map_id2thr
td_err_e
__td_ta_map_id2thr(td_thragent_t *ta_p, thread_t tid,
	td_thrhandle_t *th_p)
{
	td_err_e		return_val;
	td_mapper_param_t	data;

	if (tid == 1 &&		/* optimize for the initial lwp */
	    th_p != NULL &&
	    ta_p != NULL &&
	    ta_p->initialized == 1 &&
	    td_read_hash_parameters(ta_p) == TD_OK &&
	    ta_p->initialized == 1) {
		th_p->th_ta_p = ta_p;
		th_p->th_unique = ta_p->lwp_invar.ulwp_one_addr;
		return (TD_OK);
	}

	/*
	 * LOCKING EXCEPTION - Locking is not required here because
	 * the locking and checking will be done in __td_ta_thr_iter.
	 */

	if (ta_p == NULL)
		return (TD_BADTA);
	if (th_p == NULL)
		return (TD_BADTH);
	if (tid == 0)
		return (TD_NOTHR);

	data.tid = tid;
	data.found = 0;
	return_val = __td_ta_thr_iter(ta_p,
		(td_thr_iter_f *)td_mapper_id2thr, (void *)&data,
		TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
	if (return_val == TD_OK) {
		if (data.found == 0)
			return_val = TD_NOTHR;
		else
			*th_p = data.th;
	}

	return (return_val);
}

/*
 * Map the address of a synchronization object to a sync. object handle.
 */
#pragma weak td_ta_map_addr2sync = __td_ta_map_addr2sync
td_err_e
__td_ta_map_addr2sync(td_thragent_t *ta_p, psaddr_t addr, td_synchandle_t *sh_p)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;
	uint16_t sync_magic;

	if (sh_p == NULL)
		return (TD_BADSH);
	if (addr == NULL)
		return (TD_ERR);
	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	/*
	 * Check the magic number of the sync. object to make sure it's valid.
	 * The magic number is at the same offset for all sync. objects.
	 */
	if (ps_pread(ph_p, (psaddr_t)&((mutex_t *)addr)->mutex_magic,
	    &sync_magic, sizeof (sync_magic)) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_BADSH);
	}
	ph_unlock(ta_p);
	if (sync_magic != MUTEX_MAGIC && sync_magic != COND_MAGIC &&
	    sync_magic != SEMA_MAGIC && sync_magic != RWL_MAGIC)
		return (TD_BADSH);
	/*
	 * Just fill in the appropriate fields of the sync. handle.
	 */
	sh_p->sh_ta_p = (td_thragent_t *)ta_p;
	sh_p->sh_unique = addr;
	return (TD_OK);
}

/*
 * Iterate over the set of global TSD keys.
 * The call back function is called with three arguments,
 * a key, a pointer to the destructor function, and the cbdata pointer.
 * Currently unused by dbx.
 */
#pragma weak td_ta_tsd_iter = __td_ta_tsd_iter
td_err_e
__td_ta_tsd_iter(td_thragent_t *ta_p, td_key_iter_f *cb, void *cbdata_p)
{
	struct ps_prochandle *ph_p;
	td_err_e	return_val;
	int		key;
	int		numkeys;
	psaddr_t	dest_addr;
	psaddr_t	*destructors = NULL;
	void    	(*destructor)();

	if (cb == NULL)
		return (TD_ERR);
	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	if (ta_p->model == PR_MODEL_NATIVE) {
		tsd_common_t tsd_common;

		if (ps_pread(ph_p, ta_p->lwp_invar.tsd_common_addr,
		    &tsd_common, sizeof (tsd_common)) != PS_OK)
			return_val = TD_DBERR;
		else {
			numkeys = tsd_common.numkeys;
			dest_addr = (psaddr_t)tsd_common.destructors;
			if (numkeys > 0)
				destructors = malloc(numkeys *
					sizeof (psaddr_t));
		}
	} else {
#ifdef _SYSCALL32
		tsd_common32_t tsd_common32;

		if (ps_pread(ph_p, ta_p->lwp_invar.tsd_common_addr,
		    &tsd_common32, sizeof (tsd_common32)) != PS_OK)
			return_val = TD_DBERR;
		else {
			numkeys = tsd_common32.numkeys;
			dest_addr = (psaddr_t)tsd_common32.destructors;
			if (numkeys > 0)
				destructors = malloc(numkeys *
					sizeof (caddr32_t));
		}
#else
		return_val = TD_DBERR;
#endif	/* _SYSCALL32 */
	}

	if (return_val != TD_OK || numkeys <= 0) {
		(void) ps_pcontinue(ph_p);
		ph_unlock(ta_p);
		return (return_val);
	}

	if (destructors == NULL)
		return_val = TD_MALLOC;
	else if (ta_p->model == PR_MODEL_NATIVE) {
		if (ps_pread(ph_p, dest_addr,
		    destructors, numkeys * sizeof (psaddr_t)) != PS_OK)
			return_val = TD_DBERR;
		else {
			for (key = 1; key <= numkeys; key++) {
				destructor = (void (*)())destructors[key-1];
				if (destructor != NULL)
					(*cb)(key, destructor, cbdata_p);
			}
		}
#ifdef _SYSCALL32
	} else {
		caddr32_t *destruct32 = (caddr32_t *)destructors;

		if (ps_pread(ph_p, dest_addr,
		    destruct32, numkeys * sizeof (caddr32_t)) != PS_OK)
			return_val = TD_DBERR;
		else {
			for (key = 1; key <= numkeys; key++) {
				destructor = (void (*)())destruct32[key-1];
				if (destructor != NULL)
					(*cb)(key, destructor, cbdata_p);
			}
		}
#endif	/* _SYSCALL32 */
	}

	if (destructors)
		free(destructors);
	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Description:
 *   Iterate over all threads. For each thread call
 * the function pointed to by "cb" with a pointer
 * to a thread handle, and a pointer to data which
 * can be NULL. Only call td_thr_iter_f() on threads
 * which match the properties of state, ti_pri,
 * ti_sigmask_p, and ti_user_flags.  If cb returns
 * a non-zero value, terminate iterations.
 *
 * Input:
 *   *ta_p - thread agent
 *   *cb - call back function defined by user.
 * td_thr_iter_f() takes a thread handle and
 * cbdata_p as a parameter.
 *   cbdata_p - parameter for td_thr_iter_f().
 *
 *   state - state of threads of interest.  A value of
 * TD_THR_ANY_STATE from enum td_thr_state_e
 * does not restrict iterations by state.
 *   ti_pri - lower bound of priorities of threads of
 * interest.  A value of TD_THR_LOWEST_PRIORITY
 * defined in thread_db.h does not restrict
 * iterations by priority.  A thread with priority
 * less than ti_pri will NOT be passed to the callback
 * function.
 *   ti_sigmask_p - signal mask of threads of interest.
 * A value of TD_SIGNO_MASK defined in thread_db.h
 * does not restrict iterations by signal mask.
 *   ti_user_flags - user flags of threads of interest.  A
 * value of TD_THR_ANY_USER_FLAGS defined in thread_db.h
 * does not restrict iterations by user flags.
 */
#pragma weak td_ta_thr_iter = __td_ta_thr_iter
td_err_e
__td_ta_thr_iter(td_thragent_t *ta_p, td_thr_iter_f *cb,
	void *cbdata_p, td_thr_state_e state, int ti_pri,
	sigset_t *ti_sigmask_p, unsigned ti_user_flags)
{
	struct ps_prochandle *ph_p;
	psaddr_t	first_lwp_addr;
	psaddr_t	curr_lwp_addr;
	psaddr_t	next_lwp_addr;
	td_thrhandle_t	th;
	ps_err_e	db_return;
	td_err_e	return_val;

	if (cb == NULL)
		return (TD_ERR);
	/*
	 * If state is not within bound, short circuit.
	 */
	if (state < TD_THR_ANY_STATE || state > TD_THR_STOPPED_ASLEEP)
		return (TD_OK);

	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	/*
	 * For each ulwp_t in the circular linked list pointed
	 * to by "all_lwps":
	 * (1) Filter each thread.
	 * (2) Create the thread_object for each thread that passes.
	 * (3) Call the call back function on each thread.
	 */

	if (ta_p->model == PR_MODEL_NATIVE) {
		db_return = ps_pread(ph_p, ta_p->lwp_invar.all_lwps_addr,
			&first_lwp_addr, sizeof (first_lwp_addr));
	} else {
#ifdef _SYSCALL32
		caddr32_t first_lwp_addr32;

		db_return = ps_pread(ph_p, ta_p->lwp_invar.all_lwps_addr,
			&first_lwp_addr32, sizeof (first_lwp_addr32));
		first_lwp_addr = first_lwp_addr32;
#else	/* _SYSCALL32 */
		db_return = PS_ERR;
#endif	/* _SYSCALL32 */
	}

	/*
	 * If first_lwp_addr is NULL, libthread must not yet be initialized.
	 * Return TD_NOTHR and all will be well.
	 */
	if (db_return == PS_OK && first_lwp_addr == NULL) {
		(void) ps_pcontinue(ph_p);
		ph_unlock(ta_p);
		return (TD_NOTHR);
	}
	if (db_return != PS_OK) {
		(void) ps_pcontinue(ph_p);
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	/*
	 * Run down the list of all lwps.
	 */
	curr_lwp_addr = first_lwp_addr;
	do {
		td_thr_state_e ts_state;
		int userpri;
		unsigned userflags;

		/*
		 * Read the ulwp struct.
		 */
		if (ta_p->model == PR_MODEL_NATIVE) {
			ulwp_t ulwp;

			if (ps_pread(ph_p, curr_lwp_addr,
			    &ulwp, sizeof (ulwp)) != PS_OK) {
				return_val = TD_DBERR;
				break;
			}
			next_lwp_addr = (psaddr_t)ulwp.ul_forw;

			ts_state = ulwp.ul_wchan? TD_THR_SLEEP : TD_THR_ACTIVE;
			userpri = ulwp.ul_pri;
			userflags = ulwp.ul_usropts;
		} else {
#ifdef _SYSCALL32
			ulwp32_t ulwp;

			if (ps_pread(ph_p, curr_lwp_addr,
			    &ulwp, sizeof (ulwp)) != PS_OK) {
				return_val = TD_DBERR;
				break;
			}
			next_lwp_addr = (psaddr_t)ulwp.ul_forw;

			ts_state = ulwp.ul_wchan? TD_THR_SLEEP : TD_THR_ACTIVE;
			userpri = ulwp.ul_pri;
			userflags = ulwp.ul_usropts;
#else	/* _SYSCALL32 */
			return_val = TD_ERR;
			break;
#endif	/* _SYSCALL32 */
		}

		/*
		 * Filter on state, priority, sigmask, and user flags.
		 */

		if ((state != ts_state) &&
		    (state != TD_THR_ANY_STATE))
			continue;

		if (ti_pri > userpri)
			continue;

		/* We don't know the signal mask */
		if (ti_sigmask_p != TD_SIGNO_MASK)
			continue;

		if (ti_user_flags != userflags &&
		    ti_user_flags != (unsigned)TD_THR_ANY_USER_FLAGS)
			continue;

		/*
		 * Call back - break if the return
		 * from the call back is non-zero.
		 */
		th.th_ta_p = (td_thragent_t *)ta_p;
		th.th_unique = curr_lwp_addr;
		if ((*cb)(&th, cbdata_p))
			break;

	} while ((curr_lwp_addr = next_lwp_addr) != first_lwp_addr);

	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Enable or disable process synchronization object tracking.
 * Currently unused by dbx.
 */
#pragma weak td_ta_sync_tracking_enable = __td_ta_sync_tracking_enable
td_err_e
__td_ta_sync_tracking_enable(td_thragent_t *ta_p, int onoff)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;
	register_sync_t enable;

	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	/*
	 * Values of tdb_register_sync in the victim process:
	 *	REGISTER_SYNC_ENABLE	enables registration of synch objects
	 *	REGISTER_SYNC_DISABLE	disables registration of synch objects
	 * These cause the table to be cleared and tdb_register_sync set to:
	 *	REGISTER_SYNC_ON	registration in effect
	 *	REGISTER_SYNC_OFF	registration not in effect
	 */
	enable = onoff? REGISTER_SYNC_ENABLE : REGISTER_SYNC_DISABLE;
	if (ps_pwrite(ph_p, ta_p->lwp_invar.tdb_register_sync_addr,
	    &enable, sizeof (enable)) != PS_OK)
		return_val = TD_DBERR;
	/*
	 * Remember that this interface was called (see td_ta_delete()).
	 */
	ta_p->sync_tracking = 1;
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Iterate over all known synchronization variables.
 * It is very possible that the list generated is incomplete,
 * because the iterator can only find synchronization variables
 * that have been registered by the process since synchronization
 * object registration was enabled.
 * The call back function cb is called for each synchronization
 * variable with two arguments: a pointer to the synchronization
 * handle and the passed-in argument cbdata.
 * If cb returns a non-zero value, iterations are terminated.
 */
#pragma weak td_ta_sync_iter = __td_ta_sync_iter
td_err_e
__td_ta_sync_iter(td_thragent_t *ta_p, td_sync_iter_f *cb, void *cbdata)
{
	struct ps_prochandle *ph_p;
	td_err_e	return_val;
	int		i;
	register_sync_t	enable;
	psaddr_t	next_desc;
	tdb_sync_stats_t sync_stats;
	td_synchandle_t	synchandle;
	psaddr_t	psaddr;
	void		*vaddr;
	uint64_t	*sync_addr_hash = NULL;

	if (cb == NULL)
		return (TD_ERR);
	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}
	if (ps_pread(ph_p, ta_p->lwp_invar.tdb_register_sync_addr,
	    &enable, sizeof (enable)) != PS_OK) {
		return_val = TD_DBERR;
		goto out;
	}
	if (enable != REGISTER_SYNC_ON)
		goto out;

	/*
	 * First read the hash table.
	 * The hash table is large; allocate with zmap().
	 */
	if ((vaddr = zmap(NULL, TDB_HASH_SIZE * sizeof (uint64_t),
	    PROT_READ|PROT_WRITE, MAP_PRIVATE)) == MAP_FAILED) {
		return_val = TD_MALLOC;
		goto out;
	}
	sync_addr_hash = vaddr;

	if (ta_p->model == PR_MODEL_NATIVE) {
		if (ps_pread(ph_p, ta_p->lwp_invar.sync_addr_hash_addr,
		    &psaddr, sizeof (&psaddr)) != PS_OK) {
			return_val = TD_DBERR;
			goto out;
		}
	} else {
#ifdef  _SYSCALL32
		caddr32_t psaddr32;

		if (ps_pread(ph_p, ta_p->lwp_invar.sync_addr_hash_addr,
		    &psaddr32, sizeof (psaddr32)) != PS_OK) {
			return_val = TD_DBERR;
			goto out;
		}
		psaddr = psaddr32;
#else
		return_val = TD_ERR;
		goto out;
#endif /* _SYSCALL32 */
	}

	if (psaddr == NULL)
		goto out;
	if (ps_pread(ph_p, psaddr, sync_addr_hash,
	    TDB_HASH_SIZE * sizeof (uint64_t)) != PS_OK) {
		return_val = TD_DBERR;
		goto out;
	}

	/*
	 * Now scan the hash table.
	 */
	for (i = 0; i < TDB_HASH_SIZE; i++) {
		for (next_desc = (psaddr_t)sync_addr_hash[i];
		    next_desc != NULL;
		    next_desc = (psaddr_t)sync_stats.next) {
			if (ps_pread(ph_p, next_desc,
			    &sync_stats, sizeof (sync_stats)) != PS_OK) {
				return_val = TD_DBERR;
				goto out;
			}
			switch (sync_stats.un.type) {
			case TDB_NONE:
				/* not registered since registration enabled */
				continue;
			case TDB_MUTEX:
			case TDB_COND:
				/* skip rwlock-embedded mutex and condvar */
				if (sync_stats.un.mutex.offset)
					continue;
				break;
			}
			synchandle.sh_ta_p = ta_p;
			synchandle.sh_unique = (psaddr_t)sync_stats.sync_addr;
			if ((*cb)(&synchandle, cbdata) != 0)
				goto out;
		}
	}

out:
	if (sync_addr_hash != NULL)
		(void) munmap((void *)sync_addr_hash,
		    TDB_HASH_SIZE * sizeof (uint64_t));
	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Enable process statistics collection.
 */
#pragma weak td_ta_enable_stats = __td_ta_enable_stats
/* ARGSUSED */
td_err_e
__td_ta_enable_stats(const td_thragent_t *ta_p, int onoff)
{
	return (TD_NOCAPAB);
}

/*
 * Reset process statistics.
 */
#pragma weak td_ta_reset_stats = __td_ta_reset_stats
/* ARGSUSED */
td_err_e
__td_ta_reset_stats(const td_thragent_t *ta_p)
{
	return (TD_NOCAPAB);
}

/*
 * Read process statistics.
 */
#pragma weak td_ta_get_stats = __td_ta_get_stats
/* ARGSUSED */
td_err_e
__td_ta_get_stats(const td_thragent_t *ta_p, td_ta_stats_t *tstats)
{
	return (TD_NOCAPAB);
}

/*
 * Transfer information from lwp struct to thread information struct.
 * XXX -- lots of this needs cleaning up.
 */
static void
td_thr2to(td_thragent_t *ta_p, psaddr_t ts_addr,
	ulwp_t *ulwp, td_thrinfo_t *ti_p)
{
	(void) memset(ti_p, 0, sizeof (*ti_p));
	ti_p->ti_ta_p = ta_p;
	ti_p->ti_user_flags = ulwp->ul_usropts;
	ti_p->ti_tid = ulwp->ul_lwpid;
	ti_p->ti_tls = NULL;
	ti_p->ti_startfunc = (psaddr_t)ulwp->ul_startpc;
	ti_p->ti_stkbase = (psaddr_t)ulwp->ul_stktop;
	ti_p->ti_stksize = ulwp->ul_stksiz;
	ti_p->ti_ro_area = ts_addr;
	ti_p->ti_ro_size = sizeof (ulwp_t);
	ti_p->ti_state = ulwp->ul_wchan? TD_THR_SLEEP : TD_THR_ACTIVE;
	ti_p->ti_db_suspended = 0;
	ti_p->ti_type = TD_THR_USER;
	if (ulwp->ul_wchan) {
		ti_p->ti_pc = ulwp->ul_savedregs.rs_pc;
		ti_p->ti_sp = ulwp->ul_savedregs.rs_sp;
	}
	ti_p->ti_flags = 0;
	ti_p->ti_pri = ulwp->ul_pri;
	ti_p->ti_lid = ulwp->ul_lwpid;
	(void) sigemptyset(&ti_p->ti_sigmask);
	ti_p->ti_traceme = 0;
	ti_p->ti_preemptflag = 0;
	ti_p->ti_pirecflag = 0;
	(void) sigemptyset(&ti_p->ti_pending);
	ti_p->ti_events = ulwp->ul_td_evbuf.eventmask;
}

#ifdef _SYSCALL32
static void
td_thr2to32(td_thragent_t *ta_p, psaddr_t ts_addr,
	ulwp32_t *ulwp, td_thrinfo_t *ti_p)
{
	(void) memset(ti_p, 0, sizeof (*ti_p));
	ti_p->ti_ta_p = ta_p;
	ti_p->ti_user_flags = ulwp->ul_usropts;
	ti_p->ti_tid = ulwp->ul_lwpid;
	ti_p->ti_tls = NULL;
	ti_p->ti_startfunc = (psaddr_t)ulwp->ul_startpc;
	ti_p->ti_stkbase = (psaddr_t)ulwp->ul_stktop;
	ti_p->ti_stksize = ulwp->ul_stksiz;
	ti_p->ti_ro_area = ts_addr;
	ti_p->ti_ro_size = sizeof (ulwp32_t);
	ti_p->ti_state = ulwp->ul_wchan? TD_THR_SLEEP : TD_THR_ACTIVE;
	ti_p->ti_db_suspended = 0;
	ti_p->ti_type = TD_THR_USER;
	if (ulwp->ul_wchan) {
		ti_p->ti_pc = (uint32_t)ulwp->ul_savedregs.rs_pc;
		ti_p->ti_sp = (uint32_t)ulwp->ul_savedregs.rs_sp;
	}
	ti_p->ti_flags = 0;
	ti_p->ti_pri = ulwp->ul_pri;
	ti_p->ti_lid = ulwp->ul_lwpid;
	(void) sigemptyset(&ti_p->ti_sigmask);
	ti_p->ti_traceme = 0;
	ti_p->ti_preemptflag = 0;
	ti_p->ti_pirecflag = 0;
	(void) sigemptyset(&ti_p->ti_pending);
	ti_p->ti_events = ulwp->ul_td_evbuf.eventmask;
}
#endif	/* _SYSCALL32 */

/*
 * Get thread information.
 */
#pragma weak td_thr_get_info = __td_thr_get_info
td_err_e
__td_thr_get_info(td_thrhandle_t *th_p, td_thrinfo_t *ti_p)
{
	struct ps_prochandle *ph_p;
	td_thragent_t	*ta_p;
	td_err_e	return_val;
	psaddr_t	psaddr;

	if (ti_p == NULL)
		return (TD_ERR);
	(void) memset(ti_p, NULL, sizeof (*ti_p));

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	ta_p = th_p->th_ta_p;
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	/*
	 * Read the ulwp struct from the process.
	 * Transfer the ulwp struct to the thread information struct.
	 */
	psaddr = th_p->th_unique;
	if (ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t ulwp;

		if (ps_pread(ph_p, psaddr, &ulwp, sizeof (ulwp)) != PS_OK)
			return_val = TD_DBERR;
		else
			td_thr2to(ta_p, psaddr, &ulwp, ti_p);
	} else {
#ifdef _SYSCALL32
		ulwp32_t ulwp;

		if (ps_pread(ph_p, psaddr, &ulwp, sizeof (ulwp)) != PS_OK)
			return_val = TD_DBERR;
		else
			td_thr2to32(ta_p, psaddr, &ulwp, ti_p);
#else
		return_val = TD_ERR;
#endif	/* _SYSCALL32 */
	}

	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Given a process and an event number, return information about
 * an address in the process or at which a breakpoint can be set
 * to monitor the event.
 */
#pragma weak td_ta_event_addr = __td_ta_event_addr
td_err_e
__td_ta_event_addr(td_thragent_t *ta_p, td_event_e event, td_notify_t *notify_p)
{
	if (ta_p == NULL)
		return (TD_BADTA);
	if (event < TD_MIN_EVENT_NUM || event > TD_MAX_EVENT_NUM)
		return (TD_NOEVENT);
	if (notify_p == NULL)
		return (TD_ERR);

	notify_p->type = NOTIFY_BPT;
	notify_p->u.bptaddr =
	    ta_p->lwp_invar.tdb_events[event - TD_MIN_EVENT_NUM];

	return (TD_OK);
}

/*
 * Add the events in eventset 2 to eventset 1.
 */
static void
eventsetaddset(td_thr_events_t *event1_p, td_thr_events_t *event2_p)
{
	int	i;

	for (i = 0; i < TD_EVENTSIZE; i++)
		event1_p->event_bits[i] |= event2_p->event_bits[i];
}

/*
 * Delete the events in eventset 2 from eventset 1.
 */
static void
eventsetdelset(td_thr_events_t *event1_p, td_thr_events_t *event2_p)
{
	int	i;

	for (i = 0; i < TD_EVENTSIZE; i++)
		event1_p->event_bits[i] &= ~event2_p->event_bits[i];
}

/*
 * Either add or delete the given event set from a thread's event mask.
 */
static td_err_e
mod_eventset(td_thrhandle_t *th_p, td_thr_events_t *events, int onoff)
{
	struct ps_prochandle *ph_p;
	td_err_e	return_val = TD_OK;
	char		enable;
	td_thr_events_t	evset;
	psaddr_t	psaddr_evset;
	psaddr_t	psaddr_enab;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (th_p->th_ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;
		psaddr_evset = (psaddr_t)&ulwp->ul_td_evbuf.eventmask;
		psaddr_enab = (psaddr_t)&ulwp->ul_td_events_enable;
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;
		psaddr_evset = (psaddr_t)&ulwp->ul_td_evbuf.eventmask;
		psaddr_enab = (psaddr_t)&ulwp->ul_td_events_enable;
#else
		ph_unlock(th_p->th_ta_p);
		return (TD_ERR);
#endif	/* _SYSCALL32 */
	}
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_pread(ph_p, psaddr_evset, &evset, sizeof (evset)) != PS_OK)
		return_val = TD_DBERR;
	else {
		if (onoff)
			eventsetaddset(&evset, events);
		else
			eventsetdelset(&evset, events);
		if (ps_pwrite(ph_p, psaddr_evset, &evset, sizeof (evset))
		    != PS_OK)
			return_val = TD_DBERR;
		else {
			enable = 0;
			if (td_eventismember(&evset, TD_EVENTS_ENABLE))
				enable = 1;
			if (ps_pwrite(ph_p, psaddr_enab,
			    &enable, sizeof (enable)) != PS_OK)
				return_val = TD_DBERR;
		}
	}

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Enable or disable tracing for a given thread.  Tracing
 * is filtered based on the event mask of each thread.  Tracing
 * can be turned on/off for the thread without changing thread
 * event mask.
 * Currently unused by dbx.
 */
#pragma weak td_thr_event_enable = __td_thr_event_enable
td_err_e
__td_thr_event_enable(td_thrhandle_t *th_p, int onoff)
{
	td_thr_events_t	evset;

	td_event_emptyset(&evset);
	td_event_addset(&evset, TD_EVENTS_ENABLE);
	return (mod_eventset(th_p, &evset, onoff));
}

/*
 * Set event mask to enable event. event is turned on in
 * event mask for thread.  If a thread encounters an event
 * for which its event mask is on, notification will be sent
 * to the debugger.
 * Addresses for each event are provided to the
 * debugger.  It is assumed that a breakpoint of some type will
 * be placed at that address.  If the event mask for the thread
 * is on, the instruction at the address will be executed.
 * Otherwise, the instruction will be skipped.
 */
#pragma weak td_thr_set_event = __td_thr_set_event
td_err_e
__td_thr_set_event(td_thrhandle_t *th_p, td_thr_events_t *events)
{
	return (mod_eventset(th_p, events, 1));
}

/*
 * Enable or disable a set of events in the process-global event mask,
 * depending on the value of onoff.
 */
static td_err_e
td_ta_mod_event(td_thragent_t *ta_p, td_thr_events_t *events, int onoff)
{
	struct ps_prochandle *ph_p;
	td_thr_events_t targ_eventset;
	td_err_e	return_val;

	if ((ph_p = ph_lock_ta(ta_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}
	if (ps_pread(ph_p, ta_p->lwp_invar.tdb_eventmask_addr,
	    &targ_eventset, sizeof (targ_eventset)) != PS_OK)
		return_val = TD_DBERR;
	else {
		if (onoff)
			eventsetaddset(&targ_eventset, events);
		else
			eventsetdelset(&targ_eventset, events);
		if (ps_pwrite(ph_p, ta_p->lwp_invar.tdb_eventmask_addr,
		    &targ_eventset, sizeof (targ_eventset)) != PS_OK)
			return_val = TD_DBERR;
	}
	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Enable a set of events in the process-global event mask.
 */
#pragma weak td_ta_set_event = __td_ta_set_event
td_err_e
__td_ta_set_event(td_thragent_t *ta_p, td_thr_events_t *events)
{
	return (td_ta_mod_event(ta_p, events, 1));
}

/*
 * Set event mask to disable the given event set; these events are cleared
 * from the event mask of the thread.  Events that occur for a thread
 * with the event masked off will not cause notification to be
 * sent to the debugger (see td_thr_set_event for fuller description).
 */
#pragma weak td_thr_clear_event = __td_thr_clear_event
td_err_e
__td_thr_clear_event(td_thrhandle_t *th_p, td_thr_events_t *events)
{
	return (mod_eventset(th_p, events, 0));
}

/*
 * Disable a set of events in the process-global event mask.
 */
#pragma weak td_ta_clear_event = __td_ta_clear_event
td_err_e
__td_ta_clear_event(td_thragent_t *ta_p, td_thr_events_t *events)
{
	return (td_ta_mod_event(ta_p, events, 0));
}

/*
 * This function returns the most recent event message, if any,
 * associated with a thread.  Given a thread handle, return the message
 * corresponding to the event encountered by the thread.  Only one
 * message per thread is saved.  Messages from earlier events are lost
 * when later events occur.
 */
#pragma weak td_thr_event_getmsg = __td_thr_event_getmsg
td_err_e
__td_thr_event_getmsg(td_thrhandle_t *th_p, td_event_msg_t *msg)
{
	struct ps_prochandle *ph_p;
	td_err_e	return_val = TD_OK;
	psaddr_t	psaddr;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_BADTA);
	}
	if (th_p->th_ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;
		td_evbuf_t evbuf;

		psaddr = (psaddr_t)&ulwp->ul_td_evbuf;
		if (ps_pread(ph_p, psaddr, &evbuf, sizeof (evbuf)) != PS_OK) {
			return_val = TD_DBERR;
		} else if (evbuf.eventnum == TD_EVENT_NONE) {
			return_val = TD_NOEVENT;
		} else {
			msg->event = evbuf.eventnum;
			msg->th_p = (td_thrhandle_t *)th_p;
			msg->msg.data = (uintptr_t)evbuf.eventdata;
			/* "Consume" the message */
			evbuf.eventnum = TD_EVENT_NONE;
			evbuf.eventdata = NULL;
			if (ps_pwrite(ph_p, psaddr, &evbuf, sizeof (evbuf))
			    != PS_OK)
				return_val = TD_DBERR;
		}
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;
		td_evbuf32_t evbuf;

		psaddr = (psaddr_t)&ulwp->ul_td_evbuf;
		if (ps_pread(ph_p, psaddr, &evbuf, sizeof (evbuf)) != PS_OK) {
			return_val = TD_DBERR;
		} else if (evbuf.eventnum == TD_EVENT_NONE) {
			return_val = TD_NOEVENT;
		} else {
			msg->event = evbuf.eventnum;
			msg->th_p = (td_thrhandle_t *)th_p;
			msg->msg.data = (uintptr_t)evbuf.eventdata;
			/* "Consume" the message */
			evbuf.eventnum = TD_EVENT_NONE;
			evbuf.eventdata = NULL;
			if (ps_pwrite(ph_p, psaddr, &evbuf, sizeof (evbuf))
			    != PS_OK)
				return_val = TD_DBERR;
		}
#else
		return_val = TD_ERR;
#endif	/* _SYSCALL32 */
	}

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * The callback function td_ta_event_getmsg uses when looking for
 * a thread with an event.  A thin wrapper around td_thr_event_getmsg.
 */
static int
event_msg_cb(const td_thrhandle_t *th_p, void *arg)
{
	static td_thrhandle_t th;
	td_event_msg_t *msg = arg;

	if (__td_thr_event_getmsg((td_thrhandle_t *)th_p, msg) == TD_OK) {
		/*
		 * Got an event, stop iterating.
		 *
		 * Because of past mistakes in interface definition,
		 * we are forced to pass back a static local variable
		 * for the thread handle because th_p is a pointer
		 * to a local variable in __td_ta_thr_iter().
		 * Grr...
		 */
		th = *th_p;
		msg->th_p = &th;
		return (1);
	}
	return (0);
}

/*
 * This function is just like td_thr_event_getmsg, except that it is
 * passed a process handle rather than a thread handle, and returns
 * an event message for some thread in the process that has an event
 * message pending.  If no thread has an event message pending, this
 * routine returns TD_NOEVENT.  Thus, all pending event messages may
 * be collected from a process by repeatedly calling this routine
 * until it returns TD_NOEVENT.
 */
#pragma weak td_ta_event_getmsg = __td_ta_event_getmsg
td_err_e
__td_ta_event_getmsg(td_thragent_t *ta_p, td_event_msg_t *msg)
{
	td_err_e return_val;

	if (ta_p == NULL)
		return (TD_BADTA);
	if (ta_p->ph_p == NULL)
		return (TD_BADPH);
	if (msg == NULL)
		return (TD_ERR);
	msg->event = TD_EVENT_NONE;
	if ((return_val = __td_ta_thr_iter(ta_p, event_msg_cb, msg,
	    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY, TD_SIGNO_MASK,
	    TD_THR_ANY_USER_FLAGS)) != TD_OK)
		return (return_val);
	if (msg->event == TD_EVENT_NONE)
		return (TD_NOEVENT);
	return (TD_OK);
}

static lwpid_t
thr_to_lwpid(const td_thrhandle_t *th_p)
{
	struct ps_prochandle *ph_p = th_p->th_ta_p->ph_p;
	lwpid_t lwpid;

	/*
	 * The caller holds the prochandle lock
	 * and has already verfied everything.
	 */
	if (th_p->th_ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_lwpid,
		    &lwpid, sizeof (lwpid)) != PS_OK)
			lwpid = 0;
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_lwpid,
		    &lwpid, sizeof (lwpid)) != PS_OK)
			lwpid = 0;
#else
		lwpid = 0;
#endif	/* _SYSCALL32 */
	}

	return (lwpid);
}

/*
 * Suspend a thread.
 * XXX: What does this mean in a one-level model?
 */
#pragma weak td_thr_dbsuspend = __td_thr_dbsuspend
td_err_e
__td_thr_dbsuspend(const td_thrhandle_t *th_p)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_lstop(ph_p, thr_to_lwpid(th_p)) != PS_OK)
		return_val = TD_DBERR;
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Resume a suspended thread.
 * XXX: What does this mean in a one-level model?
 */
#pragma weak td_thr_dbresume = __td_thr_dbresume
td_err_e
__td_thr_dbresume(const td_thrhandle_t *th_p)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_lcontinue(ph_p, thr_to_lwpid(th_p)) != PS_OK)
		return_val = TD_DBERR;
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Set a thread's signal mask.
 * Currently unused by dbx.
 */
#pragma weak td_thr_sigsetmask = __td_thr_sigsetmask
/* ARGSUSED */
td_err_e
__td_thr_sigsetmask(const td_thrhandle_t *th_p, const sigset_t ti_sigmask)
{
	return (TD_NOCAPAB);
}

/*
 * Set a thread's "signals-pending" set.
 * Currently unused by dbx.
 */
#pragma weak td_thr_setsigpending = __td_thr_setsigpending
/* ARGSUSED */
td_err_e
__td_thr_setsigpending(const td_thrhandle_t *th_p,
	const uchar_t ti_pending_flag, const sigset_t ti_pending)
{
	return (TD_NOCAPAB);
}

/*
 * Get a thread's general register set.
 */
#pragma weak td_thr_getgregs = __td_thr_getgregs
td_err_e
__td_thr_getgregs(td_thrhandle_t *th_p, prgregset_t regset)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lgetregs(ph_p, thr_to_lwpid(th_p), regset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Set a thread's general register set.
 */
#pragma weak td_thr_setgregs = __td_thr_setgregs
td_err_e
__td_thr_setgregs(td_thrhandle_t *th_p, const prgregset_t regset)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lsetregs(ph_p, thr_to_lwpid(th_p), regset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Get a thread's floating-point register set.
 */
#pragma weak td_thr_getfpregs = __td_thr_getfpregs
td_err_e
__td_thr_getfpregs(td_thrhandle_t *th_p, prfpregset_t *fpregset)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lgetfpregs(ph_p, thr_to_lwpid(th_p), fpregset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Set a thread's floating-point register set.
 */
#pragma weak td_thr_setfpregs = __td_thr_setfpregs
td_err_e
__td_thr_setfpregs(td_thrhandle_t *th_p, const prfpregset_t *fpregset)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lsetfpregs(ph_p, thr_to_lwpid(th_p), fpregset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Get the size of the extra state register set for this architecture.
 * Currently unused by dbx.
 */
#pragma weak td_thr_getxregsize = __td_thr_getxregsize
/* ARGSUSED */
td_err_e
__td_thr_getxregsize(td_thrhandle_t *th_p, int *xregsize)
{
#if defined(__sparc)
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lgetxregsize(ph_p, thr_to_lwpid(th_p), xregsize) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
#else	/* __sparc */
	return (TD_NOXREGS);
#endif	/* __sparc */
}

/*
 * Get a thread's extra state register set.
 */
#pragma weak td_thr_getxregs = __td_thr_getxregs
/* ARGSUSED */
td_err_e
__td_thr_getxregs(td_thrhandle_t *th_p, void *xregset)
{
#if defined(__sparc)
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lgetxregs(ph_p, thr_to_lwpid(th_p), (caddr_t)xregset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
#else	/* __sparc */
	return (TD_NOXREGS);
#endif	/* __sparc */
}

/*
 * Set a thread's extra state register set.
 */
#pragma weak td_thr_setxregs = __td_thr_setxregs
/* ARGSUSED */
td_err_e
__td_thr_setxregs(td_thrhandle_t *th_p, const void *xregset)
{
#if defined(__sparc)
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(th_p->th_ta_p);
		return (TD_DBERR);
	}

	if (ps_lsetxregs(ph_p, thr_to_lwpid(th_p), (caddr_t)xregset) != PS_OK)
		return_val = TD_DBERR;

	(void) ps_pcontinue(ph_p);
	ph_unlock(th_p->th_ta_p);
	return (return_val);
#else	/* __sparc */
	return (TD_NOXREGS);
#endif	/* __sparc */
}

struct searcher {
	psaddr_t	addr;
	int		status;
};

/*
 * Check the struct thread address in *th_p again first
 * value in "data".  If value in data is found, set second value
 * in "data" to 1 and return 1 to terminate iterations.
 * This function is used by td_thr_validate() to verify that
 * a thread handle is valid.
 */
static int
td_searcher(const td_thrhandle_t *th_p, void *data)
{
	struct searcher *searcher_data = (struct searcher *)data;

	if (searcher_data->addr == th_p->th_unique) {
		searcher_data->status = 1;
		return (1);
	}
	return (0);
}

/*
 * Validate the thread handle.  Check that
 * a thread exists in the thread agent/process that
 * corresponds to thread with handle *th_p.
 * Currently unused by dbx.
 */
#pragma weak td_thr_validate = __td_thr_validate
td_err_e
__td_thr_validate(const td_thrhandle_t *th_p)
{
	td_err_e return_val;
	struct searcher searcher_data = {0, 0};

	if (th_p == NULL)
		return (TD_BADTH);
	if (th_p->th_unique == NULL || th_p->th_ta_p == NULL)
		return (TD_BADTH);

	/*
	 * LOCKING EXCEPTION - Locking is not required
	 * here because no use of the thread agent is made (other
	 * than the sanity check) and checking of the thread
	 * agent will be done in __td_ta_thr_iter.
	 */

	searcher_data.addr = th_p->th_unique;
	return_val = __td_ta_thr_iter(th_p->th_ta_p,
		td_searcher, &searcher_data,
		TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

	if (return_val == TD_OK && searcher_data.status == 0)
		return_val = TD_NOTHR;

	return (return_val);
}

/*
 * Get a thread's private binding to a given thread specific
 * data(TSD) key(see thr_getspecific(3T).  If the thread doesn't
 * have a binding for a particular key, then NULL is returned.
 */
#pragma weak td_thr_tsd = __td_thr_tsd
td_err_e
__td_thr_tsd(td_thrhandle_t *th_p, const thread_key_t key, void **data_pp)
{
	struct ps_prochandle *ph_p;
	td_thragent_t	*ta_p;
	td_err_e	return_val;
	int		maxkey;
	int		nkey;
	psaddr_t	tsd_paddr;

	if (data_pp == NULL)
		return (TD_ERR);
	*data_pp = NULL;
	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);
	ta_p = th_p->th_ta_p;
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	if (ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;
		tsd_common_t tsd_common;
		struct ul_tsd ul_tsd;

		if (ps_pread(ph_p, ta_p->lwp_invar.tsd_common_addr,
		    &tsd_common, sizeof (tsd_common)) != PS_OK)
			return_val = TD_DBERR;
		else if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_tsd,
		    &ul_tsd, sizeof (ul_tsd)) != PS_OK)
			return_val = TD_DBERR;
		else {
			maxkey = tsd_common.numkeys;
			nkey = ul_tsd.nkey;
			tsd_paddr = (psaddr_t)ul_tsd.tsd;
		}
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;
		tsd_common32_t tsd_common;
		struct ul_tsd32 ul_tsd;

		if (ps_pread(ph_p, ta_p->lwp_invar.tsd_common_addr,
		    &tsd_common, sizeof (tsd_common)) != PS_OK)
			return_val = TD_DBERR;
		else if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_tsd,
		    &ul_tsd, sizeof (ul_tsd)) != PS_OK)
			return_val = TD_DBERR;
		else {
			maxkey = tsd_common.numkeys;
			nkey = ul_tsd.nkey;
			tsd_paddr = (psaddr_t)ul_tsd.tsd;
		}
#else
		return_val = TD_ERR;
#endif	/* _SYSCALL32 */
	}

	if (return_val == TD_OK && (key < 1 || key > maxkey))
		return_val = TD_NOTSD;
	if (return_val != TD_OK || key > nkey) {
		/* NULL has already been stored in data_pp */
		(void) ps_pcontinue(ph_p);
		ph_unlock(ta_p);
		return (return_val);
	}

	/*
	 * Read the value from the thread's tsd array.
	 */
	if (ta_p->model == PR_MODEL_NATIVE) {
		void *value;

		if (ps_pread(ph_p, tsd_paddr + (key - 1) * sizeof (void *),
		    &value, sizeof (value)) != PS_OK)
			return_val = TD_DBERR;
		else
			*data_pp = value;
#ifdef _SYSCALL32
	} else {
		caddr32_t value32;

		if (ps_pread(ph_p, tsd_paddr + (key - 1) * sizeof (caddr32_t),
		    &value32, sizeof (value32)) != PS_OK)
			return_val = TD_DBERR;
		else
			*data_pp = (void *)value32;
#endif	/* _SYSCALL32 */
	}

	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Change a thread's priority to the value specified by ti_pri.
 * Currently unused by dbx.
 */
#pragma weak td_thr_setprio = __td_thr_setprio
td_err_e
__td_thr_setprio(td_thrhandle_t *th_p, const int ti_pri)
{
	struct ps_prochandle *ph_p;
	pri_t		priority = ti_pri;
	td_err_e	return_val = TD_OK;

	if (ti_pri < THREAD_MIN_PRIORITY || ti_pri > THREAD_MAX_PRIORITY)
		return (TD_ERR);
	if ((ph_p = ph_lock_th(th_p, &return_val)) == NULL)
		return (return_val);

	if (th_p->th_ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;

		if (ps_pwrite(ph_p, (psaddr_t)&ulwp->ul_pri,
		    &priority, sizeof (priority)) != PS_OK)
			return_val = TD_DBERR;
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;

		if (ps_pwrite(ph_p, (psaddr_t)&ulwp->ul_pri,
		    &priority, sizeof (priority)) != PS_OK)
			return_val = TD_DBERR;
#else
		return_val = TD_ERR;
#endif	/* _SYSCALL32 */
	}

	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * This structure links td_thr_lockowner and the lowner_cb callback function.
 */
typedef struct {
	td_sync_iter_f	*owner_cb;
	void		*owner_cb_arg;
	td_thrhandle_t	*th_p;
} lowner_cb_ctl_t;

static int
lowner_cb(const td_synchandle_t *sh_p, void *arg)
{
	lowner_cb_ctl_t *ocb = arg;
	int trunc = 0;
	union {
		rwlock_t rwl;
		mutex_t mx;
	} rw_m;

	if (ps_pread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
	    &rw_m, sizeof (rw_m)) != PS_OK) {
		trunc = 1;
		if (ps_pread(sh_p->sh_ta_p->ph_p, sh_p->sh_unique,
		    &rw_m.mx, sizeof (rw_m.mx)) != PS_OK)
			return (0);
	}
	if (rw_m.mx.mutex_magic == MUTEX_MAGIC &&
	    rw_m.mx.mutex_owner == ocb->th_p->th_unique)
		return ((ocb->owner_cb)(sh_p, ocb->owner_cb_arg));
	if (!trunc && rw_m.rwl.magic == RWL_MAGIC) {
		/* LINTED pointer cast */
		mutex_t *rwlock = (mutex_t *)(&rw_m.rwl.pad1);
		if (rwlock->mutex_owner == ocb->th_p->th_unique)
			return ((ocb->owner_cb)(sh_p, ocb->owner_cb_arg));
	}
	return (0);
}

/*
 * Iterate over the set of locks owned by a specified thread.
 * If cb returns a non-zero value, terminate iterations.
 */
#pragma weak td_thr_lockowner = __td_thr_lockowner
td_err_e
__td_thr_lockowner(const td_thrhandle_t *th_p, td_sync_iter_f *cb,
	void *cb_data)
{
	td_thragent_t	*ta_p;
	td_err_e	return_val;
	lowner_cb_ctl_t	lcb;

	/*
	 * Just sanity checks.
	 */
	if (ph_lock_th((td_thrhandle_t *)th_p, &return_val) == NULL)
		return (return_val);
	ta_p = th_p->th_ta_p;
	ph_unlock(ta_p);

	lcb.owner_cb = cb;
	lcb.owner_cb_arg = cb_data;
	lcb.th_p = (td_thrhandle_t *)th_p;
	return (__td_ta_sync_iter(ta_p, lowner_cb, &lcb));
}

/*
 * If a thread is asleep on a synchronization variable,
 * then get the synchronization handle.
 */
#pragma weak td_thr_sleepinfo = __td_thr_sleepinfo
td_err_e
__td_thr_sleepinfo(const td_thrhandle_t *th_p, td_synchandle_t *sh_p)
{
	struct ps_prochandle *ph_p;
	td_err_e	return_val = TD_OK;
	uintptr_t	wchan;

	if (sh_p == NULL)
		return (TD_ERR);
	if ((ph_p = ph_lock_th((td_thrhandle_t *)th_p, &return_val)) == NULL)
		return (return_val);

	/*
	 * No need to stop the process for a simple read.
	 */
	if (th_p->th_ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_wchan,
		    &wchan, sizeof (wchan)) != PS_OK)
			return_val = TD_DBERR;
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;
		caddr32_t wchan32;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_wchan,
		    &wchan32, sizeof (wchan32)) != PS_OK)
			return_val = TD_DBERR;
		wchan = wchan32;
#else
		return_val = TD_ERR;
#endif	/* _SYSCALL32 */
	}

	if (return_val != TD_OK || wchan == NULL) {
		sh_p->sh_ta_p = NULL;
		sh_p->sh_unique = NULL;
		if (return_val == TD_OK)
			return_val = TD_ERR;
	} else {
		sh_p->sh_ta_p = th_p->th_ta_p;
		sh_p->sh_unique = (psaddr_t)wchan;
	}

	ph_unlock(th_p->th_ta_p);
	return (return_val);
}

/*
 * Which thread is running on an lwp?
 */
#pragma weak td_ta_map_lwp2thr = __td_ta_map_lwp2thr
td_err_e
__td_ta_map_lwp2thr(td_thragent_t *ta_p, lwpid_t lwpid,
	td_thrhandle_t *th_p)
{
	return (__td_ta_map_id2thr(ta_p, lwpid, th_p));
}

/*
 * Common code for td_sync_get_info() and td_sync_get_stats()
 */
static td_err_e
sync_get_info_common(const td_synchandle_t *sh_p, struct ps_prochandle *ph_p,
	td_syncinfo_t *si_p)
{
	int trunc = 0;
	td_so_un_t generic_so;
	mutex_t *rwlock;
	cond_t *readers;
	cond_t *writers;

	/*
	 * Determine the sync. object type; a little type fudgery here.
	 * First attempt to read the whole union.  If that fails, attempt
	 * to read just the condvar.  A condvar is the smallest sync. object.
	 */
	if (ps_pread(ph_p, sh_p->sh_unique,
	    &generic_so, sizeof (generic_so)) != PS_OK) {
		trunc = 1;
		if (ps_pread(ph_p, sh_p->sh_unique, &generic_so.condition,
		    sizeof (generic_so.condition)) != PS_OK)
			return (TD_DBERR);
	}

	switch (generic_so.condition.cond_magic) {
	case MUTEX_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.lock, sizeof (generic_so.lock)) != PS_OK)
			return (TD_DBERR);
		si_p->si_type = TD_SYNC_MUTEX;
		si_p->si_shared_type = generic_so.lock.mutex_type;
		(void) memcpy(si_p->si_flags, &generic_so.lock.mutex_flag,
		    sizeof (generic_so.lock.mutex_flag));
		si_p->si_state.mutex_locked =
		    (generic_so.lock.mutex_lockw != 0);
		si_p->si_size = sizeof (generic_so.lock);
		si_p->si_has_waiters = generic_so.lock.mutex_waiters;
		if (si_p->si_state.mutex_locked) {
			si_p->si_owner.th_ta_p = sh_p->sh_ta_p;
			si_p->si_owner.th_unique = generic_so.lock.mutex_owner;
		}
		break;
	case COND_MAGIC:
		si_p->si_type = TD_SYNC_COND;
		si_p->si_shared_type = generic_so.condition.cond_type;
		(void) memcpy(si_p->si_flags, generic_so.condition.flags.flag,
		    sizeof (generic_so.condition.flags.flag));
		si_p->si_size = sizeof (generic_so.condition);
		si_p->si_has_waiters = CVWAITERS(&generic_so.condition)? 1 : 0;
		break;
	case SEMA_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.semaphore, sizeof (generic_so.semaphore))
		    != PS_OK)
			return (TD_DBERR);
		si_p->si_type = TD_SYNC_SEMA;
		si_p->si_shared_type = generic_so.semaphore.type;
		si_p->si_state.sem_count = generic_so.semaphore.count;
		si_p->si_size = sizeof (generic_so.semaphore);
		si_p->si_has_waiters =
		    ((lwp_sema_t *)&generic_so.semaphore)->flags[7];
		/* this is useless but the old interface provided it */
		si_p->si_data = (psaddr_t)generic_so.semaphore.count;
		break;
	case RWL_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.rwlock, sizeof (generic_so.rwlock)) != PS_OK)
			return (TD_DBERR);
		/* LINTED pointer cast may result in improper alignment */
		rwlock = (mutex_t *)(generic_so.rwlock.pad1);
		/* LINTED pointer cast may result in improper alignment */
		readers = (cond_t *)(generic_so.rwlock.pad2);
		/* LINTED pointer cast may result in improper alignment */
		writers = (cond_t *)(generic_so.rwlock.pad3);
		si_p->si_type = TD_SYNC_RWLOCK;
		si_p->si_shared_type = generic_so.rwlock.type;
		si_p->si_state.nreaders = generic_so.rwlock.readers;
		si_p->si_size = sizeof (generic_so.rwlock);
		si_p->si_has_waiters = (rwlock->mutex_waiters ||
		    CVWAITERS(readers) || CVWAITERS(writers));
		if (si_p->si_state.nreaders == -1) {
			si_p->si_is_wlock = 1;
			si_p->si_owner.th_ta_p = sh_p->sh_ta_p;
			si_p->si_owner.th_unique = rwlock->mutex_owner;
		}
		/* this is useless but the old interface provided it */
		si_p->si_data = (psaddr_t)generic_so.rwlock.readers;
		break;
	default:
		return (TD_BADSH);
	}

	si_p->si_ta_p = sh_p->sh_ta_p;
	si_p->si_sv_addr = sh_p->sh_unique;
	return (TD_OK);
}

/*
 * Given a synchronization handle, fill in the
 * information for the synchronization variable into *si_p.
 */
#pragma weak td_sync_get_info = __td_sync_get_info
td_err_e
__td_sync_get_info(const td_synchandle_t *sh_p, td_syncinfo_t *si_p)
{
	struct ps_prochandle *ph_p;
	td_err_e return_val;

	if (si_p == NULL)
		return (TD_ERR);
	(void) memset(si_p, 0, sizeof (*si_p));
	if ((ph_p = ph_lock_sh(sh_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(sh_p->sh_ta_p);
		return (TD_DBERR);
	}

	return_val = sync_get_info_common(sh_p, ph_p, si_p);

	(void) ps_pcontinue(ph_p);
	ph_unlock(sh_p->sh_ta_p);
	return (return_val);
}

static uint_t
tdb_addr_hash64(uint64_t addr)
{
	uint64_t value60 = (addr >> 4);
	uint32_t value30 = (value60 >> 30) ^ (value60 & 0x3fffffff);
	return ((value30 >> 15) ^ (value30 & 0x7fff));
}

static uint_t
tdb_addr_hash32(uint64_t addr)
{
	uint32_t value30 = (addr >> 2);		/* 30 bits */
	return ((value30 >> 15) ^ (value30 & 0x7fff));
}

static td_err_e
read_sync_stats(td_thragent_t *ta_p, psaddr_t hash_table,
	psaddr_t sync_obj_addr, tdb_sync_stats_t *sync_stats)
{
	psaddr_t next_desc;
	uint64_t first;
	uint_t ix;

	/*
	 * Compute the hash table index from the synch object's address.
	 */
	if (ta_p->model == PR_MODEL_LP64)
		ix = tdb_addr_hash64(sync_obj_addr);
	else
		ix = tdb_addr_hash32(sync_obj_addr);

	/*
	 * Get the address of the first element in the linked list.
	 */
	if (ps_pread(ta_p->ph_p, hash_table + ix * sizeof (uint64_t),
	    &first, sizeof (first)) != PS_OK)
		return (TD_DBERR);

	/*
	 * Search the linked list for an entry for the synch object..
	 */
	for (next_desc = (psaddr_t)first; next_desc != NULL;
	    next_desc = (psaddr_t)sync_stats->next) {
		if (ps_pread(ta_p->ph_p, next_desc,
		    sync_stats, sizeof (*sync_stats)) != PS_OK)
			return (TD_DBERR);
		if (sync_stats->sync_addr == sync_obj_addr)
			return (TD_OK);
	}

	(void) memset(sync_stats, 0, sizeof (*sync_stats));
	return (TD_OK);
}

/*
 * Given a synchronization handle, fill in the
 * statistics for the synchronization variable into *ss_p.
 */
#pragma weak td_sync_get_stats = __td_sync_get_stats
td_err_e
__td_sync_get_stats(const td_synchandle_t *sh_p, td_syncstats_t *ss_p)
{
	struct ps_prochandle *ph_p;
	td_thragent_t *ta_p;
	td_err_e return_val;
	register_sync_t enable;
	psaddr_t hashaddr;
	tdb_sync_stats_t sync_stats;
	size_t ix;

	if (ss_p == NULL)
		return (TD_ERR);
	(void) memset(ss_p, 0, sizeof (*ss_p));
	if ((ph_p = ph_lock_sh(sh_p, &return_val)) == NULL)
		return (return_val);
	ta_p = sh_p->sh_ta_p;
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(ta_p);
		return (TD_DBERR);
	}

	if ((return_val = sync_get_info_common(sh_p, ph_p, &ss_p->ss_info))
	    != TD_OK) {
		if (return_val != TD_BADSH)
			goto out;
		/* we can correct TD_BADSH */
		(void) memset(&ss_p->ss_info, 0, sizeof (ss_p->ss_info));
		ss_p->ss_info.si_ta_p = sh_p->sh_ta_p;
		ss_p->ss_info.si_sv_addr = sh_p->sh_unique;
		/* we correct si_type and si_size below */
		return_val = TD_OK;
	}
	if (ps_pread(ph_p, ta_p->lwp_invar.tdb_register_sync_addr,
	    &enable, sizeof (enable)) != PS_OK) {
		return_val = TD_DBERR;
		goto out;
	}
	if (enable != REGISTER_SYNC_ON)
		goto out;

	/*
	 * Get the address of the hash table in the target process.
	 */
	if (ta_p->model == PR_MODEL_NATIVE) {
		if (ps_pread(ph_p, ta_p->lwp_invar.sync_addr_hash_addr,
		    &hashaddr, sizeof (&hashaddr)) != PS_OK) {
			return_val = TD_DBERR;
			goto out;
		}
	} else {
#ifdef _SYSCALL32
		caddr32_t psaddr32;

		if (ps_pread(ph_p, ta_p->lwp_invar.sync_addr_hash_addr,
		    &psaddr32, sizeof (psaddr32)) != PS_OK) {
			return_val = TD_DBERR;
			goto out;
		}
		hashaddr = psaddr32;
#else
		return_val = TD_ERR;
		goto out;
#endif	/* _SYSCALL32 */
	}

	if (hashaddr == 0)
		return_val = TD_BADSH;
	else
		return_val = read_sync_stats(ta_p, hashaddr,
			sh_p->sh_unique, &sync_stats);
	if (return_val != TD_OK)
		goto out;

	/*
	 * We have the hash table entry.  Transfer the data to
	 * the td_syncstats_t structure provided by the caller.
	 */
	switch (sync_stats.un.type) {
	case TDB_MUTEX:
	    {
		td_mutex_stats_t *msp = &ss_p->ss_un.mutex;

		ss_p->ss_info.si_type = TD_SYNC_MUTEX;
		ss_p->ss_info.si_size = sizeof (mutex_t);
		msp->mutex_lock =
			sync_stats.un.mutex.mutex_lock;
		msp->mutex_sleep =
			sync_stats.un.mutex.mutex_sleep;
		msp->mutex_sleep_time =
			sync_stats.un.mutex.mutex_sleep_time;
		msp->mutex_hold_time =
			sync_stats.un.mutex.mutex_hold_time;
		msp->mutex_try =
			sync_stats.un.mutex.mutex_try;
		msp->mutex_try_fail =
			sync_stats.un.mutex.mutex_try_fail;
		if (sync_stats.sync_addr >= ta_p->hash_lock_addr &&
		    (ix = sync_stats.sync_addr - ta_p->hash_lock_addr)
		    < ta_p->hash_size * sizeof (mutex_t)) {
			msp->mutex_internal = ix / sizeof (mutex_t) + 1;
		}
		break;
	    }
	case TDB_COND:
	    {
		td_cond_stats_t *csp = &ss_p->ss_un.cond;

		ss_p->ss_info.si_type = TD_SYNC_COND;
		ss_p->ss_info.si_size = sizeof (cond_t);
		csp->cond_wait =
			sync_stats.un.cond.cond_wait;
		csp->cond_timedwait =
			sync_stats.un.cond.cond_timedwait;
		csp->cond_wait_sleep_time =
			sync_stats.un.cond.cond_wait_sleep_time;
		csp->cond_timedwait_sleep_time =
			sync_stats.un.cond.cond_timedwait_sleep_time;
		csp->cond_timedwait_timeout =
			sync_stats.un.cond.cond_timedwait_timeout;
		csp->cond_signal =
			sync_stats.un.cond.cond_signal;
		csp->cond_broadcast =
			sync_stats.un.cond.cond_broadcast;
		if (sync_stats.sync_addr >= ta_p->hash_cond_addr &&
		    (ix = sync_stats.sync_addr - ta_p->hash_cond_addr)
		    < ta_p->hash_size * sizeof (cond_t)) {
			csp->cond_internal = ix / sizeof (cond_t) + 1;
		}
		break;
	    }
	case TDB_RWLOCK:
	    {
		psaddr_t cond_addr;
		tdb_sync_stats_t cond_stats;
		td_rwlock_stats_t *rwsp = &ss_p->ss_un.rwlock;

		ss_p->ss_info.si_type = TD_SYNC_RWLOCK;
		ss_p->ss_info.si_size = sizeof (rwlock_t);
		rwsp->rw_rdlock =
			sync_stats.un.rwlock.rw_rdlock;
		cond_addr = (psaddr_t)((rwlock_t *)sh_p->sh_unique)->pad2;
		if (read_sync_stats(ta_p, hashaddr, cond_addr, &cond_stats)
		    == TD_OK) {
			rwsp->rw_rdlock_sleep =
				cond_stats.un.cond.cond_wait;
			rwsp->rw_rdlock_sleep_time =
				cond_stats.un.cond.cond_wait_sleep_time;
		}
		rwsp->rw_rdlock_try =
			sync_stats.un.rwlock.rw_rdlock_try;
		rwsp->rw_rdlock_try_fail =
			sync_stats.un.rwlock.rw_rdlock_try_fail;
		rwsp->rw_wrlock =
			sync_stats.un.rwlock.rw_wrlock;
		cond_addr = (psaddr_t)((rwlock_t *)sh_p->sh_unique)->pad3;
		if (read_sync_stats(ta_p, hashaddr, cond_addr, &cond_stats)
		    == TD_OK) {
			rwsp->rw_wrlock_sleep =
				cond_stats.un.cond.cond_wait;
			rwsp->rw_wrlock_sleep_time =
				cond_stats.un.cond.cond_wait_sleep_time;
		}
		rwsp->rw_wrlock_hold_time =
			sync_stats.un.rwlock.rw_wrlock_hold_time;
		rwsp->rw_wrlock_try =
			sync_stats.un.rwlock.rw_wrlock_try;
		rwsp->rw_wrlock_try_fail =
			sync_stats.un.rwlock.rw_wrlock_try_fail;
		break;
	    }
	case TDB_SEMA:
	    {
		td_sema_stats_t *ssp = &ss_p->ss_un.sema;

		ss_p->ss_info.si_type = TD_SYNC_SEMA;
		ss_p->ss_info.si_size = sizeof (sema_t);
		ssp->sema_wait =
			sync_stats.un.sema.sema_wait;
		ssp->sema_wait_sleep =
			sync_stats.un.sema.sema_wait_sleep;
		ssp->sema_wait_sleep_time =
			sync_stats.un.sema.sema_wait_sleep_time;
		ssp->sema_trywait =
			sync_stats.un.sema.sema_trywait;
		ssp->sema_trywait_fail =
			sync_stats.un.sema.sema_trywait_fail;
		ssp->sema_post =
			sync_stats.un.sema.sema_post;
		ssp->sema_max_count =
			sync_stats.un.sema.sema_max_count;
		ssp->sema_min_count =
			sync_stats.un.sema.sema_min_count;
		break;
	    }
	default:
		return_val = TD_BADSH;
		break;
	}

out:
	(void) ps_pcontinue(ph_p);
	ph_unlock(ta_p);
	return (return_val);
}

/*
 * Change the state of a synchronization variable.
 *	1) mutex lock state set to value
 *	2) semaphore's count set to value
 *	3) writer's lock set to value
 *	4) reader's lock number of readers set to value
 * Currently unused by dbx.
 */
#pragma weak td_sync_setstate = __td_sync_setstate
td_err_e
__td_sync_setstate(const td_synchandle_t *sh_p, long lvalue)
{
	struct ps_prochandle *ph_p;
	int		trunc = 0;
	td_err_e	return_val;
	td_so_un_t	generic_so;
	int		value = (int)lvalue;

	if ((ph_p = ph_lock_sh(sh_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pstop(ph_p) != PS_OK) {
		ph_unlock(sh_p->sh_ta_p);
		return (TD_DBERR);
	}

	/*
	 * Read the synch. variable information.
	 * First attempt to read the whole union and if that fails
	 * fall back to reading only the smallest member, the condvar.
	 */
	if (ps_pread(ph_p, sh_p->sh_unique, &generic_so,
	    sizeof (generic_so)) != PS_OK) {
		trunc = 1;
		if (ps_pread(ph_p, sh_p->sh_unique, &generic_so.condition,
		    sizeof (generic_so.condition)) != PS_OK) {
			(void) ps_pcontinue(ph_p);
			ph_unlock(sh_p->sh_ta_p);
			return (TD_DBERR);
		}
	}

	/*
	 * Set the new value in the sync. variable, read the synch. variable
	 * information. from the process, reset its value and write it back.
	 */
	switch (generic_so.condition.mutex_magic) {
	case MUTEX_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.lock, sizeof (generic_so.lock)) != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.lock.mutex_lockw = (uint8_t)value;
		if (ps_pwrite(ph_p, sh_p->sh_unique, &generic_so.lock,
		    sizeof (generic_so.lock)) != PS_OK)
			return_val = TD_DBERR;
		break;
	case SEMA_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.semaphore, sizeof (generic_so.semaphore))
		    != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.semaphore.count = value;
		if (ps_pwrite(ph_p, sh_p->sh_unique, &generic_so.semaphore,
		    sizeof (generic_so.semaphore)) != PS_OK)
			return_val = TD_DBERR;
		break;
	case COND_MAGIC:
		/* Operation not supported on a condition variable */
		return_val = TD_ERR;
		break;
	case RWL_MAGIC:
		if (trunc && ps_pread(ph_p, sh_p->sh_unique,
		    &generic_so.rwlock, sizeof (generic_so.rwlock)) != PS_OK) {
			return_val = TD_DBERR;
			break;
		}
		generic_so.rwlock.readers = value;
		if (ps_pwrite(ph_p, sh_p->sh_unique, &generic_so.rwlock,
		    sizeof (generic_so.rwlock)) != PS_OK)
			return_val = TD_DBERR;
		break;
	default:
		/* Bad sync. object type */
		return_val = TD_BADSH;
		break;
	}

	(void) ps_pcontinue(ph_p);
	ph_unlock(sh_p->sh_ta_p);
	return (return_val);
}

typedef struct {
	td_thr_iter_f	*waiter_cb;
	psaddr_t	sync_obj_addr;
	uint16_t	sync_magic;
	void		*waiter_cb_arg;
	td_err_e	errcode;
} waiter_cb_ctl_t;

static int
waiters_cb(const td_thrhandle_t *th_p, void *arg)
{
	td_thragent_t	*ta_p = th_p->th_ta_p;
	struct ps_prochandle *ph_p = ta_p->ph_p;
	waiter_cb_ctl_t	*wcb = arg;
	caddr_t		mblockaddr, rblockaddr, wblockaddr;
	caddr_t		wchan;

	if (ta_p->model == PR_MODEL_NATIVE) {
		ulwp_t *ulwp = (ulwp_t *)th_p->th_unique;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_wchan,
		    &wchan, sizeof (wchan)) != PS_OK) {
			wcb->errcode = TD_DBERR;
			return (1);
		}
	} else {
#ifdef _SYSCALL32
		ulwp32_t *ulwp = (ulwp32_t *)th_p->th_unique;
		caddr32_t wchan32;

		if (ps_pread(ph_p, (psaddr_t)&ulwp->ul_wchan,
		    &wchan32, sizeof (wchan32)) != PS_OK) {
			wcb->errcode = TD_DBERR;
			return (1);
		}
		wchan = (caddr_t)wchan32;
#else
		wcb->errcode = TD_ERR;
		return (1);
#endif	/* _SYSCALL32 */
	}

	if (wchan == NULL)
		return (0);

	switch (wcb->sync_magic) {
	case MUTEX_MAGIC:
	case COND_MAGIC:
	case SEMA_MAGIC:
		if (wchan == (caddr_t)wcb->sync_obj_addr)
			return ((*wcb->waiter_cb)(th_p, wcb->waiter_cb_arg));
		break;
	case RWL_MAGIC:
		mblockaddr = (caddr_t)((rwlock_t *)wcb->sync_obj_addr)->pad1;
		rblockaddr = (caddr_t)((rwlock_t *)wcb->sync_obj_addr)->pad2;
		wblockaddr = (caddr_t)((rwlock_t *)wcb->sync_obj_addr)->pad3;
		if (wchan == mblockaddr ||
		    wchan == rblockaddr ||
		    wchan == wblockaddr)
			return ((*wcb->waiter_cb)(th_p, wcb->waiter_cb_arg));
		break;
	}

	return (0);
}

/*
 * For a given synchronization variable, iterate over the
 * set of waiting threads.  The call back function is passed
 * two parameters, a pointer to a thread handle and a pointer
 * to extra call back data.
 */
#pragma weak td_sync_waiters = __td_sync_waiters
td_err_e
__td_sync_waiters(const td_synchandle_t *sh_p, td_thr_iter_f *cb, void *cb_data)
{
	struct ps_prochandle *ph_p;
	waiter_cb_ctl_t	wcb;
	td_err_e	return_val;

	if ((ph_p = ph_lock_sh(sh_p, &return_val)) == NULL)
		return (return_val);
	if (ps_pread(ph_p,
	    (psaddr_t)&((mutex_t *)sh_p->sh_unique)->mutex_magic,
	    (caddr_t)&wcb.sync_magic, sizeof (wcb.sync_magic)) != PS_OK) {
		ph_unlock(sh_p->sh_ta_p);
		return (TD_DBERR);
	}
	ph_unlock(sh_p->sh_ta_p);

	switch (wcb.sync_magic) {
	case MUTEX_MAGIC:
	case COND_MAGIC:
	case SEMA_MAGIC:
	case RWL_MAGIC:
		break;
	default:
		return (TD_BADSH);
	}

	wcb.waiter_cb = cb;
	wcb.sync_obj_addr = sh_p->sh_unique;
	wcb.waiter_cb_arg = cb_data;
	wcb.errcode = TD_OK;
	return_val = __td_ta_thr_iter(sh_p->sh_ta_p, waiters_cb, &wcb,
		TD_THR_SLEEP, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

	if (return_val != TD_OK)
		return (return_val);

	return (wcb.errcode);
}
