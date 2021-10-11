/*
 *	db_dictionary.cc
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_dictionary.cc	1.23	98/06/07 SMI"

#include "db_headers.h"
#include "db_entry.h"
#include "db_dictionary.h"
#include "db_dictlog.h"
#include "db_vers.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#ifdef TDRPC
#include <sysent.h>
#endif
#include <unistd.h>
#include <syslog.h>
#include <rpc/rpc.h>

typedef bool_t	(*db_func)(XDR *, db_table_desc *);
extern db_dictionary	*InUseDictionary;
extern db_dictionary	*FreeDictionary;
/* *************** dictionary version ****************** */

#define	DB_MAGIC 0x12340000
#define	DB_MAJOR 0
#define	DB_MINOR 10
#define	DB_VERSION_0_9	(DB_MAGIC|(DB_MAJOR<<8)|9)
#define	DB_ORIG_VERSION	DB_VERSION_0_9
#define	DB_CURRENT_VERSION (DB_MAGIC|DB_MAJOR<<8|DB_MINOR)

vers db_update_version;   /* Note 'global' for all dbs. */

#define	INMEMORY_ONLY 1

/*
 * Checks for valid version.  For now, there are two:
 * DB_VERSION_ORIG was the ORIGINAL one with major = 0, minor = 9
 * DB_CURRENT_VERSION is the latest one with changes in the database format
 *	for entry objects and the change in the dictionary format.
 *
 * Our current implementation can support both versions.
 */
static inline bool_t
db_valid_version(u_int vers)
{
	return ((vers == DB_CURRENT_VERSION) || (vers == DB_ORIG_VERSION));
}

static char *
db_version_str(u_int vers)
{
	static char vstr[128];
	u_int d_major =  (vers&0x0000ff00)>>8;
	u_int d_minor =  (vers&0x000000ff);

	sprintf(vstr, "SunSoft, SSM, Version %d.%d", d_major, d_minor);
	return (vstr);
}

/*
 * Special XDR version that checks for a valid version number.
 * If we don't find an acceptable version, exit immediately instead
 * of continuing to xdr rest of dictionary, which might lead to
 * a core dump if the formats between versions are incompatible.
 * In the future, there might be a switch to determine versions
 * and their corresponding XDR routines for the rest of the dictionary.
 */
extern "C" {
bool_t
xdr_db_dict_version(XDR *xdrs, db_dict_version *objp)
{
	if (xdrs->x_op == XDR_DECODE) {
		if (!xdr_u_int(xdrs, (u_int*) objp) ||
		    !db_valid_version(((u_int) *objp))) {
			syslog(LOG_ERR,
	"db_dictionary: invalid dictionary format! Expecting %s",
				db_version_str(DB_CURRENT_VERSION));
			fprintf(stderr,
	"db_dictionary: invalid dictionary format! Expecting %s\n",
				db_version_str(DB_CURRENT_VERSION));
			exit(1);
		}
	} else if (!xdr_u_int(xdrs, (u_int*) objp))
		return (FALSE);
	return (TRUE);
}

void
make_zero(vers* v)
{
	v->zero();
}


};


/* ******************* dictionary data structures *************** */

/* Delete contents of single db_table_desc pointed to by 'current.' */
static void
delete_table_desc(db_table_desc *current)
{
	if (current->table_name != NULL) delete current->table_name;
	if (current->scheme != NULL) delete current->scheme;
	if (current->database != NULL) delete current->database;
	delete current;
}

/* Create new table descriptor using given table name and table_object. */
db_status
db_dictionary::create_table_desc(char *tab, table_obj* zdesc,
				db_table_desc** answer)
{
	db_table_desc *newtab;
	if ((newtab = new db_table_desc) == NULL) {
		FATAL(
	    "db_dictionary::add_table: could not allocate space for new table",
		DB_MEMORY_LIMIT);
	}

	newtab->database = NULL;
	newtab->table_name = NULL;
	newtab->next = NULL;

	if ((newtab->scheme = new db_scheme(zdesc)) == NULL) {
		delete_table_desc(newtab);
		FATAL(
		"db_dictionary::add_table: could not allocate space for scheme",
		DB_MEMORY_LIMIT);
	}

	if (newtab->scheme->numkeys() == 0) {
		WARNING(
	"db_dictionary::add_table: could not translate table_obj to scheme");
		delete_table_desc(newtab);
		return (DB_BADOBJECT);
	}

	if ((newtab->table_name = strdup(tab)) == NULL) {
		delete_table_desc(newtab);
		FATAL(
	    "db_dictionary::add_table: could not allocate space for table name",
		DB_MEMORY_LIMIT);
	}

	if (answer)
		*answer = newtab;
	return (DB_SUCCESS);
}


/* Delete list of db_table_desc pointed to by 'head.' */
static void
delete_bucket(db_table_desc *head)
{
	db_table_desc * nextone, *current;

	for (current = head; current != NULL; current = nextone) {
		nextone = current->next;	// remember next
		delete_table_desc(current);
	}
}

static void
delete_dictionary(db_dict_desc *dict)
{
	db_table_desc* bucket;
	int i;
	if (dict) {
		if (dict->tables.tables_val) {
			/* delete each bucket */
			for (i = 0; i < dict->tables.tables_len; i++)
				bucket = dict->tables.tables_val[i];
				if (bucket)
					delete_bucket(bucket);
			/* delete table */
			delete dict->tables.tables_val;
		}
		/* delete dictionary */
		delete dict;
	}
}

/* Relocate bucket starting with this entry to new hashtable 'new_tab'. */
static void
relocate_bucket(db_table_desc* bucket,
		db_table_desc_p *new_tab, unsigned long hashsize)
{
	db_table_desc_p np, next_np, *hp;

	for (np = bucket; np != NULL; np = next_np) {
		next_np = np->next;
		hp = &new_tab[np->hashval % hashsize];
		np->next = *hp;
		*hp = np;
	}
}

/*
 * Return pointer to entry with same hash value and table_name
 * as those supplied.  Returns NULL if not found.
 */
static db_status
enumerate_bucket(db_table_desc* bucket, db_status(*func)(db_table_desc *))
{
	db_table_desc_p np;
	db_status status;

	for (np = bucket; np != NULL; np = np->next) {
		status = (func)(np);
		if (status != DB_SUCCESS)
			return (status);
	}
	return (DB_SUCCESS);
}


/*
 * Return pointer to entry with same hash value and table_name
 * as those supplied.  Returns NULL if not found.
 */
static db_table_desc_p
search_bucket(db_table_desc* bucket, unsigned long hval, char *target)
{
	db_table_desc_p np;

	for (np = bucket; np != NULL; np = np->next) {
		if (np->hashval == hval &&
		    strcmp(np->table_name, target) == 0) {
			break;
		}
	}
	return (np);
}


/*
 * Remove entry with the specified hashvalue and target table name.
 * Returns 'TRUE' if successful, FALSE otherwise.
 * If the entry being removed is at the head of the list, then
 * the head is updated to reflect the removal. The storage for the
 * entry is freed.
 */
static bool_t
remove_from_bucket(db_table_desc_p bucket,
		db_table_desc_p *head, unsigned long hval, char *target)
{
	db_table_desc_p np, dp;

	/* Search for it in the bucket */
	for (dp = np = bucket; np != NULL; np = np->next) {
		if (np->hashval == hval &&
		    strcmp(np->table_name, target) == 0) {
			break;
		} else {
			dp = np;
		}
	}

	if (np == NULL)
		return (FALSE);	// cannot delete if it is not there

	if (dp == np) {
		*head = np->next;	// deleting head of bucket
	} else {
		dp->next = np->next;	// deleting interior link
	}
	delete_table_desc(np);

	return (TRUE);
}


/*
 * Add given entry to the bucket pointed to by 'bucket'.
 * If an entry with the same table_name is found, no addition
 * is done.  The entry is added to the head of the bucket.
 */
static bool_t
add_to_bucket(db_table_desc_p bucket, db_table_desc **head, db_table_desc_p td)
{
	db_table_desc_p curr, prev;
	register char *target_name;
	unsigned long target_hval;
	target_name = td->table_name;
	target_hval = td->hashval;

	/* Search for it in the bucket */
	for (prev = curr = bucket; curr != NULL; curr = curr->next) {
		if (curr->hashval == target_hval &&
		    strcmp(curr->table_name, target_name) == 0) {
			break;
		} else {
			prev = curr;
		}
	}

	if (curr != NULL)
		return (FALSE);  /* duplicates not allowed */

	curr = *head;
	*head = td;
	td->next = curr;
	return (TRUE);
}


/* Print bucket starting with this entry. */
static void
print_bucket(db_table_desc *head)
{
	db_table_desc *np;
	for (np = head; np != NULL; np = np->next) {
		printf("%s: %d\n", np->table_name, np->hashval);
	}
}

static db_status
print_table(db_table_desc *tbl)
{
	if (tbl == NULL)
		return (DB_BADTABLE);
	printf("%s: %d\n", tbl->table_name, tbl->hashval);
	return (DB_SUCCESS);
}


static int hashsizes[] = {		/* hashtable sizes */
	11,
	53,
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

/* Returns the next size to use for the hash table */
static unsigned int
get_next_hashsize(long unsigned oldsize)
{
	long unsigned newsize, n;
	if (oldsize == 0)
		newsize = hashsizes[0];
	else {
		for (n = 0; newsize = hashsizes[n++]; )
			if (oldsize == newsize) {
				newsize = hashsizes[n];	/* get next size */
				break;
			}
		if (newsize == 0)
			newsize = oldsize * 2 + 1;	/* just double */
	}
	return (newsize);
}

/*
 * Grow the current hashtable upto the next size.
 * The contents of the existing hashtable is copied to the new one and
 * relocated according to its hashvalue relative to the new size.
 * Old table is deleted after the relocation.
 */
static void
grow_dictionary(db_dict_desc_p dd)
{
	unsigned int oldsize, i, new_size;
	db_table_desc_p * oldtab, *newtab;

	oldsize = dd->tables.tables_len;
	oldtab = dd->tables.tables_val;

	new_size = get_next_hashsize(oldsize);

	if (new_size > CALLOC_LIMIT) {
		FATAL("db_dictionary::grow: table size exceeds calloc limit",
			DB_MEMORY_LIMIT);
	}

	if ((newtab = (db_table_desc_p*)
		calloc((unsigned int) new_size,
			sizeof (db_table_desc_p))) == NULL) {
		FATAL("db_dictionary::grow: cannot allocate space",
			DB_MEMORY_LIMIT);
	}

	if (oldtab != NULL) {		// must transfer contents of old to new
		for (i = 0; i < oldsize; i++) {
			relocate_bucket(oldtab[i], newtab, new_size);
		}
		delete oldtab;		// delete old hashtable
	}

	dd->tables.tables_val = newtab;
	dd->tables.tables_len = new_size;
}

#define	HASHSHIFT	3
#define	HASHMASK	0x1f

static u_int
get_hashval(char *value)
{
	int i, len;
	u_int hval = 0;

	len = strlen(value);
	for (i = 0; i < len; i++) {
		hval = ((hval<<HASHSHIFT)^hval);
		hval += (value[i] & HASHMASK);
	}

	return (hval);
}

static db_status
enumerate_dictionary(db_dict_desc *dd, db_status (*func) (db_table_desc*))
{
	int i;
	db_table_desc *bucket;
	db_status status;

	if (dd == NULL)
		return (DB_SUCCESS);

	for (i = 0; i < dd->tables.tables_len; i++) {
		bucket = dd->tables.tables_val[i];
		if (bucket) {
			status = enumerate_bucket(bucket, func);
			if (status != DB_SUCCESS)
				return (status);
		}
	}

	return (DB_SUCCESS);
}


/*
 * Look up target table_name in hashtable and return its db_table_desc.
 * Return NULL if not found.
 */
static db_table_desc *
search_dictionary(db_dict_desc *dd, char *target)
{
	register unsigned long hval;
	unsigned long bucket;

	if (target == NULL || dd == NULL || dd->tables.tables_len == 0)
		return (NULL);

	hval = get_hashval(target);
	bucket = hval % dd->tables.tables_len;

	db_table_desc_p fst = dd->tables.tables_val[bucket];

	if (fst != NULL)
		return (search_bucket(fst, hval, target));
	else
		return (NULL);
}

/*
 * Remove the entry with the target table_name from the dictionary.
 * If successful, return DB_SUCCESS; otherwise DB_NOTUNIQUE if target
 * is null; DB_NOTFOUND if entry is not found.
 * If successful, decrement count of number of entries in hash table.
 */
static db_status
remove_from_dictionary(db_dict_desc *dd, char *target)
{
	register unsigned long hval;
	unsigned long bucket;
	register db_table_desc *fst;

	if (target == NULL)
		return (DB_NOTUNIQUE);
	if (dd == NULL || dd->tables.tables_len == 0)
		return (DB_NOTFOUND);
	hval = get_hashval(target);
	bucket = hval % dd->tables.tables_len;
	fst = dd->tables.tables_val[bucket];
	if (fst == NULL)
		return (DB_NOTFOUND);
	if (remove_from_bucket(fst, &dd->tables.tables_val[bucket],
			hval, target)) {
		--(dd->count);
		return (DB_SUCCESS);
	} else
		return (DB_NOTFOUND);
}

/*
 * Add a new db_table_desc to the dictionary.
 * Return DB_NOTUNIQUE, if entry with identical table_name
 * already exists.  If entry is added, return DB_SUCCESS.
 * Increment count of number of entries in index table and grow table
 * if number of entries equals size of table.
 *
 * Inputs: db_dict_desc_p dd	pointer to dictionary to add to.
 *	   db_table_desc *td	pointer to table entry to be added. The
 * 				db_table_desc.next field will be altered
 *				without regard to it's current setting.
 *				This means that if next points to a list of
 *				table entries, they may be either linked into
 *				the dictionary unexpectly or cut off (leaked).
 */
static db_status
add_to_dictionary(db_dict_desc_p dd, db_table_desc *td)
{
	register unsigned long hval;
	char *target;

	if (dd == NULL)
		return (DB_NOTFOUND);

	if (td == NULL)
		return (DB_NOTFOUND);
	target = td->table_name;
	if (target == NULL)
		return (DB_NOTUNIQUE);

	hval = get_hashval(target);

	if (dd->tables.tables_val == NULL)
		grow_dictionary(dd);

	db_table_desc_p fst;
	unsigned long bucket;
	bucket = hval % dd->tables.tables_len;
	fst = dd->tables.tables_val[bucket];
	td->hashval = hval;
	if (fst == NULL)  { /* Empty bucket */
		dd->tables.tables_val[bucket] = td;
	} else if (!add_to_bucket(fst, &dd->tables.tables_val[bucket], td)) {
			return (DB_NOTUNIQUE);
		}

	/* increase hash table size if number of entries equals table size */
	if (++(dd->count) > dd->tables.tables_len)
		grow_dictionary(dd);

	return (DB_SUCCESS);
}

/* ******************* pickling routines for dictionary ******************* */


/* Does the actual writing to/from file specific for db_dict_desc structure. */
static bool_t
transfer_aux(XDR* x, pptr tbl)
{
	return (xdr_db_dict_desc_p(x, (db_dict_desc_p *) tbl));
}

class pickle_dict_desc: public pickle_file {
    public:
	pickle_dict_desc(char *f, pickle_mode m) : pickle_file(f, m) {}

	/* Transfers db_dict_desc structure pointed to by dp to/from file. */
	int transfer(db_dict_desc_p * dp)
		{ return (pickle_file::transfer((pptr) dp, &transfer_aux)); }
};

/* ************************ dictionary methods *************************** */

db_dictionary::db_dictionary()
{
	dictionary = NULL;
	initialized = FALSE;
	filename = NULL;
	tmpfilename = NULL;
	logfilename = NULL;
	logfile = NULL;
	logfile_opened = FALSE;
	changed = FALSE;
}

/*
 * This routine clones an entire hash bucket chain. If you clone a
 * data dictionary entry with the ->next pointer set, you will get a
 * clone of that entry, as well as the entire linked list. This can cause
 * pain if you then pass the cloned bucket to routines such as
 * add_to_dictionary(), which do not expect nor handle dictionary hash
 * entries with the ->next pointer set. You might either get duplicate
 * entires or lose entries. If you wish to clone the entire bucket chain
 * and add it to a new dictionary, loop through the db_table_desc->next list
 * and call add_to_dictionary() for each item.
 */
int
db_dictionary::db_clone_bucket(db_table_desc *bucket, db_table_desc **clone)
{
	u_long		size;
	XDR		xdrs;
	char		*bufin = NULL;

	db_func use_this = xdr_db_table_desc;
	size = xdr_sizeof((xdrproc_t) use_this, (void *) bucket);
	bufin = (char *) calloc(1, (size_t) size * sizeof (char));
	if (!bufin)
		FATAL("db_dictionary::insert_modified_table: out of memory",
			DB_MEMORY_LIMIT);
	xdrmem_create(&xdrs, bufin, (size_t) size, XDR_ENCODE);
	if (!xdr_db_table_desc(&xdrs, bucket)) {
		free(bufin);
		xdr_destroy(&xdrs);
		FATAL(
		"db_dictionary::insert_modified_table: xdr encode failed.",
		DB_MEMORY_LIMIT);
	}
	*clone = (db_table_desc *) calloc(1, (size_t) size * sizeof (char));
	if (!*clone) {
		xdr_destroy(&xdrs);
		free(bufin);
		FATAL("db_dictionary::insert_modified_table: out of memory",
			DB_MEMORY_LIMIT);
	}

	xdrmem_create(&xdrs, bufin, (size_t) size, XDR_DECODE);
	if (!xdr_db_table_desc(&xdrs, *clone)) {
		free(bufin);
		free(*clone);
		xdr_destroy(&xdrs);
		FATAL(
		"db_dictionary::insert_modified_table: xdr encode failed.",
		DB_MEMORY_LIMIT);
	}
	free(bufin);
	xdr_destroy(&xdrs);
	return (1);
}


int
db_dictionary::change_table_name(db_table_desc *clone, char *tok, char *repl)
{
	char 	*newname;
	char	*loc_end, *loc_beg;

	while (clone) {
		/*
		 * Special case for a tok="". This is used for the
		 * nisrestore(1M), when restoring a replica in another
		 * domain. This routine is used to change the datafile 
		 * names in the data.dict (see bugid #4031273). This will not
		 * effect massage_dict(), since it never generates an empty
		 * string for tok.
		 */
		if (strlen(tok) == 0) {
			strcat(clone->table_name, repl);
			clone = clone->next;
			continue;
		}
		newname = (char *) calloc(1, sizeof (char) *
				strlen(clone->table_name) +
				strlen(repl) - strlen(tok) + 1);
		if (!newname)
		    FATAL("db_dictionary::change_table_name: out of memory.",
				DB_MEMORY_LIMIT);
		if (loc_beg = strstr(clone->table_name, tok)) {
			loc_end = loc_beg + strlen(tok);
			int s = loc_beg - clone->table_name;
			memcpy(newname, clone->table_name, s);
			strcat(newname + s, repl);
			strcat(newname, loc_end);
			free(clone->table_name);
			clone->table_name = newname;
		} else {
			free(newname);
		}
		clone = clone->next;
	}
	return (1);
}


/*
 * A function to initialize the temporary dictionary from the real
 * dictionary.
 */
db_dictionary::inittemp(char *dictname, db_dictionary& curdict)
{
	int status;
	db_table_desc_p	*newtab;

	db_shutdown();

	pickle_dict_desc f(dictname, PICKLE_READ);
	filename = strdup(dictname);
	if (filename == NULL)
		FATAL("db_dictionary::inittemp: could not allocate space",
			DB_MEMORY_LIMIT);
	int len = strlen(filename);
	tmpfilename = new char[len+5];
	if (tmpfilename == NULL) {
		delete filename;
		FATAL("db_dictionary::inittemp: could not allocate space",
			DB_MEMORY_LIMIT);
	}
	logfilename = new char[len+5];
	if (logfilename == NULL) {
		delete filename;
		delete tmpfilename;
		FATAL("db_dictionary::inittemp: cannot allocate space",
			DB_MEMORY_LIMIT);
	}

	sprintf(tmpfilename, "%s.tmp", filename);
	sprintf(logfilename, "%s.log", filename);
	unlink(tmpfilename);  /* get rid of partial checkpoints */
	dictionary = NULL;

	if ((status = f.transfer(&dictionary)) < 0) {
		initialized = FALSE;
	} else if (status == 1) { /* no dictionary exists, create one */
		dictionary = new db_dict_desc;
		if (dictionary == NULL)
			FATAL(
			"db_dictionary::inittemp: could not allocate space",
			DB_MEMORY_LIMIT);
		dictionary->tables.tables_len =
				curdict.dictionary->tables.tables_len;
		if ((newtab = (db_table_desc_p *) calloc(
			(unsigned int) dictionary->tables.tables_len,
			sizeof (db_table_desc_p))) == NULL) {
			FATAL("db_dictionary::inittemp: cannot allocate space",
			DB_MEMORY_LIMIT);
		}
		dictionary->tables.tables_val = newtab;
		dictionary->count = 0;
		dictionary->impl_vers = curdict.dictionary->impl_vers;
		initialized = TRUE;
	} else	/* dictionary loaded successfully */
		initialized = TRUE;

	if (initialized == TRUE) {
		changed = FALSE;
		reset_log();
	}

	return (initialized);
}


/*
 * This method replaces the token string specified with the replacment
 * string specified. It assumes that at least one and only one instance of
 * the token exists. It is the responsibility of the caller to ensure that
 * the above assumption stays valid.
 */
db_status
db_dictionary::massage_dict(char *newdictname, char *tok, char *repl)
{
	int		retval;
	u_int		i, tbl_count;
	db_status	status;
	db_table_desc 	*bucket, *np, *clone, *next_np;
	char		tail[NIS_MAXNAMELEN];
	db_dictionary	*tmpptr;

	if (dictionary == NULL)
		FATAL(
		"db_dictionary::massage_dict: uninitialized dictionary file.",
		DB_INTERNAL_ERROR);

	if ((tbl_count = dictionary->count) == 0)
		return (DB_SUCCESS);

	/* First checkpoint */
	if ((status = checkpoint()) != DB_SUCCESS)
		return (status);

#ifdef DEBUG
	enumerate_dictionary(dictionary, &print_table);
#endif

	/* Initialize the free dictionary so that we can start populating it */
	FreeDictionary->inittemp(newdictname, *this);

	for (i = 0; i < dictionary->tables.tables_len; i++) {
		bucket = dictionary->tables.tables_val[i];
		if (bucket) {
			np = bucket;
			while (np != NULL) {
				next_np = np->next;
				retval = db_clone_bucket(np, &clone);
				if (retval == -1)
					return (DB_INTERNAL_ERROR);
				if (change_table_name(clone, tok, repl) == -1) {
					delete_table_desc(clone);
					return (DB_INTERNAL_ERROR);
				}
				/*
				 * We know we don't have a log file, so we will
				 * just add to the in-memory database and dump
				 * all of it once we are done.
				 */
				status = add_to_dictionary
						(FreeDictionary->dictionary,
						clone);
				if (status != DB_SUCCESS) {
					delete_table_desc(clone);
					return (DB_INTERNAL_ERROR);
				}
				status = remove_from_dictionary(dictionary,
								np->table_name);
				if (status != DB_SUCCESS) {
					delete_table_desc(clone);
					return (DB_INTERNAL_ERROR);
				}
				np = next_np;
			}
		}
	}

	if (FreeDictionary->dump() != DB_SUCCESS)
		FATAL(
		"db_dictionary::massage_dict: Unable to dump new dictionary.",
		DB_INTERNAL_ERROR);

	/*
	 * Now, shutdown the inuse dictionary and update the FreeDictionary
	 * and InUseDictionary pointers as well. Also, delete the old dictionary
	 * file.
	 */
	unlink(filename); /* There shouldn't be a tmpfile or logfile */
	db_shutdown();
	tmpptr = InUseDictionary;
	InUseDictionary = FreeDictionary;
	FreeDictionary = tmpptr;
	return (DB_SUCCESS);
}


db_status
db_dictionary::merge_dict(db_dictionary& tempdict, char *tok, char *repl)
{

	db_status	dbstat = DB_SUCCESS;

	db_table_desc	*tbl = NULL, *clone = NULL, *next_td = NULL;
	int		retval, i;

	for (i = 0; i < tempdict.dictionary->tables.tables_len; ++i) {
		tbl = tempdict.dictionary->tables.tables_val[i];
		if (!tbl)
			continue;
		retval = db_clone_bucket(tbl, &clone);
		if (retval == -1)
			return (DB_INTERNAL_ERROR);
		while (clone) {
			next_td = clone->next;
			clone->next = NULL;
			if ((tok) &&
				(change_table_name(clone, tok, repl) == -1)) {
				delete_table_desc(clone);
				if (next_td)
					delete_table_desc(next_td);
				return (DB_INTERNAL_ERROR);
			}
			
			dbstat = add_to_dictionary(dictionary, clone);
			if (dbstat == DB_NOTUNIQUE) {
				/* Overide */
				dbstat = remove_from_dictionary(dictionary,
							clone->table_name);
				if (dbstat != DB_SUCCESS) {
					return (dbstat);
				}
				dbstat = add_to_dictionary(dictionary,
								clone);
			} else {
				if (dbstat != DB_SUCCESS) {
					return (dbstat);
				}
			}
			clone = next_td;
		}
	}
/*
 * If we were successful in merging the dictionaries, then mark the
 * dictionary changed, so that it will be properly checkpointed and
 * dumped to disk.
 */
	if (dbstat == DB_SUCCESS)
		changed = TRUE;
	return (dbstat);
}

int
db_dictionary::copyfile(char *infile, char *outfile)
{
	db_table_desc	*tbl = NULL;
	db	*dbase;

	dbase  = find_table(infile, &tbl);
	if (dbase == NULL)
		return (DB_NOTFOUND);
	return (tbl->database->dump(outfile) ? DB_SUCCESS:
				DB_INTERNAL_ERROR);
}


bool_t
db_dictionary::extract_entries(db_dictionary& tempdict, char **fs, int fscnt)
{
	int		i, retval;
	db_table_desc	*tbl, *clone;
	db_table_desc	tbl_ent;
	db_status	dbstat;

	for (i = 0; i < fscnt; ++i) {
		tbl = find_table_desc(fs[i]);
		if (!tbl) {
			syslog(LOG_DEBUG,
				"extract_entries: no dictionary entry for %s",
				fs[i]);
			return (FALSE);
		} else {
			tbl_ent.table_name = tbl->table_name;
			tbl_ent.hashval = tbl->hashval;
			tbl_ent.scheme = tbl->scheme;
			tbl_ent.database = tbl->database;
			tbl_ent.next = NULL;
		}
		retval = db_clone_bucket(&tbl_ent, &clone);
		if (retval == -1) {
			syslog(LOG_DEBUG,
			"extract_entries: unable to clone entry for %s",
			fs[i]);
			return (FALSE);
		}
		dbstat = add_to_dictionary(tempdict.dictionary, clone);
		if (dbstat != DB_SUCCESS) {
			delete_table_desc(clone);
			return (FALSE);
		}
	}
	if (tempdict.dump() != DB_SUCCESS)
		return (FALSE);
	return (TRUE);
}


/*
 * Initialize dictionary from contents in 'file'.
 * If there is already information in this dictionary, it is removed.
 * Therefore, regardless of whether the load from the file succeeds,
 * the contents of this dictionary will be altered.  Returns
 * whether table has been initialized successfully.
 */
bool_t
db_dictionary::init(char *file)
{
	int status;

	db_shutdown();

	pickle_dict_desc f(file, PICKLE_READ);
	filename = strdup(file);
	if (filename == NULL)
		FATAL("db_dictionary::init: could not allocate space",
			DB_MEMORY_LIMIT);
	int len = strlen(filename);
	tmpfilename = new char[len+5];
	if (tmpfilename == NULL) {
		delete filename;
		FATAL("db_dictionary::init: could not allocate space",
			DB_MEMORY_LIMIT);
	}
	logfilename = new char[len+5];
	if (logfilename == NULL) {
		delete filename;
		delete tmpfilename;
		FATAL("db_dictionary::init: cannot allocate space",
			DB_MEMORY_LIMIT);
	}

	sprintf(tmpfilename, "%s.tmp", filename);
	sprintf(logfilename, "%s.log", filename);
	unlink(tmpfilename);  /* get rid of partial checkpoints */
	dictionary = NULL;

	/* load dictionary */
	if ((status = f.transfer(&dictionary)) < 0) {
	    initialized = FALSE;
	} else if (status == 1) {  /* no dictionary exists, create one */
	    dictionary = new db_dict_desc;
	    if (dictionary == NULL)
		FATAL("db_dictionary::init: could not allocate space",
			DB_MEMORY_LIMIT);
	    dictionary->tables.tables_len = 0;
	    dictionary->tables.tables_val = NULL;
	    dictionary->count = 0;
	    dictionary->impl_vers = DB_CURRENT_VERSION;
	    initialized = TRUE;
	} else  /* dictionary loaded successfully */
	    initialized = TRUE;

	if (initialized == TRUE) {
	    int num_changes = 0;
	    changed = FALSE;
	    reset_log();
	    if ((num_changes = incorporate_log(logfilename)) < 0)
		syslog(LOG_ERR,
			"incorporation of dictionary logfile '%s' failed",
			logfilename);
	    changed = (num_changes > 0);
	}

	return (initialized);
}

/*
 * Execute log entry 'j' on the dictionary identified by 'dict' if the
 * version of j is later than that of the dictionary.  If 'j' is executed,
 * 'count' is incremented and the dictionary's verison is updated to
 * that of 'j'.
 * Returns TRUE always for valid log entries; FALSE otherwise.
 */
static bool_t
apply_log_entry(db_dictlog_entry *j, char *dictchar, int *count)
{
	db_dictionary *dict = (db_dictionary*) dictchar;
	if (db_update_version.earlier_than(j->get_version())) {
		++ *count;
#ifdef DEBUG
		j->print();
#endif DEBUG
		switch (j->get_action()) {
		case DB_ADD_TABLE:
			dict->add_table_aux(j->get_table_name(),
				j->get_table_object(), INMEMORY_ONLY);
			// ignore status
			break;

		case DB_REMOVE_TABLE:
			dict->delete_table_aux(j->get_table_name(),
							INMEMORY_ONLY);
			// ignore status
			break;

		default:
			WARNING("db::apply_log_entry: unknown action_type");
			return (FALSE);
		}
		db_update_version.assign(j->get_version());
	}

	return (TRUE);
}

int
db_dictionary::incorporate_log(char *file_name)
{
	db_dictlog f(file_name, PICKLE_READ);
	return (f.execute_on_log(&(apply_log_entry), (char *) this));
}


/* Frees memory of filename and tables.  Has no effect on disk storage. */
db_status
db_dictionary::db_shutdown()
{
	if (!initialized)
		return (DB_SUCCESS); /* DB_NOTFOUND? */

	if (filename) {
		delete filename;
		filename = NULL;
	}
	if (tmpfilename) {
		delete tmpfilename;
		tmpfilename = NULL;
	}
	if (logfilename) {
		delete logfilename;
		logfilename = NULL;
	}
	if (dictionary) {
		delete_dictionary(dictionary);
		dictionary = NULL;
	}
	initialized = FALSE;
	changed = FALSE;
	reset_log();

	return (DB_SUCCESS);
}

/*
 * Dump contents of this dictionary (minus the database representations)
 * to its file. Returns 0 if operation succeeds, -1 otherwise.
 */
int
db_dictionary::dump()
{
	int status;

	if (!initialized)
		return (-1);

	unlink(tmpfilename);  /* get rid of partial dumps */
	pickle_dict_desc f(tmpfilename, PICKLE_WRITE);

	status = f.transfer(&dictionary); 	/* dump table descs */
	if (status != 0) {
		WARNING("db_dictionary::dump: could not write out dictionary");
	} else if (rename(tmpfilename, filename) < 0) {
		WARNING_M("db_dictionary::dump: could not rename temp file: ");
		status = -1;
	}

	return (status);
}

/*
 * Write out in-memory copy of dictionary to file.
 * 1.  Update major version.
 * 2.  Dump contents to temporary file.
 * 3.  Rename temporary file to real dictionary file.
 * 4.  Remove log file.
 * A checkpoint is done only if it has changed since the previous checkpoint.
 * Returns DB_SUCCESS if checkpoint was successful; error code otherwise
 */
db_status
db_dictionary::checkpoint()
{
	if (changed == FALSE)
		return (DB_SUCCESS);

	vers *oldv = new vers(db_update_version);	// copy
	vers * newv = db_update_version.nextmajor();	// get next version
	db_update_version.assign(newv);			// update version
	delete newv;

	if (dump() != 0) {
		WARNING_M(
		    "db_dictionary::checkpoint: could not dump dictionary: ");
		db_update_version.assign(oldv);  // rollback
		delete oldv;
		return (DB_INTERNAL_ERROR);
	}
	unlink(logfilename);	/* should do atomic rename and log delete */
	reset_log();		/* should check for what? */
	delete oldv;
	changed = FALSE;
	return (DB_SUCCESS);
}

/* close existing logfile and delete its structure */
db_dictionary::reset_log()
{
	/* try to close old log file */
	/* doesnot matter since we do synchronous writes only */
	if (logfile != NULL) {
		if (logfile_opened == TRUE) {
			if (logfile->close() < 0) {
				WARNING_M(
			"db_dictionary::reset_log: could not close log file: ");
			}
		}
		delete logfile;
		logfile = NULL;
	}
	logfile_opened = FALSE;
	return (0);
}

/* close existing logfile, but leave its structure if exists */
int
db_dictionary::close_log()
{
	if (logfile != NULL && logfile_opened == TRUE) {
		logfile->close();
	}
	logfile_opened = FALSE;
	return (0);
}

/* open logfile, creating its structure if it does not exist */
int
db_dictionary::open_log()
{
	if (logfile == NULL) {
		if ((logfile = new db_dictlog(logfilename, PICKLE_APPEND))
								== NULL)
			FATAL("db_dictionary::reset_log: cannot allocate space",
				DB_MEMORY_LIMIT);
	}

	if (logfile_opened == TRUE)
		return (0);

	if ((logfile->open()) == NULL) {
		WARNING_M("db_dictionary::open_log: could not open log file: ");
		delete logfile;
		logfile = NULL;
		return (-1);
	}

	logfile_opened = TRUE;
	return (0);
}

/*
 * closes any open log files for all tables in dictionary or 'tab'.
 * "tab" is an optional argument.
 */
static int close_standby_list();

db_status
db_dictionary::db_standby(char *tab)
{
	db_table_desc *tbl;

	if (!initialized)
		return (DB_BADDICTIONARY);

	if (tab == NULL) {
	    close_log();  // close dictionary log
	    close_standby_list();
	    return (DB_SUCCESS);
	}

	if ((tbl = find_table_desc(tab)) == NULL)
	    return (DB_BADTABLE);

	if (tbl->database != NULL)
	    tbl->database->close_log();
	return (DB_SUCCESS);
}

/*
 * Returns db_table_desc of table name 'tab'.  'prev', if supplied,
 * is set to the entry located ahead of 'tab's entry in the dictionary.
 */
db_table_desc*
db_dictionary::find_table_desc(char *tab)
{
	if (!initialized)
		return (NULL);

	return (search_dictionary(dictionary, tab));
}

/*
 * Return database structure of table named by 'tab'.
 * If 'where' is set, set it to the table_desc of 'tab.'
 * If the database is loaded in from stable store if it has not been loaded.
 * If it cannot be loaded, it is initialized using the scheme stored in
 * the table_desc.  NULL is returned if the initialization fails.
 */
db *
db_dictionary::find_table(char *tab, db_table_desc **where)
{
	if (!initialized)
		return (NULL);

	db_table_desc* tbl = find_table_desc(tab);
	if (where) *where = tbl;
	db *dbase = NULL;

	if (tbl == NULL)
		return (NULL);		// not found

	if (tbl->database != NULL)
		return (tbl->database);  // return handle

	// need to load in/init database
	dbase = tbl->database = new db(tab);

	if (dbase == NULL) {
		FATAL("db_dictionary::find_table: could not allocate space",
			DB_MEMORY_LIMIT);
	}

	if (dbase->load())			// try to load in database
		return (dbase);

	WARNING("db_dictionary::find_table: could not load database");
	delete dbase;
	tbl->database = NULL;
	return (NULL);
}


/* Log action to be taken on the  dictionary and update db_update_version. */

db_status
db_dictionary::log_action(int action, char *tab, table_obj *tobj)
{
	vers *newv = db_update_version.nextminor();
	db_dictlog_entry le(action, newv, tab, tobj);

	if (open_log() < 0) {
		delete newv;
		return (DB_STORAGE_LIMIT);
	}

	if (logfile->append(&le) < 0) {
		WARNING_M("db::log_action: could not add log entry: ");
		close_log();
		delete newv;
		return (DB_STORAGE_LIMIT);
	}

	db_update_version.assign(newv);
	delete newv;
	changed = TRUE;

	return (DB_SUCCESS);
}

// For a complete 'delete' operation, we want the following behaviour:
// 1. If there is an entry in the log, the physical table exists and is
//    stable.
// 2. If there is no entry in the log, the physical table may or may not
//    exist.

db_status
db_dictionary::delete_table_aux(char *tab, int mode)
{
	if (!initialized)
		return (DB_BADDICTIONARY);

	db_table_desc *tbl;
	if ((tbl = find_table_desc(tab)) == NULL)  // table not found
		return (DB_NOTFOUND);

	if (mode != INMEMORY_ONLY) {
		int need_free = 0;

		// Update log.
		db_status status = log_action(DB_REMOVE_TABLE, tab);
		if (status != DB_SUCCESS)
			return (status);

		// Remove physical structures
		db *dbase = tbl->database;
		if (dbase == NULL) {	// need to get desc to access files
			dbase = new db(tab);
			need_free = 1;
		}
		if (dbase == NULL) {
			WARNING(
		"db_dictionary::delete_table: could not create db structure");
			return (DB_MEMORY_LIMIT);
		}
		dbase->remove_files();	// remove physical files
		if (need_free)
			delete dbase;
	}

	// Remove in-memory structures
	return (remove_from_dictionary(dictionary, tab));
}

/*
 * Delete table with given name 'tab' from dictionary.
 * Returns error code if table does not exist or if dictionary has not been
 * initialized.   Dictionary is updated to stable store if deletion is
 * successful.  Fatal error occurs if dictionary cannot be saved.
 * Returns DB_SUCCESS if dictionary has been updated successfully.
 * Note that the files associated with the table are also removed.
 */
db_status
db_dictionary::delete_table(char *tab)
{
	return (delete_table_aux(tab, !INMEMORY_ONLY));
}

// For a complete 'add' operation, we want the following behaviour:
// 1. If there is an entry in the log, then the physical table exists and
//    has been initialized properly.
// 2. If there is no entry in the log, the physical table may or may not
//    exist.  In this case, we don't really care because we cannot get at
//    it.  The next time we add a table with the same name to the dictionary,
//    it will be initialized properly.
// This mode is used when the table is first created.
//
// For an INMEMORY_ONLY operation, only the internal structure is created and
// updated.  This mode is used when the database gets loaded and the internal
// dictionary gets updated from the log entries.

db_status
db_dictionary::add_table_aux(char *tab, table_obj* tobj, int mode)
{
	if (!initialized)
		return (DB_BADDICTIONARY);

	if (find_table_desc(tab) != NULL)
		return (DB_NOTUNIQUE);		// table already exists

	// create data structures for table
	db_table_desc *new_table = 0;
	db_status status = create_table_desc(tab, tobj, &new_table);

	if (status != DB_SUCCESS)
		return (status);

	if (mode != INMEMORY_ONLY) {
		// create physical structures for table
		new_table->database = new db(tab);
		if (new_table->database == NULL) {
			delete_table_desc(new_table);
			FATAL(
		    "db_dictionary::add_table: could not allocate space for db",
			DB_MEMORY_LIMIT);
		}
		if (new_table->database->init(new_table->scheme) == 0) {
			WARNING(
	"db_dictionary::add_table: could not initialize database from scheme");
			new_table->database->remove_files();
			delete_table_desc(new_table);
			return (DB_STORAGE_LIMIT);
		}

		// update 'external' copy of dictionary
		status = log_action(DB_ADD_TABLE, tab, tobj);

		if (status != DB_SUCCESS) {
			new_table->database->remove_files();
			delete_table_desc(new_table);
			return (status);
		}
	}

	// finally, update in-memory copy of dictionary
	return (add_to_dictionary(dictionary, new_table));
}

/*
 * Add table with given name 'tab' and description 'zdesc' to dictionary.
 * Returns errror code if table already exists, or if no memory can be found
 * to store the descriptor, or if dictionary has not been intialized.
 * Dictionary is updated to stable store if addition is successful.
 * Fatal error occurs if dictionary cannot be saved.
 * Returns DB_SUCCESS if dictionary has been updated successfully.
*/
db_status
db_dictionary::add_table(char *tab, table_obj* tobj)
{
	return (add_table_aux(tab, tobj, !INMEMORY_ONLY));
}

/*
 * Translate given NIS attribute list to a db_query structure.
 * Return FALSE if dictionary has not been initialized, or
 * table does not have a scheme (which should be a fatal error?).
 */
db_query*
db_dictionary::translate_to_query(db_table_desc* tbl, int numattrs,
				nis_attr* attrlist)
{
	if (!initialized ||
		tbl->scheme == NULL || numattrs == 0 || attrlist == NULL)
		return (NULL);

	db_query *q = new db_query(tbl->scheme, numattrs, attrlist);
	if (q == NULL) {
		FATAL("db_dictionary::translate: could not allocate space",
			DB_MEMORY_LIMIT);
	}

	if (q->size() == 0) {
		delete q;
		return (NULL);
	}
	return (q);
}

static db_table_names gt_answer;
static int gt_posn;

static db_status
get_table_name(db_table_desc* tbl)
{
	if (tbl)
		return (DB_BADTABLE);

	if (gt_posn < gt_answer.db_table_names_len)
		gt_answer.db_table_names_val[gt_posn++] =
			strdup(tbl->table_name);
	else
		return (DB_BADTABLE);

	return (DB_SUCCESS);
}


/*
 * Return the names of tables in this dictionary.
 * XXX This routine is used only for testing only;
 *	if to be used for real, need to free memory sensibly, or
 *	caller of get_table_names should have freed them.
 */
db_table_names*
db_dictionary::get_table_names()
{
	gt_answer.db_table_names_len = dictionary->count;
	gt_answer.db_table_names_val = new db_table_namep[dictionary->count];
	gt_posn = 0;
	if ((gt_answer.db_table_names_val) == NULL) {
		FATAL(
	"db_dictionary::get_table_names: could not allocate space for names",
		DB_MEMORY_LIMIT);
	}

	enumerate_dictionary(dictionary, &get_table_name);
	return (&gt_answer);
}

static db_status
db_checkpoint_aux(db_table_desc *current)
{
	db *dbase;
	int status;

	if (current == NULL)
		return (DB_BADTABLE);

	if (current->database == NULL) {  /* need to load it in */
		dbase = new db(current->table_name);
		if (dbase == NULL) {
			FATAL(
		    "db_dictionary::db_checkpoint: could not allocate space",
			DB_MEMORY_LIMIT);
		}
		if (dbase->load() == 0) {
			syslog(LOG_ERR,
			"db_dictionary::db_checkpoint: could not load table %s",
							current->table_name);
			delete dbase;
			return (DB_BADTABLE);
		}
		status = dbase->checkpoint();
		delete dbase;  // unload
	} else
	    status = current->database->checkpoint();

	if (status == 0)
		return (DB_STORAGE_LIMIT);
	return (DB_SUCCESS);
}

/* Like db_checkpoint_aux except only stops on LIMIT errors */
static db_status
db_checkpoint_aux_cont(db_table_desc *current)
{
	db_status status = db_checkpoint_aux(current);

	if (status == DB_STORAGE_LIMIT || status == DB_MEMORY_LIMIT)
		return (status);
	else
		return (DB_SUCCESS);
}

db_status
db_dictionary::db_checkpoint(char *tab)
{
	db_table_desc *tbl;

	if (!initialized)
		return (DB_BADDICTIONARY);

	checkpoint();	// checkpoint dictionary first

	if (tab == NULL) {
	    return (enumerate_dictionary(dictionary, &db_checkpoint_aux_cont));
	}

	if ((tbl = find_table_desc(tab)) == NULL)
	    return (DB_BADTABLE);

	return (db_checkpoint_aux(tbl));
}

/* *********************** db_standby **************************** */
/* Deal with list of tables that need to be 'closed' */

/* Allows a maximum of databases to be opened for updates */

/* XXX Note that the following functions needs to be locked for MT XXX */


#define	MAX_OPENED_DBS 12
static db* db_standby_list[MAX_OPENED_DBS];
static db_standby_count = 0;

/* Returns 1 if some databases were closed; 0 otherwise. */
static int
close_standby_list()
{
	db* database;
	int i;

	if (db_standby_count == 0)
		return (0);

	for (i = 0; i < MAX_OPENED_DBS && db_standby_count; i++) {
		if ((database = db_standby_list[i])) {
			database->close_log(1);
			db_standby_list[i] = (db*)NULL;
			--db_standby_count;
		}
	}

	/* db_standby_count should be 0 */
	return (1);
}

/*
 * Add given database to list of databases that have been opened for updates.
 * If size of list exceeds maximum, close opened databases first.
 */

int
add_to_standby_list(db* database)
{
	int i;

	if (db_standby_count >= MAX_OPENED_DBS)
		close_standby_list();

	for (i = 0; i < MAX_OPENED_DBS; i++) {
		if (db_standby_list[i] == (db*)NULL) {
			db_standby_list[i] = database;
			++db_standby_count;
			return (1);
		}
	}
	return (0);
}

int
remove_from_standby_list(db* database)
{
	int i;

	for (i = 0; i < MAX_OPENED_DBS; i++) {
		if ((database == db_standby_list[i])) {
			db_standby_list[i] = (db*)NULL;
			--db_standby_count;
			return (1);
		}
	}
	return (0);
}
