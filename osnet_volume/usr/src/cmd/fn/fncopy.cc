/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncopy.cc	1.12	97/03/05 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <xfn/xfn.hh>
#include <xfn/fn_p.hh>
#include <xfn/fnselect.hh>
#include <xfn/FN_nameset.hh>
#include <xfn/FN_namelist_svc.hh>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libintl.h> // gettext
#include "fncreate_attr_utils.hh"
#include "fncopy_utils.hh"

/*
 *
 * The following are being used from fnsp_internal.cc.
 * Some other source organization (such as a small utilities object)
 * might obviate the need to include the need to link against the .so.
 *
 * FNSP_orgname_of()
 * FNSP_nis_orgname_of()
 *
 */

extern FN_string *
FNSP_orgname_of(const FN_string &internal_name, unsigned &status,
    int org = 0);

extern FN_string *
FNSP_nis_orgname_of(FN_string &name, unsigned &status);

// Global variables
extern int verbose = 0;
static FN_composite_name *source = 0;
static FN_ctx *src_initial_ctx;
static FN_composite_name *destination = 0;
static FN_ctx *dst_initial_ctx;
extern int source_ns = 0;
extern int destination_ns = 0;
static int read_from_file = 0;
static char *file_name = 0;
extern char *program_name = 0;
extern unsigned global_bind_flags = FN_OP_EXCLUSIVE;
static int return_value = 0;
static int follow_link = 0;

// Variable that maintains the context that is
// being created. Initially it is empty
static FN_composite_name current_path;

#define	AUTHORITATIVE 1
#define	MAXINPUTLEN 1024

// Required to get the real internal name of user's fs type
int _fn_fs_user_pure = 1;

enum FNSP_ctx_type {
	context = 1,
	context_binding = 2,
	binding = 3,
	fs_context_user = 4,
	fs_context_host = 5,
	fs_context_mount = 6
};

typedef struct FNSP_ctx_info {
	FNSP_ctx_type type;		// Context or Binding
	struct FNSP_ctx_info *next;	// Peer contexts
	struct FNSP_ctx_info *sub_ctx;	// Sub Contexts
	FN_composite_name *name;
	FN_attrset *attrset;

	// Information about Contexts
	const FN_identifier *ref_type;
	unsigned ctx_type;
	unsigned repr_type;
	unsigned version;
	FN_ref *ref;

	// If context_binding or binding_binding, bind name
	FN_composite_name *b_name;
} FNSP_ctx_info_t;

static const FN_composite_name empty_name((unsigned char *) "");
static const FN_string nns_name((unsigned char *) "");
static const FN_string NISPLUS_separator((unsigned char *)".");

static void
delete_all_static_variables()
{
	delete source;
	delete src_initial_ctx;
	delete destination;
	delete dst_initial_ctx;
}

void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, msg);
	fprintf(stderr, gettext("Usage:\t%s [ -v ] [ -s ] "
	    "[ -i old-name-service] [ -o new-name-service ]\n\t\t"
	    "[ -f filename ] old-fns-context"
	    "new-fns-context\n"), cmd);
	exit(1);
}

static const char *fns_nisplus_ns = "nisplus";
static const char *fns_nis_ns = "nis";
static const char *fns_files_ns = "files";

static int
FNSP_get_destination_ns(char *ns)
{
	if (strcmp(ns, fns_nisplus_ns) == 0)
		return (FNSP_nisplus_ns);
	if (strcmp(ns, fns_nis_ns) == 0)
		return (FNSP_nis_ns);
	if (strcmp(ns, fns_files_ns) == 0)
		return (FNSP_files_ns);
	return (FNSP_unknown_ns);
}

static int
FNSP_get_source_ns(char *ns)
{
	if (strcmp(ns, fns_nisplus_ns) == 0)
		return (FNSP_nisplus_ns);
	if (strcmp(ns, fns_nis_ns) == 0)
		return (FNSP_nis_ns);
	if (strcmp(ns, fns_files_ns) == 0)
		return (FNSP_files_ns);
	return (FNSP_unknown_ns);
}

void
process_cmd_line(int argc, char **argv)
{
	program_name = argv[0];
	int c;
	while ((c = getopt(argc, argv, "vsi:o:f:")) != -1) {
		switch (c) {
		case 'o' :
			// Destination naming service
			destination_ns =
			    FNSP_get_destination_ns(optarg);
			if (destination_ns == FNSP_unknown_ns)
				usage(argv[0],
				    gettext("Illegal destination "
				    "naming service"));
			break;
		case 's':
			global_bind_flags = 0;
			break;
		case 'i':
			// Source naming service
			source_ns = FNSP_get_source_ns(optarg);
			if (source_ns == FNSP_unknown_ns)
				usage(argv[0],
				    gettext("Illegal source naming service"));
			break;
		case 'f':
			read_from_file = 1;
			file_name = (optarg ? strdup(optarg) : 0);
			if (file_name == 0)
				usage(argv[0], gettext("File name incorrect"));
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(argv[0], gettext("Illegal option"));
			break;
		}
	}

	// If file name is present, check it
	struct stat buffer;
	if ((read_from_file) &&
	    (stat(file_name, &buffer) != 0))
		usage(argv[0], gettext("File not found"));

	// Check for number of arguments
	if ((argc - optind) != 2)
		usage(argv[0], gettext("Incorrect number of arguments"));

	source = new FN_composite_name((unsigned char *)
	    argv[optind++]);
	destination = new FN_composite_name((unsigned char *)
	    argv[optind]);
}

// Ref identifier for files context type
extern const FN_identifier
FNSP_fs_reftype((const unsigned char *)"onc_fn_fs");

static const int FNSP_num_of_ctxs = 14;
static const FN_identifier *FNSP_ctx_identifiers[FNSP_num_of_ctxs];

static void
fnsp_get_std_ctx_identifiers()
{
	int i;
	for (i = 0; i < 13; i++) {
		FNSP_ctx_identifiers[i] =
		    FNSP_reftype_from_ctxtype(i+1);
	}

	FNSP_ctx_identifiers[i] = &FNSP_fs_reftype;
}

static unsigned
get_context_type(const FN_identifier &ref_id)
{
	unsigned ctx_type;

	for (int i = 0; i < FNSP_num_of_ctxs; i++) {
		if (ref_id == *(FNSP_ctx_identifiers[i])) {
			ctx_type = i;
			break;
		}
	}
	ctx_type++;
	if (ctx_type == 14)
		// FS ref type, hence make generic context
		ctx_type = FNSP_generic_context;
	return (ctx_type);
}

static void
check_status(const FN_status &status, char *msg = 0)
{
	if (!status.is_success()) {
		FN_string *desc = status.description();
		if (msg)
			fprintf(stderr, "%s: %s: %s\n", program_name, msg,
			    desc? (char *)(desc->str()): "");
		else
			fprintf(stderr, "%s: %s\n", program_name,
			    desc? (char *)(desc->str()): "");
		if (desc)
			delete desc;
		exit(1);
	}
}

// Address types
static const FN_identifier
FNSP_nisplus_addr_type((const unsigned char *) "onc_fn_nisplus");
static const FN_identifier
FNSP_nis_addr_type((const unsigned char *) "onc_fn_nis");
static const FN_identifier
FNSP_files_addr_type((const unsigned char *) "onc_fn_files");
static const FN_identifier
FNSP_nisplus_ptr_addr_type((const unsigned char *) "onc_fn_printer_nisplus");
static const FN_identifier
FNSP_nis_ptr_addr_type((const unsigned char *) "onc_fn_printer_nis");
static const FN_identifier
FNSP_files_ptr_addr_type((const unsigned char *) "onc_fn_printer_files");

// Address types for files context
extern const FN_identifier
FNSP_nisplus_user_fs_addr((const unsigned char *) "onc_fn_fs_user_nisplus");
extern const FN_identifier
FNSP_user_fs_addr((const unsigned char *) "onc_fn_fs_user");
extern const FN_identifier
FNSP_fs_host_addr((const unsigned char *) "onc_fn_fs_host");
static const FN_identifier
FNSP_fs_mount_addr((const unsigned char *) "onc_fn_fs_mount");

static int
is_source_address_type(const FN_ref_addr &addr)
{
	const FN_identifier *address = addr.type();

	// Check for the common address types
	if (((*address) == FNSP_user_fs_addr) ||
	    ((*address) == FNSP_fs_host_addr))
		return (1);

	// Check for name service specific address types
	switch (source_ns) {
	case FNSP_nisplus_ns:
		if (((*address) == FNSP_nisplus_addr_type) ||
		    ((*address) == FNSP_nisplus_ptr_addr_type) ||
		    ((*address) == FNSP_nisplus_user_fs_addr))
			return (1);
	case FNSP_nis_ns:
		if (((*address) == FNSP_nis_addr_type) ||
		    ((*address) == FNSP_nis_ptr_addr_type))
			return (1);
	case FNSP_files_ns:
		if (((*address) == FNSP_files_addr_type) ||
		    ((*address) == FNSP_files_ptr_addr_type))
			return (1);
	default:
		break;
	}
	return (0);
}

// Check for context type, returns 0 if not a context
static const FN_identifier *
get_context_identifier(const FN_ref &ref)
{
	// Check if the ref has source address type
	const FN_ref_addr *addr;
	void *ip;
	for (addr = ref.first(ip); addr; addr = ref.next(ip)) {
		if (is_source_address_type(*addr))
			break;
	}
	if (addr == 0)
		return (0);

	// Get the context type
	const FN_identifier *ref_id = ref.type();

	// Check if the reference of the std ref. types
	for (int i = 0; i < FNSP_num_of_ctxs; i++) {
		if ((*ref_id) == *(FNSP_ctx_identifiers[i]))
			return (FNSP_ctx_identifiers[i]);
	}
	return (0);
}

// Aliases names
static const FN_string FNSP_service_string((unsigned char *)"service");
static const FN_string FNSP_service_string_sc((unsigned char *)"_service");
static const FN_string FNSP_host_string((unsigned char *)"host");
static const FN_string FNSP_host_string_sc((unsigned char *)"_host");
static const FN_string FNSP_user_string((unsigned char *)"user");
static const FN_string FNSP_user_string_sc((unsigned char *)"_user");
static const FN_string FNSP_site_string_sc((unsigned char *)"_site");
static const FN_string FNSP_site_string((unsigned char *)"site");
// static const FN_string FNSP_fs_string_sc((unsigned char *)"_fs");
// static const FN_string FNSP_fs_string((unsigned char *)"fs");

// Returns 1 if it a binding entry
static int
check_if_binding_entry(const FN_ref & /* ref */,
    const FN_string &name)
{
	// Checking of reference types address types are
	// done by get_context_ideintifier

	// Check if the name is one of the alias names
	if ((name.compare(FNSP_service_string_sc) == 0) ||
	    (name.compare(FNSP_host_string_sc) == 0) ||
	    (name.compare(FNSP_user_string_sc) == 0) ||
	    (name.compare(FNSP_site_string_sc) == 0))
		return (1);
	else
		return (0);
}

FN_composite_name *
get_context_binding_name(const FN_string &name)
{
	FN_composite_name *answer = 0;
	if (name.compare(FNSP_service_string_sc) == 0)
		answer = new FN_composite_name(FNSP_service_string);
	if (name.compare(FNSP_host_string_sc) == 0)
		answer = new FN_composite_name(FNSP_host_string);
	if (name.compare(FNSP_user_string_sc) == 0)
		answer = new FN_composite_name(FNSP_user_string);
	if (name.compare(FNSP_site_string_sc) == 0)
		answer = new FN_composite_name(FNSP_site_string);
	// if (name.compare(FNSP_fs_string_sc) == 0)
	// answer = new FN_composite_name(FNSP_fs_string);

	return (answer);
}

static FNSP_ctx_info *
get_ctx_info_from_ref(const FN_ref &ref, const FN_string &name)
{
	FNSP_ctx_info *new_ctx_info = (FNSP_ctx_info *)
	    malloc(sizeof (struct FNSP_ctx_info));
	if (!new_ctx_info) {
		fprintf(stderr, gettext("Insufficient resources: "
		    "Unable to malloc\n"));
		return (0);
	}
	new_ctx_info->next = 0;
	new_ctx_info->sub_ctx = 0;
	new_ctx_info->name = new FN_composite_name(name);
	if (!new_ctx_info->name) {
		fprintf(stderr, gettext("Insufficient resources: "
		    "Unable to malloc\n"));
		free(new_ctx_info);
		return (0);
	}
	new_ctx_info->version = 0;
	new_ctx_info->ref = 0;
	new_ctx_info->attrset = 0;
	new_ctx_info->ref_type = 0;
	new_ctx_info->b_name = 0;

	// Get identifier, if it exists
	const FN_identifier *id = get_context_identifier(ref);
	if (!id) {
		// Binding information
		new_ctx_info->type = binding;
		new_ctx_info->ref = new FN_ref(ref);
		if (!new_ctx_info->ref) {
			fprintf(stderr, gettext("Insufficient resources: "
			    "Unable to malloc\n"));
			return (0);
		}
		return (new_ctx_info);
	}

	// Check if the name is a binding entry
	void *ip;
	const FN_ref_addr *addr;
	if (check_if_binding_entry(ref, name)) {
		// Binding to a context
		new_ctx_info->ref_type = id;
		new_ctx_info->type = context_binding;
		new_ctx_info->b_name = get_context_binding_name(name);
	} else {
		// Context
		new_ctx_info->type = context;
		new_ctx_info->ref_type = id;
		new_ctx_info->ctx_type = get_context_type(*id);
		new_ctx_info->repr_type = FNSP_normal_repr;

		// Check if users' (or) hosts' fs context
		if ((*id) == FNSP_fs_reftype) {
			// Determine the type for fs context
			// Assume the default to be user
			new_ctx_info->type = fs_context_user;

			// Check for regular fs context
			for (addr = ref.first(ip); addr;
			    addr = ref.next(ip)) {
				if (((*addr->type()) ==
				    FNSP_nisplus_addr_type) ||
				    ((*addr->type()) ==
				    FNSP_nis_addr_type) ||
				    ((*addr->type()) ==
				    FNSP_files_addr_type)) {
					// Regular FS-FNS context
					new_ctx_info->type = context;
					break;
				}
			}

			// Check for mount reference and host reference
			if (new_ctx_info->type == fs_context_user) {
				new_ctx_info->ref = new FN_ref(ref);
				if (!new_ctx_info->ref) {
					fprintf(stderr,
					    gettext("Insufficient "
					    "resources: "
					    "Unable to malloc\n"));
					free(new_ctx_info);
					return (0);
				}
				for (addr = ref.first(ip); addr;
				    addr = ref.next(ip)) {
					if ((*addr->type()) ==
					    FNSP_fs_mount_addr) {
						new_ctx_info->type =
						    fs_context_mount;
						break;
					} else if ((*addr->type()) ==
					    FNSP_fs_host_addr) {
						new_ctx_info->type =
						    fs_context_host;
						break;
					}
				}
			}
		}
	}

	// Check for additional addresses
	if (ref.addrcount() == 1)
		return (new_ctx_info);

	// Obtain additional address information
	for (addr = ref.first(ip); addr; addr = ref.next(ip)) {
		if (!is_source_address_type(*addr)) {
			if (new_ctx_info->ref == 0)
				new_ctx_info->ref = new
				    FN_ref(*id);
			new_ctx_info->ref->append_addr(*addr);
			}
	}
	return (new_ctx_info);
}

// Function to delete all the context information
// Sub contexts and next pointers
void
delete_all_context_info(FNSP_ctx_info *ctx_info)
{
	if (!ctx_info)
		return;

	delete ctx_info->name;
	delete ctx_info->ref;
	delete ctx_info->b_name;

	// Traverse subcontexts
	if (ctx_info->sub_ctx)
		delete_all_context_info(ctx_info->sub_ctx);

	// Traverse the peer contexts
	if (ctx_info->next)
		delete_all_context_info(ctx_info->next);

	// Free itself
	free(ctx_info);
}

// Functions to update and delete components of current_path
static void
update_current_path(FN_composite_name &name)
{
	FN_string *str = name.string();
	if (str)
		current_path.append_comp(*str);
	delete str;
}

static void
delete_last_current_path()
{
	void *ip;
	current_path.last(ip);
	current_path.next(ip);
	current_path.delete_comp(ip);
}

// Builtin attributes
// User builtin attributes
static const FN_identifier
    user_attr_passwd((unsigned char *) "onc_unix_passwd");
static const FN_identifier
    user_attr_shadow((unsigned char *) "onc_unix_shadow");
// Host builtint attributes
static const FN_identifier
    host_attr_name((unsigned char *) "onc_host_name");
static const FN_identifier
    host_attr_aliases((unsigned char *) "onc_host_aliases");
static const FN_identifier
    host_attrs((unsigned char *) "onc_host_ip_addresses");
static const FN_identifier
    ascii((unsigned char *)"fn_attr_syntax_ascii");

static FN_attrset *
get_attrset(FN_ctx *ctx, const FN_composite_name &name)
{
	FN_status status;
	FN_attrset *attrset_ids /* , *attrset */;

	attrset_ids = ctx->attr_get_ids(name, follow_link, status);
	if (attrset_ids == 0)
		return (0);

	// Get the attributes
	// Since the implementation of onc_ns
	// returns attributes also, just return attr_ids

	// Delete builtin attributes
	attrset_ids->remove(host_attr_name);
	attrset_ids->remove(host_attr_aliases);
	attrset_ids->remove(host_attrs);
	attrset_ids->remove(user_attr_passwd);
	attrset_ids->remove(user_attr_shadow);
	return (attrset_ids);
}

// Recursive function being called to construct the
// source structure. Return 1 if SUCCESS and 0 if failed
int
construct_sub_ctx_and_bindings(const FN_ref *ref, FNSP_ctx_info *init)
{
	if ((init) && (init->type != context))
		return (1);

	FN_status status;
	FN_string *path_str;
	FN_string *desc;

	// Get the context and list names
	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
	if (!status.is_success()) {
		path_str = current_path.string();
		fprintf(stderr, gettext("Unable to get context from "
		    "reference: %s\n"),
		    path_str ? (char *) path_str->str() : "");
		desc = status.description();
		if (desc)
			fprintf(stderr, "%s: %s\n", program_name,
			    desc->str());
		delete path_str;
		delete desc;
		return (0);
	}

	FN_namelist *names = ctx->list_names(empty_name, status);
	if (!status.is_success()) {
		path_str = current_path.string();
		fprintf(stderr, gettext("Unable to list names of "
		    "context: %s\n"),
		    path_str ? (char *) path_str->str() : "");
		desc = status.description();
		if (desc)
			fprintf(stderr, "%s: %s\n", program_name,
			    desc->str());
		delete path_str;
		delete desc;
		delete ctx;
		return (0);
	}

	// If the context is of site context add "" to the
	// list of sub-contexts
	FN_string *sub_ctx;
	FN_nameset *nameset;
	if (init->ctx_type == FNSP_site_context) {
		nameset = new FN_nameset;
		if (!nameset) {
			fprintf(stderr, gettext("Insufficient resources: "
			    "Unable to malloc\n"));
			delete ctx;
			delete names;
			return (0);
		}
		nameset->add(nns_name);
		while ((sub_ctx = names->next(status))) {
			nameset->add(*sub_ctx);
			delete sub_ctx;
		}
		delete names;
		names = new FN_namelist_svc(nameset);
		if (!names) {
			fprintf(stderr, gettext("Insufficient resources: "
			    "Unable to malloc\n"));
			delete ctx;
			return (0);
		}
	}

	// Travese the context lists
	FN_ref *sub_ref;
	FNSP_ctx_info *sub_ctx_info;
	FNSP_ctx_info *current = init;
	FN_composite_name *temp;
	while ((sub_ctx = names->next(status))) {
		sub_ref = ctx->lookup((*sub_ctx), status);
		if (!status.is_success()) {
			path_str = current_path.string();
			desc = status.description();
			fprintf(stderr, "Unable to get reference for: %s\n"
			    "Error: %s\n", path_str ? (char *)
			    path_str->str() : "",
			    desc ? (char *) desc->str() :
			    gettext("No specified error"));
			delete sub_ctx;
			delete path_str;
			continue;
		}
		sub_ctx_info = get_ctx_info_from_ref(*sub_ref, *sub_ctx);

		// Check for malloc errors
		if (sub_ctx_info == 0) {
			delete names;
			delete ctx;
			delete_all_context_info(init->sub_ctx);
			init->sub_ctx = 0;
			return (0);
		}

		// Update the context handle
		if (current == init)
			current->sub_ctx = sub_ctx_info;
		else
			current->next = sub_ctx_info;

		// Construct the subcontexts
		temp = new FN_composite_name(*sub_ctx);
		update_current_path(*temp);
		// Get attrset
		sub_ctx_info->attrset = get_attrset(ctx, *temp);
		delete temp;
		if (!construct_sub_ctx_and_bindings(sub_ref, sub_ctx_info)) {
			delete names;
			delete ctx;
			delete_all_context_info(init->sub_ctx);
			init->sub_ctx = 0;
			delete_last_current_path();
			return (0);
		}
		delete_last_current_path();

		current = sub_ctx_info;
		delete sub_ctx;
		delete sub_ref;
	}
	delete names;
	delete ctx;
	return (1);
}

static int
construct_sub_ctx_from_file(const FN_ref *ref, FNSP_ctx_info *init)
{
	if ((init) && (init->type != context))
		return (1);

	// Construct the name set from file
	FN_nameset names;
	FILE *rf;
	if ((rf = fopen(file_name, "r")) == NULL) {
		fprintf(stderr, gettext("Could not open file: %s\n"),
		    file_name);
		return (0);
	}

	FN_string *sub_ctx_name;
	char line[MAXINPUTLEN];
	while (fgets(line, MAXINPUTLEN, rf) != NULL) {
		sub_ctx_name = new FN_string((unsigned char *) line);
		if (!sub_ctx_name) {
			fprintf(stderr, gettext("Insufficient resources:"
			    " Unable to malloc\n"));
			fclose(rf);
			return (0);
		}
		names.add(*sub_ctx_name);
		delete sub_ctx_name;
	}
	fclose(rf);

	// Get the context
	FN_status status;
	FN_string *desc, *path_str;
	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
	if (!status.is_success()) {
		path_str = current_path.string();
		fprintf(stderr, gettext("Unable to get context from "
		    "reference: %s\n"),
		    path_str ? (char *) path_str->str() : "");
		desc = status.description();
		if (desc)
			fprintf(stderr, "%s: %s\n", program_name,
			    desc->str());
		delete path_str;
		delete desc;
		return (0);
	}

	// Construct the sub context information
	const FN_string *sub_ctx;
	FN_ref *sub_ref;
	void *ip;
	FN_composite_name *temp;
	FNSP_ctx_info *sub_ctx_info;
	FNSP_ctx_info *current = init;
	for (sub_ctx = names.first(ip); sub_ctx;
	    sub_ctx = names.next(ip)) {
		sub_ref = ctx->lookup((*sub_ctx), status);
		if (!status.is_success()) {
			path_str = current_path.string();
			desc = status.description();
			fprintf(stderr, "Unable to get reference for: %s\n"
			    "Error: %s\n", path_str ? (char *)
			    path_str->str() : "",  desc ?
			    (char *) desc->str() :
			    gettext("No specified error"));
			delete path_str;
			continue;
		}
		sub_ctx_info = get_ctx_info_from_ref(*sub_ref, *sub_ctx);
		// Check for malloc errors
		if (sub_ctx_info == 0) {
			delete ctx;
			delete_all_context_info(init->sub_ctx);
			init->sub_ctx = 0;
			return (0);
		}

		// Update the context handle
		if (current == init)
			current->sub_ctx = sub_ctx_info;
		else
			current->next = sub_ctx_info;

		// Update the current path
		temp = new FN_composite_name(*sub_ctx);
		update_current_path(*temp);
		// Get attrset
		sub_ctx_info->attrset = get_attrset(ctx, *temp);
		delete temp;
		if (!construct_sub_ctx_and_bindings(sub_ref, sub_ctx_info)) {
			delete ctx;
			delete_all_context_info(init->sub_ctx);
			init->sub_ctx = 0;
			return (0);
		}
		// Remove last name from current path
		delete_last_current_path();

		current = sub_ctx_info;
		delete sub_ref;
	}
	delete ctx;
	return (1);
}

static FNSP_ctx_info *
construct_ctx_from_source()
{
	FN_status status;
	FN_string *path_str, *desc;
	int ret;

	// Set the current path
	void *ip;
	const FN_string *comp;
	for (comp = source->first(ip); comp; comp = source->next(ip))
		current_path.append_comp(*comp);

	// Obtain the desired context to be copied
	FN_ref *first_ref = src_initial_ctx->lookup((*source), status);
	if (!status.is_success()) {
		path_str = current_path.string();
		fprintf(stderr, gettext("Unable to get reference "
		    "for: %s\n"),
		    path_str ? (char *) path_str->str() : "");
		desc = status.description();
		if (desc)
			fprintf(stderr, "%s: %s\n", program_name,
			    desc->str());
		delete path_str;
		delete desc;
		return (0);
	}

	// Determine the source ns if not provided
	if (source_ns == 0) {
		const FN_ref_addr *addr;
		const FN_identifier *id;
		void *ip;
		for (addr = first_ref->first(ip);
		    addr; addr = first_ref->next(ip)) {
			id = addr->type();
			if (((*id) == FNSP_nisplus_addr_type) ||
			    ((*id) == FNSP_nisplus_ptr_addr_type))
				source_ns = FNSP_nisplus_ns;
			else if (((*id) == FNSP_nis_addr_type) ||
			    ((*id) == FNSP_nis_ptr_addr_type))
				source_ns = FNSP_nis_ns;
			else if (((*id) == FNSP_files_addr_type) ||
			    ((*id) == FNSP_files_ptr_addr_type))
				source_ns = FNSP_files_ns;
			if (source_ns != 0)
				break;
		}
	}

	if (source_ns == 0) {
		delete first_ref;
		usage(program_name,
		    gettext("Unable to determine the source "
		    "name service.\n"));
	}

	FN_string *source_str = source->string();
	FNSP_ctx_info *answer = get_ctx_info_from_ref(*first_ref,
	    *source_str);
	delete source_str;

	if (answer) {
		answer->attrset = get_attrset(src_initial_ctx,
		    (*source));
		if (read_from_file)
			ret =
			    construct_sub_ctx_from_file(first_ref,
			    answer);
		else
			ret =
			    construct_sub_ctx_and_bindings(first_ref,
			    answer);
	}

	// Remove name from current_path
	while (current_path.first(ip)) {
		current_path.next(ip);
		current_path.delete_comp(ip);
	}

	delete first_ref;
	if (ret == 0) {
		delete_all_context_info(answer);
		answer = 0;
	}
	return (answer);
}

void
print_fnsp_context_info(FNSP_ctx_info *ctx_info)
{
	char path[1024];
	FN_string *ctx_string = ctx_info->name->string();

	FN_string *current_string = current_path.string();
	printf("\n%s/%s\n", current_string->str(), ctx_string->str());
	delete current_string;
	if (ctx_info->type == context_binding) {
		FN_string *ctx_bind = ctx_info->b_name->string();
		printf("The above context bound to: %s\n", ctx_bind->str());
		delete ctx_bind;
	}

	// Update the current path, and proceed down the list
	update_current_path(*ctx_info->name);

	// Traverse the list
	if (ctx_info->sub_ctx)
		print_fnsp_context_info(ctx_info->sub_ctx);

	// Remove the last name from current path
	delete_last_current_path();

	// Travese the peer list
	if (ctx_info->next)
		print_fnsp_context_info(ctx_info->next);
}

static FN_ref *
fnscreate(FN_ctx *ctx, const FN_composite_name &name,
    const FN_attrset *attrset,
    unsigned context_type, FN_status &status,
    const FN_identifier *ref_type = 0)
{
	// Full name of the context being created
	FN_string *path_str = current_path.string();

	FN_ref *ref;
	FN_attrset *attrs = generate_creation_attrs(context_type,
	    ref_type);

	if (attrs == NULL) {
		fprintf(stderr, gettext("fncopy: could not generate "
		    "creation attributes for: %s\n"), path_str ?
		    (char *) path_str->str() : "");
		delete path_str;
		return (0);
	}

	// Combine attrs and attrset
	void *ip;
	const FN_attribute *attr;
	if ((attrset) && (attrset->count() > 0))
		for (attr = attrset->first(ip); attr;
		    attr = attrset->next(ip))
			attrs->add(*attr);

	ref = ctx->attr_create_subcontext(name, attrs, status);
	delete attrs;

	if (status.is_success()) {
		if (verbose)
			fprintf(stdout, gettext("Created context %s\n"),
				path_str ? (char *) path_str->str() : "");
	} else if (status.code() == FN_E_NAME_IN_USE) {
		fprintf(stderr, gettext("Binding for %s already exists\n"),
		    path_str ? (char *) path_str->str() : "");
		ref = ctx->lookup(name, status);
	} else {
		// Possibility of being in initial context
		FN_status istatus;
		ref = ctx->lookup(name, istatus);
		if (!istatus.is_success()) {
			FN_string *desc = status.description();
			fprintf(stderr, gettext("Create of %s failed: %s\n"),
			    path_str ? (char *) path_str->str() : "",
			    desc->str());
			delete desc;
			return_value = 1;
		} else
			fprintf(stderr, gettext("Binding for %s already "
			    "exists\n"), path_str ? (char *)
			    path_str->str() : "");
	}

	delete path_str;
	return (ref);
}

static FN_string *
get_domain_name_from_ref(FN_ref *ref)
{
	FN_string *domainname;
	if (destination_ns == FNSP_files_ns) {
		domainname = new FN_string((unsigned char *) "");
		return (domainname);
	}

	FN_string *domainname_dir = FNSP_reference_to_internal_name(*ref);
	unsigned status;
	switch (destination_ns) {
	case FNSP_nis_ns:
		domainname = FNSP_nis_orgname_of(*domainname_dir, status);
		break;
	case FNSP_nisplus_ns:
		domainname = FNSP_orgname_of(*domainname_dir, status);
	default:
		break;
	}

	delete domainname_dir;
	return (domainname);
}

static void
create_fnsp_contexts(FN_ctx *ctx, FNSP_ctx_info *ctx_info)
{
	FN_status status;
	FN_ref *ref = 0;
	FN_ctx *sub_ctx;
	FN_string *path_str;
	void *ip;

	// Update the current_path varibale
	update_current_path(*ctx_info->name);

	if (ctx_info->type == context) {
		// Create the context
		ref = fnscreate(ctx, (*ctx_info->name),
		    ctx_info->attrset, ctx_info->ctx_type,
		    status, ctx_info->ref_type);
		if (ref == 0)
			return_value = 1;

		// Check if more address are to be added
		if ((ref) && (ctx_info->ref != 0)) {
			// More address to be added to the context
			const FN_ref_addr *addr;
			for (addr = ctx_info->ref->first(ip); addr;
			    addr = ctx_info->ref->next(ip))
				ref->append_addr(*addr);
			ctx->bind(*ctx_info->name, *ref, 0, status);
			if (!status.is_success()) {
				FN_string *desc = status.description();
				path_str = current_path.string();
				fprintf(stderr,
				    gettext("Binding for %s failed: %s\n"),
				    path_str ? (char *) path_str->str() :
				    "", desc->str());
				delete desc;
				delete path_str;
				return_value = 1;
			}
		}

		// Traverse the sub-context list
		if ((ref) && (ctx_info->sub_ctx)) {
			// Obtain the child context
			sub_ctx = FN_ctx::from_ref(*ref,
			    AUTHORITATIVE, status);
			if (!status.is_success()) {
				FN_string *desc = status.description();
				path_str = current_path.string();
				fprintf(stderr,
				    gettext("Unable to get context from "
				    "reference of: %s\nError: %s\n"),
				    path_str ? (char *) path_str->str() :
				    "", desc->str());
				delete desc;
				delete path_str;
				return_value = 1;
			} else
				create_fnsp_contexts(sub_ctx,
				    ctx_info->sub_ctx);
			delete sub_ctx;
		}
		delete ref;
	}

	// Remove last name from current path
	delete_last_current_path();

	// Travese the peer list
	if (ctx_info->next)
		create_fnsp_contexts(ctx, ctx_info->next);
}

static unsigned
fnsbind(FN_ctx *ctx, const FN_composite_name &name,
    const FN_ref &ref, const FN_attrset *attrset)
{
	FN_status status;
	FN_string *ptr_path = current_path.string();

	if (ctx->attr_bind(name, ref, attrset,
	    global_bind_flags, status) ||
	    status.code() == FN_E_NAME_IN_USE) {
		if (status.code() == FN_E_NAME_IN_USE)
			fprintf(stderr,
			    gettext("Binding for '%s' already exists\n"),
			    (ptr_path ? (char *)ptr_path->str() : ""));
		else if (verbose)
			fprintf(stdout, gettext("Bindings for %s"
			    " added\n"), ptr_path ?
			    (char *) ptr_path->str() : "");
		delete ptr_path;
		return (FN_SUCCESS);
	} else {
		FN_string *desc = status.description();
		FN_string *ref_desc = ref.description();
		fprintf(stderr, gettext("%s: '%s' could not be bound with "
		    "context reference of %s: '%s'\n"),
			program_name,
			(ptr_path ? (char *)ptr_path->str() : ""),
			(ref_desc ? (char *)ref_desc->str() : ""),
			(desc ? (char *)(desc->str()) :
			    (gettext("No status description"))));
		delete desc;
		delete ptr_path;
		delete ref_desc;
		return (status.code());
	}
}

void
create_fnsp_bindings(FN_ctx *ctx, FNSP_ctx_info *ctx_info)
{
	FN_ref *ref;
	FN_status status;
	FN_string *full_name, *aname, *desc;
	FN_string *domainname, *user_name, *host_name;
	FN_ctx *sub_ctx;
	switch (ctx_info->type) {
	case context_binding:
		ref = ctx->lookup((*ctx_info->b_name), status);
		update_current_path(*ctx_info->name);
		if (status.is_success()) {
			fnsbind(ctx, (*ctx_info->name), (*ref),
			    ctx_info->attrset);
			delete ref;
		} else {
			full_name = current_path.string();
			aname = ctx_info->name->string();
			desc = status.description();
			fprintf(stderr, gettext("Binding of %s "
			    "failed, since lookup of %s failed\n"),
			    full_name ? (char *) full_name->str() : "",
			    aname ? (char *) aname->str() : "");
			fprintf(stderr, gettext("Error: %s\n"),
			    desc ? (char *) desc->str() : "");
			delete aname;
			delete full_name;
			delete desc;
		}
		delete_last_current_path();
		break;
	case binding:
	case fs_context_mount:
		update_current_path(*ctx_info->name);
		fnsbind(ctx, (*ctx_info->name), (*ctx_info->ref),
		    ctx_info->attrset);
		delete_last_current_path();
		break;
	case fs_context_user:
		user_name = get_user_name_from_ref(ctx_info->ref);
		domainname = get_domain_name_from_ref(ctx->get_ref(status));
		ref = create_user_fs_ref(*user_name, *domainname);
		update_current_path(*ctx_info->name);
		fnsbind(ctx, (*ctx_info->name), (*ref), ctx_info->attrset);
		delete_last_current_path();
		delete ref;
		delete domainname;
		delete user_name;
		break;
	case fs_context_host:
		host_name = get_host_name_from_ref(ctx_info->ref);
		domainname = get_domain_name_from_ref(ctx->get_ref(status));
		ref = create_host_fs_ref(*host_name, *domainname);
		update_current_path(*ctx_info->name);
		fnsbind(ctx, (*ctx_info->name), (*ref), ctx_info->attrset);
		delete_last_current_path();
		delete ref;
		delete domainname;
		delete host_name;
		break;
	default:
		// Traverse the sub-context list
		if (ctx_info->sub_ctx) {
			// Update current path
			update_current_path(*ctx_info->name);
			// Obtain the child context
			ref = ctx->lookup(*ctx_info->name, status);
			if (!status.is_success()) {
				full_name = current_path.string();
				desc = status.description();
				fprintf(stderr, gettext("Unable to get "
				    "context from reference of: %s\n"
				    "Error: %s\n"), full_name ?
				    (char *) full_name->str() : "",
				    desc ? (char *) desc->str() : "");
				delete full_name;
				delete desc;
				return_value = 1;
				delete_last_current_path();
				break;
			}
			sub_ctx = FN_ctx::from_ref(*ref,
			    AUTHORITATIVE, status);
			delete ref;
			if (!status.is_success()) {
				full_name = current_path.string();
				desc = status.description();
				fprintf(stderr, gettext("Unable to get "
				    "context from reference of: %s\n"
				    "Error: %s\n"), full_name ?
				    (char *) full_name->str() : "",
				    desc ? (char *) desc->str() : "");
				delete full_name;
				delete desc;
				return_value = 1;
			} else
				create_fnsp_bindings(sub_ctx,
				    ctx_info->sub_ctx);
			// Remove last name from current_path
			delete_last_current_path();
		}
		break;
	}

	// Travese the peer list
	if (ctx_info->next)
		create_fnsp_bindings(ctx, ctx_info->next);
}

// Make sure that NIS_GROUP has been set, only for org
static int
check_nis_group()
{
	if (getenv("NIS_GROUP") == 0) {
		fprintf(stderr, "%s",
gettext("The environment variable NIS_GROUP has not been set.  This has\n\
administrative implications for contexts that will be created.\n\
See nis+(1) and fncreate(1M). Please try again after setting NIS_GROUP.\n"));
		return (0);
	} else
		return (1);
}

static void
check_for_org_context(FNSP_ctx_info *ctx_info)
{
	FN_status status;
	FN_ref *ref;
	FN_string *str;
	if ((ctx_info->ctx_type == FNSP_organization_context) ||
	    (ctx_info->ctx_type == FNSP_enterprise_context)) {
		// Make sure the destination context exists
		ref = dst_initial_ctx->lookup(*destination, status);
		if (!status.is_success()) {
			str = destination->string();
			fprintf(stderr, gettext("The destination "
			    "org/ens context %s does not exist.\n"
			    "Must be created using fncreate and then"
			    "fncopy can be used\n"), str->str());
			delete str;
		}
		delete ref;
		delete_all_context_info(ctx_info);
		delete_all_static_variables();
		exit(1);
	}
}

static FN_ref *
get_dst_penultimate_reference(FN_status &status,
    FNSP_ctx_info *ctx_info, FN_ref **c_ref)
{
	// Get the reference of the penultimate context
	FN_composite_name p_ctx(*destination);
	void *ip;
	const FN_string *last_comp;
	if (p_ctx.count() > 1) {
		last_comp = p_ctx.last(ip);
		p_ctx.next(ip);
		p_ctx.delete_comp(ip);

		// Name of the destination context should
		// be the last component
		delete ctx_info->name;
		ctx_info->name = new FN_composite_name(*last_comp);

		// Set the current path varibale
		const FN_string *comp;
		for (comp = p_ctx.first(ip); comp;
		    comp = p_ctx.next(ip))
			current_path.append_comp(*comp);
		return (dst_initial_ctx->lookup(p_ctx, status));
	}

	// Trying to create in the inital context
	// Make sure the contexts exists
	*c_ref = dst_initial_ctx->lookup(p_ctx, status);
	if (status.is_success())
		return (0);

	// Trying to create in the initial context
	// NOT ALLOWED
	last_comp = p_ctx.string();
	fprintf(stdout, gettext("Cannot create %s in the"
	    "initial context"), last_comp->str());
	status.set_code(FN_E_CTX_NO_PERMISSION);
	return (0);
}

static void
create_all_fnsp_contexts(FNSP_ctx_info *ctx_info)
{
	// Make sure fncopy does not create organization context
	check_for_org_context(ctx_info);

	FN_status status;
	FN_string *path_str, *desc;
	FN_ref *c_ref;
	FN_ref *ref = get_dst_penultimate_reference(status, ctx_info,
	    &c_ref);
	if (!status.is_success()) {
		path_str = current_path.string();
		fprintf(stderr, gettext("Unable to get reference "
		    "for: %s\n"),
		    path_str ? (char *) path_str->str() : "");
		desc = status.description();
		if (desc)
			fprintf(stderr, "%s: %s\n", program_name,
			    desc->str());
		delete path_str;
		delete desc;
		return;
	}

	// If the destination_ns = 0 set it
	if (ref)
		c_ref = ref;
	if (destination_ns == 0) {
		const FN_ref_addr *addr;
		const FN_identifier *id;
		void *ip;
		for (addr = c_ref->first(ip);
		    addr; addr = c_ref->next(ip)) {
			id = addr->type();
			if (((*id) == FNSP_nisplus_addr_type) ||
			    ((*id) == FNSP_nisplus_ptr_addr_type))
				destination_ns = FNSP_nisplus_ns;
			else if (((*id) == FNSP_nis_addr_type) ||
			    ((*id) == FNSP_nis_ptr_addr_type))
				destination_ns = FNSP_nis_ns;
			else if (((*id) == FNSP_files_addr_type) ||
			    ((*id) == FNSP_files_ptr_addr_type))
				destination_ns = FNSP_files_ns;
			if (destination_ns != 0)
				break;
		}
	}

	if (destination_ns == 0) {
		delete c_ref;
		fprintf(stderr, "Unable to determine the destination name"
		    " service. Need to specify explicitly\n");
		delete_all_context_info(ctx_info);
		delete_all_static_variables();
		usage(program_name, "Specify destination name service");
	}

	if ((destination_ns == FNSP_nisplus_ns) &&
	    ((ctx_info->ctx_type == FNSP_organization_context) ||
	    (ctx_info->ctx_type == FNSP_enterprise_context)))
		if (check_nis_group() == 0) {
			delete c_ref;
			return;
		}

	FN_ctx *pctx;
	if (ref) {
		pctx = FN_ctx::from_ref((*ref), AUTHORITATIVE, status);
		if (!status.is_success()) {
			path_str = current_path.string();
			fprintf(stderr, gettext("Unable to get context "
			    "from reference of: %s\n"),
			    path_str ? (char *) path_str->str() : "");
			    desc = status.description();
			if (desc)
				fprintf(stderr, "%s: %s\n", program_name,
				    desc->str());
			delete path_str;
			delete desc;
			return;
		}
		delete ref;
	} else
		pctx = dst_initial_ctx;

	// Create the contexts and bindings
	create_fnsp_contexts(pctx, ctx_info);
	create_fnsp_bindings(pctx, ctx_info);
}

main(int argc, char **argv)
{
	process_cmd_line(argc, argv);
	// print_cmd_arguments();

	// Get the initial contexts for source and destination
	FN_status status;
	if (source_ns)
		src_initial_ctx = FN_ctx::from_initial_with_ns(source_ns,
		    AUTHORITATIVE, status);
	else
		src_initial_ctx = FN_ctx::from_initial(AUTHORITATIVE, status);
	check_status(status,
	    gettext("Unable to get source name service's initial context"));

	// Get the destination initial context
	if (destination_ns)
		dst_initial_ctx = FN_ctx::from_initial_with_ns(
		    destination_ns, AUTHORITATIVE, status);
	else
		dst_initial_ctx = FN_ctx::from_initial(
		    AUTHORITATIVE, status);
	check_status(status,
	    gettext("Unable to get destination name service's "
	    "initial context"));

	// Get the standard context type identifiers
	fnsp_get_std_ctx_identifiers();

	// Construct the source ctx and bindings
	FNSP_ctx_info *source_ctx = construct_ctx_from_source();

	if (source_ctx) {
		// Print the contents of the linked list
		// print_fnsp_context_info(source_ctx);

		// Create the contexts and bindings at the destination
		create_all_fnsp_contexts(source_ctx);
	}

	delete_all_context_info(source_ctx);
	delete_all_static_variables();
	exit(return_value);
}
