/*
 * Copyright (c) 1997, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)mt_rpcinit.c	1.13	99/08/13 SMI"	/* SVr4.0 */

/*
 * This file is a merge of the previous two separate files: mt_clntinit.c,
 * mt_svcinit.c, plus some kstat_create code from os/kstat_fr.c file.
 * Previously, mt_rpcclnt_init() and mt_rpcsvc_init() are called from
 * startup() routine in $KARCH/os/startup.c; and mt_kstat_init() is
 * called from os/kstat_fr.c. Now, all three of them are called from
 * the _init() routine in rpcmod.c.
 */

/*
 * Define and initialize MT client/server data.
 */

#include	<sys/types.h>
#include	<sys/t_lock.h>
#include	<sys/kstat.h>

kmutex_t rcstat_lock;		/* rcstat structure updating */
kmutex_t xid_lock;		/* XID allocation */
kmutex_t clnt_pending_lock;	/* for list of pending calls awaiting replies */
kmutex_t connmgr_lock;		/* for connection mngr's list of transports */
kmutex_t clnt_max_msg_lock;	/* updating max message sanity check for cots */

extern	kstat_named_t	*rcstat_ptr;
extern	uint_t		rcstat_ndata;
extern	kstat_named_t	*rsstat_ptr;
extern	uint_t		rsstat_ndata;
extern  kstat_named_t   *cotsrcstat_ptr;
extern	uint_t		cotsrcstat_ndata;
extern	kstat_named_t	*cotsrsstat_ptr;
extern	uint_t		cotsrsstat_ndata;


void
mt_kstat_init(void)
{
	kstat_t *ksp;

	ksp = kstat_create("unix", 0, "rpc_clts_client", "rpc",
	    KSTAT_TYPE_NAMED, rcstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) rcstat_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "rpc_cots_client", "rpc",
	    KSTAT_TYPE_NAMED, cotsrcstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) cotsrcstat_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "rpc_cots_connections", "rpc",
	    KSTAT_TYPE_NAMED, 0, KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_VAR_SIZE);
	if (ksp) {
		extern int conn_kstat_update(), conn_kstat_snapshot();

		ksp->ks_lock = &connmgr_lock;
		ksp->ks_update = conn_kstat_update;
		ksp->ks_snapshot = conn_kstat_snapshot;
		kstat_install(ksp);
	}

	/*
	 * Backwards compatibility for old kstat clients
	 */
	ksp = kstat_create("unix", 0, "rpc_client", "rpc",
	    KSTAT_TYPE_NAMED, rcstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) rcstat_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "rpc_clts_server", "rpc",
	    KSTAT_TYPE_NAMED, rsstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) rsstat_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "rpc_cots_server", "rpc",
	    KSTAT_TYPE_NAMED, cotsrsstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) cotsrsstat_ptr;
		kstat_install(ksp);
	}

	/*
	 * Backwards compatibility for old kstat clients
	 */
	ksp = kstat_create("unix", 0, "rpc_server", "rpc",
	    KSTAT_TYPE_NAMED, rsstat_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = (void *) rsstat_ptr;
		kstat_install(ksp);
	}
}

/*
 * Deletes the previously allocated "rpc" kstats
 * This routine is called by _init() if mod_install() failed.
 */
void
mt_kstat_fini(void)
{
	kstat_t *ksp;

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_clts_client");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_cots_client");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_cots_connections");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_client");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_clts_server");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_cots_server");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("unix", 0, "rpc_server");
	mutex_exit(&kstat_chain_lock);
	if (ksp)
		kstat_delete(ksp);
}

static uint32_t clnt_xid = 0;	/* transaction id used by all clients */

uint32_t
alloc_xid(void)
{
	uint32_t  xid;

	mutex_enter(&xid_lock);
	if (!clnt_xid) {
		clnt_xid = (uint32_t)((hrestime.tv_sec << 20) |
		    (hrestime.tv_nsec >> 10));
	}

	/*
	 * Pre-increment in-case for whatever reason, clnt_xid is still
	 * zero after the above initialization.
	 */
	xid = ++clnt_xid;
	mutex_exit(&xid_lock);
	return (xid);
}
