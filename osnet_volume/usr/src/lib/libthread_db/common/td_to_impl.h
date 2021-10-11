/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TD_TO_IMPL_H
#define	_TD_TO_IMPL_H

#pragma ident	"@(#)td_to_impl.h	1.4	96/11/22 SMI"

/*
* MODULE_td_to_impl.h________________________________________________
*  Description:
____________________________________________________________________ */

#include "libthread.h"
#include <sys/types.h>
#include <sys/siginfo.h>
#include <sys/procfs_isa.h>
#include <sys/ucontext.h>
#include <signal.h>
#include <string.h>

#include "td_to.h"

#ifdef TD_INTERNAL_TESTS
static char	*td_thr_state_names[] = {
	"TD_THR_UNKNOWN",
	"TD_THR_ANY_STATE",
	"TD_THR_STOPPED",
	"TD_THR_RUN",
	"TD_THR_ACTIVE",
	"TD_THR_ZOMBIE",
	"TD_THR_SLEEP",
	"TD_THR_STOPPED_ASLEEP",
};
#endif	/* TD_INTERNAL_TESTS */

static struct tsd_common null_tsd_common = {0, 0, 0, 0};
static	tsd_t null_tsd_t = {0, 0};

#define	NULL_TSD_COMMON null_tsd_common
#define	NULL_TSD_T null_tsd_t

#endif /* _TD_TO_IMPL_H */
