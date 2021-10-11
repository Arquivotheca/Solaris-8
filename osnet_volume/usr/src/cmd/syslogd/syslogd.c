/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	All Rights Reserved
 *
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)syslogd.c	1.84	99/10/27 SMI"

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reconfigure.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages.
 * DEFSPRI -- the default priority for kernel messages.
 *
 */

#include <unistd.h>
#include <note.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <netconfig.h>
#include <netdir.h>
#include <pwd.h>
#include <tiuser.h>
#include <utmpx.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>
#include <stropts.h>
#include <assert.h>

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/note.h>
#include <door.h>

#include <wchar.h>
#include <wctype.h>
#include <widec.h>
#include <locale.h>

#include "dataq.h"
#include "conf.h"
#include "syslogd.h"

#define	DOORFILE	"/var/run/syslog_door"
#define	OLDDOORFILE	"/etc/.syslog_door"

static char		*Version = "1.84";
static char		*LogName = "/dev/log";
static char		*ConfFile = "/etc/syslog.conf";
static char		*PidFile = "/etc/syslog.pid";
static char		ctty[] = "/dev/console";
static char		sysmsg[] = "/dev/sysmsg";
/*
 * configuration file directives
 */
static struct code	PriNames[] = {
	"panic",	LOG_EMERG,
	"emerg",	LOG_EMERG,
	"alert",	LOG_ALERT,
	"crit",		LOG_CRIT,
	"err",		LOG_ERR,
	"error",	LOG_ERR,
	"warn",		LOG_WARNING,
	"warning",	LOG_WARNING,
	"notice",	LOG_NOTICE,
	"info",		LOG_INFO,
	"debug",	LOG_DEBUG,
	"none",		NOPRI,
	NULL,		-1
};

static struct code	FacNames[] = {
	"kern",		LOG_KERN,
	"user",		LOG_USER,
	"mail",		LOG_MAIL,
	"daemon",	LOG_DAEMON,
	"auth",		LOG_AUTH,
	"security",	LOG_AUTH,
	"mark",		LOG_MARK,
	"syslog",	LOG_SYSLOG,
	"lpr",		LOG_LPR,
	"news",		LOG_NEWS,
	"uucp",		LOG_UUCP,
	"cron",		LOG_CRON,
	"local0",	LOG_LOCAL0,
	"local1",	LOG_LOCAL1,
	"local2",	LOG_LOCAL2,
	"local3",	LOG_LOCAL3,
	"local4",	LOG_LOCAL4,
	"local5",	LOG_LOCAL5,
	"local6",	LOG_LOCAL6,
	"local7",	LOG_LOCAL7,
	NULL,		-1
};

static char		*TypeNames[7] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL"
};

/*
 * we allocate our own thread stacks so we can create them
 * without the MAP_NORESERVE option. We need to be sure
 * we have stack space even if the machine runs out of swap
 */

#define	DEFAULT_STACKSIZE (100 * 1024)  /* 100 k stack */
#define	DEFAULT_REDZONESIZE (8 * 1024)	/* 8k redzone */

static pthread_mutex_t wmp = PTHREAD_MUTEX_INITIALIZER;	/* wallmsg lock */

static pthread_mutex_t cft = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t log_threads_configured = PTHREAD_COND_INITIALIZER;
static int conf_threads = 0;

static pthread_mutex_t hup_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hup_done = PTHREAD_COND_INITIALIZER;

static size_t stacksize;		/* thread stack size */
static size_t redzonesize;		/* thread stack redzone size */
static char *stack_ptr;			/* ptr to allocated stacks */
static char *cstack_ptr;		/* ptr to conf_thr stacks */

static time_t start_time;

static pthread_t sys_thread;		/* queues messages from us */
static pthread_t net_thread;		/* queues messages from the net */
static pthread_t log_thread;		/* message processing thread */

static dataq_t inputq;			/* the input queue */

static struct filed fallback[2];
static struct filed *Files;
static int nlogs;
static int Debug;			/* debug flag */
static host_list_t LocalHostName;	/* our hostname */
static host_list_t NullHostName;	/* in case of lookup failure */
static char mynumericaddr[SYS_NMLN+1];
static int debuglev = 1;		/* debug print level */

static int Initialized;			/* set when initalized */
static int MarkInterval = 20;		/* interval between marks (mins) */
static int Marking = 0;			/* non-zero if marking some file */
static int Ninputs = 0;			/* number of network inputs */
static int curalarm = 0;		/* current timeout value (secs) */
static int sys_msg_count = 0;		/* total msgs rcvd from local log */
static int net_msg_count = 0;		/* total msgs rcvd from net */

static struct pollfd *Nfd;		/* network poll descriptors */
static struct netconfig *Ncf;
static struct netbuf **Myaddrs;
static struct t_unitdata **Udp;
static struct t_uderr **Errp;
static int turnoff = 0;

#define	DPRINT0(d, m)		if ((Debug) && debuglev >= (d)) \
				(void) fprintf(stderr, m)
#define	DPRINT1(d, m, a)	if ((Debug) && debuglev >= (d)) \
				(void) fprintf(stderr, m, a)
#define	DPRINT2(d, m, a, b)	if ((Debug) && debuglev >= (d)) \
				(void) fprintf(stderr, m, a, b)
#define	DPRINT3(d, m, a, b, c)	if ((Debug) && debuglev >= (d)) \
				(void) fprintf(stderr, m, a, b, c)
#define	MALLOC_FAIL_EXIT	\
		(void) fprintf(stderr, "malloc failed - fatal\n"); \
		exit(1)

/*
 * Number of seconds to wait before giving up on threads that won't
 * shutdown: (that's right, 10 minutes!)
 */

#define	LOOP_MAX (10 * 60)
/*
 * To use bound threads, uncomment the following
 */
/* #define USE_BOUND_THREADS */
int
main(int argc, char **argv)
{
	int i;
	rlim_t r;
	char *pstr;
	void *p;
	int sig, fd;
	char buf[100];
	sigset_t sigs, allsigs;
	struct rlimit rlim;
	char *debugstr;
	int mcount = 0, loop = LOOP_MAX;
	struct filed *f;

#ifdef DEBUG
#define	DEBUGDIR "/var/tmp"
	if (chdir(DEBUGDIR))
		DPRINT1(1, "Unable to cd to %s\n", DEBUGDIR);
#endif /* DEBUG */

	(void) setlocale(LC_ALL, "");

	if ((debugstr = getenv("SYSLOGD_DEBUG")) != NULL)
		if ((debuglev = atoi(debugstr)) == 0)
			debuglev = 1;

#if ! defined(TEXT_DOMAIN)	/* should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) time(&start_time);
	Initialized = 0;

	while ((i = getopt(argc, argv, "df:p:m:t")) != EOF) {
		switch (i) {
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;

		case 'd':		/* debug */
			Debug++;
			break;

		case 'p':		/* path */
			LogName = optarg;
			break;

		case 'm':		/* mark interval */
			for (pstr = optarg; *pstr; pstr++) {
				if (! (isdigit(*pstr))) {
					(void) fprintf(stderr,
						"Illegal interval\n");
					usage();
				}
			}
			MarkInterval = atoi(optarg);
			if (MarkInterval < 1 || MarkInterval > INT_MAX) {
				(void) fprintf(stderr,
					"Interval must be between 1 and %d\n",
					INT_MAX);
				usage();
			}
			break;
		case 't':		/* turn off remote reception */
			turnoff++;
			break;
		default:
			usage();
		}
	}

	if (optind < argc)
		usage();

	/*
	 * ensure that file descriptor limit is "high enough"
	 */
	(void) getrlimit(RLIMIT_NOFILE, &rlim);
	if (rlim.rlim_cur < rlim.rlim_max)
		rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
		logerror("Unable to increase file descriptor limit.");

	/*
	 * close all fd's except 0-2
	 */
	for (r = 3; r <= rlim.rlim_cur; r++)
		(void) close((int)r);

	if (!Debug) {
		if (fork())
			return (0);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		untty();
	}

	/* block all signals from all threads initially */
	(void) sigfillset(&allsigs);
	(void) pthread_sigmask(SIG_BLOCK, &allsigs, NULL);

	init();			/* read configuration, start threads */

	DPRINT0(1, "off & running....\n");

	/* now set up to catch signals we care about */
	(void) sigemptyset(&sigs);
	(void) sigaddset(&sigs, SIGHUP);	/* reconfigure */
	(void) sigaddset(&sigs, SIGALRM);	/* mark & flush timer */
	(void) sigaddset(&sigs, SIGTERM);	/* exit */
	(void) sigaddset(&sigs, SIGINT);	/* exit if debugging */
	(void) sigaddset(&sigs, SIGQUIT);	/* exit if debugging */
	(void) sigaddset(&sigs, SIGPIPE);	/* catch & discard */
	(void) sigaddset(&sigs, SIGUSR1);	/* dump debug stats */

	/* we now turn into the signal handling thread */

	DPRINT0(2, "main() is now handling signals\n");
	for (;;) {
		(void) sigwait(&sigs, &sig);
		DPRINT1(2, "received signal %d\n", sig);
		switch (sig) {
		case SIGALRM:
			flushmsg(NOCOPY);
			if (Marking && (++mcount % MARKCOUNT == 0)) {
				logmymsg(LOG_INFO, "-- MARK --",
					ADDDATE|MARK|NOCOPY);
				mcount = 0;
			}
			curalarm = MarkInterval * 60 / MARKCOUNT;
			(void) alarm((unsigned)curalarm);
			DPRINT1(2, "Next alarm in %d seconds\n", curalarm);
			break;
		case SIGHUP:
			DPRINT0(1, "got SIGHUP - reconfiguring\n");

			/* If we get here then we must need to regen */
			logmymsg(LOG_SYSLOG|LOG_INFO,
			    "syslogd: configuration restart", ADDDATE);
			shutdown_msg();	 /* stop configured threads */
			close_door();

			/* give them a chance to exit */
			while (loop) {
				/* we don't need the mutex to read */
				if (conf_threads == 0)
					break;
				--loop;
				(void) sleep(1);
			}

			Initialized = 0;

			/*
			** If all threads still haven't exited
			** something is stuck or hosed. We just
			** have no option but to exit.
			*/
			if (conf_threads) {
				(void) sprintf(buf, "fatal thread problem");
				Initialized = 0;
				logerror(buf);
				return (1);
			}
			loop = LOOP_MAX; /* reset our counter */

			/* Free up some resources */
			if (Files != (struct filed *)&fallback) {
				for (f = Files; f < &Files[nlogs]; f++) {
					(void) pthread_join(f->f_thread, &p);
					filed_destroy(f);
				}
				dealloc_stacks(nlogs);
				(void) free(Files);
			}

			conf_init();	/* start reconfigure */
			Initialized = 1; /* we're reinitialized */
			/* Wake up the log thread */
			pthread_cond_signal(&hup_done);
			break;
		case SIGQUIT:
		case SIGINT:
			if (!Debug)	/* allow these signals if debugging */
				break;
			/* FALLTHROUGH */
		case SIGTERM:
			DPRINT1(1, "syslogd: going down on signal %d\n", sig);
			(void) alarm(0);
			flushmsg(0);
			(void) sprintf(buf, "going down on signal %d", sig);
			errno = 0;
			t_errno = 0;
			logerror(buf);		/* print shutdown msg */
			Initialized = 0;	/* force msg to console */
			shutdown_msg();		/* stop threads */
			close_door();
			return (0);
			break;
		case SIGUSR1:			/* secret debug dump mode */
			/* if in debug mode, use stdout */
			if (Debug) {
				dumpstats(STDOUT_FILENO);
				break;
			}
			/* otherwise dump to a debug file */
			if ((fd = open(DEBUGFILE,
				(O_WRONLY|O_CREAT|O_TRUNC|O_EXCL),
					0644)) < 0)
				break;
			dumpstats(fd);
			(void) close(fd);
			break;
		default:
			DPRINT0(2, "unexpected signal\n");
			break;
		}
	}
}

/*
 * Attempts to open the local log device
 * and return a file descriptor.
 */
static int
openklog(char *name, int mode)
{
	int fd;
	struct strioctl str;
	char line[MAXLINE + 1];

	if ((fd = open(name, mode)) < 0) {
		(void) sprintf(line, "cannot open %s", name);
		logerror(line);
		DPRINT2(1, "cannot create %s (%d)\n", name, errno);
		return (-1);
	}
	str.ic_cmd = I_CONSLOG;
	str.ic_timout = 0;
	str.ic_len = 0;
	str.ic_dp = NULL;
	if (ioctl(fd, I_STR, &str) < 0) {
		logerror("cannot register to log console messages");
		DPRINT1(1, "cannot register to log console messages (%d)\n",
			errno);
		return (-1);
	}
	return (fd);
}

/*
 * this thread listens to the local stream log driver for log messages
 * generated by this host, formats them, and queues them to the logger
 * thread.
 */
/*ARGSUSED*/
static void *
sys_poll(void *ap)
{
	int nfds, i, funix;
	static int klogerrs = 0;
	int flags = 0;
	struct strbuf ctl, dat;
	struct log_ctl hdr;
	char buf[MAXLINE+1];
	char *lastline;
	struct pollfd Pfd[1];
	int timeout, sys_init_msg_count;

	DPRINT0(5, "sys_thread started\n");

	if ((funix = openklog(LogName, O_RDONLY)) < 0)
		exit(1);

	Pfd[0].fd = funix;
	Pfd[0].events = POLLIN;

	/*
	 * Try to process as many messages as we can without blocking on poll.
	 * We count such "initial" messages with sys_init_msg_count and
	 * enqueue them without the SYNC_FILE flag.  When no more data is
	 * waiting on the local log device, we set timeout to INFTIM,
	 * clear sys_init_msg_count, and generate a flush message to sync
	 * the previously counted initial messages out to disk.
	 */

	timeout = 0;
	sys_init_msg_count = 0;

	for (;;) {
		errno = 0;
		t_errno = 0;
		nfds = poll(Pfd, 1, timeout);
		if (nfds == 0) {
			if (timeout == 0) {
				DPRINT1(1, "sys_poll blocking, init_cnt=%d\n",
				    sys_init_msg_count);

				if (sys_init_msg_count > 0) {
					flushmsg(SYNC_FILE);
					sys_init_msg_count = 0;
				}
				timeout = INFTIM;
			}
			continue;
		}

		if (nfds < 0) {
			if (errno != EINTR)
				logerror("poll");
			continue;
		}
		if (Pfd[0].revents & POLLIN) {
			dat.maxlen = MAXLINE;
			dat.buf = buf;
			ctl.maxlen = sizeof (struct log_ctl);
			ctl.buf = (caddr_t)&hdr;

			while ((i = getmsg(Pfd[0].fd, &ctl, &dat, &flags))
				== MOREDATA) {

				lastline = &dat.buf[dat.len];
				*lastline = '\0';

				while (*lastline != '\n' && lastline != buf)
					lastline--;
				if (lastline != buf)
					*lastline++ = '\0';

				/*
				 * Format sys will enqueue the log message.
				 * Set the sync flag if timeout != 0, which
				 * means that we're done handling all the
				 * initial messages ready during startup.
				 */
				if (timeout == 0) {
					formatsys(&hdr, buf, 0);
					sys_init_msg_count++;
				} else {
					formatsys(&hdr, buf, 1);
				}
				sys_msg_count++;

				if (lastline != buf) {
					(void) strncpy(buf, lastline, MAXLINE);
					dat.maxlen = MAXLINE - strlen(buf);
					dat.buf = &buf[strlen(buf)];
				} else {
					dat.maxlen = MAXLINE;
					dat.buf = buf;
				}
			}

			if (i == 0 && dat.len > 0) {
				dat.buf[dat.len] = '\0';
				/*
				 * Format sys will enqueue the log message.
				 * Set the sync flag if timeout != 0, which
				 * means that we're done handling all the
				 * initial messages ready during startup.
				 */
				if (timeout == 0) {
					formatsys(&hdr, buf, 0);
					sys_init_msg_count++;
				} else {
					formatsys(&hdr, buf, 1);
				}
				sys_msg_count++;
				nfds--;
			} else if (i < 0 && errno != EINTR) {
				logerror("kernel log driver read error");
				(void) close(Pfd[0].fd);
				Pfd[0].fd = -1;
				nfds--;
			}
		} else if (Pfd[0].revents & (POLLNVAL|POLLHUP|POLLERR)) {
			logerror("kernel log driver poll error");
			(void) close(Pfd[0].fd);
			Pfd[0].fd = -1;
		}

		while (Pfd[0].fd == -1 && klogerrs++ < 10) {
			Pfd[0].fd = openklog(LogName, O_RDONLY);
		}
		if (klogerrs >= 10) {
			logerror("can't reopen kernel log device");
			exit(1);
		}
	}
	/*NOTREACHED*/
}

/*
 * this thread polls all the network interfaces for syslog messages
 * forwarded to us, tags them with the hostname they are received
 * from, and queues them to the logger thread.
 */
/*ARGSUSED*/
static void *
net_poll(void *ap)
{
	int nfds, i;
	int flags = 0;
	struct t_unitdata *udp;
	struct t_uderr *errp;
	char buf[MAXLINE+1];
	struct netconfig *ncp;
	char *uap;
	log_message_t *mp;

	DPRINT0(5, "net_thread started\n");

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp))

	for (;;) {
		errno = 0;
		t_errno = 0;
		nfds = poll(Nfd, Ninputs, -1);
		if (nfds == 0)
			continue;

		if (nfds < 0) {
			if (errno != EINTR)
				logerror("poll");
			continue;
		}
		i = 0;
		while (nfds > 0 && i < Ninputs) {
			if (Nfd[i].revents & POLLIN) {
				udp = Udp[i];
				udp->udata.buf = buf;
				udp->udata.maxlen = MAXLINE;
				udp->udata.len = 0;
				flags = 0;
				if (t_rcvudata(Nfd[i].fd, udp, &flags) < 0) {
					errp = Errp[i];
					if (t_errno == TLOOK) {
						if (t_rcvuderr(Nfd[i].fd,
							errp) < 0) {
							logerror("t_rcvuderr");
							t_close(Nfd[i].fd);
							Nfd[i].fd = -1;
						}
					} else {
						logerror("t_rcvudata");
						t_close(Nfd[i].fd);
						Nfd[i].fd = -1;
					}
					nfds--;
					continue;
				}
				nfds--;
				if (udp->udata.len > 0 && turnoff == 0) {
					/* Force EOL in buffer */
					mp = new_msg();
					if (mp == NULL) {
						logerror("malloc failed");
						continue;
					}
					mp->source = i;
					buf[udp->udata.len] = '\0';
					ncp = (struct netconfig *)&Ncf[i];
					if ((uap = taddr2uaddr(ncp,
						&udp->addr)) != (char *)NULL) {
						DPRINT1(1, "received message"
							" from %s\n", uap);
						(void) strncpy(mp->curaddr,
							uap, SYS_NMLN);
					} else
						(void) strncpy(mp->curaddr,
							"<unknown>", SYS_NMLN);
					mp->hlp = cvthname
					    (&udp->addr, ncp, mp->curaddr);
					if (mp->hlp == NULL)
						mp->hlp = &NullHostName;
					formatnet(&udp->udata, mp);
					net_msg_count++;
					(void) dataq_enqueue(&inputq,
					    (void *)mp);
					DPRINT1(5, "net_thread queued %p\n",
						mp);
					free(uap);
				}
			} else if (Nfd[i].revents &
				(POLLNVAL|POLLHUP|POLLERR)) {
				logerror("POLLNVAL|POLLHUP|POLLERR");
				(void) t_close(Nfd[i].fd);
				Nfd[i].fd = -1;
				nfds--;
			}
			i++;
		}
	}
	/*NOTREACHED*/
}

static void
usage(void)
{
	(void) fprintf(stderr,
		"usage: syslogd [-d] [-mmarkinterval] [-ppath] [-fconffile]\n");
	exit(1);
}

static void
untty(void)
{
	if (!Debug)
		(void) setsid();
}

/*
 * generate a log message internally. The original version of syslogd
 * simply called logmsg directly, but because everything is now based
 * on message passing, we need an internal way to generate and queue
 * log messages from within syslogd itself.
 */
static void
logmymsg(int pri, char *msg, int flags)
{
	log_message_t *mp = new_msg();

	if (mp == NULL) {
		logerror("malloc failed");
		return;
	}
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp))
	mp->pri = pri;
	mp->hlp = &LocalHostName;
	(void) strncpy(mp->msg, msg, MAXLINE);
	(void) strncpy(mp->curaddr, mynumericaddr, SYS_NMLN);
	mp->flags = flags;
	(void) time(&mp->ts);
	DPRINT1(5, "logmymsg queued %p\n", mp);
	(void) dataq_enqueue(&inputq, (void *)mp);
}

/*
 * Generate an internal shutdown message
 */
static void
shutdown_msg(void)
{
	log_message_t *mp = new_msg();

	if (mp == NULL) {
		logerror("malloc failed");
		return;
	}
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp));
	mp->flags = SHUTDOWN;
	mp->hlp = &LocalHostName;
	DPRINT1(5, "shutdown_msg queued %p\n", mp);
	(void) dataq_enqueue(&inputq, (void *)mp);
}

/*
 * Generate an internal flush message
 */
static void
flushmsg(int flags)
{
	log_message_t *mp = new_msg();

	if (mp == NULL) {
		logerror("malloc failed");
		return;
	}
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp));
	mp->flags = FLUSHMSG | flags;
	mp->hlp = &LocalHostName;
	DPRINT2(5, "flush_msg queued %p, flags 0x%x\n", mp, flags);
	(void) dataq_enqueue(&inputq, (void *)mp);
}

/*
 * Do some processing on messages received from the net
 */
static void
formatnet(struct netbuf *nbp, log_message_t *mp)
{
	char *p, *q;
	int i, c;
	int pri;
	char line[MAXLINE + 1];

	DPRINT1(5, "formatnet called %p\n", mp);

	mp->flags = NETWORK;
	(void) time(&mp->ts);

	/* test for special codes */
	pri = DEFUPRI;
	p = nbp->buf;
	DPRINT1(9, "Message content:\n>%s<\n", p);
	if (*p == '<' && isdigit(*(p+1))) {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
		if (pri <= 0 || pri >= (LOG_NFACILITIES << 3))
			pri = DEFUPRI;
	}

	/* don't allow users to log kernel messages */
	if ((pri & LOG_PRIMASK) == LOG_KERN)
		pri |= LOG_USER;

	q = line;
	i = 0;
	while ((c = *p++ & 0177) != '\0' && i < MAXLINE) {
		if (iscntrl(c)) {
			if (c == '\n') {
				*q++ = '\\';
				*q++ = 'n';
			} else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
			i += 2;
		} else {
			*q++ = (char)c;
			i++;
		}
	}
	*q = '\0';

	/* Remove the trailing NL, if it's there */

	if (q - 2 >= line && q[-2] == '\\' && q[-1] == 'n') {
		q[-2] = '\0';
	}
	mp->pri = pri;
	(void) strncpy(mp->msg, line, MAXLINE);
}

/*
 * Do some processing on messages generated by this host
 * and then enqueue the log message.
 */
static void
formatsys(struct log_ctl *lp, char *msg, int sync)
{
	char *p, *q;
	int c, i;
	char line[MAXLINE + 1];
	log_message_t	*mp;
	char cbuf[30];

	DPRINT2(3, "log_ctl.mid = %d, log_ctl.sid = %d\n", lp->mid, lp->sid);
	DPRINT1(9, "Message Content:\n>%s<\n", msg);

	for (p = msg; *p != '\0'; ) {
		/*
		 * Allocate a log_message_t structure.
		 * We should do it here since a single message (msg)
		 * could be composed of many lines.
		 */
		_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp));
		mp = new_msg();
		if (mp == NULL) {
			logerror("malloc failed");
			continue;
		}


		mp->flags &= ~NETWORK;
		mp->hlp = &LocalHostName;
		mp->ts = lp->ttime;
		(void) strncpy(mp->curaddr, mynumericaddr, SYS_NMLN);
		if (lp->flags & SL_LOGONLY)
			mp->flags |= IGN_CONS;
		if (lp->flags & SL_CONSONLY)
			mp->flags |= IGN_FILE;


		/* extract facility */
		if ((lp->pri & LOG_FACMASK) == LOG_KERN)
			(void) sprintf(line, "%.15s ",
				ctime_r(&mp->ts, cbuf) + 4);
		else
			(void) sprintf(line, "");
		q = line + strlen(line);
		i = strlen(line);
		while (*p != '\0' && (c = *p++) != '\n' && i < MAXLINE) {
			*q++ = c;
			i++;
		}
		*q = '\0';

		if (sync && ((lp->pri & LOG_FACMASK) == LOG_KERN))
			mp->flags |= SYNC_FILE;	/* fsync file after write */

		if (i != 0) {
			(void) strncpy(mp->msg, line, MAXLINE);
			mp->pri = lp->pri;
			mp->source = 0;
			(void) dataq_enqueue(&inputq, (void *)mp);
			DPRINT1(5, "sys_thread queued(1) %p\n", mp);
		} else
			free_msg(mp);
	}
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
/*ARGSUSED*/
static void *
logmsg(void *ap)
{
	struct filed *f;
	int fac, prilev;
	log_message_t *mp;

	DPRINT0(5, "file_selector_thread started\n");

	for (;;) {
		(void) dataq_dequeue(&inputq, (void **)&mp);
		_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*mp))
		DPRINT1(5, "file_selector dequeued %p\n", mp);
		/*
		 * is it a shutdown or flush message ?
		 */
		if ((mp->flags & SHUTDOWN) || (mp->flags & FLUSHMSG)) {
			pthread_mutex_lock(&mp->msg_mutex);
			for (f = Files; f < &Files[nlogs]; f++) {
				pthread_mutex_lock(&f->filed_mutex);
				f->f_queue_count++;
				mp->refcnt++;
				(void) dataq_enqueue(&f->f_queue, (void *)mp);
				pthread_mutex_unlock(&f->filed_mutex);
			}
			pthread_mutex_unlock(&mp->msg_mutex);

			if (mp->flags & SHUTDOWN) {
				pthread_mutex_lock(&hup_lock);
				pthread_cond_wait(&hup_done, &hup_lock);
				pthread_mutex_unlock(&hup_lock);
			}

			continue;
		}
		/*
		 * Check to see if msg looks non-standard.
		 */
		if ((int)strlen(mp->msg) < 16 || mp->msg[3] != ' ' ||
			mp->msg[6] != ' ' || mp->msg[9] != ':' ||
			mp->msg[12] != ':' || mp->msg[15] != ' ')
			mp->flags |= ADDDATE;

		/* extract facility and priority level */
		fac = (mp->pri & LOG_FACMASK) >> 3;
		if (mp->flags & MARK)
			fac = LOG_NFACILITIES;
		prilev = mp->pri & LOG_PRIMASK;

		DPRINT2(3, "logmsg: fac = %d, pri = %d\n", fac, prilev);

		/*
		 * Ensure that there are threads around to receive the
		 * incoming messages
		 */
		pthread_mutex_lock(&cft);

		while (conf_threads == 0) {
			pthread_cond_wait(&log_threads_configured, &cft);
		}
		pthread_mutex_unlock(&cft);

		/*
		 * Because different devices log at different speeds,
		 * it's important to hold the mutex for the current
		 * message until it's been queued to all log files,
		 * so the reference count is accurate before any
		 * of the log threads can decrement it.
		 */
		_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*mp))
		_NOTE(COMPETING_THREADS_NOW)
		pthread_mutex_lock(&mp->msg_mutex);
		for (f = Files; f < &Files[nlogs]; f++) {
			/* skip messages that are incorrect priority */
			if (f->f_pmask[fac] < (unsigned)prilev ||
				f->f_pmask[fac] == NOPRI)
				continue;
			if (f->f_queue_count > Q_HIGHWATER_MARK) {
				DPRINT3(5, "Dropping message %p on file "
					"%p, count = %d\n",
					mp, f, f->f_queue_count);
				continue;
			}
			pthread_mutex_lock(&f->filed_mutex);
			f->f_queue_count++;
			mp->refcnt++;
			(void) dataq_enqueue(&f->f_queue, (void *)mp);
			pthread_mutex_unlock(&f->filed_mutex);
		}
		/*
		 * if refcnt == 0, no other thread should have a
		 * pointer to this message, but as a precation we'll
		 * hold the lock until after testing the refcnt.
		 */
		if (mp->refcnt == 0) {
			pthread_mutex_unlock(&mp->msg_mutex);
			free_msg(mp);
		} else
			pthread_mutex_unlock(&mp->msg_mutex);
	}
}

/*
 * function to actually write the log message to the selected file.
 * each file has a logger thread that runs this routine. The function
 * is called with a pointer to its file structure.
 */
static void *
logit(void *ap)
{
	struct filed *f = ap;
	log_message_t *mp;
	int forwardingloop = 0;
	char *errmsg = "%s to %s forwarding loop detected\n";
	int i, currofst, prevofst;
	host_list_t *hlp;

	assert(f != NULL);

	DPRINT1(5, "filed thread started for filed %p\n", f);
	_NOTE(COMPETING_THREADS_NOW);
	for (;;) {
		(void) dataq_dequeue(&f->f_queue, (void **)&mp);
		pthread_mutex_lock(&f->filed_mutex);
		assert(f->f_queue_count > 0);
		f->f_queue_count--;
		pthread_mutex_unlock(&f->filed_mutex);
		DPRINT3(5, "filed thread %p (queue %p) dequeued %p\n",
			f, &f->f_queue, mp);
		assert(mp->refcnt > 0);

		/*
		 * is it a shutdown message ?
		 */
		if (mp->flags & SHUTDOWN) {
			pthread_mutex_lock(&mp->msg_mutex);
			mp->refcnt--;
			if (mp->refcnt == 0) {
				pthread_mutex_unlock(&mp->msg_mutex);
				free_msg(mp);
			} else {
				pthread_mutex_unlock(&mp->msg_mutex);
			}
			break;
		}

		/*
		 * Is it a logsync message?
		 */
		if ((mp->flags & (FLUSHMSG | LOGSYNC)) ==
		    (FLUSHMSG | LOGSYNC)) {
			if (f->f_type != F_FILE)
				goto out;	/* nothing to do */
			(void) close(f->f_file);
			f->f_file = open(f->f_un.f_fname,
					O_WRONLY|O_APPEND|O_NOCTTY);
			if (f->f_file < 0) {
				f->f_type = F_UNUSED;
				logerror(f->f_un.f_fname);
				f->f_stat.errs++;
				break;
			}
			goto out;
		}

		/*
		 * If the message flags include both flush and sync,
		 * then just sync the file out to disk if appropriate.
		 */
		if ((mp->flags & (FLUSHMSG | SYNC_FILE)) ==
		    (FLUSHMSG | SYNC_FILE)) {
			if (f->f_type == F_FILE) {
				DPRINT1(5, "got FLUSH|SYNC for filed %p\n", f);
				(void) fsync(f->f_file);
			}
			goto out;
		}

		/*
		 * Otherwise if it's a standard flush message, write
		 * out any saved messages to the file.
		 */
		if ((mp->flags & FLUSHMSG) && (f->f_prevcount > 0)) {
			set_flush_msg(f);
			writemsg(SAVED, f);
			goto out;
		}

		(void) strncpy(f->f_current.msg, mp->msg, MAXLINE);
		(void) strncpy(f->f_current.host, mp->hlp->hl_hosts[0],
				SYS_NMLN);
		f->f_current.pri = mp->pri;
		f->f_current.flags = mp->flags;
		f->f_current.time = mp->ts;
		f->f_msgflag &= ~CURRENT_VALID;
		hlp = mp->hlp;

		prevofst = (f->f_prevmsg.flags & ADDDATE) ? 0 : 16;
		currofst = (f->f_current.flags & ADDDATE) ? 0 : 16;

		if (f->f_type == F_FORW) {
			/*
			 * Should not forward MARK messages, as they are
			 * not defined outside of the current system.
			 */

			if (mp->flags & MARK) {
				DPRINT0(1, "logit: cannot forward Mark\n");
				goto out;
			}

			/*
			 * can not forward message if we do
			 * not have a host to forward to
			 */
			if (hlp == (host_list_t *)NULL)
				goto out;
			/*
			 * a forwarding loop is created on machines
			 * with multiple interfaces because the
			 * network address of the sender is different
			 * to the receiver even though it is the
			 * same machine. Instead, if the
			 * hostname the source and target are
			 * the same the message if thrown away
			 */
			forwardingloop = 0;
			for (i = 0; i < hlp->hl_cnt; i++) {
				if (strcmp(hlp->hl_hosts[i],
					f->f_un.f_forw.f_hname) == 0) {
					DPRINT2(1, errmsg,
						f->f_un.f_forw.f_hname,
						hlp->hl_hosts[i]);
					forwardingloop = 1;
					break;
				}
			}

			if (forwardingloop == 1) {
				f->f_stat.cantfwd++;
				goto out;
			}
		}

		f->f_msgflag |= CURRENT_VALID;

		/* check for dup message */
		if ((f->f_msgflag & CURRENT_VALID) &&
			(f->f_msgflag & OLD_VALID) &&
			prevofst == currofst &&
			(strcmp(f->f_prevmsg.msg + prevofst,
				f->f_current.msg + currofst) == 0) &&
			(strcmp(f->f_prevmsg.host,
				f->f_current.host) == 0)) { /* a dup */
			DPRINT0(2, "logit: dup message\n");
			if (currofst == 16) {
				(void) strncpy(f->f_prevmsg.msg,
				f->f_current.msg, 15); /* update time */
			}
			f->f_prevcount++;
			f->f_stat.dups++;
			f->f_stat.total++;
			f->f_msgflag &= ~CURRENT_VALID;
		} else { /* new: mark or prior dups exist */
			if (f->f_current.flags & MARK || f->f_prevcount > 0) {
				if (f->f_prevcount > 0) {
					set_flush_msg(f);
					if (f->f_msgflag & OLD_VALID)
						writemsg(SAVED, f);
				}
				if (f->f_msgflag & CURRENT_VALID)
					writemsg(CURRENT, f);
				if (!(mp->flags & NOCOPY))
					copy_msg(f);
				if (f->f_current.flags & MARK) {
					DPRINT0(2, "mark\n");
					f->f_msgflag &= ~OLD_VALID;
				} else {
					DPRINT0(2, "saved\n");
				}
				f->f_stat.total++;
			} else { /* new message */
				DPRINT0(2, "logit: new\n");
				writemsg(CURRENT, f);
				if (!(mp->flags & NOCOPY))
					copy_msg(f);
				f->f_stat.total++;
			}
		}
		/*
		 * if message refcnt goes to zero after we decrement
		 * it here, we are the last consumer of the message,
		 * and we should free it.  We need to hold the lock
		 * between decrementing the count and checking for
		 * zero so another thread doesn't beat us to it.
		 */
out:
		pthread_mutex_lock(&mp->msg_mutex);
		mp->refcnt--;
		if (mp->refcnt == 0) {
			pthread_mutex_unlock(&mp->msg_mutex);
			free_msg(mp);
		} else
			pthread_mutex_unlock(&mp->msg_mutex);
	}
	/* register our exit */
	(void) close(f->f_file);
	pthread_mutex_lock(&cft);
	--conf_threads;
	pthread_mutex_unlock(&cft);
	DPRINT0(5, "logging thread exited\n");
	return (NULL);
}

/*
 * change the previous message to a flush message, stating how
 * many repeats occurred since the last flush
 */
static void
set_flush_msg(struct filed *f)
{
	char tbuf[10];
	int prevofst = (f->f_prevmsg.flags & ADDDATE) ? 0 : 16;

	if (f->f_prevcount == 1)
		(void) strncpy(tbuf, "time", sizeof (tbuf));
	else
		(void) strncpy(tbuf, "times", sizeof (tbuf));

	(void) sprintf(f->f_prevmsg.msg+prevofst,
		"last message repeated %d %s", f->f_prevcount, tbuf);
	f->f_prevcount = 0;
	f->f_msgflag |= OLD_VALID;
}


/*
 * the actual writing of the message is broken into a separate function
 * because each file has a current and saved message associated with
 * it (for duplicate message detection). It is necessary to be able
 * to write either the saved message or the current message.
 */
static void
writemsg(int selection, struct filed *f)
{
	char *cp;
	int pri;
	int flags;
	int l;
	time_t ts;
	struct t_unitdata ud;
	char *eomp, *eomp2, *from, *text, *msg;
	char line[MAXLINE*2], line2[MAXLINE*2], filtered[MAXLINE*2];
	char cbuf[30];
	char *msgid_start, *msgid_end;

	switch (selection) {
	default:
	case CURRENT:		/* print current message */
		msg = f->f_current.msg;
		from = f->f_current.host;
		pri = f->f_current.pri;
		flags = f->f_current.flags;
		ts = f->f_current.time;
		f->f_msgflag &= ~CURRENT_VALID;
		break;
	case SAVED:		/* print saved message */
		msg = f->f_prevmsg.msg;
		from = f->f_prevmsg.host;
		pri = f->f_prevmsg.pri;
		flags = f->f_prevmsg.flags;
		ts = f->f_prevmsg.time;
		f->f_msgflag &= ~OLD_VALID;
		break;
	}

	if (msg[0] == '\0')
		return;

	cp = line;

	if (flags & ADDDATE)
		(void) strncpy(cp, ctime_r(&ts, cbuf) + 4, 15);
	else
		(void) strncpy(cp, msg, 15);

	line[15] = '\0';
	(void) strcat(cp, " ");
	(void) strcat(cp, from);
	(void) strcat(cp, " ");
	text = cp + strlen(cp);

	if (flags & ADDDATE)
		(void) strcat(cp, msg);
	else
		(void) strcat(cp, msg+16);

	/*
	 * filter the string in preparation for writing it
	 * save the original for possible forwarding
	 */

	filter_string(cp, filtered);

	/*
	 * If we're writing to the console, strip out the message ID
	 * to reduce visual clutter.
	 */
	if ((msgid_start = strstr(filtered, "[ID ")) != NULL &&
	    (msgid_end = strstr(msgid_start, "] ")) != NULL &&
	    f->f_type == F_CONSOLE)
		(void) strcpy(msgid_start, msgid_end + 2);

	eomp = filtered + strlen(filtered);

	errno = 0;
	t_errno = 0;
	switch (f->f_type) {
	case F_UNUSED:
		DPRINT0(1, "\n");
		break;
	case F_FORW:
		DPRINT2(1, "Logging to %s %s\n", TypeNames[f->f_type],
			f->f_un.f_forw.f_hname);
		(void) sprintf(line2, "<%d>%.15s %s",
			pri, cp, text);
		l = strlen(line2);
		if (l > MAXLINE)
			l = MAXLINE;
		ud.opt.buf = NULL;
		ud.opt.len = 0;
		ud.udata.buf = line2;
		ud.udata.len = l;
		ud.addr.maxlen = f->f_un.f_forw.f_addr.maxlen;
		ud.addr.buf = f->f_un.f_forw.f_addr.buf;
		ud.addr.len = f->f_un.f_forw.f_addr.len;
		if (t_sndudata(f->f_file, &ud) < 0) {
			logerror("t_sndudata");
			(void) t_close(f->f_file);
			f->f_type = F_UNUSED;
		}
		break;
	case F_CONSOLE:
	case F_TTY:
	case F_FILE:
		if ((f->f_type == F_FILE && (flags & IGN_FILE)) ||
		    (f->f_type == F_CONSOLE && (flags & IGN_CONS)))
			break;
		DPRINT2(1, "Logging to %s %s\n",
			TypeNames[f->f_type], f->f_un.f_fname);
		if (f->f_type != F_FILE) {
			/* CSTYLED */
			(void) strncpy(eomp, "\r\n", 3); /*lint !e669*/
		} else {
			if ((eomp2 = strchr(filtered, '\r')) != NULL) {
				(void) strncpy(eomp2, "\n", 2);
			} else {
				/* CSTYLED */
				(void) strncpy(eomp, "\n", 2); /*lint !e669*/
			}
		}
		if (write(f->f_file, filtered, strlen(filtered)) < 0) {
			int e = errno;
			(void) close(f->f_file);
			/*
			 * Check for EBADF on TTY's due
			 * to vhangup() XXX
			 */
			if (e == EBADF && f->f_type != F_FILE) {
				f->f_file = open(f->f_un.f_fname,
					O_WRONLY|O_APPEND|O_NOCTTY);
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror(f->f_un.f_fname);
					f->f_stat.errs++;
				}
				untty();
			} else {
				f->f_type = F_UNUSED;
				f->f_stat.errs++;
				errno = e;
				logerror(f->f_un.f_fname);
			}
		} else if (flags & SYNC_FILE)
			if (((pri & LOG_FACMASK) >> 3) == LOG_KERN)
				(void) fsync(f->f_file);
		break;
	case F_USERS:
	case F_WALL:
		DPRINT1(1, "Logging to %s\n", TypeNames[f->f_type]);
		/* CSTYLED */
		(void) strcat(eomp, "\r\n"); /*lint !e669*/
		/*
		** Since wallmsg messes with utmpx we need
		** to guarantee single threadedness...
		*/
		pthread_mutex_lock(&wmp);
		wallmsg(f, from, filtered);
		pthread_mutex_unlock(&wmp);
		break;
	}
}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
static void
wallmsg(struct filed *f, char *from, char *msg)
{
	int i;
	char *cp;
	struct utmpx *utxp;
	time_t now;
	char line[MAXLINE*2], dev[100];
	struct stat statbuf;
	walldev_t *w;
	char cbuf[30];

	if (access(UTMPX_FILE, R_OK) != 0 || stat(UTMPX_FILE, &statbuf) != 0) {
		logerror(UTMPX_FILE);
		return;
	} else if (statbuf.st_uid != 0 || (statbuf.st_mode & 07777) != 0644) {
		(void) sprintf(line, "%s %s", UTMPX_FILE,
			"not owned by root or not mode 644.\n"
			"This file must be owned by root "
			"and not writable by\n"
			"anyone other than root.  This alert is being "
			"dropped because of\n"
			"this problem.");
		logerror(line);
		return;
	}

	if (f->f_type == F_WALL) {
		(void) time(&now);
		(void) sprintf(line,
			"\r\n\7Message from syslogd@%s "
			"at %.24s ...\r\n", from, ctime_r(&now, cbuf));
		(void) strcat(line, msg+16);
		cp = line;
	} else {
		cp = msg;
	}
	/* scan the user login file */
	setutxent();
	while ((utxp = getutxent()) != NULL) {
		/* is this slot used? */
		if (utxp->ut_name[0] == '\0' ||
			utxp->ut_line[0] == '\0' ||
			utxp->ut_type == DEAD_PROCESS)
			continue;
		/* should we send the message to this user? */
		if (f->f_type == F_USERS) {
			for (i = 0; i < MAXUNAMES; i++) {
				if (!f->f_un.f_uname[i][0]) {
					i = MAXUNAMES;
					break;
				}
				if (strncmp(f->f_un.f_uname[i],
					utxp->ut_name, UNAMESZ) == 0)
					break;
			}
			if (i >= MAXUNAMES)
				continue;
		}
		/* compute the device name */
		(void) strncpy(dev, "/dev/", sizeof (dev) - 1);
		(void) strncat(dev, utxp->ut_line, UDEVSZ);
		DPRINT1(1, "write to '%s'\n", dev);
		if ((w = malloc(sizeof (walldev_t))) != NULL) {
			int rc;
			(void) pthread_attr_init(&w->thread_attr);
			(void) pthread_attr_setdetachstate(&w->thread_attr,
				PTHREAD_CREATE_DETACHED);
			(void) strncpy(w->dev, dev, PATH_MAX);
			(void) strncpy(w->msg, cp, MAXLINE);
			(void) strncpy(w->ut_name, utxp->ut_name,
			    sizeof (w->ut_name));

			if ((rc = pthread_create(&w->thread, &w->thread_attr,
				writetodev, (void *) w)) != 0) {
				DPRINT1(5, "wallmsg thread create failed "
					"rc = %d\n", rc);
				free(w);
				break;
			}
		}
	}
	/* close the user login file */
	endutxent();
}

/*
 * Each time we need to write to a tty device (a potentially expensive
 * or long-running operation) this routine gets called as a new
 * detached, unbound thread. This allows writes to many devices
 * to proceed nearly in parallel, without having to resort to
 * asynchronous I/O or forking.
 */
static void *
writetodev(void *ap)
{
	walldev_t *w = ap;
	int ttyf;
	int len;
	struct stat statb;
	struct passwd *pw;
	char errorbuf[MAXLINE];

	len = strlen(w->msg);

	ttyf = open(w->dev, O_WRONLY|O_NOCTTY|O_NDELAY);
	if (ttyf >= 0) {
		if (fstat(ttyf, &statb) != 0) {
			DPRINT1(1, "Can't stat '%s'\n", w->dev);
			(void) sprintf(errorbuf, "Can't stat '%s'", w->dev);
			errno = 0;
			logerror(errorbuf);
		} else if (!(statb.st_mode & S_IWRITE)) {
			DPRINT1(1, "Can't write to '%s'\n", w->dev);
		} else if (!isatty(ttyf)) {
			DPRINT1(1, "'%s' not a tty\n", w->dev);
			(void) sprintf(errorbuf, "'%s' not a tty", w->dev);
			errno = 0;
			logerror(errorbuf);
		} else if ((pw = getpwuid(statb.st_uid)) == NULL) {
			DPRINT1(1, "Can't determine owner of '%s'\n", w->dev);
			(void) sprintf(errorbuf, "Can't determine owner "
				"of '%s'", w->dev);
			errno = 0;
			logerror(errorbuf);
		} else if (strncmp(pw->pw_name, w->ut_name, UNAMESZ) != 0) {
			DPRINT1(1, "Bad terminal owner '%s'\n", w->dev);
			(void) sprintf(errorbuf, "%s %s owns '%s' %s %.*s",
				"Bad terminal owner;", pw->pw_name, w->dev,
				"but utmpx says", UNAMESZ, w->ut_name);
			errno = 0;
			logerror(errorbuf);
		} else if (write(ttyf, w->msg, len) != len) {
			DPRINT1(1, "Write failed to '%s'\n", w->dev);
			(void) sprintf(errorbuf, "Write failed to '%s'",
				w->dev);
			logerror(errorbuf);
		}
		(void) close(ttyf);
	} else
		DPRINT1(1, "Can't open '%s'\n", w->dev);

	pthread_attr_destroy(&w->thread_attr);
	(void) free(w);
	pthread_exit(0);
	return (NULL);
	/*NOTREACHED*/
}
/*
 * Return a printable representation of a host address. If unable to
 * look up hostname, format the numeric address for display instead.
 */
static host_list_t *
cvthname(struct netbuf *nbp, struct netconfig *ncp, char *failsafe_addr)
{
	int i;
	host_list_t *h;
	struct nd_hostservlist *hsp;
	struct nd_hostserv *hspp;

	/* memory allocation failure here is fatal */
	if ((h = malloc(sizeof (host_list_t))) == NULL) {
		logerror("malloc failed converting host name");
		return ((host_list_t *)NULL);
	}

	if (ncp->nc_semantics == NC_TPI_CLTS) {
		if (netdir_getbyaddr(ncp, &hsp, nbp) == 0) {
			if (hsp->h_cnt > 0) {
				hspp = hsp->h_hostservs;
				h->hl_cnt = hsp->h_cnt;
				h->hl_hosts = (char **)malloc(sizeof (char *) *
					(h->hl_cnt));
				if (h->hl_hosts == NULL)
					goto out;
				DPRINT1(2, "Found %d hostnames\n", h->hl_cnt);
				for (i = 0; i < h->hl_cnt; i++) {
					h->hl_hosts[i] = (char *)
					    malloc(sizeof (char) *
						    (strlen(hspp->h_host)
						    + 1));
					if (h->hl_hosts[i] == NULL) {
						int j;
						for (j = 0; j < i; j++) {
							free(h->hl_hosts[j]);
						}
						(void) free(h->hl_hosts);
						goto out;
					}
					(void) strcpy(h->hl_hosts[i],
						hspp->h_host);
					hspp++;
				}
			} else {
out:
				netdir_free((void *)hsp, ND_HOSTSERVLIST);
				free(h);
				return ((host_list_t *)NULL);
			}
			netdir_free((void *)hsp, ND_HOSTSERVLIST);
		} else { /* unknown address */
			h->hl_cnt = 1;
			h->hl_hosts = (char **)malloc(sizeof (char *));
			if (h->hl_hosts == NULL) {
				(void) free(h);
				return ((host_list_t *)NULL);
			}
			h->hl_hosts[0] = (char *)malloc(sizeof (char) *
				(strlen(failsafe_addr) + 3));
			if (h->hl_hosts[0] == NULL) {
				(void) free(h->hl_hosts);
				(void) free(h);
				return ((host_list_t *)NULL);
			}
			(void) sprintf(h->hl_hosts[0], "[%s]", failsafe_addr);
			DPRINT1(1, "Hostname lookup failed - using address"
				" %s instead\n", h->hl_hosts[0]);
		}
	}
	return (h);
}

/*
 * Print syslogd errors some place. Need to be careful here, because
 * this routine is called at times when we're not initialized and
 * ready to log messages...in this case, fall back to using the console.
 */
void
logerror(char *type)
{
	char buf[MAXLINE+1];
	FILE *console;
	int cfd;

	if (t_errno == 0 || t_errno == TSYSERR) {
		char *errstr;

		if (errno == 0)
			(void) sprintf(buf, "syslogd: %.*s", MAXLINE, type);
		else if ((errstr = strerror(errno)) == (char *)NULL)
			(void) sprintf(buf, "syslogd: %s: error %d",
				type, errno);
		else
			(void) sprintf(buf, "syslogd: %s: %s", type, errstr);
	} else {
		if (t_errno > t_nerr)
			(void) sprintf(buf, "syslogd: %s: t_error %d",
				type, t_errno);
		else
			(void) sprintf(buf, "syslogd: %s: %s",
				type, t_errlist[t_errno]);
	}
	errno = 0;
	t_errno = 0;
	DPRINT1(1, "%s\n", buf);
	if (Initialized)
		logmymsg(LOG_SYSLOG|LOG_ERR, buf, ADDDATE);
	else {
		/*
		 * must use open here instead of fopen, because
		 * we need the O_NOCTTY behavior - otherwise we
		 * could hang the console at boot time
		 */
		if (((cfd = open(sysmsg, O_WRONLY|O_APPEND|O_NOCTTY)) >= 0) ||
		    ((cfd = open(ctty, O_WRONLY|O_APPEND|O_NOCTTY)) >= 0)) {
			console = fdopen(cfd, "a+");
			(void) fprintf(console, "%s\n", buf);
			(void) fclose(console);
		} else {
			/* punt */
			DPRINT0(1, "Not initialized & can't open console\n");
		}
	}
}

/*
 * copy current message to saved message in filed structure.
 */
static void
copy_msg(struct filed *f)
{
	(void) strncpy(f->f_prevmsg.msg, f->f_current.msg, MAXLINE);
	(void) strncpy(f->f_prevmsg.host, f->f_current.host, SYS_NMLN);
	f->f_prevmsg.pri = f->f_current.pri;
	f->f_prevmsg.flags = f->f_current.flags;
	f->f_prevmsg.time = f->f_current.time;
	f->f_msgflag |= OLD_VALID;
}


/*
 * function to free a host_list_t struct that was allocated
 * out of cvthname(). There is a special case where we don't
 * free the hostname list in LocalHostName, because that's
 * our own addresses, and we just want to have to look it
 * up once and save it.  Also don't free it if it's
 * NullHostName, because that's a special one we use if
 * name service lookup fails.
 */
static void
freehl(host_list_t *h)
{
	int i;

	if (h == NULL || h == &LocalHostName || h == &NullHostName)
		return;
	for (i = 0; i < h->hl_cnt; i++)
		free(h->hl_hosts[i]);
	free(h->hl_hosts);
	free(h);
}


/*
 * create the /etc/syslog.pid file. This is kept for backwards
 * compatibility, but we no longer use /etc/syslog.pid for
 * synchronization. Instead, we create a new file named by
 * DOORFILE and create a door for the library function to use for
 * synchronization.
 */
static void
open_door(void)
{
	int fd;
	int door;
	struct stat buf;
	door_info_t info;
	char line[MAXLINE+1];

	/*
	 * first see if another syslogd is running by trying
	 * a door call - if it succeeds, there is already
	 * a syslogd process active
	 */

	if ((door = open(DOORFILE, O_RDONLY)) >= 0) {
		DPRINT0(5, "doorfile opened successfully\n");
		if (door_info(door, &info) >= 0) {
			DPRINT1(5, "door_info:info.di_target = %ld\n",
				info.di_target);
			if (info.di_target > 0) {
				(void) sprintf(line, "syslogd pid %ld already "
					"running. Cannot start another "
					"syslogd pid %ld",
					info.di_target, getpid());
				errno = 0;
				logerror(line);
				exit(1);
			}
		}
	} else {
		if (stat(DOORFILE, &buf) < 0) {
			DPRINT1(5, "stat of doorfile failed - %d\n",
				errno);
			if ((fd = creat(DOORFILE, 0644)) < 0) {
				logerror("doorfile creat failed\n");
				exit(1);
			}
			(void) close(fd);
		}
	}
	if ((door = door_create(server, 0, 0)) < 0) {
		logerror("door_create failed\n");
		exit(1);
	}
	if (symlink(DOORFILE, OLDDOORFILE) < 0) {
		if (errno == EEXIST) {
			if (unlink(OLDDOORFILE) < 0) {
				logerror("unlink of symlink to doorfile"
					"failed\n");
				exit(1);
			}
		} else {
			logerror("symlink to doorfile failed\n");
			exit(1);
		}
	}
	DPRINT0(5, "door_create succeeded\n");
	(void) fdetach(DOORFILE);	/* just in case... */

	if (fattach(door, DOORFILE) < 0) {
		logerror("doorfile fattach failed\n");
		exit(1);
	}
	DPRINT1(5, "door attached to %s\n", DOORFILE);
	/*
	 * create pidfile anyway, so those using it to control
	 * syslogd (with kill `cat /etc/syslog.pid` perhaps)
	 * don't get broken.
	 */
	if ((fd = open(PidFile, O_RDWR | O_CREAT |O_TRUNC, 0644)) >= 0) {
		(void) fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		(void) sprintf(line, "%ld\n", getpid());
		if (write(fd, line, strlen(line)) < 0) {
			(void) sprintf(line, "Cannot write pid file (%s)\n",
				PidFile);
			errno = 0;
			logerror(line);
			DPRINT2(1, "cannot write pid file %s (%d)\n",
				PidFile, errno);
		}
	} else {
		(void) sprintf(line, "cannot create pid file %s\n", PidFile);
		logerror(line);
		DPRINT2(1, "cannot create pid file %s (%d)\n", PidFile, errno);
	}
}

/*
 * the 'server' function that we export via the door. It does
 * nothing but return.
 */
/*ARGSUSED*/
static void
server(void *cookie, char *argp, size_t arg_size, door_desc_t *dp, uint_t n)
{
	(void) door_return(NULL, 0, NULL, 0);
	/* NOTREACHED */
}

/*
 * checkm4 - used to verify that the external utilities that
 * syslogd depends on are where we expect them to be.
 * Returns 0 if all utilities are found, > 0 if any are missing.
 * Also logs errors so user knows what's missing
 */
static int
checkm4(void)
{
	int notfound = 0;
	int saverrno;

	if (access("/usr/ccs/bin/m4", X_OK) < 0) {
		saverrno = errno;
		logerror("/usr/ccs/bin/m4");
		DPRINT1(1, "/usr/ccs/bin/m4 - access returned %d\n", saverrno);
		notfound++;
	}

	return (notfound);
}

/*
 *  INIT -- Initialize syslogd from configuration table, start up
 *  input and logger threads. This routine is called only once. To
 *  really "re-initialize" syslogd now execs itself.
 */
static void
init(void)
{
	struct utsname *up;
	pthread_attr_t sys_attr, net_attr, log_attr;

	DPRINT0(2, "init\n");

	/* hand-craft a host_list_t entry for our local host name */
	if ((up = malloc(sizeof (struct utsname))) == NULL) {
		MALLOC_FAIL_EXIT;
	}
	(void) uname(up);
	LocalHostName.hl_cnt = 1;
	if ((LocalHostName.hl_hosts = malloc(sizeof (char *))) == NULL) {
		MALLOC_FAIL_EXIT;
	}
	if ((LocalHostName.hl_hosts[0] = strdup(up->nodename)) == NULL) {
		(void) free(LocalHostName.hl_hosts);
		MALLOC_FAIL_EXIT;
	}
	(void) free(up);
	/* also hand craft one for use if name resolution fails */
	NullHostName.hl_cnt = 1;
	if ((NullHostName.hl_hosts = malloc(sizeof (char *))) == NULL) {
		MALLOC_FAIL_EXIT;
	}
	if ((NullHostName.hl_hosts[0] = strdup("name lookup failed")) == NULL) {
		MALLOC_FAIL_EXIT;
	}

	getnets();

	/*
	 * Start up configured theads
	 */
	conf_init();

	/*
	 * allocate thread stacks for the 3 persistant threads
	 */
	if ((stack_ptr = alloc_stacks(3)) == NULL) {
		perror("alloc_stacks");
		exit(1);
	}

	if (Debug) {
		dumpstats(STDOUT_FILENO);
	}

	(void) dataq_init(&inputq);	/* init the input queue */

	if (pthread_attr_init(&sys_attr) != 0) {
		logerror("pthread_attr_init failed");
		exit(1);
	}
	if (pthread_attr_init(&log_attr) != 0) {
		logerror("pthread_attr_init failed");
		exit(1);
	}
	if (pthread_attr_init(&net_attr) != 0) {
		logerror("pthread_attr_init failed");
		exit(1);
	}
#if defined(USE_BOUND_THREADS)
	(void) pthread_attr_setscope(&sys_attr, PTHREAD_SCOPE_SYSTEM);
	(void) pthread_attr_setscope(&log_attr, PTHREAD_SCOPE_SYSTEM);
	(void) pthread_attr_setscope(&net_attr, PTHREAD_SCOPE_SYSTEM);
#else
	(void) pthread_attr_setscope(&sys_attr, PTHREAD_SCOPE_PROCESS);
	(void) pthread_attr_setscope(&log_attr, PTHREAD_SCOPE_PROCESS);
	(void) pthread_attr_setscope(&net_attr, PTHREAD_SCOPE_PROCESS);
#endif
	(void) pthread_attr_setstacksize(&sys_attr, stacksize);
	(void) pthread_attr_setstackaddr(&sys_attr, stack_ptr);
	stack_ptr += stacksize + redzonesize;
	if (pthread_create(&sys_thread, &sys_attr, sys_poll, NULL) != 0) {
		logerror("pthread_create failed");
		exit(1);
	}
	(void) pthread_attr_setstacksize(&net_attr, stacksize);
	(void) pthread_attr_setstackaddr(&net_attr, stack_ptr);
	stack_ptr += stacksize + redzonesize;
	if (turnoff == 0) {
		if (pthread_create(&net_thread, &net_attr, net_poll,
					NULL) != 0) {
			logerror("pthread_create failed");
			exit(1);
		}
	}
	(void) pthread_attr_setstacksize(&log_attr, stacksize);
	(void) pthread_attr_setstackaddr(&log_attr, stack_ptr);
	stack_ptr += stacksize + redzonesize;
	if (pthread_create(&log_thread, &log_attr, logmsg, NULL) != 0) {
		logerror("pthread_create failed");
		exit(1);
	}
	Initialized = 1;
	curalarm = MarkInterval * 60 / MARKCOUNT;
	(void) alarm((unsigned)curalarm);
	DPRINT1(2, "Next alarm in %d seconds\n", curalarm);
	DPRINT0(1, "syslogd: restarted\n");
}

/*
 * will print a bunch of debugging stats on 'fd'
 */
static void
dumpstats(int fd)
{
	FILE *out;
	struct filed *f;
	int i;
	char users[1024];
	char cbuf[30];

	if ((out = fdopen(fd, "w+")) == NULL)
		return;

	(void) fprintf(out, "\n  syslogd: version %s\n", Version);
	(void) fprintf(out, "  Started: %s", ctime_r(&start_time, cbuf));
	(void) fprintf(out, "Input message count: system %d, network %d\n",
		sys_msg_count, net_msg_count);
	(void) fprintf(out, "# Outputs: %d\n\n", nlogs);
	for (f = Files; f < &Files[nlogs]; f++) {
		for (i = 0; i <= LOG_NFACILITIES; i++)
			if (f->f_pmask[i] == NOPRI)
				(void) fprintf(out, "X ");
			else
				(void) fprintf(out, "%d ", f->f_pmask[i]);
		(void) fprintf(out, "%s: ", TypeNames[f->f_type]);
		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
			(void) fprintf(out, "%s", f->f_un.f_fname);
			break;
		case F_FORW:
			(void) fprintf(out, "%s", f->f_un.f_forw.f_hname);
			break;
		case F_USERS:
			for (i = 0; i < MAXUNAMES &&
				 *f->f_un.f_uname[i]; i++) {
				if (!i)
					(void) fprintf(out, "%s",
						f->f_un.f_uname[i]);
				else
					(void) fprintf(out, ", %s",
						f->f_un.f_uname[i]);
			}
			break;
		}
		(void) fprintf(out, "\n");
	}
	(void) fprintf(out, "\n\n\n\t\tPer File Statistics\n");
	(void) fprintf(out, "%-24s\tTot\tDups\tNofwd\tErrs\n", "File");
	(void) fprintf(out, "%-24s\t---\t----\t-----\t----\n", "----");
	for (f = Files; f < &Files[nlogs]; f++) {
		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
			(void) fprintf(out, "%-24s", f->f_un.f_fname);
			break;
		case F_WALL:
			(void) fprintf(out, "%-24s", TypeNames[f->f_type]);
			break;
		case F_FORW:
			(void) fprintf(out, "%-24s", f->f_un.f_forw.f_hname);
			break;
		case F_USERS:
			for (i = 0; i < MAXUNAMES &&
				 *f->f_un.f_uname[i]; i++) {
				if (!i)
					(void) strcpy(users,
						f->f_un.f_uname[i]);
				else {
					(void) strcat(users, ",");
					(void) strcat(users,
						f->f_un.f_uname[i]);
				}
			}
			(void) fprintf(out, "%-24s", users);
			break;
		}
		(void) fprintf(out, "\t%d\t%d\t%d\t%d\n",
			f->f_stat.total, f->f_stat.dups,
			f->f_stat.cantfwd, f->f_stat.errs);
	}
	(void) fprintf(out, "\n\n");
	if (Debug && fd == 1)
		return;
	(void) fclose(out);
}

/*
** conf_init - This routine is code seperated from the
** init routine in order to be re-callable when we get
** a SIGHUP signal.
*/
static void
conf_init(void)
{
	char *p;
	int i;
	struct filed *f;
	char *m4argv[4];
	int m4argc = 0;
	conf_t cf;

	DPRINT0(2, "conf_init\n");

	m4argv[m4argc++] = "m4";

	if (amiloghost() == 1) {
		DPRINT0(1, "I am loghost\n");
		m4argv[m4argc++] = "-DLOGHOST=1";
	}

	m4argv[m4argc++] = ConfFile;
	m4argv[m4argc] = NULL;

	/*
	 * Make sure the configuration file and m4 exist, and then parse
	 * the configuration file with m4.  If any of these fail, resort
	 * to our hardcoded fallback configuration.
	 */

	if (access(ConfFile, R_OK) == -1) {
		DPRINT1(1, "cannot open %s\n", ConfFile);
		logerror("can't open configuration file");
		/* CSTYLED */
		Files = (struct filed *) &fallback; /*lint !e545 */
		cfline("*.ERR\t/dev/sysmsg", 0, &Files[0]);
		cfline("*.PANIC\t*", 0, &Files[1]);
		nlogs = 2;
		goto nofile;
	}

	if (checkm4() != 0 || conf_open(&cf, "/usr/ccs/bin/m4", m4argv) == -1) {
		/* CSTYLED */
		Files = (struct filed *) &fallback; /*lint !e545 */
		cfline("*.ERR\t/dev/sysmsg", 0, &Files[0]);
		cfline("*.PANIC\t*", 0, &Files[1]);
		nlogs = 2;
		goto nofile;
	}

	/* Count the number of lines which are not blanks or comments */
	nlogs = 0;
	while ((p = conf_read(&cf)) != NULL) {
		if (p[0] != '\0' && p[0] != '#')
			nlogs++;
	}

	Files = (struct filed *)malloc(sizeof (struct filed) * nlogs);

	if (!Files) {
		DPRINT0(1, "malloc failed - can't allocate 'Files' array\n");
		/* CSTYLED */
		Files = (struct filed *)&fallback; /*lint !e545 */
		cfline("*.ERR\t/dev/sysmsg", 0, &Files[0]);
		cfline("*.PANIC\t*", 0, &Files[1]);
		nlogs = 2;
		conf_close(&cf);
		goto nofile;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	conf_rewind(&cf);
	f = Files;
	i = 0;
	while (((p = conf_read(&cf)) != NULL) && (f < &Files[nlogs])) {
		i++;
		/* check for end-of-section */
		if (p[0] == '\0' || p[0] == '#')
			continue;

		cfline(p, i, f++);
	}

	conf_close(&cf);

	/*
	 * See if marks are to be written to any files.  If so, set up a
	 * timeout for marks.
	 */
nofile:
	Marking = 0;

	/*
	 * allocate thread stacks - one for each logger thread.
	 */
	if ((cstack_ptr = alloc_stacks(nlogs)) == NULL) {
		perror("alloc_stacks");
		exit(1);
	}

	/* And now one thread for each configured file */
	for (f = Files; f < &Files[nlogs]; f++) {
		if (filed_init(f) != 0) {
			logerror("pthread_create failed");
			exit(1);
		}

		pthread_mutex_lock(&cft);
		++conf_threads;
		pthread_mutex_unlock(&cft);

		if (f->f_type != F_UNUSED &&
			f->f_pmask[LOG_NFACILITIES] != NOPRI)
			Marking = 1;
	}

	if (conf_threads) {
		pthread_cond_signal(&log_threads_configured);
	}

	open_door();
}

/*
 * filed init - initialize fields in a file descriptor struct
 * this is called before multiple threads are running, so no mutex
 * needs to be held at this time.
 */
static
filed_init(struct filed *f)
{
	pthread_attr_t stack_attr;

	if (pthread_mutex_init(&f->filed_mutex, NULL) != 0) {
		logerror("pthread_mutex_init failed");
		return (-1);
	}

	DPRINT1(5, "dataq_init for queue %p\n", &f->f_queue);
	(void) dataq_init(&f->f_queue);

	if (pthread_attr_init(&stack_attr) != 0) {
		logerror("pthread_attr_init failed");
		return (-1);
	}

	(void) pthread_attr_setstacksize(&stack_attr, stacksize);
	(void) pthread_attr_setstackaddr(&stack_attr, cstack_ptr);
	cstack_ptr += stacksize + redzonesize;
	if (pthread_create(&f->f_thread, NULL, logit, (void *)f) != 0) {
		logerror("pthread_create failed");
		pthread_attr_destroy(&stack_attr);
		return (-1);
	}

	f->f_msgflag = 0;
	f->f_prevmsg.msg[0] = '\0';
	f->f_prevmsg.flags = 0;
	f->f_prevmsg.pri = 0;
	f->f_prevmsg.host[0] = '\0';

	f->f_current.msg[0] = '\0';
	f->f_current.flags = 0;
	f->f_current.pri = 0;
	f->f_current.host[0] = '\0';

	f->f_prevcount = 0;

	f->f_stat.flag = 0;
	f->f_stat.total = 0;
	f->f_stat.dups = 0;
	f->f_stat.cantfwd = 0;
	f->f_stat.errs = 0;

	pthread_attr_destroy(&stack_attr);
	return (0);
}


/*
 * Crack a configuration file line
 */
static void
cfline(char *line, int lineno, struct filed *f)
{
	char *p;
	char *q;
	int i;
	char *bp;
	int pri;
	char buf[MAXLINE];
	char xbuf[200];
	char ebuf[100];
	mode_t fmode, omode = O_WRONLY|O_APPEND|O_NOCTTY;
	struct stat sbuf;

	DPRINT1(1, "cfline(%s)\n", line);

	errno = 0;	/* keep sys_errlist stuff out of logerror messages */

	/* clear out file entry */
	bzero((char *)f, sizeof (*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = NOPRI;

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t'; ) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
			q++;

		/* decode priority name */
		pri = decode(buf, PriNames);
		if (pri < 0) {
			(void) sprintf(xbuf,
				"line %d: unknown priority name \"%s\"",
				lineno, buf);
			logerror(xbuf);
			return;
		}

		/* scan facilities */
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = (uchar_t)pri;
			else {
				i = decode(buf, FacNames);
				if (i < 0) {
					(void) sprintf(xbuf, "line %d: "
						"unknown facility name \"%s\"",
						lineno, buf);
					logerror(xbuf);
					return;
				}
				f->f_pmask[i >> 3] = (uchar_t)pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	switch (*p) {
	case '\0':
		(void) sprintf(xbuf, "line %d: no action part", lineno);
		errno = 0;
		logerror(xbuf);
		break;

	case '@':
		(void) strncpy(f->f_un.f_forw.f_hname, ++p, SYS_NMLN);
		if (logforward(f, ebuf) != 0) {
			(void) sprintf(xbuf, "line %d: %s", lineno, ebuf);
			logerror(xbuf);
			break;
		}
		f->f_type = F_FORW;
		break;

	case '/':
		(void) strncpy(f->f_un.f_fname, p, MAXPATHLEN);
		if (stat(p, &sbuf) < 0) {
			logerror(p);
			break;
		}
		/*
		 * don't block trying to open a pipe
		 * with no reader on the other end
		 */
		fmode = 0; 	/* reset each pass */
		if (S_ISFIFO(sbuf.st_mode))
			fmode = O_NONBLOCK;

		f->f_file = open(p, omode|fmode);
		if (f->f_file < 0) {
			if (fmode && errno == ENXIO) {
				char ebuf[80];
				(void) sprintf(ebuf, "%s - no reader", p);
				errno = 0;
				logerror(ebuf);
			} else
				logerror(p);
			break;
		}

		/*
		** Fifos are initially opened NONBLOCK
		** to insure we don't hang, but once
		** we are open, we need to change the
		** behavior back to blocking, otherwise
		** we may get write errors, and the log
		** will get closed down the line.
		*/
		if (S_ISFIFO(sbuf.st_mode))
			(void) fcntl(f->f_file, F_SETFL, omode);

		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			untty();
		} else
			f->f_type = F_FILE;

		if ((strcmp(p, ctty) == 0) || (strcmp(p, sysmsg) == 0))
			f->f_type = F_CONSOLE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void) strncpy(f->f_un.f_uname[i], p, UNAMESZ);
			if ((q - p) > UNAMESZ)
				f->f_un.f_uname[i][UNAMESZ] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
static int
decode(char *name, struct code *codetab)
{
	struct code *c;
	char *p;
	char buf[40];

	if (isdigit(*name))
		return (atoi(name));

	(void) strncpy(buf, name, sizeof (buf) - 1);
	for (p = buf; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	for (c = codetab; c->c_name; c++)
		if (!(strcmp(buf, c->c_name)))
			return (c->c_val);

	return (-1);
}

static int
ismyaddr(struct netbuf *nbp)
{
	int i;

	if (nbp == NULL)
		return (0);

	for (i = 1; i < Ninputs; i++) {
		if (nbp->len == Myaddrs[i]->len &&
			same(nbp->buf, Myaddrs[i]->buf, nbp->len))
			return (1);
	}
	return (0);
}

static void
getnets(void)
{
	struct nd_hostserv hs;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	struct netbuf *nbp;
	int i, inputs;
	void *handle;
	char *uap;

	hs.h_host = HOST_SELF;
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL)
		return;
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				DPRINT1(1, "getnets() found %d addresses",
					nap->n_cnt);
				nbp = nap->n_addrs;
				if (nap->n_cnt > 0) {
					DPRINT0(1, ", they are: ");
					for (i = 0; i < nap->n_cnt; i++) {
						if ((uap = taddr2uaddr(ncp,
							nbp)) != (char *)NULL) {
							DPRINT1(1, "%s ", uap);
							if (!i)
							(void) strncpy
								(mynumericaddr,
								uap,
								SYS_NMLN);
							free(uap);
						}
						nbp++;
					}
					DPRINT0(1, "\n");
				}
				/*
				 * all malloc failures here are fatal
				 */
				if (nap->n_cnt > 0) {
					nbp = nap->n_addrs;
					inputs = nap->n_cnt;
					Nfd = (struct pollfd *)
					    malloc(inputs *
						    sizeof (struct pollfd));
					if (Nfd == NULL) {
						MALLOC_FAIL_EXIT;
					}
					Ncf = (struct netconfig *)
					    malloc(inputs *
						    sizeof (struct netconfig));
					if (Ncf == NULL) {
						MALLOC_FAIL_EXIT;
					}
					Myaddrs = (struct netbuf **)
					    malloc(inputs *
						    sizeof (struct netbuf **));
					if (Myaddrs == NULL) {
						MALLOC_FAIL_EXIT;
					}
					Udp = (struct t_unitdata **)
					    malloc(inputs *
						sizeof (struct t_unitdata **));
					if (Udp == NULL) {
						MALLOC_FAIL_EXIT;
					}
					Errp = (struct t_uderr **)
					    malloc(inputs *
						    sizeof (struct t_uderr **));
					if (Errp == NULL) {
						MALLOC_FAIL_EXIT;
					}
					Ninputs = 0;
					for (i = 0; i < nap->n_cnt; i++) {
						add(ncp, nbp);
						nbp++;
					}
				}
				netdir_free((void *)nap, ND_ADDRLIST);
			}
		}
	}
	endnetconfig(handle);
}

static void
add(struct netconfig *ncp, struct netbuf *nbp)
{
	int fd;
	struct t_bind bind;
	struct t_bind *bound;

	if (turnoff == 0) {
		fd = t_open(ncp->nc_device, O_RDWR, NULL);
		if (fd < 0)
			return;
		(void) memcpy(&Ncf[Ninputs], ncp, sizeof (struct netconfig));
		bound = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
		bind.addr = *nbp;
		bind.qlen = 0;
		if (t_bind(fd, &bind, bound) < 0) {
			t_close(fd);
			t_free((char *)bound, T_BIND);
			return;
		}
		if ((bind.addr.len != bound->addr.len) ||
			!same(bind.addr.buf, bound->addr.buf, bind.addr.len)) {
			t_close(fd);
			t_free((char *)bound, T_BIND);
			return;
		}
		Udp[Ninputs] = (struct t_unitdata *)t_alloc(fd, T_UNITDATA,
				T_ADDR);
		if (Udp[Ninputs] == NULL) {
			t_close(fd);
			t_free((char *)bound, T_BIND);
			return;
		}
		Errp[Ninputs] = (struct t_uderr *)t_alloc(fd, T_UDERROR,
				T_ADDR);
		if (Errp[Ninputs] == NULL) {
			t_close(fd);
			t_free((char *)Udp[Ninputs], T_UNITDATA);
			t_free((char *)bound, T_BIND);
			return;
		}
		Nfd[Ninputs].fd = fd;
		Nfd[Ninputs].events = POLLIN;
		Myaddrs[Ninputs++] = &bound->addr;
	}
}

static int
logforward(struct filed *f, char *ebuf)
{
	struct nd_hostserv hs;
	struct netbuf *nbp;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	void *handle;
	char *hp;

	hp = f->f_un.f_forw.f_hname;
	hs.h_host = hp;
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL) {
		(void) strcpy(ebuf,
			"unable to rewind the netconfig database");
		errno = 0;
		return (-1);
	}
	nap = (struct nd_addrlist *)NULL;
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				nbp = nap->n_addrs;
				break;
			}
		}
	}
	if (ncp == NULL) {
		endnetconfig(handle);
		(void) sprintf(ebuf, "WARNING: %s could not be resolved", hp);
		errno = 0;
		return (-1);
	}
	if (nap == (struct nd_addrlist *)NULL) {
		endnetconfig(handle);
		(void) sprintf(ebuf, "unknown host %s", hp);
		errno = 0;
		return (-1);
	}
	/* CSTYLED */
	if (ismyaddr(nbp)) { /*lint !e644 */
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) sprintf(ebuf, "host %s is this host - logging loop",
			hp);
		errno = 0;
		return (-1);
	}
	f->f_un.f_forw.f_addr.buf = malloc(nbp->len);
	if (f->f_un.f_forw.f_addr.buf == NULL) {
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) strcpy(ebuf, "malloc");
		return (-1);
	}
	bcopy(nbp->buf, f->f_un.f_forw.f_addr.buf, nbp->len);
	f->f_un.f_forw.f_addr.len = nbp->len;
	f->f_file = t_open(ncp->nc_device, O_RDWR, NULL);
	if (f->f_file < 0) {
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) strcpy(ebuf, "t_open");
		return (-1);
	}
	netdir_free((void *)nap, ND_ADDRLIST);
	endnetconfig(handle);
	if (t_bind(f->f_file, NULL, NULL) < 0) {
		(void) strcpy(ebuf, "t_bind");
		t_close(f->f_file);
		return (-1);
	}
	return (0);
}

static int
amiloghost(void)
{
	struct nd_hostserv hs;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	struct netbuf *nbp;
	int i, fd;
	void *handle;
	char *uap;
	struct t_bind bind, *bound;
	/*
	 * we need to know if we are running on the loghost. This is
	 * checked by binding to the address associated with "loghost"
	 * and "syslogd" service over the connectionless transport
	 */
	hs.h_host = "loghost";
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL)
		return (0);
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				nbp = nap->n_addrs;
				for (i = 0; i < nap->n_cnt; i++) {
					if ((uap = taddr2uaddr(ncp, nbp))
						!= (char *)NULL) {
						DPRINT1(1, "amiloghost() "
							"testing %s\n", uap);
					}
					free(uap);

					fd = t_open(ncp->nc_device, O_RDWR,
						NULL);
					if (fd < 0) {
						endnetconfig(handle);
						return (0);
					}
					bound = (struct t_bind *)t_alloc(fd,
						T_BIND, T_ADDR);
					bind.addr = *nbp;
					bind.qlen = 0;
					if (t_bind(fd, &bind, bound) == 0) {
						t_close(fd);
						t_free((char *)bound, T_BIND);
						netdir_free((void *)nap,
							ND_ADDRLIST);
						endnetconfig(handle);
						return (1);
					} else {
						t_close(fd);
						t_free((char *)bound, T_BIND);
					}
					nbp++;
				}
				netdir_free((void *)nap, ND_ADDRLIST);
			}
		}
	}
	endnetconfig(handle);
	return (0);
}

static int
same(char *a, char *b, unsigned int n)
{
	assert(a != NULL && b != NULL);
	if (n == 0)
		return (0);
	while (n-- > 0)
		if (*a++ != *b++)
			return (0);
	return (1);
}

/*
 * allocates a new message structure, initializes it
 * and returns a pointer to it
 */
static log_message_t *
new_msg(void)
{
	log_message_t *lm;

	if ((lm = malloc(sizeof (log_message_t))) == NULL)
		return ((log_message_t *)NULL);

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*lm))

	if (pthread_mutex_init(&lm->msg_mutex, NULL) != 0)
		return ((log_message_t *)NULL);
	lm->refcnt = 0;
	lm->source = -1;
	lm->pri = 0;
	lm->flags = 0;
	lm->hlp = NULL;
	lm->msg[0] = '\0';
	lm->curaddr[0] = '\0';
	DPRINT1(3, "new_msg(%p)\n", lm);

	return (lm);
}

/*
 * frees a message structure - should only be called if
 * the refcount is 0
 */
static void
free_msg(log_message_t *lm)
{
	assert(lm != NULL && lm->refcnt == 0);
	if (lm->hlp != NULL)
		freehl(lm->hlp);
	DPRINT1(3, "free_msg(%p)\n", lm);
	free(lm);
	lm = (log_message_t *)NULL;
}

/*
 * fix for bug 1244433 - ensure that the string we print is
 * 8-bit clean (i.e. does not contain ascii control chars).
 */
static void
filter_string(char *mbstr, char *filtered)
{
	int i, mbyte_sz = 0;
	wchar_t wdchar;
	unsigned char c;

	assert(mbstr != NULL && filtered != NULL);
	if (MB_CUR_MAX > 1) {
		while (*mbstr != '\0') {
			if ((mbyte_sz = mbtowc(&wdchar, mbstr,
				MB_CUR_MAX)) == -1) {
				mbstr++;
				continue;
			}
			/* copy printable stuff into 2nd string */
			if (wdchar == (wchar_t)'\t' ||
				wdchar == (wchar_t)'\n' ||
				iswprint(wdchar)) {
				for (i = 0; i < mbyte_sz; i++)
					*filtered++ = *mbstr++;
			} else
				mbstr += mbyte_sz;
		}
	} else {
		while (*mbstr != '\0') {
			c = *mbstr;
			if (c == '\t' || c == '\n' || isprint(c))
				*filtered++ = *mbstr++;
			else
				mbstr++;
		}
	}
	*filtered = '\0';
}


static char *
alloc_stacks(int numstacks)
{
	size_t pagesize, mapsize;
	char *stack_top;
	int devzero;
	char *addr;
	int i;

	pagesize = (size_t)sysconf(_SC_PAGESIZE);
	/*
	 * stacksize and redzonesize are global so threads
	 * can be created elsewhere and refer to the sizes
	 */
	stacksize = (size_t)roundup(sysconf(_SC_THREAD_STACK_MIN) +
		DEFAULT_STACKSIZE, pagesize);
	redzonesize = (size_t)roundup(DEFAULT_REDZONESIZE, pagesize);

	if ((devzero = open("/dev/zero", O_RDWR)) == NULL)
		return (NULL);
	/*
	 * allocate an additional "redzonesize" chunk in addition
	 * to what we require, so we can create a redzone at the
	 * bottom of the last stack as well.
	 */
	mapsize = redzonesize + numstacks * (stacksize + redzonesize);
	stack_top = mmap(NULL, mapsize, PROT_READ|PROT_WRITE,
		MAP_PRIVATE, devzero, 0);
	if (stack_top == MAP_FAILED)
		return (NULL);

	addr = stack_top;
	/*
	 * this loop is intentionally <= instead of <, so we can
	 * protect the redzone at the bottom of the last stack
	 */
	for (i = 0; i <= numstacks; i++) {
		(void) mprotect(addr, redzonesize, PROT_NONE);
		addr += stacksize + redzonesize;
	}
	(void) close(devzero);
	return ((char *)(stack_top + redzonesize));
}

static void
dealloc_stacks(int numstacks)
{
	size_t pagesize, mapsize;

	pagesize = (size_t)sysconf(_SC_PAGESIZE);

	stacksize = (size_t)roundup(sysconf(_SC_THREAD_STACK_MIN) +
	    DEFAULT_STACKSIZE, pagesize);

	redzonesize = (size_t)roundup(DEFAULT_REDZONESIZE, pagesize);

	mapsize = redzonesize + numstacks * (stacksize + redzonesize);
	(void) munmap(cstack_ptr - mapsize, mapsize);
}

static void
filed_destroy(struct filed * f)
{
	(void) dataq_destroy(&f->f_queue);
	pthread_mutex_destroy(&f->filed_mutex);
}

static void
close_door(void)
{
	(void) fdetach(DOORFILE);
	(void) unlink(PidFile);
	(void) unlink(OLDDOORFILE);
}
