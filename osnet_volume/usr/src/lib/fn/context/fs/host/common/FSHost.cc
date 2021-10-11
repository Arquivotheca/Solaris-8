/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FSHost.cc	1.3	96/03/31 SMI"

#include <stdlib.h>
#include <xfn/xfn.hh>
#include <xfn/fn_spi.hh>
#include <rpc/rpc.h>
#include "FSHost.hh"
#include "Export.hh"
#include "getexports.hh"
#include "xdr_utils.hh"


static const FN_identifier attr_exported((const unsigned char *)"exported");


FSHost::FSHost(const FN_ref &ref, unsigned int auth) : FN_ctx_svc(auth)
{
	my_reference = new FN_ref(ref);
	my_name = NULL;
	hostname = NULL;
}


FSHost::~FSHost()
{
	delete my_reference;
	delete my_name;
	delete hostname;
}


FN_ref *
FSHost::get_ref(FN_status &stat) const
{
	FN_ref *answer = new FN_ref(*my_reference);
	if (answer != NULL) {
		stat.set_success();
	} else {
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FN_composite_name *
FSHost::equivalent_name(const FN_composite_name &name,
    const FN_string &, FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

FSHost *
FSHost::from_address(const FN_ref_addr &addr, const FN_ref &ref,
	unsigned int auth, FN_status &stat)
{
	FSHost *answer = new FSHost(ref, auth);

	if (answer != NULL && answer->my_reference != NULL) {
		fs_host_decode_addr(addr, answer->my_name, answer->hostname);
		if (answer->my_name != NULL && answer->hostname != NULL) {
			stat.set_success();
			return (answer);
		}
	}
	delete answer;
	stat.set(FN_E_INSUFFICIENT_RESOURCES);
	return (NULL);
}


FN_ref *
FSHost::resolve(const FN_composite_name &name, FN_status_cnsvc &cnstat,
    ExportNode *&exports)
{
	ExportTree *tree = export_tree(hostname);
	if (tree == NULL) {
		cnstat.set_error(FN_E_CTX_UNAVAILABLE, *my_reference, name);
		exports = NULL;
		return (NULL);
	}
	exports = &tree->root;

	// Before returning, either:
	// - set exports to NULL and release the export tree, or
	// - leave exports non-NULL so resolve_fini() can release it later.
	// The latter is done on success; either is fine on failure.

	// Walk the exports tree to find node for this context.
	void *iter;
	const FN_string *atom;
	for (atom = my_name->is_empty() ? NULL : my_name->first(iter);
	    atom != NULL;
	    atom = my_name->next(iter)) {
		exports = exports->find_child(*atom);
		if (exports == NULL) {
			cnstat.set_error(FN_E_CTX_UNAVAILABLE, *my_reference,
					name);
			release_export_tree(tree);
			return (NULL);
		}
	}

	if (name.is_empty()) {			// A little shortcut
		return (get_ref(cnstat));
	}

	FN_composite_name dir(*my_name);	// full name of new directory
	FN_ref *ref;

	// Continue walking the exports tree to resolve "name".
	for (atom = name.is_empty() ? NULL : name.first(iter);
	    atom != NULL;
	    atom = name.next(iter)) {
		exports = exports->find_child(*atom);
		if (exports == NULL) {
			ref = fs_host_encode_ref(hostname, &dir);
			if (ref == NULL) {
				cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
			} else {
				name.prev(iter);
				FN_composite_name *remains = name.suffix(iter);
				cnstat.set_error(FN_E_NAME_NOT_FOUND, *ref,
						*remains);
				delete ref;
				delete remains;
			}
			release_export_tree(tree);
			return (NULL);
		}
		if (dir.is_empty()) {
			dir = *atom;
		} else if (dir.append_comp(*atom) == 0) {
			cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
			return (NULL);
		}
	}
	ref = fs_host_encode_ref(hostname, &dir);
	if (ref == NULL) {
		cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		cnstat.set_success();
	}
	return (ref);
}


void
FSHost::resolve_fini(ExportNode *exports)
{
	if (exports != NULL) {
		release_export_tree(exports->tree);
	}
}


// Context operations for the multi-component naming service


FN_ref *
FSHost::cn_lookup(const FN_composite_name &name, unsigned int,
    FN_status_cnsvc &cnstat)
{
	ExportNode *exports;
	FN_ref *ref = resolve(name, cnstat, exports);
	resolve_fini(exports);
	return (ref);
}


FN_namelist *
FSHost::cn_list_names(const FN_composite_name &name, FN_status_cnsvc &cnstat)
{
	ExportNode *exports;
	FN_ref *ref = resolve(name, cnstat, exports);
	if (ref == NULL) {
		resolve_fini(exports);
		return (NULL);
	}
	delete ref;

	FN_nameset *set = new FN_nameset();
	FN_namelist_svc *nlist = new FN_namelist_svc(set);
	if (set == NULL || nlist == NULL) {
		goto mem_failure;
	}
	void *iter;
	ExportNode *child;
	for (child = (ExportNode *)exports->List::first(iter);
	    child != NULL;
	    child = (ExportNode *)exports->List::next(iter)) {
		if (set->add(child->name) == 0) {
			goto mem_failure;
		}
	}
	resolve_fini(exports);
	return (nlist);

mem_failure:
	cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	delete set;
	delete nlist;
	resolve_fini(exports);
	return (NULL);
}


FN_bindinglist *
FSHost::cn_list_bindings(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	ExportNode *exports;
	FN_ref *ref = resolve(name, cnstat, exports);
	if (ref == NULL) {
		resolve_fini(exports);
		return (NULL);
	}
	delete ref;

	FN_bindingset *set = new FN_bindingset();
	FN_bindinglist_svc *blist = new FN_bindinglist_svc(set);
	if (blist == NULL) {
		delete set;
	}
	FN_composite_name *child_name = new FN_composite_name(*my_name);
	if (set == NULL || blist == NULL || child_name == NULL) {
		goto mem_failure;
	}
	if (!name.is_empty()) {
		if (child_name->append_name(name) == 0) {
			goto mem_failure;
		}
	}
	// Remove leading empty component.
	void *iter;
	const FN_string *first;
	first = child_name->first(iter);
	if (first->is_empty()) {
		child_name->delete_comp(iter);
	}

	ExportNode *child;
	for (child = (ExportNode *)exports->List::first(iter);
	    child != NULL;
	    child = (ExportNode *)exports->List::next(iter)) {
		// Add child's name to end of child_name.
		if (child_name->append_comp(child->name) == 0) {
			goto mem_failure;
		}
		FN_ref *child_ref = fs_host_encode_ref(hostname, child_name);
		if (child_ref == NULL ||
		    set->add(child->name, *child_ref) == 0) {
			delete child_ref;
			goto mem_failure;
		}
		delete child_ref;
		// Remove child's name from end of child_name.
		void *iter2;
		const FN_string *last = child_name->last(iter2);
		child_name->next(iter2);
		child_name->delete_comp(iter2);
	}
	resolve_fini(exports);
	delete child_name;
	return (blist);

mem_failure:
	cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	delete blist;
	delete child_name;
	resolve_fini(exports);
	return (NULL);
}


int
FSHost::cn_bind(const FN_composite_name &name, const FN_ref &, unsigned int,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FSHost::cn_unbind(const FN_composite_name &name, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_ref *
FSHost::cn_create_subcontext(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (NULL);
}


int
FSHost::cn_destroy_subcontext(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FSHost::cn_rename(const FN_composite_name &name, const FN_composite_name &,
    unsigned int, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


// fs_host syntax is slash-separated, left-to-right, and case-sensitive.

FN_attrset *
FSHost::cn_get_syntax_attrs(const FN_composite_name &, FN_status_cnsvc &cnstat)
{
	static const FN_string slash((unsigned char *)"/");
	static const FN_syntax_standard
	    syntax(FN_SYNTAX_STANDARD_DIRECTION_LTR,
		    FN_STRING_CASE_SENSITIVE, &slash);

	FN_attrset* syntax_attrs = syntax.get_syntax_attrs();
	if (syntax_attrs != NULL)
		cnstat.set_success();
	else
		cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	return (syntax_attrs);
}


// Attribute operations


FN_attribute *
FSHost::cn_attr_get(const FN_composite_name &name,
    const FN_identifier &attrname, unsigned int /* follow_link */,
    FN_status_cnsvc &cnstat)
{
	static FN_identifier
	    syntax((const unsigned char *)"fn_attr_syntax_ascii");

	if (attrname != attr_exported) {
		cnstat.set_error(FN_E_NO_SUCH_ATTRIBUTE, *my_reference, name);
		return (NULL);
	}

	ExportNode *exports;
	FN_ref *ref = resolve(name, cnstat, exports);
	if (ref == NULL) {
		resolve_fini(exports);
		return (NULL);
	}
	bool_t exported = exports->exported;
	resolve_fini(exports);

	delete ref;
	if (!exported) {
		cnstat.set_error(FN_E_NO_SUCH_ATTRIBUTE, *my_reference, name);
		return (NULL);
	}
	FN_attribute *exp = new FN_attribute(attr_exported, syntax);
	if (exp == NULL) {
		cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (exp);
}


int
FSHost::cn_attr_modify(const FN_composite_name &name, unsigned int,
    const FN_attribute &, unsigned int, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_valuelist *
FSHost::cn_attr_get_values(const FN_composite_name &name,
    const FN_identifier &attrname, unsigned int follow_link,
    FN_status_cnsvc &cnstat)
{
	FN_attribute *attr = cn_attr_get(name, attrname, follow_link, cnstat);
	if (attr == NULL) {
		return (NULL);
	}
	FN_valuelist_svc *vals = new FN_valuelist_svc(attr);
	if (vals == NULL) {
		delete attr;
		cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (vals);
}


FN_attrset *
FSHost::cn_attr_get_ids(const FN_composite_name &name,
    unsigned int follow_link, FN_status_cnsvc &cnstat)
{
	FN_attribute *exp =
	    cn_attr_get(name, attr_exported, follow_link, cnstat);
	if (exp == NULL) {
		return (NULL);
	}
	FN_attrset *attrs = new FN_attrset;
	if (attrs == NULL ||
	    attrs->add(*exp) == 0) {
		cnstat.set(FN_E_INSUFFICIENT_RESOURCES);
		delete attrs;
		attrs = NULL;
	}
	delete exp;
	return (attrs);
}


FN_multigetlist *
FSHost::cn_attr_multi_get(const FN_composite_name &name,
    const FN_attrset *attrset, unsigned int follow_link,
    FN_status_cnsvc &cnstat)
{
	// Check if attr_exported was requested.  If not, return NULL.
	if (attrset != NULL) {
		void *iter;
		const FN_attribute *attr;
		for (attr = attrset->first(iter);
		    attr != NULL;
		    attr = attrset->next(iter)) {
			if (attr->identifier() != NULL &&
			    *attr->identifier() == attr_exported) {
				break;
			}
		}
		if (attr == NULL) {
			return (NULL);
		}
	}

	// cn_attr_get_ids() returns values too.
	FN_attrset *attrs = cn_attr_get_ids(name, follow_link, cnstat);
	if (attrs == NULL) {
		return (NULL);
	}
	return (new FN_multigetlist_svc(attrs));
}


int
FSHost::cn_attr_multi_modify(const FN_composite_name &name,
    const FN_attrmodlist &, unsigned int,
    FN_attrmodlist **, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


// Extended attribute interface

int
FSHost::cn_attr_bind(const FN_composite_name &name,
    const FN_ref &,
    const FN_attrset *,
    unsigned int,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FSHost::cn_attr_create_subcontext(const FN_composite_name &name,
    const FN_attrset *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FSHost::cn_attr_search(
    const FN_composite_name &name,
    const FN_attrset *,
    unsigned int,
    const FN_attrset *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ext_searchlist *
FSHost::cn_attr_ext_search(
    const FN_composite_name &name,
    const FN_search_control *,
    const FN_search_filter *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


// Next naming system (nns) context operations


FN_ref *
FSHost::cn_lookup_nns(const FN_composite_name &name, unsigned int flags,
    FN_status_cnsvc &cnstat)
{
	if (name.is_empty()) {
		cnstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
		return (0);
	}
	return (cn_lookup(name, flags, cnstat));
}


FN_namelist *
FSHost::cn_list_names_nns(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	return (cn_list_names(name, cnstat));
}


FN_bindinglist *
FSHost::cn_list_bindings_nns(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	return (cn_list_bindings(name, cnstat));
}


int
FSHost::cn_bind_nns(const FN_composite_name &name, const FN_ref &,
    unsigned int, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FSHost::cn_unbind_nns(const FN_composite_name &name, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_ref *
FSHost::cn_create_subcontext_nns(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (NULL);
}


int
FSHost::cn_destroy_subcontext_nns(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FSHost::cn_rename_nns(const FN_composite_name &name, const FN_composite_name &,
    unsigned int, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_attrset *
FSHost::cn_get_syntax_attrs_nns(const FN_composite_name &name,
    FN_status_cnsvc &cnstat)
{
	return (cn_get_syntax_attrs(name, cnstat));
}


// Next naming system (nns) attribute operations


FN_attribute *
FSHost::cn_attr_get_nns(const FN_composite_name &name,
    const FN_identifier &attrname, unsigned int follow_link,
    FN_status_cnsvc &cnstat)
{
	return (cn_attr_get_nns(name, attrname, follow_link, cnstat));
}


int
FSHost::cn_attr_modify_nns(const FN_composite_name &name, unsigned int,
    const FN_attribute &, unsigned int, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_valuelist *
FSHost::cn_attr_get_values_nns(const FN_composite_name &name,
    const FN_identifier &attrname, unsigned int follow_link,
    FN_status_cnsvc &cnstat)
{
	return (cn_attr_get_values(name, attrname, follow_link, cnstat));
}


FN_attrset *
FSHost::cn_attr_get_ids_nns(const FN_composite_name &name,
    unsigned int follow_link, FN_status_cnsvc &cnstat)
{
	return (cn_attr_get_ids(name, follow_link, cnstat));
}


FN_multigetlist *
FSHost::cn_attr_multi_get_nns(const FN_composite_name &name,
    const FN_attrset *attrs, unsigned int follow_link, FN_status_cnsvc &cnstat)
{
	return (cn_attr_multi_get(name, attrs, follow_link, cnstat));
}


int
FSHost::cn_attr_multi_modify_nns(const FN_composite_name &name,
    const FN_attrmodlist &, unsigned int,
    FN_attrmodlist **, FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


// Extended attribute interface for nns

int
FSHost::cn_attr_bind_nns(const FN_composite_name &name,
    const FN_ref &,
    const FN_attrset *,
    unsigned int,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FSHost::cn_attr_create_subcontext_nns(const FN_composite_name &name,
    const FN_attrset *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FSHost::cn_attr_search_nns(
    const FN_composite_name &name,
    const FN_attrset *,
    unsigned int,
    const FN_attrset *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ext_searchlist *
FSHost::cn_attr_ext_search_nns(
    const FN_composite_name &name,
    const FN_search_control *,
    const FN_search_filter *,
    FN_status_cnsvc &cnstat)
{
	cnstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}
