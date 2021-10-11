/*
 * Copyright (c) 1985-1994, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)read_elf_file.c	1.1	95/06/21 SMI"

#include <sys/types.h>
#include <sys/elf.h>
#include <sys/param.h>
#include <sys/platnames.h>
#include <sys/boot_redirect.h>
#include "cbootblk.h"

unsigned long
read_elf_file(int fd, char *file)
{
	Elf32_Ehdr elfhdr;
	Elf32_Phdr phdr;	/* program header */
	register int i;

	seekfile(fd, (off_t)0);
	if (readfile(fd, (char *)&elfhdr, sizeof (elfhdr)) != sizeof (elfhdr))
		goto bad;
	if (elfhdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    elfhdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    elfhdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    elfhdr.e_ident[EI_MAG3] != ELFMAG3 ||
	    elfhdr.e_phnum == 0)
		goto bad;

	for (i = 0; i < (int)elfhdr.e_phnum; i++) {
		seekfile(fd,
		    (off_t)(elfhdr.e_phoff + (elfhdr.e_phentsize * i)));
		if (readfile(fd, (char *)&phdr, sizeof (phdr)) < sizeof (phdr))
			goto bad;
		if (phdr.p_type != PT_LOAD)
			continue;
		seekfile(fd, (off_t)phdr.p_offset);
		if (readfile(fd, (char *)phdr.p_vaddr, (int)phdr.p_filesz) <
		    phdr.p_filesz)
			goto bad;
		if (phdr.p_memsz > phdr.p_filesz)
			bzero((caddr_t)phdr.p_vaddr + phdr.p_filesz,
			    (size_t)(phdr.p_memsz - phdr.p_filesz));
	}
	return ((unsigned long)elfhdr.e_entry);
bad:
	(void) closefile(fd);
	puts("bootblk: "); puts(file); puts(" - not an ELF file\n");
	exit();
	/*NOTREACHED*/
}
