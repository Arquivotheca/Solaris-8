/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)rpc_comdata.c	1.13	97/04/24 SMI"

#include <rpc/rpc.h>
#include <rpc/trace.h>

/*
 * This file should only contain common data (global data) that is exported
 * by public interfaces
 */
struct opaque_auth _null_auth;
fd_set svc_fdset;
pollfd_t *svc_pollfd;
void (*_svc_getreqset_proc)();
