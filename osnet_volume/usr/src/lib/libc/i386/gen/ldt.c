/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ldt.c	1.9	99/09/14 SMI"

#include <sys/types.h>
#include <sys/sysi86.h>
#include <sys/segment.h>
#include <synch.h>

/*
 * We use these versions of _lwp_mutex_lock() and _lwp_mutex_unlock()
 * rather than the exported versions in order to avoid invoking
 * the dynamic linker at an inopportune time for libthread.
 * In particular, we cannot afford to invoke the dynamic linker
 * when we allocate/deallocate the selector for %gs.
 */
extern	int	_private_lwp_mutex_lock(lwp_mutex_t *);
extern	int	_private_lwp_mutex_unlock(lwp_mutex_t *);

#define	MKSEL(index, ti, rpl)	\
	(((index&0x1fff)<<3)|((ti&0x1)<<2)|(rpl&0x3))

#define	MAX_BITS	8192	/* max length of an ldt */
#define	BPL		32	/* #bits in a long */
#define	NLONGS		(MAX_BITS/BPL)
static unsigned long ldtmap[NLONGS] = {	/* first 32 are reserved */
	0xffffffff
};

static int ldtnxtfree(void);
extern void __free_selector(int);

static lwp_mutex_t ldt_lock;	/* default type is USYNC_THREAD */

/*
 * The following two routines are called by the threads library fork1() wrapper.
 * Since the ldt_lock is a very low-level lock, atfork() routines cannot be
 * installed for it. So, this additional interface had to be created.
 */

void
__ldt_lock()
{
	(void) _lwp_mutex_lock(&ldt_lock);
}

void
__ldt_unlock()
{
	(void) _lwp_mutex_unlock(&ldt_lock);
}

int
__alloc_selector(void *addr, int size)
{
	int freesel;
	struct ssd s;

	if ((freesel = ldtnxtfree()) < 0)
		return (-1);
	s.sel = MKSEL(freesel, 1, 0x3); /* in ldt, priv level 3 */
	s.bo = (unsigned int)addr;
	s.ls = size;
	s.acc1 = UDATA_ACC1;	/* data selector, RW */
	s.acc2 = DATA_ACC2_S;	/* 32 bit access */

	if (sysi86(SI86DSCR, &s) < 0) {
		__free_selector(freesel);
		return (-1);
	}
	return (s.sel);
}

void
__free_selector(int sel)
{
	int ind = (sel >> 3) & 0x1fff;

	(void) _private_lwp_mutex_lock(&ldt_lock);
	ldtmap[ind/BPL] &= ~(1 << (ind%BPL));
	(void) _private_lwp_mutex_unlock(&ldt_lock);
}

/*
 * free all selectors except 'sel', its our callers %gs
 */
void
__free_all_selectors(int sel)
{
	int ind = (sel >> 3) & 0x1fff;
	int i;

	(void) _private_lwp_mutex_lock(&ldt_lock);
	for (i = 1; i < NLONGS; i++)
		ldtmap[i] = 0L;
	ldtmap[ind/BPL] |= 1 << (ind%BPL);
	(void) _private_lwp_mutex_unlock(&ldt_lock);
}

static int
ldtnxtfree()
{
	unsigned long mask;
	int i, j, n;

	(void) _private_lwp_mutex_lock(&ldt_lock);
	for (i = n = 0; i < NLONGS; i++) {
		for (mask = 1, j = 0; j < BPL; j++, mask <<= 1, n++) {
			if (!(ldtmap[i] & mask)) {
				ldtmap[i] |= mask;
				(void) _private_lwp_mutex_unlock(&ldt_lock);
				return (n);
			}
		}
	}
	(void) _private_lwp_mutex_unlock(&ldt_lock);
	return (-1);
}
