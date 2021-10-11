/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DEVFSADM_H
#define	_DEVFSADM_H

#pragma ident	"@(#)devfsadm.h	1.6	99/08/30 SMI"

#include <sys/types.h>
#include <libdevinfo.h>
#include <sys/devinfo_impl.h>
#include <regex.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEVFSADM_SUCCESS 0
#define	DEVFSADM_FAILURE -1
#define	DEVFSADM_TRUE 0
#define	DEVFSADM_FALSE -1

#define	ILEVEL_0 0
#define	ILEVEL_1 1
#define	ILEVEL_2 2
#define	ILEVEL_3 3
#define	ILEVEL_4 4
#define	ILEVEL_5 5
#define	ILEVEL_6 6
#define	ILEVEL_7 7
#define	ILEVEL_8 8
#define	ILEVEL_9 9

#define	DEVFSADM_V0 0

#define	DEVFSADM_CONTINUE 0
#define	DEVFSADM_TERMINATE 1

#define	INTEGER 0
#define	CHARACTER 1

#define	RM_HOT 0x01
#define	RM_PRE 0x02
#define	RM_POST 0x04
#define	RM_ALWAYS 0x08

#define	TYPE_EXACT 0x01
#define	TYPE_RE 0x02
#define	TYPE_PARTIAL 0x04
#define	TYPE_MASK 0x07
#define	DRV_EXACT 0x10
#define	DRV_RE 0x20
#define	DRV_MASK 0x30
#define	CREATE_DEFER 0x100
#define	CREATE_MASK 0x100

/* flags for devfsadm_mklink */
#define	DEV_SYNC 0x02	/* synchronous mklink */

#define	INFO_MID		NULL		/* always prints */
#define	VERBOSE_MID		"verbose"	/* prints with -v */
#define	CHATTY_MID		"chatty" 	/* prints with -V chatty */

typedef struct devfsadm_create {
	char	*device_class;	/* eg "disk", "tape", "display" */
	char	*node_type;	/* eg DDI_NT_TAPE, DDI_NT_BLOCK, etc */
	char	*drv_name;	/* eg sd, ssd */
	int	flags;		/* TYPE_{EXACT,RE,PARTIAL}, DRV_{EXACT,RE} */
	int interpose_lvl;	/* eg ILEVEL_0.. ILEVEL_10 */
	int (*callback_fcn)(di_minor_t minor, di_node_t node);
} devfsadm_create_t;

typedef struct devfsadm_remove {
	char 	*device_class;	/* eg "disk", "tape", "display" */
	char    *dev_dirs_re;   /* dev dirs regex selector */
	int	flags;		/* eg POST, PRE, HOT, ALWAYS */
	int	interpose_lvl;	/* eg ILEVEL_0 .. ILEVEL_10 */
	void (*callback_fcn)(char *logical_link);
} devfsadm_remove_t;

typedef struct _devfsadm_create_reg {
	uint_t version;
	uint_t count;	/* number of node type registration */
			/* structures */
	devfsadm_create_t *tblp;
} _devfsadm_create_reg_t;

typedef struct _devfsadm_remove_reg {
	uint_t version;
	uint_t count;   /* number of node type registration */
			/* structures */
	devfsadm_remove_t *tblp;
} _devfsadm_remove_reg_t;


/*
 * "flags" in the devfs_enumerate structure can take the following values.
 * These values specify the substring of devfs path to be used for
 * enumeration. Components (see MATCH_ADDR/MATCH_MINOR) may be specified
 * by using the "match_arg" member in the devfsadm_enumerate structure.
 */
#define	MATCH_ALL	0x001	/* Match entire devfs path */
#define	MATCH_PARENT	0x002	/* Match upto last '/' in devfs path */
#define	MATCH_ADDR	0x004	/* Match upto nth component of last address */
#define	MATCH_MINOR	0x008	/* Match upto nth component of minor name */
#define	MATCH_CALLBACK	0x010	/* Use callback to derive match string */

/*
 * The following flags are private to devfsadm and the disks module.
 * NOT to be used by other modules.
 */
#define	MATCH_NODE	0x020
#define	MATCH_MASK	0x03F
#define	MATCH_UNCACHED	0x040 /* retry flags for disks module */

typedef struct devfsadm_enumerate {
	char *re;
	int subexp;
	uint_t flags;
	char *match_arg;
	char *(*sel_fcn)(const char *path, void *cb_arg);
	void *cb_arg;
} devfsadm_enumerate_t;

#define	DEVFSADM_CREATE_INIT_V0(tbl) \
	_devfsadm_create_reg_t _devfsadm_create_reg = { \
	DEVFSADM_V0, \
	(sizeof (tbl) / sizeof (devfsadm_create_t)), \
	((devfsadm_create_t *)(tbl)) }

#define	DEVFSADM_REMOVE_INIT_V0(tbl)\
	_devfsadm_remove_reg_t _devfsadm_remove_reg = {\
	DEVFSADM_V0, \
	(sizeof (tbl) / sizeof (devfsadm_remove_t)), \
	((devfsadm_remove_t *)(tbl)) }

int devfsadm_noupdate(void);
const char *devfsadm_root_path(void);
int devfsadm_link_valid(char *link);
int devfsadm_mklink(char *link, di_node_t node, di_minor_t minor, int flags);
int devfsadm_secondary_link(char *link, char *primary_link, int flags);
void devfsadm_rm_link(char *file);
void devfsadm_rm_all(char *file);
void devfsadm_rm_stale_links(char *dir_re, char *valid_link, di_node_t node,
		di_minor_t minor);
void devfsadm_errprint(char *message, ...);
void devfsadm_print(char *mid, char *message, ...);
int devfsadm_enumerate_int(char *devfs_path, int index, char **buf,
			    devfsadm_enumerate_t rules[], int nrules);
int devfsadm_enumerate_char(char *devfs_path, int index, char **buf,
			    devfsadm_enumerate_t rules[], int nrules);

#ifdef	__cplusplus
}
#endif

#endif	/* _DEVFSADM_H */
