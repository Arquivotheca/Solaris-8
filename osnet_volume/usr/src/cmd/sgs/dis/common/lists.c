/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)lists.c	1.8	98/09/30 SMI"

/*
 * miscellaneous routines for management of lists of sections,
 * and the functions contained therein.
 */

#include	"dis.h"
#include	"extn.h"

#define	TOOL	"dis"

static void add_func(FUNCLIST *func, GElf_Half sect);


/*
 *	build_sections()
 *
 *	create the list of sections to be disassembled
 */

void
build_sections(void)
{
	SCNLIST	*	sclisttail;
	SCNLIST	*	sectp;
	Elf_Scn	*	scn = 0;
	unsigned int	sect = 1, _symtab = 0, _dynsym = 0;
	int		i;

	sclisttail = sclist = NULL;

	/*
	 * Read all the section headers in the file.  Special sections, such as
	 * the symbol table, debugging and line info are remembered for later
	 * use.
	 * If the section is one of the named sections, add it to the list.  If
	 * there were no named sections, and the section is a text section, add
	 * it to the list.
	 */
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		GElf_Shdr	shdr;
		char *		name;

		if (gelf_getshdr(scn, &shdr) == 0)
			fatal("No section header table");

		name = elf_strptr(elf, ehdr->e_shstrndx, (size_t)shdr.sh_name);

		/*
		 * Do we need to remember this section?
		 */
		if ((nsecs > 0) && !Fflag) {
			for (i = 0; i <= nsecs; i++) {
				if (strcmp(namedsec[i], name) != 0)
					continue;

				if (trace > 0)
					(void) printf("\nsection name is "
					    "{%s}\n", namedsec[i]);

				if (shdr.sh_type == SHT_NOBITS) {
					if (archive)
						(void) fprintf(stderr,
							"%s: %s[%s]: %.8s: Can "
							"not disassemble a "
							"NOBITS section\n",
							TOOL, fname,
							mem_header->ar_name,
							namedsec[i]);
					else
						(void) fprintf(stderr,
							"%s: %s: %.8s: Can "
							"not disassemble a "
							"NOBITS section\n",
							TOOL, fname,
							namedsec[i]);
					continue;
				}

				if ((sectp = calloc(1,
				    sizeof (SCNLIST))) == NULL)
					fatal("Memory allocation failure");
				if ((sectp->shdr =
				    malloc(sizeof (GElf_Shdr))) == NULL)
					fatal("Memory allocation failure");

				(void) memcpy(sectp->shdr, &shdr,
				    sizeof (GElf_Shdr));

				sectp->scnam = namedsec[i];
				sectp->scnum = sect;
				sectp->stype = namedtype[i];

				if (sclisttail)
					sclisttail->snext = sectp;
				sclisttail = sectp;
				if (sclist == NULL)
					sclist = sectp;
			}
		} else {
			/*
			 * If no specific sections have been requested remember
			 * all text sections.
			 */
			if ((shdr.sh_type == SHT_PROGBITS) &&
			    (shdr.sh_flags == (SHF_ALLOC | SHF_EXECINSTR))) {

				if ((sectp = calloc(1,
				    sizeof (SCNLIST))) == NULL)
					fatal("memory allocation failure");
				if ((sectp->shdr =
				    malloc(sizeof (GElf_Shdr))) == NULL)
					fatal("Memory allocation failure");

				(void) memcpy(sectp->shdr, &shdr,
				    sizeof (GElf_Shdr));

				sectp->scnam = name;
				sectp->scnum = sect;
				sectp->stype = TEXT;

				if (sclisttail)
					sclisttail->snext = sectp;
				sclisttail = sectp;
				if (sclist == NULL)
					sclist = sectp;
			}
		}

		/*
		 * Determine if we've found any special sections.
		 */
		if (shdr.sh_type == SHT_DYNSYM)
			_dynsym = sect;
		else if (shdr.sh_type == SHT_SYMTAB)
			_symtab = sect;
		else if (strcmp(name, ".debug") == 0)
			debug = sect;
		else if (strcmp(name, ".line") == 0)
			line = sect;
		sect++;
	}

	/*
	 * Pick the best symbol table.
	 */
	if (_symtab)
		symtab = _symtab;
	else if (_dynsym)
		symtab = _dynsym;

	/*
	 * If we were asked to locate named sections make sure we've found
	 * them all.
	 */
	if ((nsecs == 0) || Fflag)
		return;

	for (i = 0; i <= nsecs; i++) {
		for (sectp = sclist; sectp; sectp = sectp->snext) {
			if (namedsec[i] == sectp->scnam)
				break;
		}
		if (sectp)
			continue;

		if (archive)
			(void) fprintf(stderr, "%s: %s[%s]: %s: cannot find "
			    "section header\n",
			    TOOL, fname, mem_header->ar_name, namedsec[i]);
		else
			(void) fprintf(stderr, "%s: %s: %s: cannot find "
			    "section header\n",
			    TOOL, fname, namedsec[i]);
	}
}


/* free the space used by the list of section headers */

void
section_free(void)
{
	SCNLIST	*sectp;
	SCNLIST	*stemp;
	FUNCLIST	*funcp;
	FUNCLIST	*ftemp;

	if (sclist == NULL)
		return;

	sectp = sclist;
	while (sectp) {
		stemp = sectp;
		funcp = sectp->funcs;
		sectp = sectp->snext;
		(void) free(stemp->shdr);
		(void) free(stemp);

		while (funcp) {
			ftemp = funcp;
			funcp = funcp->nextfunc;
			(void) free(ftemp->funcnm);
			(void) free(ftemp);
		}
	}
}



/*  Make a list of all the functions contained in the sections */

void
build_funcs(void)
{

	SCNLIST		*sectp;
	FUNCLIST	*func;
	char		*func_name;
	Elf_Scn		*scn;
	GElf_Shdr	g_shdr, *shdr = &g_shdr;
	Elf_Data	*sym_data;
	int		no_of_symbols, counter;
	GElf_Sym 	sym, *p = &sym;

	if ((scn = elf_getscn(elf, symtab)) == NULL)
		fatal("failed to get the symbol table section");
	if (gelf_getshdr(scn, shdr) == NULL)
		fatal("failed to get the section header");
	if (shdr->sh_entsize == 0)
		fatal("the symbol table entry size is 0!");

	sym_data = 0;
	sym_data = elf_getdata(scn, sym_data);
	no_of_symbols = sym_data->d_size / gelf_fsize(elf, ELF_T_SYM,
		1, EV_CURRENT);

	for (counter = 1; counter < (no_of_symbols); counter++) {

		(void) gelf_getsym(sym_data, counter, &sym);
		for (sectp = sclist; sectp; sectp = sectp->snext)
			if (sectp->scnum == p->st_shndx)
				break;

		if (GELF_ST_TYPE(p->st_info) == STT_FUNC) {
			if ((func = calloc(1, sizeof (FUNCLIST))) == NULL)
				fatal("memory allocation failure");

			func_name = (char *)elf_strptr(elf, shdr->sh_link,
			    (size_t)p->st_name);

			if (Cflag)
				func_name = demangled_name(func_name);

			if ((func->funcnm = calloc(1,
			    (unsigned)(strlen(func_name)+1))) == NULL)
				fatal("memory allocation failure");

			(void) strcpy(func->funcnm, func_name);
			func->faddr = p->st_value;
			func->fcnindex = counter;
			add_func(func, p->st_shndx);
		}
	}
}

#ifdef ELFDEBUG
static void
dump_sects(void)
{
	SCNLIST *p;

	for (p = sclist; p; p = p->snext) {
		fprintf(stderr, "shdr = %p, name = %s, next = %p, "
		    "scnum = %d, type = %d, funcs = %p\n",
		    p->shdr, p->scnam, p->snext, p->scnum,
		    p->stype, p->funcs);
	}
}

static void
dump_funcs(void)
{
	SCNLIST *p;
	FUNCLIST *f;

	for (p = sclist; p; p = p->snext) {
		fprintf(stderr, "section: %s\n", p->scnam);
		for (f = p->funcs; f; f = f->nextfunc) {
			fprintf(stderr, "\tfunct: %s\taddr: %lld\n",
			    f->funcnm, f->faddr);
		}
	}
}
#endif	/* ELFDEBUG */

/*
 *	add_func()
 *
 *	add func to the list of functions associated with the section
 *	given by sect
 */

static void
add_func(FUNCLIST *func, GElf_Half sect)
{
	static short	last_sect = 0;
	static FUNCLIST	*last_func = NULL;
	static char	*last_file = NULL;

	SCNLIST		*sectp;
	FUNCLIST	*funcp;
	FUNCLIST	*backp;
	static int	elist = 1;

	/*
	 * if this function follows the last function added to the list,
	 * the addition can be done quickly
	 */
	if (elist && (last_sect == sect) && last_func &&
	    (last_func->faddr < func->faddr) && last_file &&
	    (strcmp(fname, last_file) == 0)) {
		funcp = last_func->nextfunc;
		func->nextfunc = funcp;
		funcp = func;
		last_func = func;
		elist = 1;
	} else {	/* find the corresponding section pointer */
		for (sectp = sclist; sectp; sectp = sectp->snext) {
			if (sectp->scnum == sect)
				break;
		}

		if (sectp) {
			/* keep the list of functions ordered by address */
			if ((sectp->funcs == NULL) ||
			    (sectp->funcs->faddr > func->faddr)) {
				func->nextfunc = sectp->funcs;
				sectp->funcs = func;
				if (sectp->funcs == NULL) elist = 1;
				else elist = 0;
			} else {
				backp = sectp->funcs;
				funcp = sectp->funcs->nextfunc;
				for (; funcp; funcp = funcp->nextfunc) {
					if (func->faddr <= funcp->faddr) {
						func->nextfunc = funcp;
						backp->nextfunc = func;
						break;
					}
					backp = funcp;
				}
				if (funcp == NULL)
					backp->nextfunc = func;
				elist = 0;
			}

			last_func = func;
			last_sect = sect;
			last_file = fname;
		}

		else
			(void) free(func);
	}
}
