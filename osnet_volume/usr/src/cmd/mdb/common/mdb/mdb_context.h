/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_CONTEXT_H
#define	_MDB_CONTEXT_H

#pragma ident	"@(#)mdb_context.h	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <setjmp.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_MDB

/*
 * We hide the details of the context from the rest of MDB using the opaque
 * mdb_context tag.  This will facilitate later porting activities.
 */
typedef struct mdb_context mdb_context_t;

extern mdb_context_t *mdb_context_create(int (*)(void));
extern void mdb_context_destroy(mdb_context_t *);
extern void mdb_context_switch(mdb_context_t *);
extern jmp_buf *mdb_context_getpcb(mdb_context_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_CONTEXT_H */
