/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfga_rcm.c	1.3	99/11/10 SMI"

#include "cfga_scsi.h"

#define	MAX_FORMAT	80
#define	DEVICES		"/devices"

typedef struct {
	char *bus_path;
	char *filter;
	char **errstring;
	scfga_ret_t ret;
	cfga_flags_t flags;
	scfga_ret_t (*func)(char *, char *, char **, cfga_flags_t);
} walkargs_t;

static scfga_ret_t scsi_rcm_info_table(rcm_info_t *, char **);
static int scsi_rcm_process_node(di_node_t, void *);
static scfga_ret_t scsi_rcm_init(char *, cfga_flags_t, char **, uint_t *,
    char **);
static char *chop_minor(char *);

static rcm_handle_t *rcm_handle = NULL;
static mutex_t rcm_handle_lock;

/*
 * scsi_rcm_offline()
 *
 *	Offline SCSI resource consumers.
 */
scfga_ret_t
scsi_rcm_offline(char *rsrc, char **errstring, cfga_flags_t flags)
{
	int rret;
	uint_t rflags = 0;
	char *rsrc_fixed;
	rcm_info_t *rinfo = NULL;
	scfga_ret_t ret = SCFGA_OK;

	if ((ret = scsi_rcm_init(rsrc, flags, errstring, &rflags, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	if ((rret = rcm_request_offline(rcm_handle, rsrc_fixed, rflags, &rinfo))
	    != RCM_SUCCESS) {
		cfga_err(errstring, 0, ERRARG_RCM_OFFLINE, rsrc_fixed, 0);
		if (rinfo) {
			(void) scsi_rcm_info_table(rinfo, errstring);
			rcm_free_info(rinfo);
		}
		if (rret == RCM_FAILURE)
			(void) scsi_rcm_online(rsrc, errstring, flags);
		ret = SCFGA_BUSY;
	}

	S_FREE(rsrc_fixed);

	return (ret);
}

/*
 * scsi_rcm_online()
 *
 *	Online SCSI resource consumers that were previously offlined.
 */
scfga_ret_t
scsi_rcm_online(char *rsrc, char **errstring, cfga_flags_t flags)
{
	int rret;
	char *rsrc_fixed;
	rcm_info_t *rinfo = NULL;
	scfga_ret_t ret = SCFGA_OK;

	if ((ret = scsi_rcm_init(rsrc, flags, errstring, NULL, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	if ((rret = rcm_notify_online(rcm_handle, rsrc_fixed, 0, &rinfo))
	    != RCM_SUCCESS && rinfo != NULL) {
		cfga_err(errstring, 0, ERRARG_RCM_ONLINE, rsrc_fixed, 0);
		(void) scsi_rcm_info_table(rinfo, errstring);
		rcm_free_info(rinfo);
		ret = SCFGA_BUSY;
	}

	S_FREE(rsrc_fixed);

	return (ret);
}

/*
 * scsi_rcm_remove()
 *
 *	Remove SCSI resource consumers after their kernel removal.
 */
scfga_ret_t
scsi_rcm_remove(char *rsrc, char **errstring, cfga_flags_t flags)
{
	char *rsrc_fixed;
	rcm_info_t *rinfo = NULL;
	scfga_ret_t ret = SCFGA_OK;

	if ((ret = scsi_rcm_init(rsrc, flags, errstring, NULL, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	if (rcm_notify_remove(rcm_handle, rsrc_fixed, 0, &rinfo)
	    != RCM_SUCCESS) {
		cfga_err(errstring, 0, ERRARG_RCM_REMOVE, rsrc_fixed, 0);
		if (rinfo) {
			(void) scsi_rcm_info_table(rinfo, errstring);
			rcm_free_info(rinfo);
		}
		ret = SCFGA_BUSY;
	}

	S_FREE(rsrc_fixed);

	return (ret);
}

/*
 * scsi_rcm_suspend()
 *
 *	Suspend SCSI resource consumers before a bus quiesce.
 */
scfga_ret_t
scsi_rcm_suspend(char *rsrc, char *filter, char **errstring, cfga_flags_t flags)
{
	int rret;
	uint_t rflags = 0;
	char *rsrc_fixed;
	char *filter_fixed;
	char *rsrc_devpath;
	rcm_info_t *rinfo = NULL;
	di_node_t node;
	scfga_ret_t ret = SCFGA_OK;
	walkargs_t walkargs;
	timespec_t zerotime = { 0, 0 };

	if ((ret = scsi_rcm_init(rsrc, flags, errstring, &rflags, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	/* If a filter is provided, ensure that it makes sense */
	if (filter != NULL && strstr(filter, rsrc) != filter) {
		S_FREE(rsrc_fixed);
		cfga_err(errstring, 0, ERR_APID_INVAL, 0);
		return (SCFGA_ERR);
	}

	/*
	 * If no filter is specified: attempt a suspension on the resource,
	 * directly.
	 */
	if (filter == NULL) {
		if ((rret = rcm_request_suspend(rcm_handle, rsrc_fixed, rflags,
		    &zerotime, &rinfo)) != RCM_SUCCESS) {
			cfga_err(errstring, 0, ERRARG_RCM_SUSPEND, rsrc_fixed,
			    0);
			if (rinfo) {
				(void) scsi_rcm_info_table(rinfo, errstring);
				rcm_free_info(rinfo);
			}
			if (rret == RCM_FAILURE)
				(void) scsi_rcm_resume(rsrc, filter, errstring,
				    (flags & (~CFGA_FLAG_FORCE)));
			ret = SCFGA_BUSY;
		}
		S_FREE(rsrc_fixed);
		return (ret);
	}

	/*
	 * If a filter is specified: open the resource with libdevinfo, walk
	 * through its nodes, and attempt a suspension of each node that
	 * mismatches the filter.
	 */

	/* Chop off the filter's minor name */
	if ((filter_fixed = chop_minor(filter)) == NULL)
		return (SCFGA_ERR);

	/* get a libdevinfo snapshot of the resource's subtree */
	rsrc_devpath = rsrc_fixed;
	if (strstr(rsrc_fixed, DEVICES) != NULL)
		rsrc_devpath += strlen(DEVICES);
	node = di_init(rsrc_devpath, DINFOSUBTREE | DINFOMINOR);
	if (node == DI_NODE_NIL) {
		cfga_err(errstring, 0, ERRARG_DEVINFO, rsrc_fixed, 0);
		ret = SCFGA_ERR;
	}

	/* apply the filter, and suspend all resources not filtered out */
	if (ret == SCFGA_OK) {

		walkargs.bus_path = rsrc_fixed;
		walkargs.filter = filter_fixed;
		walkargs.errstring = errstring;
		walkargs.ret = SCFGA_OK;
		walkargs.flags = rflags;
		walkargs.func = scsi_rcm_suspend;

		if (di_walk_node(node, 0, &walkargs, scsi_rcm_process_node) < 0)
			cfga_err(errstring, 0, ERRARG_DEVINFO, rsrc_fixed, 0);

		ret = walkargs.ret;
	}

	if (node != DI_NODE_NIL)
		di_fini(node);

	S_FREE(rsrc_fixed);
	S_FREE(filter_fixed);

	if (ret != SCFGA_OK)
		(void) scsi_rcm_resume(rsrc, filter, errstring,
		    (flags & (~CFGA_FLAG_FORCE)));

	return (ret);
}

/*
 * scsi_rcm_resume()
 *
 *	Resume SCSI resource consumers after a bus has been unquiesced.
 */
scfga_ret_t
scsi_rcm_resume(char *rsrc, char *filter, char **errstring, cfga_flags_t flags)
{
	uint_t rflags = 0;
	char *rsrc_fixed;
	char *filter_fixed;
	char *rsrc_devpath;
	rcm_info_t *rinfo = NULL;
	di_node_t node;
	scfga_ret_t ret = SCFGA_OK;
	walkargs_t walkargs;

	if ((ret = scsi_rcm_init(rsrc, flags, errstring, &rflags, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	/* If a filter is provided, ensure that it makes sense */
	if (filter != NULL && strstr(filter, rsrc) != filter) {
		S_FREE(rsrc_fixed);
		cfga_err(errstring, 0, ERR_APID_INVAL, 0);
		return (SCFGA_ERR);
	}

	/*
	 * If no filter is specified: resume the resource directly.
	 */
	if (filter == NULL) {
		if (rcm_notify_resume(rcm_handle, rsrc_fixed, rflags, &rinfo)
		    != RCM_SUCCESS && rinfo != NULL) {
			cfga_err(errstring, 0, ERRARG_RCM_RESUME, rsrc_fixed,
			    0);
			(void) scsi_rcm_info_table(rinfo, errstring);
			rcm_free_info(rinfo);
			ret = SCFGA_BUSY;
		}
		S_FREE(rsrc_fixed);
		return (ret);
	}

	/*
	 * If a filter is specified: open the resource with libdevinfo, walk
	 * through its nodes, and resume each of its nodes that mismatches
	 * the filter.
	 */

	/* Chop off the filter's minor name */
	if ((filter_fixed = chop_minor(filter)) == NULL)
		return (SCFGA_ERR);

	/* get a libdevinfo snapshot of the resource's subtree */
	rsrc_devpath = rsrc_fixed;
	if (strstr(rsrc_fixed, DEVICES) != NULL)
		rsrc_devpath += strlen(DEVICES);
	node = di_init(rsrc_devpath, DINFOSUBTREE | DINFOMINOR);
	if (node == DI_NODE_NIL) {
		cfga_err(errstring, 0, ERRARG_DEVINFO, rsrc_fixed, 0);
		ret = SCFGA_ERR;
	}

	/* apply the filter, and resume all resources not filtered out */
	if (ret == SCFGA_OK) {

		walkargs.bus_path = rsrc_fixed;
		walkargs.filter = filter_fixed;
		walkargs.errstring = errstring;
		walkargs.ret = SCFGA_OK;
		walkargs.flags = rflags;
		walkargs.func = scsi_rcm_resume;

		if (di_walk_node(node, 0, &walkargs, scsi_rcm_process_node) < 0)
			cfga_err(errstring, 0, ERRARG_DEVINFO, rsrc_fixed, 0);

		ret = walkargs.ret;
	}

	if (node != DI_NODE_NIL)
		di_fini(node);

	S_FREE(rsrc_fixed);
	S_FREE(filter_fixed);

	return (ret);
}

/*
 * scsi_rcm_info
 *
 *	Queries RCM information for resources, and formats it into a table.
 * The table is appended to the info argument.  If the info argument is a
 * null pointer, then a new string is malloc'ed.  If the info argument is
 * not a null pointer, then it is realloc'ed to the required size.
 */
scfga_ret_t
scsi_rcm_info(char *rsrc, char **errstring, char **info)
{
	char *rsrc_fixed;
	rcm_info_t *rinfo = NULL;
	scfga_ret_t ret = SCFGA_OK;

	if ((ret = scsi_rcm_init(rsrc, 0, errstring, NULL, &rsrc_fixed))
	    != SCFGA_OK)
		return (ret);

	if (info == NULL) {
		S_FREE(rsrc_fixed);
		return (SCFGA_ERR);
	}

	if (rcm_get_info(rcm_handle, rsrc_fixed, 0, &rinfo)
	    != RCM_SUCCESS) {
		cfga_err(errstring, 0, ERRARG_RCM_INFO, rsrc_fixed, 0);
		ret = SCFGA_ERR;
	} else if (rinfo == NULL)
		ret = SCFGA_OK;

	if (rinfo) {
		if ((ret = scsi_rcm_info_table(rinfo, info)) != SCFGA_OK)
			cfga_err(errstring, 0, ERRARG_RCM_INFO, rsrc_fixed, 0);
		rcm_free_info(rinfo);
	}

	S_FREE(rsrc_fixed);

	return (ret);
}

/*
 * scsi_rcm_init()
 *
 *	Contains common initialization code for entering a scsi_rcm_xx()
 * routine.
 */
static scfga_ret_t
scsi_rcm_init(char *rsrc, cfga_flags_t flags, char **errstring, uint_t *rflags,
	char **rsrc_fixed)
{
	/* Validate the rsrc argument */
	if (rsrc == NULL) {
		cfga_err(errstring, 0, ERR_APID_INVAL, 0);
		return (SCFGA_ERR);
	}

	/* Translate the cfgadm flags to RCM flags */
	if (rflags && (flags & CFGA_FLAG_FORCE))
		*rflags |= RCM_FORCE;

	/* Get a handle for the RCM operations */
	(void) mutex_lock(&rcm_handle_lock);
	if (rcm_handle == NULL) {
		if (rcm_alloc_handle(NULL, RCM_NOPID, NULL, &rcm_handle) !=
		    RCM_SUCCESS) {
			cfga_err(errstring, 0, ERR_RCM_HANDLE, 0);
			(void) mutex_unlock(&rcm_handle_lock);
			return (SCFGA_LIB_ERR);
		}
	}
	(void) mutex_unlock(&rcm_handle_lock);

	/* Chop off the rsrc's minor, if it has one */
	if ((*rsrc_fixed = chop_minor(rsrc)) == NULL)
		return (SCFGA_ERR);

	return (SCFGA_OK);
}

/*
 * scsi_rcm_process_node
 *
 *	Helper routine for scsi_rcm_{suspend,resume}.  This is a di_walk_node()
 * callback that will apply a filter to every node it sees, and either suspend
 * or resume it if it doesn't match the filter.
 */
static int
scsi_rcm_process_node(di_node_t node, void *argp)
{
	char *devfs_path;
	walkargs_t *walkargs;
	scfga_ret_t ret = SCFGA_OK;
	char disk_path[MAXPATHLEN];

	/* Guard against bad arguments */
	if ((walkargs = (walkargs_t *)argp) == NULL)
		return (DI_WALK_TERMINATE);
	if (walkargs->filter == NULL || walkargs->errstring == NULL) {
		walkargs->ret = SCFGA_ERR;
		return (DI_WALK_TERMINATE);
	}

	/* If the node has no minors, then skip it */
	if (di_minor_next(node, DI_MINOR_NIL) == DI_MINOR_NIL)
		return (DI_WALK_CONTINUE);

	/* Construct the devices path */
	if ((devfs_path = di_devfs_path(node)) == NULL)
		return (DI_WALK_CONTINUE);
	(void) snprintf(disk_path, MAXPATHLEN, "%s%s", DEVICES, devfs_path);
	di_devfs_path_free(devfs_path);

	/*
	 * If the node does not correspond to the targeted SCSI bus or the
	 * disk being filtered out, then use the appropriate suspend/resume
	 * function.
	 */
	if (strcmp(disk_path, walkargs->bus_path) != 0 &&
	    strcmp(disk_path, walkargs->filter) != 0)
		ret = (*walkargs->func)(disk_path, NULL, walkargs->errstring,
		    walkargs->flags);

	/* Stop the walk early if the above operation failed */
	if (ret != SCFGA_OK) {
		walkargs->ret = ret;
		return (DI_WALK_TERMINATE);
	}

	return (DI_WALK_CONTINUE);
}

/*
 * scsi_rcm_info_table
 *
 *	Takes an opaque rcm_info_t pointer and a character pointer, and appends
 * the rcm_info_t data in the form of a table to the given character pointer.
 */
static scfga_ret_t
scsi_rcm_info_table(rcm_info_t *rinfo, char **table)
{
	int i;
	size_t w;
	size_t width = 0;
	size_t w_rsrc = 0;
	size_t w_info = 0;
	size_t table_size = 0;
	uint_t tuples = 0;
	rcm_info_tuple_t *tuple = NULL;
	char *rsrc;
	char *info;
	char *newtable;
	static char format[MAX_FORMAT];

	/* Protect against invalid arguments */
	if (rinfo == NULL || table == NULL)
		return (SCFGA_ERR);

	/* Set localized table header strings */
	rsrc = gettext("Resource");
	info = gettext("Information");

	/* A first pass, to size up the RCM information */
	while (tuple = rcm_info_next(rinfo, tuple)) {
		tuples++;
		if ((w = strlen(rcm_info_rsrc(tuple))) > w_rsrc)
			w_rsrc = w;
		if ((w = strlen(rcm_info_info(tuple))) > w_info)
			w_info = w;
	}

	/* If nothing was sized up above, stop early */
	if (tuples == 0)
		return (SCFGA_OK);

	/* Adjust column widths for column headings */
	if ((w = strlen(rsrc)) > w_rsrc)
		w_rsrc = w;
	else if ((w_rsrc - w) % 2)
		w_rsrc++;
	if ((w = strlen(info)) > w_info)
		w_info = w;
	else if ((w_info - w) % 2)
		w_info++;

	/*
	 * Compute the total line width of each line,
	 * accounting for intercolumn spacing.
	 */
	width = w_info + w_rsrc + 4;

	/* Allocate space for the table */
	table_size = (2 + tuples) * (width + 1) + 2;
	if (*table == NULL)
		*table = malloc(table_size);
	else {
		newtable = realloc(*table, strlen(*table) + table_size);
		if (newtable != NULL)
			*table = newtable;
	}
	if (*table == NULL)
		return (SCFGA_ERR);

	/* Place a table header into the string */

	/* The resource header */
	(void) strcat(*table, "\n");
	w = strlen(rsrc);
	for (i = 0; i < ((w_rsrc - w) / 2); i++)
		(void) strcat(*table, " ");
	(void) strcat(*table, rsrc);
	for (i = 0; i < ((w_rsrc - w) / 2); i++)
		(void) strcat(*table, " ");

	/* The information header */
	(void) strcat(*table, "  ");
	w = strlen(info);
	for (i = 0; i < ((w_info - w) / 2); i++)
		(void) strcat(*table, " ");
	(void) strcat(*table, info);
	for (i = 0; i < ((w_info - w) / 2); i++)
		(void) strcat(*table, " ");

	/* Underline the headers */
	(void) strcat(*table, "\n");
	for (i = 0; i < w_rsrc; i++)
		(void) strcat(*table, "-");
	(void) strcat(*table, "  ");
	for (i = 0; i < w_info; i++)
		(void) strcat(*table, "-");

	/* Construct the format string */
	(void) snprintf(format, MAX_FORMAT, "%%-%ds  %%-%ds", w_rsrc, w_info);

	/* Add the tuples to the table string */
	tuple = NULL;
	while ((tuple = rcm_info_next(rinfo, tuple)) != NULL) {
		(void) strcat(*table, "\n");
		(void) sprintf(&((*table)[strlen(*table)]),
		    format, rcm_info_rsrc(tuple),
		    rcm_info_info(tuple));
	}

	return (SCFGA_OK);
}

/*
 * chop_minor()
 *
 *	Chops off the minor name portion of a resource.  Allocates storage for
 * the returned string.  Caller must free the storage if return is non-NULL.
 */
static char *
chop_minor(char *rsrc)
{
	char *rsrc_fixed;
	char *cp;

	if ((rsrc_fixed = strdup(rsrc)) == NULL)
		return (NULL);
	if ((cp = strrchr(rsrc_fixed, ':')) != NULL)
		*cp = '\0';
	return (rsrc_fixed);
}
