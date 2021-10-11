/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gcore.c	1.19	99/03/23 SMI"	/* SVr4.0 1.1	*/

/*
 * ******************************************************************
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986, 1987, 1988, 1989  Sun Microsystems,  Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *	          All rights reserved.
 * *******************************************************************
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sysmacros.h>
#include <libproc.h>
#include <sys/elf.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/auxv.h>
#include <sys/machelf.h>
#include "elf_notes.h"

#ifdef _LP64

#define	PR_MODEL_OTHER	PR_MODEL_ILP32
#ifdef sparc
#define	OTHER_MODEL	"sparcv7"
#elif i386
#define	OTHER_MODEL	"i86"
#endif

#else

#define	PR_MODEL_OTHER	PR_MODEL_LP64
#ifdef sparc
#define	OTHER_MODEL	"sparcv9"
#elif i386
#define	OTHER_MODEL	"ia64"
#endif

#endif

static	int	dumpcore(struct ps_prochandle *, pid_t);

static	char	*command = NULL;	/* name of command ("gcore") */
static	char	*filename = "core";	/* default filename prefix */
static	char	*procdir = "/proc";	/* default PROC directory */
static	int	buf[8*1024];	/* big buffer, used for almost everything */

int
main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = FALSE;
	char **xargv;
	char **xav;
	const char *execname;
	char *other_gcore;
	char *str;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/*
	 * We keep track of the argument vector in case we have
	 * to exec() the other gcore somewhere along the way.
	 */
	if ((xargv = malloc((argc + 3) * sizeof (char *))) == NULL) {
		perror("malloc()");
		return (2);
	}
	xav = xargv;
	*xav++ = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "o:")) != EOF) {
		switch (opt) {
		case 'o':		/* filename prefix (default "core") */
			filename = optarg;
			*xav++ = "-o";
			*xav++ = filename;
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr,
			"usage:\t%s [-o filename] pid ...\n",
			command);
		return (2);
	}

	while (--argc >= 0) {
		char *arg;
		pid_t pid;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		/* get the specified pid and its psinfo struct */
		if ((pid = proc_arg_psinfo(arg = *argv++, PR_ARG_PIDS,
		    &psinfo, &gcode)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			retc++;
			continue;
		}

		/*
		 * If the target process's data model does not match our own,
		 * exec the version of gcore that matches the target's data
		 * model, but only if the data model is not PR_MODEL_UNKNOWN
		 * (meaning that the process is a system process).
		 */
		if (psinfo.pr_dmodel == PR_MODEL_OTHER)
			goto exec_other;

		if ((Pr = Pgrab(pid, 0, &gcode)) == NULL) {
			(void) fprintf(stderr, "%s: cannot grab %d: %s\n",
				command, (int)pid, Pgrab_error(gcode));
			retc++;
			continue;
		}

		if (dumpcore(Pr, pid) != 0)
			retc++;

		Prelease(Pr, 0);
	}

	return (retc);

exec_other:
	execname = getexecname();
	other_gcore = (char *)buf;

	(void) strcpy(other_gcore, execname);
	if ((str = strrchr(other_gcore, '/')) == NULL) {
		(void) fprintf(stderr,
			"%s: cannot find %s version of %s\n",
			command, OTHER_MODEL, other_gcore);
		return (2);
	}
	*str = '\0';
	execname += (str - other_gcore);
	if ((str = strrchr(other_gcore, '/')) == NULL)
		str = other_gcore;
	else
		str++;
	(void) strcpy(str, OTHER_MODEL);
	(void) strcat(str, execname);

	*xav++ = *(argv-1);
	while (--argc >= 0)
		*xav++ = *argv++;
	*xav++ = NULL;
	(void) fflush(NULL);
	(void) execv(other_gcore, xargv);
	(void) fprintf(stderr, "%s: cannot exec %s\n",
		command, other_gcore);
	return (2);
}

static ssize_t
safe_write(int fd, const void *buf, size_t n)
{
	ssize_t resid = n;
	ssize_t len;

	while (resid != 0) {
		if ((len = write(fd, buf, resid)) <= 0)
			break;

		resid -= len;
		buf = (char *)buf + len;
	}

	if (resid == n && n != 0)
		return (-1);

	return (n - resid);
}

/* ARGSUSED */
static int
dumpcore(struct ps_prochandle *Pr, pid_t pid)
{
	int asfd = Pasfd(Pr);
	int dfd = -1;		/* dump file descriptor */
	int mfd = -1;		/* /proc/pid/map */
	int nsegments;		/* current number of segments */
	Ehdr ehdr;		/* ELF header */
	Phdr *v = NULL;		/* ELF program header */
	prmap_t *pdp = NULL;
	int nlwp;
	struct stat statb;
	ulong hdrsz;
	off_t poffset;
	int nhdrs, i;
	size_t size;
	size_t count;
	ssize_t ncount;
	char fname[MAXPATHLEN];

	/*
	 * Fetch the memory map and look for text, data, and stack.
	 */
	(void) sprintf(fname, "%s/%d/map", procdir, (int)pid);
	if ((mfd = open(fname, O_RDONLY)) < 0) {
		perror("dumpcore(): open() map");
		goto bad;
	}
	if (fstat(mfd, &statb) != 0 ||
	    (nsegments = statb.st_size / sizeof (prmap_t)) <= 0) {
		perror("dumpcore(): stat() map");
		goto bad;
	}
	if ((pdp = malloc((nsegments+1)*sizeof (prmap_t))) == NULL)
		goto nomalloc;
	if (read(mfd, (char *)pdp, (nsegments+1)*sizeof (prmap_t))
	    != nsegments*sizeof (prmap_t)) {
		perror("dumpcore(): read map");
		goto bad;
	}
	(void) close(mfd);
	mfd = -1;

	if ((nlwp = Pstatus(Pr)->pr_nlwp) == 0)
		nlwp = 1;

	nhdrs = nsegments + 2;		/* two PT_NOTE headers */
	hdrsz = nhdrs * sizeof (Phdr);

	if ((v = malloc(hdrsz)) == NULL)
		goto nomalloc;
	(void) memset(v, 0, hdrsz);

	(void) memset(&ehdr, 0, sizeof (Ehdr));
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
#if defined(__sparcv9)
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
	ehdr.e_machine = EM_SPARCV9;
#elif defined(sparc)
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
	ehdr.e_machine = EM_SPARC;
#elif defined(i386)
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_machine = EM_386;
#endif
	ehdr.e_type = ET_CORE;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_phoff = sizeof (Ehdr);
	ehdr.e_ehsize = sizeof (Ehdr);
	ehdr.e_phentsize = sizeof (Phdr);
	ehdr.e_phnum = (Half)nhdrs;

	/*
	 * Create the core dump file.
	 */
	(void) sprintf(fname, "%s.%d", filename, (int)pid);
	if ((dfd = creat(fname, 0666)) < 0) {
		perror(fname);
		goto bad;
	}

	if (safe_write(dfd, &ehdr, sizeof (Ehdr)) != sizeof (Ehdr)) {
		perror("dumpcore(): write");
		goto bad;
	}

	poffset = sizeof (Ehdr) + hdrsz;

	if (setup_old_note_header(&v[0], nlwp, procdir, pid) != 0)
		goto bad;
	v[0].p_offset = poffset;
	poffset += v[0].p_filesz;

	if (setup_note_header(&v[1], nlwp, procdir, pid) != 0) {
		cancel_old_notes();
		goto bad;
	}
	v[1].p_offset = poffset;
	poffset += v[1].p_filesz;

	for (i = 2; i < nhdrs; i++, pdp++) {
		v[i].p_type = PT_LOAD;
		v[i].p_vaddr = (Addr)pdp->pr_vaddr;
		size = pdp->pr_size;
		v[i].p_memsz = size;
		if (pdp->pr_mflags & MA_WRITE)
			v[i].p_flags |= PF_W;
		if (pdp->pr_mflags & MA_READ)
			v[i].p_flags |= PF_R;
		if (pdp->pr_mflags & MA_EXEC)
			v[i].p_flags |= PF_X;
		if ((pdp->pr_mflags & MA_READ) &&
		    (pdp->pr_mflags & (MA_WRITE|MA_EXEC)) != MA_EXEC) {
			/*
			 * Don't dump a shared memory mapping
			 * that has an underlying mapped file.
			 */
			if (!(pdp->pr_mflags & MA_SHARED) ||
			    pdp->pr_mapname[0] == '\0') {
				v[i].p_offset = poffset;
				v[i].p_filesz = size;
				poffset += size;
			}
		}
	}

	if (safe_write(dfd, v, hdrsz) != hdrsz) {
		perror("dumpcore(): write");
		cancel_notes();
		cancel_old_notes();
		goto bad;
	}

	if (write_old_elfnotes(nlwp, dfd) != 0) {
		cancel_notes();
		goto bad;
	}

	if (write_elfnotes(nlwp, dfd) != 0)
		goto bad;

	/*
	 * Dump data and stack
	 */
	for (i = 2; i < nhdrs; i++) {
		if (v[i].p_filesz == 0)
			continue;
		(void) lseek(asfd, v[i].p_vaddr, 0);
		count = v[i].p_filesz;
		while (count != 0) {
			ncount = (count < sizeof (buf))? count : sizeof (buf);
			if ((ncount = read(asfd, buf, ncount)) <= 0)
				break;
			if (safe_write(dfd, buf, ncount) != ncount) {
				perror("dumpcore(): write");
				goto bad;
			}
			count -= ncount;
		}
	}

	(void) fprintf(stderr, "%s: %s.%d dumped\n",
		command, filename, (int)pid);
	(void) close(dfd);
	free(v);
	return (0);
nomalloc:
	(void) fprintf(stderr, "%s: malloc() failed\n", command);
bad:
	if (mfd >= 0)
		(void) close(mfd);
	if (dfd >= 0)
		(void) close(dfd);
	if (pdp != NULL)
		free(pdp);
	if (v != NULL)
		free(v);
	return (-1);
}


void
elfnote(int dfd, int type, char *ptr, int size)
{
	Note note;		/* ELF note */

	(void) memset(&note, 0, sizeof (Note));
	(void) memcpy(note.name, "CORE", 4);
	note.nhdr.n_type = type;
	note.nhdr.n_namesz = 5;
	note.nhdr.n_descsz = roundup(size, sizeof (Word));
	(void) safe_write(dfd, &note, sizeof (Note));
	(void) safe_write(dfd, ptr, roundup(size, sizeof (Word)));
}
