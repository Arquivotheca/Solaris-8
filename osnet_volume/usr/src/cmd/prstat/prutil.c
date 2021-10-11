/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prutil.c	1.2	99/09/08 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/priocntl.h>
#include <sys/rtpriocntl.h>
#include <sys/tspriocntl.h>

#include <libintl.h>
#include <wchar.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <poll.h>

#include "prfile.h"
#include "prstat.h"
#include "prutil.h"

static char PRG_FMT[] = "%s: ";
static char ERR_FMT[] = ": %s\n";
static char *progname;

void
Exit()
{
	curses_off();
	lwp_clear();
	ulwp_clear();
	fd_exit();
}

static void
Warn(char *format, ...)
{
	int err = errno;
	va_list alist;

	if (progname != NULL)
		(void) fprintf(stderr, PRG_FMT, progname);
	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);
	if (strchr(format, '\n') == NULL)
		(void) fprintf(stderr, gettext(ERR_FMT), strerror(err));
}

void
Die(char *format, ...)
{
	int err = errno;
	va_list alist;

	if (progname != NULL)
		(void) fprintf(stderr, PRG_FMT, progname);
	va_start(alist, format);
	(void) vfprintf(stderr, format, alist);
	va_end(alist);
	if (strchr(format, '\n') == NULL)
		(void) fprintf(stderr, gettext(ERR_FMT), strerror(err));
	exit(1);
}

void
Progname(char *arg0)
{
	char *p = strrchr(arg0, '/');
	if (p == NULL)
		p = arg0;
	else
		p++;
	progname = p;
}

void
Usage()
{
	(void) fprintf(stderr, gettext(
	    "Usage:\tprstat [-acLmRtv] [-u euidlist] [-U uidlist]\n"
	    "\t[-p pidlist] [-P cpulist] [-C psrsetlist]\n"
	    "\t[-s key | -S key] [-n nprocs[,nusers]]\n"
	    "\t[interval [counter]]\n"));
	exit(1);
}

int
Atoi(char *p)
{
	int i;
	char *q;
	errno = 0;
	i = (int)strtol(p, &q, 10);
	if (errno != 0 || q == p || i < 0 || *q != '\0')
		Die(gettext("illegal argument -- %s\n"), p);
		/*NOTREACHED*/
	else
		return (i);
}

void
Format_size(char *str, size_t size, int length)
{
	char tag = 'K';
	if (size >= 10000) {
		size = (size + 512) / 1024;
		tag = 'M';
		if (size >= 10000) {
			size = (size + 512) / 1024;
			tag = 'G';
		}
	}
	(void) snprintf(str, length, "%4d%c", (int)size, tag);
}

void
Format_time(char *str, ulong_t time, int length)
{
	(void) snprintf(str, length, gettext("%3d:%2.2d.%2.2d"), /* hr:m.s */
	    (int)time/3600, (int)(time % 3600)/60, (int)time % 60);
}

void
Format_pct(char *str, float val, int length)
{
	if (val > (float)100)
		val = 100;
	if (val < 0)
		val = 0;

	if (val < (float)9.95)
		(void) snprintf(str, length, "%1.1f", val);
	else
		(void) snprintf(str, length, "%.0f", val);
}

void
Format_num(char *str, int num, int length)
{
	if (num >= 100000) {
		(void) snprintf(str, length, ".%1dM", num/100000);
	} else {
		if (num >= 1000)
			(void) snprintf(str, length, "%2dK", num/1000);
		else
			(void) snprintf(str, length, "%3d", num);
	}
}

void
Format_state(char *str, char state, processorid_t pr_id, int length)
{
	switch (state) {
	case 'S':
		(void) strncpy(str, "sleep", length);
		break;
	case 'R':
		(void) strncpy(str, "run", length);
		break;
	case 'Z':
		(void) strncpy(str, "zombie", length);
		break;
	case 'T':
		(void) strncpy(str, "stop", length);
		break;
	case 'I':
		(void) strncpy(str, "idle", length);
		break;
	case 'X':
		(void) strncpy(str, "xbrk", length);
		break;
	case 'O':
		(void) snprintf(str, length, "cpu%-3d", (int)pr_id);
		break;
	default:
		(void) strncpy(str, "?", length);
		break;
	}
}

void *
Realloc(void *ptr, size_t size)
{
	int	cnt = 0;
	void	*sav = ptr;

eagain:	if ((ptr = realloc(ptr, size)))
		return (ptr);

	if ((++cnt <= 3) && (errno == EAGAIN)) {
		Warn(gettext("realloc() failed, attempt %d"), cnt);
		(void) poll(NULL, 0, 5000); /* wait for 5 seconds */
		ptr = sav;
		goto eagain;
	}
	ptr = sav;
	Die(gettext("not enough memory"));
	/*NOTREACHED*/
}

void *
Malloc(size_t size)
{
	return (Realloc(NULL, size));
}

void *
Zalloc(size_t size)
{
	return (memset(Realloc(NULL, size), 0, size));
}

int
Setrlimit()
{
	struct rlimit rlim;
	int fd_limit;
	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
		Die(gettext("getrlimit failed"));
	fd_limit = rlim.rlim_cur;
	rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == -1)
		return (fd_limit);
	else
		return (rlim.rlim_cur);
}                   

void
Priocntl(char *class)
{
	pcinfo_t pcinfo;
	pcparms_t pcparms;
	(void) strcpy(pcinfo.pc_clname, class);
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1) {
		Warn(gettext("cannot get real time class parameters"));
		return;
	}
	pcparms.pc_cid = pcinfo.pc_cid;
	((rtparms_t *)pcparms.pc_clparms)->rt_pri = 0;
	((rtparms_t *)pcparms.pc_clparms)->rt_tqsecs = 0;
	((rtparms_t *)pcparms.pc_clparms)->rt_tqnsecs = RT_NOCHANGE;
	if (priocntl(P_PID, getpid(), PC_SETPARMS, (caddr_t)&pcparms) == -1)
		Warn(gettext("cannot enter the real time class"));
}
