/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_BIO_H
#define	_BIO_H

#pragma ident	"@(#)bio.h	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int buf_walk_init(mdb_walk_state_t *);
extern int buf_walk_step(mdb_walk_state_t *);
extern void buf_walk_fini(mdb_walk_state_t *);

extern int bufpagefind(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _BIO_H */
