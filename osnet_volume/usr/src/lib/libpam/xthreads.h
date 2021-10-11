/*
 * $XConsortium: Xthreads.h /main/cde_main/1 1995/08/13 17:10:50 rws $
 *
 *
Copyright(c) 1993  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 * *
 */

#if defined(OPT_INCLUDE_XTHREADS_H)

#ifndef _XTHREADS_H_
#define	_XTHREADS_H_

#pragma	ident	"@(#)xthreads.h	1.4	98/06/14 SMI"

/*
 * Redefine these to XtMalloc/XtFree or whatever you want before including
 * this header file.
 */
#ifndef xmalloc
#define	xmalloc malloc
#endif
#ifndef	xfree
#define	xfree free
#endif

#ifdef CTHREADS
#include <cthreads.h>
typedef cthread_t xthread_t;
typedef struct condition xcondition_rec;
typedef struct mutex xmutex_rec;
#define	xthread_init() cthread_init()
#define	xthread_self cthread_self
#define	xthread_fork(func, closure) cthread_fork(func, closure)
#define	xthread_yield() cthread_yield()
#define	xthread_exit(v) cthread_exit(v)
#define	xthread_set_name(t, str) cthread_set_name(t, str)
#define	xmutex_init(m) mutex_init(m)
#define	xmutex_clear(m) mutex_clear(m)
#define	xmutex_lock(m) mutex_lock(m)
#define	xmutex_unlock(m) mutex_unlock(m)
#define	xmutex_set_name(m, str) mutex_set_name(m, str)
#define	xcondition_init(cv) condition_init(cv)
#define	xcondition_clear(cv) condition_clear(cv)
#define	xcondition_wait(cv, m) condition_wait(cv, m)
#define	xcondition_signal(cv) condition_signal(cv)
#define	xcondition_broadcast(cv) condition_broadcast(cv)
#define	xcondition_set_name(cv, str) condition_set_name(cv, str)
#else
#ifdef SVR4
#include <thread.h>
#include <synch.h>
typedef thread_t xthread_t;
typedef thread_key_t xthread_key_t;
typedef cond_t xcondition_rec;
typedef mutex_t xmutex_rec;
#define	xthread_self thr_self
#define	xthread_fork(func, closure) \
	thr_create(NULL, 0, func, closure, THR_NEW_LWP|THR_DETACHED, NULL)
#define	xthread_yield() thr_yield()
#define	xthread_exit(v) thr_exit(v)
#define	xthread_key_create(kp, d) thr_keycreate(kp, d)
#ifdef sun
#define	xthread_key_delete(k) 0
#else
#define	xthread_key_delete(k) thr_keydelete(k)
#endif
#define	xthread_set_specific(k, v) thr_setspecific(k, v)
#define	xthread_get_specific(k, vp) thr_getspecific(k, vp)
#define	xmutex_init(m) mutex_init(m, USYNC_THREAD, 0)
#define	xmutex_clear(m) mutex_destroy(m)
#define	xmutex_lock(m) mutex_lock(m)
#define	xmutex_unlock(m) mutex_unlock(m)
#define	xcondition_init(cv) cond_init(cv, USYNC_THREAD, 0)
#define	xcondition_clear(cv) cond_destroy(cv)
#define	xcondition_wait(cv, m) cond_wait(cv, m)
#define	xcondition_signal(cv) cond_signal(cv)
#define	xcondition_broadcast(cv) cond_broadcast(cv)
#else
#ifdef WIN32
#define	BOOL wBOOL
#ifdef Status
#undef Status
#define	Status wStatus
#endif
#include <windows.h>
#ifdef Status
#undef Status
#define	Status int
#endif
#undef BOOL
typedef DWORD xthread_t;
typedef DWORD xthread_key_t;
struct _xthread_waiter {
    HANDLE sem;
    struct _xthread_waiter *next;
};
typedef struct {
    CRITICAL_SECTION cs;
    struct _xthread_waiter *waiters;
} xcondition_rec;
typedef CRITICAL_SECTION xmutex_rec;
#define	xthread_init() _Xthread_init()
#define	xthread_self GetCurrentThreadId
#define	xthread_fork(func, closure) { \
    DWORD _tmptid; \
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)closure, 0, \
		&_tmptid); \
}
#define	xthread_yield() Sleep(0)
#define	xthread_exit(v) ExitThread((DWORD)(v))
#define	xthread_key_create(kp, d) *(kp) = TlsAlloc()
#define	xthread_key_delete(k) TlsFree(k)
#define	xthread_set_specific(k, v) TlsSetValue(k, v)
#define	xthread_get_specific(k, vp) *(vp) = TlsGetValue(k)
#define	xmutex_init(m) InitializeCriticalSection(m)
#define	xmutex_clear(m) DeleteCriticalSection(m)
#define	_XMUTEX_NESTS
#define	xmutex_lock(m) EnterCriticalSection(m)
#define	xmutex_unlock(m) LeaveCriticalSection(m)
#define	xcondition_init(cv) { \
	InitializeCriticalSection(&(cv)->cs); \
	(cv)->waiters = NULL; \
}
#define	xcondition_clear(cv) DeleteCriticalSection(&(cv)->cs)
extern struct _xthread_waiter *_Xthread_waiter();
#define	xcondition_wait(cv, m) { \
	struct _xthread_waiter *_tmpthr = _Xthread_waiter(); \
	EnterCriticalSection(&(cv)->cs); \
	_tmpthr->next = (cv)->waiters; \
	(cv)->waiters = _tmpthr; \
	LeaveCriticalSection(&(cv)->cs); \
	LeaveCriticalSection(m); \
	WaitForSingleObject(_tmpthr->sem, INFINITE); \
	EnterCriticalSection(m); \
}
#define	xcondition_signal(cv) { \
	EnterCriticalSection(&(cv)->cs); \
	if ((cv)->waiters) { \
		ReleaseSemaphore((cv)->waiters->sem, 1, NULL); \
		(cv)->waiters = (cv)->waiters->next; \
	} \
	LeaveCriticalSection(&(cv)->cs); \
}
#define	xcondition_broadcast(cv) { \
	struct _xthread_waiter *_tmpthr; \
	EnterCriticalSection(&(cv)->cs); \
	for (_tmpthr = (cv)->waiters; _tmpthr; _tmpthr = _tmpthr->next) \
		ReleaseSemaphore(_tmpthr->sem, 1, NULL); \
	(cv)->waiters = NULL; \
	LeaveCriticalSection(&(cv)->cs); \
}
#else
#include <pthread.h>
#ifdef AIXV3
#ifndef _AIX32_THREADS
#define	POSIX_DRAFT7_THREADS
#endif
#endif
/*
 * The above would probably be better handled with an imake config
 * variable, but since AIX 4.X seems to be the only platform implementing
 * the draft 7 API, this will suffice for now. An imake config variable
 * would also require changes to the Xlib and Xt Imakefiles to compile
 * Xlib's locking.c and  Xt's Threads.c, so this has less impact.
 */
typedef pthread_t xthread_t;
typedef pthread_key_t xthread_key_t;
typedef pthread_cond_t xcondition_rec;
typedef pthread_mutex_t xmutex_rec;
#define	xthread_self pthread_self
#define	xthread_yield() pthread_yield()
#define	xthread_exit(v) pthread_exit(v)
#define	xthread_set_specific(k, v) pthread_setspecific(k, v)
#define	xmutex_clear(m) pthread_mutex_destroy(m)
#define	xmutex_lock(m) pthread_mutex_lock(m)
#define	xmutex_unlock(m) pthread_mutex_unlock(m)
#ifndef POSIX_DRAFT7_THREADS
#define	xthread_key_create(kp, d) pthread_keycreate(kp, d)
#define	xthread_key_delete(k) 0
#define	xthread_get_specific(k, vp) pthread_getspecific(k, vp)
#define	xthread_fork(func, closure) { pthread_t _tmpxthr; \
	pthread_create(&_tmpxthr, pthread_attr_default, func, closure); }
#define	xmutex_init(m) pthread_mutex_init(m, pthread_mutexattr_default)
#define	xcondition_init(c) pthread_cond_init(c, pthread_condattr_default)
#else /* POSIX_DRAFT7_THREADS */
#define	xthread_key_create(kp, d) pthread_key_create(kp, d)
#define	xthread_key_delete(k) pthread_key_delete(k)
#define	xthread_get_specific(k, vp) *(vp) = pthread_getspecific(k)
#define	xthread_fork(func, closure) { pthread_t _tmpxthr; \
	pthread_create(&_tmpxthr, &pthread_attr_default, func, closure); }
#define	xmutex_init(m) \
	pthread_mutex_init(m, &pthread_mutexattr_default)
#define	xcondition_init(c) \
	pthread_cond_init(c, &pthread_condattr_default)
#endif /* POSIX_DRAFT7_THREADS */
#define	xcondition_clear(c) pthread_cond_destroy(c)
#define	xcondition_wait(c, m) pthread_cond_wait(c, m)
#define	xcondition_signal(c) pthread_cond_signal(c)
#define	xcondition_broadcast(c) pthread_cond_broadcast(c)
#ifdef _DECTHREADS_
static xthread_t _X_no_thread_id;
#define	xthread_have_id(id) !pthread_equal(id, _X_no_thread_id)
#define	xthread_clear_id(id) id = _X_no_thread_id
#define	xthread_equal(id1, id2) pthread_equal(id1, id2)
#endif /* _DECTHREADS_ */
#if _CMA_VENDOR_ == _CMA__IBM
#ifdef DEBUG			/* too much of a hack to enable normally */
/* see also cma__obj_set_name() */
#define	xmutex_set_name(m, str) ((char **)(m)->field1)[5] = (str)
#define	xcondition_set_name(cv, str) ((char **)(cv)->field1)[5] = (str)
#endif /* DEBUG */
#endif /* _CMA_VENDOR_ == _CMA__IBM */
#endif /* WIN32 */
#endif /* SVR4 */
#endif /* CTHREADS */
typedef xcondition_rec *xcondition_t;
typedef xmutex_rec *xmutex_t;
#ifndef xcondition_malloc
#define	xcondition_malloc() (xcondition_t)xmalloc(sizeof (xcondition_rec))
#endif
#ifndef xcondition_free
#define	xcondition_free(c) xfree((char *)c)
#endif
#ifndef xmutex_malloc
#define	xmutex_malloc() (xmutex_t)xmalloc(sizeof (xmutex_rec))
#endif
#ifndef xmutex_free
#define	xmutex_free(m) xfree((char *)m)
#endif
#ifndef xthread_have_id
#define	xthread_have_id(id) id
#endif
#ifndef xthread_clear_id
#define	xthread_clear_id(id) id = 0
#endif
#ifndef xthread_equal
#define	xthread_equal(id1, id2) ((id1) == (id2))
#endif
/* aids understood by some debuggers */
#ifndef xthread_set_name
#define	xthread_set_name(t, str)
#endif
#ifndef xmutex_set_name
#define	xmutex_set_name(m, str)
#endif
#ifndef xcondition_set_name
#define	xcondition_set_name(cv, str)
#endif

#endif /* _XTHREADS_H_ */
#endif /* OPT_INCLUDE_XTHREADS_H */
