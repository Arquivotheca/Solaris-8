/*
 *	nis_db.cc
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nis_db.cc	1.23	99/06/03 SMI"

#include <syslog.h>
#include "db_headers.h"
#include "db_entry.h"
#include "db.h"
#include "db_dictionary.h"
#include "db_pickle.h"

db_dictionary	curdict;
db_dictionary	tempdict; /* a temporary one */

db_dictionary *InUseDictionary = &curdict;
db_dictionary *FreeDictionary = &tempdict;
jmp_buf dbenv;

/*
 * Free resources associated with a db_result structure
 */
void
db_free_result(db_result *dr)
{
	int	i;

	if (dr == 0)
		return;

	/* Can't have valid objects */
	if (dr->status != DB_SUCCESS) {
		free(dr);
		return;
	}

	for (i = 0; i < dr->objects.objects_len; i++)
		free_entry(dr->objects.objects_val[i]);
	free(dr->objects.objects_val);
	free(dr);
}


/* Return an empty db_result structure with its status field set to 's'. */
db_result*
empty_result(db_status s)
{
	db_result * res = new db_result;
	if (res != NULL)  {
		res->status = s;
		res->nextinfo.db_next_desc_len = 0;
		res->nextinfo.db_next_desc_val = NULL;
		res->objects.objects_len = 0;
		res->objects.objects_val = NULL;
	} else {
		WARNING("nis_db::empty_result: cannot allocate space");
	}
	return (res);
}

static db_result*
set_result(db_result* res, db_status s)
{
	if (res != NULL)  {
		res->status = s;
	}
	return (res);
}

extern "C" {

bool_t
db_in_dict_file(char *name)
{
	return ((bool_t) InUseDictionary->find_table_desc(name));

}

char
*db_perror(db_status dbstat)
{
	char	*str = NULL;

	switch (dbstat) {
		case DB_SUCCESS:
			str = "Success";
			break;
		case DB_NOTFOUND:
			str = "Not Found";
			break;
		case DB_BADTABLE:
			str = "Bad Table";
			break;
		case DB_BADQUERY:
			str = "Bad Query";
			break;
		case DB_BADOBJECT:
			str = "Bad Object";
			break;
		case DB_MEMORY_LIMIT:
			str = "Memory limit exceeded";
			break;
		case DB_STORAGE_LIMIT:
			str = "Database storage limit exceeded";
			break;
		case DB_INTERNAL_ERROR:
			str = "Database internal error";
			break;
		case DB_SYNC_FAILED:
			str = "Sync of log file failed";
			break;
		default:
			str = "Unknown Error";
			break;
	}
	return (str);
}

bool_t
db_extract_dict_entries(char *newdict, char **fs, int fscnt)
{
	/*
	 * Use the "FreeDictionary" ptr for the backup
	 * dictionary.
	 */
	if (setjmp(dbenv) == 0) {
		if (!FreeDictionary->inittemp(newdict, *InUseDictionary))
			return (FALSE);
		return (InUseDictionary->extract_entries (*FreeDictionary,
			fs, fscnt));
	} else
		return (FALSE);
}

bool_t
db_copy_file(char *infile, char *outfile)
{
	return (InUseDictionary->copyfile(infile, outfile));

}


/*
 * The tok and repl parameters will allow us to merge two dictionaries
 * that reference tables from different domains (master/replica in live
 * in different domains). If set to NULL, then the dictionary merge is
 * done as normal (no name changing).
 */
db_status
db_begin_merge_dict(char *newdict, char *tok, char *repl)
{
	db_status dbstat;

	/*
	 * It is assumed that InUseDictionary has already been initialized.
	 */
	if (setjmp(dbenv) == 0) {
		dbstat = InUseDictionary->checkpoint();
		if (dbstat != DB_SUCCESS)
			return (dbstat);

		/*
		 * Use the "FreeDictionary" ptr for the backup
		 * dictionary.
		 */
		if (!FreeDictionary->init(newdict))
			return (DB_INTERNAL_ERROR);

		return (InUseDictionary->merge_dict(*FreeDictionary,
			tok, repl));
	} else
		return (DB_INTERNAL_ERROR);
}



db_status
db_end_merge_dict()
{
	db_status	dbstat;

	dbstat = InUseDictionary->checkpoint();
	if (dbstat != DB_SUCCESS) {
		return (dbstat);
	}
	dbstat = InUseDictionary->db_shutdown();
	if (dbstat != DB_SUCCESS) {
		return (dbstat);
	}
	dbstat = FreeDictionary->db_shutdown();
	if (dbstat != DB_SUCCESS) {
		return (dbstat);
	}
	return (dbstat);
}



db_status
db_abort_merge_dict()
{
	db_status	dbstat;

	dbstat = InUseDictionary->db_shutdown();
	if (dbstat != DB_SUCCESS)
		return (dbstat);
	dbstat = FreeDictionary->db_shutdown();
	if (dbstat != DB_SUCCESS)
		return (dbstat);
}


/*
 * Initialize system (dictionary) using file 'filename'.  If system cannot
 * be read from file, it is initialized to be empty. Returns TRUE if
 * initialization succeeds, FALSE otherwise.
 * This function must be called before any other.
*/
bool_t
db_initialize(char * filename)
{
	if (setjmp(dbenv) == 0)
		return (InUseDictionary->init(filename));
	else
		return (FALSE);
}


/*
 * Massage the dictionary file by replacing the specified token with the
 * the replacement string. This function is needed to provide backwards
 * compatibility for providing a transportable dictionary file. The idea
 * is that rpc.nisd will call this function when it wants to change the
 * /var/nis/<hostname> strings with something like /var/nis/data.
 *
 */
db_status
db_massage_dict(char *newdictname, char *tok, char *repl)
{
	if (setjmp(dbenv) == 0)
		return (InUseDictionary->massage_dict(newdictname, tok, repl));
	else
		return (DB_INTERNAL_ERROR);
}



/*
 * Create new table using given table name and table descriptor.
 * Returns DB_SUCCESS if successful; appropriate error code otherwise.
*/
db_status
db_create_table(char * table_name, table_obj * table_desc)
{
	int jmp_status;
	if ((jmp_status = setjmp(dbenv)) == 0)
	    return (InUseDictionary->add_table(table_name, table_desc));
	else /* error */
	    return (DB_INTERNAL_ERROR);
}

/*
 * Destroys table named by 'table_name.'  Returns DB_SUCCESS if successful,
 * error code otherwise.  Note that currently, the removed table is no
 * longer accessible from this interface and all files associated with it
 * are removed from stable storage.
*/
db_status
db_destroy_table(char * table_name)
{
	int jmp_status;
	if ((jmp_status = setjmp(dbenv)) == 0)
	    return (InUseDictionary->delete_table(table_name));
	else /* error */
	    return (DB_INTERNAL_ERROR);
}


/*
* Return a copy of the first entry in the specified table, that satisfies
* the given attributes.  The returned structure 'db_result' contains the status,
* the  copy of the object, and a 'db_next_desc' to be used for the 'next'
* operation.
 */
db_result *
db_first_entry(char * table_name, int numattrs, nis_attr * attrname)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc * tbl = NULL;
		db * dbase = InUseDictionary->find_table(table_name, &tbl);
		if (tbl == NULL || dbase == NULL)
			return (set_result(safety, DB_BADTABLE));
		else {
			db_result * res = NULL;
			db_query *query = NULL;

			if (numattrs != 0) {
				query = InUseDictionary->translate_to_query(tbl,
						numattrs, attrname);
				if (query == NULL)
					return (set_result(safety,
							DB_BADQUERY));
			}
			res = dbase->execute(DB_FIRST, query, NULL, NULL);
			if (query) delete query;
			if (safety) delete safety;
			return (res);
		}
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

/*
 * Return a copy of the next entry in the specified table as specified by
 * the 'next_desc'.  The returned structure 'db_result' contains the status,
 * a copy of the object, and a db_next_desc to be used for a subsequent
 * 'next' operation.
*/
db_result *
db_next_entry(char * table_name, db_next_desc * next_desc)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase != NULL) {
			if (safety) delete safety;
			return (dbase->execute(DB_NEXT, NULL, NULL, next_desc));
		} else
			return (set_result(safety, DB_BADTABLE));
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

/*
 * Indicate to the system that you are no longer interested in the rest of the
 * results identified by [next_desc].  After executing this operation, the
 * [next_desc] is no longer valid (cannot  be used as an argument for next).
*/

db_result *
db_reset_next_entry(char * table_name, db_next_desc * next_desc)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase != NULL) {
			if (safety) delete safety;
			return (dbase->execute(DB_RESET_NEXT,
						NULL, NULL, next_desc));
		} else
			return (set_result(safety, DB_BADTABLE));
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

/*
 * Returns copies of entries that satisfy the given attributes from table.
 * Returns the status and entries in a db_result structure.
 * If no attributes are specified, DB_BADQUERY is returned.
*/
db_result *
db_list_entries(char * table_name, int numattrs, nis_attr * attrname)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc * tbl = NULL;
		db * dbase = InUseDictionary->find_table(table_name, &tbl);
		if (tbl == NULL || dbase == NULL)
			return (set_result(safety, DB_BADTABLE));
		else {
			db_result * res = NULL;
			if (numattrs != 0) {
				db_query *query;
				query = InUseDictionary->translate_to_query(tbl,
							    numattrs, attrname);
				if (query == NULL)
					return (set_result(safety,
								DB_BADQUERY));
				res = dbase->execute(DB_LOOKUP, query,
								NULL, NULL);
				delete query;
			} else {
				res = dbase->execute(DB_ALL, NULL, NULL, NULL);
			}
			if (safety) delete safety;
			return (res);
		}
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

/*
 * Object identified by given attribute name is added to specified table.
 * If object already exists, it is replaced.  If more than one object
 * matches the given attribute name, DB_NOTUNIQUE is returned.
 */
static
db_result *
db_add_entry_x(char * tab, int numattrs, nis_attr * attrname,
		entry_obj * newobj, int skiplog, int nosync)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc * tbl = NULL;
		db * dbase = InUseDictionary->find_table(tab, &tbl);
		if (tbl == NULL || dbase == NULL) {
			return (set_result(safety, DB_BADTABLE));
		} else if (skiplog) {
			db_result * res;
			res = dbase->execute(DB_ADD_NOLOG, NULL,
				    (entry_object *) newobj, NULL);
			if (safety) delete safety;
			return (res);
		} else {
			db_result *res;
			db_query *
			query = InUseDictionary->translate_to_query(tbl,
							numattrs, attrname);
			if (query == NULL)
				return (set_result(safety, DB_BADQUERY));
			if (nosync)
				res = dbase->execute(DB_ADD_NOSYNC,
					query, (entry_object *) newobj, NULL);
			else
				res = dbase->execute(DB_ADD, query,
					(entry_object *) newobj, NULL);
			delete query;
			if (safety) delete safety;
			return (res);
		}
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

db_result *
db_add_entry(char * tab, int numattrs, nis_attr * attrname,
		entry_obj * newobj)
{
	return (db_add_entry_x(tab, numattrs, attrname, newobj, 0, 0));
}

db_result *
__db_add_entry_nolog(char * tab, int numattrs, nis_attr * attrname,
		entry_obj * newobj)
{
	return (db_add_entry_x(tab, numattrs, attrname, newobj, 1, 0));
}

db_result *
__db_add_entry_nosync(char * tab, int numattrs, nis_attr * attrname,
			entry_obj * newobj)
{
	return (db_add_entry_x(tab, numattrs, attrname, newobj, 0, 1));
}

/*
 * Remove object identified by given attributes from specified table.
 * If no attribute is supplied, all entries in table are removed.
 * If attributes identify more than one object, all objects are removed.
*/

db_result *
db_remove_entry_x(char * table_name, int num_attrs, nis_attr * attrname,
			int nosync)
{
	int jmp_status;
	db_result * safety = empty_result(DB_SUCCESS);

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc * tbl = NULL;
		db * dbase = InUseDictionary->find_table(table_name, &tbl);
		db_result * res;
		if (tbl == NULL || dbase == NULL)
			return (set_result(safety, DB_BADTABLE));
		else {
			if (num_attrs != 0) {
				db_query *query;
				query = InUseDictionary->translate_to_query(tbl,
						num_attrs, attrname);
				if (query == NULL)
					return (set_result(safety,
							DB_BADQUERY));
				if (nosync)
					res = dbase->execute(DB_REMOVE_NOSYNC,
							query, NULL, NULL);
				else
					res = dbase->execute(DB_REMOVE, query,
							NULL, NULL);
				delete query;
			} else {
				if (nosync)
					res = dbase->execute(DB_REMOVE_NOSYNC,
						NULL, NULL, NULL);
				else
					res = dbase->execute(DB_REMOVE,
						NULL, NULL, NULL);
			}
			if (safety) delete safety;
			return (res);
		}
	} else
		return (set_result(safety, DB_INTERNAL_ERROR));
}

db_result *
db_remove_entry(char * table_name, int num_attrs, nis_attr * attrname)
{
	return (db_remove_entry_x(table_name, num_attrs, attrname, 0));
}

db_result *
__db_remove_entry_nosync(char * table_name, int num_attrs, nis_attr * attrname)
{
	return (db_remove_entry_x(table_name, num_attrs, attrname, 1));
}

/* Return a copy of the version of specified table. */
vers *
db_version(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (NULL);
		vers* v = new vers(dbase->get_version());
		if (v == NULL)
			WARNING("nis_db::db_version: cannot allocate space");
		return (v);
	} else
		return (NULL);
}

/* Return log entries since (later than) given version 'v' of table. */
db_log_list *
db_log_entries_since(char * table_name, vers * v)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (NULL);
		return (dbase->get_log_entries_since(v));
	} else
		return (NULL);
}

db_status
db_sync_log(char *table_name) {

	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (DB_BADTABLE);
		return (dbase->sync_log());
	} else
		return (DB_INTERNAL_ERROR);
}

/*
 * Apply the given update specified in 'entry' to the specified table.
 * Returns DB_SUCCESS if update was executed.
 * Returns DB_NOTFOUND if update occurs too early to be applied.
*/
db_status
db_apply_log_entry(char * table_name, db_log_entry * entry)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (DB_BADTABLE);
		if (dbase->execute_log_entry(entry))
			return (DB_SUCCESS);   /* got executed */
		else
			return (DB_NOTFOUND);  /* not executed */
	} else
		return (DB_INTERNAL_ERROR);
}

/*
 * Checkpoint specified table (i.e. incorporate logged updates to main
 * database file).  If table_name is NULL, checkpoint all tables that
 * needs it.
*/
db_status
db_checkpoint(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		return (InUseDictionary->db_checkpoint(table_name));
	} else
		return (DB_INTERNAL_ERROR);
}

/* Print names of tables in system. */
void
db_print_table_names()
{
	int i;
	int jmp_status;
	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_names * answer = InUseDictionary->get_table_names();
		if (answer != NULL) {
			for (i = 0; i < answer->db_table_names_len; i++) {
				printf("%s\n", answer->db_table_names_val[i]);
				delete answer->db_table_names_val[i];
			}
			delete answer->db_table_names_val;
			delete answer;
		}
	}
}

/* Print statistics of specified table to stdout. */
db_status
db_stats(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc * tbl = NULL;
		db *dbase = InUseDictionary->find_table(table_name, &tbl);
		if (tbl == NULL || dbase == NULL || tbl->scheme == NULL)
			return (DB_BADTABLE);

		dbase->print();
		tbl->scheme->print();
		return (DB_SUCCESS);
	} else
		return (DB_INTERNAL_ERROR);
}


/* Print statistics of indices of specified table to stdout. */
db_status
db_print_all_indices(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (DB_BADTABLE);
		dbase->print_all_indices();
		return (DB_SUCCESS);
	} else
		return (DB_INTERNAL_ERROR);
}

/* Print specified index of table to stdout. */
db_status
db_print_index(char * table_name, int which)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db * dbase = InUseDictionary->find_table(table_name);
		if (dbase == NULL)
			return (DB_BADTABLE);
		dbase->print_index(which);
		return (DB_SUCCESS);
	} else
		return (DB_INTERNAL_ERROR);
}

/* close open files */
db_status
db_standby(char * table_name)
{
	return (InUseDictionary->db_standby(table_name));
}

/* Returns DB_SUCCESS if table exists; DB_BADTABLE if table does not exist. */
db_status
db_table_exists(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc *
		dbtab = InUseDictionary->find_table_desc(table_name);
		if (dbtab == NULL)
			return (DB_BADTABLE);
		return (DB_SUCCESS);
	} else
		return (DB_INTERNAL_ERROR);
}

/*
 * Returns DB_SUCCESS if table exists; DB_BADTABLE if table does not exist.
 *  If table already loaded, unload it.
*/
db_status
db_unload_table(char * table_name)
{
	int jmp_status;

	if ((jmp_status = setjmp(dbenv)) == 0) {
		db_table_desc *
		dbtab = InUseDictionary->find_table_desc(table_name);
		if (dbtab == NULL)
			return (DB_BADTABLE);
		// unload
		if (dbtab->database != NULL) {
			delete dbtab->database;
			dbtab->database = NULL;
		}
		return (DB_SUCCESS);
	} else
		return (DB_INTERNAL_ERROR);
}


};  /* extern "C" */
