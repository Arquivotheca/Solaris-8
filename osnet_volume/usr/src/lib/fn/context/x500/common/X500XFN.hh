/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_X500XFN_HH
#define	_X500XFN_HH

#pragma ident	"@(#)X500XFN.hh	1.1	96/03/31 SMI"


#include <xfn/xfn.h>
#include <xfn/fn_spi.hh>
#include "X500Trace.hh"


/*
 * XFN data structure manipulation
 */


class X500XFN : public X500Trace
{

protected:

	static const FN_identifier	x500;
	static const FN_identifier	paddr;
	static const FN_identifier	ascii;
	static const FN_identifier	octet;

	static const int		max_ref_length;
	static const int		max_filter_length;
	static const int		max_stack_length;
	static const int		max_dn_length;


public:

	unsigned char		*string_to_id_format(unsigned char *cp,
				    unsigned int &format) const;

	unsigned char		*id_format_to_string(unsigned int format,
				    unsigned char *cp) const;

	FN_ref			*string_ref_to_ref(unsigned char *sref,
				    unsigned int len, FN_attrset **attrs,
				    int &err) const;

	unsigned char		*ref_to_string_ref(const FN_ref	*ref,
				    int *length) const;

	FN_attrmodlist		*attrs_and_ref_to_mods(const FN_attrset *attrs,
				    const FN_ref *ref, int &err);

	int			is_or(const unsigned char *fx);

	int			is_and(const unsigned char *fx);

	int			is_not(const unsigned char *fx);

	int			is_equal(const unsigned char *fx);

	int			is_not_equal(const unsigned char *fx);

	int			is_approx_equal(const unsigned char *fx);

	int			is_greater_or_equal(const unsigned char *fx);

	int			is_greater_than(const unsigned char *fx);

	int			is_less_or_equal(const unsigned char *fx);

	int			is_less_than(const unsigned char *fx);

	unsigned char		*parenthesize_filter_expression(
				    unsigned char *px, unsigned char *&ppx,
				    int operands);

	unsigned char		*substitute_filter_arguments(unsigned char *fx,
				    const FN_search_filter *filter, int &err);

	unsigned char		*filter_expression_to_prefix(
				    const unsigned char *ix, int ix_len,
				    unsigned char *px, int px_len);

	unsigned char		*filter_to_string(
				    const FN_search_filter *filter, int &err);


	X500XFN() {};
	virtual ~X500XFN() {};
};


#endif	/* _X500XFN_HH */
