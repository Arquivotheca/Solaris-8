/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_gelf.c	1.1	99/08/11 SMI"

#include <sys/isa_defs.h>
#include <sys/link.h>
#include <strings.h>
#include <stdlib.h>

#include <mdb/mdb_debug.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_gelf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb.h>

#define	GST_GROW	2	/* Mutable symbol table growth multiplier */
#define	GST_DEFSZ	16	/* Mutable symbol table initial size */

#define	GST_NVFLG	(MDB_NV_EXTNAME | MDB_NV_SILENT)

static mdb_gelf_file_t *
gelf_sect_init(mdb_gelf_file_t *gf)
{
	mdb_gelf_sect_t *gsp, *shstr = &gf->gf_sects[gf->gf_ehdr.e_shstrndx];
	GElf_Half i, npbit = 0;
	GElf_Shdr *shp;
	GElf_Phdr *gpp;

	if (gf->gf_mode == GF_PROGRAM)
		gf->gf_ehdr.e_shnum = 0; /* Simplifies other code paths */

	if (gf->gf_ehdr.e_shnum == 0)
		return (gf); /* If no section headers we're done here */

	if (IOP_SEEK(gf->gf_io, shstr->gs_shdr.sh_offset, SEEK_SET) == -1) {
		warn("failed to seek %s to shdr strings", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	shstr->gs_data = mdb_alloc(shstr->gs_shdr.sh_size, UM_SLEEP);

	if (IOP_READ(gf->gf_io, shstr->gs_data, shstr->gs_shdr.sh_size) !=
	    shstr->gs_shdr.sh_size) {
		warn("failed to read %s shdr strings", IOP_NAME(gf->gf_io));
		mdb_free(shstr->gs_data, shstr->gs_shdr.sh_size);
		return (NULL);
	}

	for (gsp = gf->gf_sects, i = 0; i < gf->gf_ehdr.e_shnum; i++, gsp++) {
		shp = &gsp->gs_shdr;
		gsp->gs_name = (const char *)shstr->gs_data + shp->sh_name;

		if (shp->sh_type == SHT_PROGBITS && (shp->sh_flags & SHF_ALLOC))
			npbit++; /* Keep count for ET_REL code below */
	}

	/*
	 * If the file is of type ET_REL, we would still like to provide file
	 * i/o using the mdb_gelf_rw() function defined below.  To simplify
	 * things, we forge up a sequence of Phdrs based on Shdrs which have
	 * been marked SHF_ALLOC and are of type SHT_PROGBITS.  We convert
	 * relevant Shdr fields to their Phdr equivalents, and then set the
	 * p_vaddr (virtual base address) to the section's file offset.
	 * This allows us to relocate a given symbol by simply incrementing
	 * its st_value by the file offset of the section corresponding to
	 * its st_shndx, and then perform i/o to read or write the symbol's
	 * value in the object file.
	 */
	if (gf->gf_ehdr.e_type == ET_REL && npbit != 0) {
		gf->gf_phdrs = mdb_zalloc(sizeof (GElf_Phdr) * npbit, UM_SLEEP);
		gf->gf_ehdr.e_phnum = npbit;
		gf->gf_npload = npbit;

		gpp = gf->gf_phdrs;
		gsp = gf->gf_sects;

		for (i = 0; i < gf->gf_ehdr.e_shnum; i++, gsp++) {
			shp = &gsp->gs_shdr;

			if ((shp->sh_type == SHT_PROGBITS) &&
			    (shp->sh_flags & SHF_ALLOC)) {
				gpp->p_type = PT_LOAD;
				gpp->p_flags = PF_R;

				if (shp->sh_flags & SHF_EXECINSTR)
					gpp->p_flags |= PF_X;
				if (shp->sh_flags & SHF_WRITE)
					gpp->p_flags |= PF_W;

				gpp->p_offset = shp->sh_offset;
				gpp->p_vaddr = shp->sh_offset;
				gpp->p_filesz = shp->sh_size;
				gpp->p_memsz = shp->sh_size;
				gpp->p_align = shp->sh_addralign;

				gpp++;
			}
		}
	}

	return (gf);
}

static void *
gelf_sect_load(mdb_gelf_file_t *gf, mdb_gelf_sect_t *gsp)
{
	ssize_t nbytes;

	if (gsp->gs_data != NULL)
		return (gsp->gs_data);

	mdb_dprintf(MDB_DBG_ELF, "loading %s:%s (%lu bytes)\n",
	    IOP_NAME(gf->gf_io), gsp->gs_name, (ulong_t)gsp->gs_shdr.sh_size);

	gsp->gs_data = mdb_alloc(gsp->gs_shdr.sh_size, UM_SLEEP);

	if (IOP_SEEK(gf->gf_io, gsp->gs_shdr.sh_offset, SEEK_SET) == -1) {
		warn("failed to seek to start of %s:%s",
		    IOP_NAME(gf->gf_io), gsp->gs_name);
		goto err;
	}

	nbytes = IOP_READ(gf->gf_io, gsp->gs_data, gsp->gs_shdr.sh_size);

	if (nbytes < 0) {
		warn("failed to read %s:%s", IOP_NAME(gf->gf_io), gsp->gs_name);
		goto err;
	}

	if (nbytes < gsp->gs_shdr.sh_size) {
		mdb_dprintf(MDB_DBG_ELF, "only %ld of %llu bytes of %s:%s "
		    "could be read\n", (long)nbytes, (u_longlong_t)
		    gsp->gs_shdr.sh_size, IOP_NAME(gf->gf_io), gsp->gs_name);
		bzero((char *)gsp->gs_data + nbytes,
		    (size_t)gsp->gs_shdr.sh_size - nbytes);
	}

	return (gsp->gs_data);

err:
	mdb_free(gsp->gs_data, sizeof (gsp->gs_shdr.sh_size));
	gsp->gs_data = NULL;
	return (NULL);
}

static GElf_Shdr *
gelf32_to_shdr(const Elf32_Shdr *src, GElf_Shdr *dst)
{
	if (src != NULL) {
		dst->sh_name = src->sh_name;
		dst->sh_type = src->sh_type;
		dst->sh_flags = src->sh_flags;
		dst->sh_addr = src->sh_addr;
		dst->sh_offset = src->sh_offset;
		dst->sh_size = src->sh_size;
		dst->sh_link = src->sh_link;
		dst->sh_info = src->sh_info;
		dst->sh_addralign = src->sh_addralign;
		dst->sh_entsize = src->sh_entsize;

		return (dst);
	}

	return (NULL);
}

static GElf_Shdr *
gelf64_to_shdr(const Elf64_Shdr *src, GElf_Shdr *dst)
{
	if (src != NULL) {
		bcopy(src, dst, sizeof (Elf64_Shdr));
		return (dst);
	}

	return (NULL);
}

static mdb_gelf_file_t *
gelf_shdrs_init(mdb_gelf_file_t *gf, size_t shdr_size,
    GElf_Shdr *(*elf2gelf)(const void *, GElf_Shdr *))
{
	caddr_t shdrs, shp;
	GElf_Half i;

	mdb_gelf_sect_t *gsp;
	size_t nbytes;

	mdb_dprintf(MDB_DBG_ELF, "loading %s section headers (%hu entries)\n",
	    IOP_NAME(gf->gf_io), gf->gf_ehdr.e_shnum);

	if (gf->gf_ehdr.e_shnum == 0)
		return (gf);

	if (IOP_SEEK(gf->gf_io, (off64_t)gf->gf_ehdr.e_shoff, SEEK_SET) == -1) {
		warn("failed to seek %s to shdrs", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	nbytes = shdr_size * gf->gf_ehdr.e_shnum;
	shdrs = mdb_alloc(nbytes, UM_SLEEP);

	if (IOP_READ(gf->gf_io, shdrs, nbytes) != nbytes) {
		warn("failed to read %s section headers", IOP_NAME(gf->gf_io));
		mdb_free(shdrs, nbytes);
		return (NULL);
	}

	gf->gf_sects = mdb_zalloc(sizeof (mdb_gelf_sect_t) *
	    gf->gf_ehdr.e_shnum, UM_SLEEP);

	shp = shdrs;
	gsp = gf->gf_sects;

	for (i = 0; i < gf->gf_ehdr.e_shnum; i++, shp += shdr_size, gsp++)
		(void) elf2gelf(shp, &gsp->gs_shdr);

	mdb_free(shdrs, nbytes);
	return (gf);
}

static GElf_Phdr *
gelf32_to_phdr(const Elf32_Phdr *src, GElf_Phdr *dst)
{
	if (src != NULL) {
		dst->p_type = src->p_type;
		dst->p_offset = src->p_offset;
		dst->p_vaddr = src->p_vaddr;
		dst->p_paddr = src->p_paddr;
		dst->p_filesz = src->p_filesz;
		dst->p_memsz = src->p_memsz;
		dst->p_flags = src->p_flags;
		dst->p_align = src->p_align;

		return (dst);
	}

	return (NULL);
}

static GElf_Phdr *
gelf64_to_phdr(const Elf64_Phdr *src, GElf_Phdr *dst)
{
	if (src != NULL) {
		bcopy(src, dst, sizeof (Elf64_Phdr));
		return (dst);
	}

	return (NULL);
}

static int
gelf_phdr_compare(const void *lp, const void *rp)
{
	GElf_Phdr *lhs = (GElf_Phdr *)lp;
	GElf_Phdr *rhs = (GElf_Phdr *)rp;

	/*
	 * If both p_type fields are PT_LOAD, we want to sort by vaddr.
	 * Exception is that p_vaddr == 0 means ignore this (put at end).
	 */
	if (lhs->p_type == PT_LOAD && rhs->p_type == PT_LOAD) {
		if (lhs->p_vaddr != rhs->p_vaddr) {
			if (lhs->p_vaddr == 0)
				return (1); /* lhs is "greater" */

			if (rhs->p_vaddr == 0)
				return (-1); /* rhs is "greater" */

			return (lhs->p_vaddr > rhs->p_vaddr ? 1 : -1);
		}

		return (0);
	}

	/*
	 * If the p_type fields don't match, we need to make sure that PT_LOAD
	 * entries are considered "less" (i.e. move towards the beginning
	 * of the array we are sorting)
	 */
	if (lhs->p_type != rhs->p_type) {
		if (lhs->p_type == PT_LOAD)
			return (-1); /* rhs is "greater" */

		if (rhs->p_type == PT_LOAD)
			return (1); /* lhs is "greater" */

		return (lhs->p_type > rhs->p_type ? 1 : -1);
	}

	/*
	 * If the p_type is the same but neither is PT_LOAD, then
	 * just sort by file offset (doesn't really matter)
	 */
	if (lhs->p_offset != rhs->p_offset)
		return (lhs->p_offset > rhs->p_offset ? 1 : -1);

	return (0);
}

static mdb_gelf_file_t *
gelf_phdrs_init(mdb_gelf_file_t *gf, size_t phdr_size,
    GElf_Phdr *(*elf2gelf)(const void *, GElf_Phdr *))
{
	caddr_t phdrs, php;
	GElf_Half i;

	GElf_Phdr *gpp;
	size_t nbytes;

	mdb_dprintf(MDB_DBG_ELF, "loading %s program headers (%hu entries)\n",
	    IOP_NAME(gf->gf_io), gf->gf_ehdr.e_phnum);

	if (gf->gf_ehdr.e_phnum == 0)
		return (gf);

	if (IOP_SEEK(gf->gf_io, (off64_t)gf->gf_ehdr.e_phoff, SEEK_SET) == -1) {
		warn("failed to seek %s to phdrs", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	nbytes = phdr_size * gf->gf_ehdr.e_phnum;
	phdrs = mdb_alloc(nbytes, UM_SLEEP);

	if (IOP_READ(gf->gf_io, phdrs, nbytes) != nbytes) {
		warn("failed to read %s program headers", IOP_NAME(gf->gf_io));
		mdb_free(phdrs, nbytes);
		return (NULL);
	}

	gf->gf_phdrs = mdb_zalloc(sizeof (GElf_Phdr) *
	    gf->gf_ehdr.e_phnum, UM_SLEEP);

	php = phdrs;
	gpp = gf->gf_phdrs;

	/*
	 * Iterate through the list of phdrs locating those that are of type
	 * PT_LOAD; increment gf_npload so we know how many are loadable.
	 */
	for (i = 0; i < gf->gf_ehdr.e_phnum; i++, php += phdr_size, gpp++) {
		(void) elf2gelf(php, gpp);
		if (gpp->p_type != PT_LOAD)
			continue;

		mdb_dprintf(MDB_DBG_ELF, "PT_LOAD va=0x%llx flags=0x%x "
		    "memsz=%llu filesz=%llu off=%llu\n", (u_longlong_t)
		    gpp->p_vaddr, gpp->p_flags, (u_longlong_t)gpp->p_memsz,
		    (u_longlong_t)gpp->p_filesz, (u_longlong_t)gpp->p_offset);

		gf->gf_npload++;
	}

	/*
	 * Now we sort the phdrs array using a comparison routine which
	 * arranges for the PT_LOAD phdrs with non-zero virtual addresses
	 * to come first sorted by virtual address.  This means that we
	 * can access the complete phdr table by examining the array
	 * gf->gf_phdrs[0 .. gf->gf_ehdr.e_phnum - 1], and we can access a
	 * sorted array of valid PT_LOAD pdhrs by examining the array
	 * gf->gf_phdrs[0 .. gf->gf_npload - 1].
	 */
	qsort(gf->gf_phdrs, gf->gf_ehdr.e_phnum, sizeof (GElf_Phdr),
	    gelf_phdr_compare);

	/*
	 * Locate the PT_DYNAMIC Phdr if one is present; we save this
	 * Phdr pointer in gf->gf_dynp for future use.
	 */
	for (gpp = gf->gf_phdrs, i = 0; i < gf->gf_ehdr.e_phnum; i++, gpp++) {
		if (gpp->p_type == PT_DYNAMIC) {
			mdb_dprintf(MDB_DBG_ELF, "PT_DYNAMIC "
			    "filesize = %lluULL off=%lluULL\n",
			    (u_longlong_t)gpp->p_filesz,
			    (u_longlong_t)gpp->p_offset);

			gf->gf_dynp = gpp;
			break;
		}
	}

	mdb_free(phdrs, nbytes);
	return (gf);
}

static GElf_Dyn *
gelf32_to_dyn(const Elf32_Dyn *src, GElf_Dyn *dst)
{
	if (src != NULL) {
		dst->d_tag = (GElf_Xword)(Elf32_Word)src->d_tag;
		dst->d_un.d_ptr = src->d_un.d_ptr;
		return (dst);
	}

	return (NULL);
}

static GElf_Dyn *
gelf64_to_dyn(const Elf64_Dyn *src, GElf_Dyn *dst)
{
	if (src != NULL) {
		bcopy(src, dst, sizeof (Elf64_Dyn));
		return (dst);
	}

	return (NULL);
}

static GElf_Xword
gelf_dyn_lookup(mdb_gelf_file_t *gf, GElf_Xword tag)
{
	size_t i;

	for (i = 0; i < gf->gf_ndyns; i++) {
		if (gf->gf_dyns[i].d_tag == tag)
			return (gf->gf_dyns[i].d_un.d_val);
	}

	return ((GElf_Xword)-1L);
}

static GElf_Dyn *
gelf_dyns_init(mdb_gelf_file_t *gf, size_t dyn_size,
    GElf_Dyn *(*elf2gelf)(const void *, GElf_Dyn *))
{
	size_t nbytes, ndyns, i;
	caddr_t dyns, dp;
	GElf_Dyn *gdp;

	off64_t dyn_addr;

	if (gf->gf_dyns != NULL)
		return (gf->gf_dyns);	/* Already loaded */

	if (gf->gf_dynp == NULL)
		return (NULL);		/* No PT_DYNAMIC entry was found */

	nbytes = gf->gf_dynp->p_filesz;
	ndyns = nbytes / dyn_size;

	/*
	 * If this is an executable, then p_vaddr is an absolute address;
	 * we need to subtract the virtual base address of the mapping.
	 */
	if (gf->gf_ehdr.e_type == ET_EXEC && gf->gf_npload != 0)
		dyn_addr = gf->gf_dynp->p_vaddr - gf->gf_phdrs->p_vaddr;
	else
		dyn_addr = gf->gf_dynp->p_vaddr;

	mdb_dprintf(MDB_DBG_ELF, "loading _DYNAMIC[] (%lu entries) "
	    "from offset %llx\n", (ulong_t)ndyns, (longlong_t)dyn_addr);

	if (IOP_SEEK(gf->gf_io, dyn_addr, SEEK_SET) == -1) {
		warn("failed to seek %s to _DYNAMIC", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	dyns = mdb_alloc(nbytes, UM_SLEEP);

	if (IOP_READ(gf->gf_io, dyns, nbytes) != nbytes) {
		warn("failed to read %s:_DYNAMIC", IOP_NAME(gf->gf_io));
		mdb_free(dyns, nbytes);
		return (NULL);
	}

	gf->gf_dyns = mdb_zalloc(sizeof (GElf_Dyn) * ndyns, UM_SLEEP);
	gf->gf_ndyns = ndyns;

	dp = dyns;
	gdp = gf->gf_dyns;

	for (i = 0; i < ndyns; i++, dp += dyn_size, gdp++)
		(void) elf2gelf(dp, gdp);

	mdb_free(dyns, nbytes);
	return (gf->gf_dyns);
}

static mdb_gelf_file_t *
gelf32_init(mdb_gelf_file_t *gf, const Elf32_Ehdr *ehdr)
{
	/*
	 * Convert the Elf32_Ehdr to a GElf_Ehdr
	 */
	bcopy(ehdr->e_ident, gf->gf_ehdr.e_ident, EI_NIDENT);

	gf->gf_ehdr.e_type = ehdr->e_type;
	gf->gf_ehdr.e_machine = ehdr->e_machine;
	gf->gf_ehdr.e_version = ehdr->e_version;
	gf->gf_ehdr.e_entry = ehdr->e_entry;
	gf->gf_ehdr.e_phoff = ehdr->e_phoff;
	gf->gf_ehdr.e_shoff = ehdr->e_shoff;
	gf->gf_ehdr.e_flags = ehdr->e_flags;
	gf->gf_ehdr.e_ehsize = ehdr->e_ehsize;
	gf->gf_ehdr.e_phentsize = ehdr->e_phentsize;
	gf->gf_ehdr.e_phnum = ehdr->e_phnum;
	gf->gf_ehdr.e_shentsize = ehdr->e_shentsize;
	gf->gf_ehdr.e_shnum = ehdr->e_shnum;
	gf->gf_ehdr.e_shstrndx = ehdr->e_shstrndx;

	/*
	 * Initialize the section and program headers.  We skip initializing
	 * the section headers if this is a program image because they are
	 * not loadable and thus we can't get at them.
	 */
	if (gf->gf_mode == GF_FILE && gelf_shdrs_init(gf, sizeof (Elf32_Shdr),
	    (GElf_Shdr *(*)(const void *, GElf_Shdr *))gelf32_to_shdr) == NULL)
		return (NULL);

	if (gelf_phdrs_init(gf, sizeof (Elf32_Phdr),
	    (GElf_Phdr *(*)(const void *, GElf_Phdr *))gelf32_to_phdr) == NULL)
		return (NULL);

	return (gf);
}

static mdb_gelf_file_t *
gelf64_init(mdb_gelf_file_t *gf, Elf64_Ehdr *ehdr)
{
	/*
	 * Save a copy of the ELF file header
	 */
	bcopy(ehdr, &gf->gf_ehdr, sizeof (Elf64_Ehdr));

	/*
	 * Initialize the section and program headers.  We skip initializing
	 * the section headers if this is a program image because they are
	 * not loadable and thus we can't get at them.
	 */
	if (gf->gf_mode == GF_FILE && gelf_shdrs_init(gf, sizeof (Elf64_Shdr),
	    (GElf_Shdr *(*)(const void *, GElf_Shdr *))gelf64_to_shdr) == NULL)
		return (NULL);

	if (gelf_phdrs_init(gf, sizeof (Elf64_Phdr),
	    (GElf_Phdr *(*)(const void *, GElf_Phdr *))gelf64_to_phdr) == NULL)
		return (NULL);

	return (gf);
}

int
mdb_gelf_check(mdb_io_t *io, Elf32_Ehdr *ehp, GElf_Half etype)
{
#ifdef _BIG_ENDIAN
	uchar_t order = ELFDATA2MSB;
#else
	uchar_t order = ELFDATA2LSB;
#endif
	ssize_t nbytes;

	(void) IOP_SEEK(io, (off64_t)0L, SEEK_SET);
	nbytes = IOP_READ(io, ehp, sizeof (Elf32_Ehdr));

	if (nbytes == -1) {
		if (etype != ET_NONE)
			warn("failed to read ELF header from %s", IOP_NAME(io));
		return (-1);
	}

	if (nbytes != sizeof (Elf32_Ehdr) ||
	    bcmp(&ehp->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0) {
		if (etype != ET_NONE)
			warn("%s is not an ELF file\n", IOP_NAME(io));
		return (-1);
	}

	if (ehp->e_ident[EI_DATA] != order) {
		warn("ELF file %s has different endianness from debugger\n",
		    IOP_NAME(io));
		return (-1);
	}

	if (ehp->e_version != EV_CURRENT) {
		warn("ELF file %s uses different ELF version (%lu) than "
		    "debugger (%u)\n", IOP_NAME(io),
		    (ulong_t)ehp->e_version, EV_CURRENT);
		return (-1);
	}

	if (etype != ET_NONE && ehp->e_type != etype) {
		warn("ELF file %s is not of the expected type\n", IOP_NAME(io));
		return (-1);
	}

	return (0);
}

mdb_gelf_file_t *
mdb_gelf_create(mdb_io_t *io, GElf_Half etype, int mode)
{
	union {
		Elf32_Ehdr h32;
		Elf64_Ehdr h64;
	} ehdr;

	mdb_gelf_file_t *gf = mdb_zalloc(sizeof (mdb_gelf_file_t), UM_SLEEP);

	ASSERT(mode == GF_FILE || mode == GF_PROGRAM);
	gf->gf_mode = mode;

	/*
	 * Assign the i/o backend now, but don't hold it until we're sure
	 * we're going to succeed; otherwise the caller will be responsible
	 * for mdb_io_destroy()ing it.
	 */
	gf->gf_io = io;

	if (mdb_gelf_check(io, &ehdr.h32, etype) == -1)
		goto err;

	switch (ehdr.h32.e_ident[EI_CLASS]) {
	case ELFCLASS32:
		gf = gelf32_init(gf, &ehdr.h32);
		break;

	case ELFCLASS64:
		if (IOP_SEEK(io, (off64_t)0L, SEEK_SET) == -1) {
			warn("failed to seek %s", IOP_NAME(io));
			goto err;
		}

		if (IOP_READ(io, &ehdr.h64, sizeof (ehdr.h64)) !=
		    sizeof (ehdr.h64)) {
			warn("failed to read ELF header from %s", IOP_NAME(io));
			goto err;
		}

		gf = gelf64_init(gf, &ehdr.h64);
		break;

	default:
		warn("%s is an unsupported ELF class: %u\n",
		    IOP_NAME(io), ehdr.h32.e_ident[EI_CLASS]);
		goto err;
	}

	if (gf != NULL && gelf_sect_init(gf) != NULL) {
		gf->gf_io = mdb_io_hold(io);
		return (gf);
	}

err:
	if (gf != NULL) {
		if (gf->gf_sects != NULL) {
			mdb_free(gf->gf_sects, gf->gf_ehdr.e_shnum *
			    sizeof (mdb_gelf_sect_t));
		}
		mdb_free(gf, sizeof (mdb_gelf_file_t));
	}
	return (NULL);
}

void
mdb_gelf_destroy(mdb_gelf_file_t *gf)
{
	mdb_gelf_sect_t *gsp;
	GElf_Half i;

	for (gsp = gf->gf_sects, i = 0; i < gf->gf_ehdr.e_shnum; i++, gsp++) {
		if (gsp->gs_data != NULL)
			mdb_free(gsp->gs_data, gsp->gs_shdr.sh_size);
	}

	mdb_free(gf->gf_sects,
	    gf->gf_ehdr.e_shnum * sizeof (mdb_gelf_sect_t));

	mdb_free(gf->gf_phdrs,
	    gf->gf_ehdr.e_phnum * sizeof (GElf_Phdr));

	mdb_io_rele(gf->gf_io);
	mdb_free(gf, sizeof (mdb_gelf_file_t));
}

static int
gelf32_sym_compare(const void *lp, const void *rp)
{
	Elf32_Sym *lhs = *((Elf32_Sym **)lp);
	Elf32_Sym *rhs = *((Elf32_Sym **)rp);

	if (lhs->st_value != rhs->st_value)
		return (lhs->st_value > rhs->st_value ? 1 : -1);

	return (0);
}

static int
gelf64_sym_compare(const void *lp, const void *rp)
{
	Elf64_Sym *lhs = *((Elf64_Sym **)lp);
	Elf64_Sym *rhs = *((Elf64_Sym **)rp);

	if (lhs->st_value != rhs->st_value)
		return (lhs->st_value > rhs->st_value ? 1 : -1);

	return (0);
}

static void
gelf32_symtab_sort(mdb_gelf_symtab_t *gst)
{
	Elf32_Sym **sympp = (Elf32_Sym **)gst->gst_asmap;
	mdb_var_t *v;

	mdb_nv_rewind(&gst->gst_nv);

	while ((v = mdb_nv_advance(&gst->gst_nv)) != NULL) {
		Elf32_Sym *sym = MDB_NV_COOKIE(v);
		if (sym->st_value != 0 &&
		    (ELF32_ST_BIND(sym->st_info) != STB_LOCAL || sym->st_size))
			*sympp++ = sym;
	}

	gst->gst_aslen = (size_t)(sympp - (Elf32_Sym **)gst->gst_asmap);
	ASSERT(gst->gst_aslen <= gst->gst_asrsv);

	qsort(gst->gst_asmap, gst->gst_aslen,
	    sizeof (Elf32_Sym *), gelf32_sym_compare);
}

static void
gelf32_symtab_init(mdb_gelf_symtab_t *gst)
{
	const char *base = (const char *)gst->gst_ssect->gs_data;
	Elf32_Sym *sym = gst->gst_dsect->gs_data;
	mdb_nv_t *nv = &gst->gst_nv;

	Elf32_Word ss_size = gst->gst_ssect->gs_shdr.sh_size;
	size_t asrsv = 0;
	GElf_Word i, n;

	n = gst->gst_dsect->gs_shdr.sh_size /
	    gst->gst_dsect->gs_shdr.sh_entsize;

	for (i = 0; i < n; i++, sym++) {
		const char *name = base + sym->st_name;
		uchar_t type = ELF32_ST_TYPE(sym->st_info);

		if (type > STT_FILE || type == STT_SECTION)
			continue; /* skip sections and unknown types */

		if (sym->st_name >= ss_size || name[0] < '!' || name[0] > '~') {
			if (sym->st_name >= ss_size || name[0] != '\0') {
				warn("ignoring %s symbol [%u]: invalid name\n",
				    gst->gst_dsect->gs_name, i);
				sym->st_name = 0;
			}
			continue; /* skip corrupt or empty names */
		}

		(void) mdb_nv_insert(nv, name, NULL, (uintptr_t)sym, GST_NVFLG);

		if (sym->st_value != 0 &&
		    (ELF32_ST_BIND(sym->st_info) != STB_LOCAL || sym->st_size))
			asrsv++; /* reserve space in the address map */
	}

	if (gst->gst_ehdr->e_type == ET_REL) {
		GElf_Word smax = gst->gst_ehdr->e_shnum;
		mdb_gelf_sect_t *gsp;

		for (sym = gst->gst_dsect->gs_data, i = 0; i < n; i++, sym++) {
			if (sym->st_shndx > SHN_UNDEF && sym->st_shndx < smax) {
				gsp = &gst->gst_file->gf_sects[sym->st_shndx];
				sym->st_value += gsp->gs_shdr.sh_offset;

				if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL ||
				    sym->st_size != 0)
					asrsv++; /* reserve space in asmap */
			}
		}
	}

	gst->gst_asmap = mdb_alloc(sizeof (Elf32_Sym *) * asrsv, UM_SLEEP);
	gst->gst_asrsv = asrsv;

	gelf32_symtab_sort(gst);
}

static void
gelf64_symtab_sort(mdb_gelf_symtab_t *gst)
{
	Elf64_Sym **sympp = (Elf64_Sym **)gst->gst_asmap;
	mdb_var_t *v;

	mdb_nv_rewind(&gst->gst_nv);

	while ((v = mdb_nv_advance(&gst->gst_nv)) != NULL) {
		Elf64_Sym *sym = MDB_NV_COOKIE(v);
		if (sym->st_value != 0 &&
		    (ELF64_ST_BIND(sym->st_info) != STB_LOCAL || sym->st_size))
			*sympp++ = sym;
	}

	gst->gst_aslen = (size_t)(sympp - (Elf64_Sym **)gst->gst_asmap);
	ASSERT(gst->gst_aslen <= gst->gst_asrsv);

	qsort(gst->gst_asmap, gst->gst_aslen,
	    sizeof (Elf64_Sym *), gelf64_sym_compare);
}

static void
gelf64_symtab_init(mdb_gelf_symtab_t *gst)
{
	const char *base = (const char *)gst->gst_ssect->gs_data;
	Elf64_Sym *sym = gst->gst_dsect->gs_data;
	mdb_nv_t *nv = &gst->gst_nv;

	Elf64_Xword ss_size = gst->gst_ssect->gs_shdr.sh_size;
	size_t asrsv = 0;
	GElf_Word i, n;

	n = gst->gst_dsect->gs_shdr.sh_size /
	    gst->gst_dsect->gs_shdr.sh_entsize;

	for (i = 0; i < n; i++, sym++) {
		const char *name = base + sym->st_name;
		uchar_t type = ELF64_ST_TYPE(sym->st_info);

		if (type > STT_FILE || type == STT_SECTION)
			continue; /* skip sections and unknown types */

		if (sym->st_name >= ss_size || name[0] < '!' || name[0] > '~') {
			if (sym->st_name >= ss_size || name[0] != '\0') {
				warn("ignoring %s symbol [%u]: invalid name\n",
				    gst->gst_dsect->gs_name, i);
				sym->st_name = 0;
			}
			continue; /* skip corrupt or empty names */
		}

		(void) mdb_nv_insert(nv, name, NULL, (uintptr_t)sym, GST_NVFLG);

		if (sym->st_value != 0 &&
		    (ELF64_ST_BIND(sym->st_info) != STB_LOCAL || sym->st_size))
			asrsv++; /* reserve space in the address map */
	}

	if (gst->gst_ehdr->e_type == ET_REL) {
		GElf_Word smax = gst->gst_ehdr->e_shnum;
		mdb_gelf_sect_t *gsp;

		for (sym = gst->gst_dsect->gs_data, i = 0; i < n; i++, sym++) {
			if (sym->st_shndx > SHN_UNDEF && sym->st_shndx < smax) {
				gsp = &gst->gst_file->gf_sects[sym->st_shndx];
				sym->st_value += gsp->gs_shdr.sh_offset;

				if (ELF64_ST_BIND(sym->st_info) != STB_LOCAL ||
				    sym->st_size != 0)
					asrsv++; /* reserve space in asmap */
			}
		}
	}

	gst->gst_asmap = mdb_alloc(sizeof (Elf64_Sym *) * asrsv, UM_SLEEP);
	gst->gst_asrsv = asrsv;

	gelf64_symtab_sort(gst);
}

mdb_gelf_symtab_t *
mdb_gelf_symtab_create_file(mdb_gelf_file_t *gf, const char *dsname,
    const char *ssname)
{
	mdb_gelf_symtab_t *gst;
	mdb_gelf_sect_t *gsp;
	GElf_Half i;

	gst = mdb_alloc(sizeof (mdb_gelf_symtab_t), UM_SLEEP);
	mdb_nv_create(&gst->gst_nv);

	gst->gst_asmap = NULL;
	gst->gst_aslen = 0;
	gst->gst_asrsv = 0;
	gst->gst_ehdr = &gf->gf_ehdr;
	gst->gst_file = gf;
	gst->gst_dsect = NULL;
	gst->gst_ssect = NULL;

	for (gsp = gf->gf_sects, i = 0; i < gf->gf_ehdr.e_shnum; i++, gsp++) {
		if (strcmp(gsp->gs_name, dsname) == 0) {
			gst->gst_dsect = gsp;
			break;
		}
	}

	for (gsp = gf->gf_sects, i = 0; i < gf->gf_ehdr.e_shnum; i++, gsp++) {
		if (strcmp(gsp->gs_name, ssname) == 0) {
			gst->gst_ssect = gsp;
			break;
		}
	}

	if (gst->gst_dsect == NULL || gst->gst_ssect == NULL)
		goto err; /* Failed to locate data or string section */

	if (gelf_sect_load(gf, gst->gst_dsect) == NULL)
		goto err; /* Failed to load data section */

	if (gelf_sect_load(gf, gst->gst_ssect) == NULL)
		goto err; /* Failed to load string section */

	if (gf->gf_ehdr.e_ident[EI_CLASS] == ELFCLASS32)
		gelf32_symtab_init(gst);
	else
		gelf64_symtab_init(gst);

	return (gst);

err:
	mdb_nv_destroy(&gst->gst_nv);
	mdb_free(gst, sizeof (mdb_gelf_symtab_t));
	return (NULL);
}

mdb_gelf_symtab_t *
mdb_gelf_symtab_create_raw(const GElf_Ehdr *ehdr, const void *dshdr,
    void *ddata, const void *sshdr, void *sdata)
{
	mdb_gelf_symtab_t *gst;

	gst = mdb_alloc(sizeof (mdb_gelf_symtab_t), UM_SLEEP);
	mdb_nv_create(&gst->gst_nv);

	gst->gst_asmap = NULL;
	gst->gst_aslen = 0;
	gst->gst_asrsv = 0;
	gst->gst_ehdr = ehdr;
	gst->gst_file = NULL; /* Flag for raw symtab */

	gst->gst_dsect = mdb_zalloc(sizeof (mdb_gelf_sect_t), UM_SLEEP);
	gst->gst_dsect->gs_name = ".symtab";
	gst->gst_dsect->gs_data = ddata;

	gst->gst_ssect = mdb_zalloc(sizeof (mdb_gelf_sect_t), UM_SLEEP);
	gst->gst_ssect->gs_name = ".strtab";
	gst->gst_ssect->gs_data = sdata;

	if (ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		(void) gelf32_to_shdr(dshdr, &gst->gst_dsect->gs_shdr);
		(void) gelf32_to_shdr(sshdr, &gst->gst_ssect->gs_shdr);
		gelf32_symtab_init(gst);
	} else {
		(void) gelf64_to_shdr(dshdr, &gst->gst_dsect->gs_shdr);
		(void) gelf64_to_shdr(sshdr, &gst->gst_ssect->gs_shdr);
		gelf64_symtab_init(gst);
	}

	return (gst);
}

mdb_gelf_symtab_t *
mdb_gelf_symtab_create_dynamic(mdb_gelf_file_t *gf)
{
	GElf_Addr dt_symtab, dt_strtab, dt_hash;
	GElf_Xword dt_syment, dt_strsz;

	mdb_gelf_symtab_t *gst;
	uint_t hash_h[2];
	off64_t base = 0;

	ASSERT(gf->gf_mode == GF_PROGRAM);

	/*
	 * Read in and cache the array of GElf_Dyn structures from the
	 * PT_DYNAMIC phdr.  Abort if this is not possible.
	 */
	if (gf->gf_ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
		(void) gelf_dyns_init(gf, sizeof (Elf32_Dyn),
		    (GElf_Dyn *(*)(const void *, GElf_Dyn *))gelf32_to_dyn);
	} else {
		(void) gelf_dyns_init(gf, sizeof (Elf64_Dyn),
		    (GElf_Dyn *(*)(const void *, GElf_Dyn *))gelf64_to_dyn);
	}

	/*
	 * Pre-fetch all the DT_* entries we will need for creating the
	 * dynamic symbol table; abort if any are missing.
	 */
	if ((dt_hash = gelf_dyn_lookup(gf, DT_HASH)) == -1L) {
		warn("failed to get DT_HASH for %s\n", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	if ((dt_symtab = gelf_dyn_lookup(gf, DT_SYMTAB)) == -1L) {
		warn("failed to get DT_SYMTAB for %s\n", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	if ((dt_syment = gelf_dyn_lookup(gf, DT_SYMENT)) == -1L) {
		warn("failed to get DT_SYMENT for %s\n", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	if ((dt_strtab = gelf_dyn_lookup(gf, DT_STRTAB)) == -1L) {
		warn("failed to get DT_STRTAB for %s\n", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	if ((dt_strsz = gelf_dyn_lookup(gf, DT_STRSZ)) == -1L) {
		warn("failed to get DT_STRSZ for %s\n", IOP_NAME(gf->gf_io));
		return (NULL);
	}

	/*
	 * If this is an executable, then DT_HASH is an absolute address;
	 * we need to subtract the virtual base address of the mapping.
	 */
	if (gf->gf_ehdr.e_type == ET_EXEC && gf->gf_npload != 0)
		base = (off64_t)gf->gf_phdrs->p_vaddr;

	/*
	 * Read in the header for the DT_HASH: this consists of nbucket
	 * and nchain values (nchain is the number of hashed symbols).
	 */
	if (IOP_SEEK(gf->gf_io, (off64_t)dt_hash - base, SEEK_SET) == -1) {
		warn("failed to seek ELF file to start of DT_HASH");
		return (NULL);
	}

	if (IOP_READ(gf->gf_io, hash_h, sizeof (hash_h)) != sizeof (hash_h)) {
		warn("failed to read DT_HASH header");
		return (NULL);
	}

	gst = mdb_zalloc(sizeof (mdb_gelf_symtab_t), UM_SLEEP);
	mdb_nv_create(&gst->gst_nv);

	gst->gst_asmap = NULL;
	gst->gst_aslen = 0;
	gst->gst_asrsv = 0;
	gst->gst_ehdr = &gf->gf_ehdr;
	gst->gst_file = gf;

	gst->gst_dsect = mdb_zalloc(sizeof (mdb_gelf_sect_t), UM_SLEEP);
	gst->gst_dsect->gs_name = ".dynsym";
	gst->gst_dsect->gs_shdr.sh_offset = dt_symtab - (GElf_Addr)base;
	gst->gst_dsect->gs_shdr.sh_size = hash_h[1] * dt_syment;
	gst->gst_dsect->gs_shdr.sh_entsize = dt_syment;

	gst->gst_ssect = mdb_zalloc(sizeof (mdb_gelf_sect_t), UM_SLEEP);
	gst->gst_ssect->gs_name = ".dynstr";
	gst->gst_ssect->gs_shdr.sh_offset = dt_strtab - (GElf_Addr)base;
	gst->gst_ssect->gs_shdr.sh_size = dt_strsz;
	gst->gst_ssect->gs_shdr.sh_entsize = 0;

	if (gelf_sect_load(gf, gst->gst_dsect) == NULL)
		goto err;

	if (gelf_sect_load(gf, gst->gst_ssect) == NULL)
		goto err;

	if (gf->gf_ehdr.e_ident[EI_CLASS] == ELFCLASS32)
		gelf32_symtab_init(gst);
	else
		gelf64_symtab_init(gst);

	return (gst);

err:
	mdb_gelf_symtab_destroy(gst);
	return (NULL);
}

mdb_gelf_symtab_t *
mdb_gelf_symtab_create_mutable(void)
{
	mdb_gelf_symtab_t *gst;
	static GElf_Ehdr ehdr;

	gst = mdb_zalloc(sizeof (mdb_gelf_symtab_t), UM_SLEEP);
	mdb_nv_create(&gst->gst_nv);
	gst->gst_ehdr = &ehdr;

	if (ehdr.e_version == 0) {
#ifdef	_LP64
		uchar_t class = ELFCLASS64;
#else
		uchar_t class = ELFCLASS32;
#endif

#ifdef _BIG_ENDIAN
		uchar_t data = ELFDATA2MSB;
#else
		uchar_t data = ELFDATA2LSB;
#endif
		/*
		 * Since all mutable symbol tables will use a native Ehdr,
		 * we can just have a single static copy which they all
		 * point to and we only need initialize once.
		 */
		ehdr.e_ident[EI_MAG0] = ELFMAG0;
		ehdr.e_ident[EI_MAG1] = ELFMAG1;
		ehdr.e_ident[EI_MAG2] = ELFMAG2;
		ehdr.e_ident[EI_MAG3] = ELFMAG3;
		ehdr.e_ident[EI_CLASS] = class;
		ehdr.e_ident[EI_DATA] = data;
		ehdr.e_ident[EI_VERSION] = EV_CURRENT;
		ehdr.e_type = ET_NONE;
		ehdr.e_version = EV_CURRENT;
	}

	return (gst);
}

void
mdb_gelf_symtab_destroy(mdb_gelf_symtab_t *gst)
{
	if (gst->gst_file == NULL) {
		if (gst->gst_dsect == NULL && gst->gst_ssect == NULL) {
			mdb_var_t *v;

			mdb_nv_rewind(&gst->gst_nv);
			while ((v = mdb_nv_advance(&gst->gst_nv)) != NULL) {
				char *name = (char *)mdb_nv_get_name(v);
				mdb_gelf_dsym_t *dsp = mdb_nv_get_cookie(v);

				mdb_free(name, strlen(name) + 1);
				mdb_free(dsp, sizeof (mdb_gelf_dsym_t));
			}

		} else {
			mdb_free(gst->gst_dsect, sizeof (mdb_gelf_sect_t));
			mdb_free(gst->gst_ssect, sizeof (mdb_gelf_sect_t));
		}

	} else if (gst->gst_file->gf_mode == GF_PROGRAM) {
		mdb_gelf_sect_t *dsect = gst->gst_dsect;
		mdb_gelf_sect_t *ssect = gst->gst_ssect;

		if (dsect->gs_data != NULL)
			mdb_free(dsect->gs_data, dsect->gs_shdr.sh_size);
		if (ssect->gs_data != NULL)
			mdb_free(ssect->gs_data, ssect->gs_shdr.sh_size);

		mdb_free(gst->gst_dsect, sizeof (mdb_gelf_sect_t));
		mdb_free(gst->gst_ssect, sizeof (mdb_gelf_sect_t));
	}

	mdb_nv_destroy(&gst->gst_nv);
	mdb_free(gst->gst_asmap, gst->gst_asrsv * sizeof (void *));
	mdb_free(gst, sizeof (mdb_gelf_symtab_t));
}

size_t
mdb_gelf_symtab_size(mdb_gelf_symtab_t *gst)
{
	return (mdb_nv_size(&gst->gst_nv));
}

static GElf_Sym *
gelf32_to_sym(const Elf32_Sym *src, GElf_Sym *dst)
{
	if (src != NULL) {
		dst->st_name = src->st_name;
		dst->st_info = src->st_info;
		dst->st_other = src->st_other;
		dst->st_shndx = src->st_shndx;
		dst->st_value = src->st_value;
		dst->st_size = src->st_size;
		return (dst);
	}

	return (NULL);
}

static GElf_Sym *
gelf64_to_sym(const Elf64_Sym *src, GElf_Sym *dst)
{
	if (src != NULL) {
		bcopy(src, dst, sizeof (GElf_Sym));
		return (dst);
	}

	return (NULL);
}

/*ARGSUSED*/
static GElf_Sym *
gelf64_nocopy(const Elf64_Sym *src, GElf_Sym *dst)
{
	return ((GElf_Sym *)src);
}

static const void *
gelf32_sym_search(const Elf32_Sym **asmap, size_t aslen, uintptr_t addr)
{
	size_t mid, lo = 0, hi = aslen - 1;
	const Elf32_Sym *symp;

	if (aslen == 0)
		return (NULL);

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (addr >= asmap[mid]->st_value)
			lo = mid;
		else
			hi = mid;
	}

	if (addr < asmap[hi]->st_value)
		symp = asmap[lo];
	else
		symp = asmap[hi];

	if (mdb.m_symdist == 0) {
		if (addr >= symp->st_value &&
		    addr <= symp->st_value + symp->st_size)
			return (symp);
	} else {
		if (addr >= symp->st_value &&
		    addr <= symp->st_value + mdb.m_symdist)
			return (symp);
	}

	return (NULL);
}

static const void *
gelf64_sym_search(const Elf64_Sym **asmap, size_t aslen, uintptr_t addr)
{
	size_t mid, lo = 0, hi = aslen - 1;
	const Elf64_Sym *symp;

	if (aslen == 0)
		return (NULL);

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (addr >= asmap[mid]->st_value)
			lo = mid;
		else
			hi = mid;
	}

	if (addr < asmap[hi]->st_value)
		symp = asmap[lo];
	else
		symp = asmap[hi];

	if (mdb.m_symdist == 0) {
		if (addr >= symp->st_value &&
		    addr <= symp->st_value + symp->st_size)
			return (symp);
	} else {
		if (addr >= symp->st_value &&
		    addr <= symp->st_value + mdb.m_symdist)
			return (symp);
	}

	return (NULL);
}

const char *
mdb_gelf_sym_name(mdb_gelf_symtab_t *gst, const GElf_Sym *sym)
{
	const mdb_gelf_dsym_t *dsp;

	if (gst->gst_ssect != NULL)
		return ((const char *)gst->gst_ssect->gs_data + sym->st_name);

	if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32)
		dsp = gelf32_sym_search(gst->gst_asmap,
		    gst->gst_aslen, sym->st_value);
	else
		dsp = gelf64_sym_search(gst->gst_asmap,
		    gst->gst_aslen, sym->st_value);

	if (dsp != NULL)
		return (mdb_nv_get_name(dsp->ds_var));

	return (NULL);
}

int
mdb_gelf_sym_closer(const GElf_Sym *s1, const GElf_Sym *s2, uintptr_t addr)
{
	uintptr_t v1 = (uintptr_t)s1->st_value;
	uintptr_t v2 = (uintptr_t)s2->st_value;

	uintptr_t d1 = v1 > addr ? v1 - addr : addr - v1;
	uintptr_t d2 = v2 > addr ? v2 - addr : addr - v2;

	return (d1 < d2);
}

int
mdb_gelf_symtab_lookup_by_addr(mdb_gelf_symtab_t *gst, uintptr_t addr,
    uint_t flags, char *buf, size_t nbytes, GElf_Sym *sym)
{
	union {
		const mdb_gelf_dsym_t *dsp;
		const Elf32_Sym *s32;
		const Elf64_Sym *s64;
	} u;

	const char *name;

	if (gst == NULL)
		return (set_errno(EMDB_NOSYMADDR));

	if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		u.s32 = gelf32_sym_search(gst->gst_asmap, gst->gst_aslen, addr);
		if (gelf32_to_sym(u.s32, sym) == NULL)
			return (set_errno(EMDB_NOSYMADDR));
	} else {
		u.s64 = gelf64_sym_search(gst->gst_asmap, gst->gst_aslen, addr);
		if (gelf64_to_sym(u.s64, sym) == NULL)
			return (set_errno(EMDB_NOSYMADDR));
	}

	if ((flags & GST_EXACT) && (sym->st_value != addr))
		return (set_errno(EMDB_NOSYMADDR));

	if (gst->gst_ssect != NULL)
		name = (const char *)gst->gst_ssect->gs_data + sym->st_name;
	else
		name = mdb_nv_get_name(u.dsp->ds_var);

	(void) strncpy(buf, name, nbytes - 1);
	buf[nbytes - 1] = '\0';
	return (0);
}

int
mdb_gelf_symtab_lookup_by_name(mdb_gelf_symtab_t *gst, const char *name,
    GElf_Sym *sym)
{
	mdb_var_t *v;

	if (gst != NULL && (v = mdb_nv_lookup(&gst->gst_nv, name)) != NULL) {
		if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32)
			(void) gelf32_to_sym(mdb_nv_get_cookie(v), sym);
		else
			(void) gelf64_to_sym(mdb_nv_get_cookie(v), sym);
		return (0);
	}

	return (set_errno(EMDB_NOSYM));
}

int
mdb_gelf_symtab_lookup_by_file(mdb_gelf_symtab_t *gst, const char *file,
    const char *name, GElf_Sym *sym)
{
	GElf_Sym *(*s2gelf)(const void *, GElf_Sym *);
	size_t sym_size;
	caddr_t sp, ep;
	mdb_var_t *v;

	if (gst == NULL)
		return (set_errno(EMDB_NOSYM));

	if ((v = mdb_nv_lookup(&gst->gst_nv, file)) == NULL)
		return (set_errno(EMDB_NOOBJ));

	if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		s2gelf = (GElf_Sym *(*)(const void *, GElf_Sym *))gelf32_to_sym;
		sym_size = sizeof (Elf32_Sym);
	} else {
		s2gelf = (GElf_Sym *(*)(const void *, GElf_Sym *))gelf64_to_sym;
		sym_size = sizeof (Elf64_Sym);
	}

	(void) s2gelf(mdb_nv_get_cookie(v), sym);

	if (GELF_ST_TYPE(sym->st_info) != STT_FILE)
		return (set_errno(EMDB_NOOBJ));

	ep = (caddr_t)gst->gst_dsect->gs_data + gst->gst_dsect->gs_shdr.sh_size;
	sp = (caddr_t)mdb_nv_get_cookie(v);

	/*
	 * We assume that symbol lookups scoped by source file name are only
	 * relevant for userland debugging and are a relatively rare request,
	 * and so we use a simple but inefficient linear search with copying.
	 */
	for (sp += sym_size; sp < ep; sp += sym_size) {
		(void) s2gelf(sp, sym);	/* Convert native symbol to GElf */

		if (GELF_ST_TYPE(sym->st_info) > STT_FUNC ||
		    GELF_ST_BIND(sym->st_info) != STB_LOCAL)
			break;		/* End of this file's locals */

		if (strcmp(mdb_gelf_sym_name(gst, sym), name) == 0)
			return (0);
	}

	return (set_errno(EMDB_NOSYM));
}

void
mdb_gelf_symtab_iter(mdb_gelf_symtab_t *gst, int (*func)(void *,
    const GElf_Sym *, const char *), void *private)
{
	GElf_Sym *(*s2gelf)(const void *, GElf_Sym *);
	GElf_Sym sym, *symp;
	size_t sym_size;

	if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		s2gelf = (GElf_Sym *(*)(const void *, GElf_Sym *))gelf32_to_sym;
		sym_size = sizeof (Elf32_Sym);
	} else {
		s2gelf = (GElf_Sym *(*)(const void *, GElf_Sym *))gelf64_nocopy;
		sym_size = sizeof (Elf64_Sym);
	}

	/*
	 * If this is a mutable symbol table, we iterate over the hash table
	 * of symbol names; otherwise we go iterate over the data buffer.  For
	 * non-mutable tables, this means that ::nm will show all symbols,
	 * including those with duplicate names (not present in gst_nv).
	 */
	if (gst->gst_file == NULL && gst->gst_dsect == NULL) {
		mdb_var_t *v;

		mdb_nv_rewind(&gst->gst_nv);
		while ((v = mdb_nv_advance(&gst->gst_nv)) != NULL) {
			symp = s2gelf(mdb_nv_get_cookie(v), &sym);
			if (func(private, symp, mdb_nv_get_name(v)) == -1)
				break;
		}

	} else {
		const char *sbase = gst->gst_ssect->gs_data;
		caddr_t sp = gst->gst_dsect->gs_data;
		caddr_t ep = sp + gst->gst_dsect->gs_shdr.sh_size;

		for (; sp < ep; sp += sym_size) {
			symp = s2gelf(sp, &sym);
			if (func(private, symp, sbase + symp->st_name) == -1)
				break;
		}
	}
}

static void
gelf_sym_to_32(const GElf_Sym *src, Elf32_Sym *dst)
{
	dst->st_name = src->st_name;
	dst->st_info = src->st_info;
	dst->st_other = src->st_other;
	dst->st_shndx = src->st_shndx;
	dst->st_value = (Elf32_Addr)src->st_value;
	dst->st_size = (Elf32_Word)src->st_size;
}

static void
gelf_sym_to_64(const GElf_Sym *src, Elf64_Sym *dst)
{
	bcopy(src, dst, sizeof (Elf64_Sym));
}

void
mdb_gelf_symtab_insert(mdb_gelf_symtab_t *gst,
    const char *name, const GElf_Sym *symp)
{
	mdb_gelf_dsym_t *dsp;
	mdb_var_t *v;

	ASSERT(gst->gst_file == NULL && gst->gst_dsect == NULL);
	v = mdb_nv_lookup(&gst->gst_nv, name);

	if (v == NULL) {
		char *s = mdb_alloc(strlen(name) + 1, UM_SLEEP);
		(void) strcpy(s, name);

		dsp = mdb_alloc(sizeof (mdb_gelf_dsym_t), UM_SLEEP);
		dsp->ds_var = mdb_nv_insert(&gst->gst_nv, s, NULL,
		    (uintptr_t)dsp, GST_NVFLG);

		gst->gst_aslen++;
		ASSERT(gst->gst_aslen == mdb_nv_size(&gst->gst_nv));

		if (gst->gst_aslen > gst->gst_asrsv) {
			mdb_free(gst->gst_asmap,
			    sizeof (void *) * gst->gst_asrsv);

			gst->gst_asrsv = gst->gst_asrsv != 0 ?
			    gst->gst_asrsv * GST_GROW : GST_DEFSZ;

			gst->gst_asmap = mdb_alloc(sizeof (void *) *
			    gst->gst_asrsv, UM_SLEEP);
		}
	} else
		dsp = mdb_nv_get_cookie(v);

	mdb_dprintf(MDB_DBG_ELF, "added symbol (\"%s\", %llx)\n",
	    name, (u_longlong_t)symp->st_value);

	bcopy(symp, &dsp->ds_sym, sizeof (GElf_Sym));

	if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		gelf_sym_to_32(symp, &dsp->ds_u.ds_s32);
		gelf32_symtab_sort(gst);
	} else {
		gelf_sym_to_64(symp, &dsp->ds_u.ds_s64);
		gelf64_symtab_sort(gst);
	}
}

void
mdb_gelf_symtab_delete(mdb_gelf_symtab_t *gst,
    const char *name, GElf_Sym *symp)
{
	mdb_var_t *v;

	ASSERT(gst->gst_file == NULL && gst->gst_dsect == NULL);
	v = mdb_nv_lookup(&gst->gst_nv, name);

	if (v != NULL) {
		char *name = (char *)mdb_nv_get_name(v);
		mdb_gelf_dsym_t *dsp = mdb_nv_get_cookie(v);

		if (symp != NULL)
			bcopy(&dsp->ds_sym, symp, sizeof (GElf_Sym));

		mdb_dprintf(MDB_DBG_ELF, "removed symbol (\"%s\", %llx)\n",
		    name, (u_longlong_t)dsp->ds_sym.st_value);

		mdb_nv_remove(&gst->gst_nv, v);
		gst->gst_aslen--;
		ASSERT(gst->gst_aslen == mdb_nv_size(&gst->gst_nv));

		mdb_free(name, strlen(name) + 1);
		mdb_free(dsp, sizeof (mdb_gelf_dsym_t));

		if (gst->gst_ehdr->e_ident[EI_CLASS] == ELFCLASS32)
			gelf32_symtab_sort(gst);
		else
			gelf64_symtab_sort(gst);
	}
}

static const GElf_Phdr *
gelf_phdr_lookup(mdb_gelf_file_t *gf, uintptr_t addr)
{
	const GElf_Phdr *gpp = gf->gf_phdrs;
	GElf_Half i;

	for (i = 0; i < gf->gf_npload; i++, gpp++) {
		if (addr >= gpp->p_vaddr && addr < gpp->p_vaddr + gpp->p_memsz)
			return (gpp);
	}

	return (NULL);
}

ssize_t
mdb_gelf_rw(mdb_gelf_file_t *gf, void *buf, size_t nbytes, uintptr_t addr,
    ssize_t (*prw)(mdb_io_t *, void *, size_t), mdb_gelf_rw_t rw)
{
	ssize_t resid = nbytes;

	while (resid != 0) {
		const GElf_Phdr *php = gelf_phdr_lookup(gf, addr);

		uintptr_t mapoff;
		ssize_t memlen, filelen, len = 0;
		off64_t off;

		if (php == NULL)
			break; /* No mapping for this address */

		mapoff = addr - php->p_vaddr;
		memlen = MIN(resid, php->p_memsz - mapoff);
		filelen = MIN(resid, php->p_filesz - mapoff);
		off = (off64_t)php->p_offset + mapoff;

		if (filelen > 0 && (IOP_SEEK(gf->gf_io, off, SEEK_SET) != off ||
		    (len = prw(gf->gf_io, buf, filelen)) <= 0))
			break;

		if (rw == GIO_READ && len == filelen && filelen < memlen) {
			bzero((char *)buf + len, memlen - filelen);
			len += memlen - filelen;
		}

		resid -= len;
		addr += len;
		buf = (char *)buf + len;
	}

	if (resid == nbytes && nbytes != 0)
		return (set_errno(EMDB_NOMAP));

	return (nbytes - resid);
}
