/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pthread.c	1.2	99/11/02 SMI"

#include "liblwp.h"
#include <sys/syscall.h>

/*
 * The following variable is initialized to 1 if libpthread is loaded, and that
 * is the indication that user wants POSIX behavior.
 */
int _libpthread_loaded = 0;		/* set to 1 by libpthread init */

/*
 * pthread_once related data
 * This structure is exported as pthread_once_t in pthread.h.
 * We export only the size of this structure. so check
 * pthread_once_t in pthread.h before making a change here.
 */
typedef struct  __once {
	mutex_t	mlock;
	union {
		uint8_t		pad8_flag[8];
		uint64_t	pad64_flag;
	} oflag;
} __once_t;

#define	once_flag	oflag.pad8_flag[7]

/*
 * pthread_atfork related data
 * This is internal structure and is used to store atfork handlers.
 */
typedef struct __atfork {
	struct	__atfork *fwd;		/* forward pointer */
	struct	__atfork *bckwd;	/* backward pointer */
	void (*prepare)(void);		/* pre-fork handler */
	void (*child)(void);		/* post-fork child handler */
	void (*parent)(void);		/* post-fork parent handler */
} __atfork_t;

__atfork_t *_atforklist = NULL;		/* circular Q for fork handlers */
mutex_t	_atforklock;			/* protect the above Q */

__atfork_t *_latforklist = NULL;	/* circular Q for rtld fork handlers */
mutex_t _latforklock;			/* protex the above Q */

/* default attribute object for pthread_create with NULL attr pointer */
thrattr_t _defattr =
	{NULL, 0, NULL, PTHREAD_CREATE_JOINABLE, PTHREAD_SCOPE_PROCESS, 0};

/*
 * This function is called from libpthread's init section.
 */
void
__pthread_init(void)
{
	_libpthread_loaded = 1;
	_defattr.guardsize = _lpagesize = _sysconf(_SC_PAGESIZE);
}

/*
 * pthread_create: creates a thread in the current process.
 * calls common _thrp_create() after copying the attributes.
 */
#pragma weak	pthread_create			= _pthread_create
#pragma weak	_liblwp_pthread_create		= _pthread_create
int
_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	void * (*start_routine)(void *), void *arg)
{
	long		flag;
	thrattr_t	*ap;
	pthread_t	tid;
	int		policy;
	pri_t		priority;
	int		error;
	int		mapped = 0;
	int		mappedpri;
	int		rt = 0;

	if (attr == NULL)
		ap = &_defattr;
	else if ((ap = attr->__pthread_attrp) == NULL)
		return (EINVAL);

	if (ap->inherit == PTHREAD_INHERIT_SCHED) {
		ulwp_t *self = curthread;
		policy = self->ul_policy;
		priority = self->ul_pri;
		mapped = self->ul_pri_mapped;
		mappedpri = self->ul_mappedpri;
	} else {
		policy = ap->policy;
		priority = ap->prio;
		if (policy == SCHED_OTHER ||
		    (ap->scope == PTHREAD_SCOPE_PROCESS &&	/* a pretense */
		    (policy == SCHED_FIFO || policy == SCHED_RR))) {
			if (priority < THREAD_MIN_PRIORITY ||
			    priority > THREAD_MAX_PRIORITY) {
				if (_validate_rt_prio(policy, priority))
					return (EINVAL);
				mapped = 1;
				mappedpri = priority;
				priority = _map_rtpri_to_gp(priority);
				ASSERT(priority >= THREAD_MIN_PRIORITY &&
				    priority <= THREAD_MAX_PRIORITY);
			}
		} else if (policy == SCHED_FIFO || policy == SCHED_RR) {
			if (_geteuid() != 0)
				return (EPERM);
			if (_validate_rt_prio(policy, priority))
				return (EINVAL);
			rt = 1;
		} else {
			return (EINVAL);
		}
	}

	flag = ap->scope | ap->detachstate | THR_SUSPENDED;
	error = _thrp_create(ap->stkaddr, ap->stksize, start_routine, arg,
		flag, &tid, priority, policy, ap->guardsize);
	if (error == 0) {
		if (mapped) {
			ulwp_t *ulwp = find_lwp(tid);
			ulwp->ul_pri_mapped = 1;
			ulwp->ul_mappedpri = mappedpri;
			ulwp_unlock(ulwp);
		}
		if (rt && _thrp_setlwpprio(tid, policy, priority))
			panic("_thrp_setlwpprio() falied");
		if (thread)
			*thread = tid;
		(void) _thr_continue(tid);
	}

	/* posix version expects EAGAIN for lack of memory */
	if (error == ENOMEM)
		error = EAGAIN;
	return (error);
}

/*
 * pthread_once: calls given function only once.
 * it synchronizes via mutex in pthread_once_t structure
 */
#pragma weak	pthread_once			= _pthread_once
#pragma weak	_liblwp_pthread_once		= _pthread_once
int
_pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	__once_t *once = (__once_t *)once_control;

	if (once == NULL || init_routine == NULL)
		return (EINVAL);

	(void) _mutex_lock(&once->mlock);
	if (once->once_flag == PTHREAD_ONCE_NOTDONE) {
		pthread_cleanup_push(_mutex_unlock, &once->mlock);
		(*init_routine)();
		pthread_cleanup_pop(0);
		once->once_flag = PTHREAD_ONCE_DONE;
	}
	(void) _mutex_unlock(&once->mlock);

	return (0);
}

/*
 * pthread_equal: equates two thread ids.
 */
#pragma weak	pthread_equal			= _pthread_equal
#pragma weak	_liblwp_pthread_equal		= _pthread_equal
int
_pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

/*
 * pthread_getschedparam: gets the sched parameters in a struct.
 */
#pragma weak	pthread_getschedparam		= _pthread_getschedparam
#pragma weak	_liblwp_pthread_getschedparam	= _pthread_getschedparam
int
_pthread_getschedparam(pthread_t tid, int *policy, struct sched_param *param)
{
	ulwp_t *ulwp;
	int error = 0;

	if (param == NULL || policy == NULL)
		error = EINVAL;
	else if ((ulwp = find_lwp(tid)) == NULL)
		error = ESRCH;
	else {
		if (ulwp->ul_pri_mapped)
			param->sched_priority = ulwp->ul_mappedpri;
		else
			param->sched_priority = ulwp->ul_pri;
		*policy = ulwp->ul_policy;
		ulwp_unlock(ulwp);
	}

	return (error);
}

/*
 * Besides the obvious arguments, the inheritflag needs to be explained:
 * If set to PRIO_SET, it does the normal, expected work of setting thread's
 * assigned scheduling parameters and policy, including the lwp if necessary.
 * If set to PRIO_INHERIT, it sets the thread's effective priority values
 * (t_epri, t_empappedpri), and does not update the assigned priority values
 * (t_pri, t_mappedpri), again influencing the lwp if necessary.
 * If set to PRIO_DISINHERIT, it clears the thread's effective priority
 * values, and reverts the lwp (if necessary), back to the assigned priority
 * values.
 */
int
_thread_setschedparam_main(pthread_t tid, int policy,
    const struct sched_param *param, int inheritflag)
{
	ulwp_t	*ulwp;
	int	error = 0;
	int	prio;
	int	usropts;
	int	opolicy;
	int	mappedprio;
	int	mapped = 0;
	pri_t	*mappedprip;

	if (param == NULL)
		return (EINVAL);
	if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);
	prio = param->sched_priority;
	usropts = ulwp->ul_usropts;
	opolicy = ulwp->ul_policy;
	ASSERT(inheritflag == PRIO_SET || opolicy == policy);
	if (inheritflag == PRIO_DISINHERIT) {
		ulwp->ul_emappedpri = 0;
		ulwp->ul_epri = 0;
		prio = ulwp->ul_pri;	/* ignore prio in sched_param */
	}
	if (policy == SCHED_OTHER ||
	    (!(usropts & THR_BOUND) &&	/* a pretense */
	    (policy == SCHED_FIFO || policy == SCHED_RR))) {
		/*
		 * Set unbound thread's policy to one of FIFO/RR/OTHER
		 * OR set bound thread's policy to OTHER
		 */
		if (prio < THREAD_MIN_PRIORITY || prio > THREAD_MAX_PRIORITY) {
			if (_validate_rt_prio(policy, prio)) {
				error = EINVAL;
				goto out;
			}
			mapped = 1;
			mappedprio = prio;
			prio = _map_rtpri_to_gp(prio);
			ASSERT(prio >= THREAD_MIN_PRIORITY &&
			    prio <= THREAD_MAX_PRIORITY);
		}
		/*
		 * Bound thread changing from FIFO/RR to OTHER
		 */
		if ((opolicy == SCHED_FIFO || opolicy == SCHED_RR) &&
		    (usropts & THR_BOUND)) {
			if ((error = _thrp_setlwpprio(tid, policy, prio)) != 0)
				goto out;
		}
		if (inheritflag != PRIO_DISINHERIT) {
			if (inheritflag == PRIO_INHERIT)
				mappedprip = &ulwp->ul_emappedpri;
			else
				mappedprip = &ulwp->ul_mappedpri;
			if (mapped) {
				ulwp->ul_pri_mapped = 1;
				*mappedprip = mappedprio;
			} else {
				ulwp->ul_pri_mapped = 0;
				*mappedprip = 0;
			}
		}
		ulwp->ul_policy = policy;
		if (inheritflag == PRIO_INHERIT)
			ulwp->ul_epri = prio;
		else
			ulwp->ul_pri = prio;
	} else if (policy == SCHED_FIFO || policy == SCHED_RR) {
		if (_validate_rt_prio(policy, prio))
			error = EINVAL;
		else if (_geteuid() != 0)
			error = EPERM;
		else {
			if (_thrp_setlwpprio(tid, policy, prio))
				panic("_thrp_setlwpprio failed");
			ulwp->ul_policy = policy;
			if (inheritflag == PRIO_INHERIT)
				ulwp->ul_epri = prio;
			else
				ulwp->ul_pri = prio;
		}
	} else {
		error = EINVAL;
	}

out:
	ulwp_unlock(ulwp);
	return (error);
}

/*
 * pthread_setschedparam: sets the sched parameters for a thread.
 */
#pragma weak	pthread_setschedparam		= _pthread_setschedparam
#pragma weak	_liblwp_pthread_setschedparam	= _pthread_setschedparam
int
_pthread_setschedparam(pthread_t tid,
	int policy, const struct sched_param *param)
{
	return (_thread_setschedparam_main(tid, policy, param, PRIO_SET));
}

/*
 * Routine to maintain the atfork queues.  This is called by both
 * _pthread_atfork() & _lpthread_atfork().
 */
int
_atfork_append(void (*prepare) (void), void (*parent) (void),
		void (*child) (void), __atfork_t ** atfork_q,
		mutex_t *mlockp)
{
	__atfork_t *atfp;

	if ((atfp = malloc(sizeof (__atfork_t))) == NULL) {
		return (ENOMEM);
	}
	atfp->prepare = prepare;
	atfp->child = child;
	atfp->parent = parent;

	(void) _mutex_lock(mlockp);
	if (*atfork_q == NULL) {
		*atfork_q = atfp;
		atfp->fwd = atfp->bckwd = atfp;
	} else {
		(*atfork_q)->bckwd->fwd = atfp;
		atfp->fwd = *atfork_q;
		atfp->bckwd = (*atfork_q)->bckwd;
		(*atfork_q)->bckwd = atfp;
	}
	(void) _mutex_unlock(mlockp);

	return (0);
}

/*
 * pthread_atfork: installs handlers to be called during fork().
 * It allocates memory off the heap and put the handler addresses
 * in circular Q. Once installed atfork handlers can not be removed.
 * There is no POSIX API which allows to "delete" atfork handlers.
 */
#pragma weak	pthread_atfork			= _pthread_atfork
#pragma weak	_liblwp_pthread_atfork		= _pthread_atfork
int
_pthread_atfork(void (*prepare) (void), void (*parent) (void),
    void (*child) (void))
{
	return (_atfork_append(prepare, parent, child,
	    &_atforklist, &_atforklock));
}

#pragma weak	lpthread_atfork			= _lpthread_atfork
#pragma weak	_liblwp_lpthread_atfork		= _lpthread_atfork
int
_lpthread_atfork(void (*prepare) (void), void (*parent) (void),
    void (*child) (void))
{
	return (_atfork_append(prepare, parent, child,
	    &_latforklist, &_latforklock));
}

void
_run_prefork(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp, *last;

	(void) _mutex_lock(mlockp);
	if (atfork_q != NULL) {
		atfp = last = atfork_q->bckwd;
		do {
			if (atfp->prepare) {
				pthread_cleanup_push(_mutex_unlock,
				    (void *)mlockp);
				(*(atfp->prepare))();
				pthread_cleanup_pop(0);
			}
			atfp = atfp->bckwd;
		} while (atfp != last);
	}
	/* _?atforklock mutex is unlocked by _postfork_child/parent_handler */
}


/*
 * _prefork_handler is called by fork1() before it starts processing.
 * It acquires global atfork lock to protect the circular list.
 * It executes user installed "prepare" routines in LIFO order (POSIX)
 */
void
_prefork_handler(void)
{
	_run_prefork(_atforklist, &_atforklock);
}

void
_lprefork_handler(void)
{
	_run_prefork(_latforklist, &_latforklock);
}

void
_run_postfork_parent(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp;

	if (atfork_q != NULL) {
		atfp = atfork_q;
		do {
			if (atfp->parent) {
				pthread_cleanup_push(_mutex_unlock,
				    (void *)mlockp);
				(*(atfp->parent))();
				pthread_cleanup_pop(0);
			}
			atfp = atfp->fwd;
		} while (atfp != atfork_q);
	}
	(void) _mutex_unlock(mlockp);
}

/*
 * _postfork_parent_handler is called by fork1() after it finishes parent
 * processing. It acquires global atfork lock to protect the circular Q.
 * It executes user installed "parent" routines in FIFO order (POSIX).
 */
void
_postfork_parent_handler(void)
{
	_run_postfork_parent(_atforklist, &_atforklock);
}

void
_lpostfork_parent_handler(void)
{
	_run_postfork_parent(_latforklist, &_latforklock);
}

void
_run_postfork_child(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp;

	if (atfork_q != NULL) {
		atfp = atfork_q;
		do {
			if (atfp->child) {
				pthread_cleanup_push(_mutex_unlock,
				    (void *)mlockp);
				(*(atfp->child))();
				pthread_cleanup_pop(0);
			}
			atfp = atfp->fwd;
		} while (atfp != atfork_q);
	}
	(void) _mutex_unlock(mlockp);
}

/*
 * _postfork_child_handler is called by fork1() after it finishes child
 * processing. It acquires global atfork lock to protect the circular Q.
 * It executes user installed "child" routines in FIFO order (POSIX).
 */
void
_postfork_child_handler(void)
{
	_run_postfork_child(_atforklist, &_atforklock);
}

void
_lpostfork_child_handler(void)
{
	_run_postfork_child(_latforklist, &_latforklock);
}
