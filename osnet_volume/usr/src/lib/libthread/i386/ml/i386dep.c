/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)i386dep.c	1.23	99/06/09 SMI"

#define	NOTHREAD

#include "libthread.h"
#include <sys/reg.h>
#include <sys/psw.h>

/* PROBE_SUPPORT begin */
extern void * _thread_probe_getfunc(void);
void * (*thr_probe_getfunc_addr)(void) = _thread_probe_getfunc;
/* PROBE_SUPPORT end */

/*
 * load a function call into a thread.
 */
int
_thread_call(uthread_t *t, void (*func)(), void *arg)
{
	int *stack;

	/*
	 * create a "c" call frame for "thread_start", the caller
	 * of "func".
	 */
	stack = (int *)t->t_sp;
	*--stack = 0;
	*--stack = (int)func;
	*--stack = (int)arg;
	*--stack = (int)t;

	/*
	 * set thread to resume at thread_start()
	 */
	t->t_pc = (uint)_thread_start;
	t->t_sp = (uint)stack;
	t->t_bp = 0;
	return (0);
}

void
_thread_ret(uthread_t *t, void (*func)())
{
	t->t_pc = (uint)func;
}

ucontext_t _ucontext_proto;	/* prototype for _setcontext */

int
_lwp_exec(uthread_t *t, uintptr_t retpc, caddr_t sp, void (*func)(),
			int flags, lwpid_t *lwpidp)
{
	ucontext_t uc;
	int ret;

	ASSERT(t != NULL);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_START, "_lwpexec start");

	uc = _ucontext_proto;		/* structure assign */

	uc.uc_mcontext.gregs[EIP] = (int)func;
	uc.uc_mcontext.gregs[EBP] = 0;
	uc.uc_mcontext.gregs[UESP] = (int)sp;
	uc.uc_mcontext.gregs[GS] = __setupgs(t); /* libc/i386/sys/machlwp.c */

	if (flags & __LWP_ASLWP)
		uc.uc_sigmask = t->t_hold;
	else
		uc.uc_sigmask = _null_sigset;
	if (lwpidp == NULL)
		_panic("lwp_exec: NULL lwpidp");
	ret = _lwp_create(&uc, flags, lwpidp);

	if (ret > 0) {
		*lwpidp = ret;
		ret = 0;
	}

	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_END, "_lwpexec end");
	return (ret);
}

void
_init386(void)
{
	/*
	 * Set up prototype ucontext structure.
	 */
	__getcontext(&_ucontext_proto);
	_ucontext_proto.uc_mcontext.gregs[EDI] = 0;
	_ucontext_proto.uc_mcontext.gregs[ESI] = 0;
	_ucontext_proto.uc_mcontext.gregs[EBP] = 0;
	_ucontext_proto.uc_mcontext.gregs[EBX] = 0;
	_ucontext_proto.uc_mcontext.gregs[EDX] = 0;
	_ucontext_proto.uc_mcontext.gregs[ECX] = 0;
	_ucontext_proto.uc_mcontext.gregs[EAX] = 0;
	_ucontext_proto.uc_mcontext.gregs[TRAPNO] = 0;
	_ucontext_proto.uc_mcontext.gregs[ERR] = 0;
	_ucontext_proto.uc_mcontext.gregs[EIP] = 0;
	_ucontext_proto.uc_mcontext.gregs[EFL] &= ~PSL_USERMASK;
	_ucontext_proto.uc_mcontext.gregs[UESP] = 0;
}

void
_fpinherit(uthread_t *t)
{
	_copy_fctrl_fstat(&t->t_resumestate.rs_fpuenv.fctrl,
	    &t->t_resumestate.rs_fpuenv.fstat);
	t->t_resumestate.rs_fpuenv.ftag = 0xffffffff;

}

void
_get_resumestate(uthread_t *t, gregset_t rs)
{
	rs[EBP] = t->t_resumestate.rs_bp;
	rs[EDI] = t->t_resumestate.rs_edi;
	rs[ESI] = t->t_resumestate.rs_esi;
	rs[EBX] = t->t_resumestate.rs_ebx;
	rs[EIP] = t->t_resumestate.rs_pc;
	rs[UESP] = t->t_resumestate.rs_uesp;
}

void
_set_resumestate(uthread_t *t, gregset_t rs)
{
	t->t_resumestate.rs_bp = rs[EBP];
	t->t_resumestate.rs_edi = rs[EDI];
	t->t_resumestate.rs_esi = rs[ESI];
	t->t_resumestate.rs_ebx = rs[EBX];
	t->t_resumestate.rs_pc = rs[EIP];
	t->t_resumestate.rs_uesp = rs[UESP];
}
