/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getprof.c	1.1	99/04/07 SMI"

/*
 * Routines to handle getprof* calls in nscd
 */

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/door.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread.h>
#include <unistd.h>
#include <nss_common.h>

#include <prof_attr.h>

#include <getxby_door.h>
#include "server_door.h"
#include "nscd.h"

extern profstr_t *_getprofnam(const char *, profstr_t *, char *, int, int *);

static hash_t * uid_hash;
static hash_t * nam_hash;
static mutex_t  db_lock = DEFAULTMUTEX;
static waiter_t db_wait;

extern	admin_t	current_admin;

static getprof_namekeepalive(int keep, int interval);
static int update_prof_bucket(nsc_bucket_t **old, nsc_bucket_t *new,
    int callnumber);
static nsc_bucket_t *fixbuffer(nsc_return_t *in, int maxlen);
static void do_findgnams(nsc_bucket_t *ptr, int *table, char *gnam);
static void do_invalidate(nsc_bucket_t **ptr, int callnumber);
static void getprof_invalidate_unlocked(void);

void
getprof_init()
{
	nam_hash = make_hash(current_admin.prof.nsc_suggestedsize);
}

static void
do_invalidate(nsc_bucket_t ** ptr, int callnumber)
{
	if (*ptr != NULL && *ptr != (nsc_bucket_t *) -1) {
		/* leave pending calls alone */
		update_prof_bucket(ptr, NULL, callnumber);
	}
}

static void
do_findgnams(nsc_bucket_t *ptr, int * table, char * gnam)
{

	/*
	 * be careful with ptr - it may be -1 or NULL.
	 */

	if (ptr != NULL && ptr != (nsc_bucket_t *)-1) {
		char *tmp = (char *) insertn(table, ptr->nsc_hits,
		    (int)strdup(gnam));
		if (tmp != (char *) -1)
			free(tmp);
	}
}

void
getprof_revalidate()
{
	/*CONSTCOND*/
	while (1) {
		int slp;
		int interval;
		int count;

		slp = current_admin.prof.nsc_pos_ttl;

		if (slp < 60) {
			slp = 60;
		}

		if ((count = current_admin.prof.nsc_keephot) != 0) {
			interval = (slp / 2)/count;
			if (interval == 0) interval = 1;
			sleep(slp * 2 / 3);
			getprof_namekeepalive(count, interval);
		} else {
			sleep(slp);
		}
	}
}

static
getprof_namekeepalive(int keep, int interval)
{
	int * table;
	union {
		nsc_data_t  ping;
		char space[sizeof (nsc_data_t) + NSCDMAXNAMELEN];
	} u;

	int i;

	if (!keep)
		return (0);

	table = maken(keep);
	mutex_lock(&db_lock);
	operate_hash(nam_hash, do_findgnams, (char *)table);
	mutex_unlock(&db_lock);

	for (i = 1; i <= keep; i++) {
		char * tmp;
		u.ping.nsc_call.nsc_callnumber = GETPROFNAM;

		if ((tmp = (char *)table[keep + 1 + i]) == (char *)-1)
			continue; /* unused slot in table */

		strcpy(u.ping.nsc_call.nsc_u.name, tmp);

		launch_update(&u.ping.nsc_call);
		sleep(interval);
	}

	for (i = 1; i <= keep; i++) {
		char * tmp;
		if ((tmp = (char *)table[keep + 1 + i]) != (char *)-1)
			free(tmp);
	}

	free(table);
	return (0);
}


/*
 *   This routine marks all entries as invalid
 *
 */

void
getprof_invalidate()
{
	mutex_lock(&db_lock);
	getprof_invalidate_unlocked();
	mutex_unlock(&db_lock);
}

static void
getprof_invalidate_unlocked()
{
	operate_hash_addr(nam_hash, do_invalidate, (char *)GETPROFNAM);
}

int
getprof_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in, time_t now)
{
	int		out_of_date;
	nsc_bucket_t	*retb;
	char 		**bucket;

	static time_t	lastmod;

	int bufferspace = maxsize - sizeof (nsc_return_t);

	if (current_admin.prof.nsc_enabled == 0) {
		out->nsc_return_code = NOSERVER;
		out->nsc_bufferbytesused = sizeof (*out);
		return (0);
	}

	mutex_lock(&db_lock);

	if (current_admin.prof.nsc_check_files) {
		struct stat buf;

		if (stat(PROFATTR_FILENAME, &buf) < 0) {
			/*EMPTY*/;
		} else if (lastmod == 0) {
			lastmod = buf.st_mtime;
		} else if (lastmod < buf.st_mtime) {
			getprof_invalidate_unlocked();
			lastmod = buf.st_mtime;
		}
	}

	if (current_admin.debug_level >= DBG_ALL) {
		logit("getprof_lookup: looking for name %s\n",
				in->nsc_u.name);
	}

	/*CONSTCOND*/
	while (1) {
		if (attr_strlen(in->nsc_u.name) > NSCDMAXNAMELEN) {
			door_cred_t dc;

			if (_door_cred(&dc) < 0) {
				perror("door_cred");
			}

		logit("getprof_lookup: Name too long from pid %d uid %d\n",
			    dc.dc_pid, dc.dc_ruid);

			out->nsc_errno = NSS_NOTFOUND;
			out->nsc_return_code = NOTFOUND;
			out->nsc_bufferbytesused = sizeof (*out);
			goto getout;
		}
		bucket = get_hash(nam_hash, in->nsc_u.name);

		if (*bucket == (char *) -1) {	/* pending lookup */
			if (get_clearance(in->nsc_callnumber) != 0) {
			    /* no threads available */
				out->nsc_return_code = NOSERVER;
				    /* cannot process now */
				out->nsc_bufferbytesused =
				    sizeof (*out);
				current_admin.prof.nsc_throttle_count++;
				goto getout;
			}
			nscd_wait(&db_wait, &db_lock, bucket);
			release_clearance(in->nsc_callnumber);
			continue; /* go back and relookup hash bucket */
		}
		break;
	}

	/*
	 * check for no name_service mode
	 */

	if (*bucket == NULL && current_admin.avoid_nameservice) {
		out->nsc_return_code = NOTFOUND;
		out->nsc_bufferbytesused = sizeof (*out);
	} else if ((*bucket == NULL) || /* New entry in name service */
	    (in->nsc_callnumber & UPDATEBIT) || /* needs updating */
	    (out_of_date = (!current_admin.avoid_nameservice &&
	    (current_admin.prof.nsc_old_data_ok == 0) &&
	    (((nsc_bucket_t *)*bucket)->nsc_timestamp < now)))) {
		/* time has expired */
		int saved_errno;
		int saved_hits = 0;
		profstr_t *p;

		if (get_clearance(in->nsc_callnumber) != 0) {
		    /* no threads available */
			out->nsc_return_code = NOSERVER;
			    /* cannot process now */
			out->nsc_bufferbytesused = sizeof (*out);
			current_admin.prof.nsc_throttle_count++;
			goto getout;
		}

		if (*bucket != NULL) {
			saved_hits =
			    ((nsc_bucket_t *)*bucket)->nsc_hits;
		}

		/*
		 * block any threads accessing this bucket if data is
		 * non-existent out of date
		 */

		if (*bucket == NULL || out_of_date) {
			update_prof_bucket((nsc_bucket_t **)bucket,
			    (nsc_bucket_t *)-1, in->nsc_callnumber);
		} else {
		/*
		 * if still not -1 bucket we are doing update...
		 * mark to prevent
		 * pileups of threads if the name service is hanging....
		 */
			((nsc_bucket_t *)(*bucket))->nsc_status |=
			    ST_UPDATE_PENDING;
			/* cleared by deletion of old data */
		}
		mutex_unlock(&db_lock);

		/*
		 * Call non-caching version in libnsl.
		 */
		p = _getprofnam(in->nsc_u.name, &out->nsc_u.prof,
		    out->nsc_u.buff + sizeof (profstr_t),
		    bufferspace, &errno);
		saved_errno = errno;

		mutex_lock(&db_lock);

		release_clearance(in->nsc_callnumber);

		if (p == NULL) { /* data not found */

			if (current_admin.debug_level >= DBG_CANT_FIND) {
		logit("getprof_lookup: nscd COULDN'T FIND prof_attr name %s\n",
						in->nsc_u.name);
			}

			if (!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.prof.nsc_neg_cache_misses++;

			retb = (nsc_bucket_t *) malloc(sizeof (nsc_bucket_t));

			retb->nsc_refcount = 1;
			retb->nsc_data.nsc_bufferbytesused =
				sizeof (nsc_return_t);
			retb->nsc_data.nsc_return_code = NOTFOUND;
			retb->nsc_data.nsc_errno = saved_errno;
			memcpy(out, &retb->nsc_data,
			    retb->nsc_data.nsc_bufferbytesused);
			update_prof_bucket((nsc_bucket_t **)bucket,
			    retb, in->nsc_callnumber);
			goto getout;
		} else {
			if (current_admin.debug_level >= DBG_ALL) {
		logit("getprof_lookup: nscd FOUND prof_attr name %s\n",
						in->nsc_u.name);
			}
			if (!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.prof.nsc_pos_cache_misses++;

			retb = fixbuffer(out, bufferspace);
			update_prof_bucket((nsc_bucket_t **)bucket,
			    retb, in->nsc_callnumber);
			if (saved_hits)
				retb->nsc_hits = saved_hits;
		}
	} else { 	/* found entry in cache */
		retb = (nsc_bucket_t *)*bucket;

		retb->nsc_hits++;

		memcpy(out, &(retb->nsc_data),
		    retb->nsc_data.nsc_bufferbytesused);

		if (out->nsc_return_code == SUCCESS) {
			if (!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.prof.nsc_pos_cache_hits++;
			if (current_admin.debug_level >= DBG_ALL) {
			logit("getprof_lookup: found name %s in cache\n",
						in->nsc_u.name);
			}
		} else {
			if (!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.prof.nsc_neg_cache_hits++;
			if (current_admin.debug_level >= DBG_ALL) {
		logit("getprof_lookup: %s marked as NOT FOUND in cache.\n",
						in->nsc_u.name);
			}
		}

		if ((retb->nsc_timestamp < now) &&
		    !(in->nsc_callnumber & UPDATEBIT) &&
		    !(retb->nsc_status & ST_UPDATE_PENDING)) {
		logit("launch update since time = %d\n", retb->nsc_timestamp);
			retb->nsc_status |= ST_UPDATE_PENDING;
			/* cleared by deletion of old data */
			launch_update(in);
		}
	}

getout:
	mutex_unlock(&db_lock);
	if ((current_admin.prof.nsc_secure_mode != 0) &&
	    (out->nsc_return_code == SUCCESS) &&
	    !(UPDATEBIT & in->nsc_callnumber)) {
		int ok = 0;
		door_cred_t dc;

		if (_door_cred(&dc) < 0) {
			perror("door_cred");
		}
	}
	return (0);
}

/*ARGSUSED*/
static int
update_prof_bucket(nsc_bucket_t ** old, nsc_bucket_t * new, int callnumber)
{
	if (*old != NULL && *old != (nsc_bucket_t *) -1) { /* old data exists */
		free(*old);
		current_admin.prof.nsc_entries--;
	}

	/*
	 *  we can do this before reseting *old since we're holding the lock
	 */

	else if (*old == (nsc_bucket_t *) -1) {
		nscd_signal(&db_wait, (char **) old);
	}

	*old = new;

	if ((new != NULL) && (new != (nsc_bucket_t *) -1)) {
		/* real data, not just update pending or invalidate */
		new->nsc_hits = 1;
		new->nsc_status = 0;
		new->nsc_refcount = 1;
		current_admin.prof.nsc_entries++;

		if (new->nsc_data.nsc_return_code == SUCCESS) {
			new->nsc_timestamp = time(NULL) +
			    current_admin.prof.nsc_pos_ttl;
		} else {
			new->nsc_timestamp = time(NULL) +
			    current_admin.prof.nsc_neg_ttl;
		}
	}
	return (0);
}

/*ARGSUSED*/
static nsc_bucket_t *
fixbuffer(nsc_return_t * in, int maxlen)
{
	nsc_bucket_t *retb;
	nsc_return_t *out;
	char 	*dest;
	int 	offset;
	int 	strs;
	char ** members;
	int pwlen;

	/*
	 * find out the size of the data block we're going to need
	 */

	strs = attr_strlen(in->nsc_u.prof.name) +
	    attr_strlen(in->nsc_u.prof.res1) +
	    attr_strlen(in->nsc_u.prof.res2) +
	    attr_strlen(in->nsc_u.prof.desc) +
	    attr_strlen(in->nsc_u.prof.attr) + PROFATTR_DB_NCOL;

	/*
	 * allocate it and copy it in
	 * code doesn't assume packing order in original buffer
	 */

	if ((retb = (nsc_bucket_t *) malloc(sizeof (*retb) + strs)) == NULL) {
		return (NULL);
	}

	out = &(retb->nsc_data);
	out->nsc_bufferbytesused = strs + ((int)&out->nsc_u.prof - (int)out) +
	    sizeof (profstr_t);
	out->nsc_return_code 	= SUCCESS;
	out->nsc_errno 		= 0;

	dest = retb->nsc_data.nsc_u.buff + sizeof (profstr_t);
	offset = (int) dest;

	attr_strcpy(dest, in->nsc_u.prof.name);
	strs = 1 + attr_strlen(in->nsc_u.prof.name);
	out->nsc_u.prof.name = dest - offset;
	dest += strs;

	attr_strcpy(dest, in->nsc_u.prof.res1);
	strs = 1 + attr_strlen(in->nsc_u.prof.res1);
	out->nsc_u.prof.res1 = dest - offset;
	dest += strs;

	attr_strcpy(dest, in->nsc_u.prof.res2);
	strs = 1 + attr_strlen(in->nsc_u.prof.res2);
	out->nsc_u.prof.res2 = dest - offset;
	dest += strs;

	attr_strcpy(dest, in->nsc_u.prof.desc);
	strs = 1 + attr_strlen(in->nsc_u.prof.desc);
	out->nsc_u.prof.desc = dest - offset;
	dest += strs;

	attr_strcpy(dest, in->nsc_u.prof.attr);
	out->nsc_u.prof.attr = dest - offset;

	memcpy(in, out, out->nsc_bufferbytesused);

	return (retb);
}
