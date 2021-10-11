/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)parse_bsd.c	1.13	99/03/30 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/job.h>
#include <print/list.h>

#include "parse.h"


#define	STRNCASECMP(a, b)	strncasecmp(a, b, strlen(b))

static job_t *
parse_bsd_entry(char *data)
{
	job_t *tmp;
	char *p;

	tmp = (job_t *)malloc(sizeof (*tmp));
	(void) memset(tmp, 0, sizeof (*tmp));

	/*
	 * 1st line...
	 *	 user: rank			[job (ID)(host)]\n
	 */
	if ((p = strtok(data, ": ")) == NULL)	 /* user: ... */
		return (NULL);
	tmp->job_user = strdup(p);
	p = strtok(NULL, "\t ");	/* ...rank... */
	p = strtok(NULL, " ");		/* ...[job ... */
	if ((p = strtok(NULL, "]\n")) == NULL)	/* ... (id)(hostname)] */
		return (NULL);
	tmp->job_id = atoi(p);
	while (isdigit(*(++p)));
	tmp->job_host = strdup(p);

	/*
	 * rest o lines
	 *	file				size bytes\n
	 */
	do {
		jobfile_t *file;

		p = strtok(NULL, "\t ");
		p = strtok(NULL, "\t "); /* file Name */
		if (p != NULL) {
			file = (jobfile_t *)malloc(sizeof (*file));
			file->jf_name = strdup(p);
			if ((p = strtok(NULL, "\t ")) != NULL)
				if ((file->jf_size = atoi(p)) == 0)
					while ((p = strtok(NULL, "\t "))
						!= NULL)
						if (isdigit(p[0])) {
							file->jf_size = atoi(p);
							break;
						}
			tmp->job_df_list = (jobfile_t **)list_append((void **)
							tmp->job_df_list,
							(void *)file);
		}
	} while (p != NULL);
	return (tmp);
}


/*ARGSUSED*/
print_queue_t *
parse_bsd_queue(ns_bsd_addr_t *binding, char *data, int len)
{
	char *p;
	print_queue_t *tmp;

	tmp = (print_queue_t *)malloc(sizeof (*tmp));
	(void) memset(tmp, 0, sizeof (*tmp));
	tmp->state = RAW;
	tmp->status = data;
	tmp->binding = binding;

	if (data == NULL)
		return (tmp);

	if ((p = strstr(data, "\n\n")) == NULL) {
		if (strstr(data, "no entries") != NULL)
			tmp->state = IDLE;
		return (tmp);
	}

	*(p++) = NULL;
	tmp->status = strdup(data);
	data = ++p;

	tmp->state = FAULTED;			/* Error */
	if (STRNCASECMP(tmp->status, "no entries") == 0)
		tmp->state = IDLE;
	else if (STRNCASECMP((tmp->status + strlen(tmp->status)
				- strlen("ready and printing")),
				"ready and printing") == 0)
		tmp->state = PRINTING;

	do {					/* parse jobs */
		job_t *job;
		char *q = data;

		if ((p = strstr(data, "\n\n")) != NULL) {
			*(++p) = NULL;
			data = ++p;
		}
		if ((job = parse_bsd_entry(q)) == NULL)
			break;

		job->job_server = strdup(binding->server);
		job->job_printer = strdup(binding->printer);
		tmp->jobs = (job_t **)list_append((void **)tmp->jobs,
							(void *)job);
	} while (p != NULL);

	return (tmp);
}
