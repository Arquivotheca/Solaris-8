// ------------------------------------------------------------
//
//			cache.h
//
// Include file for the cache class.
//

#pragma ident   "@(#)cfsd_cache.h 1.2     95/03/08 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

#ifndef CFSD_CACHE
#define	CFSD_CACHE

class cfsd_cache {
private:
	RWCString		 i_cachedir;		// cache directory
	int			 i_cacheid;		// cache id
	RWTPtrDlist<cfsd_fscache> i_fscachelist;	// list of fscaches
	mutex_t			 i_lock;		// synchronizing lock
	int			 i_refcnt;		// refs to object
	int			 i_nextfscacheid;	// for fscache ids
	int			 i_modify;		// changes when modified

public:
	cfsd_cache();
	~cfsd_cache();

	int cache_setup(const char *cachedirp, int cacheid);
	const char *cache_cachedir();
	int cache_cacheid() { return i_cacheid; }

	void cache_lock();
	void cache_unlock();

	int cache_nextfscacheid() { return i_nextfscacheid++; }
	int cache_modify() { return i_modify; }

	void cache_refinc() { i_refcnt++; }
	void cache_refdec() { i_refcnt--; }
	int cache_refcnt() { return i_refcnt; }

	size_t cache_fscachelist_entries();
	cfsd_fscache *cache_fscachelist_at(size_t index);
	void cache_fscachelist_add(cfsd_fscache *fscachep);
	cfsd_fscache *cache_fscachelist_find(const char *namep);

	int operator==(const cfsd_cache &cache) const;
};

#endif /* CFSD_CACHE */
