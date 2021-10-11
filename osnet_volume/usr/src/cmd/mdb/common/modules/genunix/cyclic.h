/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_CYCLIC_H
#define	_MDB_CYCLIC_H

#pragma ident	"@(#)cyclic.h	1.2	99/11/19 SMI"

#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int cyccpu_walk_init(mdb_walk_state_t *);
extern int cyccpu_walk_step(mdb_walk_state_t *);

extern int cyctrace_walk_init(mdb_walk_state_t *);
extern int cyctrace_walk_step(mdb_walk_state_t *);
extern void cyctrace_walk_fini(mdb_walk_state_t *);

extern int cycinfo(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cyclic(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cyctrace(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cyccover(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_CYCLIC_H */
