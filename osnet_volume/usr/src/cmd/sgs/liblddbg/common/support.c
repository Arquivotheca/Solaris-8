/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)support.c	1.5	98/08/28 SMI"

#include	"msg.h"
#include	"_debug.h"

void
Dbg_support_req(const char * define, int flag)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SUPPORT))
		return;

	if (flag == DBG_SUP_ENVIRON)
		str = MSG_INTL(MSG_SUP_REQ_ENV);
	else if (flag == DBG_SUP_CMDLINE)
		str = MSG_INTL(MSG_SUP_REQ_CMD);
	else
		str = MSG_INTL(MSG_SUP_REQ_DEF);

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SUP_REQ), define, str);
}

void
Dbg_support_load(const char * obj, const char * func)
{
	if (DBG_NOTCLASS(DBG_SUPPORT))
		return;

	dbg_print(MSG_INTL(MSG_SUP_ROUTINE), obj, func);
}

void
Dbg_support_action(const char * obj, const char * func, int flag,
    const char * name)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SUPPORT))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (flag == DBG_SUP_START)
		str = MSG_INTL(MSG_SUP_OUTFILE);
	else if (flag == DBG_SUP_FILE)
		str = MSG_INTL(MSG_SUP_INFILE);
	else if (flag == DBG_SUP_SECTION)
		str = MSG_INTL(MSG_SUP_INSEC);

	if (flag == DBG_SUP_ATEXIT)
		dbg_print(MSG_INTL(MSG_SUP_CALLING_1), func, obj);
	else
		dbg_print(MSG_INTL(MSG_SUP_CALLING_2), func, obj, str, name);
}
