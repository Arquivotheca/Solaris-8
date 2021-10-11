/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _CMD_MODLOAD_ADDREM_H
#define	_CMD_MODLOAD_ADDREM_H

#pragma ident	"@(#)addrem.h	1.13	98/07/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* defines for add_drv.c and rem_drv.c */

#define	SUCCESS	0
#define	FAILURE -1
#define	NOERR	0
#define	ERROR	-1
#define	UNIQUE	-2
#define	NOT_UNIQUE -3

#define	MAX_CMD_LINE	256
#define	MAX_N2M_ALIAS_LINE	FILENAME_MAX + FILENAME_MAX + 1
#define	MAXLEN_NAM_TO_MAJ_ENT 	FILENAME_MAX + MAX_STR_MAJOR + 1
#define	OPT_LEN		128
#define	CADDR_HEX_STR	16
#define	UINT_STR	10
#define	MODLINE_ENT_MAX	(4 * UINT_STR) + CADDR_HEX_STR + MODMAXNAMELEN
#define	MAX_STR_MAJOR	UINT_STR
#define	STR_LONG	10
#define	PERM_STR	4
#define	MAX_PERM_ENTRY	(2 * STR_LONG) + PERM_STR + (2 * FILENAME_MAX) + 1
#define	MAX_DBFILE_ENTRY	MAX_PERM_ENTRY

#define	CLEAN_MINOR_PERM	0x00000001
#define	CLEAN_DRV_ALIAS		0x00000002
#define	CLEAN_NAM_MAJ		0x00000004
#define	CLEAN_DRV_CLASSES	0x00000010
#define	CLEAN_ALL		(CLEAN_MINOR_PERM | CLEAN_DRV_ALIAS | \
				CLEAN_NAM_MAJ | CLEAN_DRV_CLASSES)

/* add_drv/rem_drv database files */
#define	DRIVER_ALIAS	"/etc/driver_aliases"
#define	DRIVER_CLASSES	"/etc/driver_classes"
#define	MINOR_PERM	"/etc/minor_perm"
#define	NAM_TO_MAJ	"/etc/name_to_major"
#define	REM_NAM_TO_MAJ	"/etc/rem_name_to_major"

#define	ADD_REM_LOCK	"/tmp/AdDrEm.lck"
#define	TMPHOLD		"/etc/TmPhOlD"

/* pointers to add_drv/rem_drv database files */
char *driver_aliases;
char *driver_classes;
char *minor_perm;
char *name_to_major;
char *rem_name_to_major;
char *add_rem_lock;
char *tmphold;

/* devfs root string */
char *devfs_root;

/* names of things: directories, commands, files */
#define	KERNEL_DRV	"/kernel/drv"
#define	USR_KERNEL_DRV	"/usr/kernel/drv"
#define	DRVCONFIG_PATH	"/usr/sbin/drvconfig"
#define	DRVCONFIG	"drvconfig"
#define	DEVFSADM_PATH	"/usr/sbin/devfsadm"
#define	DEVFSADM	"devfsadm"
#define	DEVFS_ROOT	"/devices"


#define	RECONFIGURE	"/reconfigure"
#define	DEVLINKS_PATH	"/usr/sbin/devlinks"
#define	DISKS_PATH	"/usr/sbin/disks"
#define	PORTS_PATH	"/usr/sbin/ports"
#define	TAPES_PATH	"/usr/sbin/tapes"
#define	MODUNLOAD_PATH	"/usr/sbin/modunload"

extern void remove_entry(int, char *);
extern char *get_next_entry(char *, char *);
extern char *get_perm_entry(char *, char *);
extern int some_checking(int, int);
extern void err_exit(void);
extern void exit_unlock(void);
extern char *get_entry(char *, char *, char);
extern int build_filenames(char *);
extern int append_to_file(char *, char *, char *, char, char *);
extern int get_major_no(char *, char *);
extern int get_driver_name(int, char *, char *);
extern int delete_entry(char *oldfile, char *driver_name, char *marker);
extern int get_max_major(char *file_name);

/* modctl() not defined */
extern int modctl(int, ...);

/* drvsubr.c */
#define	XEND	".XXXXXX"

/*
 * XXX
 * define for maximum length of modules paths - we need
 * a common symbol with kbi folks for this
 */
#define	MAXMODPATHS 1024

/* module path list separators */
#define	MOD_SEP	" :"

#ifdef	__cplusplus
}
#endif

#endif /* _CMD_MODLOAD_ADDREM_H */
