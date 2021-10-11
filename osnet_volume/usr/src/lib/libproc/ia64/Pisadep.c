/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pisadep.c	1.1	99/05/04 SMI"

#include <sys/types.h>
#include <errno.h>

#include "Pcontrol.h"

#define	M_PLT_NRSV		1	/* reserved PLT entries */
#define	M_PLT_ENTSIZE		16	/* size of each PLT entry */

#define	M_MODRM_DISP32_EBX	0xa3	/* ModR/M byte = disp32[ebx] */

/*
 * x86 PLT entry:
 *      8048738:  ff 25 c8 45 05 08   jmp    *0x80545c8  <OFFSET_INTO_GOT>
 *      804873e:  68 20 00 00 00      pushl  $0x20
 *      8048743:  e9 70 ff ff ff      jmp    0xffffff70  <80486b8> <&.plt>
 *
 * The first time around OFFSET_INTO_GOT contains the address of the pushl;
 * this forces resolution to go to the PLT's first entry (which is a call).
 *
 * The Nth time around, the OFFSET_INTO_GOT actually contains the resolved
 * address of the symbol(name), so the jmp is direct.
 *
 * The final complication is when going from one shared object to another:
 * in this case the GOT's address is in %ebx.
 */
uintptr_t
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr, int *boundp)
{
#ifdef __i386	/* XXX Merced -- fix me */
	map_info_t *mp = Paddr2mptr(P, pltaddr);
	file_info_t *fp;

	uint32_t addr, inst;
	int bound;

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 || pltaddr < fp->file_plt_base ||
	    pltaddr >= fp->file_plt_base + fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	/*
	 * Read the target of the jmp instruction and the byte which will
	 * tell us if the branch is relative to %ebx.
	 */
	if (Pread(P, &addr, sizeof (addr), pltaddr + 2) != sizeof (addr) ||
	    Pread(P, &inst, sizeof (inst), pltaddr + 1) != sizeof (inst))
		return (NULL);

	/*
	 * If we're relative to %ebx, get its value and add it (the base
	 * address of the GOT) to the offset.
	 */
	if ((inst & 0xff) == M_MODRM_DISP32_EBX) {
		prgreg_t ebx;

		if (Pgetareg(P, EBX, &ebx) == -1)
			return (NULL);

		addr += ebx;
	}

	/*
	 * Dereference the address by reading from it.  If it's the address
	 * of the pushl instruction shown above (pltaddr + 6), then we know
	 * this entry has not yet been bound by the dynamic linker.
	 */
	if (Pread(P, &addr, sizeof (addr), addr) != sizeof (addr))
		return (NULL);

	if (addr == (pltaddr + 6)) {
		size_t i = (pltaddr - fp->file_plt_base -
		    M_PLT_NRSV * M_PLT_ENTSIZE) / M_PLT_ENTSIZE;

		uintptr_t r_addr = fp->file_jmp_rel + i * sizeof (Elf32_Rel);
		Elf32_Rel r;

		if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
		    (i = ELF32_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {

			Elf_Data *data = fp->file_dynsym.sym_data;
			Elf32_Sym *symp = &(((Elf32_Sym *)data->d_buf)[i]);

			addr = symp->st_value + fp->file_dyn_base;
			bound = 0;

		} else
			return (NULL);
	} else
		bound = 1;

	if (boundp != NULL)
		*boundp = bound;

	return (addr);
#endif	/* XXX Merced -- fix me */
}
