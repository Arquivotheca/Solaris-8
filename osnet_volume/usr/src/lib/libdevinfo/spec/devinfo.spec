#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)devinfo.spec	1.2	99/05/05 SMI"
#
# lib/libdevinfo/spec/devinfo.spec

function	di_minor_class
include		<libdevinfo.h>
declaration	unsigned int di_minor_class(di_minor_t minor)
version		SUNW_1.2
end		

function	di_init
include		<libdevinfo.h>
declaration	di_node_t di_init(const char *phys_path, uint_t flag)
version		SUNW_1.1
end		

function	di_fini
include		<libdevinfo.h>
declaration	void di_fini(di_node_t root)
version		SUNW_1.1
end		

function	di_parent_node
include		<libdevinfo.h>
declaration	di_node_t di_parent_node(di_node_t node)
version		SUNW_1.1
end		

function	di_sibling_node
include		<libdevinfo.h>
declaration	di_node_t di_sibling_node(di_node_t node)
version		SUNW_1.1
end		

function	di_child_node
include		<libdevinfo.h>
declaration	di_node_t di_child_node(di_node_t node)
version		SUNW_1.1
end		

function	di_drv_first_node
include		<libdevinfo.h>
declaration	di_node_t di_drv_first_node(const char *drv_name, \
			di_node_t root)
version		SUNW_1.1
end		

function	di_drv_next_node
include		<libdevinfo.h>
declaration	di_node_t di_drv_next_node(di_node_t node)
version		SUNW_1.1
end		

function	di_walk_node
include		<libdevinfo.h>
declaration	int di_walk_node(di_node_t root, uint_t flag, void *arg, int (*node_callback)(di_node_t, void *))
#declaration	int di_walk_node(di_node_t root, uint_t flag, void *arg, void *node_callback)
version		SUNW_1.1
end		

function	di_walk_minor
include		<libdevinfo.h>
declaration	int di_walk_minor(di_node_t root, const char *minor_type, uint_t flag, void *arg, int (*minor_callback)(di_node_t, di_minor_t, void *))
version		SUNW_1.1
end		

function	di_node_name
include		<libdevinfo.h>
declaration	char * di_node_name(di_node_t node)
version		SUNW_1.1
end		

function	di_bus_addr
include		<libdevinfo.h>
declaration	char * di_bus_addr(di_node_t node)
version		SUNW_1.1
end		

function	di_binding_name
include		<libdevinfo.h>
declaration	char * di_binding_name(di_node_t node)
version		SUNW_1.1
end		

function	di_compatible_names
include		<libdevinfo.h>
declaration	int di_compatible_names(di_node_t node, char **names)
version		SUNW_1.1
end		

function	di_instance
include		<libdevinfo.h>
declaration	int di_instance(di_node_t node)
version		SUNW_1.1
end		

function	di_nodeid
include		<libdevinfo.h>
declaration	int di_nodeid(di_node_t node)
version		SUNW_1.1
end		

function	di_state
include		<libdevinfo.h>
declaration	uint_t di_state(di_node_t node)
version		SUNW_1.1
end		

function	di_devid
include		<libdevinfo.h>
declaration	ddi_devid_t di_devid(di_node_t node)
version		SUNW_1.1
end		

function	di_driver_name
include		<libdevinfo.h>
declaration	char * di_driver_name(di_node_t node)
version		SUNW_1.1
end		

function	di_driver_ops
include		<libdevinfo.h>
declaration	uint_t di_driver_ops(di_node_t node)
version		SUNW_1.1
end		

function	di_devfs_path
include		<libdevinfo.h>
declaration	char * di_devfs_path(di_node_t node)
version		SUNW_1.1
end		

function	di_devfs_path_free
include		<libdevinfo.h>
declaration	void di_devfs_path_free(char *buf)
version		SUNW_1.1
end		

function	di_minor_next
include		<libdevinfo.h>
declaration	di_minor_t di_minor_next(di_node_t node, di_minor_t minor)
version		SUNW_1.1
end		

function	di_minor_type
include		<libdevinfo.h>
declaration	ddi_minor_type di_minor_type(di_minor_t minor)
version		SUNW_1.1
end		

function	di_minor_name
include		<libdevinfo.h>
declaration	char * di_minor_name(di_minor_t minor)
version		SUNW_1.1
end		

function	di_minor_devt
include		<libdevinfo.h>
declaration	dev_t di_minor_devt(di_minor_t minor)
version		SUNW_1.1
end		

function	di_minor_spectype
include		<libdevinfo.h>
declaration	int di_minor_spectype(di_minor_t minor)
version		SUNW_1.1
end		

function	di_minor_nodetype
include		<libdevinfo.h>
declaration	char * di_minor_nodetype(di_minor_t minor)
version		SUNW_1.1
end		

function	di_prop_next
include		<libdevinfo.h>
declaration	di_prop_t di_prop_next(di_node_t node, di_prop_t prop)
version		SUNW_1.1
end		

function	di_prop_devt
include		<libdevinfo.h>
declaration	dev_t di_prop_devt(di_prop_t prop)
version		SUNW_1.1
end		

function	di_prop_name
include		<libdevinfo.h>
declaration	char * di_prop_name(di_prop_t prop)
version		SUNW_1.1
end		

function	di_prop_type
include		<libdevinfo.h>
declaration	int di_prop_type(di_prop_t prop)
version		SUNW_1.1
end		

function	di_prop_ints
include		<libdevinfo.h>
declaration	int di_prop_ints(di_prop_t prop, int **prop_data)
version		SUNW_1.1
end		

function	di_prop_strings
include		<libdevinfo.h>
declaration	int di_prop_strings(di_prop_t prop, char **prop_data)
version		SUNW_1.1
end		

function	di_prop_bytes
include		<libdevinfo.h>
declaration	int di_prop_bytes(di_prop_t prop, uchar_t **prop_data)
version		SUNW_1.1
end		

function	di_prop_lookup_ints
include		<libdevinfo.h>
declaration	int di_prop_lookup_ints(dev_t dev, di_node_t node, \
			const char *prop_name, int **prop_data)
version		SUNW_1.1
end		

function	di_prop_lookup_strings
include		<libdevinfo.h>
declaration	int di_prop_lookup_strings(dev_t dev, di_node_t node, \
			const char *prop_name, char **prop_data)
version		SUNW_1.1
end		

function	di_prop_lookup_bytes
include		<libdevinfo.h>
declaration	int di_prop_lookup_bytes(dev_t dev, di_node_t node, \
			const char *prop_name, uchar_t **prop_data)
version		SUNW_1.1
end		

function	di_prom_init
include		<libdevinfo.h>
declaration	di_prom_handle_t di_prom_init(void)
version		SUNW_1.1
end		

function	di_prom_fini
include		<libdevinfo.h>
declaration	void di_prom_fini(di_prom_handle_t ph)
version		SUNW_1.1
end		

function	di_prom_prop_next
include		<libdevinfo.h>
declaration	di_prom_prop_t di_prom_prop_next(di_prom_handle_t ph, \
			di_node_t node, di_prom_prop_t prom_prop)
version		SUNW_1.1
end		

function	di_prom_prop_name
include		<libdevinfo.h>
declaration	char * di_prom_prop_name(di_prom_prop_t prom_prop)
version		SUNW_1.1
end		

function	di_prom_prop_data
include		<libdevinfo.h>
declaration	int di_prom_prop_data(di_prom_prop_t prom_prop, \
			uchar_t **prom_prop_data)
version		SUNW_1.1
end		

function	di_prom_prop_lookup_ints
include		<libdevinfo.h>
declaration	int di_prom_prop_lookup_ints(di_prom_handle_t ph, \
			di_node_t node, const char *prom_prop_name, \
			int **prom_prop_data)
version		SUNW_1.1
end		

function	di_prom_prop_lookup_strings
include		<libdevinfo.h>
declaration	int di_prom_prop_lookup_strings(di_prom_handle_t ph, \
			di_node_t node, const char *prom_prop_name, \
			char **prom_prop_data)
version		SUNW_1.1
end		

function	di_prom_prop_lookup_bytes
include		<libdevinfo.h>
declaration	int di_prom_prop_lookup_bytes(di_prom_handle_t ph, \
			di_node_t node, const char *prom_prop_name, \
			uchar_t **prom_prop_data)
version		SUNW_1.1
end		

function	devfs_path_to_drv
include		<libdevinfo.h>, <device_info.h>
declaration	int devfs_path_to_drv(char *devfs_path, char *drv_buf)
version		SUNWprivate_1.1
end		

function	devfs_dev_to_prom_name
include		<libdevinfo.h>
declaration	int devfs_dev_to_prom_name(char *dev_path, char *prom_path)
version		SUNWprivate_1.1
end		

function	devfs_resolve_aliases
include		<libdevinfo.h>
declaration	char * devfs_resolve_aliases(char *drv)
version		SUNWprivate_1.1
end		

function	devfs_bootdev_set_list
include		<libdevinfo.h>
declaration	int devfs_bootdev_set_list(const char *dev_name, \
			const u_int options)
version		SUNWprivate_1.1
end		

function	devfs_bootdev_modifiable
include		<libdevinfo.h>
declaration	int devfs_bootdev_modifiable(void)
version		SUNWprivate_1.1
end		

function	devfs_bootdev_get_list
include		<libdevinfo.h>
declaration	int devfs_bootdev_get_list(const char *default_root, \
			struct boot_dev ***bootdev_list)
version		SUNWprivate_1.1
end		

function	devfs_bootdev_free_list
include		<libdevinfo.h>
declaration	void devfs_bootdev_free_list(struct boot_dev **array)
version		SUNWprivate_1.1
end		

function	devfs_get_prom_names
include		<libdevinfo.h>
declaration	int devfs_get_prom_names(const char *dev_name, \
			u_int options, char ***prom_list)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_init_impl
include		<libdevinfo.h>
declaration	di_node_t di_init_impl(const char *phys_path, uint_t flag, \
			struct di_priv_data *priv)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_init_driver
include		<libdevinfo.h>
declaration	di_node_t di_init_driver(const char *drv_name, uint_t flag)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_prop_drv_next
include		<libdevinfo.h>
declaration	di_prop_t di_prop_drv_next(di_node_t node, di_prop_t prop)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_prop_sys_next
include		<libdevinfo.h>
declaration	di_prop_t di_prop_sys_next(di_node_t node, di_prop_t prop)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_prop_global_next
include		<libdevinfo.h>
declaration	di_prop_t di_prop_global_next(di_node_t node, di_prop_t prop)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_prop_hw_next
include		<libdevinfo.h>
declaration	di_prop_t di_prop_hw_next(di_node_t node, di_prop_t prop)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_prop_rawdata
include		<libdevinfo.h>
declaration	int di_prop_rawdata(di_prop_t prop, uchar_t **prop_data)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_parent_private_data
include		<libdevinfo.h>
declaration	void * di_parent_private_data(di_node_t node)
version		SUNWprivate_1.1
end		

#
# Consolidation private PSARC 1997/127
#
function	di_driver_private_data
include		<libdevinfo.h>
declaration	void * di_driver_private_data(di_node_t node)
version		SUNWprivate_1.1
end		

