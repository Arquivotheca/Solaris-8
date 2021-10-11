/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lpmove.c	1.14	99/08/06 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <locale.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/list.h>
#include <print/job.h>

/*
 *	 lpr/lp
 *	This program will submit print jobs to a spooler using the BSD
 *	printing protcol as defined in RFC1179, plus some extension for
 *	support of additional lp functionality.
 */

extern char *optarg;
extern int optind, opterr, optopt;
extern char *getenv(const char *);


/*
 *	creates a new job moves/modifies control data and data files.
 *	If a job is moved, we should return 1 regardless of whether we
 *	killed the daemon or not.  This will allow us to attempt to start
 *	the transfer agent for any jobs in /var/spool/print.
 */
static int
vjob_reset_destination(job_t *job, va_list ap)
{
	char	*user = va_arg(ap, char *),
		buf[BUFSIZ];
	int	id = va_arg(ap, int),
		lock;
	ns_bsd_addr_t *src = va_arg(ap, ns_bsd_addr_t *),
		*dst = va_arg(ap, ns_bsd_addr_t *);


	if ((id != -1) && (job->job_id != id))
		return (0);
	if (strcmp(job->job_server, src->server) != 0)
		return (0);
	if ((strcmp(user, "root") != 0) &&
	    (strcmp(user, job->job_user) != 0))
		return (0);

	while ((lock = get_lock(job->job_cf->jf_src_path, 0)) < 0)
		(void) kill_process(job->job_cf->jf_src_path);

	/* just do it */
	(void) sprintf(buf, "%s:%s\n", dst->server, dst->printer);
	(void) ftruncate(lock, 0);
	(void) write(lock, buf, strlen(buf));
	(void) close(lock);
	(void) printf(gettext("moved: %s-%d to %s-%d\n"), src->printer,
		job->job_id, dst->printer, job->job_id);
	return (1);
}


static int
job_move(char *user, int id, ns_bsd_addr_t *src_binding,
				ns_bsd_addr_t *dst_binding)
{
	job_t	**list = NULL;
	int	rc = 0;

	if ((list = job_list_append(NULL, src_binding->printer,
				    src_binding->server, SPOOL_DIR)) != NULL)
		rc = list_iterate((void **)list,
				(VFUNC_T)vjob_reset_destination, user, id,
				src_binding, dst_binding);
	return (rc);

}

/*
 *	move print jobs from one queue to another.  This gets the lock
 *  file (killing the locking process if necessary), Moves the jobs, and
 *  restarts the transfer daemon.
 */

#define	OLD_LPMOVE	"/usr/lib/lp/local/lpmove"

main(int ac, char *av[])
{
	char	*program = NULL,
		*dst = NULL,
		*user = get_user_name();
	char	**argv = NULL;
	int	remote_moved = 0,
		i, argc = 0;
	ns_bsd_addr_t * dst_binding;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (access(OLD_LPMOVE, F_OK) == 0) {	/* copy args for local move */
		argv = (char **)calloc(ac + 1, sizeof (char *));
		if (av[0] != NULL)
			argv[argc++] = strdup(av[0]);
	}

	(void) chdir(SPOOL_DIR);

	/*
	 * Get the destination; use binding to set it; the input may
	 * be an alias, not a printer name
	 */
	dst = av[--ac];
	if ((dst_binding = ns_bsd_addr_get_name(dst)) == NULL) {
		(void) fprintf(stderr, gettext("%s: unknown printer\n"), dst);
		return (-1);
	}

	for (i = 1; i < ac; i++) {
		ns_bsd_addr_t *src_binding = NULL;
		char *src = av[i], *s;
		int id = -1;

		/*
		 * if the argument contains a '-', see if it is a request
		 * or a printer.
		 */
		if ((s = strrchr(src, '-')) != NULL) {
			*(s++) = NULL;

			id = atoi(s);
			if ((id <= 0) && (errno == EINVAL)) {
				id = -1;
				*(--s) = '-';
			}
		}

		/*
		 * get the binding for the printer, so we can use the printer
		 * name from the binding to move the job(s)
		 */
		if ((src_binding = ns_bsd_addr_get_name(src)) == NULL) {
			(void) fprintf(stderr,
					gettext("%s: unknown printer\n"), src);
			return (-1);
		}

		if (job_move(user, id, src_binding, dst_binding) == 0) {
			/*
			 * There was nothing "remote" to move, so add it to the
			 * list of "local" items to move.  Use the printer name
			 * from the binding, because that is the one that
			 * lpsched is most likely to understand.
			 */
			char buf[BUFSIZ];

                       	(void) snprintf(buf, sizeof (buf), "%s%s%s",
                                       	src_binding->printer,
					(id == -1 ? "" : "-"),
					(id == -1 ? "" : s));
			argv[argc++] = strdup(buf);
		} else
			remote_moved++;
	}

	/* if we moved a "remote" job(s), try to (re)start the transfer agent */
	if (remote_moved != 0)
		start_daemon(1);

	/* if there is something "local" to move, try it */
	if ((argv != NULL) && (remote_moved != ac)) {
		argv[argc++] = strdup(dst_binding->printer);

		/* limit ourselves to real user's perms before exec'ing */
		(void) setuid(getuid());
		(void) execv(OLD_LPMOVE, argv);
	}

	return (0);
}
