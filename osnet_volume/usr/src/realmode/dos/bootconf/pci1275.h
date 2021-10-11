/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * pci1275.h -- public definitions for pci 1275 binding routines
 */

#ifndef	_PCI1275_H
#define	_PCI1275_H

#ident "@(#)pci1275.h   1.19   99/04/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */
void reset_pci1275(void);
void init_pci1275();
int build_node_pci1275(Board *bp);
void bp_to_name_pci1275(Board *bp, char *buf);
char *bp_to_desc_pci1275(Board *bp, int verbose);
int parse_bootpath_pci1275(char *path, char **rest);
int is_bdp_bootpath_pci1275(bef_dev *bdp);
int is_bp_bootpath_pci1275(Board *bp, struct bdev_info *dip);
void get_path_from_bdp_pci1275(bef_dev *bdp, char *path, int compat);
char *get_desc_pci(u_char baseclass, u_char subclass, u_char progclass);

/*
 * Public boot globals
 */
extern struct range **pci_bus_io_avail;
extern struct range **pci_bus_mem_avail;
extern struct range **pci_bus_pmem_avail;

#ifdef	__cplusplus
}
#endif

#endif	/* _PCI1275_H */
