/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_search_control_c.cc	1.1	96/03/31 SMI"

#include <xfn/FN_search_control.hh>

extern "C"
FN_search_control_t *
prelim_fn_search_control_create(
	unsigned int scope,
	unsigned int follow_links,
	unsigned int max_names,
	unsigned int return_ref,
	const FN_attrset_t *return_attr_ids,
	unsigned int *status)
{
	FN_search_control *answer;
	unsigned int s;

	answer = new FN_search_control(scope, follow_links, max_names,
	    return_ref, (const FN_attrset *)return_attr_ids, s);
	if (status)
		*status = s;

	return ((FN_search_control_t *)answer);
}

extern "C"
void
prelim_fn_search_control_destroy(FN_search_control_t *scontrol)
{
	delete (FN_search_control *)scontrol;
}

extern "C"
FN_search_control_t *
prelim_fn_search_control_copy(const FN_search_control_t *scontrol)
{
	return ((FN_search_control_t *)new FN_search_control(
	    *((const FN_search_control *)scontrol)));
}

extern "C"
FN_search_control_t *
prelim_fn_search_control_assign(FN_search_control_t *dst,
    const FN_search_control_t *src)
{
	return ((FN_search_control_t *)&(*((FN_search_control *)dst) =
					 *((const FN_search_control *)src)));
}

extern "C"
unsigned int
prelim_fn_search_control_scope(const FN_search_control_t *scontrol)
{
	return (((const FN_search_control *)scontrol)->scope());
}

extern "C"
unsigned int
prelim_fn_search_control_follow_links(const FN_search_control_t *scontrol)
{
	return (((const FN_search_control *)scontrol)->follow_links());
}

extern "C"
unsigned int
prelim_fn_search_control_max_names(const FN_search_control_t *scontrol)
{
	return (((const FN_search_control *)scontrol)->max_names());
}

extern "C"
unsigned int
prelim_fn_search_control_return_ref(const FN_search_control_t *scontrol)
{
	return (((const FN_search_control *)scontrol)->return_ref());
}

extern "C"
const FN_attrset_t *
prelim_fn_search_control_return_attr_ids(const FN_search_control_t *scontrol)
{
	const FN_attrset *answer =
		((const FN_search_control *)scontrol)->return_attr_ids();
	return ((const FN_attrset_t *)answer);
}
