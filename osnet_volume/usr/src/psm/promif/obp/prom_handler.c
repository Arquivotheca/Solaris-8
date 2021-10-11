/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_handler.c	1.6	96/02/22 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * XXX	Sigh.  The function types and args depend on the romvec version.
 */

int
prom_sethandler(void (*v0_func)(), void (*v2_func)())
{
	switch (obp_romvec_version)  {

	case OBP_V0_ROMVEC_VERSION:
		if (v0_func == 0)  {
			prom_printf("NOTE: can't register OBPV0 CB handler\n");
			return (-1);
		}
		OBP_CB_HANDLER = v0_func;
		return (0);

	default:
		if (v2_func == 0)  {
			prom_printf("NOTE: can't register OBPV2 CB handler\n");
			return (-1);
		}
		OBP_CB_HANDLER = v2_func;
		return (0);
	}
}
