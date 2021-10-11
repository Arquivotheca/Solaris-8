/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)auditsys.c 1.2     96/05/06 SMI"

#include <sys/systm.h>
#include <sys/errno.h>

#include <c2/audit.h>

/*ARGSUSED1*/
int
auditsys(struct auditcalls *uap, rval_t *rvp)
{
	/*
	 * this ugly hack is because auditsys returns 0 for
	 * all cases except audit_acitve == 0 and
	 * uap->code  == BSM_AUDITCTRL || BSM_AUDITON || default)
	 */

	switch (uap->code) {
	case BSM_GETAUID:
	case BSM_SETAUID:
	case BSM_GETAUDIT:
	case BSM_SETAUDIT:
	case BSM_AUDIT:
	case BSM_AUDITSVC:
		return (0);
	case BSM_AUDITON:
	case BSM_AUDITCTL:
	default:
		return (EINVAL);
	}
}
