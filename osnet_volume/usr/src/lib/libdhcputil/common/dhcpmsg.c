/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcpmsg.c	1.3	99/09/01 SMI"

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <libintl.h>
#include <string.h>
#include <limits.h>

#include "dhcpmsg.h"

/* LINTLIBRARY */

static boolean_t	is_daemon  = B_FALSE;
static boolean_t	is_verbose = B_FALSE;
static char		program[PATH_MAX] = "<unknown>";
static int		debug_level;

static const char	*err_to_string(int);
static int		err_to_syslog(int);

/*
 * dhcpmsg(): logs a message to the console or to syslog
 *
 *   input: int: the level to log the message at
 *	    const char *: a printf-like format string
 *	    ...: arguments to the format string
 *  output: void
 */

void
dhcpmsg(int errlevel, const char *fmt, ...)
{
	va_list		ap;
	char		buf[512];
	char		*errmsg;

	if ((errlevel == MSG_DEBUG2 && (debug_level < 2)) ||
	    (errlevel == MSG_DEBUG && (debug_level < 1)) ||
	    (errlevel == MSG_VERBOSE && !is_verbose))
		return;

	va_start(ap, fmt);

	/*
	 * either log to stderr, or log to syslog.  print out unix
	 * error message if errlevel is MSG_ERR and errno is set
	 */

	if (is_daemon) {
		(void) snprintf(buf, sizeof (buf), (errlevel == MSG_ERR &&
		    errno != 0) ? "%s: %%m\n" : "%s\n", gettext(fmt));
		(void) vsyslog(err_to_syslog(errlevel), buf, ap);
	} else {
		errmsg = strerror(errno);
		if (errmsg == NULL)
			errmsg = "<unknown error>";

		(void) snprintf(buf, sizeof (buf), (errlevel == MSG_ERR &&
		    errno != 0) ? "%s: %s: %s: %s\n" : "%s: %s: %s\n", program,
		    gettext(err_to_string(errlevel)), gettext(fmt), errmsg);

		(void) vfprintf(stderr, buf, ap);
	}

	va_end(ap);
}

/*
 * dhcpmsg_init(): opens and initializes the DHCP messaging facility
 *
 *   input: const char *: the name of the executable
 *	    boolean_t: whether the executable is a daemon
 *	    boolean_t: whether the executable is running "verbosely"
 *	    int: the debugging level the executable is being run at
 *  output: void
 */

void
dhcpmsg_init(const char *program_name, boolean_t daemon, boolean_t verbose,
    int level)
{
	(void) strlcpy(program, program_name, sizeof (program));

	debug_level = level;
	is_verbose = verbose;

	if (daemon) {
		is_daemon = B_TRUE;
		(void) openlog(program, LOG_PID, LOG_DAEMON);
		dhcpmsg(MSG_VERBOSE, "Daemon started");
	}
}

/*
 * dhcpmsg_fini(): closes the DHCP messaging facility.
 *
 *   input: void
 *  output: void
 */

void
dhcpmsg_fini(void)
{
	if (is_daemon) {
		dhcpmsg(MSG_VERBOSE, "Daemon terminated");
		closelog();
	}
}

/*
 * err_to_syslog(): converts a dhcpmsg log level into a syslog log level
 *
 *   input: int: the dhcpmsg log level
 *  output: int: the syslog log level
 */

static int
err_to_syslog(int errlevel)
{
	switch (errlevel) {

	case MSG_DEBUG:
	case MSG_DEBUG2:
		return (LOG_DEBUG);

	case MSG_ERROR:
	case MSG_ERR:
		return (LOG_ERR);

	case MSG_WARNING:
		return (LOG_WARNING);

	case MSG_NOTICE:
		return (LOG_NOTICE);

	case MSG_CRIT:
		return (LOG_CRIT);

	case MSG_VERBOSE:
	case MSG_INFO:
		return (LOG_INFO);
	}

	return (LOG_INFO);
}

/*
 * err_to_string(): converts a log level into a string
 *
 *   input: int: the log level
 *  output: const char *: the stringified log level
 */

static const char *
err_to_string(int errlevel)
{
	switch (errlevel) {

	case MSG_DEBUG:
	case MSG_DEBUG2:
		return ("debug");

	case MSG_ERR:
	case MSG_ERROR:
		return ("error");

	case MSG_WARNING:
		return ("warning");

	case MSG_NOTICE:
		return ("notice");

	case MSG_CRIT:
		return ("CRITICAL");

	case MSG_VERBOSE:
	case MSG_INFO:
		return ("info");
	}

	return ("<unknown>");
}
