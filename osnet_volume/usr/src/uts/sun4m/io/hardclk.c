/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hardclk.c	1.67	99/10/22 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/clock.h>
#include <sys/intreg.h>
#include <sys/cpuvar.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <vm/as.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/machsystm.h>
#include <sys/cyclic.h>
#include <sys/clock.h>
#include <sys/kmem.h>
#include <sys/mon_clock.h>

void flush_windows(void);
void hat_chgprot(struct hat *, caddr_t, size_t, uint_t);

uint_t
level14_nsec(processorid_t me)
{
	uint_t lmt = v_counter_addr[me]->timer_msw;

	return ((lmt & CTR_USEC_MASK) >> CTR_USEC_SHIFT) * (NANOSEC / MICROSEC);
}

void
level14_enable(processorid_t me, uint_t nsec)
{
	uint_t usec = nsec / (NANOSEC / MICROSEC);
	uint_t limit = (usec << CTR_USEC_SHIFT) & CTR_USEC_MASK;

	v_counter_addr[me]->timer_msw = limit;
}

void
level14_disable(processorid_t me)
{
	v_counter_addr[me]->timer_msw = 0;
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

#if	!defined(SAS) && !defined(MPSAS)
	/*
	 * The eeprom (which also contains the tod clock) is normally
	 * marked ro; change it to rw temporarily to update todc.
	 * This must be done every time the todc is written since the
	 * prom changes the todc mapping back to ro when it changes
	 * nvram variables (e.g. the eeprom cmd).
	 */
	hat_chgprot(kas.a_hat, (caddr_t)((uint_t)CLOCK & PAGEMASK), PAGESIZE,
		PROT_READ | PROT_WRITE);

	CLOCK->clk_ctrl |= CLK_CTRL_WRITE;	/* allow writes */
	CLOCK->clk_year		= BYTE_TO_BCD(tod.tod_year - YRBASE);
	CLOCK->clk_month	= BYTE_TO_BCD(tod.tod_month);
	CLOCK->clk_day		= BYTE_TO_BCD(tod.tod_day);
	CLOCK->clk_weekday	= BYTE_TO_BCD(tod.tod_dow);
	CLOCK->clk_hour		= BYTE_TO_BCD(tod.tod_hour);
	CLOCK->clk_min		= BYTE_TO_BCD(tod.tod_min);
	CLOCK->clk_sec		= BYTE_TO_BCD(tod.tod_sec);
	CLOCK->clk_ctrl &= ~CLK_CTRL_WRITE;	/* load values */

	/*
	 * Now write protect it, preserving the new modify/ref bits
	 */
	hat_chgprot(kas.a_hat, (caddr_t)((uint_t)CLOCK & PAGEMASK), PAGESIZE,
		PROT_READ);

	/* prevent false alarm in tod_validate() due to tod value change */
	tod_fault_reset();
#endif
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
	todinfo_t tod;

	ASSERT(MUTEX_HELD(&tod_lock));

#if	!defined(SAS) && !defined(MPSAS)

	hat_chgprot(kas.a_hat, (caddr_t)((uint_t)CLOCK & PAGEMASK), PAGESIZE,
		PROT_READ | PROT_WRITE);

	CLOCK->clk_ctrl |= CLK_CTRL_READ;
	tod.tod_year	= BCD_TO_BYTE(CLOCK->clk_year) + YRBASE;
	tod.tod_month	= BCD_TO_BYTE(CLOCK->clk_month & 0x1f);
	tod.tod_day	= BCD_TO_BYTE(CLOCK->clk_day & 0x3f);
	tod.tod_dow	= BCD_TO_BYTE(CLOCK->clk_weekday & 0x7);
	tod.tod_hour	= BCD_TO_BYTE(CLOCK->clk_hour & 0x3f);
	tod.tod_min	= BCD_TO_BYTE(CLOCK->clk_min & 0x7f);
	tod.tod_sec	= BCD_TO_BYTE(CLOCK->clk_sec & 0x7f);
	CLOCK->clk_ctrl &= ~CLK_CTRL_READ;

	hat_chgprot(kas.a_hat, (caddr_t)((uint_t)CLOCK & PAGEMASK), PAGESIZE,
			PROT_READ);
#endif !SAS

	ts.tv_sec = tod_validate(tod_to_utc(tod), (hrtime_t)0);
	ts.tv_nsec = 0;

	return (ts);
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
