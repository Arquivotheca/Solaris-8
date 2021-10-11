/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pisadep.c	1.2	99/08/11 SMI"

#include <sys/types.h>
#include <errno.h>

#include "Pcontrol.h"

#define	M_PLT32_NRSV	4			/* reserved PLT entries */
#define	M_PLT32_ENTSIZE	12			/* size of each PLT entry */

#define	M_PLT64_NRSV	4			/* reserved bit PLT entries */
#define	M_PLT64_ENTSIZE	32			/* size of each PLT entry */

#define	M_BA_A_XCC	0x30680000		/* ba,a %xcc */
#define	M_JMPL_G5G0	0x81c16000		/* jmpl %g5 + 0, %g0 */
#define	M_XNOR_G5G1	0x82396000		/* xnor %g5, 0, %g1 */
#define	M_BA_A		0x30800000		/* ba,a */
#define	M_JMPL		0x81c06000		/* jmpl %g1 + simm13, %g0 */

#define	S_MASK(n)	((1 << (n)) - 1)	/* mask of lower n bits */

uintptr_t
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr, int *boundp)
{
	map_info_t *mp = Paddr2mptr(P, pltaddr);
	file_info_t *fp;

	uintptr_t dstpc = NULL;
	int bound = 0;

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 || pltaddr < fp->file_plt_base ||
	    pltaddr >= fp->file_plt_base + fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	if (P->status.pr_dmodel == PR_MODEL_LP64) {
		instr_t instr[8];

		if (Pread(P, instr, sizeof (instr), pltaddr) != sizeof (instr))
			return (NULL);

		if ((instr[1] & (~(S_MASK(19)))) == M_BA_A_XCC) {
			/*
			 * Unresolved 64-bit PLT entry:
			 *
			 * .PLT:
			 * 0	sethi	. - .PLT0, %g1
			 * 1	ba,a	%xcc, .PLT1
			 * 2	nop
			 * 3	nop
			 * 4	nop
			 * 5	nop
			 * 6	nop
			 * 7	nop
			 */
			Elf64_Sym *symp;
			Elf64_Rela r;

			uintptr_t r_addr;
			size_t i, symn;

			i = (pltaddr - fp->file_plt_base -
			    M_PLT64_NRSV * M_PLT64_ENTSIZE) / M_PLT64_ENTSIZE;

			r_addr = fp->file_jmp_rel + i * sizeof (Elf64_Rela);
			symn = fp->file_dynsym.sym_symn;

			if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
			    (i = ELF64_R_SYM(r.r_info)) < symn) {

				Elf_Data *data = fp->file_dynsym.sym_data;
				symp = &(((Elf64_Sym *)data->d_buf)[i]);
				dstpc = symp->st_value + fp->file_dyn_base;
				bound = 0;
			}

		} else if ((instr[6] & (~(S_MASK(13)))) == M_JMPL_G5G0) {
			/*
			 * Resolved 64-bit PLT entry format (abs-64):
			 *
			 * .PLT:
			 * 0	nop
			 * 1	sethi	%hh(dest), %g1
			 * 2	sethi	%lm(dest), %g5
			 * 3	or	%g1, %hm(dest), %g1
			 * 4	sllx	%g1, 32, %g1
			 * 5	or	%g1, %g5, %g5
			 * 6	jmpl	%g5 + %lo(dest), %g0
			 * 7	nop
			 */
			ulong_t hh_bits = instr[1] & S_MASK(22); /* 63..42 */
			ulong_t hm_bits = instr[3] & S_MASK(10); /* 41..32 */
			ulong_t lm_bits = instr[2] & S_MASK(22); /* 31..10 */
			ulong_t lo_bits = instr[6] & S_MASK(10); /* 09..00 */

			dstpc = (hh_bits << 42) | (hm_bits << 32) |
			    (lm_bits << 10) | lo_bits;

			bound = 1;

		} else if (instr[3] == M_JMPL) {
			/*
			 * Resolved 64-bit PLT entry format (top-32):
			 *
			 * .PLT:
			 * 0	nop
			 * 1	sethi	%hi(~dest), %g5
			 * 2	xnor	%g5, %lo(~dest), %g1
			 * 3	jmpl	%g1, %g0
			 * 4	nop
			 * 5	nop
			 * 6	nop
			 * 7	nop
			 */
			ulong_t hi_bits = (instr[1] & S_MASK(22)) << 10;
			ulong_t lo_bits = (instr[2] & S_MASK(10));

			dstpc = hi_bits ^ ~lo_bits;
			bound = 1;

		} else if ((instr[2] & (~(S_MASK(13)))) == M_XNOR_G5G1) {
			/*
			 * Resolved 64-bit PLT entry format (abs-44):
			 *
			 * .PLT:
			 * 0	nop
			 * 1	sethi	%h44(~dest), %g5
			 * 2	xnor	%g5, %m44(~dest), %g1
			 * 3	sllx	%g1, 12, %g1
			 * 4	jmp	%g1 + %l44(dest)
			 * 5	nop
			 * 6	nop
			 * 7	nop
			 */
			long h44 = (((long)instr[1] & S_MASK(22)) << 10);
			long m44 = (((long)instr[2] & S_MASK(13)) << 41) >> 41;
			long l44 = (((long)instr[4] & S_MASK(13)) << 41) >> 41;

			dstpc = (~(h44 ^ m44) << 12) + l44;
			bound = 1;

		} else
			errno = EINVAL;

	} else /* PR_MODEL_ILP32 */ {
		instr_t instr[4];

		if (Pread(P, instr, sizeof (instr), pltaddr) != sizeof (instr))
			return (NULL);

		if ((instr[1] & (~(S_MASK(22)))) == M_BA_A) {
			/*
			 * Unresolved 32-bit PLT entry:
			 *
			 * .PLT:
			 * 0	sethi	. - .PLT0, %g1
			 * 1	ba,a	.PLT0
			 * 2	nop
			 */
			Elf32_Sym *symp;
			Elf32_Rela r;

			uintptr_t r_addr;
			size_t i, symn;

			i = (pltaddr - fp->file_plt_base -
			    M_PLT32_NRSV * M_PLT32_ENTSIZE) / M_PLT32_ENTSIZE;

			r_addr = fp->file_jmp_rel + i * sizeof (Elf32_Rela);
			symn = fp->file_dynsym.sym_symn;

			if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
			    (i = ELF32_R_SYM(r.r_info)) < symn) {

				Elf_Data *data = fp->file_dynsym.sym_data;
				symp = &(((Elf32_Sym *)data->d_buf)[i]);
				dstpc = symp->st_value + fp->file_dyn_base;
				bound = 0;
			}

		} else if ((instr[2] & (~(S_MASK(13)))) == M_JMPL) {
			/*
			 * Resolved 32-bit PLT entry:
			 *
			 * .PLT:
			 * 0	sethi	. - .PLT0, %g1
			 * 1	sethi	%hi(dest), %g1
			 * 2	jmp	%g1 + %lo(dest)
			 */
			int simm = (int)((instr[2] & S_MASK(13)) << 19) >> 19;
			dstpc = (uintptr_t)(uint_t)
			    ((int)((instr[1] & S_MASK(22)) << 10) + simm);
			bound = 1;

		} else
			errno = EINVAL;
	}

	if (boundp != NULL)
		*boundp = bound;

	return (dstpc);
}
