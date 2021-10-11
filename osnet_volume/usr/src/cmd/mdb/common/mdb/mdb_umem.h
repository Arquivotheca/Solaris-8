/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_UMEM_H
#define	_MDB_UMEM_H

#pragma ident	"@(#)mdb_umem.h	1.1	99/08/11 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

#define	UMEM_FREE_PATTERN		0xdefacedd
#define	UMEM_UNINITIALIZED_PATTERN	0xbeefbabe

#define	UMF_DEBUG			0x1

typedef struct mdb_mblk {
	void *blk_addr;			/* address of allocated block */
	size_t blk_size;		/* size of block in bytes */
	struct mdb_mblk *blk_next;	/* link to next block */
} mdb_mblk_t;

void mdb_recycle(mdb_mblk_t **);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_UMEM_H */
