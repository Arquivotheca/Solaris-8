#pragma ident "@(#)siglongjmp.c	1.7	98/04/21 SMI"

#ifdef __STDC__
#pragma weak siglongjmp = _siglongjmp
#endif /* __STDC__ */

#include "libthread.h"
#include <sys/ucontext.h>
#include <sys/types.h>
#include <setjmp.h>

/*
 * _sigsetjmp() sets up the jmp buffer so that on a subsequent _siglongjmp()
 * to this buffer, control first goes to __thr_jmpret() below. The stack
 * has been carefully crafted so that on return from __thr_jmpret(), the
 * thread returns to the caller of _sigsetjmp(), at the place right after
 * the call. __thr_jmpret() is needed for correct switching of signal masks
 * in an MT process.
 */
int
__thr_jmpret(sigjmp_buf env, int val)
{
	register ucontext_t *ucp = (ucontext_t *)env;

	if (ucp->uc_flags & UC_SIGMASK)
		thr_sigsetmask(SIG_SETMASK, &(ucp->uc_sigmask), NULL);
	return (val);
}

/*
 * The following routine is the latter part of sigsetjmp(), which starts out
 * in assembly language. It is called only if the signal mask needs to be saved.
 * sigsetjmp() replaces its second argument, with an argument which represents
 * the correct value of the return PC to be used by siglongjmp().
 * The following routine sets up the context so that a siglongjmp() to this
 * context first enters __thr_jmpret().
 * siglongjmp() also dummies up the stack so that a return from __thr_jmpret(),
 * returns to this return PC ("retpc" below). "retpc" should be the caller
 * of sigsetjmp().
 */
int
__csigsetjmp(sigjmp_buf env, greg_t retpc)
{
	register ucontext_t *ucp = (ucontext_t *)env;
	register uthread_t *t = curthread;
	register greg_t *cpup;
	/*
	 * Fix
	 *	%eip: pointer to __thr_jmpret(),
	 *	%edx: store the PC __thr_jmpret() should return to
	 */
	ucp->uc_sigmask = t->t_hold;
	cpup = (greg_t *)&(ucp->uc_mcontext.gregs);
	cpup[EIP] = (greg_t)(&__thr_jmpret);
	cpup[EDX] = retpc;
	return (0);
}

void
_siglongjmp(sigjmp_buf env, int val)
{
	ucontext_t ucl = *((ucontext_t *)env);
	register greg_t *cpup;
	int *sp;

	/*
	 * First empty the signal mask. It is being passed to setcontext
	 * below which impacts the LWP mask. The signal mask stored in the
	 * jump buf is not touched since "ucl" is a local copy of the jumpbuf
	 * The signal mask is switched, if necessary, in __thr_jmpret() which
	 * has access to the jump buf, via one of its arguments.
	 */
	_sigemptyset(&ucl.uc_sigmask);
	cpup = (greg_t *)&ucl.uc_mcontext.gregs;
	/*
	 * If val is non-zero, store it in the ucontext's EAX register to
	 * simulate a return from __thr_jmpret() (i.e. _sigsetjmp()) with this
	 * value. Since cpup points to a local copy, the jmp buf is not touched
	 * and so a second call to _siglongjmp() with the same jmp buf works
	 * as expected.
	 */
	if (val)
		cpup[EAX] = val;
	else
		cpup[EAX] = 1;
	if (ucl.uc_flags & UC_SIGMASK) {
		/*
		 * If the jmp buf has a saved signal mask, the current mask
		 * cannot be changed until the longjmp to the new context
		 * occurs. Until then, to make the signal mask change atomic
		 * with respect to the switch, mask all signals on the current
		 * thread, which flushes all signals for this thread. Now,
		 * this thread should not receive any signals until the mask
		 * switch occurs in __thr_jmpret().
		 */
		thr_sigsetmask(SIG_SETMASK, &_totalmasked, NULL);
		sp = (int *)cpup[UESP];
		/* "sp" is the stack being switched to */

		/*
		 * Take the return pc for _sigsetjmp() (pointing to the caller
		 * of _sigsetjmp()) and store it into the first word that the
		 * stack points to. i386 calling convention expects the return
		 * address to be the first word pointed to by %esp on entry to
		 * a function.
		 */

		*(sp) = (long)cpup[EDX];

		/*
		 * The following two lines serve to dummy-up the stack for the
		 * call to __thr_jmpret() with arguments that it needs. The
		 * first word on the stack is the return pc, stored by the
		 * above line. The second word on the stack is the first
		 * argument, and the third word on the stack is the second
		 * argument. Since sp is a "int *", adding 1 and 2 works.
		 */
		*(sp+1) = (long)env;
		*(sp+2) = cpup[EAX];
	}
	setcontext(&ucl);
}
