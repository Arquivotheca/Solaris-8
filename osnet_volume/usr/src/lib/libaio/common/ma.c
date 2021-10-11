/*
 *	Copyright (c) 1992, Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)ma.c	1.9	98/12/22	SMI"


#include	<sys/mman.h>
#include	<sys/param.h>
#include	<sys/lwp.h>
#include	<synch.h>
#include	<fcntl.h>
#include	<libaio.h>


extern int _open(const char *, int, ...);
int	_aio_alloc_stack(int, caddr_t *);
void	_aio_free_stack_unlocked(int, caddr_t);
static	int	_aio_alloc_chunk(caddr_t, int, caddr_t *);

static int DEFAULTSTACKINCR = 0;
static int DEFAULTSTACK = 0;
static int MAXSTACKS = 0;

static struct stkcache {
	int size;
	char *next;
} _defaultstkcache;
static lwp_mutex_t _stkcachelock;

void
__init_stacks(int stksz, int ncached_stks)
{
	int stkincr;

	DEFAULTSTACK = stksz;
	MAXSTACKS = ncached_stks;
	DEFAULTSTACKINCR = ((stkincr = ncached_stks/16)) ? stkincr : 2;
	_aio_alloc_stack(stksz, NULL);
}

/*
 * allocate a stack with redzone. stacks of default size are
 * cached and allocated in increments greater than 1.
 */
int
_aio_alloc_stack(int size, caddr_t *sp)
{
	int i;
	caddr_t addr;

	ASSERT(size == DEFAULTSTACK);
	_lwp_mutex_lock(&_stkcachelock);
	if (_defaultstkcache.next == NULL) {
		/* add redzone */
		/* LINTED */
		size += (int)PAGESIZE;
		if (!_aio_alloc_chunk(0, DEFAULTSTACKINCR*size, &addr)) {
			_lwp_mutex_unlock(&_stkcachelock);
			return (0);
		}
		for (i = 0; i < DEFAULTSTACKINCR; i++) {
			/*
			 * invalidate the top stack page.
			 */
			if (mprotect(addr, (size_t)PAGESIZE, PROT_NONE)) {
				_lwp_mutex_unlock(&_stkcachelock);
				perror("aio_alloc_stack: mprotect 1");
				return (0);
			}
			_aio_free_stack_unlocked(DEFAULTSTACK, addr + PAGESIZE);
			addr += (DEFAULTSTACK + PAGESIZE);
		}
	}
	if (sp) {
		*sp = _defaultstkcache.next;
		_defaultstkcache.next = (caddr_t)(**((long **)sp));
		_defaultstkcache.size -= 1;
	}
	_lwp_mutex_unlock(&_stkcachelock);
	return (1);
}

/*
 * free up stack space. stacks of default size are cached until some
 * high water mark and then they are also freed.
 */
void
_aio_free_stack_unlocked(int size, caddr_t addr)
{
	if (size == DEFAULTSTACK) {
		if (_defaultstkcache.size < MAXSTACKS) {
			/* LINTED */
			*(long *)(addr) = (long)_defaultstkcache.next;
			_defaultstkcache.next = addr;
			_defaultstkcache.size += 1;
			return;
		}
	}
	/* include one page for redzone */
	if (munmap(addr - PAGESIZE, (size_t) (size + PAGESIZE))) {
		perror("aio_free_stack: munmap");
	}
}

void
_aio_free_stack(int size, caddr_t addr)
{
	if (size == DEFAULTSTACK) {
		_lwp_mutex_lock(&_stkcachelock);
		if (_defaultstkcache.size < MAXSTACKS) {
			/* LINTED */
			*(long *)(addr) = (long)_defaultstkcache.next;
			_defaultstkcache.next = addr;
			_defaultstkcache.size += 1;
			_lwp_mutex_unlock(&_stkcachelock);
			return;
		}
		_lwp_mutex_unlock(&_stkcachelock);
	}
	/* include one page for redzone */
	if (munmap(addr - PAGESIZE, (size_t) (size + PAGESIZE))) {
		perror("aio_free_stack: munmap");
	}
}

/*
 * allocate a chunk of /dev/zero memory.
 */
static int
_aio_alloc_chunk(caddr_t at, int size, caddr_t *cp)
{
	int devzero;

	if ((devzero = _open("/dev/zero", O_RDWR)) == -1) {
		perror("_open(/dev/zero)");
		_aiopanic("aio_alloc_chunk");
	}
	if ((*cp = mmap(at, size, PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_PRIVATE|MAP_NORESERVE, devzero, 0)) ==
			(caddr_t)-1) {
		_aiopanic("aio_alloc_chunk: no mem");
	}
	_close(devzero);
	return (1);
}
