/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__CLIENT_CACHE_H
#define	__CLIENT_CACHE_H

#pragma ident	"@(#)client_cache.h	1.2	96/05/22 SMI"

#include <netdir.h>
#include <netconfig.h>
#include "mapped_cache.h"

class NisClientCache : public NisMappedCache {
    public:
	NisClientCache(nis_error &error);
	~NisClientCache();

	int okay();
	nis_error bindReplica(char *dname, nis_bound_directory **binding);
	nis_error bindMaster(char *dname, nis_bound_directory **binding);
	nis_error bindServer(nis_server *srv, int nsrv,
			nis_bound_directory **binding);
	int refreshBinding(nis_bound_directory *binding);
	int refreshAddress(nis_bound_endpoint *bep);
	int refreshCallback(nis_bound_endpoint *bep);
	bool_t readColdStart();

    private:
	CLIENT *mgr_clnt;	/* rpc handle connected to cache manager */
	netconfig *ticlts;	/* cached netconfig entry for ticlts */
	char *curUaddr;		/* uaddr of cache manager */
	int curFd;		/* fd in mgr_clnt */
	dev_t curRdev;
	pid_t curPid;
	int cache_is_bad;

	CLIENT *clientHandle();
	void setClntState();
	int checkClntState();
	void cacheIsBad();
};

#endif	/* __CLIENT_CACHE_H */
