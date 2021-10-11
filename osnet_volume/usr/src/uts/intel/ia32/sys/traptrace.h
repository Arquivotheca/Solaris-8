/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA32_SYS_TRAPTRACE_H
#define	_IA32_SYS_TRAPTRACE_H

#pragma ident	"@(#)traptrace.h	1.1	99/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap tracing.  If TRAPTRACE is defined, an entry is recorded every time
 * the CPU jumps through the Interrupt Descriptor Table (IDT).  One exception
 * is the Double Fault handler, which does not record a traptrace entry.
 */

#ifndef	_ASM

typedef struct {
	uintptr_t	ttc_next;
	uintptr_t	ttc_first;
	uintptr_t	ttc_limit;
	uintptr_t	ttc_current;
} trap_trace_ctl_t;

typedef struct {
	struct regs	ttr_regs;
	union {
		struct {
			uchar_t	vector;
			uchar_t	ipl;
			uchar_t	spl;
			uchar_t	pri;
		} idt_entry;
		struct {
			int	sysnum;
		} gate_entry;
	} ttr_info;
	caddr_t		ttr_pad0;
	uchar_t		ttr_pad1[3];
	uchar_t		ttr_marker;
	hrtime_t	ttr_stamp;
} trap_trace_rec_t;

#define	ttr_vector	ttr_info.idt_entry.vector
#define	ttr_ipl		ttr_info.idt_entry.ipl
#define	ttr_spl		ttr_info.idt_entry.spl
#define	ttr_pri		ttr_info.idt_entry.pri
#define	ttr_sysnum	ttr_info.gate_entry.sysnum

#define	TRAPTR_NENT	128

extern trap_trace_ctl_t	trap_trace_ctl[];	/* Allocated in locore.s */
extern size_t		trap_trace_bufsize;
extern int		trap_trace_freeze;
extern trap_trace_rec_t	trap_trace_postmort;	/* Entry used after death */

#define	TRAPTRACE_FREEZE	trap_trace_freeze = 1;
#define	TRAPTRACE_UNFREEZE	trap_trace_freeze = 0;

#else

/*
 * ptr    -- will be set to a TRAPTRACE entry.
 * scr1   -- scratch
 * scr2   -- scratch
 * marker -- register containing byte to store in marker field of entry
 *
 * Note that this macro defines labels "8" and "9".
 */
#ifdef TRAPTRACE
#define	TRACE_PTR(ptr, scr1, scr2, marker)	\
	mov	$trap_trace_postmort, ptr;	\
	cmpl	$0, trap_trace_freeze;		\
	jne	9f;				\
	LOADCPU(ptr);				\
	movl	CPU_ID(ptr), scr1;		\
	shll	$TRAPTR_SIZE_SHIFT, scr1;	\
	addl	$trap_trace_ctl, scr1;		\
	movl	TRAPTR_NEXT(scr1), ptr;		\
	movl	ptr, scr2;			\
	addl	$TRAP_ENT_SIZE, scr2;		\
	cmpl	TRAPTR_LIMIT(scr1), scr2;	\
	jl	8f;				\
	movl	TRAPTR_FIRST(scr1), scr2;	\
8:	movl	scr2, TRAPTR_NEXT(scr1);	\
9:	movb	marker, TTR_MARKER(ptr);

/*
 * ptr  -- pointer to the current TRAPTRACE entry.
 * reg  -- pointer to the stored registers; must be on the stack
 * scr1 -- scratch used as array index
 * scr2 -- scratch used as temporary
 *
 * Note that this macro defines label "9".
 */
#define	TRACE_REGS(ptr, reg, scr1, scr2)	\
	xorl	scr1, scr1;			\
	/*CSTYLED*/				\
9:	movl	(reg, scr1, 1), scr2;		\
	movl	scr2, (ptr, scr1, 1);		\
	addl	$4, scr1;			\
	cmpl	$REGSIZE, scr1;			\
	jl	9b

/*
 * The time stamp macro records a high-resolution time stamp for the
 * given TRAPTRACE entry.  Note that %eax and %edx are plowed by this
 * macro;  if they are to be preserved, it's up to the caller of the macro.
 */
#define	TRACE_STAMP(reg)			\
	xorl	%eax, %eax;			\
	xorl	%edx, %edx;			\
	testl	$X86_TSC, x86_feature;		\
	jz	9f;				\
	rdtsc;					\
9:	movl	%eax, TTR_STAMP(reg);		\
	movl	%edx, TTR_STAMP+4(reg)

#else

#define	TRACE_PTR(ptr, reg, scr, scr2)
#define	TRACE_REGS(ptr, reg, scr1, scr2)
#define	TRACE_STAMP(reg)

#endif	/* TRAPTRACE */

#endif 	/* _ASM */

#define	TT_SYSCALL	0xaa
#define	TT_INTERRUPT	0xbb
#define	TT_TRAP		0xcc
#define	TT_INTTRAP	0xdd

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_TRAPTRACE_H */
