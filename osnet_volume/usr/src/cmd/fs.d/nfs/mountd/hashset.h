/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_HASHSET_H
#define	_HASHSET_H

#pragma ident	"@(#)hashset.h	1.1	99/02/03 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct HashSet *HASHSET;
typedef struct HashSetIterator *HASHSET_ITERATOR;

extern HASHSET h_create(uint_t (*hash) (const void *),
    int    (*equal) (const void *, const void *),
    uint_t initialCapacity,
    float loadFactor);
extern const void *h_get(const HASHSET h, void *key);
extern const void *h_put(HASHSET h, const void *key);
extern const void *h_delete(HASHSET h, const void *key);

extern HASHSET_ITERATOR h_iterator(HASHSET h);
extern const void *h_next(HASHSET_ITERATOR i);

#ifdef	__cplusplus
}
#endif

#endif	/* _HASHSET_H */
