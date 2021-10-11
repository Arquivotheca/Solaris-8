/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_HELP_H
#define	_MDB_HELP_H

#pragma ident	"@(#)mdb_help.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

extern int cmd_dmods(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_dcmds(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_walkers(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_formats(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_help(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_which(uintptr_t, uint_t, int, const mdb_arg_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_HELP_H */
