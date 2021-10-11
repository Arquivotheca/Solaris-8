/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)interp.c	1.1	98/09/08 SMI"

/*
 * Program to modify interpretor string.
 *
 * The new string must of course be less than or equal in size to the original.
 * The new string is written to the .interp section maintaining the original
 * sections size.  The program header size is modified to indicate the actual
 * string size (thus keeping the kernel exec mechanism happy).
 */
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<errno.h>
#include	<libelf.h>
#include	<gelf.h>
#include	<unistd.h>
#include	<strings.h>

static	const char *	efmt = "%s: %s: %s\n";

static int
process_file(const char * prog, const char * file, Elf * elf,
    const char * interp)
{
	int		pndx, sndx;
	Elf_Scn *	scn;
	Elf_Data *	data;
	GElf_Ehdr	ehdr;
	GElf_Phdr	phdr;
	GElf_Shdr	shdr;
	size_t		olen, nlen;

	/*
	 * Determine if this file already contains an interpretor.
	 */
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		(void) fprintf(stderr, efmt, prog, file,
			elf_errmsg(elf_errno()));
		return (1);
	}

	for (pndx = 0; pndx < ehdr.e_phnum; pndx++) {
		if (gelf_getphdr(elf, pndx, &phdr) == NULL) {
			(void) fprintf(stderr, efmt, prog, file,
				elf_errmsg(elf_errno()));
			return (1);
		}
		if (phdr.p_type == PT_INTERP)
			break;
	}
	if (pndx == ehdr.e_phnum) {
		(void) fprintf(stderr, efmt, prog, file,
			"file does not specify an interpretor");
		return (1);
	}

	/*
	 * Locate the associated section (should be .interp, but we use the
	 * program header offset to find the match).
	 */
	for (sndx = 0; sndx < ehdr.e_shnum; sndx++) {
		if (((scn = elf_getscn(elf, sndx)) == NULL) ||
		    ((gelf_getshdr(scn, &shdr)) == NULL)) {
			(void) fprintf(stderr, efmt, prog, file,
				elf_errmsg(elf_errno()));
			return (1);
		}
		if (shdr.sh_offset == phdr.p_offset)
			break;
	}
	if (sndx == ehdr.e_shnum) {
		(void) fprintf(stderr, efmt, prog, file,
			"unable to locate interpretor section");
		return (1);
	}

	/*
	 * Will the new string fit.  Use the original section size as the
	 * maximum size (the program header may have been modified by a previous
	 * interpretor change).
	 */
	olen = shdr.sh_size;
	nlen = strlen(interp) + 1;
	if (nlen > olen) {
		(void) fprintf(stderr, efmt, prog, file,
			"interpretor name too long");
		return (1);
	}

	/*
	 * Get the actual data, and override it with the new string.
	 */
	if ((data = elf_getdata(scn, NULL)) == NULL) {
		(void) fprintf(stderr, efmt, prog, file,
			elf_errmsg(elf_errno()));
		return (1);
	}
	(void) strncpy((char *)data->d_buf, interp, olen);

	/*
	 * Update the program header to specify the actual string size.
	 */
	phdr.p_filesz = nlen;
	if (gelf_update_phdr(elf, pndx, &phdr) == NULL) {
		(void) fprintf(stderr, efmt, prog, file,
			elf_errmsg(elf_errno()));
		return (1);
	}

	/*
	 * Mark the program headers and section as dirty so that they get
	 * updated.
	 */
	(void) elf_flagphdr(elf, ELF_C_SET, ELF_F_DIRTY);
	(void) elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY);

	if (elf_update(elf, ELF_C_WRITE) == -1) {
		(void) fprintf(stderr, efmt, prog, file,
			elf_errmsg(elf_errno()));
		return (1);
	}

	return (0);
}

main(int argc, char ** argv)
{
	const char *	prog = argv[0];
	const char *	interp = argv[1];
	int		error = 0;

	/*
	 * Verify arguments - need a new interpretor and at least one file to
	 * operate on.
	 */
	if (argc < 3) {
		(void) fprintf(stderr, "usage: %s interpreter file ...\n",
		    prog);
		return (1);
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, "%s: %s\n", prog,
		    elf_errmsg(elf_errno()));
		return (1);
	}

	for (optind++; optind < argc; optind++) {
		const char *	file = argv[optind];
		int		fd;
		Elf *		elf;

		if ((fd = open(file, O_RDWR, 0)) == -1) {
			(void) fprintf(stderr, efmt, prog, file,
			    strerror(errno));
			error = 1;
			continue;
		}

		if ((elf = elf_begin(fd, ELF_C_RDWR, 0)) == NULL) {
			(void) fprintf(stderr, efmt, prog, file,
			    elf_errmsg(elf_errno()));
			error = 1;
			continue;
		}

		if (process_file(prog, file, elf, interp) == 1)
			error = 1;

		(void) close(fd);
		(void) elf_end(elf);
	}

	return (error);
}
