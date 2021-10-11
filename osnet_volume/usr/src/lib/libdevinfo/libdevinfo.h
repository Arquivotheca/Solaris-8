/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBDEVINFO_H
#define	_LIBDEVINFO_H

#pragma ident	"@(#)libdevinfo.h	1.7	99/03/31 SMI"

#include <errno.h>
#include <sys/param.h>
#include <sys/sunddi.h>
#include <sys/openpromio.h>
#include <sys/ddi_impldefs.h>
#include <sys/devinfo_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * flags for di_walk_node
 */
#define	DI_WALK_CLDFIRST	0
#define	DI_WALK_SIBFIRST	1
#define	DI_WALK_LINKGEN		2

#define	DI_WALK_MASK		0xf

/*
 * return code for node_callback
 */
#define	DI_WALK_CONTINUE	0
#define	DI_WALK_PRUNESIB	-1
#define	DI_WALK_PRUNECHILD	-2
#define	DI_WALK_TERMINATE	-3

/*
 * flags for di_walk_minor
 */
#define	DI_CHECK_ALIAS		0x10
#define	DI_CHECK_INTERNAL_PATH	0x20

#define	DI_CHECK_MASK		0xf0

/* nodeid types */
#define	DI_PSEUDO_NODEID	-1
#define	DI_SID_NODEID		-2
#define	DI_PROM_NODEID		-3

/* node & device states */
#define	DI_DRIVER_DETACHED	0x8000
#define	DI_DEVICE_OFFLINE	0x1
#define	DI_DEVICE_DOWN		0x2
#define	DI_BUS_QUIESCED		0x100
#define	DI_BUS_DOWN		0x200

/* property types */
#define	DI_PROP_TYPE_BOOLEAN	0
#define	DI_PROP_TYPE_INT	1
#define	DI_PROP_TYPE_STRING	2
#define	DI_PROP_TYPE_BYTE	3
#define	DI_PROP_TYPE_UNKNOWN	4
#define	DI_PROP_TYPE_UNDEF_IT	5

/* opaque handles */

typedef struct di_node *di_node_t;	/* opaque handle to node */
typedef struct di_minor *di_minor_t;	/* opaque handle to minor node */
typedef struct di_prop *di_prop_t;	/* opaque handle to property */
typedef struct di_prom_prop *di_prom_prop_t;	/* opaque handle to prom prop */
typedef struct di_prom_handle *di_prom_handle_t;	/* opaque handle */

/*
 * Null handles to make handles really opaque
 */
#define	DI_NODE_NIL	NULL
#define	DI_MINOR_NIL	NULL
#define	DI_PROP_NIL	NULL
#define	DI_PROM_PROP_NIL	NULL
#define	DI_PROM_HANDLE_NIL	NULL

/* Interface Prototypes */

/*
 * Snapshot initialization and cleanup
 */
extern di_node_t di_init(const char *phys_path, uint_t flag);
extern void di_fini(di_node_t root);

/*
 * tree traversal
 */
extern di_node_t di_parent_node(di_node_t node);
extern di_node_t di_sibling_node(di_node_t node);
extern di_node_t di_child_node(di_node_t node);
extern di_node_t di_drv_first_node(const char *drv_name, di_node_t root);
extern di_node_t di_drv_next_node(di_node_t node);

/*
 * tree walking assistants
 */
extern int di_walk_node(di_node_t root, uint_t flag, void *arg,
    int (*node_callback)(di_node_t node, void *arg));
extern int di_walk_minor(di_node_t root, const char *minortype, uint_t flag,
    void *arg, int (*minor_callback)(di_node_t node, di_minor_t minor,
    void *arg));

/*
 * generic node parameters
 */
extern char *di_node_name(di_node_t node);
extern char *di_bus_addr(di_node_t node);
extern char *di_binding_name(di_node_t node);
extern int di_compatible_names(di_node_t, char **names);
extern int di_instance(di_node_t node);
extern int di_nodeid(di_node_t node);
extern uint_t di_state(di_node_t node);
extern ddi_devid_t di_devid(di_node_t node);

extern char *di_driver_name(di_node_t node);
extern uint_t di_driver_ops(di_node_t node);

extern char *di_devfs_path(di_node_t node);
extern void di_devfs_path_free(char *path_buf);

/*
 * minor data access
 */
extern di_minor_t di_minor_next(di_node_t node, di_minor_t minor);
extern ddi_minor_type di_minor_type(di_minor_t minor);
extern char *di_minor_name(di_minor_t minor);
extern dev_t di_minor_devt(di_minor_t minor);
extern int di_minor_spectype(di_minor_t minor);
extern char *di_minor_nodetype(di_minor_t node);
extern unsigned int di_minor_class(di_minor_t minor);

/*
 * Software property access
 */
extern di_prop_t di_prop_next(di_node_t node, di_prop_t prop);
extern dev_t di_prop_devt(di_prop_t prop);
extern char *di_prop_name(di_prop_t prop);
extern int di_prop_type(di_prop_t prop);

extern int di_prop_ints(di_prop_t prop, int **prop_data);
extern int di_prop_strings(di_prop_t prop, char **prop_data);
extern int di_prop_bytes(di_prop_t prop, uchar_t **prop_data);
extern int di_prop_lookup_ints(dev_t dev, di_node_t node,
    const char *prop_name, int **prop_data);
extern int di_prop_lookup_strings(dev_t dev, di_node_t node,
    const char *prop_name, char **prop_data);
extern int di_prop_lookup_bytes(dev_t dev, di_node_t node,
    const char *prop_name, uchar_t **prop_data);

/*
 * PROM property access
 */
extern di_prom_handle_t di_prom_init();
extern void di_prom_fini(di_prom_handle_t ph);

extern di_prom_prop_t di_prom_prop_next(di_prom_handle_t ph, di_node_t node,
    di_prom_prop_t prom_prop);

extern char *di_prom_prop_name(di_prom_prop_t prom_prop);
extern int di_prom_prop_data(di_prom_prop_t prop, uchar_t **prom_prop_data);

extern int di_prom_prop_lookup_ints(di_prom_handle_t prom, di_node_t node,
    const char *prom_prop_name, int **prom_prop_data);
extern int di_prom_prop_lookup_strings(di_prom_handle_t prom, di_node_t node,
    const char *prom_prop_name, char **prom_prop_data);
extern int di_prom_prop_lookup_bytes(di_prom_handle_t prom, di_node_t node,
    const char *prom_prop_name, uchar_t **prom_prop_data);

/*
 * Private interfaces--may break from release to release
 */

/*
 * Interfaces for private data
 */
extern di_node_t di_init_driver(const char *drv_name, uint_t flag);
extern di_node_t di_init_impl(const char *phys_path, uint_t flag,
    struct di_priv_data *priv_data);

/*
 * Prtconf needs to know property lists, raw prop_data, and private data
 */
extern di_prop_t di_prop_drv_next(di_node_t node, di_prop_t prop);
extern di_prop_t di_prop_sys_next(di_node_t node, di_prop_t prop);
extern di_prop_t di_prop_global_next(di_node_t node, di_prop_t prop);
extern di_prop_t di_prop_hw_next(di_node_t node, di_prop_t prop);

extern int di_prop_rawdata(di_prop_t prop, uchar_t **prop_data);
extern void *di_parent_private_data(di_node_t node);
extern void *di_driver_private_data(di_node_t node);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBDEVINFO_H */
