/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)visinstr.c	1.11	99/03/12 SMI"

/* VIS floating point instruction simulator for Sparc FPU simulator. */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/fpu/fpu_simulator.h>
#include <sys/vis_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/privregs.h>
#include <sys/spitasi.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/cpu_module.h>
#include <sys/systm.h>

#define	FPU_REG_FIELD uint32_reg	/* Coordinate with FPU_REGS_TYPE. */
#define	FPU_DREG_FIELD uint64_reg	/* Coordinate with FPU_DREGS_TYPE. */
#define	FPU_FSR_FIELD uint64_reg	/* Coordinate with V9_FPU_FSR_TYPE. */

static enum ftt_type vis_alignaddr(fp_simd_type *, vis_inst_type, struct regs *,
				void *, struct v9_fpu *);
static enum ftt_type vis_faligndata(fp_simd_type *, fp_inst_type,
				struct v9_fpu *);
static enum ftt_type vis_fcmp(fp_simd_type *, vis_inst_type, struct regs *,
				void *);
static enum ftt_type vis_fmul(fp_simd_type *, vis_inst_type);
static enum ftt_type vis_fpixel(fp_simd_type *, vis_inst_type, struct v9_fpu *);
static enum ftt_type vis_fpaddsub(fp_simd_type *, vis_inst_type);
static enum ftt_type vis_pdist(fp_simd_type *, fp_inst_type);
static enum ftt_type vis_prtl_fst(fp_simd_type *, vis_inst_type, struct regs *,
				void *, kfpu_t *, u_int);
static enum ftt_type vis_short_fls(fp_simd_type *, vis_inst_type, struct regs *,
				void *, kfpu_t *, u_int);
static enum ftt_type vis_blk_fldst(fp_simd_type *, vis_inst_type, struct regs *,
				void *, kfpu_t *, u_int);

/*
 * Simulator for VIS instructions with op3 == 0x36 that get fp_disabled
 * traps.
 */
enum ftt_type
vis_fpu_simulator(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*fp)	/* Need to fp to access gsr reg */
{
	enum ftt_type ftt = ftt_none;
	union {
		vis_inst_type	inst;
		fp_inst_type	pinst;
	} f;
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	u_int	us1, us2, usr;
	uint64_t lus1, lus2, lusr;

	ASSERT(USERMODE(pregs->r_tstate));
	nrs1 = pinst.rs1;
	nrs2 = pinst.rs2;
	nrd = pinst.rd;
	f.pinst = pinst;
	if ((f.inst.opf & 1) == 0) {		/* double precision */
		if ((nrs1 & 1) == 1) 		/* fix register encoding */
			nrs1 = (nrs1 & 0x1e) | 0x20;
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		if ((nrd & 1) == 1)
			nrd = (nrd & 0x1e) | 0x20;
	}

	switch (f.inst.opf) {
		/* these instr's do not use fp regs */
	case edge8:
	case edge8l:
	case edge16:
	case edge16l:
	case edge32:
	case edge32l:
	case array8:
	case array16:
	case array32:
		return (ftt_none);

	case alignaddr:
	case alignaddrl:
		ftt = vis_alignaddr(pfpsd, f.inst, pregs, prw, fp);
		break;
	case fcmple16:
	case fcmpne16:
	case fcmpgt16:
	case fcmpeq16:
	case fcmple32:
	case fcmpne32:
	case fcmpgt32:
	case fcmpeq32:
		ftt = vis_fcmp(pfpsd, f.inst, pregs, prw);
		break;
	case fmul8x16:
	case fmul8x16au:
	case fmul8x16al:
	case fmul8sux16:
	case fmul8ulx16:
	case fmuld8sux16:
	case fmuld8ulx16:
		ftt = vis_fmul(pfpsd, f.inst);
		break;
	case fpack16:
	case fpack32:
	case fpackfix:
	case fexpand:
	case fpmerge:
		ftt = vis_fpixel(pfpsd, f.inst, fp);
		break;
	case pdist:
		ftt = vis_pdist(pfpsd, pinst);
		break;
	case faligndata:
		ftt = vis_faligndata(pfpsd, pinst, fp);
		break;
	case fpadd16:
	case fpadd16s:
	case fpadd32:
	case fpadd32s:
	case fpsub16:
	case fpsub16s:
	case fpsub32:
	case fpsub32s:
		ftt = vis_fpaddsub(pfpsd, f.inst);
		break;
	case fzero:
		lusr = 0;
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fzeros:
		usr = 0;
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fnor:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = ~(lus1 | lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fnors:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = ~(us1 | us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fandnot2:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (lus1 & ~lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fandnot2s:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (us1 & ~us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fnot2:
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = ~lus2;
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fnot2s:
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = ~us2;
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fandnot1:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (~lus1 & lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fandnot1s:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (~us1 & us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fnot1:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		lusr = ~lus1;
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fnot1s:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		usr = ~us1;
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fxor:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (lus1 ^ lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fxors:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (us1 ^ us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fnand:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = ~(lus1 & lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fnands:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = ~(us1 & us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fand:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (lus1 & lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fands:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (us1 & us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fxnor:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = ~(lus1 ^ lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fxnors:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = ~(us1 ^ us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fsrc1:
		_fp_unpack_extword(pfpsd, &lusr, nrs1);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fsrc1s:
		_fp_unpack_word(pfpsd, &usr, nrs1);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fornot2:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (lus1 | ~lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fornot2s:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (us1 | ~us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fsrc2:
		_fp_unpack_extword(pfpsd, &lusr, nrs2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fsrc2s:
		_fp_unpack_word(pfpsd, &usr, nrs2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fornot1:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (~lus1 | lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fornot1s:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (~us1 | us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case for_op:
		_fp_unpack_extword(pfpsd, &lus1, nrs1);
		_fp_unpack_extword(pfpsd, &lus2, nrs2);
		lusr = (lus1 | lus2);
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fors_op:
		_fp_unpack_word(pfpsd, &us1, nrs1);
		_fp_unpack_word(pfpsd, &us2, nrs2);
		usr = (us1 | us2);
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	case fone:
		lusr = 0xffffffffffffffff;
		_fp_pack_extword(pfpsd, &lusr, nrd);
		break;
	case fones:
		usr = 0xffffffffUL;
		_fp_pack_word(pfpsd, &usr, nrd);
		break;
	default:
		return (ftt_unimplemented);
	}

	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt);
}

/*
 * Simulator for alignaddr and alignaddrl instructions.
 */
static enum ftt_type
vis_alignaddr(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	struct v9_fpu	*fp)	/* Need to fp to access gsr reg */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	enum ftt_type ftt;
	uint64_t ea, tea, g, r;
	short s;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;

	ftt = read_iureg(pfpsd, nrs1, pregs, prw, &ea);
	if (ftt != ftt_none)
		return (ftt);
	ftt = read_iureg(pfpsd, nrs2, pregs, prw, &tea);
	if (ftt != ftt_none)
		return (ftt);
	ea += tea;
	r = (ea >> 3) << 3;	/* zero least 3 significant bits */
	ftt = write_iureg(pfpsd, nrd, pregs, prw, &r);

	g = get_gsr(fp);
	g &= 0x78;		/* zero the alignaddr_offset */
	r = ea & 0x7; 		/* store lower 3 bits in gsr save area */
	if (inst.opf == alignaddrl) {
		s = (short)(~r);	/* 2's complement for alignaddrl */
		if (s < 0)
			r = (uint64_t)((s + 1) & 0x7);
		else
			r = (uint64_t)(s & 0x7);
	}
	g |= r;
	set_gsr(g, fp);

	return (ftt);
}

/*
 * Simulator for fp[add|sub]* instruction.
 */
static enum ftt_type
vis_fpaddsub(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst)	/* FPU instruction to simulate. */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
	} lrs1, lrs2, lrd;
	union {
		uint32_t	i;
		uint16_t	s[2];
	} krs1, krs2, krd;
	int i;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((inst.opf & 1) == 0) {	/* double precision */
		if ((nrs1 & 1) == 1) 	/* fix register encoding */
			nrs1 = (nrs1 & 0x1e) | 0x20;
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		if ((nrd & 1) == 1)
			nrd = (nrd & 0x1e) | 0x20;
	}

	switch (inst.opf) {
	case fpadd16:
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 3; i++) {
			lrd.s[i] = lrs1.s[i] + lrs2.s[i];
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpadd16s:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 1; i++) {
			krd.s[i] = krs1.s[i] + krs2.s[i];
		}
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	case fpadd32:
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 1; i++) {
			lrd.i[i] = lrs1.i[i] + lrs2.i[i];
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpadd32s:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		krd.i = krs1.i + krs2.i;
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	case fpsub16:
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 3; i++) {
			lrd.s[i] = lrs1.s[i] - lrs2.s[i];
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpsub16s:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 1; i++) {
			krd.s[i] = krs1.s[i] - krs2.s[i];
		}
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	case fpsub32:
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 1; i++) {
			lrd.i[i] = lrs1.i[i] - lrs2.i[i];
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpsub32s:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		krd.i = krs1.i - krs2.i;
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	}
	return (ftt_none);
}

/*
 * Simulator for fcmp* instruction.
 */
static enum ftt_type
vis_fcmp(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw)	/* Pointer to locals and ins. */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
	} krs1, krs2, krd;
	enum ftt_type ftt;
	short sr1, sr2;
	int i, ir1, ir2;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	krd.ll = 0;
	if ((nrs1 & 1) == 1) 	/* fix register encoding */
		nrs1 = (nrs1 & 0x1e) | 0x20;
	if ((nrs2 & 1) == 1)
		nrs2 = (nrs2 & 0x1e) | 0x20;

	_fp_unpack_extword(pfpsd, &krs1.ll, nrs1);
	_fp_unpack_extword(pfpsd, &krs2.ll, nrs2);
	switch (inst.opf) {
	case fcmple16:
		for (i = 0; i <= 3; i++) {
			sr1 = (short)krs1.s[i];
			sr2 = (short)krs2.s[i];
			if (sr1 <= sr2)
				krd.ll += (0x8 >> i);
		}
		break;
	case fcmpne16:
		for (i = 0; i <= 3; i++) {
			sr1 = (short)krs1.s[i];
			sr2 = (short)krs2.s[i];
			if (sr1 != sr2)
				krd.ll += (0x8 >> i);
		}
		break;
	case fcmpgt16:
		for (i = 0; i <= 3; i++) {
			sr1 = (short)krs1.s[i];
			sr2 = (short)krs2.s[i];
			if (sr1 > sr2)
				krd.ll += (0x8 >> i);
		}
		break;
	case fcmpeq16:
		for (i = 0; i <= 3; i++) {
			sr1 = (short)krs1.s[i];
			sr2 = (short)krs2.s[i];
			if (sr1 == sr2)
				krd.ll += (0x8 >> i);
		}
		break;
	case fcmple32:
		for (i = 0; i <= 1; i++) {
			ir1 = (int)krs1.i[i];
			ir2 = (int)krs2.i[i];
			if (ir1 <= ir2)
				krd.ll += (0x2 >> i);
		}
		break;
	case fcmpne32:
		for (i = 0; i <= 1; i++) {
			ir1 = (int)krs1.i[i];
			ir2 = (int)krs2.i[i];
			if (ir1 != ir2)
				krd.ll += (0x2 >> i);
		}
		break;
	case fcmpgt32:
		for (i = 0; i <= 1; i++) {
			ir1 = (int)krs1.i[i];
			ir2 = (int)krs2.i[i];
			if (ir1 > ir2)
				krd.ll += (0x2 >> i);
		}
		break;
	case fcmpeq32:
		for (i = 0; i <= 1; i++) {
			ir1 = (int)krs1.i[i];
			ir2 = (int)krs2.i[i];
			if (ir1 == ir2)
				krd.ll += (0x2 >> i);
		}
		break;
	}
	ftt = write_iureg(pfpsd, nrd, pregs, prw, &krd.ll);
	return (ftt);
}

/*
 * Simulator for fmul* instruction.
 */
static enum ftt_type
vis_fmul(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst)	/* FPU instruction to simulate. */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} lrs1, lrs2, lrd;
	union {
		uint32_t	i;
		uint16_t	s[2];
		uint8_t		c[4];
	} krs1, krs2, kres;
	short s1, s2, sres;
	u_short us1;
	char c1;
	int i;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((inst.opf & 1) == 0) {	/* double precision */
		if ((nrd & 1) == 1) 	/* fix register encoding */
			nrd = (nrd & 0x1e) | 0x20;
	}

	switch (inst.opf) {
	case fmul8x16:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 3; i++) {
			us1 = (u_short)krs1.c[i];
			s2 = (short)lrs2.s[i];
			kres.i = us1 * s2;
			sres = (short)((kres.c[1] << 8) | kres.c[2]);
			if (kres.c[3] >= 0x80)
				sres++;
			lrd.s[i] = sres;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmul8x16au:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 3; i++) {
			us1 = (u_short)krs1.c[i];
			s2 = (short)krs2.s[0];
			kres.i = us1 * s2;
			sres = (short)((kres.c[1] << 8) | kres.c[2]);
			if (kres.c[3] >= 0x80)
				sres++;
			lrd.s[i] = sres;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmul8x16al:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 3; i++) {
			us1 = (u_short)krs1.c[i];
			s2 = (short)krs2.s[1];
			kres.i = us1 * s2;
			sres = (short)((kres.c[1] << 8) | kres.c[2]);
			if (kres.c[3] >= 0x80)
				sres++;
			lrd.s[i] = sres;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmul8sux16:
		if ((nrs1 & 1) == 1) 	/* fix register encoding */
			nrs1 = (nrs1 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 3; i++) {
			c1 = lrs1.c[(i*2)];
			s1 = (short)c1;		/* keeps the sign alive */
			s2 = (short)lrs2.s[i];
			kres.i = s1 * s2;
			sres = (short)((kres.c[1] << 8) | kres.c[2]);
			if (kres.c[3] >= 0x80)
				sres++;
			if (sres < 0)
				lrd.s[i] = (sres & 0xFFFF);
			else
				lrd.s[i] = sres;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmul8ulx16:
		if ((nrs1 & 1) == 1) 	/* fix register encoding */
			nrs1 = (nrs1 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		for (i = 0; i <= 3; i++) {
			us1 = (u_short)lrs1.c[(i*2)+1];
			s2 = (short)lrs2.s[i];
			kres.i = us1 * s2;
			sres = (short)kres.s[0];
			if (kres.s[1] >= 0x8000)
				sres++;
			lrd.s[i] = sres;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmuld8sux16:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 1; i++) {
			c1 = krs1.c[(i*2)];
			s1 = (short)c1;		/* keeps the sign alive */
			s2 = (short)krs2.s[i];
			kres.i = s1 * s2;
			lrd.i[i] = kres.i << 8;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fmuld8ulx16:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 1; i++) {
			us1 = (u_short)krs1.c[(i*2)+1];
			s2 = (short)krs2.s[i];
			lrd.i[i] = us1 * s2;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	}
	return (ftt_none);
}

/*
 * Simulator for fpixel formatting instructions.
 */
static enum ftt_type
vis_fpixel(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* FPU instruction to simulate. */
	struct v9_fpu	*fp)	/* Need to fp to access gsr reg */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	int	i, j, k, sf;
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} lrs1, lrs2, lrd;
	union {
		uint32_t	i;
		uint16_t	s[2];
		uint8_t		c[4];
	} krs1, krs2, krd;
	uint64_t r;
	int64_t l, m;
	short s;
	u_char uc;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((inst.opf != fpack16) && (inst.opf != fpackfix)) {
		if ((nrd & 1) == 1) 	/* fix register encoding */
			nrd = (nrd & 0x1e) | 0x20;
	}

	switch (inst.opf) {
	case fpack16:
		if ((nrs2 & 1) == 1) 	/* fix register encoding */
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		r = get_gsr(fp);
		sf = (int)((r >> 3) & 0xf);
		for (i = 0; i <= 3; i++) {
			s = (short)lrs2.s[i];	/* preserve the sign */
			j = ((int)s << sf);
			k = j >> 7;
			if (k < 0) {
				uc = 0;
			} else if (k > 255) {
				uc = 255;
			} else {
				uc = (u_char)k;
			}
			krd.c[i] = uc;
		}
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	case fpack32:
		if ((nrs1 & 1) == 1) 	/* fix register encoding */
			nrs1 = (nrs1 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		r = get_gsr(fp);
		sf = (int)((r >> 3) & 0xf);
		lrd.ll = lrs1.ll << 8;
		for (i = 0, k = 3; i <= 1; i++, k += 4) {
			j = (int)lrs2.i[i];	/* preserve the sign */
			l = ((int64_t)j << sf);
			m = l >> 23;
			if (m < 0) {
				uc = 0;
			} else if (m > 255) {
				uc = 255;
			} else {
				uc = (u_char)m;
			}
			lrd.c[k] = uc;
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpackfix:
		if ((nrs2 & 1) == 1)
			nrs2 = (nrs2 & 0x1e) | 0x20;
		_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
		r = get_gsr(fp);
		sf = (int)((r >> 3) & 0xf);
		for (i = 0; i <= 1; i++) {
			j = (int)lrs2.i[i];	/* preserve the sign */
			l = ((int64_t)j << sf);
			m = l >> 16;
			if (m < -32768) {
				s = -32768;
			} else if (m > 32767) {
				s = 32767;
			} else {
				s = (short)m;
			}
			krd.s[i] = s;
		}
		_fp_pack_word(pfpsd, &krd.i, nrd);
		break;
	case fexpand:
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0; i <= 3; i++) {
			uc = krs2.c[i];
			lrd.s[i] = (u_short)(uc << 4);
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	case fpmerge:
		_fp_unpack_word(pfpsd, &krs1.i, nrs1);
		_fp_unpack_word(pfpsd, &krs2.i, nrs2);
		for (i = 0, j = 0; i <= 3; i++, j += 2) {
			lrd.c[j] = krs1.c[i];
			lrd.c[j+1] = krs2.c[i];
		}
		_fp_pack_extword(pfpsd, &lrd.ll, nrd);
		break;
	}
	return (ftt_none);
}

/*
 * Simulator for pdist instruction.
 */
enum ftt_type
vis_pdist(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst)	/* FPU instruction to simulate. */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	int	i;
	short	s;
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} lrs1, lrs2, lrd;

	nrs1 = pinst.rs1;
	nrs2 = pinst.rs2;
	nrd = pinst.rd;
	if ((nrs1 & 1) == 1) 		/* fix register encoding */
		nrs1 = (nrs1 & 0x1e) | 0x20;
	if ((nrs2 & 1) == 1)
		nrs2 = (nrs2 & 0x1e) | 0x20;
	if ((nrd & 1) == 1)
		nrd = (nrd & 0x1e) | 0x20;

	_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
	_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);
	_fp_unpack_extword(pfpsd, &lrd.ll, nrd);

	for (i = 0; i <= 7; i++) {
		s = (short)(lrs1.c[i] - lrs2.c[i]);
		if (s < 0)
			s = ~s + 1;
		lrd.ll += s;
	}

	_fp_pack_extword(pfpsd, &lrd.ll, nrd);
	return (ftt_none);
}

/*
 * Simulator for faligndata instruction.
 */
static enum ftt_type
vis_faligndata(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct v9_fpu	*fp)	/* Need to fp to access gsr reg */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	int	i, j, k, ao;
	union {
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} lrs1, lrs2, lrd;
	uint64_t r;

	nrs1 = pinst.rs1;
	nrs2 = pinst.rs2;
	nrd = pinst.rd;
	if ((nrs1 & 1) == 1) 		/* fix register encoding */
		nrs1 = (nrs1 & 0x1e) | 0x20;
	if ((nrs2 & 1) == 1)
		nrs2 = (nrs2 & 0x1e) | 0x20;
	if ((nrd & 1) == 1)
		nrd = (nrd & 0x1e) | 0x20;

	_fp_unpack_extword(pfpsd, &lrs1.ll, nrs1);
	_fp_unpack_extword(pfpsd, &lrs2.ll, nrs2);

	r = get_gsr(fp);
	ao = (int)(r & 0x7);

	for (i = 0, j = ao, k = 0; i <= 7; i++)
		if (j <= 7) {
			lrd.c[i] = lrs1.c[j++];
		} else {
			lrd.c[i] = lrs2.c[k++];
		}
	_fp_pack_extword(pfpsd, &lrd.ll, nrd);

	return (ftt_none);
}

/*
 * Simulator for VIS loads and stores between floating-point unit and memory.
 */
enum ftt_type
vis_fldst(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu,	/* Pointer to FPU register block. */
	u_int		asi)	/* asi to emulate! */
{
	union {
		vis_inst_type	inst;
		fp_inst_type	pinst;
	} i;

	ASSERT(USERMODE(pregs->r_tstate));
	i.pinst = pinst;
	switch (asi) {
		case ASI_PST8_P:
		case ASI_PST8_S:
		case ASI_PST16_P:
		case ASI_PST16_S:
		case ASI_PST32_P:
		case ASI_PST32_S:
		case ASI_PST8_PL:
		case ASI_PST8_SL:
		case ASI_PST16_PL:
		case ASI_PST16_SL:
		case ASI_PST32_PL:
		case ASI_PST32_SL:
			return (vis_prtl_fst(pfpsd, i.inst, pregs,
				prw, pfpu, asi));
		case ASI_FL8_P:
		case ASI_FL8_S:
		case ASI_FL8_PL:
		case ASI_FL8_SL:
		case ASI_FL16_P:
		case ASI_FL16_S:
		case ASI_FL16_PL:
		case ASI_FL16_SL:
			return (vis_short_fls(pfpsd, i.inst, pregs,
				prw, pfpu, asi));
		case ASI_BLK_AIUP:
		case ASI_BLK_AIUS:
		case ASI_BLK_AIUPL:
		case ASI_BLK_AIUSL:
		case ASI_BLK_P:
		case ASI_BLK_S:
		case ASI_BLK_PL:
		case ASI_BLK_SL:
		case ASI_BLK_COMMIT_P:
		case ASI_BLK_COMMIT_S:
			return (vis_blk_fldst(pfpsd, i.inst, pregs,
				prw, pfpu, asi));
		default:
			return (ftt_unimplemented);
	}
}

/*
 * Simulator for partial stores between floating-point unit and memory.
 */
static enum ftt_type
vis_prtl_fst(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* ISE instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu,	/* Pointer to FPU register block. */
	u_int		asi)	/* asi to emulate! */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	u_int	opf, msk;
	int	h, i, j;
	uint64_t ea, tmsk;
	union {
		freg_type	f;
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} k, l, res;
	enum ftt_type   ftt;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((nrd & 1) == 1) 		/* fix register encoding */
		nrd = (nrd & 0x1e) | 0x20;
	opf = inst.opf;
	res.ll = 0;
	if ((opf & 0x100) == 0) {	/* effective address = rs1  */
		ftt = read_iureg(pfpsd, nrs1, pregs, prw, &ea);
		if (ftt != ftt_none)
			return (ftt);
		ftt = read_iureg(pfpsd, nrs2, pregs, prw, &tmsk);
		if (ftt != ftt_none)
			return (ftt);
		msk = (u_int)tmsk;
	} else {
		pfpsd->fp_trapaddr = (caddr_t)pregs->r_pc;
		return (ftt_unimplemented);
	}

	pfpsd->fp_trapaddr = (caddr_t)ea; /* setup bad addr in case we trap */
	if ((ea & 0x3) != 0)
		return (ftt_alignment);	/* Require 32 bit-alignment. */

	switch (asi) {
	case ASI_PST8_P:
	case ASI_PST8_S:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (i = 0, j = 0x80; i <= 7; i++, j >>= 1) {
			if ((msk & j) == j)
				res.c[i] = k.c[i];
			else
				res.c[i] = l.c[i];
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	case ASI_PST8_PL:	/* little-endian */
	case ASI_PST8_SL:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (h = 7, i = 0, j = 0x80; i <= 7; h--, i++, j >>= 1) {
			if ((msk & j) == j)
				res.c[h] = k.c[i];
			else
				res.c[h] = l.c[i];
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	case ASI_PST16_P:
	case ASI_PST16_S:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (i = 0, j = 0x8; i <= 3; i++, j >>= 1) {
			if ((msk & j) == j)
				res.s[i] = k.s[i];
			else
				res.s[i] = l.s[i];
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	case ASI_PST16_PL:
	case ASI_PST16_SL:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (h = 7, i = 0, j = 0x8; i <= 6; h -= 2, i += 2, j >>= 1) {
			if ((msk & j) == j) {
				res.c[h] = k.c[i];
				res.c[h-1] = k.c[i+1];
			} else {
				res.c[h] = l.c[i];
				res.c[h-1] = l.c[i+1];
			}
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	case ASI_PST32_P:
	case ASI_PST32_S:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (i = 0, j = 0x2; i <= 1; i++, j >>= 1) {
			if ((msk & j) == j)
				res.i[i] = k.i[i];
			else
				res.i[i] = l.i[i];
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	case ASI_PST32_PL:
	case ASI_PST32_SL:
		ftt = _fp_read_extword((uint64_t *)ea, &l.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		k.f.FPU_DREG_FIELD = pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
		for (h = 7, i = 0, j = 0x2; i <= 4; h -= 4, i += 4, j >>= 1) {
			if ((msk & j) == j) {
				res.c[h] = k.c[i];
				res.c[h-1] = k.c[i+1];
				res.c[h-2] = k.c[i+2];
				res.c[h-3] = k.c[i+3];
			} else {
				res.c[h] = l.c[i];
				res.c[h-1] = l.c[i+1];
				res.c[h-2] = l.c[i+2];
				res.c[h-3] = l.c[i+3];
			}
		}
		ftt = _fp_write_extword((uint64_t *)ea, res.ll, pfpsd);
		if (ftt != ftt_none)
			return (ftt);
		break;
	}

	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt_none);
}

/*
 * Simulator for short load/stores between floating-point unit and memory.
 */
static enum ftt_type
vis_short_fls(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* ISE instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu,	/* Pointer to FPU register block. */
	u_int		asi)	/* asi to emulate! */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	u_int	opf;
	uint64_t ea, tea;
	union {
		freg_type	f;
		uint64_t	ll;
		uint32_t	i[2];
		uint16_t	s[4];
		uint8_t		c[8];
	} k;
	union {
		vis_inst_type	inst;
		int		i;
	} fp;
	enum ftt_type   ftt = ftt_none;
	u_short us;
	u_char uc;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((nrd & 1) == 1) 		/* fix register encoding */
		nrd = (nrd & 0x1e) | 0x20;
	opf = inst.opf;
	fp.inst = inst;
	if ((opf & 0x100) == 0) { /* effective address = rs1 + rs2 */
		ftt = read_iureg(pfpsd, nrs1, pregs, prw, &ea);
		if (ftt != ftt_none)
			return (ftt);
		ftt = read_iureg(pfpsd, nrs2, pregs, prw, &tea);
		if (ftt != ftt_none)
			return (ftt);
		ea += tea;
	} else {	/* effective address = rs1 + imm13 */
		fp.inst = inst;
		ea = (fp.i << 19) >> 19;	/* Extract simm13 field. */
		ftt = read_iureg(pfpsd, nrs1, pregs, prw, &tea);
		if (ftt != ftt_none)
			return (ftt);
		ea += tea;
	}
#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		ea = (uint64_t)(caddr32_t)ea;
#endif

	pfpsd->fp_trapaddr = (caddr_t)ea; /* setup bad addr in case we trap */
	switch (asi) {
	case ASI_FL8_P:
	case ASI_FL8_S:
	case ASI_FL8_PL:		/* little-endian */
	case ASI_FL8_SL:
		if ((inst.op3 & 7) == 3) {	/* load byte */
			if (fuword8((void *)ea, &uc) == -1)
				return (ftt_fault);
			k.ll = 0;
			k.c[7] = uc;
			pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)] =
				k.f.FPU_DREG_FIELD;
		} else {			/* store byte */
			k.f.FPU_DREG_FIELD =
				pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
			uc = k.c[7];
			if (subyte((caddr_t)ea, uc) == -1)
				return (ftt_fault);
		}
		break;
	case ASI_FL16_P:
	case ASI_FL16_S:
		if ((ea & 1) == 1)
			return (ftt_alignment);
		if ((inst.op3 & 7) == 3) {	/* load short */
			if (fuword16((void *)ea, &us) == -1)
				return (ftt_fault);
			k.ll = 0;
			k.s[3] = us;
			pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)] =
				k.f.FPU_DREG_FIELD;
		} else {			/* store short */
			k.f.FPU_DREG_FIELD =
				pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
			us = k.s[3];
			if (suword16((caddr_t)ea, us) == -1)
				return (ftt_fault);
		}
		break;
	case ASI_FL16_PL:		/* little-endian */
	case ASI_FL16_SL:
		if ((ea & 1) == 1)
			return (ftt_alignment);
		if ((inst.op3 & 7) == 3) {	/* load short */
			if (fuword16((void *)ea, &us) == -1)
				return (ftt_fault);
			k.ll = 0;
			k.c[6] = (u_char)us;
			k.c[7] = (u_char)((us & 0xff00) >> 8);
			pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)] =
				k.f.FPU_DREG_FIELD;
		} else {			/* store short */
			k.f.FPU_DREG_FIELD =
				pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
			uc = k.c[7];
			us = (u_short) ((uc << 8) | k.c[6]);
			if (suword16((void *)ea, us) == -1)
				return (ftt_fault);
		}
		break;
	}

	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt_none);
}

/*
 * Simulator for block loads and stores between floating-point unit and memory.
 * XXX - OK, so it is really gross to flush the whole Ecache for a block commit
 *	 store - but the circumstances under which this code actually gets
 *	 used in real life are so obscure that you can live with it!
 */
static enum ftt_type
vis_blk_fldst(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	vis_inst_type	inst,	/* ISE instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	kfpu_t		*pfpu,	/* Pointer to FPU register block. */
	u_int		asi)	/* asi to emulate! */
{
	u_int	nrs1, nrs2, nrd;	/* Register number fields. */
	u_int	opf, h, i, j;
	uint64_t ea, tea;
	union {
		freg_type	f;
		uint64_t	ll;
		uint8_t		c[8];
	} k, l;
	union {
		vis_inst_type	inst;
		int32_t		i;
	} fp;
	enum ftt_type   ftt;
	boolean_t little_endian = B_FALSE;

	nrs1 = inst.rs1;
	nrs2 = inst.rs2;
	nrd = inst.rd;
	if ((nrd & 1) == 1) 		/* fix register encoding */
		nrd = (nrd & 0x1e) | 0x20;

	/* ensure register is 8-double precision aligned */
	if ((nrd & 0xf) != 0)
		return (ftt_unimplemented);

	opf = inst.opf;
	if ((opf & 0x100) == 0) { 	/* effective address = rs1 + rs2 */
		ftt = read_iureg(pfpsd, nrs1, pregs, prw, &ea);
		if (ftt != ftt_none)
			return (ftt);
		ftt = read_iureg(pfpsd, nrs2, pregs, prw, &tea);
		if (ftt != ftt_none)
			return (ftt);
		ea += tea;
	} else {			/* effective address = rs1 + imm13 */
		fp.inst = inst;
		ea = (fp.i << 19) >> 19;	/* Extract simm13 field. */
		ftt = read_iureg(pfpsd, nrs1, pregs, prw, &tea);
		if (ftt != ftt_none)
			return (ftt);
		ea += tea;
	}
	if ((ea & 0x3F) != 0)		/* Require 64 byte-alignment. */
		return (ftt_alignment);

	pfpsd->fp_trapaddr = (caddr_t)ea; /* setup bad addr in case we trap */
	switch (asi) {
	case ASI_BLK_AIUPL:
	case ASI_BLK_AIUSL:
	case ASI_BLK_PL:
	case ASI_BLK_SL:
		little_endian = B_TRUE;
		/* FALLTHROUGH */
	case ASI_BLK_AIUP:
	case ASI_BLK_AIUS:
	case ASI_BLK_P:
	case ASI_BLK_S:
	case ASI_BLK_COMMIT_P:
	case ASI_BLK_COMMIT_S:
		if ((inst.op3 & 7) == 3) {	/* lddf */
		    for (i = 0; i < 8; i++, ea += 8, nrd += 2) {
			ftt = _fp_read_extword((uint64_t *)ea, &k.ll, pfpsd);
			if (ftt != ftt_none)
				return (ftt);
			if (little_endian) {
				for (j = 0, h = 7; j < 8; j++, h--)
					l.c[h] = k.c[j];
				k.ll = l.ll;
			}
			pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)] =
				k.f.FPU_DREG_FIELD;
		    }
		} else {			/* stdf */
		    for (i = 0; i < 8; i++, ea += 8, nrd += 2) {
			k.f.FPU_DREG_FIELD =
				pfpu->fpu_fr.fpu_dregs[DOUBLE(nrd)];
			if (little_endian) {
				for (j = 0, h = 7; j < 8; j++, h--)
					l.c[h] = k.c[j];
				k.ll = l.ll;
			}
			ftt = _fp_write_extword((uint64_t *)ea, k.ll, pfpsd);
			if (ftt != ftt_none)
				return (ftt);
		    }
		}
		if ((asi == ASI_BLK_COMMIT_P) || (asi == ASI_BLK_COMMIT_S))
			cpu_flush_ecache();
		break;
	default:
		/* addr of unimp inst */
		pfpsd->fp_trapaddr = (caddr_t)pregs->r_pc;
		return (ftt_unimplemented);
	}

	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt_none);
}

/*
 * Simulator for rd %gsr instruction.
 */
enum ftt_type
vis_rdgsr(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	struct v9_fpu	*fp)	/* Need to fp to access gsr reg */
{
	u_int nrd;
	uint64_t r = 0;
	enum ftt_type ftt = ftt_none;

	nrd = pinst.rd;
	r = get_gsr(fp);
	if (r > 0x7f)
		cmn_err(CE_CONT, "vis_rdgsr: gsr 0x%" PRIx64 "\n", r);
	ftt = write_iureg(pfpsd, nrd, pregs, prw, &r);
	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt);
}

/*
 * Simulator for wr %gsr instruction.
 */
enum ftt_type
vis_wrgsr(
	fp_simd_type	*pfpsd,	/* FPU simulator data. */
	fp_inst_type	pinst,	/* FPU instruction to simulate. */
	struct regs	*pregs,	/* Pointer to PCB image of registers. */
	void		*prw,	/* Pointer to locals and ins. */
	struct v9_fpu	*fp)	/* Need to fp to access gsr reg */
{
	u_int nrs1;
	uint64_t r, r1, r2;
	enum ftt_type ftt = ftt_none;

	nrs1 = pinst.rs1;
	ftt = read_iureg(pfpsd, nrs1, pregs, prw, &r1);
	if (ftt != ftt_none)
		return (ftt);
	if (pinst.ibit == 0) {	/* copy the value in r[rs2] */
		u_int nrs2;

		nrs2 = pinst.rs2;
		ftt = read_iureg(pfpsd, nrs2, pregs, prw, &r2);
		if (ftt != ftt_none)
			return (ftt);
	} else {	/* use sign_ext(simm13) */
		union {
			fp_inst_type	inst;
			uint32_t	i;
		} fp;

		fp.inst = pinst;		/* Extract simm13 field */
		r2 = (fp.i << 19) >> 19;
	}
	r = r1 ^ r2;
	r &= 0x7f;
	set_gsr(r, fp);
	pregs->r_pc = pregs->r_npc;	/* Do not retry emulated instruction. */
	pregs->r_npc += 4;
	return (ftt);
}

/*
 * This is the loadable module wrapper.
 */
#include <sys/errno.h>
#include <sys/modctl.h>

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops,
	"vis fp simulation",
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
