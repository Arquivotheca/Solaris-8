/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * assembly code interface to flexible module routines
 */

#pragma ident	"@(#)module_srmmu_asm.s	1.5	96/02/27 SMI" /* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/physaddr.h>
#include "assym.s"

#if !defined(lint)

	.seg	".text"
	.align	4


	ENTRY(srmmu_mmu_getctp)		! int	mmu_getctp(void);
	set	RMMU_CTP_REG, %o1	! get srmmu context table ptr
	retl
	lda	[%o1]ASI_MOD, %o0


	ENTRY(srmmu_mmu_flushall)	! void	srmmu_mmu_flushall(void)
	or	%g0, FT_ALL<<8, %o0	! flush entire mmu
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	retl
	nop				! MMU delay


	ENTRY(srmmu_mmu_flushpage)	! void srmmu_mmu_flushpage(caddr_t base)
	or	%o0, FT_PAGE<<8, %o0

	sta	%g0, [%o0]ASI_FLPR	! do the flush
	retl
	nop				! PSR or MMU delay
#endif	/* !defined(lint) */
