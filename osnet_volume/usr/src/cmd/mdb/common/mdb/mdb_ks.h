/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_KS_H
#define	_MDB_KS_H

#pragma ident	"@(#)mdb_ks.h	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/vnode.h>
#include <sys/proc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * MDB Kernel Support Interfaces:
 *
 * Debugger modules for kernel crash dumps can make use of these utility
 * functions.  This module also provides support for <mdb/mdb_param.h>.
 */

#define	MDB_PATH_NELEM	256			/* Maximum path components */

typedef struct mdb_path {
	size_t mdp_nelem;			/* Number of components */
	uint_t mdp_complete;			/* Path completely resolved? */
	uintptr_t mdp_vnode[MDB_PATH_NELEM];	/* Array of vnode_t addresses */
	char *mdp_name[MDB_PATH_NELEM];		/* Array of name components */
} mdb_path_t;

extern int mdb_autonode2path(uintptr_t, mdb_path_t *);
extern int mdb_vnode2path(uintptr_t, mdb_path_t *);
extern int mdb_sprintpath(char *, size_t, mdb_path_t *);

extern char *mdb_vnode2buf(uintptr_t, char *, size_t);
extern uintptr_t mdb_vnode2page(uintptr_t, uintptr_t);

extern uintptr_t mdb_pid2proc(pid_t, proc_t *);
extern char mdb_vtype2chr(vtype_t, mode_t);
extern uintptr_t mdb_addr2modctl(uintptr_t);

extern int mdb_name_to_major(const char *, major_t *);
extern const char *mdb_major_to_name(major_t);
extern int mdb_get_soft_state(const char *, size_t, uintptr_t *);

/*
 * MDB Kernel STREAMS Subsystem:
 *
 * Debugger modules such as ip can provide facilities for decoding private
 * q_ptr data for STREAMS queues using this mechanism.  The module first
 * registers a set of functions which may be invoked when q->q_qinfo matches
 * a given qinit address (such as ip`winit).  The q_info function provides
 * a way for the module to return an information string about the particular
 * queue.  The q_rnext and q_wnext functions provide a way for the generic
 * queue walker to ask how to proceed deeper in the STREAM when q_next is
 * NULL.  This allows ip, for example, to provide access to the link-layer
 * queues beneath the ip-client queue.
 */

typedef struct mdb_qops {
	void (*q_info)(const queue_t *, char *, size_t);
	uintptr_t (*q_rnext)(const queue_t *);
	uintptr_t (*q_wnext)(const queue_t *);
} mdb_qops_t;

extern void mdb_qops_install(const mdb_qops_t *, uintptr_t);
extern void mdb_qops_remove(const mdb_qops_t *, uintptr_t);

extern char *mdb_qname(const queue_t *, char *, size_t);
extern void mdb_qinfo(const queue_t *, char *, size_t);

extern uintptr_t mdb_qrnext(const queue_t *);
extern uintptr_t mdb_qwnext(const queue_t *);

/*
 * These functions, provided by mdb_ks, may be used to fill in the q_rnext
 * and q_wnext members of mdb_qops_t, in the case where the client wishes
 * to simply return q->q_next:
 */
extern uintptr_t mdb_qrnext_default(const queue_t *);
extern uintptr_t mdb_qwnext_default(const queue_t *);

/*
 * MDB KPROC Target Interface:
 *
 * The kproc target (user processes from kernel crash dump) relies on looking
 * up and invoking these functions in mdb_ks so that dependencies on the
 * current kernel implementation are isolated in mdb_ks.
 */

struct mdb_map; /* Private between kproc and ks */

extern int mdb_kproc_asiter(uintptr_t,
    void (*)(const struct mdb_map *, void *), void *);
extern int mdb_kproc_auxv(uintptr_t, auxv_t *);
extern uintptr_t mdb_kproc_as(uintptr_t);
extern pid_t mdb_kproc_pid(uintptr_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_KS_H */
