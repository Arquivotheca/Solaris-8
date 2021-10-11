/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpc_subr.c	1.3	99/11/20 SMI"

/*
 * Pentium-specific routines used by the CPU Performance counter driver.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/cpc_event.h>
#include <sys/cpc_impl.h>
#include <sys/reg.h>
#include <sys/x86_archext.h>
#include <sys/sunddi.h>

/*
 * Virtualizes the 40-bit field of the %pic
 * register into a 64-bit software register.
 *
 * We can retrieve 40 (signed) bits from the counters,
 * but we can set only 32 (signed) bits into the counters.
 * This makes virtualizing more than 31-bits of registers
 * quite tricky.
 *
 * If bits 39 to 31 are set in the virtualized pic register,
 * then we can preset the counter to this value using the fact
 * that wrmsr sign extends bit 31.   Though it might look easier
 * to only use the bottom 31-bits of the register, we have to allow
 * the full 40-bits to be used to perform overflow profiling.
 */
#define	MASK40		UINT64_C(0xffffffffff)
#define	MASK31		UINT64_C(0x7fffffff)
#define	BITS_39_31	UINT64_C(0xff80000000)

static int64_t
diff3931(uint64_t sample, uint64_t old)
{
	int64_t diff;

	if ((old & BITS_39_31) == BITS_39_31) {
		diff = (MASK40 & sample) - old;
		if (diff < 0)
			diff += (UINT64_C(1) << 40);
	} else {
		diff = (MASK31 & sample) - old;
		if (diff < 0)
			diff += (UINT64_C(1) << 31);
	}
	return (diff);
}

static uint64_t
trunc3931(uint64_t value)
{
	if ((value & BITS_39_31) == BITS_39_31)
		return (MASK40 & value);
	return (MASK31 & value);
}

void
kcpc_hw_sample(kcpc_ctx_t *ctx)
{
	uint64_t curtsc, curpic[2];

	ASSERT(KCPC_VALID_CTX(ctx));

	switch (x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		(void) rdmsr(REG_PERFCTR0, &curpic[0]);
		(void) rdmsr(REG_PERFCTR1, &curpic[1]);
		(void) rdmsr(REG_TSC, &curtsc);
		break;
	case X86_P5:
		(void) rdmsr(P5_CTR0, &curpic[0]);
		(void) rdmsr(P5_CTR1, &curpic[1]);
		(void) rdmsr(REG_TSC, &curtsc);
		break;
	default:
		return;
	}

	ctx->c_event.ce_hrt = gethrtime();
	ctx->c_event.ce_tsc += curtsc - ctx->c_rawtsc;
	ctx->c_rawtsc = curtsc;

	ctx->c_event.ce_pic[0] += diff3931(curpic[0], ctx->c_rawpic[0]);
	ctx->c_rawpic[0] = trunc3931(ctx->c_event.ce_pic[0]);
	ctx->c_event.ce_pic[1] += diff3931(curpic[1], ctx->c_rawpic[1]);
	ctx->c_rawpic[1] = trunc3931(ctx->c_event.ce_pic[1]);
}

#define	P6_PES_EN	(UINT32_C(1) << CPC_P6_PES_EN)
#define	P6_PES_INT	(UINT32_C(1) << CPC_P6_PES_INT)
#define	P6_PES_OS	(UINT32_C(1) << CPC_P6_PES_OS)

static uint64_t allstopped;
static uint_t (*overflow_intr_handler)(caddr_t);

int kcpc_hw_overflow_intr_installed;		/* set by APIC code */

/*
 * The current thread context had an overflow interrupt; we're
 * executing here in high-level interrupt context.  Called directly
 * out of the APIC on pcplusmp-compliant machines.
 */
uint_t
kcpc_hw_overflow_intr(caddr_t arg)
{
	uint64_t pes[2];

	/*
	 * If we could guarantee that we're the only routine on this
	 * level, we can assert that it -must- have been our
	 * interrupt.  For now, assume we have to be reasonably cautious ..
	 */
	(void) rdmsr(REG_PERFEVNT0, &pes[0]);
	if (((uint32_t)pes[0] & P6_PES_EN) != P6_PES_EN)
		return (DDI_INTR_UNCLAIMED);	/* not counting */

	(void) rdmsr(REG_PERFEVNT1, &pes[1]);
	if (((uint32_t)pes[0] & P6_PES_INT) != P6_PES_INT &&
	    ((uint32_t)pes[1] & P6_PES_INT) != P6_PES_INT)
		return (DDI_INTR_UNCLAIMED);	/* not trying to interrupt */

	if (overflow_intr_handler != NULL) {
		if (overflow_intr_handler(arg)) {
			/*
			 * Disable the counters
			 */
			wrmsr(REG_PERFEVNT0, &allstopped);
			setcr4((uint32_t)cr4() & ~CR4_PCE);
		}
	}
	return (DDI_INTR_CLAIMED);
}

/*
 * Now we're being called from trap from an AST on the way out
 * of the kernel.  On Pentium II processors, we can only tell if
 * a register overflowed iff we know that it's the only register
 * that could have generated an interrupt, so we can't, in general
 * use the overflow to automatically update our virtualized counters.
 *
 * Since the only way we can get here is if we're expecting a signal
 * to be delivered, the counters are already stopped, so just sample
 * the counters and leave them frozen.
 *
 * This assumes the caller has additional semantic knowledge about the
 * overflow that can resolve any ambiguity.
 */
void
kcpc_hw_overflow_trap(kthread_t *t)
{
	kcpc_ctx_t *ctx = ttocpcctx(t);

	ASSERT(KCPC_VALID_CTX(ctx));
	ASSERT(ctx->c_flags & KCPC_CTX_SIGOVF);

	kcpc_hw_sample(ctx);
	atomic_or_uint(&ctx->c_flags, KCPC_CTX_FREEZE);
}

/*
 * Called when switching away from this thread
 */
void
kcpc_hw_save(kcpc_ctx_t *ctx)
{
	ASSERT(KCPC_VALID_CTX(ctx));

	if (ctx->c_flags & KCPC_CTX_INVALID)
		return;
	switch (x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		wrmsr(REG_PERFEVNT0, &allstopped);
		break;
	case X86_P5:
		wrmsr(P5_CESR, &allstopped);
		break;
	default:
		break;
	}
	if ((x86_feature & X86_MMX) != 0)
		setcr4((uint32_t)cr4() & ~CR4_PCE);

	if (ctx->c_flags & KCPC_CTX_FREEZE)
		return;
	kcpc_hw_sample(ctx);
}

/*
 * Called when switching back to this thread
 */
void
kcpc_hw_restore(kcpc_ctx_t *ctx)
{
	uint64_t evsel;

	ASSERT(KCPC_VALID_CTX(ctx));

	if (ctx->c_flags & (KCPC_CTX_INVALID | KCPC_CTX_FREEZE))
		return;

	switch (x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		wrmsr(REG_PERFEVNT0, &allstopped);
		wrmsr(REG_PERFCTR0, &ctx->c_rawpic[0]);
		wrmsr(REG_PERFCTR1, &ctx->c_rawpic[1]);
		evsel = (uint64_t)ctx->c_event.ce_pes[1];
		wrmsr(REG_PERFEVNT1, &evsel);
		evsel = (uint64_t)(ctx->c_event.ce_pes[0] |
		    (UINT32_C(1) << CPC_P6_PES_EN));
		wrmsr(REG_PERFEVNT0, &evsel);
		(void) rdmsr(REG_TSC, &ctx->c_rawtsc);
		break;
	case X86_P5:
		wrmsr(P5_CESR, &allstopped);
		wrmsr(P5_CTR0, &ctx->c_rawpic[0]);
		wrmsr(P5_CTR1, &ctx->c_rawpic[1]);
		evsel = (uint64_t)ctx->c_event.ce_cesr;
		wrmsr(P5_CESR, &evsel);
		(void) rdmsr(REG_TSC, &ctx->c_rawtsc);
		break;
	default:
		break;
	}

	ctx->c_event.ce_hrt = gethrtime();

	/*
	 * The rdpmc instruction is only available on P6 and
	 * P5 with MMX processors.
	 */
	if ((x86_feature & X86_MMX) != 0) {
		uint32_t curcr4 = cr4();
		if (ctx->c_flags & KCPC_CTX_NONPRIV)
			setcr4(curcr4 | CR4_PCE);
		else
			setcr4(curcr4 & ~CR4_PCE);
	}
}

/*
 * Prepare for an event to be bound to the hardware on the next
 * call to kcpc_restore().
 */
int
kcpc_hw_bind(kcpc_ctx_t *ctx)
{
	ASSERT(KCPC_VALID_CTX(ctx));

	switch (x86_feature & X86_CPU_TYPE) {
		uint32_t *pes;
	case X86_P6:
		/*
		 * Check the interrupt specifications.
		 */
		pes = &ctx->c_event.ce_pes[0];
		if (ctx->c_flags & KCPC_CTX_SIGOVF) {
			/*
			 * We have to have set up the interrupt handler
			 * in the APIC module before we allow overflow
			 * interrupts to be programmed.
			 */
			if (kcpc_hw_overflow_intr_installed == 0)
				return (ENOTSUP);
			/*
			 * Don't allow interrupts to be generated if
			 * a counter is counting system events,
			 * otherwise we will damage lwp_pcb state.
			 */
			if ((pes[0] & (P6_PES_INT|P6_PES_OS)) ==
			    (P6_PES_INT|P6_PES_OS))
				return (EINVAL);
			if ((pes[1] & (P6_PES_INT|P6_PES_OS)) ==
			    (P6_PES_INT|P6_PES_OS))
				return (EINVAL);
		} else if (((pes[0] | pes[1]) & P6_PES_INT) == P6_PES_INT)
			return (EINVAL);
		pes[0] &= ~P6_PES_EN;
		pes[1] &= ~P6_PES_EN;
		break;
	case X86_P5:
		if (ctx->c_flags & KCPC_CTX_SIGOVF)
			return (ENOTSUP);
		break;
	default:
		return (ENOTSUP);
	}

	ctx->c_event.ce_hrt = gethrtime();

	ctx->c_event.ce_tsc = 0;
	ctx->c_rawpic[0] = trunc3931(ctx->c_event.ce_pic[0]);
	ctx->c_rawpic[1] = trunc3931(ctx->c_event.ce_pic[1]);

	return (0);
}

/*
 * Our caller ensures that users can't enable system event
 * counting when interrupts are being generated.
 */
void
kcpc_hw_setusrsys(kcpc_ctx_t *ctx, int usr, int on)
{
	int flagshift;
	int flagbit;

	switch (x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		flagshift = usr ? CPC_P6_PES_USR : CPC_P6_PES_OS;
		flagbit = UINT32_C(1) << flagshift;
		if (on) {
			ctx->c_event.ce_pes[0] |= flagbit;
			ctx->c_event.ce_pes[1] |= flagbit;
		} else {
			ctx->c_event.ce_pes[0] &= ~flagbit;
			ctx->c_event.ce_pes[1] &= ~flagbit;
		}
		break;
	case X86_P5:
		flagshift = usr ? CPC_P5_CESR_USR0 : CPC_P5_CESR_OS0;
		flagbit = UINT32_C(1) << flagshift;
		if (on) {
			ctx->c_event.ce_cesr |= flagbit;
			flagbit <<= CPC_P5_CESR_ES1_SHIFT;
			ctx->c_event.ce_cesr |= flagbit;
		} else {
			ctx->c_event.ce_cesr &= ~flagbit;
			flagbit <<= CPC_P5_CESR_ES1_SHIFT;
			ctx->c_event.ce_cesr &= ~flagbit;
		}
		break;
	default:
		break;
	}
}

/*
 * Clone a new context onto a new lwp.
 * Only need to inherit appropriate non-zero fields.
 */
void
kcpc_hw_clone(kcpc_ctx_t *ctx, kcpc_ctx_t *cctx)
{
	ASSERT(KCPC_VALID_CTX(ctx));

	cctx->c_event.ce_cpuver = ctx->c_event.ce_cpuver;
	atomic_or_uint(&cctx->c_flags, ctx->c_flags);
	cctx->c_event.ce_pes[0] = ctx->c_event.ce_pes[0];
	cctx->c_event.ce_pes[1] = ctx->c_event.ce_pes[1];
	cctx->c_event.ce_cesr = ctx->c_event.ce_cesr;
}

#include <sys/kobj.h>

/*
 * Test the hardware to ensure that we can actually
 * do performance counting on this platform
 */
int
kcpc_hw_probe(void)
{
	/*
	 * The "NCR version" of the pcplusmp module uses the
	 * performance counters as a watchdog timer.  In this
	 * case, we can't use the performance counters at all.
	 */
	if (kobj_getsymvalue("apic_watchdog_timer", 0) != 0)
		return (0);

	switch (x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		if ((x86_feature & X86_MSR) == X86_MSR)
			return (1);
		break;
	case X86_P5:
		if ((x86_feature & X86_MSR) == X86_MSR)
			return (1);
		break;
	case X86_K5:
	default:
		break;
	}
	return (0);
}

int
kcpc_hw_add_ovf_intr(uint_t (*handler)(caddr_t))
{
	if ((x86_feature & X86_CPU_TYPE) != X86_P6)
		return (-1);
	overflow_intr_handler = handler;
	return (0);
}

void
kcpc_hw_rem_ovf_intr(void)
{
	overflow_intr_handler = NULL;
}

#ifdef	DEBUG
/*
 * The scope and lifetime of thread specific data and the thread
 * context operators is, at best, complex.  This routine is used
 * to validate our usage in the DEBUG kernel.
 */
kcpc_ctx_t *
__ttocpcctx(kthread_t *t)
{
	kcpc_ctx_t *ctx;

	ctx = tsd_agent_get(t, kcpc_key);
	if (ctx)
		ASSERT(KCPC_VALID_CTX(ctx));
	return (ctx);
}
#endif	/* DEBUG */
