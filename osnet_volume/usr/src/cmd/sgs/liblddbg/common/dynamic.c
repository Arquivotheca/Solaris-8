/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dynamic.c	1.21	99/09/14 SMI"

#include	<link.h>
#include	<stdio.h>
#include	"msg.h"
#include	"_debug.h"

/*
 * Print out the dynamic section entries.
 */
void
Gelf_dyn_title()
{
	dbg_print(MSG_INTL(MSG_DYN_TITLE));
}

void
Gelf_dyn_print(GElf_Dyn * dyn, int ndx, const char * names, Half mach)
{
	const char *	name;
	char		index[10];

	/*
	 * Print the information numerically, and if possible as a string.
	 */
	if (names && ((dyn->d_tag == DT_NEEDED) ||
	    (dyn->d_tag == DT_SONAME) ||
	    (dyn->d_tag == DT_FILTER) ||
	    (dyn->d_tag == DT_AUXILIARY) ||
	    (dyn->d_tag == DT_CONFIG) ||
	    (dyn->d_tag == DT_RPATH) ||
	    (dyn->d_tag == DT_USED) ||
	    (dyn->d_tag == DT_DEPAUDIT) ||
	    (dyn->d_tag == DT_AUDIT)))
		name = names + dyn->d_un.d_ptr;
	else if (dyn->d_tag == DT_FLAGS_1)
		/* LINTED */
		name = conv_dynflag_1_str((Word)dyn->d_un.d_val);
	else if (dyn->d_tag == DT_POSFLAG_1)
		/* LINTED */
		name = conv_dynposflag_1_str((Word)dyn->d_un.d_val);
	else if (dyn->d_tag == DT_FEATURE_1)
		/* LINTED */
		name = conv_dynfeature_1_str((Word)dyn->d_un.d_val);
	else if (dyn->d_tag == DT_DEPRECATED_SPARC_REGISTER)
		name = MSG_INTL(MSG_STR_DEPRECATED);
	else
		name = MSG_ORIG(MSG_STR_EMPTY);

	(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), ndx);
	dbg_print(MSG_INTL(MSG_DYN_ENTRY), index,
	    /* LINTED */
	    conv_dyntag_str((Sword)dyn->d_tag, mach),
	    EC_XWORD(dyn->d_un.d_val), name);
}
