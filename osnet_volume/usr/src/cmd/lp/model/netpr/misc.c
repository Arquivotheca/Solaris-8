
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)misc.c 1.10     96/04/15 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "netpr.h"
#include "netdebug.h"

extern char *strtok_r(char *, const char *, char **);

int
check_file(char * filename)
{
	struct stat status;

	if (filename  == NULL)
		return (-1);

	/* Checking read permission */
	if (access(filename, R_OK) < 0)
		return (-1);

	if (stat(filename, &status) < 0)
		return (-1);

	/* Checking for regular file */
	if (S_ISREG(status.st_mode) == 0) {
		errno = EISDIR;
		return (-1);
	}

	/* Checking for empty file */
	if (status.st_size == 0) {
		errno = ESRCH;
		return (-1);
	}
	return (status.st_size);
}


/*
 * allocate the space; fill with input
 */
char *
alloc_str(char * instr)
{
	char * outstr;

	outstr = (char *)malloc(strlen(instr) + 1);
	ASSERT(outstr, MALLOC_ERR);
	(void) memset(outstr, 0, strlen(instr) + 1);
	(void) strcpy(outstr, instr);

	return (outstr);
}

np_job_t *
init_job()
{
	np_job_t * job;

	job = (np_job_t *)malloc(sizeof (np_job_t));

	job->filename = NULL;
	job->request_id = NULL;
	job->printer = NULL;
	job->dest = NULL;
	job->title = NULL;
	job->protocol = BSD;
	job->username = NULL;
	job->timeout = 0;
	job->banner = BANNER;
	job->filesize = 0;

	return (job);
}

void
tell_lptell(int type, char *fmt, ...)
{
	char msg[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
	(void) vsprintf(msg, fmt, ap);
	va_end(ap);

	if (msg == NULL)
		return;

	switch (type) {
	case ERRORMSG:
		(void) fprintf(stderr, "%%%%[PrinterError: %s ]%%%%\n", msg);
		break;
	case OKMSG:
		/* In this case, the message is the job request-id */
		(void) fprintf(stderr,
		"%%%%[job: %s status: ok source: Netpr]%%%%\n", msg);
		break;
	default:
		/* unknown type, ignore */
		break;
	}


}


/*
 * Parse destination
 * bsd: <printer_host>[:<printer_vendor_defined_name]
 * tcp: <printer_host>[:port_number]
 */

void
parse_dest(char * dest, char **str1, char **str2, char * sep)
{
	char * tmp;
	char * nexttok;

	*str1 = NULL;
	*str2 = NULL;

	if (dest != NULL) {
		tmp = (char *)strtok_r(dest, sep, &nexttok);
		if (tmp != NULL)
			*str1 = strdup(tmp);
		tmp = (char *)strtok_r(NULL, sep, &nexttok);
		if (tmp != NULL)
			*str2 = strdup(tmp);
	}

} 

/*
 * void panic call
 * used with ASSERT macro; gives us a place to stop the debugger
 */
void
panic()
{
}
