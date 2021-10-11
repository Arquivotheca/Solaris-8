/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.166	99/10/22 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
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

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/modctl.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/clock.h>
#include <sys/tss.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/mmu.h>
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
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/machlock.h>
#include <sys/x_call.h>
#include <sys/instance.h>

#include <sys/time.h>
#include <sys/smp_impldefs.h>
#include <sys/psm_types.h>
#include <sys/atomic.h>
#include <sys/panic.h>

/*
 * The panicbuf array is used to record messages and state:
 */
#pragma align 8(panicbuf)
char panicbuf[PANICBUFSIZE];

/*
 * maxphys - used during physio
 * klustsize - used for klustering by swapfs and specfs
 */
int maxphys = 56 * 1024;    /* XXX See vm_subr.c - max b_count in physio */
int klustsize = 56 * 1024;

caddr_t	p0_va;		/* Virtual address for accessing physical page 0 */
int	pokefault = 0;

/*
 * defined here, though unused on x86,
 * to make kstat_fr.c happy.
 */
int vac;

struct debugvec *dvec = (struct debugvec *)0;
struct cons_polledio *system_polledio;

void stop_other_cpus();
void debug_enter(char *);

int	procset = 1;

/*
 * Flags set by mdboot if we're panicking and we invoke mdboot on a CPU which
 * is not the boot CPU.  When set, panic_idle() on the boot CPU will invoke
 * mdboot with the corresponding arguments.
 */

#define	BOOT_WAIT	-1		/* Flag indicating we should idle */

volatile int cpu_boot_cmd = BOOT_WAIT;
volatile int cpu_boot_fcn = BOOT_WAIT;

/*
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *mdep)
{
	extern void reset_leaves(void);
	extern void mtrr_resync(void);

	/*
	 * The PSMI guarantees the implementor of psm_shutdown that it will
	 * only be called on the boot CPU.  This was needed by Corollary
	 * because the hardware does not allow other CPUs to reset the
	 * boot CPU.  So before rebooting, we switch over to the boot CPU.
	 * If we are panicking, the other CPUs are at high spl spinning in
	 * panic_idle(), so we set the cpu_boot_* variables and wait for
	 * the boot CPU to re-invoke mdboot() for us.
	 */
	if (!panicstr) {
		kpreempt_disable();
		affinity_set(getbootcpuid());
	} else if (CPU->cpu_id != getbootcpuid()) {
		cpu_boot_cmd = cmd;
		cpu_boot_fcn = fcn;
		for (;;);
	}

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	/*
	 * Print the reboot message now, before pausing other cpus.
	 * There is a race condition in the printing support that
	 * can deadlock multiprocessor machines.
	 */
	if (!(fcn == AD_HALT || fcn == AD_POWEROFF))
		prom_printf("rebooting...\n");

	(void) spl6();
	reset_leaves();		/* call all drivers reset entry point	*/

	if (!panicstr) {
		mutex_enter(&cpu_lock);
		pause_cpus(NULL);
		mutex_exit(&cpu_lock);
	}
	(void) spl8();
	(*psm_shutdownf)(cmd, fcn);

	mtrr_resync();

	if (fcn == AD_HALT || fcn == AD_POWEROFF)
		halt((char *)NULL);
	else
		prom_reboot("");
	/*NOTREACHED*/
}

void
idle_other_cpus()
{
	int cpuid = CPU->cpu_id;
	cpuset_t xcset;

	ASSERT(cpuid < NCPU);
	CPUSET_ALL_BUT(xcset, cpuid);
	xc_capture_cpus(xcset);
}

void
resume_other_cpus()
{
	ASSERT(CPU->cpu_id < NCPU);

	xc_release_cpus();
}

extern void	mp_halt(char *);

void
stop_other_cpus()
{
	int cpuid = CPU->cpu_id;
	cpuset_t xcset;

	ASSERT(cpuid < NCPU);

	/*
	 * xc_call_debug will make all other cpus to execute mp_halt and
	 * then return immediately without waiting for acknowledgment
	 */
	CPUSET_ALL_BUT(xcset, cpuid);
	xc_call_debug(NULL, NULL, NULL, X_CALL_HIPRI, xcset,
	    (int (*)())mp_halt);
}

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable != 0)
		debug_enter(msg);
}

/*
 * Enter debugger.  Called when the user types ctrl-alt-d or whenever
 * code wants to enter the debugger and possibly resume later.
 */
void
debug_enter(msg)
	char	*msg;		/* message to print, possibly NULL */
{
	int s;

	if (msg)
		prom_printf("%s\n", msg);

	if (boothowto & RB_DEBUG) {
		s = clear_int_flag();
		int20();
		restore_int_flag(s);
	}
}

void
reset(void)
{
	extern	void pc_reset(void);
	ushort_t *bios_memchk;

	if (bios_memchk = (ushort_t *)psm_map_phys(0x472, sizeof (ushort_t),
		PROT_READ|PROT_WRITE))
		*bios_memchk = 0x1234;	/* bios memmory check disable */

	pc_reset();
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
	stop_other_cpus();	/* send stop signal to other CPUs */
	if (s)
		prom_printf("(%s) \n", s);
	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Enter monitor.  Called via cross-call from stop_other_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);

	/*CONSTANTCONDITION*/
	while (1);
}

#ifndef SAS
/*
 * Console put and get character routines.
 * XXX	NEEDS REVIEW w.r.t MP (Sherif?)
 *	(Also fix all the calls to this routine in this file.)
 */
/*ARGSUSED1*/
void
cnputc(int c, int device_in_use)
{
	int s;

	s = splzs();
	if (c == '\n')
		prom_putchar('\r');
	prom_putchar(c);
	splx(s);
}

/* ARGSUSED2 */
void
cnputs(const char *str, unsigned int len, int device_in_use)
{
	int s;

	s = splzs();
	for (; len != 0; len--, str++) {
		if (*str == '\n')
			prom_putchar('\r');
		prom_putchar(*str);
	}
	splx(s);
}

int
cngetc()
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 */
getchar()
{
	int c;

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
	char *lp;
	int c;

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

		default:
			*lp++ = (char)c;
			break;
		}
	}
}

#endif !SAS

/*
 * Allocate threads and stacks for interrupt handling.
 */
#define	NINTR_THREADS	(LOCK_LEVEL-1)	/* number of interrupt threads */

void
init_intr_threads(cp)
	struct cpu *cp;
{
	int i;

	for (i = 0; i < NINTR_THREADS; i++)
		(void) thread_create_intr(cp);

	cp->cpu_intr_stack = (caddr_t)segkp_get(segkp, INTR_STACK_SIZE,
		KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED) +
		INTR_STACK_SIZE - SA(MINFRAME);
}

/*
 * XXX These probably ought to live somewhere else
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

#ifdef	NM_DEBUG
int nmi_test = 0;	/* checked in intentry.s during clock int */
int nmtest = -1;
nmfunc1(arg, rp)
int	arg;
struct regs *rp;
{
	printf("nmi called with arg = %x, regs = %x\n", arg, rp);
	nmtest += 50;
	if (arg == nmtest) {
		printf("ip = %x\n", rp->r_eip);
		return (1);
	}
	return (0);
}

#endif

/*
 * Return the number of ticks per second of the highest
 * resolution clock or free running timer in the system.
 * XXX - if/when we provide higher resolution data, update this.
 */
getclkfreq()
{
	return (hz);
}

/* XXX stubbed stuff that is mainly here for debugging */
/* XXX   BOOTOPS definition until we get the new boot */
/* XXX   Things we cannot afford to forget! */



#include <sys/bootsvcs.h>

/* Hacked up initialization for initial kernel check out is HERE. */
/* The basic steps are: */
/*	kernel bootfuncs definition/initialization for KADB */
/*	kadb bootfuncs pointer initialization */
/*	putchar/getchar (interrupts disabled) */

/* kadb bootfuncs pointer initialization */

/*
 *	Redirect KADB's pointer to bootfuncs into the kernel,
 *	when the boot is about to go away.
 *	Make the kernel's pointer to boot services NULL.
 */

static void kadb_error(int n);

static char *
kadb_error1()
{
	kadb_error(1);
	return (NULL);
}

static char *
kadb_error2()
{
	kadb_error(2);
	return (NULL);
}

static char *
kadb_error3()
{
	kadb_error(3);
	return (NULL);
}

static size_t
kadb_error4()
{
	kadb_error(4);
	return (0);
}

static void *
kadb_error5()
{
	kadb_error(5);
	return (NULL);
}

static int
kadb_error6()
{
	kadb_error(6);
	return (0);
}

static int
kadb_error10()
{
	kadb_error(10);
	return (0);
}

static int
kadb_error11()
{
	kadb_error(11);
	return (0);
}

static void *
kadb_error12()
{
	kadb_error(12);
	return (0);
}

static int
kadb_error13()
{
	kadb_error(13);
	return (0);
}

static ssize_t
kadb_error14()
{
	kadb_error(14);
	return (0);
}

static off_t
kadb_error15()
{
	kadb_error(15);
	return (0);
}

static int
kadb_error16()
{
	kadb_error(16);
	return (0);
}

static int
kadb_error17()
{
	kadb_error(17);
	return (0);
}

static char *
kadb_error18()
{
	kadb_error(18);
	return (NULL);
}

static char *
kadb_error19()
{
	kadb_error(19);
	return (NULL);
}

static uint_t
kadb_error20()
{
	kadb_error(20);
	return (0);
}

static void
kadb_error(int n)
{
	prom_printf("Kadb service routine number %d called in ERROR!\n", n);
}

int
sysp_getchar()
{
	int i;
	int s;

	if (system_polledio == NULL) {
		/* Uh oh */
		prom_printf("getchar called with no console\n");
		for (;;)
			/* LOOP FOREVER */;
	}

	s = clear_int_flag();
	i = system_polledio->cons_polledio_getchar(
		system_polledio->cons_polledio_argument);
	restore_int_flag(s);
	return (i);
}

void
sysp_putchar(int c)
{
	int s;

	if (system_polledio == NULL) {
		/*
		 * We have no alternative but to drop the output on the floor.
		 */
		return;
	}

	s = clear_int_flag();
	system_polledio->cons_polledio_putchar(
		system_polledio->cons_polledio_argument, c);
	restore_int_flag(s);
}

int
sysp_ischar()
{
	int i;
	int s;

	if (system_polledio == NULL)
		return (0);

	s = clear_int_flag();
	i = system_polledio->cons_polledio_ischar(
		system_polledio->cons_polledio_argument);
	restore_int_flag(s);
	return (i);
}

static void
sysp_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	prom_printf(fmt, ap);
	va_end(ap);
}

int
goany(void)
{
	prom_printf("Type any key to continue ");
	(void) prom_getchar();
	prom_printf("\n");
	return (1);
}

static struct boot_syscalls kern_sysp = {
	sysp_printf,	/*	int	(*printf)();	0  */
	kadb_error1,	/*	char	*(*strcpy)();	1  */
	kadb_error2,	/*	char	*(*strncpy)();	2  */
	kadb_error3,	/*	char	*(*strcat)();	3  */
	kadb_error4,	/*	size_t	(*strlen)();	4  */
	kadb_error5,	/*	char	*(*memcpy)();	5  */
	kadb_error6,	/*	char	*(*memcmp)();	6  */
	sysp_getchar,	/*	unchar	(*getchar)();	7  */
	sysp_putchar,	/*	int	(*putchar)();	8  */
	sysp_ischar,	/*	int	(*ischar)();	9  */
	kadb_error10,	/*	int	(*goany)();	10 */
	kadb_error11,	/*	int	(*gets)();	11 */
	kadb_error12,	/*	int	(*memset)();	12 */
	kadb_error13,	/*	int	(*open)();	13 */
	kadb_error14,	/*	int	(*read)();	14 */
	kadb_error15,	/*	int	(*lseek)();	15 */
	kadb_error16,	/*	int	(*close)();	16 */
	kadb_error17,	/*	int	(*fstat)();	17 */
	kadb_error18,	/*	char	*(*malloc)();	18 */
	kadb_error19,	/*	char	*(*get_fonts)();19 */
	kadb_error20,	/*	uint_t (*vlimit)();	20 */
};

void
kadb_uses_kernel()
{
	/*
	 * This routine is now totally misnamed, since it does not in fact
	 * control kadb's I/O; it only controls the kernel's prom_* I/O.
	 */
	sysp = &kern_sysp;
}

/*
 *	the interface to the outside world
 */

/*
 * poll_port -- wait for a register to achieve a
 *		specific state.  Arguments are a mask of bits we care about,
 *		and two sub-masks.  To return normally, all the bits in the
 *		first sub-mask must be ON, all the bits in the second sub-
 *		mask must be OFF.  If about seconds pass without the register
 *		achieving the desired bit configuration, we return 1, else
 *		0.
 */
int
poll_port(ushort_t port, ushort_t mask, ushort_t onbits,
							ushort_t offbits)
{
	int i;
	ushort_t maskval;

	for (i = 500000; i; i--) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
			((maskval & offbits) == 0))
			return (0);
		drv_usecwait(10);
	}
	return (1);
}

int
getlongprop(id, prop)
char *prop;
{
	if (id == 0) {
		if (strcmp(prop, "name") == 0)
			return ((int)"i86pc");
		prom_printf("getlongprop: root property '%s' not defined.\n",
			prop);
	}
	return (0);
}

int ticks_til_clock;
int unix_tick;

/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*LINTED: static unused */
static uint_t last_idle_cpu;

/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{
	last_idle_cpu = cpun;
	(*psm_set_idle_cpuf)(cpun);
}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{
	(*psm_unset_idle_cpuf)(cpun);
}

/*
 * This routine is almost correct now, but not quite.  It still needs the
 * equivalent concept of "hres_last_tick", just like on the sparc side.
 * The idea is to take a snapshot of the hi-res timer while doing the
 * hrestime_adj updates under hres_lock in locore, so that the small
 * interval between interrupt assertion and interrupt processing is
 * accounted for correctly.  Once we have this, the code below should
 * be modified to subtract off hres_last_tick rather than hrtime_base.
 *
 * I'd have done this myself, but I don't have source to all of the
 * vendor-specific hi-res timer routines (grrr...).  The generic hook I
 * need is something like "gethrtime_unlocked()", which would be just like
 * gethrtime() but would assume that you're already holding CLOCK_LOCK().
 * This is what the GET_HRTIME() macro is for on sparc (although it also
 * serves the function of making time available without a function call
 * so you don't take a register window overflow while traps are diasbled).
 */
extern volatile hrtime_t hres_last_tick;
void
pc_gethrestime(timestruc_t *tp)
{
	int lock_prev;
	timestruc_t now;
	int nslt;		/* nsec since last tick */
	int adj;		/* amount of adjustment to apply */

loop:
	lock_prev = hres_lock;
	now = hrestime;
	nslt = (int)(gethrtime() - hres_last_tick);
	if (nslt < 0)
		nslt += nsec_per_tick;
	now.tv_nsec += nslt;
	if (hrestime_adj != 0) {
		if (hrestime_adj > 0) {
			adj = (nslt >> ADJ_SHIFT);
			if (adj > hrestime_adj)
				adj = (int)hrestime_adj;
		} else {
			adj = -(nslt >> ADJ_SHIFT);
			if (adj < hrestime_adj)
				adj = (int)hrestime_adj;
		}
		now.tv_nsec += adj;
	}
	if (now.tv_nsec >= NANOSEC) {
		now.tv_nsec -= NANOSEC;
		now.tv_sec++;
	}
	if ((hres_lock & ~1) != lock_prev)
		goto loop;

	*tp = now;
}

/*
 * Initialize kernel thread's stack
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
	caddr_t oldstk;

	oldstk = stk;
	stk -= SA(sizeof (struct regs) + MINFRAME);
	bzero((caddr_t)stk, (size_t)(oldstk - stk));
	lwp->lwp_regs = (void *)(stk + REGOFF);

	return (stk);

}

/*ARGSUSED*/
void
lwp_stk_fini(klwp_t *lwp)
{}

/*
 * If we're not the panic CPU, we wait in panic_idle for reboot.  If we're
 * the boot CPU, then we are responsible for actually doing the reboot, so
 * we watch for cpu_boot_cmd to be set.
 */
static void
panic_idle(void)
{
	(void) splx(ipltospl(CLOCK_LEVEL));
	(void) setjmp(&curthread->t_pcb);

	if (CPU->cpu_id == getbootcpuid()) {
		while (cpu_boot_cmd == BOOT_WAIT || cpu_boot_fcn == BOOT_WAIT)
			drv_usecwait(10);

		mdboot(cpu_boot_cmd, cpu_boot_fcn, NULL);
	}

	for (;;);
}

/*
 * Stop the other CPUs by cross-calling them and forcing them to enter
 * the panic_idle() loop above.
 */
/*ARGSUSED*/
void
panic_stopcpus(cpu_t *cp, kthread_t *t, int spl)
{
	processorid_t i;
	cpuset_t xcset;

	(void) splzs();

	CPUSET_ALL_BUT(xcset, cp->cpu_id);
	xc_call_debug(NULL, NULL, NULL, X_CALL_HIPRI, xcset,
	    (int (*)())panic_idle);

	for (i = 0; i < NCPU; i++) {
		if (i != cp->cpu_id && cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS))
			cpu[i]->cpu_flags |= CPU_QUIESCED;
	}
}

/*
 * Platform-specific code to execute after panicstr is set: we invoke
 * the PSM entry point to indicate that a panic has occurred.
 */
/*ARGSUSED*/
void
panic_quiesce_hw(panic_data_t *pdp)
{
	psm_notifyf(PSM_PANIC_ENTER);
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
