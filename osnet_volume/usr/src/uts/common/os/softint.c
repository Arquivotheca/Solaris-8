/*
 * Copyright (c) 1985-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)softint.c	1.24	99/10/25 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/spl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * Handle software interrupts through 'softcall' mechanism
 */

typedef void (*func_t)(void *);

#define	NSOFTCALLS	200

static struct softcall {
	func_t	sc_func;		/* function to call */
	void	*sc_arg;		/* arg to pass to func */
	struct softcall *sc_next;	/* next in list */
} softcalls[NSOFTCALLS];

static struct softcall *softhead, *softtail, *softfree;

static kmutex_t	softcall_lock;		/* protects softcall lists */

void
softcall_init(void)
{
	struct softcall *sc;

	for (sc = softcalls; sc < &softcalls[NSOFTCALLS]; sc++) {
		sc->sc_next = softfree;
		softfree = sc;
	}
	mutex_init(&softcall_lock, NULL, MUTEX_SPIN, (void *)ipltospl(SPL8));
}

/*
 * Call function func with argument arg
 * at some later time at software interrupt priority
 */
void
softcall(func_t func, void *arg)
{
	struct softcall *sc;
	extern void siron();

	/*
	 * protect against cross-calls
	 */
	mutex_enter(&softcall_lock);
	/* coalesce identical softcalls */
	for (sc = softhead; sc != 0; sc = sc->sc_next) {
		if (sc->sc_func == func && sc->sc_arg == arg) {
			mutex_exit(&softcall_lock);
			return;
		}
	}
	if ((sc = softfree) == 0)
		panic("too many softcalls");
	softfree = sc->sc_next;
	sc->sc_func = func;
	sc->sc_arg = arg;
	sc->sc_next = 0;

	if (softhead) {
		softtail->sc_next = sc;
		softtail = sc;
		mutex_exit(&softcall_lock);
	} else {
		softhead = softtail = sc;
		mutex_exit(&softcall_lock);
		siron();
	}
}

/*
 * Called to process software interrupts
 * take one off queue, call it, repeat
 * Note queue may change during call
 */
void
softint(void)
{
	struct softcall *sc;
	func_t func;
	caddr_t arg;

	for (;;) {
		mutex_enter(&softcall_lock);
		if ((sc = softhead) != NULL) {
			func = sc->sc_func;
			arg = sc->sc_arg;
			softhead = sc->sc_next;
			sc->sc_next = softfree;
			softfree = sc;
		}
		mutex_exit(&softcall_lock);
		if (sc == NULL)
			return;
		(*func)(arg);
	}
}
