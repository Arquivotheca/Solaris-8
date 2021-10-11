/*
 * Copyright (c) 1988, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iu_simulator.c	1.21	97/07/09 SMI"

/* Integer Unit simulator for Sparc FPU simulator. */

#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>

#include <sys/regset.h>
#include <sys/privregs.h>
#ifdef  __sparcv9cpu
#include <sys/vis_simulator.h>
#endif

/*
 * fbcc also handles V9 fbpcc, and ignores the prediction bit.
 */
static enum ftt_type
fbcc(
	fp_inst_type    pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	kfpu_t		*pfpu)	/* Pointer to FPU register block. */

{
#ifdef	__sparcv9cpu
	fsr_type	fsr;
	int fbpcc = 0;
#else
	union {
		fsr_type	fsr;
		uint32_t	i;
	} f;
#endif
	union {
		fp_inst_type	fi;
		int32_t		i;	/* for sign_ext(disp22) */
	} fp;
	enum fcc_type	fcc;
	enum icc_type {
		fbn, fbne, fblg, fbul, fbl, fbug, fbg, fbu,
		fba, fbe, fbue, fbge, fbuge, fble, fbule, fbo
	} icc;

	u_int	annul, takeit;

#ifdef	__sparcv9cpu
	if (((pinst.op3 >> 3) & 0xf) == 5)
		fbpcc = 1;
	fsr.ll = pfpu->fpu_fsr;
	if (fbpcc) {
		u_int nfcc = (pinst.op3 >> 1) & 0x3;
		switch (nfcc) {
			case fcc_0:
				fcc = fsr.fcc0;
				break;
			case fcc_1:
				fcc = fsr.fcc1;
				break;
			case fcc_2:
				fcc = fsr.fcc2;
				break;
			case fcc_3:
				fcc = fsr.fcc3;
				break;
			}
	} else {
		fcc = fsr.fcc0;
	}
#else
	f.i = pfpu->fpu_fsr;
	fcc = f.fsr.fcc;
#endif
	icc = (enum icc_type) (pinst.rd & 0xf);
	annul = pinst.rd & 0x10;

	switch (icc) {
	case fbn:
		takeit = 0;
		break;
	case fbl:
		takeit = fcc == fcc_less;
		break;
	case fbg:
		takeit = fcc == fcc_greater;
		break;
	case fbu:
		takeit = fcc == fcc_unordered;
		break;
	case fbe:
		takeit = fcc == fcc_equal;
		break;
	case fblg:
		takeit = (fcc == fcc_less) || (fcc == fcc_greater);
		break;
	case fbul:
		takeit = (fcc == fcc_unordered) || (fcc == fcc_less);
		break;
	case fbug:
		takeit = (fcc == fcc_unordered) || (fcc == fcc_greater);
		break;
	case fbue:
		takeit = (fcc == fcc_unordered) || (fcc == fcc_equal);
		break;
	case fbge:
		takeit = (fcc == fcc_greater) || (fcc == fcc_equal);
		break;
	case fble:
		takeit = (fcc == fcc_less) || (fcc == fcc_equal);
		break;
	case fbne:
		takeit = fcc != fcc_equal;
		break;
	case fbuge:
		takeit = fcc != fcc_less;
		break;
	case fbule:
		takeit = fcc != fcc_greater;
		break;
	case fbo:
		takeit = fcc != fcc_unordered;
		break;
	case fba:
		takeit = 1;
		break;
	}
	if (takeit) {		/* Branch taken. */
		uintptr_t	tpc;

		fp.fi = pinst;
		tpc = pregs->r_pc;
		if (annul && (icc == fba)) {	/* fba,a is wierd */
#ifdef	__sparcv9cpu
			if (fbpcc) {
				pregs->r_pc = tpc +
					(int)((fp.i << 13) >> 11);
			} else {
#endif
				pregs->r_pc = tpc +
					(int)((fp.i << 10) >> 8);
#ifdef	__sparcv9cpu
			}
#endif
			pregs->r_npc = pregs->r_pc + 4;
		} else {
			pregs->r_pc = pregs->r_npc;
#ifdef	__sparcv9cpu
			if (fbpcc) {
				pregs->r_npc = tpc +
					(int)((fp.i << 13) >> 11);
			} else {
#endif
				pregs->r_npc = tpc +
					(int)((fp.i << 10) >> 8);
#ifdef	__sparcv9cpu
			}
#endif
		}
	} else {		/* Branch not taken. */
		if (annul) {	/* Annul next instruction. */
			pregs->r_pc = pregs->r_npc + 4;
			pregs->r_npc += 8;
		} else {	/* Execute next instruction. */
			pregs->r_pc = pregs->r_npc;
			pregs->r_npc += 4;
		}
	}
	return (ftt_none);
}

/* PUBLIC FUNCTIONS */

enum ftt_type
_fp_iu_simulator(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu)	/* Pointer to FPU register block. */
{
	switch (pinst.hibits) {
	case 0:				/* fbcc and V9 fbpcc */
		return (fbcc(pinst, pregs, pfpu));
#ifdef	__sparcv9cpu
	case 2:
		switch (pinst.op3) {
		case 0x28:
			if (pinst.rs1 == 0x13)
				return (vis_rdgsr(pfpsd, pinst, pregs,
					prw, pfpu));
			else
				return (ftt_unimplemented);
		case 0x30:
			if (pinst.rd == 0x13)
				return (vis_wrgsr(pfpsd, pinst, pregs,
					prw, pfpu));
			else
				return (ftt_unimplemented);
		case 0x2C:
			return (movcc(pfpsd, pinst, pregs, prw, pfpu));
		default:
			return (ftt_unimplemented);
	}
#endif
	case 3:
		return (fldst(pfpsd, pinst, pregs, prw, pfpu));
	default:
		return (ftt_unimplemented);
	}
}
