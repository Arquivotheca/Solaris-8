/*
 * Copyright (c) 1992 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_internal.cc	1.42	99/05/26 SMI"


#include <sys/time.h>
#include <sys/socket.h>
#include <rpcsvc/nis.h>
#include <string.h>
#include <stdlib.h>	// for strtol
#include <malloc.h>

#include <xfn/fn_xdr.hh>
#include "fnsp_internal.hh"
#include "FNSP_nisplusImpl.hh"
#include "fnsp_hostuser.hh"
#include "fnsp_attrs.hh"	// for FNSP_extract_attrset_result
#include "fnsp_utils.hh"	// for FNSP_process_user_fs
#include "FNSP_nisplus_address.hh"
#include <FNSP_Syntax.hh>

extern "C" {
nis_result * __nis_list_localcb(nis_name, u_long,
    int (*)(nis_name, nis_object *, void *), void *);

bool_t xdr_nis_server(XDR*, nis_server*);
};


/*
 *
 * Internal routines used by FNSP context implementations.
 *
 * Contains routines that make NIS+ library calls.
 */

enum FNSP_binding_type {
	FNSP_bound_reference = 0,
	FNSP_child_context = 1
	};

#define	AUTHORITATIVE 1

static char __nis_default_separator = FNSP_NIS_DEFAULT_SEP;
static const char *__nis_default_path = FNSP_NIS_DEFAULT_PATH;
static const char *__nis_default_table_type = FNSP_NIS_DEFAULT_TABTYPE;
static const char *FNSP_name_col_label = FNSP_NAME_COL_LABEL;
static const char *FNSP_ref_col_label = "reference";
static const char *FNSP_bind_col_label = "flags";
static const char *FNSP_attr_col_label = "attributes";
static const char *FNSP_ctx_col_label = FNSP_CONTEXTNAME;

static const char FNSP_internal_name_separator = '_';
static const char FNSP_default_char = '#';

static const FN_string FNSP_context_directory((unsigned char *) "ctx_dir.");
static int FNSP_cd_size = FNSP_context_directory.charcount();

static const FN_string FNSP_org_directory((unsigned char *) "org_dir.");
static int FNSP_od_size = FNSP_context_directory.charcount();

static const FN_string FNSP_org_attr((unsigned char *) "fns_attribute");

static const FN_string FNSP_self_name((unsigned char *) FNSP_SELF_NAME);
static const char *FNSP_nns_name = "_FNS_nns_";

static const FN_string FNSP_prefix((unsigned char *) "fns");
static const FN_identifier
FNSP_nisplus_address_type((unsigned char *) "onc_fn_nisplus");
static const FN_identifier
FNSP_printer_nisplus_address_type((unsigned char *) "onc_fn_printer_nisplus");

static FN_ref *
FNSP_lookup_org(const FN_string &org_name, unsigned int access_flags,
		unsigned &status);

// table layouts for ctx tables

#define	FNSP_NAME_COL 0
#define	FNSP_REF_COL 1
#define	FNSP_BIND_COL 2
#define	FNSP_ATTR_COL 3
#define	FNSP_CTX_COL 4

#define	FNSP_WIDE_TABLE_WIDTH 5
#define	FNSP_NARROW_TABLE_WIDTH 3

#define	FNSP_DEFAULT_TTL 43200

#define	ORG_DIRECTORY "org_dir"

#define	NOBODY_RIGHTS ((NIS_READ_ACC) << 24)
#define	WORLD_RIGHTS (NIS_READ_ACC)
#define	GROUP_RIGHTS ((NIS_READ_ACC |\
	NIS_MODIFY_ACC |\
	NIS_CREATE_ACC |\
	NIS_DESTROY_ACC) << 8)
#define	FNSP_DEFAULT_RIGHTS (NOBODY_RIGHTS | WORLD_RIGHTS | OWNER_DEFAULT | \
	GROUP_RIGHTS)

#define	ENTRY_FLAGS(obj, col) \
	(obj)->EN_data.en_cols.en_cols_val[col].ec_flags

static inline fnsp_meta_char(char c)
{
	return (c == FNSP_internal_name_separator || c == FNSP_default_char);
}

static inline nis_bad_value_char(char c)
{
	switch (c) {
	case '.':
	case '[':
	case ']':
	case ',':
	case '=':
	case '"': // not illegal but causes problems
		return (1);
	default:
		return (0);
	}
}

// Characters that are used by NIS+ to terminate a name
static inline nis_terminal_char(char c)
{
	switch (c) {
	case '.':
	case '[':
	case ']':
	case ',':
	case '=':
	case '"': // not illegal but causes problems
	case '/':  // not NIS+ reserved but NIS+ will reject it
		return (1);
	default:
		return (0);
	}
}

// Characters that cannot be leading characters in a NIS+ name
static inline nis_bad_lead_char(char c)
{
	switch (c) {
	case '@':
	case '+':
	case '-':
	case '"': // not illegal but causes problems
		return (1);
	default:
		return (0);
	}
}

// ************************** Dot name manipulation routines ************
// Eventually, will use FN_compound_name methods for (some of) these

static const FN_string dot_string((unsigned char *)".");

// A fully qualified name has a trailing dot.

static int
fully_qualified_dotname_p(const FN_string& name)
{
	int last = name.charcount() - 1;

	if (last < 0)
		return (0);
	else
		return (name.compare_substring(last, last, dot_string) == 0);
}


// Find last component of dotted name (rightmost)
// Simple version (don't care about quotes or escapes)

static FN_string *
rightmost_dotname(const FN_string& name, FN_string **rest)
{
	int namelen = name.charcount();
	if (namelen == 1) {
		if (rest)
			*rest = 0;
		return (new FN_string(name));
	}

	FN_string *rightmost;
	int lastposn;
	if (name.compare_substring(namelen-1, namelen-1, dot_string) == 0)
		lastposn = namelen - 2;  // delete trailing dot
	else
		lastposn = namelen - 1;

	int dotloc = name.prev_substring(dot_string, lastposn);
	if (dotloc == FN_STRING_INDEX_NONE) {
		// no separator found, just return copy of whole name
		if (rest)
			*rest = 0;
		return (new FN_string(name));
	}

	// split name at dot  (dot cannot be last)
	rightmost = new FN_string(name, dotloc+1, lastposn);
	if (rest)
		*rest = new FN_string(name, 0, (dotloc? (dotloc-1) : 0));
	return (rightmost);
}

// Return new string composed of headname '.' tailname
// Trailing dot in the headname  is used if it exists, otherwise
// a dot is inserted between the names.

static FN_string *
compose_dotname(const FN_string& headname, const FN_string& tailname)
{
	if (fully_qualified_dotname_p(headname))
		return (new FN_string(0, &headname, &tailname, 0));
	else
		return (new FN_string(0, &headname, &dot_string,
		    &tailname, 0));
}


// ********************** nisplus interface routines *******************

static unsigned
#ifdef DEBUG
FNSP_map_status(nis_error nisstatus, char *msg)
#else
FNSP_map_status(nis_error nisstatus, char *)
#endif /* DEBUG */
{
#ifdef DEBUG
	if (nisstatus != NIS_SUCCESS && msg != 0) {
		nis_perror(nisstatus, msg);
	}
#endif DEBUG

	switch (nisstatus) {
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		return (FN_SUCCESS);
	case NIS_NOTFOUND:
	case NIS_PARTIAL:
		return (FN_E_NAME_NOT_FOUND);
	case NIS_BADNAME:
	case NIS_BADATTRIBUTE:
		return (FN_E_ILLEGAL_NAME);
	case NIS_NOSUCHNAME:
	case NIS_NOSUCHTABLE:
		return (FN_E_NOT_A_CONTEXT);
		// %%% was: context_not_found
	case NIS_NOMEMORY:
	case NIS_NOFILESPACE:
	case NIS_NOPROC:
	case NIS_RES2BIG:
		return (FN_E_INSUFFICIENT_RESOURCES);
	case NIS_S_NOTFOUND:
	case NIS_TRYAGAIN:
	case NIS_UNAVAIL:
		return (FN_E_CTX_UNAVAILABLE);
	case NIS_RPCERROR:
	case NIS_NAMEUNREACHABLE:
	case NIS_CBERROR:
		return (FN_E_COMMUNICATION_FAILURE);
	case NIS_PERMISSION:
	case NIS_CLNTAUTH:
	case NIS_SRVAUTH:
		return (FN_E_CTX_NO_PERMISSION);
	case NIS_NAMEEXISTS:
		return (FN_E_NAME_IN_USE);
	case NIS_CHAINBROKEN:
		return (FN_E_INVALID_ENUM_HANDLE);
	case NIS_FOREIGNNS:
		/* should try to continue with diff ns?  */
		/* return FN_E_continue */
	default:
		return (FN_E_UNSPECIFIED_ERROR); /* generic error */
	}
}


// Maps NIS+ result in nis_result structure to FN_ctx status code
unsigned
FNSP_map_result(nis_result *res, char *msg)
{
	nis_error nisstatus;

	if (res) {
		nisstatus = res->status;
	} else {
		nisstatus = NIS_NOMEMORY;
	}

	return (FNSP_map_status(nisstatus, msg));
}

static inline void
free_nis_result(nis_result *res)
{
	if (res)
		nis_freeresult(res);
}

const FN_identifier &
FNSP_nisplus_address_type_name(void)
{
	return (FNSP_nisplus_address_type);
}

// Create the directory named
unsigned
FNSP_create_directory(const FN_string &dirname, unsigned int access_flags)
{
	nis_result *res, *ares;
	unsigned status;
	nis_error s;
	char leaf[NIS_MAXNAMELEN+1];
	nis_object *obj;
	char *parent;
	nis_name dirname_nn = (nis_name) (dirname.str(&status));

	if (status != FN_SUCCESS)
		return (status);

	/* first check whether directory exists */
	res = nis_lookup(dirname_nn, MASTER_ONLY|FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);
	if (status == FN_SUCCESS) {
#ifdef DEBUG
		fprintf(stderr, "directory %s already exists.\n",
		    dirname_nn);
#endif
		free_nis_result(res);
		return (status);
	}
	free_nis_result(res);

	// Break name into leaf and domain components.
	if ((nis_leaf_of_r(dirname_nn, leaf, NIS_MAXNAMELEN)) == 0) {
		return (FN_E_NOT_A_CONTEXT); /* or IllegalName */
	}
	parent = nis_domain_of(dirname_nn);

	// Get the parent directory object.
	res = nis_lookup(parent, MASTER_ONLY|FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, "Cannot access parent directory");
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}

	// Turn parent directory object into  subdirectory object
	// to be created.
	obj = &(NIS_RES_OBJECT(res)[0]);

	free(obj->zo_owner);
	free(obj->zo_group);
	free(obj->DI_data.do_name);

	obj->zo_owner = nis_local_principal();
	obj->zo_group = nis_local_group();
	obj->zo_access = FNSP_DEFAULT_RIGHTS;
	obj->DI_data.do_name = dirname_nn;

	// Add it to the namespace and create the directory object.
	ares = nis_add(dirname_nn, obj);

	// zero out fields so they won't get de-allocated
	obj->zo_owner = 0;
	obj->zo_group = 0;
	obj->DI_data.do_name = 0;

	status = FNSP_map_result(ares, "Cannot add directory to namespace");
	free_nis_result(ares);
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	} else {
		s = nis_mkdir(dirname_nn,
		    &(obj->DI_data.do_servers.do_servers_val[0]));
		status = FNSP_map_status(s, "Cannot create directory");
		if (status != FN_SUCCESS) {
			(void) nis_remove(dirname_nn, 0);
		}
		free_nis_result(res);
		return (status);
	}
}

extern void
FNSP_nisplus_admin_group(const char *tablename, char *group)
{
	nis_result *res;
	nis_object *obj;
	char group_tblname[NIS_MAXNAMELEN+1];
	const char *start = strchr(tablename, '.');

	strcpy(group_tblname, &(start[1]));
	res = nis_lookup((nis_name) group_tblname, FNSP_nisflags);
	obj = &(res->objects.objects_val[0]);
	if (obj)
		strcpy(group, (char *) obj->zo_group);
	else
		strcpy(group, (char *) nis_local_group());
	free_nis_result(res);
}

/* ********************* Binding tables manipulation ******************** */

// Remove table of given NIS+ name
// If table is non-empty, remove contents first.
// If table is not found, return SUCCESS.

static unsigned
FNSP_remove_table(nis_name tablename)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result *res;
	unsigned status;

	// empty table first
	sprintf(sname, "[]%s", tablename);
	res = nis_remove_entry(sname, 0, REM_MULTIPLE);
	switch (res->status) {
	case NIS_SUCCESS:
	case NIS_NOTFOUND:
		break;
	default:
		status = FNSP_map_result(res, "cannot empty table");
		return (status);
	}
	free_nis_result(res);

	// remove table
	res = nis_remove(tablename, 0);
	status = FNSP_map_result(res, "cannot remove table");
	free_nis_result(res);
	return (status);
}

/*
 * xxx.ctx_dir.yyy -> head = xxx, tail = ctx_dir.yyy
 * ctx_dir.yyy -> head = , tail = ctx_dir.yyy
 *
 * Returns FN_SUCCESS if name split, otherwise, FN_E_NAME_NOT_FOUND
 * (e.g. no 'ctx_dir'
 * in given string).
 */
extern unsigned
split_internal_name(const FN_string &wholename,
    FN_string **head,
    FN_string **tail)
{
	int c_start = wholename.prev_substring(FNSP_context_directory);
	unsigned status = FN_SUCCESS;

	if (c_start == FN_STRING_INDEX_NONE)
		return (FN_E_MALFORMED_REFERENCE);

	*tail = new FN_string(wholename, c_start, wholename.charcount()-1);
	if (*tail == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	if (c_start == 0)
		*head = 0;
	else {
		*head = new FN_string(wholename, 0, c_start-2);
		if (*head == 0) {
			delete *tail;
			*tail = 0;
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
	}

	return (FN_SUCCESS);
}

#define USER_TABLE_NAME "fns_user_"
#define HOST_TABLE_NAME "fns_host_"
#define TABLE_NAME_LEN 9
#define CTX_DIR_NAME_LEN 8
#define USER_CTX_NAME "fns_user.ctx_dir"
#define HOST_CTX_NAME "fns_host.ctx_dir"
#define HU_CTX_NAME_LEN 16

// Create a "bindings" table using the given internal name.
// If table already exists, remove it first.
// Use 'impl' to determine how many columns to create
static unsigned
FNSP_create_bindings_table_base(FNSP_implementation_type impl,
    const FN_string &tabname,
    unsigned int access_flags,
    unsigned string_case)
{
	unsigned status;
	nis_name tablename = (nis_name)tabname.str(&status);
	char owner[NIS_MAXNAMELEN+1], group[NIS_MAXNAMELEN+1];
	table_col tcols[FNSP_WIDE_TABLE_WIDTH];
	nis_object tobj;
	nis_result *res;
	unsigned num_cols = (impl == FNSP_single_table_impl?
	    FNSP_NARROW_TABLE_WIDTH : FNSP_WIDE_TABLE_WIDTH);

	if (status != FN_SUCCESS)
		return (status);
	/* first check whether table exists */
	res = nis_lookup(tablename, MASTER_ONLY|FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);
	free_nis_result(res);
	if (status == FN_SUCCESS) {
#ifdef DEBUG
		fprintf(stderr, "table %s already exists.\n", tablename);
#endif /* DEBUG */
		// %%%%
		// Calling create context via an API would delete the
		// NIS+ table
		// status = FNSP_remove_table(tablename);
		// if (status != FN_SUCCESS)
		return (status);
	}

	if (num_cols == FNSP_WIDE_TABLE_WIDTH) {
		// wide table format has ctx name column
		tcols[FNSP_CTX_COL].tc_name = (char *) FNSP_ctx_col_label;
		tcols[FNSP_CTX_COL].tc_flags = TA_SEARCHABLE;
		tcols[FNSP_CTX_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
		tcols[FNSP_CTX_COL].tc_flags |= TA_CASE;  // case insensitive
	}

	tcols[FNSP_NAME_COL].tc_name = (char *) FNSP_name_col_label;
	tcols[FNSP_NAME_COL].tc_flags = TA_SEARCHABLE;
	tcols[FNSP_NAME_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
	if (string_case == FN_STRING_CASE_INSENSITIVE)
		tcols[FNSP_NAME_COL].tc_flags |= TA_CASE;

	tcols[FNSP_REF_COL].tc_name = (char *) FNSP_ref_col_label;
	tcols[FNSP_REF_COL].tc_flags = TA_BINARY;
	tcols[FNSP_REF_COL].tc_rights = FNSP_DEFAULT_RIGHTS;

	tcols[FNSP_BIND_COL].tc_name = (char *) FNSP_bind_col_label;
	tcols[FNSP_BIND_COL].tc_flags = TA_BINARY;
	tcols[FNSP_BIND_COL].tc_rights = FNSP_DEFAULT_RIGHTS;

	if (num_cols == FNSP_WIDE_TABLE_WIDTH) {
		// wide table format has attribute column
		tcols[FNSP_ATTR_COL].tc_name = (char *) FNSP_attr_col_label;
		tcols[FNSP_ATTR_COL].tc_flags = TA_BINARY;
		tcols[FNSP_ATTR_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
	}

	// Determine the owner of the table!!
	// If the table starts with fns_user_ (or) fns_host_
	// owner should be the respective user/host
	if ((strncmp(tablename, USER_TABLE_NAME, TABLE_NAME_LEN) == 0) ||
	    (strncmp(tablename, HOST_TABLE_NAME, TABLE_NAME_LEN) == 0)) {
		strcpy(owner, (char *) &tablename[TABLE_NAME_LEN]);
		strtok(owner, ".");
		strcat(owner, (char *) &tablename[TABLE_NAME_LEN +
		    strlen(owner) + CTX_DIR_NAME_LEN]);
		tobj.zo_owner = (nis_name) owner;
	} else
		tobj.zo_owner = nis_local_principal();	
	FNSP_nisplus_admin_group(tablename, group);
	tobj.zo_group = (nis_name) group;
	tobj.zo_access = FNSP_DEFAULT_RIGHTS;
	tobj.zo_data.zo_type = NIS_TABLE_OBJ;
	tobj.TA_data.ta_type = (char *) __nis_default_table_type;
	tobj.TA_data.ta_maxcol = num_cols;
	tobj.TA_data.ta_sep = __nis_default_separator;
	tobj.TA_data.ta_path = (char *) __nis_default_path;
	tobj.TA_data.ta_cols.ta_cols_len = num_cols;
	tobj.TA_data.ta_cols.ta_cols_val = tcols;

	// add table to name space
	res = nis_add(tablename, &tobj);
	status = FNSP_map_result(res, "could not create 'bindings' table");
	free_nis_result(res);
	return (status);
}

static FN_ref *
FNSP_extract_lookup_result(nis_result* res, unsigned &status,
    FNSP_binding_type *btype)
{
	FN_ref *ref;

	/* extract reference */
	ref = FN_ref_xdr_deserialize(ENTRY_VAL(res->objects.objects_val,
	    FNSP_REF_COL),
	    ENTRY_LEN(res->objects.objects_val,
	    FNSP_REF_COL),
	    status);
	/* extract binding type */
	if (btype) {
		int blen = ENTRY_LEN(res->objects.objects_val, FNSP_BIND_COL);
		char *bstr = ENTRY_VAL(res->objects.objects_val,
		    FNSP_BIND_COL);
		unsigned bnum = 3;
		// neither FNSP_(child_context, bound_reference)

		if (blen == 1)
			bnum = bstr[0];

		switch (bnum) {
		case FNSP_child_context:
		case FNSP_bound_reference:
			*btype = (FNSP_binding_type) bnum;
			break;
		default:
#ifdef DEBUG
			fprintf(stderr,
			    "FNSP_lookup_binding aux: bad btype %d\n",
			    bstr[0]);
#endif /* DEBUG */
			*btype = FNSP_bound_reference;
			// default appropriate ???
		}
	}

	return (ref);
}


// Lookup the given atomic name 'aname', in the bindings table of the object
// with the given internal name 'tabname'.
// If found, return the reference bound to the name.
// If 'btype' is set, return the binding type associated with the name.
// Upon return 'status' is set to the status of the search.
//
// 'nisflags' contains the NIS+ flags to be used in the NIS+ lookup call.

static FN_ref *
FNSP_lookup_binding_single(const FN_string &tabname,
    const FN_string &aname,
    unsigned &status,
    FNSP_binding_type *btype,
    unsigned nisflags)
{
	char sname[NIS_MAXNAMELEN+1];
	FN_ref *ref = 0;
	nis_result *res;
	nis_name tablename = (nis_name)tabname.str(&status);

	if (status != FN_SUCCESS)
		return (0);

	sprintf(sname, "[%s=\"%s\"], %s",
	    FNSP_name_col_label, aname.str(&status), tablename);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	if (status != FN_SUCCESS)
		return (0);

	res = nis_list(sname, FNSP_nisflags|nisflags, NULL, NULL);
	status = FNSP_map_result(res, 0);
	if (status == FN_SUCCESS && res->objects.objects_len == 0) {
		// Don't know why this happens, but on rare occasions it does.
		status = FN_E_UNSPECIFIED_ERROR;
	}

	if (status == FN_SUCCESS) {
		ref = FNSP_extract_lookup_result(res, status, btype);
	}
	free_nis_result(res);
	return (ref);
}

static FN_ref *
FNSP_lookup_binding_shared(const FN_string &tabname,
    const FN_string &cname,
    const FN_string &aname,
    unsigned &status,
    FNSP_binding_type *btype,
    unsigned nisflags,
    FN_attrset **attrs = 0)
{
	char sname[NIS_MAXNAMELEN+1];
	FN_ref *ref = 0;
	nis_result *res;
	nis_name tablename = (nis_name)tabname.str(&status);

	if (attrs)
		*attrs = NULL;

	if (status != FN_SUCCESS)
		return (0);

	sprintf(sname, "[%s=\"%s\", %s=\"%s\"], %s",
		FNSP_name_col_label, aname.str(&status),
		FNSP_ctx_col_label, cname.str(&status),
		tablename);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	if (status != FN_SUCCESS)
		return (0);

	res = nis_list(sname, FNSP_nisflags|nisflags, NULL, NULL);
	status = FNSP_map_result(res, 0);
	if (status == FN_SUCCESS && res->objects.objects_len == 0) {
		// Don't know why this happens, but on rare occasions it does.
		status = FN_E_UNSPECIFIED_ERROR;
	}

	if (status == FN_SUCCESS) {
		ref = FNSP_extract_lookup_result(res, status, btype);
		if (attrs)
			*attrs = FNSP_extract_attrset_result(res, status);
	}
	free_nis_result(res);
	return (ref);
}

// as a convenience, for the FNSP_shared_table_impl and FNSP_entries_impl,
// the caller can also ask for the attributes associated with the
// named object

static FN_ref *
FNSP_lookup_binding_aux(const FNSP_Address& parent,
    const FN_string &aname,
    unsigned &status,
    FNSP_binding_type* btype,
    unsigned nisflags,
    FN_attrset **attrs = 0)
{
	nisflags |= parent.get_access_flags();

	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		return (FNSP_lookup_binding_single(parent.get_table_name(),
		    aname, status, btype, nisflags));
	case FNSP_shared_table_impl:
		return (FNSP_lookup_binding_shared(parent.get_table_name(),
		    FNSP_self_name, aname, status, btype, nisflags, attrs));
	case FNSP_entries_impl:
		return (FNSP_lookup_binding_shared(parent.get_table_name(),
		    parent.get_index_name(),
		    aname, status, btype, nisflags, attrs));
	default:
		return (0);
	}
}

static FN_ref *
FNSP_lookup_binding(const FNSP_Address& parent,
    const FN_string &aname,
    unsigned &status)
{
	FN_ref *ref = FNSP_lookup_binding_aux(parent, aname, status, 0, 0);
	if (ref == 0) {
		return (0);
	}
	FNSP_process_user_fs(parent, *ref, FNSP_nisplus_get_homedir, status);
	if (status != FN_SUCCESS) {
		delete ref;
		ref = 0;
	}
	return (ref);
}

// Lookup atomic name 'aname' in context associated with 'parent'.
// Return status of operation in 'status', and reference, if found as ret val.

FN_ref *
FNSP_nisplusImpl::lookup_binding(
    const FN_string &aname,
    unsigned &status)
{
	return (FNSP_lookup_binding(*my_address, aname, status));
}


int
FNSP_nisplusImpl::is_this_address_type_p(const FN_ref_addr &addr)
{
	return (((*addr.type()) == FNSP_nisplus_address_type) ||
	    ((*addr.type()) == FNSP_printer_nisplus_address_type));
}


// narrow:
// Add entry consisting of (aname, ref, bind_type) to the bindings table
// associated with 'tabname'.
// wide:
// Add entry consisting of (ctx-name, aname, ref, bind_type, attrs)
// 'flags', if set appropriately, allows the existing entry, if any, to
// be overwritten.

static unsigned
FNSP_add_binding_entry(const FN_string &tabname,
    const FN_string &aname,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned flags,
    FNSP_binding_type bind_type,
    const FN_string *cname = 0)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_object obj;
	entry_col *cols;
	entry_obj *eo;
	char *refbuf;
	nis_result *res;
	int len;
	unsigned status;
	char bt_buf[1];
	nis_name tablename = (nis_name)tabname.str(&status);
	char atomic_tablename[NIS_MAXNAMELEN+1];
	char owner[NIS_MAXNAMELEN+1], group[NIS_MAXNAMELEN+1];
	int num_cols;
	int wide = (cname? 1 : 0);

	if (status != FN_SUCCESS)
		return (FN_E_UNSPECIFIED_ERROR);

	if (wide)
		num_cols = FNSP_WIDE_TABLE_WIDTH;
	else
		num_cols = FNSP_NARROW_TABLE_WIDTH;

	cols = (entry_col*) calloc(num_cols, sizeof (entry_col));
	if (cols == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	memset((char *)(&obj), 0, sizeof (obj));

	nis_leaf_of_r(tablename, atomic_tablename, NIS_MAXNAMELEN);
	obj.zo_name = atomic_tablename;

	// Get the default group name
	FNSP_nisplus_admin_group(tablename, group);
	obj.zo_group = (nis_name) group;

	// Determine the owner of the entry!!
	// If the table starts with fns_user_ (or) fns_host_
	// owner should be the respective user/host
	if ((strncmp(tablename, USER_TABLE_NAME, TABLE_NAME_LEN) == 0) ||
	    (strncmp(tablename, HOST_TABLE_NAME, TABLE_NAME_LEN) == 0)) {
		strcpy(owner, (char *) &tablename[TABLE_NAME_LEN]);
		strtok(owner, ".");
		strcat(owner, (char *) &tablename[TABLE_NAME_LEN +
		    strlen(owner) + CTX_DIR_NAME_LEN]);
		obj.zo_owner = (nis_name) owner;
	} else if ((strncmp(tablename, USER_CTX_NAME, HU_CTX_NAME_LEN)
	    == 0) || (strncmp(tablename, HOST_CTX_NAME, HU_CTX_NAME_LEN)
	    == 0)) {
		strcpy(owner, (char *) (aname.str(&status)));
		strtok(owner, ".");
		strcat(owner, (char *) &tablename[HU_CTX_NAME_LEN]);
		obj.zo_owner = (nis_name) owner;
	} else
		obj.zo_owner = nis_local_principal();

	obj.zo_access = FNSP_DEFAULT_RIGHTS;
	obj.zo_ttl = FNSP_DEFAULT_TTL;
	obj.zo_data.zo_type = NIS_ENTRY_OBJ;
	eo = &(obj.EN_data);
	eo->en_type = (char *)__nis_default_table_type;
	eo->en_cols.en_cols_val = cols;
	eo->en_cols.en_cols_len = num_cols;

	ENTRY_VAL(&obj, FNSP_NAME_COL) = (nis_name) (aname.str(&status));
	ENTRY_LEN(&obj, FNSP_NAME_COL) = aname.bytecount() + 1;

	refbuf = FN_ref_xdr_serialize(ref, len);
	if (refbuf == NULL) {
		free(cols);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	ENTRY_VAL(&obj, FNSP_REF_COL) = refbuf;
	ENTRY_LEN(&obj, FNSP_REF_COL) = len;
	ENTRY_FLAGS(&obj, FNSP_REF_COL) = EN_BINARY;

	bt_buf[0] = bind_type;
	ENTRY_VAL(&obj, FNSP_BIND_COL) = &bt_buf[0];
	ENTRY_LEN(&obj, FNSP_BIND_COL) = 1;
	ENTRY_FLAGS(&obj, FNSP_BIND_COL) = EN_BINARY;

	if (!wide) {
		sprintf(sname, "[%s=\"%s\"], %s", FNSP_name_col_label,
			aname.str(&status), tablename);
	} else {
		/* wide table format has 'context' and 'attributes' column */
		ENTRY_VAL(&obj, FNSP_CTX_COL) = (nis_name)
		    (cname->str(&status));
		ENTRY_LEN(&obj, FNSP_CTX_COL) = cname->bytecount() + 1;

		sprintf(sname, "[%s=\"%s\", %s=\"%s\"], %s",
			FNSP_name_col_label, aname.str(&status),
			FNSP_ctx_col_label, cname->str(&status),
			tablename);

		/* initialize attribute column with given attributes */
		FNSP_store_attrset(attrs, &(ENTRY_VAL(&obj, FNSP_ATTR_COL)),
		    (size_t *) &(ENTRY_LEN(&obj, FNSP_ATTR_COL)));
		ENTRY_FLAGS(&obj, FNSP_ATTR_COL) = EN_BINARY;
	}
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	res = nis_add_entry(sname, &obj,
	    (flags&FN_OP_EXCLUSIVE) ? 0 : ADD_OVERWRITE);
	status = FNSP_map_result(res, "could not add entry");
	if (wide)
		free(ENTRY_VAL(&obj, FNSP_ATTR_COL));

	// not found must mean table does not exist.
	if (status == FN_E_NAME_NOT_FOUND)
		status = FN_E_NOT_A_CONTEXT; // %%% CONTEXT_NOT_FOUND

	free(refbuf);
	free_nis_result(res);
	free(cols);
	return (status);
}

static unsigned
FNSP_add_binding_aux(const FNSP_Address& parent,
    const FN_string &aname,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned flags,
    FNSP_binding_type bind_type)
{
	unsigned answer;
	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		answer = FNSP_add_binding_entry(parent.get_table_name(),
		    aname, ref, NULL, flags, bind_type);
		/* setting of attributes is done by caller as required */
		break;

	case FNSP_shared_table_impl:
		answer = FNSP_add_binding_entry(parent.get_table_name(),
		    aname, ref, attrs, flags, bind_type,
		    &FNSP_self_name);
		break;

	case FNSP_entries_impl:
		answer = FNSP_add_binding_entry(parent.get_table_name(),
		    aname, ref, attrs, flags, bind_type,
		    &(parent.get_index_name()));
		break;
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		return (FN_E_MALFORMED_REFERENCE);
	}

	return (answer);
}


// Add new binding (aname, ref) to bindings table associated with 'parent'.
//
// Policy:  Cannot overwrite a child reference if the new reference does not
//	    atleast have an address of the original FNSP context

unsigned
FNSP_nisplusImpl::add_binding(
    const FN_string &aname,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned flags)
{
	unsigned status;

	if (flags&FN_OP_EXCLUSIVE)
		return (FNSP_add_binding_aux(*my_address, aname, ref, attrs,
		    flags, FNSP_bound_reference));

	// bind_supercede:  check that we are not overwriting a child reference
	FNSP_binding_type btype;
	unsigned lstatus;
	FN_attrset *old_attrs = NULL;
	FN_ref *oref = FNSP_lookup_binding_aux(*my_address, aname, lstatus,
	    &btype, MASTER_ONLY, &old_attrs);

	if (lstatus == FN_E_NAME_NOT_FOUND ||
	    (lstatus == FN_SUCCESS && btype != FNSP_child_context)) {
		// binding does not exist or not a child context
		status = FNSP_add_binding_aux(*my_address, aname, ref,
		    (attrs ? attrs : old_attrs), flags,
		    FNSP_bound_reference);
	} else if (lstatus == FN_SUCCESS && btype == FNSP_child_context) {
		// binding to child
		// check if new address list contains the old address
		if (check_if_old_addr_present(*oref, ref) == 0)
		    status = FN_E_NAME_IN_USE;
		else
			status = FNSP_add_binding_aux(*my_address, aname, ref,
			    (attrs ? attrs : old_attrs), flags,
			    FNSP_child_context);
	} else {
		// some other kind of error
		status = lstatus;
	}

	// attrs for FNSP_single_table_impl is handled by caller, not here

	delete old_attrs;
	delete oref;
	return (status);
}


// Remove entry with atomic name 'aname' from bindings table associated with
// 'tabname'.
// Return status of operation.
// aname is the atomic name
// cname is the context name
//
// If aname == 0, that means remove all names identified by cname
// If cname == 0, that means table does not have further context name
// qualification (i.e. entire table is the context)

static unsigned
FNSP_remove_binding_entry(const FN_string &tabname,
    const FN_string *aname,
    const FN_string *cname = 0)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result *res;
	unsigned status;
	nis_name tablename = (nis_name)tabname.str(&status);

	if (aname) {
		if (cname == 0)
			sprintf(sname, "[%s=\"%s\"], %s",
				FNSP_name_col_label, aname->str(&status),
				tablename);
		else
			sprintf(sname, "[%s=\"%s\", %s=\"%s\"], %s",
				FNSP_name_col_label, aname->str(&status),
				FNSP_ctx_col_label, cname->str(&status),
				tablename);
	} else {
		// no atomic name specified;
		// remove all entries qualified by context information
		if (cname == 0)
			sprintf(sname, "[], %s",	tablename);
		else
			sprintf(sname, "[%s=\"%s\"], %s",
				FNSP_ctx_col_label, cname->str(&status),
				tablename);
	}

	if (sname[strlen(sname)-1] != '.')
	    strcat(sname, ".");

	res = nis_remove_entry(sname, 0, REM_MULTIPLE);
	status = FNSP_map_result(res, "could not remove entry");
	free_nis_result(res);
	return (status);
}

static unsigned
FNSP_remove_binding_aux(const FNSP_Address& parent, const FN_string &aname)
{
	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		return (FNSP_remove_binding_entry(parent.get_table_name(),
		    &aname));

	case FNSP_shared_table_impl:
		return (FNSP_remove_binding_entry(parent.get_table_name(),
		    &aname, &FNSP_self_name));
	case FNSP_entries_impl:
		return (FNSP_remove_binding_entry(parent.get_table_name(),
		    &aname, &(parent.get_index_name())));
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		return (FN_E_MALFORMED_REFERENCE);
	}
}


// Remove entry with atomic name 'aname' from bindings table of 'parent'.
// Policies:
// 1. Cannot remove binding of a child context if child context still exists
//    (must explicitly destroy)
// 2. return 'success' if binding was not there to begin with.

unsigned
FNSP_nisplusImpl::remove_binding(const FN_string &aname)
{
	FNSP_binding_type btype;
	unsigned status;
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, aname, status,
	    &btype, MASTER_ONLY);
	switch (status) {
	case FN_SUCCESS:
		if (btype == FNSP_child_context) {
			FNSP_nisplus_address child(*ref);
			if (child.get_context_type() == 0)
				status = FN_E_MALFORMED_REFERENCE; // %%%
			else {
				switch (FNSP_context_exists(child)) {
				case FN_E_NOT_A_CONTEXT:
					// %%% was context_not_found
					// context no longer exists;
					// go ahead and unbind
					status = FNSP_remove_binding_aux(
					    *my_address, aname);
					break;
				case FN_SUCCESS:
					// must explicitly destroy to
					// avoid orphan
					status = FN_E_NAME_IN_USE;
					break;
				default:
					// cannot determine state of
					// child context
					status = FN_E_OPERATION_NOT_SUPPORTED;
				}
			}
		} else
			status = FNSP_remove_binding_aux(*my_address, aname);
		break;

	case FN_E_NAME_NOT_FOUND:
		status = FN_SUCCESS;
	}
	delete ref;
	return (status);
}




// narrow:
// Entry consisting of (aname, ref, bind_type) to the bindings table
// Change aname to newname.
// wide:
// Entry consisting of (contextname, aname, ref, bind_type)
// Change aname to newname.
//
// 'flags', if set appropriately, allows the existing entry, if any, to
// be overwritten.

static unsigned
FNSP_rename_binding_entry(const FN_string &tabname,
    const FN_string &aname,
    const FN_string &newname,
    unsigned flags,
    const FN_string *cname = 0)
{
	char sname[NIS_MAXNAMELEN+1];
	char group[NIS_MAXNAMELEN+1];
	nis_object obj;
	entry_col	*cols;
	entry_obj	*eo;
	nis_result *res;
	unsigned status;
	char bt_buf[1];
	nis_name tablename = (nis_name)tabname.str(&status);
	char atomic_tablename[NIS_MAXNAMELEN+1];
	int num_cols;
	int wide = (cname? 1 : 0);

	if (status != FN_SUCCESS)
		return (0);

	if (wide)
		num_cols = FNSP_WIDE_TABLE_WIDTH;
	else
		num_cols = FNSP_NARROW_TABLE_WIDTH;

	cols = (entry_col*) calloc(num_cols, sizeof (entry_col));
	memset((char *)(&obj), 0, sizeof (obj));

	nis_leaf_of_r(tablename, atomic_tablename, NIS_MAXNAMELEN);
	obj.zo_name = atomic_tablename;
	FNSP_nisplus_admin_group(tablename, group);
	obj.zo_group = (nis_name) group;
	obj.zo_access = FNSP_DEFAULT_RIGHTS;
	obj.zo_ttl = FNSP_DEFAULT_TTL;
	obj.zo_data.zo_type = NIS_ENTRY_OBJ;
	eo = &(obj.EN_data);
	eo->en_type = (char *) __nis_default_table_type;
	eo->en_cols.en_cols_val = cols;
	eo->en_cols.en_cols_len = num_cols;

	ENTRY_VAL(&obj, FNSP_NAME_COL) = (nis_name) (newname.str(&status));
	ENTRY_LEN(&obj, FNSP_NAME_COL) = newname.bytecount() + 1;
	ENTRY_FLAGS(&obj, FNSP_NAME_COL) = EN_MODIFIED;

	/* clear the rest of the columns not being modified */
	ENTRY_VAL(&obj, FNSP_REF_COL) = 0;
	ENTRY_LEN(&obj, FNSP_REF_COL) = 0;
	ENTRY_FLAGS(&obj, FNSP_REF_COL) = EN_BINARY;

	ENTRY_VAL(&obj, FNSP_BIND_COL) = 0;
	ENTRY_LEN(&obj, FNSP_BIND_COL) = 0;
	ENTRY_FLAGS(&obj, FNSP_BIND_COL) = EN_BINARY;

	if (!wide) {
		sprintf(sname, "[%s=\"%s\"], %s", FNSP_name_col_label,
			aname.str(&status), tablename);
	} else {
		/* these other columns are not modified */
		ENTRY_VAL(&obj, FNSP_CTX_COL) = 0;
		ENTRY_LEN(&obj, FNSP_CTX_COL) = 0;

		/* wide table format has 'context' column */
		sprintf(sname, "[%s=\"%s\", %s=\"%s\"], %s",
			FNSP_name_col_label, aname.str(&status),
			FNSP_ctx_col_label, cname->str(&status),
			tablename);

		ENTRY_VAL(&obj, FNSP_ATTR_COL) = 0;
		ENTRY_LEN(&obj, FNSP_ATTR_COL) = 0;
		ENTRY_FLAGS(&obj, FNSP_ATTR_COL) = EN_BINARY;
	}
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	res = nis_modify_entry(sname, &obj,
	    (flags&FN_OP_EXCLUSIVE) ? 0 : ADD_OVERWRITE);
	status = FNSP_map_result(res, "could not modify entry");

	// not found must mean table does not exist.
	if (status == FN_E_NAME_NOT_FOUND)
		status = FN_E_NOT_A_CONTEXT; // %%% was CONTEXT_NOT_FOUND;

	free_nis_result(res);
	free(cols);
	return (status);
}

static unsigned
FNSP_rename_binding_aux(const FNSP_Address& parent,
    const FN_string &aname,
    const FN_string &newname,
    unsigned flags)
{
	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		return FNSP_rename_binding_entry(parent.get_table_name(),
		    aname, newname, flags);

	case FNSP_shared_table_impl:
		return FNSP_rename_binding_entry(parent.get_table_name(),
		    aname, newname, flags, &FNSP_self_name);
	case FNSP_entries_impl:
		return FNSP_rename_binding_entry(parent.get_table_name(),
		    aname, newname, flags, &(parent.get_index_name()));
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		return (FN_E_MALFORMED_REFERENCE);
	}
}

// Rename binding (aname, newname) in bindings table associated with 'parent'.
//
// Policy:  Cannot overwrite if newname is bound to a child reference

unsigned
FNSP_nisplusImpl::rename_binding(
    const FN_string &aname,
    const FN_string &newname,
    unsigned flags)
{
	unsigned status;

	if (flags&FN_OP_EXCLUSIVE)
		return (FNSP_rename_binding_aux(*my_address, aname, newname,
		    flags));

	// bind_supercede:  check that we are not overwriting a child reference
	FNSP_binding_type btype;
	unsigned lstatus;
	FN_ref *oref = FNSP_lookup_binding_aux(*my_address, aname, lstatus,
	    &btype, MASTER_ONLY);

	if (lstatus == FN_E_NAME_NOT_FOUND ||
	    (lstatus == FN_SUCCESS && btype != FNSP_child_context)) {
		// binding does not exist or not a child context
		status = FNSP_rename_binding_aux(*my_address, aname, newname,
		    flags);
	} else if (lstatus == FN_SUCCESS && btype == FNSP_child_context) {
		// newname is bound to child reference; cannot do that
		    status = FN_E_NAME_IN_USE;
	} else {
		// some other kind of error
		status = lstatus;
	}
	delete oref;
	return (status);
}


// Callback function used for constructing FN_nameset of names
// found in a bindings table.
// Assumes 'udata' points to an FNSP_nameset_cb_t struct containing
// the FN_nameset to add to, and updates its contents.

typedef struct {
	FN_nameset *nameset;
	int flags;
} FNSP_nameset_cb_t;

static int
add_obj_to_nameset(char *, nis_object *ent, void *udata)
{
	FNSP_nameset_cb_t *ld = (FNSP_nameset_cb_t *) udata;
	FN_nameset *ns;

	ns = ld->nameset;

	/* child_only */
	if (ld->flags) {
		int blen = ENTRY_LEN(ent, FNSP_BIND_COL);
		char *bstr = ENTRY_VAL(ent, FNSP_BIND_COL);
		unsigned bnum = 3; /* bogus default */

		if (blen == 1)
			bnum = bstr[0];

		switch (bnum) {
		case FNSP_child_context:
			break;

		default: /* otherewise, ignore this entry */
			return (0);
		}
	}
	// Remove the _FNS_nns_ names
	if (strncmp((char *) (ENTRY_VAL(ent, FNSP_NAME_COL)),
	    FNSP_nns_name, strlen(FNSP_nns_name)) != 0)
		ns->add((unsigned char *)(ENTRY_VAL(ent, FNSP_NAME_COL)));
	return (0);
}


// Callback function used for constructing FN_bindingset of bindings
// found in a bindings table.
// Assumes 'udata' contains an FNSP_bindingset_cb_t structure.

typedef struct {
	const FNSP_Address *parent;
	FN_bindingset *bs;
} FNSP_bindingset_cb_t;

static int
add_obj_to_bindingset(char *, nis_object *ent, void *udata)
{
	FNSP_bindingset_cb_t *cbdata = (FNSP_bindingset_cb_t *)udata;
	unsigned status;

	FN_ref *ref =
	    FN_ref_xdr_deserialize(ENTRY_VAL(ent, FNSP_REF_COL),
	    ENTRY_LEN(ent, FNSP_REF_COL),
	    status);
	if (status == FN_SUCCESS && ref) {
		FNSP_process_user_fs(*cbdata->parent, *ref,
		    FNSP_nisplus_get_homedir, status);
		if ((status == FN_SUCCESS) &&
		    (strncmp((char *) (ENTRY_VAL(ent, FNSP_NAME_COL)),
		    FNSP_nns_name, strlen(FNSP_nns_name)) != 0)) {
			cbdata->bs->add(
			    (unsigned char *)(ENTRY_VAL(ent, FNSP_NAME_COL)),
			    *ref);
		}
	}
	delete ref;
	return (0);
}



// Base function used by FNSP_list_names and FNSP_list_bindings to construct
// FN_nameset and FN_bindingset of bindings associated with 'tabname'.

unsigned
FNSP_get_binding_entries(const FN_string &tabname,
    unsigned int access_flags,
    int (*add_func)(char *, nis_object*, void *),
    void *add_params,
    const FN_string *cname)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result *res;
	unsigned status;
	nis_name tablename = (nis_name)tabname.str(&status);

#ifdef CAREFUL_BUT_SLOW
	/* lookup bindings table object */
	res = nis_lookup(tablename, FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);

	if (status == FN_E_NAME_NOT_FOUND)
		status = FN_E_NOT_A_CONTEXT; // %%% CONTEXT_NOT_FOUND

	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}

	/* Make sure it is a table object. */
	if (res->objects.objects_val[0].zo_data.zo_type != NIS_TABLE_OBJ) {
#ifdef DEBUG
		fprintf(stderr, "%s is not a table!\n", tablename);
#endif /* DEBUG */
		free_nis_result(res);
		return (FN_E_NOT_A_CONTEXT);
	}
	free_nis_result(res);
#endif /* CAREFUL_BUT_SLOW */

	/* Construct query that identifies entries in context */
	if (cname == 0)
		sprintf(sname, tablename);
	else
		sprintf(sname, "[%s=\"%s\"], %s",
			FNSP_ctx_col_label, cname->str(), tablename);

	/* Get table contents using callback function */
	res = __nis_list_localcb(sname, FNSP_nisflags|access_flags,
				    add_func, add_params);

	if (res->status == NIS_RPCERROR) {
		// may have failed because too big; use TCP
		free_nis_result(res);
		unsigned long new_flags =
		    access_flags|(FNSP_nisflags&(~USE_DGRAM));
		res = __nis_list_localcb(sname, new_flags, add_func,
		    add_params);
	}

	if ((res->status == NIS_CBRESULTS) || (res->status == NIS_NOTFOUND))
		status = FN_SUCCESS;
	else
		status = FNSP_map_result(res, 0);

	free_nis_result(res);
	return (status);
}


// Return names of bindings associated with 'tabname'.
// Set 'status' appropriately upon return.

FN_nameset *
FNSP_list_names(const FNSP_Address& parent, unsigned &status, int children_only)
{
	FN_nameset *ns = new FN_nameset;

	if (ns == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	FNSP_nameset_cb_t ld;
	ld.nameset = ns;
	ld.flags = children_only;

	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_nameset, (void *) &ld);
		break;
	case FNSP_shared_table_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_nameset, (void *) &ld,
		    &FNSP_self_name);
		break;
	case FNSP_entries_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_nameset, (void *) &ld,
		    &(parent.get_index_name()));
		// get rid of context identifier ('self')
		if (status == FN_SUCCESS)
			ns->remove(FNSP_self_name);
		break;
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		status = FN_E_MALFORMED_REFERENCE;
	}

	if (status != FN_SUCCESS) {
		delete ns;
		ns = 0;
	}
	return (ns);
}

FN_namelist *
FNSP_nisplusImpl::list_names(unsigned &status, int children_only)
{
	FN_nameset *answer = FNSP_list_names(*my_address, status, children_only);
	if (answer)
		return (new FN_namelist_svc(answer));
	else
		return (0);
}


// Return bindings associated with 'parent'.
// Set 'status' appropriately upon return.

FN_bindingset *
FNSP_list_bindings(const FNSP_Address& parent, unsigned &status)
{
	FN_bindingset *bs = new FN_bindingset;
	if (bs == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	FNSP_bindingset_cb_t cbdata;
	cbdata.parent = &parent;
	cbdata.bs = bs;

	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_bindingset, &cbdata);
		break;
	case FNSP_shared_table_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_bindingset, &cbdata,
		    &FNSP_self_name);
		break;
	case FNSP_entries_impl:
		status = FNSP_get_binding_entries(parent.get_table_name(),
		    parent.get_access_flags(),
		    add_obj_to_bindingset, &cbdata,
		    &(parent.get_index_name()));
		// get rid of context identifier ('self')
		if (status == FN_SUCCESS)
			bs->remove(FNSP_self_name);
		break;
	default:
#ifdef DEBUG
		fprintf(stderr, "bad implementation type = %d\n",
		    parent.get_impl_type());
#endif /* DEBUG */
		status = FN_E_MALFORMED_REFERENCE;
	}

	if (status != FN_SUCCESS) {
		delete bs;
		bs = 0;
	}
	return (bs);
}

FN_bindinglist *
FNSP_nisplusImpl::list_bindings(unsigned &status)
{
	FN_bindingset *answer = FNSP_list_bindings(*my_address, status);
	if (answer)
		return (new FN_bindinglist_svc(answer));
	else
		return (0);
}


/* ******************** directory operations ***************************** */


// Callback function used to determine whether the given object is
// a directory object.
// Assumes 'udata' points to a counter to be updated if object is a directory.

static int
_is_subdir(char *, nis_object *ent, void *udata)
{
	int *subdir_count;
	uint32_t entry_type, t;

	subdir_count = (int *) udata;
	memcpy(&t, ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val,
	    sizeof (t));
	entry_type = ntohl(t);

	if (entry_type == NIS_DIRECTORY_OBJ)
		++*subdir_count;

	return (0);
}


// Conditions for valid directory removal:
// 1.  object named by 'dirname' is a directory
// 2.  directory has no subdirectories
//
// Returns 'success' if OK; error status otherwise.
// If success, set 'return_res' to point to result of lookup of directory obj.

static unsigned
FNSP_check_conditions_for_rmdir(nis_name dirname, nis_result **return_res,
				unsigned int access_flags)
{
	unsigned status;
	nis_result *res = 0;

	*return_res = 0;

	res = nis_lookup(dirname, MASTER_ONLY|FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (0);
	}

	/* Make sure it is a directory object. */
	if (res->objects.objects_val[0].zo_data.zo_type != NIS_DIRECTORY_OBJ) {
#ifdef DEBUG
		fprintf(stderr, "%s is not a directory!\n", dirname);
#endif /* DEBUG */
		free_nis_result(res);
		return (FN_E_NOT_A_CONTEXT); // ? or bad_reference?
	}

#ifdef CAREFUL_BUT_SLOW
	int subdir_count = 0;
	nis_result *lres = 0;
	// Check whether directory has subdirectories.
	// Get directory contents using callback and check whether it
	// has subdirectories.
	lres = __nis_list_localcb(dirname, access_flags, _is_subdir,
	    (void *) &subdir_count);
	if ((lres->status == NIS_CBRESULTS) ||
	    (lres->status == NIS_NOTFOUND)) {
		if (subdir_count == 0)
			status = FN_SUCCESS;
		else
			status = FN_E_CTX_NOT_EMPTY;
		// ??? config error?
	} else
		status = FNSP_map_result(lres, 0);

	if (status != FN_SUCCESS) {
		free_nis_result(res);
		free_nis_result(lres);
		return (0);
	}
	free_nis_result(lres);
#endif /* CAREFUL_BUT_SLOW */

	*return_res = res;
	return (FN_SUCCESS);
}


// Delete directory specified.
//
// Policy:
// 1. Can only delete a directory if it is has no subdirectories
// 2. Can only delete it if can contact all replicas for deletion;

static unsigned
FNSP_destroy_directory(const FN_string &dirname, unsigned int access_flags)
{
	unsigned status;
	nis_result *res2, *res;
	nis_object *obj;
	int nserv, i, j, failed;
	nis_server *servers;
	nis_error s;
	nis_name dirname_nn = (nis_name) (dirname.str(&status));

	status = FNSP_check_conditions_for_rmdir(dirname_nn, &res,
						    access_flags);

	if (status != FN_SUCCESS) {
		return (status);
	}

	obj = &(NIS_RES_OBJECT(res)[0]);
	nserv = obj->DI_data.do_servers.do_servers_len;
	servers = obj->DI_data.do_servers.do_servers_val;

	// remove directory from name space
	res2 = nis_remove(dirname_nn, 0);
	status = FNSP_map_result(res2, 0);
	free_nis_result(res2);
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}

	// remove replica directories
	for (failed = 0, i = 1; i < nserv; i++) {
		s = nis_rmdir(dirname_nn, &(servers[i]));
		if (s != NIS_SUCCESS) {
			failed++;
#ifdef DEBUG
			fprintf(stderr, "cannot remove replica %s: %s.\n",
				servers[i].name, nis_sperrno(s));
#endif /* DEBUG */
		} else {
			xdr_free((xdrproc_t) xdr_nis_server,
			    (char *) &(servers[i]));
			servers[i].name = 0;
		}
	}

	// check for failure and add back object
	if (failed) {
		status = FNSP_map_status(s, 0);
		goto put_back;
	}

	//  if all replica directories were removed, remove master
	s = nis_rmdir(dirname_nn, &(servers[0]));
	status = FNSP_map_status(s, 0);
	if (status == FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}
#ifdef DEBUG
	fprintf(stderr, "cannot remove master %s: %s.\n",
		servers[0].name, nis_sperrno(s));
#endif /* DEBUG */

put_back:
	// put any servers that were not removed back into directory
	for (j = 0, i = 0; i < nserv; i++) {
		if (servers[i].name) {
			if (j == i)
				j++;
			else
				servers[j++] = servers[i];
		}
	}
	if (j) {
		obj->DI_data.do_servers.do_servers_len = j;
		res2 = nis_add(dirname_nn, obj);
		/* ignore status */
		free_nis_result(res2);
	}
	free_nis_result(res);
	return (status);
}

/* ******************* creating and destroying contexts ***************** */

// Returns whether given name is name of context directory
static inline unsigned
FNSP_context_directory_p(const FN_string &name)
{
	return (name.compare_substring(0, FNSP_cd_size-1,
	    FNSP_context_directory) == 0);
}

static unsigned
FNSP_table_empty_p(nis_name tablename, unsigned &status)
{
	nis_result *res = nis_first_entry(tablename);
	unsigned answer = 0;

	if (res->status == NIS_NOTFOUND || res->status == NIS_NOSUCHTABLE)
		answer = 1;
	else
		status = FNSP_map_result(res, 0);

	free_nis_result(res);
	return (answer);
}

static unsigned
FNSP_remove_context(const FNSP_Address &parent)
{
	unsigned status;
	int done = 0;
	FN_nameset* ns;
	nis_name tablename = (nis_name)parent.get_table_name().str();

	switch (parent.get_impl_type()) {
	case FNSP_single_table_impl:
		if (FNSP_table_empty_p(tablename, status)) {
			status = FNSP_remove_table(tablename);
		} else
			status = FN_E_CTX_NOT_EMPTY;
		break;
	case FNSP_shared_table_impl:
	case FNSP_entries_impl:
		// check if context is empty
		ns = FNSP_list_names(parent, status, 1); /* children_only */
		if (status == FN_E_NAME_NOT_FOUND ||
		    status == FN_E_NOT_A_CONTEXT) {
			// %%% was context_not_found
			status = FN_SUCCESS;
			done = 1;
		} else if (ns) {
			if (ns->count() > 0)
				status = FN_E_CTX_NOT_EMPTY;
			delete ns;
		}
		if (done || status != FN_SUCCESS)
			break;
		if (parent.get_impl_type() == FNSP_shared_table_impl)
			status = FNSP_remove_table(tablename);
		else {
			/* remove all entries associated with this context */
			status = FNSP_remove_binding_entry(
			    parent.get_table_name(), 0,
			    &(parent.get_index_name()));
		}
		break;
	default:
		status = FN_E_MALFORMED_REFERENCE;
		break;
	}

	return (status);
}


// For contexts that involve directories (only ctx_dir for now)
//  Conditions for removal:
//  1.  Bindings table should be empty
//  2.  Directory should have no subdirectories
//
// For contexts that involve only a bindings table,
// check that it is empty.
//
// If context structure no longer exists, return success.
//
static unsigned
FNSP_destroy_context(const FNSP_Address &parent, const FN_string *dirname = 0)
{
	unsigned status = FNSP_remove_context(parent);

	if (status == FN_SUCCESS && dirname) {
		// destroy entire context directory.
		FN_string *ctx_dir = new FN_string(&status,
		    &FNSP_context_directory, dirname, 0);
		if (status == FN_SUCCESS) {
			status = FNSP_destroy_directory(*ctx_dir,
			    parent.get_access_flags());
			delete(ctx_dir);
		}
	}

	// not there; still OK
	if (status == FN_E_NAME_NOT_FOUND ||
	    status == FN_E_NOT_A_CONTEXT)
		// %%% was context_not_found
		status = FN_SUCCESS;

	return (status);
}

unsigned
FNSP_nisplusImpl::destroy_context(const FN_string *dirname)
{
	return (FNSP_destroy_context(*my_address, dirname));
}



// Return legalized value of given name
//

static FN_string
legalize_value(const FN_string &name)
{
	const unsigned char *name_str = name.str();
	int len  = name.bytecount(), i, ri = 0;
	unsigned char *result = 0;

	if (name_str != 0)
		result = (unsigned char *) malloc(len+len+1);
	// double for escapes
	if (result == 0)
		return (FN_string((unsigned char *) ""));

	for (i = 0; i < len; i++) {
		if (fnsp_meta_char(name_str[i])) {
			result[ri++] = FNSP_default_char;
			result[ri++] = name_str[i];
		} else if (nis_bad_value_char(name_str[i]))
			result[ri++] = FNSP_default_char;
		else
			result[ri++] = name_str[i];
	}
	result[ri] = '\0';

	FN_string res(result);
	delete result;
	return (res);
}


// Construct a legal nis+ name.
// Terminals (. [ ] , =) to '_'.
// Leading '@', '+', '-' to '_'.

static FN_string
legalize_tabname(const FN_string &name)
{
	const unsigned char *name_str = name.str();
	int len  = name.bytecount(), i, ri = 0;
	unsigned char *result = 0;

	if (name_str != 0)
		result = (unsigned char *) malloc(len+len+1);
	if (result == 0)
		return (FN_string((unsigned char *) ""));

	if (len >= 0) {
		if (nis_bad_lead_char(name_str[0]) ||
		    nis_terminal_char(name_str[0]))
			result[ri++] = FNSP_default_char;
		else if (fnsp_meta_char(name_str[0])) {
			result[ri++] = FNSP_default_char;
			result[ri++] = name_str[0];
		} else
			result[ri++] = name_str[0];
	}

	for (i = 1; i < len; i++) {
		if (fnsp_meta_char(name_str[i])) {
			result[ri++] = FNSP_default_char;
			result[ri++] = name_str[i];
		} else if (nis_terminal_char(name_str[i]))
			result[ri++] = FNSP_default_char;
		else
			result[ri++] = name_str[i];
	}
	result[ri] = '\0';

	FN_string res(result);
	delete result;
	return (res);
}

/*
 *  Return a legal internal tablename using the parent's internal name and the
 * child's component name.
 * Set 'status' appropriately upon return.
 * If 'find_legal_childname' is set, substitute given child name with
 * one that is legal; otherwise, assume given name is legal.
 *
 * Rules for composing an internal table name of
 * parent = xxx.ctx_dir.yyy, child = zzz
 * (zzz, xxx.ctx_dir.yyy) -> xxx_zzz.ctx_dir.yyy
 * (zzz, ctx_dir.yyy) -> zzz.ctx_dir.yyy
 */
static FN_string *
compose_child_tablename(const FN_string &parent_tabname,
    const FN_string &childname,
    unsigned &status,
    int find_legal_childname)
{
	FN_string realchild = (find_legal_childname?
	    legalize_tabname(childname) : childname);
	char buf[NIS_MAXNAMELEN+1];
	FN_string *head, *tail;
	FN_string *answer = 0;

	status = split_internal_name(parent_tabname, &head, &tail);
	if (status == FN_SUCCESS) {
		if (head)
			sprintf(buf, "%s%c%s.%s",
			    head->str(),
			    FNSP_internal_name_separator,
			    realchild.str(),
			    tail->str());
		else
			sprintf(buf, "%s.%s", realchild.str(), tail->str());
		answer = new FN_string((unsigned char *)buf);
		if (answer == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
	}

	delete head;
	delete tail;
	return (answer);
}

// Creates a 'normal' (non-merged) FNSP context and return its reference.
//
// Create a FNSP reference using (tabname, reftype, context_type)
// and construct the structures (bindings table and sometimes directory)
// associated with it.

static FN_ref *
FNSP_create_normal_context(const FNSP_Address& new_addr,
    const FN_identifier &reftype,
    const FN_attrset *attrs,
    unsigned string_case,
    unsigned &status)
{
	FNSP_implementation_type impl;
	FN_ref *ref;
	if ((new_addr.get_context_type() == FNSP_printername_context) ||
	    (new_addr.get_context_type() == FNSP_printer_object)) {
		// For the printername and printer objects to set
		// the correct context_type. This is a hack to provide
		// backward compatibility wuth FNS in 2.5
		ref = FNSP_reference(
		    FNSP_printer_nisplus_address_type,
		    reftype,
		    new_addr.get_internal_name(),
		    (new_addr.get_context_type() - 11),
		    FNSP_normal_repr);
	} else
		ref = FNSP_reference(
		    FNSP_nisplus_address_type_name(),
		    reftype,
		    new_addr.get_internal_name(),
		    new_addr.get_context_type(),
		    FNSP_normal_repr);

	if (ref == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	switch (impl = new_addr.get_impl_type()) {
	case FNSP_single_table_impl:
	{
		FN_string *attrtab;
		FN_string *suffix = new FN_string(
		    (const unsigned char *)"attribute");
		// kludge to create attribute table
		attrtab = compose_child_tablename(new_addr.get_table_name(),
		    *suffix, status, 0);
		if (attrtab) {
			status = FNSP_create_attribute_table_base(*attrtab,
			    new_addr.get_access_flags(),
			    FN_STRING_CASE_SENSITIVE);
		}
		delete suffix;
		delete attrtab;
		if (status != FN_SUCCESS)
			return (0);
		// otherwise fall through to create context table
	}
	case FNSP_shared_table_impl:
		// create bindings table for storing bindings of new context
		// ignore attrs (these will be stored with binding)
		status = FNSP_create_bindings_table_base(impl,
		    new_addr.get_table_name(),
		    new_addr.get_access_flags(),
		    string_case);
		break;
	case FNSP_entries_impl:
		// add entry with atomicname = 'self' to signify new context
		// 'ref' is actually ref of self
		status = FNSP_add_binding_entry(new_addr.get_table_name(),
		    FNSP_self_name, *ref, attrs, 0,
		    FNSP_bound_reference,
		    &(new_addr.get_index_name()));
		break;
	default:
		status = FN_E_MALFORMED_REFERENCE;
	}

	if (status != FN_SUCCESS) {
		delete ref;
		return (0);
	}

	return (ref);
}

// Creates a FNSP context and return its reference.

static FN_ref *
FNSP_create_context(const FNSP_Address &new_addr,
    unsigned int &status,
    const FN_string *dirname = 0,
    const FN_identifier *reftype = 0,
    const FN_attrset *attrs = 0)
{
	unsigned context_type = new_addr.get_context_type();

	// Weed out context types that cannot be created
	switch (context_type) {
	case FNSP_organization_context:
	case FNSP_enterprise_context:
		// cannot create NIS+ domain this way
	case FNSP_null_context:
		// no associated physical storage
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	}

	// Obtain reference type of context to be created if not supplied
	if (reftype == 0)
		reftype = FNSP_reftype_from_ctxtype(context_type);
	if (reftype == 0) {
		status = FN_E_UNSPECIFIED_ERROR;
		// ??? bad_reference?
		return (0);
	}

	unsigned string_case = FNSP_Syntax(context_type)->string_case();

	// create directory if requested
	if (dirname) {
		FN_string *ctx_dir = new FN_string(&status,
		    &FNSP_context_directory, dirname, 0);
		if (status != FN_SUCCESS)
			return (0);
		if ((status = FNSP_create_directory(*ctx_dir,
			new_addr.get_access_flags())) != FN_SUCCESS) {
			delete ctx_dir;
			return (0);
		}
		// Create the attribute map for org context
		FN_string *attr_table = new FN_string (&status,
		    &FNSP_org_attr, &dot_string, ctx_dir, 0);
		if (status != FN_SUCCESS) {
			delete ctx_dir;
			return (0);
		}
		if ((status = FNSP_create_attribute_table_base(
		    *attr_table, new_addr.get_access_flags(),
		    FN_STRING_CASE_SENSITIVE)) != FN_SUCCESS) {
			delete ctx_dir;
			delete attr_table;
			return (0);
		}
		delete ctx_dir;
		delete attr_table;
	}

	// Use representation information for creation
	switch (new_addr.get_repr_type()) {
	case FNSP_normal_repr:
		return (FNSP_create_normal_context(new_addr, *reftype,
		    attrs, string_case, status));
	case FNSP_merged_repr:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);

	default:
		status = FN_E_UNSPECIFIED_ERROR;
		// ??? bad_reference?
		return (0);
	}
}

FN_ref *
FNSP_nisplusImpl::create_context(unsigned &status, const FN_string *dirname,
				    const FN_identifier *reftype)
{
	return (FNSP_create_context(*my_address, status, dirname, reftype));
}

static FNSP_Address*
compose_child_addr(const FNSP_Address &parent,
    const FN_string &childname,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int find_legal_childname)
{
	FN_string *iname = 0;
	FNSP_Address* answer = 0;
	char sname[NIS_MAXNAMELEN+1];

	// ignore parent, these are always single table implementations

	if (context_type == FNSP_hostname_context ||
	    context_type == FNSP_username_context) {
		// create FNSP single_table_impl for these
		iname = compose_child_tablename(parent.get_table_name(),
		    childname,
		    status,
		    find_legal_childname);
	} else {
		FN_string realchild = (find_legal_childname?
		    legalize_value(childname) : childname);
		switch (parent.get_impl_type()) {
		case FNSP_shared_table_impl:
			sprintf(sname, "[%s=\"%s\"], %s",
			    FNSP_ctx_col_label, realchild.str(),
			    parent.get_table_name().str());
			iname = new FN_string((unsigned char *)sname);
			break;

		case FNSP_entries_impl:
			// ignore context type, children can only be
			// FNSP_entries_impl
			// cname = parent''s cname + childname
			// iname = [contextname=cname], tablename
			sprintf(sname, "[%s=\"%s%c%s\"], %s",
			    FNSP_ctx_col_label,
			    parent.get_index_name().str(),
			    FNSP_internal_name_separator,
			    realchild.str(),
			    parent.get_table_name().str());
			iname = new FN_string((unsigned char *)sname);
			break;

		default:
			// probably FNSP_shared_impl
			iname = compose_child_tablename(
			    parent.get_table_name(), childname,	status,
			    find_legal_childname);
		}
	}

	if (iname) {
		answer = new FNSP_nisplus_address(*iname, context_type,
		    repr_type, AUTHORITATIVE);
		delete iname;
	}

	return (answer);
}


// Create structures for new context and bind it in the given parent context.
// Policies:
//  1.  Fails if flags indicate bind_exclusive and binding already exists.
//  2.  Fails if binding already exists and is that of a child context.
//  3.  Add reference of new context to binding table, indicating it is a child

FN_ref *
FNSP_nisplusImpl::create_and_bind(
    const FN_string &childname,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int find_legal_tabname,
    const FN_identifier *ref_type,
    const FN_attrset *attrs)
{
	FNSP_binding_type btype;
	unsigned lstatus;
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, childname,
	    lstatus, &btype,
	    MASTER_ONLY);
	status = FN_SUCCESS;  // initialize

	// check for existing binding
	if (ref) {
		status = FN_E_NAME_IN_USE;  // must destroy explicitly
		delete ref;
		return (0);
	}

	// compose internal name of new context
	FNSP_Address *child_addr = compose_child_addr(*my_address, childname,
	    context_type, repr_type,
	    status, find_legal_tabname);

	// create context
	if (child_addr) {
		ref = FNSP_create_context(*child_addr, status, 0, ref_type,
		    attrs);
	}

	// add binding to parent context
	// at this point, always do an 'add-overwrite' to update or add binding
	if (status == FN_SUCCESS) {
		status = FNSP_add_binding_aux(*my_address, childname, *ref,
		    attrs, 0, FNSP_child_context);
		// try to recover
		if (status != FN_SUCCESS) {
			(void) FNSP_destroy_context(*child_addr);
			delete ref;
			ref = 0;
		}
	}
	delete child_addr;
	return (ref);
}

// Destroy structures associated with context and unbind it from parent.
// Policies:
// 1.  Can delete a context, even one that we have not created.
// 2.  Can only delete empty contexts.
// 3.  If context no longer exists but binding does, remove binding.

unsigned
FNSP_nisplusImpl::destroy_and_unbind(const FN_string &childname)
{
	unsigned status;
	FNSP_binding_type btype;
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, childname,
	    status, &btype,
	    MASTER_ONLY);
	if (status == FN_E_NAME_NOT_FOUND)
		return (FN_SUCCESS);
	else if (status != FN_SUCCESS)
		return (status);

	FNSP_nisplus_address child(*ref);
	delete ref;

	if (child.get_context_type() == 0)
		// reference is not one that we can delete
		return (FN_E_OPERATION_NOT_SUPPORTED);

	status = FNSP_destroy_context(child);

	switch (status) {
	case FN_SUCCESS:
		status = FNSP_remove_binding_aux(*my_address, childname);
		break;
	case FN_E_ILLEGAL_NAME:
		// set to different error to avoid confusion
		status = FN_E_MALFORMED_REFERENCE;  // ??? appropriate error?
		break;
	}
	return (status);
}


// Does (normal) context associated with given internal name exists?
// (i.e. does the given internal name has an associated bindings table?)

unsigned
FNSP_context_exists(const FNSP_Address &ctx)
{
	nis_result *res;
	unsigned status;
	nis_name tablename;
	FN_ref *ref;

	switch (ctx.get_impl_type()) {
	case FNSP_directory_impl:
	case FNSP_single_table_impl:
	case FNSP_shared_table_impl:
		tablename = (nis_name)ctx.get_table_name().str();
		res = nis_lookup(tablename,
		    FNSP_nisflags|ctx.get_access_flags());
		status = FNSP_map_result(res, 0);
		free_nis_result(res);
		break;
	case FNSP_entries_impl:
		/* look for 'self' entry that denotes context exists */
		ref = FNSP_lookup_binding_shared(ctx.get_table_name(),
		    ctx.get_index_name(),
		    FNSP_self_name,
		    status, 0, ctx.get_access_flags());
		delete ref;
		break;
	default:
		status = FN_E_MALFORMED_REFERENCE;
	}

	if (status == FN_E_NAME_NOT_FOUND)
		status = FN_E_NOT_A_CONTEXT; // CONTEXT_NOT_FOUND

	return (status);
}

unsigned
FNSP_nisplusImpl::context_exists()
{
	return (FNSP_context_exists(*my_address));
}


/* *************** Dealing with NIS+ organizations ******************* */

// Callback function used for constructing FN_nameset of suborganizations.
// Assumes 'udata' points to the FN_nameset to add to and updates its contents.
// A 'suborganization' is one that has an associated 'org_dir' subdirectory.

static int
add_obj_to_org_nameset(char *, nis_object *ent, void *udata)
{
	FNSP_nameset_cb_t *ld = (FNSP_nameset_cb_t *) udata;
	FN_nameset *ns;
	unsigned status;
	uint32_t entry_type, t;
	char fname [NIS_MAXNAMELEN+1];
	FN_ref *ref;

	ns = ld->nameset;

	memcpy(&t, ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val,
	    sizeof (t));
	entry_type = ntohl(t);

	if (entry_type == NIS_DIRECTORY_OBJ) {
		sprintf(fname, "%s.", ent->zo_name);
		if (*(ent->zo_domain) != '.')
			strcat(fname, ent->zo_domain);
		ref = FNSP_lookup_org((unsigned char *)fname,
				    (unsigned int)ld->flags, /* access */
				    status);
		if (ref) {
			ns->add((unsigned char *)(ent->zo_name));
			delete ref;
		}
	}
	return (0);
}

// Callback function used for constructing FN_bindingset of suborganizations.
// Assumes 'udata' points to the FN_bindingset to add to and
// updates its contents.
// A 'suborganization' is one that has an associated 'org_dir' subdirectory.

typedef struct {
	FN_bindingset *bindset;
	unsigned int flags;
} FNSP_listbinddata_t;


static int
add_obj_to_org_bindingset(char *, nis_object *ent, void *udata)
{
	FN_ref *ref = 0;
	char fname[NIS_MAXNAMELEN+1];
	unsigned status;
	uint32_t entry_type, t;
	FNSP_listbinddata_t *ld = (FNSP_listbinddata_t *) udata;
	FN_bindingset *bs;

	bs = ld->bindset;

	memcpy(&t, ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val,
	    sizeof (t));
	entry_type = ntohl(t);

	if (entry_type == NIS_DIRECTORY_OBJ) {
		sprintf(fname, "%s.", ent->zo_name);
		if (*(ent->zo_domain) != '.')
			strcat(fname, ent->zo_domain);
		ref = FNSP_lookup_org((unsigned char *)fname,
				    ld->flags, status); /* flags */
		if (ref) {
			bs->add((unsigned char *)(ent->zo_name), *ref,
			    FN_OP_EXCLUSIVE);
			delete ref;
		}
	}
	return (0);
}


// Base function used to generate FN_nameset and FN_bindingset of
// suborganizations of given directory identified by 'dirname'.

static unsigned
FNSP_get_orgbindings(const FN_string &dirname, unsigned int access_flags,
    int (*add_func)(char *, nis_object*, void *),
    void *add_params)
{
	nis_result *res = 0;
	unsigned status;
	char sname[NIS_MAXNAMELEN+1];
	nis_name dirname_nn = (nis_name) (dirname.str());

	sprintf(sname, "%s.%s", ORG_DIRECTORY, dirname_nn);

#ifdef CAREFUL_BUT_SLOW
	/* make sure org_dir exists */
	res = nis_lookup(sname, FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);
	if (status != FN_SUCCESS) {
		free_nis_result(tres);
		return (status);
	}

	/* Make sure it is a directory object. */
	if (res->objects.objects_val[0].zo_data.zo_type != NIS_DIRECTORY_OBJ) {
#ifdef DEBUG
		fprintf(stderr, "%s is not a directory!\n", sname);
#endif /* DEBUG */
		free_nis_result(tres);
		return (FN_E_NOT_A_CONTEXT);
	}
	free_nis_result(res);
#endif /* CAREFUL_BUT_SLOW */

	/* Get directory contents of original directory using callback */
	res = __nis_list_localcb(dirname_nn, FNSP_nisflags|access_flags,
	    add_func, add_params);

	if (res->status == NIS_RPCERROR) {
		// may have failed because too big; try using TCP
		free_nis_result(res);
		unsigned long new_flags =
		    access_flags|(FNSP_nisflags&(~USE_DGRAM));
		res = __nis_list_localcb(dirname_nn, new_flags,
		    add_func, add_params);
	}

	if ((res->status == NIS_CBRESULTS) || (res->status == NIS_NOTFOUND))
		status = FN_SUCCESS;
	else
		status = FNSP_map_result(res, 0);

	free_nis_result(res);
	return (status);
}


// Return FN_nameset of suborganizations of given organization (dirname).
// Set 'status' appropriately upon return.

FN_nameset *
FNSP_list_orgnames(const FN_string &dirname,
		    unsigned int access_flags,
		    unsigned &status)
{
	FN_nameset *ns = new FN_nameset;

	if (ns == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	FNSP_nameset_cb_t ld;
	ld.nameset = ns;
	ld.flags = (int) access_flags;

	status = FNSP_get_orgbindings(dirname, access_flags,
				    add_obj_to_org_nameset, (void *)&ld);
	if (status != FN_SUCCESS) {
		delete ns;
		ns = 0;
	}
	return (ns);
}


// Return FN_bindingset of suborganizations of given organization (dirname).
// Set 'status' appropriately upon return.

FN_bindingset *
FNSP_list_orgbindings(const FN_string &dirname,
		    unsigned int access_flags,
		    unsigned &status)
{
	FN_bindingset *bs = new FN_bindingset;
	FNSP_listbinddata_t ld;

	if (bs == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	ld.bindset = bs;
	ld.flags = access_flags;

	status = FNSP_get_orgbindings(dirname, access_flags,
	    add_obj_to_org_bindingset, (void *)&ld);
	if (status != FN_SUCCESS) {
		delete bs;
		bs = 0;
	}
	return (bs);
}


// Determine whether given directory exists and whether it is a domain
// (it is a domain if it has a "org_dir" subdirectory).
// If the name refers to a domain, the FN_ref of the object is returned.

static FN_ref *
FNSP_lookup_org(const FN_string &dirname, unsigned int access_flags,
		unsigned &status)
{
	char org_dir[NIS_MAXNAMELEN+1];
	FN_ref *ref = 0;
	nis_result *res;

	sprintf(org_dir, "%s.%s", ORG_DIRECTORY, dirname.str(&status));

	res = nis_lookup((nis_name)org_dir, FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);

	if (status == FN_E_NOT_A_CONTEXT)
		status = FN_E_NAME_NOT_FOUND; // CONTEXT_NOT_FOUND
	else if (status == FN_SUCCESS)
		ref =  FNSP_reference(FNSP_nisplus_address_type_name(),
		    dirname, FNSP_organization_context);
	free_nis_result(res);
	return (ref);
}

// Find longest component of 'name' that resolves to a valid organization
// relative to 'directory'
// and set 'stat' appropriately.

static int
FNSP_resolve_partial(const FN_string &directory,
    const FN_string &name,
    unsigned int access_flags,
    FN_status &stat)
{
	unsigned status;
	FN_string *rest_name = 0, *old_rest_name = 0;
	FN_string *rightmost_name;
	FN_string *longest_name = 0;
	FN_ref *goodref = 0, *ref;
	int components_added;
	FN_string *old_longest_name = 0;

	rightmost_name = rightmost_dotname(name, &rest_name);

	// Name only had one component and we should have already tried that.
	// We are only interested in partial resolutions
	if (rest_name == 0) {
		if (rightmost_name) delete rightmost_name;
		return (0);
	}

	old_longest_name = (FN_string *)&directory;

	for (components_added = 0; rightmost_name != 0; components_added++) {
		longest_name = compose_dotname(*rightmost_name,
		    *old_longest_name);
		if (longest_name == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			break;
		}
		ref = FNSP_lookup_org(*longest_name, access_flags, status);

		if (status != FN_SUCCESS)
			break;

		// remember good reference (delete old)
		if (goodref)
			delete goodref;
		goodref = ref;

		// remember new longest_name
		if (components_added)
			delete old_longest_name;
		old_longest_name = longest_name;

		// extract next component to be added
		delete rightmost_name;
		if (old_rest_name)
			delete old_rest_name;
		old_rest_name = rest_name;
		rightmost_name = rightmost_dotname(*old_rest_name, &rest_name);
	}

	// unchanged
	if (components_added == 0) {
		if (rightmost_name) delete rightmost_name;
		if (longest_name) delete longest_name;
		return (0);
	}

	// Since the only reason that FNSP_resolve_partial gets called is when
	// the whole name did not work, old_rest_name should
	// always be non-empty.

	if (old_rest_name == 0) {
#ifdef DEBUG
		fprintf(stderr, "Internal error in FNSP_resolve_partial\n");
#endif /* DEBUG */
		return (0);
	}

	FN_composite_name cname(*old_rest_name);
	stat.set(status, goodref, 0, &cname);

	delete rightmost_name;
	delete rest_name;
	delete old_rest_name;
	delete longest_name;
	delete old_longest_name;
	delete goodref;
	return (1);
}


// Resolve given 'name' and return its reference using following rule:
// Resolution policy:
//  1.  Names with no trailing dot are resolved relative to given organization.
//  2.  Names with trailing dot are resolved directly by NIS+.
//
// Set 'status' appropriately.
// If name is not resolved successfully, set 'stat' to contain information
// about any partial resolution results and set 'stat_set' to TRUE.

FN_ref *
FNSP_resolve_orgname(const FN_string &directory,
    const FN_string &name,
    unsigned int access_flags,
    unsigned &status,
    FN_status &stat,
    int &stat_set)
{
	FN_ref *answer = 0;
	FN_string *target;

	stat_set = 0;

	// Sanity check:  directory must be fully qualfied (internal error?)

	if (!fully_qualified_dotname_p(directory)) {
		status = FN_E_NAME_NOT_FOUND;
		return (0);
	}

	if (fully_qualified_dotname_p(name)) {
		// name fully qualified; resolve directly it in NIS+
		answer = FNSP_lookup_org(name, access_flags, status);
	} else {
		// resolve relative to current directory
		target = compose_dotname(name, directory);
		if (target) {
			answer = FNSP_lookup_org(*target, access_flags, status);
			delete target;
			// try to figure out where failure occurred
			switch (status) {
			case FN_E_NAME_NOT_FOUND:
			case FN_E_CTX_NO_PERMISSION:
				stat_set = FNSP_resolve_partial(directory,
				    name, access_flags, stat);
			}
		} else {
			status = FN_E_INSUFFICIENT_RESOURCES;
		}
	}

	return (answer);
}

// Return rightmost substring of dirname appearing after occurrence of 'term'.

static FN_string *
FNSP_orgname_of_aux(const FN_string &dirname, unsigned &status,
    const FN_string &term, int t_size)
{
	int t_start = dirname.prev_substring(term,
	    FN_STRING_INDEX_LAST,
	    FN_STRING_CASE_SENSITIVE,
	    &status);
	if (status != FN_SUCCESS)
		return (0);

	if (t_start == FN_STRING_INDEX_NONE) {
		status = FN_E_NAME_NOT_FOUND;
		return (0);
	}

	FN_string *answer;
	answer = new FN_string(dirname, t_start+t_size, dirname.bytecount()-1);
	if (answer == 0)
		status = FN_E_INSUFFICIENT_RESOURCES;
	return (answer);
}


// Find organization name associated with given directory.
// This is the string that is to the right of the "ctx_dir" or "org_dir".

FN_string *
FNSP_orgname_of(const FN_string &dirname, unsigned &status, int org)
{
	if (org)
		return FNSP_orgname_of_aux(dirname, status,
		    FNSP_org_directory, FNSP_od_size);
	else
		return FNSP_orgname_of_aux(dirname, status,
		    FNSP_context_directory, FNSP_cd_size);
}



// given (tname, dname) -> fns_<tname>.ctx_dir.<dname>
FN_string *
FNSP_compose_ctx_tablename(const FN_string &short_tabname,
    const FN_string &domain)
{
	FN_string *answer = new FN_string(0,
	    &FNSP_prefix,
	    &short_tabname,
	    &dot_string,
	    &FNSP_context_directory,
	    &domain,
	    0);
	return (answer);
}

extern char *
FNSP_process_iter(nis_result *res, netobj &iter_pos,
    unsigned int &status, FN_ref **ref, int column = FNSP_NAME_COL)
{
	char *name = 0;
	status = FNSP_map_status(res->status, 0);

	if (status == FN_SUCCESS) {
		name = strdup(
			ENTRY_VAL(res->objects.objects_val, column));
		if (name == 0) {
			iter_pos.n_len = 0;
			iter_pos.n_bytes = 0;
			if (ref)
				*ref = 0;
			status = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
		if (ref) {
			*ref = FN_ref_xdr_deserialize(
			    ENTRY_VAL(res->objects.objects_val, FNSP_REF_COL),
			    ENTRY_LEN(res->objects.objects_val, FNSP_REF_COL),
			    status);
			if (status != FN_SUCCESS) {
				iter_pos.n_len = 0;
				iter_pos.n_bytes = 0;
				free(name);
				return (0);
			}
		}
		// copy cookie
		iter_pos.n_len = res->cookie.n_len;
		if (iter_pos.n_len) {
			iter_pos.n_bytes = (char *)malloc(iter_pos.n_len);
			if (iter_pos.n_bytes == 0) {
				iter_pos.n_len = 0;
				free(name);
				if (ref) {
					delete *ref;
					*ref  = 0;
				}
				status = FN_E_INSUFFICIENT_RESOURCES;
			} else {
				memcpy(iter_pos.n_bytes, res->cookie.n_bytes,
				    iter_pos.n_len);
			}
		} else
			iter_pos.n_bytes = 0;
	} else {
		iter_pos.n_len = 0;
		iter_pos.n_bytes = 0;
		if (ref)
			*ref = 0;
		if (status == FN_E_NAME_NOT_FOUND) {
			// encountered end-of-table
			// null name will indicate no more
			status = FN_SUCCESS;
		}
	}
	free_nis_result(res);
	return (name);
}

char *
FNSP_read_first(const char *tab_name, netobj &iter_pos,
    unsigned int &status, FN_ref **ref)
{
	nis_result *res = nis_first_entry((nis_name)tab_name);

	return (FNSP_process_iter(res, iter_pos, status, ref));
}

char *
FNSP_read_next(const char *tab_name, netobj &iter_pos,
    unsigned int &status, FN_ref **ref)
{
	nis_result *res = nis_next_entry((nis_name)tab_name, &iter_pos);

	/* free old cookie */
	free(iter_pos.n_bytes);

	return (FNSP_process_iter(res, iter_pos, status, ref));
}

FN_attrset *
get_create_params_from_attrs(const FN_attrset *attrs,
    unsigned int &context_type,
    unsigned int &repr_type,
    FN_identifier **ref_type,
    unsigned int default_context_type)
{
	// default settings
	context_type = default_context_type;
	repr_type = FNSP_normal_repr;
	if (ref_type != NULL)
		*ref_type = NULL;

	if (attrs == NULL || attrs->count() == 0) {
		return (NULL);
	}

	FN_attrset *attr_copy = new FN_attrset (*attrs);
	if (attr_copy == NULL)
		return (NULL);

	static FN_identifier
		CTXTYPE_ATTR_ID((const unsigned char *)"fn_context_type");
	static FN_identifier
		REFTYPE_ATTR_ID((const unsigned char *)"fn_reference_type");

	// Extract context type information
	const FN_attribute *attr = attrs->get(CTXTYPE_ATTR_ID);
	if (attr) {
		void *ip;
		const FN_attrvalue *ctx_val = attr->first(ip);
		if (ctx_val) {
			unsigned int *cval =
			    (unsigned int *)(ctx_val->contents());
			context_type = *cval;
		}
		attr_copy->remove(CTXTYPE_ATTR_ID);
	}

	// Extract reference type information
	attr = attrs->get(REFTYPE_ATTR_ID);
	if (attr) {
		attr_copy->remove(REFTYPE_ATTR_ID);

		if (ref_type == NULL)
			return (attr_copy);

		void *ip;
		const FN_attrvalue *ref_val = attr->first(ip);

		if (ref_val == NULL)
			return (attr_copy);

		size_t len = ref_val->length();
		char *buf = (char *)malloc(len);

		memcpy(buf, ref_val->contents(), len);

		FN_identifier_t *reftype_val = (FN_identifier_t *)buf;
		reftype_val->contents = buf + sizeof (FN_identifier_t);

		*ref_type = new FN_identifier(*reftype_val);
		free(buf);
	}

	return (attr_copy);
}

FN_string *
FNSP_nisplusImpl::get_nns_objname(const FN_string *dirname)
{
	return (FNSP_compose_ctx_tablename((unsigned char *)"", *dirname));
}

FN_ref *
FNSP_nisplusImpl::get_nns_ref()
{
	return (FNSP_reference(FNSP_nisplus_address_type_name(),
	    my_address->get_internal_name(), FNSP_nsid_context));
}

FNSP_nisplusImpl::FNSP_nisplusImpl(FNSP_Address *addr) : FNSP_Impl(addr)
{
}

FNSP_nisplusImpl::~FNSP_nisplusImpl() {
}
