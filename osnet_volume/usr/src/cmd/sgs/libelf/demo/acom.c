/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)acom.c	1.3	97/12/05 SMI"

/*
 * acom: Append Comment
 *
 * This program demonstrates the use of the libelf interface to
 * modify a ELF file. This program will open an ELF file and
 * either modify an existing .comment section and/or append
 * a new .comment section to an existing ELF file.
 */

#include <stdio.h>
#include <libelf.h>
#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


static const char * CommentStr =	".comment";

static void
update_comment(Elf * elf, const char * file, const char * comment)
{
	Elf_Scn *	scn = 0;
	GElf_Shdr	shdr;
	GElf_Ehdr	ehdr;
	Elf_Data *	data;

	if (gelf_getehdr(elf, &ehdr) == 0) {
		(void) fprintf(stderr, "%s: elf_getehdr() failed: %s\n",
			file, elf_errmsg(0));
		return;
	}

	while ((scn = elf_nextscn(elf, scn)) != 0) {
		/*
		 * Do a string compare to examine each section header
		 * to see if it is a ".comment" section.  If it is then
		 * this is the section we want to process.
		 */
		if (gelf_getshdr(scn, &shdr) == 0) {
			(void) fprintf(stderr,
				"%s: elf_getshdr() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		if (strcmp(CommentStr, elf_strptr(elf, ehdr.e_shstrndx,
		    shdr.sh_name)) == 0)
			break;
	}

	if (scn == 0) {
		int	ndx;

		(void) printf("%s has no .comment section.  "
			"Creating one...\n", file);
		/*
		 * First add the ".comment" string to the string table
		 */
		if ((scn = elf_getscn(elf, ehdr.e_shstrndx)) == 0) {
			(void) fprintf(stderr, "%s: elf_getscn() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		if ((data = elf_getdata(scn, 0)) == 0) {
			(void) fprintf(stderr, "%s: elf_getdata() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		ndx = data->d_off + data->d_size;
		if ((data = elf_newdata(scn)) == 0) {
			(void) fprintf(stderr, "%s: elf_newdata() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		data->d_buf = (void *)CommentStr;
		data->d_size = strlen(CommentStr) + 1;
		data->d_align = 1;

		/*
		 * Add the ".comment" section to the end of the file.
		 * Initialize the fields in the Section Header that
		 * libelf will not fill in.
		 */
		if ((scn = elf_newscn(elf)) == 0) {
			(void) fprintf(stderr, "%s: elf_newscn() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		if (gelf_getshdr(scn, &shdr) == 0) {
			(void) fprintf(stderr,
				"%s: elf_getshdr() failed: %s\n",
				file, elf_errmsg(0));
			return;
		}
		shdr.sh_name = ndx;
		shdr.sh_type = SHT_PROGBITS;
		shdr.sh_flags = 0;
		shdr.sh_addr = 0;
		shdr.sh_link = 0;
		shdr.sh_info = 0;

		/*
		 * Flush the changes to the underlying elf32 or elf64
		 * section header.
		 */
		gelf_update_shdr(scn, &shdr);
	}

	if (shdr.sh_addr != 0) {
		(void) printf("%s: .comment section is part of a "
			"loadable segment, it cannot be changed.\n", file);
		return;
	}

	if ((data = elf_newdata(scn)) == 0) {
		(void) fprintf(stderr, "%s: elf_getdata() failed: %s\n",
			file, elf_errmsg(0));
		return;
	}
	data->d_buf = (void *)comment;
	data->d_size = strlen(comment) + 1;
	data->d_align = 1;

	if (elf_update(elf, ELF_C_WRITE) == -1)
		(void) fprintf(stderr, "%s: elf_update() failed: %s\n", file,
			elf_errmsg(0));
}



main(int argc, char ** argv)
{
	int	i;
	char *	new_comment;


	if (argc < 3) {
		(void) printf("usage: %s <new comment> elf_file ...\n",
			argv[0]);
		exit(1);
	}

	/*
	 * Initialize the elf library, must be called before elf_begin()
	 * can be called.
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr, "elf_version() failed: %s\n",
			elf_errmsg(0));
		exit(1);
	}

	/*
	 * The new comment is passed in through the command line.
	 * This string will be used to update the .comment section of
	 * the specified ELF files.
	 */
	new_comment = argv[1];
	for (i = 2; i < argc; i++) {
		int	fd;
		Elf *	elf;
		char *	elf_fname;

		elf_fname = argv[i];
		if ((fd = open(elf_fname, O_RDWR)) == -1) {
			perror("open");
			continue;
		}

		/*
		 * Attempt to open an Elf descriptor Read/Write
		 * for each file.
		 */
		if ((elf = elf_begin(fd, ELF_C_RDWR, 0)) == NULL) {
			(void) fprintf(stderr, "elf_begin() failed: %s\n",
			    elf_errmsg(0));
			(void) close(fd);
			continue;
		}
		/*
		 * Determine what kind of elf file this is:
		 */
		if (elf_kind(elf) == ELF_K_ELF)
			update_comment(elf, elf_fname, new_comment);
		else
			(void) printf("%s not of type ELF_K_ELF.  "
				"elf_kind == %d\n",
				elf_fname, elf_kind(elf));

		(void) elf_end(elf);
		(void) close(fd);
	}

	return (0);
}
