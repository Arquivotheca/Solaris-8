/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ma.c	1.41	99/05/19 SMI"

#include "libthread.h"
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Global variables
 */
stkcache_t _defaultstkcache;
mutex_t _stkcachelock = DEFAULTMUTEX;
mutex_t _tsslock;

/*
 * Static variables
 */
static	caddr_t _tmpstkcache = 0;

/*
 * Static functions
 */
static	void _free_chunk(caddr_t, size_t);


/*
 * allocate a stack with redzone. stacks of default size are
 * cached and allocated in increments greater than 1.
 * guardsize is a multiple of pagesize.
 */
int
_alloc_stack(size_t size, caddr_t *sp, size_t guardsize)
{
	int i, j;
	int stackincr = DEFAULTSTACKINCR;

	if ((size == DEFAULTSTACK) && (guardsize == _lpagesize)) {
		if ((_defaultstkcache.next == NULL) && (_deathrow != NULL)) {
			if (_alloc_stack_fromreapq(sp)) {
				return (1);
			}
		}
		_lmutex_lock(&_stkcachelock);
		while (_defaultstkcache.next == NULL) {
			if (_defaultstkcache.busy) {
				_cond_wait(&_defaultstkcache.cv,
				    &_stkcachelock);
				continue;
			}
			_defaultstkcache.busy = 1;
			_lmutex_unlock(&_stkcachelock);
			ITRACE_0(UTR_FAC_TLIB_MISC, UTR_CACHE_MISS,
			    "thread stack cache miss");
			/* add redzone */
			size += _lpagesize;
retry_alloc_chunk:
			if (!_alloc_chunk(0, stackincr * size, sp)) {
				/*
				 * This does not try to reduce demand and retry
				 * but just gets out cleanly - so that threads
				 * waiting for the busy bit are woken up.
				 * Should be fixed later to squeeze more memory
				 * out of what is left. XXX
				 */
				if (stackincr != 1) {
					stackincr = 1;
					goto retry_alloc_chunk;
				}
				_lmutex_lock(&_stkcachelock);
				_defaultstkcache.busy = 0;
				_cond_broadcast(&_defaultstkcache.cv);
				_lmutex_unlock(&_stkcachelock);
				return (0);
			}
			for (i = 0; i < stackincr; i++) {
				/*
				 * invalidate the top stack page.
				 */
				if (_mprotect(*sp, _lpagesize, PROT_NONE)) {
					perror("alloc_stack: mprotect 1");
					for (; i < stackincr; i++) {
						/*
						 * from wherever mprotect
						 * failed, free the rest of the
						 * allocated chunk.
						 */
						if (_munmap(*sp,
						    DEFAULTSTACK+_lpagesize)) {
							perror("_munmap");
							_panic("_alloc_stack");
						}
						*sp += (DEFAULTSTACK +
						    _lpagesize);
					}
					_lmutex_lock(&_stkcachelock);
					_defaultstkcache.busy = 0;
					_cond_broadcast(&_defaultstkcache.cv);
					_lmutex_unlock(&_stkcachelock);
					return (0);
				}
				_free_stack(*sp + _lpagesize, DEFAULTSTACK,
						1, guardsize);
				*sp += (DEFAULTSTACK + _lpagesize);
			}
			_lmutex_lock(&_stkcachelock);
			_defaultstkcache.busy = 0;
			if (_defaultstkcache.next) {
				_cond_broadcast(&_defaultstkcache.cv);
				break;
			}
		}
#ifdef ITRACE
		else {
			ITRACE_0(UTR_FAC_TLIB_MISC, UTR_CACHE_HIT,
			    "thread stack cache hit");
		}
#endif
		ASSERT(_defaultstkcache.size > 0 &&
		    _defaultstkcache.next != NULL);
		*sp = _defaultstkcache.next;
		_defaultstkcache.next = (caddr_t)(**((long **)sp));
		_defaultstkcache.size -= 1;
		_lmutex_unlock(&_stkcachelock);
		return (1);
	} else {
		/* add redzone */

		size += guardsize;
		if (!_alloc_chunk(0, size, sp))
			return (0);
		/*
		 * invalidate the top stack page.
		 */
		if (guardsize && _mprotect(*sp, guardsize, PROT_NONE)) {
			perror("alloc_stack: mprotect 2");
			return (0);
		}
		*sp += guardsize;
		return (1);
	}
}

/*
 * free up stack space. stacks of default size are cached until some
 * high water mark and then they are also freed.
 * guardsize is a multiple of pagesize.
 */
void
_free_stack(caddr_t addr, size_t size, int cache, size_t guardsize)
{
	if (cache && (size == DEFAULTSTACK) && (guardsize == _lpagesize)) {
		_lmutex_lock(&_stkcachelock);
		if (_defaultstkcache.size < MAXSTACKS) {
			*(long *)(addr) = (long)_defaultstkcache.next;
			_defaultstkcache.next = addr;
			_defaultstkcache.size += 1;
			_lmutex_unlock(&_stkcachelock);
			return;
		}
		_lmutex_unlock(&_stkcachelock);
	}
	/* include redzone */
	if (_munmap(addr - guardsize, size + guardsize)) {
		perror("free_stack: _munmap");
		_panic("free_stack");
	}
}


#define	TMPSTKSIZE	256

int
_alloc_tmpstack(caddr_t *stk)
{
	caddr_t addr;
	caddr_t *p, np;
	int i;

	_lmutex_lock(&_tsslock);
	if (!_tmpstkcache) {
		if (!_alloc_chunk(0, _lpagesize, &addr)) {
			_lmutex_unlock(&_tsslock);
			return (0);
		}
		p = &_tmpstkcache;
		np = (caddr_t)addr;
		for (i = 0; i < (_lpagesize/TMPSTKSIZE) - 1; i++) {
			np += TMPSTKSIZE;
			*p = np;
			p = &np;
		}
		*p = NULL;
	}
	*stk = _tmpstkcache;
	_tmpstkcache = *(caddr_t *)_tmpstkcache;
	_lmutex_unlock(&_tsslock);
	return (1);
}

void
_free_tmpstack(caddr_t stk)
{
	_lmutex_lock(&_tsslock);
	*(caddr_t *)stk = _tmpstkcache;
	_tmpstkcache = stk;
	_lmutex_unlock(&_tsslock);
}

/*
 * allocate a chunk of anonymous memory.
 * returns 1 on success, otherwise zero.
 */
int
_alloc_chunk(caddr_t at, size_t size, caddr_t *cp)
{
	extern caddr_t _mmap(caddr_t, size_t, int, int, int, off_t);

	*cp = _mmap(at, size, sysconf(_SC_STACK_PROT),
	    MAP_PRIVATE | MAP_NORESERVE | MAP_ANON, -1, 0);
	return (*cp == (caddr_t)-1 ? 0 : 1);
}

/*
 * free a chunk of allocated /dev/zero memory.
 */
static void
_free_chunk(caddr_t addr, size_t size)
{
	if (_munmap(addr, size)) {
		perror("_munmap");
		_panic("_munmap");
	}
}
