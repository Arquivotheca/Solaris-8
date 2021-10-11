/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)args.c	1.5	98/08/28 SMI"

#include	"msg.h"
#include	"_debug.h"

void
Dbg_args_flags(int ndx, int c)
{
	if (DBG_NOTCLASS(DBG_ARGS))
		return;

	dbg_print(MSG_INTL(MSG_ARG_FLAG), ndx, c);
}

void
Dbg_args_files(int ndx, char * file)
{
	if (DBG_NOTCLASS(DBG_ARGS))
		return;

	dbg_print(MSG_INTL(MSG_ARG_FILE), ndx, file);
}
