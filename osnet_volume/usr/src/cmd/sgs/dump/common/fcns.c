/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fcns.c	6.10	98/10/09 SMI"

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<libelf.h>
#include	<limits.h>
#include	<sys/elf_M32.h>
#include	<sys/elf_386.h>
#include	<sys/elf_SPARC.h>
#include	"dump.h"

extern int	p_flag;
extern char *	prog_name;

/*
 * Symbols in the archive symbol table are in machine independent
 * representation.  This function translates each symbol.
 */
static long
sgetl(char * buffer)
{
	register long	w = 0;
	register int	i = CHAR_BIT * sizeof (long);

	while ((i -= CHAR_BIT) >= 0)
		w |= (long) ((unsigned char) *buffer++) << i;
	return (w);
}

/*
 * Print the symbols in the archive symbol table.
 * The function requires a file descriptor returned by
 * a call to open(), a pointer to an archive file opened with elf_begin(),
 * a pointer to the archive header of the associated archive symbol table,
 * and the name of the archive file.
 * Seek to the start of the symbol table and read it into memory.
 * Assume that the number of entries recorded in the beginning
 * of the archive symbol table is correct but check for truncated
 * symbol table.
 */
void
ar_sym_read(int fd, Elf *elf_file, Elf_Arhdr *ar_p, char *filename)
{
	long    here;
	long	symsize;
	long	num_syms;
	char    *offsets;
	long    n;
	typedef unsigned char    word[4];
	word *ar_sym;

	(void) printf("%s:\n", filename);

	if (!p_flag) {
		(void) printf("     **** ARCHIVE SYMBOL TABLE ****\n");
		(void) printf("%-8s%s\n\n", "Offset", "Name");
	}

	if ((symsize = ar_p->ar_size) == 0) {
		(void) fprintf(stderr,
			"%s: %s: cannot read symbol table header\n",
			prog_name, filename);
		return;
	}
	if ((ar_sym = (word *)malloc(symsize * sizeof (char))) == NULL) {
		(void) fprintf(stderr,
			"%s: %s: could not malloc space\n",
			prog_name, filename);
		return;
	}

	here = elf_getbase(elf_file);
	if ((lseek(fd, here, 0)) != here) {
		(void) fprintf(stderr,
			"%s: %s: could not lseek\n", prog_name, filename);
		return;
	}

	if ((read(fd, ar_sym, symsize * sizeof (char))) == -1) {
		(void) fprintf(stderr,
			"%s: %s: could not read\n", prog_name, filename);
		return;
	}

	num_syms = sgetl((char *)ar_sym);
	ar_sym++;
	offsets = (char *)ar_sym;
	offsets += (num_syms)*sizeof (long);

	for (; num_syms; num_syms--, ar_sym++) {
		(void) printf("%-8ld", sgetl((char *)ar_sym));
		if ((n = strlen(offsets)) == NULL) {
			(void) fprintf(stderr, "%s: %s: premature EOF\n",
				prog_name, filename);
			return;
		}
		(void) printf("%s\n", offsets);
		offsets += n + 1;
	}
}

/*
 * Print the program execution header.  Input is an opened ELF object file, the
 * number of structure instances in the header as recorded in the ELF header,
 * and the filename.
 */
void
dump_exec_header(Elf *elf_file, unsigned nseg, char *filename)
{
	GElf_Ehdr ehdr;
	GElf_Phdr p_phdr;
	int counter;
	int field;
	extern int v_flag, p_flag;
	extern char *prog_name;

	if (gelf_getclass(elf_file) == ELFCLASS64)
		field = 16;
	else
		field = 12;

	if (!p_flag) {
		(void) printf(" ***** PROGRAM EXECUTION HEADER *****\n");
		(void) printf("%-*s%-*s%-*s%s\n",
		    field, "Type", field, "Offset",
		    field, "Vaddr", "Paddr");
		(void) printf("%-*s%-*s%-*s%s\n\n",
		    field, "Filesz", field, "Memsz",
		    field, "Flags", "Align");
	}

	if ((gelf_getehdr(elf_file, &ehdr) == 0) || (ehdr.e_phnum == 0)) {
		return;
	}

	for (counter = 0; counter < nseg; counter++) {

		if (gelf_getphdr(elf_file, counter, &p_phdr) == 0) {
			(void) fprintf(stderr,
			"%s: %s: premature EOF on program exec header\n",
				prog_name, filename);
			return;
		}

		if (!v_flag) {
			(void) printf(
	"%-*d%-#*llx%-#*llx%-#*llx\n%-#*llx%-#*llx%-*u%-#*llx\n\n",
				field, EC_WORD(p_phdr.p_type),
				field, EC_OFF(p_phdr.p_offset),
				field, EC_ADDR(p_phdr.p_vaddr),
				field, EC_ADDR(p_phdr.p_paddr),
				field, EC_XWORD(p_phdr.p_filesz),
				field, EC_XWORD(p_phdr.p_memsz),
				field, EC_WORD(p_phdr.p_flags),
				field, EC_XWORD(p_phdr.p_align));
		} else {
			switch (p_phdr.p_type) {
			case PT_NULL:
				(void) printf("%-*s", field, "NULL");
				break;
			case PT_LOAD:
				(void) printf("%-*s", field, "LOAD");
				break;
			case PT_DYNAMIC:
				(void) printf("%-*s", field, "DYN");
				break;
			case PT_INTERP:
				(void) printf("%-*s", field, "INTERP");
				break;
			case PT_NOTE:
				(void) printf("%-*s", field, "NOTE");
				break;
			case PT_PHDR:
				(void) printf("%-*s", field, "PHDR");
				break;
			case PT_SHLIB:
				(void) printf("%-*s", field, "SHLIB");
				break;
			default:
				(void) printf("%-*d", field,
					(int)p_phdr.p_type);
				break;
			}
			(void) printf(
				"%-#*llx%-#*llx%-#*llx\n%-#*llx%-#*llx",
				field, EC_OFF(p_phdr.p_offset),
				field, EC_ADDR(p_phdr.p_vaddr),
				field, EC_ADDR(p_phdr.p_paddr),
				field, EC_XWORD(p_phdr.p_filesz),
				field, EC_XWORD(p_phdr.p_memsz));

			switch (p_phdr.p_flags) {
			case 0: (void) printf("%-*s", field, "---"); break;
			case PF_X:
				(void) printf("%-*s", field, "--x");
				break;
			case PF_W:
				(void) printf("%-*s", field, "-w-");
				break;
			case PF_W+PF_X:
				(void) printf("%-*s", field, "-wx");
				break;
			case PF_R:
				(void) printf("%-*s", field, "r--");
				break;
			case PF_R+PF_X:
				(void) printf("%-*s", field, "r-x");
				break;
			case PF_R+PF_W:
				(void) printf("%-*s", field, "rw-");
				break;
			case PF_R+PF_W+PF_X:
				(void) printf("%-*s", field, "rwx");
				break;
			default:
				(void) printf("%-*d", field, p_phdr.p_flags);
				break;
			}
			(void) printf(
				"%-#*llx\n\n", field, EC_XWORD(p_phdr.p_align));
		}
	}
}
