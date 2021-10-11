/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_TYPES_H
#define	_MDB_TYPES_H

#pragma ident	"@(#)mdb_types.h	1.1	99/08/11 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * If we're compiling on a system before <sys/int_types.h> existed,
 * make sure we have uintptr_t appropriately defined.
 */
#ifndef _SYS_INT_TYPES_H
#if defined(_LP64) || defined(_I32LPx)
typedef unsigned long uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif
#endif

typedef uchar_t mdb_bool_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_TYPES_H */
