/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_COMPOUND_NAME_HH
#define	_XFN_FN_COMPOUND_NAME_HH

#pragma ident	"@(#)FN_compound_name.hh	1.4	96/03/31 SMI"

/*
 * Declarations for compound names.
 */

#include <xfn/FN_string.hh>
#include <xfn/FN_attrset.hh>
#include <xfn/FN_status.hh>

#include <xfn/FN_compound_name.h>

class FN_compound_name {
    public:
	// duplicate the object
	virtual FN_compound_name *dup() const = 0;

	// construct an FN_compound_name from an FN_ctx's FN_attrset
	static FN_compound_name *from_syntax_attrs(const FN_attrset &aset,
						    const FN_string &name,
						    FN_status &status);

	// get the FN_attrset for this compound name's syntax
	virtual FN_attrset *get_syntax_attrs() const = 0;

	// FN_compound_name(const FN_compound_name &);
	virtual ~FN_compound_name();

	// convert to string representation
	virtual FN_string *string(void) const = 0;
	virtual FN_compound_name &operator=(const FN_compound_name &) = 0;

	virtual unsigned int count(void) const = 0;

	virtual const FN_string *first(void *&iter_pos) const = 0;
	virtual const FN_string *next(void *&iter_pos) const = 0;
	virtual const FN_string *prev(void *&iter_pos) const = 0;
	virtual const FN_string *last(void *&iter_pos) const = 0;

	virtual FN_compound_name *prefix(const void *iter_pos) const = 0;
	virtual FN_compound_name *suffix(const void *iter_pos) const = 0;

	virtual int is_empty(void) const = 0;
	virtual int is_equal(const FN_compound_name &,
			    unsigned int *status = 0) const = 0;
	virtual int is_prefix(const FN_compound_name &prefix,
			    void *&iter_pos,
			    unsigned int *status = 0) const = 0;
	virtual int is_suffix(const FN_compound_name &suffix,
			    void *&iter_pos,
			    unsigned int *status = 0) const = 0;

	virtual int prepend_comp(const FN_string &atomic_comp,
				    unsigned int *status = 0) = 0;
	virtual int append_comp(const FN_string &atomic_comp,
				unsigned int *status = 0) = 0;

	virtual int insert_comp(void *&iter_pos,
				const FN_string &atomic_comp,
				unsigned int *status = 0) = 0;
	virtual int delete_comp(void *&iter_pos) = 0;

	virtual int delete_all(void) = 0;

#if 0
	// syntactic comparison; extensions
	virtual operator==(const FN_compound_name&) const = 0;
	virtual operator!=(const FN_compound_name&) const = 0;
#endif
};

#endif /* _XFN_FN_COMPOUND_NAME_HH */
