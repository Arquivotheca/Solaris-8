/*
 *	Copyright (c) 1990 - 1991 by Sun Microsystems, Inc.
 *
 * "mp" layer for module interface. slides into the same
 * hooks as a normal module interface, replacing the service
 * routines for the specific module with linkages that will
 * force crosscalls.
 *
 */

#ident	"@(#)module_mp_asm.s	1.12	97/05/24 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>

#if defined(lint)

void
mp_mmu_flushall(void)
{}

/* ARGSUSED */
void
mp_mmu_flushctx(u_int c_num)
{}

/* ARGSUSED */
void
mp_mmu_flushrgn(caddr_t addr)
{}

/* ARGSUSED */
void
mp_mmu_flushseg(caddr_t addr)
{}

void
mp_mmu_flushpage(void)
{}

void
mp_mmu_flushpagectx(void)
{}

void
mp_vac_usrflush(void)
{}

void
mp_vac_color_flush(void)
{}

void
mp_vac_ctxflush(void)
{}

void
mp_vac_rgnflush(void)
{}

void
mp_vac_segflush(void)
{}

void
mp_vac_pageflush(void)
{}

void
mp_vac_flush(void)
{}

void
mp_vac_allflush(void)
{}


void
mp_pac_pageflush(void)
{}

/* IFLUSH - mp ICACHE flush */
void
mp_ic_flush(void)
{}

#else	/* lint */

	.seg	".text"
	.align	4

/*
 * REVEC: indirect call via xc_sync_cache
 */
#define	REVEC(name)				\
	sethi	%hi(s_/**/name), %o3		; \
	b	xc_sync_cache			; \
	ld	[%o3+%lo(s_/**/name)], %o3

/*
 * FAST_REVEC: similar to REVEC, except that if reg has FL_LOCALCPU
 *	bit set, just call s_XXX() ie. just execute function on the local
 *	CPU. There is no need to do a xc_sync_cache [ ie. execute that
 *	function on all CPUS ].
 */
#define	FAST_REVEC(name, reg)				\
	andcc	reg, FL_LOCALCPU, %g0	; \
	sethi	%hi(s_/**/name), %o3		; \
	bz	0f				; \
	ld	[%o3+%lo(s_/**/name)], %o3	; \
	jmp	%o3				; \
	nop					; \
0:						; \
	b	xc_sync_cache			; \
	nop

	ENTRY(mp_mmu_flushall)
	REVEC(mmu_flushall)
	SET_SIZE(mp_mmu_flushall)

	ENTRY(mp_mmu_flushctx)
	FAST_REVEC(mmu_flushctx, %o3)
	SET_SIZE(mp_mmu_flushctx)

	ENTRY(mp_mmu_flushrgn)
	FAST_REVEC(mmu_flushrgn, %o3)
	SET_SIZE(mp_mmu_flushrgn)

	ENTRY(mp_mmu_flushseg)
	FAST_REVEC(mmu_flushseg, %o3)
	SET_SIZE(mp_mmu_flushseg)

	ENTRY(mp_mmu_flushpage)
	REVEC(mmu_flushpage)
	SET_SIZE(mp_mmu_flushpage)

	ENTRY(mp_mmu_flushpagectx)
	FAST_REVEC(mmu_flushpagectx, %o3)
	SET_SIZE(mp_mmu_flushpagectx)

	ENTRY(mp_vac_usrflush)
	FAST_REVEC(vac_usrflush, %o0)
	SET_SIZE(mp_vac_usrflush)

	ENTRY(mp_vac_ctxflush)
	FAST_REVEC(vac_ctxflush, %o1)
	SET_SIZE(mp_vac_ctxflush)

	ENTRY(mp_vac_rgnflush)
	FAST_REVEC(vac_rgnflush, %o2)
	SET_SIZE(mp_vac_rgnflush)

	ENTRY(mp_vac_segflush)
	FAST_REVEC(vac_segflush, %o2)
	SET_SIZE(mp_vac_segflush)

	ENTRY(mp_vac_pageflush)
	FAST_REVEC(vac_pageflush, %o2)
	SET_SIZE(mp_vac_pageflush)

	ENTRY(mp_vac_flush)
	REVEC(vac_flush)
	SET_SIZE(mp_vac_flush)

	ENTRY(mp_vac_allflush)
	FAST_REVEC(vac_allflush, %o0)
	SET_SIZE(mp_vac_allflush)

	ENTRY(mp_pac_pageflush)
	REVEC(pac_pageflush)
	SET_SIZE(mp_pac_pageflush)

	ENTRY(mp_vac_color_flush)
	REVEC(vac_color_flush)
	SET_SIZE(mp_vac_color_flush)

/* IFLUSH - mp ICACHE flush */
	ENTRY(mp_ic_flush)
	REVEC(ic_flush)
	SET_SIZE(mp_ic_flush)

#endif	/* lint	*/
