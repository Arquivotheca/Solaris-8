/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_bad.c	1.1	99/11/03 SMI"


/*
 * bad BIOS identifiers
 */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>
#include "acpi_bad.h"

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif


acpi_header_t acpi_bad_bios_list[] = {

	/* global "good" ACPI BIOS date starts at 1 Jan 1999 */
	ACPI_BAD_DATE(ACPI_BDESC_END, ACPI_BDATE_LT, 1999, 1, 1),


	/* Intel "Sitka" SC450NX */
	/* 4280299 bad _PRT for PCI bus 1 before PR 11, 4 Aug 1999 */
	ACPI_BAD_TABLE(0, ACPI_BSREV_EQ | ACPI_BOEM_EQ | ACPI_BTAB_EQ,
	    ACPI_RSDT, 1, "INTEL ", "S450NX01", NO_VAL, NO_VAL, NO_VAL),
	ACPI_BAD_DATE(ACPI_BDESC_END, ACPI_BDATE_LT, 1999, 8, 4),

	/* Intel 440GX+ motherboard */
	/* 4253680 bad _PRT before PR 6, 20 Apr 1999 */
	ACPI_BAD_TABLE(0, ACPI_BSREV_EQ | ACPI_BOEM_EQ | ACPI_BTAB_EQ,
	    ACPI_FACP, 1, "Intel ", "L440GX  ", NO_VAL, NO_VAL, NO_VAL),
	ACPI_BAD_DATE(ACPI_BDESC_END, ACPI_BDATE_LT, 1999, 4, 20),


	/* end of bad BIOS list - should always be last */
	ACPI_BAD_BIOS_LIST_END,
};


/* eof */
