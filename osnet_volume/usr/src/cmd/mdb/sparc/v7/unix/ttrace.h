/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_TTRACE_H
#define	_TTRACE_H

#pragma ident	"@(#)ttrace.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int ttrace_walk_init(mdb_walk_state_t *);
extern int ttrace_walk_step(mdb_walk_state_t *);
extern void ttrace_walk_fini(mdb_walk_state_t *);

extern int ttrace(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _TTRACE_H */
