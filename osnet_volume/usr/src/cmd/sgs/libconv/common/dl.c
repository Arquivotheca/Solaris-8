/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dl.c	1.17	99/09/14 SMI"

#include	<string.h>
#include	"_conv.h"
#include	"dl_msg.h"

#define	MODESZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_RTLD_GLOBAL_SIZE + \
		MSG_RTLD_LAZY_SIZE + \
		MSG_RTLD_PARENT_SIZE + \
		MSG_RTLD_WORLD_SIZE + \
		MSG_RTLD_GROUP_SIZE + \
		MSG_RTLD_NODELETE_SIZE + \
		MSG_RTLD_NOLOAD_SIZE + \
		MSG_RTLD_CONFGEN_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

/*
 * String conversion routine for dlopen() attributes.
 */
const char *
conv_dlmode_str(int mode)
{
	static	char	string[MODESZ] = { '\0' };

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if (mode & RTLD_GLOBAL)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_GLOBAL));
	else if ((mode & RTLD_NOLOAD) != RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_LOCAL));

	if ((mode & RTLD_NOW) == RTLD_NOW)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOW));
	else if ((mode & RTLD_NOLOAD) != RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_LAZY));

	if (mode & RTLD_PARENT)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_PARENT));
	if (mode & RTLD_WORLD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_WORLD));
	if (mode & RTLD_GROUP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_GROUP));
	if (mode & RTLD_NODELETE)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NODELETE));
	if (mode & RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOLOAD));
	if (mode & RTLD_CONFGEN)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_CONFGEN));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

	return ((const char *)string);
}

#define	FLAGSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_RTLD_REL_RELATIVE_SIZE + \
		MSG_RTLD_REL_EXEC_SIZE + \
		MSG_RTLD_REL_DEPENDS_SIZE + \
		MSG_RTLD_REL_PRELOAD_SIZE + \
		MSG_RTLD_REL_SELF_SIZE + \
		MSG_RTLD_REL_WEAK_SIZE + \
		MSG_RTLD_MEMORY_SIZE + \
		MSG_RTLD_STRIP_SIZE + \
		MSG_RTLD_NOHEAP_SIZE + \
		MSG_RTLD_CONFSET_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

/*
 * String conversion routine for dldump() flags.
 */
const char *
conv_dlflag_str(int flags)
{
	static	char	string[FLAGSZ] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if ((flags & RTLD_REL_ALL) == RTLD_REL_ALL)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_ALL));
	else {
		if (flags & RTLD_REL_RELATIVE)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_RELATIVE));
		if (flags & RTLD_REL_EXEC)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_EXEC));
		if (flags & RTLD_REL_DEPENDS)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_DEPENDS));
		if (flags & RTLD_REL_PRELOAD)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_PRELOAD));
		if (flags & RTLD_REL_SELF)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_SELF));
		if (flags & RTLD_REL_WEAK)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_WEAK));
	}

	if (flags & RTLD_MEMORY)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_MEMORY));
	if (flags & RTLD_STRIP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_STRIP));
	if (flags & RTLD_NOHEAP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOHEAP));
	if (flags & RTLD_CONFSET)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_CONFSET));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));
	return ((const char *)string);
}
