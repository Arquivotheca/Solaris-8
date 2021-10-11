#ifndef lint
static char sccsid[] = "@(#)audit_mountd.c 1.19 99/10/14 SMI";
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
#include <unistd.h>
#include <synch.h>
#include <generic.h>

#ifdef C2_DEBUG2
#define	dprintf(x) { printf x; }
#else
#define	dprintf(x)
#endif

static mutex_t audit_mountd_lock = DEFAULTMUTEX;
static int cannotaudit = 0;

/*
 * This setup call is made only once at the start of mountd.
 * The call sets the auditing state off if appropriate, and is
 * made in single threaded code, hence no locking is required.
 */
void
audit_mountd_setup()
{
	dprintf(("audit_mountd_setup()\n"));

	aug_save_namask();

	if (cannot_audit(0))
		cannotaudit = 1;
}

void
audit_mountd_mount(clname, path, sorf)
char	*clname;	/* client name */
char	*path;		/* mount path */
int	sorf;		/* flag for success or failure */
{
	uint32_t buf[4], type;
	dprintf(("audit_mountd_mount()\n"));

	if (cannotaudit)
		return;

	mutex_lock(&audit_mountd_lock);
	aug_save_event(AUE_mountd_mount);
	aug_save_sorf(sorf);
	aug_save_text(clname);
	aug_save_path(path);
	(void)aug_get_machine(clname, buf, &type);
	aug_save_tid_ex(aug_get_port(), buf, type);
	aug_audit();
	mutex_unlock(&audit_mountd_lock);
}

void
audit_mountd_umount(clname, path)
char	*clname;	/* client name */
char	*path;		/* mount path */
{
	uint32_t buf[4], type;

	dprintf(("audit_mountd_mount()\n"));

	if (cannotaudit)
		return;

	mutex_lock(&audit_mountd_lock);
	aug_save_event(AUE_mountd_umount);
	aug_save_sorf(0);
	aug_save_text(clname);
	aug_save_path(path);
	(void)aug_get_machine(clname, buf, &type);
	aug_save_tid_ex(aug_get_port(), buf, type);
	aug_audit();
	mutex_unlock(&audit_mountd_lock);
}
