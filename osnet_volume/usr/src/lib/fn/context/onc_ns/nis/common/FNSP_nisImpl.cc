/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisImpl.cc	1.41	97/11/10 SMI"

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>

#include <xfn/xfn.hh>
#include <xfn/FN_namelist_svc.hh>
#include <xfn/FN_bindinglist_svc.hh>
#include <xfn/FN_searchlist_svc.hh>

#include <FNSP_Syntax.hh>
#include <fnsp_utils.hh>

#include "FNSP_nisImpl.hh"
#include "fnsp_nis_internal.hh"
#include "fnsp_internal_common.hh"

// Address type idenitifier for NIS
static const FN_identifier
FNSP_nis_address_type((unsigned char *) "onc_fn_nis");
static const FN_identifier
FNSP_printer_nis_address_type((unsigned char *) "onc_fn_printer_nis");

static const FN_string FNSP_self = (unsigned char *) FNSP_SELF_STR;
static const FN_string FNSP_self_name((unsigned char *) "_fns_nns_");

/* Index names, seperators and suffixes for NIS maps */
static const char *FNSP_attr_suffix = FNSP_ATTR_SUFFIX;
static const char *FNSP_bind_suffix = FNSP_BIND_SUFFIX;
static const char *FNSP_subctx_suffix = FNSP_SUBCTX_SUFFIX;
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
FNSP_nis_get_homedir(const FNSP_Address &,
    const FN_string &username,
    unsigned &status);

const FN_identifier &
FNSP_nis_address_type_name(void)
{
	return (FNSP_nis_address_type);
}

static const FN_identifier &
FNSP_printer_nis_address_type_name()
{
	return (FNSP_printer_nis_address_type);
}

FN_string *
FNSP_nis_orgname_of(FN_string &name, unsigned &status)
{
	FN_string table;
	FN_string index;
	FNSP_decompose_nis_index_name(name, table, index);

	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(table, &map, &domain);
	delete map;
	return (domain);
}

// ------------------------------------------
// FNSP_nisImpl
// ------------------------------------------

FNSP_nisImpl::FNSP_nisImpl(FNSP_nisAddress *addr) : FNSP_Impl(addr)
{
}

FNSP_nisImpl::~FNSP_nisImpl() {
}

int
FNSP_nisImpl::is_this_address_type_p(const FN_ref_addr &addr)
{
	return (((*addr.type()) == FNSP_nis_address_type) ||
	    ((*addr.type()) == FNSP_printer_nis_address_type));
}

static FN_ref *
FNSP_lookup_binding_aux(const FNSP_Address &parent,
    const FN_string &bname,
    unsigned &status)
{
	// Construct local name to lookup
	char aname[FNS_NIS_INDEX];
	strcpy(aname, (char *) bname.str());
	unsigned context_type = parent.get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), aname);
	FNSP_legalize_name(aname);

	// For map index
	char map_index[FNS_NIS_INDEX];

	// Check the implementation type
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

	// Obtain domain and map name
	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(parent.get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (0);

	// Lookup for binding
	char *mapentry;
	int maplen, from_local_map = 0;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen);
	delete map;
	delete domain;
	if (status != FN_SUCCESS)
		return (0);

	// Construct the reference
	FNSP_binding_type btype;
	FN_ref *ref = FNSP_nis_binding_deserialize(mapentry, maplen,
	    btype, status);
	free(mapentry);
	return (ref);
}

FN_ref *
FNSP_nisImpl::lookup_binding(const FN_string &bname, unsigned &status)
{
	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, bname, status);
	if (ref == 0) {
		return (0);
	}

	FNSP_process_user_fs(*my_address, *ref, FNSP_nis_get_homedir, status);
	if (status != FN_SUCCESS) {
		delete ref;
		ref = 0;
	}
	return (ref);
}

// ---------------------------------------------------
// Routine to read single table implementation maps,
// ie., passwd.byname and hosts.byname
// ---------------------------------------------------
class FN_nis_namelist_hu : public FN_namelist {
	FN_nis_map_enumerate *nis_map;
	char *next_in_line;
	char *next_ref;
	unsigned int next_status;
	char *outval, *outkey;
	int outvallen, outkeylen, yp_ret;
	int name_from_keyvalue();

	// Variables for huge sub-contexts
	char *parent_index;
	int name_from_keyvalue_and_my_address();
public:
	FN_nis_namelist_hu(const char *domain, const char *map,
	    const char *pindex = 0);
	virtual ~FN_nis_namelist_hu();
	FN_string *next(FN_status &);
	FN_string *next_with_ref(FN_status &, FN_ref ** = 0);
};

FN_nis_namelist_hu::~FN_nis_namelist_hu()
{
	if (next_in_line)
		free(next_in_line);
	if (next_ref)
		free(next_ref);
	if (parent_index)
		free(parent_index);
	delete nis_map;
}

int
FN_nis_namelist_hu::name_from_keyvalue_and_my_address()
{
	char *key = (char *) calloc(outkeylen+1, sizeof(char));
	strncpy(key, outkey, outkeylen);
	next_in_line = FNSP_check_if_subcontext(parent_index, key);
	free (key);
	if (!next_in_line)
		return (0);
	// Copy the reference value
	next_ref = (char *) malloc(outvallen + 1);
	strncpy(next_ref, outval, outvallen);
	next_ref[outvallen] = '\0';
	return (1);
}

int
FN_nis_namelist_hu::name_from_keyvalue()
{
	if (parent_index)
		return (name_from_keyvalue_and_my_address());

	if ((strncmp(outval, FNSP_nis_hu_reference,
	    strlen(FNSP_nis_hu_reference)) == 0) ||
	    (strncmp(outval, FNSP_nis_hu_context,
	    strlen(FNSP_nis_hu_context)) == 0)) {
		// Found an entry
		next_in_line = (char *) malloc(outkeylen -
		    strlen(FNSP_bind_suffix) + 1);
		strncpy(next_in_line, outkey,
		    (outkeylen - strlen(FNSP_bind_suffix)));
		next_in_line[outkeylen - strlen(FNSP_bind_suffix)] = '\0';
		// Copy the reference value
		next_ref = (char *) malloc(outvallen + 1);
		strncpy(next_ref, outval, outvallen);
		next_ref[outvallen] = '\0';
		return (1);
	} else
		return (0);
}

FN_nis_namelist_hu::FN_nis_namelist_hu(const char *domain,
    const char *map, const char *pindex)
{
	nis_map = new FN_nis_map_enumerate(domain, map);
	next_in_line = 0;
	next_ref = 0;
	if (pindex)
		parent_index = strdup(pindex);
	else
		parent_index = 0;
	next_status = FN_SUCCESS;

	while ((yp_ret = nis_map->next(&outkey, &outkeylen,
	    &outval, &outvallen)) == 0) {
		if (name_from_keyvalue()) {
			free (outkey);
			free (outval);
			break;
		}
		free (outkey);
		free (outval);
	}

	if ((yp_ret != 0) &&
	    (yp_ret != YPERR_NOMORE) &&
	    (yp_ret != YPERR_MAP))
		next_status = FNSP_nis_map_status(yp_ret);
}

FN_string *
FN_nis_namelist_hu::next(FN_status &status)
{
	return (next_with_ref(status));
}

FN_string *
FN_nis_namelist_hu::next_with_ref(FN_status &status, FN_ref **ref)
{
	if (next_status != FN_SUCCESS) {
		status.set(next_status);
		return (0);
	}

	status.set_success();
	if ((next_in_line == 0) || (next_ref == 0)) {
		next_status = FN_E_INVALID_ENUM_HANDLE;
		return (0);
	}

	// User and host name must be normalized
	FNSP_normalize_name(next_in_line);
	FN_string *name = new
	    FN_string((const unsigned char *) next_in_line);
	free(next_in_line);

	// Get the reference
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
		return (name);
	}

	while ((yp_ret = nis_map->next(&outkey, &outkeylen,
	    &outval, &outvallen)) == 0) {
		if (name_from_keyvalue()) {
			free(outkey);
			free(outval);
			break;
		}
		free(outkey);
		free(outval);
	}
	if (yp_ret != 0) {
		if ((yp_ret == YPERR_NOMORE) ||
		    (yp_ret == YPERR_MAP)) {
			next_in_line = 0;
			next_ref = 0;
			next_status = FN_SUCCESS;
		} else
			next_status = FNSP_nis_map_status(yp_ret);
	}
	return (name);
}

static FN_namelist *
FNSP_namelist_from_map(const FN_string &domain, const FN_string &map,
    unsigned &status)
{
	FN_nis_namelist_hu *ns = new
	    FN_nis_namelist_hu((const char *) domain.str(),
	    (const char *) map.str());
	if (ns == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	} else
		status = FN_SUCCESS;
	return (ns);
}

FN_namelist *
FNSP_nisImpl::list_names_hu(unsigned &status)
{
	if (my_address->get_impl_type() != FNSP_single_table_impl)
		return (0);
	FN_string *map;
	FN_string *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (0);
	FN_namelist *ns = FNSP_namelist_from_map(*domain, *map, status);
	delete map;
	delete domain;
	return (ns);
}

// Namelist for contexts that are greater than 5K in size
// class FN_nis_namelist_org : public FN_namelist {
	
FN_nameset *
FNSP_nisImpl::list_nameset(unsigned &status, int /* children_only */)
{
	FN_nameset *ns = 0;

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		// Only organizations have direcotory implementation
		// Since no sub-organizations in NIS, return empty
		// name set
		ns = new FN_nameset;
		if (ns != 0)
			status = FN_SUCCESS;
		else
			status = FN_E_INSUFFICIENT_RESOURCES;
		return (ns);
	case FNSP_shared_table_impl:
		break;
	case FNSP_single_table_impl:
	default:
		status = FN_E_CONFIGURATION_ERROR; // internal error
		return (0);
	}

	// else FNSP_shared_table_impl
	FN_string *map;
	FN_string *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (0);
	// Get map index
	char map_index[FNS_NIS_INDEX];
	strcpy(map_index, (char *) my_address->get_index_name().str());
	strcat(map_index, FNSP_subctx_suffix);

	char *mapentry;
	int maplen;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen);

	if (status == FN_SUCCESS) {
		ns = FNSP_nis_sub_context_deserialize(mapentry, status);
		if (ns)
			ns->remove(FNSP_self_name);
		else
			status = FN_E_INSUFFICIENT_RESOURCES;
		free(mapentry);
	}
	delete domain;
	delete map;
	return (ns);
}

FN_namelist *
FNSP_nisImpl::list_names(unsigned &status, int /* children_only */)
{
	FN_nameset *ns = list_nameset(status);
	if ((!ns) || (status != FN_SUCCESS)) {
		delete ns;
		return (0);
	}

	// Check if the subcontext is huge
	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		FN_string *map, *domain;
		status = FNSP_nis_split_internal_name(
		    my_address->get_table_name(), &map, &domain);
		if (status != FN_SUCCESS)
			return (0);
		FN_namelist *nl = new FN_nis_namelist_hu(
		    (char *) domain->str(),
		    (char *) map->str(),
		    (char *) my_address->get_index_name().str());
		delete domain;
		delete map;
		return (nl);
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
FNSP_nisImpl::context_exists()
{
	if (my_address->get_impl_type() == FNSP_directory_impl)
		return (FN_SUCCESS);

	unsigned status;
	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (status);

	switch (my_address->get_impl_type()) {
	case FNSP_single_table_impl:
		delete map;
		map = new FN_string((unsigned char *) FNSP_org_map);
		break;
	case FNSP_shared_table_impl:
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	char map_index[FNS_NIS_INDEX];
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_bind_suffix);

	/* Perform a yp_match */
	char *mapentry;
	int maplen;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen);
	delete map;
	delete domain;

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
	return (FN_SUCCESS);
}

// ----------------------------------------------
// List bindings -- calls list_names() to get names
// and then calls lookup_binding() to get each binding
// ----------------------------------------------
class FN_nis_bindinglist_hu : public FN_bindinglist {
	FN_nis_namelist_hu *namelist;
public:
	FN_nis_bindinglist_hu(FNSP_nisImpl *impl, int non_hu = 0);
	virtual ~FN_nis_bindinglist_hu();

	FN_string *next(FN_ref **, FN_status &);
};

FN_nis_bindinglist_hu::FN_nis_bindinglist_hu(FNSP_nisImpl *impl,
    int non_hu)
{
	unsigned status;
	FN_string *domain, *map;
	status = FNSP_nis_split_internal_name(impl->my_address->get_table_name(),
	    &map, &domain);
	if (status == FN_SUCCESS) {
		if (non_hu)
			namelist = new FN_nis_namelist_hu(
			    (char *) domain->str(),
			    (char *) map->str(),
			    (char *) (impl->my_address->get_table_name()).str());
		else
			namelist = new FN_nis_namelist_hu(
			    (char *) domain->str(),
			    (char *) map->str());
		delete domain;
		delete map;
	} else
		namelist = 0;
}

FN_nis_bindinglist_hu::~FN_nis_bindinglist_hu()
{
	delete namelist;
}

FN_string *
FN_nis_bindinglist_hu::next(FN_ref **ref, FN_status &in_status)
{
	return (namelist->next_with_ref(in_status, ref));
}

FN_bindinglist *
FNSP_nisImpl::list_bindings_hu(unsigned &status)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		FN_nis_bindinglist_hu *hu_bs =
		    new FN_nis_bindinglist_hu(this);
		if (hu_bs == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
		else
			status = FN_SUCCESS;
		return (hu_bs);
	}
	status = FN_E_CONFIGURATION_ERROR;
	return (0);
}

FN_bindinglist *
FNSP_nisImpl::list_bindings(unsigned &status)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}
	FN_nameset *ns = list_nameset(status);
	if ((!ns || status != FN_SUCCESS))
		return (0);

	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		return (new FN_nis_bindinglist_hu(this, 1));
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	FN_bindingset *bs = new FN_bindingset;
	if (bs == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	const FN_string *aname;
	FN_ref *ref;
	void *ip;
	for (aname = ns->first(ip); aname; aname = ns->next(ip)) {
		ref = lookup_binding((*aname), status);
		if (status != FN_SUCCESS)
			return (new FN_bindinglist_svc(bs));
		bs->add(*aname, *ref);
		delete ref;
	}
	delete ns;
	return (new FN_bindinglist_svc(bs));
}

// --------------------------------------------
// Routine used by the organization context
// to get *thisorgunit* reference.
// --------------------------------------------
FN_string *
FNSP_nis_get_org_nns_objname(const FN_string *dirname)
{
	FN_string *nnsobjname;
	char *domain;
	char buf[256];
	char domain_name[256];

	strcpy(domain_name, (char *) dirname->str());
	domain = strchr(domain_name, ' ');
	if (domain != NULL)
		*domain = '\0';
	sprintf(buf, "[%s]%s.%s",
	    FNSP_self.str(),
	    FNSP_org_map,
	    domain_name);

	nnsobjname = new FN_string((unsigned char *) buf);
	return (nnsobjname);
}

FN_string *
FNSP_nisImpl::get_nns_objname(const FN_string *dirname)
{
	return (FNSP_nis_get_org_nns_objname(dirname));
}


// ---------------------------------------------
// Adding and deleting sub-contexts ie., lists
// in a context.
// ---------------------------------------------

unsigned
FNSP_nisImpl::add_sub_context_entry(const FN_string &child)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		return (FN_SUCCESS);

	// Construct local names
	char aname[FNS_NIS_INDEX];
	strcpy(aname, (char *) child.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), aname);
	const FN_string child_name((unsigned char *) aname);

	unsigned status;
	FN_nameset *ns = list_nameset(status);
	if (status == FN_E_NAME_NOT_FOUND)
		status = FN_SUCCESS;
	else if (status != FN_SUCCESS)
		return (status);
	if (ns == 0)
		ns = new FN_nameset;

	// Check if subcontext is already huge
	if (ns->add((unsigned char *) FNSP_HUGE_SUBCONTEXT, 1) == 0) {
		delete ns;
		return (FN_SUCCESS);
	} else
		ns->remove((unsigned char *) FNSP_HUGE_SUBCONTEXT);

	// Serialize the name set and place it back in the YP-map
	ns->add(child_name);
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

	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS) {
		free(data);
		return (status);
	}

	// Get the map name, and index
	char map_index[FNS_NIS_INDEX];
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_subctx_suffix);

	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), (char *) map_index, data, FNSP_map_store);
	delete map;
	delete domain;
	free(data);
	return (status);
}

unsigned
FNSP_nisImpl::delete_sub_context_entry(const FN_string &child)
{
	if (my_address->get_impl_type() == FNSP_single_table_impl)
		return (FN_SUCCESS);

	// Construct local name
	char aname[FNS_NIS_INDEX];
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

	// Get mapname and domainname
	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS) {
		free(data);
		return (status);
	}

	// Get the index name
	const FN_string index = my_address->get_index_name();
	char map_index[FNS_NIS_INDEX];
	strcpy(map_index, (char *) index.str());
	strcat(map_index, FNSP_subctx_suffix);

	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), (char *) map_index, data, FNSP_map_store);

	delete map;
	delete domain;
	free(data);
	return (status);
}

unsigned
FNSP_nisImpl::add_binding_without_attrs(
    const FN_string &aname,
    const FN_ref &ref,
    unsigned flags)
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
	if ((status = context_exists())
	    != FN_SUCCESS)
		return (status);

	// Construct local name
	char atomic_name[FNS_NIS_INDEX];
	strcpy(atomic_name, (char *) aname.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), atomic_name);
	FNSP_legalize_name(atomic_name);

	// Add binding information, Serialize reference
	char map_index[FNS_NIS_INDEX];
	char *refbuf;
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		refbuf = FNSP_nis_binding_serialize(ref,
		    FNSP_hu_reference, status);
		strcpy(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
	} else {
		refbuf = FNSP_nis_binding_serialize(ref,
		    FNSP_bound_reference, status);
		strcpy(map_index, (char *)
		    my_address->get_index_name().str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
	}
	if (status != FN_SUCCESS) {
		return (status);
	}

	// Obtain the child's map name and domainname from parent address
	FN_string *map, *domain;
	const FN_string parent_index = my_address->get_index_name();
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS) {
		return (status);
	}

	if (flags == FN_OP_EXCLUSIVE)
		op = FNSP_map_insert;
	else
		op = FNSP_map_store;

	// Lookup to check if the bindings exists
	char *old_mapentry;
	int old_maplen;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &old_mapentry, &old_maplen);
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
			    strlen(FNSP_nis_context));
		}
		free(old_mapentry);
	}

	// Update map with binding information
	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), map_index, refbuf, op);

	if (status == FN_SUCCESS) {
		// Add the child name to the sub-context name list
		status = add_sub_context_entry(aname);
		if (status != FN_SUCCESS)
			FNSP_update_map((char *) domain->str(),
			    (char *) map->str(), map_index, refbuf,
			    FNSP_map_delete);
	}
	delete map;
	delete domain;
	free(refbuf);
	return (status);
}

unsigned int
FNSP_nisImpl::add_binding(const FN_string &name,
    const FN_ref &ref, const FN_attrset *attrs,
    unsigned int exclusive)
{
	unsigned status;
	status = add_binding_without_attrs(name, ref, exclusive);

	if ((status == FN_SUCCESS) && (attrs) &&
	    (attrs->count() > 0)) {
		status = set_attrset(name, *attrs);
		if (status != FN_SUCCESS)
			remove_binding(name);
	}
	return (status);
}

unsigned
FNSP_nisImpl::remove_binding(const FN_string &aname, int force)
{
	unsigned status;

	// Construct local name
	char atomic_name[FNS_NIS_INDEX];
	strcpy(atomic_name, (char *) aname.str());
	unsigned context_type = my_address->get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(context_type)->string_case(), atomic_name);
	FNSP_legalize_name(atomic_name);

	char map_index[FNS_NIS_INDEX];
	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_OPERATION_NOT_SUPPORTED);
	case FNSP_single_table_impl:
		strcpy(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
		break;
	case FNSP_shared_table_impl:
		strcpy(map_index, (char *)
		    (my_address->get_index_name()).str());
		strcat(map_index, FNSP_internal_name_seperator);
		strcat(map_index, atomic_name);
		strcat(map_index, FNSP_bind_suffix);
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (status);

	// Lookup at the bindings and check
	int maplen;
	char *mapentry = 0;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen);
	if (status != FN_SUCCESS) {
		FNSP_update_map((char *) domain->str(),
		    (char *) map->str(), map_index, mapentry,
		    FNSP_map_delete);
		delete map;
		delete domain;
		delete_attrset(aname);
		delete_sub_context_entry(aname);
		if (status == FN_E_NAME_NOT_FOUND)
			return (FN_SUCCESS);
		return (status);
	}

	// Check for *bound reference* (not a context)
	if (((strncmp(mapentry, FNSP_nis_reference,
	    strlen(FNSP_nis_reference)) == 0) ||
	    (strncmp(mapentry, FNSP_nis_hu_reference,
	    strlen(FNSP_nis_hu_reference)) == 0)) || (force))
		status = FNSP_update_map((char *) domain->str(),
		    (char *) map->str(), map_index, mapentry,
		    FNSP_map_delete);
	else
		status = FN_E_NAME_IN_USE;

	delete map;
	delete domain;
	free(mapentry);
	if (status != FN_SUCCESS)
		return (status);

	// Remove from the parent's sub-context listing
	status = delete_sub_context_entry(aname);
	delete_attrset(aname);
	return (status);
}

unsigned
FNSP_nisImpl::remove_binding(const FN_string &aname)
{
	return (remove_binding(aname, 0));
}

unsigned
FNSP_nisImpl::rename_binding(
    const FN_string &atomic_name,
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

	// Add new binding binding
	status = add_binding(new_name, *ref, attrset, flags);
	if (status != FN_SUCCESS) {
		delete ref;
		delete attrset;
		return (status);
	}

	// Delete binding
	status = remove_binding(atomic_name);
	if ((status == FN_SUCCESS) && (attrset))
		delete_attrset(atomic_name);
	delete ref;
	delete attrset;
	return (status);
}

// ---------------------------------------------------
// Function to compose address of the child name.
// The arguments are parent address, child_name etc.,
// With atomic child_name, the index-ed child name
// must be determined, and the address composed
// from it.
// ---------------------------------------------------
static FNSP_nisAddress *
compose_child_addr(const FNSP_Address &parent,
    const FN_string &child,
    unsigned context_type,
    unsigned repr_type,
    unsigned &status,
    int /* find_legal_name */)
{
	FN_string *child_internal_name;
	char buf[FNS_NIS_INDEX];
	FN_string *map, *domain;
	const FN_string parent_index = parent.get_index_name();
	const FN_string parent_table = parent.get_table_name();
	status = FNSP_nis_split_internal_name(parent_table, &map, &domain);
	if (status != FN_SUCCESS)
		return (0);

	// Construct local name
	char achild_name[FNS_NIS_INDEX];
	strcpy(achild_name, (char *) child.str());
	unsigned pcontext_type = parent.get_context_type();
	FNSP_construct_local_name(
	    FNSP_Syntax(pcontext_type)->string_case(), achild_name);
	FNSP_legalize_name(achild_name);
	const FN_string child_name((unsigned char *) achild_name);

	// Special case for FNSP_username_context and
	// FNSP_hostname_context
	switch (context_type) {
	case FNSP_username_context:
		sprintf(buf, "[%s%s%s]%s.%s",
		    parent_index.str(),
		    FNSP_internal_name_seperator,
		    child_name.str(),
		    FNSP_user_ctx_map, domain->str());
		break;
	case FNSP_hostname_context:
		sprintf(buf, "[%s%s%s]%s.%s",
		    parent_index.str(),
		    FNSP_internal_name_seperator,
		    child_name.str(),
		    FNSP_host_ctx_map, domain->str());
		break;
	case FNSP_host_context:
		sprintf(buf, "[%s]%s.%s",
		    child_name.str(),
		    FNSP_host_ctx_map, domain->str());
		break;
	case FNSP_user_context:
		sprintf(buf, "[%s]%s.%s",
		    child_name.str(),
		    FNSP_user_ctx_map, domain->str());
		break;
	default:
		// For all other context types
		sprintf(buf, "[%s%s%s]%s", parent_index.str(),
		    FNSP_internal_name_seperator, child_name.str(),
		    parent_table.str());
		break;
	}

	child_internal_name = new FN_string((unsigned char *) buf);
	delete map;
	delete domain;

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
FNSP_nisImpl::create_context(
    unsigned &status,
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

	/* Construct the identifier if not provided */
	if (reftype == 0)
		reftype = FNSP_reftype_from_ctxtype(
		    my_address->get_context_type());
	if (reftype == 0) {
		status = FN_E_UNSPECIFIED_ERROR;
		return (0);
	}

	/* Construct the reference */
	FN_ref *ref;
	if ((my_address->get_context_type() == FNSP_printername_context) ||
	    (my_address->get_context_type() == FNSP_printer_object))
		ref = FNSP_reference(FNSP_printer_nis_address_type_name(),
		    *reftype, my_address->get_internal_name(),
		    my_address->get_context_type(), FNSP_normal_repr);
	else
		ref = FNSP_reference(FNSP_nis_address_type_name(),
		    *reftype, my_address->get_internal_name(),
		    my_address->get_context_type(), FNSP_normal_repr);

	if (ref == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS) {
		delete ref;
		return (0);
	}

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
		delete map;
		delete domain;
		return (0);
	}

	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		char map_file[FNS_NIS_INDEX];
		strcpy(map_file, FNSP_org_map);
		delete map;
		map = new FN_string((unsigned char *) map_file);
		if (map == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			delete ref;
			delete domain;
			free(data);
			return (0);
		}
	}

	char index_char[FNS_NIS_INDEX];
	/* Create the context entry */
	strcpy(index_char, (char *) (my_address->get_index_name()).str());
	strcat(index_char, FNSP_bind_suffix);
	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), (char *) index_char, data,
	    FNSP_map_insert);
	free(data);
	data = 0;
	if (status != FN_SUCCESS) {
		delete ref;
		delete map;
		delete domain;
		return (0);
	}

	/* create the sub_context entry */
	strcpy(index_char, (char *) (my_address->get_index_name()).str());
	strcat(index_char, FNSP_subctx_suffix);
	data = FNSP_nis_sub_context_serialize(0, status);
	if (status != FN_SUCCESS) {
		delete ref;
		delete map;
		delete domain;
		return (0);
	}
	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), (char *) index_char, data,
	    FNSP_map_insert);
	if (status != FN_SUCCESS) {
		strcpy(index_char, (char *)
		    (my_address->get_index_name()).str());
		strcat(index_char, FNSP_subctx_suffix);
		FNSP_update_map((char *) domain->str(),
		    (char *) map->str(), index_char, data,
		    FNSP_map_delete);
		delete ref;
		ref = 0;
	}
	delete map;
	delete domain;
	if (data)
		free(data);
	return (ref);
}

FN_ref *
FNSP_nisImpl::create_and_bind(
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
FNSP_nisImpl::create_and_bind_without_attrs(
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
	FNSP_nisImpl child_impl(child_addr);
	// Create the child context
	ref = child_impl.create_context(status, 0, ref_type);
	if (status != FN_SUCCESS) {
		delete_sub_context_entry(child_name);
		return (0);
	}
	return (ref);
}

unsigned
FNSP_nisImpl::destroy_context(const FN_string * /* dirname */)
{
	unsigned status = FN_SUCCESS;

	switch (my_address->get_impl_type()) {
	case FNSP_directory_impl:
		return (FN_E_MALFORMED_REFERENCE);
	case FNSP_single_table_impl:
	case FNSP_shared_table_impl:
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Check if sub-context is empty
	FN_namelist *nl = 0;
	if (my_address->get_impl_type() == FNSP_shared_table_impl)
		nl = list_names(status);
	else
		nl = list_names_hu(status);

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

	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS)
		return (status);

	// Fix the map name for single table implementation
	if (my_address->get_impl_type() == FNSP_single_table_impl) {
		delete map;
		map = new FN_string((unsigned char *) FNSP_org_map);
	}

	// Remove parent's sub-context entry
	char map_index[FNS_NIS_INDEX];
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_subctx_suffix);
	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), map_index, 0, FNSP_map_delete);
	if (status != FN_SUCCESS) {
		delete map;
		delete domain;
		return (status);
	}

	// Remove context binding
	strcpy(map_index, (char *) (my_address->get_index_name()).str());
	strcat(map_index, FNSP_bind_suffix);
	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), map_index, 0, FNSP_map_delete);
	
	delete map;
	delete domain;
	return (status);
}

unsigned
FNSP_nisImpl::destroy_and_unbind(const FN_string &child_name)
{
	unsigned status;

	FN_ref *ref = FNSP_lookup_binding_aux(*my_address, child_name, status);
	if (status != FN_SUCCESS) {
		// Could be confuguration error, remove all entries
		// delete the subcontexts and attributes
		delete_attrset(child_name);
		if (my_address->get_impl_type() !=
		    FNSP_single_table_impl)
			delete_sub_context_entry(child_name);
		// Delete bindings
		remove_binding(child_name, 1);
		return (FN_SUCCESS);
	}

	FNSP_nisAddress *childaddr = new FNSP_nisAddress(*ref);
	FNSP_nisImpl child(childaddr);
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
FNSP_nis_update_attrval_hostuser_map(int hu_map, const char *domain,
    const char *name, const char *attrid, const char *attrval,
    int operation)
{
	unsigned status;
	char map[FNS_NIS_INDEX];
	char *map_index;
	switch (hu_map) {
	case 1:
		strcpy(map, FNSP_user_attr_map);
		break;
	case 2:
		strcpy(map, FNSP_host_attr_map);
		break;
	case 3:
		strcpy(map, FNSP_thisorgunit_attr_map);
		break;
	default:
		return (FN_E_CONFIGURATION_ERROR);
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
	status = FNSP_yp_map_lookup((char *) domain, map,
	    map_index, strlen(map_index), &mapentry, &maplen);
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
		if ((ns->add((unsigned char *) name, FN_OP_SUPERCEDE))
		    == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
		break;
	case FNSP_map_insert:
		if ((ns->add((unsigned char *) name)) == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
		break;
	case FNSP_map_delete:
		ns->remove((unsigned char *) name);
		break;
	default:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		break;
	}
	if (status != FN_SUCCESS) {
		free(map_index);
		delete ns;
		return (status);
	}

	// Update the entry in the map
	if (ns->count() == 0) {
		// Delete the entry in the map
		status = FNSP_update_map((char *) domain, map,
		    map_index, 0, FNSP_map_delete);
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
	status = FNSP_update_map((char *) domain, map, map_index,
	    data, FNSP_map_store);
	free(data);
	free(map_index);
	return (status);
}

static unsigned
FNSP_nis_attr_construct_names(const FNSP_Address &parent,
    const FN_string &atomic_name, char *map_index,
    FN_string **map, FN_string **domain)
{
	unsigned status;
	if ((parent.get_impl_type() != FNSP_single_table_impl) &&
	    (parent.get_impl_type() != FNSP_shared_table_impl)) {
		return (FN_E_OPERATION_NOT_SUPPORTED);
	}

	// Obtain index, map and domain
	const FN_string index = parent.get_index_name();
	status = FNSP_nis_split_internal_name(parent.get_table_name(),
	    map, domain);
	if (status != FN_SUCCESS)
		return (status);

	// Construct local name if necessary
	char local_name[FNS_NIS_INDEX];
	if (!atomic_name.is_empty()) {
		strcpy(local_name, (char *) atomic_name.str());
		FNSP_construct_local_name(
		    FNSP_Syntax(parent.get_context_type())->string_case(),
		    local_name);
		FNSP_legalize_name(local_name);
	}

	// Fix the map name and index name
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
			strcpy(map_index, local_name);
			strcat(map_index, FNSP_attr_suffix);
		}
	} else {
		if (atomic_name.is_empty()) {
			strcpy(map_index, (char *) index.str());
			strcat(map_index, FNSP_attr_suffix);
		} else {
			strcpy(map_index, (char *) index.str());
			strcat(map_index, FNSP_internal_name_seperator);
			strcat(map_index, local_name);
			strcat(map_index, FNSP_attr_suffix);
		}
	}
	return (FN_SUCCESS);
}

static const FN_identifier
    ascii_syntax((unsigned char *) "fn_attr_syntax_ascii");
static const FN_identifier
    text_syntax((unsigned char *) "fn_attr_syntax_text");

static unsigned
FNSP_nis_update_attribute_hostuser_map(const FNSP_Address &parent,
    const FN_string &name, const FN_attribute &attr, int op)
{
	// Make sure the syntax of the attribute is ASCII
	const FN_identifier *syntax = attr.syntax();

	// Get the domainname, host/user name etc.
	char map_index[FNS_NIS_INDEX];
	FN_string *map, *domain;
	unsigned status = FNSP_nis_attr_construct_names(parent,
	    name, map_index, &map, &domain);
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
	// status = FNSP_nis_update_attrval_hostuser_map(
	// hu_map, (char *) domain->str(),
	// map_index, (char *) id->str(), 0, op);
	// if (status != FN_SUCCESS) {
	// delete map;
	// delete domain;
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
		status = FNSP_nis_update_attrval_hostuser_map(
		    hu_map, (char *) domain->str(),
		    map_index, (char *) id->str(),
		    value_char, op);
		delete value_str;
		if (status != FN_SUCCESS)
			break;
	}
	delete domain;
	delete map;
	return (status);
}

static unsigned
FNSP_nis_update_attrset_hostuser_map(const FNSP_Address &parent,
    const FN_string &name, const FN_attrset &attrset, int op)
{
	unsigned status = FN_SUCCESS;
	void *ip;
	const FN_attribute *attr;
	for (attr = attrset.first(ip);
	    attr != NULL;
	    attr = attrset.next(ip)) {
		status = FNSP_nis_update_attribute_hostuser_map(
		    parent, name, *attr, op);
		if (status != FN_SUCCESS)
			break;
	}
	return (status);
}

static const char *passwd_map = "passwd.byname";
static const char *hosts_map = "hosts.byname";
static const char *alias_map = "mail.aliases";

static FN_string *
FNSP_nis_get_homedir(const FNSP_Address &parent,
    const FN_string &username,
    unsigned &status)
{
	const unsigned char *user = username.str(&status);
	if (user == NULL) {
		return (NULL);
	}
	FN_string *map, *domain;
	status = FNSP_nis_split_internal_name(parent.get_table_name(),
	    &map, &domain);
	if (status != FN_SUCCESS) {
		return (NULL);
	}
	delete map;
	const unsigned char *dom = domain->str(&status);
	if (dom == NULL) {
		delete domain;
		return (NULL);
	}
	char *pw_entry;
	int pw_entrylen;
	status = FNSP_yp_map_lookup((char *)dom, (char *)passwd_map,
	    (char *)user, username.charcount(), &pw_entry, &pw_entrylen);

	delete domain;
	if (status != FN_SUCCESS) {
		status = FN_E_CONFIGURATION_ERROR;
		return (NULL);
	}
	FN_string *homedir = FNSP_homedir_from_passwd_entry(pw_entry, status);
	free(pw_entry);
	return (homedir);
}

static FN_attribute *
FNSP_get_user_mail_attribute(const char *username, const char *domainname)
{
	FN_attribute *attribute;
	unsigned status;
	char *answer = 0, *mapentry;
	int maplen;

	char *lookup_name = strdup(username);
	while ((status = FNSP_yp_map_lookup((char *) domainname,
	    (char *) alias_map, lookup_name, (int) (strlen(lookup_name) + 1),
	    &mapentry, &maplen)) == FN_SUCCESS) {
		free(lookup_name);
		lookup_name = (char *) calloc(maplen+1, sizeof(char));
		strncpy(lookup_name, mapentry, maplen);
		free(mapentry);
		if (answer)
			free (answer);
		answer = strdup(lookup_name);
	}
	free (lookup_name);
	if (answer == 0)
		return (0);
	attribute = FNSP_get_builtin_mail_attribute(answer, status);
	free (answer);
	return (attribute);
}

static FN_attrset *
FNSP_nis_get_builtin_attrset(const FNSP_Address &parent,
    const FN_string &atomic_name, unsigned &status)
{
	status = FN_SUCCESS;
	int user_ctx = 0;

	// Check if the builtin attributes exists
	if (FNSP_does_builtin_attrset_exist(parent, atomic_name) == 0)
		return (0);

	char map_index[FNS_NIS_INDEX];
	FN_string *map, *domain;
	status = FNSP_nis_attr_construct_names(parent,
	    atomic_name, map_index, &map, &domain);
	if (status != FN_SUCCESS)
		return (0);
	// Remove the attribute suffix
	map_index[strlen(map_index) -
	    strlen(FNSP_attr_suffix)] = '\0';

	// Do a yp_lookup
	char *mapentry;
	int maplen;

	// Fix the mapname
	delete map;
	if (FNSP_is_hostuser_ctx_type(parent, atomic_name) == 1) {
		user_ctx = 1;
		map = new FN_string((unsigned char *) passwd_map);
	} else
		map = new FN_string((unsigned char *) hosts_map);

	if ((status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen)) != FN_SUCCESS) {
		status = FN_SUCCESS;
		delete domain;
		delete map;
		return (0);
	} else if (mapentry == NULL) {
		delete domain;
		delete map;
		return (0);
	}

	FN_attrset *answer;
	char entry[FNS_NIS_SIZE];
	strncpy(entry, mapentry, maplen);
	free(mapentry);
	entry[maplen] = '\0';
	if (user_ctx)
		answer  = FNSP_get_user_builtin_attrset(
		    entry, entry, status);
	else
		answer = FNSP_get_host_builtin_attrset(
		    map_index, entry, status);
	if (user_ctx) {
		/* FN_attribute *mail_attribute =
		    FNSP_get_user_mail_attribute(map_index,
		    (char *) domain->str());
		if (mail_attribute)
			answer->add(*mail_attribute);
		delete mail_attribute; */
	}
	delete domain;
	delete map;
	return (answer);
}

FN_attrset *
FNSP_nisImpl::get_attrset(
    const FN_string &atomic_name,
    unsigned &status)
{
	char map_index[FNS_NIS_INDEX];
	FN_string *map, *domain;

	status = FNSP_nis_attr_construct_names(*my_address, atomic_name,
	    map_index, &map, &domain);
	if (status != FN_SUCCESS)
		return (0);

	char *mapentry;
	int maplen;
	status = FNSP_yp_map_lookup((char *) domain->str(),
	    (char *) map->str(), map_index, strlen(map_index),
	    &mapentry, &maplen);

	if (status == FN_E_NAME_NOT_FOUND) {
		// Check if the binding exists
		FN_ref *ref = lookup_binding(atomic_name, status);
		if (status == FN_SUCCESS) {
			status = FN_E_NO_SUCH_ATTRIBUTE;
			delete ref;
		}
	}
	delete map;
	delete domain;

	FN_attrset *attrset;
	if (status == FN_SUCCESS) {
		attrset = FNSP_nis_attrset_deserialize(mapentry,
		    maplen, status);
		free(mapentry);
		if (status != FN_SUCCESS)
			return (0);
	} else if (status == FN_E_NO_SUCH_ATTRIBUTE) {
		attrset = 0;
		status = FN_SUCCESS;
	} else
		return (0);

	return (attrset ? attrset : new FN_attrset);

	FN_attrset *builtin = FNSP_nis_get_builtin_attrset(*my_address,
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
FNSP_nis_modify_set_attrset(const FNSP_Address &parent,
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

	char map_index[FNS_NIS_INDEX];
	FN_string *map, *domain;
	status = FNSP_nis_attr_construct_names(parent, atomic_name,
	    map_index, &map, &domain);
	if (status != FN_SUCCESS)
		return (status);

	if (attrset.count() == 0) {
		status = FNSP_update_map((char *) domain->str(),
		    (char *) map->str(), map_index, 0,
		    FNSP_map_delete);
	} else {
		char *attrbuf =
		    FNSP_nis_attrset_serialize(attrset, status);
		if (status != FN_SUCCESS) {
			delete map;
			delete domain;
			return (status);
		}
		status = FNSP_update_map((char *) domain->str(),
		    (char *) map->str(), map_index, attrbuf,
		    FNSP_map_store);
		free(attrbuf);
	}
	delete map;
	delete domain;
	return (status);
}

int
FNSP_nisImpl::set_attrset(
    const FN_string &atomic_name,
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

	status = FNSP_nis_modify_set_attrset(*my_address, atomic_name,
	    attrset);

	// If host or user context type, attributes must be
	// added to seperate maps
	if ((status == FN_SUCCESS) &&
	    FNSP_is_hostuser_ctx_type(*my_address, atomic_name)) {
		status = FNSP_nis_update_attrset_hostuser_map(*my_address,
		    atomic_name, attrset, FNSP_map_store);
	}

	return (status);
}

int
FNSP_nisImpl::delete_attrset(const FN_string &atomic_name)
{
	unsigned status = FN_SUCCESS;

	// If host or user context type, attributes must be
	// deleted from the seperate maps
	FN_attrset *attrset = 0;
	int hostuser_ctx = 0;
	unsigned hu_status;
	if (FNSP_is_hostuser_ctx_type(*my_address, atomic_name)) {
		attrset  = get_attrset(atomic_name, hu_status);
		hostuser_ctx = 1;
	}

	char map_index[FNS_NIS_INDEX];
	FN_string *map, *domain;

	status = FNSP_nis_attr_construct_names(*my_address, atomic_name,
	    map_index, &map, &domain);
	if (status != FN_SUCCESS) {
		delete attrset;
		return (status);
	}

	status = FNSP_update_map((char *) domain->str(),
	    (char *) map->str(), map_index, 0, FNSP_map_delete);
	delete map;
	delete domain;
	if (status != FN_SUCCESS) {
		delete attrset;
		return (status);
	}

	if ((hostuser_ctx) && (hu_status == FN_SUCCESS)) {
		status = FNSP_remove_builtin_attrset(*attrset);
		if (status == FN_SUCCESS) {
			status = FNSP_nis_update_attrset_hostuser_map(
			    (*my_address), atomic_name, *attrset,
			    FNSP_map_delete);
		}
		delete attrset;
	}
	return (status);
}

int
FNSP_nisImpl::modify_attribute(
    const FN_string &atomic_name,
    const FN_attribute &attr,
    unsigned flags)
{
	unsigned status;
	void *ip;
	int howmany, i;

	// Check if for builtin attributes
	status = FNSP_check_builtin_attribute(*my_address,
	    atomic_name, attr);
	if (status != FN_SUCCESS)
		return (status);

	// First get the attribute set
	FN_attrset *aset = get_attrset(atomic_name, status);

	// Check for errors
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

	// Make a copy of the attribute set
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
			delete delattr;
			status = FN_E_INSUFFICIENT_RESOURCES;
			break;
		}
		// First change the attributes in FNS tables
		if ((status = FNSP_nis_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS) {
			delete delattr;
			break;
		}
		// If host or user context type, attributes must be
		// added to seperate maps.
		if (FNSP_is_hostuser_ctx_type(*my_address,
		    atomic_name)) {
			// First delete the old attributes, if present
			if (delattr) 
				FNSP_nis_update_attribute_hostuser_map(
				    *my_address, atomic_name, *delattr,
				    FNSP_map_delete);
			status = FNSP_nis_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_store);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_nis_modify_set_attrset(*my_address,
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

		// First change the attributes in FNS tables
		if ((status = FNSP_nis_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS)
			break;
		// Change in host/user special maps
		if (FNSP_is_hostuser_ctx_type(*my_address, atomic_name)) {
			status = FNSP_nis_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_insert);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_nis_modify_set_attrset(*my_address,
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

		// First change the attributes in FNS tables
		if ((status = FNSP_nis_modify_set_attrset(*my_address,
		    atomic_name, (*aset))) != FN_SUCCESS)
			break;
		// Change in host/user special maps
		if (FNSP_is_hostuser_ctx_type(*my_address,
		    atomic_name)) {
			status = FNSP_nis_update_attribute_hostuser_map(
			    *my_address, atomic_name, attr, FNSP_map_store);
			if (status != FN_SUCCESS)
				// Copy back the old attributes
				FNSP_nis_modify_set_attrset(*my_address,
		    		    atomic_name, old_aset);
		}
	}
	break;

	case FN_ATTR_OP_REMOVE:
	case FN_ATTR_OP_REMOVE_VALUES: {
		const FN_identifier *attr_id = attr.identifier();
		const FN_attribute *old_attr = aset->get(*attr_id);
		const FN_attrvalue *attr_value;

		// Cehck for empty attribute values
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

		// First change the attributes in FNS tables
		if (aset->count() > 0) {
			if ((status = FNSP_nis_modify_set_attrset(*my_address,
			    atomic_name, (*aset))) != FN_SUCCESS)
				break;

			// Delete the entries from HU special maps if they exist
			if (FNSP_is_hostuser_ctx_type(*my_address,
			    atomic_name)) {
				if (flags == FN_ATTR_OP_REMOVE) {
					old_attr = old_aset.get(*attr_id);
					status =
					    FNSP_nis_update_attribute_hostuser_map(
					    *my_address, atomic_name, *old_attr,
					    FNSP_map_delete);
				} else
					status =
					    FNSP_nis_update_attribute_hostuser_map(
					    *my_address, atomic_name, attr,
					    FNSP_map_delete);

				if (status != FN_SUCCESS) {
					// Retrive the old attrset in FNS tables
					FNSP_nis_modify_set_attrset(*my_address,
					    atomic_name, old_aset);
				}
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
FNSP_nis_hu_search_attrval(char *domain, char *map,
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
		strcat(map_index, (char *) FNSP_internal_name_seperator);
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
	status = FNSP_yp_map_lookup(domain, map, map_index,
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
FNSP_nis_hu_search_attribute(char *domain, char *map,
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
	const FN_string *ns_string;
	FN_string *value_string;
	char *value_char;
	FN_attrvalue *text_value;

	// If first *value* is NULL search for *id* existance
	value = attr.first(ip);
	if (value == NULL) {
		answer = FNSP_nis_hu_search_attrval(domain, map, *id,
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
		ns = FNSP_nis_hu_search_attrval(domain, map, *id, value,
		    status);
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

class FN_nis_searchlist_hu : public FN_searchlist {
	FN_bindinglist *bl;
	unsigned int return_ref;
	FN_attrset *return_attr_id;
	FNSP_nisImpl *impl;
public:
	FN_nis_searchlist_hu(FNSP_nisImpl *i, FN_bindinglist *b,
	    unsigned int rr, const FN_attrset *ra);
	~FN_nis_searchlist_hu();
	FN_string *next(FN_ref **ref, FN_attrset **attrset,
	    FN_status &status);
};

FN_nis_searchlist_hu::FN_nis_searchlist_hu(FNSP_nisImpl *i,
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

FN_nis_searchlist_hu::~FN_nis_searchlist_hu()
{
	delete bl;
	delete return_attr_id;
	delete impl;
}

FN_string *
FN_nis_searchlist_hu::next(FN_ref **ref, FN_attrset **attrset,
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
FNSP_nisImpl::search_attrset_hu(
    const FN_attrset *attrset, unsigned int return_ref,
    const FN_attrset *return_attr_id, unsigned int &status)
{
	// Currently no support for searching builtin attrs
	if ((attrset) && (FNSP_is_attrset_in_builtin_attrset(*attrset))) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);
	}

	// Get map name
	char map[FNS_NIS_INDEX];
	int thisorgunit_map = 0;
	if (my_address->get_context_type() == FNSP_hostname_context)
		strcpy(map, FNSP_host_attr_map);
	else if (my_address->get_context_type() == FNSP_username_context)
		strcpy(map, FNSP_user_attr_map);
	else {
		strcpy(map, FNSP_thisorgunit_attr_map);
		thisorgunit_map = 1;
	}

	// Get domainname
	char domain[FNS_NIS_INDEX];
	FN_string *domainname, *mapname;
	status = FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &mapname, &domainname);
	if (status != FN_SUCCESS)
		return (0);
	strcpy(domain, (char *) domainname->str());
	delete mapname;
	delete domainname;

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
		FNSP_nisImpl *newImpl = new FNSP_nisImpl(
		    new FNSP_nisAddress((FNSP_nisAddress *)
		    my_address));
		return (new FN_nis_searchlist_hu(newImpl,
		    bl, return_ref, return_attr_id));
	}

	// Get the name set for each attribute
	const FN_attribute *attr;
	void *ip, *ip_ns;
	FN_nameset *ns, *copy, *answer = 0;
	int initial = 0;
	const FN_string *ns_string;
	for (attr = attrset->first(ip); attr != 0;
	    attr = attrset->next(ip)) {
		ns = FNSP_nis_hu_search_attribute(domain, map, *attr, status);
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
			// obtain the correct name
			size_t parent_index_len = strlen((char *)
			    (my_address->get_index_name()).str()) +
			    strlen(FNSP_internal_name_seperator);
			char parent_index_name[FNS_NIS_INDEX];
			strcpy(parent_index_name, (char *) ns_string->str());
			if (strlen(parent_index_name) < parent_index_len)
				continue;
			char *lookup_name = &parent_index_name[parent_index_len];
			nss_string = new FN_string((unsigned char *) lookup_name);
			ns_string = nss_string;
		} else {
			// Host name and user name must be normalized
			char hu_name[FNS_NIS_INDEX];
			strcpy(hu_name, (char *) ns_string->str());
			FNSP_normalize_name(hu_name);
			nss_string = new FN_string((unsigned char *) hu_name);
			ns_string = nss_string;
		}

		// Make sure binding exist in NIS
		ss_ref = FNSP_lookup_binding_aux(*my_address,
		    *ns_string, status);
		if (status != FN_SUCCESS) {
			// Bindings do not exists, drop the name
			delete nss_string;
			status = FN_SUCCESS;
			continue;
		}
		if (!return_ref) {
			delete ss_ref;
			ss_ref = 0;
		}

		ss_attrset = get_attrset(*ns_string, status);
		if (status != FN_SUCCESS) {
			delete nss_string;
			delete ss_attrset;
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
FNSP_nisImpl::search_attrset(
    const FN_attrset *old_attrset,
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
	FN_namelist *ns = list_names(status);
	if (!ns || (status != FN_SUCCESS))
		return (0);

	// Allocate for searchset
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

	FN_status sstat;
	FN_attrset *name_attrset;
	FN_ref *ref;
	FN_attrset *request_attrset;
	FN_string *name;
	while (name = ns->next(sstat)) {
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
		delete name;
	}
	delete attrset;
	delete ns;
	if ((status != FN_SUCCESS) ||
	    ((ss != NULL && ss->count() == 0))) {
		delete ss;
		return (0);
	}

	return (new FN_searchlist_svc(ss));
}

int
FNSP_nisImpl::is_org_context()
{
	// Get the index name from address
	char index_name[FNS_NIS_INDEX];
	const FN_string index = my_address->get_index_name();

	// Construct the index name for "service"
	sprintf(index_name, "%s%s%s%s%s", FNSP_self.str(),
	    FNSP_internal_name_seperator, "service",
	    FNSP_internal_name_seperator, "printer");
	if (strncasecmp((char *) index.str(),
	    index_name, strlen(index_name)) == 0)
		return (1);

	// Construct for "_service"
	sprintf(index_name, "%s%s%s%s%s", FNSP_self.str(),
	    FNSP_internal_name_seperator, "_service",
	    FNSP_internal_name_seperator, "printer");
	if (strncasecmp((char *) index.str(),
	    index_name, strlen(index_name)) == 0)
		return (1);

	// Check the table name, if the table name
	// has only the domainname ie., map = ""
	// the it is an organization context
	FN_string *map, *domain;
	if (FNSP_nis_split_internal_name(my_address->get_table_name(),
	    &map, &domain) == FN_SUCCESS) {
		delete domain;
		// Check if map is empty
		if ((map) &&
		    (map->compare((unsigned char *) "") == 0)) {
			delete map;
			return (1);
		}
		delete map;
	}

	return (0);
}

FN_ref *
FNSP_nisImpl::get_nns_ref()
{
	return (FNSP_reference(FNSP_nis_address_type_name(),
	    my_address->get_internal_name(), FNSP_nsid_context));
}
