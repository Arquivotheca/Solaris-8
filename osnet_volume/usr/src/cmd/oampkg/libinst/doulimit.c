/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#ident	"@(#)doulimit.c	1.1	95/02/15 SMI"	/* SVr4.0 1.4.1.1	*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <libintl.h>

#define	ERR_SET_ULIMIT	"unable to set ulimit to <%ld> blocks"
#define	ERR_DO_ULIMIT	"An attempt was made to create a file larger than " \
			    "ULIMIT. Source of fault is unknown."
#define	ERR_SCRULIMIT	"Script <%s> attempted to create a file exceeding " \
			    "ULIMIT."

static char *script_name = NULL, *scr_error = NULL;
static struct rlimit ulimit = {RLIM_INFINITY, RLIM_INFINITY};
static struct rlimit dblimit = {RLIM_INFINITY, RLIM_INFINITY};
static int limit_is_set = 0, fail_return = 0;

void ulimit_quit();	/* XFSZ controlled signal handler. */
int clr_ulimit();	/* Clear the user supplied file size limit. */
int set_ulimit(char *script, char *err_msg);
int assign_ulimit(char *fslimit);

extern int	warnflag;

extern void	quit(int exitval);

int
clr_ulimit()
{
	if (limit_is_set) {
		if (script_name)
			free(script_name);
		script_name = NULL;
		if (scr_error)
			free(scr_error);
		scr_error = NULL;
		fail_return = 99;

		/* Clear out the limit to infinity. */
		return (setrlimit(RLIMIT_FSIZE, &dblimit));
	} else
		return (0);
}

/*
 * This sets up the ULIMIT facility for the signal retrieval. This sets up
 * the static pointers to the message constants for indicating where the
 * error occurred.
 */
int
set_ulimit(char *script, char *err_msg)
{
	int n;

	if (limit_is_set) {
		(void) signal(SIGXFSZ, ulimit_quit);
		if (script_name)
			free(script_name);
		script_name = strdup(script);
		if (scr_error)
			free(scr_error);
		scr_error = strdup(err_msg);
		fail_return = 99;

		n = setrlimit(RLIMIT_FSIZE, &ulimit);

		return (n);
	} else
		return (0);

}	

/* Validate ULIMIT and set accordingly. */
int
assign_ulimit(char *fslimit)
{
	rlim_t limit;
	int cnt = 0;

	if (fslimit && *fslimit) {
		/* fslimit must be a simple unsigned integer. */
		do {
			if (!isdigit(fslimit[cnt]))
				return (-1);
		} while (fslimit[++cnt]);

		limit = atol(fslimit);

		ulimit.rlim_cur = (limit * 512); /* fslimit is in blocks */

		limit_is_set = 1;

		return (0);
	} else
		return (-1);
}	

/*
 * This is the signal handler for ULIMIT.
 */
void
ulimit_quit(int n)
{
#ifdef lint
	int i = n;
	n = i;
#endif	/* lint */

	setrlimit(RLIMIT_FSIZE, &dblimit);
	signal(SIGXFSZ, SIG_IGN);

	if (script_name) {
		progerr(gettext(ERR_SCRULIMIT), script_name);
		if (scr_error)
			progerr("%s", scr_error);
	} else
		progerr(gettext(ERR_DO_ULIMIT));

	quit(fail_return);
}
