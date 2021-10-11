/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1996 Sun Microsystems, Inc	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)file.c	1.46	99/05/04 SMI"

#define	_LARGEFILE64_SOURCE

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <libelf.h>
#include <stdlib.h>
#include <limits.h>
#include <locale.h>
#include <wctype.h>
#include <string.h>
#include <errno.h>
#include <door.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/elf.h>
#include <sys/elf_M32.h>
#include <sys/elf_SPARC.h>
#include <procfs.h>
#include <sys/core.h>
#include <sys/dumphdr.h>
#include <gelf.h>
#include "libcmd.h"

typedef Elf64_Nhdr	GElf_Nhdr;

/*
 *	Misc
 */

#define	FBSZ	512

/* Assembly language comment char */
#ifdef pdp11
#define	ASCOMCHAR '/'
#else
#define	ASCOMCHAR '!'
#endif

#pragma	align	16(fbuf)
static char	fbuf[FBSZ];

static char	*mfile = NULL;
static char	*troff[] = {	/* new troff intermediate lang */
		"x", "T", "res", "init", "font", "202", "V0", "p1", 0};

static char	*fort[] = {			/* FORTRAN */
		"function", "subroutine", "common", "dimension", "block",
		"integer", "real", "data", "double",
		"FUNCTION", "SUBROUTINE", "COMMON", "DIMENSION", "BLOCK",
		"INTEGER", "REAL", "DATA", "DOUBLE", 0};

static char	*asc[] = {		/* Assembler Commands */
		"sys", "mov", "tst", "clr", "jmp", "cmp", "set", "inc",
		"dec", 0};

static char	*c[] = {			/* C Language */
		"int", "char", "float", "double", "short", "long", "unsigned",
		"register", "static", "struct", "extern", 0};

static char	*as[] = {	/* Assembler Pseudo Ops, prepended with '.' */
		"globl", "global", "ident", "file", "byte", "even",
		"text", "data", "bss", "comm", 0};

/* start for MB env */
static wchar_t wchar;
static int	length;
static int	IS_ascii;
static int	Max;
/* end for MB env */
static int	i = 0;	/* global index into first 'fbsz' bytes of file */
static int	fbsz;
static int	ifd = -1;
static int	elffd = -1;
static int	tret;
static int	hflg = 0;

static void is_stripped(Elf *elf);
static Elf *is_elf_file(int elffd);
static void ar_coff_or_aout(int ifd);
static int type(char *file);
static int troffint(char *bp, int n);
static int lookup(char **tab);
static int ccom(void);
static int ascom(void);
static int sccs(void);
static int english(char *bp, int n);
static int old_core(Elf *elf, GElf_Ehdr *ehdr, int format);
static int core(Elf *elf, GElf_Ehdr *ehdr, int format);
static int shellscript(char buf[], struct stat64 *sb);
static int elf_check(Elf *elf);
static int get_door_target(char *, char *);
static int zipfile(char *, int);
static int is_crash_dump(const char *, int);
static void usage();

#define	prf(x)	(void) printf("%s:%s", x, (int)strlen(x) > 6 ? "\t" : "\t\t");

int
main(int argc, char **argv)
{
	register char	*p;
	register int	ch;
	register FILE	*fl;
	register int	cflg = 0, eflg = 0, fflg = 0;
	char	*ap = NULL;
	int	pathlen;
	struct stat	statbuf;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((ch = getopt(argc, argv, "chf:m:")) != EOF) {
		switch (ch) {
		case 'c':
			cflg++;
			break;

		case 'f':
			fflg++;
			if ((fl = fopen(optarg, "r")) == NULL) {
				(void) fprintf(stderr,
					gettext("cannot open %s\n"), optarg);
				usage();
			}
			pathlen = pathconf("/", _PC_PATH_MAX);
			if (pathlen == -1) {
				(void) fprintf(stderr,
				    gettext("pathconf: cannot determine "
					"maximum path length\n"));
				exit(1);
			}
			pathlen += 2; /* for null and newline in fgets */
			ap = malloc(pathlen * sizeof (char));
			if (ap == NULL) {
				perror("malloc");
				exit(1);
			}
			break;

		case 'm':
			mfile = optarg;
			break;

		case 'h':
			hflg++;
			break;

		case '?':
			eflg++;
			break;
		}
	}
	if (!cflg && !fflg && (eflg || optind == argc))
		usage();

	if (mfile == NULL) { /* -m is not used */
		const char *s1 = "/usr/lib/locale/";
		const char *msg_locale = setlocale(LC_MESSAGES, NULL);
		const char *s3 = "/LC_MESSAGES/magic";

		mfile = malloc(strlen(s1)+strlen(msg_locale)+strlen(s3)+1);
		(void) strcpy(mfile, s1);
		(void) strcat(mfile, msg_locale);
		(void) strcat(mfile, s3);
		if (stat(mfile, &statbuf) != 0)
			(void) strcpy(mfile, "/etc/magic"); /* use /etc/magic */
	}

	if (mkmtab(mfile, cflg) == -1)
		exit(2);
	if (cflg) {
		prtmtab();
		exit(0);
	}
	for (; fflg || optind < argc; optind += !fflg) {
		register int	l;

		if (fflg) {
			if ((p = fgets(ap, pathlen, fl)) == NULL) {
				fflg = 0;
				optind--;
				continue;
			}
			l = strlen(p);
			if (l > 0)
				p[l - 1] = '\0';
		} else
			p = argv[optind];
		prf(p);				/* print "file_name:<tab>" */

		if (type(p))
			tret = 1;
	}
	if (ap != NULL)
		free(ap);
	if (tret != 0) {
		exit(tret);
	}
	return (0);
}

static int
type(char *file)
{
	int	j, nl;
	int	cc;
	char	ch;
	char	buf[BUFSIZ];
	struct	stat64	mbuf;
	int	(*statf)() = hflg ? lstat64 : stat64;
	Elf	*elf;
	int	len;

	i = 0;		/* reset index to beginning of file */
	ifd = -1;
	if ((*statf)(file, &mbuf) < 0) {
		if (statf == lstat64 || lstat64(file, &mbuf) < 0) {
			(void) printf(gettext("cannot open: %s\n"),
			    strerror(errno));
			return (0);		/* POSIX.2 */
		}
	}
	switch (mbuf.st_mode & S_IFMT) {
	case S_IFCHR:
		(void) printf(gettext("character"));
		goto spcl;

	case S_IFDIR:
		(void) printf(gettext("directory\n"));
		return (0);

	case S_IFIFO:
		(void) printf(gettext("fifo\n"));
		return (0);

	case S_IFNAM:
		switch (mbuf.st_rdev) {
		case S_INSEM:
			(void) printf(gettext("Xenix semaphore\n"));
			return (0);
		case S_INSHD:
			(void) printf(gettext("Xenix shared memory handle\n"));
			return (0);
		default:
			(void) printf(gettext("unknown Xenix name "
			    "special file\n"));
			return (0);
		}

	case S_IFLNK:
		if ((cc = readlink(file, buf, BUFSIZ)) < 0) {
			(void) printf(gettext("readlink error: %s\n"),
				strerror(errno));
			return (1);
		}
		buf[cc] = '\0';
		(void) printf(gettext("symbolic link to %s\n"), buf);
		return (0);

	case S_IFBLK:
		(void) printf(gettext("block"));
					/* major and minor, see sys/mkdev.h */
spcl:
		(void) printf(gettext(" special (%d/%d)\n"),
		    major(mbuf.st_rdev), minor(mbuf.st_rdev));
		return (0);

	case S_IFSOCK:
		(void) printf("socket\n");
		/* FIXME, should open and try to getsockname. */
		return (0);

	case S_IFDOOR:
		if (get_door_target(file, buf) == 0)
			(void) printf(gettext("door to %s\n"), buf);
		else
			(void) printf(gettext("door\n"));
		return (0);

	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) printf(gettext("libelf is out of date\n"));
		return (1);
	}

	ifd = open64(file, O_RDONLY);
	if (ifd < 0) {
		(void) printf(gettext("cannot open: %s\n"), strerror(errno));
		return (0);			/* POSIX.2 */
	}

	/* need another fd for elf, since we might want to read the file too */
	elffd = open64(file, O_RDONLY);
	if (elffd < 0) {
		(void) printf(gettext("cannot open: %s\n"), strerror(errno));
		(void) close(ifd);
		ifd = -1;
		return (0);			/* POSIX.2 */
	}
	if ((fbsz = read(ifd, fbuf, FBSZ)) == -1) {
		(void) printf(gettext("cannot read: %s\n"), strerror(errno));
		(void) close(ifd);
		ifd = -1;
		return (0);			/* POSIX.2 */
	}
	if (fbsz == 0) {
		(void) printf(gettext("empty file\n"));
		goto out;
	}
	if (sccs()) {	/* look for "1hddddd" where d is a digit */
		(void) printf("sccs \n");
		goto out;
	}
	if (fbuf[0] == '#' && fbuf[1] == '!' && shellscript(fbuf+2, &mbuf))
		goto out;
	if ((elf = is_elf_file(elffd)) != NULL) {
		(void) elf_check(elf);
		(void) elf_end(elf);
		(void) putchar('\n');
		goto out;
	} else if (*(int *)fbuf == CORE_MAGIC) {
		(void) printf("a.out core file");
		if (*((struct core *)fbuf)->c_cmdname != '\0')
			(void) printf(" from '%s'",
			    ((struct core *)fbuf)->c_cmdname);
		(void) putchar('\n');
		goto out;
	}

	/*
	 * ZIP files, JAR files, and Java executables
	 */
	if (zipfile(fbuf, ifd))
		goto out;

	if (is_crash_dump(fbuf, ifd))
		goto out;

	switch (ckmtab(fbuf, fbsz, 0)) { /* ChecK against Magic Table entries */
		case -1:	/* Error */
			exit(2);
			break;
		case 0:		/* Not magic */
			break;
		default:	/* Switch is magic index */

			/*
			 * ckmtab recognizes file type,
			 * check if it is PostScript.
			 * if not, check if elf or a.out
			 */
			if (fbuf[0] == '%' && fbuf[1] == '!') {
				(void) putchar('\n');
				goto out;
			} else {

				/*
				 * Check that the file is executable (dynamic
				 * objects must be executable to be exec'ed,
				 * shared objects need not be, but by convention
				 * should be executable).
				 *
				 * Note that we should already have processed
				 * the file if it was an ELF file.
				 */
				ar_coff_or_aout(elffd);
				(void) putchar('\n');
				goto out;
			}
	}
	if (ccom() == 0)
		goto notc;
	while (fbuf[i] == '#') {
		j = i;
		while (fbuf[i++] != '\n') {
			if (i - j > 255) {
				(void) printf(gettext("data\n"));
				goto out;
			}
			if (i >= fbsz)
				goto notc;
		}
		if (ccom() == 0)
			goto notc;
	}
check:
	if (lookup(c) == 1) {
		while ((ch = fbuf[i]) != ';' && ch != '{') {
			if ((len = mblen(&fbuf[i], MB_CUR_MAX)) <= 0)
				len = 1;
			i += len;
			if (i >= fbsz)
				goto notc;
		}
		(void) printf(gettext("c program text"));
		goto outa;
	}
	nl = 0;
	while (fbuf[i] != '(') {
		if (fbuf[i] <= 0)
			goto notas;
		if (fbuf[i] == ';') {
			i++;
			goto check;
		}
		if (fbuf[i++] == '\n')
			if (nl++ > 6)
				goto notc;
		if (i >= fbsz)
			goto notc;
	}
	while (fbuf[i] != ')') {
		if (fbuf[i++] == '\n')
			if (nl++ > 6)
				goto notc;
		if (i >= fbsz)
			goto notc;
	}
	while (fbuf[i] != '{') {
		if ((len = mblen(&fbuf[i], MB_CUR_MAX)) <= 0)
			len = 1;
		if (fbuf[i] == '\n')
			if (nl++ > 6)
				goto notc;
		i += len;
		if (i >= fbsz)
			goto notc;
	}
	(void) printf(gettext("c program text"));
	goto outa;
notc:
	i = 0;			/* reset to begining of file again */
	while (fbuf[i] == 'c' || fbuf[i] == 'C'|| fbuf[i] == '!' ||
	    fbuf[i] == '*' || fbuf[i] == '\n') {
		while (fbuf[i++] != '\n')
			if (i >= fbsz)
				goto notfort;
	}
	if (lookup(fort) == 1) {
		(void) printf(gettext("fortran program text"));
		goto outa;
	}
notfort:			/* looking for assembler program */
	i = 0;			/* reset to beginning of file again */
	if (ccom() == 0)	/* assembler programs may contain */
				/* c-style comments */
		goto notas;
	if (ascom() == 0)
		goto notas;
	j = i - 1;
	if (fbuf[i] == '.') {
		i++;
		if (lookup(as) == 1) {
			(void) printf(gettext("assembler program text"));
			goto outa;
		} else if (j != -1 && fbuf[j] == '\n' && isalpha(fbuf[j + 2])) {
			(void) printf(
			    gettext("[nt]roff, tbl, or eqn input text"));
			goto outa;
		}
	}
	while (lookup(asc) == 0) {
		if (ccom() == 0)
			goto notas;
		if (ascom() == 0)
			goto notas;
		while (fbuf[i] != '\n' && fbuf[i++] != ':') {
			if (i >= fbsz)
				goto notas;
		}
		while (fbuf[i] == '\n' || fbuf[i] == ' ' || fbuf[i] == '\t')
			if (i++ >= fbsz)
				goto notas;
		j = i - 1;
		if (fbuf[i] == '.') {
			i++;
			if (lookup(as) == 1) {
				(void) printf(
				    gettext("assembler program text"));
				goto outa;
			} else if (fbuf[j] == '\n' && isalpha(fbuf[j+2])) {
				(void) printf(
				    gettext("[nt]roff, tbl, or eqn input "
				    "text"));
				goto outa;
			}
		}
	}
	(void) printf(gettext("assembler program text"));
	goto outa;
notas:
	/* start modification for multibyte env */
	IS_ascii = 1;
	if (fbsz < FBSZ)
		Max = fbsz;
	else
		Max = FBSZ - MB_LEN_MAX; /* prevent cut of wchar read */
	/* end modification for multibyte env */

	for (i = 0; i < Max; /* null */)
		if (fbuf[i] & 0200) {
			IS_ascii = 0;
			if (fbuf[0] == '\100' && fbuf[1] == '\357') {
				(void) printf(gettext("troff output\n"));
				goto out;
			}
		/* start modification for multibyte env */
			if ((length = mbtowc(&wchar, &fbuf[i], MB_CUR_MAX))
			    <= 0 || !iswprint(wchar)) {
				(void) printf(gettext("data\n"));
				goto out;
			}
			i += length;
		}
		else
			i++;
	i = fbsz;
		/* end modification for multibyte env */
	if (mbuf.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH))
		(void) printf(gettext("commands text"));
	else if (troffint(fbuf, fbsz))
		(void) printf(gettext("troff intermediate output text"));
	else if (english(fbuf, fbsz))
		(void) printf(gettext("English text"));
	else if (IS_ascii)
		(void) printf(gettext("ascii text"));
	else
		(void) printf(gettext("text")); /* for multibyte env */
outa:
	/*
	 * This code is to make sure that no MB char is cut in half
	 * while still being used.
	 */
	fbsz = (fbsz < FBSZ ? fbsz : fbsz - MB_CUR_MAX + 1);
	while (i < fbsz) {
		if (isascii(fbuf[i])) {
			i++;
			continue;
		} else {
			if ((length = mbtowc(&wchar, &fbuf[i], MB_CUR_MAX))
			    <= 0 || !iswprint(wchar)) {
				(void) printf(gettext(" with garbage\n"));
				goto out;
			}
			i = i + length;
		}
	}
	(void) printf("\n");
out:
	if (ifd != -1) {
		(void) close(ifd);
		ifd = -1;
	}
	if (elffd != -1) {
		(void) close(elffd);
		elffd = -1;
	}
	return (0);
}

static int
troffint(char *bp, int n)
{
	int k;

	i = 0;
	for (k = 0; k < 6; k++) {
		if (lookup(troff) == 0)
			return (0);
		if (lookup(troff) == 0)
			return (0);
		while (i < n && bp[i] != '\n')
			i++;
		if (i++ >= n)
			return (0);
	}
	return (1);
}

/*
 * Determine if the passed descriptor describes an ELF file.
 * If so, return the Elf handle.
 */
static Elf *
is_elf_file(int elffd)
{
	Elf *elf;

	elf = elf_begin(elffd, ELF_C_READ, (Elf *)0);
	switch (elf_kind(elf)) {
	case ELF_K_ELF:
		break;
	default:
		(void) elf_end(elf);
		elf = NULL;
		break;
	}
	return (elf);
}

static void
ar_coff_or_aout(int elffd)
{
	Elf *elf;

	/*
	 * Get the files elf descriptor and process it as an elf or
	 * a.out (4.x) file.
	 */

	elf = elf_begin(elffd, ELF_C_READ, (Elf *)0);
	switch (elf_kind(elf)) {
		case ELF_K_AR :
			(void) printf(gettext(", not a dynamic executable "
			    "or shared object"));
			break;
		case ELF_K_COFF:
			(void) printf(gettext(", unsupported or unknown "
			    "file type"));
			break;
		default:
			/*
			 * This is either an unknown file or an aout format
			 * At this time, we don't print dynamic/stripped
			 * info. on a.out or non-Elf binaries.
			 */
			break;
	}
	(void) elf_end(elf);
}


static void
print_elf_type(Elf *elf, GElf_Ehdr *ehdr, int format)
{
	switch (ehdr->e_type) {
	case ET_NONE:
		(void) printf(" %s", gettext("unknown type"));
		break;
	case ET_REL:
		(void) printf(" %s", gettext("relocatable"));
		break;
	case ET_EXEC:
		(void) printf(" %s", gettext("executable"));
		break;
	case ET_DYN:
		(void) printf(" %s", gettext("dynamic lib"));
		break;
	case ET_CORE:
		if (old_core(elf, ehdr, format))
			(void) printf(" %s", gettext("pre-2.6 core file"));
		else
			(void) printf(" %s", gettext("core file"));
		break;
	default:
		break;
	}
}

static void
print_elf_machine(int machine)
{
	switch (machine) {
	case EM_NONE:
		(void) printf(" %s", gettext("unknown machine"));
		break;
	case EM_M32:
		(void) printf(" %s", gettext("WE32100"));
		break;
	case EM_SPARC:
		(void) printf(" %s", gettext("SPARC"));
		break;
	case EM_386:
		(void) printf(" %s", gettext("80386"));
		break;
	case EM_68K:
		(void) printf(" %s", gettext("M68000"));
		break;
	case EM_88K:
		(void) printf(" %s", gettext("M88000"));
		break;
	case EM_486:
		(void) printf(" %s", gettext("80486"));
		break;
	case EM_860:
		(void) printf(" %s", gettext("i860"));
		break;
	case EM_MIPS:
		(void) printf(" %s", gettext("MIPS RS3000 Big-Endian"));
		break;
	case EM_MIPS_RS3_LE:
		(void) printf(" %s", gettext("MIPS RS3000 Little-Endian"));
		break;
	case EM_RS6000:
		(void) printf(" %s", gettext("MIPS RS6000"));
		break;
	case EM_PA_RISC:
		(void) printf(" %s", gettext("PA-RISC"));
		break;
	case EM_nCUBE:
		(void) printf(" %s", gettext("nCUBE"));
		break;
	case EM_VPP500:
		(void) printf(" %s", gettext("VPP500"));
		break;
	case EM_SPARC32PLUS:
		(void) printf(" %s", gettext("SPARC32PLUS"));
		break;
	case EM_PPC:
		(void) printf(" %s", gettext("PowerPC"));
		break;
	case EM_SPARCV9:
		(void) printf(" %s", gettext("SPARCV9"));
		break;
	case EM_IA_64:
		(void) printf(" %s", gettext("IA64"));
		break;
	default:
		break;
	}
}

static void
print_elf_datatype(int datatype)
{
	switch (datatype) {
	case ELFDATA2LSB:
		(void) printf(" %s", gettext("LSB"));
		break;
	case ELFDATA2MSB:
		(void) printf(" %s", gettext("MSB"));
		break;
	default:
		break;
	}
}

static void
print_elf_class(int class)
{
	switch (class) {
	case ELFCLASS32:
		(void) printf(" %s", gettext("32-bit"));
		break;
	case ELFCLASS64:
		(void) printf(" %s", gettext("64-bit"));
		break;
	default:
		break;
	}
}

static void
print_elf_flags(int machine, unsigned int flags)
{
	switch (machine) {
	case EM_M32:
		if (flags & EF_M32_MAU)
			(void) printf("%s", gettext(", MAU Required"));
		break;
	case EM_SPARCV9:
		if (flags & EF_SPARC_EXT_MASK) {
			if (flags & EF_SPARC_SUN_US1) {
				(void) printf("%s", gettext(
				    ", UltraSPARC1 Extensions Required"));
			}
			if (flags & EF_SPARC_HAL_R1)
				(void) printf("%s", gettext(
				    ", HaL R1 Extensions Required"));
		}
		break;
	case EM_SPARC32PLUS:
		if (flags & EF_SPARC_32PLUS)
			(void) printf("%s", gettext(", V8+ Required"));
		if (flags & EF_SPARC_SUN_US1) {
			(void) printf("%s",
				gettext(", UltraSPARC1 Extensions Required"));
		}
		if (flags & EF_SPARC_HAL_R1)
			(void) printf("%s",
				gettext(", HaL R1 Extensions Required"));
		break;
	default:
		break;
	}
}

static int
elf_check(Elf *elf)
{
	GElf_Ehdr	ehdr;
	GElf_Phdr	phdr;
	int		dynamic, cnt;
	char	*ident;
	size_t	size;

	/*
	 * verify information in file header
	 */
	if (gelf_getehdr(elf, &ehdr) == (GElf_Ehdr *)0) {
		(void) fprintf(stderr, gettext("can't read ELF header\n"));
		return (1);
	}
	ident = elf_getident(elf, &size);
	(void) printf("%s", gettext("ELF"));
	print_elf_class(ident[EI_CLASS]);
	print_elf_datatype(ident[EI_DATA]);
	print_elf_type(elf, &ehdr, ident[EI_DATA]);
	print_elf_machine(ehdr.e_machine);
	if (ehdr.e_version == 1)
		(void) printf(" %s %d",
		    gettext("Version"), (int)ehdr.e_version);
	print_elf_flags(ehdr.e_machine, ehdr.e_flags);

	if (core(elf, &ehdr, ident[EI_DATA]))	/* check for core file */
		return (0);

	/*
	 * check type
	 */
	if ((ehdr.e_type != ET_EXEC) && (ehdr.e_type != ET_DYN))
		return (1);

	/*
	 * read program header and check for dynamic section
	 */
	if (ehdr.e_phnum == 0) {
		(void) fprintf(stderr, gettext("can't read program header\n"));
		return (1);
	}

	for (dynamic = 0, cnt = 0; cnt < (int)ehdr.e_phnum; cnt++) {
		if (gelf_getphdr(elf, cnt, &phdr) == NULL) {
			(void) fprintf(stderr,
				gettext("can't read program header\n"));
			return (1);
		}
		if (phdr.p_type == PT_DYNAMIC) {
			dynamic = 1;
			break;
		}
	}
	if (dynamic)
		(void) printf(gettext(", dynamically linked"));
	else
		(void) printf(gettext(", statically linked"));

	is_stripped(elf);
	return (0);
}

/*
 * is_stripped prints information on whether the executable has
 * been stripped.
 */
static void
is_stripped(Elf *elf)
{
	GElf_Shdr	shdr;
	int		flag;
	Elf_Scn		*scn, *nextscn;


	/*
		If the Symbol Table exists, the executable file has not
		been stripped.
	*/
	flag = 0;
	scn = NULL;
	while ((nextscn = elf_nextscn(elf, scn)) != NULL) {
		scn = nextscn;
		if (gelf_getshdr(scn, &shdr) == NULL) {
			return;
		}
		if (shdr.sh_type == SHT_SYMTAB) {
			flag = 1;
			break;
		}
	}

	if (flag)
		(void) printf(gettext(", not stripped"));
	else
		(void) printf(gettext(", stripped"));
}

/*
 * lookup -
 * Attempts to match one of the strings from a list, 'tab',
 * with what is in the file, starting at the current index position 'i'.
 * Looks past any initial whitespace and expects whitespace or other
 * delimiting characters to follow the matched string.
 * A match identifies the file as being 'assembler', 'fortran', 'c', etc.
 * Returns 1 for a successful match, 0 otherwise.
 */
static int
lookup(char **tab)
{
	register char	r;
	register int	k, j, l;

	while (fbuf[i] == ' ' || fbuf[i] == '\t' || fbuf[i] == '\n')
		i++;
	for (j = 0; tab[j] != 0; j++) {
		l = 0;
		for (k = i; ((r = tab[j][l++]) == fbuf[k] && r != '\0'); k++);
		if (r == '\0')
			if (fbuf[k] == ' ' || fbuf[k] == '\n' ||
			    fbuf[k] == '\t' || fbuf[k] == '{' ||
			    fbuf[k] == '/') {
				i = k;
				return (1);
			}
	}
	return (0);
}

/*
 * ccom -
 * Increments the current index 'i' into the file buffer 'fbuf' past any
 * whitespace lines and C-style comments found, starting at the current
 * position of 'i'.  Returns 1 as long as we don't increment i past the
 * size of fbuf (fbsz).  Otherwise, returns 0.
 */

static int
ccom(void)
{
	register char	cc;
	int		len;

	while ((cc = fbuf[i]) == ' ' || cc == '\t' || cc == '\n')
		if (i++ >= fbsz)
			return (0);
	if (fbuf[i] == '/' && fbuf[i+1] == '*') {
		i += 2;
		while (fbuf[i] != '*' || fbuf[i+1] != '/') {
			if (fbuf[i] == '\\')
				i++;
			if ((len = mblen(&fbuf[i], MB_CUR_MAX)) <= 0)
				len = 1;
			i += len;
			if (i >= fbsz)
				return (0);
		}
		if ((i += 2) >= fbsz)
			return (0);
	}
	if (fbuf[i] == '\n')
		if (ccom() == 0)
			return (0);
	return (1);
}

/*
 * ascom -
 * Increments the current index 'i' into the file buffer 'fbuf' past
 * consecutive assembler program comment lines starting with ASCOMCHAR,
 * starting at the current position of 'i'.
 * Returns 1 as long as we don't increment i past the
 * size of fbuf (fbsz).  Otherwise returns 0.
 */

static int
ascom(void)
{
	while (fbuf[i] == ASCOMCHAR) {
		i++;
		while (fbuf[i++] != '\n')
			if (i >= fbsz)
				return (0);
		while (fbuf[i] == '\n')
			if (i++ >= fbsz)
				return (0);
	}
	return (1);
}

static int
sccs(void)
{				/* look for "1hddddd" where d is a digit */
	register int j;

	if (fbuf[0] == 1 && fbuf[1] == 'h') {
		for (j = 2; j <= 6; j++) {
			if (isdigit(fbuf[j]))
				continue;
			else
				return (0);
		}
	} else {
		return (0);
	}
	return (1);
}

static int
english(char *bp, int n)
{
#define	NASC 128		/* number of ascii char ?? */
	register int	j, vow, freq, rare, len;
	register int	badpun = 0, punct = 0;
	int	ct[NASC];

	if (n < 50)
		return (0); /* no point in statistics on squibs */
	for (j = 0; j < NASC; j++)
		ct[j] = 0;
	for (j = 0; j < n; j += len) {
		if ((unsigned char)bp[j] < NASC)
			ct[bp[j]|040]++;
		switch (bp[j]) {
		case '.':
		case ',':
		case ')':
		case '%':
		case ';':
		case ':':
		case '?':
			punct++;
			if (j < n-1 && bp[j+1] != ' ' && bp[j+1] != '\n')
				badpun++;
		}
		if ((len = mblen(&bp[j], MB_CUR_MAX)) <= 0)
			len = 1;
	}
	if (badpun*5 > punct)
		return (0);
	vow = ct['a'] + ct['e'] + ct['i'] + ct['o'] + ct['u'];
	freq = ct['e'] + ct['t'] + ct['a'] + ct['i'] + ct['o'] + ct['n'];
	rare = ct['v'] + ct['j'] + ct['k'] + ct['q'] + ct['x'] + ct['z'];
	if (2*ct[';'] > ct['e'])
		return (0);
	if ((ct['>'] + ct['<'] + ct['/']) > ct['e'])
		return (0);	/* shell file test */
	return (vow * 5 >= n - ct[' '] && freq >= 10 * rare);
}

/*
 * Convert a word from an elf file to native format.
 * This is needed because there's no elf routine to
 * get and decode a Note section header.
 */
static void
convert_gelf_word(Elf *elf, GElf_Word *data, int version, int format)
{
	Elf_Data src, dst;

	dst.d_buf = data;
	dst.d_version = version;
	dst.d_size = sizeof (GElf_Word);
	dst.d_type = ELF_T_WORD;
	src.d_buf = data;
	src.d_version = version;
	src.d_size = sizeof (GElf_Word);
	src.d_type = ELF_T_WORD;
	(void) gelf_xlatetom(elf, &dst, &src, format);
}

static void
convert_gelf_nhdr(Elf *elf, GElf_Nhdr *nhdr, GElf_Word version, int format)
{
	convert_gelf_word(elf, &nhdr->n_namesz, version, format);
	convert_gelf_word(elf, &nhdr->n_descsz, version, format);
	convert_gelf_word(elf, &nhdr->n_type, version, format);
}

/*
 * Return true if it is an old (pre-restructured /proc) core file.
 */
static int
old_core(Elf *elf, GElf_Ehdr *ehdr, int format)
{
	register int inx;
	GElf_Phdr phdr;
	GElf_Phdr nphdr;
	GElf_Nhdr nhdr;
	off_t offset;

	if (ehdr->e_type != ET_CORE)
		return (0);
	for (inx = 0; inx < (int)ehdr->e_phnum; inx++) {
		if (gelf_getphdr(elf, inx, &phdr) == NULL) {
			return (0);
		}
		if (phdr.p_type == PT_NOTE) {
			/*
			 * If the next segment is also a note, use it instead.
			 */
			if (gelf_getphdr(elf, inx+1, &nphdr) == NULL) {
				return (0);
			}
			if (nphdr.p_type == PT_NOTE)
				phdr = nphdr;
			offset = (off_t)phdr.p_offset;
			(void) pread(ifd, &nhdr, sizeof (GElf_Nhdr), offset);
			convert_gelf_nhdr(elf, &nhdr, ehdr->e_version, format);
			/*
			 * Old core files have type NT_PRPSINFO.
			 */
			if (nhdr.n_type == NT_PRPSINFO)
				return (1);
			return (0);
		}
	}
	return (0);
}

/*
 * If it's a core file, print out the name of the file that dumped core.
 */
static int
core(Elf *elf, GElf_Ehdr *ehdr, int format)
{
	register int inx;
	char *psinfo;
	GElf_Phdr phdr;
	GElf_Phdr nphdr;
	GElf_Nhdr nhdr;
	off_t offset;

	if (ehdr->e_type != ET_CORE)
		return (0);
	for (inx = 0; inx < (int)ehdr->e_phnum; inx++) {
		if (gelf_getphdr(elf, inx, &phdr) == NULL) {
			(void) fprintf(stderr,
				gettext("can't read program header\n"));
			return (0);
		}
		if (phdr.p_type == PT_NOTE) {
			char *fname;
			size_t size;
			/*
			 * If the next segment is also a note, use it instead.
			 */
			if (gelf_getphdr(elf, inx+1, &nphdr) == NULL) {
				(void) fprintf(stderr,
				    gettext("can't read program header\n"));
				return (0);
			}
			if (nphdr.p_type == PT_NOTE)
				phdr = nphdr;
			offset = (off_t)phdr.p_offset;
			(void) pread(ifd, &nhdr, sizeof (GElf_Nhdr), offset);
			convert_gelf_nhdr(elf, &nhdr, ehdr->e_version, format);
			/*
			 * Note: the ABI states that n_namesz must
			 * be rounded up to a 4 byte boundary.
			 */
			offset += sizeof (GElf_Nhdr) +
			    ((nhdr.n_namesz + 0x03) & ~0x3);
			size = nhdr.n_descsz;
			psinfo = malloc(size);
			(void) pread(ifd, psinfo, size, offset);
			/*
			 * We want to print the string contained
			 * in psinfo->pr_fname[], where 'psinfo'
			 * is either an old NT_PRPSINFO structure
			 * or a new NT_PSINFO structure.
			 *
			 * Old core files have only type NT_PRPSINFO.
			 * New core files have type NT_PSINFO.
			 *
			 * These structures are also different by
			 * virtue of being contained in a core file
			 * of either 32-bit or 64-bit type.
			 *
			 * To further complicate matters, we ourself
			 * might be compiled either 32-bit or 64-bit.
			 *
			 * For these reason, we just *know* the offsets of
			 * pr_fname[] into the four different structures
			 * here, regardless of how we are compiled.
			 */
			if (gelf_getclass(elf) == ELFCLASS32) {
				/* 32-bit core file, 32-bit structures */
				if (nhdr.n_type == NT_PSINFO)
					fname = psinfo + 88;
				else	/* old: NT_PRPSINFO */
					fname = psinfo + 84;
			} else if (gelf_getclass(elf) == ELFCLASS64) {
				/* 64-bit core file, 64-bit structures */
				if (nhdr.n_type == NT_PSINFO)
					fname = psinfo + 136;
				else	/* old: NT_PRPSINFO */
					fname = psinfo + 120;
			} else {
				free(psinfo);
				break;
			}
			(void) printf(gettext(", from '%s'"), fname);
			free(psinfo);
			break;
		}
	}
	return (1);
}

static int
shellscript(char buf[], struct stat64 *sb)
{
	register char *tp;
	char *cp, *xp;

	cp = strchr(buf, '\n');
	if (cp == 0 || cp - fbuf > fbsz)
		return (0);
	for (tp = buf; tp != cp && isspace(*tp); tp++)
		if (!isascii(*tp))
			return (0);
	for (xp = tp; tp != cp && !isspace(*tp); tp++)
		if (!isascii(*tp))
			return (0);
	if (tp == xp)
		return (0);
	if (sb->st_mode & S_ISUID)
		(void) printf("set-uid ");
	if (sb->st_mode & S_ISGID)
		(void) printf("set-gid ");
	if (strncmp(xp, "/bin/sh", tp - xp) == 0)
		xp = "shell";
	else if (strncmp(xp, "/bin/csh", tp - xp) == 0)
		xp = "c-shell";
	else
		*tp = '\0';
	(void) printf(gettext("executable %s script\n"), xp);
	return (1);
}

static int
get_door_target(char *file, char *buf)
{
	int fd;
	door_info_t door_info;
	psinfo_t psinfo;

	if ((fd = open64(file, O_RDONLY)) < 0 ||
	    _door_info(fd, &door_info) != 0) {
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	(void) close(fd);

	(void) sprintf(buf, "/proc/%ld/psinfo", door_info.di_target);
	if ((fd = open64(buf, O_RDONLY)) < 0 ||
	    read(fd, &psinfo, sizeof (psinfo)) != sizeof (psinfo)) {
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	(void) close(fd);

	(void) sprintf(buf, "%s[%ld]", psinfo.pr_fname, door_info.di_target);
	return (0);
}

/*
 * ZIP file header information
 */
#define	SIGSIZ		4
#define	LOCSIG		"PK\003\004"
#define	LOCHDRSIZ	30

#define	CH(b, n)	(((unsigned char *)(b))[n])
#define	SH(b, n)	(CH(b, n) | (CH(b, n+1) << 8))
#define	LG(b, n)	(SH(b, n) | (SH(b, n+2) << 16))

#define	LOCNAM(b)	(SH(b, 26))	/* filename size */
#define	LOCEXT(b)	(SH(b, 28))	/* extra field size */

#define	XFHSIZ		4		/* header id, data size */
#define	XFHID(b)	(SH(b, 0))	/* extract field header id */
#define	XFDATASIZ(b)	(SH(b, 2))	/* extract field data size */
#define	XFJAVASIG	0xcafe		/* java executables */

static int
zipfile(char *fbuf, int fd)
{
	off_t xoff, xoff_end;

	if (strncmp(fbuf, LOCSIG, SIGSIZ) != 0)
		return (0);

	xoff = LOCHDRSIZ + LOCNAM(fbuf);
	xoff_end = xoff + LOCEXT(fbuf);

	while (xoff < xoff_end) {
		char xfhdr[XFHSIZ];

		if (pread(fd, xfhdr, XFHSIZ, xoff) != XFHSIZ)
			break;

		if (XFHID(xfhdr) == XFJAVASIG) {
			(void) printf("%s\n", gettext("java program"));
			return (1);
		}
		xoff += sizeof (xfhdr) + XFDATASIZ(xfhdr);
	}

	/*
	 * We could just print "ZIP archive" here.
	 *
	 * However, customers may be using their own entries in
	 * /etc/magic to distinguish one kind of ZIP file from another, so
	 * let's defer the printing of "ZIP archive" to there.
	 */
	return (0);
}

static int
is_crash_dump(const char *buf, int fd)
{
	const dumphdr_t *dhp = (const dumphdr_t *)buf;
	dumphdr_t dh; /* Exceeds FBSZ bytes in size */

	/*
	 * The current DUMP_MAGIC string covers Solaris 7 and later releases.
	 * The utsname struct is only present in dumphdr_t's with dump_version
	 * greater than or equal to 9.
	 */
	if (dhp->dump_magic == DUMP_MAGIC) {
		if (dhp->dump_version > 8 && pread(fd, &dh, sizeof (dumphdr_t),
		    (off_t)0) == sizeof (dumphdr_t)) {
			(void) printf(
			    gettext("%s %s %s %u-bit crash dump from '%s'\n"),
			    dh.dump_utsname.sysname, dh.dump_utsname.release,
			    dh.dump_utsname.version, dh.dump_wordsize,
			    dh.dump_utsname.nodename);
		} else {
			(void) printf(gettext("SunOS %u-bit crash dump\n"),
			    dhp->dump_wordsize);
		}

		return (1);
	}

	/*
	 * The 0x8FCA0102 magic string covered all previous versions of the
	 * Solaris crash dump format; we don't bother to distinguish them.
	 */
	if (dhp->dump_magic == 0x8FCA0102) {
		(void) printf(gettext("SunOS 32-bit crash dump\n"));
		return (1);
	}

	return (0);
}

static void
usage()
{
	(void) fprintf(stderr, gettext(
		"usage: file [-h] [-m mfile] [-f ffile] file ...\n"
		"       file [-h] [-m mfile] -f ffile\n"
		"       file -c [-m mfile]\n"));
	exit(2);
}
