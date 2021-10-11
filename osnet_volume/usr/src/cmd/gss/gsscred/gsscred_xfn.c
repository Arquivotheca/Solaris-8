/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)gsscred_xfn.c 1.9     98/12/17 SMI"

#include <string.h>
#include <synch.h>
#include <stdlib.h>
#include <xfn/xfn.h>
#include "gsscred_xfn.h"

/* Definitions and constants for GSSCRED */
#define	AUTHORITATIVE 0
#define	ATTR_ID_SYNTAX "fn_attr_syntax_ascii"
#define	GSSCRED_UID_ID "onc_gsscred_unix_uid"
#define	GSSCRED_COMMENT_ID "onc_gsscred_comment"
#define	GSSCRED_CONTEXT "thisorgunit/service/gsscred"
#define	GSSCRED_PRINCIPAL_ADDRESS_ID "onc_gsscred_principal_address"
#define	GSSCRED_PRINCIPAL_REFERENCE_ID "onc_gsscred_principal_reference"

/* GSSCRED contexts. Must be MT-SAFE */
static mutex_t gsscred_context_lock = DEFAULTMUTEX;
static FN_ctx_t *gsscred_context = 0;

typedef struct GssCredContext_t {
	char *name;
	FN_ctx_t *context;
	struct GssCredContext_t *next;
} GssCredContext;

static FN_composite_name_t *empty_name = 0;

/* Structure to hold namelists */
typedef struct GssCredNameList_t {
	int current_pointer;
	FN_status_t *status;
	FN_searchlist_t **namelists;
} GssCredNameList;


/*
 * private xfn interface to allow selection of backend
 * mechanisms.
 */
extern FN_ctx_t *
_fn_ctx_handle_from_initial_with_ns(int, int, FN_status_t *);


/*
 * local function declarations
 */
static void
destroy_all_gsscred_context(GssCredContext *entry);

static void
destroy_all_gsscred_entries(GssCredEntry *entry);

static void
destroy_gsscred_context(GssCredContext *entry);

static void
destroy_gsscred_entry(GssCredEntry *entry);

static int
fns_deleteAllGssCredContext(int xfnBackEnd, FN_status_t *status);

static int
fns_deleteByUidForAllMechs(int xfnBackEnd, const char *uidStr,
		FN_status_t *status);

static int
fns_deleteGssCredEntry(FN_ctx_t *mech_context,
		const char *principal_name, FN_status_t *status);

static int
fns_deleteGssCredMechanism(int xfnBackEnd, const char *mechanism,
		FN_status_t *status);

static FN_attribute_t *
fns_get_attribute(const char *id, const char *value);

static char *
fns_get_attrvalue(const FN_attrset_t *attrset, const FN_identifier_t *id);

static GssCredEntry *
fns_getGssCredEntryByPrincipalName(int xfnBackEnd, const char *mechanism,
		const char *principal, FN_status_t *status);

static GssCredEntry *
fns_getGssCredEntryByUnixUID(int xfnBackEnd, const char *mechanism,
		const char *unix_uid, FN_status_t *status);

static int
fns_getGssCredEntryFromAttrsetInData(const char *principal,
		const FN_attrset_t *attrset, GssCredEntry *answer);

static FN_ctx_t	*
fns_get_gsscred_mechanism_context(int xfnBackEnd, const char *mechanism,
		int create, FN_status_t *status);

static GssCredContext *
fns_get_gsscred_mechanism_contexts(int xfnBackEnd, FN_status_t *status);

static FN_ref_t *
fns_get_principal_name_binding(const char *principal);

static int
fns_setup_gsscred_context(int xfnBackEnd, FN_status_t *status);

static void
getErrStr(FN_status_t *status, char **errStr);



static void
getErrStr(FN_status_t *status, char **errStr)
{
	FN_string_t *xfnStr;
	unsigned int anInt = 0;

	/* check if error string is required */
	if (errStr == NULL)
	    return;

	xfnStr = fn_status_description(status, 0, &anInt);
	if (xfnStr)
		*errStr = strdup((const char *)fn_string_str(xfnStr, &anInt));
}


static int
fns_setup_gsscred_context(int xfnBackEnd, FN_status_t *status)
{
	FN_ctx_t *initial_context;
	FN_composite_name_t *name;
	FN_ref_t *reference;

	if (gsscred_context)
		return (1);

	/* Obtain the initial context - user specified backend */
	if (xfnBackEnd == 0)
		initial_context = fn_ctx_handle_from_initial(
			AUTHORITATIVE, status);
	else
		initial_context = _fn_ctx_handle_from_initial_with_ns(
			xfnBackEnd, AUTHORITATIVE, status);

	if (fn_status_code(status) != FN_SUCCESS)
		return (0);

	name = fn_composite_name_from_str((unsigned char *) GSSCRED_CONTEXT);

	/* Obtain the GSSCRED context's reference */
	mutex_lock(&gsscred_context_lock);
	if (gsscred_context) {
		mutex_unlock(&gsscred_context_lock);
		fn_composite_name_destroy(name);
		fn_ctx_handle_destroy(initial_context);
		return (1);
	}
	reference = fn_ctx_lookup(initial_context, name, status);
	if ((reference == 0) || (fn_status_code(status) != FN_SUCCESS))
		reference = fn_ctx_create_subcontext(initial_context,
		    name, status);
	fn_composite_name_destroy(name);
	fn_ctx_handle_destroy(initial_context);
	if (fn_status_code(status) != FN_SUCCESS) {
		mutex_unlock(&gsscred_context_lock);
		return (0);
	}

	/* Get the GSSCRED's context */
	gsscred_context =
	    fn_ctx_handle_from_ref(reference, AUTHORITATIVE, status);
	fn_ref_destroy(reference);

	/* Setup the empty name, if it is empty */
	if (empty_name == 0)
		empty_name = fn_composite_name_from_str((unsigned char *) "");

	mutex_unlock(&gsscred_context_lock);
	return (1);
}

static FN_ctx_t *
fns_get_gsscred_mechanism_context(int xfnBackEnd, const char *mechanism,
	int create, FN_status_t *status)
{
	FN_ctx_t *answer;
	FN_composite_name_t *name;
	FN_ref_t *reference;

	if (fns_setup_gsscred_context(xfnBackEnd, status) == 0)
		return (0);

	name = fn_composite_name_from_str((unsigned char *) mechanism);
	reference = fn_ctx_lookup(gsscred_context, name, status);
	if ((create) &&
	    ((reference == 0) || (fn_status_code(status) != FN_SUCCESS)))
		reference = fn_ctx_create_subcontext(
		    gsscred_context, name, status);
	fn_composite_name_destroy(name);
	if (fn_status_code(status) != FN_SUCCESS)
		return (0);

	/* Get the GSSCRED's context */
	answer = fn_ctx_handle_from_ref(reference,
	    AUTHORITATIVE, status);
	fn_ref_destroy(reference);
	return (answer);
}

static GssCredContext *
fns_get_gsscred_mechanism_contexts(int xfnBackEnd, FN_status_t *status)
{
	FN_bindinglist_t *bl;
	FN_ref_t *reference;
	FN_string_t *string;
	GssCredContext *cache = 0, *new, *current;

	/* Check if gsscred context is setup */
	if (fns_setup_gsscred_context(xfnBackEnd, status) == 0)
		return (0);

	/* Get the binding list and construct the contexts */
	cache = 0;
	bl = fn_ctx_list_bindings(gsscred_context, empty_name, status);
	while ((bl) && ((string = fn_bindinglist_next(bl, &reference, status))
	    != 0)) {
		new = (GssCredContext *) malloc(sizeof (GssCredContext));
		new->name = strdup((char *) fn_string_str(string, 0));
		new->context = fn_ctx_handle_from_ref(reference,
		    AUTHORITATIVE, status);
		new->next = 0;
		if (cache == 0) {
			cache = new;
			current = new;
		} else {
			current->next = new;
			current = new;
		}
		fn_string_destroy(string);
		fn_ref_destroy(reference);
	}
	fn_bindinglist_destroy(bl);
	return (cache);
}

static FN_ref_t *
fns_get_principal_name_binding(const char *principal)
{
	FN_ref_t *reference;
	FN_ref_addr_t *ref_addr;
	FN_identifier_t identifier;

	/* Construct the address identifier */
	identifier.format = FN_ID_STRING;
	identifier.contents = GSSCRED_PRINCIPAL_ADDRESS_ID;
	identifier.length = strlen(GSSCRED_PRINCIPAL_ADDRESS_ID);

	/* Construct the address */
	ref_addr = fn_ref_addr_create(&identifier, strlen(principal),
	    principal);

	/* Construct the reference identifier */
	identifier.contents = GSSCRED_PRINCIPAL_REFERENCE_ID;
	identifier.length = strlen(GSSCRED_PRINCIPAL_REFERENCE_ID);
	reference = fn_ref_create(&identifier);
	fn_ref_append_addr(reference, ref_addr);
	fn_ref_addr_destroy(ref_addr);
	return (reference);
}

static FN_attribute_t *
fns_get_attribute(const char *id, const char *value)
{
	FN_attribute_t *answer;
	FN_identifier_t syntax, ident;
	FN_attrvalue_t attr_value;

	/* Construct syntax identifier */
	syntax.format = FN_ID_STRING;
	syntax.length = strlen(ATTR_ID_SYNTAX);
	syntax.contents = ATTR_ID_SYNTAX;

	/* Construct attribute identifier */
	ident.format = FN_ID_STRING;
	ident.length = strlen(id);
	ident.contents = (void*)id;

	/* Construct attribute value */
	attr_value.length = strlen(value);
	attr_value.contents = (void *) value;

	/* Construct the attribute */
	answer = fn_attribute_create(&ident, &syntax);

	/* Add attribute value */
	if (fn_attribute_add(answer, &attr_value, 0) == 0) {
		fn_attribute_destroy(answer);
		return (0);
	}

	return (answer);
}

int
xfn_addGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *principal, const char *uidString,
	const char *comment, char **errStr)
{
	FN_ctx_t *mechanism_context;
	FN_ref_t *reference;
	FN_composite_name_t *name;
	FN_status_t *status = fn_status_create();
	FN_attribute_t *attribute;
	int ret_code;

	if ((mechanism_context =
	    fns_get_gsscred_mechanism_context(xfnBackEnd, mechanism, 1,
	    status)) == 0) {
		getErrStr(status, errStr);
		fn_status_destroy(status);
		return (0);
	}

	reference = fns_get_principal_name_binding(principal);

	if (reference == 0) {
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}
	name = fn_composite_name_from_str((unsigned char *) principal);

	ret_code = fn_ctx_bind(mechanism_context, name, reference, 1, status);
	fn_ref_destroy(reference);

	if (ret_code == 0) {
		getErrStr(status, errStr);
		fn_status_destroy(status);
		fn_composite_name_destroy(name);
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}

	/* Add the attributes */
	attribute = fns_get_attribute(GSSCRED_UID_ID, uidString);

	if (attribute == 0) {
		getErrStr(status, errStr);
		fn_ctx_unbind(mechanism_context, name, status);
		fn_status_destroy(status);
		fn_composite_name_destroy(name);
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}
	ret_code = fn_attr_modify(mechanism_context, name, FN_ATTR_OP_ADD,
	    attribute, 1, status);
	fn_attribute_destroy(attribute);

	if (ret_code == 0) {
		getErrStr(status, errStr);
		fn_ctx_unbind(mechanism_context, name, status);
		fn_status_destroy(status);
		fn_composite_name_destroy(name);
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}

	attribute = fns_get_attribute(GSSCRED_COMMENT_ID, comment);
	if (attribute == 0) {
		getErrStr(status, errStr);
		fn_ctx_unbind(mechanism_context, name, status);
		fn_status_destroy(status);
		fn_composite_name_destroy(name);
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}
	ret_code = fn_attr_modify(mechanism_context, name, FN_ATTR_OP_ADD,
	    attribute, 1, status);

	if (fn_status_code(status) != FN_SUCCESS) {
		getErrStr(status, errStr);
		fn_ctx_unbind(mechanism_context, name, status);
	}

	fn_attribute_destroy(attribute);
	fn_status_destroy(status);
	fn_composite_name_destroy(name);
	fn_ctx_handle_destroy(mechanism_context);
	return (ret_code);
}

static char *
fns_get_attrvalue(const FN_attrset_t *attrset,
    const FN_identifier_t *id)
{
	char *answer;
	const FN_attribute_t *attribute;
	const FN_attrvalue_t *attrvalue;
	void *ip;

	attribute = fn_attrset_get(attrset, id);
	if (attribute == 0)
		return (0);

	attrvalue = fn_attribute_first(attribute, &ip);
	if (attrvalue == 0)
		return (0);

	answer = (char *) calloc(1, (attrvalue->length + 1));
	memcpy(answer, attrvalue->contents, attrvalue->length);
	return (answer);
}

static void
destroy_gsscred_entry(GssCredEntry *entry)
{
	free(entry->principal_name);
	free(entry->comment);
	free(entry);
}

static void
destroy_all_gsscred_entries(GssCredEntry *entry)
{
	GssCredEntry *next, *current = entry;

	while (current) {
		next = current->next;
		destroy_gsscred_entry(current);
		current = next;
	}
}

static void
destroy_gsscred_context(GssCredContext *entry)
{
	free(entry->name);
	fn_ctx_handle_destroy(entry->context);
	free(entry);
}

static void
destroy_all_gsscred_context(GssCredContext *entry)
{
	GssCredContext *next, *current = entry;

	while (current) {
		next = current->next;
		destroy_gsscred_context(current);
		current = next;
	}
}

static GssCredEntry *
fns_getGssCredEntryByPrincipalName(int xfnBackEnd,
	const char *mechanism, const char *principal,
	FN_status_t *status)
{
	GssCredEntry *answer;
	FN_attrset_t *attrset;
	FN_composite_name_t *name;
	FN_ctx_t *mechanism_context;

	if ((mechanism_context =
			fns_get_gsscred_mechanism_context(xfnBackEnd,
			mechanism, 0, status)) == 0)
		return (0);

	name = fn_composite_name_from_str((unsigned char *) principal);
	attrset = fn_attr_get_ids(mechanism_context, name, 1, status);
	fn_composite_name_destroy(name);
	if (fn_status_code(status) != FN_SUCCESS) {
		fn_attrset_destroy(attrset);
		fn_ctx_handle_destroy(mechanism_context);
		return (0);
	}

	answer = malloc(sizeof (GssCredEntry));
	fns_getGssCredEntryFromAttrsetInData(principal, attrset, answer);
	fn_attrset_destroy(attrset);
	fn_ctx_handle_destroy(mechanism_context);

	return (answer);
}


static GssCredEntry *
fns_getGssCredEntryByUnixUID(int xfnBackEnd, const char *mechanism,
	const char *uidStr, FN_status_t *status)
{
	GssCredEntry *answer = 0, *next, *new;
	FN_ctx_t *context;
	GssCredContext *current, *gsscred_mechanism_context;
	FN_searchlist_t *searchlist;
	FN_attrset_t *attrset, *return_attrset;
	FN_attribute_t *attribute;
	FN_string_t *string;

	if (mechanism) {
		if ((context = fns_get_gsscred_mechanism_context(
			xfnBackEnd, mechanism, 0, status)) == 0)
			return (0);

		gsscred_mechanism_context = malloc(sizeof (GssCredContext));
		gsscred_mechanism_context->name = strdup(mechanism);
		gsscred_mechanism_context->next = 0;
		gsscred_mechanism_context->context = context;
	} else {
		if ((gsscred_mechanism_context =
			fns_get_gsscred_mechanism_contexts(
			xfnBackEnd, status))  == 0)
			return (0);
	}

	/* Set up requisite for search operations */
	attrset = fn_attrset_create();
	attribute = fns_get_attribute(GSSCRED_UID_ID, uidStr);
	fn_attrset_add(attrset, attribute, 1);
	fn_attribute_destroy(attribute);

	/* Obtain the read lock and search for UID */
	current = gsscred_mechanism_context;
	while (current) {
		searchlist = prelim_fn_attr_search(current->context,
			empty_name, attrset, 0, 0, status);
		while ((searchlist) && (string =
		    prelim_fn_searchlist_next(searchlist,
		    0, &return_attrset, status))) {

			new = malloc(sizeof (GssCredEntry));
			fns_getGssCredEntryFromAttrsetInData(
			    (char *) fn_string_str(string, 0),
			    return_attrset, new);
			if (answer == 0) {
				answer = new;
				next = new;
			} else {
				next->next = new;
				next = new;
			}
			fn_string_destroy(string);
			fn_attrset_destroy(return_attrset);

		}
		if (searchlist)
			prelim_fn_searchlist_destroy(searchlist);
		current = current->next;
	}
	destroy_all_gsscred_context(gsscred_mechanism_context);
	fn_attrset_destroy(attrset);
	return (answer);
}

GssCredEntry *
xfn_getGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *principal_name, const char *unix_uid, char **errStr)
{
	GssCredEntry *from_princ = 0;
	GssCredEntry *from_uid = 0;
	GssCredEntry *current;
	FN_status_t *status = fn_status_create();

	if (mechanism && principal_name)
		from_princ = fns_getGssCredEntryByPrincipalName(
				xfnBackEnd, mechanism, principal_name, status);
	if (unix_uid)
		from_uid = fns_getGssCredEntryByUnixUID(xfnBackEnd,
				mechanism, unix_uid, status);

	if (fn_status_code(status) != FN_SUCCESS &&
	    fn_status_code(status) != FN_E_NAME_NOT_FOUND) {
		getErrStr(status, errStr);
		fn_status_destroy(status);
		return (0);
	}
	fn_status_destroy(status);

	if (from_princ == 0 && principal_name == NULL)
		return (from_uid);
	else if (from_uid == 0 && !unix_uid)
		return (from_princ);
	else if ((from_uid == 0 && unix_uid) ||
	    (from_princ == 0 && principal_name != NULL)) {

		if (from_princ)
			destroy_all_gsscred_entries(from_princ);

		if (from_uid)
			destroy_all_gsscred_entries(from_uid);

		return (0);
	}

	/*
	 * Find the union of the two results
	 * from_princ will have at most 1 entry
	 */
	for (current = from_uid; current; current = current->next) {
		if ((strcmp(current->principal_name,
		    from_princ->principal_name) == 0) &&
		    (current->unix_uid == from_princ->unix_uid)) {
			destroy_all_gsscred_entries(from_uid);
			return (from_princ);
		}
	}

	return (0);
}

static int
fns_deleteGssCredEntry(FN_ctx_t *mech_context,
    const char *principal_name, FN_status_t *status)
{
	FN_composite_name_t *name;
	int ret;

	name = fn_composite_name_from_str((unsigned char *) principal_name);
	ret = fn_ctx_unbind(mech_context, name, status);
	fn_composite_name_destroy(name);
	return (ret);
}

static int
fns_deleteGssCredMechanism(int xfnBackEnd, const char *mechanism,
	FN_status_t *status)
{
	FN_composite_name_t *name;
	int ret;

	if (!fns_setup_gsscred_context(xfnBackEnd, status))
		return (0);

	name = fn_composite_name_from_str((unsigned char *) mechanism);
	ret = fn_ctx_destroy_subcontext(gsscred_context, name, status);
	fn_composite_name_destroy(name);
	return (ret);
}

static int
fns_deleteAllGssCredContext(int xfnBackEnd, FN_status_t *status)
{
	GssCredContext *gssContexts, *current;
	int ret = 1;

	gssContexts = fns_get_gsscred_mechanism_contexts(xfnBackEnd, status);
	if (fn_status_code(status) != FN_SUCCESS)
		return (0);

	current = gssContexts;
	while (current) {
		ret = fns_deleteGssCredMechanism(xfnBackEnd,
				current->name, status);
		if (!ret)
			break;
		current = current->next;
	}
	destroy_all_gsscred_context(gssContexts);
	return (ret);
}


static int
fns_deleteByUidForAllMechs(int xfnBackEnd, const char *uidStr,
	FN_status_t *status)
{
	/* deletes by specified uid across all mechs */
	GssCredContext *ctxts, *current;
	FN_attrset_t *attrset;
	FN_attribute_t *attribute;
	int foundOne = 0;
	FN_searchlist_t *searchList;
	FN_string_t *string;

	if ((ctxts = fns_get_gsscred_mechanism_contexts(
			xfnBackEnd, status)) == 0) {
		if (fn_status_code(status) == FN_SUCCESS)
			fn_status_set_code(status, FN_E_NAME_NOT_FOUND);

		return (0);
	}

	/* create the attribute the search by */
	attrset = fn_attrset_create();
	attribute = fns_get_attribute(GSSCRED_UID_ID, uidStr);
	fn_attrset_add(attrset, attribute, 1);
	fn_attribute_destroy(attribute);

	/* now search across all contexts */
	current = ctxts;
	while (current) {
		searchList = prelim_fn_attr_search(current->context,
				empty_name, attrset, 0, 0, status);
		while (searchList && (string =
			prelim_fn_searchlist_next(searchList, 0, 0, status))) {
			foundOne = 1;
			fns_deleteGssCredEntry(current->context,
				(char *)fn_string_str(string, 0), status);
			fn_string_destroy(string);
		}

		if (searchList)
			prelim_fn_searchlist_destroy(searchList);

		current = ctxts->next;
	}

	destroy_all_gsscred_context(ctxts);
	if (fn_status_code(status) == FN_SUCCESS && !foundOne) {
		fn_status_set_code(status, FN_E_NAME_NOT_FOUND);
		return (0);
	} else if (fn_status_code(status) == FN_SUCCESS)
		return (1);

	return (0);
} /* fns_deleteByUidForAllMechs */


int
xfn_deleteGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *unixUid, const char *princName, char **errStr)
{
	GssCredEntry *entries, *current;
	FN_ctx_t *mech_context;
	char *interErrStr = NULL;
	FN_status_t *status = fn_status_create();
	int retCode = 0;

	/*
	 * Assumption:
	 *
	 * we assume that when the mech is NULL, name is not specified
	 * which should always be true, since if we have the name we
	 * also have the mech (i.e. name contains mech in it)
	 */


	/* are we deleting the entire gsscred table */
	if (!mechanism && !unixUid && !princName)
		retCode = fns_deleteAllGssCredContext(xfnBackEnd, status);

	/* deleting entire mechanism */
	else if (mechanism && !unixUid && !princName)
		retCode = fns_deleteGssCredMechanism(xfnBackEnd,
				mechanism, status);

	/* deleting by principal name (and mechanism - see assumption ) */
	else if (mechanism && princName && !unixUid) {
		mech_context = fns_get_gsscred_mechanism_context(xfnBackEnd,
					mechanism, 0, status);
		if (mech_context) {
			retCode = fns_deleteGssCredEntry(mech_context,
					princName, status);
			fn_ctx_handle_destroy(mech_context);
			if (!retCode)
				getErrStr(status, errStr);
		} else
			getErrStr(status, errStr);

		fn_status_destroy(status);
		return (retCode);
	}

	/* deleting by uid for all mechs and all names */
	else if (!mechanism && !princName && unixUid)
		retCode = fns_deleteByUidForAllMechs(xfnBackEnd,
					unixUid, status);

	/* deleting for a single mech, with uid, and possibly name */
	else {
		entries = xfn_getGssCredEntry(xfnBackEnd, mechanism,
					princName, unixUid, &interErrStr);
		if (entries == NULL) {
			if (errStr != NULL) {
				/* was it an error or just not found ? */
				if (interErrStr != NULL) {
					*errStr = interErrStr;
					interErrStr = NULL;
				} else {
					fn_status_set_code(status,
						FN_E_NAME_NOT_FOUND);
					getErrStr(status, errStr);
				}
			}
			if (interErrStr != NULL)
				free(interErrStr);

			fn_status_destroy(status);
			return (0);
		}

		/* delete all matches found */
		current = entries;
		mech_context = fns_get_gsscred_mechanism_context(xfnBackEnd,
					mechanism, 0, status);
		while (current && mech_context) {
			retCode = fns_deleteGssCredEntry(mech_context,
					current->principal_name, status);
			current = current->next;
		}
		destroy_all_gsscred_entries(entries);
		if (mech_context)
			fn_ctx_handle_destroy(mech_context);
	}

	/* on error get error string */
	if (!retCode)
		getErrStr(status, errStr);

	fn_status_destroy(status);
	return (retCode);
}


static int
fns_getGssCredEntryFromAttrsetInData(const char *principal,
    const FN_attrset_t *attrset, GssCredEntry *answer)
{
	FN_identifier_t identifier;
	char *attrvalue;

	answer->principal_name = strdup(principal);
	answer->next = 0;

	/* Get the Unix_uid attribute */
	identifier.format = FN_ID_STRING;
	identifier.length = strlen(GSSCRED_UID_ID);
	identifier.contents = GSSCRED_UID_ID;
	attrvalue = fns_get_attrvalue(attrset, &identifier);
	if (attrvalue == NULL)
	    answer->unix_uid = -1;
	else {
	    answer->unix_uid = atoi(attrvalue);
	    free(attrvalue);
	}

	/* Get the Comment */
	identifier.length = strlen(GSSCRED_COMMENT_ID);
	identifier.contents = GSSCRED_COMMENT_ID;
	answer->comment = fns_get_attrvalue(attrset, &identifier);
	if (answer->comment == NULL)
		answer->comment = strdup("empty comment");
	return (1);
}

int
xfn_getNextGssCredEntry(void *searchHandle, GssCredEntry *entry)
{
	FN_searchlist_t *sl;
	FN_string_t *string;
	FN_attrset_t *attrset;
	GssCredNameList *cred_name_list = (GssCredNameList *) searchHandle;
	int ret;

	while ((sl = cred_name_list->
	    namelists[cred_name_list->current_pointer]) != NULL) {
		string = prelim_fn_searchlist_next(sl, 0, &attrset,
		    cred_name_list->status);
		if ((string == NULL) || (fn_status_code(cred_name_list->status)
		    != FN_SUCCESS)) {
			cred_name_list->current_pointer++;
			continue;
		}
		ret = fns_getGssCredEntryFromAttrsetInData((char *)
		    fn_string_str(string, 0), attrset, entry);
		fn_string_destroy(string);
		fn_attrset_destroy(attrset);
		return (ret);
	}
	return (0);
}

int
xfn_deleteGssCredSearchHandle(void *searchHandle)
{
	int i;
	GssCredNameList *cred_name_list = (GssCredNameList *) searchHandle;

	fn_status_destroy(cred_name_list->status);
	for (i = 0; cred_name_list->namelists[i]; i++)
		prelim_fn_searchlist_destroy(cred_name_list->namelists[i]);
	free(cred_name_list->namelists);
	free(cred_name_list);
	return (1);
}

int
xfn_getFirstGssCredEntry(int xfnBackEnd, const char *mechanism,
	void **searchHandle, GssCredEntry *entry, char **errStr)
{
	GssCredNameList *cred_name_list;
	FN_ctx_t *mech_context;
	FN_searchlist_t *sl;
	GssCredContext *cred_contexts, *current;
	int i, number_of_cred_contexts = 0;

	cred_name_list = (GssCredNameList *)
			calloc(1, sizeof (GssCredNameList));
	cred_name_list->status = fn_status_create();
	if (mechanism) {
		if ((mech_context = fns_get_gsscred_mechanism_context(
					xfnBackEnd, mechanism, 0,
					cred_name_list->status)) == 0) {

			getErrStr(cred_name_list->status, errStr);
			fn_status_destroy(cred_name_list->status);
			free(cred_name_list);
			return (0);
		}
		sl = prelim_fn_attr_search(mech_context, empty_name, 0, 0, 0,
		    cred_name_list->status);
		if ((sl == 0) || (fn_status_code(
		    cred_name_list->status) != FN_SUCCESS)) {
			if (fn_status_code(cred_name_list->status) !=
				FN_SUCCESS)
				getErrStr(cred_name_list->status, errStr);
			fn_status_destroy(cred_name_list->status);
			free(cred_name_list);
			fn_ctx_handle_destroy(mech_context);
			return (0);
		}
		cred_name_list->namelists = (FN_searchlist_t **)
		    calloc(2, sizeof (FN_searchlist_t *));
		cred_name_list->namelists[0] = sl;
		fn_ctx_handle_destroy(mech_context);
	} else {
		cred_contexts = fns_get_gsscred_mechanism_contexts(
					xfnBackEnd, cred_name_list->status);
		if (cred_contexts == NULL) {

			if (fn_status_code(cred_name_list->status)
			    != FN_SUCCESS)
				getErrStr(cred_name_list->status, errStr);

			fn_status_destroy(cred_name_list->status);
			free(cred_name_list);
			return (0);
		}

		for (current = cred_contexts; current;
		    current = current->next)
			number_of_cred_contexts++;
		cred_name_list->namelists = (FN_searchlist_t **)
		    calloc((number_of_cred_contexts + 1),
		    sizeof (FN_searchlist_t *));
		for (current = cred_contexts, i = 0; current;
		    current = current->next, i++) {
			cred_name_list->namelists[i] =
			    prelim_fn_attr_search(current->context, empty_name,
			    0, 0, 0, cred_name_list->status);
			if (fn_status_code(cred_name_list->status)
			    != FN_SUCCESS) {
				getErrStr(cred_name_list->status, errStr);
				destroy_all_gsscred_context(cred_contexts);
				fn_status_destroy(cred_name_list->status);
				free(cred_name_list->namelists);
				free(cred_name_list);
				return (0);
			}
		}
		destroy_all_gsscred_context(cred_contexts);
	}

	(*searchHandle) = cred_name_list;
	return (xfn_getNextGssCredEntry(*searchHandle, entry));
}
