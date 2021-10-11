/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rcm_impl.c	1.1	99/08/10 SMI"

#include <librcm_impl.h>
#include "rcm_impl.h"

/*
 * The following ops are invoked when modules initiate librcm calls which
 * require daemon processing. Cascaded RCM operations must come through
 * this path.
 */
librcm_ops_t rcm_ops = {
	add_resource_client,
	remove_resource_client,
	get_resource_info,
	process_resource_suspend,
	notify_resource_resume,
	process_resource_offline,
	notify_resource_online,
	notify_resource_remove
};

/*
 * Process a request or a notification on a subtree
 */
/*ARGSUSED2*/
static int
common_resource_op(int cmd, char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    timespec_t *interval, rcm_info_t **info)
{
	int error;
	rsrc_node_t *node;
	tree_walk_arg_t arg;

	/*
	 * Find the node (root of subtree) in the resource tree, invoke
	 * appropriate callbacks for all clients hanging off the subtree,
	 * and mark the subtree with the appropriate state.
	 *
	 * NOTE: It's possible the node doesn't exist, which means no RCM
	 * consumer * registered for the resource. In this case we silently
	 * succeed.
	 */
	error = rsrc_node_find(rsrcname, 0, &node);
	if ((error == RCM_SUCCESS) && (node != NULL)) {
		arg.flag = flag;
		arg.info = info;
		arg.seq_num = seq_num;
		arg.interval = interval;
		error = rsrc_tree_action(node, cmd, &arg);
	}
	return (error);
}

/*
 * When a resource is removed, notify all clients who registered for this
 * particular resource.
 */
int
notify_resource_remove(char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    rcm_info_t **info)
{
	int error;

	rcm_log_message(RCM_TRACE2,
	    "notify_resource_remove(%s, %ld, 0x%x, %d)\n",
	    rsrcname, pid, flag, seq_num);

	/*
	 * Mark state as issuing removal notification. Return failure if no DR
	 * request for this node exists.
	 */
	error = dr_req_update(rsrcname, pid, flag, RCM_STATE_REMOVING, seq_num,
	    info);
	if (error != RCM_SUCCESS) {
		return (error);
	}

	error = common_resource_op(CMD_REMOVE, rsrcname, pid, flag, seq_num,
	    NULL, info);

	/*
	 * delete the request entry from DR list
	 */
	dr_req_remove(rsrcname, flag);
	return (error);
}

/*
 * Notify users that a resource has been resumed
 */
int
notify_resource_resume(char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    rcm_info_t **info)
{
	int error;

	rcm_log_message(RCM_TRACE2,
	    "notify_resource_resume(%s, %ld, 0x%x, %d)\n",
	    rsrcname, pid, flag, seq_num);

	/*
	 * Mark state as sending resumption notifications
	 */
	error = dr_req_update(rsrcname, pid, flag, RCM_STATE_RESUMING, seq_num,
	    info);
	if (error != RCM_SUCCESS) {
		return (error);
	}

	error = common_resource_op(CMD_RESUME, rsrcname, pid, flag, seq_num,
	    NULL, info);

	dr_req_remove(rsrcname, flag);
	return (error);
}

/*
 * Notify users that an offlined device is again available
 */
int
notify_resource_online(char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    rcm_info_t **info)
{
	int error;

	rcm_log_message(RCM_TRACE2,
	    "notify_resource_online(%s, %ld, 0x%x, %d)\n",
	    rsrcname, pid, flag, seq_num);

	/*
	 * Mark state as sending onlining notifications
	 */
	error = dr_req_update(rsrcname, pid, flag, RCM_STATE_ONLINING, seq_num,
	    info);
	if (error != RCM_SUCCESS) {
		return (error);
	}

	error = common_resource_op(CMD_ONLINE, rsrcname, pid, flag, seq_num,
	    NULL, info);

	dr_req_remove(rsrcname, flag);
	return (error);
}

/*
 * For offline and suspend, need to get the logic correct here. There are
 * several cases:
 *
 * 1. It is a door call and RCM_QUERY is not set:
 *	run a QUERY; if that succeeds, run the operation.
 *
 * 2. It is a door call and RCM_QUERY is set:
 *	run the QUERY only.
 *
 * 3. It is not a door call:
 *	run the call. Run the call, but look at the flag to see if
 *	lock should be kept.
 */

/*
 * Request permission to suspend a resource
 */
int
process_resource_suspend(char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    timespec_t *interval, rcm_info_t **info)
{
	int error;
	int is_doorcall = (seq_num & SEQ_NUM_MASK);

	rcm_log_message(RCM_TRACE2,
	    "process_resource_suspend(%s, %ld, 0x%x, %d)\n",
	    rsrcname, pid, flag, seq_num);

	/*
	 * Add the request to the list of DR requests
	 */
	error = dr_req_add(rsrcname, pid, flag, RCM_STATE_SUSPENDING, seq_num,
	    interval, info);
	if (error != 0) {
		rcm_log_message(RCM_DEBUG,
		    "suspend %s denied with error = %d\n", rsrcname, error);

		/* preserve EAGAIN if it is a door call */
		return (is_doorcall ? error: RCM_CONFLICT);
	}

	/*
	 * Start with a query
	 */
	if (is_doorcall || (flag & RCM_QUERY)) {
		error = common_resource_op(CMD_SUSPEND, rsrcname, pid,
		    flag | RCM_QUERY, seq_num, interval, info);
		if (error != RCM_SUCCESS) {
			rcm_log_message(RCM_DEBUG, "suspend %s query denied\n",
			    rsrcname);
			(void) dr_req_remove(rsrcname, flag);
			return (RCM_CONFLICT);
		}
	}

	/*
	 * If query only, return now
	 */
	if (flag & RCM_QUERY) {
		(void) dr_req_remove(rsrcname, flag);
		return (RCM_SUCCESS);
	}

	/*
	 * Perform the real operation
	 */
	error = common_resource_op(CMD_SUSPEND, rsrcname, pid, flag, seq_num,
	    interval, info);
	if (error != RCM_SUCCESS) {
		(void) dr_req_update(rsrcname, pid, flag,
		    RCM_STATE_SUSPEND_FAIL, seq_num, info);
		rcm_log_message(RCM_DEBUG, "suspend tree failed for %s\n",
		    rsrcname);
		return (RCM_FAILURE);
	}

	rcm_log_message(RCM_TRACE3, "suspend tree succeeded for %s\n",
	    rsrcname);

	(void) dr_req_update(rsrcname, pid, flag, RCM_STATE_SUSPEND, seq_num,
	    info);
	return (RCM_SUCCESS);
}

/*
 * Process a device removal request, reply is needed
 */
int
process_resource_offline(char *rsrcname, pid_t pid, uint_t flag, int seq_num,
    rcm_info_t **info)
{
	int error;
	int is_doorcall = (seq_num & SEQ_NUM_MASK);

	rcm_log_message(RCM_TRACE2,
	    "process_resource_offline(%s, %ld, 0x%x, %d)\n",
	    rsrcname, pid, flag, seq_num);

	/*
	 * Add the request to the list of dr requests
	 */
	error = dr_req_add(rsrcname, pid, flag, RCM_STATE_OFFLINING, seq_num,
	    NULL, info);
	if (error != RCM_SUCCESS) {
		rcm_log_message(RCM_DEBUG, "offline %s denied with error %d\n",
		    rsrcname, error);

		/*
		 * When called from a module, don't return EAGAIN.
		 * This is to avoid recursion if module always retries.
		 */
		if (!is_doorcall && error == EAGAIN)
			return (RCM_CONFLICT);

		return (error);
	}

	if (is_doorcall || (flag & RCM_QUERY)) {
		error = common_resource_op(CMD_OFFLINE, rsrcname, pid,
		    flag | RCM_QUERY, seq_num, NULL, info);
		if (error != RCM_SUCCESS) {
			rcm_log_message(RCM_DEBUG, "offline %s query denied\n",
			    rsrcname);
			(void) dr_req_remove(rsrcname, flag);
			return (RCM_CONFLICT);
		}
	}

	if (flag & RCM_QUERY) {
		(void) dr_req_remove(rsrcname, flag);
		return (RCM_SUCCESS);
	}

	error = common_resource_op(CMD_OFFLINE, rsrcname, pid, flag, seq_num,
	    NULL, info);
	if (error != RCM_SUCCESS) {
		(void) dr_req_update(rsrcname, pid, flag,
		    RCM_STATE_OFFLINE_FAIL, seq_num, info);
		rcm_log_message(RCM_DEBUG, "offline tree failed for %s\n",
		    rsrcname);
		return (RCM_FAILURE);
	}

	rcm_log_message(RCM_TRACE3, "offline tree succeeded for %s\n",
	    rsrcname);

	(void) dr_req_update(rsrcname, pid, flag, RCM_STATE_OFFLINE, seq_num,
	    info);
	return (error);
}

/*
 * Add a resource client who wishes to interpose on DR.
 * reply needed
 */
int
add_resource_client(char *modname, char *rsrcname, pid_t pid, uint_t flag,
    rcm_info_t **infop)
{
	int error;
	rsrc_node_t *node;
	rcm_info_t *info = NULL;

	rcm_log_message(RCM_TRACE2,
	    "add_resource_client(%s, %s, %ld, 0x%x)\n",
	    modname, rsrcname, pid, flag);

	if (strcmp(rsrcname, "/") == 0) {
		/*
		 * No need to register for /  because it will never go away.
		 */
		rcm_log_message(RCM_INFO, gettext(
		    "registering for / by %s has been turned into a no-op\n"),
		    modname);
		return (RCM_SUCCESS);
	}

	/*
	 * Hold the rcm_req_lock so no dr request may come in while the
	 * registration is in progress.
	 */
	(void) mutex_lock(&rcm_req_lock);
	if (rsrc_check_lock_conflicts(rsrcname, flag, LOCK_FOR_USE, &info)
	    != RCM_SUCCESS) {
		/*
		 * The resource is being DR'ed, so return failure
		 */
		(void) mutex_unlock(&rcm_req_lock);

		/*
		 * If caller doesn't care about info, free it
		 */
		if (infop)
			*infop = info;
		else
			rcm_free_info(info);

		return (RCM_CONFLICT);
	}

	error = rsrc_node_find(rsrcname, RSRC_NODE_CREATE, &node);
	if ((error != RCM_SUCCESS) || (node == NULL)) {
		(void) mutex_unlock(&rcm_req_lock);
		return (RCM_FAILURE);
	}

	error = rsrc_node_add_user(node, rsrcname, modname, pid, flag);
	(void) mutex_unlock(&rcm_req_lock);

	return (error);
}

/*
 * Remove a resource client, who no longer wishes to interpose on DR
 */
int
remove_resource_client(char *modname, char *rsrcname, pid_t pid, uint_t flag)
{
	int error;
	rsrc_node_t *node;

	rcm_log_message(RCM_TRACE2,
	    "remove_resource_client(%s, %s, %ld, 0x%x)\n",
	    modname, rsrcname, pid, flag);

	/*
	 * Allow resource client to leave anytime, assume client knows what
	 * it is trying to do.
	 */
	error = rsrc_node_find(rsrcname, 0, &node);
	if ((error != RCM_SUCCESS) || (node == NULL)) {
		rcm_log_message(RCM_WARNING,
		    gettext("resource %s not found\n"), rsrcname);
		return (ENOENT);
	}

	return (rsrc_node_remove_user(node, modname, pid));
}

/*
 * Reply is needed
 */
int
get_resource_info(char *rsrcname, uint_t flag, int seq_num, rcm_info_t **info)
{
	int rv = RCM_SUCCESS;

	if (flag & RCM_DR_OPERATION) {
		*info = rsrc_dr_info();
	} else if (flag & RCM_MOD_INFO) {
		*info = rsrc_mod_info();
	} else {
		rv = rsrc_usage_info(rsrcname, flag, seq_num, info);
	}

	return (rv);
}
