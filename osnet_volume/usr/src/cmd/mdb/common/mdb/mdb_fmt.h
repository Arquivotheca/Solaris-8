/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_FMT_H
#define	_MDB_FMT_H

#pragma ident	"@(#)mdb_fmt.h	1.1	99/08/11 SMI"

#include <mdb/mdb_target.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

mdb_tgt_addr_t mdb_fmt_print(mdb_tgt_t *, mdb_tgt_as_t,
    mdb_tgt_addr_t, size_t, char);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_FMT_H */
