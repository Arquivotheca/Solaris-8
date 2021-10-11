/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_CTX_HH
#define	_XFN_FN_CTX_HH

#pragma ident	"@(#)FN_ctx.hh	1.5	96/03/31 SMI"

#include <sys/types.h>  /* for uid_t */
#include <xfn/FN_ctx.h>
#include <xfn/FN_ref.hh>
#include <xfn/FN_composite_name.hh>
#include <xfn/FN_status.hh>
#include <xfn/FN_attrset.hh>
#include <xfn/FN_attrmodlist.hh>
#include <xfn/FN_search_control.hh>
#include <xfn/FN_search_filter.hh>

class FN_namelist {
public:
    virtual ~FN_namelist();
    virtual FN_string* next(FN_status&) = 0;

};

class FN_bindinglist {
public:
    virtual ~FN_bindinglist();
    virtual FN_string* next(FN_ref** ref, FN_status&) = 0;
};


class FN_valuelist {
public:
    virtual ~FN_valuelist();
    virtual FN_attrvalue *next(FN_identifier **,
	FN_status&) = 0;
};

class FN_multigetlist {
public:
    virtual ~FN_multigetlist();
    virtual FN_attribute *next(FN_status&) = 0;
};

class FN_searchlist {
public:
    virtual ~FN_searchlist();
    virtual FN_string *next(FN_ref **returned_ref,
	FN_attrset **returned_attrs,
	FN_status&) = 0;
};

class FN_ext_searchlist {
public:
    virtual ~FN_ext_searchlist();
    virtual FN_composite_name *next(FN_ref **returned_ref,
	FN_attrset **returned_attrs,
	unsigned int &relative, // nonzero means relative to starting ctx
	FN_status&) = 0;
};

class FN_ctx {
    public:
	virtual ~FN_ctx();

	// construct handle to initial context
	static FN_ctx* from_initial(unsigned int auth, FN_status&);

	// construct handle to initial context given uid
	static FN_ctx* from_initial_with_uid(uid_t uid, unsigned int auth,
		FN_status&);

	// constuct handle to initial context with the given
	// naming service
	static FN_ctx* from_initial_with_ns(int ns,
		unsigned int auth, FN_status&);

	// construct context from a reference
	static FN_ctx* from_ref(const FN_ref&, unsigned int auth, FN_status&);

	// get reference for this context
	virtual FN_ref* get_ref(FN_status&) const = 0;

	// look up the binding of a name
	virtual FN_ref* lookup(const FN_composite_name&, FN_status&) = 0;

	// list all the (atomic) names bound in the named context
	virtual FN_namelist* list_names(const FN_composite_name&,
					FN_status&) = 0;

	// list all the bindings in the named context
	virtual FN_bindinglist* list_bindings(const FN_composite_name&,
					    FN_status&) = 0;

	// bind a name to a reference
	virtual int bind(const FN_composite_name&,
			    const FN_ref&,
			    unsigned int exclusive,
			    FN_status&) = 0;

	// unbind a name
	virtual int unbind(const FN_composite_name&, FN_status&) = 0;

	virtual int rename(const FN_composite_name &oldname,
			    const FN_composite_name &newname,
			    unsigned int exclusive,
			    FN_status &status) = 0;

	// create a subcontext
	virtual FN_ref* create_subcontext(const FN_composite_name&,
					    FN_status&) = 0;

	// destroy a subcontext
	virtual int destroy_subcontext(const FN_composite_name&,
	    FN_status&) = 0;

	virtual FN_ref *lookup_link(const FN_composite_name &name,
			FN_status &status) = 0;

	virtual FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status) = 0;

	// get the syntax attributes of the named context
	virtual FN_attrset* get_syntax_attrs(const FN_composite_name&,
					    FN_status&) = 0;


	// Attribute operations

	// To obtain a single attribute
	virtual FN_attribute *attr_get(const FN_composite_name&,
	    const FN_identifier&,
	    unsigned int follow_link,
	    FN_status&) = 0;

	// To modifiy a single attribute
	virtual int attr_modify(const FN_composite_name&,
				unsigned int,
				const FN_attribute&,
				unsigned int follow_link,
				FN_status&) = 0;

	// Obtain multiple attribute values
	virtual FN_valuelist *attr_get_values(const FN_composite_name&,
					    const FN_identifier&,
					    unsigned int follow_link,
					    FN_status&) = 0;

	// Obtain the set of attributes
	virtual FN_attrset *attr_get_ids(const FN_composite_name&,
					    unsigned int follow_link,
					    FN_status&) = 0;

	// Attribute operations for multiple attribute list
	virtual FN_multigetlist *attr_multi_get(const FN_composite_name&,
						const FN_attrset *,
						unsigned int follow_link,
						FN_status&) = 0;

	// Operations to modify multiple attributes
	virtual int attr_multi_modify(const FN_composite_name&,
				    const FN_attrmodlist&,
				    unsigned int follow_link,
				    FN_attrmodlist**,
				    FN_status&) = 0;

	// Extended Attribute interface

	virtual int attr_bind(const FN_composite_name &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status &status) = 0;

	virtual FN_ref *attr_create_subcontext(const FN_composite_name &name,
	    const FN_attrset *attr,
	    FN_status &status) = 0;

	virtual FN_searchlist *attr_search(
	    const FN_composite_name &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_id,
	    FN_status &status) = 0;

	virtual FN_ext_searchlist *attr_ext_search(
	    const FN_composite_name &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status &status) = 0;

};

#endif /* _XFN_FN_CTX_HH */
