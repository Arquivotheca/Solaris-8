/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)audit.c	1.11	99/05/27 SMI"

#include	<dlfcn.h>
#include	<stdio.h>
#include	"_debug.h"
#include	"msg.h"
#include	"libld.h"


void
Dbg_audit_version(const char * lib, ulong_t version)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_VERSION), lib, (int)version);
}

void
Dbg_audit_lib(const char * lib)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_INIT), lib);
}

void
Dbg_audit_interface(const char * lib, const char * interface)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_INTERFACE), lib, interface);
}

void
Dbg_audit_object(const char * lib, const char * obj)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_OBJECT), lib, obj);
}

void
Dbg_audit_symval(const char * lib, const char * func, const char * sym,
    Addr pval, Addr nval)
{
	char	mesg[100];

	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (pval == nval)
		mesg[0] = '\0';
	else
		(void) sprintf(mesg, MSG_INTL(MSG_AUD_SYMNEW), EC_XWORD(nval));

	dbg_print(MSG_INTL(MSG_AUD_SYM), lib, func, sym, EC_XWORD(pval), mesg);
}
