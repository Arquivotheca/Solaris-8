/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ERRMSG_H
#define	_ERRMSG_H

#pragma ident	"@(#)errmsg.h	1.14	99/11/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* text for gettext error messages for adddrv.c and remdrv.c */

#define	USAGE	"Usage:\n"\
"	add_drv [ -m '<permission> ','<...>' ]\n"\
"		[ -n ]\n"\
"		[ -f ]\n"\
"		[ -v ]\n"\
"		[ -i '<identify_name  <...>' ] \n"\
"		[ -b <basedir> ]\n"\
"		[ -c <class_name> ]\n"\
"		<driver_module>\n"\
"Example:\n"\
"	add_drv -m '* 0666 bin bin' -i 'acme,sd new,sd' sd \n"\
"	Add 'sd' drive with identify names: acme,sd and new,sd.\n"\
"	Every minor node will have the permission 0666,\n"\
"	and be owned by bin with group bin.\n"

#define	BOOT_CLIENT	"Reboot client to install driver.\n"
#define	DRIVER_INSTALLED	"Driver (%s) installed.\n"

#define	ERR_INSTALL_FAIL	"Error: Could not install driver (%s).\n"
#define	ERR_ALIAS_IN_NAM_MAJ	"Alias (%s) already in use as driver name.\n"
#define	ERR_ALIAS_IN_USE	"(%s) already in use as a driver or alias.\n"
#define	ERR_CANT_ACCESS_FILE	"Cannot access file (%s).\n"
#define	ERR_REM_LOCK		"Cannot remove lockfile (%s). Remove by hand.\n"
#define	ERR_BAD_PATH	"Bad syntax for pathname : (%s)\n"
#define	ERR_NO_DRVNAME  "Missing driver name : (%s)\n"
#define	ERR_FORK_FAIL	"Fork failed; cannot exec : %s\n"
#define	ERR_PROG_IN_USE	"add_drv/rem_drv currently busy; try later\n"
#define	ERR_NOT_ROOT	"You must be root to run this program.\n"
#define	ERR_BAD_LINE	"Bad line in file %s : %s\n"
#define	ERR_CANNOT_OPEN	"Cannot open (%s).\n"
#define	ERR_MIS_TOK	"Option (%s) : missing token: (%s)\n"
#define	ERR_TOO_MANY_ARGS	"Option (%s) : too many arguments: (%s)\n"
#define	ERR_BAD_MODE	"Bad mode: (%s)\n"
#define	ERR_CANT_OPEN	"Cannot open (%s)\n"
#define	ERR_NO_UPDATE	"Cannot update (%s)\n"
#define	ERR_CANT_RM	"Cannot remove temporary file (%s); remove by hand.\n"
#define	ERR_BAD_LINK	"(%s) exists as (%s); Please rename by hand.\n"
#define	ERR_NO_MEM		"Not enough memory\n"
#define	ERR_DEL_ENTRY	"Cannot delete entry for driver (%s) from file (%s).\n"
#define	ERR_INT_UPDATE	"Internal error updating (%s).\n"
#define	ERR_NOMOD	"Cannot find module (%s).\n"
#define	ERR_MAX_MAJOR	"Cannot get major device information.\n"
#define	ERR_NO_FREE_MAJOR	"No available major numbers.\n"
#define	ERR_NOT_UNIQUE	"Driver (%s) is already installed.\n"
#define	ERR_NOT_INSTALLED "Driver (%s) not installed.\n"
#define	ERR_UPDATE	"Cannot update (%s).\n"
#define	ERR_MAX_EXCEEDS "Major number (%d) exceeds maximum (%d).\n"
#define	ERR_NO_CLEAN	"Cannot update; check file %s and rem_drv %s by hand.\n"
#define	ERR_CONFIG	\
"Warning: Driver (%s) successfully added to system but failed to attach\n"
#define	ERR_DEVTREE	\
"Warning: Unable to check for driver configuration conflicts.\n"
#define	ERR_MODPATH	"System error: Could not get module path.\n"
#define	ERR_BAD_MAJNUM	\
"Warning: Major number (%d) inconsistent with /etc/name_to_major file.\n"
#define	ERR_MAJ_TOOBIG	"Warning: Entry '%s %llu' in %s has a major number " \
			"larger\nthan the maximum allowed value %u.\n"
#define	ERR_LOCKFILE	"Failed to create lock file.\n"

/* remdrv messages */

#define	REM_USAGE1 "Usage:\n\t rem_drv [ -b <basedir> ] driver_name\n"
#define	ERR_NO_MAJ	"Cannot get major number for :  %s\n"
#define	ERR_NOUNLOAD "Cannot modunload driver : %s\n"
#define	ERR_UNLINK	"Warning: Cannot remove %s from devfs namespace.\n"
#define	ERR_PIPE	"System error : Cannot create pipe\n"
#define	ERR_EXEC	"System error : Exec failed\n"
#define	ERR_DEVFSCLEAN  \
"Warning: Cannot remove entries from devfs namespace for driver : %s.\n"
#define	ERR_DEVFSALCLEAN  \
"Warning: Cannot remove alias entries from devfs namespace for driver : %s .\n"
#define	ERR_MODID	"Cannot get modid for : (%s)\n"
#define	ERR_MODUN	\
	"Cannot unload module: %s\nWill be unloaded upon reboot.\n"
#define	ERR_NOENTRY	"Cannot find (%s) in file : %s\n"

/* drvsubr messages */
#define	ERR_NOFILE	"Warning: (%s) file missing.\n"
#define	ERR_NO_SPACE	\
"Can't have space within double quote: %s. \
Use octal escape sequence \"\\040\".\n"

#ifdef	__cplusplus
}
#endif

#endif	/* _ERRMSG_H */
