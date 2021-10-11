
#pragma ident "@(#)elfbloat.c	1.2 96/12/13 SMI"

/*
 * Utility to expand a class-32 ELF file into a class-64 Elf.
 * See man elf_update(3e) for a list of the relevant fields
 * in the elf structures.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <libelf.h>
#include <link.h>


/* Globals */
static boolean_t	g_verbose = B_FALSE;
static const char *	g_outfile = NULL;



static void
punt(const char * str)
{
	fprintf(stderr, "%s\n", str ? str : "Unknown error occurred");
	if (g_outfile != NULL) {
		unlink(g_outfile);
	}
	exit(-1);
}


static void
bloat_ehdr(Elf32_Ehdr * e32, Elf64_Ehdr * e64)
{
	e64->e_type		= e32->e_type;
	e64->e_machine		= EM_SPARCV9;
	e64->e_version		= e32->e_version;
	e64->e_entry		= e32->e_entry;
	e64->e_flags		= e32->e_flags;
	e64->e_ehsize		= sizeof (Elf64_Ehdr);
	e64->e_phentsize	= sizeof (Elf64_Phdr);
	e64->e_phnum		= e32->e_phnum;
	e64->e_shentsize	= sizeof (Elf64_Shdr);
	e64->e_shnum		= e32->e_shnum;
	e64->e_shstrndx		= e32->e_shstrndx;
}


static void
bloat_phdr(Elf32_Phdr * p32, Elf64_Phdr * p64)
{
	p64->p_type	= p32->p_type;
	p64->p_flags	= p32->p_flags;
	p64->p_offset	= p32->p_offset;
	p64->p_vaddr	= p32->p_vaddr;
	p64->p_paddr	= p32->p_paddr;
	p64->p_filesz	= p32->p_filesz;
	p64->p_memsz	= p32->p_memsz;
	p64->p_align	= p32->p_align;
}


static void
bloat_dynamic(Elf_Scn * scn32, Elf32_Shdr * s32,
		Elf_Scn * scn64, Elf64_Shdr * s64)
{
	size_t ii;
	Elf_Data* d32		= elf_getdata(scn32, NULL);
	Elf_Data* d64		= elf_newdata(scn64);
	size_t count		= s32->sh_size/s32->sh_entsize;
	Elf32_Dyn* dyn32	= (Elf32_Dyn*) d32->d_buf;
	Elf64_Dyn* dyn64	= (Elf64_Dyn*)calloc(count, s64->sh_entsize);


	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}
	if (dyn64 == NULL) {
		perror("elfbloat");
		punt("error converting dynamic section");
	}


	/* fill out the Elf64_Dyn array */
	for (ii = 0; ii < count; ++ii) {
		dyn64[ii].d_tag		= dyn32[ii].d_tag;
		dyn64[ii].d_un.d_val	= dyn32[ii].d_un.d_val;
	}

	/* fill out the Elf_Data structure */
	d64->d_type	= d32->d_type;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= dyn64;
	d64->d_size	= count * s64->sh_entsize;
}


static void
bloat_rela(Elf_Scn* scn32, Elf32_Shdr* s32, Elf_Scn* scn64, Elf64_Shdr* s64)
{
	size_t ii;
	Elf_Data* d32		= elf_getdata(scn32, NULL);
	Elf_Data* d64		= elf_newdata(scn64);
	size_t count		= s32->sh_size/s32->sh_entsize;
	Elf32_Rela* rela32	= (Elf32_Rela*) d32->d_buf;
	Elf64_Rela* rela64	= (Elf64_Rela*)calloc(count, s64->sh_entsize);


	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}
	if (rela64 == NULL) {
		perror("elfbloat");
		punt("error converting relocation section");
	}


	/* fill out the Elf64_Rela array */
	for (ii = 0; ii < count; ++ii) {
		rela64[ii].r_offset	= rela32[ii].r_offset;
		rela64[ii].r_info
			= ELF64_R_INFO((int)ELF32_R_SYM(rela32[ii].r_info),
					(int)ELF32_R_TYPE(rela32[ii].r_info));
		rela64[ii].r_addend	= rela32[ii].r_addend;
	}

	/* fill out the Elf_Data structure */
	d64->d_type	= d32->d_type;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= rela64;
	d64->d_size	= count * s64->sh_entsize;
}


static void
bloat_rel(Elf_Scn* scn32, Elf32_Shdr* s32, Elf_Scn* scn64, Elf64_Shdr* s64)
{
	size_t ii;
	Elf_Data* d32		= elf_getdata(scn32, NULL);
	Elf_Data* d64		= elf_newdata(scn64);
	size_t count		= s32->sh_size/s32->sh_entsize;
	Elf32_Rel* rel32	= (Elf32_Rel*) d32->d_buf;
	Elf64_Rel* rel64	= (Elf64_Rel*)calloc(count, s64->sh_entsize);


	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}
	if (rel64 == NULL) {
		perror("elfbloat");
		punt("error converting relocation section");
	}


	/* fill out the Elf64_Rel array */
	for (ii = 0; ii < count; ++ii) {
		rel64[ii].r_offset = rel32[ii].r_offset;
		rel64[ii].r_info
			= ELF64_R_INFO((int)ELF32_R_SYM(rel32[ii].r_info),
					(int)ELF32_R_TYPE(rel32[ii].r_info));
	}

	/* fill out the Elf_Data structure */
	d64->d_type	= d32->d_type;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= rel64;
	d64->d_size	= count * s64->sh_entsize;
}


static void
bloat_note(Elf_Scn* scn32, Elf32_Shdr* s32, Elf_Scn* scn64, Elf64_Shdr* s64)
{
	size_t ii;
	Elf_Data* d32	= elf_getdata(scn32, NULL);
	Elf_Data* d64	= elf_newdata(scn64);
	size_t count	= s32->sh_size/s32->sh_entsize;
	Elf32_Nhdr* n32	= (Elf32_Nhdr*) d32->d_buf;
	Elf64_Nhdr* n64	= (Elf64_Nhdr*)calloc(count, s64->sh_entsize);


	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}
	if (n64 == NULL) {
		perror("elfbloat");
		punt("error converting note section");
	}


	/* fill out the Elf64_Nhdr array */
	for (ii = 0; ii < count; ++ii) {
		n64[ii].n_namesz	= n32[ii].n_namesz;
		n64[ii].n_descsz	= n32[ii].n_descsz;
		n64[ii].n_type		= n32[ii].n_type;
	}

	/* fill out the Elf_Data structure */
	d64->d_type	= d32->d_type;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= n64;
	d64->d_size	= count * s64->sh_entsize;
}


static void
bloat_symtab(Elf_Scn* scn32, Elf32_Shdr* s32, Elf_Scn* scn64, Elf64_Shdr* s64)
{
	size_t ii;
	Elf_Data* d32		= elf_getdata(scn32, NULL);
	Elf_Data* d64		= elf_newdata(scn64);
	size_t count		= s32->sh_size/s32->sh_entsize;
	Elf32_Sym* sym32	= (Elf32_Sym*) d32->d_buf;
	Elf64_Sym* sym64	= (Elf64_Sym*)calloc(count, s64->sh_entsize);


	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}
	if (sym64 == NULL) {
		perror("elfbloat");
		punt("error converting symbol table section");
	}


	/* fill out the Elf64_Sym array */
	for (ii = 0; ii < count; ++ii) {
		sym64[ii].st_name	= sym32[ii].st_name;
		sym64[ii].st_value	= sym32[ii].st_value;
		sym64[ii].st_size	= sym32[ii].st_size;
		sym64[ii].st_info	= sym32[ii].st_info;
		sym64[ii].st_other	= sym32[ii].st_other;
		sym64[ii].st_shndx	= sym32[ii].st_shndx;
	}

	/* fill out the Elf_Data structure */
	d64->d_type	= d32->d_type;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= sym64;
	d64->d_size	= count * s64->sh_entsize;
}


static void
bloat_generic(Elf_Scn* scn32, Elf_Scn* scn64)
{
	Elf_Data* d32 = elf_getdata(scn32, NULL);
	Elf_Data* d64 = elf_newdata(scn64);

	if ((d32 == NULL) || (d64 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}

	d64->d_type	= d32->d_type;
	d64->d_size	= d32->d_size;
	d64->d_align	= d32->d_align;
	d64->d_version	= d32->d_version;
	d64->d_buf	= d32->d_buf;
}


static void
bloat_shdr(Elf_Scn* scn32, Elf_Scn* scn64)
{
	Elf32_Shdr* s32;
	Elf64_Shdr* s64;
	Elf32_Word  type;

	s32 = elf32_getshdr(scn32);
	if (s32 == NULL) {
		punt(elf_errmsg(elf_errno()));
	}
	s64 = elf64_getshdr(scn64);
	if (s64 == NULL) {
		punt(elf_errmsg(elf_errno()));
	}

	s64->sh_name	= s32->sh_name;
	s64->sh_type	= s32->sh_type;
	s64->sh_flags	= s32->sh_flags;
	s64->sh_addr	= s32->sh_addr;
	s64->sh_link	= s32->sh_link;
	s64->sh_info	= s32->sh_info;
	s64->sh_entsize	= s32->sh_entsize; /* fixup below */

	type = s32->sh_type;
	switch (type) {
	case SHT_DYNAMIC:
		s64->sh_entsize = elf64_fsize(ELF_T_DYN, 1, EV_CURRENT);
		bloat_dynamic(scn32, s32, scn64, s64);
		break;

	case SHT_NOTE:
		s64->sh_entsize = sizeof (Elf64_Nhdr);
		bloat_note(scn32, s32, scn64, s64);
		break;

	case SHT_RELA:
		s64->sh_entsize = elf64_fsize(ELF_T_RELA, 1, EV_CURRENT);
		bloat_rela(scn32, s32, scn64, s64);
		break;

	case SHT_REL:
		s64->sh_entsize = elf64_fsize(ELF_T_REL, 1, EV_CURRENT);
		bloat_rela(scn32, s32, scn64, s64);
		break;

	case SHT_DYNSYM:
	case SHT_SYMTAB:
		s64->sh_entsize = elf64_fsize(ELF_T_SYM, 1, EV_CURRENT);
		bloat_symtab(scn32, s32, scn64, s64);
		break;

	case SHT_SHLIB:
	case SHT_NOBITS:
	case SHT_HASH:
	case SHT_STRTAB:
	case SHT_NULL:
	case SHT_PROGBITS:
		bloat_generic(scn32, scn64);
		break;

	case SHT_SUNW_verdef:
	case SHT_SUNW_versym:
	case SHT_SUNW_verneed:
		/* XX64:  ASSumes Elf32_Verdef == Elf64_Verdef */
		/* ... */
		bloat_generic(scn32, scn64);
		break;

	default:
		if (((type >= SHT_LOSUNW) && (type <= SHT_HISUNW)) ||
			((type >= SHT_LOPROC) && (type <= SHT_HIPROC)) ||
			((type >= SHT_LOUSER) && (type <= SHT_HIUSER))) {
			/*
			 * Here we have a valid section of some anonymous
			 * type.  About all we can do here is copy it over
			 * and hope all is well.
			 */
			bloat_generic(scn32, scn64);
			break;
		}

		punt("Bad shdr type");
	}
}



static void
bloat_elf(Elf* ein, Elf* eout)
{
	Elf32_Ehdr* e32 = elf32_getehdr(ein);
	Elf64_Ehdr* e64 = elf64_newehdr(eout);
	Elf32_Phdr* p32;
	Elf64_Phdr* p64;
	Elf_Scn* scn32 = NULL;
	Elf_Scn* scn64 = NULL;
	int ii;

	if ((e64 == NULL) || (e32 == NULL)) {
		punt(elf_errmsg(elf_errno()));
	}

	/* process elf header */
	bloat_ehdr(e32, e64);

	/* process program headers */
	if (e32->e_phnum > 0) {
		p32 = elf32_getphdr(ein);
		if (p32 == NULL) {
			punt(elf_errmsg(elf_errno()));
		}
		p64 = elf64_newphdr(eout, e32->e_phnum);
		if (p64 == NULL) {
			punt(elf_errmsg(elf_errno()));
		}

		for (ii = 0; ii < e32->e_phnum; ++ii) {
			bloat_phdr(&p32[ii], &p64[ii]);
		}
	}


	/* process sections */
	for (ii = 1; ii < e32->e_shnum; ++ii) {
		/* start at 1, libelf owns first shdr */

		scn32 = elf_nextscn(ein, scn32);
		if (scn32 == NULL) {
		    punt(elf_errmsg(elf_errno()));
		}

		scn64 = elf_newscn(eout);
		if (scn64 == NULL) {
			punt(elf_errmsg(elf_errno()));
		}

		if (g_verbose == B_TRUE) {
			fprintf(stderr, "converting <%s> section...",
				elf_strptr(ein, e32->e_shstrndx,
				elf32_getshdr(scn32)->sh_name));
		}

		bloat_shdr(scn32, scn64);

		if (g_verbose == B_TRUE) {
			fprintf(stderr, "done.\n");
		}
	}
}


void
main(int argc, char ** argv)
{
	int infd = STDIN_FILENO;
	int outfd = STDOUT_FILENO;
	long ii;
	Elf * inelf;
	Elf * outelf;
	char * idents;
	Elf_Kind kind;


	for (ii = 1; ii < argc; ++ii) {
		if (strcmp(argv[ii], "-i") == 0) {
			infd = open(argv[++ii], O_RDONLY);
			if (infd == -1) {
				perror("input file");
				punt("Unable to open input file");
			}
		} else if (strcmp(argv[ii], "-o") == 0) {
			g_outfile = argv[++ii];
			outfd = open(g_outfile, O_RDWR|O_CREAT|O_EXCL, 0755);
			if (outfd == -1) {
				g_outfile = NULL;
				perror("Unable to open output file");
				punt("Unable to open output file");
			}
		} else if (strcmp(argv[ii], "-v") == 0) {
			g_verbose = B_TRUE;
		}
	} /* for -- args */


	if (elf_version(EV_CURRENT) == EV_NONE) {
		punt(elf_errmsg(elf_errno()));
	}


	inelf = elf_begin(infd, ELF_C_READ, NULL);
	if (inelf == NULL) {
		punt(elf_errmsg(elf_errno()));
	}

	outelf = elf_begin(outfd, ELF_C_WRITE, NULL);
	if (outelf == NULL) {
		punt(elf_errmsg(elf_errno()));
	}


	/* Get header information on input file */
	kind = elf_kind(inelf);
	idents = elf_getident(inelf, (size_t *)NULL);
	if (idents == NULL) {
		punt(elf_errmsg(elf_errno()));
	}


	/* do the conversion of Elf32 -> Elf64 */
	switch (kind) {
	case ELF_K_ELF:
		if (idents[EI_CLASS] != ELFCLASS32) {
			punt("Input file not ELFCLASS32");
		}
		bloat_elf(inelf, outelf);
		break;

	case ELF_K_AR:
		punt("Archives not supported in this version of the\n"
			"Elf converter.  Please extract the objects and\n"
			"convert them individually.\n");
		break;

	case ELF_K_COFF:
	default:
		punt("File is neither Elf or archive format!\n");
	}


	/* write out the new Elf */
	if (g_verbose == B_TRUE) {
		fprintf(stderr, "Writing out Elf64 file...");
	}

	ii = elf_update(outelf, ELF_C_WRITE);
	if (ii == -1) {
		punt(elf_errmsg(elf_errno()));
	}

	if (g_verbose == B_TRUE) {
		fprintf(stderr, "Done.\n");
	}


	/* cleanup */
	elf_end(outelf);
	elf_end(inelf);
	close(infd);
	close(outfd);
}
