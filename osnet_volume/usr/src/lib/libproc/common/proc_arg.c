/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_arg.c	1.1	99/03/23 SMI"

#include <sys/types.h>
#include <sys/stat.h>

#include <libgen.h>
#include <limits.h>
#include <alloca.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "libproc.h"

static int
open_psinfo(const char *arg, int *perr)
{
	/*
	 * Allocate enough space for "/proc/" + arg + "/psinfo"
	 */
	char *path = alloca(strlen(arg) + 14);

	struct stat st;
	int fd;

	if (strchr(arg, '/') == NULL) {
		(void) strcpy(path, "/proc/");
		(void) strcat(path, arg);
	} else
		(void) strcpy(path, arg);

	(void) strcat(path, "/psinfo");

	/*
	 * Attempt to open the psinfo file, and return the fd if we can
	 * confirm this is a regular file provided by /proc.
	 */
	if ((fd = open(path, O_RDONLY)) >= 0) {
		if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
		    strcmp(st.st_fstype, "proc") != 0) {
			(void) close(fd);
			fd = -1;
		}
	} else if (errno == EACCES || errno == EPERM)
		*perr = G_PERM;

	return (fd);
}

static int
open_core(const char *arg, int *perr)
{
	GElf_Ehdr ehdr;
	int fd;

	/*
	 * Attempt to open the core file, and return the fd if we can confirm
	 * this is an ELF file of type ET_CORE.
	 */
	if ((fd = open(arg, O_RDONLY)) >= 0) {
		if (read(fd, &ehdr, sizeof (ehdr)) != sizeof (ehdr) ||
		    memcmp(&ehdr.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		    ehdr.e_type != ET_CORE) {
			(void) close(fd);
			fd = -1;
		}
	} else if (errno == EACCES || errno == EPERM)
		*perr = G_PERM;

	return (fd);
}

/*
 * Make the error message precisely match the type of arguments the caller
 * wanted to process.  This ensures that a tool which only accepts pids does
 * not produce an error message saying "no such process or core file 'foo'".
 */
static int
open_error(int oflag)
{
	if ((oflag & PR_ARG_ANY) == PR_ARG_PIDS)
		return (G_NOPROC);

	if ((oflag & PR_ARG_ANY) == PR_ARG_CORES)
		return (G_NOCORE);

	return (G_NOPROCORCORE);
}

struct ps_prochandle *
proc_arg_grab(const char *arg, int oflag, int gflag, int *perr)
{
	psinfo_t psinfo;
	char *core;
	int fd;

	*perr = 0;

	if ((oflag & PR_ARG_PIDS) && (fd = open_psinfo(arg, perr)) != -1) {
		if (read(fd, &psinfo, sizeof (psinfo_t)) == sizeof (psinfo_t)) {
			(void) close(fd);
			return (Pgrab(psinfo.pr_pid, gflag, perr));
		}
		/*
		 * If the read failed, the process may have gone away;
		 * we continue checking for core files or fail with G_NOPROC
		 */
		(void) close(fd);
	}

	if ((oflag & PR_ARG_CORES) && (fd = open_core(arg, perr)) != -1) {
		core = alloca(strlen(arg) + 1);
		(void) strcpy(core, arg);
		return (Pfgrab_core(fd, dirname(core), perr));
	}

	if (*perr == 0)
		*perr = open_error(oflag);

	return (NULL);
}

pid_t
proc_arg_psinfo(const char *arg, int oflag, psinfo_t *psp, int *perr)
{
	struct ps_prochandle *P;
	psinfo_t psinfo;
	int fd;

	if (psp == NULL)
		psp = &psinfo; /* Allow caller to pass NULL psinfo pointer */

	*perr = 0;

	if ((oflag & PR_ARG_PIDS) && (fd = open_psinfo(arg, perr)) != -1) {
		if (read(fd, psp, sizeof (psinfo_t)) == sizeof (psinfo_t)) {
			(void) close(fd);
			return (psp->pr_pid);
		}

		/*
		 * If the read failed, the process may have gone away;
		 * we continue checking for core files or fail with G_NOPROC
		 */
		(void) close(fd);
	}

	if ((oflag & PR_ARG_CORES) && (fd = open_core(arg, perr)) != -1) {
		char *core = alloca(strlen(arg) + 1);

		(void) strcpy(core, arg);

		if ((P = Pfgrab_core(fd, dirname(core), perr)) != NULL) {
			(void) memcpy(psp, Ppsinfo(P), sizeof (psinfo_t));
			Prelease(P, 0);
			return (psp->pr_pid);
		}

		(void) close(fd);
		return (-1);
	}

	if (*perr == 0)
		*perr = open_error(oflag);

	return (-1);
}

/*
 * Convert psinfo_t.pr_psargs string into itself, replacing unprintable
 * characters with space along the way.  Stop on a null character.
 */
void
proc_unctrl_psinfo(psinfo_t *psp)
{
	char *s = &psp->pr_psargs[0];
	size_t n = PRARGSZ;
	int c;

	while (n-- != 0 && (c = (*s & UCHAR_MAX)) != '\0') {
		if (!isprint(c))
			c = ' ';
		*s++ = (char)c;
	}

	*s = '\0';
}
