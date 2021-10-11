/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu_driver.c	1.141	99/08/03 SMI"

#include <sys/machtypes.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/xdb_inline.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/archsystm.h>
#include <sys/syserr.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/mmu.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/stack.h>
#include <sys/t_lock.h>
#include <sys/thread.h>
#include <sys/types.h>
#include <sys/vtrace.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/hat_srmmu.h>
#include <sys/iocache.h>
#include <sys/callb.h>

#include <sys/disp.h>
#include <sys/varargs.h>

#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

#include <sys/iommu.h>
#include <sys/led.h>

/*
 * startup code must initialize:
 *	cpu0.cpu_id = index of cpu that kernel booted upon;
 *	cpu[cpu0.cpu_id] = &cpu0;
 */
struct cpu cpu0;			/* gotta have one to start */
struct cpu *cpu[NCPU];			/* per-cpu generic data */

int hotp_critical_section = 0;

/*
 * random function prototypes?
 */
extern void cpu_startup();
extern void idle();
extern void init_intr_threads(struct cpu *);
extern int spl0();
extern void mxcc_knobs(void);
extern void bw_knobs(void);
extern void resetcpudelay(void);
extern void itr_round_robin(int);
extern void debug_flush_windows(void);

extern uint_t bootbusses;
extern int do_stopcpu;

/*
 * disable "start_other_cpus",
 * ie. 0 => single cpu only
 */
int use_mp = 1;

/*
 * Use this static variable to warn user only once that system contains
 * processors with different clock frequencies.
 * Changes to Cpudelay have been made, so mixed speed cpus should operate
 * properly with drv_usecwait (default is 1, so there's no warning).
 */
int warn_mixed_mhz = 1;

int cpu_take_sbus[NCPU];

/*
 * debugging output
 */
int cpu_debug = 0;
#define	CPU_PRINTF if (cpu_debug) printf


/*
 * Processor initialization
 *
 * OBP turns control over to cpu_startup with:
 *	xx. Supervisor Mode
 *	xx. Traps Enabled/Disabled?
 *	xx. MMU turned on
 *	xx. context table set to requested value
 * cpu_startup adds:
 *	xx. %tbr -> scb
 *	xx. THREAD_REG(%g7) valid
 *	xx. Traps Enabled
 *	xx. PIL = MAX
 *	xx. ret pc & fp of parent window valid
 */

extern void srmmu_ctxtab_to_dev_reg(struct regspec *context);

static void distribute_sbus_intr(void);
static int next_cpu_index(int hint);
static int next_sbus_index(int current);

static void
init_cpu(struct cpu *cpu)
{
	dev_info_t *cpu_get_dip();
	int cpu_id = cpu->cpu_id;
	dev_info_t *dip;
	dnode_t nodeid;
	int whichcontext = 0;
	caddr_t pc = (caddr_t)cpu_startup;
	struct regspec context;
	int rc;

	if (dip = cpu_get_dip(cpu_id)) {
		nodeid = (dnode_t)ddi_get_nodeid(dip);
	} else {
		cmn_err(CE_CONT, "ERROR: cpu%d has not been attached, no dip\n",
			cpu_id);
		return;
	}

	srmmu_ctxtab_to_dev_reg(&context);
	rc = prom_startcpu(nodeid, (struct prom_reg *)&context,
	    whichcontext, pc);
	if (rc != 0) {
		cmn_err(CE_CONT, "ERROR: prom_startcpu(cpu%d)=0x%x\n"
			"\tnode=0x%x, context=0x%p, whichcontext=%d, pc=0x%p\n",
			cpu_id, rc, nodeid, (void *)&context,
			whichcontext, (void *)pc);
	}
}

/*
 * Dummy functions - no sun4d platforms support dynamic cpu allocation.
 */
/*ARGSUSED*/
int
mp_cpu_configure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*ARGSUSED*/
int
mp_cpu_unconfigure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

static int cpu_deferstart = 0;

/*
 * Startup function for an 'other' processor (besides OBP-Master).
 */
static void
mp_startup()
{
	struct cpu *self = CPU;
	int cpu_id = self->cpu_id;

	set_cpu_revision();

	/*
	 * apply use_mxcc_prefetch and use_multiple_cmds to this CPU
	 */
	mxcc_knobs();

	/*
	 * apply bw_lccnt to this CPU
	 */
	bw_knobs();

	/*
	 * try to recalibrate Cpudelay from this CPU
	 */
	resetcpudelay();

	(void) spl0();	/* enable interrupts */

	mutex_enter(&cpu_lock);
	self->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_ENABLE | CPU_EXISTS;
	cpu_add_active(self);
	mutex_exit(&cpu_lock);

	CPU_PRINTF("cpu%d activated\n", cpu_id);
	check_options(0);

	if (cpu_deferstart != 0) {
		mutex_enter(&cpu_lock);
		self->cpu_flags |= CPU_QUIESCED | CPU_OFFLINE;
		self->cpu_flags &= ~CPU_ENABLE;
		mutex_exit(&cpu_lock);
		cmn_err(CE_CONT, "cpu%d: going quiescent\n", cpu_id);
	} else {
		/*
		 * If this cpu owns a bootbus,
		 * enable level15 BootBus interrupts on it.
		 */
		/*
		 * XXX if we can take leve15's on a quiescent cpu,
		 * then this should be done also for cpu_deferstart != 0
		 */
		if (CPU_IN_SET(bootbusses, cpu_id))
			level15_enable_bbus(cpu_id);

		/*
		 * Non-boot cpu's come out of OBP with level15's masked.
		 * unmask them here.
		 */
		intr_clear_mask_bits(1 << 15);
	}

	thread_exit();
	cmn_err(CE_PANIC, "mp_startup: may not complete");
	/*NOTREACHED*/
}

/*
 * fresh_thread - panic if none available
 */
static kthread_id_t
fresh_thread(void (*func)(), int stksize, int pri)
{
	kthread_id_t me = curthread;
	proc_t *pp = me->t_procp;
	kthread_id_t tp = thread_create(NULL, stksize, func, NULL, 0,
		pp, TS_STOPPED, pri);

	if (tp == NULL) {
		cmn_err(CE_PANIC, "fresh_thread: can't allocate new thread");
		/*NOTREACHED*/
	}

	tp->t_stk = thread_stk_init(tp->t_stk);
	return (tp);
}

static kthread_id_t
cpu_startup_init(struct cpu *cpu)
{
	kthread_id_t tp = fresh_thread(mp_startup, NULL, maxclsyspri);

	tp->t_bound_cpu = cpu;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cpu;
	THREAD_ONPROC(tp, cpu);
	/* tp->t_sp = (uint_t)((struct rwindow *)sp - 1); */
	/* tp->t_pc = (uint_t)(mp_startup - 8; */
	return (tp);
}

static kthread_id_t
cpu_idle_init(struct cpu *cpu)
{
	kthread_id_t tp = fresh_thread(idle, PAGESIZE, -1);

	tp->t_preempt = 1;
	tp->t_bound_cpu = cpu;
	tp->t_affinitycnt = 1;
	/*
	 * Registering a thread in the callback table is usually
	 * done in the initialization code of the thread. In this
	 * case, we do it right after thread creation to avoid
	 * blocking idle thread while registering itself. It also
	 * avoids the possibility of reregistration in case a CPU
	 * restarts its idle thread.
	 */
	CALLB_CPR_INIT_SAFE(tp, "idle");
	THREAD_ONPROC(tp, cpu);
	return (tp);
}

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
static void
init_cpu_info(struct cpu *cp)
{
	dev_info_t *cpu_get_dip();
	processor_info_t *pi = &cp->cpu_type_info;

	/*
	 * get clock frequency for processor_info system call.
	 * This will be zero if the property is not there.
	 */
	pi->pi_clock = (ddi_prop_get_int(DDI_DEV_T_ANY,
	    cpu_get_dip(cp->cpu_id), 0, "clock-frequency",
	    0) + 500000) / 1000000;

	if (pi->pi_clock != cpu0.cpu_type_info.pi_clock) {
		if (!warn_mixed_mhz)
			cmn_err(CE_WARN, "System contains processors with "
				"different clock frequencies.");
		warn_mixed_mhz = 1;
	}

	(void) strcpy(pi->pi_processor_type, "sparc");
	(void) strcpy(pi->pi_fputypes, "sparc");
}

static void
cpu_init_unit(struct cpu *cpu)
{
	struct cpu *self = CPU;		/* can't use this until now?! */

	if (cpu != self) {
		cpu->cpu_lwp = NULL;
		cpu->cpu_idle_thread = cpu_idle_init(cpu);
		cpu->cpu_thread = cpu_startup_init(cpu);
		cpu->cpu_dispthread = cpu->cpu_thread;
		cpu->cpu_dispatch_pri = DISP_PRIO(cpu->cpu_dispthread);

		/* cpu->cpu_m.in_prom = 0; */
#ifdef TRACE
		cpu->cpu_trace.event_map = null_event_map;
#endif /* TRACE */

		init_intr_threads(cpu);
		/* init_cpu(cpu); */
	}
	init_cpu_info(cpu);
	/*
	 * interrupt and process switch stacks allocated later.
	 */
}

/*
 * start_other_cpus is only called once - at boot time
 */
/*ARGSUSED*/
void
start_other_cpus(int cprboot)
{
	struct cpu *self = CPU;
	int unit, n, too_long;
	extern int do_robin;

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	if (use_mp) {
		cpu_pause_init();
	}

	cmn_err(CE_CONT, "?OBP-Master initialization complete - online\n");
	cmn_err(CE_CONT, "?number of CPUs: %d\n", (use_mp == 0) ? 1 : ncpus);

	ASSERT(self == &cpu0);

	/*
	 * initialization for all but OBP-Master.
	 */
	for (unit = 0; unit < NCPU; unit++) {
		struct cpu *cp = cpu[unit];

		if (cp == 0) {
			continue;	/* doesn't exist */
		}

		if ((cp == self) || !(cp->cpu_flags & CPU_EXISTS)) {
			continue;
		}

		if (use_mp) {
			init_cpu(cp);
		} else {
			cp->cpu_flags |= CPU_QUIESCED | CPU_OFFLINE;
		}
	}

	/*
	 * make sure for all cpus before we starts to distribute
	 * sbus intr.
	 */
	if (use_mp) {
		too_long = 0;
		do {
			for (n = unit = 0; unit < NCPU; unit++)
				if (cpu[unit] &&
				    cpu[unit]->cpu_flags & CPU_ENABLE)
					n++;
			if (n == ncpus)
				break;
			delay(1);		/* wait for one clock tick */
		} while (too_long++ < (5 * hz * ncpus));

		if (n != ncpus)
			cmn_err(CE_WARN, "%d cpus fail to come up.", ncpus - n);
	}

	if (do_robin == 0)
		distribute_sbus_intr();
}

/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
uint_t last_idle_cpu;

void
set_idle_cpu(int cpun)
{
	last_idle_cpu = cpun;
}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/* ARGSUSED */
void
unset_idle_cpu(int cpun)
{
}

/*
 * Start CPU on user request.
 *	Called from p_online(2) to allow architectural dependent startup
 *	to occur before CPU_QUIESCED flag is turned off.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);			/* nothing special to do on this arch */
}

/*
 * Stop CPU on user request.
 *	Called from p_online(2) to allow architectural dependent stopping
 *	code to occur just before CPU_QUISCED flag is turned on.
 *	Interrupts should already be disabled for the processor.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);			/* nothing special to do on this arch */
}

/*
 * Power on CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweron(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}

/*
 * Power off CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweroff(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}

/*
 * Take the specified CPU out of participation in interrupts.
 *	Called by p_online(2) when a processor is being taken off-line.
 *	This allows interrupt threads being handled on the processor to
 *	complete before the processor is idled.
 *
 *	Returns a non-zero error number (EBUSY) if not all interrupts can
 *	be disabled.
 */
int
cpu_disable_intr(struct cpu *cp)
{
	int retval = 0;
	int new, i;

	extern int do_robin;
	extern int cpu_take_sbus[];
	extern struct sbus_private sbus_pri_data[];
	extern uint_t last_cpu_id;

	ASSERT(MUTEX_HELD(&cpu_lock));

	cp->cpu_flags &= ~CPU_ENABLE;

	/*
	 * Cpu0 and bootbus owners have to handle some
	 * local intrs.
	 */
	if (cp == &cpu0 || CPU_IN_SET(bootbusses, cp->cpu_id))
		retval = EBUSY;

	if (do_robin) {
		/* reassign Sbus intrs */
		itr_round_robin(cp->cpu_id);
	} else {
		/*
		 * redirect statically assigned Sbus to
		 * other cpus.
		 */
		for (i = 0; i < MX_SBUS; i++) {
			if (cpu_take_sbus[cp->cpu_id] & (1 << i)) {
				new = next_cpu_index(cp->cpu_id + 1);
				(void) set_sbus_intr_id(sbus_pri_data[i].va_sbi,
					(new << 3));
				cpu_take_sbus[new] |= (1 << i);
			}
		}
		/* no more Sbus intr goes to cp */
		cpu_take_sbus[cp->cpu_id] = 0;
	}

	if (cp->cpu_id == last_cpu_id) {
		for (i = last_cpu_id - 1; i >= 0; i--)
			if (cpu[i] && (cpu[i]->cpu_flags & CPU_ENABLE)) {
				last_cpu_id = i;
				break;
			}
		/*
		 * This routine should not be called if cp points to the very
		 * last active CPU in system.
		 */
		ASSERT(last_cpu_id != cp-> cpu_id);
	}

	return (retval);
}

/*
 * Allow the specified CPU to participate in interrupts.
 *	Called by p_online(2) if a processor could not be taken off-line
 *	because of bound threads, in order to resume processing interrupts.
 *	Also called after starting a processor.
 */
void
cpu_enable_intr(struct cpu *cp)
{
	int i;
	extern uint_t last_cpu_id;
	extern int do_robin;

	ASSERT(MUTEX_HELD(&cpu_lock));

	cp->cpu_flags |= CPU_ENABLE;

	if (do_robin == 0) {
		/* forget about current distribution */
		for (i = 0; i < NCPU; i++)
			cpu_take_sbus[i] = 0;

		/* redistribute intr */
		distribute_sbus_intr();

	}

	if (cp->cpu_id > last_cpu_id)
		last_cpu_id = cp->cpu_id;

}

extern void xmit_cpu_intr(uint_t cpu_id, uint_t pri);

#ifdef DEBUG
uint_t xpoke[NCPU];
#endif DEBUG

void
poke_cpu(int cpu_id)
{
	uint_t pri = 13;
	xmit_cpu_intr(cpu_id, pri);
#ifdef DEBUG
	xpoke[cpu_id]++;
#endif DEBUG
}

#define	OBP_ACTION_REQUESTED(obp_state)	(obp_state > OBP_MB_ACTIVE)

#ifdef	DEBUG
static uint_t mbox_exit_stop[NCPU];
static uint_t mbox_wd_stop[NCPU];
static uint_t mbox_enter_idle[NCPU];
static uint_t mbox_bkpt_idle[NCPU];
#endif	DEBUG

void
poll_obp_mbox(void)
{
	dev_info_t *cpu_get_dip();
	caddr_t cpu_get_mailbox();

	uchar_t obp_state;
	dnode_t self;
	int cpu_id = CPU->cpu_id;
	caddr_t mbox_reg;
	int saved_pc, saved_sp;

	extern char cpustate[];

	mbox_reg = cpu_get_mailbox(cpu_id);
	if (mbox_reg == NULL)
		return;
	obp_state = *mbox_reg;

	self = (dnode_t)ddi_get_nodeid(cpu_get_dip(cpu_id));

	if (!OBP_ACTION_REQUESTED(obp_state))
		return;

	led_set_cpu(cpu_id, obp_state);

	/*
	 * Some debugging support. 4m does in locore.s where
	 * it examines the mbox. With this, one can have
	 * stack trace for threads entering OBP.
	 */
	debug_flush_windows();
	saved_pc = curthread->t_pc;
	saved_sp = curthread->t_sp;

	/*
	 * stuff in current PC/SP
	 */
	(void) setjmp(&curthread->t_pcb);

	cpustate[CPU->cpu_id] |= CPU_IN_OBP;

	switch (obp_state) {
	case OBP_MB_EXIT_STOP:
		DEBUG_INC(mbox_exit_stop[CPU->cpu_id]);
		(void) prom_stopcpu(self);
		break;
	case OBP_MB_WATCHDOG_STOP:
		DEBUG_INC(mbox_wd_stop[CPU->cpu_id]);
		(void) prom_stopcpu(self);
		break;
	case OBP_MB_ENTER_IDLE:
		DEBUG_INC(mbox_enter_idle[CPU->cpu_id]);
		/*
		 * workaround for OBP bug 1119958 and 4250826
		 */
		do {
			(void) prom_idlecpu(self);
		} while (hotp_critical_section || (panicstr && !do_stopcpu));
		break;
	case OBP_MB_BRKPT_IDLE:
		DEBUG_INC(mbox_bkpt_idle[CPU->cpu_id]);
		/*
		 * workaround for OBP bug 1119958.
		 */
		do {
			(void) prom_idlecpu(self);
		} while (panicstr && !do_stopcpu);
		break;
	default:
		cmn_err(CE_PANIC, "poll_obp_mbox: "
			"OBP Mailbox error - request=0x%x\n",
			obp_state);
	}

	cpustate[CPU->cpu_id] &= ~CPU_IN_OBP;

	/*
	 * restore what was there.
	 */
	curthread->t_pc = saved_pc;
	curthread->t_sp = saved_sp;

	led_set_cpu(cpu_id, LED_CPU_RESUME);
}



/*
 * cpu_driver character/block entry point structure
 *
 * According to SPARC DDI, this is SPARC architecture specific.
 */
struct cb_ops cpu_cb_ops = {
	nodev,			/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* chpoll */
	ddi_prop_op,		/* prop_op */
	0,			/* str */
	D_NEW | D_MP		/* flag */
};

/*
 * cpu_driver device operations structure
 *
 * According to SPARC DDI, this is SPARC architecture specific.
 */

static int cpu_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
			void *arg, void **result);
static int cpu_identify(dev_info_t *dip);
static int cpu_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);

struct dev_ops cpu_ops = {
	DEVO_REV,		/* rev */
	0,			/* refcnt */
	cpu_getinfo,		/* getinfo */
	cpu_identify,		/* identify */
	nulldev,		/* probe */
	cpu_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&cpu_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
};

/*
 * auxiliary per cpu information
 */
typedef struct {
	dev_info_t	*dip;		/* OBP device tree name */
	int 		device_id;	/* hardware address */
	int 		lbolt;		/* for deadman switch code */
	caddr_t 	mailbox;	/* OBP V3 mailbox protocol */
} auxcpu_t;

static auxcpu_t 	*auxcpu[NCPU];	/* per-cpu private data */

/*
 * structure of cpu "mailbox-virtual" property
 */
typedef struct {
	caddr_t 	vaddr;
	uint_t 		bytes;
} vaddrspec_t;

/*
 * given a cpu id, return its device info pointer
 */
dev_info_t *
cpu_get_dip(processorid_t id)
{
	if ((id >= 0) && (id < NCPU) && auxcpu[id]) {
		return (auxcpu[id]->dip);
	}
	return ((dev_info_t *)NULL);
}

/*
 * given a cpu id, return its OBP mailbox virtual address register
 */
caddr_t
cpu_get_mailbox(processorid_t id)
{
	if ((id >= 0) && (id < NCPU) && auxcpu[id]) {
		return (auxcpu[id]->mailbox);
	}
	return ((caddr_t)NULL);
}

/*
 * get device driver information
 *
 * According to SPARC DDI, this entry point is SPARC architecture specific
 * and required.
 */
/*ARGSUSED*/
static int
cpu_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int rv = DDI_FAILURE;

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		/*
		 * return the dev_info pointer associated with arg
		 */
		*result = (void *)cpu_get_dip(CPU->cpu_id);
		rv = DDI_SUCCESS;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		/*
		 * return the instance number associated with arg
		 */
		*result = (void *)CPU->cpu_id;
		rv = DDI_SUCCESS;
		break;
	}

	CPU_PRINTF("cpu_getinfo: cpu%d cmd=%d, rv=%d, *result=0x%p\n",
		CPU->cpu_id, infocmd, rv, *result);
	return (rv);
}

/*
 * the identify(9E) routine determines whether this drivers drives the
 * device pointed to by dip.
 *
 * According to SPARC DDI, this entry point is SPARC architecture specific
 * and required.
 */
static int
cpu_identify(dev_info_t *dip)
{
	char *name = ddi_get_name(dip);
	int rv = DDI_NOT_IDENTIFIED;

	if ((strcmp(name, "cpu") == 0) ||
	    (strcmp(name, "TI,TMS390Z55") == 0)) {
		rv = DDI_IDENTIFIED;
	}

	return (rv);
}

typedef struct cpu cpu_struct_t;
#define	ZALLOC_SLEEP(type)	\
	(type *)kmem_zalloc(sizeof (type), KM_SLEEP)

/*
 * attach a device to the system
 *
 * According to SPARC DDI, this entry point is SPARC architecture specific
 * and required.
 */
static int
cpu_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	extern uint_t last_cpu_id;
	int bytes;
	int pnid;
	int device_id;
	int nodeid;
	int cpu_id;
	vaddrspec_t mbox_reg;
	struct cpu *cp;
	auxcpu_t *aux;
	int rv = DDI_FAILURE;

	switch (cmd) {
	case DDI_ATTACH:
		/*
		 * get this cpu's device-id, i.e. hardware address
		 */
		pnid = ddi_get_nodeid(ddi_get_parent(dip));
		if ((bytes = prom_getproplen((dnode_t)pnid, "device-id")) !=
		    sizeof (int)) {
			cmn_err(CE_CONT, "cpu_attach: bad device-id proplen ");
			cmn_err(CE_PANIC, "%d, pnid=%d\n", bytes, pnid);
		}
		if (prom_getprop((dnode_t)pnid, "device-id",
		    (caddr_t)&device_id) == -1) {
			cmn_err(CE_CONT, "cpu_attach: ");
			cmn_err(CE_PANIC, "no device_id property, pnid=%d\n",
				pnid);
		}

		/*
		 * get this cpu's mailbox address for OBP V3 mailbox protocol
		 */
		nodeid = ddi_get_nodeid(dip);
		if ((bytes = prom_getproplen((dnode_t)nodeid,
		    "mailbox-virtual")) != sizeof (vaddrspec_t)) {
			cmn_err(CE_CONT, "cpu_attach: bad mailbox-virtual ");
			cmn_err(CE_PANIC, "proplen %d, nodeid=%d\n",
				bytes, nodeid);
		}
		if (prom_getprop((dnode_t)nodeid, "mailbox-virtual",
		    (caddr_t)&mbox_reg) == -1) {
			cmn_err(CE_CONT, "cpu_attach: no mailbox-virtual ");
			cmn_err(CE_PANIC, "property, nodeid=%d\n", nodeid);
		}

		cpu_id = xdb_cpu_unit(device_id);
		cp = (cpu_id == cpu0.cpu_id) ? &cpu0 :
				ZALLOC_SLEEP(cpu_struct_t);
		cp->cpu_id = (uchar_t)cpu_id;
		cp->cpu_flags |= CPU_EXISTS;

		aux = ZALLOC_SLEEP(auxcpu_t);
		auxcpu[cpu_id] = aux;
		aux->dip = dip;
		aux->device_id = device_id;
		aux->mailbox = mbox_reg.vaddr;

		DEVI(dip)->devi_instance = cp->cpu_id;

		cpu_init_unit(cp);

		mutex_enter(&cpu_lock);
		if (cp != &cpu0) {
			cpu_add_unit(cp);
			if (cp->cpu_id > last_cpu_id)
				last_cpu_id = cp->cpu_id;
		}
		mutex_exit(&cpu_lock);
		ddi_report_dev(dip);

		rv = DDI_SUCCESS;
		break;
	}

	return (rv);
}

/*
 * This routine assigns all intr from a Sbus to a CPU.
 * It starts from cpu0 + 1 since we know cpu0 is taking
 * clock already.
 */
static void
distribute_sbus_intr(void)
{
	uint_t cpu_index;
	int sbus_index;
	int n;

	extern int nsbus;
	extern struct sbus_private sbus_pri_data[];

	cpu_index = next_cpu_index(cpu0.cpu_id + 1);

	sbus_index = next_sbus_index(-1);
	n = nsbus;

	while (n--) {

		(void) set_sbus_intr_id(sbus_pri_data[sbus_index].va_sbi,
			(cpu_index << 3));

		cpu_take_sbus[cpu_index] |= (1 << sbus_index);

		/* advance cpu_index */
		cpu_index = next_cpu_index(cpu_index + 1);

		/* advance sbus_index */
		sbus_index = next_sbus_index(sbus_index);
	}
}

static int
cnt_load(int load)
{
	int i, cnt;

	cnt = 0;
	for (i = 0; i < MX_SBUS; i++)
		if (load & (1 << i))
			cnt++;

	return (cnt);
}

static int
next_cpu_index(int hint)
{
	int i, avl_cpu, minload, cpu_load;

	if (hint >= NCPU)
		hint = 0;

	ASSERT((hint >= 0) && (hint < NCPU));

	i = hint;
	minload = 0xffffff;	/* a load too heavy! */

	do {
		if (cpu[i] && cpu[i]->cpu_flags & CPU_ENABLE) {
			if ((cpu_load = cnt_load(cpu_take_sbus[i])) < minload) {
				avl_cpu = i;
				minload = cpu_load;
			}
		}

		if (++i >= NCPU)
		    i = 0;

	} while (i != hint);

	return (avl_cpu);
}

static int
next_sbus_index(int current)
{
	extern struct sbus_private sbus_pri_data[];

	do {
		if (++current >= MX_SBUS)
		    current = 0;

	} while (sbus_pri_data[current].va_sbi == 0);

	return (current);
}
