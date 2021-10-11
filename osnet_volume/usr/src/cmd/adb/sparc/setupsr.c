/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setupsr.c	1.72	99/03/23 SMI"

/*
 * adb - routines to read a.out+core at startup
 */

#include <stdio.h>
#include "adb.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/elf_SPARC.h>
#include <procfs.h>
#include <sys/cpuvar.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/auxv.h>
#include <string.h>
#ifndef KADB
#include <libgen.h>
#endif
#include "symtab.h"


off_t	datbas;			/* offset of the base of the data segment */
off_t	stksiz;			/* stack size in the core image */
off_t   textaddr();		/* address of the text segment */

extern int v9flag;
extern int dismode;
int getpcb(label_t *);

#ifndef KADB
char *get_prog_name(char *);
#endif

#ifndef KADB
#include <kvm.h>

char	*symfil	= "a.out";
char	*corfil	= "core";

extern kvm_t *kvmd;		/* see main.c */
struct asym *trampsym;
extern int use_shlib;   /* non-zero __DYNAMIC ==> extended maps for text */
extern void ksyms_dismode(void);
#endif !KADB

int		textseg;	/* index of text segment header */
int		dataseg = -1;	/* index of data segment header */
Elf32_Shdr	*secthdr;	/* ELF section header */
psinfo_t	core_psinfo;	/* struct psinfo from core file */
char		core_platform[SYS_NMLN];
				/* platform upon which core file created */
auxv_t		*core_auxv = NULL;
				/* auxiliary vector from core file */
char *rtld_path;

void
setsym()
{
	off_t loc;
	unsigned long txtbas;
	struct map_range *fsym_rng1, *fsym_rng2;
	int i;
	int symfile_type = AOUT;

	fsym_rng1 = (struct map_range*) calloc(1,sizeof(struct map_range));
	fsym_rng2 = (struct map_range*) calloc(1,sizeof(struct map_range));
	txtmap.map_head = fsym_rng1;
	txtmap.map_tail = fsym_rng2;
	fsym_rng1->mpr_next = fsym_rng2;

#ifdef KADB
	datmap.map_head = txtmap.map_head;
	datmap.map_tail = txtmap.map_tail;
#else KADB
	fsym_rng1->mpr_fn = fsym_rng2->mpr_fn  = symfil;
	fsym = getfile(symfil, 1);
	fsym_rng1->mpr_fd = fsym_rng2->mpr_fd = fsym;
	fsym_rng1->mpr_b = fsym_rng2->mpr_b = 0;
	fsym_rng1->mpr_f = fsym_rng2->mpr_f = 0;
	if (read(fsym, (char *)&filhdr, sizeof filhdr) != sizeof filhdr ||
		filhdr.e_ident[EI_MAG0] != ELFMAG0 ||
		filhdr.e_ident[EI_MAG1] != ELFMAG1 ||
		filhdr.e_ident[EI_MAG2] != ELFMAG2 ||
		filhdr.e_ident[EI_MAG3] != ELFMAG3 ||
		!(filhdr.e_type == ET_EXEC || filhdr.e_type == ET_REL ||
		filhdr.e_type == ET_DYN) ||
		filhdr.e_version != EV_CURRENT)
	{
			fsym_rng1->mpr_e = MAXFILE;
			return;
	}

#endif KADB

	if (filhdr.e_type == ET_REL)
		symfile_type = REL;

	switch (filhdr.e_machine) {
	case EM_SPARC:
		/* SPARC ABI supplement requires these fields: */
		if (filhdr.e_ident[EI_CLASS] != ELFCLASS32 ||
			filhdr.e_ident[EI_DATA] != ELFDATA2MSB) {
			printf("warning: erroneous SPARC ELF ident field.\n");
		}
#ifndef	_KADB
		if (!strcmp(symfil, "/dev/ksyms"))
			ksyms_dismode();
		else
#endif
			dismode = V8_MODE;
		break;
	case EM_SPARC32PLUS:
		/* SPARC ABI supplement requires these fields: */
		if (filhdr.e_ident[EI_CLASS] != ELFCLASS32 ||
			filhdr.e_ident[EI_DATA] != ELFDATA2MSB) {
			printf("warning: erroneous SPARC ELF ident field.\n");
		}
		switch (filhdr.e_flags & EF_SPARC_32PLUS_MASK) {
		default:
			printf("warning: unknown SPARC ELF flags.\n");
			/* fall into... */
		case (EF_SPARC_32PLUS):
			/* use SPARC V9 disassembly mode */
			dismode = V9_MODE;
			break;
		case (EF_SPARC_32PLUS|EF_SPARC_SUN_US1):
			/* use SPARC V9/UltraSPARC-1 disassembly mode */
			dismode = V9_MODE | V9_VIS_MODE;
			break;
		}
		v9flag = 1;
		break;
	case EM_68K:
		break;		/* no spec for this yet */
	default:
		printf("warning: unknown machine type %d\n", filhdr.e_machine);
		break;
	}

	/* Don't necessarily expect a program header. */
	if (filhdr.e_phnum != 0) {
#ifndef	KADB
		/* Get space for a copy of the program header. */
		if ((proghdr = (Elf32_Phdr *) malloc(filhdr.e_phentsize *
			filhdr.e_phnum)) == NULL) {
			printf("Unable to allocate program header.\n");
			return;
		}
		/* Seek to program header table and read it. */
		if ((loc = lseek(fsym, filhdr.e_phoff, L_SET) != filhdr.e_phoff) ||
			(read(fsym, (char *)proghdr, filhdr.e_phentsize *
			filhdr.e_phnum) != filhdr.e_phentsize * filhdr.e_phnum)) {
			printf("Unable to read program header.\n");
			return;
		}
		for (textseg = 0; textseg < (int) filhdr.e_phnum; textseg++) {
			if (proghdr[textseg].p_type == PT_INTERP) {
		    		Elf32_Phdr *intp_hdr = &proghdr[textseg];
		    		rtld_path = (void*) malloc(intp_hdr->p_filesz+1);
		    		if (((loc = lseek(fsym, intp_hdr->p_offset, L_SET)) != 
			    		intp_hdr->p_offset) ||
					(read(fsym, (char *)rtld_path, intp_hdr->p_filesz)
			    		!= intp_hdr->p_filesz))
		    			{
			    			printf("Unable to read PT_INTERP path.\n");
			    			return;
												    			}
		    		use_shlib = 1;
		    		break;
													}
		}
#endif	/* !KADB */
	/* Find the text segment.  This is a problem.  The ABI doesn't show
	 * a certain way of distinguishing text and data segments.  It says
	 * only that text "usually" has "RX" access and data "usually" has
	 * "RWX" access.  I'm counting on finding text followed by data,
	 * which is the tradition.  This could need work.
	 */
		for (textseg = 0; textseg < (int) filhdr.e_phnum; textseg++) {
			if ((proghdr[textseg].p_type == PT_LOAD) &&
				(proghdr[textseg].p_flags & PF_X))
				break;
		}
		fsym_rng1->mpr_b = proghdr[textseg].p_vaddr;
		fsym_rng1->mpr_e = fsym_rng1->mpr_b + proghdr[textseg].p_filesz;
		fsym_rng1->mpr_f = proghdr[textseg].p_offset;

		dataseg = textseg + 1;	/* follows immediately, right? */
		if (dataseg > (int) filhdr.e_phnum) {
			dataseg = -1;	/* no data? */
			fsym_rng2->mpr_b = 0;
			fsym_rng2->mpr_e = MAXFILE;
			fsym_rng2->mpr_f = 0;
		} else {
			fsym_rng2->mpr_b = proghdr[dataseg].p_vaddr;
			fsym_rng2->mpr_e = fsym_rng2->mpr_b +
				proghdr[dataseg].p_filesz;
			fsym_rng2->mpr_f = proghdr[dataseg].p_offset;
		}
	}
	/* Get space for the section header. */
	if ((secthdr = (Elf32_Shdr *) malloc (filhdr.e_shentsize *
		filhdr.e_shnum)) == NULL) {
		printf("Unable to allocate section header.\n");
		return;
	}
	/* Seek to section header and read it. */
	if ((loc = lseek(fsym, filhdr.e_shoff, L_SET) == -1) ||
		(read(fsym, (char *)secthdr, filhdr.e_shentsize *
		filhdr.e_shnum) != filhdr.e_shentsize * filhdr.e_shnum)) {
		printf("Unable to read section header.\n");
		return;
	}
#if	!defined(KADB)
	/* If there wasn't a program header, we need to fix up the maps
	 * from information in the section header table.
	 */
	if (filhdr.e_phnum == 0) {

		char *sect_names = NULL;
		int sn;
		int ds = 0, d1s = 0, ts = 0;

		/* Look for the first string table.  It should be the
		 * section names.  We need the names to distinguish the
		 * interesting sections because the sh_type fields don't
		 * resolve them for this purpose.  Unfortunately, we only
		 * recognize the section names string table by finding
		 * the name ".shstrtab" in the section names string table.
		 */
		for (sn = 0; sn < (int) filhdr.e_shnum; sn++) {
			if (secthdr[sn].sh_type == SHT_STRTAB) {
				if ((sect_names = malloc(secthdr[sn].sh_size))
					== NULL) {
					printf("Unable to malloc section name table.\n");
					goto bad_shdr;
				}
				if ((loc = lseek(fsym, secthdr[sn].sh_offset,
					L_SET) == -1) ||
					(read(fsym, sect_names, secthdr[sn].
					sh_size) != secthdr[sn].sh_size)) {
					printf("Unable to read section names.\n");
					goto bad_shdr;
				}
				loc = (off_t) ((int) sect_names +
					(int) secthdr[sn].sh_name);
				if (strcmp((char *)loc, ".shstrtab") == 0)
					break;		/* found it */
			}
		}
		if (sn == filhdr.e_shnum)
			goto bad_shdr;		/* must not have found it */

		/* Look for the text and data. */

		if (symfile_type == REL) {
			int count = 0;
			/* finds the data section first */
			for (i = 0; i < (int) filhdr.e_shnum; i++) {
				/* since we have no prog headers
				 * then txtmap is currently NULL
				 */
				if (secthdr[i].sh_type == SHT_PROGBITS) {
					if(((secthdr[i].sh_flags & SHF_ALLOC) &&
					(secthdr[i].sh_flags&SHF_WRITE)) ||
					((secthdr[i].sh_flags&SHF_EXECINSTR) &&
					(secthdr[i].sh_flags&SHF_ALLOC)))  {
						count++;
						add_map_range(&txtmap,
						    secthdr[i].sh_offset,
						    secthdr[i].sh_size +
						    secthdr[i].sh_offset,
						    secthdr[i].sh_offset,
						    symfil);
					}
				}
				if (count == 1)
					txtmap.map_head = txtmap.map_tail;
			}
		} else {
			for (i = 0; i < (int) filhdr.e_shnum; i++) {
				if (secthdr[i].sh_type == SHT_PROGBITS) {
					loc = (off_t) ((int) sect_names +
						(int) secthdr[i].sh_name);
					if (strcmp((char *)loc, ".text") == 0)
						ts = i;
					else if (strcmp((char *)loc,
					    ".data") == 0)
						ds = i;
					else if (strcmp((char *)loc,
					    ".data1") == 0)
						d1s = i;
				}
			}
			if (ts != 0) {
				fsym_rng1->mpr_b = 0;
				fsym_rng1->mpr_e = secthdr[ts].sh_size;
				fsym_rng1->mpr_f = secthdr[ts].sh_offset;
			}
			if (ds != 0) {
				if (secthdr[ds].sh_size == 0 && d1s != 0)
					ds = d1s;
				fsym_rng2->mpr_b = fsym_rng1->mpr_e;
				fsym_rng2->mpr_e = fsym_rng2->mpr_b +
					secthdr[ds].sh_size;
				fsym_rng2->mpr_f = secthdr[ds].sh_offset;
			}
		}
	bad_shdr:
		if (sect_names != NULL)
			free(sect_names);
	}
#endif	/* !defined(KADB) */
	stinit(fsym, secthdr, filhdr.e_shnum, symfile_type);
#ifndef KADB
	if (!kernel) {
		(void) lookup( "sighandler");
		trampsym = cursym;
		errflg = NULL;		/* not a problem not to find it */
	}
#ifdef DEBUG
  { extern int adb_debug;
   
       if( adb_debug )
	   printf("sighandler at 0x%X\n", trampsym? trampsym->s_value : 0);
  } 
#endif
#endif !KADB
}

/*
 * address of the text segment.  Normally this is given
 * by (x).a_entry, but an exception is made for demand
 * paged (ZMAGIC == 0413) files, in which the exec structure
 * is at the beginning of text address space, and the entry
 * point immediately follows.
 */
static off_t
textaddr(fhdr)
	Elf32_Ehdr *fhdr;
{
	return (proghdr ? proghdr[textseg].p_vaddr : 0);
}

#ifndef KADB
static
ksetcor(readpanicregs)
	int readpanicregs;
{
	char *looking;
	struct cpu *pcpu;
	u_int cpu_vaddr;

	kcore = 1;
	Curproc = 0; /* don't override Sysmap mapping */
	/*
	 * Pretend we have 16 meg until we read in the
	 * value of 'physmem'
	 */
	physmem = 16 * 1024 *1024;
	(void) lookup(looking = "physmem");
	if (cursym == 0)
		goto nosym;
	if (kvm_read(kvmd, (long)cursym->s_value, (char *)&physmem,
	    sizeof physmem) != sizeof physmem)
		goto nosym;
	printf("physmem %X\n", physmem);
	datmap.map_head->mpr_e = MAXFILE;
	Curthread = NULL;
	Curproc = NULL;

	/* Look for the address of cpu[]. */ 
	(void) lookup(looking = "cpu");
	if (cursym == 0) {
		printf("Unable to find cpu symbol; no regs available.\n");
		return;
	}

	cpu_vaddr = cursym->s_value; 

	do {
		if (kvm_read(kvmd, cpu_vaddr, (char *)&pcpu,
		    sizeof pcpu) != sizeof pcpu)
			goto nosym;

		/* Look for the thread struct pointer in the cpu struct. */
		if (pcpu) {
			if (kvm_read(kvmd, (long) &pcpu->cpu_thread,
			    (char *)&Curthread, sizeof Curthread) != 
			    sizeof Curthread) {
				printf("Unable to locate current thread;");
				printf("no regs available.\n");
				return;
			}

			/* 
			 * Look for the proc struct pointer in this thread 
			 * struct.
			 */
			if (kvm_read(kvmd, (long)&Curthread->t_procp,
			    (char *)&Curproc, sizeof Curproc) != sizeof Curproc) {
				printf("Unable to locate current proc;");
				printf("no regs available.\n");
				return;
			}
		} else
			cpu_vaddr += (sizeof pcpu);
	} while (pcpu == NULL);

	getproc();
	if (readpanicregs) {
		label_t pregs;

		(void) lookup(looking = "panic_regs");
		if (cursym == 0)
			goto nosym;

		db_printf(1, "ksetcor:  if( readpanicregs ) getpcb( 0x%X );\n",
		    cursym->s_value );

		if (kvm_read(kvmd, (long)cursym->s_value, (char *)&pregs,
		    sizeof pregs) != sizeof pregs)
			goto nosym;

		if (pregs.val[0] != 0 || pregs.val[1] != 0) {
			/* we really panic'ed */
			getpcb(&pregs);

			/* setup Curthread/Curproc */
			(void) lookup(looking = "panic_thread");
			if (cursym == 0)
				goto nosym;

			if (kvm_read(kvmd, (long)cursym->s_value, 
			    (char *)&Curthread, sizeof Curthread) != 
			    sizeof Curthread)
				printf("Curthread not set\n");

			if (kvm_read(kvmd, (long)&Curthread->t_procp,
		    	    (char *)&Curproc, sizeof Curproc) != sizeof Curproc)
				printf("Curproc not set\n");
		}
	}
	return;

nosym:	
	printf("Cannot adb -k: %s missing symbol %s\n",
	    corfil, looking);
	exit(2);
}


void
setcor()
{
	struct stat stb;
	Elf32_Ehdr core_ehdr;
	Elf32_Phdr *core_phdr;
	char *noteseg;
	int nindx;
	char *offset;
	int i;
	int so_far = 0;
	extern char platname[];
	char    cmd_line[PRARGSZ];
	char    *prog_name;

	struct map_range *fcor_rng1, *fcor_rng2;
	 
	fcor_rng1 = (struct map_range*) calloc(1,sizeof(struct map_range));
	fcor_rng2 = (struct map_range*) calloc(1,sizeof(struct map_range));
#ifdef	KADB
	datmap.map_head->mpr_e = MAXFILE;
#endif	KADB
	datmap.map_head = fcor_rng1;
	datmap.map_tail = fcor_rng2;
	fcor_rng1->mpr_next = fcor_rng2;
						  
	fcor_rng1->mpr_fn = fcor_rng2->mpr_fn  = corfil;
	fcor = getfile(corfil, 2);
	fcor_rng1->mpr_fd = fcor_rng2->mpr_fd = fcor;
	fcor_rng1->mpr_b = fcor_rng2->mpr_b = 0;
	fcor_rng1->mpr_f = fcor_rng2->mpr_f = 0;
	fcor_rng1->mpr_e = MAXFILE;	/* liable to be overwritten */
	if (fcor == -1) {
		  return;
	}
	if (kernel) {
		ksetcor(1);
		return;
	}
	fstat(fcor, &stb);

	/* Exhaustive test for rectitude: */
	if (((stb.st_mode&S_IFMT) == S_IFREG) &&
	    (read(fcor, (char *)&core_ehdr, sizeof (core_ehdr)) ==
	     sizeof (core_ehdr)) &&
	    (core_ehdr.e_ident[EI_MAG0] == ELFMAG0) &&
	    (core_ehdr.e_ident[EI_MAG1] == ELFMAG1) &&
	    (core_ehdr.e_ident[EI_MAG2] == ELFMAG2) &&
	    (core_ehdr.e_ident[EI_MAG3] == ELFMAG3) &&
	    (core_ehdr.e_ident[EI_CLASS] == ELFCLASS32) &&
	    (core_ehdr.e_ident[EI_DATA] == ELFDATA2MSB) &&
	    (core_ehdr.e_type == ET_CORE) &&
	    ((core_ehdr.e_machine == EM_SPARC) ||
	    (core_ehdr.e_machine == EM_SPARC32PLUS)) &&
	    (core_ehdr.e_version == EV_CURRENT)) {

		Elf32_Nhdr *nhdr;
		struct map_range *prev_rng = NULL;

		/* Position to the program header table. */
		if (lseek(fcor, (long) core_ehdr.e_phoff, L_SET) !=
		    core_ehdr.e_phoff) {
			fprintf(stderr,
				"Unable to find core file program header.\n");
			goto bad1;
		}
		/* Get space for a copy of the program header table. */
		if ((core_phdr = (Elf32_Phdr *) malloc(core_ehdr.e_phentsize *
		    core_ehdr.e_phnum)) == NULL) {
			fprintf(stderr,
				"Unable to allocate core program header.\n");
			fcor_rng1->mpr_e = MAXFILE;
			return;
		}
		/* Read the program header table. */
		if (read(fcor, core_phdr, core_ehdr.e_phentsize *
		    core_ehdr.e_phnum) != core_ehdr.e_phentsize *
		    core_ehdr.e_phnum) {
			fprintf(stderr,
				"Unable to read core program header.\n");
			goto bad1;
		}
		/* The first segment should be a "note". */
		nindx = 0;
		if (core_phdr[0].p_type != PT_NOTE) {
			fprintf(stderr, "Core file state info unavailable.\n");
			goto bad1;
		}
		/* If the second segment is also a "note", then use it */
		if (core_phdr[1].p_type == PT_NOTE)
			nindx = 1;

		/* Position to the note segment. */
		if (lseek(fcor, (long) core_phdr[nindx].p_offset, L_SET) !=
		    core_phdr[nindx].p_offset) {
			fprintf(stderr,
			  "Unable to find process state info in core file.\n");
			goto bad1;
		}
		/* Get a place to keep the note segment. */
		if ((noteseg = malloc(core_phdr[nindx].p_filesz)) == NULL) {
			fprintf(stderr,
			  "Unable to allocate core file state info tables.\n");
			fcor_rng1->mpr_e = MAXFILE;
			return;
		}
		/* Read the note segment. */
		if (read(fcor, noteseg, core_phdr[nindx].p_filesz) !=
		    core_phdr[nindx].p_filesz) {
			fprintf(stderr,
				"Unable to read core file state info.\n");
			goto bad1;
		}
		/* there should be a header at the beginning of the segment. */
		nhdr = (Elf32_Nhdr *) noteseg;

		/*
		 * The loop runs until the nhdr pointer reaches the end of
		 * the note segment that we just read.  This will happen as
		 * long as the note segment is well-formed.  Is that too
		 * much to assume?
		 */
		while ((int) nhdr < (int) noteseg +
			(int) core_phdr[nindx].p_filesz) {

			if ((int)nhdr->n_descsz > 0) {
				offset = (char *)(sizeof (Elf32_Nhdr) +
					(int)nhdr + (((int)nhdr->n_namesz +
					sizeof (int) - 1) &
					~(sizeof (int) - 1)));
				switch (nhdr->n_type) {
				case NT_PSTATUS:
					if (!(so_far & (1<<NT_PSTATUS))) {
						memcpy(&Prstatus, offset,
							sizeof (Prstatus));
						Curthread = (kthread_id_t)
						  Prstatus.pr_lwp.pr_reg[R_G7];
						so_far |= (1 << NT_PSTATUS);
						memcpy(&Prfpregs,
						    &Prstatus.pr_lwp.pr_fpreg,
						    sizeof (Prfpregs));
					} else {
						fprintf(stderr,
						"Ignoring another NT_PSTATUS"
						" note segment entry.\n");
					}
					break;
				case NT_PSINFO:
					if (!(so_far & (1<<NT_PSINFO))) {
						memcpy(&core_psinfo, offset,
							sizeof (core_psinfo));
						so_far |= (1 << NT_PSINFO);
					} else {
						fprintf(stderr,
						"Ignoring another NT_PSINFO"
						" note segment entry.\n");
					}
					break;
				case NT_GWINDOWS:
					if (!(so_far & (1<<NT_GWINDOWS))) {
						fprintf(stderr,
						"NT_GWINDOWS currently "
						"unsupported note segment "
						"entry.\n");
						so_far |= (1 << NT_GWINDOWS);
					}
					break;
				case NT_PRXREG:
					if (!(so_far & (1<<NT_PRXREG))) {
						memcpy(&xregs, offset,
							sizeof (xregs));
						so_far |= (1 << NT_PRXREG);
					}
					break;
				case NT_PLATFORM:
					if (!(so_far & (1<<NT_PLATFORM))) {
						memcpy(core_platform, offset,
							sizeof (core_platform));
						if (!strncmp(core_platform,
						    "SUNW,Ultra", 10)) {
							v9flag = 1;
							change_dismode(V9_VIS_MODE,
							    0);
						}
						so_far |= (1 << NT_PLATFORM);
					} else {
						fprintf(stderr,
						"Ignoring another NT_PLATFORM"
						" note segment entry.\n");
					}
					break;
				case NT_AUXV:
					if (!(so_far & (1<<NT_AUXV))) {
						if (core_auxv)
							free(core_auxv);
						core_auxv = malloc(
						    nhdr->n_descsz);
						memcpy(core_auxv, offset,
						    nhdr->n_descsz);
						so_far |= (1 << NT_AUXV);
					} else {
						fprintf(stderr,
						"Ignoring another NT_AUXV"
						" note segment entry.\n");
					}
					break;
				/* recognize but ignore these (for now) */
				case NT_LWPSINFO:
				case NT_LWPSTATUS:
				case NT_PRCRED:
				case NT_UTSNAME:
					break;
				default:
					if (!(so_far & (1<<nhdr->n_type))) {
						fprintf(stderr,
						"Ignoring unrecognized"
						" note segment entry %d.\n",
						nhdr->n_type);
						so_far |= (1 << nhdr->n_type);
					}
					break;
				}
			}
			nhdr = (Elf32_Nhdr *)((int) offset +
				(((int)nhdr->n_descsz + sizeof (int) - 1)
				& ~(sizeof (int) -1)));
		}
		free(noteseg);

		/*
		 * Print pr_fname field as the file name if it is complete.
		 * If it is incomplete, but if pr_psargs is also too long,
		 * it is better to print pr_fname than to print pr_psargs.
		 */
		strcpy(cmd_line, core_psinfo.pr_psargs);
		if (strlen(core_psinfo.pr_fname) < (PRFNSZ-2) ||
			    (prog_name = get_prog_name(cmd_line)) == NULL) {
		    printf("core file = %s -- program ``%s''",
						corfil, core_psinfo.pr_fname);
		} else {
		    printf("core file = %s -- program ``%s''",
						corfil, prog_name);
		}
		if (so_far & (1<<NT_PLATFORM))
		    printf(" on platform %s\n", core_platform);
		else
		    printf("\n");

		/* Allow for an arbitrary number of segments. */
		fcor_rng2 = fcor_rng1;
		for (i = nindx+1; i < (u_int) core_ehdr.e_phnum; i++) {

			/* Does this entry describes something in the file? */
			if (core_phdr[i].p_filesz) {
				/* Save start and end addresses in memory
				 * and file offset.
				 */
				fcor_rng2->mpr_b = core_phdr[i].p_vaddr;
				fcor_rng2->mpr_e = fcor_rng2->mpr_b +
					core_phdr[i].p_filesz;
				fcor_rng2->mpr_f = core_phdr[i].p_offset;

				/* Link previous map to the current one. */
				if (prev_rng != NULL)
					prev_rng->mpr_next = fcor_rng2;
				prev_rng = fcor_rng2;
				fcor_rng2 = (struct map_range *)
					calloc(1, sizeof(struct map_range));
				fcor_rng2->mpr_fn = corfil;
				fcor_rng2->mpr_fd = fcor;
				fcor_rng2->mpr_next = NULL;
			}
		}
		free(core_phdr);
		datmap.map_tail = prev_rng;
		core_to_regs();
		signo = (int) Prstatus.pr_lwp.pr_cursig;
		sigprint(signo);
		printf("\n");
		/*
		 * Make sure we're at least on the same architecture version as the
		 * machine that dumped core, otherwise, we'll map in the wrong shared
		 * libraries. XXX - should try and match release as well.
		 */
		if (get_platname() && use_shlib &&
		    (!strcmp(platname, core_platform) ||
		    (!v9flag && strncmp(platname, "SUNW,Ultra", 10)))) {
			scan_linkmap();
		}
	} else
	    
		{
	      bad1:
		/* either not a regular file, or core struct not convincing */
		printf("not core file = %s\n", corfil);
		fcor_rng1->mpr_e = MAXFILE;
	}
}

#ifdef notdef
regs_not_on_stack ( ) {
	return ( u.u_pcb.pcb_wbcnt > 0 );
}
#endif

#else KADB

/*
 * For kadb's sake, we pretend we can always find
 * the registers on the stack.
 */
regs_not_on_stack ( ) {
	return 0;
}

#endif KADB



/*
 * Only called if kernel debugging (-k or KADB).  Read in (or map)
 * the u-area for the appropriate process and get the registers.
 */
void
getproc()
{
	struct proc proc;
#ifdef	KADB
#define foffset  ((char *)&proc.p_flag  - (char *)&proc)
#define uoffset ((char *)&proc.p_uarea - (char *)&proc)
	struct user *uptr;
	int flags;
#else	/* KADB */
	struct _kthread *pthread = (struct _kthread *) 0;
	label_t tpcb;
#endif	/* KADB */

	if (Curproc == 0)
		return;
#ifdef	KADB
#ifdef	BOGUS
	flags = rwmap('r', (int)Curproc + foffset, DSP, 0);
	if (errflg) {
		errflg = "cannot find proc";
		return;
	}
	if ((flags & SLOAD) == 0) {
		errflg = "process not in core";
		return;
	}
	uptr = (struct user *)rwmap('r', (int)Curproc + uoffset, DSP, 0);
	if (errflg) {
		errflg = "cannot get u area";
		return;
	}
	(void) rwmap('r', (int)&(uptr->u_pcb.pcb_regs), DSP, 0);
	if (errflg) {
		errflg = "cannot read pcb";
		return;
	}
	getpcb(&uptr->u_pcb.pcb_regs);
#endif	/* BOGUS */
#else !KADB
	if (kvm_read(kvmd, (long)Curproc, (char *)&proc, sizeof proc) !=
	    sizeof proc) {
		errflg = "cannot find proc";
		return;
	}
	if (proc.p_stat == SZOMB) {
		errflg = "zombie process - no useful registers";
		return;
	}
	if (kvm_read(kvmd, (long)Curthread+(uint)&pthread->t_pcb,
	    (char *)&tpcb, sizeof tpcb) != sizeof tpcb) {
		errflg = "Unable to find t_pcb";
		return;
	}
	getpcb(&tpcb);
#endif !KADB
}

#if	defined(KADB)
/*
 * The address of a thread struct was put in Curthread elsewhere.
 */
get_thread()
{
	int tpcb_addr;
	struct _kthread *thread = NULL;
#define tpcb_offset ((char *)&thread->t_pcb - (char *)thread)

	if (Curthread == NULL)
		return;

	/* Cobble up the address of the thread struct's label_t. */
	tpcb_addr = (int)Curthread + tpcb_offset;
	getpcb((label_t *)tpcb_addr);
	/* Might as well fix up the one global that we do "know". */
	setreg(Reg_G7, Curthread);
}
#endif	/* defined(kadb) */

/* 
 * getpcb is called from ksetcor() and from getproc() when debugging in
 * kernel mode (-k), we read the pcb structure (and as many
 * registers as we can find) into the place where adb keeps the
 * appropriate registers.  The first time getpcb is called, the
 * argument is a pointer to the register list in the pcb structure
 * in the u-area.  The second time getpcb is called, the argument
 * is a pointer to the registers pulled out of panic_regs.  If these
 * are all zero, assume we are looking at a live kernel and ignore them.
 * Later, getpcb is called to read the registers of various mapped processes.
 *
 * The registers come from the label_t (see types.h) that begins a pcb.
 * The order of those registers is not published anywhere (a label_t is just
 * an array), but I've been told that the two registers are a PC and an SP.
 *
 * Unlike the 68k (whose label_t includes all of the registers), we must
 * then go find where the rest of the registers were saved.
 */
int
getpcb(label_t *regaddr)
{
	register int i;
	register int *rp;
	int pcval, spval;

	db_printf(1, "getpcb:  kernel %d\n", kernel );

	rp = (int *)(&(regaddr->val[0]));
	pcval = *rp++;
	spval = *rp++;
	if (pcval == 0 && spval == 0)
		return;
	setreg(Reg_PC, pcval);
	setreg(Reg_SP, spval);
	
	db_printf(1, "getpcb:  Reg_PC %X, Reg_SP %X\n", pcval, spval );

	/*
	 * We have PC & SP; now we have to find the rest of the registers.
	 *  If in kernel mode (are we always?), the globals and current
	 *   out-registers are unreachable.
	 */
#ifndef KADB
	if( kernel == 0 ) {
		errflg = "internal error -- getpcb not in kernel mode." ;
	}
#endif KADB
	for (i = Reg_G1; i <= Reg_G7; ++i ) setreg( i, 0 );
	for (i = Reg_O1; i <= Reg_O5; ++i ) setreg( i, 0 );
	setreg( Reg_O7, pcval);

	for (i = Reg_L0; i <= Reg_L7; ++i ) {
		setreg( i, rwmap( 'r', (spval + FR_LREG(i)), DSP, 0 ) );
	}
	for (i = Reg_I0; i <= Reg_I7; ++i ) {
		setreg( i, rwmap( 'r', (spval + FR_IREG(i)), DSP, 0 ) );
	}
}

#ifndef KADB

create(f)
	char *f;
{
	int fd;

	if ((fd = creat64(f, 0644)) >= 0) {
		close(fd);
		return (open64(f, wtflag));
	} else
		return (-1);
}

getfile(filnam, cnt)
	char *filnam;
{
	register int fsym;

	if (!strcmp(filnam, "-"))
	    return (-1);
	fsym = open64(filnam, wtflag);
	if (fsym < 0 && xargc > cnt) {
	    if (wtflag) {
		fsym = create(filnam);
		if (fsym < 0) {
		    /* try reading only */
		    fsym = open64(filnam, 0);
		    if (fsym >=0)
			printf("warning: `%s' read-only\n", filnam);
		}
	    }
	    if (fsym < 0)
		printf("cannot open `%s'\n", filnam);
	}
	return (fsym);
}
#endif !KADB

void
setvar()
{

	var[varchk('b')] = (dataseg == -1) ? 0 : proghdr[dataseg].p_vaddr;
	var[varchk('d')] = (dataseg == -1) ? 0 : proghdr[dataseg].p_memsz;
	var[varchk('e')] = *((int *) filhdr.e_ident);
	var[varchk('m')] = *((int *) filhdr.e_ident);
	var[varchk('t')] = proghdr ? proghdr[textseg].p_memsz : 0;
	var[varchk('s')] = stksiz;
	var[varchk('F')] = 0;	/* There's never an fpa on a sparc */
}

#ifndef KADB
/*
 * The address of a thread struct is put in Curthread before get_thread()
 * is called.
 */

int
get_thread(void)
{
	label_t *t_pcb, pcb;
	kthread_t *thread = (kthread_t *) NULL;

	if (Curthread == NULL)
		return -1;

	t_pcb = (label_t *) 
	           ((unsigned long) Curthread + (unsigned long) &thread->t_pcb);
	if (kvm_read(kvmd, (long) t_pcb, (char *) &pcb, sizeof pcb) !=
	             sizeof pcb) {
		(void) printf("Cannot read current thread's t_pcb at 0x%X\n",
			      t_pcb);
		return -1;
	}
	if (getpcb(&pcb) == -1)
		return -1;
	return 0;
}
#endif /* !KADB */

#ifndef KADB
char *
get_prog_name(path)
char    *path;
{
	char    *p;

	/*  Get arg0 - look for the first blank */
	if (p = strchr(path, ' '))
	    *p = 0;

	if (strlen(path) >= (PRARGSZ-1))
	    return ((char *) NULL);

	/* Get the basename */
	return (basename(path));
}
#endif
