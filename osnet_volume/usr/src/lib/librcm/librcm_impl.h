/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LIBRCM_IMPL_H
#define	_LIBRCM_IMPL_H

#pragma ident	"@(#)librcm_impl.h	1.1	99/08/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/systeminfo.h>
#include <librcm.h>
#include <libdevfsevent.h>

/*
 * This file contains information private to librcm rcm_daemon.
 */
#define	RCM_DAEMON_START	"/usr/lib/rcm/rcm_daemon"
#define	RCM_SERVICE_DOOR	"/var/run/rcm_daemon_door"
#define	RCM_MODULE_SUFFIX	"_rcm.so"

/*
 * flag fields supported by individual librcm interfaces
 */
#define	RCM_ALLOC_HDL_MASK	(RCM_NOPID)
#define	RCM_GET_INFO_MASK	(RCM_INCLUDE_SUBTREE|RCM_INCLUDE_DEPENDENT|\
				RCM_DR_OPERATION|RCM_MOD_INFO|RCM_FILESYS)
#define	RCM_REGISTER_MASK	(RCM_FILESYS)
#define	RCM_REQUEST_MASK	(RCM_QUERY|RCM_FORCE|RCM_FILESYS)
#define	RCM_NOTIFY_MASK		(RCM_FILESYS)

/*
 * EC_RCM event types and attributes
 */

/* librcm->rcm_daemon */
#define	ET_RCM_NOOP		0
#define	ET_RCM_SUSPEND		1
#define	ET_RCM_RESUME		2
#define	ET_RCM_OFFLINE		3
#define	ET_RCM_ONLINE		4
#define	ET_RCM_REMOVE		5
#define	ET_RCM_REGIS_RESOURCE	6
#define	ET_RCM_UNREGIS_RESOURCE	7
#define	ET_RCM_GET_INFO		8

/* rcm_daemon-> librcm */
#define	ET_RCM_REQ_GRANTED	11
#define	ET_RCM_REQ_CONFLICT	12
#define	ET_RCM_REQ_DENIED	13
#define	ET_RCM_INFO		14
#define	ET_RCM_EFAULT		15
#define	ET_RCM_EPERM		16
#define	ET_RCM_EINVAL		17
#define	ET_RCM_EAGAIN		18
#define	ET_RCM_NOTIFY_DONE	19
#define	ET_RCM_NOTIFY_FAIL	20
#define	ET_RCM_EALREADY		21
#define	ET_RCM_ENOENT		22

/* event data names */
#define	RCM_RSRCNAME		"rcm.rsrcname"
#define	RCM_RSRCSTATE		"rcm.rsrcstate"
#define	RCM_CLIENT_MODNAME	"rcm.client_modname"
#define	RCM_CLIENT_INFO		"rcm.client_info"
#define	RCM_CLIENT_ID		"rcm.client_id"
#define	RCM_SEQ_NUM		"rcm.seq_num"
#define	RCM_REQUEST_FLAG	"rcm.request_flag"
#define	RCM_SUSPEND_INTERVAL	"rcm.suspend_interval"

/*
 * action commands shared by librcm and rcm_daemon
 */
#define	CMD_REGISTER	1
#define	CMD_UNREGISTER	2
#define	CMD_GETINFO	3
#define	CMD_SUSPEND	4
#define	CMD_RESUME	5
#define	CMD_OFFLINE	6
#define	CMD_ONLINE	7
#define	CMD_REMOVE	8

/*
 * Ops vector for calling directly into daemon from RCM modules
 */
typedef struct {
	int	(*librcm_regis)();
	int	(*librcm_unregis)();
	int	(*librcm_getinfo)();
	int	(*librcm_suspend)();
	int	(*librcm_resume)();
	int	(*librcm_offline)();
	int	(*librcm_online)();
	int	(*librcm_remove)();
} librcm_ops_t;

/*
 * rcm handle struture
 */
struct rcm_handle {
	char		*modname;
	pid_t		pid;
	int		seq_num;
	librcm_ops_t	*lrcm_ops;
};

struct rcm_info {
	struct rcm_info	*next;
	int	seq_num;
	pid_t	pid;
	int	state;	/* numerical state */
	char	*info;
	char	*rsrcname;
	char	*modname;
};

/*
 * module utility routines
 */
char *rcm_module_dir(uint_t);
void *rcm_module_open(char *);
void rcm_module_close(void *);

#ifdef	__cplusplus
}
#endif

#endif /* _LIBRCM_IMPL_H */
