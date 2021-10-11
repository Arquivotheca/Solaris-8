/*
 * Copyright (c) 1990, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)subr_small4m.s	1.7	97/05/24 SMI"

/*
 * small4m implementation specific
 * assembly language routines.
 *
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/privregs.h>
#include <sys/module.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/mmu.h>
#include <sys/eeprom.h>
#include <sys/param.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include <sys/async.h>
#include <sys/iommu.h>
#include <sys/devaddr.h>
#include <sys/auxio.h>

/*
 * small4m stuff
 *
 * include anything here that small4m needs to do differently 
 * from the generic sun4m code.
 */

#if defined(lint)

/*
 * System specific routines
 */

void
small_sun4m_sys_setfunc(void)
{}

int
small_sun4m_get_sysctl(void)
{ return(0); }

/* ARGSUSED */
void
small_sun4m_set_sysctl(int i)
{}

void
small_sun4m_enable_dvma(void)
{}

void
small_sun4m_disable_dvma(void)
{}

/*
 * Module specific routines
 */

void
small_sun4m_mmu_getasyncflt(void)
{}

int
small_sun4m_mmu_chk_wdreset(void)
{ return (0); }

#else	/* lint */

#undef SVC
#define SVC(fn) set 	small_sun4m_/**/fn, %o5 ;\
		set 	v_/**/fn, %o0 ;\
		st	%o5, [%o0] ;

#undef STB
#define STB(fn) set     v_/**/fn, %o0 ;\
		st	%o4, [%o0] ;

#define EVECT(s) .global v_/**/s; 

EVECT(get_sysctl)
EVECT(set_sysctl)
EVECT(get_diagmesg)
EVECT(set_diagmesg)
EVECT(enable_dvma)
EVECT(disable_dvma)
EVECT(l15_async_fault)
EVECT(flush_writebuffers)
EVECT(flush_writebuffers_to)
EVECT(memerr_init)
EVECT(memerr_disable)
EVECT(impl_bustype)
EVECT(process_aflt)

	ENTRY(small_sun4m_sys_setfunc)
	set	small_sun4m_stub, %o4
	SVC(get_sysctl)
	SVC(set_sysctl)
	STB(get_diagmesg)
	STB(set_diagmesg)
	SVC(enable_dvma)
	SVC(disable_dvma)
	SVC(l15_async_fault)
	STB(flush_writebuffers)
	STB(flush_writebuffers_to)
	SVC(memerr_init)
	SVC(memerr_disable)
	SVC(impl_bustype)
	SVC(process_aflt)
	retl
	nop
	SET_SIZE(small_sun4m_sys_setfunc)

#define	PA_SMALL4M_SYSCTL  0x71F00000 /* asi=20: system control/status */
#define ASI_SMALL4M_CTL ASI_MEM
/* XXX */
#define PA_SMALL4M_MID	   0x10002000 /* asi=20: module/sbae register */
#define SMALL4M_SBAE	   0x1f0000

	ENTRY(small_sun4m_get_sysctl)
	set 	PA_SMALL4M_SYSCTL, %o1
	retl
	lda	[%o1]ASI_SMALL4M_CTL, %o0
	SET_SIZE(small_sun4m_get_sysctl)

	ENTRY(small_sun4m_set_sysctl)
	set 	PA_SMALL4M_SYSCTL, %o1
	retl
	sta	%o0, [%o1]ASI_SMALL4M_CTL
	SET_SIZE(small_sun4m_set_sysctl)

	ENTRY(small_sun4m_mmu_getasyncflt)
	sub     %g0, 1, %o1
	retl
	st      %o1, [%o0]			! return -1 at offset 0
	SET_SIZE(small_sun4m_mmu_getasyncflt)

        ENTRY(small_sun4m_mmu_chk_wdreset)
        set     PA_SMALL4M_SYSCTL, %o1          ! system control/status
        lda     [%o1]ASI_SMALL4M_CTL, %o0
        retl
        and     %o0, SYSCTLREG_WD, %o0          ! mask WD reset bit
        SET_SIZE(small_sun4m_mmu_chk_wdreset)

	ENTRY(small_sun4m_enable_dvma)
        set     PA_SMALL4M_MID, %o1
	lda 	[%o1]ASI_SMALL4M_CTL, %o0
	set 	SMALL4M_SBAE, %o2
	or	%o0, %o2, %o0
	retl
        sta     %o0, [%o1]ASI_SMALL4M_CTL
	SET_SIZE(small_sun4m_enable_dvma)

	ENTRY(small_sun4m_disable_dvma)
        set     PA_SMALL4M_MID, %o1
	lda 	[%o1]ASI_SMALL4M_CTL, %o0
	set 	SMALL4M_SBAE, %o2
	andn	%o0, %o2, %o0
	retl
        sta     %o0, [%o1]ASI_SMALL4M_CTL
	SET_SIZE(small_sun4m_disable_dvma)

small_sun4m_stub:
	retl
	clr	%o0

#endif	/* lint */


