/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)agent.c	1.1	99/08/15 SMI"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "libcpc.h"
#include "libcpc_impl.h"

int
cpc_pctx_bind_event(pctx_t *pctx, id_t lwpid, cpc_event_t *event, int flags)
{
	if (event == NULL)
		return (cpc_pctx_rele(pctx, lwpid));
	else if (flags != 0) {
		errno = EINVAL;
		return (-1);
	} else
		return (__pctx_cpc(pctx,
		    CPC_BIND_EVENT, lwpid, event, 0, sizeof (*event)));
}

int
cpc_pctx_take_sample(pctx_t *pctx, id_t lwpid, cpc_event_t *event)
{
	return (__pctx_cpc(pctx,
	    CPC_TAKE_SAMPLE, lwpid, event, 0, sizeof (*event)));
}

/*
 * Given a process context and an lwpid, mark the CPU performance
 * counter context as invalid.
 */
int
cpc_pctx_invalidate(pctx_t *pctx, id_t lwpid)
{
	return (__pctx_cpc(pctx, CPC_INVALIDATE, lwpid, NULL, 0, 0));
}

/*
 * Given a process context and an lwpid, remove all our
 * hardware context from it.
 */
int
cpc_pctx_rele(pctx_t *pctx, id_t lwpid)
{
	if (__pctx_cpc(pctx, CPC_RELE, lwpid, NULL, 0, 0) == -1 &&
	    errno != EINVAL)
		return (-1);
	return (0);
}
