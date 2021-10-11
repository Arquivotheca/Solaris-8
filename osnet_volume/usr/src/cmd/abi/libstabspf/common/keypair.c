/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)keypair.c	1.1	99/05/14 SMI"

#include <stdlib.h>
#include <string.h>
#include "stabspf_impl.h"

/*
 * The keypair table is a two dimensional ragged array that contains
 * type descriptors.  Wastes some memory but will be flushed for every
 * compilation unit and freed when we are done.
 */

/* Type key array that contains type descriptors */
typedef struct keypair_type_table {
	uint_t ktt_ntds;	/* Number of type descriptors. */
	size_t ktt_size;	/* Size, in bytes. */
	typedesc_t *ktt_tds;	/* Type descriptor array. */
} ktypet_t;
#define	KTT_TD_INIT	128	/* Initial size. */
#define	KTT_TD_GROW	16	/* Grow by this much if needed. */

/* File key array that contains type key arrays. */
typedef struct keypair_file_table {
	uint_t kft_nktypets;		/* Number of type key arrays. */
	size_t kft_size;		/* Size, in bytes. */
	ktypet_t  **kft_ktypets;	/* Array of type key arrays. */

} kfilet_t;
#define	KFT_KTT_INIT	384	/* Initial size. */
#define	KFT_KTT_GROW	50	/* Grow by this much if needed. */

/*
 * Global keypair table
 * Usage: global_keypair_table->kft_ktypets[kp_file]->ktt_tds[kp_type]
 */
static kfilet_t *global_keypair_table;

/* ktypet_new() - Create a type key table. */
static stabsret_t
ktypet_new(ktypet_t **ktypet)
{
	if ((*ktypet = calloc(1, sizeof (ktypet_t))) == NULL) {
		return (STAB_NOMEM);
	}
	return (STAB_SUCCESS);
}
/* ktypet_destroy() - Free a type key table. */
static void
ktypet_destroy(ktypet_t **ktypet) {
	free(*ktypet);
	*ktypet = NULL;
}


/* ktypet_new_tds() - Create type descriptor elements for a type key array. */
static stabsret_t
ktypet_new_tds(ktypet_t *ktypet)
{
	/* Sanity Check */
	if (ktypet == NULL		||
	    ktypet->ktt_tds != NULL	||
	    ktypet->ktt_ntds != 0	||
	    ktypet->ktt_size != 0) {
		return (STAB_FAIL);
	}

	/* Create array of type descriptors. */
	ktypet->ktt_tds =
	    calloc(KTT_TD_INIT, sizeof (typedesc_t));
	if (ktypet->ktt_tds == NULL) {
		return (STAB_NOMEM);
	}

	/* Initialize information for type key array. */
	ktypet->ktt_ntds = KTT_TD_INIT;
	ktypet->ktt_size = KTT_TD_INIT * sizeof (typedesc_t);

	return (STAB_SUCCESS);
}

/*
 * ktypet_grow_tds() - Grow The number of type descriptors in the type key
 * 	array.
 *
 * Argument <hint> is the type key that required the growth to be
 * necessary. We must assure that this type key is accommodated in the case
 * that the growsize is too small.
 */
static stabsret_t
ktypet_grow_tds(ktypet_t *ktypet, int hint)
{
	uint_t new_ntds;
	size_t new_size;
	size_t diff;
	typedesc_t *new_tds;

	/* Sanity check. */
	if (ktypet == NULL		||
	    ktypet->ktt_tds == NULL	||
	    ktypet->ktt_ntds == 0	||
	    ktypet->ktt_size == 0) {
		return (STAB_FAIL);
	}

	/* Calculate the new number of type descriptors in the array. */
	new_ntds = ktypet->ktt_ntds + KTT_TD_GROW;

	/* Is it big enough for our current purposes? */
	if (new_ntds < hint) {
		/* Make it even bigger. */
		new_ntds += hint;
	}
	/* New size in bytes. */
	new_size = new_ntds * sizeof (typedesc_t);

	if ((new_tds = realloc(ktypet->ktt_tds, new_size)) == NULL) {
		return (STAB_NOMEM);
	}

	ktypet->ktt_tds = new_tds;

	/* Assign the new portion. This assures the creation of new types. */
	new_tds += ktypet->ktt_ntds;
	diff = new_size - ktypet->ktt_size;
	(void) memset(new_tds, TD_NOTYPE, diff);

	/* Update type key array information.  */
	ktypet->ktt_size = new_size;
	ktypet->ktt_ntds = new_ntds;

	return (STAB_SUCCESS);
}

/*
 * ktypet_flush_tds() - initialize all type descriptors
 *	in the type key array.
 */
static stabsret_t
ktypet_flush_tds(ktypet_t *ktypet)
{
	/* Sanity check. */
	if (ktypet == NULL		||
	    ktypet->ktt_tds == NULL	||
	    ktypet->ktt_ntds == 0	||
	    ktypet->ktt_size == 0) {
		return (STAB_FAIL);
	}

	(void) memset(ktypet->ktt_tds, TD_NOTYPE, ktypet->ktt_size);

	return (STAB_SUCCESS);
}

/* ktypet_destroy_tds() - free the type descriptors in the type key array. */
static stabsret_t
ktypet_destroy_tds(ktypet_t *ktypet)
{
	/* Sanity check. */
	if (ktypet == NULL		||
	    ktypet->ktt_tds == NULL	||
	    ktypet->ktt_ntds == 0	||
	    ktypet->ktt_size == 0) {
		return (STAB_FAIL);
	}

	free(ktypet->ktt_tds);
	ktypet->ktt_tds = NULL;
	ktypet->ktt_ntds = 0;
	ktypet->ktt_size = 0;

	return (STAB_SUCCESS);
}

/*
 * ktypet_lookup_type() - Find/add a type key to the type key array.
 *
 * If the type key is associated with a named type then see if a type
 * descriptor already exists for the name and use it.
 *
 * If this is a new type key, the type descriptor will be TD_NOTYPE.
 */
static stabsret_t
ktypet_lookup_type(ktypet_t *ktypet, int key, typedesc_t *td,
    namestr_t *namestr)
{
	stabsret_t ret;

	/* Key is ALWAYS greater than zero. */
	if (key < 1) {
		return (STAB_FAIL);
	}

	/* Create a type key array if necessary. */
	if (ktypet->ktt_ntds == NULL) {
		ret = ktypet_new_tds(ktypet);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* Grow type key array if necessary. */
	if (key > ktypet->ktt_ntds) {
		ret = ktypet_grow_tds(ktypet, key);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/*
	 * Type Key is ALWAYS greater than zero but should still use
	 * the zero index of the array.
	 */
	--key;

	if (ktypet->ktt_tds[key] == TD_NOTYPE) {
		/* new type key */
		ret = ttable_get_type(&ktypet->ktt_tds[key], namestr);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* Assign type descriptor */
	*td = ktypet->ktt_tds[key];

	return (STAB_SUCCESS);
}


/* kfilet_new() - Create a file key table. */
static stabsret_t
kfilet_new(kfilet_t **kfilet)
{
	kfilet_t *new_kfilet;

	if ((new_kfilet = calloc(1, sizeof (kfilet_t))) == NULL) {
		return (STAB_NOMEM);
	}

	*kfilet = new_kfilet;

	return (STAB_SUCCESS);
}

/* kfilet_destroy() - Free a file key table. */
static void
kfilet_destroy(kfilet_t **kfilet)
{
	free(*kfilet);
	*kfilet = NULL;
}

/* kfilet_new_ktypets() - Create type key tables for a file key table. */
static stabsret_t
kfilet_new_ktypets(kfilet_t *kfilet)
{
	if (kfilet == NULL		||
	    kfilet->kft_ktypets != NULL	||
	    kfilet->kft_nktypets != 0	||
	    kfilet->kft_size != 0) {
		return (STAB_FAIL);
	}

	kfilet->kft_ktypets =
	    calloc(KFT_KTT_INIT, sizeof (kfilet->kft_ktypets));
	if (kfilet->kft_ktypets == NULL) {
		return (STAB_NOMEM);
	}
	kfilet->kft_nktypets = KFT_KTT_INIT;
	kfilet->kft_size = KFT_KTT_INIT * sizeof (kfilet->kft_ktypets);

	return (STAB_SUCCESS);
}

/*
 * kfilet_grow_ktypets() - Grow the number of type key tables for this
 *	file key table.
 *
 * Use <hint> to make sure we grow enough.
 */
static stabsret_t
kfilet_grow_ktypets(kfilet_t *kfilet, int hint)
{
	uint_t new_nktypets;
	size_t new_size;
	size_t diff;
	ktypet_t **new_ktypets;

	/* Sanity Check */
	if (kfilet == NULL		||
	    kfilet->kft_ktypets == NULL ||
	    kfilet->kft_nktypets == 0	||
	    kfilet->kft_size == 0) {
		return (STAB_FAIL);
	}

	/* New number of type key tables for this file key */
	new_nktypets = kfilet->kft_nktypets + KFT_KTT_GROW;
	if (new_nktypets <= hint) {
		new_nktypets += hint;
	}
	new_size = new_nktypets * sizeof (ktypet_t *);

	if ((new_ktypets = realloc(kfilet->kft_ktypets, new_size)) == NULL) {
		return (STAB_NOMEM);
	}

	kfilet->kft_ktypets = new_ktypets;

	/* Zero the new portion, since we cannot have a type key of zero. */
	new_ktypets += kfilet->kft_nktypets;
	diff = new_size - kfilet->kft_size;
	(void) memset(new_ktypets, 0, diff);

	/* update kfilet */
	kfilet->kft_size = new_size;
	kfilet->kft_nktypets = new_nktypets;

	return (STAB_SUCCESS);
}

/* kfilet_flush_ktypets() - Flush all the information in a filetype table. */
static stabsret_t
kfilet_flush_table(kfilet_t *kfilet)
{
	int i;
	stabsret_t ret = STAB_SUCCESS;

	/* Sanity check. */
	if (kfilet == NULL		||
	    kfilet->kft_ktypets == NULL ||
	    kfilet->kft_nktypets == 0	||
	    kfilet->kft_size == 0) {
		return (STAB_FAIL);
	}

	/*
	 * Step through the entire file key array and
	 * flush the type descriptors.
	 */
	for (i = 0; ret == STAB_SUCCESS && i < kfilet->kft_nktypets; i++) {
		if (kfilet->kft_ktypets[i] == NULL) {
			continue;
		}
		ret = ktypet_flush_tds(kfilet->kft_ktypets[i]);
	}

	return (ret);
}

/* kfilet_destroy_table() - Free all the memory in a filetype table. */
static stabsret_t
kfilet_destroy_table(kfilet_t *kfilet)
{
	int i;
	stabsret_t ret = STAB_SUCCESS;

	/* Sanity check. */
	if (kfilet == NULL		||
	    kfilet->kft_ktypets == NULL ||
	    kfilet->kft_nktypets == 0	||
	    kfilet->kft_size == 0) {
		return (STAB_FAIL);
	}

	/*
	 * Step through the entire file key array and
	 * flush the type descriptors.
	 */
	for (i = 0; ret == STAB_SUCCESS && i < kfilet->kft_nktypets; i++) {
		if (kfilet->kft_ktypets[i] == NULL) {
			continue;
		}
		ret = ktypet_destroy_tds(kfilet->kft_ktypets[i]);
		ktypet_destroy(&kfilet->kft_ktypets[i]);
	}

	if (ret == STAB_SUCCESS) {
		free(kfilet->kft_ktypets);
		kfilet->kft_ktypets = NULL;
		kfilet->kft_nktypets = 0;
		kfilet->kft_size = 0;
	}

	return (ret);
}

/*
 * kfilet_lookup_type() - Find/add a keypair to a given keypair table.
 *
 * If either file key or type key is KP_EMPTY then there is to be no
 * keypair associated with the new type.
 */
static stabsret_t
kfilet_lookup_type(kfilet_t *kfilet, int key, int type_key,
    typedesc_t *td, namestr_t *namestr)
{
	stabsret_t ret;

	if (key == KP_EMPTY || type_key == KP_EMPTY) {
		/* Just create a type with no keypair */
		return (ttable_get_type(td, namestr));
	}

	/* Sanity check. */
	if (key < 0) {
		return (STAB_FAIL);
	}

	/* Is the array big enough? */
	if (key >= kfilet->kft_nktypets) {
		if ((ret = kfilet_grow_ktypets(kfilet, key)) != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* If nothing linked at the key position get one and link it in. */
	if (kfilet->kft_ktypets[key] == NULL) {
		/* Create a new type key table for this file key. */
		ret = ktypet_new(&kfilet->kft_ktypets[key]);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* Get/create a type descriptor for the keypair. */
	ret = ktypet_lookup_type(kfilet->kft_ktypets[key], type_key,
	    td, namestr);

	return (ret);
}

/*
 * keypair_lookup_type() - Get/create a type descriptor from the
 *	 global_keypair_table using a keypair.
 */
stabsret_t
keypair_lookup_type(keypair_t *kp, typedesc_t *td, namestr_t *namestr)
{
	/* Get/create the type descriptor. */
	return (kfilet_lookup_type(global_keypair_table,
	    kp->kp_file, kp->kp_type, td, namestr));
}

/* keypair_create_table() - Create the global_keypair_table. */
stabsret_t
keypair_create_table(void)
{
	stabsret_t ret;

	if (global_keypair_table == NULL) {
		ret = kfilet_new(&global_keypair_table);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		ret = kfilet_new_ktypets(global_keypair_table);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	} else {
		ret = STAB_FAIL;
	}

	return (ret);
}

/* keypair_flush_table() - Flush the global_keypair_table. */
stabsret_t
keypair_flush_table(void)
{
	return (kfilet_flush_table(global_keypair_table));
}

/* keypair_destroy_table() - Free the global_keypair_table. */
stabsret_t
keypair_destroy_table(void)
{
	stabsret_t ret;

	ret = kfilet_destroy_table(global_keypair_table);
	if (ret == STAB_SUCCESS) {
		kfilet_destroy(&global_keypair_table);
	}

	return (ret);
}

void
keypair_report(void)
{
	size_t sum;
	kfilet_t *kfilet = global_keypair_table;
	int i;

	sum = kfilet->kft_size;

	for (i = 0; i < kfilet->kft_nktypets; i++) {
		if (kfilet->kft_ktypets[i] == NULL) {
			continue;
		}
		sum += kfilet->kft_ktypets[i]->ktt_size;
	}
	(void) fprintf(stderr, "==== keypair: size = %.2f K ====\n",
	    (float)(sum / 1024.0));
}
