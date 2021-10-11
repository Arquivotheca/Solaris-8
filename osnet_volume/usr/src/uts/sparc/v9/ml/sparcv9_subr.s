/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sparcv9_subr.s	1.83	99/08/18 SMI"

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * independent of the specific kernel architecture, and those that are
 * common across kernel architectures.
 * As architectures diverge, and implementations of specific
 * architecture-dependent routines change, the routines should be moved
 * from this file into the respective ../`arch -k`/subr.s file.
 * Or, if you want to be really nice, move them to a file whose
 * name has something to do with the routine you are moving.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/scb.h>
#include <sys/systm.h>
#include <sys/regset.h>
#include <sys/sunddi.h>
#include <sys/lockstat.h>
#endif	/* lint */

#include <sys/atomic_prim.h>
#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/machparam.h>	/* To get SYSBASE and PAGESIZE */
#include <sys/machthread.h>
#include <sys/clock.h>
#include <sys/psr_compat.h>
#include <sys/isa_defs.h>
#include <sys/dditypes.h>
#include <sys/panic.h>

#if !defined(lint)
#include "assym.h"

	.seg	".text"
	.align	4

/*
 * Macro to raise processor priority level.
 * Avoid dropping processor priority if already at high level.
 * Also avoid going below CPU->cpu_base_spl, which could've just been set by
 * a higher-level interrupt thread that just blocked.
 *
 * level can be %o0 (not other regs used here) or a constant.
 */
#define	RAISE(level) \
	rdpr	%pil, %o1;		/* get current PIL */		\
	cmp	%o1, level;		/* is PIL high enough? */	\
	bge	1f;			/* yes, return */		\
	nop;								\
	wrpr	%g0, PIL_MAX, %pil;	/* freeze CPU_BASE_SPL */	\
	ldn	[THREAD_REG + T_CPU], %o2;				\
	ld	[%o2 + CPU_BASE_SPL], %o2;				\
	cmp	%o2, level;		/* compare new to base */	\
	movl	%xcc, level, %o2;	/* use new if base lower */	\
	wrpr	%g0, %o2, %pil;						\
1:									\
	retl;								\
	mov	%o1, %o0		/* return old PIL */

/*
 * Macro to raise processor priority level to level >= LOCK_LEVEL..
 * Doesn't require comparison to CPU->cpu_base_spl.
 *
 * newpil can be %o0 (not other regs used here) or a constant.
 */
#define	RAISE_HIGH(level) \
	rdpr	%pil, %o1;		/* get current PIL */		\
	cmp	%o1, level;		/* is PIL high enough? */	\
	bge	1f;			/* yes, return */		\
	nop;								\
	wrpr	%g0, level, %pil;	/* use chose value */		\
1:									\
	retl;								\
	mov	%o1, %o0		/* return old PIL */
	
/*
 * Macro to set the priority to a specified level.
 * Avoid dropping the priority below CPU->cpu_base_spl.
 *
 * newpil can be %o0 (not other regs used here) or a constant with
 * the new PIL in the PSR_PIL field of the level arg.
 */
#define SETPRI(level) \
	rdpr	%pil, %o1;		/* get current PIL */		\
	wrpr	%g0, PIL_MAX, %pil;	/* freeze CPU_BASE_SPL */	\
	ldn	[THREAD_REG + T_CPU], %o2;				\
	ld	[%o2 + CPU_BASE_SPL], %o2;				\
	cmp	%o2, level;		/* compare new to base */	\
	movl	%xcc, level, %o2;	/* use new if base lower */	\
	wrpr	%g0, %o2, %pil;						\
	retl;								\
	mov	%o1, %o0		/* return old PIL */

/*
 * Macro to set the priority to a specified level at or above LOCK_LEVEL.
 * Doesn't require comparison to CPU->cpu_base_spl.
 *
 * newpil can be %o0 (not other regs used here) or a constant with
 * the new PIL in the PSR_PIL field of the level arg.
 */
#define	SETPRI_HIGH(level) \
	rdpr	%pil, %o1;		/* get current PIL */		\
	wrpr	%g0, level, %pil;					\
	retl;								\
	mov	%o1, %o0		/* return old PIL */

#endif	/* lint */

	/*
	 * Berkley 4.3 introduced symbolically named interrupt levels
	 * as a way deal with priority in a machine independent fashion.
	 * Numbered priorities are machine specific, and should be
	 * discouraged where possible.
	 *
	 * Note, for the machine specific priorities there are
	 * examples listed for devices that use a particular priority.
	 * It should not be construed that all devices of that
	 * type should be at that priority.  It is currently were
	 * the current devices fit into the priority scheme based
	 * upon time criticalness.
	 *
	 * The underlying assumption of these assignments is that
	 * SPARC9 IPL 10 is the highest level from which a device
	 * routine can call wakeup.  Devices that interrupt from higher
	 * levels are restricted in what they can do.  If they need
	 * kernels services they should schedule a routine at a lower
	 * level (via software interrupt) to do the required
	 * processing.
	 *
	 * Examples of this higher usage:
	 *	Level	Usage
	 *	15	Asynchronous memory exceptions
	 *	14	Profiling clock (and PROM uart polling clock)
	 *	13	Audio device
	 *	12	Serial ports
	 *	11	Floppy controller
	 *
	 * The serial ports request lower level processing on level 6.
	 * Audio and floppy request lower level processing on level 4.
	 *
	 * Also, almost all splN routines (where N is a number or a
	 * mnemonic) will do a RAISE(), on the assumption that they are
	 * never used to lower our priority.
	 * The exceptions are:
	 *	spl8()		Because you can't be above 15 to begin with!
	 *	splzs()		Because this is used at boot time to lower our
	 *			priority, to allow the PROM to poll the uart.
	 *	spl0()		Used to lower priority to 0.
	 */

#if defined(lint)

int spl0(void)		{ return (0); }
int spl6(void)		{ return (0); }
int spl7(void)		{ return (0); }
int spl8(void)		{ return (0); }
int splhi(void)		{ return (0); }
int splhigh(void)	{ return (0); }
int splzs(void)		{ return (0); }

#else	/* lint */

	/* locks out all interrupts, including memory errors */
	ENTRY(spl8)
	SETPRI_HIGH(15)
	SET_SIZE(spl8)

	/* just below the level that profiling runs */
	ENTRY(spl7)
	RAISE_HIGH(13)
	SET_SIZE(spl7)

	/* sun specific - highest priority onboard serial i/o zs ports */
	ENTRY(splzs)
	SETPRI_HIGH(12)	/* Can't be a RAISE, as it's used to lower us */
	SET_SIZE(splzs)

	/*
	 * should lock out clocks and all interrupts,
	 * as you can see, there are exceptions
	 */
	ENTRY(splhi)
	ALTENTRY(splhigh)
	ALTENTRY(spl6)
	ALTENTRY(i_ddi_splhigh)
	RAISE_HIGH(10)
	SET_SIZE(i_ddi_splhigh)
	SET_SIZE(spl6)
	SET_SIZE(splhigh)
	SET_SIZE(splhi)

	/* allow all interrupts */
	ENTRY(spl0)
	SETPRI(0)
	SET_SIZE(spl0)

#endif	/* lint */

/*
 * splx - set PIL back to that indicated by the old %pil passed as an argument,
 * or to the CPU's base priority, whichever is higher.
 */

#if defined(lint)

/* ARGSUSED */
void
splx(int level)
{}

#else	/* lint */

	ENTRY(splx)
	ALTENTRY(i_ddi_splx)
	SETPRI(%o0)		/* set PIL */
	SET_SIZE(i_ddi_splx)
	SET_SIZE(splx)

#endif	/* level */

/*
 * splr()
 *
 * splr is like splx but will only raise the priority and never drop it
 * Be careful not to set priority lower than CPU->cpu_base_pri,
 * even though it seems we're raising the priority, it could be set higher
 * at any time by an interrupt routine, so we must block interrupts and
 * look at CPU->cpu_base_pri.
 */

#if defined(lint)

/* ARGSUSED */
int
splr(int level)
{ return (0); }

#else	/* lint */
	ENTRY(splr)
	RAISE(%o0)
	SET_SIZE(splr)

#endif	/* lint */

/*
 * on_fault()
 * Catch lofault faults. Like setjmp except it returns one
 * if code following causes uncorrectable fault. Turned off
 * by calling no_fault().
 */

#if defined(lint)

/* ARGSUSED */
int
on_fault(label_t *ljb)
{ return (0); }

#else	/* lint */

	ENTRY(on_fault)
	stn	%o0, [THREAD_REG + T_ONFAULT]
	set	catch_fault, %o1
	b	setjmp			! let setjmp do the rest
	stn	%o1, [THREAD_REG + T_LOFAULT]	! put catch_fault in u.u_lofault

catch_fault:
	save	%sp, -SA(WINDOWSIZE), %sp ! goto next window so that we can rtn
	ldn	[THREAD_REG + T_ONFAULT], %o0
	stn	%g0, [THREAD_REG + T_ONFAULT]	! turn off onfault
	b	longjmp			! let longjmp do the rest
	stn	%g0, [THREAD_REG + T_LOFAULT]	! turn off lofault
	SET_SIZE(on_fault)

#endif	/* lint */

/*
 * no_fault()
 * turn off fault catching.
 */

#if defined(lint)

void
no_fault(void)
{}

#else	/* lint */

	ENTRY(no_fault)
	stn	%g0, [THREAD_REG + T_ONFAULT]
	retl
	stn	%g0, [THREAD_REG + T_LOFAULT]	! turn off lofault
	SET_SIZE(no_fault)

#endif	/* lint */

/*
 * on_data_trap(), no_data_trap()
 * XXX doling out plenty of rope and tieing a noose!
 * These routines are used if one wants to avoid taking an MMU miss fault.
 * The t_nofault field allows a routine to bypass the trap handler and force
 * a failing return code when a data access trap happens.
 * These functions also hide the use of the no_fault field in the thread
 * structure from the caller.  If you call on_data_trap(), you must call
 * no_data_trap() to clean up correctly.
 */

#if defined(lint)

/* ARGSUSED */
int
on_data_trap(ddi_nofault_data_t *nf_data)
{ return (0); }

#else	/* lint */

	ENTRY(on_data_trap)
	set	NO_FAULT, %o1
	st	%o1, [%o0 + NF_OP_TYPE]		! Set NO_FAULT flag
	ldn	[THREAD_REG + T_NOFAULT], %o1	! Grab the nofault field
	stn	%o1, [%o0 + NF_SAVE_NOFAULT]	! Save the nofault field
	stn	%o0, [THREAD_REG + T_NOFAULT]	! Set the nofault data
	set	catch_nofault, %o1	! Get handler which catches nofaults
	stn	%o1, [%o0 + NF_PC]	! Set the handler in the PC field
	b	setjmp			! let setjmp do the rest
	add	%o0, NF_JMPBUF, %o0	! put setjmp buf into %o0

catch_nofault:
	ldn      [THREAD_REG + T_NOFAULT], %o0	! Get the nofault data
	b	longjmp				! let longjmp do the rest
	add	%o0, NF_JMPBUF, %o0		! Get the longjmp buf
	SET_SIZE(on_data_trap)

#endif	/* lint */

/*
 * no_data_trap()
 * turn off fault catching when t_nofault is set
 */

#if defined(lint)

/* ARGSUSED */
void
no_data_trap(ddi_nofault_data_t *nf_data)
{}

#else	/* lint */

	ENTRY(no_data_trap)
	ldn	[%o0 + NF_SAVE_NOFAULT], %o1	! Get the saved nofault data
	retl
	stn	%o1, [THREAD_REG + T_NOFAULT]	! restore the saved nofault
	SET_SIZE(no_data_trap)

#endif	/* lint */

/*
 * Setjmp and longjmp implement non-local gotos using state vectors
 * type label_t.
 */

#if defined(lint)

/* ARGSUSED */
int
setjmp(label_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(setjmp)
	stn	%o7, [%o0 + L_PC]	! save return address
	stn	%sp, [%o0 + L_SP]	! save stack ptr
	retl
	clr	%o0			! return 0
	SET_SIZE(setjmp)

#endif	/* lint */


#if defined(lint)

/* ARGSUSED */
void
longjmp(label_t *lp)
{}

#else	/* lint */

	ENTRY(longjmp)
	!
        ! The following save is required so that an extra register
        ! window is flushed.  Flushw flushes nwindows-2
        ! register windows.  If setjmp and longjmp are called from
        ! within the same window, that window will not get pushed
        ! out onto the stack without the extra save below.  Tail call
        ! optimization can lead to callers of longjmp executing
        ! from a window that could be the same as the setjmp,
        ! thus the need for the following save.
        !
	save    %sp, -SA(MINFRAME), %sp
	flushw				! flush all but this window
	ldn	[%i0 + L_PC], %i7	! restore return addr
	ldn	[%i0 + L_SP], %fp	! restore sp for dest on foreign stack
	ret				! return 1
	restore	%g0, 1, %o0		! takes underflow, switches stacks
	SET_SIZE(longjmp)

#endif	/* lint */

/*
 * movtuc(length, from, to, table)
 *
 * VAX movtuc instruction (sort of).
 */

#if defined(lint)

/*ARGSUSED*/
int
movtuc(size_t length, u_char *from, u_char *to, u_char table[])
{ return (0); }

#else	/* lint */

	ENTRY(movtuc)
	tst     %o0
	ble,pn	%ncc, 2f		! check length
	clr     %o4

	ldub    [%o1 + %o4], %g1        ! get next byte in string
0:
	ldub    [%o3 + %g1], %g1        ! get corresponding table entry
	tst     %g1                     ! escape char?
	bnz     1f
	stb     %g1, [%o2 + %o4]        ! delay slot, store it

	retl                            ! return (bytes moved)
	mov     %o4, %o0
1:
	inc     %o4                     ! increment index
	cmp     %o4, %o0                ! index < length ?
	bl,a,pt	%ncc, 0b
	ldub    [%o1 + %o4], %g1        ! delay slot, get next byte in string
2:
	retl                            ! return (bytes moved)
	mov     %o4, %o0
	SET_SIZE(movtuc)

#endif	/* lint */

/*
 * scanc(length, string, table, mask)
 *
 * VAX scanc instruction.
 */

#if defined(lint)

/*ARGSUSED*/
int
scanc(size_t length, u_char *string, u_char table[], u_char mask)
{ return (0); }

#else	/* lint */

	ENTRY(scanc)
	tst	%o0	
	ble,pn	%ncc, 1f		! check length
	clr	%o4
0:
	ldub	[%o1 + %o4], %g1	! get next byte in string
	cmp	%o4, %o0		! interlock slot, index < length ?
	ldub	[%o2 + %g1], %g1	! get corresponding table entry
	bge,pn	%ncc, 1f		! interlock slot
	btst	%o3, %g1		! apply the mask
	bz,a	0b
	inc	%o4			! delay slot, increment index
1:
	retl				! return(length - index)
	sub	%o0, %o4, %o0
	SET_SIZE(scanc)

#endif	/* lint */

/*
 * if a() calls b() calls caller(),
 * caller() returns return address in a().
 */

#if defined(lint)

caddr_t
caller(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller)
	retl
	mov	%i7, %o0
	SET_SIZE(caller)

#endif	/* lint */

/*
 * if a() calls callee(), callee() returns the
 * return address in a();
 */

#if defined(lint)

caddr_t
callee(void)
{ return (0); }

#else	/* lint */

	ENTRY(callee)
	retl
	mov	%o7, %o0
	SET_SIZE(callee)

#endif	/* lint */

/*
 * return the current frame pointer
 */

#if defined(lint)

greg_t
getfp(void)
{ return (0); }

#else	/* lint */

	ENTRY(getfp)
	retl
	mov	%fp, %o0
	SET_SIZE(getfp)

#endif	/* lint */

/*
 * Get vector base register
 */

#if defined(lint)

greg_t
gettbr(void)
{ return (0); }

#else	/* lint */

	ENTRY(gettbr)
	retl
	mov     %tbr, %o0
	SET_SIZE(gettbr)

#endif	/* lint */

/*
 * Get processor state register, V9 faked to look like V8.
 * Note: does not provide ccr.xcc and provides FPRS.FEF instead of
 * PSTATE.PEF, because PSTATE.PEF is always on in order to allow the
 * libc_psr memcpy routines to run without hitting the fp_disabled trap.
 */

#if defined(lint)

greg_t
getpsr(void)
{ return (0); }

#else	/* lint */

	ENTRY(getpsr)
	rd	%ccr, %o1			! get ccr
        sll	%o1, PSR_ICC_SHIFT, %o0		! move icc to V8 psr.icc
	rd	%fprs, %o1			! get fprs
	and	%o1, FPRS_FEF, %o1		! mask out dirty upper/lower
	sllx	%o1, PSR_FPRS_FEF_SHIFT, %o1	! shift fef to V8 psr.ef
        or	%o0, %o1, %o0			! or into psr.ef
        set	V9_PSR_IMPLVER, %o1		! SI assigned impl/ver: 0xef
        retl
        or	%o0, %o1, %o0			! or into psr.impl/ver
	SET_SIZE(getpsr)

#endif	/* lint */

/*
 * Get current processor interrupt level
 */

#if defined(lint)

u_int
getpil(void)
{ return (0); }

#else	/* lint */

	ENTRY(getpil)
	retl
	rdpr	%pil, %o0
	SET_SIZE(getpil)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
setpil(u_int pil)
{}

#else	/* lint */

	ENTRY(setpil)
	retl
	wrpr	%g0, %o0, %pil
	SET_SIZE(setpil)

#endif	/* lint */


/*
 * _insque(entryp, predp)
 *
 * Insert entryp after predp in a doubly linked list.
 */

#if defined(lint)

/*ARGSUSED*/
void
_insque(caddr_t entryp, caddr_t predp)
{}

#else	/* lint */

	ENTRY(_insque)
	ldn	[%o1], %g1		! predp->forw
	stn	%o1, [%o0 + CPTRSIZE]	! entryp->back = predp
	stn	%g1, [%o0]		! entryp->forw = predp->forw
	stn	%o0, [%o1]		! predp->forw = entryp
	retl
	stn	%o0, [%g1 + CPTRSIZE]	! predp->forw->back = entryp
	SET_SIZE(_insque)

#endif	/* lint */

/*
 * _remque(entryp)
 *
 * Remove entryp from a doubly linked list
 */

#if defined(lint)

/*ARGSUSED*/
void
_remque(caddr_t entryp)
{}

#else	/* lint */

	ENTRY(_remque)
	ldn	[%o0], %g1		! entryp->forw
	ldn	[%o0 + CPTRSIZE], %g2	! entryp->back
	stn	%g1, [%g2]		! entryp->back->forw = entryp->forw
	retl
	stn	%g2, [%g1 + CPTRSIZE]	! entryp->forw->back = entryp->back
	SET_SIZE(_remque)

#endif	/* lint */

/*
 * strlen(str), ustrlen(str)
 *
 * Returns the number of non-NULL bytes in string argument.
 *
 * ustrlen is to accesss user address space. It assumes it is being called
 * in the context on an onfault setjmp.
 *
 * XXX -  why is this here, rather than the traditional file?
 *	  why does it have local labels which don't start with a `.'?
 */

#if defined(lint)

/*ARGSUSED*/
size_t
strlen(const char *str)
{ return (0); }

/*ARGSUSED*/
size_t
ustrlen(const char *str)
{ return (0); }

#else	/* lint */

	ENTRY(ustrlen)
	ba	.genstrlen
	wr	%g0, ASI_USER, %asi
	
	ENTRY(strlen)
	wr	%g0, ASI_P, %asi
.genstrlen:
	mov	%o0, %o1
	andcc	%o1, 3, %o3		! is src word aligned
	bz	$nowalgnd
	clr	%o0			! length of non-zero bytes
	cmp	%o3, 2			! is src half-word aligned
	be	$s2algn
	cmp	%o3, 3			! src is byte aligned
	lduba	[%o1]%asi, %o3		! move 1 or 3 bytes to align it
	inc	1, %o1			! in either case, safe to do a byte
	be	$s3algn
	tst	%o3
$s1algn:
	bnz,a	$s2algn			! now go align dest
	inc	1, %o0
	b,a	$done

$s2algn:
	lduha	[%o1]%asi, %o3		! know src is half-byte aligned
	inc	2, %o1
	srl	%o3, 8, %o4
	tst	%o4			! is the first byte zero
	bnz,a	1f
	inc	%o0
	b,a	$done
1:	andcc	%o3, 0xff, %o3		! is the second byte zero
	bnz,a	$nowalgnd
	inc	%o0
	b,a	$done
$s3algn:
	bnz,a	$nowalgnd
	inc	1, %o0
	b,a	$done

$nowalgnd:
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this allways works, otherwise
	! there is a specil case that rarely happens, see below

	set	0x7efefeff, %o3
	set	0x81010100, %o4

3:	lda	[%o1]%asi, %o2		! main loop
	inc	4, %o1
	add	%o2, %o3, %o5		! generate byte-carries
	xor	%o5, %o2, %o5		! see if orignal bits set
	and	%o5, %o4, %o5
	cmp	%o5, %o4		! if ==,  no zero bytes
	be,a	3b
	inc	4, %o0

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again

	sethi	%hi(0xff000000), %o5	! mask used to test for terminator
	andcc	%o2, %o5, %g0		! check if first byte was zero
	bnz	1f
	srl	%o5, 8, %o5
$done:
	retl
	nop
1:	andcc	%o2, %o5, %g0		! check if second byte was zero
	bnz	1f
	srl	%o5, 8, %o5
$done1:
	retl
	inc	%o0
1:	andcc 	%o2, %o5, %g0		! check if third byte was zero
	bnz	1f
	andcc	%o2, 0xff, %g0		! check if last byte is zero
$done2:
	retl
	inc	2, %o0
1:	bnz,a	3b
	inc	4, %o0			! count of bytes
$done3:
	retl
	inc	3, %o0
	SET_SIZE(strlen)
	SET_SIZE(ustrlen)

#endif	/* lint */

/*
 * copyinstr_noerr(s1, s2, len)
 *
 * Copy string s2 to s1.  s1 must be large enough and len contains the
 * number of bytes copied.  s1 is returned to the caller.
 * s2 is user space and s1 is in kernel space
 * 
 * knstrcpy(s1, s2, len)
 *
 * This routine copies a string s2 in the kernel address space to string
 * s2 which is also in the kernel address space.
 *
 * XXX so why is the third parameter *len?
 *	  why is this in this file, rather than the traditional?
 */

#if defined(lint)

/*ARGSUSED*/
char *
copyinstr_noerr(char *s1, const char *s2, size_t *len)
{ return ((char *)0); }

/*ARGSUSED*/
char *
knstrcpy(char *s1, const char *s2, size_t *len)
{ return ((char *)0); }

#else	/* lint */

#define	DEST	%i0
#define	SRC	%i1
#define LEN	%i2
#define DESTSV	%i5
#define ADDMSK	%l0
#define	ANDMSK	%l1
#define	MSKB0	%l2
#define	MSKB1	%l3
#define	MSKB2	%l4
#define SL	%o0
#define	SR	%o1
#define	MSKB3	0xff

	ENTRY(copyinstr_noerr)
	ba,pt	%icc, .do_cpy
	  wr	%g0, ASI_USER, %asi

	ALTENTRY(knstrcpy)
	wr	%g0, ASI_P, %asi

.do_cpy:
	save	%sp, -SA(WINDOWSIZE), %sp	! get a new window
	clr	%l6
	clr	%l7
	andcc	SRC, 3, %i3		! is src word aligned
	bz	.aldest
	mov	DEST, DESTSV		! save return value
	cmp	%i3, 2			! is src half-word aligned
	be	.s2algn
	cmp	%i3, 3			! src is byte aligned
	lduba	[SRC]%asi, %i3		! move 1 or 3 bytes to align it
	inc	1, SRC
	stb	%i3, [DEST]		! move a byte to align src
	be	.s3algn
	tst	%i3
.s1algn:
	bnz	.s2algn			! now go align dest
	inc	1, DEST
	b,a	.done
.s2algn:
	lduha	[SRC]%asi, %i3		! know src is 2 byte alinged
	inc	2, SRC
	srl	%i3, 8, %i4
	tst	%i4
	stb	%i4, [DEST]
	bnz,a	1f
	stb	%i3, [DEST + 1]
	inc	1, %l6
	b,a	.done
1:	andcc	%i3, MSKB3, %i3
	bnz	.aldest
	inc	2, DEST
	b,a	.done
.s3algn:
	bnz	.aldest
	inc	1, DEST
	b,a	.done

.aldest:
	set	0xfefefeff, ADDMSK	! masks to test for terminating null
	set	0x01010100, ANDMSK
	sethi	%hi(0xff000000), MSKB0
	sethi	%hi(0x00ff0000), MSKB1

	! source address is now aligned
	andcc	DEST, 3, %i3		! is destination word aligned?
	bz	.w4str
	srl	MSKB1, 8, MSKB2		! generate 0x0000ff00 mask
	cmp	%i3, 2			! is destination halfword aligned?
	be	.w2str
	cmp	%i3, 3			! worst case, dest is byte alinged
.w1str:
	lda	[SRC]%asi, %i3		! read a word
	inc	4, SRC			! point to next one
	be	.w3str
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz	1f
	andcc	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2
1:	bnz	1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	bz	.w1done4
	srl	%i3, 24, %o3		! move three bytes to align dest
	stb	%o3, [DEST]
	srl	%i3, 8, %o3
	sth	%o3, [DEST + 1]
	inc	3, DEST			! destination now aligned
	mov	%i3, %o3
	mov	24, SL
	b	8f			! inner loop same for w1str and w3str
	mov	8, SR			! shift amounts are different

2:	inc	4, DEST
8:	andcc	%l7, MSKB3, %g0		! check if exit flag is set
	bnz	3f
	sll	%o3, SL, %o2		! save remaining byte
	lda	[SRC]%asi, %o3		! read a word
	inc	4, SRC			! point to next one
3:	srl	%o3, SR, %i3
	addcc	%o3, ADDMSK, %l5	! check for a zero byte
	bcc	1f			! there must be one
	xor	%l5, %o3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a	4f			! if no zero byte in word,don't set flag
	nop
1:	inc	1, %l7			! Set exit flag
4:
	addcc	%i3, ADDMSK, %l5	! check for a zero byte
	bcc	1f			! there must be one
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a	2b			! if no zero byte in word, loop
	st	%i3, [DEST]

1:
	or	%i3, %o2, %i3
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz	1f
	andcc	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2
1:	bnz	1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz	8b			! if not zero, go read another word
	inc	4, DEST			! else finished
	b,a	.done

.w1done4:
	stb	%i3, [DEST + 3]
	inc	1, %l6
.w1done3:
	srl	%i3, 8, %o3
	stb	%o3, [DEST + 2]
	inc	1, %l6
.w1done2:
	srl	%i3, 24, %o3
	stb	%o3, [DEST]
	srl	%i3, 16, %o3
	inc	2, %l6
	b	.done
	stb	%o3, [DEST + 1]

.w3str:
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz	1f
	andcc	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2
1:	bnz	1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	bz	.w1done4
	srl	%i3, 24, %o3
	stb	%o3, [DEST]
	inc	1, DEST
	mov	%i3, %o3
	mov	8, SL
	b	8b			! inner loop same for w1str and w3str
	mov	24, SR			! shift amounts are different

.w2done4:
	srl	%i3, 16, %o3
	sth	%o3, [DEST]
	inc	4, %l6
	b	.done
	sth	%i3, [DEST + 2]

.w2str:
	lda	[SRC]%asi, %i3		! read a word
	inc	4, SRC			! point to next one
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bz	.done2

	srl	%i3, 16, %o3
	sth	%o3, [DEST]
	inc	2, DEST
	b	9f
	mov	%i3, %o3

2:	inc	4, DEST
9:	sll	%o3, 16, %i3		! save rest
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,a	1f
	lda	[SRC]%asi, %o3		! read a word
	b,a	.done2
1:	inc	4, SRC			! point to next one
	srl	%o3, 16, %o2
	or	%o2, %i3, %i3

	addcc	%i3, ADDMSK, %l5	! check for a zero byte
	bcc	1f			! there must be one
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a	2b			! if no zero byte in word, loop
	st	%i3, [DEST]

1:	andcc	%i3, MSKB2, %g0		! check if third byte was zero
	bnz	1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz	9b			! if not zero, go read another word
	inc	4, DEST			! else fall through
	b,a	.done

2:	inc	4, DEST
.w4str:
	lda	[SRC]%asi, %i3		! read a word
	inc	4, %i1			! point to next one

	addcc	%i3, ADDMSK, %l5	! check for a zero byte
	bcc	1f			! there must be one
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a	2b			! if no zero byte in word, loop
	st	%i3, [DEST]

1:	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz	1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz	1f
	andcc	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.done2
1:	bnz	1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz	.w4str			! if not zero, go read another word
	inc	4, DEST			! else fall through

.done:
	sub	DEST, DESTSV, %l0
	add	%l0, %l6, %l0
	stn	%l0, [LEN]
	ret			! last byte of word was the terminating zero
	restore	DESTSV, %g0, %o0

.done1:
	stb	%g0, [DEST]	! first byte of word was the terminating zero
	sub	DEST, DESTSV, %l0
	inc	1, %l6
	add	%l0, %l6, %l0
	stn	%l0, [LEN]
	ret
	restore	DESTSV, %g0, %o0

.done2:
	srl	%i3, 16, %i4	! second byte of word was the terminating zero
	sth	%i4, [DEST]
	sub	DEST, DESTSV, %l0
	inc	2, %l6
	add	%l0, %l6, %l0
	stn	%l0, [LEN]
	ret
	restore	DESTSV, %g0, %o0

.done3:
	srl	%i3, 16, %i4	! third byte of word was the terminating zero
	sth	%i4, [DEST]
	stb	%g0, [DEST + 2]
	sub	DEST, DESTSV, %l0
	inc	3, %l6
	add	%l0, %l6, %l0
	stn	%l0, [LEN]
	ret
	restore	DESTSV, %g0, %o0
	SET_SIZE(copyinstr_noerr)
	SET_SIZE(knstrcpy)

#endif	/* lint */


#ifdef	TRACE

/*
 * Provide a C callable interface to the tracing traps.
 */

#if defined(lint)

/* ARGSUSED */
void trace_0(u_long)
{}
/* ARGSUSED */
void trace_1(u_long, u_long)
{}
/* ARGSUSED */
void trace_2(u_long, u_long, u_long)
{}
/* ARGSUSED */
void trace_3(u_long, u_long, u_long, u_long)
{}
/* ARGSUSED */
void trace_4(u_long, u_long, u_long, u_long, u_long)
{}
/* ARGSUSED */
void trace_5(u_long, u_long, u_long, u_long, u_long, u_long)
{}
/* ARGSUSED */
void trace_write_buffer(u_long *, u_long)
{}
hrtime_t get_vtrace_time(void)
{
	return ((hrtime_t)0);
}
u_int get_vtrace_frequency(void)
{
	return (0);
}

#else	/* lint */

	ENTRY_NP(trace_0)
	ta	ST_TRACE_0; retl; nop;
	SET_SIZE(trace_0)

	ENTRY_NP(trace_1)
	ta	ST_TRACE_1; retl; nop;
	SET_SIZE(trace_1)

	ENTRY_NP(trace_2)
	ta	ST_TRACE_2; retl; nop;
	SET_SIZE(trace_2)

	ENTRY_NP(trace_3)
	ta	ST_TRACE_3; retl; nop;
	SET_SIZE(trace_3)

	ENTRY_NP(trace_4)
	ta	ST_TRACE_4; retl; nop;
	SET_SIZE(trace_4)

	ENTRY_NP(trace_5)
	ta	ST_TRACE_5; retl; nop;
	SET_SIZE(trace_5)

	ENTRY_NP(trace_write_buffer)
	ta	ST_TRACE_WRITE_BUFFER; retl; nop;
	SET_SIZE(trace_write_buffer)

	ENTRY_NP(get_vtrace_time)
	GET_VTRACE_TIME_64(%g1, %o0, %o1)		! %g1 = 64-bit time
	srlx	%g1, 32, %o0				! %o0 = hi32(%g1)
	retl
	srl	%g1, 0, %o1				! %o1 = lo32(%g1)
	SET_SIZE(get_vtrace_time)

	ENTRY_NP(get_vtrace_frequency)
	GET_VTRACE_FREQUENCY(%o0, %o1, %o2)
	retl
	nop
	SET_SIZE(get_vtrace_frequency)

/*
 * Fast traps to dump trace records.  Each uses only the trap window,
 * and leaves traps disabled.  They're all very similar, so we use
 * macros to generate the code -- see sparc9/sys/asm_linkage.h.
 * All trace traps are entered with %o0 = FTT2HEAD (fac, tag, event_info).
 */

	ENTRY_NP(trace_trap_0)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_0(%o0, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_0)

	ENTRY_NP(trace_trap_1)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_1(%o0, %o1, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_1)

	ENTRY_NP(trace_trap_2)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_2(%o0, %o1, %o2, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_2)

	ENTRY_NP(trace_trap_3)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_3(%o0, %o1, %o2, %o3, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_3)

	ENTRY_NP(trace_trap_4)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_4(%o0, %o1, %o2, %o3, %o4, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_4)

	ENTRY_NP(trace_trap_5)
	CPU_ADDR(%g1, %g2);
	TRACE_DUMP_5(%o0, %o1, %o2, %o3, %o4, %o5, %g1, %g2, %g3, %g4);
	done
	SET_SIZE(trace_trap_5)

	ENTRY_NP(trace_trap_write_buffer)
	CPU_ADDR(%g1, %g2);			! %g1 = CPU address
	andncc	%o1, 3, %g4			! %g4 = # of bytes to copy
	bz,pn	%xcc, _trace_trap_ret		! no data, return from trap
	ld	[%g1 + CPU_TRACE_THREAD], %g3	! last thread id
	tst	%g3				! NULL thread pointer?
	bz,pn	%xcc, _trace_trap_ret		! if NULL, bail out
	ld	[%g1 + CPU_TRACE_HEAD], %g3	! %g3 = buffer head address
	add	%g3, %g4, %g2			! %g2 = new buffer head
1:	subcc	%g4, 4, %g4
	ld	[%o0+%g4], %g5
	bg	1b
	st	%g5, [%g3+%g4]
	TRACE_DUMP_TAIL(%g1, %g2, %g3, %g4);
	ENTRY(_trace_trap_ret)
	done
	SET_SIZE(_trace_trap_ret)
	SET_SIZE(trace_trap_write_buffer)

#endif	/* lint */

#endif	/* TRACE */

/*
 * Provide a C callable interface to the membar instruction.
 */

#if defined(lint)

void
membar_ldld(void)
{}

void
membar_stld(void)
{}

void
membar_ldst(void)
{}

void
membar_stst(void)
{}

void
membar_ldld_ldst(void)
{}

void
membar_ldld_stld(void)
{}

void
membar_ldld_stst(void)
{}

void
membar_stld_ldld(void)
{}

void
membar_stld_ldst(void)
{}

void
membar_stld_stst(void)
{}

void
membar_ldst_ldld(void)
{}

void
membar_ldst_stld(void)
{}

void
membar_ldst_stst(void)
{}

void
membar_stst_ldld(void)
{}

void
membar_stst_stld(void)
{}

void
membar_stst_ldst(void)
{}

void
membar_lookaside(void)
{}

void
membar_memissue(void)
{}

void
membar_sync(void)
{}

#else
	ENTRY(membar_ldld)
	retl
	membar	#LoadLoad
	SET_SIZE(membar_ldld)

	ENTRY(membar_stld)
	retl
	membar	#StoreLoad
	SET_SIZE(membar_stld)

	ENTRY(membar_ldst)
	retl
	membar	#LoadStore
	SET_SIZE(membar_ldst)

	ENTRY(membar_stst)
	retl
	membar	#StoreStore
	SET_SIZE(membar_stst)

	ENTRY(membar_ldld_stld)
	ALTENTRY(membar_stld_ldld)
	retl
	membar	#LoadLoad|#StoreLoad
	SET_SIZE(membar_stld_ldld)
	SET_SIZE(membar_ldld_stld)

	ENTRY(membar_ldld_ldst)
	ALTENTRY(membar_ldst_ldld)
	retl
	membar	#LoadLoad|#LoadStore
	SET_SIZE(membar_ldst_ldld)
	SET_SIZE(membar_ldld_ldst)

	ENTRY(membar_ldld_stst)
	ALTENTRY(membar_stst_ldld)
	retl
	membar	#LoadLoad|#StoreStore
	SET_SIZE(membar_stst_ldld)
	SET_SIZE(membar_ldld_stst)

	ENTRY(membar_stld_ldst)
	ALTENTRY(membar_ldst_stld)
	retl
	membar	#StoreLoad|#LoadStore
	SET_SIZE(membar_ldst_stld)
	SET_SIZE(membar_stld_ldst)

	ENTRY(membar_stld_stst)
	ALTENTRY(membar_stst_stld)
	retl
	membar	#StoreLoad|#StoreStore
	SET_SIZE(membar_stst_stld)
	SET_SIZE(membar_stld_stst)

	ENTRY(membar_ldst_stst)
	ALTENTRY(membar_stst_ldst)
	retl
	membar	#LoadStore|#StoreStore
	SET_SIZE(membar_stst_ldst)
	SET_SIZE(membar_ldst_stst)

	ENTRY(membar_lookaside)
	retl
	membar	#Lookaside
	SET_SIZE(membar_lookaside)

	ENTRY(membar_memissue)
	retl
	membar	#MemIssue
	SET_SIZE(membar_memissue)

	ENTRY(membar_sync)
	retl
	membar	#Sync
	SET_SIZE(membar_sync)

#endif	/* lint */

/*
 * Primitives for atomic increments/decrements of a counter.
 * All routines expect an address and a positive or negative count.
 * The _nv versions return the new value; this is easy on sparc,
 * so atomic_add_XX() and atomic_add_XX_nv() are just aliases.
 */
#ifdef lint

void
atomic_add_16(uint16_t *target, int16_t delta)
{ *target += delta; }

void
atomic_add_32(uint32_t *target, int32_t delta)
{ *target += delta; }

void
atomic_add_long(ulong_t *target, long delta)
{ *target += delta; }

void
atomic_add_64(uint64_t *target, int64_t delta)
{ *target += delta; }

void
atomic_or_uint(uint_t *target, uint_t bits)
{ *target |= bits; }

void
atomic_or_32(uint32_t *target, uint32_t bits)
{ *target |= bits; }

void
atomic_and_uint(uint_t *target, uint_t bits)
{ *target &= bits; }

void
atomic_and_32(uint32_t *target, uint32_t bits)
{ *target &= bits; }

uint16_t
atomic_add_16_nv(uint16_t *target, int16_t delta)
{ return (*target += delta); }

uint32_t
atomic_add_32_nv(uint32_t *target, int32_t delta)
{ return (*target += delta); }

ulong_t
atomic_add_long_nv(ulong_t *target, long delta)
{ return (*target += delta); }

uint64_t
atomic_add_64_nv(uint64_t *target, int64_t delta)
{ return (*target += delta); }

#else /* lint */

/*
 * Atomic add on a halfword is a pain because cas doesn't support it,
 * so we load the nearest word and shift and mask it into submission.
 */
	ENTRY(atomic_add_16)
	ALTENTRY(atomic_add_16_nv)
	and	%o0, 2, %o4		! %o4 = cnt_in_upper_16 ? 0 : 2
	andn	%o0, 3, %o0		! %o0 = word address
	sll	%o4, 3, %o4		! %o4 = upper ? 0 : 16
	sethi	%hi(0xffff0000), %o3	! %o3 = mask
	ld	[%o0], %o2		! %o2 = old value (H:L)
	srl	%o3, %o4, %o3		! %o3 = upper ? ffff0000 : 0000ffff
	xor	%o4, 16, %g1		! %g1 = upper ? 16 : 0
	sll	%o1, %g1, %o1		! %o1 = upper ? (delta:0) : (0:delta)
1:
	add	%o2, %o1, %o4		! %o4 = (H+delta:L) : (H+carry:L+delta)
	andn	%o2, %o3, %o5		! %o5 = (0:L) : (H:0)
	and	%o4, %o3, %o4		! %o4 = (H+delta:0) : (0:L+delta)
	or	%o4, %o5, %o5		! %o5 = (H+delta:L) : (H:L+delta)
	cas	[%o0], %o2, %o5
	cmp	%o2, %o5
	bne,a,pn %icc, 1b
	ld	[%o0], %o2		! %o2 = old value (H:L)
	retl
	srl	%o4, %g1, %o0		! %o0 = (0:H+delta) : (0:L+delta)
	SET_SIZE(atomic_add_16_nv)
	SET_SIZE(atomic_add_16)

	ENTRY(atomic_add_32)
	ALTENTRY(atomic_add_32_nv)
#if !defined(__sparcv9)
	ALTENTRY(atomic_add_long)
	ALTENTRY(atomic_add_long_nv)
#endif
	ld	[%o0], %o2
1:
	add	%o2, %o1, %o3
	cas	[%o0], %o2, %o3
	cmp	%o2, %o3
	bne,a,pn %icc, 1b
	ld	[%o0], %o2
	retl
	add	%o2, %o1, %o0		! return new value
#if !defined(__sparcv9)
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_long)
#endif
	SET_SIZE(atomic_add_32_nv)
	SET_SIZE(atomic_add_32)

#ifdef __sparcv9

	ENTRY(atomic_add_long)
	ALTENTRY(atomic_add_long_nv)
	ALTENTRY(atomic_add_64)
	ALTENTRY(atomic_add_64_nv)
	ldx	[%o0], %o2
1:
	add	%o2, %o1, %o3
	casx	[%o0], %o2, %o3
	cmp	%o2, %o3
	bne,a,pn %xcc, 1b
	ldx	[%o0], %o2
	retl
	add	%o2, %o1, %o0		! return new value
	SET_SIZE(atomic_add_64)
	SET_SIZE(atomic_add_64_nv)
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_long)

#else	/* __sparcv9 */

	ENTRY(atomic_add_64)
	ALTENTRY(atomic_add_64_nv)
	sllx	%o1, 32, %o1
	srl	%o2, 0, %o2
	or	%o1, %o2, %o1		! %o1 = 64-bit delta
1:
	ldx	[%o0], %o2		! %o2 = old value
	add	%o2, %o1, %o3		! %o3 = proposed new value
	casx	[%o0], %o2, %o3		! try to store new value
	cmp	%o2, %o3		! did it take?
	bne,pn	%xcc, 1b		! if not, try again
	add	%o2, %o1, %o3		! delay: compute new value
	srlx	%o3, 32, %o0		! %o0 = hi32(new)
	retl
	srl	%o3, 0, %o1		! %o1 = lo32(new)
	SET_SIZE(atomic_add_64_nv)
	SET_SIZE(atomic_add_64)

#endif	/* __sparcv9 */

	ENTRY(atomic_or_uint)
	ALTENTRY(atomic_or_32)
	ld	[%o0], %o2
1:
	or	%o2, %o1, %o3
	cas	[%o0], %o2, %o3
	cmp	%o2, %o3
	bne,a,pn %icc, 1b
	ld	[%o0], %o2
	retl
	add	%o2, %o1, %o0		! return new value
	SET_SIZE(atomic_or_32)
	SET_SIZE(atomic_or_uint)

	ENTRY(atomic_and_uint)
	ALTENTRY(atomic_and_32)
	ld	[%o0], %o2
1:
	and	%o2, %o1, %o3
	cas	[%o0], %o2, %o3
	cmp	%o2, %o3
	bne,a,pn %icc, 1b
	ld	 [%o0], %o2
	retl
	add	%o2, %o1, %o0		! return new value
	SET_SIZE(atomic_and_32)
	SET_SIZE(atomic_and_uint)

#endif /* lint */

#ifdef lint
/*
 * These routines are for readers-writer lock where the lock is an u_int.
 * Note: Caller should not loop trying to grab the writer lock, since
 * these routines don't give any priority to a writer over readers.
 */

/* ARGSUSED */
void
rwlock_word_init(u_int *lock)
{}

/* ARGSUSED */
int
rwlock_word_enter(u_int *lock, int flag)
{ return (0); }

/* ARGSUSED */
void
rwlock_word_exit(u_int *lock, int flag)
{}

/*
 * These routines are for readers-writer lock where the lock is a u_short.
 * Note: Caller should not loop trying to grab the writer lock, since
 * these routines don't give any priority to a writer over readers.
 */

/* ARGSUSED */
void
rwlock_hword_init(u_short *lock)
{}

/* ARGSUSED */
int
rwlock_hword_enter(u_short *lock, int flag)
{ return (0); }

/* ARGSUSED */
void
rwlock_hword_exit(u_short *lock, int flag)
{}

#else /* lint */

#ifdef DEBUG
	.seg	".data"
rw_panic1:
	.asciz	"rwlock_hword_exit: write lock held"
rw_panic2:
	.asciz	"rwlock_hword_exit: reader lock not held"
rw_panic3:
	.asciz	"rwlock_hword_exit: write lock not held"
#endif /* DEBUG */

	.seg	".data"
rw_panic4:
	.asciz	"rwlock_hword_enter: # of reader locks equal WLOCK"

	ENTRY(rwlock_word_init)
	st	%g0, [%o0]		/* set rwlock to 0 */
	retl
	nop
	SET_SIZE(rwlock_word_init)

	ENTRY(rwlock_word_enter)
	cmp	%o1, READER_LOCK
	bne	%icc, 4f
	ld	[%o0], %o3		/* o3 = rwlock */
	/*
	 * get reader lock
	 */
	set	WORD_WLOCK, %o5
0:
	cmp	%o3, %o5
	be,a,pn	%icc, 3f		/* return failure */
	sub	%g0, 1, %o0

	mov	%o3, %o1		/* o1 = old value */
	inc	%o3			/* o3 = new value */

	cmp	%o3, %o5		/* # of readers == WLOCK */
	bne,pt	%icc, 1f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic4), %o0
	call	panic
	or	%o0, %lo(rw_panic4), %o0

1:
	cas	[%o0], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 3f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 0b		/* try again */
	ld	[%o0], %o3

	/*
	 * get writer lock
	 */
2:
	cmp	%o3, %g0
	bne,a,pn %icc, 3f		/* return failure */
	sub	%g0, 1, %o0

	mov	%o3, %o1		/* o1 = old value */
	set	WORD_WLOCK, %o3		/* o3 = new value */

	cas	[%o0], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 3f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 2b		/* try again */
	ld	[%o0], %o3
		
	/*
	 * return status
	 */
3:
	retl
	nop
	SET_SIZE(rwlock_word_enter)


	ENTRY(rwlock_word_exit)

	cmp	%o1, READER_LOCK
	bne	%icc, 4f
	ld	[%o0], %o3		/* o3 = rwlock */
	/*
	 * exit reader lock
	 */
1:
#ifdef	DEBUG
	set	WORD_WLOCK, %o5
	cmp	%o3, %o5
	bne,a,pt %icc, 2f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic1), %o0
	call	panic
	or	%o0, %lo(rw_panic1), %o0
2:
	cmp	%o3, %g0		/* reader lock is held */
	bg,a,pt %icc, 3f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic2), %o0
	call	panic
	or	%o0, %lo(rw_panic2), %o0
3:
#endif	DEBUG

	mov	%o3, %o1		/* o1 = old value */
	dec	%o3			/* o3 = new value */

	cas	[%o0], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 6f		/* return success */
	nop

	ba,pt	%icc, 1b		/* try again */
	ld	[%o0], %o3

	/*
	 * exit writer lock
	 */
4:
#ifdef	DEBUG
	set	WORD_WLOCK, %o5
	cmp	%o3, %o5
	be,a,pt %icc, 5f			/* return failure */
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic3), %o0
	call	panic
	or	%o0, %lo(rw_panic3), %o0
5:
#endif	DEBUG

	mov	%o3, %o1		/* o1 = old value */
	mov	%g0, %o3		/* o3 = new value */

	cas	[%o0], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 6f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 4b		/* try again */
	ld	[%o0], %o3
6:
	retl
	nop
	SET_SIZE(rwlock_word_exit)

	ENTRY(rwlock_hword_init)
	sth	%g0, [%o0]		/* set rwlock to 0 */
	retl
	nop
	SET_SIZE(rwlock_hword_init)

	ENTRY(rwlock_hword_enter)
	xor	%o0, 2, %o4
	cmp	%o1, READER_LOCK
	bne	%icc, 4f
	lduh	[%o0], %o2		/* o2 = rwlock */
	/*
	 * get reader lock
	 */
0:
	lduh	[%o4], %o3		/* o3 = other half */

	set	HWORD_WLOCK, %o5
	cmp	%o2, %o5
	be,a,pn	%icc, 7f		/* return failure */
	sub	%g0, 1, %o0

	dec	%o5
	cmp	%o2, %o5		/* # of readers == WLOCK-1 */
	bne,pt	%icc, 1f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic4), %o0
	call	panic
	or	%o0, %lo(rw_panic4), %o0
1:
	btst	2, %o0			/* which halfword */
	bz	%icc,2f
	mov	%o2, %o5
/*
 * rwlock is lower halfword
 */
	inc	%o5
	sll	%o3, 16, %o3
	or	%o2, %o3, %o1		/* o1 = old value */
	ba,pt	%icc, 3f
	or	%o5, %o3, %o3		/* o3 = new value */

/*
 * rwlock is upper halfword
 */
2:
	inc	%o5
	sll	%o2, 16, %o2
	or	%o2, %o3, %o1		/* o1 = old value */
	sll	%o5, 16, %o5
	or	%o5, %o3, %o3		/* o3 = new value */

/*
 * Now compare and swap
 */
3:
	cas	[%o4], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 7f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 0b		/* try again */
	lduh	[%o0], %o2

	/*
	 * get writer lock
	 */
4:
	lduh	[%o4], %o3		/* o3 = other half */
	cmp	%o2, %g0
	bne,a,pn %icc, 7f		/* return failure */
	sub	%g0, 1, %o0

	set	HWORD_WLOCK, %o5
	btst	2, %o0			/* which halfword */
	bz,a	%icc,5f
	sll	%o2, 16, %o2
/*
 * rwlock is lower halfword
 */
	sll	%o3, 16, %o3
	or	%o2, %o3, %o1		/* o1 = old value */
	ba,pt	%icc, 6f
	or	%o5, %o3, %o3		/* o3 = new value */

/*
 * rwlock is upper halfword
 */
5:
	or	%o2, %o3, %o1		/* o1 = old value */
	sll	%o5, 16, %o5
	or	%o5, %o3, %o3		/* o3 = new value */
/*
 * Now compare and swap
 */
6:
	cas	[%o4], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 7f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 4b		/* try again */
	lduh	[%o0], %o2
		
	/*
	 * return status
	 */
7:
	retl
	nop
	SET_SIZE(rwlock_hword_enter)


	ENTRY(rwlock_hword_exit)

	xor	%o0, 2, %o4
	cmp	%o1, READER_LOCK
	bne	%icc, 6f
	lduh	[%o0], %o2		/* o2 = rwlock */
	/*
	 * exit reader lock
	 */
1:
	lduh	[%o4], %o3		/* o3 = other half */

#ifdef	DEBUG
	set	HWORD_WLOCK, %o5	/* check for writer lock */
	cmp	%o2, %o5
	bne,a,pt %icc, 2f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic1), %o0
	call	panic
	or	%o0, %lo(rw_panic1), %o0
2:
	cmp	%o2, %g0		/* check for reader lock */
	bg,a,pt %icc, 3f
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic2), %o0
	call	panic
	or	%o0, %lo(rw_panic2), %o0
3:
#endif	DEBUG

	btst	2, %o0			/* which halfword */
	bz	%icc,4f
	mov	%o2, %o5
/*
 * rwlock is lower halfword
 */
	dec	%o5
	sll	%o3, 16, %o3
	or	%o2, %o3, %o1		/* o1 = old value */
	ba,pt	%icc, 5f
	or	%o5, %o3, %o3		/* o3 = new value */

/*
 * rwlock is upper halfword
 */
4:
	dec	%o5
	sll	%o2, 16, %o2
	or	%o2, %o3, %o1		/* o1 = old value */
	sll	%o5, 16, %o5
	or	%o5, %o3, %o3		/* o3 = new value */

/*
 * Now compare and swap
 */
5:
	cas	[%o4], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 0f		/* return success */
	nop

	ba,pt	%icc, 1b		/* try again */
	lduh	[%o0], %o2

	/*
	 * exit writer lock
	 */
6:
	lduh	[%o4], %o3		/* o3 = other half */
	andn	%o0, 3, %o5
	ld	[%o5], %o1		/* o1 = old value */
#ifdef	DEBUG
	set	HWORD_WLOCK, %o5
	cmp	%o2, %o5
	be,a,pt %icc, 7f			/* return failure */
	nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(rw_panic3), %o0
	call	panic
	or	%o0, %lo(rw_panic3), %o0
7:
#endif	DEBUG

	btst	2, %o0			/* which halfword */
	bz	%icc,8f
	mov	%g0, %o5
/*
 * rwlock is lower halfword
 */
	sll	%o3, 16, %o3
	or	%o2, %o3, %o1		/* o1 = old value */
	ba,pt	%icc, 9f
	or	%o5, %o3, %o3		/* o3 = new value */
/*
 * rwlock is upper halfword
 */
8:
	sll	%o2, 16, %o2
	or	%o2, %o3, %o1		/* o1 = old value */
/*
 * Now compare and swap
 */
9:
	cas	[%o4], %o1, %o3
	cmp	%o1, %o3
	be,a	%icc, 0f		/* return success */
	mov	0, %o0

	ba,pt	%icc, 6b		/* try again */
	lduh	[%o0], %o2
		
0:
	retl
	nop
	SET_SIZE(rwlock_hword_exit)

#endif /* lint */

#ifdef lint
/*
 * Primitives for atomic increments of a pointer to a circular array.
 * atomically adds 1 to the word pointed to by *curidx, unless it's reached
 * the maxidx, in which case it atomically returns *pidx to zero.
 * Rules for use: 1. maxidx must be > 0
 *		  2. *never* write *pidx after initialization
 *		  3. always use the returned or *newidx value as the
 *			index to your array
 *
 * 
 * void atinc_cidx_extword(longlong_t *curidx, longlong_t *newidx,
 *			longlong_t *maxidx)
 * int atinc_cidx_word(int *curidx, int maxidx)
 * short atinc_cidx_hword(short *curidx, int maxidx)
 * {
 *	curidx = *pidx;
 *      if (curidx < maxidx)
 *              *pidx = curidx++;
 *      else
 *              *pidx = curidx = 0;
 *	return(curidx);			! word, hword
 *	    OR	
 *	*newidx = curidx;		! extword
 * }
 */

/* ARGSUSED */
void
atinc_cidx_extword(longlong_t *curidx, longlong_t *newidx, longlong_t maxidx)
{}

/* ARGSUSED */
int
atinc_cidx_word(int *curidx, int maxidx)
{return 0;}

/* ARGSUSED */
short
atinc_cidx_hword(short *curidx, short maxidx)
{return 0;}

#else /* lint */

	ENTRY(atinc_cidx_extword)
1:
	ldx	[%o0], %o3			! load current idx to curidx
	subcc	%o2, %o3, %g0			! compare maxidx to curidx
	be,a,pn	%icc, 2f			! if maxidx == curidx
	clr	%o4				! then move 0 to newidx
	add	%o3, 1, %o4			! else newidx = curidx++
2:
	or	%o4, %g0, %o5			! save copy of newidx in %o5
	casx	[%o0], %o3, %o4	
	cmp	%o3, %o4			! if curidx != current idx
	bne,a,pn %icc, 1b			! someone else got there first
	nop 
	retl					! put saved copy of
	stx	%o5, [%o1] 			! newidx into [%o1]
	SET_SIZE(atinc_cidx_extword)

	ENTRY(atinc_cidx_word)
1:
	ld	[%o0], %o2			! load current idx to curidx
	subcc	%o1, %o2, %g0			! compare maxidx to curidx
	be,a,pn	%icc, 2f			! if maxidx == curidx
	clr	%o3				! then move 0 to newidx
	add	%o2, 1, %o3			! else newidx = curidx++
2:
	or	%o3, %g0, %o4			! save copy of newidx in %o4
	cas	[%o0], %o2, %o3	
	cmp	%o2, %o3			! if curidx != current idx
	bne,a,pn %icc, 1b			! someone else got there first
	nop 
	retl					! put saved copy of
	or	%o4, %g0, %o0 			! newidx into %o0
	SET_SIZE(atinc_cidx_word)

	ENTRY(atinc_cidx_hword)
	xor	%o0, 2, %o4
1:
	lduh	[%o0], %o2			! load current idx to curidx
	lduh	[%o4], %o3			! load the other half word
	subcc	%o1, %o2, %g0			! compare maxidx to curidx
	be,a,pn	%icc, 2f			! if maxidx == curidx
	clr	%o5				! then move 0 to newidx
	add	%o2, 1, %o5			! else newidx = curidx++
2:
	btst	2, %o0
	bnz,a,pn %icc, 3f			! counter is lower half
	sll	%o3, 16, %o3

	sll	%o5, 16, %o5			! counter is upper half
	sll	%o2, 16, %o2
3:
	or	%o5, %g0, %o4			! save copy of newidx in %o4
	or	%o5, %o3, %o5			! or newidx w/other half word
	or	%o2, %o3, %g2			! or curidx w/other half word
	cas	[%o0], %g2, %o5
	cmp	%g2, %o5			! if curidx != current idx
	bne,a,pn %icc, 1b			! someone else got there first
	nop 
	retl					! put saved copy of
	or	%o4, %g0, %o0			! newidx into %o0
	SET_SIZE(atinc_cidx_hword)

#endif /* lint */

/*
 * Fetch user extended word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_fuword64(const void *addr, uint64_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY(default_fuword64)

! XX64	do we need to preserve the 'stacking' behaviour of these
!	routines w.r.t. lofault?

        btst    0x7, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	ldxa	[%o0]ASI_USER, %o2	! get the extended word
	stn	%o3, [THREAD_REG + T_LOFAULT]
	clr	%o0
	retl
	stx	%o2, [%o1]		! and store it ..
.fsuerr:
	stn	%o3, [THREAD_REG + T_LOFAULT]
.fsubad:
	retl
	mov	-1, %o0			! return error
	SET_SIZE(default_fuword64)

#endif	/* lint */

/*
 * Fetch user word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_fuword32(const void *addr, uint32_t *dst)
{ return (0); }

/*ARGSUSED*/
int
default_fuiword32(const void *addr, uint32_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY2(default_fuiword32,default_fuword32)
        btst    0x3, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	lda	[%o0]ASI_USER, %o2	! get the word
	stn	%o3, [THREAD_REG + T_LOFAULT]
	mov	0, %o0
	retl
	st	%o2, [%o1]		! and store it ..
	SET_SIZE(default_fuiword32)
	SET_SIZE(default_fuword32)

#endif	/* lint */

/*
 * Fetch user byte.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_fuword8(const void *addr, uint8_t *dst)
{ return (0); }

/*ARGSUSED*/
int
default_fuiword8(const void *addr, uint8_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY2(default_fuiword8,default_fuword8)
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	lduba	[%o0]ASI_USER, %o2	! get the byte
	stn	%o3, [THREAD_REG + T_LOFAULT]
	clr	%o0
	retl
	stb	%o2, [%o1]		! and store it ..
	SET_SIZE(default_fuiword8)
	SET_SIZE(default_fuword8)

#endif	/* lint */

/*
 * Set user extended word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_suword64(void *addr, uint64_t value)
{ return (0); }

#else	/* lint */

	ENTRY(default_suword64)
        btst    0x7, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
#if !defined(__sparcv9)
	sllx	%o1, 32, %o1		! compiler assumes that the
	srl	%o2, 0, %o2		! upper 32 bits are in %o1,
	or	%o1, %o2, %o1		! lower 32 bits are in %o2
#endif
	stxa	%o1, [%o0]ASI_USER
.suret:	
	stn	%o3, [THREAD_REG + T_LOFAULT]
	retl
	clr	%o0
	SET_SIZE(default_suword64)

#endif	/* lint */

/*
 * Set user word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_suword32(void *addr, uint32_t value)
{ return (0); }

/*ARGSUSED*/
int
default_suiword(void *addr, uint32_t value)
{ return (0); }

#else	/* lint */

	ENTRY2(default_suiword32,default_suword32)
        btst    0x3, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	b	.suret
	sta	%o1, [%o0]ASI_USER
	SET_SIZE(default_suiword32)
	SET_SIZE(default_suword32)

#endif	/* lint */

/*
 * Set user byte.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_suword8(void *addr, uint8_t value)
{ return (0); }

/*ARGSUSED*/
int
default_suiword8(void *addr, uint8_t value)
{ return (0); }

#else	/* lint */

	ENTRY2(default_suiword8,default_suword8)
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	b	.suret
	stba	%o1, [%o0]ASI_USER
	SET_SIZE(default_suiword8)
	SET_SIZE(default_suword8)

#endif	/* lint */

/*
 * Fetch user short (half) word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_fuword16(const void *addr, uint16_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY(default_fuword16)
        btst    0x1, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	lduha	[%o0]ASI_USER, %o0
	b	.suret
	sth	%o0, [%o1]
	SET_SIZE(default_fuword16)

#endif	/* lint */

/*
 * Set user short word.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_suword16(void *addr, uint16_t value)
{ return (0); }

#else	/* lint */

	ENTRY(default_suword16)
        btst    0x1, %o0		! test alignment
        bne     .fsubad
        .empty
	set	.fsuerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o3
	stn	%o5, [THREAD_REG + T_LOFAULT]
	b	.suret
	stha	%o1, [%o0]ASI_USER
	SET_SIZE(default_suword16)

#endif	/* lint */
	
#if defined(lint)

/*ARGSUSED*/
void
fuword8_noerr(const void *addr, uint8_t *dst)
{}

/*ARGSUSED*/
void
fuword16_noerr(const void *addr, uint16_t *dst)
{}

/*ARGSUSED*/
void
fuword32_noerr(const void *addr, uint32_t *dst)
{}

/*ARGSUSED*/
void
fulword_noerr(const void *addr, u_long *dst)
{}

/*ARGSUSED*/
void
fuword64_noerr(const void *addr, uint64_t *dst)
{}

#else	/* lint */

	ENTRY(fuword8_noerr)
	lduba	[%o0]ASI_USER, %o0	
	retl
	stb	%o0, [%o1]
	SET_SIZE(fuword8_noerr)

	ENTRY(fuword16_noerr)
	lduha	[%o0]ASI_USER, %o0
	retl
	sth	%o0, [%o1]
	SET_SIZE(fuword16_noerr)

	ENTRY(fuword32_noerr)
	lda	[%o0]ASI_USER, %o0
	retl
	st	%o0, [%o1]
	SET_SIZE(fuword32_noerr)

	ENTRY(fulword_noerr)
	ldna	[%o0]ASI_USER, %o0
	retl
	stn	%o0, [%o1]
	SET_SIZE(fulword_noerr)

	ENTRY(fuword64_noerr)
	ldxa	[%o0]ASI_USER, %o0
	retl
	stx	%o0, [%o1]
	SET_SIZE(fuword64_noerr)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
suword8_noerr(void *addr, uint8_t value)
{}

/*ARGSUSED*/
void
subyte_noerr(void *addr, u_char value)
{}

/*ARGSUSED*/
void
suword16_noerr(void *addr, uint16_t value)
{}

/*ARGSUSED*/
void
suword32_noerr(void *addr, uint32_t value)
{}

/*ARGSUSED*/
void
sulword_noerr(void *addr, u_long value)
{}

/*ARGSUSED*/
void
suword64_noerr(void *addr, uint64_t value)
{}

#else	/* lint */

	ENTRY2(suword8_noerr,subyte_noerr)
	retl
	stba	%o1, [%o0]ASI_USER
	SET_SIZE(suword8_noerr)
	SET_SIZE(subyte_noerr)

	ENTRY(suword16_noerr)
	retl
	stha	%o1, [%o0]ASI_USER
	SET_SIZE(suword16_noerr)

	ENTRY(suword32_noerr)
	retl
	sta	%o1, [%o0]ASI_USER
	SET_SIZE(suword32_noerr)

	ENTRY(sulword_noerr)
	retl
	stna	%o1, [%o0]ASI_USER
	SET_SIZE(sulword_noerr)

	ENTRY(suword64_noerr)
#if !defined(__sparcv9)
	sllx	%o1, 32, %o1
	srl	%o2, 0, %o2	!  upper bits are in %o1,
	or	%o1, %o2, %o1	!  lower bits are in %o2
#endif
	retl
	stxa	%o1, [%o0]ASI_USER
	SET_SIZE(suword64_noerr)

#endif	/* lint */


/*
 * Set tba to given address, no side effects.
 * This entry point is used by callback handlers.
 */
#if defined (lint)

/*ARGSUSED*/
void *
set_tba(void *new_tba)
{ return (0); }

#else	/* lint */

	ENTRY(set_tba)
	mov	%o0, %o1
	rdpr	%tba, %o0
	wrpr	%o1, %tba
	retl
	nop
	SET_SIZE(set_tba)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
setpstate(u_int pstate)
{}

#else	/* lint */

	ENTRY_NP(setpstate)
	retl
	wrpr	%g0, %o0, %pstate
	SET_SIZE(setpstate)

#endif	/* lint */

#if defined(lint) || defined(__lint)

u_int
getpstate(void)
{ return(0); }

#else	/* lint */

	ENTRY_NP(getpstate)
	retl
	rdpr	%pstate, %o0
	SET_SIZE(getpstate)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
setwstate(u_int wstate)
{}

#else	/* lint */

	ENTRY_NP(setwstate)
	retl
	wrpr	%g0, %o0, %wstate
	SET_SIZE(setwstate)

#endif	/* lint */


#if defined(lint) || defined(__lint)

u_int
getwstate(void)
{ return(0); }

#else	/* lint */

	ENTRY_NP(getwstate)
	retl
	rdpr	%wstate, %o0
	SET_SIZE(getwstate)

#endif	/* lint */


/*
 * int panic_trigger(int *tp)
 *
 * A panic trigger is a word which is updated atomically and can only be set
 * once.  We atomically store 0xFF into the high byte and load the old value.
 * If the byte was 0xFF, the trigger has already been activated and we fail.
 * If the previous value was 0 or not 0xFF, we succeed.  This allows a
 * partially corrupt trigger to still trigger correctly.
 */
#if defined(lint)

/*ARGSUSED*/
int panic_trigger(int *tp) { return (0); }

#else	/* lint */

	ENTRY_NP(panic_trigger)
	ldstub	[%o0], %o0		! store 0xFF, load byte into %o0
	cmp	%o0, 0xFF		! compare %o0 to 0xFF
	set	1, %o1			! %o1 = 1
	be,a	0f			! if (%o0 == 0xFF) goto 0f (else annul)
	set	0, %o1			! delay - %o1 = 0
0:	retl
	mov	%o1, %o0		! return (%o1);
	SET_SIZE(panic_trigger)

#endif	/* lint */

/*
 * void vpanic(const char *format, va_list alist)
 *
 * The panic() and cmn_err() functions invoke vpanic() as a common entry point
 * into the panic code implemented in panicsys().  vpanic() is responsible
 * for passing through the format string and arguments, and constructing a
 * regs structure on the stack into which it saves the current register
 * values.  If we are not dying due to a fatal trap, these registers will
 * then be preserved in panicbuf as the current processor state.  Before
 * invoking panicsys(), vpanic() activates the first panic trigger (see
 * common/os/panic.c) and switches to the panic_stack if successful.
 */
#if defined(lint)

/*ARGSUSED*/
void vpanic(const char *format, va_list alist) {}

#else	/* lint */

	ENTRY_NP(vpanic)

	save	%sp, -SA(MINFRAME + REGSIZE), %sp	! save and allocate regs

	!
	! The v9 struct regs has a 64-bit r_tstate field, which we use here
	! to store the %ccr, %asi, %pstate, and %cwp as they would appear
	! in %tstate if a trap occurred.  We leave it up to the debugger to
	! realize what happened and extract the register values.
	!
	rd	%ccr, %l0				! %l0 = %ccr
	sllx	%l0, TSTATE_CCR_SHIFT, %l0		! %l0 <<= CCR_SHIFT
	rd	%asi, %l1				! %l1 = %asi
	sllx	%l1, TSTATE_ASI_SHIFT, %l1		! %l1 <<= ASI_SHIFT
	or	%l0, %l1, %l0				! %l0 |= %l1
	rdpr	%pstate, %l1				! %l1 = %pstate
	sllx	%l1, TSTATE_PSTATE_SHIFT, %l1		! %l1 <<= PSTATE_SHIFT
	or	%l0, %l1, %l0				! %l0 |= %l1
	rdpr	%cwp, %l1				! %l1 = %cwp
	sllx	%l1, TSTATE_CWP_SHIFT, %l1		! %l1 <<= CWP_SHIFT
	or	%l0, %l1, %l0				! %l0 |= %l1

	set	vpanic, %l1				! %l1 = %pc (vpanic)
	add	%l1, 4, %l2				! %l2 = %npc (vpanic+4)
	rd	%y, %l3					! %l3 = %y
	
	sethi	%hi(panic_quiesce), %o0
	call	panic_trigger
	or	%o0, %lo(panic_quiesce), %o0		! if (!panic_trigger(
	tst	%o0					!     &panic_quiesce))
	be	0f					!   goto 0f;
	mov	%o0, %l4				!   delay - %l4 = %o0

	!
	! If panic_trigger() was successful, we are the first to initiate a
	! panic: flush our register windows and switch to the panic_stack.
	!
	call	flush_windows
	nop

	set	panic_stack, %o0			! %o0 = panic_stack
	set	PANICSTKSIZE, %o1			! %o1 = size of stack
	add	%o0, %o1, %o0				! %o0 = top of stack

	sub	%o0, SA(MINFRAME + REGSIZE) + STACK_BIAS, %sp

	!
	! Now that we've got everything set up, store each register to its
	! designated location in the regs structure allocated on the stack.
	! The register set we store is the equivalent of the registers at
	! the time the %pc was pointing to vpanic, thus the %i's now contain
	! what the %o's contained prior to the save instruction.
	!
0:	stx	%l0, [%sp + STACK_BIAS + SA(MINFRAME) + TSTATE_OFF]
	stx	%g1, [%sp + STACK_BIAS + SA(MINFRAME) + G1_OFF]
	stx	%g2, [%sp + STACK_BIAS + SA(MINFRAME) + G2_OFF]
	stx	%g3, [%sp + STACK_BIAS + SA(MINFRAME) + G3_OFF]
	stx	%g4, [%sp + STACK_BIAS + SA(MINFRAME) + G4_OFF]
	stx	%g5, [%sp + STACK_BIAS + SA(MINFRAME) + G5_OFF]
	stx	%g6, [%sp + STACK_BIAS + SA(MINFRAME) + G6_OFF]
	stx	%g7, [%sp + STACK_BIAS + SA(MINFRAME) + G7_OFF]
	stx	%i0, [%sp + STACK_BIAS + SA(MINFRAME) + O0_OFF]
	stx	%i1, [%sp + STACK_BIAS + SA(MINFRAME) + O1_OFF]
	stx	%i2, [%sp + STACK_BIAS + SA(MINFRAME) + O2_OFF]
	stx	%i3, [%sp + STACK_BIAS + SA(MINFRAME) + O3_OFF]
	stx	%i4, [%sp + STACK_BIAS + SA(MINFRAME) + O4_OFF]
	stx	%i5, [%sp + STACK_BIAS + SA(MINFRAME) + O5_OFF]
	stx	%i6, [%sp + STACK_BIAS + SA(MINFRAME) + O6_OFF]
	stx	%i7, [%sp + STACK_BIAS + SA(MINFRAME) + O7_OFF]
	stn	%l1, [%sp + STACK_BIAS + SA(MINFRAME) + PC_OFF]
	stn	%l2, [%sp + STACK_BIAS + SA(MINFRAME) + NPC_OFF]
	st	%l3, [%sp + STACK_BIAS + SA(MINFRAME) + Y_OFF]

	mov	%l4, %o3				! %o3 = on_panic_stack
	add	%sp, STACK_BIAS + SA(MINFRAME), %o2	! %o2 = &regs
	mov	%i1, %o1				! %o1 = alist
	call	panicsys				! panicsys();
	mov	%i0, %o0				! %o0 = format
	ret
	restore

	SET_SIZE(vpanic)

#endif	/* lint */
