#ifndef lint
static char	sccsid[] = "@(#)audit_shutdown.c 1.2 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <netinet/in.h>
#include <generic.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

extern int	errno;

static int	pflag;			/* preselection flag */
static au_event_t	event;	/* audit event number */
static int	gargc;
static char	**gargv;
static char	**garge;
static int	save_afunc();

int audit_shutdown_generic(int);

int
audit_shutdown_setup(argc, argv)
	int    argc;
	char **argv;
{
	dprintf(("audit_shutdown_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}
	gargc = argc;
	gargv = argv;

	aug_init();
	aug_save_event(AUE_shutdown_solaris);
	aug_save_me();
/*	aug_save_afunc(save_afunc); */
	return (0);
}

/*
static int
save_afunc(int ad)
{
	if (gargv && gargv[1])
		au_write(ad, au_to_text(gargv[1]));
}
*/

int
audit_shutdown_fail()
{
	return (audit_shutdown_generic(-1));
}

int
audit_shutdown_success()
{
	return (audit_shutdown_generic(0));
}

int
audit_shutdown_generic(sorf)
	int sorf;
{
	int r;

	dprintf(("audit_shutdown_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	r = aug_audit();

	return (r);
}
