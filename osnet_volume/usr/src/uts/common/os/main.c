/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)main.c	1.102	99/11/24 SMI"	/* from SVr4.0 1.31 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/pcb.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/priocntl.h>
#include <sys/procset.h>
#include <sys/var.h>
#include <sys/disp.h>
#include <sys/callo.h>
#include <sys/callb.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/bootconf.h>
#include <sys/utsname.h>
#include <sys/cmn_err.h>
#include <sys/vmparam.h>
#include <sys/modctl.h>
#include <sys/vm.h>
#include <sys/callb.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/cpuvar.h>
#include <sys/cladm.h>
#include <sys/corectl.h>
#include <sys/exec.h>
#include <sys/syscall.h>
#include <sys/reboot.h>

#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/dc_ki.h>

#include <c2/audit.h>

#include <sys/rce.h>

/* well known processes */
proc_t *proc_sched;		/* memory scheduler */
proc_t *proc_init;		/* init */
proc_t *proc_pageout;		/* pageout daemon */
proc_t *proc_fsflush;		/* fsflush daemon */

pgcnt_t	maxmem;		/* Maximum available memory in pages.	*/
pgcnt_t	freemem;	/* Current available memory in pages.	*/
int	audit_active;
int	interrupts_unleashed;	/* set when we do the first spl0() */

kmem_cache_t *process_cache;	/* kmem cache for proc structures */

extern void hotplug_daemon_init(void);

/*
 * Machine-independent initialization code
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork	- process 0 to schedule
 *		- process 1 execute bootstrap
 *		- process 2 to page out
 *	create system threads
 */

int cluster_bootflags = 0;

void
cluster_wrapper(void)
{
	extern void	cluster();
	cluster();
	panic("cluster()  returned");
}

char *initname = "/etc/init";

/*
 * Start the initial user process.
 * The program [initname] is invoked with one argument
 * containing the boot flags.
 *
 * It must be a 32-bit program.
 */
void
icode(void)
{
	char *ucp;
	caddr32_t *uap;
	char *arg0, *arg1;
	proc_t *p = ttoproc(curthread);
	static char pathbuf[128];
	int i, error = 0;
	klwp_t *lwp = ttolwp(curthread);

	/*
	 * Allocate user address space and stack segment
	 */
	proc_init = p;

	p->p_cstime = p->p_stime = p->p_cutime = p->p_utime = 0;
	p->p_usrstack = (caddr_t)USRSTACK32;
	p->p_model = DATAMODEL_ILP32;
	p->p_stkprot = PROT_ZFOD & ~PROT_EXEC;

	p->p_as = as_alloc();
	p->p_as->a_userlimit = (caddr_t)USERLIMIT32;
	(void) hat_setup(p->p_as->a_hat, HAT_ALLOC);
	init_core();

	/*
	 * Construct the boot flag argument.
	 */
	ucp = p->p_usrstack;
	error |= subyte(--ucp, '\0');		/* trailing null byte */

	/*
	 * XXX - should we also handle "-i" ?
	 */
	if (boothowto & RB_SINGLE)
		error |= subyte(--ucp, 's');
	if (boothowto & RB_NOBOOTRC)
		error |= subyte(--ucp, 'b');
	if (boothowto & RB_RECONFIG)
		error |= subyte(--ucp, 'r');
	if (boothowto & RB_VERBOSE)
		error |= subyte(--ucp, 'v');
	if (boothowto & RB_FLUSHCACHE)
		error |= subyte(--ucp, 'f');
	error |= subyte(--ucp, '-');		/* leading hyphen */
	arg1 = ucp;

	/*
	 * Build a pathname.
	 */
	(void) strcpy(pathbuf, initname);

	/*
	 * Move out the file name (also arg 0).
	 */
	for (i = 0; pathbuf[i]; i++);		/* size the name */
	for (; i >= 0; i--)
		error |= subyte(--ucp, pathbuf[i]);
	arg0 = ucp;

	/*
	 * Move out the arg pointers.
	 */
	uap = (caddr32_t *)P2ALIGN((uintptr_t)ucp, sizeof (caddr32_t));
	error |= suword32(--uap, 0);	/* terminator */
	error |= suword32(--uap, (uint32_t)arg1);
	error |= suword32(--uap, (uint32_t)arg0);

	if (error != 0)
		panic("can't create stack for init");

	/*
	 * Point at the arguments.
	 */
	lwp->lwp_ap = lwp->lwp_arg;
	lwp->lwp_arg[0] = (uintptr_t)arg0;
	lwp->lwp_arg[1] = (uintptr_t)uap;
	lwp->lwp_arg[2] = NULL;
	curthread->t_sysnum = SYS_execve;

	init_mstate(curthread, LMS_SYSTEM);

	/*
	 * Now let exec do the hard work.
	 */
	if (error = exec_common((const char *)arg0, (const char **)uap, NULL))
		panic("Can't invoke %s, error %d\n", initname, error);

	lwp_rtt();
}

void
main(void)
{
	int		(**initptr)();
	extern int	sched();
	extern int	fsflush();
	extern void	thread_reaper();
	extern int	(*init_tbl[])();
	extern int	(*mp_init_tbl[])();
	extern id_t	syscid, initcid;
	extern int	swaploaded;
	extern int	netboot;
	extern void	strplumb(void);
	extern void	vm_init(void);
	extern void	cbe_init(void);
	extern void	clock_init(void);
	extern void	physio_bufs_init(void);
	extern void	pm_init(void);
	extern void	start_other_cpus(int);

	/*
	 * In the horrible world of x86 inlines, you can't get symbolic
	 * structure offsets a la genassym.  This assertion is here so
	 * that the next poor slob who innocently changes the offset of
	 * cpu_thread doesn't waste as much time as I just did finding
	 * out that it's hard-coded in i86/ml/i86.il.  You're welcome.
	 */
	ASSERT(curthread == CPU->cpu_thread);

	startup();
	segkmem_gc();
	callb_init();
	callout_init();	/* callout table MUST be init'd before clock starts */
	cbe_init();
	clock_init();

	hotplug_daemon_init();

	/*
	 * Call all system initialization functions.
	 */
	for (initptr = &init_tbl[0]; *initptr; initptr++)
		(**initptr)();

	/*
	 * initialize vm related stuff.
	 */
	vm_init();

	/*
	 * initialize buffer pool for raw I/O requests
	 */
	physio_bufs_init();

	ttolwp(curthread)->lwp_error = 0; /* XXX kludge for SCSI driver */

	/*
	 * Drop the interrupt level and allow interrupts.  At this point
	 * the DDI guarantees that interrupts are enabled.
	 */
	(void) spl0();
	interrupts_unleashed = 1;

	/*
	 * This pm-related call must happen before the root is mounted
	 * since we want to be able to power manage the root device
	 */
	pm_init();		/* initialize the power management framework */

	vfs_mountroot();	/* Mount the root file system */
	cpu_kstat_init(CPU);	/* after vfs_mountroot() so TOD is valid */

	post_startup();
	swaploaded = 1;

	/*
	 * Initial C2 audit system
	 */
#ifdef C2_AUDIT
	audit_init();	/* C2 hook */
#endif

	/*
	 * Plumb the protocol modules and drivers only if we are not
	 * networked booted, in this case we already did it in rootconf().
	 */
	if (netboot == 0)
		strplumb();

	curthread->t_start = u.u_start = hrestime.tv_sec;
	ttoproc(curthread)->p_mstart = gethrtime();
	init_mstate(curthread, LMS_SYSTEM);

	/*
	 * Perform setup functions that can only be done after root
	 * and swap have been set up.
	 */
	consconfig();
#ifdef __i386
	release_bootstrap();
#endif

	/*
	 * Set the scan rate and other parameters of the paging subsystem.
	 */
	setupclock(0);

	/*
	 * Create kmem cache for proc structures
	 */
	process_cache = kmem_cache_create("process_cache", sizeof (proc_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);

	/*
	 * Make init process; enter scheduling loop with system process.
	 */

	/* create init process */
	if (newproc((void (*)())icode, initcid, 59)) {
		panic("main: unable to fork init.");
	}

	/* create pageout daemon */
	if (newproc((void (*)())pageout, syscid, maxclsyspri - 1)) {
		panic("main: unable to fork pageout()");
	}

	/* create fsflush daemon */
	if (newproc((void (*)())fsflush, syscid, minclsyspri)) {
		panic("main: unable to fork fsflush()");
	}

	/* create cluster process if we're a member of one */
	if (cluster_bootflags & CLUSTER_BOOTED) {
		if (newproc((void (*)())cluster_wrapper, syscid, minclsyspri)) {
			panic("main: unable to fork cluster()");
		}
	}

	/*
	 * Create system threads (threads are associated with p0)
	 */

	/* create thread_reaper daemon */
	if (thread_create(NULL, PAGESIZE, (void (*)())thread_reaper,
	    0, 0, &p0, TS_RUN, minclsyspri) == NULL) {
		panic("main: unable to create thread_reaper thread");
	}

	/* create module uninstall daemon */
	/* BugID 1132273. If swapping over NFS need a bigger stack */
	if ((thread_create(NULL, DEFAULTSTKSZ, (void (*)())mod_uninstall_daemon,
	    0, 0, &p0, TS_RUN, minclsyspri)) == NULL) {
		panic("main: unable to create module uninstall thread");
	}

	/*
	 * SRM hook for final initialization of SRM before proc 0 enters
	 * sched(); a system process will be forked using newproc(), if that
	 * fails then the error status is returned.
	 * The hook is placed just before pid_setmin() so that the SRM process
	 * will get a small pid. Other processes with small pids and system
	 * threads of proc 0 has already started. Proc 1 exists but has not
	 * begun executing yet. SHR scheduler has already been loaded by the
	 * dispinit() call via the startup() call above.
	 * If SRM_START() will fail then the system is probably in some kind
	 * of serious trouble; if initclass is "SHR" then /etc/init is already
	 * committed to be in the SHR class but it is not going to be scheduled
	 * properly - we may be doomed but let's not panic yet.
	 * SRM_START() does nothing unless sched/SHR has already been loaded
	 * (as a result of *initclass or *extraclass changed to "SHR" in
	 * /etc/system file).
	 */
	(void) SRM_START(syscid, minclsyspri);

	pid_setmin();

	/*
	 * Perform MP initialization, if any.
	 */
	start_other_cpus(0);

	/*
	 * After mp_init(), number of cpus are known (this is
	 * true for the time being, when there are acutally
	 * hot pluggable cpus then this scheme  would not do).
	 * Any per cpu initialization is done here.
	 */
	kmem_mp_init();
	vmem_mp_init();

	if (thread_create(NULL, PAGESIZE, seg_pasync_thread,
	    0, 0, &p0, TS_RUN, minclsyspri) == NULL) {
		panic("main: unable to create page async thread");
	}

	for (initptr = &mp_init_tbl[0]; *initptr; initptr++)
		(**initptr)();

	bcopy("sched", u.u_psargs, 6);
	bcopy("sched", u.u_comm, 5);
	sched();
	/* NOTREACHED */
}
