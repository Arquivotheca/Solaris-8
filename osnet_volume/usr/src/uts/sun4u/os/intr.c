/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)intr.c	1.46	99/07/22 SMI"

#include <sys/sysmacros.h>
#include <sys/stack.h>
#include <sys/cpuvar.h>
#include <sys/ivintr.h>
#include <sys/intreg.h>
#include <sys/membar.h>
#include <sys/kmem.h>
#include <sys/intr.h>
#include <sys/cmn_err.h>
#include <sys/privregs.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/x_call.h>
#include <vm/seg_kp.h>
#include <sys/debug.h>
#include <sys/cyclic.h>

#include <sys/cpu_sgnblk_defs.h>

kmutex_t soft_iv_lock;	/* protect software interrupt vector table */
/* Global lock which protects the interrupt distribution lists */
kmutex_t intr_dist_lock;
/* Head of the interrupt distriubution list */
struct intr_dist *intr_dist_head = NULL;

uint_t swinum_base;
uint_t maxswinum;
uint_t siron_inum;
uint_t poke_cpu_inum;
int siron_pending;
int intr_policy = INTR_FLAT_DIST;	/* interrupt distribution policy */
int test_dirint = 0;		/* Flag for testing directed interrupts */

/*
 * Mask to use when searching for a matching mondo. This masks off the
 * lower 3 bits, which are the bits which define level for an external
 * SBus slot mondo. This allows the code to seacrh for unique key properly.
 */
#define	SBUS_LVL_MASK	0x7

/*
 * static routine to support interrupt distribution.
 */
static uint_t intr_dist_elem(enum intr_policies);
static void sw_ivintr_init(cpu_t *);

/*
 * intr_init() - interrutp initialization
 *	Initialize the system's software interrupt vector table and
 *	CPU's interrupt free list
 */
void
intr_init(cpu_t *cp)
{
	sw_ivintr_init(cp);
	init_intr_pool(cp);

	mutex_init(&intr_dist_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * poke_cpu_intr - fall through when poke_cpu calls
 */

/* ARGSUSED */
uint_t
poke_cpu_intr(caddr_t arg)
{
	CPU->cpu_m.poke_cpu_outstanding = B_FALSE;
	membar_stld_stst();
	return (1);
}

/*
 * sw_ivintr_init() - software interrupt vector initialization
 *	called after CPU is active
 *	the software interrupt vector table is part of the intr_vector[]
 */
static void
sw_ivintr_init(cpu_t *cp)
{
	extern u_int softlevel1();

	mutex_init(&soft_iv_lock, NULL, MUTEX_DEFAULT, NULL);

	swinum_base = SOFTIVNUM;

	/*
	 * the maximum software interrupt == MAX_SOFT_INO
	 */
	maxswinum = swinum_base + MAX_SOFT_INO;

	REGISTER_BBUS_INTR();

	siron_inum = add_softintr(PIL_1, softlevel1, 0);
	poke_cpu_inum = add_softintr(PIL_13, poke_cpu_intr, 0);
	cp->cpu_m.poke_cpu_outstanding = B_FALSE;
}

cpuset_t intr_add_pools_inuse;

/*
 * cleanup_intr_pool()
 *	Free up the extra intr request pool for this cpu.
 */
void
cleanup_intr_pool(cpu_t *cp)
{
	extern struct intr_req *intr_add_head;
	int poolno;
	struct intr_req *pool;

	poolno = cp->cpu_m.intr_pool_added;
	if (poolno >= 0) {
		cp->cpu_m.intr_pool_added = -1;
		pool = (poolno * INTR_PENDING_MAX * intr_add_pools) +

			intr_add_head;	/* not byte arithmetic */
		bzero(pool, INTR_PENDING_MAX * intr_add_pools *
		    sizeof (struct intr_req));

		CPUSET_DEL(intr_add_pools_inuse, poolno);
	}
}

/*
 * init_intr_pool()
 *	initialize the intr request pool for the cpu
 * 	should be called for each cpu
 */
void
init_intr_pool(cpu_t *cp)
{
	extern struct intr_req *intr_add_head;
#ifdef	DEBUG
	extern struct intr_req *intr_add_tail;
#endif	/* DEBUG */
	int i, pool;

	cp->cpu_m.intr_pool_added = -1;

	for (i = 0; i < INTR_PENDING_MAX-1; i++) {
		cp->cpu_m.intr_pool[i].intr_next =
		    &cp->cpu_m.intr_pool[i+1];
	}
	cp->cpu_m.intr_pool[INTR_PENDING_MAX-1].intr_next = NULL;

	cp->cpu_m.intr_head[0] = &cp->cpu_m.intr_pool[0];
	cp->cpu_m.intr_tail[0] = &cp->cpu_m.intr_pool[INTR_PENDING_MAX-1];

	if (intr_add_pools != 0) {

		/*
		 * If additional interrupt pools have been allocated,
		 * initialize those too and add them to the free list.
		 */

		struct intr_req *trace;

		for (pool = 0; pool < max_ncpus; pool++) {
			if (!(CPU_IN_SET(intr_add_pools_inuse, pool)))
			    break;
		}
		if (pool >= max_ncpus) {
			/*
			 * XXX - intr pools are alloc'd, just not as
			 * much as we would like.
			 */
			cmn_err(CE_WARN, "Failed to alloc all requested intr "
			    "pools for cpu%d", cp->cpu_id);
			return;
		}
		CPUSET_ADD(intr_add_pools_inuse, pool);
		cp->cpu_m.intr_pool_added = pool;

		trace = (pool * INTR_PENDING_MAX * intr_add_pools) +
			intr_add_head;	/* not byte arithmetic */

		cp->cpu_m.intr_pool[INTR_PENDING_MAX-1].intr_next = trace;

		for (i = 1; i < intr_add_pools * INTR_PENDING_MAX; i++, trace++)
			trace->intr_next = trace + 1;
		trace->intr_next = NULL;

		ASSERT(trace >= intr_add_head && trace <= intr_add_tail);

		cp->cpu_m.intr_tail[0] = trace;
	}
}

/*
 * siron - primitive for sun/os/softint.c
 */
void
siron()
{
	if (!siron_pending) {
		siron_pending = 1;
		setsoftint(siron_inum);
	}
}

#ifdef DEBUG

/*
 * no_ivintr()
 * 	called by vec_interrupt() through sys_trap()
 *	vector interrupt received but not valid or not
 *	registered in intr_vector[]
 *	considered as a spurious mondo interrupt
 */
/* ARGSUSED */
void
no_ivintr(struct regs *rp, int inum, int pil)
{
	cmn_err(CE_WARN, "invalid vector intr: number 0x%x, pil 0x%x",
		inum, pil);
#ifdef DEBUG_VEC_INTR
	prom_enter_mon();
#endif /* DEBUG_VEC_INTR */
}

/*
 * no_intr_pool()
 * 	called by vec_interrupt() through sys_trap()
 *	vector interrupt received but no intr_req entries
 */
/* ARGSUSED */
void
no_intr_pool(struct regs *rp, int inum, int pil)
{
#ifdef DEBUG_VEC_INTR
	cmn_err(CE_WARN, "intr_req pool empty: num 0x%x, pil 0x%x",
		inum, pil);
	prom_enter_mon();
#else
	cmn_err(CE_PANIC, "intr_req pool empty: num 0x%x, pil 0x%x",
		inum, pil);
#endif /* DEBUG_VEC_INTR */
}

/*
 * no_intr_req()
 * 	called by pil_interrupt() through sys_trap()
 *	pil interrupt request received but not valid
 *	considered as a spurious pil interrupt
 */
/* ARGSUSED */
void
no_intr_req(struct regs *rp, uintptr_t intr_req, int pil)
{
	cmn_err(CE_WARN, "invalid pil intr request 0x%p, pil 0x%x",
	    (void *)intr_req, pil);
#ifdef DEBUG_PIL_INTR
	prom_enter_mon();
#endif /* DEBUG_PIL_INTR */
}
#endif /* DEBUG */

/*
 * Send a directed interrupt of specified interrupt number id to a cpu.
 */
void
send_dirint(
	int cpuix,		/* cpu to be interrupted */
	int intr_id)		/* interrupt number id */
{
	xt_one(cpuix, (xcfunc_t *)intr_id, 0, 0);
}

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

/*
 * Take the specified CPU out of participation in interrupts.
 *	Called by p_online(2) when a processor is being taken off-line.
 *	This allows interrupt threads being handled on the processor to
 *	complete before the processor is idled.
 */
int
cpu_disable_intr(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Turn off the CPU_ENABLE flag before calling the redistribution
	 * function, since it checks for this in the cpu flags.
	 */
	cp->cpu_flags &= ~CPU_ENABLE;

	intr_redist_all_cpus(intr_policy);

	return (0);
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
	ASSERT(MUTEX_HELD(&cpu_lock));

	cp->cpu_flags |= CPU_ENABLE;

	intr_redist_all_cpus(intr_policy);
}

/*
 * Add the specified interrupt to the interrupt list. Then call the
 * policy funtion to determine the CPU to target the interrupt at.
 * returns the CPU MID. The func entry is a pointer to a
 * callback function into the nexus driver. This along with
 * instance of the nexus driver and the mondo, allows interrupts to be
 * retargeted at different CPUs.
 */
uint_t
intr_add_cpu(void (*func)(void *, int, uint_t), void *dip, int mondo,
	int mask_flag)
{
	struct intr_dist *ptr;

	ASSERT(MUTEX_HELD(&intr_dist_lock));
	ASSERT(func);
	ASSERT(dip);

	/* Allocate a new interrupt element and fill in all the fields. */
	ptr = kmem_zalloc(sizeof (struct intr_dist), KM_SLEEP);

	ptr->func = func;
	ptr->dip = dip;
	ptr->mondo = mondo;
	ptr->mask_flag = mask_flag;
	ptr->next = intr_dist_head;
	intr_dist_head = ptr;

	/*
	 * Call the policy function to determine a CPU target for this
	 * interrupt.
	 */
	return (intr_dist_elem(intr_policy));
}

/*
 * Search for the interupt distribution structure with the specified
 * mondo vec reg in the interrupt distribution list. If a match is found,
 * then delete the entry from the list. The caller is responsible for
 * modifying the mondo vector registers.
 */
void
intr_rem_cpu(int mondo)
{
	struct intr_dist *iptr;
	struct intr_dist **vect;

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	for (iptr = intr_dist_head,
	    vect = &intr_dist_head; iptr != NULL;
	    vect = &iptr->next, iptr = iptr->next) {
		if (iptr->mask_flag) {
			if ((mondo & ~SBUS_LVL_MASK) ==
			    (iptr->mondo & ~SBUS_LVL_MASK)) {
				*vect = iptr->next;
				kmem_free(iptr, sizeof (struct intr_dist));
				return;
			}
		} else {
			if (mondo == iptr->mondo) {
				*vect = iptr->next;
				kmem_free(iptr, sizeof (struct intr_dist));
				return;
			}
		}
	}
	if (!panicstr) {
		cmn_err(CE_PANIC,
			"Mondo %x not found on interrupt distribution list",
			mondo);
	}
}

/*
 * Generic function to search and find withing the intr_dist list for element
 * with matching dip.  If found, return the pointer, if not return NULL.
 */
struct intr_dist *
intr_exist(void *dip)
{
	struct intr_dist  *iptr;

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	for (iptr = intr_dist_head;
		iptr != NULL;
		iptr = iptr->next) {
		if (dip == iptr->dip)
			return (iptr);
	}
	return (NULL);
}

/*
 * Generic function to update the dip in intr_dist.
 */
void
intr_update_cb_data(struct intr_dist *iptr, void *dip)
{
	ASSERT(MUTEX_HELD(&intr_dist_lock));

	iptr->dip = dip;
}

/*
 * Redistribute all interrupts
 *
 * This function redistributes all interrupting devices, running the
 * parent callback functions for each node.
 */
void
intr_redist_all_cpus(enum intr_policies policy)
{
	struct intr_dist *iptr;
	uint_t cpu_id;

	ASSERT(MUTEX_HELD(&cpu_lock));

	mutex_enter(&intr_dist_lock);

	/* now distribute all interrupts from the list */
	for (iptr = intr_dist_head; iptr != NULL; iptr = iptr->next) {
		/* now distribute it into the new intr array */
		cpu_id = intr_dist_elem(policy);

		/* run the callback and inform the parent */
		if (iptr->func != NULL)
			iptr->func(iptr->dip, iptr->mondo, cpu_id);
	}

	cpu_id = intr_dist_elem(policy);

	mutex_exit(&intr_dist_lock);
}

/*
 * Determine what CPU to target, based on the interrupt policy passed
 * in. For the INTR_FLAT_DIST case we hold a current cpu pointer in a
 * static variable which is stepped through each interrupt enabled
 * cpu in turn.
 */
static uint_t
intr_dist_elem(enum intr_policies policy)
{
	static struct cpu *curr_cpu;
	struct cpu *new_cpu;

	switch (policy) {
	case INTR_CURRENT_CPU:
		return (CPU->cpu_id);

	case INTR_BOOT_CPU:
		panic("INTR_BOOT_CPU no longer supported.");
		/*NOTREACHED*/

	case INTR_FLAT_DIST:
	default:
		/*
		 * move the current CPU to the next one.
		 * As we hold onto a cpu struct pointer we must test
		 * the next pointer to see if the cpu has been
		 * deleted (cpu structs are never freed.)
		 */
		if (curr_cpu == NULL || curr_cpu->cpu_next == NULL)
			curr_cpu = CPU;

		for (new_cpu = curr_cpu->cpu_next_onln;
		    new_cpu != curr_cpu; new_cpu = new_cpu->cpu_next_onln) {
			/* Is it OK to add interrupts to this CPU? */
			if (new_cpu->cpu_flags & CPU_ENABLE) {
				curr_cpu = new_cpu;
				break;
			}
		}
		return (curr_cpu->cpu_id);
	}
}
