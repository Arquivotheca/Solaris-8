/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma	ident	"@(#)lm_shutdown.c	1.3	97/05/15 SMI"

#include <sys/types.h>
#include <nfs/nfs.h>
#include <nfs/nfssys.h>
#include <nfs/lm.h>

extern int _nfssys(enum nfssys_op, void *);

int
lm_shutdown()
{
	return (_nfssys(KILL_LOCKMGR, NULL));
}
