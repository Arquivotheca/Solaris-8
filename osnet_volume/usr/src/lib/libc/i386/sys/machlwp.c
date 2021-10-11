/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machlwp.c	1.15	99/09/14 SMI"

#pragma weak _lwp_makecontext = __lwp_makecontext

#include "synonyms.h"
#include <memory.h>
#include <fcntl.h>
#include <ucontext.h>
#include <synch.h>
#include <sys/lwp.h>
#include <sys/stack.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/param.h>

/*
 * We use these versions of _lwp_mutex_lock() and _lwp_mutex_unlock()
 * rather than the exported versions in order to avoid invoking
 * the dynamic linker at an inopportune time for libthread.
 * In particular, we cannot afford to invoke the dynamic linker
 * when we allocate/deallocate the selector for %gs.
 */
extern	int	_private_lwp_mutex_lock(lwp_mutex_t *);
extern	int	_private_lwp_mutex_unlock(lwp_mutex_t *);

extern int __setupgs(void *);
static void **_alloc_gs(void);

/*
 * It is assumed that the ucontext_t structure has already been filled
 * with valid context information.  Here, we update the structure
 * so that when it is passed to _lwp_create() the newly-created
 * lwp will begin execution in the specified function with the
 * specified stack, properly initialized.
 *
 * However, the ucontext_t structure may contain uninitialized data.
 * We must be sure that this does not cause _lwp_create() to malfunction.
 * _lwp_create() only uses the signal mask and the general registers.
 */
void
_lwp_makecontext(
	ucontext_t *ucp,
	void (*func)(),
	void *arg,
	void *private,
	caddr_t stk,
	size_t stksize)
{
	ucontext_t uc;
	uint32_t *stack;

	(void) getcontext(&uc);	/* needed to load segment registers */
	ucp->uc_mcontext.gregs[FS] = uc.uc_mcontext.gregs[FS];
	ucp->uc_mcontext.gregs[ES] = uc.uc_mcontext.gregs[ES];
	ucp->uc_mcontext.gregs[DS] = uc.uc_mcontext.gregs[DS];
	ucp->uc_mcontext.gregs[CS] = uc.uc_mcontext.gregs[CS];
	ucp->uc_mcontext.gregs[SS] = uc.uc_mcontext.gregs[SS];
	if (private)
		ucp->uc_mcontext.gregs[GS] = (greg_t)__setupgs(private);
	else
		ucp->uc_mcontext.gregs[GS] = 0;

	/* top-of-stack must be rounded down to STACK_ALIGN */
	stack = (uint32_t *)(((uintptr_t)stk + stksize) & ~(STACK_ALIGN-1));

	/* set up top stack frame */
	*--stack = 0;
	*--stack = 0;
	*--stack = (uint32_t)arg;
	*--stack = (uint32_t)_lwp_exit;	/* return here if function returns */

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[EIP] = (greg_t)func;
	ucp->uc_mcontext.gregs[UESP] = (greg_t)stack;
	ucp->uc_mcontext.gregs[EBP] = (greg_t)(stack+2);
}

static void **freegsmem = NULL;
static lwp_mutex_t freegslock;

/*
 * Private interface for libthread to acquire/release freegslock.
 * This is needed in libthread's fork1(), where it grabs all internal locks.
 */
void
__freegs_lock()
{
	(void) _lwp_mutex_lock(&freegslock);
}

void
__freegs_unlock()
{
	(void) _lwp_mutex_unlock(&freegslock);
}

int
__setupgs(void *private)
{
	extern int __alloc_selector(void *, int);
	int sel;
	void **priptr;

	(void) _private_lwp_mutex_lock(&freegslock);
	if ((priptr = freegsmem) !=  NULL)
		freegsmem = *freegsmem;
	else
		priptr = _alloc_gs();
	(void) _private_lwp_mutex_unlock(&freegslock);
	if (priptr) {
		sel = __alloc_selector(priptr, 2 * sizeof (void *));
		if (sel != -1) {
			priptr[0] = private;
			priptr[1] = priptr;
			return (sel);
		}
	}
	return (0);
}

void
__freegs(int sel)
{
	extern void **_getpriptr(int);
	extern void __free_selector(int);
	void **priptr;

	(void) _private_lwp_mutex_lock(&freegslock);
	priptr = _getpriptr(sel);
	priptr[0] = freegsmem;
	priptr[1] = NULL;
	freegsmem = priptr;
	(void) _private_lwp_mutex_unlock(&freegslock);
	__free_selector(sel);
}

void
_lwp_freecontext(ucontext_t *ucp)
{
	if (ucp->uc_mcontext.gregs[GS] != 0) {
		__freegs(ucp->uc_mcontext.gregs[GS]);
		ucp->uc_mcontext.gregs[GS] = 0;
	}
}

static void **
_alloc_gs()
{
	static int pagesize = 0;
	void *cp;
	int i;

	if (pagesize == 0)
		pagesize = PAGESIZE;
	cp = mmap(0, pagesize, PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_PRIVATE|MAP_ANON, -1, 0);
	if (cp == MAP_FAILED)
		return (NULL);
	for (i = pagesize / (2 * sizeof (void *)); i > 0; i--) {
		*(void **)cp = freegsmem;
		freegsmem = cp;
		cp = (caddr_t)cp + 2 * sizeof (void *);
	}
	cp = freegsmem;
	freegsmem = *freegsmem;
	return (cp);
}
