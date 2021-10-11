/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_search_filter_c.cc	1.1	96/03/31 SMI"

#include <xfn/FN_search_filter.h>
#include <xfn/FN_search_filter.hh>
#include <stdarg.h>

extern "C"
FN_search_filter_t *
prelim_fn_search_filter_create(
	unsigned int *status,
	const unsigned char *estr,
	...)
{
	va_list args;
	FN_search_filter *answer;
	unsigned int s;

	va_start(args, estr);
	answer = new FN_search_filter(s, estr, args);
	va_end(args);

	if (status)
		*status = s;

	return ((FN_search_filter_t *)answer);
}

extern "C"
void
prelim_fn_search_filter_destroy(FN_search_filter_t *sfilter)
{
	delete (FN_search_filter *)sfilter;
}

extern "C"
FN_search_filter_t *
prelim_fn_search_filter_copy(const FN_search_filter_t *sfilter)
{
	return ((FN_search_filter_t *)new FN_search_filter(
	    *((const FN_search_filter *)sfilter)));
}

extern "C"
FN_search_filter_t *
prelim_fn_search_filter_assign(FN_search_filter_t *dst,
    const FN_search_filter_t *src)
{
	return ((FN_search_filter_t *)&(*((FN_search_filter *)dst) =
					 *((const FN_search_filter *)src)));
}

extern "C"
const unsigned char *
prelim_fn_search_filter_expression(const FN_search_filter_t *sfilter)
{
	return (((const FN_search_filter *)sfilter)->filter_expression());
}

extern "C"
const void **
prelim_fn_search_filter_arguments(const FN_search_filter_t *sfilter,
	size_t *number_of_arguments)
{
	size_t num_args;
	const void **answer;

	answer = ((const FN_search_filter *)sfilter)->
	    filter_arguments(&num_args);

	if (number_of_arguments)
		*number_of_arguments = num_args;

	return (answer);
}
