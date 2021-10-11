/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__MGR_CACHE_H
#define	__MGR_CACHE_H

#pragma ident	"@(#)mgr_cache.h	1.4	99/03/16 SMI"

#include "mapped_cache.h"

#define	MIN_REFRESH_WAIT (5 * 60)	/* 5 minutes */
#define	PING_WAIT (15 * 60)		/* 15 minutes */
#define	CONFIG_WAIT (12 * 60 * 60)	/* 12 hours */

class NisMgrCache : public NisMappedCache {
    public:
	NisMgrCache(nis_error &error, int discardOldCache);
	~NisMgrCache();
	void start();
	uint32_t loadPreferredServers();

	uint32_t timers();
	uint32_t nextTime();
	uint32_t refreshCache();

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { free(arg); }

    private:
	uint32_t refresh_time;
	uint32_t ping_time;
	uint32_t config_time;
	uint32_t config_interval;

	void refresh();
	void ping();
	uint32_t config();
	void parse_info(char *info, char **srvr, char **option);
	char *get_line(FILE *fp);
	uint32_t writeDotFile();
	uint32_t loadLocalFile();
	uint32_t loadNisTable();
};

#endif	/* __MGR_CACHE_H */
