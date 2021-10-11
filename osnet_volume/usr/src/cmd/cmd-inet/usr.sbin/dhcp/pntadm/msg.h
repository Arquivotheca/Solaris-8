/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MSG_H
#define	_MSG_H

#pragma ident	"@(#)msg.h	1.11	99/11/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MSG_INV_CLIENT	"Invalid Client parameter: %1$s Error: %2$s\n"
#define	MSG_INV_RESRC	\
	"Invalid resource argument 'files' and 'nisplus' only.\n"
#define	MSG_RESRC_2BIG	"Resource path is too long. Max: %d characters.\n"
#define	MSG_COMMENT_2BIG	"Comment is too long. Max: %d characters.\n"
#define	MSG_INV_LEASE	"Invalid lease time. Should be mm/dd/yyyy.\n"
#define	MSG_INV_KEYWORD	"Invalid flag keyword: '%s' ignored.\n"
#define	MSG_HOST_2BIG	\
	"Host name must be less than or equal to %d characters.\n"
#define	MSG_HOST_DIGIT	"Host names starting with digits are invalid.\n"
#define	MSG_CID_2BIG	"Client identifier is too long. Max: %d characters.\n"
#define	MSG_MACRO_2BIG	\
	"Macro name must be less than or equal to %d characters.\n"
#define	MSG_INV_NEW_CIP	"Invalid New Client IP address\n"
#define	MSG_SIP_2LONG	\
	"Server name must be less than or equal to %d characters.\n"
#define	MSG_INV_SERVER	"Invalid Server parameter: %1$s Error: %2$s\n"
#define	MSG_NO_NETWORK	"No network has been specified.\n"
#define	MSG_NET_2LONG	"Network must be less than or equal to %d characters.\n"
#define	MSG_INV_NET_IP	"Invalid network IP address.\n"
#define	MSG_NO_SUCH_NET	"network: '%s' not in 'networks' table\n"
#define	MSG_USE_DEF_MASK	"Warning: Using default subnetmask: %s.\n"
#define	MSG_WRONG_NET	"Client: '%1$s' is not part of the '%2$s' network.\n"
#define	MSG_CANT_DET_RESRC	 "Cannot determine resource type.\n"
#define	MSG_ERR_CNVT_CID	"Error converting client identifier.\n"
#define	MSG_CANT_FIND_MACRO	\
	"Cannot find dhcptab macro '%1$s'. Error: %2$s\n"
#define	MSG_CANT_GEN_DEF_SIP	"Cannot generate default server address.\n"
#define	MSG_HOST_EXISTS	"Add: Hosts entry already exists for %s\n"
#define	MSG_ADD_HOST_FAIL	\
	"Add: Hosts entry: '%1$s %2$s' failed. Error: %3$s\n"
#define	MSG_ADD_FAILED	"Add: %1$s failed. Error: %2$s\n"
#define	MSG_HOST_CLEANUP	\
	"Add: Could not clean up Host entry: %1$s. Error: %2$s\n"
#define	MSG_CREATE_EXISTS	"Create: %1$s already exists, error: %2$s\n"
#define	MSG_CREATE_FAILED	"Create: %1$s failed. Error: %2$s\n"
#define	MSG_DEL_HOST_FAIL	"Delete: Host entry: %1$s Failed. Error: %2$s\n"
#define	MSG_DEL_FAILED	"Delete: %1$s Failed. Error: %2$s\n"
#define	MSG_MOD_HOST_INUSE	"Modify: Hosts entry %s in use.\n"
#define	MSG_MOD_ADDHOST_FAILED	\
	"Modify: Failed to add hosts entry: '%1$s %2$s'. Error: %3$s\n"
#define	MSG_MOD_CHNG_HOSTS	\
	"Modify: Change Hosts entry: '%1$s' to '%s' failed. Error: %s\n"
#define	MSG_MOD_NOENT	"Modify: Could not find entry: '%1$s'. Error: %2$s.\n"
#define	MSG_MOD_FAILED	"Modify: %1$s failed. Error: %2$s.\n"
#define	MSG_RM_FAILED	"Remove: failed to remove %1$s, error: %2$s\n"
#define	MSG_USAGE	"pntadm [-r (resource)] [-p (path)] (options) [(network ip or name)]\n\nWhere (options) is one of:\n\n -C\t\t\tCreate the named table\n\n -A (client ip or name)\tAdd client entry. Sub-options:\n\t\t\t[-c (comment)]\n\t\t\t[-e (lease expiration)]\n\t\t\t[-f (flags)]\n\t\t\t[-h (client host name)]\n\t\t\t[-i (client identifier)[-a]]\n\t\t\t[-m (dhcptab macro reference)[-y]]\n\t\t\t[-s (server ip or name)]\n\n -M (client ip or name)\tModify client entry. Sub-options:\n\t\t\t[-c (new comment)]\n\t\t\t[-e (new lease expiration)]\n\t\t\t[-f (new flags)]\n\t\t\t[-h (new client host name)]\n\t\t\t[-i (new client identifier)[-a]]\n\t\t\t[-m (new dhcptab macro reference)[-y]]\n\t\t\t[-n (new client ip)]\n\t\t\t[-s (new server ip or name)]\n\n -D (client ip or name)\tDelete client entry. Sub-options:\n\t\t\t[-y]\tRemove host table entry\n\n -R\t\t\tRemove the named table\n\n -P\t\t\tDisplay the named table. Sub-options:\n\t\t\t[-v]\tDisplay lease time in full format.\n\n -L\t\t\tList the configured DHCP networks\n\nThe network ip or name argument is required for all options except -L\n\n"
#define	MSG_INV_IP	"Invalid IP address"
#define	MSG_NO_CNVT_NM_TO_IP	"Cannot convert name into an IP address"
#define	MSG_BAD_CID_ODD	"Client ID must have an even number length.\n"
#define	MSG_BAD_CID_FORMAT	\
	"Client ID must consist of valid hexadecimal digits [0-9], [a-F].\n"
#define	MSG_BAD_LIST_PATH	\
"List: Unable to locate DHCP network tables at %1$s\n"
#define	MSG_LIST_FAILED	"List: Failed. Error: %1$s\n"

#ifdef	__cplusplus
}
#endif

#endif	/* _MSG_H */
