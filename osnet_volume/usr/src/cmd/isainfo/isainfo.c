/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isainfo.c	1.3	99/10/05 SMI"

#include <sys/types.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>

static char *pgmname;

/*
 * Extract the isalist(5) for userland from the kernel.
 */
static char *
isalist(void)
{
	char *buf;
	size_t bufsize = BUFSIZ;	/* wild guess */
	long ret;

	buf = malloc(bufsize);
	do {
		ret = sysinfo(SI_ISALIST, buf, bufsize);
		if (ret == -1l)
			return (NULL);
		if (ret > bufsize) {
			bufsize = ret;
			buf = realloc(buf, bufsize);
		} else
			break;
	} while (buf != NULL);

	return (buf);
}

/*
 * Extract the kernel's "isalist" by poking around in the kernel's
 * ELF header.
 *
 * XXX	Perhaps we should have an SI_KISALIST option?
 */
static char *
kisalist(void)
{
	static const char ksyms[] = "/dev/ksyms";
	short e_machine;
	int d;
	Elf *elf;
	char *ident;
	struct utsname unm;

	if (uname(&unm) != -1) {
		/*
		 * Shortcut: we know these machines will only
		 * ever have a 32-bit sparc kernel on them.
		 */
		if (strcmp(unm.machine, "sun4m") == 0 ||
		    strcmp(unm.machine, "sun4d") == 0)
			return ("sparc");
	}

	if ((d = open(ksyms, O_RDONLY)) < 0) {
		(void) fprintf(stderr,
		    gettext("%s: cannot open: %s -- %s\n"),
		    pgmname, ksyms, strerror(errno));
		return (NULL);
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) fprintf(stderr,
		    gettext("%s: internal error: ELF library out of date?\n"),
		    pgmname);
		(void) close(d);
		return (NULL);
	}

	elf = elf_begin(d, ELF_C_READ, (Elf *)0);
	if (elf_kind(elf) != ELF_K_ELF) {
		(void) elf_end(elf);
		(void) close(d);
		return (NULL);
	}

	ident = elf_getident(elf, 0);
	e_machine = EM_NONE;
	if (ident[EI_CLASS] == ELFCLASS32) {
		Elf32_Ehdr *ehdr;

		if ((ehdr = elf32_getehdr(elf)) != NULL)
			e_machine = ehdr->e_machine;

	} else if (ident[EI_CLASS] == ELFCLASS64) {
		Elf64_Ehdr *ehdr;

		if ((ehdr = elf64_getehdr(elf)) != NULL)
			e_machine = ehdr->e_machine;
	}
	(void) elf_end(elf);
	(void) close(d);

	switch (e_machine) {
	case EM_SPARC:
	case EM_SPARC32PLUS:
		return ("sparc");
	case EM_386:
		return ("i386");
	case EM_SPARCV9:
		return ("sparcv9");
	case EM_IA_64:
		return ("ia64");
	default:
		break;
	}

	return (NULL);
}

/*
 * Classify isa's as to bitness of the corresponding ABIs.
 * isa's which have no "official" Solaris ABI are returned
 * unrecognised i.e. zero bits.
 */
static int
bitness(char *isaname)
{
	if (strcmp(isaname, "sparc") == 0 ||
	    strcmp(isaname, "i386") == 0)
		return (32);

	if (strcmp(isaname, "sparcv9") == 0 ||
	    strcmp(isaname, "ia64") == 0)
		return (64);

	return (0);
}

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

#define	BITS_MODE	0x1
#define	NATIVE_MODE	0x2
#define	KERN_MODE	0x4

int
main(int argc, char *argv[])
{
	int mode = 0;
	int verbose = 0, errflg = 0;
	int bits = 0;
	int c;
	char *list, *vfmt, *isa;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if ((pgmname = strrchr(*argv, '/')) == 0)
		pgmname = argv[0];
	else
		pgmname++;

	while ((c = getopt(argc, argv, "nbkv")) != EOF)
		switch (c) {
		case 'n':
			if (mode != 0)
				errflg++;
			mode = NATIVE_MODE;
			break;
		case 'b':
			if (mode != 0)
				errflg++;
			mode = BITS_MODE;
			break;
		case 'k':
			if (mode != 0)
				errflg++;
			mode = KERN_MODE;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			errflg++;
			break;
		}

	if (errflg || optind != argc) {
		(void) fprintf(stderr,
		    gettext("usage: %s [-v] [-b | -n | -k]\n"), pgmname);
		return (1);
	}

	if (mode == KERN_MODE) {
		list = kisalist();
		vfmt = gettext("%d-bit %s kernel modules");
	} else {
		list = isalist();
		vfmt = gettext("%d-bit %s applications");
	}

	if (list == NULL) {
		(void) fprintf(stderr, gettext("%s: unable to find isa(s)\n"),
		    pgmname);
		exit(2);
	}

	/*
	 * Find first "interesting" element in the isalist
	 */
	for (isa = strtok(list, " "); isa; isa = strtok(0, " "))
		if ((bits = bitness(isa)) != 0)
			break;	/* ignore "extension" architectures */

	if (isa == 0 || bits == 0) {
		(void) fprintf(stderr, gettext("%s: unable to find isa(s)!\n"),
		    pgmname);
		exit(3);
	}

	do {
		if (verbose)
			(void) printf(vfmt, bits, isa);
		else if (mode == BITS_MODE)
			(void) printf("%d", bits);
		else
			(void) printf("%s", isa);

		if (mode == NATIVE_MODE || mode == BITS_MODE) {
			(void) putchar('\n');
			break;
		}

		/*
		 * Find next "interesting" element in the isalist
		 */
		while (isa = strtok(0, " "))
			if ((bits = bitness(isa)) != 0)
				break;

		if (verbose || isa == NULL || bits == 0)
			(void) putchar('\n');
		else
			(void) putchar(' ');

	} while (isa);

	return (0);
}
