/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lintsup.c	1.2	98/09/30 SMI"

/*
 * Supplimental definitions for lint that help us avoid
 * options like `-x' that filter out things we want to
 * know about as well as things we don't.
 */
#include <thread.h>
#include <sys/syscall.h>

int		profile_rtld;
uintptr_t	(*	p_cg_interp)(int, caddr_t, caddr_t);
