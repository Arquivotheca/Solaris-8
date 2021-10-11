#ident	"@(#)log.c	1.4	96/09/06 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "aspppd.h"
#include "log.h"

#define	LOG_FILE	"/var/adm/log/asppp.log"

static FILE	*log_file = stdout;

static char	*timestamp(void);

void
open_log(void)
{
	FILE *f;

	if ((f = freopen(LOG_FILE, "a", stderr)) == NULL)
		log(42, "open_log: fopen failed\n");
	else {
		log_file = f;
		setbuf(log_file, NULL);
		fchmod(fileno(log_file), S_IRUSR | S_IWUSR);
	}
}

static char *
timestamp(void)
{
	static char buf[9];
	time_t stime;
	struct tm *ltime;

	if (time(&stime) > -1) {
		ltime = localtime(&stime);
		if (strftime(buf, sizeof (buf), "%X", ltime) == 0)
			buf[0] = (char)0;
	} else
		buf[0] = (char)0;

	return (buf);
}

void
log(int level, char *fmt, ...)
{
	va_list args;

	if (level > debug)
		return;

	(void) fprintf(log_file, "%s ", timestamp());
	va_start(args, fmt);
	(void) vfprintf(log_file, fmt, args);
	va_end(args);
}

void
fail(char *fmt, ...)
{
	va_list args;

	(void) fprintf(log_file, "%s ", timestamp());
	va_start(args, fmt);
	(void) vfprintf(log_file, fmt, args);
	va_end(args);
	if (errno)
		(void) fprintf(log_file, "         Error %d: %s\n",
				errno, strerror(errno));
	exit(EXIT_FAILURE);
}
