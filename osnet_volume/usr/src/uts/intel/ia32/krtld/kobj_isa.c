/*
 * Copyright (c) 1994,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_isa.c	1.5	99/05/04 SMI"

/*
 * Miscellaneous ISA-specific code.
 */
#include <sys/types.h>
#include <sys/elf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>

/*
 * Check that an ELF header corresponds to this machine's
 * instruction set architecture.  Used by kobj_load_module()
 * to not get confused by a misplaced driver or kernel module
 * built for a different ISA.
 */
int
elf_mach_ok(Elf32_Ehdr *h)
{
	return ((h->e_ident[EI_DATA] == ELFDATA2LSB) &&
	    (h->e_machine == EM_386));
}

/*
 * return non-zero for a bad address
 */
int
kobj_addrcheck(void *xmp, caddr_t adr)
{
	struct module *mp;

	mp = (struct module *)xmp;

	if ((adr >= mp->text && adr < mp->text + mp->text_size) ||
	    (adr >= mp->data && adr < mp->data + mp->data_size))
		return (0); /* ok */
	if (mp->bss && adr >= (caddr_t)mp->bss &&
	    adr < (caddr_t)mp->bss + mp->bss_size)
		return (0);
	return (1);
}


/*
 * Flush instruction cache after updating text
 * 	This is a nop for this machine arch.
 */
/*ARGSUSED*/
void
kobj_sync_instruction_memory(caddr_t addr, size_t len)
{}

/*
 * Calculate memory image required for relocable object.
 */
/* ARGSUSED3 */
int
get_progbits_size(struct module *mp, struct proginfo *tp, struct proginfo *dp,
	struct proginfo *sdp)
{
	struct proginfo *pp;
	uint_t shn;
	Shdr *shp;

	/*
	 * loop through sections to find out how much space we need
	 * for text, data, (also bss that is already assigned)
	 */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
		if (shp->sh_addr != 0) {
			_kobj_printf(ops,
			    "%s non-zero sect addr in input file\n",
			    mp->filename);
			return (-1);
		}
		pp = (shp->sh_flags & SHF_WRITE)? dp : tp;

		if (shp->sh_addralign > pp->align)
			pp->align = shp->sh_addralign;
		pp->size = ALIGN(pp->size, shp->sh_addralign);
		pp->size += shp->sh_size;
	}
	return (0);
}
