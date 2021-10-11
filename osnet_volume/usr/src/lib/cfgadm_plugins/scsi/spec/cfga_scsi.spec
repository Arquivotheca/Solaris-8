#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)cfga_scsi.spec	1.1	99/03/19 SMI"
#
# lib/cfgadm_plugins/scsi/spec/cfga_scsi.spec


function	cfga_ap_id_cmp
include		<sys/param.h>, <config_admin.h>
declaration	int cfga_ap_id_cmp(const cfga_ap_log_id_t, \
			const cfga_ap_log_id_t)
version		SUNWprivate_1.1
end

function	cfga_change_state
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t cfga_change_state(cfga_cmd_t, const char *, \
			const char *, struct cfga_confirm *, \
			struct cfga_msg *, char **, cfga_flags_t)
version		SUNWprivate_1.1
end

function	cfga_help
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t cfga_help(struct cfga_msg *, const char *, \
			cfga_flags_t)
version		SUNWprivate_1.1
end

function	cfga_list_ext
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t cfga_list_ext(const char *, \
			struct cfga_list_data **, int *, const char *, \
			const char *, char **, cfga_flags_t)
version		SUNWprivate_1.1
end

function	cfga_private_func
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t cfga_private_func(const char *, const char *, \
			const char *, struct cfga_confirm *, \
			struct cfga_msg *, char **, cfga_flags_t)
version		SUNWprivate_1.1
end

function	cfga_test
include		<sys/param.h>, <config_admin.h>
declaration	cfga_err_t cfga_test(const char *, const char *, \
			struct cfga_msg *, char **, cfga_flags_t)
version		SUNWprivate_1.1
end

data		cfga_version
declaration	int cfga_version
version		SUNWprivate_1.1
end
