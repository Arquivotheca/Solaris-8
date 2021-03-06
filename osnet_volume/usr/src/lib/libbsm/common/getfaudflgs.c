#ifndef lint
static char	sccsid[] = "@(#)getfaudflgs.c 1.7 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <bsm/audit.h>

#define	MAXSTRLEN 360

/*	getfaudflgs.c */

/*
 * getfauditflags() - combines system event flag mask with user event
 *	flag masks.
 *
 * input: usremasks->as_success - always audit on success
 *	usremasks->as_failure - always audit on failure
 *	usrdmasks->as_success - never audit on success
 *	usrdmasks->as_failure - never audit on failure
 *
 * output: lastmasks->as_success - audit on success
 *	lastmasks->as_failure - audit on failure
 *
 * returns:	0 - ok
 * 		-1 - error
 */

extern int getauditflagsbin();
extern int getacflg();

int
getfauditflags(usremasks, usrdmasks, lastmasks)
au_mask_t *usremasks;
au_mask_t *usrdmasks;
au_mask_t *lastmasks;
{
	int	len = MAXSTRLEN, retstat = 0;
	char	s_auditstring[MAXSTRLEN];
	audit_state_t masks;

	masks.as_success = 0;
	masks.as_failure = 0;
	/* get system audit mask and convert to bit mask */
	if ((getacflg(s_auditstring, len)) >= 0)  {
		if ((getauditflagsbin(s_auditstring, &masks)) != 0)
			retstat = -1;
	} else
		retstat = -1;

	/* combine system and user event masks */
	if (retstat == 0) {
		lastmasks->as_success = masks.as_success;
		lastmasks->as_failure = masks.as_failure;

		lastmasks->as_success |= usremasks->as_success;
		lastmasks->as_failure |= usremasks->as_failure;

		lastmasks->as_success &= ~(usrdmasks->as_success);
		lastmasks->as_failure &= ~(usrdmasks->as_failure);
	}
	return (retstat);
}
