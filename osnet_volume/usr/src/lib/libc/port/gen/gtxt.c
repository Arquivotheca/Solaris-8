/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gtxt.c	1.14	99/05/04 SMI"

/*LINTLIBRARY*/

/* __gtxt(): Common part to gettxt() and pfmt()	*/

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <string.h>
#include <locale.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <synch.h>
#include <pfmt.h>
#include <thread.h>
#include <unistd.h>
#include "../i18n/_locale.h"
#include "../i18n/_loc_path.h"

#define	P_locale	_DFLT_LOC_PATH
#define	L_locale	(sizeof (P_locale))
#define	DEF_LOCALE (const char *)"C"
#define	MESSAGES "/LC_MESSAGES/"
static const char *not_found = "Message not found!!\n";
static struct db_info *db_info;
static int db_count, maxdb;

#define	FALSE			0
#define	TRUE			1

struct db_info {
	char	db_name[DB_NAME_LEN];	/* Name of the message file */
	uintptr_t	addr;		/* Virtual memory address   */
	size_t	length;
	int	fd;
	char	saved_locale[15];
	char	flag;
};

#define	DB_EXIST	1		/* The catalogue exists	   */
#define	DB_OPEN		2		/* Already tried to open   */
#define	DB_DFLT		4		/* In default locale	   */

/* Minimum number of open catalogues */
#define	MINDB			3

static char cur_cat[DB_NAME_LEN];
static rwlock_t _rw_cur_cat = DEFAULTRWLOCK;


/*
 * setcat(cat): Specify the default catalogue.
 * Return a pointer to the local copy of the default catalogue
 */
const char *
setcat(const char *cat)
{
	(void) rw_wrlock(&_rw_cur_cat);
	if (cat) {
		if (((strchr(cat, '/') != NULL)) ||
		    ((strchr(cat, ':') != NULL))) {
			cur_cat[0] = '\0';
			goto out;
		}
		(void) strncpy(cur_cat, cat, sizeof (cur_cat) - 1);
		cur_cat[sizeof (cur_cat) - 1] = '\0';
	}
out:
	(void) rw_unlock(&_rw_cur_cat);
	return (cur_cat[0] ? cur_cat : NULL);
}

/*
 * __gtxt(catname, id, dflt): Return a pointer to a message.
 *	catname is the name of the catalog. If null, the default catalog is
 *		used.
 *	id is the numeric id of the message in the catalogue
 *	dflt is the default message.
 *
 *	Information about non-existent catalogues is kept in db_info, in
 *	such a way that subsequent calls with the same catalogue do not
 *	try to open the catalogue again.
 */
const char *
__gtxt(const char *catname, int id, const char *dflt)
{
	char pathname[128];
	int i;
	int fd;
	struct	stat64 sb;
	caddr_t addr;
	char cur_flag;
	char	*curloc;
	struct db_info *db;
	/* Check for invalid message id */
	if (id < 0)
		return (not_found);

	/* First time called, allocate space */
	if (!db_info) {
		if ((db_info = (struct db_info *)
			malloc(MINDB * sizeof (struct db_info))) == 0)
			return (not_found);
		maxdb = MINDB;
	}

	/*
	 * If catalogue is unspecified, use default catalogue.
	 * No catalogue at all is an error
	*/
	if (!catname || !*catname) {
		(void) rw_rdlock(&_rw_cur_cat);
		if (cur_cat == NULL || !*cur_cat) {
			(void) rw_unlock(&_rw_cur_cat);
			return (not_found);
		}
		catname = cur_cat;
		(void) rw_unlock(&_rw_cur_cat);
	}

	/* Retrieve catalogue in the table */
	for (i = 0, cur_flag = 0; ; ) {
		for (; i < db_count; i++) {
			if (strcmp(catname, db_info[i].db_name) == 0)
				break;
		}
		/* New catalogue */
		if (i == db_count) {
			if (db_count == maxdb) {
				if ((db_info = (struct db_info *)
					realloc(db_info,
					++maxdb * sizeof (struct db_info))) ==
					0)
					return (not_found);
			}
			(void) strcpy(db_info[i].db_name, catname);
			db_info[i].flag = cur_flag;
			db_info[i].saved_locale[0] = '\0';
			db_count++;
		}
		db = &db_info[i];

		/*
		 * Check for a change in locale. If necessary unmap and close
		 * the opened catalogue. The entry in the table is
		 * NOT freed. The catalogue offset remains valid.
		*/
		curloc = setlocale(LC_MESSAGES, NULL);
		if (strcmp(curloc, db->saved_locale) != 0) {
			if (db->flag & (DB_OPEN|DB_EXIST) ==
				(DB_OPEN|DB_EXIST)) {
				(void) munmap((caddr_t)db->addr, db->length);
				(void) close(db->fd);
			}
			db->flag &= ~(DB_OPEN|DB_EXIST);
		}

		/* Retrieve the message from the catalogue */
		for (;;) {
			/*
			 * Open and map catalogue if not done yet. In case of
			 * failure, mark the catalogue as non-existent
			*/
			if (!(db->flag & DB_OPEN)) {
				db->flag |= DB_OPEN;
				(void) strcpy(db->saved_locale,
					curloc);
				(void) strcpy(pathname, P_locale);
				(void) strcpy(pathname + L_locale - 1,
					(db->flag & DB_DFLT) ?
					DEF_LOCALE : db->saved_locale);
				(void) strcat(pathname, MESSAGES);
				(void) strcat(pathname, db->db_name);
				if ((fd = open(pathname, O_RDONLY)) == -1 ||
					fstat64(fd, &sb) == -1 ||
					(addr = mmap(0, (size_t)sb.st_size,
						PROT_READ, MAP_SHARED,
						fd, 0)) == (caddr_t)-1) {
					if (fd != -1)
						(void) close(fd);
				} else {
					db->flag |= DB_EXIST;
					db->fd = fd;
					db->addr = (uintptr_t)addr;
					db->length = (size_t)sb.st_size;
				}
			}
			/* Return the message from the catalogue */
			if (id != 0 && db->flag & DB_EXIST &&
				id <= *(int *)(db->addr)) {
				return ((char *)(db->addr + *(int *)(db->addr +
					id * sizeof (int))));
			}
			/*
			 * Return the default message(or 'Message not found'
			 * if no default message was passed
			*/

			if (db->flag & DB_DFLT ||
				    strcmp(db->saved_locale, DEF_LOCALE) == 0)
				return ((dflt && *dflt) ? dflt : not_found);
			if (!(db->flag & DB_EXIST)) {
				db->flag |= DB_DFLT;
				continue;
			}
			break;
		}
		cur_flag = DB_DFLT;
		i++;
	}
}
