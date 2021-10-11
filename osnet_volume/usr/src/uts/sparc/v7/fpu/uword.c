/*
 * Copyright (c) 1987-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uword.c	1.24	97/09/05 SMI"

/* Read/write user memory procedures for Sparc FPU simulator. */

#include <sys/param.h>
#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/systm.h>
#include <vm/seg.h>

/* read the user instruction */
enum ftt_type
_fp_read_inst(
	const uint32_t *address,
	uint32_t *pvalue,
	fp_simd_type *pfpsd)
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

	if (fuiword32(address, pvalue) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_READ;
		return (ftt_fault);
	}
	return (ftt_none);
}

#include <sys/privregs.h>

enum ftt_type
_fp_read_word(
	const uint32_t *address,
	uint32_t *pvalue,
	fp_simd_type *pfpsd)
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

	if (fuword32(address, pvalue) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_READ;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
_fp_write_word(
	uint32_t *address,
	uint32_t value,
	fp_simd_type *pfpsd)
{
	if (((uintptr_t)address & 0x3) != 0)
		return (ftt_alignment);	/* Must be word-aligned. */

	if (suword32(address, value) == -1) {
		pfpsd->fp_trapaddr = (caddr_t)address;
		pfpsd->fp_traprw = S_WRITE;
		return (ftt_fault);
	}
	return (ftt_none);
}

enum ftt_type
read_iureg(
	fp_simd_type	*pfpsd,
	u_int		n,
	struct regs	*pregs,		/* Pointer to PCB image of registers. */
	struct rwindow	*pwindow,	/* Pointer to locals and ins. */
	u_int		*pvalue)
{				/* Reads integer unit's register n. */
	greg_t	*pint;

	if (n < 16) {
		if (n == 0) {
			*pvalue = 0;
			return (ftt_none);	/* Read global register 0. */
		}
		pint = &pregs->r_g1;		/* globals and outs */
		*pvalue = pint[n-1];
		return (ftt_none);
	} else {		/* locals and ins */
		if (n < 24)
			pint = &pwindow->rw_local[n - 16];
		else
			pint = &pwindow->rw_in[n - 24];
		if ((uintptr_t)pint > KERNELBASE) {
			*pvalue = *pint;
			return (ftt_none);
		}
		return (_fp_read_word((uint32_t *)pint, pvalue, pfpsd));
	}
}
