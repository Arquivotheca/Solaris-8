/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)old_notes.c	1.3	97/12/23 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/regset.h>
#include <sys/old_procfs.h>
#include <sys/elf.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/auxv.h>
#include <sys/machelf.h>
#include "elf_notes.h"

static id_t *lwpid = NULL;

static int has_platname;
static char platform_name[SYS_NMLN];

static int nauxv;
static auxv_t *auxv = NULL;

static int has_fp;

#if defined(sparc) || defined(__sparc)
static int has_gwins;
#endif /* sparc */

static int has_xrs;
static int xregs_size;
static char *xregs = NULL;

static int pfd = -1;

int
setup_old_note_header(Phdr *vp, int nlwp, char *pdir, pid_t pid)
{
	char procname[100];
	prfpregset_t fpregs;

	if ((lwpid = malloc((nlwp+1)*sizeof (id_t))) == NULL)
		goto nomalloc;

	(void) strncpy(procname, pdir, sizeof (procname) - 12);
	(void) sprintf(procname + strlen(procname), "/%d", (int)pid);
	if ((pfd = open(procname, O_RDONLY)) < 0) {
		perror("setup_old_note_header(): open old /proc file");
		return (-1);
	}

	if (ioctl(pfd, PIOCLWPIDS, lwpid) != 0) {
		perror("setup_old_note_header(): PIOCLWPIDS");
		goto bad;
	}

	vp->p_type = PT_NOTE;
	vp->p_flags = PF_R;
	vp->p_filesz = (sizeof (Note) * (1+nlwp)) +
		roundup(sizeof (prpsinfo_t), sizeof (Word)) +
		nlwp * roundup(sizeof (prstatus_t), sizeof (Word));

	if (ioctl(pfd, PIOCNAUXV, &nauxv) != 0) {
		perror("setup_old_note_header(): PIOCNAUXV");
		goto bad;
	}
	if (nauxv > 0) {
		vp->p_filesz += sizeof (Note) +
			roundup(nauxv * sizeof (auxv_t), sizeof (Word));
		if ((auxv = malloc(nauxv * sizeof (auxv_t))) == NULL)
			goto nomalloc;
	}
	has_platname = FALSE;
	if (sysinfo(SI_PLATFORM, platform_name, sizeof (platform_name)) != -1) {
		has_platname = TRUE;
		vp->p_filesz += sizeof (Note) +
			roundup(strlen(platform_name) + 1, sizeof (Word));
	}
	has_fp = FALSE;
	if (ioctl(pfd, PIOCGFPREG, &fpregs) == 0) {
		has_fp = TRUE;
		vp->p_filesz += nlwp*sizeof (Note) +
			nlwp*roundup(sizeof (prfpregset_t),
			sizeof (Word));
	}
	has_xrs = FALSE;
	if ((ioctl(pfd, PIOCGXREGSIZE, &xregs_size) == 0) && (xregs_size > 0)) {
		has_xrs = TRUE;
		vp->p_filesz += nlwp * sizeof (Note) +
			nlwp * roundup(xregs_size, sizeof (Word));
		if ((xregs = malloc(xregs_size)) == NULL)
			goto nomalloc;
	}

#if defined(sparc) || defined(__sparc)
	/*
	 * Need to examine each lwp to see if register windows are present.
	 */
	has_gwins = FALSE;
	while (--nlwp >= 0) {
		int lwpfd;
		gwindows_t gwins;
		int size;

		if ((lwpfd = ioctl(pfd, PIOCOPENLWP, &lwpid[nlwp])) < 0) {
			perror("setup_old_note_header(): PIOCOPENLWP");
			goto bad;
		}

		if (ioctl(lwpfd, PIOCGWIN, &gwins) != 0)
			size = 0;
		else
			size = gwins.wbcnt;
		if (size != 0)
			size = sizeof (gwindows_t) -
			    ((SPARC_MAXREGWINDOW - size)
			    * sizeof (struct rwindow));
		if (size > 0) {
			has_gwins = TRUE;
			vp->p_filesz += sizeof (Note) +
				roundup(size, sizeof (Word));
		}

		(void) close(lwpfd);
	}
#endif /* sparc */

	return (0);

nomalloc:
	(void) fprintf(stderr, "gcore: malloc() failed\n");
bad:
	cancel_old_notes();
	return (-1);
}

int
write_old_elfnotes(int nlwp, int dfd)
{
	int rc = -1;
	int i;
	prstatus_t piocstat;
	prpsinfo_t psstat;
	prfpregset_t fpregs;

	if (ioctl(pfd, PIOCPSINFO, &psstat) != 0) {
		perror("write_old_elfnotes(): PIOCPSINFO");
		goto bad;
	}
	elfnote(dfd, NT_PRPSINFO, (char *)&psstat, sizeof (prpsinfo_t));

	if (has_platname)
		elfnote(dfd, NT_PLATFORM, platform_name,
			strlen(platform_name) + 1);

	if (nauxv > 0) {
		if (ioctl(pfd, PIOCAUXV, auxv) != 0) {
			perror("write_old_elfnotes(): PIOCAUXV");
			goto bad;
		}
		elfnote(dfd, NT_AUXV, (char *)auxv, nauxv * sizeof (auxv_t));
	}

	for (i = 0; i < nlwp; i++) {
		int lwpfd;

		if ((lwpfd = ioctl(pfd, PIOCOPENLWP, &lwpid[i])) < 0) {
			perror("write_old_elfnotes(): PIOCOPENLWP");
			goto bad;
		}

		if (ioctl(lwpfd, PIOCSTATUS, &piocstat) != 0) {
			perror("write_old_elfnotes(): PIOCSTATUS");
			(void) close(lwpfd);
			goto bad;
		}
		elfnote(dfd, NT_PRSTATUS, (char *)&piocstat,
				sizeof (prstatus_t));

		if (has_fp) {
			if (ioctl(lwpfd, PIOCGFPREG, &fpregs) != 0)
				(void) memset(&fpregs, 0,
					sizeof (prfpregset_t));
			elfnote(dfd, NT_PRFPREG, (char *)&fpregs,
				sizeof (prfpregset_t));
		}

#if defined(sparc) || defined(__sparc)
		if (has_gwins) {
			gwindows_t gwins;
			int size;

			if (ioctl(lwpfd, PIOCGWIN, &gwins) != 0)
				size = 0;
			else
				size = gwins.wbcnt;
			if (size != 0)
				size = sizeof (gwindows_t) -
				    ((SPARC_MAXREGWINDOW - size)
				    * sizeof (struct rwindow));
			if (size > 0)
				elfnote(dfd, NT_GWINDOWS, (char *)&gwins, size);
		}
#endif /* sparc */

		if (has_xrs) {
			if (ioctl(lwpfd, PIOCGXREG, xregs) != 0)
				(void) memset(xregs, 0, xregs_size);
			elfnote(dfd, NT_PRXREG, xregs, xregs_size);
		}

		(void) close(lwpfd);
	}

	rc = 0;		/* bad is now good */
bad:
	if (lwpid)
		free(lwpid);
	cancel_old_notes();
	return (rc);
}

void
cancel_old_notes()
{
	if (pfd >= 0)
		(void) close(pfd);
	if (lwpid)
		free(lwpid);
	if (auxv)
		free(auxv);
	if (xregs)
		free(xregs);
	pfd = -1;
	lwpid = NULL;
	auxv = NULL;
	xregs = NULL;
}
