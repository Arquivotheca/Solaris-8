/*
 * Copyright (c) 1987, 1995, 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uword.c	1.15	97/07/09 SMI"

/* Read/write user memory procedures for Sparc9 FPU simulator. */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/systm.h>
#include <vm/seg.h>
#include <sys/privregs.h>
#include <sys/stack.h>
#include <sys/debug.h>
#ifdef	__sparcv9
#include <sys/model.h>
#endif

/* read the user instruction */
enum ftt_type
_fp_read_inst(
	const uint32_t *address,	/* FPU instruction address. */
	uint32_t *pvalue,		/* Place for instruction value. */
	fp_simd_type *pfpsd)		/* Pointer to fpu simulator data. */
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		address = (uint32_t *)(caddr32_t)address;
#endif

	if (fuiword32(address, pvalue) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_READ;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
_fp_read_extword(
	const uint64_t *address,	/* FPU data address. */
	uint64_t *pvalue,		/* Place for extended word value. */
	fp_simd_type *pfpsd)		/* Pointer to fpu simulator data. */
{
	if (((uintptr_t)address & 0x7) != 0)
		return (ftt_alignment);	/* Must be extword-aligned. */

#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		address = (uint64_t *)(caddr32_t)address;
#endif

	if (fuword64(address, pvalue) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_READ;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
_fp_read_word(
	const uint32_t *address,	/* FPU data address. */
	uint32_t *pvalue,		/* Place for word value. */
	fp_simd_type *pfpsd)		/* Pointer to fpu simulator data. */
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		address = (uint32_t *)(caddr32_t)address;
#endif

	if (fuword32(address, pvalue) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_READ;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
_fp_write_extword(
	uint64_t *address,		/* FPU data address. */
	uint64_t value,			/* Extended word value to write. */
	fp_simd_type *pfpsd)		/* Pointer to fpu simulator data. */
{
	if (((uintptr_t) address & 0x7) != 0)
		return (ftt_alignment);	/* Must be extword-aligned. */

#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		address = (uint64_t *)(caddr32_t)address;
#endif

	if (suword64(address, value) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_WRITE;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
_fp_write_word(
	uint32_t *address,		/* FPU data address. */
	uint32_t value,			/* Word value to write. */
	fp_simd_type *pfpsd)		/* Pointer to fpu simulator data. */
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

#ifdef __sparcv9
	if (get_udatamodel() == DATAMODEL_ILP32)
		address = (uint32_t *)(caddr32_t)address;
#endif

	if (suword32(address, value) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_WRITE;
		return (ftt_fault);
	}
	return (ftt_none);
}

/*
 * Reads integer unit's register n.
 */
enum ftt_type
read_iureg(
	fp_simd_type	*pfpsd,		/* Pointer to fpu simulator data */
	u_int		n,		/* IU register n */
	struct regs	*pregs,		/* Pointer to PCB image of registers. */
	void		*prw,		/* Pointer to locals and ins. */
	uint64_t	*pvalue)	/* Place for extended word value. */
{
	enum ftt_type ftt;

	if (n == 0) {
		*pvalue = 0;
		return (ftt_none);	/* Read global register 0. */
	} else if (n < 16) {
		long long *preg;

		preg = &pregs->r_ps;		/* globals and outs */
		*pvalue = preg[n];
		return (ftt_none);
	} else if (USERMODE(pregs->r_tstate)) { /* locals and ins */
#ifdef	__sparcv9
		if (lwp_getdatamodel(curthread->t_lwp) == DATAMODEL_ILP32) {
#endif
			uint32_t res, *addr, *rw;

			rw = (uint32_t *)(caddr32_t)prw;
			addr = (uint32_t *)&rw[n - 16];
			ftt = _fp_read_word(addr, &res, pfpsd);
			*pvalue = (uint64_t)res;
#ifdef	__sparcv9
		} else {
			uint64_t res, *addr, *rw = (uint64_t *)
					((uintptr_t)prw + STACK_BIAS);

			addr = (uint64_t *)&rw[n - 16];
			ftt = _fp_read_extword(addr, &res, pfpsd);
			*pvalue = res;
		}
#endif
		return (ftt);
	} else {
		u_long *addr, *rw = (u_long *)((uintptr_t)prw + STACK_BIAS);
		u_long res;

		addr = (u_long *)&rw[n - 16];
		res = *addr;
		*pvalue = res;

		return (ftt_none);
	}
}

/*
 * Writes integer unit's register n.
 */
enum ftt_type
write_iureg(
	fp_simd_type	*pfpsd,		/* Pointer to fpu simulator data. */
	u_int		n,		/* IU register n. */
	struct regs	*pregs,		/* Pointer to PCB image of registers. */
	void		*prw,		/* Pointer to locals and ins. */
	uint64_t	*pvalue)	/* Extended word value to write. */
{
	long long *preg;
	enum ftt_type ftt;

	if (n == 0) {
		return (ftt_none);	/* Read global register 0. */
	} else if (n < 16) {
		preg = &pregs->r_ps;		/* globals and outs */
		preg[n] = *pvalue;
		return (ftt_none);
	} else if (USERMODE(pregs->r_tstate)) { /* locals and ins */
#ifdef	__sparcv9
		if (lwp_getdatamodel(curthread->t_lwp) == DATAMODEL_ILP32) {
#endif
			uint32_t res, *addr, *rw;

			rw = (uint32_t *)(caddr32_t)prw;
			addr = &rw[n - 16];
			res = (u_int)*pvalue;
			ftt = _fp_write_word(addr, res, pfpsd);
#ifdef	__sparcv9
		} else {
			uint64_t *addr, *rw = (uint64_t *)
				((uintptr_t)prw + STACK_BIAS);
			uint64_t res;

			addr = &rw[n - 16];
			res = *pvalue;
			ftt = _fp_write_extword(addr, res, pfpsd);
		}
#endif
		return (ftt);
	} else {
		u_long *addr, *rw = (u_long *)((uintptr_t)prw + STACK_BIAS);
		u_long res = *pvalue;

		addr = &rw[n - 16];
		*addr = res;

		return (ftt_none);
	}
}
