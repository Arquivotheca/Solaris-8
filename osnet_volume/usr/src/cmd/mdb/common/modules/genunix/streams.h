/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_H
#define	_STREAMS_H

#pragma ident	"@(#)streams.h	1.4	99/10/04 SMI"

#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

int queue_walk_init(mdb_walk_state_t *);
int queue_link_step(mdb_walk_state_t *);
int queue_next_step(mdb_walk_state_t *);
void queue_walk_fini(mdb_walk_state_t *);

int str_walk_init(mdb_walk_state_t *);
int strr_walk_step(mdb_walk_state_t *);
int strw_walk_step(mdb_walk_state_t *);
void str_walk_fini(mdb_walk_state_t *);

int stream(uintptr_t, uint_t, int, const mdb_arg_t *);
int queue(uintptr_t, uint_t, int, const mdb_arg_t *);
int q2syncq(uintptr_t, uint_t, int, const mdb_arg_t *);
int q2rdq(uintptr_t, uint_t, int, const mdb_arg_t *);
int q2wrq(uintptr_t, uint_t, int, const mdb_arg_t *);
int q2otherq(uintptr_t, uint_t, int, const mdb_arg_t *);
int syncq(uintptr_t, uint_t, int, const mdb_arg_t *);
int syncq2q(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_H */
