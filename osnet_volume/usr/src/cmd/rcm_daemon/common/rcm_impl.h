/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RCM_IMPL_H
#define	_RCM_IMPL_H

#pragma ident	"@(#)rcm_impl.h	1.1	99/08/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <strings.h>
#include <syslog.h>
#include <thread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <librcm.h>
#include <librcm_impl.h>
#include <libdevfsevent.h>

#include "rcm_module.h"


/*
 * Daemon states for thread control
 */
#define	RCMD_INIT	1
#define	RCMD_NORMAL	2
#define	RCMD_CLEANUP	3
#define	RCMD_FINI	4

/*
 * flags for node operation
 */
#define	RSRC_NODE_CREATE	1
#define	RSRC_NODE_REMOVE	2	/* not used */

/*
 * Resource types
 */
#define	RSRC_TYPE_NORMAL	0
#define	RSRC_TYPE_DEVICE	1
#define	RSRC_TYPE_FILESYS	2
#define	RSRC_TYPE_ABSTRACT	3

/*
 * lock conflict checking flags
 */
#define	LOCK_FOR_DR		0
#define	LOCK_FOR_USE		1

/*
 * Sequence number encoding constants
 */
#define	SEQ_NUM_SHIFT	8	/* lowest 8 bits indicate cascade operation */
#define	SEQ_NUM_MASK	((1 << SEQ_NUM_SHIFT) - 1)

/*
 * Struct for client loadable module
 */
typedef struct module {
	struct module	*next;
	void		*dlhandle;
	struct rcm_mod_ops *(*init)();
	const char	*(*info)();
	int		(*fini)();
	struct rcm_mod_ops *modops;	/* ops vector */
	char		*name;		/* module name */
	rcm_handle_t	*rcmhandle;
	int		ref_count;
} module_t;

/*
 * Struct for describing a resource client
 */
typedef struct client {
	struct client	*next;
	module_t	*module;	/* per-client module */
	char		*alias;		/* rsrc_name known to client */
	pid_t		pid;		/* pid of regis process */
	int		state;		/* rsrc state known to client */
	uint_t		flag;		/* flag specified for registration */
} client_t;

/*
 * Struct for a list of outstanding rcm requests
 */
typedef struct {
	int	n_req;
	int	n_req_max;	/* max entries in this block */
	struct {
		int	seq_num;		/* sequence number of request */
		int	state;			/* current state */
		id_t	id;			/* id of initiator */
		uint_t	flag;			/* request flags */
		int	type;			/* resource(device) type */
		char	device[MAXPATHLEN];	/* name of device or resource */
	} req[1];
	/* more entries may follow */
} rcm_req_t;

/*
 * struct for describing resource tree node
 */
typedef struct rsrc_node {
	struct rsrc_node	*parent;
	struct rsrc_node	*sibling;
	struct rsrc_node	*child;
	char			*name;		/* phys path for devices */
	client_t		*users;		/* linked list of users */
	int			type;		/* resource type */
} rsrc_node_t;

/*
 * struct for tree action args
 */
typedef struct {
	int cmd;		/* command */
	int seq_num;		/* unique sequence number */
	int retcode;		/* return code */
	uint_t flag;		/* flag assoc. w command */
	timespec_t *interval;	/* for suspend command */
	rcm_info_t **info;	/* info to be filled in */
} tree_walk_arg_t;

/*
 * for synchrizing various threads
 */
typedef struct {
	int thr_count;
	short wanted;
	short state;
	time_t last_update;
	cond_t cv;
	mutex_t lock;
} barrier_t;

/*
 * locks
 */
extern mutex_t rcm_req_lock;

/*
 * global variables
 */
extern librcm_ops_t rcm_ops;	/* ops for module callback */
extern int need_cleanup;

/*
 * comparison macros
 *	EQUAL, AFTER, DESCENDENT
 */
#define	EQUAL(x, y)	(strcmp(x, y) == 0)
#define	AFTER(x, y)	(strcmp(x, y) > 0)
#define	DESCENDENT(x, y)			\
	((strlen(x) > strlen(y)) &&		\
	(strncmp(x, y, strlen(y)) == 0) &&	\
	((x[strlen(y)] == '/') ||		\
	(x[strlen(y)] == ':') ||		\
	(x[strlen(y) - 1] == '/')))

/*
 * function prototypes
 */

/* top level request handling routines */

void event_service(void **, size_t *);
int process_resource_suspend(char *, pid_t, uint_t, int,
    timespec_t *, rcm_info_t **);
int notify_resource_resume(char *, pid_t, uint_t, int, rcm_info_t **);
int process_resource_offline(char *, pid_t, uint_t, int, rcm_info_t **);
int notify_resource_online(char *, pid_t, uint_t, int, rcm_info_t **);
int notify_resource_remove(char *, pid_t, uint_t, int, rcm_info_t **);
int add_resource_client(char *, char *, pid_t, uint_t, rcm_info_t **);
int remove_resource_client(char *, char *, pid_t, uint_t);
int get_resource_info(char *, uint_t, int, rcm_info_t **);
rcm_info_t *rsrc_mod_info();

/* dr request list routines */

rcm_info_t *rsrc_dr_info();
void clean_dr_list();
int dr_req_add(char *, pid_t, uint_t, int, int, timespec_t *, rcm_info_t **);
int dr_req_update(char *, pid_t, uint_t, int, int, rcm_info_t **);
void dr_req_remove(char *, uint_t);
int info_req_add(char *, uint_t, int);
void info_req_remove(int);
int rsrc_check_lock_conflicts(char *, uint_t, int, rcm_info_t **);

/* node related routines */

int rsrc_get_type(const char *);
int rsrc_node_find(char *, int, rsrc_node_t **);
int rsrc_node_add_user(rsrc_node_t *, char *, char *, pid_t, uint_t);
int rsrc_node_remove_user(rsrc_node_t *, char *, pid_t);

/* tree related routines */

int rsrc_usage_info(char *, uint_t, int, rcm_info_t **);
int rsrc_tree_action(rsrc_node_t *, int, tree_walk_arg_t *);

/* database helpers and misc */

void rcmd_set_state(int);
int rcmd_thr_incr(int);
void rcmd_thr_decr(void);
void rcmd_thr_signal(void);
void rcmd_lock_init(void);
void rcmd_db_init(void);
void rcmd_db_sync(void);
void rcmd_db_clean(void);
void rcmd_start_timer(int);
void rcmd_exit(int);
void rcm_log_message(int, char *, ...);
void add_busy_rsrc_to_list(char *, pid_t, int, int, char *, char *,
    rcm_info_t **);
char *resolve_name(char *);
int proc_exist(pid_t);
void *s_malloc(size_t);
void *s_calloc(int, size_t);
void *s_realloc(void *, size_t);
char *s_strdup(const char *);

#ifdef	__cplusplus
}
#endif

#endif /* _RCM_IMPL_H */
