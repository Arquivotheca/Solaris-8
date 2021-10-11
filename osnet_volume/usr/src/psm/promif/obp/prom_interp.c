/*
 * Copyright (c) 1991-1997, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_interp.c	1.11	97/05/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_interpret(char *string, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
    uintptr_t arg4, uintptr_t arg5)
{
	switch (obp_romvec_version) {
	case OBP_V0_ROMVEC_VERSION:
		promif_preprom();
		OBP_INTERPRET(prom_strlen(string), string, arg1, arg2, arg3,
		    arg4);
		promif_postprom();
		break;

	default:
	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
		promif_preprom();
		OBP_INTERPRET(string, arg1, arg2, arg3, arg4, arg5);
		promif_postprom();
		break;
	}
}
