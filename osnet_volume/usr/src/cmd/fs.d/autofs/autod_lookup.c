/*
 *	autod_lookup.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autod_lookup.c	1.8	97/10/20 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "automount.h"

extern (*getmapent_fn)(char *, char *, char *, uid_t,
	bool_t, getmapent_fn_res *);

do_lookup1(mapname, key, subdir, mapopts, path, isdirect, action, linkp, cred)
	char *mapname;
	char *key;
	char *subdir;
	char *mapopts;
	char *path;
	u_int isdirect;
	enum autofs_action *action;
	struct linka *linkp;
	struct authunix_parms *cred;
{
	struct mapline ml;
	struct mapent *mapents = NULL;
	getmapent_fn_res fnres;
	int err;
	struct rddir_cache *rdcp;
	int found = 0;
	bool_t iswildcard = FALSE;

#ifdef lint
	path = path;
#endif /* lint */

	/*
	 * Default action is for no work to be done by kernel AUTOFS.
	 */
	*action = AUTOFS_NONE;

	/*
	 * Is there a cache for this map?
	 */
	rw_rdlock(&rddir_cache_lock);
	err = rddir_cache_lookup(mapname, &rdcp);
	if (!err && rdcp->full) {
		rw_unlock(&rddir_cache_lock);
		/*
		 * Try to lock readdir cache entry for reading, if
		 * the entry can not be locked, then avoid blocking
		 * and go to the name service. I'm assuming it is
		 * faster to go to the name service than to wait for
		 * the cache to be populated.
		 */
		if (rw_tryrdlock(&rdcp->rwlock) == 0) {
			found = (rddir_entry_lookup(key, rdcp->entp) != NULL);
			rw_unlock(&rdcp->rwlock);
		}
	} else
		rw_unlock(&rddir_cache_lock);

	if (!err) {
		/*
		 * release reference on cache entry
		 */
		mutex_lock(&rdcp->lock);
		rdcp->in_use--;
		assert(rdcp->in_use >= 0);
		mutex_unlock(&rdcp->lock);
	}

	if (found)
		return (0);

	/*
	 * entry not found in cache, try the name service now
	 */
	err = 0;
	if (strncmp(mapname, FNPREFIX, FNPREFIXLEN) == 0) {
		getmapent_fn(key, mapname, mapopts, cred->aup_uid, TRUE,
		    &fnres);
		switch (fnres.type) {
		case FN_MAPENTS:
			mapents = fnres.m_or_l.mapents;
			break;
		case FN_SYMLINK:
			*action = AUTOFS_LINK_RQ;
			linkp->link = fnres.m_or_l.symlink;
			if (linkp->link == NULL) {
				return (ENOENT);
			}
			linkp->dir = strdup(key);
			if (linkp->dir == NULL) {
				syslog(LOG_ERR, "Memory allocation failed");
				return (ENOMEM);
			}
			break;
		case FN_NONE:	/* fall through */
		default:
			err = ENOENT;
			goto done;
		}
	} else {
		char *stack[STACKSIZ];
		char **stkptr = stack;

		/* initialize the stack of open files for this thread */
		stack_op(INIT, NULL, stack, &stkptr);

		err = getmapent(key, mapname, &ml, stack, &stkptr, &iswildcard);
		if (err == 0) /* call parser w default mount_access = TRUE */
			mapents = parse_entry(key, mapname, mapopts, &ml,
				subdir, isdirect, TRUE);
	}

	/*
	 * Now we indulge in a bit of hanky-panky.
	 * If the entry isn't found in the map and the
	 * name begins with an "=" then we assume that
	 * the name is an undocumented control message
	 * for the daemon.  This is accessible only
	 * to superusers.
	 */
	if (mapents == NULL && *action == AUTOFS_NONE) {
		if (*key == '=' && cred->aup_uid == 0) {
			if (isdigit(*(key+1))) {
				/*
				 * If next character is a digit
				 * then set the trace level.
				 */
				trace = atoi(key+1);
				trace_prt(1, "Automountd: trace level = %d\n",
					trace);
			} else if (*(key+1) == 'v') {
				/*
				 * If it's a "v" then
				 * toggle verbose mode.
				 */
				verbose = !verbose;
				trace_prt(1, "Automountd: verbose %s\n",
						verbose ? "on" : "off");
			}
		}

		err = ENOENT;
		goto done;
	}

	/*
	 * Each mapent in the list describes a mount to be done.
	 * Since I'm only doing a lookup, I only care whether a mapentry
	 * was found or not. The mount will be done on a later RPC to
	 * do_mount1.
	 */
	if (mapents == NULL && *action == AUTOFS_NONE)
		err = ENOENT;

done:	if (mapents)
		free_mapent(mapents);

	if (*action == AUTOFS_NONE && (iswildcard == TRUE)) {
		*action = AUTOFS_MOUNT_RQ;
	}
	if (trace > 1) {
		trace_prt(1, "  do_lookup1: action=%d wildcard=%s error=%d\n",
			*action, iswildcard ? "TRUE" : "FALSE", err);
	}
	return (err);
}
