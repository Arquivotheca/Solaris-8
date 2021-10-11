/*
 * Copyright (c) 1998 by Sun Microsystems Inc.
 */

#ident	"@(#)rpc_comdata1.c	1.1	98/02/09 SMI"

/*
 * FD_SETSIZE definition must precede any save isa_defs.h since that
 * is where _LP64 is defined....
 */
#include <sys/isa_defs.h>
#if !defined(_LP64)
#ifdef FD_SETSIZE
#undef FD_SETSIZE
#endif
#define FD_SETSIZE 65536

#include <sys/select.h>

/*
 * This file should only contain common data (global data) that is exported
 * by public interfaces
 */

/*
 * Definition of alternate fd_set for svc_fdset to be used when
 * someone redefine SVC_FDSETSIZE. This is here solely to
 * protect against someone doing a svc_fdset = a_larger_fd_set.
 * If we're not a 64 bit app and someone defines fd_setsize > 1024
 * then svc_fdset is redefined to be _new_svc_fdset (in <rpc/svc.h>)
 * which we size here at the maximum size.
 */

fd_set _new_svc_fdset;
#else
extern svc_fdset;	/* to avoid "empty translation unit" */
#endif
