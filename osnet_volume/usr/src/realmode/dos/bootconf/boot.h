/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * boot.h -- public definitions for boot routines
 */

#ifndef	_BOOT_H
#define	_BOOT_H

#ident "@(#)boot.h   1.40   99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <dostypes.h>

#include "types.h"
#include "dev_info.h"
#include "devdb.h"

/*
 * Public boot globals
 */

/*
 * The bef_dev list contains a list of bootable devices found
 * from loading the befs
 */
typedef struct bef_dev_s {
	char *name;
	char *dev_type;
	char *desc;
	u_char installed;
	char slice;
	Board *bp;
	struct bdev_info *info; /* ptr to copy of buffer when bef deinstalled */
	struct bdev_info *info_orig; /* original pointer into the bef */
} bef_dev;

#define	N_BEF_DEVS	144
#define	STDIN_PROPLEN	80
#define	MAXLINE		120		/* maximum line */
#ifndef PATH_MAX
#define	PATH_MAX _MAX_PATH	/* DOS's maximum path length */
#endif /* PATH_MAX */

extern bef_dev *bef_devs;
extern char *busen[];
extern int nbusen;
extern int n_boot_dev;
extern int dos_emul_boot;
extern int Autoboot;
extern char bootpath_in[];
extern struct bdev_info dev80;
extern Board *Same_bef_chain;
extern u_char booted_from_eltorito_cdrom;
extern u_char bootpath_set;

#define	RTYPE(rp) ((rp)->flags & (RESF_TYPE+RESF_DISABL+RESF_ALT))

/*
 * Public boot function prototypes
 */
void init_boot();
void fini_boot(void *arg, int exitcode);
void menu_boot(void);
void auto_boot(void);
void get_path_from_bdp(bef_dev *bdp, char *path, int compat);
void get_path(Board *bp, char *path);
void auto_boot_timeout(void);
void free_boot_list(struct menu_list *boot_list, int nboot_list);
int make_boot_desc(bef_dev *bdp, char *s, int def_select);
void make_boot_list(struct menu_list **boot_listp, int *nboot_listp);
int get_bootpath();
int parse_bootpath();
int parse_target(char *s, u_char *lunp, u_char *targetp, char *slicep);
int parse_slice(char *s, char *slicep);
char *determine_scsi_target_driver(bef_dev *bdp);
char *strdup(const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _BOOT_H */
