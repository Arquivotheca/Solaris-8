/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd_impl.c	1.15	97/03/27 SMI"

/*
 * This module contains various routines private to the implementation of
 * the table library.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <synch.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/systeminfo.h>
#include <xfn/xfn.h>
#include <dd_impl.h>

#define	AUTHORITATIVE 1

/*
 * Function to allocate a row structure.
 */
Row *
_dd_new_row(void)
{
	return ((Row *)calloc(1, sizeof (Row)));
}

/*
 * Function to free a row structure.
 */
void
_dd_free_row(Row *rp)
{
	int i;

	for (i = 0; i < TBL_MAX_COLS; ++i)
		if (rp->ca[i] != NULL)
			(void) free(rp->ca[i]);

	(void) free(rp);
}

/*
 * Function to append a row to an array in an existing tbl structure.
 */
int
_dd_append_row(Tbl *tbl, Row *rp)
{
	int sz = tbl->rows;

	++sz;
	if ((tbl->ra = (Row **)realloc(tbl->ra, (sz * sizeof (Row *)))) ==
	    NULL) {
		return (-1);
	}
	tbl->ra[tbl->rows++] = rp;
	return (0);
}

/*
 * Function to set a column value within a row.  If the column already has a
 * value we append this as another token within the column.
 */
int
_dd_set_col_val(Row *rp, int cn, char *cv, char *as)
{
	if ((cv == NULL) || (cn >= TBL_MAX_COLS) || (cn < 0))
		return (0);
	if (rp->ca[cn] != NULL) {
		if ((rp->ca[cn] = (char *)realloc(rp->ca[cn],
		    (strlen(rp->ca[cn]) + strlen(cv) + 2))) == NULL)
			return (-1);
		(void) strcat(rp->ca[cn], as);
		(void) strcat(rp->ca[cn], cv);
	} else {
		if ((rp->ca[cn] = strdup(cv)) == NULL)
			return (-1);
	}
	return (0);
}

/*
 * Verify that path is not the default path. If it is the default path, do
 * nothing. Otherwise, prepend the path (with org_dir prepended) to the
 * existing search path. Returns 0 if successful, nonzero otherwise.
 * Also sets NIS_GROUP to "admin", if it isn't currently set in the environment.
 */
int
_set_nis_path(const char *pathp)
{
	register char *cp, *tp;
	char domain[DOM_NM_LN];
	char tmp[MAXPATHLEN];
	static char ndomain[MAXPATHLEN];
	static char ngroup[MAXPATHLEN];

	if (pathp == NULL || ndomain[0] != '\0')
		return (EINVAL);

	(void) sysinfo((int)SI_SRPC_DOMAIN, domain, (long)(DOM_NM_LN));

	if (strcmp(domain, pathp) != 0) {
		if ((cp = getenv(NIS_PATH)) != NULL) {
			(void) strcpy(tmp, cp);
			for (tp = (char *)strtok(tmp, ":");
			    tp != NULL; tp = (char *)strtok(NULL, ":")) {
				if (strcmp(tp, pathp) == 0)
					return (0);	/* already in path */
			}
			(void) sprintf(ndomain, "%s=org_dir.%s.:%s.:%s",
			    NIS_PATH, pathp, pathp, cp);
		} else {
			(void) sprintf(ndomain, "%s=org_dir.%s.:%s.:$",
			    NIS_PATH, pathp, pathp);
		}
		if (ndomain[0] != '\0') {
			if (putenv(ndomain) != 0)
				return (errno);
		}
	}
	if (getenv(NIS_GROUP_ENV_STR) == NULL) {
		(void) sprintf(ngroup, "%s=admin.%s.", NIS_GROUP_ENV_STR,
		    domain);
		(void) putenv(ngroup);
	}
	return (0);
}

/*
 * The following code is used to create or destroy hosts context in FNS,
 * if FNS is installed. No error messages are produced; just a best
 * attempt is made.
 */

/* Returns handle to initial context */
static FN_ctx_t *
fns_get_initial_context()
{
	static mutex_t initial_context_lock = DEFAULTMUTEX;
	static FN_ctx_t *initial_context = NULL;

	(void) mutex_lock(&initial_context_lock);
	if (initial_context == NULL) {
		FN_status_t	*status = fn_status_create();
		if (status == NULL) {
			(void) mutex_unlock(&initial_context_lock);
			return (NULL);
		}
		initial_context =
			fn_ctx_handle_from_initial(AUTHORITATIVE, status);
		(void) fn_status_destroy(status);
	}
	(void) mutex_unlock(&initial_context_lock);
	return (initial_context);
}

/*
 *  Creates context for host 'hostname' in domain 'domainname'
 *  Returns 0 on success and 1 on failure
 */
int
_dd_update_hosts_context(const char *domainname, const char *hostname)
{
	int			err = 1, len;
	char			*target = NULL;
	FN_ref_t		*ref = NULL;
	FN_composite_name_t	*hcompname = NULL;
	FN_status_t		*status = NULL;
	FN_ctx_t		*hostctx = NULL;
	FN_ctx_t		*initctx;

	/* Check input and setting of host context */
	if (hostname == NULL ||
	    (initctx = fns_get_initial_context()) == NULL)
		return (err);

	len = strlen(hostname) + (domainname ? strlen(domainname) : 0) + 20;
	target = (char *)malloc(len);
	if (target == NULL)
		return (err);

	/* Create hostname composite name */
	if (domainname != NULL)
		(void) sprintf(target, "org/%s/host", domainname);
	else
		(void) strcpy(target, "host");
	hcompname = fn_composite_name_from_str((u_char *) target);
	status = fn_status_create();
	if (hcompname == NULL || status == NULL)
		goto create_cleanup;

	/* Check if FNS is installed. Lookup for the hostname context */

	if ((ref = fn_ctx_lookup(initctx, hcompname, status)) == NULL)
		goto create_cleanup;   /* FNS namespace not set up */

	/* FNS namespace has been setup; continue and create host context */
	fn_composite_name_destroy(hcompname);

	/* Construct composite name for host */
	hcompname = fn_composite_name_from_str((u_char *) hostname);
	if (hcompname == NULL)
		goto create_cleanup;

	/* Get the hostname context */
	hostctx = fn_ctx_handle_from_ref(ref, AUTHORITATIVE, status);
	if (hostctx == NULL)
		goto create_cleanup;
	fn_ref_destroy(ref);

	/* If ctx does not exist, create it */
	if ((ref = fn_ctx_lookup(hostctx, hcompname, status)) != NULL ||
	    (ref = fn_ctx_create_subcontext(hostctx, hcompname,
	    status)) != NULL)
		err = 0;
create_cleanup:
	fn_ref_destroy(ref);
	fn_composite_name_destroy(hcompname);
	fn_ctx_handle_destroy(hostctx);
	fn_status_destroy(status);
	(void) free(target);
	return (err);
}

/*
 *  Destroys context for host 'hostname' in domain 'domainname'
 *  Returns 0 on success and 1 on failure.
 */
int
_dd_destroy_hosts_context(const char *domainname, const char *hostname)
{
	int			err = 1, len;
	FN_status_t 		*status = NULL;
	FN_composite_name_t 	*hcompname = NULL;
	char			*target, *prefix;
	FN_ctx_t		*initctx;

	if (hostname == NULL || (initctx = fns_get_initial_context()) == NULL)
		return (err);

	len = strlen(hostname) + (domainname ? strlen(domainname) : 0) + 20;
	prefix = (char *)malloc(len);
	target = (char *)malloc(len);
	if (prefix == NULL || target == NULL)
		return (err);

	if (domainname != NULL)
		(void) sprintf(prefix, "org/%s/host", domainname);
	else
		(void) strcpy(prefix, "host");

	(void) sprintf(target, "%s/%s/service", prefix, hostname);

	status = fn_status_create();
	hcompname = fn_composite_name_from_str((u_char *) target);
	if (status == NULL || hcompname == NULL)
		goto destroy_cleanup;

	/* Destroy the service context */
	if (fn_ctx_destroy_subcontext(initctx, hcompname, status) == 1) {
		fn_composite_name_destroy(hcompname);

		/* Destroy the host context */
		(void) sprintf(target, "%s/%s", prefix, hostname);
		hcompname = fn_composite_name_from_str((u_char *) target);
		if (hcompname != NULL &&
		    (fn_ctx_destroy_subcontext(initctx, hcompname,
		    status) == 1))
			err = 0;
	}

destroy_cleanup:
	fn_status_destroy(status);
	fn_composite_name_destroy(hcompname);
	(void) free(prefix);
	(void) free(target);
	return (err);
}
