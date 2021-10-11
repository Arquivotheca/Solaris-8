/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpupart.c	1.14	99/06/05 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/var.h>
#include <sys/cyclic.h>

static cpupart_t *find_cp(cpupartid_t cpid);
extern void	disp_kp_alloc(pri_t);
extern void	disp_kp_free(disp_t *);

cpupart_t		*cp_list_head;
cpupart_t		cp_default;
kmutex_t		cp_list_lock;

int			num_partitions;
static cpupartid_t	cp_id_next;

/*
 * Initialize the default partition and kpreempt disp queue.
 */
void
cpupart_initialize_default()
{
	cp_list_head = &cp_default;
	cp_default.cp_next = &cp_default;
	cp_default.cp_prev = &cp_default;
	cp_default.cp_base = &cp_default;
	cp_default.cp_id = 0;
	cp_default.cp_level = CP_DEFAULT;
	cp_default.cp_kp_queue.disp_maxrunpri = -1;
	cp_default.cp_kp_queue.disp_max_unbound_pri = -1;
	cp_default.cp_kp_queue.disp_cpu = NULL;
	DISP_LOCK_INIT(&cp_default.cp_kp_queue.disp_lock);
	cp_id_next = 1;
	num_partitions = 1;
}


/*ARGSUSED*/
static int
cpupart_move_cpu(cpu_t *cp, cpupart_t *newpp, int active)
{
	cpupart_t *oldpp, *basepp;
	cpu_t	*ncp, *newlist;
	kthread_t *t;
	int	move_threads = 1;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(newpp != NULL);

	oldpp = cp->cpu_part;
	ASSERT(oldpp != NULL);
	ASSERT(oldpp->cp_ncpus > 0);

	if (newpp == oldpp) {
		/*
		 * Don't need to do anything.
		 */
		return (0);
	}

	/*
	 * No hierarchies of private partitions for now.
	 */
	ASSERT(active || CP_PUBLIC(newpp));

	if (!active || (CP_PUBLIC(newpp) && CP_PUBLIC(oldpp)) ||
	    !disp_bound_partition(cp)) {
		/*
		 * Don't need to move threads if removing an inactive
		 * partition, or if we're moving to and from a "public"
		 * partition, or if there's no threads in the partition.
		 * Note that threads can't enter the partition while
		 * we're holding the cp_list_lock.
		 */
		move_threads = 0;
	} else if (oldpp->cp_ncpus == 1) {
		return (EBUSY);
	}

	if (move_threads) {
		int loop_count;
		/*
		 * Check for threads bound to this CPU.
		 */
		for (loop_count = 0; disp_bound_threads(cp); loop_count++) {
			if (loop_count >= 5) {
				return (EBUSY);	/* some threads still bound */
			}
			delay(1);
		}
	}

	/*
	 * Before we actually start changing data structures, notify
	 * the cyclic subsystem that we want to move this CPU out of its
	 * partition.
	 */
	if (!cyclic_move_out(cp)) {
		/*
		 * This CPU must be the last CPU in a processor set with
		 * a bound cyclic.
		 */
		return (EBUSY);
	}

	if (newpp->cp_base != NULL) {
		if (newpp->cp_base != oldpp->cp_base &&
		    newpp->cp_level == oldpp->cp_level)
			return (EINVAL);
	} else {
		basepp = oldpp;
		while (newpp->cp_level < basepp->cp_level)
			basepp = basepp->cp_base;
		if (oldpp->cp_level < newpp->cp_level)
			newpp->cp_base = oldpp;
		else
			newpp->cp_base = oldpp->cp_base;
	}

	pause_cpus(cp);

	/* move out of old partition */
	oldpp->cp_ncpus--;
	if (oldpp->cp_ncpus > 0) {
		ncp = cp->cpu_prev_part->cpu_next_part = cp->cpu_next_part;
		cp->cpu_next_part->cpu_prev_part = cp->cpu_prev_part;
		if (oldpp->cp_cpulist == cp) {
			oldpp->cp_cpulist = ncp;
		}
	} else {
		ncp = oldpp->cp_cpulist = NULL;
	}

	/* move into new partition */
	newlist = newpp->cp_cpulist;
	if (newlist == NULL) {
		newpp->cp_cpulist = cp->cpu_next_part = cp->cpu_prev_part = cp;
	} else {
		cp->cpu_next_part = newlist;
		cp->cpu_prev_part = newlist->cpu_prev_part;
		newlist->cpu_prev_part->cpu_next_part = cp;
		newlist->cpu_prev_part = cp;
	}
	cp->cpu_part = newpp;
	newpp->cp_ncpus++;

	/*
	 * If necessary, move threads off processor.
	 */
	if (move_threads) {
		ASSERT(ncp != NULL);

		t = curthread;
		do {
			if (t->t_cpu == cp &&
			    t->t_cpupart == oldpp &&
			    t->t_bound_cpu != cp) {
				t->t_cpu = disp_lowpri_cpu(ncp, 0);
			}
			t = t->t_next;
		} while (t != curthread);

		/*
		 * Clear off the CPU's run queue, and the kp queue if the
		 * partition is now empty.
		 */
		disp_cpu_inactive(cp);

		/*
		 * Make cp switch to a thread from the new partition.
		 */
		cp->cpu_runrun = 1;
		cp->cpu_kprunrun = 1;
	} else {
		/*
		 * If we're not moving the threads, fix the partition
		 * pointers (and pset bindings).
		 */
		t = curthread;
		do {
			ASSERT(t != NULL);
			if (t->t_cpu == cp ||
			    (ncp == NULL && t->t_cpupart == oldpp)) {
				t->t_cpupart = newpp;
				t->t_bind_pset = PS_NONE;
				if (t->t_state == TS_RUN &&
				    t->t_disp_queue == &oldpp->cp_kp_queue) {
					thread_lock(t);
					(void) dispdeq(t);
					setbackdq(t);
					thread_unlock(t);
				}
			}
			t = t->t_next;
		} while (t != curthread);
	}

	start_cpus();

	/*
	 * Now let the cyclic subsystem know that it can reshuffle cyclics
	 * bound to the new processor set.
	 */
	cyclic_move_in(cp);

	return (0);
}



static int
cpupart_move_thread(kthread_id_t tp, cpupart_t *newpp)
{
	cpupart_t *oldpp = tp->t_cpupart;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&ttoproc(tp)->p_lock));
	ASSERT(newpp != NULL);

	if (newpp->cp_cpulist == NULL) {
		/*
		 * If we're unbinding from a system partition, and there
		 * are no processors in the default partition, just leave
		 * the thread implicitly bound to the system partition,
		 * but return 0 (no error) so that the pset_bind operation
		 * will succeed.  We want the thread to return to the state
		 * it was in before the user bound it to the system
		 * partition.  If we don't satisfy these conditions, just
		 * return EINVAL.
		 */
		if (CP_PUBLIC(oldpp) && newpp->cp_level == CP_DEFAULT) {
			return (0);
		} else {
			return (EINVAL);
		}
	}

	thread_lock(tp);
	/* don't move bound threads */
	if (tp->t_bound_cpu != NULL &&
	    tp->t_bound_cpu->cpu_part != newpp) {
		thread_unlock(tp);
		return (EBUSY);
	}

	/* move the thread */
	if (oldpp != newpp) {
		/*
		 * Make the thread switch to the new partition.
		 */
		tp->t_cpupart = newpp;
		if (tp->t_state == TS_ONPROC) {
			cpu_surrender(tp);
		} else if (tp->t_state == TS_RUN) {
			(void) dispdeq(tp);
			setbackdq(tp);
		}
	}

	/*
	 * Our binding has changed; set TP_CHANGEBIND.
	 */
	tp->t_proc_flag |= TP_CHANGEBIND;
	aston(tp);

	thread_unlock(tp);

	return (0);		/* success */
}


/*
 * Migrate a thread to the "public" partition with the lightest load.
 * This is usually called at exec time.
 */
void
cpupart_migrate()
{
	kthread_t *t = curthread;
	cpu_t *cp;

	ASSERT(MUTEX_NOT_HELD(&pidlock));
	ASSERT(MUTEX_NOT_HELD(&ttoproc(t)->p_lock));

	if (t->t_bind_pset != PS_NONE)
		return;
	mutex_enter(&cp_list_lock);
	cp = disp_lowpri_cpu(t->t_cpu, 1);
	if (cp->cpu_part != t->t_cpu->cpu_part) {
		t->t_cpupart = cp->cpu_part;
		mutex_exit(&cp_list_lock);
		preempt();
	} else {
		/*
		 * Don't bother migrating if it's just going to be
		 * within the same partition.
		 */
		mutex_exit(&cp_list_lock);
	}
}


/*
 * This function binds a thread to a partitions.  It must be called with
 * cp_list_lock already held, since cp_list_lock must be acquired before
 * p_lock.
 */
int
cpupart_bind_thread(kthread_id_t tp, cpupartid_t cpid)
{
	cpupart_t	*newpp;
	int		err;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&ttoproc(tp)->p_lock));

	if (cpid == CP_NONE) {
		newpp = tp->t_cpupart->cp_base;
	} else {
		newpp = find_cp(cpid);
		if (newpp == NULL)
			return (EINVAL);
	}
	err = cpupart_move_thread(tp, newpp);
	return (err);
}


/*
 * Create a new partition at the designated level.  On MP systems,
 * this also allocates a kpreempt disp queue for that partition.
 */
int
cpupart_create(cpupartid_t *cpid, u_int level)
{
	cpupart_t	*pp;

	ASSERT(level != CP_DEFAULT);
	pp = kmem_zalloc(sizeof (cpupart_t), KM_SLEEP);
	mutex_enter(&cp_list_lock);
	if (num_partitions > ncpus * 2) {	/* sanity check */
		mutex_exit(&cp_list_lock);
		kmem_free(pp, sizeof (cpupart_t));
		return (ENOMEM);
	}
	while (find_cp(cp_id_next++) != NULL)
		;
	pp->cp_id = cp_id_next - 1;
	pp->cp_next = cp_list_head;
	pp->cp_prev = cp_list_head->cp_prev;
	cp_list_head->cp_prev->cp_next = pp;
	cp_list_head->cp_prev = pp;
	pp->cp_level = level;
	pp->cp_base = NULL;
	pp->cp_ncpus = 0;
	pp->cp_cpulist = NULL;
	pp->cp_kp_queue.disp_maxrunpri = -1;
	pp->cp_kp_queue.disp_max_unbound_pri = -1;
	pp->cp_kp_queue.disp_cpu = NULL;
	DISP_LOCK_INIT(&pp->cp_kp_queue.disp_lock);
	*cpid = pp->cp_id;
	num_partitions++;
	disp_kp_alloc(v.v_nglobpris);
	mutex_exit(&cp_list_lock);

	return (0);
}


/*
 * Destroy a partition.
 */
int
cpupart_destroy(cpupartid_t cpid)
{
	cpu_t	*cp, *first_cp;
	cpupart_t *pp, *opp, *basepp;
	int	err = 0;

	mutex_enter(&cp_list_lock);	/* protects cpu partition list */
	mutex_enter(&cpu_lock);		/* protects cpu list */

	pp = find_cp(cpid);
	if (pp == NULL || pp == &cp_default) {
		mutex_exit(&cpu_lock);
		mutex_exit(&cp_list_lock);
		return (EINVAL);
	}

	/*
	 * cpupart_move_cpu automatically moves the threads in the partition
	 * back to the base partition when all processors are moved out.
	 */
	basepp = pp->cp_base;
	while ((cp = pp->cp_cpulist) != NULL) {
		if (err = cpupart_move_cpu(cp, basepp, 0)) {
			mutex_exit(&cpu_lock);
			mutex_exit(&cp_list_lock);
			return (err);
		}
	}

	/*
	 * Reset the pointers in any offline processors so they won't
	 * try to rejoin the destroyed partition when they're turned
	 * online.
	 */
	first_cp = cp = CPU;
	do {
		if (cp->cpu_part == pp) {
			ASSERT(cp->cpu_flags & CPU_OFFLINE);
			cp->cpu_part = basepp;
		}
		cp = cp->cpu_next;
	} while (cp != first_cp);

	mutex_exit(&cpu_lock);

	pp->cp_prev->cp_next = pp->cp_next;
	pp->cp_next->cp_prev = pp->cp_prev;
	if (cp_list_head == pp)
		cp_list_head = pp->cp_next;

	/*
	 * Make sure that no other partition has a pointer to
	 * this one as a base.
	 */
	opp = cp_list_head;
	do {
		if (opp->cp_base == pp)
			opp->cp_base = pp->cp_base;
		opp = opp->cp_next;
	} while (opp != cp_list_head);

	if (cp_id_next > pp->cp_id)
		cp_id_next = pp->cp_id;

	disp_kp_free(&pp->cp_kp_queue);

	kmem_free(pp, sizeof (cpupart_t));
	num_partitions--;

	mutex_exit(&cp_list_lock);
	return (err);
}


/*
 * Return the ID of the partition to which the specified processor belongs.
 */
cpupartid_t
cpupart_query_cpu(cpu_t *cp)
{
	cpupartid_t	cpid;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	cpid = cp->cpu_part->cp_id;

	return (cpid);
}


/*
 * Return the ID of the partition to which the specified processor belongs.
 */
cpupartid_t
cpupart_query_thread(kthread_id_t tp)
{
	cpupartid_t	cpid;

	mutex_enter(&cp_list_lock);
	cpid = tp->t_cpupart->cp_id;
	mutex_exit(&cp_list_lock);

	return (cpid);
}


/*
 * Attach a processor to an existing partition.
 */
int
cpupart_attach_cpu(cpupartid_t cpid, cpu_t *cp)
{
	cpupart_t	*pp;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&cpu_lock));

	if (cpid == CP_NONE)
		pp = cp->cpu_part->cp_base;
	else {
		pp = find_cp(cpid);
		if (pp == NULL) {
			return (EINVAL);
		}
	}
	if (cp->cpu_flags & CPU_OFFLINE) {
		return (EINVAL);
	}

	return (cpupart_move_cpu(cp, pp, 1));
}


/*
 * Get the partition level.
 */
int
cpupart_get_level(cpupartid_t cpid, u_int *levelp)
{
	cpupart_t	*pp;

	ASSERT(MUTEX_HELD(&cp_list_lock));
	pp = find_cp(cpid);
	if (pp == NULL) {
		return (EINVAL);
	}

	ASSERT(levelp != NULL);
	*levelp = pp->cp_level;
	return (0);
}


/*
 * Get a list of cpus belonging to the partition.  If numcpus is NULL,
 * this just checks for a valid partition.  If numcpus is non-NULL but
 * cpulist is NULL, the current number of cpus is stored in *numcpus.
 * If both are non-NULL, the current number of cpus is stored in *numcpus,
 * and a list of those cpus up to the size originally in *numcpus is
 * stored in cpulist[].
 */
int
cpupart_get_cpus(cpupartid_t cpid, processorid_t *cpulist, u_int *numcpus)
{
	cpupart_t	*pp;
	u_int		ncpus;
	cpu_t		*c;
	int		i;

	mutex_enter(&cp_list_lock);
	pp = find_cp(cpid);
	if (pp == NULL) {
		mutex_exit(&cp_list_lock);
		return (EINVAL);
	}
	ncpus = pp->cp_ncpus;
	if (numcpus) {
		if (ncpus > *numcpus) {
			/*
			 * Only copy as many cpus as were passed in, but
			 * pass back the real number.
			 */
			u_int t = ncpus;
			ncpus = *numcpus;
			*numcpus = t;
		} else
			*numcpus = ncpus;

		if (cpulist) {
			c = pp->cp_cpulist;
			for (i = 0; i < ncpus; i++) {
				ASSERT(c != NULL);
				cpulist[i] = c->cpu_id;
				c = c->cpu_next_part;
			}
		}
	}
	mutex_exit(&cp_list_lock);
	return (0);
}


static cpupart_t *
find_cp(cpupartid_t cpid)
{
	cpupart_t *cp;

	ASSERT(MUTEX_HELD(&cp_list_lock));

	cp = cp_list_head;
	do {
		if (cp->cp_id == cpid)
			return (cp);
		cp = cp->cp_next;
	} while (cp != cp_list_head);
	return (NULL);
}
