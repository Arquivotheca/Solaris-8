/*
 * Copyright (c) 1993, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)emul_init.c	1.10	99/05/04 SMI"


/*
 * Initialization code for emul_80387 float point emulation loadable module.
 */

#include <sys/types.h>
#include <sys/trap.h>
#include <sys/reg.h>
#include <sys/fp.h>
#include <sys/mman.h>
#include <sys/modctl.h>
#include <vm/as.h>
#include <sys/mmu.h>
#include <sys/cpuvar.h>

struct fpchip_state fpinit_result;
int e80387_saved_pfdest1;

extern struct mod_ops mod_miscops;
extern struct as kas;
extern void (*(fasttable[]))();
extern struct gate_desc idt[];

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "fpemul"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

static int emul_init();

extern void bcopy();
extern void fpinit();
extern void e80387();
extern void e80387_finish();
extern void e80387_getfp();
extern void e80387_setfp();
extern void e80387_start();
extern void e80387_pgflt1();

/* Routines expected by modload */
int
_init(void)
{
	int status;

	status = emul_init();
	if (status == 0) {
		status = mod_install(&modlinkage);
	}
	return (status);
}

int
_fini(void)
{
	/* Reject attempts to unload this module */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* End of modload glue */

/*
 * Emulator initialization module.  Return non-zero for failure.
 * Set fp_kind to FP_SW for success and return 0.
 */
static int
emul_init()
{
	register uint_t start, finish;
	union {
		struct seg_desc sd;
		struct dscr d;
		struct gate_desc gd;
		struct gdscr g;
		ushort_t s[4];
		uchar_t  c[8];
	} du;
	register struct fpu_ctx *fp;
	extern void hat_chgattr_pagedir();

	/*
	 * There should really be a test here to fail the emulator
	 * initialization if the system has multiple CPUs.  At present
	 * we do not support CPUs without FPUs in MP systems so no
	 * test is required.
	 */

	/*
	 * Save page fault interrupt destination from idt and
	 * verify that gate is a kernel interrupt gate.
	 */
	du.gd = idt[T_PGFLT];
	e80387_saved_pfdest1 = du.s[0] | (du.s[3] << 16);
	if (du.c[5] != (GATE_KACC | GATE_386INT))
		return (1);

	/*
	 * Change pages containing emulator code and table to be
	 * user readable.  Also change page directory
	 * entry (or entries) to allow user access.  Algorithm allows
	 * for emulator code to cross one 4-Meg VA boundary but not
	 * more than one.
	 * we will use a new call hat_chgattr_pagedir() to change protection
	 * attributes of the page directory. This call will change the
	 * attributes only on the current CPU. The attributes in
	 * kernel_only_pagedir are not changed.
	 * On an MP system, we need to  propagate the permissions  to
	 * other cpus.
	 */
	start = (int)e80387_start & ~(MMU_PAGESIZE-1);
	finish = ((int)e80387_finish | (MMU_PAGESIZE-1)) + 1;
	rw_enter(&kas.a_lock, RW_READER);
	hat_chgattr((struct hat *)kas.a_hat, (caddr_t)start, finish - start,
		PROT_READ | PROT_USER);
	hat_chgattr_pagedir((struct hat *)kas.a_hat, (caddr_t)start,
		(size_t)(finish - start), PROT_READ | PROT_USER | PROT_WRITE);
	rw_exit(&kas.a_lock);

	/*
	 * Fill in emulator code segment descriptor in GDT.  It is a
	 * copy of the standard kernel code segment descriptor with
	 * the conforming code bit turned on.
	 */
	du.sd = CPU->cpu_gdt[seltoi(KCSSEL)];
	du.d.a_acc0007 |= SEG_CONFORM;
	CPU->cpu_gdt[seltoi(FPESEL)] = du.sd;

	/*
	 * Change coprocessor not present gate to address emulator
	 * and make sure it is a trap gate not an interrupt gate.
	 */
	du.gd = idt[T_NOEXTFLT];
	du.s[0] = (int)e80387 & 0xFFFF;
	du.s[1] = FPESEL;
	du.s[3] = (int)e80387 >> 16;
	du.c[5] = (GATE_UACC | GATE_386TRP);
	idt[T_NOEXTFLT] = du.gd;

	/*
	 * Change page fault interrupt gate to point to emulator page fault
	 * handler preamble.
	 */
	du.gd = idt[T_PGFLT];
	du.s[0] = (int)e80387_pgflt1 & 0xFFFF;
	du.s[3] = (int)e80387_pgflt1 >> 16;
	idt[T_PGFLT] = du.gd;

	/*
	 * Change floating point emulator error gate to be user-level
	 * interrupt gate.
	 */
	du.gd = idt[T_EXTERRFLT];
	du.c[5] = GATE_UACC | GATE_386INT;
	idt[T_EXTERRFLT] = du.gd;

	/*
	 * Change fast system call table entries to point at real versions
	 * of emulator fast system call routines.
	 */
	fasttable[T_FGETFP] = e80387_getfp;
	fasttable[T_FSETFP] = e80387_setfp;

	/*
	 * Everything is set up properly.  Say that FP is emulated. */
	fp_kind = FP_SW;

	/*
	 * Do an fpinit and save the result.  We will copy the saved
	 * result instead of emulating fpinit on the first emulated FP
	 * instruction of every process.
	 */
	fp = &ttolwp(curthread)->lwp_pcb.pcb_fpu;
	fp->fpu_flags = FPU_EN | FPU_VALID;
	fpinit();
	fpinit_result = fp->fpu_regs.fp_reg_set.fpchip_state;
	fp->fpu_flags = FPU_INVALID;

	return (0);
}
