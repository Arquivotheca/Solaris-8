/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_SET_H
#define	_MDB_SET_H

#pragma ident	"@(#)mdb_set.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_MDB

extern int mdb_set_options(const char *, int);
extern int cmd_set(uintptr_t, uint_t, int, const mdb_arg_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_SET_H */
