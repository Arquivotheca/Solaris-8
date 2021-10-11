/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)symbols_sparc.c	1.5	98/09/19 SMI"

#include <stdio.h>
#include "_conv.h"
#include "symbols_sparc_msg.h"
#include <sys/elf_SPARC.h>

/*
 * SPARC specific register symbols
 */

const char *
conv_sym_SPARC_value_str(Lword val)
{
	static char	string[STRSIZE64] = { '\0' };

	if (val == STO_SPARC_REGISTER_G2)
		return (MSG_ORIG(MSG_STO_REGISTERG2));
	else if (val == STO_SPARC_REGISTER_G3)
		return (MSG_ORIG(MSG_STO_REGISTERG3));
	else
		return (conv_invalid_str(string, val, 0));
}
