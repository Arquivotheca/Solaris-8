
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_HOTPLUG_HPCSVC_H
#define	_SYS_HOTPLUG_HPCSVC_H

#pragma ident	"@(#)hpcsvc.h	1.2	99/01/27 SMI"

#include <sys/hotplug/hpctrl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
extern int hpc_nexus_register_bus(dev_info_t *dip,
	int (* callback)(dev_info_t *dip, hpc_slot_t handle,
		hpc_slot_info_t *slot_info, int slot_state),
	uint_t flags);
extern int hpc_nexus_unregister_bus(dev_info_t *dip);
extern int hpc_nexus_connect(hpc_slot_t handle, void *data, uint_t flags);
extern int hpc_nexus_disconnect(hpc_slot_t handle, void *data, uint_t flags);
extern int hpc_nexus_insert(hpc_slot_t handle, void *data, uint_t flags);
extern int hpc_nexus_remove(hpc_slot_t handle, void *data, uint_t flags);
extern int hpc_nexus_control(hpc_slot_t handle, int request, caddr_t arg);
extern int hpc_install_event_handler(hpc_slot_t handle, uint_t event_mask,
	int (*event_handler)(caddr_t, uint_t), caddr_t arg);
extern int hpc_remove_event_handler(hpc_slot_t handle);
#else
extern int hpc_nexus_register_bus();
extern int hpc_nexus_unregister_bus();
extern int hpc_nexus_connect();
extern int hpc_nexus_disconnect();
extern int hpc_nexus_insert();
extern int hpc_nexus_remove();
extern int hpc_nexus_control();
extern int hpc_install_event_handler();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HOTPLUG_HPCSVC_H */
