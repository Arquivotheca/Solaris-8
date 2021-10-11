#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)tnf.spec	1.1	99/01/25 SMI"
#
# lib/libtnf/spec/tnf.spec

function	tnf_default_error_handler
include		<libtnf.h>
declaration	void tnf_default_error_handler(void *arg, TNF *tnf, \
			tnf_errcode_t err)
version		SUNWprivate_1.1
end		

function	tnf_error_message
include		<libtnf.h>
declaration	char * tnf_error_message(tnf_errcode_t err)
version		SUNWprivate_1.1
end		

function	tnf_get_block_absolute
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_block_absolute(TNF *tnf, unsigned index)
version		SUNWprivate_1.1
end		

function	tnf_get_block_count
include		<libtnf.h>
declaration	unsigned tnf_get_block_count(TNF *tnf)
version		SUNWprivate_1.1
end		

function	tnf_get_block_header
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_block_header(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_block_relative
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_block_relative(tnf_datum_t datum, \
			int adjust)
version		SUNWprivate_1.1
end		

function	tnf_get_char
include		<libtnf.h>
declaration	char tnf_get_char(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_chars
include		<libtnf.h>
declaration	char * tnf_get_chars(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_element
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_element(tnf_datum_t datum, unsigned)
version		SUNWprivate_1.1
end		

function	tnf_get_element_count
include		<libtnf.h>
declaration	unsigned tnf_get_element_count(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_element_type
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_element_type(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_elements
include		<libtnf.h>
declaration	caddr_t tnf_get_elements(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_file_header
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_file_header(TNF *tnf)
version		SUNWprivate_1.1
end		

function	tnf_get_float32
include		<libtnf.h>
declaration	tnf_float32_t tnf_get_float32(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_float64
include		<libtnf.h>
declaration	tnf_float64_t tnf_get_float64(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_int16
include		<libtnf.h>
declaration	tnf_int16_t tnf_get_int16(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_int32
include		<libtnf.h>
declaration	tnf_int32_t tnf_get_int32(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_int64
include		<libtnf.h>
declaration	tnf_int64_t tnf_get_int64(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_int8
include		<libtnf.h>
declaration	tnf_int8_t tnf_get_int8(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_kind
include		<libtnf.h>
declaration	tnf_kind_t tnf_get_kind(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_next_record
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_next_record(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_raw
include		<libtnf.h>
declaration	caddr_t tnf_get_raw(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_size
include		<libtnf.h>
declaration	size_t tnf_get_size(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_slot_count
include		<libtnf.h>
declaration	unsigned tnf_get_slot_count(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_slot_index
include		<libtnf.h>
declaration	unsigned tnf_get_slot_index(tnf_datum_t datum, char *name)
version		SUNWprivate_1.1
end		

function	tnf_get_slot_indexed
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_slot_indexed(tnf_datum_t datum, \
			unsigned index)
version		SUNWprivate_1.1
end		

function	tnf_get_slot_name
include		<libtnf.h>
declaration	char * tnf_get_slot_name(tnf_datum_t datum, unsigned index)
version		SUNWprivate_1.1
end		

function	tnf_get_slot_named
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_slot_named(tnf_datum_t datum, char *name)
version		SUNWprivate_1.1
end		

function	tnf_get_tag_arg
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_tag_arg(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_type
include		<libtnf.h>
declaration	tnf_datum_t tnf_get_type(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_get_type_name
include		<libtnf.h>
declaration	char * tnf_get_type_name(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_array
include		<libtnf.h>
declaration	int tnf_is_array(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_block_header
include		<libtnf.h>
declaration	int tnf_is_block_header(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_inline
include		<libtnf.h>
declaration	int tnf_is_inline(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_record
include		<libtnf.h>
declaration	int tnf_is_record(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_scalar
include		<libtnf.h>
declaration	int tnf_is_scalar(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_string
include		<libtnf.h>
declaration	int tnf_is_string(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_struct
include		<libtnf.h>
declaration	int tnf_is_struct(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_is_type
include		<libtnf.h>
declaration	int tnf_is_type(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_reader_begin
include		<libtnf.h>
declaration	tnf_errcode_t tnf_reader_begin(caddr_t base, size_t size, \
			TNF **tnfret)
version		SUNWprivate_1.1
end		

function	tnf_reader_end
include		<libtnf.h>
declaration	tnf_errcode_t tnf_reader_end(TNF *tnf)
version		SUNWprivate_1.1
end		

function	tnf_set_error_handler
include		<libtnf.h>
#declaration	void tnf_set_error_handler(tnf_error_handler_t *handler, void *arg)
# Using alternate binary equivalent for delaration
declaration	void tnf_set_error_handler(void (*handler)(void *, TNF *, tnf_errcode_t),  void *arg)
version		SUNWprivate_1.1
end		

function	tnf_type_get_base
include		<libtnf.h>
declaration	tnf_datum_t tnf_type_get_base(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_type_get_kind
include		<libtnf.h>
declaration	tnf_kind_t tnf_type_get_kind(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_type_get_name
include		<libtnf.h>
declaration	char * tnf_type_get_name(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

function	tnf_type_get_property
include		<libtnf.h>
declaration	tnf_datum_t tnf_type_get_property(tnf_datum_t datum, char *name)
version		SUNWprivate_1.1
end		

function	tnf_type_get_size
include		<libtnf.h>
declaration	size_t tnf_type_get_size(tnf_datum_t datum)
version		SUNWprivate_1.1
end		

