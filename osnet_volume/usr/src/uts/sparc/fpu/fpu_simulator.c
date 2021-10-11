/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fpu_simulator.c	1.41	98/11/03 SMI"

/* Main procedures for Sparc FPU simulator. */

#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/fpu/fpusystm.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>
#include <sys/atomic.h>

#include <sys/privregs.h>
#ifdef	__sparcv9cpu
#include <sys/vis_simulator.h>
#endif

/* PUBLIC FUNCTIONS */

int fp_notp = 1;	/* fp checking not a problem */

static enum ftt_type
_fp_fpu_simulator(
	fp_simd_type	*pfpsd,	/* Pointer to fpu simulator data */
	fp_inst_type	inst,	/* FPU instruction to simulate. */
	fsr_type	*pfsr)	/* Pointer to image of FSR to read and write. */
{
	unpacked	us1, us2, ud;	/* Unpacked operands and result. */
	uint32_t	nrs1, nrs2, nrd; /* Register number fields. */
	uint32_t	usr, andexcep;
	fsr_type	fsr;
	enum fcc_type	cc;
#ifdef	__sparcv9cpu
	uint32_t	nfcc;		/* fcc number field. */
	uint64_t	lusr;
#endif

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	fsr = *pfsr;
	pfpsd->fp_current_exceptions = 0;	/* Init current exceptions. */
	pfpsd->fp_fsrtem    = fsr.tem;		/* Obtain fsr's tem */
	pfpsd->fp_direction = fsr.rnd;		/* Obtain rounding direction. */
	pfpsd->fp_precision = fsr.rnp;		/* Obtain rounding precision. */
#ifdef	__sparcv9cpu
	nfcc = nrd & 0x3;
	if (inst.op3 == 0x35) {		/* fpop2 */
		fsr.cexc = 0;
		*pfsr = fsr;
		if ((inst.opcode & 0xf) == 0) {
			if ((fp_notp) && (inst.prec == 0))
				return (ftt_unimplemented);
			return (fmovcc(pfpsd, inst, pfsr));	/* fmovcc */
		} else if ((inst.opcode & 0x7) == 1) {
			if ((fp_notp) && (inst.prec == 0))
				return (ftt_unimplemented);
			return (fmovr(pfpsd, inst));		/* fmovr */
		}
	}
#endif
	/* ibit not valid for fpop1 instructions */
	if ((fp_notp) && (inst.ibit != 0))
		return (ftt_unimplemented);
	if ((fp_notp) && (inst.prec == 0)) {	/* fxto[sdq], fito[sdq] */
		if ((inst.opcode != flltos) &&
		    (inst.opcode != flltod) &&
		    (inst.opcode != flltox) &&
		    (inst.opcode != fitos) &&
		    (inst.opcode != fitod) &&
		    (inst.opcode != fitox)) {
			return (ftt_unimplemented);
		}
	}
	switch (inst.opcode) {
	case fmovs:		/* also covers fmovd, fmovq */
		if (inst.prec < 2) {	/* fmovs */
			_fp_unpack_word(pfpsd, &usr, nrs2);
			_fp_pack_word(pfpsd, &usr, nrd);
#ifdef	__sparcv9cpu
		} else {		/* fmovd */
			_fp_unpack_extword(pfpsd, &lusr, nrs2);
			_fp_pack_extword(pfpsd, &lusr, nrd);
			if (inst.prec > 2) {		/* fmovq */
				_fp_unpack_extword(pfpsd, &lusr, nrs2+2);
				_fp_pack_extword(pfpsd, &lusr, nrd+2);
			}
#endif
		}
		break;
	case fabss:		/* also covers fabsd, fabsq */
		if (inst.prec < 2) {	/* fabss */
			_fp_unpack_word(pfpsd, &usr, nrs2);
			usr &= 0x7fffffff;
			_fp_pack_word(pfpsd, &usr, nrd);
#ifdef	__sparcv9cpu
		} else {		/* fabsd */
			_fp_unpack_extword(pfpsd, &lusr, nrs2);
			lusr &= 0x7fffffffffffffff;
			_fp_pack_extword(pfpsd, &lusr, nrd);
			if (inst.prec > 2) {		/* fabsq */
				_fp_unpack_extword(pfpsd, &lusr, nrs2+2);
				_fp_pack_extword(pfpsd, &lusr, nrd+2);
			}
#endif
		}
		break;
	case fnegs:		/* also covers fnegd, fnegq */
		if (inst.prec < 2) {	/* fnegs */
			_fp_unpack_word(pfpsd, &usr, nrs2);
			usr ^= 0x80000000;
			_fp_pack_word(pfpsd, &usr, nrd);
#ifdef	__sparcv9cpu
		} else {		/* fnegd */
			_fp_unpack_extword(pfpsd, &lusr, nrs2);
			lusr ^= 0x8000000000000000;
			_fp_pack_extword(pfpsd, &lusr, nrd);
			if (inst.prec > 2) {		/* fnegq */
				_fp_unpack_extword(pfpsd, &lusr, nrs2+2);
				lusr ^= 0x0000000000000000;
				_fp_pack_extword(pfpsd, &lusr, nrd+2);
			}
#endif
		}
		break;
	case fadd:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_add(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, inst.prec);
		break;
	case fsub:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_sub(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, inst.prec);
		break;
	case fmul:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_mul(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, inst.prec);
		break;
	case fsmuld:
		if ((fp_notp) && (inst.prec != 1))
			return (ftt_unimplemented);
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_mul(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, (enum fp_op_type) ((int)inst.prec+1));
		break;
	case fdmulx:
		if ((fp_notp) && (inst.prec != 2))
			return (ftt_unimplemented);
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_mul(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, (enum fp_op_type) ((int)inst.prec+1));
		break;
	case fdiv:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		_fp_div(pfpsd, &us1, &us2, &ud);
		_fp_pack(pfpsd, &ud, nrd, inst.prec);
		break;
	case fcmp:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		cc = _fp_compare(pfpsd, &us1, &us2, 0);
		if (!(pfpsd->fp_current_exceptions & pfpsd->fp_fsrtem))
#ifndef	__sparcv9cpu
			fsr.fcc = cc;
#else
			switch (nfcc) {
			case fcc_0:
				fsr.fcc0 = cc;
				break;
			case fcc_1:
				fsr.fcc1 = cc;
				break;
			case fcc_2:
				fsr.fcc2 = cc;
				break;
			case fcc_3:
				fsr.fcc3 = cc;
				break;
			}
#endif
		break;
	case fcmpe:
		_fp_unpack(pfpsd, &us1, nrs1, inst.prec);
		_fp_unpack(pfpsd, &us2, nrs2, inst.prec);
		cc = _fp_compare(pfpsd, &us1, &us2, 1);
		if (!(pfpsd->fp_current_exceptions & pfpsd->fp_fsrtem))
#ifndef	__sparcv9cpu
			fsr.fcc = cc;
#else
			switch (nfcc) {
			case fcc_0:
				fsr.fcc0 = cc;
				break;
			case fcc_1:
				fsr.fcc1 = cc;
				break;
			case fcc_2:
				fsr.fcc2 = cc;
				break;
			case fcc_3:
				fsr.fcc3 = cc;
				break;
			}
#endif
		break;
	case fsqrt:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		_fp_sqrt(pfpsd, &us1, &ud);
		_fp_pack(pfpsd, &ud, nrd, inst.prec);
		break;
	case ftoi:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		pfpsd->fp_direction = fp_tozero;
		/* Force rounding toward zero. */
		_fp_pack(pfpsd, &us1, nrd, fp_op_int32);
		break;
#ifdef	__sparcv9cpu
	case ftoll:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		pfpsd->fp_direction = fp_tozero;
		/* Force rounding toward zero. */
		_fp_pack(pfpsd, &us1, nrd, fp_op_int64);
		break;
	case flltos:
		_fp_unpack(pfpsd, &us1, nrs2, fp_op_int64);
		_fp_pack(pfpsd, &us1, nrd, fp_op_single);
		break;
	case flltod:
		_fp_unpack(pfpsd, &us1, nrs2, fp_op_int64);
		_fp_pack(pfpsd, &us1, nrd, fp_op_double);
		break;
	case flltox:
		_fp_unpack(pfpsd, &us1, nrs2, fp_op_int64);
		_fp_pack(pfpsd, &us1, nrd, fp_op_extended);
		break;
#endif
	case fitos:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		_fp_pack(pfpsd, &us1, nrd, fp_op_single);
		break;
	case fitod:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		_fp_pack(pfpsd, &us1, nrd, fp_op_double);
		break;
	case fitox:
		_fp_unpack(pfpsd, &us1, nrs2, inst.prec);
		_fp_pack(pfpsd, &us1, nrd, fp_op_extended);
		break;
	default:
		return (ftt_unimplemented);
	}
	fsr.cexc = pfpsd->fp_current_exceptions;
	if (pfpsd->fp_current_exceptions) {	/* Exception(s) occurred. */
		andexcep = pfpsd->fp_current_exceptions & fsr.tem;
		if (andexcep != 0) {	/* Signal an IEEE SIGFPE here. */
			if (andexcep & (1 << fp_invalid)) {
				pfpsd->fp_trapcode = FPE_FLTINV;
				fsr.cexc = FSR_CEXC_NV;
			} else if (andexcep & (1 << fp_overflow)) {
				pfpsd->fp_trapcode = FPE_FLTOVF;
				fsr.cexc = FSR_CEXC_OF;
			} else if (andexcep & (1 << fp_underflow)) {
				pfpsd->fp_trapcode = FPE_FLTUND;
				fsr.cexc = FSR_CEXC_UF;
			} else if (andexcep & (1 << fp_division)) {
				pfpsd->fp_trapcode = FPE_FLTDIV;
				fsr.cexc = FSR_CEXC_DZ;
			} else if (andexcep & (1 << fp_inexact)) {
				pfpsd->fp_trapcode = FPE_FLTRES;
				fsr.cexc = FSR_CEXC_NX;
			} else {
				pfpsd->fp_trapcode = 0;
			}
			*pfsr = fsr;
			return (ftt_ieee);
		} else {	/* Just set accrued exception field. */
			fsr.aexc |= pfpsd->fp_current_exceptions;
		}
	}
	*pfsr = fsr;
	return (ftt_none);
}

/*
 * fpu_simulator simulates FPU instructions only;
 * reads and writes FPU data registers directly.
 */
enum ftt_type
fpu_simulator(
	fp_simd_type	*pfpsd,	/* Pointer to simulator data */
	fp_inst_type	*pinst,	/* Address of FPU instruction to simulate */
	fsr_type	*pfsr,	/* Pointer to image of FSR to read and write */
	uint32_t	inst)	/* The FPU instruction to simulate */
{
	union {
		uint32_t	i;
		fp_inst_type	inst;
	} fp;

	fp.i = inst;
	pfpsd->fp_trapaddr = (caddr_t)pinst;
	pfpsd->fp_current_read_freg = _fp_read_pfreg;
	pfpsd->fp_current_write_freg = _fp_write_pfreg;
#ifdef	__sparcv9cpu
	pfpsd->fp_current_read_dreg = _fp_read_pdreg;
	pfpsd->fp_current_write_dreg = _fp_write_pdreg;
#endif
	return (_fp_fpu_simulator(pfpsd, fp.inst, pfsr));
}

/*
 * fp_emulator simulates FPU and CPU-FPU instructions; reads and writes FPU
 * data registers from image in pfpu.
 */
enum ftt_type
fp_emulator(
	fp_simd_type	*pfpsd,	/* Pointer to simulator data */
	fp_inst_type	*pinst,	/* Pointer to FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu)	/* Pointer to FPU register block. */
{
	klwp_id_t lwp = ttolwp(curthread);
	union {
		uint32_t	i;
		fp_inst_type	inst;
	} fp;
	enum ftt_type	ftt;
#ifdef	__sparcv9cpu
	struct v9_fpu *pfp = lwptofpu(lwp);
	V9_FPU_FSR_TYPE	tfsr;
	uint32_t opf;
#else
	FPU_FSR_TYPE	tfsr;
#endif

	tfsr = pfpu->fpu_fsr;
	pfpsd->fp_current_pfregs = pfpu;
	pfpsd->fp_current_read_freg = _fp_read_vfreg;
	pfpsd->fp_current_write_freg = _fp_write_vfreg;
#ifdef	__sparcv9cpu
	pfpsd->fp_current_read_dreg = _fp_read_vdreg;
	pfpsd->fp_current_write_dreg = _fp_write_vdreg;
#endif
	pfpsd->fp_trapaddr = (caddr_t)pinst; /* bad inst addr in case we trap */
	ftt = _fp_read_inst((uint32_t *)pinst, &(fp.i), pfpsd);
	if (ftt != ftt_none)
		return (ftt);

	if ((fp.inst.hibits == 2) &&
	    ((fp.inst.op3 == 0x34) || (fp.inst.op3 == 0x35))) {
		ftt = _fp_fpu_simulator(pfpsd, fp.inst, (fsr_type *)&tfsr);
		/* Do not retry emulated instruction. */
		pregs->r_pc = pregs->r_npc;
		pregs->r_npc += 4;
		pfpu->fpu_fsr = tfsr;
		if (ftt != ftt_none) {
			/*
			 * Simulation generated an exception of some kind,
			 * simulate the fp queue for a signal.
			 */
			pfpu->fpu_q->FQu.fpq.fpq_addr = (uint32_t *)pinst;
			pfpu->fpu_q->FQu.fpq.fpq_instr = fp.i;
			pfpu->fpu_qcnt = 1;
		}

#ifdef	__sparcv9cpu	/* implementation dependent instructions */
	} else if ((fp.inst.hibits == 2) && (fp.inst.op3 == 0x36)) {
		opf = (fp.i >> 5) & 0x1ff;
		if (opf > 0x14) {
			ftt = vis_fpu_simulator(pfpsd, fp.inst,
						pregs, prw, pfp);
		} else {	/* integer only instructions */
			return (ftt);
		}
#endif
	} else
		ftt = _fp_iu_simulator(pfpsd, fp.inst, pregs, prw, pfpu);

	if (ftt != ftt_none)
		return (ftt);

	/*
	 * If we are single-stepping, don't emulate any more instructions.
	 */
	if (lwp->lwp_pcb.pcb_step != STEP_NONE)
		return (ftt);
again:
	/*
	 * now read next instruction and see if it can be emulated
	 */
	pinst = (fp_inst_type *)pregs->r_pc;
	pfpsd->fp_trapaddr = (caddr_t)pinst; /* bad inst addr in case we trap */
	ftt = _fp_read_inst((uint32_t *)pinst, &(fp.i), pfpsd);
	if (ftt != ftt_none)
		return (ftt);
	if ((fp.inst.hibits == 2) &&		/* fpops */
	    ((fp.inst.op3 == 0x34) || (fp.inst.op3 == 0x35))) {
		ftt = _fp_fpu_simulator(pfpsd, fp.inst, (fsr_type *)&tfsr);
		/* Do not retry emulated instruction. */
		pfpu->fpu_fsr = tfsr;
		pregs->r_pc = pregs->r_npc;
		pregs->r_npc += 4;
		if (ftt != ftt_none) {
			/*
			 * Simulation generated an exception of some kind,
			 * simulate the fp queue for a signal.
			 */
			pfpu->fpu_q->FQu.fpq.fpq_addr = (uint32_t *)pinst;
			pfpu->fpu_q->FQu.fpq.fpq_instr = fp.i;
			pfpu->fpu_qcnt = 1;
		}
#ifdef	__sparcv9cpu	/* implementation dependent instructions */
	} else if ((fp.inst.hibits == 2) && (fp.inst.op3 == 0x36)) {
		opf = (fp.i >> 5) & 0x1ff;
		if (opf > 0x14) {
			ftt = vis_fpu_simulator(pfpsd, fp.inst,
						pregs, prw, pfp);
		} else {	/* integer only instructions */
			return (ftt);
		}
#endif
	} else if (
#ifdef	__sparcv9cpu
						/* rd %gsr */
	    ((fp.inst.hibits == 2) && ((fp.inst.op3 & 0x3f) == 0x28) &&
			(fp.inst.rs1 == 0x13)) ||
						/* wr %gsr */
	    ((fp.inst.hibits == 2) && ((fp.inst.op3 & 0x3f) == 0x30) &&
			(fp.inst.rd == 0x13)) ||
						/* movcc */
	    ((fp.inst.hibits == 2) && ((fp.inst.op3 & 0x3f) == 0x2c) &&
			(((fp.i>>18) & 0x1) == 0)) ||
						/* fbpcc */
	    ((fp.inst.hibits == 0) && (((fp.i>>22) & 0x7) == 5)) ||
#endif
						/* fldst */
	    ((fp.inst.hibits == 3) && ((fp.inst.op3 & 0x38) == 0x20)) ||
						/* fbcc */
	    ((fp.inst.hibits == 0) && (((fp.i>>22) & 0x7) == 6))) {
		ftt = _fp_iu_simulator(pfpsd, fp.inst, pregs, prw, pfpu);
	} else
		return (ftt);

	if (ftt != ftt_none)
		return (ftt);
	else
		goto again;
}

/*
 * FPU simulator global kstat data
 */
static struct {
	struct kstat_named		fpu_ieee_traps;
	struct kstat_named		fpu_unfinished_traps;
	struct kstat_named		fpu_unimplemented_traps;
} fpustat = {
	{ "fpu_ieee_traps",		KSTAT_DATA_UINT64 },
	{ "fpu_unfinished_traps",	KSTAT_DATA_UINT64 },
	{ "fpu_unimplemented",		KSTAT_DATA_UINT64 },
};

kstat_t *fpu_kstat = NULL;

void
fp_kstat_init(void)
{
	const uint_t fpustat_ndata = sizeof (fpustat) / sizeof (kstat_named_t);

	ASSERT(fpu_kstat == NULL);
	if ((fpu_kstat = kstat_create("unix", 0, "fpu_traps", "misc",
	    KSTAT_TYPE_NAMED, fpustat_ndata, KSTAT_FLAG_VIRTUAL)) == NULL) {
		cmn_err(CE_WARN, "CPU%d: kstat_create for fpu_traps failed",
		    CPU->cpu_id);
	} else {
		fpu_kstat->ks_data = (void *)&fpustat;
		kstat_install(fpu_kstat);
	}
}

void
fp_kstat_update(enum ftt_type ftt)
{
	ASSERT((ftt == ftt_ieee) || (ftt == ftt_unfinished) ||
	    (ftt == ftt_unimplemented));
	if (ftt == ftt_ieee)
		atomic_add_64(&fpustat.fpu_ieee_traps.value.ui64, 1);
	else if (ftt == ftt_unfinished)
		atomic_add_64(&fpustat.fpu_unfinished_traps.value.ui64, 1);
	else if (ftt == ftt_unimplemented)
		atomic_add_64(&fpustat.fpu_unimplemented_traps.value.ui64, 1);
}
