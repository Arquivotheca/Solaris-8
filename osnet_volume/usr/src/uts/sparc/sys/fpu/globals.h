/*
 * Copyright (c) 1988,1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FPU_GLOBALS_H
#define	_SYS_FPU_GLOBALS_H

#pragma ident	"@(#)globals.h	1.31	98/01/06 SMI"

/*
 * Sparc floating-point simulator PRIVATE include file.
 */

#include <sys/types.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*	PRIVATE CONSTANTS	*/

#define	INTEGER_BIAS	   31
#define	LONGLONG_BIAS	   63
#define	SINGLE_BIAS	  127
#define	DOUBLE_BIAS	 1023
#define	EXTENDED_BIAS	16383

/* PRIVATE TYPES	*/

#define	DOUBLE_E(n) (n & 0xfffe)	/* More significant word of double. */
#define	DOUBLE_F(n) (1+DOUBLE_E(n))	/* Less significant word of double. */
#define	EXTENDED_E(n) (n & 0xfffc) /* Sign/exponent/significand of extended. */
#define	EXTENDED_F(n) (1+EXTENDED_E(n)) /* 2nd word of extended significand. */
#define	EXTENDED_G(n) (2+EXTENDED_E(n)) /* 3rd word of extended significand. */
#define	EXTENDED_H(n) (3+EXTENDED_E(n)) /* 4th word of extended significand. */
#define	DOUBLE(n) ((n & 0xfffe) >> 1)	/* Shift n to access double regs. */
#define	QUAD_E(n) ((n & 0xfffc) >> 1)	/* More significant half of quad. */
#define	QUAD_F(n) (1+QUAD_E(n))		/* Less significant half of quad. */


#if defined(_KERNEL)

typedef struct {
	int sign;
	enum fp_class_type fpclass;
	int	exponent;		/* Unbiased exponent	*/
	uint_t significand[4];		/* Four significand word . */
	int	rounded;		/* rounded bit */
	int	sticky;			/* stick bit */
} unpacked;


/*
 * PRIVATE FUNCTIONS
 * pfreg routines use "physical" FPU registers, in fpusystm.h.
 * vfreg routines use "virtual" FPU registers at *_fp_current_pfregs.
 */

extern void _fp_read_vfreg(FPU_REGS_TYPE *, uint_t, fp_simd_type *);
extern void _fp_write_vfreg(FPU_REGS_TYPE *, uint_t, fp_simd_type *);
#ifdef	__sparcv9cpu
extern void _fp_read_vdreg(FPU_DREGS_TYPE *, uint_t, fp_simd_type *);
extern void _fp_write_vdreg(FPU_DREGS_TYPE *, uint_t, fp_simd_type *);
#endif

extern enum ftt_type _fp_iu_simulator(fp_simd_type *, fp_inst_type,
			struct regs *, void *, kfpu_t *);

extern void _fp_unpack(fp_simd_type *, unpacked *, uint_t, enum fp_op_type);
extern void _fp_pack(fp_simd_type *, unpacked *, uint_t, enum fp_op_type);
extern void _fp_unpack_word(fp_simd_type *, uint32_t *, uint_t);
extern void _fp_pack_word(fp_simd_type *, uint32_t *, uint_t);
#ifdef	__sparcv9cpu
extern void _fp_unpack_extword(fp_simd_type *, uint64_t *, uint_t);
extern void _fp_pack_extword(fp_simd_type *, uint64_t *, uint_t);
extern enum ftt_type fmovcc(fp_simd_type *, fp_inst_type, fsr_type *);
extern enum ftt_type fmovr(fp_simd_type *, fp_inst_type);
extern enum ftt_type movcc(fp_simd_type *, fp_inst_type, struct regs *,
				void *, kfpu_t *);
#endif
extern enum ftt_type fldst(fp_simd_type *, fp_inst_type, struct regs *,
				void *, kfpu_t *);
extern void fpu_normalize(unpacked *);
extern void fpu_rightshift(unpacked *, int);
extern void fpu_set_exception(fp_simd_type *, enum fp_exception_type);
extern void fpu_error_nan(fp_simd_type *, unpacked *);
extern void unpacksingle(fp_simd_type *, unpacked *, single_type);
extern void unpackdouble(fp_simd_type *, unpacked *, double_type, uint_t);
extern uint_t fpu_add3wc(uint_t *, uint_t, uint_t, uint_t);
extern uint_t fpu_sub3wc(uint_t *, uint_t, uint_t, uint_t);
extern uint_t fpu_neg2wc(uint_t *, uint_t, uint_t);
extern int fpu_cmpli(uint_t *, uint_t *, int);

/* extern void _fp_product(uint_t, uint_t, uint_t *); */

extern enum fcc_type _fp_compare(fp_simd_type *, unpacked *, unpacked *, int);

extern void _fp_add(fp_simd_type *, unpacked *, unpacked *, unpacked *);
extern void _fp_sub(fp_simd_type *, unpacked *, unpacked *, unpacked *);
extern void _fp_mul(fp_simd_type *, unpacked *, unpacked *, unpacked *);
extern void _fp_div(fp_simd_type *, unpacked *, unpacked *, unpacked *);
extern void _fp_sqrt(fp_simd_type *, unpacked *, unpacked *);

extern enum ftt_type	_fp_write_word(uint32_t *, uint32_t, fp_simd_type *);
extern enum ftt_type	_fp_read_word(const uint32_t *, uint32_t *,
					fp_simd_type *);
extern enum ftt_type	_fp_read_inst(const uint32_t *, uint32_t *,
					fp_simd_type *);
#ifdef	__sparcv9cpu
extern enum ftt_type	_fp_write_extword(uint64_t *, uint64_t, fp_simd_type *);
extern enum ftt_type	_fp_read_extword(const uint64_t *, uint64_t *,
					fp_simd_type *);
extern enum ftt_type	read_iureg(fp_simd_type *, uint_t, struct regs *,
					void *, uint64_t *);
extern enum ftt_type	write_iureg(fp_simd_type *, uint_t, struct regs *,
					void *, uint64_t *);
#else
extern enum ftt_type	read_iureg(fp_simd_type *, uint_t, struct regs *,
				struct rwindow *, uint_t *);
#endif

#endif  /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FPU_GLOBALS_H */
