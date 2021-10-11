/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)watchpt.s	1.5	96/09/10 SMI"

/*
 * Data breakpoint (watchpoint) support for Fusion.
 *
 * Watchpoints are enabled by setting the appropriate bits in the
 * LSU control register and writing the address to be watched to
 * the VA or PA data watchpoint register. The low 3 bits of the
 * address are ignored; the VA/PA mask bits control which byte(s)
 * within the 64-bit word are to be watched, e.g.:
 *
 *	Mask		7 6 5 4 3 2 1 0 (Address of bytes watched)
 *	====		===============
 *	0x01		0 0 0 0 0 0 0 1
 *	0x32		0 0 1 1 0 0 1 0
 *	0xff		1 1 1 1 1 1 1 1
 */

#if defined(lint)
#include <sys/types.h>
#endif

#include <sys/spitregs.h>
#include <sys/spitasi.h>
#include <sys/asm_linkage.h>

#define	VA_WP_REG	0x38		/* VA watchpoint register */
#define	PA_WP_REG	0x40		/* PA watchpoint register */
#define	VAWP_MASK_SHIFT	25		/* VM field in LSU */
#define	PAWP_MASK_SHIFT	33		/* PM field in LSU */

#ifdef	lint
/*ARGSUSED*/
void
wp_vaccess(caddr_t addr, char mask) {}		/* virt addr access (r/w) */
/*ARGSUSED*/
void
wp_vread(caddr_t addr, char mask) {}		/* virt read */
/*ARGSUSED*/
void
wp_vwrite(caddr_t addr, char mask) {}		/* virt write */
/*ARGSUSED*/
void
wp_paccess(caddr_t addr, char mask) {}		/* phys addr access (r/w) */
#else

	ENTRY(wp_vaccess)
	set	(LSU_VW | LSU_VR), %g2
	b	.wp_on_cmn
	mov	VA_WP_REG, %o3
	SET_SIZE(wp_vaccess)

	ENTRY(wp_vread)
	set	LSU_VR, %g2
	b	.wp_on_cmn
	mov	VA_WP_REG, %o3
	SET_SIZE(wp_vread)

	ENTRY(wp_vwrite)
	set	LSU_VW, %g2
	b	.wp_on_cmn
	mov	VA_WP_REG, %o3
	SET_SIZE(wp_vwrite)

	ENTRY(wp_paccess)
	set	(LSU_PW | LSU_PR), %g2
	b	.wp_on_cmn
	mov	PA_WP_REG, %o3
	SET_SIZE(wp_paccess)

.wp_on_cmn:
	ldxa	[%g0]ASI_LSU, %g1
	or	%g1, %g2, %g1
	sllx	%o1, VAWP_MASK_SHIFT, %o1
	or	%g1, %o1, %g1		! set wp mask
	stxa	%g1, [%g0]ASI_LSU	! set appropriate LSU bits
	stxa	%o0, [%o3]ASI_DMMU	! stuff watchpoint addr
	retl
	membar	#Sync

#endif	/* lint */

#ifdef	lint
void
wp_clrall() {}		/* clr all watch points */
#else
	ENTRY(wp_clrall)
	
	b	.clr_cmn
	! clear VW/VR/PW/PR in LSU
        set     (LSU_VW | LSU_VR | LSU_PW | LSU_PR), %g2
#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
wp_off(caddr_t addr) {}			/* turn off wp */
#else

	ENTRY(wp_off)

	! match addr with either physical or virtual wp register
	! and turn off appropriate bits and mask in the LSU.

	mov	VA_WP_REG, %o3
	ldxa	[%o3]ASI_DMMU, %g1
	andn	%o0, 0x7, %o1		! clear low 3 bits before comparing
	cmp	%g1, %o1		! addr is virtual wp?
	beq,a	.clr_cmn
	set	(LSU_VW | LSU_VR), %g2	! clear VW/VR in LSU

	mov	PA_WP_REG, %o3
	ldxa	[%o3]ASI_DMMU, %g1
	cmp	%g1, %o1		! addr is physical wp?
	beq,a	.clr_cmn
	set	(LSU_PW | LSU_PR), %g2	! clear PW/PR in LSU

	sethi	%hi(_wp_notfound), %o0
	call	prom_panic		! can't find previously set wp
	or	%o0, %lo(_wp_notfound), %o0

.clr_cmn:
	ldxa	[%g0]ASI_LSU, %g1
	andn	%g1, %g2, %g1
	stxa	%g1, [%g0]ASI_LSU
	retl
	membar	#Sync
	SET_SIZE(wp_off)
	SET_SIZE(wp_clrall)

	.seg	".data"
	.align	4
_wp_notfound:
	.asciz	"wp_off: watchpoint not found"

#endif	/* lint */
