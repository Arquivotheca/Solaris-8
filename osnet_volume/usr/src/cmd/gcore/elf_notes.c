/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_notes.c	1.5	99/03/23 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/regset.h>
#include <procfs.h>
#include <sys/elf.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/auxv.h>
#include <sys/machelf.h>
#include "elf_notes.h"

static char procname[100];	/* "/proc/pid" or other /proc directory */

static int has_platname;
static char platform_name[SYS_NMLN];

static int nauxv;
static auxv_t *auxv = NULL;

static int has_uts;
static struct utsname uts;

static prcred_t *cred = NULL;
static size_t cr_size;

#if defined(sparc) || defined(__sparc)
static int has_gwins;
#endif /* sparc */

static int has_asrs;
static int asrs_size;
static char *asrs = NULL;

static int has_xrs;
static int xregs_size;
static char *xregs = NULL;

int
setup_note_header(Phdr *vp, int nlwp, char *pdir, pid_t pid)
{
	char filename[sizeof (procname) + 32];
	char *fname;
	DIR *dirp = NULL;
	struct dirent *dentp;
	struct stat statb;

	(void) strncpy(procname, pdir, sizeof (procname) - 12);
	(void) sprintf(procname + strlen(procname), "/%d", (int)pid);

	(void) strcpy(filename, procname);
	fname = filename + strlen(filename);

	vp->p_type = PT_NOTE;
	vp->p_flags = PF_R;
	vp->p_filesz = (sizeof (Note) * (2 + 2*nlwp)) +
		roundup(sizeof (psinfo_t), sizeof (Word)) +
		roundup(sizeof (pstatus_t), sizeof (Word)) +
		nlwp * roundup(sizeof (lwpsinfo_t), sizeof (Word)) +
		nlwp * roundup(sizeof (lwpstatus_t), sizeof (Word));

	(void) strcpy(fname, "/auxv");
	if (stat(filename, &statb) == 0 &&
	    (nauxv = statb.st_size / sizeof (auxv_t)) > 0) {
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

	has_uts = FALSE;
	if (uname(&uts) != -1) {
		has_uts = TRUE;
		vp->p_filesz += sizeof (Note) +
			roundup(sizeof (uts), sizeof (Word));
	}

	(void) strcpy(fname, "/cred");
	if (stat(filename, &statb) == 0 && statb.st_size >= sizeof (prcred_t)) {
		/*
		 * Make sure the size is enough for a prcred_t and round number
		 * of supplementary groups.
		 */
		int ngroups = (statb.st_size - sizeof (prcred_t)) /
		    sizeof (gid_t) + 1;

		if (ngroups > 1) {
			cr_size = sizeof (prcred_t) +
			    (ngroups - 1) * sizeof (gid_t);
		} else
			cr_size = sizeof (prcred_t);

		vp->p_filesz += sizeof (Note) + roundup(cr_size, sizeof (Word));

		if ((cred = malloc(cr_size)) == NULL)
			goto nomalloc;
	}

	/*
	 * We have to search the active lwp to determine
	 * the existence of extra registers and gwindows.
	 */
#if defined(sparc) || defined(__sparc)
	has_gwins = FALSE;
#endif /* sparc */
	has_xrs = FALSE;
	has_asrs = FALSE;

	(void) strcpy(fname, "/lwp");
	if ((dirp = opendir(filename)) == NULL)
		return (0);

	fname = fname + strlen(fname);
	*fname++ = '/';
	while (dentp = readdir(dirp)) {
		if (dentp->d_name[0] == '.')
			continue;

#if defined(sparc) || defined(__sparc)
		(void) strcpy(fname, dentp->d_name);
		(void) strcat(fname, "/gwindows");
		if (stat(filename, &statb) == 0 && statb.st_size != 0) {
			has_gwins = TRUE;
			vp->p_filesz += sizeof (Note) +
				roundup(statb.st_size, sizeof (Word));
		}
#endif /* sparc */

		(void) strcpy(fname, dentp->d_name);
		(void) strcat(fname, "/asrs");
		if (stat(filename, &statb) == 0 && statb.st_size != 0) {
			has_asrs = TRUE;
			asrs_size = statb.st_size;
			vp->p_filesz += sizeof (Note) +
				roundup(asrs_size, sizeof (Word));
			if (asrs == NULL &&
			    (asrs = malloc(asrs_size)) == NULL) {
				(void) closedir(dirp);
				goto nomalloc;
			}
		}

		(void) strcpy(fname, dentp->d_name);
		(void) strcat(fname, "/xregs");
		if (stat(filename, &statb) == 0 && statb.st_size != 0) {
			has_xrs = TRUE;
			xregs_size = statb.st_size;
			vp->p_filesz += sizeof (Note) +
				roundup(xregs_size, sizeof (Word));
			if (xregs == NULL &&
			    (xregs = malloc(xregs_size)) == NULL) {
				(void) closedir(dirp);
				goto nomalloc;
			}
		}
	}
	(void) closedir(dirp);

	return (0);

nomalloc:
	(void) fprintf(stderr, "gcore: malloc() failed\n");
	cancel_notes();
	return (-1);
}

int
write_elfnotes(int nlwp, int dfd)
{
	char filename[sizeof (procname) + 32];
	char *fname;
	int fd;
	DIR *dirp = NULL;
	struct dirent *dentp;
	int rc = -1;
	psinfo_t psinfo;
	pstatus_t pstatus;

	(void) strcpy(filename, procname);
	fname = filename + strlen(filename);

	(void) strcpy(fname, "/psinfo");
	if ((fd = open(filename, O_RDONLY)) < 0) {
		(void) perror("write_elfnotes(): open psinfo");
		goto bad;
	}
	if (read(fd, &psinfo, sizeof (psinfo)) != sizeof (psinfo)) {
		(void) close(fd);
		(void) perror("write_elfnotes(): read psinfo");
		goto bad;
	}
	(void) close(fd);
	elfnote(dfd, NT_PSINFO, (char *)&psinfo, sizeof (psinfo));

	(void) strcpy(fname, "/status");
	if ((fd = open(filename, O_RDONLY)) < 0) {
		(void) perror("write_elfnotes(): open status");
		goto bad;
	}
	if (read(fd, &pstatus, sizeof (pstatus)) != sizeof (pstatus)) {
		(void) close(fd);
		(void) perror("write_elfnotes(): read status");
		goto bad;
	}
	(void) close(fd);
	elfnote(dfd, NT_PSTATUS, (char *)&pstatus, sizeof (pstatus));

	if (has_platname)
		elfnote(dfd, NT_PLATFORM, platform_name,
			strlen(platform_name) + 1);

	if (nauxv > 0) {
		(void) strcpy(fname, "/auxv");
		if ((fd = open(filename, O_RDONLY)) < 0) {
			(void) perror("write_elfnotes(): open auxv");
			goto bad;
		}
		if (read(fd, auxv, nauxv * sizeof (auxv_t)) !=
		    nauxv * sizeof (auxv_t)) {
			(void) close(fd);
			(void) perror("write_elfnotes(): read auxv");
			goto bad;
		}
		(void) close(fd);
		elfnote(dfd, NT_AUXV, (char *)auxv, nauxv * sizeof (auxv_t));
	}

	if (has_uts)
		elfnote(dfd, NT_UTSNAME, (char *)&uts, sizeof (uts));

	(void) strcpy(fname, "/cred");
	if ((fd = open(filename, O_RDONLY)) < 0) {
		(void) perror("write_elfnotes(): open cred");
		goto bad;
	}
	if (read(fd, cred, cr_size) != cr_size) {
		(void) close(fd);
		(void) perror("write_elfnotes(): read cred");
		goto bad;
	}
	(void) close(fd);
	elfnote(dfd, NT_PRCRED, (char *)cred, cr_size);

	(void) strcpy(fname, "/lwp");
	if ((dirp = opendir(filename)) == NULL)
		goto bad;
	fname = filename + strlen(filename);
	*fname++ = '/';

	/* for each lwp */
	while (dentp = readdir(dirp)) {
		int lwpfd;
		lwpsinfo_t lwpsinfo;
		lwpstatus_t lwpstatus;

		if (dentp->d_name[0] == '.')
			continue;

		if (nlwp-- <= 0) {
			(void) fprintf(stderr, "gcore: too many lwps\n");
			goto bad;
		}

		(void) strcpy(fname, dentp->d_name);
		(void) strcat(fname, "/lwpsinfo");
		if ((lwpfd = open(filename, O_RDONLY)) < 0) {
			(void) perror("write_elfnotes(): open lwpsinfo");
			goto bad;
		}
		if (read(lwpfd, &lwpsinfo, sizeof (lwpsinfo)) < 0) {
			(void) close(lwpfd);
			(void) perror("write_elfnotes(): read lwpsinfo");
			goto bad;
		}
		(void) close(lwpfd);
		elfnote(dfd, NT_LWPSINFO, (char *)&lwpsinfo,
			sizeof (lwpsinfo_t));

		(void) strcpy(fname, dentp->d_name);
		(void) strcat(fname, "/lwpstatus");
		if ((lwpfd = open(filename, O_RDONLY)) < 0) {
			(void) perror("write_elfnotes(): open lwpstatus");
			goto bad;
		}
		if (read(lwpfd, &lwpstatus, sizeof (lwpstatus)) < 0) {
			(void) close(lwpfd);
			(void) perror("write_elfnotes(): read lwpstatus");
			goto bad;
		}
		(void) close(lwpfd);
		elfnote(dfd, NT_LWPSTATUS, (char *)&lwpstatus,
			sizeof (lwpstatus_t));

#if defined(sparc) || defined(__sparc)
		if (has_gwins) {
			gwindows_t gwins;
			ssize_t size;

			(void) strcpy(fname, dentp->d_name);
			(void) strcat(fname, "/gwindows");
			if ((lwpfd = open(filename, O_RDONLY)) < 0) {
				(void) perror(
					"write_elfnotes(): open gwindows");
				goto bad;
			}
			size = read(lwpfd, &gwins, sizeof (gwins));
			if (size < 0) {
				(void) close(lwpfd);
				(void) perror(
					"write_elfnotes(): read gwindows");
				goto bad;
			}
			(void) close(lwpfd);
			if (size > 0)
				elfnote(dfd, NT_GWINDOWS, (char *)&gwins, size);
		}
#endif /* sparc */

		if (has_asrs) {
			(void) strcpy(fname, dentp->d_name);
			(void) strcat(fname, "/asrs");
			if ((lwpfd = open(filename, O_RDONLY)) < 0) {
				(void) perror("write_elfnotes(): open asrs");
				goto bad;
			}
			if (read(lwpfd, asrs, asrs_size) != asrs_size) {
				(void) close(lwpfd);
				(void) perror("write_elfnotes(): read asrs");
				goto bad;
			}
			(void) close(lwpfd);
			elfnote(dfd, NT_ASRS, asrs, asrs_size);
		}

		if (has_xrs) {
			(void) strcpy(fname, dentp->d_name);
			(void) strcat(fname, "/xregs");
			if ((lwpfd = open(filename, O_RDONLY)) < 0) {
				(void) perror("write_elfnotes(): open xregs");
				goto bad;
			}
			if (read(lwpfd, xregs, xregs_size) != xregs_size) {
				(void) close(lwpfd);
				(void) perror("write_elfnotes(): read xregs");
				goto bad;
			}
			(void) close(lwpfd);
			elfnote(dfd, NT_PRXREG, xregs, xregs_size);
		}
	}

	if (nlwp != 0) {
		(void) fprintf(stderr, "gcore: too few lwps\n");
		goto bad;
	}

	rc = 0;		/* bad is now good */
bad:
	if (dirp != NULL)
		(void) closedir(dirp);
	cancel_notes();
	return (rc);
}

void
cancel_notes()
{
	if (auxv)
		free(auxv);
	if (xregs)
		free(xregs);
	auxv = NULL;
	xregs = NULL;
}
