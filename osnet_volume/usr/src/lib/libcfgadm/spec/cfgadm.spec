#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)cfgadm.spec	1.2	99/03/19 SMI"
#
# lib/libcfgadm/spec/cfgadm.spec

function	config_ap_id_cmp
include		<sys/param.h>, <config_admin.h>
version		SUNW_1.1
end		

function	config_change_state
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_change_state(cfga_cmd_t state_change_cmd, \
			int num_ap_ids, char * const *ap_ids, \
			const char *options, struct cfga_confirm *confp, \
			struct cfga_msg *msgp, char **errstring, \
			cfga_flags_t flags)
version		SUNW_1.1
end		

function	config_help
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_help( int num_ap_ids, \
			char * const *ap_ids, struct cfga_msg *msgp, \
			const char *options, cfga_flags_t flags)
version		SUNW_1.1
end		

function	config_list
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_list( struct cfga_stat_data **ap_di_list, \
			int *nlist, const char *options, char **errstring)
version		SUNW_1.1
end		

function	config_list_ext
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_list_ext( int num_ap_ids, \
			char * const *ap_ids, \
			struct cfga_list_data **ap_id_list, int *nlistp, \
			const char *options, const char *listopts, \
			char **errstring, cfga_flags_t flags)
version		SUNW_1.2
end

function	config_private_func
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_private_func( const char *function, \
			int num_ap_ids, char * const *ap_ids, \
			const char *options, struct cfga_confirm *confp, \
			struct cfga_msg *msgp, char **errstring, \
			cfga_flags_t flags)
version		SUNW_1.1
end		

function	config_stat
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_stat( int num_ap_ids, \
			char * const *ap_ids, struct cfga_stat_data *buf, \
			const char *options, char **errstring)
version		SUNW_1.1
end		

function	config_strerror
include		<sys/param.h>, <config_admin.h>
declaration	const char * config_strerror( int cfgerrnum)
version		SUNW_1.1
end		

function	config_test
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t config_test( int num_ap_ids, char * const *ap_ids, \
			const char *options, struct cfga_msg *msgp, \
			char **errstring, cfga_flags_t flags)
version		SUNW_1.1
end		

function	config_unload_libs
include		<sys/param.h>, <config_admin.h>
declaration	void config_unload_libs(void)
version		SUNW_1.1
end		


