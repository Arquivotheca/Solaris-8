/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MODHASH_H
#define	_MODHASH_H

#pragma ident	"@(#)modhash.h	1.1	99/01/26 SMI"

/*
 * Generic hash implementation for the device framework.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A generic hash table for the device framework.
 */
#ifdef _KERNEL

#include <sys/types.h>
#include <sys/t_lock.h>

/*
 * Opaque data types for storing keys and values
 */
typedef void *mod_hash_val_t;
typedef void *mod_hash_key_t;

struct mod_hash_entry;

struct mod_hash_stat {
	ulong_t mhs_hit;	/* tried a 'find' and it succeeded */
	ulong_t mhs_miss;	/* tried a 'find' but it failed */
	ulong_t mhs_coll;	/* occur when insert fails because of dup's */
	ulong_t mhs_nelems;	/* total number of stored key/value pairs */
	ulong_t mhs_nomem;	/* number of times kmem_alloc failed */
};

typedef struct mod_hash {
	krwlock_t	mh_contents;	/* lock protecting contents */
	char		*mh_name;	/* hash name */
	int		mh_sleep;	/* kmem_alloc flag */
	size_t		mh_nchains;	/* # of elements in mh_entries */

	/* key and val destructor */
	void    (*mh_kdtor)(mod_hash_key_t);
	void    (*mh_vdtor)(mod_hash_val_t);

	/* key comparator */
	int	(*mh_keycmp)(mod_hash_key_t, mod_hash_key_t);

	/* hash algorithm, and algorithm-private data */
	uint_t  (*mh_hashalg)(void *, mod_hash_key_t);
	void    *mh_hashalg_data;

	struct mod_hash	*mh_next;	/* next hash in list */

	struct mod_hash_stat mh_stat;

	struct mod_hash_entry *mh_entries[1];
} mod_hash_t;

/*
 * String hash table
 */
mod_hash_t *mod_hash_create_strhash(char *, size_t, void (*)(mod_hash_val_t));
void mod_hash_destroy_strhash(mod_hash_t *);
int mod_hash_strkey_cmp(mod_hash_key_t, mod_hash_key_t);
void mod_hash_strkey_dtor(mod_hash_key_t);
void mod_hash_strval_dtor(mod_hash_val_t);
uint_t mod_hash_bystr(void *, mod_hash_key_t);

/*
 * Pointer hash table
 */
mod_hash_t *mod_hash_create_ptrhash(char *, size_t, void (*)(mod_hash_val_t),
    size_t);
void mod_hash_destroy_ptrhash(mod_hash_t *);
int mod_hash_ptrkey_cmp(mod_hash_key_t, mod_hash_key_t);
uint_t mod_hash_byptr(void *, mod_hash_key_t);

/*
 * Hash management functions
 */
void mod_hash_init();

mod_hash_t *mod_hash_create_extended(char *, size_t, void (*)(mod_hash_key_t),
    void (*)(mod_hash_val_t), uint_t (*)(void *, mod_hash_key_t), void *,
    int (*)(mod_hash_key_t, mod_hash_key_t), int);

void mod_hash_destroy_hash(mod_hash_t *);
void mod_hash_clear(mod_hash_t *);

/*
 * Null key and value destructors
 */
void mod_hash_null_keydtor(mod_hash_key_t key);
void mod_hash_null_valdtor(mod_hash_val_t val);

/*
 * Basic hash operations
 */

/*
 * Error codes for insert, remove, find, destroy.
 */
#define	MH_ERR_NOMEM -1
#define	MH_ERR_DUPLICATE -2
#define	MH_ERR_NOTFOUND -3

/*
 * Basic hash operations
 */
int mod_hash_insert(mod_hash_t *, mod_hash_key_t, mod_hash_val_t);
int mod_hash_replace(mod_hash_t *, mod_hash_key_t, mod_hash_val_t);
int mod_hash_remove(mod_hash_t *, mod_hash_key_t, mod_hash_val_t *);
int mod_hash_destroy(mod_hash_t *, mod_hash_key_t);
int mod_hash_find(mod_hash_t *, mod_hash_key_t, mod_hash_val_t *);


#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _MODHASH_H */
