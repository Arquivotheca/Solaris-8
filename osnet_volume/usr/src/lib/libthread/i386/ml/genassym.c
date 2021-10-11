/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)genassym.c 1.15     97/07/24     SMI"

#include <libthread.h>
#include <signal.h> /* for SIG_SETMASK */
#include <sys/psw.h>
#include <ucontext.h>

/*
 * Used to generate WAITER_MASK below.
 */
#define	default_waiter_mask 0x000000ff

main()
{
	struct thread *t = (struct thread *)0;
	struct tls *tls = (struct tls *)0;
	mutex_t *mp = (mutex_t *)0;
	ucontext_t *ucp = (ucontext_t *)0;
	lwp_mutex_t lm;

	printf("#define\tTHREAD_SIZE\t0x%x\n", sizeof (struct thread));
	printf("#define\tT_FCTRL\t0x%x\n", &t->t_fctrl);
	printf("#define\tT_FSTAT\t0x%x\n", &t->t_fstat);
	printf("#define\tT_FPENV\t0x%x\n", &t->t_fpenv);
	printf("#define\tT_PC\t0x%x\n", &t->t_pc);
	printf("#define\tT_SP\t0x%x\n", &t->t_sp);
	printf("#define\tT_BP\t0x%x\n", &t->t_bp);
	printf("#define\tT_EDI\t0x%x\n", &t->t_edi);
	printf("#define\tT_ESI\t0x%x\n", &t->t_esi);
	printf("#define\tT_EBX\t0x%x\n", &t->t_ebx);
	printf("#define\tT_LOCK\t0x%x\n", &t->t_lock);
	printf("#define\tT_STOP\t0x%x\n", &t->t_stop);
	printf("#define\tT_PSIG\t0x%x\n", &t->t_psig);
	printf("#define\tT_HOLD\t0x%x\n", &t->t_hold);
	printf("#define\tT_FLAG\t0x%x\n", &t->t_flag);
	printf("#define\tT_SSIG\t0x%x\n", &t->t_ssig);
	printf("#define\tT_NOSIG\t0x%x\n", &t->t_nosig);
	printf("#define\tT_PENDING\t0x%x\n", &t->t_pending);
	printf("#define\tT_LINK\t0x%x\n", &t->t_link);
	printf("#define\tT_PREV\t0x%x\n", &t->t_prev);
	printf("#define\tT_NEXT\t0x%x\n", &t->t_next);
	printf("#define\tT_TLS\t0x%x\n", &t->t_tls);
	printf("#define\tT_STATE\t0x%x\n", &t->t_state);
	printf("#define\tT_STKSIZE\t0x%x\n", &t->t_stksize);
	printf("#define\tT_TID\t0x%x\n", &t->t_tid);
	printf("#define\tT_LWPID\t0x%x\n", &t->t_lwpid);
	printf("#define\tT_IDLE\t0x%x\n", &t->t_idle);
	printf("#define\tT_FORW\t0x%x\n", &t->t_forw);
	printf("#define\tT_BACKW\t0x%x\n", &t->t_backw);
	printf("#define\tT_IDLETHREAD\t0x%x\n", T_IDLETHREAD);
	printf("#define\tFP_SIZE\t0x%lx\n", sizeof (struct fpuenv));
	printf("#define\tMUTEX_LOCK\t0x%p\n", &mp->mutex_lockw);
	printf("#define\tMUTEX_OWNER\t0x%p\n", &mp->mutex_owner);
	printf("#define\tMUTEX_WANTED\t0x%x\n", 0);
	printf("#define\tT_USROPTS\t0x%p\n", &t->t_usropts);
	printf("#define M_LOCK_WORD 0x%p\n",
	    (char *)&lm.mutex_lockword - (char *)&lm);
	printf("#define	M_WAITER_MASK 0x%x\n",
	    (uint_t)default_waiter_mask <<
	    (8*((char *)&lm.mutex_waiters - (char *)&lm.mutex_lockword)));
	/* PROBE_SUPPORT begin */
	printf("#define\tT_TPDP\t0x%p\n", &t->t_tpdp);
	/* PROBE_SUPPORT end */

	/* cancellation support being */
	printf("#define\tT_CANSTATE\t0x%p\n", &t->t_can_state);
	printf("#define\tT_CANTYPE\t0x%p\n", &t->t_can_type);
	printf("#define\tT_CANPENDING\t0x%p\n", &t->t_can_pending);
	printf("#define\tT_CANCELABLE\t0x%p\n", &t->t_cancelable);

	printf("#define\tTC_PENDING\t0x%x\n", TC_PENDING);
	printf("#define\tTC_DISABLE\t0x%x\n", TC_DISABLE);
	printf("#define\tTC_ENABLE\t0x%x\n", TC_ENABLE);
	printf("#define\tTC_ASYNCHRONOUS\t0x%x\n", TC_ASYNCHRONOUS);
	printf("#define\tTC_DEFERRED\t0x%x\n", TC_DEFERRED);
	printf("#define\tPTHREAD_CANCELED\t0x%x\n", PTHREAD_CANCELED);
	/* cancellation support ends */

	printf("#define\tTS_ZOMB\t0x%x\n", TS_ZOMB);
	printf("#define\tTS_ONPROC\t0x%x\n", TS_ONPROC);
	printf("#define\tSIG_SETMASK\t0x%x\n", SIG_SETMASK);
	printf("#define\tPAGESIZE\t0x%lx\n", PAGESIZE);
#ifdef TRACE_INTERNAL
	printf("#define\tTR_T_SWTCH\t0x%x\n", TR_T_SWTCH);
#endif
	/*
	 * offsets used by libthread's sigsetjmp, written in assembler
	 */

	printf("#define\tUC_ALL\t0x%x\n", UC_ALL);
	printf("#define\tUC_SIGMASK\t0x%x\n", UC_SIGMASK);
	printf("#define\tUC_EAX\t0x%x\n", &(ucp->uc_mcontext.gregs[EAX]));
	printf("#define\tUC_UESP\t0x%x\n", &(ucp->uc_mcontext.gregs[UESP]));
	printf("#define\tUC_EIP\t0x%x\n", &(ucp->uc_mcontext.gregs[EIP]));

	/*
	 * The following defines are required only for the version
	 * of sigsetjmp() which does not call getcontext().
	 */

	/*
	 * required by ABI for preservation across sigsetjmp/longjmp
	 */
	printf("#define\tUC_EBP\t0x%x\n", &(ucp->uc_mcontext.gregs[EBP]));
	printf("#define\tUC_EBX\t0x%x\n", &(ucp->uc_mcontext.gregs[EBX]));
	printf("#define\tUC_EDI\t0x%x\n", &(ucp->uc_mcontext.gregs[EDI]));
	printf("#define\tUC_ESI\t0x%x\n", &(ucp->uc_mcontext.gregs[ESI]));

	/*
	 * segment registers need to be preserved too.
	 */
	printf("#define\tUC_DS\t0x%x\n", &(ucp->uc_mcontext.gregs[DS]));
	printf("#define\tUC_ES\t0x%x\n", &(ucp->uc_mcontext.gregs[ES]));
	printf("#define\tUC_FS\t0x%x\n", &(ucp->uc_mcontext.gregs[FS]));
	printf("#define\tUC_GS\t0x%x\n", &(ucp->uc_mcontext.gregs[GS]));
	printf("#define\tUC_CS\t0x%x\n", &(ucp->uc_mcontext.gregs[CS]));
	printf("#define\tUC_SS\t0x%x\n", &(ucp->uc_mcontext.gregs[SS]));

	exit(0);
}
