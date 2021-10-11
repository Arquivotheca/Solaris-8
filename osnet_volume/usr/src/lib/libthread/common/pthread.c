/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pthread.c	1.31	99/11/19 SMI"

#ifdef __STDC__

#pragma weak	pthread_create		= _pthread_create
#pragma weak	pthread_join 		= _pthread_join
#pragma weak	pthread_once		= _pthread_once
#pragma weak	pthread_equal		= _pthread_equal
#pragma weak	pthread_setschedparam	= _pthread_setschedparam
#pragma weak	pthread_getschedparam	= _pthread_getschedparam
#pragma weak	pthread_getspecific	= _pthread_getspecific
#pragma weak	pthread_atfork		= _pthread_atfork
#pragma weak	pthread_getconcurrency	= _pthread_getconcurrency
#pragma weak	pthread_setconcurrency	= _pthread_setconcurrency

#pragma weak	_ti_pthread_create		= _pthread_create
#pragma weak	_ti_pthread_join 		= _pthread_join
#pragma weak	_ti_pthread_once		= _pthread_once
#pragma weak	_ti_pthread_equal		= _pthread_equal
#pragma weak	_ti_pthread_setschedparam	= _pthread_setschedparam
#pragma weak	_ti_pthread_getschedparam	= _pthread_getschedparam
#pragma weak	_ti_pthread_getspecific		= _pthread_getspecific
#pragma	weak	_ti_pthread_atfork		= _pthread_atfork
#pragma weak	_ti_pthread_getconcurrency	= _pthread_getconcurrency
#pragma weak	_ti_pthread_setconcurrency	= _pthread_setconcurrency

#endif /* __STDC__ */

#include <string.h>
#include <sched.h>
#include <stdlib.h>
#include "libpthr.h"
#include "libthread.h"

/*
 * The following variable is initialized to 1 if libpthread is loaded, and that
 * is the indication that user wants POSIX behavior.
 */
uint8_t _libpthread_loaded = 0;		/* set to 1 by libpthread init */
static uint8_t _first_setconcurrency;

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

static __atfork_t *_atforklist = NULL;	/* circular Q for fork handlers */
static	mutex_t	_atforklock;		/* protect the above Q */

static __atfork_t *_latforklist = NULL;	/* circular Q for rtld fork handlers */
static	mutex_t _latforklock;		/* protex the above Q */

/* default attribute object for pthread_create with NULL attr pointer */
static thrattr_t	_defattr = {NULL, 0, NULL, PTHREAD_CREATE_JOINABLE,
					PTHREAD_SCOPE_PROCESS, 0};

/*
 * This function is called from libpthread's init section.
 */
void
__pthread_init(void)
{
	_libpthread_loaded = 1;
	_defattr.guardsize = _lpagesize;
}

/*
 * pthread_create: creates a thread in the current process.
 * calls common _thrp_create() after copying the attributes.
 */
int
_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
					void * (*start_routine)(void *),
					void *arg)
{
	long		flag;
	thrattr_t	*ap;
	int		ret;
	size_t		guardsize;
	int		prio;
	int		policy;
	int		mappedprioflag = 0, rt = 0;
	int		mappedprio;
	pthread_t	tid;

	if (attr != NULL) {
		if ((ap = attr->__pthread_attrp) == NULL)
			return (EINVAL);
	} else {
		ap = &_defattr;
	}

	flag = ap->scope | ap->detachstate;
	flag |= THR_SUSPENDED;
	guardsize = ap->stkaddr ? 0 : ap->guardsize;
	if (ap->inherit == PTHREAD_INHERIT_SCHED) {
		prio = curthread->t_pri;
		policy = curthread->t_policy;
	} else {
		prio = ap->prio;
		policy = ap->policy;
		if ((policy == SCHED_OTHER) ||
			((ap->scope == PTHREAD_SCOPE_PROCESS) &&
			(policy == SCHED_FIFO || policy == SCHED_RR))) {
			if (CHECK_PRIO(prio)) {
				if (!_validate_rt_prio(policy, prio)) {
					mappedprio = prio;
					prio = _map_rtpri_to_gp(prio);
					ASSERT(!CHECK_PRIO(prio));
					mappedprioflag = 1;
				} else {
					return (EINVAL);
				}
			}
		} else if (policy == SCHED_FIFO || policy == SCHED_RR) {
			if (_geteuid() != 0) {
				return (EPERM);
			}
			if (_validate_rt_prio(policy, prio)) {
				return (EINVAL);
			}
			rt = 1;
		} else {
			return (EINVAL);
		}
	}
	if (ap->scope == PTHREAD_SCOPE_PROCESS) {
		if (policy == SCHED_RR) {
			flag |= THR_NEW_LWP;
		}
	}
	ret = _thrp_create(ap->stkaddr, ap->stksize, start_routine,
	    arg, flag, &tid, prio, policy, guardsize);
	if (!ret) {
		int ix;
		uthread_t *t;

		_lock_bucket((ix = HASH_TID(tid)));
		t = THREAD(tid);
		if (mappedprioflag) {
			t->t_mappedpri = mappedprio;
		}
		_unlock_bucket(ix);
		if (rt && _thrp_setlwpprio(t->t_lwpid, policy, prio))
			_panic("_thrp_setlwpprio failed");
		if (thread)
			*thread = tid;
		_thr_continue(tid);
	}
	if (ret == ENOMEM)
		/* posix version expects EAGAIN for lack of memory */
		return (EAGAIN);
	else
		return (ret);
}

/*
 * pthread_once: calls given function only once.
 * it synchronizes via mutex in pthread_once_t structure
 */
int
_pthread_once(pthread_once_t *once_control,
					void (*init_routine)(void))
{
	if (once_control == NULL || init_routine == NULL)
		return (EINVAL);

	_mutex_lock(&((__once_t *)once_control)->mlock);
	if (((__once_t *)once_control)->once_flag == PTHREAD_ONCE_NOTDONE) {

		pthread_cleanup_push(_mutex_unlock,
		    &((__once_t *)once_control)->mlock);

		(*init_routine)();

		pthread_cleanup_pop(0);

		((__once_t *)once_control)->once_flag = PTHREAD_ONCE_DONE;
	}
	_mutex_unlock(&((__once_t *)once_control)->mlock);

	return (0);
}

/*
 * pthread_join: joins the terminated thread.
 * differs from Solaris thr_join(); it does return departed thread's
 * id and hence does not have "departed" argument.
 */
int
_pthread_join(pthread_t thread, void **status)
{
	if (thread == (pthread_t)0)
		return (ESRCH);
	else
		return (_thrp_join((thread_t)thread, NULL, status));
}

/*
 * pthread_equal: equates two thread ids.
 */
int
_pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

/*
 * pthread_getspecific: get the tsd vale for a specific key value.
 * It is same as thr_getspecific() except that tsd value is returned
 * by POSIX call whereas thr_* call pass it in an argument.
 */
void *
_pthread_getspecific(pthread_key_t key)
{
	void	*value;

	if (_thr_getspecific((thread_key_t)key, &value) != 0)
		return (NULL);
	else
		return (value);
}

/*
 * pthread_getschedparam: gets the sched parameters in a struct.
 */
int
_pthread_getschedparam(pthread_t thread, int *policy,
					struct sched_param *param)
{
	uthread_t	*t;
	int		ix;

	if (param != NULL && policy != NULL) {
		_lock_bucket((ix = HASH_TID(thread)));
		if ((t = THREAD(thread)) == (uthread_t *)-1 ||
				t->t_flag & T_INTERNAL) {
			_unlock_bucket(ix);
			return (ESRCH);
		}
		if (t->t_mappedpri != 0)
			param->sched_priority = t->t_mappedpri;
		else
			param->sched_priority = t->t_pri;
		*policy = t->t_policy;
		_unlock_bucket(ix);
		return (0);
	} else {
		return (EINVAL);
	}

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
_thread_setschedparam_main(pthread_t thread, int policy,
    const struct sched_param *param, int inheritflag)
{
	uthread_t	*t;
	lwpid_t		lwpid;
	int		ret;
	int		ix;
	int		tuopts;
	int		opolicy;
	int		prio;
	int		mappedprio;
	int		mappedprioflag = 0;
	int		*mappedprip;
	int		inherit = 0;

	if (param != NULL) {
		prio =  param->sched_priority;
		_lock_bucket((ix = HASH_TID(thread)));
		if ((t = THREAD(thread)) == (uthread_t *)-1 ||
				t->t_flag & T_INTERNAL) {
			_unlock_bucket(ix);
			return (ESRCH);
		}
		lwpid = t->t_lwpid;
		tuopts = t->t_usropts;
		opolicy = t->t_policy;
		ASSERT(inheritflag == PRIO_SET || opolicy == policy);
		if (inheritflag == PRIO_DISINHERIT) {
			t->t_emappedpri = 0;
			t->t_epri = 0;
			prio = t->t_pri; /* ignore prio in sched_param */
		} else if (inheritflag == PRIO_INHERIT) {
			inherit = 1;
		}
		if (policy == SCHED_OTHER || ((!(tuopts & THR_BOUND) &&
		    !(ISTEMPRTBOUND(t))) &&
		    (policy == SCHED_FIFO || policy == SCHED_RR))) {
			/*
			 * Set unbound thread's policy to one of FIFO/RR/OTHER
			 * OR set bound thread's policy to OTHER
			 */
			if (CHECK_PRIO(prio)) {
				if (!_validate_rt_prio(policy, prio)) {
					mappedprio = prio;
					prio = _map_rtpri_to_gp(prio);
					ASSERT(!CHECK_PRIO(prio));
					mappedprioflag = 1;
				} else {
					_unlock_bucket(ix);
					return (EINVAL);
				}
			}
			/*
			 * Bound thread changing from FIFO/RR to OTHER
			 */
			if (((opolicy == SCHED_FIFO) ||
			    (opolicy == SCHED_RR)) &&
			    ((tuopts & THR_BOUND) || ISTEMPRTBOUND(t))) {
				ret = _thrp_setlwpprio(lwpid, policy, prio);
				if (ret) {
					_unlock_bucket(ix);
					return (ret);
				}
			}

			if (inheritflag != PRIO_DISINHERIT) {
				if (inherit)
					mappedprip = &t->t_emappedpri;
				else
					mappedprip = &t->t_mappedpri;
				if (mappedprioflag) {
					*mappedprip = mappedprio;
				} else {
					if (*mappedprip != 0)
						*mappedprip = 0;
				}
			}
			/* will release the bucket lock */
			ret = _thrp_setprio(t, prio, policy, inherit, ix);
			if (!ret) {
				if (opolicy != SCHED_RR && policy == SCHED_RR) {
					/* creating a new SCHED_RR thread */
					_thr_setconcurrency(
						_thr_getconcurrency() + 1);
				}
			}
			return (ret);
		} else if (policy == SCHED_FIFO || policy == SCHED_RR) {
			ASSERT((tuopts & THR_BOUND) || ISTEMPRTBOUND(t));
			if (_validate_rt_prio(policy, prio)) {
				_unlock_bucket(ix);
				return (EINVAL);
			}
			ret = _thrp_setlwpprio(lwpid, policy, prio);
			if (ret == EPERM) {
				_unlock_bucket(ix);
				return (ret);
			} else if (ret > 0) {
				_panic("_thrp_setlwpprio failed");
			}
			if ((t = THREAD(thread)) == (uthread_t *)-1 ||
					t->t_flag & T_INTERNAL) {
				_unlock_bucket(ix);
				return (ESRCH);
			}
			t->t_policy = policy;
			if (inherit)
				t->t_epri = prio;
			else
				t->t_pri = prio;
			_unlock_bucket(ix);
			return (0);
		}
		_unlock_bucket(ix);
	}
	return (EINVAL);
}

/*
 * pthread_setschedparam: sets the sched parameters for a thread.
 */
int
_pthread_setschedparam(pthread_t thread, int policy,
					const struct sched_param *param)
{
	return (_thread_setschedparam_main(thread, policy, param, PRIO_SET));
}

/*
 * Routine to maintain the atfork queues.  This is called by both
 * _pthread_atfork() & _lpthread_atfork().
 */
static int
_atfork_append(void (*prepare) (void), void (*parent) (void),
		void (*child) (void), __atfork_t ** atfork_q,
		mutex_t *mlockp)
{
	__atfork_t *atfp;

	if ((atfp = (__atfork_t *)malloc(sizeof (__atfork_t))) == NULL) {
		return (ENOMEM);
	}
	atfp->prepare = prepare;
	atfp->child = child;
	atfp->parent = parent;

	_lmutex_lock(mlockp);
	if (*atfork_q == NULL) {
		*atfork_q = atfp;
		atfp->fwd = atfp->bckwd = atfp;
	} else {
		(*atfork_q)->bckwd->fwd = atfp;
		atfp->fwd = *atfork_q;
		atfp->bckwd = (*atfork_q)->bckwd;
		(*atfork_q)->bckwd = atfp;
	}
	_lmutex_unlock(mlockp);

	return (0);
}

/*
 * pthread_atfork: installs handlers to be called during fork().
 * It allocates memory off the heap and put the handler addresses
 * in circular Q. Once installed atfork handlers can not be removed.
 * There is no POSIX API which allows to "delete" atfork handlers.
 */
int
_pthread_atfork(void (*prepare) (void), void (*parent) (void),
    void (*child) (void))
{
	return (_atfork_append(prepare, parent, child,
	    &_atforklist, &_atforklock));
}

int
_lpthread_atfork(void (*prepare) (void), void (*parent) (void),
    void (*child) (void))
{
	return (_atfork_append(prepare, parent, child,
	    &_latforklist, &_latforklock));
}

static void
_run_prefork(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp, *last;

	_mutex_lock(mlockp);
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

static void
_run_postfork_parent(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp;

	/* _atforklock mutex is locked by _prefork_handler */
	ASSERT(MUTEX_HELD(&_atforklock));

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
	_mutex_unlock(mlockp);
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

static void
_run_postfork_child(__atfork_t *atfork_q, mutex_t *mlockp)
{
	__atfork_t *atfp;

	/* _atforklock mutex is locked by _prefork_handler */
	ASSERT(MUTEX_HELD(&_atforklock));

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
	_mutex_unlock(mlockp);
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

/*
 * UNIX 98
 */

int
_pthread_getconcurrency()
{
	if (_first_setconcurrency)
		return (_uconcurrency);
	else
		return (0);
}

int
_pthread_setconcurrency(int new_level)
{
	int rc;

	rc = _thr_setconcurrency(new_level);

	if (!_first_setconcurrency && rc == 0)
		_first_setconcurrency = 1;
	return (rc);
}
