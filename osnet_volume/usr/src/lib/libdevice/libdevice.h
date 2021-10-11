/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ifndef	_LIBDEVICE_H
#define	_LIBDEVICE_H

#pragma ident	"@(#)libdevice.h	1.7	99/01/15 SMI"

#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/devctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DC_EXCL	1

typedef struct devctl_dummy_struct *devctl_hdl_t;

devctl_hdl_t
devctl_device_acquire(char *devfs_path, uint_t flags);

devctl_hdl_t
devctl_bus_acquire(char *devfs_path, uint_t flags);

devctl_hdl_t
devctl_ap_acquire(char *devfs_path, uint_t flags);

void
devctl_release(devctl_hdl_t hdl);

int
devctl_device_offline(devctl_hdl_t hdl);

int
devctl_device_remove(devctl_hdl_t hdl);

int
devctl_device_online(devctl_hdl_t hdl);

int
devctl_device_reset(devctl_hdl_t hdl);

int
devctl_device_getstate(devctl_hdl_t hdl, uint_t *statep);

int
devctl_bus_quiesce(devctl_hdl_t hdl);

int
devctl_bus_unquiesce(devctl_hdl_t hdl);

int
devctl_bus_reset(devctl_hdl_t hdl);

int
devctl_bus_resetall(devctl_hdl_t hdl);

int
devctl_bus_getstate(devctl_hdl_t hdl, uint_t *statep);

int
devctl_bus_configure(devctl_hdl_t hdl);

int
devctl_bus_unconfigure(devctl_hdl_t hdl);

int
devctl_ap_insert(devctl_hdl_t hdl);

int
devctl_ap_remove(devctl_hdl_t hdl);

int
devctl_ap_connect(devctl_hdl_t hdl);

int
devctl_ap_disconnect(devctl_hdl_t hdl);

int
devctl_ap_configure(devctl_hdl_t hdl);

int
devctl_ap_unconfigure(devctl_hdl_t hdl);

int
devctl_ap_getstate(devctl_hdl_t hdl, devctl_ap_state_t *statep);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBDEVICE_H */
