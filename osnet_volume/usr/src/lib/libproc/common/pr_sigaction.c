/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_sigaction.c	1.2	99/03/23 SMI"

#include <sys/isa_defs.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <memory.h>
#include <errno.h>

#include "P32ton.h"
#include "libproc.h"

#ifdef _LP64
static void
sigaction_n_to_32(const struct sigaction *src, struct sigaction32 *dest)
{
	(void) memset(dest, 0, sizeof (*dest));
	dest->sa_flags = src->sa_flags;
	/* LINTED - pointer narrowing */
	dest->sa_handler = (caddr32_t)src->sa_handler;
	(void) memcpy(&dest->sa_mask, &src->sa_mask,
		sizeof (dest->sa_mask));
}
#endif	/* _LP64 */

/*
 * sigaction() system call -- executed by subject process.
 */
int
pr_sigaction(struct ps_prochandle *Pr,
	int sig, const struct sigaction *act, struct sigaction *oact)
{
	sysret_t rval;			/* return value from sigaction() */
	argdes_t argd[3];		/* arg descriptors for sigaction() */
	argdes_t *adp;
#ifdef _LP64
	struct sigaction32 act32;
	struct sigaction32 oact32;
#endif	/* _LP64 */

	if (Pr == NULL)		/* no subject process */
		return (sigaction(sig, act, oact));

	adp = &argd[0];		/* sig argument */
	adp->arg_value = sig;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* act argument */
	adp->arg_value = 0;
	if (act == NULL) {
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_object = NULL;
		adp->arg_size = 0;
	} else {
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INPUT;
#ifdef _LP64
		if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32) {
			sigaction_n_to_32(act, &act32);
			adp->arg_object = &act32;
			adp->arg_size = sizeof (act32);
		} else {
			adp->arg_object = (void *)act;
			adp->arg_size = sizeof (*act);
		}
#else	/* _LP64 */
		adp->arg_object = (void *)act;
		adp->arg_size = sizeof (*act);
#endif	/* _LP64 */
	}

	adp++;			/* oact argument */
	adp->arg_value = 0;
	if (oact == NULL) {
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_object = NULL;
		adp->arg_size = 0;
	} else {
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_OUTPUT;
#ifdef _LP64
		if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32) {
			adp->arg_object = &oact32;
			adp->arg_size = sizeof (oact32);
		} else {
			adp->arg_object = oact;
			adp->arg_size = sizeof (*oact);
		}
#else	/* _LP64 */
		adp->arg_object = oact;
		adp->arg_size = sizeof (*oact);
#endif	/* _LP64 */
	}

	rval = Psyscall(Pr, SYS_sigaction, 3, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

#ifdef _LP64
	if (oact != NULL && Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		sigaction_32_to_n(&oact32, oact);
#endif	/* _LP64 */

	return (rval.sys_rval1);
}
