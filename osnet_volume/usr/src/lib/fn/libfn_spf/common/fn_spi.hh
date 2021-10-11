/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_SPI_HH
#define	_XFN_FN_SPI_HH

#pragma ident	"@(#)fn_spi.hh	1.7	96/03/31 SMI"

/*
 * SC3.0.alpha cannot deal with nested virtual classes too well.
 * define BROKEN_SC3 to disable "virtual" from base class declarations.
 */

#include <xfn/xfn.h>
#include <xfn/fn_spi.h>
#include <xfn/FN_status_svc.hh>

/*
 * Context Service:
 *    Implements operations on composite names for FN_ctx interfaces
 *    on top of partial composite name interface (p_).
 *
 *    Providers of the p_ implementations can return FN_E_SPI_CONTINUE and
 *    or FN_E_SPI_FOLLOW_LINK to indicate to continue resolution and
 *    FN_ctx_svc will continue the operation on the remaining name or link
 *    as appropriate.
 *
 *    Subclasses of FN_ctx_svc must provide implementations for:
 *    1. p_ methods
 *    2. from_ref_addr C interface constructor with prototype signature:
 *      FN_ctx_svc_from_ref_addr_func
 *
 *    Types of clients are those that wants to do partial composite
 *    name operations (i.e. deal with names that spans possible more than
 *    one naming system).  Examples of such are the service providers for
 *    any of the XFN protocols.
 */

class FN_ctx_svc :
public FN_ctx {
protected:
	const unsigned int authoritative;
	FN_ctx_svc(unsigned int a) : authoritative(a) {};

	FN_ctx_svc();

public:

	virtual ~FN_ctx_svc();

	// provides implementation of FN_ctx interface
	// on top of partial service interfaces (p_ methods)
	FN_ref *lookup(const FN_composite_name &, FN_status &);
	FN_ref *lookup_link(const FN_composite_name &, FN_status &);
	FN_namelist* list_names(const FN_composite_name &, FN_status &);
	FN_bindinglist* list_bindings(const FN_composite_name &, FN_status &);
	int bind(const FN_composite_name &, const FN_ref &,
	    unsigned bind_flags, FN_status &);
	int unbind(const FN_composite_name &, FN_status &);
	FN_ref *create_subcontext(const FN_composite_name &, FN_status &);
	int destroy_subcontext(const FN_composite_name &, FN_status &);
	int rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status &status);
	FN_attrset* get_syntax_attrs(const FN_composite_name &, FN_status &);

	// Attribute operations
	FN_attribute *attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status &);
	int attr_modify(const FN_composite_name &, unsigned int modop,
	    const FN_attribute&, unsigned int follow_link, FN_status &);
	FN_valuelist *attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status &);
	FN_attrset *attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status &);
	FN_multigetlist *attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status &);
	int attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link, FN_attrmodlist **,
	    FN_status &);

	int attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status &status);

	FN_ref *attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status &status);

	FN_searchlist *attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status &status);

	FN_ext_searchlist *attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status &status);

	// get_ref() and equivalent_name() are inherited directly by subclass

	// construct FN_ctx_svc from a reference
	static FN_ctx_svc* from_ref(const FN_ref &, unsigned int auth,
	    FN_status &);
	static FN_ctx_svc* from_initial(unsigned int auth, FN_status &);

	// A subclass of FN_ctx_svc may provide implementation for these
	virtual FN_ctx_svc_data_t *p_get_ctx_svc_data();
	virtual int p_set_ctx_svc_data(FN_ctx_svc_data_t *);

	// A subclass of FN_ctx_svc must provide implementation for these
	virtual FN_ref *p_lookup(const FN_composite_name &,
	    unsigned int lookup_flags, FN_status_psvc&) = 0;
	virtual FN_namelist* p_list_names(const FN_composite_name &,
	    FN_status_psvc&) = 0;
	virtual FN_bindinglist* p_list_bindings(const FN_composite_name &,
	    FN_status_psvc&) = 0;
	virtual int p_bind(const FN_composite_name &, const FN_ref &,
	    unsigned bind_flags, FN_status_psvc&) = 0;
	virtual int p_unbind(const FN_composite_name &, FN_status_psvc&) = 0;
	virtual FN_ref *p_create_subcontext(const FN_composite_name &,
	    FN_status_psvc&) = 0;
	virtual int p_destroy_subcontext(const FN_composite_name &,
	    FN_status_psvc&) = 0;
	virtual int p_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_psvc&) = 0;
	virtual FN_attrset* p_get_syntax_attrs(const FN_composite_name &,
	    FN_status_psvc&) = 0;

	// Attribute operations
	virtual FN_attribute *p_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_psvc&) = 0;
	virtual int p_attr_modify(const FN_composite_name &,
	    unsigned int modop, const FN_attribute&, unsigned int follow_link,
	    FN_status_psvc&) = 0;
	virtual FN_valuelist *p_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_psvc&) = 0;
	virtual FN_attrset *p_attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status_psvc&) = 0;
	virtual FN_multigetlist *p_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_psvc&) = 0;
	virtual int p_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link, FN_attrmodlist **,
	    FN_status_psvc&) = 0;

	// Extended Attribute Operations
	virtual int p_attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_psvc &status) = 0;
	virtual FN_ref *p_attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_psvc &status) = 0;
	virtual FN_searchlist *p_attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_psvc &status) = 0;
	virtual FN_ext_searchlist *p_attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_psvc &status) = 0;
};

// Multiple Component Service: cnsvc
// Provides declarations for interfaces that will implement operations on
// a component name (from a single naming system) that involves 1 or more
// components of the given composite name.
//
// Defines cn_ interfaces, which are like p_ interfaces except the cn_
// interfaces know about component boundaries within a composite name
// and next naming system pointers.
//
// Subclass must provide implementations for
// 1. from_address described in FN_ctx_svc
// 2. partial composite name operations (p_ interfaces)
// using cn_ interfaces
// 3. cn_ interfaces (implements ns component operations)
//
// Types of clients are those that want to define either how
// partial composite
// name operations should be done (p_) or how operations on
// single naming system
// names should be done (cn_) or both.  Most of the clients would be
// intermediaries who define services for either of these, rather than
// direct context service providers.  Expects input name to be one or more
// components of a composite name.
//
// declare FN_ctx_svc as virtual to allow one copy of FN_ctx_svc to be
// used when multiply inherited
//
class FN_ctx_cnsvc:
#ifndef BROKEN_SC3
virtual
#endif /* BROKEN_SC3 */
public FN_ctx_svc
{
public:
	FN_ctx_cnsvc();
	virtual ~FN_ctx_cnsvc();

	// protected:
	// multi component name service interface
	virtual FN_ref *cn_lookup(const FN_composite_name &name,
	    unsigned int lookup_flags, FN_status_cnsvc&) = 0;
	virtual FN_namelist* cn_list_names(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual FN_bindinglist* cn_list_bindings(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual int cn_bind(const FN_composite_name &name,
	    const FN_ref &, unsigned bind_flags, FN_status_cnsvc&) = 0;
	virtual int cn_unbind(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual FN_ref *cn_create_subcontext(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual int cn_destroy_subcontext(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual int cn_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_cnsvc&) = 0;
	virtual FN_attrset* cn_get_syntax_attrs(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;

	// Attribute operations
	virtual FN_attribute *cn_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual int cn_attr_modify(const FN_composite_name &,
	    unsigned int modop, const FN_attribute&, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual FN_valuelist *cn_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual FN_attrset *cn_attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc&) = 0;
	virtual FN_multigetlist *cn_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_cnsvc&) = 0;
	virtual int cn_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_cnsvc &) = 0;

	// Extended Attribute interface

	virtual int cn_attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status) = 0;
	virtual FN_ref *cn_attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_cnsvc &status) = 0;
	virtual FN_searchlist *cn_attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status) = 0;
	virtual FN_ext_searchlist *cn_attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status) = 0;

	// Next naming system naming operations
	virtual FN_ref *cn_lookup_nns(const FN_composite_name &name,
	    unsigned int lookup_flags, FN_status_cnsvc &) = 0;
	virtual FN_namelist*  cn_list_names_nns(const FN_composite_name &name,
	    FN_status_cnsvc &) = 0;
	virtual FN_bindinglist* cn_list_bindings_nns(
	    const FN_composite_name &name, FN_status_cnsvc &) = 0;
	virtual int cn_bind_nns(const FN_composite_name &name,
	    const FN_ref &, unsigned bind_flags, FN_status_cnsvc &) = 0;
	virtual int cn_unbind_nns(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual FN_ref *cn_create_subcontext_nns(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual int cn_destroy_subcontext_nns(const FN_composite_name &name,
	    FN_status_cnsvc&) = 0;
	virtual int cn_rename_nns(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_cnsvc&) = 0;
	virtual FN_attrset* cn_get_syntax_attrs_nns(
	    const FN_composite_name &name, FN_status_cnsvc&) = 0;

	// Attribute operations on next naming system pointers
	virtual FN_attribute *cn_attr_get_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual int cn_attr_modify_nns(const FN_composite_name &,
	    unsigned int modop, const FN_attribute&, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual FN_valuelist *cn_attr_get_values_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc&) = 0;
	virtual FN_attrset *cn_attr_get_ids_nns(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc&) = 0;
	virtual FN_multigetlist *cn_attr_multi_get_nns(
	    const FN_composite_name &, const FN_attrset *,
	    unsigned int follow_link, FN_status_cnsvc&) = 0;
	virtual int cn_attr_multi_modify_nns(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_cnsvc&) = 0;

	// Extended Attribute operations on next naming system pointers
	virtual int cn_attr_bind_nns(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status) = 0;
	virtual FN_ref *cn_attr_create_subcontext_nns(
	    const FN_composite_name &n, const FN_attrset *attr,
	    FN_status_cnsvc &status) = 0;
	virtual FN_searchlist *cn_attr_search_nns(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status) = 0;
	virtual FN_ext_searchlist *cn_attr_ext_search_nns(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status) = 0;
};


// Multiple Component Implementation Service:
// Implements p_ interfaces using cn_ interfaces supplied by subclass.
//
// Adds p_resolve virtual functions defined by subclass which resolves
// through the intermediate contexts.
//
// Subclass must provide implementations for
// 1. from_address  described in FN_ctx_svc
// 2. p_resolve interfaces
// 3. cn_ interfaces
//
// Types of clients are those that want to use do per naming system
// operations using the cn_ interfaces.  Clients expects the name input to
// consists of multiple components (in the form of a composite name).
//

enum p_resolve_status {
	RS_STATUS_SET,
	RS_TERMINAL_NNS_COMPONENT,
	RS_TERMINAL_COMPONENT
	};

// declare FN_ctx_cnsvc as virtual to allow one copy of FN_ctx_cnsvc to be
// used when multiply inherited

class FN_ctx_cnsvc_impl:
#ifndef BROKEN_SC3
virtual
#endif /* BROKEN_SC3 */
public FN_ctx_cnsvc
{
protected:
	FN_ctx_cnsvc_impl();
	virtual ~FN_ctx_cnsvc_impl();

	virtual int p_resolve(const FN_composite_name &n,
	    FN_status_psvc& s, FN_composite_name **ret_fn) = 0;

	// implementation of FN_ctx_svc interface (p_) on top of
	// FN_ctx_cnsvc interface (cn_)
	FN_ref *p_lookup(const FN_composite_name &, unsigned int lookup_flag,
	    FN_status_psvc &);
	FN_namelist* p_list_names(const FN_composite_name &, FN_status_psvc &);
	FN_bindinglist* p_list_bindings(const FN_composite_name &,
	    FN_status_psvc &);
	int p_bind(const FN_composite_name &, const FN_ref &,
	    unsigned bind_flags, FN_status_psvc &);
	int p_unbind(const FN_composite_name &, FN_status_psvc &);
	FN_ref *p_create_subcontext(const FN_composite_name &,
	    FN_status_psvc &);
	int p_destroy_subcontext(const FN_composite_name &, FN_status_psvc &);
	int p_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_psvc &);
	FN_attrset* p_get_syntax_attrs(const FN_composite_name &,
	    FN_status_psvc &);

	// Attribute operations
	FN_attribute *p_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_psvc &);
	int p_attr_modify(const FN_composite_name &,
	    unsigned int modop, const FN_attribute &,
	    unsigned int follow_link, FN_status_psvc &);
	FN_valuelist *p_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_psvc &);
	FN_attrset *p_attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status_psvc &);
	FN_multigetlist *p_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_psvc &);
	int p_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist &,
	    unsigned int follow_link, FN_attrmodlist **, FN_status_psvc &);

	// Extended Attribute Operations
	int p_attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_psvc &status);
	FN_ref *p_attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_psvc &status);
	FN_searchlist *p_attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_psvc &status);
	FN_ext_searchlist *p_attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_psvc &status);
};


/*
 * Static Component Service:  for weakly separated context
 *     Implements component name operations based on static discovery of naming
 *     system boundary by giving implementations for cn_resolve_ functions.
 *
 *     Adds declaration for
 *          p_component_parser()
 *     to be defined by subclass for determining ns boundary.
 *
 *     Subclasses must  provide implementations for:
 *     1. from_ref_addr  described in FN_ctx_svc
 *     2. p_component_parser()
 *     3. cn_ interfaces
 *
 *     Clients of this are contexts that support strong separation or
 *     syntactic boundary discovery (or any other discovery methods that
 *     can be done statically).
 */
class FN_ctx_cnsvc_weak_static:
public FN_ctx_cnsvc_impl
{
protected:
	FN_ctx_cnsvc_weak_static();
	virtual ~FN_ctx_cnsvc_weak_static();
	virtual int p_resolve(const FN_composite_name &n,
	    FN_status_psvc &s, FN_composite_name **ret_fn);
	virtual FN_composite_name *p_component_parser(
	    const FN_composite_name &, FN_composite_name **rest,
	    FN_status_psvc &s) = 0;
};

/*
* Single component service:
* Provides interferace on operations on a single component string name
* (i.e. subclasses can expect the name to be in a single string).
*
* Adds declarations for c_ interfaces which takes FN_string as the
* name argument.
*
* Implements the cn_ interfaces using the c_ interfaces.
*
* Subclasses must provide implementations for:
* 1. from_address described in FN_ctx_svc
* 2. c_ interfaces(component name, expects input name in string form)
* 3. p_ interfaces(partial composite name operations using cn_ interfaces)
*
* Clients of this type are those that expect the 'name' argument to
* be in string form rather than in FN_composite_name form.
*/
class FN_ctx_csvc:
#ifndef BROKEN_SC3
virtual
#endif /* BROKEN_SC3 */
public FN_ctx_cnsvc
{
protected:
	FN_ctx_csvc();
	virtual ~FN_ctx_csvc();

	// implementation of FN_ctx_cnsvc interface (on top of
	// component string name service interface)
	FN_ref *cn_lookup(const FN_composite_name &,
	    unsigned int lookup_flags, FN_status_cnsvc &);
	FN_namelist* cn_list_names(const FN_composite_name &,
	    FN_status_cnsvc &);
	FN_bindinglist* cn_list_bindings(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_bind(const FN_composite_name &, const FN_ref &,
	    unsigned bind_flags, FN_status_cnsvc &);
	int cn_unbind(const FN_composite_name &, FN_status_cnsvc &);
	FN_ref *cn_create_subcontext(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_destroy_subcontext(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive, FN_status_cnsvc &);
	FN_attrset* cn_get_syntax_attrs(const FN_composite_name &,
	    FN_status_cnsvc &);

	// Attribute operations
	FN_attribute *cn_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_modify(const FN_composite_name &,
	    unsigned int modop, const FN_attribute &, unsigned int follow_link,
	    FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc &);
	FN_multigetlist *cn_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_cnsvc &);

	// Extended Attribute interface
	int cn_attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status);
	FN_ref *cn_attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_cnsvc &status);
	FN_searchlist *cn_attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status);
	FN_ext_searchlist *cn_attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status);

	// Naming operations on next naming system operations
	FN_ref *cn_lookup_nns(const FN_composite_name &,
	    unsigned int lookup_flags, FN_status_cnsvc &);
	FN_namelist* cn_list_names_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	FN_bindinglist* cn_list_bindings_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_bind_nns(const FN_composite_name &, const FN_ref &,
	    unsigned bind_flags, FN_status_cnsvc &);
	int cn_unbind_nns(const FN_composite_name &, FN_status_cnsvc &);
	FN_ref *cn_create_subcontext_nns(const FN_composite_name &,
	    FN_status_cnsvc&);
	int cn_destroy_subcontext_nns(const FN_composite_name &,
	    FN_status_cnsvc &);
	int cn_rename_nns(const FN_composite_name &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_cnsvc &);
	FN_attrset* cn_get_syntax_attrs_nns(const FN_composite_name &,
	    FN_status_cnsvc &);

	// Attribute operations on next naming system pointers
	FN_attribute *cn_attr_get_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_modify_nns(const FN_composite_name &,
	    unsigned int modop, const FN_attribute &, unsigned int follow_link,
	    FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids_nns(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc &);
	FN_multigetlist *cn_attr_multi_get_nns(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_multi_modify_nns(const FN_composite_name &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_cnsvc &);

	// Extended attribute operations on Next naming system pointer
	int cn_attr_bind_nns(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status);
	FN_ref *cn_attr_create_subcontext_nns(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_cnsvc &status);
	FN_searchlist *cn_attr_search_nns(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status);
	FN_ext_searchlist *cn_attr_ext_search_nns(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status);

	// Operations that must be supplied by subclass
	virtual FN_ref *c_lookup(const FN_string &name,
	    unsigned int lookup_flag, FN_status_csvc &) = 0;
	virtual FN_namelist* c_list_names(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_bind(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc &) = 0;
	virtual int c_unbind(const FN_string &name, FN_status_csvc &) = 0;
	virtual FN_ref *c_create_subcontext(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_destroy_subcontext(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_rename(const FN_string &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_csvc &status) = 0;
	virtual FN_attrset* c_get_syntax_attrs(const FN_string &name,
	    FN_status_csvc &) = 0;

	// Attribute operations
	virtual FN_attribute *c_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual int c_attr_modify(const FN_string &, unsigned int modop,
	    const FN_attribute&, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual FN_valuelist *c_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual FN_attrset *c_attr_get_ids(const FN_string &,
	    unsigned int follow_link, FN_status_csvc &) = 0;
	virtual FN_multigetlist *c_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int follow_link, FN_status_csvc &) = 0;
	virtual int c_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_csvc &) = 0;

	// Extended attribute operations
	virtual int c_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status) = 0;
	virtual FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status) = 0;
	virtual FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status) = 0;
	virtual FN_ext_searchlist *c_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status) = 0;

	// Naming Operations on next naming system pointer
	virtual FN_ref *c_lookup_nns(const FN_string &name,
	    unsigned int lookup_fg, FN_status_csvc &) = 0;
	virtual FN_namelist* c_list_names_nns(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_bind_nns(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc &) = 0;
	virtual int c_unbind_nns(const FN_string &name, FN_status_csvc &) = 0;
	virtual FN_ref *c_create_subcontext_nns(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_destroy_subcontext_nns(const FN_string &name,
	    FN_status_csvc &) = 0;
	virtual int c_rename_nns(const FN_string &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_csvc &status) = 0;
	virtual FN_attrset* c_get_syntax_attrs_nns(const FN_string &name,
	    FN_status_csvc &) = 0;

	// Attribute operations
	virtual FN_attribute *c_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual int c_attr_modify_nns(const FN_string &, unsigned int modop,
	    const FN_attribute &, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual FN_valuelist *c_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_csvc &) = 0;
	virtual FN_attrset *c_attr_get_ids_nns(const FN_string &,
	    unsigned int follow_link, FN_status_csvc&) = 0;
	virtual FN_multigetlist *c_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int follow_link, FN_status_csvc &) = 0;
	virtual int c_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_csvc &) = 0;

	// Extended attribute operations
	virtual int c_attr_bind_nns(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status) = 0;
	virtual FN_ref *c_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status) = 0;
	virtual FN_searchlist *c_attr_search_nns(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status) = 0;
	virtual FN_ext_searchlist *c_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status) = 0;
};

/*
 * Single Component Static Service(Weakly separated)
 * Supports operations on a component name that is a single string and uses
 * a static method for determining the component within the name to resolve.
 *
 * Subclasses must provide implementations for:
 * 1. from_address prototype described in FN_ctx_svc
 * 2. c_ interfaces(provides compound name or atomic name operations using
 *     single input string name)
 * 3. p_component_parser interface
 *
 * Types of clients include those that support strong separation, or weak
 * separation with static determination of ns boundary, *and* expects
 * input name to be a single string.
 */

class FN_ctx_csvc_weak_static :
	public FN_ctx_cnsvc_weak_static,
	public FN_ctx_csvc
{
protected:
	FN_ctx_csvc_weak_static();
	virtual ~FN_ctx_csvc_weak_static();
};

/* For naming system that support strongly separated context */

class FN_ctx_csvc_strong :
public FN_ctx_csvc_weak_static
{
protected:
	FN_ctx_csvc_strong();
	virtual ~FN_ctx_csvc_strong();

	FN_composite_name *p_component_parser(const FN_composite_name &,
	    FN_composite_name **rest, FN_status_psvc& s);
};

/*
 * Atomic interface (a_) :
 *
 */
// declare FN_ctx_svc as virtual to allow one copy of FN_ctx_svc to be
// used when multiply inherited

class FN_ctx_asvc:
#ifndef BROKEN_SC3
virtual
#endif /* BROKEN_SC3 */
public FN_ctx_svc
{
protected:
	FN_ctx_asvc();
	virtual ~FN_ctx_asvc();

public:
	virtual FN_ref *a_lookup(const FN_string &name,
	    unsigned int lookup_flags, FN_status_asvc &) = 0;
	virtual FN_namelist* a_list_names(FN_status_asvc &) = 0;
	virtual FN_bindinglist* a_list_bindings(FN_status_asvc &) = 0;
	virtual int a_bind(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_asvc &) = 0;
	virtual int a_unbind(const FN_string &name, FN_status_asvc &) = 0;
	virtual FN_ref *a_create_subcontext(const FN_string &name,
	    FN_status_asvc &) = 0;
	virtual int a_destroy_subcontext(const FN_string &name,
	    FN_status_asvc &) = 0;
	virtual int a_rename(const FN_string &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_asvc &) = 0;
	virtual FN_attrset* a_get_syntax_attrs(FN_status_asvc &) = 0;

	// Attribute operations
	virtual FN_attribute *a_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual int a_attr_modify(const FN_string &,
	    unsigned int modop, const FN_attribute &,
	    unsigned int follow_link, FN_status_asvc &) = 0;
	virtual FN_valuelist *a_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual FN_attrset *a_attr_get_ids(const FN_string &,
	    unsigned int follow_link, FN_status_asvc &) = 0;
	virtual FN_multigetlist *a_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int follow_link, FN_status_asvc &) = 0;
	virtual int a_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_asvc &) = 0;

	// Extended attribute operations
	virtual int a_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_asvc &status) = 0;
	virtual FN_ref *a_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_asvc &status) = 0;
	virtual FN_searchlist *a_attr_search(
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_asvc &status) = 0;
	virtual FN_ext_searchlist *a_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_asvc &status) = 0;

	// Next Naming System (nns) operations

	virtual FN_ref *a_lookup_nns(const FN_string &,
	    unsigned int lookup_flags, FN_status_asvc &) = 0;
	virtual int a_bind_nns(const FN_string &,
	    const FN_ref &, unsigned bind_flags,
	    FN_status_asvc &) = 0;
	virtual int a_unbind_nns(const FN_string &, FN_status_asvc &) = 0;
	virtual FN_ref *a_create_subcontext_nns(const FN_string&,
	    FN_status_asvc &) = 0;
	virtual int a_destroy_subcontext_nns(const FN_string &,
	    FN_status_asvc &) = 0;
	virtual int a_rename_nns(const FN_string &,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_asvc &) = 0;

	// Attribute operations
	virtual FN_attribute *a_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual int a_attr_modify_nns(const FN_string &,
	    unsigned int modop,
	    const FN_attribute &,
	    unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual FN_valuelist *a_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual FN_attrset *a_attr_get_ids_nns(const FN_string &,
	    unsigned int follow_link, FN_status_asvc &) = 0;
	virtual FN_multigetlist *a_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int follow_link,
	    FN_status_asvc &) = 0;
	virtual int a_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **,
	    FN_status_asvc &) = 0;

	// Extended attribute operations
	virtual int a_attr_bind_nns(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_asvc &status) = 0;
	virtual FN_ref *a_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_asvc &status) = 0;
	// *a_attr_search_nns() implemented by framework
	virtual FN_ext_searchlist *a_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_asvc &status) = 0;
};

/*
* Single Compound Static Service:
*    Implements compound name resolution on top of atomic name interface.
*    The compound name is a single string and was extracted
*    from a composite name using static methods.
*
*    Adds declaration for a_ interfaces, which does atomic name operations.
*    Adds declaration for c_component_parser, which does compound name parsing.
*
*    Subclasses must provide implementations for:
*    1.  from_address described in FN_ctx_svc
*    2.  a_ interfaces(implements atomic name operations)
*    3.  c_component_parser(parses compound name)
*
*    Clients are those whose contexts are hierarchical, can determine the
*    ns boundary statically, and uses the XFN standard model for parsing
*    composite names, *and* wants the system to deal with the compound name
*    resolution.
*/

class FN_ctx_asvc_strong:
public FN_ctx_csvc_strong,
public FN_ctx_asvc
{
protected:
	FN_ctx_asvc_strong();
	virtual ~FN_ctx_asvc_strong();

	// protected:
	// implementation of FN_ctx_csvc_strong interface
	// (on top of compound name parser and atomic name service interface)
	FN_ref *c_lookup(const FN_string &name, unsigned int lookup_flags,
	    FN_status_csvc &);
	FN_namelist* c_list_names(const FN_string &name, FN_status_csvc &);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc &);
	int c_bind(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc &);
	int c_unbind(const FN_string &name, FN_status_csvc &);
	FN_ref *c_create_subcontext(const FN_string &name, FN_status_csvc &);
	int c_destroy_subcontext(const FN_string &name, FN_status_csvc &);
	int c_rename(const FN_string &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_csvc &);
	FN_attrset* c_get_syntax_attrs(const FN_string &name,
	    FN_status_csvc &);

	// Attribute operations
	FN_attribute *c_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int follow_link, FN_status_csvc &);
	int c_attr_modify(const FN_string &, unsigned int modop,
	    const FN_attribute &, unsigned int follow_link, FN_status_csvc &);
	FN_valuelist *c_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int follow_link, FN_status_csvc &);
	FN_attrset *c_attr_get_ids(const FN_string &,
	    unsigned int follow_link, FN_status_csvc &);
	FN_multigetlist *c_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int follow_link, FN_status_csvc &);
	int c_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link, FN_attrmodlist **,
	    FN_status_csvc &);

	// Extended attribute operations
	int c_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

	// operations on Next naming system pointer
	FN_ref *c_lookup_nns(const FN_string &name, unsigned int lookup_flags,
	    FN_status_csvc &);
	FN_namelist* c_list_names_nns(const FN_string &name, FN_status_csvc &);
	FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc &);
	int c_bind_nns(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc &);
	int c_unbind_nns(const FN_string &name, FN_status_csvc &);
	FN_ref *c_create_subcontext_nns(const FN_string &name,
	    FN_status_csvc &);
	int c_destroy_subcontext_nns(const FN_string &name, FN_status_csvc &);
	int c_rename_nns(const FN_string &oldname,
	    const FN_composite_name &newname, unsigned int exclusive,
	    FN_status_csvc &);
	FN_attrset* c_get_syntax_attrs_nns(const FN_string &name,
	    FN_status_csvc &);

	// Attribute operations
	FN_attribute *c_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link, FN_status_csvc &);
	int c_attr_modify_nns(const FN_string &, unsigned int modop,
	    const FN_attribute &, unsigned int follow_link, FN_status_csvc &);
	FN_valuelist *c_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int follow_link, FN_status_csvc &);
	FN_attrset *c_attr_get_ids_nns(const FN_string &,
	    unsigned int follow_link, FN_status_csvc &);
	FN_multigetlist *c_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int follow_link, FN_status_csvc &);
	int c_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist &, unsigned int follow_link,
	    FN_attrmodlist **, FN_status_csvc &);

	// Extended attribute operations on next naming system pointer
	int c_attr_bind_nns(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search_nns(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

public:
	// compound name parser
	virtual FN_string *c_component_parser(const FN_string &,
	    FN_string **rest, FN_status_csvc &status) = 0;
};


/*
* Multiple Compound Service:
* Implements compound name resolution using multiple components.
*
* Adds a_ interface declarations for atomic name operations and
* implements cn_ interfaces using them.
*
* Subclasses must provide implementation for
* 1. from_address described in FN_ctx_svc
* 2. a_ interfaces(which implements atomic name operations)
* 3. p_ interfaces(which determines ns boundary on top of cn_)
*
* Clients are those whose contexts are hierarchical(l-to-r /)and
* supports weak separation, and which wants the system to do the
* compound name resolution for it(and wants to define the p_ interfaces).
*/

// declare FN_ctx_cnsvc as virtual to allow one copy of FN_ctx_cnsvc to be
// used when multiply inherited

class FN_ctx_asvc_weak:
#ifndef BROKEN_SC3
virtual
#endif /* BROKEN_SC3 */
public FN_ctx_cnsvc,
public FN_ctx_asvc
{
protected:
	FN_ctx_asvc_weak();
	virtual ~FN_ctx_asvc_weak();

	// protected:
	// implementation of FN_ctx_cnsvc  interface (on top
	// atomic name service interface)
	FN_ref *cn_lookup(const FN_composite_name &name,
	    unsigned int lookup_flags, FN_status_cnsvc &);
	FN_namelist* cn_list_names(const FN_composite_name &name,
	    FN_status_cnsvc &);
	FN_bindinglist* cn_list_bindings(const FN_composite_name &name,
	    FN_status_cnsvc &);
	int cn_bind(const FN_composite_name &name, const FN_ref &,
	    unsigned bind_flags, FN_status_cnsvc &);
	int cn_unbind(const FN_composite_name &name, FN_status_cnsvc&);
	FN_ref *cn_create_subcontext(const FN_composite_name &name,
	    FN_status_cnsvc&);
	int cn_destroy_subcontext(const FN_composite_name &name,
	    FN_status_cnsvc &);
	int cn_rename(const FN_composite_name &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_cnsvc &);
	FN_attrset* cn_get_syntax_attrs(const FN_composite_name &name,
	    FN_status_cnsvc &);

	// Attribute operations
	FN_attribute *cn_attr_get(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc&);
	int cn_attr_modify(const FN_composite_name &,
	    unsigned int modop, const FN_attribute &, unsigned int follow_link,
	    FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link, FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc &);
	FN_multigetlist *cn_attr_multi_get(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_multi_modify(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link, FN_attrmodlist **,
	    FN_status_cnsvc &);
	int cn_attr_bind(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status);
	FN_ref *cn_attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_cnsvc &status);
	FN_searchlist *cn_attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status);
	FN_ext_searchlist *cn_attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status);

	// Next naming system pointer operations
	FN_ref *cn_lookup_nns(const FN_composite_name &name,
	    unsigned int lookup_flags, FN_status_cnsvc &);
	FN_namelist* cn_list_names_nns(const FN_composite_name &name,
	    FN_status_cnsvc &);
	FN_bindinglist* cn_list_bindings_nns(const FN_composite_name &name,
	    FN_status_cnsvc &);
	int cn_bind_nns(const FN_composite_name &name, const FN_ref &,
	    unsigned bind_flags, FN_status_cnsvc &);
	int cn_unbind_nns(const FN_composite_name &name, FN_status_cnsvc &);
	FN_ref *cn_create_subcontext_nns(const FN_composite_name &name,
	    FN_status_cnsvc &);
	int cn_destroy_subcontext_nns(const FN_composite_name &name,
	    FN_status_cnsvc &);
	int cn_rename_nns(const FN_composite_name &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_cnsvc &);
	FN_attrset* cn_get_syntax_attrs_nns(const FN_composite_name &name,
	    FN_status_cnsvc &);

	// Attribute operations
	FN_attribute *cn_attr_get_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc &);
	int cn_attr_modify_nns(const FN_composite_name &,
	    unsigned int modop,
	    const FN_attribute &, unsigned int follow_link,
	    FN_status_cnsvc &);
	FN_valuelist *cn_attr_get_values_nns(const FN_composite_name &,
	    const FN_identifier &, unsigned int follow_link,
	    FN_status_cnsvc &);
	FN_attrset *cn_attr_get_ids_nns(const FN_composite_name &,
	    unsigned int follow_link, FN_status_cnsvc&);
	FN_multigetlist *cn_attr_multi_get_nns(const FN_composite_name &,
	    const FN_attrset *, unsigned int follow_link, FN_status_cnsvc &);
	int cn_attr_multi_modify_nns(const FN_composite_name &,
	    const FN_attrmodlist&, unsigned int follow_link,
	    FN_attrmodlist **,
	    FN_status_cnsvc &);

	// Extended attribute operations on Next naming system pointer
	int cn_attr_bind_nns(const FN_composite_name &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_cnsvc &status);
	FN_ref *cn_attr_create_subcontext_nns(const FN_composite_name &name,
	    const FN_attrset *attr, FN_status_cnsvc &status);
	FN_searchlist *cn_attr_search_nns(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_cnsvc &status);
	FN_ext_searchlist *cn_attr_ext_search_nns(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_cnsvc &status);
};

/*
* Multiple Compound Static Service:
* Implements compound name resolution using multiple components(e.g. weakly
* separated name) whose naming system boundary is determined statically eg.,
* by syntax.
*
* Subclasses must provide implementation for
* 1. from_address described in FN_ctx_svc
* 2. a_ interfaces(which implements atomic name operations)
* 3. p_component_parser
*
* Clients are those whose contexts are hierarchical(l-to-r /)and
* supports weak separation, and which wants the system to do the
* compound name resolution for it.
*/
class FN_ctx_asvc_weak_static :
public FN_ctx_cnsvc_weak_static,
public FN_ctx_asvc_weak
{
protected:
	FN_ctx_asvc_weak_static();
	virtual ~FN_ctx_asvc_weak_static();
};

/*
* Dynamic Component Boundary Service:
* Supports component resolution based on dynamic discover of naming
* system boundary during resolution.
* This is useful for contexts that support weak separation and does not
* use static boundary discovery.  It defines implementation for p_resolve
* routines that expects the target ns to return unresolved remaining
* components.
*
* Subclass must provide implementations for
* 1. from_address described in FN_ctx_svc
* 2. cn_ interfaces
*
*/
class FN_ctx_cnsvc_weak_dynamic:
public FN_ctx_cnsvc_impl
{
protected:
	FN_ctx_cnsvc_weak_dynamic();
	virtual ~FN_ctx_cnsvc_weak_dynamic();

	virtual int p_resolve(const FN_composite_name &n,
	    FN_status_psvc &s,
	    FN_composite_name **ret_fn);
};

/*
* Single component dynamic service:
*   Like FN_ctx_cnsvc_weak_dynamic except defines c_ interfaces
*   to enable clients to expect input name in single string form.
*
*   Subclasses must provide implementations for:
*   1. from_address described in FN_ctx_svc
*   2. c_ interfaces(component name, expects input name in string form)
*/
class FN_ctx_csvc_weak_dynamic:
public FN_ctx_cnsvc_weak_dynamic,
public FN_ctx_csvc
{
protected:
	FN_ctx_csvc_weak_dynamic();
	virtual ~FN_ctx_csvc_weak_dynamic();
};

/*
* Multiple Compound Dynamic Service:
* Implements compound name resolution using multiple components of
* weakly separated composite name whose naming system boundary is to be
* determined dynamically during resolution.
*
*   Subclasses must provide implementation for
*   1. from_address described in FN_ctx_svc
*   2. a_ interfaces(which implements atomic name operations)
*/
class FN_ctx_asvc_weak_dynamic:
public FN_ctx_cnsvc_weak_dynamic,
public FN_ctx_asvc_weak
{
protected:
	FN_ctx_asvc_weak_dynamic();
	virtual ~FN_ctx_asvc_weak_dynamic();
};

// Compound name Support
#include <xfn/FN_syntax_standard.hh>
#include <xfn/FN_compound_name_standard.hh>

// Iterator Service APIs
#include <xfn/FN_namelist_svc.hh>
#include <xfn/FN_bindinglist_svc.hh>
#include <xfn/FN_valuelist_svc.hh>
#include <xfn/FN_multigetlist_svc.hh>
#include <xfn/FN_searchlist_svc.hh>
#include <xfn/FN_ext_searchlist_svc.hh>

#endif /* _XFN_FN_SPI_HH */
