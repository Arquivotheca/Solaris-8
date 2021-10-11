/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pisadep.c	1.1	99/03/23 SMI"

#include <sys/types.h>
#include <errno.h>

#include "Pcontrol.h"

#define	M_PLT_NRSV	4			/* reserved PLT entries */
#define	M_PLT_ENTSIZE	12			/* size of each PLT entry */

#define	M_BA_A		0x30800000		/* ba,a */
#define	M_JMPL		0x81c06000		/* jmpl %g1 + simm13, %g0 */

#define	S_MASK(n)	((1 << (n)) - 1)	/* mask of lower n bits */

uintptr_t
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr, int *boundp)
{
	map_info_t *mp = Paddr2mptr(P, pltaddr);
	file_info_t *fp;

	ulong_t instr[4];
	uintptr_t dstpc = NULL;
	int bound = 0;

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 || pltaddr < fp->file_plt_base ||
	    pltaddr >= fp->file_plt_base + fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	if (Pread(P, instr, sizeof (instr), pltaddr) != sizeof (instr))
		return (NULL);

	if ((instr[1] & (~(S_MASK(22)))) == M_BA_A) {
		/*
		 * Unresolved PLT entry:
		 *
		 * .PLT:
		 *	sethi	. - .PLT0, %g1
		 *	ba,a	.PLT0
		 *	nop
		 */
		size_t i = (pltaddr - fp->file_plt_base -
		    M_PLT_NRSV * M_PLT_ENTSIZE) / M_PLT_ENTSIZE;

		uintptr_t r_addr = fp->file_jmp_rel + i * sizeof (Elf32_Rela);
		Elf32_Rela r;

		if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
		    (i = ELF32_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {

			Elf_Data *data = fp->file_dynsym.sym_data;
			Elf32_Sym *symp = &(((Elf32_Sym *)data->d_buf)[i]);

			dstpc = symp->st_value + fp->file_dyn_base;
			bound = 0;
		}

	} else if ((instr[2] & (~(S_MASK(13)))) == M_JMPL) {
		/*
		 * Resolved PLT entry:
		 *
		 * .PLT:
		 *	sethi	. - .PLT0, %g1
		 *	sethi	%hi(dest), %g1
		 *	jmp	%g1 + %lo(dest)
		 */
		int32_t simm = (int32_t)((instr[2] & S_MASK(13)) << 19) >> 19;
		dstpc = (int32_t)((instr[1] & S_MASK(22)) << 10) + simm;
		bound = 1;

	} else
		errno = EINVAL;

	if (boundp != NULL)
		*boundp = bound;

	return (dstpc);
}
