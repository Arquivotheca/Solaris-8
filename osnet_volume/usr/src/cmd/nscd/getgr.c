/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getgr.c	1.12	99/10/11 SMI"

/*
 * Routines to handle getgr* calls in nscd
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

#include <getxby_door.h>
#include "server_door.h"
#include "nscd.h"

static hash_t * uid_hash;
static hash_t * nam_hash;
static mutex_t  group_lock = DEFAULTMUTEX;
static waiter_t group_wait;

extern	admin_t	current_admin;

static getgr_gidkeepalive( int keep, int interval );
static getgr_namekeepalive( int keep, int interval );
static int update_gr_bucket( nsc_bucket_t ** old, nsc_bucket_t * new, int callnumber );
static nsc_bucket_t * fixbuffer( nsc_return_t * in, int maxlen );
static void do_findgids( nsc_bucket_t *ptr, int * table, int gid );
static void do_findgnams( nsc_bucket_t *ptr, int * table, char * gnam );
static void do_invalidate( nsc_bucket_t ** ptr, int callnumber );
static void getgr_invalidate_unlocked( void );


void
getgr_init()
{
	uid_hash = make_ihash(current_admin.group.nsc_suggestedsize);
	nam_hash = make_hash(current_admin.group.nsc_suggestedsize);

}

static void
do_invalidate(nsc_bucket_t ** ptr, int callnumber)
{
	if (*ptr != NULL && *ptr != (nsc_bucket_t *) -1) {
		/* leave pending calls alone */
		update_gr_bucket(ptr, NULL, callnumber);
	}
				      
}

static void 
do_findgids(nsc_bucket_t *ptr, int * table, int gid)
{

	/*
	 * be careful with ptr - it may be -1 or NULL.
	 */
	if(ptr != NULL && ptr != (nsc_bucket_t*)-1) {
		insertn(table, ptr->nsc_hits, gid);
	} 
}

static void 
do_findgnams(nsc_bucket_t *ptr, int * table, char * gnam)
{

	/*
	 * be careful with ptr - it may be -1 or NULL.
	 */

	if(ptr != NULL && ptr != (nsc_bucket_t*)-1) {
		char * tmp = (char *) insertn(table, ptr->nsc_hits, (int)strdup(gnam));
		if (tmp != (char *) -1)
			free(tmp);
	}
}

void
getgr_revalidate()
{
	/*CONSTCOND*/
	while (1) {
		int slp;
		int interval;
		int count;

		slp = current_admin.group.nsc_pos_ttl;

		if (slp < 60) {
			slp = 60;
		}
		
		if((count = current_admin.group.nsc_keephot)!=0) {
			interval = (slp / 2)/count;
			if (interval == 0) interval = 1;
			sleep(slp * 2 / 3);
			getgr_gidkeepalive(count, interval);
			getgr_namekeepalive(count, interval);
		}
		else {
			sleep(slp);
		}
	}
}

static
getgr_gidkeepalive(int keep, int interval)
{
	int * table;
	nsc_data_t  ping;
	int i;

	if (!keep) 
		return(0);

	table = maken(keep);
	mutex_lock(&group_lock);
	operate_hash(uid_hash, do_findgids, (char*)table);
	mutex_unlock(&group_lock);
	
	for (i = 1;i <= keep; i++) {
	    ping.nsc_call.nsc_callnumber = GETGRGID;
	    if ((ping.nsc_call.nsc_u.gid = table[keep + 1 + i])==-1)
		continue; /* unused slot in table */
	    launch_update(&ping.nsc_call); 
	    sleep(interval);
	}
	free(table);
	return(0);
}

static 
getgr_namekeepalive(int keep, int interval)
{
	int * table;
	union {
		nsc_data_t  ping;
		char space[sizeof(nsc_data_t) + NSCDMAXNAMELEN];
	} u;

	int i;

	if(!keep) 
		return(0);

	table = maken(keep);
	mutex_lock(&group_lock);
	operate_hash(nam_hash, do_findgnams, (char*)table);
	mutex_unlock(&group_lock);
	
	for(i=1;i<=keep;i++) {
		char * tmp;
		u.ping.nsc_call.nsc_callnumber = GETGRNAM;
	    
		if((tmp = (char*)table[keep + 1 + i])==(char*)-1)
			continue; /* unused slot in table */

		strcpy(u.ping.nsc_call.nsc_u.name, tmp);
		
		launch_update(&u.ping.nsc_call); 
		sleep(interval);
	}

	for(i=1;i<=keep;i++) {
		char * tmp;
		if((tmp = (char*)table[keep + 1 + i])!=(char*)-1)
			free(tmp);
	}

	free(table);
	return(0);
}


/*
 *   This routine marks all entries as invalid
 *
 */

void
getgr_invalidate()
{
	mutex_lock(&group_lock);
	getgr_invalidate_unlocked();
	mutex_unlock(&group_lock);
}

static void
getgr_invalidate_unlocked()
{
	operate_hash_addr(nam_hash, do_invalidate, (char*)GETGRNAM);
	operate_hash_addr(uid_hash, do_invalidate, (char*)GETGRGID);
}

int
getgr_lookup(nsc_return_t *out, int maxsize, nsc_call_t * in, time_t now)
{
	int		out_of_date;
	nsc_bucket_t	*retb;
	char 		**bucket;

	static time_t	lastmod;

	int bufferspace = maxsize - sizeof (nsc_return_t);
	
	if (current_admin.group.nsc_enabled == 0) {
		out->nsc_return_code = NOSERVER;
		out->nsc_bufferbytesused = sizeof (*out);
		return (0);
	}				
	
	mutex_lock(&group_lock);

	if (current_admin.group.nsc_check_files) {
		struct stat buf;
		
		if (stat("/etc/group", &buf) < 0) {
			/*EMPTY*/;
		} else if (lastmod == 0) {
			lastmod = buf.st_mtime;
		} else if (lastmod < buf.st_mtime) {
			getgr_invalidate_unlocked();
			lastmod = buf.st_mtime;
		}
	}
		
	if (current_admin.debug_level >= DBG_ALL) {
		if (MASKUPDATEBIT(in->nsc_callnumber) == GETGRGID) {
			logit("getgr_lookup: looking for gid %d\n",
				in->nsc_u.gid);
		} else {
			logit("getgr_lookup: looking for name %s\n",
				in->nsc_u.name);
		}
	}
	
	/*CONSTCOND*/
	while (1) {
		if (MASKUPDATEBIT(in->nsc_callnumber) == GETGRGID) {
			bucket = get_hash(uid_hash, (char*)in->nsc_u.gid);
		} else {
			if (strlen(in->nsc_u.name) > NSCDMAXNAMELEN) {
				door_cred_t dc;
		
				if (_door_cred(&dc) < 0) {
					perror("door_cred");
				}

				logit("getgr_lookup: Name too long from pid %d uid %d\n",
				      dc.dc_pid, dc.dc_ruid);
				
				out->nsc_errno = NSS_NOTFOUND;
				out->nsc_return_code = NOTFOUND;
				out->nsc_bufferbytesused = sizeof(*out);
				goto getout;
			}
			bucket = get_hash(nam_hash, in->nsc_u.name);
		}
		
		if (*bucket == (char *) -1) {	/* pending lookup */
		        if(get_clearance(in->nsc_callnumber) != 0) { /* no threads available */
			        out->nsc_return_code = NOSERVER; /* cannot process now */
				out->nsc_bufferbytesused = sizeof(*out);
				current_admin.group.nsc_throttle_count++;
				goto getout;
			}
			nscd_wait(&group_wait, &group_lock, bucket);
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
	} else if ((*bucket == NULL) 	/* New entry in name service */
	    || 
	    (in->nsc_callnumber & UPDATEBIT) /* needs updating */
	    || 
	    (out_of_date = (!current_admin.avoid_nameservice 
			    &&
			    (current_admin.group.nsc_old_data_ok == 0 )
			    && 
	     (((nsc_bucket_t *)*bucket)->nsc_timestamp < now)))) {
		/* time has expired */
		int saved_errno;
		int saved_hits = 0;
		struct group * p;

		if(get_clearance(in->nsc_callnumber) != 0) { /* no threads available */
		        out->nsc_return_code = NOSERVER; /* cannot process now */
			out->nsc_bufferbytesused = sizeof(*out);
			current_admin.group.nsc_throttle_count++;
			goto getout;
		}

		if (*bucket != NULL) {
			saved_hits = ((nsc_bucket_t*)*bucket)->nsc_hits;
		}

		/*
		 *  block any threads accessing this bucket if data is 
		 *  non-existent out of date
		 */

		if (*bucket == NULL || out_of_date) {
			update_gr_bucket((nsc_bucket_t **)bucket, 
					 (nsc_bucket_t *)-1,
					 in->nsc_callnumber);
		} else {
		/* 
		 * if still not -1 bucket we are doing update...
		 * mark to prevent
		 * pileups of threads if the name service is hanging....
		 */
		    ((nsc_bucket_t*)(*bucket))->nsc_status |= ST_UPDATE_PENDING; 
			/* cleared by deletion of old data */
		}
		mutex_unlock(&group_lock);
		
		if (MASKUPDATEBIT(in->nsc_callnumber) == GETGRGID) {
			p = _uncached_getgrgid_r(in->nsc_u.gid, &out->nsc_u.grp,
			    out->nsc_u.buff + sizeof (struct group),
			    bufferspace);
			saved_errno = errno;
		} else {
			p = _uncached_getgrnam_r(in->nsc_u.name, &out->nsc_u.grp,
			    out->nsc_u.buff + sizeof (struct group),
			    bufferspace);
			saved_errno = errno;
		}
		
		mutex_lock(&group_lock);
		
		release_clearance(in->nsc_callnumber);

		if (p == NULL) { /* data not found */
			
			if (current_admin.debug_level >= DBG_CANT_FIND) {
				if (MASKUPDATEBIT(in->nsc_callnumber) ==
				    GETGRGID) {
			logit("getgr_lookup: nscd COULDN'T FIND gid %d\n", 
						in->nsc_u.gid);
				} else {
		logit("getgr_lookup: nscd COULDN'T FIND group name %s\n", 
						in->nsc_u.name);
				}
			}
			
			
			if(!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.group.nsc_neg_cache_misses++;
			
			retb = (nsc_bucket_t *) malloc(sizeof (nsc_bucket_t));

			retb->nsc_refcount = 1;
			retb->nsc_data.nsc_bufferbytesused =
				sizeof (nsc_return_t);
			retb->nsc_data.nsc_return_code = NOTFOUND;
			retb->nsc_data.nsc_errno = saved_errno;
			memcpy(out, &retb->nsc_data,
			       retb->nsc_data.nsc_bufferbytesused);
			update_gr_bucket((nsc_bucket_t **)bucket, 
					 retb, 
					 in->nsc_callnumber);
			goto getout;
		} else {
			if (current_admin.debug_level >= DBG_ALL) {
				if (MASKUPDATEBIT(in->nsc_callnumber) ==
				    GETGRGID) {
		logit("getgr_lookup: nscd FOUND gid %d\n",
						in->nsc_u.gid);
				} else {
		logit("getgr_lookup: nscd FOUND group name %s\n",
						in->nsc_u.name);
				}
			}
			if(!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.group.nsc_pos_cache_misses++;
			
			retb = fixbuffer(out, bufferspace);
			update_gr_bucket((nsc_bucket_t**)bucket, 
					 retb, 
					 in->nsc_callnumber);
			if (saved_hits)
				retb->nsc_hits = saved_hits;
		}
	} else { 	/* found entry in cache */
		retb = (nsc_bucket_t *)*bucket;
		
		retb->nsc_hits++;
		
		memcpy(out, &(retb->nsc_data),
		       retb->nsc_data.nsc_bufferbytesused);
		
		if (out->nsc_return_code == SUCCESS) {
			if(!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.group.nsc_pos_cache_hits++;
			if (current_admin.debug_level >= DBG_ALL) {
				if (MASKUPDATEBIT(in->nsc_callnumber) ==
				    GETGRGID) {
			logit("getgr_lookup: found gid %d in cache\n",
						in->nsc_u.gid);
				} else {
			logit("getgr_lookup: found name %s in cache\n",
						in->nsc_u.name);
				}
			}
		} else {
			if(!(UPDATEBIT & in->nsc_callnumber))
			    current_admin.group.nsc_neg_cache_hits++;
			if (current_admin.debug_level >= DBG_ALL) {
				if (MASKUPDATEBIT(in->nsc_callnumber) ==
				    GETGRGID) {
		logit("getgr_lookup: %d marked as NOT FOUND in cache.\n",
						in->nsc_u.gid);
				} else {
		logit("getgr_lookup: %s marked as NOT FOUND in cache.\n",
						in->nsc_u.name);
				}
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
	
	mutex_unlock(&group_lock);

	return (0);
}

/*ARGSUSED*/
static int
update_gr_bucket(nsc_bucket_t ** old, nsc_bucket_t * new, int callnumber)
{
	if(*old != NULL && *old != (nsc_bucket_t *) -1) { /* old data exists */
		free(*old);
		current_admin.group.nsc_entries--;
	}       

	/*
	 *  we can do this before reseting *old since we're holding the lock
	 */

	else if(*old == (nsc_bucket_t * ) -1) {
		nscd_signal(&group_wait, (char **) old);
	}


		
	*old = new;

	if((new != NULL) 
	   && 
	   (new != (nsc_bucket_t *) -1)) { 
		/* real data, not just update pending or invalidate*/

		new->nsc_hits = 1;
		new->nsc_status = 0;
		new->nsc_refcount = 1;
		current_admin.group.nsc_entries++;

		if(new->nsc_data.nsc_return_code == SUCCESS) {
			new->nsc_timestamp = time(NULL) + current_admin.group.nsc_pos_ttl;
		}
		else {
			new->nsc_timestamp = time(NULL) + current_admin.group.nsc_neg_ttl;
		}
	}
	return(0);
}


/*ARGSUSED*/
static nsc_bucket_t *
fixbuffer(nsc_return_t * in, int maxlen)
{
	int	group_members;
	int	i;
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
	
	strs = 0;
	strs += 1 + strlen(in->nsc_u.grp.gr_name);
	pwlen = strlen(in->nsc_u.grp.gr_passwd);
	if(pwlen < 4)
	    pwlen = 4;
	strs += 1 + pwlen;

	group_members = 0;
	while (in->nsc_u.grp.gr_mem[group_members]) {
		strs += 1 + strlen(in->nsc_u.grp.gr_mem[group_members]);
		group_members++;
	}

	strs += (group_members+1) * sizeof(char*);
		
	/*
	 * allocate it and copy it in
	 * code doesn't assume packing order in original buffer
	 */
	
	if ((retb = (nsc_bucket_t * ) malloc(sizeof (*retb) + strs)) == NULL) {
		return (NULL);
	}
	
	out = &(retb->nsc_data);
	out->nsc_bufferbytesused = strs + ((int)&out->nsc_u.grp - (int)out) +
					sizeof(struct group);
	out->nsc_return_code 	= SUCCESS;
	out->nsc_errno 		= 0;


	out->nsc_u.grp.gr_gid = in->nsc_u.grp.gr_gid;

        dest = retb->nsc_data.nsc_u.buff + sizeof (struct group);
	offset = (int) dest;

	members = (char**)dest;
	out->nsc_u.grp.gr_mem = (char**)(dest - offset);
	dest += (group_members+1)*sizeof(char*);


	strcpy(dest, in->nsc_u.grp.gr_name);
	strs = 1 + strlen(in->nsc_u.grp.gr_name);
	out->nsc_u.grp.gr_name = dest - offset;
	dest += strs;
	
	strcpy(dest, in->nsc_u.grp.gr_passwd);
	strs = 1 + pwlen;
	out->nsc_u.grp.gr_passwd = dest - offset;
	dest += strs;
	
	for (i = 0; i < group_members; i++) {
		members[i] = dest - offset;
		strcpy(dest, in->nsc_u.grp.gr_mem[i]);
		strs = 1 + strlen(in->nsc_u.grp.gr_mem[i]);
		dest += strs;
	}
	members[i] = NULL; /* null terminate list */
	memcpy(in, out, out->nsc_bufferbytesused);

	return (retb);
}


