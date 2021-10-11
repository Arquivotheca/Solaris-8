/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_context.c	1.1	99/08/11 SMI"

/*
 * Debugger co-routine context support:  In order to implement the context-
 * switching necessary for MDB pipes, we need the ability to establish a
 * co-routine context that has a separate stack.  We use this stack to execute
 * the MDB parser, and then switch back and forth between this code and the
 * dcmd which is producing output to be consumed.  We implement a context by
 * mapping a few pages of anonymous memory, and then using setcontext(2) to
 * switch to this stack and begin execution of a new function.
 */

#include <mdb/mdb_context.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_err.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/lwp.h>

#include <rtc_api.h>
#include <ucontext.h>
#include <unistd.h>
#include <setjmp.h>
#include <fcntl.h>
#include <errno.h>

struct mdb_context {
	int (*ctx_func)(void);		/* pointer to start function */
	int ctx_status;			/* return status of ctx_func */
	int ctx_resumes;		/* count of context resume calls */
	size_t ctx_stacksize;		/* size of stack in bytes */
	void *ctx_stack;		/* stack base address */
	ucontext_t ctx_uc;		/* user context structure */
	jmp_buf ctx_pcb;		/* control block for resume */
};

static void
context_init(mdb_context_t *volatile c)
{
	c->ctx_status = c->ctx_func();
	ASSERT(c->ctx_resumes > 0);
	longjmp(c->ctx_pcb, 1);
}

mdb_context_t *
mdb_context_create(int (*func)(void))
{
	mdb_context_t *c = mdb_zalloc(sizeof (mdb_context_t), UM_NOSLEEP);
	size_t pagesize = sysconf(_SC_PAGESIZE);
	int prot = sysconf(_SC_STACK_PROT);
	static int zfd = -1;
	RTC_Result res;

	if (c == NULL)
		return (NULL);

	if (prot == -1)
		prot = PROT_READ | PROT_WRITE | PROT_EXEC;

	c->ctx_func = func;
	c->ctx_stacksize = pagesize * 4;
	c->ctx_stack = mmap(NULL, c->ctx_stacksize, prot,
	    MAP_PRIVATE | MAP_ANON, -1, 0);

	/*
	 * If the mmap failed with EBADFD, this kernel doesn't have MAP_ANON
	 * support; fall back to opening /dev/zero, caching the fd, and using
	 * that to mmap chunks of anonymous memory.
	 */
	if (c->ctx_stack == MAP_FAILED && errno == EBADF) {
		if (zfd == -1 && (zfd = open("/dev/zero", O_RDWR)) >= 0)
			(void) fcntl(zfd, F_SETFD, FD_CLOEXEC);

		if (zfd >= 0) {
			c->ctx_stack = mmap(NULL, c->ctx_stacksize, prot,
			    MAP_PRIVATE, zfd, 0);
		}
	}

	if (c->ctx_stack == MAP_FAILED) {
		mdb_free(c, sizeof (mdb_context_t));
		return (NULL);
	}

	if ((res = _rtc_check_malloc(c->ctx_stacksize)) != RTC_SUCCESS ||
	    (res = _rtc_record_malloc(c->ctx_stack,
	    c->ctx_stacksize)) != RTC_SUCCESS)
		_rtc_report_error(res);

	_lwp_makecontext(&c->ctx_uc, (void (*)(void *))context_init,
	    c, NULL, c->ctx_stack, c->ctx_stacksize);

	return (c);
}

void
mdb_context_destroy(mdb_context_t *c)
{
	RTC_Result res = _rtc_check_free(c->ctx_stack);

	if (res != RTC_SUCCESS)
		_rtc_report_error(res);

	if (munmap(c->ctx_stack, c->ctx_stacksize) == -1)
		fail("failed to unmap stack %p", c->ctx_stack);

	(void) _rtc_record_free(c->ctx_stack);
	mdb_free(c, sizeof (mdb_context_t));
}

void
mdb_context_switch(mdb_context_t *c)
{
	if (setjmp(c->ctx_pcb) == 0 && setcontext(&c->ctx_uc) == -1)
		fail("failed to change context to %p", (void *)c);
	else
		fail("unexpectedly returned from context %p", (void *)c);
}

jmp_buf *
mdb_context_getpcb(mdb_context_t *c)
{
	c->ctx_resumes++;
	return (&c->ctx_pcb);
}
