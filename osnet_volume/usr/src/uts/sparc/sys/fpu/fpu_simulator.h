/*
 * Copyright (c) 1988,1990,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYS_FPU_FPU_SIMULATOR_H
#define	_SYS_FPU_FPU_SIMULATOR_H

#pragma ident	"@(#)fpu_simulator.h	1.28	98/10/21 SMI"
/* SunOS-4.0 1.10	*/

/*
 * Sparc floating-point simulator PUBLIC include file.
 */

#include <sys/types.h>
#include <sys/ieeefp.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*	PUBLIC TYPES	*/

enum fcc_type {			/* relationships */
	fcc_equal	= 0,
	fcc_less	= 1,
	fcc_greater	= 2,
	fcc_unordered	= 3
};

enum cc_type {			/* icc/fcc number */
	fcc_0	= 0,
	fcc_1	= 1,
	fcc_2	= 2,
	fcc_3	= 3,
	icc	= 4,
	xcc	= 6
};

/* FSR types. */

enum ftt_type {			/* types of traps */
	ftt_none	= 0,
	ftt_ieee	= 1,
	ftt_unfinished	= 2,
	ftt_unimplemented = 3,
	ftt_sequence	= 4,
	ftt_alignment	= 5,	/* defined by software convention only */
	ftt_fault	= 6,	/* defined by software convention only */
	ftt_7		= 7
};

#ifndef	__sparcv9cpu
typedef	struct {		/* Sparc FSR. */
						/* enum fp_direction_type */
	unsigned int		rnd	: 2;	/* rounding direction */
						/* enum fp_precision_type */
	unsigned int		rnp	: 2;	/* rounding precision */
	unsigned int		tem	: 5;	/* trap enable mask */
	unsigned int			: 6;
						/* enum ftt_type */
	unsigned int		ftt	: 3;	/* FPU trap type */
	unsigned int		qne	: 1;	/* FPQ not empty */
	unsigned int		 pr	: 1;	/* partial result */
						/* enum fcc_type */
	unsigned int		fcc	: 2;	/* FPU condition code */
	unsigned int		aexc	: 5;	/* accumulated exceptions */
	unsigned int		cexc	: 5;	/* current exception */
} fsr_type;
#else
typedef	struct {		/* Sparc V9 FSR. */
	unsigned int			: 26;
	unsigned int		fcc3	: 2;	/* fp condition code 3 */
	unsigned int		fcc2	: 2;	/* fp condition code 2 */
	unsigned int		fcc1	: 2;	/* fp condition code 1 */
						/* enum fp_direction_type */
	unsigned int		rnd	: 2;	/* rounding direction */
	unsigned int		rnp	: 2;	/* for v7 compatibility only */
	unsigned int		tem	: 5;	/* trap enable mask */
	unsigned int		 ns	: 1;	/* non-standard */
	unsigned int			: 5;
						/* enum ftt_type */
	unsigned int 		ftt	: 3;	/* FPU trap type */
	unsigned int		qne	: 1;	/* FPQ not empty */
	unsigned int		 pr	: 1;	/* partial result */
						/* enum fcc_type */
	unsigned int 		fcc	: 2;	/* fp condition code 0 */
	unsigned int		aexc	: 5;	/* accumulated exceptions */
	unsigned int		cexc	: 5;	/* current exception */
} fsr_types;

/*
 * The C compiler and the C spec do not support bitfields in a long long,
 * as per fsr_types above, so don't hold your breath waiting for this
 * workaround cruft to disappear.
 */

typedef union {
	fsr_types	fsr;
	uint64_t	ll;
} fsr_type;

#define	fcc3	fsr.fcc3
#define	fcc2	fsr.fcc2
#define	fcc1	fsr.fcc1
#define	fcc0	fsr.fcc
#define	rnd	fsr.rnd
#define	rnp	fsr.rnp
#define	tem	fsr.tem
#define	aexc	fsr.aexc
#define	cexc	fsr.cexc
#endif

typedef			/* FPU register viewed as single components. */
	struct {
	uint32_t sign :		1;
	uint32_t exponent :	8;
	uint32_t significand : 23;
} single_type;

typedef			/* FPU register viewed as double components. */
	struct {
	uint32_t sign :		1;
	uint32_t exponent :    11;
	uint32_t significand : 20;
} double_type;

typedef			/* FPU register viewed as extended components. */
	struct {
	uint32_t sign :		 1;
	uint32_t exponent :	15;
	uint32_t significand :	16;
} extended_type;

typedef			/* FPU register with multiple data views. */
	union {
	int32_t		int32_reg;
	int64_t		int64_reg;
	uint32_t	uint32_reg;
	uint64_t	uint64_reg;
	float		float_reg;
	single_type	single_reg;
	double_type	double_reg;
	extended_type	extended_reg;
} freg_type;

enum fp_op_type {		/* Type specifiers in FPU instructions. */
	fp_op_int32	= 0,	/* Not in hardware, but convenient to define. */
	fp_op_single	= 1,
	fp_op_double	= 2,
	fp_op_extended	= 3,
	fp_op_int64	= 4
};

enum fp_opcode {	/* FPU op codes, minus precision and leading 0. */
	fmovs		= 0x0,
	fnegs		= 0x1,
	fabss		= 0x2,
	fp_op_3 = 3, fp_op_4 = 4, fp_op_5 = 5, fp_op_6 = 6, fp_op_7 = 7,
	fp_op_8		= 0x8,
	fp_op_9		= 0x9,
	fsqrt		= 0xa,
	fp_op_b = 0xb, fp_op_c = 0xc, fp_op_d = 0xd,
	fp_op_e = 0xe, fp_op_f = 0xf,
	fadd		= 0x10,
	fsub		= 0x11,
	fmul		= 0x12,
	fdiv		= 0x13,
	fcmp		= 0x14,
	fcmpe		= 0x15,
	fp_op_16 = 0x16, fp_op_17 = 0x17,
	fp_op_18	= 0x18,
	fp_op_19	= 0x19,
	fsmuld		= 0x1a,
	fdmulx		= 0x1b,
	ftoll		= 0x20,
	flltos		= 0x21,
	flltod		= 0x22,
	flltox		= 0x23,
	fp_op_24 = 0x24, fp_op_25 = 0x25, fp_op_26 = 0x26, fp_op_27 = 0x27,
	fp_op_28 = 0x28, fp_op_29 = 0x29, fp_op_2a = 0x2a, fp_op_2b = 0x2b,
	fp_op_2c = 0x2c, fp_op_2d = 0x2d, fp_op_2e = 0x2e, fp_op_2f = 0x2f,
	fp_op_30	= 0x30,
	fitos		= 0x31,
	fitod		= 0x32,
	fitox		= 0x33,
	ftoi		= 0x34,
	fp_op_35 = 0x35, fp_op_36 = 0x36, fp_op_37 = 0x37,
	ft_op_38	= 0x38,
	fp_op_39 = 0x39, fp_op_3a = 0x3a, fp_op_3b = 0x3b,
	fp_op_3c	= 0x3c,
	fp_op_3d = 0x3d, fp_op_3e = 0x3e, fp_op_3f = 0x3f
};

typedef			/* FPU instruction. */
	struct {
	uint32_t		hibits	: 2;	/* Top two bits. */
	uint32_t		rd	: 5;	/* Destination. */
	uint32_t		op3	: 6;	/* Main op code. */
	uint32_t		rs1	: 5;	/* First operand. */
	uint32_t		ibit	: 1;	/* I format bit. */
	uint32_t /* enum fp_opcode */  opcode : 6; /* Floating-point op code. */
	uint32_t /* enum fp_op_type */ prec   : 2; /* Precision. */
	uint32_t		rs2	: 5;	/* Second operand. */
} fp_inst_type;

typedef			/* Integer condition code. */
	struct {
	uint32_t			: 28;	/* the unused part */
	uint32_t		n	: 1;	/* Negative bit. */
	uint32_t		z	: 1;	/* Zero bit. */
	uint32_t		v	: 1;	/* Overflow bit. */
	uint32_t		c	: 1;	/* Carry bit. */
} ccr_type;

typedef			/* FPU data used by simulator. */
	struct {
	uint_t			fp_fsrtem;
	enum fp_direction_type	fp_direction;
	enum fp_precision_type	fp_precision;
	uint_t			fp_current_exceptions;
	kfpu_t			*fp_current_pfregs;
	void			(*fp_current_read_freg) ();
	void			(*fp_current_write_freg) ();
#ifdef	__sparcv9cpu
	void			(*fp_current_read_dreg) ();
	void			(*fp_current_write_dreg) ();
#endif
	int			fp_trapcode;
	char			*fp_trapaddr;
	struct	regs		*fp_traprp;
	enum	seg_rw		fp_traprw;
} fp_simd_type;

/* PUBLIC FUNCTIONS */

#ifdef	__STDC__

/*
 * fpu_simulator simulates FPU instructions only; reads and writes FPU data
 * registers directly.
 */
extern enum ftt_type fpu_simulator(fp_simd_type *pfpsd, fp_inst_type *pinst,
	fsr_type *pfsr, uint32_t inst);
/*
 * fp_emulator simulates FPU and CPU-FPU instructions; reads and writes FPU
 * data registers from image in pfpu.
 */
extern enum ftt_type fp_emulator(fp_simd_type *pfpsd, fp_inst_type *pinst,
	struct regs *rp, void *prw, kfpu_t *pfpu);
/*
 * fp_traps handles passing exception conditions to the kernel.
 * It is called after fp_simulator or fp_emulator fail (return a non-zero ftt).
 */
extern void fp_traps(fp_simd_type *pfpsd, enum ftt_type ftt, struct regs *rp);

/*
 * fp_kstat_update tracks fpu exception conditions.
 * It is called after a hardware trap returns a non-zero ftt.
 */
extern void fp_kstat_update(enum ftt_type ftt);

#ifdef  __sparcv9cpu
/*
 * fp_precise handles floating point unimplemented and unfinished traps,
 * for sparc V9 hardware. These traps are normally passed along to the
 * fpu_simulator, to see if it can run the unimplemented instruction or
 * finish the unfinished instruction. Needless to say, this takes time.
 */
extern void fp_precise(struct regs *rp);

/*
 * fpu_trap handles V9 floating point ieee and other floating point traps.
 * It is called after fp_simulator or fp_emulator fail (return a non-zero ftt),
 * and from the _fp_ieee_exception trap handler.
 */
extern void fpu_trap(struct regs *rp, caddr_t addr, uint32_t type,
			uint32_t code);
#else
/*
 * fp_kstat_runq handles the sparc V7/V8 floating point ieee, unfinished and
 * unimplemented traps. These traps are normally passed along to the
 * fpu_simulator, to see if it can run the unimplemented instruction or
 * finish the unfinished instruction, and rerun the ieee trap instructions.
 * Needless to say, this takes time, and the ieee case is probably not needed.
 */
extern void fp_kstat_runq(struct regs *rp, enum ftt_type ftt);
#endif

#else	/* ! __STDC__ */

/*
 * fpu_simulator simulates FPU instructions only; reads and writes FPU data
 * registers directly.
 */
extern enum ftt_type fpu_simulator(/* pfpsd, pinst, pfsr, instr */);
/*	fp_simd_type	*pfpsd;	 Pointer to FPU simulator data */
/*	fp_inst_type	*pinst;	 Pointer to FPU instruction to simulate. */
/*	fsr_type	*pfsr;	 Pointer to image of FSR to read and write. */
/*	int		instr;	 Instruction to emulate. */

/*
 * fp_emulator simulates FPU and CPU-FPU instructions; reads and writes FPU
 * data registers from image in pfpu.
 */
extern enum ftt_type fp_emulator(/* pfpsd, pinst, pregs, pwindow, pfpu */);
/*	fp_simd_type	*pfpsd;	   Pointer to FPU simulator data */
/*	fp_inst_type	*pinst;    Pointer to FPU instruction to simulate. */
/*	struct regs	*pregs;    Pointer to PCB image of registers. */
/*	struct rwindow	*pwindow;  Pointer to locals and ins. */
/*	struct fpu	*pfpu;	   Pointer to FPU register block. */

/*
 * fp_traps handles passing exception conditions to the kernel.
 * It is called after fp_simulator or fp_emulator fail (return a non-zero ftt).
 */
extern void fp_traps(/* pfpsd, ftt, rp */);
/*	fp_simd_type	*pfpsd;	 Pointer to FPU simulator data */
/*	enum ftt_type	ftt;	 Type of trap. */
/*	struct regs	*rp;	 Pointer to PCB image of registers. */

/*
 * fp_kstat_update tracks fpu exception conditions.
 * It is called after a hardware trap returns a non-zero ftt.
 */
extern void fp_kstat_update(/* ftt */);
/*	enum ftt_type ftt;	Type of trap. */

#ifdef  __sparcv9cpu
/*
 * fp_precise handles floating point unimplemented and unfinished traps,
 * for sparc V9 hardware. These traps are normally passed along to the
 * fpu_simulator, to see if it can run the unimplemented instruction or
 * finish the unfinished instruction. Needless to say, this takes time.
 */
extern void fp_precise(/* rp */);
/*	struct regs *rp;	Pointer to PCB image of registers. */

/*
 * fpu_trap handles V9 floating point ieee and other floating point traps.
 * It is called after fp_simulator or fp_emulator fail (return a non-zero ftt),
 * and from the _fp_ieee_exception trap handler.
 */
extern void fpu_trap(/* rp, addr, type, code */);
/*	struct regs *rp;	Pointer to PCB image of registers. */
/*	caddr_t addr;		Address of trapping instruction. */
/*	uint32_t type;		Type of trapping exception. */
/*	uint32_t code;		Trap code -> si_code. */

#else
/*
 * fp_kstat_runq handles the sparc V7/V8 floating point ieee, unfinished and
 * unimplemented traps. These traps are normally passed along to the
 * fpu_simulator, to see if it can run the unimplemented instruction or
 * finish the unfinished instruction, and rerun the ieee trap instructions.
 * Needless to say, this takes time, and the ieee case is probably not needed.
 */
extern void fp_kstat_runq(/* rp, ftt */);
/*	struct regs *rp;	Pointer to PCB image of registers. */
/*	enum ftt_type ftt;	Type of trap. */
#endif

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FPU_FPU_SIMULATOR_H */
