/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)type_table.c	1.1	99/05/14 SMI"

#include <stdlib.h>
#include <string.h>
#include <stab.h>
#include "stabspf_impl.h"

/*
 * The type table contains an array of type_t's (aka types).
 * If it is not big enough it is grown AS A WHOLE.
 *
 * NOTE:
 *	The base of the type table can grow (realloc()) at any time
 *	so a type descriptor must always be used to access a type.
 */
typedef struct type_table {
	struct type_table_hdr {
		typedesc_t	tth_used;	/* Number of used types. */
		typedesc_t	tth_have;	/* Number of types alloc'd. */
		/* Byte size of type array portion. */
		size_t		tth_size;
	} tt_hdr;
	type_t	tt_types[1];		/* Place holder for first type */
} ttable_t;

/*
 * In order to minimize reallocations, tune these values to what is typically
 * needed.
 *
 * The space required for the uniq types in libc, libdl, libnsl and
 * libsocket, which are needed for an app like ksh requires a type table
 * just under 20k.
 */
#define	TYPE_TABLE_NTYPES	20480	/* Initial number of types. */
#define	TYPE_TABLE_GROW		8192	/* Number of types to grow by. */

/* The global type table */
static ttable_t *global_type_table;

/* Handy macros to access type table information directly. */
#define	tt_used	tt_hdr.tth_used
#define	tt_have	tt_hdr.tth_have
#define	tt_size	tt_hdr.tth_size

/* ttable_create() - Create a new type table and attach it to given pointer. */
static stabsret_t
ttable_create(ttable_t **ttable)
{
	size_t type_size;
	size_t ttable_size;
	ttable_t *t;

	type_size = TYPE_TABLE_NTYPES * sizeof (type_t);
	ttable_size = sizeof (struct type_table_hdr) + type_size;

	if ((t = calloc(1, ttable_size)) == NULL) {
		return (STAB_NOMEM);
	}

	/* Assign initial values. */
	t->tt_have = TYPE_TABLE_NTYPES;
	t->tt_size = type_size;

	*ttable = t;
	return (STAB_SUCCESS);
}

/*
 * ttable_destroy_type_table() - Free the given type table.
 *
 * NOTE:
 *	Though this memory usually persists for the life of the process,
 *	the ability to free this memory is available.  At a minimum it
 *	is useful for checking memory leaks.
 */
static void
ttable_destroy_type_table(ttable_t **ttable)
{
	free(*ttable);
	*ttable = NULL;
}

/* ttable_grow_types() - Grow the type table. */
static stabsret_t
ttable_grow_types(ttable_t **ttable)
{
	ttable_t *t = *ttable;
	ttable_t *new_ttable;
	type_t *new_types;
	uint_t new_have;
	size_t new_type_size;
	size_t new_ttable_size;
	size_t diff;

	/* Sanity check. */
	if (t == NULL		||
	    t->tt_have == 0	||
	    t->tt_size == 0) {
		return (STAB_FAIL);
	}

	/* Calculate new info. */
	new_have = t->tt_have + TYPE_TABLE_GROW;
	new_type_size = new_have * sizeof (type_t);
	new_ttable_size = sizeof (struct type_table_hdr) + new_type_size;

	if ((new_ttable = realloc(t, new_ttable_size)) == NULL) {
		return (STAB_NOMEM);
	}

	/* Update in case it changed. */
	t = new_ttable;

	/* Zero the new memory. */
	new_types = new_ttable->tt_types + t->tt_have;
	diff = new_type_size - t->tt_size;
	(void) memset(new_types, 0, diff);

	/* Update the type table with new info */
	t->tt_have = new_have;
	t->tt_size = new_type_size;

	*ttable = t;

	return (STAB_SUCCESS);
}

/*
 * ttable_get_type_from_table() - Get/create a new type in the given
 *	type table for the given type descriptor.
 *
 * If <*td> is not TS_NOTYPE then
 *	 its range will be checked.
 *
 * If <*td> == TD_NOTYPE and the type is named (<namestr>) then
 *	we try to find the named type in the hash and assign the
 *	<*td> to the type descriptor that is associated with the name.
 *
 * Otherwise:
 *	Create a new type.
 *
 * NOTE:
 *	New types and type descriptors are ALWAYS
 *	increasing and there are never any "holes".
 */
static stabsret_t
ttable_get_type_from_table(ttable_t **ttable, typedesc_t *td,
    namestr_t *namestr)
{
	stabsret_t ret;
	ttable_t *t;

	t = *ttable;

	if (*td != TD_NOTYPE) {
		/* Check the validity of the type descriptor */
		if (*td > t->tt_used || *td < 1) {
			ret = STAB_FAIL;
		}
	} else {
		/* See if the type is named */
		if (namestr != NULL	&&
		    namestr->ms_str != NULL) {
			hnode_t *hnode = NULL;
			char save;

			/* Check if the name is in the hash table. */
			save = namestr->ms_str[namestr->ms_len];
			namestr->ms_str[namestr->ms_len] = '\0';

			ret = hash_get_name(namestr->ms_str,
			    &hnode, HASH_FIND);

			/* Is there a type descriptor for that name? */
			if (ret == STAB_SUCCESS &&
			    hnode->hn_td != TD_NOTYPE) {
				/* Reuse the type. */
				*td = hnode->hn_td;
				/* Restore the save character. */
				namestr->ms_str[namestr->ms_len] = save;
				/* We are done. */
				return (ret);
			}
			/* Restore the save character. */
			namestr->ms_str[namestr->ms_len] = save;
		}

		/* New type is needed. */
		if (t->tt_used >= t->tt_have) {
			ret = ttable_grow_types(ttable);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
		}

		/* Update type table information. */
		t = *ttable;
		++t->tt_used;
		*td = t->tt_used;
		ret = STAB_SUCCESS;
	}
	return (ret);
}

/* ttable_get_type() - Get/create a type from the global type table. */
stabsret_t
ttable_get_type(typedesc_t *td, namestr_t *namestr)
{
	return (ttable_get_type_from_table(&global_type_table,
	    td, namestr));
}

/*
 * ttable_td2ptr_from_table() - Get a 'type_t *' from the given type table for
 *	a given type descriptor.
 *
 * NOTE:
 * 	The type tables can be relocated at ANY time.  This is the only
 *	way to get at the information befind a type descriptor.
 */
static stabsret_t
ttable_td2ptr_from_table(ttable_t *ttable, typedesc_t td, type_t **type)
{
	if (td < 1 || td > ttable->tt_used) {
		return (STAB_FAIL);
	}

	--td;
	*type = &ttable->tt_types[td];

	return (STAB_SUCCESS);
}

/* ttable_td2ptr_from_table() - Get a 'type_t *' from the global type table. */
stabsret_t
ttable_td2ptr(typedesc_t td, type_t **type)
{
	return (ttable_td2ptr_from_table(global_type_table, td, type));
}

stabsret_t
ttable_create_table(void)
{
	stabsret_t ret;

	/* Initialize type table if NULL */
	if (global_type_table == NULL) {
		ret = ttable_create(&global_type_table);
	} else {
		ret = STAB_FAIL;
	}

	return (ret);
}


/* ttable_destroy() - Free the global type table. */
void
ttable_destroy_table(void)
{
	ttable_destroy_type_table(&global_type_table);
}

void
ttable_report(void)
{
	(void) fprintf(stderr, "==== type_table: size = %.2f K ====\n",
	    (float)(global_type_table->tt_size / 1024.0));
}
