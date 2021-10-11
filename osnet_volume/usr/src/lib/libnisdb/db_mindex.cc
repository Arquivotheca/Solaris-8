/*
 *	db_mindex.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_mindex.cc	1.11	97/04/21 SMI"

#include <stdio.h>

#include <malloc.h>
#include "db_headers.h"
#include "db_mindex.h"
#include "db_pickle.h"

/*
 *  Constructor:  Create new table using scheme defintion supplied.
 *  (Make copy of scheme and keep it with table.)
 */
db_mindex::db_mindex(db_scheme * how) : rversion()
{
	init(how);
}

/* Constructor:  Create empty table (no scheme, no table or indices). */
db_mindex::db_mindex() : rversion()
{
	scheme = NULL;
	table = NULL;
	indices.indices_len = 0;
	indices.indices_val = NULL;
}

db_mindex::~db_mindex()
{
	reset();   /* get rid of data structures first */
}

/*
 * Initialize table using information given in scheme 'how'.
 * Record the scheme for later use (make copy of it);
 * create the required number of indices; and create table for storing
 * entries.
 */
void
db_mindex::init(db_scheme * how)
{
	scheme = new db_scheme(how);		// make copy
	if (scheme == NULL)
		FATAL("db_mindex::init: could not allocate space for scheme",
			DB_MEMORY_LIMIT);

	if (scheme->numkeys() == 0) {
	    WARNING("db_mindex::init: empty scheme encountered");
	    /* what action should we take here? */
	}

	indices.indices_len = how->numkeys();
	db_key_desc * keys = how->keyloc();
	int i;

	/* homogeneous indices for now */
	indices.indices_val = new db_index[indices.indices_len];
	if (indices.indices_val == NULL) {
		delete scheme;
		indices.indices_len = 0;
		scheme = NULL;
		FATAL("db_mindex::init: could not allocate space for indices",
			DB_MEMORY_LIMIT);
	}
	for (i = 0; i < indices.indices_len; i++) {
		indices.indices_val[i].init(&(keys[i]));
	}
	table = new db_table();
	if (table == NULL) {
		delete scheme;
		scheme = NULL;
		delete indices.indices_val;
		indices.indices_val = NULL;
		indices.indices_len = 0;
		FATAL("db_mindex::init: could not allocate space for table",
			DB_MEMORY_LIMIT);
	}
	rversion.zero();
}

/* empty associated tables associated */
void
db_mindex::reset_tables()
{
	int i;

        /* Add sanity check in case of table corruption */
        if (indices.indices_val != NULL) {
		for (i = 0; i < indices.indices_len; i++) {
			indices.indices_val[i].reset();
		}
	}
	if (table) table->reset();
}


/*
 * Return a list of index_entries that satsify the given query 'q'.
 * Return the size of the list in 'count'. Return NULL if list is empty.
 * Return in 'valid' FALSE if query is not well formed.
*/
db_index_entry_p
db_mindex::satisfy_query(db_query *q, long * count, bool_t * valid)
{
	db_index_entry_p oldres = NULL, newres;
	int i, curr_ind;
	long num_new, num_old = 0;
	int limit = q->size();
	db_qcomp * comps = q->queryloc();
	if (valid) *valid = TRUE;   /* True to begin with. */

	/* Add sanity check in case table corrupted */
	if (indices.indices_len != 0 && indices.indices_val == NULL) {
		WARNING("db_mindex::satisfy_query: table has no indices");
		if (valid) *valid = FALSE;
		*count = 0;
		return (NULL);
	}

	for (i = 0; i < limit; i++) {
		if ((curr_ind = comps[i].which_index) < indices.indices_len) {
			newres = indices.indices_val[curr_ind].lookup(
					comps[i].index_value, &num_new);
			if (newres == NULL) {
				*count = 0;
				return (NULL);
			}
			if (oldres == NULL) {
				oldres = newres;
				num_old = num_new;
			} else {
				oldres = newres->join(num_new, num_old,
							oldres, &num_old);
				if (oldres == NULL) {
					*count = 0;
					return (NULL);
				}
			}
		} else {
			WARNING("db_mindex::satisfy_query: index out of range");
			if (valid) *valid = FALSE;
			*count = 0;
			return (NULL);
		}
	}
	*count = num_old;
	return (oldres);
}

/*
 * Returns an array of size 'count' of 'entry_object_p's, pointing to
 * copies of entry_objects named by the result list of db_index_entries 'res'.
 * Sets db_status 'statp' if error encountered; otherwise, leaves it unchanged.
*/
entry_object_p *
db_mindex::prepare_results(int count, db_index_entry_p res, db_status *statp)
{
	entry_object_p * entries = new entry_object_p[count];
	int i;

	if (entries == NULL) {
		FATAL("db_mindex::prepare_results: could not allocate space",
			DB_MEMORY_LIMIT);
	}

	for (i = 0; i < count; i++) {
		if (res == NULL) {
			int j;
			for (j = 0; j < i; j++) // cleanup
				free_entry(entries[j]);
			syslog(LOG_ERR,
				"db_mindex::prepare_results: incorrect count");
			*statp = DB_INTERNAL_ERROR;
		} else {
			entries[i] =
				new_entry(table->get_entry(res->getlocation()));
			res = res->getnextresult();
		}
	}

	return (entries);
}

/*
 * Returns a newly created db_query structure containing the index values
 * as obtained from the record named by 'recnum'.  The record itself, along
 * with information on the schema definition of this table, will determine
 * which values are extracted from the record and placed into the result.
 * Returns NULL if recnum is not a valid entry.
 * Note that space is allocated for the query and the index values
 * (i.e. do not share pointers with strings in 'obj'.)
 */
db_query *
db_mindex::extract_index_values_from_record(entryp recnum)
{
	return (extract_index_values_from_object(table->get_entry(recnum)));
}

/*
 * Returns a newly created db_query containing the index values as
 * obtained from the given object.  The object itself,
 * along with information on the scheme given, will determine
 * which values are extracted from the object and placed into the query.
 * Returns an empty query if 'obj' is not a valid entry.
 * Note that space is allocated for the query and the index values
 * (i.e. do not share pointers with strings in 'obj'.)
*/
db_query *
db_mindex::extract_index_values_from_object(entry_object_p obj)
{
	if (scheme->numkeys() != indices.indices_len) { // probably built wrong
		syslog(LOG_ERR,
	    "number of keys (%d) does not equal number of indices (%d)",
	    scheme->numkeys(), indices.indices_len);
		return (new db_query());	// null query
	}  else if (obj == NULL)
		return (NULL);
	else {
		db_query* answer = new db_query(scheme, obj);
		if (answer)
			return (answer);
		else {
			FATAL("db_mindex::extract: could not allocate space",
				DB_MEMORY_LIMIT);
		}
	}
}

/* Returns the first entry found in the table by setting 'answer' to
 * point to the a copy of entry_object.  Returns DB_SUCCESS if found;
 * DB_NOTFOUND otherwise.
*/
db_status
db_mindex::first(entryp *where, entry_object ** answer)
{
	entry_object_p ptr = table->first_entry(where);
	if (ptr == NULL)
		return (DB_NOTFOUND);

	*answer = new_entry(ptr);
	return (DB_SUCCESS);
}

/*
 * Returns the next entry in the table after 'previous' by setting 'answer' to
 * point to copy of the entry_object.  Returns DB_SUCCESS if 'previous' is
 * valid and next entry is found; DB_NOTFOUND otherwise.  Sets 'where' to
 * location of where entry is found for input as subsequent 'next' operation.
*/
db_status
db_mindex::next(entryp previous, entryp *where, entry_object **answer)
{
	if (!(table->entry_exists_p(previous)))
		return (DB_NOTFOUND);

	entry_object * ptr = table->next_entry(previous, where);
	if (ptr == NULL)
		return (DB_NOTFOUND);
	*answer = new_entry(ptr);
	return (DB_SUCCESS);
}

static void
delete_result_list(db_next_index_desc* orig)
{
	db_next_index_desc* curr, *save_next;
	for (curr = orig; curr != NULL;) {
		save_next = curr->next;
		delete curr;
		curr = save_next;
	}
}


static db_next_index_desc *
copy_result_list(db_index_entry* orig)
{
	db_next_index_desc *head = NULL, *curr;
	db_index_entry *current;

	for (current = orig; current != NULL;
		current = current->getnextresult()) {
		curr = new db_next_index_desc(current->getlocation(), head);
		if (curr == NULL) {
			FATAL(
			"db_mindex::copy_result_list: could not allocate space",
			DB_MEMORY_LIMIT);
		}
		head = curr;  // list is actually reversed
	}
	return (head);
}

/*
 * Delete the given list of results; used when no longer interested in
 * the results of the first/next query that returned this list.
 */
db_status
db_mindex::reset_next(db_next_index_desc *orig)
{
	if (orig == NULL)
		return (DB_NOTFOUND);

	delete_result_list(orig);
	return (DB_SUCCESS);
}

/*
* Finds entry that satisfy the query 'q'.  Returns the first answer by
* setting the pointer 'answer' to point to a copy of it.  'where' is set
* so that the other answers could be gotten by passing 'where' to 'next'
* successively.   Note that the answer is a  pointer to a copy of the entry.
* Returns DB_SUCCESS if search was successful; DB_NOTFOUND if none is found.
 */
db_status
db_mindex::first(db_query *q,
		db_next_index_desc **where, entry_object ** answer)
{
	long count;
	bool_t valid_query;
	db_index_entry * rp = satisfy_query(q, &count, &valid_query);

	if (valid_query != TRUE)
	    return (DB_BADQUERY);

	if (rp == NULL) {
		*answer = NULL;
		return (DB_NOTFOUND);
	}

	*where = copy_result_list(rp);

	entry_object_p ptr = table->get_entry((*where)->location);
	if (ptr == NULL)
		return (DB_NOTFOUND);

	*answer = new_entry(ptr);
	return (DB_SUCCESS);
}

/*
 * Returns the next entry in the table after 'previous' by setting 'answer' to
 * point to copy of the entry_object.  Next is next in chain of answers found
 * in previous first search with query.   Returns DB_SUCCESS if 'previous' is
 * valid and next entry is found; DB_NOTFOUND otherwise.  Sets 'where' to
 * location of where entry is found for input as subsequent 'next' operation.
*/
db_status
db_mindex::next(db_next_index_desc *previous, db_next_index_desc **where,
		entry_object **answer)
{
	if (previous == NULL)
		return (DB_NOTFOUND);

	// should further check validity of 'previous' pointer
	*where = previous->next;
	delete previous;    // delete previous entry
	if (*where == NULL)
		return (DB_NOTFOUND);

	entry_object * ptr = table->get_entry((*where)->location);
	if (ptr == NULL)
		return (DB_NOTFOUND);
	*answer = new_entry(ptr);
	return (DB_SUCCESS);
}

/*
 * Finds entry that satisfy the query 'q'.  Returns the answer by
 * setting the pointer 'rp' to point to the list of answers.
 * Note that the answers are pointers to the COPIES of entries.
 * Returns the number of answers find in 'count'.
 * Returns DB_SUCCESS if search found at least one answer;
 * returns DB_NOTFOUND if none is found.
*/
db_status
db_mindex::lookup(db_query *q, long* count, entry_object_p **result)
{
	bool_t valid_query;
	db_index_entry * rp = satisfy_query(q, count, &valid_query);
	db_status stat = DB_SUCCESS;

	if (valid_query != TRUE)
		return (DB_BADQUERY);

	if (rp == NULL) {
		*result = NULL;
		return (DB_NOTFOUND);
	}

	*result = prepare_results((int) *count, rp, &stat);

	return (stat);
}

/*
 * Return all entries within table.  Returns the answer by
 * setting the pointer 'rp' to point to the list of answers.
 * Note that the answers are pointers to copies of the entries.
 * Returns the number of answers find in 'count'.
 * Returns DB_SUCCESS if search found at least one answer;
 * returns DB_NOTFOUND if none is found.
*/
db_status
db_mindex::all(long* count, entry_object_p **result)
{
	entry_object *ptr;
	entryp where;
	long how_many, i;

	if (table == NULL || (how_many = table->fullness()) <= 0) {
		*result = NULL;
		return (DB_NOTFOUND);
	}

	entry_object_p * answer = new entry_object_p[how_many];
	if (answer == NULL) {
		FATAL("db_mindex::all: could not allocate space",
			DB_MEMORY_LIMIT);
	}

	*count = how_many;

	ptr = table->first_entry(&where);
	if (ptr != NULL)
		answer[0] = new_entry(ptr);
	else {
		WARNING("db_mindex::all: null first entry found in all");
		answer[0] = NULL;
	}
	for (i = 1; i < how_many; i++) {
		ptr = table->next_entry(where, &where);
		if (ptr != NULL)
			answer[i] = new_entry(ptr);
		else {
			WARNING(
			    "db_mindex::all: null internal entry found in all");
			answer[i] = NULL; /* Answer gets null too. -CM */
		}
	}
	*result = answer;
	return (DB_SUCCESS);
}

/*
 * Remove the entry identified by 'recloc' from:
 * 1.  all indices, as obtained by extracting the index values from the entry
 * 2.  table where entry is stored.
*/
db_status
db_mindex::remove_aux(entryp recloc)
{
	int i, curr_ind;

	/* get index values of this record */
	db_query * cq = extract_index_values_from_record(recloc);
	if (cq == NULL) {
		FATAL("db_mindex::remove_aux: could not allocate space",
			DB_MEMORY_LIMIT);
	}
	if (cq->size() != indices.indices_len) { /* something is wrong */
		delete cq; // clean up
		syslog(LOG_ERR,
	    "db_mindex::remove_aux: record contains wrong number of indices");
		return (DB_INTERNAL_ERROR);
	}
	db_qcomp * comps = cq->queryloc();

	/* Add sanity check in case of corrupted table */
	if (indices.indices_val != NULL) {
		/* update indices */
		for (i = 0; i < indices.indices_len; i++) {
			curr_ind = comps[i].which_index; /* not necessary if sorted */
			indices.indices_val[curr_ind].remove(
						comps[i].index_value, recloc);
		}
	}

	/* update table where record is stored */
	table->delete_entry(recloc);

	/* delete query */
	delete cq;
	return (DB_SUCCESS);
}

/*
 * Removes the entry in the table named by given query 'q'.
 * If a NULL query is supplied, all entries in table are removed.
 * Returns DB_NOTFOUND if no entry is found.
 * Returns DB_SUCCESS if one entry is found; this entry is removed from
 * its record storage, and it is also removed from all the indices of the
 * table. If more than one entry satisfying 'q' is found, all are removed.
 */
db_status
db_mindex::remove(db_query *q)
{
	long count = 0;
	db_index_entry *rp;
	db_status rstat;
	bool_t valid_query;

	if (q == NULL)  {  /* remove all entries in table */
		if (table != NULL && table->getsize() > 0) {
			reset_tables();
			return (DB_SUCCESS);
		} else
			return (DB_NOTFOUND);
	}

	rp = satisfy_query(q, &count, &valid_query);

	if (valid_query != TRUE)
		return (DB_BADQUERY);

	if (count == 0)		/* not found */
		return (DB_NOTFOUND);
	else if (count == 1) {	/* found, update indices  */
		return (remove_aux(rp->getlocation()));
	} else {		/* ambiguous, remove all entries */
		int i;
		db_index_entry *next_entry;
		for (i = 0; i < count; i++) {
			if (rp == NULL) {
				syslog(LOG_ERR,
			"db_mindex::remove:  incorrect number of indices");
				return (DB_INTERNAL_ERROR);
			}
			next_entry = rp->getnextresult(); // save before removal
			rstat = remove_aux(rp->getlocation ());
			if (rstat != DB_SUCCESS)
				return (rstat);
			rp = next_entry;		// go on to next
		}
		return (DB_SUCCESS);
	}
}

/*
 * Add copy of given entry to table.  Entry is identified by query 'q'.
 * The entry (if any) satisfying the query is first deleted, then
 *  added to the indices (using index values extracted form the given entry)
 * and the table.
 * Returns DB_NOTUNIQUE if more than one entry satisfies the query.
 * Returns DB_NOTFOUND if query is not well-formed.
 * Returns DB_SUCCESS if entry can be added.
*/
db_status
db_mindex::add(db_query *q, entry_object * obj)
{
	long count = 0;
	int i, curr_ind;
	bool_t valid;
	db_index_entry *rp = NULL;
	db_status rstat;

	/*
	 *  The argument q is only NULL when we know that there are
	 *  no objects in the database that match the object.
	 */
	if (q) {
		rp = satisfy_query(q, &count, &valid);
		if (!valid) {
			return (DB_BADQUERY);
		}
	}

	if (count == 1) {	/* found, first delete */
		if ((rstat = remove_aux(rp->getlocation())) != DB_SUCCESS)
			return (rstat);
		count = 0;	/* fall through to add */
	}

	if (count == 0) { 	/* not found, insert */
		entryp recloc = table->add_entry(obj); /* add object to table */
		/* get index values of this object, might be same as 'q' */
		db_query *cq = extract_index_values_from_object(obj);
		if (cq == NULL) {
			table->delete_entry(recloc);
			FATAL("db_mindex::add: could not allocate space for",
				DB_MEMORY_LIMIT);
		}
		if (cq ->size() != indices.indices_len) { /* something wrong */
			table->delete_entry(recloc);
			delete cq; // clean up
			syslog(LOG_ERR,
		    "db_mindex::add: record contains wrong number of indices");
			return (DB_INTERNAL_ERROR);
		}
		db_qcomp * comps = cq->queryloc();

		/* update indices */
		if (indices.indices_val != NULL) {
			for (i = 0; i < indices.indices_len; i++) {
				curr_ind = comps[i].which_index;
				indices.indices_val[curr_ind].add(
					comps[i].index_value, recloc);
			}
		}
		delete cq;  // clean up
		return (DB_SUCCESS);
	}  else  /* ambiguous */
		return (DB_NOTUNIQUE);
}

/* ************************* pickle_mindex ********************* */
/* Does the actual writing to/from file specific for db_mindex structure. */
static bool_t
transfer_aux(XDR* x, pptr rp)
{
	return (xdr_db_mindex(x, (db_mindex*) rp));
}

class pickle_mindex: public pickle_file {
    public:
	pickle_mindex(char* f, pickle_mode m) : pickle_file(f, m) {}

	/* Transfers db_mindex structure pointed to by dp to/from file. */
	int transfer(db_mindex* dp)
		{ return (pickle_file::transfer((pptr) dp, &transfer_aux)); }
};

/* Write this structure (table, indices, scheme) into the specified file. */
int
db_mindex::dump(char *file)
{
	pickle_mindex f(file, PICKLE_WRITE);
	int status = f.transfer(this);

	if (status == 1)
		return (-1); /* could not open for write */
	else
		return (status);
}

/*
 * Reset the table by: deleting all the indices, table of entries, and its
 * scheme.
*/
void
db_mindex::reset()
{
	reset_tables();   /* clear table contents first */

	if (indices.indices_val) {
		delete indices.indices_val;
		indices.indices_val = NULL;
	}
	if (table) { delete table; table = NULL;  }
	if (scheme) { delete scheme; scheme = NULL;  }
	indices.indices_len = 0;
	rversion.zero();
}

/*
 * Initialize table using information from specified file.
 * The table is first 'reset', then the attempt to load from the file
 * is made.  If the load failed, the table is again reset.
 * Therefore, the table will be modified regardless of the success of the
 * load.  Returns TRUE if successful, FALSE otherwise.
*/
int
db_mindex::load(char *file)
{
	pickle_mindex f(file, PICKLE_READ);
	int status;

	reset();

	/* load new mindex */
	if ((status = f.transfer(this)) < 0) {
		/* load failed.  Reset. */
		reset();
	}

	return (status);
}

/*
 * Prints statistics of the table.  This includes the size of the table,
 * the number of entries, and the index sizes.
 */
void
db_mindex::print_stats()
{
	long size, count, i;
	long* stats = table->stats(TRUE);

	printf("table_size = %d\n", stats[0]);
	printf("last_used = %d\n", stats[1]);
	printf("count = %d\n", stats[2]);
	printf("free list size = %d\n", stats[3]);
	printf("free list count = %d\n", stats[4]);

	for (i = 5; i < 5+stats[4]; i++) {
		printf("%d, ", stats[i]);
	}
	printf("\n");
	free((char *) stats);

	/* Add sanity check in case of corrupted table */
	if (indices.indices_val == NULL) {
		printf("No indices to print\n");
		return;
	}
	for (i = 0; i < indices.indices_len; i++) {
		printf("***** INDEX %d ******\n", i);
		indices.indices_val[i].stats(&size, &count);
		printf("index table size = %d\ncount = %d\n", size, count);
	}
}

/* Prints statistics about all indices of table. */
void
db_mindex::print_all_indices()
{
	int i;

	/* Add sanity check in case of corrupted table */
	if (indices.indices_val == NULL) {
		printf("No indices to print\n");
		return;
	}
	for (i = 0; i < indices.indices_len; i++) {
		printf("***** INDEX %d ******\n", i);
		indices.indices_val[i].print();
	}
}

/* Prints statistics about indices identified by 'n'. */
void
db_mindex::print_index(int n)
{
	if (n >= 0 && n < indices.indices_len)
		indices.indices_val[n].print();
}
