/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)deftag.c	1.5	98/08/31 SMI"

/*
 * String conversion routine for symbol references.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"deftag_msg.h"

static const Msg refs[] = {
	MSG_REF_DYN_SEEN,	MSG_REF_DYN_NEED,	MSG_REF_REL_NEED
};

const char *
conv_deftag_str(Symref ref)
{
	static char	string[STRSIZE] = { '\0' };

	if (ref >= REF_NUM)
		return (conv_invalid_str(string, (Lword)ref, 0));
	else
		return (MSG_ORIG(refs[ref]));
}
