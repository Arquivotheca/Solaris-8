/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_files_internal.cc	1.21	98/01/27 SMI"

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ndbm.h>
#include <thread.h>
#include <synch.h>

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <FNSP_Syntax.hh>
#include "FNSP_filesImpl.hh"
#include "fnsp_files_internal.hh"

/* /var files map names and directory */
static const char *FNSP_files_map_dir = FNSP_FILES_MAP_DIR;
static const char *FNSP_user_map = FNSP_USER_MAP_PRE;
static const char *FNSP_user_attr_map = FNSP_USER_ATTR_MAP;
static const char *FNSP_map_suffix = FNSP_MAP_SUFFIX;
static const char *FNSP_lock_file = "fns.lock";
static const char *FNSP_count_file = "fns.count";

// Define min lines in the DBM file and max disk used
// by a dbm file
#define	FNS_MAX_ENTRY (64*1024)
#define	DBM_FILES_MIN_LINES	20480
#define	DBM_FILES_MAX_SIZE	73728

// ----------------------------------------------
// File maipulation routines for /var files maps
// This routine can be used to selectively
// insert, delete, store and modify an entry
// in the map speficied
// ----------------------------------------------
static unsigned
FNSP_is_update_valid(const char *map)
{
	// Check for *root* permissions
	uid_t pid = geteuid();
	if (pid == 0)
		return (FN_SUCCESS);

	// Check if users' attribute map
	if (strcmp(map, FNSP_user_attr_map) == 0)
		return (FN_SUCCESS);

	// Else check for user's DBM file
	struct passwd *user_entry, user_buffer;
	char buffer[FNS_FILES_SIZE];
	// %%% Should we use fgetent instead?
	user_entry = getpwuid_r(pid, &user_buffer, buffer,
	    FNS_FILES_SIZE);
	if (user_entry == NULL)
		return (FN_E_CTX_NO_PERMISSION);

	// Username must be legalized
	char username[FNS_FILES_INDEX];
	strcpy(username, user_buffer.pw_name);
	FNSP_legalize_name(username);

	// Construct the map name
	char mapfile[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_user_map);
	strcat(mapfile, "_");
	strcat(mapfile, username);
	strcat(mapfile, FNSP_map_suffix);
	if (strcmp(mapfile, map) == 0)
		return (FN_SUCCESS);
	else
		return (FN_E_CTX_NO_PERMISSION);
}

static char *
FNSP_legalize_map_name(const char *name)
{
	// Parse the string to check for "/"
	// If present replace with "#"
	// if "#" is present replace it with "~#"
	size_t i,j;
	int count = 0;
	int replace_count = 0;
	char *answer;
	char internal_name = '/';
	char replace_name = '#';
	for (i = 0; i < strlen(name); i++) {
		if (name[i] == internal_name)
			count++;
		if (name[i] == replace_name)
			replace_count++;
	}
	if ((count == 0) && (replace_count == 0))
		return (strdup(name));
	answer = (char *) malloc(strlen(name) + (size_t) 2*replace_count + (size_t) 1);
	if (answer == 0)
		return (0);
	for (i = 0, j = 0; i < strlen(name); i++, j++) {
		if (name[i] == internal_name)
			answer[j] = replace_name;
		else if (name[i] == replace_name) {
			answer[j] = replace_name; j++;
			answer[j] = '~'; j++;
			answer[j] = name[i];
		} else
			answer[j] = name[i];
	}
	answer[strlen(name) + replace_count] = '\0';
	return (answer);
}

#define MAP_SUFFIX_LEN 4
#define FNSP_ATTR_MAP_SUFFIX ".attr"
#define ATTR_MAP_SUFFIX_LEN 5
#define FNSP_USER_MAP_PREFIX "fns_user_"
#define USER_MAP_PREFIX_LEN 9

// Cache to store the READ ONLY NDBM files
// Does NOT cache the user's NDBM files, since
// this would make the cache extremely large and
// inefficient
class FNSP_ndbm_files {
protected:
	char *dbm_file;
	DBM  *dbm;
	hrtime_t cache_time;
	FNSP_ndbm_files *next;
	DBM *open_dbm();

public:
	FNSP_ndbm_files(const char *file_name);
	virtual ~FNSP_ndbm_files();
	static DBM *open_dbm(const char *file_name, int *valid);
};

static mutex_t files_rd_only_ndbm_cache_lock = DEFAULTMUTEX;
static FNSP_ndbm_files files_rd_only_ndbm_cache("/var/fn/fns_org.ctx");
static files_rd_only_ndbm_cache_dirty;
#define NDBM_MAX_CACHE_TIME	((hrtime_t) (300000000000))

FNSP_ndbm_files::FNSP_ndbm_files(const char *file_name)
{
	dbm_file = strdup(file_name);
	dbm = dbm_open(file_name, O_RDONLY, 0444);
	cache_time = gethrtime();
	next = 0;
}

FNSP_ndbm_files::~FNSP_ndbm_files()
{
	if (next)
		delete (next);
	if (dbm_file)
		free (dbm_file);
	if (dbm)
		dbm_close(dbm);
}

DBM *FNSP_ndbm_files::open_dbm()
{
	hrtime_t currenttime = gethrtime();
	if (((currenttime - cache_time) > NDBM_MAX_CACHE_TIME) ||
	    (files_rd_only_ndbm_cache_dirty)) {
		if (dbm)
			dbm_close(dbm);
		dbm = dbm_open(dbm_file, O_RDONLY, 0444);
		cache_time = currenttime;
	}
	return (dbm);
}

DBM *FNSP_ndbm_files::open_dbm(const char *file_name, int *valid)
{
	FNSP_ndbm_files *current;

	// Check if there has been a write operation
	if (files_rd_only_ndbm_cache_dirty) {
		files_rd_only_ndbm_cache.open_dbm();
		files_rd_only_ndbm_cache_dirty = 0;
		delete files_rd_only_ndbm_cache.next;
		files_rd_only_ndbm_cache.next = 0;
	}

	const char *cache_name = &file_name[strlen("/var/fn/")];
	*valid = 0;
	
	if ((strncmp(cache_name, FNSP_USER_MAP_PREFIX,
	    (size_t) USER_MAP_PREFIX_LEN) == 0) &&
	    (strncmp(&cache_name[strlen(cache_name) - ATTR_MAP_SUFFIX_LEN],
	    FNSP_ATTR_MAP_SUFFIX, (size_t) ATTR_MAP_SUFFIX_LEN) != 0) &&
	    (isdigit(cache_name[USER_MAP_PREFIX_LEN]) == 0))
		return (0);

	*valid = 1;
	mutex_lock(&files_rd_only_ndbm_cache_lock);
	current = &files_rd_only_ndbm_cache;
	while (current) {
		if (strcmp(current->dbm_file, file_name) == 0) {
			mutex_unlock(&files_rd_only_ndbm_cache_lock);
			return (current->open_dbm());
		}
		current = current->next;
	}
	mutex_unlock(&files_rd_only_ndbm_cache_lock);

	
	current = new FNSP_ndbm_files(file_name);
	mutex_lock(&files_rd_only_ndbm_cache_lock);
	current->next = files_rd_only_ndbm_cache.next;
	files_rd_only_ndbm_cache.next = current;
	mutex_unlock(&files_rd_only_ndbm_cache_lock);
	return (current->open_dbm());
}

unsigned
FNSP_files_compose_next_map_name(char *map)
{
	int ctx = 0;

	// Check if the last char are of FNSP_map_suffix
	size_t length = strlen(map) - MAP_SUFFIX_LEN;
	if (strcmp(&map[length], FNSP_MAP_SUFFIX) == 0) {
		map[length] = '\0';
		ctx = 1;
	} else {
		length = strlen(map) - ATTR_MAP_SUFFIX_LEN;
		map[length] = '\0';
	}

	if (!isdigit(map[strlen(map) - 1])) {
		strcat(map, "_0");
		if (ctx)
			strcat(map, FNSP_MAP_SUFFIX);
		else
			strcat(map, FNSP_ATTR_MAP_SUFFIX);
		return (FN_SUCCESS);
	}

	char num[FNS_FILES_INDEX];
	int map_num;
	size_t i = strlen(map) - 1;
	while (map[i] != '_') i--;
	strcpy(num, &map[i+1]);
	map_num = atoi(num) + 1;
	if (ctx)
		sprintf(&map[i+1], "%d%s", map_num, FNSP_MAP_SUFFIX);
	else
		sprintf(&map[i+1], "%d%s", map_num, FNSP_ATTR_MAP_SUFFIX);
	return (FN_SUCCESS);
}

static int
FNSP_get_number_of_lines(const char *mapfile)
{
	int count = 0;
	FILE *rf;
	char count_file[FNS_FILES_INDEX], map[FNS_FILES_INDEX], *num;
	char line[FNS_FILES_SIZE];

	// If map is of user context, return 0
	strcpy(map, &mapfile[strlen(FNSP_files_map_dir) + 1]);
	if (strlen(map) < USER_MAP_PREFIX_LEN)
		return (0);
	if ((strncmp(map, FNSP_USER_MAP_PREFIX, (size_t) USER_MAP_PREFIX_LEN) == 0) &&
	    (strncmp(&map[strlen(map) - ATTR_MAP_SUFFIX_LEN],
	    FNSP_ATTR_MAP_SUFFIX, (size_t) ATTR_MAP_SUFFIX_LEN) != 0) &&
	    (isdigit(map[USER_MAP_PREFIX_LEN]) == 0))
		return (0);

	// Construnct the count_file name
	strcpy(count_file, FNSP_files_map_dir);
	strcat(count_file, "/");
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((rf = fopen(count_file, "w")) == NULL)
			return (-1);
		fclose(rf);
		return (0);
	}

	while (fgets(line, sizeof (line), rf)) {
		if (FNSP_match_map_index(line, map)) {
			num = line + strlen(map);
			while (isspace(*num))
				num++;
			count = atoi(num);
		}
	}
	fclose(rf);
	return (count);
}

static unsigned
FNSP_set_number_of_lines(const char *mapfile, int count)
{
	// If mapfile is of user context, return 0
	char map[FNS_FILES_INDEX];
	strcpy(map, &mapfile[strlen(FNSP_files_map_dir) + 1]);
	if (strlen(map) < USER_MAP_PREFIX_LEN)
		return (FN_SUCCESS);
	if ((strncmp(map, FNSP_USER_MAP_PREFIX, (size_t) USER_MAP_PREFIX_LEN) == 0) &&
	    (strncmp(&map[strlen(map) - ATTR_MAP_SUFFIX_LEN],
	    FNSP_ATTR_MAP_SUFFIX, (size_t) ATTR_MAP_SUFFIX_LEN) != 0) &&
	    (isdigit(map[USER_MAP_PREFIX_LEN]) == 0))
		return (FN_SUCCESS);

	FILE *wf, *rf;
	char count_file[FNS_FILES_INDEX];
	char temp_file[FNS_FILES_INDEX];
	char line[FNS_FILES_SIZE];

	// Construnct the count_file name
	strcpy(count_file, FNSP_files_map_dir);
	strcat(count_file, "/");
	strcat(count_file, FNSP_count_file);
	if ((rf = fopen(count_file, "r")) == NULL) {
		if ((wf = fopen(count_file, "w")) == NULL)
			return (FN_E_INSUFFICIENT_RESOURCES);
		fprintf(wf, "%s  %d\n", mapfile, count);
		fclose(wf);
		return (FN_SUCCESS);
	}
	strcpy(temp_file, count_file);
	strcat(temp_file, ".tmp");
	if ((wf = fopen(temp_file, "w")) == NULL) {
		fclose(rf);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	int found = 0;
	while (fgets(line, sizeof (line), rf)) {
		if ((!found) &&
		    (FNSP_match_map_index(line, map))) {
			found = 1;
			fprintf(wf, "%s  %d\n", map, count);
		} else
			fputs(line, wf);
	}
	if (!found)
		fprintf(wf, "%s  %d\n", map, count);
	fclose(rf);
	fclose(wf);
	if (rename(temp_file, count_file) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	return (FN_SUCCESS);
}

static unsigned
FNSP_files_fast_update_map(const char *map,
    const char *index, const void *data)
{
	// Compose the map name
	char mapfile[FNS_FILES_INDEX], chmod_map_file[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, map);

	// Construct the DBM items
	DBM *db;
	int dbm_ret, chmod_for_attr_map = 0;
	datum dbm_key, dbm_value, dbm_added;
	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);
	dbm_value.dptr = (char *) data;
	dbm_value.dsize = strlen((char *) data);

	// Declarations to check the memory size
	struct stat dbm_stat;
	char dbmfile[FNS_FILES_INDEX];
	int valid_dbm_file, lines, loop_count = 0;

 fast_files_update_start_over:
	loop_count++;
	if (loop_count > 100)
		return (FN_E_INSUFFICIENT_RESOURCES);
	// Check the meory size of the dbm file before using it
	valid_dbm_file = 0;
	unsigned status = FN_SUCCESS;
	while (!valid_dbm_file) {
		lines = FNSP_get_number_of_lines(mapfile);
		if (lines < DBM_FILES_MIN_LINES)
			valid_dbm_file = 1;
		else if (lines >= FNS_MAX_ENTRY)
			FNSP_files_compose_next_map_name(mapfile);
		else {
			strcpy(dbmfile, mapfile);
			strcat(dbmfile, ".pag");
			if (stat(dbmfile, &dbm_stat) != 0)
				// File not found
				valid_dbm_file = 1;
			else
				if (dbm_stat.st_blocks < DBM_FILES_MAX_SIZE)
					valid_dbm_file = 1;
				else
					FNSP_files_compose_next_map_name(mapfile);
		}
	}

	if ((strncmp(map, FNSP_USER_MAP_PREFIX, (size_t) USER_MAP_PREFIX_LEN) == 0) &&
	    (strncmp(&map[strlen(map) - ATTR_MAP_SUFFIX_LEN],
	    FNSP_ATTR_MAP_SUFFIX, (size_t) ATTR_MAP_SUFFIX_LEN) == 0)) {
		struct stat map_stat;
		strcpy(chmod_map_file, mapfile);
		strcat(chmod_map_file, ".dir");
		int stat_ret = stat(chmod_map_file, &map_stat);
		db = dbm_open(mapfile, O_RDWR | O_CREAT, S_ISVTX | 0666);
		if (stat_ret != 0)
			chmod_for_attr_map = 1;
	} else
		db = dbm_open(mapfile, O_RDWR | O_CREAT, 0644);

	if (db == 0)
		// dbm_open failed, hence possible error conditions
		// are insufficent resources or premissions denied.
		//  Does not make sense to go to fast_files_update_start_over
		return (FN_E_INSUFFICIENT_RESOURCES);

	dbm_clearerr(db);
	dbm_ret = dbm_store(db, dbm_key, dbm_value, DBM_INSERT);
	if (dbm_ret == 1)
		status = FN_E_NAME_IN_USE;
	else if ((dbm_ret != 0) || dbm_error(db)) {
		// Hashing problem, try next file
		dbm_delete(db, dbm_key);
		dbm_close(db);
		FNSP_files_compose_next_map_name(mapfile);
		if (dbm_error(db))
			dbm_clearerr(db);
		goto fast_files_update_start_over;
	} else {
		FNSP_set_number_of_lines(mapfile, lines+1);
		status = FN_SUCCESS;
	}

	// Make sure the entry is present in the DBM file
	// %%% There seems to be a BUG in NDBM where, a success
	// is returned but the entry is not added to the DBM file
	dbm_clearerr(db);
	dbm_added = dbm_fetch(db, dbm_key);
	if (dbm_added.dptr == NULL) {
		// Value is not added, add to the next map
		dbm_close(db);
		FNSP_set_number_of_lines(mapfile, lines);
		FNSP_files_compose_next_map_name(mapfile);
		goto fast_files_update_start_over;
	}
	dbm_close(db);
	if (chmod_for_attr_map) {
		chmod(chmod_map_file, S_ISVTX | 0666);
		strcpy(chmod_map_file, mapfile);
		strcat(chmod_map_file, ".pag");
		chmod(chmod_map_file, S_ISVTX | 0666);
	}
	return (status);
}

static unsigned
FNSP_files_local_update_map(const char *old_map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	unsigned status;

	// Check if update is valid
	if ((status = FNSP_is_update_valid(old_map))
	    != FN_SUCCESS)
		return (status);

	// Legalize the map name
	char map[FNS_FILES_INDEX];
	char *legal_map = FNSP_legalize_map_name(old_map);
	if (legal_map == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	strcpy(map, legal_map);
	free (legal_map);

	// DBM files and datum variables
	DBM *db;
	int dbm_ret;
	datum dbm_key, dbm_val, dbm_value;
	dbm_key.dptr = (char *) index;
	dbm_key.dsize = strlen(index);

	// DBM file name
	struct stat buffer;
	char mapfile[FNS_FILES_INDEX], dbmfile[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	strcpy(dbmfile, mapfile);
	strcat(dbmfile, ".dir");

	dbm_ret = 1;
	while (stat(dbmfile, &buffer) == 0) {
		if ((db = dbm_open(mapfile, O_RDWR, 0644)) == 0) {
			// %%% print error message?
			// perror("unable to open file for adding");
			return (FN_E_INSUFFICIENT_RESOURCES);
		}

		switch (op) {
		case FNSP_map_store:
		case FNSP_map_modify:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_value.dptr = (char *) data;
				dbm_value.dsize = strlen((char *) data);
				dbm_ret = dbm_store(db, dbm_key, dbm_value,
				    DBM_REPLACE);
				if (dbm_ret != 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					dbm_ret = 0;
				}
			}
			break;

		case FNSP_map_delete:
			dbm_ret = dbm_delete(db, dbm_key);
			if (dbm_ret == 0) {
				FNSP_set_number_of_lines(mapfile,
				    FNSP_get_number_of_lines(mapfile)
				    - 1);
				status = FN_SUCCESS;
			}
			break;
		case FNSP_map_insert:
			dbm_val = dbm_fetch(db, dbm_key);
			if (dbm_val.dptr != 0) {
				dbm_ret = 0;
				status = FN_E_NAME_IN_USE;
			}
			break;
		}

		dbm_close(db);
		if (!dbm_ret) {
			// Found entry
			return (status);
		}
		FNSP_files_compose_next_map_name(mapfile);
		strcpy(dbmfile, mapfile);
		strcat(dbmfile, ".dir");
	}

	switch (op) {
	case FNSP_map_modify:
	case FNSP_map_delete:
		return (FN_E_NAME_NOT_FOUND);
	case FNSP_map_insert:
	case FNSP_map_store:
		return (FNSP_files_fast_update_map(map, index, data));
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}
}

unsigned
FNSP_files_update_map(const char *map,
    const char *index, const void *data,
    FNSP_map_operation op)
{
	// If the operation is store or modify, delete first
	unsigned status;
	switch (op) {
	case FNSP_map_store:
	case FNSP_map_modify:
		status = FNSP_files_update_map(map, index, data,
		    FNSP_map_delete);
		if (status == FN_E_INSUFFICIENT_RESOURCES)
			return (status);
	default:
		break;
	}

	// Lock FNS update map
	int lock_fs;
	char lock_file[FNS_FILES_INDEX];
	strcpy(lock_file, FNSP_files_map_dir);
	strcat(lock_file, "/");
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// Hold all the signals
	sigset_t mask_signal, org_signal;
	sigfillset(&mask_signal);
	sigprocmask(SIG_BLOCK, &mask_signal, &org_signal);

	// Need to split the data to fit 1024 byte limitation
	char new_index[FNS_FILES_SIZE], next_index[FNS_FILES_SIZE];
	char new_data[FNS_FILES_SIZE];
	unsigned stat, count = 1, delete_count = 0;
	size_t length = 0;
	for (status = FNSP_get_first_index_data(op, index, data,
	    new_index, new_data, length, next_index);
	    status == FN_SUCCESS;
	    status = FNSP_get_next_index_data(op, index, data,
	    new_index, new_data, length, next_index)) {
		stat = FNSP_files_local_update_map(map,
		    new_index, new_data, op);
		if (stat != FN_SUCCESS) {
			if ((op == FNSP_map_delete) &&
			    (delete_count < 2) && (count != 1))
				delete_count++;
			else {
				if ((count == 1) ||
				    (op != FNSP_map_delete))
					status = stat;
				else
					status = FN_SUCCESS;
				goto files_map_update;
			}
		} else if (op == FNSP_map_delete)
			delete_count = 0;
		count++;
	}
	if (!((count == 1) && (status == FN_E_INSUFFICIENT_RESOURCES)))
		status = FN_SUCCESS;

 files_map_update:
	if (status == FN_SUCCESS)
		files_rd_only_ndbm_cache_dirty = 1;
	sigprocmask(SIG_SETMASK, &org_signal, 0);
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	return (status);
}

static unsigned
FNSP_files_local_lookup(char *old_map, char *map_index, int,
    char **mapentry, int *maplen)
{
	DBM *db;
	int from_cache = 0;
	char mapfile[FNS_FILES_INDEX];
	datum dbm_key, dbm_value;

	// Legalize the map name
	char *map = FNSP_legalize_map_name(old_map);
	if (map == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	dbm_key.dptr = map_index;
	dbm_key.dsize = strlen(map_index);
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, map);
	free (map);
	db = FNSP_ndbm_files::open_dbm(mapfile, &from_cache);
	if (!from_cache)
		db = dbm_open(mapfile, O_RDONLY, 0444);
	while (db) {
		dbm_value = dbm_fetch(db, dbm_key);
		if (dbm_value.dptr != 0) {
			*maplen = dbm_value.dsize;
			*mapentry = (char *) malloc(*maplen + 1);
			strncpy(*mapentry, dbm_value.dptr, (*maplen));
			(*mapentry)[(*maplen)] = '\0';
			if (!from_cache)
				dbm_close(db);
			return (FN_SUCCESS);
		}
		if (!from_cache)
			dbm_close(db);
		FNSP_files_compose_next_map_name(mapfile);
		db = FNSP_ndbm_files::open_dbm(mapfile, &from_cache);
		if (!from_cache)
			db = dbm_open(mapfile, O_RDONLY, 0444);
	}

	return (FN_E_NAME_NOT_FOUND);
}

class FNSP_files_individual_entry {
public:
	char *mapentry;
	size_t maplen;
	FNSP_files_individual_entry *next;
};

unsigned FNSP_files_lookup(char *map, char *map_index, int,
    char **mapentry, int *maplen)
{
	char new_index[FNS_FILES_SIZE], next_index[FNS_FILES_SIZE];
	unsigned status, stat;
	FNSP_files_individual_entry *prev_entry, *new_entry, *entries;

	entries = 0;
	prev_entry = 0;
	// Obtain the FNS lock
	int lock_fs;
	char lock_file[FNS_FILES_INDEX];
	strcpy(lock_file, FNSP_files_map_dir);
	strcat(lock_file, "/");
	strcat(lock_file, FNSP_lock_file);
	if ((lock_fs = open(lock_file, O_WRONLY)) == -1)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if (lockf(lock_fs, F_LOCK, 0L) == -1) {
		close(lock_fs);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	for (status = FNSP_get_first_lookup_index(map_index,
	    new_index, next_index); status == FN_SUCCESS;
	    status = FNSP_get_next_lookup_index(new_index, next_index,
	    *mapentry, *maplen)) {
		stat = FNSP_files_local_lookup(map, new_index,
		    strlen(new_index), mapentry, maplen);
		if (stat != FN_SUCCESS)
			break;
		new_entry = (FNSP_files_individual_entry *)
		    malloc(sizeof(FNSP_files_individual_entry));
		new_entry->mapentry = *mapentry;
		new_entry->maplen = *maplen;
		new_entry->next = 0;
		if (entries == 0)
			entries = new_entry;
		else
			prev_entry->next = new_entry;
		prev_entry = new_entry;
	}
	// Release the FNS lock
	lockf(lock_fs, F_ULOCK, 0L);
	close(lock_fs);
	if (!entries)
		return (stat);

	// Construct the mapentry from the linked list
	size_t length = 0;
	new_entry = entries;
	while (new_entry) {
		length += new_entry->maplen;
		new_entry = new_entry->next;
	}
	*maplen = (int) length;
	*mapentry = (char *) malloc(sizeof(char)*(length + 1));
	if (*mapentry == 0)
		// Malloc error
		return (FN_E_INSUFFICIENT_RESOURCES);

	memset((void *) (*mapentry), 0, sizeof(char)*(length + 1));
	new_entry = entries;
	while (new_entry) {
		if (new_entry == entries)
			strcpy((*mapentry), new_entry->mapentry);
		else
			strcat((*mapentry), new_entry->mapentry);
		new_entry = new_entry->next;
	}

	// Destroy entries
	new_entry = entries;
	while (new_entry) {
		free (new_entry->mapentry);
		prev_entry = new_entry;
		new_entry = prev_entry->next;
		free (prev_entry);
	}
	return (FN_SUCCESS);
}

int
FNSP_files_is_fns_installed(const FN_ref_addr *)
{
	// Check if fns.lock file exits
	char fns_lock[FNS_FILES_INDEX];
	struct stat buffer;
	strcpy(fns_lock, FNSP_files_map_dir);
	strcat(fns_lock, "/");
	strcat(fns_lock, FNSP_lock_file);
	if (stat(fns_lock, &buffer) == 0)
		return (FN_SUCCESS);
	else
		return (0);
}

int
FNSP_change_user_ownership(const char *username)
{
	char mapfile[FNS_FILES_INDEX];
	char uname[FNS_FILES_INDEX];

	// Normalize the user name
	strcpy(uname, username);
	FNSP_legalize_name(uname);

	// Open the passwd file
	FILE *passwdfile;
	if ((passwdfile = fopen("/etc/passwd", "r")) == NULL) {
		// fprintf(stderr,
		// gettext("%s: could not open file %s for read\n"
		// "Permissions for user context %s not changed\n"),
		// program_name, FNSP_get_user_source_table(),
		// username);
		return (0);
	}

	// Seach for the user entry
	char buffer[MAX_CANON+1];
	struct passwd *owner_passwd, buf_passwd;
	while ((owner_passwd = fgetpwent_r(passwdfile, &buf_passwd,
	    buffer, MAX_CANON)) != 0) {
		if (strcmp(owner_passwd->pw_name, username) == 0)
			break;
	}
	fclose(passwdfile);

	// Check if the user entry exists in the passwd table
	if (owner_passwd == NULL) {
		// fprintf(stderr,
		// gettext("Unable to get user ID of %s\n"), username);
		// fprintf(stderr, gettext("Permissions for the user context "
		// "%s not changed\n"), username);
		return (0);
	}

	// Change the ownership of the .pag file
	strcpy(mapfile, "/var/fn/fns_user_");
	strcat(mapfile, uname);
	strcat(mapfile, ".ctx.pag");
	if (chown(mapfile, owner_passwd->pw_uid, -1) != 0) {
		// fprintf(stderr, gettext("Permissions for the user context "
		// "%s not changed\n"), username);
		return (0);
	}

	// Change the ownership of the .dir file
	strcpy(&mapfile[strlen(mapfile) - strlen("dir")], "dir");
	if (chown(mapfile, owner_passwd->pw_uid, -1) != 0) {
		// fprintf(stderr, gettext("Permissions for the user "
		// "context %s not changed\n"), username);
		return (0);
	}
	return (1);
}

// Support for looking entries in /etc/* files

static int
FNSP_match_passwd_entry(const char *line, const char *name)
{
	size_t len = strlen(name);
	return ((strncasecmp(line, name, len) == 0) &&
	    (line[len] == ':'));
}

static int
FNSP_match_hosts_entry(char *line, const char *name)
{
	char *ptr = strstr(line, name);
	if (ptr) {
		ptr--;
		if (!((*ptr == ' ') || (*ptr == '\t')))
			return (0);
		ptr += strlen(name) + 1;
	}
	if ((ptr) &&
	    ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == EOF)))
		return (1);
	else
		return (0);
}

static unsigned
FNSP_files_lookup_user_entry_from_file(FILE *file,
    const char *map_index, char **mapentry, int *maplen)
{
	char line[FNS_FILES_INDEX];

	unsigned status = FN_E_NO_SUCH_ATTRIBUTE;
	while (fgets(line, FNS_FILES_INDEX, file)) {
		if (FNSP_match_passwd_entry(line,
		    map_index)) {
			*maplen = strlen(line);
			*mapentry = (char *)
			    malloc(*maplen + 1);
			strcpy(*mapentry, line);
			status = FN_SUCCESS;
			break;
		}
	}
	return (status);
}

unsigned
FNSP_files_lookup_user_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *filename)
{
	FILE *file;
	if ((file = fopen(filename, "r")) == NULL)
		return (FN_E_NAME_NOT_FOUND);
	unsigned status = FNSP_files_lookup_user_entry_from_file(
	    file, map_index, mapentry, maplen);
	fclose(file);
	return (status);
}

unsigned
FNSP_files_lookup_host_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *filename)
{
	char line[FNS_FILES_INDEX];
	FILE *file;

	// Open the file
	if ((file = fopen(filename, "r")) == NULL)
		return (FN_E_NAME_NOT_FOUND);

	unsigned status = FN_E_NO_SUCH_ATTRIBUTE;
	char tmp_buf[FNS_FILES_INDEX];
	while (fgets(line, FNS_FILES_INDEX, file)) {
		if ((line[0] == '\0') || (line[0] == ' ') ||
		    (line[0] == '\t') || (line[0] == '\n') ||
		    (line[0] == '#'))
			continue;
		strcpy(tmp_buf, line);
		if (FNSP_match_hosts_entry(tmp_buf,
		    map_index)) {
			*maplen = strlen(line);
			*mapentry = (char *)
			    malloc(*maplen + 1);
			strcpy(*mapentry, line);
			status = FN_SUCCESS;
			break;
		}
	}
	fclose(file);
	return (status);
}

unsigned FNSP_files_lookup_service_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *file_name)
{
	char line[FNS_FILES_INDEX];
	FILE *file;

	// Open the file
	if ((file = fopen(file_name, "r")) == NULL)
		return (FN_E_NAME_NOT_FOUND);

	unsigned status = FN_E_NO_SUCH_ATTRIBUTE;
	while (fgets(line, FNS_FILES_INDEX, file)) {
		if ((line[0] == '\0') || (line[0] == ' ') ||
		    (line[0] == '\t') || (line[0] == '\n') ||
		    (line[0] == '#'))
			continue;
		if ((strncasecmp(line, map_index, strlen(map_index)) == 0) &&
		    ((line[strlen(map_index)] == ' ') ||
		    (line[strlen(map_index)] == '\t') ||
		    (line[strlen(map_index)] == '\n'))) {
			*maplen = strlen(line);
			*mapentry = (char *)
			    malloc(*maplen + 1);
			strcpy(*mapentry, line);
			status = FN_SUCCESS;
			break;
		}
	}
	fclose(file);
	return (status);
}
