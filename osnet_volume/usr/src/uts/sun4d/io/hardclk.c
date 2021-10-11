/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hardclk.c	1.89	99/10/22 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/mman.h>
#include <sys/vmem.h>
#include <sys/promif.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <vm/seg_kmem.h>
#include <sys/stat.h>
#include <sys/sunddi.h>

/*
 * Machine-dependent clock routines.
 *	Mostek 48T02 time-of-day for date functions
 *	"tick" timer for kern_clock.c interrupt events
 *	"profile" timer
 */

extern int intr_bw_cntrl_get(int cpuid, int whichbus);
extern int intr_bw_cntrl_set(int cpuid, int busid, int value);
extern int intr_prof_setlimit(int cpuid, int whichbus, int value);
extern int intr_prof_getlimit(int cpuid, int whichbus);
extern int intr_prof_getlimit_local(void);

/*
 * External Data
 */
extern struct cpu cpu0;
extern u_int bootbusses;

#ifdef	MOSTEK_WRITE_PROTECTED
extern void mmu_getpte();
extern void mmu_setpte();
#endif	MOSTEK_WRITE_PROTECTED

/*
 * Static Routines:
 */
static caddr_t mapin_tod(u_int cpu_id);
static void mapout_tod(caddr_t addr);

static void mostek_write(struct mostek48T02 *mostek_regs, todinfo_t tod);
static todinfo_t mostek_read(struct mostek48T02 *mostek_regs);
static u_int mostek_writable(struct mostek48T02 *mostek_regs);
static void mostek_restore(struct mostek48T02 *mostek_regs, u_int saveprot);
static void mostek_sync(todinfo_t tod);

static struct mostek48T02 *system_tod = 0;

/*
 * Map in the system TOD clock, sync up all the slacve TOD clocks to
 * the system TOD clock.
 */
void
init_all_tods(void)
{
	int cpu_w_bbus;		/* CPU index to use to map in TOD */

	/*
	 * Make sure the boot CPU has the boot bus semaphore.
	 * If it does not, then mapin the ECSR address of the TOD via
	 * the other CPU on the board.
	 * This is done by XORing the CPU ID with 1. If you are running
	 * on A, this picks B, and vice versa.
	 */
	if (!CPU_IN_SET(bootbusses, CPU->cpu_id)) {
		cpu_w_bbus = (CPU->cpu_id) ^ 1;
	} else {
		cpu_w_bbus = CPU->cpu_id;
	}

	system_tod = (struct mostek48T02 *)(mapin_tod(cpu_w_bbus) +
		TOD_BYTE_OFFSET);

	mostek_sync(mostek_read(system_tod));
}

static void
mostek_sync(todinfo_t tod)
{
	int cpu_id;

	/* go through all of the CPUs in the system */
	for (cpu_id = 0; cpu_id < NCPU; cpu_id++) {
		/*
		 * Find the ones which own the bootbus but are not the
		 * boot CPU.
		 */
		if ((cpu_id != cpu0.cpu_id) &&
		    (CPU_IN_SET(bootbusses, cpu_id))) {
			struct mostek48T02 *local_tod;
			caddr_t local_tod_map;

			/*
			 * map in the tod. If for some reason it fails to
			 * map in the adress then do not attempt to update
			 * the system clock
			 */
			if ((local_tod_map = mapin_tod(cpu_id)) == NULL)
				continue;

			local_tod = (struct mostek48T02 *)(local_tod_map +
				TOD_BYTE_OFFSET);

			/* write it! */
			mostek_write(local_tod, tod);

			/* destroy the mapping */
			mapout_tod(local_tod_map);
		}
	}
}


static void
mapout_tod(caddr_t addr)
{
	hat_unload(kas.a_hat, addr, TOD_BYTES, HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, addr, TOD_BYTES);
}


static caddr_t
mapin_tod(u_int cpu_id)
{
	caddr_t vaddr;

	if ((vaddr = vmem_alloc(heap_arena, TOD_BYTES, VM_NOSLEEP)) == NULL)
		return (NULL);

	hat_devload(kas.a_hat, vaddr, TOD_BYTES,
	    (u_int)(ECSR_PFN(CPU_DEVICEID(cpu_id)) + TOD_PAGE_OFFSET),
	    PROT_READ | PROT_WRITE, HAT_LOAD_LOCK);

	return (vaddr);
}

/*
 * change access protections
 */
/*ARGSUSED*/
static u_int
mostek_writable(struct mostek48T02 *mostek_regs)
{
	u_int saveprot = 0;
#ifdef	MOSTEK_WRITE_PROTECTED
	struct pte pte;

	/* write-enable the eeprom */
	mmu_getpte((caddr_t)mostek_regs, &pte);
	saveprot = pte.AccessPermissions;
	pte.AccessPermissions = MMU_STD_SRWUR;
	mmu_setpte((caddr_t)mostek_regs, pte);
#endif	MOSTEK_WRITE_PROTECTED
	return (saveprot);
}

/*ARGSUSED*/
static void
mostek_restore(struct mostek48T02 *mostek_regs, u_int saveprot)
{
#ifdef	MOSTEK_WRITE_PROTECTED
	/* Now write protect it, preserving the new modify/ref bits */
	mmu_getpte((caddr_t)mostek_regs, &pte);
	pte.AccessPermissions = saveprot;
	mmu_setpte((caddr_t)mostek_regs, pte);
#endif	MOSTEK_WRITE_PROTECTED
}

/*
 * mostek_read - access the actual hardware
 */
todinfo_t
mostek_read(struct mostek48T02 *mostek_regs)
{
	todinfo_t tod;
	u_int saveprot = mostek_writable(mostek_regs);

	/*
	 * Turn off updates so we can read the clock cleanly. Then read
	 * all the registers into a temp, and reenable updates.
	 */
	mostek_regs->clk_ctrl |= CLK_CTRL_READ;
	tod.tod_year	= BCD_TO_BYTE(mostek_regs->clk_year) + YRBASE;
	tod.tod_month	= BCD_TO_BYTE(mostek_regs->clk_month & 0x1f);
	tod.tod_day	= BCD_TO_BYTE(mostek_regs->clk_day & 0x3f);
	tod.tod_dow	= BCD_TO_BYTE(mostek_regs->clk_weekday & 0x7);
	tod.tod_hour	= BCD_TO_BYTE(mostek_regs->clk_hour & 0x3f);
	tod.tod_min	= BCD_TO_BYTE(mostek_regs->clk_min & 0x7f);
	tod.tod_sec	= BCD_TO_BYTE(mostek_regs->clk_sec & 0x7f);
	mostek_regs->clk_ctrl &= ~CLK_CTRL_READ;

	mostek_restore(mostek_regs, saveprot);

	return (tod);
}

/*
 * mostek_write - access the actual hardware
 */
static void
mostek_write(struct mostek48T02 *mostek_regs, todinfo_t tod)
{
#ifdef	MOSTEK_WRITE_PROTECTED
	unsigned int saveprot;
	struct pte pte;

	/* write-enable the eeprom */
	mmu_getpte((caddr_t)mostek_regs, &pte);
	saveprot = pte.AccessPermissions;
	pte.AccessPermissions = MMU_STD_SRWUR;
	mmu_setpte((caddr_t)mostek_regs, pte);
#endif	MOSTEK_WRITE_PROTECTED

	mostek_regs->clk_ctrl |= CLK_CTRL_WRITE;	/* allow writes */
	mostek_regs->clk_year		= BYTE_TO_BCD(tod.tod_year - YRBASE);
	mostek_regs->clk_month		= BYTE_TO_BCD(tod.tod_month);
	mostek_regs->clk_day		= BYTE_TO_BCD(tod.tod_day);
	mostek_regs->clk_weekday	= BYTE_TO_BCD(tod.tod_dow);
	mostek_regs->clk_hour		= BYTE_TO_BCD(tod.tod_hour);
	mostek_regs->clk_min		= BYTE_TO_BCD(tod.tod_min);
	mostek_regs->clk_sec		= BYTE_TO_BCD(tod.tod_sec);
	mostek_regs->clk_ctrl &= ~CLK_CTRL_WRITE;	/* load values */

#ifdef	MOSTEK_WRITE_PROTECTED
	/* Now write protect it, preserving the new modify/ref bits */
	mmu_getpte((caddr_t)mostek_regs, &pte);
	pte.AccessPermissions = saveprot;
	mmu_setpte((caddr_t)mostek_regs, pte);
#endif	MOSTEK_WRITE_PROTECTED
}

/*
 * Read the current time from the clock chip and convert to UNIX form.
 * Assumes that the year in the clock chip is valid.
 * Must be called with tod_lock held.
 */
timestruc_t
tod_get(void)
{
	timestruc_t ts;

	ASSERT(MUTEX_HELD(&tod_lock));

	ts.tv_sec = tod_validate(tod_to_utc(mostek_read(system_tod)),
					(hrtime_t)0);
	ts.tv_nsec = 0;

	return (ts);
}

/*
 * Write the specified time into the clock chip.
 * Must be called with tod_lock held.
 */
void
tod_set(timestruc_t ts)
{
	todinfo_t tod = utc_to_tod(ts.tv_sec);

	ASSERT(MUTEX_HELD(&tod_lock));

	mostek_write(system_tod, tod);
	mostek_sync(tod);

	/* prevent false alarm in tod_validate() due to tod value change */
	tod_fault_reset();
}

/*
 * This section provides support for the "tick" timer.
 */
#define	CTR_LIMIT_BIT	0x80000000	/* bit mask */
#define	CTR_USEC_MASK	0x7FFFFC00	/* bit mask */
#define	CTR_USEC_SHIFT	10		/* shift count */

/*
 * User timer enable
 */
#define	BW_CNTRL_UTE	(1 << 2)
#define	TIMER_FREERUN(cntrl)	((cntrl & BW_CNTRL_UTE) != 0)
#define	TIMER_SETFREE(cntrl)	(cntrl | BW_CNTRL_UTE)
#define	TIMER_CLRFREE(cntrl)	(cntrl & ~BW_CNTRL_UTE)

#define	BW_WALL_UCEN	(1 << 0)
#define	WALL_FREERUN(frozen)	((frozen & BW_WALL_UCEN) != 0)
#define	WALL_SETFREE(rsvd)	(rsvd | BW_WALL_UCEN)
#define	WALL_CLRFREE(rsvd)	(rsvd & ~BW_WALL_UCEN)

extern u_int intr_usercntrl_get(void);
extern void intr_usercntrl_set(int);
extern longlong_t intr_usertimer_get(void);
extern void intr_usertimer_set(longlong_t);
extern hrtime_t hrtime_base;

#define	XDBUS_ZERO	0

void
level14_enable(processorid_t cpu, uint_t nsec)
{
	uint_t usec = nsec / (NANOSEC / MICROSEC);
	uint_t limit = (usec << CTR_USEC_SHIFT) & CTR_USEC_MASK;
	uint_t cntrl;

	/*
	 * Zero the UTE (User Timer Enable) bit in the BW's control register.
	 * This sets the level-14 to interrupt mode.  To support single XDBus
	 * machines, we only use the BW on XDBus 0.
	 */
	cntrl = intr_bw_cntrl_get(cpu, XDBUS_ZERO);
	(void) intr_bw_cntrl_set(cpu, XDBUS_ZERO, cntrl & ~BW_CNTRL_UTE);
	(void) intr_prof_setlimit(cpu, XDBUS_ZERO, limit);
}

void
level14_disable(processorid_t cpu)
{
	/*
	 * We disable the level-14 counter/timer by clearing the limit register
	 * and setting the User Timer Enable bit in the BW's control register.
	 * Setting the UTE bit puts the profile timer in user timer mode;
	 * the Counter and the Limit registers are merged to form a single
	 * 64-bit value.  More importantly from our perspective, interrupts
	 * from the profile timer are disabled in user timer mode.
	 *
	 * The ordering here is important; the limit register _must_ be zeroed
	 * before the UTE bit can be set.
	 */
	uint_t cntrl = intr_bw_cntrl_get(cpu, XDBUS_ZERO);
	(void) intr_prof_setlimit(cpu, XDBUS_ZERO, 0);
	(void) intr_bw_cntrl_set(cpu, XDBUS_ZERO, cntrl | BW_CNTRL_UTE);
}

uint_t
level14_nsec(processorid_t cpu)
{
	uint_t lmt = intr_prof_getlimit(cpu, XDBUS_ZERO);
	return ((lmt & CTR_USEC_MASK) >> CTR_USEC_SHIFT) * (NANOSEC / MICROSEC);
}

/*
 * read_scb_int - dumb little routine to read from trap vectors
 */
void
read_scb_int(int level, trapvec *vec)
{
	*vec = scb.interrupts[level - 1];
}

/*
 * The following wrappers have been added so that locking
 * can be exported to platform-independent clock routines
 * (ie adjtime(), clock_setttime()), via a functional interface.
 */
int
hr_clock_lock(void)
{
	u_short s;

	CLOCK_LOCK(&s);
	return (s);
}

void
hr_clock_unlock(int s)
{
	CLOCK_UNLOCK(s);
}
