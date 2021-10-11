#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)device.spec	1.3	99/05/14 SMI"
#
# lib/libdevice/spec/device.spec

function	devctl_release
include		<sys/types.h>, <libdevice.h>
declaration	void devctl_release(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_device_online
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_device_online(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_device_offline
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_device_offline(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_device_getstate
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_device_getstate(devctl_hdl_t hdl, uint_t *statep)
version		SUNWprivate_1.1
end		

function	devctl_device_reset
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_device_reset(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_bus_quiesce
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_quiesce(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_bus_unquiesce
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_unquiesce(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_bus_getstate
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_getstate(devctl_hdl_t hdl, uint_t *statep)
version		SUNWprivate_1.1
end		

function	devctl_bus_reset
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_reset(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_bus_resetall
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_resetall(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_bus_acquire
include		<sys/types.h>, <libdevice.h>
declaration	devctl_hdl_t devctl_bus_acquire(char *devfs_path, uint_t flags)
version		SUNWprivate_1.1
end		

function	devctl_bus_configure
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_configure(devctl_hdl_t dcp)
version		SUNWprivate_1.1
end		

function	devctl_bus_unconfigure
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_bus_unconfigure(devctl_hdl_t dcp)
version		SUNWprivate_1.1
end		

function	devctl_device_acquire
include		<sys/types.h>, <libdevice.h>
declaration	devctl_hdl_t devctl_device_acquire(char *devfs_path, \
			uint_t flags)
version		SUNWprivate_1.1
end		

function	devctl_device_remove
include		<sys/types.h>, <libdevice.h>
declaration	int devctl_device_remove(devctl_hdl_t dcp)
version		SUNWprivate_1.1
end		

function	devctl_ap_acquire
include		<libdevice.h>
declaration	devctl_hdl_t devctl_ap_acquire(char *devfs_path, uint_t flags)
version		SUNWprivate_1.1
end		

function	devctl_ap_insert
include		<libdevice.h>
declaration	int devctl_ap_insert(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_remove
include		<libdevice.h>
declaration	int devctl_ap_remove(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_connect
include		<libdevice.h>
declaration	int devctl_ap_connect(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_disconnect
include		<libdevice.h>
declaration	int devctl_ap_disconnect(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_configure
include		<libdevice.h>
declaration	int devctl_ap_configure(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_unconfigure
include		<libdevice.h>
declaration	int devctl_ap_unconfigure(devctl_hdl_t hdl)
version		SUNWprivate_1.1
end		

function	devctl_ap_getstate
include		<libdevice.h>
declaration	int devctl_ap_getstate(devctl_hdl_t hdl,	\
			devctl_ap_state_t *statep)
version		SUNWprivate_1.1
end		

