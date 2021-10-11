// ------------------------------------------------------------
//
//			all.h
//
// Include file for the cfsd_all class.
//

#pragma ident   "@(#)cfsd_all.h 1.3     95/08/31 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

#ifndef CFSD_ALL
#define	CFSD_ALL

class cfsd_all {
private:
	RWCString		 i_machname;		// machine name
	RWTPtrDlist<cfsd_cache>  i_cachelist;		// list of caches
	mutex_t			 i_lock;		// synchronizing lock
	int			 i_nextcacheid;		// for cache ids
	int			 i_modify;		// changed when modified
	// cfsd_hoard		*i_hoardp;		// hoarding class

public:
	cfsd_all();
	~cfsd_all();

	const char *all_machname();

	void all_lock();
	void all_unlock();

	int all_nextcacheid() { return i_nextcacheid++; }
	int all_modify() { return i_modify; }

	size_t all_cachelist_entries();
	cfsd_cache *all_cachelist_at(size_t index);
	void all_cachelist_add(cfsd_cache *cachep);
	cfsd_cache *all_cachelist_find(const char *namep);

	// cfsd_hoard *all_hoard() { return al_hoardp; }
	// void all_hoard(cfsd_hoard *hoardp) { al_hoardp = hoardp; }

	void all_cachefstab_update();
};


#endif /* CFSD_ALL */
