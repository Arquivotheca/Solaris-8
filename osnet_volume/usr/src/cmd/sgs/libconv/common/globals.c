/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)globals.c	1.6	98/08/31 SMI"

#include	<stdio.h>
#include	<sys/machelf.h>
#include	"globals_msg.h"

const char *
conv_invalid_str(char * string, Lword value, int decimal)
{
	Msg	format;

	if (decimal)
		format = MSG_GBL_FMT_DEC;
	else
		format = MSG_GBL_FMT_HEX;

	(void) sprintf(string, MSG_ORIG(format), value);
	return ((const char *)string);
}
