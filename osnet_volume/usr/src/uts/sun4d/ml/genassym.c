/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genassym.c	1.110	99/09/22 SMI"

#ifndef _GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/elf_notes.h>
#include <sys/sysinfo.h>
#include <sys/vmmeter.h>
#include <sys/vmparam.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/mutex_impl.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/rt.h>
#include <sys/ts.h>
#include <sys/obpdefs.h>
#include <sys/cpuvar.h>

#include <sys/pte.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/scb.h>
#include <sys/clock.h>
#include <sys/memerr.h>
#include <sys/eeprom.h>
#include <sys/vtrace.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <vm/hat_srmmu.h>

#include <sys/avintr.h>

#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/file.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/fdvar.h>
#include <sys/dditypes.h>


#define	OFFSET(type, field)	((int)(&((type *)0)->field))


main()
{
	struct dev_info *devi = (struct dev_info *)0;
	struct dev_ops *ops = (struct dev_ops *)0;
	struct bus_ops *busops = (struct bus_ops *)0;
	struct proc *p = (struct proc *)0;
	kthread_t *tid = (kthread_t *)0;
	klwp_t *lwpid = (klwp_t *)0;
	struct user *up = (struct user *)0;
	struct regs *rp = (struct regs *)0;
	struct as *as = (struct as *)0;
	struct hat *hat = (struct hat *)0;
	struct srmmu *srmmu = (struct srmmu *)0;
	struct ctx *ctx = (struct ctx *)0;
	label_t *l = 0;
	struct counterregs *cntr = (struct counterregs *)0;
	struct fpu *fp = (struct fpu *)0;
	struct autovec *av = (struct autovec *)0;
	struct timestruc *tms = (struct timestruc *)0;
	struct machpcb *mpcb = (struct machpcb *)0;
	ddi_dma_impl_t *dmah = (ddi_dma_impl_t *)0;
	ddi_nofault_data_t *nfdata = (ddi_nofault_data_t *)0;


	printf("#define\tP_AS 0x%x\n", &p->p_as);
	printf("#define\tP_FLAG 0x%x\n", &p->p_flag);
	printf("#define\tP_PIDP 0x%x\n", &p->p_pidp);
	printf("#define\tP_USER 0x%x\n", &p->p_user);
	printf("#define\tPID_PIDID 0x%x\n",
		OFFSET(struct pid, pid_id));
	printf("#define\tP_FIXALIGNMENT 0x%x\n", &p->p_fixalignment);
	printf("#define\tPROC_SIZE 0x%x\n", sizeof (struct proc));
	printf("#define\tSLOAD 0x%x\n", SLOAD);

	printf("#define\tT_LOCKP 0x%x\n", &tid->t_lockp);
	printf("#define\tT_LOCK_FLUSH 0x%x\n", &tid->t_lock_flush);
	printf("#define\tT_KPRI_REQ 0x%x\n", &tid->t_kpri_req);
	printf("#define\tT_LOCKSTAT 0x%x\n", &tid->t_lockstat);
	printf("#define\tT_PREEMPT_LK 0x%x\n", &tid->t_preempt_lk);
	printf("#define\tT_OLDSPL 0x%x\n", &tid->t_oldspl);
	printf("#define\tT_PRI 0x%x\n", &tid->t_pri);
	printf("#define\tT_PIL 0x%x\n", &tid->t_pil);
	printf("#define\tT_LWP 0x%x\n", &tid->t_lwp);
	printf("#define\tT_PROCP 0x%x\n", &tid->t_procp);
	printf("#define\tT_LINK 0x%x\n", &tid->t_link);
	printf("#define\tT_LOCK 0x%x\n", &tid->t_lock);
	printf("#define\tT_STATE 0x%x\n", &tid->t_state);
	printf("#define\tT_MSTATE 0x%x\n", &tid->t_mstate);
	printf("#define\tT_STACK 0x%x\n", &tid->t_stk);
	printf("#define\tT_FLAGS 0x%x\n", &tid->t_flag);
	printf("#define\tT_CTX 0x%x\n", &tid->t_ctx);
	printf("#define\tT_PC 0x%x\n", &tid->t_pcb.val[0]);
	printf("#define\tT_SP 0x%x\n", &tid->t_pcb.val[1]);
	printf("#define\tT_LOFAULT 0x%x\n", &tid->t_lofault);
	printf("#define\tT_ONFAULT 0x%x\n", &tid->t_onfault);
	printf("#define\tT_NOFAULT 0x%x\n", &tid->t_nofault);
	printf("#define\tT_CPU 0x%x\n", &tid->t_cpu);
	printf("#define\tT_BOUND_CPU 0x%x\n", &tid->t_bound_cpu);
	printf("#define\tT_INTR 0x%x\n", &tid->t_intr);
	printf("#define\tT_TID 0x%x\n", &tid->t_tid);
	printf("#define\tT_ASTFLAG 0x%x\n", &tid->t_astflag);
	printf("#define\tT_POST_SYS 0x%x\n", &tid->t_post_sys);
	printf("#define\tT_POST_SYS_AST 0x%x\n", &tid->t_post_sys_ast);
	printf("#define\tT_PRE_SYS 0x%x\n", &tid->t_pre_sys);
	printf("#define\tT_PROC_FLAG 0x%x\n", &tid->t_proc_flag);
	printf("#define\tT_SYSNUM 0x%x\n", &tid->t_sysnum);
	printf("#define\tT_INTR_THREAD %d\n", T_INTR_THREAD);
	printf("#define\tFREE_THREAD 0x%x\n", TS_FREE);
	printf("#define\tTS_ZOMB 0x%x\n", TS_ZOMB);
	printf("#define\tTP_MSACCT 0x%x\n", TP_MSACCT);
	printf("#define\tTP_WATCHPT 0x%x\n", TP_WATCHPT);
	printf("#define\tONPROC_THREAD 0x%x\n", TS_ONPROC);
	printf("#define\tT0STKSZ 0x%x\n", DEFAULTSTKSZ * 2);
	printf("#define\tTHREAD_SIZE 0x%x\n", sizeof (kthread_t));
	printf("#define\tCPU_SYSINFO_SYSCALL 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.syscall));

	printf("#define\tA_HAT 0x%x\n", &as->a_hat);
	printf("#define\tSRMMU_HAT 0x%x\n", &hat->hat_data[0]);
	printf("#define\tSRMMU_CTX 0x%x\n", &srmmu->s_ctx);

	printf("#define\tS_READ 0x%x\n", (int)S_READ);
	printf("#define\tS_WRITE 0x%x\n", (int)S_WRITE);
	printf("#define\tS_EXEC 0x%x\n", (int)S_EXEC);
	printf("#define\tS_OTHER 0x%x\n", (int)S_OTHER);

	printf("#define\tL_PC 0x%x\n", &l->val[0]);
	printf("#define\tL_SP 0x%x\n", &l->val[1]);

	printf("#define\tUSER_SIZE 0x%x\n", sizeof (struct user));
	printf("#define\tU_COMM 0x%x\n", up->u_comm);

	printf("#define\tKL1PT_SIZE 0x%x\n", sizeof (union ptpe));
	printf("#define\tKL2PT_SIZE 0x%x\n", sizeof (union ptpe));

	/*
	 * locore.s uses N_NKL2PTS rather than NKL2PTS
	 * see sun4d/sys/machparam.h
	 */
	printf("#define\tN_NKL2PTS "
	    "((0xFFFFFFFF - KERNELBASE + 1) / L2PTSIZE)\n");

	printf("#define\tLWP_THREAD 0x%x\n", &lwpid->lwp_thread);
	printf("#define\tLWP_REGS 0x%x\n", &lwpid->lwp_regs);
	printf("#define\tLWP_STATE 0x%x\n", &lwpid->lwp_state);
	printf("#define\tLWP_STIME 0x%x\n", &lwpid->lwp_stime);
	printf("#define\tLWP_USER 0x%x\n", LWP_USER);
	printf("#define\tLWP_UTIME 0x%x\n", &lwpid->lwp_utime);
	printf("#define\tLWP_RU_SYSC 0x%x\n", &lwpid->lwp_ru.sysc);
	printf("#define\tLWP_SYS 0x%x\n", LWP_SYS);
	printf("#define\tLWP_STATE_START 0x%x\n",
				&lwpid->lwp_mstate.ms_state_start);
	printf("#define\tLWP_ACCT_USER 0x%x\n",
				&lwpid->lwp_mstate.ms_acct[LMS_USER]);
	printf("#define\tLWP_ACCT_SYSTEM 0x%x\n",
				&lwpid->lwp_mstate.ms_acct[LMS_SYSTEM]);
	printf("#define\tLWP_MS_PREV 0x%x\n",
				&lwpid->lwp_mstate.ms_prev);
	printf("#define\tLWP_MS_START 0x%x\n", &lwpid->lwp_mstate.ms_start);
	printf("#define\tLWP_PCB 0x%x\n", &lwpid->lwp_pcb);
	printf("#define\tPCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tPCB_TRAP0 0x%x\n", &lwpid->lwp_pcb.pcb_trap0addr);

	printf("#define\tMPCBSIZE 0x%x\n", sizeof (struct machpcb));
	printf("#define\tMPCB_REGS 0x%x\n", &mpcb->mpcb_regs);
	printf("#define\tMPCB_WBUF 0x%x\n", mpcb->mpcb_wbuf);
	printf("#define\tMPCB_SPBUF 0x%x\n", mpcb->mpcb_spbuf);
	printf("#define\tMPCB_WBCNT 0x%x\n", &mpcb->mpcb_wbcnt);
	printf("#define\tMPCB_RWIN 0x%x\n", mpcb->mpcb_rwin);
	printf("#define\tMPCB_RSP 0x%x\n", mpcb->mpcb_rsp);
	printf("#define\tMPCB_UWM 0x%x\n", &mpcb->mpcb_uwm);
	printf("#define\tMPCB_SWM 0x%x\n", &mpcb->mpcb_swm);
	printf("#define\tMPCB_FPU_REGS 0x%x\n",
				&mpcb->mpcb_fpu.fpu_fr.fpu_regs[0]);
	printf("#define\tMPCB_FPU_FSR 0x%x\n",
				&mpcb->mpcb_fpu.fpu_fsr);
	printf("#define\tMPCB_FPU_Q 0x%x\n",
				mpcb->mpcb_fpu_q);
	printf("#define\tMPCB_FPU_QCNT 0x%x\n",
				&mpcb->mpcb_fpu.fpu_qcnt);
	printf("#define\tMPCB_FPU_EN 0x%x\n",
				&mpcb->mpcb_fpu.fpu_en);
	printf("#define\tMPCB_FLAGS 0x%x\n", &mpcb->mpcb_flags);
	printf("#define\tMPCB_THREAD 0x%x\n", &mpcb->mpcb_thread);

	printf("#define\tFPU_FSR 0x%x\n", &fp->fpu_fsr);

	printf("#define\tPSR_PIL_BIT %d\n", bit(PSR_PIL));
	printf("#define\tREGSIZE 0x%x\n", sizeof (struct regs));

	/*
	 * Originally from reg.h; relocated here to support v7/v9.
	 * Location of the users' stored registers relative to R0.
	 * Used as an index into a gregset_t array.
	 */
	printf("#define\tPSR\t(%d)\n", ((long)(&rp->r_psr))/4);
	printf("#define\tPC\t(%d)\n", ((long)(&rp->r_pc))/4);
	printf("#define\tnPC\t(%d)\n", ((long)(&rp->r_npc))/4);
	printf("#define\tY\t(%d)\n", ((long)(&rp->r_y))/4);
	printf("#define\tG1\t(%d)\n", ((long)(&rp->r_g1))/4);
	printf("#define\tG2\t(%d)\n", ((long)(&rp->r_g2))/4);
	printf("#define\tG3\t(%d)\n", ((long)(&rp->r_g3))/4);
	printf("#define\tG4\t(%d)\n", ((long)(&rp->r_g4))/4);
	printf("#define\tG5\t(%d)\n", ((long)(&rp->r_g5))/4);
	printf("#define\tG6\t(%d)\n", ((long)(&rp->r_g6))/4);
	printf("#define\tG7\t(%d)\n", ((long)(&rp->r_g7))/4);
	printf("#define\tO0\t(%d)\n", ((long)(&rp->r_o0))/4);
	printf("#define\tO1\t(%d)\n", ((long)(&rp->r_o1))/4);
	printf("#define\tO2\t(%d)\n", ((long)(&rp->r_o2))/4);
	printf("#define\tO3\t(%d)\n", ((long)(&rp->r_o3))/4);
	printf("#define\tO4\t(%d)\n", ((long)(&rp->r_o4))/4);
	printf("#define\tO5\t(%d)\n", ((long)(&rp->r_o5))/4);
	printf("#define\tO6\t(%d)\n", ((long)(&rp->r_o6))/4);
	printf("#define\tO7\t(%d)\n", ((long)(&rp->r_o7))/4);

	/*
	 * The following defines are for portability.
	 */
	printf("#define\tPS\tPSR\n");
	printf("#define\tSP\tO6\n");

	printf("#define\tELF_NOTE_SOLARIS \"%s\"\n", ELF_NOTE_SOLARIS);
	printf("#define\tELF_NOTE_PAGESIZE_HINT %d\n", ELF_NOTE_PAGESIZE_HINT);

	printf("#define\tAV_VECTOR 0x%x\n", &av->av_vector);
	printf("#define\tAV_INTARG 0x%x\n", &av->av_intarg);
	printf("#define\tAUTOVECSIZE 0x%x\n", sizeof (struct autovec));

	printf("#define\tCPU_ID 0x%x\n", OFFSET(struct cpu, cpu_id));
	printf("#define\tCPU_ENABLE %d\n", CPU_ENABLE);
	printf("#define\tCPU_EXISTS 0x%x\n", CPU_EXISTS);
	printf("#define\tCPU_FLAGS 0x%x\n", OFFSET(struct cpu, cpu_flags));
	printf("#define\tCPU_READY %d\n", CPU_READY);
	printf("#define\tCPU_QUIESCED %d\n", CPU_QUIESCED);
	printf("#define\tCPU_THREAD 0x%x\n", OFFSET(struct cpu, cpu_thread));
	printf("#define\tCPU_THREAD_LOCK 0x%x\n",
		OFFSET(struct cpu, cpu_thread_lock));
	printf("#define\tCPU_KPRUNRUN 0x%x\n",
		OFFSET(struct cpu, cpu_kprunrun));
	printf("#define\tCPU_SYNCFLT_STATUS 0x%x\n",
		OFFSET(struct cpu, cpu_m.syncflt_status));
	printf("#define\tCPU_SYNCFLT_ADDR 0x%x\n",
		OFFSET(struct cpu, cpu_m.syncflt_addr));
	printf("#define\tCPU_LWP 0x%x\n", OFFSET(struct cpu, cpu_lwp));
	printf("#define\tCPU_FPOWNER 0x%x\n", OFFSET(struct cpu, cpu_fpowner));
	printf("#define\tCPU_IDLE_THREAD 0x%x\n",
		OFFSET(struct cpu, cpu_idle_thread));
	printf("#define\tCPU_INTR_THREAD 0x%x\n",
		OFFSET(struct cpu, cpu_intr_thread));
	printf("#define\tCPU_INTR_ACTV 0x%x\n",
		OFFSET(struct cpu, cpu_intr_actv));
	printf("#define\tCPU_BASE_SPL 0x%x\n",
		OFFSET(struct cpu, cpu_base_spl));
	printf("#define\tCPU_ON_INTR 0x%x\n", OFFSET(struct cpu, cpu_on_intr));
	printf("#define\tCPU_INTR_STACK 0x%x\n",
		OFFSET(struct cpu, cpu_intr_stack));
	printf("#define\tCPU_STATS 0x%x\n", OFFSET(struct cpu, cpu_stat));
	printf("#define\tCPU_SYSINFO_INTR 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intr));
	printf("#define\tCPU_SYSINFO_INTRTHREAD 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intrthread));
	printf("#define\tCPU_SYSINFO_INTRBLK 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intrblk));
	printf("#define\tCPU_SYSINFO_CPUMIGRATE 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.cpumigrate));
	printf("#define\tCPU_SYSINFO_UO_CNT 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uo_cnt));
	printf("#define\tCPU_SYSINFO_UU_CNT 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uu_cnt));
	printf("#define\tCPU_SYSINFO_SO_CNT 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_so_cnt));
	printf("#define\tCPU_SYSINFO_SU_CNT 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_su_cnt));
	printf("#define\tCPU_SYSINFO_SUO_CNT 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_suo_cnt));

	printf("#define\tCPU_PROFILE_PC 0x%x\n",
		OFFSET(struct cpu, cpu_profile_pc));
	printf("#define\tCPU_PROFILE_PIL 0x%x\n",
		OFFSET(struct cpu, cpu_profile_pil));

	printf("#define\tCPU_TRACE 0x%x\n",
		OFFSET(struct cpu, cpu_trace));
	printf("#define\tCPU_TRACE_START 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_start));
	printf("#define\tCPU_TRACE_END 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_end));
	printf("#define\tCPU_TRACE_WRAP 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_wrap));
	printf("#define\tCPU_TRACE_HEAD 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_head));
	printf("#define\tCPU_TRACE_TAIL 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_tail));
	printf("#define\tCPU_TRACE_REDZONE 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_redzone));
	printf("#define\tCPU_TRACE_OVERFLOW 0x%x\n",
		OFFSET(struct cpu, cpu_trace.tbuf_overflow));
	printf("#define\tCPU_TRACE_REAL_MAP 0x%x\n",
		OFFSET(struct cpu, cpu_trace.real_event_map));
	printf("#define\tCPU_TRACE_EVENT_MAP 0x%x\n",
		OFFSET(struct cpu, cpu_trace.event_map));
	printf("#define\tCPU_TRACE_HRTIME 0x%x\n",
		OFFSET(struct cpu, cpu_trace.last_hrtime_lo32));
	printf("#define\tCPU_TRACE_THREAD 0x%x\n",
		OFFSET(struct cpu, cpu_trace.last_thread));
	printf("#define\tCPU_TRACE_SCRATCH 0x%x\n",
		OFFSET(struct cpu, cpu_trace.scratch[0]));

	printf("#define\tTRACE_KTID_HEAD 0x%x\n",
		OFFSET(struct vt_raw_kthread_id, head));
	printf("#define\tTRACE_KTID_TID 0x%x\n",
		OFFSET(struct vt_raw_kthread_id, tid));
	printf("#define\tTRACE_KTID_SIZE 0x%x\n",
		sizeof (struct vt_raw_kthread_id));

	printf("#define\tTRACE_ETIME_HEAD 0x%x\n",
		OFFSET(struct vt_elapsed_time, head));
	printf("#define\tTRACE_ETIME_TIME 0x%x\n",
		OFFSET(struct vt_elapsed_time, time));
	printf("#define\tTRACE_ETIME_SIZE 0x%x\n",
		sizeof (struct vt_elapsed_time));

	printf("#define\tCPU_MPCB 0x%x\n", OFFSET(struct cpu, cpu_m.mpcb));

	printf("#define\tDEVI_BUS_CTL 0x%x\n", &devi->devi_bus_ctl);
	printf("#define\tDEVI_BUS_DMA_MAP 0x%x\n", &devi->devi_bus_dma_map);
	printf("#define\tDEVI_BUS_DMA_ALLOCHDL 0x%x\n",
		&devi->devi_bus_dma_allochdl);
	printf("#define\tDEVI_BUS_DMA_FREEHDL 0x%x\n",
		&devi->devi_bus_dma_freehdl);
	printf("#define\tDEVI_BUS_DMA_BINDHDL 0x%x\n",
		&devi->devi_bus_dma_bindhdl);
	printf("#define\tDEVI_BUS_DMA_UNBINDHDL 0x%x\n",
		&devi->devi_bus_dma_unbindhdl);
	printf("#define\tDEVI_BUS_DMA_UNBINDFUNC 0x%x\n",
		&devi->devi_bus_dma_unbindfunc);
	printf("#define\tDEVI_BUS_DMA_FLUSH 0x%x\n", &devi->devi_bus_dma_flush);
	printf("#define\tDEVI_BUS_DMA_WIN 0x%x\n", &devi->devi_bus_dma_win);
	printf("#define\tDEVI_BUS_DMA_CTL 0x%x\n", &devi->devi_bus_dma_ctl);
	printf("#define\tDEVI_DEV_OPS 0x%x\n", &devi->devi_ops);
	printf("#define\tDEVI_BUS_OPS 0x%x\n", &ops->devo_bus_ops);
	printf("#define\tOPS_MAP 0x%x\n", &busops->bus_dma_map);
	printf("#define\tOPS_ALLOCHDL 0x%x\n", &busops->bus_dma_allochdl);
	printf("#define\tOPS_FREEHDL 0x%x\n", &busops->bus_dma_freehdl);
	printf("#define\tOPS_BINDHDL 0x%x\n", &busops->bus_dma_bindhdl);
	printf("#define\tOPS_UNBINDHDL 0x%x\n", &busops->bus_dma_unbindhdl);
	printf("#define\tOPS_FLUSH 0x%x\n", &busops->bus_dma_flush);
	printf("#define\tOPS_WIN 0x%x\n", &busops->bus_dma_win);
	printf("#define\tOPS_MCTL 0x%x\n", &busops->bus_dma_ctl);
	printf("#define\tOPS_CTL 0x%x\n", &busops->bus_ctl);

	printf("#define\tDMA_HANDLE_RDIP 0x%x\n", &dmah->dmai_rdip);
	printf("#define\tDMA_HANDLE_RFLAGS 0x%x\n", &dmah->dmai_rflags);

	printf("#define\tDMP_NOSYNC\t0x%x\n", DMP_NOSYNC);

	printf("#define\tAHI_GETB 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_get8));
	printf("#define\tAHI_GETW 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_get16));
	printf("#define\tAHI_GETL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_get32));
	printf("#define\tAHI_GETLL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_get64));
	printf("#define\tAHI_PUTB 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_put8));
	printf("#define\tAHI_PUTW 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_put16));
	printf("#define\tAHI_PUTL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_put32));
	printf("#define\tAHI_PUTLL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_put64));
	printf("#define\tAHI_REP_GETB 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_get8));
	printf("#define\tAHI_REP_GETW 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_get16));
	printf("#define\tAHI_REP_GETL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_get32));
	printf("#define\tAHI_REP_GETLL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_get64));
	printf("#define\tAHI_REP_PUTB 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_put8));
	printf("#define\tAHI_REP_PUTW 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_put16));
	printf("#define\tAHI_REP_PUTL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_put32));
	printf("#define\tAHI_REP_PUTLL 0x%x\n",
		OFFSET(ddi_acc_impl_t, ahi_rep_put64));

	printf("#define\tM_WAITERS\t0x%x\n",
		OFFSET(struct adaptive_mutex, _m_waiters));
	printf("#define\tM_SPINLOCK\t0x%x\n",
		OFFSET(struct spin_mutex, m_spinlock));

	printf("#define\tRW_WRITER\t0x%x\n", RW_WRITER);
	printf("#define\tRW_READER\t0x%x\n", RW_READER);

	printf("#define\tNSYSCALL %d\n", NSYSCALL);
	printf("#define\tSYSENT_SIZE %d\n", sizeof (struct sysent));
	printf("#define\tSY_CALLC 0x%x\n", OFFSET(struct sysent, sy_callc));

	printf("#define\tSD_LOCK 0x%x\n",
	    OFFSET(struct stdata, sd_lock));
	printf("#define\tQ_FLAG 0x%x\n",
	    OFFSET(queue_t, q_flag));
	printf("#define\tQ_NEXT 0x%x\n",
	    OFFSET(queue_t, q_next));
	printf("#define\tQ_STREAM 0x%x\n",
	    OFFSET(queue_t, q_stream));
	printf("#define\tQ_SYNCQ 0x%x\n",
	    OFFSET(queue_t, q_syncq));
	printf("#define\tQ_QINFO 0x%x\n",
	    OFFSET(queue_t, q_qinfo));
	printf("#define\tQI_PUTP 0x%x\n",
	    OFFSET(struct qinit, qi_putp));
	printf("#define\tSQ_FLAGS 0x%x\n",
	    OFFSET(syncq_t, sq_flags));
	printf("#define\tSQ_COUNT 0x%x\n",
	    OFFSET(syncq_t, sq_count));
	printf("#define\tSQ_LOCK 0x%x\n",
	    OFFSET(syncq_t, sq_lock));
	printf("#define\tSQ_WAIT 0x%x\n",
	    OFFSET(syncq_t, sq_wait));
	printf("#define\tSQ_EXITWAIT 0x%x\n",
	    OFFSET(syncq_t, sq_exitwait));
	printf("#define\tSQ_SAVE 0x%x\n",
	    OFFSET(syncq_t, sq_save));

	printf("#define\tQHOT 0x%x\n", QHOT);
	printf("#define\tQNEXTHOT 0x%x\n", QNEXTHOT);

	printf("#define\tSQ_EXCL 0x%x\n", SQ_EXCL);
	printf("#define\tSQ_BLOCKED 0x%x\n", SQ_BLOCKED);
	printf("#define\tSQ_FROZEN 0x%x\n", SQ_FROZEN);
	printf("#define\tSQ_WRITER 0x%x\n", SQ_WRITER);
	printf("#define\tSQ_QUEUED 0x%x\n", SQ_QUEUED);
	printf("#define\tSQ_WANTWAKEUP 0x%x\n", SQ_WANTWAKEUP);
	printf("#define\tSQ_WANTEXWAKEUP 0x%x\n", SQ_WANTEXWAKEUP);
	printf("#define\tSQ_CIPUT 0x%x\n", SQ_CIPUT);
	printf("#define\tSQ_TYPEMASK 0x%x\n", SQ_TYPEMASK);
	printf("#define\tSQ_GOAWAY 0x%x\n", SQ_GOAWAY);
	printf("#define\tSQ_STAYAWAY 0x%x\n", SQ_STAYAWAY);

	printf("#define\tFKIOCTL\t0x%x\n", FKIOCTL);

	printf("#define\tNF_JMPBUF\t0x%x\n", &nfdata->jmpbuf);
	printf("#define\tNF_OP_TYPE 0x%x\n", &nfdata->op_type);
	printf("#define\tNF_SAVE_NOFAULT 0x%x\n", &nfdata->save_nofault);

/*
 * Gross hack... Although genassym is a user program and hence exit has one
 * parameter, it is compiled with the kernel headers and the _KERNEL define
 * so ANSI-C thinks it should have two!
 */
	exit(0, 0);
}

bit(mask)
	long mask;
{
	int i;

	for (i = 0; i < sizeof (int) * NBBY; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}

/*
 * Gross hack... Although genassym is a user program and hence exit has one
 * parameter, it is compiled with the kernel headers and the _KERNEL define
 * so ANSI-C thinks it should have two!
 */
	exit(1, 0);
}
