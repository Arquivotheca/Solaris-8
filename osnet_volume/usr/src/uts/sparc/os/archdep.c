/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)archdep.c	1.60	99/11/08 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/auxv.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/elf_SPARC.h>
#include <sys/cmn_err.h>
#include <sys/spl.h>
#include <sys/privregs.h>
#include <sys/kobj.h>
#include <sys/modctl.h>
#include <sys/reboot.h>
#include <sys/time.h>

extern struct bootops *bootops;

/*
 * Workaround for broken FDDI driver (remove when 4289172 is fixed)
 */
short cputype = 0x80;

/*
 * Advertised via /etc/system
 */
int enable_mixed_bcp = 1;

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(uintptr_t *pcstack, int pcstack_limit)
{
	struct frame *fp = (struct frame *)((caddr_t)getfp() + STACK_BIAS);
	struct frame *nextfp, *minfp, *stacktop;
	int depth = 0;
	int on_intr;

	flush_windows();

	if ((on_intr = CPU->cpu_on_intr) != 0)
		stacktop = (struct frame *)CPU->cpu_intr_stack + SA(MINFRAME);
	else
		stacktop = (struct frame *)curthread->t_stk;
	minfp = fp;

	while (depth < pcstack_limit) {
		nextfp = (struct frame *)((caddr_t)fp->fr_savfp + STACK_BIAS);
		if (nextfp <= minfp || nextfp >= stacktop) {
			if (on_intr) {
				/*
				 * Hop from interrupt stack to thread stack.
				 */
				stacktop = (struct frame *)curthread->t_stk;
				minfp = (struct frame *)curthread->t_stkbase;
				on_intr = 0;
				continue;
			}
			break;
		}
		pcstack[depth++] = fp->fr_savpc;
		fp = nextfp;
		minfp = fp;
	}
	return (depth);
}

/*
 * The following ELF header fields are defined as processor-specific
 * in the SPARC V8 ABI:
 *
 *	e_ident[EI_DATA]	encoding of the processor-specific
 *				data in the object file
 *	e_machine		processor identification
 *	e_flags			processor-specific flags associated
 *				with the file
 */

/*
 * The value of at_flags reflects a platform's cpu module support.
 * at_flags is used to check for allowing a binary to execute and
 * is passed as the value of the AT_FLAGS auxiliary vector.
 */
int at_flags = 0;

/*
 * Check the processor-specific fields of an ELF header.
 *
 * returns 1 if the fields are valid, 0 otherwise
 */
int
elfheadcheck(
	unsigned char e_data,
	Elf32_Half e_machine,
	Elf32_Word e_flags)
{
	if (e_data != ELFDATA2MSB)
		return (0);

	switch (e_machine) {
	case EM_SPARC:
		if (e_flags == 0)
			return (1);
		else
			return (0);
	case EM_SPARCV9:
		if (e_flags & EF_SPARC_EXT_MASK) {
			if (e_flags == EF_SPARC_SUN_US1)
				return (1);
			else
				return (0);
		}
		return (1);
	case EM_SPARC32PLUS:
		if ((e_flags & EF_SPARC_32PLUS) != 0 &&
		    ((e_flags & ~at_flags) & EF_SPARC_32PLUS_MASK) == 0)
			return (1);
		else
			return (0);
	default:
		return (0);
	}
}

int auxv_hwcap_mask = 0;	/* user: patch for broken cpus, debugging */
int kauxv_hwcap_mask = 0;	/* kernel: patch for broken cpus, debugging */

/*
 * Gather information about the processor
 * Determine if the hardware supports mul/div instructions
 * Determine whether we'll use 'em in the kernel or in userland.
 */
void
bind_hwcap(void)
{
	auxv_hwcap = get_hwcap_flags(0) & ~auxv_hwcap_mask;
	kauxv_hwcap = get_hwcap_flags(1) & ~kauxv_hwcap_mask;

#ifndef	__sparcv9cpu
	/*
	 * Conditionally switch the kernels .umul, .div etc. to use
	 * the whizzy instructions.  The processor better be able
	 * to handle them!
	 */
	if (kauxv_hwcap)
		kern_use_hwinstr(kauxv_hwcap & AV_SPARC_HWMUL_32x32,
		    kauxv_hwcap & AV_SPARC_HWDIV_32x32);
#ifdef DEBUG
	/* XXX	Take this away! */
	cmn_err(CE_CONT, "?kernel: %sware multiply, %sware divide\n",
	    kauxv_hwcap & AV_SPARC_HWMUL_32x32 ? "hard" : "soft",
	    kauxv_hwcap & AV_SPARC_HWDIV_32x32 ? "hard" : "soft");
	cmn_err(CE_CONT, "?  user: %sware multiply, %sware divide\n",
	    auxv_hwcap & AV_SPARC_HWMUL_32x32 ? "hard" : "soft",
	    auxv_hwcap & AV_SPARC_HWDIV_32x32 ? "hard" : "soft");
#endif
#endif
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}

/*
 * Print a stack backtrace using the specified stack pointer.  We delay two
 * seconds before continuing, unless this is the panic traceback.  Note
 * that the frame for the starting stack pointer value is omitted because
 * the corresponding %pc is not known.
 */
void
traceback(caddr_t sp)
{

	struct frame *stacktop = (struct frame *)curthread->t_stk;
	struct frame *prevfp = (struct frame *)KERNELBASE;
	struct frame *fp = (struct frame *)(sp + STACK_BIAS);

	flush_windows();

	if (!panicstr)
		printf("traceback: %%sp = %p\n", (void *)sp);

	while (fp > prevfp && fp < stacktop) {
		struct frame *nextfp = (struct frame *)
		    ((uintptr_t)fp->fr_savfp + STACK_BIAS);

		uintptr_t pc = (uintptr_t)fp->fr_savpc;
		ulong_t off;
		char *sym;

		if (nextfp <= fp || nextfp >= stacktop)
			break; /* Stop if we're outside of the expected range */

		if ((uintptr_t)nextfp & (STACK_ALIGN - 1)) {
			printf("  >> mis-aligned %%fp = %p\n", (void *)nextfp);
			break;
		}

		if ((sym = kobj_getsymname(pc, &off)) != NULL) {
#ifdef	_LP64
			printf("%016lx %s:%s+%lx "
#else
			printf("%08lx %s:%s+%lx "
#endif
			    "(%lx, %lx, %lx, %lx, %lx, %lx)\n", (ulong_t)nextfp,
			    mod_containing_pc((caddr_t)pc), sym, off,
			    nextfp->fr_arg[0], nextfp->fr_arg[1],
			    nextfp->fr_arg[2], nextfp->fr_arg[3],
			    nextfp->fr_arg[4], nextfp->fr_arg[5]);
		} else {
#ifdef _LP64
			printf("%016lx %p (%lx, %lx, %lx, %lx, %lx, %lx)\n",
#else
			printf("%08lx %p (%lx, %lx, %lx, %lx, %lx, %lx)\n",
#endif
			    (ulong_t)nextfp, (void *)pc,
			    nextfp->fr_arg[0], nextfp->fr_arg[1],
			    nextfp->fr_arg[2], nextfp->fr_arg[3],
			    nextfp->fr_arg[4], nextfp->fr_arg[5]);
		}

#ifdef _LP64
		printf("  %%l0-3: %016lx %016lx %016lx %016lx\n"
		    "  %%l4-7: %016lx %016lx %016lx %016lx\n",
#else
		printf("  %%l0-7: %08lx %08lx %08lx %08lx "
		    "%08lx %08lx %08lx %08lx\n",
#endif
		    nextfp->fr_local[0], nextfp->fr_local[1],
		    nextfp->fr_local[2], nextfp->fr_local[3],
		    nextfp->fr_local[4], nextfp->fr_local[5],
		    nextfp->fr_local[6], nextfp->fr_local[7]);

		prevfp = fp;
		fp = nextfp;
	}

	if (!panicstr) {
		printf("end of traceback\n");
		DELAY(2 * MICROSEC);
	}
}

/*
 * Generate a stack backtrace from a saved register set.
 */
void
traceregs(struct regs *rp)
{
	traceback((caddr_t)rp->r_sp);
}

/*
 * Generate a stack backtrace from the current location.
 */
void
tracedump(void)
{
	label_t lbl;

	(void) setjmp(&lbl);
	traceback((caddr_t)lbl.val[1]);
}

void
exec_set_sp(size_t stksize)
{
	klwp_t *lwp = ttolwp(curthread);

	lwp->lwp_pcb.pcb_xregstat = XREGNONE;
	if (curproc->p_model == DATAMODEL_NATIVE)
		stksize += sizeof (struct rwindow) + STACK_BIAS;
	else
		stksize += sizeof (struct rwindow32);
	lwptoregs(lwp)->r_sp = (uintptr_t)curproc->p_usrstack - stksize;
}
