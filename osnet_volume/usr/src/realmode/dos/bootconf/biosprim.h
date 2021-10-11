/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * biosprim.h -- public definitions for bios primary routines
 */

#ifndef	_BIOSPRIM_H
#define	_BIOSPRIM_H

#ident "@(#)biosprim.h   1.19   99/08/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_biosdev();
void output_biosprim();
int check_biosdev(bef_dev *bdp);
extern bef_dev *bios_primaryp;
extern bef_dev *bios_boot_devp;
extern int bios_primary_failure;
int supports_lba(u_char dev);


#define	ATA_PRIMARY_IO		0x1f0

#ifdef	__cplusplus
}
#endif

#endif	/* _BIOSPRIM_H */
