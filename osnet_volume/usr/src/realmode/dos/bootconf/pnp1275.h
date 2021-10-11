/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * pnp1275.h -- public definitions for pnp isa 1275 binding routines
 */

#ifndef _PNP1275_H
#define	_PNP1275_H

#ident "@(#)pnp1275.h   1.7   99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
int build_node_pnp1275(Board *bp);
int parse_bootpath_pnp1275(char *path, char **rest);
int is_bdp_bootpath_pnp1275(bef_dev *bdp);
int is_bp_bootpath_pnp1275(Board *bp, struct bdev_info *dip);
void get_path_from_bdp_pnp1275(bef_dev *bdp, char *path, int compat);
char *id_to_str_pnp1275(u_long eisaid);
char *bp_to_desc_pnp1275(Board *bp, int verbose);

/*
 * Public boot globals
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _PNP1275_H */
