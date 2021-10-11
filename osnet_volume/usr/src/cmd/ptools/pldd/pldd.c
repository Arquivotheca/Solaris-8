/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pldd.c	1.6	99/03/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <libproc.h>
#include <proc_service.h>

static	int	show_map(void *, const prmap_t *, const char *);

static	char	*command;

int
main(int argc, char **argv)
{
	int rc = 0;
	int opt;
	int errflg = 0;
	int Fflag = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "F")) != EOF) {
		switch (opt) {
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr,
			"usage:\t%s [-F] { pid | core } ...\n", command);
		(void) fprintf(stderr,
			"  (report process dynamic libraries)\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		return (2);
	}

	while (argc-- > 0) {
		char *arg;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		if ((Pr = proc_arg_grab(arg = *argv++, PR_ARG_ANY,
		    PGRAB_RETAIN | Fflag, &gcode)) == NULL) {

			(void) fprintf(stderr, "%s: cannot examine %s: %s\n",
			    command, arg, Pgrab_error(gcode));
			rc++;
			continue;
		}

		(void) memcpy(&psinfo, Ppsinfo(Pr), sizeof (psinfo_t));
		proc_unctrl_psinfo(&psinfo);

		if (Pstate(Pr) == PS_DEAD) {
			(void) printf("core '%s' of %d:\t%.70s\n",
			    arg, (int)psinfo.pr_pid, psinfo.pr_psargs);
		} else {
			(void) printf("%d:\t%.70s\n",
			    (int)psinfo.pr_pid, psinfo.pr_psargs);
		}

		if (Pgetauxval(Pr, AT_BASE) != -1L && Prd_agent(Pr) == NULL) {
			(void) fprintf(stderr, "%s: warning: librtld_db failed "
			    "to initialize; shared library information will "
			    "not be available\n", command);
		}

		rc += Pobject_iter(Pr, show_map, Pr);
		Prelease(Pr, 0);
	}

	return (rc);
}

static int
show_map(void *cd, const prmap_t *pmap, const char *object_name)
{
	char pathname[PATH_MAX];
	struct ps_prochandle *Pr = cd;
	const auxv_t *auxv;
	int len;

	/* omit the executable file */
	if (strcmp(pmap->pr_mapname, "a.out") == 0)
		return (0);

	/* also omit the dynamic linker */
	if (ps_pauxv(Pr, &auxv) == PS_OK) {
		while (auxv->a_type != AT_NULL) {
			if (auxv->a_type == AT_BASE) {
				if (pmap->pr_vaddr == auxv->a_un.a_val)
					return (0);
				break;
			}
			auxv++;
		}
	}

	/* freedom from symlinks; canonical form */
	if ((len = resolvepath(object_name, pathname, sizeof (pathname))) > 0)
		pathname[len] = '\0';
	else
		(void) strncpy(pathname, object_name, sizeof (pathname));

	(void) printf("%s\n", pathname);
	return (0);
}
