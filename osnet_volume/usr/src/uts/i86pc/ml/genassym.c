/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genassym.c	1.105	99/07/26 SMI"

#ifndef _GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1


/*
 * This user program uses the kernel header files and the kernel prototype
 * for "exit" isn't appropriate for programs linked against libc so exit
 * is mapped to kern_exit by the preprocessor and an appropriate exit
 * prototype is provided after the header files are included.
 */
#define	exit	kern_exit

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/elf_notes.h>
#include <sys/sysinfo.h>
#include <sys/vmmeter.h>
#include <sys/vmparam.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/lwp.h>
#include <sys/rt.h>
#include <sys/ts.h>
#include <sys/cpuvar.h>
#include <sys/dditypes.h>
#include <sys/vtrace.h>

#include <sys/tss.h>
#include <sys/trap.h>
#include <sys/stack.h>
#include <sys/psw.h>
#include <sys/segment.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/clock.h>

#include <sys/devops.h>
#include <sys/ddi_impldefs.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/hat.h>

#include <sys/avintr.h>
#include <sys/pic.h>
#include <sys/pit.h>
#include <sys/fp.h>

#include <sys/rm_platter.h>
#include <sys/x86_archext.h>

#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_subrdefs.h>
#include <sys/dditypes.h>
#include <sys/traptrace.h>

#include <vm/mach_page.h>

#undef	exit		/* unhide exit, see comment above */
extern void exit(int);

/* forward declarations */
int bit(long mask);
int sizeshift(size_t size);
int byteoffset(void *ptr, size_t len);
int bytevalue(void *ptr, size_t len);

#define	OFFSET(type, field)	((int)(&((type *)0)->field))

int
main(int argc, char *argv[])
{
	ddi_dma_impl_t	*hp = (ddi_dma_impl_t *)0;
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
	struct fpu *fp = (struct fpu *)0;
	struct autovec *av = (struct autovec *)0;
	struct av_head *avh = (struct av_head *)0;
	struct timestruc *tms = (struct timestruc *)0;
	struct tss386 *tss = (struct tss386 *)0;
	struct cpu *cpu = (struct cpu *)0;
	struct machcpu *mc = (struct machcpu *)0;
	struct standard_pic *sc = (struct standard_pic *)0;
	struct rm_platter *rmp = (struct rm_platter *)0;
	struct ctxop *ctxop = (struct ctxop *)0;
	struct machpage	mypp;
	struct ddi_acc_impl *acchdlp = (struct ddi_acc_impl *)0;
	ddi_nofault_data_t *nfdata = (ddi_nofault_data_t *)0;
	trap_trace_ctl_t *ttc = (trap_trace_ctl_t *)0;
	trap_trace_rec_t *rec = (trap_trace_rec_t *)0;

	printf("#define\tCPU_ARCH %d\n", CPU_ARCH);

	printf("#define\tI86_386_ARCH %d\n", I86_386_ARCH);
	printf("#define\tI86_486_ARCH %d\n", I86_486_ARCH);
	printf("#define\tI86_P5_ARCH %d\n",  I86_P5_ARCH);

	printf("#define\tREGS_GS 0x%x\n", &rp->r_gs);
	printf("#define\tREGS_FS 0x%x\n", &rp->r_fs);
	printf("#define\tREGS_ES 0x%x\n", &rp->r_es);
	printf("#define\tREGS_DS 0x%x\n", &rp->r_ds);
	printf("#define\tREGS_EDI 0x%x\n", &rp->r_edi);
	printf("#define\tREGS_ESI 0x%x\n", &rp->r_esi);
	printf("#define\tREGS_EBP 0x%x\n", &rp->r_ebp);
	printf("#define\tREGS_ESP 0x%x\n", &rp->r_esp);
	printf("#define\tREGS_EBX 0x%x\n", &rp->r_ebx);
	printf("#define\tREGS_EDX 0x%x\n", &rp->r_edx);
	printf("#define\tREGS_ECX 0x%x\n", &rp->r_ecx);
	printf("#define\tREGS_EAX 0x%x\n", &rp->r_eax);
	printf("#define\tREGS_TRAPNO 0x%x\n", &rp->r_trapno);
	printf("#define\tREGS_ERR 0x%x\n", &rp->r_err);
	printf("#define\tREGS_EIP 0x%x\n", &rp->r_eip);
	printf("#define\tREGS_PC 0x%x\n", &rp->r_pc); /* Alias for r_eip */
	printf("#define\tREGS_CS 0x%x\n", &rp->r_cs);
	printf("#define\tREGS_EFL 0x%x\n", &rp->r_efl);
	printf("#define\tREGS_UESP 0x%x\n", &rp->r_uesp);
	printf("#define\tREGS_SS 0x%x\n", &rp->r_ss);

	printf("#define\tT_AST 0x%x\n", T_AST);

	printf("#define\tLOCK_LEVEL 0x%x\n", LOCK_LEVEL);
	printf("#define\tCLOCK_LEVEL 0x%x\n", CLOCK_LEVEL);

	printf("#define\tPIC_NSEOI 0x%x\n", PIC_NSEOI);
	printf("#define\tPIC_SEOI_LVL7 0x%x\n", PIC_SEOI_LVL7);

	printf("#define\tNANOSEC 0x%x\n", NANOSEC);
	printf("#define\tADJ_SHIFT 0x%x\n", ADJ_SHIFT);

	printf("#define\tTSS_ESP0 0x%x\n", &tss->t_esp0);
	printf("#define\tTSS_SS0 0x%x\n", &tss->t_ss0);
	printf("#define\tTSS_LDT 0x%x\n", &tss->t_ldt);
	printf("#define\tTSS_CR3 0x%x\n", &tss->t_cr3);
	printf("#define\tTSS_CS 0x%x\n", &tss->t_cs);
	printf("#define\tTSS_SS 0x%x\n", &tss->t_ss);
	printf("#define\tTSS_DS 0x%x\n", &tss->t_ds);
	printf("#define\tTSS_ES 0x%x\n", &tss->t_es);
	printf("#define\tTSS_FS 0x%x\n", &tss->t_fs);
	printf("#define\tTSS_GS 0x%x\n", &tss->t_gs);
	printf("#define\tTSS_EBP 0x%x\n", &tss->t_ebp);
	printf("#define\tTSS_EIP 0x%x\n", &tss->t_eip);
	printf("#define\tTSS_EFL 0x%x\n", &tss->t_eflags);
	printf("#define\tTSS_ESP 0x%x\n", &tss->t_esp);
	printf("#define\tTSS_EAX 0x%x\n", &tss->t_eax);
	printf("#define\tTSS_EBX 0x%x\n", &tss->t_ebx);
	printf("#define\tTSS_ECX 0x%x\n", &tss->t_ecx);
	printf("#define\tTSS_EDX 0x%x\n", &tss->t_edx);
	printf("#define\tTSS_ESI 0x%x\n", &tss->t_esi);
	printf("#define\tTSS_EDI 0x%x\n", &tss->t_edi);
	printf("#define\tTSS_LDT 0x%x\n", &tss->t_ldt);
	printf("#define\tP_LINK 0x%x\n", &p->p_link);
	printf("#define\tP_NEXT 0x%x\n", &p->p_next);
	printf("#define\tP_CHILD 0x%x\n", &p->p_child);
	printf("#define\tP_SIBLING 0x%x\n", &p->p_sibling);
	printf("#define\tP_SIG 0x%x\n", &p->p_sig);
	printf("#define\tP_FLAG 0x%x\n", &p->p_flag);
	printf("#define\tP_TLIST 0x%x\n", &p->p_tlist);
	printf("#define\tP_AS 0x%x\n", &p->p_as);
	printf("#define\tP_FLAG 0x%x\n", &p->p_flag);
	printf("#define\tP_LOCKP 0x%x\n", &p->p_lockp);
	printf("#define\tP_USER 0x%x\n", &p->p_user);
	printf("#define\tP_LDT 0x%x\n", &p->p_ldt);
	printf("#define\tP_LDT_DESC 0x%x\n", &p->p_ldt_desc);
	printf("#define\tPROCSIZE 0x%x\n", sizeof (struct proc));
	printf("#define\tSLOAD 0x%x\n", SLOAD);
	printf("#define\tSSLEEP 0x%x\n", SSLEEP);
	printf("#define\tSRUN 0x%x\n", SRUN);
	printf("#define\tSONPROC 0x%x\n", SONPROC);

	printf("#define\tT_PC 0x%x\n", &tid->t_pc);
	printf("#define\tT_SP 0x%x\n", &tid->t_sp);
	printf("#define\tT_LOCK 0x%x\n", &tid->t_lock);
	printf("#define\tT_LOCKSTAT 0x%x\n", &tid->t_lockstat);
	printf("#define\tT_LOCKP 0x%x\n", &tid->t_lockp);
	printf("#define\tT_LOCK_FLUSH 0x%x\n", &tid->t_lock_flush);
	printf("#define\tT_KPRI_REQ 0x%x\n", &tid->t_kpri_req);
	printf("#define\tT_OLDSPL 0x%x\n", &tid->t_oldspl);
	printf("#define\tT_PRI 0x%x\n", &tid->t_pri);
	printf("#define\tT_PIL 0x%x\n", &tid->t_pil);
	printf("#define\tT_LWP 0x%x\n", &tid->t_lwp);
	printf("#define\tT_PROCP 0x%x\n", &tid->t_procp);
	printf("#define\tT_LINK 0x%x\n", &tid->t_link);
	printf("#define\tT_STATE 0x%x\n", &tid->t_state);
	printf("#define\tT_MSTATE 0x%x\n", &tid->t_mstate);
	printf("#define\tT_PREEMPT_LK 0x%x\n", &tid->t_preempt_lk);
	printf("#define\tT_STACK 0x%x\n", &tid->t_stk);
	printf("#define\tT_SWAP 0x%x\n", &tid->t_swap);
	printf("#define\tT_WCHAN 0x%x\n", &tid->t_wchan);
	printf("#define\tT_FLAGS 0x%x\n", &tid->t_flag);
	printf("#define\tT_CTX 0x%x\n", &tid->t_ctx);
	printf("#define\tT_LOFAULT 0x%x\n", &tid->t_lofault);
	printf("#define\tT_ONFAULT 0x%x\n", &tid->t_onfault);
	printf("#define\tT_NOFAULT 0x%x\n", &tid->t_nofault);
	printf("#define\tT_CPU 0x%x\n", &tid->t_cpu);
	printf("#define\tT_BOUND_CPU 0x%x\n", &tid->t_bound_cpu);
	printf("#define\tT_INTR 0x%x\n", &tid->t_intr);
	printf("#define\tT_FORW 0x%x\n", &tid->t_forw);
	printf("#define\tT_BACK 0x%x\n", &tid->t_back);
	printf("#define\tT_SIG 0x%x\n", &tid->t_sig);
	printf("#define\tT_TID 0x%x\n", &tid->t_tid);
	printf("#define\tT_PRE_SYS 0x%x\n", &tid->t_pre_sys);
	printf("#define\tT_PREEMPT 0x%x\n", &tid->t_preempt);
	printf("#define\tT_PROC_FLAG 0x%x\n", &tid->t_proc_flag);
	printf("#define\tT_MMUCTX 0x%x\n", &tid->t_mmuctx);
	printf("#define\tT_STARTPC 0x%x\n", &tid->t_startpc);
	printf("#define\tT_ASTFLAG 0x%x\n", &tid->t_astflag);
	printf("#define\tT_POST_SYS_AST 0x%x\n", &tid->t_post_sys_ast);
	printf("#define\tT_SYSNUM 0x%x\n", &tid->t_sysnum);
	printf("#define\tT_INTR_THREAD %d\n", T_INTR_THREAD);
	printf("#define\tCTXOP_SAVE %d\n", &ctxop->save_op);
	printf("#define\tFREE_THREAD 0x%x\n", TS_FREE);
	printf("#define\tTS_FREE 0x%x\n", TS_FREE);
	printf("#define\tTS_ZOMB 0x%x\n", TS_ZOMB);
	printf("#define\tTP_MSACCT 0x%x\n", TP_MSACCT);
	printf("#define\tTP_WATCHPT 0x%x\n", TP_WATCHPT);
	printf("#define\tONPROC_THREAD 0x%x\n", TS_ONPROC);
	printf("#define\tT0STKSZ 0x%x\n", DEFAULTSTKSZ);
	printf("#define\tTHREAD_SIZE %d\n", sizeof (kthread_t));

	printf("#define\tA_HAT 0x%x\n", &as->a_hat);

	printf("#define\tS_READ 0x%x\n", (int)S_READ);
	printf("#define\tS_WRITE 0x%x\n", (int)S_WRITE);
	printf("#define\tS_EXEC 0x%x\n", (int)S_EXEC);
	printf("#define\tS_OTHER 0x%x\n", (int)S_OTHER);

	printf("#define\tU_COMM 0x%x\n", up->u_comm);
	printf("#define\tU_SIGNAL 0x%x\n", up->u_signal);
	printf("#define\tUSIZEBYTES 0x%x\n", sizeof (struct user));

	printf("#define\tNORMALRETURN 0x%x\n", (int)NORMALRETURN);
	printf("#define\tLWP_THREAD 0x%x\n", &lwpid->lwp_thread);
	printf("#define\tLWP_PROCP 0x%x\n", &lwpid->lwp_procp);
	printf("#define\tLWP_EOSYS 0x%x\n", &lwpid->lwp_eosys);
	printf("#define\tLWP_REGS 0x%x\n", &lwpid->lwp_regs);
	printf("#define\tLWP_ARG 0x%x\n", lwpid->lwp_arg);
	printf("#define\tLWP_AP 0x%x\n", &lwpid->lwp_ap);
	printf("#define\tLWP_CURSIG 0x%x\n", &lwpid->lwp_cursig);
	printf("#define\tLWP_STATE 0x%x\n", &lwpid->lwp_state);
	printf("#define\tLWP_USER 0x%x\n", LWP_USER);
	printf("#define\tLWP_UTIME 0x%x\n", &lwpid->lwp_utime);
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
	printf("#define\tLWP_PCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tLWP_PCB_DREGS 0x%x\n", &lwpid->lwp_pcb.pcb_dregs);
	printf("#define\tLWP_RU_SYSC 0x%x\n", &lwpid->lwp_ru.sysc);
	printf("#define\tLWP_FPU_REGS 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_regs);
	printf("#define\tLWP_FPU_FLAGS 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_flags);
	printf("#define\tLWP_FPU_CHIP_STATE 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_regs.fp_reg_set.fpchip_state);
	printf("#define\tLWP_PCB_FPU 0x%x\n", &lwpid->lwp_pcb.pcb_fpu);
	printf("#define\tPCB_FPU_REGS 0x%x\n",
	    (int)&lwpid->lwp_pcb.pcb_fpu.fpu_regs -
	    (int)&lwpid->lwp_pcb);
	printf("#define\tPCB_FPU_FLAGS 0x%x\n",
	    (int)&lwpid->lwp_pcb.pcb_fpu.fpu_flags -
	    (int)&lwpid->lwp_pcb);
	printf("#define\tLMS_USER 0x%x\n", LMS_USER);
	printf("#define\tLMS_SYSTEM 0x%x\n", LMS_SYSTEM);

	printf("#define\tFP_487 %d\n", FP_487);
	printf("#define\tFP_486 %d\n", FP_486);
	printf("#define\tFPU_CW_INIT 0x%x\n", FPU_CW_INIT);
	printf("#define\tFPU_EN 0x%x\n", FPU_EN);
	printf("#define\tFPU_VALID 0x%x\n", FPU_VALID);

	printf("#define\tPCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tPCBSIZE 0x%x\n", sizeof (struct pcb));

	printf("#define\tREGSIZE %d\n", sizeof (struct regs));

	printf("#define\tELF_NOTE_SOLARIS \"%s\"\n", ELF_NOTE_SOLARIS);
	printf("#define\tELF_NOTE_PAGESIZE_HINT %d\n", ELF_NOTE_PAGESIZE_HINT);

	printf("#define\tIDTSZ %d\n", IDTSZ);
	printf("#define\tGDTSZ %d\n", GDTSZ);
	printf("#define\tMINLDTSZ %d\n", MINLDTSZ);

	printf("#define\tFP_NO %d\n", FP_NO);
	printf("#define\tFP_SW %d\n", FP_SW);
	printf("#define\tFP_HW %d\n", FP_HW);
	printf("#define\tFP_287 %d\n", FP_287);
	printf("#define\tFP_387 %d\n", FP_387);

	printf("#define\tAV_VECTOR 0x%x\n", &av->av_vector);
	printf("#define\tAV_INTARG 0x%x\n", &av->av_intarg);
	printf("#define\tAV_INT_SPURIOUS %d\n", AV_INT_SPURIOUS);
	printf("#define\tAUTOVECSIZE 0x%x\n", sizeof (struct autovec));
	printf("#define\tAV_LINK 0x%x\n", &av->av_link);
	printf("#define\tAV_PRILEVEL 0x%x\n", &av->av_prilevel);

	printf("#define\tAVH_LINK 0x%x\n", &avh->avh_link);
	printf("#define\tAVH_HI_PRI 0x%x\n", &avh->avh_hi_pri);
	printf("#define\tAVH_LO_PRI 0x%x\n", &avh->avh_lo_pri);

	printf("#define\tCPU_ID 0x%x\n", OFFSET(struct cpu, cpu_id));
	printf("#define\tCPU_FLAGS 0x%x\n", OFFSET(struct cpu, cpu_flags));
	printf("#define\tCPU_READY %d\n", CPU_READY);
	printf("#define\tCPU_QUIESCED %d\n", CPU_QUIESCED);
	printf("#define\tCPU_THREAD 0x%x\n", OFFSET(struct cpu, cpu_thread));
	printf("#define\tCPU_THREAD_LOCK 0x%x\n",
	    OFFSET(struct cpu, cpu_thread_lock));
	printf("#define\tCPU_KPRUNRUN 0x%x\n",
	    OFFSET(struct cpu, cpu_kprunrun));
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
	printf("#define\tCPU_SYSINFO_SYSCALL 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.syscall));
	printf("#define\tCPU_SYSINFO_SUO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_suo_cnt));

	printf("#define\tCPU_PROFILE_PC 0x%x\n",
	    OFFSET(struct cpu, cpu_profile_pc));
	printf("#define\tCPU_PROFILE_PIL 0x%x\n",
	    OFFSET(struct cpu, cpu_profile_pil));

	printf("#define\tCPU_CURRENT_HAT 0x%x\n",
	    OFFSET(struct cpu, cpu_current_hat));
	printf("#define\tCPU_CR3 0x%x\n", OFFSET(struct cpu, cpu_cr3));
	printf("#define\tCPU_GDT 0x%x\n", OFFSET(struct cpu, cpu_gdt));
	printf("#define\tCPU_IDT 0x%x\n", OFFSET(struct cpu, cpu_idt));
	printf("#define\tCPU_TSS 0x%x\n", OFFSET(struct cpu, cpu_tss));
	printf("#define\tCPU_PRI 0x%x\n", &mc->mcpu_pri);
	printf("#define\tCPU_PRI_DATA 0x%x\n", &mc->mcpu_pri_data);
	printf("#define\tCPU_INTR_STACK 0x%x\n", &cpu->cpu_intr_stack);
	printf("#define\tCPU_ON_INTR 0x%x\n", &cpu->cpu_on_intr);
	printf("#define\tCPU_INTR_THREAD 0x%x\n", &cpu->cpu_intr_thread);
	printf("#define\tCPU_INTR_ACTV 0x%x\n", &cpu->cpu_intr_actv);
	printf("#define\tCPU_BASE_SPL 0x%x\n", &cpu->cpu_base_spl);
	printf("#define\tCPU_FTRACE_STATE 0x%x\n",
	    OFFSET(struct cpu, cpu_ftrace.ftd_state));
	printf("#define\tCPU_SOFTINFO 0x%x\n", &cpu->cpu_softinfo);
	printf("#define\tCPU_M 0x%x\n", &cpu->cpu_m);
	printf("#define\tC_CURMASK 0x%x\n", &sc->c_curmask);
	printf("#define\tC_IPLMASK 0x%x\n", &sc->c_iplmask);
	printf("#define\tMCMD_PORT %d\n", MCMD_PORT);
	printf("#define\tSCMD_PORT %d\n", SCMD_PORT);
	printf("#define\tMIMR_PORT %d\n", MIMR_PORT);
	printf("#define\tSIMR_PORT %d\n", SIMR_PORT);

	printf("#define\tDMP_NOSYNC 0x%x\n", DMP_NOSYNC);
	printf("#define\tDMAI_RFLAGS 0x%x\n", &hp->dmai_rflags);
	printf("#define\tDMAI_RDIP 0x%x\n", &hp->dmai_rdip);
	printf("#define\tDEVI_DEV_OPS 0x%x\n", &devi->devi_ops);
	printf("#define\tDEVI_BUS_CTL 0x%x\n", &devi->devi_bus_ctl);
	printf("#define\tDEVI_BUS_DMA_MAP 0x%x\n", &devi->devi_bus_dma_map);
	printf("#define\tDEVI_BUS_DMA_CTL 0x%x\n", &devi->devi_bus_dma_ctl);
	printf("#define\tDEVI_BUS_DMA_ALLOCHDL 0x%x\n",
	    &devi->devi_bus_dma_allochdl);
	printf("#define\tDEVI_BUS_DMA_FREEHDL 0x%x\n",
	    &devi->devi_bus_dma_freehdl);
	printf("#define\tDEVI_BUS_DMA_BINDHDL 0x%x\n",
	    &devi->devi_bus_dma_bindhdl);
	printf("#define\tDEVI_BUS_DMA_UNBINDHDL 0x%x\n",
	    &devi->devi_bus_dma_unbindhdl);
	printf("#define\tDEVI_BUS_DMA_FLUSH 0x%x\n", &devi->devi_bus_dma_flush);
	printf("#define\tDEVI_BUS_DMA_WIN 0x%x\n", &devi->devi_bus_dma_win);

	printf("#define\tDEVI_BUS_OPS 0x%x\n", &ops->devo_bus_ops);
	printf("#define\tOPS_CTL 0x%x\n", &busops->bus_ctl);
	printf("#define\tOPS_MAP 0x%x\n", &busops->bus_dma_map);
	printf("#define\tOPS_MCTL 0x%x\n", &busops->bus_dma_ctl);
	printf("#define\tOPS_ALLOCHDL 0x%x\n", &busops->bus_dma_allochdl);
	printf("#define\tOPS_FREEHDL 0x%x\n", &busops->bus_dma_freehdl);
	printf("#define\tOPS_BINDHDL 0x%x\n", &busops->bus_dma_bindhdl);
	printf("#define\tOPS_UNBINDHDL 0x%x\n", &busops->bus_dma_unbindhdl);
	printf("#define\tOPS_FLUSH 0x%x\n", &busops->bus_dma_flush);
	printf("#define\tOPS_WIN 0x%x\n", &busops->bus_dma_win);

	printf("#define\tRW_WRITER\t0x%x\n", RW_WRITER);
	printf("#define\tRW_READER\t0x%x\n", RW_READER);

	printf("#define\tNSYSCALL %d\n", NSYSCALL);
	printf("#define\tSYSENT_SIZE %d\n", sizeof (struct sysent));
	printf("#define\tSY_CALLC 0x%x\n", OFFSET(struct sysent, sy_callc));
	printf("#define\tSY_NARG 0x%x\n", OFFSET(struct sysent, sy_narg));

	printf("#define\tMAXSYSARGS\t%d\n", MAXSYSARGS);

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
	printf("#define\tSQ_SAVE 0x%x\n",
	    OFFSET(syncq_t, sq_save));

	/* Hack value just to allow clock to be kicked */
	printf("#define\tNSEC_PER_CLOCK_TICK 0x%x\n", NANOSEC / 100);

	printf("#define\tNSEC_PER_COUNTER_TICK 0x%x\n", NANOSEC / PIT_HZ);

	printf("#define\tPITCTR0_PORT 0x%x\n", PITCTR0_PORT);
	printf("#define\tPITCTL_PORT 0x%x\n", PITCTL_PORT);
	printf("#define\tPIT_COUNTDOWN 0x%x\n",
				PIT_C0 | PIT_LOADMODE |	PIT_NDIVMODE);

	printf("#define\tNBPW %d\n", NBPW);

	printf("#define\tIDTROFF 0x%x\n", &rmp->rm_idt_lim);
	printf("#define\tGDTROFF 0x%x\n", &rmp->rm_gdt_lim);
	printf("#define\tCR3OFF 0x%x\n", &rmp->rm_pdbr);
	printf("#define\tCPUNOFF 0x%x\n", &rmp->rm_cpu);
	printf("#define\tCR4OFF 0x%x\n", &rmp->rm_cr4);
	printf("#define\tX86FEATURE 0x%x\n", &rmp->rm_x86feature);

	printf("#define\tDDI_ACCATTR_IO_SPACE 0x%x\n", DDI_ACCATTR_IO_SPACE);
	printf("#define\tDDI_ACCATTR_DIRECT 0x%x\n", DDI_ACCATTR_DIRECT);
	printf("#define\tDDI_ACCATTR_CPU_VADDR 0x%x\n", DDI_ACCATTR_CPU_VADDR);
	printf("#define\tACC_ATTR 0x%x\n", &acchdlp->ahi_acc_attr);
	printf("#define\tACC_GETB 0x%x\n", &acchdlp->ahi_get8);
	printf("#define\tACC_GETW 0x%x\n", &acchdlp->ahi_get16);
	printf("#define\tACC_GETL 0x%x\n", &acchdlp->ahi_get32);
	printf("#define\tACC_GETLL 0x%x\n", &acchdlp->ahi_get64);
	printf("#define\tACC_PUTB 0x%x\n", &acchdlp->ahi_put8);
	printf("#define\tACC_PUTW 0x%x\n", &acchdlp->ahi_put16);
	printf("#define\tACC_PUTL 0x%x\n", &acchdlp->ahi_put32);
	printf("#define\tACC_PUTLL 0x%x\n", &acchdlp->ahi_put64);
	printf("#define\tACC_REP_GETB 0x%x\n", &acchdlp->ahi_rep_get8);
	printf("#define\tACC_REP_GETW 0x%x\n", &acchdlp->ahi_rep_get16);
	printf("#define\tACC_REP_GETL 0x%x\n", &acchdlp->ahi_rep_get32);
	printf("#define\tACC_REP_GETLL 0x%x\n", &acchdlp->ahi_rep_get64);
	printf("#define\tACC_REP_PUTB 0x%x\n", &acchdlp->ahi_rep_put8);
	printf("#define\tACC_REP_PUTW 0x%x\n", &acchdlp->ahi_rep_put16);
	printf("#define\tACC_REP_PUTL 0x%x\n", &acchdlp->ahi_rep_put32);
	printf("#define\tACC_REP_PUTLL 0x%x\n", &acchdlp->ahi_rep_put64);
	printf("#define\tDDI_DEV_AUTOINCR 0x%x\n", DDI_DEV_AUTOINCR);

	printf("#define\tX86_P5 0x%x\n", X86_P5);
	printf("#define\tX86_K5 0x%x\n", X86_K5);
	printf("#define\tX86_P6 0x%x\n", X86_P6);

	printf("#define\tK5_PSE 0x%x\n", K5_PSE);
	printf("#define\tK5_GPE 0x%x\n", K5_GPE);
	printf("#define\tK5_PGE 0x%x\n", K5_PGE);
	printf("#define\tP5_PSE_SUPPORTED 0x%x\n", P5_PSE_SUPPORTED);
	printf("#define\tP5_TSC_SUPPORTED 0x%x\n", P5_TSC_SUPPORTED);
	printf("#define\tP5_MSR_SUPPORTED 0x%x\n", P5_MSR_SUPPORTED);
	printf("#define\tP6_APIC_SUPPORTED 0x%x\n", P6_APIC_SUPPORTED);
	printf("#define\tP6_MTRR_SUPPORTED 0x%x\n", P6_MTRR_SUPPORTED);
	printf("#define\tP6_PGE_SUPPORTED 0x%x\n", P6_PGE_SUPPORTED);
	printf("#define\tP6_CMOV_SUPPORTED 0x%x\n", P6_CMOV_SUPPORTED);
	printf("#define\tP6_MCA_SUPPORTED 0x%x\n", P6_MCA_SUPPORTED);
	printf("#define\tP6_MCE_SUPPORTED 0x%x\n", P6_MCE_SUPPORTED);
	printf("#define\tP6_PAE_SUPPORTED 0x%x\n", P6_PAE_SUPPORTED);
	printf("#define\tP6_CXS_SUPPORTED 0x%x\n", P6_CXS_SUPPORTED);
	printf("#define\tP6_PAT_SUPPORTED 0x%x\n", P6_PAT_SUPPORTED);
	printf("#define\tP5_MMX_SUPPORTED 0x%x\n", P5_MMX_SUPPORTED);
	printf("#define\tK5_PGE_SUPPORTED 0x%x\n", K5_PGE_SUPPORTED);
	printf("#define\tK5_SCE_SUPPORTED 0x%x\n", K5_SCE_SUPPORTED);


	printf("#define\tX86_LARGEPAGE 0x%x\n", X86_LARGEPAGE);
	printf("#define\tX86_TSC 0x%x\n", X86_TSC);
	printf("#define\tX86_MSR 0x%x\n", X86_MSR);
	printf("#define\tX86_MTRR 0x%x\n", X86_MTRR);
	printf("#define\tX86_PGE 0x%x\n", X86_PGE);
	printf("#define\tX86_APIC 0x%x\n", X86_APIC);
	printf("#define\tX86_CMOV 0x%x\n", X86_CMOV);
	printf("#define\tX86_MMX 0x%x\n", X86_MMX);
	printf("#define\tX86_MCA 0x%x\n", X86_MCA);
	printf("#define\tX86_PAE 0x%x\n", X86_PAE);
	printf("#define\tX86_CXS 0x%x\n", X86_CXS);
	printf("#define\tX86_PAT 0x%x\n", X86_PAT);
	printf("#define\tX86_CPUID 0x%x\n", X86_CPUID);
	printf("#define\tX86_INTEL 0x%x\n", X86_INTEL);
	printf("#define\tX86_AMD 0x%x\n", X86_AMD);

	printf("#define\tMMU_STD_PAGESHIFT 0x%x\n", (uint_t)MMU_STD_PAGESHIFT);
	printf("#define\tMMU_STD_PAGEMASK 0x%x\n", (uint_t)MMU_STD_PAGEMASK);
	printf("#define\tMMU_L2_MASK 0x%x\n", (uint_t)(NPTEPERPT - 1));
	printf("#define\tMMU_PAGEOFFSET 0x%x\n", (uint_t)(MMU_PAGESIZE - 1));
	printf("#define\tNPTESHIFT 0x%x\n", (uint_t)NPTESHIFT);
	printf("#define\tFOURMB_PAGEOFFSET 0x%x\n", (uint_t)FOURMB_PAGEOFFSET);
	printf("#define\tFOURMB_PAGESIZE 0x%x\n", (uint_t)FOURMB_PAGESIZE);
	printf("#define\tPTE_LARGEPAGE 0x%x\n", (uint_t)PTE_LARGEPAGE);
	printf("#define\tPTE_VALID 0x%x\n", (uint_t)PTE_VALID);
	printf("#define\tPTE_VALID 0x%x\n", (uint_t)PTE_VALID);
	printf("#define\tPTE_SRWX 0x%x\n", (uint_t)PTE_SRWX);

	printf("#define\tNF_JMPBUF 0x%x\n", &nfdata->jmpbuf);
	printf("#define\tNF_OP_TYPE 0x%x\n", &nfdata->op_type);
	printf("#define\tNF_SAVE_NOFAULT 0x%x\n", &nfdata->save_nofault);

	printf("#define\tTRAPTR_NEXT 0x%x\n", &ttc->ttc_next);
	printf("#define\tTRAPTR_FIRST 0x%x\n", &ttc->ttc_first);
	printf("#define\tTRAPTR_LIMIT 0x%x\n", &ttc->ttc_limit);

	printf("#define\tTRAPTR_SIZE_SHIFT %d\n",
	    sizeshift(sizeof (trap_trace_ctl_t)));
	printf("#define\tTRAPTR_SIZE %d\n",
	    1 << sizeshift(sizeof (trap_trace_ctl_t)));

	printf("#define\tTRAP_ENT_SIZE %d\n", sizeof (trap_trace_rec_t));
	printf("#define\tTRAP_TSIZE %d\n",
	    sizeof (trap_trace_rec_t) * TRAPTR_NENT);

	printf("#define\tTTR_VECTOR %d\n", &rec->ttr_vector);
	printf("#define\tTTR_IPL %d\n", &rec->ttr_ipl);
	printf("#define\tTTR_SPL %d\n", &rec->ttr_spl);
	printf("#define\tTTR_PRI %d\n", &rec->ttr_pri);
	printf("#define\tTTR_SYSNUM %d\n", &rec->ttr_sysnum);
	printf("#define\tTTR_MARKER %d\n", &rec->ttr_marker);
	printf("#define\tTTR_STAMP %d\n", &rec->ttr_stamp);

	printf("#define\tNCPU %d\n", NCPU);

	exit(0);
}

int
bit(long mask)
{
	int i;

	for (i = 0; i < sizeof (int) * NBBY; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}

	exit(1);
}

/*
 * Given a size of a structure, returns the shift required to address
 * into an array.  This is different from highbit() because it will round
 * up (that is, a size of 16 returns 4, but a size of 17 will return 5).
 */
int
sizeshift(size_t size)
{
	int i, shift = -1;

	for (i = 0; i < sizeof (size_t) * NBBY; i++) {
		if (size & 1)
			shift = shift == -1 ? i : i + 1;
		size >>= 1;
	}
	return (shift);
}

/*
 * This function returns the byte offset of the first non zero byte in the
 * block of length 'len' starting at ptr
 */
int
byteoffset(void *ptr, size_t len)
{
	size_t	i;
	char	*p = ptr;

	for (i = 0; i < len; i++, p++)
		if (*p)
			return (i);
	return (0);
}

/*
 * This function returns the value of the first non zero byte in the
 * block of length 'len' starting at 'ptr'
 */
int
bytevalue(void *ptr, size_t len)
{
	size_t	i;
	char	*p = ptr;

	for (i = 0; i < len; i++, p++)
		if (*p)
			return (*p);
	return (0);
}

void
bzero(void *ptr_arg, size_t len)
{
	char	*ptr = ptr_arg;
	int	i;

	for (i = 0; i < len; i++, ptr++)
		*ptr = 0;
}
