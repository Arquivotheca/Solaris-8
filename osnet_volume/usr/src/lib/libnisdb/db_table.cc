/*
 *	db_table.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_table.cc	1.10	96/02/27 SMI"

#include <stdio.h>
#include <malloc.h>
#include "db_headers.h"
#include "db_table.h"
#include "db_pickle.h"    /* for dump and load */
#include "db_entry.h"

/* How much to grow table by */
#define DB_TABLE_GROWTH_INCREMENT 1024

/* 0'th not used; might be confusing. */
#define DB_TABLE_START 1 

/* prevents wrap around numbers from being passed */
#define	CALLOC_LIMIT 536870911

/* Initial table sizes to use before using 1K increments. */
/* This helps conserve memory usage when there are lots of small tables. */
static int tabsizes[] = {
	16,
	128,
	512,
	DB_TABLE_GROWTH_INCREMENT,
	0
	};

/* Returns the next size to use for table */
static long unsigned
get_new_table_size(long unsigned oldsize)
{
	long unsigned newsize = 0, n;
	if (oldsize == 0)
		newsize = tabsizes[0];
	else {
		for (n = 0; newsize = tabsizes[n++];)
			if (oldsize == newsize) {
				newsize = tabsizes[n];	/* get next size */
				break;
			}
		if (newsize == 0)
			newsize = oldsize + DB_TABLE_GROWTH_INCREMENT;
	}
	return (newsize);
}


/* destructor */
db_free_list::~db_free_list()
{
	reset();   /* free list entries */
}

void
db_free_list::reset()
{
	db_free_entry *current, *nextentry;
	for (current = head; current != NULL;) {
		nextentry = current->next;
		delete current;
		current = nextentry;
	}
	head = NULL;
	count = 0;
}

/* Returns the location of a free entry, or NULL, if there aren't any. */
entryp
db_free_list::pop()
{
	if (head == NULL) return NULL;
	db_free_entry* old_head = head;
	entryp found = head->where;
	head = head->next;
	delete old_head;
	--count;
	return (found);
}

/*
 * Adds given location to the free list.
 * Returns TRUE if successful, FALSE otherwise (when out of memory).
*/
bool_t
db_free_list::push(entryp tabloc)
{
	db_free_entry * newentry = new db_free_entry;
	if (newentry == NULL)
	    FATAL("db_free_list::push: cannot allocation space",
		    DB_MEMORY_LIMIT);
	newentry->where = tabloc;
	newentry->next = head;
	head = newentry;
	++count;
	return (TRUE);
}

/*
 * Returns in a vector the information in the free list.
 * Vector returned is of form: [n free cells][n1][n2][loc1], ..[locn].
 * Leave the first 'n' cells free.
 * n1 is the number of entries that should be in the freelist.
 * n2 is the number of entries actually found in the freelist.
 * [loc1...locn] are the entries.   n2 <= n1 because we never count beyond n1.
 * It is up to the caller to free the returned vector when he is through.
*/
long*
db_free_list::stats(int nslots)
{
	long	realcount = 0,
		i,
		liststart = nslots,		// start of freelist
		listend = nslots+count+2;	// end of freelist
	long * answer = (long*) malloc((int) (listend)*sizeof (long));
	db_free_entry_p current = head;

	if (answer == 0)
	    FATAL("db_free_list::stats:  cannot allocation space",
		    DB_MEMORY_LIMIT);

	answer[liststart] = count;  /* size of freelist */

	for (i = liststart+2; i < listend && current != NULL; i++) {
		answer[i] = current->where;
		current = current->next;
		++realcount;
	}

	answer[liststart+1] = realcount;
	return (answer);
}


/* db_table constructor */
db_table::db_table() : freelist()
{
	tab = NULL;
	table_size = 0;
	last_used = 0;
	count = 0;
/*  grow(); */
}

/*
 * db_table destructor:
 * 1.  Get rid of contents of freelist
 * 2.  delete all entries hanging off table
 * 3.  get rid of table itself
*/
db_table::~db_table()
{
	reset();
}

/* reset size and pointers */
void
db_table::reset()
{
	int i, done = 0;

	freelist.reset();

	/* Add sanity check in case of table corruption */
	if (tab != NULL) {
		for (i = 0;
		     i <= last_used && i < table_size && done < count;
		     i++) {
			if (tab[i]) {
				free_entry(tab[i]);
				++done;
			}
		}
	}

	delete tab;
	table_size = last_used = count = 0;
	tab = NULL;
}


/* Expand the table.  Fatal error if insufficient memory. */
void
db_table::grow()
{
	long oldsize = table_size;
	entry_object_p *oldtab = tab;
	long i;

	table_size = get_new_table_size(oldsize);

#ifdef DEBUG
	fprintf(stderr, "db_table GROWING to %d\n", table_size);
#endif

	if (table_size > CALLOC_LIMIT) {
		table_size = oldsize;
		FATAL("db_table::grow: table size exceeds calloc limit",
			DB_MEMORY_LIMIT);
	}

//  if ((tab = new entry_object_p[table_size]) == NULL)
	if ((tab = (entry_object_p*)
		calloc((unsigned int) table_size,
			sizeof (entry_object_p))) == NULL) {
		tab = oldtab;		// restore previous table info
		table_size = oldsize;
		FATAL("db_table::grow: cannot allocate space", DB_MEMORY_LIMIT);
	}

	if (oldtab != NULL) {
		for (i = 0; i < oldsize; i++) { // transfer old to new
			tab[i] = oldtab[i];
		}
		delete oldtab;
	}
}

/*
 * Return the first entry in table, also return its position in
 * 'where'.  Return NULL in both if no next entry is found.
*/
entry_object*
db_table::first_entry(entryp * where)
{
	if (count == 0 || tab == NULL) {  /* empty table */
		*where = NULL;
		return (NULL);
	} else {
		entryp i;
		for (i = DB_TABLE_START;
			i < table_size && i <= last_used; i++) {
			if (tab[i] != NULL) {
				*where = i;
				return (tab[i]);
			}
		}
	}
	*where = NULL;
	return (NULL);
}

/*
 * Return the next entry in table from 'prev', also return its position in
 * 'newentry'.  Return NULL in both if no next entry is found.
 */
entry_object *
db_table::next_entry(entryp prev, entryp* newentry)
{
	long i;
	if (prev >= table_size || tab == NULL || tab[prev] == NULL)
		return (NULL);
	for (i = prev+1; i < table_size && i <= last_used; i++) {
		if (tab[i] != NULL) {
			*newentry = i;
			return (tab[i]);
		}
	}
	*newentry = NULL;
	return (NULL);
}

/* Return entry at location 'where', NULL if location is invalid. */
entry_object *
db_table::get_entry(entryp where)
{
	if (where < table_size && tab != NULL && tab[where] != NULL)
		return (tab[where]);
	else
		return (NULL);
}

/*
 * Add given entry to table in first available slot (either look in freelist
 * or add to end of table) and return the the position of where the record
 * is placed. 'count' is incremented if entry is added. Table may grow
 * as a side-effect of the addition. Copy is made of input.
*/
entryp
db_table::add_entry(entry_object * obj)
{
	entryp where = freelist.pop();
	if (where == NULL) {				/* empty freelist */
		if (last_used >= (table_size-1))	/* full (> is for 0) */
			grow();
		where = ++last_used;
	}
	if (tab != NULL) {
		++count;
		tab[where] = new_entry(obj);
		return (where);
	} else {
		return (NULL);
	}
}

/*
 * Replaces object at specified location by given entry.
 * Returns TRUE if replacement successful; FALSE otherwise.
 * There must something already at the specified location, otherwise,
 * replacement fails. Copy is not made of the input.
 * The pre-existing entry is freed.
 */
bool_t
db_table::replace_entry(entryp where, entry_object * obj)
{
	if (where < DB_TABLE_START || where >= table_size ||
	    tab == NULL || tab[where] == NULL)
		return (FALSE);
	free_entry(tab[where]);
	tab[where] = obj;
	return (TRUE);
}

/*
 * Deletes entry at specified location.  Returns TRUE if location is valid;
 * FALSE if location is invalid, or the freed location cannot be added to
 * the freelist.  'count' is decremented if the deletion occurs.  The object
 * at that location is freed.
 */
bool_t
db_table::delete_entry(entryp where)
{
	if (where < DB_TABLE_START || where >= table_size ||
	    tab == NULL || tab[where] == NULL)
		return (FALSE);
	free_entry(tab[where]);
	tab[where] = NULL;    /* very important to set it to null */
	--count;
	if (where == last_used) { /* simple case, deleting from end */
		--last_used;
		return (TRUE);
	} else {
		return (freelist.push(where));
	}
}

/*
 * Returns statistics of table.
 * [vector_size][table_size][last_used][count][freelist].
 * It is up to the caller to free the returned vector when his is through.
 * The free list is included if 'fl' is TRUE.
*/
long *
db_table::stats(bool_t include_freelist)
{
	long * answer;

	if (include_freelist)
		answer = freelist.stats(3);
	else {
		answer = (long *) malloc(3*sizeof (long));
	}

	if (answer) {
		answer[0] = table_size;
		answer[1] = last_used;
		answer[2] = count;
	}
	return (answer);
}

/* ************************* pickle_table ********************* */
/* Does the actual writing to/from file specific for db_table structure. */
/* This was a static earlier with the func name being transfer_aux. The
 * backup and restore project needed this to copy files over.
 */
bool_t
transfer_aux_table(XDR* x, pptr dp)
{
	return (xdr_db_table(x, (db_table*) dp));
}

class pickle_table: public pickle_file {
    public:
        pickle_table(char* f, pickle_mode m) : pickle_file(f, m) {}
 
        /* Transfers db_table structure pointed to by dp to/from file. */
        int transfer(db_table* dp)
                { return (pickle_file::transfer((pptr) dp,&transfer_aux_table)); }
};

/*
 * Writes the contents of table, including the all the entries, into the
 * specified file in XDR format.  May need to change this to use APPEND
 * mode instead.
 */
int
db_table::dump(char *file)
{
	pickle_table f(file, PICKLE_WRITE);   /* may need to use APPEND mode */
	int status = f.transfer(this);
	if (status == 1)
		return (-1);
	else
		return (status);
}

/* Constructor that loads in the table from the given file */
db_table::db_table(char *file)  : freelist()
{
	pickle_table f(file, PICKLE_READ);
	tab = NULL;
	table_size = last_used = count = 0;

	/* load  table */
	if (f.transfer(this) < 0) {
		/* fell through, something went wrong, initialize to null */
		tab = NULL;
		table_size = last_used = count = 0;
		freelist.init();
	}
}
