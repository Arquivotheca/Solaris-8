/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)schedctl.c	1.28	98/10/25 SMI"

#include "libthread.h"
#include <unistd.h>
#include <door.h>
#include <errno.h>

u_int	_sc_dontfork = 0;
cond_t	_sc_dontfork_cv = DEFAULTCV;
mutex_t	_sc_lock = DEFAULTMUTEX;

static door_info_t sc_info;
static u_int	sc_flags = 0;

/*
 * sc_list is a list of lwps in the process, linked through threads running
 * on these LWPs. We keep track of this for two reasons:
 * - so we can clear the t_lwpdata pointers in the child of a fork.
 * - to serve as a list of lwps to be traversed by the "stop-all-mutators"
 *   interfaces required by the JVM.
 * The list is protected by _schedlock.
 */
uthread_t *_sc_list = NULL;

extern void (*__door_server_func)(door_info_t *);
extern void (*__thr_door_server_func)(door_info_t *);
static void	_sc_door_create_server(door_info_t *);
static void	_sc_door_func(void);
static void	_schedctl_start(void *, char *, size_t, door_desc_t *, uint_t);
extern int	_door_create(void (*)(void *, char *, size_t, door_desc_t *,
		    uint_t), void *, u_int);
extern int	_door_info(int, door_info_t *);
extern int	_door_bind(int);
extern int	_door_return(char *, size_t, door_desc_t *, size_t, caddr_t);
extern int	_door_unbind(void);

extern pid_t	__door_create_pid;


void
_sc_init(void)
{
	int did;

	/*
	 * One-time-only setup for scheduler activations.  This is called
	 * when t0 is initialized and from the child of a fork1.
	 */
	sc_flags = SC_STATE | SC_BLOCK;
	__thr_door_server_func = _sc_door_create_server;
	did = _door_create(_schedctl_start, NULL, DOOR_PRIVATE);
	if (did >= 0) {
		__door_create_pid = _getpid();
		_sc_setup(did, 1);
		if (_door_info(did, &sc_info) == 0) {
			_new_lwp(NULL, (void (*)())_sc_door_func, 1);
		}
		(void) _close(did);
	} else
		_sc_setup(-1, 1);
}

/*
 * add thread to the _sc_list. the _sc_list should always reflect
 * the actual number of visible LWPs in the process.
 */
void
_sc_add(uthread_t *t)
{
	_sched_lock();
	if (_sc_list == NULL)
		_sc_list = t->t_scforw = t->t_scback = t;
	else {
		t->t_scforw = _sc_list;
		t->t_scback = _sc_list->t_scback;
		_sc_list = t;
		t->t_scback->t_scforw = t;
		t->t_scforw->t_scback = t;
	}
	_sched_unlock();
}

void
_sc_setup(int did, int sc_add_flag)
{
	sc_shared_t	*addr;
	uint_t		flags;
	uthread_t	*t = curthread;
	door_info_t	info;

	if (ISBOUND(t) && !IDLETHREAD(t) && !(t->t_flag & T_INTERNAL))
		flags = SC_STATE;
	else
		flags = sc_flags;

	/*
	 * Need protect against fork().  We don't want to get an address
	 * back from _lwp_schedctl, then fork, and have a bad address in
	 * the child.  We use a counter and a cv so we don't have to hold
	 * _schedlock across the system call.
	 */
	_lmutex_lock(&_sc_lock);
	++_sc_dontfork;
	_lmutex_unlock(&_sc_lock);

	if (_lwp_schedctl(flags, did, &addr) != 0)
		t->t_lwpdata = NULL;		/* error */
	else
		t->t_lwpdata = addr;

	if (sc_add_flag)
		_sc_add(t);
	_lmutex_lock(&_sc_lock);
	if (--_sc_dontfork == 0)
		_cond_signal(&_sc_dontfork_cv);	/* wake up waiting forkers */
	_lmutex_unlock(&_sc_lock);
}

/*
 * Substitute new thread for current one in the list of lwps.
 */
void
_sc_switch(uthread_t *next)
{
	uthread_t	*t = curthread;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(t != next);

	/*
	 * XXX:
	 * The t_scforw pointer can be NULL, if this process is the child of
	 * a multi-threaded process which called fork(), that forks all LWPs
	 * (i.e. a forkall()) and the LWP existed at the time of the fork()
	 * and was not the LWP which called fork().
	 * See _sc_cleanup(). This is a bug. We need to fix it in _sc_cleanup()
	 * by leaving all LWPs on the _sc_list, for a forkall() call. This
	 * bug is not being fixed at this stage because of a risk/benefit
	 * tradeoff.
	 */
	if (t->t_scforw == NULL) {
		ASSERT(t->t_scback == NULL);
		return;
	}
	ASSERT(t->t_scback != NULL);
	next->t_lwpdata = t->t_lwpdata;
	t->t_lwpdata = NULL;
	if (t->t_scforw != t) {
		ASSERT(t->t_scback != t);
		next->t_scforw = t->t_scforw;
		next->t_scforw->t_scback = next;
		next->t_scback = t->t_scback;
		next->t_scback->t_scforw = next;
	} else {
		ASSERT(t->t_scback == t);
		next->t_scforw = next;
		next->t_scback = next;
	}
	if (_sc_list == t)
		_sc_list = next;
	t->t_scforw = NULL;
	t->t_scback = NULL;
}

/*
 * Remove thread from list.
 */
void
_sc_exit(void)
{
	uthread_t *t = curthread;

	ASSERT(MUTEX_HELD(&_schedlock));
	if (!ISBOUND(t))
		return;
	t->t_lwpdata = NULL;
	if (t->t_scforw == t)
		_sc_list = NULL;
	else {
		t->t_scforw->t_scback = t->t_scback;
		t->t_scback->t_scforw = t->t_scforw;
		if (_sc_list == t)
			_sc_list = t->t_scforw;
	}
	t->t_scforw = NULL;
	t->t_scback = NULL;
}

/*
 * Cleanup on fork or fork1.
 */
void
_sc_cleanup(int frk1)
{
	uthread_t *t, *first, *next;

	ASSERT(MUTEX_HELD(&_schedlock));

	/*
	 * Reset pointers to shared data.  Thread list is protected
	 * by _schedlock.
	 * XXX: Eventually, if this is a forkall(), we need to leave the
	 * LWPs on the list, so that the child continues to work with respect
	 * to the "stop-the-world" interfaces for the JVM.
	 * See _sc_switch() also. This bug is not being fixed at this stage
	 * because of a risk/benefit tradeoff.
	 */
	if (_sc_list) {
		first = t = _sc_list;
		do {
			next = t->t_scforw;

			t->t_lwpdata = NULL;
			t->t_scforw = NULL;
			t->t_scback = NULL;

			/* only free idle threads on the _sc_list. */
			if (frk1 && IDLETHREAD(t) && (t->t_flag & T_ALLOCSTK)) {
				_free_stack(t->t_stk - t->t_stksize,
				    t->t_stksize, 0,  t->t_stkguardsize);
			}
		} while ((t = next) != first);
		_sc_list = NULL;
	}
}

static void
_sc_door_func(void)
{
	door_info_t info;
	int did;

	did = _lwp_schedctl(SC_DOOR, 0, NULL);
	if (did >= 0 && _door_bind(did) == 0) {
		(void) _close(did);
		(void) _door_return(NULL, 0, NULL, 0, NULL);
	} else {
		if (did >= 0)
			(void) _close(did);
		_age();
	}
}

static void
_sc_door_create_server(door_info_t *dip)
{
	if (dip == NULL || dip->di_uniquifier != sc_info.di_uniquifier) {
		if (__door_server_func)
			(*__door_server_func)(dip);
		else
			_thr_exit(0);
	} else {
		_new_lwp(NULL, (void (*)())_sc_door_func, 1);
	}
}

int
__thr_door_unbind(void)
{
	door_info_t info;

	if (_door_info(DOOR_QUERY, &info) != 0 ||
	    info.di_uniquifier != sc_info.di_uniquifier)
		return (_door_unbind());
	else {
		curthread->t_errno = EBADF;
		return (-1);
	}
}

/* ARGSUSED */
static void
_schedctl_start(void *cookie, char *argp, size_t arg_size,
    door_desc_t *dp, uint_t n_desc)
{
	_sched_lock_nosig();
	if (!_nrunnable || (_nrunnable - _nidlecnt) <= _naging)
		if (_sigwaitingset)
			_sigwaiting_disabled();
	_sched_unlock_nosig();
	_age();
}
