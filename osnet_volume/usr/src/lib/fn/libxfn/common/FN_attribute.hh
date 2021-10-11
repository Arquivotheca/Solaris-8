/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_ATTRIBUTE_HH
#define	_XFN_FN_ATTRIBUTE_HH

#pragma ident	"@(#)FN_attribute.hh	1.4	96/03/31 SMI"

#include <xfn/FN_identifier.hh>
#include <xfn/FN_attribute.h>
#include <xfn/FN_attrvalue.hh>
#include <xfn/misc_codes.h>

class FN_attribute_rep;

class FN_attribute {
public:
	FN_attribute(const FN_identifier &id, const FN_identifier &syntax);
	~FN_attribute();

	FN_attribute(const FN_attribute &);
	FN_attribute &operator=(const FN_attribute &);

	const FN_identifier *identifier(void) const;
	const FN_identifier *syntax(void) const;
	unsigned int valuecount(void) const;

	const FN_attrvalue *first(void *&iter_pos) const;
	const FN_attrvalue *next(void *&iter_pos) const;

	int add(const FN_attrvalue &av,
		unsigned int exclusive = FN_OP_EXCLUSIVE);
	int remove(const FN_attrvalue &av);

	// %%% extensions
	// compares identifier, syntax and attribute values
	int operator==(const FN_attribute &)const;
	int operator!=(const FN_attribute &)const;

protected:
	FN_attribute_rep *rep;
	static FN_attribute_rep *get_rep(const FN_attribute &);
	FN_attribute(FN_attribute_rep *);

private:
	FN_attribute();
};

#endif /* _XFN_FN_ATTRIBUTE_HH */
