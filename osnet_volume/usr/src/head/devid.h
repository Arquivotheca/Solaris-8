/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DEVID_H
#define	_DEVID_H

#pragma ident	"@(#)devid.h	1.6	96/05/06 SMI"

#include <sys/sunddi.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct devid_nmlist {
	char 	*devname;
	dev_t	dev;
} devid_nmlist_t;

extern int	devid_get(int fd, ddi_devid_t *devid);
extern int	devid_get_minor_name(int fd, char **minor_name);
extern size_t	devid_sizeof(ddi_devid_t devid);
extern int	devid_compare(ddi_devid_t devid1, ddi_devid_t devid2);
extern int	devid_deviceid_to_nmlist(char *search_path, ddi_devid_t devid,
		    char *minor_name, devid_nmlist_t **retlist);
extern void	devid_free_nmlist(devid_nmlist_t *list);

#ifdef	__cplusplus
}
#endif

#endif	/* _DEVID_H */
