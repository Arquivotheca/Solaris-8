#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)xfn.spec	1.2	99/05/14 SMI"
#
# lib/fn/libxfn/spec/xfn.spec

function	fn_composite_name_from_string
include		<xfn/xfn.h>
declaration	FN_composite_name_t *fn_composite_name_from_string( \
			const FN_string_t *str)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_string_from_composite_name
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_from_composite_name( \
			const FN_composite_name_t *name, unsigned int *status)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ref_create_link
include		<xfn/xfn.h>
declaration	FN_ref_t *fn_ref_create_link( \
			const FN_composite_name_t *link_name)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_status_set_link_remaining_name
include		<xfn/xfn.h>
declaration	int fn_status_set_link_remaining_name(\
			FN_status_t *stat, const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_set_link_resolved_name
include		<xfn/xfn.h>
declaration	int fn_status_set_link_resolved_name(FN_status_t *stat, \
			const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_set_link_resolved_ref
include		<xfn/xfn.h>
declaration	int fn_status_set_link_resolved_ref(FN_status_t	*stat, \
			const FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_ref_is_link
include		<xfn/xfn.h>
declaration	int fn_ref_is_link(const FN_ref_t *ref)
version		SUNW_1.1
exception	$return == 0
end

function	fn_ref_link_name
include		<xfn/xfn.h>
declaration	FN_composite_name_t *fn_ref_link_name(const FN_ref_t *link_ref)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ctx_lookup_link
declaration	FN_ref_t *fn_ctx_lookup_link(FN_ctx_t *ctx, \
			const FN_composite_name_t *name, FN_status_t *status)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_status_link_code
include		<xfn/xfn.h>
declaration	unsigned int fn_status_link_code(const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_link_remaining_name
include		<xfn/xfn.h>
declaration	const FN_composite_name_t *fn_status_link_remaining_name( \
			const FN_status_t *stat)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_status_link_resolved_name
include		<xfn/xfn.h>
declaration	const FN_composite_name_t *fn_status_link_resolved_name( \
			const FN_status_t *stat)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_status_link_resolved_ref
include		<xfn/xfn.h>
declaration	const FN_ref_t *fn_status_link_resolved_ref( \
			const FN_status_t *stat)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_status_set_link_code
include		<xfn/xfn.h>
declaration	int fn_status_set_link_code(FN_status_t	*stat, \
			unsigned int linkcode)
version		SUNW_1.1
end

function	fn_ref_addr_assign
include		<xfn/xfn.h>
declaration	FN_ref_addr_t *fn_ref_addr_assign(FN_ref_addr_t *dst, \
			const FN_ref_addr_t *src)
version		SUNW_1.1
end

function	fn_ref_addr_copy
include		<xfn/xfn.h>
declaration	FN_ref_addr_t *fn_ref_addr_copy(const FN_ref_addr_t *addr)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ref_addr_create
include		<xfn/xfn.h>
declaration	FN_ref_addr_t *fn_ref_addr_create( \
			const FN_identifier_t *type, \
			size_t length, \
			const void *data)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ref_addr_data
include		<xfn/xfn.h>
declaration	const void *fn_ref_addr_data(const FN_ref_addr_t *addr)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ref_addr_description
include		<xfn/xfn.h>
declaration	FN_string_t *fn_ref_addr_description( \
			const FN_ref_addr_t *addr, \
			unsigned int detail, \
			unsigned int *more_detail)
version		SUNW_1.1
exception	$return == NULL
end

function	fn_ref_addr_destroy
include		<xfn/xfn.h>
declaration	void fn_ref_addr_destroy(FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_addr_length
include		<xfn/xfn.h>
declaration	size_t fn_ref_addr_length(const FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_addr_type
include		<xfn/xfn.h>
declaration	const FN_identifier_t *fn_ref_addr_type( \
			const FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_addrcount
include		<xfn/xfn.h>
declaration	unsigned int fn_ref_addrcount(const FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_ref_append_addr
include		<xfn/xfn.h>
declaration	int fn_ref_append_addr(FN_ref_t *ref, \
			const FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_assign
include		<xfn/xfn.h>
declaration	FN_ref_t *fn_ref_assign(FN_ref_t *dst, \
			const FN_ref_t *src)
version		SUNW_1.1
end

function	fn_ref_copy
include		<xfn/xfn.h>
declaration	FN_ref_t *fn_ref_copy(const FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_ref_create
include		<xfn/xfn.h>
declaration	FN_ref_t *fn_ref_create(const FN_identifier_t *ref_type)
version		SUNW_1.1
end

function	fn_ref_delete_addr
include		<xfn/xfn.h>
declaration	int fn_ref_delete_addr(FN_ref_t *ref, void **iter_pos)
version		SUNW_1.1
end

function	fn_ref_delete_all
include		<xfn/xfn.h>
declaration	int fn_ref_delete_all(FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_ref_description
include		<xfn/xfn.h>
declaration	FN_string_t *fn_ref_description(const FN_ref_t *ref, \
			unsigned int detail, unsigned int *more_detail)
version		SUNW_1.1
end

function	fn_ref_destroy
include		<xfn/xfn.h>
declaration	void fn_ref_destroy(FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_ref_first
include		<xfn/xfn.h>
declaration	const FN_ref_addr_t *fn_ref_first(const FN_ref_t *ref, \
			void **iter_pos)
version		SUNW_1.1
end

function	fn_ref_insert_addr
include		<xfn/xfn.h>
declaration	int fn_ref_insert_addr(FN_ref_t *ref, \
			void **iter_pos, const FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_next
include		<xfn/xfn.h>
declaration	const FN_ref_addr_t *fn_ref_next(const FN_ref_t *ref, \
			void **iter_pos)
version		SUNW_1.1
end

function	fn_ref_prepend_addr
include		<xfn/xfn.h>
declaration	int fn_ref_prepend_addr(FN_ref_t *ref, \
			const FN_ref_addr_t *addr)
version		SUNW_1.1
end

function	fn_ref_type
include		<xfn/xfn.h>
declaration	const FN_identifier_t *fn_ref_type(const FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_status_advance_by_name
include		<xfn/xfn.h>
declaration	int fn_status_advance_by_name(FN_status_t *stat, \
			const FN_composite_name_t *prefix, \
			const FN_ref_t *resolved_ref)
version		SUNW_1.1
end

function	fn_status_append_remaining_name
include		<xfn/xfn.h>
declaration	int fn_status_append_remaining_name(FN_status_t *stat, \
			const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_append_resolved_name
include		<xfn/xfn.h>
declaration	int fn_status_append_resolved_name(FN_status_t *stat, \
			const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_assign
include		<xfn/xfn.h>
declaration	FN_status_t *fn_status_assign(FN_status_t *dst, \
			const FN_status_t *src)
version		SUNW_1.1
end

function	fn_status_code
include		<xfn/xfn.h>
declaration	unsigned int fn_status_code(const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_copy
include		<xfn/xfn.h>
declaration	FN_status_t *fn_status_copy(const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_description
include		<xfn/xfn.h>
declaration	FN_string_t *fn_status_description(const FN_status_t *stat, \
			unsigned int detail, unsigned int *more_detail)
version		SUNW_1.1
end

function	fn_status_destroy
include		<xfn/xfn.h>
declaration	void fn_status_destroy(FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_diagnostic_message
include		<xfn/xfn.h>
declaration	const FN_string_t *fn_status_diagnostic_message( \
			const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_is_success
include		<xfn/xfn.h>
declaration	int fn_status_is_success(const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_link_diagnostic_message
include		<xfn/xfn.h>
declaration	const FN_string_t *fn_status_link_diagnostic_message( \
			const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_remaining_name
include		<xfn/xfn.h>
declaration	const FN_composite_name_t *fn_status_remaining_name( \
			const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_resolved_name
include		<xfn/xfn.h>
declaration	const FN_composite_name_t *fn_status_resolved_name( \
			const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_resolved_ref
include		<xfn/xfn.h>
declaration	const FN_ref_t *fn_status_resolved_ref( \
			const FN_status_t *stat)
version		SUNW_1.1
end

function	fn_status_set
include		<xfn/xfn.h>
declaration	int fn_status_set(FN_status_t *stat, unsigned int code, \
			const FN_ref_t *resolved_ref, \
			const FN_composite_name_t *resolved_name, \
			const FN_composite_name_t *remaining_name)
version		SUNW_1.1
end

function	fn_status_set_code
include		<xfn/xfn.h>
declaration	int fn_status_set_code(FN_status_t *stat, \
			unsigned int code)
version		SUNW_1.1
end

function	fn_status_set_diagnostic_message
include		<xfn/xfn.h>
declaration	int fn_status_set_diagnostic_message( \
			FN_status_t *stat, const FN_string_t *msg)
version		SUNW_1.1
end

function	fn_status_set_link_diagnostic_message
include		<xfn/xfn.h>
declaration	int fn_status_set_link_diagnostic_message(FN_status_t *stat, \
			const FN_string_t *msg)
version		SUNW_1.1
end

function	fn_status_set_remaining_name
include		<xfn/xfn.h>
declaration	int fn_status_set_remaining_name(FN_status_t *stat, \
			const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_set_resolved_name
include		<xfn/xfn.h>
declaration	int fn_status_set_resolved_name(FN_status_t *stat, \
			const FN_composite_name_t *name)
version		SUNW_1.1
end

function	fn_status_set_resolved_ref
include		<xfn/xfn.h>
declaration	int fn_status_set_resolved_ref(FN_status_t *stat, \
			const FN_ref_t *ref)
version		SUNW_1.1
end

function	fn_status_set_success
include		<xfn/xfn.h>
declaration	int fn_status_set_success(FN_status_t *stat)
version		SUNW_1.1
end

function	fn_string_assign
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_assign(FN_string_t *dst, \
			const FN_string_t *src)
version		SUNW_1.1
end

function	fn_string_charcount
include		<xfn/xfn.h>
declaration	size_t fn_string_charcount(const FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_code_set
include		<xfn/xfn.h>
declaration	unsigned long fn_string_code_set(const FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_compare
include		<xfn/xfn.h>
declaration	int fn_string_compare(const FN_string_t *str1, \
			const FN_string_t *str2, unsigned int string_case, \
			unsigned int *status)
version		SUNW_1.1
end

function	fn_string_compare_substring
include		<xfn/xfn.h>
declaration	int fn_string_compare_substring(const FN_string_t *str1, \
			int first, int last, const FN_string_t *str2, \
			unsigned int string_case, unsigned int *status)
version		SUNW_1.1
end

function	fn_string_contents
include		<xfn/xfn.h>
declaration	const void *fn_string_contents(const FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_copy
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_copy(const FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_create
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_create(void)
version		SUNW_1.1
end

function	fn_string_destroy
include		<xfn/xfn.h>
declaration	void fn_string_destroy(FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_from_contents
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_from_contents(unsigned long code_set, \
			unsigned long lang_terr, \
			size_t charcount, size_t bytecount, \
			const void *contents, unsigned int *status)
version		SUNW_1.1
end

function	fn_string_from_str
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_from_str(const unsigned char *cstr)
version		SUNW_1.1
end

function	fn_string_from_str_n
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_from_str_n(const unsigned char *cstr, \
			size_t n)
version		SUNW_1.1
end

function	fn_string_from_strings
include		<xfn/xfn.h>
version		SUNW_1.1
end

function	fn_string_from_substring
include		<xfn/xfn.h>
declaration	FN_string_t *fn_string_from_substring( \
			const FN_string_t *str, int first, int last)
version		SUNW_1.1
end

function	fn_string_is_empty
include		<xfn/xfn.h>
declaration	int fn_string_is_empty(const FN_string_t *str)
version		SUNW_1.1
end

function	fn_string_lang_terr
version		SUNW_1.1
end

function	fn_string_next_substring
include		<xfn/xfn.h>
declaration	int fn_string_next_substring(const FN_string_t *str, \
			const FN_string_t *sub, int index, \
			unsigned int string_case, unsigned int *status)
version		SUNW_1.1
end

function	fn_string_prev_substring
include		<xfn/xfn.h>
declaration	int fn_string_prev_substring(const FN_string_t *str, \
			const FN_string_t *sub, int index, \
			unsigned int string_case, unsigned int *status)
version		SUNW_1.1
end

function	prelim_fn_attr_ext_search
version		SUNW_1.1
end

function	prelim_fn_attr_search
version		SUNW_1.1
end

function	prelim_fn_ctx_equivalent_name
version		SUNW_1.1
end

function	prelim_fn_ext_searchlist_destroy
version		SUNW_1.1
end

function	prelim_fn_ext_searchlist_next
version		SUNW_1.1
end

function	prelim_fn_search_control_assign
version		SUNW_1.1
end

function	prelim_fn_search_control_copy
version		SUNW_1.1
end

function	prelim_fn_search_control_create
version		SUNW_1.1
end

function	prelim_fn_search_control_destroy
version		SUNW_1.1
end

function	prelim_fn_search_control_follow_links
version		SUNW_1.1
end

function	prelim_fn_search_control_max_names
version		SUNW_1.1
end

function	prelim_fn_search_control_return_attr_ids
version		SUNW_1.1
end

function	prelim_fn_search_control_return_ref
version		SUNW_1.1
end

function	prelim_fn_search_control_scope
version		SUNW_1.1
end

function	prelim_fn_search_filter_arguments
version		SUNW_1.1
end

function	prelim_fn_search_filter_assign
version		SUNW_1.1
end

function	prelim_fn_search_filter_copy
version		SUNW_1.1
end

function	prelim_fn_search_filter_create
version		SUNW_1.1
end

function	prelim_fn_search_filter_destroy
version		SUNW_1.1
end

function	prelim_fn_search_filter_expression
version		SUNW_1.1
end

function	prelim_fn_searchlist_destroy
version		SUNW_1.1
end

function	prelim_fn_searchlist_next
version		SUNW_1.1
end

function	_fn_ctx_handle_from_initial_with_ns
version		SUNWprivate_1.1
end

function	_fn_ctx_handle_from_initial_with_uid
version		SUNWprivate_1.1
end

function	_fn_string_from_strings_va_alloc
version		SUNWprivate_1.1
end

function	_fn_string_from_strings_va_size
version		SUNWprivate_1.1
end

function	fn_bindingset_add
version		SUNWprivate_1.1
end

function	fn_bindingset_assign
version		SUNWprivate_1.1
end

function	fn_bindingset_copy
version		SUNWprivate_1.1
end

function	fn_bindingset_count
version		SUNWprivate_1.1
end

function	fn_bindingset_create
version		SUNWprivate_1.1
end

function	fn_bindingset_destroy
version		SUNWprivate_1.1
end

function	fn_bindingset_first
version		SUNWprivate_1.1
end

function	fn_bindingset_get_ref
version		SUNWprivate_1.1
end

function	fn_bindingset_next
version		SUNWprivate_1.1
end

function	fn_bindingset_remove
version		SUNWprivate_1.1
end

function	fn_composite_name_assign_string
version		SUNWprivate_1.1
end

function	fn_nameset_add
version		SUNWprivate_1.1
end

function	fn_nameset_assign
version		SUNWprivate_1.1
end

function	fn_nameset_copy
version		SUNWprivate_1.1
end

function	fn_nameset_count
version		SUNWprivate_1.1
end

function	fn_nameset_create
version		SUNWprivate_1.1
end

function	fn_nameset_destroy
version		SUNWprivate_1.1
end

function	fn_nameset_first
version		SUNWprivate_1.1
end

function	fn_nameset_next
version		SUNWprivate_1.1
end

function	fn_nameset_remove
version		SUNWprivate_1.1
end

function	xdr_xFN_attribute
version		SUNWprivate_1.1
end

function	xdr_xFN_attrset
version		SUNWprivate_1.1
end

function	xdr_xFN_attrvalue
version		SUNWprivate_1.1
end

function	xdr_xFN_identifier
version		SUNWprivate_1.1
end

function	xdr_xFN_ref
version		SUNWprivate_1.1
end

function	xdr_xFN_ref_addr
version		SUNWprivate_1.1
end
