/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ifndef	_ITU_H
#define	_ITU_H

#ident	"@(#)itu.h	1.12	99/04/23 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  itu.h -- public definitions for itu module
 */

#define	TOK_IF_VERSION	{"interface_version", IF_VERSION, I_FALSE, 0}
#define	TOK_PSTAMP	{"pstamp", PSTAMP, I_FALSE, 0}
#define	TOK_ITUTYPE	{"itu_type", ITU_TYPE, I_FALSE, 0}
#define	TOK_NAME 	{"name", DRV_NAME, I_FALSE, 0}
#define	TOK_CLASS	{"class", DRV_CLASS, I_TRUE, 0}
#define	TOK_SYS_ENT	{"system_entry", SYSTEM_ENTRY, I_TRUE, 0}
#define	TOK_DEVLINK_ENT	{"devlink_entry", DEVLINK_ENTRY, I_TRUE, 0}
#define	TOK_DEV_ID	{"dev_id", DEV_ID, I_TRUE, 0}
#define	TOK_NODE_NAME	{"node_name", NODE_NAME, I_TRUE, 0}
#define	TOK_DEV_TYPE	{"dev_type", DEV_TYPE, I_FALSE, 0}
#define	TOK_BUS_TYPE	{"bus_type", BUS_TYPE, I_TRUE, 0}
#define	TOK_BEF_NAME	{"bef_name", BEF_NAME, I_FALSE, 0}
#define	TOK_DEV_DESC	{"describe_dev", DEV_DESC, I_TRUE, 0}
#define	TOK_REALMODE_PATH {"realmode_path", REALMODE_PATH, I_FALSE, 0}
#define	TOK_MAP		{"map", MAP, I_TRUE, 0}
#define	TOK_LEGACY_DEV	{"legacy_device", LEGACY_DEV, I_FALSE, 0}
#define	TOK_LOAD_ALWAYS	{"load_always", LOAD_ALWAYS, I_FALSE, 0}
#define	TOK_INSTALL_ALWAYS {"install_always", INSTALL_ALWAYS, I_FALSE, 0}
#define	TOK_MACH_ENT	{"mach_entry", MACH_ENTRY, I_FALSE, 0}
#define	TOK_FILE_EDIT	{"file_edit", FILE_EDIT, I_TRUE, 0}

/* Driver related keywords */

#define	TOK_DRV_PATH	{"driver_path", DRV_PATH, I_FALSE, 0}
#define	TOK_DRV_PKG	{"driverpkg", DRV_PKG, I_FALSE, 0}
#define	TOK_DRV_PKG_DESC {"driverpkgdesc", DRV_PKG_DESC, I_FALSE, 0}
#define	TOK_DRV_PKG_VERS {"driverpkgvers", DRV_PKG_VERS, I_FALSE, 0}

/* Bef related keywords */

#define	TOK_BEF_PKG	{"befpkg", BEF_PKG, I_FALSE, 0}
#define	TOK_BEF_PKG_DESC {"befpkgdesc", BEF_PKG_DESC, I_FALSE, 0}
#define	TOK_BEF_PKG_VERS {"befpkgvers", BEF_PKG_VERS, I_FALSE, 0}

/* Man related keywords */

#define	TOK_MAN_DELIVERY  {"mandelivery", MAN_DELIVERY, I_TRUE, 0}
#define	TOK_MAN_PKG	{"manpkg", MAN_PKG, I_FALSE, 0}
#define	TOK_MAN_PKG_DESC {"manpkgdesc", MAN_PKG_DESC, I_FALSE, 0}
#define	TOK_MAN_PKG_VERS {"manpkgvers", MAN_PKG_VERS, I_FALSE, 0}

/* Patch related keywords */

#define	TOK_PATCH_ID {"patchid", PATCH_ID, I_FALSE, 0}
#define	TOK_PATCH_REQUIRED\
			{"patch_required", PATCH_REQUIRED, I_TRUE, 0}
#define	TOK_PATCH_OBSOLETE\
			{"patch_obsoletes", PATCH_OBSOLETE, I_TRUE, 0}

#define	KEYWORDS (sizeof (keyval)/sizeof (*keyval))

#define	CR_ASSIGN_SEP "=\n\r"
#define	WHITESP_CR_SEP " \t\n\r"
#define	QUOTE_SEP "\""
#define	BRACES "{}"
#define	OPEN_BRACE '{'
#define	CLOSE_BRACE '}'
#define	MAXLINE 120
#define	MAXVERS 5  /* Maximum number of versions supported per floppy */

#define	VOL_A "A:/"
#define	DRV_DIR "/i86pc/Tools/Boot/"
#define	BACK_UP "BACK_UP" /* XXX Returned if we want to back up a screen */
#define	I86PC "/i86pc"
#define	DEFAULT_RELEASE "release.def"
#define	LAYOUT_VER_FILE "layout.ver"
#define	LAYOUT_VERSION "1.0"

/*
 * Comment at beginning of /etc/system entries intended only
 * for boot use; make_ITU doesn't remove comment from patch version.
 */
#define	BOOT_ONLY_SYS	"*boot-only:"
#define	BOOT_ONLY_SYS_LEN	(11)	/* length of string */

/*
 * directories on itus are of the type, sol_25, sol_251.
 * Its a concatatination of sol with rev. The sol_ is referred as the head,
 *  version number tail.
 */
#define	OSDIR_HEAD "sol_"
#define	SLASH '/'

/*
 * Default directory paths.
 */
#define	LEGACY_DIR "/solaris/drivers/isa.160/"
#define	CD_LEGACY_DIR "/boot/solaris/drivers/isa.160/"
#define	SELFID_DIR "/solaris/drivers/notisa.010/"
#define	CD_SELFID_DIR "/boot/solaris/drivers/notisa.010/"
#define	DEFAULT_KDRV_DIR "/platform/i86pc/kernel/drv/"
#define	SPACES "    "

/*
 * RAM File name for kernel files
 */

#define	R_DRV_ALIASES "R:aliases"
#define	R_DRV_CLASSES "R:classes"
#define	R_SYSTEM "R:system"
#define	R_DEVLINK_TAB "R:devlink.tab"
#define	R_NAME_TO_MAJ "R:nam2maj"
#define	R_MACH "R:mach"
#define	R_EDITFILE "R:editfile"

#define	U_DRV_ALIASES "U:aliases"
#define	U_DRV_CLASSES "U:classes"
#define	U_SYSTEM "U:system"
#define	U_DEVLINK_TAB "U:devlink.tab"
#define	U_NAME_TO_MAJ "U:nam2maj"
#define	U_MACH "U:mach"
#define	U_EDITFILE "U:editfile"

#define	ITU_PROPS "itu-props"

typedef enum bool { I_FALSE, I_TRUE } bool_t; /* multiple instantiations */

typedef enum keyval {
	IF_VERSION,
	PSTAMP,
	ITU_TYPE,
	DRV_NAME,
	DRV_CLASS,
	SYSTEM_ENTRY,
	MACH_ENTRY,
	DEVLINK_ENTRY,
	DEV_ID,
	NODE_NAME,
	DEV_TYPE,
	BUS_TYPE,
	DEV_DESC,
	BEF_NAME,
	REALMODE_PATH,
	MAP,
	LEGACY_DEV,
	LOAD_ALWAYS,
	INSTALL_ALWAYS,
	DRV_PATH,
	DRV_PKG,
	DRV_PKG_DESC,
	DRV_PKG_VERS,
	BEF_PKG,
	BEF_PKG_DESC,
	BEF_PKG_VERS,
	MAN_DELIVERY,
	MAN_PKG,
	MAN_PKG_DESC,
	MAN_PKG_VERS,
	PATCH_ID,
	PATCH_REQUIRED,
	PATCH_OBSOLETE,
	FILE_EDIT
} keyval_t;

struct tokmap {
	char *key_name;		/* keyword */
	keyval_t val;		/* value associated with keyword */
	bool_t multi_inst;	/* multiple instances allowed ? */
	int instances;		/* number of instances seen. */
};


struct tokmap keyval[] = {
	TOK_IF_VERSION,
	TOK_PSTAMP,
	TOK_ITUTYPE,
	TOK_NAME,
	TOK_CLASS,
	TOK_SYS_ENT,
	TOK_MACH_ENT,
	TOK_DEVLINK_ENT,
	TOK_DEV_ID,
	TOK_NODE_NAME,
	TOK_DEV_TYPE,
	TOK_BUS_TYPE,
	TOK_DEV_DESC,
	TOK_BEF_NAME,
	TOK_REALMODE_PATH,
	TOK_MAP,
	TOK_LEGACY_DEV,
	TOK_LOAD_ALWAYS,
	TOK_INSTALL_ALWAYS,
	TOK_DRV_PKG,
	TOK_DRV_PATH,
	TOK_DRV_PKG_DESC,
	TOK_DRV_PKG_VERS,
	TOK_BEF_PKG,
	TOK_BEF_PKG_DESC,
	TOK_BEF_PKG_VERS,
	TOK_MAN_PKG,
	TOK_MAN_PKG_DESC,
	TOK_MAN_PKG_VERS,
	TOK_PATCH_ID,
	TOK_PATCH_REQUIRED,
	TOK_PATCH_OBSOLETE,
	TOK_FILE_EDIT
};

/*
 * Struct db_entry. Contains all the fields necessary for creating an entry in
 * the master file. Its a null terminated list.
 */
typedef struct db_entry {
	struct db_entry *db_nextp;
	struct db_entry *db_prevp; /* Unused */
	char *db_dev_id;
	char *db_node_name;
	char *db_dev_type;
	char *db_bus_type;
	char *db_dev_desc;
	char *db_spare;
}dbe_t;

/*
 * entries are generic holders for various string entries. The lists are
 * NULL terminated entries.
 */
typedef struct entries {
	struct entries *en_nextp;
	char *en_data;
}entries_t;

typedef struct ent_head {
	entries_t *ent_hp;
	entries_t *ent_tailp;
} ent_head_t;

/*
 * Different types of updates that ITU can perform.
 */
typedef enum itu_types {
	COMPLETE,
	PARTIAL,
	UNDEFINED
}itu_types_t;

/*
 * itu_config structure contains all the information from an ITU file.
 * Multiple ITU files form a NULL terminated linked list.
 */

typedef struct itu_config {
	struct itu_config *itu_nextp;
	itu_types_t itu_type;		/* type of update  */
	unsigned long itu_pstamp;	/* timestamp of the ITUO */
	float itu_if_version;		/* iterface version number */
	char *itu_pkgpath;		/* path on the floppy or proto dir */
	char *itu_drv_name;		/* driver or node name */
	ent_head_t itu_classes;		/* entries in /etc/driver_classes */
	ent_head_t itu_sysents;		/* entries in the /etc/system */
	ent_head_t itu_machents;	/* entries in the /etc/system */
	ent_head_t itu_devlink;		/* entries in /etc/devlink.tab */
	ent_head_t itu_aliases;		/* entries in /etc/driver_aliases */
	ent_head_t itu_edits;		/* file edits */
	dbe_t *itu_dbes;		/* entries in master file (x86) */
	ent_head_t itu_maps;		/* map commands (x86) */
	char *itu_rmodepath;		/* exact bef path (x86) */
	char *itu_drvpath;		/* path where kernel module goes */
	char *itu_bef_name;		/* name of the realmode driver */
	bool_t itu_legacy_dev;		/* set to TRUE for a legacy device */
	bool_t itu_load_always;		/* flag to forceload modules */
	bool_t itu_install_always;	/* flag to force install of modules */
	bool_t itu_bef_statd;		/* set if bef is found. */
	bool_t itu_kdrv_statd;		/* set if kernel driver is found. */
	bool_t itu_cnf_statd;		/* set if conf file is found */
	short itu_drvind;		/* index to last drvpath component */
	char itu_volname[13];		/* floppy volume name. includes a ":" */
}itu_config_t;


typedef struct flop_info {
	struct flop_info *f_nextp;
	char itu_name[PATH_MAX];
	char bef_name[PATH_MAX];
}flop_info_t;


#ifdef __cplusplus
}
#endif

#endif /* _ITU_H */
