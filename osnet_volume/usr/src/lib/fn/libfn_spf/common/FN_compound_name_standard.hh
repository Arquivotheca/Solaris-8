/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN__FN_COMPOUND_NAME_STANDARD_HH
#define	_XFN__FN_COMPOUND_NAME_STANDARD_HH

#pragma ident "@(#)FN_compound_name_standard.hh	1.3 94/10/03 SMI"

#include <xfn/xfn.h>

#include "FN_syntax_standard.hh"

class FN_compound_name_standard_rep;

/*
 * Implements compound name resolution using multiple components.
 */

class FN_compound_name_standard : public FN_compound_name {
	FN_compound_name *dup() const;
public:
	~FN_compound_name_standard();

	// construct an FN_compound_name_standard
	FN_compound_name_standard(const FN_syntax_standard &);
	// construct and initialize an FN_compound_name_standard
	FN_compound_name_standard(const FN_syntax_standard &,
	    const FN_string &name);

	// get the Syntax for this FN_compound_name_standard
	FN_syntax_standard *get_syntax(void) const;

	// construct an FN_compound_name_standard from an FN_ctx FN_attrset
	static FN_compound_name_standard *from_syntax_attrs(const FN_attrset &,
	    const FN_string &name, FN_status &);

	// get the FN_attrset for this FN_compound_name_standard
	FN_attrset *get_syntax_attrs(void) const;

	// copy and assignment
	FN_compound_name_standard(const FN_compound_name_standard &);
	FN_compound_name& operator=(const FN_compound_name &);

	// convert to string representation
	FN_string *string(void) const;

	// syntactic comparison (extensions)
	operator==(const FN_compound_name &) const;
	operator!=(const FN_compound_name &) const;

	// extensions
	operator<(const FN_compound_name &) const;
	operator>(const FN_compound_name &) const;
	operator<=(const FN_compound_name &) const;
	operator>=(const FN_compound_name &) const;

	// get count of components in name
	unsigned count(void) const;
	int is_empty(void) const;

	int is_equal(const FN_compound_name &, unsigned int *status = 0) const;

	// get first component (points iter_pos after name)
	const FN_string *first(void *&iter_pos) const;

	// test for prefix (points iter_pos after prefix)
	int is_prefix(const FN_compound_name &, void *&iter_pos,
	    unsigned int *status = 0) const;

	// get last component (points iter_pos before name)
	const FN_string *last(void *&iter_pos) const;

	// test for suffix (points iter_pos before suffix)
	int is_suffix(const FN_compound_name &,
	    void *&iter_pos,
	    unsigned int *status = 0) const;

	// get component following iter_pos (points iter_pos after component)
	const FN_string *next(void *&iter_pos) const;
	// get component before iter_pos (points iter_pos before component)
	const FN_string *prev(void *&iter_pos) const;

	// get copy of name from first component through iter_pos
	FN_compound_name *prefix(const void *iter_pos) const;
	// get copy of name from iter_pos through last component
	FN_compound_name *suffix(const void *iter_pos) const;

	// prepend component to name
	int prepend_comp(const FN_string &, unsigned int *status = 0);
	// append component to name
	int append_comp(const FN_string &, unsigned int *status = 0);

	// insert component before iter_pos
	int insert_comp(void *&iter_pos, const FN_string &,
	    unsigned int *status = 0);
	// delete component before iter_pos
	int delete_comp(void *&iter_pos);
	// delete all components
	int delete_all(void);

protected:
	FN_compound_name_standard_rep *rep;
	static FN_compound_name_standard_rep *get_rep(
	    const FN_compound_name_standard &);
	FN_compound_name_standard(FN_compound_name_standard_rep *);
};

#endif /* _XFN__FN_COMPOUND_NAME_STANDARD_HH */
