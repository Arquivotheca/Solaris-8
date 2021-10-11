/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#pragma ident   "@(#)drvsubr.c 1.14     97/08/12 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>
#include <wait.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include "addrem.h"
#include "errmsg.h"
#include <string.h>
#include <errno.h>


static int get_cached_n_to_m_file(char *filename, char ***cache);
static int get_name_to_major_entry(int *major_no, char *driver_name,
    char *file_name);

/*
 *  open file
 * for each entry in list
 *	where list entries are separated by <list_separator>
 * 	append entry : driver_name <entry_separator> entry
 * close file
 * return error/noerr
 */
int
append_to_file(
	char *driver_name,
	char *entry_list,
	char *filename,
	char list_separator,
	char *entry_separator)
{
	FILE *fp;
	int fpint;
	char *line;
	char *current_head;
	char *previous_head;
	char *one_entry;
	int len;
	int i;

	if ((fp = fopen(filename, "a")) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    filename);
		return (ERROR);
	}

	len = strlen(entry_list);

	one_entry = calloc(len + 1, 1);
	if (one_entry == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_UPDATE), filename);
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		(void) fclose(fp);
		return (ERROR);
	}

	previous_head = entry_list;

	line = calloc(strlen(driver_name) + len + 4, 1);
	if (line == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		(void) fclose(fp);
		err_exit();
	}

	/*
	 * get one entry at a time from list and append to
	 * <filename> file
	 */

	do {

		for (i = 0; i <= len; i++)
			one_entry[i] = 0;

		for (i = 0; i <= (int)strlen(line); i++)
			line[i] = 0;

		current_head = get_entry(previous_head, one_entry,
		    list_separator);
		previous_head = current_head;

		(void) strcpy(line, driver_name);
		(void) strcat(line, entry_separator);
		(void) strcat(line, one_entry);
		(void) strcat(line, "\n");

		if ((fputs(line, fp)) == EOF) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_NO_UPDATE),
			    filename);
		}

	} while (*current_head != '\0');


	(void) fflush(fp);

	fpint = fileno(fp);
	(void) fsync(fpint);

	(void) fclose(fp);

	free(one_entry);
	free(line);

	return (NOERR);
}


/*
 *  open file
 * read thru file, deleting all entries if first
 *    entry = driver_name
 * close
 * if error, leave original file intact with message
 * assumption : drvconfig has been modified to work with clone
 *  entries in /etc/minor_perm as driver:mummble NOT
 *  clone:driver mummble
 * this implementation will NOT find clone entries
 * clone:driver mummble
 */
int
delete_entry(
	char *oldfile,
	char *driver_name,
	char *marker)
{
	FILE *fp;
	FILE *newfp;
	int newfpint;
	char line[MAX_DBFILE_ENTRY];
	char drv[FILENAME_MAX + 1];

	int i;
	int status = NOERR;

	char *newfile;
	char *tptr;

	if ((fp = fopen(oldfile, "r")) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE), oldfile);
		return (ERROR);
	}

	/*
	 * Build filename for temporary file
	 */

	if ((tptr = calloc(strlen(oldfile) + strlen(XEND) + 1, 1)) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
	}

	(void) strcpy(tptr, oldfile);
	(void) strcat(tptr, XEND);

	newfile = mktemp(tptr);

	if ((newfp = fopen(newfile, "w")) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    newfile);
		return (ERROR);
	}

	while ((fgets(line, sizeof (line), fp) != NULL) && status == NOERR) {
		if (*line == '#' || *line == '\n') {
			if ((fputs(line, newfp)) == EOF) {
				(void) fprintf(stderr, gettext(ERR_UPDATE),
				    oldfile);
				status = ERROR;
			}
			continue;
		}
		if (sscanf(line, "%s", drv) != 1) {
			(void) fprintf(stderr, gettext(ERR_BAD_LINE),
			    oldfile, line);
			status = ERROR;
		}


		for (i = strcspn(drv, marker); i < FILENAME_MAX; i++) {
			drv[i] =  '\0';
		}

		if (strcmp(driver_name, drv) != 0) {
			if ((fputs(line, newfp)) == EOF) {
				(void) fprintf(stderr, gettext(ERR_UPDATE),
				    oldfile);
				status = ERROR;
			}

		}
	}

	(void) fclose(fp);

	newfpint = fileno(newfp);
	(void) fsync(newfpint);
	(void) fclose(newfp);

	/*
	 * if error, leave original file, delete new file
	 * if noerr, replace original file with new file
	 */

	if (status == NOERR) {
		if (rename(oldfile, tmphold) == -1) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_UPDATE), oldfile);
			(void) unlink(newfile);
			return (ERROR);
		} else if (rename(newfile, oldfile) == -1) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_UPDATE), oldfile);
			(void) unlink(oldfile);
			(void) unlink(newfile);
			if (link(tmphold, oldfile) == -1) {
				perror(NULL);
				(void) fprintf(stderr, gettext(ERR_BAD_LINK),
				    oldfile, tmphold);
			}
			return (ERROR);
		}
		(void) unlink(tmphold);
	} else {
		/*
		 * since there's an error, leave file alone; remove
		 * new file
		 */
		if (unlink(newfile) == -1) {
			(void) fprintf(stderr, gettext(ERR_CANT_RM), newfile);
		}
		return (ERROR);
	}

	return (NOERR);

}

/*
 * wrapper for call to get_name_to_major_entry(): given driver name,
 * retrieve major number.
 */
int
get_major_no(char *driver_name, char *file_name)
{
	int major = UNIQUE;

	if (get_name_to_major_entry(&major, driver_name, file_name) == ERROR)
		return (ERROR);
	else
		return (major);
}

/*
 * wrapper for call to get_name_to_major_entry(): given major number,
 * retrieve driver name.
 */
int
get_driver_name(int major, char *file_name, char *buf)
{
	if (major < 0)
		return (ERROR);
	return (get_name_to_major_entry(&major, buf, file_name));
}


/*
 * return pointer to cached name_to_major file - reads file into
 * cache if this has not already been done.  Since there may be
 * requests for multiple name_to_major files (rem_name_to_major,
 * name_to_major), this routine keeps a list of cached files.
 */
static int
get_cached_n_to_m_file(char *filename, char ***cache)
{
	struct n_to_m_cache {
		char *file;
		char **cached_file;
		int size;
		struct n_to_m_cache *next;
	};
	static struct n_to_m_cache *head = NULL;
	struct n_to_m_cache *ptr;
	FILE *fp;
	char drv[FILENAME_MAX + 1];
	char entry[FILENAME_MAX + 1];
	char line[MAX_N2M_ALIAS_LINE];
	int maj;
	int size = 0;
	int i;


	/*
	 * see if the file is already cached - either
	 * rem_name_to_major or name_to_major
	 */
	ptr = head;
	while (ptr != NULL) {
		if (strcmp(ptr->file, filename) == 0)
			break;
		ptr = ptr->next;
	}

	if (ptr == NULL) {	/* we need to cache the contents */
		if ((fp = fopen(filename, "r")) == NULL) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_CANT_OPEN),
			    filename);
			return (ERROR);
		}

		while (fgets(line, sizeof (line), fp) != NULL) {
			if (sscanf(line, "%s%s", drv, entry) != 2) {
				(void) fprintf(stderr, gettext(ERR_BAD_LINE),
				    filename, line);
				continue;
			}
			maj = atoi(entry);
			if (maj > size)
				size = maj;
		}

		/* allocate struct to cache the file */
		ptr = (struct n_to_m_cache *)calloc(1,
		    sizeof (struct n_to_m_cache));
		if (ptr == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			return (ERROR);
		}
		ptr->size = size + 1;
		/* allocate space to cache contents of file */
		ptr->cached_file = (char **)calloc(ptr->size, sizeof (char *));
		if (ptr->cached_file == NULL) {
			free(ptr);
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			return (ERROR);
		}

		rewind(fp);

		/*
		 * now fill the cache
		 * the cache is an array of char pointers indexed by major
		 * number
		 */
		while (fgets(line, sizeof (line), fp) != NULL) {
			if (sscanf(line, "%s%s", drv, entry) != 2) {
				(void) fprintf(stderr, gettext(ERR_BAD_LINE),
				    filename, line);
				continue;
			}
			maj = atoi(entry);
			if ((ptr->cached_file[maj] = strdup(drv)) == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				free(ptr->cached_file);
				free(ptr);
				return (ERROR);
			}
			strcpy(ptr->cached_file[maj], drv);
		}
		fclose(fp);
		/* link the cache struct into the list of cached files */
		ptr->file = strdup(filename);
		if (ptr->file == NULL) {
			for (maj = 0; maj <= ptr->size; maj++)
				free(ptr->cached_file[maj]);
			free(ptr->cached_file);
			free(ptr);
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			return (ERROR);
		}
		ptr->next = head;
		head = ptr;
	}
	/* return value pointer to contents of file */
	*cache = ptr->cached_file;

	/* return size */
	return (ptr->size);
}


/*
 * Using get_cached_n_to_m_file(), retrieve maximum major number
 * found in the specificed file (name_to_major/rem_name_to_major).
 *
 * The return value is actually the size of the internal cache including 0.
 */
int
get_max_major(char *file_name)
{
	int max_major = 0;
	char **n_to_m_cache = NULL;

	return (get_cached_n_to_m_file(file_name, &n_to_m_cache));
}


/*
 * searching name_to_major: if major_no == UNIQUE then the caller wants to
 * use the driver name as the key.  Otherwise, the caller wants to use
 * the major number as a key.
 *
 * This routine caches the contents of the name_to_major file on
 * first call.  And it could be generalized to deal with other
 * config files if necessary.
 */
static int
get_name_to_major_entry(int *major_no, char *driver_name, char *file_name)
{
	FILE *fp;
	char drv[FILENAME_MAX + 1];
	char entry[FILENAME_MAX + 1];
	char line[MAX_N2M_ALIAS_LINE];
	int maj;
	char **n_to_m_cache = NULL;
	int size = 0;

	int ret = NOT_UNIQUE;

	/*
	 * read the file in - we cache it in case caller wants to
	 * do multiple lookups
	 */
	size = get_cached_n_to_m_file(file_name, &n_to_m_cache);

	if (size == ERROR)
		return (ERROR);

	/* search with driver name as key */
	if (*major_no == UNIQUE) {
		for (maj = 0; maj < size; maj++) {
			if ((n_to_m_cache[maj] != NULL) &&
			    (strcmp(driver_name, n_to_m_cache[maj]) == 0)) {
				*major_no = maj;
				break;
			}
		}
		if (maj >= size)
			ret = UNIQUE;
	/* search with major number as key */
	} else {
		/*
		 * Bugid 1254588, drvconfig dump core after loading driver
		 * with major number bigger than entries defined in
		 * /etc/name_to_major.
		 */
		if (*major_no >= size)
			return (UNIQUE);

		if (n_to_m_cache[*major_no] != NULL) {
			strcpy(driver_name, n_to_m_cache[*major_no]);
		} else
			ret = UNIQUE;
	}
	return (ret);
}


/*
 * given pointer to member n in space separated list, return pointer
 * to member n+1, return member n
 */
char *
get_entry(
	char *prev_member,
	char *current_entry,
	char separator)
{
	char *ptr;

	ptr = prev_member;

	/* skip white space */
	while (*ptr == '\t' || *ptr == ' ')
		ptr++;

	/* read thru the current entry */
	while (*ptr != separator && *ptr != '\0') {
		*current_entry++ = *ptr++;
	}
	*current_entry = '\0';

	if ((separator == ',') && (*ptr == separator))
		ptr++;	/* skip over comma */

	/* skip white space */
	while (*ptr == '\t' || *ptr == ' ') {
		ptr++;
	}

	return (ptr);
}

void
err_exit(void)
{
	/* remove add_drv/rem_drv lock */
	exit_unlock();
	exit(1);
}

void
exit_unlock(void)
{
	struct stat buf;

	if (stat(add_rem_lock, &buf) == NOERR) {
		if (unlink(add_rem_lock) == -1) {
			(void) fprintf(stderr, gettext(ERR_REM_LOCK),
			    add_rem_lock);
		}
	}
}

/*
 * error adding driver; need to back out any changes to files.
 * check flag to see which files need entries removed
 * entry removal based on driver name
 */
void
remove_entry(
	int c_flag,
	char *driver_name)
{

	if (c_flag & CLEAN_NAM_MAJ) {
		if (delete_entry(name_to_major, driver_name, " ") == ERROR) {
			(void) fprintf(stderr, gettext(ERR_NO_CLEAN),
			    name_to_major, driver_name);
		}
	}

	if (c_flag & CLEAN_DRV_ALIAS) {
		if (delete_entry(driver_aliases, driver_name, " ") == ERROR) {
			(void) fprintf(stderr, gettext(ERR_DEL_ENTRY),
			    driver_name, driver_aliases);
		}
	}

	if (c_flag & CLEAN_DRV_CLASSES) {
		if (delete_entry(driver_classes, driver_name, "\t") == ERROR) {
			(void) fprintf(stderr, gettext(ERR_DEL_ENTRY),
			    driver_name, driver_classes);
		}
	}

	if (c_flag & CLEAN_MINOR_PERM) {
		if (delete_entry(minor_perm, driver_name, ":") == ERROR) {
			(void) fprintf(stderr, gettext(ERR_DEL_ENTRY),
			    driver_name, minor_perm);
		}
	}
}


int
some_checking(
	int m_flag,
	int i_flag)
{
	int status = NOERR;
	int mode = 0;

	/* check name_to_major file : exists and is writable */

	mode = R_OK | W_OK;

	if (access(name_to_major, mode)) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    name_to_major);
		status =  ERROR;
	}

	/* check minor_perm file : exits and is writable */
	if (m_flag) {
		if (access(minor_perm, mode)) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
			    minor_perm);
			status =  ERROR;
		}
	}

	/* check driver_aliases file : exits and is writable */
	if (i_flag) {
		if (access(driver_aliases, mode)) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
			    driver_aliases);
			status =  ERROR;
		}
	}

	return (status);
}

/*
 * All this stuff is to support a server installing
 * drivers on diskless clients.  When on the server
 * need to prepend the basedir
 */
int
build_filenames(char *basedir)
{
	int len;

	if (basedir == NULL) {
		driver_aliases = DRIVER_ALIAS;
		driver_classes = DRIVER_CLASSES;
		minor_perm = MINOR_PERM;
		name_to_major = NAM_TO_MAJ;
		rem_name_to_major = REM_NAM_TO_MAJ;
		add_rem_lock = ADD_REM_LOCK;
		tmphold = TMPHOLD;
		devfs_root = DEVFS_ROOT;

	} else {
		len = strlen(basedir);

		driver_aliases = calloc(len + strlen(DRIVER_ALIAS) +1, 1);
		driver_classes = calloc(len + strlen(DRIVER_CLASSES) +1, 1);
		minor_perm = calloc(len + strlen(MINOR_PERM) +1, 1);
		name_to_major = calloc(len + strlen(NAM_TO_MAJ) +1, 1);
		rem_name_to_major = calloc(len +
		    strlen(REM_NAM_TO_MAJ) +1, 1);
		add_rem_lock = calloc(len + strlen(ADD_REM_LOCK) +1, 1);
		tmphold = calloc(len + strlen(TMPHOLD) +1, 1);
		devfs_root = calloc(len + strlen(DEVFS_ROOT) + 1, 1);


		if ((driver_aliases == NULL) ||
		    (driver_classes == NULL) ||
		    (minor_perm == NULL) ||
		    (name_to_major == NULL) ||
		    (rem_name_to_major == NULL) ||
		    (add_rem_lock == NULL) ||
		    (tmphold == NULL) ||
		    (devfs_root == NULL)) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			return (ERROR);
		}

		(void) sprintf(driver_aliases, "%s%s", basedir, DRIVER_ALIAS);
		(void) sprintf(driver_classes, "%s%s", basedir, DRIVER_CLASSES);
		(void) sprintf(minor_perm, "%s%s", basedir, MINOR_PERM);
		(void) sprintf(name_to_major, "%s%s", basedir, NAM_TO_MAJ);
		(void) sprintf(rem_name_to_major, "%s%s", basedir,
				REM_NAM_TO_MAJ);
		(void) sprintf(add_rem_lock, "%s%s", basedir, ADD_REM_LOCK);
		(void) sprintf(tmphold, "%s%s", basedir, TMPHOLD);
		(void) sprintf(devfs_root, "%s%s", basedir, DEVFS_ROOT);

	}

	return (NOERR);
}
