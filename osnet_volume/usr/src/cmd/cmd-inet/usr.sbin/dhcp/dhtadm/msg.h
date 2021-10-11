/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MSG_H
#define	_MSG_H

#pragma ident	"@(#)msg.h	1.5	97/10/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MSG_INV_RESRC	\
	"Invalid resource argument 'files' and 'nisplus' only.\n"
#define	MSG_RESRC_2LONG	"Resource path is too long. Max: %d characters.\n"
#define	MSG_SYM_2LONG	\
	"Symbol name must be less than or equal to %d characters.\n"
#define	MSG_DEF_2LONG	\
	"Definition must be less than or equal to %d characters.\n"
#define	MSG_SYMVAL_2LONG	\
	"Symbol/value pair must be less than or equal to %d characters.\n"
#define	MSG_MACRO_2LONG	\
	"Macro name must be less than or equal to %d characters.\n"
#define	MSG_NMAC_2LONG	\
	"New macro name must be less than or equal to %d characters.\n"
#define	MSG_UNKN_RESRC_TYPE	\
	"Cannot determine resource type.\n"
#define	MSG_SYM_DEF_ERR	\
	"Symbol definition syntax error. See dhcptab(4) manual page.\n"
#define	MSG_MAC_DEF_ERR	\
	"Macro definition syntax error. See dhcptab(4) manual page.\n"
#define	MSG_ADD_FAILED	"Add: failed. Error: %s\n"
#define	MSG_DT_EXISTS	"Create: dhcptab already exists, error: %s\n"
#define	MSG_CREAT_FAILED	"Create: failed. Error: %s\n"
#define	MSG_DEL_FAILED	"Delete: Failed. Error: %s\n"
#define	MSG_MOD_SYM_NOEXIST	\
	"Modify: (symbol) Could not locate entry: '%1$s'. Error: %2$s.\n"
#define	MSG_MODSYM_FAILED	"Modify: (symbol) failed. Error: %s.\n"
#define	MSG_MODMAC_NOEXIST	\
	"Modify: (macro) Could not locate entry: '%1$s'. Error: %2$s.\n"
#define	MSG_MODMAC_FAILED	\
	"Modify: (Macro): Error occurred modifying symbol: '%1$s', Error: %2$s\n"
#define	MSG_MOD_FAILED	"Modify: failed. Error: %s.\n"
#define	MSG_RM_FAILED	"Remove: failed to remove dhcptab, error: %s\n"
#define	MSG_BAD_CNTXT	"Bad context: Must be 'Site' or 'Vendor=...'\n"
#define	MSG_DISP_FAILED	"Print: failed to display dhcptab, error: %s\n"
#define	MSG_BAD_CODE	"Bad code value. Must be a numeric value.\n"
#define	MSG_BAD_EXTEND	"Bad extend code number. Must be %d-127 inclusive.\n"
#define	MSG_BAD_VEND	"Bad vendor code number. Must be 1-254 inclusive.\n"
#define	MSG_BAD_SITE	"Bad site code number. Must be 128-254 inclusive.\n"
#define	MSG_BAD_VAL	\
	"Bad value type. Valid types are: %1$s, %2$s, %3$s, %4$s, %5$s \n"
#define	MSG_BAD_GRAN	"Bad granularity. Must be a numeric value.\n"
#define	MSG_BAD_MAX	"Bad max items. Must be a numeric value.\n"
#define	MSG_NO_QUOTE	"Quoted string has no closing quote.\n"
#define	MSG_BAD_SYM_FORM	"Edit string: %s not of symbol=value form.\n"
#define	MSG_BAD_STRING	\
	"Newlines in string values must be escaped using backslashes (\\)\n"
#define	MSG_SYM_2BIG	"Symbol name: '%s' is greater than %d characters.\n"
#define	MSG_USAGE	"dhtadm [-r (resource)] [-p (path)] (options)\n\nWhere (options) is one of:\n\n-C\t\tCreate the dhcptab\n\n-A\t\tAdd symbol or macro. Sub-options:\n\t\t{ -s (symbol name) | -m (macro name) } -d (definition) \n\n-M\t\tModify symbol or macro. Sub-options:\n\t\t-s (old symbol name) {-n (new name) | -d (definition)}\n\t\t\t\tOr\n\t\t-m (old macro name) {-n (new name) | -d (definition) | -e (symbol = value)}\n\n-D\t\tDelete symbol or macro definition. Sub-options:\n\t\t-s ( symbol name ) | -m ( macro name ) \n\n-R\t\tRemove the dhcptab\n\n-P\t\tDisplay the dhcptab\n\n"

#ifdef	__cplusplus
}
#endif

#endif	/* _MSG_H */
