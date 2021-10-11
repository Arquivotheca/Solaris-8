/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpc_subr.c	1.2	99/11/20 SMI"

/*
 * UltraSPARC-specific routines used by the CPU Performance counter driver.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/thread.h>
#include <sys/regset.h>
#include <sys/archsystm.h>
#include <sys/cpc_event.h>
#include <sys/cpc_impl.h>
#include <sys/cpc_ultra.h>
#include <sys/sunddi.h>

/*
 * Virtualizes the two 32-bit fields of the %pic
 * register into two 64-bit software registers, and
 * samples hrtime, and relative %tick and hrtime values.
 */
void
kcpc_hw_sample(kcpc_ctx_t *ctx)
{
	uint64_t curtick, curpic;
	int64_t diff;

	ASSERT(KCPC_VALID_CTX(ctx));

	curpic = ultra_getpic();

	curtick = gettick();
	ctx->c_event.ce_hrt = gethrtime();

	ctx->c_event.ce_tick += curtick - ctx->c_rawtick;
	ctx->c_rawtick = curtick;

	diff = (uint64_t)(uint32_t)curpic -
	    (uint64_t)(uint32_t)ctx->c_rawpic;
	if (diff < 0)
		diff += (1ll << 32);
	ctx->c_event.ce_pic[0] += diff;

	diff = (uint64_t)(uint32_t)(curpic >> 32) -
	    (uint64_t)(uint32_t)(ctx->c_rawpic >> 32);
	if (diff < 0)
		diff += (1ll << 32);
	ctx->c_event.ce_pic[1] += diff;

	ctx->c_rawpic = curpic;
}

#define	ULTRA_PCR_SYS		(UINT64_C(1) << CPC_ULTRA_PCR_SYS)
#define	ULTRA_PCR_PRIVPIC	(UINT64_C(1) << CPC_ULTRA_PCR_PRIVPIC)

static uint64_t allstopped = ULTRA_PCR_PRIVPIC;
static uint_t (*overflow_intr_handler)(caddr_t);

int kcpc_hw_overflow_intr_installed;

/*
 * The current thread context had an overflow; we're executing
 * here in in high-level interrupt context.
 * (Should be called out of the level15 interrupt handler)
 */
uint_t
kcpc_hw_overflow_intr(caddr_t arg)
{
	if (overflow_intr_handler != NULL) {
		if (overflow_intr_handler(arg)) {
			/*
			 * Disable the counters
			 */
			ultra_setpcr(allstopped);
		}
	}
	return (DDI_INTR_CLAIMED);
}

/*
 * Now we're being called from trap from an AST on the way out
 * of the kernel.  On UltraSPARC 3, we can't tell which register
 * overflowed, so we can't use the overflow to automatically propagate
 * the carry bit into our virtualized counter(s).
 *
 * So, if we're expecting a signal to be delivered, sample the
 * counters and leave them frozen.  This assumes the caller has
 * additional semantic knowledge about the overflow that can resolve
 * the ambiguity.
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
	ultra_setpcr(allstopped);
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
	ASSERT(KCPC_VALID_CTX(ctx));

	if (ctx->c_flags & (KCPC_CTX_INVALID | KCPC_CTX_FREEZE))
		return;
	ultra_setpcr(allstopped);
	ultra_setpic(ctx->c_rawpic);
	ultra_setpcr(ctx->c_event.ce_pcr);
	ctx->c_rawtick = gettick();
}

int
kcpc_hw_bind(kcpc_ctx_t *ctx)
{
	ASSERT(KCPC_VALID_CTX(ctx));

	if (ctx->c_flags & KCPC_CTX_SIGOVF) {
		/*
		 * We have to have set up the interrupt handler
		 * in the interrupt code before we allow overflow
		 * interrupts to be programmed.
		 */
		if (kcpc_hw_overflow_intr_installed == 0)
			return (ENOTSUP);	/* Not until UltraSPARC III */
		/*
		 * Don't allow signals to be generated if the counters
		 * are counting system events, otherwise we will damage
		 * lwp_pcb state.
		 */
		if ((ctx->c_event.ce_pcr & ULTRA_PCR_SYS) == ULTRA_PCR_SYS)
			return (EINVAL);
	}
	if (ctx->c_flags & KCPC_CTX_NONPRIV)
		ctx->c_event.ce_pcr &= ~ULTRA_PCR_PRIVPIC;
	else
		ctx->c_event.ce_pcr |= ULTRA_PCR_PRIVPIC;

	ctx->c_event.ce_tick = INT64_C(0);
	ctx->c_event.ce_hrt = gethrtime();
	ctx->c_rawpic =
	    ((uint64_t)(uint32_t)ctx->c_event.ce_pic[1]) << 32 |
	    ((uint64_t)(uint32_t)ctx->c_event.ce_pic[0]);

	return (0);
}

/*
 * Our caller ensures that users can't enable system event
 * counting when interrupts are being generated.
 */
void
kcpc_hw_setusrsys(kcpc_ctx_t *ctx, int usr, int on)
{
	int flagbit = usr ? CPC_ULTRA_PCR_USR : CPC_ULTRA_PCR_SYS;

	if (on)
		ctx->c_event.ce_pcr |= (UINT64_C(1) << flagbit);
	else
		ctx->c_event.ce_pcr &= ~(UINT64_C(1) << flagbit);
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
	cctx->c_event.ce_pcr = ctx->c_event.ce_pcr;
}

/*
 * Test the hardware to ensure that we can actually
 * do performance counting on this platform
 */
int
kcpc_hw_probe(void)
{
	return (1);	/* true because we're a v9plus/v8plus binary */
}

int
kcpc_hw_add_ovf_intr(uint_t (*handler)(caddr_t))
{
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
