/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rcm_subr.c	1.1	99/08/10 SMI"

#include "rcm_impl.h"
#include "rcm_module.h"

int need_cleanup;	/* flag indicating if clean up is needed */

static mutex_t mod_lock;	/* protects module list */
static module_t *module_head;	/* linked list of modules */
static rsrc_node_t *rsrc_root;	/* root of all resources */

/*
 * Misc help routines
 */
static void rcmd_db_print();
static void rcm_handle_free(rcm_handle_t *);
static rcm_handle_t *rcm_handle_alloc(module_t *module);
static void rsrc_clients_free(client_t *);

extern void start_polling_thread();

/*
 * translate /dev name to a /devices path
 *
 * N.B. This routine can be enhanced to understand network names
 *	and friendly names in the future.
 */
char *
resolve_name(char *alias)
{
	char *tmp;
	const char *dev = "/dev/";

	if (strlen(alias) == 0)
		return (NULL);

	if (strncmp(alias, dev, strlen(dev)) == 0) {
		/*
		 * Treat /dev/... as a symbolic link
		 */
		tmp = s_malloc(PATH_MAX);
		if (realpath(alias, tmp) != NULL) {
			return (tmp);
		} else {
			free(tmp);
		}
		/* Fail to resolve /dev/ name, use the name as is */
	}

	return (s_strdup(alias));
}

/*
 * Figure out resource type based on "resolved" name
 *
 * N.B. This routine does not figure out file system mount points.
 *	This is determined at runtime when filesys module register
 *	with RCM_FILESYS flag.
 */
int
rsrc_get_type(const char *resolved_name)
{
	if (resolved_name[0] != '/')
		return (RSRC_TYPE_ABSTRACT);

	if (strncmp("/devices/", resolved_name, 9) == 0)
		return (RSRC_TYPE_DEVICE);

	return (RSRC_TYPE_NORMAL);
}

/*
 * Module operations:
 *	module_load, module_unload, module_info, module_attach, module_detach,
 *	cli_module_hold, cli_module_rele
 */

#ifdef	ENABLE_MODULE_DETACH
/*
 * call unregister() entry point to allow module to unregister for
 * resources without getting confused.
 */
static void
module_detach(module_t *module)
{
	struct rcm_mod_ops *ops = module->modops;

	rcm_log_message(RCM_TRACE2, "module_detach(name=%s)\n", module->name);

	ops->rcmop_unregister(module->rcmhandle);
}
#endif	/* ENABLE_MODULE_DETACH */

/*
 * call register() entry point to allow module to register for resources
 */
static void
module_attach(module_t *module)
{
	struct rcm_mod_ops *ops = module->modops;

	rcm_log_message(RCM_TRACE2, "module_attach(name=%s)\n", module->name);

	if (ops->rcmop_register(module->rcmhandle) != RCM_SUCCESS) {
		rcm_log_message(RCM_WARNING,
		    gettext("module %s register() failed\n"), module->name);
	}
}

/*
 * call rmc_mod_info() entry of module
 */
static const char *
module_info(module_t *module)
{
	return (module->info());
}

/*
 * call rmc_mod_fini() entry of module, dlclose module, and free memory
 */
static void
module_unload(module_t *module)
{
	rcm_log_message(RCM_DEBUG, "module_unload(name=%s)\n", module->name);

	rcm_module_close(module->dlhandle);
	rcm_handle_free(module->rcmhandle);
	free(module->name);
	free(module);
}

/*
 * Locate the module, execute rcm_mod_init() and check ops vector version
 */
static module_t *
module_load(char *modname)
{
	module_t *module;

	rcm_log_message(RCM_DEBUG, "module_load(name=%s)\n", modname);

	/*
	 * dlopen the module
	 */
	module = s_calloc(1, sizeof (*module));

	module->dlhandle = rcm_module_open(modname);

	if (module->dlhandle == NULL) {
		rcm_log_message(RCM_NOTICE, gettext("cannot open module %s\n"),
		    modname);
		free(module);
		return (NULL);
	}

	/*
	 * dlsym rcm_mod_init/fini/info() entry points
	 */
	module->init = (struct rcm_mod_ops *(*)())dlsym(module->dlhandle,
	    "rcm_mod_init");
	module->fini = (int (*)())dlsym(module->dlhandle, "rcm_mod_fini");
	module->info = (const char *(*)())dlsym(module->dlhandle,
	    "rcm_mod_info");
	if (module->init == NULL || module->fini == NULL ||
	    module->info == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("missing entries in module %s\n"), modname);
		goto fail;
	}

	if ((module->modops = module->init()) == NULL) {
		rcm_log_message(RCM_ERROR, gettext("cannot init module %s\n"),
		    modname);
		goto fail;
	}

	/*
	 * Check ops vector version
	 */
	if (module->modops->version != RCM_MOD_OPS_VERSION) {
		rcm_log_message(RCM_ERROR,
		    gettext("module %s rejected: version %d not supported\n"),
		    modname, module->modops->version);
		(void) module->fini();
		goto fail;
	}

	/*
	 * Make sure all fields are set
	 */
	if ((module->modops->rcmop_register == NULL) ||
	    (module->modops->rcmop_unregister == NULL) ||
	    (module->modops->rcmop_get_info == NULL) ||
	    (module->modops->rcmop_request_suspend == NULL) ||
	    (module->modops->rcmop_notify_resume == NULL) ||
	    (module->modops->rcmop_request_offline == NULL) ||
	    (module->modops->rcmop_notify_online == NULL) ||
	    (module->modops->rcmop_notify_remove == NULL)) {
		rcm_log_message(RCM_ERROR,
		    gettext("module %s rejected: has NULL ops fields\n"),
		    modname);
		(void) module->fini();
		goto fail;
	}

	module->name = s_strdup(modname);
	module->rcmhandle = rcm_handle_alloc(module);
	return (module);

fail:
	rcm_module_close(module->dlhandle);
	free(module);
	return (NULL);
}

/*
 * add one to module hold count. load the module if not loaded
 */
static module_t *
cli_module_hold(char *modname)
{
	module_t *module;

	rcm_log_message(RCM_TRACE3, "cli_module_hold(%s)\n", modname);

	(void) mutex_lock(&mod_lock);
	module = module_head;
	while (module) {
		if (strcmp(module->name, modname) == 0) {
			break;
		}
		module = module->next;
	}

	if (module) {
		module->ref_count++;
		(void) mutex_unlock(&mod_lock);
		return (module);
	}

	/*
	 * Module not found, attempt to load it
	 */
	if ((module = module_load(modname)) == NULL) {
		(void) mutex_unlock(&mod_lock);
		return (NULL);
	}

	/*
	 * Hold module and link module into module list
	 */
	module->ref_count = 1;
	module->next = module_head;
	module_head = module;

	(void) mutex_unlock(&mod_lock);

	return (module);
}

/*
 * decrement module hold count. Unload it if no reference
 */
static void
cli_module_rele(module_t *module)
{
	module_t *curr = module_head, *prev = NULL;

	rcm_log_message(RCM_TRACE3, "cli_module_rele(name=%s)\n", module->name);

	(void) mutex_lock(&mod_lock);
	if (--(module->ref_count) != 0) {
		(void) mutex_unlock(&mod_lock);
		return;
	}

	rcm_log_message(RCM_TRACE2, "unloading module %s\n", module->name);

	/*
	 * Unlink the module from list
	 */
	while (curr && (curr != module)) {
		prev = curr;
		curr = curr->next;
	}
	if (curr == NULL) {
		rcm_log_message(RCM_ERROR,
		    gettext("Unexpected error: module %s not found.\n"),
		    module->name);
	} else if (prev == NULL) {
		module_head = curr->next;
	} else {
		prev->next = curr->next;
	}
	(void) mutex_unlock(&mod_lock);

	module_unload(module);
}

/*
 * Gather usage info be passed back to requester. Discard info if user does
 * not care (list == NULL).
 *
 * By convention, caller malloc's usage and does not refer to it again after
 * calling add_busy_rsrc_to_list().
 */
void
add_busy_rsrc_to_list(char *alias, pid_t pid, int state, int seq_num,
    char *modname, char *usage, rcm_info_t **list)
{
	rcm_info_t *info;

	if (list == NULL) {
		free(usage);
		return;
	}

	info = s_calloc(1, sizeof (*info));
	info->rsrcname = s_strdup(alias);
	info->info = usage;	/* usage is malloc'ed by module */
	info->seq_num = seq_num;
	info->pid = pid;
	info->state = state;

	if (modname)
		info->modname = s_strdup(modname);

	/* link info into list */
	info->next = *list;
	*list = info;
}

/*
 * Resource client realted operations:
 *	rsrc_client_alloc, rsrc_client_find, rsrc_client_add,
 *	rsrc_client_remove, rsrc_client_action,	rsrc_client_action_list
 */

/* Allocate rsrc_client_t structure. Load module if necessary. */
/*ARGSUSED*/
static client_t *
rsrc_client_alloc(char *alias, char *modname, pid_t pid, uint_t flag)
{
	client_t *client;
	module_t *mod;

	assert((alias != NULL) && (modname != NULL));

	rcm_log_message(RCM_TRACE4, "rsrc_client_alloc(%s, %s, %ld)\n",
	    alias, modname, pid);

	if ((mod = cli_module_hold(modname)) == NULL) {
		return (NULL);
	}

	client = s_calloc(1, sizeof (client_t));
	client->module = mod;
	client->pid = pid;
	client->alias = s_strdup(alias);
	client->state = RCM_STATE_ONLINE;
	return (client);
}

/* Find client in list matching modname and pid */
static client_t *
rsrc_client_find(char *modname, pid_t pid, client_t **list)
{
	client_t *client = *list;

	rcm_log_message(RCM_TRACE4, "rsrc_client_find(%s, %ld, %p)\n",
	    modname, pid, (void *)list);

	while (client) {
		if ((client->pid == pid) &&
		    strcmp(modname, client->module->name) == 0) {
			break;
		}
		client = client->next;
	}
	return (client);
}

/* Add a client to client list */
static void
rsrc_client_add(client_t *client, client_t **list)
{
	rcm_log_message(RCM_TRACE4, "rsrc_client_add: %s, %s, %ld\n",
	    client->alias, client->module->name, client->pid);

	client->next = *list;
	*list = client;
}

/* Remove client from list and destroy it */
static void
rsrc_client_remove(client_t *client, client_t **list)
{
	client_t *tmp, *prev = NULL;

	rcm_log_message(RCM_TRACE4, "rsrc_client_remove: %s, %s, %ld\n",
	    client->alias, client->module->name, client->pid);

	tmp = *list;
	while (tmp) {
		if (client != tmp) {
			prev = tmp;
			tmp = tmp->next;
			continue;
		}
		if (prev) {
			prev->next = tmp->next;
		} else {
			*list = tmp->next;
		}
		tmp->next = NULL;
		rsrc_clients_free(tmp);
		return;
	}
}

/* Free a list of clients. Called from cleanup thread only */
static void
rsrc_clients_free(client_t *list)
{
	client_t *client = list;

	while (client) {
		if (client->module) {
			cli_module_rele(client->module);
		}
		list = client->next;
		if (client->alias) {
			free(client->alias);
		}
		free(client);
		client = list;
	}
}

/*
 * Invoke a callback into a single client
 * This is the core of rcm_mod_ops interface
 */
static int
rsrc_client_action(client_t *client, int cmd, void *arg)
{
	int rval = RCM_SUCCESS;
	char *usage = NULL;
	rcm_handle_t *hdl;
	rcm_info_t *info = NULL;
	struct rcm_mod_ops *ops = client->module->modops;
	tree_walk_arg_t *targ = (tree_walk_arg_t *)arg;

	rcm_log_message(RCM_TRACE4, "rsrc_client_action: %s, %s, cmd=%d\n",
	    client->alias, client->module->name, cmd);

	/*
	 * Create a per-operation handle, increment seq_num by 1 so we will
	 * know if a module uses this handle to callback into rcm_daemon.
	 */
	hdl = rcm_handle_alloc(client->module);
	hdl->seq_num = targ->seq_num + 1;

	switch (cmd) {

	case CMD_GETINFO:
		rval = ops->rcmop_get_info(hdl, client->alias, client->pid,
		    targ->flag, &usage, &info);
		if (rval != RCM_SUCCESS) {
			usage = s_strdup("unknown");
		}
		break;

	case CMD_SUSPEND:
		if (client->state == RCM_STATE_SUSPEND) {
			break;
		}

		if ((targ->flag & RCM_QUERY) != 0) {
			/* quick check, don't change state */
			rcm_log_message(RCM_DEBUG, "query suspend %s\n",
			    client->alias);
			rval = ops->rcmop_request_suspend(hdl, client->alias,
			    client->pid, targ->interval, targ->flag, &usage,
			    &info);
			break;
		}

		rcm_log_message(RCM_DEBUG, "suspending %s\n", client->alias);

		client->state = RCM_STATE_SUSPENDING;
		rval = ops->rcmop_request_suspend(hdl, client->alias,
		    client->pid, targ->interval, targ->flag, &usage, &info);

		if (rval == RCM_SUCCESS) {
			client->state = RCM_STATE_SUSPEND;
		} else {
			client->state = RCM_STATE_SUSPEND_FAIL;
		}
		break;

	case CMD_RESUME:
		if (client->state == RCM_STATE_ONLINE) {
			break;
		}
		client->state = RCM_STATE_RESUMING;
		rval = ops->rcmop_notify_resume(hdl, client->alias, client->pid,
		    targ->flag, &usage, &info);

		/* online state is unconditional */
		client->state = RCM_STATE_ONLINE;
		break;

	case CMD_OFFLINE:
		if (client->state == RCM_STATE_OFFLINE) {
			break;
		}

		if ((targ->flag & RCM_QUERY) != 0) {
			/* quick check, no state change */
			rcm_log_message(RCM_DEBUG, "query offline %s\n",
			    client->alias);
			rval = ops->rcmop_request_offline(hdl, client->alias,
			    client->pid, targ->flag, &usage, &info);
			break;
		}

		rcm_log_message(RCM_DEBUG, "offlining %s\n", client->alias);
		client->state = RCM_STATE_OFFLINING;

		rval = ops->rcmop_request_offline(hdl, client->alias,
		    client->pid, targ->flag, &usage, &info);

		if (rval == RCM_SUCCESS) {
			client->state = RCM_STATE_OFFLINE;
		} else {
			client->state = RCM_STATE_OFFLINE_FAIL;
		}
		break;

	case CMD_ONLINE:
		if (client->state == RCM_STATE_ONLINE) {
			break;
		}

		rcm_log_message(RCM_DEBUG, "onlining %s\n", client->alias);

		client->state = RCM_STATE_ONLINING;
		rval = ops->rcmop_notify_online(hdl, client->alias, client->pid,
		    targ->flag, &usage, &info);
		client->state = RCM_STATE_ONLINE;
		break;

	case CMD_REMOVE:
		rcm_log_message(RCM_DEBUG, "removing %s\n", client->alias);
		client->state = RCM_STATE_REMOVING;
		rval = ops->rcmop_notify_remove(hdl, client->alias, client->pid,
		    targ->flag, &usage, &info);
		client->state = RCM_STATE_REMOVE;
		break;

	default:
		rcm_log_message(RCM_ERROR, gettext("unknown command %d\n"),
		    cmd);
		rval = RCM_FAILURE;
		break;
	}

	/* reset error code to the most significant error */
	if (rval != RCM_SUCCESS)
		targ->retcode = rval;

	if (usage) {
		add_busy_rsrc_to_list(client->alias, client->pid, client->state,
		    targ->seq_num, client->module->name, usage, targ->info);
	}

	if (info) {
		if (targ->info) {
			rcm_append_info(targ->info, info);
		} else {
			rcm_free_info(info);
		}
	}

	rcm_handle_free(hdl);
	return (rval);
}

/*
 * invoke a callback into a list of clients, return 0 if all success
 */
static int
rsrc_client_action_list(client_t *list, int cmd, void *arg)
{
	int error, rval = RCM_SUCCESS;

	while (list) {
		client_t *client = list;
		list = client->next;

		if (client->state == RCM_STATE_REMOVE)
			continue;

		error = rsrc_client_action(client, cmd, arg);
		if (error != RCM_SUCCESS) {
			rval = error;
		}
	}

	return (rval);
}

/*
 * Node realted operations:
 *
 *	rn_alloc, rn_free, rn_find_child,
 *	rn_get_child, rn_get_sibling,
 *	rsrc_node_find, rsrc_node_add_user, rsrc_node_remove_user,
 */

/* Allocate node based on a logical or physical name */
static rsrc_node_t *
rn_alloc(char *name, int type)
{
	rsrc_node_t *node;

	rcm_log_message(RCM_TRACE4, "rn_alloc(%s, %d)\n", name, type);

	node = s_calloc(1, sizeof (*node));
	node->name = s_strdup(name);
	node->type = type;

	return (node);
}

/*
 * Free node along with its siblings and children
 */
static void
rn_free(rsrc_node_t *node)
{
	if (node == NULL) {
		return;
	}

	if (node->child) {
		rn_free(node->child);
	}

	if (node->sibling) {
		rn_free(node->sibling);
	}

	rsrc_clients_free(node->users);
	free(node->name);
	free(node);
}

/*
 * Find next sibling
 */
static rsrc_node_t *
rn_get_sibling(rsrc_node_t *node)
{
	return (node->sibling);
}

/*
 * Find first child
 */
static rsrc_node_t *
rn_get_child(rsrc_node_t *node)
{
	return (node->child);
}

/*
 * Find child named childname. Create it if flag is RSRC_NODE_CRTEATE
 */
static rsrc_node_t *
rn_find_child(rsrc_node_t *parent, char *childname, int flag, int type)
{
	rsrc_node_t *child = parent->child;
	rsrc_node_t *new, *prev = NULL;

	rcm_log_message(RCM_TRACE4,
	    "rn_find_child(parent=%s, child=%s, 0x%x, %d)\n",
	    parent->name, childname, flag, type);

	/*
	 * Children are ordered based on strcmp.
	 */
	while (child && (strcmp(child->name, childname) < 0)) {
		prev = child;
		child = child->sibling;
	}

	if (child && (strcmp(child->name, childname) == 0)) {
		return (child);
	}

	if (flag != RSRC_NODE_CREATE)
		return (NULL);

	new = rn_alloc(childname, type);
	new->parent = parent;
	new->sibling = child;

	/*
	 * Set this linkage last so we don't break ongoing operations.
	 *
	 * N.B. Assume setting a pointer is an atomic operation.
	 */
	if (prev == NULL) {
		parent->child = new;
	} else {
		prev->sibling = new;
	}

	return (new);
}

/*
 * Pathname related help functions
 */
static void
pn_preprocess(char *pathname, int type)
{
	char *tmp;

	if (type != RSRC_TYPE_DEVICE)
		return;

	/*
	 * For devices, convert ':' to '/' (treat minor nodes and children)
	 */
	tmp = strchr(pathname, ':');
	if (tmp == NULL)
		return;

	*tmp = '/';
}

static char *
pn_getnextcomp(char *pathname, char **lasts)
{
	char *slash;

	if (pathname == NULL)
		return (NULL);

	/* skip slashes' */
	while (*pathname == '/')
		++pathname;

	if (*pathname == '\0')
		return (NULL);

	slash = strchr(pathname, '/');
	if (slash != NULL) {
		*slash = '\0';
		*lasts = slash + 1;
	} else {
		*lasts = NULL;
	}

	return (pathname);
}

/*
 * Find a node in tree based on device, which is the physical pathname
 * of the form /sbus@.../esp@.../sd@...
 */
int
rsrc_node_find(char *rsrcname, int flag, rsrc_node_t **nodep)
{
	char *pathname, *nodename, *lasts;
	rsrc_node_t *node;
	int type;

	rcm_log_message(RCM_TRACE4, "rn_node_find(%s, 0x%x)\n", rsrcname, flag);

	/*
	 * For RSRC_TYPE_ABSTRACT, look under /ABSTRACT. For other types,
	 * look under /SYSTEM.
	 */
	pathname = resolve_name(rsrcname);
	if (pathname == NULL)
		return (EINVAL);

	type = rsrc_get_type(pathname);
	switch (type) {
	case RSRC_TYPE_DEVICE:
	case RSRC_TYPE_NORMAL:
		node = rn_find_child(rsrc_root, "SYSTEM", RSRC_NODE_CREATE,
		    RSRC_TYPE_NORMAL);
		break;

	case RSRC_TYPE_ABSTRACT:
		node = rn_find_child(rsrc_root, "ABSTRACT", RSRC_NODE_CREATE,
		    RSRC_TYPE_NORMAL);
		break;

	default:
		/* just to make sure */
		free(pathname);
		return (EINVAL);
	}

	/*
	 * Find position of device within tree. Upon exiting the loop, device
	 * should be placed between prev and curr.
	 */
	pn_preprocess(pathname, type);
	lasts = pathname;
	while ((nodename = pn_getnextcomp(lasts, &lasts)) != NULL) {
		rsrc_node_t *parent = node;
		node = rn_find_child(parent, nodename, flag, type);
		if (node == NULL) {
			assert((flag & RSRC_NODE_CREATE) == 0);
			free(pathname);
			*nodep = NULL;
			return (RCM_SUCCESS);
		}
	}
	free(pathname);
	*nodep = node;
	return (RCM_SUCCESS);
}

/*
 * add a usage client to a node
 */
/*ARGSUSED*/
int
rsrc_node_add_user(rsrc_node_t *node, char *alias, char *modname, pid_t pid,
    uint_t flag)
{
	client_t *user;

	rcm_log_message(RCM_TRACE3,
	    "rsrc_node_add_user(%s, %s, %s, %ld, 0x%x)\n",
	    node->name, alias, modname, pid, flag);

	user = rsrc_client_find(modname, pid, &node->users);
	if ((user != NULL) && (user->state != RCM_STATE_REMOVE)) {
		rcm_log_message(RCM_NOTICE,
		    gettext("already registered: module=%s, pid=%d, rsrc=%s\n"),
		    modname, pid, alias);
		return (EALREADY);
	}

	if (user != NULL) {
		user->state = RCM_STATE_ONLINE;
		return (RCM_SUCCESS);
	}

	if ((user = rsrc_client_alloc(alias, modname, pid, flag)) != NULL) {
		rsrc_client_add(user, &node->users);
	}
	if (flag & RCM_FILESYS)
		node->type = RSRC_TYPE_FILESYS;

	return (RCM_SUCCESS);
}

/*
 * remove a usage client of a node
 */
int
rsrc_node_remove_user(rsrc_node_t *node, char *modname, pid_t pid)
{
	client_t *user;

	rcm_log_message(RCM_TRACE3, "rsrc_node_remove_user(%s, %s %ld)\n",
	    node->name, modname, pid);

	user = rsrc_client_find(modname, pid, &node->users);
	if ((user == NULL) || (user->state == RCM_STATE_REMOVE)) {
		rcm_log_message(RCM_NOTICE, gettext(
		    "client not registered: module=%s, pid=%d, dev=%s\n"),
		    modname, pid, node->name);
		return (ENOENT);
	}

	/*
	 * Mark the client as removed
	 */
	user->state = RCM_STATE_REMOVE;
	return (RCM_SUCCESS);
}

/*
 * Tree walking function - rsrc_walk
 */

#define	MAX_TREE_DEPTH		32

#define	RN_WALK_CONTINUE	0
#define	RN_WALK_PRUNESIB	1
#define	RN_WALK_PRUNECHILD	2
#define	RN_WALK_TERMINATE	3

#define	EMPTY_STACK(sp)		((sp)->depth == 0)
#define	TOP_NODE(sp)		((sp)->node[(sp)->depth - 1])
#define	PRUNE_SIB(sp)		((sp)->prunesib[(sp)->depth - 1])
#define	PRUNE_CHILD(sp)		((sp)->prunechild[(sp)->depth - 1])
#define	POP_STACK(sp)		((sp)->depth)--
#define	PUSH_STACK(sp, rn)	\
	(sp)->node[(sp)->depth] = (rn);	\
	(sp)->prunesib[(sp)->depth] = 0;	\
	(sp)->prunechild[(sp)->depth] = 0;	\
	((sp)->depth)++

struct rn_stack {
	rsrc_node_t *node[MAX_TREE_DEPTH];
	char	prunesib[MAX_TREE_DEPTH];
	char	prunechild[MAX_TREE_DEPTH];
	int	depth;
};

/* walking one node and update node stack */
/*ARGSUSED*/
static void
walk_one_node(struct rn_stack *sp, void *arg,
    int (*node_callback)(rsrc_node_t *, void *))
{
	int prunesib;
	rsrc_node_t *child, *sibling;
	rsrc_node_t *node = TOP_NODE(sp);

	rcm_log_message(RCM_TRACE4, "walk_one_node(%s)\n", node->name);

	switch (node_callback(node, arg)) {
	case RN_WALK_TERMINATE:
		POP_STACK(sp);
		while (!EMPTY_STACK(sp)) {
			node = TOP_NODE(sp);
			POP_STACK(sp);
		}
		return;

	case RN_WALK_PRUNESIB:
		PRUNE_SIB(sp) = 1;
		break;

	case RN_WALK_PRUNECHILD:
		PRUNE_CHILD(sp) = 1;
		break;

	case RN_WALK_CONTINUE:
	default:
		break;
	}

	/*
	 * Push child on the stack
	 */
	if (!PRUNE_CHILD(sp) && (child = rn_get_child(node)) != NULL) {
		PUSH_STACK(sp, child);
		return;
	}

	/*
	 * Pop the stack till a node's sibling can be pushed
	 */
	prunesib = PRUNE_SIB(sp);
	POP_STACK(sp);
	while (!EMPTY_STACK(sp) &&
	    (prunesib || (sibling = rn_get_sibling(node)) == NULL)) {
		node = TOP_NODE(sp);
		prunesib = PRUNE_SIB(sp);
		POP_STACK(sp);
	}

	if (EMPTY_STACK(sp)) {
		return;
	}

	/*
	 * push sibling onto the stack
	 */
	PUSH_STACK(sp, sibling);
}

/*
 * walk tree rooted at root in child-first order
 */
static void
rsrc_walk(rsrc_node_t *root, void *arg,
    int (*node_callback)(rsrc_node_t *, void *))
{
	struct rn_stack stack;

	rcm_log_message(RCM_TRACE3, "rsrc_walk(%s)\n", root->name);

	/*
	 * Push root on stack and walk in child-first order
	 */
	stack.depth = 0;
	PUSH_STACK(&stack, root);
	PRUNE_SIB(&stack) = 1;

	while (!EMPTY_STACK(&stack)) {
		walk_one_node(&stack, arg, node_callback);
	}
}

/*
 * Callback for a command action on a node
 */
static int
node_action(rsrc_node_t *node, void *arg)
{
	tree_walk_arg_t *targ = (tree_walk_arg_t *)arg;
	uint_t flag = targ->flag;

	rcm_log_message(RCM_TRACE4, "node_action(%s)\n", node->name);

	/*
	 * If flag indicates operation on a filesystem, we don't callback on
	 * the filesystem root to avoid infinite recursion on filesystem module.
	 *
	 * N.B. Such request should only come from filesystem RCM module.
	 */
	if (flag & RCM_FILESYS) {
		assert(node->type == RSRC_TYPE_FILESYS);
		targ->flag &= ~RCM_FILESYS;
		return (RN_WALK_CONTINUE);
	}

	/*
	 * Execute state change callback
	 */
	(void) rsrc_client_action_list(node->users, targ->cmd, arg);

	/*
	 * Upon hitting a filesys root, prune children.
	 * The filesys module should have taken care of
	 * children by now.
	 */
	if (node->type == RSRC_TYPE_FILESYS)
		return (RN_WALK_PRUNECHILD);

	return (RN_WALK_CONTINUE);
}

/*
 * Execute a command on a subtree under root.
 */
int
rsrc_tree_action(rsrc_node_t *root, int cmd, tree_walk_arg_t *arg)
{
	rcm_log_message(RCM_TRACE2, "tree_action(%s, %d)\n", root->name, cmd);

	arg->cmd = cmd;
	arg->retcode = RCM_SUCCESS;
	rsrc_walk(root, (void *)arg, node_action);

	return (arg->retcode);
}

/*
 * Get info on current regsitrations
 */
int
rsrc_usage_info(char *rsrcname, uint_t flag, int seq_num, rcm_info_t **info)
{
	rsrc_node_t *node;
	rcm_info_t *result = NULL;
	tree_walk_arg_t arg;
	int initial_req;
	int rv;

	rcm_log_message(RCM_TRACE2, "rsrc_usage_info(%s, 0x%x, %d)\n",
	    rsrcname, flag, seq_num);

	if (flag & RCM_INCLUDE_DEPENDENT) {
		initial_req = ((seq_num & SEQ_NUM_MASK) == 0);

		/*
		 * if redundant request, skip the operation
		 */
		if (info_req_add(rsrcname, flag, seq_num) != 0) {
			return (RCM_SUCCESS);
		}
	}

	arg.flag = flag;
	arg.info = &result;
	arg.seq_num = seq_num;

	rv = rsrc_node_find(rsrcname, 0, &node);
	if ((rv != RCM_SUCCESS) || (node == NULL)) {
		goto out;
	}

	if (flag & RCM_INCLUDE_SUBTREE) {
		/*
		 * Get tree usage
		 */
		(void) rsrc_tree_action(node, CMD_GETINFO, &arg);
		goto out;
	}

	/*
	 * Usage of this node only
	 */
	arg.cmd = CMD_GETINFO;
	(void) node_action(node, (void *)&arg);

out:
	if ((flag & RCM_INCLUDE_DEPENDENT) && initial_req) {
		info_req_remove(seq_num);
	}
	*info = result;
	return (rv);
}

/*
 * Get the list of currently loaded module
 */
rcm_info_t *
rsrc_mod_info()
{
	module_t *mod;
	rcm_info_t *info = NULL;

	(void) mutex_lock(&mod_lock);
	mod = module_head;
	while (mod) {
		char *modinfo = s_strdup(module_info(mod));
		add_busy_rsrc_to_list("dummy", 0, 0, 0, mod->name, modinfo,
		    &info);
		mod = mod->next;
	}
	(void) mutex_unlock(&mod_lock);

	return (info);
}

/*
 * Initialize resource map - load all modules
 */
void
rcmd_db_init()
{
	char *tmp;
	DIR *mod_dir;
	struct dirent *retp, *entp;
	int i;
	char *dir_name;

#ifdef	lint
extern int readdir_r(DIR *, struct dirent *, struct dirent **);
#endif

	rcm_log_message(RCM_DEBUG, "rcmd_db_init(): initialize database\n");

	rsrc_root = rn_alloc("/", RSRC_TYPE_NORMAL);

	entp = s_malloc(PATH_MAX + 1 + sizeof (struct dirent));

	for (i = 0; (dir_name = rcm_module_dir(i)) != NULL; i++) {

		if ((mod_dir = opendir(dir_name)) == NULL) {
			continue;	/* try next directory */
		}

		rcm_log_message(RCM_TRACE2, "search directory %s\n", dir_name);

		while (readdir_r(mod_dir, entp, &retp) == 0) {
			module_t *module;

			if (retp == NULL) {
				break;
			}

			if (((tmp = strstr(entp->d_name, RCM_MODULE_SUFFIX)) ==
			    NULL) || (tmp[strlen(RCM_MODULE_SUFFIX)] != '\0')) {
				continue;
			}

			*tmp = '\0';
			module = cli_module_hold(entp->d_name);
			if (module == NULL) {
				rcm_log_message(RCM_ERROR,
				    gettext("%s_rcm.so: failed to load\n"),
				    entp->d_name);
				continue;
			}

			if (module->ref_count == 1) {
				/*
				 * ask module to register for resource 1st time
				 */
				module_attach(module);
			}
			cli_module_rele(module);
		}
		(void) closedir(mod_dir);
	}

	free(entp);
	rcmd_db_print();
}

/*
 * sync resource map - ask all modules to register again
 */
void
rcmd_db_sync()
{
	static time_t sync_time = (time_t)-1;
	const time_t interval = 5;	/* resync at most every 5 sec */

	module_t *mod;
	time_t curr = time(NULL);

	if ((sync_time != (time_t)-1) && (curr - sync_time < interval))
		return;

	sync_time = curr;
	(void) mutex_lock(&mod_lock);
	mod = module_head;
	while (mod) {
		/*
		 * Hold module by incrementing ref count and release
		 * mod_lock to avoid deadlock, since rcmop_register()
		 * may callback into the daemon and request mod_lock.
		 */
		mod->ref_count++;
		(void) mutex_unlock(&mod_lock);

		mod->modops->rcmop_register(mod->rcmhandle);

		(void) mutex_lock(&mod_lock);
		mod->ref_count--;
		mod = mod->next;
	}
	(void) mutex_unlock(&mod_lock);
}

/*
 * Determine if a process is alive
 */
int
proc_exist(pid_t pid)
{
	char path[64];
	const char *procfs = "/proc";
	struct stat sb;

	if (pid == (pid_t)0) {
		return (1);
	}

	(void) sprintf(path, "%s/%ld", procfs, pid);
	return (stat(path, &sb) == 0);
}

/*
 * Cleaup client list
 *
 * N.B. This routine runs in a single-threaded environment only. It is only
 *	called by the cleanup thread, which never runs in parallel with other
 *	threads.
 */
static void
clean_client_list(client_t **listp)
{
	client_t *client = *listp;

	/*
	 * Cleanup notification clients for which pid no longer exists
	 */
	while (client) {
		if ((client->state != RCM_STATE_REMOVE) &&
		    proc_exist(client->pid)) {
			listp = &client->next;
			client = *listp;
			continue;
		}

		/*
		 * Destroy this client_t. rsrc_client_remove updates
		 * listp to point to the next client.
		 */
		rsrc_client_remove(client, listp);
		client = *listp;
	}
}

/*ARGSUSED*/
static int
clean_node(rsrc_node_t *node, void *arg)
{
	rcm_log_message(RCM_TRACE4, "clean_node(%s)\n", node->name);

	clean_client_list(&node->users);

	return (RN_WALK_CONTINUE);
}

static void
clean_rsrc_tree()
{
	rcm_log_message(RCM_TRACE4,
	    "clean_rsrc_tree(): delete stale dr clients\n");

	rsrc_walk(rsrc_root, NULL, clean_node);
}

static void
db_clean()
{
	extern barrier_t barrier;
	extern void clean_dr_list();

	for (;;) {
		mutex_lock(&rcm_req_lock);
		start_polling_thread();
		mutex_unlock(&rcm_req_lock);

		mutex_lock(&barrier.lock);
		while (need_cleanup == 0)
			cond_wait(&barrier.cv, &barrier.lock);
		mutex_unlock(&barrier.lock);

		/*
		 * Make sure all other threads are either blocked or exited.
		 */
		rcmd_set_state(RCMD_CLEANUP);

		need_cleanup = 0;

		/*
		 * clean dr_req_list
		 */
		clean_dr_list();

		/*
		 * clean resource tree
		 */
		clean_rsrc_tree();

		rcmd_set_state(RCMD_NORMAL);
	}
}

void
rcmd_db_clean()
{
	rcm_log_message(RCM_DEBUG,
	    "rcm_db_clean(): launch thread to clean database\n");

	if (thr_create(NULL, NULL, (void *(*)(void *))db_clean,
	    NULL, THR_DETACHED, NULL) != 0) {
		rcm_log_message(RCM_WARNING,
		    gettext("failed to create cleanup thread %s\n"),
		    strerror(errno));
	}
}

/*ARGSUSED*/
static int
print_node(rsrc_node_t *node, void *arg)
{
	client_t *user;

	rcm_log_message(RCM_DEBUG, "rscname: %s, state = 0x%x\n", node->name);
	rcm_log_message(RCM_DEBUG, "	users:\n");

	if ((user = node->users) == NULL) {
		rcm_log_message(RCM_DEBUG, "    none\n");
		return (RN_WALK_CONTINUE);
	}

	while (user) {
		rcm_log_message(RCM_DEBUG, "	%s, %d, %s\n",
		    user->module->name, user->pid, user->alias);
		user = user->next;
	}
	return (RN_WALK_CONTINUE);
}

static void
rcmd_db_print()
{
	module_t *mod;

	rcm_log_message(RCM_DEBUG, "modules:\n");
	(void) mutex_lock(&mod_lock);
	mod = module_head;
	while (mod) {
		rcm_log_message(RCM_DEBUG, "	%s\n", mod->name);
		mod = mod->next;
	}
	(void) mutex_unlock(&mod_lock);

	rcm_log_message(RCM_DEBUG, "\nresource tree:\n");

	rsrc_walk(rsrc_root, NULL, print_node);

	rcm_log_message(RCM_DEBUG, "\n");
}

/*
 * Allocate handle from calling into each RCM module
 */
static rcm_handle_t *
rcm_handle_alloc(module_t *module)
{
	rcm_handle_t *hdl;

	hdl = s_malloc(sizeof (rcm_handle_t));

	hdl->modname = module->name;
	hdl->pid = 0;
	hdl->lrcm_ops = &rcm_ops;	/* for callback into daemon directly */

	return (hdl);
}

/*
 * Free rcm_handle_t
 */
static void
rcm_handle_free(rcm_handle_t *handle)
{
	free(handle);
}

/*
 * help function that exit on memory outage
 */
void *
s_malloc(size_t size)
{
	void *buf = malloc(size);

	if (buf == NULL) {
		rcmd_exit(ENOMEM);
	}
	return (buf);
}

void *
s_calloc(int n, size_t size)
{
	void *buf = calloc(n, size);

	if (buf == NULL) {
		rcmd_exit(ENOMEM);
	}
	return (buf);
}

void *
s_realloc(void *ptr, size_t size)
{
	void *new = realloc(ptr, size);

	if (new == NULL) {
		rcmd_exit(ENOMEM);
	}
	return (new);
}

char *
s_strdup(const char *str)
{
	char *buf = strdup(str);

	if (buf == NULL) {
		rcmd_exit(ENOMEM);
	}
	return (buf);
}
