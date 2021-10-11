/*
 * Copyright (c) 1988, 1993, by Sun Microsystems, Inc.
 */

#ifndef	_BSM_DEVICES_H
#define	_BSM_DEVICES_H

#pragma ident	"@(#)devices.h	1.4	93/05/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {		/* see getdmapent(3) */
	char *dmap_devname;
	char *dmap_devtype;
	char *dmap_devlist;
} devmap_t;

devmap_t *getdmapent(), *getdmaptype(), *getdmapnam(), *getdmapdev();
void setdmapent(), enddmapent(), setdmapfile();

typedef struct {		/* see getdaent(3) */
	char *da_devname;
	char *da_devtype;
	char *da_devmin;
	char *da_devmax;
	char *da_devauth;
	char *da_devexec;
} devalloc_t;

devalloc_t *getdaent(), *getdatype(), *getdanam(), *getdadev();
void setdaent(), enddaent(), setdafile();

#ifdef	__cplusplus
}
#endif

#endif	/* _BSM_DEVICES_H */
