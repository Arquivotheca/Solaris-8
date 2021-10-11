/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)symtab.c	1.7	98/07/24 SMI"

/*
 * This file contains code for the crash functions: nm, and symbol, as well
 * as the initialization routine rdsymtab.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>

#include "crash.h"

#include <sys/elf.h>
#include <libelf.h>

/*
 * XXX -- why the $#%@! isn't this stuff defined in machelf.h???
 */
#if defined(_ELF64)
#define	elf_getshdr	elf64_getshdr
#define	ELF_ST_BIND	ELF64_ST_BIND
#define	ELF_ST_TYPE	ELF64_ST_TYPE
#else
#define	elf_getshdr	elf32_getshdr
#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#endif

static int	nmlst_tstamp;		/* namelist timestamp */

static Sym *stbl;			/* symbol table */
static int symcnt;				/* symbol count */
char *strtbl;				/* pointer to string table */
extern char *namelist;

static int rdelfsym(FILE *);
static void prnm(char *);

/* symbol table initialization function */

void
rdsymtab()
{
	FILE *np;
	Ehdr filehdr;

	/*
	 * Open the namelist file and associate a stream with it.
	 * Read the file into a buffer. Determine if the file is
	 * in the correct format via a magic number check.
	 * An invalid format results in a return to main(). Otherwise,
	 * dynamically allocate enough space for the namelist.
	 */

	if (!(np = fopen(namelist, "r")))
		fatal("cannot open namelist file\n");
	if (fread((char *)&filehdr, sizeof (Ehdr), 1, np) != 1)
		fatal("read error in namelist file\n");
	if ((filehdr.e_ident[0] == ELFMAG0) &&
		(filehdr.e_ident[1] == ELFMAG1) &&
		(filehdr.e_ident[2] == ELFMAG2) &&
		(filehdr.e_ident[3] == ELFMAG3)) {
		rewind(np);
		if (rdelfsym(np) != 0)
			fatal("namelist not in a.out format\n");
	}
	fclose(np);
}


/* find symbol */
Sym *
findsym(addr)
unsigned long addr;
{
	Sym *sp;
	Sym *save;
	unsigned long bestval;

	bestval = 0;
	save = NULL;

	for (sp = stbl; sp < &stbl[symcnt]; sp++) {
		if (((ELF_ST_TYPE(sp->st_info) == STB_LOCAL) ||
			(ELF_ST_TYPE(sp->st_info) == STB_GLOBAL) ||
			(ELF_ST_TYPE(sp->st_info) == STB_WEAK)) &&
			((unsigned long)sp->st_value <= addr) &&
			((unsigned long)sp->st_value > bestval)) {
			save = sp;
			bestval = sp->st_value;
			if (bestval == addr)
				break;
		}
	}
	return (save);
}

/* get arguments for ds and ts functions */
void
getsymbol()
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			prsymbol(args[optind++], 0);
		} while (args[optind]);
	} else longjmp(syn, 0);
}

/* print result of ds and ts functions */
void
prsymbol(string, addr)
char *string;
intptr_t addr;
{
	Sym *sp;

	if (string && addr == 0) {
		if ((addr = strcon(string, 'h')) == -1)
			error("\n");
	}

	if (!(sp = findsym((unsigned long)addr))) {
		if (string)
			prerrmes("%s does not match\n", string);
		else
			prerrmes("%x does not match\n", addr);
		return;
	}

	fprintf(fp, "%s", strtbl + sp->st_name);

	fprintf(fp, "+0x%lx\n", addr - (intptr_t)sp->st_value);
}


/* search symbol table */
Sym *
symsrch(s)
char *s;
{
	Sym *sp;
	Sym *found;
	char *name;

	found = 0;

	for (sp = stbl; sp < &stbl[symcnt]; sp++) {
		if (((ELF_ST_TYPE(sp->st_info) == STB_LOCAL) ||
			(ELF_ST_TYPE(sp->st_info) == STB_GLOBAL) ||
			(ELF_ST_TYPE(sp->st_info) == STB_WEAK)) &&
			((unsigned long)sp->st_value >= 0x10000)) {
			name = strtbl + sp->st_name;
			if (strcmp(name, s) == 0) {
				found = sp;
				break;
			}
		}
	}
	return (found);
}

/* get arguments for nm function */
void
getnm()
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		do {
			prnm(args[optind++]);
		} while (args[optind]);
	else longjmp(syn, 0);
}


/* print result of nm function */
void
prnm(string)
char *string;
{
	char *cp, *ty, *stb;
	Sym *sym;

	if ((sym = symsrch(string)) == NULL) {
		error("symbol %s not found\n", string);
		return;
	}

	fprintf(fp, "%s   %08.8lx  ", string, sym->st_value);

	if (sym->st_shndx == SHN_ABS)
		cp = " absolute";
	else if (sym->st_shndx == SHN_COMMON)
		cp = " common";
	else cp = "";

	if (ELF_ST_TYPE(sym->st_info) == STT_OBJECT)
		ty = "OBJT";
	else if (ELF_ST_TYPE(sym->st_info) == STT_FUNC)
		ty = "FUNC";
	else if (ELF_ST_TYPE(sym->st_info) == STT_FILE)
		ty = "FILE";
	else if (ELF_ST_TYPE(sym->st_info) == STT_NUM)
		ty = "NUM";
	else if (ELF_ST_TYPE(sym->st_info) == STT_SECTION)
		ty = "SECT";
	else if (ELF_ST_TYPE(sym->st_info) == STT_NOTYPE)
		ty = "NOTY";

	if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL)
		stb = "GLOB";
	else if (ELF_ST_BIND(sym->st_info) == STB_LOCAL)
		stb = "LOCL";
	else if (ELF_ST_BIND(sym->st_info) == STB_WEAK)
		stb = "WEAK";
	else    stb = "NUM";

	fprintf(fp, "type %s bind %s %s\n", ty, stb, cp);
}

int
rdelfsym(FILE *fp)
{
	int i;
	Elf *elfd;
	Elf_Scn	*scn;
	Shdr *eshdr;
	Sym *sp, *sy;
	Sym *symtab, *ts_symb;
	Elf_Data *data;
	Elf_Data *strdata;
	int fd;
	long nsyms;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fatal("ELF Access Library out of date\n");
	}

	fd = fileno(fp);

	if ((lseek(fd, 0L, 0)) == -1L) {
		fatal("Unable to rewind namelist file\n");
	}

	if ((elfd = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fatal("Unable to elf begin\n");
	}

	if ((elf_kind(elfd)) != ELF_K_ELF) {
		elf_end(elfd);
		return (-1);
	}

	scn = NULL;
	while ((scn = elf_nextscn(elfd, scn)) != NULL) {

		if ((eshdr = elf_getshdr(scn)) == NULL) {
			elf_end(elfd);
			fatal("cannot read section header\n");
		}

		if (eshdr->sh_type == SHT_SYMTAB) {
			break;		/* Can only do 1 symbol table */
		}
	}

		/* Should have scn and eshdr for symtab */

	data = NULL;
	if (((data = elf_getdata(scn, data)) == NULL) ||
		(data->d_size == 0) || (!data->d_buf)) {
			elf_end(elfd);
			fatal("can not read symbol table\n");
	}

	symtab = (Sym *)data->d_buf;

	nsyms = data->d_size / sizeof (Sym);
	/*
	 *	get string table
	 */

	if ((scn = elf_getscn(elfd, eshdr->sh_link)) == NULL) {
		elf_end(elfd);
		fatal("ELF strtab read error\n");
	}

	strdata = NULL;
	if (((strdata = elf_getdata(scn, strdata)) == NULL) ||
		(strdata->d_size == 0) || (!strdata->d_buf)) {
			elf_end(elfd);
			fatal("string table read failure\n");
	}

	if ((strtbl = malloc(strdata->d_size)) == NULL)
		fatal("cannot allocate space for string table\n");

	(void) memcpy(strtbl, strdata->d_buf, strdata->d_size);

	if ((stbl = (Sym *)
		malloc((size_t)(nsyms * sizeof (Sym)))) == NULL)
		fatal("cannot allocate space for namelist\n");

	/*
	 * Copy symbols to malloc'ed area. XXX - Not sure if necessary.
	 */

	symcnt = 0;
	sp = stbl;
	sy = symtab;
	for (i = 0; i < nsyms; i++, sy++) {
		if ((ELF_ST_TYPE(sy->st_info)) == STT_FILE)
			continue;
		if ((ELF_ST_TYPE(sy->st_info)) == STT_SECTION)
			continue;
		sp->st_name = sy->st_name;
		sp->st_value = sy->st_value;
		sp->st_size = sy->st_size;
		sp->st_info = sy->st_info;
		sp->st_shndx = sy->st_shndx;
		sp++;
		symcnt++;
	}

	/* Get time stamp */

	if (!(ts_symb = symsrch("crash_sync")))
		nmlst_tstamp = 0;
	else {

		if ((scn = elf_getscn(elfd, ts_symb->st_shndx)) == NULL) {
			elf_end(elfd);
			fatal("ELF timestamp scn read error\n");
		}

		if ((eshdr = elf_getshdr(scn)) == NULL) {
			elf_end(elfd);
			fatal("cannot read timestamp section header\n");
		}

		if ((lseek(fd, (long)(ts_symb->st_value - eshdr->sh_addr +
		    eshdr->sh_offset), 0)) == -1L)
			fatal("could not seek to namelist timestamp\n");

		if ((read(fd, (char *)&nmlst_tstamp, sizeof (nmlst_tstamp))) !=
				sizeof (nmlst_tstamp))
			fatal("could not read namelist timestamp\n");
	}

	elf_end(elfd);

	return (0);
}
