#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)private.spec	1.1	99/01/25 SMI"
#
# lib/libtnfprobe/spec/private.spec

function	__tnf_probe_alloc
include		<tnf_trace.h>
declaration	char *__tnf_probe_alloc(size_t size)
version		SUNWprivate_1.1
end		

data		__tnf_probe_list_head
version		SUNWprivate_1.1
end		

data		__tnf_probe_list_valid
version		SUNWprivate_1.1
end		

data		__tnf_probe_memseg_p
version		SUNWprivate_1.1
end		

function	__tnf_probe_notify
include		<tnf_trace.h>
declaration	void __tnf_probe_notify(void)
version		SUNWprivate_1.1
end		

data		__tnf_probe_thr_sync
version		SUNWprivate_1.1
end		

function	__tnf_probe_version_1_info
version		SUNWprivate_1.1
end		

function	__tnf_tag_version_1_info
version		SUNWprivate_1.1
end		

function	_resume_ret
include		<tnf_trace.h>
declaration	void _resume_ret(void *arg1)
version		SUNWprivate_1.1
end		

data		_tnfw_b_control
version		SUNWprivate_1.1
end		

function	fork1 extends libc/spec/sys.spec
version		SUNWprivate_1.1
end		

function	fork extends libc/spec/sys.spec
version		SUNWprivate_1.1
end		

function	pthread_create extends libthread/spec/pthread.spec
version		SUNWprivate_1.1
end		

function	pthread_exit extends libthread/spec/pthread.spec
include		<tnf_trace.h>
declaration	void pthread_exit(void * status)
version		SUNWprivate_1.1
end		

function	thr_create extends libc/spec/stubs.spec
version		SUNWprivate_1.1
end		

function	thr_exit extends libc/spec/stubs.spec
version		SUNWprivate_1.1
end		

function	tnf_allocate
include		<tnf_trace.h>
declaration	void *tnf_allocate(tnf_ops_t *ops, size_t size)
version		SUNWprivate_1.1
end		

function	tnf_char_tag_data
version		SUNWprivate_1.1
end		

function	tnf_float32_tag_data
version		SUNWprivate_1.1
end		

function	tnf_float64_tag_data
version		SUNWprivate_1.1
end		

function	tnf_int16_tag_data
version		SUNWprivate_1.1
end		

function	tnf_int32_tag_data
version		SUNWprivate_1.1
end		

function	tnf_int64_tag_data
version		SUNWprivate_1.1
end		

function	tnf_int8_tag_data
version		SUNWprivate_1.1
end		

function	tnf_lwpid_tag_data
version		SUNWprivate_1.1
end		

function	tnf_name_tag_data
version		SUNWprivate_1.1
end		

data		tnf_non_threaded_test_addr
version		SUNWprivate_1.1
end		

function	tnf_opaque_tag_data
version		SUNWprivate_1.1
end		

function	tnf_pid_tag_data
version		SUNWprivate_1.1
end		

function	tnf_probe_debug
include		<tnf_trace.h>
declaration	void tnf_probe_debug(tnf_probe_setup_t *set_p)
version		SUNWprivate_1.1
end		

function	tnf_probe_event_tag_data
version		SUNWprivate_1.1
end		

function	tnf_probe_get_arg_indexed
include		<tnf_trace.h>
declaration	void * tnf_probe_get_arg_indexed \
			(tnf_probe_control_t *probe_p, int index, void *buffer)
version		SUNWprivate_1.1
end		

function	tnf_probe_get_chars
include		<tnf_trace.h>
declaration	char * tnf_probe_get_chars(void *slot)
version		SUNWprivate_1.1
end		

function	tnf_probe_get_num_args
include		<tnf_trace.h>
declaration	int tnf_probe_get_num_args(tnf_probe_control_t *probe_p)
version		SUNWprivate_1.1
end		

function	tnf_probe_get_type_indexed
include		<tnf_trace.h>
declaration	tnf_arg_kind_t tnf_probe_get_type_indexed \
			(tnf_probe_control_t *probe_p, int index)
version		SUNWprivate_1.1
end		

function	tnf_probe_get_value
include		<tnf_trace.h>
declaration	const char * tnf_probe_get_value \
			(tnf_probe_control_t *probe_p, char *attribute, \
			ulong_t *size)
version		SUNWprivate_1.1
end		

function	tnf_process_disable
include		<tnf_trace.h>
declaration	void tnf_process_disable(void)
version		SUNWprivate_1.1
end		

function	tnf_process_enable
include		<tnf_trace.h>
declaration	void tnf_process_enable(void)
version		SUNWprivate_1.1
end		

function	tnf_ref32_1
version		SUNWprivate_1.1
end		

function	tnf_size_tag_data
version		SUNWprivate_1.1
end		

function	tnf_string_1
version		SUNWprivate_1.1
end		

function	tnf_string_tag_data
version		SUNWprivate_1.1
end		

function	tnf_struct_tag_1
version		SUNWprivate_1.1
end		

function	tnf_tag_tag_data
version		SUNWprivate_1.1
end		

function	tnf_thread_disable
include		<tnf_trace.h>
declaration	void tnf_thread_disable(void)
version		SUNWprivate_1.1
end		

function	tnf_thread_enable
include		<tnf_trace.h>
declaration	void tnf_thread_enable(void)
version		SUNWprivate_1.1
end		

data		tnf_threaded_test_addr
version		SUNWprivate_1.1
end		

function	tnf_time_base_tag_data
version		SUNWprivate_1.1
end		

function	tnf_time_delta_tag_data
version		SUNWprivate_1.1
end		

function	tnf_trace_alloc
include		<tnf_trace.h>
declaration	void * tnf_trace_alloc(tnf_ops_t *ops, \
			tnf_probe_control_t *probe_p, tnf_probe_setup_t *set_p)
version		SUNWprivate_1.1
end		

function	tnf_trace_commit
include		<tnf_trace.h>
declaration	void tnf_trace_commit(tnf_probe_setup_t *set_p)
version		SUNWprivate_1.1
end		

function	tnf_trace_end
include		<tnf_trace.h>
declaration	void tnf_trace_end(tnf_probe_setup_t *set_p)
version		SUNWprivate_1.1
end		

data		tnf_trace_file_min
version		SUNWprivate_1.1
end		

data		tnf_trace_file_name
version		SUNWprivate_1.1
end		

data		tnf_trace_file_size
version		SUNWprivate_1.1
end		

function	tnf_trace_rollback
include		<tnf_trace.h>
declaration	void tnf_trace_rollback(tnf_probe_setup_t *set_p)
version		SUNWprivate_1.1
end		

function	tnf_uint16_tag_data
version		SUNWprivate_1.1
end		

function	tnf_uint32_tag_data
version		SUNWprivate_1.1
end		

function	tnf_uint64_tag_data
version		SUNWprivate_1.1
end		

function	tnf_uint8_tag_data
version		SUNWprivate_1.1
end		

function	tnf_user_struct_properties
version		SUNWprivate_1.1
end		

#
# Weak interfaces
#

function	_tnf_resume_ret
weak		_resume_ret
end		

function	_tnf_fork
weak		fork
end		

function	_tnf_fork1
weak		fork1
end		

