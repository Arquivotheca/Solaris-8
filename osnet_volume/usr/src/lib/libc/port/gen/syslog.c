/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)syslog.c	1.37	99/07/26 SMI"
/*LINTLIBRARY*/
/* from "syslog.c 1.18 88/02/08 SMI"; from UCB 5.9 5/7/86 */

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	    All rights reserved.
 */

/*
 * SYSLOG -- print message on log file
 *
 * This routine looks a lot like printf, except that it
 * outputs to the log file instead of the standard output.
 * Also:
 *	adds a timestamp,
 *	prints the module name in front of the message,
 *	has some other formatting types (or will sometime),
 *	adds a newline on the end of the message.
 *
 * The output of this routine is intended to be read by /etc/syslogd.
 */

#ifndef DSHLIB
#pragma weak syslog = _syslog
#pragma weak vsyslog = _vsyslog
#pragma weak openlog = _openlog
#pragma weak closelog = _closelog
#pragma weak setlogmask = _setlogmask
#endif

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <sys/types32.h>
#include <sys/mman.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/log.h>		/* for LOG_MAXPS */
#include <stdlib.h>
#include <procfs.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <sys/door.h>
#include <sys/stat.h>
#include <stropts.h>
#include <sys/wait.h>

#define	MAXLINE		1024		/* max message size (but see below) */

#define	PRIMASK(p)	(1 << ((p) & LOG_PRIMASK))
#define	PRIFAC(p)	(((p) & LOG_FACMASK) >> 3)
#define	IMPORTANT 	LOG_ERR

#define	FALSE 	0
#define	TRUE	1
#define	logname		"/dev/conslog"
#define	ctty		"/dev/syscon"
#define	sysmsg		"/dev/sysmsg"

#define	DOORFILE	"/var/run/syslog_door"

static struct __syslog {
	int	_LogFile;
	int	_LogStat;
	const char	*_LogTag;
	int	_LogMask;
	char	*_SyslogHost;
	int	_LogFacility;
	int	_LogFileInvalid;
	int	_OpenLogCalled;
	dev_t   _LogDev;
	char	_ProcName[PRFNSZ + 1];
} *__syslog;

#define	LogFile (__syslog->_LogFile)
#define	LogStat (__syslog->_LogStat)
#define	LogTag (__syslog->_LogTag)
#define	LogMask (__syslog->_LogMask)
#define	SyslogHost (__syslog->_SyslogHost)
#define	LogFacility (__syslog->_LogFacility)
#define	LogFileInvalid (__syslog->_LogFileInvalid)
#define	OpenLogCalled (__syslog->_OpenLogCalled)
#define	LogDev (__syslog->_LogDev)
#define	ProcName (__syslog->_ProcName)

static int syslogd_ok(void);

extern int _door_info(int, door_info_t *);
extern int _door_call(int did, door_arg_t *arg);

#ifdef _REENTRANT
static mutex_t _syslog_lk = DEFAULTMUTEX;
#endif _REENTRANT

static int
allocstatic(void)
{
	(void) _mutex_lock(&_syslog_lk);
	if (__syslog != 0) {
		(void) _mutex_unlock(&_syslog_lk);
		return (1);
	}
	__syslog = calloc(1, sizeof (struct __syslog));
	if (__syslog == 0) {
		(void) _mutex_unlock(&_syslog_lk);
		return (0);	/* can't do it */
	}
	LogFile = -1;		/* fd for log */
	LogStat = 0;		/* status bits, set by openlog() */
	LogTag = "syslog";	/* string to tag the entry with */
	LogMask = 0xff;		/* mask of priorities to be logged */
	LogFacility = LOG_USER;	/* default facility code */
	LogFileInvalid = FALSE; /* check for validity of fd for log */
	OpenLogCalled = 0;	/* openlog has not yet been called */
	(void) _mutex_unlock(&_syslog_lk);
	return (1);
}

/*VARARGS2*/
void
syslog(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}


void
vsyslog(int pri, const char *fmt, va_list ap)
{
	char *b, *f, *o;
	char c;
	int clen;
	char buf[MAXLINE + 2];
	char outline[MAXLINE + 256];  /* pad to allow date, system name... */
	time_t now;
	pid_t pid;
	struct log_ctl hdr;
	struct strbuf dat;
	struct strbuf ctl;
	sigset_t sigs;
	sigset_t osigs;
	char timestr[26];	/* hardwired value 26 due to Posix */
	size_t taglen;
	int olderrno = errno;
	struct stat statbuff;
	int procfd;
	char procfile[32];
	psinfo_t p;
	int showpid;
	uint32_t msgid;
	char *msgid_start, *msgid_end;

/*
 * Maximum tag length is 256 (the pad in outline) minus the size of the
 * other things that can go in the pad.
 */
#define	MAX_TAG		230

	if (__syslog == 0 && !allocstatic())
		return;
	/* see if we should just throw out this message */
	if (pri < 0 || PRIFAC(pri) >= LOG_NFACILITIES ||
	    (PRIMASK(pri) & LogMask) == 0)
		return;

	if (LogFileInvalid)
		return;

	/*
	 * if openlog() has not been called by the application,
	 * try to get the name of the application and set it
	 * as the ident string for messages. If unable to get
	 * it for any reason, fall back to using the default
	 * of syslog. If we succeed in getting the name, also
	 * turn on LOG_PID, to provide greater detail.
	 */
	showpid = 0;
	if (OpenLogCalled == 0) {
		(void) sprintf(procfile, "/proc/%d/psinfo", getpid());
		if ((procfd = open(procfile, O_RDONLY)) >= 0) {
			if (read(procfd, &p, sizeof (psinfo_t)) >= 0) {
				(void) strncpy(ProcName, p.pr_fname, PRFNSZ);
				LogTag = (const char *) &ProcName;
				showpid = LOG_PID;
			}
			close(procfd);
		}
	}
	if (LogFile < 0)
		openlog(LogTag, LogStat|LOG_NDELAY|showpid, 0);

	if ((fstat(LogFile, &statbuff) != 0) ||
	    (!S_ISCHR(statbuff.st_mode)) || (statbuff.st_rdev != LogDev)) {
		LogFileInvalid = TRUE;
		return;
	}

	/* set default facility if none specified */
	if ((pri & LOG_FACMASK) == 0)
		pri |= LogFacility;

	/* build the header */
	hdr.pri = pri;
	hdr.flags = SL_CONSOLE;
	hdr.level = 0;

	/* build the message */
	/*
	 * To avoid potential security problems, bounds checking is done
	 * on outline and buf.
	 * The following code presumes that the header information will
	 * fit in 250-odd bytes, as was accounted for in the buffer size
	 * allocation.  This is dependent on the assumption that the LogTag
	 * and the string returned by sprintf() for getpid() will return
	 * be less than 230-odd characters combined.
	 */
	o = outline;
	(void) time(&now);
	(void) sprintf(o, "%.15s ", ctime_r(&now, timestr, 26) + 4);
	o += strlen(o);

	if (LogTag) {
		taglen = strlen(LogTag) < MAX_TAG ? strlen(LogTag) : MAX_TAG;
		(void) strncpy(o, LogTag, taglen);
		o[taglen] = '\0';
		o += strlen(o);
	}
	if (LogStat & LOG_PID) {
		(void) sprintf(o, "[%d]", getpid());
		o += strlen(o);
	}
	if (LogTag) {
		(void) strcpy(o, ": ");
		o += 2;
	}

	STRLOG_MAKE_MSGID(fmt, msgid);
	o += sprintf(o, "[ID %u FACILITY_AND_PRIORITY] ", msgid);

	b = buf;
	f = (char *)fmt;
	while ((c = *f++) != '\0' && b < &buf[MAXLINE]) {
		char *errmsg;
		if (c != '%') {
			*b++ = c;
			continue;
		}
		if ((c = *f++) != 'm') {
			*b++ = '%';
			*b++ = c;
			continue;
		}
		if ((errmsg = strerror(olderrno)) == NULL)
			(void) snprintf(b, &buf[MAXLINE] - b, "error %d",
			    olderrno);
		else {
			while (*errmsg != '\0' && b < &buf[MAXLINE]) {
				if (*errmsg == '%') {
					(void) strcpy(b, "%%");
					b += 2;
				}
				else
					*b++ = *errmsg;
				errmsg++;
			}
			*b = '\0';
		}
		b += strlen(b);
	}
	if (b > buf && *(b-1) != '\n')	/* ensure at least one newline */
		*b++ = '\n';
	*b = '\0';
	(void) vsnprintf(o, &outline[sizeof (outline)] - o, buf, ap);
	clen  = (int)strlen(outline) + 1;	/* add one for NULL byte */
	if (clen > MAXLINE) {
		clen = MAXLINE;
		outline[MAXLINE-1] = '\0';
	}

	/*
	 * 1136432 points out that the underlying log driver actually
	 * refuses to accept (ERANGE) messages longer than LOG_MAXPS
	 * bytes.  So it really doesn't make much sense to putmsg a
	 * longer message..
	 */
	if (clen > LOG_MAXPS) {
		clen = LOG_MAXPS;
		outline[LOG_MAXPS-1] = '\0';
	}

	/* set up the strbufs */
	ctl.maxlen = sizeof (struct log_ctl);
	ctl.len = sizeof (struct log_ctl);
	ctl.buf = (caddr_t)&hdr;
	dat.maxlen = sizeof (outline);
	dat.len = clen;
	dat.buf = outline;

	/* output the message to the local logger */
	if ((putmsg(LogFile, &ctl, &dat, 0) >= 0) && syslogd_ok())
		return;
	if (!(LogStat & LOG_CONS))
		return;

	/*
	 * Output the message to the console directly.  To reduce visual
	 * clutter, we strip out the message ID.
	 */
	if ((msgid_start = strstr(outline, "[ID ")) != NULL &&
	    (msgid_end = strstr(msgid_start, "] ")) != NULL)
		(void) strcpy(msgid_start, msgid_end + 2);

	clen = strlen(outline) + 1;

	sigemptyset(&sigs);
	sigaddset(&sigs, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
	pid = fork();
	if (pid == -1) {
		sigprocmask(SIG_SETMASK, &osigs, NULL);
		return;
	}
	if (pid == 0) {
		int fd;

		(void) signal(SIGALRM, SIG_DFL);
		(void) sigprocmask(SIG_BLOCK, NULL, &sigs);
		(void) sigdelset(&sigs, SIGALRM);
		(void) sigprocmask(SIG_SETMASK, &sigs, NULL);
		(void) alarm(5);
		if (((fd = open(sysmsg, O_WRONLY)) >= 0) ||
		    (fd = open(ctty, O_WRONLY)) >= 0) {
			(void) alarm(0);
			outline[clen - 1] = '\r';
			(void) write(fd, outline, clen);
			(void) close(fd);
		}
		_exit(0);
	}
	/*
	 * _WNOCHLD should be automatic, but Posix doesn't say that.
	 */
	if (!(LogStat & LOG_NOWAIT))
		waitpid(pid, (int *)0, _WNOCHLD);
	sigprocmask(SIG_SETMASK, &osigs, NULL);
}

/*
 * Use a door call to syslogd to see if it's alive.
 */
static int
syslogd_ok(void)
{
	int d;
	int s;
	door_arg_t darg;
	door_info_t info;

	if ((d = open(DOORFILE, O_RDONLY)) < 0)
		return (0);
	/*
	 * see if our pid matches the pid of the door server.
	 * If so, syslogd has called syslog(), probably as
	 * a result of some name service library error, and
	 * we don't want to let syslog continue and possibly
	 * fork here.
	 */
	info.di_target = 0;
	if (__door_info(d, &info) < 0 || info.di_target == getpid()) {
		(void) close(d);
		return (0);
	}
	darg.data_ptr = NULL;
	darg.data_size = 0;
	darg.desc_ptr = NULL;
	darg.desc_num = 0;
	darg.rbuf = NULL;
	darg.rsize = 0;
	s = __door_call(d, &darg);
	(void) close(d);
	if (s < 0)
		return (0);		/* failure - syslogd dead */
	else
		return (1);
}

/*
 * OPENLOG -- open system log
 */

void
openlog(const char *ident, int logstat, int logfac)
{
	struct	stat	statbuff;

	if (__syslog == 0 && !allocstatic())
		return;
	OpenLogCalled = 1;
	if (ident != NULL)
		LogTag = ident;
	LogStat = logstat;
	if (logfac != 0)
		LogFacility = logfac & LOG_FACMASK;

	/*
	 * if the fstat(2) fails or the st_rdev has changed
	 * then we must open the file
	 */
	if ((fstat(LogFile, &statbuff) == 0) &&
	    (S_ISCHR(statbuff.st_mode)) && (statbuff.st_rdev == LogDev))
		return;

	if (LogStat & LOG_NDELAY) {
		LogFile = open(logname, O_WRONLY);
		(void) fcntl(LogFile, F_SETFD, 1);
		(void) fstat(LogFile, &statbuff);
		LogDev = statbuff.st_rdev;
	}
}

/*
 * CLOSELOG -- close the system log
 */

void
closelog(void)
{
	struct	stat	statbuff;

	if (__syslog == 0)
		return;

	OpenLogCalled = 0;

	/* if the LogFile is invalid it can not be closed */
	if (LogFileInvalid)
		return;

	/*
	 * if the fstat(2) fails or the st_rdev has changed
	 * then we can not close the file
	 */
	if ((fstat(LogFile, &statbuff) == 0) && (statbuff.st_rdev == LogDev)) {
		(void) close(LogFile);
		LogFile = -1;
		LogStat = 0;
	}
}

/*
 * SETLOGMASK -- set the log mask level
 */
int
setlogmask(int pmask)
{
	int omask = 0;

	if (__syslog == 0 && !allocstatic())
		return (omask);
	omask = LogMask;
	if (pmask != 0)
		LogMask = pmask;
	return (omask);
}
