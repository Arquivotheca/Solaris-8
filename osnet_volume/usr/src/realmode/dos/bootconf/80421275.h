/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _80421275_H
#define	_80421275_H

#pragma ident	"@(#)80421275.h	1.7	99/10/25 SMI"

/*
 * Support for i8042 keyboard/mouse controller.
 */

#ifdef __cplusplus
extern "C" {
#endif

void reset_i8042_1275(void);
void init_i8042_1275(void);
int build_node_i8042_1275(Board *bp);
int parse_bootpath_i8042_1275(char *path, char **rest);
int is_bdp_bootpath_i8042_1275(bef_dev *bdp);
int is_bp_bootpath_i8042_1275(Board *bp, struct bdev_info *dip);
void get_path_from_bdp_i8042_1275(bef_dev *bdp, char *path, int compat);
char *bp_to_desc_i8042_1275(Board *bp, int verbose);

#ifdef __cplusplus
}
#endif

#endif /* _80421275_H */
