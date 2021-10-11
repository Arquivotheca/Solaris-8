/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getprinter.c	1.2	99/05/06 SMI"

#pragma weak _nss_xfn__printers_constr = _nss_xfn_printers_constr

#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <nss_common.h>
#include <nss_dbdefs.h>
#include <xfn/xfn.h>
#include "xfn_common.h"

#define	PRINTERS	"onc_printers_"

/*
 * some values that make sense to keep or not continually
 * recreate/reallocate.
 */
static FN_composite_name_t	*empty_cname = NULL;

/*
 * Get the inital context to use for all future XFN lookups.  Because
 * we need to be able to look inside of multiple contexts inside various
 * parts of the tree.
 */
static FN_ctx_t *
_xfn_initial_context()
{
	static FN_ctx_t *ctx = NULL;
	FN_status_t *status;
	FN_string_t *string;

	if (ctx != NULL)
		return (ctx);

	if ((status = fn_status_create()) == NULL)
		return (NULL);

	seteuid(getuid());	/* set euid to get proper user's context */
	ctx = fn_ctx_handle_from_initial(0, status);
	setuid(0);		/* setuid back if we can */

	if ((string = fn_string_from_str((unsigned char *)"")) == NULL)
		return (NULL);

	empty_cname = fn_composite_name_from_string(string);
	fn_string_destroy(string);
	if (empty_cname == NULL)
		return (NULL);

	/* should check it first */
	fn_status_destroy(status);

	return (ctx);
}


static char *_default_prefix_list[] = {
	"thisuser/service/printer",
	"myorgunit/service/printer",
	"thisorgunit/service/printer",
	NULL
};

static char **
_xfn_prefix_list()
{
	return (_default_prefix_list);
}


static FN_ref_t *
_xfn_find_printer(const char *name, FN_status_t *status, FN_ctx_t *ctx)
{
	FN_bindinglist_t *bindings;
	FN_string_t *string;
	FN_composite_name_t *cname;
	FN_ref_t *ref, *link_ref, *result = NULL;

	if ((string = fn_string_from_str((const unsigned char *)name)) == NULL)
		return (NULL);

	cname = fn_composite_name_from_string(string);
	fn_string_destroy(string);
	if (cname == NULL)
		return (NULL);

	ref = fn_ctx_lookup(ctx, cname, status);
	fn_composite_name_destroy(cname);
	if (ref != NULL)
		return (ref);

	/* not found, walk the walk */
	if ((bindings = fn_ctx_list_bindings(ctx, empty_cname, status)) == NULL)
		return (NULL);

	while ((string = fn_bindinglist_next(bindings, &ref, status)) != NULL) {
		if (result == NULL) {
			FN_ctx_t *child;

			/*
			 * The reference could be a link. If so, do
			 * a fn_ctx_lookup() once again
			 */
			if (fn_ref_is_link(ref)) {
				cname = fn_composite_name_from_string(string);
				link_ref = fn_ctx_lookup(ctx, cname, status);
				fn_composite_name_destroy(cname);
				if (fn_status_code(status) != FN_SUCCESS)
					continue;
				fn_ref_destroy(ref);
				ref = link_ref;
			}

			if ((child = fn_ctx_handle_from_ref(ref, 0, status))
			    != NULL) {
				result = _xfn_find_printer(name, status, child);
				fn_ctx_handle_destroy(child);
			}
		}
		fn_string_destroy(string);
		fn_ref_destroy(ref);
	}
	fn_bindinglist_destroy(bindings);
	return (result);
}


static FN_ref_t *
_xfn_get_printer(const char *name)
{
	FN_ctx_t 	*ictx;
	FN_status_t 	*status;
	FN_ref_t	*result = NULL;
	char 		**prefix;

	if ((ictx = _xfn_initial_context()) == NULL)
		return (NULL);

	if ((status = fn_status_create()) == NULL)
		return (NULL);

	/* if it has a "/" in it, try the top first */
	if (strchr(name, '/') != NULL) {
		FN_string_t *string;
		FN_composite_name_t *cname;

		if ((string = fn_string_from_str((const unsigned char *)name))
		    == NULL)
			return (NULL);

		cname = fn_composite_name_from_string(string);
		fn_string_destroy(string);
		if (cname == NULL)
			return (NULL);

		result = fn_ctx_lookup(ictx, cname, status);
		fn_composite_name_destroy(cname);
	}


	for (prefix = _xfn_prefix_list();
	    result == NULL && prefix != NULL && *prefix != NULL; prefix++) {
		FN_string_t *string;
		FN_composite_name_t *cname;
		FN_ref_t *ref;
		FN_ctx_t *ctx;

		if ((string = fn_string_from_str(
				(const unsigned char *)*prefix)) == NULL)
			continue;

		cname = fn_composite_name_from_string(string);
		fn_string_destroy(string);
		if (cname == NULL)
			continue;

		ref = fn_ctx_lookup(ictx, cname, status);
		fn_composite_name_destroy(cname);
		if (ref == NULL)
			continue;

		ctx = fn_ctx_handle_from_ref(ref, 0, status);
		fn_ref_destroy(ref);

		result = _xfn_find_printer(name, status, ctx);
		fn_ctx_handle_destroy(ctx);
	}

	fn_status_destroy(status);
	return (result);
}


static nss_status_t
xfn_convert_printer(FN_ref_t *ref, char *name, nss_XbyY_args_t *args)
{
	nss_status_t		nss_stat = NSS_NOTFOUND;
	const FN_ref_addr_t		*ref_addr;
	void			*iter = NULL;
	char			*buf = args->buf.buffer;
	int			buflen = args->buf.buflen;

	buf[0] = NULL;
	strncat(buf, name, buflen);
	strncat(buf, ":", buflen);

	for (ref_addr = fn_ref_first(ref, &iter); ref_addr != NULL;
	    ref_addr = fn_ref_next(ref, &iter)) {
		const FN_identifier_t *id;
		char key[128];
		char *value = NULL;
		XDR xdr;

		if ((id = fn_ref_addr_type(ref_addr)) == NULL)
			continue;

		if (strncmp((char *)id->contents, PRINTERS,
				sizeof (PRINTERS) - 1) != 0)
			continue;	/* unknown address type */

		/* Get the printer address type */
		memset(key, '\0', sizeof (key));
		strncpy(key, ((char *)id->contents + (sizeof (PRINTERS) - 1)),
			id->length - (sizeof (PRINTERS) - 1));

		/* Get the printer data (value) */
		xdrmem_create(&xdr, (caddr_t)fn_ref_addr_data(ref_addr),
			fn_ref_addr_length(ref_addr), XDR_DECODE);

		xdr_string(&xdr, &value, ~0);

		strncat(buf, key, buflen);
		strncat(buf, "=", buflen);
		strncat(buf, value, buflen);
		strncat(buf, ":", buflen);
	}

	if (strlen(buf) != strlen(name) + 1) {
		args->returnval = args->buf.result;
		nss_stat = NSS_SUCCESS;
	}

	return (nss_stat);
}


static nss_status_t
getbyname(xfn_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *)a;
	nss_status_t		nss_stat = NSS_NOTFOUND;
	FN_ref_t		*ref;

	if ((ref = _xfn_get_printer(args->key.name)) != NULL) {
		nss_stat = xfn_convert_printer(ref, (char *)args->key.name,
						args);
		fn_ref_destroy(ref);
	}

	return (nss_stat);
}


static nss_status_t
getent(xfn_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *)a;
	nss_status_t		nss_stat = NSS_NOTFOUND;
	static FN_bindinglist_t	*bindings = NULL;
	char			*context_name;
	FN_ctx_t		*ctx;
	FN_status_t		*status;
	FN_composite_name_t	*comp_name;
	FN_string_t		*string;
	FN_ref_t		*ref;

	if ((status = fn_status_create()) == NULL)
		return (nss_stat);

	if (bindings == NULL) {
		if ((ctx = _xfn_initial_context()) == NULL)
			return (nss_stat);

		if ((string = fn_string_from_str(
		    (const unsigned char *)"thisorgunit/service/printer"))
		    == NULL)
			return (nss_stat);

		if ((comp_name = fn_composite_name_from_string(string))
		    == NULL)
			return (nss_stat);

		if ((bindings = fn_ctx_list_bindings(ctx, comp_name, status))
		    == NULL)
			return (nss_stat);
	}

	if ((string = fn_bindinglist_next(bindings, &ref, status)) != NULL) {
		unsigned int int_status;
		char *name;

		name = (char *)fn_string_str(string, &int_status);
		nss_stat = xfn_convert_printer(ref, name, args);
	}

	return (nss_stat);
}


static xfn_backend_op_t xfn_ops[] = {
	_nss_xfn_destr,
	NULL,	/* endent */
	NULL,	/* setent */
	getent,
	getbyname
};


nss_backend_t *
_nss_xfn_printers_constr(const char *dummy1, const char *dummy2,
    const char *dummy3)
{
	return (_nss_xfn_constr(xfn_ops,
			(sizeof (xfn_ops) / sizeof (xfn_ops[0]))));
}
