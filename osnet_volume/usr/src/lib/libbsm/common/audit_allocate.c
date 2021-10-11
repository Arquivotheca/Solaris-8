#ifndef lint
static char sccsid[] = "@(#)audit_allocate.c 1.8 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <unistd.h>
#include <string.h>
#include <bsm/audit_uevents.h>
#include <generic.h>

static int s_audit;	/* successful audit event */
static int f_audit;	/* failure audit event */

static int ad;		/* audit descriptor */

void
audit_allocate_argv(flg, argc, argv)
	int   flg;
	int   argc;
	char *argv[];
{
	int i;

	if (cannot_audit(0)) {
		return;
	}

	switch (flg) {
	case 0:
		s_audit = AUE_allocate_succ;
		f_audit = AUE_allocate_fail;
		break;
	case 1:
		s_audit = AUE_deallocate_succ;
		f_audit = AUE_deallocate_fail;
		break;
	case 2:
		s_audit = AUE_listdevice_succ;
		f_audit = AUE_listdevice_fail;
		break;
	}

	ad = au_open();

	for (i = 0; i < argc; i++)
		au_write(ad, au_to_text(argv[i]));
}

void
audit_allocate_device(path)
	char *path;
{
	if (cannot_audit(0)) {
		return;
	}
	au_write(ad, au_to_path(path));
}

int
audit_allocate_record(status)
	char	status;		/* success failure of operation */
{
	auditinfo_addr_t mask;		/* audit ID */
	au_event_t	event;		/* audit event number */
	int		policy;		/* audit policy */
	int		ng;		/* number of groups in process */
	gid_t		grplst[NGROUPS_UMAX];

#ifdef DEBUG
	printf(("audit_allocate_record(%d)\n", status));
#endif

	if (cannot_audit(0)) {
		return (0);
	}

	if (getaudit_addr(&mask, sizeof(mask)) < 0) {
		if (!status)
			return (1);
		return (0);
	}

	if (auditon(A_GETPOLICY, (caddr_t)&policy, 0) < 0) {
		if (!status)
			return (1);
		return (0);
	}


		/* determine if we're preselected */
	if (status)
		event = f_audit;
	else
		event = s_audit;

	if (au_preselect(event, &mask.ai_mask, AU_PRS_BOTH, AU_PRS_REREAD)
		== NULL)
		return (0);

	au_write(ad, au_to_me());	/* add subject token */

	if (policy & AUDIT_GROUP) {	/* add optional group token */
		memset(grplst, 0, sizeof (grplst));
		if ((ng = getgroups(NGROUPS_UMAX, grplst)) < 0) {
			au_close(ad, 0, 0);
			if (!status)
				return (1);
			return (0);
		}
		au_write(ad, au_to_newgroups(ng, grplst));
	}

	if (status)
		au_write(ad, au_to_exit(status, -1));
	else
		au_write(ad, au_to_exit(0, 0));

		/* write audit record */
	if (au_close(ad, 1, event) < 0) {
		au_close(ad, 0, 0);
		if (!status)
			return (1);
	}

	return (0);
}

void
audit_allocate_list(list)
	char *list;
{
	char buf[1024];
	char *file;

	if (cannot_audit(0)) {
		return;
	}

	strcpy(buf, list);

	for (file = strtok(buf, " "); file; file = strtok(NULL, " "))
		au_write(ad, au_to_path(file));
}
