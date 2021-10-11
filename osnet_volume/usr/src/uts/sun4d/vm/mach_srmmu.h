/*
 * Copyright (c) 1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the sun referernce mmu (srmmu)
 * specific hat data structures and the srmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 */

#ifndef _VM_MACH_HAT_SRMMU_H
#define	_VM_MACH_HAT_SRMMU_H

#pragma ident	"@(#)mach_srmmu.h	1.14	98/07/21 SMI"


#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CAPTURE_CPUS
#define	RELEASE_CPUS
#define	XCALL_PROLOG
#define	XCALL_EPILOG

#define	MOD_VALID_PTE(srflags, ppte, val, va, lvl, hat, rmflags) \
	if ((srflags) & (SR_NOFLUSH)) {		\
		(void) swapl((val), (int *)(ppte));	\
	} else {				\
		(void) mmu_writepte((ppte), (val), (va), (lvl), \
		    (hat), (rmflags)); \
	}

#define	SET_NEW_PTE(ppte, val, va, lvl, rmflags) \
	(void) swapl((val), (int *)(ppte))

#define	MOD_VALID_PTP(srflags, pptp, val, va, lvl, hat, flags) \
	if ((srflags) & SR_NOFLUSH) {		\
		(void) swapl((val), (int *)(pptp));	\
	} else {				\
		(void) mmu_writeptp((pptp), (val), (va), (lvl), \
		    (hat), (flags)); \
	}

#define	SET_NEW_PTP(pptp, val, va, lvl, rmflags) \
	(void) swapl((val), (int *)(pptp))

#define	DEMAP

typedef unsigned long long pa_t;

/*
 * page table pointer to physical address (36 bits) and back.
 */
#define	ptp_to_pa(ptp)	(((pa_t)((uint_t)(ptp) & ~PTE_ETYPEMASK)) << 4)
#define	pa_to_ptp(pa)	((uint_t)((pa) >> 4) | MMU_ET_PTP)

/*
 * Various macros to read ptbl fields. This is needed for a 4m h/w bug
 * workaround.
 */
#define	mmu_readpte(src, dst)	(*(struct pte *)(dst) = *(struct pte *)(src))
#define	mmu_readptp(src, dst)	(*(struct ptp *)(dst) = *(struct ptp *)(src))

/*
 * These routines are all MMU-SPECIFIC interfaces to the srmmu routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */

uint_t 	mmu_writepte(struct pte *pte, uint_t entry, caddr_t addr,
		int level, struct hat *hat, int rmkeep);

uint_t	mmu_writepte_locked(struct pte *pte, uint_t entry, caddr_t va,
		int level, uint_t cxn, int rmkeep);

void	mmu_writeptp(struct ptp *, uint_t, caddr_t, int, struct hat *, int);

void	mmu_writeptp_locked(struct ptp *, uint_t, caddr_t, int, uint_t, int);

int	srmmu_xlate(int, caddr_t, pa_t *, union ptpe *, int *);

int	srmmu_pte_pa(int, caddr_t, pa_t *, int *);

/*
 * assembly routines used by hat_srmmu.c
 */
uint_t ldphys36(pa_t paddr);
uint_t stphys36(pa_t paddr, uint_t val);

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_MACH_HAT_SRMMU_H */
