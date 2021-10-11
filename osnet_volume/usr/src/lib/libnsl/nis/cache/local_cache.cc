/*
 *	local_cache.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)local_cache.cc	1.24	99/09/20 SMI"

#include	"../../rpc/rpc_mt.h"
#include 	<stdlib.h>
#include 	<rpc/rpc.h>
#include 	<values.h>
#include 	<string.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>
#include	<syslog.h>
#include 	<rpcsvc/nis.h>
#include	"local_cache.h"
#include	"cold_start.h"

NisLocalCache::NisLocalCache(nis_error &status)
{
	rwlock_init(&lock, USYNC_THREAD, NULL);
	head = NULL;
	tail = NULL;
	act_list = NULL;
	have_coldstart = 0;
	sem_writer = -1;

	//  read in entry from coldstart file
	if (readColdStart())
		status = NIS_SUCCESS;
	else
		status = NIS_COLDSTART_ERR;
}


NisLocalCache::NisLocalCache(nis_error &status, uint32_t *exp_time)
{
	rwlock_init(&lock, USYNC_THREAD, NULL);
	head = NULL;
	tail = NULL;
	act_list = NULL;
	have_coldstart = 0;
	sem_writer = -1;

	//  read in entry from coldstart file
	if (readServerColdStart(exp_time))
		status = NIS_SUCCESS;
	else
		status = NIS_COLDSTART_ERR;
}

NisLocalCache::~NisLocalCache()
{
	// We don't free anything because we don't know
	// how many threads have a reference to us.
}

// Return the binding for the directory name if it exists in the cache.
// If 'near' is set, then we return a binding to a directory that is
// close to 'dname'.
nis_error
NisLocalCache::searchDir(char *dname, nis_bound_directory **binding, int near)
{
	int		distance;
	int 		minDistance = MAXINT;
	int  		minLevels = MAXINT;
	nis_error	err;
	struct timeval 	now;
	sigset_t	oset;
	char		**target;
	int		target_levels;
	LocalCacheEntry	*scan;
	LocalCacheEntry	*found = NULL;

	*binding = NULL;
	target = __break_name(dname, &target_levels);
	if (target == 0)
		return (NIS_NOMEMORY);

	(void) gettimeofday(&now, NULL);

	lockShared(&oset);
	for (scan = head; scan; scan = scan->next) {
		distance = __name_distance(target, scan->components);
		if (distance <= minDistance) {
			// if two directories are at the same distance
			// then we want to select the directory closer to
			// the root.
			if (distance == minDistance &&
			    scan->levels >= minLevels) {
				// this one is further from the root, ignore it
				continue;
			}
			found = scan;
			minDistance = distance;
			minLevels = scan->levels;
		}
		if (distance == 0)
			break;
	}

	if (found == 0) {
		// cache is empty (no coldstart even)
		err = NIS_NAMEUNREACHABLE;
	} else if (near == 0 && distance != 0) {
		// we wanted an exact match, but it's not there
		err = NIS_NOTFOUND;
	} else {
		// we got an exact match or something near target
		err = NIS_SUCCESS;
		*binding = unpackBinding(found->binding_val,
					found->binding_len);
		if (*binding == NULL) {
			err = NIS_NOMEMORY;
		} else {
			struct timeval now;
			gettimeofday(&now, 0);
			if (found->expTime < now.tv_sec) {
				err = NIS_CACHEEXPIRED;
			}
		}

	}
	unlockShared(&oset);
	if (*binding)
		addAddresses(*binding);

	__free_break_name(target, target_levels);
	return (err);
}

void
NisLocalCache::addBinding(nis_bound_directory *binding)
{
	LocalCacheEntry *new_entry;
	LocalCacheEntry *scan;
	LocalCacheEntry *prev;
	sigset_t oset;
	int is_coldstart = 0;

	new_entry = createCacheEntry(binding);
	if (new_entry == 0)
		return;

	if (nis_dir_cmp(new_entry->name, coldStartDir()) == SAME_NAME)
		is_coldstart = 1;

	lockExclusive(&oset);
	prev = NULL;
	for (scan = head; scan; scan = scan->next) {
		if (nis_dir_cmp(scan->name, new_entry->name) == SAME_NAME) {
			if (scan == head) {
				head = scan->next;
			} else {
				prev->next = scan->next;
			}
			if (scan == tail) {
				if (prev)
					tail = prev;
				else
					tail = NULL;
			}
			freeCacheEntry(scan);
			break;
		}
		prev = scan;
	}

	if (is_coldstart) {
		have_coldstart = 1;
		new_entry->next = head;
		head = new_entry;
		if (tail == 0)
			tail = new_entry;
	} else {
		if (tail)
			tail->next = new_entry;
		tail = new_entry;
		if (head == 0)
			head = new_entry;
	}

	unlockExclusive(&oset);
}

void
NisLocalCache::removeBinding(nis_bound_directory *binding)
{
	LocalCacheEntry *scan;
	LocalCacheEntry *prev;
	char *dname;
	sigset_t oset;

	dname = binding->dobj.do_name;
	lockExclusive(&oset);
	prev = NULL;
	for (scan = head; scan; scan = scan->next) {
		if (nis_dir_cmp(scan->name, dname) == SAME_NAME) {
			if (scan == head) {
				have_coldstart = 0;
				head = scan->next;
			} else {
				prev->next = scan->next;
			}
			if (scan == tail) {
				if (prev)
					tail = prev;
				else
					tail = NULL;
			}
			freeCacheEntry(scan);
			break;
		}
		prev = scan;
	}
	unlockExclusive(&oset);
}

LocalCacheEntry *
NisLocalCache::createCacheEntry(nis_bound_directory *binding)
{
	LocalCacheEntry *entry;

	entry = new LocalCacheEntry;
	if (!entry)
		return (NULL);

	entry->name = strdup(binding->dobj.do_name);
	entry->components = __break_name(binding->dobj.do_name, &entry->levels);
	entry->expTime = expireTime(binding->dobj.do_ttl);
	entry->generation = nextGeneration();
	entry->binding_val = packBinding(binding, &entry->binding_len);
	entry->next = NULL;

	if (entry->name == NULL ||
	    entry->components == NULL ||
	    entry->binding_val == NULL) {
		freeCacheEntry(entry);
		return (NULL);
	}

	return (entry);
}

void
NisLocalCache::freeCacheEntry(LocalCacheEntry *entry)
{
	free(entry->name);
	if (entry->components)
		__free_break_name(entry->components, entry->levels);
	free(entry->binding_val);
	delete entry;
}

void
NisLocalCache::activeAdd(nis_active_endpoint *act)
{
	LocalActiveEntry *entry;
	sigset_t oset;

	lockExclusive(&oset);
	entry = createActiveEntry(act);
	if (entry) {
		entry->next = act_list;
		act_list = entry;
	}
	unlockExclusive(&oset);
}

void
NisLocalCache::activeRemove(endpoint *ep, int all)
{
	LocalActiveEntry *entry;
	LocalActiveEntry *prev = NULL;
	sigset_t oset;

	lockExclusive(&oset);
restart:
	for (entry = act_list; entry; entry = entry->next) {
		if (strcmp(entry->act->ep.family, ep->family) == 0 &&
		    (all || strcmp(entry->act->ep.proto, ep->proto) == 0) &&
		    strcmp(entry->act->ep.uaddr, ep->uaddr) == 0) {
			if (prev)
				prev->next = entry->next;
			else
				act_list = entry->next;
			activeFree(entry->act);
			freeActiveEntry(entry);
			if (all)
				goto restart;
			break;
		}
	}
	unlockExclusive(&oset);
}

int
NisLocalCache::activeCheck(endpoint *ep)
{
	int ret = 0;
	LocalActiveEntry *entry;
	sigset_t oset;

	lockShared(&oset);
	for (entry = act_list; entry; entry = entry->next) {
		if (strcmp(entry->act->ep.family, ep->family) == 0 &&
		    strcmp(entry->act->ep.proto, ep->proto) == 0 &&
		    strcmp(entry->act->ep.uaddr, ep->uaddr) == 0) {

			ret = 1;
			break;
		}
	}
	unlockShared(&oset);
	return (ret);
}


int
NisLocalCache::activeGet(endpoint *ep, nis_active_endpoint **act)
{
	int ret = 0;
	LocalActiveEntry *entry;
	sigset_t oset;

	lockShared(&oset);
	for (entry = act_list; entry; entry = entry->next) {
		if (strcmp(entry->act->ep.family, ep->family) == 0 &&
		    strcmp(entry->act->ep.proto, ep->proto) == 0 &&
		    strcmp(entry->act->ep.uaddr, ep->uaddr) == 0) {
			*act = activeDup(entry->act);
			ret = 1;
			break;
		}
	}
	unlockShared(&oset);
	return (ret);
}

int
NisLocalCache::getAllActive(nis_active_endpoint ***actlist)
{
	int ret = 0;
	LocalActiveEntry *entry;
	sigset_t oset;

	lockShared(&oset);
	for (entry = act_list; entry; entry = entry->next) {
		ret++;
	}

	*actlist = (nis_active_endpoint **)
		malloc(ret * sizeof (nis_active_endpoint *));
	if (*actlist == NULL) {
		unlockShared(&oset);
		return (0);
	}

	for (ret = 0, entry = act_list; entry; entry = entry->next) {
		(*actlist)[ret++] = activeDup(entry->act);
	}

	unlockShared(&oset);
	return (ret);
}

LocalActiveEntry *
NisLocalCache::createActiveEntry(nis_active_endpoint *act)
{
	LocalActiveEntry *entry;

	entry = new LocalActiveEntry;
	if (entry == NULL)
		return (NULL);

	entry->act = act;
	entry->next = NULL;
	return (entry);
}

void
NisLocalCache::freeActiveEntry(LocalActiveEntry *entry)
{
	delete entry;
}

void
NisLocalCache::print()
{
	int i;
	LocalCacheEntry *entry;
	LocalActiveEntry *act_entry;
	nis_bound_directory *binding;
	sigset_t oset;

	lockShared(&oset);
	for (entry = head, i = 0; entry; entry = entry->next, i++) {
		// hack for special format in nisshowcache
		if (__nis_debuglevel != 6) {
			if (i == 0 && have_coldstart)
				(void) printf("\nCold Start directory:\n");
			else
				(void) printf("\nNisLocalCacheEntry[%d]:\n", i);
		}

		if (__nis_debuglevel == 1) {
			(void) printf("\tdir_name:'%s'\n", entry->name);
		}

		if (__nis_debuglevel > 2) {
			binding = unpackBinding(entry->binding_val,
					entry->binding_len);
			if (binding != NULL) {
				printBinding_exptime(binding, entry->expTime);
				nis_free_binding(binding);
			}
		}
	}

	(void) printf("\nActive servers:\n");
	for (act_entry = act_list; act_entry; act_entry = act_entry->next) {
		printActive(act_entry->act);
	}
	unlockShared(&oset);
}

void
NisLocalCache::lockShared(sigset_t *sset)
{
	thr_sigblock(sset);
	rw_rdlock(&lock);
}

void
NisLocalCache::unlockShared(sigset_t *sset)
{
	rw_unlock(&lock);
	thr_sigsetmask(SIG_SETMASK, sset, NULL);
}

void
NisLocalCache::lockExclusive(sigset_t *sset)
{
	thr_sigblock(sset);
	rw_wrlock(&lock);
}

void
NisLocalCache::unlockExclusive(sigset_t *sset)
{
	rw_unlock(&lock);
	thr_sigsetmask(SIG_SETMASK, sset, NULL);
}

int
NisLocalCache::mgrUp()
{
	u_short w_array[NIS_W_NSEMS];
	union semun {
		int val;
		struct semid_ds *buf;
		ushort *array;
	} semarg;
	sigset_t oset;

	lockExclusive(&oset);
	if (sem_writer == -1) {
		// get writer semaphore
		sem_writer = semget(NIS_SEM_W_KEY, NIS_W_NSEMS, 0);
		if (sem_writer == -1) {
			syslog(LOG_DEBUG, "can't create writer semaphore: %m");
			unlockExclusive(&oset);
			return (0);
		}
	}
	unlockExclusive(&oset);

	// get writer semaphore value
	semarg.array = w_array;
	if (semctl(sem_writer, 0, GETALL, semarg) == -1) {
		syslog(LOG_DEBUG, "can't get writer value: %m");
		return (0);
	}

	// check to see if a manager is already handling the cache
	if (w_array[NIS_SEM_MGR_UP] == 0)
		return (0);

	return (1);
}


uint32_t
NisLocalCache::loadPreferredServers()
{
	uint32_t ul = 0;
	sigset_t oset;

	if (!mgrUp()) {
		return (0);
	}

	/*
	 * read from the "dot" file first.  If successful, return the
	 * TTL value.
	 */
	lockExclusive(&oset);
	ul = loadDotFile();
	unlockExclusive(&oset);
	if (ul > 0)
		return (ul);

	/* failed to load the Dot file. */
	return (0);
}

uint32_t
NisLocalCache::refreshCache()
{
	uint32_t ul;

	backupPreference();
	ul = loadPreferredServers();
	if (ul > 0) {
		delBackupPref();
		rerankServers();
		return (ul);
	}
	restorePreference();
	return (expireTime(ONE_HOUR));
}

int
NisLocalCache::resetBinding(nis_bound_directory *binding) {
	removeBinding(binding);
	return (1);
}
