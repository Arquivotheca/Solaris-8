/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DR_INFO_H
#define	_DR_INFO_H

#pragma ident	"@(#)dr_info.h	1.15	99/11/11 SMI"

/*
 * This file defines headers and routines shared between the dr_info_*.c
 * files.  These files determine devices on a system board and their
 * current usage by the system.
 */

#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <libdevinfo.h>

#include <dr_subr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This define and variable track whether we need to restart system
 * daemons after a detach operation is completed or aborted.
 */
typedef enum {
	DAEMON_UNKNOWN = 0,	/* initial state */
	DAEMON_KILLED,
	DAEMON_NOT_PRESENT
} daemon_type_t;

/*
 * For leaf devices, indicates whether the node is a
 * network node or not.
 *
 * All dr_io_t nodes are by default given the type of
 * DEVICE_NOT_DEVI_LEAF.  When we create the node
 * for a devi leaf node, we set the type to DEVICE_DEVI_LEAF.
 * In all cases we've seen so far, this type is changed DEVICE_NET or
 * DEVICE_NOTNET, but we want to make sure we can tell which nodes
 * are derived from devi leaf nodes.
 */
typedef enum {
	DEVICE_NOT_DEVI_LEAF = 0,	/* initial state */
	DEVICE_DEVI_LEAF,
	DEVICE_NET,
	DEVICE_NOTNET
} device_type_t;

#define	MAXFILNAM	1024

/*
 * Dev-info leaf nodes represent minor devices.  Currently we recognize
 * network and non-network types.
 *
 * For network devices, information consists of the name it is configured
 * under and the status of the interface.
 *
 * For non-network devices, we form the /devices pathnames
 * from the minor data of the leaf node.  The /dev names are found by
 * hunting through the /dev entries for symbolic links to the /devices
 * names.  Usage of the device consists of (mount points, configured
 * as swap, number of opens, component for Sun Online DiskSuite, etc).
 */
typedef struct dr_leaf_t *dr_leafp_t;
typedef struct dr_io_t *dr_iop_t;

typedef struct dr_leaf_t {
	dr_leafp_t	next;
	dr_iop_t	major_dev;	/* major device (eg, sd0) */
	union {
		struct {
			char		_ifname[IFNAMSIZ];
			int		_ifinstance;
			int		_netflags;
			char		_canonical_name[IFNAMSIZ];
			struct sockaddr_in	_netaddr;
			struct sockaddr_in6	_netaddr6;
			dr_leafp_t	_ap_netname;
		} net;
		struct {
			char		*_devices_name;
			char		*_dev_name;
			char		*_mount_point;
			char		*_ap_metaname;
			dr_iop_t	_ds_dev;	/* DiskSuite device */
			int		_notnetflags;
			int		_open_count;
			u_long		_device_id;	/* used in AP matchup */
		} notnet;
	} dv_union;
} dr_leaf_t;

#define	ifname		dv_union.net._ifname
#define	ifinstance	dv_union.net._ifinstance
#define	canonical_name	dv_union.net._canonical_name
#define	netaddr		dv_union.net._netaddr
#define	netaddr6	dv_union.net._netaddr6
#define	netflags	dv_union.net._netflags
#define	ap_netname	dv_union.net._ap_netname

#define	devices_name	dv_union.notnet._devices_name
#define	dev_name	dv_union.notnet._dev_name
#define	mount_point	dv_union.notnet._mount_point
#define	ap_metaname	dv_union.notnet._ap_metaname
#define	ds_dev		dv_union.notnet._ds_dev
#define	notnetflags	dv_union.notnet._notnetflags
#define	open_count	dv_union.notnet._open_count
#define	device_id	dv_union.notnet._device_id

/*
 * Defines for the notnetflags field.
 */
#define	NOTNET_SWAP		0x01
#define	NOTNET_AP_DB		0x02
#define	NOTNET_OLDS_DB		0x04

/*
 * during reading of the devinfo tree, a similar tree structure
 * is created for the IO devices on the board.  This is
 * a temporary work structure which is used to fill in the
 * information requested about devices on the board.
 */
typedef struct dr_io_t {
	di_node_t	di_node;	/* Libdevinfo node for this device */
					/* This reference is invalid after */
					/* build_device_tree() exits */
	dr_iop_t	dv_parent;
	dr_iop_t	dv_sibling;
	dr_iop_t	dv_child;	/* 0->node represents a device */
	char 		*dv_name;	/* devi name */
	char		*dv_addr;	/* devi addr */
	int		dv_instance;	/* devi instance */
	ap_info_t	dv_ap_info;
	device_type_t	dv_node_type;	/* for leaf nodes, type derived from */
					/* devi minor data node_type */
	dr_leafp_t	dv_leaf;	/* for leaf nodes, per dev info */
					/* for each minor device. */
} dr_io_t;

/*
 * Driver name/alias mappings
 */
struct devnm {
	struct devnm *next;
	char *name;
	char *alias;
};

#define	DAFILE		"/etc/driver_aliases"
#define	HASHTABSIZE	64
#define	HASHMASK	(HASHTABSIZE-1)

extern int num_leaves;			/* number of leaf devices found */
extern char devices_path[MAXFILNAM];	/* used to build /devices name */

	/* dr_info_devt.c */
extern dr_iop_t build_device_tree(int board);
extern dr_iop_t dr_dev_malloc(void);
extern dr_leafp_t dr_leaf_malloc(void);
extern void free_dr_device_tree(dr_iop_t dp);

	/* dr_info_olds.c */
extern void add_disksuite_to_device_tree(dr_leafp_t **leaf_array);

	/* dr_info_io.c */
extern int compare_devname(const void *a, const void *b);

	/* dr_info_ap.c */
#ifdef AP
extern void add_ap_meta_devices(dr_iop_t rootp, int netonly);
#endif AP

	/* dr_info_net.c */
extern void find_net_entries(dr_iop_t dp, void (*func)(dr_leafp_t dp));
extern int build_net_leaf(dr_leafp_t dmpt, char *name);
extern void add_net_usage_record(sbus_devicep_t dp, dr_iop_t dip);
#ifdef AP
extern void create_ap_net_leaf(dr_leafp_t dlp, int is_active, char *alias);
#endif AP

#ifdef	__cplusplus
}
#endif

#endif	/* _DR_INFO_H */
