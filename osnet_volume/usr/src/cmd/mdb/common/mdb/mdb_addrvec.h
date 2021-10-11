/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_ADDRVEC_H
#define	_MDB_ADDRVEC_H

#pragma ident	"@(#)mdb_addrvec.h	1.1	99/08/11 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct mdb_addrvec {
	uintptr_t *ad_data;		/* Array of addresses */
	size_t ad_nelems;		/* Number of valid elements */
	size_t ad_size;			/* Array size */
	size_t ad_ndx;			/* Array index */
} mdb_addrvec_t;

#ifdef _MDB

extern void mdb_addrvec_create(mdb_addrvec_t *);
extern void mdb_addrvec_destroy(mdb_addrvec_t *);

extern uintptr_t mdb_addrvec_shift(mdb_addrvec_t *);
extern void mdb_addrvec_unshift(mdb_addrvec_t *, uintptr_t);
extern size_t mdb_addrvec_length(mdb_addrvec_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_ADDRVEC_H */
