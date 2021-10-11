/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sundep.c	1.130	99/08/31 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/archsystm.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/ucontext.h>

#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>
#include <sys/thread.h>
#include <sys/vtrace.h>
#include <sys/consdev.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/swap.h>
#include <sys/vmparam.h>
#include <sys/cpuvar.h>
#include <sys/cpu.h>

#include <sys/privregs.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>

#include <sys/exec.h>
#include <sys/acct.h>
#include <sys/corectl.h>
#include <sys/modctl.h>
#include <sys/tuneable.h>

#include <c2/audit.h>

#include <sys/trap.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>
#include <sys/memlist.h>
#include <sys/systeminfo.h>
#include <sys/promif.h>

/*
 * Compare the version of boot that boot says it is against
 * the version of boot the kernel expects.
 *
 * XXX	There should be no need to use promif routines here.
 */
int
check_boot_version(int boots_version)
{
	if (boots_version == BO_VERSION)
		return (0);

	prom_printf("Wrong boot interface - kernel needs v%d found v%d\n",
	    BO_VERSION, boots_version);
	prom_panic("halting");
	/*NOTREACHED*/
}

/*
 * Kernel setup code, called from startup().
 */
void
kern_setup1(void)
{
	proc_t *pp;

	pp = &p0;

	proc_sched = pp;

	/*
	 * Initialize process 0 data structures
	 */
	pp->p_stat = SRUN;
	pp->p_flag = SLOAD | SSYS | SLOCK | SULOAD;

	pp->p_pidp = &pid0;
	pp->p_pgidp = &pid0;
	pp->p_sessp = &session0;
	pp->p_tlist = &t0;
	pid0.pid_pglink = pp;

	/*
	 * We assume that the u-area is zeroed out.
	 */
	u.u_cmask = (mode_t)CMASK;

	/*
	 * Set up default resource limits.
	 */
	bcopy(rlimits, u.u_rlimit, sizeof (struct rlimit64) * RLIM_NLIMITS);

	thread_init();		/* init thread_free list */
#ifdef LATER
	hrtinit();		/* init hires timer free list */
	itinit();		/* init interval timer free list */
#endif
	pid_init();		/* initialize pid (proc) table */

	init_pages_pp_maximum();

	/*
	 * Set the system wide, processor-specific flags to be
	 * passed to userland via the aux vector. (Switch
	 * on any handy kernel optimizations at the same time.)
	 */
	bind_hwcap();
}

static struct  bootcode {
	char    letter;
	uint_t   bit;
} bootcode[] = {	/* See reboot.h */
	'a',	RB_ASKNAME,
	's',	RB_SINGLE,
	'i',	RB_INITNAME,
	'h',	RB_HALT,
	'b',	RB_NOBOOTRC,
	'd',	RB_DEBUG,
	'w',	RB_WRITABLE,
	'r',    RB_RECONFIG,
	'c',    RB_CONFIG,
	'v',	RB_VERBOSE,
	'f',	RB_FLUSHCACHE,
	'x',	RB_NOBOOTCLUSTER,
	0,	0,
};

char kern_bootargs[256];		/* Max of all OBP_MAXPATHLEN's */

/*
 * Parse the boot line to determine boot flags .
 */
void
bootflags(void)
{
	char *cp;
	int i;
	extern char *initname;

	if (BOP_GETPROP(bootops, "boot-args", kern_bootargs) != 0) {
		cp = NULL;
		boothowto |= RB_ASKNAME;
	} else {
		cp = kern_bootargs;

		while (*cp && *cp != ' ')
			cp++;

		while (*cp && *cp != '-')
			cp++;

		if (*cp && *cp++ == '-')
			do {
				for (i = 0; bootcode[i].letter; i++) {
					if (*cp == bootcode[i].letter) {
						boothowto |= bootcode[i].bit;
						break;
					}
				}
				cp++;
			} while (bootcode[i].letter && *cp);
	}

	if (boothowto & RB_INITNAME) {
		/*
		 * XXX	This is a bit broken - shouldn't we
		 *	really be using the initpath[] above?
		 */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
		initname = cp;
	}

	if (boothowto & RB_HALT) {
		prom_printf("kernel halted by -h flag\n");
		prom_enter_mon();
	}
}

/*
 * Load a procedure into a thread.
 */
int
thread_load(
	kthread_t	*t,
	void		(*start)(),
	caddr_t		arg,
	size_t		len)
{
	struct rwindow *rwin;
	caddr_t sp;
	size_t framesz;
	caddr_t argp;
	label_t ljb;
	extern void thread_start();

	/*
	 * Push a "c" call frame onto the stack to represent
	 * the caller of "start".
	 */
	sp = t->t_stk;
	if (len != 0) {
		/*
		 * the object that arg points at is copied into the
		 * caller's frame.
		 */
		framesz = SA(len);
		sp -= framesz;
		if (sp < t->t_swap)	/* stack grows down */
			return (-1);
		argp = sp + SA(MINFRAME);
		if (on_fault(&ljb)) {
			no_fault();
			return (-1);
		}
		bcopy(arg, argp, len);
		no_fault();
		arg = (void *)argp;
	}
	/*
	 * store arg and len into the frames input register save area.
	 * these are then transfered to the first 2 output registers by
	 * thread_start() in swtch.s.
	 */
	rwin = (struct rwindow *)sp;
	rwin->rw_in[0] = (intptr_t)arg;
	rwin->rw_in[1] = len;
	rwin->rw_in[6] = 0;
	rwin->rw_in[7] = (intptr_t)start;
	/*
	 * initialize thread to resume at thread_start().
	 */
	t->t_pc = (uintptr_t)thread_start - 8;
	t->t_sp = (uintptr_t)sp - STACK_BIAS;

	return (0);
}

#if !defined(lwp_getdatamodel)

/*
 * Return the datamodel of the given lwp.
 */
model_t
lwp_getdatamodel(klwp_t *lwp)
{
	return (lwp->lwp_procp->p_model);
}

#endif	/* !lwp_getdatamodel */

#if !defined(get_udatamodel)

model_t
get_udatamodel(void)
{
	return (curproc->p_model);
}

#endif	/* !get_udatamodel */
