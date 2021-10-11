#ifndef lint
static char	sccsid[] = "@(#)audit_uadmin.c 1.4 98/03/23 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
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

static int	pflag;		/* preselection flag */
static au_event_t	event;	/* audit event number */
static int	gargc;
static char	**gargv;
static char	**garge;
static int	save_afunc();

static int audit_uadmin_generic(int);

int
audit_uadmin_setup(argc, argv)
	int    argc;
	char **argv;
{
	dprintf(("audit_uadmin_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}
	gargc = argc;
	gargv = argv;

	aug_init();
	aug_save_event(AUE_uadmin_solaris);
	aug_save_me();
	aug_save_afunc(save_afunc);
	return (0);
}

static int
save_afunc(int ad)
{
	if (gargv && gargv[1])
		au_write(ad, au_to_text(gargv[1]));
	if (gargv && gargv[2])
		au_write(ad, au_to_text(gargv[2]));
	return (0);
}

int
audit_uadmin_fail()
{
	return (audit_uadmin_generic(-1));
}

int
audit_uadmin_success()
{
	int res = 0;

	(void) audit_uadmin_generic(0);

	/*
	 * wait for audit daemon to put halt message onto audit trail
	 */
	if (!cannot_audit(0)) {
		int cond = AUC_NOAUDIT;
		int canaudit;

		(void) sleep(1);

		/* find out if audit daemon is running */
		(void) auditon(A_GETCOND, (caddr_t)&cond,
			sizeof (cond));
		canaudit = (cond == AUC_AUDITING);

		/* turn off audit daemon and try to flush audit queue */
		if (canaudit && system("/usr/sbin/audit -t"))
			res = -1;

		/* give a chance for syslogd to do the job */
		(void) sleep(5);
	}

	return (res);
}

int
audit_uadmin_generic(sorf)
	int sorf;
{
	int r;

	dprintf(("audit_uadmin_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	r = aug_audit();

	return (r);
}
