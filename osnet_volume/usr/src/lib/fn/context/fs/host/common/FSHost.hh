/*
 * Copyright (c) 1994 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FSHOST_HH
#define	_FSHOST_HH

#pragma ident	"@(#)FSHost.hh	1.2	96/03/31 SMI"


// An FSHost context is a read-only context derived from the
// FN_ctx_cnsvc_weak_dynamic class.  It supports a hierarchical
// namespace with a slash-separated left-to-right syntax.  FSHost
// is currently a terminal namespace.
//
// Essentially, the FSHost context gives life to the output of
// "showmount -e <hostname>".  Its contents are the directories
// exported by an NFS server and their parent directories (so
// if "/export" is the only exported directory, for example,
// there could be FSHost contexts for "/" and "/export").
//
// Each FSHost context may have a single attribute named "exported".
// The presense of this attribute indicates that the corresponding
// directory is exported.
//
// The NNS operations forward to the corresponding non-NNS operations.


#include <xfn/xfn.hh>
#include <xfn/fn_spi.hh>
#include <rpc/rpc.h>
#include "Export.hh"


class FSHost : public FN_ctx_cnsvc_weak_dynamic {
public:
	~FSHost();
	FN_ref *get_ref(FN_status &) const;

	FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	static FSHost *from_address(const FN_ref_addr &, const FN_ref &,
	    unsigned int auth, FN_status &);

private:
	FN_ref *my_reference;

	FN_composite_name *my_name;	// name of ctx relative to fs_host root
	char *hostname;			// name of NFS server

	FSHost(const FN_ref &, unsigned int auth);

	// Resolve a name to its final component, and set "exports" to
	// the corresponding node in the directory tree.  Return NULL
	// on error.  This call must be followed, even on failure, by
	// a call to resolve_fini().
	FN_ref *resolve(const FN_composite_name &, FN_status_cnsvc &,
	    ExportNode *&exports);

	// Given the ExportNode returned by a prior call to resolve(),
	// perform some cleanup actions on that node.  The node (and
	// anything derived from it) may no longer be used.
	void resolve_fini(ExportNode *exports);

	// Context operations for the multi-component naming service

	FN_ref *cn_lookup(const FN_composite_name &, unsigned int flags,
	    FN_status_cnsvc &);
	FN_namelist *cn_list_names(const FN_composite_name &,
	    FN_status_cnsvc &);
	FN_bindinglist *cn_list_bindings(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_bind(const FN_composite_name &, const FN_ref &,
	    unsigned int flags, FN_status_cnsvc &);
	int cn_unbind(const FN_composite_name &, FN_status_cnsvc &);
	FN_ref *cn_create_subcontext(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_destroy_subcontext(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_cnsvc &);
	FN_attrset *cn_get_syntax_attrs(const FN_composite_name &,
	    FN_status_cnsvc &);

	// Attribute operations

	FN_attribute *cn_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int, FN_status_cnsvc &);
	int cn_attr_modify(const FN_composite_name &, unsigned int flags,
	    const FN_attribute &, unsigned int, FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int, FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids(const FN_composite_name &,
	    unsigned int, FN_status_cnsvc &);
	FN_multigetlist *cn_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int, FN_status_cnsvc &);
	int cn_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist &, unsigned int,
	    FN_attrmodlist **, FN_status_cnsvc &);

	// Extended attribute interface

	int cn_attr_bind(const FN_composite_name &, const FN_ref &,
	    const FN_attrset *, unsigned int exclusive, FN_status_cnsvc &);
	FN_ref *cn_attr_create_subcontext(const FN_composite_name &,
	    const FN_attrset *, FN_status_cnsvc &);
	FN_searchlist *cn_attr_search(const FN_composite_name &,
	    const FN_attrset *match_attrs, unsigned int return_ref,
	    const FN_attrset *return_attr_ids, FN_status_cnsvc &);
	FN_ext_searchlist *cn_attr_ext_search(const FN_composite_name &,
	    const FN_search_control *, const FN_search_filter *,
	    FN_status_cnsvc &);

	// Next naming system (nns) context operations

	FN_ref *cn_lookup_nns(const FN_composite_name &, unsigned int flags,
	    FN_status_cnsvc &);
	FN_namelist *cn_list_names_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	FN_bindinglist *cn_list_bindings_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_bind_nns(const FN_composite_name &, const FN_ref &,
	    unsigned int flags, FN_status_cnsvc &);
	int cn_unbind_nns(const FN_composite_name &, FN_status_cnsvc &);
	FN_ref *cn_create_subcontext_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_destroy_subcontext_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_rename_nns(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_cnsvc &);
	FN_attrset *cn_get_syntax_attrs_nns(const FN_composite_name &,
	    FN_status_cnsvc &);

	// Next naming system (nns) attribute operations

	FN_attribute *cn_attr_get_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int, FN_status_cnsvc &);
	int cn_attr_modify_nns(const FN_composite_name &,
	    unsigned int flags, const FN_attribute &,
	    unsigned int, FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int, FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids_nns(const FN_composite_name &,
	    unsigned int, FN_status_cnsvc &);
	FN_multigetlist *cn_attr_multi_get_nns(const FN_composite_name &,
	    const FN_attrset *, unsigned int, FN_status_cnsvc &);
	int cn_attr_multi_modify_nns(const FN_composite_name &,
	    const FN_attrmodlist &, unsigned int,
	    FN_attrmodlist **, FN_status_cnsvc &);

	// Extended attribute operations on next naming system pointers

	int cn_attr_bind_nns(const FN_composite_name &, const FN_ref &,
	    const FN_attrset *, unsigned int exclusive, FN_status_cnsvc &);
	FN_ref *cn_attr_create_subcontext_nns(const FN_composite_name &,
	    const FN_attrset *, FN_status_cnsvc &);
	FN_searchlist *cn_attr_search_nns(const FN_composite_name &,
	    const FN_attrset *match_attrs, unsigned int return_ref,
	    const FN_attrset *return_attr_ids, FN_status_cnsvc &);
	FN_ext_searchlist *cn_attr_ext_search_nns(const FN_composite_name &,
	    const FN_search_control *, const FN_search_filter *,
	    FN_status_cnsvc &);
};


#endif	// _FSHOST_HH
