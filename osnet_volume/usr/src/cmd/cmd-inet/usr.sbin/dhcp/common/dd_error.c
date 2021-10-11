/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd_error.c	1.3	96/01/29 SMI"

#include <string.h>
#include <dhcdata.h>
#include <locale.h>

char *
disp_err(int index)
{
	static char	msg[1024];

	switch (index) {
	case TBL_BAD_DIRECTIVE:
		(void) strcpy(msg,
		    gettext("Unsupported dhcpdata.conf keyword"));
		break;
	case TBL_BAD_DOMAIN:
		(void) strcpy(msg,
		    gettext("Path must be valid nisplus domain name"));
		break;
	case TBL_BAD_NS:
		(void) strcpy(msg,
		    gettext("Bad Resource name. Must be 'files' or 'nisplus'"));
		break;
	case TBL_BAD_SYNTAX:
		(void) strcpy(msg, gettext("Syntax Error"));
		break;
	case TBL_CHMOD_ERROR:
		(void) strcpy(msg, gettext("Chmod Error"));
		break;
	case TBL_CHOWN_ERROR:
		(void) strcpy(msg, gettext("Chown Error"));
		break;
	case TBL_ENTRY_EXISTS:
		(void) strcpy(msg, gettext("Entry already exists"));
		break;
	case TBL_IS_BUSY:
		(void) strcpy(msg, gettext("Busy"));
		break;
	case TBL_MATCH_CRITERIA_BAD:
		(void) strcpy(msg, gettext("Match Criteria bad"));
		break;
	case TBL_NISPLUS_ERROR:
		(void) strcpy(msg, gettext("Nisplus error"));
		break;
	case TBL_NO_ACCESS:
		(void) strcpy(msg, gettext("No permission"));
		break;
	case TBL_NO_ENTRY:
		(void) strcpy(msg, gettext("Entry does not exist"));
		break;
	case TBL_NO_GROUP:
		(void) strcpy(msg, gettext("No group"));
		break;
	case TBL_NO_MEMORY:
		(void) strcpy(msg, gettext("No more memory"));
		break;
	case TBL_NO_TABLE:
		(void) strcpy(msg, gettext("Table does not exist"));
		break;
	case TBL_NO_USER:
		(void) strcpy(msg, gettext("No user"));
		break;
	case TBL_OPEN_ERROR:
		(void) strcpy(msg, gettext("Open error"));
		break;
	case TBL_READLINK_ERROR:
		(void) strcpy(msg, gettext("Readlink error"));
		break;
	case TBL_READ_ERROR:
		(void) strcpy(msg, gettext("Read error"));
		break;
	case TBL_RENAME_ERROR:
		(void) strcpy(msg, gettext("Rename error"));
		break;
	case TBL_STAT_ERROR:
		(void) strcpy(msg, gettext("Stat error"));
		break;
	case TBL_TABLE_EXISTS:
		(void) strcpy(msg, gettext("Table already exists"));
		break;
	case TBL_TOO_BIG:
		(void) strcpy(msg, gettext("Too Big"));
		break;
	case TBL_UNLINK_ERROR:
		(void) strcpy(msg, gettext("Unlink error"));
		break;
	case TBL_UNSUPPORTED_FUNC:
		(void) strcpy(msg, gettext("Unsupported function"));
		break;
	case TBL_WRITE_ERROR:
		(void) strcpy(msg, gettext("Write error"));
		break;
	case TBL_INV_CLIENT_ID:
		(void) strcpy(msg, gettext("Invalid client identifier"));
		break;
	case TBL_INV_DHCP_FLAG:
		(void) strcpy(msg, gettext("Invalid dhcp flag"));
		break;
	case TBL_INV_HOST_IP:
		(void) strcpy(msg, gettext("Invalid IP address"));
		break;
	case TBL_INV_LEASE_EXPIRE:
		(void) strcpy(msg, gettext("Invalid lease expiration"));
		break;
	case TBL_INV_PACKET_MACRO:
		(void) strcpy(msg, gettext("Invalid macro name"));
		break;
	case TBL_INV_DHCPTAB_KEY:
		(void) strcpy(msg, gettext("Invalid dhcptab key"));
		break;
	case TBL_INV_DHCPTAB_TYPE:
		(void) strcpy(msg, gettext("Invalid dhcptab type"));
		break;
	case TBL_INV_HOSTNAME:
		(void) strcpy(msg, gettext("Invalid hostname"));
		break;
	case TBL_NO_CRED:
		(void) strcpy(msg, gettext("No credentials"));
		break;
	default:
		(void) strcpy(msg, gettext("Unknown error"));
		break;
	}
	return (msg);
}
