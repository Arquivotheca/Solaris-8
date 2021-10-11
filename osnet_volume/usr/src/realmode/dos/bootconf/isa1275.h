/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * isa1275.h -- public definitions for isa, eisa & mc 1275 binding routines
 */

#ifndef	_ISA1275_H
#define	_ISA1275_H

#ident "@(#)isa1275.h   1.11   99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
void reset_isa1275(void);
void init_isa1275();
void build_bus_node_isa1275(int bust);
int build_node_isa1275(Board *bp);
int parse_bootpath_isa1275(char *path, char **rest);
int is_bdp_bootpath_isa1275(bef_dev *bdp);
int is_bp_bootpath_isa1275(Board *bp, struct bdev_info *dip);
void get_path_from_bdp_isa1275(bef_dev *bdp, char *path, int compat);
char *bp_to_desc_isa1275(Board *bp, int verbose);

/*
 * Public boot globals
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISA1275_H */
