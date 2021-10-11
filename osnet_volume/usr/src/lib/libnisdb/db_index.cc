/*
 *	db_index.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_index.cc	1.7	92/09/21 SMI"

#include <stdio.h>
#include <malloc.h>
#include "db_headers.h"
#include "db_index.h"
#include "db_pickle.h"

static int hashsizes[] = {		/* hashtable sizes */
	11,
	113,
	337,
	977,
	2053,
	4073,
	8011,
	16001,
	0
};

// prevents wrap around numbers from being passed
#define	CALLOC_LIMIT 536870911

/* Constructor: creates empty index. */
db_index::db_index()
{
	tab = NULL;
	table_size = 0;
	count = 0;
	case_insens = FALSE;
/*  grow(); */
}


/* Destructor: deletes index, including all associated db_index_entry. */
db_index::~db_index()
{
	reset();
}

/* Get rid of table and all associated entries, and reset counters */
void
db_index::reset()
{
	db_index_entry * curr, *n;
	int i;

	/* Add sanity test in case table was corrupted */
	if (tab != NULL) {
		for (i = 0; i < table_size; i++) {	// go through table
			curr = tab[i];
			while (curr != NULL) {		// go through bucket
				n = curr->getnextentry();
				delete curr;
				curr = n;
			}
		}
	}

	delete tab;				// get rid of table itself

	tab = NULL;
	table_size = count = 0;
}


/*
 * Initialize index according to the specification of the key descriptor
 * Currently, only affects case_insens flag of index.
 */
void
db_index::init(db_key_desc * k)
{
	if ((k->key_flags)&DB_KEY_CASE)
		case_insens = TRUE;
}

/* Returns the next size to use for the hash table */
static long unsigned
get_next_hashsize(long unsigned oldsize)
{
	long unsigned newsize = 0, n;
	if (oldsize == 0)
		newsize = hashsizes[0];
	else {
		for (n = 0; newsize = hashsizes[n++];)
			if (oldsize == newsize) {
				newsize = hashsizes[n];	/* get next size */
				break;
			}
		if (newsize == 0)
			newsize = oldsize * 2 + 1;	/* just double */
	}
	return (newsize);
}

/* Grow the current hashtable upto the next size.
	    The contents of the existing hashtable is copied to the new one and
	    relocated according to its hashvalue relative to the new size.
	    Old table is deleted after the relocation.
*/
void
db_index::grow()
{
	long unsigned oldsize = table_size, i;
	db_index_entry_p * oldtab = tab;

	table_size = get_next_hashsize(table_size);

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt, "savehash GROWING to %d\n", table_size);
#endif

	if (table_size > CALLOC_LIMIT) {
		table_size = oldsize;
		FATAL("db_index::grow: table size exceeds calloc limit",
			DB_MEMORY_LIMIT);
	}

	if ((tab = (db_index_entry_p*)
		calloc((unsigned int) table_size,
			sizeof (db_index_entry_p))) == NULL) {
		tab = oldtab;		// restore previous table info
		table_size = oldsize;
		FATAL("db_index::grow: cannot allocate space", DB_MEMORY_LIMIT);
	}

	if (oldtab != NULL) {		// must transfer contents of old to new
		for (i = 0; i < oldsize; i++) {
			oldtab[i]->relocate(tab, table_size);
		}
		delete oldtab;		// delete old hashtable
	}
}

/*
 * Look up given index value in hashtable.
 * Return pointer to db_index_entries that match the given value, linked
 * via the 'next_result' pointer.  Return in 'how_many_found' the size
 * of this list. Return NULL if not found.
 */
db_index_entry *
db_index::lookup(item* index_value, long* how_many_found)
{
	register unsigned long hval;
	unsigned long bucket;

	if (index_value == NULL || table_size == 0 || tab == NULL)
		return (NULL);
	hval = index_value->get_hashval(case_insens);
	bucket = hval % table_size;

	db_index_entry_p fst = tab[bucket ];

	if (fst != NULL)
		return (fst->lookup(case_insens, hval,
					index_value, how_many_found));
	else
		return (NULL);
}

/*
 * Remove the entry with the given index value and location 'recnum'.
 * If successful, return DB_SUCCESS; otherwise DB_NOTUNIQUE if index_value
 * is null; DB_NOTFOUND if entry is not found.
 * If successful, decrement count of number of entries in hash table.
 */
db_status
db_index::remove(item* index_value, entryp recnum)
{
	register unsigned long hval;
	unsigned long bucket;
	register db_index_entry *fst;

	if (index_value == NULL)
		return (DB_NOTUNIQUE);
	if (table_size == 0 || tab == NULL)
		return (DB_NOTFOUND);
	hval = index_value->get_hashval(case_insens);

	bucket = hval % table_size;

	fst = tab[bucket];
	if (fst == NULL)
		return (DB_NOTFOUND);

	if (fst->remove(&tab[bucket], case_insens, hval, index_value, recnum)) {
		--count;
		return (DB_SUCCESS);
	} else
		return (DB_NOTFOUND);
}

/*
 * Add a new index entry with the given index value and location 'recnum'.
 * Return DB_NOTUNIQUE, if entry with identical index_value and recnum
 * already exists.  If entry is added, return DB_SUCCESS.
 * Increment count of number of entries in index table and grow table
 * if number of entries equals size of table.
 * Note that a copy of index_value is made for new entry.
 */
db_status
db_index::add(item* index_value, entryp recnum)
{
	register unsigned long hval;

	if (index_value == NULL)
		return (DB_NOTUNIQUE);

	hval = index_value->get_hashval(case_insens);

	if (tab == NULL) grow();

	db_index_entry_p fst, newbucket;
	unsigned long bucket;
	bucket = hval %table_size;
	fst = tab[bucket];
	if (fst == NULL)  { /* Empty bucket */
		if ((newbucket =
	    new db_index_entry(hval, index_value, recnum, tab[bucket])) == NULL)
			FATAL("db_index::add: cannot allocate space",
				DB_MEMORY_LIMIT);
		tab[bucket] = newbucket;
	} else if (fst->add(&tab[bucket], case_insens,
				hval, index_value, recnum)){
		;  /* do nothing */
	} else return (DB_NOTUNIQUE);

	/* increase hash table size if number of entries equals table size */
	if (++count > table_size)
		grow();

	return (DB_SUCCESS);
}

/* ************************* pickle_index ********************* */

/* Does the actual writing to/from file specific for db_index structure. */
static bool_t
transfer_aux(XDR* x, pptr ip)
{
	return (xdr_db_index(x, (db_index*) ip));
}

class pickle_index: public pickle_file {
    public:
	pickle_index(char* f, pickle_mode m) : pickle_file(f, m) {}

	/* Transfers db_index structure pointed to by dp to/from file. */
	int transfer(db_index* dp)
		{ return (pickle_file::transfer((pptr) dp, &transfer_aux)); }
};

/* Dumps this index to named file. */
int
db_index::dump(char *file)
{
	pickle_index f(file, PICKLE_WRITE);
	int status =  f.transfer(this);

	if (status == 1)
		return (-1); /* cannot open for write */
	else
		return (status);
}

/* Constructor: creates index by loading it from the specified file.
	    If loading fails, creates empty index. */
db_index::db_index(char *file)
{
	pickle_index f(file, PICKLE_READ);
	tab = NULL;
	table_size = count = 0;

	/* load new hashbuf */
	if (f.transfer(this) < 0) {
		/* Load failed; reset. */
		tab = NULL;
		table_size = count = 0;
	}
}


/* Return in 'tsize' the table_size, and 'tcount' the number of entries
	    in the table. */
void
db_index::stats(long* tsize, long* tcount)
{
	*tsize = table_size;
	*tcount = count;
}

/* Print all entries in the table. */
void
db_index::print()
{
	long i;

	/* Add sanity check in case table corrupted */
	if (tab == NULL)
		return;
	for (i = 0; i < table_size; i++) {
		if (tab[i] != NULL)
			tab[i]->print_all();
	}
}
