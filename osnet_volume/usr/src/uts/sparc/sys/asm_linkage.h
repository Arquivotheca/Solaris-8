/*
 * Copyright (c) 1987, 1993, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#pragma ident	"@(#)asm_linkage.h	1.36	97/09/28 SMI"

#include <sys/stack.h>
#include <sys/trap.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM	/* The remainder of this file is only for assembly files */

/*
 * C pointers are different sizes between V8 and V9.
 * These constants can be used to compute offsets into pointer arrays.
 */
#ifdef __sparcv9
#define	CPTRSHIFT	3
#define	CLONGSHIFT	3
#else
#define	CPTRSHIFT	2
#define	CLONGSHIFT	2
#endif
#define	CPTRSIZE	(1<<CPTRSHIFT)
#define	CLONGSIZE	(1<<CLONGSHIFT)
#define	CPTRMASK	(CPTRSIZE - 1)
#define	CLONGMASK	(CLONGSIZE - 1)

/*
 * Symbolic section definitions.
 */
#define	RODATA	".rodata"

/*
 * profiling causes defintions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#ifdef GPROF

#define	MCOUNT_SIZE	(4*4)	/* 4 instructions */
#define	MCOUNT(x) \
	save	%sp, -SA(MINFRAME), %sp; \
	call	_mcount; \
	nop; \
	restore;

#endif /* GPROF */

#ifdef PROF

#define	MCOUNT_SIZE	(5*4)	/* 5 instructions */
#define	MCOUNT(x) \
	save	%sp, -SA(MINFRAME), %sp; \
/* CSTYLED */ \
	sethi	%hi(.L_/**/x/**/1), %o0; \
	call	_mcount; \
/* CSTYLED */ \
	or	%o0, %lo(.L_/**/x/**/1), %o0; \
	restore; \
/* CSTYLED */ \
	.common .L_/**/x/**/1, 4, 4

#endif /* PROF */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if !defined(PROF) && !defined(GPROF)
#define	MCOUNT_SIZE	0	/* no instructions inserted */
#define	MCOUNT(x)
#endif /* !defined(PROF) && !defined(GPROF) */

#define	RTMCOUNT(x)	MCOUNT(x)

/*
 * Macro to define weak symbol aliases. These are similar to the ANSI-C
 *	#pragma weak name = _name
 * except a compiler can determine type. The assembler must be told. Hence,
 * the second parameter must be the type of the symbol (i.e.: function,...)
 */
#define	ANSI_PRAGMA_WEAK(sym, stype)	\
	.weak	sym; \
	.type sym, #stype; \
/* CSTYLED */ \
sym	= _/**/sym

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:	MCOUNT(x)

#define	ENTRY_SIZE	MCOUNT_SIZE

#define	ENTRY_NP(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:

#define	RTENTRY(x) \
	.section	".text"; \
	.align	4; \
	.global	x; \
	.type	x, #function; \
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y) \
	.section	".text"; \
	.align	4; \
	.global	x, y; \
	.type	x, #function; \
	.type	y, #function; \
/* CSTYLED */ \
x:	; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.section	".text"; \
	.align	4; \
	.global	x, y; \
	.type	x, #function; \
	.type	y, #function; \
/* CSTYLED */ \
x:	; \
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.global x; \
	.type	x, #function; \
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 *
 * DGDEF provides a word aligned word of storage.
 *
 * DGDEF2 allocates "sz" bytes of storage with **NO** alignment.  This
 * implies this macro is best used for byte arrays.
 *
 * DGDEF3 allocates "sz" bytes of storage with "algn" alignment.
 */
#define	DGDEF2(name, sz) \
	.section	".data"; \
	.global name; \
	.type	name, #object; \
	.size	name, sz; \
name:

#define	DGDEF3(name, sz, algn) \
	.section	".data"; \
	.align	algn; \
	.global name; \
	.type	name, #object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF3(name, 4, 4)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x) \
	.size	x, (.-x)

#ifdef _KERNEL

#ifdef TRACE

#include <sys/vtrace.h>

/*
 * Notes on tracing from assembly files:
 *
 * 1. Usage: TRACE_ASM_<n> (scr, fac, tag, namep [, data_1, ..., data_n]);
 *
 *	scr	= scratch register (will be clobbered)
 *	fac	= facility
 *	tag	= tag
 *	namep	= address of name:format string (ascii-z)
 *	data_i	= any register or "simm13" (-4096 <= x <= 4095) constant
 *
 *    Example:
 *
 *		.global TR_intr_start;
 *	TR_intr_start:
 *		.asciz "interrupt_start:level %d";
 *		.align 4;
 *    ...
 *	TRACE_ASM_1 (%o2, TR_FAC_INTR, TR_INTR_START, TR_intr_start, %l4);
 *
 *    When you use TRACE_ASM_[1-5], the data words you specify will be
 *    copied into %o1-%o5, in that order.  Make sure that (1) you either
 *    save or don't need the affected out-registers, and (2) you don't
 *    step on yourself [e.g. TRACE_ASM_2(..., %o2, %o1) would cause %o1
 *    to be overwritten by %o2].  Also, note that %o0 is always used as a
 *    scratch register, but only *after* your data values have been
 *    copied into %o1-%o5.  Thus, for example, TRACE_ASM_1(..., %o0) will
 *    work just fine, because %o0 will be copied into %o1 before it is
 *    clobbered.
 *    In the example above, %l4 is copied into %o1, and %o0 and %o2 are
 *    clobbered.
 *
 * 2. Registers: TRACE_ASM_N destroys %o0-%oN and the scratch register,
 *    and leaves the rest intact.
 *
 * 3. If you want to put a trace point where traps are disabled, you cannot
 *    use these macros; see the example below.
 *
 * 4. (Obvious, but...) Don't put a trace macro in a branch delay slot.
 *
 * 5. "name" should be the *address* of an ascii-z string, not the string
 *    itself.
 *
 * 6. BE CAREFUL if you ever change this macro.  To avoid using local
 *    labels (which could collide with neighboring code), the two
 *    branches below are hand-computed.
 */

#define	TRACE_ASM_N(scr, fac, tag, name, len, trace_trap)		\
	ld	[THREAD_REG + T_CPU], %o0;	/* %o0 = cpup */	\
	ld	[%o0 + CPU_TRACE_EVENT_MAP], %o0;  /* %o0 = map addr */	\
	set	FT2EVENT(fac, tag), scr;	/* scr = event */	\
	ldub	[%o0 + scr], %o0;		/* %o0 = event info */	\
	andcc	%o0, VT_ENABLED, %g0;		/* is event enabled? */	\
	bz	. + 30*4;			/* EVIL! DANGER! */	\
	andcc	%o0, VT_USED, %g0;		/* is event used? */	\
	bnz	. + 26*4;			/* EVIL! DANGER! */	\
	sll	scr, 16, scr;			/* scr = header word */	\
	/* save the world, go to C for event label */			\
	save	%sp, -SA(MINFRAME), %sp;	/* save ins, locals */	\
	mov	%g1, %l1;			/* save globals */	\
	mov	%g2, %l2;						\
	mov	%g3, %l3;						\
	mov	%g4, %l4;						\
	mov	%g5, %l5;						\
	mov	%g6, %l6;						\
	mov	%g7, %l7;						\
	or	%g0, (fac), %o0;		/* %o0 = facility */	\
	or	%g0, (tag), %o1;		/* %o1 = tag */		\
	or	%g0, (len), %o2;		/* %o2 = length */	\
	sethi	%hi(name), %o3;						\
	or	%o3, %lo(name), %o3;		/* %o3 = name */	\
	call	trace_label;						\
	sub	%g0, 1, %o4;			/* delay: %o4 = -1 */	\
	mov	%o0, %i0;			/* return vaule */	\
	mov	%l1, %g1;			/* restore globals */	\
	mov	%l2, %g2;						\
	mov	%l3, %g3;						\
	mov	%l4, %g4;						\
	mov	%l5, %g5;						\
	mov	%l6, %g6;						\
	mov	%l7, %g7;						\
	restore;							\
	/* event label done, state restored */				\
	or	%o0, scr, %o0;			/* set event info */	\
	ta	trace_trap;			/* write the record */

#define	TRACE_ASM_0(scr, fac, tag, name) \
	TRACE_ASM_N(scr, fac, tag, name, 4, ST_TRACE_0)

#define	TRACE_ASM_1(scr, fac, tag, name, d1) \
	mov d1, %o1;							\
	TRACE_ASM_N(scr, fac, tag, name, 8, ST_TRACE_1)

#define	TRACE_ASM_2(scr, fac, tag, name, d1, d2) \
	mov d1, %o1; mov d2, %o2;					\
	TRACE_ASM_N(scr, fac, tag, name, 12, ST_TRACE_2)

#define	TRACE_ASM_3(scr, fac, tag, name, d1, d2, d3) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3;				\
	TRACE_ASM_N(scr, fac, tag, name, 16, ST_TRACE_3)

#define	TRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3; mov d4, %o4;		\
	TRACE_ASM_N(scr, fac, tag, name, 20, ST_TRACE_4)

#define	TRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3; mov d4, %o4; mov d5, %o5; \
	TRACE_ASM_N(scr, fac, tag, name, 24, ST_TRACE_5)

#ifdef _MACHDEP

#include <sys/clock.h>

/*
 * TRACE_DUMP_HEAD macro.
 *
 * On entry:
 *	event	= FTT2HEAD(fac, tag, event_info)
 *	cpup	= pointer to current CPU struct
 *	headp, time, hrec = scratch registers
 *
 * On exit:
 *	event	= unmodified [FTT2HEAD(fac, tag, event_info)]
 *	cpup	= unmodified [pointer to current CPU struct]
 *	headp	= head pointer into trace buffer
 *	time	= time delta (unless 16-bit overflow, then time = 0)
 *	hrec	= header record for this trace == (event & 0xffffff00) | time
 *	escape	= label to branch to on error (usually _trace_trap_ret)
 */

#define	TRACE_DUMP_HEAD(event, cpup, headp, time, hrec, escape) \
	ld	[cpup + CPU_TRACE_THREAD], hrec; /* hrec = prev thread */\
	ld	[cpup + CPU_THREAD], time;	/* time = current thread */\
	cmp	time, hrec;			/* new thread? */\
	be	. + 13*4;	/* 2f */	/* nope. */\
	nop;					/* don't hurt instr grouping */\
	tst	hrec;				/* NULL thread ptr? */\
	bnz	. + 4*4;	/* 1f */	/* no, continue */\
	sethi	%hi(FTT2HEAD(TR_FAC_TRACE, TR_RAW_KTHREAD_ID, 0)), hrec; \
	b	escape;	\
	nop; \
/* 1 */	st	time, [cpup + CPU_TRACE_THREAD]; /* set last_thread */\
	ld	[cpup + CPU_TRACE_HEAD], headp;	/* headp = buffer head addr */\
	st	hrec, [headp + TRACE_KTID_HEAD]; /* store kthread ID header */\
	st	time, [headp + TRACE_KTID_TID]; /* store new kthread ID */\
	add	headp, TRACE_KTID_SIZE, headp;	/* move head pointer */\
	st	headp, [cpup + CPU_TRACE_HEAD];	/* store new head pointer */\
/* 2 */	GET_VTRACE_TIME(time, headp, hrec);	/* time = new hrtime */\
	ld	[cpup + CPU_TRACE_HRTIME], hrec; /* hrec = old hrtime */\
	ld	[cpup + CPU_TRACE_HEAD], headp;	/* headp = buffer head addr */\
	st	time, [cpup + CPU_TRACE_HRTIME]; /* store new hrtime */\
	sub	time, hrec, time;		/* time = time delta */\
	srl	time, 16, hrec;			/* hrec != 0 if time > 2^16 */\
	tst	hrec; \
	bz	. + 7*4;	/* 3f */	/* time delta <= 16 bits */\
	andn	event, 0xff, hrec;		/* hrec = header word */\
	st	time, [headp + TRACE_ETIME_TIME]; /* 32-bit time delta */\
	sethi	%hi(FTT2HEAD(TR_FAC_TRACE, TR_ELAPSED_TIME, 0)), time; \
	st	time, [headp + TRACE_ETIME_HEAD]; /* 32-bit time delta head */\
	clr	time;				/* 16-bit delta becomes zero */\
	add	headp, TRACE_ETIME_SIZE, headp;	/* move head pointer */\
/* 3 */	or	hrec, time, hrec;		/* add time to event header */\
	st	hrec, [headp];			/* store event header */

/*
 * TRACE_STORE_STRING macro.
 *
 * On entry:
 *	mask	= string bit mask (this is a constant, not a register)
 *	maskreg	= string mask for this trace
 *	strptr	= string to be stored
 *	headp	= head pointer into trace buffer
 *	scr1	= scratch
 *	scr2	= scratch
 *
 * On exit:
 *	maskreg	= unmodified
 *	strptr	= unmodified
 *	headp	= head pointer into trace buffer
 */

#define	TRACE_STORE_STRING(mask, maskreg, strptr, headp, scr1, scr2) \
	andcc	maskreg, mask, %g0;		/* Is this a string? */\
	bz	. + 20*4;	/* 3f */	/* nope. */\
	clr	scr1;				/* index into string */\
	add	headp, 4, headp;		/* where the string goes */\
/* 1 */	cmp	scr1, VT_MAX_BYTES - 5;		/* max str length */\
	blu	. + 3*4;	/* 2f */	/* not exceeded */\
	ldub	[strptr + scr1], scr2;		/* delay: get a character */\
	clr	scr2;				/* force a NULL */\
/* 2 */	stb	scr2, [headp + scr1];		/* write char to buffer */\
	tst	scr2;				/* is this a NULL? */\
	bnz	. - 6*4;	/* 1b */	/* no, keep looping */\
	inc	scr1;				/* delay: increment index */\
	sub	headp, 4, headp;		/* where the header goes */\
	sethi	%hi(bytes2data), scr2;		/* data record array */\
	or	scr2, %lo(bytes2data), scr2; \
	sll	scr1, 2, scr1;			/* convert to word index */\
	add	scr2, scr1, scr2;		/* add the index */\
	ld	[scr2], scr1;			/* scr1 = header word */\
	st	scr1, [headp];			/* store header word */\
	ld	[scr2 + 256*4], scr1;		/* length of this data rec */\
	add	headp, scr1, headp;		/* add len to headp */
/* 3 */

/*
 * TRACE_DUMP_TAIL macro.
 *
 * On entry:
 *	cpup	= pointer to current CPU struct
 *	headp	= head pointer into trace buffer
 *	scr1	= scratch
 *	scr2	= scratch
 *
 * On exit:
 *	cpup	= pointer to current CPU struct
 *	headp	= head pointer into trace buffer
 */

#define	TRACE_DUMP_TAIL(cpup, headp, scr1, scr2) \
	ld	[cpup + CPU_TRACE_REDZONE], scr1; /* scr1 = red zone page */\
	andn	headp, 0xfff, scr2;		/* scr2 = current page */\
	cmp	scr1, scr2;			/* are we in the red zone? */\
	bne	. + 3*4;	/* 1f */	/* if not, branch... */\
	ld	[cpup + CPU_TRACE_WRAP], scr1;	/* delay: scr1 = wrap point */\
	st	headp, [cpup + CPU_TRACE_OVERFLOW]; /* store non-zero value */\
/* 1 */	cmp	headp, scr1;			/* need to wrap? */\
	blu	. + 10*4;	/* 3f */	/* if not, branch */\
	st	headp, [cpup + CPU_TRACE_HEAD];	/* delay: store new head */\
	sethi	%hi(FTT2HEAD(TR_FAC_TRACE, TR_PAD, 0)), scr2; \
	ld	[cpup + CPU_TRACE_END], scr1;	/* scr1 = buffer end */\
/* 2 */	add	headp, 4, headp;		/* move head pointer */\
	cmp	headp, scr1;			/* are we done padding? */\
	blu	. - 2*4;	/* 2b */	/* if not, do some more */\
	st	scr2, [headp - 4];		/* delay: write pad record */\
	ld	[cpup + CPU_TRACE_START], headp; /* wrap around */\
	st	headp, [cpup + CPU_TRACE_HEAD];	/* store new head pointer */\
/* 3 */

/*
 * TRACE_DUMP_[0-5] macros.
 *
 * On entry:
 *	event	= FTT2HEAD(fac, tag, event_info)
 *	d[1-5]	= data fields for the trace record
 *	cpup	= pointer to current CPU struct
 *	headp	= scratch
 *	scr1	= scratch
 *	scr2	= scratch
 *
 * On exit:
 *	event	= unmodified [FTT2HEAD(fac, tag, event_info)]
 *	d[1-5]	= unmodified [data fields for the trace record]
 *	cpup	= unmodified [pointer to current CPU struct]
 *	headp	= head pointer into trace buffer
 */

#define	TRACE_DUMP_0(event, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	add	headp, 4, headp; \
	TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

#define	TRACE_DUMP_1(event, d1, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	st	d1, [headp + 4]; \
	add	headp, (1 + 1)*4, headp; \
	TRACE_STORE_STRING(VT_STRING_1, event, d1, headp, scr1, scr2); \
	TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

#define	TRACE_DUMP_2(event, d1, d2, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	st	d1, [headp + 4]; \
	st	d2, [headp + 8]; \
	andcc	event, VT_STRING_MASK, %g0;	/* Are there any strings? */\
	bz	. + (2 + 2*21)*4;	/* 1f */	/* nope. */\
	add	headp, (1 + 2)*4, headp; \
	TRACE_STORE_STRING(VT_STRING_1, event, d1, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_2, event, d2, headp, scr1, scr2); \
/* 1 */ TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

#define	TRACE_DUMP_3(event, d1, d2, d3, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	st	d1, [headp + 4]; \
	st	d2, [headp + 8]; \
	st	d3, [headp + 12]; \
	andcc	event, VT_STRING_MASK, %g0;	/* Are there any strings? */\
	bz	. + (2 + 3*21)*4;	/* 1f */	/* nope. */\
	add	headp, (1 + 3)*4, headp; \
	TRACE_STORE_STRING(VT_STRING_1, event, d1, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_2, event, d2, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_3, event, d3, headp, scr1, scr2); \
/* 1 */ TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

#define	TRACE_DUMP_4(event, d1, d2, d3, d4, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	st	d1, [headp + 4]; \
	st	d2, [headp + 8]; \
	st	d3, [headp + 12]; \
	st	d4, [headp + 16]; \
	andcc	event, VT_STRING_MASK, %g0;	/* Are there any strings? */\
	bz	. + (2 + 4*21)*4;	/* 1f */	/* nope. */\
	add	headp, (1 + 4)*4, headp; \
	TRACE_STORE_STRING(VT_STRING_1, event, d1, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_2, event, d2, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_3, event, d3, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_4, event, d4, headp, scr1, scr2); \
/* 1 */ TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

#define	TRACE_DUMP_5(event, d1, d2, d3, d4, d5, cpup, headp, scr1, scr2) \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, _trace_trap_ret); \
	st	d1, [headp + 4]; \
	st	d2, [headp + 8]; \
	st	d3, [headp + 12]; \
	st	d4, [headp + 16]; \
	st	d5, [headp + 20]; \
	andcc	event, VT_STRING_MASK, %g0;	/* Are there any strings? */\
	bz	. + (2 + 5*21)*4;	/* 1f */	/* nope. */\
	add	headp, (1 + 5)*4, headp; \
	TRACE_STORE_STRING(VT_STRING_1, event, d1, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_2, event, d2, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_3, event, d3, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_4, event, d4, headp, scr1, scr2); \
	TRACE_STORE_STRING(VT_STRING_5, event, d5, headp, scr1, scr2); \
/* 1 */ TRACE_DUMP_TAIL(cpup, headp, scr1, scr2);

/*
 * On entry:
 *	cpup	= pointer to current CPU struct
 *	event	= scratch
 *	info	= scratch
 * On exit:
 *	cpup	= pointer to current CPU struct
 *	event	= event number (FT2EVENT(fac, tag))
 *	info	= event info (enabled bit, used bit, and strings bits)
 */

#define	VT_ASM_TEST_FT(fac, tag, cpup, event, info)			\
	ld	[cpup + CPU_TRACE_EVENT_MAP], info;			\
	set	FT2EVENT(fac, tag), event;				\
	ldub	[info + event], info;		/* info = event info */	\
	andcc	info, VT_ENABLED, %g0;		/* is event enabled? */

/*
 * Tracing where traps are disabled:
 *
 * If your registers look like this:
 *
 *	%l3, %l6	contents valuable
 *	%l4, %l5, %l7	contents expendable
 *
 * Use a sequence like this:
 *
 * #ifdef TRACE
 *	CPU_ADDR(%l7, %l4)		! get CPU struct ptr to %l7 using %l4
 *	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_KERNEL_WINDOW_OVERFLOW, %l7, %l4, %l5);
 *	bz	1f;
 *	sll	%l4, 16, %l4;
 *	or	%l4, %l5, %l4		! %l4 = event + info
 *	st	%l3, [%l7 + CPU_TRACE_SCRATCH];
 *	st	%l6, [%l7 + CPU_TRACE_SCRATCH + 4];
 *	TRACE_DUMP_1(%l4, %l1, %l7, %l3, %l5, %l6);
 *	ld	[%l7 + CPU_TRACE_SCRATCH], %l3;
 *	ld	[%l7 + CPU_TRACE_SCRATCH + 4], %l6;
 * 1:
 * #endif TRACE
 */

/*
 * Special macros for tracing the start and end of traps.
 * Can only be used while traps are disabled.
 * Does *NOT* preserve %psr CC bits -- do this manually if necessary.
 * Be careful of possible conflicts with local labels ("5:").
 * On entry, only "type" is needed.
 */

#define	TRACE_SYS_TRAP_START(type, cpup, event, headp, scr1, scr2) \
	CPU_ADDR(cpup, scr1); \
	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_TRAP_START, cpup, event, scr1); \
	bz	5f; \
	sll	event, 16, event; \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, 5f); \
	st	type, [headp + 4]; \
	add	headp, (1 + 1)*4, headp; \
	TRACE_DUMP_TAIL(cpup, headp, scr1, scr2); \
5:

#define	TRACE_SYS_TRAP_END(cpup, event, headp, scr1, scr2) \
	CPU_ADDR(cpup, scr1); \
	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_TRAP_END, cpup, event, scr1); \
	bz	5f; \
	sll	event, 16, event; \
	TRACE_DUMP_HEAD(event, cpup, headp, scr1, scr2, 5f); \
	add	headp, 4, headp; \
	TRACE_DUMP_TAIL(cpup, headp, scr1, scr2); \
5:

#endif	/* _MACHDEP */

#else /* TRACE */

#define	TRACE_ASM_0(scr, fac, tag, name)
#define	TRACE_ASM_1(scr, fac, tag, name, d1)
#define	TRACE_ASM_2(scr, fac, tag, name, d1, d2)
#define	TRACE_ASM_3(scr, fac, tag, name, d1, d2, d3)
#define	TRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4)
#define	TRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5)

#endif /* TRACE */

#endif /* _KERNEL */

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
