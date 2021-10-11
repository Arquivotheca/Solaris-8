/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DC_KI_H
#define	_DC_KI_H

#pragma ident	"@(#)dc_ki.h	1.1	98/07/17 SMI"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Device types. For description of types see sunddi.h
 * CLONE_DEV is already an advertised flag through DDI.
 * We don't define the class types in this file due to constraints
 * with DDI header files compliance.
 */

typedef enum dev_type {
	DEV_INVALID = 0,		/* dev has the default behaviour ? */
	DEV_GLOBAL = GLOBAL_DEV,	/* global device (le ip) */
	DEV_NODEBOUND = NODEBOUND_DEV,	/* bound to a node  (tiocts) */
	DEV_NODESPECIFIC = NODESPECIFIC_DEV, /* specific to the node (kmem) */
	DEV_ENUMERATED = ENUMERATED_DEV, /* device is distinguished(c0t0d0s0) */
	DEV_CLONE = CLONE_DEV		/* each device is a cloned device */
} dev_type_t;

/*
 * A constant that defines an undefined external or global minor no.
 */

#define	EMINOR_UNKNOWN	((minor_t)(-1))

/*
 * Prefix for the instance mapping file to denote a machine that is
 * configured with global devices.
 */
#define	CLUSTER_DEVICE_BASE	"/node@"
#define	CLUSTER_DEVICE_BASE_LEN (sizeof (CLUSTER_DEVICE_BASE) - 1)

/*
 * Defines for the sleep argument of dc_devconfig_lock function.
 */
#define	DC_CLSLEEP	0
#define	DC_CLNOSLEEP	1

#define	DCOPS_VERSION	1

/*
 * Operations vector between kernel and DCS. The stubs vector
 * that doesn't do any thing is defined in sunddi.c. The DCS ops
 * vector is enabled at the time DCS module is getting loaded.
 */

struct dc_ops {
	int version;
	int (*dc_get_major)(major_t *, char *);
	int (*dc_free_major)(major_t, char *);
	int (*dc_major_name)(major_t, char *);
	int (*dc_sync_instances)();
	int (*dc_instance_path)(major_t, char *, uint_t);
	int (*dc_get_instance)(major_t, const char *, uint_t *);
	int (*dc_free_instance)(major_t, const char *, uint_t);
	int (*dc_map_minor)(major_t, minor_t, minor_t *, dev_type_t);
	int (*dc_unmap_minor)(major_t, minor_t, minor_t, dev_type_t);
	int (*dc_resolve_minor)(major_t, minor_t, minor_t *, dev_type_t *);
	int (*dc_devconfig_lock)(int);
	int (*dc_devconfig_unlock)();
	int (*dc_service_config)(int, void *, int);
};

#define	DC_GET_MAJOR(mopsp, major, driver_name)	\
		(((mopsp)->dc_get_major)(major, driver_name))
#define	DC_FREE_MAJOR(mopsp, major, driver)	\
		(((mopsp)->dc_free_major)(major, driver))
#define	DC_MAJOR_NAME(mopsp, major, driver_name)	\
		(((mopsp)->dc_major_name)(major, driver_name))
#define	DC_SYNC_INSTANCES(mopsp)	\
		(((mopsp)->dc_sync_instances)())
#define	DC_INSTANCE_PATH(mopsp, major, path, inst_number)	\
		(((mopsp)->dc_instance_path)(major, path, inst_number))
#define	DC_GET_INSTANCE(mopsp, maj, path, inst_number)	\
		(((mopsp)->dc_get_instance)(maj, path, inst_number))
#define	DC_FREE_INSTANCE(mopsp, major, path, inst)	\
		(((mopsp)->dc_free_instance)(major, path, inst))
#define	DC_MAP_MINOR(mopsp, major, lminor, gminor, dev_type)	\
		(((mopsp)->dc_map_minor)(major, lminor, gminor, dev_type))
#define	DC_UNMAP_MINOR(mopsp, major, lminor, gminor, dev_type)	\
		(((mopsp)->dc_unmap_minor)(major, lminor, gminor, dev_type))
#define	DC_RESOLVE_MINOR(mopsp, major, gminor, lminor, dev_type) \
		(((mopsp)->dc_resolve_minor)(major, gminor, lminor, dev_type))
#define	DC_DEVCONFIG_LOCK(mopsp, sleep) \
		(((mopsp)->dc_devconfig_lock)(sleep))
#define	DC_DEVCONFIG_UNLOCK(mopsp) \
		(((mopsp)->dc_devconfig_unlock)())
#define	DC_SERVICE_CONFIG(mopsp, cmd, service_args_ptr, mode) \
		(((mopsp)->dc_service_config)(cmd, service_args_ptr, mode))

/*
 * The global ops vector. This is actually defined in base Solaris
 * initialised to stubs operation vector in sunddi.c. When the system
 * is booted as a cluster the DCS client side module will change
 * this ops vector to export its operations.
 */

extern struct dc_ops dcops;

/*
 * Stubs operation vector.
 */

extern int dcstub_get_major(major_t *, char *);
extern int dcstub_free_major(major_t, char *);
extern int dcstub_major_name(major_t, char *);
extern int dcstub_sync_instances();
extern int dcstub_instance_path(major_t, char *, uint_t);
extern int dcstub_get_instance(major_t, const char *, uint_t *);
extern int dcstub_free_instance(major_t, const char *, uint_t);
extern int dcstub_map_minor(major_t, minor_t, minor_t *, dev_type_t);
extern int dcstub_unmap_minor(major_t, minor_t, minor_t, dev_type_t);
extern int dcstub_resolve_minor(major_t, minor_t, minor_t *, dev_type_t *);
extern int dcstub_devconfig_lock(int);
extern int dcstub_devconfig_unlock();
extern int dcstub_service_config(int, void *, int);

/*
 * Bootstrap routines used to load mappings of global dev values to
 * local dev values and class types, from persistent storage into memory
 * using BOP_XXX() routines. The routine is used for all drivers that
 * are loaded into memory before root is mounted.
 */

extern int clboot_modload(struct modctl *mp);
extern int clboot_loadrootmodules();
extern int clboot_rootconf();
extern void clboot_mountroot();

enum dcop_state {
	DC_READONLY,		/* Bootstrap the dcops vector */
	DC_INSTALLING,		/* Installing global devices */
	DC_READY		/* Completely clustered */
};

/*
 * Routine that provides the kernel interface for manipulating the dcops
 * vector.
 */
extern int dcops_install(struct dc_ops *dcops, enum dcop_state state);

#ifdef __cplusplus
}
#endif

#endif /* _DC_KI_H */
