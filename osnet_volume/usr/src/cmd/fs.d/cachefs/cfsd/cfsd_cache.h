/*
 *
 *			cache.h
 *
 * Include file for the cache class.
 */

#ident   "@(#)cfsd_cache.h 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#ifndef CFSD_CACHE
#define	CFSD_CACHE

typedef struct cfsd_cache_object {
	char			 i_cachedir[MAXPATHLEN]; /* cache directory */
	int			 i_cacheid;		 /* cache id */
	cfsd_fscache_object_t	*i_fscachelist;		 /* list of fscaches */
	int			 i_fscachecount;	 /* # of objs in list */
	mutex_t			 i_lock;		 /* synchro lock */
	int			 i_refcnt;		 /* refs to object */
	int			 i_nextfscacheid;	 /* for fscache ids */
	int			 i_modify;		 /* changes when mod */
	struct cfsd_cache_object	*i_next;	 /* next cache object */
} cfsd_cache_object_t;

cfsd_cache_object_t *cfsd_cache_create(void);
void cfsd_cache_destroy(cfsd_cache_object_t *cache_object_p);

int cache_setup(cfsd_cache_object_t *cache_object_p, const char *cachedirp,
    int cacheid);
void cache_lock(cfsd_cache_object_t *cache_object_p);
void cache_unlock(cfsd_cache_object_t *cache_object_p);

cfsd_fscache_object_t *cache_fscachelist_at(cfsd_cache_object_t *cache_object_p,
    size_t index);
void cache_fscachelist_add(cfsd_cache_object_t *cache_object_p,
    cfsd_fscache_object_t *fscache_object_p);
cfsd_fscache_object_t *cache_fscachelist_find(
    cfsd_cache_object_t *cache_object_p, const char *namep);

#endif /* CFSD_CACHE */
