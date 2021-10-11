/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_ATTRVALUE_HH
#define	_XFN_ATTRVALUE_HH

#pragma ident	"@(#)FN_attrvalue.hh	1.4	94/08/03 SMI"

#include <xfn/FN_attrvalue.h>
#include <xfn/FN_string.hh>

class FN_attrvalue {
public:
	FN_attrvalue_t value;

	FN_attrvalue();
	FN_attrvalue(const unsigned char *);
	FN_attrvalue(const unsigned char *, size_t len);

	FN_attrvalue(const void *, size_t len);
	FN_attrvalue(const FN_attrvalue &);
	FN_attrvalue(const FN_attrvalue_t &);
	FN_attrvalue(const FN_string &);
	virtual ~FN_attrvalue();

	FN_attrvalue &operator=(const FN_attrvalue &);

	FN_string *string() const;

	size_t length() const { return (value.length); }
	void *contents() const { return (value.contents); }

	operator==(const FN_attrvalue &) const;
	operator!=(const FN_attrvalue &) const;
};

#endif // _XFN_ATTRVALUE_HH
