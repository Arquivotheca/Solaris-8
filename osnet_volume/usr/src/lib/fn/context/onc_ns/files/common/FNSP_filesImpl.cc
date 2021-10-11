/*
 * Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesImpl.cc	1.40	97/11/10 SMI"

#include <sys/time.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <ndbm.h>

#include <xfn/xfn.hh>
#include <FNSP_Syntax.hh>
#include <fnsp_utils.hh>

#include "FNSP_filesImpl.hh"
#include "fnsp_files_internal.hh"
#include <fnsp_internal_common.hh>

// Address type idenitifier for /var files
static const FN_identifier
FNSP_files_address_type((unsigned char *) "onc_fn_files");
static const FN_identifier
FNSP_printer_files_address_type((unsigned char *) "onc_fn_printer_files");

static const FN_string FNSP_self = (unsigned char *) FNSP_SELF_STR;
static const FN_string FNSP_nns_name((unsigned char *) "_fns_nns_");
static const char *FNSP_files_map_dir = FNSP_FILES_MAP_DIR;
static const char *FNSP_user_map = FNSP_USER_MAP_PRE;
static const char *FNSP_map_suffix = FNSP_MAP_SUFFIX;

// Index names, seperators and suffixes for /var files maps
static const char *FNSP_attr_suffix = FNSP_ATTR_SUFFIX;
static const char *FNSP_bind_suffix = FNSP_BIND_SUFFIX;
static const char *FNSP_subctx_suffix = FNSP_SUBCTX_SUFFIX;
// static const char *FNSP_sub_context_sept = "_#";
static const char *FNSP_internal_name_seperator = FNSP_INTERNAL_NAME_SEP;

static const char *FNSP_nis_reference = FNSP_NIS_REFERENCE;
static const char *FNSP_nis_context = FNSP_NIS_CONTEXT;
static const char *FNSP_nis_hu_reference = FNSP_NIS_HU_REFERENCE;
static const char *FNSP_nis_hu_context = FNSP_NIS_HU_CONTEXT;

static const char *FNSP_org_map = FNSP_ORG_MAP;
static const char *FNSP_user_ctx_map = FNSP_USER_CTX_MAP;
static const char *FNSP_user_attr_map = FNSP_USER_ATTR_MAP;
static const char *FNSP_host_ctx_map = FNSP_HOST_CTX_MAP;
static const char *FNSP_host_attr_map = FNSP_HOST_ATTR_MAP;
static const char *FNSP_thisorgunit_attr_map = FNSP_THISORGUNIT_ATTR_MAP;

static FN_string *
FNSP_files_get_homedir(const FNSP_Address &,
    const FN_string &username,
    unsigned &status);

const FN_identifier &
FNSP_files_address_type_name(void)
{
	return (FNSP_files_address_type);
}

const FN_identifier &
FNSP_printer_files_address_type_name(void)
{
	return (FNSP_printer_files_address_type);
}

// --------------------------------------------
//   Routine used by the organization context
//   to get *thisorgunit* reference.
// ---------------------------------------------
FN_string *
FNSP_files_get_org_nns_objname(const FN_string *)
{
	FN_string *nnsobjname;
	char buf[FNS_FILES_INDEX];
	strcpy(buf, "[");
	strcat(buf, (char *) FNSP_self.str());
	strcat(buf, "]");
	strcat(buf, FNSP_org_map);
	nnsobjname = new FN_string((unsigned char *) buf);
	return (nnsobjname);
}

FN_string *
FNSP_filesImpl::get_nns_objname(const FN_string *)
{
	return (FNSP_files_get_org_nns_objname(0));
}


// -----------------------------------------
// FNSP_filesImpl
// -----------------------------------------
FNSP_filesImpl::FNSP_filesImpl(FNSP_nisAddress *addr) : FNSP_Impl(addr)
{
}

FNSP_filesImpl::~FNSP_filesImpl()
{
}

int
FNSP_filesImpl::is_this_address_type_p(const FN_ref_addr &addr)
{
	return (((*addr.type()) == FNSP_files_address_type) ||
	    ((*addr.type()) == FNSP_printer_files_address_type));
}

static FN_ref *
FNSP_lookup_binding_aux(const FNSP_Address &parent,
    const FN_string &bname,
    unsigned &status)
{
	FN_ref *ref;

	// Construct local name of bname
	char aname[FNS_FILES_INDEX];
	strcpy(aname, (char *) bname.str());
	unsigned context_type = parent.get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), aname);
	FNSP_legalize_name(aname);

	// For map index
	char map_index[FNS_FILES_INDEX];

	switch (parent.get_impl_type()) {
	case FNSP_directory_impl:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	case FNSP_single_table_impl:
		strcpy(map_index, aname);
		strcat(map_index, FNSP_bind_suffix);
		break;
	case FNSP_shared_table_impl:
		strcpy(map_index, (char *) parent.get_index_name().str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, aname);
		strcat(map_index, FNSP_bind_suffix);
		break;
	default:
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}


	// Map name
	const FN_string map = parent.get_table_name();

	// Lookup for binding
	char *mapentry;
	int maplen;
	status = FNSP_files_lookup((char *) map.str(),
	    map_index, strlen(map_index), &mapentry, &maplen);
	if (status != FN_SUCCESS)
		return (0);

	// Construct the reference, based on impl type
	FNSP_binding_type btype;
	ref = FNSP_nis_binding_deserialize(mapentry, maplen,
	    btype, status);
	free(mapentry);

	return (ref);
}

FN_ref *
FNSP_filesImpl::lookup_binding(const FN_string &bname, unsigned &status)
{
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, bname, status);
	if (ref == 0) {
		return (0);
	}
	FNSP_process_user_fs(*my_address, *ref, FNSP_files_get_homedir,
	    status);
	if (status != FN_SUCCESS) {
		delete ref;
		ref = 0;
	}
	return (ref);
}

// ---------------------------------------------------
// Routine to read single table implementation maps,
// ie., fns_user.ctx and fns_host.ctx
// ---------------------------------------------------
class FN_files_namelist_hu : public FN_namelist {
	char mapfile[FNS_FILES_SIZE];
	int no_more_maps;
	DBM *db;
	char *next_in_line;
	char *next_ref;
	unsigned int next_status;
	int name_from_namelist(datum &, datum &);

	// Varibales for HUGE contexts
	char *parent_index;
	int name_from_namelist_and_parent_index(datum &, datum &);
public:
	FN_files_namelist_hu(const char *table_name,
	    const char *p_index = 0);
	virtual ~FN_files_namelist_hu();
	FN_string *next(FN_status &);
	FN_string *next_with_ref(FN_status &, FN_ref ** = 0);
};

FN_files_namelist_hu::~FN_files_namelist_hu()
{
	if (next_in_line)
		free(next_in_line);
	if (next_ref)
		free(next_ref);
	if (db)
		dbm_close(db);
	if (parent_index)
		free (parent_index);
}

int
FN_files_namelist_hu::name_from_namelist_and_parent_index(
    datum &dbm_key, datum &dbm_value)
{
	char *key = (char *) calloc(dbm_key.dsize+1, sizeof(char));
	strncpy(key, dbm_key.dptr, dbm_key.dsize);
	next_in_line = FNSP_check_if_subcontext(parent_index, key);
	free (key);
	if (!next_in_line)
		return (0);
	// Copy the reference
	next_ref = (char *) malloc(dbm_value.dsize + 1);
	strncpy(next_ref, dbm_value.dptr, dbm_value.dsize);
	next_ref[dbm_value.dsize] = '\0';
	return (1);
}

int FN_files_namelist_hu::name_from_namelist(datum &dbm_key,
    datum &dbm_value)
{
	if (parent_index)
		return (name_from_namelist_and_parent_index(
		    dbm_key, dbm_value));

	if ((strncmp(dbm_value.dptr, FNSP_nis_hu_reference,
	    strlen(FNSP_nis_hu_reference)) == 0) ||
	    (strncmp(dbm_value.dptr, FNSP_nis_hu_context,
	    strlen(FNSP_nis_hu_context)) == 0)) {
		// Found an entry
		next_in_line = (char *) calloc(dbm_key.dsize + 1,
		    sizeof(char));
		strncpy(next_in_line, dbm_key.dptr,
		    (dbm_key.dsize - strlen(FNSP_bind_suffix)));
		// Copy the reference
		next_ref = (char *) malloc(dbm_value.dsize + 1);
		strncpy(next_ref, dbm_value.dptr, dbm_value.dsize);
		next_ref[dbm_value.dsize] = '\0';
		return (1);
	} else
		return (0);
}

FN_files_namelist_hu::FN_files_namelist_hu(const char *table_name,
    const char *p_index)
{
	datum dbm_key, dbm_value;
	struct stat buffer;

	next_in_line = 0;
	next_ref = 0;
	no_more_maps = 0;	
	next_status = FN_SUCCESS;
	strcpy(mapfile, table_name);
	if (p_index)
		parent_index = strdup(p_index);
	else
		parent_index = 0;
	if ((db = dbm_open(mapfile, O_RDONLY, 0444)) == 0) {
		char pag_mapfile[FNS_FILES_SIZE];
		strcpy(pag_mapfile, mapfile);
		strcat(pag_mapfile, ".pag");
		if (stat(pag_mapfile, &buffer) == 0) {
			next_status = FN_E_INSUFFICIENT_RESOURCES;
		}
	} else {
		dbm_key = dbm_firstkey(db);
		while (dbm_key.dptr != NULL) {
			dbm_value = dbm_fetch(db, dbm_key);
			if (name_from_namelist(dbm_key, dbm_value))
				break;
			else
				dbm_key = dbm_nextkey(db);
		}
	}
}

FN_string *
FN_files_namelist_hu::next(FN_status &status)
{
	return (next_with_ref(status));
}

extern unsigned
FNSP_files_compose_next_map_name(char *map);

FN_string *
FN_files_namelist_hu::next_with_ref(FN_status &status, FN_ref **ref)
{
	if (next_status != FN_SUCCESS) {
		status.set(next_status);
		return (0);
	}

	if ((next_in_line == 0) || (next_ref == 0)) {
		next_status = FN_E_INVALID_ENUM_HANDLE;
		status.set_success();
		return (0);
	}

	// User and host name must be normalized
	FNSP_normalize_name(next_in_line);
	FN_string *name = new
	    FN_string((const unsigned char *) next_in_line);
	free(next_in_line);

	// Get the reference, if required
	unsigned s = FN_SUCCESS;
	if (ref != 0) {
		FNSP_binding_type b;
		(*ref) = FNSP_nis_binding_deserialize(next_ref,
		    strlen(next_ref), b, s);
	}
	free(next_ref);

	if ((name == 0) || (s != FN_SUCCESS)) {
		next_status = FN_E_INSUFFICIENT_RESOURCES;
		status.set(next_status);
		return (0);
	}

	// Get the next pointer
	datum dbm_key, dbm_value;
	dbm_key = dbm_nextkey(db);
	while ((dbm_key.dptr != NULL) || (no_more_maps == 0)) {
		if (dbm_key.dptr == NULL) {
			// Try to dbm_open the next file
			FNSP_files_compose_next_map_name(mapfile);
			dbm_close(db);
			if ((db = dbm_open(
			    mapfile, O_RDONLY, 0444)) == 0) {
				no_more_maps  = 1;
				break;
			}
			dbm_key = dbm_firstkey(db);
			continue;
		}
		dbm_value = dbm_fetch(db, dbm_key);
		if (name_from_namelist(dbm_key, dbm_value))
			break;
		else
			dbm_key = dbm_nextkey(db);
	}
	if (dbm_key.dptr == NULL) {
		next_in_line = 0;
		next_ref = 0;
	}

	return (name);
}

static FN_namelist *
FNSP_namelist_from_map(const FN_string &map, unsigned &status)
{
	char mapfile[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, (char *) map.str());

	FN_files_namelist_hu *ns = new FN_files_namelist_hu(mapfile);
	if (ns == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	} else
		status = FN_SUCCESS;

	return (ns);
}

FN_namelist *
FNSP_filesImpl::list_names_hu(unsigned &status)
{
	if (my_address->get_impl_type() !=
	    FNSP_single_table_impl)
		return (0);
	const FN_string map = my_address->get_table_name();
	return (FNSP_namelist_from_map(map, status));
}


FN_nameset *
FNSP_filesImpl::list_nameset(unsigned &status, int /* children_only */)
{
	FN_nameset *ns = 0;
	const FN_string map = my_address->get_table_name();
	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		// Only organizations have direcotory implementation
		// Since no sub-organizations in NIS, return empty
		// name set
		if ((ns = new FN_nameset) != 0)
			status = FN_SUCCESS;
		else
			status = FN_E_INSUFFICIENT_RESOURCES;
		return (ns);
	case FNSP_shared_table_impl:
		break;
	case FNSP_single_table_impl:
	default:
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	// Get map index
	char map_index[FNS_FILES_INDEX];
	strcpy(map_index, (char *) my_address->get_index_name().str());
	strcat(map_index, FNSP_subctx_suffix);

	char *mapentry;
	int maplen;
	status = FNSP_files_lookup((char *) map.str(), map_index,
	    strlen(map_index), &mapentry, &maplen);

	if (status == FN_SUCCESS) {
		ns = FNSP_nis_sub_context_deserialize(mapentry, status);
		// remove lowercase version of nns name
		ns->remove(FNSP_nns_name);
		free(mapentry);
	}

	return (ns);
}

FN_namelist *
FNSP_filesImpl::list_names(unsigned &status, int /* children only */)
{
	FN_nameset *ns = list_nameset(status);
	if ((!ns) || (status != FN_SUCCESS)) {
		delete ns;
		return (0);
	}

	// Check if the subcontext is huge
	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		char mapfile[FNS_FILES_INDEX];
		strcpy(mapfile, FNSP_files_map_dir);
		strcat(mapfile, "/");
		strcat(mapfile, (char *) my_address->get_table_name().str());
		return (new FN_files_namelist_hu(mapfile,
		    (char *) my_address->get_index_name().str()));
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);
	return (new FN_namelist_svc(ns));
}

// -----------------------------------------------
// Routine to check the existance of a context.
// This is performed by doing a lookup and
// checking for the special *context* string.
// -----------------------------------------------
unsigned
FNSP_filesImpl::context_exists()
{
	unsigned status;
	char map[FNS_FILES_INDEX];

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		// No sub-organization context can be checked
		return (FN_SUCCESS);
	case FNSP_single_table_impl:
		strcpy(map, FNSP_org_map);
		break;
	case FNSP_shared_table_impl:
		strcpy(map, (char *) my_address->get_table_name().str());
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	// In the case of user context the map is fns_user.ctx
	if (my_address->get_context_type() == FNSP_user_context)
		strcpy(map, FNSP_user_ctx_map);

	// Get map index
	char map_index[FNS_FILES_INDEX];
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_bind_suffix);

	/* Perform a dbm lookup */
	char *mapentry;
	int maplen;
	status = FNSP_files_lookup(map, map_index, strlen(map_index),
	    &mapentry, &maplen);

	if (status != FN_SUCCESS)
		if (status == FN_E_NAME_NOT_FOUND)
			return (FN_E_NOT_A_CONTEXT);
		else
			return (status);

	if ((strncmp(mapentry, FNSP_nis_context,
	    strlen(FNSP_nis_context)) == 0) ||
	    (strncmp(mapentry, FNSP_nis_hu_context,
	    strlen(FNSP_nis_hu_context)) == 0))
		status = FN_SUCCESS;
	else
		status = FN_E_NOT_A_CONTEXT;
	free(mapentry);
	return (status);
}

// ----------------------------------------------
// List bindings -- calls list_names() to get names
// and reference for each bindings
// ----------------------------------------------
class FN_files_bindinglist_hu : public FN_bindinglist {
	FN_files_namelist_hu *namelist;
public:
	FN_files_bindinglist_hu(const char *mapname,
	    const char *pindex = 0);
	virtual ~FN_files_bindinglist_hu();

	FN_string *next(FN_ref **, FN_status &);
};

FN_files_bindinglist_hu::FN_files_bindinglist_hu(const char *mapname,
    const char *pindex)
{
	namelist = new FN_files_namelist_hu(mapname, pindex);
}

FN_files_bindinglist_hu::~FN_files_bindinglist_hu()
{
	delete namelist;
}

FN_string *
FN_files_bindinglist_hu::next(FN_ref **ref, FN_status &in_status)
{
	// Get the name and reference
	FN_string *name = namelist->next_with_ref(in_status, ref);
	return (name);
}

static FN_bindinglist *
FNSP_bindinglist_from_map(const FN_string &map, unsigned &status)
{
	char mapfile[FNS_FILES_INDEX];
	strcpy(mapfile, FNSP_files_map_dir);
	strcat(mapfile, "/");
	strcat(mapfile, (char *) map.str());

	FN_files_bindinglist_hu *ns = new
	    FN_files_bindinglist_hu(mapfile);
	if (ns == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	} else
		status = FN_SUCCESS;

	return (ns);
}

FN_bindinglist *
FNSP_filesImpl::list_bindings_hu(unsigned &status)
{
	if (my_address->get_impl_type() != FNSP_single_table_impl)
		return (0);

	const FN_string map = my_address->get_table_name();
	return (FNSP_bindinglist_from_map(map, status));
}

// ------------------------------------------------
// List bindings -- calls list_names()
// and then calls lookup_binding() for each name
// ------------------------------------------------
FN_bindinglist *
FNSP_filesImpl::list_bindings(unsigned &status)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	FN_nameset *ns = list_nameset(status);
	if (!ns || (status != FN_SUCCESS))
	    return (0);

	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		char mapfile[FNS_FILES_INDEX];
		strcpy(mapfile, FNSP_files_map_dir);
		strcat(mapfile, "/");
		strcat(mapfile, (char *) my_address->get_table_name().str());
		delete ns;
		return (new FN_files_bindinglist_hu(mapfile,
		    (char *) my_address->get_index_name().str()));
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	FN_bindingset *bs = new FN_bindingset;
	FN_ref *ref;
	void *ip;
	for (const FN_string *aname = ns->first(ip);
	    aname; aname = ns->next(ip)) {
		ref = lookup_binding((*aname), status);
		if (status != FN_SUCCESS)
			return (new FN_bindinglist_svc(bs));
		bs->add(*aname, *ref);
		delete ref;
	}
	delete ns;
	return (new FN_bindinglist_svc(bs));
}

// ---------------------------------------------
// Adding and deleting sub-contexts ie., lists
// in a context.
// ---------------------------------------------
unsigned
FNSP_filesImpl::add_sub_context_entry(const FN_string &child)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		return (FN_SUCCESS);

	// Construct local name
	char aname[FNS_FILES_INDEX];
	strcpy(aname, (char *) child.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), aname);
	const FN_string child_name((unsigned char *) aname);

	FN_nameset *ns;
	unsigned status;

	// Add subcontext name to its list of sub-contexts
	// First obtain the listing
	ns = list_nameset(status, 0);
	if (status == FN_E_NAME_NOT_FOUND) {
		ns = new FN_nameset;
		status = FN_SUCCESS;
	} else if (status != FN_SUCCESS) {
		return (status);
	}

	// Check if the context is HUGE
	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		return (FN_SUCCESS);
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	// Add child_name to the name set
	ns->add(child_name);

	// Serialize the name set and place it back in the dbm files
	char *data = FNSP_nis_sub_context_serialize(ns, status);
	delete ns;
	if (status != FN_SUCCESS)
		return (status);

	// If subcontext entry is huge, delete it
	if (strlen(data) > FNSP_HUGE_SUBCONTEXT_SIZE) {
		ns = new FN_nameset;
		ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT);
		free (data);
		data = FNSP_nis_sub_context_serialize(ns, status);
		delete ns;
	}

	// Get the map name, and index
	const FN_string map = my_address->get_table_name();
	char map_index[FNS_FILES_INDEX];
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_subctx_suffix);

	status = FNSP_files_update_map((char *) map.str(), map_index,
	    data, FNSP_map_store);
	free(data);
	return (status);
}

unsigned
FNSP_filesImpl::delete_sub_context_entry(const FN_string &child)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		return (FN_SUCCESS);

	// Construct local name
	char aname[FNS_FILES_INDEX];
	strcpy(aname, (char *) child.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), aname);
	const FN_string child_name((unsigned char *) aname);

	FN_nameset *ns;
	unsigned status;

	// Add subcontext name to its list of sub-contexts
	// First obtain the listing
	ns = list_nameset(status, 0);
	if (status == FN_E_NAME_NOT_FOUND) {
		return (FN_SUCCESS);
	} else if (status != FN_SUCCESS) {
		return (status);
	}
	if (ns == NULL)
		ns = new FN_nameset;

	// Check if the context is huge
	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		return (FN_SUCCESS);
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	// Delete child_name from the name set
	ns->remove(child_name);

	// Serialize the name set and place it back in the YP-map
	char *data = FNSP_nis_sub_context_serialize(ns, status);
	delete ns;
	if (status != FN_SUCCESS) {
		return (status);
	}

	// Get the map name, and index
	const FN_string index = my_address->get_index_name();

	char map_index[FNS_FILES_INDEX];
	strcpy(map_index, (char *) index.str());
	strcat(map_index, FNSP_subctx_suffix);
	const FN_string map = my_address->get_table_name();

	status = FNSP_files_update_map((char *) map.str(), map_index,
	    data, FNSP_map_store);
	free(data);
	return (status);
}

unsigned
FNSP_filesImpl::add_binding_without_attrs(const FN_string &aname,
    const FN_ref &ref, unsigned flags)
{
	unsigned status;
	FNSP_map_operation op;

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_OPERATION_NOT_SUPPORTED);
	case FNSP_single_table_impl:
	case FNSP_shared_table_impl:
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Check if parent is a context
	if ((status = context_exists()) != FN_SUCCESS)
		return (status);

	// Construct local name
	char atomic_name[FNS_FILES_INDEX];
	strcpy(atomic_name, (char *) aname.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), atomic_name);
	FNSP_legalize_name(atomic_name);

	// Add binding information, Serialize reference
	// and obtain the child's index name
	char *refbuf;
	char map_index[FNS_FILES_INDEX];
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		refbuf = FNSP_nis_binding_serialize(ref,
		    FNSP_hu_reference, status);
		strcpy(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
	} else {
		refbuf = FNSP_nis_binding_serialize(ref,
		    FNSP_bound_reference, status);
		strcpy(map_index, (char *) my_address->get_index_name().str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
	}
	if (status != FN_SUCCESS) {
		return (status);
	}

	// Get map name from parent address
	const FN_string map = my_address->get_table_name();
	if (flags == FN_OP_EXCLUSIVE)
		op = FNSP_map_insert;
	else
		op = FNSP_map_store;

	// Lookup to check if the bindings exists
	char *old_mapentry;
	int old_maplen;
	status = FNSP_files_lookup((char *) map.str(), map_index,
	    strlen(map_index), &old_mapentry, &old_maplen);
	if (status == FN_SUCCESS) {
		// Check if old_ref is a real context
		if ((strncmp(old_mapentry, FNSP_nis_context,
		    strlen(FNSP_nis_context)) == 0) ||
		    (strncmp(old_mapentry, FNSP_nis_hu_context,
		    strlen(FNSP_nis_hu_context)) == 0)) {
			// Check if old address is present
			FNSP_binding_type obtype;
			FN_ref *old_ref = FNSP_nis_binding_deserialize(
			    old_mapentry, old_maplen, obtype, status);
			if (status != FN_SUCCESS) {
				free(old_mapentry);
				free(refbuf);
				return (status);
			}
			if (check_if_old_addr_present(*old_ref, ref)
			    == 0) {
				free(old_mapentry);
				free(refbuf);
				return (FN_E_NAME_IN_USE);
			}
			// Copy the context type
			strncpy(refbuf, old_mapentry,
			    strlen(FNSP_NIS_CONTEXT));
		}
		free(old_mapentry);
	}

	// Update map with binding information
	status = FNSP_files_update_map((char *) map.str(), map_index,
	    refbuf, op);
	if ((status == FN_SUCCESS) &&
	    (my_address->get_impl_type() != FNSP_single_table_impl)) {
		// Add the child name to the sub-context name list
		status = add_sub_context_entry(aname);
		if (status != FN_SUCCESS)
			FNSP_files_update_map((char *) map.str(), map_index,
			    refbuf, FNSP_map_delete);
	}
	free(refbuf);
	return (status);
}

unsigned
FNSP_filesImpl::remove_binding(const FN_string &aname)
{
	// For map index of the child
	char map_index[FNS_FILES_INDEX];

	// Construct local name
	char atomic_name[FNS_FILES_INDEX];
	strcpy(atomic_name, (char *) aname.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), atomic_name);
	FNSP_legalize_name(atomic_name);

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_OPERATION_NOT_SUPPORTED);
	case FNSP_single_table_impl:
		strcpy(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
		break;
	case FNSP_shared_table_impl:
		strcpy(map_index,
		    (char *) (my_address->get_index_name()).str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Get table name
	const FN_string map = my_address->get_table_name();

	// Lookup at the bindings and check
	int maplen;
	char *mapentry = 0;
	unsigned status = FNSP_files_lookup((char *) map.str(), map_index,
	    strlen(map_index), &mapentry, &maplen);
	if (status != FN_SUCCESS) {
		if (status == FN_E_NAME_NOT_FOUND) {
			// In case the sub-context entry exists
			if (my_address->get_impl_type() !=
			    FNSP_single_table_impl)
				delete_sub_context_entry(aname);
			// Delete the attributes, if any
			delete_attrset(aname);
			return (FN_SUCCESS);
		} else
			return (status);
	}

	// Check for *bound reference*
	if ((strncmp(mapentry, FNSP_nis_reference,
	    strlen(FNSP_nis_reference)) == 0) ||
	    (strncmp(mapentry, FNSP_nis_hu_reference,
	    strlen(FNSP_nis_hu_reference)) == 0))
		status = FNSP_files_update_map((char *) map.str(),
		    map_index, mapentry, FNSP_map_delete);
	else
		status = FN_E_NAME_IN_USE;
	free(mapentry);

	// If the name is a context
	if (status != FN_SUCCESS)
		return (status);

	// Remove from the parent's sub-context listing
	if (my_address->get_impl_type() != FNSP_single_table_impl)
		status = delete_sub_context_entry(aname);
	// Delete attributes, if any
	delete_attrset(aname);
	return (status);
}

unsigned
FNSP_filesImpl::add_binding(const FN_string &aname,
    const FN_ref &ref, const FN_attrset *attrs, unsigned exclusive)
{
	unsigned status = add_binding_without_attrs(aname,
	    ref, exclusive);

	if ((status == FN_SUCCESS) && (attrs) &&
	    (attrs->count() > 0)) {
		status = set_attrset(aname, *attrs);
		if (status != FN_SUCCESS)
			remove_binding(aname);
	}
	return (status);
}

unsigned
FNSP_filesImpl::rename_binding(const FN_string &atomic_name,
    const FN_string &new_name,
    unsigned flags)
{
	unsigned status;

	// Lookup binding
	FN_ref *ref =
	    FNSP_lookup_binding_aux(*my_address, atomic_name, status);
	if (status != FN_SUCCESS)
		return (status);

	// Make sure it is not a context
	FNSP_nisAddress childaddr(*ref);
	if (childaddr.get_context_type() != 0)  {
		delete ref;
		return (FN_E_OPERATION_NOT_SUPPORTED);
	}

	// Get the attributes with the atomic name
	FN_attrset *attrset = get_attrset(atomic_name, status);
	if ((status != FN_SUCCESS) &&
	    (status != FN_E_NO_SUCH_ATTRIBUTE)) {
		delete ref;
		return (status);
	}

	// Add new bindings with attributes
	status = add_binding(new_name, *ref, attrset, flags);
	if (status != FN_SUCCESS) {
		delete ref;
		delete attrset;
		return (status);
	}

	// Delete binding and attributes
	status = remove_binding(atomic_name);
	if ((status == FN_SUCCESS) && (attrset))
		delete_attrset(atomic_name);
	delete ref;
	delete attrset;
	return (status);
}

// ----------------------------------------------------
// Function to compose address of the child name.
// The arguments are parent address, child_name etc.,
// With atomic child_name, the index-ed child name
// must be determined, and the address composed
// from it.
// ----------------------------------------------------
static FNSP_nisAddress *
compose_child_addr(const FNSP_Address &parent,
    const FN_string &child,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int /* find_legal_name */)
{
	FN_string *child_internal_name;
	char buf[FNS_FILES_INDEX];
	const FN_string parent_index = parent.get_index_name();
	const FN_string parent_table = parent.get_table_name();

	// Construct local name
	char achild[FNS_FILES_INDEX];
	strcpy(achild, (char *) child.str());
	unsigned pcontext_type = parent.get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(pcontext_type)->string_case(), achild);
	FNSP_legalize_name(achild);
	const FN_string child_name((unsigned char *) achild);

	// Special case for FNSP_username_context and
	// FNSP_hostname_context, FNSP_user_context and FNSP_host_context
	switch (context_type) {
	case FNSP_username_context:
		sprintf(buf, "[%s%s%s]%s",
		    parent_index.str(),
		    FNSP_internal_name_seperator,
		    child_name.str(),
		    FNSP_user_ctx_map);
		break;
	case FNSP_hostname_context:
		sprintf(buf, "[%s%s%s]%s",
		    parent_index.str(),
		    FNSP_internal_name_seperator,
		    child_name.str(),
		    FNSP_host_ctx_map);
		break;
	case FNSP_host_context:
		sprintf(buf, "[%s]%s",
		    child_name.str(),
		    FNSP_host_ctx_map);
		break;
	case FNSP_user_context:
		sprintf(buf, "[%s]%s_%s%s",
		    child_name.str(),
		    FNSP_user_map,
		    child_name.str(),
		    FNSP_map_suffix);
		break;
	default:
		// For all other context types
		sprintf(buf, "[%s%s%s]%s", parent_index.str(),
		    FNSP_internal_name_seperator, child_name.str(),
		    parent_table.str());
		break;
	}

	child_internal_name = new FN_string((unsigned char *) buf);

	FNSP_nisAddress *child_addr = new FNSP_nisAddress(*child_internal_name,
	    context_type, repr_type);
	delete child_internal_name;
	if (child_addr == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	return (child_addr);
}

FN_ref *
FNSP_filesImpl::create_context(unsigned &status,
    const FN_string * /* dirname */,
    const FN_identifier *reftype)
{
	switch (my_address->get_context_type()) {
	case FNSP_enterprise_context:
	case FNSP_organization_context:
	case FNSP_null_context:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	default:
		break;
	}

	// Construct the identifier if not provided
	if (reftype == 0)
		reftype = FNSP_reftype_from_ctxtype(
		    my_address->get_context_type());
	if (reftype == 0) {
		status = FN_E_UNSPECIFIED_ERROR;
		return (0);
	}

	// Construct the reference
	FN_ref *ref;
	if ((my_address->get_context_type() == FNSP_printername_context) ||
	    (my_address->get_context_type() == FNSP_printer_object))
		ref = FNSP_reference(
		    FNSP_printer_files_address_type, *reftype,
		    my_address->get_internal_name(),
		    my_address->get_context_type(), FNSP_normal_repr);
	else
		ref = FNSP_reference(FNSP_files_address_type_name(),
		    *reftype, my_address->get_internal_name(),
		    my_address->get_context_type(), FNSP_normal_repr);

	if (ref == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// Construct the serialized reference
	char *data;
	if ((my_address->get_context_type() == FNSP_host_context) ||
	    (my_address->get_context_type() == FNSP_user_context))
		data = FNSP_nis_binding_serialize(*ref,
		    FNSP_hu_context, status);
	else
		data = FNSP_nis_binding_serialize(*ref,
		    FNSP_child_context, status);
	if (status != FN_SUCCESS) {
		delete ref;
		return (0);
	}

	// Construct the map name
	char map[FNS_FILES_INDEX];
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		strcpy(map, FNSP_org_map);
	else if (my_address->get_context_type() == FNSP_user_context)
		strcpy(map, FNSP_user_ctx_map);
	else
		strcpy(map, (char *) my_address->get_table_name().str());

	// Construct the map index
	char index_char[FNS_FILES_INDEX];
	strcpy(index_char, (char *) (my_address->get_index_name()).str());
	strcat(index_char, FNSP_bind_suffix);

	// Update reference value
	status = FNSP_files_update_map(map, index_char, data,
	    FNSP_map_insert);
	free(data);
	data = 0;
	if (status != FN_SUCCESS) {
		delete ref;
		return (0);
	}

	// Fix the map name for user context
	if (my_address->get_context_type() == FNSP_user_context)
		strcpy(map, (char *)
		    my_address->get_table_name().str());

	// Serialize subcontext entry
	data = FNSP_nis_sub_context_serialize(0, status);
	if (status != FN_SUCCESS) {
		delete ref;
		FNSP_files_update_map(map, index_char, data,
		    FNSP_map_delete);
		return (0);
	}

	// create the map index for sub_context entry
	strcpy(index_char, (char *) (my_address->get_index_name()).str());
	strcat(index_char, FNSP_subctx_suffix);
	status = FNSP_files_update_map(map, index_char, data,
	    FNSP_map_insert);
	if (status != FN_SUCCESS) {
		// Change the map name if user context
		if (my_address->get_context_type() == FNSP_user_context)
			strcpy(map, FNSP_user_ctx_map);
		strcpy(index_char, (char *)
		    (my_address->get_index_name()).str());
		strcat(index_char, FNSP_subctx_suffix);
		FNSP_files_update_map(map, index_char, data,
		    FNSP_map_delete);
		delete ref;
		ref = 0;
	}

	free(data);
	return (ref);
}

FN_ref *
FNSP_filesImpl::create_and_bind(
    const FN_string &name,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int find_legal_name,
    const FN_identifier *ref_type,
    const FN_attrset *attrs)
{
	FN_ref *new_ref = create_and_bind_without_attrs(
	    name, context_type, repr_type, status, find_legal_name, ref_type);

	if ((status == FN_SUCCESS) &&
	    (attrs) && (attrs->count() > 0))
		status = set_attrset(name, *attrs);
	return (new_ref);
}


FN_ref *
FNSP_filesImpl::create_and_bind_without_attrs(
    const FN_string &child_name,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int find_legal_name,
    const FN_identifier *ref_type)
{
	unsigned lstatus;
	switch (context_type) {
	case FNSP_enterprise_context:
	case FNSP_organization_context:
	case FNSP_null_context:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	case FNSP_username_context:
		// Check if the child name corresponds "user"
		if ((strcmp((char *) child_name.str(), "user") != 0) &&
		    (strcmp((char *) child_name.str(), "_user") != 0)) {
			status = FN_E_ILLEGAL_NAME;
			return (0);
		}
		break;
	case FNSP_hostname_context:
		// Check if the child name corresponds "host"
		if ((strcmp((char *) child_name.str(), "host") != 0) &&
		    (strcmp((char *) child_name.str(), "_host") != 0)) {
			status = FN_E_ILLEGAL_NAME;
			return (0);
		}
	default:
		break;
	}
	// Check if parent context exists
	if ((status = context_exists()) != FN_SUCCESS)
		return (0);

	/* Check if binding exists */
	FN_ref *ref =
	    FNSP_lookup_binding_aux(*my_address, child_name, lstatus);
	delete ref;
	if (lstatus == FN_SUCCESS) {
		status = FN_E_NAME_IN_USE;
		return (0);
	}

	if (my_address->get_impl_type() != FNSP_single_table_impl) {
		status = add_sub_context_entry(child_name);
		if (status != FN_SUCCESS) {
			return (0);
		}
	}

	// compose internal name of new context
	FNSP_nisAddress *child_addr = compose_child_addr(*my_address,
	    child_name, context_type, repr_type, status, find_legal_name);
	if ((!(child_addr)) || (status != FN_SUCCESS)) {
		delete_sub_context_entry(child_name);
		return (0);
	}

	FNSP_filesImpl impl(child_addr);
	// Create the child context
	ref = impl.create_context(status, 0, ref_type);
	if (status != FN_SUCCESS) {
		if (my_address->get_impl_type() != FNSP_single_table_impl)
			delete_sub_context_entry(child_name);
		return (0);
	}
	if (context_type == FNSP_user_context) {
		// Change ownership of the user file
		char username[FNS_FILES_INDEX];
		strcpy(username, (char *) child_name.str());
		FNSP_change_user_ownership(username);
	}
	return (ref);
}

unsigned
FNSP_filesImpl::destroy_context(const FN_string * /* dirname */)
{
	char map[FNS_FILES_INDEX];
	char map_index[FNS_FILES_INDEX];
	FN_namelist *nl;
	unsigned status;

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_OPERATION_NOT_SUPPORTED);
	case FNSP_single_table_impl:
	case FNSP_shared_table_impl:
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Check if sub-context is empty
	if (my_address->get_impl_type() == FNSP_shared_table_impl)
		nl = list_names(status);
	else
		nl = list_names_hu(status);

	// If sub-context exists, type to destroy them
	FN_status stat;
	FN_string *sub_name;
	if (nl)
		sub_name = nl->next(stat);
	else
		sub_name = 0;
	while (sub_name) {
		status = remove_binding(*sub_name);
		if (status != FN_SUCCESS) {
			delete nl;
			return (FN_E_CTX_NOT_EMPTY);
		}
		delete sub_name;
		sub_name = nl->next(stat);
	}
	delete nl;

	// Get map name
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		strcpy(map, FNSP_org_map);
	else if (my_address->get_context_type() == FNSP_user_context)
		strcpy(map, FNSP_user_ctx_map);
	else
		strcpy(map, (char *) my_address->get_table_name().str());

	// Get map index
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_bind_suffix);

	// Remove context binding
	status = FNSP_files_update_map(map, map_index, 0, FNSP_map_delete);
	if (status != FN_SUCCESS)
		return (status);

	// Remove subcontext entry
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_subctx_suffix);
	if (my_address->get_context_type() == FNSP_user_context)
		strcpy(map, (char *) my_address->get_table_name().str());
	status = FNSP_files_update_map(map, map_index, 0, FNSP_map_delete);
	return (status);
}

unsigned
FNSP_filesImpl::destroy_and_unbind(const FN_string &child_name)
{
	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_OPERATION_NOT_SUPPORTED);
	case FNSP_single_table_impl:
	case FNSP_shared_table_impl:
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	unsigned status;

	// Check if child context exists
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, child_name, status);
	if (status != FN_SUCCESS) {
		if (status == FN_E_NAME_NOT_FOUND) {
			// destroy sub-context entry and attributes
			if (my_address->get_impl_type() !=
			    FNSP_single_table_impl)
				delete_sub_context_entry(child_name);
			delete_attrset(child_name);
			return (FN_SUCCESS);
		} else
			return (status);
	}

	// Construct child reference
	FNSP_nisAddress *childaddr = new FNSP_nisAddress(*ref);
	if (childaddr == 0)
		return (FN_E_INSUFFICIENT_RESOURCES);
	FNSP_filesImpl child(childaddr);
	delete ref;
	if (childaddr->get_context_type() == 0)
		return (FN_E_OPERATION_NOT_SUPPORTED);

	status = child.destroy_context();
	if (status != FN_SUCCESS)
		return (status);

	delete_attrset(child_name);
	if ((childaddr->get_context_type() == FNSP_host_context) ||
	    (childaddr->get_context_type() == FNSP_user_context)) {
		// Parent does not have child's subcontext entry
		return (FN_SUCCESS);
	}

	// Remove child's name from parent
	status = delete_sub_context_entry(child_name);
	return (status);
}

static unsigned
FNSP_files_update_attrval_hostuser_map(int hu_map,
    const char *name, const char *attrid, const char *attrval,
    int operation)
{
	unsigned status;
	char map[FNS_FILES_INDEX];
	char *map_index;
	switch(hu_map) {
	case 1:
		strcpy(map, FNSP_user_attr_map);
		break;
	case 2:
		strcpy(map, FNSP_host_attr_map);
		break;
	case 3:
		strcpy(map, FNSP_thisorgunit_attr_map);
		break;
	}

	if (attrval) {
		map_index = (char *) malloc(strlen(attrid) +
		    strlen(FNSP_internal_name_seperator) +
		    strlen(attrval) + 1);
		if (map_index == 0)
			return (FN_E_INSUFFICIENT_RESOURCES);
		strcpy(map_index, attrid);
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, attrval);
	} else {
		map_index = (char *) malloc(strlen(attrid) + 1);
		if (map_index == 0)
			return (FN_E_INSUFFICIENT_RESOURCES);
		strcpy(map_index, attrid);
	}

	char *mapentry;
	int  maplen;
	status = FNSP_files_lookup(map, map_index,
	    strlen(map_index), &mapentry, &maplen);
	FN_nameset *ns;
	if (status == FN_SUCCESS) {
		ns = FNSP_nis_sub_context_deserialize(mapentry, status);
		free(mapentry);
		if (status != FN_SUCCESS) {
			free(map_index);
			return (status);
		}
	} else {
		status = FN_SUCCESS;
		ns = new FN_nameset;
		if (ns == 0) {
			free(map_index);
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
	}

	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		free (map_index);
		return (FN_SUCCESS);
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	switch (operation) {
	case FNSP_map_store:
	case FNSP_map_modify:
		ns->add((unsigned char *) name, FN_OP_SUPERCEDE);
		break;
	case FNSP_map_insert:
		ns->add((unsigned char *) name);
		break;
	case FNSP_map_delete:
		ns->remove((unsigned char *) name);
		break;
	default:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		break;
	}
	if (status != FN_SUCCESS) {
		delete ns;
		free(map_index);
		return (status);
	}

	// Update the entry in the map
	if (ns->count() == 0) {
		// Delete the entry in the map
		status = FNSP_files_update_map(map, map_index,
		    0, FNSP_map_delete);
		delete ns;
		if (status == FN_E_NAME_NOT_FOUND)
			status = FN_SUCCESS;
		free(map_index);
		return (status);
	}

	// Serialize the name set
	char *data = FNSP_nis_sub_context_serialize(ns, status);
	delete ns;
	if (status != FN_SUCCESS)
		return (status);
	if (strlen(data) > FNSP_HUGE_SUBCONTEXT_SIZE) {
		ns = new FN_nameset;
		ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT);
		free (data);
		data = FNSP_nis_sub_context_serialize(ns, status);
		delete ns;
	}
	status = FNSP_files_update_map(map, map_index,
	    data, FNSP_map_store);
	free(data);
	free(map_index);
	return (status);
}

static unsigned
FNSP_files_attr_construct_names(const FNSP_Address &parent,
    const FN_string &atomic_name, char *map_index,
    FN_string **map)
{
	if ((parent.get_impl_type() != FNSP_single_table_impl) &&
	    (parent.get_impl_type() != FNSP_shared_table_impl)) {
		return (FN_E_OPERATION_NOT_SUPPORTED);
	}

	// Obtain index, map
	const FN_string index = parent.get_index_name();
	(*map) = new FN_string(parent.get_table_name());

	// Construct local name if necessary
	char local_name[FNS_FILES_INDEX];
	if (!atomic_name.is_empty()) {
		strcpy(local_name, (char *) atomic_name.str());
		FNSP_construct_local_name(
		    FNSP_Syntax(parent.get_context_type())->string_case(),
		    local_name);
		FNSP_legalize_name(local_name);
	}

	unsigned ctx_type = parent.get_context_type();
	if ((ctx_type == FNSP_username_context) ||
	    (ctx_type == FNSP_hostname_context)) {
		if (atomic_name.is_empty()) {
			delete (*map);
			(*map) = new FN_string((unsigned char *)
			    FNSP_org_map);
			strcpy(map_index, (char *) index.str());
			strcat(map_index, FNSP_attr_suffix);
		} else {
			// User or host context
			strcpy(map_index, local_name);
			strcat(map_index, FNSP_attr_suffix);
			if (ctx_type == FNSP_username_context) {
				// Map should be fns_user_$user.ctx
				delete (*map);
				char new_map_name[FNS_FILES_INDEX];
				sprintf(new_map_name, "%s%s%s%s",
				    FNSP_user_map,
				    FNSP_internal_name_seperator,
				    local_name,
				    FNSP_map_suffix);
				(*map) = new FN_string(
				    (unsigned char *) new_map_name);
			}
		}
	} else {
		if (atomic_name.is_empty()) {
			strcpy(map_index, (char *) index.str());
			strcat(map_index, FNSP_attr_suffix);
			if (ctx_type == FNSP_user_context) {
				// Map should be fns_user_$user.ctx
				delete (*map);
				char new_map_name[FNS_FILES_INDEX];
				sprintf(new_map_name, "%s%s%s%s",
				    FNSP_user_map,
				    FNSP_internal_name_seperator,
				    index.str(),
				    FNSP_map_suffix);
				(*map) = new FN_string(
				    (unsigned char *) new_map_name);
			}
		} else
			sprintf(map_index, "%s%s%s%s", index.str(),
			    FNSP_internal_name_seperator,
			    local_name, FNSP_attr_suffix);
	}

	return (FN_SUCCESS);
}

static const FN_identifier
    ascii_syntax((unsigned char *) "fn_attr_syntax_ascii");
static const FN_identifier
    text_syntax((unsigned char *) "fn_attr_syntax_text");

static unsigned
FNSP_files_update_attribute_hostuser_map(const FNSP_Address &parent,
    const FN_string &name, const FN_attribute &attr, int op)
{
	// Make sure the syntax of the attribute is ASCII
	const FN_identifier *syntax = attr.syntax();

	// Get the domainname, host/user name etc.
	char map_index[FNS_FILES_INDEX];
	FN_string *map;
	unsigned status = FNSP_files_attr_construct_names(parent,
	    name, map_index, &map);
	if (status != FN_SUCCESS)
		return (status);
	// Remove the attribute suffix
	map_index[strlen(map_index) -
	    strlen(FNSP_attr_suffix)] = '\0';

	int hu_map = FNSP_is_hostuser_ctx_type(parent, name);
	const FN_attrvalue *value;
	FN_string *value_str;
	char *value_char;
	void *ip;
	const FN_identifier *id = attr.identifier();

	// Do not update attribute id only for performance resons
	// status = FNSP_files_update_attrval_hostuser_map(
	// hu_map, map_index, (char *) id->str(), 0, op);
	// if (status != FN_SUCCESS) {
	// delete map;
	// return (status);
	// }

	if (((*syntax) != ascii_syntax) &&
	    ((*syntax) != text_syntax)) {
		// This map is specially to help in fast searching.
		// Since non-ascii attributes cannot be searched, we
		// should not add this non-ascii attribute value.
		return (FN_SUCCESS);
	}

	for (value = attr.first(ip); value != NULL;
	    value = attr.next(ip)) {
		value_str = value->string();
		value_char = (char *) value_str->str();
		if ((*syntax) == text_syntax)
			// Convert to upper case
			for (size_t i = 0; i < strlen(value_char); i++)
				value_char[i] = toupper(value_char[i]);
		status = FNSP_files_update_attrval_hostuser_map(
		    hu_map, map_index, (char *) id->str(),
		    value_char, op);
		delete value_str;
		if (status != FN_SUCCESS)
			break;
	}
	delete map;
	return (status);
}

static unsigned
FNSP_files_update_attrset_hostuser_map(const FNSP_Address &parent,
    const FN_string &name, const FN_attrset &attrset, int op)
{
	unsigned status = FN_SUCCESS;
	void *ip;
	const FN_attribute *attr;
	for (attr = attrset.first(ip);
	    attr != NULL;
	    attr = attrset.next(ip)) {
		status = FNSP_files_update_attribute_hostuser_map(
		    parent, name, *attr, op);
		if (status != FN_SUCCESS)
			break;
	}
	return (status);
}

static const char *passwd_file = "/etc/passwd";
static const char *shadow_file = "/etc/shadow";
static const char *hosts_file = "/etc/hosts";

static unsigned
FNSP_files_lookup_user_passwd_entry(const char *map_index,
    char **mapentry, int *maplen)
{
	return (FNSP_files_lookup_user_like_entry(map_index,
	    mapentry, maplen, passwd_file));
}

unsigned
FNSP_files_lookup_user_shadow_entry(const char *map_index,
    char **mapentry, int *maplen)
{
	unsigned status = FNSP_files_lookup_user_like_entry(
	    map_index, mapentry, maplen, shadow_file);
	if (status != FN_SUCCESS)
		return (FNSP_files_lookup_user_passwd_entry(map_index,
		    mapentry, maplen));
	return (status);
}

static unsigned
FNSP_files_lookup_host_entry(const char *map_index,
    char **mapentry, int *maplen)
{
	return (FNSP_files_lookup_host_like_entry(map_index,
	    mapentry, maplen, hosts_file));
}


static FN_string *
FNSP_files_get_homedir(const FNSP_Address & /* parent */,
    const FN_string &username,
    unsigned &status)
{
	const char *user = (const char *)username.str(&status);
	if (user == NULL) {
		return (NULL);
	}
	char *pw_entry;
	int pw_entrylen;
	status = FNSP_files_lookup_user_passwd_entry(user,
	    &pw_entry, &pw_entrylen);
	if (status == FN_E_NO_SUCH_ATTRIBUTE) {
		status = FN_E_CONFIGURATION_ERROR;
	}
	if (status != FN_SUCCESS) {
		return (NULL);
	}
	FN_string *homedir = FNSP_homedir_from_passwd_entry(pw_entry, status);
	free(pw_entry);
	return (homedir);
}

static FN_attrset *
FNSP_files_get_builtin_attrset(const FNSP_Address &parent,
    const FN_string &atomic_name, unsigned &status)
{
	status = FN_SUCCESS;
	int user_ctx = 0;

	// Check if the builtin attributes exists
	if (FNSP_does_builtin_attrset_exist(parent, atomic_name) == 0)
		return (0);

	char map_index[FNS_FILES_INDEX];
	FN_string *map;
	status = FNSP_files_attr_construct_names(parent,
	    atomic_name, map_index, &map);
	delete map;
	if (status != FN_SUCCESS)
		return (0);
	// Remove the attribute suffix
	map_index[strlen(map_index) -
	    strlen(FNSP_attr_suffix)] = '\0';

	// Do a mmap and lookup
	char *mapentry, *shadow_mapentry;
	int maplen, shadow_maplen;

	// Fix the mapname
	if (FNSP_is_hostuser_ctx_type(parent, atomic_name) == 1) {
		user_ctx = 1;
		status = FNSP_files_lookup_user_passwd_entry(map_index,
		    &mapentry, &maplen);
		if ((status == FN_SUCCESS) && mapentry)
			status = FNSP_files_lookup_user_shadow_entry(
			    map_index, &shadow_mapentry, &shadow_maplen);
	} else
		status = FNSP_files_lookup_host_entry(map_index,
		    &mapentry, &maplen);

	if (status == FN_E_NO_SUCH_ATTRIBUTE) {
		status = FN_SUCCESS;
		return (0);
	} else if ((status != FN_SUCCESS) || (mapentry == NULL))
		return (0);

	char entry[FNS_FILES_SIZE];
	char shadow_entry[FNS_FILES_SIZE];
	strncpy(entry, mapentry, maplen);
	entry[maplen] = '\0';
	free(mapentry);
	if (user_ctx) {
		if (shadow_mapentry) {
			strncpy(shadow_entry, shadow_mapentry, shadow_maplen);
			shadow_entry[shadow_maplen] = '\0';
			free(shadow_mapentry);
		} else
			strcpy(shadow_entry, entry);
		return (FNSP_get_user_builtin_attrset(entry, shadow_entry, status));
	}
	return (FNSP_get_host_builtin_attrset(map_index, entry, status));
}

FN_attrset *
FNSP_filesImpl::get_attrset(const FN_string &atomic_name,
    unsigned &status)
{
	char map_index[FNS_FILES_INDEX];
	FN_string *map;

	status = FNSP_files_attr_construct_names(*my_address, atomic_name,
	    map_index, &map);
	if (status != FN_SUCCESS)
		return (0);

	char *mapentry;
	int maplen;
	status = FNSP_files_lookup((char *) map->str(), map_index,
	    strlen(map_index), &mapentry, &maplen);
	delete map;

	if (status == FN_E_NAME_NOT_FOUND) {
		// Check if the binding exists
		FN_ref *ref = lookup_binding(atomic_name, status);
		if (status == FN_SUCCESS)
			status = FN_E_NO_SUCH_ATTRIBUTE;
		delete ref;
	}

	FN_attrset *attrset;
	if (status == FN_SUCCESS) {
		attrset = FNSP_nis_attrset_deserialize(mapentry,
		    maplen, status);
		free(mapentry);
		if (status != FN_SUCCESS)
			return (0);
	} else if (status == FN_E_NO_SUCH_ATTRIBUTE) {
		status = FN_SUCCESS;
		attrset = 0;
	} else
		return (0);

	// Get builtin attribute if they exist
	FN_attrset *builtin = FNSP_files_get_builtin_attrset(*my_address,
	    atomic_name, status);
	if (builtin) {
		if (attrset == 0)
			return (builtin);

		// Copy the attributes from builtin to attrset
		void *ip;
		const FN_attribute *b_attr;
		for (b_attr = builtin->first(ip);
		    b_attr != 0;
		    b_attr = builtin->next(ip))
			attrset->add(*b_attr);
		delete builtin;
	}
	if (status != FN_SUCCESS) {
		delete attrset;
		return (0);
	}
	return (attrset ? attrset : new FN_attrset);
}


static int
FNSP_files_modify_set_attrset(const FNSP_Address &parent,
    const FN_string &atomic_name,
    const FN_attrset &old_attrset)
{
	unsigned status;

	// Make sure all builtin attributes are removed
	FN_attrset attrset(old_attrset);
	if (FNSP_check_builtin_attrset(parent, atomic_name,
	    attrset) != FN_SUCCESS) {
		status = FNSP_remove_builtin_attrset(attrset);
		if (status != FN_SUCCESS)
			return (status);
	}

	char map_index[FNS_FILES_INDEX];
	FN_string *map;
	status = FNSP_files_attr_construct_names(parent, atomic_name,
	    map_index, &map);
	if (status != FN_SUCCESS)
		return (status);

	if (attrset.count() == 0) {
		status = FNSP_files_update_map((char *) map->str(),
		    map_index, 0, FNSP_map_delete);
	} else {
		char *attrbuf = FNSP_nis_attrset_serialize(attrset,
		    status);
		if (status != FN_SUCCESS) {
			delete map;
			return (status);
		}
		status = FNSP_files_update_map((char *) map->str(),
		    map_index, attrbuf, FNSP_map_store);
		free(attrbuf);
	}
	delete map;
	return (status);
}

int
FNSP_filesImpl::set_attrset(const FN_string &atomic_name,
    const FN_attrset &old_attrset)
{
	unsigned status;

	// Make sure builtin attributes are removed
	FN_attrset attrset(old_attrset);
	if (FNSP_check_builtin_attrset(*my_address, atomic_name,
	    attrset) != FN_SUCCESS) {
		status = FNSP_remove_builtin_attrset(attrset);
		if (status != FN_SUCCESS)
			return (status);
	}

	status = FNSP_files_modify_set_attrset(*my_address, atomic_name,
	    attrset);

	// If host or user context type, attributes must be
	// added to seperate maps
	if ((status == FN_SUCCESS) &&
	    FNSP_is_hostuser_ctx_type(*my_address, atomic_name)) {
		status = FNSP_files_update_attrset_hostuser_map(
		    *my_address, atomic_name, attrset, FNSP_map_store);
	}

	return (status);
}

int
FNSP_filesImpl::delete_attrset(const FN_string &atomic_name)
{
	unsigned status = FN_SUCCESS;

	// Remove HU entries in the special map
	if (FNSP_is_hostuser_ctx_type(*my_address, atomic_name)) {
		FN_attrset *attrset = get_attrset(
		    atomic_name, status);
		if (attrset) {
			FNSP_remove_builtin_attrset(*attrset);
			FNSP_files_update_attrset_hostuser_map(
			    *my_address, atomic_name, *attrset,
			    FNSP_map_delete);
		}
		delete attrset;
	}

	char map_index[FNS_FILES_INDEX];
	FN_string *map;

	status = FNSP_files_attr_construct_names(*my_address,
	    atomic_name, map_index, &map);
	if (status != FN_SUCCESS)
		return (status);

	status = FNSP_files_update_map((char *) map->str(), map_index, 0,
	    FNSP_map_delete);
	delete map;
	return (status);
}

int
FNSP_filesImpl::modify_attribute(const FN_string &atomic_name,
    const FN_attribute &attr,
    unsigned flags)
{
	unsigned status;
	void *ip;
	int howmany, i;

	// Check if for builtin attributes
	status = FNSP_check_builtin_attribute(*my_address, atomic_name, attr);
	if (status != FN_SUCCESS)
		return (status);

	// First get the attribute set
	FN_attrset *aset = get_attrset(atomic_name, status);
	if (status == FN_E_NO_SUCH_ATTRIBUTE) {
		if (flags == FN_ATTR_OP_REMOVE ||
		    flags == FN_ATTR_OP_REMOVE_VALUES)
			return (FN_SUCCESS);
		else {
			aset = new FN_attrset;
			status = FN_SUCCESS;
		}
	} else if (status != FN_SUCCESS)
		return (status);
	if (!aset)
		return (FN_E_INSUFFICIENT_RESOURCES);

	// Make a copy of the attrset
	FN_attrset old_aset(*aset);

	// Perform the requested modify operation
	switch (flags) {
	case FN_ATTR_OP_ADD: {
		const FN_attribute *rm_attr = aset->get(
		    *(attr.identifier()));
		FN_attribute *delattr = 0;
		if (rm_attr)
			delattr = new FN_attribute(*rm_attr);
		if (!aset->add(attr, FN_OP_SUPERCEDE)) {
			delete aset;
			delete delattr;
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
		// First change the attribute in FNS tables
		if ((status = FNSP_files_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS) {
			delete delattr;
			break;
		}
		// If host or user context type, attributes must be
		// added to seperate maps
		if (FNSP_is_hostuser_ctx_type(*my_address,
		    atomic_name)) {
			if (delattr)
				FNSP_files_update_attribute_hostuser_map(
				    *my_address, atomic_name, *delattr,
				    FNSP_map_delete);
			status = FNSP_files_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_store);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_files_modify_set_attrset(*my_address,
				    atomic_name, old_aset);
		}
		delete delattr;
	}
	break;

	case FN_ATTR_OP_ADD_EXCLUSIVE: {
		if (!aset->add(attr, FN_OP_EXCLUSIVE)) {
			status = FN_E_ATTR_IN_USE;
			break;
		}

		// First change the attribute in FNS tables
		if ((status = FNSP_files_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS)
			break;

		// Make changes to the special HU tables
		if (FNSP_is_hostuser_ctx_type(*my_address,
		    atomic_name)) {
			status = FNSP_files_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_insert);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_files_modify_set_attrset(*my_address,
				    atomic_name, old_aset);
		}
	}
	break;

	case FN_ATTR_OP_ADD_VALUES: {
		const FN_identifier *ident = attr.identifier();
		const FN_attribute *old_attr = aset->get(*ident);

		if (old_attr == NULL) {
			if (!aset->add(attr)) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				break;
			}
		} else {
			// Check if syntax are the same
			const FN_identifier *syntax = attr.syntax();
			const FN_identifier *old_syntax = old_attr->syntax();
			if ((*syntax) != (*old_syntax)) {
				status = FN_E_INVALID_ATTR_VALUE;
				break;
			}
			// merge attr with old_attr
			FN_attribute merged_attr(*old_attr);

			howmany = attr.valuecount();
			const FN_attrvalue *new_attrval;
			new_attrval = attr.first(ip);
			for (i = 0; new_attrval && i < howmany; i++) {
				merged_attr.add(*new_attrval);
				new_attrval = attr.next(ip);
			}
			// overwrite old_attr with merged_attr
			if (!aset->add(merged_attr, FN_OP_SUPERCEDE)) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				break;
			}
		}

		// First change the attribute in FNS tables
		if ((status = FNSP_files_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS)
			break;

		// Change the values in special HU maps
		if (FNSP_is_hostuser_ctx_type(*my_address,
		    atomic_name)) {
			status = FNSP_files_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_store);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_files_modify_set_attrset(*my_address,
				    atomic_name, old_aset);
		}
	}
	break;

	case FN_ATTR_OP_REMOVE:
	case FN_ATTR_OP_REMOVE_VALUES: {
		const FN_identifier *attr_id = attr.identifier();
		const FN_attribute *old_attr = aset->get(*attr_id);
		const FN_attrvalue *attr_value;

		if (old_attr == NULL)
			break;

		if (flags == FN_ATTR_OP_REMOVE)
			aset->remove(*attr_id);
		else {
			FN_attribute inter_attr(*old_attr);
			howmany = attr.valuecount();
			attr_value = attr.first(ip);
			for (i = 0; attr_value && i < howmany; i++) {
				inter_attr.remove(*attr_value);
				attr_value = attr.next(ip);
			}
			if (inter_attr.valuecount() <= 0)
				aset->remove(*attr_id);
			else {
				if (!aset->add(inter_attr,
				    FN_OP_SUPERCEDE)) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					break;
				}
			}
		}

		// First change the attribute in FNS tables
		if (aset->count() > 0) {
			if ((status = FNSP_files_modify_set_attrset(
			    *my_address, atomic_name, (*aset)))
			    != FN_SUCCESS)
				break;
			// Delete the entries from HU maps if they exist
			if (FNSP_is_hostuser_ctx_type(*my_address,
			    atomic_name)) {
				if (flags == FN_ATTR_OP_REMOVE) {
					old_attr = old_aset.get(*attr_id);
					status =
					    FNSP_files_update_attribute_hostuser_map(
					    *my_address, atomic_name, *old_attr,
					    FNSP_map_delete);
				} else
					status =
					    FNSP_files_update_attribute_hostuser_map(
					    *my_address, atomic_name, attr,
					    FNSP_map_delete);

				if (status != FN_SUCCESS)
					// Copy old attrset
					FNSP_files_modify_set_attrset(*my_address,
					    atomic_name, old_aset);
			}
		} else
			status = delete_attrset(atomic_name);
	}
	break;

	default:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		break;	     
	}

	delete aset;
	return (status);
}

static FN_nameset *
FNSP_files_hu_search_attrval(char *map,
    const FN_identifier &id, const FN_attrvalue *value,
    unsigned int &status)
{
	// Get the index name
	char *map_index;
	if (value) {
		FN_string *name_val = value->string();
		map_index = (char *) malloc(strlen((char *) id.str()) +
		    strlen(FNSP_internal_name_seperator) + 
		    strlen((char *) name_val->str()) + 1);
		if (map_index == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
		strcpy(map_index, (char *) id.str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, (char *) name_val->str());
		delete name_val;
	} else {
		// Due to preformance reasons, return operation not supported
		// map_index = (char *) malloc(strlen((char *) id.str()) + 1);
		// if (map_index == 0) {
		// status = FN_E_INSUFFICIENT_RESOURCES;
		// return (0);
		// }
		// strcpy(map_index, (char *) id.str());
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	}

	// Lookup in map
	char *mapentry;
	int maplen;
	status = FNSP_files_lookup(map, map_index,
	    strlen(map_index), &mapentry, &maplen);
	if (status != FN_SUCCESS) {
		free(map_index);
		return (0);
	}

	// Deserialize the data
	FN_nameset *ns = FNSP_nis_sub_context_deserialize(mapentry,
	    status);
	free(mapentry);
	free(map_index);
	return (ns);
}

static FN_nameset *
FNSP_files_hu_search_attribute(char *map,
    const FN_attribute &attr, unsigned int &status)
{
	// Check if syntax is ascii
	const FN_identifier *syntax = attr.syntax();
	if (((*syntax) != ascii_syntax) && ((*syntax) != text_syntax)) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	} else
		status = FN_SUCCESS;

	FN_nameset *ns, *copy, *answer = 0;
	int initial = 0;
	const FN_identifier *id = attr.identifier();
	const FN_attrvalue *value;
	void *ip, *ip_ns;
	FN_string *value_string;
	char *value_char;
	FN_attrvalue *text_value;
	const FN_string *ns_string;

	// If first *value* is NULL search for *id* existance
	value = attr.first(ip);
	if (value == NULL) {
		answer = FNSP_files_hu_search_attrval(map, *id,
		    0, status);
	}

	for (value = attr.first(ip); value != NULL;
	    value = attr.next(ip)) {
		if ((*syntax) == text_syntax) {
			// Value must be converted to uppercase
			value_string = value->string();
			value_char = (char *) value_string->str();
			for (size_t i = 0; i < strlen(value_char); i++)
				value_char[i] = toupper(value_char[i]);
			text_value = new FN_attrvalue((unsigned char *)
			    value_char);
			delete value_string;
			value = text_value;
		}
		ns = FNSP_files_hu_search_attrval(map, *id, value, status);
		if ((*syntax) == text_syntax)
			delete (text_value);
		if ((status != FN_SUCCESS) || (ns == 0)) {
			delete answer;
			return (0);
		}
		if (initial == 0) {
			initial = 1;
			answer = ns;
		} else {
			// Get the intersection
			copy = answer;
			answer = new FN_nameset;
			for (ns_string = copy->first(ip_ns);
			    ns_string != 0;
			    ns_string = copy->next(ip_ns)) {
				if (ns->add(*ns_string) == 0)
					answer->add(*ns_string);
			}
			delete ns;
			delete copy;
		}
		if (answer->count() == 0) {
			delete answer;
			return (0);
		}
	}
	return (answer);
}

class FN_files_searchlist_hu : public FN_searchlist {
	FN_bindinglist *bl;
	unsigned int return_ref;
	FN_attrset *return_attr_id;
	FNSP_filesImpl *impl;
public:
	FN_files_searchlist_hu(FNSP_filesImpl *i, FN_bindinglist *b,
	    unsigned int rr, const FN_attrset *ra);
	~FN_files_searchlist_hu();
	FN_string *next(FN_ref **ref, FN_attrset **attrset,
	    FN_status &status);
};

FN_files_searchlist_hu::FN_files_searchlist_hu(FNSP_filesImpl *i,
    FN_bindinglist *b, unsigned int rr, const FN_attrset *ra)
{
	bl = b;
	return_ref = rr;
	impl = i;
	if (ra)
		return_attr_id = new FN_attrset(*ra);
	else
		return_attr_id = 0;
}

FN_files_searchlist_hu::~FN_files_searchlist_hu()
{
	delete bl;
	delete return_attr_id;
	delete impl;
}

FN_string *
FN_files_searchlist_hu::next(FN_ref **ref, FN_attrset **attrset,
    FN_status &status)
{
	FN_string *answer;
	if (return_ref)
		answer = bl->next(ref, status);
	else
		answer = bl->next(0, status);

	if ((!answer) || (!status.is_success()))
		return (0);

	if (attrset == 0)
		return (answer);

	// Check if attrset is required
	unsigned stat;
	FN_attrset *ss_attrset;
	if ((ss_attrset = impl->get_attrset(*answer, stat)) == 0)
		ss_attrset = new FN_attrset;
	if (!return_attr_id) {
		*attrset = ss_attrset;
		return (answer);
	}
	*attrset = FNSP_get_selected_attrset(*ss_attrset, *return_attr_id);
	delete ss_attrset;
	return (answer);
}

FN_searchlist *
FNSP_filesImpl::search_attrset_hu(
    const FN_attrset *attrset, unsigned int return_ref,
    const FN_attrset *return_attr_id, unsigned int &status)
{
	// Currently no support for searching builtin attrs
	if ((attrset) && (FNSP_is_attrset_in_builtin_attrset(*attrset))) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	}

	// Get map name
	char map[FNS_FILES_INDEX];
	int thisorgunit_map = 0;
	if (my_address->get_context_type() == FNSP_hostname_context)
		strcpy(map, FNSP_host_attr_map);
	else if (my_address->get_context_type() == FNSP_username_context)
		strcpy(map, FNSP_user_attr_map);
	else {
		strcpy(map, FNSP_thisorgunit_attr_map);
		thisorgunit_map = 1;
	}

	// Define the variables
	FN_searchset *ss;
	FN_attrset *ss_attrset;
	FN_ref *ss_ref;
	FN_attrset *rq_attrset;
	FN_string *nss_string;
	FN_status stat;

	// If the attrset is NULL, return the entire list
	if (attrset == NULL) {
		FN_bindinglist *bl;
		if (thisorgunit_map)
			bl = list_bindings(status);
		else
			bl = list_bindings_hu(status);
		if ((status != FN_SUCCESS) || (bl == NULL))
			return (0);
		FNSP_filesImpl *newImpl = new FNSP_filesImpl(
		    new FNSP_nisAddress((FNSP_nisAddress *)
		    my_address));
		return (new FN_files_searchlist_hu(newImpl,
		    bl, return_ref, return_attr_id));
	}

	// Get the name set for each attribute
	void *ip, *ip_ns;
	const FN_string *ns_string;
	const FN_attribute *attr;
	FN_nameset *ns, *copy, *answer = 0;
	int initial = 0;
	for (attr = attrset->first(ip); attr != 0;
	    attr = attrset->next(ip)) {
		ns = FNSP_files_hu_search_attribute(map, *attr, status);
		if ((status != FN_SUCCESS) || (ns == 0)) {
			delete answer;
			return (0);
		}
		if (initial == 0) {
			initial = 1;
			answer = ns;
		} else {
			// Get the intersection
			copy = answer;
			answer = new FN_nameset;
			for (ns_string = copy->first(ip_ns);
			    ns_string != 0;
			    ns_string = copy->next(ip_ns)) {
				if (ns->add(*ns_string) == 0)
					answer->add(*ns_string);
			}
			delete ns;
			delete copy;
		}
		if (answer->count() == 0) {
			delete answer;
			return (0);
		}
	}
	if ((answer == 0) || (answer->count() == 0)) {
		delete answer;
		return (0);
	}

	// Constuct the searchset
	ss = new FN_searchset;
	for (ns_string = answer->first(ip_ns);
	    ns_string != 0; ns_string = answer->next(ip_ns)) {
		if (thisorgunit_map) {
			// Obtain the correct name
			size_t parent_index_len = strlen((char *)
			    (my_address->get_index_name()).str()) +
			    strlen(FNSP_internal_name_seperator);
			char parent_index_name[FNS_FILES_INDEX];
			strcpy(parent_index_name, (char *) ns_string->str());
			if (strlen(parent_index_name) < parent_index_len)
				continue;
			char *lookup_name = &parent_index_name[parent_index_len];
			nss_string = new FN_string((unsigned char *) lookup_name);
			ns_string = nss_string;
		} else {
			// Host name and user name must be normalized
			char hu_name[FNS_FILES_INDEX];
			strcpy(hu_name, (char *) ns_string->str());
			FNSP_normalize_name(hu_name);
			nss_string = new FN_string((unsigned char *) hu_name);
			ns_string = nss_string;
		}

		if (return_ref || thisorgunit_map) {
			ss_ref = FNSP_lookup_binding_aux(*my_address,
			    *ns_string, status);
			if (status != FN_SUCCESS) {
				delete nss_string;
				if (thisorgunit_map) {
					status = FN_SUCCESS;
					continue;
				} else
					break;
			}
			if (!return_ref) {
				delete ss_ref;
				ss_ref = 0;
			}
		} else
			ss_ref = 0;

		ss_attrset = get_attrset(*ns_string, status);
		if (status != FN_SUCCESS) {
			delete nss_string;
			delete ss_attrset;
			delete ss_ref;
			break;
		}
		if (return_attr_id)
			rq_attrset = FNSP_get_selected_attrset(
			    *ss_attrset, *return_attr_id);
		else
			rq_attrset = new FN_attrset(*ss_attrset);
		ss->add(*ns_string, ss_ref, rq_attrset);
		delete nss_string;
		delete ss_ref;
		delete ss_attrset;
		delete rq_attrset;
	}
	delete answer;
	if (status != FN_SUCCESS) {
		delete ss;
		return (0);
	}
	return (new FN_searchlist_svc(ss));
}

FN_searchlist *
FNSP_filesImpl::search_attrset(const FN_attrset *old_attrset,
    unsigned int return_ref,
    const FN_attrset *return_attr_id,
    unsigned int &status)
{
	if ((my_address->get_context_type() == FNSP_hostname_context) ||
	    (my_address->get_context_type() == FNSP_username_context) ||
	    (strncmp((char *) my_address->get_table_name().str(),
	    FNSP_org_map, strlen(FNSP_org_map)) == 0)) {
		return (search_attrset_hu(old_attrset,
		    return_ref, return_attr_id, status));
	}

	// Get the subcontexts
	FN_nameset *ns = list_nameset(status);
	if (!ns || (status != FN_SUCCESS))
		return (0);

	// Allocate searchset
	FN_searchset *ss = new FN_searchset;
	if (ss == NULL) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		delete ns;
		return (0);
	}

	FN_attrset *attrset;
	if (!old_attrset)
		attrset = new FN_attrset;
	else
		attrset = new FN_attrset(*old_attrset);

	FN_attrset *name_attrset;
	FN_ref *ref;
	FN_attrset *request_attrset;
	const FN_string *name;
	void *ip;
	for (name = ns->first(ip); name != NULL;
	    name = ns->next(ip)) {
		name_attrset = get_attrset(*name, status);
		if (status == FN_E_NO_SUCH_ATTRIBUTE) {
			status = FN_SUCCESS;
			name_attrset = 0;
		} else if (status != FN_SUCCESS)
			break;

		if (FNSP_is_attrset_subset(*name_attrset, *attrset)) {
			if (return_ref) {
				ref = lookup_binding(*name, status);
				if (status != FN_SUCCESS) {
					delete name_attrset;
					break;
				}
			} else
				ref = 0;
			if (name_attrset) {
				if (return_attr_id)
					request_attrset =
					    FNSP_get_selected_attrset(
					    *name_attrset, *return_attr_id);
				else
					request_attrset = new
					    FN_attrset(*name_attrset);
			} else
				request_attrset = 0;
			ss->add(*name, ref, request_attrset);
			delete ref;
			delete request_attrset;
		}
		delete name_attrset;
	}
	delete attrset;
	delete ns;
	if (status != FN_SUCCESS) {
		delete ss;
		return (0);
	}

	if (ss != NULL && ss->count() == 0) {
		delete ss;
		return (0);
	}
	return (new FN_searchlist_svc(ss));
}

FN_ref *
FNSP_filesImpl::get_nns_ref()
{
	return (FNSP_reference(FNSP_files_address_type_name(),
	    my_address->get_internal_name(), FNSP_nsid_context));
}
