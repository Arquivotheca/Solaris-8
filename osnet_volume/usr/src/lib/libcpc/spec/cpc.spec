#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)cpc.spec	1.1	99/08/15 SMI"
#
# lib/libpctx/spec/cpc.spec

function	cpc_version
include		<libcpc.h>
declaration	uint_t cpc_version(uint_t)
version		SUNW_1.1
end

function	cpc_getcpuver
include		<libcpc.h>
declaration	int cpc_getcpuver(void)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_getcciname
include		<libcpc.h>
declaration	const char *cpc_getcciname(int cpuver)
version		SUNW_1.1
exception	( $return == 0 )
end

function	cpc_getcpuref
include		<libcpc.h>
declaration	const char *cpc_getcpuref(int cpuver)
version		SUNW_1.1
exception	( $return == 0 )
end

function	cpc_getusage
include		<libcpc.h>
declaration	const char *cpc_getusage(int cpuver)
version		SUNW_1.1
exception	( $return == 0 )
end

function	cpc_getnpic
include		<libcpc.h>
declaration	uint_t cpc_getnpic(int cpuver)
version		SUNW_1.1
exception	( $return == 0 )
end

function	cpc_walk_names
include		<libcpc.h>
declaration	void cpc_walk_names(int cpuver, int regno, void *arg, \
			void (*action)(void *arg, \
			int regno, const char *name, uint8_t bits))
version		SUNW_1.1
end

function	cpc_seterrfn
include		<libcpc.h>
declaration	void cpc_seterrfn(cpc_errfn_t *errfn)
version		SUNW_1.1
end

function	cpc_strtoevent
include		<libcpc.h>
declaration	int cpc_strtoevent(int cpuver, const char *spec, \
			cpc_event_t *event)
version		SUNW_1.1
exception	( $return != 0 )
end

function	cpc_eventtostr
include		<libcpc.h>
declaration	char *cpc_eventtostr(cpc_event_t *event)
version		SUNW_1.1
exception	( $return == 0 )
end

function	cpc_event_accum
include		<libcpc.h>
declaration	void cpc_event_accum(cpc_event_t *accum, cpc_event_t *event)
version		SUNW_1.1
end

function	cpc_event_diff
include		<libcpc.h>
declaration	void cpc_event_diff(cpc_event_t *diff, cpc_event_t *left, \
			cpc_event_t *right)
version		SUNW_1.1
end

function	cpc_access
include		<libcpc.h>
declaration	int cpc_access(void)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_bind_event
include		<libcpc.h>
declaration	int cpc_bind_event(cpc_event_t *event, int flags)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_take_sample
include		<libcpc.h>
declaration	int cpc_take_sample(cpc_event_t *event)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_count_usr_events
include		<libcpc.h>
declaration	int cpc_count_usr_events(int enable)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_count_sys_events
include		<libcpc.h>
declaration	int cpc_count_sys_events(int enable)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_rele
include		<libcpc.h>
declaration	int cpc_rele(void)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_pctx_bind_event
include		<libpctx.h>, <libcpc.h>
declaration	int cpc_pctx_bind_event(pctx_t *pctx, id_t lwpid, \
			cpc_event_t *event, int flags)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_pctx_take_sample
include		<libpctx.h>, <libcpc.h>
declaration	int cpc_pctx_take_sample(pctx_t *pctx, id_t lwpid, \
			cpc_event_t *event)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_pctx_rele
include		<libpctx.h>, <libcpc.h>
declaration	int cpc_pctx_rele(pctx_t *pctx, id_t lwpid)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_pctx_invalidate
include		<libpctx.h>, <libcpc.h>
declaration	int cpc_pctx_invalidate(pctx_t *pctx, id_t lwpid)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_shared_open
include		<libcpc.h>
declaration	int cpc_shared_open(void)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_shared_close
include		<libcpc.h>
declaration	void cpc_shared_close(int fd)
version		SUNW_1.1
end

function	cpc_shared_bind_event
include		<libcpc.h>
declaration	int cpc_shared_bind_event(int fd, cpc_event_t *event, int flags)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_shared_take_sample
include		<libcpc.h>
declaration	int cpc_shared_take_sample(int fd, cpc_event_t *event)
version		SUNW_1.1
exception	( $return == -1 )
end

function	cpc_shared_rele
include		<libcpc.h>
declaration	int cpc_shared_rele(int fd)
version		SUNW_1.1
exception	( $return == -1 )
end
