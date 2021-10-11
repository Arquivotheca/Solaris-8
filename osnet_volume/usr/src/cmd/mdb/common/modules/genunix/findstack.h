/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_FINDSTACK_H
#define	_MDB_FINDSTACK_H

#pragma ident	"@(#)findstack.h	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int findstack(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int findstack_debug(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int findstack_init(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_FINDSTACK_H */
