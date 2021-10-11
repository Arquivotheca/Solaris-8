#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)pci.spec	1.4	99/12/02 SMI"
#
# lib/cfgadm_plugins/pci/pci.spec

function	cfga_change_state
include		<sys/types.h>
include		<sys/param.h>
include		<config_admin.h>
declaration	cfga_err_t cfga_change_state(cfga_cmd_t state_change_cmd, \
			const char *ap_id, char *options, \
			struct cfga_confirm *confp, struct cfga_msg *msgp, \
			char **errstring, cfga_flags_t flags)
version		SUNWprivate_1.1
end

function	cfga_private_func
include		<sys/types.h>
include		<sys/param.h>
include		<config_admin.h>
declaration	cfga_err_t cfga_private_func(const char *function, \
			const char *ap_id, const char *options, \
			struct cfga_confirm *confp, struct cfga_msg *msgp, \
			char **errstring, cfga_flags_t flags)
version		SUNWprivate_1.1
end

function	cfga_test
include		<sys/types.h>
include		<sys/param.h>
include		<config_admin.h>
declaration	cfga_err_t cfga_test(int num_ap_ids, char *const *ap_ids, \
			const char *options, struct cfga_msg *msgp, \
			char **errstring, cfga_flags_t flags)
version		SUNWprivate_1.1
end

function	cfga_list_ext
include			<sys/types.h>
include			<sys/param.h>
include			<config_admin.h>
declaration	cfga_err_t cfga_list_ext(const char *ap_id, \
			cfga_list_data_t **cs, int *nlist, \
			const char *options, const char *listopts, \
			char **errstring, cfga_flags_t flags)
version			SUNWprivate_1.1
end


function	cfga_help
include		<sys/types.h>
include		<sys/param.h>
include		<config_admin.h>
declaration	cfga_err_t cfga_help(struct cfga_msg *msgp, \
			const char *options, cfga_flags_t flags)
version		SUNWprivate_1.1
end

function	cfga_ap_id_cmp
include		<sys/types.h>
include		<sys/param.h>
include		<config_admin.h>
declaration	int cfga_ap_id_cmp(const cfga_ap_log_id_t ap_id1, \
			const cfga_ap_log_id_t ap_id2)
version		SUNWprivate_1.1
end

data		cfga_version
declaration	int cfga_version
version		SUNWprivate_1.1
end
