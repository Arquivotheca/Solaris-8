/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)hash.c	1.1	99/08/13 SMI"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<libelf.h>
#include	"_crle.h"

Hash_tbl *
make_hash(int size, Hash_type type, ulong_t ident)
{
	Hash_tbl *	tbl;

	if ((tbl = malloc(sizeof (Hash_tbl))) == 0)
		return (0);

	if ((tbl->t_entry = calloc((unsigned)(sizeof (Hash_ent *)), size)) == 0)
		return (0);

	tbl->t_ident = ident;
	tbl->t_type = type;
	tbl->t_size = size;

	return (tbl);
}


Hash_ent *
get_hash(Hash_tbl * tbl, Addr key, int mode)
{
	int		bucket;
	Hash_ent *	ent;
	Word		hashval;

	if (tbl->t_type == HASH_STR)
		hashval = elf_hash((const char *)key);
	else
		hashval = key;

	bucket = hashval % tbl->t_size;

	if (mode & HASH_FND_ENT) {
		for (ent = tbl->t_entry[bucket]; ent != NULL;
		    ent = ent->e_next) {
			if (tbl->t_type == HASH_STR) {
				if (strcmp((const char *)ent->e_key,
				    (const char *)key) == 0)
					return (ent);
			} else {
				if (ent->e_key == key)
					return (ent);
			}
		}
	}
	if (!(mode & HASH_ADD_ENT))
		return (0);

	/*
	 * Key not found in this hash table ... insert new entry into bucket.
	 */
	if ((ent = calloc(sizeof (Hash_ent), 1)) == 0)
		return (0);

	ent->e_key = key;
	ent->e_hash = hashval;

	/*
	 * Hook into bucket chain
	 */
	ent->e_next = tbl->t_entry[bucket];
	tbl->t_entry[bucket] = ent;

	return (ent);
}
