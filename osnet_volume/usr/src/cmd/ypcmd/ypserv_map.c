/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ypserv_map.c	1.15	99/04/27 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *          All rights reserved.
 */

#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>
#include "ypsym.h"
#include "ypdefs.h"

USE_YP_MASTER_NAME
USE_YP_LAST_MODIFIED
USE_YPDBPATH
USE_YP_SECURE
USE_DBM

#include <ctype.h>
static char current_map[sizeof (ypdbpath) + YPMAXDOMAIN + YPMAXMAP + 3];
static DBM *cur_fdb; /* will be passwd back up by ypset_current_map */
static enum { UNKNOWN, SECURE, PUBLIC } current_map_access;
static char map_owner[MAX_MASTER_NAME + 1];

extern unsigned int ypcheck_domain();
int check_secure_net_ti(struct netbuf *caller, char *ypname);


/*
 * This performs an existence check on the dbm data base files <name>.pag and
 * <name>.dir.  pname is a ptr to the filename.  This should be an absolute
 * path.
 * Returns TRUE if the map exists and is accessable; else FALSE.
 *
 * Note:  The file name should be a "base" form, without a file "extension" of
 * .dir or .pag appended.  See ypmkfilename for a function which will generate
 * the name correctly.  Errors in the stat call will be reported at this level,
 * however, the non-existence of a file is not considered an error, and so will
 * not be reported.
 */
bool
ypcheck_map_existence(char *pname)
{
	char dbfile[MAXNAMLEN + 1];
	struct stat filestat;
	int len;

	if (!pname || ((len = (int)strlen(pname)) == 0) ||
		(len + sizeof (dbm_pag)) > (MAXNAMLEN + 1)) {
		return (FALSE);
	}

	errno = 0;
	(void) strcpy(dbfile, pname);
	(void) strcat(dbfile, dbm_dir);

	if (stat(dbfile, &filestat) != -1) {
		(void) strcpy(dbfile, pname);
		(void) strcat(dbfile, dbm_pag);

		if (stat(dbfile, &filestat) != -1) {
			return (TRUE);
		} else {

		    if (errno != ENOENT) {
			(void) fprintf(stderr,
					"ypserv:  Stat error on map file %s.\n",
					dbfile);
		    }

		    return (FALSE);
		}

	} else {

		if (errno != ENOENT) {
		    (void) fprintf(stderr,
					"ypserv:  Stat error on map file %s.\n",
					dbfile);
		}

		return (FALSE);
	}
}

/*
 * The retrieves the order number of a named map from the order number datum
 * in the map data base.
 */
bool
ypget_map_order(char *map, char *domain, uint_t *order)
{
	datum key;
	datum val;
	char toconvert[MAX_ASCII_ORDER_NUMBER_LENGTH + 1];
	uint_t error;
	DBM *fdb;

	if ((fdb = ypset_current_map(map, domain, &error)) != NULL) {
		key.dptr = yp_last_modified;
		key.dsize = yp_last_modified_sz;
		val = dbm_fetch(fdb, key);

		if (val.dptr != (char *) NULL) {

			if (val.dsize > MAX_ASCII_ORDER_NUMBER_LENGTH) {
			return (FALSE);
			}

			/*
			 * This is getting recopied here because val.dptr
			 * points to static memory owned by the dbm package,
			 * and we have no idea whether numeric characters
			 * follow the order number characters, nor whether
			 * the mess is null-terminated at all.
			 */

			memcpy(toconvert, val.dptr, val.dsize);
			toconvert[val.dsize] = '\0';
			*order = (unsigned long) atol(toconvert);
			return (TRUE);
		} else {
			return (FALSE);
		}

	} else {
		return (FALSE);
	}
}

/*
 * The retrieves the master server name of a named map from the master datum
 * in the map data base.
 */
bool
ypget_map_master(char **owner, DBM *fdb)
{
	datum key;
	datum val;

	key.dptr = yp_master_name;
	key.dsize = yp_master_name_sz;
	val = dbm_fetch(fdb, key);

	if (val.dptr != (char *) NULL) {

		if (val.dsize > MAX_MASTER_NAME) {
			return (FALSE);
		}

		/*
		 * This is getting recopied here because val.dptr
		 * points to static memory owned by the dbm package.
		 */
		memcpy(map_owner, val.dptr, val.dsize);
		map_owner[val.dsize] = '\0';
		*owner = map_owner;
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/*
 * This makes a map into the current map, and calls dbminit on that map
 * and returns the DBM pointer to the map. Procedures called by
 * ypserv dispatch routine would use this pointer for successive
 * ndbm operations.  Returns an YP_xxxx error code in error if FALSE.
 */
DBM *
ypset_current_map(char *map, char *domain, uint_t *error)
{
	char mapname[sizeof (current_map)];
	int lenm, lend;

	if (!map || ((lenm = (int)strlen(map)) == 0) || (lenm > YPMAXMAP) ||
		!domain || ((lend = (int)strlen(domain)) == 0) ||
		(lend > YPMAXDOMAIN) || (strchr(domain, '/') != NULL)) {
		*error = YP_BADARGS;
		return (FALSE);
	}

	ypmkfilename(domain, map, mapname);

	if ((strcmp(mapname, current_map) == 0) && (cur_fdb)) {
		return (cur_fdb);
	}

	ypclr_current_map();
	current_map_access = UNKNOWN;

	if (!lock_map(mapname)) {
		*error = YP_YPERR;
		return (NULL);
	}

	if ((cur_fdb = dbm_open(mapname, O_RDWR, 0644)) != NULL) {
		unlock_map(mapname);
		strcpy(current_map, mapname);
		return (cur_fdb);
	}

	ypclr_current_map();

	if (ypcheck_domain(domain)) {

		if (ypcheck_map_existence(mapname)) {
			*error = YP_BADDB;
		} else {
			*error = YP_NOMAP;
		}

	} else {
		*error = YP_NODOM;
	}
	unlock_map(mapname);

	return (NULL);
}

/*
 * This checks to see if there is a current map, and, if there is, does a
 * dbmclose on it and sets the current map name and its DBM ptr to null.
 */
void
ypclr_current_map(void)
{
	if (cur_fdb != NULL) {
		(void) dbm_close(cur_fdb);
		cur_fdb = NULL;
	}
	current_map[0] = '\0';
}

/*
 * Checks to see if caller has permission to query the current map (as
 * set by ypset_current_map()).  Returns TRUE if access is granted and
 * FALSE otherwise.  If FALSE then sets *error to YP_xxxxxxxx.
 */
bool
yp_map_access(SVCXPRT *transp, uint_t *error, DBM *fdb)
{
	char *ypname = "ypserv";
	struct netbuf *nbuf;
	sa_family_t af;
	in_port_t port;

	nbuf = svc_getrpccaller(transp);
	af = ((struct sockaddr_storage *)nbuf->buf)->ss_family;
	if (af != AF_INET && af != AF_INET6)
		return (FALSE);

	if (!(check_secure_net_ti(nbuf, ypname))) {
		*error = YP_NOMAP;
		return (FALSE);
	}

	/* XXX - I expect that this won't happen much */
	if (current_map_access == PUBLIC) {
		return (TRUE);
	}

	if (af == AF_INET6) {
		port = ntohs(((struct sockaddr_in6 *)nbuf->buf)->sin6_port);
	} else {
		port = ntohs(((struct sockaddr_in *)nbuf->buf)->sin_port);
	}
	if (port < IPPORT_RESERVED) {
		return (TRUE);
	}

	if (current_map_access == UNKNOWN) {
		datum key;
		datum val;

		key.dptr = yp_secure;
		key.dsize = yp_secure_sz;
		val = dbm_fetch(fdb, key);
		if (val.dptr == (char *) NULL) {
			current_map_access = PUBLIC;
			return (TRUE);
		}
		current_map_access = SECURE;
	}

	/* current_map_access == SECURE and non-priviledged caller */
	*error = YP_NOMAP;
	return (FALSE);
}
