#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)pctx.spec	1.1	99/08/15 SMI"
#
# lib/libpctx/spec/pctx.spec

function	pctx_create
include		<libpctx.h>
declaration	pctx_t *pctx_create(const char *filename, char *const *argv, \
			void *arg, int verbose, pctx_errfn_t *errfn)
version		SUNW_1.1
exception	( $return == 0 )
end

function	pctx_capture
include		<libpctx.h>
declaration	pctx_t *pctx_capture(pid_t pid, \
			void *arg, int verbose, pctx_errfn_t *errfn)
version		SUNW_1.1
exception	( $return == 0 )
end

function	pctx_set_events
include		<libpctx.h>
declaration	int pctx_set_events(pctx_t *, ...)
version		SUNW_1.1
exception	( $return == -1 )
end

function	pctx_run
include		<libpctx.h>
declaration	int pctx_run(pctx_t *pctx, uint_t msec, uint_t nsamples, \
			int (*tick)(pctx_t *, pid_t, id_t, void *))
version		SUNW_1.1
exception	( $return != 0 )
end

function	pctx_release
include		<libpctx.h>
declaration	void pctx_release(pctx_t *pctx)
version		SUNW_1.1
end

function	__pctx_cpc
include		<libpctx.h>
declaration	int __pctx_cpc(pctx_t *pctx, \
			int cmd, id_t lwpid, void *data, int flags, size_t size)
version		SUNWprivate_1.1
exception	( $return == -1 )
end
