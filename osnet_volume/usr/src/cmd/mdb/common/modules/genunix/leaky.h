/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LEAKY_H
#define	_LEAKY_H

#pragma ident	"@(#)leaky.h	1.2	99/11/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int leaky_walk_init(mdb_walk_state_t *);
extern int leaky_walk_step(mdb_walk_state_t *);
extern void leaky_walk_fini(mdb_walk_state_t *);

extern int leaky_buf_walk_init(mdb_walk_state_t *);
extern int leaky_buf_walk_step(mdb_walk_state_t *);
extern void leaky_buf_walk_fini(mdb_walk_state_t *);

extern int findleaks(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _LEAKY_H */
