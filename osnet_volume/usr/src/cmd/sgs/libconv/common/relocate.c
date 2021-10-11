/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)relocate.c	1.8	99/05/04 SMI"

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	"_conv.h"

/*
 * Generic front-end that determines machine specific relocations.
 */
const char *
conv_reloc_type_str(Half mach, uint_t rel)
{
	static char	string[STRSIZE] = { '\0' };

	if (mach == EM_386)
		return (conv_reloc_386_type_str(rel));

	if ((mach == EM_SPARC) || (mach == EM_SPARC32PLUS) ||
	    (mach == EM_SPARCV9))
		return (conv_reloc_SPARC_type_str(rel));

	if (mach == EM_IA_64)
		return (conv_reloc_ia64_type_str(rel));

	return (conv_invalid_str(string, (Lword)rel, 0));
}
