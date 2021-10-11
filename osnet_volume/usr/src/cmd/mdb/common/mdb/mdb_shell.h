/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_SHELL_H
#define	_MDB_SHELL_H

#pragma ident	"@(#)mdb_shell.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

extern void mdb_shell_exec(char *);
extern void mdb_shell_pipe(char *);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_SHELL_H */
