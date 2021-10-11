/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_SIGNAL_H
#define	_MDB_SIGNAL_H

#pragma ident	"@(#)mdb_signal.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <ucontext.h>
#include <signal.h>

typedef void mdb_signal_f(int, siginfo_t *, ucontext_t *, void *);

#ifdef _MDB

extern int mdb_signal_sethandler(int, mdb_signal_f *, void *);
extern mdb_signal_f *mdb_signal_gethandler(int, void **);

extern int mdb_signal_raise(int);
extern int mdb_signal_pgrp(int);

extern int mdb_signal_block(int);
extern int mdb_signal_unblock(int);

extern int mdb_signal_blockall(void);
extern int mdb_signal_unblockall(void);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_SIGNAL_H */
