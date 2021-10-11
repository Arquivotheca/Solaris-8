/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subr.c	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libintl.h>

#include "libcpc.h"
#include "libcpc_impl.h"

static cpc_errfn_t *__cpc_uerrfn;

/*PRINTFLIKE2*/
void
__cpc_error(const char *fn, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (__cpc_uerrfn)
		__cpc_uerrfn(fn, fmt, ap);
	else {
		(void) fprintf(stderr, "libcpc: %s: ", fn);
		(void) vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

void
cpc_seterrfn(cpc_errfn_t *errfn)
{
	__cpc_uerrfn = errfn;
}

/*
 * If and when there are multiple versions, these routines may have to
 * move to ISA-dependent files.
 */
uint_t __cpc_workver = CPC_VER_CURRENT;

uint_t
cpc_version(uint_t ver)
{
	switch (ver) {
	case CPC_VER_NONE:
		break;
	case CPC_VER_CURRENT:
		__cpc_workver = ver;
		break;
	default:
		return (CPC_VER_NONE);
	}
	return (__cpc_workver);
}
