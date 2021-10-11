/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CLOCK_H
#define	_SYS_CLOCK_H

#pragma ident	"@(#)clock.h	1.18	99/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#ifndef	_ASM

#include <sys/psw.h>
#include <sys/time.h>
#include <sys/processor.h>

extern void unlock_hres_lock(void);
extern timestruc_t pc_tod_get(void);
extern void pc_tod_set(timestruc_t);
extern void hres_tick();
extern void (*hrtime_tick)(void);

extern void tsc_hrtimeinit(uint32_t cpu_freq);
extern hrtime_t tsc_gethrtime();
extern hrtime_t tsc_gethrtime_delta();
extern void tsc_tick(void);
extern void tsc_sync_master(processorid_t);
extern void tsc_sync_slave();
extern hrtime_t tsc_read();


#define	ADJ_SHIFT 4		/* used in get_hrestime */

#define	YRBASE		00	/* 1900 - what year 0 in chip represents */

#endif	/* !_ASM */

#define	CBE_HIGH_PIL	14
#define	CBE_LOCK_PIL	LOCK_LEVEL
#define	CBE_LOW_PIL	2

/*
 * CLOCK_LOCK() sets the LSB (bit 0) of the hres_lock. The rest of the
 * 31bits are used as the counter. This lock is acquired
 * around "hrestime" and "timedelta". This lock is acquired to make
 * sure that level-14 accounts for changes to this variable in that
 * interrupt itself. The level-14 interrupt code also acquires this
 * lock.
 * (Note: It is assumed that the lock_set_spl() uses only bit 0 of the lock.)
 *
 * CLOCK_UNLOCK() increments the lower bytes straight, thus clearing the
 * lock and also incrementing the counter. This way gethrtime()
 * can figure out if the value in the lock got changed or not.
 */
#define	HRES_LOCK_OFFSET 0	/* byte 0 has the lock bit(bit 0 in the byte) */

#define	CLOCK_LOCK(oldsplp)	\
	lock_set_spl((lock_t *)&hres_lock + HRES_LOCK_OFFSET, 	\
		ipltospl(XC_HI_PIL), oldsplp)

#define	CLOCK_UNLOCK(spl)		\
	unlock_hres_lock();		\
	splx(spl);			\
	LOCKSTAT_EXIT(LS_SPIN_LOCK_HOLD,	\
		(lock_t *)&hres_lock + HRES_LOCK_OFFSET, curthread, 1);

#endif	/* KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CLOCK_H */
