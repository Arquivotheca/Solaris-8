/*
 *	Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)dyntoexec.c	1.3	92/09/04 SMI"

/*
 * Program to copy a dynamic library to a new file for use with prof(1).
 * When profil(1)'ing a shared library the output mon.out file must be
 * associated to a symbol table by prof(1).  Prof(1) requires that the
 * symbol table reside in an elf executable (ie. it refuses to read a
 * ET_DYN file), and only symbols associated with executable segments are
 * analyzed.
 *
 * This routine copies the input file to the output file and:
 *	o	sets the elf type to executable
 *	o	(optionaly) increments symbol addresses by the offset provided.
 *
 */
#include	<sys/types.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sys/elf.h>
#include	<sys/mman.h>
#include	<stdlib.h>
#include	"machdep.h"

extern	char *	optarg;
extern	int	optind;

static Addr	Disp = 0;

static const char *	usage_msg = "usage: %s [-d disp] infile outfile\n";

main(int argc, char ** argv)
{
	Ehdr		ehdr, * eptr;
	Sym *		sym;
	int		c, in, out;
	unsigned int	size, cnt;
	char *		buffer[BUFSIZ], * addr;
	Shdr *		shdr;

	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
		case 'd':
			Disp = strtoul(optarg, (char **)NULL, 0);
			break;
		case '?':
			(void) fprintf(stderr, usage_msg, argv[0]);
			exit(1);
		default:
			break;
		}
	}

	if ((argc - optind) != 2) {
		(void) fprintf(stderr, usage_msg, argv[0]);
		exit(1);
	}

	if ((in = open(argv[optind], O_RDONLY)) == -1) {
		(void) fprintf(stderr, "can't open %s for reading\n",
			argv[optind]);
		exit(1);
	}
	size = read(in, &ehdr, sizeof (Ehdr));
	if ((ehdr.e_ident[0] != 0x7f) ||
	    (ehdr.e_ident[1] != 'E') ||
	    (ehdr.e_ident[2] != 'L') ||
	    (ehdr.e_ident[3] != 'F')) {
		(void) fprintf(stderr, "input file %s is not elf\n",
			argv[optind]);
		exit(1);
	}

	if ((out = open(argv[++optind], O_RDWR|O_CREAT, 0666)) == -1) {
		(void) fprintf(stderr, "can't open %s for writing\n",
			argv[optind]);
		exit(1);
	}

	/*
	 * Copy the input file to the output file
	 */
	ehdr.e_type = ET_EXEC;
	(void) write(out, &ehdr, size);
	/* LINTED */
	while (1) {
		if ((c = read(in, &buffer, BUFSIZ)) == 0)
			break;
		size += c;
		(void) write(out, &buffer, c);
	}

	if (Disp == 0)
		return (0);

	/*
	 * Scan the file for the symbol tables and add the displacement
	 */
	if ((addr = (char *)mmap(0, size, (PROT_READ | PROT_WRITE),
		MAP_SHARED, out, 0)) == (char *)-1) {
		(void) fprintf(stderr, "can't map output file\n");
		exit(1);
	}

	/* LINTED */
	eptr = (Ehdr *)addr;
	/* LINTED */
	shdr = (Shdr *)(addr + eptr->e_shoff);
	for (cnt = 0; cnt < (int)eptr->e_shnum; cnt++, shdr++) {
		if (shdr->sh_type == SHT_SYMTAB) {
			int	num, cnt;
			/* LINTED */
			sym = (Sym *) (addr + shdr->sh_offset);
			sym++;
			num = (shdr->sh_size / shdr->sh_entsize) - 1;

			for (cnt = 0; cnt < num; cnt++, sym++) {
				int	type = ELF_ST_TYPE(sym->st_info);
				if ((type == STT_FILE) ||
				    (type == STT_SECTION)) {
					continue;
				}
				sym->st_value = sym->st_value + Disp;
			}
		}
	}

	return (0);
}
