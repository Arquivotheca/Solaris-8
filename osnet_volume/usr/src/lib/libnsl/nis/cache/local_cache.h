/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__LOCAL_CACHE_H
#define	__LOCAL_CACHE_H

#pragma ident	"@(#)local_cache.h	1.4	99/03/16 SMI"

#include "cache.h"

struct LocalCacheEntry {
	char *name;
	int levels;
	char **components;
	uint32_t expTime;
	int generation;
	int binding_len;
	void *binding_val;
	LocalCacheEntry *next;

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { free(arg); }
};

struct LocalActiveEntry {
	nis_active_endpoint *act;
	LocalActiveEntry *next;

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { free(arg); }
};

class NisLocalCache : public NisCache {
    public:
	NisLocalCache(nis_error &error);
	NisLocalCache(nis_error &error, uint32_t *expt_time);
	~NisLocalCache();

	nis_error searchDir(char *dname,
		nis_bound_directory **info, int near);
	void addBinding(nis_bound_directory *info);
	void removeBinding(nis_bound_directory *info);
	void print();

	void activeAdd(nis_active_endpoint *act);
	void activeRemove(endpoint *ep, int all);
	int activeCheck(endpoint *ep);
	int activeGet(endpoint *ep, nis_active_endpoint **act);
	int getAllActive(nis_active_endpoint ***actlist);

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { free(arg); }
	uint32_t loadPreferredServers();
	uint32_t refreshCache();
	int resetBinding(nis_bound_directory *);

    private:
	rwlock_t lock;
	LocalCacheEntry *head;
	LocalCacheEntry *tail;
	LocalActiveEntry *act_list;
	int have_coldstart;
	int sem_writer;

	void lockShared(sigset_t *sset);
	void unlockShared(sigset_t *sset);
	void lockExclusive(sigset_t *sset);
	void unlockExclusive(sigset_t *sset);

	LocalCacheEntry *createCacheEntry(nis_bound_directory *info);
	void freeCacheEntry(LocalCacheEntry *entry);

	LocalActiveEntry *createActiveEntry(nis_active_endpoint *act);
	void freeActiveEntry(LocalActiveEntry *entry);

	int mgrUp();
};

#endif	/* __LOCAL_CACHE_H */
