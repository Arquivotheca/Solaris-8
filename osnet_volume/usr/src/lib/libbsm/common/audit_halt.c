#ifndef lint
static char	sccsid[] = "@(#)audit_halt.c 1.14 98/03/23 SMI";
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
#include <libgen.h>
#include <generic.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

extern int	errno;

static int	pflag;			/* preselection flag */
static au_event_t	event;	/* audit event number */

static int audit_halt_generic(int);

int
audit_halt_setup(argc, argv)
int	argc;
char	**argv;
{
	char *cmdname;

	dprintf(("audit_halt_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	cmdname = basename(*argv);

	aug_init();

	if (strcmp(cmdname, "halt") == 0)
		aug_save_event(AUE_halt_solaris);
	else if (strcmp(cmdname, "poweroff") == 0)
		aug_save_event(AUE_poweroff_solaris);
	else
		exit(1);
	aug_save_me();
	return (0);
}

int
audit_halt_fail()
{
	return (audit_halt_generic(-1));
}

int
audit_halt_success()
{
	int res = 0;

	(void) audit_halt_generic(0);

	/* wait for audit daemon to put halt message onto audit trail */
	if (!cannot_audit(0)) {
		int cond = AUC_NOAUDIT;
		int canaudit;

		sleep(1);

		/* find out if audit daemon is running */
		(void) auditon(A_GETCOND, (caddr_t)&cond, sizeof (cond));
		canaudit = (cond == AUC_AUDITING);

		/* turn off audit daemon and try to flush audit queue */
		if (canaudit && system("/usr/sbin/audit -t"))
			res = -1;
		else
		/* give a chance for syslogd to do the job */
			sleep(5);
	}

	return (res);
}

int
audit_halt_generic(sorf)
	int sorf;
{
	int r;

	dprintf(("audit_halt_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	r = aug_audit();

	return (r);
}
