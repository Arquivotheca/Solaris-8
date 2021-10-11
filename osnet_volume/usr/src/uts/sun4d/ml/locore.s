/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s	1.331	99/10/22 SMI"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/privregs.h>
#endif	/* lint */

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/clock.h>
#include <sys/debug/debug.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/psr.h>
#include <sys/pte.h>
#include <sys/machpcb.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/scb.h>
#include <sys/machthread.h>
#include <sys/machlock.h>
#include <sys/physaddr.h>
#include <sys/devaddr.h>
#include <sys/msacct.h>
#include <sys/mutex_impl.h>
#include <sys/panic.h>
#include <sys/mon_clock.h>
#include <sys/cyclic.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

/*
 * BW Interrupt Table - Misc Interrupt assignment (bit masks)
 */
#define	ASI_BW		0x2f
#define	BW_BASE		0xe0000000	/* CSR */
#define	BW_LOCALBASE	0xfff00000
#define	BWB_LOCALBASE	(BW_LOCALBASE + (1 << 8))

#define INTR_TABLE	0x1040
#define	INTR_TBL_CLEAR	0x1080

#if defined(lint) || defined(__lint)

#include <sys/thread.h>
#include <sys/time.h>

char DVMA[1];

#else	/* lint */

#include "assym.h"

	.section	".data"

/*
 * Sanity checking -
 *	MINFRAME and register offsets are double-word aligned
 *	MPCB_WBUF is double-word aligned
 *	MSGBUFSIZE is two pages?
 */

#if	(MINFRAME & 7) == 0 || (G2 & 1) == 0 || (O0 & 1) == 0
ERROR - struct regs not aligned
#endif
#if	(MPCB_WBUF & 7)
ERROR - pcb_wbuf not aligned
#endif

/*
 * Absolute external symbols.
 *
 * "panicbuf" occupies the third and fourth pages.
 * The first 2 pages of KERNELBASE are illegal to act as a redzone
 * during copyin/copyout type operations.
 * The panic buffer is allocated in low memory to prevent being
 * overwritten during booting operations.
 * (it is small enough to share pages with others)
 */

	.global DVMA, panicbuf

DVMA	= DVMABASE			! address of DVMA area
panicbuf = SYSBASE + PAGESIZE		! address of panic buffer

	.type	panicbuf, #object
	.size	panicbuf, PANICBUFSIZE

/*
 * The thread 0 stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important if the stack overflows. We get a
 * red zone below this stack for free when the kernel text is
 * write protected.
 */
	.global	t0stack
	.align	8
	.type	t0stack, #object
t0stack:
	.skip	T0STKSZ			! thread 0 stack
	.size	t0stack, T0STKSZ

	.global	t0
	.align	PTR24_ALIGN		! thread must be aligned for mutex impl.
	.type	t0, #object
t0:
	.skip	THREAD_SIZE		! thread 0
	.size	t0, THREAD_SIZE
	.align	8

#ifdef DEBUG
/*
 * make these symbols that start with dot be global for debugging so
 * they'll get into the symbol table (so we can see them in sas)
 */
	.global	.clean_windows, .cw_out, .doast, .fix_alignment, .getcc
	.global	.interrupt_ret, .intr_ret, .setcc
	.global	.sr_align_trap, .sr_assume_stack_res, .sr_out, .sr_samecwp
	.global	.sr_stack_not_res, .sr_sup, .sr_user, .sr_user_regs
	.global	.st_assume_stack_res, .st_cleanglo, .st_cpusave
	.global	.st_flyingleap, .st_have_window, .st_stack_not_res
	.global	.st_stack_res, .st_sys_ovf
	.global	.st_user, .st_user_lastwin, .st_user_ovf

/*
 * Throw in symbols for traps to make debugging easier with sas
 * (if you want to catch illegal instructions just set a breakpoint
 * at "illegal_instruction").  See table 7-1 in SPARC v8 doc.
 * This list is in priority order.
 */

#define	trap_sym(name, tt)	name = scb+(tt*16)

trap_sym(data_store_error,		0x2B)
trap_sym(instruction_access_MMU_miss,	0x3C)
trap_sym(instruction_access_error,	0x21)
trap_sym(r_register_access_error,	0x20)
trap_sym(instruction_access_exception,	0x01)
trap_sym(privileged_instruction,	0x03)
trap_sym(illegal_instruction,		0x02)
trap_sym(fp_disabled,			0x04)
trap_sym(cp_disabled,			0x24)
trap_sym(unimplemented_FLUSH,		0x25)
trap_sym(watchpoint_detected,		0x0B)
trap_sym(window_overflow,		0x05)
trap_sym(window_underflow,		0x06)
trap_sym(mem_address_not_aligned,	0x07)
trap_sym(fp_exception,			0x08)
trap_sym(cp_exception,			0x28)
trap_sym(data_access_error,		0x29)
trap_sym(data_access_MMU_miss,		0x2C)
trap_sym(data_access_exception,		0x09)
trap_sym(tag_overflow,			0x0A)
trap_sym(division_by_zero,		0x2A)
trap_sym(trap_instruction,		0x80)
trap_sym(interrupt_level_15,		0x1F)
trap_sym(interrupt_level_14,		0x1E)
trap_sym(interrupt_level_13,		0x1D)
trap_sym(interrupt_level_12,		0x1C)
trap_sym(interrupt_level_11,		0x1B)
trap_sym(interrupt_level_10,		0x1A)
trap_sym(interrupt_level_9,		0x19)
trap_sym(interrupt_level_8,		0x18)
trap_sym(interrupt_level_7,		0x17)
trap_sym(interrupt_level_6,		0x16)
trap_sym(interrupt_level_5,		0x15)
trap_sym(interrupt_level_4,		0x14)
trap_sym(interrupt_level_3,		0x13)
trap_sym(interrupt_level_2,		0x12)
trap_sym(interrupt_level_1,		0x11)
trap_sym(impl_dependent_exception,	0x60)

#endif

/*
 * In the event of a reproducible wedge, lighting the LED banks on
 * the back of the 4d's can be useful.  LIGHT_LED lights up a CPU
 * LED with the specified pattern (a char);  DUMP_WORD_TO_[LO|HI]LED
 * dumps a 32-bit word across four banks of LEDs.
 */

#define	XOFF_SLOWBUS_LED	0x2e

#define LIGHT_LED(cpu,pat) \
	not	pat; \
	sll	cpu, 11, cpu;	\
	add	cpu, XOFF_SLOWBUS_LED, cpu; \
	sll	cpu, 16, cpu; \
	stba	pat,[cpu]ASI_ECSR; \
	not	pat;

#define DUMP_WORD_TO_HILED(reg,cpu,pat) \
	mov	14,cpu; \
	mov	reg,pat; \
	LIGHT_LED(cpu,pat); \
	mov	12, cpu; \
	srl pat, 8, pat; \
	LIGHT_LED(cpu,pat); \
	mov	10, cpu; \
	srl pat, 8, pat; \
	LIGHT_LED(cpu,pat); \
	mov	8, cpu; \
	srl pat, 8, pat; \
	LIGHT_LED(cpu,pat);

#define DUMP_WORD_TO_LOLED(reg,cpu,pat) \
	mov	reg,pat; 	\
	mov	6,cpu; 		\
	LIGHT_LED(cpu,pat); 	\
	mov	4, cpu; 	\
	srl pat, 8, pat; 	\
	LIGHT_LED(cpu,pat); 	\
	mov	2, cpu; 	\
	srl pat, 8, pat; 	\
	LIGHT_LED(cpu,pat); 	\
	mov	0, cpu; 	\
	srl pat, 8, pat; 	\
	LIGHT_LED(cpu,pat);

/*
 * Trap tracing. If TRAPTRACE is defined, every trap records info
 * in a ciruclar buffer. Define TRAPTRACE in Makefile.$ARCH
 */
#ifdef	TRAPTRACE
#define	TRAP_TSIZE	(TRAP_ENT_SIZE*256)

	.global trap_trace_ctl
	.global trap_tr0, trap_tr1, trap_tr2, trap_tr3, trap_tr4
	.global trap_tr5, trap_tr6, trap_tr7, trap_tr8, trap_tr9
	.global trap_tr10, trap_tr11, trap_tr12, trap_tr13, trap_tr14
	.global trap_tr15, trap_tr16, trap_tr17, trap_tr18, trap_tr19
	.global trap_tr_panic

_trap_last_intr:
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0

_trap_last_intr2:
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0
	.word	0, 0, 0, 0, 0

	.align	16
trap_trace_ctl:
	.word	trap_tr0			! next CPU 0
	.word	trap_tr0			! first
	.word	trap_tr0 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr1			! next CPU 1
	.word	trap_tr1			! first
	.word	trap_tr1 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr2			! next CPU 2
	.word	trap_tr2			! first
	.word	trap_tr2 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr3			! next CPU 3
	.word	trap_tr3			! first
	.word	trap_tr3 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr4			! next CPU 4
	.word	trap_tr4			! first
	.word	trap_tr4 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr5			! next CPU 5
	.word	trap_tr5			! first
	.word	trap_tr5 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr6			! next CPU 6
	.word	trap_tr6			! first
	.word	trap_tr6 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr7			! next CPU 7
	.word	trap_tr7			! first
	.word	trap_tr7 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr8			! next CPU 8
	.word	trap_tr8			! first
	.word	trap_tr8 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr9			! next CPU 9
	.word	trap_tr9			! first
	.word	trap_tr9 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr10			! next CPU 10
	.word	trap_tr10			! first
	.word	trap_tr10 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr11			! next CPU 11
	.word	trap_tr11			! first
	.word	trap_tr11 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr12			! next CPU 12
	.word	trap_tr12			! first
	.word	trap_tr12 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr13			! next CPU 13
	.word	trap_tr13			! first
	.word	trap_tr13 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr14			! next CPU 14
	.word	trap_tr14			! first
	.word	trap_tr14 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr15			! next CPU 15
	.word	trap_tr15			! first
	.word	trap_tr15 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr16			! next CPU 16
	.word	trap_tr16			! first
	.word	trap_tr16 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr17			! next CPU 17
	.word	trap_tr17			! first
	.word	trap_tr17 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr18			! next CPU 18
	.word	trap_tr18			! first
	.word	trap_tr18 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

	.word	trap_tr19			! next CPU 19
	.word	trap_tr19			! first
	.word	trap_tr19 + TRAP_TSIZE		! limit
	.word	0				! junk for alignment of prom dump

trap_tr0:
	.skip	TRAP_TSIZE

trap_tr1:
	.skip	TRAP_TSIZE

trap_tr2:
	.skip	TRAP_TSIZE

trap_tr3:
	.skip	TRAP_TSIZE

trap_tr4:
	.skip	TRAP_TSIZE

trap_tr5:
	.skip	TRAP_TSIZE

trap_tr6:
	.skip	TRAP_TSIZE

trap_tr7:
	.skip	TRAP_TSIZE

trap_tr8:
	.skip	TRAP_TSIZE

trap_tr9:
	.skip	TRAP_TSIZE

trap_tr10:
	.skip	TRAP_TSIZE

trap_tr11:
	.skip	TRAP_TSIZE

trap_tr12:
	.skip	TRAP_TSIZE

trap_tr13:
	.skip	TRAP_TSIZE

trap_tr14:
	.skip	TRAP_TSIZE

trap_tr15:
	.skip	TRAP_TSIZE

trap_tr16:
	.skip	TRAP_TSIZE

trap_tr17:
	.skip	TRAP_TSIZE

trap_tr18:
	.skip	TRAP_TSIZE

trap_tr19:
	.skip	TRAP_TSIZE

trap_tr_panic:
	.skip	TRAP_ENT_SIZE

#endif	/* TRAPTRACE */

#ifdef	TRACE

TR_intr_start:
	.asciz "interrupt_start:level %d";
	.align 4;
TR_intr_end:
	.asciz "interrupt_end";
	.align 4;
TR_intr_exit:
	.asciz "intr_thread_exit";
	.align 4;

#endif  /* TRACE */

#ifdef	SAS
	.global prom_image
	.type	prom_image, #object
	.size	prom_image, 1*1024*1024
prom_image = 0xffd00000
#endif	SAS

/*
 * Opcodes for instructions in PATCH macros
 */
#define	MOVPSRL0	0xa1480000
#define	MOVL4		0xa8102000
#define	BA		0x10800000
#define	NO_OP		0x01000000
#define SETHI		0x27000000
#define JMP		0x81c4e000


/*
 * Trap vector macros.
 *
 * A single kernel that supports machines with differing
 * numbers of windows has to write the last byte of every
 * trap vector with NW-1, the number of windows minus 1.
 * It does this at boot time after it has read the implementation
 * type from the psr.
 *
 * NOTE: All trap vectors are generated by the following macros.
 * The macros must maintain that a write to the last byte to every
 * trap vector with the number of windows minus one is safe.
 *
 * DOUBLE-NOTE: better only do this when a since cpu is running!
 */
#define TRAP(H) \
	sethi %hi(H),%l3; jmp %l3+%lo(H); mov %psr,%l0; nop;

/* the following trap uses only the trap window, you must be prepared */
#define WIN_TRAP(H) \
	mov %psr,%l0; sethi %hi(H),%l6; jmp %l6+%lo(H); mov %wim,%l3;

#define SYS_TRAP(T) \
	mov %psr,%l0; sethi %hi(sys_trap),%l3; \
	jmp %l3+%lo(sys_trap); mov (T),%l4;

#define LEVEL_INTERRUPT(L) \
	mov %psr,%l0; sethi %hi(interrupt_prologue),%l3; \
	jmp %l3+%lo(interrupt_prologue); mov (L),%l4;

#define TRAP_MON(T) \
	mov %psr,%l0; sethi %hi(trap_mon),%l3; \
	jmp %l3+%lo(trap_mon); mov (T),%l4;

#define TRAP_PSR_ET(H) \
	sethi %hi(H),%l3; jmp %l3+%lo(H); mov %psr,%l0; nop;

#ifdef VIKING_BUG_16
#define GETPSR_TRAP() \
	mov %psr, %i0; rd %y, %g0; jmp %l2; rett %l2+4;
#else
#define GETPSR_TRAP() \
	mov %psr, %i0; jmp %l2; rett %l2+4; nop;
#endif VIKING_BUG_16

#define FAST_TRAP(T) \
	sethi %hi(T), %l3; jmp %l3 + %lo(T); nop; nop;

#define BAD_TRAP	SYS_TRAP((. - scb) >> 4);

#ifdef VIKING_BUG_MFAR2
#define	DATA_TRAP(T) \
	mov %psr, %l0; nop; ba data_trap; mov (T),%l4;
#endif

/*
 * Tracing traps.  For speed we don't save the psr;  all this means is that
 * the condition codes come back different.  This is OK because these traps
 * are only generated by the trace_[0-5]() wrapper functions, and the
 * condition codes are volatile across procedure calls anyway.
 * If you modify any of this, be careful.
 */
#ifdef	TRACE
#define	TRACE_TRAP(H) \
	sethi %hi(H), %l3; jmp %l3 + %lo(H); nop; nop;
#else	/* TRACE */
#define	TRACE_TRAP(H) \
	BAD_TRAP;
#endif	/* TRACE */

#define	PATCH_ST(T, V) \
	set	scb, %g1; \
	set	MOVPSRL0, %g2; \
	st	%g2, [%g1 + ((V)*16+0*4)]; \
	sethi	%hi(sys_trap), %g2; \
	srl	%g2, %l0, %g2; \
	set	SETHI, %g3; \
	or	%g2, %g3, %g2; \
	st	%g2, [%g1 + ((V)*16+1*4)]; \
	set     JMP, %g2; \
	or	%g2, %lo(sys_trap), %g2; \
	st	%g2, [%g1 + ((V)*16+2*4)]; \
	set	MOVL4 + (T), %g2; \
	st	%g2, [%g1 + ((V)*16+3*4)];

#endif	/* lint */

/*
 * Trap vector table.
 * This must be the first text in the boot image.
 *
 * When a trap is taken, we vector to KERNELBASE+(TT*16) and we have
 * the following state:
 *	2) traps are disabled
 *	3) the previous state of PSR_S is in PSR_PS
 *	4) the CWP has been incremented into the trap window
 *	5) the previous pc and npc is in %l1 and %l2 respectively.
 *
 * Registers:
 *	%l0 - %psr immediately after trap
 *	%l1 - trapped pc
 *	%l2 - trapped npc
 *
 * Note: UNIX receives control at vector 0 (trap)
 */

#if defined(lint) || defined(__lint)

void
_start(void)
{}

#else	/* lint */

	ENTRY_NP2(_start, scb)
	TRAP(.entry);				! 00 - reset
	SYS_TRAP(T_FAULT | T_TEXT_FAULT);	! 01 - instruction access
	SYS_TRAP(T_UNIMP_INSTR);		! 02 - illegal instruction
	SYS_TRAP(T_PRIV_INSTR);			! 03 - privileged instruction
	SYS_TRAP(T_FP_DISABLED);		! 04 - floating point disabled
#ifdef	TRAPTRACE
	WIN_TRAP(window_overflow_trace);	! 05 - register window overflow
	WIN_TRAP(window_underflow_trace);	! 06 - register window underflow
#else	/* TRAPTRACE */
	WIN_TRAP(_window_overflow);		! 05 - register window overflow
	WIN_TRAP(_window_underflow);		! 06 - register window underflow
#endif	/* TRAPTRACE */
	SYS_TRAP(T_ALIGNMENT);			! 07 - alignment fault
	SYS_TRAP(T_FP_EXCEPTION);		! 08
#ifdef VIKING_BUG_MFAR2
	DATA_TRAP(T_FAULT | T_DATA_FAULT);	! 09 - data access
#else
	SYS_TRAP(T_FAULT | T_DATA_FAULT);	! 09 - data access
#endif
	SYS_TRAP(T_TAG_OVERFLOW);		! 0A - tag_overflow
	BAD_TRAP;				! 0B
	BAD_TRAP;				! 0C
	BAD_TRAP;				! 0D
	BAD_TRAP;				! 0E
	BAD_TRAP;				! 0F
	BAD_TRAP;				! 10
	LEVEL_INTERRUPT(1);			! 11
	LEVEL_INTERRUPT(2);			! 12
	LEVEL_INTERRUPT(3);			! 13
	LEVEL_INTERRUPT(4);			! 14
	LEVEL_INTERRUPT(5);			! 15
	LEVEL_INTERRUPT(6);			! 16
	LEVEL_INTERRUPT(7);			! 17
	LEVEL_INTERRUPT(8);			! 18
	LEVEL_INTERRUPT(9);			! 19
	LEVEL_INTERRUPT(10);			! 1A
	LEVEL_INTERRUPT(11);			! 1B
	LEVEL_INTERRUPT(12);			! 1C
	LEVEL_INTERRUPT(13);			! 1D
	TRAP(L14_front_end);			! 1E
	LEVEL_INTERRUPT(15);			! 1F

	BAD_TRAP;				! 20
	SYS_TRAP(T_FAULT | T_TEXT_ERROR)	! 21 - instruction access error
	BAD_TRAP;				! 22
	BAD_TRAP;				! 23
	BAD_TRAP;				! 24 - coprocessor disabled
	BAD_TRAP;				! 25 - unimplemented flush
	BAD_TRAP;				! 26
	BAD_TRAP;				! 27
	BAD_TRAP;				! 28 - coprocessor exception
#ifdef VIKING_BUG_MFAR2
	DATA_TRAP(T_FAULT | T_DATA_ERROR);	! 29 - data access error
#else
	SYS_TRAP(T_FAULT | T_DATA_ERROR);	! 29 - data access error
#endif
	SYS_TRAP(T_IDIV0);			! 2A - division by zero
#ifdef VIKING_BUG_MFAR2
	DATA_TRAP(T_FAULT | T_DATA_STORE);	! 2B - data store error
#else
	SYS_TRAP(T_FAULT | T_DATA_STORE);	! 2B - data store error
#endif

/*
 * The rest of the traps in the table up to 0x80 should 'never'
 * be generated by hardware.
 */
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 2C - 2F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 30 - 33
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 34 - 37
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 38 - 3B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 3C - 3F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 40 - 43
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 44 - 47
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 48 - 4B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 4C - 4F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 50 - 53
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 54 - 57
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 58 - 5B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 5C - 5F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 60 - 63
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 64 - 67
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 68 - 6B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 6C - 6F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 70 - 73
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 74 - 77
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 78 - 7B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 7C - 7F

/*
 * User generated traps
 */
	TRAP(syscall_trap_4x);			! 80 - SunOS4.x system call
	SYS_TRAP(T_BREAKPOINT)			! 81 - user breakpoint
	SYS_TRAP(T_DIV0)			! 82 - divide by zero
	WIN_TRAP(fast_window_flush)		! 83 - flush windows
	TRAP(.clean_windows);			! 84 - clean windows
	BAD_TRAP;				! 85 - range check
	TRAP(.fix_alignment)			! 86 - do unaligned references
	BAD_TRAP;				! 87
	TRAP(syscall_trap);			! 88 - SVR4 system call
	TRAP(.set_trap0_addr);			! 89 - set trap0 address
	BAD_TRAP;				! 8A - reserved for hot patch
	BAD_TRAP;				! 8B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 8C - 8F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 90 - 93
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 94 - 97
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 98 - 9B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 9C - 9F
	TRAP(.getcc)				! A0 - get condition codes
	TRAP(.setcc)				! A1 - set condition codes
	GETPSR_TRAP()				! A2 - get processor status
	TRAP(.setpsr)				! A3 - set condition codes
	FAST_TRAP(.get_timestamp)		! A4 - get timestamp
	FAST_TRAP(.get_virtime)			! A5 - get lwp virtual time
	TRAP_PSR_ET(.trap_psr_et);		! A6
	FAST_TRAP(get_hrestime)			! A7 - get hrestime
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! A8 - AB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! AC - AF
	TRACE_TRAP(trace_trap_0)		! B0 - trace, no data
	TRACE_TRAP(trace_trap_1)		! B1 - trace, 1 data word
	TRACE_TRAP(trace_trap_2)		! B2 - trace, 2 data words
	TRACE_TRAP(trace_trap_3)		! B3 - trace, 3 data words
	TRACE_TRAP(trace_trap_4)		! B4 - trace, 4 data words
	TRACE_TRAP(trace_trap_5)		! B5 - trace, 5 data words
	BAD_TRAP;				! B6 - trace, reserved
	TRACE_TRAP(trace_trap_write_buffer)	! B7 - trace, atomic buf write
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! B8 - BB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! BC - BF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C0 - C3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C4 - C7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C8 - CB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! CC - CF
	BAD_TRAP;				! D0
	BAD_TRAP;				! D1
	BAD_TRAP;				! D2
	BAD_TRAP;				! D3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! D4 - D7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! D8 - DB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! DC - DF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E0 - E3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E4 - E7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E8 - EB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! EC - EF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! F0 - F3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! F4 - F7
	BAD_TRAP;				! F8
	BAD_TRAP;				! F9
	BAD_TRAP;				! FA - prom re-entry?
	BAD_TRAP;				! FB
	BAD_TRAP;				! FC
	BAD_TRAP;				! FD
	BAD_TRAP;				! FE
	TRAP_MON(0xff)				! FF - prom re-entry?

/*
 * The number of windows, set once on entry.
 */
	.global nwindows
nwindows:
	.word	8

/*
 * The number of windows - 1, set once on entry.
 */
	.global nwin_minus_one
nwin_minus_one:
	.word	7

/*
 * The window mask, set once on entry.
 */
	.global winmask
winmask:
	.word	8

/*
 * panic when utimers is used (sun4d/os/time.c: setcpudelay)
 */
	.global utimers
utimers = 0

/*
 * System initialization
 *
 * Our deal with OBP and the boot program is that the kernel will
 * be mapped at its linked address by the boot program using 4k
 * pages.  The boot program determines our link address by looking
 * at the kernel's ELF header.	This means we don't have to mess
 * around with mappings to run at our correct address.
 *
 * XXX how much virtual space does the boot program map for the kernel?
 *
 * We are called with romp in %o0 and dvec in %o1.
 *
 * Our tasks are:
 *	save parameters
 *	initialize stack pointer
 *	initialize trap base register
 *	initialize window invalid mask
 *	initialize psr (with traps enabled)
 *	figure out all the module type stuff
 *	dive into main()
 *
 */
.entry:
	mov	%o0, %g7		! save arg (romp) until bss is clear
	mov	%o1, %g6		! save dvec
	mov	%o2, %g5		! save bootops

	mov	0x02, %wim		! setup wim
	set	PSR_S|PSR_PIL|PSR_ET, %g1	! setup psr, leave traps
						!  enabled for monitor XXX
	mov	%g1, %psr
	nop				! psr delay
	nop

	! Do an absolute jump to set the pc to the
	! correct virtual addresses.
	!
	set	1f, %g1
	jmp	%g1
	nop				! delay slot
1:	! *** now running in high virtual memory ***

firsthighinstr:
	!
	! Now we are running with correct addresses
	! and can use non-position independent code.
	!
	!
	! Patch vector 0 trap to "zero" in case it happens again.
	!
	PATCH_ST(T_ZERO, 0)

	!
	! Save the romp, dvec and bootops
	!
	sethi	%hi(romp), %g1
	st	%g7, [%g1 + %lo(romp)]
	sethi	%hi(dvec), %g1
	st	%g6, [%g1 + %lo(dvec)]
	sethi	%hi(bootops), %g1
	st	%g5, [%g1 + %lo(bootops)]

	!
	! Setup trap base and make a kernel stack.
	!
	mov	%tbr, %l4		! save monitor's tbr
	bclr	0xfff, %l4		! remove tt

	!
	! Save monitor's level14 clock interrupt vector code.
	!
	or	%l4, TT(T_INT_LEVEL_14), %o0
	set	mon_clock14_vec, %o1
	ldd	[%o0], %g2
	std	%g2, [%o1]
	ldd	[%o0 + 8], %g2
	std	%g2, [%o1 + 8]

	!
	! Save monitor's breakpoint vector code.
	!
	or	%l4, TT(ST_MON_BREAKPOINT + T_SOFTWARE_TRAP), %o0
	set	mon_breakpoint_vec, %o1
	ldd	[%o0], %g2
	std	%g2, [%o1]
	ldd	[%o0 + 8], %g2
	std	%g2, [%o1 + 8]

	!
	! Switch to our trap base register
	!
	set	scb, %g1		! setup trap handler
	mov	%g1, %tbr

	!
	! Zero thread 0's stack.
	!
	set	t0stack, %g1		! setup kernel stack pointer
	set	T0STKSZ, %l1
0:	subcc	%l1, 4, %l1
	bnz	0b
	clr	[%g1 + %l1]

	set	T0STKSZ, %g2
	add	%g1, %g2, %sp
	sub	%sp, SA(MPCBSIZE), %sp
	mov	0, %fp

	!
	! Dummy up fake user registers on the stack.
	!
	set	USRSTACK-WINDOWSIZE, %g1
	st	%g1, [%sp + MINFRAME + SP*4] ! user stack pointer
	set	PSL_USER, %l0
	st	%l0, [%sp + MINFRAME + PSR*4] ! psr
	set	USRTEXT, %g1
	st	%g1, [%sp + MINFRAME + PC*4] ! pc
	add	%g1, 4, %g1
	st	%g1, [%sp + MINFRAME + nPC*4] ! npc


	call	patch_win_stuff
	nop				! delay slot

	!
	! clear status of any registers that might be latching errors.
	!
	set	RMMU_FAV_REG, %g4
	lda	[%g4]ASI_MOD, %g4	! clear FSR by reading it
	set	RMMU_FSR_REG, %g4
	lda	[%g4]ASI_MOD, %g4	! clear FAR by reading it

	!
	! Initialize global thread register.
	!
	set	t0, THREAD_REG

	!
	! Call mlsetup with address of prototype user registers
	! and romp.
	CPU_INDEX_SLOW(%o1)	! get cpu index in %o1
	SET_FAST_CPU_INDEX(%o1)		! setup cpu index lookup reg. 
	.global cpu0, cpu
	set 	cpu0, %o2
	set	cpu, %o0
	sll	%o1, 2, %o3 	! convert index to word
	st	%o2, [%o0 + %o3]
	st	%o2, [THREAD_REG + T_CPU]
        st	THREAD_REG, [%o2 + CPU_THREAD] 
	add	%sp, REGOFF, %o0	! struct regs *rp
	sethi	%hi(romp), %o2
	ld	[%o2 + %lo(romp)], %o2
	call	mlsetup
	nop				! delay slot
	!
	! Initialize the ITR to cpu0.
	!
	set	cpu0 + CPU_ID, %o0
	call	set_all_itr_by_cpuid
	ld	[%o0], %o0
	!
	! Now call main. We will return as process 1 (init).
	!
	call	main
	nop
	!
	! Proceed as if this was a normal user trap.
	!
	b,a	_sys_rtt		! fake return from trap
	nop				! (possible) delay slot
	SET_SIZE(_start)
	SET_SIZE(scb)

#ifdef	TRAPTRACE
window_overflow_trace:
	!
	! make trace entry - helps in debugging watchdogs
	!
	TRACE_PTR(%l5, %l3)		! get trace pointer
	mov	%tbr, %l3
	st	%l3, [%l5 + TRAP_ENT_TBR]
	st	%l0, [%l5 + TRAP_ENT_PSR]
	st	%l1, [%l5 + TRAP_ENT_PC]
	st	%fp, [%l5 + TRAP_ENT_SP]
	st	%g7, [%l5 + TRAP_ENT_G7]
	add	%l5, TRAP_ENT_TR, %l4	! pointer to trace word
	st	%g0, [%l4]
	TRACE_NEXT(%l5, %l3, %l7)	! set new trace pointer
	b	_window_overflow
	mov	%wim, %l3		! delay slot, restore %l3

window_underflow_trace:
	!
	! make trace entry - helps in debugging watchdogs
	!
	TRACE_PTR(%l5, %l3)		! get trace pointer
	mov	%tbr, %l3
	st	%l3, [%l5 + TRAP_ENT_TBR]
	st	%l0, [%l5 + TRAP_ENT_PSR]
	st	%l1, [%l5 + TRAP_ENT_PC]
	st	%fp, [%l5 + TRAP_ENT_SP]
	st	%g7, [%l5 + TRAP_ENT_G7]
	add	%l5, TRAP_ENT_TR, %l4	! pointer to trace word
	st	%g0, [%l4]
	TRACE_NEXT(%l5, %l3, %l7)	! set new trace pointer
	b	_window_underflow
	mov	%wim, %l3		! delay slot, restore %l3

#endif	TRAPTRACE

	.type	.set_trap0_addr, #function
.set_trap0_addr:
	CPU_ADDR(%l5, %l7)		! load CPU struct addr to %l5 using %l7
	ld	[%l5 + CPU_THREAD], %l5	! load thread pointer
	ld	[%l5 + T_LWP], %l5	! load klwp pointer
	andn	%g1, 3, %g1		! force alignment
	st	%g1, [%l5 + PCB_TRAP0]	! lwp->lwp_pcb.pcb_trap0addr
	jmp	%l2			! return
	rett	%l2 + 4
	SET_SIZE(.set_trap0_addr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
data_trap(void)
{}

#else	/* lint */

#ifdef VIKING_BUG_MFAR2
/*
 * A SuperSPARC bug that happens in rare circumstances makes the
 * MFAR unreliable for data faults.  Although rare, the bug can happen
 * at any time so the MFAR should never be relied on.  When VIKING_BUG_MFAR2
 * is defined we calculate the fault address and write it into the
 * writable MFAR.
 */

/*
 * For data traps calculate the fault address by examining the
 * faulting instruction.  The idea is to insert some code before
 * calling sys_trap to make it look like the hardware worked
 * correctly.  The code looks long, but worst case should
 * be less than 60 instructions.  There are some "jump tables"
 * indexed into by register number to load the value from that
 * register.
 *
 * Register Usage:
 *	%l0 = %psr
 *	%l1 = trap %pc
 *	%l2 = trap %npc
 *	%l3 = %wim
 *	%l4 = faulting instruction
 *	%l5 = fault address
 *	%l6 = scratch/saved %g1
 *	%l7 = scratch/saved %g2
 */

#define	WR_FAR	(RMMU_FAV_REG + 0x1000)		/* writable fault addr reg */
#define	WR_FSR	(RMMU_FSR_REG + 0x1000)		/* writable fault status reg */

	ENTRY_NP(data_trap)
	/*
	 * When SuperSPARC 3.0 parts are out make this workaround
	 * be dynamic.  Change DATA_TRAP macro to put trap type in
	 * %l4.
	 */
	sethi	%hi(mfar2_bug), %l6
	ld	[%l6 + %lo(mfar2_bug)], %l6
	tst	%l6
	be	sys_trap
	nop

	/*
	 * Someone can invalidate the mapping of the pc, so try
	 * to read the instruction with the no-fault bit on.
	 */
	lda	[%g0]ASI_MOD, %l3	! get MMU csr
	or	%l3, MMCREG_NF, %l6	! set no-fault bit
	sta	%l6, [%g0]ASI_MOD	! write csr
	set	RMMU_FSR_REG, %l7	! fault status reg address
	lda	[%l7]ASI_MOD, %l6	! get/clear fault status

	ld	[%l1], %l4		! load faulting instruction

	lda	[%l7]ASI_MOD, %l5	! get fault status
	tst	%l5
	bz	0f
	sethi	%hi(WR_FSR), %l7

	/*
	 * The read of the pc didn't succeed (fsr != 0), so just
	 * return and let the address fault be resolved.  If the
	 * data fault persists we'll be back here.
	 */
	mov	%l0, %psr		! restore condition codes
	nop; nop			! psr delay (plus next instruction)
	sta	%l3, [%g0]ASI_MOD	! restore csr
	jmp	%l1
	rett	%l2

0:
	/* put back the original fault status and csr */
	or	%l7, %lo(WR_FSR), %l7
	sta	%l6, [%l7]ASI_MOD
	sta	%l3, [%g0]ASI_MOD	! restore csr

	mov	%wim, %l3		! save %wim
	mov	%g0, %wim		! clear wim so we can move around

	srl	%l4, 14, %l6		! shift/and to get rs1
	and	%l6, 0x1F, %l6
	cmp	%l6, 7			! is it a global?
	bg	2f
	nop
	sll	%l6, 3, %l6		! mult by 8 for jump tbl offset
	set	1f, %l7			! set base of table
	jmpl	%l7 + %l6, %g0		! go to it
	nop
1:
	ba	.have_rs1;	mov	%g0, %l5
	ba	.have_rs1;	mov	%g1, %l5
	ba	.have_rs1;	mov	%g2, %l5
	ba	.have_rs1;	mov	%g3, %l5
	ba	.have_rs1;	mov	%g4, %l5
	ba	.have_rs1;	mov	%g5, %l5
	ba	.have_rs1;	mov	%g6, %l5
	ba	.have_rs1;	mov	%g7, %l5
2:
	mov	%l6, %l7		! swap %g1 and %l6
	mov	%g1, %l6
	mov	%l7, %g1
	mov	%g2, %l7		! save %g2

	restore				! move into fault window

	sub	%g1, 8, %g1		! rs1 in %g1, sub 8
	sll	%g1, 3, %g1		! mult by 8 for jmp tbl offset
	set	3f, %g2			! set base of table
	jmpl	%g2 + %g1, %g0		! jump to it
	nop
3:
	ba	4f;	mov	%o0, %g2
	ba	4f;	mov	%o1, %g2
	ba	4f;	mov	%o2, %g2
	ba	4f;	mov	%o3, %g2
	ba	4f;	mov	%o4, %g2
	ba	4f;	mov	%o5, %g2
	ba	4f;	mov	%o6, %g2
	ba	4f;	mov	%o7, %g2

	ba	4f;	mov	%l0, %g2
	ba	4f;	mov	%l1, %g2
	ba	4f;	mov	%l2, %g2
	ba	4f;	mov	%l3, %g2
	ba	4f;	mov	%l4, %g2
	ba	4f;	mov	%l5, %g2
	ba	4f;	mov	%l6, %g2
	ba	4f;	mov	%l7, %g2

	ba	4f;	mov	%i0, %g2
	ba	4f;	mov	%i1, %g2
	ba	4f;	mov	%i2, %g2
	ba	4f;	mov	%i3, %g2
	ba	4f;	mov	%i4, %g2
	ba	4f;	mov	%i5, %g2
	ba	4f;	mov	%i6, %g2
	ba	4f;	mov	%i7, %g2
4:
	save				! back to trap window

	mov	%g2, %l5		! put rs1 in %l5
	mov	%l6, %g1		! restore %g1
	mov	%l7, %g2		! restore %g2

.have_rs1:
	/*
	 * now we've got rs1 in %l5, let's
	 * get rs2 or simm13
	 */
	srl	%l4, 13, %l6		! shift inst to get 'i' bit
	btst	1, %l6			! check for immediate
	be	0f
	nop
	/* immediate */
	sll	%l4, 19, %l6		! it's immediate, so
	sra	%l6, 19, %l6		! sign-extend the 13-bit number
	add	%l5, %l6, %l5		! add rs1 + simm13
	ba	.have_both
	nop
0:
	/* rs2 */
	and	%l4, 0x1F, %l6		! get rs2
	cmp	%l6, 7			! is it global?
	bg	2f
	nop
	sll	%l6, 3, %l6		! mult by 8 for jmp tbl offset
	set	1f, %l7			! set base of table
	jmpl	%l7 + %l6, %g0		! jump to it
	nop
1:
	ba	.have_both;	add	%g0, %l5, %l5
	ba	.have_both;	add	%g1, %l5, %l5
	ba	.have_both;	add	%g2, %l5, %l5
	ba	.have_both;	add	%g3, %l5, %l5
	ba	.have_both;	add	%g4, %l5, %l5
	ba	.have_both;	add	%g5, %l5, %l5
	ba	.have_both;	add	%g6, %l5, %l5
	ba	.have_both;	add	%g7, %l5, %l5
2:
	mov	%l6, %l7		! swap %g1 and %l6
	mov	%g1, %l6
	mov	%l7, %g1
	mov	%g2, %l7		! save %g2

	restore				! fault window

	sub	%g1, 8, %g1		! rs2 in %g1, sub 8
	sll	%g1, 3, %g1		! mult by 8 for jmp tbl offset
	set	3f, %g2
	jmpl	%g2 + %g1, %g0		! jump to it
	nop
3:
	ba	4f;	mov	%o0, %g2
	ba	4f;	mov	%o1, %g2
	ba	4f;	mov	%o2, %g2
	ba	4f;	mov	%o3, %g2
	ba	4f;	mov	%o4, %g2
	ba	4f;	mov	%o5, %g2
	ba	4f;	mov	%o6, %g2
	ba	4f;	mov	%o7, %g2

	ba	4f;	mov	%l0, %g2
	ba	4f;	mov	%l1, %g2
	ba	4f;	mov	%l2, %g2
	ba	4f;	mov	%l3, %g2
	ba	4f;	mov	%l4, %g2
	ba	4f;	mov	%l5, %g2
	ba	4f;	mov	%l6, %g2
	ba	4f;	mov	%l7, %g2

	ba	4f;	mov	%i0, %g2
	ba	4f;	mov	%i1, %g2
	ba	4f;	mov	%i2, %g2
	ba	4f;	mov	%i3, %g2
	ba	4f;	mov	%i4, %g2
	ba	4f;	mov	%i5, %g2
	ba	4f;	mov	%i6, %g2
	ba	4f;	mov	%i7, %g2
4:
	save				! back to trap window

	add	%g2, %l5, %l5		! rs1 + rs2
	mov	%l6, %g1		! restore %g1
	mov	%l7, %g2		! restore %g2
.have_both:
	/* write it to the writable far */
	set	WR_FAR, %l6
	sta	%l5, [%l6]ASI_MOD

	/* restore %wim */
	mov	%l3, %wim

	/* set up the locals like sys_trap expects them */
	mov	%tbr, %l4
	srl	%l4, 4, %l4
	and	%l4, 0xFF, %l4
	or	%l4, T_FAULT, %l4
	sethi   %hi(nwin_minus_one), %l6
	ba	sys_trap
	ld      [%l6+%lo(nwin_minus_one)], %l6
	SET_SIZE(data_trap)
#endif	/* VIKING_BUG_MFAR2 */

#endif	/* lint */

/*
 * Generic system trap handler.
 *
 * Register Usage:
 *	%l0 = trap %psr
 *	%l1 = trap %pc
 *	%l2 = trap %npc
 *	%l3 = %wim
 *	%l4 = trap type
 *	%l5 = CWP
 *	%l6 = nwindows - 1
 *	%l7 = stack pointer
 *
 *	%g4 = scratch
 *	%g5 = lwp pointer
 *	%g7 = thread pointer
 */
#if defined(lint) || defined(__lint)

void
sys_trap(void)
{}

#else	/* lint */

	ENTRY_NP(sys_trap)
#ifdef	TRAPTRACE
	!
	! make trace entry - helps in debugging watchdogs
	!
	TRACE_PTR(%l5, %l3)		! get trace pointer
	mov	%tbr, %l3
	st	%l3, [%l5 + TRAP_ENT_TBR]
	st	%l0, [%l5 + TRAP_ENT_PSR]
	st	%l1, [%l5 + TRAP_ENT_PC]
	st	%fp, [%l5 + TRAP_ENT_SP]
	st	%g7, [%l5 + TRAP_ENT_G7]
	TRACE_NEXT(%l5, %l3, %l7)	! set new trace pointer
#endif	TRAPTRACE

#ifdef DUMP_LBOLT_TO_LED
	sethi 	%hi(lbolt),%l7
	ld	[%l7 + %lo(lbolt)], %l7
	DUMP_WORD_TO_LOLED(%l7,%l3,%l7);
	DUMP_WORD_TO_HILED(%l2,%l3,%l7);
#endif 
	!
	! Prepare to go to C (batten down the hatches).
	!
	mov	0x01, %l5		! CWM = 0x01 << CWP
	sll	%l5, %l0, %l5
	mov	%wim, %l3		! get WIM
	btst	PSR_PS, %l0		! test pS
	bz	.st_user		! branch if user trap
	btst	%l5, %l3		! delay slot, compare WIM and CWM

	!
	! Trap from supervisor.
	! We can be either on the system stack or interrupt stack.
	!
	sub	%fp, MINFRAME+REGSIZE, %l7 ! save sys globals on stack

	SAVE_GLOBALS(%l7 + MINFRAME)
	SAVE_OUTS(%l7 + MINFRAME)

#ifdef TRACE
	! We do this now, rather than at the very start of sys_trap,
	! because the _SAVE_GLOBALS above gives us free scratch registers.
	!
	TRACE_SYS_TRAP_START(%l4, %g1, %g2, %g3, %g4, %g5)
	btst	%l5, %l3		! retest: compare WIM and CWM
#endif	/* TRACE */

.st_cleanglo:
	!
	! Restore %g7 (THREAD_REG) in case we came in from the PROM or kadb
	!
	CPU_ADDR(%l6, THREAD_REG)		! load CPU struct addr
	ld	[%l6 + CPU_THREAD], THREAD_REG	! load thread pointer
	!
	! Call mmu_getsyncflt to get the synchronous fault status
	! register and the synchronous fault address register stored in
	! the per-CPU structure.
	!
	! The reason for reading these synchronous fault registers now is
	! that the window overflow processing that we are about to do may
	! also cause another fault.
	!
	! Note that mmu_getsyncflt assumes that it can clobber %g1 and %g4.
	!

	btst	T_FAULT, %l4
	bz,a	2f
	clr	[%l6 + CPU_SYNCFLT_STATUS]

	mov	%o7, %g3		! save %o7 in %g3
	call	mmu_getsyncflt
	nop
	mov	%g3, %o7		! restore %o7.
2:

#ifdef TRAPWINDOW
	.global	.st_trapsave
.st_trapsave:
	!
	! store the window at the time of the trap into a static area.
	!
	set	trap_window, %g1
	mov	%wim, %g2
	st	%g2, [%g1+96]
	mov	%psr, %g2
	restore
	st %o0, [%g1   ]; st %o1, [%g1+ 4]; st %o2, [%g1+ 8]; st %o3, [%g1+12]
	st %o4, [%g1+16]; st %o5, [%g1+20]; st %o6, [%g1+24]; st %o7, [%g1+28]
	st %l0, [%g1+32]; st %l1, [%g1+36]; st %l2, [%g1+40]; st %l3, [%g1+44]
	st %l4, [%g1+48]; st %l5, [%g1+52]; st %l6, [%g1+56]; st %l7, [%g1+60]
	st %i0, [%g1+64]; st %i1, [%g1+68]; st %i2, [%g1+72]; st %i3, [%g1+76]
	st %i4, [%g1+80]; st %i5, [%g1+84]; st %i6, [%g1+88]; st %i7, [%g1+92]
	mov	%g2, %psr	! why isn't this "save"?
	nop; nop; nop;		! delay required here?
#endif	TRAPWINDOW
.st_cpusave:
	st	%fp, [%l7 + MINFRAME + SP*4]	! stack pointer
	st	%l0, [%l7 + MINFRAME + PSR*4]	! psr
	st	%l1, [%l7 + MINFRAME + PC*4]	! pc

	!
	! If we are in last trap window, all windows are occupied and
	! we must do window overflow stuff in order to do further calls
	!
	btst	%l5, %l3			! retest ((CWM & WIM) == 0)
	bz	.st_have_window			! if (true) no overflow
	st	%l2, [%l7 + MINFRAME + nPC*4]	! npc, delay slot
	b,a	.st_sys_ovf

.st_user:
	!
	! Trap from user. Save user globals and prepare system stack.
	! Test whether the current window is the last available window
	! in the register file (CWM == WIM).
	!
	CPU_ADDR(%l6, %l7)		! load CPU struct addr to %l6 using %l7
	ld	[%l6 + CPU_THREAD], %l7 ! load thread pointer
	ld	[%l7 + T_STACK], %l7	! %l7 is lwp's kernel stack
	SAVE_GLOBALS(%l7 + MINFRAME)

#ifdef TRACE
	! We do this now, rather than at the very start of sys_trap,
	! because the _SAVE_GLOBALS above gives us free scratch registers.
	!
	TRACE_SYS_TRAP_START(%l4, %g1, %g2, %g3, %g4, %g5)
	btst	%l5, %l3		! retest: compare WIM and CWM
#endif  /* TRACE */

	SAVE_OUTS(%l7 + MINFRAME)
	mov	%l7, %g5		! stack base is also mpcb ptr

	!
	! Call mmu_getsyncflt to get the synchronous fault status
	! register and the synchronous fault address register stored in
	! the per-CPU structure.
	!
	! The reason for reading these synchronous fault registers now is
	! that the window overflow processing that we are about to do may
	! also cause another fault.
	!
	! Note that mmu_getsyncflt assumes that it can clobber %g1 and %g4.
	!

	btst	T_FAULT, %l4
	bz,a	1f
	clr	[%l6 + CPU_SYNCFLT_STATUS]

	mov	%o7, %g3		! save %o7 in %g3
	call	mmu_getsyncflt
	nop
	mov	%g3, %o7		! restore %o7.
1:
	ld	[%l6 + CPU_THREAD], THREAD_REG	! load thread pointer
	sethi	%hi(nwin_minus_one), %l6 ! re-load NW - 1
	ld	[%l6 + %lo(nwin_minus_one)], %l6
	st	%l0, [%l7 + MINFRAME + PSR*4] ! psr
	st	%l1, [%l7 + MINFRAME + PC*4] ! pc
	st	%l2, [%l7 + MINFRAME + nPC*4] ! npc

.st_user_lastwin:
	!
	! If we are in last trap window, all windows are occupied and
	! we must do window overflow stuff in order to do further calls
	!

	btst	%l5, %l3		! if ((CWM & WIM) == 0)
	bz	1f			!	no overflow
	clr	[%g5 + MPCB_WBCNT]	! delay slot, *(save buffer) = 0
	not	%l5, %g2		! UWM = ~CWM
	sethi	%hi(winmask), %g3
	ld	[%g3 + %lo(winmask)], %g3
	andn	%g2, %g3, %g2
	b	.st_user_ovf		! overflow
	srl	%l3, 1, %g1		! ror(WIM, 1, NW)

	!
	! Compute the user window mask (mpcb_uwm), which is a mask of
	! window which contain user data. It is all the windows "between"
	! CWM and WIM.
	!
1:
	subcc	%l3, %l5, %g1		! if (WIM >= CWM)
	bneg,a	2f			!	mpcb_uwm = (WIM-CWM)&~CWM
	sub	%g1, 1, %g1		! else
2:					!	mpcb_uwm = (WIM-CWM-1)&~CWM
	sethi	%hi(winmask), %g3	! load the window mask.
	ld	[%g3 + %lo(winmask)], %g3
	bclr	%l5, %g1
	andn	%g1, %g3, %g1
	st	%g1, [%g5 + MPCB_UWM]

.st_have_window:
	!
	! The next window is open.
	!
	mov	%l7, %sp		! setup previously computed stack
	!
	! Process trap according to type
	!
	btst	T_INTERRUPT, %l4	! interrupt
	bnz	_interrupt
	btst	T_FAULT, %l4		! fault

fixfault:
	bnz,a	fault
	bclr	T_FAULT, %l4
	cmp	%l4, T_FP_EXCEPTION	! floating point exception
	be	_fp_exception
	cmp	%l4, T_FLUSH_WINDOWS	! flush user windows to stack
	bne	1f
	wr	%l0, PSR_ET, %psr	! enable traps
	nop				! psr delay

	!
	! Flush windows trap.
	!
	call	flush_user_windows	! flush user windows
	nop
	!
	! Don't redo trap instruction.
	!
	ld	[%sp + MINFRAME + nPC*4], %g1
	st	%g1, [%sp + MINFRAME + PC*4]	! pc = npc
	add	%g1, 4, %g1
	b	_sys_rtt
	st	%g1, [%sp + MINFRAME + nPC*4] ! npc = npc + 4

1:
	!
	! All other traps. Call C trap handler.
	!
	mov	%l4, %o0		! trap(t, rp)
	clr	%o2			!  addr = 0
	clr	%o3			!  be = 0
	mov	S_OTHER, %o4		!  rw = S_OTHER
	call	trap			! C trap handler
	add	%sp, MINFRAME, %o1
	b,a	_sys_rtt		! return from trap

/*
 * Sys_trap overflow handling.
 * Psuedo subroutine returns to .st_have_window.
 */
.st_sys_ovf:
	!
	! Overflow from system.
	! Determine whether the next window is a user window.
	! If lwp->mpcb_uwm has any bits set, then it is a user
	! which must be saved.
	!
	ld	[%l6 + CPU_MPCB], %g5
	sethi	%hi(nwin_minus_one), %l6	! re-load NW - 1
	ld	[%l6 + %lo(nwin_minus_one)], %l6
	tst	%g5			! mpcb == 0 for kernel threads
	bz	1f			! skip uwm checking when lwp == 0
	srl	%l3, 1, %g1		! delay, WIM = %g1 = ror(WIM, 1, NW)
	ld	[%g5 + MPCB_UWM], %g2	! if (mpcb.mpcb_uwm)
	tst	%g2			!	user window
	bnz	.st_user_ovf
	nop
1:
	!
	! Save supervisor window. Compute the new WIM and change current window
	! to the window to be saved.
	!
	sll	%l3, %l6, %l3		! %l6 == NW-1
	or	%l3, %g1, %g1
	save				! get into window to be saved
	mov	%g1, %wim		! install new WIM

	!
	! Save window on the stack.
	!
.st_stack_res:
	SAVE_WINDOW(%sp)
	b	.st_have_window		! finished overflow processing
	restore				! delay slot, back to original window

.st_user_ovf:
	!
	! Overflow. Window to be saved is a user window.
	! Compute the new WIM and change the current window to the
	! window to be saved.
	!
	! On entry:
	!
	!	%l6 = nwindows - 1
	!	%g1 = ror(WIM, 1)
	!	%g2 = UWM
	!	%g5 = mpcb pointer
	!

	sll	%l3, %l6, %l3		! %l6 == NW-1
	or	%l3, %g1, %g1
	bclr	%g1, %g2		! turn off uwm bit for window
	st	%g2, [%g5 + MPCB_UWM]	! we are about to save
	save				! get into window to be saved
	mov	%g1, %wim		! install new WIM

	ld	[%g5 + MPCB_SWM], %g4	! test shared window mask
	btst	%g1, %g4		! saving shared window?
	bz,a	1f			! no, not shared window
	mov	%sp, %g4		! delay - stack pointer to be used

	!
	! save kernel copy of shared window
	!
	clr	[%g5 + MPCB_SWM]	! clear shared window mask
	SAVE_WINDOW(%sp)		! save kernel copy of shared window
	! use saved stack pointer for user copy
	ld	[%g5 + MPCB_REGS + SP*4], %g4

	!
	! In order to save the window onto the stack, the stack
	! must be aligned on a word boundary, and the part of the
	! stack where the save will be done must be present.
	! We first check for alignment.
	!
1:
	btst	0x7, %g4		! test sp alignment - actual %sp in %g4
	set	KERNELBASE, %g1
	bnz	.st_stack_not_res	! stack misaligned, catch it later
	cmp	%g1, %g4		! test for touching non-user space
	bleu	.st_stack_not_res	! kernel address in %g4 (user %sp)
	nop

	!
	! Branch to code which is dependent upon the particular
	! type of SRMMU module.	 This code will attempt to save
	! the overflowed window onto the stack, without first
	! verifying the presence of the stack.
	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to save
	! the window onto the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the save won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the save actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	!
	! Note that a naive "if (probe ok) store" approach won't work because
	! pageout() could steal the page in between the probe and the store.
	!
	! Uses %g1 and %g2
	!
.st_assume_stack_res:
	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g0	! of the SFSR.

	lda	[%g0]ASI_MOD, %g2	! turn on no-fault bit in
	or	%g2, MMCREG_NF, %g2	! mmu control register to
	sta	%g2, [%g0]ASI_MOD	! prevent taking a fault.

	SAVE_WINDOW(%g4)		! try to save reg window

	andn	%g2, MMCREG_NF, %g2	! turn off no-fault bit
	sta	%g2, [%g0]ASI_MOD

	set	RMMU_FAV_REG, %g2
	lda	[%g2]ASI_MOD, %g2	! read SFAR
	lda	[%g1]ASI_MOD, %g1	! read SFSR

	btst    SFSREG_FAV, %g1         ! did a fault occurr?
	bz,a	.st_have_window
	restore				! delay slot, back to original window

	!
	! A fault occurred, so the stack is not resident.
	!
.st_stack_not_res:
	!
	! User stack is not resident, save in u area for processing in sys_rtt.
	!
	ld	[%g5 + MPCB_WBCNT], %g1
	sll	%g1, 2, %g1		! convert to spbuf offset

	add	%g1, %g5, %g2
	st	%g4, [%g2 + MPCB_SPBUF]	! save sp in %g1+curthread+MPCB_SPBUF
	sll	%g1, 4, %g1		! convert wbcnt to pcb_wbuf offset

	add	%g1, %g5, %g2
#if (MPCB_WBUF > ((1<<12)-1))
ERROR - pcb_wbuf offset to large
#endif
	add	%g2, MPCB_WBUF, %g2
	SAVE_WINDOW(%g2)
	srl	%g1, 6, %g1		! increment lwp->lwp_pcb->pcb_wbcnt
	add	%g1, 1, %g1
	st	%g1, [%g5 + MPCB_WBCNT]
	!
	! Set the AST flag so that PCW_WBCNT will be seen before returning
	! from the system call. _sys_rtt does not do so when returing back to
	! supervior mode(PSR_PS).
	!
	ld	[%g5 + MPCB_THREAD], %g1
chkt_intr:				! follow thread->t_intr to get a
	ld	[%g1 + T_INTR], %g2	! non-interrupt thread
	tst	%g2
	bnz,a	chkt_intr
	mov	%g2, %g1
	mov	1, %g2
	stb	%g2, [%g1 + T_ASTFLAG]	! make sure new syscall sees it
	b	.st_have_window		! finished overflow processing
	restore				! delay slot, back to original window
	SET_SIZE(sys_trap)

#endif	/* lint */

/*
 * Return from sys_trap routine.
 */

#if defined(lint) || defined(__lint)

void
_sys_rtt(void)
{}

void
sys_rtt_syscall(void)
{}

#else	/* lint */

	ENTRY_NP(_sys_rtt)
#ifdef TRAPTRACE
	mov	%psr, %g1
	andn	%g1, PSR_ET, %g2	! disable traps
	mov	%g2, %psr
	nop; nop; nop;
	TRACE_PTR(%g3, %o1)		! get trace pointer
	set	0x6666, %o1
	st	%o1, [%g3 + TRAP_ENT_TBR]
	st	%l0, [%g3 + TRAP_ENT_PSR]
	st	%l1, [%g3 + TRAP_ENT_PC]
	st	%fp, [%g3 + TRAP_ENT_SP]
	st	%g7, [%g3 + TRAP_ENT_G7]
	st	%l4, [%g3 + 0x14]
	ld	[%g7 + T_CPU], %o1
	ld	[%o1 + CPU_BASE_SPL], %o1
	st	%o1, [%g3 + 0x18]
	TRACE_NEXT(%g3, %o0, %o1)	! set new trace pointer
	mov	%g1, %psr
	nop; nop; nop;
#endif
	ld	[%sp + MINFRAME + PSR*4], %l0 ! get saved psr
	btst	PSR_PS, %l0		! test pS for return to supervisor
	bnz	.sr_sup
	mov	%psr, %g1

.sr_user:
	!
	! Return to User.
	! Turn off traps using the current CWP
	! (because we are returning to user).
	! Test for AST for resched. or prof.
	!
	and	%g1, PSR_CWP, %g1	! insert current CWP in old psr
	andn	%l0, PSR_CWP, %l0
	or	%l0, %g1, %l0
	mov	%l0, %psr		! install old psr, disable traps
	nop; nop; nop;			! psr delay

#ifdef TRAPTRACE
	mov	%psr, %g1
	andn	%g1, PSR_ET, %g2	! disable traps
	mov	%g2, %psr
	nop; nop; nop;
	TRACE_PTR(%g3, %o1)		! get trace pointer
	set	0x7777, %o1
	st	%o1, [%g3 + TRAP_ENT_TBR]
	st	%l0, [%g3 + TRAP_ENT_PSR]
	st	%l1, [%g3 + TRAP_ENT_PC]
	st	%fp, [%g3 + TRAP_ENT_SP]
	st	%g7, [%g3 + TRAP_ENT_G7]
	st	%l4, [%g3 + 0x14]
	ld	[%g7 + T_CPU], %o1
	ld	[%o1 + CPU_BASE_SPL], %o1
	st	%o1, [%g3 + 0x18]
	TRACE_NEXT(%g3, %o0, %o1)	! set new trace pointer
	mov	%g1, %psr
	nop; nop; nop;
#endif
	ldub	[THREAD_REG + T_ASTFLAG], %g1
	mov	%sp, %g5		! user stack base is mpcb ptr
	tst	%g1			! test signal or AST pending
	bz	1f
	ld	[%g5 + MPCB_WBCNT], %g3	! delay - user regs been saved?

.doast:
	!
	! Let trap handle the AST.
	!
	or	%l0, PSR_PIL, %o1	! spl8 first to protect CPU base pri
	mov	%o1, %psr		! in case of changed priority (IU bug)
	wr	%o1, PSR_ET, %psr	! turn on traps
	nop				! psr delay
	call	splx			! psr delay - splx()
	mov	%l0, %o0		! psr delay - pass old %psr to splx()
	mov	T_AST, %o0
	call	trap			! trap(T_AST, rp)
	add	%sp, MINFRAME, %o1
	b,a	_sys_rtt

1:
	!
	! If user regs have been saved to the window buffer we must clean it.
	!
	tst	%g3
	bz,a	2f
	ld	[%g5 + MPCB_UWM], %l4	! user windows in reg file?

	!
	! User regs have been saved into the PCB area.
	! Let trap handle putting them on the stack.
	!
	or	%l0, PSR_PIL, %o1	! spl8 first to protect CPU base pri
	mov	%o1, %psr		! set priority before enabling (IU bug)
	wr	%o1, PSR_ET, %psr	! turn on traps
	nop				! psr delay
	call	splx			! psr delay - splx()
	mov	%l0, %o0		! psr delay - pass old %psr to splx()
	mov	T_FLUSH_PCB, %o0
	call	trap			! trap(T_FLUSH_PCB, rp)
	add	%sp, MINFRAME, %o1
	b,a	_sys_rtt
2:
	!
	! We must insure that the rett will not take a window underflow trap.
	!
	RESTORE_OUTS(%sp + MINFRAME)	! restore user outs
	tst	%l4
	bnz	.sr_user_regs
	ld	[%sp + MINFRAME + PC*4], %l1 ! restore user pc

	!
	! The user has no windows in the register file.
	! Try to get one from the stack.
	!
	sethi	%hi(nwin_minus_one), %l6	! NW-1 for rol calculation
	ld	[%l6 + %lo(nwin_minus_one)], %l6

	mov	%wim, %l3		! get wim
	sll	%l3, 1, %l4		! next WIM = rol(WIM, 1, NW)
	srl	%l3, %l6, %l5		! %l6 == NW-1
	or	%l5, %l4, %l5
	mov	%l5, %wim		! install it

	!
	! In order to restore the window from the stack, the stack
	! must be aligned on a word boundary, and the part of the
	! stack where the save will be done must be present.
	! We first check for alignment.
	!
	btst	0x7, %fp		! test fp alignment
	bz	.sr_assume_stack_res
	nop

	!
	! A user underflow with a misaligned sp.
	! Fake a memory alignment trap.
	!
	mov	%l3, %wim		! restore old wim

.sr_align_trap:
	or	%l0, PSR_PIL, %o1	! spl8 first to protect CPU base pri
	mov	%o1, %psr		! set priority before enabling (IU bug)
	wr	%o1, PSR_ET, %psr	! turn on traps
	nop
	call	splx			! splx()
	mov	%l0, %o0		! delay - pass old %psr to splx()
	mov	T_ALIGNMENT, %o0
	call	trap			! trap(T_ALIGNMENT, rp)
	add	%sp, MINFRAME, %o1
	b,a	_sys_rtt

	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to restore
	! the window from the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the restore won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the restore actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	!
	! Other sun4 trap handlers first probe for the stack, and
	! then, if the stack is present, they restore from the stack.
	! This approach CANNOT be used with a multiprocessor system
	! because of a race condition: between the time that the
	! stack is probed, and the restore from the stack is done, the
	! stack could be stolen by the page daemon.
	!
.sr_assume_stack_res:
	! First see if we can load the window from the pcb
	ld	[%g5 + MPCB_RSP], %g1	! test for user return window in pcb
	cmp	%fp, %g1
	bne	1f			! no user return window
	clr	[%g5 + MPCB_RSP]
	restore
	RESTORE_WINDOW(%g5 + MPCB_RWIN)	! restore from user return window
	mov	%fp, %g2
	save
	!
	! If there is another window to be restored from the pcb,
	! allocate another window and restore it.
	tst	%g2
	bz,a	.sr_user_regs		! no additional window
	clr	[%g5 + MPCB_RSP + 4]
	ld	[%g5 + MPCB_RSP + 4], %g1
	cmp	%g2, %g1
	bne	.sr_user_regs		! no additional window
	clr	[%g5 + MPCB_RSP + 4]
	!
	! We have another window.  Compute %wim once again.
	! %l6 still contains nwin_minus_one.
	mov	%wim, %l3		! get wim
	sll	%l3, 1, %l4		! next WIM = rol(WIM, 1, NW)
	srl	%l3, %l6, %l5		! %l6 == NW-1
	or	%l5, %l4, %l5
	mov	%l5, %wim		! install it
	nop; nop; nop			! wim delay
	restore
	restore
	RESTORE_WINDOW(%g5 + MPCB_RWIN + 4*16)
	save
	save
	b,a	.sr_user_regs
1:
	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g0	! of the SFSR.

	lda	[%g0]ASI_MOD, %g2	! turn on no-fault bit in
	or	%g2, MMCREG_NF, %g2	! mmu control register to
	sta	%g2, [%g0]ASI_MOD	! prevent taking a fault

	restore
	RESTORE_WINDOW(%sp)		! try to restore reg window
	save

	andn	%g2, MMCREG_NF, %g2	! turn off no-fault bit
	sta	%g2, [%g0]ASI_MOD

	set	RMMU_FAV_REG, %g2
	lda	[%g2]ASI_MOD, %g2	! read SFAR
	lda	[%g1]ASI_MOD, %g1	! read SFSR

	btst    SFSREG_FAV, %g1         ! did a fault occurr?
	bz	.sr_user_regs
	nop

.sr_stack_not_res:
	!
	! Restore area on user stack is not resident.
	! We punt and fake a page fault so that trap can bring the page in.
	!
	mov	%l3, %wim		! restore old wim
	or	%l0, PSR_PIL, %o1	! spl8 first to protect CPU base pri
	mov	%o1, %psr		! set priority before enabling (IU bug)
	wr	%o1, PSR_ET, %psr	! turn on traps
	nop
	call	splx			! splx() (preserves %g1, %g2)
	mov	%l0, %o0		! delay - pass old %psr to splx()
	mov	T_SYS_RTT_PAGE, %o0
	add	%sp, MINFRAME, %o1
	mov	%g2, %o2		! save fault address
	mov	%g1, %o3		! save fault status
	call	trap			! trap(T_SYS_RTT_PAGE,
	mov	S_READ, %o4		!	rp, addr, be, S_READ)
	b,a	_sys_rtt

.sr_user_regs:
	!
	! Fix saved %psr so the PIL will be CPU->cpu_base_pri
	!
	ld	[THREAD_REG + T_CPU], %o1	! get CPU_BASE_SPL
	ld	[%o1 + CPU_BASE_SPL], %o1
	andn	%l0, PSR_PIL, %l0
	or	%o1, %l0, %l0		! fixed %psr
! XXX - add test for clock level here
	!
	! User has at least one window in the register file.
	!
	ld	[%g5 + MPCB_FLAGS], %l3
	ld	[%sp + MINFRAME + nPC*4], %l2 ! user npc

	!
	! check user pc alignment.  This can get messed up either using
	! ptrace, or by using the '-T' flag of ld to place the text
	! section at a strange location (bug id #1015631)
	!
	or	%l1, %l2, %g2
	btst	0x3, %g2
	bz,a	1f
	btst	CLEAN_WINDOWS, %l3
	b,a	.sr_align_trap

1:
	bz,a	3f
	mov	%l0, %psr		! install old PSR_CC

	!
	! Maintain clean windows.
	!
	mov	%wim, %g2		! put wim in global
	mov	0, %wim			! zero wim to allow saving
	mov	%l0, %g3		! put original psr in global
	b	2f			! test next window for invalid
	save
	!
	! Loop through windows past the trap window
	! clearing them until we hit the invlaid window.
	!
1:
	clr	%l1			! clear the window
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o6
	clr	%o7
	save
2:
	mov	%psr, %g1		! get CWP
	srl	%g2, %g1, %g1		! test WIM bit
	btst	1, %g1
	bz,a	1b			! not invalid window yet
	clr	%l0			! clear the window

	!
	! Clean up trap window.
	!
	mov	%g3, %psr		! back to trap window, restore PSR_CC
	mov	%g2, %wim		! restore wim
	nop; nop;			! psr delay

#ifdef TRACE
	!
	! We do this right before the _RESTORE_GLOBALS, so we can safely
	! use the globals as scratch registers.
	! On entry, %g3 = %psr.
	!
	TRACE_SYS_TRAP_END(%g1, %g2, %g6, %g4, %g5)
	mov	%g3, %psr
#endif  /* TRACE */

	RESTORE_GLOBALS(%sp + MINFRAME)	! restore user globals
	mov	%l1, %o6		! put pc, npc in unobtrusive place
	mov	%l2, %o7
	clr	%l0			! clear the rest of the window
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%o6			! return
	rett	%o7
3:

#ifdef TRACE
	!
	! We do this right before the _RESTORE_GLOBALS, so we can safely
	! use the globals as scratch registers.
	! On entry, %l0 = %psr.
	!
	TRACE_SYS_TRAP_END(%g1, %g2, %g3, %g4, %g5)
	mov	%l0, %psr
#endif  /* TRACE */

	RESTORE_GLOBALS(%sp + MINFRAME) ! restore user globals
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l1			! return
	rett	%l2			! delay slot

	!
	! Return to supervisor
	! %l0 = old %psr.
	! %g1 = current %psr
	!
.sr_sup:
	!
	! Check for a kernel preemption request
	!
	ld	[THREAD_REG + T_CPU], %g5	
	ldub	[%g5 + CPU_KPRUNRUN], %g5	! get CPU->cpu_kprunrun
	tst	%g5
	bz	2f
	nop					! delay slot

	!
	! Attempt to preempt, first checking to see if a clock interrupt
	! on this stack is already in the middle of a kpreempt (to
	! prevent stack overflow)
	!

	ldstub	[THREAD_REG + T_PREEMPT_LK], %g5	! load preempt lock
	tst	%g5			! can we call kpreempt?
	bnz	2f			! ...not if this thread is already
	nop				!  in it...

	call	kpreempt
	mov	%l0, %o0		! pass original interrupt level

	stub	%g0, [THREAD_REG + T_PREEMPT_LK]	! nuke the lock	

	mov	%psr, %g1		! reload %g1
	and	%l0, PSR_PIL, %o0	! compare old pil level
	and	%g1, PSR_PIL, %g5	!   with current pil level
	subcc	%o0, %g5, %o0
	bgu,a	2f			! if current is lower,
	sub	%l0, %o0, %l0		!   drop old pil to current level
2:
	!
	! We will restore the trap psr. This has the effect of disabling
	! traps and changing the CWP back to the original trap CWP. This
	! completely restores the PSR (except possibly for the PIL).
	! We only do this for supervisor return since users can't manipulate
	! the psr.  Kernel code modifying the PSR must mask interrupts and
	! then compare the proposed new PIL to the one in CPU->cpu_base_spl.
	!
	sethi	%hi(nwindows), %g5
	ld	[%g5 + %lo(nwindows)], %g5	! number of windows

	ld	[%sp + MINFRAME + SP*4], %fp	! get sys sp
	wr	%g1, PSR_ET, %psr		! disable traps, for later
	xor	%g1, %l0, %g2			! test for CWP change
	nop; nop;				! psr delay
	btst	PSR_CWP, %g2			! watch out for wr %psr
	bz	.sr_samecwp
	mov	%l0, %g3			! delay - save old psr
	!
	! The CWP will be changed. We must save sp and the ins
	! and recompute WIM. We know we need to restore the next
	! window in this case.
	!
	! using %g4 here doesn't interfere with fault info. stored in %g4,
	! since we are currently returning from the trap.
	mov	%sp, %g4		! save sp, ins for new window
	std	%i0, [%sp +(8*4)]	! normal stack save area
	std	%i2, [%sp +(10*4)]
	std	%i4, [%sp +(12*4)]
	std	%i6, [%sp +(14*4)]
	mov	%g3, %psr		! old psr, disable traps, CWP, PSR_CC
	mov	0x4, %g1		! psr delay, compute mask for CWP + 2
	sll	%g1, %g3, %g1		! psr delay, won't work for NW == 32
	srl	%g1, %g5, %g2		! psr delay
	or	%g1, %g2, %g1
	mov	%g1, %wim		! install new wim
	mov	%g4, %sp		! reestablish sp
	ldd	[%sp + (8*4)], %i0	! reestablish ins
	ldd	[%sp + (10*4)], %i2
	ldd	[%sp + (12*4)], %i4
	ldd	[%sp + (14*4)], %i6
	restore				! restore return window
	RESTORE_WINDOW(%sp)
	b	.sr_out
	save

.sr_samecwp:
	!
	! There is no CWP change.
	! We must make sure that there is a window to return to.
	!
	mov	0x2, %g1		! compute mask for CWP + 1
	sll	%g1, %l0, %g1		! XXX won't work for NW == 32
	srl	%g1, %g5, %g2		! %g5 == NW, from above
	or	%g1, %g2, %g1
	mov	%wim, %g2		! cmp with wim to check for underflow
	btst	%g1, %g2
	bz	.sr_out
	nop
	!
	! No window to return to. Restore it.
	!
	sll	%g2, 1, %g1		! compute new WIM = rol(WIM, 1, NW)
	dec	%g5			! %g5 == NW-1
	srl	%g2, %g5, %g2
	or	%g1, %g2, %g1
	mov	%g1, %wim		! install it
	nop; nop; nop;			! wim delay
	restore				! get into window to be restored
	RESTORE_WINDOW(%sp)
	save				! get back to original window
	!
	! Check the PIL in the saved PSR.  Don't go below CPU->cpu_base_spl
	!
.sr_out:
	ld	[THREAD_REG + T_CPU], %g4
	and	%g3, PSR_PIL, %g1	! check saved PIL
	ld	[%g4 + CPU_BASE_SPL], %g4
	subcc	%g4, %g1, %g4		! form base - saved PIL
	bg,a	1f			! base PIL is greater so
	add	%g3, %g4, %g3		! fix PSR by adding (base - saved) PIL
1:
	mov	%g3, %psr		! restore PSR with correct PIL, CC

#ifdef TRACE
	!
	! We do this right before the _RESTORE_GLOBALS, so we can safely
	! use the globals as scratch registers.
	! On entry, %g3 = %psr.
	!
	TRACE_SYS_TRAP_END(%g1, %g2, %g6, %g4, %g5)
	mov	%g3, %psr
#endif  /* TRACE */

	RESTORE_OUTS(%sp + MINFRAME)	! restore system outs
	RESTORE_GLOBALS(%sp + MINFRAME) ! restore system globals
	ld	[%sp + MINFRAME + PC*4], %l1 ! delay slot, restore sys pc
	ld	[%sp + MINFRAME + nPC*4], %l2 ! sys npc
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l1			! return to trapped instruction
	rett	%l2			! delay slot
	SET_SIZE(_sys_rtt)

#if defined(lint) || defined(__lint)

void
lock_trouble(void)
{}

#else	/* lint */
	ENTRY_NP(lock_trouble)
	wr	%l0, PSR_ET, %psr	! enable traps (no priority change)
	! clear the lock because the panic code may use the hat
	! layer (start_mon_clock -> write_scb_int ...)
	set	1f, %o0
	call	panic
	nop
1:
	.asciz	"lock not released in fault handler"
	.align	4
	SET_SIZE(lock_trouble)
#endif	/* lint */

/*
 * Fault handler.
 */
#if defined(lint) || defined(__lint)

void
fault(void)
{}

#else	/* lint */
	ENTRY_NP(fault)
	!
	! The synchronous fault status register (SFSR) and
	! synchronous fault address register (SFAR) were obtained
	! above by calling mmu_getsyncflt, and have been stored
	! in the per-CPU area.
	!
	! For the call to trap below, move the status reg into
	! %o3, and the fault address into %o2
	!
	CPU_ADDR(%g2, %g4)
	ld	[%g2 + CPU_SYNCFLT_STATUS], %o3
	cmp     %l4, T_TEXT_FAULT       ! text fault?
	be,a    2f
	mov     %l1, %o2                ! use saved pc as the fault address
	ld	[%g2 + CPU_SYNCFLT_ADDR], %o2

2:	srl	%o3, FS_ATSHIFT, %g2	! Get the access type bits
	and	%g2, FS_ATMASK, %g2
	mov	S_READ, %o4		! assume read fault
	cmp	%g2, AT_UREAD
	be	3f
	nop
	cmp	%g2, AT_SREAD
	be	3f
	nop
	mov	S_WRITE, %o4		! else it's a write fault
	cmp	%g2, AT_UWRITE
	be	3f
	nop
	cmp	%g2, AT_SWRITE
	be	3f
	nop

	!
	! Must be an execute cycle
	!
	mov	S_EXEC, %o4		! it's an execution fault
3:
	wr	%l0, PSR_ET, %psr	! enable traps (no priority change)
	nop; nop;
	/*
	 * %o0, %o1 are setup by SYS_TRAP,
	 * %o2 from fav,
	 * %o3 from fsr, and
	 * %o4 from looking at fsr
	 */
	mov	%l4, %o0		! trap(t, rp, addr, fsr, rw)
	call	trap			! C Trap handler
	add	%sp, MINFRAME, %o1
	b,a	_sys_rtt		! return from trap
	SET_SIZE(fault)
#endif	/* lint */

/*
 * Interrupt vector table
 *	all interrupts are vectored via the following table
 *	can't vector directly from scb because window setup hasn't been done
 */
	.section	".data"
	.align	4
	.global _int_vector
_int_vector:
	.word	spurious	! level 0
	.word	intr_level_1	! level 1
	.word	intr_level_2	! level 2,	sbus-1
	.word	intr_level_3	! level 3,	sbus-2
	.word	intr_level_4	! level 4
	.word	intr_level_5	! level 5,	sbus-3
	.word	intr_level_6	! level 6
	.word	intr_level_7	! level 7,	sbus-4
	.word	intr_level_8	! level 8
	.word	intr_level_9	! level 9,	sbus-5
	.word	intr_level_10	! level 10,	cpu_unit: tick timer
	.word	intr_level_11	! level 11,	sbus-6
	.word	intr_level_12	! level 12,	cpu_unit: uart(s)
	.word	intr_level_13	! level 13,	sbus-7
	.word	intr_level_14	! level 14,	cpu_unit: profile timer
	.word	intr_level_15	! level 15,	asynchronous error

	.section	".text"

#endif	/* lint */

/*
 * Generic interrupt handler.
 *	Entry:	traps disabled.
 *		%l4 = T_INTERRUPT ORed with level (1-15)
 *		%l0, %l1, %l2 = saved %psr, %pc, %npc.
 *		%l7 = saved %sp
 *	Uses:
 *		%l4 = interrupt level (1-15).
 *		%l5 = old PSR with PIL cleared 
 *		%l6 = new thread pointer
 */

#if defined(lint) || defined(__lint)

void
_interrupt(void)
{}

#else	/* lint */

	ENTRY_NP(_interrupt)
#if	defined(DEBUG) && defined(COUNT_INTRS)
	set	int_count, %o7
	and	%l4, T_INT_LEVEL, %o0
	sll	%o0, 2, %o0
	add	%o7, %o0, %o7
	ld	[%o7], %o0
	inc	%o0
	st	%o0, [%o7]
#endif	/* DEBUG && COUNT_INTRS */

	call	intr_get_pend_local	! remember: traps are disabled.
	nop				! delay slot
#ifdef DEBUG
	tst	%o0			! got any?
	! bz	.interrupt_ret		! no, the butler did it?
	bnz	1f			! yes, the butler did it?
	nop				! delay slot
	set     int_count, %o7
	ld      [%o7], %o0
	inc     %o0			! int_count[0]++ (spurious)
	st      %o0, [%o7]
1:
#endif	DEBUG
	call	intr_clear_pend_local		! stop pestering me
	and	%l4, T_INT_LEVEL, %o0	! delay slot - sparc level

	andn	%l0, PSR_PIL, %l5	! compute new psr with proper PIL
	and	%l4, T_INT_LEVEL, %l4
	sll	%l4, PSR_PIL_BIT, %g1
	or	%l5, %g1, %l0

	sethi	%hi(panicstr), %l6	! test for panic
	ld	[%l6 + %lo(panicstr)], %l6
	tst	%l6
	bnz,a	1f			! if NULL, test for lock level
	or	%l0, PSR_PIL, %l0	! delay - mask everthing if panicing
1:
	cmp	%l4, LOCK_LEVEL		! compare to highest thread level
	ble	intr_thread		! process as a separate thread
	ld	[THREAD_REG + T_CPU], %l3	! delay - get CPU pointer
	ld	[%l3 + CPU_ON_INTR], %l6	! load cpu_on_intr

	!
	! Handle high_priority nested interrupt on separate interrupt stack
	!
	tst	%l6			! hi-priority, anybody else here?
	inc	%l6
	bnz	1f			! already on the stack 
	st	%l6, [%l3 + CPU_ON_INTR]
	ld	[%l3 + CPU_INTR_STACK], %sp	! %sp = cpu->intr_stack
	tst	%sp			! if INTR_STACK not initialized,
	bz,a	1f			! .. stay on current stack
	mov	%l7, %sp		! (can happen; eg, early level 15)
1:
	cmp	%g1, PSR_PIL		! level-15?
	bne	2f
	sll	%l4, 2, %l3		! convert level to word offset

	/*
	 * SPARC Interrupt Request Level (IRL) 15 is a Non-Maskable-Interrupt.
	 * ie. even with PSR.PIL set to 15, you'll still get level15's.
	 *
	 * The IRL pins are level sensitive, and they'll keep an interrupt
	 * pending as long as the CC Interrupt Pending Register (IPR)
	 * drives them.
	 *
	 * Typically, we end up in level15 because of isolated ECC errors.
	 * intr_clear_pend_local() above will have removed it from the IPR,
	 * the IRL is now < 15, and we could safely set PSR.ET and vector to
	 * the C handler.
	 *
	 * However, there are three reasons we want to run leve15
	 * with level 15 interrupts masked.  So we reach into the
	 * MXCC interrupt controller and mask out level15's in the
	 * Interrupt Mask Register (IMR).
	 *
	 * 1.	If the MXCC detects an Ecache parity error, or
	 *	a late error on an asynchronous data store, then we get
	 *	a "Local Level-15 Interrupt".  In this case, the MXCC will
	 *	continue driving the IRL until we clear the source of the
	 *	error in the MXCC Error Register.
	 *
	 *	The l15 in the IPR will go away when we clear the
	 * 	MXCC error register.
	 *
	 * 2.	The BootBus critical errors for fan failure and
	 *	temperrature failure are level sensitive.
	 *	They will cause a level15 until they are masked 
	 *	in the BootBus Control Register, or the sensor
	 *	decides that the condition has gone away.
	 *
	 *	The l15 in the IPR will go away when we mask
	 *	the failure in the BootBus control register.
	 *
	 * 3.	The interrupt handler itself could encouter an ECC error.
	 */

	call	intr_set_mask_bits	! mask interrupt (uses %o0 %o1 %o2)
	set	(1 << 15), %o0		! delay slot -- level 15

	/*
	 * Get vector for high-priority level.
	 */
2:
	mov	%l0, %psr		! set level (IU bug)
	wr	%l0, PSR_ET, %psr	! enable traps
	set	_int_vector, %g1
	ld	[%g1 + %l3], %l3	! grab vector

	TRACE_ASM_1 (%o2, TR_FAC_INTR, TR_INTR_START, TR_intr_start, %l4);

	jmp	%l3			! interrupt handler
	nop				! delay slot

.interrupt_ret:
	!
	! Decide here whether we're returning as an interrupt thread or not.
	! All interrupts below LOCK_LEVEL are handled by interrupt threads.
	!
	cmp	%l4, LOCK_LEVEL
	ble	int_rtt			! slow-int, *is* interrupt thread
	nop				! delay slot

	!
	! On interrupt stack.  These handlers cannot jump to int_rtt
	! (as they used to), they must now return as normal functions
	! or jump to .intr_ret.
	!
.intr_ret:
	TRACE_ASM_0 (%o1, TR_FAC_INTR, TR_INTR_END, TR_intr_end);
	ld	[THREAD_REG + T_CPU], %l3 	! reload CPU pointer
	ld	[%l3 + CPU_SYSINFO_INTR], %g2
	inc	%g2				! cpu_sysinfo.intr++
	st	%g2, [%l3 + CPU_SYSINFO_INTR]

	!
	! Disable interrupts while changing %sp and cpu_on_intr
	! so any subsequent intrs get a precise state.
	!
	mov	%psr, %g2
	wr	%g2, PSR_ET, %psr		! disable traps
	nop
	ld	[%l3 + CPU_ON_INTR], %l6	! decrement on_intr
	dec	%l6				! psr delay 3
	st	%l6, [%l3 + CPU_ON_INTR]	! store new on_intr
	mov	%l7, %sp			! reset stack pointer
	mov	%g2, %psr			! enable traps
	nop
	b	_sys_rtt			! return from trap
	nop					! psr delay 3
	SET_SIZE(_interrupt)

/*
 * Handle an interrupt in a new thread.
 *	Entry:	traps disabled.
 *		%l3 = CPU pointer
 *		%l4 = interrupt level
 *		%l0 = old psr with new PIL
 *		%l1, %l2 = saved %psr, %pc, %npc.
 *		%l7 = saved %sp
 *	Uses:
 *		%l4 = interrupt level (1-15).
 *		%l5 = old PSR with PIL cleared
 *              %l6 = new thread pointer, indicates if interrupt level
 *			needs to be unmasked after invoking the trap handler.
 */
	ENTRY_NP(intr_thread)
	!
	! Get set to run interrupt thread.
	! There should always be an interrupt thread since we allocate one
	! for each level on the CPU, and if we release an interrupt, a new
	! thread gets created.
	!
	ld	[%l3 + CPU_INTR_THREAD], %l6	! interrupt thread pool
#ifdef DEBUG
	tst	%l6
	bnz	2f
	nop
	set	1f, %o0
	mov	%l0, %psr		! set level (IU bug)
	wr	%l0, PSR_ET, %psr	! enable traps
	nop
	call	panic
	nop
1:	.asciz	"no interrupt thread"
	.align	4
2:
#endif	DEBUG

	ld	[%l6 + T_LINK], %o2		! unlink thread from CPU's list
	st	%o2, [%l3 + CPU_INTR_THREAD]	! cpu.intr_thread = link;

	!
	! Consider the new thread part of the same LWP so that
	! window overflow code can find the PCB.
	!
	ld	[THREAD_REG + T_LWP], %o2
	st	%o2, [%l6 + T_LWP]

	!
	! Threads on the interrupt thread free list could have state already
	! set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	! Could eliminate the next two instructions with a little work.
	!
	mov	ONPROC_THREAD, %o3
	st	%o3, [%l6 + T_STATE]

	!
	! Push interrupted thread onto list from new thread.
	! Set the new thread as the current one.
	! Set interrupted thread's T_SP because if it is the idle thread,
	! resume may use that stack between threads.
	!
	st	%l7, [THREAD_REG + T_SP]	! mark stack for resume
	st	THREAD_REG, [%l6 + T_INTR]	! push old thread
	st	%l6, [%l3 + CPU_THREAD]		! set new thread
	mov	%l6, THREAD_REG			! set global curthread register
	ld	[%l6 + T_STACK], %sp		! interrupt stack pointer

	!
	! get CPU_ID while %l3 is still valid. this will be used
	! in itr_round_robin() later.
	!
	ld	[%l3 + CPU_ID], %o0
	!
	! Initialize thread priority level from intr_pri
	!
	sethi   %hi(intr_pri), %g1
	ldsh	[%g1 + %lo(intr_pri)], %l3	! grab base interrupt priority
	add	%l4, %l3, %l3		! convert level to dispatch priority
	sth	%l3, [THREAD_REG + T_PRI]
	stub	%l4, [THREAD_REG + T_PIL]	! store pil

	sethi	%hi(do_robin), %o3	! see if round robin mode is on
	ld	[%o3 + %lo(do_robin)], %o3
	tst	%o3
	bz	.st_flyingleap
	nop

	call	itr_round_robin		! do round-robin itr assignment 
	nop

.st_flyingleap:
	!
	! Get handler address for level.
	!
	sll	%l4, 2, %l3		! convert level to word offset
	set	_int_vector, %g1
	ld	[%g1 + %l3], %l3	! grab vector
#ifdef	TRAPTRACE
	!
	! make trace entry - helps in debugging watchdogs
	!
	TRACE_PTR(%g1, %g2)		! get trace pointer
	CPU_INDEX(%o1)
	sll	%o1, 2, %o1
	set	_trap_last_intr, %g2
	st	%g1, [%g2 + %o1]	! save last interrupt trace record
	mov	-1, %g2
	st	%g2, [%g1 + TRAP_ENT_TBR]
	st	%l0, [%g1 + TRAP_ENT_PSR]
	st	%l1, [%g1 + TRAP_ENT_PC]
	st	%sp, [%g1 + TRAP_ENT_SP]
	st	%g7, [%g1 + TRAP_ENT_G7]
	mov	%psr, %g2
	st	%g2, [%g1 + 0x14]
	mov	%tbr, %g2
	st	%g2, [%g1 + 0x18]
	TRACE_NEXT(%g1, %o1, %g2)	! set new trace pointer
#endif	TRAPTRACE

	mov	%l0, %psr		! set level (IU bug)
	wr	%l0, PSR_ET, %psr	! enable traps
	nop				! psr delay (jre/bj)
	TRACE_ASM_1 (%o2, TR_FAC_INTR, TR_INTR_START, TR_intr_start, %l4);
	call	%l3			! interrupt handler
	nop				! delay slot, next instr can be "save"
	SET_SIZE(intr_thread)

	!
	! return from interrupt - this is return from call above or
	! just jumpped to.
	!
	! %l4, %l6 and %l7 must remain unchanged since they contain
	! information required after returning from the trap handler.
	!
	! Note that intr_passivate(), below, relies on values in %l4 and %l7.
	!
	ENTRY_NP(int_rtt)
#ifdef	TRAPTRACE
	!
	! make trace entry - helps in debugging watchdogs
	!
	mov	%psr, %g1
	wr	%g1, PSR_ET, %psr	! disable traps
	TRACE_PTR(%l5, %g2)		! get trace pointer
	CPU_INDEX(%o1)
	sll	%o1, 2, %o1		! get index to trap_last_intr2
	set	_trap_last_intr2, %g2
	st	%l5, [%g2 + %o1]	! save last interrupt trace record
	mov	-2, %g2			! interrupt return
	st	%g2, [%l5 + TRAP_ENT_TBR]
	st	%g1, [%l5 + TRAP_ENT_PSR]
	clr	[%l5 + TRAP_ENT_PC]
	st	%sp, [%l5 + TRAP_ENT_SP]
	st	%g7, [%l5 + TRAP_ENT_G7]
	st      %l7, [%l5 + 0x14]       ! saved %sp
	clr	[%l5 + 0x18]
	ld	[THREAD_REG + T_INTR], %g2
	st	%g2, [%l5 + 0x1c]	! thread underneath
	TRACE_NEXT(%l5, %o1, %g2)	! set new trace pointer
	wr	%g1, %psr		! enable traps
#endif	TRAPTRACE

	TRACE_ASM_0 (%o1, TR_FAC_INTR, TR_INTR_END, TR_intr_end);

	ld	[THREAD_REG + T_CPU], %g1	! get CPU pointer
	ld	[%g1 + CPU_SYSINFO_INTR], %g2
	inc	%g2				! cpu_sysinfo.intr++
	st	%g2, [%g1 + CPU_SYSINFO_INTR]
	ld	[%g1 + CPU_SYSINFO_INTRTHREAD], %g2	! cpu_sysinfo.intrthread++
	inc	%g2
	st	%g2, [%g1 + CPU_SYSINFO_INTRTHREAD]

	!
	! block interrupts to protect the interrupt thread pool.
	! XXX - May be able to avoid this by assigning a thread per level.
	!
	mov	%psr, %l5
	andn	%l5, PSR_PIL, %l5	! mask out old PIL
	or	%l5, LOCK_LEVEL << PSR_PIL_BIT, %l5
	mov	%l5, %psr		! mask all interrupts
	nop; nop			! psr delay
	!
	! If there is still an interrupted thread underneath this one,
	! then the interrupt was never blocked or released and the
	! return is fairly simple.  Otherwise jump to intr_thread_exit.
	!
	ld	[THREAD_REG + T_INTR], %g2	! psr delay
	nop					! psr delay (tst sets cc's)
	tst	%g2				! psr delay
	bz	intr_thread_exit		! no interrupted thread
	ld	[THREAD_REG + T_CPU], %g1	! delay - load CPU pointer

	!
	! link the thread back onto the interrupt thread pool
	!
	ld	[%g1 + CPU_INTR_THREAD], %l6
	st	%l6, [THREAD_REG + T_LINK]
	st	THREAD_REG, [%g1 + CPU_INTR_THREAD]

	!
	!	Set the thread state to free so kadb doesn't see it
	!
	mov	FREE_THREAD, %l6
	st	%l6, [THREAD_REG + T_STATE]

	!
	! Switch back to the interrupted thread
	!
	st	%g2, [%g1 + CPU_THREAD]
	mov	%g2, THREAD_REG
	ldstub	[%g2 + T_LOCK_FLUSH], %g0 ! synchronize with mutex_exit()

	b	_sys_rtt		! restore previous stack pointer
	mov	%l7, %sp		! restore %sp
	SET_SIZE(int_rtt)

	!
	! An interrupt returned on what was once (and still might be)
	! an interrupt thread stack, but the interrupted process is no longer
	! there.  This means the interrupt must've blocked or called
	! release_interrupt().
	!
	! There is no longer a thread under this one, so put this thread back
	! on the CPU's free list and resume the idle thread which will dispatch
	! the next thread to run.
	!
	! All interrupts are disabled here (except machine checks), but traps
	! are enabled.
	!
	! Entry:
	!	%g1 = CPU pointer.
	!	%l4 = interrupt level (1-15)
	!
	ENTRY_NP(intr_thread_exit)
	TRACE_ASM_0 (%o1, TR_FAC_INTR, TR_INTR_EXIT, TR_intr_exit);
        ld      [%g1 + CPU_SYSINFO_INTRBLK], %g2   ! cpu_sysinfo.intrblk++
        inc     %g2
        st      %g2, [%g1 + CPU_SYSINFO_INTRBLK]
#ifdef TRAPTRACE
	mov	%psr, %l5
	andn	%l5, PSR_ET, %o4	! disable traps
	mov	%o4, %psr
	nop; nop; nop;
	TRACE_PTR(%o4, %o5)		! get trace pointer
	set	0x8888, %o5
	st	%o5, [%o4 + TRAP_ENT_TBR]
	st	%l0, [%o4 + TRAP_ENT_PSR]
	st	%l1, [%o4 + TRAP_ENT_PC]
	st	%sp, [%o4 + TRAP_ENT_SP]
	st	%g7, [%o4 + TRAP_ENT_G7]
	st	%l4, [%o4 + 0x14]
	ld	[%g7 + T_CPU], %o5
	ld	[%o5 + CPU_BASE_SPL], %o5
	st	%o5, [%o4 + 0x18]
	TRACE_NEXT(%o4, %l6, %o5)	! set new trace pointer
	mov	%l5, %psr
	nop; nop; nop;
#endif
	!
	! Put thread back on the either the interrupt thread list if it is still
	! an interrupt thread, or the CPU's free thread list, if it did a
	! release interrupt.
	!
	lduh	[THREAD_REG + T_FLAGS], %l5
	btst	T_INTR_THREAD, %l5		! still an interrupt thread?
	bz	1f				! No, so put back on free list
	mov	1, %o4				! delay

	!
	! This was an interrupt thread, so clear the pending interrupt flag
	! for this level.
	!
	ld	[%g1 + CPU_INTR_ACTV], %o5	! get mask of interrupts active
	sll	%o4, %l4, %o4			! form mask for level
	andn	%o5, %o4, %o5			! clear interrupt flag
	call	.intr_set_spl			! set CPU's base SPL level
	st	%o5, [%g1 + CPU_INTR_ACTV]	! delay - store active mask

	!
	! Set the thread state to free so kadb doesn't see it
	!
	mov	FREE_THREAD, %l6
	st	%l6, [THREAD_REG + T_STATE]

	!
	! Put thread on either the interrupt pool or the free pool and
	! call swtch() to resume another thread.
	!
	ld	[%g1 + CPU_INTR_THREAD], %l4	! get list pointer
	st	%l4, [THREAD_REG + T_LINK]
	call	swtch				! switch to best thread
	st	THREAD_REG, [%g1 + CPU_INTR_THREAD] ! delay - put thread on list

	/*
	 * call die(...)?
	 */
	b,a	.				! swtch() shouldn't return
1:
	mov	TS_ZOMB, %l6			! set zombie so swtch will free
	call	swtch				! run next process - free thread
	st	%l6, [THREAD_REG + T_STATE]	! delay - set state to zombie

	SET_SIZE(intr_thread_exit)

#endif	/* lint */

/*
 * Set CPU's base SPL level, based on which interrupt levels are active.
 *	Called at spl7 or above.
 */

#if defined(lint) || defined(__lint)

void
set_base_spl(void)
{}

#else	/* lint */

	ENTRY_NP(set_base_spl)
	save	%sp, -SA(MINFRAME), %sp		! get a new window
	ld	[THREAD_REG + T_CPU], %g1	! load CPU pointer
	call	.intr_set_spl			! real work done there
	ld	[%g1 + CPU_INTR_ACTV], %o5	! load active interrupts mask
	ret
	restore

/*
 * WARNING: non-standard callinq sequence; do not call from C
 *	%g1 = pointer to CPU
 *	%o5 = updated CPU_INTR_ACTV
 *	uses %l6, %l3
 */
.intr_set_spl:					! intr_thread_exit enters here
	!
	! Determine highest interrupt level active.  Several could be blocked
	! at higher levels than this one, so must convert flags to a PIL
	! Normally nothing will be blocked, so test this first.
	!
	tst	%o5
	bz	2f				! nothing active
	sra	%o5, 11, %l6			! delay - set %l6 to bits 15-11
	set	_intr_flag_table, %l3
	tst	%l6				! see if any of the bits set
	ldub	[%l3 + %l6], %l6		! load bit number
	bnz,a	1f				! yes, add 10 and we're done
	add	%l6, 11-1, %l6			! delay - add bit number - 1

	sra	%o5, 6, %l6			! test bits 10-6
	tst	%l6
	ldub	[%l3 + %l6], %l6
	bnz,a	1f
	add	%l6, 6-1, %l6

	sra	%o5, 1, %l6			! test bits 5-1
	ldub	[%l3 + %l6], %l6
	!
	! highest interrupt level number active is in %l6
	!
1:
	sll	%l6, PSR_PIL_BIT, %o5		! move PIL into position
2:
	retl
	st	%o5, [%g1 + CPU_BASE_SPL]	! delay - store base priority
	SET_SIZE(set_base_spl)

/*
 * Table that finds the most significant bit set in a five bit field.
 * Each entry is the high-order bit number + 1 of it's index in the table.
 * This read-only data is in the text segment.
 */
_intr_flag_table:
	.byte	0, 1, 2, 2,	3, 3, 3, 3,	4, 4, 4, 4,	4, 4, 4, 4
	.byte	5, 5, 5, 5,	5, 5, 5, 5,	5, 5, 5, 5,	5, 5, 5, 5
	.align	4

#endif	/* lint */

/*
 * int
 * intr_passivate(from, to)
 *	kthread_id_t	from;		interrupt thread
 *	kthread_id_t	to;		interrupted thread
 *
 * Gather state out of partially passivated interrupt thread.
 * Caller has done a flush_windows();
 *
 * RELIES ON REGISTER USAGE IN interrupt(), above.
 * In interrupt(), %l7 contained the pointer to the save area of the
 * interrupted thread.  Now the bottom of the interrupt thread should
 * contain the save area for that register window.
 *
 * Gets saved state from interrupt thread which belongs on the
 * stack of the interrupted thread.  Also returns interrupt level of
 * the thread.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
intr_passivate(kthread_id_t from, kthread_id_t to)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_passivate)
	save	%sp, -SA(MINFRAME), %sp ! get a new window
	call	flush_windows		! force register windows to stack
	!
	! restore registers from the base of the stack of the interrupt thread.
	!
	ld	[%i0 + T_STACK], %i2	! get stack save area pointer
	ldd	[%i2 + (0*4)], %l0	! load locals
	ldd	[%i2 + (2*4)], %l2
	ldd	[%i2 + (4*4)], %l4
	ldd	[%i2 + (6*4)], %l6
	ldd	[%i2 + (8*4)], %o0	! put ins from stack in outs
	ldd	[%i2 + (10*4)], %o2
	ldd	[%i2 + (12*4)], %o4
	ldd	[%i2 + (14*4)], %i4	! copy stack/pointer without using %sp
	!
	! put registers into the save area at the top of the interrupted
	! thread's stack, pointed to by %l7 in the save area just loaded.
	!
	std	%l0, [%l7 + (0*4)]	! save locals
	std	%l2, [%l7 + (2*4)]
	std	%l4, [%l7 + (4*4)]
	std	%l6, [%l7 + (6*4)]
	std	%o0, [%l7 + (8*4)]	! save ins using outs
	std	%o2, [%l7 + (10*4)]
	std	%o4, [%l7 + (12*4)]
	std	%i4, [%l7 + (14*4)]	! fp, %i7 copied using %i4

	set	_sys_rtt-8, %o3		! routine will continue in _sys_rtt.
	clr	[%i2 + ((8+6)*4)]	! clear frame pointer in save area
	st	%o3, [%i1 + T_PC]	! setup thread to be resumed.
	st	%l7, [%i1 + T_SP]
	ret
	restore %l4, 0, %o0		! return interrupt level from %l4
	SET_SIZE(intr_passivate)

#endif	/* lint */

/*
 * Return a thread's interrupt level.
 * Since this isn't saved anywhere but in %l4 on interrupt entry, we
 * must dig it out of the save area.
 *
 * Caller 'swears' that this really is an interrupt thread.
 *
 * int
 * intr_level(t)
 *	kthread_id_t	t;
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
intr_level(kthread_id_t t)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_level)
	save	%sp, -SA(MINFRAME), %sp ! get a new window
	call	flush_windows		! force register windows into stack
	ld	[%i0 + T_STACK], %o2	! get stack save area pointer
	ld	[%o2 + 4*4], %i0	! get interrupt level from %l4 on stack
	ret				! return
	restore
	SET_SIZE(intr_level)

#endif	/* lint */


/*
 * Spurious trap... 'should not happen'
 * %l4 - processor interrupt level
 */
#if defined(lint) || defined(__lint)

void
spurious(void)
{}

#else	/* lint */

	ENTRY_NP(spurious)
	set	1f, %o0
	call	printf
	mov	%l4, %o1
	b,a	.interrupt_ret

	.section	".data"
1:	.asciz	"spurious interrupt at processor level %d\n"
	.section	".text"
	SET_SIZE(spurious)
#endif	/* lint */

/*
 * SBINTR: SBus interrupt
 *	if no sbus int pending, drop out bottom
 *	if anyone on sbus list claims it, branch out
 *	set parameters for "spurious sbus level LEVEL" report
 */
#define	SBINTR(LEVEL)							\
	call	sbus_intr;		/* We can forget us now! */	\
	mov	LEVEL, %o0;		/* delay slot - sbus-level */	\
	b       .interrupt_ret;         /* all done! */                 \
	nop;                            /* delay slot */

#define	CPUINTR(LEVEL)							\
	call	cpu_intr;		/* do accumulated softcalls */	\
	mov	LEVEL, %o0;		/* delay slot */		\
	b	.interrupt_ret;		/* all done! */			\
	nop;				/* delay slot */

/*
 * there's a horrible race here that we just can't cure!
 * basically, zs_asm_intr has to clear the chip,
 * and then we have to clear the IPR - cause it's not latched anywhere.
 */
#define	BBINTR(LEVEL)							\
	call	slow_intr;		/* slowbus interrupt */		\
	mov	LEVEL, %o0;		/* delay slot */		\
	call	intr_clear_pend_local;	/* gotta clean up after bbc */	\
	mov	LEVEL, %o0;		/* delay slot */		\
	b	.interrupt_ret;		/* all done! */			\
	nop;				/* delay slot */

#if !defined(lint) && !defined(__lint)

/*
 * Level 1, cyclic_softint() and softint() processing 
 */
	ENTRY_NP(intr_level_1)
	call	cbe_fire_low
	nop
	CPUINTR(1)
	SET_SIZE(intr_level_1)

/*
 * Level 2, SBus level 1
 */
	ENTRY_NP(intr_level_2)
	SBINTR(1)
	SET_SIZE(intr_level_2)

/*
 * Level 3, SBus level 2
 */
	ENTRY_NP(intr_level_3)
	SBINTR(2)
	SET_SIZE(intr_level_3)

/*
 * Level 4, out of the blue?
 */
	ENTRY_NP(intr_level_4)
	CPUINTR(4)
	SET_SIZE(intr_level_4)

/*
 * Level 5, SBus level 3
 */
	ENTRY_NP(intr_level_5)
	SBINTR(3)
	SET_SIZE(intr_level_5)

/*
 * Level 6, out of the blue?
 */
	ENTRY_NP(intr_level_6)
	CPUINTR(6)
	SET_SIZE(intr_level_6)

/*
 * Level 7, from SBus level 4
 */
	ENTRY_NP(intr_level_7)
	SBINTR(4)
	SET_SIZE(intr_level_7)

/*
 * Level 8, out of the blue?
 */
	ENTRY_NP(intr_level_8)
	CPUINTR(8)
	SET_SIZE(intr_level_8)

/*
 * Level 9, from SBus level 5
 */
	ENTRY_NP(intr_level_9)
	SBINTR(5)
	SET_SIZE(intr_level_9)

/*
 * Level 10, cyclic_softint() processing
 */
	ENTRY_NP(intr_level_10)
	CPU_ADDR(%o0, %o1)		! load CPU struct addr to %o0 using %o1
	call	cyclic_softint
	mov	CY_LOCK_LEVEL, %o1
	b	.interrupt_ret
	nop
	SET_SIZE(intr_level_10)

/*
 * Level 11, from SBus level 6
 */
	ENTRY_NP(intr_level_11)
	SBINTR(6)
	SET_SIZE(intr_level_11)

/*
 * Level 12, from cpu_unit: uart
 */
	ENTRY_NP(intr_level_12)
	BBINTR(12)
	SET_SIZE(intr_level_12)

/*
 * Level 13, from SBus level 7 or from a poke_cpu()
 *
 * First, check for receipt of a poke_cpu() by examining the hardware
 * interrupt table. the code for doing this has detailed knowledge of
 * how poke_cpu() interrupts are sent, i.e. how the various bits in 
 * the INTSID portion of a sun4d interrupt message were formulated
 * on the sending side so that the appropriate location in the interrupt
 * table can be examined.
 */
	ENTRY_NP(intr_level_13)
	set	BW_LOCALBASE+INTR_TABLE, %o0
	lduha	[%o0]ASI_BW, %o1
	set	(1<<12), %o2
	andcc	%o1, %o2, %g0
	bz	1f
	nop
	set	BW_LOCALBASE+INTR_TBL_CLEAR, %o1
	stha	%o2, [%o1]ASI_BW
	set	(7<<3), %o2	/* Check if sbus level 7 is pending */
	or	%o0, %o2, %o0
	lduha	[%o0]ASI_BW, %o0
	tst	%o0
	bz	.interrupt_ret
	nop
1:
	SBINTR(7)
	SET_SIZE(intr_level_13)
#endif	/* lint */

#if defined(lint) || defined(__lint)

void
L14_front_end(void)
{}

#else	/* lint */
	ENTRY_NP(L14_front_end)
	!
	! If we're in the MON_CLK_EXCLUSIVE state, then we need to let the
	! monitor handle this interrupt.
	!
	sethi	%hi(mon_clock), %l4		! If obp is watching the keyboard,
	ldub	[%l4 + %lo(mon_clock)], %l4	! then this interrupt needs to be
	cmp	%l4, MON_CLK_EXCLUSIVE
	be	4f
	sethi	%hi(mon_clock_go), %l4

	!
	! We know that we're in the MON_CLK_SHARED or MON_CLK_DISABLED state.
	! If mon_clock_go is set, then we know that we're in a cyclic-triggered
	! level-14 softint; we'll clear mon_clock_go and branch to the PROM.
	!
	ldub	[%l4 + %lo(mon_clock_go)], %l5
	tst	%l5
	bz,a	5f
	mov	14, %l4				! delay - %l4 = PIL
	stub	%g0, [%l4 + %lo(mon_clock_go)]
4:
	mov	%l0, %psr			! restore psr for OBP
	nop					! 3 instr delay required
	b	mon_clock14_vec
	nop
5:
	CPU_ADDR(%l5, %l3)			! %l5 = CPU pointer
	btst	PSR_PS, %l0			! trap from supervisor mode?
	bz	interrupt_prologue		! no, go do the normal thing.
	st	%l0, [%l5 + CPU_PROFILE_PIL]	! delay - record interrupted PIL
	b	interrupt_prologue		! go do the normal thing.
	st	%l1, [%l5 + CPU_PROFILE_PC]	! delay - record interrupted PC 
	!
	! no return
	!
	SET_SIZE(L14_front_end)
#endif	/* lint */

/*
 * Level 14, from cpu_unit: profile timer
 */
#if !defined(lint) && !defined(__lint)

	ENTRY_NP(intr_level_14)
	call	poll_obp_mbox
	nop

	call	cbe_fire
	nop

	b	.interrupt_ret
	nop
	SET_SIZE(intr_level_14)

/*
 * Level 15
 */
	ENTRY_NP(intr_level_15)
	call	intr15_handler		!
	add	%l7, MINFRAME, %o0	! pass addr of save regs
	b	.interrupt_ret		! could use int_rtt here.
	nop				! (possible) delay slot
	SET_SIZE(intr_level_15)

#endif	/* lint */

/*
 * void
 * patch_win_stuff()
 *
 * Find the the number of implemented register windows.
 * The last byte of every trap vector is set to be equal
 * to the number of windows in the implementation minus one.
 *
 * XXX - Note that we don't go out and patch flush_windows. 
 * It's now assumed to run on 8 windows CPUs. Why bother
 * to do this patch_win_stuff anyway? flush_windows now already
 * assumes 8 windows!! maybe what should be done here is
 * to verify indeed the CPU has 8 windows.
 */

#if defined(lint) || defined(__lint)

void
patch_win_stuff(void)
{}

#else	/* lint */

	ENTRY_NP(patch_win_stuff)
	mov	%g0, %wim		! note psr has cwp = 0
	sethi	%hi(nwin_minus_one), %g4 ! initialize pointer to nwindows - 1

	save				! decrement cwp, wraparound to NW-1
	mov	%psr, %g1
	and	%g1, PSR_CWP, %g1	! we now have nwindows-1
	restore				! get back to orignal window
	mov	0x2, %wim		! reset initial wim

	st	%g1, [%g4 + %lo(nwin_minus_one)] ! initialize nwin_minus_one

	inc	%g1			! initialize the nwindows variable
	sethi	%hi(nwindows), %g4	! initialzie pointer to nwindows
	st	%g1, [%g4 + %lo(nwindows)] ! initialize nwindows

	!
	! Now calculate winmask.  0's set for each window.
	!
	dec	%g1
	mov	-2, %g2
	sll	%g2, %g1, %g2
	sethi	%hi(winmask), %g4
	st	%g2, [%g4 + %lo(winmask)]
	retl
	nop				! delay slot
	SET_SIZE(patch_win_stuff)

#endif	/* lint */

/*
 * Flush all windows to memory, except for the one we entered in.
 * We do this by doing NWINDOW-2 saves then the same number of restores.
 * This leaves the WIM immediately before window entered in.
 * This is used for context switching.
 *
 * This code now supports 8 windows.
 */

#if defined(lint) || defined(__lint)

void
flush_windows(void)
{}

#else	/* lint */

	ENTRY_NP(flush_windows)
	save	%sp, -WINDOWSIZE, %sp		! 1
	save	%sp, -WINDOWSIZE, %sp		! 2
	save	%sp, -WINDOWSIZE, %sp		! 3
	save	%sp, -WINDOWSIZE, %sp		! 4
	save	%sp, -WINDOWSIZE, %sp		! 5
	save	%sp, -WINDOWSIZE, %sp		! 6
	restore					! 6
	restore					! 5
	restore					! 4
	restore					! 3
	restore					! 2
	ret
	restore					! 1
	SET_SIZE(flush_windows)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
debug_flush_windows(void)
{}

#else	/* lint */

	/*
	 * This routine flushes one more window than
	 * flush_windows above. With this routine
	 * one can do setjmp()/debug_flush_windows()
	 * pair in a C-routine to flush out regs 
	 * so that a stack trace can be seen by a 
	 * debugger (e.g. kadb).
	 */
	ENTRY_NP(debug_flush_windows)
	save	%sp, -WINDOWSIZE, %sp		! 1
	save	%sp, -WINDOWSIZE, %sp		! 2
	save	%sp, -WINDOWSIZE, %sp		! 3
	save	%sp, -WINDOWSIZE, %sp		! 4
	save	%sp, -WINDOWSIZE, %sp		! 5
	save	%sp, -WINDOWSIZE, %sp		! 6
	save	%sp, -WINDOWSIZE, %sp		! 7

	restore					! 7
	restore					! 6
	restore					! 5
	restore					! 4
	restore					! 3
	restore					! 2
	ret
	restore					! 1
	SET_SIZE(debug_flush_windows)

#endif	/* lint */

/*
 * flush user windows to memory.
 * This is a leaf routine that only wipes %g1, %g2, and %g5.
 * Some callers may depend on this behavior.
 */

#if defined(lint) || defined(__lint)

void
flush_user_windows(void)
{}

#else	/* lint */

	ENTRY_NP(flush_user_windows)
	ld	[THREAD_REG + T_LWP], %g5
	tst	%g5			! t_lwp == 0 for kernel threads
	bz	3f			! return immediately when true
	nop
	ld	[%g5 + LWP_REGS], %g5
	sub	%g5, REGOFF, %g5	! mpcb
	ld	[%g5 + MPCB_UWM], %g1	! get user window mask
	tst	%g1			! do save until mask is zero
	bz	3f
	clr	%g2
1:
	save	%sp, -WINDOWSIZE, %sp
	ld	[%g5 + MPCB_UWM], %g1	! get user window mask
	tst	%g1			! do save until mask is zero
	bnz	1b
	add	%g2, 1, %g2
2:
	subcc	%g2, 1, %g2		! restore back to orig window
	bnz	2b
	restore
3:
	retl
	nop
	SET_SIZE(flush_user_windows)

#endif	/* lint */

/*
 * register allocation.
 *
 * on entry:
 *	%l0 = PSR
 *	%l1 = PC
 *	%l2 = nPC
 *	%l3 = WIM
 *
 * within routine:
 *	%g7 = NW -1
 *	%g5 = PSR
 *	%g2 = CWM 
 */

#if defined(lint) || defined(__lint)

void
fast_window_flush(void)
{}

#else	/* lint */

	ENTRY_NP(fast_window_flush)

	sethi   %hi(nwin_minus_one), %l6
	ld      [%l6+%lo(nwin_minus_one)], %l6

	mov	%g7, %l7		! save %g7 in %l7
	mov	%l6, %g7		! save NW-1 in %g7

	mov	1, %l5
	sll	%l5, %l0, %l4		! calculate CWM (1<<CWP)

	!
	! skip over the trap window by rotating the CWM left by 1
	!	
	srl	%l4, %g7, %l5		! shift CWM right by NW-1
	tst	%l5
	bz,a	.+8
	sll	%l4, 1, %l5		! shift CWM left by 1	
	mov	%l5, %l4

	!
	! check if there are any registers windows between the CWM
	! and the WIM that are left to be saved.
	!
	andcc	%l4, %l3, %g0		! WIM & CWM
	bnz,a	.ff_out_ret2		! branch if masks are equal
	mov	%l0, %psr		! restore psr to trap window

	!
	! get ready to save user windows by first saving globals
	! to the current window's locals (the trap window).
	!
	mov	%g5, %l5		! save %g5 in %l5
	mov	%l0, %g5		! save PSR in %g5
	mov	%g2, %l0		! save %g2 in %l0
	mov	%l4, %g2		! save CWM in %g2
	mov	%g3, %l3		! save %g3 in %l3
	mov	%g1, %l4		! save %g1 in %l4

	restore				! skip trap window, advance to
					! calling window
	!
	! flush user windows to the stack. this is an inlined version
	! of the window overflow code.
	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to save
	! the window onto the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the save won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the save actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	! Other sun4 trap handlers first probe for the stack, and
	! then, if the stack is present, they store to the stack.
	! This approach CANNOT be used with a multiprocessor system
	! because of a race condition: between the time that the
	! stack is probed, and the store to the stack is done, the
	! stack could be stolen by the page daemon.
	!

	set	RMMU_FSR_REG, %g1	! clear old faults from SFSR
	lda	[%g1]ASI_MOD, %g0


	set	RMMU_CTL_REG, %g1
	lda	[%g1]ASI_MOD, %g3	! turn on no-fault bit in
	or	%g3, MMCREG_NF, %g3	! mmu control register to
	sta	%g3, [%g1]ASI_MOD	! prevent taking a fault.
.ff_ustack_res:
	! Security Check to make sure that user is not
	! trying to save/restore unauthorized kernel pages
	! Check to see if we are touching non-user pages, if
	! so, fake up SFSR, SFAR to simulate an error
	set     KERNELBASE, %g1
	cmp     %g1, %sp
	bleu,a  .ff_failed
	mov	RMMU_CTL_REG, %g1	! turn off no-fault bit

	andcc   %sp, 7, %g0             ! test for misaligned sp
	bnz,a   .ff_failed
	mov     RMMU_CTL_REG, %g1       ! turn off no-fault bit

	!
	! advance to the next window to save. if the CWM is equal to
	! the WIM then there are no more windows left. terminate loop
	! and return back to the user.
	!
	std     %l0, [%sp + (0*4)]
	std     %l2, [%sp + (2*4)]
	std     %l4, [%sp + (4*4)]
	std     %l6, [%sp + (6*4)]
	std     %i0, [%sp + (8*4)]
	std     %i2, [%sp + (10*4)]
	std     %i4, [%sp + (12*4)]
	std     %i6, [%sp + (14*4)]

	set	RMMU_FAV_REG, %g3
	lda	[%g3]ASI_MOD, %g0	! read SFAR
	set	RMMU_FSR_REG, %g1
	lda	[%g1]ASI_MOD, %g1	! read SFSR

	btst	MMU_SFSR_FAV, %g1	! did a fault happen?
	bnz,a	.ff_failed		! terminate loop if fault happened
	mov	RMMU_CTL_REG, %g1	! turn off no-fault bit

	mov	%wim, %g1		! save WIM in %g1
	srl     %g2, %g7, %g3           ! shift CWM right by NW-1
	tst	%g3
	bz,a	.+8
	sll	%g2, 1, %g3		! shift CWM left by 1	
	mov	%g3, %g2
	andcc	%g2, %g1, %g0

	bz,a	.ff_ustack_res		! continue loop as long as CWM != WIM
	restore				! delay, advance window
.ff_out:

	set	RMMU_CTL_REG, %g1	! turn off no-fault bit
	lda	[%g1]ASI_MOD, %g3
	andn	%g3, MMCREG_NF, %g3
	sta	%g3, [%g1]ASI_MOD

	! restore CWP and reset WIM to clean windows position

	mov	1, %g3			
	sll	%g3, %g5, %g3		! calculate new WIM
	sub	%g7, 1, %g2		! put NW-2 in %g2
	srl	%g3, %g2, %g2		! mod(CWP+2, NW) == WIM	
	sll	%g3, 2, %g3
	wr	%g2, %g3, %wim		! install wim
	mov	%g5, %psr		! restore PSR	
.ff_out_ret:
	nop;nop;nop
	mov	%l7, %g7		! restore %g7
	mov	%l5, %g5		! restore %g5
	mov	%l3, %g3		! restore %g3
	mov	%l0, %g2		! restore %g2
	mov	%l4, %g1		! restore %g1
	jmp	%l2
	rett	%l2+4
.ff_out_ret2:
	nop;nop;nop
	mov	%l7, %g7		! restore %g7
	jmp	%l2
	rett	%l2+4
.ff_failed:
	!
	! The user's stack is not accessable. reset the WIM, and PSR to
	! the trap window. call sys_trap(T_FLUSH_WINDOWS) to do the dirty
	! work.
	!

	lda	[%g1]ASI_MOD, %g3	! turn off no-fault bit
	andn	%g3, MMCREG_NF, %g3
	sta	%g3, [%g1]ASI_MOD

	mov	%g5, %psr		! restore PSR back to trap window
	nop;nop;nop
	mov	%l7, %g7		! restore %g7
	mov	%l5, %g5		! restore %g5
	mov	%l3, %g3		! restore %g3
	mov	%l0, %g2		! restore %g2
	mov	%l4, %g1		! restore %g1
	SYS_TRAP(T_FLUSH_WINDOWS);
	SET_SIZE(fast_window_flush)

#endif	/* lint */

/*
 * Throw out any user windows in the register file.
 * Used by setregs (exec) to clean out old user.
 * Used by sigcleanup to remove extraneous windows when returning from a
 * signal.
 */

#if defined(lint) || defined(__lint)

void
trash_user_windows(void)
{}

#else	/* lint */

	ENTRY_NP(trash_user_windows)
	ld	[THREAD_REG + T_LWP], %g5
	ld	[%g5 + LWP_REGS], %g5
	sub	%g5, REGOFF, %g5	! mpcb
	ld	[%g5 + MPCB_UWM], %g1	! get user window mask
	tst	%g1
	bz	3f			! user windows?
	nop

	!
	! There are old user windows in the register file. We disable traps
	! and increment the WIM so that we don't overflow on these windows.
	! Also, this sets up a nice underflow when first returning to the
	! new user.
	!
	! using %g4 here doesn't interfere with fault info stored in %g4,
	! since this routine is not called during the time that the fault
	! info is valid.
	!
	mov	%psr, %g4
	or	%g4, PSR_PIL, %g1	! spl hi to prevent interrupts
	mov	%g1, %psr
	nop; nop; nop;

	ld	[%g5 + MPCB_UWM], %g1	! get user window mask
	clr	[%g5 + MPCB_UWM]		! throw user windows away

	sethi	%hi(nwin_minus_one), %g5
	b	2f
	ld	[%g5 + %lo(nwin_minus_one)], %g5
1:
	srl	%g2, 1, %g3		! next WIM = ror(WIM, 1, NW)
	sll	%g2, %g5, %g2		! %g5 == NW-1
	or	%g2, %g3, %g2
	mov	%g2, %wim		! install wim
	bclr	%g2, %g1		! clear bit from UWM
2:
	tst	%g1			! more user windows?
	bnz,a	1b
	mov	%wim, %g2		! get wim

	ld	[THREAD_REG + T_LWP], %g5
	ld	[%g5 + LWP_REGS], %g5
	sub	%g5, REGOFF, %g5	! mpcb
	clr	[%g5 + MPCB_WBCNT]	! zero window buffer cnt
	clr	[%g5 + MPCB_SWM]	! clear shared window mask
	clr	[%g5 + MPCB_RSP]
	clr	[%g5 + MPCB_RSP + 4]
	b	splx			! splx and return
	mov	%g4, %o0

3:
	clr	[%g5 + MPCB_SWM]	! clear shared window mask
	clr	[%g5 + MPCB_RSP]
	clr	[%g5 + MPCB_RSP + 4]
	retl
	clr	[%g5 + MPCB_WBCNT]	! zero window buffer cnt
	SET_SIZE(trash_user_windows)

/*
 * Clean out register file.
 */
	.type	.clean_windows, #function
.clean_windows:
	CPU_ADDR(%l5, %l4)		! load CPU struct addr to %l5 using %l4
	ld	[%l5 + CPU_THREAD], %l6	! load thread pointer
	mov	1, %l4
	stb	%l4, [%l6 + T_POST_SYS]	! so syscalls will clean windows
	ld	[%l5 + CPU_MPCB], %l6	! load mpcb pointer
	ld	[%l6 + MPCB_FLAGS], %l4	! set CLEAN_WINDOWS in pcb_flags
	mov	%wim, %l3
	bset	CLEAN_WINDOWS, %l4
	st	%l4, [%l6 + MPCB_FLAGS]
	srl	%l3, %l0, %l3		! test WIM bit
	btst	1, %l3
	bnz,a	.cw_out			! invalid window, just return
	mov	%l0, %psr		! restore PSR_CC

	mov	%g1, %l5		! save some globals
	mov	%g2, %l6
	mov	%g3, %l7
	mov	%wim, %g2		! put wim in global
	mov	0, %wim			! zero wim to allow saving
	mov	%l0, %g3		! put original psr in global
	b	2f			! test next window for invalid
	save


	!
	! Loop through windows past the trap window
	! clearing them until we hit the invlaid window.
	!
1:
	clr	%l1			! clear the window
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o6
	clr	%o7
	save
2:
	mov	%psr, %g1		! get CWP
	srl	%g2, %g1, %g1		! test WIM bit
	btst	1, %g1
	bz,a	1b			! not invalid window yet
	clr	%l0			! clear the window

	!
	! Clean up trap window.
	!
	mov	%g3, %psr		! back to trap window, restore PSR_CC
	mov	%g2, %wim		! restore wim
	nop; nop;			! psr delay
	mov	%l5, %g1		! restore globals
	mov	%l6, %g2
	mov	%l7, %g3
	mov	%l2, %o6		! put npc in unobtrusive place
	clr	%l0			! clear the rest of the window
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o7
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%o6			! return to npc
	rett	%o6 + 4

.cw_out:
	nop
	nop				! psr delay
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l2			! return to npc
	rett	%l2 + 4
	SET_SIZE(.clean_windows)

#endif	/* lint */

#ifdef XX_NOTUSED
/*
 * Enter the monitor -- called from console abort
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
montrap(void (*func)(void))
{}

#else	/* lint */

	ENTRY_NP(montrap)
	save	%sp, -SA(MINFRAME), %sp ! get a new window
	call	flush_windows		! flush windows to stack
	nop
	call	%i0			! go to monitor
	nop
	ret
	restore
	SET_SIZE(montrap)

#endif	/* lint */
#endif XX_NOTUSED

/*
 *	Setup g7 via the CPU data structure
 */
#if defined(lint) || defined(__lint)

struct scb *
set_tbr(struct scb *s)
{ return (s); }

#else	/* lint */

	ENTRY_NP(set_tbr)
	mov	%psr, %o1
	or	%o1, PSR_PIL, %o2
	mov	%o2, %psr
	nop
	nop
	mov	%tbr, %o4
	mov	%o0, %tbr
	nop
	nop
	nop
	mov	%o1, %psr	! restore psr
	nop
	nop
	retl
	mov	%o4, %o0	! return value = old tbr
	SET_SIZE(set_tbr)

#endif	/* lint */

/*
 * void
 * reestablish_curthread(void)
 *    - reestablishes the invariant that THREAD_REG contains
 *      the same value as the cpu struct for this cpu (implicit from
 *      where we're running). This is needed for OBP callback routines.
 */

#if defined(lint) || defined(__lint)

void
reestablish_curthread(void)
{}

void
getcc(void)
{}

void
setcc(void)
{}

void
fix_alignment(void)
{}

#else	/* lint */

	ENTRY_NP(reestablish_curthread)
	CPU_ADDR(%o0, %o1)
	retl
	ld	[%o0 + CPU_THREAD], THREAD_REG
	SET_SIZE(reestablish_curthread)

/*
 * return the condition codes in %g1
 * Note: this routine is using the trap window.
 */
	.type	.getcc, #function
.getcc:
	sll	%l0, 8, %g1		! right justify condition code
	srl	%g1, 28, %g1
1:
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l2			! return, skip trap instruction
	rett	%l2 + 4			! delay slot
	SET_SIZE(.getcc)

/*
 * set the condtion codes from the value in %g1
 * Note: this routine is using the trap window.
 */
	.type	.setcc, #function
.setcc:
	sll	%g1, 20, %l5		! mov icc bits to their position in psr
	set	PSR_ICC, %l4		! condition code mask
	andn	%l0, %l4, %l0		! zero the current bits in the psr
	or	%l5, %l0, %l0		! or new icc bits
	mov	%l0, %psr		! write new psr
	nop; nop; nop;			! psr delay
	jmp	%l2			! return, skip trap instruction
	rett	%l2 + 4
	SET_SIZE(.setcc)

/*
 * some user has to do unaligned references, yuk!
 * set a flag in the proc so that when alignment traps happen
 * we fix it up instead of killing the user
 * Note: this routine is using the trap window
 */
	.type	.fix_alignment, #function
.fix_alignment:
	CPU_ADDR(%l5, %l4)		! load CPU struct addr to %l5 using %l4
	ld	[%l5 + CPU_THREAD], %l5	! load thread pointer
	ld	[%l5 + T_PROCP], %l5
	mov	1, %l4
	stb	%l4, [%l5 + P_FIXALIGNMENT]
	jmp	%l2			! return, skip trap instruction
	rett	%l2 + 4
	SET_SIZE(.fix_alignment)

#endif	/* lint */

/*
 * Return the current THREAD pointer.
 * This is also available as an inline function.
 */
#if defined(lint) || defined(__lint)

kthread_id_t
threadp(void)
{ return ((kthread_id_t)0); }

trapvec mon_clock14_vec;
trapvec kclock14_vec;
trapvec kadb_tcode;
trapvec trap_kadb_tcode;
trapvec trap_monbpt_tcode;

#else	/* lint */

	ENTRY_NP(threadp)
	retl
	mov	THREAD_REG, %o0
	SET_SIZE(threadp)
#endif	/* lint */

#define	PSR_DISABLE	0xA6

/*
 * This code supports a 'traps disable' operation.
 * Calling this 'trap_psr_et' is a lousy idea, but it's an operation on 'et'.
 *
 * .trap_psr_et
 *	%l0 = psr
 *	%l1 = pc (ta PSR_DISABLE instruction)
 *	%l2 = npc
 */
#if defined(lint) || defined(__lint)

void
trap_psr_et(void)
{}

#else	/* lint */

	ENTRY_NP(.trap_psr_et)
	andcc	%l0, PSR_PS, %g0
	bz	sys_trap		! from user mode, shame on you!
	mov	PSR_DISABLE | T_SOFTWARE_TRAP, %l4	! delay slot
	jmp	%l2			! instruction after 'ta PSR_DISABLE'
	restore				! leave with ET = 0 on psr!!
	SET_SIZE(.trap_psr_et)

/*
 * The level 14 interrupt vector can be handled by both the
 * kernel and the monitor.  The monitor version is copied here
 * very early before the kernel sets the tbr.  The kernel copies its
 * own version a little later in mlsetup.  They are write proteced
 * later on in kvm_init() when the the kernels text is made read only.
 * The monitor's vector is installed when we call the monitor and
 * early in system booting before the kernel has set up its own
 * interrupts, oterwise the kernel's vector is installed.
 */
	.align	8
	.global mon_clock14_vec, kclock14_vec
mon_clock14_vec:
	SYS_TRAP(0x1e)			! gets overlaid.
	SET_SIZE(mon_clock14_vec)

kclock14_vec:
	SYS_TRAP(0x1e)			! gets overlaid.
	SET_SIZE(kclock14_vec)

/*
 * Glue code for traps that should take us to the monitor/kadb if they
 * occur in kernel mode, but that the kernel should handle if they occur
 * in user mode.
 */
	.global kadb_tcode
	.global mon_breakpoint_vec

/*
 * tcode to replace trap vectors if kadb steals them away
 */
#define	TCODE_KADB	0xfe
#define	TCODE_MONBPT	0xff

	ENTRY_NP(trap_monbpt_tcode)
	mov	%psr, %l0
	sethi	%hi(trap_mon), %l4
	jmp	%l4 + %lo(trap_mon)
	mov	TCODE_MONBPT, %l4
	SET_SIZE(trap_monbpt_tcode)

	ENTRY_NP(trap_kadb_tcode)
	mov	%psr, %l0
	sethi	%hi(trap_kadb), %l4
	jmp	%l4 + %lo(trap_kadb)
	mov	TCODE_KADB, %l4
	SET_SIZE(trap_kadb_tcode)
#endif	/* lint */

/*
 * This code assumes that:
 * 1. the monitor uses trap ff to breakpoint us
 * 2. kadb steals both fe and fd when we call scbsync()
 * 3. kadb uses the same tcode for both fe and fd.
 * Note: the ".align 8" is needed so that the code that copies
 *	the vectors at system boot time can use ldd and std.
 * XXX The monitor shouldn't use the same trap as kadb!
 */
#if !defined(lint) && !defined(__lint)
	.align	8
trap_mon:
	btst	PSR_PS, %l0		! test pS
	bnz,a	1f			! branch if kernel trap
	mov	%l0, %psr		! delay slot, restore psr
	b,a	sys_trap		! user-mode, treat as bad trap
1:
	nop;nop;nop;nop			! psr delay, plus alignment
mon_breakpoint_vec:
	SYS_TRAP(0xff)			! gets overlaid.

trap_kadb:
	btst	PSR_PS, %l0		! test pS
	bnz,a	1f			! branch if kernel trap
	mov	%l0, %psr		! delay slot, restore psr
	b,a	sys_trap		! user-mode, treat as bad trap
1:
	nop;nop;nop;nop			! psr delay, plus alignment
kadb_tcode:
	SYS_TRAP(0xfe)			! gets overlaid.

#endif	/* lint */

#define	INTR_GEN 	0x704
#define	ASI_CC		0x02
#define	CC_BASE		0x01f00000	/* ASI-02 */

/*
 * Set the interrupt target register to the next available CPU using
 * a round-robin scheme.
 *
 *	Entry: traps disabled
 *		%o0 = current CPU id.
 */
#if defined(lint) || defined(__lint)
 
/* ARGSUSED */
void
itr_round_robin(int cpuid)
{}

#else	/* lint */
 
	ENTRY_NP(itr_round_robin) 

	sethi	%hi(last_cpu_id), %o1		! load the wrap-around pt.
	ld	[%o1 + %lo(last_cpu_id)], %o1

	set	cpu, %o3			! get cpu array pointer
	mov	%o0, %o5			! save current cpu
1:
	inc	%o5				! increment to get next cpu id
	cmp	%o5, %o1			! see if it wraps around. 
	ble	2f
	nop
	clr	%o5

2:
	sll	%o5, 2, %o4			! get index into cpu array
	ld	[%o3 + %o4], %o4		! get cpu struct pointer
	tst	%o4				! If NULL, get next cpu id
	bz	1b
	nop
	lduh	[%o4 + CPU_FLAGS], %o4		! get cpu_flags and determine
	and	%o4, CPU_READY | CPU_ENABLE, %o4 ! if the CPU is running
	cmp	%o4, CPU_READY | CPU_ENABLE	! branch if disabled
	bne	1b
	nop
	cmp	%o0, %o5			! see if we got the same cpu
	bz	3f				! skip setting itr if yes
	nop

	sll	%o5, 3, %o5			! %o5 has new intr target
	set	(0xff << 23), %o1		! set itr by inline code because
	or	%o5, %o1, %o5			! this is a leaf routine.
	set	CC_BASE+INTR_GEN, %o1		! see set_all_itr_by_cpuid.
	retl
	sta	%o5, [%o1]ASI_CC

3:
	retl
	nop
	SET_SIZE(itr_round_robin)
#endif	/* lint */

/*
 * void
 * .setpsr(state)
 *	int state;
 */
#if defined(lint) || defined(__lint)

void
setpsr(void)
{}

#else	/* lint */

	ENTRY_NP(.setpsr)
	set	PSL_UBITS, %l1
	andn	%l0, %l1, %l0		! clear user bits from %psr
	and	%i0, %l1, %i0		! reduce to maximum permissible(new)
	wr	%i0, %l0, %psr
	nop;nop;nop			! psr delay
#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l2
	rett	%l2 + 4
	SET_SIZE(.setpsr)

#endif	/* lint */

#if defined(lint) || defined(__lint)
/*
 * These need to be defined somewhere to lint and there is no "hicore.s"...
 */
char etext[1], end[1];
#endif	/* lint*/

/*
 * grrrr... I wish we could use the %oX registers!
 */

/*
 * 4d local copy of get_timestamp. see sparc_subr.s for the original.
 * the only difference here is we have VIKING_BUG_16 workaround.
 */
#if defined(lint) || defined(__lint)

void
get_timestamp(void)
{}

#else	/* lint */

	ENTRY_NP(.get_timestamp)
	! get high res time
	GET_HRTIME(%i0, %i1, %l4, %l5, %l6)	! get high res time in %i0:%i1

#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l2
	rett	%l2 + 4
	SET_SIZE(.get_timestamp)
#endif	/* lint */

/*
 * 4d local copy of get_virtime. see sparc_subr.s for the original.
 * the only difference here is we have VIKING_BUG_16 workaround.
 */
#if defined(lint) || defined(__lint)

void
get_virtime(void)
{}

#else	/* lint */

	ENTRY_NP(.get_virtime)
	GET_HRTIME(%i0, %i1, %l4, %l5, %l6)	! get high res time in %i0:%i1
	CPU_ADDR(%l3, %l4)			! CPU struct ptr to %l3
	ld	[%l3 + CPU_THREAD], %l3		! thread pointer to %l3
	ld	[%l3 + T_LWP], %l4		! lwp pointer to %l4
	/*
	 * Subtract start time of current microstate from time
	 * of day (i0,i1) to get increment for lwp virtual time.
	 */
	ldd	[%l4 + LWP_STATE_START], %l6	! ms_state_start
	subcc	%i1, %l7, %i1
	subx	%i0, %l6, %i0

	/*
	 * Add current value of ms_acct[LMS_USER]
	 */
	ldd	[%l4 + LWP_ACCT_USER], %l6	! ms_acct[LMS_USER]
	addcc	%i1, %l7, %i1
	addx	%i0, %l6, %i0

#ifdef VIKING_BUG_16
	rd	%y, %g0
#endif
	jmp	%l2
	rett	%l2 + 4
	SET_SIZE(.get_virtime)

#endif	/* lint */

#ifdef DEBUG
/*
 * Sometimes we want to force a watchdog on a particular cpu
 * to help debug kernel/OBP watchdog handling.  force_watchdog()
 * is called from trap() if watchdogme[CPU->cpu_id] is set.
 */

#if defined(lint) || defined(__lint)

void
force_watchdog(void)
{}

#else	/* lint */

	ENTRY_NP(force_watchdog)
	mov	%psr, %l0		! get psr
	andn	%l0, PSR_ET, %l0	! turn off ET bit
	mov	%l0, %psr		! write to psr
	nop; nop; nop			! do those requisite nops
	ta	0			! take a trap
	/* we should never get here */
	SET_SIZE(force_watchdog)

#endif	/* lint */
#endif	/* DEBUG */

#ifdef VIKING_BUG_1151159

/*
 * Code to workaround bug #1151159 in the Viking processor. The code
 * disables traps, replaces all lines of the DCache, locks lines 1-3 of each
 * set (Cannot lock line 0), invalidates lines 1-3 of each set and zero fills
 * lines 1-3 of each set, enables traps and returns.
 */

#if defined(lint)

void
vik_1151159_wa(void)
{ }

#else	/* lint */
	ENTRY_NP(vik_1151159_wa)
	save	%sp, -SA(MINFRAME), %sp

	mov	%psr, %g1
	andn	%g1, PSR_ET, %g2	! disable traps
	mov	%g2, %psr
	nop; nop; nop;

	! Quick'n'dirty displacement flush of dcache
	clr	%o0
	set	_start, %o1
	set	0x8000, %o3
	mov	%o1, %o2
1:
	ld	[%o2], %g0
	add	%o0, 32, %o0
	cmp	%o0, %o3
	ble	1b
	add	%o1, %o0, %o2

	! Start mucking with tags
	!
	! Register usage:
	!
	!	%i0: increment to get to the next line in the set (line number
	!	     is set in bits <26-27> of the PTAG.
	!	%i3: maximum line number (line 3)
	! 	%i5: the SuperSPARC STAG address format
	! 	%i4: the SuperSPARC PTAG address format
	! 	%l2: temporary holding register for STAG or PTAG address format.
	!	     encoding current set #, line # in the set and double word
	!	     in the line
	!	%o0, %o1 contain data to be written to STAG (0xe)
	!	%o2, %o3 contain data to be written to PTAG (0)
	!
	!	The code works like this
	!	disable_traps;
	!	replace-lines-in the dcache (to handle SuperSPARC/No ECache)
	!	for (set = 0; set < 128; set++) {
	!		lock lines 1-3;
	!		for (line = 1; line < 4; line ++) {
	!			invalidate line;
	!			for (dbl_word = 0; dbl_word < 4; dbl_word++)
	!				zero fill double word;
	!		}
	!	}
	!	enable traps;
	!	return;

	sethi	%hi(VIK_STAGADDR), %i5
	mov	0x0, %o0		! upper 32-bits stag data
	set	0xe, %o1		! lower 32-bits stag data
	sethi	%hi(VIK_PTAGADDR), %i4

	mov	0, %i2
	or	%i2, %i5, %l2		! beginning stag addr
vhwb_nextset:
	stda	%o0, [%l2]ASI_DCT	! set stag to 0xe (lock lines 1-3)

	sethi	%hi(0x04000000),%i0	! line 1 << 26
	sethi	%hi(0x0c000000),%i3	! line 3 << 26
	or	%i0, %i4, %l2
vhwb_nextline:
	or	%i2, %l2, %l2
	mov	0, %o2
	mov	0, %o3
	stda	%o2, [%l2]ASI_DCT	! clear ptag (invalidate line)

	! zero fill the 4 double words of the current line

	mov	0, %l3			! start with double word 0
	or	%i2, %i0, %i1		! Save of a copy of current PTAG
	sll	%l3, 3, %l2		! in %i1


vhwb_nextdword:
	or	%i1, %l2, %l2		! Add in double word to the PTAG
	mov	0, %o2
	mov	0, %o3
	stda	%o2, [%l2]ASI_DCD	! clear dword
	add	%l3, 1, %l3		! increment double word#
	cmp	%l3, 3
	ble	vhwb_nextdword
	sll	%l3, 3, %l2

	sethi	%hi(0x04000000),%l2	! 1 << 26
	add	%i0, %l2, %i0		! increment line#
	cmp	%i0, %i3		! %i3 = maxline
	ble,a	vhwb_nextline
	or	%i0, %i4, %l2		! Delay: Increment line # in
					! PTAG

	add	%i2, 32, %i2		! increment set#
	cmp	%i2, 4064		! Done locking all sets?
	ble	vhwb_nextset		! No, lock lines in next set
	or	%i2, %i5, %l2		! Delay: Increment set # in
					! PTAG

	mov	%g1, %psr		! Enable traps and return
	nop; nop; nop;

	ret
	restore
	SET_SIZE(vik_1151159_wa)

#endif /* lint */
#endif	/* VIKING_BUG_1151159 */
