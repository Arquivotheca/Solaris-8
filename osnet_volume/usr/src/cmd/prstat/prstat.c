/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prstat.c	1.5	99/11/03 SMI"

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/loadavg.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>

#include <libintl.h>
#include <locale.h>

#include "prstat.h"
#include "prutil.h"
#include "prtable.h"
#include "prsort.h"
#include "prfile.h"

/*
 * x86 <sys/regs.h> ERR conflicts with <curses.h> ERR.  For the purposes
 * of this file, we care about the curses.h ERR so include that last.
 */

#if	defined(ERR)
#undef	ERR
#endif

#ifndef	TEXT_DOMAIN			/* should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* use this only if it wasn't */
#endif

#include <curses.h>
#include <term.h>

#define	NLWP_TITLE \
"   PID USERNAME  SIZE   RSS STATE  PRI NICE      TIME  CPU PROCESS/NLWP       "
#define	NVLWP_TITLE \
"   PID USERNAME USR SYS TRP TFL DFL LCK SLP LAT VCX ICX SCL SIG PROCESS/NLWP  "
#define	LWPID_TITLE \
"   PID USERNAME  SIZE   RSS STATE  PRI NICE      TIME  CPU PROCESS/LWPID      "
#define	VLWPID_TITLE \
"   PID USERNAME USR SYS TRP TFL DFL LCK SLP LAT VCX ICX SCL SIG PROCESS/LWPID "
#define	ULWP_TITLE \
" NPROC USERNAME  SIZE   RSS MEMORY      TIME  CPU                             "
#define	ULWP_TITLE2 \
"  NLWP USERNAME  SIZE   RSS MEMORY      TIME  CPU                             "

#define	LWP_LINE \
"%6d %-8s %5s %5s %-6s %3s  %3s %9s %3.3s%% %-.16s/%d"
#define	VLWP_LINE \
"%6d %-8s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s "\
"%3.3s %-.12s/%d"
#define	ULWP_LINE \
"%6d %-8s %5.5s %5.5s   %3.3s%% %9s %3.3s%%"
#define	TOTAL_LINE \
"Total: %d processes, %d lwps, load averages: %3.2f, %3.2f, %3.2f"

/* global variables */

static lwp_info_t	*lwp_head = NULL;	/* first lwp */
static lwp_info_t	*lwp_tail = NULL;	/* last lwp */
static ulwp_info_t	*ulwp_head = NULL;	/* first user */
static ulwp_info_t	*ulwp_tail = NULL;	/* last user */

static char	*t_ulon;			/* termcap: start underline */
static char	*t_uloff;			/* termcap: end underline */
static char	*t_up;				/* termcap: cursor 1 line up */
static char	*t_eol;				/* termcap: clear end of line */
static char	*t_smcup;			/* termcap: cursor mvcap on */
static char	*t_rmcup;			/* termcap: cursor mvcap off */
static char	*movecur = NULL;		/* termcap: move up string */
static char	*empty_string = "\0";		/* termcap: empty string */
static uint_t	print_movecur = FALSE;		/* print movecur or not */
static int	is_curses_on = FALSE;		/* current curses state */

static table_t	pid_tbl = {0, 0, NULL};		/* selected processes */
static table_t	cpu_tbl = {0, 0, NULL};		/* selected processors */
static table_t  set_tbl = {0, 0, NULL};		/* selected processor sets */
static nametbl_t euid_tbl = {0, 0, NULL}; 	/* selected effective users */
static nametbl_t ruid_tbl = {0, 0, NULL}; 	/* selected real users */

static uint_t	total_procs;			/* total number of procs */
static uint_t	total_lwps;			/* total number of lwps */

static uint_t	lwp_cnt = 0;			/* number of lwp_info nodes */
static uint_t	ulwp_cnt = 0;			/* number of ulwp_info nodes */
static float	ulwp_total_cpu = 0;		/* sum of all ui_pctcpu's */
static float	ulwp_total_mem = 0;		/* sum of all ui_pctmem's */

static	lp_list_t psorted;			/* pointers to lwp_infos */
static	lp_list_t usorted;			/* pointers to ulwp_infos */

static volatile uint_t sigwinch = 0;
static volatile uint_t sigtstp = 0;
static volatile uint_t sigterm = 0;

/* default settings */

static optdesc_t opts = {
	5,			/* interval between updates, seconds */
	15,			/* number of lines to display */
	5,			/* additional lines (users) -a mode */
	-1,			/* number of iterations; infinitely */
	OPT_PSINFO | OPT_FULLSCREEN | OPT_USEHOME | OPT_TERMCAP,
	-1			/* sort in decreasing order */
};

static void
ulwp_print()
{
	ulwp_info_t *ulwp;
	float cpu, mem;
	double loadavg[3];
	int i;
	char psize[6], prssize[6], pmem[6], pcpu[6], ptime[12];
	char pname[LOGNAME_MAX+1];

	(void) getloadavg(loadavg, 3);
	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	(void) putp(t_ulon);
	if (opts.o_outpmode & OPT_LWPS)
		(void) printf(ULWP_TITLE2);
	else
		(void) printf(ULWP_TITLE);
	(void) putp(t_uloff);
	(void) putp(t_eol);
	(void) putchar('\n');
	for (i = 0; i < usorted.lp_cnt; i++) {
		ulwp = usorted.lp_ptrs[i];
		/*
		 * CPU usage and memory usage normalization
		 */
		if (ulwp_total_cpu >= 100)
			cpu = (100 * ulwp->ui_pctcpu) / ulwp_total_cpu;
		else
			cpu = ulwp->ui_pctcpu;
		if (ulwp_total_mem >= 100)
			mem = (100 * ulwp->ui_pctmem) / ulwp_total_mem;
		else
			mem = ulwp->ui_pctmem;
		pwd_getname(ulwp->ui_uid, pname, LOGNAME_MAX + 1);
		Format_size(psize, ulwp->ui_size, 6);
		Format_size(prssize, ulwp->ui_rssize, 6);
		Format_pct(pmem, mem, 4);
		Format_pct(pcpu, cpu, 4);
		Format_time(ptime, ulwp->ui_time, 10);
		if (opts.o_outpmode & OPT_TTY)
			(void) putchar('\r');
		(void) printf(ULWP_LINE,
			ulwp->ui_nproc, pname,
			psize, prssize, pmem, ptime, pcpu);
		(void) putp(t_eol);
		(void) putchar('\n');
	}
	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	if (opts.o_outpmode & OPT_TERMCAP)
		while (i++ < opts.o_nusers) {
			(void) putp(t_eol);
			(void) putchar('\n');
		}
	(void) printf(TOTAL_LINE, total_procs, total_lwps,
	    loadavg[LOADAVG_1MIN], loadavg[LOADAVG_5MIN],
	    loadavg[LOADAVG_15MIN]);
	(void) putp(t_eol);
	(void) putchar('\n');
	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	(void) fflush(stdout);
}

void
ulwp_clear()
{

	ulwp_info_t *ulwp = ulwp_head;
	ulwp_info_t *nextulwp;

	while (ulwp) {
		nextulwp = ulwp->ui_next;
		free(ulwp);
		ulwp = nextulwp;
	}
	ulwp_cnt = 0;
	ulwp_total_cpu = ulwp_total_mem = 0;
	ulwp_head = NULL;

}

static void
ulwp_update(lwp_info_t *lwp)
{
	ulwp_info_t *ulwp;

	if (ulwp_head == NULL) {			/* first element */
		ulwp_head = ulwp_tail = ulwp = Zalloc(sizeof (ulwp_info_t));
		goto update;
	}

	for (ulwp = ulwp_head; ulwp; ulwp = ulwp->ui_next) {
		if (ulwp->ui_uid == lwp->li_info.pr_uid) {	/* uid found */
			ulwp->ui_nproc++;
			ulwp->ui_size	+= lwp->li_info.pr_size;
			ulwp->ui_rssize	+= lwp->li_info.pr_rssize;
			ulwp->ui_pctcpu	+=
			    FRC2PCT(lwp->li_info.pr_lwp.pr_pctcpu);
			ulwp->ui_time	+=
			    TIME2SEC(lwp->li_info.pr_lwp.pr_time);
			ulwp->ui_pctmem	+= FRC2PCT(lwp->li_info.pr_pctmem);
			ulwp->ui_key	+= lwp->li_key;
			ulwp_total_cpu  +=
			    FRC2PCT(lwp->li_info.pr_lwp.pr_pctcpu);
			ulwp_total_mem	+= FRC2PCT(lwp->li_info.pr_pctmem);
			return;
		}
	}

	ulwp_tail->ui_next = Zalloc(sizeof (ulwp_info_t));
	ulwp_tail->ui_next->ui_prev = ulwp_tail;
	ulwp_tail->ui_next->ui_next = NULL;
	ulwp_tail = ulwp_tail->ui_next;
	ulwp = ulwp_tail;

update:	ulwp->ui_uid	= lwp->li_info.pr_uid;
	ulwp->ui_nproc++;
	ulwp->ui_size	= lwp->li_info.pr_size;
	ulwp->ui_rssize	= lwp->li_info.pr_rssize;
	ulwp->ui_pctcpu	= FRC2PCT(lwp->li_info.pr_lwp.pr_pctcpu);
	ulwp->ui_time	= TIME2SEC(lwp->li_info.pr_lwp.pr_time);
	ulwp->ui_pctmem	= FRC2PCT(lwp->li_info.pr_pctmem);
	ulwp->ui_key	= lwp->li_key;
	ulwp_total_cpu	+= ulwp->ui_pctcpu;
	ulwp_total_mem	+= ulwp->ui_pctmem;
	ulwp_cnt++;
}

static void
lwp_print()
{
	lwp_info_t *lwp;
	char usr[4], sys[4], trp[4], tfl[4];
	char dfl[4], lck[4], slp[4], lat[4];
	char vcx[4], icx[4], scl[4], sig[4];
	char psize[6], prssize[6], pstate[7], pcpu[6], ptime[12];
	char pnice[4], ppri[4];
	char pname[LOGNAME_MAX+1];
	char dash[] = " - ";
	double loadavg[3];
	int i, lwpid;

	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	(void) putp(t_ulon);
	if (opts.o_outpmode & OPT_LWPS) {
		if (opts.o_outpmode & OPT_PSINFO)
			(void) printf(LWPID_TITLE);
		if (opts.o_outpmode & OPT_USAGE)
			(void) printf(VLWPID_TITLE);
	} else {
		if (opts.o_outpmode & OPT_PSINFO)
			(void) printf(NLWP_TITLE);
		if (opts.o_outpmode & OPT_USAGE)
			(void) printf(NVLWP_TITLE);
	}
	(void) putp(t_uloff);
	(void) putp(t_eol);
	(void) putchar('\n');

	for (i = 0; i < psorted.lp_cnt; i++) {
		lwp = psorted.lp_ptrs[i];
		if (opts.o_outpmode & OPT_LWPS)
			lwpid = lwp->li_info.pr_lwp.pr_lwpid;
		else
			lwpid = lwp->li_info.pr_nlwp;
		pwd_getname(lwp->li_info.pr_uid, pname, LOGNAME_MAX + 1);
		if (opts.o_outpmode & OPT_PSINFO) {
			Format_size(psize, lwp->li_info.pr_size, 6);
			Format_size(prssize, lwp->li_info.pr_rssize, 6);
			Format_state(pstate, lwp->li_info.pr_lwp.pr_sname,
			    lwp->li_info.pr_lwp.pr_onpro, 7);
			if (strcmp(lwp->li_info.pr_lwp.pr_clname, "RT") == 0 ||
			    strcmp(lwp->li_info.pr_lwp.pr_clname, "SYS") == 0 ||
			    lwp->li_info.pr_lwp.pr_sname == 'Z')
				(void) strcpy(pnice, "  -");
			else
				Format_num(pnice,
				    lwp->li_info.pr_lwp.pr_nice-NZERO, 4);
			Format_num(ppri, lwp->li_info.pr_lwp.pr_pri, 4);
			Format_pct(pcpu,
			    FRC2PCT(lwp->li_info.pr_lwp.pr_pctcpu), 4);
			Format_time(ptime,
			    lwp->li_info.pr_lwp.pr_time.tv_sec, 10);
			if (opts.o_outpmode & OPT_TTY)
				(void) putchar('\r');
			(void) printf(LWP_LINE,
			    (int)lwp->li_info.pr_pid, pname,
			    psize, prssize, pstate, ppri, pnice,
			    ptime, pcpu, lwp->li_info.pr_fname, lwpid);
			(void) putp(t_eol);
			(void) putchar('\n');
		}
		if (opts.o_outpmode & OPT_USAGE) {
			Format_pct(usr, lwp->li_usr, 4);
			Format_pct(sys, lwp->li_sys, 4);
			Format_pct(slp, lwp->li_slp, 4);
			Format_num(vcx, lwp->li_vcx, 4);
			Format_num(icx, lwp->li_icx, 4);
			Format_num(scl, lwp->li_scl, 4);
			Format_num(sig, lwp->li_sig, 4);
			if (lwp->li_msacct & MSACCT_IS_SET) {
				Format_pct(trp, lwp->li_trp, 4);
				Format_pct(tfl, lwp->li_tfl, 4);
				Format_pct(dfl, lwp->li_dfl, 4);
				Format_pct(lck, lwp->li_lck, 4);
				Format_pct(lat, lwp->li_lat, 4);
			} else {
				(void) strcpy(trp, dash);
				(void) strcpy(tfl, dash);
				(void) strcpy(dfl, dash);
				(void) strcpy(lck, dash);
				(void) strcpy(lat, dash);
			}
			if (opts.o_outpmode & OPT_TTY)
				(void) putchar('\r');
			(void) printf(VLWP_LINE,
				(int)lwp->li_info.pr_pid, pname,
				usr, sys, trp, tfl, dfl, lck,
				slp, lat, vcx, icx, scl, sig,
				lwp->li_info.pr_fname, lwpid);
			(void) putp(t_eol);
			(void) putchar('\n');
		}
	}
	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	if (opts.o_outpmode & OPT_TERMCAP)
		while (i++ < opts.o_nprocs) {
			(void) putp(t_eol);
			(void) putchar('\n');
		}
	if (!(opts.o_outpmode & OPT_SPLIT)) {
		(void) getloadavg(loadavg, 3);
		if (opts.o_outpmode & OPT_TTY)
			(void) putchar('\r');
		(void) printf(TOTAL_LINE, total_procs, total_lwps,
		    loadavg[LOADAVG_1MIN], loadavg[LOADAVG_5MIN],
		    loadavg[LOADAVG_15MIN]);
		(void) putp(t_eol);
		(void) putchar('\n');
		if (opts.o_outpmode & OPT_TTY)
			(void) putchar('\r');
		(void) putp(t_eol);
	}
	(void) fflush(stdout);
}

static lwp_info_t *
lwp_add(pid_t pid, id_t lwpid)
{
	lwp_info_t *lwp;

	if (lwp_head == NULL) {
		lwp_head = lwp_tail = lwp = Zalloc(sizeof (lwp_info_t));
	} else {
		lwp = Zalloc(sizeof (lwp_info_t));
		lwp->li_prev = lwp_tail;
		lwp_tail->li_next = lwp;
		lwp_tail = lwp;
	}
	lwp->li_info.pr_pid = pid;
	lwp->li_info.pr_lwp.pr_lwpid = lwpid;
	lwpid_add(lwp, pid, lwpid);
	lwp_cnt++;
	return (lwp);
}

static void
lwp_remove(lwp_info_t *lwp)
{

	if (lwp->li_prev)
		lwp->li_prev->li_next = lwp->li_next;
	else
		lwp_head = lwp->li_next;	/* removing lwp_head */
	if (lwp->li_next)
		lwp->li_next->li_prev = lwp->li_prev;
	else
		lwp_tail = lwp->li_prev;	/* removing lwp_tail */
	lwpid_del(lwp->li_info.pr_pid, lwp->li_info.pr_lwp.pr_lwpid);
	if (lwpid_pidcheck(lwp->li_info.pr_pid) == 0)
		fds_rm(lwp->li_info.pr_pid);
	lwp_cnt--;
	free(lwp);
}

void
lwp_clear()
{
	lwp_info_t	*lwp = lwp_tail;
	lwp_info_t	*lwp_tmp;
	long		ctl[2] = {PCUNSET, PR_MSACCT};
	char		pfile[MAX_PROCFS_PATH];
	fd_t		*fd;
	pid_t		pid;
	id_t		lwpid;

	fd_closeall();
	while (lwp) {

		if (!(lwp->li_msacct & MSACCT_WAS_SET) &&
		    (lwp->li_msacct & MSACCT_IS_SET)) {
			pid = lwp->li_info.pr_pid;
			lwpid = lwp->li_info.pr_lwp.pr_lwpid;
			(void) sprintf(pfile, "/proc/%d/lwp/%d/lwpctl",
			    (int)pid, (int)lwpid);
			if ((fd = fd_open(pfile, O_WRONLY|O_EXCL,
			    NULL)) !=  NULL) {
				(void) pwrite(fd_getfd(fd), &ctl[0],
				    sizeof (ctl), 0);
				fd_close(fd);
			}
		}
		lwp_tmp = lwp;
		lwp = lwp->li_prev;
		lwp_remove(lwp_tmp);
	}
}

static void
lwp_update(lwp_info_t *lwp, pid_t pid, id_t lwpid, struct prusage *usage_buf)
{
	float period;

	if (!lwpid_is_active(pid, lwpid)) {
		/*
		 * If we are reading cpu times for the first time then
		 * calculate average cpu times based on whole process
		 * execution time.
		 */
		(void) memcpy(&lwp->li_usage, usage_buf, sizeof (prusage_t));
		period = TIME2NSEC(usage_buf->pr_tstamp)-
		    TIME2NSEC(usage_buf->pr_create);
		if (opts.o_outpmode & OPT_LWPS)
			period = period/(float)100;
		else
			period = (period * lwp->li_info.pr_nlwp)/(float)100;
		if (period == 0) { /* zombie */
			period = 1;
			lwp->li_usr = 0;
			lwp->li_sys = 0;
			lwp->li_slp = 0;
		} else {
			lwp->li_usr = TIME2NSEC(usage_buf->pr_utime)/period;
			lwp->li_sys = TIME2NSEC(usage_buf->pr_stime)/period;
			lwp->li_slp = TIME2NSEC(usage_buf->pr_slptime)/period;
		}
		if (lwp->li_msacct & MSACCT_IS_SET) {
			lwp->li_trp = TIME2NSEC(usage_buf->pr_ttime)/period;
			lwp->li_tfl = TIME2NSEC(usage_buf->pr_tftime)/period;
			lwp->li_dfl = TIME2NSEC(usage_buf->pr_dftime)/period;
			lwp->li_lck = TIME2NSEC(usage_buf->pr_ltime)/period;
			lwp->li_lat = TIME2NSEC(usage_buf->pr_wtime)/period;
		}
		period = (period / NANOSEC)*(float)100; /* now in seconds */
		lwp->li_vcx = opts.o_interval * (usage_buf->pr_vctx/period);
		lwp->li_icx = opts.o_interval * (usage_buf->pr_ictx/period);
		lwp->li_scl = opts.o_interval * (usage_buf->pr_sysc/period);
		lwp->li_sig = opts.o_interval * (usage_buf->pr_sigs/period);
		(void) lwpid_set_active(pid, lwpid);
	} else {
		/*
		 * If this is not a first time we are reading a process's
		 * CPU times then recalculate CPU times based on fresh data
		 * obtained from procfs and previous CPU time usage values.
		 */
		period = TIME2NSEC(usage_buf->pr_tstamp)-
		    TIME2NSEC(lwp->li_usage.pr_tstamp);
		if (opts.o_outpmode & OPT_LWPS)
			period = period/(float)100;
		else
			period = (period * lwp->li_info.pr_nlwp)/(float)100;
		if (period == 0) { /* zombie */
			period = 1;
			lwp->li_usr = 0;
			lwp->li_sys = 0;
			lwp->li_slp = 0;
		} else {
			lwp->li_usr = (TIME2NSEC(usage_buf->pr_utime)-
			    TIME2NSEC(lwp->li_usage.pr_utime))/period;
			lwp->li_sys = (TIME2NSEC(usage_buf->pr_stime) -
			    TIME2NSEC(lwp->li_usage.pr_stime))/period;
			lwp->li_slp = (TIME2NSEC(usage_buf->pr_slptime) -
			    TIME2NSEC(lwp->li_usage.pr_slptime))/period;
		}
		if (lwp->li_msacct & MSACCT_IS_SET) {
			lwp->li_trp = (TIME2NSEC(usage_buf->pr_ttime) -
			    TIME2NSEC(lwp->li_usage.pr_ttime))/period;
			lwp->li_tfl = (TIME2NSEC(usage_buf->pr_tftime) -
			    TIME2NSEC(lwp->li_usage.pr_tftime))/period;
			lwp->li_dfl = (TIME2NSEC(usage_buf->pr_dftime) -
			    TIME2NSEC(lwp->li_usage.pr_dftime))/period;
			lwp->li_lck = (TIME2NSEC(usage_buf->pr_ltime) -
			    TIME2NSEC(lwp->li_usage.pr_ltime))/period;
			lwp->li_lat = (TIME2NSEC(usage_buf->pr_wtime) -
			    TIME2NSEC(lwp->li_usage.pr_wtime))/period;
		}
		lwp->li_vcx = usage_buf->pr_vctx - lwp->li_usage.pr_vctx;
		lwp->li_icx = usage_buf->pr_ictx - lwp->li_usage.pr_ictx;
		lwp->li_scl = usage_buf->pr_sysc - lwp->li_usage.pr_sysc;
		lwp->li_sig = usage_buf->pr_sigs - lwp->li_usage.pr_sigs;
		(void) memcpy(&lwp->li_usage, usage_buf, sizeof (prusage_t));
	}
}

static void
lwp_msacct(pid_t pid, int *fds_msacct)
{
	fd_t	*fd;
	int	entsz;
	char	pfile[MAX_PROCFS_PATH];
	long	ctl[2] = {PCSET, PR_MSACCT};
	pstatus_t buf;

	if (*fds_msacct & MSACCT_CANT_SET)
		return;
	if (opts.o_outpmode & OPT_MSACCT && !(*fds_msacct & MSACCT_IS_SET)) {
		(void) snprintf(pfile, MAX_PROCFS_PATH,
			"/proc/%d/status", (int)pid);
		if ((fd = fd_open(pfile, O_RDONLY, NULL)) == NULL) {
			*fds_msacct = MSACCT_CANT_SET;
			return;
		}
		entsz = sizeof (buf);
		if (pread(fd_getfd(fd), &buf, entsz, 0) == entsz) {
			if (buf.pr_flags & PR_MSACCT)
				*fds_msacct = MSACCT_WAS_SET | MSACCT_IS_SET;
			else
				*fds_msacct = 0;
		}
		fd_close(fd);
		if (!(*fds_msacct & MSACCT_IS_SET)) {
			(void) snprintf(pfile, MAX_PROCFS_PATH,
			    "/proc/%d/ctl", (int)pid);
			if ((fd = fd_open(pfile,
			    O_WRONLY | O_EXCL, NULL)) == NULL) {
				*fds_msacct = MSACCT_CANT_SET;
				return;
			}
			if (pwrite(fd_getfd(fd), &ctl[0],
			    sizeof (ctl), 0) == sizeof (ctl))
				*fds_msacct = MSACCT_IS_SET;
			fd_close(fd);
		}
	}
}

static void
lwp_readdir(DIR *procdir)
{
	char *pidstr;
	pid_t pid;
	id_t lwpid;
	size_t entsz;
	long nlwps, nent, i;
	char *buf, *ptr;
	char pfile[MAX_PROCFS_PATH];

	fds_t *fds;
	lwp_info_t *lwp;
	dirent_t *direntp;

	prheader_t	header_buf;
	psinfo_t	psinfo_buf;
	prusage_t	usage_buf;
	lwpsinfo_t	*lwpsinfo_buf;
	prusage_t	*lwpusage_buf;

	total_procs = 0;
	total_lwps = 0;

	for (rewinddir(procdir); (direntp = readdir(procdir)); ) {
		pidstr = direntp->d_name;
		if (pidstr[0] == '.')	/* skip "." and ".."  */
			continue;
		pid = atoi(pidstr);
		if (pid == 0 || pid == 2 || pid == 3)
			continue;	/* skip sched, pageout and fsflush */
		if (has_element(&pid_tbl, pid) == 0)
			continue;	/* check if we really want this pid */
		fds = fds_get(pid);	/* get ptr to file descriptors */
		/*
		 * Here we are going to read information about
		 * current process (pid) from /proc/pid/psinfo file.
		 * If process will have more than one lwp, we also should
		 * read /proc/pid/lpsinfo for information about all lwps.
		 */
		(void) snprintf(pfile, MAX_PROCFS_PATH,
		    "/proc/%s/psinfo", pidstr);
		if ((fds->fds_psinfo = fd_open(pfile, O_RDONLY,
		    fds->fds_psinfo)) == NULL)
			continue;
		if (pread(fd_getfd(fds->fds_psinfo), &psinfo_buf,
		    sizeof (struct psinfo), 0) != sizeof (struct psinfo)) {
			fd_close(fds->fds_psinfo);
			continue;
		}

		nlwps = psinfo_buf.pr_nlwp;

		if (!has_uid(&ruid_tbl, psinfo_buf.pr_uid) ||
		    !has_uid(&euid_tbl, psinfo_buf.pr_euid))
			continue;
		if ((opts.o_outpmode & OPT_LWPS) && (nlwps > 1)) {
			(void) snprintf(pfile, MAX_PROCFS_PATH,
			    "/proc/%s/lpsinfo", pidstr);
			if ((fds->fds_lpsinfo = fd_open(pfile, O_RDONLY,
			    fds->fds_lpsinfo)) == NULL)
				continue;
			entsz = sizeof (struct prheader);
			if (pread(fd_getfd(fds->fds_lpsinfo), &header_buf,
			    entsz, 0) != entsz) {
				fd_close(fds->fds_lpsinfo);
				continue;
			}
			nent = header_buf.pr_nent;
			entsz = header_buf.pr_entsize * nent;
			ptr = buf = Malloc(entsz);
			if (pread(fd_getfd(fds->fds_lpsinfo), buf,
			    entsz, sizeof (struct prheader)) != entsz) {
				fd_close(fds->fds_lpsinfo);
				free(buf);
				continue;
			}
			for (i = 0; i < nent;
			    i++, ptr += header_buf.pr_entsize) {
				/*LINTED ALIGNMENT*/
				lwpsinfo_buf = (lwpsinfo_t *)ptr;
				if (!has_element(&cpu_tbl,
				    lwpsinfo_buf->pr_onpro) ||
				    !has_element(&set_tbl,
				    lwpsinfo_buf->pr_bindpset))
					continue;
				lwpid = lwpsinfo_buf->pr_lwpid;
				if ((lwp = lwpid_get(pid, lwpid)) == NULL)
					lwp = lwp_add(pid, lwpid);
				lwp->li_alive = TRUE;
				(void) memcpy(&lwp->li_info, &psinfo_buf,
				    sizeof (psinfo_t) - sizeof (lwpsinfo_t));
				(void) memcpy(&lwp->li_info.pr_lwp,
				    lwpsinfo_buf, sizeof (lwpsinfo_t));
			}
			free(buf);
		} else {
			if (!has_element(&cpu_tbl,
			    psinfo_buf.pr_lwp.pr_onpro) ||
			    !has_element(&set_tbl,
			    psinfo_buf.pr_lwp.pr_bindpset))
				continue;
			lwpid = psinfo_buf.pr_lwp.pr_lwpid;
			if ((lwp = lwpid_get(pid, lwpid)) == NULL)
				lwp = lwp_add(pid, lwpid);
			lwp->li_alive = TRUE;
			(void) memcpy(&lwp->li_info, &psinfo_buf,
			    sizeof (psinfo_t));
			lwp->li_info.pr_lwp.pr_pctcpu = lwp->li_info.pr_pctcpu;
		}
		if (!(opts.o_outpmode & OPT_USAGE)) {
			total_procs++;
			total_lwps += nlwps;
			continue;
		}
		/*
		 * At this part of lwp_readdir we are going to read some
		 * additional information abour processes from /proc/pid/usage
		 * file. Again, if process has more than one lwp, then we
		 * will get information about all its lwps from
		 * /proc/pid/lusage file.
		 */
		if ((opts.o_outpmode & OPT_LWPS) && (nlwps > 1)) {
			(void) snprintf(pfile, MAX_PROCFS_PATH,
			    "/proc/%s/lusage", pidstr);
			if ((fds->fds_lusage = fd_open(pfile, O_RDONLY,
			    fds->fds_lusage)) == NULL)
				continue;
			entsz = sizeof (struct prheader);
			if (pread(fd_getfd(fds->fds_lusage), &header_buf,
			    entsz, 0) != entsz) {
				fd_close(fds->fds_lusage);
				continue;
			}
			nent = header_buf.pr_nent;
			entsz = header_buf.pr_entsize * nent;
			buf = Malloc(entsz);
			if (pread(fd_getfd(fds->fds_lusage), buf,
			    entsz, sizeof (struct prheader)) != entsz) {
				fd_close(fds->fds_lusage);
				free(buf);
				continue;
			}
			lwp_msacct(pid, &fds->fds_msacct);
			for (i = 1, ptr = buf + header_buf.pr_entsize; i < nent;
			    i++, ptr += header_buf.pr_entsize) {
				/*LINTED ALIGNMENT*/
				lwpusage_buf = (prusage_t *)ptr;
				lwpid = lwpusage_buf->pr_lwpid;
				if ((lwp = lwpid_get(pid, lwpid)) == NULL)
					continue;
				lwp->li_msacct = fds->fds_msacct;
				lwp_update(lwp, pid, lwpid, lwpusage_buf);
			}
			free(buf);
		} else {
			(void) snprintf(pfile, MAX_PROCFS_PATH,
			    "/proc/%s/usage", pidstr);
			if ((fds->fds_usage = fd_open(pfile, O_RDONLY,
			    fds->fds_usage)) == NULL)
				continue;
			entsz = sizeof (prusage_t);
			if (pread(fd_getfd(fds->fds_usage), &usage_buf,
			    entsz, 0) != entsz) {
				fd_close(fds->fds_usage);
				continue;
			}
			lwpid = psinfo_buf.pr_lwp.pr_lwpid;
			if ((lwp = lwpid_get(pid, lwpid)) == NULL)
				continue;
			lwp_msacct(pid, &fds->fds_msacct);
			lwp->li_msacct = fds->fds_msacct;
			lwp_update(lwp, pid, lwpid, &usage_buf);
		}
		total_procs++;
		total_lwps += nlwps;
	}
	fd_update();
}

/*
 * This procedure removes all dead lwps from the linked list of all lwps.
 * It also creates the ulwp linked list if OPT_USERS mode was selected.
 */

static void
lwp_refresh()
{
	lwp_info_t *lwp = lwp_head;
	lwp_info_t *lwp_next;

	while (lwp) {
		if (lwp->li_alive == FALSE) {	/* lwp is dead */
			lwp_next = lwp->li_next;
			lwp_remove(lwp);
			lwp = lwp_next;
		} else {			/* lwp is alive */
			lwp->li_key = get_keyval(&psorted, lwp);
			if ((opts.o_outpmode & OPT_USERS) ||
			    (opts.o_outpmode & OPT_SPLIT))
				ulwp_update(lwp);
			lwp->li_alive = FALSE;
			lwp = lwp->li_next;
		}
	}
}

static void
curses_on()
{
	if ((opts.o_outpmode & OPT_TERMCAP) && (is_curses_on == FALSE)) {
		(void) initscr();
		(void) nonl();
		(void) putp(t_smcup);
		is_curses_on = TRUE;
	}
}

void
curses_off()
{
	if ((is_curses_on == TRUE) && (opts.o_outpmode & OPT_TERMCAP)) {
		(void) putp(t_rmcup);
		(void) endwin();
		is_curses_on = FALSE;
	}
	(void) fflush(stdout);
}

static int
nlines()
{
	struct winsize ws;
	char *envp;
	int n;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
		if (ws.ws_row > 0)
			return (ws.ws_row);
	}
	if (envp = getenv("LINES")) {
		if ((n = Atoi(envp)) > 0) {
			opts.o_outpmode &= ~OPT_USEHOME;
			return (n);
		}
	}
	return (-1);
}

static void
setmovecur()
{
	int i, n;
	if ((opts.o_outpmode & OPT_FULLSCREEN) &&
	    (opts.o_outpmode & OPT_USEHOME)) {
		movecur = tigetstr("home");
		if (movecur != NULL)
			return;
	}
	if (opts.o_outpmode & OPT_SPLIT) {
		n = opts.o_nprocs + opts.o_nusers + 2;
	} else {
		if (opts.o_outpmode & OPT_USERS)
			n = opts.o_nusers + 1;
		else
			n = opts.o_nprocs + 1;
	}
	if (movecur)
		free(movecur);
	movecur = Zalloc(strlen(t_up) * (n + 5));
	for (i = 0; i <= n; i++)
		(void) strcat(movecur, t_up);
}

static int
setsize()
{
	static int oldn = 0;
	int n;

	if (opts.o_outpmode & OPT_FULLSCREEN) {
		n = nlines();
		if (n == oldn)
			return (0);
		oldn = n;
		if (n == -1) {
			opts.o_outpmode &= ~OPT_USEHOME;
			setmovecur();		/* set default window size */
			return (1);
		}
		n = n - 3;	/* minus header, total and cursor lines */
		if (n < 1)
			Die(gettext("window is too small (try -n)\n"));
		if (opts.o_outpmode & OPT_SPLIT) {
			if (n < 8) {
				Die(gettext("window is too small (try -n)\n"));
			} else {
				opts.o_nprocs = (n / 4) * 3;
				opts.o_nusers = n - 1 - opts.o_nprocs;
			}
		} else {
			if (opts.o_outpmode & OPT_USERS)
				opts.o_nusers = n;
			else
				opts.o_nprocs = n;
		}
	}
	setmovecur();
	return (1);
}

static void
ldtermcap()
{
	int err;
	if (setupterm(NULL, STDIN_FILENO, &err) == ERR) {
		switch (err) {
		case 0:
			Die(gettext("failed to load terminal info\n"));
			/*NOTREACHED*/
		case -1:
			Die(gettext("terminfo database not found\n"));
			/*NOTREACHED*/
		default:
			Die(gettext("failed to initialize terminal\n"));
		}
	}
	t_ulon	= tigetstr("smul");
	t_uloff	= tigetstr("rmul");
	t_up	= tigetstr("cuu1");
	t_eol	= tigetstr("el");
	t_smcup	= tigetstr("smcup");
	t_rmcup = tigetstr("rmcup");
	if ((t_up == (char *)-1) || (t_eol == (char *)-1) ||
	    (t_smcup == (char *)-1) || (t_rmcup == (char *)-1)) {
		opts.o_outpmode &= ~OPT_TERMCAP;
		t_up = t_eol = t_smcup = t_rmcup = movecur = empty_string;
		return;
	}
	if (t_up == NULL || t_eol == NULL) {
		opts.o_outpmode &= ~OPT_TERMCAP;
		t_eol = t_up = movecur = empty_string;
		return;
	}
	if (t_ulon == (char *)-1 || t_uloff == (char *)-1 ||
	    t_ulon == NULL || t_uloff == NULL) {
		t_ulon = t_uloff = empty_string;  /* can live without it */
	}
	if (t_smcup == NULL || t_rmcup == NULL)
		t_smcup = t_rmcup = empty_string;
}

static void
sig_handler(int sig)
{
	switch (sig) {
	case SIGTSTP:	sigtstp = 1;
			break;
	case SIGWINCH:	sigwinch = 1;
			break;
	case SIGINT:
	case SIGTERM:	sigterm = 1;
			break;
	}
}

static void
set_signals()
{
	(void) signal(SIGTSTP, sig_handler);
	(void) signal(SIGINT, sig_handler);
	(void) signal(SIGTERM, sig_handler);
	if (opts.o_outpmode & OPT_FULLSCREEN)
		(void) signal(SIGWINCH, sig_handler);
}

int
main(int argc, char **argv)
{
	DIR *procdir;
	char *p;
	char *sortk = "cpu";	/* default sort key */
	long l;
	int opt;
	int timeout;
	struct pollfd fds;
	char key;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	Progname(argv[0]);
	lwpid_init();
	fd_init(Setrlimit());

	while ((opt = getopt(argc, argv, "vcmaRLtu:U:n:p:C:P:s:S:"))
	    != (int)EOF) {
		switch (opt) {
		case 'R':
			opts.o_outpmode |= OPT_REALTIME;
			break;
		case 'c':
			opts.o_outpmode &= ~OPT_TERMCAP;
			opts.o_outpmode &= ~OPT_FULLSCREEN;
			break;
		case 'm':
			opts.o_outpmode &= ~OPT_PSINFO;
			opts.o_outpmode |= OPT_USAGE | OPT_MSACCT;
			break;
		case 'a':
			opts.o_outpmode |= OPT_SPLIT;
			break;
		case 'n':
			p = strtok(optarg, ",");
			opts.o_nprocs = Atoi(p);
			if (p = strtok(NULL, ","))
				opts.o_nusers = Atoi(p);
			opts.o_outpmode &= ~OPT_FULLSCREEN;
			break;
		case 's':
			opts.o_sortorder = -1;
			sortk = optarg;
			break;
		case 'S':
			opts.o_sortorder = 1;
			sortk = optarg;
			break;
		case 'v':
			opts.o_outpmode &= ~OPT_PSINFO;
			opts.o_outpmode |= OPT_USAGE;
			break;
		case 'u':
			p = strtok(optarg, ", ");
			add_uid(&euid_tbl, p);
			while (p = strtok(NULL, ", "))
				add_uid(&euid_tbl, p);
			break;
		case 'U':
			p = strtok(optarg, ", ");
			add_uid(&ruid_tbl, p);
			while (p = strtok(NULL, ", "))
				add_uid(&ruid_tbl, p);
			break;
		case 't':
			opts.o_outpmode &= ~OPT_PSINFO;
			opts.o_outpmode |= OPT_USERS;
			break;
		case 'p':
			p = strtok(optarg, ", ");
			l = Atoi(p);
			add_element(&pid_tbl, l);
			while (p = strtok(NULL, ", ")) {
				l = Atoi(p);
				add_element(&pid_tbl, l);
			}
			break;
		case 'C':
			p = strtok(optarg, ", ");
			l = Atoi(p);
			add_element(&set_tbl, l);
			while (p = strtok(NULL, ", ")) {
				l = Atoi(p);
				add_element(&set_tbl, l);
			}
			break;
		case 'P':
			p = strtok(optarg, ", ");
			l = Atoi(p);
			add_element(&cpu_tbl, l);
			while (p = strtok(NULL, ", ")) {
				l = Atoi(p);
				add_element(&cpu_tbl, l);
			}
			break;
		case 'L':
			opts.o_outpmode |= OPT_LWPS;
			break;
		default:
			Usage();
		}
	}

	(void) atexit(Exit);
	if (opts.o_outpmode & OPT_USERS && !(opts.o_outpmode & OPT_SPLIT))
		opts.o_nusers = opts.o_nprocs;
	if (opts.o_nprocs == 0 || opts.o_nusers == 0)
		Die(gettext("invalid argument for -n\n"));
	if ((opts.o_outpmode & OPT_USERS) &&
	    (opts.o_outpmode & (OPT_SPLIT | OPT_USAGE)))
		Die(gettext("-t option cannot be used with -v, -m or -a\n"));
	if (argc > optind)
		opts.o_interval = Atoi(argv[optind++]);
	if (argc > optind)
		opts.o_count = Atoi(argv[optind++]);
	if (opts.o_count == 0)
		Die(gettext("invalid counter value\n"));
	if (argc > optind)
		Usage();
	if (isatty(STDOUT_FILENO) == 1)
		opts.o_outpmode |= OPT_TTY;
	if (!(opts.o_outpmode & OPT_TTY))	/* no termcap for files */
		opts.o_outpmode &= ~OPT_TERMCAP;
	if (opts.o_outpmode & OPT_TERMCAP) {
		ldtermcap();
		(void) setsize();
		curses_on();
	}
	list_alloc(&psorted, opts.o_nprocs);
	list_set_keyfunc(sortk, &opts, &psorted);
	list_alloc(&usorted, opts.o_nusers);
	list_set_keyfunc(NULL, &opts, &usorted);
	if (opts.o_outpmode & OPT_REALTIME)
		Priocntl("RT");
	if ((procdir = opendir("/proc")) == NULL)
		Die(gettext("cannot open /proc directory\n"));
	if (opts.o_outpmode & OPT_TTY)
		(void) printf(gettext("Please wait...\r"));
	(void) fflush(stdout);
	set_signals();
	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;
	timeout = opts.o_interval * MILLISEC;

	/*
	 * main program loop
	 */
	do {
		if (sigterm == 1)
			break;
		if (sigtstp == 1) {
			curses_off();
			(void) signal(SIGTSTP, SIG_DFL);
			(void) kill(0, SIGTSTP);
			/*
			 * prstat stops here until it receives SIGCONT signal.
			 */
			sigtstp = 0;
			(void) signal(SIGTSTP, sig_handler);
			curses_on();
			print_movecur = FALSE;
			if (opts.o_outpmode & OPT_FULLSCREEN)
				sigwinch = 1;
		}
		if (sigwinch == 1) {
			if (setsize() == 1) {
				list_free(&psorted);
				list_free(&usorted);
				list_alloc(&psorted, opts.o_nprocs);
				list_alloc(&usorted, opts.o_nusers);
			}
			sigwinch = 0;
			(void) signal(SIGWINCH, sig_handler);
		}
		lwp_readdir(procdir);
		lwp_refresh();
		if (print_movecur)
			(void) putp(movecur);
		print_movecur = TRUE;
		if ((opts.o_outpmode & OPT_PSINFO) ||
		    (opts.o_outpmode & OPT_USAGE)) {
			sort_lwps(&psorted, lwp_head);
			lwp_print();
		}
		if ((opts.o_outpmode & OPT_USERS) ||
		    (opts.o_outpmode & OPT_SPLIT)) {
			sort_ulwps(&usorted, ulwp_head);
			ulwp_print();
			ulwp_clear();
		}
		if (opts.o_count == 1)
			break;
		/*
		 * If poll() returns -1 and sets errno to EINTR here because
		 * the process received a signal, it is Ok to abort this
		 * timeout and loop around because we check the signals at the
		 * top of the loop.
		 */
		if (poll(&fds, (nfds_t)1, timeout) > 0) {
			if (read(STDIN_FILENO, &key, 1) == 1) {
				if (tolower(key) == 'q')
					break;
			}
		}
	} while (opts.o_count == (-1) || --opts.o_count);

	if (opts.o_outpmode & OPT_TTY)
		(void) putchar('\r');
	list_free(&psorted);
	list_free(&usorted);
	return (0);
}
