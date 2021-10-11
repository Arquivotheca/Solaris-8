#ident "@(#)syslwp.s 1.26 98/09/16"

	.file "syslwp.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_create,function)
	ANSI_PRAGMA_WEAK(_lwp_continue,function)
	ANSI_PRAGMA_WEAK(_lwp_suspend,function)
	ANSI_PRAGMA_WEAK(_lwp_suspend2,function)
	ANSI_PRAGMA_WEAK(_lwp_kill,function)
	ANSI_PRAGMA_WEAK(_lwp_self,function)
	ANSI_PRAGMA_WEAK(_lwp_wait,function)
	ANSI_PRAGMA_WEAK(_lwp_exit,function)
	ANSI_PRAGMA_WEAK(_lwp_cond_broadcast,function)
	ANSI_PRAGMA_WEAK(_lwp_cond_signal,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_wait,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_trywait,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_post,function)
	ANSI_PRAGMA_WEAK(_lwp_setprivate,function)
	ANSI_PRAGMA_WEAK(_lwp_getprivate,function)
	ANSI_PRAGMA_WEAK(_lwp_info,function)
	ANSI_PRAGMA_WEAK(_lwp_schedctl,function)
	ANSI_PRAGMA_WEAK(_lwp_sigredirect,function)
	ANSI_PRAGMA_WEAK(_lwp_sigtimedwait,function)

#include "SYS.h"

/*
 * int
 * _lwp_create (uc, flags, lwpidp)
 *	ucontext_t *uc;
 *	unsigned long flags;
 *	lwpid_t *lwpidp;
 */
	ENTRY(_lwp_create)
	SYSTRAP(lwp_create)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_create)

/*
 * int
 * _lwp_continue (lwpid)
 *	lwp_id_t lwpid;
 */
	ENTRY(_lwp_continue)
	SYSTRAP(lwp_continue)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_continue)
/*
 * int
 * _lwp_suspend (lwpid)
 * 	lwp_id_t lwpid;
 */
	ENTRY(_lwp_suspend)
	SYSTRAP(lwp_suspend)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_suspend)
/*
 * int
 * _lwp_suspend2(lwp_id_t lwp, int *sysnump)
 */
	ENTRY(_lwp_suspend2)
	st	%o1, [%sp + 68]		!save address of sysnump
	SYSTRAP(lwp_suspend)
	bcc,a	2f
	clr	%o0
	cmp	%o0, ERESTART
	beq,a	1f
	mov	EINTR, %o0
1:
	RET
2:	
	ld	[%sp + 68], %o2		!retrieve address of sysnump
	tst	%o2			!verify that address is not zero
	bnz,a	3f
	st	%o1, [%o2]		!set sysnump to rval2
3:
	RET
	SET_SIZE(_lwp_suspend2)

/*
 * int
 * _lwp_kill (lwpid,sig)
 *	lwp_id_t lwpid;
 *	int sig;
 */

	ENTRY(_lwp_kill);
	SYSTRAP(lwp_kill)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_kill)
/*
 * lwp_id_t
 * _lwp_self ()
 */
	ENTRY(_lwp_self)
	SYSTRAP(lwp_self)
	SYSCERROR
	RET
	SET_SIZE(_lwp_self)

/*
 * void *
 * _lwp_getprivate()
 */
	ENTRY(_lwp_getprivate)
	SYSTRAP(lwp_getprivate)
	SYSCERROR
	RET
	SET_SIZE(_lwp_getprivate)

/*
 * void
 * _lwp_setprivate()
 */
	ENTRY(_lwp_setprivate)
	SYSTRAP(lwp_setprivate)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_setprivate)

/*
 * int
 * _lwp_wait (lwpid, departed)
 *	lwp_id_t lwpid;
 *	lwpid_t *departed;
 */
	ENTRY(_lwp_wait)
	SYSTRAP(lwp_wait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_wait)

/*
 * void
 * _lwp_exit ()
 */
	ENTRY(_lwp_exit)
	SYSTRAP(lwp_exit)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_exit)

/*
 * int
 * ___lwp_mutex_lock (mp)
 *	mutex_t *mp;
 */
	SYSREENTRY(___lwp_mutex_lock)
	SYSTRAP(lwp_mutex_lock)
	SYSINTR_RESTART(.restart____lwp_mutex_lock)
	RET
	SET_SIZE(___lwp_mutex_lock)

/*
* int
* ___lwp_mutex_wakeup (mp)
*	mutex_t *mp;
*/
 	ENTRY(___lwp_mutex_wakeup)
 	SYSTRAP(lwp_mutex_wakeup)
 	SYSLWPERR
 	RET
 	SET_SIZE(___lwp_mutex_wakeup)

/*
 * int
 * _lwp_cond_broadcast (cvp)
 * 	lwp_cond_t *cvp;
 */
	ENTRY(_lwp_cond_broadcast)
	SYSTRAP(lwp_cond_broadcast)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_cond_broadcast)

/*
 * int
 * ___lwp_cond_wait (cvp,mp,ts)
 * 	lwp_cond_t *cvp;
 *	lwp_mutex_t *mp;
 *	timestruc_t *ts;
 */
	ENTRY(___lwp_cond_wait)
	SYSTRAP(lwp_cond_wait);
	SYSLWPERR
	RET
	SET_SIZE(___lwp_cond_wait)

/*
 * int
 * _lwp_cond_signal (cvp)
 * 	lwp_cond_t *cvp;
 */
	ENTRY(_lwp_cond_signal)
	SYSTRAP(lwp_cond_signal)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_cond_signal)

/*
 * int
 * _lwp_sema_wait (sp);
 *	lwp_sema_t *sp;
 */
	SYSREENTRY(_lwp_sema_wait)
	SYSTRAP(lwp_sema_wait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_wait)

/*
 * int
 * _lwp_sema_trywait (sp);
 *	lwp_sema_t *sp;
 */
	ENTRY(_lwp_sema_trywait)
	SYSTRAP(lwp_sema_trywait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_trywait)

/*
 * int
 * _lwp_sema_post (sp);
 *	lwp_sema_t *sp;
 */
	ENTRY(_lwp_sema_post)
	SYSTRAP(lwp_sema_post)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_post)

/*
 * int
 * _lwp_info (infop);
 *	struct lwpinfo *infop;
 */
	ENTRY(_lwp_info)
	SYSTRAP(lwp_info)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_info)

/*
 * int
 * _lwp_schedctl (flags, upcall_did, addrp)
 *	unsigned long flags; 
 *	int upcall_did;
 *	sc_shared_t **addrp;
 */
	ENTRY(_lwp_schedctl)
	SYSTRAP(schedctl)
	SYSCERROR
	RET
	SET_SIZE(_lwp_schedctl)

/*
 * int
 * _lwp_sigredirect (lwpid, sig, queued);
 *	int lwpid;
 *	int sig;
 * 	int *queued;
 */
	ENTRY(_lwp_sigredirect)
	SYSTRAP(lwp_sigredirect)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sigredirect)

/*
 * int
 * ___lwp_mutex_trylock (mp)
 *	mutex_t *mp;
 */
	ENTRY(___lwp_mutex_trylock)
	SYSTRAP(lwp_mutex_trylock)
 	SYSLWPERR
	RET
	SET_SIZE(___lwp_mutex_trylock)

/*
 * int
 * ___lwp_mutex_unlock (mp)
 *	mutex_t *mp;
 */
 	ENTRY(___lwp_mutex_unlock)
 	SYSTRAP(lwp_mutex_unlock)
 	SYSLWPERR
 	RET
 	SET_SIZE(___lwp_mutex_unlock)
/*
 * int
 * ___lwp_mutex_init(mp, type)
 *	mutex_t *mp;
 *	int type;
 */

	ENTRY(___lwp_mutex_init)
	SYSTRAP(lwp_mutex_init)
	SYSLWPERR
	RET
	SET_SIZE(___lwp_mutex_init)

/*
 * int
 * _lwp_sigtimedwait(set, info, timeout, queued)
 *	const sigset_t *set;
 *	siginfo_t *info;
 *	const struct timespec *timeout;
 *	int queued;
 */

	ENTRY(_lwp_sigtimedwait)
	SYSTRAP(lwp_sigtimedwait)
	SYSCERROR
	RET
	SET_SIZE(_lwp_sigtimedwait)
