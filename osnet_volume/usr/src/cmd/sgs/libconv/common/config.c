/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)config.c	1.1	99/08/13 SMI"

#include	<string.h>
#include	"debug.h"
#include	"rtc.h"
#include	"_conv.h"
#include	"config_msg.h"

#define	MODESZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_CONF_EDLIBPATH_SIZE + \
		MSG_CONF_ADLIBPATH_SIZE + \
		MSG_CONF_ESLIBPATH_SIZE + \
		MSG_CONF_ASLIBPATH_SIZE + \
		MSG_CONF_DIRCFG_SIZE + \
		MSG_CONF_OBJALT_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

/*
 * String conversion routine for configuration file information.
 */
const char *
conv_config_str(int feature)
{
	static	char	string[MODESZ] = { '\0' };

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if (feature & DBG_CONF_EDLIBPATH)
		(void) strcat(string, MSG_ORIG(MSG_CONF_EDLIBPATH));
	if (feature & DBG_CONF_ESLIBPATH)
		(void) strcat(string, MSG_ORIG(MSG_CONF_ESLIBPATH));
	if (feature & DBG_CONF_ADLIBPATH)
		(void) strcat(string, MSG_ORIG(MSG_CONF_ADLIBPATH));
	if (feature & DBG_CONF_ASLIBPATH)
		(void) strcat(string, MSG_ORIG(MSG_CONF_ASLIBPATH));
	if (feature & DBG_CONF_DIRCFG)
		(void) strcat(string, MSG_ORIG(MSG_CONF_DIRCFG));
	if (feature & DBG_CONF_OBJALT)
		(void) strcat(string, MSG_ORIG(MSG_CONF_OBJALT));
	if (feature & DBG_CONF_MEMRESV)
		(void) strcat(string, MSG_ORIG(MSG_CONF_MEMRESV));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

	return ((const char *)string);
}

#define	FLAGSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_CONF_DIRENT_SIZE + \
		MSG_CONF_EXEC_SIZE + \
		MSG_CONF_ALTER_SIZE + \
		MSG_CONF_DUMP_SIZE + \
		MSG_CONF_ALIAS_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

/*
 * String conversion routine for object flags.
 */
const char *
conv_config_obj(Half flags)
{
	static char	string[FLAGSZ] = { '\0' };

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if (flags & RTC_OBJ_DIRENT) {
		(void) strcat(string, MSG_ORIG(MSG_CONF_DIRENT));
		if (flags & RTC_OBJ_NOEXIST)
			(void) strcat(string, MSG_ORIG(MSG_CONF_NOEXIST));
		if (flags & RTC_OBJ_ALLENTS)
			(void) strcat(string, MSG_ORIG(MSG_CONF_ALLENTS));
	} else {
		if (flags & RTC_OBJ_EXEC)
			(void) strcat(string, MSG_ORIG(MSG_CONF_EXEC));
		if (flags & RTC_OBJ_ALTER)
			(void) strcat(string, MSG_ORIG(MSG_CONF_ALTER));
		if (flags & RTC_OBJ_DUMP)
			(void) strcat(string, MSG_ORIG(MSG_CONF_DUMP));
	}
	if ((flags & RTC_OBJ_REALPTH) == 0)
		(void) strcat(string, MSG_ORIG(MSG_CONF_ALIAS));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

	if (strlen(string) == (MSG_GBL_OSQBRKT_SIZE + MSG_GBL_CSQBRKT_SIZE))
		return (MSG_ORIG(MSG_GBL_NULL));
	else
		return ((const char *)string);
}
