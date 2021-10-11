#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rtld_db.spec	1.2	99/02/18 SMI"
#
# cmd/sgs/librtld_db/spec/rtld_db.spec

function	rd_delete
include		<rtld_db.h>
declaration	void rd_delete(rd_agent_t *)
version		SUNW_1.1
end		

function	rd_errstr
include		<rtld_db.h>
declaration	char * rd_errstr(rd_err_e)
exception	$return == NULL
version		SUNW_1.1
end		

function	rd_event_addr
include		<rtld_db.h>
declaration	rd_err_e rd_event_addr(rd_agent_t *, rd_event_e, rd_notify_t *)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_event_enable
include		<rtld_db.h>
declaration	rd_err_e rd_event_enable(rd_agent_t *, int)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_event_getmsg
include		<rtld_db.h>
declaration	rd_err_e rd_event_getmsg(rd_agent_t *, rd_event_msg_t *)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_init
include		<rtld_db.h>
declaration	rd_err_e rd_init(int)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_loadobj_iter
include		<rtld_db.h>
declaration	rd_err_e rd_loadobj_iter(rd_agent_t *, rl_iter_f *, void *)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_log
include		<rtld_db.h>
declaration	void rd_log(const int)
version		SUNW_1.1
end		

function	rd_new
include		<rtld_db.h>
declaration	rd_agent_t * rd_new(struct ps_prochandle *)
exception	$return == NULL
version		SUNW_1.1
end		

function	rd_objpad_enable
include		<rtld_db.h>
declaration	rd_err_e rd_objpad_enable(rd_agent_t *, size_t)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_plt_resolution
include		<rtld_db.h>
declaration	rd_err_e rd_plt_resolution(rd_agent_t *, psaddr_t, \
			lwpid_t, psaddr_t, rd_plt_info_t *)
exception	$return != RD_OK
version		SUNW_1.1
end		

function	rd_reset
include		<rtld_db.h>
declaration	rd_err_e rd_reset(struct rd_agent *)
exception	$return != RD_OK
version		SUNW_1.1
end		
