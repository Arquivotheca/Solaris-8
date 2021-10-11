/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_door.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <door.h>
#include "libproc.h"

/*
 * door() system calls -- executed by subject process
 */

int
pr_door_info(struct ps_prochandle *Pr, int did, door_info_t *di)
{
	extern int _door_info(int, door_info_t *);	/* header file? */
	sysret_t rval;			/* return value from _door_info() */
	argdes_t argd[6];		/* arg descriptors for _door_info() */
	argdes_t *adp = &argd[0];	/* first argument */

	if (Pr == NULL)		/* no subject process */
		return (_door_info(did, di));

	adp->arg_value = did;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to door_info_t argument */

	adp->arg_value = 0;
	adp->arg_object = di;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
	adp->arg_size = sizeof (door_info_t);
	adp++;			/* move to unused argument */

	adp->arg_value = 0;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to unused argument */

	adp->arg_value = 0;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to unused argument */

	adp->arg_value = 0;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to subcode argument */

	adp->arg_value = DOOR_INFO;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, SYS_door, 6, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno == 0)
		return (0);
	errno = rval.sys_errno;
	return (-1);
}
