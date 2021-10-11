/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Bus operations callout table and error function definitions
 */

#ifndef	_BUS_H
#define	_BUS_H

#ident "@(#)bus.h   1.8   99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct bus_ops_s {
		/* build device tree node from bp */
	int (*build_node)(Board *bp);
		/* parse and save bootpath */
	int (*parse_bootpath)(char *path, char **rest);
		/* check if bdp is bootpath */
	int (*is_bdp_bootpath)(bef_dev *bdp);
		/* check if bp is bootpath */
	int (*is_bp_bootpath)(Board *bp, struct bdev_info *dip);
		/* return path from bdp */
	void (*get_path_from_bdp)(bef_dev *bdp, char *path, int compat);
		/* get a descriptive name from the bp */
	char *(*bp_to_desc)(Board *bp, int verbose);
};

extern struct bus_ops_s bus_ops[];

#ifdef	__cplusplus
}
#endif

#endif	/* _BUS_H */
