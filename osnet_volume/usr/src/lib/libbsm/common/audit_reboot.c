#ifndef lint
static char	sccsid[] = "@(#)audit_reboot.c 1.14 98/03/23 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <generic.h>

#ifdef C2_DEBUG
#define	dprintf(x) { printf x; }
#else
#define	dprintf(x)
#endif

static int audit_reboot_generic(int);

int
audit_reboot_setup()
{
	dprintf(("audit_reboot_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_init();
	aug_save_event(AUE_reboot_solaris);
	aug_save_me();
	return (0);
}

int
audit_reboot_fail()
{
	return (audit_reboot_generic(-1));
}

int
audit_reboot_success()
{
	int res = 0;

	(void) audit_reboot_generic(0);
	/*
	 * wait for audit daemon
	 * to put reboot message onto audit trail
	 */
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

		sleep(5);
	}

	return (res);
}

int
audit_reboot_generic(int sorf)
{
	int r;

	dprintf(("audit_reboot_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	aug_audit();

	return (0);
}
