/*
 *	db.cc
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db.cc	1.17	99/06/03 SMI"

#include <stdio.h>
#include <string.h>
#ifdef TDRPC
#include <sysent.h>
#else
#include <unistd.h>
#endif

#include "db_headers.h"
#include "db.h"

extern db_result *empty_result(db_status);
extern int add_to_standby_list(db*);
extern int remove_from_standby_list(db*);

/* for db_next_desc */

#define	LINEAR 1
#define	CHAINED 2

struct db_next_info {
	int next_type;		/* linear or chained */
	void* next_value;	/* linear: entryp; */
				/* chained: db_next_index_desc* */
};


/* Constructor:  Create a database using the given name, 'dbname.'
	    The database is stored in a file named 'dbname'.
	    The log file is stored in a file named 'dbname'.log.
	    A temporary file 'dbname'.tmp is also used.   */
db::db(char* dbname)
{
	int len = strlen(dbname);
	dbfilename = new char[len+1];
	if (dbfilename == NULL)
		FATAL("db::db: cannot allocate space", DB_MEMORY_LIMIT);
	logfilename = new char[len+5];
	if (logfilename == NULL) {
		delete dbfilename;
		FATAL("db::db: cannot allocate space", DB_MEMORY_LIMIT);
	}
	tmpfilename = new char[len+5];
	if (tmpfilename == NULL) {
		delete dbfilename;
		delete logfilename;
		FATAL("db::db: cannot allocate space", DB_MEMORY_LIMIT);
	}
	sprintf(dbfilename, "%s", dbname);
	sprintf(logfilename, "%s.log", dbname);
	sprintf(tmpfilename, "%s.tmp", dbname);
	logfile = NULL;
	logfile_opened = FALSE;
	changed = FALSE;
}

/* destructor:  note that associated files should be removed separated  */
db::~db()
{
	internal_db.reset();  /* clear any associated data structures */
	delete dbfilename;
	delete logfilename;
	delete tmpfilename;
	close_log();
	delete logfile;
}


static void
assign_next_desc(db_next_desc* desc, entryp value)
{
	db_next_info * store = new db_next_info;
	if (store == NULL) {
		desc->db_next_desc_val =  NULL;
		desc->db_next_desc_len = 0;
		FATAL("db::assign_next_desc: cannot allocate space",
			DB_MEMORY_LIMIT);
	}

	store->next_type = LINEAR;
	store->next_value = (void*)value;
	desc->db_next_desc_val =  (char*) store;
	desc->db_next_desc_len = sizeof (db_next_info);
}

static void
assign_next_desc(db_next_desc* desc, db_next_index_desc * value)
{
	db_next_info * store = new db_next_info;
	if (store == NULL) {
		desc->db_next_desc_val =  NULL;
		desc->db_next_desc_len = 0;
		FATAL("db::assign_next_desc: cannot allocate space (2)",
			DB_MEMORY_LIMIT);
	}
	store->next_type = CHAINED;
	store->next_value = (void*)value;
	desc->db_next_desc_val =  (char*) store;
	desc->db_next_desc_len = sizeof (db_next_info);
}

static entryp
extract_next_desc(db_next_desc* desc, int *next_type,
		db_next_index_desc** place2)
{
	entryp place;

	if (desc == NULL || desc->db_next_desc_len != sizeof (db_next_info)) {
		*next_type = 0;
		return (0);
	}
	*next_type = ((db_next_info*) desc->db_next_desc_val)->next_type;
	switch (*next_type) {
	case LINEAR:
		place = (entryp)
			((db_next_info*) desc->db_next_desc_val)->next_value;
		return (place);

	case CHAINED:
		*place2 = (db_next_index_desc*)
			((db_next_info*) desc->db_next_desc_val) ->next_value;
		return (0);
	default:
		*next_type = 0;   // invalid type
		return (0);
	}
}

/* Execute the specified action using the rest of the arguments as input.
	    Return  a structure db_result containing the result. */
db_result *
db::exec_action(db_action action, db_query *query,
		entry_object *content, db_next_desc* previous)
{
	entryp where, prev;
	db_result *res = new db_result;
	long num_answers;
	entry_object_p * ans;
	entry_object * single;
	db_next_index_desc *index_desc;
	int next_type;
	db_next_index_desc *prev_desc;

	if (res == NULL)
		FATAL("db::exec_action: cannot allocate space for result",
			DB_MEMORY_LIMIT);

	res->objects.objects_len = 0; /* default */
	res->objects.objects_val = NULL;  /* default */

	switch (action) {
	case DB_LOOKUP:
		res->status = internal_db.lookup(query, &num_answers, &ans);
		res->objects.objects_len = (int) num_answers;
		res->objects.objects_val = ans;
		break;

	case DB_ADD:
		res->status = internal_db.add(query, content);
		break;

	case DB_REMOVE:
		res->status = internal_db.remove(query);
		break;

	case DB_FIRST:
		if (query == NULL) {
			res->status = internal_db.first(&where, &single);
			if (res->status == DB_SUCCESS)
				assign_next_desc(&(res->nextinfo), where);
		}  else {
			res->status = internal_db.first(query,
							&index_desc,
							&single);
			if (res->status == DB_SUCCESS)
				assign_next_desc(&(res->nextinfo), index_desc);
		}
		if (res->status == DB_SUCCESS) {
			res->objects.objects_val = new entry_object_p;
			if (res->objects.objects_val == NULL) {
				res->objects.objects_len = 0;
				delete res;
				FATAL(
		"db::exec_action: cannot allocate space for DB_FIRST result",
		DB_MEMORY_LIMIT);
			}
			res->objects.objects_len = 1;
			res->objects.objects_val[0] = single;
		}
		break;

	case DB_NEXT:
		prev = extract_next_desc(previous, &next_type, &prev_desc);
		switch (next_type) {
		case LINEAR:
			if (prev != 0) {
				res->status = internal_db.next(prev, &where,
								&single);
				if (res->status == DB_SUCCESS)
					assign_next_desc(&(res->nextinfo),
								where);
			} else
					// invalid previous indicator
				res->status = DB_NOTFOUND;
			break;
		case CHAINED:
			if (prev_desc != NULL) {
				res->status = internal_db.next(prev_desc,
							&index_desc, &single);
				if (res->status == DB_SUCCESS)
					assign_next_desc(&(res->nextinfo),
								index_desc);
			} else
					// invalid previous indicator
				res->status = DB_NOTFOUND;
			break;
		default:
			WARNING("db::exec_action: invalid previous indicator");
			res->status = DB_BADQUERY;
		}
		if (previous && previous->db_next_desc_val) {
			delete previous->db_next_desc_val;
			previous->db_next_desc_len = 0;
			previous->db_next_desc_val = NULL;
		}
		if (res->status == DB_SUCCESS) {
			res->objects.objects_len = 1;
			res->objects.objects_val = new entry_object_p;
			if (res->objects.objects_val == NULL) {
				res->objects.objects_len = 0;
				delete res;
				FATAL(
		    "db::exec_action: cannot allocate space for DB_NEXT result",
		    DB_MEMORY_LIMIT);
			}
			res->objects.objects_val[0] = single;
		}
		break;

	case DB_RESET_NEXT:
		prev = extract_next_desc(previous, &next_type, &prev_desc);
		switch (next_type) {
		case LINEAR:
			res->status = DB_SUCCESS;
			if (previous->db_next_desc_val) {
	delete previous->db_next_desc_val;
	previous->db_next_desc_len = 0;
	previous->db_next_desc_val = NULL;
			}
			break;   // do nothing
		case CHAINED:
			res->status = internal_db.reset_next(prev_desc);
			if (previous->db_next_desc_val) {
	delete previous->db_next_desc_val;
	previous->db_next_desc_len = 0;
	previous->db_next_desc_val = NULL;
			}
			break;
		default:
			WARNING("db::exec_action: invalid previous indicator");
			res->status = DB_BADQUERY;
		}
		break;

	case DB_ALL:
		res->status = internal_db.all(&num_answers, &ans);
		res->objects.objects_len = (int) num_answers;
		res->objects.objects_val = ans;
		break;

	default:
		WARNING("unknown request");
		res->status = DB_BADQUERY;
		return (res);
	}
	return (res);
}

/*
 * Log the given action and execute it.
 * The minor version of the database is updated after the action has
 * been executed and the database is flagged as being changed.
 * Return the structure db_result, or NULL if the logging failed or the
 * action is unknown.
*/
db_result *
db::log_action(db_action action, db_query *query, entry_object *content)
{
	vers *v = internal_db.get_version()->nextminor();
	db_result * res;
	db_log_entry le(action, v, query, content);

	if (open_log() < 0)  {
		delete v;
		return (empty_result(DB_STORAGE_LIMIT));
	}

	if (logfile->append(&le) < 0) {
		WARNING_M("db::log_action: could not add log entry: ");
		close_log();
		delete v;
		return (empty_result(DB_STORAGE_LIMIT));
	}

	switch (action) {
	case DB_ADD_NOSYNC:
		action = DB_ADD;
		break;
	case DB_REMOVE_NOSYNC:
		action = DB_REMOVE;
		break;
	default:
		if (logfile->sync_log() < 0) {
		    WARNING_M("db::log_action: could not add log entry: ");
		    close_log();
		    delete v;
		    return (empty_result(DB_STORAGE_LIMIT));
		}
		break;
	}
	res = exec_action(action, query, content, NULL);
	internal_db.change_version(v);
	delete v;
	changed = TRUE;

	return (res);
}

/*
 * Execute 'action' using the rest of the arguments as input.
 * Return the result of the operation in a db_result structure;
 * Return NULL if the request is unknown.
 * If the action involves updates (ADD and REMOVE), it is logged first.
 */
db_result *
db::execute(db_action action, db_query *query,
		entry_object *content, db_next_desc* previous)
{
	switch (action) {
	case DB_LOOKUP:
	case DB_FIRST:
	case DB_NEXT:
	case DB_ALL:
	case DB_RESET_NEXT:
		return (exec_action(action, query, content, previous));

	case DB_ADD_NOLOG:
		changed = TRUE;
		return (exec_action(DB_ADD, query, content, previous));

	case DB_ADD:
	case DB_REMOVE:
	case DB_ADD_NOSYNC:
	case DB_REMOVE_NOSYNC:
		return (log_action(action, query, content));

	default:
		WARNING("db::execute: unknown request");
		return (empty_result(DB_INTERNAL_ERROR));
	}
}

/* close existing logfile and delete its structure */
db::reset_log()
{
	/* try to close old log file */
	/* doesnot matter since we do synchronous writes only */
	if (logfile != NULL) {
	    if (logfile_opened == TRUE) {
		    logfile->sync_log();
		    if (logfile->close() < 0) {
			WARNING_M("db::reset_log: could not close log file: ");
		    }
		    remove_from_standby_list(this);
	    }
	    delete logfile;
	    logfile = NULL;
	}
	logfile_opened = FALSE;
	return (0);
}

/* close existing logfile, but leave its structure if exists */
int
db::close_log(int bypass_standby)
{
	if (logfile != NULL && logfile_opened == TRUE) {
		logfile->sync_log();
		logfile->close();
		if (!bypass_standby)
		    remove_from_standby_list(this);
	}
	logfile_opened = FALSE;
	return (0);
}

/* open logfile, creating its structure if it does not exist */
int
db::open_log()
{
	if (logfile == NULL) {
		if ((logfile = new db_log(logfilename, PICKLE_APPEND))
		    == NULL)
			FATAL("db::reset_log: cannot allocate space",
			    DB_MEMORY_LIMIT);
	}

	if (logfile_opened == TRUE)
		return (0);

	if ((logfile->open()) == NULL){
		WARNING_M("db::open_log: could not open log file: ");
		delete logfile;
		logfile = NULL;
		return (-1);
	}
	add_to_standby_list(this);
	logfile_opened = TRUE;
	return (0);
}

/*
 * Execute log entry 'j' on the database identified by 'dbchar' if the
 * version of j is later than that of the database.  If 'j' is executed,
 * 'count' is incremented and the database's verison is updated to that of 'j'.
 * Returns TRUE always for valid log entries; FALSE otherwise.
 */
static bool_t
apply_log_entry(db_log_entry * j, char * dbchar, int *count)
{
	db_mindex * db = (db_mindex *) dbchar;
	bool_t status = TRUE;

	if (db->get_version()->earlier_than(j->get_version())) {
		++ *count;
#ifdef DEBUG
		j->print();
#endif DEBUG
		switch (j->get_action()) {
		case DB_ADD:
		case DB_ADD_NOSYNC:
			db->add(j->get_query(), j->get_object());
			break;

		case DB_REMOVE:
		case DB_REMOVE_NOSYNC:
			db->remove(j->get_query());
			break;

		default:
			WARNING("db::apply_log_entry: unknown action_type");
			return (FALSE);
		}
		db->change_version(j->get_version());
	}

	return (TRUE);  /* always want to TRUE if action valid ? */
}

/*
 * Execute log entry 'j' on this db.  'j' is executed if its version is
 * later than that of the database; if executed, the database's version
 * will be changed to that of 'j', regardless of the status of the operation.
 * Returns TRUE if 'j' was executed;   FALSE if it was not.
 * Log entry is added to this database's log if log_entry is applied.
 */
bool_t
db::execute_log_entry(db_log_entry *j)
{
	int count = 0;
	apply_log_entry (j, (char *) &internal_db, &count);

	/*
	 * should really record the log entry first, but can''t do that without
	 * knowing whether the log entry is applicable.
	 */
	if (count == 1) {
	    if (open_log() < 0)
		    return (FALSE);

	    if (logfile->append(j) < 0) {
		WARNING_M("db::execute_log_entry: could not add log entry: ");
		close_log();
		return (FALSE);
	    }
//	  close_log();  /* do this asynchronously */
	}

	return (count == 1);
}

/* Incorporate updates in log to database already loaded.
	    Does not affect "logfile" */
int
db::incorporate_log(char* filename)
{
	db_log f(filename, PICKLE_READ);

	return (f.execute_on_log(&(apply_log_entry), (char *) &internal_db));
}

/* Load database and incorporate any logged updates into the loaded copy.
	    Return TRUE if load succeeds; FALSE otherwise. */
bool_t
db::load()
{
	int count;
	int load_status;
	if (changed == TRUE)
		syslog(LOG_ERR,
	"WARNING: the current db '%s' has been changed but not checkpointed",
			dbfilename);

	unlink(tmpfilename);  /* get rid of partial checkpoints */

	if ((load_status = internal_db.load(dbfilename)) != 0) {
	    if (load_status < 0)
		    syslog(LOG_ERR, "Load of db '%s' failed", dbfilename);
	    /* otherwise, there was just nothing to load */
	    return (FALSE);
	}

	changed = FALSE;
	reset_log();
	if ((count = incorporate_log(logfilename)) < 0)
		syslog(LOG_ERR, "incorporation of db logfile '%s' load failed",
	    logfilename);
	changed = (count > 0);
	return (TRUE);
}

/*
 * Initialize the database using table scheme 's'.
 * Because the 'scheme' must be 'remembered' between restarts,
 * after the initialization, the empty database is checkpointed to record
 * the scheme. Returns TRUE if initialization succeeds; FALSE otherwise.
 */
bool_t
db::init(db_scheme * s)
{
	internal_db.init(s);
	if (internal_db.good()) {
		unlink(tmpfilename);	/* delete partial checkpoints */
		unlink(logfilename);	/* delete previous logfile */
		reset_log();
		changed = TRUE;		/* force dump to get scheme stored. */
		return (checkpoint());
	}
	else
		return (FALSE);
}

/*
    Write out in-memory copy of database to file.
	    1.  Update major version.
	    2.  Dump contents to temporary file.
	    3.  Rename temporary file to real database file.
	    4.  Remove log file.
    A checkpoint is done only if it has changed since the previous checkpoint.
    Returns TRUE if checkpoint was successful; FALSE otherwise.
*/
bool_t
db::checkpoint()
{
	if (changed == FALSE) {
		return (TRUE);
	}

	vers *oldversion = new vers(internal_db.get_version()); /* copy */
	vers *nextversion = oldversion->nextmajor();	/* get next version */
	internal_db.change_version(nextversion);	/* change version */

	if (internal_db.dump(tmpfilename) < 0) {  	/* dump to tempfile */
		WARNING_M("db::checkpoint: could not dump database: ");
		internal_db.change_version(oldversion);	/* rollback */
		delete nextversion;
		delete oldversion;
		return (FALSE);
	}
	if (rename(tmpfilename, dbfilename) < 0){  	/* rename permanently */
		WARNING_M(
		    "db::checkpoint: could not rename temp file to db file: ");
		internal_db.change_version(oldversion);	/* rollback */
		delete nextversion;
		delete oldversion;
		return (FALSE);
	}
	reset_log();		/* should check for what? */
	unlink(logfilename);	/* should do atomic rename and log delete */
	delete nextversion;
	delete oldversion;
	changed = FALSE;
	return (TRUE);
}


/* For generating log_list */

struct traverse_info {
	vers *version;		// version to check for
	db_log_entry * head;	// head of list of log entries found
	db_log_entry * tail;	// tail of list of log entries found
};

/* For the given entry determine, if it is later than the version supplied,
	    1.  increment 'count'.
	    2.  add the entry to the list of log entries found. */
static bool_t entry_since(db_log_entry * j, char * tichar, int *count)
{
	traverse_info *ti = (traverse_info*) tichar;

	if (ti->version->earlier_than(j->get_version())) {
		++ *count;
//    j->print();   // debug
		if (ti->head == NULL)
			ti->head = j;
		else {
			ti->tail->setnextptr(j); // make last entry point to j
		}
		ti->tail = j;			// make j new last entry
	}

	return (TRUE);
}

/* Return structure db_log_list containing entries that are later
	    than the version 'v' given.  */
db_log_list*
db::get_log_entries_since(vers * v)
{
	int count;
	struct traverse_info ti;
	db_log f(logfilename, PICKLE_READ);

	ti.version = v;
	ti.head = ti.tail = NULL;

	count = f.execute_on_log(&(entry_since), (char *) &ti, FALSE);

	db_log_list * answer = new db_log_list;

	if (answer == NULL)
		FATAL("db::get_log_entries_since: cannot allocate space",
			DB_MEMORY_LIMIT);

	answer->list.list_len = count;

	if (count > 0) {
		db_log_entry_p *entries;
		db_log_entry_p currentry, nextentry;
		int i;

		entries = answer->list.list_val = new db_log_entry_p[count];
		if (entries == NULL) {
			delete answer;
			FATAL(
		"db::get_log_entries_since: cannot allocate space for entries",
		DB_MEMORY_LIMIT);
			}
		currentry = ti.head;
		for (i = 0, currentry = ti.head;
			i < count && currentry != NULL;
			i++) {
			entries[i] = currentry;
			nextentry = currentry->getnextptr();
			currentry->setnextptr(NULL);
			currentry = nextentry;
		}
	} else
		answer->list.list_val = NULL;

	return (answer);
}

/* Delete all files associated with database. */
void
db::remove_files()
{
	unlink(tmpfilename);  /* delete partial checkpoints */
	reset_log();
	unlink(logfilename);  /* delete logfile */
	unlink(dbfilename);   /* delete database file */
}

db_status
db::sync_log() {

	if (logfile == 0)
		return (DB_BADTABLE);

	if (logfile_opened == FALSE || logfile->sync_log())
		return (DB_SUCCESS);
	else
		return (DB_SYNC_FAILED);
}
