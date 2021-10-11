/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hash.c	1.1	99/05/14 SMI"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include "stabspf_impl.h"

/*
 * Very simple Chained Hash List. Buckets are chained for simplicity
 * and unlimited in size.
 *
 * Public function: hash_get_name() which takes as a final argument:
 *   HASH_FIND: searches for a hash node according to string and returns
 *	STAB_FAIL if not found or STAB_SUCCESS if found and provides a pointer
 *	to the hash node to be querried.
 *
 *   HASH_ENTER: if search fails then add the hashnode to the hash list
 *	and provide a pointer to the hash node that can be filled in
 *	or queried.
 *
 * NOTE:
 *	Only HASH_FIND is referred to in the code, anything else
 *	will cause a HASH_ENTER action to be used.
 */

/* The hash table and supporting information. */
typedef struct hash_table {
	uint_t ht_size;		/* How many hash_nodes. */
	uint_t ht_collisions;	/* Keep track of collisions, statistical. */
	uint_t ht_entries;	/* Statistic for number of defined nodes. */
	hnode_t ht_nodes[1];	/* Place holder for array of hash nodes. */
} htable_t;

/*
 * The code is designed for more than one hash table, but only one is
 * currently needed and only costs one malloc().
 */
static htable_t *global_htable;

/*
 *
 * The following hash list sizes are a sequence of prime numbers that seem
 * to provide the best concentration of single entry hash lists (within
 * the confines of the present elf_hash() functionality.
 *
 * NOTE:
 *	This list was taken from libld, values < 1000 have been removed.
 */
#define	HASH_MAXNBKTS	10007
static const int hashsizes[] = {
	1009,	1103,	1201,	1301,	1409,	1511,	1601,	1709,	1801,
	1901,	2003,	2111,	2203,	2309,	2411,	2503,	2609,	2707,
	2801,	2903,	3001,	3109,	3203,	3301,	3407,	3511,	3607,
	3701,	3803,	3907,	4001,	5003,   6101,   7001,   8101,   9001,
	HASH_MAXNBKTS
};

/*
 * hash_create() - Creates a NEW hash table using <hint>
 * to figure out the best number of nodes in a hash list.
 */
static stabsret_t
hash_create(htable_t **htableptr, uint_t hint)
{
	htable_t *new_htable;	/* New hash table. */
	uint_t size = 0;
	size_t nodes_size;
	size_t htable_size;
	int i;

	if (hint == 0) {
		/* Nothing to do is bad. */
		return (STAB_FAIL);
	}

	/* short cut */
	if (hint >= HASH_MAXNBKTS) {
		size = HASH_MAXNBKTS;
	} else {
		/*
		 * Go through our list of primes until we get one that is
		 * bigger than hint.
		 */
		for (i = 0; i < (sizeof (hashsizes) / sizeof (int)); i++) {
			if (hint > hashsizes[i]) {
				continue;
			}
			size = hashsizes[i];
			break;
		}
	}

	nodes_size = size * sizeof (hnode_t);
	htable_size = nodes_size + sizeof (htable_t);


	/* New Hash Table */
	if ((new_htable = calloc(1, htable_size)) == NULL) {
		return (STAB_NOMEM);
	}

	/* Link it all together. */
	new_htable->ht_size = size;
	*htableptr = new_htable;

	return (STAB_SUCCESS);
}

/*
 * Free up all the memory in the hash table.
 *
 * NOTE:
 *	Though this memory usually persists for the life of the process,
 *	the ability to free this memory is available.  At a minimum it
 *	is useful for checking memory leaks.
 */
static void
hash_destroy(htable_t **htableptr)
{
	htable_t *htable = *htableptr;
	uint_t index;

	for (index = 0; index < htable->ht_size; index++) {
		if (htable->ht_nodes[index].hn_stroffset != 0) {
			hnode_t *cur_node;
			hnode_t *free_node;
			cur_node = htable->ht_nodes[index].hn_next;
			while (cur_node != NULL) {
				free_node = cur_node;
				cur_node = cur_node->hn_next;
				free(free_node);
			}
		}
	}

	free(htable);
	htable = NULL;
}


/*
 * hash_get_name_from_htable() - Find or search for a hash node from
 *	a particular hash list using strings for comparison.
 *
 * If action is HASH_FIND and the name is not found, set *hnode to NULL.
 */
static stabsret_t
hash_get_name_from_htable(htable_t **htableptr, const char *name,
    hnode_t **hnode, haction_t action)
{
	htable_t *htable = *htableptr;
	hnode_t *cur_node;
	uint_t index;			/* hash index */
	char *string;
	stabsret_t ret;

	/* Make sure *hnode is NULL if we do not assign it later. */
	*hnode = NULL;

	/* get index using elf_hash() */
	index = elf_hash(name) % htable->ht_size;
	cur_node = &htable->ht_nodes[index];


	for (;;) {
		/* Is cur_node empty? */
		if (cur_node->hn_stroffset == SO_NOSTRING) {
			if (action == HASH_FIND) {
				return (STAB_FAIL);
			}

			/* Register the new entry */
			++htable->ht_entries;

			/* Return with the empty node. */
			*hnode = cur_node;
			return (STAB_SUCCESS);
		}

		/*
		 * There is a string behind this hash node,
		 * get a a pointer to it.
		 */
		ret = string_offset2ptr(cur_node->hn_stroffset, &string);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		/*
		 * Check if the hashed string matches the name
		 * we are looking for.
		 */
		if (string[0] == name[0] &&
		    strcmp(string, name) == 0) {
			/* Found the node. */
			*hnode = cur_node;
			return (STAB_SUCCESS);
		}

		/*
		 * Collision occured in the hash table.
		 * Check the next hash slot in chain.
		 */
		if (cur_node->hn_next == NULL) {
			/* This is the end of the chain. */
			if (action != HASH_FIND) {
				/* Create a new node in the chain. */
				hnode_t *new_node;

				new_node = calloc(1, sizeof (hnode_t));
				if (new_node == NULL) {
					return (STAB_NOMEM);
				}
				/* Resgister the collision. */
				++htable->ht_collisions;

				/* Add new node to the list. */
				cur_node->hn_next = new_node;
			} else {
				/* Do not insert. */
				return (STAB_FAIL);
			}
		}

		/* Go to the next node in the chain. */
		cur_node = cur_node->hn_next;
	}
}

/*
 * hash_get_name() - Find or search for a name in the global hash table.
 */
stabsret_t
hash_get_name(const char *name, hnode_t **hnode, haction_t action)
{
	/* Perform the lookup in the global hash table. */
	return (hash_get_name_from_htable(&global_htable,
	    name, hnode, action));
}

void
hash_destroy_table(void)
{
	hash_destroy(&global_htable);
}

stabsret_t
hash_create_table(uint_t hint)
{
	stabsret_t ret;

	/* Create the global hash table. */
	if (global_htable == NULL) {
		ret = hash_create(&global_htable, hint);
	} else {
		ret = STAB_FAIL;
	}

	return (ret);
}

void
hash_report(void)
{
	(void) printf("==== hash: "
	    "entries = %u, collisions = %u, size = %.2f K ====\n",
	    global_htable->ht_entries,
	    global_htable->ht_collisions,
	    (float)((global_htable->ht_size * sizeof (hnode_t)) / 1024.0));
}
