/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CPUCMDS_H
#define	_CPUCMDS_H

#pragma ident	"@(#)cpucmds.h	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int capabilities(FILE *, int cpuver);
extern void zerotime(void);
extern float mstimestamp(cpc_event_t *event);

/*
 * Events can be manipulated in collections called eventsets
 * Perhaps these routines should be part of the full API?
 */
typedef struct __cpc_eventset cpc_eventset_t;

extern cpc_eventset_t *cpc_eset_new(int cpuver);
extern cpc_eventset_t *cpc_eset_newevent(cpc_eventset_t *eset,
    const char *spec, int *errcnt);
extern cpc_eventset_t *cpc_eset_clone(cpc_eventset_t *eset);
extern void cpc_eset_free(cpc_eventset_t *eset);

extern cpc_event_t *cpc_eset_getevent(cpc_eventset_t *eset);
extern const char *cpc_eset_getname(cpc_eventset_t *eset);
extern int cpc_eset_numevents(cpc_eventset_t *eset);
extern cpc_event_t *cpc_eset_nextevent(cpc_eventset_t *eset);
extern void cpc_eset_reset(cpc_eventset_t *to);
extern void cpc_eset_accum(cpc_eventset_t *accum, cpc_eventset_t *eset);

#ifdef __cplusplus
}
#endif

#endif	/* _CPUCMDS_H */
