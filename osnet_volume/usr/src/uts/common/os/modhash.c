/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modhash.c	1.1	99/01/26 SMI"

/*
 * mod_hash: generic hash table implementation for the device framework.
 *
 * This is a reasonably fast, reasonably flexible hash table implementation
 * which features pluggable hash algorithms to support storing arbitrary keys
 * and values.  It is designed to handle small (< 100,000 items) amounts of
 * data.  The hash uses chaining to resolve collisions, and does not feature a
 * mechanism to grow the hash.  Care must be taken to pick nchains to be large
 * enough for the application at hand, or lots of time will be wasted searching
 * hash chains.
 *
 * The client of the hash is required to supply a number of items to support
 * the various hash functions:
 *
 * 	- Destructor functions for the key and value being hashed.
 *	  A destructor is responsible for freeing an object when the hash
 *	  table is no longer storing it.  Since keys and values can be of
 *	  arbitrary type, separate destructors for keys & values are used.
 *	  These may be mod_hash_null_keydtor and mod_hash_null_valdtor if no
 *	  destructor is needed for either a key or value.
 *
 *	- A hashing algorithm which returns a uint_t representing a hash index
 *	  The number returned need _not_ be between 0 and nchains.  The mod_hash
 *	  code will take care of doing that.  The second argument (after the
 *	  key) to the hashing function is a void * that represents
 *	  hash_alg_data-- this is provided so that the hashing algrorithm can
 *	  maintain some state across calls, or keep algorithm-specific
 *	  constants associated with the hash table.
 *
 *	  A pointer-hashing and a string-hashing algorithm are supplied in
 *	  this file.
 *
 *	- A key comparator (a la qsort).
 *	  This is used when searching the hash chain.  The key comparator
 *	  determines if two keys match.  It should follow the return value
 *	  semantics of strcmp.
 *
 *	  string and pointer comparators are supplied in this file.
 *
 * mod_hash_create_strhash() and mod_hash_create_ptrhash() provide good
 * examples of how to create a customized hash table.
 *
 * Basic hash operations:
 *
 *   mod_hash_create_strhash(name, nchains, dtor),
 *	create a hash using strings as keys.
 *	NOTE: This create a hash which automatically cleans up the string
 *	      values it is given for keys.
 *
 *   mod_hash_create_ptrhash(name, nchains, dtor, key_elem_size):
 *	create a hash using pointers as keys.
 *
 *   mod_hash_create_extended(name, nchains, kdtor, vdtor,
 *			      hash_alg, hash_alg_data,
 *			      keycmp, sleep)
 *	create a customized hash table.
 *
 *   mod_hash_destroy_hash(hash):
 *	destroy the given hash table, calling the key and value destructors
 *	on each key-value pair stored in the hash.
 *
 *   mod_hash_insert(hash, key, val):
 *	place a key, value pair into the given hash.
 *	duplicate keys are rejected.
 *
 *   mod_hash_remove(hash, key, *val):
 *	remove a key-value pair with key 'key' from 'hash', destroying the
 *	stored key, and returning the value in val.
 *
 *   mod_hash_replace(hash, key, val)
 * 	atomically remove an existing key-value pair from a hash, and replace
 * 	the key and value with the ones supplied.  The removed key and value
 * 	(if any) are destroyed.
 *
 *   mod_hash_destroy(hash, key):
 *	remove a key-value pair with key 'key' from 'hash', destroying both
 *	stored key and stored value.
 *
 *   mod_hash_find(hash, key, val):
 *	find a value in the hash table corresponding to the given key.
 *
 *   mod_hash_clear(hash):
 *	clears the given hash table of entries, calling the key and value
 *	destructors for every element in the hash.
 *
 */

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/varargs.h>
#include <sys/systm.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <sys/modhash.h>


/*
 * MH_SIZE()
 * 	Compute the size of a mod_hash_t, in bytes, given the number of
 * 	elements it contains.
 */
#define	MH_SIZE(n) \
	(sizeof (mod_hash_t) + (n - 1) * (sizeof (struct mod_hash_entry *)))

/*
 * MH_KEY_DESTROY()
 * 	Invoke the key destructor.
 */
#define	MH_KEY_DESTROY(hash, key) ((hash->mh_kdtor)(key))

/*
 * MH_VAL_DESTROY()
 * 	Invoke the value destructor.
 */
#define	MH_VAL_DESTROY(hash, val) ((hash->mh_vdtor)(val))

/*
 * MH_KEYCMP()
 * 	Call the key comparator for the given hash keys.
 */
#define	MH_KEYCMP(hash, key1, key2) ((hash->mh_keycmp)(key1, key2))

struct mod_hash_entry {
	mod_hash_key_t mhe_key;			/* stored hash key	*/
	mod_hash_val_t mhe_val;			/* stored hash value	*/
	struct mod_hash_entry *mhe_next;	/* next item in chain	*/
};

static void i_mod_hash_clear_nosync(mod_hash_t *);
static int i_mod_hash_find_nosync(mod_hash_t *, mod_hash_key_t,
    mod_hash_val_t *);
static int i_mod_hash_insert_nosync(mod_hash_t *, mod_hash_key_t,
    mod_hash_val_t);
static int i_mod_hash_remove_nosync(mod_hash_t *, mod_hash_key_t,
    mod_hash_val_t *);
static uint_t i_mod_hash(mod_hash_t *, mod_hash_key_t);

/*
 * Cache for struct mod_hash_entry
 */
kmem_cache_t *mh_e_cache = NULL;
mod_hash_t *mh_head = NULL;
kmutex_t mh_head_lock;

/*
 * mod_hash_null_keydtor()
 * mod_hash_null_valdtor()
 * 	no-op key and value destructors.
 */
/*ARGSUSED*/
void
mod_hash_null_keydtor(mod_hash_key_t key)
{
}

/*ARGSUSED*/
void
mod_hash_null_valdtor(mod_hash_val_t val)
{
}



/*
 * mod_hash_bystr()
 * mod_hash_strkey_cmp()
 * mod_hash_strkey_dtor()
 * mod_hash_strval_dtor()
 *	Hash and key comparison routines for hashes with string keys.
 *
 * mod_hash_create_strhash()
 * 	Create a hash using strings as keys
 *
 *	The string hashing algorithm is from the "Dragon Book" --
 *	"Compilers: Principles, Tools & Techniques", by Aho, Sethi, Ullman
 */

/*ARGSUSED*/
uint_t
mod_hash_bystr(void *hash_data, mod_hash_key_t key)
{
	uint_t hash = 0;
	uint_t g;
	char *p, *k = (char *)key;

	ASSERT(k);
	for (p = k; *p != '\0'; p++) {
		hash = (hash << 4) + *p;
		if ((g = (hash & 0xf0000000)) != 0) {
			hash ^= (g >> 24);
			hash ^= g;
		}
	}
	return (hash);
}

int
mod_hash_strkey_cmp(mod_hash_key_t key1, mod_hash_key_t key2)
{
	return (strcmp((char *)key1, (char *)key2));
}

void
mod_hash_strkey_dtor(mod_hash_key_t key)
{
	char *c = (char *)key;
	kmem_free(c, strlen(c) + 1);
}

void
mod_hash_strval_dtor(mod_hash_val_t val)
{
	char *c = (char *)val;
	kmem_free(c, strlen(c) + 1);
}

mod_hash_t *
mod_hash_create_strhash(char *name, size_t nchains,
    void (*val_dtor)(mod_hash_val_t))
{
	return mod_hash_create_extended(name, nchains, mod_hash_strkey_dtor,
	    val_dtor, mod_hash_bystr, NULL, mod_hash_strkey_cmp, KM_SLEEP);
}

void
mod_hash_destroy_strhash(mod_hash_t *strhash)
{
	ASSERT(strhash);
	mod_hash_destroy_hash(strhash);
}


/*
 * mod_hash_byptr()
 * mod_hash_ptrkey_cmp()
 *	Hash and key comparison routines for hashes with pointer keys.
 *
 * mod_hash_create_ptrhash()
 * mod_hash_destroy_ptrhash()
 * 	Create a hash that uses pointers as keys.  This hash algorithm
 * 	picks an appropriate set of middle bits in the address to hash on
 * 	based on the size of the hash table and a hint about the size of
 * 	the items pointed at.
 */
uint_t
mod_hash_byptr(void *hash_data, mod_hash_key_t key)
{
	uintptr_t k = (uintptr_t)key;
	k >>= (int)hash_data;

	return ((uint_t)k);
}

int
mod_hash_ptrkey_cmp(mod_hash_key_t key1, mod_hash_key_t key2)
{
	uintptr_t k1 = (uintptr_t)key1;
	uintptr_t k2 = (uintptr_t)key2;
	if (k1 > k2)
		return (-1);
	else if (k1 < k2)
		return (1);
	else
		return (0);
}

mod_hash_t *
mod_hash_create_ptrhash(char *name, size_t nchains,
    void (*val_dtor)(mod_hash_val_t), size_t key_elem_size)
{
	size_t rshift;

	/*
	 * We want to hash on the bits in the middle of the address word
	 * Bits far to the right in the word have little significance, and
	 * are likely to all look the same (for example, an array of
	 * 256-byte structures will have the bottom 8 bits of address
	 * words the same).  So we want to right-shift each address to
	 * ignore the bottom bits.
	 *
	 * The high bits, which are also unused, will get taken out when
	 * mod_hash takes hashkey % nchains.
	 */
	rshift = highbit(key_elem_size);

	return mod_hash_create_extended(name, nchains, mod_hash_null_keydtor,
	    val_dtor, mod_hash_byptr, (void *)rshift, mod_hash_ptrkey_cmp,
	    KM_SLEEP);
}

void
mod_hash_destroy_ptrhash(mod_hash_t *hash)
{
	ASSERT(hash);
	mod_hash_destroy_hash(hash);
}


/*
 * mod_hash_init()
 * 	sets up globals, etc for mod_hash_*
 */
void
mod_hash_init()
{
	ASSERT(mh_e_cache == NULL);
	mh_e_cache = kmem_cache_create("mod_hash_entries",
	    sizeof (struct mod_hash_entry), 0, NULL, NULL, NULL, NULL,
	    NULL, 0);
}

/*
 * mod_hash_create_extended()
 * 	The full-blown hash creation function.
 *
 * notes:
 * 	nchains		- how many hash slots to create.  More hash slots will
 *			  result in shorter hash chains, but will consume
 *			  slightly more memory up front.
 *	sleep		- should be KM_SLEEP or KM_NOSLEEP, to indicate whether
 *			  to sleep for memory, or fail in low-memory conditions.
 *
 * 	Fails only if KM_NOSLEEP was specified, and no memory was available.
 */
mod_hash_t *
mod_hash_create_extended(
    char *hname,			/* descriptive name for hash */
    size_t nchains,			/* number of hash slots */
    void (*kdtor)(mod_hash_key_t),	/* key destructor */
    void (*vdtor)(mod_hash_val_t),	/* value destructor */
    uint_t (*hash_alg)(void *, mod_hash_key_t), /* hash algorithm */
    void *hash_alg_data,		/* pass-thru arg for hash_alg */
    int (*keycmp)(mod_hash_key_t, mod_hash_key_t), /* key comparator */
    int sleep)				/* whether to sleep for mem */
{
	mod_hash_t *mod_hash;
	ASSERT(hname && keycmp && hash_alg && vdtor && kdtor);

	mod_hash = (mod_hash_t *)kmem_zalloc(MH_SIZE(nchains), sleep);
	if (mod_hash == NULL) {
		return (NULL);
	}

	mod_hash->mh_name = kmem_alloc(strlen(hname) + 1, sleep);
	if (mod_hash->mh_name == NULL) {
		kmem_free(mod_hash, MH_SIZE(nchains));
		return (NULL);
	}
	(void) strcpy(mod_hash->mh_name, hname);

	mod_hash->mh_sleep = sleep;
	mod_hash->mh_nchains = nchains;
	mod_hash->mh_kdtor = kdtor;
	mod_hash->mh_vdtor = vdtor;
	mod_hash->mh_hashalg = hash_alg;
	mod_hash->mh_hashalg_data = hash_alg_data;
	mod_hash->mh_keycmp = keycmp;

	/*
	 * Link the hash up on the list of hashes
	 */
	mutex_enter(&mh_head_lock);
	mod_hash->mh_next = mh_head;
	mh_head = mod_hash;
	mutex_exit(&mh_head_lock);

	return (mod_hash);
}

/*
 * mod_hash_destroy_hash()
 * 	destroy a hash table, destroying all of its stored keys and values
 * 	as well.
 */
void
mod_hash_destroy_hash(mod_hash_t *hash)
{
	mod_hash_t *mhp, *mhpp;

	mutex_enter(&mh_head_lock);
	/*
	 * Remove the hash from the hash list
	 */
	if (hash == mh_head) {		/* removing 1st list elem */
		mh_head = mh_head->mh_next;
	} else {
		/*
		 * mhpp can start out NULL since we know the 1st elem isn't the
		 * droid we're looking for.
		 */
		mhpp = NULL;
		for (mhp = mh_head; mhp != NULL; mhp = mhp->mh_next) {
			if (mhp == hash) {
				mhpp->mh_next = mhp->mh_next;
				break;
			}
			mhpp = mhp;
		}
	}
	mutex_exit(&mh_head_lock);

	/*
	 * Clean out keys and values.
	 */
	mod_hash_clear(hash);

	kmem_free(hash->mh_name, strlen(hash->mh_name) + 1);
	kmem_free(hash, MH_SIZE(hash->mh_nchains));
}

/*
 * i_mod_hash()
 * 	Call the hashing algorithm for this hash table, with the given key.
 */
static uint_t
i_mod_hash(mod_hash_t *hash, mod_hash_key_t key)
{
	uint_t h;
	/*
	 * Prevent div by 0 problems;
	 * Also a nice shortcut when using a hash as a list
	 */
	if (hash->mh_nchains == 1)
		return (0);

	h = (hash->mh_hashalg)(hash->mh_hashalg_data, key);
	return (h % (hash->mh_nchains - 1));
}

/*
 * i_mod_hash_insert_nosync()
 * mod_hash_insert()
 * 	insert 'val' into the hash table, using 'key' as it's key.  If 'key' is
 * 	already a key in the hash, an error will be returned, and the key-val
 * 	pair will not be inserted.
 */
int
i_mod_hash_insert_nosync(mod_hash_t *hash, mod_hash_key_t key,
    mod_hash_val_t val)
{
	uint_t hashidx;
	struct mod_hash_entry *entry;
	mod_hash_val_t v;

	ASSERT(hash);

	/*
	 * Disallow duplicate keys in the hash
	 */
	if (i_mod_hash_find_nosync(hash, key, &v) == 0) {
		hash->mh_stat.mhs_coll++;
		return (MH_ERR_DUPLICATE);
	}

	entry = kmem_cache_alloc(mh_e_cache, hash->mh_sleep);
	if (entry == NULL) {
		hash->mh_stat.mhs_nomem++;
		return (MH_ERR_NOMEM);
	}

	hashidx = i_mod_hash(hash, key);
	entry->mhe_key = key;
	entry->mhe_val = val;
	entry->mhe_next = hash->mh_entries[hashidx];

	hash->mh_entries[hashidx] = entry;
	hash->mh_stat.mhs_nelems++;

	return (0);
}

int
mod_hash_insert(mod_hash_t *hash, mod_hash_key_t key, mod_hash_val_t val)
{
	int res;

	rw_enter(&hash->mh_contents, RW_WRITER);
	res = i_mod_hash_insert_nosync(hash, key, val);
	rw_exit(&hash->mh_contents);

	return (res);
}

/*
 * i_mod_hash_remove_nosync()
 * mod_hash_remove()
 * 	Remove an element from the hash table.
 */
int
i_mod_hash_remove_nosync(mod_hash_t *hash, mod_hash_key_t key,
    mod_hash_val_t *val)
{
	int hashidx;
	struct mod_hash_entry *e, *ep;

	hashidx = i_mod_hash(hash, key);
	ep = NULL; /* e's parent */

	for (e = hash->mh_entries[hashidx]; e != NULL; e = e->mhe_next) {
		if (MH_KEYCMP(hash, e->mhe_key, key) == 0)
			break;
		ep = e;
	}

	if (e == NULL) {	/* not found */
		return (MH_ERR_NOTFOUND);
	}

	if (ep == NULL) 	/* special case 1st element in bucket */
		hash->mh_entries[hashidx] = e->mhe_next;
	else
		ep->mhe_next = e->mhe_next;

	/*
	 * Clean up resources used by the node's key.
	 */
	MH_KEY_DESTROY(hash, e->mhe_key);

	*val = e->mhe_val;
	kmem_cache_free(mh_e_cache, e);
	hash->mh_stat.mhs_nelems--;

	return (0);
}

int
mod_hash_remove(mod_hash_t *hash, mod_hash_key_t key, mod_hash_val_t *val)
{
	int res;

	rw_enter(&hash->mh_contents, RW_WRITER);
	res = i_mod_hash_remove_nosync(hash, key, val);
	rw_exit(&hash->mh_contents);

	return (res);
}

/*
 * mod_hash_replace()
 * 	atomically remove an existing key-value pair from a hash, and replace
 * 	the key and value with the ones supplied.  The removed key and value
 * 	(if any) are destroyed.
 */
int
mod_hash_replace(mod_hash_t *hash, mod_hash_key_t key, mod_hash_val_t val)
{
	int res;
	mod_hash_val_t v;

	rw_enter(&hash->mh_contents, RW_WRITER);

	if (i_mod_hash_remove_nosync(hash, key, &v) == 0) {
		/*
		 * mod_hash_remove() takes care of freeing up the key resources.
		 */
		MH_VAL_DESTROY(hash, v);
	}
	res = i_mod_hash_insert_nosync(hash, key, val);

	rw_exit(&hash->mh_contents);

	return (res);
}

/*
 * mod_hash_destroy()
 * 	Remove an element from the hash table matching 'key', and destroy it.
 */
int
mod_hash_destroy(mod_hash_t *hash, mod_hash_key_t key)
{
	mod_hash_val_t val;
	int rv;

	rw_enter(&hash->mh_contents, RW_WRITER);

	if ((rv = i_mod_hash_remove_nosync(hash, key, &val)) == 0) {
		/*
		 * mod_hash_remove() takes care of freeing up the key resources.
		 */
		MH_VAL_DESTROY(hash, val);
	}

	rw_exit(&hash->mh_contents);
	return (rv);
}

/*
 * i_mod_hash_find_nosync()
 * mod_hash_find()
 * 	Find a value in the hash table corresponding to the given key.
 */
static int
i_mod_hash_find_nosync(mod_hash_t *hash, mod_hash_key_t key,
    mod_hash_val_t *val)
{
	uint_t hashidx;
	struct mod_hash_entry *e;

	hashidx = i_mod_hash(hash, key);

	for (e = hash->mh_entries[hashidx]; e != NULL; e = e->mhe_next) {
		if (MH_KEYCMP(hash, e->mhe_key, key) == 0) {
			*val = e->mhe_val;
			hash->mh_stat.mhs_hit++;
			return (0);
		}
	}
	hash->mh_stat.mhs_miss++;
	return (MH_ERR_NOTFOUND);
}

int
mod_hash_find(mod_hash_t *hash, mod_hash_key_t key, mod_hash_val_t *val)
{
	int res;

	rw_enter(&hash->mh_contents, RW_READER);
	res = i_mod_hash_find_nosync(hash, key, val);
	rw_exit(&hash->mh_contents);

	return (res);
}

/*
 * i_mod_hash_clear_nosync()
 * mod_hash_clear()
 *	Clears the given hash table by calling the destructor of every hash
 *	element and freeing up all mod_hash_entry's.
 */
static void
i_mod_hash_clear_nosync(mod_hash_t *hash)
{
	int i;
	struct mod_hash_entry *e, *old_e;

	for (i = 0; i < hash->mh_nchains; i++) {
		e = hash->mh_entries[i];
		while (e != NULL) {
			MH_KEY_DESTROY(hash, e->mhe_key);
			MH_VAL_DESTROY(hash, e->mhe_val);
			old_e = e;
			e = e->mhe_next;
			kmem_cache_free(mh_e_cache, old_e);
		}
		hash->mh_entries[i] = NULL;
	}
	hash->mh_stat.mhs_nelems = 0;
}

void
mod_hash_clear(mod_hash_t *hash)
{
	ASSERT(hash);
	rw_enter(&hash->mh_contents, RW_WRITER);
	i_mod_hash_clear_nosync(hash);
	rw_exit(&hash->mh_contents);
}
