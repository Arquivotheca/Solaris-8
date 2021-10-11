/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)main.c	1.17	98/02/23 SMI"

/*
 * main routine for standalone disassembler.
 * includes the command line option parser, the disassembly routine
 * that disassembles by function or section, (text or data).
 */

#include	"dis.h"
#include	<sys/elf_SPARC.h>
#include	"extn.h"

#include	<dlfcn.h>
#define	TOOL	"dis"
#define	LIB	"/lib"
#define	LIBDIR	"LIBDIR"
#define	LIBDEFAULT 	"/usr/ccs/lib"
#define	OPTSTR	"oCVLsST?d:D:F:t:l:"
#define	OPTIONS	"-oCVLs? -d name -Dname -F name -t name -l name"

static	int	save_sflag; /* used to save original value of sflag */
			    /* When a file has no symbol table, sflag */
			    /* is set to 0 for only that file; must be reset */
struct l_list {
	char	*data;
	struct l_list *next;
};

typedef struct l_list elt;
typedef elt *elink;

int	sparcver;		/* output V7 or V9 disassembly */

static void addl_file(char *file);
static void command_line(int argc, char *argv[]);
static void each_file(void);
static void disassembly(void);
static void search_table(void);
static void dis_funcs(void);

/*
 * LINKED LIST OF FILE NAMES TO BE DISASSEMBLED - HEAD AND TAIL POINTERS
 */

static elink 	fhead = NULL, ftail;

/*
 *	main (argc, argv)
 *
 *	This routine calls the function command_line() to process the
 *	command line arguments - options and file names. The file names

 *	(those given via the -l option or via the command line)
 *	are placed in a linked list when the function addl_file() is called.
 *
 */

main(int argc, char *argv[])
{
	static int	header = 0;
	if (argc == 1) {
		(void) fprintf(stderr, "%s: Usage: %s [%s] file(s) \n",
		    TOOL, TOOL, OPTIONS);
		exit(1);
	}

	(void) elf_version(EV_NONE);
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, "dis: libelf.a out of date.\n");
		exit(1);
	}

	command_line(argc, argv);

	ftail = fhead;	 /* Traverse linked list of file names */
	while (ftail != NULL) {
		if (!header) {		/* print header only once */
			(void) printf("\t\t****   DISASSEMBLER  ****\n\n");
			header++;
		}

		each_file();
		ftail = ftail->next;	/* Next file in linked list */
	}

	exit(0);
	/* NOTREACHED */
}

/*
 * addl_file()
 *
 * ADD A FILE NAME INTO THE LINKED LIST OF FILES TO BE DISASSEMBLED.
 *
 */

static void
addl_file(char *file)
{
	if (fhead == NULL) {
		fhead = (elink) malloc(sizeof (elt));
		ftail = fhead;
	} else {
		ftail->next = (elink) malloc(sizeof (elt));
		ftail = ftail->next;
	}
	ftail->data = file;
	ftail->next = NULL;
}


/*
 * command_line (argc, argv)
 *
 * This routine processes the command line received by the disassembler.
 */

static void
command_line(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;

	char *optstr = OPTSTR;
	int optchar, str_1, str_2, str_len, Vflag = 0, errflag = 0;
	char *libs;
	static int lflag = 0;
	static char	*libstr;

	if ((ffunction = (NFUNC *) calloc(argc, sizeof (NFUNC))) == NULL) {
		(void) fprintf(stderr,
		    "memory allocation failure on call to calloc");
		exit(1);
	}

	if ((namedsec = (char **) calloc(argc, sizeof (char *))) == NULL) {
		(void) fprintf(stderr,
		    "memory allocation failure on call to calloc");
		exit(1);
	}

	if ((namedtype = (int *) calloc(argc, sizeof (int))) == NULL) {
		(void) fprintf(stderr,
		    "memory allocation failure on call to calloc");
		exit(1);
	}

	while ((optchar = getopt(argc, argv, optstr)) != -1) {
		switch (optchar) {
		case 'T':
			trace++;
			break;

		case 'o':
			oflag++;
			break;
#if defined(i386)
		case 'R':
			Rflag++;
			break;
#endif	/* i386 */
		case 'C':
			Cflag = 1;
			break;

		case 's':
			sflag++;
			save_sflag++;
			break;

		case 'S':
			Sflag++;
			break;

		case 'L':
			Lflag++;
			break;

		case 'l':
			lflag++;
			if (((libstr = getenv(LIBDIR)) == NULL) ||
			    (*libstr == '\0'))
				libstr = LIBDEFAULT;

			str_1 = strlen(libstr) + strlen(LIB);
			str_2 = strlen(optarg);
			/* add 2 for '.a'	*/
			str_len = str_1 + str_2 + 2;
			if ((libs = (char *) malloc(str_len + 1 *
			    sizeof (char))) == NULL) {
				(void) fprintf(stderr,
				"memory allocation failure on call to malloc");
				exit(1);
			}

			(void) sprintf(libs, "%s%s%s.a", libstr, LIB, optarg);

			if (trace > 0)
				(void) printf("\nlib is {%s}\n", libs);

			addl_file(libs);
			break;

		case 'd':	/* disassemble as a data section flag */
			nsecs++;
			namedsec[nsecs] = optarg;
			namedtype[nsecs] = DATA;
			break;

		case 'D': /* 	print addresses of data rather than offsets */
			aflag++;
			nsecs++;
			namedsec[nsecs] = optarg;
			namedtype[nsecs] = DATA;
			break;

		case 't':
			tflag++;
			nsecs++;
			namedsec[nsecs] = optarg;
			namedtype[nsecs] = TEXT;
			break;

		case 'V':	/* version flag */
			Vflag++;
			(void) fprintf(stderr, "dis: %s %s\n",
			    (const char *)SGU_PKG, (const char *)SGU_REL);
			break;

		case 'F':
			fflag++;
			ffunction[fflag - 1].funcnm = optarg;
			break;

		case '?':
			errflag++;
			break;

		default:
			break;
		}	/* end of switch */

	}	/* end of while */

	if (errflag || (optind >= argc)) {
		if (!(Vflag && (argc == 2)) && !lflag) {
			(void) fprintf(stderr, "dis: Usage: dis [%s] file\n",
			    OPTIONS);
			exit(1);
		}
	}

	if (errflag && lflag) {
		(void) fprintf(stderr, "dis: Usage: dis [%s] file\n", OPTIONS);
		exit(1);
	}

	for (; optind < argc; optind++) {
		addl_file(argv[optind]);
	}
}


static void
each_file(void)
{
	int 	fd;

	archive = 0;
	symtab  = 0;
	debug   = 0;
	line	= 0;
	Rel_data = 0;
	Rela_data = 0;
	Rel_sec = FAILURE;

	fname = ftail->data;  /* Name of file or archive to be disassembled */
	(void) printf("\ndisassembly for %s\n", fname);

	if ((fd = open(fname, O_RDONLY)) == -1) {
		(void) fprintf(stderr, "dis: cannot open file %s\n", fname);
		return;
	}

	cmd = ELF_C_READ;
	arf = elf_begin(fd, cmd, (Elf *)0);

	if (elf_kind(arf) == ELF_K_AR)
		archive = 1;

	while ((elf = elf_begin(fd, cmd, arf)) != 0) {

		if (archive) {
			if ((mem_header = elf_getarhdr(elf)) == NULL) {
				(void) fprintf(stderr,
				    "dis: %s: malformed archive (at %ld)\n",
				    fname, elf_getbase(elf));
				return; /* next file on command line */
			}
		}

		if (gelf_getehdr(elf, ehdr) != 0) {
			if (archive == 1)
				(void) printf("\narchive member\t\t%s\n",
				    mem_header->ar_name);
			switch (ehdr->e_machine) {
			case EM_SPARC:
				/* SPARC ABI suppl requires these fields: */
				if (ehdr->e_ident[EI_CLASS] != ELFCLASS32 ||
					ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
					printf("warning: erroneous SPARC ELF"
						"ident field.\n");
				}
				sparcver = V8_MODE;
				break;
			case EM_SPARC32PLUS:
				/* SPARC ABI suppl requires these fields: */
				if (ehdr->e_ident[EI_CLASS] != ELFCLASS32 ||
					ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
					printf("warning: erroneous SPARC ELF"
						"ident field.\n");
				}

				switch (ehdr->e_flags & EF_SPARC_32PLUS_MASK) {
				default:
				case EF_SPARC_32PLUS:
					/* use SPARC V9 disassembly mode */
					sparcver = V9_MODE;
					break;
				case (EF_SPARC_32PLUS|EF_SPARC_SUN_US1):
					/* use SPARC V9/UltraSPARC-1 mode */
					sparcver = V9_MODE | V9_SGI_MODE;
					break;
				}
				break;
			case EM_SPARCV9:
				/* SPARC ABI suppl requires these fields: */
				if (ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
					ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
					printf("warning: erroneous SPARC ELF"
						"ident field.\n");
				}

				/* use SPARC V9/UltraSPARC-1 mode */
				sparcver = V9_MODE | V9_SGI_MODE;
				/* sparcver = V9_MODE; */
				break;
			default:
				sparcver = 0;
				break;
			}
			elfclass = gelf_getclass(elf);
			disassembly();
		} else {
			if (archive) {
				if (strcmp(mem_header->ar_name, "/") != 0 &&
				    strcmp(mem_header->ar_name, "//") != 0)
					(void) fprintf(stderr,
					    "dis: %s[%s]: invalid file type\n",
					    fname, mem_header->ar_name);
			} else
				(void) fprintf(stderr,
				    "dis: %s: invalid file type\n", fname);
		}

		cmd = elf_next(elf);
		(void) elf_end(elf);
	}
	(void) elf_end(arf);
	(void) close(fd);
}

/*
 *	disassembly ()
 *
 *	For each file that is disassembled, disassembly opens the
 *	necessary file pointers, builds the list of section
 *	headers, and if necessary, lists of functions and labels.
 * 	It then calls text_sections or dis_data to disassemble
 *	the sections.
 */

static void
disassembly(void)
{
	Elf_Scn		*scn;
	Elf_Data	*data;

	SCNLIST		*sectp;

	sflag = save_sflag;

	if (fflag)
		Fflag = 1;

	build_sections(); /* make a linked list of sections */

	if (symtab == 0 && sflag) {
		sflag = 0;
		if (archive)
			(void) fprintf(stderr, "\ndis: %s[%s]:",
			    fname, mem_header->ar_name);
		else
			(void) fprintf(stderr, "\ndis: %s:", fname);

		(void) fprintf(stderr,
	"No symbol table in file: symbolic disassembly cannot be performed.\n");
	}

	if (symtab != 0)
		build_funcs(); /* make linked list of functions for sections */

#ifdef DEBUG_INFO
	if ((sflag || Lflag) && debug != 0)
		get_debug_info();

	if (line)
		get_line_info();
#endif

	if (fflag) {
		if (symtab)
			search_table();
		dis_funcs();
		Fflag = 0;
		return;
	}

	for (sectp = sclist; sectp; sectp = sectp->snext) {

#if defined(DEBUG)
	printf("%s %x: %s %s: sectp_scnum is %d: sectp->stype is %d\n",
	    "DEBUG: sectp->shdr->sh_addr is", sectp->shdr->sh_addr,
	    "sectp->scnam is", sectp->scnam, sectp->scnum, sectp->stype);
#endif	/* DEBUG */

		if (sectp->stype == TEXT) {
			sname = sectp->scnam;
			(void) printf("\nsection\t%s\n", sname);
			next_function = sectp->funcs;
			scnhdr = sectp->shdr;

			scn = elf_getscn(elf, sectp->scnum);
			data = 0;
			if ((data = elf_getdata(scn, data)) == 0 ||
			    data->d_size == 0) {
				(void) fprintf(stderr,
				    "dis: no data in section %s\n", sname);
				continue;
			}
			p_data = (unsigned char *)data->d_buf;

			/*
			 * If symbolic disassembly is requested, see if
			 * there's a relocation section associated with the
			 * current section.
			 */
			if (sflag) {
				Rel_sec = get_rel_section(sectp->scnum);
			}
			dis_text(sectp->shdr);

		} else {
			sname = sectp->scnam;
			scn = elf_getscn(elf, sectp->scnum);
			data = 0;
			if ((data = elf_getdata(scn, data)) == 0 ||
			    data->d_size == 0) {
				(void) fprintf(stderr,
				    "dis: no data in section %s\n", sname);
				continue;
			}
			p_data = (unsigned char *)data->d_buf;
			dis_data(sectp->shdr);
		}
	}

	section_free();
	label_free();
}


/*
 * search the symbol table for the named functions and fill in the
 * information in the ffunction array
 */
static void
search_table(void)
{
	int		j;
	char		*name;
	FUNCLIST	*funcp, *fp;
	SCNLIST		*sectp;

	Elf_Scn		*scn;
	GElf_Shdr	g_shdr, *shdr = &g_shdr;
	Elf_Data	*sym_data;
	GElf_Sym	sym, *p = &sym;
	int		no_of_symbols, counter;

	for (j = 0; j < fflag; j++)
		ffunction[j].found = 0;

	if ((scn = elf_getscn(elf, symtab)) == 0)
		fatal("failed to get the symbol table section");
	if (gelf_getshdr(scn, shdr) == 0)
		fatal("failed to get the section header");
	if (shdr->sh_entsize == 0)
		fatal("the symbol table entry size is 0!");

	sym_data = 0;
	if ((sym_data = elf_getdata(scn, sym_data)) == NULL)
		fatal("no data in symbol table section");

	no_of_symbols = sym_data->d_size / gelf_fsize(elf, ELF_T_SYM,
		1, EV_CURRENT);

	for (counter = 1; counter < (no_of_symbols); counter++) {
		(void) gelf_getsym(sym_data, counter, &sym);
		name = (char *)elf_strptr(elf, shdr->sh_link,
		    (size_t)p->st_name);
		if (Cflag)
		    name = demangled_name(name);
		for (j = 0; j < fflag; j++) {
			if (strcmp(name, ffunction[j].funcnm) == 0)
				break;
		}

		if (j != fflag) {
			for (sectp = sclist; sectp; sectp = sectp->snext)
				if (sectp->scnum == p->st_shndx)
					break;
			if (sectp == NULL)
				continue;

			if (GELF_ST_TYPE(p->st_info) == STT_FUNC) {
				ffunction[j].faddr = p->st_value;
				ffunction[j].fcnindex = counter;
				ffunction[j].found = 1;
				ffunction[j].fscnum = p->st_shndx;
				for (funcp = sectp->funcs; funcp;
				    funcp = funcp->nextfunc) {
					if (strcmp((char *)name,
					    funcp->funcnm) == 0)
						break;
				}

				if (funcp->nextfunc != NULL) {
					fp = funcp->nextfunc;

					while (fp != NULL &&
					    fp->faddr <= funcp->faddr)
						fp = fp->nextfunc;

					if (fp != NULL)
						ffunction[j].fsize =
						    fp->faddr - funcp->faddr;
					else
						ffunction[j].fsize =
						    (sectp->shdr->sh_addr +
						    sectp->shdr->sh_size) -
						    funcp->faddr;
				} else
					ffunction[j].fsize =
					    (sectp->shdr->sh_addr +
					    sectp->shdr->sh_size) -
					    funcp->faddr;
			}
		}
	}
}

/*
 * disassemble the functions in the ffunction array
 */

static void
dis_funcs(void)
{
	Elf_Scn		*scn;
	GElf_Shdr	g_shdr, *shdr = &g_shdr;
	GElf_Shdr	elfshdr, *elf_shdr = &elfshdr;
	Elf_Data	*data;
	int		i;
	FUNCLIST	func;

	for (i = 0; i < fflag; i++) {
		if (!ffunction[i].found) {
			if (archive)
				(void) fprintf(stderr,
				    "%s: %s[%s]: function %s not found\n",
				    TOOL, fname, mem_header->ar_name,
				    ffunction[i].funcnm);
			else
				(void) fprintf(stderr,
				    "%s: %s: function %s not found\n",
				    TOOL, fname, ffunction[i].funcnm);
			continue;
		}

		/* Get the section header, and section data */
		scn = elf_getscn(elf, ffunction[i].fscnum);
		(void) gelf_getshdr(scn, shdr);
		/* Let's pretend that the section contains only	*/
		/* the data for the current function. Thus must	*/
		/* save the original values 			*/

		sname = elf_strptr(elf, ehdr->e_shstrndx,
		    (size_t)shdr->sh_name);
		(void) printf("\nsection\t%s\n", sname);

		data = 0;
		if ((data = elf_getdata(scn, data)) == 0 || data->d_size == 0) {
			(void) fprintf(stderr,
			    "dis: no data in section %s\n", sname);
			return;
		}

		/*
		 * seek to the start of the function, and change the section
		 * header to fake out dis_text(); make it think the section
		 * has only one function.
		 */
		p_data = (unsigned char *)data->d_buf;

		p_data += (ffunction[i].faddr - shdr->sh_addr);
		elf_shdr->sh_addr = ffunction[i].faddr;
		elf_shdr->sh_size = ffunction[i].fsize;

		func.funcnm = ffunction[i].funcnm;
		func.faddr = ffunction[i].faddr;
		func.fcnindex = ffunction[i].fcnindex;
		func.nextfunc = NULL;
		next_function = &func;

		if (sflag)
			(void) get_rel_section(ffunction[i].fscnum);

		dis_text(elf_shdr);
	}
}
