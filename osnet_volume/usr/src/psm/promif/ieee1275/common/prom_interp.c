/*
 * Copyright (c) 1991-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_interp.c	1.13	97/05/30 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_interpret(char *string, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
    uintptr_t arg4, uintptr_t arg5)
{
	cell_t ci[9];

	/*
	 * NB: It is not possible to provide a compatible interface
	 * here, if the size of an int is different from the size of a
	 * cell, since we don't know how to promote the arguments.  We
	 * simply promote arguments treating them as unsigned integers;
	 * thus pointers will be properly promoted and negative signed
	 * integer value will not be properly promoted.  This isn't an
	 * issue for LP64 client programs since this code will simply
	 * pass the entire 64-bit arguments through unchanged.
	 *
	 * XXX: This is not fully capable via this interface.  Use
	 * p1275_cif_handler directly for all features.  Specifically,
	 * there's no catch_result and no result cells available via this
	 * interface.  This interface provided for compatibilty with
	 * existing OBP code.  Note that we also assume that the
	 * arguments are not to be sign extended.  Assuming the code
	 * using these arguments is written correctly, this should be OK.
	 */

	ci[0] = p1275_ptr2cell("interpret");	/* Service name */
	ci[1] = (cell_t)6;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */
	ci[3] = p1275_ptr2cell(string);		/* Arg1: Interpreted string */
	ci[4] = p1275_uintptr2cell(arg1);	/* Arg2: stack arg 1 */
	ci[5] = p1275_uintptr2cell(arg2);	/* Arg3: stack arg 2 */
	ci[6] = p1275_uintptr2cell(arg3);	/* Arg4: stack arg 3 */
	ci[7] = p1275_uintptr2cell(arg4);	/* Arg5: stack arg 4 */
	ci[8] = p1275_uintptr2cell(arg5);	/* Arg6: stack arg 5 */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}
