/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.275	99/10/22 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/syserr.h>
#include <sys/user.h>
#include <sys/vmem.h>
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
#include <sys/processor.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/panic.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/modctl.h>

#include <sys/consdev.h>
#include <sys/frame.h>
#include <sys/psr.h>
#include <sys/machpcb.h>

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/spl.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/trap.h>
#include <sys/led.h>
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
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <vm/hat_srmmu.h>
#include <sys/iommu.h>
#include <sys/vtrace.h>
#include <sys/autoconf.h>
#include <sys/instance.h>
#include <sys/bt.h>
#include <sys/aflt.h>
#include <sys/physaddr.h>

extern struct cpu cpu0;				/* gotta have one to start */
extern struct cpu *cpu[];			/* per-cpu generic data */

static void idle_other_cpus(void);
static void resume_other_cpus(void);
static void stop_other_cpus(void);
void power_down(char *);

int do_stopcpu;		/* we are running FCS and later PROM, do prom_stopcpu */

int __cg92_used;	/* satisfy C compiler reference for -xcg92 option */

extern int	initing;
extern uint_t	nwindows;	/* computed in locore.s */
extern uint_t	last_idle_cpu;  /* computed in disp.c */

int do_pg_coloring = 1;		/* patch to 0 to disable */

/*
 * Default delay loop count is set for 70 MHz SuperSPARC processors.
 * This algorithm is (Max CPU MHZ/2) * .95
 */
int Cpudelay = 34;		/* delay loop count/usec */

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

int maxphys = 124 << 10;	/* XXX What is this used for? */
int klustsize = 124 << 10;	/* XXX "		    " */

/*
 * Most of these vairables are set to indicate whether or not a particular
 * architectural feature is implemented.  If the kernel is not configured
 * for these features they are #defined as zero. This allows the unused
 * code to be eliminated by dead code detection without messy ifdefs all
 * over the place.
 */

int vac = 0;			/* virtual address cache type (none == 0) */

/*
 * Globals for asynchronous fault handling
 */

int	pokefault = 0;

/*
 * cpustate is used to keep track of which cpu is running
 */
char	cpustate[NCPU];

/*
 * A dummy location where the flush_writebuffers routine
 * can perform a swap in order to flush the module write buffer.
 */
uint_t	module_wb_flush;

extern dev_info_t *cpu_get_dip();

/*
 * Sun4d has its own way of doing software power off.
 */
extern	int	power_off_is_supported(void);

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

	/*
	 * we don't call start_mon_clock() here since we need L14
	 * vector for OBP MBox to work.
	 */

#if defined(SAS) || defined(MPSAS)
	asm("t 255");
#else
	/* extreme priority; allow clock interrupts to monitor at level 14 */
	s = spl6();
	reset_leaves(); 		/* try and reset leaf devices */
	if (fcn == AD_HALT) {
		halt((char *)NULL);
		fcn &= ~RB_HALT;
		/* MAYBE REACHED */
	} else if (fcn == AD_POWEROFF) {
		if (power_off_is_supported())
			power_down(NULL);
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
		} else if (*bootstr == '/') {
			/* take care of any devfs->prom device name mappings */
			bootstr = convert_boot_device_name(bootstr);
		}
		led_set_cpu(CPU->cpu_id, 0x7);
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
	dev_info_t *dip;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if ((i != cpuid) && (dip = cpu_get_dip(i))) {
			(void) prom_idlecpu((dnode_t)ddi_get_nodeid(dip));
		}
	}
}

static void
resume_other_cpus(void)
{
	int i;
	int cpuid = CPU->cpu_id;
	dev_info_t *dip;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if ((i != cpuid) && (dip = cpu_get_dip(i))) {
			(void) prom_resumecpu((dnode_t)ddi_get_nodeid(dip));
		}
	}
}

static void
stop_other_cpus(void)
{
	int i;
	int cpuid = CPU->cpu_id;
	dev_info_t *dip;

	ASSERT(cpuid <= NCPU);

	for (i = 0; i < NCPU; i++) {
		if ((i != cpuid) && (dip = cpu_get_dip(i))) {
			(void) prom_stopcpu((dnode_t)ddi_get_nodeid(dip));
		}
	}

	/* verify all cpus are stopped properly */
	drv_usecwait(1500000);		/* wait for 1.5 sec */

	for (i = 0; i < NCPU; i++) {
		if ((i != cpuid) && (dip = cpu_get_dip(i)) &&
		    !(cpustate[i] & CPU_IN_OBP)) {
			printf("\ncpu %d was not stopped \n", i);
		}
	}
}

/*
 *	Machine dependent abort sequence handling
 */

#define		SR1_KEYPOS		(1 << 4) /* fourth bit */

void
abort_sequence_enter(char *msg)
{
	if ((abort_enable != 0) &&
	    (!(xdb_bb_status1_get() & SR1_KEYPOS))) {
		debug_enter(msg);
		(void) intr_clear_pend_local(SPLTTY);
	}
}

/*
 * Enter debugger.  Called when the user types L1-A or break or whenever
 * code wants to enter the debugger and possibly resume later.
 * If the debugger isn't present, enter the PROM monitor.
 */
void
debug_enter(char *msg)
{
	int s;

	if (msg)
		prom_printf("%s\n", msg);

	s = splzs();

	if (ncpus > 1 && panic_thread == NULL)
		idle_other_cpus();

	if (boothowto & RB_DEBUG) {
		label_t debug_save = curthread->t_pcb;
		(void) setjmp(&curthread->t_pcb);
		(*(vfunc_t)(&dvec->dv_entry))();
		curthread->t_pcb = debug_save;
	} else {
		led_set_cpu(CPU->cpu_id, 0x9);
		prom_enter_mon();
		led_set_cpu(CPU->cpu_id, LED_CPU_RESUME);
	}

	if (ncpus > 1 && panic_thread == NULL)
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
	if (ncpus > 1) {
		if (do_stopcpu)
			stop_other_cpus();
		else
			/* workaround for an OBP bug */
			idle_other_cpus();
	}

	if (s)
		prom_printf("(%s) ", s);
	led_set_cpu(CPU->cpu_id, 0x7);
	prom_exit_to_mon();
	/*NOTREACHED*/
}


void
power_down(char *s)
{
	flush_windows();
	if (ncpus > 1) {
		if (do_stopcpu)
			stop_other_cpus();
		else
			/* workaround for an OBP bug */
			idle_other_cpus();
	}

	if (s)
		prom_printf("(%s) ", s);
	power_off();	/* trip the circuit breaker */
	/*NOTREACHED*/
	led_set_cpu(CPU->cpu_id, 0x7);
	prom_exit_to_mon();
	/*NOTREACHED*/
}


/*
 * Write the scb, which is the first page of the kernel.
 * Normally it is write protected, we provide a function
 * to fiddle with the mapping temporarily.
 *	1) lock out interrupts
 *	2) change protections to make writable
 *	3) write the desired vector into the scb
 *	4) change protections to make write-protected
 *	5) restore interrupts to their previous state
 */
void
write_scb_int(int level, struct trapvec *ip)
{
	register int s;
	register trapvec *sp;
	caddr_t addr;

	sp = &scb.interrupts[level - 1];
	s = spl8();

	addr = (caddr_t)((uint_t)sp & PAGEMASK);

	rw_enter(&kas.a_lock, RW_READER);
	hat_chgprot(kas.a_hat, addr, PAGESIZE,
	    PROT_READ | PROT_WRITE | PROT_EXEC);

	/* write out new vector code */
	*sp = *ip;

	hat_chgprot(kas.a_hat, addr, PAGESIZE, PROT_READ | PROT_EXEC);
	rw_exit(&kas.a_lock);

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
/*ARGSUSED2*/
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
		 * the others in their tracks.  The usual result is
		 * a completely wedged system which requires a hard reset.
		 *
		 * This is an unfortunate 'hackaround' to the general
		 * problem of sharing a device between the OBP and userland.
		 *
		 * Fortunately, this should happen -very- rarely on a
		 * running system provided you have a console window
		 * redirecting console output when running your
		 * favourite window system ;-)
		 */
		s = splhi();
		idle_other_cpus();
		prom_writestr(buf, bufsize);
		resume_other_cpus();
		splx(s);
	} else
		prom_writestr(buf, bufsize);
}

/*ARGSUSED1*/
void
cnputc(register int c, int device_in_use)
{
	int s;

	if (device_in_use) {
		s = splhi();
		idle_other_cpus();
	}
	if (c == '\n')
		prom_putchar('\r');
	prom_putchar(c);
	if (device_in_use) {
		resume_other_cpus();
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
static char
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
			/* FALLTHROUGH */
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/* FALLTHROUGH */
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
	int s;

	reestablish_curthread();
	s = splhigh();
	oldtbr = set_tbr(&scb);
	splx(s);

	if (strcmp(str, "sync") == 0) {
		nopanicdebug = 1;	/* don't try to re-enter kadb or prom */
		panic("zero");		/* force a panic */
	} else
		prom_printf("Don't understand '%s'\n", str);

	s = splhigh();
	(void) set_tbr(oldtbr);
	splx(s);
}

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

/*
 * MXCC Control knobs (pg 38)
 * SuperSPARC MCNTL knobs (pg 96)
 * SuperSPARC breakpoint ACTION knobs (pg 129)
 * BusWatcher Prescaler (pg 61)
 * BootBus Semaphore 0 (pg 17)
 */
static int option_print = 1;
char *mxcc_cntrl_bits = "\20\12RC\7WI\6PF\5MC\4PE\3CE\2CS\1HC";
char *vik_mcntl_bits = "\20\23PF\21TC\20AC\17SE\16BT\15PE\14MB\13SB"
	"\12IE\11DE\10PSO\2NF\1EN";
char *vik_action_bits = "\20\15MIX"
	"\10STEN_CBK\7STEN_ZIC\6STEN_DBK\5STEN_ZCC"
	"\4IEN_CBK\3IEN_ZIC\2IEN_DBK\1IEN_ZCC";

extern uint_t intr_mxcc_cntrl_get(void);
extern void intr_mxcc_cntrl_set(uint_t value);
extern uint_t intr_vik_mcntl_get(void);
extern void intr_vik_mcntl_set(uint_t value);
extern int intr_bb_sema_status(void);

void
check_options(int master)
{
	struct cpu *cp = CPU;
	uint_t cpu_id = cp->cpu_id;
	uint_t mxcc_cntl = intr_mxcc_cntrl_get();
	uint_t mcntl = intr_vik_mcntl_get();
	uint_t action = intr_vik_action_get();
	uint_t prescaler = intr_prescaler_get();
	uint_t freq = 256 - ((prescaler >> 8) & 0xff);
	uint_t sema = (uint_t)intr_bb_sema_status();
	uint_t id_sb = sema & 0x3;
	uint_t held = (id_sb & 1) ? ((id_sb >> 1) == (cpu_id & 1)) : 0;

	if (option_print == 0) {
		return;	/* don't print anything */
	}

	cmn_err(CE_CONT, "?cpu%d:"
		"\tBW Prescaler/XDBus Frequency=%dMHz\n"
		"\tBootbus Semaphore %sheld (0x%x)\n"
		"\tMXCC control register=0x%b\n"
		"\tSuperSPARC MCNTL register=0x%b\n"
		"\tSuperSPARC ACTION register=0x%b\n",
		cpu_id, freq, held ? "" : "NOT ", sema,
		(long)mxcc_cntl, mxcc_cntrl_bits,
		(long)mcntl, vik_mcntl_bits,
		(long)action, vik_action_bits);

	/*
	 * I know this is gross, but it's only temporary....
	 */
	if (master)

#if defined(VIKING_BUG_MFAR2) || \
	defined(VIKING_BUG_1151159) || defined(VIKING_BUG_PTP2)
		cmn_err(CE_CONT, "?kernel contains workarounds for "
		    "SuperSPARC bugs: "

#ifdef VIKING_BUG_MFAR2
		    "MFAR2 "
#endif
#ifdef VIKING_BUG_1151159
		    "1151159 "
#endif
#ifdef VIKING_BUG_PTP2
		    "PTP2 "
#endif
		    "\n");
#endif

	/* printf("	BCIPL=%x\n",	bitfield(action, 8, (12-8))); */
	/* printf("	DBC=%x\n",	bitfield(mxcc_cntl, 7, (9 - 7))); */
	/* printf("	Impl=%x\n",	bitfield(mcntl, 28, (32-28))); */
	/* printf("	Ver=%x\n",	bitfield(mcntl, 24, (28-24))); */
	/* printf("	rsvd17=%x\n",	bitfield(mcntl, 17, (18 - 17))); */
	/* printf("	rsvd19=%x\n",	bitfield(mcntl, 23, (24 - 19))); */
	/* printf("	rsvd2=%x\n",	bitfield(mcntl,  2, ( 7 -  2))); */
}

/*
 * indicates lowest revision cpu in this system.
 * used for dynamic workaround handling.
 */
int cpu_revision = 0;

/*
 * keep track of the revisions per cpu for debug or so we
 * can check with adb on a running kernel.
 */
static char cpu_revs[NCPU];

/*
 * SuperSPARC bug workarounds the kernel is able to handle
 */
int mfar2_bug = 0;		/* SuperSPARC bug MFAR2 */
extern int enable_mbit_wa;	/* see bug id 1220902 (viking tlb.m bug) */

/*
 * SuperSPARC PTP2 bug:
 * PTP2 is the second level page table pointer that is cached by Viking.
 * It is used to access tables of 4K PTEs.  A DEMAP REGION is used to
 * flush the TLB of all entries matching VA 31:24, and can come from
 * outside in systems that support demaps over the bus, or can be an
 * internal TLB FLUSH instruction.
 *
 * TLB entries are all flushed correctly, but the PTP2 is not always
 * invalidated.  PTP2 is only invalidated if VA 31:18 match, which is a
 * stronger condition than REGION DEMAP, that being VA 31:24 match.
 *
 * It is possible that an OS remapping memory could issue a REGION flush,
 * but the old PTP2 could later be used to fetch a PTE from the old page
 * table.
 *
 * CONTEXT, SEGMENT, and PAGE demaps correctly invalidate PTP2.
 */
int viking_ptp2_bug = 0;	/* SuperSPARC bug PTP2 */

/*
 * enable_sm_wa allows manually enabling SuperSPARC
 * bug 1151159, i.e. via /etc/system
 * the name of this variable matches the name used for sun4m
 */
int enable_sm_wa = 0;

/*
 * disable_1151159 provides the ability to disable SuperSPARC
 * bug 1151159 regardless of all other settings.
 *
 * for disable_1151159 to take effect for the boot cpu, it must
 * be set to a non-zero value before the boot cpu executes
 * set_cpu_revision(). For example, setting disable_1151159=1 in
 * /etc/system is too late but setting it via kadb as early as
 * possible would work.
 */
int disable_1151159 = 0;

/*
 * Based on the mask version and implementation fields of the
 * processor status word and on the version and implementation
 * fields of the module control register, try to determine the
 * version of SuperSPARC processor.
 *
 * Due to the way these fields are maintained, exact version
 * determination is impossible.
 *
 * The following table provides the necessary information for
 * determining the SuperSPARC processor version.
 *
 * +-------+----------+------------+-----------+------------+----------+
 * | Rev   | PSR.IMPL | MCNTL.IMPL | MCNTL.VER |  JTAG.CID  | FSR.VER  |
 * |       | (4 bits) | (4 bits)   | (4 bits)  |  (32 bits) | (3 bits) |
 * |       | PSR.VER  |            |           |            |          |
 * |       | (4 bits) |            |           |            |          |
 * +-------+----------+------------+-----------+------------+----------+
 * | 1.x   |   0x40   |     0      |     0     | 0x0000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 2.x   |   0x41   |     0      |     0     | 0x0000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 3.x   |   0x40   |     0      |     1     | 0x1000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 4.x   |   0x40   |     0      |     2     | 0x2000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 5.x   |   0x40   |     0      |     3     | 0x3000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 3.5   |   0x40   |     0      |     4     | 0x1000402F |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 *	SuperSPARC2 (Voyager)
 * +-------+----------+------------+-----------+------------+----------+
 * | 1.x   |   0x40   |     0      |     8     | 0x0001602f |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 * | 2.x   |   0x40   |     0      |     9     | 0x1001602f |     0    |
 * +-------+----------+------------+-----------+------------+----------+
 *
 */

#define	SSPARC_CPU_NAMELEN	PI_TYPELEN
#define	SSPARC_FPU_NAMELEN	PI_FPUTYPE

struct supersparc_version {
	uint_t	psr_impl;
	uint_t	psr_ver;
	uint_t	mcr_impl;
	uint_t	mcr_ver;
	char	cpu_name[SSPARC_CPU_NAMELEN];
	char	fpu_name[SSPARC_FPU_NAMELEN];
} ss_version[] = {
	{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, "SuperSPARC",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x00000000, "SuperSPARC 1.2",
	"SuperSPARC" },
	{ 0x40000000, 0x01000000, 0x00000000, 0x00000000, "SuperSPARC 2.0",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x01000000, "SuperSPARC 3.0",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x02000000, "SuperSPARC 4.0",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x03000000, "SuperSPARC 5.0",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x04000000, "SuperSPARC 3.5",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x08000000, "SuperSPARC2 1.0",
	"SuperSPARC" },
	{ 0x40000000, 0x00000000, 0x00000000, 0x09000000, "SuperSPARC2 2.0",
	"SuperSPARC" },
};

/*
 * the following constants correspond to the entries in the
 * ss_version[] array, declared above.
 */
#define	SSPARC_REV_DEFAULT	0	/* default identification entry */
#define	SSPARC_REV_1DOT2	1
#define	SSPARC_REV_2DOT0	2
#define	SSPARC_REV_3DOT0	3
#define	SSPARC_REV_4DOT0	4
#define	SSPARC_REV_5DOT0	5
#define	SSPARC_REV_3DOT5	6
#define	SSPARC2_REV_1DOT0	7
#define	SSPARC2_REV_2DOT0	8


/*
 * set_cpu_version is called for each cpu in the system and
 * determines the version of SuperSPARC processor and which
 * dynamic bug workarounds are necessary.
 * in the event of mixed SuperSPARC versions on an MP system,
 * variables are set to reflect a union of the bugs present.
 */
void
set_cpu_revision(void)
{
	/*LINTED: set but not used - used in some ifdefs */
	char *pstr2 = "kernel needs workaround for SuperSPARC"
			" %s bug for revision %d cpu %d\n";
	int enable_1151159 = 0;		/* SuperSPARC bug 1151159 */
	uint_t psr = getpsr();
	uint_t mcr = intr_vik_mcntl_get();
	uint_t version;
	int i;

	/*
	 * determine this cpu's SuperSPARC version
	 */
	version = cpu_revs[CPU->cpu_id] = SSPARC_REV_DEFAULT;
	for (i = 0; i < (sizeof (ss_version) / sizeof (ss_version[0])); i++) {
		if ((ss_version[i].psr_impl == (psr & PSR_IMPL)) &&
		    (ss_version[i].psr_ver == (psr & PSR_VER)) &&
		    (ss_version[i].mcr_impl  == (mcr & MCR_IMPL)) &&
		    (ss_version[i].mcr_ver  == (mcr & MCR_VER))) {
			cpu_revs[CPU->cpu_id] = version = i;
			break;
		}
	}

	/*
	 * based on the version of SuperSPARC cpu,
	 * determine which workarounds are necessary
	 */
	switch (version) {
		case SSPARC_REV_1DOT2:
			cmn_err(CE_PANIC, "SuperSparc 1.X is no longer"
			    " supported.");
			/* NOTREACHED */
			break;

		case SSPARC_REV_2DOT0:
			mfar2_bug = 1;
			enable_1151159 = 1;
			viking_ptp2_bug = 1;
			enable_mbit_wa = 1;
			break;
		case SSPARC_REV_3DOT0:
			/*
			 * The mfar2  bug is suppose to be fixed in this version
			 * of SuperSPARC but until we have a chance to verify
			 * that it is indeed fixed, let's be safe and make sure
			 * we have the code to handle the workaround enabled.
			 */
			mfar2_bug = 1;
			enable_1151159 = 1;
			viking_ptp2_bug = 1;
			enable_mbit_wa = 1;
			break;
		case SSPARC_REV_4DOT0:
		case SSPARC_REV_5DOT0:
		case SSPARC_REV_3DOT5:
			/*
			 * The mfar2  bug is suppose to be fixed in this version
			 * of SuperSPARC but until we have a chance to verify
			 * that it is indeed fixed, let's be safe and make sure
			 * we have the code to handle the workaround enabled.
			 */
			mfar2_bug = 1;
			viking_ptp2_bug = 1;
			enable_mbit_wa = 1;
			break;
		case SSPARC2_REV_1DOT0:
		case SSPARC2_REV_2DOT0:
		default:
			/* Unknown versions run without workarounds */
			break;
	}

	/*
	 * In the following checks, if the cpu_id is 0 then we assume
	 * we're too early in kernel initialization to call panic()
	 * and thus must use prom_panic().
	 */

#if !defined(VIKING_BUG_MFAR2)
	if (mfar2_bug) {
		if (CPU->cpu_id == cpu0.cpu_id) {
			prom_printf(pstr2, "mfar2", version, CPU->cpu_id);
			prom_panic("\n");
		} else {
			panic(pstr2, "mfar2", version, CPU->cpu_id);
		}
		/* NOTREACHED */
	}
#endif !VIKING_BUG_MFAR2

#if defined(VIKING_BUG_1151159)
	if ((enable_sm_wa || enable_1151159) && (disable_1151159 == 0)) {
		extern void vik_1151159_wa(void);

		vik_1151159_wa();
	}
#endif VIKING_BUG_1151159

#if !defined(VIKING_BUG_PTP2)
	if (viking_ptp2_bug) {
		if (CPU->cpu_id == cpu0.cpu_id) {
			prom_printf(pstr2, "ptp2", version, CPU->cpu_id);
			prom_panic("\n");
		} else {
			panic(pstr2, "ptp2", version, CPU->cpu_id);
		}
		/* NOTREACHED */
	}
#endif

	/*
	 * keep track of lowest revision cpu in this system.
	 */
	if (cpu_revision == 0 || version < cpu_revision)
		cpu_revision = version;
}

/*
 * search 1st level children in devinfo tree for io-unit
 * read CID's from each IOC, return 1 if old one found.
 */

int
need_ioc_workaround(void)
{
	dev_info_t	*dip;
	uint_t n_io_unit = 0;

	dip = ddi_root_node();

	for (dip = ddi_get_child(dip); dip; dip = ddi_get_next_sibling(dip)) {
		char	*name;
		int	devid;
		uint_t	b;

		name = ddi_get_name(dip);

		if (strcmp("io-unit", name))
			continue;

		n_io_unit += 1;

		if (prom_getprop((dnode_t)ddi_get_nodeid(dip),
		    PROP_DEVICE_ID, (caddr_t)&devid) == -1) {
			cmn_err(CE_WARN,
				"need_ioc_workaround(): "
				" no %s for %s", PROP_DEVICE_ID, name);
			continue;
		}

		for (b = 0; b < n_xdbus; ++b) {
			if (ioc_get_cid(devid, b) == IOC_CID_DW_BUG)
				return (1);
		}
	}

	/* found nothing */
	if (n_io_unit == 0) {
		cmn_err(CE_WARN, "no io-units found!");
		return (1);	/* assume worst case */
	}
	return (0);
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
}

/*ARGSUSED*/
void
lwp_stk_fini(klwp_t *lwp)
{}

/*
 * Stop all active CPUs except the current CPU using prom_stopcpu().  If
 * do_stopcpu is false, we use prom_idlecpu() to work around a PROM bug.
 */
/*ARGSUSED*/
void
panic_stopcpus(cpu_t *cp, kthread_t *t, int spl)
{
	dev_info_t *dip;
	int i;

	(void) splzs();

	for (i = 0; i < NCPU; i++) {
		if (i == cp->cpu_id)
			continue; /* Skip current CPU */

		if (cpu[i] != NULL && (cpu[i]->cpu_flags & CPU_EXISTS)) {
			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
		}

		if ((dip = cpu_get_dip(i)) != NULL) {
			dnode_t dn = (dnode_t)ddi_get_nodeid(dip);

			if (do_stopcpu)
				(void) prom_stopcpu(dn);
			else
				(void) prom_idlecpu(dn);
		}
	}
}

/*
 * Miscellaneous hardware-specific code to execute after panicstr is set by the
 * panic code: we make sure panicbuf changes are flushed out to main memory,
 * and redirect interrupts to the current CPU.
 */
/*ARGSUSED*/
void
panic_quiesce_hw(panic_data_t *pdp)
{
	vac_flush(panicbuf, PANICBUFSIZE);
	set_all_itr_by_cpuid(CPU->cpu_id);
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
