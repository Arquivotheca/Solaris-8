#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)gss.spec	1.1	99/01/25 SMI"
#
# lib/libgss/spec/gss.spec

function	GSS_C_NT_USER_NAME
version		SUNWprivate_1.1
end		

function	GSS_C_NT_MACHINE_UID_NAME
version		SUNWprivate_1.1
end		

function	GSS_C_NT_STRING_UID_NAME
version		SUNWprivate_1.1
end		

function	GSS_C_NT_HOSTBASED_SERVICE
version		SUNWprivate_1.1
end		

function	GSS_C_NT_ANONYMOUS
version		SUNWprivate_1.1
end		

function	GSS_C_NT_EXPORT_NAME
version		SUNWprivate_1.1
end		

function	gss_acquire_cred
version		SUNWprivate_1.1
end		

function	gss_release_cred
version		SUNWprivate_1.1
end		

function	gss_init_sec_context
version		SUNWprivate_1.1
end		

function	gss_accept_sec_context
version		SUNWprivate_1.1
end		

function	gss_process_context_token
version		SUNWprivate_1.1
end		

function	gss_delete_sec_context
version		SUNWprivate_1.1
end		

function	gss_context_time
version		SUNWprivate_1.1
end		

function	gss_display_status
version		SUNWprivate_1.1
end		

function	gss_indicate_mechs
version		SUNWprivate_1.1
end		

function	gss_compare_name
version		SUNWprivate_1.1
end		

function	gss_display_name
version		SUNWprivate_1.1
end		

function	gss_import_name
version		SUNWprivate_1.1
end		

function	gss_release_name
version		SUNWprivate_1.1
end		

function	gss_release_buffer
version		SUNWprivate_1.1
end		

function	gss_release_oid_set
version		SUNWprivate_1.1
end		

function	gss_inquire_cred
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_inquire_context
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_get_mic
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_verify_mic
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_wrap
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_unwrap
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_wrap_size_limit
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_export_name
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_add_cred
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_inquire_cred_by_mech
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_export_sec_context
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_import_sec_context
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_release_oid
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_create_empty_oid_set
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_add_oid_set_member
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_test_oid_set_member
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_str_to_oid
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_oid_to_str
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_inquire_names_for_mech
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_canonicalize_name
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_duplicate_name
version		SUNWprivate_1.1
end		

# GSSAPI V2
function	gss_copy_oid_set
version		SUNWprivate_1.1
end		

# GSSAPI V1
function	gss_sign
version		SUNWprivate_1.1
end		

# GSSAPI V1
function	gss_verify
version		SUNWprivate_1.1
end		

# GSSAPI V1
function	gss_seal
version		SUNWprivate_1.1
end		

# GSSAPI V1
function	gss_unseal
version		SUNWprivate_1.1
end		

function	gss_nt_service_name
version		SUNWprivate_1.1
end		

function	__gss_qop_to_num
version		SUNWprivate_1.1
end		

function	__gss_num_to_qop
version		SUNWprivate_1.1
end		

function	__gss_get_mech_info
version		SUNWprivate_1.1
end		

function	__gss_mech_qops
version		SUNWprivate_1.1
end		

function	__gss_mech_to_oid
version		SUNWprivate_1.1
end		

function	__gss_oid_to_mech
version		SUNWprivate_1.1
end		

function	__gss_get_mechanisms
version		SUNWprivate_1.1
end		

function	gsscred_expname_to_unix_cred
version		SUNWprivate_1.1
end		

function	gsscred_name_to_unix_cred
version		SUNWprivate_1.1
end		

function	gss_get_group_info
version		SUNWprivate_1.1
end		

function	__gss_get_kmodName	
version		SUNWprivate_1.1
end		

# Needed by mech_dummy.so to run rpcgss_sample with -m 2
function	generic_gss_copy_oid
version		SUNWprivate_1.1
end		

# Needed by mech_dummy.so to run rpcgss_sample with -m 2
function	generic_gss_release_oid
version		SUNWprivate_1.1
end		

function	__gss_get_mech_type
version		SUNWprivate_1.1
end		

function	der_length_size
version		SUNWprivate_1.1
end		

function	get_der_length
version		SUNWprivate_1.1
end		

function	put_der_length
version		SUNWprivate_1.1
end		

