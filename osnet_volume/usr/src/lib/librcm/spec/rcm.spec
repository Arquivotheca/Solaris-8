#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rcm.spec	1.1	99/08/10 SMI"
#
# lib/librcm/spec/rcm.spec

#
# Consolidation private PSARC 1998/460
#

function	rcm_alloc_handle
include		<librcm.h>
declaration	int rcm_alloc_handle(char *, uint_t, void *, rcm_handle_t **)
version		SUNWprivate_1.1
end

function	rcm_free_handle
include		<librcm.h>
declaration	int rcm_free_handle(rcm_handle_t *)
version		SUNWprivate_1.1
end

function	rcm_get_info
include		<librcm.h>
include		<librcm_impl.h>
declaration	int rcm_get_info(rcm_handle_t *, char *, uint_t, rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_free_info
include		<librcm.h>
declaration	void rcm_free_info(rcm_info_t *)
version		SUNWprivate_1.1
end

function	rcm_append_info
include		<librcm.h>
declaration	int rcm_append_info(rcm_info_t **, rcm_info_t *)
version		SUNWprivate_1.1
end

function	rcm_info_next
include		<librcm.h>
declaration	rcm_info_tuple_t *rcm_info_next(rcm_info_t *, \
		rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_rsrc
include		<librcm.h>
declaration	const char *rcm_info_rsrc(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_info
include		<librcm.h>
declaration	const char *rcm_info_info(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_modname
include		<librcm.h>
declaration	const char *rcm_info_modname(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_pid
include		<librcm.h>
declaration	pid_t rcm_info_pid(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_state
include		<librcm.h>
declaration	int rcm_info_state(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_info_seqnum
include		<librcm.h>
declaration	int rcm_info_seqnum(rcm_info_tuple_t *)
version		SUNWprivate_1.1
end

function	rcm_request_offline
include		<librcm.h>
declaration	int rcm_request_offline(rcm_handle_t *, char *, uint_t, \
		rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_notify_online
include		<librcm.h>
declaration	int rcm_notify_online(rcm_handle_t *, char *, uint_t, \
		rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_notify_remove
include		<librcm.h>
declaration	int rcm_notify_remove(rcm_handle_t *, char *, uint_t, \
		rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_request_suspend
include		<librcm.h>
declaration	int rcm_request_suspend(rcm_handle_t *, char *, uint_t, \
		timespec_t *, rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_notify_resume
include		<librcm.h>
declaration	int rcm_notify_resume(rcm_handle_t *, char *, uint_t, \
		rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_register_interest
include		<librcm.h>
declaration	int rcm_register_interest(rcm_handle_t *, char *, uint_t, \
		rcm_info_t **)
version		SUNWprivate_1.1
end

function	rcm_unregister_interest
include		<librcm.h>
declaration	int rcm_unregister_interest(rcm_handle_t *, char *, uint_t)
version		SUNWprivate_1.1
end

function	rcm_exec_cmd
include		<librcm.h>
declaration	int rcm_exec_cmd(char *)
version		SUNWprivate_1.1
end

#
# Project private interfaces
#
function	rcm_module_dir
include		<librcm_impl.h>
declaration	char *rcm_module_dir(uint_t)
version		SUNWprivate_1.1
end

function	rcm_module_open
include		<librcm_impl.h>
declaration	void *rcm_module_open(char *)
version		SUNWprivate_1.1
end

function	rcm_module_close
include		<librcm_impl.h>
declaration	void rcm_module_close(void *)
version		SUNWprivate_1.1
end

function	rcm_log_message
include		<librcm_impl.h>
declaration	void rcm_log_message(int, char *, ...)
version		SUNWprivate_1.1
end
