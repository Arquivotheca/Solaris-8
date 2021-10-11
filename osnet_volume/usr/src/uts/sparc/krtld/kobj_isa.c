/*
 * Copyright (c) 1994, 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_isa.c	1.9	99/05/04 SMI"

/*
 * Miscellaneous ISA-specific code.
 */
#include <sys/types.h>
#include <sys/elf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/archsystm.h>

extern	int	use_iflush;

/*
 * Check that an ELF header corresponds to this machine's
 * instruction set architecture.  Used by kobj_load_module()
 * to not get confused by a misplaced driver or kernel module
 * built for a different ISA.
 */
int
elf_mach_ok(Ehdr *h)
{
#ifdef __sparcv9
	/*
	 * XX64	This should eventually be folded into a v9-isa specific file
	 */
#ifdef _ELF64
	return (h->e_ident[EI_DATA] == ELFDATA2MSB &&
	    (h->e_machine == EM_SPARCV9));
#else /* _ELF64 */
	return (h->e_ident[EI_DATA] == ELFDATA2MSB &&
	    (h->e_machine == EM_SPARC32PLUS));
#endif /* _ELF64 */
#else /* __sparcv9 */
	return (h->e_ident[EI_DATA] == ELFDATA2MSB &&
	    (h->e_machine == EM_SPARC || h->e_machine == EM_SPARC32PLUS));
#endif /* __sparcv9 */
}


/*
 * return non-zero for a bad-address
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
 */
void
kobj_sync_instruction_memory(caddr_t addr, size_t len)
{
	caddr_t	eaddr = addr + len;

	if (!use_iflush)
		return;

	while (addr < eaddr) {
		doflush(addr);
		addr += 8;
	}
}

/*
 * Calculate memory image required for relocatable object.
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
