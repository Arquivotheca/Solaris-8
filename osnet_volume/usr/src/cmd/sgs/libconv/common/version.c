/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)version.c	1.6	98/08/28 SMI"

/*
 * String conversion routine for version flag entries.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"version_msg.h"

const char *
conv_verflg_str(Half flags)
{
	/*
	 * Presently we only know about a weak flag
	 */
	if (flags & VER_FLG_WEAK)
		return (MSG_ORIG(MSG_VER_FLG_WEAK));
	else if (flags & VER_FLG_BASE)
		return (MSG_ORIG(MSG_VER_FLG_BASE));
	else
		return (MSG_ORIG(MSG_GBL_NULL));
}
