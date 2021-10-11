/*
 * Copyright (c) 1993,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_boot.c	1.12	99/05/04 SMI"

/*
 * Bootstrap the linker/loader.
 */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/kobj.h>
#include <sys/elf.h>
#include <sys/bootsvcs.h>
#include <sys/kobj_impl.h>

/*
 * We don't use the global offset table, but
 * ld may throw in an UNDEFINED reference in
 * our symbol table.
 */
#pragma	weak		_GLOBAL_OFFSET_TABLE_

#define	MASK(n)		((1<<(n))-1)
#define	IN_RANGE(v, n)	((-(1<<((n)-1))) <= (v) && (v) < (1<<((n)-1)))

#define	roundup		ALIGN

/*
 * Boot transfers control here. At this point,
 * we haven't relocated our own symbols, so the
 * world (as we know it) is pretty small right now.
 */
void
_kobj_boot(syscallp, dvec, bootops, ebp)
	struct boot_syscalls *syscallp;
	void *dvec;
	struct bootops *bootops;
	Elf32_Boot *ebp;
{
	Elf32_Shdr *section[24];	/* cache */
	val_t bootaux[BA_NUM];
	struct bootops *bop;
	Elf32_Phdr *phdr;
	auxv_t *auxv = NULL;
	uint_t sh, sh_num, sh_size;
	uint_t end, edata = 0;
	int i;

	bop = (dvec) ? *(struct bootops **)bootops : bootops;

	for (i = 0; i < BA_NUM; i++)
		bootaux[i].ba_val = NULL;
	/*
	 * Check the bootstrap vector.
	 */
	for (; ebp->eb_tag != EB_NULL; ebp++) {
		switch (ebp->eb_tag) {
		case EB_AUXV:
			auxv = (auxv_t *)ebp->eb_un.eb_ptr;
			break;
		case EB_DYNAMIC:
			bootaux[BA_DYNAMIC].ba_ptr = (void *)ebp->eb_un.eb_ptr;
			break;
		}
	}
	if (auxv == NULL)
		return;
	/*
	 * Now the aux vector.
	 */
	for (; auxv->a_type != AT_NULL; auxv++) {
		switch (auxv->a_type) {
		case AT_PHDR:
			bootaux[BA_PHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_PHENT:
			bootaux[BA_PHENT].ba_val = auxv->a_un.a_val;
			break;
		case AT_PHNUM:
			bootaux[BA_PHNUM].ba_val = auxv->a_un.a_val;
			break;
		case AT_PAGESZ:
			bootaux[BA_PAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_SUN_LDELF:
			bootaux[BA_LDELF].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDSHDR:
			bootaux[BA_LDSHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDNAME:
			bootaux[BA_LDNAME].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LPAGESZ:
			bootaux[BA_LPAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_SUN_CPU:
			bootaux[BA_CPU].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_MMU:
			bootaux[BA_MMU].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_ENTRY:
			bootaux[BA_ENTRY].ba_ptr = auxv->a_un.a_ptr;
			break;
		}
	}

	sh = (uint_t)bootaux[BA_LDSHDR].ba_ptr;
	sh_num = ((Elf32_Ehdr *)bootaux[BA_LDELF].ba_ptr)->e_shnum;
	sh_size = ((Elf32_Ehdr *)bootaux[BA_LDELF].ba_ptr)->e_shentsize;
	/*
	 * Build cache table for section addresses.
	 */
	for (i = 0; i < sh_num; i++) {
		section[i] = (Elf32_Shdr *)sh;
		sh += sh_size;
	}
	/*
	 * Find the end of data
	 * (to allocate bss)
	 */
	phdr = (Elf32_Phdr *)bootaux[BA_PHDR].ba_ptr;
	for (i = 0; i < bootaux[BA_PHNUM].ba_val; i++) {
		if (phdr->p_type == PT_LOAD &&
		    (phdr->p_flags & PF_W) && (phdr->p_flags & PF_X)) {
			edata = end = phdr->p_vaddr + phdr->p_memsz;
			break;
		}
		phdr = (Elf32_Phdr *)((uint_t)phdr + bootaux[BA_PHENT].ba_val);
	}
	if (edata == NULL)
		return;

	/*
	 * Find the symbol table, and then loop
	 * through the symbols adjusting their
	 * values to reflect where the sections
	 * were loaded.
	 */
	for (i = 1; i < sh_num; i++) {
		Elf32_Shdr *shp;
		Elf32_Sym *sp;
		uint_t off;

		shp = section[i];
		if (shp->sh_type != SHT_SYMTAB)
			continue;

		for (off = 0; off < shp->sh_size; off += shp->sh_entsize) {
			sp = (Elf32_Sym *)(shp->sh_addr + off);

			if (sp->st_shndx == SHN_ABS ||
			    sp->st_shndx == SHN_UNDEF)
				continue;
			/*
			 * Assign the addresses for COMMON
			 * symbols even though we haven't
			 * actually allocated bss yet.
			 */
			if (sp->st_shndx == SHN_COMMON) {
				end = ALIGN(end, sp->st_value);
				sp->st_value = end;
				/*
				 * Squirrel it away for later.
				 */
				if (bootaux[BA_BSS].ba_val == 0)
					bootaux[BA_BSS].ba_val = end;
				end += sp->st_size;
				continue;
			} else if (sp->st_shndx > (Elf32_Half)sh_num)
				return;

			/*
			 * Symbol's new address.
			 */
			sp->st_value += section[sp->st_shndx]->sh_addr;
		}
	}
	/*
	 * Allocate bss for COMMON, if any.
	 */
	if (end > edata) {
		unsigned int va, bva;
		unsigned int asize;
		unsigned int align;

		if (bootaux[BA_LPAGESZ].ba_val) {
			asize = bootaux[BA_LPAGESZ].ba_val;
			align = bootaux[BA_LPAGESZ].ba_val;
		} else {
			asize = bootaux[BA_PAGESZ].ba_val;
			align = BO_NO_ALIGN;
		}
		va = roundup(edata, asize);
		bva = roundup(end, asize);

		if (bva > va) {
			bva = (unsigned int)BOP_ALLOC(bop, (caddr_t)va,
				bva - va, align);
			if (bva == NULL)
				return;
		}
		/*
		 * Zero it.
		 */
		for (va = edata; va < end; va++)
			*(char *)va = 0;
		/*
		 * Update the size of data.
		 */
		phdr->p_memsz += (end - edata);
	}
	/*
	 * Relocate our own symbols.  We'll handle the
	 * undefined symbols later.
	 */
	for (i = 1; i < sh_num; i++) {
		Elf32_Shdr *rshp, *shp, *ssp;
		unsigned long baseaddr, reladdr, rend;
		int relocsize;

		rshp = section[i];

		if (rshp->sh_type != SHT_REL)
			continue;
		/*
		 * Get the section being relocated
		 * and the symbol table.
		 */
		shp = section[rshp->sh_info];
		ssp = section[rshp->sh_link];

		reladdr = rshp->sh_addr;
		baseaddr = shp->sh_addr;
		rend = reladdr + rshp->sh_size;
		relocsize = rshp->sh_entsize;
		/*
		 * Loop through relocations.
		 */
		while (reladdr < rend) {
			Elf32_Sym *symref;
			Elf32_Rel *reloc;
			register unsigned long stndx;
			unsigned long off, *offptr;
			long value;
			int rtype;

			reloc = (Elf32_Rel *)reladdr;
			off = reloc->r_offset;
			rtype = ELF32_R_TYPE(reloc->r_info);
			stndx = ELF32_R_SYM(reloc->r_info);

			reladdr += relocsize;

			if (rtype == R_386_NONE) {
				continue;
			}
			off += baseaddr;
			/*
			 * if R_386_RELATIVE, simply add base addr
			 * to reloc location
			 */
			if (rtype == R_386_RELATIVE) {
				value = baseaddr;
			} else {
				register unsigned int symoff, symsize;

				symsize = ssp->sh_entsize;

				for (symoff = 0; stndx; stndx--)
					symoff += symsize;
				symref = (Elf32_Sym *)(ssp->sh_addr + symoff);

				/*
				 * Check for bad symbol index.
				 */
				if (symoff > ssp->sh_size)
					return;
				/*
				 * Just bind our own symbols at this point.
				 */
				if (symref->st_shndx == SHN_UNDEF) {
					continue;
				}

				value = symref->st_value;
				if (ELF32_ST_BIND(symref->st_info) !=
				    STB_LOCAL) {
					/*
					 * If PC-relative, subtract ref addr.
					 */
					if (rtype == R_386_PC32 ||
					    rtype == R_386_PLT32 ||
					    rtype == R_386_GOTPC)
						value -= off;
				}
			}
			offptr = (unsigned long *)off;
			/*
			 * insert value calculated at reference point
			 * 2 cases - normal byte order aligned, normal byte
			 * order unaligned.
			 */
			switch (rtype) {
			case R_386_PC32:
			case R_386_32:
			case R_386_PLT32:
			case R_386_RELATIVE:
				*offptr += value;
				break;
			default:
				return;
			}
			/*
			 * We only need to do it once.
			 */
			reloc->r_info = ELF32_R_INFO(stndx, R_386_NONE);
		} /* while */
	}
	/*
	 * Done relocating all of our *defined*
	 * symbols, so we hand off.
	 */
	kobj_init(syscallp, dvec, bootops, bootaux);
}
