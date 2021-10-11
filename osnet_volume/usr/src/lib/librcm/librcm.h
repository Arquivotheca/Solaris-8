/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBRCM_H
#define	_LIBRCM_H

#pragma ident	"@(#)librcm.h	1.1	99/08/10 SMI"

#include <sys/types.h>
#include <sys/procset.h>
#include <sys/time_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Flags for rcm requests
 */
#define	RCM_INCLUDE_SUBTREE	0x01
#define	RCM_INCLUDE_DEPENDENT	0x02
#define	RCM_QUERY		0x04
#define	RCM_FORCE		0x08
#define	RCM_FILESYS		0x10	/* private to filesys module */
#define	RCM_NOPID		0x20
#define	RCM_DR_OPERATION	0x40
#define	RCM_MOD_INFO		0x80	/* private */

/*
 * RCM return values
 */
#define	RCM_SUCCESS		0
#define	RCM_FAILURE		-1
#define	RCM_CONFLICT		-2

/*
 * RCM resource states
 */
#define	RCM_STATE_UNKNOWN	0
#define	RCM_STATE_ONLINE	1
#define	RCM_STATE_ONLINING	2
#define	RCM_STATE_OFFLINE_FAIL	3
#define	RCM_STATE_OFFLINING	4
#define	RCM_STATE_OFFLINE	5
#define	RCM_STATE_REMOVING	6
#define	RCM_STATE_RESUMING	10
#define	RCM_STATE_SUSPEND_FAIL	11
#define	RCM_STATE_SUSPENDING	12
#define	RCM_STATE_SUSPEND	13
#define	RCM_STATE_REMOVE	14	/* private to rcm_daemon */

/*
 * rcm handles
 */
typedef struct rcm_handle rcm_handle_t;
typedef struct rcm_info rcm_info_t;
typedef rcm_info_t rcm_info_tuple_t;

/*
 * Interface definitions
 */
int rcm_alloc_handle(char *, uint_t, void *, rcm_handle_t **);
int rcm_free_handle(rcm_handle_t *);
int rcm_get_info(rcm_handle_t *, char *, uint_t, rcm_info_t **);
void rcm_free_info(rcm_info_t *);
int rcm_append_info(rcm_info_t **, rcm_info_t *);
rcm_info_tuple_t *rcm_info_next(rcm_info_t *, rcm_info_tuple_t *);
const char *rcm_info_rsrc(rcm_info_tuple_t *);
const char *rcm_info_info(rcm_info_tuple_t *);
const char *rcm_info_modname(rcm_info_tuple_t *);
pid_t rcm_info_pid(rcm_info_tuple_t *);
int rcm_info_state(rcm_info_tuple_t *);
int rcm_info_seqnum(rcm_info_tuple_t *);

int rcm_request_offline(rcm_handle_t *, char *, uint_t, rcm_info_t **);
int rcm_notify_online(rcm_handle_t *, char *, uint_t, rcm_info_t **);
int rcm_notify_remove(rcm_handle_t *, char *, uint_t, rcm_info_t **);
int rcm_request_suspend(rcm_handle_t *, char *, uint_t, timespec_t *,
    rcm_info_t **);
int rcm_notify_resume(rcm_handle_t *, char *, uint_t, rcm_info_t **);

int rcm_register_interest(rcm_handle_t *, char *, uint_t, rcm_info_t **);
int rcm_unregister_interest(rcm_handle_t *, char *, uint_t);

int rcm_exec_cmd(char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBRCM_H */
