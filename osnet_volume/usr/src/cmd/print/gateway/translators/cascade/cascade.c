/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cascade.c	1.10	99/03/30 SMI"

/*LINTLIBRARY*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <libintl.h>

#include <print/ns.h>
#include <print/job.h>
#include <print/list.h>
#include <print/misc.h>
#include <print/network.h>


static ns_printer_t *printer_object = NULL;
static ns_bsd_addr_t *printer_addr = NULL;


static char *_rank_suffixes[] = {
	"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
};

static char *
rank_string(const int rank)
{
	static char buf[12];

	if (rank < 0)
		sprintf(buf, gettext("invalid"));
	else if (rank == 0)
		sprintf(buf, gettext("active"));
	else if ((rank > 10) && (rank < 14))
		sprintf(buf, "%dth", rank);
	else
		sprintf(buf, "%d%s", rank, _rank_suffixes[rank % 10]);

	return (buf);
}



/*
 * cascade_spooler_available() always returns 0, because it is really
 * used as a waiting space for contacting another spooler.
 */
int
cascade_spooler_available(const char *printer)
{
	syslog(LOG_DEBUG, "cascade_spooler_available(%s)",
		(printer ? printer : "NULL"));

	if ((printer_object = ns_printer_get_name(printer, NULL)) == NULL)
		return (-1);

	if ((printer_addr = ns_bsd_addr_get_name((char *)printer)) == NULL)
		return (-1);

	return (0);
}


/*
 * cascade_client_access() always returns 0, because all requestors should
 * be allowed to cascade.
 */
int
cascade_client_access(const char *printer, const char *host)
{
	syslog(LOG_DEBUG, "cascade_client_access(%s, %s)",
		(printer ? printer : "NULL"), (host ? host : "NULL"));

	return (0);
}


/*
 * cascade_spooler_accepting_jobs() always returns 0, because all requestors
 * should be allowed to cascade.
 */
int
cascade_spooler_accepting_jobs(const char *printer)
{
	syslog(LOG_DEBUG, "cascade_spooler_accepting_jobs(%s)",
		(printer ? printer : "NULL"));

	return (0);
}


/*
 * cascade_temp_dir() returns the directory to be used as the working
 * directory for the cascaded spooler.  All data files will be expected to
 * be stored and retreived from this directory
 */
char *
cascade_temp_dir(const char *printer, const char *host)
{
	char *tmp = NULL;

	if ((tmp = ns_get_value_string("spool-dir", printer_object)) == NULL)
		tmp = strdup("/var/spool/print");

	syslog(LOG_DEBUG, "cascade_temp_dir(%s, %s) = %s",
		(printer ? printer : "NULL"), (host ? host : "NULL"), tmp);

	if ((access((const char *)tmp, F_OK) < 0) && (mkdir(tmp, 0755) < 0))
		return (NULL);

	return (tmp);
}


/*
 * cascade_restart_printer() always returns 0, because it should always be
 * processing if a job is waiting to cascade.
 */
int
cascade_restart_printer(const char *printer)
{
	syslog(LOG_DEBUG, "cascade_restart_printer(%s)",
		(printer ? printer : "NULL"));

	start_daemon(1);

	return (0);
}


/*
 * cascade_submit_job() will save the control file data, write a binding
 * file and attempt to start a transfer agent.
 */
int
cascade_submit_job(const char *printer, const char *host, char *cf,
		    char **df_list)
{
	FILE *fp;

	syslog(LOG_DEBUG, "cascade_submit_job(%s, %s, 0x%x, 0x%x)",
		(printer ? printer : "NULL"), (host ? host : "NULL"), cf,
		df_list);

	/* write the control file */
	df_list[0][0] = 'c';
	if ((fp = fopen(df_list[0], "w")) != NULL) {
		fprintf(fp, "%s", cf);
		fclose(fp);
	} else
		return (-1);

	/* write a binding file */
	df_list[0][0] = 'x';
	if ((fp = fopen(df_list[0], "w")) != NULL) {
		fprintf(fp, "%s:%s", printer_addr->server,
			printer_addr->printer);
		fclose(fp);
	} else {
		df_list[0][0] = 'c';
		unlink(df_list[0]);
		return (-1);
	}

	start_daemon(1);

	return (0);
}


/*
 * cascade_show_queue() will relay job information from a remote spooler
 * and then add any local jobs waiting to cascade to the same destination.
 */
int
cascade_show_queue(const char *printer, FILE *ofp, const int type,
			const char **list)
{
	int fd;
	job_t **jobs = NULL;

	syslog(LOG_DEBUG, "cascade_show_queue(%s, 0x%x, %d, 0x%x)",
		(printer ? printer : "NULL"), ofp, type, list);

	/* get the remote list first */
	if ((fd = net_open(printer_addr->server, 5)) >= 0) {
		char buf[BUFSIZ];
		char **jlist = (char **)list;

		sprintf(buf, "%c%s", type, printer);
		while ((jlist != NULL) && (*jlist != NULL)) {
			strcat(buf, " ");
			strcat(buf, *jlist++);
		}

		net_printf(fd, "%s\n", buf);
		while (net_gets(buf, sizeof (buf), fd) != NULL)
			fputs(buf, ofp);

		close(fd);
	} else
		fprintf(ofp, gettext("can't connect to %s\n"),
			printer_addr->server);

	/* list any "local" jobs */
	if ((jobs = job_list_append(NULL, (char *)printer,
							NULL,  ".")) != NULL) {
		int rank = 1;

		while (*jobs != NULL) {
			if (type == 3) {
				fprintf(ofp,
				gettext("%-7.7s %8.8s %5d "
					"%-32.32s %8d bytes\n"),
					rank_string(rank++), (*jobs)->job_user,
					(*jobs)->job_id, "files", -1);
			} else {
				fprintf(ofp,
				gettext("%-8s:%-7s\t\t\t[ job %d%s ]\n"),
					(*jobs)->job_user, rank_string(rank++),
					(*jobs)->job_id, (*jobs)->job_host);

			}
			jobs++;
		}
	}

	return (0);
}


/*
 * cascade_cancel_job() will remove any matching jobs waiting to cascade to
 * the remote system, and then request the remote system to remove any
 * matching jobs.
 */
int
cascade_cancel_job(const char *printer, FILE *ofp, const char *user,
			const char *host, const char **list)
{
	int fd;
	job_t **jobs = NULL;

	syslog(LOG_DEBUG, "cascade_show_queue(%s, 0x%x, %s, %s, 0x%x)",
		(printer ? printer : "NULL"), ofp, (user ? user : "NULL"),
		(host ? host : "NULL"), list);

	/* cancel "local" jobs */
	if ((jobs = job_list_append(NULL, (char *)printer,
						(char *)host, ".")) != NULL)
		list_iterate((void **)jobs, (VFUNC_T)vjob_cancel, user,
			printer, host, list);

	/* ask the remote system next */
	if ((fd = net_open(printer_addr->server, 5)) >= 0) {
		char buf[BUFSIZ];
		char **jlist = (char **)list;

		sprintf(buf, "%c%s %s", 5, printer, user);
		while ((jlist != NULL) && (*jlist != NULL)) {
			strcat(buf, " ");
			strcat(buf, *jlist++);
		}

		net_printf(fd, "%s\n", buf);
		while (net_gets(buf, sizeof (buf), fd) != NULL)
			fputs(buf, ofp);

		close(fd);
	} else
		fprintf(ofp, gettext("can't connect to %s\n"),
			printer_addr->server);

	return (0);
}
