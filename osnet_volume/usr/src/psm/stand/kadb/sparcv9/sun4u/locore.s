/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s	1.19	99/05/25 SMI"

/*
 * Assembly language support for Fusion kadb64
 * This is very similar to the kadb for sun4u. Instead of using #ifdef
 * all over the place, we created a seperate file for kadb64.
 *
 * Fusion differs from other SPARC machines in that the MMU trap handling
 * code is done in software. For this and other reasons, it is simpler to
 * use the PROM's trap table while running in the debugger than to have
 * kadb manage its own trap table. Breakpoint/L1-A traps work by downloading
 * a FORTH 'defer word' to the PROM, which gives it an address to jump to
 * when it gets a trap it doesn't recognize, e.g. a kadb-induced breakpoint
 * (N.B. PROM breakpoints are independent of this scheme and work as before).
 * The PROM saves some of the register state at the time of the breakpoint
 * trap and jumps to 'start' in kadb, where we save off everything else and
 * proceed as usual.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/modctl.h>
#include <sys/scb.h>
#include <sys/debug/debugger.h>
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/stack.h>
#include <sys/privregs.h>
#include <sys/machparam.h>
#include <sys/spitregs.h>
#include <sys/debug/debug.h>

/*
 * The debug stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important. We get a red zone below this stack
 * for free when the text is write protected.
 */
#define	STACK_SIZE	0x8000

#if !defined(lint)

	.seg	".data"
	.align	16
	.global estack

	.skip	STACK_SIZE
estack:				! end (top) of debugger stack

	.align	16
	.global nwindows
	.global p1275_cif	! 1275 client interface handler

nwindows:
	.word   8
p1275_cif:
	.word	0

	.seg	".bss"
	.align	16
	.global	bss_start
bss_start:

	.seg	".text"


/*
 * The debugger vector table.
 */
	.global	dvec
dvec:
	b,a	.enterkadb	! dv_enter
	.word	trap		! dv_trap
	.word	pagesused	! dv_pages
	.word	scbsync		! dv_scbsync
	.word	DEBUGVEC_VERSION_2	! dv_version
 	.word	callb_format_wrapper	! dv_format
 	.word	callb_arm_wrapper	! dv_arm
 	.word	callb_cpu_change_wrapper	! dv_cpu_change
	.word	callb_set_polled_callbacks_wrapper
	.word	0


	.align	8
	.global	dvec64
dvec64:
	b,a	.enterkadb
	nop
	.xword	trap
	.xword	pagesused
	.xword	scbsync		! was scbsync64
	.word	DEBUGVEC_VERSION_2, 0
	.xword	callb_format 
	.xword 	callb_arm
	.xword	callb_cpu_change
	.xword  callb_set_polled_callbacks 
	.xword	0
	

#endif	/* !defined(lint) */

/*
 * Debugger entry point.
 *
 * Inputs:
 *	%o0 - p1275cif pointer
 *	%o1 - dvec (null)
 *	%o2 - bootops vector
 *	%o3 - ELF boot vector
 *
 * Our tasks are:
 *
 *	save parameters
 *	initialize CPU state registers
 *	clear kadblock
 *	dive into main()
 */
#ifdef	lint
void
start() {}
#else

	.align	16
	.global	start		! start address of kadb text (sorta)

	ENTRY(start)

	! Save the 1275 cif handler and ELF boot vector for later use.

	set	p1275_cif, %g1
	st	%o0, [%g1]
	set	elfbootvec, %g1
	stx	%o3, [%g1]

	! Miscellaneous initialization.

	!wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV+PSTATE_IE, %pstate
	wrpr	%g0, PSTATE_PEF+PSTATE_PRIV+PSTATE_IE, %pstate

	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1
	inc	%g1
	set	nwindows, %g2
	st	%g1, [%g2]

	! early_startup(p1275cif, dvec, bootops, elfbootvec)

	call	early_startup		! non-machdep startup code
	nop
	call	startup			! do the rest of the startup work.
	nop
	call	main			! enter the debugger.
	nop

	t	ST_KADB_TRAP

	! In the unlikely event we get here, return to the monitor.

	call	prom_exit_to_mon
	clr	%o0

	SET_SIZE(start)
#endif	/* lint */

/*
 * Enter the debugger. This goes out to the kernel (who owns the
 * trap table) and down into the PROM (because we patched the
 * kernel's ST_KADB_TRAP vector with the PROM's).
 */
#ifndef lint

	ENTRY(.enterkadb)
	t	ST_KADB_TRAP
	nop
	retl
	nop
	SET_SIZE(.enterkadb)

#endif	/* lint */

#ifdef lint
/*ARGSUSED*/
int
kadb_1275_wrapper(void *p)
{
	return (0);
}

#else
	ENTRY(kadb_1275_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
        setn    kadb_1275_call, %g1, %o1       	! Ctall boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     kadb_1275_call, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(kadb_1275_wrapper)
#endif

#ifdef lint
/*ARGSUSED*/
void
kobj_notify_kadb_wrapper(unsigned int u, struct modctl *modp) {}
#else
	
	ENTRY(kobj_notify_kadb_wrapper)

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
	srl	%i1, 0, %o1

	rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    kobj_notify_kadb_deferred_bkpt, %g1, %o2
        jmpl    %o2, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0

	SET_SIZE(kobj_notify_kadb_wrapper)
#endif
	
#ifdef lint
/*ARGSUSED*/
void
callb_format_wrapper(void *arg) {}

#else
	ENTRY(callb_format_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0

	rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_format, %g1, %o1       	! Call boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_format, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(callb_format_wrapper)
#endif

#ifdef lint
/*ARGSUSED*/
void
callb_arm_wrapper(void) {}

#else
	ENTRY(callb_arm_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_arm, %g1, %o1       	! Call boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_arm, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(callb_arm_wrapper)
#endif

#ifdef lint
/*ARGSUSED*/
void
callb_cpu_change_wrapper(int cpuid, kadb_cpu_attr_t what, int arg) {}

#else
	ENTRY(callb_cpu_change_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
	srl	%i1, 0, %o1
	srl	%i2, 0, %o2
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_cpu_change, %g1, %o4     	! Call boot service with ...
        jmpl    %o4, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_cpu_change, %o4
        jmp     %o4
        nop
        /* NOTREACHED */

	SET_SIZE(callb_cpu_change_wrapper)
#endif

#ifdef lint
/*ARGSUSED*/
void
callb_set_polled_callbacks_wrapper(cons_polledio_t *arg) {}

#else

	ENTRY(callb_set_polled_callbacks_wrapper)

	andcc    %sp, 0x1, %g0
        bnz     6f
        nop

	
	save    %sp, -SA(MINFRAME), %sp		! Create a new stack frame
        srl     %i0, 0, %o0			! Move parameter into %o0

	rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2		! Change state to 64 bit 
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero

	rdpr    %wstate, %l5			! Get the window state reg
        andn    %l5, WSTATE_MASK, %l6
        wrpr    %l6, WSTATE_KMIX, %wstate	! Set trap handler to mixed 32/64
						! bit stack

	setn    callb_set_polled_callbacks, %g1, %o1      
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
						
						! Change state back to 32 bit 
        wrpr    %l1, 0, %pstate                 ! by setting PSTATE.AM to one.
	
        wrpr    %l5, 0, %wstate			! Restore the window state reg

        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0

6:	/*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_set_polled_callbacks, %o1
        jmp     %o1
        nop
        /* NOTREACHED */
 
        SET_SIZE(callb_set_polled_callbacks_wrapper)



#endif

/*
 * Pass control to the client program.
 */
#ifdef	lint
/*ARGSUSED*/
void
_exitto(int (*addr)())
{}
#else

	ENTRY(_exitto)
	sub	%g0, SA(MINFRAME) - STACK_BIAS, %g1
	save	%sp, %g1, %sp
	set	bootops, %o0		! pass bootops to callee
	ldx	[%o0], %o2
	set	dvec, %o1		! pass dvec address to callee
	set	elfbootvec, %o3		! pass elf bootstrap vector
	ldx	[%o3], %o3
	set	p1275_cif, %o0		! pass CIF to callee
	ld	[%o0], %o0
	rdpr	%pstate, %g1		! This is a 32 bit client
	or	%g1, PSTATE_AM, %g1	! Set address mask
	wrpr	%g0, %g1, %pstate
	jmpl	%i0, %o7		! register-indirect call
	mov	%o0, %o4		! 1210378: Pass cif in %o0 and %o4
	ret
	restore
	SET_SIZE(_exitto)

#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
_exitto64(int (*addr)())
{}
#else

	ENTRY(_exitto64)
	save	%sp, -SA(MINFRAME64), %sp
	set	bootops, %o0		! pass bootops to callee
	ldx	[%o0], %o2
	set	dvec64, %o1		! pass dvec address to callee
	set	elfbootvecELF64, %o3		! pass elf bootstrap vector
	ldx	[%o3], %o3
	set	p1275_cif, %o0		! pass CIF to callee
	ld	[%o0], %o0
	jmpl	%i0, %o7		! register-indirect call
	mov	%o0, %o4		! 1210378: Pass cif in %o0 and %o4
	ret
	restore
	SET_SIZE(_exitto64)

#endif	/* lint */

#ifdef lint
u_int
turn_off_interrupts(void)
{ return(0); }
#else
	ENTRY(turn_off_interrupts)
	rdpr	%pstate, %o0
	andn	%o0, PSTATE_IE, %g1
	retl
	wrpr	%g0, %g1, %pstate
	SET_SIZE(turn_off_interrupts)
#endif /* lint */

#ifdef lint
/*ARGSUSED*/
void
turn_on_interrupts(u_int pstate_save)	
{}
#else
	ENTRY(turn_on_interrupts)
	retl
	wrpr	%g0, %o0, %pstate
	SET_SIZE(turn_on_interrupts)
#endif


/*
 * Perform a trap on the behalf of our (C) caller.
 */
#ifdef	lint
/*ARGSUSED*/
void
asm_trap(int x) {}
#else

	ENTRY(asm_trap)
	retl
	t	%o0
	SET_SIZE(asm_trap)

#endif	/* lint */

/*
 * Flush all active (i.e. restoreable) windows to memory.
 */
#if defined(lint)
void
flush_windows(void) {}
#else	/* lint */

	ENTRY_NP(flush_windows)
	retl
	flushw
	SET_SIZE(flush_windows)

#endif	/* lint */

/*
 * The interface for a 32-bit client program calling the 64-bit 1275 OBP.
 */
#if defined(lint)
/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array) {return (0);}
#else	/* lint */

	ENTRY(client_handler)
	save	%sp, -SA64(MINFRAME64), %sp	! 32 bit frame, 64 bit sized
	mov	%i1, %o0
1:
	rdpr	%pstate, %l4			! Get the present pstate value
	andn	%l4, PSTATE_AM, %l6
	wrpr	%l6, 0, %pstate			! Set PSTATE_AM = 0
	jmpl	%i0, %o7			! Call cif handler
	nop
	wrpr	%l4, 0, %pstate			! Just restore 
	ret					! Return result ...
	restore	%o0, %g0, %o0			! delay; result in %o0
	SET_SIZE(client_handler)

#endif	/* lint */

/*
 * Misc subroutines.
 */

#ifdef	lint
struct scb *
gettba() {return (0);}
#else

	ENTRY(gettba)
	rdpr	%tba, %o0
	srl	%o0, 14, %o0
	retl
	sll	%o0, 14, %o0
	SET_SIZE(gettba)

#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
settba(struct scb *t) {}
#else

	ENTRY(settba)
	srl	%o0, 14, %o0
	sll	%o0, 14, %o0
	retl
	wrpr	%o0, %tba
	SET_SIZE(settba)

#endif	/* lint */

#ifdef	lint
char *
getsp(void) {return ((char *)0);}
#else

	ENTRY(getsp)
	retl
	add	%sp, STACK_BIAS, %o0
	SET_SIZE(getsp)

#endif	/* lint */

/*
 * Flush page containing <addr> from the spitfire I-cache.
 */
#ifdef	lint
/*ARGSUSED*/
void
sf_iflush(int *addr) {}
#else

	ENTRY(sf_iflush)
	set	MMU_PAGESIZE, %o1
	srlx	%o0, MMU_PAGESHIFT, %o0		! Go to begining of page
	sllx	%o0, MMU_PAGESHIFT, %o0
	membar	#StoreStore			! make stores globally visible
1:
	flush	%o0
	sub	%o1, ICACHE_FLUSHSZ, %o1	! bytes = bytes-8 
	brnz,a,pt %o1, 1b
	add	%o0, ICACHE_FLUSHSZ, %o0	! vaddr = vaddr+8
	retl
	nop
	SET_SIZE(sf_iflush)

#endif


/*
 * _setjmp(buf_ptr)
 * buf_ptr points to a five word array (jmp_buf). In the first is our
 * return address, the second, is the callers SP.
 * The rest is cleared by _setjmp
 *
 *		+----------------+
 *   %i0->	|      pc        |
 *		+----------------+
 *		|      sp        |
 *		+----------------+
 *		|    sigmask     |
 *		+----------------+
 *		|   stagstack    |
 *		|   structure    |
 *		+----------------+
 */

#if defined(lint)

/* ARGSUSED */
int
_setjmp(jmp_buf_ptr buf_ptr)
{ return (0); }

#else	/* lint */


	PCVAL	=	0	! offsets in buf structure
	SPVAL	=	8
	SIGMASK	=	16
	SIGSTACK =	24	

	SS_SP	   =	0	! offset in sigstack structure
	SS_ONSTACK =	8

	ENTRY(_setjmp)
	stx	%o7, [%o0 + PCVAL] 	! return pc
	stx	%sp, [%o0 + SPVAL] 	! save caller's sp
	clr	[%o0 + SIGMASK]		! clear the remainder of the jmp_buf
	clr	[%o0 + SIGSTACK + SS_SP]
	clr	[%o0 + SIGSTACK + SS_ONSTACK]
	retl
	clr	%o0
	SET_SIZE(_setjmp)

#endif	/* lint */

/*
 * _longjmp(buf_ptr, val)
 *
 * buf_ptr points to an array which has been initialized by _setjmp.
 * val is the value we wish to return to _setjmp's caller
 */

#if defined(lint)

/* ARGSUSED */
void
_longjmp(jmp_buf_ptr buf_ptr, int val)
{
	return;
}

#else	/* lint */

	ENTRY(_longjmp)

	save	%sp, -SA64(MINFRAME64), %sp
	flushw

	ldx	[%i0 + SPVAL], %fp		! build new stack frame
	ldx	[%i0 + PCVAL], %i7		! get new return pc

	movrz	%i1, 1, %i1			! mustn't return 0

	ret
	restore	%i1, 0, %o0			! return (val)

	SET_SIZE(_longjmp)

#endif	/* lint */
