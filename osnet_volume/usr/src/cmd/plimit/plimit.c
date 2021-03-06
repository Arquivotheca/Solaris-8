/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)plimit.c	1.2	99/03/23 SMI"	/* SVr4.0 1.5	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mkdev.h>
#include <libproc.h>

/* XXX A bug in the <string.h> header file requires this */
extern char *strtok_r(char *s1, const char *s2, char **lasts);

#define	TRUE	1
#define	FALSE	0

static	int	interrupt;
static	char	*command;
static	int	Fflag;
static	int	kbytes = FALSE;
static	int	mbytes = FALSE;
static	char	set_current[RLIM_NLIMITS];
static	char	set_maximum[RLIM_NLIMITS];
static	struct rlimit64 rlimit[RLIM_NLIMITS];

static	void	intr(int);
static	int	parse_limits(int, char *);
static	void	show_limits(struct ps_prochandle *);
static	void	set_limits(struct ps_prochandle *);

static void
usage()
{
	(void) fprintf(stderr,
		"usage:\n"
		"    For each process, report all resource limits:\n"
		"\t%s [-km] pid ...\n"
		"\t-k\treport file sizes in kilobytes\n"
		"\t-m\treport file/memory sizes in megabytes\n"
		"    For each process, set specified resource limits:\n"
		"\t%s -{cdfnstv} soft,hard ... pid ...\n"
		"\t-c soft,hard\tset core file size limits\n"
		"\t-d soft,hard\tset data segment (heap) size limits\n"
		"\t-f soft,hard\tset file size limits\n"
		"\t-n soft,hard\tset file descriptor limits\n"
		"\t-s soft,hard\tset stack segment size limits\n"
		"\t-t soft,hard\tset CPU time limits\n"
		"\t-v soft,hard\tset virtual memory size limits\n"
		"\t(default units are as shown by the output of '%s pid')\n",
		command, command, command);
	exit(2);
}

main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = 0;
	int set = FALSE;
	struct ps_prochandle *Pr;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	while ((opt = getopt(argc, argv, "Fkmc:d:f:n:s:t:v:")) != EOF) {
		switch (opt) {
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		case 'k':
			kbytes = TRUE;
			mbytes = FALSE;
			break;
		case 'm':
			kbytes = FALSE;
			mbytes = TRUE;
			break;
		case 'c':	/* core file size */
			set = TRUE;
			errflg += parse_limits(RLIMIT_CORE, optarg);
			break;
		case 'd':	/* data segment size */
			set = TRUE;
			errflg += parse_limits(RLIMIT_DATA, optarg);
			break;
		case 'f':	/* file size */
			set = TRUE;
			errflg += parse_limits(RLIMIT_FSIZE, optarg);
			break;
		case 'n':	/* file descriptors */
			set = TRUE;
			errflg += parse_limits(RLIMIT_NOFILE, optarg);
			break;
		case 's':	/* stack segment size */
			set = TRUE;
			errflg += parse_limits(RLIMIT_STACK, optarg);
			break;
		case 't':	/* CPU time */
			set = TRUE;
			errflg += parse_limits(RLIMIT_CPU, optarg);
			break;
		case 'v':	/* virtual memory size */
			set = TRUE;
			errflg += parse_limits(RLIMIT_VMEM, optarg);
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0)
		usage();

	/* catch signals from terminal */
	if (sigset(SIGHUP, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGHUP, intr);
	if (sigset(SIGINT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGINT, intr);
	if (sigset(SIGQUIT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGQUIT, intr);
	(void) sigset(SIGTERM, intr);

	while (--argc >= 0 && !interrupt) {
		psinfo_t psinfo;
		char *arg;
		pid_t pid;
		int gret;

		(void) fflush(stdout);	/* process-at-a-time */

		/* get the specified pid and the psinfo struct */
		if ((pid = proc_arg_psinfo(arg = *argv++, PR_ARG_PIDS,
		    &psinfo, &gret)) == -1) {
			(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
				command, arg, Pgrab_error(gret));
			retc++;
		} else if ((Pr = Pgrab(pid, Fflag, &gret)) != NULL) {
			if (Pcreate_agent(Pr) == 0) {
				if (set)
					set_limits(Pr);
				else {
					proc_unctrl_psinfo(&psinfo);
					(void) printf("%d:\t%.70s\n",
						(int)pid, psinfo.pr_psargs);
					show_limits(Pr);
				}
				Pdestroy_agent(Pr);
			} else {
				(void) fprintf(stderr,
					"%s: cannot control process %d\n",
					command, (int)pid);
				retc++;
			}
			Prelease(Pr, 0);
		} else {
			if ((gret == G_SYS || gret == G_SELF) && !set) {
				proc_unctrl_psinfo(&psinfo);
				(void) printf("%d:\t%.70s\n", (int)pid,
					psinfo.pr_psargs);
				if (gret == G_SYS)
					(void) printf("  [system process]\n");
				else
					show_limits(NULL);
			} else {
				(void) fprintf(stderr,
					"%s: %s: %d\n",
					command, Pgrab_error(gret), (int)pid);
				retc++;
			}
		}
	}

	if (interrupt)
		retc++;
	return (retc);
}

static void
intr(int sig)
{
	interrupt = sig;
}

/* ------ begin specific code ------ */

/*
 * Compute a limit, given a string:
 *	unlimited	unlimited
 *	nnn k		nnn kilobytes
 *	nnn m		nnn megabytes (minutes for CPU time)
 *	nnn h		nnn hours (for CPU time only)
 *	mm : ss		minutes and seconds (for CPU time only)
 */
static int
limit_value(int which, char *arg, rlim64_t *limit)
{
	rlim64_t value;
	rlim64_t unit;
	char *lastc;

	if (strcmp(arg, "unlimited") == 0) {
		*limit = RLIM64_INFINITY;
		return (0);
	}

	if (which == RLIMIT_CPU && strchr(arg, ':') != NULL) {
		char *minutes = strtok_r(arg, " \t:", &lastc);
		char *seconds = strtok_r(NULL, " \t", &lastc);
		rlim64_t sec;

		if (seconds != NULL && strtok_r(NULL, " \t", &lastc) != NULL)
			return (1);
		value = strtoull(minutes, &lastc, 10);
		if (*lastc != '\0' || value > RLIM64_INFINITY / 60)
			return (1);
		if (seconds == NULL || *seconds == '\0')
			sec = 0;
		else {
			sec = strtoull(seconds, &lastc, 10);
			if (*lastc != '\0' || sec > 60)
				return (1);
		}
		value = value * 60 + sec;
		if (value > RLIM64_INFINITY)
			value = RLIM64_INFINITY;
		*limit = value;
		return (0);
	}

	switch (*(lastc = arg + strlen(arg) - 1)) {
	case 'k':
		unit = 1024;
		*lastc = '\0';
		break;
	case 'm':
		if (which == RLIMIT_CPU)
			unit = 60;
		else
			unit = 1024 * 1024;
		*lastc = '\0';
		break;
	case 'h':
		if (which == RLIMIT_CPU)
			unit = 60 * 60;
		else
			return (1);
		*lastc = '\0';
		break;
	default:
		switch (which) {
		case RLIMIT_CPU:	unit = 1;	break;
		case RLIMIT_FSIZE:	unit = 512;	break;
		case RLIMIT_DATA:	unit = 1024;	break;
		case RLIMIT_STACK:	unit = 1024;	break;
		case RLIMIT_CORE:	unit = 512;	break;
		case RLIMIT_NOFILE:	unit = 1;	break;
		case RLIMIT_VMEM:	unit = 1024;	break;
		}
		break;
	}

	value = strtoull(arg, &lastc, 10);
	if (*lastc != '\0' || value > RLIM64_INFINITY / unit)
		return (1);

	value *= unit;
	if (value > RLIM64_INFINITY)
		value = RLIM64_INFINITY;
	*limit = value;
	return (0);
}

static int
parse_limits(int which, char *arg)
{
	char *lastc;
	char *soft = strtok_r(arg, " \t,", &lastc);
	char *hard = strtok_r(NULL, " \t", &lastc);
	struct rlimit64 *rp = &rlimit[which];

	if (hard != NULL && strtok_r(NULL, " \t", &lastc) != NULL)
		return (1);

	if (soft == NULL || *soft == '\0') {
		rp->rlim_cur = 0;
		set_current[which] = FALSE;
	} else {
		if (limit_value(which, soft, &rp->rlim_cur) != 0)
			return (1);
		set_current[which] = TRUE;
	}

	if (hard == NULL || *hard == '\0') {
		rp->rlim_max = 0;
		set_maximum[which] = FALSE;
	} else {
		if (limit_value(which, hard, &rp->rlim_max) != 0)
			return (1);
		set_maximum[which] = TRUE;
	}
	if (set_current[which] && set_maximum[which] &&
	    rp->rlim_cur > rp->rlim_max)
		return (1);

	return (0);
}

static void
limit_adjust(struct rlimit64 *rp, int units)
{
	if (rp->rlim_cur != RLIM64_INFINITY)
		rp->rlim_cur /= units;
	if (rp->rlim_max != RLIM64_INFINITY)
		rp->rlim_max /= units;
}

static char *
limit_values(struct rlimit64 *rp)
{
	static char buffer[64];
	char buf1[32];
	char buf2[32];
	char *s1;
	char *s2;

	if (rp->rlim_cur == RLIM64_INFINITY)
		s1 = "unlimited";
	else {
		(void) sprintf(s1 = buf1, "%lld", rp->rlim_cur);
		if (strlen(s1) < 8)
			(void) strcat(s1, "\t");
	}

	if (rp->rlim_max == RLIM64_INFINITY)
		s2 = "unlimited";
	else {
		(void) sprintf(s2 = buf2, "%lld", rp->rlim_max);
	}

	(void) sprintf(buffer, "%s\t%s", s1, s2);

	return (buffer);
}

static void
show_limits(struct ps_prochandle *Pr)
{
	struct rlimit64 rlim;
	int resource;
	char buf[32];
	char *s;

	(void) printf("   resource\t\t current\t maximum\n");

	for (resource = 0; resource < RLIM_NLIMITS; resource++) {
		if (pr_getrlimit64(Pr, resource, &rlim) != 0)
			continue;

		switch (resource) {
		case RLIMIT_CPU:
			s = "  time(seconds)\t\t";
			break;
		case RLIMIT_FSIZE:
			if (kbytes) {
				s = "  file(kbytes)\t\t";
				limit_adjust(&rlim, 1024);
			} else if (mbytes) {
				s = "  file(mbytes)\t\t";
				limit_adjust(&rlim, 1024 * 1024);
			} else {
				s = "  file(blocks)\t\t";
				limit_adjust(&rlim, 512);
			}
			break;
		case RLIMIT_DATA:
			if (mbytes) {
				s = "  data(mbytes)\t\t";
				limit_adjust(&rlim, 1024 * 1024);
			} else {
				s = "  data(kbytes)\t\t";
				limit_adjust(&rlim, 1024);
			}
			break;
		case RLIMIT_STACK:
			if (mbytes) {
				s = "  stack(mbytes)\t\t";
				limit_adjust(&rlim, 1024 * 1024);
			} else {
				s = "  stack(kbytes)\t\t";
				limit_adjust(&rlim, 1024);
			}
			break;
		case RLIMIT_CORE:
			if (kbytes) {
				s = "  coredump(kbytes)\t";
				limit_adjust(&rlim, 1024);
			} else if (mbytes) {
				s = "  coredump(mbytes)\t";
				limit_adjust(&rlim, 1024 * 1024);
			} else {
				s = "  coredump(blocks)\t";
				limit_adjust(&rlim, 512);
			}
			break;
		case RLIMIT_NOFILE:
			s = "  nofiles(descriptors)\t";
			break;
		case RLIMIT_VMEM:
			if (mbytes) {
				s = "  vmemory(mbytes)\t";
				limit_adjust(&rlim, 1024 * 1024);
			} else {
				s = "  vmemory(kbytes)\t";
				limit_adjust(&rlim, 1024);
			}
			break;
		default:
			(void) sprintf(buf, "  rlimit #%d\t", resource);
			s = buf;
			break;
		}

		(void) printf("%s%s\n", s, limit_values(&rlim));
	}
}

static void
set_one_limit(struct ps_prochandle *Pr, int which, rlim64_t cur, rlim64_t max)
{
	struct rlimit64 rlim;
	int be_su = 0;
	prcred_t prcred;
	struct {
		long cmd;
		prcred_t prcred;
	} ctl;

	if (pr_getrlimit64(Pr, which, &rlim) != 0)
		return;

	if (!set_current[which])
		cur = rlim.rlim_cur;
	if (!set_maximum[which])
		max = rlim.rlim_max;

	if (max < cur)
		max = cur;

	if (max > rlim.rlim_max && Pr != NULL)
		be_su = 1;
	rlim.rlim_cur = cur;
	rlim.rlim_max = max;

	if (be_su && geteuid() != 0) {
		(void) fprintf(stderr,
			"%s: must be super-user to increase the max limit\n",
			command);
		return;
	}
	if (be_su &&
	    proc_get_cred(Pstatus(Pr)->pr_pid, &prcred, 0) == 0 &&
	    prcred.pr_euid != 0) {
		ctl.prcred = prcred;
		ctl.prcred.pr_euid = 0;
		ctl.cmd = PCSCRED;
		(void) write(Pctlfd(Pr), &ctl, sizeof (ctl));
	} else {
		be_su = 0;
	}
	if (pr_setrlimit64(Pr, which, &rlim) != 0)
		perror(NULL);
	if (be_su) {
		ctl.prcred = prcred;
		ctl.cmd = PCSCRED;
		(void) write(Pctlfd(Pr), &ctl, sizeof (ctl));
	}
}

static void
set_limits(struct ps_prochandle *Pr)
{
	int which;

	for (which = 0; which < RLIM_NLIMITS; which++) {
		if (set_current[which] || set_maximum[which])
			set_one_limit(Pr, which,
				rlimit[which].rlim_cur, rlimit[which].rlim_max);
	}
}
