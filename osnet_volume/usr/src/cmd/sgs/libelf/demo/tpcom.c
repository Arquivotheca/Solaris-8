/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc.
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */

#pragma	ident	"@(#)tpcom.c	1.3	97/12/05 SMI"

/*
 * tpcom: Threaded Print Comment
 *
 * tpcom is a threaded version of the pcom program.  It will create
 * a new thread for each new ELF descriptor that it examines.  It
 * will then examine each elf descriptor and print the .comment section
 * if found.
 *
 * This program demonstrates that libelf is MT-Safe and the usage
 * of elf_begin(ELF_C_READ).
 */


#include <stdio.h>
#include <libelf.h>
#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>


#define	NUMLWPS		32		/* arbratary number of LWPS */

static const char * CommentStr = ".comment";

/*
 * arguements to be passed into process_elf().
 */
typedef struct {
	Elf *	pe_elf;
	char *	pe_file;		/* elf member name */
	int	pe_fd;
	short	pe_member;		/* is this an archive member? */
} pe_args;


static mutex_t	printlock = DEFAULTMUTEX;	/* printlock used to */
						/* group output */
						/* of comment sections */

static void
print_comment(Elf * elf, const char * file)
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
		    shdr.sh_name)) == 0) {
			int	i;
			char *	ptr;

			mutex_lock(&printlock);
			(void) printf("%s .comment:\n", file);

			/*
			 * Get the data associated with the .comment
			 * section.
			 */
			if ((data = elf_getdata(scn, 0)) == 0) {
				(void) fprintf(stderr,
					"%s: elf_getdata() failed: %s\n",
					file, elf_errmsg(0));
				mutex_unlock(&printlock);
				return;
			}
			/*
			 * Data in a .comment section is a list of 'null'
			 * terminated strings.  The following will print
			 * one string per line.
			 */
			for (i = 0, ptr = (char *)data->d_buf;
			    i < data->d_size; i++)
				if (ptr[i]) {
					(void) puts(&ptr[i]);
					i += strlen(&ptr[i]);
				}
			(void) putchar('\n');
			mutex_unlock(&printlock);
		}
	}

}


static void
process_elf(pe_args * pep)
{
	Elf_Cmd	cmd;
	Elf *	_elf;

	switch (elf_kind(pep->pe_elf)) {
	case ELF_K_ELF:
		print_comment(pep->pe_elf, pep->pe_file);
		break;
	case ELF_K_AR:
		cmd = ELF_C_READ;
		while ((_elf = elf_begin(pep->pe_fd, cmd,
		    pep->pe_elf)) != 0) {
			Elf_Arhdr *	arhdr;
			pe_args *	_pep;
			int		rc;

			if ((arhdr = elf_getarhdr(_elf)) == 0) {
				(void) fprintf(stderr,
					"%s: elf_getarhdr() failed: %s\n",
					pep->pe_file, elf_errmsg(0));
			}
			cmd = elf_next(_elf);
			_pep = malloc(sizeof (pe_args));
			_pep->pe_elf = _elf;
			_pep->pe_file = malloc(strlen(pep->pe_file) +
				strlen(arhdr->ar_name) + 5);
			(void) sprintf(_pep->pe_file,
				"%s(%s)", pep->pe_file, arhdr->ar_name);
			_pep->pe_fd = pep->pe_fd;
			_pep->pe_member = 1;
			if ((rc = thr_create(NULL, 0,
			    (void *(*)(void *))process_elf,
			    (void *)_pep, THR_DETACHED, 0)) != 0) {
				(void) fprintf(stderr,
					"thr_create() failed, rc = %d\n", rc);
			}
		}
		break;
	default:
		if (!pep->pe_member) {
			mutex_lock(&printlock);
			(void) fprintf(stderr,
				"%s: unexpected elf_kind(): 0x%x\n",
				pep->pe_file, elf_kind(pep->pe_elf));
			mutex_unlock(&printlock);
		}
	}

	(void) elf_end(pep->pe_elf);
	if (pep->pe_member)
		free(pep->pe_file);
	free(pep);
	thr_exit(0);
}


main(int argc, char ** argv)
{
	int	i;


	if (argc < 2) {
		(void) printf("usage: %s elf_file ...\n", argv[0]);
		exit(1);
	}

	/*
	 * Initialize the elf library, must be called before elf_begin()
	 * can be called.
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr,
			"elf_version() failed: %s\n", elf_errmsg(0));
		exit(1);
	}

	/*
	 * create an arbitrary number of LWP's to run the
	 * threads that will be created.
	 */
	if (thr_setconcurrency(NUMLWPS) != 0) {
		(void) fprintf(stderr, "thread setconcurrency failed\n");
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		int		fd;
		Elf *		elf;
		pe_args *	pep;
		int		rc;
		char *		elf_fname;

		elf_fname = argv[i];

		if ((fd = open(elf_fname, O_RDONLY)) == -1) {
			perror("open");
			continue;
		}

		/*
		 * Attempt to open an Elf descriptor Read/Write
		 * for each file.
		 */
		if ((elf = elf_begin(fd, ELF_C_READ, 0)) == NULL) {
			mutex_lock(&printlock);
			(void) fprintf(stderr, "elf_begin() failed: %s\n",
			    elf_errmsg(0));
			mutex_unlock(&printlock);
			(void) close(fd);
			continue;
		}
		pep = malloc(sizeof (pe_args));
		pep->pe_elf = elf;
		pep->pe_file = elf_fname;
		pep->pe_fd = fd;
		pep->pe_member = 0;
		if ((rc = thr_create(NULL, 0, (void *(*)(void *))process_elf,
		    (void *)pep, THR_DETACHED, 0)) != 0) {
			mutex_lock(&printlock);
			(void) fprintf(stderr,
				"thr_create() failed with code: %d\n", rc);
			mutex_unlock(&printlock);
			exit(1);
		}
	}

	thr_exit(0);
}
