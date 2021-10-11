/*
 * Copyright (c) 1991-1993, 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_reloc.c	1.24	99/06/07 SMI"

/*
 * SPARC relocation code.
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
 * Probe discovery support
 */

void *__tnf_probe_list_head;
void *__tnf_tag_list_head;

#define	PROBE_MARKER_SYMBOL	"__tnf_probe_version_1"
#ifdef _ELF64
#define	PROBE_CONTROL_BLOCK_LINK_OFFSET 8
#else
#define	PROBE_CONTROL_BLOCK_LINK_OFFSET 4
#endif
#define	TAG_MARKER_SYMBOL	"__tnf_tag_version_1"

/*
 * The kernel run-time linker calls this to try to resolve a reference
 * it can't otherwise resolve.  We see if it's marking a probe control
 * block or a probe tag block; if so, we do the resolution and return 0.
 * If not, we return 1 to show that we can't resolve it, either.
 */
int
tnf_reloc_resolve(char *symname, Addr *value_p,
#ifdef _ELF64
    Elf64_Sxword *addend_p,
#else
    Elf32_Sword *addend_p,
#endif
    long *offset_p)
{
	if (strcmp(symname, PROBE_MARKER_SYMBOL) == 0) {
		*addend_p = 0;
		*value_p = (Addr)__tnf_probe_list_head;
		__tnf_probe_list_head = (void *)*offset_p;
		*offset_p += PROBE_CONTROL_BLOCK_LINK_OFFSET;
		return (0);
	}
	if (strcmp(symname, TAG_MARKER_SYMBOL) == 0) {
		*addend_p = 0;
		*value_p = (Addr) __tnf_tag_list_head;
		__tnf_tag_list_head = (void *)*offset_p;
		return (0);
	}
	return (1);
}

int
/* ARGSUSED2 */
do_relocate(
	struct module *mp,
	char *reltbl,
	Word relshtype,
	int nreloc,
	int relocsize,
	Addr baseaddr)
{
	Word stndx;
	long off, roff;
	uintptr_t reladdr, rend;
	uint_t rtype;
#ifdef _ELF64
	Elf64_Sxword addend;
#else
	Elf32_Sword addend;
#endif
	Addr value, destination;
	Sym *symref;
	int symnum;
	int err = 0;

	reladdr = (uintptr_t)reltbl;
	rend = reladdr + nreloc * relocsize;

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_RELOCATIONS) {
		_kobj_printf(ops, "krtld:\ttype\t\t\toffset\t   addend"
			"      symbol\n");
		_kobj_printf(ops, "krtld:\t\t\t\t\t   value\n");
	}
#endif
	destination = baseaddr;

	/*
	 * If this machine is loading a module through an alternate address
	 * we need to compute the spot where the actual relocation will
	 * take place.
	 */
	if (mp->destination) {
		int i;
		Shdr * shp;
		shp = (Shdr *)mp->shdrs;
		for (i = 0; i < mp->hdr.e_shnum; i++, shp++) {
			if (shp->sh_addr == baseaddr) {
				if ((shp->sh_flags & SHF_ALLOC) &&
					!(shp->sh_flags & SHF_WRITE))
					destination = (Addr)mp->destination +
						(baseaddr - (Addr)mp->text);
				break;
			}
		}
	}

	symnum = -1;
	/* loop through relocations */
	while (reladdr < rend) {

		symnum++;
		rtype = ELF_R_TYPE(((Rela *)reladdr)->r_info);
		roff = off = ((Rela *)reladdr)->r_offset;
		stndx = ELF_R_SYM(((Rela *)reladdr)->r_info);
		if (stndx >= mp->nsyms) {
			_kobj_printf(ops,
			    "do_relocate: bad strndx %d\n", symnum);
			return (-1);
		}
		if (rtype > R_SPARC_NUM) {
			_kobj_printf(ops, "krtld: invalid relocation type %d",
			    rtype);
			_kobj_printf(ops, " at 0x%llx:", off);
			_kobj_printf(ops, " file=%s\n", mp->filename);
		}
		addend = (long)(((Rela *)reladdr)->r_addend);
		reladdr += relocsize;


#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			Sym *symp;
			symp = (Sym *)
			    (mp->symtbl+(stndx * mp->symhdr->sh_entsize));
			_kobj_printf(ops, "krtld:\t%s",
			    conv_reloc_SPARC_type_str(rtype));
			_kobj_printf(ops, "\t0x%8llx", off);
			_kobj_printf(ops, " 0x%8llx", addend);
			_kobj_printf(ops, "  %s\n",
			    (const char *)mp->strings + symp->st_name);
		}
#endif

		if (rtype == R_SPARC_NONE)
			continue;

		if (!(mp->flags & KOBJ_EXEC))
			off += destination;

		/*
		 * if R_SPARC_RELATIVE, simply add base addr
		 * to reloc location
		 */
		if (rtype == R_SPARC_RELATIVE) {
			value = baseaddr;
		} else {
			/*
			 * get symbol table entry - if symbol is local
			 * value is base address of this object
			 */
			symref = (Sym *)
				(mp->symtbl+(stndx * mp->symhdr->sh_entsize));
			if (ELF_ST_BIND(symref->st_info) == STB_LOCAL) {
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
					&addend, &off) != 0) {
					if (ELF_ST_BIND(symref->st_info)
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
			}
		} /* end not R_SPARC_RELATIVE */

		value += addend;
		if (IS_EXTOFFSET(rtype)) {
			value +=
			(Word) ELF_R_TYPE_DATA(((Rela *)reladdr)->r_info);
		}

		/*
		 * calculate final value -
		 * if PC-relative, subtract ref addr
		 */
		if (IS_PC_RELATIVE(rtype)) {
			if (mp->destination)
				value -= (baseaddr + roff);
			else
				value -= off;
		}

#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			_kobj_printf(ops, "krtld:\t\t\t\t0x%8llx", off);
			_kobj_printf(ops, " 0x%8llx\n", value);
		}
#endif
		if (do_reloc(rtype, (unsigned char *)off, (Xword *)&value,
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
	Shdr *shp, *rshp;
	uint_t nreloc;

	/* do the relocations */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		rshp = (Shdr *)
			(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (rshp->sh_type == SHT_REL) {
			_kobj_printf(ops, "%s can't process type SHT_REL\n",
			    mp->filename);
			return (1);
		}
		if (rshp->sh_type != SHT_RELA)
			continue;
		if (rshp->sh_link != mp->symtbl_section) {
			_kobj_printf(ops, "%s reloc for non-default symtab\n",
			    mp->filename);
			return (-1);
		}
		if (rshp->sh_info >= mp->hdr.e_shnum) {
			_kobj_printf(ops, "do_relocations: %s ", mp->filename);
			_kobj_printf(ops, " sh_info out of range %lld\n", shn);
			goto bad;
		}
		nreloc = rshp->sh_size / rshp->sh_entsize;

		/* get the section header that this reloc table refers to */
		shp = (Shdr *)
		    (mp->shdrs + rshp->sh_info * mp->hdr.e_shentsize);
		/*
		 * Do not relocate any section that isn't loaded into memory.
		 * Most commonly this will skip over the .rela.stab* sections
		 */
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			_kobj_printf(ops, "krtld: relocating: file=%s ",
				mp->filename);
			_kobj_printf(ops, " section=%d\n", shn);
		}
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
