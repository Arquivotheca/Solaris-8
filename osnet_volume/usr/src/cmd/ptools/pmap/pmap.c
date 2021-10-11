/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pmap.c	1.14	99/09/06 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <link.h>
#include <libelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <libproc.h>

/* obsolete flags */
#ifndef	MA_BREAK
#define	MA_BREAK	0
#endif
#ifndef	MA_STACK
#define	MA_STACK	0
#endif

static	int	rmapping_iter(struct ps_prochandle *, proc_map_f *, void *);
static	int	look_map(ulong_t *, const prmap_t *, const char *);
static	int	look_xmap(struct ps_prochandle *);
static	int	perr(char *);
static	char	*mflags(uint_t);

static	int	reserved = 0;
static	int	do_xmap = 0;
static	int	lflag = 0;

static	char	*command;
static	char	*procname;
static	struct ps_prochandle *Pr;

int
main(int argc, char **argv)
{
	int errflg = 0;
	int rc = 0;
	int opt;
	int Fflag = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "rxlF")) != EOF) {
		switch (opt) {
		case 'r':		/* show reserved mappings */
			reserved = 1;
			break;
		case 'x':		/* show extended mappings */
			do_xmap = 1;
			break;
		case 'l':		/* show unresolved link map names */
			lflag = 1;
			break;
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (do_xmap && reserved) {
		(void) fprintf(stderr, "%s: -r and -x are mutually exclusive\n",
			command);
		return (2);
	}

	if (errflg || argc <= 0) {
		(void) fprintf(stderr,
		    "usage:\t%s [-rxlF] { pid | core } ...\n", command);
		(void) fprintf(stderr,
			"  (report process address maps)\n");
		(void) fprintf(stderr,
			"  -r: show reserved address maps\n");
		(void) fprintf(stderr,
			"  -x: show resident/shared/private mapping details\n");
		(void) fprintf(stderr,
			"  -l: show unresolved dynamic linker map names\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		return (2);
	}

	while (argc-- > 0) {
		char *arg;
		int gcode;
		psinfo_t psinfo;

		if ((Pr = proc_arg_grab(arg = *argv++, PR_ARG_ANY,
		    PGRAB_RETAIN | Fflag, &gcode)) == NULL) {
			(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
			    command, arg, Pgrab_error(gcode));
			rc++;
			continue;
		}

		(void) memcpy(&psinfo, Ppsinfo(Pr), sizeof (psinfo_t));
		proc_unctrl_psinfo(&psinfo);

		if (Pstate(Pr) == PS_DEAD) {
			(void) printf("core '%s' of %d:\t%.70s\n",
			    arg, (int)psinfo.pr_pid, psinfo.pr_psargs);

			if (reserved || do_xmap) {
				(void) printf("  -%c option is not compatible "
				    "with core files\n", do_xmap ? 'x' : 'r');
				Prelease(Pr, 0);
				rc++;
				continue;
			}

		} else {
			(void) printf("%d:\t%.70s\n",
			    (int)psinfo.pr_pid, psinfo.pr_psargs);
		}

		if (!(Pstatus(Pr)->pr_flags & PR_ISSYS)) {
			procname = arg;		/* for perr() */

			if (Pgetauxval(Pr, AT_BASE) != -1L &&
			    Prd_agent(Pr) == NULL) {
				(void) fprintf(stderr, "%s: warning: "
				    "librtld_db failed to initialize; "
				    "shared library information will not be "
				    "available\n", command);
			}

			if (!do_xmap) {
				ulong_t total = 0;

				if (reserved) {
					rc += rmapping_iter(Pr,
					    (proc_map_f *)look_map, &total);
				} else {
					rc += Pmapping_iter(Pr,
					    (proc_map_f *)look_map, &total);
				}

				(void) printf(" %stotal %8luK\n",
				    Pstatus(Pr)->pr_dmodel == PR_MODEL_LP64 ?
				    "        " : "", total);
			} else
				rc += look_xmap(Pr);
		}

		Prelease(Pr, 0);
	}

	return (rc);
}

static char *
make_name(struct ps_prochandle *Pr, uintptr_t addr, const char *mapname,
	char *buf, size_t bufsz)
{
	const pstatus_t *Psp = Pstatus(Pr);
	char fname[100];
	struct stat statb;
	int len;

	if (!lflag && strcmp(mapname, "a.out") == 0 &&
	    Pexecname(Pr, buf, bufsz) != NULL)
		return (buf);

	if (Pobjname(Pr, addr, buf, bufsz) != NULL) {
		if (lflag)
			return (buf);
		if ((len = resolvepath(buf, buf, bufsz)) > 0) {
			buf[len] = '\0';
			return (buf);
		}
	}

	if (Pstate(Pr) != PS_DEAD && *mapname != '\0') {
		(void) sprintf(fname, "/proc/%d/object/%s",
			(int)Psp->pr_pid, mapname);
		if (stat(fname, &statb) == 0) {
			dev_t dev = statb.st_dev;
			ino_t ino = statb.st_ino;
			(void) snprintf(buf, bufsz, "dev:%lu,%lu ino:%lu",
				(ulong_t)major(dev), (ulong_t)minor(dev), ino);
			return (buf);
		}
	}

	return (NULL);
}

static char *
anon_name(char *name, const pstatus_t *Psp,
    uintptr_t vaddr, size_t size, int mflags, int shmid)
{
	if ((mflags & MA_SHARED) && shmid != -1) {
		if (mflags & MA_ISM)
			(void) sprintf(name, " [ ism shmid=0x%x ]", shmid);
		else
			(void) sprintf(name, " [ shmid=0x%x ]", shmid);

	} else if (vaddr + size > Psp->pr_stkbase &&
	    vaddr < Psp->pr_stkbase + Psp->pr_stksize) {
		(void) strcpy(name, "  [ stack ]");
	} else if (vaddr + size > Psp->pr_brkbase &&
	    vaddr < Psp->pr_brkbase + Psp->pr_brksize) {
		(void) strcpy(name, "  [ heap ]");
	} else {
		(void) strcpy(name, "  [ anon ]");
	}

	return (name);
}

static int
rmapping_iter(struct ps_prochandle *Pr, proc_map_f *func, void *cd)
{
	char mapname[PATH_MAX];
	int mapfd, nmap, i, rc;
	struct stat st;
	prmap_t *prmapp, *pmp;
	ssize_t n;

	(void) snprintf(mapname, sizeof (mapname),
	    "/proc/%d/rmap", (int)Pstatus(Pr)->pr_pid);

	if ((mapfd = open(mapname, O_RDONLY)) < 0 || fstat(mapfd, &st) != 0) {
		if (mapfd >= 0)
			(void) close(mapfd);
		return (perr(mapname));
	}

	nmap = st.st_size / sizeof (prmap_t);
	prmapp = malloc((nmap + 1) * sizeof (prmap_t));

	if ((n = pread(mapfd, prmapp, (nmap + 1) * sizeof (prmap_t), 0L)) < 0) {
		(void) close(mapfd);
		free(prmapp);
		return (perr("read map"));
	}

	(void) close(mapfd);
	nmap = n / sizeof (prmap_t);

	for (i = 0, pmp = prmapp; i < nmap; i++, pmp++) {
		if ((rc = func(cd, pmp, NULL)) != 0) {
			free(prmapp);
			return (rc);
		}
	}

	free(prmapp);
	return (0);
}

/*ARGSUSED*/
static int
look_map(ulong_t *total, const prmap_t *pmp, const char *object_name)
{
	const pstatus_t *Psp = Pstatus(Pr);
	size_t size = (pmp->pr_size + 1023) / 1024;
	char mname[PATH_MAX];
	char *lname = NULL;

	/*
	 * If the mapping is not anon or not part of the heap, make a name
	 * for it.  We don't want to report the heap as a.out's data.
	 */
	if (!(pmp->pr_mflags & MA_ANON) ||
	    pmp->pr_vaddr + pmp->pr_size <= Psp->pr_brkbase ||
	    pmp->pr_vaddr >= Psp->pr_brkbase + Psp->pr_brksize)
		lname = make_name(Pr, pmp->pr_vaddr, pmp->pr_mapname,
			mname, sizeof (mname));

	if (lname == NULL && (pmp->pr_mflags & MA_ANON)) {
		lname = anon_name(mname, Psp, pmp->pr_vaddr,
		    pmp->pr_size, pmp->pr_mflags, pmp->pr_shmid);
	}

	(void) printf(lname ? "%.*lX %6ldK %-17s %s\n" : "%.*lX %6ldK %s\n",
	    Psp->pr_dmodel == PR_MODEL_LP64 ? 16 : 8, (uintptr_t)pmp->pr_vaddr,
	    size, mflags(pmp->pr_mflags), lname);

	*total += size;
	return (0);
}

static void
printK(long value)
{
	if (value == 0)
		(void) printf("       -");
	else
		(void) printf(" %7ld", value);
}

static int
look_xmap(struct ps_prochandle *Pr)
{
	char mapname[PATH_MAX];
	const pstatus_t *Psp = Pstatus(Pr);
	int mapfd;
	struct stat statb;
	prxmap_t *prmapp, *pmp;
	int nmap;
	ssize_t n;
	int i;
	ulong_t total_size = 0;
	ulong_t total_res = 0;
	ulong_t total_shared = 0;
	ulong_t total_priv = 0;
	int addr_width = (Psp->pr_dmodel == PR_MODEL_LP64) ? 16 : 8;

	(void) printf("%*s   Kbytes Resident Shared Private Permissions       "
	    "Mapped File\n", addr_width, "Address");

	(void) sprintf(mapname, "/proc/%d/xmap", (int)Psp->pr_pid);
	if ((mapfd = open(mapname, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0) {
		if (mapfd >= 0)
			(void) close(mapfd);
		return (perr(mapname));
	}
	nmap = statb.st_size / sizeof (prxmap_t);
	prmapp = malloc((nmap + 1) * sizeof (prxmap_t));

	if ((n = pread(mapfd, prmapp, (nmap+1) * sizeof (prxmap_t), 0L)) < 0) {
		(void) close(mapfd);
		return (perr("read xmap"));
	}
	(void) close(mapfd);
	nmap = n / sizeof (prxmap_t);

	for (i = 0, pmp = prmapp; i < nmap; i++, pmp++) {
		char *lname = NULL;
		char *ln;

		/* don't report heap as belonging to the a.out's data */
		if (!(pmp->pr_mflags & MA_ANON) ||
		    pmp->pr_vaddr + pmp->pr_size <= Psp->pr_brkbase ||
		    pmp->pr_vaddr >= Psp->pr_brkbase + Psp->pr_brksize)
			lname = make_name(Pr, pmp->pr_vaddr, pmp->pr_mapname,
				mapname, sizeof (mapname));

		if (lname != NULL) {
			if ((ln = strrchr(lname, '/')) != NULL)
				lname = ln + 1;
		} else if (pmp->pr_mflags & MA_ANON) {
			lname = anon_name(mapname, Psp, pmp->pr_vaddr,
			    pmp->pr_size, pmp->pr_mflags, pmp->pr_shmid);
		}

		(void) printf("%.*lX", addr_width, (ulong_t)pmp->pr_vaddr);

		printK((pmp->pr_size + 1023) / 1024);
		printK((pmp->pr_anon + pmp->pr_vnode) *
				(pmp->pr_pagesize / 1024));
		printK((pmp->pr_ashared + pmp->pr_vshared) *
				(pmp->pr_pagesize / 1024));
		printK((pmp->pr_anon + pmp->pr_vnode-pmp->pr_ashared -
				pmp->pr_vshared) * (pmp->pr_pagesize / 1024));
		(void) printf(lname?  " %-17s %s\n" : " %s\n",
			mflags(pmp->pr_mflags), lname);

		total_size += (pmp->pr_size + 1023) / 1024;
		total_res += (pmp->pr_anon + pmp->pr_vnode) *
			(pmp->pr_pagesize / 1024);
		total_shared += (pmp->pr_ashared + pmp->pr_vshared) *
			(pmp->pr_pagesize / 1024);
		total_priv += (pmp->pr_anon + pmp->pr_vnode - pmp->pr_ashared -
		    pmp->pr_vshared) * (pmp->pr_pagesize / 1024);
	}
	(void) printf("%s--------  ------  ------  ------  ------\n",
		(addr_width == 16)? "--------" : "");

	(void) printf("%stotal Kb",
		(addr_width == 16)? "        " : "");
	printK(total_size);
	printK(total_res);
	printK(total_shared);
	printK(total_priv);
	(void) printf("\n");

	free(prmapp);

	return (0);
}

static int
perr(char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

static char *
mflags(uint_t arg)
{
	static char code_buf[80];
	char *str = code_buf;

	arg &= ~(MA_ANON|MA_BREAK|MA_STACK|MA_ISM);

	if (arg == 0)
		return ("-");

	if (arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED))
		(void) sprintf(str, "0x%x",
			arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED));
	else
		*str = '\0';

	if (arg & MA_READ)
		(void) strcat(str, "/read");
	if (arg & MA_WRITE)
		(void) strcat(str, "/write");
	if (arg & MA_EXEC)
		(void) strcat(str, "/exec");
	if (arg & MA_SHARED)
		(void) strcat(str, "/shared");

	if (*str == '/')
		str++;

	return (str);
}
