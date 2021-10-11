/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.273	99/10/22 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/vm.h>

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/callb.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/panic.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/async.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/comvec.h>
#include <sys/modctl.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/ddidmareq.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/bustypes.h>
#include <sys/clock.h>
#include <sys/physaddr.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/intreg.h>
#include <sys/memerr.h>
#include <sys/auxio.h>
#include <sys/trap.h>
#include <sys/module.h>
#include <sys/x_call.h>
#include <sys/spl.h>
#include <sys/machpcb.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <vm/hat_srmmu.h>
#include <sys/iommu.h>
#include <sys/vtrace.h>
#include <sys/autoconf.h>
#include <sys/instance.h>
#include <sys/bt.h>
#include <sys/avintr.h>
#include <sys/aflt.h>
#include <sys/memctl.h>
#include <sys/epm.h>

static void idle_other_cpus(void);
static void resume_other_cpus(void);
static void stop_other_cpus(void);
extern int xc_level_ignore;

extern void set_idle_cpu(int);
extern void unset_idle_cpu(int);

extern void ic_flush();

void	power_down(char *);

/*
 * Miscellaneous hardware feature override flags
 */

int use_ic = 1;
int use_dc = 1;
int use_ec = 1;

/*
 * Viking/MXCC specific overrides
 */

int use_vik_prefetch = 0;
int use_mxcc_prefetch = 1;
int use_store_buffer = 1;
int use_multiple_cmds = 1;
int use_rdref_only = 0;

int do_pg_coloring = 0;		/* will be set for Viking/MXCC only */
int use_page_coloring = 1;	/* patch to 0 to disable above */

int Cpudelay = 0;		/* delay loop count/usec */

int dvmasize = 255;		/* usable dvma space */

/*
 * Boot hands us the romp.
 */
#if !defined(SAS) && !defined(MPSAS)
union sunromvec *romp = (union sunromvec *)0;
struct debugvec *dvec = (struct debugvec *)0;
#else
union sunromvec *romp = (struct sunromvec *)0;
#endif

int maxphys = 124 << 10;
int klustsize = 124 << 10;

int vac = 0;		/* virtual address cache type (none == 0) */

#ifdef	BCOPY_BUF
int bcopy_buf = 0;		/* block copy buffer present */
#endif /* BCOPY_BUF */

/*
 * Globals for asynchronous fault handling
 */
/*
 * Synchronization used by the level-15 asynchronous fault
 * handling threads (one per CPU).  Each CPU which responds
 * to the broadcast level 15 interrupt, sets its entry in
 * aflt_sync.  Then, at the synchronization point, each CPU waits
 * for CPU 0 to clear aflt_sync.
 */
volatile uint_t	aflt_sync[NCPU];
int	procset = 1;

int	pokefault = 0;

/*
 * A dummy location where the flush_writebuffers routine
 * can perform a swap in order to flush the module write buffer.
 */
uint_t	module_wb_flush;

/*
 * When nofault is set, if an asynchronous fault occurs, this
 * flag will get set, so that the probe routine knows something
 * went wrong.  It is up to the probe routine to reset this
 * once it's done messing around.
 */
volatile uint_t	aflt_ignored = 0;

/*
 * When the first system-fatal asynchronous fault is detected,
 * this variable is atomically set.
 */
volatile uint_t	system_fatal = 0;

/*
 * ... and info specifying the fault is stored here:
 */
struct async_flt sys_fatal_flt[NCPU];

/*
 * Used to store a queue of non-fatal asynchronous faults to be
 * processed.
 */
struct	async_flt a_flts[NCPU][MAX_AFLTS];

/*
 * Incremented by 1 (modulo MAX_AFLTS) by the level-15 asynchronous
 * interrupt thread before info describing the next non-fatal fault
 * in a_flts[a_head].
 */
uint_t	a_head[NCPU];

/*
 * Incremented by 1 (modulo MAX_AFLTS) by the level-12 asynchronous
 * fault handler thread before processing the next non-fatal fault
 * described by info in a_flts[a_tail].
 */
uint_t	a_tail[NCPU];

/* Flag for testing directed interrupts */
int	test_dirint = 0;

/*
 * Interrupt Target Mask Register masks for each interrupt level.
 * Element n corresponds to interrupt level n.  To optimize the
 * interrupt code that manipulates this data structure, element 0 is
 * not used (no level 0 interrupts).
 */
uint_t	itmr_masks[] = {0, SIR_L1, SIR_L2, SIR_L3, SIR_L4, SIR_L5,
			SIR_L6, SIR_L7, SIR_L8, SIR_L9, SIR_L10,
			SIR_L11, SIR_L12, SIR_L13, SIR_L14, SIR_L15};

/*
 * When set to non-zero, the nth element of this array indicates that
 * some CPU has masked level n interrupts (using the ITMR).
 * Set to the MID of the CPU that did the mask, so that upon returning
 * from the interrupt, a CPU can tell if it set the mask.  To optimize
 * the interrupt code that manipulates this data structure, element 0 is
 * not used (no level 0 interrupts).
 * The information contained here could have been represented with a
 * one-word bit mask, but then additional time consuming code would be
 * necessary to protect multiple CPUs accessing such a bit mask at the
 * same time.  Note that no two CPUs will ever be accessing the same
 * element simultaneously.
 */
uchar_t	ints_masked[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
 * workaround for hardware bug Ross 605
 */
int ross_hd_bug;

int small_4m = 0;		/* flag for small sun4m machines */
size_t dump_reserve;		/* flags a series of VOP_DUMP() */


/*
 * Check to see if the system has the ability of doing software power off.
 */
int
power_off_is_supported(void)
{
	int is_defined = 0;
	char *wordexists = "p\" power-off\" find nip swap ! ";

	prom_interpret(wordexists, (int)(&is_defined), 0, 0, 0, 0);
	if (is_defined != 0) /* is_defined has value -1 when defined */
		return (1);
	else
		return (0);
}


/*
 * If bootstring contains a device path, we need to convert to a format
 * the prom will understand and return this new bootstring in buf.
 */
static char *
convert_boot_device_name(char *cur_path) {
	char *ret = cur_path;
	char *ptr, *buf;

	if ((buf = (char *)kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return (cur_path);
	if ((ptr = strchr(cur_path, ' ')) != NULL)
		*ptr = '\0';
	if (i_devname_to_promname(cur_path, buf) == 0) {
		/* the conversion worked */
		if (ptr != NULL) {
			*ptr = ' ';
			(void) strcat(buf, ptr);
			ptr = NULL;
		}
		ret = buf;
	} else if (ptr != NULL) {	/* the conversion failed */
		kmem_free((void *)buf, MAXPATHLEN);
		*ptr = ' ';
		ret = cur_path;
	}
	return (ret);
}

/*
 * Machine dependent code to reboot.
 * If "bootstr" is non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *bootstr)
{
	int s;

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

#if defined(SAS) || defined(MPSAS)
	asm("t 255");
#else
	s = spl6();
	reset_leaves(); 		/* try and reset leaf devices */
	if (fcn == AD_HALT) {
		halt((char *)NULL);
		fcn &= ~RB_HALT;
		/* MAYBE REACHED */
	} else if (fcn == AD_POWEROFF) {
		if (power_off_is_supported())
			power_down((char *)NULL);
		else
			halt((char *)NULL);
		/* NOTREACHED */
	} else {
		if (bootstr == NULL) {
			switch (fcn) {

			case AD_BOOT:
				bootstr = "";
				break;

			case AD_IBOOT:
				bootstr = "-a";
				break;

			case AD_SBOOT:
				bootstr = "-s";
				break;

			case AD_SIBOOT:
				bootstr = "-sa";
				break;
			default:
				cmn_err(CE_WARN,
				    "mdboot: invalid function %d", fcn);
				bootstr = "";
				break;
			}
		} if (*bootstr == '/') {
			/* take care of any devfs->prom device name mappings */
			bootstr = convert_boot_device_name(bootstr);
		}
		prom_printf("rebooting...\n");
		prom_reboot(bootstr);
		/*NOTREACHED*/
	}
	splx(s);
#endif /* SAS */
}

static void
idle_other_cpus(void)
{
	int i;
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if (i != cpuid && cpu_nodeid[i] != -1 &&
		    cpu[i] != NULL && (cpu[i]->cpu_flags & CPU_EXISTS)) {
			(void) prom_idlecpu((dnode_t)cpu_nodeid[i]);
		}
	}
}

static void
resume_other_cpus(void)
{
	int i;
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if (i != cpuid && cpu_nodeid[i] != -1 &&
		    cpu[i] != NULL && (cpu[i]->cpu_flags & CPU_EXISTS)) {
			(void) prom_resumecpu((dnode_t)cpu_nodeid[i]);
		}
	}
}

static void
stop_other_cpus(void)
{
	int i;
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if (i != cpuid && cpu_nodeid[i] != -1 &&
		    cpu[i] != NULL && (cpu[i]->cpu_flags & CPU_EXISTS)) {
			(void) prom_stopcpu((dnode_t)cpu_nodeid[i]);
		}
	}
}


/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable == 0)
		return;
	(void) pm_powerup_console();
	debug_enter(msg);
}

void
vac_flushall(void)
{
	XCALL_PROLOG
	vac_ctxflush(0, FL_CACHE);	/* supv and ctx 0 */
	vac_usrflush(FL_ALLCPUS);	/* user */
	XCALL_EPILOG
}

/*
 * Enter debugger.  Called when the user types L1-A or break or whenever
 * code wants to enter the debugger and possibly resume later.
 * If the debugger isn't present, enter the PROM monitor.
 */
int debug_enter_cpuid;
int vx_entered = 0;

void
debug_enter(char *msg)
{
	int s;
	label_t debug_save;

	if (msg)
		prom_printf("%s\n", msg);

	s = splzs();

	if (panic_thread == NULL) {
		/*
		 * Do not try cross-calls if we have too high
		 * a PSR level, since we may be deadlocked in
		 * the cross-call routines.
		 */
		if (getpil() < XC_MED_PIL)
			vac_flushall();
		idle_other_cpus();
	}

	debug_enter_cpuid = CPU->cpu_id;

	/*
	 * If we came in through vx_handler, then kadb won't talk to us
	 * anymore, so until we fix it, just drop into the prom--all we
	 * want to be able to do at this point is reboot
	 */
	if (!vx_entered && boothowto & RB_DEBUG) {
		flush_windows();
		debug_save = curthread->t_pcb;
		(void) setjmp(&curthread->t_pcb);
		(*(vfunc_t)(&dvec->dv_entry))();
		curthread->t_pcb = debug_save;
	} else {
		prom_enter_mon();
	}

	if (panic_thread == NULL)
		resume_other_cpus();

	splx(s);
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s)
		prom_printf("(%s) ", s);

	prom_exit_to_mon();
	/*NOTREACHED*/
}


/*
 * Halt the machine and power off the system.
 */
void
power_down(char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s)
		prom_printf("(%s) ", s);

	prom_interpret("power-off", 0, 0, 0, 0, 0);
	/*
	 * If here is reached, for some reason prom's power-off command failed.
	 * Prom should have already printed out error messages. Exit to
	 * firmware.
	 */
	prom_exit_to_mon();
	/*NOTREACHED*/
}


/*
 * Enter monitor.  Called via cross-call from stop_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);
	prom_exit_to_mon();
}


/*
 * Write the scb, which is the first page of the kernel.
 * Normally it is write protected, we provide a function
 * to fiddle with the mapping temporarily.
 *	1) lock out interrupts
 *	2) save the old pte value of the scb page
 *	3) set the pte so it is writable
 *	4) write the desired vector into the scb
 *	5) restore the pte to its old value
 *	6) restore interrupts to their previous state
 */
void
write_scb_int(register int level, struct trapvec *ip)
{
	register int s;
	register trapvec *sp;

	/*
	 * Don't touch anything if V_TBR_WR_ADDR is not set up yet. This
	 * can happen if the system panics early in the boot process. We
	 * don't want to cause a DBE here.
	 */
	if (!tbr_wr_addr_inited)
		return;

	sp = &((struct scb *)V_TBR_WR_ADDR)->interrupts[level - 1];
	s = spl8();

	/*
	 * Ensure that only threads with the hat mutex flush do cache
	 * flushes.
	 */
	hat_enter(kas.a_hat);

	/*
	 * We purposely avoid using the CAPTURE_CPUS macro, because
	 * we always want to capture all CPUs, even when OPTIMAL_CACHE
	 * is enabled.
	 */
	if (vac) {
		int xc_lsave = xc_level_ignore;

		xc_level_ignore = 1;	/* disable xc assertion */
		xc_capture_cpus(-1);

		xc_level_ignore = xc_lsave;
	}

	/* write out new vector code */
	*sp = *ip;

	if (vac) {
		/*
		 * This flush ensures that the cache line that was updated when
		 * the trap vector was modified via the RW mapping above gets
		 * written to memory.
		 */
		vac_allflush(FL_CACHE);

		/*
		 * We flush again to deal with the scenerio where a trap could
		 * have occured before the last flush completed and caused the
		 * cache line associated with the RO mapping to become stale.
		 */
		vac_allflush(FL_CACHE);

		/*
		 * Finally, since the code above may only have flushed the
		 * external cache on some processors, we must also flush the
		 * internal cache.
		 */
		ic_flush();

		xc_release_cpus();
	}

	hat_exit(kas.a_hat);

	splx(s);
}

#ifdef	DEBUGGING_MEM

static int dset;

#define	STOP()			if (dset) prom_enter_mon()
#define	DPRINTF(str)		if (dset) prom_printf((str))
#define	DPRINTF1(str, a)	if (dset) prom_printf((str), (a))
#define	DPRINTF2(str, a, b)	if (dset) prom_printf((str), (a), (b))
#define	DPRINTF3(str, a, b, c)	if (dset) prom_printf((str), (a), (b), (c))
#else	DEBUGGING_MEM
#define	STOP()
#define	DPRINTF(str)
#define	DPRINTF1(str, a)
#define	DPRINTF2(str, a, b)
#define	DPRINTF3(str, a, b, c)
#endif	DEBUGGING_MEM

#ifndef SAS
/*
 * Console put and get character routines.
 */
void
cnputs(const char *buf, uint_t bufsize, int device_in_use)
{
	if (device_in_use) {
		int s;
		/*
		 * This means that some other CPU may have a mapping
		 * to the device (framebuffer) that the OBP is about
		 * to render onto.  Some of the fancier framebuffers get upset
		 * about being accessed by more than one CPU - so stop
		 * the others in their tracks.
		 *
		 * This is a somewhat unfortunate 'hackaround' to the general
		 * problem of sharing a device between the OBP and userland.
		 *
		 * This should happen -very- rarely on a running system
		 * provided you have a console window redirecting console
		 * output when running your favourite window system ;-)
		 */
		s = splhi();
		xc_capture_cpus(CPUSET(CPU->cpu_id));
		idle_other_cpus();
		prom_writestr(buf, bufsize);
		resume_other_cpus();
		xc_release_cpus();
		splx(s);
	} else
		prom_writestr(buf, bufsize);
}

void
cnputc(register int c, int device_in_use)
{
	int s;
	if (device_in_use) {
		s = splhi();
		xc_capture_cpus(CPUSET(CPU->cpu_id));
		idle_other_cpus();
	}

	if (c == '\n')
		prom_putchar('\r');
	prom_putchar(c);

	if (device_in_use) {
		resume_other_cpus();
		xc_release_cpus();
		splx(s);
	}
}

static int
cngetc(void)
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 *
 * XXX	There's no need for both cngetc() and getchar() -- merge 'em
 */
static int
getchar(void)
{
	register c;

	c = cngetc();
	if (c == '\r')
		c = '\n';
	cnputc(c, 0);
	return (c);
}

/*
 * Get a line from the console.
 */
void
gets(char *cp)
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		c = getchar() & 0177;
		switch (c) {

		case '\n':
			*lp++ = '\0';
			return;

		case 0177:
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;

		case 'u'&037:
			lp = cp;
			cnputc('\n', 0);
			continue;

		case '@':	/* used in physical device names! */
		default:
			*lp++ = (char)c;
		}
	}
}

#endif /* !SAS */

/*
 * Allocate threads and stacks for interrupt handling.
 */
#define	NINTR_THREADS	LOCK_LEVEL	/* number of interrupt threads */

void
init_intr_threads(struct cpu *cp)
{
	int i;

	for (i = 0; i < NINTR_THREADS; i++)
		(void) thread_create_intr(cp);

	cp->cpu_intr_stack = (caddr_t)segkp_get(segkp, INTR_STACK_SIZE,
		KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED) +
		INTR_STACK_SIZE - SA(MINFRAME);
}

void
vx_handler(char *str)
{
	struct scb *oldtbr;
	int i;

	reestablish_curthread();

	/*
	 * Set tbr to the appropriate value.
	 * See CPU_INDEX macro in srmmu/sys/mmu.h for more info.
	 * set_tbr performs splhi/splx equivalent, so we don't have to.
	 */
	if (CPU->cpu_id != 0) {
		oldtbr = set_tbr((struct scb *)
		    (V_TBR_ADDR_BASE + (CPU->cpu_id * MMU_PAGESIZE)));
	} else
		oldtbr = set_tbr(&scb);


	if (strcmp(str, "sync") == 0) {
		/*
		 * Avoid trying to talk to the other CPUs since they are
		 * already sitting in the prom and won't reply.
		 */
		for (i = 0; i < NCPU; i++) {
			if (i != CPU->cpu_id && cpu_nodeid[i] != -1 &&
			    cpu[i] != NULL &&
			    (cpu[i]->cpu_flags & CPU_EXISTS)) {
				cpu[i]->cpu_flags &= ~CPU_READY;
				cpu[i]->cpu_flags |= CPU_QUIESCED;
			}
		}
		nopanicdebug = 1;	/* don't try to re-enter kadb or prom */
		vx_entered++;		/* tell debug_enter() to avoid kadb */
		panic("zero");		/* force a panic */
	} else
		prom_printf("Don't understand '%s'\n", str);

	(void) set_tbr(oldtbr);
}

/*
 * set delay constant for usec_delay()
 * NOTE: we use the fact that the per-
 * processor clocks are available and
 * mapped properly at "*utimersp".
 */
void
setcpudelay(void)
{
	extern volatile struct count14	*utimersp;
	unsigned	r;		/* timer resolution, ~ns */
	unsigned	e;		/* delay time, us */
	unsigned	es;		/* delay time, ~ns */
	unsigned	t, f;		/* for time measurement */
	int		s;		/* saved PSR for inhibiting ints */
	uint_t		orig_control;	/* original clock control register */
	uint_t		orig_config;	/* original timer config register */
	uint_t		orig_limit;	/* oringal counter/timer limit */

	if (Cpudelay != 0)	/* for MPSAS, adb patch Cpudelay = 1 */
		return;

	r = 512;		/* worst supported timer resolution */
	es = r * 100;		/* target delay in ~ns */
	e = ((es + 1023) >> 10); /* request delay in us, round up */
	es = e << 10;		/* adjusted target delay in ~ns */
	Cpudelay = 1;		/* initial guess */

	/*
	 * Save current configuration of cpu 0's timer so we can put things
	 * back the way we found them.  If the timer is configured as a
	 * counter/timer, save the limit register, reconfig as a user
	 * timer and restore everything on exit.  If configured already
	 * as a user timer, start it if it isn't running and turn it off
	 * before exit.  If already running, don't change it now or on exit.
	 *
	 * Note: If it is necessary to reconfig as a user timer, we can
	 * only restore the original limit register value on exit which
	 * causes the counter to be reinitialized to 500ns.  Otherwise,
	 * the counter register is read-only.
	 */
	if (((orig_config = v_level10clk_addr->config) & TMR0_CONFIG) == 0) {
		orig_limit = utimersp->timer_msw;
		v_level10clk_addr->config |= TMR0_CONFIG;
		orig_control = utimersp->control;
		utimersp->control = 1;
	} else
	if ((orig_control = utimersp->control) == 0)
		utimersp->control = 1;

	DELAY(1);		/* first time may be weird */
	do {
		Cpudelay <<= 1;	/* double until big enough */
		do {
			s = spl8();
			t = utimersp->timer_lsw;
			DELAY(e);
			f = utimersp->timer_lsw;
			splx(s);
		} while (f < t);
		t = f - t;
	} while (t < es);
	Cpudelay = (Cpudelay * es + t) / t;
	if (Cpudelay < 0)
		Cpudelay = 0;

	do {
		s = spl8();
		t = utimersp->timer_lsw;
		DELAY(e);
		f = utimersp->timer_lsw;
		splx(s);
	} while (f < t);
	t = f - t;

	/*
	 * Restore the original timer conifiguration.
	*/
	if ((orig_config & TMR0_CONFIG) == 0) { /* formerly a counter/timer */
		utimersp->control = orig_control;
		v_level10clk_addr->config = orig_config;
		utimersp->timer_msw = orig_limit;
	} else					/* formerly a user timer */
		if (orig_control == 0)
			utimersp->control = 0;
}

/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{}

/*
 * XXX These probably ought to live somewhere else - how about vm_machdep.c?
 * XXX They are called from mem.c
 */

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
pfn_t
impl_obmem_pfnum(pfn_t pf)
{
	return (pf);
}

/*
 * A run-time interface to the MAKE_PFNUM macro.
 */
int
impl_make_pfnum(struct pte *pte)
{
	return (MAKE_PFNUM(pte));
}

/*ARGSUSED*/
int
aflt_get_iblock_cookie(dev_info_t *dip, int fault_type,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	extern uint_t sx_ctlr_present;

	/*
	 * Currently we only offer this service on C2/C2+ for nvsimms
	 * and SX.
	 */
	if (!nvsimm_present && !sx_ctlr_present)
		return (AFLT_NOTSUPPORTED);

	if (mc_type != MC_EMC && mc_type != MC_SMC)
		return (AFLT_NOTSUPPORTED);

	if (fault_type != AFLT_ECC && fault_type != AFLT_SX)
		return (AFLT_NOTSUPPORTED);

	*iblock_cookiep = (ddi_iblock_cookie_t)ipltospl(AFLT_HANDLER_LEVEL);
	return (AFLT_SUCCESS);
}

int
aflt_add_handler(dev_info_t *dip, int fault_type, void **hid,
    int (*func)(void *, void *), void *arg)
{
	extern struct memslot memslots[];
	extern void *sx_aflt_fun;
	extern uint_t sx_ctlr_present;
	extern char nvsimm_name[];
	struct aflt_cookie *ac;
	void *hid2;
	struct regspec *rp;
	uint_t regsize;
	int slot;

	*hid = NULL;

	/*
	 * Currently we only offer this service on C2/C2+ for nvsimms
	 * and SX.
	 */
	if (mc_type != MC_EMC && mc_type != MC_SMC) {
		return (AFLT_NOTSUPPORTED);
	}

	switch (fault_type) {
	case AFLT_ECC:
		if (!nvsimm_present)
			return (AFLT_NOTSUPPORTED);

		if (strcmp(DEVI(dip)->devi_name, nvsimm_name) != 0 ||
		    ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, OBP_REG, (int **)&rp, &regsize) !=
		    DDI_PROP_SUCCESS) {
			    return (AFLT_NOTSUPPORTED);
		}
		/* convert to bytes */
		regsize = regsize * sizeof (int);
		ASSERT(regsize >= 2 * sizeof (struct regspec));

		slot = PMEM_SLOT(rp->regspec_addr);
		ASSERT(memslots[slot].ms_bustype == BT_NVRAM);
		ddi_prop_free((void *)rp);
		if (memslots[slot].ms_func != NULL) {
			return (AFLT_FAILURE);
		}
		hid2 = (void *)&memslots[slot];
		break;

	case AFLT_SX:
		if (!sx_ctlr_present)
			return (AFLT_NOTSUPPORTED);

		if (sx_aflt_fun != NULL)
			return (AFLT_FAILURE);
		hid2 = (void *)func;
		break;

	default:
		return (AFLT_NOTSUPPORTED);
	}

	ac = kmem_zalloc(sizeof (struct aflt_cookie), KM_NOSLEEP);
	if (ac == NULL)
		return (AFLT_FAILURE);

	ac->handler_type = fault_type;
	ac->cookie = hid2;
	*hid = ac;

	switch (fault_type) {
	case AFLT_ECC:
		memslots[slot].ms_dip = (void *)dip;
		memslots[slot].ms_arg = arg;
		memslots[slot].ms_func = func;
		break;

	case AFLT_SX:
		sx_aflt_fun = hid2;
		break;
	}

	return (AFLT_SUCCESS);
}

int
aflt_remove_handler(void *hid)
{
	extern void *sx_aflt_fun;
	register struct memslot *mp;
	register struct aflt_cookie *ac = (struct aflt_cookie *)hid;

	if (ac == NULL)
		return (AFLT_FAILURE);

	switch (ac->handler_type) {
	case AFLT_ECC:
		mp = ac->cookie;
		if (mp == NULL || mp->ms_bustype != BT_NVRAM ||
		    mp->ms_func == NULL) {
			return (AFLT_FAILURE);
		}
		mp->ms_func = NULL;
		break;

	case AFLT_SX:
		if (ac->cookie == NULL)
			return (AFLT_FAILURE);
		sx_aflt_fun = NULL;
		break;

	default:
		return (AFLT_NOTSUPPORTED);
	}

	ac->handler_type = 0;
	ac->cookie = NULL;

	wait_till_seen(AFLT_HANDLER_LEVEL);
	kmem_free(ac, sizeof (struct aflt_cookie));

	return (AFLT_SUCCESS);
}


int
mem_bus_type(uint_t pfn)
{
	extern struct memslot memslots[];
	int slot = PMEM_PFN_SLOT(pfn);

	ASSERT(memslots[slot].ms_bustype);
	return (memslots[slot].ms_bustype);
}


/*
 * Initialize kernel thread's stack.
 */
caddr_t
thread_stk_init(caddr_t stk)
{
	return (stk - SA(MINFRAME));
}

/*
 * Initialize lwp's kernel stack.
 */
caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	struct machpcb *mpcb;

	stk -= SA(sizeof (struct machpcb));
	mpcb = (struct machpcb *)stk;
	bzero((caddr_t)mpcb, sizeof (struct machpcb));
	lwp->lwp_regs = (void *)&mpcb->mpcb_regs;
	lwp->lwp_fpu = (void *)&mpcb->mpcb_fpu;
	mpcb->mpcb_fpu.fpu_q = mpcb->mpcb_fpu_q;
	mpcb->mpcb_thread = lwp->lwp_thread;
	return (stk);
}

/*ARGSUSED*/
void
lwp_stk_fini(klwp_t *lwp)
{}

void
sync_icache(caddr_t va, uint_t len)
{
	caddr_t end;

	end = va + len;
	va = (caddr_t)((uintptr_t)va & -8l);	/* sparc needs 8-byte align */
	while (va < end) {
		doflush(va);
		va += 8;
	}
#ifdef VAC
	/*
	 * We need a vac_flush() if we've modified kernel text because
	 * such modifications are done through a different (writable)
	 * mapping; with a virtually-indexed, virtually-tagged cache
	 * the flush instruction may not realize it has anything to do.
	 */
	if (va >= (caddr_t)KERNELBASE) {
		XCALL_PROLOG
		vac_flush(va, len);
		XCALL_EPILOG
	}
#endif
}

/*
 * Call prom_stopcpu() on each active CPU except the current CPU.  Afterward
 * set xc_level_ignore to ignore cross-calls since other CPUs are stopped.
 */
/*ARGSUSED*/
void
panic_stopcpus(cpu_t *cp, kthread_t *t, int spl)
{
	processorid_t i;
	cpuset_t xcset;

	CPUSET_ALL_BUT(xcset, cp->cpu_id);
	(void) splzs();
	xc_capture_cpus(xcset);

	for (i = 0; i < NCPU; i++) {
		if (i != cp->cpu_id && cpu_nodeid[i] != -1 && cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS)) {
			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
			(void) prom_stopcpu((dnode_t)cpu_nodeid[i]);
		}
	}

	xc_level_ignore = 1;
}

/*
 * Miscellaneous hardware-specific code to execute after panicstr is set
 * by the panic code and the other CPUs have been stopped.
 */
/*ARGSUSED*/
void
panic_quiesce_hw(panic_data_t *pdp)
{
	/*
	 * Make sure panicbuf changes hit memory.
	 */
	vac_flush(panicbuf, PANICBUFSIZE);

	/*
	 * Redirect all interrupts to the panic CPU.
	 */
	set_intmask(IR_ENA_INT, 1);
	set_itr_bycpu(CPU->cpu_id);
	set_intmask(IR_ENA_INT, 0);
}

/*
 * Platform callback prior to writing crash dump.
 */
/*ARGSUSED*/
void
panic_dump_hw(int spl)
{
	/* Nothing to do here */
}

/*ARGSUSED*/
void
plat_tod_fault(enum tod_fault_type tod_bad)
{
}
