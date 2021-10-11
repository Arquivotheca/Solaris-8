/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_reloc.c	1.13	99/05/04 SMI"

/*
 * x86 relocation code.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/bootconf.h>
#include <sys/modctl.h>
#include <sys/elf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>

#include "reloc.h"


/*
 * Probe Discovery
 */

void *__tnf_probe_list_head;
void *__tnf_tag_list_head;

#define	PROBE_MARKER_SYMBOL	"__tnf_probe_version_1"
#define	PROBE_CONTROL_BLOCK_LINK_OFFSET 4
#define	TAG_MARKER_SYMBOL	"__tnf_tag_version_1"

/*
 * The kernel run-time linker calls this to try to resolve a reference
 * it can't otherwise resolve.  We see if it's marking a probe control
 * block; if so, we do the resolution and return 0.  If not, we return
 * 1 to show that we can't resolve it, either.
 */
int
tnf_reloc_resolve(char *symname, Elf32_Addr *value_p, unsigned long *offset_p)
{
	if (strcmp(symname, PROBE_MARKER_SYMBOL) == 0) {
		*value_p = (long)__tnf_probe_list_head;
		__tnf_probe_list_head = (void *)*offset_p;
		*offset_p += PROBE_CONTROL_BLOCK_LINK_OFFSET;
		return (0);
	}
	if (strcmp(symname, TAG_MARKER_SYMBOL) == 0) {
		*value_p = (long)__tnf_tag_list_head;
		__tnf_tag_list_head = (void *)*offset_p;
		return (0);
	}
	return (1);
}

int
/* ARGSUSED2 */
do_relocate(struct module *mp, char *reltbl, Word relshtype, int nreloc,
	int relocsize, Elf32_Addr baseaddr)
{
	unsigned long stndx;
	unsigned long off;	/* can't be register for tnf_reloc_resolve() */
	register unsigned long reladdr, rend;
	register unsigned int rtype;
	long value;
	Elf32_Sym *symref;
	int err = 0;
	int symnum;

	reladdr = (unsigned long)reltbl;
	rend = reladdr + nreloc * relocsize;

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_RELOCATIONS) {
		_kobj_printf(ops, "krtld:\ttype\t\t\toffset      symbol\n");
		_kobj_printf(ops, "krtld:\t\t\t\t\t   value\n");
	}
#endif

	symnum = -1;
	/* loop through relocations */
	while (reladdr < rend) {
		symnum++;
		rtype = ELF32_R_TYPE(((Elf32_Rel *)reladdr)->r_info);
		off = ((Elf32_Rel *)reladdr)->r_offset;
		stndx = ELF32_R_SYM(((Elf32_Rel *)reladdr)->r_info);
		if (stndx >= mp->nsyms) {
			_kobj_printf(ops, "do_relocate: bad strndx %d\n",
			    symnum);
			return (-1);
		}
		reladdr += relocsize;


		if (rtype == R_386_NONE)
			continue;

#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			Elf32_Sym *	symp;
			symp = (Elf32_Sym *)
				(mp->symtbl+(stndx * mp->symhdr->sh_entsize));
			_kobj_printf(ops, "krtld:\t%s\t0x%8x  %s\n",
				conv_reloc_386_type_str(rtype), off,
				(const char *)mp->strings + symp->st_name);
		}
#endif

		if (!(mp->flags & KOBJ_EXEC))
			off += baseaddr;

		/*
		 * if R_386_RELATIVE, simply add base addr
		 * to reloc location
		 */

		if (rtype == R_386_RELATIVE) {
			value = baseaddr;
		} else {
			/*
			 * get symbol table entry - if symbol is local
			 * value is base address of this object
			 */
			symref = (Elf32_Sym *)
				(mp->symtbl+(stndx * mp->symhdr->sh_entsize));

			if (ELF32_ST_BIND(symref->st_info) == STB_LOCAL) {
				/* *** this is different for .o and .so */
				value = symref->st_value;
			} else {
				/*
				 * It's global. Allow weak references.  If
				 * the symbol is undefined, give TNF (the
				 * kernel probes facility) a chance to see
				 * if it's a probe site, and fix it up if so.
				 */
				if (symref->st_shndx == SHN_UNDEF &&
				    tnf_reloc_resolve(mp->strings +
					symref->st_name, &symref->st_value,
					&off) != 0) {
					if (ELF32_ST_BIND(symref->st_info)
					    != STB_WEAK) {
						_kobj_printf(ops,
						    "not found: %s\n",
						    mp->strings +
						    symref->st_name);
						err = 1;
					}
					continue;
				} else { /* symbol found  - relocate */
					/*
					 * calculate location of definition
					 * - symbol value plus base address of
					 * containing shared object
					 */
					value = symref->st_value;

				} /* end else symbol found */
			} /* end global or weak */
		} /* end not R_386_RELATIVE */

		/*
		 * calculate final value -
		 * if PC-relative, subtract ref addr
		 */
		if (IS_PC_RELATIVE(rtype))
			value -= off;

#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS)
			_kobj_printf(ops, "krtld:\t\t\t\t0x%8x 0x%8x\n",
				off, value);
#endif

		if (do_reloc(rtype, (unsigned char *)off, (Word *)&value,
		    (const char *)mp->strings + symref->st_name,
		    mp->filename) == 0)
			err = 1;

	} /* end of while loop */
	if (err)
		return (-1);
	return (0);
}

int
do_relocations(struct module *mp)
{
	uint_t shn;
	Elf32_Shdr *shp, *rshp;
	uint_t nreloc;

	/* do the relocations */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		rshp = (Elf32_Shdr *)
			(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (rshp->sh_type == SHT_RELA) {
			_kobj_printf(ops, "%s can't process type SHT_RELA\n",
			    mp->filename);
			return (1);
		}
		if (rshp->sh_type != SHT_REL)
			continue;
		if (rshp->sh_link != mp->symtbl_section) {
			_kobj_printf(ops, "%s reloc for non-default symtab\n",
			    mp->filename);
			return (-1);
		}
		if (rshp->sh_info >= mp->hdr.e_shnum) {
			_kobj_printf(ops, "do_relocations: %s sh_info out of "
			    "range %d\n", mp->filename, shn);
			goto bad;
		}
		nreloc = rshp->sh_size / rshp->sh_entsize;

		/* get the section header that this reloc table refers to */
		shp = (Elf32_Shdr *)
		    (mp->shdrs + rshp->sh_info * mp->hdr.e_shentsize);

		/*
		 * Do not relocate any section that isn't loaded into memory.
		 * Most commonly this will skip over the .rela.stab* sections
		 */
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS)
			_kobj_printf(ops,
				"krtld: relocating: file=%s section=%d\n",
				mp->filename, shn);
#endif

		if (do_relocate(mp, (char *)rshp->sh_addr, rshp->sh_type,
		    nreloc, rshp->sh_entsize, shp->sh_addr) < 0) {
			_kobj_printf(ops,
			    "do_relocations: %s do_relocate failed\n",
			    mp->filename);
			goto bad;
		}
		kobj_free((void *)rshp->sh_addr, rshp->sh_size);
		rshp->sh_addr = 0;
	}
	return (0);
bad:
	kobj_free((void *)rshp->sh_addr, rshp->sh_size);
	return (-1);
}
