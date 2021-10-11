/*
 * Copyright (c) 1990, 1993, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subr_4u.s	1.60	99/07/21 SMI"

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/machsystm.h>
#include <sys/t_lock.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/eeprom.h>
#include <sys/param.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include <sys/iocache.h>
#include <sys/privregs.h>
#include <sys/archsystm.h>
#include <sys/clock.h>

#if defined(lint)

/*ARGSUSED*/
int
getprocessorid(void)
{ return (0); }

#else	/* lint */

/*
 * Get the processor ID.
 * === MID reg as specified in 15dec89 sun4u spec, sec 5.4.3
 */

	ENTRY(getprocessorid)
	CPU_INDEX(%o0, %o1)
	retl
	nop
	SET_SIZE(getprocessorid)

#endif	/* lint */

#if defined(lint)
caddr_t
set_trap_table(void)
{
	return ((caddr_t)0);
}
#else /* lint */

	ENTRY(set_trap_table)
	set	trap_table, %o1
	rdpr	%tba, %o0
	wrpr	%o1, %tba
	retl
	wrpr	%g0, WSTATE_KERN, %wstate
	SET_SIZE(set_trap_table)

#endif /* lint */

#if defined(lint)
/*ARGSUSED*/
void
set_error_enable_tl1(uint64_t neer, uint64_t dummy)
{}

/* ARGSUSED */
void
set_error_enable(uint64_t neer)
{}

uint64_t
get_error_enable()
{
	return ((uint64_t)0);
}
#else /* lint */

	ENTRY(set_error_enable_tl1)
	stxa	%g1, [%g0]ASI_ESTATE_ERR	/* ecache error enable reg */
	membar	#Sync
	retry
	SET_SIZE(set_error_enable_tl1)

	ENTRY(set_error_enable)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#endif
	stxa	%o0, [%g0]ASI_ESTATE_ERR	/* ecache error enable reg */
	retl
	membar	#Sync
	SET_SIZE(set_error_enable)

	ENTRY(get_error_enable)
#ifndef __sparcv9
	ldxa	[%g0]ASI_ESTATE_ERR, %o0	/* ecache error enable reg */
	srl	%o0, 0, %o1	! put lower 32 bits in o1, clear upper 32 bits
	retl
	srlx	%o0, 32, %o0	! put the high 32 bits in low part of o0
#else
	retl
	ldxa	[%g0]ASI_ESTATE_ERR, %o0	/* ecache error enable reg */
#endif
	SET_SIZE(get_error_enable)

#endif /* lint */

#if defined(lint)
void
get_asyncflt(uint64_t *afsr)
{
	afsr = afsr;
}
#else /* lint */

	ENTRY(get_asyncflt)
	ldxa	[%g0]ASI_AFSR, %o1		! afsr reg
	retl
	stx	%o1, [%o0]
	SET_SIZE(get_asyncflt)

#endif /* lint */

#if defined(lint)
void
set_asyncflt(uint64_t afsr)
{
	afsr = afsr;
}
#else /* lint */

	ENTRY(set_asyncflt)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using(%o0, %o1)
#endif
	stxa	%o0, [%g0]ASI_AFSR		! afsr reg
	retl
	membar	#Sync
	SET_SIZE(set_asyncflt)

#endif /* lint */

#if defined(lint)
void
get_asyncaddr(uint64_t *afar)
{
	afar = afar;
}
#else /* lint */

	ENTRY(get_asyncaddr)
	ldxa	[%g0]ASI_AFAR, %o1		! afar reg
	retl
	stx	%o1, [%o0]
	SET_SIZE(get_asyncaddr)

#endif /* lint */

#if defined(lint)
/* ARGSUSED */
void
scrubphys(uint64_t paddr, int ecache_size)
{
}

#else	/* lint */

/*
 * scrubphys - Pass in the aligned physical memory address that you want
 * to scrub, along with the ecache size.
 *
 *	1) Displacement flush the E$ line corresponding to %addr.
 *	   The first ldxa guarantees that the %addr is no longer in
 *	   M, O, or E (goes to I or S (if instruction fetch also happens).
 *	2) "Write" the data using a CAS %addr,%g0,%g0.
 *	   The casxa guarantees a transition from I to M or S to M.
 *	3) Displacement flush the E$ line corresponding to %addr.
 *	   The second ldxa pushes the M line out of the ecache, into the
 *	   writeback buffers, on the way to memory.
 *	4) The "membar #Sync" pushes the cache line out of the writeback
 *	   buffers onto the bus, on the way to dram finally.
 *
 * This is a modified version of the algorithm suggested by Gary Lauterbach.
 * In theory the CAS %addr,%g0,%g0 is supposed to mark the addr's cache line
 * as modified, but then we found out that for spitfire, if it misses in the
 * E$ it will probably install as an M, but if it hits in the E$, then it
 * will stay E, if the store doesn't happen. So the first displacement flush
 * should ensure that the CAS will miss in the E$.  Arrgh.
 */

	ENTRY(scrubphys)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#else
	or	%o1, %g0, %o2	! put ecache size in %o2
#endif
	xor	%o0, %o2, %o1	! calculate alias address
	add	%o2, %o2, %o3	! 2 * ecachesize in case
				! addr == ecache_flushaddr
	sub	%o3, 1, %o3	! -1 == mask
	and	%o1, %o3, %o1	! and with xor'd address
	set	ecache_flushaddr, %o3
	ldx	[%o3], %o3

	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, %g0, %pstate	! clear IE, AM bits

	ldxa	[%o1 + %o3]ASI_MEM, %g0 ! load ecache_flushaddr + alias
	casxa	[%o0]ASI_MEM, %g0, %g0
	ldxa	[%o1 + %o3]ASI_MEM, %g0	! load ecache_flushaddr + alias
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value

	retl
	membar	#Sync			! move the data out of the load buffer
	SET_SIZE(scrubphys)

#endif	/* lint */

#if defined(lint)

/*
 * clearphys - Pass in the aligned physical memory address that you want
 * to push out, as a 64 byte block of zeros, from the ecache zero-filled.
 */
/* ARGSUSED */
void
clearphys(uint64_t paddr, int ecache_size)
{
}

#else	/* lint */

	ENTRY(clearphys)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#else
	or	%o1, %g0, %o2	! ecache size
#endif
	xor	%o0, %o2, %o1	! calculate alias address
	add	%o2, %o2, %o3	! 2 * ecachesize
	sub	%o3, 1, %o3	! -1 == mask
	and	%o1, %o3, %o1	! and with xor'd address
	set	ecache_flushaddr, %o3
	ldx	[%o3], %o3
	set	ecache_linesize, %o2
	ld	[%o2], %o2

	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, %g0, %pstate	! clear IE, AM bits

	! need to put zeros in the cache line before displacing it

1:
	stxa	%g0, [%o0 + %o2]ASI_MEM	! put zeros in the ecache line
	sub	%o2, 8, %o2
	brgez,a,pt %o2, 1b
	nop
	ldxa	[%o1 + %o3]ASI_MEM, %g0	! load ecache_flushaddr + alias
	casxa	[%o0]ASI_MEM, %g0, %g0
	ldxa	[%o1 + %o3]ASI_MEM, %g0	! load ecache_flushaddr + alias
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value

	retl
	membar	#Sync			! move the data out of the load buffer
	SET_SIZE(clearphys)

#endif	/* lint */

/*
 * Answer questions about any extended SPARC hardware capabilities.
 * On this platform, for now, it is NONE. XXXXX
 */

#if	defined(lint)

/*ARGSUSED*/
int
get_hwcap_flags(int inkernel)
{ return (0); }

#else   /* lint */

	ENTRY(get_hwcap_flags)
#define	FLAGS   AV_SPARC_HWMUL_32x32 | AV_SPARC_HWDIV_32x32 | AV_SPARC_HWFSMULD
	sethi	%hi(FLAGS), %o0
	retl
	or	%o0, %lo(FLAGS), %o0
#undef	FLAGS
	SET_SIZE(get_hwcap_flags)

#endif  /* lint */

#if defined(lint)

/*ARGSUSED*/
void
stphys(uint64_t physaddr, int value)
{}

/*ARGSUSED*/
int
ldphys(uint64_t physaddr)
{ return (0); }

/*ARGSUSED*/
void
stdphys(uint64_t physaddr, uint64_t value)
{}

/*ARGSUSED*/
uint64_t
lddphys(uint64_t physaddr)
{ return (0x0ull); }

/* ARGSUSED */
void
stphysio(u_longlong_t physaddr, u_int value)
{}

/* ARGSUSED */
u_int
ldphysio(u_longlong_t physaddr)
{ return(0); }

/* ARGSUSED */
void
sthphysio(u_longlong_t physaddr, ushort_t value)
{}
 
/* ARGSUSED */
ushort_t
ldhphysio(u_longlong_t physaddr)
{ return(0); }

/* ARGSUSED */
void
stbphysio(u_longlong_t physaddr, uchar_t value)
{}

/* ARGSUSED */
uchar_t
ldbphysio(u_longlong_t physaddr)
{ return(0); }

/*ARGSUSED*/
void
stdphysio(u_longlong_t physaddr, u_longlong_t value)
{}

/*ARGSUSED*/
u_longlong_t
lddphysio(u_longlong_t physaddr)
{ return (0ull); }

#else

	! Store long word value at physical address
	!
	! void  stdphys(uint64_t physaddr, uint64_t value)
	!
	ENTRY(stdphys)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

	sllx	%o2, 32, %o2	! shift upper 32 bits
	srl	%o3, 0, %o3	! clear upper 32 bits
	or	%o2, %o3, %g1	! form 64 bit value in %g1 using (%o2,%o3)
#endif
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef __sparcv9
	stxa	%o1, [%o0]ASI_MEM
#else
	stxa	%g1, [%o0]ASI_MEM
#endif
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(stdphys)


	! Store long word value at physical i/o address
	!
	! void  stdphysio(u_longlong_t physaddr, u_longlong_t value)
	!
	ENTRY(stdphysio)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

	sllx	%o2, 32, %o2	! shift upper 32 bits
	srl	%o3, 0, %o3	! clear upper 32 bits
	or	%o2, %o3, %g1	! form 64 bit value in %g1 using (%o2,%o3)
#endif
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate		! clear IE, AM bits
#ifdef __sparcv9
	stxa	%o1, [%o0]ASI_IO
#else
	stxa	%g1, [%o0]ASI_IO
#endif
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(stdphysio)


	!
	! Load long word value at physical address
	!
	! uint64_t lddphys(uint64_t physaddr)
	!
	ENTRY(lddphys)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#endif
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef __sparcv9
	ldxa	[%o0]ASI_MEM, %o0
#else
	ldxa	[%o0]ASI_MEM, %g1
	srlx	%g1, 32, %o0	! put the high 32 bits in low part of o0
	srl	%g1, 0, %o1	! put lower 32 bits in o1, clear upper 32 bits
#endif
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(lddphys)

	!
	! Load long word value at physical i/o address
	!
	! unsigned long long lddphysio(u_longlong_t physaddr)
	!
	ENTRY(lddphysio)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#endif
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate	! clear IE, AM bits
#ifdef __sparcv9
	ldxa	[%o0]ASI_IO, %o0
#else
	ldxa	[%o0]ASI_IO, %g1
	srlx	%g1, 32, %o0	! put the high 32 bits in low part of o0
	srl	%g1, 0, %o1	! put lower 32 bits in o1, clear upper 32 bits
#endif
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(lddphysio)

	!
	! Store value at physical address
	!
	! void  stphys(uint64_t physaddr, int value)
	!
	ENTRY(stphys)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#endif
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef __sparcv9
	sta	%o1, [%o0]ASI_MEM
#else
	sta	%o2, [%o0]ASI_MEM
#endif
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(stphys)


	!
	! load value at physical address
	!
	! int   ldphys(uint64_t physaddr)
	!
	ENTRY(ldphys)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)
#endif
	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
	lda	[%o0]ASI_MEM, %o0
	srl	%o0, 0, %o0	! clear upper 32 bits
	retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(ldphys)

	!
	! Store value into physical address in I/O space
	!
	! void stphysio(u_longlong_t physaddr, u_int value)
	!
	ENTRY_NP(stphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef	__sparcv9
	stwa	%o1, [%o0]ASI_IO	/* store value via bypass ASI */
#else
	stwa	%o2, [%o0]ASI_IO
#endif
	retl
	wrpr	%g0, %o4, %pstate	/* restore the PSTATE */
	SET_SIZE(stphysio)

	!
	! Store value into physical address in I/O space
	!
	! void sthphysio(u_longlong_t physaddr, ushort_t value)
	!
	ENTRY_NP(sthphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef	__sparcv9
	stha	%o1, [%o0]ASI_IO        /* store value via bypass ASI */
#else
	stha	%o2, [%o0]ASI_IO
#endif
	retl
	wrpr	%g0, %o4, %pstate		/* restore the PSTATE */
	SET_SIZE(sthphysio)

	!
	! Store value into one byte physical address in I/O space
	!
	! void stbphysio(u_longlong_t physaddr, uchar_t value)
	!
	ENTRY_NP(stbphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
#ifdef	__sparcv9
	stba	%o1, [%o0]ASI_IO	/* store byte via bypass ASI */
#else
	stba	%o2, [%o0]ASI_IO
#endif
	retl
	wrpr	%g0, %o4, %pstate	/* restore the PSTATE */
	SET_SIZE(stbphysio)

	!
	! load value at physical address in I/O space
	!
	! u_int   ldphysio(u_longlong_t physaddr)
	!
	ENTRY_NP(ldphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
	lduwa	[%o0]ASI_IO, %o0	/* load value via bypass ASI */
	retl
	wrpr	%g0, %o4, %pstate	/* restore pstate */
	SET_SIZE(ldphysio)

	!
	! load value at physical address in I/O space
	!
	! ushort_t   ldhphysio(u_longlong_t physaddr)
	!
	ENTRY_NP(ldhphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
	lduha	[%o0]ASI_IO, %o0	/* load value via bypass ASI */
	retl
	wrpr	%g0, %o4, %pstate	/* restore pstate */
	SET_SIZE(ldhphysio)

	!
	! load byte value at physical address in I/O space
	!
	! uchar_t   ldbphysio(u_longlong_t physaddr)
	!
	ENTRY_NP(ldbphysio)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o5
	wrpr	%o5, 0, %pstate
	lduba	[%o0]ASI_IO, %o0	/* load value via bypass ASI */
	retl
	wrpr	%g0, %o4, %pstate	/* restore pstate */
	SET_SIZE(ldbphysio)
#endif  /* lint */

/*
 * save_gsr(kfpu_t *fp)
 * Store the graphics status register
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
save_gsr(kfpu_t *fp)
{}

#else	/* lint */

	ENTRY_NP(save_gsr)
	rd	%gsr, %g2			! save gsr
	retl
	stx	%g2, [%o0 + FPU_GSR]
	SET_SIZE(save_gsr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
restore_gsr(kfpu_t *fp)
{}

#else	/* lint */

	ENTRY_NP(restore_gsr)
	ldx	[%o0 + FPU_GSR], %g2
	wr	%g2, %g0, %gsr
	retl
	nop
	SET_SIZE(restore_gsr)

#endif	/* lint */

/*
 * uint64_t
 * get_gsr(kfpu_t *fp)
 * Get the graphics status register info from fp and return it
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
uint64_t
get_gsr(kfpu_t *fp)
{ return 0; }

#else	/* lint */

	ENTRY_NP(get_gsr)
#ifndef __sparcv9
	ldx	[%o0 + FPU_GSR], %o1
	srlx	%o1, 32, %o0	! move upper 32 bits to %o0
	retl
	sll	%o1, 0, %o1	! clear upper 32 bits in %o1
#else
	retl
	ldx	[%o0 + FPU_GSR], %o0
#endif
	SET_SIZE(get_gsr)

#endif	/* lint */

/*
 * set_gsr(uint64_t *buf, kfpu_t *fp)
 * Set the graphics status register info to fp from buf
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
set_gsr(uint64_t buf, kfpu_t *fp)
{}

#else	/* lint */

	ENTRY_NP(set_gsr)
#ifndef __sparcv9
	sllx	%o0, 32, %o0	! shift upper 32 bits to correct spot in %o0
	or	%o1, %o0, %o0	! or in the lower 32 bits to %o0
	retl
	stx	%o0, [%o2 + FPU_GSR]
#else
	retl
	stx	%o0, [%o1 + FPU_GSR]
#endif
	SET_SIZE(set_gsr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
u_int
find_cpufrequency(volatile uchar_t *clock_ptr)
{
	return (0);
}

#else	/* lint */

#ifdef DEBUG
	.seg	".data"
	.align	4
find_cpufreq_panic:
	.asciz	"find_cpufrequency: interrupts already disabled on entry"
#endif

	ENTRY_NP(find_cpufrequency)

	rdpr	%pstate, %g1

#ifdef DEBUG
	andcc	%g1, PSTATE_IE, %g0	! If DEBUG, check that interrupts
	bnz	0f			! are currently enabled
	sethi	%hi(find_cpufreq_panic), %o1
	call	panic
	or	%o1, %lo(find_cpufreq_panic), %o0
#endif

0:
	wrpr	%g1, PSTATE_IE, %pstate	! Disable interrupts
3:
	ldub	[%o0], %o1		! Read the number of seconds
	mov	%o1, %o2		! remember initial value in %o2
1:
	GET_NATIVE_TIME(%o3)		! get a timestamp
	cmp	%o1, %o2		! did the seconds register roll over?
	be,pt	%icc, 1b		! branch back if unchanged
	ldub	[%o0], %o2		!   delay: load the new seconds val

	brz,pn	%o2, 3b			! if the minutes just rolled over,
					! the last second could have been
					! inaccurate; try again.
	mov	%o2, %o4		!   delay: store init. val. in %o2
2:
	GET_NATIVE_TIME(%o5)		! get a timestamp
	cmp	%o2, %o4		! did the seconds register roll over?
	be,pt	%icc, 2b		! branch back if unchanged
	ldub	[%o0], %o4		!   delay: load the new seconds val

	brz,pn	%o4, 0b			! if the minutes just rolled over,
					! the last second could have been
					! inaccurate; try again.
	wrpr	%g0, %g1, %pstate	!   delay: re-enable interrupts

	retl
	sub	%o5, %o3, %o0		! return the difference in ticks

	SET_SIZE(find_cpufrequency)

#endif
